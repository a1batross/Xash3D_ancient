//=======================================================================
//			Copyright XashXT Group 2007 ©
//			bspmodel.h - world model header
//=======================================================================
#ifndef BSPMODEL_H
#define BSPMODEL_H

/*
==============================================================================

BRUSH MODELS
==============================================================================
*/

//header
#define BSPMOD_VERSION	38
#define IDBSPMODHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I') // little-endian "IBSP"

// 16 bit short limits
#define MAX_KEY			32
#define MAX_MAP_AREAS		256
#define MAX_VALUE			1024
#define MAX_MAP_MODELS		1024
#define MAX_MAP_AREAPORTALS		1024
#define MAX_MAP_ENTITIES		2048
#define MAX_MAP_TEXINFO		8192
#define MAX_MAP_BRUSHES		16384
#define MAX_MAP_PLANES		65536
#define MAX_MAP_NODES		65536
#define MAX_MAP_BRUSHSIDES		65536
#define MAX_MAP_LEAFS		65536
#define MAX_MAP_VERTS		65536
#define MAX_MAP_FACES		65536
#define MAX_MAP_LEAFFACES		65536
#define MAX_MAP_LEAFBRUSHES		65536
#define MAX_MAP_PORTALS		65536
#define MAX_MAP_EDGES		128000
#define MAX_MAP_SURFEDGES		256000
#define MAX_MAP_ENTSTRING		0x40000
#define MAX_MAP_LIGHTING		0x400000
#define MAX_MAP_VISIBILITY		0x100000

typedef struct
{
	int	fileofs;
	int	filelen;
} lump_t;

//lump offset
#define LUMP_ENTITIES		0
#define LUMP_PLANES			1
#define LUMP_VERTEXES		2
#define LUMP_VISIBILITY		3
#define LUMP_NODES			4
#define LUMP_TEXINFO		5
#define LUMP_FACES			6
#define LUMP_LIGHTING		7
#define LUMP_LEAFS			8
#define LUMP_LEAFFACES		9
#define LUMP_LEAFBRUSHES		10
#define LUMP_EDGES			11
#define LUMP_SURFEDGES		12
#define LUMP_MODELS			13
#define LUMP_BRUSHES		14
#define LUMP_BRUSHSIDES		15
#define LUMP_POP			16
#define LUMP_AREAS			17
#define LUMP_AREAPORTALS		18
#define HEADER_LUMPS		19


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
	lump_t	lumps[HEADER_LUMPS];
} dheader_t;

typedef struct
{
	float	mins[3], maxs[3];
	float	origin[3];	// for sounds or lights
	int	headnode;
	int	firstface;	// submodels just draw faces 
	int	numfaces;		// without walking the bsp tree
} dmodel_t;

typedef struct
{
	float	point[3];
} dvertex_t;

typedef struct
{
	float	normal[3];
	float	dist;
	int	type;		// PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate
} dplane_t;

typedef struct
{
	int	planenum;
	int	children[2];	// negative numbers are -(leafs+1), not nodes
	short	mins[3];		// for frustom culling
	short	maxs[3];
	word	firstface;
	word	numfaces;		// counting both sides
} dnode_t;

typedef struct texinfo_s
{
	float	vecs[2][4];	// [s/t][xyz offset]
	int	flags;		// miptex flags + overrides
	int	value;		// light emission, etc
	char	texture[32];	// texture name (textures/*.jpg)
	int	nexttexinfo;	// for animations, -1 = end of chain
} texinfo_t;

typedef struct
{
	word	v[2];		// vertex numbers
} dedge_t;


typedef struct
{
	word	planenum;
	short	side;
	int	firstedge;	// we must support > 64k edges
	short	numedges;	
	short	texinfo;

	// lighting info
	byte	styles[MAXLIGHTMAPS];
	int	lightofs;		// start of [numstyles*surfsize] samples
} dface_t;

typedef struct
{
	int	contents;		// OR of all brushes (not needed?)
	short	cluster;
	short	area;
	short	mins[3];		// for frustum culling
	short	maxs[3];

	word	firstleafface;
	word	numleaffaces;
	word	firstleafbrush;
	word	numleafbrushes;
} dleaf_t;

typedef struct
{
	word	planenum;		// facing out of the leaf
	short	texinfo;
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

/*
==============================================================================

ENGINE TRACE FORMAT
==============================================================================
*/
typedef struct cplane_s
{
	vec3_t	normal;
	float	dist;
	byte	type;		// for fast side tests
	byte	signbits;		// signx + (signy<<1) + (signz<<1)
	byte	pad[2];
} cplane_t;


typedef struct cmodel_s
{
	vec3_t	mins, maxs;
	vec3_t	origin;		// for sounds or lights
	int	headnode;

	void	*extradata;	//for studio models
} cmodel_t;

typedef struct csurface_s
{
	char	name[16];
	int	flags;
	int	value;
} csurface_t;

typedef struct mapsurface_s
{
	csurface_t c;
	char	rname[32];
} mapsurface_t;

#endif//BSPMODEL_H