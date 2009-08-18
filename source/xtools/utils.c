//=======================================================================
//			Copyright XashXT Group 2007 ©
//			   utils.c - platform utils
//=======================================================================

#include "xtools.h"
#include "byteorder.h"
#include "utils.h"
#include "mdllib.h"

string	gs_basedir;	// initial dir before loading gameinfo.txt (used for compilers too)
string	gs_filename;	// used for compilers only

float ColorNormalize( const vec3_t in, vec3_t out )
{
	float	max, scale;

	max = in[0];
	if( in[1] > max ) max = in[1];
	if( in[2] > max ) max = in[2];

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
NormalToLatLong

We use two byte encoded normals in some space critical applications.
Lat = 0 at (1,0,0) to 360 (-1,0,0), encoded in 8-bit sine table format
Lng = 0 at (0,0,1) to 180 (0,0,-1), encoded in 8-bit sine table format
================
*/
void NormalToLatLong( const vec3_t normal, byte bytes[2] )
{
	// check for singularities
	if( normal[0] == 0 && normal[1] == 0 )
	{
		if( normal[2] > 0 )
		{
			bytes[0] = 0;
			bytes[1] = 0;	// lat = 0, long = 0
		}
		else
		{
			bytes[0] = 128;
			bytes[1] = 0;	// lat = 0, long = 128
		}
	}
	else
	{
		int	a, b;

		a = (int)( RAD2DEG( atan2( normal[1], normal[0] )) * (255.0f / 360.0f ));
		a &= 0xff;

		b = (int)( RAD2DEG( acos( normal[2] )) * ( 255.0f / 360.0f ));
		b &= 0xff;

		bytes[0] = b;	// longitude
		bytes[1] = a;	// lattitude
	}
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
