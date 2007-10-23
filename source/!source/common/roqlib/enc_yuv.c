/*
** Copyright (C) 2003 Eric Lasota/Orbiter Productions
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation; either version 2.1 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "roqe_local.h"

// CCIR 601-1 YCbCr colorspace.

// Acceleration tables
static short r2y[256];
static short r2cb[256];
static short r2cr[256];

static short g2y[256];
static short g2cb[256];
static short g2cr[256];

static short b2y[256];
static short b2cb[256];
static short b2cr[256];

static short cb2g[256];
static short cb2b[256];

static short cr2r[256];
static short cr2g[256];

void RoQEnc_InitYUV(void)
{
	int i;
	static int initialized = 0;

	if(initialized)
		return;
	initialized = 1;

	for(i=0;i<256;i++)
	{
		// Set up encode tables
		r2y[i] = (short)((i * 19595)>>16);					// 0.299
		r2cb[i] = (short)((i * (-11056))>>16);				// -0.1687
		r2cr[i] = (short)(i>>1);							// 0.5

		g2y[i] = (short)((i * 38470)>>16);					// 0.587
		g2cb[i] = (short)((i * (-21712))>>16);				// -0.3313
		g2cr[i] = (short)((i * (-27440))>>16);				// -0.4187

		b2y[i] = (short)((i * 7471)>>16);					// 0.114
		b2cb[i] = (short)(i>>1);							// 0.5
		b2cr[i] = (short)((i * (-5328))>>16);				// -0.0813

		cb2g[i] = (short)(( (i-128)*(-22554) )>>16);		// -0.34414
		cb2b[i] = (short)(( (i-128)*112853   )>>16);		// 1.722
		
		cr2r[i] = (short)(( (i-128)*91881    )>>16);		// 1.402
		cr2g[i] = (short)(( (i-128)*(-46802) )>>16);		// -0.71414
	}
}

void Enc_RGBtoYUV(unsigned char r, unsigned char g, unsigned char b,
			  unsigned char *yp, unsigned char *up, unsigned char *vp)
{
	short y;
	short cb;
	short cr;

	y = r2y[r] + g2y[g] + b2y[b];
	y = (y < 0) ? 0 : ((y > 255) ? 255 : y);

	cb = r2cb[r] + g2cb[g] + b2cb[b] + 128;
	cb = (cb < 0) ? 0 : ((cb > 255) ? 255 : cb);

	cr = r2cr[r] + g2cr[g] + b2cr[b] + 128;
	cr = (cr < 0) ? 0 : ((cr > 255) ? 255 : cr);

	*yp = (unsigned char)y;
	*up = (unsigned char)cb;
	*vp = (unsigned char)cr;
}

void Enc_YUVtoRGB(unsigned char y, unsigned char u, unsigned char v,
			  unsigned char *rp, unsigned char *gp, unsigned char *bp)
{
	short r;
	short g;
	short b;

	r = y + cr2r[v];
	r = (r < 0) ? 0 : ((r > 255) ? 255 : r);

	g = y + cb2g[u] + cr2g[v];
	g = (g < 0) ? 0 : ((g > 255) ? 255 : g);

	b = y + cb2b[u];
	b = (b < 0) ? 0 : ((b > 255) ? 255 : b);

	*rp = (unsigned char)r;
	*gp = (unsigned char)g;
	*bp = (unsigned char)b;
}
