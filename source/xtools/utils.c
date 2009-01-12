//=======================================================================
//			Copyright XashXT Group 2007 ©
//			   utils.c - platform utils
//=======================================================================

#include "xtools.h"
#include "byteorder.h"
#include "utils.h"
#include "bsplib.h"
#include "mdllib.h"

string	gs_basedir;	// initial dir before loading gameinfo.txt (used for compilers too)
string	gs_filename;	// used for compilers only

float ColorNormalize( const vec3_t in, vec3_t out )
{
	float	max, scale;

#if 0
	max = VectorMax( in );
#else
	max = in[0];
	if( in[1] > max ) max = in[1];
	if( in[2] > max ) max = in[2];
#endif
	if( max == 0 )
	{
		out[0] = out[1] = out[2] = 1.0f;
		return 0;
	}

	scale = 1.0f / max;
	VectorScale( in, scale, out );
	return max;
}

/*
================
Com_ValidScript

validate qc-script for unexcpected keywords
================
*/
bool Com_ValidScript( const char *token, qctype_t scripttype )
{ 
	if( !com.stricmp( token, "$spritename") && scripttype != QC_SPRITEGEN )
	{
		Msg( "%s probably spritegen qc.script, skipping...\n", gs_filename );
		return false;
	}
	else if( !com.stricmp( token, "$resample" ) && scripttype != QC_SPRITEGEN )
	{
		Msg( "%s probably spritegen qc.script, skipping...\n", gs_filename );
		return false;
	}
	else if( !com.stricmp( token, "$modelname" ) && scripttype != QC_STUDIOMDL )
	{
		Msg( "%s probably studio qc.script, skipping...\n", gs_filename );
		return false;
	}	
	else if( !com.stricmp( token, "$body" ) && scripttype != QC_STUDIOMDL )
	{
		Msg( "%s probably studio qc.script, skipping...\n", gs_filename );
		return false;
	}
	else if( !com.stricmp( token, "$wadname" ) && scripttype != QC_WADLIB )
	{
		Msg( "%s probably wadlib qc.script, skipping...\n", gs_filename );
		return false;
	}
	else if( !com.stricmp( token, "$mipmap" ) && scripttype != QC_WADLIB )
	{
		Msg("%s probably wadlib qc.script, skipping...\n", gs_filename );
		return false;
	}
	return true;
};
