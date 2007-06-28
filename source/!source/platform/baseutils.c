//=======================================================================
//			Copyright XashXT Group 2007 ©
//			baseutils.c - platform utils
//=======================================================================

#include "platform.h"
#include <winreg.h>
#include "baseutils.h"
#include "blankframe.h"

//=======================================================================
//			BYTE ORDER FUNCTIONS
//=======================================================================
dword BuffBigLong (const byte *buffer){ return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];}
word BuffBigShort (const byte *buffer){ return (buffer[0] << 8) | buffer[1]; }
dword BuffLittleLong (const byte *buffer){ return (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];}
word BuffLittleShort (const byte *buffer){return (buffer[1] << 8) | buffer[0];}

char *strupr (char *start)
{
	char *in;
	in = start;
	while (*in)
	{
		*in = toupper(*in);
		in++;
	}
	return start;
}

char *strlower (char *start)
{
	char *in;
	in = start;
	while (*in)
	{
		*in = tolower(*in); 
		in++;
	}
	return start;
}

char *copystring(char *s)
{
	char	*b;
	b = Malloc(strlen(s)+1);
	strcpy (b, s);
	return b;
}

// search case-insensitive for string2 in string
char *stristr( const char *string, const char *string2 )
{
	int c, len;
	c = tolower( *string2 );
	len = strlen( string2 );

	while (string)
	{
		for ( ; *string && tolower( *string ) != c; string++ );
		if (*string)
		{
			if (strnicmp( string, string2, len ) == 0)
				break;
			string++;
		}
		else return NULL;
	}
	return (char *)string;
}

//=======================================================================
//			PARSING STUFF
//=======================================================================

typedef struct
{
	char	filename[1024];
	byte	*buffer; 
	byte	*script_p;
	byte	*end_p;
	int	line;
} script_t;

//max included scripts
#define	MAX_INCLUDES	32

script_t	scriptstack[ MAX_INCLUDES ];
script_t	*script;
int	scriptline;

char token[ MAX_SYSPATH ]; //contains token info
char g_TXcommand; //only for internal use

bool endofscript;
bool tokenready; // only true if UnGetToken was just called
bool EndOfScript (bool newline);

/*
==============
AddScriptToStack
==============
*/
bool AddScriptToStack(char *name, byte *buffer, int size)
{
	if (script == &scriptstack[MAX_INCLUDES - 1])
	{
		Msg("script file exceeded limit %d", MAX_INCLUDES );
		return false;
	}
          if(!buffer || !size) return false;
	
	script++;
	strcpy (script->filename, name );
	script->buffer = buffer;
	script->line = 1;
	script->script_p = script->buffer;
	script->end_p = script->buffer + size;

	return true;
}

bool FS_LoadScript( char *filename )
{
	int size, result;
	byte *buf = FS_LoadFile (filename, &size );

	script = scriptstack;
	result = AddScriptToStack(filename, buf, size);
	if(result)MsgDev("Load script %s\n", filename );

	endofscript = false;
	tokenready = false;
	return result;
}

bool FS_AddScript( char *filename )
{
	int size, result;
	byte *buf = FS_LoadFile (filename, &size );


	result = AddScriptToStack(filename, buf, size);
	if(result) MsgDev("Insert script %s\n", filename );

	return result;
}

bool MS_LoadScript( char *buf, int size )
{
	int result;
	
	script = scriptstack;
	result = AddScriptToStack("script buffer", buf, size );

	endofscript = false;
	tokenready = false;
	return result;
}

/*
==============
GetToken
==============
*/
bool GetToken (bool newline)
{
	char	*token_p;

	if (tokenready) // is a token allready waiting?
	{
		tokenready = false;
		return true;
	}

	if (script->script_p >= script->end_p)
		return EndOfScript (newline);

	
skip_whitespace:	// skip whitespace
	while (*script->script_p <= 32)
	{
		if (script->script_p >= script->end_p)
			return EndOfScript (newline);
		if (*script->script_p++ == '\n')
		{
			if (!newline) goto line_incomplete;
			scriptline = script->line++;
		}
	}

	if (script->script_p >= script->end_p)
		return EndOfScript (newline);

	// ; # // comments
	if (*script->script_p == ';' || *script->script_p == '#' || ( script->script_p[0] == '/' && script->script_p[1] == '/') )
	{
		if (!newline) goto line_incomplete;

		//ets+++
		if (*script->script_p == '/') script->script_p++;
		if (script->script_p[1] == 'T' && script->script_p[2] == 'X')
			g_TXcommand = script->script_p[3];//TX#"-style comment
		while (*script->script_p++ != '\n')
			if (script->script_p >= script->end_p)
				return EndOfScript (newline);
		goto skip_whitespace;
	}

	// /* */ comments
	if (script->script_p[0] == '/' && script->script_p[1] == '*')
	{
		if (!newline) goto line_incomplete;
		script->script_p+=2;
		while (script->script_p[0] != '*' && script->script_p[1] != '/')
		{
			script->script_p++;
			if (script->script_p >= script->end_p)
				return EndOfScript (newline);
		}
		script->script_p += 2;
		goto skip_whitespace;
	}

	// copy token
	token_p = token;

	if (*script->script_p == '"')
	{
		// quoted token
		script->script_p++;
		while (*script->script_p != '"')
		{
			if (token_p == &token[MAX_SYSPATH - 1])
			{
				Msg("GetToken: Token too large on line %i\n",scriptline);
				break;
			}
			
			*token_p++ = *script->script_p++;
			if (script->script_p == script->end_p)
				break;
		}
		script->script_p++;
	}
	else // regular token
	{
		while ( *script->script_p > 32 && *script->script_p != ';')
		{
			if (token_p == &token[MAX_SYSPATH - 1])
			{
				Msg("GetToken: Token too large on line %i\n",scriptline);
				break;
			}
		
			*token_p++ = *script->script_p++;
			if (script->script_p == script->end_p)
				break;
		}
          }
          
	*token_p = 0;

	//quake style include & default MSVC style
	if (!strcmp(token, "$include") || !strcmp(token, "#include"))
	{
		GetToken (false);
		FS_AddScript(token);
		return GetToken (newline);
	}
	return true;

line_incomplete:

	//invoke error
	return EndOfScript( newline );
}
 
/*
==============
EndOfScript
==============
*/
bool EndOfScript (bool newline)
{
	if (!newline)
		Sys_Error ("Line %i is incomplete\n", scriptline);

	if (!strcmp (script->filename, "script buffer"))
	{
		endofscript = true;
		return false;
	}

	//Free (script->buffer);
	if (script == scriptstack + 1)
	{
		endofscript = true;
		return false;
	}

	script--;
	scriptline = script->line;
	endofscript = true;

	MsgDev("returning to %s\n", script->filename);

	return false;
}

/*
==============
TokenAvailable
==============
*/
bool TokenAvailable (void)
{
	char    *search_p;

	search_p = script->script_p;

	if(search_p >= script->end_p)
		return false;

	while ( *search_p <= 32)
	{
		if (*search_p == '\n')
			return false;
		search_p++;
		if (search_p == script->end_p)
			return false;
	}

	if (*search_p == ';')
		return false;
	return true;
}


/*
==============
SC_ParseToken

Parse a token out of a string
==============
*/
char *SC_ParseToken(const char **data_p)
{
	int		c, len;
	const char	*data;

	data = *data_p;
	len = 0;
	token[0] = 0;
	
	if (!data)
	{
		*data_p = NULL;
		return NULL;
	}
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
		{
			endofscript = true;
			*data_p = NULL;
			return NULL; // end of file;
		}
		data++;
	}
	
	// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	// skip /* comments
	if (c=='/' && data[1] == '*')
	{
		while (data[1] && (data[0] != '*' || data[1] != '/'))
			data++;
		data += 2;
		goto skipwhite;
	}
	

	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while( 1 )
		{
			c = *data++;
			if (c=='\"'|| c== '\0')
			{
				token[len] = 0;
				*data_p = data;
				return token;
			}
			token[len] = c;
			len++;
		}
	}

	// parse single characters
	if (c == '{' || c == '}'|| c == ')' || c == '(' || c == '\'' || c == ':' || c == ',')
	{
		token[len] = c;
		len++;
		token[len] = 0;
		*data_p = data;
		return token + 1;
	}

	// parse a regular word
	do
	{
		token[len] = c;
		data++;
		len++;
		c = *data;
		if (c == '{' || c == '}'|| c == ')'|| c == '(' || c == '\'' || c == ':' || c == '\"' || c == ',')
			break;
	} while(c > 32);
	
	token[len] = 0;
	*data_p = data;
	return token;
}

/*
=============================================================================

MAIN PUBLIC FUNCTIONS

=============================================================================
*/

/*
==============
Match Token With

check current token for match with user keyword
==============
*/
bool SC_MatchToken( char *match )
{
	if (!strcmp( token, match ))
		return true;
	return false;
}

/*
==============
SC_SkipToken

skip current token and jump into newline
==============
*/
void SC_SkipToken( void )
{
	GetToken( true );
	tokenready = true;
}

/*
==============
SC_FreeToken

release current token to get 
him again with SC_GetToken()
==============
*/
void SC_FreeToken( void )
{
	tokenready = true;
}

/*
==============
SC_TryToken

check token for available on current line
==============
*/
bool SC_TryToken( void )
{
	if(!TokenAvailable())
		return false;

	GetToken( false );
	return true;
}


/*
==============
SC_GetToken

get token on current or newline
==============
*/
char *SC_GetToken( bool newline )
{
	if(GetToken( newline ))
		return token;
	return NULL;
}


//=======================================================================
//			GET CPU INFO
//=======================================================================
typedef struct register_s
{
	unsigned long eax;
	unsigned long ebx;
	unsigned long ecx;
	unsigned long edx;
	bool retval;
}register_t;

static register_t cpuid(unsigned int function )
{
	register_t local;
          
          local.retval = true;
	
	_asm pushad;

	__try
	{
		_asm
		{
			xor edx, edx	// Clue the compiler that EDX is about to be used.
			mov eax, function   // set up CPUID to return processor version and features
					// 0 = vendor string, 1 = version info, 2 = cache info
			cpuid		// code bytes = 0fh,  0a2h
			mov local.eax, eax	// features returned in eax
			mov local.ebx, ebx	// features returned in ebx
			mov local.ecx, ecx	// features returned in ecx
			mov local.edx, edx	// features returned in edx
		}
	} 

	__except(EXCEPTION_EXECUTE_HANDLER) 
	{ 
		local.retval = false; 
	}

	_asm popad
          //Com_Printf("return retval %d\n", retval );
	return local;
}

bool CheckMMXTechnology(void)
{
	register_t mmx = cpuid(1);

	if( !mmx.retval ) return false;
	return ( mmx.edx & 0x800000 ) != 0;
}

bool CheckSSETechnology(void)
{
	register_t sse = cpuid(1);
	
	if( !sse.retval ) return false;
	return ( sse.edx & 0x2000000L ) != 0;
}

bool CheckSSE2Technology(void)
{
	register_t sse2 = cpuid(1);

	if( !sse2.retval ) return false;
	return ( sse2.edx & 0x04000000 ) != 0;
}

bool Check3DNowTechnology(void)
{
	register_t amd = cpuid( 0x80000000 );
	
	if( !amd.retval ) return false;
	if ( amd.eax > 0x80000000L )
	{
		amd = cpuid( 0x80000001 );
		if( !amd.retval ) return false;
		return ( amd.edx & 1<<31 ) != 0;
	}
	return false;
}

bool CheckCMOVTechnology()
{
	register_t cmov = cpuid(1);

	if( !cmov.retval ) return false;
	return ( cmov.edx & (1<<15) ) != 0;
}

bool CheckFCMOVTechnology(void)
{
	register_t fcmov = cpuid(1);

	if( !fcmov.retval ) return false;
	return ( fcmov.edx & (1<<16) ) != 0;
}

bool CheckRDTSCTechnology(void)
{
	register_t rdtsct = cpuid(1);

	if( !rdtsct.retval ) return false;
	return ( rdtsct.edx & 0x10 ) != 0;
}

// Return the Processor's vendor identification string, or "Generic_x86" if it doesn't exist on this CPU
const char* GetProcessorVendorId()
{
	static char VendorID[13];
	register_t vendor = cpuid(0);
	
	memset( VendorID, 0, sizeof(VendorID) );
	if( !vendor.retval )
	{
		strcpy( VendorID, "Generic_x86" ); 
	}
	else
	{
		memcpy( VendorID+0, &(vendor.ebx), sizeof( vendor.ebx ) );
		memcpy( VendorID+4, &(vendor.edx), sizeof( vendor.edx ) );
		memcpy( VendorID+8, &(vendor.ecx), sizeof( vendor.ecx ) );
	}
	return VendorID;
}

// Returns non-zero if Hyper-Threading Technology is supported on the processors and zero if not.  This does not mean that 
// Hyper-Threading Technology is necessarily enabled.
static bool HTSupported(void)
{
	const unsigned int HT_BIT		= 0x10000000;	// EDX[28] - Bit 28 set indicates Hyper-Threading Technology is supported in hardware.
	const unsigned int FAMILY_ID		= 0x0f00;		// EAX[11:8] - Bit 11 thru 8 contains family processor id
	const unsigned int EXT_FAMILY_ID	= 0x0f00000;	// EAX[23:20] - Bit 23 thru 20 contains extended family  processor id
	const unsigned int PENTIUM4_ID	= 0x0f00;		// Pentium 4 family processor id

          register_t intel1 = cpuid(0);
          register_t intel2 = cpuid(1);
        
	if(!intel1.retval || !intel2.retval ) return false;

	//  Check to see if this is a Pentium 4 or later processor
	if (((intel2.eax & FAMILY_ID) ==  PENTIUM4_ID) || (intel2.eax & EXT_FAMILY_ID))
		if (intel1.ebx == 'uneG' && intel1.edx == 'Ieni' && intel1.ecx == 'letn')
			return (intel2.edx & HT_BIT) != 0; // Genuine Intel Processor with Hyper-Threading Technology
	return false;  // This is not a genuine Intel processor.
}

// Returns the number of logical processors per physical processors.
static unsigned char LogicalProcessorsPerPackage(void)
{
	const unsigned NUM_LOGICAL_BITS = 0x00FF0000; // EBX[23:16] indicate number of logical processors per package
          register_t core = cpuid(1);
          
	if (!HTSupported()) return 1; 
	if( !core.retval) return 1;

	return (unsigned char) ((core.ebx & NUM_LOGICAL_BITS) >> 16);
}

int64 ClockSample( void )
{
	static int64 m_time = 0;
	unsigned long* pSample = (unsigned long *)&m_time;
	__asm
	{
		mov	ecx, pSample
		rdtsc
		mov	[ecx],     eax
		mov	[ecx+4],   edx
	}
	return m_time;
}

// Measure the processor clock speed by sampling the cycle count, waiting
// for some fraction of a second, then measuring the elapsed number of cycles.
static int64 CalculateClockSpeed()
{
	LARGE_INTEGER waitTime, startCount, curCount;
	int scale = 5; // Take 1/32 of a second for the measurement.
	int64 start, end;
		
	QueryPerformanceCounter(&startCount);
	QueryPerformanceFrequency(&waitTime);
	waitTime.QuadPart >>= scale;

	start = ClockSample();
	do
	{
		QueryPerformanceCounter(&curCount);
	}
	while(curCount.QuadPart - startCount.QuadPart < waitTime.QuadPart);
	end = ClockSample();

	return (end - start) << scale;
}

cpuinfo_t GetCPUInformation( void )
{
	static cpuinfo_t pi;
	SYSTEM_INFO si;
	
	// Has the structure already been initialized and filled out?
	if( pi.m_size == sizeof(pi) ) return pi;

	// Redundant, but just in case the user somehow messes with the size.
	ZeroMemory(&pi, sizeof(pi));

	// Fill out the structure, and return it:
	pi.m_size = sizeof(pi);

	// Grab the processor frequency:
	pi.m_speed = CalculateClockSpeed();

	
	// Get the logical and physical processor counts:
	pi.m_usNumLogicCore = LogicalProcessorsPerPackage();


	ZeroMemory( &si, sizeof(si) );

	GetSystemInfo( &si );

	pi.m_usNumPhysCore = si.dwNumberOfProcessors / pi.m_usNumLogicCore;
	pi.m_usNumLogicCore *= pi.m_usNumPhysCore;

	// Make sure I always report at least one, when running WinXP with the /ONECPU switch, 
	// it likes to report 0 processors for some reason.
	if( pi.m_usNumPhysCore == 0 && pi.m_usNumLogicCore == 0 )
	{
		pi.m_usNumPhysCore = 1;
		pi.m_usNumLogicCore = 1;
	}

	// Determine Processor Features:
	pi.m_bRDTSC = CheckRDTSCTechnology();
	pi.m_bCMOV  = CheckCMOVTechnology();
	pi.m_bFCMOV = CheckFCMOVTechnology();
	pi.m_bMMX   = CheckMMXTechnology();
	pi.m_bSSE   = CheckSSETechnology();
	pi.m_bSSE2  = CheckSSE2Technology();
	pi.m_b3DNow = Check3DNowTechnology();

	pi.m_szCPUID = (char*)GetProcessorVendorId();
	return pi;
}

/*
================
ReadBMP

used for make sprites and models (old stuff)
================
*/


byte *ReadBMP (char *filename, byte **palette, int *width, int *height)
{
	byte  *buf_p, *pbBmpBits;
	byte  *buf, *pb, *pbPal = NULL;
	int i, filesize, columns, rows;
	ULONG cbBmpBits;
	ULONG cbPalBytes;
	ULONG biTrueWidth;
	bmp_t bhdr;

	RGBQUAD rgrgbPalette[256];

	// File exists?
	buf = buf_p = FS_LoadFile( filename, &filesize );
	if(!buf_p)
	{
		//blank_frame
		buf_p = (char *)blank_frame; 
		filesize = sizeof(blank_frame);
		Msg("Warning: couldn't load %s\n", filename );
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
		Msg("ReadBMP: only Windows-style BMP files supported (%s)\n", filename );
		return NULL;
	} 

	// Bogus info header check
	if (bhdr.fileSize != filesize)
	{
		Msg("ReadBMP: incorrect file size %i should be %i\n", filesize, bhdr.fileSize);
		return NULL;
          }
          
	// Bogus bit depth?  Only 8-bit supported.
	if (bhdr.bitsPerPixel != 8)
	{
		Msg("ReadBMP: %d not a 8 bit image\n", bhdr.bitsPerPixel );
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

//=======================================================================
//			MULTITHREAD SYSTEM
//=======================================================================
#define MAX_THREADS		64

int dispatch;
int workcount;
int oldf;
bool pacifier;
bool threaded;
void (*workfunction) (int);
int numthreads = -1;
CRITICAL_SECTION crit;
static int enter;

void ThreadLock (void)
{
	if (!threaded) return;
	EnterCriticalSection (&crit);
	if (enter) Sys_Error ("Recursive ThreadLock\n");
	enter = 1;
}

void ThreadUnlock (void)
{
	if (!threaded) return;
	if (!enter) Sys_Error ("ThreadUnlock without lock\n");
	enter = 0;
	LeaveCriticalSection (&crit);
}

int GetThreadWork (void)
{
	int	r;
	int	f;

	ThreadLock ();

	if (dispatch == workcount)
	{
		ThreadUnlock ();
		return -1;
	}

	f = 10*dispatch / workcount;
	if (f != oldf)
	{
		oldf = f;
		if (pacifier) Msg("%i...", f);
	}

	r = dispatch;
	dispatch++;
	ThreadUnlock ();

	return r;
}

void ThreadWorkerFunction (int threadnum)
{
	int		work;

	while (1)
	{
		work = GetThreadWork ();
		if (work == -1) break;
		workfunction(work);
	}
}

void ThreadSetDefault (void)
{
	if (numthreads == -1) // not set manually
	{
		//NOTE: we must init Plat_InitCPU() first
		numthreads = GI.cpunum;
		if (numthreads < 1 || numthreads > MAX_THREADS)
			numthreads = 1;
	}
	MsgDev("%i thread%s\n", numthreads, numthreads== 1 ? "" : "s" );
}

void RunThreadsOnIndividual (int workcnt, bool showpacifier, void(*func)(int))
{
	if (numthreads == -1) ThreadSetDefault ();
	workfunction = func;
	RunThreadsOn (workcnt, showpacifier, ThreadWorkerFunction);
}

/*
=============
RunThreadsOn
=============
*/
void RunThreadsOn (int workcnt, bool showpacifier, void(*func)(int))
{
	int	threadid[MAX_THREADS];
	HANDLE	threadhandle[MAX_THREADS];
	int	i;
	double	start, end;

	start = Plat_DoubleTime();
	dispatch = 0;
	workcount = workcnt;
	oldf = -1;
	pacifier = showpacifier;
	threaded = true;

	// run threads in parallel
	InitializeCriticalSection (&crit);

	if (numthreads == 1) func (0); // use same thread
	else
	{
		for (i = 0;i < numthreads; i++)
		{
			threadhandle[i] = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)func, (LPVOID)i, 0, &threadid[i]);
		}

		for (i=0 ; i<numthreads ; i++)
			WaitForSingleObject (threadhandle[i], INFINITE);
	}
	DeleteCriticalSection (&crit);

	threaded = false;
	end = Plat_DoubleTime();
	if (pacifier) Msg(" Done [%.2f sec]\n", end - start);
}

//=======================================================================
//			PATH BUILDER
//=======================================================================
char* FlipSlashes(char* string)
{
	while (*string)
	{
		if(PATHSEPARATOR(*string))
			*string = SYSTEM_SLASH_CHAR;
		string++;
	}
	return string;
}

void ExtractFilePath(const char* const path, char* dest)
{
	const char* src;
	src = path + strlen(path) - 1;

	// back up until a \ or the start
	while (src != path && !PATHSEPARATOR(*(src - 1)))
		src--;

	memcpy(dest, path, src - path);
	dest[src - path] = 0;
}