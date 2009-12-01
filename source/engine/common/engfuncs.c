//=======================================================================
//			Copyright XashXT Group 2008 ©
//		engfuncs.c - misc functions used by dlls'
//=======================================================================

#include "common.h"
#include "byteorder.h"
#include "mathlib.h"
#include "const.h"
#include "client.h"
#include "cvardef.h"
#include "com_library.h"

/*
==============
pfnParseToken

simple dlls version
==============
*/
char *pfnParseToken( const char **data_p )
{
	int		c, len = 0;
	const char	*data;
	static char	token[8192];
	
	token[0] = 0;
	data = *data_p;
	
	if( !data ) 
	{
		*data_p = NULL;
		return NULL;
	}		

	// skip whitespace
skipwhite:
	while(( c = *data) <= ' ' )
	{
		if( c == 0 )
		{
			*data_p = NULL;
			return NULL; // end of file;
		}
		data++;
	}
	
	// skip // comments
	if( c=='/' && data[1] == '/' )
	{
		while( *data && *data != '\n' )
			data++;
		goto skipwhite;
	}

	// skip /* comments
	if( c=='/' && data[1] == '*' )
	{
		while( data[1] && (data[0] != '*' || data[1] != '/' ))
			data++;
		data += 2;
		goto skipwhite;
	}
	

	// handle quoted strings specially
	if( *data == '\"' || *data == '\'' )
	{
		data++;
		while( 1 )
		{
			c = *data++;
			if( c=='\"' || c=='\0' )
			{
				token[len] = 0;
				*data_p = data;
				return token;
			}
			token[len] = c;
			len++;
		}
	}

	// parse single characters
	if( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':' || c == ',' )
	{
		token[len] = c;
		data++;
		len++;
		token[len] = 0;
		*data_p = data;
		return token;
	}

	// parse a regular word
	do
	{
		token[len] = c;
		data++;
		len++;
		c = *data;
		if( c == '{' || c == '}'|| c == ')'|| c == '(' || c == '\'' || c == ':' || c == ',' )
			break;
	} while( c > 32 );
	
	token[len] = 0;
	*data_p = data;
	return token;
}

/*
=============
pfnMemFgets

=============
*/
char *pfnMemFgets( byte *pMemFile, int fileSize, int *filePos, char *pBuffer, int bufferSize )
{
	int	i, last, stop;

	if( !pMemFile || !pBuffer || !filePos )
		return NULL;

	if( *filePos >= fileSize )
		return NULL;

	i = *filePos;
	last = fileSize;

	// fgets always NULL terminates, so only read bufferSize-1 characters
	if( last - *filePos > ( bufferSize - 1 ))
		last = *filePos + ( bufferSize - 1);

	stop = 0;

	// stop at the next newline (inclusive) or end of buffer
	while( i < last && !stop )
	{
		if( pMemFile[i] == '\n' )
			stop = 1;
		i++;
	}


	// if we actually advanced the pointer, copy it over
	if( i != *filePos )
	{
		// we read in size bytes
		int	size = i - *filePos;

		// copy it out
		Mem_Copy( pBuffer, pMemFile + *filePos, size );
		
		// If the buffer isn't full, terminate (this is always true)
		if( size < bufferSize ) pBuffer[size] = 0;

		// update file pointer
		*filePos = i;
		return pBuffer;
	}
	return NULL;
}

/*
=============
pfnLoadFile

=============
*/
byte* pfnLoadFile( const char *filename, int *pLength )
{
	return FS_LoadFile( filename, pLength );
}

void pfnFreeFile( void *buffer )
{
	if( buffer ) Mem_Free( buffer );
}

/*
=============
pfnFileExists

=============
*/
int pfnFileExists( const char *filename )
{
	return FS_FileExists( filename );
}

/*
=============
pfnRandomLong

=============
*/
long pfnRandomLong( long lLow, long lHigh )
{
	return Com_RandomLong( lLow, lHigh );
}

/*
=============
pfnRandomFloat

=============
*/
float pfnRandomFloat( float flLow, float flHigh )
{
	return Com_RandomFloat( flLow, flHigh );
}

/*
=============
pfnTime

=============
*/
float pfnTime( void )
{
	return (Host_Milliseconds() * 0.001f);
}

/*
=============
pfnCVarRegister

=============
*/
cvar_t *pfnCVarRegister( const char *szName, const char *szValue, int flags, const char *szDesc )
{
	int	real_flags = 0;

	if( flags & FCVAR_ARCHIVE ) real_flags |= CVAR_ARCHIVE;
	if( flags & FCVAR_USERINFO ) real_flags |= CVAR_USERINFO;
	if( flags & FCVAR_SERVERINFO ) real_flags |= CVAR_SERVERINFO;
	if( flags & FCVAR_LATCH ) real_flags |= CVAR_LATCH;

	return Cvar_Get( szName, szValue, real_flags, szDesc );
}

/*
=============
pfnAlertMessage

=============
*/
void pfnAlertMessage( ALERT_TYPE level, char *szFmt, ... )
{
	char		buffer[2048];	// must support > 1k messages
	va_list		args;

	va_start( args, szFmt );
	com.vsnprintf( buffer, 2048, szFmt, args );
	va_end( args );

	if( host.developer < level )
		return;

	switch( level )
	{
	case at_console:	
		com.print( buffer );
		break;
	case at_warning:
		com.print( va("^3Warning:^7 %s", buffer ));
		break;
	case at_error:
		com.print( va("^1Error:^7 %s", buffer ));
		break;
	case at_loading:
		com.print( va("^2Loading:^7 %s", buffer ));
		break;
	case at_aiconsole:
	case at_logged:
		com.print( buffer );
		break;
	}
}

/*
=============
pfnMemCopy

=============
*/
void pfnMemCopy( void *dest, const void *src, size_t cb, const char *filename, const int fileline )
{
	com.memcpy( dest, src, cb, filename, fileline );
}

/*
=============
pfnFOpen

=============
*/
void *pfnFOpen( const char* path, const char* mode )
{
	return FS_Open( path, mode );
}

/*
=============
pfnFClose

=============
*/
int pfnFClose( void *file )
{
	return FS_Close( file );
}

/*
=============
pfnFWrite

=============
*/
long pfnFWrite( void *file, const void* data, size_t datasize )
{
	return FS_Write( file, data, datasize );
}

/*
=============
pfnFRead

=============
*/
long pfnFRead( void *file, void* buffer, size_t buffersize )
{
	return FS_Read( file, buffer, buffersize );
}

/*
=============
pfnFGets

=============
*/
int pfnFGets( void *file, byte *string, size_t bufsize )
{
	return FS_Gets( file, string, bufsize );
}

/*
=============
pfnFSeek

=============
*/
int pfnFSeek( void *file, long offset, int whence )
{
	return FS_Seek( file, offset, whence );
}

/*
=============
pfnFTell

=============
*/
long pfnFTell( void *file )
{
	return FS_Tell( file );
}

/*
=============
pfnEngineFprintf

=============
*/
void pfnEngineFprintf( void *file, char *szFmt, ... )
{
	char		buffer[2048];	// must support > 1k messages
	va_list		args;

	va_start( args, szFmt );
	com.vsnprintf( buffer, 2048, szFmt, args );
	va_end( args );

	FS_Print( file, buffer );
}

/*
=============
pfnLoadLibrary

=============
*/
void *pfnLoadLibrary( const char *name )
{
	string	libpath;

	Com_BuildPath( name, libpath );
	return Com_LoadLibrary( libpath );
}

/*
=============
pfnGetProcAddress

=============
*/
void *pfnGetProcAddress( void *hInstance, const char *name )
{
	if( !hInstance ) return NULL;
	return Com_GetProcAddress( hInstance, name );
}

/*
=============
pfnFreeLibrary

=============
*/
void pfnFreeLibrary( void *hInstance )
{
	Com_FreeLibrary( hInstance );
}

/*
=============
pfnRemoveFile

=============
*/
void pfnRemoveFile( const char *szFilename )
{
	string	path;

	if( !szFilename || !*szFilename ) return;
	com.sprintf( path, "%s/%s", GI->gamedir, szFilename );
	FS_Delete( path );
}