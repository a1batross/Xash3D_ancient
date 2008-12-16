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

// bsp contents
typedef enum
{
	CONTENTS_NONE		= 0, 	// just a mask for source tabulation
	CONTENTS_SOLID		= BIT(0),	// an eye is never valid in a solid
	CONTENTS_WINDOW		= BIT(1),	// translucent, but not watery
	CONTENTS_AUX		= BIT(2),
	CONTENTS_LAVA		= BIT(3),
	CONTENTS_SLIME		= BIT(4),
	CONTENTS_WATER		= BIT(5),
	CONTENTS_SKY		= BIT(6),
	
	// space for new user contents

	CONTENTS_MIST		= BIT(12),// g-cont. what difference between fog and mist ?
	LAST_VISIBLE_CONTENTS	= BIT(12),// mask (LAST_VISIBLE_CONTENTS-1)
	CONTENTS_FOG		= BIT(13),// future expansion
	CONTENTS_AREAPORTAL		= BIT(14),// func_areaportal volume
	CONTENTS_PLAYERCLIP		= BIT(15),// clip affect only by player or bot
	CONTENTS_MONSTERCLIP	= BIT(16),// clip affect only by monster or npc
	CONTENTS_CLIP		= (CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP), // both type clip
	CONTENTS_ORIGIN		= BIT(17),// removed before bsping an entity
	CONTENTS_BODY		= BIT(18),// should never be on a brush, only in game
	CONTENTS_CORPSE		= BIT(19),// deadbody
	CONTENTS_DETAIL		= BIT(20),// brushes to be added after vis leafs
	CONTENTS_TRANSLUCENT	= BIT(21),// auto set if any surface has trans
	CONTENTS_LADDER		= BIT(22),// like water but ladder : )
	CONTENTS_TRIGGER		= BIT(23),// trigger volume

	// content masks
	MASK_SOLID		= (CONTENTS_SOLID|CONTENTS_WINDOW),
	MASK_PLAYERSOLID		= (CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_BODY),
	MASK_MONSTERSOLID		= (CONTENTS_SOLID|CONTENTS_MONSTERCLIP|CONTENTS_WINDOW|CONTENTS_BODY),
	MASK_DEADSOLID		= (CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_WINDOW),
	MASK_WATER		= (CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME),
	MASK_OPAQUE		= (CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA),
	MASK_SHOT			= (CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_WINDOW|CONTENTS_CORPSE)
} contentType_t;

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

/*
==============================================================================

STUDIO MODELS

Studio models are position independent, so the cache manager can move them.
==============================================================================
*/

// header
#define STUDIO_VERSION	10
#define IDSTUDIOHEADER	(('T'<<24)+('S'<<16)+('D'<<8)+'I') // little-endian "IDST"
#define IDSEQGRPHEADER	(('Q'<<24)+('S'<<16)+('D'<<8)+'I') // little-endian "IDSQ"

// studio limits
#define MAXSTUDIOTRIANGLES		32768	// max triangles per model
#define MAXSTUDIOVERTS		4096	// max vertices per submodel
#define MAXSTUDIOSEQUENCES		256	// total animation sequences
#define MAXSTUDIOSKINS		128	// total textures
#define MAXSTUDIOSRCBONES		512	// bones allowed at source movement
#define MAXSTUDIOBONES		128	// total bones actually used
#define MAXSTUDIOMODELS		32	// sub-models per model
#define MAXSTUDIOBODYPARTS		32	// body parts per submodel
#define MAXSTUDIOGROUPS		16	// sequence groups (e.g. barney01.mdl, barney02.mdl, e.t.c)
#define MAXSTUDIOANIMATIONS		512	// max frames per sequence
#define MAXSTUDIOMESHES		256	// max textures per model
#define MAXSTUDIOEVENTS		1024	// events per model
#define MAXSTUDIOPIVOTS		256	// pivot points
#define MAXSTUDIOBLENDS		16	// max anim blends
#define MAXSTUDIOCONTROLLERS		16	// max controllers per model
#define MAXSTUDIOATTACHMENTS		16	// max attachments per model

// model global flags
#define STUDIO_STATIC		0x0001	// model without anims
#define STUDIO_RAGDOLL		0x0002	// ragdoll animation pose

// lighting & rendermode options
#define STUDIO_NF_FLATSHADE		0x0001
#define STUDIO_NF_CHROME		0x0002
#define STUDIO_NF_FULLBRIGHT		0x0004
#define STUDIO_NF_COLORMAP		0x0008	// can changed by colormap command
#define STUDIO_NF_BLENDED		0x0010	// rendering as semiblended
#define STUDIO_NF_ADDITIVE		0x0020	// rendering with additive mode
#define STUDIO_NF_TRANSPARENT		0x0040	// use texture with alpha channel

// motion flags
#define STUDIO_X			0x0001
#define STUDIO_Y			0x0002	
#define STUDIO_Z			0x0004
#define STUDIO_XR			0x0008
#define STUDIO_YR			0x0010
#define STUDIO_ZR			0x0020
#define STUDIO_LX			0x0040
#define STUDIO_LY			0x0080
#define STUDIO_LZ			0x0100
#define STUDIO_AX			0x0200
#define STUDIO_AY			0x0400
#define STUDIO_AZ			0x0800
#define STUDIO_AXR			0x1000
#define STUDIO_AYR			0x2000
#define STUDIO_AZR			0x4000
#define STUDIO_TYPES		0x7FFF
#define STUDIO_RLOOP		0x8000	// controller that wraps shortest distance

// bonecontroller types
#define STUDIO_MOUTH		4

// sequence flags
#define STUDIO_LOOPING		0x0001

// render flags
#define STUDIO_RENDER		0x0001
#define STUDIO_EVENTS		0x0002
#define STUDIO_MIRROR		0x0004	// a local player in mirror 

// bone flags
#define STUDIO_HAS_NORMALS		0x0001
#define STUDIO_HAS_VERTICES		0x0002
#define STUDIO_HAS_BBOX		0x0004
#define STUDIO_HAS_CHROME		0x0008	// if any of the textures have chrome on them

typedef struct
{
	int		ident;
	int		version;

	char		name[64];
	int		length;

	vec3_t		eyeposition;	// ideal eye position
	vec3_t		min;		// ideal movement hull size
	vec3_t		max;			

	vec3_t		bbmin;		// clipping bounding box
	vec3_t		bbmax;		

	int		flags;

	int		numbones;		// bones
	int		boneindex;

	int		numbonecontrollers;	// bone controllers
	int		bonecontrollerindex;

	int		numhitboxes;	// complex bounding boxes
	int		hitboxindex;			
	
	int		numseq;		// animation sequences
	int		seqindex;

	int		numseqgroups;	// demand loaded sequences
	int		seqgroupindex;

	int		numtextures;	// raw textures
	int		textureindex;
	int		texturedataindex;

	int		numskinref;	// replaceable textures
	int		numskinfamilies;
	int		skinindex;

	int		numbodyparts;		
	int		bodypartindex;

	int		numattachments;	// queryable attachable points
	int		attachmentindex;

	int		soundtable;
	int		soundindex;
	int		soundgroups;
	int		soundgroupindex;

	int		numtransitions;	// animation node to animation node transition graph
	int		transitionindex;
} dstudiohdr_t;

// header for demand loaded sequence group data
typedef struct 
{
	int		id;
	int		version;

	char		name[64];
	int		length;
} dstudioseqhdr_t;

// bones
typedef struct 
{
	char		name[32];		// bone name for symbolic links
	int		parent;		// parent bone
	int		flags;		// ??
	int		bonecontroller[6];	// bone controller index, -1 == none
	float		value[6];		// default DoF values
	float		scale[6];		// scale for delta DoF values
} dstudiobone_t;

// bone controllers
typedef struct 
{
	int		bone;		// -1 == 0
	int		type;		// X, Y, Z, XR, YR, ZR, M
	float		start;
	float		end;
	int		rest;		// byte index value at rest
	int		index;		// 0-3 user set controller, 4 mouth
} dstudiobonecontroller_t;

// intersection boxes
typedef struct
{
	int		bone;
	int		group;		// intersection group
	vec3_t		bbmin;		// bounding box
	vec3_t		bbmax;		
} dstudiobbox_t;

// demand loaded sequence groups
typedef struct
{
	char		label[32];	// textual name
	char		name[64];		// file name
	void		*cache;		// cache index pointer (only in memory)
	int		data;		// hack for group 0
} dstudioseqgroup_t;

// sequence descriptions
typedef struct
{
	char		label[32];	// sequence label (name)

	float		fps;		// frames per second	
	int		flags;		// looping/non-looping flags

	int		activity;
	int		actweight;

	int		numevents;
	int		eventindex;

	int		numframes;	// number of frames per sequence

	int		numpivots;	// number of foot pivots
	int		pivotindex;

	int		motiontype;	
	int		motionbone;
	vec3_t		linearmovement;
	int		automoveposindex;
	int		automoveangleindex;

	vec3_t		bbmin;		// per sequence bounding box
	vec3_t		bbmax;		

	int		numblends;
	int		animindex;	// mstudioanim_t pointer relative to start of sequence group data
					// [blend][bone][X, Y, Z, XR, YR, ZR]

	int		blendtype[2];	// X, Y, Z, XR, YR, ZR
	float		blendstart[2];	// starting value
	float		blendend[2];	// ending value
	int		blendparent;

	int		seqgroup;		// sequence group for demand loading

	int		entrynode;	// transition node at entry
	int		exitnode;		// transition node at exit
	int		nodeflags;	// transition rules
	
	int		nextseq;		// auto advancing sequences
} dstudioseqdesc_t;

// events
typedef struct 
{
	int 		frame;
	int		event;
	int		type;
	char		options[64];
} dstudioevent_t;

// pivots
typedef struct 
{
	vec3_t		org;		// pivot point
	int		start;
	int		end;
} dstudiopivot_t;

// attachment
typedef struct 
{
	char		name[32];
	int		type;
	int		bone;
	vec3_t		org;		// attachment point
	vec3_t		vectors[3];
} dstudioattachment_t;

typedef struct
{
	unsigned short	offset[6];
} dstudioanim_t;

// animation frames
typedef union 
{
	struct
	{
		byte	valid;
		byte	total;
	} num;
	short		value;
} dstudioanimvalue_t;


// body part index
typedef struct
{
	char		name[64];
	int		nummodels;
	int		base;
	int		modelindex;	// index into models array
} dstudiobodyparts_t;

// skin info
typedef struct
{
	char		name[64];
	int		flags;
	int		width;
	int		height;

	union
	{
		int	index;		// disk: offset at start of buffer
		shader_t	shader;		// ref: shader number
	};
} dstudiotexture_t;

// skin families
// short	index[skinfamilies][skinref]		// skingroup info

// studio models
typedef struct
{
	char		name[64];

	int		type;
	float		boundingradius;	// software stuff

	int		nummesh;
	int		meshindex;

	int		numverts;		// number of unique vertices
	int		vertinfoindex;	// vertex bone info
	int		vertindex;	// vertex vec3_t
	int		numnorms;		// number of unique surface normals
	int		norminfoindex;	// normal bone info
	int		normindex;	// normal vec3_t

	int		numgroups;	// deformation groups
	int		groupindex;
} dstudiomodel_t;

// meshes
typedef struct 
{
	int		numtris;
	int		triindex;
	int		skinref;
	int		numnorms;		// per mesh normals
	int		normindex;	// normal vec3_t
} dstudiomesh_t;

/*
==============================================================================
SAVE FILE

included global, and both (client & server) pent list
==============================================================================
*/
#define LUMP_COMMENTS	"map_comment"
#define LUMP_CFGSTRING	"configstrings"
#define LUMP_AREASTATE	"areaportals"
#define LUMP_GAMESTATE	"globals"
#define LUMP_MAPCMDS	"map_cmds"
#define LUMP_GAMECVARS	"latched_cvars"
#define LUMP_GAMEENTS	"entities"
#define LUMP_SNAPSHOT	"saveshot"	// currently not implemented

#define DENT_KEY		0
#define DENT_VAL		1

typedef struct
{
	string_t		epair[2];		// 0 - key, 1 - value (indexes from stringtable)
} dkeyvalue_t;

#endif//REF_DFILES_H