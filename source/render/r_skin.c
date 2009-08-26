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

#define SkinFile_CopyString( str )	com.stralloc( r_skinsPool, str, __FILE__, __LINE__ )

typedef struct
{
	char		*meshname;
	ref_shader_t	*shader;
} mesh_shader_pair_t;

typedef struct skinfile_s
{
	char		*name;
	mesh_shader_pair_t	*pairs;
	int		numpairs;
} skinfile_t;

static skinfile_t	r_skinfiles[MAX_SKINFILES];
static byte	*r_skinsPool;

/*
================
R_InitSkinFiles
================
*/
void R_InitSkinFiles( void )
{
	r_skinsPool = Mem_AllocPool( "Skins" );
	Mem_Set( r_skinfiles, 0, sizeof( r_skinfiles ));
}

/*
================
SkinFile_FreeSkinFile
================
*/
static void SkinFile_FreeSkinFile( skinfile_t *skinfile )
{
	int	i;

	for( i = 0; i < skinfile->numpairs; i++ )
		Mem_Free( skinfile->pairs[i].meshname );

	Mem_Free( skinfile->pairs );
	Mem_Free( skinfile->name );
	Mem_Set( skinfile, 0, sizeof( skinfile_t ));
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
	int	numpairs;
	string	skinname;
	string	meshname;
	script_t	*script;
	token_t	tok;

	script = Com_OpenScript( "skinfile", buffer, com.strlen( buffer ));
	numpairs = 0;

	while( 1 )
	{
		if( !Com_ReadToken( script, SC_ALLOW_NEWLINES, &tok )) // skip tag
			break;
		if( !com.strcmp( tok.string, "," ))
		{
			if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES, &tok ))
				continue;	// tag without shadername
			com.strncpy( skinname, tok.string, sizeof( skinname ));
			FS_StripExtension( skinname );
		}
		else
		{
			com.strncpy( meshname, tok.string, sizeof( meshname ));
			continue;	// waiting for ','
		}

		if( pairs )
		{
			pairs[numpairs].meshname = SkinFile_CopyString( meshname );
			pairs[numpairs].shader = R_LoadShader( skinname, SHADER_ALIAS_MD3, false, 0, SHADER_INVALID );
		}
		numpairs++;
	}
	Com_CloseScript( script );

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
