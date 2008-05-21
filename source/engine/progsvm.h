/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/*
This is a try to make the vm more generic, it is mainly based on the progs.h file.
For the license refer to progs.h.

Generic means, less as possible hard-coded links with the other parts of the engine.
This means no edict_engineprivate struct usage, etc.
The code uses void pointers instead.
*/

#ifndef PROGSVM_H
#define PROGSVM_H

#include "pmove.h"
#include "sv_edict.h"		// server progs
#include "cl_edict.h"		// client progs
#include "ui_edict.h"		// uimenu progs
#include "mathlib.h"

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
		vm_edict_t		*ed;	// vm edict state 
		void			*vp;	// generic edict
		sv_edict_t		*sv;	// sv edict state
		ui_edict_t		*ui;	// ui edict state
	} priv;

	// QuakeC prog fields (stored in dynamically resized array)
	union
	{
		void			*vp;	// generic entvars
		sv_entvars_t		*sv;	// server entvars
		ui_entvars_t		*ui;	// uimenu entvars
	} progs;

};

#define PRVM_GETEDICTFIELDVALUE(ed, fieldoffset) (fieldoffset ? (prvm_eval_t *)((unsigned char *)ed->progs.vp + fieldoffset) : NULL)
#define PRVM_GETGLOBALFIELDVALUE(fieldoffset) (fieldoffset ? (prvm_eval_t *)((unsigned char *)prog->globals.gp + fieldoffset) : NULL)

#define PRVM_FE_CLASSNAME		8
#define PRVM_FE_CHAIN		4
#define PRVM_OP_STATE		1

#define PRVM_MAX_STACK_DEPTH		1024
#define PRVM_LOCALSTACK_SIZE		16384

#define PRVM_MAX_OPENFILES 256
#define PRVM_MAX_OPENSEARCHES 128

typedef void (*prvm_builtin_t) (void);

// [INIT] variables flagged with this token can be initialized by 'you'
// NOTE: external code has to create and free the mempools but everything else is done by prvm !
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

extern prvm_prog_t *prog;

enum
{
	PRVM_SERVERPROG = 0,
	PRVM_CLIENTPROG,
	PRVM_MENUPROG,
	PRVM_MAXPROGS,	// must be last			
};

extern prvm_prog_t prvm_prog_list[PRVM_MAXPROGS];

//============================================================================
// prvm_cmds part

extern prvm_builtin_t vm_sv_builtins[];
extern prvm_builtin_t vm_cl_builtins[];
extern prvm_builtin_t vm_m_builtins[];

extern const int vm_sv_numbuiltins;
extern const int vm_cl_numbuiltins;
extern const int vm_m_numbuiltins;

extern char *vm_sv_extensions;
extern char *vm_cl_extensions;
extern char *vm_m_extensions;

void VM_SV_Cmd_Init(void);
void VM_SV_Cmd_Reset(void);

void VM_CL_Cmd_Init(void);
void VM_CL_Cmd_Reset(void);

void VM_M_Cmd_Init(void);
void VM_M_Cmd_Reset(void);

void VM_Cmd_Init(void);
void VM_Cmd_Reset(void);
//============================================================================

void PRVM_Init (void);

void PRVM_ExecuteProgram (func_t fnum, const char *errormessage);

#define PRVM_Alloc(buffersize) _PRVM_Alloc(buffersize, __FILE__, __LINE__)
#define PRVM_Free(buffer) _PRVM_Free(buffer, __FILE__, __LINE__)
#define PRVM_FreeAll() _PRVM_FreeAll(__FILE__, __LINE__)
void *_PRVM_Alloc (size_t buffersize, const char *filename, int fileline);
void _PRVM_Free (void *buffer, const char *filename, int fileline);
void _PRVM_FreeAll (const char *filename, int fileline);

void PRVM_Profile (int maxfunctions, int mininstructions);
void PRVM_Profile_f (void);
void PRVM_PrintFunction_f (void);

void PRVM_PrintState(void);
void PRVM_CrashAll (void);
void PRVM_Crash (void);

int PRVM_ED_FindFieldOffset(const char *field);
int PRVM_ED_FindGlobalOffset(const char *global);
ddef_t *PRVM_ED_FindField (const char *name);
ddef_t *PRVM_ED_FindGlobal (const char *name);
mfunction_t *PRVM_ED_FindFunction (const char *name);
func_t PRVM_ED_FindFunctionOffset(const char *function);

void PRVM_MEM_IncreaseEdicts(void);

edict_t *PRVM_ED_Alloc (void);
void PRVM_ED_Free (edict_t *ed);
void PRVM_ED_ClearEdict (edict_t *e);

void PRVM_PrintFunctionStatements (const char *name);
void PRVM_ED_Print(edict_t *ed);
void PRVM_ED_Write (vfile_t *f, edict_t *ed);
char *PRVM_ED_Info(edict_t *ent);
const char *PRVM_ED_ParseEdict (const char *data, edict_t *ent);

void PRVM_ED_WriteGlobals (vfile_t *f);
void PRVM_ED_ParseGlobals (const char *data);

void PRVM_ED_LoadFromFile (const char *data);

edict_t *PRVM_EDICT_NUM_ERROR(int n, char *filename, int fileline);
#define PRVM_EDICT_NUM(n) (((n) >= 0 && (n) < prog->max_edicts) ? prog->edicts + (n) : PRVM_EDICT_NUM_ERROR(n, __FILE__, __LINE__))
#define PRVM_EDICT_NUM_UNSIGNED(n) (((n) < prog->max_edicts) ? prog->edicts + (n) : PRVM_EDICT_NUM_ERROR(n, __FILE__, __LINE__))
#define PRVM_NUM_FOR_EDICT(e) ((int)((edict_t *)(e) - prog->edicts))
#define PRVM_NEXT_EDICT(e) ((e) + 1)
#define PRVM_EDICT_TO_PROG(e) (PRVM_NUM_FOR_EDICT(e))
#define PRVM_PROG_TO_EDICT(n) (PRVM_EDICT_NUM(n))
#define PRVM_PUSH_GLOBALS prog->pev_save = prog->globals.sv->pev, prog->other_save = prog->globals.sv->other
#define PRVM_POP_GLOBALS prog->globals.sv->pev = prog->pev_save, prog->globals.sv->other = prog->other_save
#define PRVM_ED_POINTER(p) (prvm_eval_t *)((byte *)prog->edictsfields + p->_int)
#define PRVM_EM_POINTER(p) (prvm_eval_t *)((byte *)prog->edictsfields + (p))
#define PRVM_EV_POINTER(p) (prvm_eval_t *)(((byte *)prog->edicts) + p->_int) 	// this is correct ???
#define PRVM_CHECK_PTR(p, size) if(prvm_boundscheck->value && (p->_int < 0 || p->_int + size > prog->edictareasize))\
{\
prog->xfunction->profile += (st - startst);\
prog->xstatement = st - prog->statements;\
PRVM_ERROR("%s attempted to write to an out of bounds edict (%i)", PRVM_NAME, p->_int);\
return;\
}
#define PRVM_CHECK_INFINITE() if (++jumpcount == 10000000)\
{\
prog->xstatement = st - prog->statements;\
PRVM_Profile(1<<30, 1000000);\
PRVM_ERROR("runaway loop counter hit limit of %d jumps\n", jumpcount, PRVM_NAME);\
}
//============================================================================

// get arguments from progs
#define	PRVM_G_FLOAT(o) (prog->globals.gp[o])
#define	PRVM_G_INT(o) (*(int *)&prog->globals.gp[o])
#define	PRVM_G_EDICT(o) (PRVM_PROG_TO_EDICT(*(int *)&prog->globals.gp[o]))
#define	PRVM_G_EDICTNUM(o) PRVM_NUM_FOR_EDICT(PRVM_G_EDICT(o))
#define	PRVM_G_VECTOR(o) (&prog->globals.gp[o])
#define	PRVM_G_STRING(o) (PRVM_GetString(*(string_t *)&prog->globals.gp[o]))
#define	PRVM_G_FUNCTION(o) (*(func_t *)&prog->globals.gp[o])

// FIXME: make these go away?
#define	PRVM_E_FLOAT(e,o) (((float*)e->progs.vp)[o])
#define	PRVM_E_INT(e,o) (((int*)e->progs.vp)[o])
//#define	PRVM_E_VECTOR(e,o) (&((float*)e->progs.vp)[o])
#define	PRVM_E_STRING(e,o) (PRVM_GetString(*(string_t *)&((float*)e->progs.vp)[o]))

extern int prvm_type_size[8]; // for consistency : I think a goal of this sub-project is to
// make the new vm mostly independent from the old one, thus if it's necessary, I copy everything

void PRVM_Init_Exec(void);

void PRVM_ED_PrintEdicts_f (void);
void PRVM_ED_PrintNum (int ent);

const char *PRVM_GetString(int num);
int PRVM_SetEngineString(const char *s);
int PRVM_AllocString(size_t bufferlength, char **pointer);
void PRVM_FreeString(int num);

//============================================================================

#define PRVM_Begin
#define PRVM_End	prog = 0
#define PRVM_NAME	(prog->name ? prog->name : "unnamed.dat")

// helper macro to make function pointer calls easier
#define PRVM_GCALL(func) if(prog->func) prog->func
#define PRVM_ERROR if(prog) prog->error_cmd

// other prog handling functions
bool PRVM_SetProgFromString(const char *str);
void PRVM_SetProg(int prognr);

/*
Initializing a vm:
Call InitProg with the num
Set up the fields marked with [INIT] in the prog struct
Load a program with LoadProgs
*/
void PRVM_InitProg(int prognr);
// LoadProgs expects to be called right after InitProg
void PRVM_LoadProgs (const char *filename, int numrequiredfunc, char **required_func, int numrequiredfields, fields_t *required_field);
void PRVM_ResetProg(void);

bool PRVM_ProgLoaded(int prognr);

int PRVM_GetProgNr(void);

void VM_Warning(const char *fmt, ...);
void VM_Error(const char *fmt, ...);

// TODO: fill in the params
//void PRVM_Create();

//============================================================================
// nice helper macros

#ifndef VM_NOPARMCHECK
#define VM_SAFEPARMCOUNT(p,f)	if(prog->argc != p) PRVM_ERROR(#f " wrong parameter count (" #p " expected ) !")
#else
#define VM_SAFEPARMCOUNT(p,f)
#endif

#define VM_RETURN_EDICT(e)	(((int *)prog->globals.gp)[OFS_RETURN] = PRVM_EDICT_TO_PROG(e))

#define e10	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL
#define e100	e10,e10,e10,e10,e10,e10,e10,e10,e10,e10
#define e1000	e100,e100,e100,e100,e100,e100,e100,e100,e100,e100

#define VM_STRINGTEMP_BUFFERS 16
#define VM_STRINGTEMP_LENGTH MAX_INPUTLINE

// builtins and other general functions

char *VM_GetTempString(void);
void VM_CheckEmptyString (const char *s);
void VM_VarString(int first, char *out, int outlength);

void VM_checkextension (void);
void VM_error (void);
void VM_objerror (void);
void VM_print (void);
void VM_bprint (void);
void VM_sprint (void);
void VM_centerprint(void);
void VM_normalize (void);
void VM_veclength (void);
void VM_vectoyaw (void);
void VM_vectoangles (void);
void VM_random_long (void);
void VM_random_float (void);
void VM_localsound(void);
void VM_break (void);
void VM_localcmd (void);
void VM_cvar (void);
void VM_cvar_string(void);
void VM_cvar_set (void);
void VM_wprint (void);
void VM_ftoa (void);
void VM_fabs (void);
void VM_vtoa (void);
void VM_atov (void);
void VM_etos (void);
void VM_atof(void);
void VM_itof(void);
void VM_ftoe(void);
void VM_create (void);
void VM_remove (void);
void VM_find (void);
void VM_findfloat (void);
void VM_findchain (void);
void VM_findchainfloat (void);
void VM_findflags (void);
void VM_findchainflags (void);
void VM_precache_file (void);
void VM_precache_error (void);
void VM_precache_sound (void);
void VM_coredump (void);

void VM_stackdump (void);
void VM_crash(void); // REMOVE IT
void VM_traceon (void);
void VM_traceoff (void);
void VM_eprint (void);
void VM_rint (void);
void VM_floor (void);
void VM_ceil (void);
void VM_nextent (void);

void VM_changelevel (void);
void VM_sin (void);
void VM_cos (void);
void VM_sqrt (void);
void VM_randomvec (void);
void VM_registercvar (void);
void VM_min (void);
void VM_max (void);
void VM_bound (void);
void VM_pow (void);
void VM_copyentity (void);

void VM_Files_Init(void);
void VM_Files_CloseAll(void);

void VM_fopen(void);
void VM_fclose(void);
void VM_fgets(void);
void VM_fputs(void);
vfile_t *VM_GetFileHandle( int index );

void VM_strlen(void);
void VM_strcat(void);
void VM_substring(void);
void VM_stov(void);
void VM_allocstring(void);
void VM_freestring(void);

void VM_servercmd (void);
void VM_clientcmd (void);

void VM_tokenize (void);
void VM_argv (void);

void VM_isserver(void);
void VM_clientcount(void);
void VM_clientstate(void);
void VM_getmousepos(void);
void VM_gettime(void);
void VM_loadfromdata(void);
void VM_parseentitydata(void);
void VM_loadfromfile(void);
void VM_modulo(void);

void VM_search_begin(void);
void VM_search_end(void);
void VM_search_getsize(void);
void VM_search_getfilename(void);
void VM_chr(void);
void VM_iscachedpic(void);
void VM_precache_pic(void);
void VM_drawcharacter(void);
void VM_drawstring(void);
void VM_drawpic(void);
void VM_drawfill(void);
void VM_drawsetcliparea(void);
void VM_drawresetcliparea(void);
void VM_getimagesize(void);

void VM_vectorvectors (void);
void VM_makevectors (void);
void VM_makevectors2 (void);

void VM_keynumtostring (void);
void VM_stringtokeynum (void);

void VM_cin_open( void );
void VM_cin_close( void );
void VM_cin_getstate( void );
void VM_cin_restart( void );

void VM_R_PolygonBegin (void);
void VM_R_PolygonVertex (void);
void VM_R_PolygonEnd (void);

void VM_bitshift (void);

void VM_altstr_count( void );
void VM_altstr_prepare( void );
void VM_altstr_get( void );
void VM_altstr_set( void );
void VM_altstr_ins(void);

void VM_buf_create(void);
void VM_buf_del (void);
void VM_buf_getsize (void);
void VM_buf_copy (void);
void VM_buf_sort (void);
void VM_buf_implode (void);
void VM_bufstr_get (void);
void VM_bufstr_set (void);
void VM_bufstr_add (void);
void VM_bufstr_free (void);

void VM_Cmd_Init(void);
void VM_Cmd_Reset(void);

#endif//PROGSVM_H