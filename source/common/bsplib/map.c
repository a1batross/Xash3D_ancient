// map.c

#include "bsplib.h"

extern bool onlyents;

#define ScaleCorrection	(1.0/128.0)
#define VALVE_FORMAT	220
#define PLANE_HASHES	1024

// brushes are parsed into a temporary array of sides,
// which will have the bevels added and duplicates
// removed before the final brush is allocated
bspbrush_t	*buildBrush;
int		g_brushtype;
int		entity_numbrushes;		// to track editor brush numbers

int		nummapplanes;
plane_t		mapplanes[MAX_MAP_PLANES];
plane_t		*planehash[PLANE_HASHES];
vec3_t		map_mins, map_maxs, map_size;
bsp_entity_t	*mapent;

int		c_boxbevels;
int		c_edgebevels;
int		c_areaportals;
int		c_clipbrushes;

/*
=============================================================================

PLANE FINDING

=============================================================================
*/
#define USE_HASHING		// comment this to make plane finding use linear sort

/*
=================
PlaneTypeForNormal
=================
*/
int PlaneTypeForNormal (vec3_t normal)
{
	// NOTE: should these have an epsilon around 1.0?		
	if(fabs(normal[0]) == 1.0) return PLANE_X;
	if(fabs(normal[1]) == 1.0) return PLANE_Y;
	if(fabs(normal[2]) == 1.0) return PLANE_Z;
	
	return 3;
}

/*
================
PlaneEqual
================
*/
#define	NORMAL_EPSILON	0.00001
#define	DIST_EPSILON	0.01

bool PlaneEqual( plane_t *p, vec3_t normal, vec_t dist )
{
	if( fabs(p->normal[0] - normal[0]) < NORMAL_EPSILON && fabs(p->normal[1] - normal[1]) < NORMAL_EPSILON
	&& fabs(p->normal[2] - normal[2]) < NORMAL_EPSILON && fabs(p->dist - dist) < DIST_EPSILON )
		return true;
	return false;
}

/*
================
AddPlaneToHash
================
*/
void AddPlaneToHash( plane_t *p )
{
	int	hash;

	hash = (int)fabs(p->dist) / 8;
	hash &= (PLANE_HASHES-1);

	p->hash_chain = planehash[hash];
	planehash[hash] = p;
}

/*
================
CreateNewFloatPlane
================
*/
int CreateNewFloatPlane( vec3_t normal, vec_t dist )
{
	plane_t	*p, temp;

	if( VectorLength(normal) < 0.5 )
	{
		MsgDev( D_ERROR, "CreateNewFloatPlane: bad normal\n");
		return -1;
	}

	// create a new plane
	if( nummapplanes + 2 > MAX_MAP_PLANES ) Sys_Error( "MAX_MAP_PLANES limit exceeded\n" );

	p = &mapplanes[nummapplanes];
	VectorCopy( normal, p->normal );
	p->dist = dist;
	p->type = (p+1)->type = PlaneTypeForNormal( p->normal );

	VectorSubtract( vec3_origin, normal, (p+1)->normal );
	(p+1)->dist = -dist;

	nummapplanes += 2;

	// allways put axial planes facing positive first
	if( p->type < 3 )
	{
		if( p->normal[0] < 0 || p->normal[1] < 0 || p->normal[2] < 0 )
		{
			// flip order
			temp = *p;
			*p = *(p+1);
			*(p+1) = temp;

			AddPlaneToHash( p+0 );
			AddPlaneToHash( p+1 );
			return nummapplanes - 1;
		}
	}

	AddPlaneToHash( p+0 );
	AddPlaneToHash( p+1 );
	return nummapplanes - 2;
}

/*
==============
SnapVector
==============
*/
void SnapVector( vec3_t normal )
{
	int	i;

	for( i = 0; i < 3; i++ )
	{
		if( fabs(normal[i] - 1) < NORMAL_EPSILON )
		{
			VectorClear (normal);
			normal[i] = 1;
			break;
		}
		if( fabs(normal[i] - -1) < NORMAL_EPSILON )
		{
			VectorClear( normal );
			normal[i] = -1;
			break;
		}
	}
}

/*
==============
SnapPlane
==============
*/
void SnapPlane( vec3_t normal, vec_t *dist )
{
	SnapVector( normal );

	if(fabs( *dist - floor( *dist + 0.5 )) < DIST_EPSILON )
		*dist = floor( *dist + 0.5 );
}

/*
=============
FindFloatPlane

=============
*/
#ifndef USE_HASHING
int		FindFloatPlane (vec3_t normal, vec_t dist)
{
	int		i;
	plane_t	*p;

	SnapPlane (normal, &dist);
	for (i=0, p=mapplanes ; i<nummapplanes ; i++, p++)
	{
		if (PlaneEqual (p, normal, dist))
			return i;
	}

	return CreateNewFloatPlane (normal, dist);
}
#else
int		FindFloatPlane (vec3_t normal, vec_t dist)
{
	int		i;
	plane_t	*p;
	int		hash, h;

	SnapPlane (normal, &dist);
	hash = (int)fabs(dist) / 8;
	hash &= (PLANE_HASHES-1);

	// search the border bins as well
	for (i=-1 ; i<=1 ; i++)
	{
		h = (hash+i)&(PLANE_HASHES-1);
		for (p = planehash[h] ; p ; p=p->hash_chain)
		{
			if (PlaneEqual (p, normal, dist))
				return p-mapplanes;
		}
	}

	return CreateNewFloatPlane (normal, dist);
}
#endif

/*
================
PlaneFromPoints
================
*/
int PlaneFromPoints (vec_t *p0, vec_t *p1, vec_t *p2)
{
	vec3_t	t1, t2, normal;
	vec_t	dist;

	VectorSubtract (p0, p1, t1);
	VectorSubtract (p2, p1, t2);
	CrossProduct (t1, t2, normal);
	VectorNormalize (normal);

	dist = DotProduct (p0, normal);

	return FindFloatPlane (normal, dist);
}


//====================================================================


/*
===========
SetBrushContents
===========
*/
void SetBrushContents( bspbrush_t *b )
{
	int	brush_contents;
	int	brush_surface;
	side_t	*s;
	int	i;

	s = &b->sides[0];
	brush_contents = s->contents;
	brush_surface = texinfo[s->texinfo].flags;

	for( i = 1; i < b->numsides; i++, s++ )
	{
		s = &b->sides[i];
		brush_surface |= texinfo[s->texinfo].flags; 
		brush_contents |= s->contents; // multi-contents support

		// merge contents for every side and store it for brushtexture too
		// translucent objects are automatically classified as detail
		if( s->surf & ( SURF_TRANS33|SURF_TRANS66 ))
		{
			s->contents |= CONTENTS_DETAIL;
			s->texture->contents |= CONTENTS_DETAIL;
			brush_contents |= CONTENTS_DETAIL;
		}
		if( s->contents & ( CONTENTS_CLIP ))
		{
			s->contents |= CONTENTS_DETAIL;
			s->texture->contents |= CONTENTS_DETAIL;
			brush_contents |= CONTENTS_DETAIL;
		}
		if(!(s->contents & ((LAST_VISIBLE_CONTENTS-1)|CONTENTS_CLIP|CONTENTS_MIST)))
		{
			s->contents |= CONTENTS_SOLID;
			s->texture->contents |= CONTENTS_SOLID;
			brush_contents |= CONTENTS_SOLID;
		}
		if( s->surf & ( SURF_HINT|SURF_SKIP ))
		{
			// hints and skips are never detail, and have no content
			brush_contents = s->contents = s->texture->contents = 0;
		}

	}

	if( brush_contents & CONTENTS_DETAIL )
		b->detail = true;
	else b->detail = false;

	// if any side is translucent, mark the contents and change solid to window
	if( brush_surface & (SURF_TRANS33|SURF_TRANS66))
	{
		brush_contents |= CONTENTS_TRANSLUCENT;
		if( brush_contents & CONTENTS_SOLID )
		{
			brush_contents &= ~CONTENTS_SOLID;
			brush_contents |= CONTENTS_WINDOW;
		}
	}
	if( brush_contents & CONTENTS_TRANSLUCENT )
		b->opaque = false;
	else b->opaque = true;

	// total contents from all sides
	b->contents = brush_contents;
}


//============================================================================

/*
=================
AddBrushBevels

Adds any additional planes necessary to allow the brush being
built to be expanded against axial bounding boxes
=================
*/
void AddBrushBevels( void )
{
	int		axis, dir;
	int		i, j, k, l, order = 0;
	side_t		*s, *s2, sidetemp;
	vec3_t		normal, vec, vec2;
	float		dist, d;
	winding_t		*w, *w2;

	// add the axial planes
	for( axis = 0; axis < 3; axis++ )
	{
		for( dir = -1; dir <= 1; dir += 2, order++ )
		{
			// see if the plane is allready present
			for( i = 0, s = buildBrush->sides; i < buildBrush->numsides; i++, s++ )
			{
				if( mapplanes[s->planenum].normal[axis] == dir )
					break;
			}

			if( i == buildBrush->numsides )
			{	
				// add a new side
				if( buildBrush->numsides == MAX_BUILD_SIDES )
					Sys_Break( "MAX_BUILD_SIDES limit exceeded\n" );
				memset( s, 0, sizeof( *s ));
				buildBrush->numsides++;
				VectorClear( normal );
				normal[axis] = dir;
				if( dir == 1 ) dist = buildBrush->maxs[axis];
				else dist = -buildBrush->mins[axis];
				s->planenum = FindFloatPlane (normal, dist);
				s->contents = buildBrush->sides[0].contents;
				s->texinfo = buildBrush->sides[0].texinfo;
				s->texture = buildBrush->sides[0].texture;
				s->bevel = true;
				c_boxbevels++;
			}

			// if the plane is not in it canonical order, swap it
			if( i != order )
			{
				sidetemp = buildBrush->sides[order];
				buildBrush->sides[order] = buildBrush->sides[i];
				buildBrush->sides[i] = sidetemp;
			}
		}
	}

	// add the edge bevels
	if( buildBrush->numsides == 6 ) return; // pure axial

	// test the non-axial plane edges
	// this code tends to cause some problems...
	for( i = 6; i < buildBrush->numsides; i++ )
	{
		s = buildBrush->sides + i;
		w = s->winding;
		if( !w ) continue;

		for( j = 0; j < w->numpoints; j++ )
		{
			k = (j+1)%w->numpoints;
			VectorSubtract( w->p[j], w->p[k], vec );
			if( VectorNormalizeLength( vec ) < 0.5f )
				continue;
			SnapVector( vec );
			for( k = 0; k < 3; k++ )
				if ( vec[k] == -1 || vec[k] == 1)
					break; // axial
			if( k != 3 ) continue; // only test non-axial edges

			// try the six possible slanted axials from this edge
			for( axis = 0; axis < 3; axis++ )
			{
				for( dir = -1; dir <= 1; dir += 2 )
				{
					// construct a plane
					VectorClear( vec2 );
					vec2[axis] = dir;
					CrossProduct( vec, vec2, normal );
					if( VectorNormalizeLength( normal ) < 0.5f )
						continue;
					dist = DotProduct (w->p[j], normal);

					// if all the points on all the sides are
					// behind this plane, it is a proper edge bevel
					for( k = 0; k < buildBrush->numsides; k++ )
					{
						// if this plane has allready been used, skip it
						if( PlaneEqual( &mapplanes[buildBrush->sides[k].planenum], normal, dist ))
							break;

						w2 = buildBrush->sides[k].winding;
						if( !w2 ) continue;

						for( l = 0; l < w2->numpoints; l++ )
						{
							d = DotProduct (w2->p[l], normal) - dist;
							if( d > 0.1f ) break; // point in front
						}
						if( l != w2->numpoints ) break;
					}

					// wasn't part of the outer hull
					if( k != buildBrush->numsides ) continue;

					// add this plane
					if( buildBrush->numsides == MAX_BUILD_SIDES )
						Sys_Break( "MAX_BUILD_SIDES limit exceeded\n" );
					s2 = &buildBrush->sides[buildBrush->numsides];
					buildBrush->numsides++;
					memset( s2, 0, sizeof( *s2 ));

					s2->planenum = FindFloatPlane (normal, dist);
					s2->contents = buildBrush->sides[0].contents;
					s2->texinfo = buildBrush->sides[0].texinfo;
					s2->texture = buildBrush->sides[0].texture;
					s2->bevel = true;
					c_edgebevels++;
				}
			}
		}
	}
}

void ParseRawBrush( void )
{
	int		mt, i, j;
	int		planenum;
	vec_t		planepts[3][3];	// quark used float coords
	side_t		*side;
	shader_t		*si;
	brush_texture_t	td;

	buildBrush->numsides = 0;
	buildBrush->detail = false;
	
	while( 1 )
	{
		g_TXcommand = 0;
		if( !Com_GetToken( true )) break;
		if( Com_MatchToken( "}" )) break;

		side = &buildBrush->sides[buildBrush->numsides];
		memset( side, 0, sizeof( *side ));

		// read the three point plane definition
		for( i = 0; i < 3; i++ )
		{
			if( i != 0 ) Com_GetToken( true );
			if(!Com_MatchToken( "(" )) Sys_Break( "ParseBrush: error parsing %d\n", buildBrush->brushnum );
			
			for( j = 0; j < 3; j++ )
			{
				Com_GetToken( false );
				planepts[i][j] = com.atof( com_token );
			}
			
			Com_GetToken( false );
			if(!Com_MatchToken( ")" )) Sys_Break( "ParseBrush: missing \")\" in brush definition\n" );
		}

		// read the texturedef
		Com_GetToken( false );
		com.strcpy( td.name, com_token );

		if( g_brushtype == BRUSH_WORLDCRAFT_22 ) // Worldcraft 2.2+
                    {
			// texture U axis
			Com_GetToken( false );
			if(!Com_MatchToken("[")) Sys_Break( "missing '[' in texturedef (U)\n" );
			Com_GetToken(false);
			td.vects.valve.UAxis[0] = com.atof(com_token);
			Com_GetToken(false);
			td.vects.valve.UAxis[1] = com.atof(com_token);
			Com_GetToken(false);
			td.vects.valve.UAxis[2] = com.atof(com_token);
			Com_GetToken(false);
			td.vects.valve.shift[0] = com.atof(com_token);
			Com_GetToken(false);
			if (strcmp(com_token, "]")) Sys_Break( "missing ']' in texturedef (U)\n" );

			// texture V axis
			Com_GetToken( false );
			if (strcmp(com_token, "[")) Sys_Break( "missing '[' in texturedef (V)\n" );
			Com_GetToken( false );
			td.vects.valve.VAxis[0] = com.atof( com_token );
			Com_GetToken( false );
			td.vects.valve.VAxis[1] = com.atof( com_token );
			Com_GetToken( false );
			td.vects.valve.VAxis[2] = com.atof( com_token );
			Com_GetToken( false );
			td.vects.valve.shift[1] = com.atof( com_token );
			Com_GetToken( false );
			if(com.strcmp( com_token, "]")) Sys_Break( "missing ']' in texturedef (V)\n");

			// texture rotation is implicit in U/V axes.
			Com_GetToken(false);
			td.vects.valve.rotate = 0;

			// texure scale
			Com_GetToken( false );
			td.vects.valve.scale[0] = com.atof( com_token );
			Com_GetToken(false);
			td.vects.valve.scale[1] = com.atof( com_token );
                    }
		else if( g_brushtype == BRUSH_WORLDCRAFT_21 )
		{
			// worldcraft 2.1-, Radiant
			Com_GetToken( false );
			td.vects.valve.shift[0] = com.atof(com_token);
			Com_GetToken( false );
			td.vects.valve.shift[1] = com.atof(com_token);
			Com_GetToken( false );
			td.vects.valve.rotate = com.atof(com_token);	
			Com_GetToken( false );
			td.vects.valve.scale[0] = com.atof(com_token);
			Com_GetToken( false );
			td.vects.valve.scale[1] = com.atof(com_token);
                    }

		if(( g_TXcommand == '1' || g_TXcommand == '2' ))
		{
			// We are QuArK mode and need to translate some numbers to align textures its way
			// from QuArK, the texture vectors are given directly from the three points
			vec3_t          TexPt[2];
			float           dot22, dot23, dot33, mdet, aa, bb, dd;
			int             k;

			k = g_TXcommand - '0';
			for( j = 0; j < 3; j++ )
			{
				TexPt[1][j] = (planepts[k][j] - planepts[0][j]) * ScaleCorrection;
            		}

			k = 3 - k;
			for( j = 0; j < 3; j++ )
			{
				TexPt[0][j] = (planepts[k][j] - planepts[0][j]) * ScaleCorrection;
			}

			dot22 = DotProduct( TexPt[0], TexPt[0] );
			dot23 = DotProduct( TexPt[0], TexPt[1] );
			dot33 = DotProduct( TexPt[1], TexPt[1] );
			mdet = dot22 * dot33 - dot23 * dot23;
			if( mdet < 1E-6 && mdet > -1E-6 )
			{
				aa = bb = dd = 0;
				Msg( "Degenerate texcoords: Entity %i, Brush %i\n", buildBrush->entitynum, buildBrush->brushnum );
			}
			else
			{
				mdet = 1.0 / mdet;
				aa = dot33 * mdet;
				bb = -dot23 * mdet;
				dd = dot22 * mdet;
			}
			for( j = 0; j < 3; j++ )
			{
				td.vects.quark.vects[0][j] = aa * TexPt[0][j] + bb * TexPt[1][j];
				td.vects.quark.vects[1][j] = -(bb * TexPt[0][j] + dd * TexPt[1][j]);
			}
			td.vects.quark.vects[0][3] = -DotProduct(td.vects.quark.vects[0], planepts[0]);
			td.vects.quark.vects[1][3] = -DotProduct(td.vects.quark.vects[1], planepts[0]);
		}
		td.txcommand = g_TXcommand;		// Quark stuff, but needs setting always
		td.flags = td.contents = td.value = 0;	// reset all values before setting
		side->contents = side->surf = 0;

		// get size from miptex info
		mt = FindMiptex( td.name );
		td.size[0] = dmiptex[mt].size[0];
		td.size[1] = dmiptex[mt].size[1];

		// get flags and contents from shader
		si = FindShader( td.name );
		if( si )
		{
			int t_next, t_name;

			side->contents = td.contents = si->contents;
			side->surf = td.flags = si->surfaceFlags;
			dmiptex[mt].s_next = t_next = FindMiptex( si->nextframe );
			t_name = dmiptex[t_next].s_name;

			// NOTE: all textures in animchain must be stored into
			// dmiptex array so engine can precache them correctly
			while( t_next && dmiptex[mt].s_name != t_name ) 
			{
				si = FindShader(GetStringFromTable( t_name ));					
				if( !si ) break; // end of animchain
				t_next = dmiptex[t_next].s_next = FindMiptex( si->nextframe );
				t_name = dmiptex[t_next].s_name;
			}
		}
		
		if( Com_TryToken()) // hidden quake2 legacy, but can be used
		{
			// overwrite shader values directly from .map file
			side->contents = td.contents = com.atoi( com_token );
			Com_GetToken( false );
			side->surf = td.flags = com.atoi( com_token );
			Com_GetToken( false );
			td.value = com.atoi(com_token);
		}

		// find the plane number
		planenum = PlaneFromPoints( planepts[0], planepts[1], planepts[2] );
		side = &buildBrush->sides[buildBrush->numsides];
		side->planenum = planenum;
		side->texinfo = TexinfoForBrushTexture( &mapplanes[planenum], &td, vec3_origin );
		// save the td off in case there is an origin brush and we
		// have to recalculate the texinfo
		side->texture = CopyTexture( &td );
		buildBrush->numsides++;

		if( buildBrush->numsides == MAX_BUILD_SIDES )
		{
			Msg( "Entity %i, Brush %i: brush sides limit exceeded\n", buildBrush->entitynum, buildBrush->brushnum );
			break; // we will produce degenerated brush, but it's better than corrupted memory!
		}
	}
}

/*
=================
MergeBrush

Returns false if the brush has a mirrored set of planes,
meaning it encloses no volume.
Also removes planes without any normal
=================
*/
bool MergeBrush( bspbrush_t *b )
{
	int	i, j, k;
	side_t	*sides;

	sides = b->sides;

	for( i = 1; i < b->numsides; i++ )
	{
		// check for a degenerate plane
		if( sides[i].planenum == -1 )
		{
			// just remove it
			Msg( "Entity %i, Brush %i: plane with no normal\n", b->entitynum, b->brushnum );
			for( k = i + 1; k < b->numsides; k++ )
				sides[k-1] = sides[k];
			b->numsides--;
			i--;
			continue;
		}

		// check for duplication and mirroring
		for( j = 0; j < i; j++ )
		{
			if( sides[i].planenum == sides[j].planenum )
			{
				// remove the second duplicate
				Msg( "Entity %i, Brush %i: duplicate plane\n", b->entitynum, b->brushnum );
				for( k = i + 1; k < b->numsides; k++ )
					sides[k-1] = sides[k];
				b->numsides--;
				i--;
				break;
			}

			if( sides[i].planenum == (sides[j].planenum ^ 1))
			{
				// mirror plane, brush is invalid
				Msg( "Entity %i, Brush %i: mirrored plane\n", b->entitynum, b->brushnum );
				return false;
			}
		}
	}
	return true;
}

/*
===============
FinishBrush

Produces a final brush based on the buildBrush->sides array
and links it to the current entity
===============
*/
bspbrush_t *FinishBrush( void )
{
	bspbrush_t	*b;
	int		i;

	// create windings for sides and bounds for brush
	if( !CreateBrushWindings( buildBrush ))
	{
		// don't keep this brush
		return NULL;
	}

	// brushes that will not be visible at all will never be used as bsp splitters
	if( buildBrush->contents & ( CONTENTS_CLIP ))
	{
		c_clipbrushes++;
		for( i = 0; i < buildBrush->numsides; i++ )
			buildBrush->sides[i].texinfo = TEXINFO_NODE;
		buildBrush->detail = true;
	}

	// origin brushes are removed, but they set
	// the rotation origin for the rest of the brushes
	// in the entity.  After the entire entity is parsed,
	// the planenums and texinfos will be adjusted for
	// the origin brush
	if( buildBrush->contents & CONTENTS_ORIGIN )
	{
		vec3_t	size, movedir, origin;
		char	string[32];
			
		if( num_entities == 1 )
		{
			// g-cont. rotating world it's a interesting idea, hmm.....
			Msg( "origin brushes not allowed in world\n" );
			return NULL;
		}

		// calcualte movedir (Xash 0.4 style)
		VectorAverage( buildBrush->mins, buildBrush->maxs, origin );
		VectorSubtract( buildBrush->maxs, buildBrush->mins, size );
		if( size[2] > size[0] && size[2] > size[1] )
		{
            		VectorSet( movedir, 0, 1, 0 );	// x-rotate
		}
		else if( size[1] > size[2] && size[1] > size[0] )
		{
            		VectorSet( movedir, 1, 0, 0 );	// y-rotate
		}
		else if( size[0] > size[2] && size[0] > size[1] )
		{
			VectorSet( movedir, 0, 0, 1 );	// z-rotate
		}

		if(!VectorIsNull( movedir ))
		{
			com.sprintf( string, "%i %i %i", (int)movedir[0], (int)movedir[1], (int)movedir[2]);
			SetKeyValue( &entities[buildBrush->entitynum], "movedir", string );
		}
		if(!VectorIsNull( origin ))
		{
			com.sprintf( string, "%i %i %i", (int)origin[0], (int)origin[1], (int)origin[2]);
			SetKeyValue( &entities[buildBrush->entitynum], "origin", string );
			VectorCopy( origin, entities[buildBrush->entitynum].origin );
		}

		// don't keep this brush
		return NULL;
	}

	AddBrushBevels();

	// keep it
	b = CopyBrush( buildBrush );
	b->entitynum = num_entities-1;
	b->brushnum = entity_numbrushes;
	b->original = b;
	b->next = mapent->brushes;
	mapent->brushes = b;

	return b;
}

/*
=================
ParseBrush
=================
*/
bool ParseBrush( bsp_entity_t *mapent )
{
	bspbrush_t	*b;

	ParseRawBrush();

	buildBrush->entitynum = num_entities - 1;
	buildBrush->brushnum = entity_numbrushes;

	// if there are mirrored planes, the entire brush is invalid
	if( !MergeBrush( buildBrush )) return false;

	// get the content for the entire brush
	SetBrushContents( buildBrush );

	b = FinishBrush();
	if( !b ) return false;
	return true;
}

/*
================
MoveBrushesToWorld

Takes all of the brushes from the current entity and
adds them to the world's brush list.

Used by func_group and func_areaportal
================
*/
void MoveBrushesToWorld( bsp_entity_t *mapent )
{
	bspbrush_t	*b, *next;

	// move brushes
	for( b = mapent->brushes; b; b = next )
	{
		next = b->next;
		b->next = entities[0].brushes;
		entities[0].brushes = b;
	}
	mapent->brushes = NULL;
}

/*
================
AdjustBrushesForOrigin
================
*/
void AdjustBrushesForOrigin( bsp_entity_t *ent )
{
	bspbrush_t	*b;
	side_t		*s;
	vec_t		newdist;
	int		i;

	for ( b = ent->brushes; b; b = b->next )
	{
		for( i = 0; i < b->numsides; i++ )
		{
			s = &b->sides[i];
			newdist = mapplanes[s->planenum].dist - DotProduct( mapplanes[s->planenum].normal, ent->origin );
			s->planenum = FindFloatPlane( mapplanes[s->planenum].normal, newdist );
			s->texinfo = TexinfoForBrushTexture( &mapplanes[s->planenum], s->texture, ent->origin );
		}
		CreateBrushWindings( b );
	}
}

/*
================
ParseMapEntity
================
*/
bool ParseMapEntity( void )
{
	epair_t		*e;

	if(!Com_GetToken( true )) return false;	// end of .map file
	if(!Com_MatchToken( "{" )) Sys_Break( "ParseEntity: found %s instead {\n", com_token );
	if( num_entities == MAX_MAP_ENTITIES ) Sys_Break( "MAX_MAP_ENTITIES limit exceeded\n");

	mapent = &entities[num_entities];
	num_entities++;
	memset( mapent, 0, sizeof( *mapent ));
	entity_numbrushes = 0;

	while( 1 )
	{
		if( !Com_GetToken( true )) Sys_Break( "ParseEntity: EOF without closing brace\n" );
		if( Com_MatchToken( "}" )) break;
		if( Com_MatchToken( "{" ))
		{
			ParseBrush( mapent );
			entity_numbrushes++;
		}
		else
		{
			// parse a key / value pair
			e = ParseEpair();
			if( !com.strcmp( e->key, "mapversion" ))
			{
				if( com.atoi(e->value) == VALVE_FORMAT )
					g_brushtype = BRUSH_WORLDCRAFT_22;
				else g_brushtype = BRUSH_WORLDCRAFT_21;
			}
			e->next = mapent->epairs;
			mapent->epairs = e;
		}
	}

	GetVectorForKey( mapent, "origin", mapent->origin );

	// if there was an origin brush, offset all of the planes and texinfo
	if(!VectorIsNull( mapent->origin )) AdjustBrushesForOrigin( mapent );

	// group entities are just for editor convenience
	// toss all brushes into the world entity
	if( !com.strcmp( "func_group", ValueForKey( mapent, "classname" )))
	{
		MoveBrushesToWorld( mapent );
		num_entities--;
		return true;
	}

	// areaportal entities move their brushes, but don't eliminate the entity
	if( !com.strcmp( "func_areaportal", ValueForKey( mapent, "classname" )))
	{
		if( entity_numbrushes != 1 )
		{
			Msg( "func_areaportal #%i can only be a single brush, removed\n", num_entities - 1 );
			num_entities--;
		}
		else
		{
			bspbrush_t *b = mapent->brushes;
			b->contents = CONTENTS_AREAPORTAL;
			c_areaportals++;
			mapent->areaportalnum = c_areaportals;
			// set the portal number as "areaportal"
			SetKeyValue( mapent, "areaportal", va( "%i", c_areaportals ));
			MoveBrushesToWorld( mapent );
		}
	}
	return true;
}

//===================================================================

/*
================
LoadMapFile
================
*/
void LoadMapFile( void )
{		
	bspbrush_t	*b;
		
	num_entities = 0;
	g_brushtype = BRUSH_UNKNOWN;
	
	if(!Com_LoadScript( va( "maps/%s.map", gs_filename ), NULL, 0 ))
		Sys_Break( "can't loading map file %s.map\n", gs_filename );

	// allocate a very large temporary brush for building
	// the brushes as they are loaded
	buildBrush = AllocBrush( MAX_BUILD_SIDES );
	
	while(ParseMapEntity( ));

	ClearBounds( map_mins, map_maxs );
	for( b = entities[0].brushes; b; b = b->next )
	{
		AddPointToBounds( b->mins, map_mins, map_maxs );
		AddPointToBounds( b->maxs, map_mins, map_maxs );
	}

	VectorSubtract( map_maxs, map_mins, map_size );
	MsgDev(D_INFO, "%5i total world brushes\n", CountBrushList( entities[0].brushes ));
	MsgDev(D_INFO, "%5i clipbrushes\n", c_clipbrushes );
	MsgDev(D_INFO, "%5i entities\n", num_entities );
	MsgDev(D_INFO, "%5i planes\n", nummapplanes );
	MsgDev(D_INFO, "%5i areaportals\n", c_areaportals );
	MsgDev(D_INFO, "world size %5.0f %5.0f %5.0f\n", map_size[0], map_size[1], map_size[2] );
}