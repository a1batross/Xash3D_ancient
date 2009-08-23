//=======================================================================
//			Copyright XashXT Group 2008 ©
//		   r_alias.c - Quake1 models loading & drawing
//=======================================================================

#include "r_local.h"
#include "mathlib.h"
#include "quatlib.h"
#include "byteorder.h"

static mesh_t alias_mesh;

static vec3_t alias_mins;
static vec3_t alias_maxs;
static float alias_radius;

/*
=================
Mod_QAliasLoadModel
=================
*/
void Mod_QAliasLoadModel( ref_model_t *mod, ref_model_t *parent, const void *buffer )
{
	int i, j, k;
	int version, framesize;
	float skinwidth, skinheight;
	int numverts, numelems;
	int indremap[MD2_MAX_TRIANGLES*3];
	elem_t ptempelem[MD2_MAX_TRIANGLES*3], ptempstelem[MD2_MAX_TRIANGLES*3];
	dmd2_t *pinmodel;
	dstvert_t *pinst;
	dtriangle_t *pintri;
	daliasframe_t *pinframe;
	elem_t *poutelem;
	maliasmodel_t *poutmodel;
	maliasmesh_t *poutmesh;
	vec2_t *poutcoord;
	maliasframe_t *poutframe;
	maliasvertex_t *poutvertex;
	maliasskin_t *poutskin;

	pinmodel = ( dmd2_t * )buffer;
	version = LittleLong( pinmodel->version );
	framesize = LittleLong( pinmodel->framesize );

	if( version != MD2_ALIAS_VERSION )
		Com_Error( ERR_DROP, "%s has wrong version number (%i should be %i)",
		mod->name, version, MD2_ALIAS_VERSION );

	mod->type = mod_alias;
	mod->aliasmodel = poutmodel = Mod_Malloc( mod, sizeof( maliasmodel_t ) );
	mod->radius = 0;
	ClearBounds( mod->mins, mod->maxs );

	// byte swap the header fields and sanity check
	skinwidth = LittleLong( pinmodel->skinwidth );
	skinheight = LittleLong( pinmodel->skinheight );

	if( skinwidth <= 0 )
		Com_Error( ERR_DROP, "model %s has invalid skin width", mod->name );
	if( skinheight <= 0 )
		Com_Error( ERR_DROP, "model %s has invalid skin height", mod->name );

	poutmodel->numframes = LittleLong( pinmodel->num_frames );
	poutmodel->numskins = LittleLong( pinmodel->num_skins );

	if( poutmodel->numframes > MD2_MAX_FRAMES )
		Com_Error( ERR_DROP, "model %s has too many frames", mod->name );
	else if( poutmodel->numframes <= 0 )
		Com_Error( ERR_DROP, "model %s has no frames", mod->name );
	if( poutmodel->numskins > MD2_MAX_SKINS )
		Com_Error( ERR_DROP, "model %s has too many skins", mod->name );
	else if( poutmodel->numskins < 0 )
		Com_Error( ERR_DROP, "model %s has invalid number of skins", mod->name );

	poutmodel->numtags = 0;
	poutmodel->tags = NULL;
	poutmodel->nummeshes = 1;

	poutmesh = poutmodel->meshes = Mod_Malloc( mod, sizeof( maliasmesh_t ) );
	Q_strncpyz( poutmesh->name, "default", MD3_MAX_PATH );

	poutmesh->numverts = LittleLong( pinmodel->num_xyz );
	poutmesh->numtris = LittleLong( pinmodel->num_tris );

	if( poutmesh->numverts <= 0 )
		Com_Error( ERR_DROP, "model %s has no vertices", mod->name );
	else if( poutmesh->numverts > MD2_MAX_VERTS )
		Com_Error( ERR_DROP, "model %s has too many vertices", mod->name );
	if( poutmesh->numtris > MD2_MAX_TRIANGLES )
		Com_Error( ERR_DROP, "model %s has too many triangles", mod->name );
	else if( poutmesh->numtris <= 0 )
		Com_Error( ERR_DROP, "model %s has no triangles", mod->name );

	numelems = poutmesh->numtris * 3;
	poutelem = poutmesh->elems = Mod_Malloc( mod, numelems * sizeof( elem_t ) );

	//
	// load triangle lists
	//
	pintri = ( dtriangle_t * )( ( qbyte * )pinmodel + LittleLong( pinmodel->ofs_tris ) );
	pinst = ( dstvert_t * ) ( ( qbyte * )pinmodel + LittleLong( pinmodel->ofs_st ) );

	for( i = 0, k = 0; i < poutmesh->numtris; i++, k += 3 )
	{
		for( j = 0; j < 3; j++ )
		{
			ptempelem[k+j] = ( elem_t )LittleShort( pintri[i].index_xyz[j] );
			ptempstelem[k+j] = ( elem_t )LittleShort( pintri[i].index_st[j] );
		}
	}

	//
	// build list of unique vertexes
	//
	numverts = 0;
	memset( indremap, -1, MD2_MAX_TRIANGLES * 3 * sizeof( int ) );

	for( i = 0; i < numelems; i++ )
	{
		if( indremap[i] != -1 )
			continue;

		// remap duplicates
		for( j = i + 1; j < numelems; j++ )
		{
			if( ( ptempelem[j] == ptempelem[i] )
				&& ( pinst[ptempstelem[j]].s == pinst[ptempstelem[i]].s )
				&& ( pinst[ptempstelem[j]].t == pinst[ptempstelem[i]].t ) )
			{
				indremap[j] = i;
				poutelem[j] = numverts;
			}
		}

		// add unique vertex
		indremap[i] = i;
		poutelem[i] = numverts++;
	}

	Com_DPrintf( "%s: remapped %i verts to %i (%i tris)\n", mod->name, poutmesh->numverts, numverts, poutmesh->numtris );

	poutmesh->numverts = numverts;

	//
	// load base s and t vertices
	//
	poutcoord = poutmesh->stArray = Mod_Malloc( mod, numverts * sizeof( vec2_t ) );

	for( i = 0; i < numelems; i++ )
	{
		if( indremap[i] == i )
		{
			poutcoord[poutelem[i]][0] = ( (float)LittleShort( pinst[ptempstelem[i]].s ) + 0.5 ) / skinwidth;
			poutcoord[poutelem[i]][1] = ( (float)LittleShort( pinst[ptempstelem[i]].t ) + 0.5 ) / skinheight;
		}
	}

	//
	// load the frames
	//
	poutframe = poutmodel->frames = Mod_Malloc( mod, poutmodel->numframes * ( sizeof( maliasframe_t ) + numverts * sizeof( maliasvertex_t ) ) );
	poutvertex = poutmesh->vertexes = ( maliasvertex_t *)( ( qbyte * )poutframe + poutmodel->numframes * sizeof( maliasframe_t ) );

	for( i = 0; i < poutmodel->numframes; i++, poutframe++, poutvertex += numverts )
	{
		pinframe = ( daliasframe_t * )( ( qbyte * )pinmodel + LittleLong( pinmodel->ofs_frames ) + i * framesize );

		for( j = 0; j < 3; j++ )
		{
			poutframe->scale[j] = LittleFloat( pinframe->scale[j] );
			poutframe->translate[j] = LittleFloat( pinframe->translate[j] );
		}

		for( j = 0; j < numelems; j++ )
		{                               // verts are all 8 bit, so no swapping needed
			if( indremap[j] == j )
			{
				poutvertex[poutelem[j]].point[0] = (short)pinframe->verts[ptempelem[j]].v[0];
				poutvertex[poutelem[j]].point[1] = (short)pinframe->verts[ptempelem[j]].v[1];
				poutvertex[poutelem[j]].point[2] = (short)pinframe->verts[ptempelem[j]].v[2];
			}
		}

		Mod_AliasCalculateVertexNormals( numelems, poutelem, numverts, poutvertex );

		VectorCopy( poutframe->translate, poutframe->mins );
		VectorMA( poutframe->translate, 255, poutframe->scale, poutframe->maxs );
		poutframe->radius = RadiusFromBounds( poutframe->mins, poutframe->maxs );

		mod->radius = max( mod->radius, poutframe->radius );
		AddPointToBounds( poutframe->mins, mod->mins, mod->maxs );
		AddPointToBounds( poutframe->maxs, mod->mins, mod->maxs );
	}

	//
	// build S and T vectors for frame 0
	//
	Mod_AliasBuildMeshesForFrame0( mod );


	// register all skins
	poutskin = poutmodel->skins = Mod_Malloc( mod, poutmodel->numskins * sizeof( maliasskin_t ) );

	for( i = 0; i < poutmodel->numskins; i++, poutskin++ )
	{
		if( LittleLong( pinmodel->ofs_skins ) == -1 )
			continue;
		poutskin->shader = R_RegisterSkin( ( char * )pinmodel + LittleLong( pinmodel->ofs_skins ) + i*MD2_MAX_SKINNAME );
	}
}