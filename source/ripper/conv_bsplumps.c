//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       conv_bsplumps.c - convert bsp lumps
//=======================================================================

#include "ripper.h"
#include "qc_gen.h"

#define IDIWADHEADER	(('D'<<24)+('A'<<16)+('W'<<8)+'I')	// little-endian "IWAD" doom1 game wad
#define IDPWADHEADER	(('D'<<24)+('A'<<16)+('W'<<8)+'P')	// little-endian "PWAD" doom1 game wad
#define IDWAD2HEADER	(('2'<<24)+('D'<<16)+('A'<<8)+'W')	// little-endian "WAD2" quake1 gfx.wad

typedef struct
{
	int		version;
	lump_t		lumps[15];
} dbspheader_t;

typedef struct
{
	int		ident;
	int		version;
} gbsphdr_t;

typedef struct
{
	int		nummiptex;
	int		dataofs[4]; // [nummiptex]
} dmiptexlump_t;

byte*	bsp_base;
bool	bsp_halflife = false;

void Conv_BspTextures( const char *name, lump_t *l )
{
	dmiptexlump_t	*m;
	string		genericname;
	mip_t		*mip;
	int		i, k;
	int		*dofs, size;
	byte		*buffer;

	if(!l->filelen) return; // no textures stored
	FS_FileBase( name, genericname );

	m = (dmiptexlump_t *)(bsp_base + l->fileofs);
	m->nummiptex = LittleLong(m->nummiptex);
	dofs = m->dataofs;

	for (i = 0; i < m->nummiptex; i++)
	{
		dofs[i] = LittleLong(dofs[i]);
		if (dofs[i] == -1) continue;

		// needs to simulate directly loading
		mip = (mip_t *)((byte *)m + dofs[i]);
		if(!LittleLong( mip->offsets[0] )) continue;		// not in bsp
		com.strnlwr(mip->name, mip->name, sizeof(mip->name));	// name
		buffer = ((byte *)m + dofs[i]);			// buffer
		size = (int)sizeof(mip_t) + (((mip->width * mip->height) * 85)>>6);
		if( bsp_halflife ) size += sizeof(short) + 768; // palette

		// check for '*' symbol issues
		k = com.strlen( com.strrchr( mip->name, '*' ));
		if( k ) mip->name[com.strlen(mip->name)-k] = '!'; // quake1 issues
		// some Q1 mods contains blank names (e.g. "after the fall")
		if(!com.strlen( mip->name )) com.snprintf( mip->name, 16, "%s_%d", genericname, i );
		ConvMIP(va("miptex/%s", mip->name ), buffer, size ); // convert it
	}
}

/*
============
ConvBSP
============
*/
bool ConvBSP( const char *name, char *buffer, int filesize )
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

	for (i = 0; i < 15; i++)
	{
		header->lumps[i].fileofs = LittleLong(header->lumps[i].fileofs);
		header->lumps[i].filelen = LittleLong(header->lumps[i].filelen);
	}
	Conv_BspTextures( name, &header->lumps[2]); // LUMP_TEXTURES

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
	switch( hdr.ident )
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
		switch( hdr.version )
		{
		case 18:
			game_family = GAME_HALFLIFE2_BETA;
			FS_Close( f );
			return true;
		case 38:
			game_family = GAME_QUAKE2;
			FS_Close( f );
			return true;
		case 39:
			game_family = GAME_XASH3D;
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
	default:
		game_family = GAME_GENERIC;
		FS_Close( f );
		return false;
	}
}

bool Conv_CheckWad( const char *wadname )
{
	file_t	*f = FS_Open( wadname, "rb" );
	dwadinfo_t hdr; // generic header

	if( !f ) return false;

	if(FS_Read( f, &hdr, sizeof(dwadinfo_t)) != sizeof(dwadinfo_t))
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