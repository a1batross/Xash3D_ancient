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

/*
==============================================================================

VIRTUAL MACHINE

a internal virtual machine like as QuakeC, but it has more extensions
==============================================================================
*/
// header
#define QPROGS_VERSION	6	// Quake1 progs version
#define FPROGS_VERSION	7	// Fte progs version
#define VPROGS_VERSION	8	// xash progs version
#define LNNUMS_VERSION	1	// line numbers version

#define VPROGSHEADER16	(('6'<<24)+('1'<<16)+('D'<<8)+'I') // little-endian "ID16"
#define VPROGSHEADER32	(('2'<<24)+('3'<<16)+('D'<<8)+'I') // little-endian "ID32"
#define LINENUMSHEADER	(('F'<<24)+('O'<<16)+('N'<<8)+'L') // little-endian "LNOF"

#define MAX_EDICTS		4096	// must change protocol to increase more

#define PRVM_OP_STATE	1
#define PRVM_FE_CHAIN	4
#define PRVM_FE_CLASSNAME	8

enum
{
	PRVM_SERVERPROG = 0,
	PRVM_CLIENTPROG,
	PRVM_MENUPROG,
	PRVM_MAXPROGS,	// must be last			
};

// global ofssets
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
#define RESERVED_OFS	28

// misc flags
#define DEF_SHARED		(1<<14)
#define DEF_SAVEGLOBAL	(1<<15)

// compression block flags
#define COMP_STATEMENTS	1
#define COMP_DEFS		2
#define COMP_FIELDS		4
#define COMP_FUNCTIONS	8
#define COMP_STRINGS	16
#define COMP_GLOBALS	32
#define COMP_LINENUMS	64
#define COMP_TYPES		128
#define COMP_SOURCE		256
#define MAX_PARMS		8

#define PRVM_MAX_STACK_DEPTH	1024
#define PRVM_LOCALSTACK_SIZE	16384
#define PRVM_MAX_OPENFILES	256
#define PRVM_MAX_OPENSEARCHES	128

// 16-bit mode
#define dstatement_t	dstatement16_t
#define ddef_t		ddef16_t		// these should be the same except the string type

typedef void (*prvm_builtin_t)( void );

typedef enum 
{
	ev_void,
	ev_string,
	ev_float,
	ev_vector,
	ev_entity,
	ev_field,
	ev_function,
	ev_pointer,
	ev_integer,
	ev_variant,
	ev_struct,
	ev_union,
	ev_bool,
} etype_t;

typedef struct statement16_s
{
	word		op;
	short		a,b,c;

} dstatement16_t;

typedef struct statement32_s
{
	dword		op;
	long		a,b,c;

} dstatement32_t;

typedef struct ddef16_s
{
	word		type;		// if DEF_SAVEGLOBAL bit is set
					// the variable needs to be saved in savegames
	word		ofs;
	string_t		s_name;
} ddef16_t;

typedef struct ddef32_s
{
	dword		type;		// if DEF_SAVEGLOBAL bit is set
					// the variable needs to be saved in savegames
	dword		ofs;
	string_t		s_name;
} ddef32_t;

typedef struct fdef_s
{
	uint		type;		// if DEF_SAVEGLOBAL bit is set
					// the variable needs to be saved in savegames
	uint		ofs;
	uint		progsofs;		// used at loading time, so maching field offsets (unions/members) 
					// are positioned at the same runtime offset.
	char		*name;
} fdef_t;

typedef struct
{
	int		first_statement;	// negative numbers are builtins
	int		parm_start;
	int		locals;		// total ints of parms + locals
	int		profile;		// runtime
	string_t		s_name;
	string_t		s_file;		// source file defined in
	int		numparms;
	byte		parm_size[MAX_PARMS];
} dfunction_t;

typedef struct
{
	int		version;		// version number
	int		crc;		// check of header file
	
	uint		ofs_statements;	// comp 1
	uint		numstatements;	// statement 0 is an error
	uint		ofs_globaldefs;	// comp 2
	uint		numglobaldefs;
	uint		ofs_fielddefs;	// comp 4
	uint		numfielddefs;
	uint		ofs_functions;	// comp 8
	uint		numfunctions;	// function 0 is an empty
	uint		ofs_strings;	// comp 16
	uint		numstrings;	// first string is a null string
	uint		ofs_globals;	// comp 32
	uint		numglobals;
	uint		entityfields;

	// version 7 extensions
	uint		ofsfiles;		// source files always compressed
	uint		ofslinenums;	// numstatements big // comp 64
	uint		ofsbodylessfuncs;	// no comp
	uint		numbodylessfuncs;
	uint		ofs_types;	// comp 128
	uint		numtypes;
	uint		blockscompressed;	// who blocks are compressed (COMP flags)

	int		ident;		// version 7 id

} dprograms_t;

typedef struct dlno_s
{
	int	header;
	int	version;

	uint	numglobaldefs;
	uint	numglobals;
	uint	numfielddefs;
	uint	numstatements;
} dlno_t;

typedef struct mfunction_s
{
	int		first_statement;	// negative numbers are builtins
	int		parm_start;
	int		locals;		// total ints of parms + locals

	// these are doubles so that they can count up to 54bits or so rather than 32bit
	double		profile;		// runtime
	double		builtinsprofile;	// cost of builtin functions called by this function
	double		callcount;	// times the functions has been called since the last profile call

	int		s_name;
	int		s_file;		// source file defined in
	int		numparms;
	byte		parm_size[MAX_PARMS];
} mfunction_t;

typedef struct
{
	char		filename[128];
	int		size;
	int		compsize;
	int		compmethod;
	int		ofs;
} includeddatafile_t;

typedef struct type_s
{
	etype_t		type;

	struct type_s	*parentclass;	// type_entity...
	struct type_s	*next;
	struct type_s	*aux_type;	// return type or field type
	struct type_s	*param;

	int		num_parms;	// -1 = variable args
	uint		ofs;		// inside a structure.
	uint		size;
	char		*name;
} type_t;

typedef struct prvm_stack_s
{
	int			s;
	mfunction_t		*f;
} prvm_stack_t;

// virtual typedef
typedef union prvm_eval_s
{
	int		edict;
	int		_int;
	float		_float;
	float		vector[3];
	func_t		function;
	string_t		string;
} prvm_eval_t;

typedef struct edict_state_s
{
	bool	free;
	float	freetime;

} vm_edict_t;

struct edict_s
{
	// engine-private fields (stored in dynamically resized array)
	union
	{
		void			*vp;	// generic edict
		vm_edict_t		*ed;	// vm edict state 
		sv_edict_t		*sv;	// sv edict state
		cl_edict_t		*cl;	// cl edict state
		ui_edict_t		*ui;	// ui edict state
	} priv;

	// QuakeC prog fields (stored in dynamically resized array)
	union
	{
		void			*vp;	// generic entvars
		sv_entvars_t		*sv;	// server entvars
		cl_entvars_t		*cl;	// client entvars
		ui_entvars_t		*ui;	// uimenu entvars
	} progs;

};

typedef struct prvm_prog_s
{
	dprograms_t	*progs;
	mfunction_t	*functions;
	char		*strings;
	int		stringssize;
	ddef_t		*fielddefs;
	ddef_t		*globaldefs;
	dstatement_t	*statements;
	int		*linenums;	// debug versions only
	type_t		*types;
	int		edict_size;	// in bytes
	int		edictareasize;	// in bytes (for bound checking)
	int		pev_save;		// used by PRVM_PUSH_GLOBALS\PRVM_POP_GLOBALS
	int		other_save;	// used by PRVM_PUSH_GLOBALS\PRVM_POP_GLOBALS	

	int		*statement_linenums;// NULL if not available
	double		*statement_profile; // only incremented if prvm_statementprofiling is on

	union
	{
		float			*gp;
		sv_globalvars_t		*sv;
		cl_globalvars_t		*cl;
		ui_globalvars_t		*ui;
	} globals;

	int		maxknownstrings;
	int		numknownstrings;

	// this is updated whenever a string is removed or added
	// (simple optimization of the free string search)
	int		firstfreeknownstring;
	const char	**knownstrings;
	byte		*knownstrings_freeable;
	const char	***stringshash;

	// all memory allocations related to this vm_prog (code, edicts, strings)
	byte		*progs_mempool;	// [INIT]

	prvm_builtin_t	*builtins;	// [INIT]
	int		numbuiltins;	// [INIT]

	int		argc;

	int		trace;
	mfunction_t	*xfunction;
	int		xstatement;

	// stacktrace writes into stack[MAX_STACK_DEPTH]
	// thus increase the array, so depth wont be overwritten
	prvm_stack_t	stack[PRVM_MAX_STACK_DEPTH + 1];
	int		depth;

	int		localstack[PRVM_LOCALSTACK_SIZE];
	int		localstack_used;

	word		filecrc;
	int		intsize;

	//============================================================================
	// until this point everything also exists (with the pr_ prefix) in the old vm

	vfile_t		*openfiles[PRVM_MAX_OPENFILES];
	search_t		*opensearches[PRVM_MAX_OPENSEARCHES];

	// copies of some vars that were former read from sv
	int		num_edicts;
	// number of edicts for which space has been (should be) allocated
	int		max_edicts;	// [INIT]
	// used instead of the constant MAX_EDICTS
	int		limit_edicts;	// [INIT]

	// number of reserved edicts (allocated from 1)
	int		reserved_edicts;	// [INIT]

	edict_t	*edicts;
	void		*edictsfields;
	void		*edictprivate;

	// size of the engine private struct
	int		edictprivate_size;	// [INIT]

	// has to be updated every frame - so the vm time is up-to-date
	// AK changed so time will point to the time field (if there is one) else it points to _time
	// actually should be double, but qc doesnt support it
	float		*time;
	float		_time;

	// allow writing to world entity fields, this is set by server init and
	// cleared before first server frame
	bool		protect_world;

	// name of the prog, e.g. "Server", "Client" or "Menu" (used for text output)
	char		*name;		// [INIT]

	// flag - used to store general flags like PRVM_GE_PEV, etc.
	int		flag;

	char		*extensionstring;	// [INIT]

	bool		loadintoworld;	// [INIT]

	// used to indicate whether a prog is loaded
	bool		loaded;

	//============================================================================

	ddef_t		*pev; // if pev != 0 then there is a global pev

	//============================================================================
	// function pointers

	void		(*begin_increase_edicts)(void); // [INIT] used by PRVM_MEM_Increase_Edicts
	void		(*end_increase_edicts)(void); // [INIT]

	void		(*init_edict)(edict_t *edict); // [INIT] used by PRVM_ED_ClearEdict
	void		(*free_edict)(edict_t *ed); // [INIT] used by PRVM_ED_Free

	void		(*count_edicts)(void); // [INIT] used by PRVM_ED_Count_f

	bool		(*load_edict)(edict_t *ent); // [INIT] used by PRVM_ED_LoadFromFile

	void		(*init_cmd)(void);	// [INIT] used by PRVM_InitProg
	void		(*reset_cmd)(void);	// [INIT] used by PRVM_ResetProg

	void		(*error_cmd)(const char *format, ...); // [INIT]

} prvm_prog_t;

#endif//BASE_FILES_H