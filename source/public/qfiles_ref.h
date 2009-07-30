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
#define Q3IDBSP_VERSION	46
#define RTCWBSP_VERSION	47
#define IGIDBSP_VERSION	48	// extended brushides
#define RFIDBSP_VERSION	1	// both raven bsp and qfusion bsp

#define IDBSPMODHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I') // little-endian "IBSP"
#define RBBSPMODHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'R') // little-endian "RBSP"
#define QFBSPMODHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'F') // little-endian "FBSP"

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
#define LUMP_LIGHTARRAY		17
#define LUMP_COLLISION		18		// Xash extra lump
#define HEADER_LUMPS		18		// 16 for IDBSP

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
	SURF_NOSTEPS		= BIT(13),		// REMOVE? no footstep sounds
	SURF_NONSOLID		= BIT(14),	// don't collide against curves with this set
	SURF_LIGHTFILTER		= BIT(15),	// act as a light filter during q3map -light
	SURF_ALPHASHADOW		= BIT(16),	// do per-pixel light shadow casting in q3map
	SURF_NODLIGHT		= BIT(17),	// never add dynamic lights
	SURF_DUST			= BIT(18),	// REMOVE? leave a dust trail when walking on this surface
	SURF_BLEND		= BIT(19),
	SURF_ALPHA		= BIT(20),
	SURF_ADDITIVE		= BIT(21),
	SURF_GLOW			= BIT(22),
} surfaceType_t;

enum
{
	MST_BAD = 0,
	MST_PLANAR,
	MST_PATCH,
	MST_TRISURF,
	MST_FLARE,
	MST_FOLIAGE
};

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
} dvertexq_t;

typedef struct
{
	vec3_t	point;
	vec2_t	tex_st;
	vec2_t	lm_st[LM_STYLES];
	vec3_t	normal;
	byte	color[LM_STYLES][4];
} dvertexr_t;

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

	union
	{
		int	surfacenum;
		int	surfaceflags;
	};
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
} dlightgridq_t;

typedef struct
{
	byte	ambient[LM_STYLES][3];
	byte	diffuse[LM_STYLES][3];
	byte	styles[LM_STYLES];
	byte	direction[2];
} dlightgridr_t;

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
} dsurfaceq_t;

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
} dsurfacer_t;

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
========================================================================

.MD3 model file format

========================================================================
*/

#define IDMD3HEADER			(('3'<<24)+('P'<<16)+('D'<<8)+'I') // little-endian "IDP3"

#define MD3_ALIAS_VERSION		15
#define MD3_ALIAS_MAX_LODS		4

#define MD3_MAX_TRIANGLES		8192	// per mesh
#define MD3_MAX_VERTS		4096	// per mesh
#define MD3_MAX_SHADERS		256	// per mesh
#define MD3_MAX_FRAMES		1024	// per model
#define MD3_MAX_MESHES		32	// per model
#define MD3_MAX_TAGS		16	// per frame
#define MD3_MAX_PATH		64

// vertex scales
#define MD3_XYZ_SCALE		(1.0f/64)

typedef struct
{
	float		st[2];
} dmd3coord_t;

typedef struct
{
	short		point[3];
	byte		norm[2];
} dmd3vertex_t;

typedef struct
{
	float		mins[3];
	float		maxs[3];
	float		translate[3];
	float		radius;
	char		creator[16];
} dmd3frame_t;

typedef struct
{
	char		name[MD3_MAX_PATH];		// tag name
	float		origin[3];
	float		axis[3][3];
} dmd3tag_t;

typedef struct 
{
	char		name[MD3_MAX_PATH];
	int		unused;			// shader
} dmd3skin_t;

typedef struct
{
	int		id;
	char		name[MD3_MAX_PATH];
	int		flags;
	int		num_frames;
	int		num_skins;
	int		num_verts;
	int		num_tris;
	int		ofs_elems;
	int		ofs_skins;
	int		ofs_tcs;
	int		ofs_verts;
	int		meshsize;
} dmd3mesh_t;

typedef struct
{
	int		id;
	int		version;
	char		filename[MD3_MAX_PATH];
	int		flags;
	int		num_frames;
	int		num_tags;
	int		num_meshes;
	int		num_skins;
	int		ofs_frames;
	int		ofs_tags;
	int		ofs_meshes;
	int		ofs_end;
} dmd3header_t;

/*
========================================================================

.SKM and .SKP models file formats

========================================================================
*/

#define SKMHEADER			(('1'<<24)+('M'<<16)+('K'<<8)+'S') // little-endian "SKM1"

#define SKM_MAX_NAME		64
#define SKM_MAX_MESHES		32
#define SKM_MAX_FRAMES		65536
#define SKM_MAX_TRIS		65536
#define SKM_MAX_VERTS		(SKM_MAX_TRIS * 3)
#define SKM_MAX_BONES		256
#define SKM_MAX_SHADERS		256
#define SKM_MAX_FILESIZE		16777216
#define SKM_MAX_ATTACHMENTS		SKM_MAX_BONES
#define SKM_MAX_LODS		4

// model format related flags
#define SKM_BONEFLAG_ATTACH		1
#define SKM_MODELTYPE		2	// (hierarchical skeletal pose)

typedef struct
{
	char			id[4];	// SKMHEADER
	uint			type;
	uint			filesize;	// size of entire model file

	uint			num_bones;
	uint			num_meshes;

	// this offset is relative to the file
	uint			ofs_meshes;
} dskmheader_t;

// there may be more than one of these
typedef struct
{
	// these offsets are relative to the file
	char			shadername[SKM_MAX_NAME];		// name of the shader to use
	char			meshname[SKM_MAX_NAME];

	uint			num_verts;
	uint			num_tris;
	uint			num_references;
	uint			ofs_verts;	
	uint			ofs_texcoords;
	uint			ofs_indices;
	uint			ofs_references;
} dskmmesh_t;

// one or more of these per vertex
typedef struct
{
	float			origin[3];		// vertex location (these blend)
	float			influence;		// influence fraction (these must add up to 1)
	float			normal[3];		// surface normal (these blend)
	uint			bonenum;	// number of the bone
} dskmbonevert_t;

// variable size, parsed sequentially
typedef struct
{
	uint			numweights;
	// immediately followed by 1 or more ddpmbonevert_t structures
	dskmbonevert_t		verts[1];
} dskmvertex_t;

typedef struct
{
	float			st[2];
} dskmcoord_t;

typedef struct
{
	char			id[4];				// SKMHEADER
	uint			type;
	uint			filesize;	// size of entire model file

	uint			num_bones;
	uint			num_frames;

	// these offsets are relative to the file
	uint			ofs_bones;
	uint			ofs_frames;
} dskpheader_t;

// one per bone
typedef struct
{
	// name examples: upperleftarm leftfinger1 leftfinger2 hand, etc
	char			name[SKM_MAX_NAME];
	signed int		parent;		// parent bone number
	uint			flags;		// flags for the bone
} dskpbone_t;

typedef struct
{
	float			quat[4];
	float			origin[3];
} dskpbonepose_t;

// immediately followed by bone positions for the frame
typedef struct
{
	// name examples: idle_1 idle_2 idle_3 shoot_1 shoot_2 shoot_3, etc
	char			name[SKM_MAX_NAME];
	uint			ofs_bonepositions;
} dskpframe_t;

/*
==============================================================================
SAVE FILE

included global, and both (client & server) pent list
==============================================================================
*/
#define LUMP_CFGSTRING	"configstrings"
#define LUMP_AREASTATE	"areaportals"
#define LUMP_BASEENTS	"entities"	// entvars + CBase->fields
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