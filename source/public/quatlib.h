//=======================================================================
//			Copyright XashXT Group 2009 ©
//		         quatlib.h - base math functions
//=======================================================================
#ifndef QUATLIB_H
#define QUATLIB_H

// FIXME: move out to mathlib.h
_inline float Q_RSqrt( float number )
{
	int	i;
	float	x2, y;

	if( number == 0.0f )
		return 0.0f;

	x2 = number * 0.5f;
	y = number;
	i = * (int *) &y;			// evil floating point bit level hacking
	i = 0x5f3759df - (i >> 1);		// what the fuck?
	y = * (float *) &i;
	y = y * (1.5f - (x2 * y * y));	// this can be done a second time

	return y;
}

// The expression a * rsqrt(b) is intended as a higher performance alternative to a / sqrt(b).
// The two expressions are comparably accurate, but do not compute exactly the same value in every case.
// For example, a * rsqrt(a*a + b*b) can be just slightly greater than 1, in rare cases.
#define SQRTFAST( x )		(( x ) * Q_RSqrt( x ))
#define VectorLengthFast(v)		(SQRTFAST(DotProduct((v),(v))))
#define DistanceFast(v1,v2)		(SQRTFAST(VectorDistance2(v1,v2)))

_inline void Quat_Identity( quat_t q )
{
	q[0] = 0;
	q[1] = 0;
	q[2] = 0;
	q[3] = 1;
}

_inline void Quat_Copy( const quat_t q1, quat_t q2 )
{
	q2[0] = q1[0];
	q2[1] = q1[1];
	q2[2] = q1[2];
	q2[3] = q1[3];
}

_inline bool Quat_Compare( const quat_t q1, const quat_t q2 )
{
	if( q1[0] != q2[0] || q1[1] != q2[1] || q1[2] != q2[2] || q1[3] != q2[3] )
		return false;
	return true;
}

_inline void Quat_Conjugate( const quat_t q1, quat_t q2 )
{
	q2[0] = -q1[0];
	q2[1] = -q1[1];
	q2[2] = -q1[2];
	q2[3] = q1[3];
}

_inline float Quat_Normalize( quat_t q )
{
	float	length;

	length = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
	if( length != 0 )
	{
		float	ilength = 1.0 / sqrt( length );

		q[0] *= ilength;
		q[1] *= ilength;
		q[2] *= ilength;
		q[3] *= ilength;
	}
	return length;
}

_inline float Quat_Inverse( const quat_t q1, quat_t q2 )
{
	Quat_Conjugate( q1, q2 );
	return Quat_Normalize( q2 );
}

_inline void Matrix_Quat( vec3_t m[3], quat_t q )
{
	float	tr, s;

	tr = m[0][0] + m[1][1] + m[2][2];
	if( tr > 0.00001 )
	{
		s = sqrt( tr + 1.0 );
		q[3] = s * 0.5; s = 0.5 / s;
		q[0] = (m[2][1] - m[1][2]) * s;
		q[1] = (m[0][2] - m[2][0]) * s;
		q[2] = (m[1][0] - m[0][1]) * s;
	}
	else
	{
		int	i, j, k;

		i = 0;
		if( m[1][1] > m[0][0] ) i = 1;
		if( m[2][2] > m[i][i] ) i = 2;
		j = (i + 1) % 3;
		k = (i + 2) % 3;

		s = sqrt( m[i][i] - (m[j][j] + m[k][k]) + 1.0 );

		q[i] = s * 0.5; if( s != 0.0 ) s = 0.5 / s;
		q[j] = (m[j][i] + m[i][j]) * s;
		q[k] = (m[k][i] + m[i][k]) * s;
		q[3] = (m[k][j] - m[j][k]) * s;
	}
	Quat_Normalize( q );
}

_inline void Quat_Multiply( const quat_t q1, const quat_t q2, quat_t out )
{
	out[0] = q1[3] * q2[0] + q1[0] * q2[3] + q1[1] * q2[2] - q1[2] * q2[1];
	out[1] = q1[3] * q2[1] + q1[1] * q2[3] + q1[2] * q2[0] - q1[0] * q2[2];
	out[2] = q1[3] * q2[2] + q1[2] * q2[3] + q1[0] * q2[1] - q1[1] * q2[0];
	out[3] = q1[3] * q2[3] - q1[0] * q2[0] - q1[1] * q2[1] - q1[2] * q2[2];
}

_inline void Quat_Lerp( const quat_t q1, const quat_t q2, float t, quat_t out )
{
	quat_t	p1;
	vec_t	omega, cosom, sinom, scale0, scale1, sinsqr;

	if( Quat_Compare( q1, q2 ))
	{
		Quat_Copy( q1, out );
		return;
	}

	cosom = q1[0] * q2[0] + q1[1] * q2[1] + q1[2] * q2[2] + q1[3] * q2[3];
	if( cosom < 0.0 )
	{ 
		cosom = -cosom;
		p1[0] = -q1[0]; p1[1] = -q1[1];
		p1[2] = -q1[2]; p1[3] = -q1[3];
	}
	else
	{
		p1[0] = q1[0]; p1[1] = q1[1];
		p1[2] = q1[2]; p1[3] = q1[3];
	}

	if( cosom < 1.0 - 0.0001 )
	{
		sinsqr = 1.0 - cosom * cosom;
		sinom = Q_RSqrt( sinsqr );
		omega = atan2( sinsqr * sinom, cosom );
		scale0 = sin( (1.0 - t) * omega ) * sinom;
		scale1 = sin( t * omega ) * sinom;
	}
	else
	{ 
		scale0 = 1.0 - t;
		scale1 = t;
	}

	out[0] = scale0 * p1[0] + scale1 * q2[0];
	out[1] = scale0 * p1[1] + scale1 * q2[1];
	out[2] = scale0 * p1[2] + scale1 * q2[2];
	out[3] = scale0 * p1[3] + scale1 * q2[3];
}

_inline void Quat_Vectors( const quat_t q, vec3_t f, vec3_t r, vec3_t u )
{
	float	wx, wy, wz, xx, yy, yz, xy, xz, zz, x2, y2, z2;

	x2 = q[0] + q[0]; y2 = q[1] + q[1]; z2 = q[2] + q[2];

	xx = q[0] * x2; yy = q[1] * y2; zz = q[2] * z2;
	f[0] = 1.0f - yy - zz; r[1] = 1.0f - xx - zz; u[2] = 1.0f - xx - yy;

	yz = q[1] * z2; wx = q[3] * x2;
	r[2] = yz - wx; u[1] = yz + wx; 

	xy = q[0] * y2; wz = q[3] * z2;
	f[1] = xy - wz; r[0] = xy + wz;

	xz = q[0] * z2; wy = q[3] * y2; 
	f[2] = xz + wy; u[0] = xz - wy;
}

_inline void Quat_Matrix( const quat_t q, vec3_t m[3] )
{
	Quat_Vectors( q, m[0], m[1], m[2] );
}

_inline void Quat_TransformVector( const quat_t q, const vec3_t v, vec3_t out )
{
	float	wx, wy, wz, xx, yy, yz, xy, xz, zz, x2, y2, z2;

	x2 = q[0] + q[0]; y2 = q[1] + q[1]; z2 = q[2] + q[2];
	xx = q[0] * x2; xy = q[0] * y2; xz = q[0] * z2;
	yy = q[1] * y2; yz = q[1] * z2; zz = q[2] * z2;
	wx = q[3] * x2; wy = q[3] * y2; wz = q[3] * z2;

	out[0] = (1.0f - yy - zz) * v[0] + (xy - wz) * v[1] + (xz + wy) * v[2];
	out[1] = (xy + wz) * v[0] + (1.0f - xx - zz) * v[1] + (yz - wx) * v[2];
	out[2] = (xz - wy) * v[0] + (yz + wx) * v[1] + (1.0f - xx - yy) * v[2];
}

_inline void Quat_ConcatTransforms( const quat_t q1,const vec3_t v1,const quat_t q2,const vec3_t v2,quat_t q, vec3_t v )
{
	Quat_Multiply( q1, q2, q );
	Quat_TransformVector( q1, v2, v );
	v[0] += v1[0];
	v[1] += v1[1];
	v[2] += v1[2];
}

_inline void Matrix_Identity( vec3_t m[3] )
{
	int	i, j;

	for( i = 0; i < 3; i++ )
	{
		for( j = 0; j < 3; j++ )
		{
			if( i == j )
				m[i][j] = 1.0f;
			else m[i][j] = 0.0f;
		}
	}
}

_inline void Matrix_Copy( vec3_t m1[3], vec3_t m2[3] )
{
	Mem_Copy( m2, m1, sizeof( m2 ));
}

_inline bool Matrix_Compare( vec3_t m1[3], vec3_t m2[3] )
{
	int	i, j;

	for( i = 0; i < 3; i++ )
		for( j = 0; j < 3; j++ )
			if( m1[i][j] != m2[i][j] )
				return false;
	return true;
}

_inline void Matrix_Multiply( vec3_t m1[3], vec3_t m2[3], vec3_t out[3] )
{
	out[0][0] = m1[0][0]*m2[0][0] + m1[0][1]*m2[1][0] + m1[0][2]*m2[2][0];
	out[0][1] = m1[0][0]*m2[0][1] + m1[0][1]*m2[1][1] + m1[0][2]*m2[2][1];
	out[0][2] = m1[0][0]*m2[0][2] + m1[0][1]*m2[1][2] + m1[0][2]*m2[2][2];
	out[1][0] = m1[1][0]*m2[0][0] + m1[1][1]*m2[1][0] + m1[1][2]*m2[2][0];
	out[1][1] = m1[1][0]*m2[0][1] + m1[1][1]*m2[1][1] + m1[1][2]*m2[2][1];
	out[1][2] = m1[1][0]*m2[0][2] + m1[1][1]*m2[1][2] + m1[1][2]*m2[2][2];
	out[2][0] = m1[2][0]*m2[0][0] + m1[2][1]*m2[1][0] + m1[2][2]*m2[2][0];
	out[2][1] = m1[2][0]*m2[0][1] + m1[2][1]*m2[1][1] + m1[2][2]*m2[2][1];
	out[2][2] = m1[2][0]*m2[0][2] + m1[2][1]*m2[1][2] + m1[2][2]*m2[2][2];
}

_inline void Matrix_TransformVector( vec3_t m[3], vec3_t v, vec3_t out )
{
	out[0] = m[0][0]*v[0] + m[0][1]*v[1] + m[0][2]*v[2];
	out[1] = m[1][0]*v[0] + m[1][1]*v[1] + m[1][2]*v[2];
	out[2] = m[2][0]*v[0] + m[2][1]*v[1] + m[2][2]*v[2];
}

_inline void Matrix_Transpose( vec3_t in[3], vec3_t out[3] )
{
	out[0][0] = in[0][0];
	out[1][1] = in[1][1];
	out[2][2] = in[2][2];

	out[0][1] = in[1][0];
	out[0][2] = in[2][0];
	out[1][0] = in[0][1];
	out[1][2] = in[2][1];
	out[2][0] = in[0][2];
	out[2][1] = in[1][2];
}

_inline void Matrix_EulerAngles( vec3_t m[3], vec3_t angles )
{
	float	c;
	float	pitch, yaw, roll;

	pitch = -asin( m[0][2] );
	c = cos( pitch );
	if( fabs( c ) > 5*10e-6 )			// Gimball lock?
	{	
		// no
		c = 1.0f / c;
		pitch = RAD2DEG( pitch );
		yaw = RAD2DEG( atan2( m[0][1] * c, m[0][0] * c ));
		roll = RAD2DEG( atan2( -m[1][2] * c, m[2][2] * c ));
	}
	else
	{	// yes
		pitch = m[0][2] > 0 ? -90 : 90;
		yaw = RAD2DEG( atan2 ( m[1][0], -m[1][1] ) );
		roll = 180;
	}

	angles[PITCH] = pitch;
	angles[YAW] = yaw;
	angles[ROLL] = roll;
}

_inline void Matrix_Rotate( vec3_t m[3], float angle, float x, float y, float z )
{
	vec3_t	t[3], b[3];
	float	c = cos( DEG2RAD( angle ));
	float	s = sin( DEG2RAD( angle ));
	float	mc = 1 - c, t1, t2;
	
	t[0][0] = (x * x * mc) + c;
	t[1][1] = (y * y * mc) + c;
	t[2][2] = (z * z * mc) + c;

	t1 = y * x * mc;
	t2 = z * s;
	t[0][1] = t1 + t2;
	t[1][0] = t1 - t2;

	t1 = x * z * mc;
	t2 = y * s;
	t[0][2] = t1 - t2;
	t[2][0] = t1 + t2;

	t1 = y * z * mc;
	t2 = x * s;
	t[1][2] = t1 + t2;
	t[2][1] = t1 - t2;

	Matrix_Copy( m, b );
	Matrix_Multiply( b, t, m );
}

_inline void Matrix_FromPoints( vec3_t v1, vec3_t v2, vec3_t v3, vec3_t m[3] )
{
	float	d;

	m[2][0] = (v1[1] - v2[1]) * (v3[2] - v2[2]) - (v1[2] - v2[2]) * (v3[1] - v2[1]);
	m[2][1] = (v1[2] - v2[2]) * (v3[0] - v2[0]) - (v1[0] - v2[0]) * (v3[2] - v2[2]);
	m[2][2] = (v1[0] - v2[0]) * (v3[1] - v2[1]) - (v1[1] - v2[1]) * (v3[0] - v2[0]);
	VectorNormalizeFast( m[2] );

	// this rotate and negate guarantees a vector not colinear with the original
	VectorSet( m[1], m[2][2], -m[2][0], m[2][1] );
	d = -DotProduct( m[1], m[2] );
	VectorMA( m[1], d, m[2], m[1] );
	VectorNormalizeFast( m[1] );
	CrossProduct( m[1], m[2], m[0] );
}

#endif//QUATLIB_H