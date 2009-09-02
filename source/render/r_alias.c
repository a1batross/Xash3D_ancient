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

// r_alias.c: Quake 2 .md2 and Quake III Arena .md3 model formats support

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
Mod_AliasBuildMeshesForFrame0
=================
*/
static void Mod_AliasBuildMeshesForFrame0( ref_model_t *mod )
{
	int i, j, k;
	size_t size;
	maliasframe_t *frame;
	maliasmodel_t *aliasmodel = ( maliasmodel_t * )mod->extradata;

	frame = &aliasmodel->frames[0];
	for( k = 0; k < aliasmodel->nummeshes; k++ )
	{
		maliasmesh_t *mesh = &aliasmodel->meshes[k];

		size = sizeof( vec4_t ) + sizeof( vec4_t ); // xyz and normals
		if( GL_Support( R_SHADER_GLSL100_EXT ))
			size += sizeof( vec4_t );       // s-vectors
		size *= mesh->numverts;

		mesh->xyzArray = ( vec4_t * )Mod_Malloc( mod, size );
		mesh->normalsArray = ( vec4_t * )( ( byte * )mesh->xyzArray + mesh->numverts * sizeof( vec4_t ) );
		if( GL_Support( R_SHADER_GLSL100_EXT ))
			mesh->sVectorsArray = ( vec4_t * )( ( byte * )mesh->normalsArray + mesh->numverts * sizeof( vec4_t ) );

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
==============================================================================

MD3 MODELS

==============================================================================
*/

/*
=================
Mod_AliasLoadModel
=================
*/
void Mod_AliasLoadModel( ref_model_t *mod, ref_model_t *parent, const void *buffer )
{
	int version, i, j, l;
	int bufsize, numverts;
	byte *buf;
	dmd3header_t *pinmodel;
	dmd3frame_t *pinframe;
	dmd3tag_t *pintag;
	dmd3mesh_t *pinmesh;
	dmd3skin_t *pinskin;
	dmd3coord_t *pincoord;
	dmd3vertex_t *pinvert;
	elem_t *pinelem, *poutelem;
	maliasvertex_t *poutvert;
	vec2_t *poutcoord;
	maliasskin_t *poutskin;
	maliasmesh_t *poutmesh;
	maliastag_t *pouttag;
	maliasframe_t *poutframe;
	maliasmodel_t *poutmodel;
	vec3_t ebbox = { 0, 0, 0 };

	pinmodel = ( dmd3header_t * )buffer;
	version = LittleLong( pinmodel->version );

	if( version != MD3_ALIAS_VERSION )
		Host_Error( "%s has wrong version number (%i should be %i)\n",
		mod->name, version, MD3_ALIAS_VERSION );

	mod->type = mod_alias;
	mod->extradata = poutmodel = Mod_Malloc( mod, sizeof( maliasmodel_t ) );
	mod->radius = 0;
	ClearBounds( mod->mins, mod->maxs );

	// byte swap the header fields and sanity check
	poutmodel->numframes = LittleLong( pinmodel->num_frames );
	poutmodel->numtags = LittleLong( pinmodel->num_tags );
	poutmodel->nummeshes = LittleLong( pinmodel->num_meshes );
	poutmodel->numskins = 0;

	if( poutmodel->numframes <= 0 )
		Host_Error( "model %s has no frames\n", mod->name );
	//	else if( poutmodel->numframes > MD3_MAX_FRAMES )
	//		Host_Error( "model %s has too many frames\n", mod->name );

	if( poutmodel->numtags > MD3_MAX_TAGS )
		Host_Error( "model %s has too many tags\n", mod->name );
	else if( poutmodel->numtags < 0 )
		Host_Error( "model %s has invalid number of tags\n", mod->name );

	if( poutmodel->nummeshes < 0 )
		Host_Error( "model %s has invalid number of meshes\n", mod->name );
	else if( !poutmodel->nummeshes && !poutmodel->numtags )
		Host_Error( "model %s has no meshes and no tags\n", mod->name );
	//	else if( poutmodel->nummeshes > MD3_MAX_MESHES )
	//		Host_Error( "model %s has too many meshes\n", mod->name );

	bufsize = poutmodel->numframes * ( sizeof( maliasframe_t ) + sizeof( maliastag_t ) * poutmodel->numtags ) +
		poutmodel->nummeshes * sizeof( maliasmesh_t );
	buf = Mod_Malloc( mod, bufsize );

	//
	// load the frames
	//
	pinframe = ( dmd3frame_t * )( ( byte * )pinmodel + LittleLong( pinmodel->ofs_frames ) );
	poutframe = poutmodel->frames = ( maliasframe_t * )buf; buf += sizeof( maliasframe_t ) * poutmodel->numframes;
	for( i = 0; i < poutmodel->numframes; i++, pinframe++, poutframe++ )
	{
		for( j = 0; j < 3; j++ )
		{
			poutframe->scale[j] = MD3_XYZ_SCALE;
			poutframe->translate[j] = LittleFloat( pinframe->translate[j] );
		}

		// never trust the modeler utility and recalculate bbox and radius
		ClearBounds( poutframe->mins, poutframe->maxs );
	}

	//
	// load the tags
	//
	pintag = ( dmd3tag_t * )( ( byte * )pinmodel + LittleLong( pinmodel->ofs_tags ) );
	pouttag = poutmodel->tags = ( maliastag_t * )buf; buf += sizeof( maliastag_t ) * poutmodel->numframes * poutmodel->numtags;
	for( i = 0; i < poutmodel->numframes; i++ )
	{
		for( l = 0; l < poutmodel->numtags; l++, pintag++, pouttag++ )
		{
			vec3_t axis[3];

			for( j = 0; j < 3; j++ )
			{
				axis[0][j] = LittleFloat( pintag->axis[0][j] );
				axis[1][j] = LittleFloat( pintag->axis[1][j] );
				axis[2][j] = LittleFloat( pintag->axis[2][j] );
				pouttag->origin[j] = LittleFloat( pintag->origin[j] );
			}

			Quat_FromAxis( axis, pouttag->quat );
			Quat_Normalize( pouttag->quat );

			com.strncpy( pouttag->name, pintag->name, MD3_MAX_PATH );
		}
	}

	//
	// load the meshes
	//
	pinmesh = ( dmd3mesh_t * )( ( byte * )pinmodel + LittleLong( pinmodel->ofs_meshes ) );
	poutmesh = poutmodel->meshes = ( maliasmesh_t * )buf;
	for( i = 0; i < poutmodel->nummeshes; i++, poutmesh++ )
	{
		if( pinmesh->id != ALIASMODHEADER )
			Host_Error( "mesh %s in model %s has wrong id (%s should be %s)\n",
			pinmesh->name, mod->name, pinmesh->id, ALIASMODHEADER );

		com.strncpy( poutmesh->name, pinmesh->name, MD3_MAX_PATH );

		Mod_StripLODSuffix( poutmesh->name );

		poutmesh->numtris = LittleLong( pinmesh->num_tris );
		poutmesh->numskins = LittleLong( pinmesh->num_skins );
		poutmesh->numverts = numverts = LittleLong( pinmesh->num_verts );

		/*		if( poutmesh->numskins <= 0 )
		Host_Error( ERR_DROP, "mesh %i in model %s has no skins", i, mod->name );
		else*/ if( poutmesh->numskins > MD3_MAX_SHADERS )
			Host_Error( "mesh %i in model %s has too many skins\n", i, mod->name );
		if( poutmesh->numtris <= 0 )
			Host_Error( "mesh %i in model %s has no elements\n", i, mod->name );
		else if( poutmesh->numtris > MD3_MAX_TRIANGLES )
			Host_Error( "mesh %i in model %s has too many triangles\n", i, mod->name );
		if( poutmesh->numverts <= 0 )
			Host_Error( "mesh %i in model %s has no vertices\n", i, mod->name );
		else if( poutmesh->numverts > MD3_MAX_VERTS )
			Host_Error( "mesh %i in model %s has too many vertices\n", i, mod->name );

		bufsize = sizeof( maliasskin_t ) * poutmesh->numskins + poutmesh->numtris * sizeof( elem_t ) * 3 +
			numverts * ( sizeof( vec2_t ) + sizeof( maliasvertex_t ) * poutmodel->numframes );
		buf = Mod_Malloc( mod, bufsize );

		//
		// load the skins
		//
		pinskin = ( dmd3skin_t * )( ( byte * )pinmesh + LittleLong( pinmesh->ofs_skins ) );
		poutskin = poutmesh->skins = ( maliasskin_t * )buf; buf += sizeof( maliasskin_t ) * poutmesh->numskins;
		for( j = 0; j < poutmesh->numskins; j++, pinskin++, poutskin++ )
		{
			FS_StripExtension( pinskin->name );
			poutskin->shader = R_LoadShader( pinskin->name, SHADER_ALIAS_MD3, false, 0, SHADER_INVALID );
			R_DeformvBBoxForShader( poutskin->shader, ebbox );
		}

		//
		// load the elems
		//
		pinelem = ( elem_t * )( ( byte * )pinmesh + LittleLong( pinmesh->ofs_elems ) );
		poutelem = poutmesh->elems = ( elem_t * )buf; buf += poutmesh->numtris * sizeof( elem_t ) * 3;
		for( j = 0; j < poutmesh->numtris; j++, pinelem += 3, poutelem += 3 )
		{
			poutelem[0] = (elem_t)LittleLong( pinelem[0] );
			poutelem[1] = (elem_t)LittleLong( pinelem[1] );
			poutelem[2] = (elem_t)LittleLong( pinelem[2] );
		}

		//
		// load the texture coordinates
		//
		pincoord = ( dmd3coord_t * )( ( byte * )pinmesh + LittleLong( pinmesh->ofs_tcs ) );
		poutcoord = poutmesh->stArray = ( vec2_t * )buf; buf += poutmesh->numverts * sizeof( vec2_t );
		for( j = 0; j < poutmesh->numverts; j++, pincoord++ )
		{
			poutcoord[j][0] = LittleFloat( pincoord->st[0] );
			poutcoord[j][1] = LittleFloat( pincoord->st[1] );
		}

		//
		// load the vertexes and normals
		//
		pinvert = ( dmd3vertex_t * )( ( byte * )pinmesh + LittleLong( pinmesh->ofs_verts ) );
		poutvert = poutmesh->vertexes = ( maliasvertex_t * )buf;
		for( l = 0, poutframe = poutmodel->frames; l < poutmodel->numframes; l++, poutframe++, pinvert += poutmesh->numverts, poutvert += poutmesh->numverts )
		{
			vec3_t v;

			for( j = 0; j < poutmesh->numverts; j++ )
			{
				poutvert[j].point[0] = LittleShort( pinvert[j].point[0] );
				poutvert[j].point[1] = LittleShort( pinvert[j].point[1] );
				poutvert[j].point[2] = LittleShort( pinvert[j].point[2] );

				poutvert[j].latlong[0] = pinvert[j].norm[0];
				poutvert[j].latlong[1] = pinvert[j].norm[1];

				VectorCopy( poutvert[j].point, v );
				AddPointToBounds( v, poutframe->mins, poutframe->maxs );
			}
		}

		pinmesh = ( dmd3mesh_t * )( ( byte * )pinmesh + LittleLong( pinmesh->meshsize ) );
	}

	//
	// build S and T vectors for frame 0
	//
	Mod_AliasBuildMeshesForFrame0( mod );

	//
	// calculate model bounds
	//
	poutframe = poutmodel->frames;
	for( i = 0; i < poutmodel->numframes; i++, poutframe++ )
	{
		VectorMA( poutframe->translate, MD3_XYZ_SCALE, poutframe->mins, poutframe->mins );
		VectorMA( poutframe->translate, MD3_XYZ_SCALE, poutframe->maxs, poutframe->maxs );
		VectorSubtract( poutframe->mins, ebbox, poutframe->mins );
		VectorAdd( poutframe->maxs, ebbox, poutframe->maxs );
		poutframe->radius = RadiusFromBounds( poutframe->mins, poutframe->maxs );

		AddPointToBounds( poutframe->mins, mod->mins, mod->maxs );
		AddPointToBounds( poutframe->maxs, mod->mins, mod->maxs );
		mod->radius = max( mod->radius, poutframe->radius );
	}
	mod->touchFrame = tr.registration_sequence; // register model
}

/*
=============
R_AliasModelLOD
=============
*/
static ref_model_t *R_AliasModelLOD( ref_entity_t *e )
{
	int	lod;
	float	dist;

	if( !e->model->numlods ) return e->model;

	dist = DistanceFast( e->origin, RI.viewOrigin );
	dist *= RI.lod_dist_scale_for_fov;

	lod = (int)( dist / e->model->radius );
	if( r_lodscale->integer )
		lod /= r_lodscale->integer;
	lod += r_lodbias->integer;

	if( lod < 1 ) return e->model;
	return e->model->lods[min( lod, e->model->numlods )-1];
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
R_AliasModelLerpTag
=============
*/
bool R_AliasModelLerpTag( orientation_t *orient, maliasmodel_t *aliasmodel, int oldframenum, int framenum, float lerpfrac, const char *name )
{
	int		i;
	quat_t		quat;
	maliastag_t	*tag, *oldtag;

	// find the appropriate tag
	for( i = 0; i < aliasmodel->numtags; i++ )
	{
		if( !com.stricmp( aliasmodel->tags[i].name, name ) )
			break;
	}

	if( i == aliasmodel->numtags )
	{
		//		MsgDev ("R_AliasModelLerpTag: no such tag %s\n", name );
		return false;
	}

	// ignore invalid frames
	if( ( framenum >= aliasmodel->numframes ) || ( framenum < 0 ) )
	{
		MsgDev( D_ERROR, "R_AliasModelLerpTag %s: no such oldframe %i\n", name, framenum );
		framenum = 0;
	}
	if( ( oldframenum >= aliasmodel->numframes ) || ( oldframenum < 0 ) )
	{
		MsgDev( D_ERROR, "R_AliasModelLerpTag %s: no such oldframe %i\n", name, oldframenum );
		oldframenum = 0;
	}

	tag = aliasmodel->tags + framenum * aliasmodel->numtags + i;
	oldtag = aliasmodel->tags + oldframenum * aliasmodel->numtags + i;

	// interpolate axis and origin
	Quat_Lerp( oldtag->quat, tag->quat, lerpfrac, quat );
	Quat_Matrix( quat, orient->axis );

	orient->origin[0] = oldtag->origin[0] + ( tag->origin[0] - oldtag->origin[0] ) * lerpfrac;
	orient->origin[1] = oldtag->origin[1] + ( tag->origin[1] - oldtag->origin[1] ) * lerpfrac;
	orient->origin[2] = oldtag->origin[2] + ( tag->origin[2] - oldtag->origin[2] ) * lerpfrac;

	return true;
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
	ref_model_t	*mod;

	mod = R_AliasModelLOD( e );
	if( !mod )
		return 0;

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
	int i, j, clipped;
	bool frustum, query;
	unsigned int modhandle, numtris;
	ref_model_t	*mod;
	ref_shader_t *shader;
	meshbuffer_t *mb;
	maliasmodel_t *aliasmodel;
	maliasmesh_t *mesh;

	mod = R_AliasModelLOD( e );
	if( !( aliasmodel = ( ( maliasmodel_t * )mod->extradata ) ) || !aliasmodel->nummeshes )
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

		if( e->skinfile )
			shader = R_FindShaderForSkinFile( e->skinfile, mesh->name );
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
	int i, j;
	unsigned int modhandle, entnum = e - r_entities;
	mfog_t *fog = NULL;
	ref_model_t	*mod;
	ref_shader_t *shader;
	maliasmodel_t *aliasmodel;
	maliasmesh_t *mesh;

	mod = R_AliasModelLOD( e );
	aliasmodel = ( maliasmodel_t * )mod->extradata;
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

		if( e->skinfile ) shader = R_FindShaderForSkinFile( e->skinfile, mesh->name );
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
