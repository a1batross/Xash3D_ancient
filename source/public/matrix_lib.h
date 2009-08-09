//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       matrix_lib.h - inline matrix library
//=======================================================================
#ifndef MATRIX_LIB_H
#define MATRIX_LIB_H

//#define OPENGL_STYLE		// TODO: enable OpenGL style someday

/*
	Quake engine tranformation matrix
		Quake	OpenGL	other changes
{ 0, 0,-1, 0, },	[ROLL]	[PITCH]	also put Z going up
{-1, 0, 0, 0, },	[PITCH]	[YAW]	also put Z going up
{ 0, 1, 0, 0, },	[YAW]	[ROLL]
{ 0, 0, 0, 1, },	[ORIGIN]	[ORIGIN]
*/

/*
========================================================================

		Matrix3x3 operations
	 matrix3x3 always keep in opengl style
========================================================================
*/
#define Matrix3x3_LoadIdentity( mat )	Matrix3x3_Copy( mat, matrix3x3_identity )

static const matrix3x3 matrix3x3_identity =
{
{ 1, 0, 0 },	// PITCH	[forward]
{ 0, 1, 0 },	// YAW	[right]
{ 0, 0, 1 },	// ROLL	[up]
};

_inline void Matrix3x3_Copy( matrix3x3 out, const matrix3x3 in )
{
	Mem_Copy( out, in, sizeof( matrix3x3 ));
}

_inline void Matrix3x3_FromNormal( const vec3_t normal, matrix3x3 out )
{
	out[0][0] = normal[0];
	out[0][1] = normal[1];
	out[0][2] = normal[2];

	if( normal[0] || normal[1] )
	{
		float	length;

		out[1][0] = normal[1];
		out[1][1] = -normal[0];
		out[1][2] = 0;

		length = out[1][0] * out[1][0] + out[1][1] * out[1][1] + out[1][2] * out[1][2];
		if( length != 0.0f )
		{
			float	ilength;
			ilength = 1.0f / com.sqrt( length );
			out[1][0] *= ilength;
			out[1][1] *= ilength;
			out[1][2] *= ilength;
		}

		out[2][0] = out[0][1] * out[1][2] - out[0][2] * out[1][1];
		out[2][1] = out[0][2] * out[1][0] - out[0][0] * out[1][2];
		out[2][2] = out[0][0] * out[1][1] - out[0][1] * out[1][0];

	}
	else
	{
		// set identity
		out[1][0] = 1.0f;
		out[1][1] = 0.0f;
		out[1][2] = 0.0f;
		out[2][0] = 0.0f;
		out[2][1] = 1.0f;
		out[2][2] = 0.0f;
	}
}

_inline void Matrix3x3_FromAngles( const vec3_t angles, matrix3x3 out )
{
	float	sp, sy, sr, cp, cy, cr;
	float	angle;

	angle = DEG2RAD( angles[PITCH] );
	sp = com.sin( angle );
	cp = com.cos( angle );
	angle = DEG2RAD( angles[YAW] );
	sy = com.sin( angle );
	cy = com.cos( angle );
	angle = DEG2RAD( angles[ROLL] );
	sr = com.sin( angle );
	cr = com.cos( angle );

	out[0][0] = cp*cy;
	out[0][1] = cp*sy;
	out[0][2] = -sp;
	out[1][0] = sr*sp*cy+cr*-sy;
	out[1][1] = sr*sp*sy+cr*cy;
	out[1][2] = sr*cp;
	out[2][0] = cr*sp*cy+-sr*-sy;
	out[2][1] = cr*sp*sy+-sr*cy;
	out[2][2] = cr*cp;
}

_inline void Matrix3x3_FromMatrix4x4( matrix3x3 out, const matrix4x4 in )
{
#ifdef OPENGL_STYLE
	out[0][0] = in[0][0];
	out[1][0] = in[1][0];
	out[2][0] = in[2][0];
	out[0][1] = in[0][1];
	out[1][1] = in[1][1];
	out[2][1] = in[2][1];
	out[0][2] = in[0][2];
	out[1][2] = in[1][2];
	out[2][2] = in[2][2];
#else
	out[0][0] = in[0][0];
	out[1][0] = in[0][1];
	out[2][0] = in[0][2];
	out[0][1] = in[1][0];
	out[1][1] = in[1][1];
	out[2][1] = in[1][2];
	out[0][2] = in[2][0];
	out[1][2] = in[2][1];
	out[2][2] = in[2][2];
#endif
}

_inline void Matrix3x3_ToAngles( const matrix3x3 matrix, vec3_t out, bool rhand )
{
	float	pitch, cpitch, yaw, roll;

	pitch = -com.asin( matrix[0][2] );
	cpitch = com.cos( pitch );

	if( fabs( cpitch ) > EQUAL_EPSILON )	// gimball lock?
	{
		cpitch = 1.0f / cpitch;
		pitch = RAD2DEG( pitch );
		if( rhand ) yaw = RAD2DEG( com.atan2( matrix[0][1] * cpitch, matrix[0][0] * cpitch ));
		else yaw = RAD2DEG( com.atan2((-1)*-matrix[0][1] * cpitch, matrix[0][0] * cpitch ));
		roll = RAD2DEG( com.atan2( -matrix[1][2] * cpitch, matrix[2][2] * cpitch ));
	}
	else
	{
		pitch = matrix[0][2] > 0 ? -90.0f : 90.0f;
		yaw = RAD2DEG( atan2( matrix[1][0], -matrix[1][1] ));
		if( rhand ) roll = 180;
		else roll = 0;
	}

	out[PITCH] = pitch;
	out[YAW] = yaw;
	out[ROLL] = roll;
}

_inline bool Matrix3x3_Compare( const matrix3x3 mat1, const matrix3x3 mat2 )
{
	if( mat1[0][0] != mat2[0][0] || mat1[0][1] != mat2[0][1] || mat1[0][2] != mat2[0][2] )
		return false;
	if( mat1[1][0] != mat2[1][0] || mat1[1][1] != mat2[1][1] || mat1[1][2] != mat2[1][2] )
		return false;
	if( mat1[2][0] != mat2[2][0] || mat1[2][1] != mat2[2][1] || mat1[2][2] != mat2[2][2] )
		return false;
	return true;
}

_inline void Matrix3x3_Transpose( matrix3x3 out, const matrix3x3 in )
{
	out[0][0] = in[0][0];
	out[1][0] = in[0][1];
	out[2][0] = in[0][2];
	out[0][1] = in[1][0];
	out[1][1] = in[1][1];
	out[2][1] = in[1][2];
	out[0][2] = in[2][0];
	out[1][2] = in[2][1];
	out[2][2] = in[2][2];
}

_inline void Matrix3x3_Transform( matrix3x3 in, const float v[3], float out[3] )
{
	out[0] = in[0][0] * v[0] + in[0][1] * v[1] + in[0][2] * v[2];
	out[1] = in[1][0] * v[0] + in[1][1] * v[1] + in[1][2] * v[2];
	out[2] = in[2][0] * v[0] + in[2][1] * v[1] + in[2][2] * v[2];
}

_inline void Matrix3x3_CreateRotate( matrix3x3 out, float angle, float x, float y, float z )
{
	float	len, c, s;

	len = x * x + y * y + z * z;
	if( len != 0.0f ) len = 1.0f / sqrt( len );
	x *= len;
	y *= len;
	z *= len;

	angle = DEG2RAD( angle );
	c = com.cos( angle );
	s = com.sin( angle );
	
	out[0][0] = x * x + c * (1 - x * x);
	out[0][1] = x * y * (1 - c) + z * s;
	out[0][2] = x * z * (1 - c) - y * s;
	out[1][0] = x * y * (1 - c) - z * s;
	out[1][1] = y * y + c * (1 - y * y);
	out[1][2] = y * z * (1 - c) + x * s;
	out[2][0] = x * z * (1 - c) + y * s;
	out[2][1] = y * z * (1 - c) - x * s;
	out[2][2] = z * z + c * (1 - z * z);
}

_inline void Matrix3x3_Concat( matrix3x3 out, const matrix3x3 in1, const matrix3x3 in2 )
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

_inline void Matrix3x3_FromPoints( const vec3_t v1, const vec3_t v2, const vec3_t v3, matrix3x3 out )
{
	float	d;

	out[2][0] = (v1[1] - v2[1]) * (v3[2] - v2[2]) - (v1[2] - v2[2]) * (v3[1] - v2[1]);
	out[2][1] = (v1[2] - v2[2]) * (v3[0] - v2[0]) - (v1[0] - v2[0]) * (v3[2] - v2[2]);
	out[2][2] = (v1[0] - v2[0]) * (v3[1] - v2[1]) - (v1[1] - v2[1]) * (v3[0] - v2[0]);
	d = rsqrt( out[2][0] * out[2][0] + out[2][1] * out[2][1] + out[2][2] * out[2][2]);

	out[2][0] *= d;
	out[2][1] *= d;
	out[2][2] *= d;

	// this rotate and negate guarantees a vector not colinear with the original
	out[1][0] = out[2][2];
	out[1][1] = -out[2][0];
	out[1][2] = out[2][1];
	d = -( out[1][0] * out[2][0] + out[1][1] * out[2][1] + out[1][2] * out[2][2] );

	out[1][0] = out[1][0] + d * out[2][0];
	out[1][1] = out[1][1] + d * out[2][1];
	out[1][2] = out[1][2] + d * out[2][2];
	d = rsqrt( out[1][0] * out[1][0] + out[1][1] * out[1][1] + out[1][2] * out[1][2]);

	out[1][0] *= d;
	out[1][1] *= d;
	out[1][2] *= d;
	out[0][0] = out[1][1] * out[2][2] - out[1][2] * out[2][1];
	out[0][1] = out[1][2] * out[2][0] - out[1][0] * out[2][2];
	out[0][2] = out[1][0] * out[2][1] - out[1][1] * out[2][0];
}


// FIXME: optimize
_inline void Matrix3x3_ConcatRotate( matrix3x3 out, float angle, float x, float y, float z )
{
	matrix3x3 base, temp;

	Matrix3x3_Copy( base, out );
	Matrix3x3_CreateRotate( temp, angle, x, y, z );
	Matrix3x3_Concat( out, base, temp );
}

/*
========================================================================

		Matrix4x4 operations

========================================================================
*/
#define Matrix4x4_LoadIdentity( mat )	Matrix4x4_Copy( mat, matrix4x4_identity )

static const matrix4x4 matrix4x4_identity =
{
{ 1, 0, 0, 0 },	// PITCH
{ 0, 1, 0, 0 },	// YAW
{ 0, 0, 1, 0 },	// ROLL
{ 0, 0, 0, 1 },	// ORIGIN
};

_inline void Matrix4x4_BuildLightIdentity( matrix4x4 out )
{
	out[0][0] = out[1][1] = out[2][2] = 0.5f;
	out[3][3] = 1.0f;

	out[1][0] = out[2][0] = out[2][1] = 0.0f;
	out[0][1] = out[0][2] = out[1][2] = 0.0f;

#ifdef OPENGL_STYLE
	out[3][0] = 0.5f;
	out[3][1] = 0.5f;
	out[3][2] = 0.5f;
	out[0][3] = 0;
	out[1][3] = 0;
	out[2][3] = 0;
#else
	out[0][3] = 0.5f;
	out[1][3] = 0.5f;
	out[2][3] = 0.5f;
	out[3][0] = 0;
	out[3][1] = 0;
	out[3][2] = 0;
#endif
}

_inline void Matrix4x4_Copy( matrix4x4 out, const matrix4x4 in )
{
	// FIXME: replace with Mem_Copy
	memcpy( out, in, sizeof( matrix4x4 ));
}

_inline void Matrix4x4_TransformPoint( const matrix4x4 in, vec3_t point )
{
	float	out1, out2, out3;
#ifdef OPENGL_STYLE
	out1 =  in[0][0] * point[0];
	out2 =  in[0][1] * point[0];
	out3 =  in[0][2] * point[0];
	out1 += in[1][0] * point[1];
	out2 += in[1][1] * point[1];
	out3 += in[1][2] * point[1];
	out1 += in[2][0] * point[2];
	out2 += in[2][1] * point[2];
	out3 += in[2][2] * point[2];
	out1 += in[3][0];
	out2 += in[3][1];
	out3 += in[3][2];
#else
	out1 =  in[0][0] * point[0];
	out2 =  in[1][0] * point[0];
	out3 =  in[2][0] * point[0];
	out1 += in[0][1] * point[1];
	out2 += in[1][1] * point[1];
	out3 += in[2][1] * point[1];
	out1 += in[0][2] * point[2];
	out2 += in[1][2] * point[2];
	out3 += in[2][2] * point[2];
	out1 += in[0][3];
	out2 += in[1][3];
	out3 += in[2][3];
#endif
	point[0] = out1;
	point[1] = out2;
	point[2] = out3;
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

// same as Matrix4x4_Transform3x3 but transpose matrix before
_inline void Matrix4x4_Rotate3x3( const matrix4x4 in, const float v[3], float out[3] )
{
#ifdef OPENGL_STYLE
	out[0] = v[0] * in[0][0] + v[1] * in[0][1] + v[2] * in[0][2];
	out[1] = v[0] * in[1][0] + v[1] * in[1][1] + v[2] * in[1][2];
	out[2] = v[0] * in[2][0] + v[1] * in[2][1] + v[2] * in[2][2];
#else
	out[0] = v[0] * in[0][0] + v[1] * in[1][0] + v[2] * in[2][0];
	out[1] = v[0] * in[0][1] + v[1] * in[1][1] + v[2] * in[2][1];
	out[2] = v[0] * in[0][2] + v[1] * in[1][2] + v[2] * in[2][2];
#endif
}

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

_inline void Matrix4x4_ConcatVector( const matrix4x4 in, const float v[4], float out[4] )
{
#ifdef OPENGL_STYLE
	out[0] = in[0][0] * v[0] + in[1][0] * v[1] + in[2][0] * v[2] + in[3][0] * v[3];
	out[1] = in[0][1] * v[0] + in[1][1] * v[1] + in[2][1] * v[2] + in[3][1] * v[3];
	out[2] = in[0][2] * v[0] + in[1][2] * v[1] + in[2][2] * v[2] + in[3][2] * v[3];
	out[3] = in[0][3] * v[0] + in[1][3] * v[1] + in[2][3] * v[2] + in[3][3] * v[3];
#else
	out[0] = in[0][0] * v[0] + in[0][1] * v[1] + in[0][2] * v[2] + in[0][3] * v[3];
	out[1] = in[1][0] * v[0] + in[1][1] * v[1] + in[1][2] * v[2] + in[1][3] * v[3];
	out[2] = in[2][0] * v[0] + in[2][1] * v[1] + in[2][2] * v[2] + in[2][3] * v[3];
	out[3] = in[3][0] * v[0] + in[3][1] * v[1] + in[3][2] * v[2] + in[3][3] * v[3];
#endif
}

_inline void Matrix4x4_Invert_Simple( matrix4x4 out, const matrix4x4 in1 )
{
	// we only support uniform scaling, so assume the first row is enough
	// (note the lack of sqrt here, because we're trying to undo the scaling,
	// this means multiplying by the inverse scale twice - squaring it, which
	// makes the sqrt a waste of time)
	float scale = 1.0 / (in1[0][0] * in1[0][0] + in1[0][1] * in1[0][1] + in1[0][2] * in1[0][2]);

	// invert the rotation by transposing and multiplying by the squared
	// recipricol of the input matrix scale as described above
	out[0][0] = in1[0][0] * scale;
	out[0][1] = in1[1][0] * scale;
	out[0][2] = in1[2][0] * scale;
	out[1][0] = in1[0][1] * scale;
	out[1][1] = in1[1][1] * scale;
	out[1][2] = in1[2][1] * scale;
	out[2][0] = in1[0][2] * scale;
	out[2][1] = in1[1][2] * scale;
	out[2][2] = in1[2][2] * scale;

#ifdef OPENGL_STYLE
	// invert the translate
	out[3][0] = -(in1[3][0] * out[0][0] + in1[3][1] * out[1][0] + in1[3][2] * out[2][0]);
	out[3][1] = -(in1[3][0] * out[0][1] + in1[3][1] * out[1][1] + in1[3][2] * out[2][1]);
	out[3][2] = -(in1[3][0] * out[0][2] + in1[3][1] * out[1][2] + in1[3][2] * out[2][2]);

	// don't know if there's anything worth doing here
	out[0][3] = 0;
	out[1][3] = 0;
	out[2][3] = 0;
	out[3][3] = 1;
#else
	// invert the translate
	out[0][3] = -(in1[0][3] * out[0][0] + in1[1][3] * out[0][1] + in1[2][3] * out[0][2]);
	out[1][3] = -(in1[0][3] * out[1][0] + in1[1][3] * out[1][1] + in1[2][3] * out[1][2]);
	out[2][3] = -(in1[0][3] * out[2][0] + in1[1][3] * out[2][1] + in1[2][3] * out[2][2]);

	// don't know if there's anything worth doing here
	out[3][0] = 0;
	out[3][1] = 0;
	out[3][2] = 0;
	out[3][3] = 1;
#endif
}

_inline void Matrix4x4_CreateTranslate( matrix4x4 out, float x, float y, float z )
{
#ifdef OPENGL_STYLE
	out[0][0] = 1.0f;
	out[1][0] = 0.0f;
	out[2][0] = 0.0f;
	out[3][0] = x;
	out[0][1] = 0.0f;
	out[1][1] = 1.0f;
	out[2][1] = 0.0f;
	out[3][1] = y;
	out[0][2] = 0.0f;
	out[1][2] = 0.0f;
	out[2][2] = 1.0f;
	out[3][2] = z;
	out[0][3] = 0.0f;
	out[1][3] = 0.0f;
	out[2][3] = 0.0f;
	out[3][3] = 1.0f;
#else
	out[0][0] = 1.0f;
	out[0][1] = 0.0f;
	out[0][2] = 0.0f;
	out[0][3] = x;
	out[1][0] = 0.0f;
	out[1][1] = 1.0f;
	out[1][2] = 0.0f;
	out[1][3] = y;
	out[2][0] = 0.0f;
	out[2][1] = 0.0f;
	out[2][2] = 1.0f;
	out[2][3] = z;
	out[3][0] = 0.0f;
	out[3][1] = 0.0f;
	out[3][2] = 0.0f;
	out[3][3] = 1.0f;
#endif
}

_inline void Matrix4x4_CreateRotate( matrix4x4 out, float angle, float x, float y, float z )
{
	float len, c, s;

	len = x * x + y * y + z * z;
	if( len != 0.0f ) len = 1.0f / com.sqrt( len );
	x *= len;
	y *= len;
	z *= len;

	angle *= (-M_PI / 180.0);
	c = com.cos( angle );
	s = com.sin( angle );

#ifdef OPENGL_STYLE
	out[0][0]=x * x + c * (1 - x * x);
	out[1][0]=x * y * (1 - c) + z * s;
	out[2][0]=z * x * (1 - c) - y * s;
	out[3][0]=0.0f;
	out[0][1]=x * y * (1 - c) - z * s;
	out[1][1]=y * y + c * (1 - y * y);
	out[2][1]=y * z * (1 - c) + x * s;
	out[3][1]=0.0f;
	out[0][2]=z * x * (1 - c) + y * s;
	out[1][2]=y * z * (1 - c) - x * s;
	out[2][2]=z * z + c * (1 - z * z);
	out[3][2]=0.0f;
	out[0][3]=0.0f;
	out[1][3]=0.0f;
	out[2][3]=0.0f;
	out[3][3]=1.0f;
#else
	out[0][0]=x * x + c * (1 - x * x);
	out[0][1]=x * y * (1 - c) + z * s;
	out[0][2]=z * x * (1 - c) - y * s;
	out[0][3]=0.0f;
	out[1][0]=x * y * (1 - c) - z * s;
	out[1][1]=y * y + c * (1 - y * y);
	out[1][2]=y * z * (1 - c) + x * s;
	out[1][3]=0.0f;
	out[2][0]=z * x * (1 - c) + y * s;
	out[2][1]=y * z * (1 - c) - x * s;
	out[2][2]=z * z + c * (1 - z * z);
	out[2][3]=0.0f;
	out[3][0]=0.0f;
	out[3][1]=0.0f;
	out[3][2]=0.0f;
	out[3][3]=1.0f;
#endif
}

_inline void Matrix4x4_CreateScale( matrix4x4 out, float x )
{
	out[0][0] = x;
	out[0][1] = 0.0f;
	out[0][2] = 0.0f;
	out[0][3] = 0.0f;
	out[1][0] = 0.0f;
	out[1][1] = x;
	out[1][2] = 0.0f;
	out[1][3] = 0.0f;
	out[2][0] = 0.0f;
	out[2][1] = 0.0f;
	out[2][2] = x;
	out[2][3] = 0.0f;
	out[3][0] = 0.0f;
	out[3][1] = 0.0f;
	out[3][2] = 0.0f;
	out[3][3] = 1.0f;
}

_inline void Matrix4x4_CreateScale3( matrix4x4 out, float x, float y, float z )
{
	out[0][0] = x;
	out[0][1] = 0.0f;
	out[0][2] = 0.0f;
	out[0][3] = 0.0f;
	out[1][0] = 0.0f;
	out[1][1] = y;
	out[1][2] = 0.0f;
	out[1][3] = 0.0f;
	out[2][0] = 0.0f;
	out[2][1] = 0.0f;
	out[2][2] = z;
	out[2][3] = 0.0f;
	out[3][0] = 0.0f;
	out[3][1] = 0.0f;
	out[3][2] = 0.0f;
	out[3][3] = 1.0f;
}

_inline void Matrix4x4_CreateFromEntity( matrix4x4 out, float x, float y, float z, float pitch, float yaw, float roll, float scale )
{
	float angle, sr, sp, sy, cr, cp, cy;

	if( roll )
	{
		angle = yaw * (M_PI*2 / 360);
		sy = com.sin( angle );
		cy = com.cos( angle );
		angle = pitch * (M_PI*2 / 360);
		sp = com.sin( angle );
		cp = com.cos( angle );
		angle = roll * (M_PI*2 / 360);
		sr = com.sin( angle );
		cr = com.cos( angle );
#ifdef OPENGL_STYLE
		out[0][0] = (cp*cy) * scale;
		out[1][0] = (sr*sp*cy+cr*-sy) * scale;
		out[2][0] = (cr*sp*cy+-sr*-sy) * scale;
		out[3][0] = x;
		out[0][1] = (cp*sy) * scale;
		out[1][1] = (sr*sp*sy+cr*cy) * scale;
		out[2][1] = (cr*sp*sy+-sr*cy) * scale;
		out[3][1] = y;
		out[0][2] = (-sp) * scale;
		out[1][2] = (sr*cp) * scale;
		out[2][2] = (cr*cp) * scale;
		out[3][2] = z;
		out[0][3] = 0;
		out[1][3] = 0;
		out[2][3] = 0;
		out[3][3] = 1;
#else
		out[0][0] = (cp*cy) * scale;
		out[0][1] = (sr*sp*cy+cr*-sy) * scale;
		out[0][2] = (cr*sp*cy+-sr*-sy) * scale;
		out[0][3] = x;
		out[1][0] = (cp*sy) * scale;
		out[1][1] = (sr*sp*sy+cr*cy) * scale;
		out[1][2] = (cr*sp*sy+-sr*cy) * scale;
		out[1][3] = y;
		out[2][0] = (-sp) * scale;
		out[2][1] = (sr*cp) * scale;
		out[2][2] = (cr*cp) * scale;
		out[2][3] = z;
		out[3][0] = 0;
		out[3][1] = 0;
		out[3][2] = 0;
		out[3][3] = 1;
#endif
	}
	else if( pitch )
	{
		angle = yaw * (M_PI*2 / 360);
		sy = com.sin( angle );
		cy = com.cos( angle );
		angle = pitch * (M_PI*2 / 360);
		sp = com.sin( angle );
		cp = com.cos( angle );
#ifdef OPENGL_STYLE
		out[0][0] = (cp*cy) * scale;
		out[1][0] = (-sy) * scale;
		out[2][0] = (sp*cy) * scale;
		out[3][0] = x;
		out[0][1] = (cp*sy) * scale;
		out[1][1] = (cy) * scale;
		out[2][1] = (sp*sy) * scale;
		out[3][1] = y;
		out[0][2] = (-sp) * scale;
		out[1][2] = 0;
		out[2][2] = (cp) * scale;
		out[3][2] = z;
		out[0][3] = 0;
		out[1][3] = 0;
		out[2][3] = 0;
		out[3][3] = 1;
#else
		out[0][0] = (cp*cy) * scale;
		out[0][1] = (-sy) * scale;
		out[0][2] = (sp*cy) * scale;
		out[0][3] = x;
		out[1][0] = (cp*sy) * scale;
		out[1][1] = (cy) * scale;
		out[1][2] = (sp*sy) * scale;
		out[1][3] = y;
		out[2][0] = (-sp) * scale;
		out[2][1] = 0;
		out[2][2] = (cp) * scale;
		out[2][3] = z;
		out[3][0] = 0;
		out[3][1] = 0;
		out[3][2] = 0;
		out[3][3] = 1;
#endif
	}
	else if( yaw )
	{
		angle = yaw * (M_PI*2 / 360);
		sy = com.sin( angle );
		cy = com.cos( angle );
#ifdef OPENGL_STYLE
		out[0][0] = (cy) * scale;
		out[1][0] = (-sy) * scale;
		out[2][0] = 0;
		out[3][0] = x;
		out[0][1] = (sy) * scale;
		out[1][1] = (cy) * scale;
		out[2][1] = 0;
		out[3][1] = y;
		out[0][2] = 0;
		out[1][2] = 0;
		out[2][2] = scale;
		out[3][2] = z;
		out[0][3] = 0;
		out[1][3] = 0;
		out[2][3] = 0;
		out[3][3] = 1;
#else
		out[0][0] = (cy) * scale;
		out[0][1] = (-sy) * scale;
		out[0][2] = 0;
		out[0][3] = x;
		out[1][0] = (sy) * scale;
		out[1][1] = (cy) * scale;
		out[1][2] = 0;
		out[1][3] = y;
		out[2][0] = 0;
		out[2][1] = 0;
		out[2][2] = scale;
		out[2][3] = z;
		out[3][0] = 0;
		out[3][1] = 0;
		out[3][2] = 0;
		out[3][3] = 1;
#endif
	}
	else
	{
#ifdef OPENGL_STYLE
		out[0][0] = scale;
		out[1][0] = 0;
		out[2][0] = 0;
		out[3][0] = x;
		out[0][1] = 0;
		out[1][1] = scale;
		out[2][1] = 0;
		out[3][1] = y;
		out[0][2] = 0;
		out[1][2] = 0;
		out[2][2] = scale;
		out[3][2] = z;
		out[0][3] = 0;
		out[1][3] = 0;
		out[2][3] = 0;
		out[3][3] = 1;
#else
		out[0][0] = scale;
		out[0][1] = 0;
		out[0][2] = 0;
		out[0][3] = x;
		out[1][0] = 0;
		out[1][1] = scale;
		out[1][2] = 0;
		out[1][3] = y;
		out[2][0] = 0;
		out[2][1] = 0;
		out[2][2] = scale;
		out[2][3] = z;
		out[3][0] = 0;
		out[3][1] = 0;
		out[3][2] = 0;
		out[3][3] = 1;
#endif
	}
}

_inline void Matrix4x4_FromOriginQuat( matrix4x4 out, float ox, float oy, float oz, float x, float y, float z, float w )
{
#ifdef OPENGL_STYLE
	out[0][0] = 1-2*(y*y+z*z);
	out[1][0] = 2*(x*y-z*w);
	out[2][0] = 2*(x*z+y*w);
	out[3][0] = ox;
	out[0][1] = 2*(x*y+z*w);
	out[1][1] = 1-2*(x*x+z*z);
	out[2][1] = 2*(y*z-x*w);
	out[3][1] = oy;
	out[0][2] = 2*(x*z-y*w);
	out[1][2] = 2*(y*z+x*w);
	out[2][2] = 1-2*(x*x+y*y);
	out[3][2] = oz;
	out[0][3] = 0;
	out[1][3] = 0;
	out[2][3] = 0;
	out[3][3] = 1;
#else
	out[0][0] = 1-2*(y*y+z*z);
	out[0][1] = 2*(x*y-z*w);
	out[0][2] = 2*(x*z+y*w);
	out[0][3] = ox;
	out[1][0] = 2*(x*y+z*w);
	out[1][1] = 1-2*(x*x+z*z);
	out[1][2] = 2*(y*z-x*w);
	out[1][3] = oy;
	out[2][0] = 2*(x*z-y*w);
	out[2][1] = 2*(y*z+x*w);
	out[2][2] = 1-2*(x*x+y*y);
	out[2][3] = oz;
	out[3][0] = 0;
	out[3][1] = 0;
	out[3][2] = 0;
	out[3][3] = 1;
#endif
}

/*
================
ConcatTransforms
================
*/
_inline void Matrix4x4_ConcatTransforms( matrix4x4 out, const matrix4x4 in1, const matrix4x4 in2 )
{
#ifdef OPENGL_STYLE
	out[0][0] = in1[0][0] * in2[0][0] + in1[1][0] * in2[0][1] + in1[2][0] * in2[0][2];
	out[1][0] = in1[0][0] * in2[1][0] + in1[1][0] * in2[1][1] + in1[2][0] * in2[1][2];
	out[2][0] = in1[0][0] * in2[2][0] + in1[1][0] * in2[2][1] + in1[2][0] * in2[2][2];
	out[3][0] = in1[0][0] * in2[3][0] + in1[1][0] * in2[3][1] + in1[2][0] * in2[3][2] + in1[3][0];
	out[0][1] = in1[0][1] * in2[0][0] + in1[1][1] * in2[0][1] + in1[2][1] * in2[0][2];
	out[1][1] = in1[0][1] * in2[1][0] + in1[1][1] * in2[1][1] + in1[2][1] * in2[1][2];
	out[2][1] = in1[0][1] * in2[2][0] + in1[1][1] * in2[2][1] + in1[2][1] * in2[2][2];
	out[3][1] = in1[0][1] * in2[3][0] + in1[1][1] * in2[3][1] + in1[2][1] * in2[3][2] + in1[3][1];
	out[0][2] = in1[0][2] * in2[0][0] + in1[1][2] * in2[0][1] + in1[2][2] * in2[0][2];
	out[1][2] = in1[0][2] * in2[1][0] + in1[1][2] * in2[1][1] + in1[2][2] * in2[1][2];
	out[2][2] = in1[0][2] * in2[2][0] + in1[1][2] * in2[2][1] + in1[2][2] * in2[2][2];
	out[3][2] = in1[0][2] * in2[3][0] + in1[1][2] * in2[3][1] + in1[2][2] * in2[3][2] + in1[3][2];
#else
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
#endif
}

_inline void Matrix4x4_Concat( matrix4x4 out, const matrix4x4 in1, const matrix4x4 in2 )
{
#ifdef OPENGL_STYLE
	out[0][0] = in1[0][0] * in2[0][0] + in1[1][0] * in2[0][1] + in1[2][0] * in2[0][2] + in1[3][0] * in2[0][3];
	out[1][0] = in1[0][0] * in2[1][0] + in1[1][0] * in2[1][1] + in1[2][0] * in2[1][2] + in1[3][0] * in2[1][3];
	out[2][0] = in1[0][0] * in2[2][0] + in1[1][0] * in2[2][1] + in1[2][0] * in2[2][2] + in1[3][0] * in2[2][3];
	out[3][0] = in1[0][0] * in2[3][0] + in1[1][0] * in2[3][1] + in1[2][0] * in2[3][2] + in1[3][0] * in2[3][3];
	out[0][1] = in1[0][1] * in2[0][0] + in1[1][1] * in2[0][1] + in1[2][1] * in2[0][2] + in1[3][1] * in2[0][3];
	out[1][1] = in1[0][1] * in2[1][0] + in1[1][1] * in2[1][1] + in1[2][1] * in2[1][2] + in1[3][1] * in2[1][3];
	out[2][1] = in1[0][1] * in2[2][0] + in1[1][1] * in2[2][1] + in1[2][1] * in2[2][2] + in1[3][1] * in2[2][3];
	out[3][1] = in1[0][1] * in2[3][0] + in1[1][1] * in2[3][1] + in1[2][1] * in2[3][2] + in1[3][1] * in2[3][3];
	out[0][2] = in1[0][2] * in2[0][0] + in1[1][2] * in2[0][1] + in1[2][2] * in2[0][2] + in1[3][2] * in2[0][3];
	out[1][2] = in1[0][2] * in2[1][0] + in1[1][2] * in2[1][1] + in1[2][2] * in2[1][2] + in1[3][2] * in2[1][3];
	out[2][2] = in1[0][2] * in2[2][0] + in1[1][2] * in2[2][1] + in1[2][2] * in2[2][2] + in1[3][2] * in2[2][3];
	out[3][2] = in1[0][2] * in2[3][0] + in1[1][2] * in2[3][1] + in1[2][2] * in2[3][2] + in1[3][2] * in2[3][3];
	out[0][3] = in1[0][3] * in2[0][0] + in1[1][3] * in2[0][1] + in1[2][3] * in2[0][2] + in1[3][3] * in2[0][3];
	out[1][3] = in1[0][3] * in2[1][0] + in1[1][3] * in2[1][1] + in1[2][3] * in2[1][2] + in1[3][3] * in2[1][3];
	out[2][3] = in1[0][3] * in2[2][0] + in1[1][3] * in2[2][1] + in1[2][3] * in2[2][2] + in1[3][3] * in2[2][3];
	out[3][3] = in1[0][3] * in2[3][0] + in1[1][3] * in2[3][1] + in1[2][3] * in2[3][2] + in1[3][3] * in2[3][3];
#else
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0] + in1[0][3] * in2[3][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1] + in1[0][3] * in2[3][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2] + in1[0][3] * in2[3][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] + in1[0][2] * in2[2][3] + in1[0][3] * in2[3][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0] + in1[1][3] * in2[3][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1] + in1[1][3] * in2[3][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2] + in1[1][3] * in2[3][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] + in1[1][2] * in2[2][3] + in1[1][3] * in2[3][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0] + in1[2][3] * in2[3][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1] + in1[2][3] * in2[3][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2] + in1[2][3] * in2[3][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] + in1[2][2] * in2[2][3] + in1[2][3] * in2[3][3];
	out[3][0] = in1[3][0] * in2[0][0] + in1[3][1] * in2[1][0] + in1[3][2] * in2[2][0] + in1[3][3] * in2[3][0];
	out[3][1] = in1[3][0] * in2[0][1] + in1[3][1] * in2[1][1] + in1[3][2] * in2[2][1] + in1[3][3] * in2[3][1];
	out[3][2] = in1[3][0] * in2[0][2] + in1[3][1] * in2[1][2] + in1[3][2] * in2[2][2] + in1[3][3] * in2[3][2];
	out[3][3] = in1[3][0] * in2[0][3] + in1[3][1] * in2[1][3] + in1[3][2] * in2[2][3] + in1[3][3] * in2[3][3];
#endif
}

_inline bool Matrix4x4_CompareRotateOnly( const matrix4x4 mat1, const matrix4x4 mat2 )
{
#ifdef OPENGL_STYLE
	if( mat1[0][0] != mat2[0][0] || mat1[0][1] != mat2[0][1] || mat1[0][2] != mat2[0][2] )
		return false;
	if( mat1[1][0] != mat2[1][0] || mat1[1][1] != mat2[1][1] || mat1[1][2] != mat2[1][2] )
		return false;
	if( mat1[2][0] != mat2[2][0] || mat1[2][1] != mat2[2][1] || mat1[2][2] != mat2[2][2] )
		return false;
#else
	if( mat1[0][0] != mat2[0][0] || mat1[1][0] != mat2[1][0] || mat1[2][0] != mat2[2][0] )
		return false;
	if( mat1[0][1] != mat2[0][1] || mat1[1][1] != mat2[1][1] || mat1[2][1] != mat2[2][1] )
		return false;
	if( mat1[0][2] != mat2[0][2] || mat1[1][2] != mat2[1][2] || mat1[2][2] != mat2[2][2] )
		return false;
#endif
	return true;
}

_inline bool Matrix4x4_Compare( const matrix4x4 mat1, const matrix4x4 mat2 )
{
#ifdef OPENGL_STYLE
	if( mat1[0][0] != mat2[0][0] || mat1[0][1] != mat2[0][1] || mat1[0][2] != mat2[0][2] )
		return false;
	if( mat1[1][0] != mat2[1][0] || mat1[1][1] != mat2[1][1] || mat1[1][2] != mat2[1][2] )
		return false;
	if( mat1[2][0] != mat2[2][0] || mat1[2][1] != mat2[2][1] || mat1[2][2] != mat2[2][2] )
		return false;
	if( mat1[3][0] != mat2[3][0] || mat1[3][1] != mat2[3][1] || mat1[3][2] != mat2[3][2] )
		return false;
#else
	if( mat1[0][0] != mat2[0][0] || mat1[1][0] != mat2[1][0] || mat1[2][0] != mat2[2][0] )
		return false;
	if( mat1[0][1] != mat2[0][1] || mat1[1][1] != mat2[1][1] || mat1[2][1] != mat2[2][1] )
		return false;
	if( mat1[0][2] != mat2[0][2] || mat1[1][2] != mat2[1][2] || mat1[2][2] != mat2[2][2] )
		return false;
	if( mat1[0][3] != mat2[0][3] || mat1[1][3] != mat2[1][3] || mat1[2][3] != mat2[2][3] )
		return false;
#endif
	return true;
}

_inline void Matrix4x4_FromVectors( matrix4x4 out, const float vx[3], const float vy[3], const float vz[3], const float t[3])
{
#ifdef OPENGL_STYLE
	out[0][0] = vx[0];
	out[1][0] = vy[0];
	out[2][0] = vz[0];
	out[3][0] = t[0];
	out[0][1] = vx[1];
	out[1][1] = vy[1];
	out[2][1] = vz[1];
	out[3][1] = t[1];
	out[0][2] = vx[2];
	out[1][2] = vy[2];
	out[2][2] = vz[2];
	out[3][2] = t[2];
	out[0][3] = 0.0f;
	out[1][3] = 0.0f;
	out[2][3] = 0.0f;
	out[3][3] = 1.0f;
#else
	out[0][0] = vx[0];
	out[0][1] = vy[0];
	out[0][2] = vz[0];
	out[0][3] = t[0];
	out[1][0] = vx[1];
	out[1][1] = vy[1];
	out[1][2] = vz[1];
	out[1][3] = t[1];
	out[2][0] = vx[2];
	out[2][1] = vy[2];
	out[2][2] = vz[2];
	out[2][3] = t[2];
	out[3][0] = 0.0f;
	out[3][1] = 0.0f;
	out[3][2] = 0.0f;
	out[3][3] = 1.0f;
#endif
}

_inline void Matrix4x4_FromMatrix3x3( matrix4x4 out, const matrix3x3 in, const float scale )
{
#ifdef OPENGL_STYLE
	out[0][0] = in[0][0] * scale;
	out[1][0] = in[1][0] * scale;
	out[2][0] = in[2][0] * scale;
	out[3][0] = 0.0f;
	out[0][1] = in[0][1] * scale;
	out[1][1] = in[1][1] * scale;
	out[2][1] = in[2][1] * scale;
	out[3][1] = 0.0f;
	out[0][2] = in[0][2] * scale;
	out[1][2] = in[1][2] * scale;
	out[2][2] = in[2][2] * scale;
	out[3][2] = 0.0f;
	out[0][3] = 0.0f;
	out[1][3] = 0.0f;
	out[2][3] = 0.0f;
	out[3][3] = 1.0f;
#else
	out[0][0] = in[0][0] * scale;
	out[0][1] = in[1][0] * scale;
	out[0][2] = in[2][0] * scale;
	out[0][3] = 0.0f;
	out[1][0] = in[0][1] * scale;
	out[1][1] = in[1][1] * scale;
	out[1][2] = in[2][1] * scale;
	out[1][3] = 0.0f;
	out[2][0] = in[0][2] * scale;
	out[2][1] = in[1][2] * scale;
	out[2][2] = in[2][2] * scale;
	out[2][3] = 0.0f;
	out[3][0] = 0.0f;
	out[3][1] = 0.0f;
	out[3][2] = 0.0f;
	out[3][3] = 1.0f;
#endif
}

/*
================
Matrix4x4_CreateProjection

NOTE: produce quake style world orientation
================
*/
_inline void Matrix4x4_CreateProjection( matrix4x4 out, float xMax, float xMin, float yMax, float yMin, float zNear, float zFar )
{
	out[0][0] = ( 2.0f * zNear ) / ( xMax - xMin );
	out[1][1] = ( 2.0f * zNear ) / ( yMax - yMin );
	out[2][2] = -( zFar + zNear ) / ( zFar - zNear );
	out[3][3] = out[0][1] = out[1][0] = out[3][0] = out[0][3] = out[3][1] = out[1][3] = 0.0f;

#ifdef OPENGL_STYLE
	out[0][2] = 0.0f;
	out[1][2] = 0.0f;
	out[2][0] = ( xMax + xMin ) / ( xMax - xMin );
	out[2][1] = ( yMax + yMin ) / ( yMax - yMin );
	out[2][3] = -1.0f;
	out[3][2] = -( 2.0f * zFar * zNear ) / ( zFar - zNear );
#else
	out[2][0] = 0.0f;
	out[2][1] = 0.0f;
	out[0][2] = ( xMax + xMin ) / ( xMax - xMin );
	out[1][2] = ( yMax + yMin ) / ( yMax - yMin );
	out[3][2] = -1.0f;
	out[2][3] = -( 2.0f * zFar * zNear ) / ( zFar - zNear );
#endif
}

/*
================
Matrix4x4_CreateModelview

NOTE: produce quake style world orientation
================
*/
_inline void Matrix4x4_CreateModelview( matrix4x4 out )
{
	out[0][0] = out[1][1] = out[2][2] = 0.0f;
	out[3][0] = out[0][3] = 0.0f;
	out[3][1] = out[1][3] = 0.0f;
	out[3][2] = out[2][3] = 0.0f;
	out[3][3] = 1.0f;
#ifdef OPENGL_STYLE
	out[0][1] = out[2][0] = out[1][2] = 0.0f;
	out[0][2] = out[1][0] = -1.0f;
	out[2][1] = 1;
#else
	out[1][0] = out[0][2] = out[2][1] = 0.0f;
	out[2][0] = out[0][1] = -1.0f;
	out[1][2] = 1.0f;
#endif
}

_inline void Matrix4x4_ToArrayFloatGL( const matrix4x4 in, float out[16] )
{
#ifdef OPENGL_STYLE
	out[ 0] = in[0][0];
	out[ 1] = in[0][1];
	out[ 2] = in[0][2];
	out[ 3] = in[0][3];
	out[ 4] = in[1][0];
	out[ 5] = in[1][1];
	out[ 6] = in[1][2];
	out[ 7] = in[1][3];
	out[ 8] = in[2][0];
	out[ 9] = in[2][1];
	out[10] = in[2][2];
	out[11] = in[2][3];
	out[12] = in[3][0];
	out[13] = in[3][1];
	out[14] = in[3][2];
	out[15] = in[3][3];
#else
	out[ 0] = in[0][0];
	out[ 1] = in[1][0];
	out[ 2] = in[2][0];
	out[ 3] = in[3][0];
	out[ 4] = in[0][1];
	out[ 5] = in[1][1];
	out[ 6] = in[2][1];
	out[ 7] = in[3][1];
	out[ 8] = in[0][2];
	out[ 9] = in[1][2];
	out[10] = in[2][2];
	out[11] = in[3][2];
	out[12] = in[0][3];
	out[13] = in[1][3];
	out[14] = in[2][3];
	out[15] = in[3][3];
#endif
}

_inline void Matrix4x4_FromArrayFloatGL( matrix4x4 out, const float in[16] )
{
#ifdef OPENGL_STYLE
	out[0][0] = in[0];
	out[0][1] = in[1];
	out[0][2] = in[2];
	out[0][3] = in[3];
	out[1][0] = in[4];
	out[1][1] = in[5];
	out[1][2] = in[6];
	out[1][3] = in[7];
	out[2][0] = in[8];
	out[2][1] = in[9];
	out[2][2] = in[10];
	out[2][3] = in[11];
	out[3][0] = in[12];
	out[3][1] = in[13];
	out[3][2] = in[14];
	out[3][3] = in[15];
#else
	out[0][0] = in[0];
	out[1][0] = in[1];
	out[2][0] = in[2];
	out[3][0] = in[3];
	out[0][1] = in[4];
	out[1][1] = in[5];
	out[2][1] = in[6];
	out[3][1] = in[7];
	out[0][2] = in[8];
	out[1][2] = in[9];
	out[2][2] = in[10];
	out[3][2] = in[11];
	out[0][3] = in[12];
	out[1][3] = in[13];
	out[2][3] = in[14];
	out[3][3] = in[15];
#endif
}

_inline void Matrix4x4_FromArrayFloatD3D( matrix4x4 out, const float in[16] )
{
#ifdef OPENGL_STYLE
	out[0][0] = in[0];
	out[1][0] = in[1];
	out[2][0] = in[2];
	out[3][0] = in[3];
	out[0][1] = in[4];
	out[1][1] = in[5];
	out[2][1] = in[6];
	out[3][1] = in[7];
	out[0][2] = in[8];
	out[1][2] = in[9];
	out[2][2] = in[10];
	out[3][2] = in[11];
	out[0][3] = in[12];
	out[1][3] = in[13];
	out[2][3] = in[14];
	out[3][3] = in[15];
#else
	out[0][0] = in[0];
	out[0][1] = in[1];
	out[0][2] = in[2];
	out[0][3] = in[3];
	out[1][0] = in[4];
	out[1][1] = in[5];
	out[1][2] = in[6];
	out[1][3] = in[7];
	out[2][0] = in[8];
	out[2][1] = in[9];
	out[2][2] = in[10];
	out[2][3] = in[11];
	out[3][0] = in[12];
	out[3][1] = in[13];
	out[3][2] = in[14];
	out[3][3] = in[15];
#endif
}

_inline void Matrix4x4_ToMatrix3x3( matrix3x3 out, const matrix4x4 in )
{
	out[0][0] = in[0][0];
	out[1][1] = in[1][1];
	out[2][2] = in[2][2];

#ifdef OPENGL_STYLE
	out[0][1] = in[1][0];
	out[0][2] = in[1][0];
	out[1][0] = in[0][1];
	out[1][2] = in[2][1];
	out[2][0] = in[0][2];
	out[2][1] = in[1][2];
#else
	out[0][1] = in[0][1];
	out[0][2] = in[0][2];
	out[1][0] = in[1][0];
	out[1][2] = in[1][2];
	out[2][0] = in[2][0];
	out[2][1] = in[2][1];
#endif
}

_inline bool Matrix4x4_Invert_Full( matrix4x4 out, const matrix4x4 in1 )
{
	float	*temp;
	float	*r[4];
	float	rtemp[4][8];
	float	m[4];
	float	s;

	r[0] = rtemp[0];
	r[1] = rtemp[1];
	r[2] = rtemp[2];
	r[3] = rtemp[3];

#ifdef OPENGL_STYLE
	r[0][0] = in1[0][0];
	r[0][1] = in1[1][0];
	r[0][2] = in1[2][0];
	r[0][3] = in1[3][0];
	r[0][4] = 1.0;
	r[0][5] = 0.0;
	r[0][6] = 0.0;
	r[0][7] = 0.0;

	r[1][0] = in1[0][1];
	r[1][1] = in1[1][1];
	r[1][2] = in1[2][1];
	r[1][3] = in1[3][1];
	r[1][5] = 1.0;
	r[1][4] =	0.0;
	r[1][6] =	0.0;
	r[1][7] = 0.0;

	r[2][0] = in1[0][2];
	r[2][1] = in1[1][2];
	r[2][2] = in1[2][2];
	r[2][3] = in1[3][2];
	r[2][6] = 1.0;
	r[2][4] =	0.0;
	r[2][5] =	0.0;
	r[2][7] = 0.0;

	r[3][0] = in1[0][3];
	r[3][1] = in1[1][3];
	r[3][2] = in1[2][3];
	r[3][3] = in1[3][3];
	r[3][4] =	0.0;
	r[3][5] =	0.0;
	r[3][6] = 0.0;
	r[3][7] = 1.0;	
#else
	r[0][0] = in1[0][0];
	r[0][1] = in1[0][1];
	r[0][2] = in1[0][2];
	r[0][3] = in1[0][3];
	r[0][4] = 1.0;
	r[0][5] =	0.0;
	r[0][6] =	0.0;
	r[0][7] = 0.0;

	r[1][0] = in1[1][0];
	r[1][1] = in1[1][1];
	r[1][2] = in1[1][2];
	r[1][3] = in1[1][3];
	r[1][5] = 1.0;
	r[1][4] =	0.0;
	r[1][6] =	0.0;
	r[1][7] = 0.0;

	r[2][0] = in1[2][0];
	r[2][1] = in1[2][1];
	r[2][2] = in1[2][2];
	r[2][3] = in1[2][3];
	r[2][6] = 1.0;
	r[2][4] =	0.0;
	r[2][5] =	0.0;
	r[2][7] = 0.0;

	r[3][0] = in1[3][0];
	r[3][1] = in1[3][1];
	r[3][2] = in1[3][2];
	r[3][3] = in1[3][3];
	r[3][4] =	0.0;
	r[3][5] = 0.0;
	r[3][6] = 0.0;
	r[3][7] = 1.0;	
#endif

	if( fabs( r[3][0] ) > fabs( r[2][0] ))
	{
		temp = r[3];
		r[3] = r[2];
		r[2] = temp;
	}
	if( fabs( r[2][0] ) > fabs( r[1][0] ))
	{
		temp = r[2];
		r[2] = r[1];
		r[1] = temp;
	}
	if( fabs( r[1][0] ) > fabs( r[0][0] ))
	{
		temp = r[1];
		r[1] = r[0];
		r[0] = temp;
	}

	if( r[0][0] )
	{
		m[1] = r[1][0] / r[0][0];
		m[2] = r[2][0] / r[0][0];
		m[3] = r[3][0] / r[0][0];

		s = r[0][1];
		r[1][1] -= m[1] * s;
		r[2][1] -= m[2] * s;
		r[3][1] -= m[3] * s;

		s = r[0][2];
		r[1][2] -= m[1] * s;
		r[2][2] -= m[2] * s;
		r[3][2] -= m[3] * s;

		s = r[0][3];
		r[1][3] -= m[1] * s;
		r[2][3] -= m[2] * s;
		r[3][3] -= m[3] * s;

		s = r[0][4];
		if( s )
		{
			r[1][4] -= m[1] * s;
			r[2][4] -= m[2] * s;
			r[3][4] -= m[3] * s;
		}

		s = r[0][5];
		if( s )
		{
			r[1][5] -= m[1] * s;
			r[2][5] -= m[2] * s;
			r[3][5] -= m[3] * s;
		}

		s = r[0][6];
		if( s )
		{
			r[1][6] -= m[1] * s;
			r[2][6] -= m[2] * s;
			r[3][6] -= m[3] * s;
		}

		s = r[0][7];
		if( s )
		{
			r[1][7] -= m[1] * s;
			r[2][7] -= m[2] * s;
			r[3][7] -= m[3] * s;
		}

		if( fabs( r[3][1] ) > fabs( r[2][1] ))
		{
			temp = r[3];
			r[3] = r[2];
			r[2] = temp;
		}
		if( fabs( r[2][1] ) > fabs( r[1][1] ))
		{
			temp = r[2];
			r[2] = r[1];
			r[1] = temp;
		}

		if( r[1][1] )
		{
			m[2] = r[2][1] / r[1][1];
			m[3] = r[3][1] / r[1][1];
			r[2][2] -= m[2] * r[1][2];
			r[3][2] -= m[3] * r[1][2];
			r[2][3] -= m[2] * r[1][3];
			r[3][3] -= m[3] * r[1][3];

			s = r[1][4];
			if( s )
			{
				r[2][4] -= m[2] * s;
				r[3][4] -= m[3] * s;
			}

			s = r[1][5];
			if( s )
			{
				r[2][5] -= m[2] * s;
				r[3][5] -= m[3] * s;
			}

			s = r[1][6];
			if( s )
			{
				r[2][6] -= m[2] * s;
				r[3][6] -= m[3] * s;
			}

			s = r[1][7];
			if( s )
			{
				r[2][7] -= m[2] * s;
				r[3][7] -= m[3] * s;
			}

			if( fabs( r[3][2] ) > fabs( r[2][2] ))
			{
				temp = r[3];
				r[3] = r[2];
				r[2] = temp;
			}

			if( r[2][2] )
			{
				m[3] = r[3][2] / r[2][2];
				r[3][3] -= m[3] * r[2][3];
				r[3][4] -= m[3] * r[2][4];
				r[3][5] -= m[3] * r[2][5];
				r[3][6] -= m[3] * r[2][6];
				r[3][7] -= m[3] * r[2][7];

				if( r[3][3] )
				{
					s = 1.0 / r[3][3];
					r[3][4] *= s;
					r[3][5] *= s;
					r[3][6] *= s;
					r[3][7] *= s;

					m[2] = r[2][3];
					s = 1.0 / r[2][2];
					r[2][4] = s * (r[2][4] - r[3][4] * m[2]);
					r[2][5] = s * (r[2][5] - r[3][5] * m[2]);
					r[2][6] = s * (r[2][6] - r[3][6] * m[2]);
					r[2][7] = s * (r[2][7] - r[3][7] * m[2]);

					m[1] = r[1][3];
					r[1][4] -= r[3][4] * m[1];
					r[1][5] -= r[3][5] * m[1];
					r[1][6] -= r[3][6] * m[1];
					r[1][7] -= r[3][7] * m[1];

					m[0] = r[0][3];
					r[0][4] -= r[3][4] * m[0];
					r[0][5] -= r[3][5] * m[0];
					r[0][6] -= r[3][6] * m[0];
					r[0][7] -= r[3][7] * m[0];

					m[1] = r[1][2];
					s = 1.0 / r[1][1];
					r[1][4] = s * (r[1][4] - r[2][4] * m[1]);
					r[1][5] = s * (r[1][5] - r[2][5] * m[1]);
					r[1][6] = s * (r[1][6] - r[2][6] * m[1]);
					r[1][7] = s * (r[1][7] - r[2][7] * m[1]);

					m[0] = r[0][2];
					r[0][4] -= r[2][4] * m[0];
					r[0][5] -= r[2][5] * m[0];
					r[0][6] -= r[2][6] * m[0];
					r[0][7] -= r[2][7] * m[0];

					m[0] = r[0][1];
					s = 1.0 / r[0][0];
					r[0][4] = s * (r[0][4] - r[1][4] * m[0]);
					r[0][5] = s * (r[0][5] - r[1][5] * m[0]);
					r[0][6] = s * (r[0][6] - r[1][6] * m[0]);
					r[0][7] = s * (r[0][7] - r[1][7] * m[0]);
#ifdef OPENGL_STYLE
					out[0][0]	= r[0][4];
					out[0][1]	= r[1][4];
					out[0][2]	= r[2][4];
					out[0][3]	= r[3][4];
					out[1][0]	= r[0][5];
					out[1][1]	= r[1][5];
					out[1][2]	= r[2][5];
					out[1][3]	= r[3][5];
					out[2][0]	= r[0][6];
					out[2][1]	= r[1][6];
					out[2][2]	= r[2][6];
					out[2][3]	= r[3][6];
					out[3][0]	= r[0][7];
					out[3][1]	= r[1][7];
					out[3][2]	= r[2][7];
					out[3][3]	= r[3][7];
#else
					out[0][0]	= r[0][4];
					out[0][1]	= r[0][5];
					out[0][2]	= r[0][6];
					out[0][3]	= r[0][7];
					out[1][0]	= r[1][4];
					out[1][1]	= r[1][5];
					out[1][2]	= r[1][6];
					out[1][3]	= r[1][7];
					out[2][0]	= r[2][4];
					out[2][1]	= r[2][5];
					out[2][2]	= r[2][6];
					out[2][3]	= r[2][7];
					out[3][0]	= r[3][4];
					out[3][1]	= r[3][5];
					out[3][2]	= r[3][6];
					out[3][3]	= r[3][7];
#endif
					return true;
				}
			}
		}
	}
	return false;
}

_inline void Matrix4x4_Transpose( matrix4x4 out, const matrix4x4 in1 )
{
	out[0][0] = in1[0][0];
	out[0][1] = in1[1][0];
	out[0][2] = in1[2][0];
	out[0][3] = in1[3][0];
	out[1][0] = in1[0][1];
	out[1][1] = in1[1][1];
	out[1][2] = in1[2][1];
	out[1][3] = in1[3][1];
	out[2][0] = in1[0][2];
	out[2][1] = in1[1][2];
	out[2][2] = in1[2][2];
	out[2][3] = in1[3][2];
	out[3][0] = in1[0][3];
	out[3][1] = in1[1][3];
	out[3][2] = in1[2][3];
	out[3][3] = in1[3][3];
}

_inline void Matrix4x4_SetOrigin( matrix4x4 out, float x, float y, float z )
{
#ifdef OPENGL_STYLE
	out[3][0] = x;
	out[3][1] = y;
	out[3][2] = z;
#else
	out[0][3] = x;
	out[1][3] = y;
	out[2][3] = z;
#endif
}

_inline void Matrix4x4_OriginFromMatrix( const matrix4x4 in, float *out )
{
#ifdef OPENGL_STYLE
	out[0] = in[3][0];
	out[1] = in[3][1];
	out[2] = in[3][2];
#else
	out[0] = in[0][3];
	out[1] = in[1][3];
	out[2] = in[2][3];
#endif
}

_inline void Matrix4x4_Setup2D( matrix4x4 out, float in1, float in2, float in3, float in4, float in5, float in6 )
{
	out[0][0] = in1;
	out[1][1] = in4;
	
#ifdef OPENGL_STYLE
	out[0][1] = in2;
	out[1][0] = in3;
	out[3][0] = in5;
	out[3][1] = in6;
#else
	out[1][0] = in2;
	out[0][1] = in3;
	out[0][3] = in5;
	out[1][3] = in6;
#endif
}

_inline void Matrix4x4_Copy2D( matrix4x4 out, const matrix4x4 in )
{
#ifdef OPENGL_STYLE
	out[0][0] = in[0][0];
	out[0][1] = in[0][1];
	out[1][0] = in[1][0];
	out[1][1] = in[1][1];
	out[3][0] = in[3][0];
	out[3][1] = in[3][1];
#else
	out[0][0] = in[0][0];
	out[1][0] = in[1][0];
	out[0][1] = in[0][1];
	out[1][1] = in[1][1];
	out[0][3] = in[0][3];
	out[1][3] = in[1][3];
#endif
}

_inline void Matrix4x4_Concat2D( matrix4x4 out, const matrix4x4 in1, const matrix4x4 in2 )
{
#ifdef OPENGL_STYLE
	out[0][0] = in1[0][0] * in2[0][0] + in1[1][0] * in2[0][1];
	out[0][1] = in1[0][1] * in2[0][0] + in1[1][1] * in2[0][1];
	out[1][0] = in1[0][0] * in2[1][0] + in1[1][0] * in2[1][1];
	out[1][1] = in1[0][1] * in2[1][0] + in1[1][1] * in2[1][1];
	out[3][0] = in1[0][0] * in2[3][0] + in1[1][0] * in2[3][1] + in1[3][0];
	out[3][1] = in1[0][1] * in2[3][0] + in1[1][1] * in2[3][1] + in1[3][1];
#else
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] + in1[0][3];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] + in1[1][3];
#endif
}

_inline void Matrix4x4_Scale2D( matrix4x4 out, vec_t x, vec_t y )
{
	out[0][0] *= x;
	out[1][1] *= y;

#ifdef OPENGL_STYLE
	out[0][1] *= x;
	out[1][0] *= y;
#else
	out[1][0] *= x;
	out[0][1] *= y;
#endif
}

_inline void Matrix4x4_Translate2D( matrix4x4 out, vec_t x, vec_t y )
{
#ifdef OPENGL_STYLE
	out[3][0] += x;
	out[3][1] += y;
#else
	out[0][3] += x;
	out[1][3] += y;
#endif
}

_inline void Matrix4x4_SetOrigin2D( matrix4x4 out, vec_t x, vec_t y )
{
#ifdef OPENGL_STYLE
	out[3][0] = x;
	out[3][1] = y;
#else
	out[0][3] = x;
	out[1][3] = y;
#endif
}

_inline void Matrix4x4_Stretch2D( matrix4x4 out, vec_t s, vec_t t )
{
	out[0][0] *= s;
	out[1][1] *= s;
#ifdef OPENGL_STYLE
	out[0][1] *= s;
	out[1][0] *= s;
	out[3][0] = s * out[3][0] + t;
	out[3][1] = s * out[3][1] + t;
#else
	out[1][0] *= s;
	out[0][1] *= s;
	out[0][3] = s * out[0][3] + t;
	out[1][3] = s * out[1][3] + t;
#endif
}

_inline void Matrix4x4_ConcatScale( matrix4x4 out, float x )
{
	matrix4x4	base, temp;

	Matrix4x4_Copy( base, out );
	Matrix4x4_CreateScale( temp, x );
	Matrix4x4_Concat( out, base, temp );
}

_inline void Matrix4x4_ConcatTranslate( matrix4x4 out, float x, float y, float z )
{
	matrix4x4 base, temp;

	Matrix4x4_Copy( base, out );
	Matrix4x4_CreateTranslate( temp, x, y, z );
	Matrix4x4_Concat( out, base, temp );
}

_inline void Matrix4x4_ConcatRotate( matrix4x4 out, float angle, float x, float y, float z )
{
	matrix4x4 base, temp;

	Matrix4x4_Copy( base, out );
	Matrix4x4_CreateRotate( temp, angle, x, y, z );
	Matrix4x4_Concat( out, base, temp );
}

_inline void Matrix4x4_ConcatScale3( matrix4x4 out, float x, float y, float z )
{
	matrix4x4  base, temp;

	Matrix4x4_Copy( base, out );
	Matrix4x4_CreateScale3( temp, x, y, z );
	Matrix4x4_Concat( out, base, temp );
}

_inline void Matrix4x4_Pivot( matrix4x4 m, const vec3_t org, const vec3_t ang, const vec3_t scale, const vec3_t pivot )
{
	vec3_t	temp;

	VectorAdd( pivot, org, temp );
	Matrix4x4_LoadIdentity( m );
	Matrix4x4_ConcatTranslate( m, temp[0], temp[1], temp[2] );
	Matrix4x4_ConcatRotate( m, ang[0], 1, 0, 0 );
	Matrix4x4_ConcatRotate( m, ang[1], 0, 1, 0 );
	Matrix4x4_ConcatRotate( m, ang[2], 0, 0, 1 );
	Matrix4x4_ConcatScale3( m, scale[0], scale[1], scale[2] );
	VectorNegate( pivot, temp );
	Matrix4x4_ConcatTranslate( m, temp[0], temp[1], temp[2] );
}

#endif//MATRIX_LIB_H