//=======================================================================
//			Copyright XashXT Group 2007 ©
//			r_utils.c - render utils
//=======================================================================

#include "gl_local.h"

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

/*
====================
Image Decompress

read colors from dxt image
====================
*/
void R_DXTReadColors(const byte* data, color32* out)
{
	byte r0, g0, b0, r1, g1, b1;

	b0 = data[0] & 0x1F;
	g0 = ((data[0] & 0xE0) >> 5) | ((data[1] & 0x7) << 3);
	r0 = (data[1] & 0xF8) >> 3;

	b1 = data[2] & 0x1F;
	g1 = ((data[2] & 0xE0) >> 5) | ((data[3] & 0x7) << 3);
	r1 = (data[3] & 0xF8) >> 3;

	out[0].r = r0 << 3;
	out[0].g = g0 << 2;
	out[0].b = b0 << 3;

	out[1].r = r1 << 3;
	out[1].g = g1 << 2;
	out[1].b = b1 << 3;
}

/*
====================
Image Decompress

read one color from dxt image
====================
*/
void R_DXTReadColor(word data, color32* out)
{
	byte r, g, b;

	b = data & 0x1f;
	g = (data & 0x7E0) >>5;
	r = (data & 0xF800)>>11;

	out->r = r << 3;
	out->g = g << 2;
	out->b = b << 3;
}

/*
====================
R_GetBitsFromMask

====================
*/
void R_GetBitsFromMask(uint Mask, uint *ShiftLeft, uint *ShiftRight)
{
	uint Temp, i;

	if (Mask == 0)
	{
		*ShiftLeft = *ShiftRight = 0;
		return;
	}

	Temp = Mask;
	for (i = 0; i < 32; i++, Temp >>= 1)
	{
		if (Temp & 1) break;
	}
	*ShiftRight = i;

	// Temp is preserved, so use it again:
	for (i = 0; i < 8; i++, Temp >>= 1)
	{
		if (!(Temp & 1)) break;
	}
	*ShiftLeft = 8 - i;
	return;
}