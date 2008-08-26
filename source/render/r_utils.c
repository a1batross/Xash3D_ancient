//=======================================================================
//			Copyright XashXT Group 2007 ©
//			r_utils.c - render utils
//=======================================================================

#include "r_local.h"
#include "mathlib.h"

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