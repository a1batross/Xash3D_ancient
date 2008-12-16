//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        infostring.c - network info strings
//=======================================================================

#include "common.h"
#include "byteorder.h"

static char sv_info[MAX_INFO_STRING];

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
void Info_Print( char *s )
{
	char	key[512];
	char	value[512];
	char	*o;
	int	l;

	if( *s == '\\' )s++;

	while (*s)
	{
		o = key;
		while (*s && *s != '\\') *o++ = *s++;

		l = o - key;
		if (l < 20)
		{
			memset (o, ' ', 20-l);
			key[20] = 0;
		}
		else *o = 0;
		Msg ("%s", key);

		if (!*s)
		{
			Msg ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\') *o++ = *s++;
		*o = 0;

		if (*s) s++;
		Msg ("%s\n", value);
	}
}

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
char *Info_ValueForKey( char *s, char *key )
{
	char	pkey[512];
	static	char value[2][512];	// use two buffers so compares work without stomping on each other
	static	int valueindex;
	char	*o;
	
	valueindex ^= 1;
	if( *s == '\\' ) s++;
	while( 1 )
	{
		o = pkey;
		while( *s != '\\' && *s != '\n' )
		{
			if(!*s) return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while( *s != '\\' && *s != '\n' && *s)
		{
			if (!*s) return "";
			*o++ = *s++;
		}
		*o = 0;

		if(!com.strcmp( key, pkey )) return value[valueindex];
		if(!*s) return "";
		s++;
	}
}

void Info_RemoveKey (char *s, char *key)
{
	char	*start;
	char	pkey[512];
	char	value[512];
	char	*o;

	if (com.strstr (key, "\\")) return;

	while (1)
	{
		start = s;
		if (*s == '\\') s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s) return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s) return;
			*o++ = *s++;
		}
		*o = 0;

		if (!com.strcmp (key, pkey) )
		{
			com.strcpy (start, s);	// remove this part
			return;
		}
		if (!*s) return;
	}
}

/*
==================
Info_Validate

Some characters are illegal in info strings because they
can mess up the server's parsing
==================
*/
bool Info_Validate (char *s)
{
	if (com.strstr (s, "\"")) return false;
	if (com.strstr (s, ";")) return false;
	return true;
}

void Info_SetValueForKey (char *s, char *key, char *value)
{
	char	newi[MAX_INFO_STRING], *v;
	int	c, maxsize = MAX_INFO_STRING;

	if (com.strstr (key, "\\") || com.strstr (value, "\\") )
	{
		Msg ("Can't use keys or values with a \\\n");
		return;
	}

	if (com.strstr (key, ";") )
	{
		Msg ("Can't use keys or values with a semicolon\n");
		return;
	}
	if (com.strstr (key, "\"") || com.strstr (value, "\"") )
	{
		Msg ("Can't use keys or values with a \"\n");
		return;
	}
	if (com.strlen(key) > MAX_INFO_KEY - 1 || com.strlen(value) > MAX_INFO_KEY-1)
	{
		Msg ("Keys and values must be < 64 characters.\n");
		return;
	}

	Info_RemoveKey (s, key);
	if (!value || !com.strlen(value)) return;
	com.sprintf (newi, "\\%s\\%s", key, value);

	if (com.strlen(newi) + com.strlen(s) > maxsize)
	{
		Msg ("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += com.strlen(s);
	v = newi;
	while (*v)
	{
		c = *v++;
		c &= 127;	// strip high bits
		if (c >= 32 && c < 127) *s++ = c;
	}
	*s = 0;
}

static void Cvar_LookupBitInfo( const char *name, const char *string, const char *info, void *unused )
{
	Info_SetValueForKey( (char *)info, (char *)name, (char *)string );
}

char *Cvar_Userinfo( void )
{
	sv_info[0] = 0; // clear previous calls
	Cvar_LookupVars( CVAR_USERINFO, sv_info, NULL, Cvar_LookupBitInfo ); 
	return sv_info;
}

char *Cvar_Serverinfo( void )
{
	sv_info[0] = 0; // clear previous calls
	Cvar_LookupVars( CVAR_SERVERINFO, sv_info, NULL, Cvar_LookupBitInfo ); 
	return sv_info;
}