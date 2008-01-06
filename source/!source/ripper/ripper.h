//=======================================================================
//			Copyright XashXT Group 2007 ©
//		idconv.h - Doom1\Quake\Hl convertor into Xash Formats
//=======================================================================
#ifndef BASECONVERTOR_H
#define BASECONVERTOR_H

#include <windows.h>
#include "basetypes.h"

extern stdlib_api_t com;
extern byte *zonepool;
extern string gs_gamedir;

//=====================================
//	convertor modules
//=====================================
bool ConvSPR( const char *name, char *buffer, int filesize );	// quake1, half-life and spr32 sprites
bool ConvSP2( const char *name, char *buffer, int filesize );	// quake2 sprites
bool ConvPCX( const char *name, char *buffer, int filesize );	// pcx images (can using q2 palette or builtin)
bool ConvFLT( const char *name, char *buffer, int filesize );	// Doom1 flat images (textures)
bool ConvFLP( const char *name, char *buffer, int filesize );	// Doom1 flat images (menu pics)
bool ConvJPG( const char *name, char *buffer, int filesize );	// Quake3 textures
bool ConvPAL( const char *name, char *buffer, int filesize );	// Quake1, half-life palette.lmp, just in case
bool ConvMIP( const char *name, char *buffer, int filesize );	// Quake1, Half-Life wad textures
bool ConvLMP( const char *name, char *buffer, int filesize );	// Quake1, Half-Life lump images
bool ConvFNT( const char *name, char *buffer, int filesize );	// Half-Life system fonts
bool ConvWAL( const char *name, char *buffer, int filesize );	// Quake2 textures
bool ConvSKN( const char *name, char *buffer, int filesize );	// Doom1 sprite models
bool ConvBSP( const char *name, char *buffer, int filesize );	// Extract textures from bsp (q1\hl)
bool ConvSND( const char *name, char *buffer, int filesize );	// not implemented
bool ConvMID( const char *name, char *buffer, int filesize );	// Doom1 music files (midi)
bool ConvRAW( const char *name, char *buffer, int filesize );	// write file without converting

#endif//BASECONVERTOR_H