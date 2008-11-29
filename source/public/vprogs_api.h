//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       vprogs_api.h - xash virtual machine
//=======================================================================
#ifndef VPROGS_API_H
#define VPROGS_API_H

/*
==============================================================================

VPROGS.DLL INTERFACE
==============================================================================
*/
#define PRVM_MAX_STACK_DEPTH		1024
#define PRVM_LOCALSTACK_SIZE		16384
#define PRVM_MAX_OPENFILES		256
#define PRVM_MAX_OPENSEARCHES		128

// prog flags
#define PRVM_OP_STATE		1
#define PRVM_FE_CHAIN		4
#define PRVM_FE_CLASSNAME		8
#define PRVM_OP_THINKTIME		16

enum
{
	PRVM_SERVERPROG = 0,
	PRVM_CLIENTPROG,
	PRVM_MENUPROG,
	PRVM_DECOMPILED,
	PRVM_MAXPROGS		// must be last			
};

typedef void (*prvm_builtin_t)( void );
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
		vm_edict_t		*ui;	// ui edict state
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

typedef struct prvm_stack_s
{
	int		s;
	mfunction_t	*f;
} prvm_stack_t;

typedef struct prvm_prog_s
{
	dprograms_t	*progs;
	mfunction_t	*functions;
	char		*strings;
	int		stringssize;
	ddef_t		*fielddefs;
	ddef_t		*globaldefs;
	dstatement_t	*statements;
	dsource_t		*sources;		// debug version include packed source files
	int		*linenums;	// debug versions only
	void		*types;		// (type_t *)
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
	int		firstfreeknownstring;
	const char	**knownstrings;
	byte		*knownstrings_freeable;
	const char	***stringshash;
	byte		*progs_mempool;
	prvm_builtin_t	*builtins;
	int		numbuiltins;
	int		argc;
	int		trace;
	mfunction_t	*xfunction;
	int		xstatement;
	prvm_stack_t	stack[PRVM_MAX_STACK_DEPTH + 1];
	int		depth;
	int		localstack[PRVM_LOCALSTACK_SIZE];
	int		localstack_used;
	word		filecrc;
	int		intsize;
	vfile_t		*file[PRVM_MAX_OPENFILES];
	script_t		*script;
	search_t		*search;
	int		num_edicts;
	int		max_edicts;
	int		limit_edicts;
	int		reserved_edicts;
	edict_t		*edicts;
	void		*edictsfields;
	void		*edictprivate;
	int		edictprivate_size;
	float		*time;
	float		_time;
	bool		protect_world;
	bool		loadintoworld;
	bool		loaded;
	char		*name;
	int		flag;
	ddef_t		*pev; // if pev != 0 then there is a global pev

	// function pointers
	void		(*begin_increase_edicts)(void);
	void		(*end_increase_edicts)(void);
	void		(*init_edict)(edict_t *edict);
	void		(*free_edict)(edict_t *ed);
	void		(*count_edicts)(void);
	bool		(*load_edict)(edict_t *ent);		// initialize edict for first loading
	void		(*restore_edict)(edict_t *ent);	// restore edict from savegame or changelevel
	bool		(*keyvalue_edict)(edict_t *ent, const char *key, const char *value );	// KeyValueData
	void		(*init_cmd)(void);
	void		(*reset_cmd)(void);
	void		(*error_cmd)(const char *format, ...);

} prvm_prog_t;

typedef struct vprogs_exp_s
{
	// interface validator
	size_t	api_size;				// must matched with sizeof(vprogs_api_t)

	void ( *Init ) ( int argc, char **argv );	// init host
	void ( *Free ) ( void );			// close host

	// compiler functions
	void ( *PrepareDAT )( const char *dir, const char *name );
	void ( *CompileDAT )( void );
	void ( *Update )( dword time );		// refreshing compile, exec some programs e.t.c

	// edict operations
	edict_t *(*AllocEdict)( void );
	void (*FreeEdict)( edict_t *ed );
	void (*PrintEdict)( edict_t *ed );

	// savegame stuff
	void (*WriteGlobals)( void *buffer, void *ptr, setpair_t callback );
	void (*ReadGlobals)( int s_table, dkeyvalue_t *globals, int count );
	void (*WriteEdict)( edict_t *ed, void *buffer, void *ptr, setpair_t callback );
	void (*ReadEdict)( int s_table, int ednum, dkeyvalue_t *fields, int numpairs );

	// load ents description
	void (*LoadFromFile)( const char *data );

	// string manipulations
	const char *(*GetString)( int num );
	int (*SetEngineString)( const char *s );
	int (*SetTempString)( const char *s );

	void (*InitProg)( int prognr );
	void (*SetProg)( int prognr );
	bool (*ProgLoaded)( int prognr );
	void (*LoadProgs)( const char *filename );
	void (*ResetProg)( void );

	// abstract layer for searching globals and locals
	func_t (*FindFunctionOffset)( const char *function );
	int (*FindGlobalOffset)( const char *global );	
	int (*FindFieldOffset)( const char *field );	
	mfunction_t *(*FindFunction)( const char *name );
	ddef_t *(*FindGlobal)( const char *name );
	ddef_t *(*FindField)( const char *name );

	void (*StackTrace)( void );
	void (*Warning)( const char *fmt, ... );
	void (*Error)( const char *fmt, ... );

	void (*ExecuteProgram)( func_t fnum, const char *name, const char *file, const int line );
	void (*Crash)( void );			// crash virtual machine

	prvm_prog_t *prog;				// virtual machine current state

} vprogs_exp_t;

#endif//VPROGS_API_H