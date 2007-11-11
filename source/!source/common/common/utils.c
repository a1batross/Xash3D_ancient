//=======================================================================
//			Copyright XashXT Group 2007 ©
//			   utils.c - platform utils
//=======================================================================

#include "platform.h"
#include "utils.h"
#include "bsplib.h"
#include "mdllib.h"
#include "qcclib.h"
#include "blankframe.h"

int com_argc;
char **com_argv;
char gs_basedir[ MAX_SYSPATH ]; // initial dir before loading gameinfo.txt (used for compilers too)
char gs_mapname[ MAX_QPATH ]; // used for compilers only

/*
================
CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int FS_CheckParm (const char *parm)
{
	int i;

	for (i = 1; i < com_argc; i++ )
	{
		// NEXTSTEP sometimes clears appkit vars.
		if (!com_argv[i]) continue;
		if (!strcmp (parm, com_argv[i])) return i;
	}
	return 0;
}

bool FS_GetParmFromCmdLine( char *parm, char *out )
{
	int argc = FS_CheckParm( parm );

	if(!argc) return false;
	if(!out) return false;	
	if(!com_argv[argc + 1]) return false;

	strcpy( out, com_argv[argc+1] );
	return true;
}


//=======================================================================
//			INFOSTRING STUFF
//=======================================================================
/*
===============
Info_Print

printing current key-value pair
===============
*/
void Info_Print (char *s)
{
	char	key[512];
	char	value[512];
	char	*o;
	int	l;

	if (*s == '\\') s++;

	while (*s)
	{
		o = key;
		while (*s && *s != '\\') *o++ = *s++;

		l = o - key;
		if (l < 20)
		{
			memset (o, ' ', 20-l);
			key[20] = 0;
		}
		else *o = 0;
		Msg ("%s", key);

		if (!*s)
		{
			Msg ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\') *o++ = *s++;
		*o = 0;

		if (*s) s++;
		Msg ("%s\n", value);
	}
}

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
char *Info_ValueForKey (char *s, char *key)
{
	char	pkey[512];
	static	char value[2][512];	// use two buffers so compares work without stomping on each other
	static	int valueindex;
	char	*o;
	
	valueindex ^= 1;
	if (*s == '\\') s++;
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s) return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s) return "";
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) ) return value[valueindex];
		if (!*s) return "";
		s++;
	}
}

void Info_RemoveKey (char *s, char *key)
{
	char	*start;
	char	pkey[512];
	char	value[512];
	char	*o;

	if (strstr (key, "\\")) return;

	while (1)
	{
		start = s;
		if (*s == '\\') s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s) return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s) return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
		{
			strcpy (start, s);	// remove this part
			return;
		}
		if (!*s) return;
	}
}

/*
==================
Info_Validate

Some characters are illegal in info strings because they
can mess up the server's parsing
==================
*/
bool Info_Validate (char *s)
{
	if (strstr (s, "\"")) return false;
	if (strstr (s, ";")) return false;
	return true;
}

void Info_SetValueForKey (char *s, char *key, char *value)
{
	char	newi[MAX_INFO_STRING], *v;
	int	c, maxsize = MAX_INFO_STRING;

	if (strstr (key, "\\") || strstr (value, "\\") )
	{
		Msg ("Can't use keys or values with a \\\n");
		return;
	}

	if (strstr (key, ";") )
	{
		Msg ("Can't use keys or values with a semicolon\n");
		return;
	}
	if (strstr (key, "\"") || strstr (value, "\"") )
	{
		Msg ("Can't use keys or values with a \"\n");
		return;
	}
	if (strlen(key) > MAX_INFO_KEY - 1 || strlen(value) > MAX_INFO_KEY-1)
	{
		Msg ("Keys and values must be < 64 characters.\n");
		return;
	}

	Info_RemoveKey (s, key);
	if (!value || !strlen(value)) return;
	sprintf (newi, "\\%s\\%s", key, value);

	if (strlen(newi) + strlen(s) > maxsize)
	{
		Msg ("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = newi;
	while (*v)
	{
		c = *v++;
		c &= 127;	// strip high bits
		if (c >= 32 && c < 127) *s++ = c;
	}
	*s = 0;
}

/*
=============================================================================

EXTERNAL INFOSTRING STUFF INTERFACE
=============================================================================
*/
infostring_api_t Info_GetAPI( void )
{
	static infostring_api_t	info;

	info.api_size = sizeof(infostring_api_t);

	info.Print = Info_Print;
	info.Validate = Info_Validate;
	info.RemoveKey = Info_RemoveKey;
	info.ValueForKey = Info_ValueForKey;
	info.SetValueForKey = Info_SetValueForKey;

	return info;
}

unsigned __int64 __g_ProfilerStart;
unsigned __int64 __g_ProfilerEnd;
unsigned __int64 __g_ProfilerEnd2;
unsigned __int64 __g_ProfilerSpare;
unsigned __int64 __g_ProfilerTotalTicks;
double __g_ProfilerTotalMsec;

void Profile_Store( void )
{
	if (std.GameInfo->rdtsc)
	{
		__g_ProfilerSpare = __g_ProfilerEnd - __g_ProfilerStart - (__g_ProfilerEnd2 - __g_ProfilerEnd);
	}
}

void Profile_RatioResults( void )
{
	if (std.GameInfo->rdtsc)
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
	if (std.GameInfo->rdtsc)
	{
		unsigned __int64 total = __g_ProfilerEnd - __g_ProfilerStart - (__g_ProfilerEnd2 - __g_ProfilerEnd);
		double msec = (double)total / std.GameInfo->tickcount; 
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
	if (std.GameInfo->rdtsc)
	{
		Msg("Profiling results:\n");
		Msg("----- total ticks: %I64d -----\n", __g_ProfilerTotalTicks);
		Msg("----- total secs %f ----- \n", __g_ProfilerTotalMsec  );
	}
	else MsgWarn("--- Profiler not supported ---\n");
}

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
	if( buf ) Free( buf );

	return pbBmpBits;
}

/*
=============================================================================

COMPILERS PACKAGE INTERFACE
=============================================================================
*/
compilers_api_t Comp_GetAPI( void )
{
	static compilers_api_t cp;

	cp.api_size = sizeof(compilers_api_t);

	cp.Studio = CompileStudioModel;
	cp.Sprite = CompileSpriteModel;
	cp.Image = ConvertImagePixels;
	cp.PrepareBSP = PrepareBSPModel;
	cp.BSP = CompileBSPModel;
	cp.PrepareDAT = PrepareDATProgs;
	cp.DAT = CompileDATProgs;
	cp.DecryptDAT = PR_decode;
	cp.PrepareROQ = PrepareROQVideo;
	cp.ROQ = MakeROQ;

	return cp;
}