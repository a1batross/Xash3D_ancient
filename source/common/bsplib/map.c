//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	   map.c - parse map script
//=======================================================================

#include "bsplib.h"
#include "const.h"

extern bool onlyents;

#define PLANE_HASHES	1024
#define MAPTYPE()		Msg( "map type: %s\n", g_sMapType[g_brushtype] )
#define NORMAL_EPSILON	0.00001
#define DIST_EPSILON	0.01

script_t		*mapfile;
int		nummapbrushes;
mapbrush_t	mapbrushes[MAX_MAP_BRUSHES];
int		nummapbrushsides;
side_t		brushsides[MAX_MAP_SIDES];
brush_texture_t	side_brushtextures[MAX_MAP_SIDES];
int		nummapplanes;
plane_t		mapplanes[MAX_MAP_PLANES];
plane_t		*planehash[PLANE_HASHES];
vec3_t		map_mins, map_maxs, map_size;
int		g_mapversion;
int		g_brushtype;

int		c_boxbevels;
int		c_edgebevels;
int		c_areaportals;
int		c_clipbrushes;

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
bool PlaneEqual( plane_t *p, vec3_t normal, vec_t dist )
{
	float	ne, de;

	ne = NORMAL_EPSILON;
	de = DIST_EPSILON;

	if( fabs(p->normal[0] - normal[0]) < ne && fabs(p->normal[1] - normal[1]) < ne
	&& fabs(p->normal[2] - normal[2]) < ne && fabs(p->dist - dist) < de )
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
	uint	hash;

	hash = (PLANE_HASHES - 1) & (int)fabs( p->dist );
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
	if( nummapplanes + 2 > MAX_MAP_PLANES )
		Sys_Error( "MAX_MAP_PLANES limit exceeded\n" );

	p = &mapplanes[nummapplanes];
	VectorCopy( normal, p->normal );
	p->dist = dist;
	p->type = (p+1)->type = PlaneTypeForNormal( p->normal );

	VectorSubtract( vec3_origin, normal, (p+1)->normal );
	(p+1)->dist = -dist;

	nummapplanes += 2;

	// always put axial planes facing positive first
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
			VectorClear( normal );
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
void SnapPlane( vec3_t normal, float *dist )
{
	SnapVector( normal );

	if( fabs( *dist - floor( *dist + 0.5 )) < DIST_EPSILON )
		*dist = floor( *dist + 0.5 );
}

/*
=============
FindFloatPlane

=============
*/
int FindFloatPlane( vec3_t normal, float dist )
{
	int	i, h;
	uint	hash;
	plane_t	*p;

	SnapPlane( normal, &dist );
	hash = (PLANE_HASHES - 1) & (int)fabs( dist );

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
PlaneFromPoints
================
*/
int PlaneFromPoints( vec_t *p0, vec_t *p1, vec_t *p2 )
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

void PrintContents( int cnt )
{
	Msg("Contents:" );

	if( cnt & CONTENTS_SOLID ) Msg( "solid " );
	if( cnt & CONTENTS_WINDOW) Msg( "window " );
	if( cnt & CONTENTS_AUX ) Msg( "aux " );
	if( cnt & CONTENTS_LAVA) Msg( "lava " );
	if( cnt & CONTENTS_SLIME) Msg( "slime " );
	if( cnt & CONTENTS_WATER) Msg( "water " );
	if( cnt & CONTENTS_SKY) Msg( "sky " );

	if( cnt & CONTENTS_MIST) Msg( "mist " );
	if( cnt & CONTENTS_FOG) Msg( "fog " );
	if( cnt & CONTENTS_AREAPORTAL) Msg( "areaportal " );
	if( cnt & CONTENTS_PLAYERCLIP) Msg( "playerclip " );
	if( cnt & CONTENTS_MONSTERCLIP) Msg(" monsterclip " );
	if( cnt & CONTENTS_CLIP) Msg( "clip " );
	if( cnt & CONTENTS_ORIGIN) Msg(" origin" );
	if( cnt & CONTENTS_BODY) Sys_Error("\nCONTENTS_BODY detected\n" );
	if( cnt & CONTENTS_CORPSE) Sys_Error("\nCONTENTS_CORPSE detected\n" );
	if( cnt & CONTENTS_DETAIL) Msg(" detail " );
	if( cnt & CONTENTS_TRANSLUCENT) Msg( "translucent " );
	if( cnt & CONTENTS_LADDER) Msg( "ladder " );
	if( cnt & CONTENTS_TRIGGER) Msg( "trigger " );
	Msg( "\n" );
}

//====================================================================
/*
===========
BrushContents
===========
*/
int BrushContents( mapbrush_t *b )
{
	int	contents;
	int	i, trans;
	side_t	*s;

	s = &b->original_sides[0];
	b->shadernum = texinfo[s->texinfo].shadernum;
	trans = dshaders[texinfo[s->texinfo].shadernum].surfaceFlags;
	contents = s->contents;

	for( i = 1; i < b->numsides; i++, s++ )
	{
		s = &b->original_sides[i];
		trans |= dshaders[texinfo[s->texinfo].shadernum].surfaceFlags;
		if( s->contents != contents && !( trans & SURF_NODRAW ))
		{
			// nodraw textures are ignored
			MsgDev( D_WARN, "Entity %i, Brush %i: mixed face contents\n", b->entitynum, b->brushnum );
			break;
		}
	}

	// if any side is translucent, mark the contents
	// and change solid to window
	if( trans & (SURF_TRANS|SURF_BLEND))
	{
		contents |= CONTENTS_TRANSLUCENT;
		if( contents & CONTENTS_SOLID )
		{
			contents &= ~CONTENTS_SOLID;
			contents |= CONTENTS_WINDOW;
		}
	}
	return contents;
}


//============================================================================

/*
=================
AddBrushBevels

Adds any additional planes necessary to allow the brush to be expanded
against axial bounding boxes
=================
*/
void AddBrushBevels( mapbrush_t *b )
{
	int		axis, dir;
	int		i, j, k, l, order;
	side_t		sidetemp;
	brush_texture_t	tdtemp;
	side_t		*s, *s2;
	vec3_t		normal;
	float		dist;
	winding_t		*w, *w2;
	vec3_t		vec, vec2;
	float		d;

	// add the axial planes
	for( axis = order = 0; axis < 3; axis++ )
	{
		for( dir = -1; dir <= 1; dir += 2, order++ )
		{
			// see if the plane is allready present
			for( i = 0, s = b->original_sides; i < b->numsides; i++, s++ )
			{
				if( mapplanes[s->planenum].normal[axis] == dir )
					break;
			}

			if( i == b->numsides )
			{	
				// add a new side
				if( nummapbrushsides == MAX_MAP_BRUSHSIDES )
					Sys_Error( "MAX_MAP_BRUSHSIDES limit exceeded\n" );
				nummapbrushsides++;
				b->numsides++;
				VectorClear( normal );
				normal[axis] = dir;
				if( dir == 1 ) dist = b->maxs[axis];
				else dist = -b->mins[axis];
				s->planenum = FindFloatPlane( normal, dist );
				s->texinfo = b->original_sides[0].texinfo;
				s->contents = b->original_sides[0].contents;
				s->bevel = true;
				c_boxbevels++;
			}

			// if the plane is not in it canonical order, swap it
			if( i != order )
			{
				sidetemp = b->original_sides[order];
				b->original_sides[order] = b->original_sides[i];
				b->original_sides[i] = sidetemp;

				j = b->original_sides - brushsides;
				tdtemp = side_brushtextures[j+order];
				side_brushtextures[j+order] = side_brushtextures[j+i];
				side_brushtextures[j+i] = tdtemp;
			}
		}
	}

	// add the edge bevels
	if( b->numsides == 6 ) return; // pure axial

	// test the non-axial plane edges
	for( i = 6; i < b->numsides; i++ )
	{
		s = b->original_sides + i;
		w = s->winding;
		if( !w ) continue;

		for( j = 0; j < w->numpoints; j++ )
		{
			k = (j+1) % w->numpoints;
			VectorSubtract( w->p[j], w->p[k], vec );
			if( VectorNormalizeLength( vec ) < 0.5f ) continue;
			SnapVector( vec );
			for( k = 0; k < 3; k++ )
				if( vec[k] == -1 || vec[k] == 1 )
					break;	// axial
			if( k != 3 ) continue;	// only test non-axial edges

			// try the six possible slanted axials from this edge
			for( axis = 0; axis < 3; axis++ )
			{
				for( dir = -1; dir <= 1; dir += 2 )
				{
					// construct a plane
					VectorClear( vec2 );
					vec2[axis] = dir;
					CrossProduct( vec, vec2, normal );
					if( VectorNormalizeLength( normal ) < 0.5f ) continue;
					dist = DotProduct( w->p[j], normal );

					// if all the points on all the sides are
					// behind this plane, it is a proper edge bevel
					for( k = 0; k < b->numsides; k++ )
					{
						// if this plane has allready been used, skip it
						if( PlaneEqual( &mapplanes[b->original_sides[k].planenum], normal, dist ))
							break;

						w2 = b->original_sides[k].winding;
						if( !w2 ) continue;

						for( l = 0; l < w2->numpoints; l++ )
						{
							d = DotProduct( w2->p[l], normal ) - dist;
							if( d > 0.1f ) break; // point in front
						}
						if( l != w2->numpoints ) break;
					}

					if( k != b->numsides ) continue; // wasn't part of the outer hull
					// add this plane
					if( nummapbrushsides == MAX_MAP_BRUSHSIDES )
						Sys_Error( "MAX_MAP_BRUSHSIDES limit exceeded\n" );
					nummapbrushsides++;
					s2 = &b->original_sides[b->numsides];
					s2->planenum = FindFloatPlane( normal, dist );
					s2->texinfo = b->original_sides[0].texinfo;
					s2->contents = b->original_sides[0].contents;
					s2->bevel = true;
					c_edgebevels++;
					b->numsides++;
				}
			}
		}
	}
}


/*
================
MakeBrushWindings

makes basewindigs for sides and mins / maxs for the brush
================
*/
bool MakeBrushWindings( mapbrush_t *ob )
{
	int		i, j;
	winding_t		*w;
	side_t		*side;
	plane_t		*plane;

	ClearBounds( ob->mins, ob->maxs );

	for( i = 0; i < ob->numsides; i++ )
	{
		plane = &mapplanes[ob->original_sides[i].planenum];
		w = BaseWindingForPlane( plane->normal, plane->dist );
		for( j = 0; j < ob->numsides && w; j++ )
		{
			if( i == j ) continue;
			if( ob->original_sides[j].bevel ) continue;
			plane = &mapplanes[ob->original_sides[j].planenum^1];
			ChopWindingInPlace( &w, plane->normal, plane->dist, 0 );
		}

		side = &ob->original_sides[i];
		side->winding = w;
		if( w )
		{
			side->visible = true;
			for( j = 0; j < w->numpoints; j++ )
				AddPointToBounds( w->p[j], ob->mins, ob->maxs );
		}
	}

	for( i = 0; i < 3; i++ )
	{
		if( ob->mins[i] < MIN_WORLD_COORD || ob->maxs[i] > MAX_WORLD_COORD )
			return false;
		if( ob->mins[i] >= ob->maxs[i] )
			return false;
	}
	return true;
}


/*
=================
ParseBrush
=================
*/
void ParseBrush( bsp_entity_t *mapent )
{
	mapbrush_t	*b;
	int		i,j, k;
	int		mt;
	side_t		*side, *s2;
	int		planenum;
	vec_t		planepts[3][3];	// quark used float coords
	token_t		token;
	bsp_shader_t	*si;
	brush_texture_t	td;

	if( nummapbrushes == MAX_MAP_BRUSHES ) Sys_Break( "MAX_MAP_BRUSHES limit exceeded\n");
	if( g_brushtype == BRUSH_RADIANT ) Com_CheckToken( mapfile, "{" );

	b = &mapbrushes[nummapbrushes];
	b->original_sides = &brushsides[nummapbrushsides];
	b->entitynum = num_entities-1;
	b->brushnum = nummapbrushes - mapent->firstbrush;

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

		if( nummapbrushsides == MAX_MAP_BRUSHSIDES )
			Sys_Break( "MAX_MAP_BRUSHSIDES limit exceeded\n" );
		Com_SaveToken( mapfile, &token );
		side = &brushsides[nummapbrushsides];

		// read the three point plane definition
		Com_Parse1DMatrix( mapfile, 3, planepts[0] );
		Com_Parse1DMatrix( mapfile, 3, planepts[1] );
		Com_Parse1DMatrix( mapfile, 3, planepts[2] );

		if( g_brushtype == BRUSH_RADIANT )
			Com_Parse2DMatrix( mapfile, 2, 3, (float *)td.vects.radiant.matrix );

		// read the texturedef
		Com_ReadToken( mapfile, SC_ALLOW_PATHNAMES|SC_PARSE_GENERIC, &token );
		com.strncpy( td.name, token.string, sizeof( td.name ));

		if( g_brushtype == BRUSH_WORLDCRAFT_22 ) // Worldcraft 2.2+
                    {
			// texture U axis
			Com_ReadToken( mapfile, 0, &token );
			if( com.strcmp( token.string, "[")) Sys_Break( "missing '[' in texturedef (U)\n" );
			Com_ReadFloat( mapfile, false, &td.vects.hammer.UAxis[0] );
			Com_ReadFloat( mapfile, false, &td.vects.hammer.UAxis[1] );
			Com_ReadFloat( mapfile, false, &td.vects.hammer.UAxis[2] );
			Com_ReadFloat( mapfile, false, &td.vects.hammer.shift[0] );
			Com_ReadToken( mapfile, 0, &token );
			if( com.strcmp( token.string, "]")) Sys_Break( "missing ']' in texturedef (U)\n" );

			// texture V axis
			Com_ReadToken( mapfile, 0, &token );
			if( com.strcmp( token.string, "[")) Sys_Break( "missing '[' in texturedef (V)\n" );
			Com_ReadFloat( mapfile, false, &td.vects.hammer.VAxis[0] );
			Com_ReadFloat( mapfile, false, &td.vects.hammer.VAxis[1] );
			Com_ReadFloat( mapfile, false, &td.vects.hammer.VAxis[2] );
			Com_ReadFloat( mapfile, false, &td.vects.hammer.shift[1] );
			Com_ReadToken( mapfile, 0, &token );
			if( com.strcmp( token.string, "]")) Sys_Break( "missing ']' in texturedef (V)\n");

			// texture rotation is implicit in U/V axes.
			Com_ReadToken( mapfile, 0, &token );
			td.vects.hammer.rotate = 0;

			// texure scale
			// texure scale
			Com_ReadFloat( mapfile, false, &td.vects.hammer.scale[0] );
			Com_ReadFloat( mapfile, false, &td.vects.hammer.scale[1] );
                    }
		else if( g_brushtype == BRUSH_WORLDCRAFT_21 || g_brushtype == BRUSH_QUARK )
		{
			// worldcraft 2.1-, old Radiant, QuArK
			Com_ReadFloat( mapfile, false, &td.vects.hammer.shift[0] );
			Com_ReadFloat( mapfile, false, &td.vects.hammer.shift[1] );
			Com_ReadFloat( mapfile, false, &td.vects.hammer.rotate );
			Com_ReadFloat( mapfile, false, &td.vects.hammer.scale[0] );
			Com_ReadFloat( mapfile, SC_COMMENT_SEMICOLON, &td.vects.hammer.scale[1] );
                    }

		// hidden q2/q3 legacy, but can be used
                    if( g_brushtype != BRUSH_QUARK && Com_ReadToken( mapfile, SC_COMMENT_SEMICOLON, &token ))
		{
			// overwrite shader values directly from .map file
			Com_SaveToken( mapfile, &token );
			Com_ReadLong( mapfile, false, &td.contents );
			Com_ReadLong( mapfile, false, &td.flags );
			Com_ReadLong( mapfile, false, &td.value );
		}

		if( mapfile->TXcommand == '1' || mapfile->TXcommand == '2' )
		{
			// we are QuArK mode and need to translate some numbers to align textures its way
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
				MsgDev( D_WARN, "Degenerate QuArK-style brush texture : Entity %i, Brush %i\n", b->entitynum, b->brushnum );
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
				td.vects.quark.vecs[0][j] = aa * TexPt[0][j] + bb * TexPt[1][j];
				td.vects.quark.vecs[1][j] = -(bb * TexPt[0][j] + dd * TexPt[1][j]);
			}
			td.vects.quark.vecs[0][3] = -DotProduct( td.vects.quark.vecs[0], planepts[0] );
			td.vects.quark.vecs[1][3] = -DotProduct( td.vects.quark.vecs[1], planepts[0] );
		}

		td.brush_type = g_brushtype;	// member map type
		td.flags = td.contents = td.value = 0;	// reset all values before setting
		side->contents = side->surf = 0;

		// get size from miptex info
		mt = FindMiptex( td.name );
		td.size[0] = dshaders[mt].size[0];
		td.size[1] = dshaders[mt].size[1];

		// get flags and contents from shader
		// FIXME: rewote this relationship
		si = FindShader( td.name );
		if( si )
		{
			side->contents = td.contents = si->contents;
			side->surf = td.flags = si->surfaceFlags;
			td.value = si->intensity;
		}
		else
		{
			side->contents = td.contents;
			side->surf = td.flags;
		}

		// translucent objects are automatically classified as detail
		if( side->surf & ( SURF_TRANS|SURF_BLEND ))
		{
			side->contents |= CONTENTS_DETAIL;
			td.contents |= CONTENTS_DETAIL;
		}
		if( side->contents & CONTENTS_CLIP )
		{
			side->contents |= CONTENTS_DETAIL;
			td.contents |= CONTENTS_DETAIL;
		}
		if(!(side->contents & ((LAST_VISIBLE_CONTENTS - 1)|CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP|CONTENTS_MIST)))
		{
			side->contents |= CONTENTS_SOLID;
			td.contents |= CONTENTS_SOLID;
		}
		// hints and skips are never detail, and have no content
		if( side->surf & ( SURF_HINT|SURF_SKIP ))
		{
			side->contents = td.contents = 0;
			side->surf &= ~CONTENTS_DETAIL;
			td.flags &= ~CONTENTS_DETAIL;
		}

		// Msg( "flags %p, value %d, contents %p\n", td.flags, td.value, side->contents );

		// find the plane number
		planenum = PlaneFromPoints( planepts[0], planepts[1], planepts[2] );
		if( planenum == -1 )
		{
			Msg( "Entity %i, Brush %i: plane with no normal\n", b->entitynum, b->brushnum );
			continue;
		}

		if( g_brushtype == BRUSH_RADIANT )
		{
			float	m[2][3], vecs[2][4];
			float	a, ac, as, bc, bs;
			plane_t	*plane = mapplanes + planenum;
		
			Mem_Copy( m, td.vects.radiant.matrix, sizeof (m )); // save outside 

			// calculate proper texture vectors from GTKRadiant/Doom3 brushprimitives matrix
			a = -com.atan2( plane->normal[2], com.sqrt( plane->normal[0] * plane->normal[0] + plane->normal[1] * plane->normal[1] ));
			ac = com.cos( a );
			as = com.sin( a );
			a = com.atan2( plane->normal[1], plane->normal[0] );
			bc = com.cos( a );
			bs = com.sin( a );

			vecs[0][0] = -bs;
			vecs[0][1] = bc;
			vecs[0][2] = 0.0f;
			vecs[0][3] = 0;	// FIXME: set to 1.0f ?
			vecs[1][0] = -as*bc;
			vecs[1][1] = -as*bs;
			vecs[1][2] = -ac;
			vecs[1][3] = 0;	// FIXME: set to 1.0f ?

			td.vects.quark.vecs[0][0] = m[0][0] * vecs[0][0] + m[0][1] * vecs[1][0];
			td.vects.quark.vecs[0][1] = m[0][0] * vecs[0][1] + m[0][1] * vecs[1][1];
			td.vects.quark.vecs[0][2] = m[0][0] * vecs[0][2] + m[0][1] * vecs[1][2];
			td.vects.quark.vecs[0][3] = m[0][0] * vecs[0][3] + m[0][1] * vecs[1][3] + m[0][2];
			td.vects.quark.vecs[1][0] = m[1][0] * vecs[0][0] + m[1][1] * vecs[1][0];
			td.vects.quark.vecs[1][1] = m[1][0] * vecs[0][1] + m[1][1] * vecs[1][1];
			td.vects.quark.vecs[1][2] = m[1][0] * vecs[0][2] + m[1][1] * vecs[1][2];
			td.vects.quark.vecs[1][3] = m[1][0] * vecs[0][3] + m[1][1] * vecs[1][3] + m[1][2];
		}

		// see if the plane has been used already
		for( k = 0; k < b->numsides; k++ )
		{
			s2 = b->original_sides + k;
			if( s2->planenum == planenum )
			{
				Msg( "Entity %i, Brush %i: duplicate plane\n", b->entitynum, b->brushnum );
				break;
			}
			if( s2->planenum == ( planenum^1 ))
			{
				Msg( "Entity %i, Brush %i: mirrored plane\n", b->entitynum, b->brushnum );
				break;
			}
		}
		if( k != b->numsides ) continue; // duplicated

		// keep this side
		side = b->original_sides + b->numsides;
		side->planenum = planenum;
		side->texinfo = TexinfoForBrushTexture( &mapplanes[planenum], &td, vec3_origin );
		// save the td off in case there is an origin brush and we
		// have to recalculate the texinfo
		side_brushtextures[nummapbrushsides] = td;

		nummapbrushsides++;
		b->numsides++;
	}

	if( g_brushtype == BRUSH_RADIANT )
	{
		Com_SaveToken( mapfile, &token );
		Com_CheckToken( mapfile, "}" );
		Com_CheckToken( mapfile, "}" );
	}

	// get the content for the entire brush
	b->contents = BrushContents( b );

	// create windings for sides and bounds for brush
	if(!MakeBrushWindings( b ))
	{
		// brush outside of the world, remove
		b->numsides = 0;
		return;
	}

	// brushes that will not be visible at all will never be
	// used as bsp splitters
	if( b->contents & CONTENTS_CLIP )
	{
		c_clipbrushes++;
		for( i = 0; i < b->numsides; i++ )
			b->original_sides[i].texinfo = TEXINFO_NODE;
	}

	// origin brushes are removed, but they set
	// the rotation origin for the rest of the brushes
	// in the entity.  After the entire entity is parsed,
	// the planenums and texinfos will be adjusted for
	// the origin brush
	if( b->contents & CONTENTS_ORIGIN )
	{
		vec3_t	size, movedir, origin;
		char	string[32];

		if( num_entities == 1 )
		{
			// g-cont. rotating world it's a interesting idea, hmm.....
			MsgDev( D_WARN, "Entity %i, Brush %i: origin brushes not allowed in world", b->entitynum, b->brushnum );
			return;
		}

		// calcualte movedir (Xash 0.4 style)
		VectorAverage( b->mins, b->maxs, origin );
		VectorSubtract( b->maxs, b->mins, size );

		if( size[2] > size[0] && size[2] > size[1] )
            		VectorSet( movedir, 0, 1, 0 );	// x-rotate
		else if( size[1] > size[2] && size[1] > size[0] )
            		VectorSet( movedir, 1, 0, 0 );	// y-rotate
		else if( size[0] > size[2] && size[0] > size[1] )
			VectorSet( movedir, 0, 0, 1 );	// z-rotate
		else VectorClear( movedir ); // custom movedir

		if(!VectorIsNull( movedir ))
		{
			com.sprintf( string, "%i %i %i", (int)movedir[0], (int)movedir[1], (int)movedir[2] );
			SetKeyValue( &entities[b->entitynum], "movedir", string );
		}
		if(!VectorIsNull( origin ))
		{
			com.sprintf( string, "%i %i %i", (int)origin[0], (int)origin[1], (int)origin[2] );
			SetKeyValue( &entities[b->entitynum], "origin", string );
			VectorCopy( origin, entities[num_entities - 1].origin );
		}

		// don't keep this brush
		b->numsides = 0;
		return;
	}

	AddBrushBevels( b );

	nummapbrushes++;
	mapent->numbrushes++;		
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
	int			newbrushes;
	int			worldbrushes;
	mapbrush_t	*temp;
	int			i;

	// this is pretty gross, because the brushes are expected to be
	// in linear order for each entity

	newbrushes = mapent->numbrushes;
	worldbrushes = entities[0].numbrushes;

	temp = Malloc(newbrushes*sizeof(mapbrush_t));
	Mem_Copy( temp, mapbrushes + mapent->firstbrush, newbrushes * sizeof( mapbrush_t ));

	// make space to move the brushes (overlapped copy)
	memmove( mapbrushes + worldbrushes + newbrushes, mapbrushes + worldbrushes,
		sizeof( mapbrush_t ) * ( nummapbrushes - worldbrushes - newbrushes ));

	// copy the new brushes down
	Mem_Copy( mapbrushes + worldbrushes, temp, sizeof( mapbrush_t ) * newbrushes );

	// fix up indexes
	entities[0].numbrushes += newbrushes;
	for( i = 1; i < num_entities; i++ )
		entities[i].firstbrush += newbrushes;
	Mem_Free( temp );

	mapent->numbrushes = 0;
}

/*
================
ParseMapEntity
================
*/
bool ParseMapEntity( void )
{
	bsp_entity_t	*mapent;
	token_t		token;
	epair_t		*e;
	side_t		*s;
	int		i, j;
	int		startbrush, startsides;
	float		newdist;
	mapbrush_t	*b;

	if( !Com_ReadToken( mapfile, SC_ALLOW_NEWLINES|SC_COMMENT_SEMICOLON, &token ))
		return false; // end of .map file
	if( com.stricmp( token.string, "{" )) Sys_Break( "ParseEntity: found %s instead {\n", token.string );
	if( num_entities == MAX_MAP_ENTITIES ) Sys_Break( "MAX_MAP_ENTITIES limit exceeded\n");

	startbrush = nummapbrushes;
	startsides = nummapbrushsides;

	mapent = &entities[num_entities];
	num_entities++;
	Mem_Set( mapent, 0, sizeof( *mapent ));
	mapent->firstbrush = nummapbrushes;
	mapent->numbrushes = 0;

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
				Com_SkipBracedSection( mapfile, 0 );
			}
			else if( !com.stricmp( token.string, "terrainDef" ))
			{
				g_brushtype = BRUSH_RADIANT;
				Com_SkipBracedSection( mapfile, 0 );
			}
			else if( !com.stricmp( token.string, "brushDef" ))
			{
				g_brushtype = BRUSH_RADIANT;
				ParseBrush( mapent );	// parse brush primitive
			}
			else
			{
				// predict state
				if( g_brushtype == BRUSH_UNKNOWN )
					g_brushtype = BRUSH_WORLDCRAFT_21;
				// QuArK or WorldCraft map
				Com_SaveToken( mapfile, &token );
				ParseBrush( mapent );
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
	if( mapent->origin[0] || mapent->origin[1] || mapent->origin[2] )
	{
		for( i = 0; i < mapent->numbrushes; i++ )
		{
			b = &mapbrushes[mapent->firstbrush + i];
			for( j = 0; j < b->numsides; j++ )
			{
				s = &b->original_sides[j];
				newdist = mapplanes[s->planenum].dist - DotProduct (mapplanes[s->planenum].normal, mapent->origin);
				s->planenum = FindFloatPlane( mapplanes[s->planenum].normal, newdist );
				s->texinfo = TexinfoForBrushTexture( &mapplanes[s->planenum], &side_brushtextures[s-brushsides], mapent->origin );
			}
			MakeBrushWindings( b );
		}
	}

	// group entities are just for editor convenience
	// toss all brushes into the world entity
	if( !com.strcmp( "func_group", ValueForKey( mapent, "classname" )))
	{
		MoveBrushesToWorld( mapent );
		mapent->numbrushes = 0;
		return true;
	}

	// areaportal entities move their brushes, but don't eliminate
	// the entity
	if (!strcmp ("func_areaportal", ValueForKey (mapent, "classname")))
	{
		char	str[128];

		if (mapent->numbrushes != 1)
			Sys_Error ("Entity %i: func_areaportal can only be a single brush", num_entities-1);

		b = &mapbrushes[nummapbrushes-1];
		b->contents = CONTENTS_AREAPORTAL;
		c_areaportals++;
		mapent->areaportalnum = c_areaportals;
		// set the portal number as "style"
		com.sprintf (str, "%i", c_areaportals);
		SetKeyValue (mapent, "style", str);
		MoveBrushesToWorld (mapent);
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
	int	i;

	g_brushtype = BRUSH_UNKNOWN;	
	nummapbrushsides = 0;
	num_entities = 0;

	mapfile = Com_OpenScript( va( "maps/%s.map", gs_filename ), NULL, 0 );
	if( !mapfile ) Sys_Break( "can't loading map file %s.map\n", gs_filename );	
	
	while(ParseMapEntity( ));

	Com_CloseScript( mapfile );
	ClearBounds( map_mins, map_maxs );
	for( i = 0; i < entities[0].numbrushes; i++ )
	{
		AddPointToBounds( mapbrushes[i].mins, map_mins, map_maxs );
		AddPointToBounds( mapbrushes[i].maxs, map_mins, map_maxs );
	}
	VectorSubtract( map_maxs, map_mins, map_size );
	MsgDev( D_INFO, "%5i brushes\n", nummapbrushes );
	MsgDev( D_INFO, "%5i clipbrushes\n", c_clipbrushes );
	MsgDev( D_INFO, "%5i total sides\n", nummapbrushsides );
	MsgDev( D_INFO, "%5i entities\n", num_entities );
	MsgDev( D_INFO, "%5i planes\n", nummapplanes );
	MsgDev( D_INFO, "%5i areaportals\n", c_areaportals );
	MsgDev( D_INFO, "world size %5.0f %5.0f %5.0f\n", map_size[0], map_size[1], map_size[2] );
}