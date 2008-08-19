//=======================================================================
//			Copyright XashXT Group 2007 ©
//			    cm_local.h - main struct
//=======================================================================
#ifndef CM_LOCAL_H
#define CM_LOCAL_H

#include "physic.h"
#include "mathlib.h"
#include "cm_utils.h"

#define MAX_MATERIALS	64
#define CAPSULE_MODEL_HANDLE	MAX_MODELS - 2
#define BOX_MODEL_HANDLE	MAX_MODELS - 1

typedef struct cpointf_s
{
	float v[3];
} cpointf_t;

typedef struct cplanef_s
{
	csurface_t	*surface;
	int		surfaceflags;	// it's really needed?
	float		normal[3];
	float		dist;
} cplanef_t;

typedef struct cbrushf_s
{
	
	int	contents;		// the content flags of this brush
	int	numplanes;	// the number of bounding planes on this brush
	int	numpoints;	// the number of corner points on this brush
	int	numtriangles;	// the number of renderable triangles on this brush
	cplanef_t	*planes;		// array of bounding planes on this brush
	cpointf_t *points;		// array of corner points on this brush
	int	*elements;	// renderable triangles, as int[3] elements indexing the points
	int	markframe;	// used to avoid tracing against the same brush more than once
	vec3_t	mins;		// culling box
	vec3_t	maxs;
} cbrushf_t;

typedef struct cbspnode_s
{
	cplane_t		plane;
	struct cbspnode_s	*children[2];
	int		numcbrushf;
	int		maxcbrushf;
	cbrushf_t		**cbrushflist;
} cbspnode_t;

typedef struct cbsp_s
{
	byte		*mempool;
	cbspnode_t	*nodes;
} cbsp_t;

typedef struct cnode_s
{
	// this part shared between node and leaf
	struct cnode_s	*parent;
	cplane_t		*plane;		// always != NULL
	vec3_t		mins;
	vec3_t		maxs;
	int		contents;		// combined contents for node

	// this part unique to node
	struct cnode_s	*children[2];
} cnode_t;

typedef struct cleaf_s
{
	// this part shared between node and leaf
	struct cnode_s	*parent;
	cplane_t		*plane;		// always == NULL 
	vec3_t		mins;
	vec3_t		maxs;
	int		contents;		// combined contents for leaf

	// this part unique to leaf
	int		cluster;
	int		area;
	bool		havepatches;	// leaf have collision patches (triangles)
	int		*firstleafbrush;
	int		numleafbrushes;
	int 		numleafsurfaces;
	int		*firstleafsurface;
} cleaf_t;

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
	cbrushf_t		*colbrushf;	// float plane collision
} cbrush_t;

typedef struct
{
	int		numareaportals;
	int		firstareaportal;
	int		floodnum;		// if two areas have equal floodnums, they are connected
	int		floodvalid;
} carea_t;

typedef struct material_info_s
{
	string	name;
	float	softness;
	float	elasticity;
	float	friction_static;
	float	friction_kinetic;

} material_info_t;

typedef struct collide_info_s
{
	NewtonBody	*m_body0;
	NewtonBody	*m_body1;
	vec3_t		position;
	float		normal_speed;
	float		tangent_speed;
} collide_info_t;

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
	dword		*leafsurfaces;
	cnode_t		*nodes;		// 6 extra planes for box hull
	dvertex_t		*vertices;
	dedge_t		*edges;
	dface_t		*surfaces;
	int		*surfedges;
	csurface_t	*texinfo;
	cbrush_t		*brushes;
	cbrushside_t	*brushsides;
	byte		*visbase;		// vis offset
	dvis_t		*vis;
	NewtonCollision	*collision;
	dmiptex_t		*textures;
	char		*stringdata;
	int		*stringtable;
	carea_t		*areas;
	dareaportal_t	*areaportals;

	int		numbrushsides;
	int		numtexinfo;
	int		numplanes;
	int		numbmodels;
	int		numnodes;
	int		numleafs;		// allow leaf funcs to be called without a map
	int		numleafbrushes;
	int		numleafsurfaces;
	int		numtextures;
	int		numbrushes;
	int		numfaces;
	int		numareas;
	int		numareaportals;
	int		numclusters;
	int		floodvalid;

	// misc stuff
	NewtonBody	*body;
	matrix4x4		matrix;		// world matrix
	NewtonJoint	*upVector;	// world upvector
	material_info_t	mat[MAX_MATERIALS];
	uint		num_materials;	// number of parsed materials
	collide_info_t	touch_info;	// global info about two touching objects
	bool		loaded;		// map is loaded?
	bool		tree_build;	// phys tree is created ?
	vfile_t		*world_tree;	// pre-calcualated collision tree (worldmodel only)
	trace_t		trace;		// contains result of last trace
	int		checkcount;
} clipmap_t;

typedef struct physic_s
{
	bool	initialized;
	int	developer;

	cmdraw_t	debug_line;
} physic_t;

typedef struct studio_s
{
	studiohdr_t	*hdr;
	mstudiomodel_t	*submodel;
	mstudiobodyparts_t	*bodypart;
	matrix4x4		rotmatrix;
	matrix4x4		bones[MAXSTUDIOBONES];
	vec3_t		vertices[MAXSTUDIOVERTS];
	vec3_t		indices[MAXSTUDIOVERTS];
	vec3_t		vtransform[MAXSTUDIOVERTS];
	vec3_t		ntransform[MAXSTUDIOVERTS];
	vec3_t		*m_pVerts;		// pointer to studio.vertices array
	uint		numtriangles;
	uint		bodycount;
	uint		numverts;
} studio_t;

typedef struct leaflist_s
{
	int		count;
	int		maxcount;
	bool		overflowed;
	int		*list;
	vec3_t		bounds[2];
	int		lastleaf;		// for overflows where each leaf can't be stored individually
	void		(*storeleafs)( struct leaflist_s *ll, cnode_t *node );
} leaflist_t;

extern clipmap_t cm;
extern studio_t studio;
extern physic_t ph;

extern cvar_t *cm_noareas;
extern cvar_t *cm_debugdraw;
extern cvar_t *cm_novis;

// test variables
extern int	characterID; 
extern uint	m_jumpTimer;
extern bool	m_isStopped;
extern bool	m_isAirBorne;
extern float	m_maxStepHigh;
extern float	m_yawAngle;
extern float	m_maxTranslation;
extern vec3_t	m_size;
extern vec3_t	m_stepContact;
extern matrix4x4	m_matrix;
extern float	*m_upVector;

//
// cm_test.c
//
int CM_PointLeafnum_r( const vec3_t p, cnode_t *node );
int CM_PointLeafnum( const vec3_t p );
void CM_StoreLeafs( leaflist_t *ll, cnode_t *node );
void CM_StoreBrushes( leaflist_t *ll, cnode_t *node );
void CM_BoxLeafnums_r( leaflist_t *ll, cnode_t *node );
int CM_BoxLeafnums( const vec3_t mins, const vec3_t maxs, int *list, int listsize, int *lastleaf );
int CM_BoxBrushes( const vec3_t mins, const vec3_t maxs, cbrush_t **list, int listsize );
cmodel_t *CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, bool capsule );

//
// cm_callbacks.c
//
int Callback_ContactBegin( const NewtonMaterial* material, const NewtonBody* body0, const NewtonBody* body1 );
int Callback_ContactProcess( const NewtonMaterial* material, const NewtonContact* contact );
void Callback_ContactEnd( const NewtonMaterial* material );

//
// cm_materials.c
//
void CM_InitMaterials( void );

//
// cm_collision.c
//
void CM_CollisionCalcPlanesForPolygonBrushFloat( cbrushf_t *brush );
cbrushf_t *CM_CollisionAllocBrushFromPermanentPolygonFloat( byte *mempool, int numpoints, float *points, int supercontents );
cbrushf_t *CM_CollisionNewBrushFromPlanes( byte *mempool, int numoriginalplanes, const cplanef_t *originalplanes, int supercontents );
void CM_CollisionTraceBrushBrushFloat( trace_t *trace, const cbrushf_t *thisbrush_start, const cbrushf_t *thisbrush_end, const cbrushf_t *thatbrush_start, const cbrushf_t *thatbrush_end );
void CM_CollisionTraceBrushPolygonFloat( trace_t *trace, const cbrushf_t *thisbrush_start, const cbrushf_t *thisbrush_end, int numpoints, const float *points, int supercontents );
void CM_CollisionTraceBrushTriangleMeshFloat( trace_t *trace, const cbrushf_t *thisbrush_start, const cbrushf_t *thisbrush_end, int numtriangles, const int *element3i, const float *vertex3f, int supercontents, int q3surfaceflags, csurface_t *texture, const vec3_t segmentmins, const vec3_t segmentmaxs );
void CM_CollisionTraceLineBrushFloat( trace_t *trace, const vec3_t linestart, const vec3_t lineend, const cbrushf_t *thatbrush_start, const cbrushf_t *thatbrush_end );
void CM_CollisionTraceLinePolygonFloat( trace_t *trace, const vec3_t linestart, const vec3_t lineend, int numpoints, const float *points, int supercontents );
void CM_CollisionTraceLineTriangleMeshFloat( trace_t *trace, const vec3_t linestart, const vec3_t lineend, int numtriangles, const int *element3i, const float *vertex3f, int supercontents, int q3surfaceflags, csurface_t *texture, const vec3_t segmentmins, const vec3_t segmentmaxs );
void CM_CollisionTracePointBrushFloat( trace_t *trace, const vec3_t point, const cbrushf_t *thatbrush );
bool CM_CollisionPointInsideBrushFloat( const vec3_t point, const cbrushf_t *brush );
void CM_CollisionTraceBrushPolygonTransformFloat(trace_t *trace, const cbrushf_t *thisbrush_start, const cbrushf_t *thisbrush_end, int numpoints, const float *points, const matrix4x4 polygonmatrixstart, const matrix4x4 polygonmatrixend, int supercontents, int surfaceflags, csurface_t *texture );
cbrushf_t *CM_CollisionBrushForBox( const matrix4x4 matrix, const vec3_t mins, const vec3_t maxs, int supercontents, int q3surfaceflags, csurface_t *texture);
void CM_CollisionBoundingBoxOfBrushTraceSegment( const cbrushf_t *start, const cbrushf_t *end, vec3_t mins, vec3_t maxs, float startfrac, float endfrac );
float CM_CollisionClipTrace_LineSphere( double *linestart, double *lineend, double *sphereorigin, double sphereradius, double *impactpoint, double *impactnormal );
void CM_CollisionTraceLineTriangleFloat( trace_t *trace, const vec3_t linestart, const vec3_t lineend, const float *point0, const float *point1, const float *point2, int supercontents, int surfaceflags, csurface_t *texture );

// traces a box move against a single entity
// mins and maxs are relative
//
// if the entire move stays in a single solid brush, trace.allsolid will be set
//
// if the starting point is in a solid, it will be allowed to move out to an
// open area, and trace.startsolid will be set
//
// type is one of the MOVE_ values such as MOVE_NOMONSTERS which skips box
// entities, only colliding with SOLID_BSP entities (doors, lifts)
//
// passedict is excluded from clipping checks
void CM_CollisionClipToGenericEntity( trace_t *trace, cmodel_t *model, const vec3_t bodymins, const vec3_t bodymaxs, int bodysupercontents, matrix4x4 matrix, matrix4x4 inversematrix, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int hitcontentsmask );
// like above but does not do a transform and does nothing if model is NULL
void CM_CollisionClipToWorld( trace_t *trace, cmodel_t *model, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int hitsupercontents );
// combines data from two traces:
// merges contents flags, startsolid, allsolid, inwater
// updates fraction, endpos, plane and surface info if new fraction is shorter
void CM_CollisionCombineTraces( trace_t *cliptrace, const trace_t *trace, edict_t *touch, bool is_bmodel );
void CM_CollisionDrawForEachBrush( void );
void CM_CollisionInit( void );

#endif//CM_LOCAL_H