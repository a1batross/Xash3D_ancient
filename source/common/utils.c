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