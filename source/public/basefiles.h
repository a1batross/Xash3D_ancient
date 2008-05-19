//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        basefiles.h - xash supported formats
//=======================================================================
#ifndef BASE_FILES_H
#define BASE_FILES_H

/*
==============================================================================

SPRITE MODELS

.spr extended version (non-paletted 32-bit sprites with zlib-compression for each frame)
==============================================================================
*/

#define IDSPRITEHEADER		(('R'<<24)+('P'<<16)+('S'<<8)+'I')	// little-endian "ISPR"
#define SPRITE_VERSION		3

typedef enum
{
	SPR_STATIC = 0,
	SPR_BOUNCE,
	SPR_GRAVITY,
	SPR_FLYING
} phystype_t;

typedef enum
{
	SPR_SINGLE = 0,
	SPR_GROUP,
	SPR_ANGLED
} frametype_t;

typedef enum
{
	SPR_SOLID = 0,
	SPR_ADDITIVE,
	SPR_GLOW,
	SPR_ALPHA
} drawtype_t;

typedef enum
{
	SPR_FWD_PARALLEL_UPRIGHT = 0,
	SPR_FACING_UPRIGHT,
	SPR_FWD_PARALLEL,
	SPR_ORIENTED,
	SPR_FWD_PARALLEL_ORIENTED
} angletype_t; 

typedef struct
{
	int		ident;		// LittleLong 'ISPR'
	int		version;		// current version 3
	angletype_t	type;		// camera align
	drawtype_t	rendermode;	// rendering mode
	int		bounds[2];	// minmaxs
	int		numframes;	// including groups
	phystype_t	movetype;		// particle physic
	float		scale;		// initial scale
} dsprite_t;

typedef struct
{
	int		origin[2];
	int		width;
	int		height;
	int		compsize;
} dframe_t;

typedef struct
{
	int		numframes;
} dspritegroup_t;

typedef struct
{
	float		interval;
} dspriteinterval_t;

typedef struct
{
	frametype_t	type;
} dframetype_t;

/*
==============================================================================
BRUSH MODELS

.bsp contain level static geometry with including PVS, PHS, PHYS and lightning info
==============================================================================
*/

// header
#define BSPMOD_VERSION	39
#define IDBSPMODHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I') // little-endian "IBSP"

// 32 bit limits
#define MAX_KEY			128
#define MAX_VALUE			512
#define MAX_MAP_AREAS		0x100	// don't increase this
#define MAX_MAP_MODELS		0x2000	// mesh models and sprites too
#define MAX_MAP_AREAPORTALS		0x400
#define MAX_MAP_ENTITIES		0x2000
#define MAX_MAP_TEXINFO		0x2000
#define MAX_MAP_BRUSHES		0x8000
#define MAX_MAP_PLANES		0x20000
#define MAX_MAP_NODES		0x20000
#define MAX_MAP_BRUSHSIDES		0x20000
#define MAX_MAP_LEAFS		0x20000
#define MAX_MAP_VERTS		0x80000
#define MAX_MAP_FACES		0x20000
#define MAX_MAP_LEAFFACES		0x20000
#define MAX_MAP_LEAFBRUSHES		0x40000
#define MAX_MAP_PORTALS		0x20000
#define MAX_MAP_EDGES		0x80000
#define MAX_MAP_SURFEDGES		0x80000
#define MAX_MAP_ENTSTRING		0x80000
#define MAX_MAP_LIGHTING		0x800000
#define MAX_MAP_VISIBILITY		0x800000
#define MAX_MAP_COLLISION		0x800000
#define MAX_MAP_STRINGDATA		0x40000
#define MAX_MAP_NUMSTRINGS		0x10000

// game limits
#define MAX_MODELS			MAX_MAP_MODELS>>1	// brushmodels and other models
#define MAX_WORLD_COORD		( 128 * 1024 )
#define MIN_WORLD_COORD		(-128 * 1024 )
#define WORLD_SIZE			( MAX_WORLD_COORD - MIN_WORLD_COORD )

// lump offset
#define LUMP_ENTITIES		0
#define LUMP_PLANES			1
#define LUMP_LEAFS			2
#define LUMP_LEAFFACES		3
#define LUMP_LEAFBRUSHES		4
#define LUMP_NODES			5
#define LUMP_VERTEXES		6
#define LUMP_EDGES			7
#define LUMP_SURFEDGES		8
#define LUMP_SURFDESC		9
#define LUMP_FACES			10
#define LUMP_MODELS			11
#define LUMP_BRUSHES		12
#define LUMP_BRUSHSIDES		13
#define LUMP_VISIBILITY		14
#define LUMP_LIGHTING		15
#define LUMP_COLLISION		16	// newton collision tree (worldmodel coords already convert to meters)
#define LUMP_BVHSTATIC		17	// bullet collision tree (currently not used)
#define LUMP_SVPROGS		18	// private server.dat for current map
#define LUMP_WAYPOINTS		19	// AI navigate tree (like .aas file for quake3)		
#define LUMP_STRINGDATA		20	// string array
#define LUMP_STRINGTABLE		21	// string table id's

// get rid of this
#define LUMP_AREAS			22
#define LUMP_AREAPORTALS		23

#define LUMP_TOTALCOUNT		32	// max lumps


// the visibility lump consists of a header with a count, then
// byte offsets for the PVS and PHS of each cluster, then the raw
// compressed bit vectors
#define DVIS_PVS			0
#define DVIS_PHS			1

//other limits
#define MAXLIGHTMAPS		4

typedef struct
{
	int	ident;
	int	version;	
	lump_t	lumps[LUMP_TOTALCOUNT];
} dheader_t;

typedef struct
{
	float	mins[3];
	float	maxs[3];
	int	headnode;
	int	firstface;	// submodels just draw faces 
	int	numfaces;		// without walking the bsp tree
	int	firstbrush;	// physics stuff
	int	numbrushes;
} dmodel_t;

typedef struct
{
	float	point[3];
} dvertex_t;

typedef struct
{
	float	normal[3];
	float	dist;
} dplane_t;

typedef struct
{
	int	planenum;
	int	children[2];	// negative numbers are -(leafs+1), not nodes
	int	mins[3];		// for frustom culling
	int	maxs[3];
	int	firstface;
	int	numfaces;		// counting both sides
} dnode_t;

typedef struct dsurfdesc_s
{
	float	vecs[2][4];	// [s/t][xyz offset] texture s\t
	int	size[2];		// valid size for current s\t coords (used for replace texture)
	int	texid;		// string table texture id number
	int	animid;		// string table animchain id number 
	int	flags;		// surface flags
	int	value;		// used by qrad, not engine
} dsurfdesc_t;

typedef struct
{
	int	v[2];		// vertex numbers
} dedge_t;

typedef struct
{
	int	planenum;
	int	firstedge;
	int	numedges;	
	int	desc;

	// lighting info
	byte	styles[MAXLIGHTMAPS];
	int	lightofs;		// start of [numstyles*surfsize] samples

	// get rid of this
	short	side;
} dface_t;

typedef struct
{
	int	contents;		// or of all brushes (not needed?)
	int	cluster;
	int	area;
	int	mins[3];		// for frustum culling
	int	maxs[3];
	int	firstleafface;
	int	numleaffaces;
	int	firstleafbrush;
	int	numleafbrushes;
} dleaf_t;

typedef struct
{
	int	planenum;		// facing out of the leaf
	int	surfdesc;		// surface description (s/t coords, flags, etc)
} dbrushside_t;

typedef struct
{
	int	firstside;
	int	numsides;
	int	contents;
} dbrush_t;

typedef struct
{
	int	numclusters;
	int	bitofs[8][2];	// bitofs[numclusters][2]
} dvis_t;

typedef struct
{
	int	portalnum;
	int	otherarea;
} dareaportal_t;

typedef struct
{
	int	numareaportals;
	int	firstareaportal;
} darea_t;

#endif//BASE_FILES_H