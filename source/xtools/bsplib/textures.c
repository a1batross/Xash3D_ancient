
#include "bsplib.h"
#include "byteorder.h"

//==========================================================================
int FindMiptex( const char *name )
{
	rgbdata_t	*image;
	int	i;

	if(!com.strlen( name )) return 0;

	for( i = 0; i < numshaders; i++ )
	{
		if( !com.stricmp( dshaders[i].name, name ))
			return i;
	}

	// allocate a new texture
	if( numshaders == MAX_MAP_SHADERS )
		Sys_Break( "MAX_MAP_SHADERS limit exceeded\n" );

	// alloc new texture
	com.strncpy( dshaders[i].name, name, sizeof( dshaders[i].name ));

	image = FS_LoadImage( name, NULL, 0 );
	if( image )
	{
		// save texture dimensions that can be used
		// for replacing original textures with HQ images
		dshaders[i].size[0] = image->width;		
		dshaders[i].size[1] = image->height;
		FS_FreeImage( image );
	}
	else dshaders[i].size[0] = dshaders[i].size[1] = -1; // technically an error

	return numshaders++;
}

/*
==================
textureAxisFromPlane
==================
*/
vec3_t baseaxis[18] =
{
{0,0,1},	{1,0,0},	{0,-1,0},	// floor
{0,0,-1}, {1,0,0},	{0,-1,0},	// ceiling
{1,0,0},	{0,1,0},	{0,0,-1},	// west wall
{-1,0,0},	{0,1,0},	{0,0,-1},	// east wall
{0,1,0},	{1,0,0},	{0,0,-1},	// south wall
{0,-1,0},	{1,0,0},	{0,0,-1},	// north wall
};

void TextureAxisFromPlane( plane_t *plane, vec3_t xv, vec3_t yv )
{
	int	bestaxis = 0;
	vec_t	dot, best = 0;
	int	i;
	
	for( i = 0; i < 6; i++ )
	{
		dot = DotProduct( plane->normal, baseaxis[i*3] );
		if (dot > best)
		{
			best = dot;
			bestaxis = i;
		}
	}
	
	VectorCopy( baseaxis[bestaxis*3+1], xv );
	VectorCopy( baseaxis[bestaxis*3+2], yv );
}

int TexinfoForBrushTexture( plane_t *plane, brush_texture_t *bt, vec3_t origin )
{
	vec_t		ang, sinv, cosv;
	vec3_t		vecs[2];
	int		i, j, k;
	dtexinfo_t	tx, *tc;
	int		sv, tv;
	vec_t		ns, nt;

	if( !bt->name[0] ) return 0;

	Mem_Set( &tx, 0, sizeof( tx ));
	tx.shadernum = FindMiptex( bt->name );

	if( bt->brush_type == BRUSH_QUARK )
	{
		Mem_Copy( tx.vecs, bt->vects.quark.vecs, sizeof( tx.vecs ));

		if(!VectorIsNull( origin ))
		{
			tx.vecs[0][3] += DotProduct( origin, tx.vecs[0] );
			tx.vecs[1][3] += DotProduct( origin, tx.vecs[1] );
		}

		dshaders[tx.shadernum].contentFlags = bt->contents;
		dshaders[tx.shadernum].surfaceFlags = bt->flags;
		tx.value = bt->value;
	}
	else if( bt->brush_type != BRUSH_RADIANT )
	{
		if( bt->brush_type == BRUSH_WORLDCRAFT_21 )
			TextureAxisFromPlane( plane, vecs[0], vecs[1] );
		if( !bt->vects.hammer.scale[0] ) bt->vects.hammer.scale[0] = 1.0f;
		if( !bt->vects.hammer.scale[1] ) bt->vects.hammer.scale[1] = 1.0f;

		if( bt->brush_type == BRUSH_WORLDCRAFT_21 )
		{
			// rotate axis
			if( bt->vects.hammer.rotate == 0 )
			{
				sinv = 0;
				cosv = 1;
			}
			else if( bt->vects.hammer.rotate == 90 )
			{
				sinv = 1;
				cosv = 0;
			}
			else if( bt->vects.hammer.rotate == 180 )
			{
				sinv = 0;
				cosv = -1;
			}
			else if( bt->vects.hammer.rotate == 270 )
			{
				sinv = -1;
				cosv = 0;
			}
			else
			{
				ang = bt->vects.hammer.rotate / 180 * M_PI;
				sinv = com.sin( ang );
				cosv = com.cos( ang );
			}
			if( vecs[0][0] ) sv = 0;
			else if( vecs[0][1] ) sv = 1;
			else sv = 2;

			if( vecs[1][0] ) tv = 0;
			else if( vecs[1][1] ) tv = 1;
			else tv = 2;
			
			for( i = 0; i < 2; i++ )
			{
				ns = cosv * vecs[i][sv] - sinv * vecs[i][tv];
				nt = sinv * vecs[i][sv] + cosv * vecs[i][tv];
				vecs[i][sv] = ns;
				vecs[i][tv] = nt;
			}

			for( i = 0; i < 2; i++ )
				for( j = 0; j < 3; j++ )
					tx.vecs[i][j] = vecs[i][j] / bt->vects.hammer.scale[i];
		}
		else if( bt->brush_type == BRUSH_WORLDCRAFT_22 )
		{
			vec_t scale;

			scale = 1 / bt->vects.hammer.scale[0];
			VectorScale( bt->vects.hammer.UAxis, scale, tx.vecs[0] );
			scale = 1 / bt->vects.hammer.scale[1];
			VectorScale( bt->vects.hammer.VAxis, scale, tx.vecs[1] );
		}

		tx.vecs[0][3] = bt->vects.hammer.shift[0] + DotProduct( origin, tx.vecs[0] );
		tx.vecs[1][3] = bt->vects.hammer.shift[1] + DotProduct( origin, tx.vecs[1] );
	}
	else if( bt->brush_type == BRUSH_RADIANT )
	{
		VectorCopy( bt->vects.radiant.matrix[0], tx.vecs[0] );
		VectorCopy( bt->vects.radiant.matrix[1], tx.vecs[1] );

		if(!VectorIsNull( origin ))
		{
			tx.vecs[0][3] += DotProduct( origin, tx.vecs[0] );
			tx.vecs[1][3] += DotProduct( origin, tx.vecs[1] );
		}

		dshaders[tx.shadernum].contentFlags = bt->contents;
		dshaders[tx.shadernum].surfaceFlags = bt->flags;
		tx.value = bt->value;
	}

	dshaders[tx.shadernum].contentFlags = bt->contents;
	dshaders[tx.shadernum].surfaceFlags = bt->flags;
	tx.value = bt->value;

	// find the texinfo
	tc = texinfo;
	for( i = 0; i < numtexinfo; i++, tc++ )
	{
		if( tc->value != tx.value ) continue;
		for( j = 0; j < 2; j++ )
		{
			if( tc->shadernum != tx.shadernum )
				goto skip;
			for( k = 0; k < 4; k++ )
			{
				if( tc->vecs[j][k] != tx.vecs[j][k] )
					goto skip;
			}
		}
		return i;
skip:;
	}

	// register new texinfo
	*tc = tx;
	return numtexinfo++;
}