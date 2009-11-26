//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        infostring.c - network info strings
//=======================================================================

#include "common.h"
#include "byteorder.h"

#define MAX_INFO_KEY	64
#define MAX_INFO_VALUE	64

static char		infostring[MAX_INFO_STRING*4];

/*
=======================================================================

			INFOSTRING STUFF
=======================================================================
*/
/*
===============
Info_Print

printing current key-value pair
===============
*/
void Info_Print( const char *s )
{
	char	key[MAX_INFO_STRING];
	char	value[MAX_INFO_STRING];
	char	*o;
	int	l;

	if( *s == '\\' ) s++;

	while( *s )
	{
		o = key;
		while( *s && *s != '\\' )
			*o++ = *s++;

		l = o - key;
		if( l < 20 )
		{
			Mem_Set( o, ' ', 20 - l );
			key[20] = 0;
		}
		else *o = 0;
		Msg( "%s", key );

		if( !*s )
		{
			Msg( "(null)\n" );
			return;
		}

		o = value;
		s++;
		while( *s && *s != '\\' )
			*o++ = *s++;
		*o = 0;

		if( *s ) s++;
		Msg( "%s\n", value );
	}
}

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
char *Info_ValueForKey( const char *s, const char *key )
{
	char	pkey[MAX_INFO_STRING];
	static	char value[2][MAX_INFO_STRING]; // use two buffers so compares work without stomping on each other
	static	int valueindex;
	char	*o;
	
	valueindex ^= 1;
	if( *s == '\\' ) s++;
	while( 1 )
	{
		o = pkey;
		while( *s != '\\' && *s != '\n' )
		{
			if( !*s ) return "";
			*o++ = *s++;
		}

		*o = 0;
		s++;

		o = value[valueindex];

		while( *s != '\\' && *s != '\n' && *s )
		{
			if( !*s ) return "";
			*o++ = *s++;
		}
		*o = 0;

		if( !com.strcmp( key, pkey ))
			return value[valueindex];
		if( !*s ) return "";
		s++;
	}
}

bool Info_RemoveKey( char *s, const char *key )
{
	char	*start;
	char	pkey[MAX_INFO_STRING];
	char	value[MAX_INFO_STRING];
	char	*o;

	if( com.strstr( key, "\\" ))
		return false;

	while( 1 )
	{
		start = s;
		if( *s == '\\' ) s++;
		o = pkey;

		while( *s != '\\' )
		{
			if( !*s ) return false;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while( *s != '\\' && *s )
		{
			if( !*s ) return false;
			*o++ = *s++;
		}
		*o = 0;

		if( !com.strcmp( key, pkey ))
		{
			com.strcpy( start, s ); // remove this part
			return true;
		}
		if( !*s ) return false;
	}
}

/*
==================
Info_Validate

Some characters are illegal in info strings because they
can mess up the server's parsing
==================
*/
bool Info_Validate( const char *s )
{
	if( com.strstr( s, "\"" )) return false;
	if( com.strstr( s, ";" )) return false;
	return true;
}

bool Info_SetValueForKey( char *s, const char *key, const char *value )
{
	char	newi[MAX_INFO_STRING], *v;
	int	c, maxsize = MAX_INFO_STRING;

	if( com.strstr( key, "\\" ) || com.strstr( value, "\\" ))
	{
		MsgDev( D_ERROR, "SetValueForKey: can't use keys or values with a \\\n" );
		return false;
	}

	if( com.strstr( key, ";" ))
	{
		MsgDev( D_ERROR, "SetValueForKey: can't use keys or values with a semicolon\n" );
		return false;
	}

	if( com.strstr( key, "\"" ) || com.strstr( value, "\"" ))
	{
		MsgDev( D_ERROR, "SetValueForKey: can't use keys or values with a \"\n" );
		return false;
	}

	if( com.strlen( key ) > MAX_INFO_KEY - 1 || com.strlen( value ) > MAX_INFO_KEY - 1 )
	{
		MsgDev( D_ERROR, "SetValueForKey: keys and values must be < %i characters.\n", MAX_INFO_KEY );
		return false;
	}

	Info_RemoveKey( s, key );
	if( !value || !com.strlen( value ))
		return true;	// just clear variable

	com.sprintf( newi, "\\%s\\%s", key, value );
	if( com.strlen( newi ) + com.strlen( s ) > maxsize )
	{
		MsgDev( D_ERROR, "SetValueForKey: info string length exceeded\n" );
		return true; // info changed, new value can't saved
	}

	// only copy ascii values
	s += com.strlen( s );
	v = newi;

	while( *v )
	{
		c = *v++;
		c &= 255;	// strip high bits
		if( c >= 32 && c <= 255 )
			*s++ = c;
	}
	*s = 0;

	// all done
	return true;
}

static void Cvar_LookupBitInfo( const char *name, const char *string, const char *info, void *unused )
{
	Info_SetValueForKey( (char *)info, name, string );
}

char *Cvar_Userinfo( void )
{
	infostring[0] = 0; // clear previous calls
	Cvar_LookupVars( CVAR_USERINFO, infostring, NULL, Cvar_LookupBitInfo ); 
	return infostring;
}

char *Cvar_Serverinfo( void )
{
	infostring[0] = 0; // clear previous calls
	Cvar_LookupVars( CVAR_SERVERINFO, infostring, NULL, Cvar_LookupBitInfo ); 
	return infostring;
}