//=======================================================================
//			Copyright XashXT Group 2007 ©
//			   bsplib.h - bsplib header
//=======================================================================
#ifndef BSPLIB_H
#define BSPLIB_H

#include <stdio.h>
#include "platform.h"
#include "utils.h"
#include "mathlib.h"

// supported map formats
enum
{
	BRUSH_UNKNOWN = 0,
	BRUSH_WORLDCRAFT_21,	// quake worldcraft  <= 2.1
	BRUSH_WORLDCRAFT_22,	// half-life worldcraft >= 2.2
	BRUSH_RADIANT,
	BRUSH_QUARK,
	BRUSH_COUNT
};

#define VALVE_FORMAT		220
#define BOGUS_RANGE			WORLD_SIZE
#define MAX_EXPANDED_AXIS		128
#define MAX_PATCH_SIZE		32
#define TEXINFO_NODE		-1		// side is allready on a node
#define MAX_PORTALS			32768
#define PORTALFILE			"PRT1"
#define DEFAULT_IMAGE		"#default.dds"	// use '#' to force FS_LoadImage get texture from buffer  
#define DEGENERATE_EPSILON		0.1
#define MAX_POINTS_ON_WINDING		64
#define MAX_POINTS_ON_FIXED_WINDING	12
#define MAX_SEPERATORS		64
#define MAX_HULL_POINTS		128
#define MAX_MAP_GRID		0xFFFF
#define LG_EPSILON			4
#define PLANENUM_LEAF		-1
#define MAX_PORTALS_ON_LEAF		128
#define MAX_PATCHES			65000		// larger will cause 32 bit overflows
#define PSIDE_FRONT			1
#define PSIDE_BACK			2
#define PSIDE_BOTH			(PSIDE_FRONT|PSIDE_BACK)
#define PSIDE_FACING		4
#define DEF_BACKSPLASH_FRACTION	0.05f	// 5% backsplash by default
#define DEF_BACKSPLASH_DISTANCE	23
#define DEF_RADIOSITY_BOUNCE		1.0f	// default to 100% re-emitted light
#define WORLDSPAWN_CAST_SHADOWS	1
#define WORLDSPAWN_RECV_SHADOWS	1
#define ENTITY_CAST_SHADOWS		0
#define ENTITY_RECV_SHADOWS		1

#define HINT_PRIORITY		1000	// force hint splits first and hintportal/areaportal splits last
#define HINTPORTAL_PRIORITY		-1000
#define AREAPORTAL_PRIORITY		-1000

//#define USE_MALLOC

#ifndef USE_MALLOC
 #define BSP_Realloc( ptr, size )	Mem_Realloc( basepool, ptr, size )
 #define BSP_Malloc( size )		Mem_Alloc( basepool, size )
 #define BSP_Free( ptr )		Mem_Free( ptr )  
#else
 #define BSP_Realloc( ptr, size )	realloc( ptr, size )
 #define BSP_Malloc( size )		malloc( size )
 #define BSP_Free( ptr )		free( ptr ) 
#endif

// bsp cvars setting
extern cvar_t	*bsp_lmsample_size;
extern cvar_t	*bsp_lightmap_size;
extern byte	*checkermate_dds;
extern size_t	checkermate_dds_size;
extern bool full_compile;

// qbsp settings
extern bool notjunc;
extern bool onlyents;
extern bool nosubdivide;
extern bool fastgrid;

extern bool onlyvis;
extern bool onlyrad;
extern physic_exp_t *pe;

extern bool fast;
extern bool faster;
extern bool dirty;
extern bool fastbounce;
extern bool bouncegrid;
extern bool patchMeta;

//
// bsplib.c
//
void ProcessCollisionTree( void );

void WradMain ( bool option );
void WvisMain ( bool option );
void WbspMain ( bool option );

typedef struct plane_s
{
	vec3_t		normal;
	vec_t		dist;
	int		type;
	struct plane_s	*hash_chain;
} plane_t;

typedef struct
{
	int		numpoints;
	vec3_t		p[12];		// variable sized
} winding_t;

typedef struct epair_s
{
	struct epair_s	*next;
	char		*key;
	char		*value;
} epair_t;

typedef struct
{
	vec3_t		UAxis;
	vec3_t		VAxis;
	vec_t		shift[2];
	vec_t		rotate;
	vec_t		scale[2];
} std_vects;

typedef struct
{
	float		vects[2][4];
} qrk_vects;

typedef union
{
	std_vects		valve;
	qrk_vects		quark;
} vects_t;

typedef struct sun_s
{
	struct sun_s	*next;
	vec3_t		direction, color;
	float		photons, deviance, filterRadius;
	int		numSamples, style;
} sun_t;

typedef struct surfmod_s 
{
	struct surfmod_s	*next;
	string		model;
	float		density, odds;
	float		minScale, maxScale;
	float		minAngle, maxAngle;
	bool		oriented;
} surfmod_t;

typedef struct foliage_s 
{
	struct foliage_s		*next;
	string			model;
	float			scale, density, odds;
	bool			inverseAlpha;
} foliage_t;

typedef struct foliageInstance_s
{
	vec3_t			point;
	vec3_t			normal;
} foliageInstance_t;

typedef enum
{
	AM_IDENTITY,
	AM_DOT_PRODUCT
} aModType_t;

typedef struct alphaMod_s
{
	struct alphaMod_s	*next;
	aModType_t	type;
	vec_t		data[16];
} aMod_t;

typedef enum
{
	CM_IDENTITY,
	CM_VOLUME,
	CM_COLOR_SET,
	CM_ALPHA_SET,
	CM_COLOR_SCALE,
	CM_ALPHA_SCALE,
	CM_COLOR_DOT_PRODUCT,
	CM_ALPHA_DOT_PRODUCT,
	CM_COLOR_DOT_PRODUCT_2,
	CM_ALPHA_DOT_PRODUCT_2
} cModType_t;


typedef struct cMod_s
{
	struct cMod_s	*next;
	cModType_t	type;
	vec_t		data[16];
} cMod_t;

typedef float tcMod_t[3][3];

typedef enum
{
	IM_NONE,
	IM_OPAQUE,
	IM_MASKED,
	IM_BLEND
} impMap_t;

typedef struct
{
	string	name;
	int	surfaceFlags;
	int	contents;
	int	value;			// intensity value

	string	flareShader;		// for light flares
	string	backShader;		// for surfaces that generate different front and back passes
	string	cloneShader;		// for cloning of a surface
	string	remapShader;		// remap a shader in final stage

	surfmod_t	*surfaceModel;		// for distribution of models
	foliage_t	*foliage;			// wolf et foliage

	float	subdivisions;		// from a "tesssize xxx"
	float	backsplashFraction;		// floating point value, usually 0.05
	float	backsplashDistance;		// default 16
	float	lightSubdivide;		// default 120
	float	lightFilterRadius;		// lightmap filtering/blurring radius (default 0)
	int	lightmapSampleSize;		// default 16
	float	lightmapSampleOffset;	// default 1.0f

	float	bounceScale;		// radiosity re-emission [0,1.0+]
	float	offset;			// offset in units
	float	shadeAngleDegrees;		// breaking angle for smooth shading (degrees)
	
	vec3_t	mins, maxs;		// for particle studio vertexDeform move support

	bool	legacyTerrain;		// enable legacy terrain crutches
	bool	indexed;	   		// attempt to use indexmap (terrain alphamap style)
	bool	forceMeta;		// force metasurface path
	bool	noClip;	   		// don't clip into bsp, preserve original face winding
	bool	noFast;	   		// supress fast lighting for surfaces with this shader
	bool	invert;	   		// reverse facing
	bool	nonplanar;   		// for nonplanar meta surface merging
	bool	tcGen;	   		// has explicit texcoord generation
	vec3_t	vecs[2];   		// explicit texture vectors for [0,1] texture space
	tcMod_t	mod;	   		// q3map_tcMod matrix
	vec3_t	lightmapAxis;		// explicit lightmap axis projection
	cMod_t	*colorMod;		// q3map_rgb/color/alpha/Set/Mod support
	aMod_t	*alphaMod;		// q3map_alphaMod support

	int	furNumLayers;		// number of fur layers
	float	furOffset;		// offset of each layer
	float	furFade;			// alpha fade amount per layer

	bool	splotchFix;		// filter splotches on lightmaps	
	bool	hasStages;		// false if the shader doesn't define any rendering stages
	bool	globalTexture;		// don't normalize texture repeats
	bool	twoSided;			// cull none
	bool	autosprite;		// autosprite shaders will become point lights
	bool	polygonOffset;		// don't face cull this or against this
	bool	patchShadows;		// have patches casting shadows when using -light for this surface
	bool	vertexShadows;		// shadows will be casted at this surface even when vertex lit
	bool	forceSunlight;		// force sun light at this surface
	bool	notjunc;			// don't use this surface for tjunction fixing
	bool	fogParms;			// has fogparms
	bool	noFog;			// disable fog
	bool	clipModel;		// solid model hack
	bool	noVertexLight;		// leave vertex color alone

	byte	styleMarker;		// light styles hack
	float	vertexScale;		// vertex light scale

	string	skyParmsImageBase;		// for skies
	string	editorImagePath;		// use this image to generate texture coordinates
	string	lightImagePath;		// use this image to generate color / averageColor
	string	normalImagePath;		// normalmap image for bumpmapping

	impMap_t	implicitMap;		// enemy territory implicit shaders
	string	implicitImagePath;

	rgbdata_t	*shaderImage;
	rgbdata_t	*lightImage;
	rgbdata_t	*normalImage;

	float	skyLightValue;
	int	skyLightIterations;
	sun_t	*sun;

	vec3_t	color;			// colorNormalized
	vec3_t	averageColor;
	byte	lightStyle;

	// lightmap custom parms
	bool	lmMergable;
	int	lmCustomWidth;
	int	lmCustomHeight;
	float	lmBrightness;
	float	lmFilterRadius;		// lightmap filtering/blurring radius for this shader

	int	width;			// image width
	int	height;			// image height
	float	stFlat[ 2 ];
	vec3_t	fogDir;
	
	char	*shaderText;
	bool	custom;
	bool	finished;

} bsp_shader_t;

typedef struct indexMap_s
{
	int		w;
	int		h;
	int		numLayers;
	string		name;
	string		shader;
	float		offsets[ 256 ];
	byte		*pixels;
} indexMap_t;

typedef struct side_s
{
	int		planenum;
	int		outputnum;
	float		matrix[2][3];	// quake3 brush primitive texture matrix
	float		vecs[2][4];	// classic texture coordinate mapping
	winding_t		*winding;
	winding_t		*visibleHull;	// convex hull of all visible fragments
	bsp_shader_t	*shader;		// other information about texture

	int		contents;		// from shaderInfo
	int		surfaceFlags;	// from shaderInfo
	int		value;		// from shaderInfo

	bool		visible;		// choose visble planes first
	bool		bevel;		// don't ever use for bsp splitting
	bool		culled;		// bsp face culling
} side_t;

typedef struct sideRef_s
{
	struct sideRef_s	*next;
	side_t		*side;
} sideRef_t;

typedef struct bspface_s
{
	struct bspface_s	*next;		// on node

	int		planenum;
	int		priority;	// added to value calculation

	bool		checked;
	bool		hint;
	winding_t		*w;
} bspface_t;

typedef struct bspbrush_s
{
	struct bspbrush_s	*next;
	struct bspbrush_s	*nextColorModBrush;		// colorMod volume brushes go here
	struct bspbrush_s	*original;		// chopped up brushes will reference the originals
	
	int		entitynum;		// editor numbering
	int		brushnum;			// editor numbering
	int		outputnum;		// set when the brush is written to the file list

	int		castShadows;
	int		recvShadows;

	float		lightmapScale;
	vec3_t		eMins, eMaxs;
	indexMap_t	*im;

	bsp_shader_t	*shader;			// content shader
	int		contents;

	bool		detail;
	bool		opaque;

	int		portalareas[2];

	vec3_t		mins, maxs;
	int		numsides;
	side_t		sides[6];			// variably sized
} bspbrush_t;

typedef struct fog_s
{
	bsp_shader_t	*si;
	bspbrush_t	*brush;
	int		visibleSide;	// the brush side that ray tests need to clip against (-1 == none)
} fog_t;

typedef struct
{
	int		width, height;
	dvertex_t		*verts;
} bsp_mesh_t;

typedef struct parseMesh_s
{
	struct parseMesh_s	*next;
	
	int		entitynum;
	int		brushnum;

	// for shadowcasting entities
	int		castShadows;
	int		recvShadows;
	
	bsp_mesh_t	mesh;
	bsp_shader_t	*shader;
	
	float		lightmapScale;
	vec3_t		eMins, eMaxs;
	indexMap_t	*im;
	
	bool		grouped;
	float		longestCurve;
	int		maxIterations;
} parseMesh_t;

typedef enum
{
	// these match up exactly with dmst_t
	SURFACE_BAD,
	SURFACE_FACE,
	SURFACE_PATCH,
	SURFACE_TRIANGLES,
	SURFACE_FLARE,
	SURFACE_FOLIAGE,
	
	// compiler-relevant surface types
	SURFACE_FORCED_META,
	SURFACE_META,
	SURFACE_FOGHULL,
	SURFACE_DECAL,
	SURFACE_SHADER,
	
	NUM_SURFACE_TYPES
} surfaceType_t;

typedef struct drawsurf_s
{
	surfaceType_t	type;
	bool		planar;
	int		outputnum;	// to match this sort of thing up

	bool		fur;		// this is kind of a hack, but hey...
	bool		skybox;		// yet another fun hack
	bool		backSide;		// q3map_backShader support

	struct drawsurf_s	*parent;		// for cloned (skybox) surfaces to share lighting data
	struct drawsurf_s	*clone;		// for cloned surfaces

	bsp_shader_t	*shader;
	bspbrush_t	*mapBrush;
	parseMesh_t	*mapMesh;
	sideRef_t		*sideRef;

	int		fogNum;

	int		numVerts;
	dvertex_t		*verts;
	int		numIndexes;
	int		*indexes;

	int		planeNum;
	vec3_t		lightmapOrigin;
	vec3_t		lightmapVecs[3];
	int		lightStyle;

	float		lightmapScale;
	
	// surface classification
	vec3_t		mins, maxs;
	vec3_t		lightmapAxis;
	int		sampleSize;

	int		castShadows;
	int		recvShadows;

	float		bias[2];
	int		texMins[2];
	int		texMaxs[2];
	int		texRange[2];

	// patches only
	float		longestCurve;
	int		maxIterations;
	int		patchWidth, patchHeight;
	vec3_t		bounds[2];
	int		lightmapNum;	// -1 = no lightmap

	// for foliage
	int		numFoliageInstances;

	int		entitynum;
	int		surfacenum;

} drawsurf_t;

typedef struct surfaceref_s
{
	struct surfaceref_s	*next;
	int		outputnum;
} surfaceref_t;

// metasurfaces are constructed from lists of metatriangles so they can be merged in the best way
typedef struct metaTriangle_s
{
	bsp_shader_t	*si;
	side_t		*side;
	int		entityNum;
	int		surfaceNum;
	int		planeNum;
	int		fogNum;
	int		sampleSize;
	int		castShadows;
	int		recvShadows;
	vec4_t		plane;
	vec3_t		lightmapAxis;
	int		indexes[ 3 ];
} metaTriangle_t;

typedef struct
{
	vec3_t		origin;
	bspbrush_t	*brushes;
	bspbrush_t	*lastBrush;
	bspbrush_t	*colorModBrushes;
	parseMesh_t	*patches;
	int		entitynum;
	int		firstsurf;
	int		firstBrush;	// only valid during BSP compile
	int		numBrushes;
	epair_t		*epairs;
} bsp_entity_t;

typedef struct node_s
{
	// both leafs and nodes
	int		planenum;		// -1 = leaf node
	struct node_s	*parent;
	vec3_t		mins, maxs;	// valid after portalization
	bspbrush_t	*volume;		// one for each leaf/node

	// this part unique for nodes
	side_t		*side;		// the side that created the node
	struct node_s	*children[2];
	bool		hint;
	int		tinyportals;
	vec3_t		referencepoint;

	// this part unique for leafs
	bool		opaque;		// view can never be inside
	bool		areaportal;
	bool		skybox;		// a skybox leaf
	bool		sky;		// a sky leaf
	int		cluster;		// for portalfile writing
	int		area;		// for areaportals
	bspbrush_t	*brushlist;	// fragments of all brushes in this leaf
	surfaceref_t	*surfaces;	// references to patches pushed down
	int		occupied;		// 1 or greater can reach entity
	bsp_entity_t	*occupant;	// for leak file testing
	int		contents;		// OR of all brush contents

	struct portal_s	*portals;		// also on nodes during construction
} node_t;

typedef struct portal_s
{
	plane_t		plane;
	node_t		*onnode;		// NULL = outside box
	node_t		*nodes[2];	// [0] = front side of plane
	struct portal_s	*next[2];
	winding_t		*winding;

	bool		sidefound;	// false if ->side hasn't been checked
	bool		hint;
	int		contents;		// CONTENTS_AREAPORTAL etc
	side_t		*side;		// NULL = non-visible
} portal_t;

typedef struct
{
	node_t		*headnode;
	node_t		outside_node;
	vec3_t		mins, maxs;
} tree_t;

typedef struct
{
	vec3_t		normal;
	float		dist;
} vplane_t;

typedef struct passage_s
{
	struct passage_s	*next;
	byte		cansee[1];
} passage_t;

typedef enum { stat_none, stat_working, stat_done } vstatus_t;
typedef struct
{
	int		num;
	bool		hint;		// true if this portal was created from a hint splitter
	bool		removed;
	vplane_t		plane;		// normal pointing into neighbor
	int		leaf;		// neighbor
	
	vec3_t		origin;		// for fast clip testing
	float		radius;

	winding_t		*winding;
	vstatus_t		status;
	byte		*portalfront;	// [portals], preliminary
	byte		*portalflood;	// [portals], intermediate
	byte		*portalvis;	// [portals], final

	int		nummightsee;	// bit count on portalflood for sort
	passage_t		*passages;	// there are just as many passages as there
} vportal_t;

typedef struct leaf_s
{
	int		numportals;
	int		merged;
	vportal_t		*portals[MAX_PORTALS_ON_LEAF];
} leaf_t;
	
typedef struct pstack_s
{
	byte		mightsee[MAX_PORTALS/8];		// bit string
	struct pstack_s	*next;
	leaf_t		*leaf;
	vportal_t		*portal;				// portal exiting
	winding_t		*source;
	winding_t		*pass;

	winding_t		windings[3];			// source, pass, temp in any order
	int		freewindings[3];

	vplane_t		portalplane;
	int		depth;
	vplane_t		seperators[2][MAX_SEPERATORS];
	int		numseperators[2];
} pstack_t;

typedef struct
{
	vportal_t		*base;
	int		c_chains;
	pstack_t		pstack_head;
} threaddata_t;

extern	int		num_entities;
extern	bsp_entity_t	entities[MAX_MAP_ENTITIES];
extern	int		num_lightbytes;
extern	byte		*lightbytes;
extern	script_t		*mapfile;

void	ParseEntities( void );
void	UnparseEntities( void );
void	Com_CheckToken( script_t *script, const char *match );
void	Com_Parse1DMatrix( script_t *script, int x, vec_t *m );
void	Com_Parse2DMatrix( script_t *script, int y, int x, vec_t *m );
void	Com_Parse3DMatrix( script_t *script, int z, int y, int x, vec_t *m );
bool	Com_GetTokenAppend( script_t *script, char *buffer, bool crossline, token_t *token );
void	Com_Parse1DMatrixAppend( script_t *script, char *buffer, int x, vec_t *m );
void	SetKeyValue( bsp_entity_t *ent, const char *key, const char *value );
char	*ValueForKey( const bsp_entity_t *ent, const char *key ); // will return "" if not present
vec_t	FloatForKey( const bsp_entity_t *ent, const char *key );
long	IntForKey( const bsp_entity_t *ent, const char *key );
void	GetVectorForKey( const bsp_entity_t *ent, const char *key, vec3_t vec );
void	GetEntityShadowFlags( const bsp_entity_t *e1, const bsp_entity_t *e2, int *castShadows, int *recvShadows );
bsp_entity_t *FindTargetEntity( const char *target );
epair_t	*ParseEpair( token_t *token );

extern	int entity_num;
extern	int entity_numbrushes;
extern	int g_brushtype;
extern	bsp_entity_t *mapent;
extern	bspbrush_t *buildBrush;
extern	plane_t mapplanes[MAX_MAP_PLANES];
extern	int nummapplanes;
extern	vec3_t map_mins, map_maxs, map_size;
extern	drawsurf_t drawsurfs[MAX_MAP_SURFACES];
extern	int numdrawsurfs;
extern	int defaultFogNum;
extern	int numMapFogs;
extern	fog_t mapFogs[MAX_MAP_FOGS];
extern	bool skyboxPresent;
extern	int skyboxArea;
extern	matrix4x4	skyboxTransform;
extern	int texRange;
extern	double normalEpsilon;
extern	double distanceEpsilon;
extern	bool debugPortals;
extern	bool emitFlares;
extern	int patchSubdivisions;
extern	const byte debugColors[12][3];
extern	bool noCollapse;
extern	const char *surfaceTypes[NUM_SURFACE_TYPES];

// bsp lumps
extern	char dentdata[MAX_MAP_ENTSTRING];
extern	int entdatasize;
extern	dshader_t	dshaders[MAX_MAP_SHADERS];
extern	int numshaders;
extern	dplane_t	dplanes[MAX_MAP_PLANES];
extern	int numplanes;
extern	dnode_t dnodes[MAX_MAP_NODES];
extern	int numnodes;
extern	dleaf_t dleafs[MAX_MAP_LEAFS];
extern	int numleafs;
extern	dword dleaffaces[MAX_MAP_LEAFFACES];
extern	int numleaffaces;
extern	dword dleafbrushes[MAX_MAP_LEAFBRUSHES];
extern	int numleafbrushes;
extern	dmodel_t	dmodels[MAX_MAP_MODELS];
extern	int nummodels;
extern	dbrush_t	dbrushes[MAX_MAP_BRUSHES];
extern	int numbrushes;
extern	dbrushside_t dbrushsides[MAX_MAP_BRUSHSIDES];
extern	int numbrushsides;
extern	dvertex_t	dvertexes[MAX_MAP_VERTEXES];
extern	int numvertexes;
extern	int dindexes[MAX_MAP_INDEXES];
extern	int numindexes;
extern	dsurface_t dsurfaces[MAX_MAP_SURFACES];
extern	int numsurfaces;
extern	dfog_t dfogs[MAX_MAP_FOGS];
extern	int numfogs;
extern	int visdatasize;
extern	byte dvisdata[MAX_MAP_VISIBILITY];
extern	dvis_t *dvis;
extern	dlightgrid_t dlightgrid[MAX_MAP_LIGHTGRID];
extern	int numgridpoints;
extern	byte dcollision[MAX_MAP_COLLISION];
extern	int dcollisiondatasize;

void	LoadMapFile ( void );
int	FindFloatPlane( vec3_t normal, vec_t dist, int numPoints, vec3_t *points );
bool	LoadBSPFile ( void );
void	WriteBSPFile ( void );

//=============================================================================
//
// terrain.c
//
void SetTerrainTextures( void );
void ParseTerrain( void );

bspbrush_t *Brush_LoadEntity (bsp_entity_t *ent);
int	PlaneTypeForNormal (vec3_t normal);
bool	MakeBrushPlanes (bspbrush_t *b);
int	FindIntPlane (int *inormal, int *iorigin);
void	CreateBrush( int brushnum );
bool	CreateBrushWindings( bspbrush_t *brush );
bspbrush_t *BrushFromBounds( vec3_t mins, vec3_t maxs );

//=============================================================================

//
// facebsp.c
//

bspface_t	*MakeStructuralBspFaceList( bspbrush_t *list );
bspface_t	*MakeVisibleBspFaceList( bspbrush_t *list );
tree_t	*FaceBSP( bspface_t *list );

//
// brush.c
//

bspbrush_t *AllocBrush( int numsides );
bspbrush_t *CopyBrush( bspbrush_t *brush );
sideRef_t	*AllocSideRef( side_t *side, sideRef_t *next );
void	FilterDetailBrushesIntoTree( bsp_entity_t *e, tree_t *tree );
void	FilterStructuralBrushesIntoTree( bsp_entity_t *e, tree_t *tree );
void	SplitBrush( bspbrush_t *brush, int planenum, bspbrush_t **front, bspbrush_t **back );
void	SnapWeldVector( vec3_t a, vec3_t b, vec3_t out );

//
// fog.c
//
void	FogDrawSurfaces( bsp_entity_t *e );
void	CreateMapFogs( void );
int	FogForBounds( vec3_t mins, vec3_t maxs, float epsilon );
int	FogForPoint( vec3_t point, float epsilon );

//
// surface.c
//
drawsurf_t	*CloneSurface( drawsurf_t *src, bsp_shader_t *si );
drawsurf_t	*DrawSurfaceForSide( bsp_entity_t *e, bspbrush_t *b, side_t *s, winding_t *w );
drawsurf_t	*DrawSurfaceForMesh( bsp_entity_t *e, parseMesh_t *p, bsp_mesh_t *mesh );
drawsurf_t	*DrawSurfaceForFlare( int num, vec3_t org, vec3_t nm, vec3_t col, const char *shader, int lStyle );
drawsurf_t	*DrawSurfaceForShader( const char *shader );

void	SubdivideFaceSurfaces( bsp_entity_t *e, tree_t *tree );
bool	CalcLightmapAxis( vec3_t normal, vec3_t axis );
void	MakeDebugPortalSurfs( tree_t *tree );
void	MakeFogHullSurfs( bsp_entity_t *e, tree_t *tree, const char *shader );
void	AddEntitySurfaceModels( bsp_entity_t *e );
void	ClassifySurfaces( int numSurfs, drawsurf_t *ds );
void	ClassifyEntitySurfaces( bsp_entity_t *e );
void	ClearSurface( drawsurf_t *ds );
void	FilterDrawsurfsIntoTree( bsp_entity_t *e, tree_t *tree );
void	ClipSidesIntoTree( bsp_entity_t *e, tree_t *tree );
int	FilterFaceIntoTree( drawsurf_t *ds, tree_t *tree );
void	SubdivideDrawSurfs( bsp_entity_t *e, tree_t *tree );
void	FixTJunctions( bsp_entity_t *ent );
drawsurf_t *AllocDrawSurf( surfaceType_t type );
void	StripFaceSurface( drawsurf_t *ds );
void	FanFaceSurface( drawsurf_t *ds );
void	TidyEntitySurfaces( bsp_entity_t *e );
bool	CalcSurfaceTextureRange( drawsurf_t *ds );

//
// surface_meta.c
//

void SetDefaultSampleSize( int sampleSize );
void SetSurfaceExtra( drawsurf_t *ds, int num );
bsp_shader_t *GetSurfaceExtraShader( int num );
int GetSurfaceExtraParentSurfaceNum( int num );
int GetSurfaceExtraEntityNum( int num );
int GetSurfaceExtraCastShadows( int num );
int GetSurfaceExtraRecvShadows( int num );
int GetSurfaceExtraSampleSize( int num );
float GetSurfaceExtraLongestCurve( int num );
void GetSurfaceExtraLightmapAxis( int num, vec3_t lightmapAxis );
void WriteSurfaceExtraFile( void );
void LoadSurfaceExtraFile( void );
void MakeEntityMetaTriangles( bsp_entity_t *e );
void FixMetaTJunctions( void );
void Foliage( drawsurf_t *src );
void ClearMetaTriangles( void );
void SmoothMetaTriangles( void );
void MergeMetaTriangles( void );
void Fur( drawsurf_t *ds );

extern int c_stripSurfaces, c_fanSurfaces;

//
// decals.c
//
void ProcessDecals( void );
void MakeEntityDecals( bsp_entity_t *e );

//
// lightmaps.c
//
void WriteLightmaps( void );

//
// map.c
//
void AddBrushBevels( void );

//
// mesh.c
//
void LerpDrawVert( dvertex_t *a, dvertex_t *b, dvertex_t *out );
void LerpDrawVertAmount( dvertex_t *a, dvertex_t *b, float amount, dvertex_t *out );
void FreeMesh( bsp_mesh_t *m );
bsp_mesh_t *CopyMesh( bsp_mesh_t *mesh );
bsp_mesh_t *TransposeMesh( bsp_mesh_t *in );
void InvertMesh( bsp_mesh_t *in );
void MakeMeshNormals( bsp_mesh_t in );
void PutMeshOnCurve( bsp_mesh_t in );
bsp_mesh_t *SubdivideMesh( bsp_mesh_t in, float maxError, float minLength );
int IterationsForCurve( float len, int subdivisions );
bsp_mesh_t *SubdivideMesh2( bsp_mesh_t in, int iterations );
void ProjectPointOntoVector( vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj );
bsp_mesh_t *SubdivideMeshQuads( bsp_mesh_t *in, float minLength, int maxsize, int *widthtable, int *heighttable );
bsp_mesh_t *RemoveLinearMeshColumnsRows( bsp_mesh_t *in );

//
// model.c
//
void InsertModel( const char *name, int body, int seq, float f, matrix4x4 transform, drawsurf_t *ds, int spawnflags );
void AddTriangleModels( bsp_entity_t *e );

//
// patch.c
//
void ParsePatch( void );
void PatchMapDrawSurfs( bsp_entity_t *e );

//
// tree.c
//
tree_t *AllocTree (void);
node_t *AllocNode (void);
int CountBrushList (bspbrush_t *brushes);
void FreeBrush (bspbrush_t *brushes);
vec_t BrushVolume (bspbrush_t *brush);
bool BoundBrush( bspbrush_t *brush );
void FreeBrushList (bspbrush_t *brushes);
tree_t *BrushBSP (bspbrush_t *brushlist, vec3_t mins, vec3_t maxs);
int CountBrushList( bspbrush_t *brushes );

//=============================================================================

// winding.c

winding_t	*AllocWinding( int points );
winding_t	*WindingFromDrawSurf( drawsurf_t *ds );
int	WindingOnPlaneSide( winding_t *w, vec3_t normal, vec_t dist );
void	AddWindingToConvexHull( winding_t *w, winding_t **hull, vec3_t normal );
bool	WindingIsTiny( winding_t *w );
vec_t	WindingArea (winding_t *w);
void	WindingCenter (winding_t *w, vec3_t center);
void	ClipWindingEpsilon( winding_t *in, vec3_t norm, vec_t dist, vec_t eps, winding_t **front, winding_t **back );
winding_t	*CopyWinding (winding_t *w);
winding_t	*ReverseWinding (winding_t *w);
winding_t	*BaseWindingForPlane (vec3_t normal, vec_t dist);
void	CheckWinding (winding_t *w);
void	WindingPlane (winding_t *w, vec3_t normal, vec_t *dist);
void	RemoveColinearPoints (winding_t *w);
void	FreeWinding (winding_t *w);
void	WindingBounds (winding_t *w, vec3_t mins, vec3_t maxs);
void	ChopWindingInPlace (winding_t **w, vec3_t normal, vec_t dist, vec_t epsilon);
//=============================================================================

//
// lightmap.c
//
void AllocateLightmaps( bsp_entity_t *e );

//
// portals.c
//
void	MakeHeadnodePortals( tree_t *tree );
void	MakeNodePortal( node_t *node );
void	SplitNodePortals( node_t *node );
bool	Portal_Passable( portal_t *p );
bool	FloodEntities( tree_t *tree );
void	FillOutside( node_t *headnode );
void	FloodAreas( tree_t *tree );
void	RemovePortalFromNode( portal_t *portal, node_t *l );
bspface_t	*VisibleFaces( bsp_entity_t *e, tree_t *tree );
void	MakeTreePortals( tree_t *tree );
void	FreePortal( portal_t *p );

//=============================================================================
// shaders.c

int	LoadShaderInfo( void );
bsp_shader_t *FindShader( const char *texture );
void	AlphaMod( aMod_t *am, int numVerts, dvertex_t *drawVerts );
void	TcMod( tcMod_t mod, float st[ 2 ] );
void	TcModIdentity( tcMod_t mod );
void	TcModMultiply( tcMod_t a, tcMod_t b, tcMod_t out );
void	TcModTranslate( tcMod_t mod, float s, float t );
void	TcModScale( tcMod_t mod, float s, float t );
void	TcModRotate( tcMod_t mod, float euler );
bool	ApplySurfaceParm( const char *name, int *contentFlags, int *surfaceFlags );
void	BeginMapShaderFile( void );
void	WriteMapShaderFile( void );
bsp_shader_t *CustomShader( bsp_shader_t *si, const char *find, const char *replace );
void	EmitVertexRemapShader( char *from, char *to );
void	ColorMod( cMod_t *cm, int numVerts, dvertex_t *drawVerts );
void	TCMod( tcMod_t mod, float st[2] );

//=============================================================================
// leakfile.c

void LeakFile (tree_t *tree);

//=============================================================================
//
// prtfile.c
//

void NumberClusters( tree_t *tree );
void WritePortalFile( tree_t *tree );

//=============================================================================

// writebsp.c
int EmitShader( const char *shader, int *contents, int *surfaceFlags );
void SetModelNumbers( void );
void SetLightStyles( void );
void BeginBSPFile( void );
void EndBSPFile( void );
void BeginModel( void );
void EndModel( bsp_entity_t *e, node_t *headnode );

//=============================================================================

//
// tree.c
//

void FreeTree( tree_t *tree );
void FreeTree_r( node_t *node );
void FreeTreePortals_r( node_t *node );

//=============================================================================

// vis.c

winding_t	*NewVisWinding( int points );
void	FreeVisWinding( winding_t *w );
winding_t	*CopyVisWinding( winding_t *w );
int CompressVis( byte *vis, byte *dest );
void DecompressVis( byte *in, byte *decompressed );

extern	int	numportals;
extern	int	portalclusters;

extern	vportal_t	*portals;
extern	leaf_t	*leafs;

extern	int	c_portaltest, c_portalpass, c_portalcheck;
extern	int	c_portalskip, c_leafskip;
extern	int	c_vistest, c_mighttest;
extern	int	c_chains;
extern	byte	*vismap, *vismap_p, *vismap_end;	// past visfile
extern	int	testlevel;
extern	byte	*uncompressed;
extern	int	leafbytes, leaflongs;
extern	int	portalbytes, portallongs;
extern	float	farPlaneDist;

//
// visflow.c
//

void	CreatePassages( int portalnum );
void	PassageFlow( int portalnum );
void	PassagePortalFlow( int portalnum );
void	BasePortalVis( int portalnum );
void	BetterPortalVis( int portalnum );
void	PortalFlow( int portalnum );
int	CountBits( byte *bits, int numbits );

extern	vportal_t	*sorted_portals[MAX_MAP_PORTALS*2];


//=============================================================================

// qrad3.c

#define LIGHT_ATTEN_LINEAR		1
#define LIGHT_ATTEN_ANGLE		2
#define LIGHT_ATTEN_DISTANCE		4
#define LIGHT_TWOSIDED		8
#define LIGHT_GRID			16
#define LIGHT_SURFACES		32
#define LIGHT_DARK			64		// probably never use this
#define LIGHT_FAST			256
#define LIGHT_FAST_TEMP		512
#define LIGHT_FAST_ACTUAL		(LIGHT_FAST|LIGHT_FAST_TEMP)
#define LIGHT_NEGATIVE		1024

#define LIGHT_SUN_DEFAULT		(LIGHT_ATTEN_ANGLE|LIGHT_GRID|LIGHT_SURFACES)
#define LIGHT_AREA_DEFAULT		(LIGHT_ATTEN_ANGLE|LIGHT_ATTEN_DISTANCE|LIGHT_GRID|LIGHT_SURFACES)
#define LIGHT_Q3A_DEFAULT		(LIGHT_ATTEN_ANGLE|LIGHT_ATTEN_DISTANCE|LIGHT_GRID|LIGHT_SURFACES|LIGHT_FAST)
#define LIGHT_WOLF_DEFAULT		(LIGHT_ATTEN_LINEAR|LIGHT_ATTEN_DISTANCE|LIGHT_GRID|LIGHT_SURFACES|LIGHT_FAST)

#define MAX_TRACE_TEST_NODES		256
#define DEFAULT_INHIBIT_RADIUS	1.5f

#define LUXEL_EPSILON		0.125f
#define VERTEX_EPSILON		-0.125f
#define GRID_EPSILON		0.0f

#define DEFAULT_LIGHTMAP_SAMPLE_SIZE	16
#define DEFAULT_LIGHTMAP_SAMPLE_OFFSET	1.0f
#define DEFAULT_SUBDIVIDE_THRESHOLD	1.0f

#define EXTRA_SCALE			2	/* -extrawide = -super 2 */
#define EXTRAWIDE_SCALE		2	/* -extrawide = -super 2 -filter */

#define CLUSTER_UNMAPPED		-1
#define CLUSTER_OCCLUDED		-2
#define CLUSTER_FLOODED			-3

#define VERTEX_LUXEL_SIZE		3
#define BSP_LUXEL_SIZE			3
#define RAD_LUXEL_SIZE			3
#define SUPER_LUXEL_SIZE		4
#define SUPER_ORIGIN_SIZE		3
#define SUPER_NORMAL_SIZE		4
#define SUPER_DELUXEL_SIZE		3
#define BSP_DELUXEL_SIZE		3

#define VERTEX_LUXEL( s, v )		(vertexLuxels[s] + ((v) * VERTEX_LUXEL_SIZE))
#define RAD_VERTEX_LUXEL( s, v )	(radVertexLuxels[s] + ((v) * VERTEX_LUXEL_SIZE))
#define BSP_LUXEL( s, x, y )		(lm->bspLuxels[s] + ((((y) * lm->w) + (x)) * BSP_LUXEL_SIZE))
#define RAD_LUXEL( s, x, y )		(lm->radLuxels[s] + ((((y) * lm->w) + (x)) * RAD_LUXEL_SIZE))
#define SUPER_LUXEL( s, x, y )	(lm->superLuxels[s] + ((((y) * lm->sw) + (x)) * SUPER_LUXEL_SIZE))
#define SUPER_DELUXEL( x, y )		(lm->superDeluxels + ((((y) * lm->sw) + (x)) * SUPER_DELUXEL_SIZE))
#define BSP_DELUXEL( x, y )		(lm->bspDeluxels + ((((y) * lm->w) + (x)) * BSP_DELUXEL_SIZE))
#define SUPER_CLUSTER( x, y )		(lm->superClusters + (((y) * lm->sw) + (x)))
#define SUPER_ORIGIN( x, y )		(lm->superOrigins + ((((y) * lm->sw) + (x)) * SUPER_ORIGIN_SIZE))
#define SUPER_NORMAL( x, y )		(lm->superNormals + ((((y) * lm->sw) + (x)) * SUPER_NORMAL_SIZE))
#define SUPER_DIRT( x, y )		(lm->superNormals + ((((y) * lm->sw) + (x)) * SUPER_NORMAL_SIZE) + 3)

typedef enum
{
	emit_point,
	emit_area,
	emit_spotlight,
	emit_sun
} emittype_t;

typedef struct light_s
{
	struct light_s	*next;
	emittype_t	type;
	int		flags;		// condensed all the booleans into one flags int
	bsp_shader_t	*si;

	vec3_t		origin;
	vec3_t		normal;		// for surfaces, spotlights, and suns
	float		dist;		// plane location along normal

	int		photons;
	int		style;
	vec3_t		color;
	float		radiusByDist;	// for spotlights
	float		fade;	 	// from wolf, for linear lights
	float		angleScale;	// stolen from vlight for K

	float		add;		// used for area lights
	float		envelope;		// units until falloff < tolerance
	float		envelope2;	// envelope squared (tiny optimization)
	vec3_t		mins, maxs;	// pvs envelope */
	int		cluster;		// cluster light falls into */

	winding_t		*w;
	vec3_t		emitColor;	// full out-of-gamut value

	float		falloffTolerance;	// minimum attenuation threshold
	float		filterRadius;	// lightmap filter radius in world units, 0 == default
} light_t;

extern	float	lightscale;
extern	float	ambient;
extern	float	maxlight;
extern	float	direct_scale;
extern	float	entity_scale;
extern	bool	noSurfaces;
extern	bool	bouncing;
extern	int	superSample;
extern	bool	notrace;
extern	bool	debugSurfaces;
extern	bool	debug;
extern	bool	debugAxis;
extern	bool	debugCluster;
extern	bool	debugOrigin;
extern	bool	lightmapBorder;
extern 	bool	normalmap;
extern	bool	deluxemap;
extern	vec3_t	ambientColor;
extern	vec3_t	minLight;
extern	vec3_t	minVertexLight;
extern	vec3_t	minGridLight;
extern	int	bounce;
extern	bool	bounceOnly;
extern	bool	bouncing;
extern	bool	exactPointToPolygon;
extern	float	linearScale;
extern	float	falloffTolerance;
extern	bool	noStyles;
extern	float	skyScale;
extern	float	pointScale;
extern	float	areaScale;
extern	float	bounceScale;
extern	float	lightmapGamma;
extern	float	lightmapCompensate;
extern	int	lightSamples;
extern	bool	filter;
extern	bool	dark;
extern	float	shadeAngleDegrees;
extern	int	approximateTolerance;
extern	bool	trisoup;
extern	bool	debugDeluxemap;
extern	bool	dirtDebug;
extern	int	dirtMode;
extern	float	dirtDepth;
extern	float	dirtScale;
extern	float	dirtGain;
extern	bool	cpmaHack;

void ColorToBytes( const float *color, byte *colorBytes, float scale );
void CreateEntityLights( void );
void CreateSurfaceLights( void );
bool ClusterVisible( int a, int b );

//===============================================================

//
// rad_sample.c
//
int ClusterForPointExtFilter( vec3_t point, float epsilon, int numClusters, int *clusters );
void SetupTraceNodes( void );

//
// rad_trace.c
//

typedef struct
{
	// constant input
	bool		testOcclusion;
	bool		forceSunlight;
	bool		testAll;
	int		recvShadows;
	
	int		numSurfaces;
	int		*surfaces;
	
	int		numLights;
	light_t		**lights;

	bool		twoSided;

	// per-sample input
	int		cluster;
	vec3_t		origin, normal;
	vec_t		inhibitRadius;	// sphere in which occluding geometry is ignored

	// per-light input
	light_t		*light;
	vec3_t		end;
	
	// calculated input
	vec3_t		displacement, direction;
	vec_t		distance;
	
	// input and output
	vec3_t		color;		// starts out at full color, may be reduced
					// if transparent surfaces are crossed
	
	// output
	vec3_t		hit;
	int		surfaceFlags;	// for determining surface compile flags traced through
	bool		passSolid;
	bool		opaque;
	
	// working data
	int		numTestNodes;
	int		testNodes[MAX_TRACE_TEST_NODES]; 
} lighttrace_t;

// must be identical to bspDrawVert_t except for float color!
typedef struct
{
	vec3_t		point;
	float		st[2];
	float		lm[LM_STYLES][2];
	vec3_t		normal;
	float		color[LM_STYLES][4];
} radVert_t;

typedef struct
{
	int		numVerts;
	radVert_t		verts[MAX_POINTS_ON_WINDING];
} radWinding_t;

// crutch for poor local allocations in win32 smp
typedef struct
{
	vec_t		dists[MAX_POINTS_ON_WINDING+4];
	int		sides[MAX_POINTS_ON_WINDING+4];
} clipWork_t;

typedef struct outLightmap_s
{
	int		lightmapNum;
	int		extLightmapNum;
	int		customWidth;
	int		customHeight;
	int		numLightmaps;
	int		freeLuxels;
	int		numShaders;
	bsp_shader_t	*shaders[MAX_LIGHTMAP_SHADERS];
	byte		*lightBits;
	byte		*bspLightBytes;
	byte		*bspDirBytes;
} outLightmap_t;

typedef struct lightmap_s
{
	bool		finished;
	bool		splotchFix;
	bool		wrap[2];
	int		customWidth;
	int		customHeight;
	float		brightness;
	float		filterRadius;
	
	int		firstLightSurface;
	int		numLightSurfaces;	// index into lightSurfaces
	int		numLightClusters;
	int		*lightClusters;
	
	int		sampleSize;
	int		actualSampleSize;
	int		axisNum;
	int		entityNum;
	int		recvShadows;
	vec3_t		mins;
	vec3_t		maxs;
	vec3_t		axis;
	vec3_t		origin;
	vec3_t		*vecs;
	float		*plane;
	int		w, h, sw, sh, used;
	
	bool		solid[LM_STYLES];
	vec3_t		solidColor[LM_STYLES];
	
	int		numStyledTwins;
	struct lightmap_s	*twins[LM_STYLES];

	int		outLightmapNums[LM_STYLES];
	int		twinNums[LM_STYLES];
	int		lightmapX[LM_STYLES];
	int		lightmapY[LM_STYLES];
	byte		styles[LM_STYLES];
	float		*bspLuxels[LM_STYLES];
	float		*radLuxels[LM_STYLES];
	float		*superLuxels[LM_STYLES];
	float		*superOrigins;
	float		*superNormals;
	int		*superClusters;
	
	float		*superDeluxels;	// average light direction
	float		*bspDeluxels;

} rawLightmap_t;

typedef struct rawGridPoint_s
{
	vec3_t		ambient[LM_STYLES];
	vec3_t		directed[LM_STYLES];
	vec3_t		dir;
	byte		styles[LM_STYLES];
} rawGridPoint_t;


typedef struct surfaceInfo_s
{
	dmodel_t		*model;
	bsp_shader_t	*si;
	rawLightmap_t	*lm;
	int		parentSurfaceNum;
	int		childSurfaceNum;
	int		entityNum;
	int		castShadows;
	int		recvShadows;
	int		sampleSize;
	int		patchIterations;
	float		longestCurve;
	float		*plane;
	vec3_t		axis, mins, maxs;
	bool		hasLightmap;
	bool		approximated;
	int		firstSurfaceCluster;
	int		numSurfaceClusters;

} surfaceInfo_t;

void InitTrace( void );
void RadLightForTriangles( int num, int lNum, rawLightmap_t *lm, bsp_shader_t *si, float s, float sub, clipWork_t *cw );
void RadLightForPatch( int num, int lNum, rawLightmap_t *lm, bsp_shader_t *si, float s, float sub, clipWork_t *cw );
void LightingAtSample( lighttrace_t *trace, byte styles[LM_STYLES], vec3_t colors[LM_STYLES] );
int LightContributionToSample( lighttrace_t *trace );
int LightContributionToPoint( lighttrace_t *trace );
int ClusterForPointExt( vec3_t point, float epsilon );

// traceWork_t is only a parameter to crutch up poor large local allocations on
// winNT and macOS.  It should be allocated in the worker function, but never
// looked at.
typedef struct
{
	vec3_t		start, end;
	int		numOpenLeafs;
	int		openLeafNumbers[MAX_MAP_LEAFS];
	lighttrace_t	*trace;
} traceWork_t;

void CreateTraceLightsForBounds( vec3_t m1, vec3_t m2, vec3_t n, int numCl, int *cl, int flags, lighttrace_t *trace );
float PointToPolygonFormFactor( const vec3_t point, const vec3_t normal, const winding_t *w );
void CreateTraceLightsForSurface( int num, lighttrace_t *trace );
void SetupEnvelopes( bool forGrid, bool fastFlag );
void IlluminateRawLightmap( int rawLightmapNum );
void FreeTraceLights( lighttrace_t *trace );
void DirtyRawLightmap( int rawLightmapNum );
void MapRawLightmap( int rawLightmapNum );
float SetupTrace( lighttrace_t *trace );
void TraceLine( lighttrace_t *trace );
void RadCreateDiffuseLights( void );
void StitchSurfaceLightmaps( void );
void StoreSurfaceLightmaps( void );
void IlluminateVertexes( int num );
bool PointInSolid( vec3_t start );
void RadFreeLights( void );
void SmoothNormals( void );
void SetupBrushes( void );
void SetupDirt( void );
void SetupSurfaceLightmaps( void );
void WriteLightmaps( void );

typedef struct
{
	int		textureNum;
	int		x, y, width, height;

	// for faces
	vec3_t		origin;
	vec3_t		vecs[3];
} lightmap_t;

extern float	*vertexLuxels[LM_STYLES];
extern float	*radVertexLuxels[LM_STYLES];

extern surfaceInfo_t *surfaceInfos;
extern int	numLightSurfaces;
extern int	*lightSurfaces;
extern int	numSurfaceClusters, maxSurfaceClusters;
extern int	*surfaceClusters;

extern light_t	*lights;
extern dvertex_t	*yDrawVerts;
extern int	numPointLights;
extern int	numDiffuseLights;
extern int	numSpotLights;
extern int	numSunLights;

extern int	numLights;
extern int	numCulledLights;
extern int	lightsBoundsCulled;
extern int	lightsEnvelopeCulled;
extern int	lightsPlaneCulled;
extern int	lightsClusterCulled;

extern int	numLuxels;
extern int	numLuxelsMapped;
extern int	numLuxelsOccluded;
extern int	numLuxelsIlluminated;
extern int	numVertsIlluminated;
extern int	numRawLightmaps;
extern rawLightmap_t *rawLightmaps;

bool PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c );
bool RadSampleImage( byte *pixels, int width, int height, float st[2], float color[4] );

#endif//BSPLIB_H