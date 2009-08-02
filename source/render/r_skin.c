/*
Copyright (C) 2002-2007 Victor Luchits

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

#include "r_local.h"

typedef struct
{
	char				*meshname;
	ref_shader_t			*shader;
} mesh_shader_pair_t;

typedef struct skinfile_s
{
	char				*name;

	mesh_shader_pair_t	*pairs;
	int					numpairs;
} skinfile_t;

static skinfile_t r_skinfiles[MAX_SKINFILES];
static byte *r_skinsPool;

// Com_ParseExt it's temporary stuff
#define	MAX_TOKEN_CHARS	1024
char	com_token[MAX_TOKEN_CHARS];

/*
==============
COM_ParseExt

Parse a token out of a string
==============
*/
char *COM_ParseExt( const char **data_p, bool nl )
{
	int		c;
	int		len;
	const char	*data;
	bool 		newlines = false;

	data = *data_p;
	len = 0;
	com_token[0] = 0;

	if (!data)
	{
		*data_p = NULL;
		return "";
	}

// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
		{
			*data_p = NULL;
			return "";
		}
		if (c == '\n')
			newlines = true;
		data++;
	}

	if ( newlines && !nl )
	{
		*data_p = data;
		return com_token;
	}

	// skip // comments
	if (c == '/' && data[1] == '/')
	{
		data += 2;

		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	// skip /* */ comments
	if (c == '/' && data[1] == '*')
	{
		data += 2;

		while (1)
		{
			if (!*data)
				break;
			if (*data != '*' || *(data+1) != '/')
				data++;
			else
			{
				data += 2;
				break;
			}
		}
		goto skipwhite;
	}

	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				if (len == MAX_TOKEN_CHARS)
					len = 0;
				com_token[len] = 0;
				*data_p = data;
				return com_token;
			}
			if (len < MAX_TOKEN_CHARS)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c>32);

	if (len == MAX_TOKEN_CHARS)
		len = 0;
	com_token[len] = 0;

	*data_p = data;
	return com_token;
}

/*
================
R_InitSkinFiles
================
*/
void R_InitSkinFiles( void )
{
	r_skinsPool = Mem_AllocPool( "Skins" );

	memset( r_skinfiles, 0, sizeof( r_skinfiles ) );
}

/*
================
SkinFile_CopyString
================
*/
static char *SkinFile_CopyString( const char *in )
{
	char *out;

	out = Mem_Alloc( r_skinsPool, ( strlen( in ) + 1 ) );
	strcpy( out, in );

	return out;
}

/*
================
SkinFile_FreeSkinFile
================
*/
static void SkinFile_FreeSkinFile( skinfile_t *skinfile )
{
	int i;

	for( i = 0; i < skinfile->numpairs; i++ )
		Mem_Free( skinfile->pairs[i].meshname );

	Mem_Free( skinfile->pairs );
	Mem_Free( skinfile->name );

	memset( skinfile, 0, sizeof( skinfile_t ) );
}

/*
================
R_FindShaderForSkinFile
================
*/
ref_shader_t *R_FindShaderForSkinFile( const skinfile_t *skinfile, const char *meshname )
{
	int i;
	mesh_shader_pair_t *pair;

	if( !skinfile || !skinfile->numpairs )
		return NULL;

	for( i = 0, pair = skinfile->pairs; i < skinfile->numpairs; i++, pair++ )
	{
		if( !com.stricmp( pair->meshname, meshname ) )
			return pair->shader;
	}

	return NULL;
}

/*
================
SkinFile_ParseBuffer
================
*/
static int SkinFile_ParseBuffer( char *buffer, mesh_shader_pair_t *pairs )
{
	int numpairs;
	char *ptr, *t, *token;

	ptr = buffer;
	numpairs = 0;

	while( ptr )
	{
		token = COM_ParseExt( &ptr, false );
		if( !token[0] )
			continue;

		t = strchr( token, ',' );
		if( !t )
			continue;
		if( *( t+1 ) == '\0' || *( t+1 ) == '\n' )
			continue;

		if( pairs )
		{
			*t = 0;
			pairs[numpairs].meshname = SkinFile_CopyString( token );
			pairs[numpairs].shader = R_RegisterSkin( token + strlen( token ) + 1 );
		}

		numpairs++;
	}

	return numpairs;
}

/*
================
R_RegisterSkinFile
================
*/
skinfile_t *R_RegisterSkinFile( const char *name )
{
	string	filename;
	int i;
	char *buffer;
	skinfile_t *skinfile;

	com.strncpy( filename, name, sizeof( filename ) );
	FS_DefaultExtension( filename, ".skin" );

	for( i = 0, skinfile = r_skinfiles; i < MAX_SKINFILES; i++, skinfile++ )
	{
		if( !skinfile->name )
			break;
		if( !com.stricmp( skinfile->name, filename ) )
			return skinfile;
	}

	if( i == MAX_SKINFILES )
	{
		MsgDev( D_ERROR, "R_SkinFile_Load: Skin files limit exceeded\n" );
		return NULL;
	}

	if((buffer = FS_LoadFile( filename, NULL )) == NULL )
	{
		MsgDev( D_WARN, "R_SkinFile_Load: Failed to load %s\n", name );
		return NULL;
	}

	skinfile = &r_skinfiles[i];
	skinfile->name = SkinFile_CopyString( filename );

	skinfile->numpairs = SkinFile_ParseBuffer( buffer, NULL );
	if( skinfile->numpairs )
	{
		skinfile->pairs = Mem_Alloc( r_skinsPool, skinfile->numpairs * sizeof( mesh_shader_pair_t ) );
		SkinFile_ParseBuffer( buffer, skinfile->pairs );
	}
	else
	{
		MsgDev( D_WARN, "R_SkinFile_Load: no mesh/shader pairs in %s\n", name );
	}

	Mem_Free( buffer );

	return skinfile;
}

/*
================
R_ShutdownSkinFiles
================
*/
void R_ShutdownSkinFiles( void )
{
	int i;
	skinfile_t *skinfile;

	if( !r_skinsPool )
		return;

	for( i = 0, skinfile = r_skinfiles; i < MAX_SKINFILES; i++, skinfile++ )
	{
		if( !skinfile->name )
			break;

		if( skinfile->numpairs )
			SkinFile_FreeSkinFile( skinfile );

		Mem_Free( skinfile->name );
	}

	Mem_FreePool( &r_skinsPool );
}
