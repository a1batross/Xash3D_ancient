/*
Copyright (C) 1997-2001 Id Software, Inc.
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

// r_light.c

#include "r_local.h"
#include "mathlib.h"
#include "quatlib.h"

/*
=============================================================================

DYNAMIC LIGHTS BLEND RENDERING

=============================================================================
*/

/*
=============
R_SurfPotentiallyLit
=============
*/
bool R_SurfPotentiallyLit( msurface_t *surf )
{
	ref_shader_t *shader;

	if( surf->flags & ( SURF_SKY|SURF_NODLIGHT|SURF_NODRAW ) )
		return false;

	shader = surf->shader;
	if( ( shader->flags & ( SHADER_SKY|SHADER_FLARE ) ) || !shader->numpasses )
		return false;

	return ( surf->mesh && ( surf->facetype != MST_FLARE ) /* && (surf->facetype != MST_TRISURF)*/ );
}

/*
=============
R_LightBounds
=============
*/
void R_LightBounds( const vec3_t origin, float intensity, vec3_t mins, vec3_t maxs )
{
	VectorSet( mins, origin[0] - intensity, origin[1] - intensity, origin[2] - intensity /* - intensity*/ );
	VectorSet( maxs, origin[0] + intensity, origin[1] + intensity, origin[2] + intensity /* + intensity*/ );
}

/*
=============
R_AddSurfDlighbits
=============
*/
unsigned int R_AddSurfDlighbits( msurface_t *surf, unsigned int dlightbits )
{
	unsigned int k, bit;
	dlight_t *lt;
	float dist;

	for( k = 0, bit = 1, lt = r_dlights; k < r_numDlights; k++, bit <<= 1, lt++ )
	{
		if( dlightbits & bit )
		{
			if( surf->facetype == MST_PLANAR )
			{
				dist = PlaneDiff( lt->origin, surf->plane );
				if( dist <= -lt->intensity || dist >= lt->intensity )
					dlightbits &= ~bit; // how lucky...
			}
			else if( surf->facetype == MST_PATCH || surf->facetype == MST_TRISURF )
			{
				if( !BoundsIntersect( surf->mins, surf->maxs, lt->mins, lt->maxs ) )
					dlightbits &= ~bit;
			}
		}
	}
	return dlightbits;
}

/*
=================
R_AddDynamicLights
=================
*/
void R_AddDynamicLights( unsigned int dlightbits, int state )
{
	unsigned int i, j, numTempElems;
	bool cullAway;
	const dlight_t *light;
	const ref_shader_t *shader;
	vec3_t tvec, dlorigin, normal;
	elem_t tempElemsArray[MAX_ARRAY_ELEMENTS];
	float inverseIntensity, *v1, *v2, *v3, dist;
	const mat4x4_t m = { 0.5, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 0.5, 0, 0.5, 0.5, 0.5, 1 };
	GLfloat xyzFallof[4][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 0 } };

	r_backacc.numColors = 0;

	// we multitexture or texture3D support for dynamic lights
	if( !GL_Support( R_TEXTURE_3D_EXT ) && !GL_Support( R_ARB_MULTITEXTURE ))
		return;

	for( i = 0; i < (unsigned)( GL_Support( R_TEXTURE_3D_EXT ) ? 1 : 2 ); i++ )
	{
		GL_SelectTexture( i );
		GL_TexEnv( GL_MODULATE );
		GL_SetState( state | ( i ? 0 : GLSTATE_BLEND_MTEX ) );
		GL_SetTexCoordArrayMode( 0 );
		GL_EnableTexGen( GL_S, GL_OBJECT_LINEAR );
		GL_EnableTexGen( GL_T, GL_OBJECT_LINEAR );
		GL_EnableTexGen( GL_R, 0 );
		GL_EnableTexGen( GL_Q, 0 );
	}

	if( GL_Support( R_TEXTURE_3D_EXT ))
	{
		GL_EnableTexGen( GL_R, GL_OBJECT_LINEAR );
		pglDisable( GL_TEXTURE_2D );
		pglEnable( GL_TEXTURE_3D );
	}
	else
	{
		pglEnable( GL_TEXTURE_2D );
	}

	for( i = 0, light = r_dlights; i < r_numDlights; i++, light++ )
	{
		if( !( dlightbits & ( 1<<i ) ) )
			continue; // not lit by this light

		VectorSubtract( light->origin, RI.currententity->origin, dlorigin );
		if( !Matrix_Compare( RI.currententity->axis, axis_identity ) )
		{
			VectorCopy( dlorigin, tvec );
			Matrix_TransformVector( RI.currententity->axis, tvec, dlorigin );
		}

		shader = light->shader;
		if( shader && ( shader->flags & SHADER_CULL_BACK ) )
			cullAway = true;
		else
			cullAway = false;

		numTempElems = 0;
		if( cullAway )
		{
			for( j = 0; j < r_backacc.numElems; j += 3 )
			{
				v1 = ( float * )( vertsArray + elemsArray[j+0] );
				v2 = ( float * )( vertsArray + elemsArray[j+1] );
				v3 = ( float * )( vertsArray + elemsArray[j+2] );

				normal[0] = ( v1[1] - v2[1] ) * ( v3[2] - v2[2] ) - ( v1[2] - v2[2] ) * ( v3[1] - v2[1] );
				normal[1] = ( v1[2] - v2[2] ) * ( v3[0] - v2[0] ) - ( v1[0] - v2[0] ) * ( v3[2] - v2[2] );
				normal[2] = ( v1[0] - v2[0] ) * ( v3[1] - v2[1] ) - ( v1[1] - v2[1] ) * ( v3[0] - v2[0] );
				dist = ( dlorigin[0] - v1[0] ) * normal[0] + ( dlorigin[1] - v1[1] ) * normal[1] + ( dlorigin[2] - v1[2] ) * normal[2];

				if( dist <= 0 || dist * Q_RSqrt( DotProduct( normal, normal ) ) >= light->intensity )
					continue;

				tempElemsArray[numTempElems++] = elemsArray[j+0];
				tempElemsArray[numTempElems++] = elemsArray[j+1];
				tempElemsArray[numTempElems++] = elemsArray[j+2];
			}
			if( !numTempElems )
				continue;
		}

		inverseIntensity = 1 / light->intensity;

		GL_Bind( 0, r_dlighttexture );
		GL_LoadTexMatrix( m );
		pglColor4f( light->color[0], light->color[1], light->color[2], 255 );

		xyzFallof[0][0] = inverseIntensity;
		xyzFallof[0][3] = -dlorigin[0] * inverseIntensity;
		pglTexGenfv( GL_S, GL_OBJECT_PLANE, xyzFallof[0] );

		xyzFallof[1][1] = inverseIntensity;
		xyzFallof[1][3] = -dlorigin[1] * inverseIntensity;
		pglTexGenfv( GL_T, GL_OBJECT_PLANE, xyzFallof[1] );

		xyzFallof[2][2] = inverseIntensity;
		xyzFallof[2][3] = -dlorigin[2] * inverseIntensity;

		if( GL_Support( R_TEXTURE_3D_EXT ))
		{
			pglTexGenfv( GL_R, GL_OBJECT_PLANE, xyzFallof[2] );
		}
		else
		{
			GL_Bind( 1, r_dlighttexture );
			GL_LoadTexMatrix( m );

			pglTexGenfv( GL_S, GL_OBJECT_PLANE, xyzFallof[2] );
			pglTexGenfv( GL_T, GL_OBJECT_PLANE, xyzFallof[3] );
		}

		if( numTempElems )
		{
			if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
				pglDrawRangeElementsEXT( GL_TRIANGLES, 0, r_backacc.numVerts, numTempElems, GL_UNSIGNED_INT, tempElemsArray );
			else
				pglDrawElements( GL_TRIANGLES, numTempElems, GL_UNSIGNED_INT, tempElemsArray );
		}
		else
		{
			if( GL_Support( R_DRAW_RANGEELEMENTS_EXT ))
				pglDrawRangeElementsEXT( GL_TRIANGLES, 0, r_backacc.numVerts, r_backacc.numElems, GL_UNSIGNED_INT, elemsArray );
			else
				pglDrawElements( GL_TRIANGLES, r_backacc.numElems, GL_UNSIGNED_INT, elemsArray );
		}
	}

	if( GL_Support( R_TEXTURE_3D_EXT ))
	{
		GL_EnableTexGen( GL_R, 0 );
		pglDisable( GL_TEXTURE_3D );
		pglEnable( GL_TEXTURE_2D );
	}
	else
	{
		pglDisable( GL_TEXTURE_2D );
		GL_SelectTexture( 0 );
	}
}

//===================================================================

static ref_shader_t *r_coronaShader;

/*
=================
R_InitCoronas
=================
*/
void R_InitCoronas( void )
{
	r_coronaShader = R_LoadShader( "***r_coronatexture***", SHADER_BSP_FLARE, true, TF_NOMIPMAP|TF_NOPICMIP|TF_UNCOMPRESSED|TF_CLAMP, SHADER_INVALID );
}

/*
=================
R_DrawCoronas
=================
*/
void R_DrawCoronas( void )
{
	unsigned int i;
	float dist;
	dlight_t *light;
	meshbuffer_t *mb;
	trace_t tr;

	if( r_dynamiclight->integer != 2 )
		return;

	for( i = 0, light = r_dlights; i < r_numDlights; i++, light++ )
	{
		dist =
			RI.vpn[0] * ( light->origin[0] - RI.viewOrigin[0] ) +
			RI.vpn[1] * ( light->origin[1] - RI.viewOrigin[1] ) +
			RI.vpn[2] * ( light->origin[2] - RI.viewOrigin[2] );
		if( dist < 24.0f )
			continue;
		dist -= light->intensity;

		R_TraceLine( &tr, light->origin, RI.viewOrigin, SURF_NONSOLID );
		if( tr.fraction != 1.0f )
			continue;

		mb = R_AddMeshToList( MB_CORONA, NULL, r_coronaShader, -( (signed int)i + 1 ) );
		if( mb )
			mb->shaderkey |= ( bound( 1, 0x4000 - (unsigned int)dist, 0x4000 - 1 ) << 12 );
	}
}

//===================================================================

/*
===============
R_LightForOrigin
===============
*/
void R_LightForOrigin( const vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius )
{
	int i, j;
	int k, s;
	int vi[3], elem[4];
	float dot, t[8];
	vec3_t vf, vf2, tdir;
	vec3_t ambientLocal, diffuseLocal;
	vec_t *gridSize, *gridMins;
	int *gridBounds;
	mgridlight_t **lightarray;

	VectorSet( ambientLocal, 0, 0, 0 );
	VectorSet( diffuseLocal, 0, 0, 0 );

	if( !r_worldmodel /* || (RI.refdef.rdflags & RDF_NOWORLDMODEL)*/ ||
		!r_worldbrushmodel->lightgrid || !r_worldbrushmodel->numlightgridelems )
	{
		VectorSet( dir, 0.5f, 0.2f, -1.0f );
		goto dynamic;
	}

	lightarray = r_worldbrushmodel->lightarray;
	gridSize = r_worldbrushmodel->gridSize;
	gridMins = r_worldbrushmodel->gridMins;
	gridBounds = r_worldbrushmodel->gridBounds;

	for( i = 0; i < 3; i++ )
	{
		vf[i] = ( origin[i] - gridMins[i] ) / gridSize[i];
		vi[i] = (int)vf[i];
		vf[i] = vf[i] - floor( vf[i] );
		vf2[i] = 1.0f - vf[i];
	}

	elem[0] = vi[2] * gridBounds[3] + vi[1] * gridBounds[0] + vi[0];
	elem[1] = elem[0] + gridBounds[0];
	elem[2] = elem[0] + gridBounds[3];
	elem[3] = elem[2] + gridBounds[0];

	for( i = 0; i < 4; i++ )
	{
		if( elem[i] < 0 || elem[i] >= ( r_worldbrushmodel->numlightarrayelems-1 ) )
		{
			VectorSet( dir, 0.5f, 0.2f, -1.0f );
			goto dynamic;
		}
	}

	t[0] = vf2[0] * vf2[1] * vf2[2];
	t[1] = vf[0] * vf2[1] * vf2[2];
	t[2] = vf2[0] * vf[1] * vf2[2];
	t[3] = vf[0] * vf[1] * vf2[2];
	t[4] = vf2[0] * vf2[1] * vf[2];
	t[5] = vf[0] * vf2[1] * vf[2];
	t[6] = vf2[0] * vf[1] * vf[2];
	t[7] = vf[0] * vf[1] * vf[2];

	VectorClear( dir );

	for( i = 0; i < 4; i++ )
	{
		R_LatLongToNorm( lightarray[elem[i]]->direction, tdir );
		VectorScale( tdir, t[i *2], tdir );
		for( k = 0; k < LM_STYLES && ( s = lightarray[elem[i]]->styles[k] ) != 255; k++ )
		{
			dir[0] += r_lightStyles[s].rgb[0] * tdir[0];
			dir[1] += r_lightStyles[s].rgb[1] * tdir[1];
			dir[2] += r_lightStyles[s].rgb[2] * tdir[2];
		}

		R_LatLongToNorm( lightarray[elem[i] + 1]->direction, tdir );
		VectorScale( tdir, t[i *2+1], tdir );
		for( k = 0; k < LM_STYLES && ( s = lightarray[elem[i] + 1]->styles[k] ) != 255; k++ )
		{
			dir[0] += r_lightStyles[s].rgb[0] * tdir[0];
			dir[1] += r_lightStyles[s].rgb[1] * tdir[1];
			dir[2] += r_lightStyles[s].rgb[2] * tdir[2];
		}
	}

	for( j = 0; j < 3; j++ )
	{
		if( ambient )
		{
			for( i = 0; i < 4; i++ )
			{
				for( k = 0; k < LM_STYLES; k++ )
				{
					if( ( s = lightarray[elem[i]]->styles[k] ) != 255 )
						ambientLocal[j] += t[i*2] * lightarray[elem[i]]->ambient[k][j] * r_lightStyles[s].rgb[j];
					if( ( s = lightarray[elem[i] + 1]->styles[k] ) != 255 )
						ambientLocal[j] += t[i*2+1] * lightarray[elem[i] + 1]->ambient[k][j] * r_lightStyles[s].rgb[j];
				}
			}
		}
		if( diffuse || radius )
		{
			for( i = 0; i < 4; i++ )
			{
				for( k = 0; k < LM_STYLES; k++ )
				{
					if( ( s = lightarray[elem[i]]->styles[k] ) != 255 )
						diffuseLocal[j] += t[i*2] * lightarray[elem[i]]->diffuse[k][j] * r_lightStyles[s].rgb[j];
					if( ( s = lightarray[elem[i] + 1]->styles[k] ) != 255 )
						diffuseLocal[j] += t[i*2+1] * lightarray[elem[i] + 1]->diffuse[k][j] * r_lightStyles[s].rgb[j];
				}
			}
		}
	}

dynamic:
	// add dynamic lights
	if( radius && r_dynamiclight->integer && r_numDlights )
	{
		unsigned int lnum;
		dlight_t *dl;
		float dist, dist2, add;
		vec3_t direction;
		bool anyDlights = false;

		for( lnum = 0, dl = r_dlights; lnum < r_numDlights; lnum++, dl++ )
		{
			if( !BoundsAndSphereIntersect( dl->mins, dl->maxs, origin, radius ) )
				continue;

			VectorSubtract( dl->origin, origin, direction );
			dist = VectorLength( direction );

			if( !dist || dist > dl->intensity + radius )
				continue;

			if( !anyDlights )
			{
				VectorNormalizeFast( dir );
				anyDlights = true;
			}

			add = 1.0 - (dist / (dl->intensity + radius));
			dist2 = add * 0.5 / dist;
			for( i = 0; i < 3; i++ )
			{
				dot = dl->color[i] * add;
				diffuseLocal[i] += dot;
				ambientLocal[i] += dot * 0.05;
				dir[i] += direction[i] * dist2;
			}
		}
	}

	VectorNormalizeFast( dir );

	if( ambient )
	{
		dot = bound( 0.0f, r_lighting_ambientscale->value, 1.0f ) * ( 1 << mapConfig.pow2MapOvrbr ) * ( 1.0 / 255.0 );
		for( i = 0; i < 3; i++ )
		{
			ambient[i] = ambientLocal[i] * dot;
			ambient[i] = bound( 0, ambient[i], 1 );
		}
		ambient[3] = 1.0f;
	}

	if( diffuse )
	{
		dot = bound( 0.0f, r_lighting_directedscale->value, 1.0f ) * ( 1 << mapConfig.pow2MapOvrbr ) * ( 1.0 / 255.0 );
		for( i = 0; i < 3; i++ )
		{
			diffuse[i] = diffuseLocal[i] * dot;
			diffuse[i] = bound( 0, diffuse[i], 1 );
		}
		diffuse[3] = 1.0f;
	}
}

/*
===============
R_LightForEntity
===============
*/
void R_LightForEntity( ref_entity_t *e, byte *bArray )
{
	unsigned int i, lnum;
	dlight_t *dl;
	unsigned int dlightbits;
	float dot, dist;
	vec3_t lightDirs[MAX_DLIGHTS], direction, temp;
	vec4_t ambient, diffuse;

	if( ( e->flags & RF_FULLBRIGHT ) || r_fullbright->value )
		return;

	// probably weird shader, see mpteam4 for example
	if( !e->model || ( e->model->type == mod_brush ) )
		return;

	R_LightForOrigin( e->lightingOrigin, temp, ambient, diffuse, 0 );

	if( e->flags & EF_MINLIGHT )
	{
		for( i = 0; i < 3; i++ )
			if( ambient[i] > 0.1 )
				break;
		if( i == 3 )
			VectorSet( ambient, 0.1f, 0.1f, 0.1f );
	}

	// rotate direction
	Matrix_TransformVector( e->axis, temp, direction );

	// see if we are affected by dynamic lights
	dlightbits = 0;
	if( r_dynamiclight->integer == 1 && r_numDlights )
	{
		for( lnum = 0, dl = r_dlights; lnum < r_numDlights; lnum++, dl++ )
		{
			// translate
			VectorSubtract( dl->origin, e->origin, lightDirs[lnum] );
			dist = VectorLength( lightDirs[lnum] );

			if( !dist || dist > dl->intensity + e->model->radius * e->scale )
				continue;
			dlightbits |= ( 1<<lnum );
		}
	}

	if( !dlightbits )
	{
		vec3_t color;

		for( i = 0; i < r_backacc.numColors; i++, bArray += 4 )
		{
			dot = DotProduct( normalsArray[i], direction );
			if( dot <= 0 )
				VectorCopy( ambient, color );
			else
				VectorMA( ambient, dot, diffuse, color );

			bArray[0] = R_FloatToByte( color[0] );
			bArray[1] = R_FloatToByte( color[1] );
			bArray[2] = R_FloatToByte( color[2] );
		}
	}
	else
	{
		float add, intensity8, dot, *cArray, *dir;
		vec3_t dlorigin, tempColorsArray[MAX_ARRAY_VERTS];

		cArray = tempColorsArray[0];
		for( i = 0; i < r_backacc.numColors; i++, cArray += 3 )
		{
			dot = DotProduct( normalsArray[i], direction );
			if( dot <= 0 )
				VectorCopy( ambient, cArray );
			else
				VectorMA( ambient, dot, diffuse, cArray );
		}

		for( lnum = 0, dl = r_dlights; lnum < r_numDlights; lnum++, dl++ )
		{
			if( !( dlightbits & ( 1<<lnum ) ) )
				continue;

			// translate
			dir = lightDirs[lnum];

			// rotate
			Matrix_TransformVector( e->axis, dir, dlorigin );
			intensity8 = dl->intensity * 8 * e->scale;

			cArray = tempColorsArray[0];
			for( i = 0; i < r_backacc.numColors; i++, cArray += 3 )
			{
				VectorSubtract( dlorigin, vertsArray[i], dir );
				add = DotProduct( normalsArray[i], dir );

				if( add > 0 )
				{
					dot = DotProduct( dir, dir );
					add *= ( intensity8 / dot ) *Q_RSqrt( dot );
					VectorMA( cArray, add, dl->color, cArray );
				}
			}
		}

		cArray = tempColorsArray[0];
		for( i = 0; i < r_backacc.numColors; i++, bArray += 4, cArray += 3 )
		{
			bArray[0] = R_FloatToByte( cArray[0] );
			bArray[1] = R_FloatToByte( cArray[1] );
			bArray[2] = R_FloatToByte( cArray[2] );
		}
	}
}

/*
=============================================================================

LIGHTMAP ALLOCATION

=============================================================================
*/

static byte *r_lightmapBuffer;
static int r_lightmapBufferSize;
static int r_numUploadedLightmaps;
static int r_maxLightmapBlockSize;

/*
=======================
R_BuildLightmap
=======================
*/
static void R_BuildLightmap( int w, int h, bool deluxe, const byte *data, byte *dest, int blockWidth )
{
	int x, y, bits;
	byte *rgba;
	float scaled[3], maxScaled;
	bool normalize;

	if( !data || r_fullbright->integer )
	{
		if( deluxe )
		{
			for( y = 0; y < h; y++, dest )
				memset( dest + y * blockWidth, 128, w * 4 );
		}
		else
		{
			for( y = 0; y < h; y++, dest )
				memset( dest + y * blockWidth, 255, w * 4 );
		}
		return;
	}

	if( deluxe )
	{
		bits = 0;
		normalize = false;
	}
	else
	{
		bits = mapConfig.pow2MapOvrbr;
		normalize = true;
	}

	if( normalize )
	{
		for( y = 0; y < h; y++ )
		{
			for( x = 0, rgba = dest + y * blockWidth; x < w; x++, data += LM_BYTES, rgba += 4 )
			{
				scaled[0] = (float)( (int)data[0] << bits );
				scaled[1] = (float)( (int)data[1] << bits );
				scaled[2] = (float)( (int)data[2] << bits );

				maxScaled = max( max( scaled[0], scaled[1] ), scaled[2] );
				if( maxScaled > 255 )
				{
					maxScaled = 255.0f / maxScaled;
					VectorScale( scaled, maxScaled, scaled );
				}

				rgba[0] = ( byte )( ( bound( 0, scaled[0], 255 ) ) );
				rgba[1] = ( byte )( ( bound( 0, scaled[1], 255 ) ) );
				rgba[2] = ( byte )( ( bound( 0, scaled[2], 255 ) ) );
			}
		}
	}
	else
	{
		for( y = 0; y < h; y++ )
		{
			for( x = 0, rgba = dest + y * blockWidth; x < w; x++, data += LM_BYTES, rgba += 4 )
			{
				rgba[0] = bound( 0, ( (int)data[0] << bits ), 255 );
				rgba[1] = bound( 0, ( (int)data[1] << bits ), 255 );
				rgba[2] = bound( 0, ( (int)data[2] << bits ), 255 );
			}
		}
	}
}

/*
=======================
R_PackLightmaps
=======================
*/
static int R_PackLightmaps( int num, int w, int h, int size, int stride, bool deluxe, const char *name, const byte *data, mlightmapRect_t *rects )
{
	int		i, x, y, root;
	byte		*block;
	texture_t 	*image;
	int		rectX, rectY, rectSize;
	int		maxX, maxY, max, xStride;
	double		tw, th, tx, ty;
	mlightmapRect_t	*rect;
	rgbdata_t		r_image;
	char		uploadName[128];

	Mem_Set( &r_image, 0, sizeof( rgbdata_t ));
	maxX = r_maxLightmapBlockSize / w;
	maxY = r_maxLightmapBlockSize / h;
	max = min( maxX, maxY );

	MsgDev( D_NOTE, "Packing %i lightmap(s) -> ", num );
	com.snprintf( uploadName, sizeof( uploadName ), "%s%i", name, r_numUploadedLightmaps );

	if( !max || num == 1 || !mapConfig.lightmapsPacking /* || !r_lighting_packlightmaps->integer*/ )
	{
		// process as it is
		R_BuildLightmap( w, h, deluxe, data, r_lightmapBuffer, w * 4 );

		r_image.width = w;
		r_image.height = h;
		r_image.type = PF_RGB_24;
		r_image.size = w * h * LM_BYTES;
		r_image.flags = 0;
		r_image.depth = r_image.numMips = 1;
		r_image.palette = NULL;
		r_image.buffer = r_lightmapBuffer;
		image = R_LoadTexture( uploadName, &r_image, LM_BYTES, TF_NOPICMIP|TF_CLAMP|TF_NOMIPMAP );

		r_lightmapTextures[r_numUploadedLightmaps] = image;
		if( rects )
		{
			rects[0].texNum = r_numUploadedLightmaps;

			// this is not a real texture matrix, but who cares?
			rects[0].texMatrix[0][0] = 1; rects[0].texMatrix[0][1] = 0;
			rects[0].texMatrix[1][0] = 1; rects[0].texMatrix[1][1] = 0;
		}

		MsgDev( D_NOTE, "\n" );

		r_numUploadedLightmaps++;
		return 1;
	}

	// find the nearest square block size
	root = ( int )sqrt( num );
	if( root > max )
		root = max;

	// keep row size a power of two
	for( i = 1; i < root; i <<= 1 ) ;
	if( i > root )
		i >>= 1;
	root = i;

	num -= root * root;
	rectX = rectY = root;

	if( maxY > maxX )
	{
		for(; ( num >= root ) && ( rectY < maxY ); rectY++, num -= root ) ;

		// sample down if not a power of two
		for( y = 1; y < rectY; y <<= 1 ) ;
		if( y > rectY )
			y >>= 1;
		rectY = y;
	}
	else
	{
		for(; ( num >= root ) && ( rectX < maxX ); rectX++, num -= root ) ;

		// sample down if not a power of two
		for( x = 1; x < rectX; x <<= 1 ) ;
		if( x > rectX )
			x >>= 1;
		rectX = x;
	}

	tw = 1.0 / (double)rectX;
	th = 1.0 / (double)rectY;

	xStride = w * 4;
	rectSize = ( rectX * w ) * ( rectY * h ) * 4;
	if( rectSize > r_lightmapBufferSize )
	{
		if( r_lightmapBuffer )
			Mem_Free( r_lightmapBuffer );
		r_lightmapBuffer = Mem_Alloc( r_temppool, rectSize );
		Mem_Set( r_lightmapBuffer, 255, rectSize );
		r_lightmapBufferSize = rectSize;
	}

	MsgDev( D_NOTE, "%ix%i : %ix%i\n", rectX, rectY, rectX * w, rectY * h );

	block = r_lightmapBuffer;
	for( y = 0, ty = 0.0, num = 0, rect = rects; y < rectY; y++, ty += th, block += rectX * xStride * h )
	{
		for( x = 0, tx = 0.0; x < rectX; x++, tx += tw, num++, data += size * stride )
		{
			R_BuildLightmap( w, h, mapConfig.deluxeMappingEnabled && ( num & 1 ) ? true : false, data, block + x * xStride, rectX * xStride );

			// this is not a real texture matrix, but who cares?
			if( rects )
			{
				rect->texMatrix[0][0] = tw; rect->texMatrix[0][1] = tx;
				rect->texMatrix[1][0] = th; rect->texMatrix[1][1] = ty;
				rect += stride;
			}
		}
	}

	r_image.width = rectX * w;
	r_image.height = rectY * h;
	r_image.type = PF_RGB_24;
	r_image.size = r_image.width * r_image.height * LM_BYTES;
	r_image.depth = r_image.numMips = 1;
	r_image.flags = 0;
	r_image.palette = NULL;
	r_image.buffer = r_lightmapBuffer;
	image = R_LoadTexture( uploadName, &r_image, LM_BYTES, TF_NOPICMIP|TF_UNCOMPRESSED|TF_CLAMP|TF_NOMIPMAP );

	r_lightmapTextures[r_numUploadedLightmaps] = image;
	if( rects )
	{
		for( i = 0, rect = rects; i < num; i++, rect += stride )
			rect->texNum = r_numUploadedLightmaps;
	}

	r_numUploadedLightmaps++;
	return num;
}

/*
=======================
R_BuildLightmaps
=======================
*/
void R_BuildLightmaps( int numLightmaps, int w, int h, const byte *data, mlightmapRect_t *rects )
{
	int i, j, p, m;
	int numBlocks, size, stride;
	const byte *lmData;

	if( !mapConfig.lightmapsPacking )
		size = max( w, h );
	else
		for( size = 1; ( size < r_lighting_maxlmblocksize->integer ) && ( size < glConfig.max_2d_texture_size ); size <<= 1 ) ;

	if( mapConfig.deluxeMappingEnabled && ( ( size == w ) || ( size == h ) ) )
	{
		MsgDev( D_WARN, "Lightmap blocks larger than %ix%i aren't supported, deluxemaps will be disabled\n", size, size );
		mapConfig.deluxeMappingEnabled = false;
	}

	r_maxLightmapBlockSize = size;
	size = w * h * LM_BYTES;
	r_lightmapBufferSize = w * h * 4;
	r_lightmapBuffer = Mem_Alloc( r_temppool, r_lightmapBufferSize );
	r_numUploadedLightmaps = 0;

	m = 1;
	lmData = data;
	stride = 1;
	numBlocks = numLightmaps;

	if( mapConfig.deluxeMaps && !mapConfig.deluxeMappingEnabled )
	{
		m = 2;
		stride = 2;
		numLightmaps /= 2;
	}

	for( i = 0, j = 0; i < numBlocks; i += p * m, j += p )
		p = R_PackLightmaps( numLightmaps - j, w, h, size, stride, false, "*lm", lmData + j * size * stride, &rects[i] );

	if( r_lightmapBuffer )
		Mem_Free( r_lightmapBuffer );

	MsgDev( D_NOTE, "Packed %i lightmap blocks into %i texture(s)\n", numBlocks, r_numUploadedLightmaps );
}

/*
=============================================================================

LIGHT STYLE MANAGEMENT

=============================================================================
*/

int r_numSuperLightStyles;
superLightStyle_t r_superLightStyles[MAX_SUPER_STYLES];

/*
=======================
R_InitLightStyles
=======================
*/
void R_InitLightStyles( void )
{
	int i;

	for( i = 0; i < MAX_LIGHTSTYLES; i++ )
	{
		r_lightStyles[i].rgb[0] = 1;
		r_lightStyles[i].rgb[1] = 1;
		r_lightStyles[i].rgb[2] = 1;
	}
	r_numSuperLightStyles = 0;
}

/*
=======================
R_AddSuperLightStyle
=======================
*/
int R_AddSuperLightStyle( const int *lightmaps, const byte *lightmapStyles, const byte *vertexStyles, mlightmapRect_t **lmRects )
{
	int i, j;
	superLightStyle_t *sls;

	for( i = 0, sls = r_superLightStyles; i < r_numSuperLightStyles; i++, sls++ )
	{
		for( j = 0; j < LM_STYLES; j++ )
			if( sls->lightmapNum[j] != lightmaps[j] ||
				sls->lightmapStyles[j] != lightmapStyles[j] ||
				sls->vertexStyles[j] != vertexStyles[j] )
				break;
		if( j == LM_STYLES )
			return i;
	}

	if( r_numSuperLightStyles == MAX_SUPER_STYLES )
		Host_Error( "R_AddSuperLightStyle: r_numSuperLightStyles == MAX_SUPER_STYLES\n" );

	sls->features = 0;
	for( j = 0; j < LM_STYLES; j++ )
	{
		sls->lightmapNum[j] = lightmaps[j];
		sls->lightmapStyles[j] = lightmapStyles[j];
		sls->vertexStyles[j] = vertexStyles[j];

		if( lmRects && lmRects[j] && ( lightmaps[j] != -1 ) )
		{
			sls->stOffset[j][0] = lmRects[j]->texMatrix[0][0];
			sls->stOffset[j][1] = lmRects[j]->texMatrix[1][0];
		}
		else
		{
			sls->stOffset[j][0] = 0;
			sls->stOffset[j][0] = 0;
		}

		if( j )
		{
			if( lightmapStyles[j] != 255 )
				sls->features |= ( MF_LMCOORDS << j );
			if( vertexStyles[j] != 255 )
				sls->features |= ( MF_COLORS << j );
		}
	}

	return r_numSuperLightStyles++;
}

/*
=======================
R_SuperLightStylesCmp

Compare function for qsort
=======================
*/
static int R_SuperLightStylesCmp( superLightStyle_t *sls1, superLightStyle_t *sls2 )
{
	int i;

	for( i = 0; i < LM_STYLES; i++ )
	{	// compare lightmaps
		if( sls2->lightmapNum[i] > sls1->lightmapNum[i] )
			return 1;
		else if( sls1->lightmapNum[i] > sls2->lightmapNum[i] )
			return -1;
	}

	for( i = 0; i < LM_STYLES; i++ )
	{	// compare lightmap styles
		if( sls2->lightmapStyles[i] > sls1->lightmapStyles[i] )
			return 1;
		else if( sls1->lightmapStyles[i] > sls2->lightmapStyles[i] )
			return -1;
	}

	for( i = 0; i < LM_STYLES; i++ )
	{	// compare vertex styles
		if( sls2->vertexStyles[i] > sls1->vertexStyles[i] )
			return 1;
		else if( sls1->vertexStyles[i] > sls2->vertexStyles[i] )
			return -1;
	}

	return 0; // equal
}

/*
=======================
R_SortSuperLightStyles
=======================
*/
void R_SortSuperLightStyles( void )
{
	qsort( r_superLightStyles, r_numSuperLightStyles, sizeof( superLightStyle_t ), ( int ( * )( const void *, const void * ) )R_SuperLightStylesCmp );
}
