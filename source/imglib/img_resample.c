//=======================================================================
//			Copyright XashXT Group 2007 ©
//			img_resample.c - resample 24 or 32-bit buffer
//=======================================================================

#include "imagelib.h"

#define LERPBYTE(i) r = resamplerow1[i];out[i] = (byte)((((resamplerow2[i] - r) * lerp)>>16) + r)

void Image_RoundDimensions( int *scaled_width, int *scaled_height )
{
	int width, height;

	for( width = 1; width < *scaled_width; width <<= 1 );
	for( height = 1; height < *scaled_height; height <<= 1 );

	*scaled_width = bound( 1, width, IMAGE_MAXWIDTH );
	*scaled_height = bound( 1, height, IMAGE_MAXHEIGHT );
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

	resamplerow1 = (byte *)Mem_Alloc( zonepool, outwidth * 4 * 2);
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

	resamplerow1 = (byte *)Mem_Alloc(zonepool, outwidth * 3 * 2);
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

	// malloc new buffer
	switch( type )
	{
	case PF_RGB_24:
	case PF_RGB_24_FLIP:
		outdata = (byte *)Mem_Alloc( zonepool, outwidth * outheight * 3 );
		if( quality ) Image_Resample24Lerp( indata, inwidth, inheight, outdata, outwidth, outheight );
		else Image_Resample24Nolerp( indata, inwidth, inheight, outdata, outwidth, outheight );
		break;
	case PF_RGBA_32:
		outdata = (byte *)Mem_Alloc( zonepool, outwidth * outheight * 4 );
		if( quality ) Image_Resample32Lerp( indata, inwidth, inheight, outdata, outwidth, outheight );
		else Image_Resample32Nolerp( indata, inwidth, inheight, outdata, outwidth, outheight );
		break;
	default:
		MsgDev( D_WARN, "Image_Resample: unsupported format %s\n", PFDesc[type].name );
		return (byte *)indata;	
	}
	return (byte *)outdata;
}

bool Image_Resample( const char *name, rgbdata_t **image, int width, int height, bool free_baseimage )
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
		MsgDev(D_NOTE, "Resampling %s from[%d x %d] to [%d x %d]\n", name, pix->width, pix->height, w, h );
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