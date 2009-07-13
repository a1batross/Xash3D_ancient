//=======================================================================
//			Copyright XashXT Group 2009 ©
//			patch.c - generate bezier curves
//=======================================================================

#include "launch.h"
#include "mathlib.h"

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
	int i, p, u, v;
	float maxflat2 = maxflat * maxflat;

	flat[0] = flat[1] = 0;
	for( v = 0; v < patch_cp[1] - 1; v += 2 )
	{
		for( u = 0; u < patch_cp[0] - 1; u += 2 )
		{
			p = v * patch_cp[0] + u;

			i = Patch_FlatnessTest( maxflat2, &points[p*comp], &points[(p+1)*comp], &points[(p+2)*comp] );
			flat[0] = max( flat[0], i );
			i = Patch_FlatnessTest( maxflat2, &points[(p+patch_cp[0])*comp], &points[(p+patch_cp[0]+1)*comp], &points[(p+patch_cp[0]+2)*comp] );
			flat[0] = max( flat[0], i );
			i = Patch_FlatnessTest( maxflat2, &points[(p+2*patch_cp[0])*comp], &points[(p+2*patch_cp[0]+1)*comp], &points[(p+2*patch_cp[0]+2)*comp] );
			flat[0] = max( flat[0], i );

			i = Patch_FlatnessTest( maxflat2, &points[p*comp], &points[(p+patch_cp[0])*comp], &points[(p+2*patch_cp[0])*comp] );
			flat[1] = max( flat[1], i );
			i = Patch_FlatnessTest( maxflat2, &points[(p+1)*comp], &points[(p+patch_cp[0]+1)*comp], &points[(p+2*patch_cp[0]+1)*comp] );
			flat[1] = max( flat[1], i );
			i = Patch_FlatnessTest( maxflat2, &points[(p+2)*comp], &points[(p+patch_cp[0]+2)*comp], &points[(p+2*patch_cp[0]+2)*comp] );
			flat[1] = max( flat[1], i );
		}
	}
}

/*
===============
Patch_Evaluate_QuadricBezier
===============
*/
static void Patch_Evaluate_QuadricBezier( float t, const float *point0, const float *point1, const float *point2, float *out, int comp )
{
	int		i;
	float	qt = t * t;
	float	dt = 2.0f * t, tt, tt2;

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
void Patch_Evaluate( const float *p, int *numcp, const int *tess, float *dest, int comp )
{
	int num_patches[2], num_tess[2];
	int index[3], dstpitch, i, u, v, x, y;
	float s, t, step[2];
	float *tvec, *tvec2;
	const float *pv[3][3];
	vec4_t v1, v2, v3;

	num_patches[0] = numcp[0] / 2;
	num_patches[1] = numcp[1] / 2;
	dstpitch = (num_patches[0] * tess[0] + 1) * comp;

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

			index[0] = (v * numcp[0] + u) * 2;
			index[1] = index[0] + numcp[0];
			index[2] = index[1] + numcp[0];

			// current 3x3 patch control points
			for( i = 0; i < 3; i++ )
			{
				pv[i][0] = &p[(index[0]+i) * comp];
				pv[i][1] = &p[(index[1]+i) * comp];
				pv[i][2] = &p[(index[2]+i) * comp];
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
