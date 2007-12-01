//=======================================================================
//			Copyright XashXT Group 2007 ©
//			utils.c - shared launcher utils
//=======================================================================

#include "launch.h"
#include "mathlib.h"

system_t		Sys;
stdlib_api_t	com;
launch_exp_t	*Host;	// callback to mainframe 
FILE		*logfile;

dll_info_t common_dll = { "common.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(launch_exp_t) };
dll_info_t engine_dll = { "engine.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(launch_exp_t) };
dll_info_t editor_dll = { "editor.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(launch_exp_t) };

static const char *show_credits = "\n\n\n\n\tCopyright XashXT Group 2007 ©\n\t\
          All Rights Reserved\n\n\t           Visit www.xash.ru\n";

// stubs
void NullInit( uint funcname, int argc, char **argv ) {}
void NullFunc( void ) {}
void Sys_NullPrint( const char *msg ) {}

void Sys_GetStdAPI( void )
{
	// interface validator
	com.api_size = sizeof(stdlib_api_t);

	// base events
	com.printf = Sys_Msg;
	com.dprintf = Sys_MsgDev;
	com.wprintf = Sys_MsgWarn;
	com.error = Sys_Error;
	com.abort = Sys_Break;
	com.exit = Sys_Exit;
	com.print = Sys_Print;
	com.input = Sys_Input;
	com.sleep = Sys_Sleep;
	com.clipboard = Sys_GetClipboardData;
	com.keyevents = Sys_SendKeyEvents;

	// crclib.c funcs
	com.crc_init = CRC_Init;
	com.crc_block = CRC_Block;
	com.crc_process = CRC_ProcessByte;
	com.crc_sequence = CRC_BlockSequence;
	com.crc_blockchecksum = Com_BlockChecksum;
	com.crc_blockchecksumkey = Com_BlockChecksumKey;

	// memlib.c
	com.memcpy = _mem_copy;
	com.memset = _mem_set;
	com.realloc = _mem_realloc;
	com.move = _mem_move;
	com.malloc = _mem_alloc;
	com.free = _mem_free;
	com.mallocpool = _mem_allocpool;
	com.freepool = _mem_freepool;
	com.clearpool = _mem_emptypool;
	com.memcheck = _mem_check;

	// common functions
	com.Com_InitRootDir = FS_InitRootDir;		// init custom rootdir 
	com.Com_LoadGameInfo = FS_LoadGameInfo;		// gate game info from script file
	com.Com_AddGameHierarchy = FS_AddGameHierarchy;	// add base directory in search list
	com.Com_CheckParm = FS_CheckParm;		// get parm from cmdline
	com.Com_GetParm = FS_GetParmFromCmdLine;	// get filename without path & ext
	com.Com_FileBase = FS_FileBase;		// get filename without path & ext
	com.Com_FileExists = FS_FileExists;		// return true if file exist
	com.Com_FileSize = FS_FileSize;		// same as Com_FileExists but return filesize
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
	com.Com_LoadScript = SC_LoadScript;		// load script into stack from file or bufer
	com.Com_AddScript = SC_AddScript;		// include script from file or buffer
	com.Com_ResetScript = SC_ResetScript;		// reset current script state
	com.Com_ReadToken = SC_GetToken;		// get next token on a line or newline
	com.Com_TryToken = SC_TryToken;		// return 1 if have token on a line 
	com.Com_FreeToken = SC_FreeToken;		// free current token to may get it again
	com.Com_SkipToken = SC_SkipToken;		// skip current token and jump into newline
	com.Com_MatchToken = SC_MatchToken;		// compare current token with user keyword
	com.Com_ParseToken = SC_ParseToken;		// parse token from char buffer
	com.Com_ParseWord = SC_ParseWord;		// parse word from char buffer
	com.Com_Search = FS_Search;			// returned list of found files
	com.Com_Filter = SC_FilterToken;		// compare keyword by mask with filter
	com.com_token = token;			// contains current token

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
	com.fgets = FS_Gets;		// like a fgets, but can return EOF
	com.fseek = FS_Seek;		// fseek, can seek in packfiles too
	com.ftell = FS_Tell;		// like a ftell

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
	com.vftell = VFS_Tell;		// like a ftell

	// filesystem simply user interface
	com.Com_LoadFile = FS_LoadFile;		// load file into heap
	com.Com_WriteFile = FS_WriteFile;		// write file into disk
	com.Com_LoadImage = FS_LoadImage;		// extract image into rgba buffer
	com.Com_SaveImage = FS_SaveImage;		// save image into specified format
	com.Com_ProcessImage = Image_Processing;	// convert and resample image
	com.Com_FreeImage = FS_FreeImage;		// free image buffer
	com.Com_LoadLibrary = Sys_LoadLibrary;		// load library 
	com.Com_FreeLibrary = Sys_FreeLibrary;		// free library
	com.Com_GetProcAddress = Sys_GetProcAddress;	// gpa
	com.Com_DoubleTime = Sys_DoubleTime;		// hi-res timer

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
	com.strstr = com_stristr;		// FIXME
	com.strpack = com_strpack;
	com.strunpack = com_strunpack;
	com.vsprintf = com_vsprintf;
	com.sprintf = com_sprintf;
	com.va = va;
	com.vsnprintf = com_vsnprintf;
	com.snprintf = com_snprintf;
	com.timestamp = com_timestamp;

	com.GameInfo = &GI;
}

/*
==================
Parse program name to launch and determine work style

NOTE: at this day we have eleven instances

1. "host_shared" - normal game launch
2. "host_dedicated" - dedicated server
3. "host_editor" - resource editor
4. "bsplib" - three BSP compilers in one
5. "imglib" - convert old formats (mip, pcx, lmp) to 32-bit tga
6. "qcclib" - quake c complier
7. "roqlib" - roq video file maker
8. "sprite" - sprite creator (requires qc. script)
9. "studio" - Half-Life style models creator (requires qc. script) 
10. "credits" - display credits of engine developers
11. "host_setup" - write current path into registry (silently)

This list will be expnaded in future
==================
*/
void Sys_LookupInstance( void )
{
	Sys.app_name = HOST_OFFLINE;

	// lookup all instances
	if(!com_strcmp(Sys.progname, "host_shared"))
	{
		Sys.app_name = HOST_NORMAL;
		Sys.con_readonly = true;
		// don't show console as default
		if(!Sys.debug) Sys.con_showalways = false;
		Sys.linked_dll = &engine_dll;	// pointer to engine.dll info
		com_sprintf(Sys.log_path, "engine.log", com_timestamp(TIME_NO_SECONDS)); // logs folder
		com_strcpy(Sys.caption, va("Xash3D ver.%g", XASH_VERSION ));
	}
	else if(!com_strcmp(Sys.progname, "host_dedicated"))
	{
		Sys.app_name = HOST_DEDICATED;
		Sys.con_readonly = false;
		Sys.linked_dll = &engine_dll;	// pointer to engine.dll info
		com_sprintf(Sys.log_path, "engine.log", com_timestamp(TIME_NO_SECONDS)); // logs folder
		com_strcpy(Sys.caption, va("Xash3D ver.%g", XASH_VERSION ));
	}
	else if(!com_strcmp(Sys.progname, "host_editor"))
	{
		Sys.app_name = HOST_EDITOR;
		Sys.con_readonly = true;
		//don't show console as default
		if(!Sys.debug) Sys.con_showalways = false;
		Sys.linked_dll = &editor_dll;	// pointer to editor.dll info
		com_strcpy(Sys.log_path, "editor.log" ); // xash3d root directory
		com_strcpy(Sys.caption, va("Xash3D Editor ver.%g", XASH_VERSION ));
	}
	else if(!com_strcmp(Sys.progname, "bsplib"))
	{
		Sys.app_name = BSPLIB;
		Sys.linked_dll = &common_dll;	// pointer to common.dll info
		com_strcpy(Sys.log_path, "bsplib.log" ); // xash3d root directory
		com_strcpy(Sys.caption, "Xash3D BSP Compiler");
	}
	else if(!com_strcmp(Sys.progname, "imglib"))
	{
		Sys.app_name = IMGLIB;
		Sys.linked_dll = &common_dll;	// pointer to common.dll info
		com_sprintf(Sys.log_path, "%s/convert.log", sys_rootdir ); // same as .exe file
		com_strcpy(Sys.caption, "Xash3D Image Converter");
	}
	else if(!com_strcmp(Sys.progname, "qcclib"))
	{
		Sys.app_name = QCCLIB;
		Sys.linked_dll = &common_dll;	// pointer to common.dll info
		com_sprintf(Sys.log_path, "%s/compile.log", sys_rootdir ); // same as .exe file
		com_strcpy(Sys.caption, "Xash3D QuakeC Compiler");
	}
	else if(!com_strcmp(Sys.progname, "roqlib"))
	{
		Sys.app_name = ROQLIB;
		Sys.linked_dll = &common_dll;	// pointer to common.dll info
		com_sprintf(Sys.log_path, "%s/roq.log", sys_rootdir ); // same as .exe file
		com_strcpy(Sys.caption, "Xash3D ROQ Video Maker");
	}
	else if(!com_strcmp(Sys.progname, "sprite"))
	{
		Sys.app_name = SPRITE;
		Sys.linked_dll = &common_dll;	// pointer to common.dll info
		com_sprintf(Sys.log_path, "%s/spritegen.log", sys_rootdir ); // same as .exe file
		com_strcpy(Sys.caption, "Xash3D Sprite Compiler");
	}
	else if(!com_strcmp(Sys.progname, "studio"))
	{
		Sys.app_name = STUDIO;
		Sys.linked_dll = &common_dll;	// pointer to common.dll info
		com_sprintf(Sys.log_path, "%s/studiomdl.log", sys_rootdir ); // same as .exe file
		com_strcpy(Sys.caption, "Xash3D Studio Models Compiler");
	}
	else if(!com_strcmp(Sys.progname, "paklib"))
	{
		Sys.app_name = PAKLIB;
		Sys.linked_dll = &common_dll;	// pointer to common.dll info
		com_sprintf(Sys.log_path, "%s/paklib.log", sys_rootdir ); // same as .exe file
		com_strcpy(Sys.caption, "Xash3D Pak\\Pk3 maker");
	}
	else if(!com_strcmp(Sys.progname, "credits")) // easter egg
	{
		Sys.app_name = CREDITS;
		Sys.linked_dll = NULL; // no need to loading library
		Sys.log_active = Sys.developer = Sys.debug = 0; // clear all dbg states
		com_strcpy(Sys.caption, "About");
		Sys.con_showcredits = true;
	}
	else if(!com_strcmp(Sys.progname, "host_setup")) // write path into registry
	{
		Sys.app_name =  HOST_INSTALL;
		Sys.linked_dll = NULL;	// no need to loading library
		Sys.log_active = Sys.developer = Sys.debug = 0; //clear all dbg states
		Sys.con_silentmode = true;
	}
}

/*
==================
Find needed library, setup and run it
==================
*/
void Sys_CreateInstance( void )
{
	// export
	launch_t		CreateHost;

	Sys_LoadLibrary( Sys.linked_dll ); // loading library if need

	// pre initializations
	switch(Sys.app_name)
	{
	case HOST_NORMAL:
	case HOST_DEDICATED:
	case HOST_EDITOR:		
	case BSPLIB:
	case QCCLIB:
	case ROQLIB:
	case IMGLIB:
	case SPRITE:
	case STUDIO:
	case PAKLIB:
		CreateHost = (void *)Sys.linked_dll->main;
		Host = CreateHost( &com, NULL ); // second interface not allowed
		Sys.Init = Host->Init;
		Sys.Main = Host->Main;
		Sys.Free = Host->Free;
		Sys.CPrint = Host->CPrint;
		Sys.Cmd = Host->Cmd;
		break;
	case CREDITS:
		Sys_Break( show_credits );
		break;
	case HOST_INSTALL:
		// FS_UpdateEnvironmentVariables() is done, quit now
		Sys_Exit();
		break;
	case HOST_OFFLINE:
		Sys_Break("Host offline\n Press \"ESC\" to exit\n");		
		break;
	}

	// init our host now!
	Sys.Init( Sys.app_name, fs_argc, fs_argv );

	// post initializations
	switch(Sys.app_name)
	{
	case HOST_NORMAL:
		Con_ShowConsole( false );		// hide console
	case HOST_DEDICATED:
		Cbuf_AddText("exec init.rc\n");	// execute startup config and cmdline
		Cbuf_Execute();
		// if stuffcmds wasn't run, then init.rc is probably missing, use default
		if(!Sys.stuffcmdsrun) Cbuf_ExecuteText( EXEC_NOW, "stuffcmds\n" );
		break;
	case HOST_EDITOR:
		Con_ShowConsole( false );
	case BSPLIB:
	case QCCLIB:
	case ROQLIB:
	case IMGLIB:
	case SPRITE:
	case STUDIO:
	case PAKLIB:
		// always run stuffcmds for current instances
		Cbuf_ExecuteText( EXEC_NOW, "stuffcmds\n" );
		break;
	}
}

uint Sys_SendKeyEvents( void )
{
	MSG	msg;
	int	msg_time;

	while (PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		if(!GetMessage (&msg, NULL, 0, 0)) break;
		msg_time = msg.time;
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}
	return msg.time;
}

/*
==================
Sys_ParseCommandLine

==================
*/
void Sys_ParseCommandLine (LPSTR lpCmdLine)
{
	fs_argc = 1;
	fs_argv[0] = "exe";

	while (*lpCmdLine && (fs_argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && *lpCmdLine <= ' ') lpCmdLine++;
		if (!*lpCmdLine) break;

		if (*lpCmdLine == '\"')
		{
			// quoted string
			lpCmdLine++;
			fs_argv[fs_argc] = lpCmdLine;
			fs_argc++;
			while (*lpCmdLine && (*lpCmdLine != '\"')) lpCmdLine++;
		}
		else
		{
			// unquoted word
			fs_argv[fs_argc] = lpCmdLine;
			fs_argc++;
			while (*lpCmdLine && *lpCmdLine > ' ') lpCmdLine++;
		}

		if (*lpCmdLine)
		{
			*lpCmdLine = 0;
			lpCmdLine++;
		}
	}

}

void Sys_InitLog( void )
{
	// create log if needed
	if(!Sys.log_active || !com_strlen(Sys.log_path) || Sys.con_silentmode) return;
	logfile = fopen( Sys.log_path, "w");
	if(!logfile) Sys_Error("Sys_InitLog: can't create log file %s\n", Sys.log_path );

	fprintf(logfile, "=======================================================================\n" );
	fprintf(logfile, "\t%s started at %s\n", Sys.caption, com_timestamp(TIME_FULL));
	fprintf(logfile, "=======================================================================\n");
}

void Sys_CloseLog( void )
{
	if(!logfile) return;

	fprintf(logfile, "\n");
	fprintf(logfile, "=======================================================================");
	fprintf(logfile, "\n\t%s %sed at %s\n", Sys.caption, Sys.crash ? "crash" : "stopp", com_timestamp(TIME_FULL));
	fprintf(logfile, "=======================================================================");

	fclose(logfile);
	logfile = NULL;
}

void Sys_PrintLog( const char *pMsg )
{
	if(!logfile) return;
	fprintf(logfile, "%s", pMsg );
}

/*
================
Sys_Print

print into window console
================
*/
void Sys_Print(const char *pMsg)
{
	const char	*msg;
	char		buffer[MAX_INPUTLINE * 2];
	char		logbuf[MAX_INPUTLINE * 2];
	char		*b = buffer;
	char		*c = logbuf;	
	int		i = 0;

	if(Sys.con_silentmode) return;
	if(Sys.CPrint) Sys.CPrint( pMsg );

	// if the message is REALLY long, use just the last portion of it
	if ( com_strlen( pMsg ) > MAX_INPUTLINE - 1 )
		msg = pMsg + com_strlen( pMsg ) - MAX_INPUTLINE + 1;
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

	Sys_PrintLog( logbuf );
	if(Sys.Con_Print) Sys.Con_Print( buffer );
}

/*
================
Sys_Msg

formatted message
================
*/
void Sys_Msg( const char *pMsg, ... )
{
	va_list		argptr;
	char text[MAX_INPUTLINE];
	
	va_start (argptr, pMsg);
	com_vsprintf (text, pMsg, argptr);
	va_end (argptr);

	Sys_Print( text );
}

void Sys_MsgDev( int level, const char *pMsg, ... )
{
	va_list	argptr;
	char	text[MAX_INPUTLINE];

	if(Sys.developer < level) return;

	va_start (argptr, pMsg);
	com_vsprintf (text, pMsg, argptr);
	va_end (argptr);

	switch(level)
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
		Sys_Print(va("^6Mem: ^7%s", text));
		break;
	}
}

void Sys_MsgWarn( const char *pMsg, ... )
{
	va_list	argptr;
	char	text[MAX_INPUTLINE];
	
	if(!Sys.debug) return;

	va_start (argptr, pMsg);
	com_vsprintf (text, pMsg, argptr);
	va_end (argptr);
	Sys_Print(va("^3Warning:^7 %s", text));
}

/*
================
Sys_DoubleTime
================
*/
double Sys_DoubleTime( void )
{
	static int first = true;
	static bool nohardware_timer = false;
	static double oldtime = 0.0, curtime = 0.0;
	double newtime;
	
	// LordHavoc: note to people modifying this code, 
	// DWORD is specifically defined as an unsigned 32bit number, 
	// therefore the 65536.0 * 65536.0 is fine.
	if (GI.cpunum > 1 || nohardware_timer)
	{
		static int firsttimegettime = true;
		// timeGetTime
		// platform:
		// Windows 95/98/ME/NT/2000/XP
		// features:
		// reasonable accuracy (millisecond)
		// issues:
		// wraps around every 47 days or so (but this is non-fatal to us, 
		// odd times are rejected, only causes a one frame stutter)

		// make sure the timer is high precision, otherwise different versions of
		// windows have varying accuracy
		if (firsttimegettime)
		{
			timeBeginPeriod (1);
			firsttimegettime = false;
		}

		newtime = (double)timeGetTime () * 0.001;
	}
	else
	{
		// QueryPerformanceCounter
		// platform:
		// Windows 95/98/ME/NT/2000/XP
		// features:
		// very accurate (CPU cycles)
		// known issues:
		// does not necessarily match realtime too well
		// (tends to get faster and faster in win98)
		// wraps around occasionally on some platforms
		// (depends on CPU speed and probably other unknown factors)
		double timescale;
		LARGE_INTEGER PerformanceFreq;
		LARGE_INTEGER PerformanceCount;

		if (!QueryPerformanceFrequency (&PerformanceFreq))
		{
			Msg("No hardware timer available\n");
			// fall back to timeGetTime
			nohardware_timer = true;
			return Sys_DoubleTime();
		}
		QueryPerformanceCounter (&PerformanceCount);

		timescale = 1.0 / ((double) PerformanceFreq.LowPart + (double) PerformanceFreq.HighPart * 65536.0 * 65536.0);
		newtime = ((double) PerformanceCount.LowPart + (double) PerformanceCount.HighPart * 65536.0 * 65536.0) * timescale;
	}

	if (first)
	{
		first = false;
		oldtime = newtime;
	}

	if (newtime < oldtime)
	{
		// warn if it's significant
		if (newtime - oldtime < -0.01)
			Msg("Plat_DoubleTime: time stepped backwards (went from %f to %f, difference %f)\n", oldtime, newtime, newtime - oldtime);
	}
	else curtime += newtime - oldtime;
	oldtime = newtime;

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
Sys_Sleep

freeze application for some time
================
*/
void Sys_Sleep( int msec)
{
	msec = bound(1, msec, 1000 );
	Sleep( msec );
}

void Sys_WaitForQuit( void )
{
	MSG		msg;

	if(Sys.hooked_out)
	{
		Sys_Print("press any key to quit\n");
		getchar(); // wait for quit
	}
	else
	{
		Con_RegisterHotkeys();		
		Mem_Set(&msg, 0, sizeof(msg));

		// wait for the user to quit
		while(msg.message != WM_QUIT)
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
	char		text[MAX_INPUTLINE];
         
	if(Sys.error) return; // don't multiple executes
	
	va_start (argptr, error);
	com_vsprintf (text, error, argptr);
	va_end (argptr);
         
	Sys.error = true;
	Con_ShowConsole( true );
	if(Sys.developer) Sys_Print( text );		// print error message
	else Sys_Print( "Internal engine error\n" );	// don't confuse non-developers with technique stuff

	Sys_WaitForQuit();
	Sys_Exit();
}

void Sys_Break(const char *error, ...)
{
	va_list		argptr;
	char		text[MAX_INPUTLINE];
         
	va_start (argptr, error);
	com_vsprintf (text, error, argptr);
	va_end (argptr);
	
	Sys.error = true;
	Con_ShowConsole( true );
	Sys_Print( text );
	Sys_WaitForQuit();
	Sys_Exit();
}

long _stdcall Sys_Crash( PEXCEPTION_POINTERS pInfo )
{
	// save config
	if(!Sys.crash)
	{
		// check to avoid recursive call
		Sys.crash = true;
		Sys.Free(); // prepare host to close
		Sys_FreeLibrary( Sys.linked_dll );

		if(Sys.developer >= D_MEMORY)
		{
			// show execption in native console too
			Con_ShowConsole( true );
			Msg("Sys_Crash: call %p at address %p\n", pInfo->ExceptionRecord->ExceptionCode, pInfo->ExceptionRecord->ExceptionAddress );
			Sys_WaitForQuit();
		}
		Con_DestroyConsole();	
          }

	if( Sys.oldFilter )
		return Sys.oldFilter( pInfo );
	return EXCEPTION_CONTINUE_SEARCH;
}

void Sys_Init( void )
{
	HANDLE		hStdout;
	MEMORYSTATUS	lpBuffer;
	char		dev_level[4];

	lpBuffer.dwLength = sizeof(MEMORYSTATUS);
	Sys.oldFilter = SetUnhandledExceptionFilter( Sys_Crash );
	GlobalMemoryStatus (&lpBuffer);

	Sys.hInstance = (HINSTANCE)GetModuleHandle( NULL ); // get current hInstance first
	hStdout = GetStdHandle (STD_OUTPUT_HANDLE); // check for hooked out

	Sys_GetStdAPI();
	Sys.Init = NullInit;
	Sys.Main = NullFunc;
	Sys.Free = NullFunc;

	// parse and copy args into local array
	Sys_ParseCommandLine(GetCommandLine());

	if(FS_CheckParm ("-debug")) Sys.debug = true;
	if(FS_CheckParm ("-log")) Sys.log_active = true;
	if(FS_GetParmFromCmdLine("-dev", dev_level )) Sys.developer = com_atoi(dev_level);

	// ugly hack to get pipeline state, but it works
	if(abs((short)hStdout) < 100) Sys.hooked_out = false;
	else Sys.hooked_out = true;
	FS_UpdateEnvironmentVariables(); // set working directory

	Sys.con_showalways = true;
	Sys.con_readonly = true;
	Sys.con_showcredits = false;
	Sys.con_silentmode = false;

	Sys_LookupInstance(); // init launcher
	Con_CreateConsole();
	Sys_InitCPU();
	Memory_Init();
	Cmd_Init();
	Cvar_Init();

	FS_Init();
	Sys_CreateInstance();
}

/*
================
Sys_Exit

NOTE: we must prepare engine to shutdown
before call this
================
*/
void Sys_Exit ( void )
{
	// prepare host to close
	Sys.Free();
	Sys_FreeLibrary( Sys.linked_dll );

	FS_Shutdown();
	Memory_Shutdown();
	Con_DestroyConsole();

	// restore filter	
	if( Sys.oldFilter )
		SetUnhandledExceptionFilter( Sys.oldFilter );
	exit( Sys.error );
}

//=======================================================================
//			DLL'S MANAGER SYSTEM
//=======================================================================
bool Sys_LoadLibrary ( dll_info_t *dll )
{
	const dllfunc_t	*func;
	bool		native_lib = false;
	char		errorstring[MAX_QPATH];

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

bool Sys_FreeLibrary ( dll_info_t *dll )
{
	if(!dll || !dll->link) // invalid desc or alredy freed
		return false;
	if(Sys.crash)
	{
		// we need to hold down all modules, while MSVC can find erorr
		MsgDev(D_NOTE, "Sys_FreeLibrary: Hold %s for debugging\n", dll->name );
		return false;
	}
	else MsgDev(D_NOTE, "Sys_FreeLibrary: Unloading %s\n", dll->name );

	FreeLibrary (dll->link);
	dll->link = NULL;

	return true;
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
	if (!threaded) return;
	EnterCriticalSection(&crit);
	if (enter) Sys_Error( "Recursive ThreadLock\n" ); 
	enter = 1;
}

void Sys_ThreadUnlock( void )
{
	if (!threaded) return;
	if (!enter) Sys_Error( "ThreadUnlock without lock\n" ); 
	enter = 0;
	LeaveCriticalSection (&crit);
}

int Sys_GetThreadWork( void )
{
	int	r, f;

	Sys_ThreadLock ();

	if (dispatch == workcount)
	{
		Sys_ThreadUnlock();
		return -1;
	}

	f = 10 * dispatch / workcount;
	if (f != oldf)
	{
		oldf = f;
		if (pacifier) Msg("%i...", f);
	}

	r = dispatch;
	dispatch++;
	Sys_ThreadUnlock ();

	return r;
}

void Sys_ThreadWorkerFunction (int threadnum)
{
	int		work;

	while (1)
	{
		work = Sys_GetThreadWork();
		if (work == -1) break;
		workfunction(work);
	}
}

void Sys_ThreadSetDefault (void)
{
	if(numthreads == -1) // not set manually
	{
		// NOTE: we must init Plat_InitCPU() first
		numthreads = GI.cpunum;
		if (numthreads < 1 || numthreads > MAX_THREADS)
			numthreads = 1;
	}
}

void Sys_RunThreadsOnIndividual(int workcnt, bool showpacifier, void(*func)(int))
{
	if (numthreads == -1) Sys_ThreadSetDefault ();
	workfunction = func;
	Sys_RunThreadsOn (workcnt, showpacifier, Sys_ThreadWorkerFunction);
}

/*
=============
Sys_RunThreadsOn
=============
*/
void Sys_RunThreadsOn (int workcnt, bool showpacifier, void(*func)(int))
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
	InitializeCriticalSection (&crit);

	if (numthreads == 1) func(0); // use same thread
	else
	{
		for (i = 0; i < numthreads; i++)
		{
			threadhandle[i] = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)func, (LPVOID)i, 0, &threadid[i]);
		}
		for (i = 0; i < numthreads; i++)
		{
			WaitForSingleObject (threadhandle[i], INFINITE);
		}
	}
	DeleteCriticalSection (&crit);

	threaded = false;
	end = Sys_DoubleTime();
	if (pacifier) Msg(" Done [%.2f sec]\n", end - start);
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