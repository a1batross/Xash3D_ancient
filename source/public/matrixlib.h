//=======================================================================
//			Copyright XashXT Group 2007 �
//		         mathlib.h - base math functions
//=======================================================================
#ifndef BASEMATRIX_H
#define BASEMATRIX_H

#include <math.h>
//#define OPENGL_STYLE		//TODO: enable OpenGL style someday

#ifndef M_PI
#define M_PI			(float)3.14159265358979323846
#endif

#define MatrixLoadIdentity( mat )	Matrix4x4_Copy( mat, identitymatrix )

static const matrix4x4 identitymatrix =
{
{ 1, 0, 0, 0 },	// PITCH
{ 0, 1, 0, 0 },	// YAW
{ 0, 0, 1, 0 },	// ROLL
{ 0, 0, 0, 1 },	// ORIGIN
};

_inline void Matrix4x4_Copy( matrix4x4 out, const matrix4x4 in )
{
	memcpy( out, in, sizeof(matrix4x4));
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

_inline void Matrix4x4_Rotate( const matrix4x4 in, const float v[3], float out[3] )
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

_inline void Matrix4x4_TransposeRotate( const matrix4x4 in, const float v[3], float out[3] )
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

_inline void Matrix4x4_Invert_Simple( matrix4x4 out, const matrix4x4 in1 )
{
	// we only support uniform scaling, so assume the first row is enough
	// (note the lack of sqrt here, because we're trying to undo the scaling,
	// this means multiplying by the inverse scale twice - squaring it, which
	// makes the sqrt a waste of time)
	double scale = 1.0 / (in1[0][0] * in1[0][0] + in1[0][1] * in1[0][1] + in1[0][2] * in1[0][2]);

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

_inline void Matrix4x4_CreateTranslate( matrix4x4 out, double x, double y, double z )
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

_inline void Matrix4x4_CreateRotate( matrix4x4 out, double angle, double x, double y, double z )
{
	double len, c, s;

	len = x * x + y * y + z * z;
	if( len != 0.0f ) len = 1.0f / sqrt(len);
	x *= len;
	y *= len;
	z *= len;

	angle *= (-M_PI / 180.0);
	c = cos( angle );
	s = sin( angle );

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


_inline void Matrix4x4_CreateFromEntity( matrix4x4 out, double x, double y, double z, double pitch, double yaw, double roll, double scale )
{
	double angle, sr, sp, sy, cr, cp, cy;

	if( roll )
	{
		angle = yaw * (M_PI*2 / 360);
		sy = sin(angle);
		cy = cos(angle);
		angle = pitch * (M_PI*2 / 360);
		sp = sin(angle);
		cp = cos(angle);
		angle = roll * (M_PI*2 / 360);
		sr = sin(angle);
		cr = cos(angle);
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
		sy = sin(angle);
		cy = cos(angle);
		angle = pitch * (M_PI*2 / 360);
		sp = sin(angle);
		cp = cos(angle);
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
		sy = sin(angle);
		cy = cos(angle);
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

_inline void Matrix4x4_FromOriginQuat( matrix4x4 out, double ox, double oy, double oz, double x, double y, double z, double w )
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
_inline void Matrix4x4_ConcatTransforms( matrix4x4 out, matrix4x4 in1, matrix4x4 in2 )
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

_inline void Matrix4x4_CreateScale( matrix4x4 out, double x )
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

_inline void Matrix4x4_ConcatScale( matrix4x4 out, double x )
{
	matrix4x4	base, temp;

	Matrix4x4_Copy( base, out );
	Matrix4x4_CreateScale( temp, x );
	Matrix4x4_Concat( out, base, temp );
}

#endif//BASEMATRIX_H