//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       conv_bsplumps.c - convert bsp lumps
//=======================================================================

#include "ripper.h"
#include "byteorder.h"
#include "qfiles_ref.h"

#define VDBSPMODHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'V')	// little-endian "VBSP" hl2 bsp's
#define IDIWADHEADER	(('D'<<24)+('A'<<16)+('W'<<8)+'I')	// little-endian "IWAD" doom1 game wad
#define IDPWADHEADER	(('D'<<24)+('A'<<16)+('W'<<8)+'P')	// little-endian "PWAD" doom1 game wad
#define IDWAD2HEADER	(('2'<<24)+('D'<<16)+('A'<<8)+'W')	// little-endian "WAD2" quake1 gfx.wad
#define IDWAD3HEADER	(('3'<<24)+('D'<<16)+('A'<<8)+'W')	// little-endian "WAD3" half-life wads

typedef struct
{
	int		ident;		// should be IWAD, WAD2 or WAD3
	int		numlumps;		// num files
	int		infotableofs;
} dwadheader_t;

typedef struct
{
	int		fileofs;
	int		filelen;
} dlump_t;

typedef struct
{
	int		version;
	dlump_t		lumps[15];
} dbspheader_t;

typedef struct
{
	int		ident;
	int		version;
} gbsphdr_t;

typedef struct
{
	int		nummiptex;
	int		dataofs[4];	// [nummiptex]
} dmiptexlump_t;

typedef struct
{
	char		name[64];
} dmiptexname_t;

typedef struct
{
	const char	*texname;
	const char	*detail;
	const char	material;
	int		lMin;
	int		lMax;
} dmaterial_t;

byte*		bsp_base;
bool		bsp_halflife = false;
dmiptexname_t	*mipnames = NULL;
int		mip_count = 0;
file_t		*detail_txt;

static const dmaterial_t detail_table[] =
{
{ "crt",		"dt_conc",	'C', 0, 0 },	// concrete
{ "rock",		"dt_rock",	'C', 0, 0 },
{ "conc", 	"dt_conc",	'C', 0, 0 },
{ "wall", 	"dt_brick",	'C', 0, 0 },
{ "crete",	"dt_conc",	'C', 0, 0 },
{ "generic",	"dt_brick",	'C', 0, 0 },
{ "metal",	"dt_metal%i",	'M', 1, 2 },	// metal
{ "mtl",		"dt_metal%i",	'M', 1, 2 },
{ "pipe",		"dt_metal%i",	'M', 1, 2 },
{ "elev",		"dt_metal%i",	'M', 1, 2 },
{ "sign",		"dt_metal%i",	'M', 1, 2 },
{ "barrel",	"dt_metal%i",	'M', 1, 2 },
{ "bath",		"dt_ssteel1",	'M', 1, 2 },
{ "refbridge",	"dt_metal%i",	'M', 1, 2 },
{ "panel",	"dt_ssteel1",	'M', 0, 0 },
{ "brass",	"dt_ssteel1",	'M', 0, 0 },
{ "car",		"dt_metal%i",	'M', 1, 2 },
{ "circuit",	"dt_metal%i",	'M', 1, 2 },
{ "steel",	"dt_ssteel1",	'M', 0, 0 },
{ "dirt",		"dt_ground%i",	'D', 1, 5 },	// dirt
{ "drt",		"dt_ground%i",	'D', 1, 5 },
{ "out",		"dt_ground%i",	'D', 1, 5 },
{ "grass",	"dt_grass1",	'D', 0, 0 },
{ "mud",		"dt_carpet1",	'D', 0, 0 },	// FIXME
{ "vent",		"dt_ssteel1",	'V', 1, 4 },	// vent
{ "duct",		"dt_ssteel1",	'V', 1, 4 },
{ "tile",		"dt_smooth%i",	'T', 1, 2 },
{ "labflr",	"dt_smooth%i",	'T', 1, 2 },
{ "bath",		"dt_smooth%i",	'T', 1, 2 },
{ "grate",	"dt_stone%i",	'G', 1, 4 },	// vent
{ "stone",	"dt_stone%i",	'G', 1, 4 },
{ "grt",		"dt_stone%i",	'G', 1, 4 },
{ "wood",		"dt_wood%i",	'W', 1, 3 },
{ "wd",		"dt_wood%i",	'W', 1, 3 },
{ "table",	"dt_wood%i",	'W', 1, 3 },
{ "board",	"dt_wood%i",	'W', 1, 3 },
{ "chair",	"dt_wood%i",	'W', 1, 3 },
{ "brd",		"dt_wood%i",	'W', 1, 3 },
{ "carp",		"dt_carpet1",	'W', 1, 3 },
{ "book",		"dt_wood%i",	'W', 1, 3 },
{ "box",		"dt_wood%i",	'W', 1, 3 },
{ "cab",		"dt_wood%i",	'W', 1, 3 },
{ "couch",	"dt_wood%i",	'W', 1, 3 },
{ "crate",	"dt_wood%i",	'W', 1, 3 },
{ "poster",	"dt_plaster%i",	'W', 1, 2 },
{ "sheet",	"dt_plaster%i",	'W', 1, 2 },
{ "stucco",	"dt_plaster%i",	'W', 1, 2 },
{ "comp",		"dt_smooth1",	'P', 0, 0 },
{ "cmp",		"dt_smooth1",	'P', 0, 0 },
{ "elec",		"dt_smooth1",	'P', 0, 0 },
{ "vend",		"dt_smooth1",	'P', 0, 0 },
{ "monitor",	"dt_smooth1",	'P', 0, 0 },
{ "phone",	"dt_smooth1",	'P', 0, 0 },
{ "glass",	"dt_ssteel1",	'Y', 0, 0 },
{ "window",	"dt_ssteel1",	'Y', 0, 0 },
{ "flesh",	"dt_rough1",	'F', 0, 0 },
{ "meat",		"dt_rough1",	'F', 0, 0 },
{ "fls",		"dt_rough1",	'F', 0, 0 },
{ "ground",	"dt_ground%i",	'D', 1, 5 },
{ "gnd",		"dt_ground%i",	'D', 1, 5 },
{ "snow",		"dt_snow%i",	'O', 1, 2 },	// snow
{ NULL, NULL, 0, 0, 0 }
};

bool MipExist( const char *name )
{
	int	i;

	if( !name ) return false;
	for( i = 0; i < mip_count; i++ )
	{
		if( !com.stricmp( name, mipnames[i].name ))
			return true;
	}
	return FS_FileExists( name );
}

static const char *DetailTextureForName( const char *name )
{
	const dmaterial_t	*table;

	if( !name || !*name ) return NULL;
	if( !com.strnicmp( name, "sky", 3 ))
		return NULL; // never details for sky

	// never apply details for liquids
	if( !com.strnicmp( name + 1, "!lava", 5 ))
		return NULL;
	if( !com.strnicmp( name + 1, "!slime", 6 ))
		return NULL;
	if( !com.strnicmp( name, "!cur_90", 7 ))
		return NULL;
	if( !com.strnicmp( name, "!cur_0", 6 ))
		return NULL;
	if( !com.strnicmp( name, "!cur_270", 8 ))
		return NULL;
	if( !com.strnicmp( name, "!cur_180", 8 ))
		return NULL;
	if( !com.strnicmp( name, "!cur_up", 7 ))
		return NULL;
	if( !com.strnicmp( name, "!cur_dwn", 8 ))
		return NULL;
	if( name[0] == '!' )
		return NULL;

	// never apply details to the special textures
	if( !com.strnicmp( name, "origin", 6 ))
		return NULL;
	if( !com.strnicmp( name, "clip", 4 ))
		return NULL;
	if( !com.strnicmp( name, "hint", 4 ))
		return NULL;
	if( !com.strnicmp( name, "skip", 4 ))
		return NULL;
	if( !com.strnicmp( name, "translucent", 11 ))
		return NULL;
	if( !com.strnicmp( name, "3dsky", 5 ))
		return NULL;
	if( name[0] == '@' )
		return NULL;

	// last check ...
	if( !com.strnicmp( name, "null", 4 ))
		return NULL;

	for( table = detail_table; table && table->texname; table++ )
	{
		if( com.stristr( name, table->texname ))
		{
			if(( table->lMin + table->lMax ) > 0 )
				return va( table->detail, Com_RandomLong( table->lMin, table->lMax )); 
			return table->detail;
		}
	}
	return "dt_smooth1"; // default
}

void Conv_BspTextures( const char *name, dlump_t *l, const char *ext )
{
	dmiptexlump_t	*m;
	string		genericname;
	const char	*det_name;
	mip_t		*mip;
	int		i, k;
	int		*dofs, size;
	byte		*buffer;

	if( !l->filelen ) return; // no textures stored
	FS_FileBase( name, genericname );

	m = (dmiptexlump_t *)(bsp_base + l->fileofs);
	m->nummiptex = LittleLong( m->nummiptex );
	dofs = m->dataofs;

	detail_txt = FS_Open( va( "%s/%s_detail.txt", gs_gamedir, name ), "wb" );

	mipnames = Mem_Realloc( basepool, mipnames, m->nummiptex * sizeof( dmiptexname_t )); 
	mip_count = 0;

	// first pass: store all names into linear array
	for( i = 0; i < m->nummiptex; i++ )
	{
		dofs[i] = LittleLong( dofs[i] );
		if( dofs[i] == -1 ) continue;

		// needs to simulate directly loading
		mip = (mip_t *)((byte *)m + dofs[i]);

		// build detailtexture string
		det_name = DetailTextureForName( mip->name );		// lookup both registers
		com.strnlwr( mip->name, mip->name, sizeof( mip->name ));	// name

		// detailtexture detected
		if( det_name ) FS_Printf( detail_txt, "%s detail/%s 10.0 10.0\n", mip->name, det_name );
		if( !LittleLong( mip->offsets[0] )) continue;		// not in bsp, skipped

		// check for '*' symbol issues
		k = com.strlen( com.strrchr( mip->name, '*' ));
		if( k ) mip->name[com.strlen(mip->name)-k] = '!'; // quake1 issues
		// some Q1 mods contains blank names (e.g. "after the fall")
		if( !com.strlen( mip->name )) com.snprintf( mip->name, 16, "%s_%d", genericname, i );
		com.snprintf( mipnames[mip_count].name, 64, "%s/%s.mip", genericname, mip->name ); // build name
		mip_count++; 
	}

	// second pass: convert lumps
	for( i = 0; i < m->nummiptex; i++ )
	{
		dofs[i] = LittleLong( dofs[i] );
		if( dofs[i] == -1 ) continue;

		// needs to simulate direct loading
		mip = (mip_t *)((byte *)m + dofs[i]);
		if( !LittleLong( mip->offsets[0] )) continue;		// not in bsp

		buffer = ((byte *)m + dofs[i]);			// buffer
		size = (int)sizeof(mip_t) + (((mip->width * mip->height) * 85)>>6);
		if( bsp_halflife ) size += sizeof(short) + 768; // palette

		if( !FS_FileExists( va( "%s/%s/%s.%s", gs_gamedir, name, mip->name, ext )))
			ConvMIP( va("%s/%s", name, mip->name ), buffer, size, ext ); // convert it
	}
	mip_count = 0; // freed and invaliadte
	FS_Close( detail_txt );
}

/*
============
ConvBSP
============
*/
bool ConvBSP( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	dbspheader_t *header = (dbspheader_t *)buffer;
	int i = LittleLong( header->version );
	
	switch( i )
	{
	case 28:
	case 29:
		bsp_halflife = false;
		break;
	case 30:
		bsp_halflife = true;
		break;
	default:
		return false; // another bsp version
	}
	bsp_base = (byte*)buffer;

	for( i = 0; i < 15; i++ )
	{
		header->lumps[i].fileofs = LittleLong(header->lumps[i].fileofs);
		header->lumps[i].filelen = LittleLong(header->lumps[i].filelen);
	}
	Conv_BspTextures( name, &header->lumps[2], ext ); // LUMP_TEXTURES

	return true;
}

bool Conv_CheckMap( const char *mapname )
{
	file_t	*f = FS_Open( mapname, "rb" );
	gbsphdr_t	hdr; // generic header

	if( !f ) return false;

	if(FS_Read( f, &hdr, sizeof(gbsphdr_t)) != sizeof(gbsphdr_t))
	{
		FS_Close( f );	// very strange file with size smaller than 8 bytes and ext .bsp
		return false;
	}
	// detect game type
	switch( LittleLong( hdr.ident ))
	{
	case 28:	// quake 1 beta
	case 29:	// quake 1 release
		game_family = GAME_QUAKE1;
		FS_Close( f );
		return true;
	case 30:
		game_family = GAME_HALFLIFE;
		FS_Close( f );
		return true;
	case IDBSPMODHEADER: // continue checking
		switch( LittleLong( hdr.version ))
		{
		case 38:
			game_family = GAME_QUAKE2;
			FS_Close( f );
			return true;
		case 46:
			game_family = GAME_QUAKE3;
			FS_Close( f );
			return true;
		case 47:
			game_family = GAME_RTCW;
			FS_Close( f );
			return true;
		}
	case VDBSPMODHEADER: // continue checking
		switch( LittleLong( hdr.version ))
		{
		case 18:
			game_family = GAME_HALFLIFE2_BETA;
			FS_Close( f );
			return true;
		case 19:
		case 20:
			game_family = GAME_HALFLIFE2;
			FS_Close( f );
			return true;
		}
	case IDWAD3HEADER:
		game_family = GAME_XASH3D;
		FS_Close( f );
		return true;
	default:
		game_family = GAME_GENERIC;
		FS_Close( f );
		return false;
	}
}

bool Conv_CheckWad( const char *wadname )
{
	file_t	*f = FS_Open( wadname, "rb" );
	dwadheader_t hdr; // generic header

	if( !f ) return false;

	if(FS_Read( f, &hdr, sizeof(dwadheader_t)) != sizeof(dwadheader_t))
	{
		FS_Close( f );	// very strange file with size smaller than 12 bytes and ext .wad
		return false;
	}

	// detect game type
	switch( hdr.ident )
	{
	case IDIWADHEADER:
	case IDPWADHEADER:
		game_family = GAME_DOOM1;
		FS_Close( f );
		break;		
	case IDWAD2HEADER:
		game_family = GAME_QUAKE1;
		FS_Close( f );
		break;
	case IDWAD3HEADER:
		game_family = GAME_HALFLIFE;
		FS_Close( f );
		break;
	default:
		game_family = GAME_GENERIC;
		break;
	}

	// and check wadnames in case
	if(!com.stricmp( wadname, "tnt.wad" ) && game_family == GAME_DOOM1 )
	{
		Msg("Wow! Doom2 na TNT!\n" );
	}
	return (game_family != GAME_GENERIC);
}