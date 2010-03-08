/* -------------------------------------------------------------------------------

Copyright (C) 1999-2007 id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

----------------------------------------------------------------------------------

This code has been altered significantly from its original form, to support
several games based on the Quake III Arena engine, in the form of "Q3Map2."

------------------------------------------------------------------------------- */

#define MAP_C

#include "q3map2.h"
#include "trace_def.h"

#define VALVE_FORMAT	220
#define GEARBOX_FORMAT	510
#define PLANE_HASHES	8192
#define NORMAL_EPSILON	0.00001
#define DIST_EPSILON	0.01
#define MAPTYPE()		Msg( "map type: %s\n", g_sMapType[g_bBrushPrimit] )

#define GEARBOX_DETAIL	8
#define GEARBOX_NODRAW	32
#define GEARBOX_NOIMPACTS	131072
#define GEARBOX_NODECALS	262144

static const char *g_sMapType[BRUSH_COUNT] =
{
"unknown",
"Worldcraft 2.1",
"Valve Hammer 3.4",
"GearCraft 4.0",
"Radiant",
"QuArK"
};

plane_t	*planehash[ PLANE_HASHES ];
int	c_boxbevels;
int	c_edgebevels;
int	c_areaportals;
int	c_detail;
int	c_structural;

/*
=================
PlaneEqual

replaced with variable epsilon for djbob
=================
*/
bool PlaneEqual( plane_t *p, vec3_t normal, vec_t dist )
{
	float	ne, de;

	ne = normalEpsilon;
	de = distanceEpsilon;
	
	if( fabs( p->dist - dist ) <= de && fabs( p->normal[0] - normal[0] ) <= ne &&
		fabs( p->normal[1] - normal[1] ) <= ne && fabs( p->normal[2] - normal[2] ) <= ne )
		return true;
	return false;
}

/*
=================
MapPlaneTypeForNormal
=================
*/
int MapPlaneTypeForNormal( const vec3_t normal )
{
	if( normal[0] == 1.0 || normal[0] == -1.0 )
		return PLANE_X;
	if( normal[1] == 1.0 || normal[1] == -1.0 )
		return PLANE_Y;
	if( normal[2] == 1.0 || normal[2] == -1.0 )
		return PLANE_Z;
	return PLANE_NONAXIAL;
}

/*
=================
AddPlaneToHash
=================
*/
void AddPlaneToHash( plane_t *p )
{
	int	hash;

	
	hash = (PLANE_HASHES - 1) & (int) fabs( p->dist );
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

	if( VectorLength( normal ) < 0.5f )
	{
		MsgDev( D_NOTE, "CreateNewFloatPlane: bad normal\n");
		return -1;
	}

	// create a new plane
	if( nummapplanes + 2 > MAX_MAP_PLANES ) Sys_Error( "MAX_MAP_PLANES limit exceeded\n" );

	p = &mapplanes[nummapplanes];
	VectorCopy( normal, p->normal );
	p->dist = dist;
	p->type = (p+1)->type = MapPlaneTypeForNormal( p->normal );

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
=================
SnapNormal

snaps a near-axial normal vector
=================
*/
void SnapNormal( vec3_t normal )
{
	int	i;

	for( i = 0; i < 3; i++ )
	{
		if( fabs( normal[i] - 1 ) < normalEpsilon )
		{
			VectorClear( normal );
			normal[i] = 1;
			break;
		}
		if( fabs( normal[i] - -1 ) < normalEpsilon )
		{
			VectorClear( normal );
			normal[i] = -1;
			break;
		}
	}
}


/*
=================
SnapPlane

snaps a plane to normal/distance epsilons
=================
*/
void SnapPlane( vec3_t normal, vec_t *dist )
{
	SnapNormal( normal );

	if( fabs( *dist - Q_rint( *dist )) < distanceEpsilon )
		*dist = Q_rint( *dist );
}



/*
=================
FindFloatPlane

changed to allow a number of test points to be supplied that
must be within an epsilon distance of the plane
=================
*/
int FindFloatPlane( vec3_t normal, vec_t dist, int numPoints, vec3_t *points )
{
	int		i, j, hash, h;
	plane_t		*p;
	vec_t		d;
	
	
	SnapPlane( normal, &dist );
	hash = (PLANE_HASHES - 1) & (int)fabs( dist );
	
	for( i = -1; i <= 1; i++ )
	{
		h = (hash + i) & (PLANE_HASHES - 1);
		for( p = planehash[ h ]; p != NULL; p = p->hash_chain )
		{
			if( !PlaneEqual( p, normal, dist ))
				continue;
			
			// test supplied points against this plane
			for( j = 0; j < numPoints; j++ )
			{
				d = DotProduct( points[j], normal ) - dist;
				if( fabs( d ) > distanceEpsilon )
					break;
			}
			
			if( j >= numPoints )
				return p - mapplanes;
		}
	}
	
	// none found, so create a new one
	return CreateNewFloatPlane( normal, dist );
}

/*
=====================
PlaneFromPoints

Returns false if the triangle is degenrate.
The normal will point out of the clock for clockwise ordered points
=====================
*/
bool PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c )
{
	vec3_t	d1, d2;

	VectorSubtract( b, a, d1 );
	VectorSubtract( c, a, d2 );
	CrossProduct( d2, d1, plane );
	if( VectorNormalizeLength( plane ) == 0.0f )
		return false;
	plane[3] = DotProduct( a, plane );

	return true;
}

/*
=================
MapPlaneFromPoints

takes 3 points and finds the plane they lie in
=================
*/
int MapPlaneFromPoints( vec3_t *p )
{
	vec3_t	t1, t2, normal;
	vec_t	dist;
	
	VectorSubtract( p[0], p[1], t1 );
	VectorSubtract( p[2], p[1], t2 );
	CrossProduct( t1, t2, normal );
	VectorNormalize( normal );
	dist = DotProduct( p[0], normal );
	
	return FindFloatPlane( normal, dist, 3, p );
}

void MapCntString( int cnt, char *contents )
{
	contents[0] = '\0';

	if( cnt & BASECONT_SOLID ) com.strcat( contents, "solid " );
	if( cnt & BASECONT_LAVA ) com.strcat( contents, "lava " );
	if( cnt & BASECONT_SLIME ) com.strcat( contents, "slime " );
	if( cnt & BASECONT_WATER ) com.strcat( contents, "water " );
	if( cnt & BASECONT_SKY ) com.strcat( contents, "sky " );

	if( cnt & BASECONT_FOG ) com.strcat( contents, "fog " );
	if( cnt & BASECONT_AREAPORTAL ) com.strcat( contents, "areaportal " );
	if( cnt & BASECONT_PLAYERCLIP ) com.strcat( contents, "playerclip " );
	if( cnt & BASECONT_MONSTERCLIP ) com.strcat( contents, " monsterclip " );
	if( cnt & BASECONT_CLIP ) com.strcat( contents, "clip " );
	if( cnt & BASECONT_ORIGIN ) com.strcat( contents, " origin" );
	if( cnt & BASECONT_BODY ) Sys_Error("\nBASECONT_BODY detected\n" );
	if( cnt & BASECONT_CORPSE ) Sys_Error("\nBASECONT_CORPSE detected\n" );
	if( cnt & BASECONT_DETAIL ) com.strcat( contents, " detail " );
	if( cnt & BASECONT_TRANSLUCENT ) com.strcat( contents, "translucent " );
	if( cnt & BASECONT_TRIGGER ) com.strcat( contents, "trigger " );
}

/*
=================
SetBrushContents

the content flags and compile flags on all sides of a brush should be the same
=================
*/
void SetBrushContents( brush_t *b )
{
	int	contentFlags, compileFlags;
	side_t	*s;	
	
	s = &b->sides[0];
	contentFlags = s->contentFlags;
	compileFlags = s->compileFlags;
	b->contentShader = s->shaderInfo;

	/* check for detail & structural */
	if( (compileFlags & C_DETAIL) && (compileFlags & C_STRUCTURAL) )
	{
		MsgDev( D_WARN, "Entity %i, Brush %i: mixed BASECONT_DETAIL and BASECONT_STRUCTURAL\n", mapEnt->mapEntityNum, entitySourceBrushes );
		compileFlags &= ~C_DETAIL;
	}
	
	// the fulldetail flag will cause detail brushes to be treated like normal brushes
	if( fulldetail ) compileFlags &= ~C_DETAIL;
	
	// all translucent brushes that aren't specifically made structural will be detail
	if(( compileFlags & C_TRANSLUCENT ) && !( compileFlags & C_STRUCTURAL ))
		compileFlags |= C_DETAIL;
	
	if( compileFlags & C_DETAIL )
	{
		c_detail++;
		b->detail = true;
	}
	else
	{
		c_structural++;
		b->detail = false;
	}
	
	if( compileFlags & C_TRANSLUCENT )
		b->opaque = false;
	else b->opaque = true;
	
	if( compileFlags & C_AREAPORTAL )
		c_areaportals++;
	
	b->contentFlags = contentFlags;
	b->compileFlags = compileFlags;
}

/*
=================
AddBrushBevels

adds any additional planes necessary to allow the brush being
built to be expanded against axial bounding boxes
2003-01-20: added mr.Elusive fixes
=================
*/
void AddBrushBevels( void )
{
	int		axis, dir;
	int		i, j, k, l, order = 0;
	side_t		sidetemp;
	side_t		*s, *s2;
	winding_t		*w, *w2;
	vec3_t		normal;
	float		dist;
	vec3_t		vec, vec2;
	float		d, minBack;

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
					Sys_Break( "Entity %i, Brush %i MAX_BUILD_SIDES\n", buildBrush->entityNum, buildBrush->brushNum );

				Mem_Set( s, 0, sizeof( *s ));
				buildBrush->numsides++;
				VectorClear (normal);
				normal[axis] = dir;

				if( dir == 1 )
				{
					// adding bevel plane snapping for fewer bsp planes
					if( bevelSnap > 0 )
						dist = floor( buildBrush->maxs[axis] / bevelSnap ) * bevelSnap;
					else
						dist = buildBrush->maxs[axis];
				}
				else
				{
					// adding bevel plane snapping for fewer bsp planes
					if( bevelSnap > 0 )
						dist = -ceil( buildBrush->mins[axis] / bevelSnap ) * bevelSnap;
					else
						dist = -buildBrush->mins[axis];
				}

				s->planenum = FindFloatPlane( normal, dist, 0, NULL );
				s->contentFlags = buildBrush->sides[0].contentFlags;
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
	if( buildBrush->numsides == 6 )
		return; // pure axial

	// test the non-axial plane edges
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
			SnapNormal( vec );
			for( k = 0; k < 3; k++ )
			{
				if( vec[k] == -1.0f || vec[k] == 1.0f || (vec[k] == 0.0f && vec[(k+1)%3] == 0.0f))
					break;	// axial
			}
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

						minBack = 0.0f;
						for( l = 0; l < w2->numpoints; l++ )
						{
							d = DotProduct( w2->p[l], normal ) - dist;
							if( d > 0.1f ) break; // point in front
							if( d < minBack ) minBack = d;
						}
						// if some point was at the front
						if( l != w2->numpoints )
							break;

						// if no points at the back then the winding is on the bevel plane
						if( minBack > -0.1f )
							break;
					}

					if( k != buildBrush->numsides )
						continue;	// wasn't part of the outer hull
					
					// add this plane
					if( buildBrush->numsides == MAX_BUILD_SIDES )
						Sys_Break( "Entity %i, Brush %i MAX_BUILD_SIDES\n", buildBrush->entityNum, buildBrush->brushNum );

					s2 = &buildBrush->sides[buildBrush->numsides];
					buildBrush->numsides++;
					Mem_Set( s2, 0, sizeof( *s2 ) );

					s2->planenum = FindFloatPlane( normal, dist, 1, &w->p[j] );
					s2->contentFlags = buildBrush->sides[0].contentFlags;
					s2->bevel = true;
					c_edgebevels++;
				}
			}
		}
	}
}



/*
=================
FinishBrush

produces a final brush based on the buildBrush->sides array
and links it to the current entity
=================
*/
brush_t *FinishBrush( void )
{
	brush_t		*b;
	
	// create windings for sides and bounds for brush
	if( !CreateBrushWindings( buildBrush ))
		return NULL;

	// origin brushes are removed, but they set the rotation origin for the rest of the brushes in the entity.
	// after the entire entity is parsed, the planenums and texinfos will be adjusted for the origin brush
	if( buildBrush->compileFlags & C_ORIGIN )
	{
		char	string[32];
		vec3_t	size, movedir, origin;

		if( numEntities == 1 )
		{
			Msg( "Entity %i, Brush %i: origin brushes not allowed in world\n", mapEnt->mapEntityNum, entitySourceBrushes );
			return NULL;
		}
		
		// calcualte movedir (Xash 0.4 style)
		VectorAverage( buildBrush->mins, buildBrush->maxs, origin );
		VectorSubtract( buildBrush->maxs, buildBrush->mins, size );

		if( size[2] > size[0] && size[2] > size[1] )
            		VectorSet( movedir, 0.0f, 1.0f, 0.0f );	// x-rotate
		else if( size[1] > size[2] && size[1] > size[0] )
            		VectorSet( movedir, 1.0f, 0.0f, 0.0f );	// y-rotate
		else if( size[0] > size[2] && size[0] > size[1] )
			VectorSet( movedir, 0.0f, 0.0f, 1.0f );	// z-rotate
		else VectorClear( movedir ); // custom movedir

#if 0
		if( !VectorIsNull( movedir ))
		{
			com.snprintf( string, sizeof( string ), "%i %i %i", (int)movedir[0], (int)movedir[1], (int)movedir[2] );
			SetKeyValue( &entities[numEntities - 1], "movedir", string );
		}
#endif
		if(!VectorIsNull( origin ))
		{
			com.snprintf( string, sizeof( string ), "%i %i %i", (int)origin[0], (int)origin[1], (int)origin[2] );
			SetKeyValue( &entities[numEntities - 1], "origin", string );
			VectorCopy( origin, entities[numEntities - 1].origin );
                    }

		// don't keep this brush
		return NULL;
	}
	
	// determine if the brush is an area portal
	if( buildBrush->compileFlags & C_AREAPORTAL )
	{
		if( numEntities != 1 )
		{
			Msg( "Entity %i, Brush %i: areaportals only allowed in world\n", mapEnt->mapEntityNum, entitySourceBrushes );
			return NULL;
		}
	}
	
	AddBrushBevels();
	
	b = CopyBrush( buildBrush );
	
	b->entityNum = mapEnt->mapEntityNum;
	b->brushNum = entitySourceBrushes;
	
	b->original = b;
	
	// link opaque brushes to head of list, translucent brushes to end
	if( b->opaque || mapEnt->lastBrush == NULL )
	{
		b->next = mapEnt->brushes;
		mapEnt->brushes = b;
		if( mapEnt->lastBrush == NULL )
			mapEnt->lastBrush = b;
	}
	else
	{
		b->next = NULL;
		mapEnt->lastBrush->next = b;
		mapEnt->lastBrush = b;
	}
	
	// link colorMod volume brushes to the entity directly
	if( b->contentShader != NULL && b->contentShader->colorMod != NULL && b->contentShader->colorMod->type == CM_VOLUME )
	{
		b->nextColorModBrush = mapEnt->colorModBrushes;
		mapEnt->colorModBrushes = b;
	}
	return b;
}

/*
=================
TextureAxisFromPlane

determines best orthagonal axis to project a texture onto a wall
(must be identical in radiant!)
=================
*/
vec3_t baseaxis[18] =
{
{ 0, 0, 1}, {1,0,0}, {0,-1, 0},	// floor
{ 0, 0,-1}, {1,0,0}, {0,-1, 0},	// ceiling
{ 1, 0, 0}, {0,1,0}, {0, 0,-1},	// west wall
{-1, 0, 0}, {0,1,0}, {0, 0,-1},	// east wall
{ 0, 1, 0}, {1,0,0}, {0, 0,-1},	// south wall
{ 0,-1, 0}, {1,0,0}, {0, 0,-1} 	// north wall
};

void TextureAxisFromPlane( plane_t *pln, vec3_t xv, vec3_t yv )
{
	int	i, bestaxis = 0;
	float	dot, best = 0;
	
	for( i = 0; i < 6; i++ )
	{
		dot = DotProduct( pln->normal, baseaxis[i*3] );

		if( dot > best + 0.0001f )	// bug 637 fix, suggested by jmonroe
		{
			best = dot;
			bestaxis = i;
		}
	}
	
	VectorCopy( baseaxis[bestaxis*3+1], xv );
	VectorCopy( baseaxis[bestaxis*3+2], yv );
}

/*
=================
ParseRawBrush

parses the sides into buildBrush->sides[], nothing else.
no validation, back plane removal, etc.
=================
*/
static void ParseRawBrush( bool onlyLights )
{
	side_t		*side;
	float		planePoints[3][3];
	int		planenum;
	char 		name[MAX_SHADERPATH];
	char		shader[MAX_SHADERPATH];
	shaderInfo_t	*si;
	token_t		token;
	vects_t		vects;
	int		flags;
	
	buildBrush->numsides = 0;
	buildBrush->detail = false;
	
	if( g_bBrushPrimit == BRUSH_RADIANT )
		Com_CheckToken( mapfile, "{" );
	
	while( 1 )
	{
		if( !Com_ReadToken( mapfile, SC_ALLOW_NEWLINES|SC_COMMENT_SEMICOLON, &token ))
			break;
		if( !com.stricmp( token.string, "}" )) break;
		if( g_bBrushPrimit == BRUSH_RADIANT )
		{
			while( 1 )
			{
				if( com.strcmp( token.string, "(" ))
					Com_ReadToken( mapfile, 0, &token );
				else break;
				Com_ReadToken( mapfile, SC_ALLOW_NEWLINES, &token );
			}
		}
		Com_SaveToken( mapfile, &token );
		
		if( buildBrush->numsides >= MAX_BUILD_SIDES )
			Sys_Break( "Entity %i, Brush %i MAX_BUILD_SIDES\n", buildBrush->entityNum, buildBrush->brushNum );
		
		side = &buildBrush->sides[ buildBrush->numsides ];
		Mem_Set( side, 0, sizeof( *side ));
		buildBrush->numsides++;
		
		// read the three point plane definition
		Com_Parse1DMatrix( mapfile, 3, planePoints[0] );
		Com_Parse1DMatrix( mapfile, 3, planePoints[1] );
		Com_Parse1DMatrix( mapfile, 3, planePoints[2] );
		
		// read the texture matrix
		if( g_bBrushPrimit == BRUSH_RADIANT )
			Com_Parse2DMatrix( mapfile, 2, 3, (float *)side->texMat );
		
		// read the texturedef or shadername
		Com_ReadToken( mapfile, SC_ALLOW_PATHNAMES|SC_PARSE_GENERIC, &token );
		com.strncpy( name, token.string, sizeof( name ));
		
		if( g_bBrushPrimit == BRUSH_WORLDCRAFT_22 || g_bBrushPrimit == BRUSH_GEARCRAFT_40 ) // Worldcraft 2.2+
                    {
			// texture U axis
			Com_ReadToken( mapfile, 0, &token );
			if( com.strcmp( token.string, "[" )) Sys_Break( "missing '[' in texturedef (U)\n" );
			Com_ReadFloat( mapfile, false, &vects.hammer.UAxis[0] );
			Com_ReadFloat( mapfile, false, &vects.hammer.UAxis[1] );
			Com_ReadFloat( mapfile, false, &vects.hammer.UAxis[2] );
			Com_ReadFloat( mapfile, false, &vects.hammer.shift[0] );
			Com_ReadToken( mapfile, 0, &token );
			if( com.strcmp( token.string, "]" )) Sys_Break( "missing ']' in texturedef (U)\n" );

			// texture V axis
			Com_ReadToken( mapfile, 0, &token );
			if( com.strcmp( token.string, "[" )) Sys_Break( "missing '[' in texturedef (V)\n" );
			Com_ReadFloat( mapfile, false, &vects.hammer.VAxis[0] );
			Com_ReadFloat( mapfile, false, &vects.hammer.VAxis[1] );
			Com_ReadFloat( mapfile, false, &vects.hammer.VAxis[2] );
			Com_ReadFloat( mapfile, false, &vects.hammer.shift[1] );
			Com_ReadToken( mapfile, 0, &token );
			if( com.strcmp( token.string, "]" )) Sys_Break( "missing ']' in texturedef (V)\n");

			// texture rotation is implicit in U/V axes.
			Com_ReadToken( mapfile, 0, &token );
			vects.hammer.rotate = 0;

			// texure scale
			Com_ReadFloat( mapfile, false, &vects.hammer.scale[0] );
			Com_ReadFloat( mapfile, false, &vects.hammer.scale[1] );

			if( g_bBrushPrimit == BRUSH_GEARCRAFT_40 )
			{
				Com_ReadLong( mapfile, false, &flags );	// read gearcraft flags
				Com_SkipRestOfLine( mapfile );	// gearcraft lightmap scale and rotate

				if( flags & GEARBOX_DETAIL )
					side->compileFlags |= C_DETAIL;

			}
                    }
		else if( g_bBrushPrimit == BRUSH_WORLDCRAFT_21 || g_bBrushPrimit == BRUSH_QUARK )
		{
			// worldcraft 2.1-, old Radiant, QuArK
			Com_ReadFloat( mapfile, false, &vects.hammer.shift[0] );
			Com_ReadFloat( mapfile, false, &vects.hammer.shift[1] );
			Com_ReadFloat( mapfile, false, &vects.hammer.rotate );
			Com_ReadFloat( mapfile, false, &vects.hammer.scale[0] );
			Com_ReadFloat( mapfile, SC_COMMENT_SEMICOLON, &vects.hammer.scale[1] );
                    }

		// set default flags and values
		com.sprintf( shader, "textures/%s", name );
		if( onlyLights ) si = &shaderInfo[0];
		else si = ShaderInfoForShader( shader );

		side->shaderInfo = si;
		side->surfaceFlags = si->surfaceFlags;
		side->contentFlags = si->contentFlags;
		side->compileFlags = si->compileFlags;
		side->value = si->value;
		
		// bias texture shift for non-radiant sides
		if( g_bBrushPrimit != BRUSH_RADIANT && si->globalTexture == false )
		{
			vects.hammer.shift[0] -= (floor( vects.hammer.shift[0] / si->shaderWidth ) * si->shaderWidth);
			vects.hammer.shift[1] -= (floor( vects.hammer.shift[1] / si->shaderHeight ) * si->shaderHeight);
		}
		
		// historically, there are 3 integer values at the end of a brushside line in a .map file.
		// in quake 3, the only thing that mattered was the first of these three values, which
		// was previously the content flags. and only then did a single bit matter, the detail
		// bit. because every game has its own special flags for specifying detail, the
		// traditionally game-specified BASECONT_DETAIL flag was overridden for Q3Map 2.3.0
		// by C_DETAIL, defined in q3map2.h. the value is exactly as it was before, but
		// is stored in compileFlags, as opposed to contentFlags, for multiple-game
		// portability. :sigh:
                    if( g_bBrushPrimit != BRUSH_QUARK && Com_ReadToken( mapfile, SC_COMMENT_SEMICOLON, &token ))
		{
			// overwrite shader values directly from .map file
			Com_SaveToken( mapfile, &token );
			Com_ReadLong( mapfile, false, &flags );
			Com_ReadLong( mapfile, false, NULL );
			Com_ReadLong( mapfile, false, NULL );
			if( flags & C_DETAIL ) side->compileFlags |= C_DETAIL;
		}		

		if( mapfile->TXcommand == '1' || mapfile->TXcommand == '2' )
		{
			// we are QuArK mode and need to translate some numbers to align textures its way
			// from QuArK, the texture vectors are given directly from the three points
			vec3_t          texMat[2];
			float           dot22, dot23, dot33, mdet, aa, bb, dd;
			int             j, k;

			g_bBrushPrimit = BRUSH_QUARK;	// we can detect it only here
			k = mapfile->TXcommand - '0';
			for( j = 0; j < 3; j++ )
				texMat[1][j] = (planePoints[k][j] - planePoints[0][j]) * (0.0078125f);	// QuArK magic value

			k = 3 - k;
			for( j = 0; j < 3; j++ )
				texMat[0][j] = (planePoints[k][j] - planePoints[0][j]) * (0.0078125f);	// QuArK magic value

			dot22 = DotProduct( texMat[0], texMat[0] );
			dot23 = DotProduct( texMat[0], texMat[1] );
			dot33 = DotProduct( texMat[1], texMat[1] );
			mdet = dot22 * dot33 - dot23 * dot23;
			if( mdet < 1E-6 && mdet > -1E-6 )
			{
				aa = bb = dd = 0;
				MsgDev( D_WARN, "Entity %i, Brush %i: degenerate QuArK-style texture: \n", buildBrush->entityNum, buildBrush->brushNum );
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
				vects.quark.vecs[0][j] = aa * texMat[0][j] + bb * texMat[1][j];
				vects.quark.vecs[1][j] = -(bb * texMat[0][j] + dd * texMat[1][j]);
			}
			vects.quark.vecs[0][3] = -DotProduct( vects.quark.vecs[0], planePoints[0] );
			vects.quark.vecs[1][3] = -DotProduct( vects.quark.vecs[1], planePoints[0] );
		}

		// find the plane number
		planenum = MapPlaneFromPoints( planePoints );
		if( planenum == -1 )
		{
			MsgDev( D_ERROR, "Entity %i, Brush %i: plane with no normal\n", buildBrush->entityNum, buildBrush->brushNum );
			continue;
		}
		side->planenum = planenum;

		if( g_bBrushPrimit == BRUSH_QUARK ) 
		{
			// QuArK format completely matched with internal
			// FIXME: don't calculate vecs, use QuArK texMat instead ?
			Mem_Copy( side->vecs, vects.quark.vecs, sizeof( side->vecs ));
		}
		else if( g_bBrushPrimit != BRUSH_RADIANT )
		{
			vec3_t	vecs[2];
			float	ang, sinv, cosv, ns, nt;
			int	i, j, sv, tv;
			
			if( g_bBrushPrimit == BRUSH_WORLDCRAFT_21 )
				TextureAxisFromPlane( &mapplanes[planenum], vecs[0], vecs[1] );
			if( !vects.hammer.scale[0] ) vects.hammer.scale[0] = 1.0f;
			if( !vects.hammer.scale[1] ) vects.hammer.scale[1] = 1.0f;

			if( g_bBrushPrimit == BRUSH_WORLDCRAFT_21 )
			{
				// rotate axis
				if( vects.hammer.rotate == 0 )
				{
					sinv = 0;
					cosv = 1;
				}
				else if( vects.hammer.rotate == 90 )
				{
					sinv = 1;
					cosv = 0;
				}
				else if( vects.hammer.rotate == 180 )
				{
					sinv = 0;
					cosv = -1;
				}
				else if( vects.hammer.rotate == 270 )
				{
					sinv = -1;
					cosv = 0;
				}
				else
				{
					ang = vects.hammer.rotate / 180 * M_PI;
					sinv = sin( ang );
					cosv = cos( ang );
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
						side->vecs[i][j] = vecs[i][j] / vects.hammer.scale[i];
			}
			else if( g_bBrushPrimit == BRUSH_WORLDCRAFT_22 || g_bBrushPrimit == BRUSH_GEARCRAFT_40 )
			{
				float	scale;

				scale = 1.0f / vects.hammer.scale[0];
				VectorScale( vects.hammer.UAxis, scale, side->vecs[0] );
				scale = 1.0f / vects.hammer.scale[1];
				VectorScale( vects.hammer.VAxis, scale, side->vecs[1] );
			}

			// add shifts
			side->vecs[0][3] = vects.hammer.shift[0];
			side->vecs[1][3] = vects.hammer.shift[1];
		}
	}
	
	if( g_bBrushPrimit == BRUSH_RADIANT )
	{
		Com_SaveToken( mapfile, &token );
		Com_CheckToken( mapfile, "}" );
		Com_CheckToken( mapfile, "}" );
	}
}



/*
=================
RemoveDuplicateBrushPlanes

returns false if the brush has a mirrored set of planes,
meaning it encloses no volume.
also removes planes without any normal
=================
*/
bool RemoveDuplicateBrushPlanes( brush_t *b )
{
	int	i, j, k;
	side_t	*sides;

	sides = b->sides;

	for( i = 1; i < b->numsides; i++ )
	{

		// check for a degenerate plane
		if( sides[i].planenum == -1 )
		{
			Msg( "Entity %i, Brush %i degenerate plane\n", b->entityNum, b->brushNum );
			for( k = i + 1; k < b->numsides; k++ )
				sides[k-1] = sides[k]; // remove it
			b->numsides--;
			i--;
			continue;
		}

		// check for duplication and mirroring
		for( j = 0; j < i; j++ )
		{
			if( sides[i].planenum == sides[j].planenum )
			{
				Msg( "Entity %i, Brush %i duplicate plane", b->entityNum, b->brushNum );
			
				for( k = i + 1; k < b->numsides; k++ )
					sides[k-1] = sides[k]; // remove the second duplicate
				b->numsides--;
				i--;
				break;
			}

			if( sides[i].planenum == ( sides[j].planenum ^ 1 ))
			{
				// mirror plane, brush is invalid
				Msg( "Entity %i, Brush %i mirrored plane", b->entityNum, b->brushNum );
				return false;
			}
		}
	}
	return true;
}

/*
=================
ParseBrush

parses a brush out of a map file and sets it up
=================
*/
static bool ParseBrush( bool onlyLights )
{
	brush_t	*b;
	
	ParseRawBrush( onlyLights );
	
	// only go this far?
	if( onlyLights ) return true;
	
	buildBrush->portalareas[0] = -1;
	buildBrush->portalareas[1] = -1;
	buildBrush->entityNum = numMapEntities - 1;
	buildBrush->brushNum = entitySourceBrushes;
	
	// if there are mirrored planes, the entire brush is invalid
	if( !RemoveDuplicateBrushPlanes( buildBrush ))
		return true;
	
	SetBrushContents( buildBrush );
	
	// allow detail brushes to be removed
	if( nodetail && (buildBrush->compileFlags & C_DETAIL ))
		return true;
	
	// allow liquid brushes to be removed
	if( nowater && (buildBrush->compileFlags & C_LIQUID ))
		return true;
	
	// allow hint brushes to be removed
	if( noHint && (buildBrush->compileFlags & C_HINT ))
		return true;
	
	b = FinishBrush();
	if( !b ) return false;
	return true;
}

/*
=================
MoveBrushesToWorld

takes all of the brushes from the current entity and
adds them to the world's brush list
(used by func_group)
=================
*/
void MoveBrushesToWorld( entity_t *ent )
{
	brush_t		*b, *next;
	parseMesh_t	*pm;
	
	for( b = ent->brushes; b != NULL; b = next )
	{
		next = b->next;
		
		// link opaque brushes to head of list, translucent brushes to end
		if( b->opaque || entities[0].lastBrush == NULL )
		{
			b->next = entities[0].brushes;
			entities[0].brushes = b;
			if( entities[0].lastBrush == NULL )
				entities[0].lastBrush = b;
		}
		else
		{
			b->next = NULL;
			entities[0].lastBrush->next = b;
			entities[0].lastBrush = b;
		}
	}
	ent->brushes = NULL;
	
	// move colormod brushes
	if( ent->colorModBrushes != NULL )
	{
		for( b = ent->colorModBrushes; b->nextColorModBrush != NULL; b = b->nextColorModBrush );
		
		b->nextColorModBrush = entities[0].colorModBrushes;
		entities[0].colorModBrushes = ent->colorModBrushes;
		ent->colorModBrushes = NULL;
	}
	
	// move patches
	if( ent->patches != NULL )
	{
		for( pm = ent->patches; pm->next != NULL; pm = pm->next );
		
		pm->next = entities[0].patches;
		entities[0].patches = ent->patches;
		ent->patches = NULL;
	}
}



/*
=================
AdjustBrushesForOrigin
=================
*/
void AdjustBrushesForOrigin( entity_t *ent )
{
	int		i;
	side_t		*s;
	vec_t		newdist;
	brush_t		*b;
	parseMesh_t	*p;
	
	for( b = ent->brushes; b != NULL; b = b->next )
	{
		for( i = 0; i < b->numsides; i++)
		{
			s = &b->sides[i];
			
			newdist = mapplanes[ s->planenum ].dist - DotProduct( mapplanes[ s->planenum ].normal, ent->origin );
			s->planenum = FindFloatPlane( mapplanes[ s->planenum ].normal, newdist, 0, NULL );
		}
		
		// rebuild brush windings (just offsetting the winding above should be fine)
		CreateBrushWindings( b );
	}
	
	for( p = ent->patches; p != NULL; p = p->next )
	{
		for( i = 0; i < (p->mesh.width * p->mesh.height); i++ )
			VectorSubtract( p->mesh.verts[i].xyz, ent->origin, p->mesh.verts[i].xyz );
	}
}



/*
=================
SetEntityBounds

finds the bounds of an entity's brushes (necessary for terrain-style generic metashaders)
=================
*/
void SetEntityBounds( entity_t *e )
{
	int		i;
	brush_t		*b;
	parseMesh_t	*p;
	vec3_t		mins, maxs;
	const char	*value;

	ClearBounds( mins, maxs );
	for( b = e->brushes; b; b = b->next )
	{
		AddPointToBounds( b->mins, mins, maxs );
		AddPointToBounds( b->maxs, mins, maxs );
	}
	for( p = e->patches; p; p = p->next )
	{
		for( i = 0; i < (p->mesh.width * p->mesh.height); i++ )
			AddPointToBounds( p->mesh.verts[i].xyz, mins, maxs );
	}
	
	value = ValueForKey( e, "min" ); 
	if( value[0] != '\0' ) GetVectorForKey( e, "min", mins );
	value = ValueForKey( e, "max" ); 
	if( value[0] != '\0' ) GetVectorForKey( e, "max", maxs );
	
	for( b = e->brushes; b; b = b->next )
	{
		VectorCopy( mins, b->eMins );
		VectorCopy( maxs, b->eMaxs );
	}
	for( p = e->patches; p; p = p->next )
	{
		VectorCopy( mins, p->eMins );
		VectorCopy( maxs, p->eMaxs );
	}
}

/*
=================
LoadEntityIndexMap

based on LoadAlphaMap() from terrain.c, a little more generic
=================
*/
void LoadEntityIndexMap( entity_t *e )
{
	int		i, size, numLayers;
	const char	*value, *indexMapFilename, *shader;
	char		offset[ 4096 ], *search, *space;
	rgbdata_t		*image;
	byte		*pixels;
	uint		*pixels32;
	indexMap_t	*im;
	brush_t		*b;
	parseMesh_t	*p;
	
	
	if( e->brushes == NULL && e->patches == NULL )
		return;
	
	value = ValueForKey( e, "_indexmap" );
	if( value[0] == '\0' ) value = ValueForKey( e, "alphamap" );
	if( value[0] == '\0' ) return;
	indexMapFilename = value;
	
	// get number of layers (support legacy "layers" key as well)
	value = ValueForKey( e, "_layers" );
	if( value[0] == '\0' ) value = ValueForKey( e, "layers" );
	if( value[0] == '\0' )
	{
		Msg( "Warning: Entity with index/alpha map \"%s\" has missing \"_layers\" or \"layers\" key\n", indexMapFilename );
		Msg( "Entity will not be textured properly. Check your keys/values.\n" );
		return;
	}
	numLayers = com.atoi( value );
	if( numLayers < 1 )
	{
		Msg( "Warning: Entity with index/alpha map \"%s\" has < 1 layer (%d)\n", indexMapFilename, numLayers );
		Msg( "Entity will not be textured properly. Check your keys/values.\n" );
		return;
	}
	
	// get base shader name (support legacy "shader" key as well)
	value = ValueForKey( mapEnt, "_shader" );
	if( value[0] == '\0' ) value = ValueForKey( e, "shader" );
	if( value[0] == '\0' )
	{
		Msg( "Warning: Entity with index/alpha map \"%s\" has missing \"_shader\" or \"shader\" key\n", indexMapFilename );
		Msg( "Entity will not be textured properly. Check your keys/values.\n" );
		return;
	}
	shader = value;
	
	MsgDev( D_NOTE, "Entity %d (%s) has shader index map \"%s\"\n",  mapEnt->mapEntityNum, ValueForKey( e, "classname" ), indexMapFilename );

	image = FS_LoadImage( indexMapFilename, NULL, 0 );
	if( !image ) return;

	Image_Process( &image, 0, 0, IMAGE_FORCE_RGBA );
	
	size = image->width * image->height;
	pixels = Malloc( size );
	pixels32 = (uint *)image->buffer;

	for( i = 0; i < size; i++ )
	{
		pixels[i] = ((pixels32[i] & 0xFF) * numLayers) / 256;
		if( pixels[i] >= numLayers ) pixels[i] = numLayers - 1;
	}

	// the index map must be at least 2x2 pixels
	if( image->width < 2 || image->height < 2 )
	{
		Msg( "Warning: Entity with index/alpha map \"%s\" is smaller than 2x2 pixels\n", indexMapFilename );
		Msg( "Entity will not be textured properly. Check your keys/values.\n" );
		FS_FreeImage( image );
		return;
	}

	// create a new index map
	im = Malloc( sizeof( *im ));
	im->w = image->width;
	im->h = image->height;
	im->numLayers = numLayers;
	com.strncpy( im->name, indexMapFilename, sizeof( im->name ));
	com.strncpy( im->shader, shader, sizeof( im->shader ));
	im->pixels = pixels;
	
	value = ValueForKey( mapEnt, "_offsets" );
	if( value[0] == '\0' ) value = ValueForKey( e, "offsets" );
	if( value[0] != '\0' )
	{
		// value is a space-seperated set of numbers
		com.strncpy( offset, value, sizeof( offset ));
		search = offset;
		
		for( i = 0; i < 256 && *search != '\0'; i++ )
		{
			space = com.strstr( search, " " );
			if( space != NULL ) *space = '\0';
			im->offsets[i] = com.atof( search );
			if( space == NULL ) break;
			search = space + 1;
		}
	}
	
	// store the index map in every brush/patch in the entity
	for( b = e->brushes; b != NULL; b = b->next ) b->im = im;
	for( p = e->patches; p != NULL; p = p->next ) p->im = im;

	FS_FreeImage( image );
}

/*
=================
ParseMapEntity

parses a single entity out of a map file
=================
*/
static bool ParseMapEntity( bool onlyLights )
{
	epair_t		*ep;
	token_t		token;
	const char	*classname, *value;
	float		lightmapScale;
	char		shader[ MAX_SHADERPATH ];
	shaderInfo_t	*celShader = NULL;
	brush_t		*brush;
	parseMesh_t	*patch;
	bool		funcGroup;
	int		castShadows, recvShadows;
	static bool	map_type = false;

	if( !Com_ReadToken( mapfile, SC_ALLOW_NEWLINES|SC_COMMENT_SEMICOLON, &token ))
		return false; // end of .map file
	if( com.stricmp( token.string, "{" ))  Sys_Break( "ParseEntity: found %s instead {\n", token.string );
	if( numEntities >= MAX_MAP_ENTITIES ) Sys_Break( "MAX_MAP_ENTITIES limit exceeded\n" );	

	entitySourceBrushes = 0;
	mapEnt = &entities[numEntities];
	numEntities++;
	memset( mapEnt, 0, sizeof( *mapEnt ));
	
	mapEnt->mapEntityNum = numMapEntities;
	numMapEntities++;
	
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
				numMapPatches++;
				ParsePatch( onlyLights );
				g_bBrushPrimit = BRUSH_RADIANT;
			}
			else if( !com.stricmp( token.string, "terrainDef" ))
			{
				MsgDev( D_WARN, "Terrain entity parsing not supported in this build.\n" );
				Com_SkipBracedSection( mapfile, 0 );
				g_bBrushPrimit = BRUSH_RADIANT;
			}
			else if( !com.stricmp( token.string, "brushDef" ))
			{
				// parse brush primitive
				g_bBrushPrimit = BRUSH_RADIANT;
				ParseBrush( onlyLights );
			}
			else
			{
				if( g_bBrushPrimit == BRUSH_RADIANT )
					Sys_Break( "mixed brush primitive with another format\n" );
				if( g_bBrushPrimit == BRUSH_UNKNOWN ) g_bBrushPrimit = BRUSH_WORLDCRAFT_21;
				
				// QuArK or WorldCraft map (unknown at this point)
				Com_SaveToken( mapfile, &token );
				ParseBrush( onlyLights );
			}
			entitySourceBrushes++;
		}
		else
		{
			// parse a key / value pair
			ep = ParseEpair( mapfile, &token );

			if( !com.strcmp( ep->key, "mapversion" ))
			{
				if( com.atoi( ep->value ) == VALVE_FORMAT )
				{
					Msg( "Valve Format Map detected\n" );
					g_bBrushPrimit = BRUSH_WORLDCRAFT_22;
				}
				else if( com.atoi( ep->value ) == GEARBOX_FORMAT )
				{
					Msg( "Gearcraft Format Map detected\n" );
					g_bBrushPrimit = BRUSH_GEARCRAFT_40;
				}
				else g_bBrushPrimit = BRUSH_WORLDCRAFT_21;
			}
			if( ep->key[0] != '\0' && ep->value[0] != '\0' )
			{
				ep->next = mapEnt->epairs;
				mapEnt->epairs = ep;
			}
		}
	}

	if( !map_type && g_bBrushPrimit != BRUSH_UNKNOWN )
	{
		MAPTYPE();
		map_type = true;
	}
	
	classname = ValueForKey( mapEnt, "classname" );
	
	if( onlyLights && com.strnicmp( classname, "light", 5 ))
	{
		numEntities--;
		return true;
	}
	
	if( !com.stricmp( "func_group", classname ))
		funcGroup = true;
	else funcGroup = false;
	
	// worldspawn (and func_groups) default to cast/recv shadows in worldspawn group
	if( funcGroup || mapEnt->mapEntityNum == 0 )
	{
		castShadows = WORLDSPAWN_CAST_SHADOWS;
		recvShadows = WORLDSPAWN_RECV_SHADOWS;
	}
	else // other entities don't cast any shadows, but recv worldspawn shadows
	{
		castShadows = ENTITY_CAST_SHADOWS;
		recvShadows = ENTITY_RECV_SHADOWS;
	}
	
	// get explicit shadow flags
	GetEntityShadowFlags( mapEnt, NULL, &castShadows, &recvShadows );
	
	// get lightmap scaling value for this entity
	if( com.strcmp( "", ValueForKey( mapEnt, "lightmapscale" )) || com.strcmp( "", ValueForKey( mapEnt, "_lightmapscale" )))
	{
		// get lightmap scale from entity
		lightmapScale = FloatForKey( mapEnt, "lightmapscale" );
		if( lightmapScale <= 0.0f ) lightmapScale = FloatForKey( mapEnt, "_lightmapscale" );
		if( lightmapScale > 0.0f ) Msg( "Entity %d (%s) has lightmap scale of %.4f\n", mapEnt->mapEntityNum, classname, lightmapScale );
	}
	else lightmapScale = 0.0f;
	
	// get cel shader :) for this entity
	value = ValueForKey( mapEnt, "_celshader" );
	if( value[0] == '\0' ) value = ValueForKey( &entities[0], "_celshader" );
	if( value[0] != '\0' )
	{
		com.snprintf( shader, sizeof( shader ), "textures/%s", value );
		celShader = ShaderInfoForShader( shader );
		Msg( "Entity %d (%s) has cel shader %s\n", mapEnt->mapEntityNum, classname, celShader->shader );
	}
	else celShader = NULL;
	
	// attach stuff to everything in the entity
	for( brush = mapEnt->brushes; brush != NULL; brush = brush->next )
	{
		brush->entityNum = mapEnt->mapEntityNum;
		brush->castShadows = castShadows;
		brush->recvShadows = recvShadows;
		brush->lightmapScale = lightmapScale;
		brush->celShader = celShader;
	}
	
	for( patch = mapEnt->patches; patch != NULL; patch = patch->next )
	{
		patch->entityNum = mapEnt->mapEntityNum;
		patch->castShadows = castShadows;
		patch->recvShadows = recvShadows;
		patch->lightmapScale = lightmapScale;
		patch->celShader = celShader;
	}
	
	SetEntityBounds( mapEnt );
	
	// load shader index map (equivalent to old terrain alphamap)
	LoadEntityIndexMap( mapEnt );
	
	// get entity origin and adjust brushes
	GetVectorForKey( mapEnt, "origin", mapEnt->origin );
	if( mapEnt->origin[0] || mapEnt->origin[1] || mapEnt->origin[2] )
		AdjustBrushesForOrigin( mapEnt );

	// group_info entities are just for editor grouping
	if( !com.stricmp( "group_info", classname ))
	{
		numEntities--;
		return true;
	}
	
	// group entities are just for editor convenience, toss all brushes into worldspawn
	if( funcGroup )
	{
		MoveBrushesToWorld( mapEnt );
		numEntities--;
		return true;
	}
	return true;
}


/*
=================
LoadMapFile

loads a map file into a list of entities
=================
*/
void LoadMapFile( const char *filename, bool onlyLights )
{		
	brush_t		*b;
	int		oldNumEntities, numMapBrushes;
	
	MsgDev( D_NOTE, "--- LoadMapFile ---\n" );
	Msg( "Loading %s\n", filename );
	
	mapfile = Com_OpenScript( filename, NULL, 0 );
	if( !mapfile ) Sys_Break( "can't loading map file %s.map\n", filename );	
	
	if( onlyLights ) 
		oldNumEntities = numEntities;
	else numEntities = 0;

	c_detail = 0;	
	numMapDrawSurfs = 0;
	g_bBrushPrimit = BRUSH_UNKNOWN;
	
	// allocate a very large temporary brush for building the brushes as they are loaded
	buildBrush = AllocBrush( MAX_BUILD_SIDES );
	
	while( ParseMapEntity( onlyLights ));
	Com_CloseScript( mapfile );
	
	if( onlyLights )
	{
		MsgDev( D_INFO, "%9d light entities\n", numEntities - oldNumEntities );
	}
	else
	{
		ClearBounds( mapMins, mapMaxs );
		for( b = entities[0].brushes; b; b = b->next )
		{
			AddPointToBounds( b->mins, mapMins, mapMaxs );
			AddPointToBounds( b->maxs, mapMins, mapMaxs );
		}

		VectorSubtract( mapMaxs, mapMins, mapSize );		
		numMapBrushes = CountBrushList( entities[0].brushes );
		if(( float )c_detail / (float) numMapBrushes < 0.10f && numMapBrushes > 500 )
			MsgDev( D_WARN, "Over 90 percent structural map detected. Compile time may be adversely affected.\n" );
		
		MsgDev( D_NOTE, "%9d total world brushes\n", numMapBrushes );
		MsgDev( D_NOTE, "%9d detail brushes\n", c_detail );
		MsgDev( D_NOTE, "%9d patches\n", numMapPatches );
		MsgDev( D_NOTE, "%9d boxbevels\n", c_boxbevels );
		MsgDev( D_NOTE, "%9d edgebevels\n", c_edgebevels );
		MsgDev( D_NOTE, "%9d entities\n", numEntities );
		MsgDev( D_NOTE, "%9d planes\n", nummapplanes );
		MsgDev( D_NOTE, "%9d areaportals\n", c_areaportals );
		MsgDev( D_INFO, "World size: %5.0f, %5.0f, %5.0f\n", mapSize[0], mapSize[1], mapSize[2] );
		
		// write bogus map
		if( fakemap ) WriteBSPBrushMap( "fakemap.map", entities[0].brushes );
	}
}