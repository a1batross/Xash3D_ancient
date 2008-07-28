//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         mathlib.h - base math functions
//=======================================================================
#ifndef BASEMATRIX_H
#define BASEMATRIX_H

#include <math.h>
//#define OPENGL_STYLE

static const matrix4x4 identitymatrix =
{
{ 1, 0, 0, 0 },	// PITCH
{ 0, 1, 0, 0 },	// YAW
{ 0, 0, 1, 0 },	// ROLL
{ 0, 0, 0, 1 },	// ORIGIN
};

_inline void Matrix4x4_Transform( const matrix4x4 in, const float v[3], float out[3] )
{
#ifdef OPENGL_STYLE
	out[0] = v[0] * in[0][0] + v[1] * in[1][0] + v[2] * in[2][0] + in[3][0];
	out[1] = v[0] * in[0][1] + v[1] * in[1][1] + v[2] * in[2][1] + in[3][1];
	out[2] = v[0] * in[0][2] + v[1] * in[1][2] + v[2] * in[2][2] + in[3][2];
#else
	out[0] = v[0] * in[0][0] + v[1] * in[0][1] + v[2] * in[0][2] + in[0][3];
	out[1] = v[0] * in[1][0] + v[1] * in[1][1] + v[2] * in[1][2] + in[1][3];
	out[2] = v[0] * in[2][0] + v[1] * in[2][1] + v[2] * in[2][2] + in[2][3];
#endif
}

_inline void Matrix4x4_Transform3x3( const matrix4x4 in, const float v[3], float out[3] )
{
#ifdef OPENGL_STYLE
	out[0] = v[0] * in[0][0] + v[1] * in[1][0] + v[2] * in[2][0];
	out[1] = v[0] * in[0][1] + v[1] * in[1][1] + v[2] * in[2][1];
	out[2] = v[0] * in[0][2] + v[1] * in[1][2] + v[2] * in[2][2];
#else
	out[0] = v[0] * in[0][0] + v[1] * in[0][1] + v[2] * in[0][2];
	out[1] = v[0] * in[1][0] + v[1] * in[1][1] + v[2] * in[1][2];
	out[2] = v[0] * in[2][0] + v[1] * in[2][1] + v[2] * in[2][2];
#endif
}

#endif//BASEMATRIX_H