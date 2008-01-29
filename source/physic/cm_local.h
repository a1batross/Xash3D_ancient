//=======================================================================
//			Copyright XashXT Group 2007 ©
//			    cm_local.h - main struct
//=======================================================================
#ifndef CM_LOCAL_H
#define CM_LOCAL_H

#include "physic.h"
#include "basefiles.h"

#define CAPSULE_MODEL_HANDLE	MAX_MODELS - 2
#define BOX_MODEL_HANDLE	MAX_MODELS - 1

typedef struct
{
	cplane_t		*plane;
	int		children[2];	// negative numbers are leafs
} cnode_t;

typedef struct
{
	cplane_t		*plane;
	csurface_t	*surface;
} cbrushside_t;

typedef struct
{
	int		contents;
	int		numsides;
	vec3_t		bounds[2];
	int		firstbrushside;
	int		checkcount;	// to avoid repeated testings
} cbrush_t;

typedef struct
{
	int		numareaportals;
	int		firstareaportal;
	int		floodnum;		// if two areas have equal floodnums, they are connected
	int		floodvalid;
} carea_t;

typedef struct clipmap_s
{
	string		name;

	uint		checksum;		// map checksum
	byte		pvsrow[MAX_MAP_LEAFS/8];
	byte		phsrow[MAX_MAP_LEAFS/8];
	byte		portalopen[MAX_MAP_AREAPORTALS];

	// brush, studio and sprite models
	cmodel_t		cmodels[MAX_MODELS];
	cmodel_t		bmodels[MAX_MODELS];
	int		numcmodels;

	byte		*mod_base;	// start of buffer

	// shared copy of map (client - server)
	char		*entitystring;
	cplane_t		*planes;		// 12 extra planes for box hull
	cleaf_t		*leafs;		// 1 extra leaf for box hull
	dword		*leafbrushes;
	cnode_t		*nodes;		// 6 extra planes for box hull
	dvertex_t		*vertices;
	dedge_t		*edges;
	dface_t		*surfaces;
	int		*surfedges;
	csurface_t	*surfdesc;
	cbrush_t		*brushes;
	cbrushside_t	*brushsides;
	byte		*visibility;
	dvis_t		*vis;		// vis offset
	NewtonCollision	*collision;
	char		*stringdata;
	int		*stringtable;
	carea_t		*areas;
	dareaportal_t	*areaportals;

	csurface_t	nullsurface;
	int		numbrushsides;
	int		numtexinfo;
	int		numplanes;
	int		numbmodels;
	int		numnodes;
	int		numleafs;		// allow leaf funcs to be called without a map
	int		emptyleaf;
	int		solidleaf;
	int		numleafbrushes;
	int		numbrushes;
	int		numfaces;
	int		numareas;
	int		numareaportals;
	int		numclusters;
	int		floodvalid;
	int		num_models;

	// misc stuff
	NewtonBody	*body;
	bool		loaded;		// map is loaded?
	bool		tree_build;	// phys tree is created ?
	bool		use_thread;	// bsplib use thread
	vfile_t		*world_tree;	// pre-calcualated collision tree (worldmodel only)
	trace_t		trace;		// contains result of last trace
	int		checkcount;
} clipmap_t;

typedef struct physic_s
{
	bool	initialized;
	cmdraw_t	debug_line;
} physic_t;

typedef struct studio_s
{
	studiohdr_t	*hdr;
	mstudiomodel_t	*submodel;
	mstudiobodyparts_t	*bodypart;
	matrix3x4		rotmatrix;
	matrix3x4		bones[MAXSTUDIOBONES];
	vec3_t		vertices[MAXSTUDIOVERTS];
	vec3_t		transform[MAXSTUDIOVERTS];
	uint		bodycount;
} studio_t;

typedef struct convex_hull_s
{
	vec3_t		*m_pVerts; // pointer to studio.vertices array
	uint		numverts;
} convex_hull_t;

typedef struct box_s
{
	cplane_t		*planes;
	cbrush_t		*brush;
	cmodel_t		*model;
} box_t;

typedef struct mapleaf_s
{
	int		count;
	int		topnode;
	int		maxcount;
	int		*list;
	float		*mins;
	float		*maxs;
} mapleaf_t;

typedef struct leaflist_s
{
	int		count;
	int		maxcount;
	bool		overflowed;
	int		*list;
	vec3_t		bounds[2];
	int		lastleaf;		// for overflows where each leaf can't be stored individually
	void		(*storeleafs)( struct leaflist_s *ll, int nodenum );
} leaflist_t;

typedef struct sphere_s
{
	bool		use;
	float		radius;
	float		halfheight;
	vec3_t		offset;
} sphere_t;

typedef struct tracework_s
{
	vec3_t		start;
	vec3_t		end;
	vec3_t		mins;
	vec3_t		maxs;
	vec3_t		offsets[8];	// [signbits][x] = either size[0][x] or size[1][x]
	float		maxOffset;	// longest corner length from origin
	vec3_t		bounds[2];	// enclosing box of start and end surrounding by size
	vec3_t		extents;		// greatest of abs(size[0]) and abs(size[1])
	vec3_t		origin;		// origin of the model tracing through
	int		contents;		// trace contents
	bool		ispoint;		// optimized case
	trace_t		result;		// returned from trace call
	sphere_t		sphere;		// sphere for oriendted capsule collision
} tracework_t;

extern clipmap_t cm;
extern studio_t studio;
extern convex_hull_t hull;
extern box_t box;
extern mapleaf_t leaf;
extern tracework_t maptrace;
extern physic_t ph;

extern cvar_t *cm_noareas;

//
// cm_test.c
//
int CM_PointLeafnum_r( const vec3_t p, int num );
int CM_PointLeafnum( const vec3_t p );
void CM_StoreLeafs( leaflist_t *ll, int nodenum );
void CM_StoreBrushes( leaflist_t *ll, int nodenum );
void CM_BoxLeafnums_r( leaflist_t *ll, int nodenum );
int CM_BoxLeafnums( const vec3_t mins, const vec3_t maxs, int *list, int listsize, int *lastleaf );
int CM_BoxBrushes( const vec3_t mins, const vec3_t maxs, cbrush_t **list, int listsize );
cmodel_t *CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, bool capsule );


#endif//CM_LOCAL_H