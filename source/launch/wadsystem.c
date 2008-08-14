//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        wadsystem.c - game wad filesystem
//=======================================================================

#include "launch.h"
#include "filesystem.h"
#include "byteorder.h"

typedef struct wfile_s
{
	char		name[64];
	int		infotableofs;
	int		numlumps;
	int		mode;
	file_t		*file;
	dlumpinfo_t	*lumps;
};

typedef struct searchwad_s
{
	wfile_t		*wad;
	struct searchwad_s	*next;
} searchwad_t;

byte *fs_searchwads = NULL;
bool fs_use_wads = false; // some utilities needs this

/*
=============================================================================

WADSYSTEM PRIVATE COMMON FUNCTIONS

=============================================================================
*/
static void W_FileBase( const char *in, char *out )
{
	_FS_FileBase( in, out, false );
}

static dlumpinfo_t *W_FindLump( wfile_t *wad, const char *name, const char matchtype )
{
	bool		anytype = false;
	int		i;
		
	if( !wad || !wad->lumps ) return NULL;
	if( matchtype == TYPE_NONE ) anytype = true;

	// serach trough current wad
	for( i = 0; i < wad->numlumps; i++)
	{
		// filtering lumps by type first
		if( anytype || matchtype == wad->lumps[i].type )
		{
			if( !com_stricmp( name, wad->lumps[i].name ))
				return &wad->lumps[i]; // found
		}
	}
	return NULL;
}

static void W_CleanupName( const char *dirtyname, char *cleanname )
{
	string	tempname;

	if( !dirtyname || !cleanname ) return;

	cleanname[0] = '\0'; // clear output
	FS_FileBase( dirtyname, tempname );
	if( com_strlen( tempname ) > WAD3_NAMELEN )
	{
		// windows style cutoff long names
		tempname[14] = '~';
		tempname[15] = '1';
	}

	com_strcat( cleanname, tempname );
	tempname[16] = '\0'; // cutoff all other ...

	// .. and turn big letters
	com.strupr( cleanname, cleanname );
}

static bool W_ConvertIWADLumps( wfile_t *wad )
{
	dlumpfile_t	*doomlumps;
	bool		flat_images = false;	// doom1 wall texture marker
	bool		skin_images = false;	// doom1 skin image ( sprite model ) marker
	bool		flmp_images = false;	// doom1 menu image marker
	size_t		lat_size;			// LAT - LumpAllocationTable		
	int		i;

	// nothing to convert ?
	if( !wad ) return false;

	lat_size = wad->numlumps * sizeof( dlumpfile_t );
	doomlumps = (dlumpfile_t *)Mem_Alloc( fs_mempool, lat_size );

	if( FS_Read( wad->file, doomlumps, lat_size ) != lat_size )
	{
		MsgDev( D_ERROR, "W_ConvertIWADLumps: %s has corrupted lump allocation table\n", wad->name );
		Mem_Free( doomlumps );
		W_Close( wad );
		return false;
	}

	// convert doom1 format into WAD3 lump format
	for( i = 0; i < wad->numlumps; i++ )
	{
		// W_Open will be swap lump later
		wad->lumps[i].filepos = doomlumps[i].filepos;
		wad->lumps[i].size = wad->lumps[i].disksize = doomlumps[i].size;
		com_strncpy( wad->lumps[i].name, doomlumps[i].name, 9 );
		wad->lumps[i].compression = CMP_NONE;
		wad->lumps[i].type = TYPE_NONE;
	
		// textures begin
		if(!com_stricmp( "S_START", wad->lumps[i].name ))
		{
			skin_images = true;
			continue; // skip identifier
		}
		else if(!com_stricmp( "P_START", wad->lumps[i].name ))
		{
			flat_images = true;
			continue; // skip identifier
		}
		else if(!com_stricmp( "P1_START", wad->lumps[i].name ))
		{
			flat_images = true;
			continue; // skip identifier
		}
		else if (!com_stricmp( "P2_START", wad->lumps[i].name ))
		{
			flat_images = true;
			continue; // skip identifier
		}
		else if (!com_stricmp( "P3_START", wad->lumps[i].name ))
		{
			// only doom2 uses this name
			flat_images = true;
			continue; // skip identifier
		}
		else if(!com_strnicmp("WI", wad->lumps[i].name, 2 )) flmp_images = true;
		else if(!com_strnicmp("ST", wad->lumps[i].name, 2 )) flmp_images = true;
		else if(!com_strnicmp("M_", wad->lumps[i].name, 2 )) flmp_images = true;
		else if(!com_strnicmp("END", wad->lumps[i].name, 3 )) flmp_images = true;
		else if(!com_strnicmp("HELP", wad->lumps[i].name, 4 )) flmp_images = true;
		else if(!com_strnicmp("CREDIT", wad->lumps[i].name, 6 )) flmp_images = true;
		else if(!com_strnicmp("TITLEPIC", wad->lumps[i].name, 8 )) flmp_images = true;
		else if(!com_strnicmp("VICTORY", wad->lumps[i].name, 7 )) flmp_images = true;
		else if(!com_strnicmp("PFUB", wad->lumps[i].name, 4 )) flmp_images = true;
		else if(!com_stricmp("P_END", wad->lumps[i].name )) flat_images = false;
		else if(!com_stricmp("P1_END", wad->lumps[i].name )) flat_images = false;
		else if(!com_stricmp("P2_END", wad->lumps[i].name )) flat_images = false;
		else if(!com_stricmp("P3_END", wad->lumps[i].name )) flat_images = false;
		else if(!com_stricmp("S_END", wad->lumps[i].name )) skin_images = false;
		else flmp_images = false;

		// setup lumptypes for doomwads
		if( flmp_images ) wad->lumps[i].type = TYPE_FLMP; // mark as menu pic
		if( flat_images ) wad->lumps[i].type = TYPE_FLAT; // mark as texture
		if( skin_images ) wad->lumps[i].type = TYPE_SKIN; // mark as skin (sprite model)
		if(!com_strnicmp( wad->lumps[i].name, "D_", 2 )) wad->lumps[i].type = TYPE_MUS;
		if(!com_strnicmp( wad->lumps[i].name, "DS", 2 )) wad->lumps[i].type = TYPE_SND;
	}

	Mem_Free( doomlumps ); // no need anymore
	return true;
}

static bool W_ReadLumpTable( wfile_t *wad )
{
	size_t	lat_size;
	int	i;

	// nothing to convert ?
	if( !wad ) return false;

	lat_size = wad->numlumps * sizeof( dlumpinfo_t );
	if( FS_Read( wad->file, wad->lumps, lat_size ) != lat_size )
	{
		MsgDev( D_ERROR, "W_ReadLumpTable: %s has corrupted lump allocation table\n", wad->name );
		W_Close( wad );
		return false;
	}

	// swap everything 
	for( i = 0; i < wad->numlumps; i++ )
	{
		wad->lumps[i].filepos = LittleLong( wad->lumps[i].filepos );
		wad->lumps[i].disksize = LittleLong( wad->lumps[i].disksize );
		wad->lumps[i].size = LittleLong( wad->lumps[i].size );
		// cleanup lumpname
		com_strnlwr( wad->lumps[i].name, wad->lumps[i].name, sizeof(wad->lumps[i].name));
	}
	return true;
}

byte *W_ReadLump( wfile_t *wad, dlumpinfo_t *lump, size_t *lumpsizeptr )
{
	byte	*buf, *cbuf;
	size_t	size = 0;

	// no wads loaded
	if( !wad || !lump ) return NULL;

	if( FS_Seek( wad->file, lump->filepos, SEEK_SET ))
	{
		MsgDev( D_ERROR, "W_ReadLump: %s is corrupted\n", lump->name );
		return 0;
	}

	switch( lump->compression )
	{
	case CMP_NONE:
		buf = (byte *)Mem_Alloc( fs_mempool, lump->disksize );
		size = FS_Read( wad->file, buf, lump->disksize );
		if( size < lump->disksize )
		{
			MsgDev( D_WARN, "W_ReadLump: %s is probably corrupted\n", lump->name );
			Mem_Free( buf );
			return NULL;
		}
		break;
	case CMP_LZSS:
		// never used by Id Software or Valve ?
		MsgDev(D_WARN, "W_ReadLump: lump %s have unsupported compression type\n", lump->name );
		return NULL;
	case CMP_ZLIB:
		cbuf = (byte *)Mem_Alloc( fs_mempool, lump->disksize );
		size = FS_Read( wad->file, cbuf, lump->disksize );
		buf = (byte *)Mem_Alloc( fs_mempool, lump->size );
		if(!VFS_Unpack( cbuf, size, &buf, lump->size ))
		{
			MsgDev( D_WARN, "W_ReadLump: %s is probably corrupted\n", lump->name );
			Mem_Free( cbuf );
			Mem_Free( buf );
			return NULL;
		}
		Mem_Free( cbuf ); // no reason to keep this data
		break;
	}					

	if( lumpsizeptr ) *lumpsizeptr = lump->size;
	return buf;
}

bool W_WriteLump( wfile_t *wad, dlumpinfo_t *lump, const void* data, size_t datasize, char cmp )
{
	size_t	ofs;
	vfile_t	*h;

	if( !wad || !lump || !data || !datasize )
		return false;

	switch(( int )cmp)
	{
	case CMP_LZSS:
		// never used by Id Software or Valve ?
		MsgDev( D_WARN, "W_SaveLump: lump %s can't be saved with comptype LZSS\n", lump->name );
		return false;		
	case CMP_ZLIB:
		h = VFS_Open( wad->file, "wz" );
		lump->size = datasize; // realsize
		VFS_Write( h, data, datasize );
		wad->file = VFS_Close( h );		// go back to real filesystem
		ofs = FS_Tell( wad->file );		// ofs - info->filepos returns compressed size
		lump->disksize = LittleLong( ofs - lump->filepos );
		return true;
	default:	// CMP_NONE method
		lump->size = lump->disksize = LittleLong( datasize );
		FS_Write( wad->file, data, datasize ); // just write file
		return true;
	}
}

/*
=============================================================================

WADSYSTEM PUBLIC BASE FUNCTIONS

=============================================================================
*/
wfile_t *W_Open( const char *filename, const char *mode )
{
	dwadinfo_t	header;
	wfile_t		*wad = (wfile_t *)Mem_Alloc( fs_mempool, sizeof( wfile_t ));

	// copy wad name
	FS_FileBase( filename, wad->name );
	wad->file = FS_Open( filename, mode );
	if( !wad->file )
	{
		W_Close( wad );
		MsgDev(D_ERROR, "W_Open: couldn't open %s\n", filename );
		return NULL;
	}

	// if the file is opened in "write", "append", or "read/write" mode
	if( mode[0] == 'w' )
	{
		dwadinfo_t	hdr;

		wad->numlumps = 0;		// blank wad
		wad->lumps = NULL;		//
		wad->mode = O_WRONLY;

		// save space for header
		hdr.ident = IDWAD3HEADER;
		hdr.numlumps = LittleLong( wad->numlumps );
		hdr.infotableofs = LittleLong(sizeof( dwadinfo_t ));
		FS_Write( wad->file, &hdr, sizeof( hdr ));
		wad->infotableofs = FS_Tell( wad->file );
	}
	else if( mode[0] == 'r' || mode[0] == 'a' )
	{
		if( mode[0] == 'a' )
		{
			FS_Seek( wad->file, 0, SEEK_SET );
			wad->mode = O_APPEND;
		}

		if( FS_Read( wad->file, &header, sizeof(dwadinfo_t)) != sizeof(dwadinfo_t))
		{
			MsgDev( D_ERROR, "W_Open: %s can't read header\n", filename );
			W_Close( wad );
			return NULL;
		}

		switch( header.ident )
		{
		case IDIWADHEADER:
		case IDPWADHEADER:
		case IDWAD2HEADER:
			if( wad->mode == O_APPEND )
			{
				MsgDev( D_WARN, "W_Open: %s is readonly\n", filename, mode );
				wad->mode = O_RDONLY; // set read-only mode
			}
			break; 
		case IDWAD3HEADER: break; // WAD3 allow r\w mode
		default:
			MsgDev( D_ERROR, "W_Open: %s unknown wadtype\n", filename );
			W_Close( wad );
			return NULL;
		}

		wad->numlumps = LittleLong( header.numlumps );
		if( wad->numlumps >= MAX_FILES_IN_WAD && wad->mode == O_APPEND )
		{
			MsgDev( D_WARN, "W_Open: %s is full (%i lumps)\n", wad->numlumps );
			wad->mode = O_RDONLY; // set read-only mode
		}
		wad->infotableofs = LittleLong( header.infotableofs ); // save infotableofs position
		if( FS_Seek( wad->file, wad->infotableofs, SEEK_SET ))
		{
			MsgDev( D_ERROR, "W_Open: %s can't find lump allocation table\n", filename );
			W_Close( wad );
			return NULL;
		}
		// NOTE: lumps table can be reallocated for O_APPEND mode
		wad->lumps = Mem_Alloc( fs_mempool, wad->numlumps * sizeof( dlumpinfo_t ));

		// setup lump allocation table
		switch( header.ident )
		{
		case IDIWADHEADER:
		case IDPWADHEADER:
			if(!W_ConvertIWADLumps( wad ))
				return NULL;
			break;		
		case IDWAD2HEADER:
		case IDWAD3HEADER: 
			if(!W_ReadLumpTable( wad ))
				return NULL;
			break;
		}

	}

	// and leaves the file open
	MsgDev( D_INFO, "W_Open: %s (%i lumps)\n", wad->name, wad->numlumps );
	return wad;
}

void W_Close( wfile_t *wad )
{
	fs_offset_t	ofs;

	if( !wad ) return;

	if( wad->file && ( wad->mode == O_APPEND || wad->mode == O_WRONLY ))
	{
		dwadinfo_t	hdr;

		// write the lumpingo
		ofs = FS_Tell( wad->file );
		FS_Write( wad->file, wad->lumps, wad->numlumps * sizeof( dlumpinfo_t ));
		
		// write the header
		hdr.ident = IDWAD3HEADER;
		hdr.numlumps = LittleLong( wad->numlumps );
		hdr.infotableofs = LittleLong( ofs );
		
		FS_Seek( wad->file, 0, SEEK_SET );
		FS_Write( wad->file, &hdr, sizeof( hdr ));
	}

	if( wad->lumps ) Mem_Free( wad->lumps );
	if( wad->file ) FS_Close( wad->file );	
	Mem_Free( wad ); // free himself
}

fs_offset_t W_SaveLump( wfile_t *wad, const char *lump, const void* data, size_t datasize, char type, char cmp )
{
	size_t		lat_size;
	dlumpinfo_t	*info;

	if( !wad || !lump || !data || !datasize ) return -1;
	if( wad->mode == O_RDONLY )
	{
		MsgDev( D_ERROR, "W_SaveLump: %s opened in readonly mode\n", wad->name ); 
		return -1;
	}

	if( wad->numlumps >= MAX_FILES_IN_WAD )
	{
		MsgDev( D_ERROR, "W_SaveLump: %s is full\n", wad->name ); 
		return -1;
	}

	wad->numlumps++;
	lat_size = wad->numlumps * sizeof( dlumpinfo_t );

	// reallocate lumptable
	wad->lumps = Mem_Realloc( fs_mempool, wad->lumps, lat_size );
	info = wad->lumps + wad->numlumps;

	// if we are in append mode - we need started from infotableofs poisition
	// overwrite lumptable as well, we have her copy in wad->lumps
	if( wad->mode == O_APPEND ) FS_Seek( wad->file, wad->infotableofs, SEEK_SET );

	// write header
	W_CleanupName( lump, info->name );
	info->filepos = LittleLong( FS_Tell( wad->file ));
	info->compression = cmp;
	info->type = type;

	if(!W_WriteLump( wad, info, data, datasize, cmp ))
	{
		wad->numlumps--;
		return -1;		
	}

	MsgDev( D_NOTE, "W_SaveLump: %s, size %d\n", info->name, info->disksize );
	return wad->numlumps;
}

byte *W_LoadLump( wfile_t *wad, const char *lumpname, size_t *lumpsizeptr, const char type )
{
	dlumpinfo_t	*lump;

	if( !wad ) return NULL;
	lump = W_FindLump( wad, lumpname, type );
	return W_ReadLump( wad, lump, lumpsizeptr );
}

/*
=============================================================================

WADSYSTEM FILE MANAGER

=============================================================================
*/
typedef struct wadtype_s
{
	char	*ext;
	int	type;
} wadtype_t;

wadtype_t wad_types[] =
{
	// associate extension with wad type
	{"flp", TYPE_FLMP	}, // doom1 menu picture
	{"snd", TYPE_SND	}, // doom1 sound
	{"mus", TYPE_MUS	}, // doom1 .mus format
	{"skn", TYPE_SKIN	}, // doom1 sprite model
	{"flt", TYPE_FLAT	}, // doom1 wall texture
	{"pal", TYPE_QPAL	}, // palette
	{"lmp", TYPE_QPIC	}, // quake1, hl pic
	{"mip", TYPE_MIPTEX2}, // hl texture
	{"mip", TYPE_MIPTEX }, // quake1 mip
	{"raw", TYPE_RAW	}, // signed raw data
	{"txt", TYPE_SCRIPT	}, // xash script file
	{"lst", TYPE_SCRIPT	}, // xash script file
	{"aur", TYPE_SCRIPT	}, // xash aurora file
	{"qc",  TYPE_SCRIPT	}, // xash qc-script file
	{"qh",  TYPE_SCRIPT	}, // xash qc-header file
	{"c",   TYPE_SCRIPT	}, // xash c-source file
	{"h",   TYPE_SCRIPT	}, // xash c-header file
	{"dat", TYPE_VPROGS	}, // xash progs
	{ NULL, TYPE_NONE	}
};


/*
====================
W_AddWad3File
====================
*/
static bool W_AddWad3File( const char *filename )
{
	dwadinfo_t	header;
	wfile_t		*w;
	dlumpfile_t	*doomlumps;	// will be converted to dlumpinfo_t struct
	int		infotableofs;
	bool		flat_images = false;// doom1 wall texture marker
	bool		skin_images = false;// doom1 skin image ( sprite model ) marker
	bool		flmp_images = false;// doom1 menu image marker
	int		i, numlumps;
	file_t		*file;
	char		type[16];

	if(!fs_searchwads) // start from scratch 
		fs_searchwads = Mem_CreateArray( fs_mempool, sizeof(wfile_t), 16 );

	for (i = 0; i < Mem_ArraySize( fs_searchwads ); i++ )
	{
		w = Mem_GetElement( fs_searchwads, i );
		if( w && !com_stricmp( filename, w->name ))
			return false; // already loaded
	}

	file = FS_Open( filename, "rb" );
	if(!file)
	{
		MsgDev(D_ERROR, "FS_AddWad3File: couldn't find %s\n", filename);
		return false;
	}

	if( FS_Read(file, &header, sizeof(dwadinfo_t)) != sizeof(dwadinfo_t))
	{
		MsgDev(D_ERROR, "FS_AddWad3File: %s have corrupted header\n");
		FS_Close( file );
		return false;
	}

	switch( header.ident )
	{
	case IDIWADHEADER:
	case IDPWADHEADER:
		com_strncpy( type, "Doom1", 16 );
		break; 		
	case IDWAD2HEADER:
		com_strncpy( type, "Quake1", 16 );
		break; 
	case IDWAD3HEADER: 
		com_strncpy( type, "Half-Life", 16 );
		break; 
	default:
		MsgDev(D_ERROR, "FS_AddWad3File: %s have invalid ident\n", filename );
		FS_Close( file );
		return false;
	}	

	numlumps = LittleLong( header.numlumps );
	if( numlumps > MAX_FILES_IN_PACK )
	{
		MsgDev(D_ERROR, "FS_AddWad3File: %s have too many lumps (%i)\n", numlumps);
		FS_Close( file );
		return false;
	}

	infotableofs = LittleLong( header.infotableofs );
	if(FS_Seek (file, infotableofs, SEEK_SET))
	{
		MsgDev(D_ERROR, "FS_AddWad3File: unable to seek to lump table\n");
		FS_Close( file );
		return false;
	}

	w = Mem_AllocElement( fs_searchwads );
	FS_FileBase( filename, w->name ); // wad name
	w->file = file;
	w->numlumps = numlumps;
	w->lumps = Mem_Alloc( fs_mempool, w->numlumps * sizeof(dlumpinfo_t));

	switch( header.ident )
	{
	case IDIWADHEADER:
	case IDPWADHEADER:
          	doomlumps = Mem_Alloc( fs_mempool, numlumps * sizeof(dlumpfile_t));
		if(FS_Read( file, doomlumps, sizeof(dlumpfile_t)*w->numlumps) != (long)sizeof(dlumpfile_t)*numlumps)
		{
			MsgDev(D_ERROR, "FS_AddWad3File: unable to read lump table\n");
			FS_Close( w->file );
			w->numlumps = 0;
			Mem_Free( w->lumps );
			Mem_Free( doomlumps );
			w->lumps = NULL;
			w->name[0] = 0;
			return false;
		}
		// convert doom1 format into quake lump 
		for( i = 0; i < numlumps; i++ )
		{
			// will swap later
			w->lumps[i].filepos = doomlumps[i].filepos;
			w->lumps[i].size = w->lumps[i].disksize = doomlumps[i].size;
			com_strncpy(w->lumps[i].name, doomlumps[i].name, 9 );
			w->lumps[i].type = TYPE_NONE;
			w->lumps[i].compression = CMP_NONE;
	
			// textures begin
			if(!com_stricmp("P_START", w->lumps[i].name ))
			{
				flat_images = true;
				continue; // skip identifier
			}
			else if(!com_stricmp("P1_START", w->lumps[i].name ))
			{
				flat_images = true;
				continue; // skip identifier
			}
			else if (!com_stricmp("P2_START", w->lumps[i].name ))
			{
				flat_images = true;
				continue; // skip identifier
			}
			else if (!com_stricmp("P3_START", w->lumps[i].name ))
			{
				flat_images = true;
				continue; // skip identifier
			}
			else if(!com_stricmp("S_START", w->lumps[i].name ))
			{
				skin_images = true;
				continue; // skip identifier
			}
			else if(!com_strnicmp("WI", w->lumps[i].name, 2 )) flmp_images = true;
			else if(!com_strnicmp("ST", w->lumps[i].name, 2 )) flmp_images = true;
			else if(!com_strnicmp("M_", w->lumps[i].name, 2 )) flmp_images = true;
			else if(!com_strnicmp("END", w->lumps[i].name, 3 )) flmp_images = true;
			else if(!com_strnicmp("HELP", w->lumps[i].name, 4 )) flmp_images = true;
			else if(!com_strnicmp("CREDIT", w->lumps[i].name, 6 )) flmp_images = true;
			else if(!com_strnicmp("TITLEPIC", w->lumps[i].name, 8 )) flmp_images = true;
			else if(!com_strnicmp("VICTORY", w->lumps[i].name, 7 )) flmp_images = true;
			else if(!com_strnicmp("PFUB", w->lumps[i].name, 4 )) flmp_images = true;
			else if(!com_stricmp("P_END", w->lumps[i].name )) flat_images = false;
			else if(!com_stricmp("P1_END", w->lumps[i].name )) flat_images = false;
			else if(!com_stricmp("P2_END", w->lumps[i].name )) flat_images = false;
			else if(!com_stricmp("P3_END", w->lumps[i].name )) flat_images = false;
			else if(!com_stricmp("S_END", w->lumps[i].name )) skin_images = false;
			else flmp_images = false;

			if( flmp_images ) w->lumps[i].type = TYPE_FLMP; // mark as menu pic
			if( flat_images ) w->lumps[i].type = TYPE_FLAT; // mark as texture
			if( skin_images ) w->lumps[i].type = TYPE_SKIN; // mark as skin (sprite model)
			if(!com_strnicmp( w->lumps[i].name, "D_", 2 )) w->lumps[i].type = TYPE_MUS;
			if(!com_strnicmp( w->lumps[i].name, "DS", 2 )) w->lumps[i].type = TYPE_SND;
		}
		Mem_Free( doomlumps ); // no need anymore
		break;		
	case IDWAD2HEADER:
	case IDWAD3HEADER: 
		if(FS_Read( file, w->lumps, sizeof(dlumpinfo_t)*w->numlumps) != (long)sizeof(dlumpinfo_t)*numlumps)
		{
			MsgDev(D_ERROR, "FS_AddWad3File: unable to read lump table\n");
			FS_Close( w->file );
			w->numlumps = 0;
			Mem_Free( w->lumps );
			w->lumps = NULL;
			w->name[0] = 0;
			return false;
		}
		break;
	}

	// swap everthing 
	for( i = 0; i < numlumps; i++ )
	{
		w->lumps[i].filepos = LittleLong(w->lumps[i].filepos);
		w->lumps[i].disksize = LittleLong(w->lumps[i].disksize);
		w->lumps[i].size = LittleLong(w->lumps[i].size);
		com_strnlwr(w->lumps[i].name, w->lumps[i].name, sizeof(w->lumps[i].name));
	}
	// and leaves the file open
	MsgDev( D_INFO, "Adding %s wadfile: %s (%i lumps)\n", type, filename, numlumps );
	return true;
}

/*
===========
FS_OpenWad3File

Look for a file in the loaded wadfiles and returns buffer with image lump
===========
*/
static byte *W_GetLump( const char *name, fs_offset_t *filesizeptr, int matchtype )
{
	uint	i, k;
	wfile_t	*w;
	string	basename;
	char	texname[17];

	// no wads loaded
	if(!fs_searchwads) return NULL;
	
	// note: wad images can't have real pathes
	// so, extarct base name from path
	W_FileBase( name, basename );
	if( filesizeptr ) *filesizeptr = 0;

          MsgDev( D_NOTE, "FS_OpenWadFile: %s\n", basename );
		
	if(com_strlen(basename) > WAD3_NAMELEN )
	{
		MsgDev( D_NOTE, "FS_OpenWadFile: %s too long name\n", basename );		
		return NULL;
	}
	com_strnlwr( basename, texname, WAD3_NAMELEN );
		
	for( k = 0; k < Mem_ArraySize( fs_searchwads ); k++ )
	{
		w = (wfile_t *)Mem_GetElement( fs_searchwads, k );
		if( !w ) continue;

		for(i = 0; i < (uint)w->numlumps; i++)
		{
			if(!com_stricmp(texname, w->lumps[i].name))
			{
				byte		*buf, *cbuf;
				size_t		size;

				if(matchtype != (int)w->lumps[i].type) return NULL; // try next type
				if(FS_Seek(w->file, w->lumps[i].filepos, SEEK_SET))
				{
					MsgDev(D_ERROR, "FS_OpenWadFile: %s probably corrupted\n", texname );
					return NULL;
				}
				switch((int)w->lumps[i].compression)
				{
				case CMP_NONE:
					buf = (byte *)Mem_Alloc( fs_mempool, w->lumps[i].disksize );
					size = FS_Read(w->file, buf, w->lumps[i].disksize );
					if( size < w->lumps[i].disksize )
					{
						Mem_Free( buf );
						MsgDev(D_WARN, "FS_OpenWadFile: %s probably corrupted\n", texname );
						return NULL;
					}
					break;
				case CMP_LZSS:
					// never used by Id Software or Valve ?
					MsgDev(D_WARN, "FS_OpenWadFile: lump %s have unsupported compression type\n", texname );
					return NULL;
				case CMP_ZLIB:
					cbuf = (byte *)Mem_Alloc( fs_mempool, w->lumps[i].disksize );
					size = FS_Read(w->file, cbuf, w->lumps[i].disksize );
					buf = (byte *)Mem_Alloc( fs_mempool, w->lumps[i].size );
					if(!VFS_Unpack( cbuf, size, &buf, w->lumps[i].size ))
					{
						Mem_Free( buf ), Mem_Free( cbuf );
						MsgDev(D_WARN, "FS_OpenWadFile: %s probably corrupted\n", texname );
						return NULL;
					}
					Mem_Free( cbuf ); // no reason to keep this data
					break;
				}					
				if(filesizeptr) *filesizeptr = w->lumps[i].disksize;
				return buf;
			}
		}
	}
	return NULL;
}

byte *W_LoadFile( const char *path, fs_offset_t *filesizeptr )
{
	wadtype_t		*type;
          const char	*ext = FS_FileExtension( path );
	bool		anyformat = !com_stricmp( ext, "" ) ? true : false;
	byte		*f;

	// now try all the formats in the selected list
	for( type = wad_types; type->ext; type++ )
	{
		if( anyformat || !com_stricmp( ext, type->ext ))
		{
			f = W_GetLump( path, filesizeptr, type->type );
			if( f ) return f; // found
		}
	}
	return NULL;
}

void W_InitGameDirectory( const char *dir )
{
	if( fs_wadsupport->integer || fs_use_wads )
	{
		search_t	*search;
		int	i, numWads = 0;
		          
		search = FS_Search( dir, true );
		if(!search) return;
		for( i = 0; i < search->numfilenames; i++ )
			W_AddWad3File( search->filenames[i] );
		Mem_Free( search );
	}
}

void W_ClearPaths( void )
{
	uint	i;
	wfile_t	*w;

	// close all wad files and free their lumps data
	for( i = 0; i < Mem_ArraySize( fs_searchwads ); i++ )
	{
		w = Mem_GetElement( fs_searchwads, i );
		if(!w) continue;
		if(w->lumps) Mem_Free(w->lumps);
		w->lumps = NULL;
		if(w->file) FS_Close(w->file);
		w->file = NULL;
		w->name[0] = 0;
	}
	if( fs_searchwads ) Mem_RemoveArray( fs_searchwads );
	fs_searchwads = NULL;
}

bool W_FileExists( const char *filename )
{
	wadtype_t		*type;
	wfile_t		*w; // names will be associated with lump types
	const char	*ext;
	string		lumpname;
	int		k, i;
		
	if( !fs_searchwads ) return false;
	ext = FS_FileExtension( filename );
	W_FileBase( filename, lumpname );

	// lookup all wads in list
	for( k = 0; k < Mem_ArraySize( fs_searchwads ); k++ )
	{
		w = (wfile_t *)Mem_GetElement( fs_searchwads, k );
		if( !w ) continue;

		for(i = 0; i < (uint)w->numlumps; i++)
		{
			for (type = wad_types; type->ext; type++)
			{
				// associate extension with lump->type
				if((!com_stricmp( ext, type->ext ) && type->type == (int)w->lumps[i].type))
				{
					if(!com_stricmp( lumpname, w->lumps[i].name ))
						return true;
				}
			}
		}
	}
	return false;
}

void W_Search( const char *pattern, int caseinsensitive, stringlist_t *resultlist )
{
	wadtype_t		*type;
	wfile_t		*w; // names will be associated with lump types
	const char	*ext = FS_FileExtension( pattern );
         	bool		anyformat = !com_stricmp(ext, "*") ? true : false;
	bool		anywadname = true;
	string		temp, lumpname, wadname;
	int		i, k;

	// also lookup all wad-files
	if( !fs_searchwads ) return;

	// using wadname as part of path to file
	// this pretty solution for "dead" lumps (duplicated files in different wads) 
	FS_FileBase( pattern, lumpname );
	FS_ExtractFilePath( pattern, wadname );
	if(com_strlen(wadname))
	{
		FS_FileBase( wadname, wadname );
		anywadname = false;
	}

	for( k = 0; k < Mem_ArraySize( fs_searchwads ); k++ )
	{
		w = (wfile_t *)Mem_GetElement( fs_searchwads, k );
		if( !w ) continue;

		if(com_stricmp( wadname, w->name ) && !anywadname )
			continue;

		for(i = 0; i < (uint)w->numlumps; i++)
		{
			for (type = wad_types; type->ext; type++)
			{
				// associate extension with lump->type
				if(anyformat || (!com_stricmp( ext, type->ext ) && type->type == (int)w->lumps[i].type))
				{
					if(SC_FilterToken(lumpname, w->lumps[i].name, !caseinsensitive ))
					{
						// build path: wadname/lumpname.ext
						com_snprintf(temp, sizeof(temp), "%s/%s", w->name, w->lumps[i].name );
						FS_DefaultExtension( temp, va(".%s", type->ext )); // make ext
						if( resultlist ) stringlistappend( resultlist, temp );
						break; // found, compare to next lump
					}
				}
			}
		}
	}
}

void W_Init( void )
{
	// enable temporary wad support for some tools
	switch( Sys.app_name )
	{
	case HOST_WADLIB:
	case HOST_RIPPER:
		fs_use_wads = true;
		break;
	default:
		fs_use_wads = false;
		break;
	}

	fs_searchwads = NULL;
}