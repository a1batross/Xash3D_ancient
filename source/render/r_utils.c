//=======================================================================
//			Copyright XashXT Group 2007 �
//			r_utils.c - render utils
//=======================================================================

#include "r_local.h"
#include "mathlib.h"

vec3_t	axisDefault[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

/*
 =================
 AxisCopy
 =================
*/
void AxisCopy( const vec3_t in[3], vec3_t out[3] )
{
	out[0][0] = in[0][0];
	out[0][1] = in[0][1];
	out[0][2] = in[0][2];
	out[1][0] = in[1][0];
	out[1][1] = in[1][1];
	out[1][2] = in[1][2];
	out[2][0] = in[2][0];
	out[2][1] = in[2][1];
	out[2][2] = in[2][2];
}

/*
 =================
 AxisClear
 =================
*/
void AxisClear( vec3_t axis[3] )
{
	axis[0][0] = 1;
	axis[0][1] = 0;
	axis[0][2] = 0;
	axis[1][0] = 0;
	axis[1][1] = 1;
	axis[1][2] = 0;
	axis[2][0] = 0;
	axis[2][1] = 0;
	axis[2][2] = 1;
}

/*
=================
AnglesToAxis
=================
*/
void AnglesToAxis ( const vec3_t angles )
{
	static float	sp, sy, sr, cp, cy, cr;
	float		angle;

	angle = DEG2RAD(angles[PITCH]);
	sp = sin(angle);
	cp = cos(angle);
	angle = DEG2RAD(angles[YAW]);
	sy = sin(angle);
	cy = cos(angle);
	angle = DEG2RAD(angles[ROLL]);
	sr = sin(angle);
	cr = cos(angle);

	r_forward[0] = cp*cy;
	r_forward[1] = cp*sy;
	r_forward[2] = -sp;
	r_right[0] = sr*sp*cy+cr*-sy;
	r_right[1] = sr*sp*sy+cr*cy;
	r_right[2] = sr*cp;
	r_up[0] = cr*sp*cy+-sr*-sy;
	r_up[1] = cr*sp*sy+-sr*cy;
	r_up[2] = cr*cp;
}

/*
 =================
 AnglesToAxis
 =================
*/
void AnglesToAxisPrivate (const vec3_t angles, vec3_t axis[3])
{

	static float	sp, sy, sr, cp, cy, cr;
	float			angle;

	angle = DEG2RAD(angles[PITCH]);
	sp = sin(angle);
	cp = cos(angle);
	angle = DEG2RAD(angles[YAW]);
	sy = sin(angle);
	cy = cos(angle);
	angle = DEG2RAD(angles[ROLL]);
	sr = sin(angle);
	cr = cos(angle);

	axis[0][0] = cp*cy;
	axis[0][1] = cp*sy;
	axis[0][2] = -sp;
	axis[1][0] = sr*sp*cy+cr*-sy;
	axis[1][1] = sr*sp*sy+cr*cy;
	axis[1][2] = sr*cp;
	axis[2][0] = cr*sp*cy+-sr*-sy;
	axis[2][1] = cr*sp*sy+-sr*cy;
	axis[2][2] = cr*cp;
}

/*
 =================
 AxisCompare
 =================
*/
bool AxisCompare( const vec3_t axis1[3], const vec3_t axis2[3] )
{
	if (axis1[0][0] != axis2[0][0] || axis1[0][1] != axis2[0][1] || axis1[0][2] != axis2[0][2])
		return false;
	if (axis1[1][0] != axis2[1][0] || axis1[1][1] != axis2[1][1] || axis1[1][2] != axis2[1][2])
		return false;
	if (axis1[2][0] != axis2[2][0] || axis1[2][1] != axis2[2][1] || axis1[2][2] != axis2[2][2])
		return false;
	return true;
}

/*
 =================
 VectorRotate
 =================
*/
void VectorRotate ( const vec3_t v, const vec3_t matrix[3], vec3_t out )
{
	out[0] = v[0]*matrix[0][0] + v[1]*matrix[0][1] + v[2]*matrix[0][2];
	out[1] = v[0]*matrix[1][0] + v[1]*matrix[1][1] + v[2]*matrix[1][2];
	out[2] = v[0]*matrix[2][0] + v[1]*matrix[2][1] + v[2]*matrix[2][2];
}

/*
 =================
 MatrixGL_MultiplyFast
 =================
*/
void MatrixGL_MultiplyFast (const gl_matrix m1, const gl_matrix m2, gl_matrix out)
{

	out[ 0] = m1[ 0] * m2[ 0] + m1[ 4] * m2[ 1] + m1[ 8] * m2[ 2];
	out[ 1] = m1[ 1] * m2[ 0] + m1[ 5] * m2[ 1] + m1[ 9] * m2[ 2];
	out[ 2] = m1[ 2] * m2[ 0] + m1[ 6] * m2[ 1] + m1[10] * m2[ 2];
	out[ 3] = 0.0;
	out[ 4] = m1[ 0] * m2[ 4] + m1[ 4] * m2[ 5] + m1[ 8] * m2[ 6];
	out[ 5] = m1[ 1] * m2[ 4] + m1[ 5] * m2[ 5] + m1[ 9] * m2[ 6];
	out[ 6] = m1[ 2] * m2[ 4] + m1[ 6] * m2[ 5] + m1[10] * m2[ 6];
	out[ 7] = 0.0;
	out[ 8] = m1[ 0] * m2[ 8] + m1[ 4] * m2[ 9] + m1[ 8] * m2[10];
	out[ 9] = m1[ 1] * m2[ 8] + m1[ 5] * m2[ 9] + m1[ 9] * m2[10];
	out[10] = m1[ 2] * m2[ 8] + m1[ 6] * m2[ 9] + m1[10] * m2[10];
	out[11] = 0.0;
	out[12] = m1[ 0] * m2[12] + m1[ 4] * m2[13] + m1[ 8] * m2[14] + m1[12];
	out[13] = m1[ 1] * m2[12] + m1[ 5] * m2[13] + m1[ 9] * m2[14] + m1[13];
	out[14] = m1[ 2] * m2[12] + m1[ 6] * m2[13] + m1[10] * m2[14] + m1[14];
	out[15] = 1.0;
}

/*
====================
RotatePointAroundVector
====================
*/
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees )
{
	float	t0, t1;
	float	angle, c, s;
	vec3_t	vr, vu, vf;

	angle = DEG2RAD( degrees );
	c = cos( angle );
	s = sin( angle );
	VectorCopy( dir, vf );
	VectorVectors( vf, vr, vu );

	t0 = vr[0] *  c + vu[0] * -s;
	t1 = vr[0] *  s + vu[0] *  c;
	dst[0] = (t0 * vr[0] + t1 * vu[0] + vf[0] * vf[0]) * point[0]
	       + (t0 * vr[1] + t1 * vu[1] + vf[0] * vf[1]) * point[1]
	       + (t0 * vr[2] + t1 * vu[2] + vf[0] * vf[2]) * point[2];

	t0 = vr[1] *  c + vu[1] * -s;
	t1 = vr[1] *  s + vu[1] *  c;
	dst[1] = (t0 * vr[0] + t1 * vu[0] + vf[1] * vf[0]) * point[0]
	       + (t0 * vr[1] + t1 * vu[1] + vf[1] * vf[1]) * point[1]
	       + (t0 * vr[2] + t1 * vu[2] + vf[1] * vf[2]) * point[2];

	t0 = vr[2] *  c + vu[2] * -s;
	t1 = vr[2] *  s + vu[2] *  c;
	dst[2] = (t0 * vr[0] + t1 * vu[0] + vf[2] * vf[0]) * point[0]
	       + (t0 * vr[1] + t1 * vu[1] + vf[2] * vf[1]) * point[1]
	       + (t0 * vr[2] + t1 * vu[2] + vf[2] * vf[2]) * point[2];
}

/*
====================
Image Decompress

ShortToFloat
====================
*/
uint ShortToFloat( word y )
{
	int s = (y >> 15) & 0x00000001;
	int e = (y >> 10) & 0x0000001f;
	int m =  y & 0x000003ff;

	// float: 1 sign bit, 8 exponent bits, 23 mantissa bits
	// half: 1 sign bit, 5 exponent bits, 10 mantissa bits

	if (e == 0)
	{
		if (m == 0) return s << 31; // Plus or minus zero
		else // Denormalized number -- renormalize it
		{
			while (!(m & 0x00000400))
			{
				m <<= 1;
				e -=  1;
			}
			e += 1;
			m &= ~0x00000400;
		}
	}
	else if (e == 31)
	{
		if (m == 0) return (s << 31) | 0x7f800000; // Positive or negative infinity
		else return (s << 31) | 0x7f800000 | (m << 13); // Nan -- preserve sign and significand bits
	}

	// Normalized number
	e = e + (127 - 15);
	m = m << 13;
	return (s << 31) | (e << 23) | m; // Assemble s, e and m.
}