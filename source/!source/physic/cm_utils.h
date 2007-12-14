//=======================================================================
//			Copyright XashXT Group 2007 ©
//			    cm_utils.h - misc utils
//=======================================================================
#ifndef CM_UTILS_H
#define CM_UTILS_H

_inline void CM_ConvertPositionToMeters( vec3_t out, vec3_t in )
{
	out[0] = LittleFloat(INCH2METER(in[0]));
	out[1] = LittleFloat(INCH2METER(in[2]));
	out[2] = LittleFloat(INCH2METER(in[1]));
}

_inline void CM_ConvertDimensionToMeters( vec3_t out, vec3_t in )
{
	out[0] = LittleFloat(INCH2METER(in[0]));
	out[1] = LittleFloat(INCH2METER(in[1]));
	out[2] = LittleFloat(INCH2METER(in[2]));
}

void CM_LoadBSP( const void *buffer );
void CM_FreeBSP( void );

void CM_LoadWorld( const void *buffer );
void CM_FreeWorld( void );

void CM_MakeCollisionTree( void );
void CM_LoadCollisionTree( void );
void CM_SaveCollisionTree( file_t *f, cmsave_t callback );

#endif//CM_UTILS_H