//=======================================================================
//			Copyright XashXT Group 2007 ©
//			r_utils.c - render utils
//=======================================================================

#include "r_local.h"
#include "mathlib.h"
#include "matrixlib.h"

/*
=============
R_TransformWorldToScreen
=============
*/
void R_TransformWorldToScreen( vec3_t in, vec3_t out )
{
	vec4_t	temp, temp2;

	Vector4Set( temp, in[0], in[1], in[2], 1.0f );
	Matrix4x4_Transform( Ref.worldProjectionMatrix, temp, temp2 );

	if( !temp2[3] ) return;

	out[0] = (temp2[0] / temp2[3] + 1.0f) * 0.5f * Ref.refdef.rect.width;
	out[1] = (temp2[1] / temp2[3] + 1.0f) * 0.5f * Ref.refdef.rect.height;
	out[2] = (temp2[2] / temp2[3] + 1.0f) * 0.5f;
}

/*
=============
R_TransformVectorToScreen
=============
*/
void R_TransformVectorToScreen( const refdef_t *rd, const vec3_t in, vec2_t out )
{
	matrix4x4	p, m;
	vec4_t	temp, temp2;

	if( !rd || !in || !out ) return;

	Vector4Set( temp, in[0], in[1], in[2], 1.0f );

	R_SetupProjectionMatrix( rd, p );
	R_SetupModelViewMatrix( rd, m );

	Matrix4x4_Transform( m, temp, temp2 );
	Matrix4x4_Transform( p, temp2, temp );

	if( !temp[3] ) return;

	out[0] = rd->rect.x + (temp[0] / temp[3] + 1.0f) * rd->rect.width * 0.5f;
	out[1] = rd->rect.y + (temp[1] / temp[3] + 1.0f) * rd->rect.height * 0.5f;
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

/*
===============
Patch_FlatnessTest

===============
*/
static int Patch_FlatnessTest( float maxflat2, const float *point0, const float *point1, const float *point2 )
{
	float	d;
	int	ft0, ft1;
	vec3_t	t, n;
	vec3_t	v1, v2, v3;

	VectorSubtract( point2, point0, n );
	if( !VectorNormalizeLength( n ))
		return 0;

	VectorSubtract( point1, point0, t );
	d = -DotProduct( t, n );
	VectorMA( t, d, n, t );
	if( DotProduct( t, t ) < maxflat2 )
		return 0;

	VectorAverage( point1, point0, v1 );
	VectorAverage( point2, point1, v2 );
	VectorAverage( v1, v2, v3 );

	ft0 = Patch_FlatnessTest( maxflat2, point0, v1, v3 );
	ft1 = Patch_FlatnessTest( maxflat2, v3, v2, point2 );

	return 1 + (int)( floor( max( ft0, ft1 )) + 0.5f );
}

/*
===============
Patch_GetFlatness

===============
 */
void Patch_GetFlatness( float maxflat, const float *points, int comp, const int *patch_cp, int *flat )
{
	int	i, p, u, v;
	float	maxflat2 = maxflat * maxflat;

	flat[0] = flat[1] = 0;

	for( v = 0; v < patch_cp[1] - 1; v += 2 )
	{
		for( u = 0; u < patch_cp[0] - 1; u += 2 )
		{
			p = v * patch_cp[0] + u;

			i = Patch_FlatnessTest( maxflat2, &points[p*comp], &points[( p+1 )*comp], &points[( p+2 )*comp] );
			flat[0] = max( flat[0], i );
			i = Patch_FlatnessTest( maxflat2, &points[( p+patch_cp[0] )*comp], &points[( p+patch_cp[0]+1 )*comp], &points[( p+patch_cp[0]+2 )*comp] );
			flat[0] = max( flat[0], i );
			i = Patch_FlatnessTest( maxflat2, &points[( p+2*patch_cp[0] )*comp], &points[( p+2*patch_cp[0]+1 )*comp], &points[( p+2*patch_cp[0]+2 )*comp] );
			flat[0] = max( flat[0], i );

			i = Patch_FlatnessTest( maxflat2, &points[p*comp], &points[( p+patch_cp[0] )*comp], &points[( p+2*patch_cp[0] )*comp] );
			flat[1] = max( flat[1], i );
			i = Patch_FlatnessTest( maxflat2, &points[( p+1 )*comp], &points[( p+patch_cp[0]+1 )*comp], &points[( p+2*patch_cp[0]+1 )*comp] );
			flat[1] = max( flat[1], i );
			i = Patch_FlatnessTest( maxflat2, &points[( p+2 )*comp], &points[( p+patch_cp[0]+2 )*comp], &points[( p+2*patch_cp[0]+2 )*comp] );
			flat[1] = max( flat[1], i );
		}
	}
}

/*
===============
Patch_Evaluate_QuadricBezier

===============
 */
static void Patch_Evaluate_QuadricBezier( float t, const vec_t *point0, const vec_t *point1, const vec_t *point2, vec_t *out, int comp )
{
	int	i;
	vec_t	qt = t * t;
	vec_t	dt = 2.0f * t, tt, tt2;

	tt = 1.0f - dt + qt;
	tt2 = dt - 2.0f * qt;

	for( i = 0; i < comp; i++ )
		out[i] = point0[i] * tt + point1[i] * tt2 + point2[i] * qt;
}

/*
===============
Patch_Evaluate

===============
 */
void Patch_Evaluate( const vec_t *p, int *numcp, const int *tess, vec_t *dest, int comp )
{
	int	num_patches[2], num_tess[2];
	int	index[3], dstpitch, i, u, v, x, y;
	float	s, t, step[2];
	vec_t	*tvec, *tvec2;
	const vec_t *pv[3][3];
	vec4_t	v1, v2, v3;

	num_patches[0] = numcp[0] / 2;
	num_patches[1] = numcp[1] / 2;
	dstpitch = ( num_patches[0] * tess[0] + 1 ) * comp;

	step[0] = 1.0f / (float)tess[0];
	step[1] = 1.0f / (float)tess[1];

	for( v = 0; v < num_patches[1]; v++ )
	{
		// last patch has one more row
		if( v < num_patches[1] - 1 )
			num_tess[1] = tess[1];
		else num_tess[1] = tess[1] + 1;

		for( u = 0; u < num_patches[0]; u++ )
		{
			// last patch has one more column
			if( u < num_patches[0] - 1 )
				num_tess[0] = tess[0];
			else num_tess[0] = tess[0] + 1;

			index[0] = ( v * numcp[0] + u ) * 2;
			index[1] = index[0] + numcp[0];
			index[2] = index[1] + numcp[0];

			// current 3x3 patch control points
			for( i = 0; i < 3; i++ )
			{
				pv[i][0] = &p[( index[0]+i ) * comp];
				pv[i][1] = &p[( index[1]+i ) * comp];
				pv[i][2] = &p[( index[2]+i ) * comp];
			}

			tvec = dest + v * tess[1] * dstpitch + u * tess[0] * comp;
			for( y = 0, t = 0.0f; y < num_tess[1]; y++, t += step[1], tvec += dstpitch )
			{
				Patch_Evaluate_QuadricBezier( t, pv[0][0], pv[0][1], pv[0][2], v1, comp );
				Patch_Evaluate_QuadricBezier( t, pv[1][0], pv[1][1], pv[1][2], v2, comp );
				Patch_Evaluate_QuadricBezier( t, pv[2][0], pv[2][1], pv[2][2], v3, comp );

				for( x = 0, tvec2 = tvec, s = 0.0f; x < num_tess[0]; x++, s += step[0], tvec2 += comp )
					Patch_Evaluate_QuadricBezier( s, v1, v2, v3, tvec2, comp );
			}
		}
	}
}