//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        vprogs.h - virtual machine export
//=======================================================================
#ifndef VPROGS_H
#define VPROGS_H

#include <windows.h>
#include "basetypes.h"
#include "stdapi.h"
#include "stdref.h"
#include "basefiles.h"
#include "dllapi.h"
#include "pr_local.h"

extern stdlib_api_t	com;
extern vprogs_exp_t	vm;

extern cvar_t *prvm_traceqc;
extern cvar_t *prvm_boundscheck;
extern cvar_t *prvm_statementprofiling;
extern int prvm_developer;

#define Host_Error com.error

#define PROG_CRC_SERVER	1320
#define PROG_CRC_CLIENT	9488
#define PROG_CRC_UIMENU	2460

enum op_state
{
	OP_DONE,		// 0
	OP_MUL_F,
	OP_MUL_V,
	OP_MUL_FV,	// (vec3_t) * (float)
	OP_MUL_VF,          // (float) * (vec3_t)
	OP_DIV_F,
	OP_ADD_F,
	OP_ADD_V,
	OP_SUB_F,
	OP_SUB_V,
	
	OP_EQ_F,		// 10
	OP_EQ_V,
	OP_EQ_S,
	OP_EQ_E,
	OP_EQ_FNC,
	
	OP_NE_F,
	OP_NE_V,
	OP_NE_S,
	OP_NE_E,
	OP_NE_FNC,
	
	OP_LE,		// = (float) <= (float);
	OP_GE,		// = (float) >= (float);
	OP_LT,		// = (float) <  (float);
	OP_GT,		// = (float) >  (float);

	OP_LOAD_F,
	OP_LOAD_V,
	OP_LOAD_S,
	OP_LOAD_ENT,
	OP_LOAD_FLD,
	OP_LOAD_FNC,

	OP_ADDRESS,	// 30

	OP_STORE_F,
	OP_STORE_V,
	OP_STORE_S,
	OP_STORE_ENT,
	OP_STORE_FLD,
	OP_STORE_FNC,

	OP_STOREP_F,
	OP_STOREP_V,
	OP_STOREP_S,
	OP_STOREP_ENT,	// 40
	OP_STOREP_FLD,
	OP_STOREP_FNC,

	OP_RETURN,
	OP_NOT_F,
	OP_NOT_V,
	OP_NOT_S,
	OP_NOT_ENT,
	OP_NOT_FNC,
	OP_IF,
	OP_IFNOT,		// 50
	OP_CALL0,
	OP_CALL1,
	OP_CALL2,
	OP_CALL3,
	OP_CALL4,
	OP_CALL5,
	OP_CALL6,
	OP_CALL7,
	OP_CALL8,
	OP_STATE,		// 60
	OP_GOTO,
	OP_AND,
	OP_OR,
	
	OP_BITAND,	// = (float) & (float); // of cource converting into integer in real code
	OP_BITOR,

	// version 7 started
	OP_MULSTORE_F,	// f *= f
	OP_MULSTORE_V,	// v *= f
	OP_MULSTOREP_F,	// e.f *= f
	OP_MULSTOREP_V,	// e.v *= f

	OP_DIVSTORE_F,	// f /= f
	OP_DIVSTOREP_F,	// e.f /= f

	OP_ADDSTORE_F,	// f += f
	OP_ADDSTORE_V,	// v += v
	OP_ADDSTOREP_F,	// e.f += f
	OP_ADDSTOREP_V,	// e.v += v

	OP_SUBSTORE_F,	// f -= f
	OP_SUBSTORE_V,	// v -= v
	OP_SUBSTOREP_F,	// e.f -= f
	OP_SUBSTOREP_V,	// e.v -= v

	OP_FETCH_GBL_F,	// 80
	OP_FETCH_GBL_V,
	OP_FETCH_GBL_S,
	OP_FETCH_GBL_E,
	OP_FETCH_G_FNC,

	OP_CSTATE,
	OP_CWSTATE,

	OP_THINKTIME,

	OP_BITSET,	// b  (+) a
	OP_BITSETP,	// .b (+) a
	OP_BITCLR,	// b  (-) a
	OP_BITCLRP,	// .b (-) a

	OP_RAND0,
	OP_RAND1,
	OP_RAND2,
	OP_RANDV0,
	OP_RANDV1,
	OP_RANDV2,

	OP_SWITCH_F,	// switches
	OP_SWITCH_V,
	OP_SWITCH_S,	// 100
	OP_SWITCH_E,
	OP_SWITCH_FNC,

	OP_CASE,
	OP_CASERANGE,

	OP_STORE_I,
	OP_STORE_IF,
	OP_STORE_FI,
	
	OP_ADD_I,
	OP_ADD_FI,
	OP_ADD_IF,	// 110
  
	OP_SUB_I,
	OP_SUB_FI,
	OP_SUB_IF,

	OP_CONV_ITOF,
	OP_CONV_FTOI,
	OP_CP_ITOF,
	OP_CP_FTOI,
	OP_LOAD_I,
	OP_STOREP_I,
	OP_STOREP_IF,	// 120
	OP_STOREP_FI,

	OP_BITAND_I,
	OP_BITOR_I,

	OP_MUL_I,
	OP_DIV_I,
	OP_EQ_I,
	OP_NE_I,

	OP_IFNOTS,
	OP_IFS,

	OP_NOT_I,		// 130

	OP_DIV_VF,

	OP_POWER_I,
	OP_RSHIFT_I,
	OP_LSHIFT_I,

	OP_GLOBAL_ADD,
	OP_POINTER_ADD,	// pointer to 32 bit (remember to *3 for vectors)

	OP_LOADA_F,
	OP_LOADA_V,	
	OP_LOADA_S,
	OP_LOADA_ENT,	// 140
	OP_LOADA_FLD,		
	OP_LOADA_FNC,
	OP_LOADA_I,

	OP_STORE_P,
	OP_LOAD_P,

	OP_LOADP_F,
	OP_LOADP_V,	
	OP_LOADP_S,
	OP_LOADP_ENT,
	OP_LOADP_FLD,	// 150
	OP_LOADP_FNC,
	OP_LOADP_I,

	OP_LE_I,            // (int)c = (int)a <= (int)b;
	OP_GE_I,		// (int)c = (int)a >= (int)b;
	OP_LT_I,		// (int)c = (int)a <  (int)b;
	OP_GT_I,		// (int)c = (int)a >  (int)b;

	OP_LE_IF,           // (float)c = (int)a <= (float)b;
	OP_GE_IF,		// (float)c = (int)a >= (float)b;
	OP_LT_IF,		// (float)c = (int)a <  (float)b;
	OP_GT_IF,		// (float)c = (int)a >  (float)b;

	OP_LE_FI,		// (float)c = (float)a <= (int)b;
	OP_GE_FI,		// (float)c = (float)a >= (int)b;
	OP_LT_FI,		// (float)c = (float)a <  (int)b;
	OP_GT_FI,		// (float)c = (float)a >  (int)b;

	OP_EQ_IF,
	OP_EQ_FI,

	OP_ADD_SF,	// (char*)c = (char*)a + (float)b
	OP_SUB_S,		// (float)c = (char*)a - (char*)b
	OP_STOREP_C,	// (float)c = *(char*)b = (float)a
	OP_LOADP_C,	// (float)c = *(char*) // 170

	OP_MUL_IF,
	OP_MUL_FI,
	OP_MUL_VI,
	OP_MUL_IV,
	OP_DIV_IF,
	OP_DIV_FI,
	OP_BITAND_IF,
	OP_BITOR_IF,
	OP_BITAND_FI,
	OP_BITOR_FI,	// 180
	OP_AND_I,
	OP_OR_I,
	OP_AND_IF,
	OP_OR_IF,
	OP_AND_FI,
	OP_OR_FI,
	OP_NE_IF,
	OP_NE_FI,

	OP_GSTOREP_I,
	OP_GSTOREP_F,	// 190		
	OP_GSTOREP_ENT,
	OP_GSTOREP_FLD,	// integers
	OP_GSTOREP_S,
	OP_GSTOREP_FNC,	// pointers
	OP_GSTOREP_V,
	OP_GADDRESS,
	OP_GLOAD_I,
	OP_GLOAD_F,
	OP_GLOAD_FLD,
	OP_GLOAD_ENT,	// 200
	OP_GLOAD_S,
	OP_GLOAD_FNC,
	OP_GLOAD_V,
	OP_BOUNDCHECK,

	OP_STOREP_P,	// back to ones that we do use.	
	OP_PUSH,
	OP_POP,

	// version 8 started
	OP_NUMOPS,
};

#define PRVM_GETEDICTFIELDVALUE(ed, fieldoffset) (fieldoffset ? (prvm_eval_t *)((byte *)ed->progs.vp + fieldoffset) : NULL)
#define PRVM_GETGLOBALFIELDVALUE(fieldoffset) (fieldoffset ? (prvm_eval_t *)((byte *)vm.prog->globals.gp + fieldoffset) : NULL)

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

// console commands
void PRVM_ED_PrintEdicts_f( void );
void PRVM_ED_PrintEdict_f( void );
void PRVM_ED_EdictSet_f( void );
void PRVM_GlobalSet_f( void );
void PRVM_ED_Count_f( void );
void PRVM_Globals_f( void );
void PRVM_Global_f( void );
void PRVM_Fields_f( void );

void PRVM_ExecuteProgram( func_t fnum, const char *name, const char *file, const int line );

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
const char *PRVM_ED_ParseEdict (const char *data, edict_t *ent);

void PRVM_ED_WriteGlobals (vfile_t *f);
void PRVM_ED_ParseGlobals (const char *data);

void PRVM_ED_LoadFromFile (const char *data);

edict_t *PRVM_EDICT_NUM_ERROR(int n, char *filename, int fileline);
#define PRVM_EDICT_NUM(n) (((n) >= 0 && (n) < vm.prog->max_edicts) ? vm.prog->edicts + (n) : PRVM_EDICT_NUM_ERROR(n, __FILE__, __LINE__))
#define PRVM_EDICT_NUM_UNSIGNED(n) (((n) < vm.prog->max_edicts) ? vm.prog->edicts + (n) : PRVM_EDICT_NUM_ERROR(n, __FILE__, __LINE__))
#define PRVM_NUM_FOR_EDICT(e) ((int)((edict_t *)(e) - vm.prog->edicts))
#define PRVM_NEXT_EDICT(e) ((e) + 1)
#define PRVM_EDICT_TO_PROG(e) (PRVM_NUM_FOR_EDICT(e))
#define PRVM_PROG_TO_EDICT(n) (PRVM_EDICT_NUM(n))
#define PRVM_ED_POINTER(p) (prvm_eval_t *)((byte *)vm.prog->edictsfields + p->_int)
#define PRVM_EM_POINTER(p) (prvm_eval_t *)((byte *)vm.prog->edictsfields + (p))
#define PRVM_EV_POINTER(p) (prvm_eval_t *)(((byte *)vm.prog->edicts) + p->_int) 	// this is correct ???
#define PRVM_CHECK_PTR(p, size) if(prvm_boundscheck->value && (p->_int < 0 || p->_int + size > vm.prog->edictareasize))\
{\
vm.prog->xfunction->profile += (st - startst);\
vm.prog->xstatement = st - vm.prog->statements;\
PRVM_ERROR("%s attempted to write to an out of bounds edict (%i)", PRVM_NAME, p->_int);\
return;\
}
#define PRVM_CHECK_INFINITE() if (++jumpcount == 10000000)\
{\
vm.prog->xstatement = st - vm.prog->statements;\
PRVM_Profile(1<<30, 1000000);\
PRVM_ERROR("runaway loop counter hit limit of %d jumps\n", jumpcount, PRVM_NAME);\
}
//============================================================================

// get arguments from progs
#define	PRVM_G_FLOAT(o) (vm.prog->globals.gp[o])
#define	PRVM_G_INT(o) (*(int *)&vm.prog->globals.gp[o])
#define	PRVM_G_EDICT(o) (PRVM_PROG_TO_EDICT(*(int *)&vm.prog->globals.gp[o]))
#define	PRVM_G_EDICTNUM(o) PRVM_NUM_FOR_EDICT(PRVM_G_EDICT(o))
#define	PRVM_G_VECTOR(o) (&vm.prog->globals.gp[o])
#define	PRVM_G_STRING(o) (PRVM_GetString(*(string_t *)&vm.prog->globals.gp[o]))
#define	PRVM_G_FUNCTION(o) (*(func_t *)&vm.prog->globals.gp[o])

// FIXME: make these go away?
#define	PRVM_E_FLOAT(e,o) (((float*)e->progs.vp)[o])
#define	PRVM_E_INT(e,o) (((int*)e->progs.vp)[o])
//#define	PRVM_E_VECTOR(e,o) (&((float*)e->progs.vp)[o])
#define	PRVM_E_STRING(e,o) (PRVM_GetString(*(string_t *)&((float*)e->progs.vp)[o]))

extern int prvm_type_size[8]; // for consistency : I think a goal of this sub-project is to
// make the new vm mostly independent from the old one, thus if it's necessary, I copy everything

void PRVM_Init_Exec(void);
void PRVM_StackTrace (void);
void PRVM_ED_PrintEdicts_f (void);
void PRVM_ED_PrintNum (int ent);

const char *PRVM_GetString(int num);
int PRVM_SetEngineString(const char *s);
int PRVM_SetTempString( const char *s );
int PRVM_AllocString(size_t bufferlength, char **pointer);
void PRVM_FreeString(int num);

//============================================================================
#define PRVM_NAME	(vm.prog->name ? vm.prog->name : "unnamed.dat")

// helper macro to make function pointer calls easier
#define PRVM_GCALL(func) if(vm.prog->func) vm.prog->func
#define PRVM_ERROR if(vm.prog) vm.prog->error_cmd

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

#endif//VPROGS_H