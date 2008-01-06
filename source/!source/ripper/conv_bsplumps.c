//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       conv_bsplumps.c - convert bsp lumps
//=======================================================================

#include "ripper.h"

typedef struct
{
	int		version;
	lump_t		lumps[15];
} dbspheader_t;

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
	int		i, *dofs, size;
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

		// needs to emulate directly loading
		mip = (mip_t *)((byte *)m + dofs[i]);
		if(!LittleLong(mip->offsets[0])) continue;
		com.strnlwr(mip->name, mip->name, sizeof(mip->name));	// name
		buffer = ((byte *)m + dofs[i]);			// buffer
		size = (int)sizeof(mip_t) + (((mip->width * mip->height) * 85)>>6);
		if( bsp_halflife ) size += sizeof(short) + 768; // palette

		// some Q1 mods contains blank names (e.g. "after the fall")
		if(!com.strlen(mip->name)) com.snprintf( mip->name, 16, "%s_%d", genericname, i );
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