//=======================================================================
//			Copyright (C) XashXT Group 2007
//		       getlib.c - used for find engine library
//		       getting direct path or from environment
//======================================================================= 

#ifndef GETLIB_H
#define GETLIB_H

#define NDEBUG

#include <sys\types.h>
#include <windows.h>
#include <winreg.h>
#include <stdio.h>

#pragma comment(lib, "advapi32")
#pragma comment(lib, "user32")

#pragma comment(linker,"/MERGE:.rdata=.text")
#pragma comment(linker,"/FILEALIGN:512 /SECTION:.text, EWRX /IGNORE:4078")

#define Run32( prog ) return CreateMain32()( #prog, lpCmdLine )

//console format
typedef int (*main_t)( char *funcname, int argc, char **argv );
typedef int (*winmain_t)( char *funcname, LPSTR lpCmdLine );
char szSearch[ 5 ][ 1024 ];
char szFsPath[ 4096 ];
HINSTANCE	hmain;

BOOL GetBin( void )
{
	//grab direct path?
	return TRUE;
}

BOOL GetEnv( void )
{
	char *pEnvPath = getenv("Xash3D");
	if(!pEnvPath) return FALSE;

	strcpy(szFsPath, pEnvPath );
	return TRUE;
}

BOOL GetReg( void )
{
	HKEY hKey;
	DWORD dwBufLen = 4096; //max env length
	long lRet;

	lRet = RegOpenKeyEx( HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Control\\Session Manager\\Environment", 0, KEY_READ, &hKey);
  	if(lRet != ERROR_SUCCESS) return FALSE;
	lRet = RegQueryValueEx( hKey, "Xash3D", NULL, NULL, (byte *)szFsPath, &dwBufLen);
	if(lRet != ERROR_SUCCESS) return FALSE;
	RegCloseKey( hKey );
	
	return TRUE;
}

void GetLibrary ( void )
{
	int i, count = 0;
	
	if(GetEnv()) //get environment
	{
		sprintf( szSearch[count], "%s\\bin\\launcher.dll", szFsPath );
		count++;
	}

	if(GetReg()) //get registry
	{
		sprintf( szSearch[count], "%s\\bin\\launcher.dll", szFsPath );
		count++;
	}
	
	if(GetBin()) //get direct
	{
		strcpy( szSearch[count], "bin\\launcher.dll" );
		count++;
	}

	if(GetBin()) //get direct
	{
		strcpy( szSearch[count], "launcher.dll" );
		count++;
	}	
	
	for(i = 0; i < count; i++) { if(hmain = LoadLibrary( szSearch[i])) break; }
}

winmain_t CreateMain32( void )
{
	winmain_t main;
	
	GetLibrary();
	
	if (hmain) 
	{
		main = (winmain_t)GetProcAddress( hmain, "CreateAPI" );
		if(!main)
		{
			MessageBox( 0, "unable to find entry point", "Error", MB_OK );
			exit(1);
			return 0; //make compiller happy
		}
		return main;
	}

	MessageBox( 0, "unable to load the launcher.dll", "Error", MB_OK );
	exit(1);
	return 0; //make compiller happy
}

#endif//GETLIB_H 