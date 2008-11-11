#include "bsplib.h"
#include "const.h"

vec3_t	texture_reflectivity[MAX_MAP_SHADERS];

/*
===================================================================

  TEXTURE LIGHT VALUES

===================================================================
*/

/*
======================
CalcTextureReflectivity
======================
*/
void CalcTextureReflectivity( void )
{
	int		j, i;
	rgbdata_t		*pic;
	bsp_shader_t	*si;
	
	// allways set index 0 even if no textures
	texture_reflectivity[0][0] = 0.5;
	texture_reflectivity[0][1] = 0.5;
	texture_reflectivity[0][2] = 0.5;

	for( i = 0; i < numtexinfo; i++ )
	{
		// see if an earlier texinfo allready got the value
		for (j = 0; j < i; j++)
		{
			if( texinfo[i].shadernum == texinfo[j].shadernum )
			{
				VectorCopy( texture_reflectivity[j], texture_reflectivity[i] );
				break;
			}
		}
		if( j != i ) continue;

		// try get direct values from shader
		if( si = FindShader( dshaders[texinfo[i].shadernum].name ))
		{						
			if( !VectorIsNull( si->color ))
			{
				texture_reflectivity[i][0] = si->color[0] / 255.0f;
				texture_reflectivity[i][1] = si->color[1] / 255.0f;
				texture_reflectivity[i][2] = si->color[2] / 255.0f;
				texinfo[i].value = si->intensity;
				continue;
			}
		}

		pic = FS_LoadImage( dshaders[texinfo[i].shadernum].name, NULL, 0 );

		if( pic )
		{
			// create default and average colors
			int	k, texels = pic->width * pic->height;
			float	r, scale;
			vec3_t	color;

			VectorClear( color );
			Com_Assert( pic->type != PF_RGBA_32 );

			for( k = 0; k < texels; k++ )
			{
				color[0] += pic->buffer[k*4+0];
				color[1] += pic->buffer[k*4+1];
				color[2] += pic->buffer[k*4+2];
			}
			
			for( k = 0; k < 3; k++ )
			{
				r = color[k] / texels;
				color[k] = r;
			}

			// scale the reflectivity up, because the textures are so dim
			scale = ColorNormalize( color, texture_reflectivity[i] );
			texinfo[i].value = texels * 255.0 / scale; // basic intensity value
          		FS_FreeImage( pic ); // don't forget free image
		}			
		else VectorSet( texture_reflectivity[i], 0.5, 0.5, 0.5 );  // no texture, no shader...
	}
}

/*
=======================================================================

MAKE FACES

=======================================================================
*/

/*
=============
WindingFromFace
=============
*/
winding_t	*WindingFromFace( dsurface_t *f )
{
	int		i;
	int		se;
	dvertex_t		*dv;
	int		v;
	winding_t		*w;

	w = AllocWinding( f->numedges );
	w->numpoints = f->numedges;

	for( i = 0; i < f->numedges; i++ )
	{
		se = dsurfedges[f->firstedge + i];
		if (se < 0)
			v = dedges[-se].v[1];
		else
			v = dedges[se].v[0];

		dv = &dvertexes[v];
		VectorCopy (dv->point, w->p[i]);
	}

	RemoveColinearPoints (w);

	return w;
}

/*
=============
BaseLightForFace
=============
*/
void BaseLightForFace( dsurface_t *f, vec3_t color )
{
	dshader_t		*si;
	dtexinfo_t	*tx;
	
	//
	// check for light emited by texture
	//
	tx = &texinfo[f->texinfo];
	si = &dshaders[tx->shadernum];
	if(!(si->surfaceFlags & SURF_LIGHT ) || tx->value == 0 )
	{
		VectorClear( color );
		return;
	}
	VectorScale( texture_reflectivity[f->texinfo], tx->value, color );
}

bool IsSky( dsurface_t *f )
{
	dshader_t	*tx;

	tx = &dshaders[texinfo[f->texinfo].shadernum];
	if( tx->surfaceFlags & SURF_SKY )
		return true;
	return false;
}

/*
=============
MakePatchForFace
=============
*/
float	totalarea;
void MakePatchForFace( int fn, winding_t *w )
{
	dsurface_t	*f;
	float		area;
	patch_t		*patch;
	dplane_t	*pl;
	int			i;
	vec3_t		color;
	dleaf_t		*leaf;

	f = &dsurfaces[fn];

	area = WindingArea( w );
	totalarea += area;

	patch = &patches[num_patches];
	if( num_patches == MAX_PATCHES )
		Sys_Error( "MAX_PATCHES limit exceeded\n" );
	patch->next = face_patches[fn];
	face_patches[fn] = patch;

	patch->winding = w;

	if( f->side ) patch->plane = &backplanes[f->planenum];
	else patch->plane = &dplanes[f->planenum];

	if( face_offset[fn][0] || face_offset[fn][1] || face_offset[fn][2] )
	{
		// origin offset faces must create new planes
		if( numplanes + fakeplanes >= MAX_MAP_PLANES )
			Sys_Error ("numplanes + fakeplanes >= MAX_MAP_PLANES");
		pl = &dplanes[numplanes + fakeplanes];
		fakeplanes++;

		*pl = *(patch->plane);
		pl->dist += DotProduct (face_offset[fn], pl->normal);
		patch->plane = pl;
	}

	WindingCenter (w, patch->origin);
	VectorAdd (patch->origin, patch->plane->normal, patch->origin);
	leaf = RadPointInLeaf(patch->origin);
	patch->cluster = leaf->cluster;

	patch->area = area;
	if (patch->area <= 1)
		patch->area = 1;
	patch->sky = IsSky (f);

	VectorCopy (texture_reflectivity[f->texinfo], patch->reflectivity);

	// non-bmodel patches can emit light
	if (fn < dmodels[0].numsurfaces)
	{
		BaseLightForFace (f, patch->baselight);

		ColorNormalize (patch->reflectivity, color);

		for (i=0 ; i<3 ; i++)
			patch->baselight[i] *= color[i];

		VectorCopy (patch->baselight, patch->totallight);
	}
	num_patches++;
}


bsp_entity_t *EntityForModel( int modnum )
{
	char	*s, name[16];
	int	i;

	com.sprintf( name, "*%i", modnum );

	// search the entities for one using modnum
	for( i = 0; i < num_entities; i++ )
	{
		s = ValueForKey( &entities[i], "model" );
		if( !com.strcmp( s, name ))
			return &entities[i];
	}
	return &entities[0];
}

/*
=============
MakePatches
=============
*/
void MakePatches( void )
{
	int		i, j, k;
	dsurface_t	*f;
	int		fn;
	winding_t		*w;
	dmodel_t		*mod;
	vec3_t		origin;
	bsp_entity_t	*ent;

	Msg( "%i faces\n", numsurfaces );

	for( i = 0; i < nummodels; i++ )
	{
		mod = &dmodels[i];
		ent = EntityForModel (i);

		// bmodels with origin brushes need to be offset into their in-use position
		GetVectorForKey( ent, "origin", origin );

		for( j = 0; j < mod->numsurfaces; j++ )
		{
			fn = mod->firstsurface + j;
			face_entity[fn] = ent;
			VectorCopy( origin, face_offset[fn] );
			f = &dsurfaces[fn];
			w = WindingFromFace( f );
			for( k = 0; k < w->numpoints; k++ )
				VectorAdd( w->p[k], origin, w->p[k] );
			MakePatchForFace( fn, w );
		}
	}

	Msg( "%i sqaure feet\n", (int)( totalarea/64 ));
}

/*
=======================================================================

SUBDIVIDE

=======================================================================
*/

void FinishSplit (patch_t *patch, patch_t *newp)
{
	dleaf_t		*leaf;

	VectorCopy (patch->baselight, newp->baselight);
	VectorCopy (patch->totallight, newp->totallight);
	VectorCopy (patch->reflectivity, newp->reflectivity);
	newp->plane = patch->plane;
	newp->sky = patch->sky;

	patch->area = WindingArea (patch->winding);
	newp->area = WindingArea (newp->winding);

	if (patch->area <= 1)
		patch->area = 1;
	if (newp->area <= 1)
		newp->area = 1;

	WindingCenter (patch->winding, patch->origin);
	VectorAdd (patch->origin, patch->plane->normal, patch->origin);
	leaf = RadPointInLeaf(patch->origin);
	patch->cluster = leaf->cluster;

	WindingCenter (newp->winding, newp->origin);
	VectorAdd (newp->origin, newp->plane->normal, newp->origin);
	leaf = RadPointInLeaf(newp->origin);
	newp->cluster = leaf->cluster;
}

/*
=============
SubdividePatch

Chops the patch only if its local bounds exceed the max size
=============
*/
void	SubdividePatch (patch_t *patch)
{
	winding_t *w, *o1, *o2;
	vec3_t	mins, maxs, total;
	vec3_t	split;
	vec_t	dist;
	int		i, j;
	vec_t	v;
	patch_t	*newp;

	w = patch->winding;
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
	for (i=0 ; i<w->numpoints ; i++)
	{
		for (j=0 ; j<3 ; j++)
		{
			v = w->p[i][j];
			if (v < mins[j])
				mins[j] = v;
			if (v > maxs[j])
				maxs[j] = v;
		}
	}
	VectorSubtract (maxs, mins, total);
	for (i=0 ; i<3 ; i++)
		if (total[i] > (subdiv+1) )
			break;
	if (i == 3)
	{
		// no splitting needed
		return;		
	}

	//
	// split the winding
	//
	VectorCopy (vec3_origin, split);
	split[i] = 1;
	dist = (mins[i] + maxs[i])*0.5;
	ClipWindingEpsilon (w, split, dist, ON_EPSILON, &o1, &o2);

	//
	// create a new patch
	//
	if (num_patches == MAX_PATCHES)
		Sys_Error ("MAX_PATCHES");
	newp = &patches[num_patches];
	num_patches++;

	newp->next = patch->next;
	patch->next = newp;

	patch->winding = o1;
	newp->winding = o2;

	FinishSplit (patch, newp);

	SubdividePatch (patch);
	SubdividePatch (newp);
}


/*
=============
DicePatch

Chops the patch by a global grid
=============
*/
void	DicePatch (patch_t *patch)
{
	winding_t *w, *o1, *o2;
	vec3_t	mins, maxs;
	vec3_t	split;
	vec_t	dist;
	int		i;
	patch_t	*newp;

	w = patch->winding;
	WindingBounds (w, mins, maxs);
	for (i=0 ; i<3 ; i++)
		if (floor((mins[i]+1)/subdiv) < floor((maxs[i]-1)/subdiv))
			break;
	if (i == 3)
	{
		// no splitting needed
		return;		
	}

	//
	// split the winding
	//
	VectorCopy (vec3_origin, split);
	split[i] = 1;
	dist = subdiv*(1+floor((mins[i]+1)/subdiv));
	ClipWindingEpsilon (w, split, dist, ON_EPSILON, &o1, &o2);

	//
	// create a new patch
	//
	if (num_patches == MAX_PATCHES)
		Sys_Error ("MAX_PATCHES");
	newp = &patches[num_patches];
	num_patches++;

	newp->next = patch->next;
	patch->next = newp;

	patch->winding = o1;
	newp->winding = o2;

	FinishSplit (patch, newp);

	DicePatch (patch);
	DicePatch (newp);
}


/*
=============
SubdividePatches
=============
*/
void SubdividePatches (void)
{
	int		i, num;

	if (subdiv < 1)
		return;

	num = num_patches;	// because the list will grow
	for (i=0 ; i<num ; i++)
	{
//		SubdividePatch (&patches[i]);
		DicePatch (&patches[i]);
	}
}