//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        cl_progs.c - client.dat interface
//=======================================================================

#include "common.h"
#include "client.h"

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
		if( ent->priv.cl->free ) continue;

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
NULL,				// #0  (leave blank as default, but can include easter egg ) 

// system events
VM_ConPrintf,			// #1  void Con_Printf( ... )
VM_ConDPrintf,			// #2  void Con_DPrintf( float level, ... )
VM_HostError,			// #3  void Com_Error( ... )
VM_SysExit,			// #4  void Sys_Exit( void )
VM_CmdArgv,			// #5  string Cmd_Argv( float arg )
VM_CmdArgc,			// #6  float Cmd_Argc( void )
NULL,				// #7  -- reserved --
NULL,				// #8  -- reserved --
NULL,				// #9  -- reserved --
NULL,				// #10 -- reserved --

// common tools
VM_ComTrace,			// #11 void Com_Trace( float enable )
VM_ComFileExists,			// #12 float Com_FileExists( string filename )
VM_ComFileSize,			// #13 float Com_FileSize( string filename )
VM_ComFileTime,			// #14 float Com_FileTime( string filename )
VM_ComLoadScript,			// #15 float Com_LoadScript( string filename )
VM_ComResetScript,			// #16 void Com_ResetScript( void )
VM_ComReadToken,			// #17 string Com_ReadToken( float newline )
VM_ComFilterToken,			// #18 float Com_Filter( string mask, string s, float casecmp )
VM_ComSearchFiles,			// #19 float Com_Search( string mask, float casecmp )
VM_ComSearchNames,			// #20 string Com_SearchFilename( float num )
VM_RandomLong,			// #21 float RandomLong( float min, float max )
VM_RandomFloat,			// #22 float RandomFloat( float min, float max )
VM_RandomVector,			// #23 vector RandomVector( vector min, vector max )
VM_CvarRegister,			// #24 void Cvar_Register( string name, string value, float flags )
VM_CvarSetValue,			// #25 void Cvar_SetValue( string name, float value )
VM_CvarGetValue,			// #26 float Cvar_GetValue( string name )
VM_CvarSetString,			// #27 void Cvar_SetString( string name, string value )
VM_CvarGetString,			// #28 void VM_CvarGetString( void )
VM_ComVA,				// #29 string va( ... )
VM_ComStrlen,			// #30 float strlen( string text )
VM_TimeStamp,			// #31 string Com_TimeStamp( float format )
VM_LocalCmd,			// #32 void LocalCmd( ... )
NULL,				// #33 -- reserved --
NULL,				// #34 -- reserved --
NULL,				// #35 -- reserved --
NULL,				// #36 -- reserved --
NULL,				// #37 -- reserved --
NULL,				// #38 -- reserved --
NULL,				// #39 -- reserved --
NULL,				// #40 -- reserved --

// quakec intrinsics ( compiler will be lookup this functions, don't remove or rename )
VM_SpawnEdict,			// #41 entity spawn( void )
VM_RemoveEdict,			// #42 void remove( entity ent )
VM_NextEdict,			// #43 entity nextent( entity ent )
VM_CopyEdict,			// #44 void copyentity( entity src, entity dst )
NULL,				// #45 -- reserved --
NULL,				// #46 -- reserved --
NULL,				// #47 -- reserved --
NULL,				// #48 -- reserved --
NULL,				// #49 -- reserved --
NULL,				// #50 -- reserved --

// filesystem
VM_FS_Open,			// #51 float fopen( string filename, float mode )
VM_FS_Close,			// #52 void fclose( float handle )
VM_FS_Gets,			// #53 string fgets( float handle )
VM_FS_Puts,			// #54 void fputs( float handle, string s )
NULL,				// #55 -- reserved --
NULL,				// #56 -- reserved --
NULL,				// #57 -- reserved --
NULL,				// #58 -- reserved --
NULL,				// #59 -- reserved --
NULL,				// #60 -- reserved --

// mathlib
VM_min,				// #61 float min(float a, float b )
VM_max,				// #62 float max(float a, float b )
VM_bound,				// #63 float bound(float min, float val, float max)
VM_pow,				// #64 float pow(float x, float y)
VM_sin,				// #65 float sin(float f)
VM_cos,				// #66 float cos(float f)
VM_tan,				// #67 float tan(float f)
VM_asin,				// #68 float asin(float f)
VM_acos,				// #69 float acos(float f)
VM_atan,				// #70 float atan(float f)
VM_sqrt,				// #71 float sqrt(float f)
VM_rint,				// #72 float rint (float v)
VM_floor,				// #73 float floor(float v)
VM_ceil,				// #74 float ceil (float v)
VM_fabs,				// #75 float fabs (float f)
VM_mod,				// #76 float fmod( float val, float m )
NULL,				// #77 -- reserved --
NULL,				// #78 -- reserved --
VM_VectorNormalize,			// #79 vector VectorNormalize( vector v )
VM_VectorLength,			// #80 float VectorLength( vector v )
e10, e10,				// #81 - #100 are reserved for future expansions

// network messaging
PF_BeginRead,			// #101 void MsgBegin( void )
NULL,				// #102
PF_ReadChar,			// #103 float ReadChar (float f)
PF_ReadShort,			// #104 float ReadShort (float f)
PF_ReadLong,			// #105 float ReadLong (float f)
PF_ReadFloat,			// #106 float ReadFloat (float f)
PF_ReadAngle,			// #107 float ReadAngle (float f)
PF_ReadCoord,			// #108 float ReadCoord (float f)
PF_ReadString,			// #109 string ReadString (string s)
PF_ReadEntity,			// #110 entity ReadEntity (entity s)
PF_EndRead,			// #111 void MsgEnd( void )

// clientfuncs_t
PF_ScreenAdjustSize,		// #112 void SCR_AdjustSize( void )
PF_FillRect,			// #113 void SCR_FillRect( float x, float y, float w, float h, vector col )
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