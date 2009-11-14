//=======================================================================
//			Copyright XashXT Group 2007 ©
//			    cm_local.h - main struct
//=======================================================================
#ifndef CM_LOCAL_H
#define CM_LOCAL_H

#include <stdio.h>
#include <windows.h>
#include "launch_api.h"
#include "engine_api.h"
#include "entity_def.h"
#include "physic_api.h"
#include "qfiles_ref.h"
#include "trace_def.h"

extern physic_imp_t		pi;
extern stdlib_api_t		com;

//
// local cvars
//
extern cvar_t		*cm_noareas;
extern cvar_t		*cm_nomeshes;
extern cvar_t		*cm_nocurves;
extern cvar_t		*cm_debugsize;

#define Host_Error		com.error
#define CAPSULE_MODEL_HANDLE	MAX_MODELS - 2
#define BOX_MODEL_HANDLE	MAX_MODELS - 1
#define MAX_FACET_BEVELS	(4 + 6 + 16)	// 3 or four + 6 axial bevels + 4 or 3 * 4 edge bevels
#define SHADER_MAX_VERTEXES	100000
#define SHADER_MAX_INDEXES	(SHADER_MAX_VERTEXES * 6)
#define SHADER_MAX_TRIANGLES	(SHADER_MAX_INDEXES / 3)
#define NORMAL_EPSILON	0.0001f
#define DIST_EPSILON	0.02f
#define MAX_FACETS		1024
#define MAX_PATCH_PLANES	4096
#define MAX_GRID_SIZE	129
#define MAX_POINTS_ON_WINDING	64
#define SUBDIVIDE_DISTANCE	16		// never more than this units away from curve
#define PLANE_TRI_EPSILON	0.1
#define WRAP_POINT_EPSILON	0.1

// 1/32 epsilon to keep floating point happy
#define SURFACE_CLIP_EPSILON	(0.03125)

typedef struct
{
	int		width;
	int		height;
	bool		wrapWidth;
	bool		wrapHeight;
	vec3_t		points[MAX_GRID_SIZE][MAX_GRID_SIZE];	// [width][height]
} cgrid_t;

typedef struct
{
	cplane_t       	*plane;
	int		children[2];	// negative numbers are leafs
} cnode_t;

typedef struct
{
	int		cluster;
	int		area;

	int		firstleafbrush;
	int		numleafbrushes;

	int		firstleafsurface;
	int		numleafsurfaces;
} cleaf_t;

typedef struct
{
	string		name;		// model name
	byte		*mempool;		// private mempool
	int		registration_sequence;

	// shared modelinfo
	modtype_t		type;		// model type
	vec3_t		mins, maxs;	// model boundbox
	byte		*extradata;	// studiomodels extradata
	int		numframes;	// sprite framecount

	cleaf_t		leaf;		// collision leaf
} cmodel_t;

typedef struct
{
	int		numpoints;
	vec3_t		p[4];
} cwinding_t;

typedef struct
{
	vec3_t		p0;
	vec3_t		p1;
} cbrushedge_t;

typedef struct
{
	int		planenum;
	int		shadernum;
	int		surfaceFlags;
	cwinding_t	*winding;
	cplane_t		*plane;
} cbrushside_t;

typedef struct
{
	int		shadernum;	// the shader that determined the contents
	int		contents;
	vec3_t		bounds[2];
	int		numsides;
	cbrushside_t	*sides;
	int		checkcount;	// to avoid repeated testings
	bool		collided;		// marker for optimisation
	cbrushedge_t	*edges;
	int		numedges;
} cbrush_t;

typedef struct cmplane_s
{
	float		plane[4];
	int		signbits;		// signx + (signy<<1) + (signz<<2), used as lookup during collision
	struct cmplane_s	*hashChain;
} cmplane_t;

// a facet is a subdivided element of a patch aproximation or model
typedef struct
{
	int		surfacePlane;
	int		numBorders;
	int		borderPlanes[MAX_FACET_BEVELS];
	int		borderInward[MAX_FACET_BEVELS];
	bool		borderNoAdjust[MAX_FACET_BEVELS];
} cfacet_t;

typedef struct
{
	int		numTriangles;
	int		indexes[SHADER_MAX_INDEXES];
	int		trianglePlanes[SHADER_MAX_TRIANGLES];
	vec3_t		points[SHADER_MAX_TRIANGLES][3];
} cTriangleSoup_t;

typedef struct
{
	vec3_t		bounds[2];
	int		numPlanes;	// surface planes plus edge planes
	cmplane_t		*planes;
	int		numFacets;
	cfacet_t		*facets;
} cSurfaceCollide_t;

typedef struct
{
	int		type;
	int		checkcount;	// to avoid repeated testings
	int		surfaceFlags;
	int		contents;
	const char	*name;		// ptr to texturename

	cSurfaceCollide_t	*sc;
} csurface_t;

typedef struct
{
	int		numareaportals;
	int		areaportals[MAX_MAP_AREAPORTALS];
	int		floodnum;		// if two areas have equal floodnums, they are connected
	int		floodvalid;
} carea_t;

typedef struct
{
	bool		open;
	int		area;
	int		otherarea;
} careaportal_t;

typedef struct
{
	float		startRadius;
	float		endRadius;
} biSphere_t;

// used for oriented capsule collision detection
typedef struct
{
	float		radius;
	float		halfheight;
	vec3_t		offset;
} sphere_t;

typedef struct
{
	trType_t		type;
	vec3_t		start;
	vec3_t		end;
	vec3_t		size[2];		// size of the box being swept through the model
	vec3_t		offsets[8];	// [signbits][x] = either size[0][x] or size[1][x]
	float		maxOffset;	// longest corner length from origin
	vec3_t		extents;		// greatest of abs(size[0]) and abs(size[1])
	vec3_t		bounds[2];	// enclosing box of start and end surrounding by size
	vec3_t		modelOrigin;	// origin of the model tracing through
	int		contents;		// ored contents of the model tracing through
	bool		isPoint;		// optimized case
	trace_t		trace;		// returned from trace call
	sphere_t		sphere;		// sphere for oriented capsule collision
	biSphere_t	biSphere;		// bi-sphere params
} traceWork_t;

typedef struct leaflist_s
{
	int		count;
	int		maxcount;
	bool		overflowed;
	int		*list;
	vec3_t		bounds[2];
	int		lastleaf;		// for overflows where each leaf can't be stored individually
} leaflist_t;

typedef struct clipmap_s
{
	string		name;
	uint		checksum;		// map checksum

	// shared copy of map (client - server)
	dshader_t		*shaders;
	int		numshaders;

	cbrushside_t	*brushsides;
	int		numbrushsides;

	cplane_t		*planes;		// 12 extra planes for box hull
	int		numplanes;

	cnode_t		*nodes;		// 6 extra planes for box hull
	int		numnodes;

	cleaf_t		*leafs;		// 1 extra leaf for box hull
	int		numleafs;		// allow leaf funcs to be called without a map

	dleafbrush_t	*leafbrushes;
	int		numleafbrushes;

	dleafface_t	*leafsurfaces;
	int		numleafsurfaces;

	cmodel_t		*models;
	int		nummodels;

	cbrush_t		*brushes;
	int		numbrushes;

	int		numclusters;
	int		clusterBytes;

	dvis_t		*pvs;
	dvis_t		*phs;
	size_t		visdata_size;	// if false, visibility is just a single cluster of ffs

	script_t		*entityscript;

	carea_t		*areas;
	int		numareas;

	careaportal_t	areaportals[MAX_MAP_AREAPORTALS];
	int		numareaportals;
	
	csurface_t	**surfaces;	// source collision data
	int		numsurfaces;

	int		floodvalid;

	int		numInvalidBevels;	// bevels failed
} clipmap_t;

typedef struct clipmap_static_s
{
	byte		*base;
	byte		*mempool;

	byte		nullrow[MAX_MAP_LEAFS/8];

	// brush, studio and sprite models
	cmodel_t		models[MAX_MODELS];
	int		nummodels;

	bool		loaded;		// map is loaded?
	int		checkcount;
	int		registration_sequence;
} clipmap_static_t;

extern clipmap_t		cm;
extern clipmap_static_t	cms;
extern cmodel_t		*loadmodel;


//
// cm_debug.c
//
void CM_DrawCollision( cmdraw_t callback );


//
// cm_test.c
//
extern const cSurfaceCollide_t	*debugSurfaceCollide;
extern const cfacet_t		*debugFacet;
extern vec3_t			debugBlockPoints[4];
extern bool			debugBlock;

int CM_LeafArea( int leafnum );
int CM_LeafCluster( int leafnum );
byte *CM_ClusterPVS( int cluster );
byte *CM_ClusterPHS( int cluster );
int CM_PointLeafnum( const vec3_t p );
bool CM_AreasConnected( int area, int otherarea );
int CM_BoxLeafnums( const vec3_t mins, const vec3_t maxs, int *list, int listsize, int *lastleaf );
model_t CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, bool capsule );
int CM_PointContents( const vec3_t p, model_t model );
int CM_TransformedPointContents( const vec3_t p, model_t model, const vec3_t origin, const vec3_t angles );
void CM_BoxLeafnums_r( leaflist_t *ll, int nodenum );

//
// cm_portals.c
//
void CM_CalcPHS( void );
byte *CM_FatPVS( const vec3_t org, bool portal );
byte *CM_FatPHS( int cluster, bool portal );
void CM_SetAreaPortals( byte *portals, size_t size );
void CM_GetAreaPortals( byte **portals, size_t *size );
void CM_SetAreaPortalState( int portalnum, int area, int otherarea, bool open );
int CM_WriteAreaBits( byte *buffer, int area, bool portal );
void CM_FloodAreaConnections( void );

//
// cm_model.c
//
extern cmodel_t			*sv_models[];

const void *CM_VisData( void );
int CM_NumClusters( void );
int CM_NumShaders( void );
void CM_FreeModels( void );
int CM_NumInlineModels( void );
script_t *CM_EntityScript( void );
const char *CM_ShaderName( int index );
void CM_ModelBounds( model_t handle, vec3_t mins, vec3_t maxs );
void CM_ModelFrames( model_t handle, int *numFrames );
modtype_t CM_ModelType( model_t handle );
cmodel_t *CM_ClipHandleToModel( model_t handle );
model_t CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, bool capsule );
void CM_BeginRegistration ( const char *name, bool clientload, uint *checksum );
bool CM_RegisterModel( const char *name, int sv_index );
void *CM_Extradata( model_t handle );
void CM_EndRegistration ( void );
void CM_FreeWorld( void );

//
// cm_studio.c
//
bool CM_SpriteModel( byte *buffer, size_t filesize );
bool CM_StudioModel( byte *buffer, size_t filesize );

//
// cm_polylib.c
//
cwinding_t *CM_AllocWinding( int points );
void CM_FreeWinding( cwinding_t *w );
cwinding_t *CM_CopyWinding( cwinding_t *w );
void CM_WindingBounds( cwinding_t *w, vec3_t mins, vec3_t maxs );
cwinding_t *CM_BaseWindingForPlane( vec3_t normal, float dist );
void CM_ChopWindingInPlace( cwinding_t **inout, vec3_t normal, float dist, float epsilon );

//
// cm_trace.c
//
void CM_BoxTrace( trace_t *tr, const vec3_t p1, const vec3_t p2, vec3_t mins, vec3_t maxs, model_t model, int mask, trType_t type );
void CM_TransformedBoxTrace( trace_t *tr, const vec3_t p1, const vec3_t p2, vec3_t mins, vec3_t maxs, model_t model, int mask, const vec3_t org, const vec3_t ang, trType_t type );

//
// cm_patches.c
//
cSurfaceCollide_t *CM_GeneratePatchCollide( int width, int height, vec3_t *points );
void CM_ClearLevelPatches( void );

//
// cm_math.c
//
int CM_BoxOnPlaneSide( const vec3_t emins, const vec3_t emaxs, const cplane_t *p );
bool CM_BoundsIntersect( const vec3_t mins, const vec3_t maxs, const vec3_t mins2, const vec3_t maxs2 );
bool CM_BoundsIntersectPoint( const vec3_t mins, const vec3_t maxs, const vec3_t point );
bool CM_PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c );
bool CM_PlaneEqual( cmplane_t *p, float plane[4], int *flipped );
void CM_SnapVector( vec3_t normal );

//
// cm_trisoup.c
//
cSurfaceCollide_t *CM_GenerateTriangleSoupCollide( int numVertexes, vec3_t *vertexes, int numIndexes, int *indexes );


#endif//CM_LOCAL_H