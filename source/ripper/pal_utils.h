//=======================================================================
//			Copyright XashXT Group 2007 ©
//			pal_utils.h - palette utils
//=======================================================================
#ifndef PAL_UTILS_H
#define PAL_UTILS_H

#include "byteorder.h"

//=====================================
//	lump utils
//=====================================
#define LUMP_NORMAL		0
#define LUMP_TRANSPARENT	1
#define LUMP_DECAL		2
#define LUMP_QFONT		3

void Skin_FinalizeScript( void );
void Skin_CreateScript( const char *name, rgbdata_t *pic );
bool PCX_ConvertImage( const char *name, char *buffer, int filesize );
bool Lump_ValidSize( char *name, rgbdata_t *pic, int maxwidth, int maxheight );
bool Conv_Copy8bitRGBA(const byte *in, byte *out, int pixels);
bool Conv_CreateShader( const char *name, rgbdata_t *pic, const char *ext, const char *anim, int surf, int cnt );
void Conv_GetPaletteLMP( byte *pal, int rendermode );
void Conv_GetPalettePCX( byte *pal );
void Conv_GetPaletteQ2( void );
void Conv_GetPaletteQ1( void );
void Conv_GetPaletteD1( void );
extern uint *d_currentpal;

#endif//PAL_UTILS_H