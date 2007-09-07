//=======================================================================
//			Copyright XashXT Group 2007 ©
//			basemath.h - base math functions
//=======================================================================
#ifndef BASEMATH_H
#define BASEMATH_H

#include <math.h>
#include "bspmodel.h"

#define SIDE_FRONT		0
#define SIDE_BACK		1
#define SIDE_ON		2

#define EQUAL_EPSILON	0.001
#define STOP_EPSILON	0.1
#define DEG2RAD( a )	( a * M_PI ) / 180.0F

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

#define RANDOM_DOUBLE(MIN, MAX) (((double)rand() / RAND_MAX) * ((MAX)-(MIN)) + (MIN))
#define RANDOM_FLOAT(MIN, MAX) (((float)rand() / RAND_MAX) * ((MAX)-(MIN)) + (MIN))
#define RANDOM_LONG(MIN, MAX) (((long)rand() / RAND_MAX) * ((MAX)-(MIN)) + (MIN))
#define bound(min,num,max) ((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))

#define DotProduct(x,y) (x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
#define VectorSubtract(a,b,c){c[0]=a[0]-b[0];c[1]=a[1]-b[1];c[2]=a[2]-b[2];}
#define VectorAdd(a,b,c) {c[0]=a[0]+b[0];c[1]=a[1]+b[1];c[2]=a[2]+b[2];}
#define VectorCopy(a,b){b[0]=a[0];b[1]=a[1];b[2]=a[2];}
_inline void VectorScale(const vec3_t a, const float b, vec3_t c){c[0]=b*a[0];c[1]=b*a[1];c[2]=b*a[2];}
#define VectorSet(v, x, y, z) {v[0] = x; v[1] = y; v[2] = z;}
#define VectorClear(x) {x[0] = x[1] = x[2] = 0;}
#define VectorNegate(x, y) {y[0] =-x[0]; y[1]=-x[1]; y[2]=-x[2];}
#define VectorM(scale1, b1, c) ((c)[0] = (scale1) * (b1)[0],(c)[1] = (scale1) * (b1)[1],(c)[2] = (scale1) * (b1)[2])
#define VectorMA(a, scale, b, c) ((c)[0] = (a)[0] + (scale) * (b)[0],(c)[1] = (a)[1] + (scale) * (b)[1],(c)[2] = (a)[2] + (scale) * (b)[2])
#define VectorMAM(scale1, b1, scale2, b2, c) ((c)[0] = (scale1) * (b1)[0] + (scale2) * (b2)[0],(c)[1] = (scale1) * (b1)[1] + (scale2) * (b2)[1],(c)[2] = (scale1) * (b1)[2] + (scale2) * (b2)[2])
#define VectorMAMAM(scale1, b1, scale2, b2, scale3, b3, c) ((c)[0] = (scale1) * (b1)[0] + (scale2) * (b2)[0] + (scale3) * (b3)[0],(c)[1] = (scale1) * (b1)[1] + (scale2) * (b2)[1] + (scale3) * (b3)[1],(c)[2] = (scale1) * (b1)[2] + (scale2) * (b2)[2] + (scale3) * (b3)[2])
#define VectorMAMAMAM(scale1, b1, scale2, b2, scale3, b3, scale4, b4, c) ((c)[0] = (scale1) * (b1)[0] + (scale2) * (b2)[0] + (scale3) * (b3)[0] + (scale4) * (b4)[0],(c)[1] = (scale1) * (b1)[1] + (scale2) * (b2)[1] + (scale3) * (b3)[1] + (scale4) * (b4)[1],(c)[2] = (scale1) * (b1)[2] + (scale2) * (b2)[2] + (scale3) * (b3)[2] + (scale4) * (b4)[2])
_inline float anglemod(const float a){return(360.0/65536) * ((int)(a*(65536/360.0)) & 65535);}

_inline void VectorBound(const float min, vec3_t v, const float max)
{
	v[0] = bound(min, v[0], max);
	v[1] = bound(min, v[1], max);
	v[2] = bound(min, v[2], max);
}

_inline void VectorCeil( vec3_t v)
{
	v[0] = ceil(v[0]);
	v[1] = ceil(v[1]);
	v[2] = ceil(v[2]);
}

_inline double VectorLength(vec3_t v)
{
	int	i;
	double	length;
	
	length = 0;
	for (i=0 ; i< 3 ; i++) length += v[i]*v[i];
	length = sqrt (length); // FIXME
	return length;
}

_inline bool VectorIsNull(vec3_t v)
{
	int i;
	float result = 0;

	for (i=0 ; i< 3 ; i++) result += v[i];
	if(result > 0) return false;
	return true;		
}

_inline bool VectorCompare (const vec3_t v1, const vec3_t v2)
{
	int		i;
	
	for (i = 0; i < 3; i++ )
		if (fabs(v1[i] - v2[i]) > EQUAL_EPSILON)
			return false;
	return true;
}

_inline bool VectorICompare (const short* v1, const short* v2)
{
	int		i;
	
	for (i = 0; i < 3; i++ )
		if (abs(v1[i] - v2[i]) > 0)
			return false;
	return true;
}

_inline vec_t VectorNormalize (vec3_t v)
{
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = sqrt (length);// FIXME

	if (length)
	{
		ilength = 1/length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}
		
	return length;

}

_inline void VectorRotate (const vec3_t in1, const matrix3x4 in2, vec3_t out)
{
	out[0] = DotProduct(in1, in2[0]);
	out[1] = DotProduct(in1, in2[1]);
	out[2] = DotProduct(in1, in2[2]);
}

// rotate by the inverse of the matrix
_inline void VectorIRotate (const vec3_t in1, const matrix3x4 in2, vec3_t out)
{
	out[0] = in1[0] * in2[0][0] + in1[1] * in2[1][0] + in1[2] * in2[2][0];
	out[1] = in1[0] * in2[0][1] + in1[1] * in2[1][1] + in1[2] * in2[2][1];
	out[2] = in1[0] * in2[0][2] + in1[1] * in2[1][2] + in1[2] * in2[2][2];
}

_inline void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

_inline void ClearBounds (vec3_t mins, vec3_t maxs)
{
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
}

_inline void AddPointToBounds (vec3_t v, vec3_t mins, vec3_t maxs)
{
	int	i;
	vec_t	val;

	for (i=0 ; i<3 ; i++)
	{
		val = v[i];
		if (val < mins[i]) mins[i] = val;
		if (val > maxs[i]) maxs[i] = val;
	}
}

_inline void VectorVectors(vec3_t forward, vec3_t right, vec3_t up)
{
	float d;

	right[0] = forward[2];
	right[1] = -forward[0];
	right[2] = forward[1];

	d = DotProduct(forward, right);
	VectorMA(right, -d, forward, right);
	VectorNormalize(right);
	CrossProduct(right, forward, up);
}

_inline void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	float		angle;
	static float	sr, sp, sy, cr, cp, cy;
	// static to help MS compiler fp bugs

	angle = angles[YAW] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[ROLL] * (M_PI*2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	if (forward)
	{
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;
	}
	if (right)
	{
		right[0] = (-1*sr*sp*cy+-1*cr*-sy);
		right[1] = (-1*sr*sp*sy+-1*cr*cy);
		right[2] = -1*sr*cp;
	}
	if (up)
	{
		up[0] = (cr*sp*cy+-sr*-sy);
		up[1] = (cr*sp*sy+-sr*cy);
		up[2] = cr*cp;
	}
}

_inline void AngleVectorsFLU(const vec3_t angles, vec3_t forward, vec3_t left, vec3_t up)
{
	float		angle;
	static float	sr, sp, sy, cr, cp, cy;
	// static to help MS compiler fp bugs

	angle = angles[YAW] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);

	if (forward)
	{
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;
	}
	if (left || up)
	{
		if (angles[ROLL])
		{
			angle = angles[ROLL] * (M_PI*2 / 360);
			sr = sin(angle);
			cr = cos(angle);
			if (left)
			{
				left[0] = sr*sp*cy+cr*-sy;
				left[1] = sr*sp*sy+cr*cy;
				left[2] = sr*cp;
			}
			if (up)
			{
				up[0] = cr*sp*cy+-sr*-sy;
				up[1] = cr*sp*sy+-sr*cy;
				up[2] = cr*cp;
			}
		}
		else
		{
			if (left)
			{
				left[0] = -sy;
				left[1] = cy;
				left[2] = 0;
			}
			if (up)
			{
				up[0] = sp*cy;
				up[1] = sp*sy;
				up[2] = cp;
			}
		}
	}
}

_inline void TransformRGB( vec3_t in, vec3_t out )
{
	out[0] = in[0]/255.0f;
	out[1] = in[1]/255.0f;
	out[2] = in[2]/255.0f;
}

_inline void TransformRGBA( vec4_t in, vec4_t out )
{
	out[0] = in[0]/255.0f;
	out[1] = in[1]/255.0f;
	out[2] = in[2]/255.0f;
	out[3] = in[3]/255.0f;
}

_inline void ResetRGBA( vec4_t in )
{
	in[0] = 1.0f;
	in[1] = 1.0f;
	in[2] = 1.0f;
	in[3] = 1.0f;
}

_inline vec_t ColorNormalize (vec3_t in, vec3_t out)
{
	float	max, scale;

	max = in[0];
	if (in[1] > max) max = in[1];
	if (in[2] > max) max = in[2];
	if (max == 0) return 0;
	scale = 1.0 / max;
	VectorScale (in, scale, out);
	return max;
}


/*
==============
BoxOnPlaneSide (engine fast version)

Returns SIDE_FRONT, SIDE_BACK, or SIDE_ON
==============
*/
_inline int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, cplane_t *p)
{
	if (p->type < 3) return ((emaxs[p->type] >= p->dist) | ((emins[p->type] < p->dist) << 1));
	switch(p->signbits)
	{
	default:
	case 0: return (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) >= p->dist) | (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 1: return (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) >= p->dist) | (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 2: return (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) >= p->dist) | (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 3: return (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) >= p->dist) | (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) < p->dist) << 1));
	case 4: return (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) >= p->dist) | (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	case 5: return (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2]) >= p->dist) | (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	case 6: return (((p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) >= p->dist) | (((p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	case 7: return (((p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2]) >= p->dist) | (((p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2]) < p->dist) << 1));
	}
}


static vec3_t vec3_origin = { 0, 0, 0 };
static vec3_t vec3_angles = { 0, 0, 0 };

#endif//BASEMATH_H