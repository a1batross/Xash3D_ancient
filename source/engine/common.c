//=======================================================================
//			Copyright XashXT Group 2008 ©
//		common.c - misc functions used in client and server
//=======================================================================

#include "common.h"
#include "basefiles.h"
#include "mathlib.h"
#include "client.h"

/*
===============================================================================

SYSTEM IO

===============================================================================
*/
void Sys_Error( const char *error, ... )
{
	char		errorstring[MAX_INPUTLINE];
	static bool	recursive = false;
	va_list		argptr;

	va_start( argptr, error );
	com.vsprintf( errorstring, error, argptr );
	va_end( argptr );

	// don't multiple executes
	if( recursive )
	{
		// echo to system console and log
		Sys_Print( va("Sys_RecursiveError: %s\n", errorstring ));
		return;
	}	

	recursive = true;

	// prepare host to close
	com.sprintf( host.finalmsg, "Server fatal crashed: %s\n", errorstring );
	host.state = HOST_ERROR;	// lock shutdown state
	Host_FreeRender();		// close render to show message error

	com.error("%s", errorstring );
}

/*
=======================================================================

		    VIRTUAL MACHINE COMMON UTILS
=======================================================================
*/
bool VM_ValidateArgs( const char *builtin, int num_argc )
{
	if( prog->argc < num_argc )
	{
		MsgDev( D_ERROR, "%s called with too few parameters\n", builtin );
		return false;
	}
	else if( prog->argc < num_argc )
	{
		MsgDev( D_ERROR, "%s called with too many parameters\n", builtin );
		return false;
	}
	return true;
}

void VM_ValidateString( const char *s )
{
	if( s[0] <= ' ' ) PRVM_ERROR( "%s: bad string\n", PRVM_NAME );
}


/*
=========
VM_VarArgs

supports follow prefixes:
%d - integer or bool (declared as float)
%i - integer or bool (declared as float)
%f - float
%g - formatted float with cutoff zeroes
%s - string
%p - function pointer (will be printed function name)
%e - entity (will be print entity number) - just in case
%v - vector (format: %g %g %g)
=========
*/
const char *VM_VarArgs( int start_arg )
{
	static char	vm_string[MAX_STRING_CHARS];
	int		arg = start_arg + 1;// skip format string	
	char		*out, *outend;
	static string	vm_arg;
	const char	*s, *m;
	mfunction_t	*func;
	float		*vec;

	// get the format string
	s = PRVM_G_STRING((OFS_PARM0 + start_arg * 3));
	out = vm_string;
	outend = out + MAX_STRING_CHARS - 1;
	memset( vm_string, 0, MAX_STRING_CHARS - 1 );

	while( out < outend && *s )
	{
		if( arg > prog->argc ) break;	// simple boundschecker
		if( *s != '%' )
		{
			*out++ = *s++;	// copy symbols
			continue;    	// wait for percents
		}

		switch((int)s[1])
		{
		case 'd': com.snprintf( vm_arg, MAX_STRING, "%d", (int)PRVM_G_FLOAT(OFS_PARM0+arg*3)); break;
		case 'i': com.snprintf( vm_arg, MAX_STRING, "%i", (int)PRVM_G_FLOAT(OFS_PARM0+arg*3)); break;
		case 's': com.snprintf( vm_arg, MAX_STRING, "%s", PRVM_G_STRING(OFS_PARM0+arg*3)); break;
		case 'f': com.snprintf( vm_arg, MAX_STRING, "%f", PRVM_G_FLOAT(OFS_PARM0+arg*3)); break;
		case 'g': com.snprintf( vm_arg, MAX_STRING, "%g", PRVM_G_FLOAT(OFS_PARM0+arg*3)); break;
		case 'e': com.snprintf( vm_arg, MAX_STRING, "%i", PRVM_G_EDICTNUM(OFS_PARM0+arg*3)); break;
		case 'p': // function ptr
			func = prog->functions + PRVM_G_INT(OFS_PARM0+arg*3);
			if(!func->s_name) com.strncpy( vm_arg, "(null)", MAX_STRING ); // MSVCRT style
			else com.snprintf( vm_arg, MAX_STRING, "%s", PRVM_GetString(func->s_name));
			break;
		case 'v': // vector
			vec = PRVM_G_VECTOR((OFS_PARM0+arg*3));
			com.snprintf( vm_arg, MAX_STRING, "%g %g %g", vec[0], vec[1], vec[2] );
			break;
		default:
			arg++; 		// skip invalid arg
			continue;
		}

		s += 2;
		m = vm_arg, arg++;
		while( out < outend && *m )
			*out++ = *m++;	// copy next arg
	}
	return vm_string;
}

vfile_t *VM_GetFileHandle( int index )
{
	if( index < 0 || index >= PRVM_MAX_OPENFILES )
	{
		Msg("VM_GetFileHandle: invalid file handle %i\n", index );
		return NULL;
	}
	if( prog->file[index] == NULL )
	{
		Msg("VM_GetFileHandle: no such file handle %i (or file has been closed)\n", index );
		return NULL;
	}
	return prog->file[index];
}

void VM_Files_Init( void )
{
	int	i;
	for( i = 0; i < PRVM_MAX_OPENFILES; i++ )
		prog->file[i] = NULL;
}

void VM_Files_CloseAll( void )
{
	int	i;

	for( i = 0; i < PRVM_MAX_OPENFILES; i++ )
	{
		if( prog->file[i] )
			FS_Close(VFS_Close(prog->file[i]));
		prog->file[i] = NULL;
	}
}

void VM_Cmd_Init( void )
{
	// only init the stuff for the current prog
	VM_Files_Init();
}

void VM_Cmd_Reset( void )
{
	VM_Files_CloseAll();
}


/*
=======================================================================

		    VIRTUAL MACHINE GENERIC API
=======================================================================
*/
/*
=========
VM_ConPrintf

void Con_Printf( ... )
=========
*/
void VM_ConPrintf( void )
{
	com.print(VM_VarArgs( 0 ));
}

/*
=========
VM_ConDPrintf

void Con_DPrintf( float level, ... )
=========
*/
void VM_ConDPrintf( void )
{
	if(host.developer < (int)PRVM_G_FLOAT(OFS_PARM0))
		return;
	com.print(VM_VarArgs( 1 ));
}

/*
=========
VM_HostError

void Com_Error( ... )
=========
*/
void VM_HostError( void )
{
	edict_t		*ed;
	const char	*s = VM_VarArgs( 0 );

	Msg( "====== %s ERROR in %s:\n%s\n", PRVM_NAME, PRVM_GetString(prog->xfunction->s_name), s );
	if( prog->pev )
	{
		ed = PRVM_G_EDICT( prog->pev->ofs );
		PRVM_ED_Print( ed );
	}
	Host_Error("%s: Program error in function %s:\n%s\n", PRVM_NAME, PRVM_GetString(prog->xfunction->s_name), s );
}

/*
=================
VM_EdictError

void Ed_Error( ... )
=================
*/
void VM_EdictError( void )
{
	edict_t		*ed;
	const char	*s = VM_VarArgs( 0 );

	Msg( "======OBJECT ERROR======\n", PRVM_NAME, PRVM_GetString(prog->xfunction->s_name), s );
	if( prog->pev )
	{
		ed = PRVM_G_EDICT( prog->pev->ofs );
		PRVM_ED_Print( ed );
		PRVM_ED_Free( ed );
	}
	else PRVM_ERROR ("VM_objecterror: pev not defined !");
	Msg("%s OBJECT ERROR in %s:\n%s\n", PRVM_NAME, PRVM_GetString(prog->xfunction->s_name), s );
}

/*
=========
VM_SysExit

void Sys_Exit( void )
=========
*/
void VM_SysExit( void )
{
	if(!VM_ValidateArgs( "Sys_Exit", 0 ))
		return;

	// using only for debugging :)
	if( host.developer ) com.exit();
}

/*
=========
VM_CmdArgv

string Cmd_Argv( float arg )
=========
*/
void VM_CmdArgv( void )
{
	int	arg;

	if(!VM_ValidateArgs( "Cmd_Argv", 1 ))
		return;

	arg = (int)PRVM_G_FLOAT(OFS_PARM0);
	if( arg >= 0 && arg < Cmd_Argc())
		PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(Cmd_Argv( arg ));
	else PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString( NULL );
}

/*
=========
VM_CmdArgc

float Cmd_Argc( void )
=========
*/
void VM_CmdArgc( void )
{
	if(!VM_ValidateArgs( "Cmd_Argc", 0 ))
		return;

	PRVM_G_FLOAT(OFS_RETURN) = Cmd_Argc();
}

/*
=========
VM_ComTrace

void Com_Trace( float enable )
=========
*/
void VM_ComTrace( void )
{
	if(!VM_ValidateArgs( "Com_Trace", 1 ))
		return;

	if(PRVM_G_FLOAT(OFS_PARM0))
		prog->trace = true;
	else prog->trace = false;
}

/*
=========
VM_ComFileExists

float Com_FileExists( string filename )
=========
*/
void VM_ComFileExists( void )
{
	if(!VM_ValidateArgs( "Com_FileExists", 1 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0)); 
	PRVM_G_FLOAT(OFS_RETURN) = FS_FileExists(PRVM_G_STRING(OFS_PARM0)) ? 1.0f : 0.0f; 
}

/*
=========
VM_ComFileSize

float Com_FileSize( string filename )
=========
*/
void VM_ComFileSize( void )
{
	if(!VM_ValidateArgs( "Com_FileSize", 1 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0)); 
	PRVM_G_FLOAT(OFS_RETURN) = (float)FS_FileSize(PRVM_G_STRING(OFS_PARM0)); 
}

/*
=========
VM_ComFileTime

float Com_FileTime( string filename )
=========
*/
void VM_ComFileTime( void )
{
	if(!VM_ValidateArgs( "Com_FileTime", 1 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	PRVM_G_FLOAT(OFS_RETURN) = (float)FS_FileTime(PRVM_G_STRING(OFS_PARM0)); 
}

/*
=========
VM_ComLoadScript

float Com_LoadScript( string filename )
=========
*/
void VM_ComLoadScript( void )
{
	static byte	*script;
	static size_t	length;

	if(!VM_ValidateArgs( "Com_LoadScript", 1 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0)); 

	// place script into memory
	if( script && length ) Mem_Free( script );
	script = FS_LoadFile( PRVM_G_STRING(OFS_PARM0), &length );	

	PRVM_G_FLOAT(OFS_RETURN) = (float)Com_LoadScript( "VM script", script, length );
}

/*
=========
VM_ComResetScript

void Com_ResetScript( void )
=========
*/
void VM_ComResetScript( void )
{
	if(!VM_ValidateArgs( "Com_ResetScript", 0 ))
		return;

	Com_ResetScript();
}

/*
=========
VM_ComReadToken

string Com_ReadToken( float newline )
=========
*/
void VM_ComReadToken( void )
{
	if(!VM_ValidateArgs( "Com_ReadToken", 1 ))
		return;

	// using slow, but safe parsing method
	if( PRVM_G_FLOAT(OFS_PARM0) )
	{
		Com_GetToken( true ); // newline token is always safe
		PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString( com_token );
	}
	else
	{
		if(Com_TryToken()) // token available
			PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString( com_token );
		else PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString( NULL );
	}
}

/*
=========
VM_ComFilterToken

float Com_Filter( string mask, string s, float casecmp )
=========
*/
void VM_ComFilterToken( void )
{
	const char *mask, *name;
	bool	casecmp;

	if(!VM_ValidateArgs( "Com_Filter", 3 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	VM_ValidateString(PRVM_G_STRING(OFS_PARM1));

	mask = PRVM_G_STRING(OFS_PARM0);
	name = PRVM_G_STRING(OFS_PARM1);
	casecmp = PRVM_G_FLOAT(OFS_PARM2) ? true : false;

	PRVM_G_FLOAT(OFS_RETURN) = (float)Com_Filter( (char *)mask, (char *)name, casecmp );
}

/*
=========
VM_ComSearchFiles

float Com_Search( string mask, float casecmp )
=========
*/
void VM_ComSearchFiles( void )
{
	const char *mask;
	bool	casecmp;
	int	numfiles = 0;
	
	if(!VM_ValidateArgs( "Com_Search", 2 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));

	// release previous search
	if( prog->search ) Mem_Free( prog->search );

	mask = PRVM_G_STRING(OFS_PARM0);
	casecmp = PRVM_G_FLOAT(OFS_PARM1) ? true : false;

	prog->search = FS_Search( mask, casecmp );

	if( prog->search ) numfiles = prog->search->numfilenames;
	PRVM_G_FLOAT(OFS_RETURN) = (float)numfiles;
}

/*
=========
VM_ComSearchNames

string Com_SearchFilename( float num )
=========
*/
void VM_ComSearchNames( void )
{
	int	num;

	if(!VM_ValidateArgs( "Com_SearchFilename", 1 ))
		return;

	num = PRVM_G_FLOAT(OFS_PARM0);

	if( prog->search && prog->search->numfilenames > num ) // in-range
		PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString( prog->search->filenames[num] );
	else PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString( NULL );
}

/*
=========
VM_RandomLong

float RandomLong( float min, float max )
=========
*/
void VM_RandomLong( void )
{
	if(!VM_ValidateArgs( "RandomLong", 2 ))
		return;

	PRVM_G_FLOAT(OFS_RETURN) = Com_RandomLong(PRVM_G_FLOAT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1));
}

/*
=========
VM_RandomFloat

float RandomFloat( float min, float max )
=========
*/
void VM_RandomFloat( void )
{
	if(!VM_ValidateArgs( "RandomFloat", 2 ))
		return;

	PRVM_G_FLOAT(OFS_RETURN) = Com_RandomFloat(PRVM_G_FLOAT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1));
}

/*
=========
VM_RandomVector

float RandomVector( vector min, vector max )
=========
*/
void VM_RandomVector( void )
{
	vec3_t	temp;
	float	*min, *max;

	if(!VM_ValidateArgs( "RandomVector", 2 ))
		return;

	min = PRVM_G_VECTOR(OFS_PARM0);
	max = PRVM_G_VECTOR(OFS_PARM1);

	temp[0] = RANDOM_FLOAT(min[0], max[0]);
	temp[1] = RANDOM_FLOAT(min[1], max[1]);
	temp[2] = RANDOM_FLOAT(min[2], max[2]);

	VectorNormalize2( temp, PRVM_G_VECTOR(OFS_RETURN));
}

/*
=========
VM_CvarRegister

void Cvar_Register( string name, string value, float flags )
=========
*/
void VM_CvarRegister( void )
{
	const char *name, *value;
	int	flags = 0;

	if(!VM_ValidateArgs( "Cvar_Register", 3 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	VM_ValidateString(PRVM_G_STRING(OFS_PARM1));

	name = PRVM_G_STRING(OFS_PARM0);
	value = PRVM_G_STRING(OFS_PARM1);

	if((int)PRVM_G_FLOAT(OFS_PARM2) & 1)
		flags |= CVAR_ARCHIVE;

	if((int)PRVM_G_FLOAT(OFS_PARM2) & 2)
		flags |= CVAR_CHEAT;

	// register new cvar	
	Cvar_Get( name, value, flags, va("%s variable", prog->name ));
}

/*
=========
VM_CvarSetValue

void Cvar_SetValue( string name, float value )
=========
*/
void VM_CvarSetValue( void )
{
	const char	*name;
	float		value;

	if(!VM_ValidateArgs( "Cvar_SetValue", 2 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	name = PRVM_G_STRING(OFS_PARM0);
	value = PRVM_G_FLOAT(OFS_PARM1);

	// set new value
	Cvar_SetValue( name, value );
}

/*
=========
VM_CvarSetString

void Cvar_SetString( string name, string value )
=========
*/
void VM_CvarSetString( void )
{
	const char	*name, *value;

	if(!VM_ValidateArgs( "Cvar_SetString", 2 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	VM_ValidateString(PRVM_G_STRING(OFS_PARM1));
	name = PRVM_G_STRING(OFS_PARM0);
	value = PRVM_G_STRING(OFS_PARM1);

	// set new value
	Cvar_Set( name, value );
}

/*
=========
VM_CvarGetValue

float Cvar_GetValue( string name )
=========
*/
void VM_CvarGetValue( void )
{
	const char *name;

	if(!VM_ValidateArgs( "Cvar_GetValue", 1 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	name = PRVM_G_STRING(OFS_PARM0);
	PRVM_G_FLOAT(OFS_RETURN) = Cvar_VariableValue( name );

}

/*
=========
VM_CvarGetString

string Cvar_GetString( string name )
=========
*/
void VM_CvarGetString( void )
{
	const char *name;

	if(!VM_ValidateArgs( "Cvar_GetString", 1 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	name = PRVM_G_STRING(OFS_PARM0);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(Cvar_VariableString( name ));

}

/*
=================
VM_LocalCmd

cmd( string, ... )
=================
*/
void VM_LocalCmd( void )
{
	Cbuf_AddText(VM_VarArgs( 0 ));
}

/*
=========
VM_ComVA

string va( ... )
=========
*/
void VM_ComVA( void )
{
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(VM_VarArgs( 0 ));
	
}

/*
=========
VM_ComStrlen

float strlen( string text )
=========
*/
void VM_ComStrlen( void )
{
	if(!VM_ValidateArgs( "Com_Strlen", 1 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = com.cstrlen(PRVM_G_STRING(OFS_PARM0));
}

/*
=========
VM_TimeStamp

string Com_TimeStamp( float format )
=========
*/
void VM_TimeStamp( void )
{
	if(!VM_ValidateArgs( "Com_TimeStamp", 1 ))
		return;

	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(timestamp((int)PRVM_G_FLOAT(OFS_PARM0)));
}

/*
=========
VM_SubString

string substring( string s, float start, float length )
=========
*/
void VM_SubString( void )
{
	int		i, start, length;
	string		tempstring;
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

	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString( tempstring );
}

/*
=========
VM_localsound

void localsound( string sample )
=========
*/
void VM_localsound( void )
{
	const char *s;

	if(!VM_ValidateArgs( "localsound", 1 ))
		return;

	s = PRVM_G_STRING( OFS_PARM0 );

	if(!S_StartLocalSound(s))
	{
		VM_Warning("localsound: can't play %s!\n", s );
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		return;
	}
	PRVM_G_FLOAT(OFS_RETURN) = 1;
}

/*
=========
VM_SpawnEdict

entity spawn( void )
=========
*/
void VM_SpawnEdict( void )
{
	edict_t	*ed;

	prog->xfunction->builtinsprofile += 20;
	ed = PRVM_ED_Alloc();
	VM_RETURN_EDICT( ed );
}

/*
=========
VM_RemoveEdict

void remove( entity ent )
=========
*/
void VM_RemoveEdict( void )
{
	edict_t	*ed;

	if(!VM_ValidateArgs( "remove", 1 ))
		return;

	prog->xfunction->builtinsprofile += 20;
	ed = PRVM_G_EDICT(OFS_PARM0);
	if( PRVM_NUM_FOR_EDICT(ed) <= prog->reserved_edicts )
	{
		if( host.developer >= D_INFO )
			VM_Warning( "VM_RemoveEdict: tried to remove the null entity or a reserved entity!\n" );
	}
	else if( ed->priv.ed->free )
	{
		if( host.developer >= D_INFO )
			VM_Warning( "VM_RemoveEdict: tried to remove an already freed entity!\n" );
	}
	else PRVM_ED_Free( ed );
}

/*
=========
VM_NextEdict

void nextent( entity ent )
=========
*/
void VM_NextEdict( void )
{
	edict_t	*ent;
	int	i;

	if(!VM_ValidateArgs( "nextent", 1 ))
		return;

	i = PRVM_G_EDICTNUM( OFS_PARM0 );
	while( 1 )
	{
		prog->xfunction->builtinsprofile++;
		i++;
		if( i == prog->num_edicts )
		{
			VM_RETURN_EDICT( prog->edicts );
			return;
		}
		ent = PRVM_EDICT_NUM(i);
		if(!ent->priv.ed->free)
		{
			VM_RETURN_EDICT( ent );
			return;
		}
	}
}

/*
=================
VM_copyentity

void copyentity( entity src, entity dst )
=================
*/
void VM_CopyEdict( void )
{
	edict_t *in, *out;

	if(!VM_ValidateArgs( "copyentity", 1 ))
		return;

	in = PRVM_G_EDICT(OFS_PARM0);
	out = PRVM_G_EDICT(OFS_PARM1);
	Mem_Copy( out->progs.vp, in->progs.vp, prog->progs->entityfields * 4 );
}

/*
=========
VM_FindEdict

entity find( entity start, .string field, string match )
=========
*/
void VM_FindEdict( void )
{
	int		e, f;
	const char	*s, *t;
	edict_t		*ed;

	if(!VM_ValidateArgs( "find", 2 ))
		return;

	e = PRVM_G_EDICTNUM(OFS_PARM0);
	f = PRVM_G_INT(OFS_PARM1);
	s = PRVM_G_STRING(OFS_PARM2);
	if(!s) s = "";

	for( e++; e < prog->num_edicts; e++ )
	{
		prog->xfunction->builtinsprofile++;
		ed = PRVM_EDICT_NUM(e);
		if( ed->priv.ed->free )continue;
		t = PRVM_GetString(*(string_t *)&((float*)ed->progs.vp)[f]);
		if(!t) t = "";
		if(!com.strcmp( t, s ))
		{
			VM_RETURN_EDICT( ed );
			return;
		}
	}
	VM_RETURN_EDICT( prog->edicts );
}

/*
=========
VM_FindField

entity findfloat(entity start, .float field, float match)
=========
*/
void VM_FindField( void )
{
	int	e, f;
	float	s;
	edict_t	*ed;

	if(!VM_ValidateArgs( "findfloat", 2 ))
		return;

	e = PRVM_G_EDICTNUM(OFS_PARM0);
	f = PRVM_G_INT(OFS_PARM1);
	s = PRVM_G_FLOAT(OFS_PARM2);

	for( e++; e < prog->num_edicts; e++ )
	{
		prog->xfunction->builtinsprofile++;
		ed = PRVM_EDICT_NUM(e);
		if( ed->priv.ed->free ) continue;
		if(((float*)ed->progs.vp)[f] == s )
		{
			VM_RETURN_EDICT( ed );
			return;
		}
	}
	VM_RETURN_EDICT( prog->edicts );
}

/*
=======================================================================

		    VIRTUAL MACHINE FILESYSTEM API
=======================================================================
*/
/*
=========
VM_FS_Open

float fopen( string filename, float mode )
=========
*/
void VM_FS_Open( void )
{
	int		filenum, mode;
	const char	*modestring, *filename;

	if(!VM_ValidateArgs( "fopen", 2 )) return;
	for( filenum = 0; filenum < PRVM_MAX_OPENFILES; filenum++ )
		if( prog->file[filenum] == NULL ) break;

	if( filenum >= PRVM_MAX_OPENFILES )
	{
		PRVM_G_FLOAT(OFS_RETURN) = -2;
		VM_Warning( "fopen: %s file handles limit exceeded( max %i )\n", PRVM_NAME, PRVM_MAX_OPENFILES );
		return;
	}

	mode = (int)PRVM_G_FLOAT( OFS_PARM1 );
	switch( mode )
	{
	case 0: modestring = "rb"; break; // FILE_READ
	case 1: modestring = "ab"; break; // FILE_APPEND
	case 2: modestring = "wb"; break; // FILE_WRITE
	default:
		PRVM_G_FLOAT(OFS_RETURN) = -3;
		VM_Warning( "fopen: no such mode %i\n", mode );
		return;
	}
	filename = PRVM_G_STRING(OFS_PARM0);
	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));

	prog->file[filenum] = VFS_Open(FS_Open( filename, modestring ), modestring );

	if (prog->file[filenum] == NULL)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -1;
		VM_Warning("fopen: can't open %s [%s]\n", filename, modestring );
	}
	else PRVM_G_FLOAT(OFS_RETURN) = filenum;
}

/*
=========
VM_FS_Close

float fclose( float handle )
=========
*/
void VM_FS_Close( void )
{
	vfile_t	*handle;

	if(!VM_ValidateArgs( "fclose", 2 )) return;
	handle = VM_GetFileHandle((int)PRVM_G_FLOAT( OFS_PARM0 ));
	if(!handle) return;

	FS_Close(VFS_Close( handle ));
}

/*
=========
VM_FS_Gets

string fgets( float handle )
=========
*/
void VM_FS_Gets( void )
{
	static char	string[MAX_INPUTLINE];
	vfile_t		*handle;
	int		c;

	if(!VM_ValidateArgs( "fgets", 1 )) return;
	handle = VM_GetFileHandle((int)PRVM_G_FLOAT( OFS_PARM0 ));
	if(!handle) return;

	c = VFS_Gets( handle, string, MAX_INPUTLINE );
	if( c >= 0 ) PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString( string );
	else PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString( NULL );
}

/*
=========
VM_FS_Puts

string fputs( float handle )
=========
*/
void VM_FS_Puts( void )
{
	vfile_t		*handle;
	const char	*s;

	if(!VM_ValidateArgs( "fputs", 1 )) return;
	handle = VM_GetFileHandle((int)PRVM_G_FLOAT( OFS_PARM0 ));
	if(!handle) return;

	s = VM_VarArgs( 1 );
	VFS_Print( handle, s );
}

/*
=======================================================================

		    PICTURE & MODEL DRAWING STUFF
=======================================================================
*/
/*
=========
VM_precache_pic

float precache_pic( string pic )
=========
*/
void VM_precache_pic( void )
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
VM_drawcharacter

float drawchar( vector pos, float char, vector scale, vector rgb, float alpha )
=========
*/
void VM_drawcharacter( void )
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
VM_drawstring

float drawstring( vector pos, string text, vector scale, vector rgb, float alpha )
=========
*/
void VM_drawstring( void )
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
VM_drawpic

float drawpic( vector pos, string pic, vector size, vector rgb, float alpha )
=========
*/
void VM_drawpic( void )
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
VM_drawfill

void drawfill( vector pos, vector size, vector rgb, float alpha )
=========
*/
void VM_drawfill( void )
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
VM_drawmodel

void drawmodel( vector pos, vector size, string model, vector origin, vector angles, float sequence )
=========
*/
void VM_drawmodel( void )
{
	float		*size, *pos, *origin, *angles;
	const char	*modname;
	refdef_t		refdef;
	int		sequence;
	static float	frame;
	entity_t		entity;

	if(!VM_ValidateArgs( "drawmodel", 4 ))
		return;

	pos = PRVM_G_VECTOR(OFS_PARM0);
	size = PRVM_G_VECTOR(OFS_PARM1);
	modname = PRVM_G_STRING(OFS_PARM2);
	origin = PRVM_G_VECTOR(OFS_PARM3);
	angles = PRVM_G_VECTOR(OFS_PARM4);
	sequence = (int)PRVM_G_FLOAT(OFS_PARM5);

	VM_ValidateString(PRVM_G_STRING(OFS_PARM2));
	memset( &entity, 0, sizeof( entity ));
	memset( &refdef, 0, sizeof( refdef ) );

	SCR_AdjustSize( &pos[0], &pos[1], &size[0], &size[1] );
	
	refdef.x = pos[0];
	refdef.y = pos[1];
	refdef.width = size[0];
	refdef.height = size[1];
	refdef.fov_x = 50;
	refdef.fov_y = V_CalcFov( refdef.fov_x, refdef.width, refdef.height );
	refdef.time = cls.realtime * 0.001f;

	entity.model = re->RegisterModel( (char *)modname );
	entity.flags = RF_FULLBRIGHT;
	VectorCopy( origin, entity.origin );
	VectorCopy( entity.origin, entity.oldorigin );
	VectorCopy( angles, entity.angles );
	entity.frame = frame += 0.7f;//FXIME
	entity.sequence = sequence;
	entity.prev.frame = 0;
	entity.backlerp = 0.0;
	entity.controller[0] = 90.0;
	entity.controller[1] = 90.0;
	entity.controller[2] = 180.0;
	entity.controller[3] = 180.0;

	refdef.areabits = 0;
	refdef.num_entities = 1;
	refdef.entities = &entity;
	refdef.lightstyles = 0;
	refdef.rdflags = RDF_NOWORLDMODEL;

	re->RenderFrame( &refdef );
	re->EndFrame();
}

/*
=========
VM_getimagesize

vector getimagesize( string pic )
=========
*/
void VM_getimagesize( void )
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
=======================================================================

		    VIRTUAL MACHINE MATHLIB
=======================================================================
*/
/*
=================
VM_min

float min( float a, float b )
=================
*/
void VM_min( void )
{
	if(!VM_ValidateArgs( "min", 2 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = min(PRVM_G_FLOAT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1));
}

/*
=================
VM_max

float max( float a, float b )
=================
*/
void VM_max( void )
{
	if(!VM_ValidateArgs( "max", 2 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = max(PRVM_G_FLOAT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1));
}

/*
=================
VM_bound

float bound( float min, float value, float max )
=================
*/
void VM_bound( void )
{
	if(!VM_ValidateArgs( "bound", 3 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = bound(PRVM_G_FLOAT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1), PRVM_G_FLOAT(OFS_PARM2));
}

/*
=================
VM_pow

float pow( float a, float b )
=================
*/
void VM_pow( void )
{
	if(!VM_ValidateArgs( "pow", 2 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = pow(PRVM_G_FLOAT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1));
}

/*
=========
VM_sin

float sin( float f )
=========
*/
void VM_sin( void )
{
	if(!VM_ValidateArgs( "sin", 1 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = sin(PRVM_G_FLOAT(OFS_PARM0));
}

/*
=========
VM_cos

float cos( float f )
=========
*/
void VM_cos( void )
{
	if(!VM_ValidateArgs( "cos", 1 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = cos(PRVM_G_FLOAT(OFS_PARM0));
}

/*
=========
VM_tan

float tan( float f )
=========
*/
void VM_tan( void )
{
	if(!VM_ValidateArgs( "tan", 1 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = tan(PRVM_G_FLOAT(OFS_PARM0));
}

/*
=========
VM_asin

float asin( float f )
=========
*/
void VM_asin( void )
{
	if(!VM_ValidateArgs( "asin", 1 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = asin(PRVM_G_FLOAT(OFS_PARM0));
}

/*
=========
VM_acos

float acos( float f )
=========
*/
void VM_acos( void )
{
	if(!VM_ValidateArgs( "acos", 1 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = acos(PRVM_G_FLOAT(OFS_PARM0));
}

/*
=========
VM_atan

float atan( float f )
=========
*/
void VM_atan( void )
{
	if(!VM_ValidateArgs( "atan", 1 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = atan(PRVM_G_FLOAT(OFS_PARM0));
}

/*
=========
VM_sqrt

float sqrt( float f )
=========
*/
void VM_sqrt( void )
{
	if(!VM_ValidateArgs( "sqrt", 1 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = sqrt(PRVM_G_FLOAT(OFS_PARM0));
}

/*
=========
VM_rint

float rint( float f )
=========
*/
void VM_rint( void )
{
	float	f;

	if(!VM_ValidateArgs( "rint", 1 ))
		return;

	f = PRVM_G_FLOAT(OFS_PARM0);
	if( f > 0 ) PRVM_G_FLOAT(OFS_RETURN) = floor(f + 0.5);
	else PRVM_G_FLOAT(OFS_RETURN) = ceil(f - 0.5);
}

/*
=========
VM_floor

float floor( float f )
=========
*/
void VM_floor( void )
{
	if(!VM_ValidateArgs( "floor", 1 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = floor(PRVM_G_FLOAT(OFS_PARM0));
}

/*
=========
VM_ceil

float ceil( float f )
=========
*/
void VM_ceil( void )
{
	if(!VM_ValidateArgs( "ceil", 1 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = ceil(PRVM_G_FLOAT(OFS_PARM0));
}

/*
=========
VM_fabs

float fabs( float f )
=========
*/
void VM_fabs( void )
{
	if(!VM_ValidateArgs( "fabs", 1 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = fabs(PRVM_G_FLOAT(OFS_PARM0));
}

/*
=========
VM_modulo

float mod( float val, float m )
=========
*/
void VM_mod( void )
{
	int val, m;

	if(!VM_ValidateArgs( "fmod", 2 )) return;

	val = (int)PRVM_G_FLOAT(OFS_PARM0);
	m = (int)PRVM_G_FLOAT(OFS_PARM1);
	PRVM_G_FLOAT(OFS_RETURN) = (float)(val % m);
}

/*
=========
VM_VectorNormalize

vector VectorNormalize( vector v )
=========
*/
void VM_VectorNormalize( void )
{
	if(!VM_ValidateArgs( "normalize", 1 )) return;
	VectorNormalize2( PRVM_G_VECTOR(OFS_PARM0), PRVM_G_VECTOR(OFS_RETURN));
}

/*
=========
VM_VectorLength

float VectorLength( vector v )
=========
*/
void VM_VectorLength( void )
{
	if(!VM_ValidateArgs( "veclength", 1 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = VectorLength(PRVM_G_VECTOR(OFS_PARM0));
}

prvm_builtin_t std_builtins[] = 
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
e10, e10				// #81 - #100 are reserved for future expansions
};

/*
=======================================================================

			INFOSTRING STUFF
=======================================================================
*/

static char sv_info[MAX_INFO_STRING];

/*
===============
Info_Print

printing current key-value pair
===============
*/
void Info_Print( char *s )
{
	char	key[512];
	char	value[512];
	char	*o;
	int	l;

	if( *s == '\\' )s++;

	while (*s)
	{
		o = key;
		while (*s && *s != '\\') *o++ = *s++;

		l = o - key;
		if (l < 20)
		{
			memset (o, ' ', 20-l);
			key[20] = 0;
		}
		else *o = 0;
		Msg ("%s", key);

		if (!*s)
		{
			Msg ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\') *o++ = *s++;
		*o = 0;

		if (*s) s++;
		Msg ("%s\n", value);
	}
}

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
char *Info_ValueForKey( char *s, char *key )
{
	char	pkey[512];
	static	char value[2][512];	// use two buffers so compares work without stomping on each other
	static	int valueindex;
	char	*o;
	
	valueindex ^= 1;
	if( *s == '\\' ) s++;
	while( 1 )
	{
		o = pkey;
		while( *s != '\\' && *s != '\n' )
		{
			if(!*s) return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while( *s != '\\' && *s != '\n' && *s)
		{
			if (!*s) return "";
			*o++ = *s++;
		}
		*o = 0;

		if(!com.strcmp( key, pkey )) return value[valueindex];
		if(!*s) return "";
		s++;
	}
}

void Info_RemoveKey (char *s, char *key)
{
	char	*start;
	char	pkey[512];
	char	value[512];
	char	*o;

	if (strstr (key, "\\")) return;

	while (1)
	{
		start = s;
		if (*s == '\\') s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s) return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s) return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
		{
			strcpy (start, s);	// remove this part
			return;
		}
		if (!*s) return;
	}
}

/*
==================
Info_Validate

Some characters are illegal in info strings because they
can mess up the server's parsing
==================
*/
bool Info_Validate (char *s)
{
	if (strstr (s, "\"")) return false;
	if (strstr (s, ";")) return false;
	return true;
}

void Info_SetValueForKey (char *s, char *key, char *value)
{
	char	newi[MAX_INFO_STRING], *v;
	int	c, maxsize = MAX_INFO_STRING;

	if (strstr (key, "\\") || strstr (value, "\\") )
	{
		Msg ("Can't use keys or values with a \\\n");
		return;
	}

	if (strstr (key, ";") )
	{
		Msg ("Can't use keys or values with a semicolon\n");
		return;
	}
	if (strstr (key, "\"") || strstr (value, "\"") )
	{
		Msg ("Can't use keys or values with a \"\n");
		return;
	}
	if (strlen(key) > MAX_INFO_KEY - 1 || strlen(value) > MAX_INFO_KEY-1)
	{
		Msg ("Keys and values must be < 64 characters.\n");
		return;
	}

	Info_RemoveKey (s, key);
	if (!value || !com.strlen(value)) return;
	com.sprintf (newi, "\\%s\\%s", key, value);

	if (com.strlen(newi) + com.strlen(s) > maxsize)
	{
		Msg ("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += com.strlen(s);
	v = newi;
	while (*v)
	{
		c = *v++;
		c &= 127;	// strip high bits
		if (c >= 32 && c < 127) *s++ = c;
	}
	*s = 0;
}

static void Cvar_LookupBitInfo(const char *name, const char *string, const char *info, void *unused)
{
	Info_SetValueForKey((char *)info, (char *)name, (char *)string);
}

char *Cvar_Userinfo (void)
{
	sv_info[0] = 0; // clear previous calls
	Cvar_LookupVars( CVAR_USERINFO, sv_info, NULL, Cvar_LookupBitInfo ); 
	return sv_info;
}

char *Cvar_Serverinfo (void)
{
	sv_info[0] = 0; // clear previous calls
	Cvar_LookupVars( CVAR_SERVERINFO, sv_info, NULL, Cvar_LookupBitInfo ); 
	return sv_info;
}

/*
=======================================================================

			FILENAME AUTOCOMPLETION
=======================================================================
*/
/*
=====================================
Cmd_GetMapList

Prints or complete map filename
=====================================
*/
bool Cmd_GetMapList( const char *s, char *completedname, int length )
{
	search_t		*t;
	file_t		*f;
	string		message;
	string		matchbuf;
	byte		buf[MAX_SYSPATH]; // 1 kb
	int		i, nummaps;

	t = FS_Search(va("maps/%s*.bsp", s), true );
	if( !t ) return false;

	FS_FileBase(t->filenames[0], matchbuf ); 
	strncpy( completedname, matchbuf, length );
	if(t->numfilenames == 1) return true;

	for(i = 0, nummaps = 0; i < t->numfilenames; i++)
	{
		const char	*data = NULL;
		char		*entities = NULL;
		char		entfilename[MAX_QPATH];
		int		ver = -1, lumpofs = 0, lumplen = 0;
		const char	*ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp(ext, "bsp" )) continue;

		strncpy(message, "^1error^7", sizeof(message));
		f = FS_Open(t->filenames[i], "rb" );
	
		if( f )
		{
			memset(buf, 0, 1024);
			FS_Read(f, buf, 1024);
			if(!memcmp(buf, "IBSP", 4))
			{
				dheader_t *header = (dheader_t *)buf;
				ver = LittleLong(((int *)buf)[1]);

				switch(ver)
				{
				case 38:	// quake2
				case 39:	// xash3d
				case 46:	// quake3
				case 47:	// return to castle wolfenstein
					lumpofs = LittleLong(header->lumps[LUMP_ENTITIES].fileofs);
					lumplen = LittleLong(header->lumps[LUMP_ENTITIES].filelen);
					break;
				}
			}
			else
			{
				lump_t	ents; // quake1 entity lump
				memcpy(&ents, buf + 4, sizeof(lump_t)); // skip first four bytes (version)
				ver = LittleLong(((int *)buf)[0]);

				switch( ver )
				{
				case 28:	// quake 1 beta
				case 29:	// quake 1 regular
				case 30:	// Half-Life regular
					lumpofs = LittleLong(ents.fileofs);
					lumplen = LittleLong(ents.filelen);
					break;
				default:
					ver = 0;
					break;
				}
			}

			com.strncpy(entfilename, t->filenames[i], sizeof(entfilename));
			FS_StripExtension( entfilename );
			FS_DefaultExtension( entfilename, ".ent" );
			entities = (char *)FS_LoadFile(entfilename, NULL);

			if( !entities && lumplen >= 10 )
			{
				FS_Seek(f, lumpofs, SEEK_SET);
				entities = (char *)Z_Malloc(lumplen + 1);
				FS_Read(f, entities, lumplen);
			}

			if( entities )
			{
				// if there are entities to parse, a missing message key just
				// means there is no title, so clear the message string now
				message[0] = 0;
				data = entities;
				while(Com_ParseToken(&data))
				{
					if(!strcmp(com_token, "{" )) continue;
					else if(!strcmp(com_token, "}" )) break;
					else if(!strcmp(com_token, "message" ))
					{
						// get the message contents
						Com_ParseToken(&data);
						strncpy(message, com_token, sizeof(message));
					}
				}
			}
		}
		if( entities )Mem_Free(entities);
		if( f )FS_Close(f);
		FS_FileBase(t->filenames[i], matchbuf );

		switch(ver)
		{
		case 28:  strncpy((char *)buf, "Quake1 beta", sizeof(buf)); break;
		case 29:  strncpy((char *)buf, "Quake1", sizeof(buf)); break;
		case 30:  strncpy((char *)buf, "Half-Life", sizeof(buf)); break;
		case 38:  strncpy((char *)buf, "Quake 2", sizeof(buf)); break;
		case 39:  strncpy((char *)buf, "Xash 3D", sizeof(buf)); break;
		case 46:  strncpy((char *)buf, "Quake 3", sizeof(buf)); break;
		case 47:  strncpy((char *)buf, "RTCW", sizeof(buf)); break;
		default:	strncpy((char *)buf, "??", sizeof(buf)); break;
		}
		Msg("%16s (%s) ^3%s^7\n", matchbuf, buf, message);
		nummaps++;
	}
	Msg("\n^3 %i maps found.\n", nummaps );
	Mem_Free( t );

	// cut shortestMatch to the amount common with s
	for( i = 0; matchbuf[i]; i++ )
	{
		if(tolower(completedname[i]) != tolower(matchbuf[i]))
			completedname[i] = 0;
	}
	return true;
}

/*
=====================================
Cmd_GetFontList

Prints or complete font filename
=====================================
*/
bool Cmd_GetFontList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numfonts;

	t = FS_Search(va("gfx/fonts/%s*.dds", s ), true);
	if(!t) return false;

	FS_FileBase(t->filenames[0], matchbuf ); 
	if(completedname && length) com.strncpy( completedname, matchbuf, length );
	if(t->numfilenames == 1) return true;

	for(i = 0, numfonts = 0; i < t->numfilenames; i++)
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp(ext, "dds" )) continue;
		FS_FileBase(t->filenames[i], matchbuf );
		Msg("%16s\n", matchbuf );
		numfonts++;
	}
	Msg("\n^3 %i fonts found.\n", numfonts );
	Mem_Free(t);

	// cut shortestMatch to the amount common with s
	if(completedname && length)
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if(com.tolower(completedname[i]) != tolower(matchbuf[i]))
				completedname[i] = 0;
		}
	}

	return true;
}

/*
=====================================
Cmd_GetDemoList

Prints or complete demo filename
=====================================
*/
bool Cmd_GetDemoList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numdems;

	t = FS_Search(va("demos/%s*.dem", s ), true);
	if(!t) return false;

	FS_FileBase(t->filenames[0], matchbuf ); 
	if(completedname && length) strncpy( completedname, matchbuf, length );
	if(t->numfilenames == 1) return true;

	for(i = 0, numdems = 0; i < t->numfilenames; i++)
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp(ext, "dem" )) continue;
		FS_FileBase(t->filenames[i], matchbuf );
		Msg("%16s\n", matchbuf );
		numdems++;
	}
	Msg("\n^3 %i fonts found.\n", numdems );
	Mem_Free(t);

	// cut shortestMatch to the amount common with s
	if(completedname && length)
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if(com.tolower(completedname[i]) != tolower(matchbuf[i]))
				completedname[i] = 0;
		}
	}

	return true;
}

/*
=====================================
Cmd_GetMovieList

Prints or complete movie filename
=====================================
*/
bool Cmd_GetMovieList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, nummovies;

	t = FS_Search(va("video/%s*.roq", s ), true);
	if(!t) return false;

	FS_FileBase(t->filenames[0], matchbuf ); 
	if(completedname && length) strncpy( completedname, matchbuf, length );
	if(t->numfilenames == 1) return true;

	for(i = 0, nummovies = 0; i < t->numfilenames; i++)
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp(ext, "roq" )) continue;
		FS_FileBase(t->filenames[i], matchbuf );
		Msg("%16s\n", matchbuf );
		nummovies++;
	}
	Msg("\n^3 %i movies found.\n", nummovies );
	Mem_Free(t);

	// cut shortestMatch to the amount common with s
	if(completedname && length)
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if(com.tolower(completedname[i]) != tolower(matchbuf[i]))
				completedname[i] = 0;
		}
	}

	return true;
}

/*
=====================================
Cmd_GetMusicList

Prints or complete background track filename
=====================================
*/
bool Cmd_GetMusicList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numtracks;

	t = FS_Search(va("music/%s*.ogg", s ), true);
	if(!t) return false;

	FS_FileBase(t->filenames[0], matchbuf ); 
	if(completedname && length) strncpy( completedname, matchbuf, length );
	if(t->numfilenames == 1) return true;

	for(i = 0, numtracks = 0; i < t->numfilenames; i++)
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp(ext, "ogg" )) continue;
		FS_FileBase(t->filenames[i], matchbuf );
		Msg("%16s\n", matchbuf );
		numtracks++;
	}
	Msg("\n^3 %i soundtracks found.\n", numtracks );
	Mem_Free(t);

	// cut shortestMatch to the amount common with s
	if(completedname && length)
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if(com.tolower(completedname[i]) != tolower(matchbuf[i]))
				completedname[i] = 0;
		}
	}

	return true;
}

/*
=====================================
Cmd_GetSoundList

Prints or complete sound filename
=====================================
*/
bool Cmd_GetSoundList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numsounds;
	const char	*snddir = "sound/"; // constant

	t = FS_Search(va("%s%s*.*", snddir, s ), true);
	if(!t) return false;

	com.strncpy( matchbuf, t->filenames[0] + com.strlen(snddir), MAX_STRING ); 
	FS_StripExtension( matchbuf ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if(t->numfilenames == 1) return true;

	for(i = 0, numsounds = 0; i < t->numfilenames; i++)
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if(com.stricmp(ext, "wav") && com.stricmp(ext, "ogg")) continue;
		com.strncpy( matchbuf, t->filenames[i] + com.strlen(snddir), MAX_STRING ); 
		FS_StripExtension( matchbuf );
		Msg("%16s\n", matchbuf );
		numsounds++;
	}
	Msg("\n^3 %i sounds found.\n", numsounds );
	Mem_Free(t);

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if(com.tolower(completedname[i]) != com.tolower(matchbuf[i]))
				completedname[i] = 0;
		}
	}

	return true;
}

bool Cmd_CheckMapsList( void )
{
	byte	buf[MAX_SYSPATH]; // 1 kb
	char	*buffer, string[MAX_STRING];
	search_t	*t;
	file_t	*f;
	int	i;

	if(FS_FileExists("scripts/maps.lst"))
		return true; // exist 

	t = FS_Search( "maps/*.bsp", false );
	if(!t) return false;

	buffer = Z_Malloc( t->numfilenames * 2 * sizeof(string));
	for( i = 0; i < t->numfilenames; i++ )
	{
		const char	*data = NULL;
		char		*entities = NULL;
		char		entfilename[MAX_QPATH];
		int		ver = -1, lumpofs = 0, lumplen = 0;
		char		mapname[MAX_QPATH], message[MAX_QPATH];

		f = FS_Open(t->filenames[i], "rb");
		FS_FileBase( t->filenames[i], mapname );

		if( f )
		{
			int num_spawnpoints = 0;

			memset(buf, 0, 1024);
			FS_Read(f, buf, 1024);
			if(!memcmp(buf, "IBSP", 4))
			{
				dheader_t *header = (dheader_t *)buf;
				ver = LittleLong(((int *)buf)[1]);
				switch(ver)
				{
				case 38:	// quake2
				case 39:	// xash
				case 46:	// quake3
				case 47:	// return to castle wolfenstein
					lumpofs = LittleLong(header->lumps[LUMP_ENTITIES].fileofs);
					lumplen = LittleLong(header->lumps[LUMP_ENTITIES].filelen);
					break;
				}
			}
			else
			{
				lump_t	ents; // quake1 entity lump
				memcpy(&ents, buf + 4, sizeof(lump_t)); // skip first four bytes (version)
				ver = LittleLong(((int *)buf)[0]);

				switch( ver )
				{
				case 28:	// quake 1 beta
				case 29:	// quake 1 regular
				case 30:	// Half-Life regular
					lumpofs = LittleLong(ents.fileofs);
					lumplen = LittleLong(ents.filelen);
					break;
				default:
					ver = 0;
					break;
				}
			}
			com.strncpy(entfilename, t->filenames[i], sizeof(entfilename));
			FS_StripExtension( entfilename );
			FS_DefaultExtension( entfilename, ".ent" );
			entities = (char *)FS_LoadFile(entfilename, NULL);

			if( !entities && lumplen >= 10 )
			{
				FS_Seek(f, lumpofs, SEEK_SET);
				entities = (char *)Z_Malloc(lumplen + 1);
				FS_Read(f, entities, lumplen);
			}
			if(entities)
			{
				// if there are entities to parse, a missing message key just
				// means there is no title, so clear the message string now
				message[0] = 0;
				data = entities;
				com.strncpy(message, "No Title", MAX_QPATH);

				while(Com_ParseToken(&data))
				{
					if(!strcmp(com_token, "{" )) continue;
					else if(!strcmp(com_token, "}" )) break;
					else if(!strcmp(com_token, "message" ))
					{
						// get the message contents
						Com_ParseToken(&data);
						if(!strcmp(com_token, "" )) continue;
						com.strncpy(message, com_token, sizeof(message));
					}
					else if(!strcmp(com_token, "classname" ))
					{
						Com_ParseToken(&data);
						if(!com.strcmp(com_token, "info_player_deatchmatch"))
							num_spawnpoints++;
						else if(!com.strcmp(com_token, "info_player_start"))
							num_spawnpoints++;
					}
					if(num_spawnpoints > 0) break; // valid map
				}
			}

			if( entities) Mem_Free(entities);
			if( f ) FS_Close(f);

			// format: mapname "maptitle"\n
			com.sprintf(string, "%s \"%s\"\n", mapname, message );
			com.strcat(buffer, string); // add new string
		}
	}
	if( t ) Mem_Free(t); // free search result

	// write generated maps.lst
	if(FS_WriteFile("scripts/maps.lst", buffer, strlen(buffer)))
	{
          	if( buffer ) Mem_Free(buffer);
		return true;
	}
	return false;
}

/*
============
Cmd_WriteVariables

Appends lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/

static void Cmd_WriteCvar(const char *name, const char *string, const char *desc, void *f )
{
	//if(!desc) return; // ignore fantom cvars
	FS_Printf(f, "seta %s \"%s\"\n", name, string );
}

void Cmd_WriteVariables( file_t *f )
{
	FS_Printf( f, "unsetall\n" );
	Cvar_LookupVars( CVAR_ARCHIVE, NULL, f, Cmd_WriteCvar ); 
}

float frand(void)
{
	return (rand()&32767)* (1.0/32767);
}

float crand(void)
{
	return (rand()&32767)* (2.0/32767) - 1;
}