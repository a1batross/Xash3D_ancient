//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        qfiles_ref.h - xash supported formats
//=======================================================================
#ifndef REF_DFILES_H
#define REF_DFILES_H

/*
========================================================================
.WAD archive format	(WhereAllData - WAD)

List of compressed files, that can be identify only by TYPE_*

<format>
header:	dwadinfo_t[dwadinfo_t]
file_1:	byte[dwadinfo_t[num]->disksize]
file_2:	byte[dwadinfo_t[num]->disksize]
file_3:	byte[dwadinfo_t[num]->disksize]
...
file_n:	byte[dwadinfo_t[num]->disksize]
infotable	dlumpinfo_t[dwadinfo_t->numlumps]
========================================================================
*/

#define IDWAD3HEADER	(('3'<<24)+('D'<<16)+('A'<<8)+'W')

// dlumpinfo_t->compression
#define CMP_NONE			0	// compression none
#define CMP_LZSS			1	// currently not used
#define CMP_ZLIB			2	// zip-archive compression

// dlumpinfo_t->type
#define TYPE_QPAL			64	// quake palette
#define TYPE_QTEX			65	// probably was never used
#define TYPE_QPIC			66	// quake1 and hl pic (lmp_t)
#define TYPE_MIPTEX			67	// half-life (mip_t) previous was TYP_SOUND but never used in quake1
#define TYPE_QMIP			68	// quake1 (mip_t) (replaced with TYPE_MIPTEX while loading)
#define TYPE_RAW			69	// raw data
#define TYPE_QFONT			70	// half-life font (qfont_t)
#define TYPE_BINDATA		71	// engine internal data (map lumps, save lumps etc)
#define TYPE_STRDATA		72	// stringdata type (stringtable marked as TYPE_BINDATA)
#define TYPE_SCRIPT			73	// .qc scripts (xash ext)

/*
==============================================================================

SPRITE MODELS

.spr extended version (Half-Life compatible sprites with some xash extensions)
==============================================================================
*/

#define IDSPRITEHEADER	(('P'<<24)+('S'<<16)+('D'<<8)+'I')	// little-endian "IDSP"
#define SPRITE_VERSION	2				// Half-Life sprites

typedef enum
{
	ST_SYNC = 0,
	ST_RAND
} synctype_t;

typedef enum
{
	FRAME_SINGLE = 0,
	FRAME_GROUP,
	FRAME_ANGLED			// xash ext
} frametype_t;

typedef enum
{
	SPR_NORMAL = 0,
	SPR_ADDITIVE,
	SPR_INDEXALPHA,
	SPR_ALPHTEST,
	SPR_ADDGLOW			// xash ext
} drawtype_t;

typedef enum
{
	SPR_FWD_PARALLEL_UPRIGHT = 0,
	SPR_FACING_UPRIGHT,
	SPR_FWD_PARALLEL,
	SPR_ORIENTED,
	SPR_FWD_PARALLEL_ORIENTED,
} angletype_t; 

typedef enum
{
	SPR_CULL_FRONT = 0,			// oriented sprite will be draw with one face
	SPR_CULL_NONE,			// oriented sprite will be draw back face too
} facetype_t;

typedef struct
{
	int		ident;		// LittleLong 'ISPR'
	int		version;		// current version 3
	angletype_t	type;		// camera align
	drawtype_t	texFormat;	// rendering mode (Xash3D ext)
	int		boundingradius;	// quick face culling
	int		bounds[2];	// minsmaxs
	int		numframes;	// including groups
	facetype_t	facetype;		// cullface (Xash3D ext)
	synctype_t	synctype;		// animation synctype
} dsprite_t;

typedef struct
{
	int		origin[2];
	int		width;
	int		height;
} dspriteframe_t;

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
========================================================================

.LMP image format	(Half-Life gfx.wad lumps)

========================================================================
*/
typedef struct lmp_s
{
	uint	width;
	uint	height;
} lmp_t;

/*
========================================================================

.MIP image format	(half-Life textures)

========================================================================
*/
typedef struct mip_s
{
	char	name[16];
	uint	width;
	uint	height;
	uint	offsets[4];	// four mip maps stored
} mip_t;

/*
==============================================================================
BRUSH MODELS

.bsp contain level static geometry with including PVS, PHS, PHYS and lightning info
==============================================================================
*/

// header
#define Q3IDBSP_VERSION	46
#define RTCWBSP_VERSION	47
#define IGIDBSP_VERSION	48	// extended brushides
#define XRIDBSP_VERSION	48	// x-real bsp version (extended verts and lightgrid)
#define RFIDBSP_VERSION	1	// both raven bsp and qfusion bsp

#define IDBSPMODHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I') // little-endian "IBSP"
#define RBBSPMODHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'R') // little-endian "RBSP"
#define QFBSPMODHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'F') // little-endian "FBSP"
#define XRBSPMODHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'X') // little-endian "XBSP"

// 32 bit limits
#define MAX_MAP_AREA_BYTES		MAX_MAP_AREAS / 8	// bit vector of area visibility
#define MAX_MAP_AREAS		0x100		// don't increase this
#define MAX_MAP_FOGS		0x100
#define MAX_MAP_MODELS		0x2000		// mesh models and sprites too
#define MAX_MAP_AREAPORTALS		0x400
#define MAX_MAP_ENTITIES		0x2000
#define MAX_MAP_SHADERS		0x1000
#define MAX_MAP_TEXINFO		0x8000
#define MAX_MAP_BRUSHES		0x8000
#define MAX_MAP_PLANES		0x20000
#define MAX_MAP_NODES		0x20000
#define MAX_MAP_BRUSHSIDES		0x20000
#define MAX_MAP_LEAFS		0x20000
#define MAX_MAP_VERTS		0x80000
#define MAX_MAP_SURFACES		0x20000
#define MAX_MAP_LEAFFACES		0x20000
#define MAX_MAP_LEAFBRUSHES		0x40000
#define MAX_MAP_PORTALS		0x20000
#define MAX_MAP_INDICES		0x80000
#define MAX_MAP_ENTSTRING		0x80000
#define MAX_MAP_LIGHTING		0x800000
#define MAX_MAP_VISIBILITY		0x800000
#define MAX_MAP_COLLISION		0x800000

// other limits
#define DVIS_PVS			0
#define DVIS_PHS			1
#define MAX_KEY			128
#define MAX_VALUE			512
#define MAX_WORLD_COORD		( 128 * 1024 )
#define MIN_WORLD_COORD		(-128 * 1024 )
#define WORLD_SIZE			( MAX_WORLD_COORD - MIN_WORLD_COORD )
#define MAX_SHADERPATH		64
#define NUMVERTEXNORMALS		162	// quake avertex normals

#define MAX_BUILD_SIDES		512	// per one brush. (don't touch!)
#define LM_SAMPLE_SIZE		16	// q1, q2, q3 default value (lightmap resoultion)
#define LM_STYLES			4	// MAXLIGHTMAPS
#define LM_BYTES			3	// RGB format
#define LS_NORMAL			0x00
#define LS_UNUSED			0xFE
#define LS_NONE			0xFF
#define MAX_LSTYLES			256
#define MAX_LIGHT_STYLES		64
#define MAX_SWITCHED_LIGHTS		32

// lump names
typedef struct
{
	int		fileofs, filelen;
} lump_t;

#define LUMP_ENTITIES		0
#define LUMP_SHADERS		1
#define LUMP_PLANES			2
#define LUMP_NODES			3
#define LUMP_LEAFS			4
#define LUMP_LEAFSURFACES		5
#define LUMP_LEAFBRUSHES		6
#define LUMP_MODELS			7
#define LUMP_BRUSHES		8
#define LUMP_BRUSHSIDES		9
#define LUMP_VERTEXES		10
#define LUMP_ELEMENTS		11
#define LUMP_FOGS			12
#define LUMP_SURFACES		13
#define LUMP_LIGHTING		14
#define LUMP_LIGHTGRID		15
#define LUMP_VISIBILITY		16
#define LUMP_LIGHTARRAY		17		// RBSP extra lump to save space
#define HEADER_LUMPS		18		// 17 for IDBSP

typedef enum
{
	SURF_NONE			= 0,		// just a mask for source tabulation
	SURF_NODAMAGE		= BIT(0),		// never give falling damage
	SURF_SLICK		= BIT(1),		// effects game physics
	SURF_SKY			= BIT(2),		// don't draw, but add to skybox
	SURF_LADDER		= BIT(3),		// this is ladder surface
	SURF_NOIMPACT		= BIT(4),		// don't make missile explosions
	SURF_NOMARKS		= BIT(5),		// don't leave missile marks
	SURF_WARP			= BIT(6),		// turbulent water warping
	SURF_NODRAW		= BIT(7),		// don't generate a drawsurface at all
	SURF_HINT			= BIT(8),		// make a primary bsp splitter
	SURF_SKIP			= BIT(9),		// completely ignore, allowing non-closed brushes
	SURF_NOLIGHTMAP		= BIT(10),	// don't place lightmap for this surface
	SURF_POINTLIGHT		= BIT(11),	// generate lighting info at vertexes
	SURF_METALSTEPS		= BIT(12),	// REMOVE? clanking footsteps
	SURF_NOSTEPS		= BIT(13),	// REMOVE? no footstep sounds
	SURF_NONSOLID		= BIT(14),	// don't collide against curves with this set
	SURF_LIGHTFILTER		= BIT(15),	// act as a light filter during q3map -light
	SURF_ALPHASHADOW		= BIT(16),	// do per-pixel light shadow casting in q3map
	SURF_NODLIGHT		= BIT(17),	// never add dynamic lights
	SURF_DUST			= BIT(18),	// REMOVE? leave a dust trail when walking on this surface
} surfaceFlags_t;

// bsp contents
typedef enum
{
	BASECONT_NONE		= 0, 		// just a mask for source tabulation
	BASECONT_SOLID		= BIT(0),		// an eye is never valid in a solid
                                                 		// reserved
	BASECONT_SKY		= BIT(2),
	BASECONT_LAVA		= BIT(3),
	BASECONT_SLIME		= BIT(4),
	BASECONT_WATER		= BIT(5),
	BASECONT_FOG		= BIT(6),
	
	// space for new user contents
	BASECONT_AREAPORTAL		= BIT(15),
	BASECONT_PLAYERCLIP		= BIT(16),
	BASECONT_MONSTERCLIP	= BIT(17),
	BASECONT_CLIP		= (BASECONT_PLAYERCLIP|BASECONT_MONSTERCLIP), // both type clip
	BASECONT_TELEPORTER		= BIT(18),
	BASECONT_JUMPPAD		= BIT(19),
	BASECONT_CLUSTERPORTAL	= BIT(20),
	BASECONT_DONOTENTER		= BIT(21),
	BASECONT_ORIGIN		= BIT(22),	// removed before bsping an entity
	BASECONT_BODY		= BIT(23),	// should never be on a brush, only in game
	BASECONT_CORPSE		= BIT(24),	// dead corpse
	BASECONT_DETAIL		= BIT(25),	// brushes not used for the bsp
	BASECONT_STRUCTURAL		= BIT(26),	// brushes used for the bsp
	BASECONT_TRANSLUCENT	= BIT(27),	// don't consume surface fragments inside
	BASECONT_LADDER		= BIT(28),	// ladder volume
	BASECONT_NODROP		= BIT(29),	// don't leave bodies or items (death fog, lava)

	// content masks
	MASK_SOLID		= (BASECONT_SOLID|BASECONT_BODY),
	MASK_PLAYERSOLID		= (BASECONT_SOLID|BASECONT_PLAYERCLIP|BASECONT_BODY|BASECONT_LADDER),
	MASK_MONSTERSOLID		= (BASECONT_SOLID|BASECONT_MONSTERCLIP|BASECONT_BODY|BASECONT_LADDER),
	MASK_DEADSOLID		= (BASECONT_SOLID|BASECONT_PLAYERCLIP),
	MASK_WATER		= (BASECONT_WATER|BASECONT_LAVA|BASECONT_SLIME),
	MASK_OPAQUE		= (BASECONT_SOLID|BASECONT_SLIME|BASECONT_LAVA),
	MASK_SHOT			= (BASECONT_SOLID|BASECONT_BODY|BASECONT_CORPSE)
} contentType_t;

typedef enum
{
	MST_BAD = 0,
	MST_PLANAR,
	MST_PATCH,
	MST_TRISURF,
	MST_FLARE,
	MST_FOLIAGE
} bspSurfaceType_t;

typedef struct
{
	int	ident;
	int	version;	
	lump_t	lumps[HEADER_LUMPS];
} dheader_t;

typedef struct
{
	vec3_t	mins;
	vec3_t	maxs;
	int	firstsurface;	// submodels just draw faces 
	int	numsurfaces;	// without walking the bsp tree
	int	firstbrush;	// physics stuff
	int	numbrushes;
} dmodel_t;

typedef struct
{
	char	name[64];		// shader name
	int	surfaceFlags;	// surface flags (can be replaced)
	int	contentFlags;	// texture contents (can be replaced)
} dshader_t;

typedef struct
{
	vec3_t	point;
	vec2_t	tex_st;		// texture coords
	vec2_t	lm_st;		// lightmap texture coords
	vec3_t	normal;		// normal
	byte	color[4];		// color used for vertex lighting
} dvertexq_t;	// IBSP

typedef struct
{
	vec3_t	point;
	vec2_t	tex_st;
	vec2_t	lm_st[LM_STYLES];
	vec3_t	normal;
	byte	color[LM_STYLES][4];
} dvertexr_t;	// RBSP

typedef struct
{
	vec3_t	point;
	vec2_t	tex_st;
	vec2_t	lm_st;
	vec3_t	normal;
	vec4_t	paintcolor;
	vec4_t	lightcolor;
	vec3_t	lightdir;
} dvertexx_t;	// XBSP

typedef struct
{
	float	normal[3];
	float	dist;
} dplane_t;

typedef struct
{
	int	planenum;
	int	children[2];	// negative numbers are -(leafs+1), not nodes
	int	mins[3];		// for frustum culling
	int	maxs[3];
} dnode_t;

typedef dword	dleafface_t;
typedef dword	dleafbrush_t;

typedef struct
{
	int	cluster;
	int	area;
	int	mins[3];		// for frustum culling
	int	maxs[3];
	int	firstleafsurface;
	int	numleafsurfaces;
	int	firstleafbrush;
	int	numleafbrushes;
} dleaf_t;

typedef struct
{
	int	planenum;		// facing out of the leaf
	int	shadernum;	// surface description
} dbrushsideq_t;

typedef struct
{
	int	planenum;		// facing out of the leaf
	int	shadernum;	// surface description
	int	surfacenum;
} dbrushsider_t;

typedef struct
{
	int	firstside;
	int	numsides;
	int	shadernum;	// brush global shader (e.g. contents)
} dbrush_t;

typedef struct
{
	char	shader[64];
	int	brushnum;
	int	visibleside;
} dfog_t;

typedef struct
{
	int	numclusters;
	int	rowsize;
	byte	data[1];		// unbounded
} dvis_t;

typedef struct
{
	byte	ambient[3];
	byte	diffuse[3];
	byte	direction[2];
} dlightgridq_t;	// IBSP

typedef struct
{
	byte	ambient[LM_STYLES][3];
	byte	diffuse[LM_STYLES][3];
	byte	styles[LM_STYLES];
	byte	direction[2];
} dlightgridr_t;	// RBSP

typedef struct
{
	float	ambient[3];
	float	diffuse[3];
	byte	direction[2];
} dlightgridx_t;	// XBSP

typedef struct
{
	int	shadernum;
	int	fognum;
	int	facetype;

	int	firstvert;
	int	numverts;
	uint	firstelem;
	int	numelems;

	int	lm_texnum;	// lightmap info
	int	lm_offset[2];
	int	lm_size[2];

	vec3_t	origin;		// MST_FLARE only

	vec3_t	mins;
	vec3_t	maxs;		// MST_PATCH and MST_TRISURF only
	vec3_t	normal;		// MST_PLANAR only

	int	patch_cp[2];	// patch control point dimensions
} dsurfaceq_t;	// IBSP, XBSP

typedef struct
{
	int	shadernum;
	int	fognum;
	int	facetype;

	int	firstvert;
	int	numverts;
	uint	firstelem;
	int	numelems;

	byte	lightmapStyles[LM_STYLES];
	byte	vertexStyles[LM_STYLES];

	int	lm_texnum[LM_STYLES];	// lightmap info
	int	lm_offset[LM_STYLES][2];
	int	lm_size[2];

	vec3_t	origin;		// FACETYPE_FLARE only

	vec3_t	mins;
	vec3_t	maxs;		// FACETYPE_PATCH and FACETYPE_TRISURF only
	vec3_t	normal;		// FACETYPE_PLANAR only

	int	patch_cp[2];	// patch control point dimensions
} dsurfacer_t;	// RBSP

#include "studio_ref.h"

/*
========================================================================
ROQ FILES

The .roq file are vector-compressed movies
========================================================================
*/
#define RoQ_HEADER1		4228
#define RoQ_HEADER2		-1
#define RoQ_HEADER3		30
#define RoQ_FRAMERATE	30

// RoQ markers
#define RoQ_INFO		0x1001
#define RoQ_QUAD_CODEBOOK	0x1002
#define RoQ_QUAD_VQ		0x1011
#define RoQ_SOUND_MONO	0x1020
#define RoQ_SOUND_STEREO	0x1021

// RoQ movie type
#define RoQ_ID_MOT		0x00
#define RoQ_ID_FCC		0x01
#define RoQ_ID_SLD		0x02
#define RoQ_ID_CCC		0x03

typedef struct 
{
	byte		y[4];
	byte		u;
	byte		v;
} dcell_t;

typedef struct 
{
	byte		idx[4];
} dquadcell_t;

typedef struct 
{
	word		id;
	uint		size;
	word		argument;
} droqchunk_t;

/*
==============================================================================
SAVE FILE

half-life implementation of saverestore system
==============================================================================
*/
#define SAVEFILE_HEADER	(('V'<<24)+('L'<<16)+('A'<<8)+'V')	// little-endian "VALV"
#define SAVEGAME_HEADER	(('V'<<24)+('A'<<16)+('S'<<8)+'J')	// little-endian "JSAV"
#define SAVEGAME_VERSION	0x0071				// Version 0.71

#endif//REF_DFILES_H