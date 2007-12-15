// map.c

#include "bsplib.h"

extern bool onlyents;

#define ScaleCorrection	(1.0/128.0)

int			nummapbrushes;
mapbrush_t	mapbrushes[MAX_MAP_BRUSHES];

int			nummapbrushsides;
side_t		brushsides[MAX_MAP_SIDES];
brush_texture_t	side_brushtextures[MAX_MAP_SIDES];

int			nummapplanes;
plane_t		mapplanes[MAX_MAP_PLANES];

#define	PLANE_HASHES	1024
plane_t		*planehash[PLANE_HASHES];

vec3_t		map_mins, map_maxs;

int		g_mapversion;

// undefine to make plane finding use linear sort
#define	USE_HASHING

void TestExpandBrushes (void);

int		c_boxbevels;
int		c_edgebevels;

int		c_areaportals;

int		c_clipbrushes;

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
bool	PlaneEqual (plane_t *p, vec3_t normal, vec_t dist)
{
#if 1
	if (
	   fabs(p->normal[0] - normal[0]) < NORMAL_EPSILON
	&& fabs(p->normal[1] - normal[1]) < NORMAL_EPSILON
	&& fabs(p->normal[2] - normal[2]) < NORMAL_EPSILON
	&& fabs(p->dist - dist) < DIST_EPSILON )
		return true;
#else
	if (p->normal[0] == normal[0]
		&& p->normal[1] == normal[1]
		&& p->normal[2] == normal[2]
		&& p->dist == dist)
		return true;
#endif
	return false;
}

/*
================
AddPlaneToHash
================
*/
void	AddPlaneToHash (plane_t *p)
{
	int		hash;

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
int CreateNewFloatPlane (vec3_t normal, vec_t dist)
{
	plane_t	*p, temp;

	if (VectorLength(normal) < 0.5)
		Sys_Error ("FloatPlane: bad normal");
	// create a new plane
	if (nummapplanes+2 > MAX_MAP_PLANES)
		Sys_Error ("MAX_MAP_PLANES");

	p = &mapplanes[nummapplanes];
	VectorCopy (normal, p->normal);
	p->dist = dist;
	p->type = (p+1)->type = PlaneTypeForNormal (p->normal);

	VectorSubtract (vec3_origin, normal, (p+1)->normal);
	(p+1)->dist = -dist;

	nummapplanes += 2;

	// allways put axial planes facing positive first
	if (p->type < 3)
	{
		if (p->normal[0] < 0 || p->normal[1] < 0 || p->normal[2] < 0)
		{
			// flip order
			temp = *p;
			*p = *(p+1);
			*(p+1) = temp;

			AddPlaneToHash (p);
			AddPlaneToHash (p+1);
			return nummapplanes - 1;
		}
	}

	AddPlaneToHash (p);
	AddPlaneToHash (p+1);
	return nummapplanes - 2;
}

/*
==============
SnapVector
==============
*/
void	SnapVector (vec3_t normal)
{
	int		i;

	for (i=0 ; i<3 ; i++)
	{
		if ( fabs(normal[i] - 1) < NORMAL_EPSILON )
		{
			VectorClear (normal);
			normal[i] = 1;
			break;
		}
		if ( fabs(normal[i] - -1) < NORMAL_EPSILON )
		{
			VectorClear (normal);
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
void SnapPlane (vec3_t normal, vec_t *dist)
{
	SnapVector (normal);

	if (fabs(*dist - floor(*dist + 0.5)) < DIST_EPSILON)
		*dist = floor(*dist + 0.5);
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
BrushContents
===========
*/
int BrushContents (mapbrush_t *b)
{
	int			contents;
	side_t		*s;
	int			i;
	int			trans;

	s = &b->original_sides[0];
	contents = s->contents;
	trans = texinfo[s->texinfo].flags;
	for (i = 1; i < b->numsides; i++, s++)
	{
		s = &b->original_sides[i];
		trans |= texinfo[s->texinfo].flags;
		if (s->contents != contents)
		{
			Msg ("Entity %i, Brush %i: mixed face contents\n", b->entitynum, b->brushnum);
			break;
		}
	}

	// if any side is translucent, mark the contents
	// and change solid to window
	if ( trans & (SURF_TRANS33|SURF_TRANS66) )
	{
		contents |= CONTENTS_TRANSLUCENT;
		if (contents & CONTENTS_SOLID)
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
void AddBrushBevels (mapbrush_t *b)
{
	int		axis, dir;
	int		i, j, k, l, order;
	side_t	sidetemp;
	brush_texture_t	tdtemp;
	side_t	*s, *s2;
	vec3_t	normal;
	float	dist;
	winding_t	*w, *w2;
	vec3_t	vec, vec2;
	float	d;

	//
	// add the axial planes
	//
	order = 0;
	for (axis=0 ; axis <3 ; axis++)
	{
		for (dir=-1 ; dir <= 1 ; dir+=2, order++)
		{
			// see if the plane is allready present
			for (i=0, s=b->original_sides ; i<b->numsides ; i++,s++)
			{
				if (mapplanes[s->planenum].normal[axis] == dir)
					break;
			}

			if (i == b->numsides)
			{	// add a new side
				if (nummapbrushsides == MAX_MAP_BRUSHSIDES)
					Sys_Error ("MAX_MAP_BRUSHSIDES");
				nummapbrushsides++;
				b->numsides++;
				VectorClear (normal);
				normal[axis] = dir;
				if (dir == 1)
					dist = b->maxs[axis];
				else
					dist = -b->mins[axis];
				s->planenum = FindFloatPlane (normal, dist);
				s->texinfo = b->original_sides[0].texinfo;
				s->contents = b->original_sides[0].contents;
				s->bevel = true;
				c_boxbevels++;
			}

			// if the plane is not in it canonical order, swap it
			if (i != order)
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

	//
	// add the edge bevels
	//
	if (b->numsides == 6)
		return;		// pure axial

	// test the non-axial plane edges
	for (i=6 ; i<b->numsides ; i++)
	{
		s = b->original_sides + i;
		w = s->winding;
		if (!w)
			continue;
		for (j=0 ; j<w->numpoints ; j++)
		{
			k = (j+1)%w->numpoints;
			VectorSubtract (w->p[j], w->p[k], vec);
			if (VectorNormalize (vec) < 0.5)
				continue;
			SnapVector (vec);
			for (k=0 ; k<3 ; k++)
				if ( vec[k] == -1 || vec[k] == 1)
					break;	// axial
			if (k != 3)
				continue;	// only test non-axial edges

			// try the six possible slanted axials from this edge
			for (axis=0 ; axis <3 ; axis++)
			{
				for (dir=-1 ; dir <= 1 ; dir+=2)
				{
					// construct a plane
					VectorClear (vec2);
					vec2[axis] = dir;
					CrossProduct (vec, vec2, normal);
					if (VectorNormalize (normal) < 0.5)
						continue;
					dist = DotProduct (w->p[j], normal);

					// if all the points on all the sides are
					// behind this plane, it is a proper edge bevel
					for (k=0 ; k<b->numsides ; k++)
					{
						// if this plane has allready been used, skip it
						if (PlaneEqual (&mapplanes[b->original_sides[k].planenum]
							, normal, dist) )
							break;

						w2 = b->original_sides[k].winding;
						if (!w2)
							continue;
						for (l=0 ; l<w2->numpoints ; l++)
						{
							d = DotProduct (w2->p[l], normal) - dist;
							if (d > 0.1)
								break;	// point in front
						}
						if (l != w2->numpoints)
							break;
					}

					if (k != b->numsides)
						continue;	// wasn't part of the outer hull
					// add this plane
					if (nummapbrushsides == MAX_MAP_BRUSHSIDES)
						Sys_Error ("MAX_MAP_BRUSHSIDES");
					nummapbrushsides++;
					s2 = &b->original_sides[b->numsides];
					s2->planenum = FindFloatPlane (normal, dist);
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
bool MakeBrushWindings (mapbrush_t *ob)
{
	int			i, j;
	winding_t	*w;
	side_t		*side;
	plane_t		*plane;

	ClearBounds (ob->mins, ob->maxs);

	for (i=0 ; i<ob->numsides ; i++)
	{
		plane = &mapplanes[ob->original_sides[i].planenum];
		w = BaseWindingForPlane (plane->normal, plane->dist);
		for (j=0 ; j<ob->numsides && w; j++)
		{
			if (i == j)
				continue;
			if (ob->original_sides[j].bevel)
				continue;
			plane = &mapplanes[ob->original_sides[j].planenum^1];
			ChopWindingInPlace (&w, plane->normal, plane->dist, 0); //CLIP_EPSILON);
		}

		side = &ob->original_sides[i];
		side->winding = w;
		if (w)
		{
			side->visible = true;
			for (j=0 ; j<w->numpoints ; j++)
				AddPointToBounds (w->p[j], ob->mins, ob->maxs);
		}
	}

	for (i=0 ; i<3 ; i++)
	{
		if (ob->mins[0] < -4096 || ob->maxs[0] > 4096)
			Msg ("entity %i, brush %i: bounds out of range\n", ob->entitynum, ob->brushnum);
		if (ob->mins[0] > 4096 || ob->maxs[0] < -4096)
			Msg ("entity %i, brush %i: no visible sides on brush\n", ob->entitynum, ob->brushnum);
	}

	return true;
}


/*
=================
ParseBrush
=================
*/
void ParseBrush (bsp_entity_t *mapent)
{
	mapbrush_t	*b;
	int		i,j, k;
	int		mt;
	side_t		*side, *s2;
	int		planenum;
	brush_texture_t	td;
	vec_t		planepts[3][3]; //quark used float coords

	if (nummapbrushes == MAX_MAP_BRUSHES)
		Sys_Error ("nummapbrushes == MAX_MAP_BRUSHES");

	b = &mapbrushes[nummapbrushes];
	b->original_sides = &brushsides[nummapbrushsides];
	b->entitynum = num_entities-1;
	b->brushnum = nummapbrushes - mapent->firstbrush;

	do
	{
		g_TXcommand = 0;
		if (!Com_GetToken (true)) break;
		if (Com_MatchToken("}")) break;

		if (nummapbrushsides == MAX_MAP_BRUSHSIDES)
			Sys_Error ("MAX_MAP_BRUSHSIDES");
		side = &brushsides[nummapbrushsides];

		// read the three point plane definition
		for (i = 0; i < 3; i++)
		{
			if (i != 0) Com_GetToken (true);
			if(!Com_MatchToken("(")) Sys_Error ("ParseBrush: error parsing %d", b->brushnum );
			
			for (j=0 ; j<3 ; j++)
			{
				Com_GetToken (false);
				planepts[i][j] = atof(com_token);
			}
			
			Com_GetToken (false);
			if(!Com_MatchToken(")"))Sys_Error ("parsing brush");
		}

		// read the texturedef
		Com_GetToken (false);
		strcpy (td.name, com_token);

		if(g_mapversion == VALVE_FORMAT) // Worldcraft 2.2+
                    {
			// texture U axis
			Com_GetToken(false);
			if (strcmp(com_token, "[")) Sys_Error("missing '[' in texturedef (U)");
			Com_GetToken(false);
			td.vects.valve.UAxis[0] = atof(com_token);
			Com_GetToken(false);
			td.vects.valve.UAxis[1] = atof(com_token);
			Com_GetToken(false);
			td.vects.valve.UAxis[2] = atof(com_token);
			Com_GetToken(false);
			td.vects.valve.shift[0] = atof(com_token);
			Com_GetToken(false);
			if (strcmp(com_token, "]")) Sys_Error("missing ']' in texturedef (U)");

			// texture V axis
			Com_GetToken(false);
			if (strcmp(com_token, "[")) Sys_Error("missing '[' in texturedef (V)");
			Com_GetToken(false);
			td.vects.valve.VAxis[0] = atof(com_token);
			Com_GetToken(false);
			td.vects.valve.VAxis[1] = atof(com_token);
			Com_GetToken(false);
			td.vects.valve.VAxis[2] = atof(com_token);
			Com_GetToken(false);
			td.vects.valve.shift[1] = atof(com_token);
			Com_GetToken(false);
			if (strcmp(com_token, "]")) Sys_Error("missing ']' in texturedef (V)");

			// Texture rotation is implicit in U/V axes.
			Com_GetToken(false);
			td.vects.valve.rotate = 0;

			// texure scale
			Com_GetToken(false);
			td.vects.valve.scale[0] = atof(com_token);
			Com_GetToken(false);
			td.vects.valve.scale[1] = atof(com_token);
                    }
		else
		{
			// Worldcraft 2.1-, Radiant
			Com_GetToken (false);
			td.vects.valve.shift[0] = atof(com_token);
			Com_GetToken (false);
			td.vects.valve.shift[1] = atof(com_token);
			Com_GetToken (false);
			td.vects.valve.rotate = atof(com_token);	
			Com_GetToken (false);
			td.vects.valve.scale[0] = atof(com_token);
			Com_GetToken (false);
			td.vects.valve.scale[1] = atof(com_token);
                    }

		if ((g_TXcommand == '1' || g_TXcommand == '2'))
		{
			// We are QuArK mode and need to translate some numbers to align textures its way
			// from QuArK, the texture vectors are given directly from the three points
			vec3_t          TexPt[2];
			int             k;
			float           dot22, dot23, dot33, mdet, aa, bb, dd;

			k = g_TXcommand - '0';
			for (j = 0; j < 3; j++)
			{
				TexPt[1][j] = (planepts[k][j] - planepts[0][j]) * ScaleCorrection;
            		}

			k = 3 - k;
			for (j = 0; j < 3; j++)
			{
				TexPt[0][j] = (planepts[k][j] - planepts[0][j]) * ScaleCorrection;
			}

			dot22 = DotProduct(TexPt[0], TexPt[0]);
			dot23 = DotProduct(TexPt[0], TexPt[1]);
			dot33 = DotProduct(TexPt[1], TexPt[1]);
			mdet = dot22 * dot33 - dot23 * dot23;
			if (mdet < 1E-6 && mdet > -1E-6)
			{
				aa = bb = dd = 0;
				Msg("Degenerate QuArK-style brush texture : Entity %i, Brush %i\n", b->entitynum, b->brushnum);
			}
			else
			{
				mdet = 1.0 / mdet;
				aa = dot33 * mdet;
				bb = -dot23 * mdet;
				dd = dot22 * mdet;
			}
			for (j = 0; j < 3; j++)
			{
				td.vects.quark.vects[0][j] = aa * TexPt[0][j] + bb * TexPt[1][j];
				td.vects.quark.vects[1][j] = -(bb * TexPt[0][j] + dd * TexPt[1][j]);
			}
			td.vects.quark.vects[0][3] = -DotProduct(td.vects.quark.vects[0], planepts[0]);
			td.vects.quark.vects[1][3] = -DotProduct(td.vects.quark.vects[1], planepts[0]);
		}
		td.txcommand = g_TXcommand;// Quark stuff, but needs setting always

		// find default flags and values
		mt = FindMiptex (td.name);
		td.size[0] = textureref[mt].size[0];
		td.size[1] = textureref[mt].size[1];
		td.flags = textureref[mt].flags;
		td.value = textureref[mt].value;
		side->contents = textureref[mt].contents;
		side->surf = td.flags = textureref[mt].flags;
                    
		//Msg("flags %d, value %d, contents %d\n", td.flags, td.value, side->contents );
		
		if (Com_TryToken()) //first com_token will be get automatically
		{
			side->contents = atoi(com_token);
			Com_GetToken (false);
			side->surf = td.flags = atoi(com_token);
			Com_GetToken (false);
			td.value = atoi(com_token);
		}

		// translucent objects are automatically classified as detail
		if (side->surf & (SURF_TRANS33|SURF_TRANS66) )
			side->contents |= CONTENTS_DETAIL;
		if (side->contents & (CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP) )
			side->contents |= CONTENTS_DETAIL;
		if (!(side->contents & ((LAST_VISIBLE_CONTENTS-1) 
			| CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP|CONTENTS_MIST)  ) )
			side->contents |= CONTENTS_SOLID;

		// hints and skips are never detail, and have no content
		if (side->surf & (SURF_HINT|SURF_SKIP) )
		{
			side->contents = 0;
			side->surf &= ~CONTENTS_DETAIL;
		}

		// find the plane number
		planenum = PlaneFromPoints (planepts[0], planepts[1], planepts[2]);
		if (planenum == -1)
		{
			Msg ("Entity %i, Brush %i: plane with no normal\n", b->entitynum, b->brushnum);
			continue;
		}

		//
		// see if the plane has been used already
		//
		for (k=0 ; k<b->numsides ; k++)
		{
			s2 = b->original_sides + k;
			if (s2->planenum == planenum)
			{
				Msg("Entity %i, Brush %i: duplicate plane\n", b->entitynum, b->brushnum);
				break;
			}
			if ( s2->planenum == (planenum^1) )
			{
				Msg("Entity %i, Brush %i: mirrored plane\n", b->entitynum, b->brushnum);
				break;
			}
		}
		if (k != b->numsides)
			continue;		// duplicated

		//
		// keep this side
		//

		side = b->original_sides + b->numsides;
		side->planenum = planenum;
		side->texinfo = TexinfoForBrushTexture (&mapplanes[planenum], &td, vec3_origin);
		// save the td off in case there is an origin brush and we
		// have to recalculate the texinfo
		side_brushtextures[nummapbrushsides] = td;

		nummapbrushsides++;
		b->numsides++;
	} while (1);

	// get the content for the entire brush
	b->contents = BrushContents (b);

	// create windings for sides and bounds for brush
	MakeBrushWindings (b);

	// brushes that will not be visible at all will never be
	// used as bsp splitters
	if (b->contents & (CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP) )
	{
		c_clipbrushes++;
		for (i=0 ; i<b->numsides ; i++)
			b->original_sides[i].texinfo = TEXINFO_NODE;
	}

	//
	// origin brushes are removed, but they set
	// the rotation origin for the rest of the brushes
	// in the entity.  After the entire entity is parsed,
	// the planenums and texinfos will be adjusted for
	// the origin brush
	//
	if (b->contents & CONTENTS_ORIGIN)
	{
		char	string[32];
		vec3_t	size, movedir, origin;

		if (num_entities == 1)
		{
			Sys_Error ("Entity %i, Brush %i: origin brushes not allowed in world", b->entitynum, b->brushnum);
			return;
		}

		VectorAdd (b->mins, b->maxs, origin);
		VectorScale (origin, 0.5, origin);

		//calcualte movedir (Xash 0.4 style)
		VectorSubtract(b->maxs, b->mins, size );
		if (size[2] > size[0] && size[2] > size[1])
		{
            		movedir[0] = 0;//x-rotate
            		movedir[1] = 1;
            		movedir[2] = 0;
		}
		else if (size[1] > size[2] && size[1] > size[0])
		{
            		movedir[0] = 1;//y-rotate
            		movedir[1] = 0;
            		movedir[2] = 0;
		}
		else if (size[0] > size[2] && size[0] > size[1])
		{
            		movedir[0] = 0;//z-rotate
            		movedir[1] = 0;
            		movedir[2] = 1;
		}
		else
		{
            		movedir[0] = 0;//deafult x-rotate
            		movedir[1] = 1;
            		movedir[2] = 0;
            		Msg("Entity %d has origin with invalid form! Make axis form for it", b->entitynum );  
		}

		sprintf (string, "%i %i %i", (int)origin[0], (int)origin[1], (int)origin[2]);
		SetKeyValue (&entities[b->entitynum], "origin", string);
		sprintf (string, "%i %i %i", (int)movedir[0], (int)movedir[1], (int)movedir[2]);
		SetKeyValue (&entities[b->entitynum], "movedir", string);

		VectorCopy (origin, entities[b->entitynum].origin);

		// don't keep this brush
		b->numsides = 0;

		return;
	}

	AddBrushBevels (b);

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
void MoveBrushesToWorld (bsp_entity_t *mapent)
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
	memcpy (temp, mapbrushes + mapent->firstbrush, newbrushes*sizeof(mapbrush_t));

	// make space to move the brushes (overlapped copy)
	memmove (mapbrushes + worldbrushes + newbrushes,
		mapbrushes + worldbrushes,
		sizeof(mapbrush_t) * (nummapbrushes - worldbrushes - newbrushes) );

	// copy the new brushes down
	memcpy (mapbrushes + worldbrushes, temp, sizeof(mapbrush_t) * newbrushes);

	// fix up indexes
	entities[0].numbrushes += newbrushes;
	for (i=1 ; i<num_entities ; i++)
		entities[i].firstbrush += newbrushes;
	Free (temp);

	mapent->numbrushes = 0;
}

/*
================
ParseMapEntity
================
*/
bool	ParseMapEntity (void)
{
	bsp_entity_t	*mapent;
	epair_t		*e;
	side_t		*s;
	int			i, j;
	int			startbrush, startsides;
	vec_t		newdist;
	mapbrush_t	*b;

	if (!Com_GetToken (true)) return false;

	if(!Com_MatchToken( "{" )) Sys_Error ("ParseEntity: { not found");
	
	if (num_entities == MAX_MAP_ENTITIES)
		Sys_Error ("num_entities == MAX_MAP_ENTITIES");

	startbrush = nummapbrushes;
	startsides = nummapbrushsides;

	mapent = &entities[num_entities];
	num_entities++;
	memset (mapent, 0, sizeof(*mapent));
	mapent->firstbrush = nummapbrushes;
	mapent->numbrushes = 0;
//	mapent->portalareas[0] = -1;
//	mapent->portalareas[1] = -1;

	do
	{
		if (!Com_GetToken (true)) Sys_Error ("ParseEntity: EOF without closing brace");
		if (Com_MatchToken( "}")) break;
		if (Com_MatchToken( "{")) ParseBrush (mapent);
		else
		{
			e = ParseEpair ();

			if (!strcmp(e->key, "mapversion"))
			{
				g_mapversion = atoi(e->value);
				MsgDev(D_NOTE, "mapversion %d\n", g_mapversion );
			}
			e->next = mapent->epairs;
			mapent->epairs = e;
		}
	} while (1);

	GetVectorForKey (mapent, "origin", mapent->origin);

	//
	// if there was an origin brush, offset all of the planes and texinfo
	//
	if (mapent->origin[0] || mapent->origin[1] || mapent->origin[2])
	{
		for (i=0 ; i<mapent->numbrushes ; i++)
		{
			b = &mapbrushes[mapent->firstbrush + i];
			for (j=0 ; j<b->numsides ; j++)
			{
				s = &b->original_sides[j];
				newdist = mapplanes[s->planenum].dist - DotProduct (mapplanes[s->planenum].normal, mapent->origin);
				s->planenum = FindFloatPlane (mapplanes[s->planenum].normal, newdist);
				s->texinfo = TexinfoForBrushTexture (&mapplanes[s->planenum], &side_brushtextures[s-brushsides], mapent->origin);
			}
			MakeBrushWindings (b);
		}
	}

	// group entities are just for editor convenience
	// toss all brushes into the world entity
	if (!strcmp ("func_group", ValueForKey (mapent, "classname")))
	{
		MoveBrushesToWorld (mapent);
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
		sprintf (str, "%i", c_areaportals);
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
void LoadMapFile (void)
{		
	int i;
	bool load;
          char path[MAX_SYSPATH];
	
	sprintf (path, "maps/%s.map", gs_mapname );
	load = Com_LoadScript(path, NULL, 0);

	nummapbrushsides = 0;
	num_entities = 0;
	
	if(!load) Sys_Break("can't loading map file %s\n", path );
	MsgDev(D_NOTE, "reading %s\n", path);
	
	while(ParseMapEntity());

	ClearBounds (map_mins, map_maxs);
	for (i = 0; i < entities[0].numbrushes; i++)
	{
		if (mapbrushes[i].mins[0] > 4096)
			continue;	// no valid points
		AddPointToBounds (mapbrushes[i].mins, map_mins, map_maxs);
		AddPointToBounds (mapbrushes[i].maxs, map_mins, map_maxs);
	}

	MsgDev(D_INFO, "%5i brushes\n", nummapbrushes);
	MsgDev(D_INFO, "%5i clipbrushes\n", c_clipbrushes);
	MsgDev(D_INFO, "%5i total sides\n", nummapbrushsides);
	MsgDev(D_INFO, "%5i entities\n", num_entities);
	MsgDev(D_INFO, "%5i planes\n", nummapplanes);
	MsgDev(D_INFO, "%5i areaportals\n", c_areaportals);
}