//=======================================================================
//			Copyright XashXT Group 2007 ©
//			   bsplib.h - bsplib header
//=======================================================================
#ifndef BSPLIB_H
#define BSPLIB_H

#include "platform.h"
#include "engine_api.h"
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

#define VALVE_FORMAT	220
#define MAX_BRUSH_SIDES	128
#define BOGUS_RANGE		MAX_WORLD_COORD
#define TEXINFO_NODE	-1		// side is allready on a node
#define MAX_PORTALS		32768
#define PORTALFILE		"PRT1"
#define MAX_POINTS_ON_WINDING	64
#define PLANENUM_LEAF	-1
#define MAXEDGES		20
#define MAX_NODE_BRUSHES	8
#define MAX_PORTALS_ON_LEAF	128
#define MAX_MAP_SIDES	(MAX_MAP_BRUSHES*6)
#define MAX_TEXTURE_FRAMES	256
#define MAX_PATCHES		65000		// larger will cause 32 bit overflows

// compile parms
typedef enum
{
	BSPLIB_MAKEBSP	= BIT(0),		// create a bsp file
	BSPLIB_MAKEVIS	= BIT(1),		// do visibility
	BSPLIB_MAKEHLRAD	= BIT(2),		// do half-life radiosity
	BSPLIB_MAKEQ2RAD	= BIT(3),		// do quake2 radiosity
	BSPLIB_FULLCOMPILE	= BIT(4),		// equals -full for vis, -extra for rad or light
	BSPLIB_ONLYENTS	= BIT(5),		// update only ents lump
	BSPLIB_RAD_NOPVS	= BIT(6),		// ignore pvs while processing radiocity (kill smooth light)
	BSPLIB_RAD_NOBLOCK	= BIT(7),
	BSPLIB_RAD_NOCOLOR	= BIT(8),
	BSPLIB_DELETE_TEMP	= BIT(9),		// delete itermediate files
	BSPLIB_SHOWINFO	= BIT(10),
} bsplibFlags_t;

extern uint bsp_parms;
extern char path[MAX_SYSPATH];
extern cvar_t *bsplib_compress_bsp;

// bsplib export functions
void WradMain( void );
void WvisMain( void );
void WbspMain( void );

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
	vec3_t		p[4];		// variable sized
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
	int		firstbrush;
	int		numbrushes;
	epair_t		*epairs;

	// only valid for func_areaportals
	int		areaportalnum;
	int		portalareas[2];
} bsp_entity_t;

typedef struct
{
	vec3_t		UAxis;
	vec3_t		VAxis;
	float		shift[2];
	float		rotate;
	float		scale[2];
} wrl_vecs;

typedef struct
{
	float		vecs[2][4];
} qrk_vecs;

typedef struct
{
	float		matrix[2][3];
} q3a_vecs;

typedef struct
{
	string	name;
	int	surfaceFlags;
	int	contents;
	vec3_t	color;
	int	intensity;

	bool	hasPasses;
} bsp_shader_t;

typedef union
{
	qrk_vecs	quark;
	wrl_vecs	hammer;
	q3a_vecs	radiant;
} vects_u;

typedef struct
{
	vects_u		vects;
	string		name;
	int		size[2];
	int		brush_type;
	int		contents;
	int		flags;
	int		value;
} brush_texture_t;

typedef struct side_s
{
	int		planenum;
	int		texinfo;
	winding_t		*winding;
	struct side_s	*original;	// bspbrush_t sides will reference the mapbrush_t sides
	int		contents;		// from miptex
	int		surf;		// from miptex
	bool		visible;		// choose visble planes first
	bool		tested;		// this plane allready checked as a split
	bool		bevel;		// don't ever use for bsp splitting
} side_t;

typedef struct mapbrush_s
{
	int		entitynum;
	int		brushnum;
	int		contents;

	int		shadernum;
	vec3_t		mins, maxs;
	int		numsides;
	side_t		*original_sides;
} mapbrush_t;

typedef struct face_s
{
	struct face_s	*next;		// on node

	// the chain of faces off of a node can be merged or split,
	// but each face_t along the way will remain in the chain
	// until the entire tree is freed
	struct face_s	*merged;	// if set, this face isn't valid anymore
	struct face_s	*split[2];	// if set, this face isn't valid anymore

	struct portal_s	*portal;
	int		texinfo;
	int		planenum;
	int		contents;	// faces in different contents can't merge
	int		outputnumber;
	winding_t		*w;
	int		numpoints;
	bool		badstartvert;	// tjunctions cannot be fixed without a midpoint vertex
	int		vertexnums[MAXEDGES];
} face_t;

typedef struct bspbrush_s
{
	struct bspbrush_s	*next;
	vec3_t		mins, maxs;
	int		side, testside;		// side of node during construction
	mapbrush_t	*original;
	int		numsides;
	side_t		sides[6];			// variably sized
} bspbrush_t;

typedef struct node_s
{
	// both leafs and nodes
	int				planenum;	// -1 = leaf node
	struct node_s	*parent;
	vec3_t			mins, maxs;	// valid after portalization
	bspbrush_t		*volume;	// one for each leaf/node

	// nodes only
	bool		detail_seperator;	// a detail brush caused the split
	side_t			*side;		// the side that created the node
	struct node_s	*children[2];
	face_t			*faces;

	// leafs only
	bspbrush_t		*brushlist;	// fragments of all brushes in this leaf
	int				contents;	// OR of all brush contents
	int				occupied;	// 1 or greater can reach entity
	bsp_entity_t		*occupant;	// for leak file testing
	int				cluster;	// for portalfile writing
	int				area;		// for areaportals
	struct portal_s	*portals;	// also on nodes during construction
} node_t;

typedef struct portal_s
{
	plane_t		plane;
	node_t		*onnode;		// NULL = outside box
	node_t		*nodes[2];		// [0] = front side of plane
	struct portal_s	*next[2];
	winding_t	*winding;

	bool	sidefound;		// false if ->side hasn't been checked
	side_t		*side;			// NULL = non-visible
	face_t		*face[2];		// output face in bsp file
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
} visplane_t;

typedef struct
{
	bool		original;				// don't free, it's part of the portal
	int		numpoints;
	vec3_t		points[12];	// variable sized
} viswinding_t;

typedef enum {stat_none, stat_working, stat_done} vstatus_t;
typedef struct
{
	visplane_t		plane;	// normal pointing into neighbor
	int			leaf;	// neighbor
	
	vec3_t		origin;	// for fast clip testing
	float		radius;

	viswinding_t	*winding;
	vstatus_t	status;
	byte		*portalfront;	// [portals], preliminary
	byte		*portalflood;	// [portals], intermediate
	byte		*portalvis;		// [portals], final

	int			nummightsee;	// bit count on portalflood for sort
} visportal_t;

typedef struct seperating_plane_s
{
	struct seperating_plane_s *next;
	visplane_t		plane;		// from portal is on positive side
} sep_t;


typedef struct passage_s
{
	struct passage_s	*next;
	int			from, to;		// leaf numbers
	sep_t				*planes;
} passage_t;

typedef struct leaf_s
{
	int			numportals;
	passage_t	*passages;
	visportal_t	*portals[MAX_PORTALS_ON_LEAF];
} leaf_t;

	
typedef struct pstack_s
{
	byte		mightsee[MAX_PORTALS/8];		// bit string
	struct pstack_s	*next;
	leaf_t		*leaf;
	visportal_t	*portal;		// portal exiting
	viswinding_t	*source;
	viswinding_t	*pass;

	viswinding_t	windings[3];	// source, pass, temp in any order
	int		freewindings[3];

	visplane_t	portalplane;
} pstack_t;

typedef struct
{
	visportal_t	*base;
	int		c_chains;
	pstack_t		pstack_head;
} threaddata_t;

extern	int		num_entities;
extern	bsp_entity_t	entities[MAX_MAP_ENTITIES];
extern	script_t		*mapfile;
extern	file_t		*bsplog;

void	ParseEntities( void );
void	UnparseEntities( void );
void	Com_CheckToken( script_t *script, const char *match );
void	Com_Parse1DMatrix( script_t *script, int x, vec_t *m );
void	Com_Parse2DMatrix( script_t *script, int y, int x, vec_t *m );
void	SetKeyValue( bsp_entity_t *ent, const char *key, const char *value );
char	*ValueForKey( const bsp_entity_t *ent, const char *key ); // will return "" if not present
vec_t	FloatForKey( const bsp_entity_t *ent, const char *key );
long	IntForKey( const bsp_entity_t *ent, const char *key );
void	GetVectorForKey( const bsp_entity_t *ent, const char *key, vec3_t vec );
bsp_entity_t *FindTargetEntity( const char *target );
void	BSP_PrintLog( const char *pMsg );
void	FindMapMessage( char *message );
epair_t	*ParseEpair( token_t *token );
void	PrintBSPFileSizes( void );

extern	int entity_num;
extern	int g_mapversion;
extern	plane_t mapplanes[MAX_MAP_PLANES];
extern	int nummapplanes;
extern	int nummapbrushes;
extern	mapbrush_t mapbrushes[MAX_MAP_BRUSHES];
extern	vec3_t map_mins, map_maxs;
extern	int nummapbrushsides;
extern	side_t brushsides[MAX_MAP_SIDES];
extern	int nummodels;
extern	dmodel_t	dmodels[MAX_MAP_MODELS];
extern	int visdatasize;
extern	byte dvisdata[MAX_MAP_VISIBILITY];
extern	dvis_t *dvis;
extern	int lightdatasize;
extern	byte dlightdata[MAX_MAP_LIGHTING];
extern	int entdatasize;
extern	char dentdata[MAX_MAP_ENTSTRING];
extern	int numleafs;
extern	dleaf_t dleafs[MAX_MAP_LEAFS];
extern	int numplanes;
extern	dplane_t	dplanes[MAX_MAP_PLANES];
extern	int numvertexes;
extern	dvertex_t	dvertexes[MAX_MAP_VERTS];
extern	int numnodes;
extern	dnode_t dnodes[MAX_MAP_NODES];
extern	int numtexinfo;
extern	dtexinfo_t texinfo[MAX_MAP_TEXINFO];
extern	int numsurfaces;
extern	dsurface_t dsurfaces[MAX_MAP_SURFACES];
extern	int numedges;
extern	dedge_t dedges[MAX_MAP_EDGES];
extern	int numleafsurfaces;
extern	dleafface_t dleafsurfaces[MAX_MAP_LEAFFACES];
extern	int numleafbrushes;
extern	dleafbrush_t dleafbrushes[MAX_MAP_LEAFBRUSHES];
extern	int numsurfedges;
extern	dsurfedge_t dsurfedges[MAX_MAP_SURFEDGES];
extern	int numareas;
extern	darea_t dareas[MAX_MAP_AREAS];
extern	int numareaportals;
extern	dareaportal_t dareaportals[MAX_MAP_AREAPORTALS];
extern	int numbrushes;
extern	dbrush_t	dbrushes[MAX_MAP_BRUSHES];
extern	int numbrushsides;
extern	dbrushside_t dbrushsides[MAX_MAP_BRUSHSIDES];
extern	int dcollisiondatasize;
extern	byte dcollision[MAX_MAP_COLLISION];
extern	dshader_t	dshaders[MAX_MAP_SHADERS];
extern	int numshaders;

void	LoadMapFile ( void );
int	FindFloatPlane (vec3_t normal, vec_t dist);
bool	LoadBSPFile ( void );
void	WriteBSPFile ( void );
void	DecompressVis (byte *in, byte *decompressed);
int	CompressVis (byte *vis, byte *dest);

//=============================================================================
// bsplib.c

void ProcessCollisionTree( void );

//=============================================================================
// textures.c

int FindMiptex( const char *name );
int TexinfoForBrushTexture( plane_t *plane, brush_texture_t *bt, vec3_t origin );

//=============================================================================

mapbrush_t *Brush_LoadEntity (bsp_entity_t *ent);
int	PlaneTypeForNormal (vec3_t normal);
bool	MakeBrushPlanes (mapbrush_t *b);
int	FindIntPlane (int *inormal, int *iorigin);
void	CreateBrush (int brushnum);

//=============================================================================

// csg

bspbrush_t *MakeBspBrushList (int startbrush, int endbrush,
		vec3_t clipmins, vec3_t clipmaxs);
bspbrush_t *ChopBrushes (bspbrush_t *head);
bspbrush_t *InitialBrushList (bspbrush_t *list);
bspbrush_t *OptimizedBrushList (bspbrush_t *list);

//=============================================================================

// brushbsp

void WriteBrushList (char *name, bspbrush_t *brush, bool onlyvis);
bspbrush_t *CopyBrush (bspbrush_t *brush);
void SplitBrush (bspbrush_t *brush, int planenum, bspbrush_t **front, bspbrush_t **back);

tree_t *AllocTree (void);
node_t *AllocNode (void);
bspbrush_t *AllocBrush (int numsides);
int CountBrushList (bspbrush_t *brushes);
void FreeBrush (bspbrush_t *brushes);
vec_t BrushVolume (bspbrush_t *brush);
void BoundBrush (bspbrush_t *brush);
void FreeBrushList (bspbrush_t *brushes);
tree_t *BrushBSP (bspbrush_t *brushlist, vec3_t mins, vec3_t maxs);

//=============================================================================

// winding.c

winding_t	*AllocWinding (int points);
vec_t	WindingArea (winding_t *w);
void	WindingCenter (winding_t *w, vec3_t center);
void	ClipWindingEpsilon (winding_t *in, vec3_t normal, vec_t dist, 
				vec_t epsilon, winding_t **front, winding_t **back);
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

// portals.c

int VisibleContents (int contents);

void MakeHeadnodePortals (tree_t *tree);
void MakeNodePortal (node_t *node);
void SplitNodePortals (node_t *node);

bool	Portal_VisFlood (portal_t *p);

bool FloodEntities (tree_t *tree);
void FillOutside (node_t *headnode);
void FloodAreas (tree_t *tree);
void MarkVisibleSides (tree_t *tree, int start, int end);
void FreePortal (portal_t *p);
void EmitAreaPortals (node_t *headnode);

void MakeTreePortals (tree_t *tree);

//=============================================================================
// shaders.c

int LoadShaderInfo( void );
bsp_shader_t *FindShader( const char *texture );

//=============================================================================
// leakfile.c

void LeakFile (tree_t *tree);

//=============================================================================
// prtfile.c

void NumberClusters( tree_t *tree );
void WritePortalFile( tree_t *tree );

//=============================================================================

// writebsp.c

void SetModelNumbers (void);
void SetLightStyles (void);

void BeginBSPFile (void);
void WriteBSP (node_t *headnode);
void EndBSPFile (void);
void BeginModel (void);
void EndModel (void);

//=============================================================================

// faces.c

void MakeFaces (node_t *headnode);
void FixTjuncs (node_t *headnode);
int GetEdge2 (int v1, int v2,  face_t *f);

face_t	*AllocFace (void);
void FreeFace (face_t *f);

void MergeNodeFaces (node_t *node);

//=============================================================================

// tree.c

void FreeTree (tree_t *tree);
void FreeTree_r (node_t *node);
void PrintTree_r (node_t *node, int depth);
void FreeTreePortals_r (node_t *node);
void PruneNodes_r (node_t *node);
void PruneNodes (node_t *node);

//=============================================================================

// vis.c

viswinding_t	*NewVisWinding (int points);
void		FreeVisWinding (viswinding_t *w);
viswinding_t	*CopyVisWinding (viswinding_t *w);

extern	int	numportals;
extern	int	portalclusters;

extern	visportal_t	*portals;
extern	leaf_t		*leafs;

extern	int	c_portaltest, c_portalpass, c_portalcheck;
extern	int	c_portalskip, c_leafskip;
extern	int	c_vistest, c_mighttest;
extern	int	c_chains;

extern	byte	*vismap, *vismap_p, *vismap_end;	// past visfile

extern	int	testlevel;

extern	byte	*uncompressedvis;

extern	int	leafbytes, leaflongs;
extern	int	portalbytes, portallongs;


void LeafFlow (int leafnum);
void BasePortalVis (int portalnum);
void BetterPortalVis (int portalnum);
void PortalFlow (int portalnum);

extern	visportal_t	*sorted_portals[MAX_MAP_PORTALS*2];

int CountBits( byte *bits, int numbits );
int PointInLeafnum( vec3_t point );
dleaf_t *PointInLeaf( vec3_t point );
bool PvsForOrigin( vec3_t org, byte *pvs );
byte *PhsForCluster( int cluster );
void CalcAmbientSounds( void );

//=============================================================================

// rad.c
#define LIGHTDISTBIAS	6800.0

typedef enum
{
	emit_surface,
	emit_point,
	emit_spotlight,
	emit_skylight
} emittype_t;

typedef struct tnode_s
{
	int		type;
	vec3_t		normal;
	float		dist;
	int		children[2];
	int		pad;
} tnode_t;

// the sum of all tranfer->transfer values for a given patch
// should equal exactly 0x10000, showing that all radiance
// reaches other patches
typedef struct
{
	word	patch;
	word	transfer;
} transfer_t;

typedef struct patch_s
{
	winding_t		*winding;
	struct patch_s	*next;		// next in face
	int		numtransfers;
	transfer_t	*transfers;
    
	int		cluster;			// for pvs checking
	vec3_t		origin;
	dplane_t		*plane;

	bool		sky;

	vec3_t		totallight;	// accumulated by radiosity
					// does NOT include light
					// accounted for by direct lighting
	float		area;

	// illuminance * reflectivity = radiosity
	vec3_t		reflectivity;
	vec3_t		baselight;			// emissivity only

	// each style 0 lightmap sample in the patch will be
	// added up to get the average illuminance of the entire patch
	vec3_t		samplelight;
	int			samples;		// for averaging direct light
} patch_t;

extern patch_t	*face_patches[MAX_MAP_SURFACES];
extern bsp_entity_t	*face_entity[MAX_MAP_SURFACES];
extern vec3_t	face_offset[MAX_MAP_SURFACES];		// for rotating bmodels
extern patch_t	patches[MAX_PATCHES];
extern tnode_t	*tnodes;
extern uint	num_patches;

extern int	leafparents[MAX_MAP_LEAFS];
extern int	nodeparents[MAX_MAP_NODES];

extern float	lightscale;
extern float	ambient;

void MakeShadowSplits (void);

//==============================================

extern	float ambient, maxlight;

void LinkPlaneFaces (void);

extern int numbounce;

extern	byte	nodehit[MAX_MAP_NODES];

void BuildLightmaps (void);

void BuildFacelights (int facenum);

void FinalLightFace (int facenum);
int TestLine_r (int node, vec3_t start, vec3_t stop);

void CreateDirectLights (void);

extern	dplane_t	backplanes[MAX_MAP_PLANES];
extern	int	fakeplanes;// created planes for origin offset 

extern	float	subdiv;

extern	float	direct_scale;
extern	float	entity_scale;

void MakeTnodes (dmodel_t *bm);
void MakePatches (void);
void SubdividePatches (void);
void PairEdges (void);
void CalcTextureReflectivity (void);

#endif//BSPLIB_H