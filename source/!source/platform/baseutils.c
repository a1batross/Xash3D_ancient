//=======================================================================
//			Copyright XashXT Group 2007 ©
//			baseutils.c - platform utils
//=======================================================================

#include "platform.h"
#include <winreg.h>
#include "baseutils.h"
#include "bsplib.h"
#include "mdllib.h"
#include "qcclib.h"
#include "blankframe.h"


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
	b = Malloc(strlen(s) + 1);
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

char token[ MAX_INPUTLINE ]; //contains token info
char g_TXcommand; //only for internal use

bool endofscript;
bool tokenready; // only true if UnGetToken was just called
bool EndOfScript (bool newline);

/*
==============
AddScriptToStack
==============
*/
bool AddScriptToStack(const char *name, byte *buffer, int size)
{
	if (script == &scriptstack[MAX_INCLUDES - 1])
	{
		MsgWarn("AddScriptToStack: script file limit exceeded %d\n", MAX_INCLUDES );
		return false;
	}
          if(!buffer || !size) return false;
	
	script++;
	strcpy (script->filename, name );
	script->buffer = buffer;
	script->line = scriptline = 1;
	script->script_p = script->buffer;
	script->end_p = script->buffer + size;

	return true;
}

bool FS_LoadScript( const char *filename, char *buf, int size )
{
	int result;

	if(!buf || size <= 0)
		buf = FS_LoadFile (filename, &size );

	script = scriptstack;
	result = AddScriptToStack( filename, buf, size);

	endofscript = false;
	tokenready = false;
	return result;
}

bool FS_AddScript( const char *filename, char *buf, int size )
{
	if(!buf || size <= 0)
		buf = FS_LoadFile (filename, &size );
	return AddScriptToStack(filename, buf, size);
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

		// ets+++
		if (*script->script_p == '/') script->script_p++;
		if (script->script_p[1] == 'T' && script->script_p[2] == 'X')
			g_TXcommand = script->script_p[3];//TX#"-style comment
		while (*script->script_p++ != '\n')
		{
			if (script->script_p >= script->end_p)
				return EndOfScript (newline);
		}
		goto skip_whitespace;
	}

	// /* */ comments
	if (script->script_p[0] == '/' && script->script_p[1] == '*')
	{
		if (!newline) goto line_incomplete;

		script->script_p += 2;
		while (script->script_p[0] != '*' && script->script_p[1] != '/')
		{
			if (script->script_p >= script->end_p)
				return EndOfScript (newline);
			if (*script->script_p++ == '\n')
			{
				if (!newline) goto line_incomplete;
				scriptline = script->line++;
			}
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
				MsgWarn("GetToken: Token too large on line %i\n", scriptline);
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
				MsgWarn("GetToken: Token too large on line %i\n",scriptline);
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
		FS_AddScript(token, NULL, 0 );
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
	{
		scriptline = script->line;
		Sys_Error ("%s: line %i is incomplete\n", script->filename, scriptline);
	}
	if (!strcmp (script->filename, "script buffer"))
	{
		endofscript = true;
		return false;
	}

	if (script == scriptstack + 1)
	{
		endofscript = true;
		return false;
	}

	script--;
	scriptline = script->line;
	endofscript = true;

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
	int		c;
	int		len = 0;
	const char	*data;
	
	token[0] = 0;
	data = *data_p;
	
	if (!data) 
	{
		endofscript = true;
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
		while(1)
		{
			c = *data++;
			if (c=='\"'||c=='\0')
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
		data++;
		len++;
		token[len] = 0;
		*data_p = data;
		return token;
	}

	// parse a regular word
	do
	{
		token[len] = c;
		data++;
		len++;
		c = *data;
		if (c == '{' || c == '}'|| c == ')'|| c == '(' || c == '\'' || c == ':' || c == ',')
			break;
	} while(c > 32);
	
	token[len] = 0;
	*data_p = data;
	return token;
}

/*
==============
SC_ParseWord

Parse a word out of a string
==============
*/
char *SC_ParseWord( const char **data_p )
{
	int		c;
	int		len = 0;
	const char	*data;
	
	token[0] = 0;
	data = *data_p;
	
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
			*data_p = NULL;
			endofscript = true;
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
	

	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		do
		{
			c = *data++;
			if (c=='\"' || c=='\0')
			{
				token[len] = 0;
				*data_p = data;
				return token;
			}
			token[len] = c;
			len++;
		} while (1);
	}

	// parse numbers
	if (c >= '0' && c <= '9')
	{
		if (c == '0' && data[1] == 'x')
		{
			//parse hex
			token[0] = '0';
			c='x';
			len=1;
			data++;
			while( 1 )
			{
				//parse regular number
				token[len] = c;
				data++;
				len++;
				c = *data;
				if ((c < '0'|| c > '9') && (c < 'a'||c > 'f') && (c < 'A'|| c > 'F') && c != '.')
					break;
			}

		}
		else
		{
			while( 1 )
			{
				//parse regular number
				token[len] = c;
				data++;
				len++;
				c = *data;
				if ((c < '0'|| c > '9') && c != '.')
					break;
			}
		}
		
		token[len] = 0;
		*data_p = data;
		return token;
	}

	// parse words
	else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')
	{
		do
		{
			token[len] = c;
			data++;
			len++;
			c = *data;
		} while ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_');
		
		token[len] = 0;
		*data_p = data;
		return token;
	}
	else
	{
		token[len] = c;
		len++;
		token[len] = 0;
		*data_p = data;
		return token;
	}
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
bool SC_MatchToken( const char *match )
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

/*
==============
SC_Token

return current token
==============
*/
char *SC_Token( void )
{
	return token;
}

/*
=============================================================================

EXTERNAL PARSE STUFF INTERFACE
=============================================================================
*/
scriptsystem_api_t Sc_GetAPI( void )
{
	static scriptsystem_api_t	sc;

	sc.api_size = sizeof(scriptsystem_api_t);

	sc.Load = FS_LoadScript;
	sc.Include = FS_AddScript;
	sc.GetToken = SC_GetToken;
	sc.Token = SC_Token;
	sc.TryToken = SC_TryToken;
	sc.FreeToken = SC_FreeToken;
	sc.SkipToken = SC_SkipToken;
	sc.MatchToken = SC_MatchToken;
	sc.ParseToken = SC_ParseToken;
	sc.ParseWord = SC_ParseWord;
	
	return sc;
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

//=======================================================================
//			GET CPU INFO
//=======================================================================
typedef struct register_s
{
	dword eax;
	dword ebx;
	dword ecx;
	dword edx;
	bool retval;
} register_t;

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
	if( amd.eax > 0x80000000L )
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
	register_t rdtsc = cpuid(1);

	if( !rdtsc.retval ) return false;
	return ( rdtsc.edx & 0x10 ) != 0;
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
	const uint HT_BIT	= 0x10000000;	// EDX[28] - Bit 28 set indicates Hyper-Threading Technology is supported in hardware.
	const uint FAMILY_ID = 0x0f00;	// EAX[11:8] - Bit 11 thru 8 contains family processor id
	const uint EXT_FAMILY_ID = 0x0f00000;	// EAX[23:20] - Bit 23 thru 20 contains extended family  processor id
	const uint PENTIUM4_ID = 0x0f00;	// Pentium 4 family processor id

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
static byte LogicalProcessorsPerPackage(void)
{
	const unsigned NUM_LOGICAL_BITS = 0x00FF0000; // EBX[23:16] indicate number of logical processors per package
          register_t core = cpuid(1);
          
	if (!HTSupported()) return 1; 
	if( !core.retval) return 1;

	return (byte)((core.ebx & NUM_LOGICAL_BITS) >> 16);
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

void Plat_InitCPU( void )
{
	cpuinfo_t cpu = GetCPUInformation();
	char szFeatureString[256];
	
	// Compute Frequency in Mhz: 
	char* szFrequencyDenomination = "Mhz";
	double fFrequency = cpu.m_speed / 1000000.0;

	//copy shared info
          GI.cpufreq = (float)fFrequency;
          GI.cpunum = cpu.m_usNumLogicCore;
          
	// Adjust to Ghz if nessecary:
	if( fFrequency > 1000.0 )
	{
		fFrequency /= 1000.0;
		szFrequencyDenomination = "Ghz";
	}

	strcpy(szFeatureString, "" ); 
	if( cpu.m_bMMX ) strcat(szFeatureString, "MMX " );
	if( cpu.m_b3DNow ) strcat(szFeatureString, "3DNow " );
	if( cpu.m_bSSE ) strcat(szFeatureString, "SSE " );
	if( cpu.m_bSSE2 ) strcat(szFeatureString, "SSE2 " );
	if( cpu.m_bRDTSC ) strcat(szFeatureString, "RDTSC " );
	if( cpu.m_bCMOV ) strcat(szFeatureString, "CMOV " );
	if( cpu.m_bFCMOV ) strcat(szFeatureString, "FCMOV " );

	// Remove the trailing space.  There will always be one.
	szFeatureString[strlen(szFeatureString)-1] = '\0';

	// Dump CPU information:
	if( cpu.m_usNumLogicCore == 1 ) MsgDev( D_INFO, "CPU: %s [1 core]. Frequency: %.01f %s\n", cpu.m_szCPUID, fFrequency, szFrequencyDenomination );
	else
	{
		char buffer[256] = "";
		if( cpu.m_usNumPhysCore != cpu.m_usNumLogicCore )
			sprintf(buffer, " (%i physical)", (int) cpu.m_usNumPhysCore );
		MsgDev(D_INFO, "CPU: %s [%i core's %s]. Frequency: %.01f %s\n ", cpu.m_szCPUID, (int)cpu.m_usNumLogicCore, buffer, fFrequency, szFrequencyDenomination );
	}
	MsgDev(D_INFO, "CPU Features: %s\n", szFeatureString );
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
	if (enter) 
	{
		MsgWarn("ThreadLock: recursive call\n");
		return;
	}
	EnterCriticalSection (&crit);
	enter = 1;
}

void ThreadUnlock (void)
{
	if (!threaded) return;
	if (!enter) 
	{
		MsgWarn("ThreadUnlock: must call ThreadLock first\n");
		return;
	}
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
		work = GetThreadWork();
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

		for (i = 0; i < numthreads; i++)
			WaitForSingleObject (threadhandle[i], INFINITE);
	}
	DeleteCriticalSection (&crit);

	threaded = false;
	end = Plat_DoubleTime();
	if (pacifier) Msg(" Done [%.2f sec]\n", end - start);
}

/*
================
Plat_DoubleTime
================
*/
double Plat_DoubleTime (void)
{
	static int first = true;
	static bool nohardware_timer = false;
	static double oldtime = 0.0, curtime = 0.0;
	double newtime;
	
	// LordHavoc: note to people modifying this code, 
	// DWORD is specifically defined as an unsigned 32bit number, 
	// therefore the 65536.0 * 65536.0 is fine.
	if (GI.cpunum > 1 || nohardware_timer)
	{
		static int firsttimegettime = true;
		// timeGetTime
		// platform:
		// Windows 95/98/ME/NT/2000/XP
		// features:
		// reasonable accuracy (millisecond)
		// issues:
		// wraps around every 47 days or so (but this is non-fatal to us, 
		// odd times are rejected, only causes a one frame stutter)

		// make sure the timer is high precision, otherwise different versions of
		// windows have varying accuracy
		if (firsttimegettime)
		{
			timeBeginPeriod (1);
			firsttimegettime = false;
		}

		newtime = (double) timeGetTime () / 1000.0;
	}
	else
	{
		// QueryPerformanceCounter
		// platform:
		// Windows 95/98/ME/NT/2000/XP
		// features:
		// very accurate (CPU cycles)
		// known issues:
		// does not necessarily match realtime too well
		// (tends to get faster and faster in win98)
		// wraps around occasionally on some platforms
		// (depends on CPU speed and probably other unknown factors)
		double timescale;
		LARGE_INTEGER PerformanceFreq;
		LARGE_INTEGER PerformanceCount;

		if (!QueryPerformanceFrequency (&PerformanceFreq))
		{
			Msg("No hardware timer available\n");
			// fall back to timeGetTime
			nohardware_timer = true;
			return Plat_DoubleTime();
		}
		QueryPerformanceCounter (&PerformanceCount);

		timescale = 1.0 / ((double) PerformanceFreq.LowPart + (double) PerformanceFreq.HighPart * 65536.0 * 65536.0);
		newtime = ((double) PerformanceCount.LowPart + (double) PerformanceCount.HighPart * 65536.0 * 65536.0) * timescale;
	}

	if (first)
	{
		first = false;
		oldtime = newtime;
	}

	if (newtime < oldtime)
	{
		// warn if it's significant
		if (newtime - oldtime < -0.01)
			Msg("Plat_DoubleTime: time stepped backwards (went from %f to %f, difference %f)\n", oldtime, newtime, newtime - oldtime);
	}
	else curtime += newtime - oldtime;
	oldtime = newtime;

	return curtime;
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
	cp.PrepareBSP = PrepareBSPModel;
	cp.BSP = CompileBSPModel;
	cp.PrepareDAT = PrepareDATProgs;
	cp.DAT = CompileDATProgs;

	return cp;
}