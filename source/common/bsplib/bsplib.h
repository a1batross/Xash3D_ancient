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
#define MAX_BRUSH_SIDES		128
#define BOGUS_RANGE			WORLD_SIZE
#define TEXINFO_NODE		-1		// side is allready on a node
#define MAX_PORTALS			32768
#define PORTALFILE			"PRT1"
#define MAX_POINTS_ON_WINDING		64
#define MAX_POINTS_ON_FIXED_WINDING	12
#define MAX_SEPERATORS		64
#define MAX_HULL_POINTS		128
#define PLANENUM_LEAF		-1
#define MAXEDGES			20
#define MAX_NODE_BRUSHES		8
#define MAX_PORTALS_ON_LEAF		128
#define MAX_MAP_SIDES		(MAX_MAP_BRUSHES*6)
#define MAX_PATCHES			65000		// larger will cause 32 bit overflows
#define PSIDE_FRONT			1
#define PSIDE_BACK			2
#define PSIDE_BOTH			(PSIDE_FRONT|PSIDE_BACK)
#define PSIDE_FACING		4

// bsp cvars setting
extern cvar_t	*bsp_lightmap_size;

extern bool full_compile;

// qbsp settings
extern bool notjunc;
extern bool onlyents;
extern bool nosubdivide;

extern bool onlyvis;
extern bool onlyrad;
extern physic_exp_t *pe;

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
	vec3_t		origin;
	struct bspbrush_s	*brushes;
	int		firstsurf;
	epair_t		*epairs;
} bsp_entity_t;

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

typedef struct
{
	string	name;
	int	surfaceFlags;
	int	contents;
	int	value;			// intensity value

	string	backShader;		// for surfaces that generate different front and back passes
	string	flareShader;		// for light flares

	float	subdivisions;		// from a "tesssize xxx"
	float	backsplashFraction;		// floating point value, usually 0.05
	float	backsplashDistance;		// default 16
	float	lightSubdivide;		// default 120
	int	lightmapSampleSize;		// default 16

	bool	hasStages;		// false if the shader doesn't define any rendering stages
	bool	globalTexture;		// don't normalize texture repeats

	bool	twoSided;			// cull none
	bool	autosprite;		// autosprite shaders will become point lights

	bool	lightFilter;		// light rays that cross surfaces of this type
					// should test against the filter image
	bool	forceTraceLight;		// always use -light for this surface
	bool	forceVLight;		// always use -vlight for this surface
	bool	forceSunLight;		// force sun light at this surface
	bool	notjunc;			// don't use this surface for tjunction fixing
	float	vertexScale;		// vertex light scale
	
	string	editorimage;		// use this image to generate texture coordinates
	string	lightimage;		// use this image to generate color / averageColor
	vec3_t	color;			// colorNormalized
	vec3_t	averageColor;

	byte	*pixels;			// pointer to lightimage

	int	width;			// image width
	int	height;			// image height
	
	vec3_t	sunLight;
	vec3_t	sunDirection;
} bsp_shader_t;

typedef struct side_s
{
	int		planenum;
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
} side_t;

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

	int		entitynum;		// editor numbering
	int		brushnum;			// editor numbering
	int		outputnum;		// set when the brush is written to the file list
	int		contents;
	bool		detail;
	bool		opaque;

	int		portalareas[2];

	struct bspbrush_s	*original;		// chopped up brushes will reference the originals
	vec3_t		mins, maxs;
	int		numsides;
	side_t		sides[6];			// variably sized
} bspbrush_t;

typedef struct drawsurf_s
{
	bsp_shader_t	*shader;

	bspbrush_t	*mapbrush;	// not valid for patches
	side_t		*side;		// not valid for patches

	struct drawsurf_s	*next;		// when sorting by shader for lightmaps

	int		planenum;
	int		numverts;
	dvertex_t		*verts;

	int		numindices;
	int		*indices;

	int		lightmapNum;	// -1 = no lightmap
	int		lightmapX, lightmapY;
	int		lightmapWidth, lightmapHeight;
	vec3_t		lightmapVecs[3];
	vec3_t		lightmapOrigin;

} drawsurf_t;

typedef struct surfaceref_s
{
	struct surfaceref_s	*next;
	int		outputnum;
} surfaceref_t;

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
	vportal_t	*base;
	int		c_chains;
	pstack_t		pstack_head;
} threaddata_t;

extern	int		num_entities;
extern	bsp_entity_t	entities[MAX_MAP_ENTITIES];

void	ParseEntities( void );
void	UnparseEntities( void );
void	Com_CheckToken( const char *match );
void	Com_Parse1DMatrix( int x, vec_t *m );
void	Com_Parse2DMatrix( int y, int x, vec_t *m );
void	Com_Parse3DMatrix( int z, int y, int x, vec_t *m );
void	SetKeyValue( bsp_entity_t *ent, const char *key, const char *value );
char	*ValueForKey( bsp_entity_t *ent, const char *key ); // will return "" if not present
vec_t	FloatForKey( bsp_entity_t *ent, const char *key );
void	GetVectorForKey( bsp_entity_t *ent, const char *key, vec3_t vec );
epair_t	*ParseEpair( void );


extern	int entity_num;
extern	int g_brushtype;
extern	plane_t mapplanes[MAX_MAP_PLANES];
extern	int nummapplanes;
extern	vec3_t map_mins, map_maxs;
extern	side_t brushsides[MAX_MAP_SIDES];
extern	drawsurf_t drawsurfs[MAX_MAP_SURFACES];
extern	int numdrawsurfs;


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
extern	byte dcollision[MAX_MAP_COLLISION];
extern	int dcollisiondatasize;
extern	int visdatasize;
extern	byte dvisdata[MAX_MAP_VISIBILITY];
extern	dvis_t *dvis;
extern	byte dlightgrid[MAX_MAP_LIGHTGRID];
extern	int numgridpoints;
extern	int lightdatasize;
extern	byte dlightdata[MAX_MAP_LIGHTDATA];

void	LoadMapFile ( void );
int	FindFloatPlane (vec3_t normal, vec_t dist);
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
void	FilterDetailBrushesIntoTree( bsp_entity_t *e, tree_t *tree );
void	FilterStructuralBrushesIntoTree( bsp_entity_t *e, tree_t *tree );
void	SplitBrush( bspbrush_t *brush, int planenum, bspbrush_t **front, bspbrush_t **back );

//
// surface.c
//

void	FilterDrawsurfsIntoTree( bsp_entity_t *e, tree_t *tree );
void	ClipSidesIntoTree( bsp_entity_t *e, tree_t *tree );
int	FilterFaceIntoTree( drawsurf_t *ds, tree_t *tree );
void	SubdivideDrawSurfs( bsp_entity_t *e, tree_t *tree );
void	FixTJunctions( bsp_entity_t *ent );

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

int LoadShaderInfo( void );
bsp_shader_t *FindShader( const char *texture );

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
int EmitShader( const char *shader );
void SetModelNumbers( void );
void SetLightStyles( void );
void BeginBSPFile( void );
void EndBSPFile( void );
void BeginModel( void );
void EndModel( node_t *headnode );

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
	bsp_shader_t	*si;

	vec3_t		origin;
	vec3_t		normal;		// for surfaces, spotlights, and suns
	float		dist;		// plane location along normal

	bool		linearLight;
	int		photons;
	int		style;
	vec3_t		color;
	float		radiusByDist;	// for spotlights

	bool		twosided;		// fog lights both sides

	winding_t		*w;
	vec3_t		emitColor;	// full out-of-gamut value
} light_t;

extern	float	lightscale;
extern	float	ambient;
extern	float	maxlight;
extern	float	direct_scale;
extern	float	entity_scale;
extern	bool	noSurfaces;

//===============================================================

//
// light_trace.c
//

// a facet is a subdivided element of a patch aproximation or model
typedef struct cFacet_s
{
	float		surface[4];
	int		numBoundaries;	// either 3 or 4, anything less is degenerate
	float		boundaries[4][4];	// positive is outside the bounds

	vec3_t		points[4];	// needed for area light subdivision
	float		textureMatrix[2][4];// compute texture coordinates at point of impact for translucency
} cFacet_t;

typedef struct
{
	vec3_t		mins, maxs;
	vec3_t		origin;
	float		radius;

	int		numFacets;
	cFacet_t		*facets;
	bsp_shader_t	*shader;		// for translucency
} surfaceTest_t;

typedef struct
{
	vec3_t		filter;		// starts out 1.0, 1.0, 1.0, may be reduced if
					// transparent surfaces are crossed
	vec3_t		hit;		// the impact point of a completely opaque surface
	float		hitFraction;	// 0 = at start, 1.0 = at end
	bool		passSolid;
} lighttrace_t;

extern	surfaceTest_t	*surfaceTest[MAX_MAP_SURFACES];

void	InitTrace( void );

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

void TraceLine( const vec3_t start, const vec3_t stop, lighttrace_t *trace, bool testAll, traceWork_t *tw );
bool PointInSolid( vec3_t start );

typedef struct
{
	int		textureNum;
	int		x, y, width, height;

	// for faces
	vec3_t		origin;
	vec3_t		vecs[3];
} lightmap_t;


bool PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c );

#endif//BSPLIB_H