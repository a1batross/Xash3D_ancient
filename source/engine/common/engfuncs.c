//=======================================================================
//			Copyright XashXT Group 2008 ©
//		vm_engfuncs.c - misc functions used by virtual machine
//=======================================================================

#include "common.h"
#include "byteorder.h"
#include "mathlib.h"
#include "const.h"
#include "client.h"
#include "cvardef.h"

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
pfnGetGameDir

=============
*/
void pfnGetGameDir( char *szGetGameDir )
{
	// FIXME: potentially crashpoint
	com.strcpy( szGetGameDir, FS_Gamedir());
}