//=======================================================================
//			Copyright XashXT Group 2007 ©
//			utils.c - shared launcher utils
//=======================================================================

#include <time.h>
#include "launcher.h"
#include "basemath.h"

char sys_rootdir[ MAX_SYSPATH ];
bool debug_mode = false;
bool console_read_only = true;
int dev_mode = 0;

void Sys_SendKeyEvents( void )
{
	MSG	wmsg;

	while (PeekMessage (&wmsg, NULL, 0, 0, PM_NOREMOVE))
	{
		if (!GetMessage (&wmsg, NULL, 0, 0)) break;
		TranslateMessage (&wmsg);
		DispatchMessage (&wmsg);
	}
}

/*
====================
Log_Timestamp
====================
*/
const char* Log_Timestamp( void )
{
	static char timestamp [128];
	time_t crt_time;
	const struct tm *crt_tm;
	char timestring [64];

	// Build the time stamp (ex: "Apr03 2007 [23:31:55]");
	time (&crt_time);
	crt_tm = localtime (&crt_time);
	strftime (timestring, sizeof (timestring), "%b%d %Y [%H:%M:%S]", crt_tm);
          strcpy( timestamp, timestring );

	return timestamp;
}

float CalcEngineVersion( void )
{
	return LAUNCH_VERSION + COMMON_VERSION + RENDER_VERSION + ENGINE_VERSION;
}

float CalcEditorVersion( void )
{
	return LAUNCH_VERSION + COMMON_VERSION + RENDER_VERSION + EDITOR_VERSION;
}

/*
==================
ParseCommandLine

==================
*/
void ParseCommandLine (LPSTR lpCmdLine)
{
	com_argc = 1;
	com_argv[0] = "exe";

	while (*lpCmdLine && (com_argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && *lpCmdLine <= ' ') lpCmdLine++;
		if (!*lpCmdLine) break;

		if (*lpCmdLine == '\"')
		{
			// quoted string
			lpCmdLine++;
			com_argv[com_argc] = lpCmdLine;
			com_argc++;
			while (*lpCmdLine && (*lpCmdLine != '\"')) lpCmdLine++;
		}
		else
		{
			// unquoted word
			com_argv[com_argc] = lpCmdLine;
			com_argc++;
			while (*lpCmdLine && *lpCmdLine > ' ') lpCmdLine++;
		}

		if (*lpCmdLine)
		{
			*lpCmdLine = 0;
			lpCmdLine++;
		}
	}

}

/*
================
CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int CheckParm (const char *parm)
{
	int i;

	for (i = 1; i < com_argc; i++ )
	{
		// NEXTSTEP sometimes clears appkit vars.
		if (!com_argv[i]) continue;
		if (!strcmp (parm, com_argv[i])) return i;
	}
	return 0;
}

bool _GetParmFromCmdLine( char *parm, char *out, size_t size )
{
	int argc = CheckParm( parm );

	if(!argc) return false;
	if(!out) return false;	

	strncpy( out, com_argv[argc+1], size );
	return true;
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

void Sys_Init( void )
{
	HANDLE		hStdout;
	OSVERSIONINFO	vinfo;
	MEMORYSTATUS	lpBuffer;

	lpBuffer.dwLength = sizeof(MEMORYSTATUS);
	vinfo.dwOSVersionInfoSize = sizeof(vinfo);
	oldFilter = SetUnhandledExceptionFilter( Sys_ExecptionFilter );
	GlobalMemoryStatus (&lpBuffer);

	base_hInstance = (HINSTANCE)GetModuleHandle( NULL ); // get current hInstance first
	hStdout = GetStdHandle (STD_OUTPUT_HANDLE); // check for hooked out

	if(!GetVersionEx (&vinfo)) Sys_ErrorFatal(ERR_OSINFO_FAIL);
	if(vinfo.dwMajorVersion < 4) Sys_ErrorFatal(ERR_INVALID_VER);
	if(vinfo.dwPlatformId == VER_PLATFORM_WIN32s) Sys_ErrorFatal(ERR_WINDOWS_32S);

	// ugly hack to get pipeline state, but it works
	if(abs((short)hStdout) < 100) hooked_out = false;
	else hooked_out = true;

	// parse and copy args into local array
	ParseCommandLine(GetCommandLine());
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
	Host_Free();
	Sys_FreeLibrary( linked_dll );

	Sys_FreeConsole();	
	SetUnhandledExceptionFilter( oldFilter ); // restore filter
	exit(sys_error);
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
		sprintf(errorstring, "Sys_LoadLibrary: couldn't load %s\n", dll->name );
		goto error;
	}

	if(native_lib)
	{
		if((dll->main = Sys_GetProcAddress(dll, dll->entry )) == 0)
		{
			sprintf(errorstring, "Sys_LoadLibrary: %s has no valid entry point\n", dll->name );
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
				sprintf(errorstring, "Sys_LoadLibrary: %s missing or invalid function (%s)\n", dll->name, func->name );
				goto error;
			}
		}
	}

	if( native_lib )
	{
		generic_api_t *check = NULL;

		// NOTE: native dlls must support null import!
		// e.g. see platform.c for details
		check = (void *)dll->main( NULL );

		if(!check) 
		{
			sprintf(errorstring, "Sys_LoadLibrary: \"%s\" have no export\n", dll->name );
			goto error;
		}
		if(check->apiversion != dll->apiversion)
		{
			sprintf(errorstring, "Sys_LoadLibrary: mismatch version (%i should be %i)\n", check->apiversion, dll->apiversion);
			goto error;
		}
		else MsgDev(D_ERROR, " [%d]", check->apiversion ); 
		if(check->api_size != dll->api_size)
		{
			sprintf(errorstring, "Sys_LoadLibrary: mismatch interface size (%i should be %i)\n", check->api_size, dll->api_size);
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

bool SysFileExists (const char *path)
{
	int desc;
     
	desc = open (path, O_RDONLY | O_BINARY);

	if (desc < 0) return false;
	close (desc);
	return true;
}

void GetBaseDir( char *pszBuffer, char *out )
{
	char basedir[ MAX_SYSPATH ];
	char szBuffer[ MAX_SYSPATH ];
	int j;
	char *pBuffer = NULL;

	strcpy( szBuffer, pszBuffer );

	pBuffer = strrchr( szBuffer,'\\' );
	if ( pBuffer ) *(pBuffer+1) = '\0';

	strcpy( basedir, szBuffer );

	j = strlen( basedir );
	if (j > 0)
	{
		if ( ( basedir[ j-1 ] == '\\' ) || ( basedir[ j-1 ] == '/' ) )
		{
			basedir[ j-1 ] = 0;
		}
	}
	strcpy(out, basedir);
}

void ReadEnvironmentVariables( char *pPath )
{
	// get basepath from registry
	REG_GetValue(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Control\\Session Manager\\Environment", "Xash3D", pPath );
}

void SaveEnvironmentVariables( char *pPath )
{
	// save new path
	REG_SetValue(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Control\\Session Manager\\Environment", "Xash3D", pPath );
	SendMessageTimeout( HWND_BROADCAST, WM_SETTINGCHANGE, 0, 0, SMTO_NORMAL, 10, NULL);//system update message
}

static void BuildPath( char *pPath, char *pOut )
{
	// set working directory
	SetCurrentDirectory ( pPath );
	sprintf( pOut, "%s\\bin\\launch.dll", pPath );
}

void UpdateEnvironmentVariables( void )
{
	char szTemp[ 4096 ];
	char szPath[ MAX_SYSPATH ]; //test path
	
	//NOTE: we want set "real" work directory
	//defined in environment variables, but in some reasons
	//we need make some additional checks before set current dir
          
          //get variable from registry and current directory
	ReadEnvironmentVariables( szTemp );
	GetCurrentDirectory(MAX_SYSPATH, sys_rootdir );
	
	// if both values is math - no run additional tests		
	if(stricmp(sys_rootdir, szTemp ))
	{
		// Step1: path from registry have higher priority than current working directory
		// because user can execute launcher from random place or from a bat-file
                    // so, set current working directory as path from registry and test it

		BuildPath( szTemp, szPath );
		if(!SysFileExists( szPath )) // Step2: engine root dir has been moved to other place?
		{
			BuildPath( sys_rootdir, szPath );
			if(!SysFileExists( szPath )) // Step3: directly execute from bin directory?
			{
				// Step4: create last test for bin directory
			          GetBaseDir( sys_rootdir, szTemp );
				BuildPath( szTemp, szPath );
				if(!SysFileExists( szPath )) //make exception
				{
					// big bada-boom: engine was moved and launcher running from other place
					// step5: so, path form registry is invalid, current path is no valid
					Sys_ErrorFatal( ERR_INVALID_ROOT );
				}
				else SaveEnvironmentVariables( szTemp );
			}
			else SaveEnvironmentVariables( sys_rootdir );
		}
	}
}