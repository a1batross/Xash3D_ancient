//=======================================================================
//			Copyright XashXT Group 2007 ©
//		idconv.h - Doom1\Quake\Hl convertor into Xash Formats
//=======================================================================
#ifndef BASECONVERTOR_H
#define BASECONVERTOR_H

#include <windows.h>
#include "basetypes.h"
#include "ref_dllapi.h"

extern stdlib_api_t com;
extern byte *zonepool;
extern string gs_gamedir;
#define Sys_Error com.error
extern vprogs_exp_t *PRVM;
extern uint app_name;
extern bool write_qscsript;
extern int game_family;

typedef enum
{
	GAME_GENERIC = 0,
	GAME_DOOM1,
	GAME_DOOM3,
	GAME_QUAKE1,
	GAME_QUAKE2,
	GAME_QUAKE3,
	GAME_QUAKE4,
	GAME_HALFLIFE,
	GAME_HALFLIFE2,
	GAME_HALFLIFE2_BETA,
	GAME_HEXEN2,
	GAME_XASH3D,
	GAME_RTCW,	
} game_family_t;

static const char *game_names[] =
{
	"Unknown",
	"Doom1\\Doom2",
	"Doom3\\Quake4",
	"Quake1",
	"Quake2",
	"Quake3",
	"Quake4",
	"Half-Life",
	"Half-Life 2",
	"Half-Life 2 Beta",
	"Nexen 2",
	"Xash 3D",
	"Return To Castle Wolfenstein"
};

//=====================================
//	convertor modules
//=====================================
bool ConvSPR( const char *name, byte *buffer, size_t filesize, const char *ext );// quake1, half-life and spr32 sprites
bool ConvSP2( const char *name, byte *buffer, size_t filesize, const char *ext );// quake2 sprites
bool ConvPCX( const char *name, byte *buffer, size_t filesize, const char *ext );// pcx images (can use custom palette)
bool ConvFLT( const char *name, byte *buffer, size_t filesize, const char *ext );// Doom1 flat images (textures)
bool ConvFLP( const char *name, byte *buffer, size_t filesize, const char *ext );// Doom1 flat images (menu pics)
bool ConvJPG( const char *name, byte *buffer, size_t filesize, const char *ext );// Quake3 textures
bool ConvMIP( const char *name, byte *buffer, size_t filesize, const char *ext );// Quake1, Half-Life wad textures
bool ConvLMP( const char *name, byte *buffer, size_t filesize, const char *ext );// Quake1, Half-Life lump images
bool ConvFNT( const char *name, byte *buffer, size_t filesize, const char *ext );// Half-Life system fonts
bool ConvWAL( const char *name, byte *buffer, size_t filesize, const char *ext );// Quake2 textures
bool ConvVTF( const char *name, byte *buffer, size_t filesize, const char *ext );// Quake2 textures
bool ConvSKN( const char *name, byte *buffer, size_t filesize, const char *ext );// Doom1 sprite models
bool ConvBSP( const char *name, byte *buffer, size_t filesize, const char *ext );// Extract textures from bsp (q1\hl)
bool ConvSND( const char *name, byte *buffer, size_t filesize, const char *ext );// not implemented
bool ConvMID( const char *name, byte *buffer, size_t filesize, const char *ext );// Doom1 music files (midi)
bool ConvRAW( const char *name, byte *buffer, size_t filesize, const char *ext );// write file without converting
bool ConvDAT( const char *name, byte *buffer, size_t filesize, const char *ext );// quakec progs into source.qc

#endif//BASECONVERTOR_H