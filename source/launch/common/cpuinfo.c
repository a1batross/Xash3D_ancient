//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         cpuinfo.c - get cpu information
//=======================================================================

#include "launch.h"

// Processor Information:
typedef struct cpuinfo_s
{
	bool	m_bRDTSC	: 1;	// Is RDTSC supported?
	bool	m_bCMOV	: 1;	// Is CMOV supported?
	bool	m_bFCMOV	: 1;	// Is FCMOV supported?
	bool	m_bSSE	: 1;	// Is SSE supported?
	bool	m_bSSE2	: 1;	// Is SSE2 Supported?
	bool	m_b3DNow	: 1;	// Is 3DNow! Supported?
	bool	m_bMMX	: 1;	// Is MMX supported?
	bool	m_bHT	: 1;	// Is HyperThreading supported?

	byte	m_usNumLogicCore;	// Number op logical processors.
	byte	m_usNumPhysCore;	// Number of physical processors

	int64	m_speed;		// In cycles per second.
	int	m_size;		// structure size
	char*	m_szCPUID;	// Processor vendor Identification.
} cpuinfo_t;

typedef struct register_s
{
	dword	eax;
	dword	ebx;
	dword	ecx;
	dword	edx;
	bool	retval;
} register_t;

static register_t cpuid( uint function )
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

bool CheckMMXTechnology( void )
{
	register_t mmx = cpuid(1);

	if( !mmx.retval ) return false;
	return ( mmx.edx & 0x800000 ) != 0;
}

bool CheckSSETechnology( void )
{
	register_t sse = cpuid(1);
	
	if( !sse.retval ) return false;
	return ( sse.edx & 0x2000000L ) != 0;
}

bool CheckSSE2Technology( void )
{
	register_t sse2 = cpuid(1);

	if( !sse2.retval ) return false;
	return ( sse2.edx & 0x04000000 ) != 0;
}

bool Check3DNowTechnology( void )
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

bool CheckCMOVTechnology( void )
{
	register_t cmov = cpuid(1);

	if( !cmov.retval ) return false;
	return ( cmov.edx & (1<<15) ) != 0;
}

bool CheckFCMOVTechnology( void )
{
	register_t fcmov = cpuid(1);

	if( !fcmov.retval ) return false;
	return ( fcmov.edx & (1<<16) ) != 0;
}

bool CheckRDTSCTechnology( void )
{
	register_t rdtsc = cpuid(1);

	if( !rdtsc.retval ) return false;
	return rdtsc.edx & 0x10 != 0;
}

/*
================
Return the Processor's vendor identification string, or "Generic_x86" if it doesn't exist on this CPU
================
*/
const char* GetProcessorVendorId( void )
{
	static char VendorID[13];
	register_t vendor = cpuid(0);
	
	memset( VendorID, 0, sizeof(VendorID) );
	if( !vendor.retval )
	{
		com_strcpy( VendorID, "Generic_x86" ); 
	}
	else
	{
		Mem_Copy( VendorID+0, &(vendor.ebx), sizeof( vendor.ebx ) );
		Mem_Copy( VendorID+4, &(vendor.edx), sizeof( vendor.edx ) );
		Mem_Copy( VendorID+8, &(vendor.ecx), sizeof( vendor.ecx ) );
	}
	return VendorID;
}

/*
================
Returns non-zero if Hyper-Threading Technology is supported on the processors and zero if not.
This does not mean that Hyper-Threading Technology is necessarily enabled.
================
*/
static bool HTSupported( void )
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

/*
================

Returns the number of logical processors per physical processors.
================
*/
static byte LogicalProcessorsPerPackage( void )
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
	dword* pSample = (dword *)&m_time;
	__asm
	{
		mov	ecx, pSample
		rdtsc
		mov	[ecx],     eax
		mov	[ecx+4],   edx
	}
	return m_time;
}

/*
================
Measure the processor clock speed by sampling the cycle count, waiting
for some fraction of a second, then measuring the elapsed number of cycles.
================
*/
static int64 CalculateClockSpeed( void )
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

/*
================
GetCPUInfo
================
*/
cpuinfo_t GetCPUInfo( void )
{
	static cpuinfo_t	pi;
	SYSTEM_INFO	si;
	

	if( pi.m_size == sizeof(pi) ) return pi;// Has the structure already been initialized and filled out?
	memset(&pi, 0, sizeof(pi));		// Redundant, but just in case the user somehow messes with the size.
	pi.m_size = sizeof(pi);		// Fill out the structure, and return it:
	pi.m_speed = CalculateClockSpeed();	// Grab the processor frequency:
	pi.m_usNumLogicCore = LogicalProcessorsPerPackage();// Get the logical and physical processor counts:

	memset( &si, 0, sizeof(si) );
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

void Sys_InitCPU( void )
{
	cpuinfo_t	cpu = GetCPUInfo();
	char	szFeatureString[256];
	
	// Compute Frequency in Mhz: 
	char* szFrequencyDenomination = "Mhz";
	double fFrequency = cpu.m_speed / 1000000.0;

	// copy shared info
	GI.tickcount = cpu.m_speed; // used for profiling
	GI.cpufreq = (float)fFrequency;
          GI.cpunum = cpu.m_usNumLogicCore;
          GI.rdtsc = cpu.m_bRDTSC;
          
	// Adjust to Ghz if nessecary:
	if( fFrequency > 1000.0 )
	{
		fFrequency /= 1000.0;
		szFrequencyDenomination = "Ghz";
	}

	com_strcpy(szFeatureString, "" ); 
	if( cpu.m_bMMX ) com_strcat(szFeatureString, "MMX " );
	if( cpu.m_b3DNow ) com_strcat(szFeatureString, "3DNow " );
	if( cpu.m_bSSE ) com_strcat(szFeatureString, "SSE " );
	if( cpu.m_bSSE2 ) com_strcat(szFeatureString, "SSE2 " );
	if( cpu.m_bRDTSC ) com_strcat(szFeatureString, "RDTSC " );
	if( cpu.m_bCMOV ) com_strcat(szFeatureString, "CMOV " );
	if( cpu.m_bFCMOV ) com_strcat(szFeatureString, "FCMOV " );

	// remove the trailing space.  There will always be one.
	szFeatureString[com_strlen(szFeatureString)-1] = '\0';

	// dump CPU information:
	if( cpu.m_usNumLogicCore == 1 ) MsgDev( D_INFO, "CPU: %s [1 core]. Frequency: %.01f %s\n", cpu.m_szCPUID, fFrequency, szFrequencyDenomination );
	else
	{
		char buffer[256] = "";
		if( cpu.m_usNumPhysCore != cpu.m_usNumLogicCore )
			com_sprintf(buffer, " (%i physical)", (int) cpu.m_usNumPhysCore );
		MsgDev(D_INFO, "CPU: %s [%i core's %s]. Frequency: %.01f %s\n ", cpu.m_szCPUID, (int)cpu.m_usNumLogicCore, buffer, fFrequency, szFrequencyDenomination );
	}
	MsgDev(D_NOTE, "CPU Features: %s\n", szFeatureString );
}