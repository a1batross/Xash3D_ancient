//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	   map.c - parse map script
//=======================================================================

#include "bsplib.h"
#include "const.h"

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
script_t		*mapfile;
bsp_entity_t	*mapent;

int		c_boxbevels;
int		c_edgebevels;
int		c_areaportals;
int		c_structural;
int		c_detail;

const char *g_sMapType[BRUSH_COUNT] =
{
"unknown",
"Worldcraft 2.1",
"Valve Hammer 3.4",
"Radiant",
"QuArK"
};

/*
=============================================================================

PLANE FINDING

=============================================================================
*/
/*
=================
PlaneTypeForNormal
=================
*/
int PlaneTypeForNormal( vec3_t normal )
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

bool PlaneEqual( plane_t *p, vec3_t normal, vec_t dist )
{
	if( fabs(p->normal[0] - normal[0]) < NORMAL_EPSILON && fabs(p->normal[1] - normal[1]) < NORMAL_EPSILON
	&& fabs(p->normal[2] - normal[2]) < NORMAL_EPSILON && fabs(p->dist - dist) < COLLISION_PLANE_DIST_EPSILON )
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

	hash = (int)fabs(p->dist) / (int)COLLISION_SNAPSCALE;
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

	if(fabs( *dist - floor( *dist + 0.5 )) < COLLISION_SNAP )
		*dist = floor( *dist + 0.5 );
}

/*
=============
FindFloatPlane

=============
*/
int FindFloatPlane( vec3_t normal, vec_t dist )
{
	int		i;
	plane_t	*p;
	int		hash, h;

	SnapPlane (normal, &dist);
	hash = (int)fabs(dist) / (int)COLLISION_SNAPSCALE;
	hash &= (PLANE_HASHES-1);

	// search the border bins as well
	for( i = -1; i <= 1; i++ )
	{
		h = (hash+i)&(PLANE_HASHES-1);
		for( p = planehash[h]; p; p = p->hash_chain )
		{
			if( PlaneEqual( p, normal, dist ))
				return p - mapplanes;
		}
	}

	return CreateNewFloatPlane( normal, dist );
}

/*
================
MapPlaneFromPoints
================
*/
int MapPlaneFromPoints (vec_t *p0, vec_t *p1, vec_t *p2)
{
	vec3_t	t1, t2, normal;
	vec_t	dist;

	VectorSubtract( p0, p1, t1 );
	VectorSubtract( p2, p1, t2 );
	CrossProduct( t1, t2, normal );
	VectorNormalize( normal );

	dist = DotProduct( p0, normal );

	return FindFloatPlane( normal, dist );
}

/*
==================
textureAxisFromPlane
==================
*/
vec3_t baseaxis[18] =
{
{0,0,1 },	{1,0,0},	{0,-1,0},	// floor
{0,0,-1}, {1,0,0},	{0,-1,0},	// ceiling
{1,0,0 },	{0,1,0},	{0,0,-1},	// west wall
{-1,0,0},	{0,1,0},	{0,0,-1},	// east wall
{0,1,0 },	{1,0,0},	{0,0,-1},	// south wall
{0,-1,0},	{1,0,0},	{0,0,-1},	// north wall
};

void TextureAxisFromPlane( plane_t *plane, vec3_t xv, vec3_t yv )
{
	int	bestaxis = 0;
	vec_t	dot, best = 0;
	int	i;
	
	for( i = 0; i < 6; i++ )
	{
		dot = DotProduct( plane->normal, baseaxis[i*3] );
		if( dot > best )
		{
			best = dot;
			bestaxis = i;
		}
	}
	
	VectorCopy( baseaxis[bestaxis*3+1], xv );
	VectorCopy( baseaxis[bestaxis*3+2], yv );
}

//====================================================================


/*
===========
SetBrushContents
===========
*/
void SetBrushContents( bspbrush_t *b )
{
	int	contents, c2;
	bool	mixed = false;
	int	allFlags = 0;
	side_t	*s;
	int	i;

	s = &b->sides[0];
	contents = c2 = s->contents;

	for( i = 1; i < b->numsides; i++, s++ )
	{
		s = &b->sides[i];

		if( !s->shader ) continue;
		c2 = s->contents;
		if( c2 != contents ) mixed = true;
		allFlags |= s->surfaceFlags;

		// multi-contents support
		// FIXME: test it
		// brush_contents |= c2;
	}

	// just throw warning
	if( mixed ) MsgDev( D_WARN, "Entity %i, Brush %i: mixed face contents\n", b->entitynum, b->brushnum );

	if(( contents & CONTENTS_DETAIL ) && ( contents & CONTENTS_STRUCTURAL ))
	{
		MsgDev( D_WARN, "Entity %i, Brush %i: mixed DETAIL and STRUCTURAL contents\n", num_entities - 1, entity_numbrushes );
		contents &= ~CONTENTS_DETAIL;
	}

	// all translucent brushes that aren't specirically made structural will
	// be detail
	if(( contents & CONTENTS_TRANSLUCENT ) && !( contents & CONTENTS_STRUCTURAL ))
		contents |= CONTENTS_DETAIL;

	if( contents & CONTENTS_DETAIL )
	{
		c_detail++;
		b->detail = true;
	}
	else
	{
		c_structural++;
		b->detail = false;
	}

	if( contents & CONTENTS_TRANSLUCENT )
		b->opaque = false;
	else b->opaque = true;

	if( contents & CONTENTS_AREAPORTAL )
		c_areaportals++;
	b->contents = contents;
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
				s->planenum = FindFloatPlane( normal, dist );
				
				s->contents = buildBrush->sides[0].contents;
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
					dist = DotProduct( w->p[j], normal );

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

					s2->planenum = FindFloatPlane( normal, dist );
					s2->contents = buildBrush->sides[0].contents;
					s2->bevel = true;
					c_edgebevels++;
				}
			}
		}
	}
}

void ParseRawBrush( void )
{
	int		i, j;
	int		planenum;
	vec_t		ang, sinv, cosv;
	vec_t		planepts[3][3];	// quark used float coords
	vec3_t		vecs[2];
	int		sv, tv;
	vec_t		ns, nt;
	token_t		token;
	vects_t		vects;
	side_t		*side;
	bsp_shader_t	*si;

	buildBrush->numsides = 0;
	buildBrush->detail = false;

	if( g_brushtype == BRUSH_RADIANT ) Com_CheckToken( "{" );

	while( 1 )
	{
		if( !Com_ReadToken( mapfile, SC_ALLOW_NEWLINES|SC_COMMENT_SEMICOLON, &token ))
			break;

		if( !com.stricmp( token.string, "}" )) break;
		if( g_brushtype == BRUSH_RADIANT )
		{
			while( 1 )
			{
				if( com.strcmp( token.string, "(" ))
					Com_ReadToken( mapfile, 0, &token );
				else break;
				Com_ReadToken( mapfile, SC_ALLOW_NEWLINES, &token );
			}
		}

		if( buildBrush->numsides == MAX_BUILD_SIDES )
			Sys_Break( "MAX_BUILD_SIDES brush limit exceeded\n" );
		Com_SaveToken( mapfile, &token );

		side = &buildBrush->sides[buildBrush->numsides];
		Mem_Set( side, 0, sizeof( *side ) );
		buildBrush->numsides++;

		// read the three point plane definition
		Com_Parse1DMatrix( 3, planepts[0] );
		Com_Parse1DMatrix( 3, planepts[1] );
		Com_Parse1DMatrix( 3, planepts[2] );

		if( g_brushtype == BRUSH_RADIANT )
			Com_Parse2DMatrix( 2, 3, (float *)side->matrix );

		// read the texturedef
		Com_ReadToken( mapfile, SC_ALLOW_PATHNAMES|SC_PARSE_GENERIC, &token );
		si = FindShader( token.string );	// register shader
		side->shader = si;
		side->contents = si->contents;
		side->surfaceFlags = si->surfaceFlags;
		side->value = si->value;

		if( g_brushtype == BRUSH_WORLDCRAFT_22 ) // Worldcraft 2.2+
                    {
			// texture U axis
			Com_ReadToken( mapfile, 0, &token );
			if( com.strcmp( token.string, "[")) Sys_Break( "missing '[' in texturedef (U)\n" );
			Com_ReadFloat( mapfile, false, &vects.valve.UAxis[0] );
			Com_ReadFloat( mapfile, false, &vects.valve.UAxis[1] );
			Com_ReadFloat( mapfile, false, &vects.valve.UAxis[2] );
			Com_ReadFloat( mapfile, false, &vects.valve.shift[0] );
			Com_ReadToken( mapfile, 0, &token );
			if( com.strcmp( token.string, "]")) Sys_Break( "missing ']' in texturedef (U)\n" );

			// texture V axis
			Com_ReadToken( mapfile, 0, &token );
			if( com.strcmp( token.string, "[")) Sys_Break( "missing '[' in texturedef (V)\n" );
			Com_ReadFloat( mapfile, false, &vects.valve.VAxis[0] );
			Com_ReadFloat( mapfile, false, &vects.valve.VAxis[1] );
			Com_ReadFloat( mapfile, false, &vects.valve.VAxis[2] );
			Com_ReadFloat( mapfile, false, &vects.valve.shift[1] );
			Com_ReadToken( mapfile, 0, &token );
			if( com.strcmp( token.string, "]")) Sys_Break( "missing ']' in texturedef (V)\n");

			// texture rotation is implicit in U/V axes.
			Com_ReadToken( mapfile, 0, &token );
			vects.valve.rotate = 0;

			// texure scale
			Com_ReadFloat( mapfile, false, &vects.valve.scale[0] );
			Com_ReadFloat( mapfile, false, &vects.valve.scale[1] );
                    }
		else if( g_brushtype == BRUSH_WORLDCRAFT_21 || g_brushtype == BRUSH_QUARK )
		{
			// worldcraft 2.1-, old Radiant, QuArK
			Com_ReadFloat( mapfile, false, &vects.valve.shift[0] );
			Com_ReadFloat( mapfile, false, &vects.valve.shift[1] );
			Com_ReadFloat( mapfile, false, &vects.valve.rotate );
			Com_ReadFloat( mapfile, false, &vects.valve.scale[0] );
			Com_ReadFloat( mapfile, SC_COMMENT_SEMICOLON, &vects.valve.scale[1] );
                    }

		// hidden q2/q3 legacy, but may be used
		if( g_brushtype != BRUSH_QUARK && Com_ReadLong( mapfile, SC_COMMENT_SEMICOLON, &side->contents ))
		{
			// overwrite shader values directly from .map file
			Com_ReadLong( mapfile, false, &side->surfaceFlags );
			Com_ReadLong( mapfile, false, &side->value );
		}
		if(( mapfile->TXcommand == '1' || mapfile->TXcommand == '2' ))
		{
			// We are QuArK mode and need to translate some numbers to align textures its way
			// from QuArK, the texture vectors are given directly from the three points
			vec3_t          TexPt[2];
			float           dot22, dot23, dot33, mdet, aa, bb, dd;
			int             k;

			g_brushtype = BRUSH_QUARK;
			k = mapfile->TXcommand - '0';
			for( j = 0; j < 3; j++ )
			{
				TexPt[1][j] = (planepts[k][j] - planepts[0][j]) * (1.0/128.0);
            		}

			k = 3 - k;
			for( j = 0; j < 3; j++ )
			{
				TexPt[0][j] = (planepts[k][j] - planepts[0][j]) * (1.0/128.0);
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
				vects.quark.vects[0][j] = aa * TexPt[0][j] + bb * TexPt[1][j];
				vects.quark.vects[1][j] = -(bb * TexPt[0][j] + dd * TexPt[1][j]);
			}
			vects.quark.vects[0][3] = -DotProduct( vects.quark.vects[0], planepts[0]);
			vects.quark.vects[1][3] = -DotProduct( vects.quark.vects[1], planepts[0]);
		}

		// find the plane number
		planenum = MapPlaneFromPoints( planepts[0], planepts[1], planepts[2] );
		side->planenum = planenum;

		if( g_brushtype == BRUSH_QUARK ) 
		{
			// QuArK format completely matched with internal
			Mem_Copy( side->vecs, vects.quark.vects, sizeof( side->vecs ));
		}
		else if( g_brushtype != BRUSH_RADIANT )
		{
			if( g_brushtype == BRUSH_WORLDCRAFT_21 )
				TextureAxisFromPlane( &mapplanes[planenum], vecs[0], vecs[1] );
			if( !vects.valve.scale[0] ) vects.valve.scale[0] = 1.0f;
			if( !vects.valve.scale[1] ) vects.valve.scale[1] = 1.0f;

			if( g_brushtype == BRUSH_WORLDCRAFT_21 )
			{
				// rotate axis
				if( vects.valve.rotate == 0 )
				{
					sinv = 0;
					cosv = 1;
				}
				else if( vects.valve.rotate == 90 )
				{
					sinv = 1;
					cosv = 0;
				}
				else if( vects.valve.rotate == 180 )
				{
					sinv = 0;
					cosv = -1;
				}
				else if( vects.valve.rotate == 270 )
				{
					sinv = -1;
					cosv = 0;
				}
				else
				{
					ang = vects.valve.rotate / 180 * M_PI;
					sinv = com.sin( ang );
					cosv = com.cos( ang );
				}
				if( vecs[0][0] ) sv = 0;
				else if( vecs[0][1] ) sv = 1;
				else sv = 2;

				if( vecs[1][0] ) tv = 0;
				else if( vecs[1][1] ) tv = 1;
				else tv = 2;
			
				for( i = 0; i < 2; i++ )
				{
					ns = cosv * vecs[i][sv] - sinv * vecs[i][tv];
					nt = sinv * vecs[i][sv] + cosv * vecs[i][tv];
					vecs[i][sv] = ns;
					vecs[i][tv] = nt;
				}

				for( i = 0; i < 2; i++ )
					for( j = 0; j < 3; j++ )
						side->vecs[i][j] = vecs[i][j] / vects.valve.scale[i];
			}
			else if( g_brushtype == BRUSH_WORLDCRAFT_22 )
			{
				vec_t	scale;

				scale = 1.0f / vects.valve.scale[0];
				VectorScale( vects.valve.UAxis, scale, side->vecs[0] );
				scale = 1.0f / vects.valve.scale[1];
				VectorScale( vects.valve.VAxis, scale, side->vecs[1] );
			}

			side->vecs[0][3] = vects.valve.shift[0];
			side->vecs[1][3] = vects.valve.shift[1];
		}

		if( buildBrush->numsides == MAX_BUILD_SIDES )
		{
			Msg( "Entity %i, Brush %i: brush sides limit exceeded\n", buildBrush->entitynum, buildBrush->brushnum );
			break; // we will produce degenerated brush, but it's better than corrupted memory!
		}
	}
	if( g_brushtype == BRUSH_RADIANT )
	{
		Com_SaveToken( mapfile, &token );
		Com_CheckToken( "}" );
		Com_CheckToken( "}" );
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

	// create windings for sides and bounds for brush
	if( !CreateBrushWindings( buildBrush ))
	{
		// don't keep this brush
		return NULL;
	}

	// brushes that will not be visible at all will never be used as bsp splitters
	if( buildBrush->contents & ( CONTENTS_CLIP ))
	{
		buildBrush->detail = true;
		c_detail++;
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
		else VectorClear( movedir ); // custom movedir

		if(!VectorIsNull( movedir ))
		{
			com.sprintf( string, "%i %i %i", (int)movedir[0], (int)movedir[1], (int)movedir[2]);
			SetKeyValue( &entities[num_entities - 1], "movedir", string );
		}
		if(!VectorIsNull( origin ))
		{
			com.sprintf( string, "%i %i %i", (int)origin[0], (int)origin[1], (int)origin[2]);
			SetKeyValue( &entities[num_entities - 1], "origin", string );
			VectorCopy( origin, entities[num_entities - 1].origin );
		}

		// don't keep this brush
		return NULL;
	}

	if( buildBrush->contents & CONTENTS_AREAPORTAL )
	{
		if( num_entities != 1 )
		{
			MsgDev( D_ERROR, "Entity %i, Brush %i: areaportals only allowed in world\n", num_entities - 1, entity_numbrushes );
			return NULL;
		}
	}
	AddBrushBevels();

	// keep it
	b = CopyBrush( buildBrush );
	b->entitynum = num_entities - 1;
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

	buildBrush->portalareas[0] = -1;
	buildBrush->portalareas[1] = -1;
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
	epair_t	*e;
	token_t	token;

	if( !Com_ReadToken( mapfile, SC_ALLOW_NEWLINES|SC_COMMENT_SEMICOLON, &token ))
		return false; // end of .map file
	if( com.stricmp( token.string, "{" )) Sys_Break( "ParseEntity: found %s instead {\n", token.string );
	if( num_entities == MAX_MAP_ENTITIES ) Sys_Break( "MAX_MAP_ENTITIES limit exceeded\n");

	mapent = &entities[num_entities];
	num_entities++;
	Mem_Set( mapent, 0, sizeof( *mapent ));
	entity_numbrushes = 0;

	while( 1 )
	{
		if( !Com_ReadToken( mapfile, SC_ALLOW_NEWLINES|SC_COMMENT_SEMICOLON, &token ))
			Sys_Break( "ParseEntity: EOF without closing brace\n" );
		if( !com.stricmp( token.string, "}" )) break;
		if( !com.stricmp( token.string, "{" ))
		{
			// parse a brush or patch
			if( !Com_ReadToken( mapfile, SC_ALLOW_NEWLINES, &token )) break;
			if( !com.stricmp( token.string, "patchDef2" ))
			{
				g_brushtype = BRUSH_RADIANT;
				Com_SkipRestOfLine( mapfile );	// Xash3D not supported patches
			}
			else if( !com.stricmp( token.string, "terrainDef" ))
			{
				g_brushtype = BRUSH_RADIANT;
				ParseTerrain();
			}
			else if( !com.stricmp( token.string, "brushDef" ))
			{
				g_brushtype = BRUSH_RADIANT;
				if(ParseBrush( mapent )) // parse brush primitive
					entity_numbrushes++;
			}
			else
			{
				if( g_brushtype == BRUSH_UNKNOWN )
					g_brushtype = BRUSH_WORLDCRAFT_21;
				// QuArK or Worldcraft map
				Com_SaveToken( mapfile, &token );
				if( ParseBrush( mapent )) entity_numbrushes++;
			}
		}
		else
		{
			// parse a key / value pair
			e = ParseEpair( &token );
			if( !com.strcmp( e->key, "mapversion" ))
			{
				if( com.atoi( e->value ) == VALVE_FORMAT )
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
		if(!com.strcmp( "1", ValueForKey( mapent, "terrain" )))
			SetTerrainTextures();
		MoveBrushesToWorld( mapent );
		num_entities--;
		return true;
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
	numdrawsurfs = 0;
	c_detail = 0;
	g_brushtype = BRUSH_UNKNOWN;
	
	mapfile = Com_OpenScript( va( "maps/%s.map", gs_filename ), NULL, 0 );
	if( !mapfile ) Sys_Break( "can't loading map file %s.map\n", gs_filename );

	// allocate a very large temporary brush for building
	// the brushes as they are loaded
	buildBrush = AllocBrush( MAX_BUILD_SIDES );
	
	while(ParseMapEntity( ));

	Com_CloseScript( mapfile );
	ClearBounds( map_mins, map_maxs );
	for( b = entities[0].brushes; b; b = b->next )
	{
		AddPointToBounds( b->mins, map_mins, map_maxs );
		AddPointToBounds( b->maxs, map_mins, map_maxs );
	}

	VectorSubtract( map_maxs, map_mins, map_size );
	MsgDev(D_INFO, "%5i total world brushes\n", CountBrushList( entities[0].brushes ));
	MsgDev(D_INFO, "%5i detail brushes\n", c_detail );
	MsgDev(D_INFO, "%5i boxbevels\n", c_boxbevels );
	MsgDev(D_INFO, "%5i edgebevels\n", c_edgebevels );
	MsgDev(D_INFO, "%5i entities\n", num_entities );
	MsgDev(D_INFO, "%5i planes\n", nummapplanes );
	MsgDev(D_INFO, "%5i areaportals\n", c_areaportals );
	MsgDev(D_INFO, "world size %5.0f %5.0f %5.0f\n", map_size[0], map_size[1], map_size[2] );
}