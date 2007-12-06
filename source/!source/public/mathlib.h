//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         mathlib.h - base math functions
//=======================================================================
#ifndef BASEMATH_H
#define BASEMATH_H

#include <math.h>
#include "basetypes.h"

#define SIDE_FRONT		0
#define SIDE_BACK		1
#define SIDE_ON		2

#ifndef M_PI
#define M_PI		(float)3.14159265358979323846
#endif

// network precision coords factor
#define SV_COORD_FRAC	(8.0f / 1.0f)
#define CL_COORD_FRAC	(1.0f / 8.0f)
#define SV_ANGLE_FRAC	(360.0f / 1.0f )
#define CL_ANGLE_FRAC	(1.0f / 360.0f )

#define METERS_PER_INCH	0.0254f
#define EQUAL_EPSILON	0.001f
#define STOP_EPSILON	0.1f

#define ANGLE2CHAR(x)	((int)((x)*256/360) & 255)
#define CHAR2ANGLE(x)	((x)*(360.0/256))
#define ANGLE2SHORT(x)	((int)((x)*65536/360) & 65535)
#define SHORT2ANGLE(x)	((x)*(360.0/65536))
#define RAD2DEG( x )	((float)(x) * (float)(180.f / M_PI))
#define DEG2RAD( x )	((float)(x) * (float)(M_PI / 180.f))
#define METER2INCH(x)	(float)(x * (1.0f/METERS_PER_INCH))
#define INCH2METER(x)	(float)(x * (METERS_PER_INCH/1.0f))
#define RAD_TO_STUDIO	(32768.0 / M_PI)
#define STUDIO_TO_RAD	(M_PI / 32768.0)
#define nanmask		(255<<23)

#define IS_NAN(x)		(((*(int *)&x)&nanmask)==nanmask)
#define RANDOM_LONG(MIN, MAX)	((rand() & 32767) * (((MAX)-(MIN)) * (1.0f / 32767.0f)) + (MIN))
#define RANDOM_FLOAT(MIN,MAX)	(((float)rand() / RAND_MAX) * ((MAX)-(MIN)) + (MIN))
#define bound(min, num, max)	((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))

#define VectorToPhysic(v) { v[0] = INCH2METER(v[0]), v[1] = INCH2METER(v[1]), v[2] = INCH2METER(v[2]); }
#define VectorToServer(v) { v[0] = METER2INCH(v[0]), v[1] = METER2INCH(v[1]), v[2] = METER2INCH(v[2]); }
#define DotProduct(x,y) (x[0]*y[0]+x[1]*y[1]+x[2]*y[2])
#define VectorSubtract(a,b,c){c[0]=a[0]-b[0];c[1]=a[1]-b[1];c[2]=a[2]-b[2];}
#define Vector4Subtract(a,b,c){c[0]=a[0]-b[0];c[1]=a[1]-b[1];c[2]=a[2]-b[2];c[3]=a[3]-b[3];}
#define VectorAdd(a,b,c) {c[0]=a[0]+b[0];c[1]=a[1]+b[1];c[2]=a[2]+b[2];}
#define VectorCopy(a,b) {b[0]=a[0];b[1]=a[1];b[2]=a[2];}
#define Vector4Copy(a,b) {b[0]=a[0];b[1]=a[1];b[2]=a[2];b[3]=a[3];}
#define VectorScale(in, scale, out) ((out)[0] = (in)[0] * (scale),(out)[1] = (in)[1] * (scale),(out)[2] = (in)[2] * (scale))
#define Vector4Scale(in, scale, out) ((out)[0] = (in)[0] * (scale),(out)[1] = (in)[1] * (scale),(out)[2] = (in)[2] * (scale),(out)[3] = (in)[3] * (scale))
#define VectorMultiply(a,b,c) ((c)[0]=(a)[0]*(b)[0],(c)[1]=(a)[1]*(b)[1],(c)[2]=(a)[2]*(b)[2])
#define VectorLength2(a) (DotProduct(a, a))
#define VectorDistance(a, b) (sqrt(VectorDistance2(a,b)))
#define VectorDistance2(a, b) (((a)[0] - (b)[0]) * ((a)[0] - (b)[0]) + ((a)[1] - (b)[1]) * ((a)[1] - (b)[1]) + ((a)[2] - (b)[2]) * ((a)[2] - (b)[2]))
#define VectorSet(v, x, y, z) {v[0] = x; v[1] = y; v[2] = z;}
#define Vector4Set(v, x, y, z, w) {v[0] = x, v[1] = y, v[2] = z, v[3] = w;}
#define VectorClear(x) {x[0] = x[1] = x[2] = 0;}
#define Vector4Clear(x) {x[0] = x[1] = x[2] = x[3] = 0;}
#define VectorNegate(x, y) {y[0] =-x[0]; y[1]=-x[1]; y[2]=-x[2];}
#define VectorM(scale1, b1, c) ((c)[0] = (scale1) * (b1)[0],(c)[1] = (scale1) * (b1)[1],(c)[2] = (scale1) * (b1)[2])
#define VectorMA(a, scale, b, c) ((c)[0] = (a)[0] + (scale) * (b)[0],(c)[1] = (a)[1] + (scale) * (b)[1],(c)[2] = (a)[2] + (scale) * (b)[2])
#define VectorMAM(scale1, b1, scale2, b2, c) ((c)[0] = (scale1) * (b1)[0] + (scale2) * (b2)[0],(c)[1] = (scale1) * (b1)[1] + (scale2) * (b2)[1],(c)[2] = (scale1) * (b1)[2] + (scale2) * (b2)[2])
#define VectorMAMAM(scale1, b1, scale2, b2, scale3, b3, c) ((c)[0] = (scale1) * (b1)[0] + (scale2) * (b2)[0] + (scale3) * (b3)[0],(c)[1] = (scale1) * (b1)[1] + (scale2) * (b2)[1] + (scale3) * (b3)[1],(c)[2] = (scale1) * (b1)[2] + (scale2) * (b2)[2] + (scale3) * (b3)[2])
#define VectorMAMAMAM(scale1, b1, scale2, b2, scale3, b3, scale4, b4, c) ((c)[0] = (scale1) * (b1)[0] + (scale2) * (b2)[0] + (scale3) * (b3)[0] + (scale4) * (b4)[0],(c)[1] = (scale1) * (b1)[1] + (scale2) * (b2)[1] + (scale3) * (b3)[1] + (scale4) * (b4)[1],(c)[2] = (scale1) * (b1)[2] + (scale2) * (b2)[2] + (scale3) * (b3)[2] + (scale4) * (b4)[2])
#define MatrixLoadIdentity(mat) {Vector4Set(mat[0], 1, 0, 0, 0); Vector4Set(mat[1], 0, 1, 0, 0); Vector4Set(mat[2], 0, 0, 1, 0); Vector4Set(mat[3], 0, 0, 0, 1); }
_inline float anglemod(const float a){return(360.0/65536) * ((int)(a*(65536/360.0)) & 65535);}

_inline int nearest_pow(int size)
{
	int i = 2;

	while( 1 ) 
	{
		i <<= 1;
		if (size == i) return i;
		if (size > i && size < (i <<1)) 
		{
			if (size >= ((i+(i<<1))/2))
				return i<<1;
			else return i;
		}
	}
}

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

	for (i = 0; i< 3; i++) result += v[i];
	if(result != 0) return false;
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

_inline void VectorRotate (const vec3_t in1, vec3_t in2[3], vec3_t out)
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

/*
====================
VectorTransform

====================
*/
_inline void VectorTransform (const vec3_t in1, matrix3x4 in2, vec3_t out)
{
	out[0] = DotProduct(in1, in2[0]) + in2[0][3];
	out[1] = DotProduct(in1, in2[1]) + in2[1][3];
	out[2] = DotProduct(in1, in2[2]) + in2[2][3];
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
	if (forward)
	{
		forward[0] = cp*cy;
		forward[1] = cp*sy;
		forward[2] = -sp;
	}
	if (right || up)
	{
		if (angles[ROLL])
		{
			angle = angles[ROLL] * (M_PI*2 / 360);
			sr = sin(angle);
			cr = cos(angle);
			if (right)
			{
				right[0] = -1*(sr*sp*cy+cr*-sy);
				right[1] = -1*(sr*sp*sy+cr*cy);
				right[2] = -1*(sr*cp);
			}
			if (up)
			{
				up[0] = (cr*sp*cy+-sr*-sy);
				up[1] = (cr*sp*sy+-sr*cy);
				up[2] = cr*cp;
			}
		}
		else
		{
			if (right)
			{
				right[0] = sy;
				right[1] = -cy;
				right[2] = 0;
			}
			if (up)
			{
				up[0] = (sp*cy);
				up[1] = (sp*sy);
				up[2] = cp;
			}
		}
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

/*
====================
AngleMatrix

====================
*/

_inline void AngleMatrix(const vec3_t angles, matrix3x4 matrix )
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	
	angle = angles[YAW] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[ROLL] * (M_PI*2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	// matrix = (YAW * PITCH) * ROLL
	matrix[0][0] = cp*cy;
	matrix[1][0] = cp*sy;
	matrix[2][0] = -sp;
	matrix[0][1] = sr*sp*cy+cr*-sy;
	matrix[1][1] = sr*sp*sy+cr*cy;
	matrix[2][1] = sr*cp;
	matrix[0][2] = (cr*sp*cy+-sr*-sy);
	matrix[1][2] = (cr*sp*sy+-sr*cy);
	matrix[2][2] = cr*cp;
	matrix[0][3] = 0.0;
	matrix[1][3] = 0.0;
	matrix[2][3] = 0.0;
}

_inline void AngleMatrixFLU( const vec3_t angles, matrix3x4 matrix )
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	
	angle = angles[ROLL] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[YAW] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[PITCH] * (M_PI*2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	// matrix = (YAW * PITCH) * ROLL
	matrix[0][0] = cp*cy;
	matrix[1][0] = cp*sy;
	matrix[2][0] = -sp;
	matrix[0][1] = sr*sp*cy+cr*-sy;
	matrix[1][1] = sr*sp*sy+cr*cy;
	matrix[2][1] = sr*cp;
	matrix[0][2] = (cr*sp*cy+-sr*-sy);
	matrix[1][2] = (cr*sp*sy+-sr*cy);
	matrix[2][2] = cr*cp;
	matrix[0][3] = 0.0;
	matrix[1][3] = 0.0;
	matrix[2][3] = 0.0;
}

_inline void AngleIMatrix(const vec3_t angles, matrix3x4 matrix )
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	
	angle = angles[YAW] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[ROLL] * (M_PI*2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	// matrix = (YAW * PITCH) * ROLL
	matrix[0][0] = cp*cy;
	matrix[0][1] = cp*sy;
	matrix[0][2] = -sp;
	matrix[1][0] = sr*sp*cy+cr*-sy;
	matrix[1][1] = sr*sp*sy+cr*cy;
	matrix[1][2] = sr*cp;
	matrix[2][0] = (cr*sp*cy+-sr*-sy);
	matrix[2][1] = (cr*sp*sy+-sr*cy);
	matrix[2][2] = cr*cp;
	matrix[0][3] = 0.0;
	matrix[1][3] = 0.0;
	matrix[2][3] = 0.0;
}

_inline void AngleIMatrixFLU(const vec3_t angles, matrix3x4 matrix )
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	
	angle = angles[ROLL] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[YAW] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[PITCH] * (M_PI*2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	// matrix = (Z * Y) * X
	matrix[0][0] = cp*cy;
	matrix[0][1] = cp*sy;
	matrix[0][2] = -sp;
	matrix[1][0] = sr*sp*cy+cr*-sy;
	matrix[1][1] = sr*sp*sy+cr*cy;
	matrix[1][2] = sr*cp;
	matrix[2][0] = (cr*sp*cy+-sr*-sy);
	matrix[2][1] = (cr*sp*sy+-sr*cy);
	matrix[2][2] = cr*cp;
	matrix[0][3] = 0.0;
	matrix[1][3] = 0.0;
	matrix[2][3] = 0.0;
}

/*
====================
MatrixCopy

====================
*/
_inline void MatrixCopy( matrix3x4 in, matrix3x4 out )
{
	memcpy( out, in, sizeof( matrix3x4 ));
}


_inline void MatrixAngles( const matrix4x4 matrix, vec3_t origin, vec3_t angles )
{ 
	vec3_t	forward, right, up;
	float	xyDist;

	forward[0] = matrix[0][0];
	forward[1] = matrix[0][2];
	forward[2] = matrix[0][1];
	right[0] = matrix[1][0];
	right[1] = matrix[1][2];
	right[2] = matrix[1][1];
	up[2] = matrix[2][1];
	
	xyDist = sqrt( forward[0] * forward[0] + forward[1] * forward[1] );
	
	if ( xyDist > EQUAL_EPSILON )	// enough here to get angles?
	{
		angles[1] = RAD2DEG( atan2( forward[1], forward[0] ));
		angles[0] = RAD2DEG( atan2( -forward[2], xyDist ));
		angles[2] = RAD2DEG( atan2( -right[2], up[2] )) + 180;
	}
	else
	{
		angles[1] = RAD2DEG( atan2( right[0], -right[1] ) );
		angles[0] = RAD2DEG( atan2( -forward[2], xyDist ) );
		angles[2] = 180;
	}
	VectorCopy(matrix[3], origin );// extract origin
}

_inline void MatrixAnglesFLU( const matrix4x4 matrix, vec3_t origin, vec3_t angles )
{ 
	vec3_t	forward, left, up;
	float	xyDist;

	forward[0] = matrix[0][0];
	forward[1] = matrix[0][2];
	forward[2] = matrix[0][1];
	left[0] = matrix[1][0];
	left[1] = matrix[1][2];
	left[2] = matrix[1][1];
	up[2] = matrix[2][1];

	xyDist = sqrt( forward[0] * forward[0] + forward[1] * forward[1] );

	if ( xyDist > EQUAL_EPSILON ) // enough here to get angles?
	{
		angles[1] = RAD2DEG( atan2( forward[1], forward[0] ) );
		angles[0] = RAD2DEG( atan2( -forward[2], xyDist ) );
		angles[2] = RAD2DEG( atan2( left[2], up[2] ) );
	}
	else
	{
		angles[1] = RAD2DEG( atan2( -left[0], left[1] ) );
		angles[0] = RAD2DEG( atan2( -forward[2], xyDist ) );
		angles[2] = 0;
	}
	VectorCopy(matrix[3], origin );// extract origin
}

_inline void AnglesMatrix(const vec3_t origin, const vec3_t angles, matrix4x4 matrix )
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;

	angle = angles[YAW] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);

	// forward
	matrix[0][0] = cp*cy;
	matrix[0][2] = cp*sy;
	matrix[0][1] = -sp;

	if (angles[ROLL])
	{
		angle = angles[ROLL] * (M_PI*2 / 360);
		sr = sin(angle);
		cr = cos(angle);

		// right
		matrix[1][0] = -1*(sr*sp*cy+cr*-sy);
		matrix[1][2] = -1*(sr*sp*sy+cr*cy);
		matrix[1][1] = -1*(sr*cp);

		// up
		matrix[2][0] = (cr*sp*cy+-sr*-sy);
		matrix[2][2] = (cr*sp*sy+-sr*cy);
		matrix[2][1] = cr*cp;
	}
	else
	{
		// right
		matrix[1][0] = sy;
		matrix[1][2] = -cy;
		matrix[1][1] = 0;

		// up
		matrix[2][0] = (sp*cy);
		matrix[2][2] = (sp*sy);
		matrix[2][1] = cp;
	}
	VectorCopy(origin, matrix[3] ); // pack origin
}


/*
====================
AngleQuaternion

====================
*/
_inline void AngleQuaternion( float *angles, vec4_t q )
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;

	// FIXME: rescale the inputs to 1/2 angle
	angle = angles[2] * 0.5;
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[1] * 0.5;
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[0] * 0.5;
	sr = sin(angle);
	cr = cos(angle);

	q[0] = sr*cp*cy-cr*sp*sy; // X
	q[1] = cr*sp*cy+sr*cp*sy; // Y
	q[2] = cr*cp*sy-sr*sp*cy; // Z
	q[3] = cr*cp*cy+sr*sp*sy; // W
}

_inline void QuaternionAngles( vec4_t q, vec3_t angles )
{
	float m11, m12, m13, m23, m33;

	m11 = ( 2.0f * q[3] * q[3] ) + ( 2.0f * q[0] * q[0] ) - 1.0f;
	m12 = ( 2.0f * q[0] * q[1] ) + ( 2.0f * q[3] * q[2] );
	m13 = ( 2.0f * q[0] * q[2] ) - ( 2.0f * q[3] * q[1] );
	m23 = ( 2.0f * q[1] * q[2] ) + ( 2.0f * q[3] * q[0] );
	m33 = ( 2.0f * q[3] * q[3] ) + ( 2.0f * q[2] * q[2] ) - 1.0f;

	// FIXME: this code has a singularity near PITCH +-90
	angles[YAW] = RAD2DEG( atan2(m12, m11));
	angles[PITCH] = RAD2DEG( asin(-m13));
	angles[ROLL] = RAD2DEG( atan2(m23, m33));
}

/*
====================
QuaternionMatrix

====================
*/
_inline void QuaternionMatrix( vec4_t quaternion, float (*matrix)[4] )
{
	matrix[0][0] = 1.0 - 2.0 * quaternion[1] * quaternion[1] - 2.0 * quaternion[2] * quaternion[2];
	matrix[1][0] = 2.0 * quaternion[0] * quaternion[1] + 2.0 * quaternion[3] * quaternion[2];
	matrix[2][0] = 2.0 * quaternion[0] * quaternion[2] - 2.0 * quaternion[3] * quaternion[1];

	matrix[0][1] = 2.0 * quaternion[0] * quaternion[1] - 2.0 * quaternion[3] * quaternion[2];
	matrix[1][1] = 1.0 - 2.0 * quaternion[0] * quaternion[0] - 2.0 * quaternion[2] * quaternion[2];
	matrix[2][1] = 2.0 * quaternion[1] * quaternion[2] + 2.0 * quaternion[3] * quaternion[0];

	matrix[0][2] = 2.0 * quaternion[0] * quaternion[2] + 2.0 * quaternion[3] * quaternion[1];
	matrix[1][2] = 2.0 * quaternion[1] * quaternion[2] - 2.0 * quaternion[3] * quaternion[0];
	matrix[2][2] = 1.0 - 2.0 * quaternion[0] * quaternion[0] - 2.0 * quaternion[1] * quaternion[1];
}


/*
====================
QuaternionSlerp

====================
*/
_inline void QuaternionSlerp( vec4_t p, vec4_t q, float t, vec4_t qt )
{
	int i;
	float	omega, cosom, sinom, sclp, sclq;

	// decide if one of the quaternions is backwards
	float a = 0;
	float b = 0;

	for (i = 0; i < 4; i++)
	{
		a += (p[i]-q[i])*(p[i]-q[i]);
		b += (p[i]+q[i])*(p[i]+q[i]);
	}
	if (a > b)
	{
		for (i = 0; i < 4; i++)
		{
			q[i] = -q[i];
		}
	}

	cosom = p[0]*q[0] + p[1]*q[1] + p[2]*q[2] + p[3]*q[3];

	if ((1.0 + cosom) > 0.000001)
	{
		if ((1.0 - cosom) > 0.000001)
		{
			omega = acos( cosom );
			sinom = sin( omega );
			sclp = sin( (1.0 - t)*omega) / sinom;
			sclq = sin( t*omega ) / sinom;
		}
		else
		{
			sclp = 1.0 - t;
			sclq = t;
		}
		for (i = 0; i < 4; i++) qt[i] = sclp * p[i] + sclq * q[i];
	}
	else
	{
		qt[0] = -q[1];
		qt[1] = q[0];
		qt[2] = -q[3];
		qt[3] = q[2];
		sclp = sin( (1.0 - t) * (0.5 * M_PI));
		sclq = sin( t * (0.5 * M_PI));
		for (i = 0; i < 3; i++)
		{
			qt[i] = sclp * p[i] + sclq * qt[i];
		}
	}
}

/*
================
ConcatRotations
================
*/
_inline void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];
}


/*
================
ConcatTransforms
================
*/
_inline void R_ConcatTransforms( matrix3x4 in1, matrix3x4 in2, matrix3x4 out )
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] + in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] + in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] + in1[2][2] * in2[2][3] + in1[2][3];
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

_inline float *GetRGBA( float r, float g, float b, float a )
{
	static vec4_t	color;

	Vector4Set( color, r, g, b, a );
	return color;
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

_inline int ColorStrlen( const char *string )
{
	int		len;
	const char	*p;

	if( !string ) return 0;

	len = 0;
	p = string;
	while( *p )
	{
		if(IsColorString( p ))
		{
			p += 2;
			continue;
		}
		p++;
		len++;
	}
	return len;
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

_inline void ConvertDimensionToPhysic( vec3_t v )
{
	vec3_t	tmp;

	VectorCopy(v, tmp);
	v[0] = INCH2METER(tmp[0]);
	v[1] = INCH2METER(tmp[1]);
	v[2] = INCH2METER(tmp[2]);
}

_inline void ConvertDimensionToGame( vec3_t v )
{
	vec3_t	tmp;

	VectorCopy(v, tmp);
	v[0] = METER2INCH(tmp[0]);
	v[1] = METER2INCH(tmp[1]);
	v[2] = METER2INCH(tmp[2]);
}

_inline void ConvertPositionToPhysic( vec3_t v )
{
	vec3_t	tmp;

	VectorCopy(v, tmp);
	v[0] = INCH2METER(tmp[0]);
	v[1] = INCH2METER(tmp[2]);
	v[2] = INCH2METER(tmp[1]);
}

_inline void ConvertPositionToGame( vec3_t v )
{
	vec3_t	tmp;

	VectorCopy(v, tmp);

	v[2] = METER2INCH(tmp[1]);
	v[1] = METER2INCH(tmp[2]);
	v[0] = METER2INCH(tmp[0]);
}

_inline void ConvertDirectionToPhysic( vec3_t v )
{
	vec3_t	tmp;

	VectorCopy(v, tmp);
	v[0] = tmp[0];
	v[1] = tmp[2];
	v[2] = tmp[1];
}

_inline void ConvertDirectionToGame( vec3_t v )
{
	vec3_t	tmp;

	VectorCopy(v, tmp);
	v[0] = tmp[0];
	v[1] = tmp[2];
	v[2] = tmp[1];
}

static vec3_t vec3_origin = { 0, 0, 0 };
static vec3_t vec3_angles = { 0, 0, 0 };


#endif//BASEMATH_H