//=======================================================================
//			Copyright XashXT Group 2007 ©
//			   qcclib.h - qcclib header
//=======================================================================
#ifndef QCCLIB_H
#define QCCLIB_H

#include "platform.h"
#include "utils.h"
#include "vprogs.h"

/*

TODO:
	other pointer types for models and clients?
	compact string heap?
	always initialize all variables to something safe
	the def->type->type arrangement is really silly.
	return type checking
	parm count type checking
	immediate overflow checking
	pass the first two parms in call->b and call->c
*/

#define MEMBERFIELDNAME	"__m%s"		// mark
#define MAX_NAME		64		// chars long
#define MAX_PARMS		8
#define MAX_PARMS_EXTRA	128
#define PROGDEFS_MAX_SIZE	MAX_INPUTLINE	// 16 kbytes

// optimization level flags
#define FL_DBG		1
#define FL_OP0		2
#define FL_OP1		4
#define FL_OP2		8
#define FL_OP3		16
#define FL_OP4		32

#define MASK_DEBUG		(FL_DBG)
#define MASK_LEVEL_O	(FL_OP0)
#define MASK_LEVEL_1	(FL_OP0|FL_OP1)
#define MASK_LEVEL_2	(FL_OP0|FL_OP1|FL_OP2)
#define MASK_LEVEL_3	(FL_OP0|FL_OP1|FL_OP2|FL_OP3)
#define MASK_LEVEL_4	(FL_OP0|FL_OP1|FL_OP2|FL_OP3|FL_OP4)

//compiler flags
#define FLAG_V7_ONLY	1	// only 7 version can use this optimization
#define FLAG_DEFAULT	2	// enabled as default

#define G_FLOAT(o)		(pr_globals[o])
#define G_INT(o)		(*(int *)&pr_globals[o])
#define G_VECTOR(o)		(&pr_globals[o])
#define G_STRING(o)		(strings + *(string_t *)&pr_globals[o])
#define G_FUNCTION(o)	(*(func_t *)&pr_globals[o])

//qcc private pool
#define Qalloc( size )	Mem_Alloc(qccpool, size )
#define Qrealloc( ptr, size )	Mem_Realloc(qccpool, ptr, size )

//loading files
#define QCC_LoadFile(f, c)	PR_LoadFile(f, c, FT_CODE)
#define QCC_LoadData(f)	PR_LoadFile(f, true, FT_DATA)

#define BytesForBuckets(b)	(sizeof(bucket_t)*b)
#define STRCMP(s1, s2)	(((*s1)!=(*s2)) || strcmp(s1+1,s2+1))
#define STRNCMP(s1, s2, l)	(((*s1)!=(*s2)) || strncmp(s1+1,s2+1,l))

#define statements16 ((dstatement16_t*) statements)
#define statements32 statements

#define qcc_globals16 ((ddef16_t*)qcc_globals)
#define qcc_globals32 qcc_globals

#define fields16 ((ddef16_t*)fields)
#define fields32 fields

typedef uint		gofs_t;	// offset in global data block
typedef struct function_s	function_t;

typedef enum {
	tt_eof,		// end of file reached
	tt_name,		// an alphanumeric name token
	tt_punct,		// code punctuation
	tt_immediate,	// string, float, vector
} token_type_t;

enum {
	// progs 6 keywords
	KEYWORD_DO,
	KEYWORD_IF,
	KEYWORD_VOID,
	KEYWORD_ELSE,
	KEYWORD_LOCAL,	// legacy
	KEYWORD_WHILE,
	KEYWORD_ENTITY,
	KEYWORD_FLOAT,
	KEYWORD_STRING,
	KEYWORD_VECTOR,
	KEYWORD_RETURN,

	// 6 extended (no new opcode used)
	KEYWORD_BREAK,
	KEYWORD_FOR,
	KEYWORD_CONTINUE,
	KEYWORD_CONST,
		
	// progs 7 keywords
	KEYWORD_ENUM,
	KEYWORD_ENUMFLAGS,
	KEYWORD_CASE,
	KEYWORD_DEFAULT,
	KEYWORD_GOTO,
	KEYWORD_INT,
	KEYWORD_STATE,
	KEYWORD_CLASS,
	KEYWORD_STRUCT,
	KEYWORD_SWITCH,
	KEYWORD_TYPEDEF,
	KEYWORD_EXTERN,
	KEYWORD_VAR,
	KEYWORD_UNION,
	KEYWORD_THINKTIME,

	// progs 8 keywords
	KEYWORD_ASM,

	KEYWORD_SHARED,
	KEYWORD_NOSAVE,
	NUM_KEYWORDS		
};

typedef struct bucket_s
{
	void		*data;
	char		*keystring;
	struct bucket_s	*next;
} bucket_t;

typedef struct hashtable_s
{
	uint	numbuckets;
	bucket_t	**bucket;
} hashtable_t;

typedef struct cachedsourcefile_s
{
	char		filename[128];
	int		size;
	byte		*file;
	enum
	{
		FT_CODE,	// quakec source
		FT_DATA,	// quakec lib
	} type;
	struct cachedsourcefile_s *next; // chain
} cachedsourcefile_t;

typedef struct includechunk_s
{
	struct includechunk_s	*prev;
	char			*filename;
	char			*currentdatapoint;
	int			currentlinenumber;
} includechunk_t;

struct function_s
{
	int		builtin;		// if non 0, call an internal function
	int		code;		// first statement
	char		*file;		// source file with definition
	int		file_line;
	struct def_s	*def;
	uint		parm_ofs[MAX_PARMS];// always contiguous, right?
};

typedef struct temp_s
{
	gofs_t		ofs;
	struct def_s	*scope;
	struct def_s	*lastfunc;
	struct temp_s	*next;
	bool		used;
	uint		size;
} temp_t;

typedef union eval_s
{
	string_t		string;
	float		_float;
	float		vector[3];
	func_t		function;
	int		_int;
	union eval_s	*ptr;
} eval_t;

typedef struct
{
	bool		*enabled;
	char		*shortname;	// for option "/Ox", where x is shortname
	char		*fullname;	// display name
	int		levelmask;	// optimization level mask
	int		flags;		// sepcial flags for kill debug extensions, e.t.c
} optimisations_t;

typedef struct keyword_s
{
	uint		version;		// target version
	int		number;
	char		*name;		// first name
	char		*alias;		// alias for keyword
	uint		ignoretype;	// ignore matching for this type
} keyword_t;

typedef struct
{
	char		name[MAX_NAME];
	char		value[MAX_NAME * 4];
	char		params[MAX_PARMS][32];
	int		numparams;
	bool		used;
	bool		inside;
	int		namelen;
} const_t;

typedef struct freeoffset_s
{
	struct freeoffset_s	*next;
	gofs_t		ofs;
	uint		size;

} freeoffset_t;

typedef struct
{
	uint		version;	//weather we need to mark the progs as a newer version
	char		*name;
	char		*opname;
	int		priority;

	enum
	{
		ASSOC_LEFT,
		ASSOC_RIGHT,
	} associative;

	struct type_s	**type_a;
	struct type_s	**type_b;
	struct type_s	**type_c;
} opcode_t;

typedef struct def_s
{
	type_t		*type;
	char		*name;
	struct def_s	*next;
	struct def_s	*nextlocal;	// provides a chain of local variables
	gofs_t		ofs;                // for the opt_locals_marshalling optimisation.
	struct def_s	*scope;		// function the var was defined in, or NULL
	int		initialized;	// 1 when a declaration included "= immediate"
	int		constant;		// 1 says we can use the value over and over again
	bool		local;		// 1 indices local variable		

	int		references;
	int		timescalled;	// part of the opt_stripfunctions optimisation.

	int		s_file;
	int		s_line;

	int		arraysize;
	bool		shared;
	bool		saved;

	temp_t		*temp;
} def_t;

typedef struct
{
	type_t		*types;
	
	def_t		def_head;		// unused head of linked list
	def_t		*def_tail;	// add new defs after this and move it
	def_t		*localvars;	// chain of variables which need to be pushed and stuff.
	
	int		size_fields;
} pr_info_t;

typedef enum {

	WARN_DEBUGGING,
	WARN_ERROR,
	WARN_NOTREFERENCED,
	WARN_NOTREFERENCEDCONST,
	WARN_CONFLICTINGRETURNS,
	WARN_TOOFEWPARAMS,
	WARN_TOOMANYPARAMS,
	WARN_UNEXPECTEDPUNCT,
	WARN_ASSIGNMENTTOCONSTANT,
	WARN_ASSIGNMENTTOCONSTANTFUNC,
	WARN_MISSINGRETURNVALUE,
	WARN_WRONGRETURNTYPE,
	WARN_POINTLESSSTATEMENT,
	WARN_MISSINGRETURN,
	WARN_DUPLICATEDEFINITION,
	WARN_UNDEFNOTDEFINED,
	WARN_PRECOMPILERMESSAGE,
	WARN_TOOMANYPARAMETERSFORFUNC,
	WARN_STRINGTOOLONG,
	WARN_BADTARGET,
	WARN_BADPRAGMA,
	WARN_HANGINGSLASHR,
	WARN_NOTDEFINED,
	WARN_NOTCONSTANT,
	WARN_SWITCHTYPEMISMATCH,
	WARN_CONFLICTINGUNIONMEMBER,
	WARN_ENUMFLAGS_NOTINTEGER,
	WARN_ENUMFLAGS_NOTBINARY,
	WARN_CASEINSENSATIVEFRAMEMACRO,
	WARN_DUPLICATELABEL,
	WARN_DUPLICATEMACRO,
	WARN_ASSIGNMENTINCONDITIONAL,
	WARN_MACROINSTRING,
	WARN_BADPARAMS,
	WARN_IMPLICITCONVERSION,
	WARN_FIXEDRETURNVALUECONFLICT,
	WARN_EXTRAPRECACHE,
	WARN_NOTPRECACHED,
	WARN_DEADCODE,
	WARN_UNREACHABLECODE,
	WARN_NOTSTANDARDBEHAVIOUR,
	WARN_INEFFICIENTPLUSPLUS,
	WARN_DUPLICATEPRECOMPILER,
	WARN_IDENTICALPRECOMPILER,
	WARN_EXTENSION_USED,		//extension that frikqcc also understands
	WARN_IFSTRING_USED,
	WARN_LAXCAST,			//some errors become this with a compiler flag
	WARN_UNDESIRABLECONVENTION,
	WARN_SAMENAMEASGLOBAL,
	WARN_CONSTANTCOMPARISON,
	WARN_IMAGETOOBIG,
	WARN_IGNOREDONLEFT,

	ERR_PARSEERRORS,			//caused by pr_parseerror being called.

	//these are definatly my fault...
	ERR_INTERNAL,
	ERR_TOOCOMPLEX,
	ERR_BADOPCODE,
	ERR_TOOMANYSTATEMENTS,
	ERR_TOOMANYSTRINGS,
	ERR_BADTARGETSWITCH,
	ERR_TOOMANYTYPES,
	ERR_TOOMANYPAKFILES,
	ERR_PRECOMPILERCONSTANTTOOLONG,
	ERR_MACROTOOMANYPARMS,
	ERR_CONSTANTTOOLONG,
	ERR_TOOMANYFRAMEMACROS,

	//limitations, some are imposed by compiler, some arn't.
	ERR_TOOMANYGLOBALS,
	ERR_TOOMANYGOTOS,
	ERR_TOOMANYBREAKS,
	ERR_TOOMANYCONTINUES,
	ERR_TOOMANYCASES,
	ERR_TOOMANYLABELS,
	ERR_TOOMANYOPENFILES,
	ERR_TOOMANYPARAMETERSVARARGS,
	ERR_TOOMANYTOTALPARAMETERS,

	//these are probably yours, or qcc being fussy.
	ERR_BADEXTENSION,
	ERR_BADIMMEDIATETYPE,
	ERR_NOOUTPUT,
	ERR_NOTAFUNCTION,
	ERR_FUNCTIONWITHVARGS,
	ERR_BADHEX,
	ERR_UNKNOWNPUCTUATION,
	ERR_EXPECTED,
	ERR_NOTANAME,
	ERR_NAMETOOLONG,
	ERR_NOFUNC,
	ERR_COULDNTOPENFILE,
	ERR_NOTFUNCTIONTYPE,
	ERR_TOOFEWPARAMS,
	ERR_TOOMANYPARAMS,
	ERR_CONSTANTNOTDEFINED,
	ERR_BADFRAMEMACRO,
	ERR_TYPEMISMATCH,
	ERR_TYPEMISMATCHREDEC,
	ERR_TYPEMISMATCHPARM,
	ERR_TYPEMISMATCHARRAYSIZE,
	ERR_UNEXPECTEDPUNCTUATION,
	ERR_NOTACONSTANT,
	ERR_REDECLARATION,
	ERR_INITIALISEDLOCALFUNCTION,
	ERR_NOTDEFINED,
	ERR_ARRAYNEEDSSIZE,
	ERR_ARRAYNEEDSBRACES,
	ERR_TOOMANYINITIALISERS,
	ERR_TYPEINVALIDINSTRUCT,
	ERR_NOSHAREDLOCALS,
	ERR_TYPEWITHNONAME,
	ERR_BADARRAYSIZE,
	ERR_NONAME,
	ERR_SHAREDINITIALISED,
	ERR_UNKNOWNVALUE,
	ERR_BADARRAYINDEXTYPE,
	ERR_NOVALIDOPCODES,
	ERR_MEMBERNOTVALID,
	ERR_BADPLUSPLUSOPERATOR,
	ERR_BADNOTTYPE,
	ERR_BADTYPECAST,
	ERR_MULTIPLEDEFAULTS,
	ERR_CASENOTIMMEDIATE,
	ERR_BADSWITCHTYPE,
	ERR_BADLABELNAME,
	ERR_NOLABEL,
	ERR_THINKTIMETYPEMISMATCH,
	ERR_STATETYPEMISMATCH,
	ERR_BADBUILTINIMMEDIATE,
	ERR_PARAMWITHNONAME,
	ERR_ILLEGALNAME,
	ERR_BADPARAMORDER,
	ERR_ILLEGALCONTINUES,
	ERR_ILLEGALBREAKS,
	ERR_ILLEGALCASES,
	ERR_NOTANUMBER,
	ERR_WRONGSUBTYPE,
	ERR_EOF,
	ERR_NOPRECOMPILERIF,
	ERR_NOENDIF,
	ERR_HASHERROR,
	ERR_NOTATYPE,
	ERR_TOOMANYPACKFILES,
	ERR_INVALIDVECTORIMMEDIATE,
	ERR_INVALIDSTRINGIMMEDIATE,
	ERR_BADCHARACTURECODE,
	ERR_BADPARMS,
	ERR_EXCEEDERRCOUNT,
	ERR_PRECOMPILERMESSAGE,

	WARN_MAX,
};


extern pr_info_t	pr;
extern byte	*qccpool;
extern file_t	*asmfile;
extern uint	MAX_REGS;
extern int	MAX_ERRORS;
extern int	MAX_STRINGS;
extern int	MAX_GLOBALS;
extern int	MAX_FIELDS;
extern int	MAX_STATEMENTS;
extern int	MAX_FUNCTIONS;
extern int	MAX_CONSTANTS;
extern char	*compilingfile;
extern char	progsoutname[MAX_SYSPATH];
extern char	sourcedir[MAX_SYSPATH];
extern int	target_version;
extern int	recursivefunctiontype;
extern freeoffset_t	*freeofs;
extern type_t	*type_void;
extern type_t	*type_string;
extern type_t	*type_float;
extern type_t	*type_vector;
extern type_t	*type_entity;
extern type_t	*type_field;
extern type_t	*type_function;
extern type_t	*type_pointer;
extern type_t	*type_integer;
extern type_t	*type_variant;
extern type_t	*type_floatfield;
extern includechunk_t *currentchunk;
extern cachedsourcefile_t *sourcefile;
extern optimisations_t pr_optimisations[];
extern char	v_copyright[1024];
extern hashtable_t	compconstantstable;
extern hashtable_t	globalstable, localstable;
extern hashtable_t	intconstdefstable;
extern hashtable_t	floatconstdefstable;
extern hashtable_t	stringconstdefstable;
extern opcode_t	pr_opcodes[];	// sized by initialization
extern keyword_t	pr_keywords[];
extern temp_t	*functemps;
const extern int	type_size[];
extern const_t	*CompilerConstant;
extern bool	autoprototype;
extern bool	bodylessfuncs;
extern char	pr_token[8192];
extern token_type_t	pr_token_type;
extern type_t	*pr_immediate_type;
extern eval_t	pr_immediate;

extern bool	opt_laxcasts;
extern bool	opt_ifstring;
extern bool	opt_overlaptemps;
extern bool	opt_shortenifnots;
extern bool	opt_noduplicatestrings;
extern bool	opt_constantarithmatic;
extern bool	opt_nonvec_parms;
extern bool	opt_constant_names;
extern bool	opt_precache_file;
extern bool	opt_filenames;
extern bool	opt_assignments;
extern bool	opt_unreferenced;
extern bool	opt_function_names;
extern bool	opt_locals;
extern bool	opt_dupconstdefs;
extern bool	opt_constant_names_strings;
extern bool	opt_return_only;
extern bool	opt_compound_jumps;
extern bool	opt_stripfunctions;
extern bool	opt_locals_marshalling;
extern bool	opt_logicops;
extern bool	opt_vectorcalls;
extern bool	opt_writelinenums;
extern bool	opt_writetypes;
extern bool	opt_writesources;
extern bool	opt_compstrings;	// compress all strings into produced file
extern bool	opt_compfunctions;	// compress all functions and statements
extern bool	opt_compress_other;
extern bool	pr_subscopedlocals;
extern bool 	pr_warning[WARN_MAX];
extern char	pr_parm_names[MAX_PARMS + MAX_PARMS_EXTRA][MAX_NAME];
extern def_t	*extra_parms[MAX_PARMS_EXTRA];
extern jmp_buf	pr_parse_abort; // longjump with this on parse error
extern int	pr_source_line;
extern char	*pr_file_p;
extern def_t	*pr_scope;
extern int	pr_bracelevel;
extern int	pr_error_count;
extern int	pr_total_error_count;
extern int	pr_warning_count;
extern int	ForcedCRC;
extern string_t	s_file, s_file2; // filename for function definition
extern def_t	def_ret;
extern def_t	def_parms[MAX_PARMS];
extern char	pr_immediate_string[8192];
extern char	sourcefilename[MAX_SYSPATH];
extern float	*pr_globals;
extern uint	numpr_globals;
extern char	*strings;
extern int	strofs;
extern dstatement_t	*statements;
extern int	numstatements;
extern int	*statement_linenums;
extern dfunction_t	*functions;
extern int	numfunctions;
extern ddef_t	*qcc_globals;
extern int	numglobaldefs;
extern def_t	*activetemps;
extern ddef_t	*fields;
extern int	numfielddefs;
extern type_t	*qcc_typeinfo;
extern int	numtypeinfos;
extern int	maxtypeinfos;
extern int	numtemps;

//
// pr_utils.c
//
extern int typecmp(type_t *a, type_t *b);
void Hash_InitTable(hashtable_t *table, int numbucks);
uint Hash_Key(char *name, int modulus);
void *Hash_Get(hashtable_t *table, char *name);
void *Hash_GetKey(hashtable_t *table, int key);
void Hash_Remove(hashtable_t *table, char *name);
void Hash_RemoveKey(hashtable_t *table, int key);
void *Hash_GetNext(hashtable_t *table, char *name, void *old);
void Hash_RemoveData(hashtable_t *table, char *name, void *data);
void *Hash_Add(hashtable_t *table, char *name, void *data, bucket_t *buck);
void *Hash_AddKey(hashtable_t *table, int key, void *data, bucket_t *buck);
void PR_WriteBlock(vfile_t *handle, fs_offset_t pos, const void *data, size_t blocksize, bool compress);
bool PR_LoadingProject( const char *filename );
void PR_SetDefaultProperties( void );
word PR_WriteProgdefs (char *filename);
void PR_WriteLNOfile(char *filename);
byte *PR_CreateProgsSRC( void );
void PR_WriteDAT( void );

//
// pr_lex.c
//
int PR_LexInteger (void);
void PR_LexString (void);
void PR_LexWhitespace (void);
bool PR_SimpleGetToken (void);
bool PR_Include(char *filename);
bool PR_UndefineName(char *name);
void PR_ConditionCompilation(void);
char *PR_CheakCompConstString(char *def);
const_t *PR_CheckCompConstDefined(char *def);
type_t *PR_NewType (char *name, int basictype);

//
// pr_comp.c
//
void PR_Lex( void );
void PR_ParseAsm(void);
bool PR_UnInclude( void );
void PR_PrintDefs( void );
void PR_ParseState (void);
char *PR_ParseName( void );
void PR_Expect (char *string);
char *TypeName( type_t *type );
void PR_ResetErrorScope( void );
void PR_SkipToSemicolon( void );
type_t *TypeForName(char *name);
void PR_ClearGrabMacros( void );
def_t *PR_MakeIntDef(int value);
void PR_UnmarshalLocals( void );
void PR_NewLine (bool incomment);
bool PR_CheckToken (char *string);
bool PR_CheckName (char *string);
const_t *PR_DefineName(char *name);
type_t *PR_ParseType (int newtype);
type_t *PR_FindType (type_t *type);
bool PR_MatchKeyword( int keyword );
def_t *PR_MakeFloatDef(float value);
def_t *PR_MakeStringDef(char *value);
bool PR_KeywordEnabled( int keyword );
void PR_Message( char *message, ... );
bool PR_CheckForKeyword( int keyword );
type_t *PR_FieldType (type_t *pointsto);
void PR_PrintStatement (dstatement_t *s);
type_t *PR_PointerType (type_t *pointsto);
int PR_WriteBodylessFuncs (vfile_t *handle);
char *PR_ValueString (etype_t type, void *val);
int PR_CopyString (char *str, bool noduplicate );
void PR_CompileFile (char *string, char *filename);
bool PR_StatementIsAJump(int stnum, int notifdest);
void PR_ParsePrintDef (int warningtype, def_t *def);
def_t *PR_Expression (int priority, bool allowcomma);
void PR_ParseError (int errortype, char *error, ...);
int PR_AStatementJumpsTo(int targ, int first, int last);
void PR_ParseWarning (int warningtype, char *error, ...);
void PR_EmitClassFromFunction(def_t *scope, char *tname);
byte *PR_LoadFile(char *filename, bool crash, int type );
int PR_encode( int len, int method, char *in, vfile_t *handle);
void PR_EmitArrayGetFunction(def_t *scope, char *arrayname);
void PR_EmitArraySetFunction(def_t *scope, char *arrayname);
type_t *PR_ParseFunctionType (int newtype, type_t *returntype);
void PR_IncludeChunk (char *data, bool duplicate, char *filename);
void PR_Warning (int type, char *file, int line, char *error, ...);
void PR_ParseErrorPrintDef (int errortype, def_t *def, char *error, ...);
int PR_WriteSourceFiles(vfile_t *h, dprograms_t *progs, bool sourceaswell);
int PR_decode(int complen, int len, int method, char *info, char **buffer);
void PR_WriteAsmFunction(def_t *sc, uint firststatement, gofs_t firstparm);
def_t *PR_GetDef (type_t *type, char *name, def_t *scope, bool allocate, int arraysize);
def_t *PR_Statement ( opcode_t *op, def_t *var_a, def_t *var_b, dstatement_t **outstatement);
void PR_RemapOffsets(uint firststatement, uint laststatement, uint min, uint max, uint newmin);
def_t *PR_DummyDef(type_t *type, char *name, def_t *scope, int arraysize, uint ofs, int referable);
void PR_BeginCompilation ( void );
bool PR_ContinueCompile ( void );
void PR_FinishCompilation ( void );

//
// pr_main.c
//
bool PrepareDATProgs ( const char *dir, const char *name, byte params );
bool CompileDATProgs ( void );

#endif//QCCLIB_H