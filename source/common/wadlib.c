//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        wadlib.c.c - wad archive compiler
//=======================================================================

#include "platform.h"
#include "byteorder.h"
#include "utils.h"

char		wadoutname[MAX_SYSPATH];
static dlumpinfo_t	wadlumps[MAX_FILES_IN_WAD];
static char	lumpname[MAX_SYSPATH];
dwadinfo_t	wadfile; // main header
int		wadheader;
int		numlumps = 0;
bool		compress_lumps = false;
bool		allow_compression = false;
file_t		*handle = NULL;

void Wad3_NewWad( void )
{
	handle = FS_Open( wadoutname, "wb" );

	if(!handle) Sys_Break("Wad3_NewWad: can't create %s\n", wadoutname );
	FS_Write( handle, &wadfile, sizeof(wadfile));
	memset( wadlumps, 0, sizeof(wadlumps));
	numlumps = 0;
}

int Lump_GetFileType( const char *name, byte *buf )
{
	const char *ext = FS_FileExtension( name );

	if(!buf) return TYPE_NONE;

	// otherwise get file type by extension
	if(!com.stricmp( ext, "tga" )) return TYPE_GFXPIC;
	else if(!com.stricmp( ext, "dds" )) return TYPE_GFXPIC;	// directdraw mip
	else if(!com.stricmp( ext, "mip" )) 
	{
		if(wadheader == IDWAD3HEADER)
			return TYPE_MIPTEX2; // half-life texture
		return TYPE_MIPTEX;	// quake1 texture
	}
	else if(!com.stricmp( ext, "lmp" )) return TYPE_QPIC;	// hud pics
	else if(!com.stricmp( ext, "pal" )) return TYPE_QPAL;	// palette	
	else if(!com.stricmp( ext, "txt" )) return TYPE_SCRIPT;	// text file
	else if(!com.stricmp( ext, "dat" )) return TYPE_VPROGS;	// qc progs
	else if(!com.stricmp( ext, "raw" )) return TYPE_RAW;	// raw data
	
	// no compares found
	return TYPE_NONE;
}

/*
===============
AddLump
===============
*/
void Wad3_AddLump( const char *name, bool compress )
{
	dlumpinfo_t	*info;
	byte		*buffer;
	int		ofs, type, length;

	if( numlumps >= MAX_FILES_IN_WAD )
	{
		MsgDev( D_ERROR, "Wad3_AddLump: max files limit execeeds\n" ); 
		return;
	}

	// we can load file from another wad
	buffer = FS_LoadFile( name, &length );
	type = Lump_GetFileType( name, buffer );

	if(!buffer)
	{
		MsgDev( D_ERROR, "Wad3_AddLump: file %s not found\n", name ); 
		return;
	}
	if(type == TYPE_NONE)
	{
		MsgDev( D_ERROR, "Wad3_AddLump: file %s have unsupported type\n", name ); 
		return;		
	}

	FS_FileBase( name, lumpname );
	if(strlen(lumpname) > WAD3_NAMELEN)
	{
		MsgDev( D_ERROR, "Wad3_AddLump: %s have too long name, max %d symbols\n", lumpname, WAD3_NAMELEN );
		return;
	}

	// create wad file
	if(!handle) Wad3_NewWad();
	
	info = &wadlumps[numlumps];
	com.strncpy( info->name, lumpname, WAD3_NAMELEN );
	com.strupr( info->name, info->name );
	ofs = FS_Tell( handle );
	info->filepos = LittleLong( ofs );
	info->type = (char)type;
	numlumps++; // increase lump number

	if( compress )
	{
		vfile_t	*h = VFS_Open( handle, "wz" );
		info->compression = CMP_ZLIB;		
		info->size = length; // realsize
		VFS_Write( h, buffer, length );
		handle = VFS_Close( h ); // go back to real filesystem
		ofs = FS_Tell( handle ); // ofs - info->filepos returns compressed size
		info->disksize = LittleLong( ofs - info->filepos );
	}
	else
	{
		info->compression = CMP_NONE;	
		info->size = info->disksize = LittleLong( length );
		FS_Write( handle, buffer, length ); // just write file
	}
	Mem_Free( buffer );
	MsgDev(D_INFO, "AddLump: %s, size %d\n", info->name, info->disksize );
}

void Cmd_WadName( void )
{
	com.strncpy( wadoutname, Com_GetToken(false), sizeof(wadoutname));
	FS_DefaultExtension( wadoutname, ".wad" );
}

void Cmd_WadType( void )
{
	Com_GetToken( false );

	if(Com_MatchToken( "Quake1") || Com_MatchToken( "Q1"))
	{
		wadheader = IDWAD2HEADER;
		allow_compression = false;
	}
	else if(Com_MatchToken( "Half-Life") || Com_MatchToken( "Hl1"))
	{
		wadheader = IDWAD3HEADER;
		allow_compression = false;
	}
	else if(Com_MatchToken( "Xash3D"))
	{
		wadheader = IDWAD4HEADER;
		allow_compression = true;
	}
	else
	{
		wadheader = IDWAD3HEADER; // default
		allow_compression = false;
	}
}

void Cmd_AddLump( void )
{
	char	filename[MAX_SYSPATH];
	bool	compress = false;

	Com_GetToken( false );
	com.strncpy( filename, com_token, sizeof(filename));

	if( allow_compression )
	{
		if(Com_TryToken()) compress = true; 
		if(compress_lumps) compress = true;
	}

	Wad3_AddLump( filename, compress );
}

void Cmd_AddLumps( void )
{
	int	i, compress = false;
	search_t	*t;

	Com_GetToken( false );
	t = FS_Search( com_token, true );
	if(!t) return;

	if( allow_compression )
	{
		if(Com_TryToken()) compress = true; 
		if(compress_lumps) compress = true;
	}

	for(i = 0; i < t->numfilenames; i++)
	{
		Wad3_AddLump( t->filenames[i], compress );
	}
}

/*
==============
Cmd_WadCompress

syntax: "$compression"
==============
*/
void Cmd_WadCompress( void )
{
	if( allow_compression )
		compress_lumps = true;
}

/*
==============
Cmd_WadUnknown

syntax: "blabla"
==============
*/
void Cmd_WadUnknown( void )
{
	MsgDev( D_WARN, "Cmd_WadUnknown: skip command \"%s\"\n", com_token);
	while(Com_TryToken());
}

void ResetWADInfo( void )
{
	FS_FileBase( gs_filename, wadoutname );		// kill path and ext
	FS_DefaultExtension( wadoutname, ".wad" );	// set new ext
	
	memset (&wadheader, 0, sizeof(wadheader));
	wadheader = IDWAD3HEADER;
	compress_lumps = false;
	allow_compression = false;
	numlumps = 0;
	handle = NULL;
}

/*
===============
ParseScript	
===============
*/
bool ParseWADfileScript( void )
{
	ResetWADInfo();
	
	while( 1 )
	{
		if(!Com_GetToken (true))break;

		if (Com_MatchToken( "$wadname" )) Cmd_WadName();
		else if (Com_MatchToken( "$wadtype" )) Cmd_WadType();
		else if (Com_MatchToken( "$compression" )) Cmd_WadCompress();
		else if (Com_MatchToken( "$addlump" )) Cmd_AddLump();
		else if (Com_MatchToken( "$addlumps" )) Cmd_AddLumps();
		else if (!Com_ValidScript( QC_WADLIB )) return false;
		else Cmd_WadUnknown();
	}
	return true;
}

bool WriteWADFile( void )
{
	int		ofs;

	if(!handle) return false;
	
	// write the lumpingo
	ofs = FS_Tell( handle );
	FS_Write( handle, wadlumps, numlumps * sizeof(dlumpinfo_t));
		
	// write the header
	wadfile.ident = wadheader;
	wadfile.numlumps = LittleLong( numlumps );
	wadfile.infotableofs = LittleLong( ofs );
		
	FS_Seek( handle, 0, SEEK_SET );
	FS_Write( handle, &wadfile, sizeof(wadfile));
	FS_Close( handle );

	return true;
}

bool BuildCurrentWAD( const char *name )
{
	bool load = false;
	
	if(name) com.strncpy( gs_filename, name, sizeof(gs_filename));
	FS_DefaultExtension( gs_filename, ".qc" );
	load = Com_LoadScript( gs_filename, NULL, 0 );
	
	if(load)
	{
		if(!ParseWADfileScript())
			return false;
		return WriteWADFile();
	}

	Msg("%s not found\n", gs_filename );
	return false;
}

bool CompileWad3Archive( byte *mempool, const char *name, byte parms )
{
	if(mempool) studiopool = mempool;
	else
	{
		Msg("Wadlib: can't allocate memory pool.\nAbort compilation\n");
		return false;
	}
	return BuildCurrentWAD( name );	
}