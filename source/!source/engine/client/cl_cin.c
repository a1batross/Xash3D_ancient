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

/******************************************************************************
*
* Class:		trFMV
*
* Description:	RoQ/RnR manipulation routines
*				not entirely complete for first run
*
******************************************************************************/

static long ROQ_YY_tab[256];
static long ROQ_UB_tab[256];
static long ROQ_UG_tab[256];
static long ROQ_VG_tab[256];
static long ROQ_VR_tab[256];
static word vq2[256*16*4];
static word vq4[256*64*4];
static word vq8[256*256*4];
e_status CIN_StopCinematic(int handle);

typedef struct
{
	byte	linbuf[DEFAULT_CIN_WIDTH*DEFAULT_CIN_HEIGHT*4*2];
	byte	file[65536];
	short	sqrTable[256];

	uint	mcomp[256];
	byte	*qStatus[2][32768];

	long	oldXOff, oldYOff, oldysize, oldxsize;
	int	currentHandle;

} cinematics_t;

typedef struct
{
	char	fileName[MAX_OSPATH];
	int	CIN_WIDTH, CIN_HEIGHT;
	int	xpos, ypos, width, height;
	bool	looping, holdAtEnd, dirty, alterGameState, silent, shader;
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
	int	playonwalls;
	byte	*buf;
	long	drawX, drawY;
} cin_cache;

static cinematics_t	cin;
static cin_cache	cinTable[MAX_VIDEO_HANDLES];
static int	currentHandle = -1;
static int	CL_handle = -1;

void CIN_CloseAllVideos( void )
{
	int		i;

	for ( i = 0; i < MAX_VIDEO_HANDLES; i++ )
	{
		if (cinTable[i].fileName[0] != 0 )
		{
			CIN_StopCinematic(i);
		}
	}
}

static int CIN_HandleForVideo( void )
{
	int		i;

	for( i = 0; i < MAX_VIDEO_HANDLES; i++ )
	{
		if ( cinTable[i].fileName[0] == 0 )
			return i;
	}
	MsgWarn("CIN_HandleForVideo: none free\n" );
	return -1;
}

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

	spl = cinTable[currentHandle].samplesPerLine;
        
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

	if (!cinTable[currentHandle].half)
	{
		if (!cinTable[currentHandle].smootheddouble)
		{
			// normal height
			if (cinTable[currentHandle].samplesPerPixel == 2)
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
			else if (cinTable[currentHandle].samplesPerPixel == 4)
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
			else if (cinTable[currentHandle].samplesPerPixel == 1)
			{
				bbptr = (byte *)bptr;
				for(i = 0; i < two; i++)
				{
					*bbptr++ = cinTable[currentHandle].gray[*input++];
					*bbptr++ = cinTable[currentHandle].gray[*input++];
					*bbptr++ = cinTable[currentHandle].gray[*input++];
					*bbptr++ = cinTable[currentHandle].gray[*input]; input +=3;
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
			if (cinTable[currentHandle].samplesPerPixel == 2)
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
			else if (cinTable[currentHandle].samplesPerPixel == 4)
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
			else if (cinTable[currentHandle].samplesPerPixel == 1)
			{
				bbptr = (byte *)bptr;
				for(i = 0; i < two; i++)
				{
					y0 = (long)*input++;
					y1 = (long)*input++;
					y2 = (long)*input++;
					y3 = (long)*input; input+= 3;
					*bbptr++ = cinTable[currentHandle].gray[y0];
					*bbptr++ = cinTable[currentHandle].gray[y1];
					*bbptr++ = cinTable[currentHandle].gray[((y0*3)+y2)/4];
					*bbptr++ = cinTable[currentHandle].gray[((y1*3)+y3)/4];
					*bbptr++ = cinTable[currentHandle].gray[(y0+(y2*3))/4];
					*bbptr++ = cinTable[currentHandle].gray[(y1+(y3*3))/4];						
					*bbptr++ = cinTable[currentHandle].gray[y2];
					*bbptr++ = cinTable[currentHandle].gray[y3];
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
		if (cinTable[currentHandle].samplesPerPixel == 2)
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
		else if (cinTable[currentHandle].samplesPerPixel == 1)
		{
			bbptr = (byte *)bptr;
				
			for(i = 0; i < two; i++)
			{
				*bbptr++ = cinTable[currentHandle].gray[*input]; input+=2;
				*bbptr++ = cinTable[currentHandle].gray[*input]; input+=4;
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
		else if (cinTable[currentHandle].samplesPerPixel == 4)
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

	offset = cinTable[currentHandle].screenDelta;
	
	lowx = lowy = 0;
	bigx = cinTable[currentHandle].xsize;
	bigy = cinTable[currentHandle].ysize;

	if (bigx > cinTable[currentHandle].CIN_WIDTH) bigx = cinTable[currentHandle].CIN_WIDTH;
	if (bigy > cinTable[currentHandle].CIN_HEIGHT) bigy = cinTable[currentHandle].CIN_HEIGHT;

	if ((startX >= lowx) && (startX+quadSize) <= (bigx) && (startY+quadSize) <= (bigy) && (startY >= lowy) && quadSize <= MAXSIZE)
	{
		useY = startY;
		scroff = cin.linbuf + (useY+((cinTable[currentHandle].CIN_HEIGHT-bigy)>>1)+yOff)*(cinTable[currentHandle].samplesPerLine) + (((startX+xOff))*cinTable[currentHandle].samplesPerPixel);

		cin.qStatus[0][cinTable[currentHandle].onQuad  ] = scroff;
		cin.qStatus[1][cinTable[currentHandle].onQuad++] = scroff+offset;
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

	if (xOff == cin.oldXOff && yOff == cin.oldYOff && cinTable[currentHandle].ysize == cin.oldysize && cinTable[currentHandle].xsize == cin.oldxsize)
		return;

	cin.oldXOff = xOff;
	cin.oldYOff = yOff;
	cin.oldysize = cinTable[currentHandle].ysize;
	cin.oldxsize = cinTable[currentHandle].xsize;

	numQuadCels = (cinTable[currentHandle].CIN_WIDTH*cinTable[currentHandle].CIN_HEIGHT) / (16);
	numQuadCels += numQuadCels/4 + numQuadCels/16;
	numQuadCels += 64; // for overflow

	numQuadCels = (cinTable[currentHandle].xsize*cinTable[currentHandle].ysize) / (16);
	numQuadCels += numQuadCels/4;
	numQuadCels += 64; // for overflow

	cinTable[currentHandle].onQuad = 0;

	for(y = 0; y < (long)cinTable[currentHandle].ysize; y += 16) 
		for(x = 0; x < (long)cinTable[currentHandle].xsize; x += 16) 
			recurseQuad( x, y, 16, xOff, yOff );

	for(i = (numQuadCels - 64); i < numQuadCels; i++)
	{
		cin.qStatus[0][i] = temp; // eoq
		cin.qStatus[1][i] = temp; // eoq
	}
}

static void readQuadInfo( byte *qData )
{
	if (currentHandle < 0) return;

	cinTable[currentHandle].xsize = qData[0]+qData[1]*256;
	cinTable[currentHandle].ysize = qData[2]+qData[3]*256;
	cinTable[currentHandle].maxsize = qData[4]+qData[5]*256;
	cinTable[currentHandle].minsize = qData[6]+qData[7]*256;
	
	cinTable[currentHandle].CIN_HEIGHT = cinTable[currentHandle].ysize;
	cinTable[currentHandle].CIN_WIDTH = cinTable[currentHandle].xsize;

	cinTable[currentHandle].samplesPerLine = cinTable[currentHandle].CIN_WIDTH*cinTable[currentHandle].samplesPerPixel;
	cinTable[currentHandle].screenDelta = cinTable[currentHandle].CIN_HEIGHT*cinTable[currentHandle].samplesPerLine;

	cinTable[currentHandle].half = false;
	cinTable[currentHandle].smootheddouble = false;
	
	cinTable[currentHandle].VQ0 = cinTable[currentHandle].VQNormal;
	cinTable[currentHandle].VQ1 = cinTable[currentHandle].VQBuffer;

	cinTable[currentHandle].t[0] = (0 - (uint)cin.linbuf)+(uint)cin.linbuf+cinTable[currentHandle].screenDelta;
	cinTable[currentHandle].t[1] = (0 - ((uint)cin.linbuf + cinTable[currentHandle].screenDelta))+(uint)cin.linbuf;

	cinTable[currentHandle].drawX = cinTable[currentHandle].CIN_WIDTH;
	cinTable[currentHandle].drawY = cinTable[currentHandle].CIN_HEIGHT;
}

static void RoQPrepMcomp( long xoff, long yoff ) 
{
	long i, j, x, y, temp, temp2;

	i = cinTable[currentHandle].samplesPerLine;
	j = cinTable[currentHandle].samplesPerPixel;

	if( cinTable[currentHandle].xsize == (cinTable[currentHandle].ysize * 4) && !cinTable[currentHandle].half )
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
			cin.mcomp[(x*16)+y] = cinTable[currentHandle].normalBuffer0-(temp2+temp);
		}
	}
}

static void initRoQ( void ) 
{
	if (currentHandle < 0) return;

	cinTable[currentHandle].VQNormal = (void (*)(byte *, void *))blitVQQuad32fs;
	cinTable[currentHandle].VQBuffer = (void (*)(byte *, void *))blitVQQuad32fs;
	cinTable[currentHandle].samplesPerPixel = 4;
	ROQ_GenYUVTables();
	RllSetupTable();
}

static void RoQReset( void )
{
	if (currentHandle < 0) return;

	FS_Close( cinTable[currentHandle].iFile );
	cinTable[currentHandle].iFile = FS_Open(cinTable[currentHandle].fileName, "rb" );
	// let the background thread start reading ahead
	FS_Read(cinTable[currentHandle].iFile, cin.file, 16 );
	RoQ_init();
	cinTable[currentHandle].status = FMV_LOOPED;
}

static void RoQInterrupt( void )
{
	byte	*framedata;
	short	sbuf[32768];
	int	ssize;
        
	if(currentHandle < 0) return;

	FS_Read(cinTable[currentHandle].iFile, cin.file, cinTable[currentHandle].RoQFrameSize + 8 ); 

	if ( cinTable[currentHandle].RoQPlayed >= cinTable[currentHandle].ROQSize )
	{ 
		if (cinTable[currentHandle].holdAtEnd == false)
		{
			if (cinTable[currentHandle].looping) RoQReset();
			else cinTable[currentHandle].status = FMV_EOF;
		}
		else cinTable[currentHandle].status = FMV_IDLE;
		return; 
	}

	framedata = cin.file;

	// new frame is ready
redump:
	switch(cinTable[currentHandle].roq_id) 
	{
	case ROQ_QUAD_VQ:
		if ((cinTable[currentHandle].numQuads&1))
		{
			cinTable[currentHandle].normalBuffer0 = cinTable[currentHandle].t[1];
			RoQPrepMcomp( cinTable[currentHandle].roqF0, cinTable[currentHandle].roqF1 );
			cinTable[currentHandle].VQ1( (byte *)cin.qStatus[1], framedata);
			cinTable[currentHandle].buf = cin.linbuf + cinTable[currentHandle].screenDelta;
		}
		else
		{
			cinTable[currentHandle].normalBuffer0 = cinTable[currentHandle].t[0];
			RoQPrepMcomp( cinTable[currentHandle].roqF0, cinTable[currentHandle].roqF1 );
			cinTable[currentHandle].VQ0( (byte *)cin.qStatus[0], framedata );
			cinTable[currentHandle].buf = cin.linbuf;
		}
		if (cinTable[currentHandle].numQuads == 0)
		{		
			// first frame
			Mem_Copy(cin.linbuf+cinTable[currentHandle].screenDelta, cin.linbuf, cinTable[currentHandle].samplesPerLine*cinTable[currentHandle].ysize);
		}
		cinTable[currentHandle].numQuads++;
		cinTable[currentHandle].dirty = true;
		break;
	case ROQ_QUAD_JPEG:
		// not implemented
		break;
	case ROQ_CODEBOOK:
		decodeCodeBook( framedata, (word)cinTable[currentHandle].roq_flags );
		break;
	case ZA_SOUND_MONO:
		if (!cinTable[currentHandle].silent)
		{
			ssize = RllDecodeMonoToStereo( framedata, sbuf, cinTable[currentHandle].RoQFrameSize, 0, (word)cinTable[currentHandle].roq_flags);
			S_RawSamples( ssize, 22050, 2, 1, (byte *)sbuf, 1.0f );
		}
		break;
	case ZA_SOUND_STEREO:
		if (!cinTable[currentHandle].silent)
		{
			if (cinTable[currentHandle].numQuads == -1)
			{
				S_Update();
				s_rawend = s_soundtime;
			}
			ssize = RllDecodeStereoToStereo( framedata, sbuf, cinTable[currentHandle].RoQFrameSize, 0, (word)cinTable[currentHandle].roq_flags);
			S_RawSamples( ssize, 22050, 2, 2, (byte *)sbuf, 1.0f );
		}
		break;
	case ROQ_QUAD_INFO:
		if (cinTable[currentHandle].numQuads == -1)
		{
			readQuadInfo( framedata );
			setupQuad( 0, 0 );
			// we need to use CL_ScaledMilliseconds because of the smp mode calls from the renderer
			cinTable[currentHandle].startTime = cinTable[currentHandle].lastTime = cls.realtime;
		}
		if (cinTable[currentHandle].numQuads != 1) cinTable[currentHandle].numQuads = 0;
		break;
	case ROQ_PACKET:
		cinTable[currentHandle].inMemory = cinTable[currentHandle].roq_flags;
		cinTable[currentHandle].RoQFrameSize = 0;           // for header
		break;
	case ROQ_QUAD_HANG:
		cinTable[currentHandle].RoQFrameSize = 0;
		break;
	default:
		cinTable[currentHandle].status = FMV_EOF;
		break;
	}	

	// read in next frame data
	if ( cinTable[currentHandle].RoQPlayed >= cinTable[currentHandle].ROQSize )
	{ 
		if (cinTable[currentHandle].holdAtEnd == false)
		{
			if (cinTable[currentHandle].looping) RoQReset();
			else cinTable[currentHandle].status = FMV_EOF;
		}
		else cinTable[currentHandle].status = FMV_IDLE;
		return; 
	}
	
	framedata	+= cinTable[currentHandle].RoQFrameSize;
	cinTable[currentHandle].roq_id = framedata[0] + framedata[1]*256;
	cinTable[currentHandle].RoQFrameSize = framedata[2] + framedata[3]*256 + framedata[4]*65536;
	cinTable[currentHandle].roq_flags = framedata[6] + framedata[7]*256;
	cinTable[currentHandle].roqF0	= (char)framedata[7];
	cinTable[currentHandle].roqF1	= (char)framedata[6];

	if (cinTable[currentHandle].RoQFrameSize > 65536||cinTable[currentHandle].roq_id == 0x1084)
	{
		MsgDev(D_WARN, "RoQInterrupt: roq_size > 65536 || roq_id == 0x1084\n");
		cinTable[currentHandle].status = FMV_EOF;
		if (cinTable[currentHandle].looping) RoQReset();
		return;
	}
	if (cinTable[currentHandle].inMemory && (cinTable[currentHandle].status != FMV_EOF))
	{
		cinTable[currentHandle].inMemory--;
		framedata += 8;
		goto redump;
	}

	// one more frame hits the dust
	cinTable[currentHandle].RoQPlayed += cinTable[currentHandle].RoQFrameSize+8;
}

static void RoQ_init( void )
{
	// we need to use CL_ScaledMilliseconds because of the smp mode calls from the renderer
	cinTable[currentHandle].startTime = cinTable[currentHandle].lastTime = cls.realtime;

	cinTable[currentHandle].RoQPlayed = 24;

	// get frame rate	
	cinTable[currentHandle].roqFPS = cin.file[6] + cin.file[7] * 256;
	
	if (!cinTable[currentHandle].roqFPS) cinTable[currentHandle].roqFPS = 30;

	cinTable[currentHandle].numQuads = -1;

	cinTable[currentHandle].roq_id = cin.file[8] + cin.file[9]*256;
	cinTable[currentHandle].RoQFrameSize = cin.file[10] + cin.file[11] * 256 + cin.file[12] * 65536;
	cinTable[currentHandle].roq_flags = cin.file[14] + cin.file[15] * 256;                     
}

static void RoQShutdown( void )
{
	const char *s;

	if(!cinTable[currentHandle].buf) return;
	if( cinTable[currentHandle].status == FMV_IDLE ) return;

	MsgDev(D_INFO, "RoQShutdown: finished cinematic\n");
	cinTable[currentHandle].status = FMV_IDLE;

	if (cinTable[currentHandle].iFile)
	{
		FS_Close( cinTable[currentHandle].iFile );
		cinTable[currentHandle].iFile = 0;
	}

	if (cinTable[currentHandle].alterGameState)
	{
		cls.state = ca_disconnected;
		// we can't just do a vstr nextmap, because
		// if we are aborting the intro cinematic with
		// a devmap command, nextmap would be valid by
		// the time it was referenced
		s = Cvar_VariableString( "nextmap" );
		if ( s[0] )
		{
			Cbuf_ExecuteText( EXEC_APPEND, va("%s\n", s));
			Cvar_Set( "nextmap", "" );
		}
		CL_handle = -1;
	}
	cinTable[currentHandle].fileName[0] = 0;
	currentHandle = -1;
}

/*
==================
CIN_StopCinematic
==================
*/
e_status CIN_StopCinematic(int handle)
{
	if (handle < 0 || handle>= MAX_VIDEO_HANDLES || cinTable[handle].status == FMV_EOF)
		return FMV_EOF;
	currentHandle = handle;

	if (!cinTable[currentHandle].buf) return FMV_EOF;

	if (cinTable[currentHandle].alterGameState)
	{
		if ( cls.state != ca_active )
			return cinTable[currentHandle].status;
	}

	cinTable[currentHandle].status = FMV_EOF;
	RoQShutdown();

	return FMV_EOF;
}

/*
==================
CIN_RunCinematic

Fetch and decompress the pending frame
==================
*/
e_status CIN_RunCinematic (int handle)
{
	float	start = 0;
	float     thisTime = 0;

	if (handle < 0 || handle>= MAX_VIDEO_HANDLES || cinTable[handle].status == FMV_EOF)
		return FMV_EOF;

	if (cin.currentHandle != handle)
	{
		currentHandle = handle;
		cin.currentHandle = currentHandle;
		cinTable[currentHandle].status = FMV_EOF;
		RoQReset();
	}

	if (cinTable[handle].playonwalls < -1) return cinTable[handle].status;
	currentHandle = handle;

	if (cinTable[currentHandle].alterGameState)
	{
		if ( cls.state != ca_active )
		{
			return cinTable[currentHandle].status;
		}
	}

	if (cinTable[currentHandle].status == FMV_IDLE) return cinTable[currentHandle].status;

	thisTime = cls.realtime;
	if (cinTable[currentHandle].shader && (fabs(thisTime - cinTable[currentHandle].lastTime)) > 0.01)
	{
		cinTable[currentHandle].startTime += thisTime - cinTable[currentHandle].lastTime;
	}

	// we need to use CL_ScaledMilliseconds because of the smp mode calls from the renderer
	cinTable[currentHandle].tfps = ((((cls.realtime) - cinTable[currentHandle].startTime) * 0.3) / 0.01);

	start = cinTable[currentHandle].startTime;
	while((cinTable[currentHandle].tfps != cinTable[currentHandle].numQuads) && (cinTable[currentHandle].status == FMV_PLAY) ) 
	{
		RoQInterrupt();
		if (start != cinTable[currentHandle].startTime)
		{
			// we need to use CL_ScaledMilliseconds because of the smp mode calls from the renderer
			cinTable[currentHandle].tfps = ((((cls.realtime) - cinTable[currentHandle].startTime)*0.3)/0.01);
			start = cinTable[currentHandle].startTime;
		}
	}

	cinTable[currentHandle].lastTime = thisTime;
	if(cinTable[currentHandle].status == FMV_LOOPED) cinTable[currentHandle].status = FMV_PLAY;

	if (cinTable[currentHandle].status == FMV_EOF)
	{
		if (cinTable[currentHandle].looping) RoQReset();
		else RoQShutdown();
	}
	return cinTable[currentHandle].status;
}

void CIN_SetExtents (int handle, int x, int y, int w, int h)
{
	if (handle < 0 || handle >= MAX_VIDEO_HANDLES || cinTable[handle].status == FMV_EOF)
		return;

	cinTable[handle].xpos = x;
	cinTable[handle].ypos = y;
	cinTable[handle].width = w;
	cinTable[handle].height = h;
	cinTable[handle].dirty = true;
}

void CIN_SetLooping(int handle, bool loop)
{
	if (handle < 0 || handle>= MAX_VIDEO_HANDLES || cinTable[handle].status == FMV_EOF)
		return;
	cinTable[handle].looping = loop;
}

/*
==================
CIN_PlayCinematic

==================
*/
int CIN_PlayCinematic( const char *arg, int x, int y, int w, int h, int systemBits )
{
	word	RoQID;
	char	name[MAX_OSPATH];
	int	i;

	if (strstr(arg, "/") == NULL && strstr(arg, "\\") == NULL)
		sprintf (name, "video/%s", arg);
	else sprintf (name, "%s", arg);

	if (!(systemBits & CIN_system))
	{
		for ( i = 0; i < MAX_VIDEO_HANDLES; i++ )
		{
			if (!strcmp(cinTable[i].fileName, name))
				return i;
		}
	}

	memset(&cin, 0, sizeof(cinematics_t));
	currentHandle = CIN_HandleForVideo();
	cin.currentHandle = currentHandle;
	strcpy(cinTable[currentHandle].fileName, name);

	cinTable[currentHandle].ROQSize = 0;
	cinTable[currentHandle].iFile = FS_Open(cinTable[currentHandle].fileName, "rb" );

	FS_Seek(cinTable[currentHandle].iFile, 0, SEEK_END);
	cinTable[currentHandle].ROQSize = FS_Tell(cinTable[currentHandle].iFile);
	FS_Seek(cinTable[currentHandle].iFile, 0, SEEK_SET);

	if (cinTable[currentHandle].ROQSize <= 0)
	{
		MsgDev(D_WARN, "play(%s), ROQSize <= 0\n", arg);
		cinTable[currentHandle].fileName[0] = 0;
		return -1;
	}

	CIN_SetExtents(currentHandle, x, y, w, h);
	CIN_SetLooping(currentHandle, (systemBits & CIN_loop) != 0);

	cinTable[currentHandle].CIN_HEIGHT = DEFAULT_CIN_HEIGHT;
	cinTable[currentHandle].CIN_WIDTH = DEFAULT_CIN_WIDTH;
	cinTable[currentHandle].holdAtEnd = (systemBits & CIN_hold) != 0;
	cinTable[currentHandle].alterGameState = (systemBits & CIN_system) != 0;
	cinTable[currentHandle].playonwalls = 1;
	cinTable[currentHandle].silent = (systemBits & CIN_silent) != 0;
	cinTable[currentHandle].shader = (systemBits & CIN_shader) != 0;

	//M_MenuOff();

	initRoQ();
	FS_Read (cinTable[currentHandle].iFile, cin.file, 16 );
	RoQID = (word)(cin.file[0]) + (word)(cin.file[1]) * 256;

	if (RoQID == 0x1084)
	{
		RoQ_init();
		cinTable[currentHandle].status = FMV_PLAY;
		if (cinTable[currentHandle].alterGameState)
			cls.state = ca_active;
		cl.cinematictime = cls.realtime;
		//Con_Close();

		s_rawend = s_soundtime;
		return currentHandle;
	}

	MsgWarn("CIN_PlayCinematic: invalid RoQ ID\n");
	RoQShutdown();
	return -1;
}

/*
==================
CIN_DrawCinematic

==================
*/
void CIN_DrawCinematic (int handle)
{
	float	x, y, w, h;
	byte	*buf;

	if (handle < 0 || handle>= MAX_VIDEO_HANDLES || cinTable[handle].status == FMV_EOF)
		return;

	if (!cinTable[handle].buf) return;

	x = cinTable[handle].xpos;
	y = cinTable[handle].ypos;
	w = cinTable[handle].width;
	h = cinTable[handle].height;
	buf = cinTable[handle].buf;
//SCR_AdjustFrom640( &x, &y, &w, &h );

	if (cinTable[handle].dirty && (cinTable[handle].CIN_WIDTH != cinTable[handle].drawX || cinTable[handle].CIN_HEIGHT != cinTable[handle].drawY))
	{
		int ix, iy, *buf2, *buf3, xm, ym, ll;
                
		xm = cinTable[handle].CIN_WIDTH/256;
		ym = cinTable[handle].CIN_HEIGHT/256;
		ll = 8;
		if (cinTable[handle].CIN_WIDTH == 512) ll = 9;
		buf3 = (int*)buf;
		buf2 = Z_Malloc( 256 * 256 * 4 );
 		if (xm==2 && ym==2)
 		{
                    	byte	*bc2, *bc3;
                    	int	ic, iiy;
                    
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
		re->DrawStretchRaw( x, y, viddef.width, viddef.height, 256, 256, (byte *)buf2, true );
		cinTable[handle].dirty = false;
		Z_Free( buf2 );
		return;
	}

	re->DrawStretchRaw( x, y, viddef.width, viddef.height, cinTable[handle].drawX, cinTable[handle].drawY, buf, cinTable[handle].dirty );
	cinTable[handle].dirty = false;
}

void CL_PlayCinematic_f( void )
{
	char	*arg, *s;
	bool	holdatend;
	int	bits = CIN_system;

	if (cls.state == ca_active)
		SCR_StopCinematic();

	arg = Cmd_Argv( 1 );
	s = Cmd_Argv(2);

	holdatend = false;
	if (s && s[0] == '1') bits |= CIN_hold;
	if (s && s[0] == '2') bits |= CIN_loop;

	S_StopAllSounds();

	CL_handle = CIN_PlayCinematic( arg, 0, 0, 640, 480, bits );

	if (CL_handle >= 0)
	{
		//do {
			SCR_RunCinematic();
		//} while (cinTable[currentHandle].buf == NULL && cinTable[currentHandle].status == FMV_PLAY);		// wait for first frame (load codebook and sound)
	}
}

void SCR_PlayCinematic (char *name)
{
	if (cls.state == ca_active)
		SCR_StopCinematic();

	S_StopAllSounds();
	CL_handle = CIN_PlayCinematic( name, 0, 0, 640, 480, 0 );

	if (CL_handle >= 0)
	{
		//do {
			SCR_RunCinematic();
		//} while (cinTable[currentHandle].buf == NULL && cinTable[currentHandle].status == FMV_PLAY);		// wait for first frame (load codebook and sound)
	}
}

void SCR_DrawCinematic( void )
{
	if (CL_handle >= 0 && CL_handle < MAX_VIDEO_HANDLES)
	{
		CIN_DrawCinematic(CL_handle);
	}
}

void SCR_RunCinematic (void)
{
	if (CL_handle >= 0 && CL_handle < MAX_VIDEO_HANDLES)
	{
		CIN_RunCinematic(CL_handle);
	}
}

void SCR_StopCinematic(void)
{
	if (CL_handle >= 0 && CL_handle < MAX_VIDEO_HANDLES)
	{
		CIN_StopCinematic(CL_handle);
		S_StopAllSounds();
		CL_handle = -1;
	}
}

/*
====================
SCR_FinishCinematic

Called when either the cinematic completes, or it is aborted
====================
*/
void SCR_FinishCinematic (void)
{
	// tell the server to advance to the next map / cinematic
	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, va("nextserver %i\n", cl.servercount));
}
