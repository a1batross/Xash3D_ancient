//=======================================================================
//			Copyright XashXT Group 2007 ©
//			   bsplib.h - bsplib header
//=======================================================================
#ifndef BSPLIB_H
#define BSPLIB_H

#include "platform.h"
#include <basemath.h>
#include <bspmodel.h>
#include <materials.h>
#include "baseutils.h"


extern int runthreads;

#define VALVE_FORMAT	220
#define MAX_BRUSH_SIDES	128
#define CLIP_EPSILON	0.1
#define BOGUS_RANGE		8192
#define TEXINFO_NODE	-1		// side is allready on a node
#define MAX_PORTALS		32768
#define PORTALFILE		"PRT1"
#define ON_EPSILON		0.1
#define MAX_POINTS_ON_WINDING	64
#define PLANENUM_LEAF	-1
#define MAXEDGES		20
#define MAX_NODE_BRUSHES	8
#define MAX_PORTALS_ON_LEAF	128
#define MAX_MAP_SIDES	(MAX_MAP_BRUSHES*6)
#define MAX_MAP_TEXTURES	1024
#define MAX_TEXTURE_FRAMES	256
#define MAX_PATCHES		65000		// larger will cause 32 bit overflows

extern bool full_compile;

extern bool onlyents;
extern bool onlyvis;
extern bool onlyrad;

// bsplib export functions
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
} entity_t;

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
}qrk_vects;

typedef union
{
	std_vects		valve;
	qrk_vects		quark;
} vects_u;

typedef struct
{
	char	name[128];
	int	surfaceFlags;
	int	contents;
	vec3_t	color;
	int	intensity;
	char	nextframe[128];

	bool	hasPasses;
} shader_t;

typedef struct
{
	char		txcommand;
	vects_u		vects;
	char		name[32];
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
	bool	visible;		// choose visble planes first
	bool	tested;			// this plane allready checked as a split
	bool	bevel;			// don't ever use for bsp splitting
} side_t;

typedef struct brush_s
{
	int		entitynum;
	int		brushnum;

	int		contents;

	vec3_t	mins, maxs;

	int		numsides;
	side_t	*original_sides;
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
	vec3_t	mins, maxs;
	int		side, testside;		// side of node during construction
	mapbrush_t	*original;
	int		numsides;
	side_t	sides[6];			// variably sized
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
	entity_t		*occupant;	// for leak file testing
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
extern	entity_t		entities[MAX_MAP_ENTITIES];

void ParseEntities (void);
void UnparseEntities (void);

void SetKeyValue (entity_t *ent, char *key, char *value);
char *ValueForKey (entity_t *ent, char *key);
// will return "" if not present

vec_t FloatForKey (entity_t *ent, char *key);
void  GetVectorForKey (entity_t *ent, char *key, vec3_t vec);
epair_t *ParseEpair (void);


extern	int entity_num;

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
extern	texinfo_t	texinfo[MAX_MAP_TEXINFO];
extern	int numfaces;
extern	dface_t dfaces[MAX_MAP_FACES];
extern	int numedges;
extern	dedge_t dedges[MAX_MAP_EDGES];
extern	int numleaffaces;
extern	unsigned short	dleaffaces[MAX_MAP_LEAFFACES];
extern	int numleafbrushes;
extern	unsigned short	dleafbrushes[MAX_MAP_LEAFBRUSHES];
extern	int numsurfedges;
extern	int dsurfedges[MAX_MAP_SURFEDGES];
extern	int numareas;
extern	darea_t dareas[MAX_MAP_AREAS];
extern	int numareaportals;
extern	dareaportal_t dareaportals[MAX_MAP_AREAPORTALS];
extern	int numbrushes;
extern	dbrush_t	dbrushes[MAX_MAP_BRUSHES];
extern	int numbrushsides;
extern	dbrushside_t dbrushsides[MAX_MAP_BRUSHSIDES];

extern	char outbase[32];
extern	byte dpop[256];

void 	LoadMapFile ( void );
int	FindFloatPlane (vec3_t normal, vec_t dist);
bool	LoadBSPFile ( void );
void	LoadBSPFileTexinfo (char *filename);	// just for qdata
void	WriteBSPFile ( void );
void	DecompressVis (byte *in, byte *decompressed);
int	CompressVis (byte *vis, byte *dest);

//=============================================================================
// textures.c

typedef struct
{
	char	name[64];
	int	flags;
	int	value;
	int	contents;
	int	numframes;
	char	animname[64];
} textureref_t;

extern	textureref_t	textureref[MAX_MAP_TEXTURES];

int FindMiptex (char *name);
int TexinfoForBrushTexture (plane_t *plane, brush_texture_t *bt, vec3_t origin);

//=============================================================================

void FindGCD (int *v);

mapbrush_t *Brush_LoadEntity (entity_t *ent);
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
int	WindingOnPlaneSide (winding_t *w, vec3_t normal, vec_t dist);
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
shader_t *FindShader( char *texture );

//=============================================================================
// leakfile.c

void LeakFile (tree_t *tree);

//=============================================================================
// prtfile.c

void WritePortalFile (tree_t *tree);

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

extern	byte	*uncompressed;

extern	int	leafbytes, leaflongs;
extern	int	portalbytes, portallongs;


void LeafFlow (int leafnum);
void BasePortalVis (int portalnum);
void BetterPortalVis (int portalnum);
void PortalFlow (int portalnum);

extern	visportal_t	*sorted_portals[MAX_MAP_PORTALS*2];

int CountBits (byte *bits, int numbits);

//=============================================================================

// rad.c

typedef enum
{
	emit_surface,
	emit_point,
	emit_spotlight
} emittype_t;



typedef struct directlight_s
{
	struct directlight_s *next;
	emittype_t	type;

	float		intensity;
	int			style;
	vec3_t		origin;
	vec3_t		color;
	vec3_t		normal;		// for surfaces and spotlights
	float		stopdot;		// for spotlights
} directlight_t;


// the sum of all tranfer->transfer values for a given patch
// should equal exactly 0x10000, showing that all radiance
// reaches other patches
typedef struct
{
	unsigned short	patch;
	unsigned short	transfer;
} transfer_t;




typedef struct patch_s
{
	winding_t	*winding;
	struct patch_s		*next;		// next in face
	int			numtransfers;
	transfer_t	*transfers;

	int			cluster;			// for pvs checking
	vec3_t		origin;
	dplane_t	*plane;

	bool	sky;

	vec3_t		totallight;			// accumulated by radiosity
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

extern	patch_t		*face_patches[MAX_MAP_FACES];
extern	entity_t	*face_entity[MAX_MAP_FACES];
extern	vec3_t		face_offset[MAX_MAP_FACES];		// for rotating bmodels
extern	patch_t		patches[MAX_PATCHES];
extern	unsigned	num_patches;

extern	int		leafparents[MAX_MAP_LEAFS];
extern	int		nodeparents[MAX_MAP_NODES];

extern	float	lightscale;
extern	float	ambient;

void MakeShadowSplits (void);

//==============================================


void BuildVisMatrix (void);
bool CheckVisBit (unsigned p1, unsigned p2);

//==============================================

extern	float ambient, maxlight;

void LinkPlaneFaces (void);

extern	bool	extrasamples;
extern int numbounce;

extern	directlight_t	*directlights[MAX_MAP_LEAFS];

extern	byte	nodehit[MAX_MAP_NODES];

void BuildLightmaps (void);

void BuildFacelights (int facenum);

void FinalLightFace (int facenum);

bool PvsForOrigin (vec3_t org, byte *pvs);

int TestLine_r (int node, vec3_t start, vec3_t stop);

void CreateDirectLights (void);

dleaf_t	*RadPointInLeaf (vec3_t point);


extern	dplane_t	backplanes[MAX_MAP_PLANES];
extern	int			fakeplanes;// created planes for origin offset 

extern	float	subdiv;

extern	float	direct_scale;
extern	float	entity_scale;

int	PointInLeafnum (vec3_t point);
void MakeTnodes (dmodel_t *bm);
void MakePatches (void);
void SubdividePatches (void);
void PairEdges (void);
void CalcTextureReflectivity (void);

#endif//BSPLIB_H