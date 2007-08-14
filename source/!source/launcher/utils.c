//=======================================================================
//			Copyright XashXT Group 2007 ©
//			utils.c - shared launcher utils
//=======================================================================

#include <time.h>
#include "launcher.h"

char sys_rootdir[ MAX_SYSPATH ];
bool debug_mode = false;
bool console_read_only = true;

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
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine)
		{
			com_argv[com_argc] = lpCmdLine;
			com_argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
				lpCmdLine++;

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
			
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

bool GetParmFromCmdLine( char *parm, char *out )
{
	int argc = CheckParm( parm );

	if(!argc) return false;
	if(!out) return false;	

	strcpy( out, com_argv[argc+1]);
	return true;
}

/*
================
Sys_Exit

NOTE: we must prepare engine to shutdown
before call this
================
*/
void Sys_Exit (void)
{
	//prepare host to close
	Host_Free();

	if(linked_dll) FreeLibrary(linked_dll);

	Sys_FreeConsole();	
	exit(sys_error);
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
	sprintf( pOut, "%s\\bin\\launcher.dll", pPath );
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
	
	//if both values is math - no run additional tests		
	if(stricmp(sys_rootdir, szTemp ))
	{
		//path from registry have higher priority than current working directory
		//because user can execute launcher from random place or from a bat-file
                    //so, set current working directory as path from registry and test it

		BuildPath( szTemp, szPath );
		if(!SysFileExists( szPath )) //engine root dir has been moved to other place?
		{
			BuildPath( sys_rootdir, szPath );
			if(!SysFileExists( szPath )) //directly execute from bin directory?
			{
				//step4: create last test for bin directory
			          GetBaseDir( sys_rootdir, szTemp );
				BuildPath( szTemp, szPath );
				if(!SysFileExists( szPath )) //make exception
				{
					//big bada-boom: engine was moved and launcher was running from other place
					//step5: so, path form registry is invalid, current path is no valid
					Sys_Error("Invalid root directory!\n");
				}
				else SaveEnvironmentVariables( szTemp );
			}
			else SaveEnvironmentVariables( sys_rootdir );
		}
	}
}