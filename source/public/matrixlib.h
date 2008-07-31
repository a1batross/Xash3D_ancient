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

#endif//BASEMATRIX_H