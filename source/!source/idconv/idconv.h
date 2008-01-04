//=======================================================================
//			Copyright XashXT Group 2007 ©
//		idconv.h - Doom1\Quake\Hl convertor into Xash Formats
//=======================================================================
#ifndef BASECONVERTOR_H
#define BASECONVERTOR_H

#include <windows.h>
#include "basetypes.h"

//=====================================
//	convertor export
//=====================================

void InitConvertor	( uint funcname, int argc, char **argv ); // init host
void RunConvertor	( void ); // host frame
void CloseConvertor ( void ); // close host
extern stdlib_api_t com;
extern byte *zonepool;
extern string gs_gamedir;

bool ConvertResource( const char *name );

// convertor modules
bool ConvSPR( const char *name, char *buffer, int filesize );
bool ConvSP2( const char *name, char *buffer, int filesize );
bool ConvPCX( const char *name, char *buffer, int filesize );
bool ConvFLT( const char *name, char *buffer, int filesize );
bool ConvMIP( const char *name, char *buffer, int filesize );
bool ConvLMP( const char *name, char *buffer, int filesize );
bool ConvFNT( const char *name, char *buffer, int filesize );
bool ConvWAL( const char *name, char *buffer, int filesize );
bool ConvSKN( const char *name, char *buffer, int filesize );
bool ConvBSP( const char *name, char *buffer, int filesize );

//=====================================
//	lump utils
//=====================================
#define LUMP_NORMAL		0
#define LUMP_TRANSPARENT	1
#define LUMP_DECAL		2
#define LUMP_QFONT		3

bool PCX_ConvertImage( const char *name, char *buffer, int filesize );
bool Lump_ValidSize( char *name, rgbdata_t *pic, int maxwidth, int maxheight );
bool Conv_Copy8bitRGBA(const byte *in, byte *out, int pixels);
bool Conv_CreateShader( const char *name, rgbdata_t *pic, const char *animchain, int imageflags, int contents );
void Conv_GetPaletteLMP( byte *pal, int rendermode );
void Conv_GetPalettePCX( byte *pal );
void Conv_GetPaletteQ2( void );
void Conv_GetPaletteQ1( void );
void Conv_GetPaletteD1( void );
extern uint *d_currentpal;

#endif//BASECONVERTOR_H