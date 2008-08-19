
#include "bsplib.h"
#include "byteorder.h"

//==========================================================================
int FindMiptex( const char *name )
{
	int	i;
	string_t	texname;
	rgbdata_t	*image;

	if(!com.strlen( name )) return 0;

	// register new name, or get already existing
	texname = GetIndexFromTable( name );

	for( i = 0; i < nummiptex; i++ )
	{
		if( dmiptex[i].s_name == texname )
			return i;
	}

	// allocate a new texture
	if( nummiptex == MAX_MAP_TEXTURES ) Sys_Break( "MAX_MAP_TEXTURES limit exceeded\n" );

	// alloc new texture
	dmiptex[i].s_name = texname;

	image = FS_LoadImage( name, NULL, 0 );
	if( image )
	{
		// save texture dimensions that can be used
		// for replacing original textures with HQ images
		dmiptex[i].size[0] = image->width;		
		dmiptex[i].size[1] = image->height;
		FS_FreeImage( image );
	}
	else dmiptex[i].size[0] = dmiptex[i].size[1] = -1; // technically an error

	return nummiptex++;
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

	memset( &tx, 0, sizeof( tx ));
	tx.texnum = FindMiptex( bt->name );

	if( bt->txcommand )
	{
		Mem_Copy( tx.vecs, bt->vects.quark.vects, sizeof( tx.vecs ));

		if(!VectorIsNull( origin ))
		{
			tx.vecs[0][3] += DotProduct( origin, tx.vecs[0] );
			tx.vecs[1][3] += DotProduct( origin, tx.vecs[1] );
		}
		tx.contents = bt->contents;
		tx.flags = bt->flags;
		tx.value = bt->value;
	}
	else
	{
		if( g_brushtype == BRUSH_WORLDCRAFT_21 ) TextureAxisFromPlane( plane, vecs[0], vecs[1] );
		if( !bt->vects.valve.scale[0] ) bt->vects.valve.scale[0] = 1;
		if( !bt->vects.valve.scale[1] ) bt->vects.valve.scale[1] = 1;

		if( g_brushtype == BRUSH_WORLDCRAFT_21 )
		{
			// rotate axis
			if( bt->vects.valve.rotate == 0 )
			{
				sinv = 0;
				cosv = 1;
			}
			else if( bt->vects.valve.rotate == 90 )
			{
				sinv = 1;
				cosv = 0;
			}
			else if( bt->vects.valve.rotate == 180 )
			{
				sinv = 0;
				cosv = -1;
			}
			else if( bt->vects.valve.rotate == 270 )
			{
				sinv = -1;
				cosv = 0;
			}
			else
			{
				ang = bt->vects.valve.rotate / 180 * M_PI;
				sinv = sin( ang );
				cosv = cos( ang );
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
					tx.vecs[i][j] = vecs[i][j] / bt->vects.valve.scale[i];
		}
		else
		{
			vec_t scale;

			scale = 1 / bt->vects.valve.scale[0];
			VectorScale(bt->vects.valve.UAxis, scale, tx.vecs[0]);
			scale = 1 / bt->vects.valve.scale[1];
			VectorScale(bt->vects.valve.VAxis, scale, tx.vecs[1]);
		}

		tx.vecs[0][3] = bt->vects.valve.shift[0] + DotProduct(origin, tx.vecs[0]);
		tx.vecs[1][3] = bt->vects.valve.shift[1] + DotProduct(origin, tx.vecs[1]);
		tx.flags = bt->flags;
		tx.value = bt->value;
	}

	// find the texinfo
	tc = texinfo;
	for( i = 0; i < numtexinfo; i++, tc++ )
	{
		if( tc->flags != tx.flags ) continue;
		if( tc->value != tx.value ) continue;
		for( j = 0; j < 2; j++ )
		{
			if( tc->texnum != tx.texnum ) goto skip;
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

brush_texture_t *CopyTexture( brush_texture_t *bt )
{
	brush_texture_t *newbt = BSP_Malloc(sizeof( brush_texture_t ));
	Mem_Copy( newbt, bt, sizeof( brush_texture_t ));
	return newbt;
}

void FreeTexture( brush_texture_t *bt )
{
	return; //FIXME

	if(*(uint *)bt == 0xdeaddead )
		Sys_Error( "FreeTexture: freed a freed texture\n" );
	*(uint *)bt = 0xdeaddead;

	Mem_Free( bt );
}