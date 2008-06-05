//=======================================================================
//			Copyright XashXT Group 2007 ©
//			   utils.c - platform utils
//=======================================================================

#include "platform.h"
#include "utils.h"
#include "bsplib.h"
#include "mdllib.h"
#include "blankframe.h"


int com_argc;
char **com_argv;
char gs_basedir[ MAX_SYSPATH ]; // initial dir before loading gameinfo.txt (used for compilers too)
string	gs_filename; // used for compilers only

unsigned __int64 __g_ProfilerStart;
unsigned __int64 __g_ProfilerEnd;
unsigned __int64 __g_ProfilerEnd2;
unsigned __int64 __g_ProfilerSpare;
unsigned __int64 __g_ProfilerTotalTicks;
double __g_ProfilerTotalMsec;

void Profile_Store( void )
{
	if (com.GameInfo->rdtsc)
	{
		__g_ProfilerSpare = __g_ProfilerEnd - __g_ProfilerStart - (__g_ProfilerEnd2 - __g_ProfilerEnd);
	}
}

void Profile_RatioResults( void )
{
	if (com.GameInfo->rdtsc)
	{
		unsigned __int64 total = __g_ProfilerEnd - __g_ProfilerStart - (__g_ProfilerEnd2 - __g_ProfilerEnd);
 		double ratio;
            
		if (total >= __g_ProfilerSpare)
		{
			ratio = (double)(total-__g_ProfilerSpare)/total;
			Msg("First code is %.2f%% faster\n", ratio * 100);
		}
		else
		{
			ratio = (double)(__g_ProfilerSpare-total)/__g_ProfilerSpare;
			Msg("Second code is %.2f%% faster\n", ratio * 100);
		}
	}
	else MsgWarn("--- Profiler not supported ---\n");
}

void _Profile_Results( const char *function )
{
	if (com.GameInfo->rdtsc)
	{
		unsigned __int64 total = __g_ProfilerEnd - __g_ProfilerStart - (__g_ProfilerEnd2 - __g_ProfilerEnd);
		double msec = (double)total / com.GameInfo->tickcount; 
		Msg("Profiling stats for %s()\n", function );
		Msg("----- ticks: %I64d -----\n", total);
		Msg("----- secs %f ----- \n", msec );
		__g_ProfilerTotalTicks += total;
		__g_ProfilerTotalMsec += msec;
	}
	else MsgWarn("--- Profiler not supported ---\n");
}

void Profile_Time( void )
{
	if (com.GameInfo->rdtsc)
	{
		Msg("Profiling results:\n");
		Msg("----- total ticks: %I64d -----\n", __g_ProfilerTotalTicks);
		Msg("----- total secs %f ----- \n", __g_ProfilerTotalMsec  );
	}
	else MsgWarn("--- Profiler not supported ---\n");
}

/*
================
Com_ValidScript

validate qc-script for unexcpected keywords
================
*/

bool Com_ValidScript( int scripttype )
{ 
	if(Com_MatchToken("$spritename") && scripttype != QC_SPRITEGEN )
	{
		Msg("%s probably spritegen qc.script, skipping...\n", gs_filename );
		return false;
	}
	else if (Com_MatchToken("$frame") && scripttype != QC_SPRITEGEN )
	{
		Msg("%s probably spritegen qc.script, skipping...\n", gs_filename );
		return false;
	}
	else if(Com_MatchToken( "$modelname" ) && scripttype != QC_STUDIOMDL )
	{
		Msg("%s probably studio qc.script, skipping...\n", gs_filename );
		return false;
	}	
	else if(Com_MatchToken( "$body" ) && scripttype != QC_STUDIOMDL )
	{
		Msg("%s probably studio qc.script, skipping...\n", gs_filename );
		return false;
	}
	else if(Com_MatchToken( "$videoname" ) && scripttype != QC_ROQLIB )
	{
		Msg("%s probably roq movie qc.script, skipping...\n", gs_filename );
		return false;
	}
	else if(Com_MatchToken( "$framemask" ) && scripttype != QC_ROQLIB )
	{
		Msg("%s probably roq movie qc.script, skipping...\n", gs_filename );
		return false;
	}
	else if(Com_MatchToken( "$wadname" ) && scripttype != QC_WADLIB )
	{
		Msg("%s probably wadlib qc.script, skipping...\n", gs_filename );
		return false;
	}
	else if(Com_MatchToken( "$addlump" ) && scripttype != QC_WADLIB )
	{
		Msg("%s probably wadlib qc.script, skipping...\n", gs_filename );
		return false;
	}
	return true;
};

void Com_CheckToken( const char *match )
{
	Com_GetToken( true );

	if( Com_MatchToken( match ))
	{
		Sys_Break( "\"%s\" not found\n" );
	}
}


void Com_Parse1DMatrix( int x, vec_t *m )
{
	int	i;

	Com_CheckToken( "(" );

	for( i = 0; i < x; i++ )
	{
		Com_GetToken( false );
		m[i] = com.atof( com_token );
	}
	Com_CheckToken( ")" );
}

void Com_Parse2DMatrix( int y, int x, vec_t *m )
{
	int	i;

	Com_CheckToken( "(" );

	for( i = 0; i < y; i++ )
	{
		Com_Parse1DMatrix( x, m+i*x );
	}
	Com_CheckToken( ")" );
}

void Com_Parse3DMatrix( int z, int y, int x, vec_t *m )
{
	int	i;

	Com_CheckToken( "(" );

	for( i = 0; i < z; i++ )
	{
		Com_Parse2DMatrix( y, x, m+i*x*y );
	}
	Com_CheckToken( ")" );
}

/*
========================================================================

.BMP image format

========================================================================
*/
typedef struct
{
	char	id[2];		//bmfh.bfType
	dword	fileSize;		//bmfh.bfSize
	dword	reserved0;	//bmfh.bfReserved1 + bmfh.bfReserved2
	dword	bitmapDataOffset;	//bmfh.bfOffBits
	dword	bitmapHeaderSize;	//bmih.biSize
	dword	width;		//bmih.biWidth
	dword	height;		//bmih.biHeight
	word	planes;		//bmih.biPlanes
	word	bitsPerPixel;	//bmih.biBitCount
	dword	compression;	//bmih.biCompression
	dword	bitmapDataSize;	//bmih.biSizeImage
	dword	hRes;		//bmih.biXPelsPerMeter
	dword	vRes;		//bmih.biYPelsPerMeter
	dword	colors;		//bmih.biClrUsed
	dword	importantColors;	//bmih.biClrImportant
	byte	palette[256][4];	//RGBQUAD palette
} bmp_t;

/*
================
ReadBMP

used for make sprites and models (old stuff)
================
*/

byte *ReadBMP (char *filename, byte **palette, int *width, int *height)
{
	byte	*buf_p, *pbBmpBits;
	byte	*buf, *pb, *pbPal = NULL;
	int	i, filesize, columns, rows;
	dword	cbBmpBits;
	dword	cbPalBytes;
	dword	biTrueWidth;
	bmp_t	bhdr;

	RGBQUAD rgrgbPalette[256];

	// File exists?
	buf = buf_p = FS_LoadFile( filename, &filesize );
	if(!buf_p)
	{
		//blank_frame
		buf_p = (char *)blank_frame; 
		filesize = sizeof(blank_frame);
		MsgWarn("ReadBMP: couldn't load %s, use blank image\n", filename );
	}

	bhdr.id[0] = *buf_p++;
	bhdr.id[1] = *buf_p++;				//move pointer
	bhdr.fileSize = LittleLong(*(long *)buf_p);		buf_p += 4;
	bhdr.reserved0 = LittleLong(*(long *)buf_p);		buf_p += 4;
	bhdr.bitmapDataOffset = LittleLong(*(long *)buf_p);	buf_p += 4;
	bhdr.bitmapHeaderSize = LittleLong(*(long *)buf_p);	buf_p += 4;
	bhdr.width = LittleLong(*(long *)buf_p);		buf_p += 4;
	bhdr.height = LittleLong(*(long *)buf_p);		buf_p += 4;
	bhdr.planes = LittleShort(*(short *)buf_p);		buf_p += 2;
	bhdr.bitsPerPixel = LittleShort(*(short *)buf_p);		buf_p += 2;
	bhdr.compression = LittleLong(*(long *)buf_p);		buf_p += 4;
	bhdr.bitmapDataSize = LittleLong(*(long *)buf_p);		buf_p += 4;
	bhdr.hRes = LittleLong(*(long *)buf_p);			buf_p += 4;
	bhdr.vRes = LittleLong(*(long *)buf_p);			buf_p += 4;
	bhdr.colors = LittleLong(*(long *)buf_p);		buf_p += 4;
	bhdr.importantColors = LittleLong(*(long *)buf_p);	buf_p += 4;
	memcpy( bhdr.palette, buf_p, sizeof( bhdr.palette ));
	
	// Bogus file header check
	if (bhdr.reserved0 != 0) return NULL;

	if (memcmp(bhdr.id, "BM", 2))
	{
		MsgWarn("ReadBMP: only Windows-style BMP files supported (%s)\n", filename );
		return NULL;
	} 

	// Bogus info header check
	if (bhdr.fileSize != filesize)
	{
		MsgWarn("ReadBMP: incorrect file size %i should be %i\n", filesize, bhdr.fileSize);
		return NULL;
          }
          
	// Bogus bit depth?  Only 8-bit supported.
	if (bhdr.bitsPerPixel != 8)
	{
		MsgWarn("ReadBMP: %d not a 8 bit image\n", bhdr.bitsPerPixel );
		return NULL;
	}
	
	// Bogus compression?  Only non-compressed supported.
	if (bhdr.compression != BI_RGB) 
	{
		Msg("ReadBMP: it's compressed file\n");
		return NULL;
          }
          
	// Figure out how many entires are actually in the table
	if (bhdr.colors == 0)
	{
		bhdr.colors = 256;
		cbPalBytes = (1 << bhdr.bitsPerPixel) * sizeof( RGBQUAD );
	}
	else cbPalBytes = bhdr.colors * sizeof( RGBQUAD );
	memcpy(rgrgbPalette, &bhdr.palette, cbPalBytes );	// Read palette (bmih.biClrUsed entries)

	// convert to a packed 768 byte palette
	pbPal = Malloc(768);
	pb = pbPal;

	// Copy over used entries
	for (i = 0; i < (int)bhdr.colors; i++)
	{
		*pb++ = rgrgbPalette[i].rgbRed;
		*pb++ = rgrgbPalette[i].rgbGreen;
		*pb++ = rgrgbPalette[i].rgbBlue;
	}

	// Fill in unused entires will 0,0,0
	for (i = bhdr.colors; i < 256; i++) 
	{
		*pb++ = 0;
		*pb++ = 0;
		*pb++ = 0;
	}

	// Read bitmap bits (remainder of file)
	columns = bhdr.width, rows = bhdr.height;
	if ( rows < 0 ) rows = -rows;
	cbBmpBits = columns * rows;
          buf_p += 1024;//move pointer
          
	pb = buf_p;
	pbBmpBits = Malloc(cbBmpBits);

	// data is actually stored with the width being rounded up to a multiple of 4
	biTrueWidth = (bhdr.width + 3) & ~3;
	
	// reverse the order of the data.
	pb += (bhdr.height - 1) * biTrueWidth;
	for(i = 0; i < bhdr.height; i++)
	{
		memmove(&pbBmpBits[biTrueWidth * i], pb, biTrueWidth);
		pb -= biTrueWidth;
	}

	pb += biTrueWidth;
	*width = bhdr.width;
	*height = bhdr.height;

	// Set output parameters
	*palette = pbPal;

	//release buffer if need
	if( buf ) Mem_Free( buf );

	return pbBmpBits;
}