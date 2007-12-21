//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        wadlib.c.c - wad archive compiler
//=======================================================================

#include "platform.h"
#include "utils.h"

char		wadoutname[MAX_SYSPATH];
static dlumpinfo_t	wadlumps[MAX_FILES_IN_PACK];
static char	lumpname[MAX_SYSPATH];
dwadinfo_t	wadfile; // main header
int		wadheader;
int		numlumps = 0;
bool		compress_lumps = false;
file_t		*handle = NULL;

void Wad3_NewWad( void )
{
	handle = FS_Open( wadoutname, "wb" );

	if(!handle) Sys_Break("Wad3_NewWad: can't create %s\n", wadoutname );
	FS_Write( handle, &wadfile, sizeof(wadfile));
	memset( wadlumps, 0, sizeof(wadlumps));
	numlumps = 0;
}

/*
===============
AddLump
===============
*/
void Wad3_AddLump( const char *name, void *buffer, int length, int type, bool compress )
{
	dlumpinfo_t	*info;
	int		ofs;

	if(!buffer || type == TYPE_NONE)
	{
		MsgWarn("Wad3_AddLump: file %s not found\n", name ); 
		return;
	}

	FS_FileBase( (char *)name, lumpname );
	if(strlen(lumpname) > WAD3_NAMELEN)
	{
		MsgWarn("Wad3_AddLump: %s have too long name, max %d symbols\n", lumpname, WAD3_NAMELEN );
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
	MsgDev(D_NOTE, "AddLump: %s, size %d\n", info->name, info->disksize );
}

int Lump_GetFileType( const char *name, byte *buf )
{
	const char *ext = FS_FileExtension( name );

	if(!buf) return TYPE_NONE;

	switch(LittleLong(*(uint *)buf))
	{
	case IDSPRITEHEADER: return TYPE_SPRITE;
	case IDSTUDIOHEADER: return TYPE_STUDIO;
	case DDSHEADER: return TYPE_MIPDDS;
	}

	// otherwise get file type by extension
	if(!com.stricmp( ext, "tga" )) return TYPE_MIPTGA;
	else if(!com.stricmp( ext, "mip" )) return TYPE_MIPTEX;	// quake1 texture
	else if(!com.stricmp( ext, "tex" )) return TYPE_MIPTEX2;	// half-life texture
	else if(!com.stricmp( ext, "lmp" )) return TYPE_QPIC;	// hud pics
	else if(!com.stricmp( ext, "pal" )) return TYPE_QPAL;	// palette	
	else if(!com.stricmp( ext, "wav" )) return TYPE_SOUND;	// wav sound
	else if(!com.stricmp( ext, "txt" )) return TYPE_SCRIPT;	// text file
	else if(!com.stricmp( ext, "dat" )) return TYPE_VPROGS;	// qc progs
	else if(!com.stricmp( ext, "raw" )) return TYPE_RAW;	// raw data

	// no compares found
	return TYPE_NONE;
}

void Cmd_WadName( void )
{
	com.strncpy( wadoutname, Com_GetToken(false), sizeof(wadoutname));
	FS_DefaultExtension( wadoutname, ".wad" );
}

void Cmd_WadType( void )
{
	Com_GetToken( false );

	if (Com_MatchToken( "wad3")) wadheader = IDWAD3HEADER;
	else if (Com_MatchToken( "wad2")) wadheader = IDWAD2HEADER;
	else wadheader = IDWAD3HEADER; // default
}

void Cmd_AddLump( void )
{
	int	depth = 0;
	char	filename[MAX_SYSPATH];
	byte	*buf;
	size_t	filesize;
	bool	compress = false;

	Com_GetToken( false );
	com.strncpy( filename, com_token, sizeof(filename));
	if(Com_TryToken()) compress = true; 
	if( compress_lumps ) compress = true;

	Msg("loadfile %s\n", filename );
	// we can load file from another wad ;)
	buf = FS_LoadFile( filename, &filesize );
	Wad3_AddLump( filename, buf, filesize, Lump_GetFileType( filename, buf ), compress );
}

/*
==============
Cmd_WadCompress

syntax: "$compression"
==============
*/
void Cmd_WadCompress( void )
{
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
	MsgWarn("Cmd_WadUnknown: skip command \"%s\"\n", com_token);
	while(Com_TryToken());
}

void ResetWADInfo( void )
{
	FS_FileBase(gs_mapname, wadoutname );//kill path and ext
	FS_DefaultExtension( wadoutname, ".wad" );//set new ext
	
	memset (&wadheader, 0, sizeof(wadheader));
	wadheader = IDWAD3HEADER;
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
		else if (!Com_ValidScript( QC_WADLIB )) return false;
		else Cmd_WadUnknown();
	}
	return true;
}

void WriteWADFile( void )
{
	int		ofs;

	if(!handle) return;
	
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
}

void ClearWADfile( void )
{
	compress_lumps = false;
	handle = NULL;
}

bool BuildCurrentWAD( const char *name )
{
	bool load = false;
	
	if(name) com.strncpy( gs_mapname, name, sizeof(gs_mapname));
	FS_DefaultExtension( gs_mapname, ".qc" );
	load = Com_LoadScript( gs_mapname, NULL, 0 );
	
	if(load)
	{
		if(!ParseWADfileScript())
			return false;
		WriteWADFile();
		ClearWADfile();
		return true;
	}

	Msg("%s not found\n", gs_mapname );
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