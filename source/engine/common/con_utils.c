//=======================================================================
//			Copyright XashXT Group 2008 ©
//			con_utils.c - console helpers
//=======================================================================

#include "common.h"
#include "client.h"
#include "byteorder.h"

/*
=======================================================================

			FILENAME AUTOCOMPLETION
=======================================================================
*/
bool Cmd_CheckName( const char *name )
{
	if(!com.stricmp(Cmd_Argv( 0 ), name ))
		return true;
	if(!com.stricmp(Cmd_Argv(0), va("\\%s", name )))
		return true;
	return false;
}

/*
=====================================
Cmd_GetMapList

Prints or complete map filename
=====================================
*/
bool Cmd_GetMapList( const char *s, char *completedname, int length )
{
	search_t		*t;
	file_t		*f;
	string		message;
	string		matchbuf;
	byte		buf[MAX_SYSPATH]; // 1 kb
	int		i, nummaps;

	t = FS_Search(va("maps/%s*.bsp", s), true );
	if( !t ) return false;

	FS_FileBase(t->filenames[0], matchbuf ); 
	com.strncpy( completedname, matchbuf, length );
	if(t->numfilenames == 1) return true;

	for(i = 0, nummaps = 0; i < t->numfilenames; i++)
	{
		string		entfilename;
		int		ver = -1, lumpofs = 0, lumplen = 0;
		const char	*ext = FS_FileExtension( t->filenames[i] ); 
		script_t		*ents = NULL;

		if( com.stricmp( ext, "bsp" )) continue;

		com.strncpy( message, "^1error^7", sizeof( message ));
		f = FS_Open( t->filenames[i], "rb" );
	
		if( f )
		{
			Mem_Set( buf, 0, MAX_SYSPATH );
			FS_Read( f, buf, MAX_SYSPATH );
			if(!memcmp( buf, "IBSP", 4 ))
			{
				dheader_t *header = (dheader_t *)buf;
				ver = LittleLong(((int *)buf)[1]);

				switch(ver)
				{
				case 38:	// quake2
				case 39:	// xash3d
				case 46:	// quake3
				case 47:	// return to castle wolfenstein
					lumpofs = LittleLong( header->lumps[LUMP_ENTITIES].fileofs );
					lumplen = LittleLong( header->lumps[LUMP_ENTITIES].filelen );
					break;
				}
			}
			else
			{
				lump_t	ents; // quake1 entity lump
				Mem_Copy( &ents, buf + 4, sizeof( lump_t )); // skip first four bytes (version)
				ver = LittleLong(((int *)buf)[0]);

				switch( ver )
				{
				case 28:	// quake 1 beta
				case 29:	// quake 1 regular
				case 30:	// Half-Life regular
					lumpofs = LittleLong(ents.fileofs);
					lumplen = LittleLong(ents.filelen);
					break;
				default:
					ver = 0;
					break;
				}
			}

			com.strncpy( entfilename, t->filenames[i], sizeof( entfilename ));
			FS_StripExtension( entfilename );
			FS_DefaultExtension( entfilename, ".ent" );
			ents = Com_OpenScript( entfilename, NULL, 0 );

			if( !ents && lumplen >= 10 )
			{
				char *entities = NULL;
		
				FS_Seek( f, lumpofs, SEEK_SET );
				entities = (char *)Z_Malloc( lumplen + 1 );
				FS_Read( f, entities, lumplen );
				ents = Com_OpenScript( "ents", entities, lumplen + 1 );
				Mem_Free( entities ); // no reason to keep it
			}

			if( ents )
			{
				// if there are entities to parse, a missing message key just
				// means there is no title, so clear the message string now
				token_t	token;

				message[0] = 0;
				while( Com_ReadToken( ents, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token ))
				{
					if( !com.strcmp( token.string, "{" )) continue;
					else if(!com.strcmp( token.string, "}" )) break;
					else if(!com.strcmp( token.string, "message" ))
					{
						// get the message contents
						Com_ReadString( ents, false, message );
					}
				}
				Com_CloseScript( ents );
			}
		}


		if( f ) FS_Close(f);
		FS_FileBase( t->filenames[i], matchbuf );

		switch( ver )
		{
		case 28:  com.strncpy((char *)buf, "Quake1 beta", sizeof(buf)); break;
		case 29:  com.strncpy((char *)buf, "Quake1", sizeof(buf)); break;
		case 30:  com.strncpy((char *)buf, "Half-Life", sizeof(buf)); break;
		case 38:  com.strncpy((char *)buf, "Quake 2", sizeof(buf)); break;
		case 39:  com.strncpy((char *)buf, "Xash 3D", sizeof(buf)); break;
		case 46:  com.strncpy((char *)buf, "Quake 3", sizeof(buf)); break;
		case 47:  com.strncpy((char *)buf, "RTCW", sizeof(buf)); break;
		default:	com.strncpy((char *)buf, "??", sizeof(buf)); break;
		}
		Msg("%16s (%s) ^3%s^7\n", matchbuf, buf, message);
		nummaps++;
	}
	Msg("\n^3 %i maps found.\n", nummaps );
	Mem_Free( t );

	// cut shortestMatch to the amount common with s
	for( i = 0; matchbuf[i]; i++ )
	{
		if(com.tolower(completedname[i]) != com.tolower(matchbuf[i]))
			completedname[i] = 0;
	}
	return true;
}

/*
=====================================
Cmd_GetFontList

Prints or complete font filename
=====================================
*/
bool Cmd_GetFontList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numfonts;

	t = FS_Search(va("gfx/fonts/%s*.*", s ), true);
	if(!t) return false;

	FS_FileBase(t->filenames[0], matchbuf ); 
	if(completedname && length) com.strncpy( completedname, matchbuf, length );
	if( t->numfilenames == 1 ) return true;

	for(i = 0, numfonts = 0; i < t->numfilenames; i++)
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp(ext, "png" ) && com.stricmp(ext, "dds" ) && com.stricmp(ext, "tga" ))
			continue;
		FS_FileBase(t->filenames[i], matchbuf );
		Msg("%16s\n", matchbuf );
		numfonts++;
	}
	Msg("\n^3 %i fonts found.\n", numfonts );
	Mem_Free(t);

	// cut shortestMatch to the amount common with s
	if(completedname && length)
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if(com.tolower(completedname[i]) != com.tolower(matchbuf[i]))
				completedname[i] = 0;
		}
	}

	return true;
}

/*
=====================================
Cmd_GetDemoList

Prints or complete demo filename
=====================================
*/
bool Cmd_GetDemoList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numdems;

	t = FS_Search(va("demos/%s*.dem", s ), true);
	if(!t) return false;

	FS_FileBase(t->filenames[0], matchbuf ); 
	if(completedname && length) com.strncpy( completedname, matchbuf, length );
	if(t->numfilenames == 1) return true;

	for(i = 0, numdems = 0; i < t->numfilenames; i++)
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp(ext, "dem" )) continue;
		FS_FileBase(t->filenames[i], matchbuf );
		Msg("%16s\n", matchbuf );
		numdems++;
	}
	Msg("\n^3 %i demos found.\n", numdems );
	Mem_Free(t);

	// cut shortestMatch to the amount common with s
	if(completedname && length)
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if(com.tolower(completedname[i]) != com.tolower(matchbuf[i]))
				completedname[i] = 0;
		}
	}

	return true;
}

/*
=====================================
Cmd_GetSourceList

Prints or complete vm source folder name
=====================================
*/
bool Cmd_GetProgsList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numprogs;

	t = FS_Search(va("%s/%s*.dat", GI->vprogs_dir, s ), true);
	if( !t ) return false;

	FS_FileBase( t->filenames[0], matchbuf ); 
	if(completedname && length) com.strncpy( completedname, matchbuf, length );
	if( t->numfilenames == 1 ) return true;

	for(i = 0, numprogs = 0; i < t->numfilenames; i++)
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp(ext, "dat" )) continue;
		FS_FileBase(t->filenames[i], matchbuf );
		Msg("%16s\n", matchbuf );
		numprogs++;
	}
	Msg("\n^3 %i progs found.\n", numprogs );
	Mem_Free(t);

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if(com.tolower(completedname[i]) != com.tolower(matchbuf[i]))
				completedname[i] = 0;
		}
	}
	return true;
}

/*
=====================================
Cmd_GetProgsList

Prints or complete vm progs name
=====================================
*/
bool Cmd_GetSourceList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numworkspaces;

	// NOTE: we want ignore folders without "progs.src" inside
	t = FS_Search(va("%s/%s*", GI->source_dir, s ), true);
	if( !t ) return false;
	if( t->numfilenames == 1 && !FS_FileExists(va("%s/progs.src", t->filenames[0] )))
		return false;

	FS_FileBase( t->filenames[0], matchbuf ); 
	if(completedname && length) com.strncpy( completedname, matchbuf, length );
	if( t->numfilenames == 1 ) return true;

	for(i = 0, numworkspaces = 0; i < t->numfilenames; i++)
	{
		if(!FS_FileExists(va("%s/progs.src", t->filenames[i] )))
			continue; 
		FS_FileBase(t->filenames[i], matchbuf );
		Msg("%16s\n", matchbuf );
		numworkspaces++;
	}
	Msg("\n^3 %i workspaces found.\n", numworkspaces );
	Mem_Free(t);

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if(com.tolower(completedname[i]) != com.tolower(matchbuf[i]))
				completedname[i] = 0;
		}
	}
	return true;
}

/*
=====================================
Cmd_GetMovieList

Prints or complete movie filename
=====================================
*/
bool Cmd_GetMovieList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, nummovies;

	t = FS_Search(va("media/%s*.dpv", s ), true);
	if(!t) return false;

	FS_FileBase(t->filenames[0], matchbuf ); 
	if(completedname && length) com.strncpy( completedname, matchbuf, length );
	if(t->numfilenames == 1) return true;

	for(i = 0, nummovies = 0; i < t->numfilenames; i++)
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp( ext, "dpv" )) continue;
		FS_FileBase(t->filenames[i], matchbuf );
		Msg("%16s\n", matchbuf );
		nummovies++;
	}
	Msg("\n^3 %i movies found.\n", nummovies );
	Mem_Free(t);

	// cut shortestMatch to the amount common with s
	if(completedname && length)
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if(com.tolower(completedname[i]) != com.tolower(matchbuf[i]))
				completedname[i] = 0;
		}
	}

	return true;
}

/*
=====================================
Cmd_GetMusicList

Prints or complete background track filename
=====================================
*/
bool Cmd_GetMusicList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numtracks;

	t = FS_Search(va("media/%s*.ogg", s ), true);
	if(!t) return false;

	FS_FileBase(t->filenames[0], matchbuf ); 
	if(completedname && length) com.strncpy( completedname, matchbuf, length );
	if(t->numfilenames == 1) return true;

	for(i = 0, numtracks = 0; i < t->numfilenames; i++)
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp(ext, "ogg" )) continue;
		FS_FileBase(t->filenames[i], matchbuf );
		Msg("%16s\n", matchbuf );
		numtracks++;
	}
	Msg("\n^3 %i soundtracks found.\n", numtracks );
	Mem_Free(t);

	// cut shortestMatch to the amount common with s
	if(completedname && length)
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if(com.tolower(completedname[i]) != com.tolower(matchbuf[i]))
				completedname[i] = 0;
		}
	}

	return true;
}

/*
=====================================
Cmd_GetSoundList

Prints or complete sound filename
=====================================
*/
bool Cmd_GetSoundList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numsounds;
	const char	*snddir = "sound/"; // constant

	t = FS_Search(va("%s%s*.*", snddir, s ), true);
	if(!t) return false;

	com.strncpy( matchbuf, t->filenames[0] + com.strlen(snddir), MAX_STRING ); 
	FS_StripExtension( matchbuf ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if(t->numfilenames == 1) return true;

	for(i = 0, numsounds = 0; i < t->numfilenames; i++)
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if(com.stricmp(ext, "wav") && com.stricmp(ext, "ogg")) continue;
		com.strncpy( matchbuf, t->filenames[i] + com.strlen(snddir), MAX_STRING ); 
		FS_StripExtension( matchbuf );
		Msg("%16s\n", matchbuf );
		numsounds++;
	}
	Msg("\n^3 %i sounds found.\n", numsounds );
	Mem_Free(t);

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if(com.tolower(completedname[i]) != com.tolower(matchbuf[i]))
				completedname[i] = 0;
		}
	}

	return true;
}

/*
=====================================
Cmd_GetGameList

Prints or complete sound filename
=====================================
*/
bool Cmd_GetGamesList( const char *s, char *completedname, int length )
{
	int	i, numgamedirs;
	string	gamedirs[128];
	string	matchbuf;

	// compare gamelist with current keyword
	for( i = 0, numgamedirs = 0; i < GI->numgamedirs; i++ )
	{
		if(!com.strnicmp(GI->gamedirs[i], s, com.strlen(s)))
			com.strcpy( gamedirs[numgamedirs++], GI->gamedirs[i] ); 
	}

	if( !numgamedirs ) return false;
	com.strncpy( matchbuf, gamedirs[0], MAX_STRING ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( numgamedirs == 1) return true;

	for( i = 0; i < numgamedirs; i++ )
	{
		com.strncpy( matchbuf, gamedirs[i], MAX_STRING ); 
		Msg("%16s\n", matchbuf );
	}
	Msg("\n^3 %i games found.\n", numgamedirs );

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if(com.tolower(completedname[i]) != com.tolower(matchbuf[i]))
				completedname[i] = 0;
		}
	}
	return true;
}

bool Cmd_CheckMapsList( void )
{
	byte	buf[MAX_SYSPATH]; // 1 kb
	char	*buffer;
	string	result;
	search_t	*t;
	file_t	*f;
	int	i;

	if(FS_FileExists( "scripts/maps.lst" ))
		return true; // exist 

	t = FS_Search( "maps/*.bsp", false );
	if(!t) return false;

	buffer = Z_Malloc( t->numfilenames * 2 * sizeof( result ));
	for( i = 0; i < t->numfilenames; i++ )
	{
		script_t		*ents = NULL;
		int		ver = -1, lumpofs = 0, lumplen = 0;
		string		mapname, message, entfilename;

		f = FS_Open( t->filenames[i], "rb" );
		FS_FileBase( t->filenames[i], mapname );

		if( f )
		{
			int num_spawnpoints = 0;

			Mem_Set( buf, 0, MAX_SYSPATH );
			FS_Read( f, buf, MAX_SYSPATH );
			if(!memcmp( buf, "IBSP", 4 ))
			{
				dheader_t *header = (dheader_t *)buf;
				ver = LittleLong(((int *)buf)[1]);
				switch(ver)
				{
				case 38:	// quake2
				case 39:	// xash
				case 46:	// quake3
				case 47:	// return to castle wolfenstein
					lumpofs = LittleLong(header->lumps[LUMP_ENTITIES].fileofs);
					lumplen = LittleLong(header->lumps[LUMP_ENTITIES].filelen);
					break;
				}
			}
			else
			{
				lump_t	ents; // quake1 entity lump
				Mem_Copy(&ents, buf + 4, sizeof(lump_t)); // skip first four bytes (version)
				ver = LittleLong(((int *)buf)[0]);

				switch( ver )
				{
				case 28:	// quake 1 beta
				case 29:	// quake 1 regular
				case 30:	// Half-Life regular
					lumpofs = LittleLong(ents.fileofs);
					lumplen = LittleLong(ents.filelen);
					break;
				default:
					ver = 0;
					break;
				}
			}
			com.strncpy( entfilename, t->filenames[i], sizeof( entfilename ));
			FS_StripExtension( entfilename );
			FS_DefaultExtension( entfilename, ".ent" );
			ents = Com_OpenScript( entfilename, NULL, 0 );

			if( !ents && lumplen >= 10 )
			{
				char *entities = NULL;
		
				FS_Seek( f, lumpofs, SEEK_SET );
				entities = (char *)Z_Malloc( lumplen + 1 );
				FS_Read( f, entities, lumplen );
				ents = Com_OpenScript( "ents", entities, lumplen + 1 );
				Mem_Free( entities ); // no reason to keep it
			}
			if( ents )
			{
				// if there are entities to parse, a missing message key just
				// means there is no title, so clear the message string now
				token_t	token;

				message[0] = 0;
				com.strncpy( message, "No Title", MAX_STRING );

				while( Com_ReadToken( ents, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token ))
				{
					if( !com.strcmp( token.string, "{" )) continue;
					else if( !com.strcmp( token.string, "}" )) break;
					else if( !com.strcmp( token.string, "message" ))
					{
						// get the message contents
						Com_ReadString( ents, 0, message );
					}
					else if( !com.strcmp( token.string, "classname" ))
					{
						Com_ReadToken( ents, 0, &token );
						if(!com.strcmp( token.string, "info_player_deatchmatch" ))
							num_spawnpoints++;
						else if(!com.strcmp( token.string, "info_player_start" ))
							num_spawnpoints++;
					}
					if( num_spawnpoints > 0 ) break; // valid map
				}
				Com_CloseScript( ents );
			}

			if( f ) FS_Close(f);

			// format: mapname "maptitle"\n
			com.sprintf( result, "%s \"%s\"\n", mapname, message );
			com.strcat( buffer, result ); // add new string
		}
	}
	if( t ) Mem_Free( t ); // free search result

	// write generated maps.lst
	if( FS_WriteFile( "scripts/maps.lst", buffer, com.strlen( buffer )))
	{
          	if( buffer ) Mem_Free( buffer );
		return true;
	}
	return false;
}

autocomplete_list_t cmd_list[] =
{
{ "prvm_printfucntion", Cmd_GetProgsList },
{ "prvm_edictcount", Cmd_GetProgsList },
{ "prvm_globalset", Cmd_GetProgsList },
{ "prvm_edictset", Cmd_GetProgsList },
{ "prvm_profile", Cmd_GetProgsList },
{ "prvm_globals", Cmd_GetProgsList },
{ "prvm_edicts", Cmd_GetProgsList },
{ "prvm_fields", Cmd_GetProgsList },
{ "prvm_global", Cmd_GetProgsList },
{ "prvm_edict", Cmd_GetProgsList },
{ "playsound", Cmd_GetSoundList },
{ "changelevel", Cmd_GetMapList },
{ "playdemo", Cmd_GetDemoList, },
{ "compile", Cmd_GetSourceList },
{ "setfont", Cmd_GetFontList, },
{ "music", Cmd_GetSoundList, },
{ "movie", Cmd_GetMovieList },
{ "game", Cmd_GetGamesList },
{ "map", Cmd_GetMapList },
{ NULL }, // termiantor
};

/*
============
Cmd_WriteVariables

Appends lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/

static void Cmd_WriteCvar(const char *name, const char *string, const char *desc, void *f )
{
	//if(!desc) return; // ignore fantom cvars
	FS_Printf(f, "seta %s \"%s\"\n", name, string );
}

static void Cmd_WriteHelp(const char *name, const char *unused, const char *desc, void *f )
{
	if( !desc ) return;				// ignore fantom cmds
	if( !com.strcmp( desc, "" )) return;		// blank description
	if( name[0] == '+' || name[0] == '-' ) return;	// key bindings	
	FS_Printf( f, "%s\t\t\t\"%s\"\n", name, desc );
}

void Cmd_WriteVariables( file_t *f )
{
	FS_Printf( f, "unsetall\n" );
	Cvar_LookupVars( CVAR_ARCHIVE, NULL, f, Cmd_WriteCvar ); 
}

/*
===============
CL_WriteConfiguration

Writes key bindings and archived cvars to config.cfg
===============
*/
void CL_WriteConfiguration( void )
{
	file_t	*f;

	if( !cls.initialized ) return;

	f = FS_Open("scripts/config/keys.rc", "w");
	if(f)
	{
		FS_Printf (f, "//=======================================================================\n");
		FS_Printf (f, "//\t\t\tCopyright XashXT Group 2007 ©\n");
		FS_Printf (f, "//\t\t\tkeys.rc - current key bindings\n");
		FS_Printf (f, "//=======================================================================\n");
		Key_WriteBindings(f);
		FS_Close (f);
	}
	else MsgDev( D_ERROR, "Couldn't write keys.rc.\n");

	f = FS_Open("scripts/config/vars.rc", "w");
	if(f)
	{
		FS_Printf (f, "//=======================================================================\n");
		FS_Printf (f, "//\t\t\tCopyright XashXT Group 2007 ©\n");
		FS_Printf (f, "//\t\t\tvars.rc - archive of cvars\n");
		FS_Printf (f, "//=======================================================================\n");
		Cmd_WriteVariables(f);
		FS_Close (f);	
	}
	else MsgDev( D_ERROR, "Couldn't write vars.rc.\n");
}

void Key_EnumCmds_f( void )
{
	file_t *f = FS_Open( "docs/help.txt", "w" );
	if( f )
	{
		FS_Printf( f, "//=======================================================================\n");
		FS_Printf( f, "//\t\t\tCopyright XashXT Group 2007 ©\n");
		FS_Printf( f, "//\t\thelp.txt - xash commands and console variables\n");
		FS_Printf( f, "//=======================================================================\n");

		FS_Printf( f, "\n\n\t\t\tconsole variables\n\n");
		Cvar_LookupVars( 0, NULL, f, Cmd_WriteHelp ); 
		FS_Printf( f, "\n\n\t\t\tconsole commands\n\n");
		Cmd_LookupCmds( NULL, f, Cmd_WriteHelp ); 
  		FS_Printf( f, "\n\n");
		FS_Close( f );
	}
	else MsgDev( D_ERROR, "Couldn't write help.txt.\n");
	Msg( "write docs/help.txt" );
}