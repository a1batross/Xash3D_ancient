//=======================================================================
//			Copyright XashXT Group 2008 ©
//		    	lightmaps.c - lightmap manager
//=======================================================================

#include <stdio.h>
#include "bsplib.h"
#include "const.h"

float	r_avertexnormals[NUMVERTEXNORMALS][3] =
{
#include "anorms.h"
};

// stupid legacy
#define ANGLE_UP		-1
#define ANGLE_DOWN		-2

typedef struct directlight_s
{
	struct directlight_s *next;
	emittype_t	type;
	int		style;

	union
	{
		vec3_t	intensity;	// scaled by intensity
		vec3_t	color;		// normalized light scale
	};

	vec3_t		origin;
	vec3_t		normal;		// for surfaces and spotlights
	float		lightscale;	// same as intensity	
	float		stopdot;		// for spotlights
	float		stopdot2;		// for spotlights

	dplane_t		*plane;
	dleaf_t		*leaf;
} directlight_t;

typedef struct
{
	dsurface_t	*faces[2];
	bool		coplanar;
} edgeshare_t;

static byte	pvs[(MAX_MAP_LEAFS+7)/8];
edgeshare_t	edgeshare[MAX_MAP_EDGES];

int		facelinks[MAX_MAP_SURFACES];
int		planelinks[2][MAX_MAP_PLANES];

/*
============
LinkPlaneFaces
============
*/
void LinkPlaneFaces (void)
{
	int		i;
	dsurface_t	*f;

	f = dsurfaces;
	for (i=0 ; i<numsurfaces ; i++, f++)
	{
		facelinks[i] = planelinks[f->side][f->planenum];
		planelinks[f->side][f->planenum] = i;
	}
}

/*
============
PairEdges
============
*/
void PairEdges (void)
{
	int		i, j, k;
	dsurface_t	*f;
	edgeshare_t	*e;

	f = dsurfaces;
	for (i=0 ; i<numsurfaces ; i++, f++)
	{
		for (j=0 ; j<f->numedges ; j++)
		{
			k = dsurfedges[f->firstedge + j];
			if (k < 0)
			{
				e = &edgeshare[-k];
				e->faces[1] = f;
			}
			else
			{
				e = &edgeshare[k];
				e->faces[0] = f;
			}

			if (e->faces[0] && e->faces[1])
			{
				// determine if coplanar
				if (e->faces[0]->planenum == e->faces[1]->planenum)
					e->coplanar = true;
			}
		}
	}
}

/*
=================================================================

  POINT TRIANGULATION

=================================================================
*/

typedef struct triedge_s
{
	int			p0, p1;
	vec3_t		normal;
	vec_t		dist;
	struct triangle_s	*tri;
} triedge_t;

typedef struct triangle_s
{
	triedge_t	*edges[3];
} triangle_t;

#define	MAX_TRI_POINTS		1024
#define	MAX_TRI_EDGES		(MAX_TRI_POINTS*6)
#define	MAX_TRI_TRIS		(MAX_TRI_POINTS*2)

typedef struct
{
	int			numpoints;
	int			numedges;
	int			numtris;
	dplane_t	*plane;
	triedge_t	*edgematrix[MAX_TRI_POINTS][MAX_TRI_POINTS];
	patch_t		*points[MAX_TRI_POINTS];
	triedge_t	edges[MAX_TRI_EDGES];
	triangle_t	tris[MAX_TRI_TRIS];
} triangulation_t;

/*
===============
AllocTriangulation
===============
*/
triangulation_t	*AllocTriangulation (dplane_t *plane)
{
	triangulation_t	*t;

	t = Malloc(sizeof(triangulation_t));
	t->numpoints = 0;
	t->numedges = 0;
	t->numtris = 0;

	t->plane = plane;

	return t;
}

/*
===============
FreeTriangulation
===============
*/
void FreeTriangulation (triangulation_t *tr)
{
	Mem_Free (tr);
}


triedge_t	*FindEdge (triangulation_t *trian, int p0, int p1)
{
	triedge_t	*e, *be;
	vec3_t		v1;
	vec3_t		normal;
	vec_t		dist;

	if (trian->edgematrix[p0][p1])
		return trian->edgematrix[p0][p1];

	if (trian->numedges > MAX_TRI_EDGES-2)
		Sys_Error ("trian->numedges > MAX_TRI_EDGES-2");

	VectorSubtract (trian->points[p1]->origin, trian->points[p0]->origin, v1);
	VectorNormalize (v1);
	CrossProduct (v1, trian->plane->normal, normal);
	dist = DotProduct (trian->points[p0]->origin, normal);

	e = &trian->edges[trian->numedges];
	e->p0 = p0;
	e->p1 = p1;
	e->tri = NULL;
	VectorCopy (normal, e->normal);
	e->dist = dist;
	trian->numedges++;
	trian->edgematrix[p0][p1] = e;

	be = &trian->edges[trian->numedges];
	be->p0 = p1;
	be->p1 = p0;
	be->tri = NULL;
	VectorSubtract (vec3_origin, normal, be->normal);
	be->dist = -dist;
	trian->numedges++;
	trian->edgematrix[p1][p0] = be;

	return e;
}

triangle_t	*AllocTriangle (triangulation_t *trian)
{
	triangle_t	*t;

	if (trian->numtris >= MAX_TRI_TRIS)
		Sys_Error ("trian->numtris >= MAX_TRI_TRIS");

	t = &trian->tris[trian->numtris];
	trian->numtris++;

	return t;
}

/*
============
TriEdge_r
============
*/
void TriEdge_r (triangulation_t *trian, triedge_t *e)
{
	int		i, bestp;
	vec3_t	v1, v2;
	vec_t	*p0, *p1, *p;
	vec_t	best, ang;
	triangle_t	*nt;

	if (e->tri)
		return;		// allready connected by someone

	// find the point with the best angle
	p0 = trian->points[e->p0]->origin;
	p1 = trian->points[e->p1]->origin;
	best = 1.1f;
	for (i=0 ; i< trian->numpoints ; i++)
	{
		p = trian->points[i]->origin;
		// a 0 dist will form a degenerate triangle
		if (DotProduct(p, e->normal) - e->dist < 0)
			continue;	// behind edge
		VectorSubtract (p0, p, v1);
		VectorSubtract (p1, p, v2);
		if( !VectorNormalizeLength( v1 )) continue;
		if( !VectorNormalizeLength( v2 )) continue;

		ang = DotProduct( v1, v2 );
		if( ang < best )
		{
			best = ang;
			bestp = i;
		}
	}
	if( best >= 1 ) return; // edge doesn't match anything

	// make a new triangle
	nt = AllocTriangle (trian);
	nt->edges[0] = e;
	nt->edges[1] = FindEdge (trian, e->p1, bestp);
	nt->edges[2] = FindEdge (trian, bestp, e->p0);
	for (i=0 ; i<3 ; i++)
		nt->edges[i]->tri = nt;
	TriEdge_r (trian, FindEdge (trian, bestp, e->p1));
	TriEdge_r (trian, FindEdge (trian, e->p0, bestp));
}

/*
============
TriangulatePoints
============
*/
void TriangulatePoints (triangulation_t *trian)
{
	vec_t	d, bestd;
	vec3_t	v1;
	int		bp1, bp2, i, j;
	vec_t	*p1, *p2;
	triedge_t	*e, *e2;

	if (trian->numpoints < 2)
		return;

	// find the two closest points
	bestd = 9999;
	for (i=0 ; i<trian->numpoints ; i++)
	{
		p1 = trian->points[i]->origin;
		for (j=i+1 ; j<trian->numpoints ; j++)
		{
			p2 = trian->points[j]->origin;
			VectorSubtract (p2, p1, v1);
			d = VectorLength (v1);
			if (d < bestd)
			{
				bestd = d;
				bp1 = i;
				bp2 = j;
			}
		}
	}

	e = FindEdge (trian, bp1, bp2);
	e2 = FindEdge (trian, bp2, bp1);
	TriEdge_r (trian, e);
	TriEdge_r (trian, e2);
}

/*
===============
AddPointToTriangulation
===============
*/
void AddPointToTriangulation (patch_t *patch, triangulation_t *trian)
{
	int			pnum;

	pnum = trian->numpoints;
	if (pnum == MAX_TRI_POINTS)
		Sys_Error ("trian->numpoints == MAX_TRI_POINTS");
	trian->points[pnum] = patch;
	trian->numpoints++;
}

/*
===============
LerpTriangle
===============
*/
void	LerpTriangle (triangulation_t *trian, triangle_t *t, vec3_t point, vec3_t color)
{
	patch_t		*p1, *p2, *p3;
	vec3_t		base, d1, d2;
	float		x, y, x1, y1, x2, y2;

	p1 = trian->points[t->edges[0]->p0];
	p2 = trian->points[t->edges[1]->p0];
	p3 = trian->points[t->edges[2]->p0];

	VectorCopy (p1->totallight, base);
	VectorSubtract (p2->totallight, base, d1);
	VectorSubtract (p3->totallight, base, d2);

	x = DotProduct (point, t->edges[0]->normal) - t->edges[0]->dist;
	y = DotProduct (point, t->edges[2]->normal) - t->edges[2]->dist;

	x1 = 0;
	y1 = DotProduct (p2->origin, t->edges[2]->normal) - t->edges[2]->dist;

	x2 = DotProduct (p3->origin, t->edges[0]->normal) - t->edges[0]->dist;
	y2 = 0;

	if (fabs(y1)<ON_EPSILON || fabs(x2)<ON_EPSILON)
	{
		VectorCopy (base, color);
		return;
	}

	VectorMA (base, x/x2, d2, color);
	VectorMA (color, y/y1, d1, color);
}

bool PointInTriangle (vec3_t point, triangle_t *t)
{
	int		i;
	triedge_t	*e;
	vec_t	d;

	for (i=0 ; i<3 ; i++)
	{
		e = t->edges[i];
		d = DotProduct (e->normal, point) - e->dist;
		if (d < 0)
			return false;	// not inside
	}

	return true;
}

/*
===============
SampleTriangulation
===============
*/
void SampleTriangulation (vec3_t point, triangulation_t *trian, vec3_t color)
{
	triangle_t	*t;
	triedge_t	*e;
	vec_t		d, best;
	patch_t		*p0, *p1;
	vec3_t		v1, v2;
	int			i, j;

	if (trian->numpoints == 0)
	{
		VectorClear (color);
		return;
	}
	if (trian->numpoints == 1)
	{
		VectorCopy (trian->points[0]->totallight, color);
		return;
	}

	// search for triangles
	for (t = trian->tris, j=0 ; j < trian->numtris ; t++, j++)
	{
		if (!PointInTriangle (point, t))
			continue;

		// this is it
		LerpTriangle (trian, t, point, color);
		return;
	}
	
	// search for exterior edge
	for (e=trian->edges, j=0 ; j< trian->numedges ; e++, j++)
	{
		if (e->tri)
			continue;		// not an exterior edge

		d = DotProduct (point, e->normal) - e->dist;
		if (d < 0)
			continue;	// not in front of edge

		p0 = trian->points[e->p0];
		p1 = trian->points[e->p1];
	
		VectorSubtract (p1->origin, p0->origin, v1);
		VectorNormalize (v1);
		VectorSubtract (point, p0->origin, v2);
		d = DotProduct (v2, v1);
		if (d < 0)
			continue;
		if (d > 1)
			continue;
		for (i=0 ; i<3 ; i++)
			color[i] = p0->totallight[i] + d * (p1->totallight[i] - p0->totallight[i]);
		return;
	}

	// search for nearest point
	best = 99999;
	p1 = NULL;
	for (j=0 ; j<trian->numpoints ; j++)
	{
		p0 = trian->points[j];
		VectorSubtract (point, p0->origin, v1);
		d = VectorLength (v1);
		if (d < best)
		{
			best = d;
			p1 = p0;
		}
	}

	if (!p1)
		Sys_Error ("SampleTriangulation: no points");

	VectorCopy (p1->totallight, color);
}

/*
=================================================================

  LIGHTMAP SAMPLE GENERATION

=================================================================
*/


#define SINGLEMAP	(128*128*4)

typedef struct
{
	vec_t	facedist;
	vec3_t	facenormal;

	int		numsurfpt;
	vec3_t	surfpt[SINGLEMAP];

	vec3_t	modelorg;		// for origined bmodels

	vec3_t	texorg;
	vec3_t	worldtotex[2];	// s = (world - texorg) . worldtotex[0]
	vec3_t	textoworld[2];	// world = texorg + s * textoworld[0]

	vec_t	exactmins[2], exactmaxs[2];
	
	int		texmins[2], texsize[2];
	int		surfnum;
	dsurface_t	*face;
} lightinfo_t;


/*
================
CalcFaceExtents

Fills in s->texmins[] and s->texsize[]
also sets exactmins[] and exactmaxs[]
================
*/
void CalcFaceExtents (lightinfo_t *l)
{
	dsurface_t *s;
	vec_t	mins[2], maxs[2], val;
	int		i,j, e;
	dvertex_t	*v;
	dtexinfo_t	*tex;
	vec3_t		vt;

	s = l->face;

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = &texinfo[s->texinfo];
	
	for (i=0 ; i<s->numedges ; i++)
	{
		e = dsurfedges[s->firstedge+i];
		if (e >= 0)
			v = dvertexes + dedges[e].v[0];
		else
			v = dvertexes + dedges[-e].v[1];
		
//		VectorAdd (v->point, l->modelorg, vt);
		VectorCopy (v->point, vt);

		for (j=0 ; j<2 ; j++)
		{
			val = DotProduct (vt, tex->vecs[j]) + tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{	
		l->exactmins[i] = mins[i];
		l->exactmaxs[i] = maxs[i];
		
		mins[i] = floor(mins[i] / LM_SAMPLE_SIZE );
		maxs[i] = ceil( maxs[i] / LM_SAMPLE_SIZE );

		l->texmins[i] = mins[i];
		l->texsize[i] = maxs[i] - mins[i];
		if (l->texsize[0] * l->texsize[1] > SINGLEMAP/4)	// div 4 for extrasamples
			Sys_Error ("Surface to large to map");
	}
}

/*
================
CalcFaceVectors

Fills in texorg, worldtotex. and textoworld
================
*/
void CalcFaceVectors (lightinfo_t *l)
{
	dtexinfo_t	*tex;
	int	i, j;
	vec3_t	texnormal;
	vec_t	distscale;
	vec_t	dist, len;
	int	w, h;

	tex = &texinfo[l->face->texinfo];
	
	// convert from float to double
	for (i=0 ; i<2 ; i++)
		for (j=0 ; j<3 ; j++)
			l->worldtotex[i][j] = tex->vecs[i][j];

	// calculate a normal to the texture axis.  points can be moved along this
	// without changing their S/T
	texnormal[0] = tex->vecs[1][1]*tex->vecs[0][2] - tex->vecs[1][2]*tex->vecs[0][1];
	texnormal[1] = tex->vecs[1][2]*tex->vecs[0][0] - tex->vecs[1][0]*tex->vecs[0][2];
	texnormal[2] = tex->vecs[1][0]*tex->vecs[0][1] - tex->vecs[1][1]*tex->vecs[0][0];
	VectorNormalize (texnormal);

	// flip it towards plane normal
	distscale = DotProduct (texnormal, l->facenormal);
	if (!distscale)
	{
		Msg("WARNING: Texture axis perpendicular to face\n");
		distscale = 1;
	}
	if (distscale < 0)
	{
		distscale = -distscale;
		VectorSubtract (vec3_origin, texnormal, texnormal);
	}	

// distscale is the ratio of the distance along the texture normal to
// the distance along the plane normal
	distscale = 1/distscale;

	for (i=0 ; i<2 ; i++)
	{
		len = VectorLength (l->worldtotex[i]);
		dist = DotProduct (l->worldtotex[i], l->facenormal);
		dist *= distscale;
		VectorMA (l->worldtotex[i], -dist, texnormal, l->textoworld[i]);
		VectorScale (l->textoworld[i], (1/len)*(1/len), l->textoworld[i]);
	}


// calculate texorg on the texture plane
	for (i=0 ; i<3 ; i++)
		l->texorg[i] = -tex->vecs[0][3]* l->textoworld[0][i] - tex->vecs[1][3] * l->textoworld[1][i];

// project back to the face plane
	dist = DotProduct (l->texorg, l->facenormal) - l->facedist - 1;
	dist *= distscale;
	VectorMA (l->texorg, -dist, texnormal, l->texorg);

	// compensate for org'd bmodels
	VectorAdd (l->texorg, l->modelorg, l->texorg);

	// total sample count
	h = l->texsize[1]+1;
	w = l->texsize[0]+1;
	l->numsurfpt = w * h;
}

/*
=================
CalcPoints

For each texture aligned grid point, back project onto the plane
to get the world xyz value of the sample point
=================
*/
void CalcPoints (lightinfo_t *l, float sofs, float tofs)
{
	int		i;
	int		s, t, j;
	int		w, h, step;
	vec_t	starts, startt, us, ut;
	vec_t	*surf;
	vec_t	mids, midt;
	vec3_t	facemid;
	dleaf_t	*leaf;

	surf = l->surfpt[0];
	mids = (l->exactmaxs[0] + l->exactmins[0])/2;
	midt = (l->exactmaxs[1] + l->exactmins[1])/2;

	for (j=0 ; j<3 ; j++)
		facemid[j] = l->texorg[j] + l->textoworld[0][j]*mids + l->textoworld[1][j]*midt;

	h = l->texsize[1]+1;
	w = l->texsize[0]+1;
	l->numsurfpt = w * h;

	starts = l->texmins[0] * LM_SAMPLE_SIZE;
	startt = l->texmins[1] * LM_SAMPLE_SIZE;
	step = LM_SAMPLE_SIZE;


	for (t=0 ; t<h ; t++)
	{
		for (s=0 ; s<w ; s++, surf+=3)
		{
			us = starts + (s+sofs)*step;
			ut = startt + (t+tofs)*step;


		// if a line can be traced from surf to facemid, the point is good
			for (i=0 ; i<6 ; i++)
			{
			// calculate texture point
				for (j=0 ; j<3 ; j++)
					surf[j] = l->texorg[j] + l->textoworld[0][j]*us
					+ l->textoworld[1][j]*ut;

				leaf = PointInLeaf (surf);
				if (leaf->contents != CONTENTS_SOLID)
				{
					if (!(TestLine_r( 0, facemid, surf ) & CONTENTS_SOLID))
						break;	// got it
				}

				// nudge it
				if (i & 1)
				{
					if (us > mids)
					{
						us -= LM_SAMPLE_SIZE >> 1;
						if (us < mids)
							us = mids;
					}
					else
					{
						us += LM_SAMPLE_SIZE >> 1;
						if (us > mids)
							us = mids;
					}
				}
				else
				{
					if (ut > midt)
					{
						ut -= LM_SAMPLE_SIZE >> 1;
						if (ut < midt)
							ut = midt;
					}
					else
					{
						ut += LM_SAMPLE_SIZE >> 1;
						if (ut > midt)
							ut = midt;
					}
				}
			}
		}
	}
	
}


//==============================================================



#define	MAX_STYLES	32
typedef struct
{
	int	numsamples;
	float	*origins;
	int	numstyles;
	int	stylenums[MAX_STYLES];
	float	*samples[MAX_STYLES];
} facelight_t;

directlight_t	*directlights[MAX_MAP_LEAFS];
facelight_t	facelight[MAX_MAP_SURFACES];
int		numdlights;

#define	DIRECT_LIGHT	3
#define	LIGHT_SCALE	10
#define	LIGHT_FACTOR	10

/*
=============
CreateDirectLights
=============
*/
void CreateDirectLights( void )
{
	patch_t		*p;
	directlight_t	*dl;
	vec3_t		dest;
	dleaf_t		*leaf;
	float		angle;
	int		i, cluster;
	bsp_entity_t	*e, *e2;
	char		*name, *target;

	//
	// surfaces
	//
	if( bsp_parms & BSPLIB_MAKEQ2RAD )
	{
		for( i = 0, p = patches; i < num_patches; i++, p++ )
		{
			if( p->totallight[0] < DIRECT_LIGHT && p->totallight[1] < DIRECT_LIGHT
			&& p->totallight[2] < DIRECT_LIGHT )
				continue;

			dl = Malloc( sizeof( directlight_t ));
			numdlights++;

			VectorCopy( p->origin, dl->origin );
			leaf = PointInLeaf( dl->origin );
			cluster = leaf->cluster;
			dl->next = directlights[cluster];
			directlights[cluster] = dl;

			dl->type = emit_surface;
			VectorCopy( p->plane->normal, dl->normal );

			dl->lightscale = ColorNormalize( p->totallight, dl->color );
			dl->lightscale *= p->area * direct_scale;
			VectorClear( p->totallight );	// all sent now
		}
	}
	else if( bsp_parms & BSPLIB_MAKEHLRAD )
	{	
		for( i = 0, p = patches; i < num_patches; i++, p++ )
		{
			if( VectorAvg(p->totallight) < 25.0f )
				continue;

			dl = Malloc( sizeof( directlight_t ));
			numdlights++;

			VectorCopy( p->origin, dl->origin );
			leaf = PointInLeaf( dl->origin );
			cluster = leaf->cluster;
			dl->next = directlights[cluster];
			directlights[cluster] = dl;

			dl->type = emit_surface;
			VectorCopy( p->plane->normal, dl->normal );

			VectorCopy( p->plane->normal, dl->normal );
			VectorCopy( p->totallight, dl->intensity );
			VectorScale( dl->intensity, p->area, dl->intensity );
			VectorScale( dl->intensity, direct_scale, dl->intensity );
			VectorClear( p->totallight );	// all sent now
		}
	}

	//
	// entities
	//
	for( i = 0; i < num_entities; i++ )
	{
		char	*value;
		int	argCnt;
		double	vec[4];
		double	col[3];
		float	intensity;
		bool	monolight = false;
			
		e = &entities[i];
		name = ValueForKey( e, "classname" );
		if( com.strncmp( name, "light", 5 ))
			continue;

		dl = Malloc( sizeof( directlight_t ));
		numdlights++;

		GetVectorForKey( e, "origin", dl->origin );
		dl->style = FloatForKey( e, "_style" );
		if( !dl->style ) dl->style = FloatForKey( e, "style" );
		if( dl->style < 0 || dl->style >= MAX_LSTYLES )
			dl->style = 0;

		leaf = PointInLeaf( dl->origin );
		cluster = leaf->cluster;

		dl->next = directlights[cluster];
		directlights[cluster] = dl;

		value = ValueForKey( e, "light" );
		if( !value[0] ) value = ValueForKey( e, "_light" );

		if( bsp_parms & BSPLIB_MAKEQ2RAD )
		{
			// assume default light color
			VectorSet( dl->color, 1.0f, 1.0f, 1.0f );
			intensity = 0.0f;
		}

		if( value[0] )
		{
			argCnt = sscanf( value, "%lf %lf %lf %lf", &vec[0], &vec[1], &vec[2], &vec[3] );
			switch( argCnt )
			{
			case 4:	// HalfLife light
				VectorSet( dl->color, vec[0], vec[1], vec[2] );
				VectorDivide( dl->color, 255.0f, dl->color );
				if( bsp_parms & BSPLIB_MAKEHLRAD )
					VectorScale( dl->intensity, (float)vec[3], dl->intensity );
				intensity = vec[3];
				break;
			case 3:	// Half-Life light_environment
				VectorSet( dl->intensity, vec[0], vec[1], vec[2] );
				if( bsp_parms & BSPLIB_MAKEQ2RAD )
					VectorDivide( dl->color, 255.0f, dl->color );
				break;
			case 1:	// Quake light
				if( bsp_parms & BSPLIB_MAKEHLRAD )
					VectorSet( dl->intensity, vec[0], vec[0], vec[0] );
				intensity = vec[0];
				monolight = true;
				break;
			default:
				MsgDev( D_WARN, "%s [%i]: '_light' key must be 1 (q1) or 3 or 4 (hl) numbers\n", name, i );
				break;
			}
		}

		if( monolight )
		{
			value = ValueForKey( e, "color" );
			if( !value[0] ) value = ValueForKey( e, "_color" );
			if( value[0] )
			{
				argCnt = sscanf( value, "%lf %lf %lf", &col[0], &col[1], &col[2] );
				if( argCnt != 3 )
				{
					MsgDev( D_WARN, "light at %.0f %.0f %.0f:\ncolor must be given 3 values\n",
					dl->origin[0], dl->origin[1], dl->origin[2] );
				}
				else if( bsp_parms & BSPLIB_MAKEHLRAD )
					VectorScale( col, vec[0], dl->intensity ); // already in range 0-1
				else if( bsp_parms & BSPLIB_MAKEQ2RAD )
					VectorCopy( col, dl->color );
			}
		}
  
		target = ValueForKey (e, "target");

		if( !com.strcmp( name, "light_spot" ) || !com.strcmp( name, "light_environment" ) || target[0] )
		{
			if( bsp_parms & BSPLIB_MAKEHLRAD && !VectorAvg( dl->intensity ))
				VectorSet( dl->intensity, 500.0f, 500.0f, 500.0f );
			else if( bsp_parms & BSPLIB_MAKEQ2RAD && !intensity )
				intensity = 500.0f;

			dl->type = emit_spotlight;
			dl->stopdot = FloatForKey( e, "_cone" );
			if( !dl->stopdot ) dl->stopdot = 10;

			if( bsp_parms & BSPLIB_MAKEQ2RAD && !com.strcmp( name, "light_environment" ))
				dl->stopdot = 90;

			dl->stopdot2 = FloatForKey( e, "_cone2" );
			if( !dl->stopdot2 )  dl->stopdot2 = dl->stopdot;

			dl->stopdot = cos( dl->stopdot / 180 * M_PI );
			dl->stopdot2 = cos( dl->stopdot2 / 180 * M_PI );

			if( target[0] )
			{	
				// point towards target
				e2 = FindTargetEntity( target );
				if( !e2 )
				{
					MsgDev( D_WARN, "light at (%i %i %i) has missing target\n",
					(int)dl->origin[0], (int)dl->origin[1], (int)dl->origin[2]);
				}
				else
				{
					GetVectorForKey( e2, "origin", dest );
					VectorSubtract( dest, dl->origin, dl->normal );
					VectorNormalize( dl->normal );
				}
			}
			else
			{	// point down angle
				vec3_t	light_angles;
				GetVectorForKey( e, "angles", light_angles );

				angle = FloatForKey( e, "angle" );
				if( angle == ANGLE_UP )
				{
					VectorSet( dl->normal, 0.0f, 0.0f, 1.0f );
				}
				else if( angle == ANGLE_DOWN )
				{
					VectorSet( dl->normal, 0.0f, 0.0f,-1.0f );
				}
				else
				{
					// if we don't have a specific "angle" use the "angles" YAW
					if( !angle ) angle = light_angles[1];

					dl->normal[2] = 0;
					dl->normal[0] = com.cos( angle / 180 * M_PI );
					dl->normal[1] = com.sin( angle / 180 * M_PI );
				}

				angle = FloatForKey( e, "pitch" );

				// if we don't have a specific "pitch" use the "angles" PITCH
				if( !angle ) angle = light_angles[0];	// don't forget about "Stupid Quake Bug"
				dl->normal[2]  = com.sin( angle / 180 * M_PI );
				dl->normal[0] *= com.cos( angle / 180 * M_PI );
				dl->normal[1] *= com.cos( angle / 180 * M_PI );
			}

			// qlight doesn't have a surface light environment - using spotlight instead
			if( FloatForKey( e, "_sky" ) || !com.strcmp( name, "light_environment" )) 
			{
				dl->stopdot2 = FloatForKey( e, "_sky" ); // hack stopdot2 to a sky key number

				if( bsp_parms & BSPLIB_MAKEHLRAD )
					dl->type = emit_skylight;
				else dl->lightscale = 8000.0f;	// default sun level
			} 
		}
		else
		{
			if( bsp_parms & BSPLIB_MAKEHLRAD && !VectorAvg( dl->intensity ))
				VectorSet( dl->intensity, 300.0f, 300.0f, 300.0f );
			else if( bsp_parms & BSPLIB_MAKEQ2RAD && !intensity )
				intensity = 300.0f;
			dl->type = emit_point;
		}

		if( dl->type != emit_skylight )
		{
			if( bsp_parms & BSPLIB_MAKEHLRAD )
			{
				float	l1 = VectorMax( dl->intensity );
				l1 = (l1 * l1) / LIGHT_SCALE;
				VectorScale( dl->intensity, l1, dl->intensity );
			}
			else
			{
				ColorNormalize( dl->color, dl->color );
				dl->lightscale = intensity * entity_scale;
                    	}
		}

		if( bsp_parms & BSPLIB_MAKEHLRAD )
			Msg("intensity ( %g %g %g)\n", dl->intensity[0], dl->intensity[1], dl->intensity[2] );
		else Msg( "color( %g %g %g)( %g)\n", dl->color[0], dl->color[1], dl->color[2], dl->lightscale );
	}
	Msg( "%i direct lights\n", numdlights );
}

/*
=============
GatherSampleLight

Lightscale is the normalizer for multisampling
=============
*/
void GatherSampleLight( vec3_t pos, vec3_t normal, float **styletable, int offset, int mapsize, float lightscale )
{
	int		i;
	vec3_t		delta;
	float		dot, dot2;
	float		dist;
	float		scale;
	float		*dest;
	directlight_t	*l, *sky_used = NULL;

	// get the PVS for the pos to limit the number of checks
	Mem_Set( pvs, 0x00, sizeof( pvs ));
	if( !PvsForOrigin( pos, pvs )) return;

	for( i = 0; i < dvis->numclusters; i++ )
	{
		if(!( pvs[i>>3] & (1<<(i & 7))))
			continue;

		for( l = directlights[i]; l; l = l->next )
		{
			VectorSubtract( l->origin, pos, delta );
			dist = VectorNormalizeLength( delta );
			dot = DotProduct( delta, normal );
			if( dot <= 0.001 ) continue; // behind sample surface

			switch( l->type )
			{
			case emit_point:
				// linear falloff
				scale = (l->lightscale - dist) * dot;
				break;
			case emit_surface:
				dot2 = -DotProduct (delta, l->normal);
				if( dot2 <= 0.001 ) goto skipadd;	// behind light surface
				scale = (l->lightscale / (dist*dist)) * (dot * dot2);
				break;
			case emit_spotlight:
				// linear falloff
				dot2 = -DotProduct( delta, l->normal );
				if (dot2 <= l->stopdot)
					goto skipadd;	// outside light cone
				scale = (l->lightscale - dist) * dot;
				break;
			default:
				Sys_Error( "Bad l->type\n" );
			}

			if( scale <= 0 ) continue;
			if( TestLine_r( 0, pos, l->origin ) & CONTENTS_SOLID )
				continue;	// occluded

			// if this style doesn't have a table yet, allocate one
			if( !styletable[l->style] )
				styletable[l->style] = Malloc (mapsize);

			dest = styletable[l->style] + offset;			
			// add some light to it
			VectorMA( dest, scale * lightscale, l->color, dest );
skipadd:;
		}
	}
}

/*
=============
GatherSampleRad

Lightscale is the normalizer for multisampling
=============
*/
void GatherSampleRad( vec3_t pos, vec3_t normal, float **styletable, int offset, int mapsize, float lightscale )
{
	int		i;
	vec3_t		delta, add;
	float		dot, dot2;
	float		dist;
	float		ratio;
	float		*dest;
	directlight_t	*l, *sky_used = NULL;

	// get the PVS for the pos to limit the number of checks
	Mem_Set( pvs, 0x00, sizeof( pvs ));
	if( !PvsForOrigin( pos, pvs )) return;

	for( i = 0; i < dvis->numclusters; i++ )
	{
		if(!( pvs[i>>3] & (1<<(i & 7))))
			continue;

		for( l = directlights[i]; l; l = l->next )
		{
			// skylights work fundamentally differently than normal lights
			if( l->type == emit_skylight )
			{
				// only allow one of each sky type to hit any given point
				if( sky_used ) continue;
				sky_used = l;

				// make sure the angle is okay
				dot = -DotProduct( normal, l->normal );
				if( dot <= ON_EPSILON / LIGHT_FACTOR ) continue;

				// search back to see if we can hit a sky brush
				VectorScale( l->normal, -10000, delta );
				VectorAdd( pos, delta, delta );
				if(!(TestLine_r( 0, pos, delta ) & CONTENTS_SKY))
					continue;	// occluded
				VectorScale( l->intensity, dot, add );
			}
			else
			{
				VectorSubtract( l->origin, pos, delta );
				dist = VectorNormalizeLength( delta );
				dot = DotProduct( delta, normal );
				if( dot <= ON_EPSILON / LIGHT_FACTOR ) continue; // behind sample surface

				switch( l->type )
				{
				case emit_point:
					ratio = dot / (dist * dist);
					VectorScale( l->intensity, ratio * lightscale, add );
					break;
				case emit_surface:
					dot2 = -DotProduct( delta, l->normal );
					if( dot2 <= ON_EPSILON / LIGHT_FACTOR ) goto skipadd; // behind light surface
					ratio = dot * dot2 / (dist * dist);
					VectorScale( l->intensity, ratio * lightscale, add );
					break;
				case emit_spotlight:
					dot2 = -DotProduct( delta, l->normal );
					if( dot2 <= l->stopdot2 ) goto skipadd; // outside light cone
					ratio = dot * dot2 / (dist * dist);
					if( dot2 <= l->stopdot )
						ratio *= (dot2 - l->stopdot2) / (l->stopdot - l->stopdot2);
					VectorScale( l->intensity, ratio * lightscale, add );
					break;
				default:
					Sys_Error( "Bad l->type\n" );
				}

				if( VectorMax( add ) > ( l->style ? 1.0f : 0.0f ))
				{
					if( l->type != emit_skylight && TestLine_r( 0, pos, l->origin ) & CONTENTS_SOLID )
						continue;	// occluded

					// if this style doesn't have a table yet, allocate one
					if( !styletable[l->style] )
						styletable[l->style] = Malloc( mapsize );

					dest = styletable[l->style] + offset;	
					VectorAdd( dest, add, dest );
				}
			}
skipadd:;
		}
	}
	if( sky_used )
	{
		vec3_t	total;
		vec3_t	sky_intensity;
		int	j;

		VectorScale( sky_used->intensity, 1.0f/(NUMVERTEXNORMALS * 2), sky_intensity );

		VectorClear( total );
		for( j = 0; j < NUMVERTEXNORMALS; j++ )
		{
			// make sure the angle is okay
			dot = -DotProduct( normal, r_avertexnormals[j] );
			if( dot <= ON_EPSILON / LIGHT_FACTOR ) continue;

			// search back to see if we can hit a sky brush
			VectorScale( r_avertexnormals[j], -10000, delta );
			VectorAdd( pos, delta, delta );
			if(!(TestLine_r (0, pos, delta) & CONTENTS_SKY))
				continue;	// occluded
			
			VectorScale( sky_intensity, dot, add );
			VectorAdd( total, add, total );
		}
		if( VectorMax( total ) > 0 )
		{
			// if this style doesn't have a table yet, allocate one
			if( !styletable[sky_used->style] )
				styletable[sky_used->style] = Malloc( mapsize );

			dest = styletable[sky_used->style] + offset;	
			VectorAdd( dest, total, dest );
		}
	}
}

/*
=============
AddSampleToPatch

Take the sample's collected light and
add it back into the apropriate patch
for the radiosity pass.

The sample is added to all patches that might include
any part of it.  They are counted and averaged, so it
doesn't generate extra light.
=============
*/
void AddSampleToPatch( vec3_t pos, vec3_t color, int facenum )
{
	patch_t	*patch;
	vec3_t	mins, maxs;
	int	i;

	if( numbounce == 0 ) return;
	if( VectorAvg( color ) < 1.0f )
		return;

	for( patch = face_patches[facenum]; patch; patch = patch->next )
	{
		// see if the point is in this patch (roughly)
		WindingBounds( patch->winding, mins, maxs );
		for( i = 0; i < 3; i++ )
		{
			if( mins[i] > pos[i] + LM_SAMPLE_SIZE )
				goto nextpatch;
			if( maxs[i] < pos[i] - LM_SAMPLE_SIZE )
				goto nextpatch;
		}

		// add the sample to the patch
		patch->samples++;
		VectorAdd( patch->samplelight, color, patch->samplelight );
nextpatch:;
	}

}


/*
=============
BuildFacelights
=============
*/
float sampleofs[5][2] = 
{  {0,0}, {-0.25, -0.25}, {0.25, -0.25}, {0.25, 0.25}, {-0.25, 0.25} };

static lightinfo_t	light_info[5];

void BuildFacelights( int facenum )
{
	dsurface_t	*f;
	int		i, j;
	float		*styletable[MAX_LSTYLES];
	float		*spot;
	patch_t		*patch;
	int		numsamples;
	int		tablesize;
	facelight_t	*fl;
	
	f = &dsurfaces[facenum];

	if( dshaders[texinfo[f->texinfo].shadernum].surfaceFlags & (SURF_WARP|SURF_SKY))
		return; // non-lit texture

	Mem_Set( styletable, 0, sizeof( styletable ));

	if( bsp_parms & BSPLIB_FULLCOMPILE )
		numsamples = 5;
	else numsamples = 1;

	for( i = 0; i < numsamples; i++ )
	{
		Mem_Set( &light_info[i], 0, sizeof( light_info[i] ));
		light_info[i].surfnum = facenum;
		light_info[i].face = f;
		VectorCopy (dplanes[f->planenum].normal, light_info[i].facenormal);
		light_info[i].facedist = dplanes[f->planenum].dist;
		if( f->side )
		{
			VectorSubtract( vec3_origin, light_info[i].facenormal, light_info[i].facenormal );
			light_info[i].facedist = -light_info[i].facedist;
		}

		// get the origin offset for rotating bmodels
		VectorCopy( face_offset[facenum], light_info[i].modelorg );

		CalcFaceVectors( &light_info[i] );
		CalcFaceExtents( &light_info[i] );
		CalcPoints( &light_info[i], sampleofs[i][0], sampleofs[i][1] );
	}

	tablesize = light_info[0].numsurfpt * sizeof( vec3_t );
	styletable[0] = Malloc( tablesize );

	fl = &facelight[facenum];
	fl->numsamples = light_info[0].numsurfpt;
	fl->origins = Malloc( tablesize );
	Mem_Copy( fl->origins, light_info[0].surfpt, tablesize );

	for( i = 0; i < light_info[0].numsurfpt; i++ )
	{
		for( j = 0; j < numsamples; j++ )
		{
			if( bsp_parms & BSPLIB_MAKEHLRAD )
				GatherSampleRad( light_info[j].surfpt[i], light_info[0].facenormal, styletable, i*3, tablesize, 1.0f / numsamples );
			else GatherSampleLight( light_info[j].surfpt[i], light_info[0].facenormal, styletable, i*3, tablesize, 1.0f / numsamples );
		}

		// contribute the sample to one or more patches
		AddSampleToPatch( light_info[0].surfpt[i], styletable[0]+i*3, facenum );
	}

	// average up the direct light on each patch for radiosity
	for (patch = face_patches[facenum] ; patch ; patch=patch->next)
	{
		if (patch->samples)
		{
			VectorScale (patch->samplelight, 1.0/patch->samples, patch->samplelight);
		}
	}

	for (i=0 ; i<MAX_LSTYLES ; i++)
	{
		if (!styletable[i])
			continue;
		if (fl->numstyles == MAX_STYLES)
			break;
		fl->samples[fl->numstyles] = styletable[i];
		fl->stylenums[fl->numstyles] = i;
		fl->numstyles++;
	}

	// the light from DIRECT_LIGHTS is sent out, but the
	// texture itself should still be full bright

	if (face_patches[facenum]->baselight[0] >= DIRECT_LIGHT ||
		face_patches[facenum]->baselight[1] >= DIRECT_LIGHT ||
		face_patches[facenum]->baselight[2] >= DIRECT_LIGHT
		)
	{
		spot = fl->samples[0];
		for (i=0 ; i<light_info[0].numsurfpt ; i++, spot+=3)
		{
			VectorAdd (spot, face_patches[facenum]->baselight, spot);
		}
	}
}


/*
=============
FinalLightFace

Add the indirect lighting on top of the direct
lighting and save into final map format
=============
*/
void FinalLightFace (int facenum)
{
	dsurface_t		*f;
	int			i, j, k, st;
	vec3_t		lb;
	patch_t		*patch;
	triangulation_t	*trian;
	facelight_t	*fl;
	float		minlight;
	float		max, newmax;
	byte		*dest;
	int			pfacenum;
	vec3_t		facemins, facemaxs;

	f = &dsurfaces[facenum];
	fl = &facelight[facenum];

	if( dshaders[texinfo[f->texinfo].shadernum].surfaceFlags & ( SURF_WARP|SURF_SKY ))
		return;		// non-lit texture

	ThreadLock ();
	f->lightofs = lightdatasize;
	lightdatasize += fl->numstyles*(fl->numsamples*3);

// add green sentinals between lightmaps
#if 0
lightdatasize += 64*3;
for (i=0 ; i<64 ; i++)
dlightdata[lightdatasize-(i+1)*3 + 1] = 255;
#endif

	if (lightdatasize > MAX_MAP_LIGHTING)
		Sys_Error ("MAX_MAP_LIGHTING");
	ThreadUnlock ();

	f->styles[0] = 0;
	f->styles[1] = f->styles[2] = f->styles[3] = 0xff;

	//
	// set up the triangulation
	//
	if (numbounce > 0)
	{
		ClearBounds (facemins, facemaxs);
		for (i=0 ; i<f->numedges ; i++)
		{
			int		ednum;

			ednum = dsurfedges[f->firstedge+i];
			if (ednum >= 0)
				AddPointToBounds (dvertexes[dedges[ednum].v[0]].point,
				facemins, facemaxs);
			else
				AddPointToBounds (dvertexes[dedges[-ednum].v[1]].point,
				facemins, facemaxs);
		}

		trian = AllocTriangulation (&dplanes[f->planenum]);

		// for all faces on the plane, add the nearby patches
		// to the triangulation
		for (pfacenum = planelinks[f->side][f->planenum]
			; pfacenum ; pfacenum = facelinks[pfacenum])
		{
			for (patch = face_patches[pfacenum] ; patch ; patch=patch->next)
			{
				for (i=0 ; i < 3 ; i++)
				{
					if (facemins[i] - patch->origin[i] > subdiv*2)
						break;
					if (patch->origin[i] - facemaxs[i] > subdiv*2)
						break;
				}
				if (i != 3)
					continue;	// not needed for this face
				AddPointToTriangulation (patch, trian);
			}
		}
		for (i=0 ; i<trian->numpoints ; i++)
			memset (trian->edgematrix[i], 0, trian->numpoints*sizeof(trian->edgematrix[0][0]) );
		TriangulatePoints (trian);
	}
	
	//
	// sample the triangulation
	//

	// _minlight allows models that have faces that would not be
	// illuminated to receive a mottled light pattern instead of
	// black
	minlight = FloatForKey (face_entity[facenum], "_minlight") * 128;

	dest = &dlightdata[f->lightofs];

	if (fl->numstyles > LM_STYLES )
	{
		fl->numstyles = LM_STYLES;
		Msg( "face with too many lightstyles: (%f %f %f)\n", face_patches[facenum]->origin[0], face_patches[facenum]->origin[1], face_patches[facenum]->origin[2] );
	}

	for (st=0 ; st<fl->numstyles ; st++)
	{
		f->styles[st] = fl->stylenums[st];
		for (j=0 ; j<fl->numsamples ; j++)
		{
			VectorCopy ( (fl->samples[st]+j*3), lb);
			if (numbounce > 0 && st == 0)
			{
				vec3_t	add;

				SampleTriangulation (fl->origins + j*3, trian, add);
				VectorAdd (lb, add, lb);
			}
			// add an ambient term if desired
			lb[0] += ambient; 
			lb[1] += ambient; 
			lb[2] += ambient; 

			VectorScale (lb, lightscale, lb);

			// we need to clamp without allowing hue to change
			for (k=0 ; k<3 ; k++)
				if (lb[k] < 1)
					lb[k] = 1;
			max = lb[0];
			if (lb[1] > max)
				max = lb[1];
			if (lb[2] > max)
				max = lb[2];
			newmax = max;
			if (newmax < 0)
				newmax = 0;		// roundoff problems
			if (newmax < minlight)
			{
				newmax = minlight + (rand()%48);
			}
			if (newmax > maxlight)
				newmax = maxlight;

			for (k=0 ; k<3 ; k++)
			{
				*dest++ = lb[k]*newmax/max;
			}
		}
	}

	if (numbounce > 0)
		FreeTriangulation (trian);
}
