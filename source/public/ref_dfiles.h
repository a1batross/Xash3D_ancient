//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        ref_dfiles.h - xash supported formats
//=======================================================================
#ifndef REF_DFILES_H
#define REF_DFILES_H

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
#define IDWAD3HEADER	(('3'<<24)+('D'<<16)+('A'<<8)+'W')	// little-endian "WAD3" half-life wads

#define WAD3_NAMELEN	16
#define MAX_FILES_IN_WAD	8192

#define CMP_NONE		0	// compression none
#define CMP_LZSS		1	// RLE compression ?
#define CMP_ZLIB		2	// zip-archive compression

#define TYPE_ANY		-1	// any type can be accepted
#define TYPE_NONE		0	// blank lump
#define TYPE_QPAL		64	// quake palette
#define TYPE_QTEX		65	// probably was never used
#define TYPE_QPIC		66	// quake1 and hl pic (lmp_t)
#define TYPE_MIPTEX2	67	// half-life (mip_t) previous was TYP_SOUND but never used in quake1
#define TYPE_MIPTEX		68	// quake1 (mip_t)
#define TYPE_RAW		69	// unrecognized raw data
#define TYPE_SCRIPT		70	// .txt scrips (xash ext)
#define TYPE_VPROGS		71	// .dat progs (xash ext)

typedef struct
{
	int		ident;		// should be IWAD, WAD2 or WAD3
	int		numlumps;		// num files
	int		infotableofs;
} dwadinfo_t;

typedef struct
{
	int		filepos;
	int		disksize;
	int		size;		// uncompressed
	char		type;
	char		compression;	// probably not used
	char		pad1;
	char		pad2;
	char		name[16];		// must be null terminated
} dlumpinfo_t;

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

// world limits
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
#define LUMP_TEXINFO		9
#define LUMP_FACES			10
#define LUMP_MODELS			11
#define LUMP_BRUSHES		12
#define LUMP_BRUSHSIDES		13
#define LUMP_VISIBILITY		14
#define LUMP_LIGHTING		15
#define LUMP_COLLISION		16	// newton collision tree (worldmodel coords already convert to meters)
#define LUMP_TEXTURES		17	// contains texture name and dims
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

#define DENT_KEY			0
#define DENT_VALUE			1

//other limits
#define MAXLIGHTMAPS		4
typedef struct
{
	int fileofs;
	int filelen;
} lump_t;				// many formats use lumps to store blocks

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
	string_t	epair[2];		// 0 - key, 1 - value (indexes from stringtable)
} dentity_t;

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

typedef struct
{
	float	vecs[2][4];	// [s/t][xyz offset] texture s\t
	int	texnum;		// texture number in LUMP_TEXTURES array
	int	contents;		// texture contents (can be replaced by shader)
	int	flags;		// surface flags (can be replaced by shader)
	int	value;		// get rid of this ? used by qrad, not engine
} dtexinfo_t;

typedef struct
{
	int	v[2];		// vertex numbers
} dedge_t;

typedef struct
{
	int	planenum;
	int	firstedge;
	int	numedges;	
	int	texinfo;		// number in LUMP_TEXINFO array

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
	int	texinfo;		// surface description (s/t coords, flags, etc)
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
	string_t	s_name;		// string system index
	string_t	s_next;		// anim chain texture
	int	size[2];		// valid size for current s\t coords (used for replace texture)
} dmiptex_t;

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
} studiohdr_t;

// header for demand loaded sequence group data
typedef struct 
{
	int		id;
	int		version;

	char		name[64];
	int		length;
} studioseqhdr_t;

// bones
typedef struct 
{
	char		name[32];		// bone name for symbolic links
	int		parent;		// parent bone
	int		flags;		// ??
	int		bonecontroller[6];	// bone controller index, -1 == none
	float		value[6];		// default DoF values
	float		scale[6];		// scale for delta DoF values
} mstudiobone_t;

// bone controllers
typedef struct 
{
	int		bone;		// -1 == 0
	int		type;		// X, Y, Z, XR, YR, ZR, M
	float		start;
	float		end;
	int		rest;		// byte index value at rest
	int		index;		// 0-3 user set controller, 4 mouth
} mstudiobonecontroller_t;

// intersection boxes
typedef struct
{
	int		bone;
	int		group;		// intersection group
	vec3_t		bbmin;		// bounding box
	vec3_t		bbmax;		
} mstudiobbox_t;

// demand loaded sequence groups
typedef struct
{
	char		label[32];	// textual name
	char		name[64];		// file name
	void		*cache;		// cache index pointer (only in memory)
	int		data;		// hack for group 0
} mstudioseqgroup_t;

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
} mstudioseqdesc_t;

// events
typedef struct 
{
	int 		frame;
	int		event;
	int		type;
	char		options[64];
} mstudioevent_t;

// pivots
typedef struct 
{
	vec3_t		org;		// pivot point
	int		start;
	int		end;
} mstudiopivot_t;

// attachment
typedef struct 
{
	char		name[32];
	int		type;
	int		bone;
	vec3_t		org;		// attachment point
	vec3_t		vectors[3];
} mstudioattachment_t;

typedef struct
{
	unsigned short	offset[6];
} mstudioanim_t;

// animation frames
typedef union 
{
	struct
	{
		byte	valid;
		byte	total;
	} num;
	short		value;
} mstudioanimvalue_t;


// body part index
typedef struct
{
	char		name[64];
	int		nummodels;
	int		base;
	int		modelindex;	// index into models array
} mstudiobodyparts_t;


// skin info
typedef struct
{
	char		name[64];
	int		flags;
	int		width;
	int		height;
	int		index;
} mstudiotexture_t;

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
} mstudiomodel_t;

// meshes
typedef struct 
{
	int		numtris;
	int		triindex;
	int		skinref;
	int		numnorms;		// per mesh normals
	int		normindex;	// normal vec3_t
} mstudiomesh_t;

/*
==============================================================================
SAVE FILE

included global, and both (client & server) pent list
==============================================================================
*/
#define SAVE_VERSION	3
#define IDSAVEHEADER	(('E'<<24)+('V'<<16)+('A'<<8)+'S') // little-endian "SAVE"

#define LUMP_COMMENTS	0 // map comments (name, savetime)
#define LUMP_CFGSTRING	1 // client info strings
#define LUMP_AREASTATE	2 // area portals state
#define LUMP_GAMESTATE	3 // progs global state (compressed)
#define LUMP_MAPNAME	4 // map name
#define LUMP_GAMECVARS	5 // contain game comment and all cvar state
#define LUMP_GAMEENTS	6 // ents state (compressed)
#define LUMP_SNAPSHOT	7 // rgb32 image snapshot (128x128)
#define SAVE_NUMLUMPS	8 // header size

typedef struct
{
	int	ident;
	int	version;	
	lump_t	lumps[SAVE_NUMLUMPS];
} dsavehdr_t;

typedef struct
{
	char	name[MAX_QPATH];
	char	value[MAX_QPATH];
} dsavecvar_t;

#endif//REF_DFILES_H