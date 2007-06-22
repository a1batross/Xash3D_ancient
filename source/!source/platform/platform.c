//=======================================================================
//			Copyright XashXT Group 2007 ©
//			platform.c - game platform dll
//=======================================================================

#include "platform.h"
#include "baseutils.h"
#include "bsplib.h"
#include "qcclib.h"

static double start, end;
static int app_name;
char parm[64];

gameinfo_t Plat_GameInfo( void )
{
	return GI;
}

platform_api_t DLLEXPORT CreateAPI ( system_api_t sysapi )
{
	platform_api_t pi;

	gSysFuncs = sysapi;

	//generic functions
	pi.api_version = PLATFORM_VERSION;
	pi.plat_init = InitPlatform;
	pi.plat_main = PlatformFrame;
	pi.plat_free = ClosePlatform;

	//memory manager
	pi.MS_Alloc = _Mem_Alloc;
	pi.MS_Free = _Mem_Free;
	pi.MS_AllocPool = _Mem_AllocPool;
	pi.MS_FreePool = _Mem_FreePool;
	pi.MS_EmptyPool = _Mem_EmptyPool;	

	//timer
	pi.DoubleTime = Plat_DoubleTime;
	pi.GameInfo = Plat_GameInfo;

	//filesystem
	pi.FS_Search = FS_Search;
	pi.FS_LoadFile = FS_LoadFile;
	pi.FS_LoadImage = FS_LoadImage;
	pi.FS_LoadImageData = FS_LoadImageData;
	pi.FS_WriteFile = FS_WriteFile;
	pi.FS_FileExists = FS_FileExists;
	pi.FS_StripExtension = FS_StripExtension;
	pi.FS_DefaultExtension = FS_DefaultExtension;
	pi.FS_FileBase = FS_FileBase;
	pi.FS_Open = FS_Open;
	pi.FS_Read = FS_Read;
	pi.FS_Write = FS_Write;
	pi.FS_Getc = FS_Getc;
	pi.FS_UnGetc = FS_UnGetc;
	pi.FS_Printf = FS_Printf;
	pi.FS_Close = FS_Close;
	pi.FS_Seek = FS_Seek;
	pi.FS_Tell = FS_Tell;
	
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

	strcpy( szFeatureString, cpu.m_szCPUID );
	strcat( szFeatureString, " " );
	
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
	if( cpu.m_usNumLogicCore == 1 ) Msg( "1 CPU, " );
	else
	{
		char buffer[256] = "";
		if( cpu.m_usNumPhysCore != cpu.m_usNumLogicCore )
			sprintf(buffer, " (%i physical)", (int) cpu.m_usNumPhysCore );
		Msg( "%i CPUs%s, ",  (int)cpu.m_usNumLogicCore, buffer );
	}
	Msg("Frequency: %.01f %s\nCPU Features: %s\n", fFrequency, szFrequencyDenomination, szFeatureString );
}

/*
================
Sys_DoubleTime
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

void InitPlatform ( char *funcname, int argc, char **argv )
{
	InitMemory();
	Plat_InitCPU();
	ThreadSetDefault();
	FS_InitCmdLine( argc, argv );
	FS_Init();

	if(!strcmp(funcname, "bsplib"))
	{
		app_name = BSPLIB;

		if(!FS_GetParmFromCmdLine("-game", gs_basedir ))
			strcpy(gs_basedir, "xash" );//default dir

		if(!FS_GetParmFromCmdLine("+map", gs_mapname ))
			strcpy(gs_mapname, "newmap" );//default map

		if(FS_GetParmFromCmdLine("-bounce", parm ))
			numbounce = atoi(parm);

		if(FS_GetParmFromCmdLine("-ambient", parm ))
			ambient = atof(parm) * 128;
          
		if(FS_CheckParm("-vis")) onlyvis = true;
		if(FS_CheckParm("-rad")) onlyrad = true;
		if(FS_CheckParm("-full")) full_compile = true;
		if(FS_CheckParm("-onlyents")) onlyents = true;

		FS_LoadGameInfo("gameinfo.txt");

		start = Plat_DoubleTime();
		LoadShaderInfo();
	}
	else if(!strcmp(funcname, "sprite"))
	{
		app_name = SPRITE;
		FS_InitPath(".");
		start = Plat_DoubleTime();
	}
	else if(!strcmp(funcname, "studio"))
	{
		app_name = STUDIO;
		FS_InitPath(".");
		start = Plat_DoubleTime();
	}
	else if(!strcmp(funcname, "qcclib"))
	{
		app_name = QCCLIB;
		FS_InitPath(".");
		start = Plat_DoubleTime();
	}
	else if(!strcmp(funcname, "host_editor"))
	{
		app_name = HOST_EDITOR;
		FS_InitEditor();
	}
	else//other reasons
	{
		app_name = DEFAULT;
		FS_LoadGameInfo("gameinfo.txt");
	}

	FS_Path();//debug
}

void PlatformFrame( void )
{
	switch(app_name)
	{
	case BSPLIB:
	          MakeBSP();
		end = Plat_DoubleTime();
		Msg ("%5.1f seconds elapsed\n", end-start);
		break;
	case SPRITE:
		MakeSprite();          
		end = Plat_DoubleTime();
		Msg ("%5.1f seconds elapsed\n", end-start);
		break;
	case STUDIO:
		MakeModel();
		end = Plat_DoubleTime();
		Msg ("%5.1f seconds elapsed\n", end-start);
		break;
	case QCCLIB:
		MakeDAT();
		end = Plat_DoubleTime();
		Msg ("%5.1f seconds elapsed\n", end-start);
		break;
	case DEFAULT:
		break;
	}
}

void ClosePlatform ( void )
{
	FS_Shutdown();
	FreeMemory();
}

//unused old stuffff
void FS_FreeFile( void *buffer )
{
	Free( buffer );
}