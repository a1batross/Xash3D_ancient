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
#define MAX_CM_AREAPORTALS	MAX_EDICTS
#define BOX_MODEL_HANDLE	MAX_MODELS - 1
#define PLANE_NORMAL_EPSILON	0.00001f
#define PLANE_DIST_EPSILON	0.01f

// 1/32 epsilon to keep floating point happy
#define SURFACE_CLIP_EPSILON	(0.03125)

typedef struct
{
	cplane_t       	*plane;
	int		children[2];	// negative numbers are leafs
} cnode_t;

typedef struct
{
	int		shadernum;
	cplane_t		*plane;
} cbrushside_t;

typedef struct
{
	int		contents;
	int		checkcount;	// to avoid repeated testings
	int		numsides;
	cbrushside_t	*brushsides;
} cbrush_t;

typedef struct
{
	int		contents;
	int		checkcount;	// to avoid repeated testings
	vec3_t		mins, maxs;
	int		numfacets;
	cbrush_t		*facets;
} cface_t;

typedef struct
{
	int		contents;
	int		cluster;
	int		area;

	int		nummarkbrushes;
	cbrush_t		**markbrushes;

	int		nummarkfaces;
	cface_t		**markfaces;
} cleaf_t;

typedef struct
{
	string		name;		// model name
	byte		*mempool;		// private mempool
	int		registration_sequence;

	// shared modelinfo
	modtype_t		type;		// model type
	vec3_t		mins, maxs;	// bounding box at angles '0 0 0'
	byte		*extradata;	// studiomodels extradata
	void		*submodels;	// animations ptr
	int		numframes;	// sprite framecount

	cleaf_t		leaf;		// holds the markbrushes and markfaces
} cmodel_t;

typedef struct
{
	int		numareaportals;
	int		areaportals[MAX_CM_AREAPORTALS];
	int		floodnum;		// if two areas have equal floodnums, they are connected
	int		floodvalid;
} carea_t;

typedef struct
{
	byte		open;
	byte		area;		// MAX_MAP_AREAS is 256
	byte		otherarea;
} careaportal_t;

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

	cbrush_t		**markbrushes;
	int		nummarkbrushes;

	cface_t		**markfaces;
	int		nummarkfaces;

	cmodel_t		*models;
	int		nummodels;

	cbrush_t		*brushes;
	int		numbrushes;

	vec3_t		*vertices;
	int		numverts;

	int		numclusters;
	int		clusterBytes;

	dvis_t		*pvs;
	dvis_t		*phs;
	size_t		visdata_size;	// if false, visibility is just a single cluster of ffs

	script_t		*entityscript;

	carea_t		*areas;
	int		numareas;

	cface_t		*faces;
	int		numfaces;

	careaportal_t	areaportals[MAX_CM_AREAPORTALS];
	int		numareaportals;

	int		floodvalid;
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
int CM_LeafArea( int leafnum );
int CM_LeafCluster( int leafnum );
byte *CM_ClusterPVS( int cluster );
byte *CM_ClusterPHS( int cluster );
int CM_PointLeafnum( const vec3_t p );
bool CM_AreasConnected( int area, int otherarea );
int CM_BoxLeafnums( const vec3_t mins, const vec3_t maxs, int *list, int listsize, int *lastleaf );
model_t CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, bool capsule );
bool CM_BoxVisible( const vec3_t mins, const vec3_t maxs, byte *visbits );
int CM_PointContents( const vec3_t p, model_t model );
int CM_TransformedPointContents( const vec3_t p, model_t model, const vec3_t origin, const vec3_t angles );
void CM_BoxLeafnums_r( leaflist_t *ll, int nodenum );

//
// cm_portals.c
//
void CM_CalcPHS( void );
byte *CM_FatPVS( const vec3_t org, bool portal );
byte *CM_FatPHS( int cluster, bool portal );
void CM_LoadAreaPortals( const char *filename );
void CM_SaveAreaPortals( const char *filename );
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
void CM_StudioInitBoxHull( void );
int CM_StudioBodyVariations( model_t handle );
void CM_StudioGetAttachment( edict_t *e, int iAttachment, float *org, float *ang );
bool CM_StudioTrace( trace_t *tr, edict_t *e, const vec3_t p1, const vec3_t p2 );
void CM_GetBonePosition( edict_t* e, int iBone, float *rgflOrigin, float *rgflAngles );
	
//
// cm_trace.c
//
void CM_BoxTrace( trace_t *tr, const vec3_t p1, const vec3_t p2, vec3_t mins, vec3_t maxs, model_t model, int mask, trType_t type );
void CM_TransformedBoxTrace( trace_t *tr, const vec3_t p1, const vec3_t p2, vec3_t mins, vec3_t maxs, model_t model, int mask, const vec3_t org, const vec3_t ang, trType_t type );

//
// cm_patches.c
//
void CM_CreatePatch( cface_t *patch, dshader_t *shader, vec3_t *verts, int *patch_cp );

//
// cm_math.c
//
int CM_BoxOnPlaneSide( const vec3_t emins, const vec3_t emaxs, const cplane_t *p );
bool CM_ComparePlanes( const vec3_t p1normal, float p1dist, const vec3_t p2normal, float p2dist );
bool CM_PlaneFromPoints( vec3_t verts[3], cplane_t *plane );
void CM_SnapPlane( vec3_t normal, float *dist );
void CM_CategorizePlane( cplane_t *plane );
void CM_SnapVector( vec3_t normal );

#endif//CM_LOCAL_H