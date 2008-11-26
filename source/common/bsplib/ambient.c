//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     ambient.c - calcualte ambient sounds level
//=======================================================================

#include "bsplib.h"

/*
Some textures (sky, water, slime, lava) are considered ambient sound emiters.
Find an aproximate distance to the nearest emiter of each class for each leaf.
*/

#define MIN_AMBIENT_DIST		128
#define MAX_AMBIENT_DIST		1024

/*
====================
SurfaceBBox
====================
*/
void SurfaceBBox( dsurface_t *s, vec3_t mins, vec3_t maxs )
{
	int	i, e, vi;
	vec3_t	v;

	ClearBounds( mins, maxs );

	for( i = 0; i < s->numedges; i++ )
	{
		e = dsurfedges[s->firstedge+i];
		if( e >= 0 ) vi = dedges[e].v[0];
		else vi = dedges[-e].v[1];

		VectorCopy( dvertexes[vi].point, v );
		AddPointToBounds( v, mins, maxs );
	}
}

void AmbientForLeaf( dleaf_t *leaf, float *dists )
{
	int	j;
	float	vol;
	
	for( j = 0; j < NUM_AMBIENTS; j++ )
	{
		if( dists[j] < MIN_AMBIENT_DIST ) vol = 1.0;
		else if( dists[j] < MAX_AMBIENT_DIST )
		{
				vol = 1.0f - (dists[j] / MAX_AMBIENT_DIST);
				vol = bound( 0.0f, vol, 1.0f );
		}
		else vol = 0.0f;
		leaf->sounds[j] = (byte)(vol * 255);
	}
}

/*
====================
CalcAmbientSounds

FIXME: make work
====================
*/
void CalcAmbientSounds( void )
{
	int		i, j, k, l;
	dleaf_t		*leaf, *hit;
	byte		*vis;
	dsurface_t	*surf;
	vec3_t		mins, maxs;
	float		d, maxd;
	int		ambient_type;
	dtexinfo_t	*info;
	dshader_t		*shader;
	float		dists[NUM_AMBIENTS];

	Msg( "---- CalcAmbientSounds ----\n" );

	for( i = 0; i < portalclusters; i++ )
	{
		leaf = &dleafs[i+1];

		// clear ambients
		for( j = 0; j < NUM_AMBIENTS; j++ )
			dists[j] = MAX_AMBIENT_DIST;
		vis = PhsForCluster( i );

		for( j = 0; j < portalclusters; j++ )
		{
			if(!(vis[j>>3] & (1<<(j & 7))))
				continue;

			// check this leaf for sound textures
			hit = &dleafs[j+1];

			for( k = 0; k < hit->numleafsurfaces; k++ )
			{
				surf = &dsurfaces[dleafsurfaces[hit->firstleafsurface + k]];
				info = &texinfo[surf->texinfo];
				shader = &dshaders[info->shadernum];

				if( shader->contentFlags & CONTENTS_SKY )
					ambient_type = AMBIENT_SKY;
				else if( shader->contentFlags & CONTENTS_SOLID )
					continue;
				else if( shader->contentFlags & CONTENTS_WATER )
					ambient_type = AMBIENT_WATER;
				else if( shader->contentFlags & CONTENTS_SLIME )
					ambient_type = AMBIENT_SLIME;
				else if( shader->contentFlags & CONTENTS_LAVA )
					ambient_type = AMBIENT_LAVA;
				else continue;

				// find distance from source leaf to polygon
				SurfaceBBox( surf, mins, maxs );
				for( l = maxd = 0; l < 3; l++ )
				{
					if (maxs[l] > leaf->maxs[l] )
						d = maxs[l] - leaf->maxs[l];
					else if( mins[l] < leaf->mins[l] )
						d = leaf->mins[l] - mins[l];
					else d = 0;
					if( d > maxd ) maxd = d;
				}
				if( maxd < dists[ambient_type] )
					dists[ambient_type] = maxd;
			}
		}
		AmbientForLeaf( leaf, dists );
	}
}