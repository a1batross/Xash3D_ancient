//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         	      menu.c - game menu
//=======================================================================

#include "common.h"
#include "client.h"
#include "ui_edict.h"

// progs menu
#define UI_MAXEDICTS	(1 << 10) // should be enough for a menu

bool ui_active = false;
static dword credits_start_time;
static dword credits_fade_time;
static dword credits_show_time;
static const char **credits;
static char *creditsIndex[2048];
static char *creditsBuffer;
static uint credit_numlines;
static bool credits_active;

// internal credits
static const char *xash_credits[] =
{
	"^3Xash3D",
	"",
	"^3PROGRAMMING",
	"Uncle Mike",
	"",
	"^3ART",
	"Chorus",
	"Small Link",
	"",
	"^3LEVEL DESIGN",
	"Scrama",
	"Mitoh",
	"",
	"",
	"^3SPECIAL THANKS",
	"Chain Studios at all",
	"",
	"",
	"",
	"",
	"",
	"",
	"^3MUSIC",
	"Dj Existence",
	"",
	"",
	"^3THANKS TO",
	"ID Software at all",
	"Georg Destroy for icon graphics",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"^3Xash3D using some parts of:",
	"Doom 1 (Id Software)",
	"Quake 1 (Id Software)",
	"Quake 2 (Id Software)",
	"Quake 3 (Id Software)",
	"Half-Life (Valve Software)",
	"Darkplaces (Darkplaces Team)",
	"Quake 2 Evolved (Team Blur)",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"Copyright XashXT Group 2008 (C)",
	0
};

void UI_VM_Begin( void )
{
	PRVM_Begin;
	PRVM_SetProg(PRVM_MENUPROG);

	// set time
	if( prog ) *prog->time = cls.realtime * 0.001f;
}

void UI_KeyEvent( int key )
{
	const char *ascii = Key_KeynumToString(key);
	UI_VM_Begin();

	// setup args
	PRVM_G_FLOAT(OFS_PARM0) = key;
	prog->globals.ui->time = cls.realtime * 0.001f;
	PRVM_G_INT(OFS_PARM1) = PRVM_SetEngineString(ascii);
	PRVM_ExecuteProgram (prog->globals.ui->m_keydown, "m_keydown");

	CL_VM_Begin(); 	// restore clvm state
}

void UI_Draw( void )
{
	if( !ui_active || cls.key_dest != key_menu )
		return;

	UI_VM_Begin();
	prog->globals.ui->time = cls.realtime * 0.001f;
	PRVM_ExecuteProgram (prog->globals.ui->m_draw, "m_draw");
	UI_DrawCredits(); // display game credits

	CL_VM_Begin(); 	// restore clvm state
}

void UI_DrawCredits( void )
{
	int	i, x, y;
	float	*color;
	float	white_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };	

	if( !credits_active ) return;

	y = SCREEN_HEIGHT - (( cls.realtime - credits_start_time ) / 40.0f );

	// draw the credits
	for ( i = 0; i < credit_numlines && credits[i]; i++, y += 20 )
	{
		// skip not visible lines, but always draw end line
		if( y <= -16 && i != credit_numlines - 1) continue;
		x = ( SCREEN_WIDTH - BIGCHAR_WIDTH * com.cstrlen( credits[i] ))/2;

		if((y < (SCREEN_HEIGHT - BIGCHAR_HEIGHT) / 2) && i == credit_numlines - 1)
		{
			if(!credits_fade_time) credits_fade_time = cls.realtime;
			color = CL_FadeColor( credits_fade_time * 0.001f, credits_show_time * 0.001f );
			if( color ) SCR_DrawStringExt( x, (SCREEN_HEIGHT - BIGCHAR_HEIGHT)/2, 16, 16, credits[i], color, true );
		}
		else SCR_DrawStringExt( x, y, 16, 16, credits[i], white_color, false );
	}
	if( y < 0 && !color )
	{
		credits_active = false; // end of credits

		// let menu progs known about credits state
		PRVM_ExecuteProgram( prog->globals.ui->m_endofcredits, "m_endofcredits");
	}
}

void UI_ShowMenu( void )
{
	UI_VM_Begin();

	ui_active = true;
	prog->globals.ui->time = cls.realtime * 0.001f;
	PRVM_ExecuteProgram (prog->globals.ui->m_show, "m_show");
	CL_VM_Begin(); 	// restore clvm state
}

void UI_HideMenu( void )
{
	cls.key_dest = key_game;
	Cvar_Set( "paused", "0" );
	Key_ClearStates();
	
	UI_VM_Begin();
	ui_active = false;
	prog->globals.ui->time = cls.realtime * 0.001f;
	PRVM_ExecuteProgram (prog->globals.ui->m_hide, "m_hide");
	CL_VM_Begin(); 	// restore clvm state
}

void UI_Shutdown( void )
{
	UI_VM_Begin();

	PRVM_ExecuteProgram (prog->globals.ui->m_shutdown, "m_shutdown");
	cls.key_dest = key_game;

	// AK not using this cause Im not sure whether this is useful at all instead :
	PRVM_ResetProg();

	PRVM_End;
}


/*
=========
PF_newgame

void NewGame( void )
=========
*/
void PF_newgame( void )
{
	if(!VM_ValidateArgs( "NewGame", 0 ))
		return;
		
	// disable updates and start the cinematic going
	cl.servercount = -1;
	Cvar_SetValue( "deathmatch", 0 );
	Cvar_SetValue( "gamerules", 0 );
	Cvar_SetValue( "paused", 0 );
	Cvar_SetValue( "coop", 0 );

	cls.key_dest = key_game;
	Cbuf_AddText("loading; killserver; wait; newgame\n");
}

/*
=========
PF_readcomment

string ReadComment( float savenum )
=========
*/
void PF_readcomment( void )
{
	int	savenum;
	string	comment;

	if(!VM_ValidateArgs( "ReadComment", 1 ))
		return;

	savenum = (int)PRVM_G_FLOAT(OFS_PARM0);
	SV_ReadComment( comment, savenum );
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString( comment );
}

/*
=========
PF_joinserver

float JoinServer( float serevernum )
=========
*/
void PF_joinserver( void )
{
	string		buffer;
	int		i;

	if(!VM_ValidateArgs( "JoinServer", 1 ))
		return;
	i = (int)PRVM_G_FLOAT(OFS_PARM0);
	PRVM_G_FLOAT(OFS_RETURN) = 0;

	if( i < cls.numservers && cls.serverlist[i].maxplayers > 1 && cls.serverlist[i].netaddress )
	{
		com.sprintf( buffer, "connect %s\n", cls.serverlist[i].netaddress );
		Cbuf_AddText( buffer );
		PRVM_G_FLOAT(OFS_RETURN) = 1;
	}
}

/*
=========
PF_getserverinfo

string ServerInfo( float num )
=========
*/
void PF_getserverinfo( void )
{
	string		serverinfo;
	serverinfo_t	*s;
	int		i;

	if(!VM_ValidateArgs( "ServerInfo", 1 ))
		return;

	i = (int)PRVM_G_FLOAT(OFS_PARM0);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString( "<empty>" );
		
	if( i < cls.numservers )
	{
		s = &cls.serverlist[i];
		if( s && s->hostname && s->maxplayers > 1 ) // ignore singleplayer servers
		{
			com.snprintf( serverinfo, MAX_STRING, "%s %15s %7i/%i %7i\n", s->hostname, s->mapname, s->numplayers, s->maxplayers, s->ping );
			PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString( serverinfo );
		}
	}
}

/*
=========
PF_getmapslist

string GetMapsList( void )
=========
*/
int mapcount = 0;

void PF_getmapslist( void )
{
	char	mapstring[MAX_MSGLEN];
	string	mapname;
	script_t	*script;

	if(!VM_ValidateArgs( "GetMapsList", 0 ))
		return;

	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString( "'none'" );
	mapstring[0] = '\0';
	mapcount = 0;

	// create new maplist if not exist
	if(!Cmd_CheckMapsList())
	{
		MsgDev( D_ERROR, "GetMapsList: maps.lst not found\n");
		return;
	}

	script = Com_OpenScript( "scripts/maps.lst", NULL, 0 );
	if( !script) return; // paranoid mode :-)

	while( Com_ReadString( script, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, mapname ))
	{
		com.strcat( mapstring, va("'%s'", mapname ));
		Com_SkipRestOfLine( script ); // skip other stuff
		mapcount++;
	}
	Com_CloseScript( script );
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString( mapstring );
}

/*
=========
PF_getmapscount

string GetMapsCount( void )
=========
*/
void PF_getmapscount( void )
{
	if(!VM_ValidateArgs( "GetMapsCount", 0 ))
		return;

	PRVM_G_FLOAT(OFS_RETURN) = mapcount;
}

/*
=========
PF_newserver

void NewServer( string mapname )
=========
*/
void PF_newserver( void )
{
	const char	*s;

	if(!VM_ValidateArgs( "NewServer", 1 ))
		return;
	s = PRVM_G_STRING(OFS_PARM0);
	VM_ValidateString( s );

	Cbuf_AddText( va("map %s\n", s ));
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
PF_getmousepos

vector getmousepos( void )
=========
*/
void PF_getmousepos( void )
{
	if(!VM_ValidateArgs( "getmousepos", 0 ))
		return;

	PRVM_G_VECTOR(OFS_RETURN)[0] = cls.mouse_x;
	PRVM_G_VECTOR(OFS_RETURN)[1] = cls.mouse_y;
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
void PF_testfunction( void )
{
	mfunction_t	*func;
	const char	*s;

	if(!VM_ValidateArgs( "testfunction", 1 ))
		return;

	s = PRVM_G_STRING(OFS_PARM0);
	VM_ValidateString( s );

	func = PRVM_ED_FindFunction( s );

	if(func) PRVM_G_FLOAT(OFS_RETURN) = true;
	else PRVM_G_FLOAT(OFS_RETURN) = false;
}

/*
=========
PF_loadcredits

void loadcredits( string filename )
=========
*/
void PF_loadcredits( void )
{
	uint		count;
	char		*p;
	const char	*s;

	if(!VM_ValidateArgs( "loadcredits", 1 ))
		return;

	s = PRVM_G_STRING(OFS_PARM0);
	VM_ValidateString( s );
	
	if(!creditsBuffer)
	{
		// load credits if needed
		creditsBuffer = FS_LoadFile( s, &count );
		if( count )
		{
			if(creditsBuffer[count - 1] != '\n' && creditsBuffer[count - 1] != '\r')
			{
				creditsBuffer = Mem_Realloc( zonepool, creditsBuffer, count + 2 );
				com.strncpy( creditsBuffer + count, "\r", 1 ); // add terminator
				count += 2;
                    	}

			p = creditsBuffer;
			for( credit_numlines = 0; credit_numlines < 2048; credit_numlines++)
			{
				creditsIndex[credit_numlines] = p;
				while( *p != '\r' && *p != '\n')
				{
					p++;
					if( --count == 0 ) break;
				}
				if( *p == '\r' )
				{
					*p++ = 0;
					if (--count == 0) break;
				}
				*p++ = 0;
				if (--count == 0) break;
			}
			creditsIndex[++credit_numlines] = 0;
			credits = creditsIndex;
		}
		else
		{
			// use built-in credits
			credits =  xash_credits;
			credit_numlines = (sizeof(xash_credits) / sizeof(xash_credits[0])) - 1; // skip term
		}
	}
	credits_active = false;
}

/*
=========
PF_runcredits

void runcredits( void )
=========
*/
void PF_runcredits( void )
{
	if(!VM_ValidateArgs( "runcredits", 0 ))
		return;

	// run credits
	credits_start_time = cls.realtime;
	credits_show_time = bound( 100, com.strlen(credits[credit_numlines - 1]) * 1000, 12000 );
	credits_fade_time = 0;
	credits_active = true;
}

/*
=========
PF_stopcredits

void stopcredits( void )
=========
*/
void PF_stopcredits( void )
{
	if(!VM_ValidateArgs( "stopcredits", 0 ))
		return;

	// stop credits
	credits_active = false;

	// let menu progs known about credits state
	PRVM_ExecuteProgram( prog->globals.ui->m_endofcredits, "m_endofcredits");
}

/*
=========
PF_creditsactive

void creditsactive( void )
=========
*/
void PF_creditsactive( void )
{
	if(!VM_ValidateArgs( "creditsactive", 0 ))
		return;

	// check state
	if( credits_active )
		PRVM_G_FLOAT(OFS_RETURN) = 1;
	else PRVM_G_FLOAT(OFS_RETURN) = 0; 
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
VM_Random,			// #18 float Random( void )
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
VM_SubString,			// #33 string substring( string s, float start, float length )
VM_AddCommand,			// #34 void Add_Command( string s )
VM_atof,				// #35 float atof( string s )
VM_atoi,				// #36 float atoi( string s )
VM_atov,				// #37 vector atov( string s )
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
VM_abs,				// #76 float abs (float f)
NULL,				// #77 -- reserved --
NULL,				// #78 -- reserved --
VM_VectorNormalize,			// #79 vector VectorNormalize( vector v )
VM_VectorLength,			// #80 float VectorLength( vector v )
e10, e10,				// #81 - #100 are reserved for future expansions

// uimenufuncs_t
VM_FindEdict,			// #101 entity find( entity start, .string field, string match )
VM_FindField,			// #102 entity findfloat( entity start, .float field, float match )
NULL,				// #103 -- reserved --
PF_serverstate,			// #104 float serverstate( void )
PF_clientstate,			// #105 float clientstate( void )
VM_localsound,			// #106 void localsound( string sample )
PF_getmousepos,			// #107 vector getmousepos( void )
PF_loadfromdata,			// #108 void loadfromdata( string data )
PF_loadfromfile,			// #109 float loadfromfile( string file )
VM_precache_pic,			// #110 float precache_pic( string pic )
VM_drawcharacter,			// #111 float drawchar( vector pos, float char, vector scale, vector rgb, float alpha )
VM_drawstring,			// #112 float drawstring( vector pos, string text, vector scale, vector rgb, float alpha )
VM_drawpic,			// #113 float drawpic( vector pos, string pic, vector size, vector rgb, float alpha )
VM_drawfill,			// #114 void drawfill( vector pos, vector size, vector rgb, float alpha )
VM_drawmodel,			// #115 void drawmodel( vector pos, vector size, string model, vector origin, vector angles, float sequence )
VM_getimagesize,			// #116 vector getimagesize( string pic )
PF_setkeydest,			// #117 void setkeydest( float dest )
PF_callfunction,			// #118 void callfunction( ..., string function_name )
PF_testfunction,			// #119 float testfunction( string function_name )
PF_newgame,			// #120 void NewGame( void )
PF_readcomment,			// #121 string ReadComment( float savenum )
PF_joinserver,			// #122 float JoinServer( float num )
PF_getserverinfo,			// #123 string ServerInfo( float num )
PF_getmapslist,			// #124 string GetMapsList( void )
PF_getmapscount,			// #125 float GetMapsCount( void )
PF_newserver,			// #126 void NewServer( string mapname )
PF_loadcredits,			// #127 void loadcredits( string filename )
PF_runcredits,			// #128 void runcredits( void )
PF_stopcredits,			// #129 void stopcredits( void )
PF_creditsactive,			// #130 float creditsactive( void )
VM_EdictError,			// #131 void objerror( string s )
};

const int vm_ui_numbuiltins = sizeof(vm_ui_builtins) / sizeof(prvm_builtin_t);

void UI_Init( void )
{
	PRVM_Begin;
	PRVM_InitProg( PRVM_MENUPROG );

	prog->progs_mempool = Mem_AllocPool( "Uimenu Progs" );
	prog->builtins = vm_ui_builtins;
	prog->numbuiltins = vm_ui_numbuiltins;
	prog->edictprivate_size = sizeof(vm_edict_t);
	prog->limit_edicts = UI_MAXEDICTS;
	prog->name = "uimenu";
	prog->num_edicts = 1;
	prog->loadintoworld = false;
	prog->init_cmd = VM_Cmd_Init;
	prog->reset_cmd = VM_Cmd_Reset;
	prog->error_cmd = VM_Error;
	prog->filecrc = PROG_CRC_UIMENU;

	PRVM_LoadProgs( va("%s/uimenu.dat", GI->vprogs_dir ));
	*prog->time = cls.realtime * 0.001f;

	PRVM_ExecuteProgram (prog->globals.ui->m_init, "m_init");
	PRVM_End;
}