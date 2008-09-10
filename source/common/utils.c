//=======================================================================
//			Copyright XashXT Group 2007 ©
//			   utils.c - platform utils
//=======================================================================

#include "platform.h"
#include "byteorder.h"
#include "utils.h"
#include "bsplib.h"
#include "mdllib.h"


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
	else MsgDev( D_ERROR, "--- Profiler not supported ---\n");
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
	else MsgDev( D_ERROR, "--- Profiler not supported ---\n");
}

void Profile_Time( void )
{
	if (com.GameInfo->rdtsc)
	{
		Msg("Profiling results:\n");
		Msg("----- total ticks: %I64d -----\n", __g_ProfilerTotalTicks);
		Msg("----- total secs %f ----- \n", __g_ProfilerTotalMsec  );
	}
	else MsgDev( D_ERROR, "--- Profiler not supported ---\n");
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
	else if (Com_MatchToken("$resample") && scripttype != QC_SPRITEGEN )
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
	else if(Com_MatchToken( "$wadname" ) && scripttype != QC_WADLIB )
	{
		Msg("%s probably wadlib qc.script, skipping...\n", gs_filename );
		return false;
	}
	else if(Com_MatchToken( "$gfxpic" ) && scripttype != QC_WADLIB )
	{
		Msg("%s probably wadlib qc.script, skipping...\n", gs_filename );
		return false;
	}
	return true;
};

void Com_CheckToken( const char *match )
{
	Com_GetToken( true );

	if(!Com_MatchToken( match ))
	{
		Sys_Break( "Com_CheckToken: \"%s\" not found\n", match );
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