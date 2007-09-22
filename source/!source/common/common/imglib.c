//=======================================================================
//			Copyright XashXT Group 2007 �
//			imagelib.c - convert textures
//=======================================================================

#include "platform.h"
#include "basemath.h" // nearest_pow
#include "utils.h"

byte palette_q1[768] =
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

byte palette_q2[768] =
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

uint d_8toQ1table[256];
uint d_8toQ2table[256];
uint d_8to24table[256];
uint *d_currentpal;
bool q1palette_init = false;
bool q2palette_init = false;

void Image_GetPalette( byte *pal, uint *d_table )
{
	int	i;
	byte	rgba[4];

	// setup palette
	for (i = 0; i < 256; i++)
	{
		rgba[0] = 0xFF;
		rgba[3] = pal[i*3+0];
		rgba[2] = pal[i*3+1];
		rgba[1] = pal[i*3+2];
		d_table[i] = BuffBigLong( rgba );
	}
	d_currentpal = d_table;
}

void Image_GetQ1Palette( void )
{
	if(!q1palette_init)
	{
		Image_GetPalette( palette_q1, d_8toQ1table );
		d_8toQ1table[255] = 0; // 255 is transparent
		q1palette_init = true;
	}
	else d_currentpal = d_8toQ1table;
}

void Image_GetQ2Palette( void )
{
	if(!q2palette_init)
	{
		Image_GetPalette( palette_q2, d_8toQ2table );
		d_8toQ2table[255] &= LittleLong(0xffffff);
		q2palette_init = true;
	}
	else d_currentpal = d_8toQ2table;
}

void Image_GetPalettePCX( byte *pal )
{
	if(pal)
	{
		Image_GetPalette( pal, d_8to24table );
		d_8to24table[255] &= LittleLong(0xffffff);
		d_currentpal = d_8to24table;
	}
	else Image_GetQ2Palette();          
}

/*
============
Image_Copy8bitRGBA

NOTE: must call Image_GetQ2Palette or Image_GetQ1Palette
before used
============
*/
void Image_Copy8bitRGBA(const byte *in, byte *out, int pixels)
{
	int *iout = (int *)out;

	if(!d_currentpal)
	{
		MsgDev(D_ERROR, "Image_Copy8bitRGBA: no palette set\n");
		return;
	}

	while (pixels >= 8)
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
	if (pixels & 4)
	{
		iout[0] = d_currentpal[in[0]];
		iout[1] = d_currentpal[in[1]];
		iout[2] = d_currentpal[in[2]];
		iout[3] = d_currentpal[in[3]];
		in += 4;
		iout += 4;
	}
	if (pixels & 2)
	{
		iout[0] = d_currentpal[in[0]];
		iout[1] = d_currentpal[in[1]];
		in += 2;
		iout += 2;
	}
	if (pixels & 1)
		iout[0] = d_currentpal[in[0]];
}


void Image_RoundDimensions(int *scaled_width, int *scaled_height)
{
	int width = *scaled_width;
	int height = *scaled_height;

	width = nearest_pow( width );
	height = nearest_pow( height);

	*scaled_width = bound(1, width, 4096 );
	*scaled_height = bound(1, height, 4096 );
}

byte *Image_Resample(uint *in, int inwidth, int inheight, int outwidth, int outheight)
{
	int		i, j;
	uint		frac, fracstep;
	uint		*inrow1, *inrow2;
	byte		*pix1, *pix2, *pix3, *pix4;
	uint		*out, *buf, p1[4096], p2[4096];

	//check for buffers
	if(!in) return NULL;
	// nothing to resample ?
	if (inwidth == outwidth && inheight == outheight)
		return (byte *)in;

	// malloc new buffer
	out = buf = (uint *)Mem_Alloc( imagepool, outwidth * outheight * 4 );

	fracstep = inwidth * 0x10000 / outwidth;
	frac = fracstep>>2;

	for( i = 0; i < outwidth; i++)
	{
		p1[i] = 4 * (frac>>16);
		frac += fracstep;
	}
	frac = 3 * (fracstep>>2);

	for( i = 0; i < outwidth; i++)
	{
		p2[i] = 4 * (frac>>16);
		frac += fracstep;
	}

	for (i = 0; i < outheight; i++, buf += outwidth)
	{
		inrow1 = in + inwidth * (int)((i + 0.25) * inheight / outheight);
		inrow2 = in + inwidth * (int)((i + 0.75) * inheight / outheight);
		frac = fracstep>>1;

		for (j = 0; j < outwidth; j++)
		{
			pix1 = (byte *)inrow1 + p1[j];
			pix2 = (byte *)inrow1 + p2[j];
			pix3 = (byte *)inrow2 + p1[j];
			pix4 = (byte *)inrow2 + p2[j];
			((byte *)(buf+j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0])>>2;
			((byte *)(buf+j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1])>>2;
			((byte *)(buf+j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2])>>2;
			((byte *)(buf+j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3])>>2;
		}
	}
	return (byte *)out;
}

bool Image_Processing( const char *name, rgbdata_t **pix )
{
	int		w, h;
	rgbdata_t		*image = *pix;
	byte		*out;

	//check for buffers
	if(!image || !image->buffer) return false;

	w = image->width;
	h = image->height;
	Image_RoundDimensions( &w, &h ); //detect new size

	out = Image_Resample((uint *)image->buffer, image->width, image->height, w, h );
	if(out != image->buffer)
	{
		Msg("Resampling %s from[%d x %d] to[%d x %d]\n", name, image->width, image->height, w, h );
		Mem_Move( imagepool, &image->buffer, out, w * h * 4 );// update image->buffer
		image->width = w, image->height = h;
		*pix = image;
		return true;
	}
	return false;
}

bool ConvertImagePixels ( byte *mempool, const char *name, byte parms )
{
	rgbdata_t *image = FS_LoadImage( name, NULL, 0 );
	char	savename[MAX_QPATH];
	int	w, h;

	if(!image || !image->buffer) return false;

	strncpy( savename, name, sizeof(savename)-1);
	FS_StripExtension( savename ); // remove extension if needed
	FS_DefaultExtension( savename, ".tga" );// set new extension
	w = image->width, h = image->height; 

	if(FS_CheckParm("-resample"))
		Image_Processing( name, &image );

	FS_SaveImage( savename, image );// save as TGA
	FS_FreeImage( image );
	Msg("\n");

	return true;
}