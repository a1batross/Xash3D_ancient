//=======================================================================
//			Copyright XashXT Group 2007 ©
//			img_utils.c - image common tools
//=======================================================================

#include "launch.h"
#include "byteorder.h"
#include "filesystem.h"
#include "mathlib.h"

cvar_t *img_resample_lerp;
cvar_t *img_oldformats;

uint d_8toD1table[256];
uint d_8toQ1table[256];
uint d_8toQ2table[256];
uint d_8to24table[256];
uint *d_currentpal;		// 32 bit palette
bool d1palette_init = false;
bool q1palette_init = false;
bool q2palette_init = false;
int d_rendermode = LUMP_NORMAL;

static byte palette_d1[768] = 
{
0,0,0,31,23,11,23,15,7,75,75,75,255,255,255,27,27,27,19,19,19,11,11,11,7,7,7,47,55,31,35,43,15,23,31,7,15,23,0,79,
59,43,71,51,35,63,43,27,255,183,183,247,171,171,243,163,163,235,151,151,231,143,143,223,135,135,219,123,123,211,115,
115,203,107,107,199,99,99,191,91,91,187,87,87,179,79,79,175,71,71,167,63,63,163,59,59,155,51,51,151,47,47,143,43,43,
139,35,35,131,31,31,127,27,27,119,23,23,115,19,19,107,15,15,103,11,11,95,7,7,91,7,7,83,7,7,79,0,0,71,0,0,67,0,0,255,
235,223,255,227,211,255,219,199,255,211,187,255,207,179,255,199,167,255,191,155,255,187,147,255,179,131,247,171,123,
239,163,115,231,155,107,223,147,99,215,139,91,207,131,83,203,127,79,191,123,75,179,115,71,171,111,67,163,107,63,155,
99,59,143,95,55,135,87,51,127,83,47,119,79,43,107,71,39,95,67,35,83,63,31,75,55,27,63,47,23,51,43,19,43,35,15,239,
239,239,231,231,231,223,223,223,219,219,219,211,211,211,203,203,203,199,199,199,191,191,191,183,183,183,179,179,179,
171,171,171,167,167,167,159,159,159,151,151,151,147,147,147,139,139,139,131,131,131,127,127,127,119,119,119,111,111,
111,107,107,107,99,99,99,91,91,91,87,87,87,79,79,79,71,71,71,67,67,67,59,59,59,55,55,55,47,47,47,39,39,39,35,35,35,
119,255,111,111,239,103,103,223,95,95,207,87,91,191,79,83,175,71,75,159,63,67,147,55,63,131,47,55,115,43,47,99,35,39,
83,27,31,67,23,23,51,15,19,35,11,11,23,7,191,167,143,183,159,135,175,151,127,167,143,119,159,135,111,155,127,107,147,
123,99,139,115,91,131,107,87,123,99,79,119,95,75,111,87,67,103,83,63,95,75,55,87,67,51,83,63,47,159,131,99,143,119,83,
131,107,75,119,95,63,103,83,51,91,71,43,79,59,35,67,51,27,123,127,99,111,115,87,103,107,79,91,99,71,83,87,59,71,79,51,
63,71,43,55,63,39,255,255,115,235,219,87,215,187,67,195,155,47,175,123,31,155,91,19,135,67,7,115,43,0,255,255,255,255,
219,219,255,187,187,255,155,155,255,123,123,255,95,95,255,63,63,255,31,31,255,0,0,239,0,0,227,0,0,215,0,0,203,0,0,191,
0,0,179,0,0,167,0,0,155,0,0,139,0,0,127,0,0,115,0,0,103,0,0,91,0,0,79,0,0,67,0,0,231,231,255,199,199,255,171,171,255,
143,143,255,115,115,255,83,83,255,55,55,255,27,27,255,0,0,255,0,0,227,0,0,203,0,0,179,0,0,155,0,0,131,0,0,107,0,0,83,
255,255,255,255,235,219,255,215,187,255,199,155,255,179,123,255,163,91,255,143,59,255,127,27,243,115,23,235,111,15,
223,103,15,215,95,11,203,87,7,195,79,0,183,71,0,175,67,0,255,255,255,255,255,215,255,255,179,255,255,143,255,255,107,
255,255,71,255,255,35,255,255,0,167,63,0,159,55,0,147,47,0,135,35,0,79,59,39,67,47,27,55,35,19,47,27,11,0,0,83,0,0,71,
0,0,59,0,0,47,0,0,35,0,0,23,0,0,11,0,255,255,255,159,67,255,231,75,255,123,255,255,0,255,207,0,207,159,0,155,111,0,107,
167,107,107
};

static byte palette_q1[768] =
{
0,0,0,15,15,15,31,31,31,47,47,47,63,63,63,75,75,75,91,91,91,107,107,107,123,123,123,139,139,139,155,155,155,171,
171,171,187,187,187,203,203,203,219,219,219,235,235,235,15,11,7,23,15,11,31,23,11,39,27,15,47,35,19,55,43,23,63,
47,23,75,55,27,83,59,27,91,67,31,99,75,31,107,83,31,115,87,31,123,95,35,131,103,35,143,111,35,11,11,15,19,19,27,
27,27,39,39,39,51,47,47,63,55,55,75,63,63,87,71,71,103,79,79,115,91,91,127,99,99,139,107,107,151,115,115,163,123,
123,175,131,131,187,139,139,203,0,0,0,7,7,0,11,11,0,19,19,0,27,27,0,35,35,0,43,43,7,47,47,7,55,55,7,63,63,7,71,71,
7,75,75,11,83,83,11,91,91,11,99,99,11,107,107,15,7,0,0,15,0,0,23,0,0,31,0,0,39,0,0,47,0,0,55,0,0,63,0,0,71,0,0,79,
0,0,87,0,0,95,0,0,103,0,0,111,0,0,119,0,0,127,0,0,19,19,0,27,27,0,35,35,0,47,43,0,55,47,0,67,55,0,75,59,7,87,67,7,
95,71,7,107,75,11,119,83,15,131,87,19,139,91,19,151,95,27,163,99,31,175,103,35,35,19,7,47,23,11,59,31,15,75,35,19,
87,43,23,99,47,31,115,55,35,127,59,43,143,67,51,159,79,51,175,99,47,191,119,47,207,143,43,223,171,39,239,203,31,255,
243,27,11,7,0,27,19,0,43,35,15,55,43,19,71,51,27,83,55,35,99,63,43,111,71,51,127,83,63,139,95,71,155,107,83,167,123,
95,183,135,107,195,147,123,211,163,139,227,179,151,171,139,163,159,127,151,147,115,135,139,103,123,127,91,111,119,
83,99,107,75,87,95,63,75,87,55,67,75,47,55,67,39,47,55,31,35,43,23,27,35,19,19,23,11,11,15,7,7,187,115,159,175,107,
143,163,95,131,151,87,119,139,79,107,127,75,95,115,67,83,107,59,75,95,51,63,83,43,55,71,35,43,59,31,35,47,23,27,35,
19,19,23,11,11,15,7,7,219,195,187,203,179,167,191,163,155,175,151,139,163,135,123,151,123,111,135,111,95,123,99,83,
107,87,71,95,75,59,83,63,51,67,51,39,55,43,31,39,31,23,27,19,15,15,11,7,111,131,123,103,123,111,95,115,103,87,107,
95,79,99,87,71,91,79,63,83,71,55,75,63,47,67,55,43,59,47,35,51,39,31,43,31,23,35,23,15,27,19,11,19,11,7,11,7,255,
243,27,239,223,23,219,203,19,203,183,15,187,167,15,171,151,11,155,131,7,139,115,7,123,99,7,107,83,0,91,71,0,75,55,
0,59,43,0,43,31,0,27,15,0,11,7,0,0,0,255,11,11,239,19,19,223,27,27,207,35,35,191,43,43,175,47,47,159,47,47,143,47,
47,127,47,47,111,47,47,95,43,43,79,35,35,63,27,27,47,19,19,31,11,11,15,43,0,0,59,0,0,75,7,0,95,7,0,111,15,0,127,23,
7,147,31,7,163,39,11,183,51,15,195,75,27,207,99,43,219,127,59,227,151,79,231,171,95,239,191,119,247,211,139,167,123,
59,183,155,55,199,195,55,231,227,87,127,191,255,171,231,255,215,255,255,103,0,0,139,0,0,179,0,0,215,0,0,255,0,0,255,
243,147,255,247,199,255,255,255,159,91,83
};

static byte palette_q2[768] =
{
0,0,0,15,15,15,31,31,31,47,47,47,63,63,63,75,75,75,91,91,91,107,107,107,123,123,123,139,139,139,155,155,155,171,171,
171,187,187,187,203,203,203,219,219,219,235,235,235,99,75,35,91,67,31,83,63,31,79,59,27,71,55,27,63,47,23,59,43,23,
51,39,19,47,35,19,43,31,19,39,27,15,35,23,15,27,19,11,23,15,11,19,15,7,15,11,7,95,95,111,91,91,103,91,83,95,87,79,91,
83,75,83,79,71,75,71,63,67,63,59,59,59,55,55,51,47,47,47,43,43,39,39,39,35,35,35,27,27,27,23,23,23,19,19,19,143,119,
83,123,99,67,115,91,59,103,79,47,207,151,75,167,123,59,139,103,47,111,83,39,235,159,39,203,139,35,175,119,31,147,99,
27,119,79,23,91,59,15,63,39,11,35,23,7,167,59,43,159,47,35,151,43,27,139,39,19,127,31,15,115,23,11,103,23,7,87,19,0,
75,15,0,67,15,0,59,15,0,51,11,0,43,11,0,35,11,0,27,7,0,19,7,0,123,95,75,115,87,67,107,83,63,103,79,59,95,71,55,87,67,
51,83,63,47,75,55,43,67,51,39,63,47,35,55,39,27,47,35,23,39,27,19,31,23,15,23,15,11,15,11,7,111,59,23,95,55,23,83,47,
23,67,43,23,55,35,19,39,27,15,27,19,11,15,11,7,179,91,79,191,123,111,203,155,147,215,187,183,203,215,223,179,199,211,
159,183,195,135,167,183,115,151,167,91,135,155,71,119,139,47,103,127,23,83,111,19,75,103,15,67,91,11,63,83,7,55,75,7,
47,63,7,39,51,0,31,43,0,23,31,0,15,19,0,7,11,0,0,0,139,87,87,131,79,79,123,71,71,115,67,67,107,59,59,99,51,51,91,47,
47,87,43,43,75,35,35,63,31,31,51,27,27,43,19,19,31,15,15,19,11,11,11,7,7,0,0,0,151,159,123,143,151,115,135,139,107,
127,131,99,119,123,95,115,115,87,107,107,79,99,99,71,91,91,67,79,79,59,67,67,51,55,55,43,47,47,35,35,35,27,23,23,19,
15,15,11,159,75,63,147,67,55,139,59,47,127,55,39,119,47,35,107,43,27,99,35,23,87,31,19,79,27,15,67,23,11,55,19,11,43,
15,7,31,11,7,23,7,0,11,0,0,0,0,0,119,123,207,111,115,195,103,107,183,99,99,167,91,91,155,83,87,143,75,79,127,71,71,
115,63,63,103,55,55,87,47,47,75,39,39,63,35,31,47,27,23,35,19,15,23,11,7,7,155,171,123,143,159,111,135,151,99,123,
139,87,115,131,75,103,119,67,95,111,59,87,103,51,75,91,39,63,79,27,55,67,19,47,59,11,35,47,7,27,35,0,19,23,0,11,15,
0,0,255,0,35,231,15,63,211,27,83,187,39,95,167,47,95,143,51,95,123,51,255,255,255,255,255,211,255,255,167,255,255,
127,255,255,83,255,255,39,255,235,31,255,215,23,255,191,15,255,171,7,255,147,0,239,127,0,227,107,0,211,87,0,199,71,
0,183,59,0,171,43,0,155,31,0,143,23,0,127,15,0,115,7,0,95,0,0,71,0,0,47,0,0,27,0,0,239,0,0,55,55,255,255,0,0,0,0,255,
43,43,35,27,27,23,19,19,15,235,151,127,195,115,83,159,87,51,123,63,27,235,211,199,199,171,155,167,139,119,135,107,87,
159,91,83	
};

void Image_Init( void )
{
	// init pools
	Sys.imagepool = Mem_AllocPool( "ImageLib Pool" );
	img_resample_lerp = Cvar_Get( "img_lerping", "1", CVAR_SYSTEMINFO, "lerping images after resample" );
	img_oldformats =  Cvar_Get( "img_oldformats", "1", CVAR_SYSTEMINFO, "enabels support of old image formats, e.g. doom1 flats, quake1 mips, quake2 wally, etc" );
}

void Image_Shutdown( void )
{
	Mem_Check(); // check for leaks
	Mem_FreePool( &Sys.imagepool );
}


void Image_RoundDimensions( int *scaled_width, int *scaled_height )
{
	int width, height;

	for( width = 1; width < *scaled_width; width <<= 1 );
	for( height = 1; height < *scaled_height; height <<= 1 );

	*scaled_width = bound( 1, width, IMAGE_MAXWIDTH );
	*scaled_height = bound( 1, height, IMAGE_MAXHEIGHT );
}

bool Image_ValidSize( const char *name )
{
	if( image_width > IMAGE_MAXWIDTH || image_height > IMAGE_MAXHEIGHT || image_width <= 0 || image_height <= 0 )
	{
		if(!com_stristr( name, "#internal" )) // internal errors are silent
			MsgDev(D_WARN, "Image_ValidSize: (%s) dims out of range[%dx%d]\n", name, image_width,image_height );
		return false;
	}
	return true;
}

vec_t Image_NormalizeColor( vec3_t in, vec3_t out )
{
	float	max, scale;

	max = in[0];
	if(in[1] > max) max = in[1];
	if(in[2] > max) max = in[2];

	// ignore green color
	if( max == 0 ) return 0;
	scale = 255.0f / max;
	VectorScale( in, scale, out );
	return max;
}

void Image_SetPalette( const byte *pal, uint *d_table )
{
	int	i;
	byte	rgba[4];
	
	// setup palette
	switch( d_rendermode )
	{
	case LUMP_DECAL:
		for(i = 0; i < 256; i++)
		{
			rgba[3] = pal[765];
			rgba[2] = pal[766];
			rgba[1] = pal[767];
			rgba[0] = i;
			d_table[i] = BuffBigLong( rgba );
		}
		break;
	case LUMP_TRANSPARENT:
		for (i = 0; i < 256; i++)
		{
			rgba[3] = pal[i*3+0];
			rgba[2] = pal[i*3+1];
			rgba[1] = pal[i*3+2];
			rgba[0] = pal[i] == 255 ? pal[i] : 0xFF;
			d_table[i] = BuffBigLong( rgba );
		}
		break;
	case LUMP_QFONT:
		for (i = 1; i < 256; i++)
		{
			rgba[3] = pal[i*3+0];
			rgba[2] = pal[i*3+1];
			rgba[1] = pal[i*3+2];
			rgba[0] = 0xFF;
			d_table[i] = BuffBigLong( rgba );
		}
		break;
	case LUMP_NORMAL:
		for (i = 0; i < 256; i++)
		{
			rgba[3] = pal[i*3+0];
			rgba[2] = pal[i*3+1];
			rgba[1] = pal[i*3+2];
			rgba[0] = 0xFF;
			d_table[i] = BuffBigLong( rgba );
		}
		break;
	}
}

void Image_GetPaletteD1( void )
{
	d_rendermode = LUMP_NORMAL;

	if(!d1palette_init)
	{
		Image_SetPalette( palette_d1, d_8toD1table );
		d_8toD1table[247] = 0; // 247 is transparent
		d1palette_init = true;
	}
	d_currentpal = d_8toD1table;
}

void Image_GetPaletteQ1( void )
{
	d_rendermode = LUMP_NORMAL;

	if(!q1palette_init)
	{
		Image_SetPalette( palette_q1, d_8toQ1table );
		d_8toQ1table[255] = 0; // 255 is transparent
		q1palette_init = true;
	}
	d_currentpal = d_8toQ1table;
}

void Image_GetPaletteQ2( void )
{
	d_rendermode = LUMP_NORMAL;

	if(!q2palette_init)
	{
		Image_SetPalette( palette_q2, d_8toQ2table );
		d_8toQ2table[255] &= LittleLong(0xffffff);
		q2palette_init = true;
	}
	d_currentpal = d_8toQ2table;
}

void Image_GetPalettePCX( const byte *pal )
{
	d_rendermode = LUMP_NORMAL;

	if( pal )
	{
		Image_SetPalette( pal, d_8to24table );
		d_8to24table[255] &= LittleLong(0xffffff);
		d_currentpal = d_8to24table;
	}
	else Image_GetPaletteQ2();          
}

void Image_GetPaletteLMP( const byte *pal, int rendermode )
{
	d_rendermode = rendermode;

	if( pal )
	{
		Image_SetPalette( pal, d_8to24table );
		d_8to24table[255] &= LittleLong(0xffffff);
		d_currentpal = d_8to24table;
	}
	else if(rendermode == LUMP_QFONT)
	{
		// quake1 base palette and font palette have some diferences
		Image_SetPalette( palette_q1, d_8to24table );
		d_8to24table[0] = 0;
		d_currentpal = d_8to24table;
	}
	else Image_GetPaletteQ1(); // default quake palette          
}

void Image_ConvertPalTo24bit( rgbdata_t *pic )
{
	byte	*pal32, *pal24;
	byte	*converted;
	int	i;

	if( !pic || !pic->buffer )
	{
		MsgDev(D_ERROR,"Image_ConvertPalTo24bit: image not loaded\n");
		return;
	}
	if( !pic->palette )
	{
		MsgDev(D_ERROR,"Image_ConvertPalTo24bit: no palette found\n");
		return;
	}
	if( pic->type == PF_INDEXED_24 )
	{
		MsgDev(D_ERROR,"Image_ConvertPalTo24bit: palette already converted\n");
		return;
	}

	pal24 = converted = Mem_Alloc( Sys.imagepool, 768 );
	pal32 = pic->palette;

	for( i = 0; i < 256; i++, pal24 += 3, pal32 += 4 )
	{
		pal24[0] = pal32[0];
		pal24[1] = pal32[1];
		pal24[2] = pal32[2];
	}
	Mem_Free( pic->palette );
	pic->palette = converted;
	pic->type = PF_INDEXED_24;
}

void Image_CopyPalette32bit( void )
{
	if( image_palette ) return; // already created ?
	image_palette = Mem_Alloc( Sys.imagepool, 1024 );
	Mem_Copy( image_palette, d_currentpal, 1024 );
}

/*
============
Image_Copy8bitRGBA

NOTE: must call Image_GetPaletteXXX before used
============
*/
bool Image_Copy8bitRGBA( const byte *in, byte *out, int pixels )
{
	int *iout = (int *)out;

	if( !d_currentpal )
	{
		MsgDev(D_ERROR,"Image_Copy8bitRGBA: no palette set\n");
		return false;
	}
	if( !in )
	{
		MsgDev(D_ERROR,"Image_Copy8bitRGBA: no input image\n");
		return false;
	}

	while( pixels >= 8 )
	{
		iout[0] = d_currentpal[in[0]];
		iout[1] = d_currentpal[in[1]];
		iout[2] = d_currentpal[in[2]];
		iout[3] = d_currentpal[in[3]];
		iout[4] = d_currentpal[in[4]];
		iout[5] = d_currentpal[in[5]];
		iout[6] = d_currentpal[in[6]];
		iout[7] = d_currentpal[in[7]];
		in += 8;
		iout += 8;
		pixels -= 8;
	}
	if( pixels & 4 )
	{
		iout[0] = d_currentpal[in[0]];
		iout[1] = d_currentpal[in[1]];
		iout[2] = d_currentpal[in[2]];
		iout[3] = d_currentpal[in[3]];
		in += 4;
		iout += 4;
	}
	if( pixels & 2 )
	{
		iout[0] = d_currentpal[in[0]];
		iout[1] = d_currentpal[in[1]];
		in += 2;
		iout += 2;
	}
	if( pixels & 1 ) // last byte
		iout[0] = d_currentpal[in[0]];

	return true;
}

static void Image_Resample32LerpLine (const byte *in, byte *out, int inwidth, int outwidth)
{
	int	j, xi, oldx = 0, f, fstep, endx, lerp;

	fstep = (int)(inwidth * 65536.0f/outwidth);
	endx = (inwidth-1);

	for( j = 0, f = 0; j < outwidth; j++, f += fstep )
	{
		xi = f>>16;
		if( xi != oldx )
		{
			in += (xi - oldx) * 4;
			oldx = xi;
		}
		if( xi < endx )
		{
			lerp = f & 0xFFFF;
			*out++ = (byte)((((in[4] - in[0]) * lerp)>>16) + in[0]);
			*out++ = (byte)((((in[5] - in[1]) * lerp)>>16) + in[1]);
			*out++ = (byte)((((in[6] - in[2]) * lerp)>>16) + in[2]);
			*out++ = (byte)((((in[7] - in[3]) * lerp)>>16) + in[3]);
		}
		else // last pixel of the line has no pixel to lerp to
		{
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
			*out++ = in[3];
		}
	}
}

static void Image_Resample24LerpLine( const byte *in, byte *out, int inwidth, int outwidth )
{
	int	j, xi, oldx = 0, f, fstep, endx, lerp;

	fstep = (int)(inwidth * 65536.0f/outwidth);
	endx = (inwidth-1);

	for( j = 0, f = 0; j < outwidth; j++, f += fstep )
	{
		xi = f>>16;
		if( xi != oldx )
		{
			in += (xi - oldx) * 3;
			oldx = xi;
		}
		if( xi < endx )
		{
			lerp = f & 0xFFFF;
			*out++ = (byte)((((in[3] - in[0]) * lerp)>>16) + in[0]);
			*out++ = (byte)((((in[4] - in[1]) * lerp)>>16) + in[1]);
			*out++ = (byte)((((in[5] - in[2]) * lerp)>>16) + in[2]);
		}
		else // last pixel of the line has no pixel to lerp to
		{
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
		}
	}
}

void Image_Resample32Lerp(const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight)
{
	int	i, j, r, yi, oldy = 0, f, fstep, lerp, endy = (inheight - 1);
	int	inwidth4 = inwidth * 4;
	int	outwidth4 = outwidth * 4;
	const byte *inrow;
	byte	*out;
	byte	*resamplerow1;
	byte	*resamplerow2;

	out = (byte *)outdata;
	fstep = (int)(inheight * 65536.0f/outheight);

	resamplerow1 = (byte *)Mem_Alloc( Sys.imagepool, outwidth * 4 * 2);
	resamplerow2 = resamplerow1 + outwidth * 4;

	inrow = (const byte *)indata;

	Image_Resample32LerpLine( inrow, resamplerow1, inwidth, outwidth );
	Image_Resample32LerpLine( inrow + inwidth4, resamplerow2, inwidth, outwidth );

	for( i = 0, f = 0; i < outheight; i++, f += fstep )
	{
		yi = f>>16;

		if( yi < endy )
		{
			lerp = f & 0xFFFF;
			if( yi != oldy )
			{
				inrow = (byte *)indata + inwidth4 * yi;
				if (yi == oldy+1) Mem_Copy( resamplerow1, resamplerow2, outwidth4 );
				else Image_Resample32LerpLine( inrow, resamplerow1, inwidth, outwidth );
				Image_Resample32LerpLine( inrow + inwidth4, resamplerow2, inwidth, outwidth );
				oldy = yi;
			}
			j = outwidth - 4;
			while( j >= 0 )
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				LERPBYTE( 6);
				LERPBYTE( 7);
				LERPBYTE( 8);
				LERPBYTE( 9);
				LERPBYTE(10);
				LERPBYTE(11);
				LERPBYTE(12);
				LERPBYTE(13);
				LERPBYTE(14);
				LERPBYTE(15);
				out += 16;
				resamplerow1 += 16;
				resamplerow2 += 16;
				j -= 4;
			}
			if( j & 2 )
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				LERPBYTE( 6);
				LERPBYTE( 7);
				out += 8;
				resamplerow1 += 8;
				resamplerow2 += 8;
			}
			if( j & 1 )
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				out += 4;
				resamplerow1 += 4;
				resamplerow2 += 4;
			}
			resamplerow1 -= outwidth4;
			resamplerow2 -= outwidth4;
		}
		else
		{
			if( yi != oldy )
			{
				inrow = (byte *)indata + inwidth4*yi;
				if( yi == oldy + 1 ) Mem_Copy( resamplerow1, resamplerow2, outwidth4 );
				else Image_Resample32LerpLine( inrow, resamplerow1, inwidth, outwidth);
				oldy = yi;
			}
			Mem_Copy( out, resamplerow1, outwidth4 );
		}
	}

	Mem_Free( resamplerow1 );
	resamplerow1 = NULL;
	resamplerow2 = NULL;
}

void Image_Resample32Nolerp( const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight )
{
	int	i, j;
	uint	frac, fracstep;
	int	*inrow, *out = (int *)outdata; // relies on int being 4 bytes

	fracstep = inwidth * 0x10000/outwidth;

	for( i = 0; i < outheight; i++)
	{
		inrow = (int *)indata + inwidth * (i * inheight/outheight);
		frac = fracstep>>1;
		j = outwidth - 4;

		while( j >= 0 )
		{
			out[0] = inrow[frac >> 16];frac += fracstep;
			out[1] = inrow[frac >> 16];frac += fracstep;
			out[2] = inrow[frac >> 16];frac += fracstep;
			out[3] = inrow[frac >> 16];frac += fracstep;
			out += 4;
			j -= 4;
		}
		if( j & 2 )
		{
			out[0] = inrow[frac >> 16];frac += fracstep;
			out[1] = inrow[frac >> 16];frac += fracstep;
			out += 2;
		}
		if( j & 1 )
		{
			out[0] = inrow[frac >> 16];frac += fracstep;
			out += 1;
		}
	}
}

void Image_Resample24Lerp( const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight )
{
	int	i, j, r, yi, oldy, f, fstep, lerp, endy = (inheight - 1);
	int	inwidth3 = inwidth * 3;
	int	outwidth3 = outwidth * 3;
	const byte *inrow;
	byte	*out = (byte *)outdata;
	byte	*resamplerow1;
	byte	*resamplerow2;
	
	fstep = (int)(inheight * 65536.0f / outheight);

	resamplerow1 = (byte *)Mem_Alloc(Sys.imagepool, outwidth * 3 * 2);
	resamplerow2 = resamplerow1 + outwidth*3;

	inrow = (const byte *)indata;
	oldy = 0;
	Image_Resample24LerpLine( inrow, resamplerow1, inwidth, outwidth );
	Image_Resample24LerpLine( inrow + inwidth3, resamplerow2, inwidth, outwidth );

	for( i = 0, f = 0; i < outheight; i++, f += fstep )
	{
		yi = f>>16;

		if( yi < endy )
		{
			lerp = f & 0xFFFF;
			if( yi != oldy )
			{
				inrow = (byte *)indata + inwidth3 * yi;
				if( yi == oldy + 1) Mem_Copy( resamplerow1, resamplerow2, outwidth3 );
				else Image_Resample24LerpLine( inrow, resamplerow1, inwidth, outwidth );
				Image_Resample24LerpLine( inrow + inwidth3, resamplerow2, inwidth, outwidth );
				oldy = yi;
			}
			j = outwidth - 4;
			while( j >= 0 )
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				LERPBYTE( 6);
				LERPBYTE( 7);
				LERPBYTE( 8);
				LERPBYTE( 9);
				LERPBYTE(10);
				LERPBYTE(11);
				out += 12;
				resamplerow1 += 12;
				resamplerow2 += 12;
				j -= 4;
			}
			if( j & 2 )
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				out += 6;
				resamplerow1 += 6;
				resamplerow2 += 6;
			}
			if( j & 1 )
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				out += 3;
				resamplerow1 += 3;
				resamplerow2 += 3;
			}
			resamplerow1 -= outwidth3;
			resamplerow2 -= outwidth3;
		}
		else
		{
			if( yi != oldy )
			{
				inrow = (byte *)indata + inwidth3*yi;
				if( yi == oldy + 1) Mem_Copy( resamplerow1, resamplerow2, outwidth3 );
				else Image_Resample24LerpLine( inrow, resamplerow1, inwidth, outwidth );
				oldy = yi;
			}
			Mem_Copy( out, resamplerow1, outwidth3 );
		}
	}
	Mem_Free( resamplerow1 );
	resamplerow1 = NULL;
	resamplerow2 = NULL;
}

void Image_Resample24Nolerp( const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight )
{
	int	i, j, f, inwidth3 = inwidth * 3;
	uint	frac, fracstep;
	byte	*inrow, *out = (byte *)outdata;

	fracstep = inwidth * 0x10000/outwidth;

	for( i = 0; i < outheight; i++)
	{
		inrow = (byte *)indata + inwidth3 * (i * inheight/outheight);
		frac = fracstep>>1;
		j = outwidth - 4;

		while( j >= 0 )
		{
			f = (frac >> 16)*3;
			*out++ = inrow[f+0];
			*out++ = inrow[f+1];
			*out++ = inrow[f+2];
			frac += fracstep;
			f = (frac >> 16)*3;
			*out++ = inrow[f+0];
			*out++ = inrow[f+1];
			*out++ = inrow[f+2];
			frac += fracstep;
			f = (frac >> 16)*3;
			*out++ = inrow[f+0];
			*out++ = inrow[f+1];
			*out++ = inrow[f+2];
			frac += fracstep;
			f = (frac >> 16)*3;
			*out++ = inrow[f+0];
			*out++ = inrow[f+1];
			*out++ = inrow[f+2];
			frac += fracstep;
			j -= 4;
		}
		if( j & 2 )
		{
			f = (frac >> 16)*3;
			*out++ = inrow[f+0];
			*out++ = inrow[f+1];
			*out++ = inrow[f+2];
			frac += fracstep;
			f = (frac >> 16)*3;
			*out++ = inrow[f+0];
			*out++ = inrow[f+1];
			*out++ = inrow[f+2];
			frac += fracstep;
			out += 2;
		}
		if( j & 1 )
		{
			f = (frac >> 16)*3;
			*out++ = inrow[f+0];
			*out++ = inrow[f+1];
			*out++ = inrow[f+2];
			frac += fracstep;
			out += 1;
		}
	}
}

/*
================
Image_Resample
================
*/
byte *Image_ResampleInternal( const void *indata, int inwidth, int inheight, int outwidth, int outheight, int type )
{
	bool	quality = img_resample_lerp->integer;
	byte	*outdata;

	// nothing to resample ?
	if (inwidth == outwidth && inheight == outheight)
		return (byte *)indata;

	// alloc new buffer
	switch( type )
	{
	case PF_RGB_24:
	case PF_RGB_24_FLIP:
		outdata = (byte *)Mem_Alloc( Sys.imagepool, outwidth * outheight * 3 );
		if( quality ) Image_Resample24Lerp( indata, inwidth, inheight, outdata, outwidth, outheight );
		else Image_Resample24Nolerp( indata, inwidth, inheight, outdata, outwidth, outheight );
		break;
	case PF_RGBA_32:
		outdata = (byte *)Mem_Alloc( Sys.imagepool, outwidth * outheight * 4 );
		if( quality ) Image_Resample32Lerp( indata, inwidth, inheight, outdata, outwidth, outheight );
		else Image_Resample32Nolerp( indata, inwidth, inheight, outdata, outwidth, outheight );
		break;
	default:
		MsgDev( D_WARN, "Image_Resample: unsupported format %s\n", PFDesc[type].name );
		return (byte *)indata;	
	}
	return (byte *)outdata;
}

bool Image_Resample( rgbdata_t **image, int width, int height, bool free_baseimage )
{
	int		w, h, pixel;
	rgbdata_t		*pix = *image;
	byte		*out;

	// check for buffers
	if(!pix || !pix->buffer) return false;

	w = pix->width;
	h = pix->height;

	if( width && height )
	{
		// custom size
		w = bound(4, width, IMAGE_MAXWIDTH );	// maxwidth 4096
		h = bound(4, height, IMAGE_MAXHEIGHT);	// maxheight 4096
	}
	else Image_RoundDimensions( &w, &h ); // auto detect new size

	out = Image_ResampleInternal((uint *)pix->buffer, pix->width, pix->height, w, h, pix->type );
	if( out != pix->buffer )
	{
		switch( pix->type )
		{
		case PF_RGBA_32:
			pixel = 4;
			break;
		case PF_RGB_24:
		case PF_RGB_24_FLIP:
			pixel = 3;
			break;
		default:
			pixel = 4;
		}
		
		// if image was resampled
		MsgDev(D_NOTE, "Resample image from[%d x %d] to [%d x %d]\n", pix->width, pix->height, w, h );
		if( free_baseimage ) Mem_Free( pix->buffer ); // free original image buffer

		// change image params
		pix->buffer = out;
		pix->width = w, pix->height = h;
		pix->size = w * h * pixel;

		*image = pix;
		return true;
	}
	return false;
}

bool Image_Process( rgbdata_t **pix, int adjust_type, bool free_baseimage )
{
	int	w, h;
	rgbdata_t	*pic = *pix;

	// check for buffers
	if(!pic || !pic->buffer) return false;

	w = pic->width;
	h = pic->height;

	//TODO: implement
	return false;
}