//=======================================================================
//			Copyright XashXT Group 2007 ©
//			ui_cmds.c - ui menu builtins
//=======================================================================

#include "uimenu.h"

/*
=========
PF_substring

string substring( string s, float start, float length )
=========
*/
void PF_substring( void )
{
	static string	tempstring;
	int		i, start, length;
	const char	*s;

	if(!VM_ValidateArgs( "substring", 3 ))
		return;

	s = PRVM_G_STRING(OFS_PARM0);
	start = (int)PRVM_G_FLOAT(OFS_PARM1);
	length = (int)PRVM_G_FLOAT(OFS_PARM2);
	if(!s) s = ""; // evil stuff...

	for (i = 0; i < start && *s; i++, s++ );
	for (i = 0; i < MAX_STRING - 1 && *s && i < length; i++, s++)
		tempstring[i] = *s;
	tempstring[i] = 0;

	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString( tempstring );
}

/*
=========
PF_serverstate

float serverstate( void )
=========
*/
void PF_serverstate( void )
{
	if(!VM_ValidateArgs( "serverstate", 0 ))
		return;
	PRVM_G_FLOAT(OFS_RETURN) = Host_ServerState();
}

/*
=========
PF_clientstate

float clientstate( void )
=========
*/
void PF_clientstate( void )
{
	if(!VM_ValidateArgs( "clientstate", 0 ))
		return;
	PRVM_G_FLOAT(OFS_RETURN) = cls.state;
}

/*
=========
PF_localsound

void localsound( string sample )
=========
*/
void PF_localsound(void)
{
	const char *s;

	if(!VM_ValidateArgs( "localsound", 1 ))
		return;

	s = PRVM_G_STRING( OFS_PARM0 );

	if(!S_StartLocalSound(s))
	{
		VM_Warning("PF_localsound: can't play %s!\n", s );
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		return;
	}
	PRVM_G_FLOAT(OFS_RETURN) = 1;
}

/*
=========
PF_getmousepos

vector getmousepos( void )
=========
*/
void PF_getmousepos( void )
{
	if(!VM_ValidateArgs( "getmousepos", 0 ))
		return;

	PRVM_G_VECTOR(OFS_RETURN)[0] = mouse_x;
	PRVM_G_VECTOR(OFS_RETURN)[1] = mouse_y;
	PRVM_G_VECTOR(OFS_RETURN)[2] = 0;
}

/*
=========
PF_loadfromdata

void loadfromdata( string data )
=========
*/
void PF_loadfromdata( void )
{
	if(!VM_ValidateArgs( "loadfromdata", 1 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	PRVM_ED_LoadFromFile(PRVM_G_STRING(OFS_PARM0));
}

/*
=========
PF_loadfromfile

float loadfromfile( string file )
=========
*/
void PF_loadfromfile( void )
{
	char	*data;

	if(!VM_ValidateArgs( "loadfromfile", 1 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	data = (char *)FS_LoadFile( PRVM_G_STRING(OFS_PARM0), NULL );
	if( !data ) PRVM_G_FLOAT(OFS_RETURN) = false;

	PRVM_ED_LoadFromFile( data );
	PRVM_G_FLOAT(OFS_RETURN) = true;
}

/*
=========
PF_precache_pic

float precache_pic( string pic )
=========
*/
void PF_precache_pic( void )
{
	if(!VM_ValidateArgs( "precache_pic", 1 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	if(re->RegisterPic((char *)PRVM_G_STRING(OFS_PARM0)))
		PRVM_G_FLOAT(OFS_RETURN) = true;
	else PRVM_G_FLOAT(OFS_RETURN) = false;
}

/*
=========
PF_drawcharacter

float drawchar( vector pos, float char, vector scale, vector rgb, float alpha )
=========
*/
void PF_drawcharacter( void )
{
	char	character;
	float	*pos, *rgb, *scale, alpha;

	if(!VM_ValidateArgs( "drawchar", 5 ))
		return;

	character = (char)PRVM_G_FLOAT(OFS_PARM1);
	if( character == 0 )
	{
		PRVM_G_FLOAT(OFS_RETURN) = false;
		VM_Warning( "PF_drawcharacter: %s passed null character!\n", PRVM_NAME );
		return;
	}

	pos = PRVM_G_VECTOR(OFS_PARM0);
	scale = PRVM_G_VECTOR(OFS_PARM2);
	rgb = PRVM_G_VECTOR(OFS_PARM3);
	alpha = PRVM_G_FLOAT(OFS_PARM4);

	re->SetColor( GetRGBA(rgb[0], rgb[1], rgb[2], alpha ));
	SCR_DrawChar( pos[0], pos[1], scale[0], scale[1], character );
	re->SetColor( NULL );
	PRVM_G_FLOAT(OFS_RETURN) = true;
}

/*
=========
PF_drawstring

float drawstring( vector pos, string text, vector scale, vector rgb, float alpha )
=========
*/
void PF_drawstring( void )
{
	float		*pos, *scale, *rgb, *rgba, alpha;
	const char	*string;

	if(!VM_ValidateArgs( "drawstring", 5 ))
		return;

	string = PRVM_G_STRING(OFS_PARM1);
	if( !string )
	{
		PRVM_G_FLOAT(OFS_RETURN) = false;
		VM_Warning( "PF_drawstring: %s passed null string!\n", PRVM_NAME );
		return;
	}

	pos = PRVM_G_VECTOR(OFS_PARM0);
	scale = PRVM_G_VECTOR(OFS_PARM2);
	rgb = PRVM_G_VECTOR(OFS_PARM3);
	alpha = PRVM_G_FLOAT(OFS_PARM4);
	rgba = GetRGBA(rgb[0], rgb[1], rgb[2], alpha );

	SCR_DrawStringExt( pos[0], pos[1], scale[0], scale[1], string, rgba, true ); 
	PRVM_G_FLOAT(OFS_RETURN) = true;
}

/*
=========
PF_drawpic

float drawpic( vector pos, string pic, vector size, vector rgb, float alpha )
=========
*/
void PF_drawpic( void )
{
	const char	*picname;
	float		*size, *pos, *rgb, alpha;

	if(!VM_ValidateArgs( "drawpic", 5 ))
		return;

	picname = PRVM_G_STRING(OFS_PARM1);
	if(!picname)
	{
		VM_Warning( "PF_drawpic: %s passed null picture name!\n", PRVM_NAME );
		PRVM_G_FLOAT(OFS_RETURN) = false;
		return;
	}

	VM_ValidateString(PRVM_G_STRING(OFS_PARM1));
	pos = PRVM_G_VECTOR(OFS_PARM0);
	size = PRVM_G_VECTOR(OFS_PARM2);
	rgb = PRVM_G_VECTOR(OFS_PARM3);
	alpha = PRVM_G_FLOAT(OFS_PARM4);

	re->SetColor( GetRGBA(rgb[0], rgb[1], rgb[2], alpha ));
	SCR_DrawPic( pos[0], pos[1], size[0], size[1], (char *)picname );
	re->SetColor( NULL );
	PRVM_G_FLOAT(OFS_RETURN) = true;
}

/*
=========
PF_drawfill

void drawfill( vector pos, vector size, vector rgb, float alpha )
=========
*/
void PF_drawfill( void )
{
	float	*size, *pos, *rgb, alpha;

	if(!VM_ValidateArgs( "drawfill", 4 ))
		return;

	pos = PRVM_G_VECTOR(OFS_PARM0);
	size = PRVM_G_VECTOR(OFS_PARM1);
	rgb = PRVM_G_VECTOR(OFS_PARM2);
	alpha = PRVM_G_FLOAT(OFS_PARM3);

	SCR_FillRect( pos[0], pos[1], size[0], size[1], GetRGBA( rgb[0], rgb[1], rgb[2], alpha )); 
}

/*
=========
PF_getimagesize

vector getimagesize( string pic )
=========
*/
void PF_getimagesize( void )
{
	const char	*p;
	int		w, h;

	if(!VM_ValidateArgs( "getimagesize", 1 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	p = PRVM_G_STRING(OFS_PARM0);

	re->DrawGetPicSize( &w, &h, (char *)p);
	VectorSet(PRVM_G_VECTOR(OFS_RETURN), w, h, 0 ); 
}

/*
=========
PF_setkeydest

void setkeydest( float dest )
=========
*/
void PF_setkeydest( void )
{
	if(!VM_ValidateArgs( "setkeydest", 1 ))
		return;
        
	switch((int)PRVM_G_FLOAT(OFS_PARM0))
	{
	case key_game:
		cls.key_dest = key_game;
		break;
	case key_menu:
		cls.key_dest = key_menu;
		break;
	case key_message:
		cls.key_dest = key_message;
		break;
	default:
		PRVM_ERROR( "PF_setkeydest: wrong destination %f!", PRVM_G_FLOAT(OFS_PARM0));
		break;
	}
}

/*
=========
PF_callfunction

void callfunction( ..., string function_name )
=========
*/
void PF_callfunction( void )
{
	mfunction_t	*func;
	const char	*s;

	if( prog->argc == 0 ) PRVM_ERROR( "PF_callfunction: 1 parameter is required!" );
	s = PRVM_G_STRING( OFS_PARM0 + (prog->argc - 1));
	if(!s) PRVM_ERROR("VM_M_callfunction: null string!");

	VM_ValidateString(s);
	func = PRVM_ED_FindFunction( s );

	if(!func) 
	{
		PRVM_ERROR( "PF_callfunciton: function %s not found!", s );
	}
	else if( func->first_statement < 0 )
	{
		// negative statements are built in functions
		int builtinnumber = -func->first_statement;
		prog->xfunction->builtinsprofile++;
		if( builtinnumber < prog->numbuiltins && prog->builtins[builtinnumber] )
			prog->builtins[builtinnumber]();
		else PRVM_ERROR("No such builtin #%i in %s", builtinnumber, PRVM_NAME );
	}
	else if( func - prog->functions > 0 )
	{
		prog->argc--;
		PRVM_ExecuteProgram( func - prog->functions, "" );
		prog->argc++;
	}
}

/*
=========
PF_testfunction

float testfunction( string function_name )
=========
*/
void PF_testfunction(void)
{
	mfunction_t	*func;
	const char	*s;

	if(!VM_ValidateArgs( "testfunction", 1 ))
		return;

	s = PRVM_G_STRING(OFS_PARM0);
	if(!s) PRVM_ERROR("PF_testfunction: null string !");
	VM_ValidateString( s );

	func = PRVM_ED_FindFunction( s );

	if(func) PRVM_G_FLOAT(OFS_RETURN) = true;
	else PRVM_G_FLOAT(OFS_RETURN) = false;
}

prvm_builtin_t vm_ui_builtins[] = 
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
VM_ComVA,				// #28 string va( ... )
VM_ComStrlen,			// #29 float strlen( string text )
VM_TimeStamp,			// #30 string Com_TimeStamp( float format )
VM_LocalCmd,			// #31 void LocalCmd( ... )
NULL,				// #32 -- reserved --
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

// uimenufuncs_t
VM_FindEdict,			// #101 entity find( entity start, .string field, string match )
VM_FindField,			// #102 entity findfloat( entity start, .float field, float match )
PF_substring,			// #103 string substring( string s, float start, float length )
PF_serverstate,			// #104 float serverstate( void )
PF_clientstate,			// #105 float clientstate( void )
PF_localsound,			// #106 void localsound( string sample )
PF_getmousepos,			// #107 vector getmousepos( void )
PF_loadfromdata,			// #108 void loadfromdata( string data )
PF_loadfromfile,			// #109 float loadfromfile( string file )
PF_precache_pic,			// #110 float precache_pic( string pic )
PF_drawcharacter,			// #111 float drawchar( vector pos, float char, vector scale, vector rgb, float alpha )
PF_drawstring,			// #112 float drawstring( vector pos, string text, vector scale, vector rgb, float alpha )
PF_drawpic,			// #113 float drawpic( vector pos, string pic, vector size, vector rgb, float alpha )
PF_drawfill,			// #114 void drawfill( vector pos, vector size, vector rgb, float alpha )
PF_getimagesize,			// #115 vector getimagesize( string pic )
PF_setkeydest,			// #116 void setkeydest( float dest )
PF_callfunction,			// #117 void callfunction( ..., string function_name )
PF_testfunction,			// #118 float testfunction( string function_name )
};

const int vm_ui_numbuiltins = sizeof(vm_ui_builtins) / sizeof(prvm_builtin_t);