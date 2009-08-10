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
#ifndef __R_MATH_H__
#define __R_MATH_H__

// r_math.h
float		CalcFov( float fov_x, float width, float height );
void		AdjustFov( float *fov_x, float *fov_y, float width, float height, bool lock_x );
void		PlaneFromPoints( vec3_t verts[3], cplane_t *plane );
int		SignbitsForPlane( const cplane_t *out );
int		PlaneTypeForNormal( const vec3_t normal );

#endif /*__R_MATH_H__*/
