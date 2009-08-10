/*
Copyright (C) 2002-2007 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// r_math.c

#include "r_local.h"
#include "r_math.h"
#include "mathlib.h"

/*
====================
CalcFov
====================
*/
float CalcFov( float fov_x, float width, float height )
{
	float	x;

	if( fov_x < 1 || fov_x > 179 )
		Host_Error( "bad fov: %f\n", fov_x );

	x = width / tan( fov_x / 360 * M_PI );

	return atan( height / x ) * 360 / M_PI;
}

/*
====================
AdjustFov
====================
*/
void AdjustFov( float *fov_x, float *fov_y, float width, float height, bool lock_x )
{
	float x, y;

	if( width * 3 == 4 * height || width * 4 == height * 5 )
	{
		// 4:3 or 5:4 ratio
		return;
	}

	if( lock_x )
	{
		*fov_y = 2 * atan((width * 3) / (height * 4) * tan (*fov_y * M_PI / 360.0 * 0.5) ) * 360 / M_PI;
		return;
	}

	y = CalcFov( *fov_x, 640, 480 );
	x = *fov_x;

	*fov_x = CalcFov( y, height, width );
	if( *fov_x < x ) *fov_x = x;
	else *fov_y = y;
}

/*
=================
SignbitsForPlane

fast box on planeside test
=================
*/
int SignbitsForPlane( const cplane_t *out )
{
	int	bits, i;

	for( i = bits = 0; i < 3; i++ )
		if( out->normal[i] < 0.0f ) bits |= 1<<i;
	return bits;
}

/*
=================
PlaneTypeForNormal
=================
*/
int PlaneTypeForNormal( const vec3_t normal )
{
	// NOTE: should these have an epsilon around 1.0?		
	if( normal[0] >= 1.0f ) return PLANE_X;
	if( normal[1] >= 1.0f ) return PLANE_Y;
	if( normal[2] >= 1.0f ) return PLANE_Z;
	return PLANE_NONAXIAL;
}


/*
=================
PlaneFromPoints
=================
*/
void PlaneFromPoints( vec3_t verts[3], cplane_t *plane )
{
	vec3_t	v1, v2;

	VectorSubtract( verts[1], verts[0], v1 );
	VectorSubtract( verts[2], verts[0], v2 );
	CrossProduct( v2, v1, plane->normal );
	VectorNormalize( plane->normal );
	plane->dist = DotProduct( verts[0], plane->normal );
}