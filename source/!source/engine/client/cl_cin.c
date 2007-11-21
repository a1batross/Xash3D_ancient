//=======================================================================
//			Copyright XashXT Group 2007 ©
//			 cl_cin.c - roq video player
//=======================================================================

#include "client.h"
#include "snd_loc.h"

#define MAXSIZE			8
#define MINSIZE			4
#define DEFAULT_CIN_WIDTH		512
#define DEFAULT_CIN_HEIGHT		512
#define ROQ_QUAD			0x1000
#define ROQ_QUAD_INFO		0x1001
#define ROQ_CODEBOOK		0x1002
#define ROQ_QUAD_VQ			0x1011
#define ROQ_QUAD_JPEG		0x1012
#define ROQ_QUAD_HANG		0x1013
#define ROQ_PACKET			0x1030
#define ZA_SOUND_MONO		0x1020
#define ZA_SOUND_STEREO		0x1021
#define MAX_VIDEO_HANDLES		16
extern int s_paintedtime;
extern int s_soundtime;		// sample PAIRS
extern int s_rawend;

static void RoQ_init( void );
static long ROQ_YY_tab[256];
static long ROQ_UB_tab[256];
static long ROQ_UG_tab[256];
static long ROQ_VG_tab[256];
static long ROQ_VR_tab[256];
static word vq2[256*16*4];
static word vq4[256*64*4];
static word vq8[256*256*4];
void CIN_StopCinematic( void );

typedef struct
{
	byte	linbuf[DEFAULT_CIN_WIDTH*DEFAULT_CIN_HEIGHT*4*2];
	byte	file[65536];
	short	sqrTable[256];

	uint	mcomp[256];
	byte	*qStatus[2][32768];

	long	oldXOff, oldYOff, oldysize, oldxsize;
} cinematics_t;

typedef struct
{
	char	fileName[MAX_OSPATH];
	int	CIN_WIDTH, CIN_HEIGHT;
	int	xpos, ypos, width, height;
	bool	looping, holdAtEnd, dirty, silent;
	file_t	*iFile;
	e_status	status;
	float	startTime;
	float	lastTime;
	long	tfps;
	long	RoQPlayed;
	long	ROQSize;
	uint	RoQFrameSize;
	long	onQuad;
	long	numQuads;
	long	samplesPerLine;
	uint	roq_id;
	long	screenDelta;

	void (*VQ0)(byte *status, void *qdata );
	void (*VQ1)(byte *status, void *qdata );
	void (*VQNormal)(byte *status, void *qdata );
	void (*VQBuffer)(byte *status, void *qdata );

	long	samplesPerPixel; // defaults to 2
	byte	*gray;
	uint	xsize, ysize, maxsize, minsize;

	bool	half, smootheddouble, inMemory;
	long	normalBuffer0;
	long	roq_flags;
	long	roqF0;
	long	roqF1;
	long	t[2];
	long	roqFPS;
	byte	*buf;
	long	drawX, drawY;
} cin_cache;

static cinematics_t	cin;
static cin_cache cinTable;

//-----------------------------------------------------------------------------
// RllSetupTable
//
// Allocates and initializes the square table.
//
// Parameters:	None
//
// Returns:		Nothing
//-----------------------------------------------------------------------------
static void RllSetupTable( void )
{
	int	z;

	for (z = 0; z < 128; z++)
	{
		cin.sqrTable[z] = (short)(z*z);
		cin.sqrTable[z+128] = (short)(-cin.sqrTable[z]);
	}
}


/*
//-----------------------------------------------------------------------------
 RllDecodeMonoToMono

 Decode mono source data into a mono buffer.

 Parameters:	from -> buffer holding encoded data
		to ->	buffer to hold decoded data
		size =	number of bytes of input (= # of shorts of output)
		signedOutput = 0 for unsigned output, non-zero for signed output
		flag = flags from asset header

 Returns: 	Number of samples placed in output buffer
//-----------------------------------------------------------------------------
*/
long RllDecodeMonoToMono(byte *from, short *to, uint size, char signedOutput, word flag)
{
	uint	z;
	int	prev;
	
	if (signedOutput) prev =  flag - 0x8000;
	else prev = flag;

	for(z = 0; z < size; z++) prev = to[z] = (short)(prev + cin.sqrTable[from[z]]); 
	return size; //*sizeof(short));
}

/*
//-----------------------------------------------------------------------------
 RllDecodeMonoToStereo

 Decode mono source data into a stereo buffer. Output is 4 times the number
 of bytes in the input.

 Parameters:	from -> buffer holding encoded data
		to ->	buffer to hold decoded data
		size =	number of bytes of input (= 1/4 # of bytes of output)
		signedOutput = 0 for unsigned output, non-zero for signed output
		flag = flags from asset header

 Returns:		Number of samples placed in output buffer
//-----------------------------------------------------------------------------
*/
long RllDecodeMonoToStereo(byte *from,short *to, uint size, char signedOutput, word flag)
{
	uint	z;
	int	prev;
	
	if (signedOutput) prev =  flag - 0x8000;
	else prev = flag;

	for (z = 0; z < size; z++)
	{
		prev = (short)(prev + cin.sqrTable[from[z]]);
		to[z*2+0] = to[z*2+1] = (short)(prev);
	}
	return size; // * 2 * sizeof(short));
}

/*
//-----------------------------------------------------------------------------
 RllDecodeStereoToStereo

 Decode stereo source data into a stereo buffer.

 Parameters:	from -> buffer holding encoded data
		to ->	buffer to hold decoded data
		size =	number of bytes of input (= 1/2 # of bytes of output)
		signedOutput = 0 for unsigned output, non-zero for signed output
		flag = flags from asset header

  Returns:	Number of samples placed in output buffer
//-----------------------------------------------------------------------------
*/
long RllDecodeStereoToStereo(byte *from,short *to,uint size,char signedOutput, word flag)
{
	uint	z;
	byte	*zz = from;
	int	prevL, prevR;

	if (signedOutput)
	{
		prevL = (flag & 0xff00) - 0x8000;
		prevR = ((flag & 0x00ff) << 8) - 0x8000;
	}
	else
	{
		prevL = flag & 0xff00;
		prevR = (flag & 0x00ff) << 8;
	}

	for (z = 0; z < size; z += 2)
	{
		prevL = (short)(prevL + cin.sqrTable[*zz++]); 
		prevR = (short)(prevR + cin.sqrTable[*zz++]);
		to[z+0] = (short)(prevL);
		to[z+1] = (short)(prevR);
	}
	return (size>>1);	//*sizeof(short));
}


/*
//-----------------------------------------------------------------------------
 RllDecodeStereoToMono

 Decode stereo source data into a mono buffer.

 Parameters:	from -> buffer holding encoded data
		to ->	buffer to hold decoded data
		size =	number of bytes of input (= # of bytes of output)
		signedOutput = 0 for unsigned output, non-zero for signed output
		flag = flags from asset header

 Returns:		Number of samples placed in output buffer
//-----------------------------------------------------------------------------
*/
long RllDecodeStereoToMono(byte *from, short *to, uint size, char signedOutput, word flag)
{
	uint	z;
	int	prevL,prevR;
	
	if (signedOutput)
	{
		prevL = (flag & 0xff00) - 0x8000;
		prevR = ((flag & 0x00ff) << 8) -0x8000;
	}
	else
	{
		prevL = flag & 0xff00;
		prevR = (flag & 0x00ff) << 8;
	}

	for (z = 0; z < size; z++)
	{
		prevL= prevL + cin.sqrTable[from[z*2]];
		prevR = prevR + cin.sqrTable[from[z*2+1]];
		to[z] = (short)((prevL + prevR)/2);
	}
	return size;
}

static void move8_32( byte *src, byte *dst, int spl )
{
	double	*dsrc, *ddst;
	int	dspl;

	dsrc = (double *)src;
	ddst = (double *)dst;
	dspl = spl>>3;

	ddst[0] = dsrc[0]; ddst[1] = dsrc[1]; ddst[2] = dsrc[2]; ddst[3] = dsrc[3];
	dsrc += dspl; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1]; ddst[2] = dsrc[2]; ddst[3] = dsrc[3];
	dsrc += dspl; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1]; ddst[2] = dsrc[2]; ddst[3] = dsrc[3];
	dsrc += dspl; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1]; ddst[2] = dsrc[2]; ddst[3] = dsrc[3];
	dsrc += dspl; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1]; ddst[2] = dsrc[2]; ddst[3] = dsrc[3];
	dsrc += dspl; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1]; ddst[2] = dsrc[2]; ddst[3] = dsrc[3];
	dsrc += dspl; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1]; ddst[2] = dsrc[2]; ddst[3] = dsrc[3];
	dsrc += dspl; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1]; ddst[2] = dsrc[2]; ddst[3] = dsrc[3];
}

static void move4_32( byte *src, byte *dst, int spl  )
{
	double	*dsrc, *ddst;
	int	dspl;

	dsrc = (double *)src;
	ddst = (double *)dst;
	dspl = spl>>3;

	ddst[0] = dsrc[0]; ddst[1] = dsrc[1];
	dsrc += dspl; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1];
	dsrc += dspl; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1];
	dsrc += dspl; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1];
}

static void blit8_32( byte *src, byte *dst, int spl  )
{
	double	*dsrc, *ddst;
	int	dspl;

	dsrc = (double *)src;
	ddst = (double *)dst;
	dspl = spl>>3;

	ddst[0] = dsrc[0]; ddst[1] = dsrc[1]; ddst[2] = dsrc[2]; ddst[3] = dsrc[3];
	dsrc += 4; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1]; ddst[2] = dsrc[2]; ddst[3] = dsrc[3];
	dsrc += 4; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1]; ddst[2] = dsrc[2]; ddst[3] = dsrc[3];
	dsrc += 4; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1]; ddst[2] = dsrc[2]; ddst[3] = dsrc[3];
	dsrc += 4; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1]; ddst[2] = dsrc[2]; ddst[3] = dsrc[3];
	dsrc += 4; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1]; ddst[2] = dsrc[2]; ddst[3] = dsrc[3];
	dsrc += 4; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1]; ddst[2] = dsrc[2]; ddst[3] = dsrc[3];
	dsrc += 4; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1]; ddst[2] = dsrc[2]; ddst[3] = dsrc[3];
}

static void blit4_32( byte *src, byte *dst, int spl  )
{
	double	*dsrc, *ddst;
	int	dspl;

	dsrc = (double *)src;
	ddst = (double *)dst;
	dspl = spl>>3;

	ddst[0] = dsrc[0]; ddst[1] = dsrc[1];
	dsrc += 2; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1];
	dsrc += 2; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1];
	dsrc += 2; ddst += dspl;
	ddst[0] = dsrc[0]; ddst[1] = dsrc[1];
}

static void blit2_32( byte *src, byte *dst, int spl  )
{
	double	*dsrc, *ddst;
	int	dspl;

	dsrc = (double *)src;
	ddst = (double *)dst;
	dspl = spl>>3;

	ddst[0] = dsrc[0];
	ddst[dspl] = dsrc[1];
}

static void blitVQQuad32fs( byte **status, byte *data )
{
	word	newd = 0, celdata = 0, code;
	uint	index = 0, i;
	int	spl;

	spl = cinTable.samplesPerLine;
        
	do
	{
		if (!newd)
		{ 
			newd = 7;
			celdata = data[0] + data[1]*256;
			data += 2;
		}
		else newd--;

		code = (word)(celdata&0xc000); 
		celdata <<= 2;
		
		switch (code)
		{
		case 0x8000:													// vq code
			blit8_32( (byte *)&vq8[(*data)*128], status[index], spl );
			data++;
			index += 5;
			break;
		case 0xc000:													// drop
			index++;													// skip 8x8
			for(i = 0; i < 4; i++)
			{
				if(!newd)
				{ 
					newd = 7;
					celdata = data[0] + data[1]*256;
					data += 2;
				}
				else newd--;
				code = (word)(celdata&0xc000); celdata <<= 2; 

				switch (code)
				{											// code in top two bits of code
				case 0x8000:										// 4x4 vq code
					blit4_32( (byte *)&vq4[(*data)*32], status[index], spl );
					data++;
					break;
				case 0xc000:										// 2x2 vq code
					blit2_32( (byte *)&vq2[(*data)*8], status[index], spl );
					data++;
					blit2_32( (byte *)&vq2[(*data)*8], status[index]+8, spl );
					data++;
					blit2_32( (byte *)&vq2[(*data)*8], status[index]+spl*2, spl );
					data++;
					blit2_32( (byte *)&vq2[(*data)*8], status[index]+spl*2+8, spl );
					data++;
					break;
				case 0x4000:										// motion compensation
					move4_32( status[index] + cin.mcomp[(*data)], status[index], spl );
					data++;
					break;
				}
				index++;
			}
			break;
		case 0x4000:													// motion compensation
			move8_32( status[index] + cin.mcomp[(*data)], status[index], spl );
			data++;
			index += 5;
			break;
		case 0x0000:
			index += 5;
			break;
		}
	} while ( status[index] != NULL );
}

static void ROQ_GenYUVTables( void )
{
	float	t_ub,t_vr,t_ug,t_vg;
	long	i;

	t_ub = (1.77200f/2.0f) * (float)(1<<6) + 0.5f;
	t_vr = (1.40200f/2.0f) * (float)(1<<6) + 0.5f;
	t_ug = (0.34414f/2.0f) * (float)(1<<6) + 0.5f;
	t_vg = (0.71414f/2.0f) * (float)(1<<6) + 0.5f;
	for(i = 0; i < 256; i++)
	{
		float x = (float)(2 * i - 255);
	
		ROQ_UB_tab[i] = (long)( ( t_ub * x) + (1<<5));
		ROQ_VR_tab[i] = (long)( ( t_vr * x) + (1<<5));
		ROQ_UG_tab[i] = (long)( (-t_ug * x));
		ROQ_VG_tab[i] = (long)( (-t_vg * x) + (1<<5));
		ROQ_YY_tab[i] = (long)( (i << 6) | (i >> 2) );
	}
}

#define VQ2TO4(a,b,c,d) {	\
    	*c++ = a[0];	\
	*d++ = a[0];	\
	*d++ = a[0];	\
	*c++ = a[1];	\
	*d++ = a[1];	\
	*d++ = a[1];	\
	*c++ = b[0];	\
	*d++ = b[0];	\
	*d++ = b[0];	\
	*c++ = b[1];	\
	*d++ = b[1];	\
	*d++ = b[1];	\
	*d++ = a[0];	\
	*d++ = a[0];	\
	*d++ = a[1];	\
	*d++ = a[1];	\
	*d++ = b[0];	\
	*d++ = b[0];	\
	*d++ = b[1];	\
	*d++ = b[1];	\
	a += 2; b += 2;	\
}
 
#define VQ2TO2(a,b,c,d) {	\
	*c++ = *a;	\
	*d++ = *a;	\
	*d++ = *a;	\
	*c++ = *b;	\
	*d++ = *b;	\
	*d++ = *b;	\
	*d++ = *a;	\
	*d++ = *a;	\
	*d++ = *b;	\
	*d++ = *b;	\
	a++; b++;		\
}

static word yuv_to_rgb( long y, long u, long v )
{ 
	long r,g,b,YY = (long)(ROQ_YY_tab[(y)]);

	r = (YY + ROQ_VR_tab[v]) >> 9;
	g = (YY + ROQ_UG_tab[u] + ROQ_VG_tab[v]) >> 8;
	b = (YY + ROQ_UB_tab[u]) >> 9;
	if (r<0) r = 0; if (g<0) g = 0; if (b<0) b = 0;
	if (r > 31) r = 31; if (g > 63) g = 63; if (b > 31) b = 31;

	return (word)((r<<11)+(g<<5)+(b));
}

static uint yuv_to_rgb24( long y, long u, long v )
{ 
	long r,g,b,YY = (long)(ROQ_YY_tab[(y)]);

	r = (YY + ROQ_VR_tab[v]) >> 6;
	g = (YY + ROQ_UG_tab[u] + ROQ_VG_tab[v]) >> 6;
	b = (YY + ROQ_UB_tab[u]) >> 6;
	
	if (r<0) r = 0; if (g<0) g = 0; if (b<0) b = 0;
	if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
	
	return LittleLong ((r)|(g<<8)|(b<<16)|(255<<24));
}

static void decodeCodeBook( byte *input, word roq_flags )
{
	long	i, j, two, four;
	word	*aptr, *bptr, *cptr, *dptr;
	long	y0,y1,y2,y3,cr,cb;
	byte	*bbptr, *baptr, *bcptr, *bdptr;
	uint	*iaptr, *ibptr, *icptr, *idptr;

	if (!roq_flags) two = four = 256;
	else
	{
		two  = roq_flags>>8;
		if (!two) two = 256;
		four = roq_flags&0xff;
	}

	four *= 2;
	bptr = (word *)vq2;

	if (!cinTable.half)
	{
		if (!cinTable.smootheddouble)
		{
			// normal height
			if (cinTable.samplesPerPixel == 2)
			{
				for(i = 0; i < two; i++)
				{
					y0 = (long)*input++;
					y1 = (long)*input++;
					y2 = (long)*input++;
					y3 = (long)*input++;
					cr = (long)*input++;
					cb = (long)*input++;
					*bptr++ = yuv_to_rgb( y0, cr, cb );
					*bptr++ = yuv_to_rgb( y1, cr, cb );
					*bptr++ = yuv_to_rgb( y2, cr, cb );
					*bptr++ = yuv_to_rgb( y3, cr, cb );
				}
				cptr = (word *)vq4;
				dptr = (word *)vq8;
		
				for(i = 0; i < four; i++)
				{
					aptr = (word *)vq2 + (*input++)*4;
					bptr = (word *)vq2 + (*input++)*4;
					for(j = 0; j < 2; j++) VQ2TO4(aptr,bptr,cptr,dptr);
				}
			}
			else if (cinTable.samplesPerPixel == 4)
			{
				ibptr = (uint *)bptr;
				for(i = 0; i < two; i++)
				{
					y0 = (long)*input++;
					y1 = (long)*input++;
					y2 = (long)*input++;
					y3 = (long)*input++;
					cr = (long)*input++;
					cb = (long)*input++;
					*ibptr++ = yuv_to_rgb24( y0, cr, cb );
					*ibptr++ = yuv_to_rgb24( y1, cr, cb );
					*ibptr++ = yuv_to_rgb24( y2, cr, cb );
					*ibptr++ = yuv_to_rgb24( y3, cr, cb );
				}
				icptr = (uint *)vq4;
				idptr = (uint *)vq8;
	
				for(i = 0; i < four; i++)
				{
					iaptr = (uint *)vq2 + (*input++)*4;
					ibptr = (uint *)vq2 + (*input++)*4;
					for(j = 0; j < 2; j++) VQ2TO4(iaptr, ibptr, icptr, idptr);
				}
			}
			else if (cinTable.samplesPerPixel == 1)
			{
				bbptr = (byte *)bptr;
				for(i = 0; i < two; i++)
				{
					*bbptr++ = cinTable.gray[*input++];
					*bbptr++ = cinTable.gray[*input++];
					*bbptr++ = cinTable.gray[*input++];
					*bbptr++ = cinTable.gray[*input]; input +=3;
				}

				bcptr = (byte *)vq4;
				bdptr = (byte *)vq8;
	
				for(i = 0; i < four; i++)
				{
					baptr = (byte *)vq2 + (*input++)*4;
					bbptr = (byte *)vq2 + (*input++)*4;
					for(j = 0; j < 2; j++) VQ2TO4(baptr,bbptr,bcptr,bdptr);
				}
			}
		}
		else
		{
			// double height, smoothed
			if (cinTable.samplesPerPixel == 2)
			{
				for(i = 0; i < two; i++)
				{
					y0 = (long)*input++;
					y1 = (long)*input++;
					y2 = (long)*input++;
					y3 = (long)*input++;
					cr = (long)*input++;
					cb = (long)*input++;
					*bptr++ = yuv_to_rgb( y0, cr, cb );
					*bptr++ = yuv_to_rgb( y1, cr, cb );
					*bptr++ = yuv_to_rgb(((y0*3)+y2)/4, cr, cb );
					*bptr++ = yuv_to_rgb(((y1*3)+y3)/4, cr, cb );
					*bptr++ = yuv_to_rgb((y0+(y2*3))/4, cr, cb );
					*bptr++ = yuv_to_rgb((y1+(y3*3))/4, cr, cb );
					*bptr++ = yuv_to_rgb( y2, cr, cb );
					*bptr++ = yuv_to_rgb( y3, cr, cb );
				}

				cptr = (word *)vq4;
				dptr = (word *)vq8;
		
				for(i = 0; i < four; i++)
				{
					aptr = (word *)vq2 + (*input++)*8;
					bptr = (word *)vq2 + (*input++)*8;
					for(j = 0; j < 2; j++)
					{
						VQ2TO4(aptr,bptr,cptr,dptr);
						VQ2TO4(aptr,bptr,cptr,dptr);
					}
				}
			}
			else if (cinTable.samplesPerPixel == 4)
			{
				ibptr = (uint *)bptr;
				for(i = 0; i < two; i++)
				{
					y0 = (long)*input++;
					y1 = (long)*input++;
					y2 = (long)*input++;
					y3 = (long)*input++;
					cr = (long)*input++;
					cb = (long)*input++;
					*ibptr++ = yuv_to_rgb24( y0, cr, cb );
					*ibptr++ = yuv_to_rgb24( y1, cr, cb );
					*ibptr++ = yuv_to_rgb24( ((y0*3)+y2)/4, cr, cb );
					*ibptr++ = yuv_to_rgb24( ((y1*3)+y3)/4, cr, cb );
					*ibptr++ = yuv_to_rgb24( (y0+(y2*3))/4, cr, cb );
					*ibptr++ = yuv_to_rgb24( (y1+(y3*3))/4, cr, cb );
					*ibptr++ = yuv_to_rgb24( y2, cr, cb );
					*ibptr++ = yuv_to_rgb24( y3, cr, cb );
				}
				icptr = (uint *)vq4;
				idptr = (uint *)vq8;
	
				for(i = 0; i < four; i++)
				{
					iaptr = (uint *)vq2 + (*input++)*8;
					ibptr = (uint *)vq2 + (*input++)*8;
					for(j = 0; j < 2; j++)
					{
						VQ2TO4(iaptr, ibptr, icptr, idptr);
						VQ2TO4(iaptr, ibptr, icptr, idptr);
					}
				}
			}
			else if (cinTable.samplesPerPixel == 1)
			{
				bbptr = (byte *)bptr;
				for(i = 0; i < two; i++)
				{
					y0 = (long)*input++;
					y1 = (long)*input++;
					y2 = (long)*input++;
					y3 = (long)*input; input+= 3;
					*bbptr++ = cinTable.gray[y0];
					*bbptr++ = cinTable.gray[y1];
					*bbptr++ = cinTable.gray[((y0*3)+y2)/4];
					*bbptr++ = cinTable.gray[((y1*3)+y3)/4];
					*bbptr++ = cinTable.gray[(y0+(y2*3))/4];
					*bbptr++ = cinTable.gray[(y1+(y3*3))/4];						
					*bbptr++ = cinTable.gray[y2];
					*bbptr++ = cinTable.gray[y3];
				}
				bcptr = (byte *)vq4;
				bdptr = (byte *)vq8;
	
				for(i = 0; i < four; i++)
				{
					baptr = (byte *)vq2 + (*input++)*8;
					bbptr = (byte *)vq2 + (*input++)*8;
					for(j = 0; j < 2; j++)
					{
						VQ2TO4(baptr,bbptr,bcptr,bdptr);
						VQ2TO4(baptr,bbptr,bcptr,bdptr);
					}
				}
			}			
		}
	}
	else
	{
		// 1/4 screen
		if (cinTable.samplesPerPixel == 2)
		{
			for(i = 0; i < two; i++)
			{
				y0 = (long)*input; input += 2;
				y2 = (long)*input; input += 2;
				cr = (long)*input++;
				cb = (long)*input++;
				*bptr++ = yuv_to_rgb( y0, cr, cb );
				*bptr++ = yuv_to_rgb( y2, cr, cb );
			}
			cptr = (word *)vq4;
			dptr = (word *)vq8;
	
			for(i = 0; i < four; i++)
			{
				aptr = (word *)vq2 + (*input++)*2;
				bptr = (word *)vq2 + (*input++)*2;
				for(j = 0; j < 2; j++) VQ2TO2(aptr,bptr,cptr,dptr);
			}
		}
		else if (cinTable.samplesPerPixel == 1)
		{
			bbptr = (byte *)bptr;
				
			for(i = 0; i < two; i++)
			{
				*bbptr++ = cinTable.gray[*input]; input+=2;
				*bbptr++ = cinTable.gray[*input]; input+=4;
			}

			bcptr = (byte *)vq4;
			bdptr = (byte *)vq8;
	
			for(i = 0; i <four; i++)
			{
				baptr = (byte *)vq2 + (*input++)*2;
				bbptr = (byte *)vq2 + (*input++)*2;
				for(j = 0; j < 2; j++) VQ2TO2(baptr,bbptr,bcptr,bdptr);
			}			
		}
		else if (cinTable.samplesPerPixel == 4)
		{
			ibptr = (uint *)bptr;
			for(i = 0; i < two; i++)
			{
				y0 = (long)*input; input += 2;
				y2 = (long)*input; input += 2;
				cr = (long)*input++;
				cb = (long)*input++;
				*ibptr++ = yuv_to_rgb24( y0, cr, cb );
				*ibptr++ = yuv_to_rgb24( y2, cr, cb );
			}

			icptr = (uint *)vq4;
			idptr = (uint *)vq8;
	
			for(i = 0; i < four; i++)
			{
				iaptr = (uint *)vq2 + (*input++)*2;
				ibptr = (uint *)vq2 + (*input++)*2;
				for(j = 0; j < 2; j++) VQ2TO2(iaptr,ibptr,icptr,idptr);
			}
		}
	}
}

static void recurseQuad( long startX, long startY, long quadSize, long xOff, long yOff )
{
	byte	*scroff;
	long	bigx, bigy, lowx, lowy, useY;
	long	offset;

	offset = cinTable.screenDelta;
	
	lowx = lowy = 0;
	bigx = cinTable.xsize;
	bigy = cinTable.ysize;

	if (bigx > cinTable.CIN_WIDTH) bigx = cinTable.CIN_WIDTH;
	if (bigy > cinTable.CIN_HEIGHT) bigy = cinTable.CIN_HEIGHT;

	if ((startX >= lowx) && (startX+quadSize) <= (bigx) && (startY+quadSize) <= (bigy) && (startY >= lowy) && quadSize <= MAXSIZE)
	{
		useY = startY;
		scroff = cin.linbuf + (useY+((cinTable.CIN_HEIGHT-bigy)>>1)+yOff)*(cinTable.samplesPerLine) + (((startX+xOff))*cinTable.samplesPerPixel);

		cin.qStatus[0][cinTable.onQuad  ] = scroff;
		cin.qStatus[1][cinTable.onQuad++] = scroff+offset;
	}
	if ( quadSize != MINSIZE )
	{
		quadSize >>= 1;
		recurseQuad( startX, startY, quadSize, xOff, yOff );
		recurseQuad( startX+quadSize, startY, quadSize, xOff, yOff );
		recurseQuad( startX, startY+quadSize, quadSize, xOff, yOff );
		recurseQuad( startX+quadSize, startY+quadSize , quadSize, xOff, yOff );
	}
}

static void setupQuad( long xOff, long yOff )
{
	long numQuadCels, i,x,y;
	byte *temp = NULL;

	if (xOff == cin.oldXOff && yOff == cin.oldYOff && cinTable.ysize == cin.oldysize && cinTable.xsize == cin.oldxsize)
		return;

	cin.oldXOff = xOff;
	cin.oldYOff = yOff;
	cin.oldysize = cinTable.ysize;
	cin.oldxsize = cinTable.xsize;

	numQuadCels = (cinTable.CIN_WIDTH*cinTable.CIN_HEIGHT) / (16);
	numQuadCels += numQuadCels/4 + numQuadCels/16;
	numQuadCels += 64; // for overflow

	numQuadCels = (cinTable.xsize*cinTable.ysize) / (16);
	numQuadCels += numQuadCels/4;
	numQuadCels += 64; // for overflow

	cinTable.onQuad = 0;

	for(y = 0; y < (long)cinTable.ysize; y += 16) 
		for(x = 0; x < (long)cinTable.xsize; x += 16) 
			recurseQuad( x, y, 16, xOff, yOff );

	for(i = (numQuadCels - 64); i < numQuadCels; i++)
	{
		cin.qStatus[0][i] = temp; // eoq
		cin.qStatus[1][i] = temp; // eoq
	}
}

static void readQuadInfo( byte *qData )
{
	cinTable.xsize = qData[0]+qData[1]*256;
	cinTable.ysize = qData[2]+qData[3]*256;
	cinTable.maxsize = qData[4]+qData[5]*256;
	cinTable.minsize = qData[6]+qData[7]*256;
	
	cinTable.CIN_HEIGHT = cinTable.ysize;
	cinTable.CIN_WIDTH = cinTable.xsize;

	cinTable.samplesPerLine = cinTable.CIN_WIDTH*cinTable.samplesPerPixel;
	cinTable.screenDelta = cinTable.CIN_HEIGHT*cinTable.samplesPerLine;

	cinTable.half = false;
	cinTable.smootheddouble = false;
	
	cinTable.VQ0 = cinTable.VQNormal;
	cinTable.VQ1 = cinTable.VQBuffer;

	cinTable.t[0] = (0 - (uint)cin.linbuf)+(uint)cin.linbuf+cinTable.screenDelta;
	cinTable.t[1] = (0 - ((uint)cin.linbuf + cinTable.screenDelta))+(uint)cin.linbuf;

	cinTable.drawX = cinTable.CIN_WIDTH;
	cinTable.drawY = cinTable.CIN_HEIGHT;
}

static void RoQPrepMcomp( long xoff, long yoff ) 
{
	long i, j, x, y, temp, temp2;

	i = cinTable.samplesPerLine;
	j = cinTable.samplesPerPixel;

	if( cinTable.xsize == (cinTable.ysize * 4) && !cinTable.half )
	{
		j = j + j;
		i = i + i;
	}
	
	for(y = 0; y < 16; y++)
	{
		temp2 = (y+yoff-8) * i;
		for(x = 0; x < 16; x++)
		{
			temp = (x+xoff-8) * j;
			cin.mcomp[(x*16)+y] = cinTable.normalBuffer0-(temp2+temp);
		}
	}
}

static void initRoQ( void ) 
{
	cinTable.VQNormal = (void (*)(byte *, void *))blitVQQuad32fs;
	cinTable.VQBuffer = (void (*)(byte *, void *))blitVQQuad32fs;
	cinTable.samplesPerPixel = 4;
	ROQ_GenYUVTables();
	RllSetupTable();
}

static void RoQReset( void )
{
	if(cinTable.iFile) FS_Close( cinTable.iFile );
	cinTable.iFile = FS_Open(cinTable.fileName, "rb" );

	// let the background thread start reading ahead
	FS_Read(cinTable.iFile, cin.file, 16 );
	RoQ_init();
	cinTable.status = FMV_LOOPED;
}

static void RoQInterrupt( void )
{
	byte	*framedata;
	short	sbuf[32768];
	int	ssize;
        
	FS_Read(cinTable.iFile, cin.file, cinTable.RoQFrameSize + 8 ); 

	if ( cinTable.RoQPlayed >= cinTable.ROQSize )
	{ 
		if (cinTable.holdAtEnd == false)
		{
			if (cinTable.looping) RoQReset();
			else cinTable.status = FMV_EOF;
		}
		else cinTable.status = FMV_IDLE;
		return; 
	}

	framedata = cin.file;

	// new frame is ready
redump:
	switch(cinTable.roq_id) 
	{
	case ROQ_QUAD_VQ:
		if ((cinTable.numQuads&1))
		{
			cinTable.normalBuffer0 = cinTable.t[1];
			RoQPrepMcomp( cinTable.roqF0, cinTable.roqF1 );
			cinTable.VQ1( (byte *)cin.qStatus[1], framedata);
			cinTable.buf = cin.linbuf + cinTable.screenDelta;
		}
		else
		{
			cinTable.normalBuffer0 = cinTable.t[0];
			RoQPrepMcomp( cinTable.roqF0, cinTable.roqF1 );
			cinTable.VQ0( (byte *)cin.qStatus[0], framedata );
			cinTable.buf = cin.linbuf;
		}
		if (cinTable.numQuads == 0)
		{		
			// first frame
			Mem_Copy(cin.linbuf+cinTable.screenDelta, cin.linbuf, cinTable.samplesPerLine*cinTable.ysize);
		}
		cinTable.numQuads++;
		cinTable.dirty = true;
		break;
	case ROQ_QUAD_JPEG:
		// not implemented
		break;
	case ROQ_CODEBOOK:
		decodeCodeBook( framedata, (word)cinTable.roq_flags );
		break;
	case ZA_SOUND_MONO:
		if (!cinTable.silent)
		{
			ssize = RllDecodeMonoToStereo( framedata, sbuf, cinTable.RoQFrameSize, 0, (word)cinTable.roq_flags);
			S_RawSamples( ssize, 22050, 2, 1, (byte *)sbuf, s_volume->value );
		}
		break;
	case ZA_SOUND_STEREO:
		if (!cinTable.silent)
		{
			if (cinTable.numQuads == -1)
			{
				S_Update();
				s_rawend = s_soundtime;
			}
			ssize = RllDecodeStereoToStereo( framedata, sbuf, cinTable.RoQFrameSize, 0, (word)cinTable.roq_flags);
			S_RawSamples( ssize, 22050, 2, 2, (byte *)sbuf, s_volume->value );
		}
		break;
	case ROQ_QUAD_INFO:
		if (cinTable.numQuads == -1)
		{
			readQuadInfo( framedata );
			setupQuad( 0, 0 );
			// we need to use CL_ScaledMilliseconds because of the smp mode calls from the renderer
			cinTable.startTime = cinTable.lastTime = cls.realtime;
		}
		if (cinTable.numQuads != 1) cinTable.numQuads = 0;
		break;
	case ROQ_PACKET:
		cinTable.inMemory = cinTable.roq_flags;
		cinTable.RoQFrameSize = 0;           // for header
		break;
	case ROQ_QUAD_HANG:
		cinTable.RoQFrameSize = 0;
		break;
	default:
		cinTable.status = FMV_EOF;
		break;
	}	

	// read in next frame data
	if ( cinTable.RoQPlayed >= cinTable.ROQSize )
	{ 
		if (cinTable.holdAtEnd == false)
		{
			if (cinTable.looping) RoQReset();
			else cinTable.status = FMV_EOF;
		}
		else cinTable.status = FMV_IDLE;
		return; 
	}
	
	framedata	+= cinTable.RoQFrameSize;
	cinTable.roq_id = framedata[0] + framedata[1]*256;
	cinTable.RoQFrameSize = framedata[2] + framedata[3]*256 + framedata[4]*65536;
	cinTable.roq_flags = framedata[6] + framedata[7]*256;
	cinTable.roqF0	= (char)framedata[7];
	cinTable.roqF1	= (char)framedata[6];

	if (cinTable.RoQFrameSize > 65536||cinTable.roq_id == 0x1084)
	{
		MsgDev(D_WARN, "RoQInterrupt: roq_size > 65536 || roq_id == 0x1084\n");
		cinTable.status = FMV_EOF;
		if (cinTable.looping) RoQReset();
		return;
	}
	if (cinTable.inMemory && (cinTable.status != FMV_EOF))
	{
		cinTable.inMemory--;
		framedata += 8;
		goto redump;
	}

	// one more frame hits the dust
	cinTable.RoQPlayed += cinTable.RoQFrameSize+8;
}

const char *CIN_Status( void )
{
	switch(cinTable.status)
	{
	case FMV_IDLE: return "IDLE";
	case FMV_PLAY: return "PLAY";
	case FMV_EOF: return "END";
	case FMV_ID_BLT: return "Id Blt";
	case FMV_ID_IDLE: return "Id idle";
	case FMV_LOOPED: return "LOOPED";
	case FMV_ID_WAIT: return "Id wait";
	default: return "FMV UNKNOWN";
	}
}

static void RoQ_init( void )
{
	// we need to use CL_ScaledMilliseconds because of the smp mode calls from the renderer
	cinTable.startTime = cinTable.lastTime = cls.realtime;

	cinTable.RoQPlayed = 24;

	// get frame rate	
	cinTable.roqFPS = cin.file[6] + cin.file[7] * 256;
	
	if (!cinTable.roqFPS) cinTable.roqFPS = 30;

	cinTable.numQuads = -1;

	cinTable.roq_id = cin.file[8] + cin.file[9]*256;
	cinTable.RoQFrameSize = cin.file[10] + cin.file[11] * 256 + cin.file[12] * 65536;
	cinTable.roq_flags = cin.file[14] + cin.file[15] * 256;                     
	MsgDev(D_INFO, "Movie: %s\n", CIN_Status());
}

static void RoQShutdown( void )
{
	if(cinTable.status == FMV_IDLE || !cinTable.buf)
		return;

	MsgDev(D_INFO, "Movie: %s\n", CIN_Status());
	cinTable.status = FMV_IDLE;

	if (cinTable.iFile)
	{
		FS_Close( cinTable.iFile );
		cinTable.iFile = 0;
	}
	cinTable.fileName[0] = 0;

	// let game known about movie state	
	cls.state = ca_disconnected;
	Cbuf_AddText ("killserver\n");
}

/*
==================
CIN_StopCinematic
==================
*/
void CIN_StopCinematic( void )
{
	// not playing
	if( cls.state != ca_cinematic )
		return;

	cinTable.status = FMV_EOF;
	RoQShutdown();
}

/*
==================
CIN_RunCinematic

Fetch and decompress the pending frame
==================
*/
e_status CIN_RunCinematic( void )
{
	float	start = 0;
	float     thisTime = 0;

	if(cinTable.status == FMV_EOF) return FMV_EOF;
	if ( cls.state != ca_cinematic ) return cinTable.status;

	thisTime = cls.realtime;

	cinTable.tfps = ((((cls.realtime) - cinTable.startTime) * 0.3) / 0.01); // 30.0 fps as default
	start = cinTable.startTime;

	while((cinTable.tfps != cinTable.numQuads) && (cinTable.status == FMV_PLAY)) 
	{
		RoQInterrupt();
		if (start != cinTable.startTime)
		{
			cinTable.tfps = ((((cls.realtime) - cinTable.startTime)*0.3)/0.01);
			start = cinTable.startTime;
		}
	}

	cinTable.lastTime = thisTime;
	if(cinTable.status == FMV_LOOPED) 
		cinTable.status = FMV_PLAY;

	if (cinTable.status == FMV_EOF)
	{
		if (cinTable.looping) RoQReset();
		else RoQShutdown();
	}
	return cinTable.status;
}

void CIN_SetExtents(int x, int y, int w, int h)
{
	if(cinTable.status == FMV_EOF)
		return;

	cinTable.xpos = x;
	cinTable.ypos = y;
	cinTable.width = w;
	cinTable.height = h;
	cinTable.dirty = true;
}

void CIN_SetLooping(bool loop)
{
	if(cinTable.status == FMV_EOF)
		return;
	cinTable.looping = loop;
}

/*
==================
CIN_PlayCinematic

==================
*/
bool CIN_PlayCinematic( const char *arg, int x, int y, int w, int h, int systemBits )
{
	word	RoQID;
	char	name[MAX_OSPATH];

	memset(&cin, 0, sizeof(cinematics_t));
	sprintf(name, "video/%s", arg);
	FS_DefaultExtension(name, ".roq" );
	strcpy(cinTable.fileName, name);

	cinTable.ROQSize = 0;
	cinTable.iFile = FS_Open(cinTable.fileName, "rb" );
	if(!cinTable.iFile) goto shutdown; // not found

	FS_Seek(cinTable.iFile, 0, SEEK_END);
	cinTable.ROQSize = FS_Tell(cinTable.iFile);
	FS_Seek(cinTable.iFile, 0, SEEK_SET);
	CIN_SetExtents(x, y, w, h);
	CIN_SetLooping((systemBits & CIN_loop) != 0);

	cinTable.CIN_HEIGHT = DEFAULT_CIN_HEIGHT;
	cinTable.CIN_WIDTH = DEFAULT_CIN_WIDTH;
	cinTable.holdAtEnd = (systemBits & CIN_hold) != 0;
	cinTable.silent = (systemBits & CIN_silent) != 0;

	initRoQ();
	FS_Read (cinTable.iFile, cin.file, 16 );
	RoQID = (word)(cin.file[0]) + (word)(cin.file[1]) * 256;

	if (RoQID == 0x1084)
	{
		RoQ_init();
		cinTable.status = FMV_PLAY;
		cls.state = ca_cinematic;

		M_ForceMenuOff();
		Con_Close();

		s_rawend = s_soundtime;
		return true;
	}

shutdown:
	MsgWarn("CIN_PlayCinematic: can't loading %s\n", arg );
	RoQShutdown();
	return false;
}

/*
==================
CIN_DrawCinematic

==================
*/
void CIN_DrawCinematic( void )
{
	float	x, y, w, h;
	byte	*buf;

	if(cinTable.status == FMV_EOF || !cinTable.buf)
		return;

	if(cls.state != ca_cinematic) return;

	x = cinTable.xpos;
	y = cinTable.ypos;
	w = cinTable.width;
	h = cinTable.height;
	buf = cinTable.buf;
	SCR_AdjustSize( &x, &y, &w, &h );

	if (cinTable.dirty && (cinTable.CIN_WIDTH != cinTable.drawX || cinTable.CIN_HEIGHT != cinTable.drawY))
	{
		int ix, iy, *buf2, *buf3, xm, ym, ll;
                
		xm = cinTable.CIN_WIDTH/256;
		ym = cinTable.CIN_HEIGHT/256;
		ll = 8;
		if (cinTable.CIN_WIDTH == 512) ll = 9;
		buf3 = (int*)buf;
		buf2 = Z_Malloc( 256 * 256 * 4 );
 		if (xm == 2 && ym == 2)
 		{
                    	byte *bc2, *bc3;
                    	int ic, iiy;
                    
			bc2 = (byte *)buf2;
			bc3 = (byte *)buf3;
			
			for (iy = 0; iy < 256; iy++)
			{
				iiy = iy<<12;
				for (ix = 0; ix < 2048; ix += 8)
				{
					for(ic = ix; ic < (ix+4); ic++)
					{
						*bc2 = (bc3[iiy+ic]+bc3[iiy+4+ic]+bc3[iiy+2048+ic]+bc3[iiy+2048+4+ic])>>2;
						bc2++;
					}
				}
			}
		}
		else if (xm == 2 && ym == 1)
		{
                    	byte	*bc2, *bc3;
                    	int	ic, iiy;
                    
			bc2 = (byte *)buf2;
                    	bc3 = (byte *)buf3;
                    	for (iy = 0; iy < 256; iy++)
                    	{
				iiy = iy<<11;
				for (ix = 0; ix < 2048; ix += 8)
				{
					for(ic = ix; ic < (ix+4); ic++)
					{
						*bc2=(bc3[iiy+ic]+bc3[iiy+4+ic])>>1;
						bc2++;
					}
				}
			}
		}
		else
		{
                    	for (iy = 0; iy < 256; iy++)
                    	{
				for (ix = 0; ix < 256; ix++)
				{
					buf2[(iy<<8)+ix] = buf3[((iy*ym)<<ll) + (ix*xm)];
				}
			}
		}
		re->DrawStretchRaw( x, y, w, h, 256, 256, (byte *)buf2, true );
		cinTable.dirty = false;
		Z_Free( buf2 );
		return;
	}

	re->DrawStretchRaw( x, y, w, h, cinTable.drawX, cinTable.drawY, buf, cinTable.dirty );
	cinTable.dirty = false;
}

/*
==============================================================================

Cinematic user interface
==============================================================================
*/
void SCR_PlayCinematic( char *name, int bits )
{
	if (cls.state == ca_cinematic)
		SCR_StopCinematic();

	S_StopAllSounds();

	if (CIN_PlayCinematic( name, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, bits ))
		SCR_RunCinematic(); // load first frame
}

void SCR_DrawCinematic( void )
{
	CIN_DrawCinematic();
}

void SCR_RunCinematic( void )
{
	CIN_RunCinematic();
}

void SCR_StopCinematic( void )
{
	CIN_StopCinematic();
	S_StopAllSounds();
}

/*
====================
SCR_FinishCinematic

Called when either the cinematic completes, or it is aborted
====================
*/
void SCR_FinishCinematic( void )
{
	// tell the server to advance to the next map / cinematic
	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print(&cls.netchan.message, va("nextserver %i\n", cl.servercount));
}
