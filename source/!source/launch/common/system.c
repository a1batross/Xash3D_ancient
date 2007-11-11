//=======================================================================
//			Copyright XashXT Group 2007 �
//			utils.c - shared launcher utils
//=======================================================================

#include <time.h>
#include "launch.h"
#include "basemath.h"

system_t	sys;
FILE	*logfile;

dll_info_t common_dll = { "common.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(common_exp_t) };
dll_info_t engine_dll = { "engine.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(launch_exp_t) };
dll_info_t editor_dll = { "editor.dll", NULL, "CreateAPI", NULL, NULL, true, sizeof(launch_exp_t) };

void NullInit ( char *funcname, int argc, char **argv )
{
}

void NullFunc( void )
{
}

gameinfo_t Sys_GameInfo( void )
{
	return GI;
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
	// lookup all instances
	if(!com_strcmp(sys.progname, "host_shared"))
	{
		sys.app_name = HOST_SHARED;
		sys.con_readonly = true;
		//don't show console as default
		if(!sys.debug) sys.con_showalways = false;
		sys.linked_dll = &engine_dll;	// pointer to engine.dll info
		com_strcpy(sys.log_path, "engine.log" ); // xash3d root directory
		com_strcpy(sys.caption, va("Xash3D ver.%g", XASH_VERSION ));
	}
	else if(!com_strcmp(sys.progname, "host_dedicated"))
	{
		sys.app_name = HOST_DEDICATED;
		sys.con_readonly = false;
		sys.linked_dll = &engine_dll;	// pointer to engine.dll info
		com_strcpy(sys.log_path, "engine.log" ); // xash3d root directory
		com_strcpy(sys.caption, va("Xash3D Dedicated Server ver.%g", XASH_VERSION ));
	}
	else if(!com_strcmp(sys.progname, "host_editor"))
	{
		sys.app_name = HOST_EDITOR;
		sys.con_readonly = true;
		//don't show console as default
		if(!sys.debug) sys.con_showalways = false;
		sys.linked_dll = &editor_dll;	// pointer to editor.dll info
		com_strcpy(sys.log_path, "editor.log" ); // xash3d root directory
		com_strcpy(sys.caption, va("Xash3D Editor ver.%g", XASH_VERSION ));
	}
	else if(!com_strcmp(sys.progname, "bsplib"))
	{
		sys.app_name = BSPLIB;
		sys.linked_dll = &common_dll;	// pointer to common.dll info
		com_strcpy(sys.log_path, "bsplib.log" ); // xash3d root directory
		com_strcpy(sys.caption, "Xash3D BSP Compiler");
	}
	else if(!com_strcmp(sys.progname, "imglib"))
	{
		sys.app_name = IMGLIB;
		sys.linked_dll = &common_dll;	// pointer to common.dll info
		com_sprintf(sys.log_path, "%s/convert.log", sys_rootdir ); // same as .exe file
		com_strcpy(sys.caption, "Xash3D Image Converter");
	}
	else if(!com_strcmp(sys.progname, "qcclib"))
	{
		sys.app_name = QCCLIB;
		sys.linked_dll = &common_dll;	// pointer to common.dll info
		com_sprintf(sys.log_path, "%s/compile.log", sys_rootdir ); // same as .exe file
		com_strcpy(sys.caption, "Xash3D QuakeC Compiler");
	}
	else if(!com_strcmp(sys.progname, "roqlib"))
	{
		sys.app_name = ROQLIB;
		sys.linked_dll = &common_dll;	// pointer to common.dll info
		com_sprintf(sys.log_path, "%s/roq.log", sys_rootdir ); // same as .exe file
		com_strcpy(sys.caption, "Xash3D ROQ Video Maker");
	}
	else if(!com_strcmp(sys.progname, "sprite"))
	{
		sys.app_name = SPRITE;
		sys.linked_dll = &common_dll;	// pointer to common.dll info
		com_sprintf(sys.log_path, "%s/spritegen.log", sys_rootdir ); // same as .exe file
		com_strcpy(sys.caption, "Xash3D Sprite Compiler");
	}
	else if(!com_strcmp(sys.progname, "studio"))
	{
		sys.app_name = STUDIO;
		sys.linked_dll = &common_dll;	// pointer to common.dll info
		com_sprintf(sys.log_path, "%s/studiomdl.log", sys_rootdir ); // same as .exe file
		com_strcpy(sys.caption, "Xash3D Studio Models Compiler");
	}
	else if(!com_strcmp(sys.progname, "credits")) // easter egg
	{
		sys.app_name = CREDITS;
		sys.linked_dll = NULL; // no need to loading library
		sys.log_active = sys.developer = sys.debug = 0; // clear all dbg states
		com_strcpy(sys.caption, "About");
		sys.con_showcredits = true;
	}
	else if(!com_strcmp(sys.progname, "host_setup")) // write path into registry
	{
		sys.app_name =  HOST_INSTALL;
		sys.linked_dll = NULL;	// no need to loading library
		sys.log_active = sys.developer = sys.debug = 0; //clear all dbg states
		sys.con_silentmode = true;
	}
	else 
	{
		sys.app_name = DEFAULT;
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
	if(!sys.log_active || !com_strlen(sys.log_path) || sys.con_silentmode) return;
	logfile = fopen( sys.log_path, "w");
	if(!logfile) Sys_Error("Sys_InitLog: can't create log file %s\n", sys.log_path );

	fprintf(logfile, "=======================================================================\n" );
	fprintf(logfile, "\t%s started at %s\n", sys.caption, com_timestamp(TIME_FULL));
	fprintf(logfile, "=======================================================================\n");
}

void Sys_CloseLog( void )
{
	if(!logfile) return;

	fprintf(logfile, "\n");
	fprintf(logfile, "=======================================================================");
	fprintf(logfile, "\n\t%s stopped at %s\n", sys.caption, com_timestamp(TIME_FULL));
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

	if(sys.con_silentmode) return;

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
	if(sys.Con_Print) sys.Con_Print( buffer );
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
	
	if(sys.developer < level) return;

	va_start (argptr, pMsg);
	com_vsprintf (text, pMsg, argptr);
	va_end (argptr);
	Sys_Print( text );
}

void Sys_MsgWarn( const char *pMsg, ... )
{
	va_list	argptr;
	char	text[MAX_INPUTLINE];
	
	if(!sys.debug) return;

	va_start (argptr, pMsg);
	com_vsprintf (text, pMsg, argptr);
	va_end (argptr);
	Sys_Print( text );
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

	if(sys.hooked_out)
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
void Sys_Error(char *error, ...)
{
	va_list		argptr;
	char		text[MAX_INPUTLINE];
         
	if(sys.error) return; // don't multiple executes
	
	va_start (argptr, error);
	com_vsprintf (text, error, argptr);
	va_end (argptr);
         
	sys.error = true;
	
	Con_ShowConsole( true );
	Sys_Print( text ); // print error message

	Sys_WaitForQuit();
	Sys_Exit();
}


long _stdcall Sys_ExecptionFilter( PEXCEPTION_POINTERS pExceptionInfo )
{
	// save config
	Sys_Print("Engine crashed\n");
	sys.Free(); // prepare host to close
	Sys_FreeLibrary( sys.linked_dll );
	Con_DestroyConsole();	

	if( sys.oldFilter ) return sys.oldFilter( pExceptionInfo );

#if 1
	return EXCEPTION_CONTINUE_SEARCH;
#else
	return EXCEPTION_CONTINUE_EXECUTION;
#endif
}

/*
================
Sys_FatalError

called while internal debugging tools 
are failed to initialize.
use generic msgbox
================
*/
void _Sys_ErrorFatal( int type, const char *filename, int fileline )
{
	char errorstring[64];

	switch( type )
	{
		case ERR_INVALID_ROOT:
			com_strncpy(errorstring, "Invalid root directory!", sizeof(errorstring));
			break;
		case ERR_CONSOLE_FAIL:
			com_strncpy(errorstring, "Can't create console window", sizeof(errorstring));
			break;
		case ERR_OSINFO_FAIL:
			com_strncpy(errorstring, "Couldn't get OS info", sizeof(errorstring));
			break;
		case ERR_INVALID_VER:
			com_strncpy(errorstring, "Requries Win95 or later", sizeof(errorstring));
			break;
		case ERR_WINDOWS_32S:
			com_strncpy(errorstring, "Win32s is not supported", sizeof(errorstring));
			break;
		default:
			com_sprintf(errorstring, "Internal engine error at %s:%i", filename, fileline );
			break;
	}
	MessageBox( 0, errorstring, "Error", MB_OK );
	exit( 1 );
}

void Sys_Init( void )
{
	HANDLE		hStdout;
	OSVERSIONINFO	vinfo;
	MEMORYSTATUS	lpBuffer;
	char		dev_level[4];

	lpBuffer.dwLength = sizeof(MEMORYSTATUS);
	vinfo.dwOSVersionInfoSize = sizeof(vinfo);
//oldFilter = SetUnhandledExceptionFilter( Sys_ExecptionFilter );
	GlobalMemoryStatus (&lpBuffer);

	sys.hInstance = (HINSTANCE)GetModuleHandle( NULL ); // get current hInstance first
	hStdout = GetStdHandle (STD_OUTPUT_HANDLE); // check for hooked out

	if(!GetVersionEx (&vinfo)) Sys_ErrorFatal(ERR_OSINFO_FAIL);
	if(vinfo.dwMajorVersion < 4) Sys_ErrorFatal(ERR_INVALID_VER);
	if(vinfo.dwPlatformId == VER_PLATFORM_WIN32s) Sys_ErrorFatal(ERR_WINDOWS_32S);

	sys.Init = NullInit;
	sys.Main = NullFunc;
	sys.Free = NullFunc;

	// parse and copy args into local array
	Sys_ParseCommandLine(GetCommandLine());

	if(FS_CheckParm ("-debug")) sys.debug = true;
	if(FS_CheckParm ("-log")) sys.log_active = true;
	if(FS_GetParmFromCmdLine("-dev", dev_level )) sys.developer = com_atoi(dev_level);

	// ugly hack to get pipeline state, but it works
	if(abs((short)hStdout) < 100) sys.hooked_out = false;
	else sys.hooked_out = true;
	FS_UpdateEnvironmentVariables(); // set working directory

	sys.con_showalways = true;
	sys.con_readonly = true;
	sys.con_showcredits = false;
	sys.con_silentmode = false;

	Sys_InitCPU();
	Sys_LookupInstance(); // init launcher
	Con_CreateConsole();

	Memory_Init();
	FS_Init();
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
	sys.Free();
	Sys_FreeLibrary( sys.linked_dll );

	Con_DestroyConsole();	
	FS_Shutdown();
	Memory_Shutdown();

	if( sys.oldFilter )  // restore filter	
		SetUnhandledExceptionFilter( sys.oldFilter );
	exit( sys.error );
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

	MsgDev(D_ERROR, "Sys_LoadLibrary: Loading %s", dll->name );

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
		check = (void *)dll->main( NULL );

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
          MsgDev(D_ERROR, " - ok\n");

	return true;
error:
	MsgDev(D_ERROR, " - failed\n");
	Sys_FreeLibrary ( dll ); // trying to free 
	if(dll->crash) Sys_Error( errorstring );
	else MsgDev( D_INFO, errorstring );			

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

	MsgDev(D_ERROR, "Sys_FreeLibrary: Unloading %s\n", dll->name );
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
	EnterCriticalSection (&crit);
	if (enter) Sys_Error ("Recursive ThreadLock\n"); 
	enter = 1;
}

void Sys_ThreadUnlock( void )
{
	if (!threaded) return;
	if (!enter) Sys_Error ("ThreadUnlock without lock\n"); 
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