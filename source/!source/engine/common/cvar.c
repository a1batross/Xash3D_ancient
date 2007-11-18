/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// cvar.c -- dynamic variable tracking

#include "engine.h"

cvar_t	*cvar_vars;
cvar_t	*host_cheats;

#define	MAX_CVARS		1024
#define FILE_HASH_SIZE	256

int cvar_numIndexes;
int cvar_modifiedFlags;
bool userinfo_modified;
cvar_t cvar_indexes[MAX_CVARS];
static cvar_t* hashTable[FILE_HASH_SIZE];

//=======================================================================
//			INFOSTRING STUFF
//=======================================================================
/*
===============
Info_Print

printing current key-value pair
===============
*/
void Info_Print (char *s)
{
	char	key[512];
	char	value[512];
	char	*o;
	int	l;

	if (*s == '\\') s++;

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
char *Info_ValueForKey (char *s, char *key)
{
	char	pkey[512];
	static	char value[2][512];	// use two buffers so compares work without stomping on each other
	static	int valueindex;
	char	*o;
	
	valueindex ^= 1;
	if (*s == '\\') s++;
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s) return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s) return "";
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) ) return value[valueindex];
		if (!*s) return "";
		s++;
	}
}

void Info_RemoveKey (char *s, char *key)
{
	char	*start;
	char	pkey[512];
	char	value[512];
	char	*o;

	if (strstr (key, "\\")) return;

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

		if (!strcmp (key, pkey) )
		{
			strcpy (start, s);	// remove this part
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
	if (strstr (s, "\"")) return false;
	if (strstr (s, ";")) return false;
	return true;
}

void Info_SetValueForKey (char *s, char *key, char *value)
{
	char	newi[MAX_INFO_STRING], *v;
	int	c, maxsize = MAX_INFO_STRING;

	if (strstr (key, "\\") || strstr (value, "\\") )
	{
		Msg ("Can't use keys or values with a \\\n");
		return;
	}

	if (strstr (key, ";") )
	{
		Msg ("Can't use keys or values with a semicolon\n");
		return;
	}
	if (strstr (key, "\"") || strstr (value, "\"") )
	{
		Msg ("Can't use keys or values with a \"\n");
		return;
	}
	if (strlen(key) > MAX_INFO_KEY - 1 || strlen(value) > MAX_INFO_KEY-1)
	{
		Msg ("Keys and values must be < 64 characters.\n");
		return;
	}

	Info_RemoveKey (s, key);
	if (!value || !strlen(value)) return;
	sprintf (newi, "\\%s\\%s", key, value);

	if (strlen(newi) + strlen(s) > maxsize)
	{
		Msg ("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = newi;
	while (*v)
	{
		c = *v++;
		c &= 127;	// strip high bits
		if (c >= 32 && c < 127) *s++ = c;
	}
	*s = 0;
}

/*
================
return a hash value for the filename
================
*/
static long Cvar_GetHashValue( const char *fname )
{
	int	i = 0;
	long	hash = 0;
	char	letter;

	while (fname[i] != '\0')
	{
		letter = tolower(fname[i]);
		hash += (long)(letter)*(i + 119);
		i++;
	}
	hash &= (FILE_HASH_SIZE - 1);
	return hash;
}

/*
============
Cvar_InfoValidate
============
*/
static bool Cvar_ValidateString(const char *s, bool isvalue )
{
	if ( !s ) return false;
	if (strstr(s, "\\") && !isvalue)
		return false;
	if (strstr(s, "\"")) return false;
	if (strstr(s, ";")) return false;
	return true;
}

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar (const char *var_name)
{
	cvar_t	*var;
	long	hash;

	hash = Cvar_GetHashValue(var_name);
	
	for (var = hashTable[hash]; var; var = var->hash)
	{
		if (!stricmp(var_name, var->name))
			return var;
	}

	return NULL;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue(const char *var_name)
{
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var) return 0;
	return var->value;
}

/*
============
Cvar_VariableIntegerValue
============
*/
int Cvar_VariableInteger( const char *var_name )
{
	cvar_t	*var;
	
	var = Cvar_FindVar(var_name);
	if (!var) return 0;
	return var->integer;
}

/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString (const char *var_name)
{
	cvar_t *var;
	
	var = Cvar_FindVar (var_name);
	if (!var) return "";
	return var->string;
}

/*
============
Cvar_VariableStringBuffer

place variable into buffer
============
*/
void Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize )
{
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var) *buffer = 0;
	else strncpy( buffer, var->string, bufsize );
}

/*
============
Cvar_CommandCompletion
============
*/
void Cvar_CommandCompletion( void(*callback)(const char *s, const char *m))
{
	cvar_t		*cvar;
	
	for( cvar = cvar_vars; cvar; cvar = cvar->next )
		callback( cvar->name, cvar->description );
}

/*
============
Cvar_Get

If the variable already exists, the value will not be set
The flags will be or'ed in if the variable exists.
============
*/
cvar_t *_Cvar_Get(const char *var_name, const char *var_value, int flags, const char *var_desc )
{
	cvar_t	*var;
	long	hash;

	if(!var_name || !var_value)
	{
		MsgDev( D_ERROR, "Cvar_Get: NULL parameter" );
		return NULL;
	}
	
	if (!Cvar_ValidateString(var_name, false))
	{
		MsgDev(D_WARN, "invalid info cvar name string %s\n", var_name );
		var_value = "noname";
	}
	if(!Cvar_ValidateString( var_value, true ))
	{
		MsgDev(D_WARN, "invalid cvar value string: %s\n", var_value );
		var_value = "default";
	}

	var = Cvar_FindVar (var_name);
	if ( var )
	{
		// if the C code is now specifying a variable that the user already
		// set a value for, take the new value as the reset value

		if(( var->flags & CVAR_USER_CREATED ) && !( flags & CVAR_USER_CREATED ) && var_value[0])
		{
			var->flags &= ~CVAR_USER_CREATED;
			Z_Free( var->reset_string );
			var->reset_string = copystring( var_value );
			cvar_modifiedFlags |= flags;
		}

		var->flags |= flags;

		// only allow one non-empty reset string without a warning
		if( !var->reset_string[0] )
		{
			// we don't have a reset string yet
			Z_Free( var->reset_string );
			var->reset_string = copystring( var_value );
		}
		else if ( var_value[0] && strcmp( var->reset_string, var_value ))
		{
			MsgDev(D_WARN, "cvar \"%s\" given initial values: \"%s\" and \"%s\"\n", var_name, var->reset_string, var_value );
		}

		// if we have a latched string, take that value now
		if ( var->latched_string )
		{
			char *s;

			s = var->latched_string;
			var->latched_string = NULL; // otherwise cvar_set2 would free it
			Cvar_Set2( var_name, s, true );
			Z_Free( s );
		}
		return var;
	}

	// allocate a new cvar
	if( cvar_numIndexes >= MAX_CVARS )
	{
		MsgWarn("Cvar_Get: MAX_CVARS limit exceeded\n" );
		return NULL;
	}

	var = &cvar_indexes[cvar_numIndexes];
	cvar_numIndexes++;
	var->name = copystring(var_name);
	var->string = copystring(var_value);
	var->description = copystring(var_desc);
	var->modified = true;
	var->modificationCount = 1;
	var->value = atof(var->string);
	var->integer = atoi(var->string);
	var->reset_string = copystring( var_value );

	// link the variable in
	var->next = cvar_vars;
	cvar_vars = var;

	var->flags = flags;

	hash = Cvar_GetHashValue(var_name);
	var->hash = hashTable[hash];
	hashTable[hash] = var;

	return var;
}

/*
============
Cvar_Set2
============
*/
cvar_t *Cvar_Set2 (const char *var_name, const char *value, bool force)
{
	cvar_t	*var;

	if( !Cvar_ValidateString( var_name, false ))
	{
		MsgDev(D_WARN, "invalid cvar name string: %s\n", var_name );
		var_name = "unknown";
	}

	if ( value && !Cvar_ValidateString( value, true ))
	{
		MsgDev(D_WARN, "invalid cvar value string: %s\n", value );
		value = "default";
	}

	var = Cvar_FindVar (var_name);
	if (!var)
	{
		if( !value ) return NULL;

		// create it
		if ( !force ) return Cvar_Get( var_name, value, CVAR_USER_CREATED );
		else return Cvar_Get (var_name, value, 0 );
	}

	if(!value ) value = var->reset_string;
	if(!strcmp(value, var->string)) return var;

	// note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
	cvar_modifiedFlags |= var->flags;

	if (!force)
	{
		if (var->flags & CVAR_READ_ONLY)
		{
			MsgDev(D_INFO, "%s is read only.\n", var_name);
			return var;
		}
		if (var->flags & CVAR_INIT)
		{
			MsgDev(D_INFO, "%s is write protected.\n", var_name);
			return var;
		}
		if (var->flags & CVAR_LATCH)
		{
			if (var->latched_string)
			{
				if (!strcmp(value, var->latched_string))
					return var;
				Z_Free (var->latched_string);
			}
			else
			{
				if (!strcmp(value, var->string))
					return var;
			}
			MsgDev(D_INFO, "%s will be changed after restarting.\n", var_name);
			var->latched_string = copystring(value);
			var->modified = true;
			var->modificationCount++;
			return var;
		}
		if ( (var->flags & CVAR_CHEAT) && !host_cheats->integer )
		{
			MsgDev(D_INFO, "%s is cheat protected.\n", var_name);
			return var;
		}
	}
	else
	{
		if (var->latched_string)
		{
			Z_Free(var->latched_string);
			var->latched_string = NULL;
		}
	}

	if (!strcmp(value, var->string))
		return var; // not changed

	var->modified = true;
	var->modificationCount++;
	
	Z_Free (var->string); // free the old value string
	
	var->string = copystring(value);
	var->value = atof (var->string);
	var->integer = atoi (var->string);

	return var;
}

/*
============
Cvar_Set
============
*/
void Cvar_Set( const char *var_name, const char *value)
{
	Cvar_Set2 (var_name, value, true);
}

/*
============
Cvar_SetLatched
============
*/
void Cvar_SetLatched( const char *var_name, const char *value)
{
	Cvar_Set2 (var_name, value, false);
}

/*
============
Cvar_FullSet
============
*/
void Cvar_FullSet (char *var_name, char *value, int flags)
{
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var) 
	{
		// create it
		Cvar_Get (var_name, value, flags);
		return;
	}
	var->modified = true;

	if (var->flags & CVAR_USERINFO)
		userinfo_modified = true; // transmit at next oportunity
	
	Z_Free (var->string); // free the old value string
	
	var->string = copystring(value);
	var->value = atof(var->string);
	var->integer = atoi(var->string);
	var->flags = flags;
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue( const char *var_name, float value)
{
	char	val[32];

	if( value == (int)value ) sprintf (val, "%i", (int)value);
	else sprintf (val, "%f", value);
	Cvar_Set (var_name, val);
}

/*
============
Cvar_Reset
============
*/
void Cvar_Reset( const char *var_name )
{
	Cvar_Set2( var_name, NULL, false );
}

/*
============
Cvar_SetCheatState

Any testing variables will be reset to the safe values
============
*/
void Cvar_SetCheatState( void )
{
	cvar_t	*var;

	// set all default vars to the safe value
	for ( var = cvar_vars ; var ; var = var->next )
	{
		if( var->flags & CVAR_CHEAT )
		{
			// the CVAR_LATCHED|CVAR_CHEAT vars might escape the reset here 
			// because of a different var->latched_string
			if (var->latched_string)
			{
				Z_Free(var->latched_string);
				var->latched_string = NULL;
			}
			if (strcmp(var->reset_string, var->string))
			{
				Cvar_Set( var->name, var->reset_string );
			}
		}
	}
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
bool Cvar_Command( void )
{
	cvar_t	*v;

	// check variables
	v = Cvar_FindVar (Cmd_Argv(0));
	if (!v) return false;

	// perform a variable print or set
	if ( Cmd_Argc() == 1 )
	{
		if(v->flags & CVAR_INIT) Msg("%s: %s\n", v->name, v->string );
		else Msg("%s: %s ( ^3%s^7 )\n", v->name, v->string, v->reset_string );
		if ( v->latched_string ) Msg( "%s: %s\n", v->name, v->latched_string );
		return true;
	}

	// set the value if forcing isn't required
	Cvar_Set2 (v->name, Cmd_Argv(1), false);
	return true;
}

/*
============
Cvar_Toggle_f

Toggles a cvar for easy single key binding
============
*/
void Cvar_Toggle_f( void )
{
	int	v;

	if( Cmd_Argc() != 2 )
	{
		Msg("usage: toggle <variable>\n");
		return;
	}

	v = Cvar_VariableValue( Cmd_Argv( 1 ));
	v = !v;

	Cvar_Set2 (Cmd_Argv(1), va("%i", v), false);
}

/*
============
Cvar_Set_f

Allows setting and defining of arbitrary cvars from console, even if they
weren't declared in C code.
============
*/
void Cvar_Set_f( void )
{
	int	i, c, l = 0, len;
	char	combined[MAX_STRING_TOKENS];

	c = Cmd_Argc();
	if ( c < 3 )
	{
		Msg("usage: set <variable> <value>\n");
		return;
	}

	combined[0] = 0;

	for ( i = 2; i < c; i++ )
	{
		len = strlen( Cmd_Argv(i) + 1 );
		if ( l + len >= MAX_STRING_TOKENS - 2 )
			break;
		strcat( combined, Cmd_Argv(i));
		if ( i != c-1 ) strcat( combined, " " );
		l += len;
	}
	Cvar_Set2 (Cmd_Argv(1), combined, false);
}

/*
============
Cvar_SetU_f

As Cvar_Set, but also flags it as userinfo
============
*/
void Cvar_SetU_f( void )
{
	cvar_t	*v;

	if ( Cmd_Argc() != 3 )
	{
		Msg("usage: setu <variable> <value>\n");
		return;
	}

	Cvar_Set_f();
	v = Cvar_FindVar( Cmd_Argv(1));

	if( !v ) return;
	v->flags |= CVAR_USERINFO;
}

/*
============
Cvar_SetS_f

As Cvar_Set, but also flags it as userinfo
============
*/
void Cvar_SetS_f( void )
{
	cvar_t	*v;

	if ( Cmd_Argc() != 3 )
	{
		Msg("usage: sets <variable> <value>\n");
		return;
	}
	Cvar_Set_f();
	v = Cvar_FindVar( Cmd_Argv(1));

	if ( !v ) return;
	v->flags |= CVAR_SERVERINFO;
}

/*
============
Cvar_SetA_f

As Cvar_Set, but also flags it as archived
============
*/
void Cvar_SetA_f( void )
{
	cvar_t	*v;

	if ( Cmd_Argc() != 3 )
	{
		Msg("usage: seta <variable> <value>\n");
		return;
	}
	Cvar_Set_f();
	v = Cvar_FindVar( Cmd_Argv( 1 ) );

	if ( !v ) return;
	v->flags |= CVAR_ARCHIVE;
}

/*
============
Cvar_Reset_f
============
*/
void Cvar_Reset_f( void )
{
	if ( Cmd_Argc() != 2 )
	{
		Msg("usage: reset <variable>\n");
		return;
	}
	Cvar_Reset( Cmd_Argv( 1 ));
}

/*
============
Cvar_WriteVariables

Appends lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/
void Cvar_WriteVariables( file_t *f )
{
	cvar_t	*var;
	char	buffer[1024];

	for(var = cvar_vars; var; var = var->next)
	{
		if( var->flags & CVAR_ARCHIVE )
		{
			// write the latched value, even if it hasn't taken effect yet
			if (!var->latched_string ) sprintf(buffer, "seta %s \"%s\"\n", var->name, var->string);
			else sprintf(buffer, "seta %s \"%s\"\n", var->name, var->latched_string);
			FS_Printf (f, "%s", buffer);
		}
	}
}

/*
============
Cvar_List_f
============
*/
void Cvar_List_f( void )
{
	cvar_t	*var;
	char	*match;
	int	i = 0;

	if ( Cmd_Argc() > 1 ) match = Cmd_Argv(1);
	else match = NULL;

	for (var = cvar_vars; var; var = var->next, i++)
	{
		if (match && !Com_Filter(match, var->name, false))
			continue;

		if (var->flags & CVAR_SERVERINFO) Msg("S");
		else Msg(" ");

		if (var->flags & CVAR_USERINFO) Msg("U");
		else Msg(" ");

		if (var->flags & CVAR_READ_ONLY) Msg("R");
		else Msg(" ");

		if (var->flags & CVAR_INIT) Msg("I");
		else Msg(" ");

		if (var->flags & CVAR_ARCHIVE) Msg("A");
		else Msg(" ");

		if (var->flags & CVAR_LATCH) Msg("L");
		else Msg(" ");

		if (var->flags & CVAR_CHEAT) Msg("C");
		else Msg(" ");
		Msg (" %s \"%s\"\n", var->name, var->string);
	}
	Msg ("\n%i total cvars\n", i);
	Msg ("%i cvar indexes\n", cvar_numIndexes);
}

/*
============
Cvar_Restart_f

Resets all cvars to their hardcoded values
============
*/
void Cvar_Restart_f( void )
{
	cvar_t	*var;
	cvar_t	**prev;

	prev = &cvar_vars;

	while ( 1 )
	{
		var = *prev;
		if ( !var ) break;

		// don't mess with rom values, or some inter-module
		// communication will get broken (com_cl_running, etc)
		if ( var->flags & ( CVAR_READ_ONLY | CVAR_INIT ))
		{
			prev = &var->next;
			continue;
		}

		// throw out any variables the user created
		if ( var->flags & CVAR_USER_CREATED )
		{
			*prev = var->next;
			if( var->name ) Z_Free( var->name );
			if ( var->string ) Z_Free( var->string );
			if ( var->latched_string ) Z_Free( var->latched_string );
			if ( var->reset_string ) Z_Free( var->reset_string );
			if ( var->description ) Z_Free( var->description );

			// clear the var completely, since we
			// can't remove the index from the list
			memset( var, 0, sizeof( var ));
			continue;
		}

		Cvar_Set( var->name, var->reset_string );
		prev = &var->next;
	}
}

char *Cvar_BitInfo (int bit)
{
	static char	info[MAX_INFO_STRING];
	cvar_t	*var;

	info[0] = 0;

	for (var = cvar_vars; var; var = var->next)
	{
		if (var->flags & bit)
			Info_SetValueForKey (info, var->name, var->string);
	}
	return info;
}

// returns an info string containing all the CVAR_USERINFO cvars
char *Cvar_Userinfo (void)
{
	return Cvar_BitInfo (CVAR_USERINFO);
}

// returns an info string containing all the CVAR_SERVERINFO cvars
char *Cvar_Serverinfo (void)
{
	return Cvar_BitInfo (CVAR_SERVERINFO);
}

/*
============
Cvar_Init

Reads in all archived cvars
============
*/
void Cvar_Init (void)
{
	host_cheats = Cvar_Get("host_cheats", "1", CVAR_READ_ONLY | CVAR_SYSTEMINFO );

	Cmd_AddCommand ("toggle", Cvar_Toggle_f, "toggles a console variable's values (use for more info)" );
	Cmd_AddCommand ("set", Cvar_Set_f, "create or change the value of a console variable" );
	Cmd_AddCommand ("sets", Cvar_SetS_f, "create or change the value of a serverinfo variable");
	Cmd_AddCommand ("setu", Cvar_SetU_f, "create or change the value of a userinfo variable");
	Cmd_AddCommand ("seta", Cvar_SetA_f, "create or change the value of a console variable that will be saved to vars.rc");
	Cmd_AddCommand ("reset", Cvar_Reset_f, "reset any type variable to initial value" );
	Cmd_AddCommand ("cvarlist", Cvar_List_f, "display all console variables beginning with the specified prefix" );
	Cmd_AddCommand ("unsetall", Cvar_Restart_f, "reset all console variables to their default values" );

}
