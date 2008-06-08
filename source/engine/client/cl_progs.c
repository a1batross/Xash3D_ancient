//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        cl_progs.c - client.dat interface
//=======================================================================

#include "client.h"
#include "sv_edict.h"
#include "cl_edict.h"

/*
===============================================================================
Client Builtin Functions

mathlib, debugger, and various misc helpers
===============================================================================
*/
void PF_ScreenAdjustSize( void )
{
	float	xscale, yscale;

	// scale for screen sizes
	xscale = scr_width->integer / 640.0f;
	yscale = scr_height->integer / 480.0f;

	prog->globals.cl->x_pos *= xscale;
	prog->globals.cl->y_pos *= yscale;
	prog->globals.cl->scr_width *= xscale;
	prog->globals.cl->scr_height *= yscale;
}

/*
================
PF_FillRect

Coordinates are 640*480 virtual values
=================
*/
void PF_FillRect( void )
{
	float	*rgb, x, y, width, height;
	vec4_t	color;

	x = PRVM_G_FLOAT(OFS_PARM0);
	y = PRVM_G_FLOAT(OFS_PARM1);
	width = PRVM_G_FLOAT(OFS_PARM2);
	height = PRVM_G_FLOAT(OFS_PARM3);
	rgb = PRVM_G_VECTOR(OFS_PARM4);

	Vector4Set( color, rgb[0], rgb[1], rgb[2], 1.0f );	
	re->SetColor( color );

	SCR_AdjustSize( &x, &y, &width, &height );
	re->DrawFill( x, y, width, height );
	re->SetColor( NULL );
}

void CL_BeginIncreaseEdicts( void )
{
	// links don't survive the transition, so unlink everything
}

void CL_EndIncreaseEdicts( void )
{
}

void CL_InitEdict( edict_t *e )
{
}

void CL_FreeEdict( edict_t *ed )
{
}

void CL_CountEdicts( void )
{
	int	i;
	edict_t	*ent;
	int	active = 0, models = 0, solid = 0;

	for (i = 0; i < prog->num_edicts; i++ )
	{
		ent = PRVM_EDICT_NUM(i);
		if( ent->priv.sv->free ) continue;

		active++;
		if( ent->progs.cl->solid ) solid++;
		if( ent->progs.cl->model ) models++;
	}

	Msg("num_edicts:%3i\n", prog->num_edicts);
	Msg("active    :%3i\n", active);
	Msg("view      :%3i\n", models);
	Msg("touch     :%3i\n", solid);
}

void CL_VM_Begin( void )
{
	PRVM_Begin;
	PRVM_SetProg( PRVM_CLIENTPROG );

	if( prog ) *prog->time = cl.time;
}

void CL_VM_End( void )
{
	PRVM_End;
}

bool CL_LoadEdict( edict_t *ent )
{
	return true;
}

void PF_BeginRead( void )
{
}

void PF_EndRead( void )
{
}

void PF_ReadChar (void){ PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadChar( cls.multicast ); }
void PF_ReadShort (void){ PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadShort( cls.multicast ); }
void PF_ReadLong (void){ PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadLong( cls.multicast ); }
void PF_ReadFloat (void){ PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadFloat( cls.multicast ); }
void PF_ReadAngle (void){ PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadAngle32( cls.multicast ); }
void PF_ReadCoord (void){ PRVM_G_FLOAT(OFS_RETURN) = MSG_ReadCoord32( cls.multicast ); }
void PF_ReadString (void){ PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString( MSG_ReadString( cls.multicast) ); }
void PF_ReadEntity (void){ VM_RETURN_EDICT( PRVM_PROG_TO_EDICT( MSG_ReadShort( cls.multicast ))); } // entindex

//NOTE: intervals between various "interfaces" was leave for future expansions
prvm_builtin_t vm_cl_builtins[] = 
{
NULL,					// #0

// network messaging
PF_BeginRead,				// #1 void MsgBegin( void )
NULL,					// #2
PF_ReadChar,				// #3 float ReadChar (float f)
PF_ReadShort,				// #4 float ReadShort (float f)
PF_ReadLong,				// #5 float ReadLong (float f)
PF_ReadFloat,				// #6 float ReadFloat (float f)
PF_ReadAngle,				// #7 float ReadAngle (float f)
PF_ReadCoord,				// #8 float ReadCoord (float f)
PF_ReadString,				// #9 string ReadString (string s)
PF_ReadEntity,				// #10 entity ReadEntity (entity s)
PF_EndRead,				// #11 void MsgEnd( void )

// mathlib
VM_min,					// #12 float min(float a, float b )
VM_max,					// #13 float max(float a, float b )
VM_bound,					// #14 float bound(float min, float val, float max)
VM_pow,					// #15 float pow(float x, float y)
VM_sin,					// #16 float sin(float f)
VM_cos,					// #17 float cos(float f)
VM_sqrt,					// #18 float sqrt(float f)
VM_rint,					// #19 float rint (float v)
VM_floor,					// #20 float floor(float v)
VM_ceil,					// #21 float ceil (float v)
VM_fabs,					// #22 float fabs (float f)
VM_random_long,				// #23 float random_long( void )
VM_random_float,				// #24 float random_float( void )
NULL,                                             // #25
NULL,                                             // #26
NULL,                                             // #27
NULL,                                             // #28
NULL,                                             // #29
NULL,                                             // #30

// vector mathlib
VM_normalize,				// #31 vector normalize(vector v) 
VM_veclength,				// #32 float veclength(vector v) 
VM_vectoyaw,				// #33 float vectoyaw(vector v) 
VM_vectoangles,				// #34 vector vectoangles(vector v) 
VM_randomvec,				// #35 vector randomvec( void )
VM_vectorvectors,				// #36 void vectorvectors(vector dir)
VM_makevectors,				// #37 void makevectors(vector dir)
VM_makevectors2,				// #38 void makevectors2(vector dir)
NULL,					// #39
NULL,					// #40

// stdlib functions
VM_atof,					// #41 float atof(string s)
VM_ftoa,					// #42 string ftoa(float s)
VM_vtoa,					// #43 string vtoa(vector v)
VM_atov,					// #44 vector atov(string s)
VM_ConPrintf,				// #45 void Msg( ... )
VM_wprint,				// #46 void MsgWarn( ... )
VM_objerror,				// #47 void Error( ... )
VM_bprint,				// #48 void bprint(string s)
NULL,					// #49
NULL,					// #50 
VM_cvar,					// #51 float cvar(string s)
VM_cvar_set,				// #52 void cvar_set(string var, string val)
VM_allocstring,				// #53 string AllocString(string s)
VM_freestring,				// #54 void FreeString(string s)
VM_strlen,				// #55 float strlen(string s)
VM_strcat,				// #56 string strcat(string s1, string s2)
VM_argv,					// #57 string argv( float parm )
NULL,					// #58
NULL,					// #59
NULL,					// #60

// internal debugger
VM_break,					// #61 void break( void )
VM_crash,					// #62 void crash( void )
VM_coredump,				// #63 void coredump( void )
VM_stackdump,				// #64 void stackdump( void )
VM_traceon,				// #65 void trace_on( void )
VM_traceoff,				// #66 void trace_off( void )
VM_eprint,				// #67 void dump_edict(entity e) 
VM_nextent,				// #68 entity nextent(entity e)
NULL,					// #69
NULL,					// #70
// clientfuncs_t
PF_ScreenAdjustSize,			// #71 void SCR_AdjustSize( void )
PF_FillRect,				// #72 void SCR_FillRect( float x, float y, float w, float h, vector col )
VM_create,				// #73
};

const int vm_cl_numbuiltins = sizeof(vm_cl_builtins) / sizeof(prvm_builtin_t); //num of builtins

void CL_InitClientProgs( void )
{
	Msg("\n");
	PRVM_Begin;
	PRVM_InitProg( PRVM_CLIENTPROG );

	if( !prog->loaded )
	{       
		prog->progs_mempool = Mem_AllocPool( "Client Progs" );
		prog->name = "client";
		prog->builtins = vm_cl_builtins;
		prog->numbuiltins = vm_cl_numbuiltins;
		prog->edictprivate_size = sizeof(cl_edict_t);
		prog->num_edicts = 1;
		prog->max_edicts = 512;
		prog->limit_edicts = MAX_EDICTS;
		prog->begin_increase_edicts = CL_BeginIncreaseEdicts;
		prog->end_increase_edicts = CL_EndIncreaseEdicts;
		prog->init_edict = CL_InitEdict;
		prog->free_edict = CL_FreeEdict;
		prog->count_edicts = CL_CountEdicts;
		prog->load_edict = CL_LoadEdict;
		prog->extensionstring = "";

		// using default builtins
		prog->init_cmd = VM_Cmd_Init;
		prog->reset_cmd = VM_Cmd_Reset;
		prog->error_cmd = VM_Error;
		prog->flag |= PRVM_OP_STATE;
		PRVM_LoadProgs( GI->client_prog, 0, NULL, CL_NUM_REQFIELDS, cl_reqfields );
	}

	// init some globals
	prog->globals.cl->time = cl.time;
	prog->globals.cl->pev = 0;
	prog->globals.cl->mapname = PRVM_SetEngineString( cls.servername );
	prog->globals.cl->player_localentnum = cl.playernum;

	// call the prog init
	PRVM_ExecuteProgram( prog->globals.cl->ClientInit, "QC function ClientInit is missing");
	PRVM_End;
}

void CL_FreeClientProgs( void )
{
	CL_VM_Begin();

	prog->globals.cl->time = cl.time;
	prog->globals.cl->pev = 0;
	PRVM_ExecuteProgram(prog->globals.cl->ClientFree, "QC function ClientFree is missing");
	PRVM_ResetProg();

	CL_VM_End();
}