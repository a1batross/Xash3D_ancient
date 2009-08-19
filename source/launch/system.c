//=======================================================================
//			Copyright XashXT Group 2007 ©
//			utils.c - shared launcher utils
//=======================================================================

#include "launch.h"
#include "baserc_api.h"
#include "engine_api.h"
#include "mathlib.h"

#define MAX_QUED_EVENTS		256
#define MASK_QUED_EVENTS		(MAX_QUED_EVENTS - 1)
#define LOG_BUFSIZE			MAX_MSGLEN * 4

system_t		Sys;
stdlib_api_t	com;
baserc_exp_t	*rc;	// library of resources
timer_t		Msec;
launch_exp_t	*Host;	// callback to mainframe 
sys_event_t	event_que[MAX_QUED_EVENTS];
int		event_head, event_tail;

dll_info_t xtools_dll = { "xtools.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(launch_exp_t) };
dll_info_t engine_dll = { "engine.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(launch_exp_t) };
dll_info_t baserc_dll = { "baserc.dll", NULL, "CreateAPI", NULL, NULL, false, sizeof(baserc_exp_t)};

static const char *show_credits = "\n\n\n\n\tCopyright XashXT Group %s ©\n\t\
          All Rights Reserved\n\n\t           Visit www.xash.ru\n";

// stubs
void NullInit( int argc, char **argv ) {}
void NullFunc( void ) {}
void NullPrint( const char *msg ) {}

void Sys_GetStdAPI( void )
{
	// interface validator
	com.api_size = sizeof(stdlib_api_t);

	// base events
	com.instance = Sys_NewInstance;
	com.printf = Sys_Msg;
	com.dprintf = Sys_MsgDev;
	com.error = Sys_Error;
	com.abort = Sys_Break;
	com.exit = Sys_Exit;
	com.print = Sys_Print;
	com.sleep = Sys_Sleep;
	com.clipboard = Sys_GetClipboardData;
	com.queevent = Sys_QueEvent;			// add event to queue
	com.getevent = Sys_GetEvent;			// get system events

	// crclib.c funcs
	com.crc_init = CRC_Init;
	com.crc_block = CRC_Block;
	com.crc_process = CRC_ProcessByte;
	com.crc_sequence = CRC_BlockSequence;
	com.crc_blockchecksum = Com_BlockChecksum;
	com.crc_blockchecksumkey = Com_BlockChecksumKey;

	// memlib.c
	com.memcpy = _crt_mem_copy;			// first time using
	com.memset = _crt_mem_set;			// first time using
	com.realloc = _mem_realloc;
	com.move = _mem_move;
	com.malloc = _mem_alloc;
	com.free = _mem_free;
	com.mallocpool = _mem_allocpool;
	com.freepool = _mem_freepool;
	com.clearpool = _mem_emptypool;
	com.memcheck = _mem_check;
	com.newarray = _mem_alloc_array;
	com.delarray = _mem_free_array;
	com.newelement = _mem_alloc_array_element;
	com.delelement = _mem_free_array_element;
	com.getelement = _mem_get_array_element;
	com.arraysize = _mem_array_size;

	// network.c funcs
	com.NET_Init = NET_Init;
	com.NET_Shutdown = NET_Shutdown;
	com.NET_Sleep = NET_Sleep;
	com.NET_Config = NET_Config;
	com.NET_AdrToString = NET_AdrToString;
	com.NET_StringToAdr = NET_StringToAdr;
	com.NET_SendPacket = NET_SendPacket;
	com.NET_IsLocalAddress = NET_IsLocalAddress;
	com.NET_BaseAdrToString = NET_BaseAdrToString;
	com.NET_StringToAdr = NET_StringToAdr;
	com.NET_CompareAdr = NET_CompareAdr;
	com.NET_CompareBaseAdr = NET_CompareBaseAdr;
	com.NET_GetPacket = NET_GetPacket;
	com.NET_SendPacket = NET_SendPacket;

	// common functions
	com.Com_InitRootDir = FS_InitRootDir;		// init custom rootdir 
	com.Com_LoadGameInfo = FS_LoadGameInfo;		// gate game info from script file
	com.Com_AddGameHierarchy = FS_AddGameHierarchy;	// add base directory in search list
	com.Com_AllowDirectPaths = FS_AllowDirectPaths;	// allow direct paths e.g. C:\windows
	com.Com_CheckParm = FS_CheckParm;		// get parm from cmdline
	com.Com_GetParm = FS_GetParmFromCmdLine;	// get filename without path & ext
	com.Com_FileBase = FS_FileBase;		// get filename without path & ext
	com.Com_FileExists = FS_FileExists;		// return true if file exist
	com.Com_FileSize = FS_FileSize;		// same as Com_FileExists but return filesize
	com.Com_FileTime = FS_FileTime;		// same as Com_FileExists but return filetime
	com.Com_FileExtension = FS_FileExtension;	// return extension of file
	com.Com_RemovePath = FS_FileWithoutPath;	// return file without path
	com.Com_StripExtension = FS_StripExtension;	// remove extension if present
	com.Com_StripFilePath = FS_ExtractFilePath;	// get file path without filename.ext
	com.Com_DefaultExtension = FS_DefaultExtension;	// append extension if not present
	com.Com_ClearSearchPath = FS_ClearSearchPath;	// delete all search pathes
	com.Com_CreateThread = Sys_RunThreadsOnIndividual;// run individual thread
	com.Com_ThreadLock = Sys_ThreadLock;		// lock current thread
	com.Com_ThreadUnlock = Sys_ThreadUnlock;	// unlock numthreads
	com.Com_NumThreads = Sys_GetNumThreads;		// returns count of active threads

	com.Com_OpenScript = PS_LoadScript;		// loading script into buffer
	com.Com_CloseScript = PS_FreeScript;		// release current script
	com.Com_ResetScript = PS_ResetScript;		// jump to start of scriptfile 
	com.Com_EndOfScript = PS_EndOfScript;		// returns true if end of script reached
	com.Com_SkipBracedSection = PS_SkipBracedSection;	// skip braced section with specified depth
	com.Com_SkipRestOfLine = PS_SkipRestOfLine;	// skip all tokene the rest of line
	com.Com_ReadToken = PS_ReadToken;		// generic reading
	com.Com_SaveToken = PS_SaveToken;		// save current token to get it again

	// script machine simple user interface
	com.Com_ReadString = PS_GetString;		// string
	com.Com_ReadFloat = PS_GetFloat;		// float value
	com.Com_ReadDword = PS_GetUnsigned;		// unsigned integer
	com.Com_ReadLong = PS_GetInteger;		// signed integer

	com.PatchEval = Patch_Evaluate;		// evaluate patch
	com.PatchFlat = Patch_GetFlatness;		// get flatness for patch

	com.Com_Search = FS_Search;			// returned list of founded files
	com.Com_HashKey = Com_HashKey;		// returns hash key for a string (generic fucntion)
	com.Com_LoadRes = Sys_LoadRes;		// get internal resource by name

	// console variables
	com.Cvar_Get = Cvar_Get;
	com.Cvar_FullSet = Cvar_FullSet;
	com.Cvar_SetLatched = Cvar_SetLatched;
	com.Cvar_SetValue = Cvar_SetValue;
	com.Cvar_SetString = Cvar_Set;
	com.Cvar_GetValue = Cvar_VariableValue;
	com.Cvar_GetString = Cvar_VariableString;
	com.Cvar_LookupVars = Cvar_LookupVars;
	com.Cvar_FindVar = Cvar_FindVar;

	// console commands
	com.Cmd_Exec = Cbuf_ExecuteText;		// process cmd buffer
	com.Cmd_Argc = Cmd_Argc;
	com.Cmd_Args = Cmd_Args;
	com.Cmd_Argv = Cmd_Argv; 
	com.Cmd_LookupCmds = Cmd_LookupCmds;
	com.Cmd_AddCommand = Cmd_AddCommand;
	com.Cmd_DelCommand = Cmd_RemoveCommand;
	com.Cmd_TokenizeString = Cmd_TokenizeString;

	// real filesystem
	com.fopen = FS_Open;		// same as fopen
	com.fclose = FS_Close;		// same as fclose
	com.fwrite = FS_Write;		// same as fwrite
	com.fread = FS_Read;		// same as fread, can see trough pakfile
	com.fprint = FS_Print;		// printed message into file		
	com.fprintf = FS_Printf;		// same as fprintf
	com.fgetc = FS_Getc;		// same as fgetc
	com.fgets = FS_Gets;		// like a fgets, but can return EOF
	com.fseek = FS_Seek;		// fseek, can seek in packfiles too
	com.fremove = FS_Remove;		// like remove
	com.ftell = FS_Tell;		// like a ftell
	com.feof = FS_Eof;			// like a feof

	// virtual filesystem
	com.vfcreate = VFS_Create;		// create virtual stream
	com.vfopen = VFS_Open;		// virtual fopen
	com.vfclose = VFS_Close;		// free buffer or write dump
	com.vfwrite = VFS_Write;		// write into buffer
	com.vfread = VFS_Read;		// read from buffer
	com.vfgets = VFS_Gets;		// read text line 
	com.vfprint = VFS_Print;		// write message
	com.vfprintf = VFS_Printf;		// write formatted message
	com.vfseek = VFS_Seek;		// fseek, can seek in packfiles too
	com.vfunpack = VFS_Unpack;		// inflate zipped buffer
	com.vfbuffer = VFS_GetBuffer;		// get pointer at start vfile buffer
	com.vftell = VFS_Tell;		// like a ftell
	com.vfeof = VFS_Eof;		// like a feof

	// wadstorag filesystem
	com.wfcheck = W_Check;		// validate container
	com.wfopen = W_Open;		// open wad file or create new
	com.wfclose = W_Close;		// close wadfile
	com.wfwrite = W_SaveLump;		// dump lump into disk
	com.wfread = W_LoadLump;		// load lump into memory

	// filesystem simply user interface
	com.Com_LoadFile = FS_LoadFile;		// load file into heap
	com.Com_WriteFile = FS_WriteFile;		// write file into disk
	com.Com_LoadLibrary = Sys_LoadLibrary;		// load library 
	com.Com_FreeLibrary = Sys_FreeLibrary;		// free library
	com.Com_GetProcAddress = Sys_GetProcAddress;	// gpa
	com.Com_DoubleTime = Sys_DoubleTime;		// hi-res timer
	com.Com_Milliseconds = Sys_Milliseconds;

	// built-in imagelib functions
	com.ImageLoad = FS_LoadImage;			// load image from disk or wad-file
	com.ImageSave = FS_SaveImage;			// save image into specified format 
	com.ImageFree = FS_FreeImage;			// release image buffer
	com.ImglibSetup = Image_Setup;		// set imagelib global features
	com.ImagePFDesc = Image_GetPixelFormat;		// get some info about current fmt
	com.ImageConvert = Image_Process;		// flip, rotate, resample etc

	com.Com_RandomLong = Com_RandomLong;
	com.Com_RandomFloat = Com_RandomFloat;

	// changed after called Sys_InitCPU
	com.sincos = SinCos;			// fast sincos
	com.atan2 = atan2f;				// fast arctan
	com.acos = acosf;				// fast arccos
	com.asin = asinf;				// fast arcsin
	com.sqrt = sqrtf;				// fast sqrt
	com.sin = sinf;				// fast sine
	com.cos = cosf;				// fast cosine
	com.tan = tanf;				// fast tan

	// stdlib.c funcs
	com.strnupr = com_strnupr;
	com.strnlwr = com_strnlwr;
	com.strupr = com_strupr;
	com.strlwr = com_strlwr;
	com.strlen = com_strlen;
	com.cstrlen = com_cstrlen;
	com.toupper = com_toupper;
	com.tolower = com_tolower;
	com.strncat = com_strncat;
	com.strcat = com_strcat;
	com.strncpy = com_strncpy;
	com.strcpy = com_strcpy;
	com.stralloc = com_stralloc;
	com.is_digit = com_isdigit;
	com.atoi = com_atoi;
	com.atof = com_atof;
	com.atov = com_atov;
	com.strchr = com_strchr;
	com.strrchr = com_strrchr;
	com.strnicmp = com_strnicmp;
	com.stricmp = com_stricmp;
	com.strncmp = com_strncmp;
	com.strcmp = com_strcmp;
	com.stristr = com_stristr;
	com.strstr = com_strstr;
	com.strpack = com_strpack;
	com.strunpack = com_strunpack;
	com.vsprintf = com_vsprintf;
	com.sprintf = com_sprintf;
	com.va = va;
	com.vsnprintf = com_vsnprintf;
	com.snprintf = com_snprintf;
	com.pretifymem = com_pretifymem;
	com.timestamp = com_timestamp;

	// stringtable.c system
	com.st_create = StringTable_CreateNewSystem;
	com.st_getstring = StringTable_GetString;
	com.st_setstring = StringTable_SetString;
	com.st_load = StringTable_LoadSystem;
	com.st_getname = StringTable_GetName;
	com.st_save = StringTable_SaveSystem;
	com.st_clear = StringTable_ClearSystem;
	com.st_remove = StringTable_DeleteSystem;

	com.GameInfo = &GI;
}

/*
==================
Parse program name to launch and determine work style

NOTE: at this day we have ten instances

0. "offline" - invalid instance
1. "credits" - show engine credits
2. "dedicated" - dedicated server
3. "normal" - normal or dedicated game launch
4. "bsplib" - four BSP compilers in one
5. "qcclib" - quake c complier
6. "sprite" - sprite creator (requires qc. script)
7. "studio" - Half-Life style models creator (requires qc. script) 
8. "roqlib" - wad-file maker
9. "ripper" - RESOURCE extraCTOR genERIC
10."dpvenc" - dp video encoder
==================
*/
void Sys_LookupInstance( void )
{
	char	szTemp[4096];
	bool	dedicated = false;

	Sys.app_name = HOST_OFFLINE;
	// we can specified custom name, from Sys_NewInstance
	if(GetModuleFileName( NULL, szTemp, MAX_SYSPATH ) && Sys.app_state != SYS_RESTART )
		FS_FileBase( szTemp, Sys.ModuleName );

	// determine host type
	if( Sys.ModuleName[0] == '#' || Sys.ModuleName[0] == '©' )
	{
		if( Sys.ModuleName[0] == '#' ) dedicated = true;
		if( Sys.ModuleName[0] == '©' ) com_strcpy( Sys.progname, "credits" );
		// cutoff hidden symbols
		com_strncpy( szTemp, Sys.ModuleName + 1, MAX_SYSPATH );
		com_strncpy( Sys.ModuleName, szTemp, MAX_SYSPATH );			
	}

	// lookup all instances
	if(!com_strcmp(Sys.progname, "credits"))
	{
		Sys.app_name = HOST_CREDITS;		// easter egg
		Sys.linked_dll = NULL;		// no need to loading library
		Sys.log_active = Sys.developer = 0;	// clear all dbg states
		com_strcpy( Sys.caption, "About" );
		Sys.con_showcredits = true;
	}
	else if(!com_strcmp(Sys.progname, "normal"))
	{
		if( dedicated )
		{
			Sys.app_name = HOST_DEDICATED;
			Sys.con_readonly = false;
		}
		else
		{
			Sys.app_name = HOST_NORMAL;
			Sys.con_readonly = true;
			// don't show console as default
			if( Sys.developer < D_WARN )
				Sys.con_showalways = false;
		}
		Sys.linked_dll = &engine_dll;	// pointer to engine.dll info
		com_sprintf( Sys.log_path, "engine.log", com_timestamp( TIME_NO_SECONDS )); // logs folder
		com_strcpy(Sys.caption, va("Xash3D ver.%g", XASH_VERSION ));
	}
	else if(!com_strcmp(Sys.progname, "bsplib"))
	{
		Sys.app_name = HOST_BSPLIB;
		Sys.linked_dll = &xtools_dll;	// pointer to common.dll info
		com_strcpy(Sys.log_path, "bsplib.log" ); // xash3d root directory
		com_strcpy(Sys.caption, "Xash3D BSP Compiler");

		// TEMPORARY HACK FOR DEBUG NEW BSPLIB
		Sys.developer = 3;
	}
	else if(!com_strcmp(Sys.progname, "qcclib"))
	{
		Sys.app_name = HOST_QCCLIB;
		Sys.linked_dll = &xtools_dll;	// pointer to common.dll info
		com_sprintf(Sys.log_path, "%s/compile.log", sys_rootdir ); // same as .exe file
		com_strcpy(Sys.caption, "Xash3D QuakeC Compiler");
	}
	else if(!com_strcmp(Sys.progname, "sprite"))
	{
		Sys.app_name = HOST_SPRITE;
		Sys.linked_dll = &xtools_dll;	// pointer to common.dll info
		com_sprintf(Sys.log_path, "%s/spritegen.log", sys_rootdir ); // same as .exe file
		com_strcpy(Sys.caption, "Xash3D Sprite Compiler");
	}
	else if(!com_strcmp(Sys.progname, "studio"))
	{
		Sys.app_name = HOST_STUDIO;
		Sys.linked_dll = &xtools_dll;	// pointer to common.dll info
		com_sprintf(Sys.log_path, "%s/studiomdl.log", sys_rootdir ); // same as .exe file
		com_strcpy(Sys.caption, "Xash3D Studio Models Compiler");
	}
	else if(!com_strcmp(Sys.progname, "wadlib"))
	{
		Sys.app_name = HOST_WADLIB;
		Sys.linked_dll = &xtools_dll;	// pointer to common.dll info
		com_sprintf(Sys.log_path, "%s/wadlib.log", sys_rootdir ); // same as .exe file
		com_strcpy(Sys.caption, "Xash3D Wad2\\Wad3 maker");
	}
	else if(!com_strcmp(Sys.progname, "ripper"))
	{
		Sys.app_name = HOST_RIPPER;
		Sys.con_readonly = true;
		Sys.log_active = true;	// always create log
		Sys.linked_dll = &xtools_dll;	// pointer to wdclib.dll info
		com_sprintf(Sys.log_path, "%s/decompile.log", sys_rootdir ); // default
		com_strcpy(Sys.caption, va("Quake Recource Extractor ver.%g", XASH_VERSION ));
	}
	else if(!com_strcmp(Sys.progname, "dpvenc"))
	{
		Sys.app_name = HOST_DPVENC;
		Sys.con_readonly = true;
		// don't show console as default
		if( Sys.developer < D_NOTE ) Sys.con_showalways = false;
		Sys.linked_dll = &xtools_dll;	// pointer to dpvenc.dll info
		com_sprintf(Sys.log_path, "%s/movie.log", sys_rootdir ); // logs folder
		com_strcpy(Sys.caption, "DarkPlaces Video Encoder" );
	}
	// share instance over all system
	GI.instance = Sys.app_name;
}

/*
==================
Find needed library, setup and run it
==================
*/
void Sys_CreateInstance( void )
{
	// export
	launch_t	CreateHost, CreateBaserc;

	srand( time( NULL ));		// init random generator
	Sys_LoadLibrary( Sys.linked_dll );	// loading library if need

	// pre initializations
	switch( Sys.app_name )
	{
	case HOST_NORMAL:
	case HOST_DEDICATED:
	case HOST_DPVENC:		
	case HOST_BSPLIB:
	case HOST_QCCLIB:
	case HOST_SPRITE:
	case HOST_STUDIO:
	case HOST_WADLIB:
	case HOST_RIPPER:
		Sys_LoadLibrary( &baserc_dll ); // load baserc
		CreateHost = (void *)Sys.linked_dll->main;
		Host = CreateHost( &com, NULL ); // second interface not allowed
		Sys.Init = Host->Init;
		Sys.Main = Host->Main;
		Sys.Free = Host->Free;
		Sys.CPrint = Host->CPrint;
		if( baserc_dll.link )
		{
			CreateBaserc = (void *)baserc_dll.main;
			rc = CreateBaserc( &com, NULL ); 
		}
		break;
	case HOST_CREDITS:
		Sys_Break( show_credits, com_timestamp( TIME_YEAR_ONLY ));
		break;
	case HOST_OFFLINE:
		Sys_Break( "Host offline\n" );		
		break;
	}

	// init our host now!
	Sys.Init( fs_argc, fs_argv );

	// post initializations
	switch( Sys.app_name )
	{
	case HOST_NORMAL:
		Con_ShowConsole( false );		// hide console
		Cbuf_AddText( "exec init.rc\n" );	// execute startup config and cmdline
	case HOST_DEDICATED:
		Cbuf_Execute();
		// if stuffcmds wasn't run, then init.rc is probably missing, use default
		if( !Sys.stuffcmdsrun ) Cbuf_ExecuteText( EXEC_NOW, "stuffcmds\n" );
		break;
	case HOST_DPVENC:
	case HOST_BSPLIB:
	case HOST_QCCLIB:
	case HOST_SPRITE:
	case HOST_STUDIO:
	case HOST_WADLIB:
	case HOST_RIPPER:
		// always run stuffcmds for current instances
		Cbuf_ExecuteText( EXEC_NOW, "stuffcmds\n" );
		break;
	}

	Cmd_RemoveCommand( "setc" );	// potentially backdoor for change system settings
	Sys.app_state = SYS_FRAME;	// system is now active
}

/*
==================
Sys_ParseCommandLine

==================
*/
void Sys_ParseCommandLine( LPSTR lpCmdLine )
{
	fs_argc = 1;
	fs_argv[0] = "exe";

	while( *lpCmdLine && (fs_argc < MAX_NUM_ARGVS))
	{
		while( *lpCmdLine && *lpCmdLine <= ' ' ) lpCmdLine++;
		if (!*lpCmdLine) break;

		if (*lpCmdLine == '\"')
		{
			// quoted string
			lpCmdLine++;
			fs_argv[fs_argc] = lpCmdLine;
			fs_argc++;
			while( *lpCmdLine && (*lpCmdLine != '\"')) lpCmdLine++;
		}
		else
		{
			// unquoted word
			fs_argv[fs_argc] = lpCmdLine;
			fs_argc++;
			while( *lpCmdLine && *lpCmdLine > ' ') lpCmdLine++;
		}
		if( *lpCmdLine )
		{
			*lpCmdLine = 0;
			lpCmdLine++;
		}
	}

}

void Sys_MergeCommandLine( LPSTR lpCmdLine )
{
	const char *blank = "censored";
	int	i;
	
	for( i = 0; i < fs_argc; i++ )
	{
		// we wan't return to first game
		if(!com_stricmp( "-game", fs_argv[i] )) fs_argv[i] = (char *)blank;
		// probably it's timewaster, because engine rejected second change
		if(!com_stricmp( "+game", fs_argv[i] )) fs_argv[i] = (char *)blank;
		// you sure what is map exists in new game?
		if(!com_stricmp( "+map", fs_argv[i] )) fs_argv[i] = (char *)blank;
		// just stupid action
		if(!com_stricmp( "+load", fs_argv[i] )) fs_argv[i] = (char *)blank;
		// changelevel beetwen games? wow it's great idea!
		if(!com_stricmp( "+changelevel", fs_argv[i] )) fs_argv[i] = (char *)blank;
	}
}

/*
================
Sys_Print

print into window console
================
*/
void Sys_Print( const char *pMsg )
{
	const char	*msg;
	char		buffer[MAX_MSGLEN];
	char		logbuf[MAX_MSGLEN];
	char		*b = buffer;
	char		*c = logbuf;	
	int		i = 0;

	if( Sys.con_silentmode ) return;
	if( Sys.CPrint && Sys.app_name == HOST_NORMAL )
		Sys.CPrint( pMsg );

	// if the message is REALLY long, use just the last portion of it
	if ( com_strlen( pMsg ) > MAX_MSGLEN - 1 )
		msg = pMsg + com_strlen( pMsg ) - MAX_MSGLEN + 1;
	else msg = pMsg;

	// copy into an intermediate buffer
	while ( msg[i] && (( b - buffer ) < sizeof( buffer ) - 1 ))
	{
		if( msg[i] == '\n' && msg[i+1] == '\r' )
		{
			b[0] = '\r';
			b[1] = c[0] = '\n';
			b += 2, c++;
			i++;
		}
		else if( msg[i] == '\r' )
		{
			b[0] = c[0] = '\r';
			b[1] = '\n';
			b += 2, c++;
		}
		else if( msg[i] == '\n' )
		{
			b[0] = '\r';
			b[1] = c[0] = '\n';
			b += 2, c++;
		}
		else if( msg[i] == '\35' || msg[i] == '\36' || msg[i] == '\37' )
		{
			i++; // skip console pseudo graph
		}
		else if(IsColorString( &msg[i])) i++; // skip color prefix
		else
		{
			*b = *c = msg[i];
			b++, c++;
		}
		i++;
	}
	*b = *c = 0; // cutoff garbage

	// because we needs to kill any psedo graph symbols
	// and color strings for other instances
	if( Sys.CPrint && Sys.app_name != HOST_NORMAL )
		Sys.CPrint( logbuf );

	Sys_PrintLog( logbuf );

	// don't flood system console with memory allocation messages or another
	if( Sys.Con_Print && Sys.printlevel < D_MEMORY )
		Sys.Con_Print( buffer );
	Sys.printlevel = 0; // reset before next message
}

/*
================
Sys_Msg

formatted message
================
*/
void Sys_Msg( const char *pMsg, ... )
{
	va_list	argptr;
	char text[MAX_MSGLEN];
	
	va_start( argptr, pMsg );
	com_vsprintf( text, pMsg, argptr );
	va_end( argptr );
	Sys.printlevel = 0;

	Sys_Print( text );
}

void Sys_MsgDev( int level, const char *pMsg, ... )
{
	va_list	argptr;
	char	text[MAX_MSGLEN];

	if( Sys.developer < level ) return;
	Sys.printlevel = level;

	va_start( argptr, pMsg );
	com_vsprintf( text, pMsg, argptr );
	va_end( argptr );

	switch( level )
	{
	case D_INFO:	
		Sys_Print( text );
		break;
	case D_WARN:
		Sys_Print(va("^3Warning:^7 %s", text));
		break;
	case D_ERROR:
		Sys_Print(va("^1Error:^7 %s", text));
		break;
	case D_LOAD:
		Sys_Print(va("^2Loading: ^7%s", text));
		break;
	case D_NOTE:
		Sys_Print( text );
		break;
	case D_MEMORY:
		Sys_Print( text );
		break;
	case D_STRING:
		Sys_Print(va("^6AllocString: ^7%s", text));
		break;
	}
}

/*
================
Sys_DoubleTime
================
*/
double Sys_DoubleTime( void )
{
	double newtime;

	if( !Msec.initialized )
	{
		timeBeginPeriod( 1 );
		Msec.timebase = timeGetTime();
		Msec.initialized = true;
		Msec.oldtime = (double)timeGetTime() * 0.001;
	}
	newtime = ( double )timeGetTime() * 0.001;

	if( newtime < Msec.oldtime )
	{
		// warn if it's significant
		if( newtime - Msec.oldtime < -0.01 )
		{
			MsgDev( D_ERROR, "Sys_DoubleTime: time stepped backwards\n" );
			MsgDev( D_NOTE, "(went from %f to %f, difference %f)\n", Msec.oldtime, newtime, newtime - Msec.oldtime );
		}
	}
	else Msec.curtime += newtime - Msec.oldtime;
	Msec.oldtime = newtime;

	return Msec.curtime;
}

/*
================
Sys_Milliseconds
================
*/
dword Sys_Milliseconds( void )
{
	dword	curtime;

	if( !Msec.initialized )
	{
		timeBeginPeriod( 1 );
		Msec.timebase = timeGetTime();
		Msec.initialized = true;
	}
	curtime = timeGetTime() - Msec.timebase;

	return curtime;
}
/*
================
Sys_GetClipboardData

create buffer, that contain clipboard
================
*/
char *Sys_GetClipboardData( void )
{
	char *data = NULL;
	char *cliptext;

	if ( OpenClipboard( NULL ) != 0 )
	{
		HANDLE hClipboardData;

		if(( hClipboardData = GetClipboardData( CF_TEXT )) != 0 )
		{
			if ( ( cliptext = GlobalLock( hClipboardData )) != 0 ) 
			{
				data = Malloc( GlobalSize( hClipboardData ) + 1 );
				com_strcpy( data, cliptext );
				GlobalUnlock( hClipboardData );
			}
		}
		CloseClipboard();
	}
	return data;
}

/*
================
Sys_GetCurrentUser

returns username for current profile
================
*/
char *Sys_GetCurrentUser( void )
{
	static string s_userName;
	dword size = sizeof( s_userName );

	if( !GetUserName( s_userName, &size ))
		com_strcpy( s_userName, "player" );
	if( !s_userName[0] )
		com_strcpy( s_userName, "player" );

	return s_userName;
}

/*
================
Sys_Sleep

freeze application for some time
================
*/
void Sys_Sleep( int msec )
{
	msec = bound( 1, msec, 1000 );
	Sleep( msec );
}

void Sys_WaitForQuit( void )
{
	MSG	msg;

	if( Sys.hooked_out )
	{
		// in-pipeline mode we don't want to wait for press any key
		if(abs((int)GetStdHandle(STD_OUTPUT_HANDLE)) <= 100 )
		{
			Sys_Print( "press any key to quit\n" );
			system( "@pause>nul\n" ); // wait for quit
		}
	}
	else
	{
		Con_RegisterHotkeys();		
		Mem_Set( &msg, 0, sizeof( msg ));

		// wait for the user to quit
		while( msg.message != WM_QUIT )
		{
			if(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			} 
			else Sys_Sleep( 20 );
		}
	}
}

/*
================
Sys_Error

NOTE: we must prepare engine to shutdown
before call this
================
*/
void Sys_Error(const char *error, ...)
{
	va_list		argptr;
	char		text[MAX_MSGLEN];
         
	if( Sys.app_state == SYS_ERROR )
		return; // don't multiple executes

	// make sure what console received last message
	// stupid windows bug :( 
	if( Sys.app_state == SYS_RESTART )
		Sys_Sleep( 200 );

	Sys.error = true;
	Sys.app_state = SYS_ERROR;	
	va_start( argptr, error );
	com_vsprintf( text, error, argptr );
	va_end( argptr );
         
	Con_ShowConsole( true );
	if( Sys.developer >= D_ERROR ) Sys_Print( text );	// print error message
	else Sys_Print( "Internal engine error\n" );	// don't confuse non-developers with technique stuff

	Sys_WaitForQuit();
	Sys_Exit();
}

void Sys_Break(const char *error, ...)
{
	va_list		argptr;
	char		text[MAX_MSGLEN];
         
	va_start( argptr, error );
	com_vsprintf( text, error, argptr );
	va_end(argptr);

	Sys.error = true;	
	Sys.app_state = SYS_ERROR;
	Con_ShowConsole( true );
	Sys_Print( text );
	Sys_WaitForQuit();
	Sys_Exit();
}

void Sys_Abort( void )
{
	// aborting by user, run normal shutdown procedure
	Sys.app_state = SYS_ABORT;
	Sys_Exit();
}

void Sys_Init( void )
{
	MEMORYSTATUS	lpBuffer;
	char		dev_level[4];

	lpBuffer.dwLength = sizeof(MEMORYSTATUS);
	GlobalMemoryStatus( &lpBuffer );
	ZeroMemory( &Msec, sizeof( Msec ));		// can't use memset - not init
	Sys.logfile = NULL;

	// get current hInstance
	Sys.hInstance = (HINSTANCE)GetModuleHandle( NULL );

	Sys_GetStdAPI();
	Sys.Init = NullInit;
	Sys.Main = NullFunc;
	Sys.Free = NullFunc;
	Sys.CPrint = NullPrint;
	Sys.Con_Print = NullPrint;

	// some commands may turn engine into infinity loop,
	// e.g. xash.exe +game xash -game xash
	// so we clearing all cmd_args, but leave dbg states as well
	if( Sys.app_state != SYS_RESTART )
		Sys_ParseCommandLine( GetCommandLine());
	else Sys_MergeCommandLine( GetCommandLine());

	// parse and copy args into local array
	if(FS_CheckParm( "-log" )) Sys.log_active = true;
	if(FS_GetParmFromCmdLine( "-dev", dev_level, sizeof( dev_level )))
		Sys.developer = com_atoi( dev_level );
          
	FS_UpdateEnvironmentVariables();	// set working directory
	SetErrorMode( SEM_FAILCRITICALERRORS );	// no abort/retry/fail errors
	if( Sys.hooked_out ) atexit( Sys_Abort );

	// set default state 
	Sys.con_showalways = Sys.con_readonly = true;
	Sys.con_showcredits = Sys.con_silentmode = Sys.stuffcmdsrun = false;

	Sys_LookupInstance();		// init launcher
	Con_CreateConsole();

	// first text message into console or log 
	MsgDev( D_NOTE, "Sys_LoadLibrary: Loading launch.dll - ok\n" );

	if( com.strlen( Sys.fmessage ) && !Sys.con_showcredits )
	{
		Sys_Print( Sys.fmessage );
		Sys.fmessage[0] = '\0';
	}

	Memory_Init();
	Sys_InitCPU();
	Cmd_Init();
	Cvar_Init();
	FS_Init();
	Image_Init();
	Sys_CreateInstance();
}

void Sys_Shutdown( void )
{
	// prepare host to close
	Sys.Free();
	Sys_FreeLibrary( Sys.linked_dll );
	Sys.CPrint = NullPrint;
	Sys_FreeLibrary( &baserc_dll );

	FS_Shutdown();
	Image_Shutdown();
	Memory_Shutdown();
	Con_DestroyConsole();
}

/*
================
Sys_Exit

NOTE: we must prepare engine to shutdown
before call this
================
*/
void Sys_Exit( void )
{
	if( Sys.shutdown_issued ) return;
	Sys.shutdown_issued = true;

	Sys_Shutdown();
	exit( Sys.error );
}

//=======================================================================
//			DLL'S MANAGER SYSTEM
//=======================================================================
bool Sys_LoadLibrary ( dll_info_t *dll )
{
	const dllfunc_t	*func;
	bool		native_lib = false;
	string		errorstring;

	// check errors
	if(!dll) return false;	// invalid desc
	if(!dll->name) return false;	// nothing to load
	if(dll->link) return true;	// already loaded

	MsgDev(D_NOTE, "Sys_LoadLibrary: Loading %s", dll->name );

	if(dll->fcts) 
	{
		// lookup export table
		for (func = dll->fcts; func && func->name != NULL; func++)
			*func->func = NULL;
	}
	else if( dll->entry) native_lib = true;

	if(!dll->link) dll->link = LoadLibrary ( va("bin/%s", dll->name));
	if(!dll->link) dll->link = LoadLibrary ( dll->name ); // environment pathes

	// No DLL found
	if (!dll->link) 
	{
		com_sprintf(errorstring, "Sys_LoadLibrary: couldn't load %s\n", dll->name );
		goto error;
	}

	if(native_lib)
	{
		if((dll->main = Sys_GetProcAddress(dll, dll->entry )) == 0)
		{
			com_sprintf(errorstring, "Sys_LoadLibrary: %s has no valid entry point\n", dll->name );
			goto error;
		}
	}
	else
	{
		// Get the function adresses
		for(func = dll->fcts; func && func->name != NULL; func++)
		{
			if (!(*func->func = Sys_GetProcAddress(dll, func->name)))
			{
				com_sprintf(errorstring, "Sys_LoadLibrary: %s missing or invalid function (%s)\n", dll->name, func->name );
				goto error;
			}
		}
	}

	if( native_lib )
	{
		generic_api_t *check = NULL;

		// NOTE: native dlls must support null import!
		// e.g. see ..\common\platform.c for details
		check = (void *)dll->main( &com, NULL ); // first iface always stdlib_api_t

		if(!check) 
		{
			com_sprintf(errorstring, "Sys_LoadLibrary: \"%s\" have no export\n", dll->name );
			goto error;
		}
		if(check->api_size != dll->api_size)
		{
			com_sprintf(errorstring, "Sys_LoadLibrary: \"%s\" mismatch interface size (%i should be %i)\n", dll->name, check->api_size, dll->api_size);
			goto error;
		}	
	}
          MsgDev(D_NOTE, " - ok\n");

	return true;
error:
	MsgDev(D_NOTE, " - failed\n");
	Sys_FreeLibrary ( dll ); // trying to free 
	if(dll->crash) Sys_Error( errorstring );
	else MsgDev( D_ERROR, errorstring );			

	return false;
}

void* Sys_GetProcAddress ( dll_info_t *dll, const char* name )
{
	if(!dll || !dll->link) // invalid desc
		return NULL;

	return (void *)GetProcAddress (dll->link, name);
}

bool Sys_FreeLibrary( dll_info_t *dll )
{
	// invalid desc or alredy freed
	if(!dll || !dll->link )
		return false;

	MsgDev(D_NOTE, "Sys_FreeLibrary: Unloading %s\n", dll->name );
	FreeLibrary( dll->link );
	dll->link = NULL;

	return true;
}

byte *Sys_LoadRes( const char *filename, size_t *size )
{
	if( baserc_dll.link )
		return rc->LoadFile( filename, size );

	if( size ) *size = 0;
	return NULL;
}

//=======================================================================
//			MULTITHREAD SYSTEM
//=======================================================================
#define MAX_THREADS		64

int	dispatch;
int	workcount;
int	oldf;
bool	pacifier;
bool	threaded;
void (*workfunction) (int);
int numthreads = -1;
CRITICAL_SECTION crit;
static int enter;

int Sys_GetNumThreads( void )
{
	return numthreads;
}

void Sys_ThreadLock( void )
{
	if( !threaded ) return;
	EnterCriticalSection( &crit );
	if( enter ) Sys_Error( "Recursive Sys_ThreadLock\n" ); 
	enter = true;
}

void Sys_ThreadUnlock( void )
{
	if( !threaded ) return;
	if( !enter ) Sys_Error( "Sys_ThreadUnlock without lock\n" ); 
	enter = false;
	LeaveCriticalSection( &crit );
}

int Sys_GetThreadWork( void )
{
	int	r, f;

	Sys_ThreadLock();

	if( dispatch == workcount )
	{
		Sys_ThreadUnlock();
		return -1;
	}

	f = 10 * dispatch / workcount;
	if( f != oldf )
	{
		oldf = f;
		if( pacifier ) Msg( "%i...", f );
	}

	r = dispatch;
	dispatch++;
	Sys_ThreadUnlock ();

	return r;
}

void Sys_ThreadWorkerFunction( int threadnum )
{
	int		work;

	while( 1 )
	{
		work = Sys_GetThreadWork();
		if( work == -1 ) break;
		workfunction( work );
	}
}

void Sys_ThreadSetDefault( void )
{
	if( numthreads == -1 ) // not set manually
	{
		// NOTE: we must init Plat_InitCPU() first
		numthreads = GI.cpunum;
		if( numthreads < 1 || numthreads > MAX_THREADS )
			numthreads = 1;
	}
}

void Sys_RunThreadsOnIndividual( int workcnt, bool showpacifier, void(*func)(int))
{
	if (numthreads == -1) Sys_ThreadSetDefault();
	workfunction = func;
	Sys_RunThreadsOn (workcnt, showpacifier, Sys_ThreadWorkerFunction);
}

/*
=============
Sys_RunThreadsOn
=============
*/
void Sys_RunThreadsOn( int workcnt, bool showpacifier, void(*func)(int))
{
	int	i, threadid[MAX_THREADS];
	HANDLE	threadhandle[MAX_THREADS];
	double	start, end;

	start = Sys_DoubleTime();
	dispatch = 0;
	workcount = workcnt;
	oldf = -1;
	pacifier = showpacifier;
	threaded = true;

	// run threads in parallel
	InitializeCriticalSection(&crit);

	if (numthreads == 1) func(0); // use same thread
	else
	{
		for (i = 0; i < numthreads; i++)
		{
			threadhandle[i] = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)func, (LPVOID)i, 0, &threadid[i]);
		}
		for (i = 0; i < numthreads; i++)
		{
			WaitForSingleObject(threadhandle[i], INFINITE);
		}
	}
	DeleteCriticalSection(&crit);

	threaded = false;
	end = Sys_DoubleTime();
	if( pacifier ) Msg(" Done [%.2f sec]\n", end - start);
}

/*
================
Sys_QueEvent

A time of 0 will get the current time
Ptr should either be null, or point to a block of data that can
be freed by the game later.
================
*/
void Sys_QueEvent( ev_type_t type, int value, int value2, int length, void *ptr )
{
	sys_event_t	*ev;

	ev = &event_que[event_head & MASK_QUED_EVENTS];
	if( event_head - event_tail >= MAX_QUED_EVENTS )
	{
		MsgDev( D_ERROR, "Sys_QueEvent: overflow\n");
		// make sure what memory is allocated by engine
		if(Mem_IsAllocated( ev->data )) Mem_Free( ev->data );
		event_tail++;
	}
	event_head++;

	ev->type = type;
	ev->value[0] = value;
	ev->value[1] = value2;
	ev->length = length;
	ev->data = ptr;
}

/*
================
Sys_GetEvent

================
*/
sys_event_t Sys_GetEvent( void )
{
	MSG		msg;
	sys_event_t	ev;
	char		*s;
	
	// return if we have data
	if( event_head > event_tail )
	{
		event_tail++;
		return event_que[(event_tail-1) & MASK_QUED_EVENTS];
	}

	// pump the message loop
	while( PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ))
	{
		if(!GetMessage( &msg, NULL, 0, 0 ))
		{
			Sys.error = true;
			Sys_Exit();
		}
		TranslateMessage(&msg );
      		DispatchMessage( &msg );
	}

	// check for console commands
	s = Sys_Input();
	if( s )
	{
		char	*b;
		int	len;

		len = com_strlen( s ) + 1;
		b = Malloc( len );
		com_strncpy( b, s, len - 1 );
		Sys_QueEvent( SE_CONSOLE, 0, 0, len, b );
	}

	// return if we have data
	if( event_head > event_tail )
	{
		event_tail++;
		return event_que[(event_tail - 1) & MASK_QUED_EVENTS];
	}

	// create an empty event to return
	Mem_Set( &ev, 0, sizeof( ev ));

	return ev;
}

bool Sys_GetModuleName( char *buffer, size_t length )
{
	if( Sys.ModuleName[0] == '\0' )
		return false;

	com.strncpy( buffer, Sys.ModuleName, length + 1 );
	return true;
}

/*
================
Sys_NewInstance

restarted engine with new instance
e.g. for change game or fallback to dedicated mode
================
*/
void Sys_NewInstance( const char *name, const char *fmsg )
{
	// save parms
	com_strncpy( Sys.ModuleName, name, sizeof( Sys.ModuleName ));
	com_strncpy( Sys.fmessage, fmsg, sizeof( Sys.fmessage ));
	Sys.app_state = SYS_RESTART;	// set right state
	Sys_Shutdown();		// shutdown current instance

	// NOTE: we never return to old instance,
	// because Sys_Exit call exit(0); and terminate program
	Sys_Init();
	Sys.Main();
	Sys_Exit();
}

//=======================================================================
//			REGISTRY COMMON TOOLS
//=======================================================================
bool REG_GetValue( HKEY hKey, const char *SubKey, const char *Value, char *pBuffer)
{
	dword dwBufLen = 4096;
	long lRet;

	if(lRet = RegOpenKeyEx( hKey, SubKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS )
  		return false;
	else
	{
		lRet = RegQueryValueEx( hKey, Value, NULL, NULL, (byte *)pBuffer, &dwBufLen);
		if(lRet != ERROR_SUCCESS) return false;
		RegCloseKey( hKey );
	}
	return true;
}

bool REG_SetValue( HKEY hKey, const char *SubKey, const char *Value, char *pBuffer )
{
	dword dwBufLen = 4096;
	long lRet;
	
	if(lRet = RegOpenKeyEx(hKey, SubKey, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
		return false;
	else
	{
		lRet = RegSetValueEx(hKey, Value, 0, REG_SZ, (byte *)pBuffer, dwBufLen );
		if(lRet != ERROR_SUCCESS) return false;
		RegCloseKey(hKey);
	}
	return true;	
}