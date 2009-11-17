//=======================================================================
//			Copyright XashXT Group 2008 ©
//			con_utils.c - console helpers
//=======================================================================

#include "common.h"
#include "client.h"
#include "byteorder.h"
#include "const.h"

/*
=======================================================================

			FILENAME AUTOCOMPLETION
=======================================================================
*/
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

	t = FS_Search( va( "maps/%s*.bsp", s ), true );
	if( !t ) return false;

	FS_FileBase( t->filenames[0], matchbuf ); 
	com.strncpy( completedname, matchbuf, length );
	if( t->numfilenames == 1 ) return true;

	for( i = 0, nummaps = 0; i < t->numfilenames; i++ )
	{
		const char	*data = NULL;
		char		entfilename[CS_SIZE];
		int		ver = -1, lumpofs = 0, lumplen = 0;
		const char	*ext = FS_FileExtension( t->filenames[i] ); 
		script_t		*ents = NULL;

		if( com.stricmp( ext, "bsp" )) continue;
		com.strncpy( message, "^1error^7", sizeof( message ));
		f = FS_Open( t->filenames[i], "rb" );
	
		if( f )
		{
			dheader_t	*header;

			Mem_Set( buf, 0, MAX_SYSPATH );
			FS_Read( f, buf, MAX_SYSPATH );
                              
			switch( LittleLong(*(uint *)buf) )
			{
			case IDBSPMODHEADER:
			case RBBSPMODHEADER:
			case QFBSPMODHEADER:
			case XRBSPMODHEADER:
				header = (dheader_t *)buf;
				ver = LittleLong(((int *)buf)[1]);

				switch( ver )
				{
				case Q3IDBSP_VERSION:	// quake3 arena
				case RTCWBSP_VERSION:	// return to castle wolfenstein
				case RFIDBSP_VERSION:	// raven or qfusion bsp
				case XRIDBSP_VERSION:	// x-real engine
					lumpofs = LittleLong( header->lumps[LUMP_ENTITIES].fileofs );
					lumplen = LittleLong( header->lumps[LUMP_ENTITIES].filelen );
					break;
				}
				break;
			}

			com.strncpy( entfilename, t->filenames[i], sizeof( entfilename ));
			FS_StripExtension( entfilename );
			FS_DefaultExtension( entfilename, ".ent" );
			ents = Com_OpenScript( entfilename, NULL, 0 );

			if( !ents && lumplen >= 10 )
			{
				char *entities = NULL;
		
				FS_Seek( f, lumpofs, SEEK_SET );
				entities = (char *)Mem_Alloc( cls.mempool, lumplen + 1 );
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
		case Q3IDBSP_VERSION:
			com.strncpy( buf, "Quake3 Arena", sizeof( buf ));
			break;
		case RTCWBSP_VERSION:
			com.strncpy( buf, "Return To Castle Wolfenstein", sizeof( buf ));
			break;
		case RFIDBSP_VERSION:
			com.strncpy( buf, "Soldier Of Fortune 2", sizeof( buf ));
			break;
		case XRIDBSP_VERSION:
			com.strncpy( buf, "X-Real Engine format", sizeof( buf ));
			break;
		default:	com.strncpy( buf, "??", sizeof( buf )); break;
		}
		Msg( "%16s (%s) ^3%s^7\n", matchbuf, buf, message );
		nummaps++;
	}
	Msg( "\n^3 %i maps found.\n", nummaps );
	Mem_Free( t );

	// cut shortestMatch to the amount common with s
	for( i = 0; matchbuf[i]; i++ )
	{
		if(com.tolower( completedname[i] ) != com.tolower( matchbuf[i] ))
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

	FS_FileBase( t->filenames[0], matchbuf ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( t->numfilenames == 1 ) return true;

	for( i = 0, numfonts = 0; i < t->numfilenames; i++ )
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp( ext, "png" ) && com.stricmp( ext, "dds" ) && com.stricmp( ext, "tga" ))
			continue;
		FS_FileBase( t->filenames[i], matchbuf );
		Msg( "%16s\n", matchbuf );
		numfonts++;
	}
	Msg( "\n^3 %i fonts found.\n", numfonts );
	Mem_Free( t );

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if(com.tolower( completedname[i] ) != com.tolower( matchbuf[i] ))
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

	t = FS_Search( va( "demos/%s*.dem", s ), true);
	if( !t ) return false;

	FS_FileBase( t->filenames[0], matchbuf ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( t->numfilenames == 1 ) return true;

	for( i = 0, numdems = 0; i < t->numfilenames; i++ )
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp( ext, "dem" )) continue;
		FS_FileBase( t->filenames[i], matchbuf );
		Msg( "%16s\n", matchbuf );
		numdems++;
	}
	Msg( "\n^3 %i demos found.\n", numdems );
	Mem_Free( t );

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if( com.tolower( completedname[i] ) != com.tolower( matchbuf[i] ))
				completedname[i] = 0;
		}
	}
	return true;
}

/*
=====================================
Cmd_GetProgsList

Prints or complete vm source folder name
=====================================
*/
bool Cmd_GetProgsList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numprogs;

	t = FS_Search( va( "bin%s*.dat", s ), true );
	if( !t ) return false;

	FS_FileBase( t->filenames[0], matchbuf ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( t->numfilenames == 1 ) return true;

	for( i = 0, numprogs = 0; i < t->numfilenames; i++ )
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp( ext, "dat" )) continue;
		FS_FileBase( t->filenames[i], matchbuf );
		Msg( "%16s\n", matchbuf );
		numprogs++;
	}
	Msg( "\n^3 %i progs found.\n", numprogs );
	Mem_Free( t );

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if( com.tolower( completedname[i] ) != com.tolower( matchbuf[i] ))
				completedname[i] = 0;
		}
	}
	return true;
}

/*
=====================================
Cmd_GetSourceList

Prints or complete vm progs name
=====================================
*/
bool Cmd_GetSourceList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numworkspaces;

	// NOTE: we want ignore folders without "progs.src" inside
	t = FS_Search(va("%s/%s*", GI->vsrcdir, s ), true);
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
Cmd_GetSavesList

Prints or complete movie filename
=====================================
*/
bool Cmd_GetSavesList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numsaves;

	t = FS_Search( va( "save/%s*.bin", s ), true );
	if( !t ) return false;

	FS_FileBase( t->filenames[0], matchbuf ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( t->numfilenames == 1 ) return true;

	for( i = 0, numsaves = 0; i < t->numfilenames; i++ )
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp( ext, "bin" )) continue;
		FS_FileBase( t->filenames[i], matchbuf );
		Msg( "%16s\n", matchbuf );
		numsaves++;
	}
	Msg( "\n^3 %i saves found.\n", numsaves );
	Mem_Free( t );

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if( com.tolower( completedname[i] ) != com.tolower( matchbuf[i] ))
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
bool Cmd_GetStringTablesList( const char *s, char *completedname, int length )
{
	int	i, numtables;
	string	tables[MAX_STRING_TABLES];
	string	matchbuf;
	const char *name;

	// compare gamelist with current keyword
	for( i = 0, numtables = 0; i < MAX_STRING_TABLES; i++ )
	{
		name = StringTable_GetName( i );
		if( name && ( *s == '*' || !com.strnicmp( name, s, com.strlen( s ))))
			com.strcpy( tables[numtables++], name ); 
	}
	if( !numtables ) return false;

	com.strncpy( matchbuf, tables[0], MAX_STRING ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( numtables == 1 ) return true;

	for( i = 0; i < numtables; i++ )
	{
		com.strncpy( matchbuf, tables[i], MAX_STRING ); 
		Msg("%16s\n", matchbuf );
	}
	Msg( "\n^3 %i stringtables found.\n", numtables );

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if( com.tolower( completedname[i]) != com.tolower( matchbuf[i] ))
				completedname[i] = 0;
		}
	}
	return true;
}

/*
=====================================
Cmd_GetItemsList

Prints or complete item classname
=====================================
*/
bool Cmd_GetItemsList( const char *s, char *completedname, int length )
{
	search_t		*t;
	string		matchbuf;
	int		i, numitems;

	t = FS_Search( va("scripts/items/%s*.txt", s ), true );
	if( !t ) return false;

	FS_FileBase( t->filenames[0], matchbuf ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( t->numfilenames == 1 ) return true;

	for(i = 0, numitems = 0; i < t->numfilenames; i++)
	{
		const char *ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp(ext, "txt" )) continue;
		FS_FileBase( t->filenames[i], matchbuf );
		Msg( "%16s\n", matchbuf );
		numitems++;
	}
	Msg("\n^3 %i items found.\n", numitems );
	Mem_Free( t );

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
Cmd_GetTexturemodes

Prints or complete sound filename
=====================================
*/
bool Cmd_GetTexturemodes( const char *s, char *completedname, int length )
{
	int	i, numtexturemodes;
	string	texturemodes[6];	// keep an actual ( sizeof( gl_texturemode) / sizeof( gl_texturemode[0] ))
	string	matchbuf;

	const char *gl_texturemode[] =
	{
	"GL_LINEAR",
	"GL_LINEAR_MIPMAP_LINEAR",
	"GL_LINEAR_MIPMAP_NEAREST",
	"GL_NEAREST",
	"GL_NEAREST_MIPMAP_LINEAR",
	"GL_NEAREST_MIPMAP_NEAREST",
	};

	// compare gamelist with current keyword
	for( i = 0, numtexturemodes = 0; i < 6; i++ )
	{
		if(( *s == '*' ) || !com.strnicmp( gl_texturemode[i], s, com.strlen( s )))
			com.strcpy( texturemodes[numtexturemodes++], gl_texturemode[i] ); 
	}

	if( !numtexturemodes ) return false;
	com.strncpy( matchbuf, gl_texturemode[0], MAX_STRING ); 
	if( completedname && length ) com.strncpy( completedname, matchbuf, length );
	if( numtexturemodes == 1 ) return true;

	for( i = 0; i < numtexturemodes; i++ )
	{
		com.strncpy( matchbuf, texturemodes[i], MAX_STRING ); 
		Msg( "%16s\n", matchbuf );
	}
	Msg("\n^3 %i filters found.\n", numtexturemodes );

	// cut shortestMatch to the amount common with s
	if( completedname && length )
	{
		for( i = 0; matchbuf[i]; i++ )
		{
			if( com.tolower( completedname[i] ) != com.tolower( matchbuf[i] ))
				completedname[i] = 0;
		}
	}
	return true;
}

/*
=====================================
Cmd_GetGameList

Prints or complete gamedir name
=====================================
*/
bool Cmd_GetGamesList( const char *s, char *completedname, int length )
{
	int	i, numgamedirs;
	string	gamedirs[128];
	string	matchbuf;

	// compare gamelist with current keyword
	for( i = 0, numgamedirs = 0; i < SI->numgames; i++ )
	{
		if(( *s == '*' ) || !com.strnicmp( SI->games[i]->gamefolder, s, com.strlen( s )))
			com.strcpy( gamedirs[numgamedirs++], SI->games[i]->gamefolder ); 
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

	if( FS_FileExists( "scripts/maps.lst" ))
		return true; // exist 

	t = FS_Search( "maps/*.bsp", false );
	if( !t ) return false;

	buffer = Mem_Alloc( cls.mempool, t->numfilenames * 2 * sizeof( result ));
	for( i = 0; i < t->numfilenames; i++ )
	{
		script_t		*ents = NULL;
		int		ver = -1, lumpofs = 0, lumplen = 0;
		string		mapname, message, entfilename;
		const char	*ext = FS_FileExtension( t->filenames[i] ); 

		if( com.stricmp( ext, "bsp" )) continue;
		f = FS_Open( t->filenames[i], "rb" );
		FS_FileBase( t->filenames[i], mapname );

		if( f )
		{
			int	num_spawnpoints = 0;
			dheader_t	*header;

			Mem_Set( buf, 0, MAX_SYSPATH );
			FS_Read( f, buf, MAX_SYSPATH );
                              
			switch( LittleLong(*(uint *)buf) )
			{
			case IDBSPMODHEADER:
			case RBBSPMODHEADER:
			case QFBSPMODHEADER:
			case XRBSPMODHEADER:
				header = (dheader_t *)buf;
				ver = LittleLong(((int *)buf)[1]);

				switch( ver )
				{
				case Q3IDBSP_VERSION:	// quake3 arena
				case RTCWBSP_VERSION:	// return to castle wolfenstein
				case RFIDBSP_VERSION:	// raven or qfusion bsp
				case XRIDBSP_VERSION:	// x-real engine
					lumpofs = LittleLong( header->lumps[LUMP_ENTITIES].fileofs );
					lumplen = LittleLong( header->lumps[LUMP_ENTITIES].filelen );
					break;
				}
				break;
			}

			com.strncpy( entfilename, t->filenames[i], sizeof( entfilename ));
			FS_StripExtension( entfilename );
			FS_DefaultExtension( entfilename, ".ent" );
			ents = Com_OpenScript( entfilename, NULL, 0 );

			if( !ents && lumplen >= 10 )
			{
				char *entities = NULL;
		
				FS_Seek( f, lumpofs, SEEK_SET );
				entities = (char *)Mem_Alloc( cls.mempool, lumplen + 1 );
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
						if( !com.strcmp( token.string, GI->dm_entity ))
							num_spawnpoints++;
						else if( !com.strcmp( token.string, GI->sp_entity ))
							num_spawnpoints++;
					}
					if( num_spawnpoints > 1 ) break; // valid map
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
{ "gl_texturemode", Cmd_GetTexturemodes },
{ "stringlist", Cmd_GetStringTablesList },
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
{ "give", Cmd_GetItemsList },
{ "drop", Cmd_GetItemsList },
{ "game", Cmd_GetGamesList },
{ "save", Cmd_GetSavesList },
{ "load", Cmd_GetSavesList },
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
	if( !desc ) return; // ignore cvars without description (fantom variables)
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
Host_WriteConfig

Writes key bindings and archived cvars to vars.rc
===============
*/
void Host_WriteConfig( void )
{
	file_t	*f;

	if( !cls.initialized ) return;

	f = FS_Open( "config/keys.rc", "w" );
	if( f )
	{
		FS_Printf( f, "//=======================================================================\n" );
		FS_Printf( f, "//\t\t\tCopyright XashXT Group %s ©\n", timestamp( TIME_YEAR_ONLY ));
		FS_Printf( f, "//\t\t\tkeys.rc - current key bindings\n" );
		FS_Printf( f, "//=======================================================================\n" );
		Key_WriteBindings( f );
		FS_Close( f );
	}
	else MsgDev( D_ERROR, "Couldn't write keys.rc.\n" );

	f = FS_Open( "config/vars.rc", "w" );
	if( f )
	{
		FS_Printf( f, "//=======================================================================\n");
		FS_Printf( f, "//\t\t\tCopyright XashXT Group %s ©\n", timestamp( TIME_YEAR_ONLY ));
		FS_Printf( f, "//\t\t\tvars.rc - archive of cvars\n" );
		FS_Printf( f, "//=======================================================================\n" );
		Cmd_WriteVariables(f);
		FS_Close (f);	
	}
	else MsgDev( D_ERROR, "Couldn't write vars.rc.\n" );
}

/*
===============
Host_WriteConfig

Writes key bindings and archived cvars to basevars.rc
===============
*/
void Host_WriteDefaultConfig( void )
{
	file_t	*f;
	bool	hasChanged = false;

	if( !cls.initialized ) return;

	if( !FS_FileExists( "config/basekeys.rc" ) || !FS_FileExists( "config/basevars.rc" ))
		Cmd_ExecuteString( "unsetall\n" );

	if( !FS_FileExists( "config/basekeys.rc" ) && (f = FS_Open( "config/basekeys.rc", "w" )) != NULL )
	{
		FS_Printf( f, "//=======================================================================\n" );
		FS_Printf( f, "//\t\t\tCopyright XashXT Group %s ©\n", timestamp( TIME_YEAR_ONLY ));
		FS_Printf( f, "//\t\tbasekeys.rc - default key bindings\n" );
		FS_Printf( f, "//=======================================================================\n" );
		Key_WriteBindings( f );
		FS_Close( f );
		hasChanged = true;
	}

	if( !FS_FileExists( "config/basevars.rc" ) && (f = FS_Open( "config/basevars.rc", "w" )) != NULL )
	{
		FS_Printf( f, "//=======================================================================\n" );
		FS_Printf( f, "//\t\t\tCopyright XashXT Group %s ©\n", timestamp( TIME_YEAR_ONLY ));
		FS_Printf( f, "//\t\tbasevars.rc - archive of default cvar states\n" );
		FS_Printf( f, "//=======================================================================\n" );
		Cmd_WriteVariables( f );
		FS_Close( f );	
		hasChanged = true;
	}

	if( hasChanged )
	{
		// restart user config
		Cbuf_AddText( "exec keys.rc\n" );
		Cbuf_AddText( "exec vars.rc\n" );
		Cbuf_Execute();
	}
}

void Key_EnumCmds_f( void )
{
	file_t *f = FS_Open( "docs/help.txt", "w" );
	if( f )
	{
		FS_Printf( f, "//=======================================================================\n");
		FS_Printf( f, "//\t\t\tCopyright XashXT Group %s ©\n", timestamp( TIME_YEAR_ONLY ));
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
	Msg( "write docs/help.txt\n" );
}