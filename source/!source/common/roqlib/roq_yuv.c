//=======================================================================
//			Copyright XashXT Group 2007 ©
//			roq_yuv.c - ROQ yuv convertor
//=======================================================================

#include "roqlib.h"

// CCIR 601-1 YCbCr colorspace.
// acceleration tables
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

void ROQ_InitYUV(void)
{
	static bool	initialized = false;
	long		i;

	if(initialized) return;
	initialized = true;

	for(i = 0; i < 256; i++)
	{
		// set up encode tables
		r2y[i] =  (short)((i * 19595)>>16);		// 0.299
		r2cb[i] = (short)((i * (-11056))>>16);		// -0.1687
		r2cr[i] = (short)(i>>1);	        		// 0.5

		g2y[i] =  (short)((i * 38470)>>16);   		// 0.587
		g2cb[i] = (short)((i * (-21712))>>16);		// -0.3313
		g2cr[i] = (short)((i * (-27440))>>16);		// -0.4187

		b2y[i] =  (short)((i * 7471)>>16);    		// 0.114
		b2cb[i] = (short)(i>>1);	        		// 0.5
		b2cr[i] = (short)((i * (-5328))>>16); 		// -0.0813

		cb2g[i] = (short)(( (i-128)*(-22554))>>16);	// -0.34414
		cb2b[i] = (short)(( (i-128)*112853)>>16);	// 1.722
		
		cr2r[i] = (short)(( (i-128)*91881)>>16);   	// 1.402
		cr2g[i] = (short)(( (i-128)*(-46802))>>16);	// -0.71414
	}
}

void ROQ_RGBtoYUV(byte r, byte g, byte b, byte *yp, byte *up, byte *vp)
{
	short y, cb, cr;

	y = r2y[r] + g2y[g] + b2y[b];
	y = (y < 0) ? 0 : ((y > 255) ? 255 : y);
	cb = r2cb[r] + g2cb[g] + b2cb[b] + 128;
	cb = (cb < 0) ? 0 : ((cb > 255) ? 255 : cb);
	cr = r2cr[r] + g2cr[g] + b2cr[b] + 128;
	cr = (cr < 0) ? 0 : ((cr > 255) ? 255 : cr);

	*yp = (byte)y;
	*up = (byte)cb;
	*vp = (byte)cr;
}

void ROQ_YUVtoRGB(byte y, byte u, byte v, byte *rp, byte *gp, byte *bp)
{
	short r, g, b;

	r = y + cr2r[v];
	r = (r < 0) ? 0 : ((r > 255) ? 255 : r);
	g = y + cb2g[u] + cr2g[v];
	g = (g < 0) ? 0 : ((g > 255) ? 255 : g);
	b = y + cb2b[u];
	b = (b < 0) ? 0 : ((b > 255) ? 255 : b);

	*rp = (byte)r;
	*gp = (byte)g;
	*bp = (byte)b;
}
