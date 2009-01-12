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
#define TYPE_BINDATA		69	// engine internal data (map lumps, save lumps etc)
#define TYPE_STRDATA		70	// stringdata type (stringtable marked as TYPE_BINDATA)
#define TYPE_RAW			71	// unrecognized raw data
#define TYPE_SCRIPT			72	// .txt scripts (xash ext)
#define TYPE_VPROGS			73	// .dat progs (xash ext)

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
	SPR_SINGLE = 0,
	SPR_GROUP,
	SPR_ANGLED			// xash ext
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
	SPR_SINGLE_FACE = 0,		// oriented sprite will be draw with one face
	SPR_DOUBLE_FACE,			// oriented sprite will be draw back face too
	SPR_XCROSS_FACE,			// same like as flame in UT'99
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
#define BSPMOD_VERSION	39
#define IDBSPMODHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I') // little-endian "IBSP"

// 32 bit limits
#define MAX_MAP_AREA_BYTES		MAX_MAP_AREAS / 8	// bit vector of area visibility
#define MAX_MAP_AREAS		0x100		// don't increase this
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
#define MAX_MAP_EDGES		0x80000
#define MAX_MAP_SURFEDGES		0x80000
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
#define LS_NORMAL			0x00
#define LS_UNUSED			0xFE
#define LS_NONE			0xFF
#define MAX_LSTYLES			256
#define MAX_LIGHT_STYLES		64
#define MAX_SWITCHED_LIGHTS		32

typedef enum
{
	AMBIENT_SKY = 0,		// windfly1.wav
	AMBIENT_WATER,
	AMBIENT_SLIME,
	AMBIENT_LAVA,
	NUM_AMBIENTS		// automatic ambient sounds
} bsp_sounds_t;

// lump names
#define LUMP_MAPINFO		"mapinfo"
#define LUMP_ENTITIES		"entities"
#define LUMP_SHADERS		"shaders"		// contains shader name and dims
#define LUMP_PLANES			"planes"
#define LUMP_LEAFS			"leafs"
#define LUMP_LEAFFACES		"leafsurfaces"	// marksurfaces
#define LUMP_LEAFBRUSHES		"leafbrushes"
#define LUMP_NODES			"nodes"
#define LUMP_VERTEXES		"vertexes"
#define LUMP_EDGES			"edges"
#define LUMP_SURFEDGES		"surfedges"
#define LUMP_TEXINFO		"texinfo"
#define LUMP_SURFACES		"surfaces"
#define LUMP_MODELS			"bmodels"		// bsp brushmodels
#define LUMP_BRUSHES		"brushes"
#define LUMP_BRUSHSIDES		"brushsides"
#define LUMP_VISIBILITY		"visdata"
#define LUMP_LIGHTING		"lightdata"
#define LUMP_COLLISION		"collision"	// prepared newton collision tree
#define LUMP_LIGHTGRID		"lightgrid"	// private server.dat for current map
#define LUMP_AREAS			"areas"
#define LUMP_AREAPORTALS		"areaportals"

#define MAP_SINGLEPLAYER		BIT(0)
#define MAP_DEATHMATCH		BIT(1)		// Classic DeathMatch
#define MAP_COOPERATIVE		BIT(2)		// Cooperative mode
#define MAP_TEAMPLAY_CTF		BIT(3)		// teamplay Capture The Flag
#define MAP_TEAMPLAY_DOM		BIT(4)		// teamplay dominate
#define MAP_LASTMANSTANDING		BIT(5)

typedef enum
{
	SURF_NONE			= 0,		// just a mask for source tabulation
	SURF_LIGHT		= BIT(0),		// value will hold the light strength
	SURF_SLICK		= BIT(1),		// effects game physics
	SURF_SKY			= BIT(2),		// don't draw, but add to skybox
	SURF_WARP			= BIT(3),		// turbulent water warp
	SURF_TRANS		= BIT(4),		// translucent
	SURF_BLEND		= BIT(5),		// same as blend
	SURF_ALPHA		= BIT(6),		// alphatest
	SURF_ADDITIVE		= BIT(7),		// additive surface
	SURF_NODRAW		= BIT(8),		// don't bother referencing the texture
	SURF_HINT			= BIT(9),		// make a primary bsp splitter
	SURF_SKIP			= BIT(10),	// completely ignore, allowing non-closed brushes
	SURF_NULL			= BIT(11),	// remove face after compile
	SURF_NOLIGHTMAP		= BIT(12),	// don't place lightmap for this surface
	SURF_MIRROR		= BIT(12),	// remove face after compile
	SURF_CHROME		= BIT(13),	// chrome surface effect
	SURF_GLOW			= BIT(14),	// sprites glow
} surfaceType_t;

typedef struct
{
	int	ident;
	int	version;	
	char	message[64];	// map message
	int	flags;		// map flags
	int	reserved[13];	// future expansions
} dheader_t;

typedef struct
{
	float	mins[3];
	float	maxs[3];
	int	firstsurface;	// submodels just draw faces 
	int	numsurfaces;	// without walking the bsp tree
	int	firstbrush;	// physics stuff
	int	numbrushes;
} dmodel_t;

typedef struct
{
	char	name[64];		// shader name
	int	size[2];		// general size for current s\t coords (used for replace texture)
	int	surfaceFlags;	// surface flags (can be replaced)
	int	contentFlags;	// texture contents (can be replaced)
} dshader_t;

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
	int	firstsurface;
	int	numsurfaces;	// counting both sides
} dnode_t;

typedef struct
{
	float	vecs[2][4];	// [s/t][xyz offset] texture s\t
	int	shadernum;	// shader number in LUMP_SHADERS array
	int	value;		// FIXME: eliminate this ? used by qrad, not engine
} dtexinfo_t;

typedef struct
{
	int	v[2];		// vertex numbers
} dedge_t;

typedef int	dsurfedge_t;
typedef dword	dleafface_t;
typedef dword	dleafbrush_t;

typedef struct
{
	int	contents;		// or of all brushes (not needed?)
	int	cluster;
	int	area;
	int	mins[3];		// for frustum culling
	int	maxs[3];
	int	firstleafsurface;
	int	numleafsurfaces;
	int	firstleafbrush;
	int	numleafbrushes;
	byte	sounds[NUM_AMBIENTS];
} dleaf_t;

typedef struct
{
	int	planenum;		// facing out of the leaf
	int	texinfo;		// surface description
} dbrushside_t;

typedef struct
{
	int	firstside;
	int	numsides;
	int	shadernum;	// brush global shader (e.g. contents)
} dbrush_t;

typedef struct
{
	int	numclusters;
	int	bitofs[8][2];	// bitofs[numclusters][2]
} dvis_t;

typedef struct
{
	vec3_t	mins;
	vec3_t	size;
	int	bounds[4];	// world bounds
	int	numpoints;	// lightgrid[points]
} dlightgrid_t;

typedef struct
{
	int	planenum;
	int	side;

	int	firstedge;
	int	numedges;	
	int	texinfo;		// number in LUMP_TEXINFO array

	// lighting info
	byte	styles[LM_STYLES];
	int	lightofs;		// start of [numstyles*surfsize] samples
} dsurface_t;

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

VIRTUAL MACHINE

a internal virtual machine like as QuakeC, but it has more extensions
==============================================================================
*/
// header
#define VPROGS_VERSION	8	// xash progs version
#define VPROGSHEADER32	(('2'<<24)+('3'<<16)+('M'<<8)+'V') // little-endian "VM32"

// prvm limits
#define MAX_REGS		65536
#define MAX_STRINGS		1000000
#define MAX_STATEMENTS	0x80000
#define MAX_GLOBALS		32768
#define MAX_FUNCTIONS	16384
#define MAX_FIELDS		2048
#define MAX_CONSTANTS	2048

// global ofsets
#define OFS_NULL		0
#define OFS_RETURN		1
#define OFS_PARM0		4
#define OFS_PARM1		7
#define OFS_PARM2		10
#define OFS_PARM3		13
#define OFS_PARM4		16
#define OFS_PARM5		19
#define OFS_PARM6		22
#define OFS_PARM7		25
#define OFS_PARM8		28
#define OFS_PARM9		31
#define RESERVED_OFS	34

// misc flags
#define DEF_SHARED		(1<<29)
#define DEF_SAVEGLOBAL	(1<<30)

// compression block flags
#define COMP_STATEMENTS	1
#define COMP_DEFS		2
#define COMP_FIELDS		4
#define COMP_FUNCTIONS	8
#define COMP_STRINGS	16
#define COMP_GLOBALS	32
#define COMP_LINENUMS	64
#define COMP_TYPES		128
#define MAX_PARMS		10

typedef struct statement_s
{
	dword		op;
	long		a,b,c;
} dstatement_t;

typedef struct ddef_s
{
	dword		type;		// if DEF_SAVEGLOBAL bit is set
					// the variable needs to be saved in savegames
	dword		ofs;
	int		s_name;
} ddef_t;

typedef struct
{
	int		first_statement;	// negative numbers are builtins
	int		parm_start;
	int		locals;		// total ints of parms + locals
	int		profile;		// runtime
	int		s_name;
	int		s_file;		// source file defined in
	int		numparms;
	byte		parm_size[MAX_PARMS];
} dfunction_t;

typedef struct
{
	int		filepos;
	int		disksize;
	int		size;
	char		compression;
	char		name[64];		// FIXME: make string_t
} dsource_t;

typedef struct
{
	int		ident;		// must be VM32
	int		version;		// version number
	int		crc;		// check of header file
	uint		flags;		// who blocks are compressed (COMP flags)
	
	uint		ofs_statements;	// COMP_STATEMENTS (release)
	uint		numstatements;	// statement 0 is an error
	uint		ofs_globaldefs;	// COMP_DEFS (release)
	uint		numglobaldefs;
	uint		ofs_fielddefs;	// COMP_FIELDS (release)
	uint		numfielddefs;
	uint		ofs_functions;	// COMP_FUNCTIONS (release)
	uint		numfunctions;	// function 0 is an empty
	uint		ofs_strings;	// COMP_STRINGS (release)
	uint		numstrings;	// first string is a null string
	uint		ofs_globals;	// COMP_GLOBALS (release)
	uint		numglobals;
	uint		entityfields;
	// debug info
	uint		ofssources;	// source are always compressed
	uint		numsources;
	uint		ofslinenums;	// COMP_LINENUMS (debug) numstatements big
	uint		ofsbodylessfuncs;	// no comp
	uint		numbodylessfuncs;
	uint		ofs_types;	// COMP_TYPES (debug)
	uint		numtypes;
} dprograms_t;

#include "studio_ref.h"

/*
==============================================================================
SAVE FILE

included global, and both (client & server) pent list
==============================================================================
*/
#define LUMP_CFGSTRING	"configstrings"
#define LUMP_AREASTATE	"areaportals"
#define LUMP_ENTITIES	"entities"	// entvars + CBase->fields
#define LUMP_ENTTABLE	"enttable"	// entity transition table
#define LUMP_ADJACENCY	"adjacency"	// Save Header + ADJACENCY
#define LUMP_GLOBALS	"global_data"	// Game Header + Global State
#define LUMP_GAMECVARS	"latched_cvars"
#define LUMP_HASHTABLE	"hashtable"	// contains string_t only for used hash-values
#define LUMP_SNAPSHOT	"saveshot"	// currently not implemented

#define DENT_KEY		0
#define DENT_VAL		1

typedef struct
{
	string_t		epair[2];		// 0 - key, 1 - value (indexes from stringtable)
} dkeyvalue_t;

#endif//REF_DFILES_H