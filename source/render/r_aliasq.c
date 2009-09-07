//=======================================================================
//			Copyright XashXT Group 2008 ©
//		   r_alias.c - Quake1 models loading & drawing
//=======================================================================

#include "r_local.h"
#include "mathlib.h"
#include "byteorder.h"

static mesh_t alias_mesh;

static float	alias_mins[3];
static float	alias_maxs[3];
static float	alias_radius;
maliashdr_t	*pheader;

float	vertst[MAXALIASVERTS*2][2];
int	vertusage[MAXALIASVERTS*2];
int	vertonseam[MAXALIASVERTS*2];
int	vertremap[MAXALIASVERTS*2];
int	temptris[MAXALIASVERTS*2][3];

typedef struct
{
	char		name[16];	// frame name
	daliastrivertx_t	*data;		// frame data
} aliasframe_t;

aliasframe_t	frames[MAXALIASFRAMES];
int		numframes;

/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes - Ed
=================
*/

typedef struct
{
	short	x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE	0x1000
#define FLOODFILL_FIFO_MASK	(FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy )				\
{							\
	if( pos[off] == fillcolor )				\
	{						\
		pos[off] = 255;				\
		fifo[inpt].x = x + ( dx );			\
		fifo[inpt].y = y + ( dy );			\
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;	\
	}						\
	else if( pos[off] != 255 ) fdc = pos[off];		\
}

/*
=================
Mod_AliasLoadFrame
=================
*/
void *Mod_AliasLoadFrame( void *pin, maliasframedesc_t *frame )
{
	daliastrivertx_t	*pinframe;
	daliasframe_t	*pdaliasframe;
	int		i;	

	pdaliasframe = (daliasframe_t *)pin;

	com.strncpy( frame->name, pdaliasframe->name, sizeof( frame->name ));
	frame->firstpose = numframes;
	frame->numposes = 1;

	for( i = 0; i < 3; i++ )
	{
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pdaliasframe->bboxmin.v[i];
		frame->bboxmin.v[i] = pdaliasframe->bboxmax.v[i];
	}

	pinframe = (daliastrivertx_t *)(pdaliasframe + 1);

	frames[numframes++].data = pinframe;
	pinframe += pheader->numverts;

	return (void *)pinframe;
}

/*
=================
Mod_AliasLoadGroup
=================
*/
void *Mod_AliasLoadGroup( void *pin,  maliasframedesc_t *frame )
{
	daliasgroup_t	*pingroup;
	int		i, numframes;
	daliasinterval_t	*pin_intervals;
	void		*ptemp;
	
	pingroup = (daliasgroup_t *)pin;
	numframes = LittleLong( pingroup->numframes );

	frame->firstpose = numframes;
	frame->numposes = numframes;

	for( i = 0; i < 3; i++ )
	{
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pingroup->bboxmin.v[i];
		frame->bboxmin.v[i] = pingroup->bboxmax.v[i];
	}

	pin_intervals = (daliasinterval_t *)(pingroup + 1);
	frame->interval = LittleFloat( pin_intervals->interval );
	pin_intervals += numframes;
	ptemp = (void *)pin_intervals;

	for( i = 0; i < numframes; i++ )
	{
		frames[numframes++].data = (daliastrivertx_t *)((daliasframe_t *)ptemp + 1);
		ptemp = (daliastrivertx_t *)((daliasframe_t *)ptemp + 1) + pheader->numverts;
	}
	return ptemp;
}

void Mod_FloodFillSkin( byte *skin, int skinwidth, int skinheight )
{
	byte		fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t	fifo[FLOODFILL_FIFO_SIZE];
	int		inpt = 0, outpt = 0;
	int		filledcolor = -1;
	rgbdata_t		*pal = FS_LoadImage( "#quake1.pal", skin, 768 );
	uint		*d_8to24table = (uint *)pal->palette;
	int		i;

	if( filledcolor == -1 )
	{
		filledcolor = 0;
		// attempt to find opaque black
		for( i = 0; i < 256; ++i )
		{
			if( d_8to24table[i] == (255<<0)) // alpha 1.0
			{
				filledcolor = i;
				break;
			}
		}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if(( fillcolor == filledcolor ) || ( fillcolor == 255 )) return;

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while( outpt != inpt )
	{
		int	x = fifo[outpt].x, y = fifo[outpt].y;
		int	fdc = filledcolor;
		byte	*pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if( x > 0 ) FLOODFILL_STEP( -1, -1, 0 );
		if( x < skinwidth - 1 )  FLOODFILL_STEP( 1, 1, 0 );
		if( y > 0 ) FLOODFILL_STEP( -skinwidth, 0, -1 );
		if( y < skinheight - 1 ) FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
	FS_FreeImage( pal );
}

/*
===============
Mod_AliasLoadSkins
===============
*/
void *Mod_AliasLoadSkins( ref_model_t *mod, int numskins, daliasskintype_t *pskintype )
{
	int			i, j, k, s;
	string			shadername;
	string			modname;
	byte			*skin;
	int			groupskins;
	daliasskingroup_t		*pinskingroup;
	daliasskininterval_t	*pinskinintervals;
	dstudiotexture_t		*ptexture;	// apply studio header for alias texture	
	texture_t			*tex;

	FS_FileBase( mod->name, modname );	
	skin = (byte *)(pskintype + 1);

	if( numskins < 1 || numskins > MAX_SKINS )
		Host_Error( "Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins );

	s = pheader->skinwidth * pheader->skinheight;
	ptexture = Mod_Malloc( mod, sizeof( *ptexture ));
	ptexture->height = pheader->skinheight;
	ptexture->width = pheader->skinwidth;
	ptexture->flags = STUDIO_NF_QUAKESKIN;	// indicates alias models

	for( i = 0; i < numskins; i++ )
	{
		if( pskintype->type == SKIN_SINGLE )
		{
			Mod_FloodFillSkin( skin, pheader->skinwidth, pheader->skinheight );

			com.sprintf( shadername, "%s.mdl/%s_%i.bmp", modname, modname, i );
			com.sprintf( ptexture->name, "Alias( %s_%i )", modname, i );
			ptexture->index = (int)skin;// don't copy, just set ptr
			tex = R_FindTexture( ptexture->name, (byte *)ptexture, s, 0 );
			R_ShaderAddStageTexture( tex );
			pheader->skins[i][0] = pheader->skins[i][1] = pheader->skins[i][2] =
			pheader->skins[i][3] = R_LoadShader( shadername, SHADER_ALIAS, false, tex->flags, SHADER_INVALID );
			pskintype = (daliasskintype_t *)((byte *)(pskintype + 1) + s );
		}
		else
		{
			// animating skin group.  yuck.
			pskintype++;
			pinskingroup = (daliasskingroup_t *)pskintype;
			groupskins = LittleLong( pinskingroup->numskins );
			pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

			pskintype = (void *)(pinskinintervals + groupskins);

			for( j = 0; j < groupskins; j++ )
			{
				skin = (byte *)(pskintype + 1);
				Mod_FloodFillSkin( skin, pheader->skinwidth, pheader->skinheight );
				com.sprintf( shadername, "%s.mdl/%s_%i_%i", modname, modname, i, j );
				com.sprintf( ptexture->name, "Alias( %s_%i_%i )", modname, i, j );
				ptexture->index = (int)skin;// don't copy, just set ptr
				tex = R_FindTexture( ptexture->name, (byte *)ptexture, s, 0 );
				R_ShaderAddStageTexture( tex );
				pheader->skins[i][j&3] = R_LoadShader( shadername, SHADER_ALIAS, false, tex->flags, SHADER_INVALID );
				pskintype = (daliasskintype_t *)((byte *)(pskintype) + s );
			}
			for( k = j; j < 4; j++ )
				pheader->skins[i][j&3] = pheader->skins[i][j - k]; 
		}
	}
	Mem_Free( ptexture );
	
	return (void *)pskintype;
}

/*
=================
Mod_AliasCalculateVertexNormals
=================
*/
static void Mod_AliasCalculateVertexNormals( int numElems, elem_t *elems, int numVerts, maliasvertex_t *v )
{
	int	i, j, k, vertRemap[MAXALIASVERTS];
	vec3_t	dir1, dir2, normal, trnormals[MAXALIASTRIS];
	int	numUniqueVerts, uniqueVerts[MAXALIASVERTS];
	byte	latlongs[MAXALIASVERTS][2];

	// count unique verts
	for( i = 0, numUniqueVerts = 0; i < numVerts; i++ )
	{
		for( j = 0; j < numUniqueVerts; j++ )
		{
			if( VectorCompare( v[uniqueVerts[j]].point, v[i].point ))
			{
				vertRemap[i] = j;
				break;
			}
		}

		if( j == numUniqueVerts )
		{
			vertRemap[i] = numUniqueVerts;
			uniqueVerts[numUniqueVerts++] = i;
		}
	}

	for( i = 0, j = 0; i < numElems; i += 3, j++ )
	{
		// calculate two mostly perpendicular edge directions
		VectorSubtract( v[elems[i+0]].point, v[elems[i+1]].point, dir1 );
		VectorSubtract( v[elems[i+2]].point, v[elems[i+1]].point, dir2 );

		// we have two edge directions, we can calculate a third vector from
		// them, which is the direction of the surface normal
		CrossProduct( dir1, dir2, trnormals[j] );
		VectorNormalize( trnormals[j] );
	}

	// sum all triangle normals
	for( i = 0; i < numUniqueVerts; i++ )
	{
		VectorClear( normal );

		for( j = 0, k = 0; j < numElems; j += 3, k++ )
		{
			if( vertRemap[elems[j+0]] == i || vertRemap[elems[j+1]] == i || vertRemap[elems[j+2]] == i )
				VectorAdd( normal, trnormals[k], normal );
		}

		VectorNormalize( normal );
		NormToLatLong( normal, latlongs[i] );
	}

	// copy normals back
	for( i = 0; i < numVerts; i++ )
		*(short *)v[i].latlong = *(short *)latlongs[vertRemap[i]];
}

/*
=================
Mod_AliasBuildMeshesForFrame0
=================
*/
static void Mod_AliasBuildMeshesForFrame0( ref_model_t *mod )
{
	int		i, j, k;
	size_t		size;
	maliasframe_t	*frame;
	maliasmodel_t	*aliasmodel = (maliasmodel_t *)mod->extradata;

	frame = &aliasmodel->frames[0];
	for( k = 0; k < aliasmodel->nummeshes; k++ )
	{
		maliasmesh_t *mesh = &aliasmodel->meshes[k];

		size = sizeof( vec4_t ) + sizeof( vec4_t );			// xyz and normals
		if( GL_Support( R_SHADER_GLSL100_EXT )) size += sizeof( vec4_t );	// s-vectors
		size *= mesh->numverts;

		mesh->xyzArray = ( vec4_t * )Mod_Malloc( mod, size );
		mesh->normalsArray = ( vec4_t * )( ( byte * )mesh->xyzArray + mesh->numverts * sizeof( vec4_t ));
		if( GL_Support( R_SHADER_GLSL100_EXT ))
			mesh->sVectorsArray = (vec4_t *)((byte *)mesh->normalsArray + mesh->numverts * sizeof( vec4_t ));

		for( i = 0; i < mesh->numverts; i++ )
		{
			for( j = 0; j < 3; j++ )
				mesh->xyzArray[i][j] = frame->translate[j] + frame->scale[j] * mesh->vertexes[i].point[j];
			mesh->xyzArray[i][3] = 1;
			R_LatLongToNorm( mesh->vertexes[i].latlong, mesh->normalsArray[i] );
			mesh->normalsArray[i][3] = 0;
		}

		if( GL_Support( R_SHADER_GLSL100_EXT ))
			R_BuildTangentVectors( mesh->numverts, mesh->xyzArray, mesh->normalsArray, mesh->stArray, mesh->numtris, mesh->elems, mesh->sVectorsArray );
	}
}

/*
=================
Mod_QAliasLoadModel
=================
*/
void Mod_QAliasLoadModel( ref_model_t *mod, const void *buffer )
{
	int		i, j, k;
	daliashdr_t	*pinmodel;
	daliastexcoord_t	*pinst;
	daliastriangle_t	*pintri;
	daliasframetype_t	*pframetype;
	daliasskintype_t	*pskintype;
	int		version, numverts, numelems;
	float		scale_s, scale_t;
	maliasmodel_t	*poutmodel;
	maliasmesh_t	*poutmesh;
	elem_t		*poutelem;
	vec2_t		*poutcoord;
	maliasframe_t	*poutframe;
	maliasvertex_t	*poutvertex;
	maliasskin_t	*poutskin;

	pinmodel = (daliashdr_t *)buffer;

	version = LittleLong( pinmodel->version );
	if( version != QALIAS_VERSION )
		Host_Error( "%s has wrong version number (%i should be %i)\n", mod->name, version, QALIAS_VERSION );

	// allocate space for a working header, plus all the data except the frames,
	// skin and group info
	pheader = Mod_Malloc( mod, sizeof( maliashdr_t ) + (LittleLong( pinmodel->numframes ) - 1) * sizeof( pheader->frames[0] ));
	mod->extradata = poutmodel = Mod_Malloc( mod, sizeof( maliasmodel_t ) );
	mod->type = mod_alias;
	
	// endian-adjust and copy the data, starting with the alias model header
	pheader->flags = LittleLong( pinmodel->flags );
	pheader->boundingradius = LittleFloat( pinmodel->boundingradius );
	poutmodel->numskins = pheader->numskins = LittleLong( pinmodel->numskins );
	pheader->skinwidth = LittleLong( pinmodel->skinwidth );
	pheader->skinheight = LittleLong( pinmodel->skinheight );

	if( pheader->skinheight > MAX_LBM_HEIGHT || pheader->skinwidth > MAX_LBM_WIDTH )
		Host_Error( "model %s has a skin taller than %dx%d\n", mod->name, MAX_LBM_WIDTH, MAX_LBM_HEIGHT );

	pheader->numframes = LittleLong( pinmodel->numframes );
	if( pheader->numframes < 1 ) Host_Error( "Mod_LoadAliasModel: model %s has no frames\n", mod->name );
	if( pheader->numframes > MAXALIASFRAMES ) Host_Error( "Mod_LoadAliasModel: model %s has too many frames\n", mod->name );

	pheader->size = LittleFloat( pinmodel->size ) * (1.0f / 11.0f);
	pheader->synctype = LittleLong( pinmodel->synctype );

	poutmodel->nummeshes = 1;

	poutmesh = poutmodel->meshes = Mod_Malloc( mod, sizeof( maliasmesh_t ));
	com.strncpy( poutmesh->name, "default", MAX_SHADERPATH );

	for( i = 0; i < 3; i++ )
	{
		pheader->scale[i] = LittleFloat( pinmodel->scale[i] );
		pheader->scale_origin[i] = LittleFloat( pinmodel->scale_origin[i] );
		pheader->eyeposition[i] = LittleFloat( pinmodel->eyeposition[i] );
	}

	poutmesh->numverts = pheader->numverts = LittleLong( pinmodel->numverts );
	if( pheader->numverts <= 0 ) Host_Error( "model %s has no vertices\n", mod->name );
	if( pheader->numverts > MAXALIASVERTS ) Host_Error( "model %s has too many vertices\n", mod->name );
	poutmesh->numtris = pheader->numtris = LittleLong( pinmodel->numtris );
	if( pheader->numtris <= 0 ) Host_Error( "model %s has no triangles\n", mod->name );
	if( pheader->numtris > MAXALIASTRIS ) Host_Error( "model %s has too many triangles\n", mod->name );

	// load the skins
	pskintype = (daliasskintype_t *)&pinmodel[1];
	pskintype = Mod_AliasLoadSkins( mod, pheader->numskins, pskintype );

	numelems = poutmesh->numtris * 3;
	poutelem = poutmesh->elems = Mod_Malloc( mod, numelems * sizeof( elem_t ));

	// load triangle lists
	pinst = (daliastexcoord_t *)pskintype;
	pintri = (daliastriangle_t *)&pinst[pheader->numverts];

	scale_s = 1.0f / pheader->skinwidth;
	scale_t = 1.0f / pheader->skinheight;
	for( i = 0; i < pheader->numverts; i++ )
	{
		vertonseam[i] = LittleLong( pinst[i].onseam );
		vertst[i][0] = LittleLong(pinst[i].s ) * scale_s;
		vertst[i][1] = LittleLong(pinst[i].t ) * scale_t;
		vertst[i+pheader->numverts][0] = vertst[i][0] + 0.5f;
		vertst[i+pheader->numverts][1] = vertst[i][1];
		vertusage[i] = 0;
		vertusage[i+pheader->numverts] = 0;
	}

	// count the vertices used
	for( i = 0; i < pheader->numverts * 2; i++ )
		vertusage[i] = 0;
	for( i = 0; i < pheader->numtris; i++ )
	{
		temptris[i][0] = LittleLong( pintri[i].vertindex[0] );
		temptris[i][1] = LittleLong( pintri[i].vertindex[1] );
		temptris[i][2] = LittleLong( pintri[i].vertindex[2] );
		if( !LittleLong( pintri[i].facesfront )) // backface
		{
			if( vertonseam[temptris[i][0]] ) temptris[i][0] += poutmesh->numtris;
			if( vertonseam[temptris[i][1]] ) temptris[i][1] += poutmesh->numtris;
			if( vertonseam[temptris[i][2]] ) temptris[i][2] += poutmesh->numtris;
		}
		vertusage[temptris[i][0]]++;
		vertusage[temptris[i][1]]++;
		vertusage[temptris[i][2]]++;
	}

	// build remapping table and compact array
	numverts = 0;
	for( i = 0; i < pheader->numverts * 2; i++ )
	{
		if( vertusage[i] )
		{
			vertremap[i] = numverts;
			vertst[numverts][0] = vertst[i][0];
			vertst[numverts][1] = vertst[i][1];
			numverts++;
		}
		else vertremap[i] = -1; // not used at all
	}

	// remap the triangle references
	for( i = 0; i < poutmesh->numtris; i++ )
	{
		poutelem[i*3+0] = vertremap[temptris[i][0]];
		poutelem[i*3+1] = vertremap[temptris[i][1]];
		poutelem[i*3+2] = vertremap[temptris[i][2]];
	}

	Msg( "%s: remapped %i verts to %i (%i tris)\n", mod->name, pheader->numverts, numverts, poutmesh->numtris );
	poutmesh->numverts = numverts;

	// load base s and t vertices
	poutcoord = poutmesh->stArray = Mod_Malloc( mod, poutmesh->numverts * sizeof( vec2_t ));
	for( i = 0; i < poutmesh->numverts; i++ )
	{
		poutcoord[i][0] = vertst[i][0];
		poutcoord[i][1] = vertst[i][1];
	}

	// load the frames
	numframes = 0;
	pframetype = (daliasframetype_t *)&pintri[pheader->numtris];

	for( i = 0; i < pheader->numframes; i++ )
	{
		frametype_t	frametype;

		frametype = LittleLong( pframetype->type );

		switch( frametype )
		{
		case FRAME_SINGLE:
			pframetype = (daliasframetype_t *)Mod_AliasLoadFrame( pframetype+1, &pheader->frames[i] );
			break;
		case FRAME_GROUP:
			pframetype = (daliasframetype_t *)Mod_AliasLoadGroup( pframetype+1, &pheader->frames[i] );
			break;
		}
		if( pframetype == NULL ) break; // technically an error
	}

	poutmodel->numframes = pheader->numposes = numframes;
	poutframe = poutmodel->frames = Mod_Malloc( mod, poutmodel->numframes * ( sizeof( maliasframe_t ) + numverts * sizeof( maliasvertex_t )));
	poutvertex = poutmesh->vertexes = ( maliasvertex_t *)((byte *)poutframe + poutmodel->numframes * sizeof( maliasframe_t ));

	for( i = 0; i < poutmodel->numframes; i++, poutframe++, poutvertex += poutmesh->numverts )
	{
		for( j = 0; j < 3; j++ )
		{
			poutframe->scale[j] = LittleFloat( pinmodel->scale[j] );
			poutframe->translate[j] = LittleFloat( pinmodel->scale_origin[j] );
		}
		for( j = 0; j < pheader->numverts; j++ )
		{
			// verts are all 8 bit, so no swapping needed
			if( vertremap[j] < 0 && vertremap[j+pheader->numverts] < 0 )
				continue;
			k = vertremap[j]; // not onseam
			if( k >= 0 )
			{
				poutvertex[k].point[0] = (short)frames[i].data[j].v[0];
				poutvertex[k].point[1] = (short)frames[i].data[j].v[1];
				poutvertex[k].point[2] = (short)frames[i].data[j].v[2];
			}
			k = vertremap[j+pheader->numverts]; // onseam
			if( k >= 0 )
			{
				poutvertex[k].point[0] = (short)frames[i].data[j].v[0];
				poutvertex[k].point[1] = (short)frames[i].data[j].v[1];
				poutvertex[k].point[2] = (short)frames[i].data[j].v[2];
			}
		}

		Mod_AliasCalculateVertexNormals( numelems, poutelem, poutmesh->numverts, poutvertex );

		VectorCopy( poutframe->translate, poutframe->mins );
		VectorMA( poutframe->translate, 255, poutframe->scale, poutframe->maxs );
		poutframe->radius = RadiusFromBounds( poutframe->mins, poutframe->maxs );

		mod->radius = max( mod->radius, poutframe->radius );
		AddPointToBounds( poutframe->mins, mod->mins, mod->maxs );
		AddPointToBounds( poutframe->maxs, mod->mins, mod->maxs );
	}

	// build S and T vectors for frame 0
	Mod_AliasBuildMeshesForFrame0( mod );

	// register all skins
	poutskin = poutmodel->skins = Mod_Malloc( mod, poutmodel->numskins * sizeof( maliasskin_t ) );

	for( i = 0; i < poutmodel->numskins; i++, poutskin++ )
		poutskin->shader = pheader->skins[i][0];

	Mem_Free( pheader );	
	mod->touchFrame = tr.registration_sequence; // register model
}

/*
=============
R_AliasModelLerpBBox
=============
*/
static void R_AliasModelLerpBBox( ref_entity_t *e, ref_model_t *mod )
{
	int i;
	maliasmodel_t *aliasmodel = ( maliasmodel_t * )mod->extradata;
	maliasframe_t *pframe, *poldframe;
	float *thismins, *oldmins, *thismaxs, *oldmaxs;

	if( !aliasmodel->nummeshes )
	{
		alias_radius = 0;
		ClearBounds( alias_mins, alias_maxs );
		return;
	}

	if( ( e->frame >= aliasmodel->numframes ) || ( e->frame < 0 ) )
	{
		MsgDev( D_ERROR, "R_DrawAliasModel %s: no such frame %g\n", mod->name, e->frame );
		e->frame = 0;
	}
	if( ( e->prev.frame >= aliasmodel->numframes ) || ( e->prev.frame < 0 ) )
	{
		MsgDev( D_ERROR, "R_DrawAliasModel %s: no such oldframe %g\n", mod->name, e->prev.frame );
		e->prev.frame = 0;
	}

	pframe = aliasmodel->frames + (int)e->frame;
	poldframe = aliasmodel->frames + (int)e->prev.frame;

	// compute axially aligned mins and maxs
	if( pframe == poldframe )
	{
		VectorCopy( pframe->mins, alias_mins );
		VectorCopy( pframe->maxs, alias_maxs );
		alias_radius = pframe->radius;
	}
	else
	{
		thismins = pframe->mins;
		thismaxs = pframe->maxs;

		oldmins = poldframe->mins;
		oldmaxs = poldframe->maxs;

		for( i = 0; i < 3; i++ )
		{
			alias_mins[i] = min( thismins[i], oldmins[i] );
			alias_maxs[i] = max( thismaxs[i], oldmaxs[i] );
		}
		alias_radius = RadiusFromBounds( thismins, thismaxs );
	}

	if( e->scale != 1.0f )
	{
		VectorScale( alias_mins, e->scale, alias_mins );
		VectorScale( alias_maxs, e->scale, alias_maxs );
		alias_radius *= e->scale;
	}
}

/*
=============
R_DrawAliasFrameLerp

Interpolates between two frames and origins
=============
*/
static void R_DrawAliasFrameLerp( const meshbuffer_t *mb, float backlerp )
{
	int i, meshnum;
	int features;
	float backv[3], frontv[3];
	vec3_t normal, oldnormal;
	bool unlockVerts, calcVerts, calcNormals, calcSTVectors;
	vec3_t move;
	maliasframe_t *frame, *oldframe;
	maliasmesh_t *mesh;
	maliasvertex_t *v, *ov;
	ref_entity_t *e = RI.currententity;
	ref_model_t	*mod = Mod_ForHandle( mb->modhandle );
	maliasmodel_t *model = ( maliasmodel_t * )mod->extradata;
	ref_shader_t *shader;
	static maliasmesh_t *alias_prevmesh;
	static ref_shader_t *alias_prevshader;
	static int alias_framecount, alias_riparams;

	if( alias_riparams != RI.params || RI.params & RP_SHADOWMAPVIEW )
	{
		alias_riparams = RI.params; // do not try to lock arrays between RI updates
		alias_framecount = !r_framecount;
	}

	meshnum = -mb->infokey - 1;
	if( meshnum < 0 || meshnum >= model->nummeshes )
		return;
	mesh = model->meshes + meshnum;

	frame = model->frames + (int)e->frame;
	oldframe = model->frames + (int)e->prev.frame;
	for( i = 0; i < 3; i++ )
		move[i] = frame->translate[i] + ( oldframe->translate[i] - frame->translate[i] ) * backlerp;

	MB_NUM2SHADER( mb->shaderkey, shader );

	features = MF_NONBATCHED | shader->features;
	if( RI.params & RP_SHADOWMAPVIEW )
	{
		features &= ~( MF_COLORS|MF_SVECTORS|MF_ENABLENORMALS );
		if( !( shader->features & MF_DEFORMVS ) )
			features &= ~MF_NORMALS;
	}
	else
	{
		if( ( features & MF_SVECTORS ) || r_shownormals->integer )
			features |= MF_NORMALS;
		if( e->outlineHeight )
			features |= MF_NORMALS|(GL_Support( R_SHADER_GLSL100_EXT ) ? MF_ENABLENORMALS : 0);
	}

	calcNormals = calcSTVectors = false;
	calcNormals = (( features & MF_NORMALS ) != 0 ) && (( e->frame != 0 ) || ( e->prev.frame != 0 ));

	if( alias_framecount == r_framecount && RI.previousentity && RI.previousentity->model == e->model && alias_prevmesh == mesh && alias_prevshader == shader )
	{
		ref_entity_t *pe = RI.previousentity;
		if( pe->frame == e->frame && pe->prev.frame == e->prev.frame && ( pe->backlerp == e->backlerp || e->frame == e->prev.frame ))
		{
			unlockVerts = ((( features & MF_DEFORMVS )));
			calcNormals = ( calcNormals && ( shader->features & SHADER_DEFORM_NORMAL ));
		}
	}

	unlockVerts = true;
	calcSTVectors = ( ( features & MF_SVECTORS ) != 0 ) && calcNormals;

	alias_prevmesh = mesh;
	alias_prevshader = shader;
	alias_framecount = r_framecount;

	if( unlockVerts )
	{
		if( !e->frame && !e->prev.frame )
		{
			calcVerts = false;

			if( calcNormals )
			{
				v = mesh->vertexes;
				for( i = 0; i < mesh->numverts; i++, v++ )
					R_LatLongToNorm( v->latlong, inNormalsArray[i] );
			}
		}
		else if( e->frame == e->prev.frame )
		{
			calcVerts = true;

			for( i = 0; i < 3; i++ )
				frontv[i] = frame->scale[i];

			v = mesh->vertexes + (int)e->frame * mesh->numverts;
			for( i = 0; i < mesh->numverts; i++, v++ )
			{
				Vector4Set( inVertsArray[i],
					move[0] + v->point[0]*frontv[0],
					move[1] + v->point[1]*frontv[1],
					move[2] + v->point[2]*frontv[2], 1 );

				if( calcNormals )
					R_LatLongToNorm( v->latlong, inNormalsArray[i] );
			}
		}
		else
		{
			calcVerts = true;

			for( i = 0; i < 3; i++ )
			{
				backv[i] = backlerp * oldframe->scale[i];
				frontv[i] = ( 1.0f - backlerp ) * frame->scale[i];
			}

			v = mesh->vertexes + (int)e->frame * mesh->numverts;
			ov = mesh->vertexes + (int)e->prev.frame * mesh->numverts;
			for( i = 0; i < mesh->numverts; i++, v++, ov++ )
			{
				Vector4Set( inVertsArray[i],
					move[0] + v->point[0]*frontv[0] + ov->point[0]*backv[0],
					move[1] + v->point[1]*frontv[1] + ov->point[1]*backv[1],
					move[2] + v->point[2]*frontv[2] + ov->point[2]*backv[2], 1 );

				if( calcNormals )
				{
					R_LatLongToNorm( v->latlong, normal );
					R_LatLongToNorm( ov->latlong, oldnormal );

					VectorSet( inNormalsArray[i],
						normal[0] + ( oldnormal[0] - normal[0] ) * backlerp,
						normal[1] + ( oldnormal[1] - normal[1] ) * backlerp,
						normal[2] + ( oldnormal[2] - normal[2] ) * backlerp );
				}
			}
		}

		if( calcSTVectors )
			R_BuildTangentVectors( mesh->numverts, inVertsArray, inNormalsArray, mesh->stArray, mesh->numtris, mesh->elems, inSVectorsArray );

		alias_mesh.xyzArray = calcVerts ? inVertsArray : mesh->xyzArray;
	}
	else
	{
		features |= MF_KEEPLOCK;
	}

	alias_mesh.elems = mesh->elems;
	alias_mesh.numElems = mesh->numtris * 3;
	alias_mesh.numVertexes = mesh->numverts;

	alias_mesh.stArray = mesh->stArray;
	if( features & MF_NORMALS )
		alias_mesh.normalsArray = calcNormals ? inNormalsArray : mesh->normalsArray;
	if( features & MF_SVECTORS )
		alias_mesh.sVectorsArray = calcSTVectors ? inSVectorsArray : mesh->sVectorsArray;

	R_RotateForEntity( e );

	R_PushMesh( &alias_mesh, features );
	R_RenderMeshBuffer( mb );
}

/*
=================
R_DrawAliasModel
=================
*/
void R_DrawAliasModel( const meshbuffer_t *mb )
{
	ref_entity_t *e = RI.currententity;

	if( OCCLUSION_QUERIES_ENABLED( RI ) && OCCLUSION_TEST_ENTITY( e ) )
	{
		ref_shader_t *shader;

		MB_NUM2SHADER( mb->shaderkey, shader );
		if( !R_GetOcclusionQueryResultBool( shader->type == SHADER_PLANAR_SHADOW ? OQ_PLANARSHADOW : OQ_ENTITY,
			e - r_entities, true ) )
			return;
	}

	// hack the depth range to prevent view model from poking into walls
	if( e->ent_type == ED_VIEWMODEL )
	{
		pglDepthRange( gldepthmin, gldepthmin + 0.3 * ( gldepthmax - gldepthmin ) );

		// backface culling for left-handed weapons
		if( r_lefthand->integer == 1 )
			GL_FrontFace( !glState.frontFace );
          }

	if( !r_lerpmodels->integer )
		e->backlerp = 0;

	R_DrawAliasFrameLerp( mb, e->backlerp );

	if( e->ent_type == ED_VIEWMODEL )
	{
		pglDepthRange( gldepthmin, gldepthmax );

		// backface culling for left-handed weapons
		if( r_lefthand->integer == 1 )
			GL_FrontFace( !glState.frontFace );
	}
}

/*
=================
R_AliasModelBBox
=================
*/
float R_AliasModelBBox( ref_entity_t *e, vec3_t mins, vec3_t maxs )
{
	ref_model_t	*mod = e->model;

	if( !mod ) return 0;

	R_AliasModelLerpBBox( e, mod );

	VectorCopy( alias_mins, mins );
	VectorCopy( alias_maxs, maxs );

	return alias_radius;
}

/*
=================
R_CullAliasModel
=================
*/
bool R_CullAliasModel( ref_entity_t *e )
{
	int		i, j, clipped;
	bool		frustum, query;
	uint		modhandle, numtris;
	ref_model_t	*mod = e->model;
	ref_shader_t	*shader;
	meshbuffer_t	*mb;
	maliasmodel_t	*aliasmodel;
	maliasmesh_t	*mesh;

	if(!( aliasmodel = (( maliasmodel_t * )mod->extradata )) || !aliasmodel->nummeshes )
		return true;

	R_AliasModelLerpBBox( e, mod );
	modhandle = Mod_Handle( mod );

	clipped = R_CullModel( e, alias_mins, alias_maxs, alias_radius );
	frustum = clipped & 1;
	if( clipped & 2 )
		return true;

	query = OCCLUSION_QUERIES_ENABLED( RI ) && OCCLUSION_TEST_ENTITY( e ) ? true : false;
	if( !frustum && query )
		R_IssueOcclusionQuery( R_GetOcclusionQueryNum( OQ_ENTITY, e - r_entities ), e, alias_mins, alias_maxs );

	if( ( RI.refdef.rdflags & RDF_NOWORLDMODEL )
		|| ( r_shadows->integer != SHADOW_PLANAR && !( r_shadows->integer == SHADOW_MAPPING && ( e->flags & EF_PLANARSHADOW )))
		|| R_CullPlanarShadow( e, alias_mins, alias_maxs, query ) )
		return frustum; // entity is not in PVS or shadow is culled away by frustum culling

	numtris = 0;
	for( i = 0, mesh = aliasmodel->meshes; i < aliasmodel->nummeshes; i++, mesh++ )
	{
		shader = NULL;

		if(( e->skin >= 0 ) && ( e->skin < aliasmodel->numskins ))
			shader = aliasmodel->skins[e->skin].shader;
		else if( mesh->numskins )
		{
			for( j = 0; j < mesh->numskins; j++ )
			{
				shader = mesh->skins[j].shader;
				if( shader && shader->sort <= SORT_ALPHATEST )
					break;
				shader = NULL;
			}
		}

		if( shader && ( shader->sort <= SORT_ALPHATEST ))
		{
			mb = R_AddMeshToList( MB_MODEL, NULL, R_PlanarShadowShader(), -( i+1 ));
			if( mb ) mb->modhandle = modhandle;
		}
	}

	return frustum;
}

/*
=================
R_AddAliasModelToList
=================
*/
void R_AddAliasModelToList( ref_entity_t *e )
{
	int		i, j;
	uint		modhandle, entnum = e - r_entities;
	mfog_t		*fog = NULL;
	ref_model_t	*mod = e->model;
	ref_shader_t	*shader;
	maliasmodel_t	*aliasmodel;
	maliasmesh_t	*mesh;

	aliasmodel = (maliasmodel_t *)mod->extradata;
	modhandle = Mod_Handle( mod );

	if( RI.params & RP_SHADOWMAPVIEW )
	{
		if( r_entShadowBits[entnum] & RI.shadowGroup->bit )
		{
			if( !r_shadows_self_shadow->integer )
				r_entShadowBits[entnum] &= ~RI.shadowGroup->bit;
			if( e->ent_type == ED_VIEWMODEL )
				return;
		}
		else
		{
			R_AliasModelLerpBBox( e, mod );
			if( !R_CullModel( e, alias_mins, alias_maxs, alias_radius ) )
				r_entShadowBits[entnum] |= RI.shadowGroup->bit;
			return; // mark as shadowed, proceed with caster otherwise
		}
	}
	else
	{
		fog = R_FogForSphere( e->origin, alias_radius );
#if 0
		if( !( e->ent_type == ED_VIEWMODEL ) && fog )
		{
			R_AliasModelLerpBBox( e, mod );
			if( R_CompletelyFogged( fog, e->origin, alias_radius ))
				return;
		}
#endif
	}

	for( i = 0, mesh = aliasmodel->meshes; i < aliasmodel->nummeshes; i++, mesh++ )
	{
		shader = NULL;

		if(( e->skin >= 0 ) && ( e->skin < aliasmodel->numskins ))
			shader = aliasmodel->skins[e->skin].shader;
		else if( mesh->numskins )
		{
			for( j = 0; j < mesh->numskins; j++ )
			{
				shader = mesh->skins[j].shader;
				if( shader ) R_AddModelMeshToList( modhandle, fog, shader, i );
			}
			continue;
		}
		if( shader ) R_AddModelMeshToList( modhandle, fog, shader, i );
	}
}