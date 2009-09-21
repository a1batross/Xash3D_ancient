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

// r_model.c -- model loading and caching

#include "stdio.h"		// sscanf
#include "r_local.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "byteorder.h"

#define Mod_CopyString( m, str )	com.stralloc( (m)->mempool, str, __FILE__, __LINE__ )

enum
{
BSP_IDBSP = 0,
BSP_RAVEN,
BSP_IGBSP,
};

typedef struct
{ 
	int	ident;
	int	version;
	int	lightmapWidth;
	int	lightmapHeight;
	int	flags;
} bspFormatDesc_t;

bspFormatDesc_t bspFormats[] =
{
{ QFBSPMODHEADER, RFIDBSP_VERSION, 512, 512, BSP_RAVEN },
{ IDBSPMODHEADER, Q3IDBSP_VERSION, 128, 128, BSP_IDBSP },
{ IDBSPMODHEADER, IGIDBSP_VERSION, 128, 128, BSP_IGBSP },
{ IDBSPMODHEADER, RTCWBSP_VERSION, 128, 128, BSP_IDBSP },
{ RBBSPMODHEADER, RFIDBSP_VERSION, 128, 128, BSP_RAVEN }
};

int numBspFormats = sizeof( bspFormats ) / sizeof( bspFormats[0] );

typedef struct
{
	char		name[MAX_SHADERPATH];
	ref_shader_t	*shader;
	int		flags;
} mshaderref_t;

typedef struct
{
	int	ident;
	void	(*loader)( ref_model_t *mod, const void *buffer );
} modelformatdescriptor_t;

static ref_model_t *loadmodel;
static int loadmodel_numverts;
static vec4_t	*loadmodel_xyz_array;		// vertexes
static vec4_t	*loadmodel_normals_array;		// normals
static vec2_t	*loadmodel_st_array;		// texture coords
static vec2_t	*loadmodel_lmst_array[LM_STYLES];	// lightmap texture coords
static rgba_t	*loadmodel_colors_array[LM_STYLES];	// colors used for vertex lighting
static dshader_t	*loadmodel_shaders[MAX_MAP_SHADERS];	// hold contents and texture size
static int	loadmodel_numsurfelems;
static elem_t	*loadmodel_surfelems;

static int		loadmodel_numlightmaps;
static mlightmapRect_t	*loadmodel_lightmapRects;
static int		loadmodel_numshaderrefs;
static mshaderref_t		*loadmodel_shaderrefs;

void Mod_SpriteLoadModel( ref_model_t *mod, const void *buffer );
void Mod_StudioLoadModel( ref_model_t *mod, const void *buffer );
void Mod_QAliasLoadModel( ref_model_t *mod, const void *buffer );
void Mod_BrushLoadModel( ref_model_t *mod, const void *buffer );

ref_model_t *Mod_LoadModel( ref_model_t *mod, bool crash );

static ref_model_t		*r_inlinemodels = NULL;
static byte		mod_novis[MAX_MAP_LEAFS/8];
static ref_model_t		r_models[MAX_MODELS];
static int		r_nummodels;
static bspFormatDesc_t	*mod_bspFormat;
static byte		*mod_mempool;

static modelformatdescriptor_t mod_supportedformats[] =
{
{ IDSPRITEHEADER,	Mod_SpriteLoadModel	}, // Half-Life sprite models
{ IDSTUDIOHEADER,	Mod_StudioLoadModel	}, // Half-Life studio models
{ IDQALIASHEADER,	Mod_QAliasLoadModel	}, // Quake alias models
{ IDBSPMODHEADER,	Mod_BrushLoadModel	}, // Quake III Arena .bsp models
{ RBBSPMODHEADER,	Mod_BrushLoadModel	}, // SOF2 and JK2 .bsp models
{ QFBSPMODHEADER,	Mod_BrushLoadModel	}, // QFusion .bsp models
{ 0, 		NULL		}  // terminator
};

static int mod_numsupportedformats = sizeof( mod_supportedformats ) / sizeof( mod_supportedformats[0] ) - 1;

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf( vec3_t p, ref_model_t *model )
{
	mnode_t	*node;
	cplane_t *plane;
	mbrushmodel_t *bmodel;

	if( !model || !( bmodel = ( mbrushmodel_t * )model->extradata ) || !bmodel->nodes )
	{
		Host_Error( "Mod_PointInLeaf: bad model\n" );
		return NULL;
	}

	node = bmodel->nodes;
	do
	{
		plane = node->plane;
		node = node->children[PlaneDiff( p, plane ) < 0];
	}
	while( node->plane != NULL );

	return (mleaf_t *)node;
}

/*
==============
Mod_ClusterPVS
==============
*/
byte *Mod_ClusterPVS( int cluster, ref_model_t *model )
{
	mbrushmodel_t *bmodel = ( mbrushmodel_t * )model->extradata;

	if( cluster == -1 || !bmodel->vis )
		return mod_novis;
	return ( (byte *)bmodel->vis->data + cluster*bmodel->vis->rowsize );
}


//===============================================================================

/*
================
Mod_Modellist_f
================
*/
void Mod_Modellist_f( void )
{
	int		i, nummodels;
	ref_model_t	*mod;

	Msg( "\n" );
	Msg( "-----------------------------------\n" );

	for( i = nummodels = 0, mod = r_models; i < r_nummodels; i++, mod++ )
	{
		if( !mod->name ) continue; // free slot
		Msg( "%s%s\n", mod->name, (mod->type == mod_bad) ? " (DEFAULTED)" : "" );
		nummodels++;
	}

	Msg( "-----------------------------------\n" );
	Msg( "%i total models\n", nummodels );
	Msg( "\n" );
}

/*
================
Mod_FreeModel
================
*/
void Mod_FreeModel( ref_model_t *mod )
{
	if( !mod || !mod->mempool ) return;

	if( mod == r_worldmodel ) R_FreeSky();
	Mem_FreePool( &mod->mempool );
	Mem_Set( mod, 0, sizeof( *mod ));
}

/*
================
Mod_FreeAll
================
*/
void Mod_FreeAll( void )
{
	int	i;

	if( r_inlinemodels )
	{
		Mem_Free( r_inlinemodels );
		r_inlinemodels = NULL;
	}

	for( i = 0; i < r_nummodels; i++ )
		Mod_FreeModel( &r_models[i] );
}

/*
===============
R_InitModels
===============
*/
void R_InitModels( void )
{
	Mem_Set( mod_novis, 0xff, sizeof( mod_novis ));
	mod_mempool = Mem_AllocPool( "Models" );
	r_nummodels = 0;

	R_StudioInit();
	R_SpriteInit();
}

/*
================
R_ShutdownModels
================
*/
void R_ShutdownModels( void )
{
	if( !mod_mempool ) return;

	R_StudioShutdown();
	Mod_FreeAll();

	r_worldmodel = NULL;
	r_worldbrushmodel = NULL;

	r_nummodels = 0;
	Mem_Set( r_models, 0, sizeof( r_models ));
	Mem_FreePool( &mod_mempool );
}

/*
==================
Mod_FindSlot
==================
*/
static ref_model_t *Mod_FindSlot( const char *name )
{
	ref_model_t	*mod;
	int		i;

	// find a free model slot spot
	for( i = 0, mod = r_models; i < r_nummodels; i++, mod++ )
		if( !mod->name ) break; // free spot

	if( i == r_nummodels )
	{
		if( r_nummodels == MAX_MODELS )
			Host_Error( "Mod_ForName: MAX_MODELS limit exceeded\n" );
		r_nummodels++;
	}
	return mod;
}

/*
==================
Mod_Handle
==================
*/
uint Mod_Handle( ref_model_t *mod )
{
	return mod - r_models;
}

/*
==================
Mod_ForHandle
==================
*/
ref_model_t *Mod_ForHandle( uint handle )
{
	return r_models + handle;
}

/*
=================
Mod_UpdateShaders

update shader and associated textures
=================
*/
static void Mod_UpdateShaders( ref_model_t *mod )
{
	ref_shader_t	*shader;
	int		i;

	if( !mod || !mod->name )
		return;

	for( i = 0; i < mod->numshaders; i++ )
	{
		shader = mod->shaders[i];
		if( !shader || !shader->name ) continue;
		Shader_TouchImages( shader, false );
	}

	if( mod == r_worldmodel && tr.currentSkyShader )
		Shader_TouchImages( tr.currentSkyShader, false );
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
ref_model_t *Mod_ForName( const char *name, bool crash )
{
	int			i;
	ref_model_t		*mod;
	uint			*buf;
	modelformatdescriptor_t	*descr;

	if( !name[0] ) Host_Error( "Mod_ForName: NULL name\n" );

	// inline models are grabbed only from worldmodel
	if( name[0] == '*' )
	{
		i = com.atoi( name + 1 );
		if( i < 1 || !r_worldmodel || i >= r_worldbrushmodel->numsubmodels )
		{
			MsgDev( D_ERROR, "bad inline model number %i\n", i );
			return NULL;
		}
		return &r_inlinemodels[i];
	}

	// search the currently loaded models
	for( i = 0, mod = r_models; i < r_nummodels; i++, mod++ )
	{
		if( !mod->name ) continue;
		if( !com.strcmp( mod->name, name ))
		{
			// prolonge registration
			mod->touchFrame = tr.registration_sequence;
			return mod;
		}
	}

	mod = Mod_FindSlot( name );

	Com_Assert( mod == NULL );

	// load the file
	buf = (uint *)FS_LoadFile( name, NULL );
	if( !buf && crash ) Host_Error( "Mod_ForName: %s not found\n", name );

	// return the NULL model
	if( !buf ) return NULL;

	loadmodel = mod;
	loadmodel_xyz_array = NULL;
	loadmodel_surfelems = NULL;
	loadmodel_lightmapRects = NULL;
	loadmodel_shaderrefs = NULL;

	// call the apropriate loader
	descr = mod_supportedformats;
	for( i = 0; i < mod_numsupportedformats; i++, descr++ )
	{
		if( LittleLong(*(uint *)buf) == descr->ident )
			break;
	}
	if( i == mod_numsupportedformats )
	{
		MsgDev( D_ERROR, "Mod_NumForName: unknown fileid for %s\n", name );
		Mem_Free( buf );
		return NULL;
	}

	mod->type = mod_bad;
	mod->mempool = Mem_AllocPool( va( "^1%s^7", name ));
	mod->name = Mod_CopyString( mod, name );

	descr->loader( mod, buf );
	Mem_Free( buf );

	if( mod->type == mod_bad )
	{
		// check for loading problems
		Mod_FreeModel( mod );
		return NULL;
	}
	return mod;
}

/*
===============================================================================

BRUSHMODEL LOADING

===============================================================================
*/

static byte		*mod_base;
static mbrushmodel_t	*loadbmodel;

/*
=================
Mod_CheckDeluxemaps
=================
*/
static void Mod_CheckDeluxemaps( const lump_t *l, byte *lmData )
{
	int i, j;
	int surfaces, lightmap;

	// there are no deluxemaps in the map if the number of lightmaps is
	// less than 2 or odd
	if( !r_lighting_deluxemapping->integer || loadmodel_numlightmaps < 2 || loadmodel_numlightmaps & 1 )
		return;

	if( mod_bspFormat->flags & BSP_RAVEN )
	{
		dsurfacer_t *in = ( void * )( mod_base + l->fileofs );

		surfaces = l->filelen / sizeof( *in );
		for( i = 0; i < surfaces; i++, in++ )
		{
			for( j = 0; j < LM_STYLES; j++ )
			{
				lightmap = LittleLong( in->lm_texnum[j] );
				if( lightmap <= 0 )
					continue;
				if( lightmap & 1 )
					return;
			}
		}
	}
	else
	{
		dsurfaceq_t	*in = ( void * )( mod_base + l->fileofs );

		surfaces = l->filelen / sizeof( *in );
		for( i = 0; i < surfaces; i++, in++ )
		{
			lightmap = LittleLong( in->lm_texnum );
			if( lightmap <= 0 )
				continue;
			if( lightmap & 1 )
				return;
		}
	}

	// check if the deluxemap is actually empty (q3map2, yay!)
	if( loadmodel_numlightmaps == 2 )
	{
		int lW = mod_bspFormat->lightmapWidth, lH = mod_bspFormat->lightmapHeight;

		lmData += lW * lH * LM_BYTES;
		for( i = lW * lH; i > 0; i--, lmData += LM_BYTES )
		{
			for( j = 0; j < LM_BYTES; j++ )
			{
				if( lmData[j] )
					break;
			}
			if( j != LM_BYTES )
				break;
		}

		// empty deluxemap
		if( !i )
		{
			loadmodel_numlightmaps = 1;
			return;
		}
	}

	mapConfig.deluxeMaps = true;
	if(GL_Support( R_SHADER_GLSL100_EXT ))
		mapConfig.deluxeMappingEnabled = true;
}

/*
=================
Mod_LoadLighting
=================
*/
static void Mod_LoadLighting( const lump_t *l, const lump_t *faces )
{
	int size;

	if( !l->filelen )
		return;
	size = mod_bspFormat->lightmapWidth * mod_bspFormat->lightmapHeight * LM_BYTES;
	if( l->filelen % size )
		Host_Error( "Mod_LoadLighting: funny lump size in %s\n", loadmodel->name );

	loadmodel_numlightmaps = l->filelen / size;
	loadmodel_lightmapRects = Mod_Malloc( loadmodel, loadmodel_numlightmaps * sizeof( *loadmodel_lightmapRects ) );

	Mod_CheckDeluxemaps( faces, mod_base + l->fileofs );

	// set overbright bits for lightmaps and lightgrid
	// deluxemapped maps have zero scale because most surfaces
	// have a gloss stage that makes them look brighter anyway
	/*if( mapConfig.deluxeMapping )
	mapConfig.pow2MapOvrbr = 0;
	else */if(r_ignorehwgamma->integer)
		mapConfig.pow2MapOvrbr = r_mapoverbrightbits->integer;
	else
		mapConfig.pow2MapOvrbr = r_mapoverbrightbits->integer - r_overbrightbits->integer;
	if( mapConfig.pow2MapOvrbr < 0 )
		mapConfig.pow2MapOvrbr = 0;

	R_BuildLightmaps( loadmodel_numlightmaps, mod_bspFormat->lightmapWidth, mod_bspFormat->lightmapHeight, mod_base + l->fileofs, loadmodel_lightmapRects );
}

/*
=================
Mod_LoadVertexes
=================
*/
static void Mod_LoadVertexes( const lump_t *l )
{
	int i, count, j;
	dvertexq_t *in;
	float *out_xyz, *out_normals, *out_st, *out_lmst;
	byte *buffer, *out_colors;
	size_t bufSize;
	vec3_t color, fcolor;
	float div;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Host_Error( "Mod_LoadVertexes: funny lump size in %s\n", loadmodel->name );
	count = l->filelen / sizeof( *in );

	bufSize = 0;
	bufSize += count * ( sizeof( vec4_t ) + sizeof( vec4_t ) + sizeof( vec2_t )*2 + sizeof( rgba_t ) );
	buffer = Mod_Malloc( loadmodel, bufSize );

	loadmodel_numverts = count;
	loadmodel_xyz_array = ( vec4_t * )buffer; buffer += count*sizeof( vec4_t );
	loadmodel_normals_array = ( vec4_t * )buffer; buffer += count*sizeof( vec4_t );
	loadmodel_st_array = ( vec2_t * )buffer; buffer += count*sizeof( vec2_t );
	loadmodel_lmst_array[0] = ( vec2_t * )buffer; buffer += count*sizeof( vec2_t );
	loadmodel_colors_array[0] = ( rgba_t * )buffer; buffer += count*sizeof( rgba_t );
	for( i = 1; i < LM_STYLES; i++ )
	{
		loadmodel_lmst_array[i] = loadmodel_lmst_array[0];
		loadmodel_colors_array[i] = loadmodel_colors_array[0];
	}

	out_xyz = loadmodel_xyz_array[0];
	out_normals = loadmodel_normals_array[0];
	out_st = loadmodel_st_array[0];
	out_lmst = loadmodel_lmst_array[0][0];
	out_colors = loadmodel_colors_array[0][0];

	if( r_mapoverbrightbits->integer > 0 )
		div = (float)( 1 << r_mapoverbrightbits->integer ) / 255.0f;
	else
		div = 1.0f / 255.0f;

	for( i = 0; i < count; i++, in++, out_xyz += 4, out_normals += 4, out_st += 2, out_lmst += 2, out_colors += 4 )
	{
		for( j = 0; j < 3; j++ )
		{
			out_xyz[j] = LittleFloat( in->point[j] );
			out_normals[j] = LittleFloat( in->normal[j] );
		}
		out_xyz[3] = 1;
		out_normals[3] = 0;

		for( j = 0; j < 2; j++ )
		{
			out_st[j] = LittleFloat( in->tex_st[j] );
			out_lmst[j] = LittleFloat( in->lm_st[j] );
		}

		if( r_fullbright->integer )
		{
			out_colors[0] = 255;
			out_colors[1] = 255;
			out_colors[2] = 255;
			out_colors[3] = in->color[3];
		}
		else
		{
			for( j = 0; j < 3; j++ )
				color[j] = ( ( float )in->color[j] * div );
			ColorNormalize( color, fcolor );

			out_colors[0] = ( byte )( fcolor[0] * 255 );
			out_colors[1] = ( byte )( fcolor[1] * 255 );
			out_colors[2] = ( byte )( fcolor[2] * 255 );
			out_colors[3] = in->color[3];
		}
	}
}

/*
=================
Mod_LoadVertexes_RBSP
=================
*/
static void Mod_LoadVertexes_RBSP( const lump_t *l )
{
	int i, count, j;
	dvertexr_t *in;
	float *out_xyz, *out_normals, *out_st, *out_lmst[LM_STYLES];
	byte *buffer, *out_colors[LM_STYLES];
	size_t bufSize;
	vec3_t color, fcolor;
	float div;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Host_Error( "Mod_LoadVertexes: funny lump size in %s\n", loadmodel->name );
	count = l->filelen / sizeof( *in );

	bufSize = 0;
	bufSize += count * ( sizeof( vec4_t ) + sizeof( vec4_t ) + sizeof( vec2_t ) + ( sizeof( vec2_t ) + sizeof( rgba_t ) )*LM_STYLES );
	buffer = Mod_Malloc( loadmodel, bufSize );

	loadmodel_numverts = count;
	loadmodel_xyz_array = ( vec4_t * )buffer; buffer += count*sizeof( vec4_t );
	loadmodel_normals_array = ( vec4_t * )buffer; buffer += count*sizeof( vec4_t );
	loadmodel_st_array = ( vec2_t * )buffer; buffer += count*sizeof( vec2_t );
	for( i = 0; i < LM_STYLES; i++ )
	{
		loadmodel_lmst_array[i] = ( vec2_t * )buffer; buffer += count*sizeof( vec2_t );
		loadmodel_colors_array[i] = ( rgba_t * )buffer; buffer += count*sizeof( rgba_t );
	}

	out_xyz = loadmodel_xyz_array[0];
	out_normals = loadmodel_normals_array[0];
	out_st = loadmodel_st_array[0];
	for( i = 0; i < LM_STYLES; i++ )
	{
		out_lmst[i] = loadmodel_lmst_array[i][0];
		out_colors[i] = loadmodel_colors_array[i][0];
	}

	if( r_mapoverbrightbits->integer > 0 )
		div = (float)( 1 << r_mapoverbrightbits->integer ) / 255.0f;
	else
		div = 1.0f / 255.0f;

	for( i = 0; i < count; i++, in++, out_xyz += 4, out_normals += 4, out_st += 2 )
	{
		for( j = 0; j < 3; j++ )
		{
			out_xyz[j] = LittleFloat( in->point[j] );
			out_normals[j] = LittleFloat( in->normal[j] );
		}
		out_xyz[3] = 1;
		out_normals[3] = 0;

		for( j = 0; j < 2; j++ )
			out_st[j] = LittleFloat( in->tex_st[j] );

		for( j = 0; j < LM_STYLES; out_lmst[j] += 2, out_colors[j] += 4, j++ )
		{
			out_lmst[j][0] = LittleFloat( in->lm_st[j][0] );
			out_lmst[j][1] = LittleFloat( in->lm_st[j][1] );

			if( r_fullbright->integer )
			{
				out_colors[j][0] = 255;
				out_colors[j][1] = 255;
				out_colors[j][2] = 255;
				out_colors[j][3] = in->color[j][3];
			}
			else
			{
				color[0] = ( ( float )in->color[j][0] * div );
				color[1] = ( ( float )in->color[j][1] * div );
				color[2] = ( ( float )in->color[j][2] * div );
				ColorNormalize( color, fcolor );

				out_colors[j][0] = ( byte )( fcolor[0] * 255 );
				out_colors[j][1] = ( byte )( fcolor[1] * 255 );
				out_colors[j][2] = ( byte )( fcolor[2] * 255 );
				out_colors[j][3] = in->color[j][3];
			}
		}
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
static void Mod_LoadSubmodels( const lump_t *l )
{
	int		i, j, count;
	dmodel_t		*in;
	mmodel_t		*out;
	mbrushmodel_t	*bmodel;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Host_Error( "Mod_LoadSubmodels: funny lump size in %s\n", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ));

	r_inlinemodels = Mod_Malloc( loadmodel, count * ( sizeof( *r_inlinemodels ) + sizeof( *bmodel )));
	loadmodel->extradata = bmodel = (mbrushmodel_t *)((byte*)r_inlinemodels + count * sizeof( *r_inlinemodels ));

	loadbmodel = bmodel;
	loadbmodel->submodels = out;
	loadbmodel->numsubmodels = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		r_inlinemodels[i].extradata = bmodel + i;

		for( j = 0; j < 3; j++ )
		{
			// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat( in->mins[j] ) - 1;
			out->maxs[j] = LittleFloat( in->maxs[j] ) + 1;
		}

		out->radius = RadiusFromBounds( out->mins, out->maxs );
		out->firstface = LittleLong( in->firstsurface );
		out->numfaces = LittleLong( in->numsurfaces );
	}
}

/*
=================
Mod_LoadShaderrefs
=================
*/
static void Mod_LoadShaderrefs( const lump_t *l )
{
	int		i, count;
	int		contents;
	dshader_t		*in;
	mshaderref_t	*out;
	cvar_t		*scr_loading = Cvar_Get( "scr_loading", "0", 0, "loading bar progress" );
	
	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Host_Error( "Mod_LoadShaderrefs: funny lump size in %s\n", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ));

	loadmodel->shaders = Mod_Malloc( loadmodel, count * sizeof( ref_shader_t* ));
	loadmodel->numshaders = count;

	loadmodel_shaderrefs = out;
	loadmodel_numshaderrefs = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		Cvar_SetValue( "scr_loading", scr_loading->value + 50.0f / count );
		if( ri.UpdateScreen ) ri.UpdateScreen();

		com.strncpy( out->name, in->name, sizeof( out->name ) );
		out->flags = LittleLong( in->surfaceFlags );
		contents = LittleLong( in->contentFlags );
		if( contents & ( MASK_WATER|CONTENTS_FOG ))
			out->flags |= SURF_NOMARKS;
		out->shader = NULL;
	}
}

/*
=================
Mod_CreateMeshForSurface
=================
*/
static mesh_t *Mod_CreateMeshForSurface( const dsurfacer_t *in, msurface_t *out )
{
	mesh_t *mesh = NULL;
	bool createSTverts;
	byte *buffer;
	size_t bufSize;

	if( ( mapConfig.deluxeMappingEnabled && !(LittleLong( in->lm_texnum[0] ) < 0 || in->lightmapStyles[0] == 255) ) ||
		( out->shader->flags & SHADER_PORTAL_CAPTURE2 ) )
	{
		createSTverts = true;
	}
	else
	{
		createSTverts = false;
	}

	switch( out->facetype )
	{
	case MST_FLARE:
		{
			int j;

			for( j = 0; j < 3; j++ )
			{
				out->origin[j] = LittleFloat( in->origin[j] );
				out->color[j] = bound( 0, LittleFloat( in->mins[j] ), 1 );
			}
			break;
		}
	case MST_PATCH:
		{
			int i, j, u, v, p;
			int patch_cp[2], step[2], size[2], flat[2];
			float subdivLevel, f;
			int numVerts, firstVert;
			vec4_t tempv[MAX_ARRAY_VERTS];
			vec4_t colors[MAX_ARRAY_VERTS];
			elem_t	*elems;

			patch_cp[0] = LittleLong( in->patch_cp[0] );
			patch_cp[1] = LittleLong( in->patch_cp[1] );

			if( !patch_cp[0] || !patch_cp[1] )
				break;

			subdivLevel = r_subdivisions->value;
			if( subdivLevel < 1 )
				subdivLevel = 1;

			numVerts = LittleLong( in->numverts );
			firstVert = LittleLong( in->firstvert );

			// find the degree of subdivision in the u and v directions
			Patch_GetFlatness( subdivLevel, (vec_t *)loadmodel_xyz_array[firstVert], 4, patch_cp, flat );

			// allocate space for mesh
			step[0] = ( 1 << flat[0] );
			step[1] = ( 1 << flat[1] );
			size[0] = ( patch_cp[0] >> 1 ) * step[0] + 1;
			size[1] = ( patch_cp[1] >> 1 ) * step[1] + 1;
			numVerts = size[0] * size[1];

			if( numVerts > MAX_ARRAY_VERTS )
				break;

			bufSize = sizeof( mesh_t ) + numVerts * ( sizeof( vec4_t ) + sizeof( vec4_t ) + sizeof( vec2_t ) );
			for( j = 0; j < LM_STYLES && in->lightmapStyles[j] != 255; j++ )
				bufSize += numVerts * sizeof( vec2_t );
			for( j = 0; j < LM_STYLES && in->vertexStyles[j] != 255; j++ )
				bufSize += numVerts * sizeof( rgba_t );
			if( createSTverts )
				bufSize += numVerts * sizeof( vec4_t );
			buffer = ( byte * )Mod_Malloc( loadmodel, bufSize );

			mesh = ( mesh_t * )buffer; buffer += sizeof( mesh_t );
			mesh->numVertexes = numVerts;
			mesh->xyzArray = ( vec4_t * )buffer; buffer += numVerts * sizeof( vec4_t );
			mesh->normalsArray = ( vec4_t * )buffer; buffer += numVerts * sizeof( vec4_t );
			mesh->stArray = ( vec2_t * )buffer; buffer += numVerts * sizeof( vec2_t );

			Patch_Evaluate( loadmodel_xyz_array[firstVert], patch_cp, step, mesh->xyzArray[0], 4 );
			Patch_Evaluate( loadmodel_normals_array[firstVert], patch_cp, step, mesh->normalsArray[0], 4 );
			Patch_Evaluate( loadmodel_st_array[firstVert], patch_cp, step, mesh->stArray[0], 2 );
			for( i = 0; i < numVerts; i++ )
				VectorNormalize( mesh->normalsArray[i] );

			for( j = 0; j < LM_STYLES && in->lightmapStyles[j] != 255; j++ )
			{
				mesh->lmstArray[j] = ( vec2_t * )buffer; buffer += numVerts * sizeof( vec2_t );
				Patch_Evaluate( loadmodel_lmst_array[j][firstVert], patch_cp, step, mesh->lmstArray[j][0], 2 );
			}

			for( j = 0; j < LM_STYLES && in->vertexStyles[j] != 255; j++ )
			{
				mesh->colorsArray[j] = ( rgba_t * )buffer; buffer += numVerts * sizeof( rgba_t );
				for( i = 0; i < numVerts; i++ )
					Vector4Scale( loadmodel_colors_array[j][firstVert + i], ( 1.0f / 255.0f ), colors[i] );
				Patch_Evaluate( colors[0], patch_cp, step, tempv[0], 4 );

				for( i = 0; i < numVerts; i++ )
				{
					f = max( max( tempv[i][0], tempv[i][1] ), tempv[i][2] );
					if( f > 1.0f )
						f = 255.0f / f;
					else
						f = 255;

					mesh->colorsArray[j][i][0] = tempv[i][0] * f;
					mesh->colorsArray[j][i][1] = tempv[i][1] * f;
					mesh->colorsArray[j][i][2] = tempv[i][2] * f;
					mesh->colorsArray[j][i][3] = bound( 0, tempv[i][3], 1 ) * 255;
				}
			}

			// compute new elems
			mesh->numElems = ( size[0] - 1 ) * ( size[1] - 1 ) * 6;
			elems = mesh->elems = ( elem_t * )Mod_Malloc( loadmodel, mesh->numElems * sizeof( elem_t ) );
			for( v = 0, i = 0; v < size[1] - 1; v++ )
			{
				for( u = 0; u < size[0] - 1; u++ )
				{
					p = v * size[0] + u;
					elems[0] = p;
					elems[1] = p + size[0];
					elems[2] = p + 1;
					elems[3] = p + 1;
					elems[4] = p + size[0];
					elems[5] = p + size[0] + 1;
					elems += 6;
				}
			}

			if( createSTverts )
			{
				mesh->sVectorsArray = ( vec4_t * )buffer; buffer += numVerts * sizeof( vec4_t );
				R_BuildTangentVectors( mesh->numVertexes, mesh->xyzArray, mesh->normalsArray, mesh->stArray, mesh->numElems / 3, mesh->elems, mesh->sVectorsArray );
			}
			break;
		}
	case MST_PLANAR:
	case MST_TRISURF:
		{
			int j, numVerts, firstVert, numElems, firstElem;

			numVerts = LittleLong( in->numverts );
			firstVert = LittleLong( in->firstvert );

			numElems = LittleLong( in->numelems );
			firstElem = LittleLong( in->firstelem );

			bufSize = sizeof( mesh_t ) + numVerts * ( sizeof( vec4_t ) + sizeof( vec4_t ) + sizeof( vec2_t ) + numElems * sizeof( elem_t ) );
			for( j = 0; j < LM_STYLES && in->lightmapStyles[j] != 255; j++ )
				bufSize += numVerts * sizeof( vec2_t );
			for( j = 0; j < LM_STYLES && in->vertexStyles[j] != 255; j++ )
				bufSize += numVerts * sizeof( rgba_t );
			if( createSTverts )
				bufSize += numVerts * sizeof( vec4_t );
			if( out->facetype == MST_PLANAR )
				bufSize += sizeof( cplane_t );
			buffer = ( byte * )Mod_Malloc( loadmodel, bufSize );

			mesh = ( mesh_t * )buffer; buffer += sizeof( mesh_t );
			mesh->numVertexes = numVerts;
			mesh->numElems = numElems;

			mesh->xyzArray = ( vec4_t * )buffer; buffer += numVerts * sizeof( vec4_t );
			mesh->normalsArray = ( vec4_t * )buffer; buffer += numVerts * sizeof( vec4_t );
			mesh->stArray = ( vec2_t * )buffer; buffer += numVerts * sizeof( vec2_t );

			Mem_Copy( mesh->xyzArray, loadmodel_xyz_array + firstVert, numVerts * sizeof( vec4_t ) );
			Mem_Copy( mesh->normalsArray, loadmodel_normals_array + firstVert, numVerts * sizeof( vec4_t ) );
			Mem_Copy( mesh->stArray, loadmodel_st_array + firstVert, numVerts * sizeof( vec2_t ) );

			for( j = 0; j < LM_STYLES && in->lightmapStyles[j] != 255; j++ )
			{
				mesh->lmstArray[j] = ( vec2_t * )buffer; buffer += numVerts * sizeof( vec2_t );
				Mem_Copy( mesh->lmstArray[j], loadmodel_lmst_array[j] + firstVert, numVerts * sizeof( vec2_t ) );
			}
			for( j = 0; j < LM_STYLES && in->vertexStyles[j] != 255; j++ )
			{
				mesh->colorsArray[j] = ( rgba_t * )buffer; buffer += numVerts * sizeof( rgba_t );
				Mem_Copy( mesh->colorsArray[j], loadmodel_colors_array[j] + firstVert, numVerts * sizeof( rgba_t ) );
			}

			mesh->elems = ( elem_t * )buffer; buffer += numElems * sizeof( elem_t );
			Mem_Copy( mesh->elems, loadmodel_surfelems + firstElem, numElems * sizeof( elem_t ) );

			if( createSTverts )
			{
				mesh->sVectorsArray = ( vec4_t * )buffer; buffer += numVerts * sizeof( vec4_t );
				R_BuildTangentVectors( mesh->numVertexes, mesh->xyzArray, mesh->normalsArray, mesh->stArray, mesh->numElems / 3, mesh->elems, mesh->sVectorsArray );
			}

			if( out->facetype == MST_PLANAR )
			{
				cplane_t *plane;

				plane = out->plane = ( cplane_t * )buffer; buffer += sizeof( cplane_t );
				plane->type = PLANE_NONAXIAL;
				plane->signbits = 0;
				for( j = 0; j < 3; j++ )
				{
					plane->normal[j] = LittleFloat( in->normal[j] );
					if( plane->normal[j] == 1.0f )
						plane->type = j;
				}
				plane->dist = DotProduct( mesh->xyzArray[0], plane->normal );
			}
			break;
		}
	}

	return mesh;
}

/*
=================
Mod_LoadFaceCommon
=================
*/
static _inline void Mod_LoadFaceCommon( const dsurfacer_t *in, msurface_t *out )
{
	int j, shaderType;
	mesh_t *mesh;
	mfog_t *fog;
	mshaderref_t *shaderref;
	int shadernum, fognum;
	float *vert;
	mlightmapRect_t *lmRects[LM_STYLES];
	int lightmaps[LM_STYLES];
	byte lightmapStyles[LM_STYLES], vertexStyles[LM_STYLES];
	vec3_t ebbox = { 0, 0, 0 };

	out->facetype = LittleLong( in->facetype );

	// lighting info
	for( j = 0; j < LM_STYLES; j++ )
	{
		lightmaps[j] = LittleLong( in->lm_texnum[j] );
		if( lightmaps[j] < 0 || out->facetype == MST_FLARE )
		{
			lmRects[j] = NULL;
			lightmaps[j] = -1;
			lightmapStyles[j] = 255;
		}
		else if( lightmaps[j] >= loadmodel_numlightmaps )
		{
			MsgDev( D_WARN, "bad lightmap number: %i\n", lightmaps[j] );
			lmRects[j] = NULL;
			lightmaps[j] = -1;
			lightmapStyles[j] = 255;
		}
		else
		{
			lmRects[j] = &loadmodel_lightmapRects[lightmaps[j]];
			lightmaps[j] = lmRects[j]->texNum;
			lightmapStyles[j] = in->lightmapStyles[j];
		}
		vertexStyles[j] = in->vertexStyles[j];
	}

	// add this super style
	R_AddSuperLightStyle( lightmaps, lightmapStyles, vertexStyles, lmRects );

	shadernum = LittleLong( in->shadernum );
	if( shadernum < 0 || shadernum >= loadmodel_numshaderrefs )
		Host_Error( "MOD_LoadBmodel: bad shader number\n" );
	shaderref = loadmodel_shaderrefs + shadernum;

	if( out->facetype == MST_FLARE )
		shaderType = SHADER_FLARE;
	else if( /*out->facetype == FACETYPE_TRISURF || */ lightmaps[0] < 0 || lightmapStyles[0] == 255 )
		shaderType = SHADER_VERTEX;
	else shaderType = SHADER_TEXTURE;

	if( !shaderref->shader )
	{
		shaderref->shader = R_LoadShader( shaderref->name, shaderType, false, 0, SHADER_INVALID );
		out->shader = shaderref->shader;
	}
	else
	{
		// some q3a maps specify a lightmap shader for surfaces that do not have a lightmap,
		// workaround that... see pukka3tourney2 for example
		if( ( shaderType == SHADER_VERTEX && ( shaderref->shader->flags & SHADER_HASLIGHTMAP ) &&
			( shaderref->shader->stages[0].flags & SHADERSTAGE_LIGHTMAP )))
			out->shader = R_LoadShader( shaderref->name, shaderType, false, 0, shaderref->shader->type );
		else out->shader = shaderref->shader;
	}

	out->flags = shaderref->flags;
	if( tr.currentSkyShader == NULL && (out->flags & SURF_SKY || out->shader->flags & SHADER_SKYPARMS ))
	{
		// because sky shader may missing skyParms, but always has surfaceparm 'sky'
		tr.currentSkyShader = out->shader;
	}

	R_DeformvBBoxForShader( out->shader, ebbox );
	fognum = LittleLong( in->fognum );
	if( fognum != -1 && ( fognum < loadbmodel->numfogs ) )
	{
		fog = loadbmodel->fogs + fognum;
		if( fog->shader && fog->shader->fog_dist )
			out->fog = fog;
	}

	mesh = out->mesh = Mod_CreateMeshForSurface( in, out );
	if( !mesh )
		return;

	ClearBounds( out->mins, out->maxs );
	for( j = 0, vert = mesh->xyzArray[0]; j < mesh->numVertexes; j++, vert += 4 )
		AddPointToBounds( vert, out->mins, out->maxs );
	VectorSubtract( out->mins, ebbox, out->mins );
	VectorAdd( out->maxs, ebbox, out->maxs );
}

/*
=================
Mod_LoadFaces
=================
*/
static void Mod_LoadFaces( const lump_t *l )
{
	int i, j, count;
	dsurfaceq_t	*in;
	dsurfacer_t rdf;
	msurface_t *out;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Host_Error( "Mod_LoadFaces: funny lump size in %s\n", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadbmodel->surfaces = out;
	loadbmodel->numsurfaces = count;

	rdf.lightmapStyles[0] = rdf.vertexStyles[0] = 0;
	for( j = 1; j < LM_STYLES; j++ )
		rdf.lightmapStyles[j] = rdf.vertexStyles[j] = 255;

	for( i = 0; i < count; i++, in++, out++ )
	{
		rdf.facetype = in->facetype;
		rdf.lm_texnum[0] = in->lm_texnum;
		rdf.lightmapStyles[0] = rdf.vertexStyles[0] = 0;
		for( j = 1; j < LM_STYLES; j++ )
		{
			rdf.lm_texnum[j] = -1;
			rdf.lightmapStyles[j] = rdf.vertexStyles[j] = 255;
		}
		for( j = 0; j < 3; j++ )
		{
			rdf.origin[j] = in->origin[j];
			rdf.normal[j] = in->normal[j];
			rdf.mins[j] = in->mins[j];
			rdf.maxs[j] = in->maxs[j];
		}
		rdf.shadernum = in->shadernum;
		rdf.fognum = in->fognum;
		rdf.numverts = in->numverts;
		rdf.firstvert = in->firstvert;
		rdf.patch_cp[0] = in->patch_cp[0];
		rdf.patch_cp[1] = in->patch_cp[1];
		rdf.firstelem = in->firstelem;
		rdf.numelems = in->numelems;
		Mod_LoadFaceCommon( &rdf, out );
	}
}

/*
=================
Mod_LoadFaces_RBSP
=================
*/
static void Mod_LoadFaces_RBSP( const lump_t *l )
{
	int i, count;
	dsurfacer_t *in;
	msurface_t *out;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Host_Error( "Mod_LoadFaces: funny lump size in %s\n", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadbmodel->surfaces = out;
	loadbmodel->numsurfaces = count;

	for( i = 0; i < count; i++, in++, out++ )
		Mod_LoadFaceCommon( in, out );
}

/*
=================
Mod_LoadNodes
=================
*/
static void Mod_LoadNodes( const lump_t *l )
{
	int i, j, count, p;
	dnode_t	*in;
	mnode_t	*out;
	bool badBounds;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Host_Error( "Mod_LoadNodes: funny lump size in %s\n", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadbmodel->nodes = out;
	loadbmodel->numnodes = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->plane = loadbmodel->planes + LittleLong( in->planenum );

		for( j = 0; j < 2; j++ )
		{
			p = LittleLong( in->children[j] );
			if( p >= 0 )
				out->children[j] = loadbmodel->nodes + p;
			else
				out->children[j] = ( mnode_t * )( loadbmodel->leafs + ( -1 - p ) );
		}

		badBounds = false;
		for( j = 0; j < 3; j++ )
		{
			out->mins[j] = (float)LittleLong( in->mins[j] );
			out->maxs[j] = (float)LittleLong( in->maxs[j] );
			if( out->mins[j] > out->maxs[j] )
				badBounds = true;
		}

		if( badBounds || VectorCompare( out->mins, out->maxs ) )
		{
			MsgDev( D_WARN, "bad node %i bounds:\n", i );
			MsgDev( D_WARN, "mins: %i %i %i\n", Q_rint( out->mins[0] ), Q_rint( out->mins[1] ), Q_rint( out->mins[2] ) );
			MsgDev( D_WARN, "maxs: %i %i %i\n", Q_rint( out->maxs[0] ), Q_rint( out->maxs[1] ), Q_rint( out->maxs[2] ) );
		}
	}
}

/*
=================
Mod_LoadFogs
=================
*/
static void Mod_LoadFogs( const lump_t *l, const lump_t *brLump, const lump_t *brSidesLump )
{
	int		i, j, count, p;
	dfog_t		*in;
	mfog_t		*out;
	dbrush_t		*inbrushes, *brush;
	dbrushsideq_t	*inbrushsides = NULL, *brushside = NULL;
	dbrushsider_t	*inrbrushsides = NULL, *rbrushside = NULL;

	inbrushes = ( void * )( mod_base + brLump->fileofs );
	if( brLump->filelen % sizeof( *inbrushes ))
		Host_Error( "Mod_LoadBrushes: funny lump size in %s\n", loadmodel->name );

	if( mod_bspFormat->flags & (BSP_RAVEN|BSP_IGBSP))
	{
		inrbrushsides = (void *)(mod_base + brSidesLump->fileofs);
		if( brSidesLump->filelen % sizeof( *inrbrushsides ) )
			Host_Error( "Mod_LoadBrushsides: funny lump size in %s\n", loadmodel->name );
	}
	else
	{
		inbrushsides = (void *)(mod_base + brSidesLump->fileofs);
		if( brSidesLump->filelen % sizeof( *inbrushsides ) )
			Host_Error( "Mod_LoadBrushsides: funny lump size in %s\n", loadmodel->name );
	}

	in = (void *)(mod_base + l->fileofs);
	if( l->filelen % sizeof( *in ))
		Host_Error( "Mod_LoadFogs: funny lump size in %s\n", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count * sizeof( *out ));

	loadbmodel->fogs = out;
	loadbmodel->numfogs = count;
	loadmodel->numshaders += count;
	loadmodel->shaders = Mod_Realloc( loadmodel, loadmodel->shaders, loadmodel->numshaders * sizeof( ref_shader_t* ));

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->shader = R_LoadShader( in->shader, SHADER_TEXTURE, false, 0, SHADER_INVALID );
		p = LittleLong( in->brushnum );
		if( p == -1 ) continue;

		brush = inbrushes + p;
		p = LittleLong( brush->firstside );
		if( p == -1 )
		{
			out->shader = NULL;
			continue;
		}

		if( mod_bspFormat->flags & (BSP_RAVEN|BSP_IGBSP))
			rbrushside = inrbrushsides + p;
		else brushside = inbrushsides + p;

		p = LittleLong( in->visibleside );
		out->numplanes = LittleLong( brush->numsides );
		out->planes = Mod_Malloc( loadmodel, out->numplanes * sizeof( cplane_t ));

		if( mod_bspFormat->flags & (BSP_RAVEN|BSP_IGBSP))
		{
			if( p != -1 ) out->visibleplane = loadbmodel->planes + LittleLong( rbrushside[p].planenum );
			for( j = 0; j < out->numplanes; j++ )
				out->planes[j] = *( loadbmodel->planes + LittleLong( rbrushside[j].planenum ));
		}
		else
		{
			if( p != -1 ) out->visibleplane = loadbmodel->planes + LittleLong( brushside[p].planenum );
			for( j = 0; j < out->numplanes; j++ )
				out->planes[j] = *( loadbmodel->planes + LittleLong( brushside[j].planenum ));
		}
	}
}

/*
=================
Mod_LoadLeafs
=================
*/
static void Mod_LoadLeafs( const lump_t *l, const lump_t *msLump )
{
	int i, j, k, count, countMarkSurfaces;
	dleaf_t	*in;
	mleaf_t	*out;
	size_t size;
	byte *buffer;
	bool badBounds;
	int *inMarkSurfaces;
	int numVisLeafs;
	int numMarkSurfaces, firstMarkSurface;
	int numVisSurfaces, numFragmentSurfaces;

	inMarkSurfaces = ( void * )( mod_base + msLump->fileofs );
	if( msLump->filelen % sizeof( *inMarkSurfaces ) )
		Host_Error( "Mod_LoadMarksurfaces: funny lump size in %s\n", loadmodel->name );
	countMarkSurfaces = msLump->filelen / sizeof( *inMarkSurfaces );

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Host_Error( "Mod_LoadLeafs: funny lump size in %s\n", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadbmodel->leafs = out;
	loadbmodel->numleafs = count;

	numVisLeafs = 0;
	loadbmodel->visleafs = Mod_Malloc( loadmodel, ( count+1 )*sizeof( out ) );
	memset( loadbmodel->visleafs, 0, ( count+1 )*sizeof( out ) );

	for( i = 0; i < count; i++, in++, out++ )
	{
		badBounds = false;
		for( j = 0; j < 3; j++ )
		{
			out->mins[j] = (float)LittleLong( in->mins[j] );
			out->maxs[j] = (float)LittleLong( in->maxs[j] );
			if( out->mins[j] > out->maxs[j] ) badBounds = true;
		}
		out->cluster = LittleLong( in->cluster );

		if( i && ( badBounds || VectorCompare( out->mins, out->maxs )))
		{
			MsgDev( D_NOTE, "bad leaf %i bounds:\n", i );
			MsgDev( D_NOTE, "mins: %i %i %i\n", Q_rint( out->mins[0] ), Q_rint( out->mins[1] ), Q_rint( out->mins[2] ) );
			MsgDev( D_NOTE, "maxs: %i %i %i\n", Q_rint( out->maxs[0] ), Q_rint( out->maxs[1] ), Q_rint( out->maxs[2] ) );
			MsgDev( D_NOTE, "cluster: %i\n", LittleLong( in->cluster ) );
			MsgDev( D_NOTE, "surfaces: %i\n", LittleLong( in->numleafsurfaces ));
			MsgDev( D_NOTE, "brushes: %i\n", LittleLong( in->numleafbrushes ));
			out->cluster = -1;
		}

		if( loadbmodel->vis )
		{
			if( out->cluster >= loadbmodel->vis->numclusters )
				Host_Error( "MOD_LoadBmodel: leaf cluster > numclusters\n" );
		}

		out->plane = NULL;
		out->area = LittleLong( in->area ) + 1;

		numMarkSurfaces = LittleLong( in->numleafsurfaces );
		if( !numMarkSurfaces )
		{
			// out->cluster = -1;
			continue;
		}

		firstMarkSurface = LittleLong( in->firstleafsurface );
		if( firstMarkSurface < 0 || numMarkSurfaces + firstMarkSurface > countMarkSurfaces )
			Host_Error( "MOD_LoadBmodel: bad marksurfaces in leaf %i\n", i );

		numVisSurfaces = numFragmentSurfaces = 0;

		for( j = 0; j < numMarkSurfaces; j++ )
		{
			k = LittleLong( inMarkSurfaces[firstMarkSurface + j] );
			if( k < 0 || k >= loadbmodel->numsurfaces )
				Host_Error( "Mod_LoadMarksurfaces: bad surface number\n" );

			if( R_SurfPotentiallyVisible( loadbmodel->surfaces + k ) )
			{
				numVisSurfaces++;
				if( R_SurfPotentiallyFragmented( loadbmodel->surfaces + k ) )
					numFragmentSurfaces++;
			}
		}

		if( !numVisSurfaces )
		{
			//out->cluster = -1;
			continue;
		}

		size = numVisSurfaces + 1;
		if( numFragmentSurfaces )
			size += numFragmentSurfaces + 1;
		size *= sizeof( msurface_t * );

		buffer = ( byte * )Mod_Malloc( loadmodel, size );

		out->firstVisSurface = ( msurface_t ** )buffer;
		buffer += ( numVisSurfaces + 1 ) * sizeof( msurface_t * );
		if( numFragmentSurfaces )
		{
			out->firstFragmentSurface = ( msurface_t ** )buffer;
			buffer += ( numFragmentSurfaces + 1 ) * sizeof( msurface_t * );
		}

		numVisSurfaces = numFragmentSurfaces = 0;

		for( j = 0; j < numMarkSurfaces; j++ )
		{
			k = LittleLong( inMarkSurfaces[firstMarkSurface + j] );

			if( R_SurfPotentiallyVisible( loadbmodel->surfaces + k ) )
			{
				out->firstVisSurface[numVisSurfaces++] = loadbmodel->surfaces + k;
				if( R_SurfPotentiallyFragmented( loadbmodel->surfaces + k ) )
					out->firstFragmentSurface[numFragmentSurfaces++] = loadbmodel->surfaces + k;
			}
		}

		loadbmodel->visleafs[numVisLeafs++] = out;
	}

	loadbmodel->visleafs = Mem_Realloc( loadmodel->mempool, loadbmodel->visleafs, ( numVisLeafs+1 )*sizeof( out ) );
}

/*
=================
Mod_LoadElems
=================
*/
static void Mod_LoadElems( const lump_t *l )
{
	int i, count;
	int *in;
	elem_t	*out;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Host_Error( "Mod_LoadElems: funny lump size in %s\n", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadmodel_surfelems = out;
	loadmodel_numsurfelems = count;

	for( i = 0; i < count; i++ )
		out[i] = LittleLong( in[i] );
}

/*
=================
Mod_LoadPlanes
=================
*/
static void Mod_LoadPlanes( const lump_t *l )
{
	int i, j;
	cplane_t *out;
	dplane_t *in;
	int count;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Host_Error( "Mod_LoadPlanes: funny lump size in %s\n", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadbmodel->planes = out;
	loadbmodel->numplanes = count;

	for( i = 0; i < count; i++, in++, out++ )
	{
		out->type = PLANE_NONAXIAL;
		out->signbits = 0;

		for( j = 0; j < 3; j++ )
		{
			out->normal[j] = LittleFloat( in->normal[j] );
			if( out->normal[j] < 0 )
				out->signbits |= 1<<j;
			if( out->normal[j] == 1.0f )
				out->type = j;
		}
		out->dist = LittleFloat( in->dist );
	}
}

/*
=================
Mod_LoadLightgrid
=================
*/
static void Mod_LoadLightgrid( const lump_t *l )
{
	int		i, j, count;
	dlightgridq_t	*in;
	mgridlight_t	*out;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Host_Error( "Mod_LoadLightgrid: funny lump size in %s\n", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadbmodel->lightgrid = out;
	loadbmodel->numlightgridelems = count;

	// lightgrid is all 8 bit
	for( i = 0; i < count; i++, in++, out++ )
	{
		out->styles[0] = 0;
		for( j = 1; j < LM_STYLES; j++ )
			out->styles[j] = 255;
		out->direction[0] = in->direction[0];
		out->direction[1] = in->direction[1];
		for( j = 0; j < 3; j++ )
		{
			out->diffuse[0][j] = in->diffuse[j];
			out->ambient[0][j] = in->diffuse[j];
		}
	}
}

/*
=================
Mod_LoadLightgrid_RBSP
=================
*/
static void Mod_LoadLightgrid_RBSP( const lump_t *l )
{
	int count;
	dlightgridr_t *in;
	mgridlight_t *out;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Host_Error( "Mod_LoadLightgrid: funny lump size in %s\n", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadbmodel->lightgrid = out;
	loadbmodel->numlightgridelems = count;

	// lightgrid is all 8 bit
	Mem_Copy( out, in, count*sizeof( *out ) );
}

/*
=================
Mod_LoadLightArray
=================
*/
static void Mod_LoadLightArray( void )
{
	int		i, count;
	mgridlight_t	**out;

	count = loadbmodel->numlightgridelems;
	out = Mod_Malloc( loadmodel, sizeof( *out ) * count );

	loadbmodel->lightarray = out;
	loadbmodel->numlightarrayelems = count;

	for( i = 0; i < count; i++, out++ )
		*out = loadbmodel->lightgrid + i;
}

/*
=================
Mod_LoadLightArray_RBSP
=================
*/
static void Mod_LoadLightArray_RBSP( const lump_t *l )
{
	int i, count;
	short *in;
	mgridlight_t **out;

	in = ( void * )( mod_base + l->fileofs );
	if( l->filelen % sizeof( *in ) )
		Host_Error( "Mod_LoadLightArray: funny lump size in %s\n", loadmodel->name );
	count = l->filelen / sizeof( *in );
	out = Mod_Malloc( loadmodel, count*sizeof( *out ) );

	loadbmodel->lightarray = out;
	loadbmodel->numlightarrayelems = count;

	for( i = 0; i < count; i++, in++, out++ )
		*out = loadbmodel->lightgrid + LittleShort( *in );
}

/*
=================
Mod_LoadEntities
=================
*/
static void Mod_LoadEntities( const lump_t *l, vec3_t gridSize, vec3_t ambient, vec3_t outline )
{
	int	n;
	bool	isworld;
	float	celcolorf[3] = { 0, 0, 0 };
	float	gridsizef[3] = { 0, 0, 0 }, colorf[3] = { 0, 0, 0 }, ambientf = 0;
	char	key[MAX_KEY], value[MAX_VALUE];
	token_t	token;
	script_t	*ents;

	Com_Assert( gridSize == NULL );
	Com_Assert( ambient == NULL );
	Com_Assert( outline == NULL );

	VectorClear( gridSize );
	VectorClear( ambient );
	VectorClear( outline );

	ents = Com_OpenScript( LUMP_ENTITIES, (char *)mod_base + l->fileofs, l->filelen );
	if( !ents ) return;

	while( Com_ReadToken( ents, SC_ALLOW_NEWLINES, &token ))
	{
		isworld = false;

		while( 1 )
		{
			// parse key
			if( !Com_ReadToken( ents, SC_ALLOW_NEWLINES, &token ))
				Host_Error( "R_LoadEntities: EOF without closing brace\n" );
			if( token.string[0] == '}' ) break; // end of desc
			com.strncpy( key, token.string, sizeof( key ) - 1 );

			// parse value	
			if( !Com_ReadToken( ents, SC_ALLOW_PATHNAMES2, &token ))
				Host_Error( "R_LoadEntities: EOF without closing brace\n" );
			com.strncpy( value, token.string, sizeof( value ) - 1 );

			if( token.string[0] == '}' )
				Host_Error( "R_LoadEntities: closing brace without data\n" );

			// now that we have the key pair worked out...
			if( !com.strcmp( key, "classname" ) )
			{
				if( !com.strcmp( value, "worldspawn" ) )
					isworld = true;
			}
			else if( !com.strcmp( key, "gridsize" ) )
			{
				n = sscanf( value, "%f %f %f", &gridsizef[0], &gridsizef[1], &gridsizef[2] );
				if( n != 3 )
				{
					int	gridsizei[3] = { 0, 0, 0 };
					sscanf( value, "%i %i %i", &gridsizei[0], &gridsizei[1], &gridsizei[2] );
					VectorCopy( gridsizei, gridsizef );
				}
			}
			else if( !com.strcmp( key, "_ambient" ) || ( !com.strcmp( key, "ambient" ) && ambientf == 0.0f ))
			{
				sscanf( value, "%f", &ambientf );
				if( !ambientf )
				{
					int	ia = 0;
					sscanf( value, "%i", &ia );
					ambientf = ia;
				}
			}
			else if( !com.strcmp( key, "_color" ))
			{
				n = sscanf( value, "%f %f %f", &colorf[0], &colorf[1], &colorf[2] );
				if( n != 3 )
				{
					int	colori[3] = { 0, 0, 0 };
					sscanf( value, "%i %i %i", &colori[0], &colori[1], &colori[2] );
					VectorCopy( colori, colorf );
				}
			}
			else if( !com.strcmp( key, "_outlinecolor" ) )
			{
				n = sscanf( value, "%f %f %f", &celcolorf[0], &celcolorf[1], &celcolorf[2] );
				if( n != 3 )
				{
					int	celcolori[3] = { 0, 0, 0 };
					sscanf( value, "%i %i %i", &celcolori[0], &celcolori[1], &celcolori[2] );
					VectorCopy( celcolori, celcolorf );
				}
			}
		}

		if( isworld )
		{
			VectorCopy( gridsizef, gridSize );

			if( VectorCompare( colorf, vec3_origin ))
				VectorSet( colorf, 1.0, 1.0, 1.0 );
			VectorScale( colorf, ambientf, ambient );

			if( max( celcolorf[0], max( celcolorf[1], celcolorf[2] ) ) > 1.0f )
				VectorScale( celcolorf, 1.0f / 255.0f, celcolorf ); // [0..1] RGB -> [0..255] RGB
			VectorCopy( celcolorf, outline );
			break;
		}
	}
	Com_CloseScript( ents );
}

/*
=================
Mod_SetParent
=================
*/
static void Mod_SetParent( mnode_t *node, mnode_t *parent )
{
	node->parent = parent;
	if( !node->plane )
		return;
	Mod_SetParent( node->children[0], node );
	Mod_SetParent( node->children[1], node );
}

/*
=================
Mod_ApplySuperStylesToFace
=================
*/
static _inline void Mod_ApplySuperStylesToFace( const dsurfacer_t *in, msurface_t *out )
{
	int j, k;
	float *lmArray;
	mesh_t *mesh;
	mlightmapRect_t *lmRects[LM_STYLES];
	int lightmaps[LM_STYLES];
	byte lightmapStyles[LM_STYLES], vertexStyles[LM_STYLES];

	for( j = 0; j < LM_STYLES; j++ )
	{
		lightmaps[j] = LittleLong( in->lm_texnum[j] );

		if( lightmaps[j] < 0 || out->facetype == MST_FLARE || lightmaps[j] >= loadmodel_numlightmaps )
		{
			lmRects[j] = NULL;
			lightmaps[j] = -1;
			lightmapStyles[j] = 255;
		}
		else
		{
			lmRects[j] = &loadmodel_lightmapRects[lightmaps[j]];
			lightmaps[j] = lmRects[j]->texNum;

			if( mapConfig.lightmapsPacking )
			{                       // scale/shift lightmap coords
				mesh = out->mesh;
				lmArray = mesh->lmstArray[j][0];
				for( k = 0; k < mesh->numVertexes; k++, lmArray += 2 )
				{
					lmArray[0] = (double)( lmArray[0] ) * lmRects[j]->texMatrix[0][0] + lmRects[j]->texMatrix[0][1];
					lmArray[1] = (double)( lmArray[1] ) * lmRects[j]->texMatrix[1][0] + lmRects[j]->texMatrix[1][1];
				}
			}
			lightmapStyles[j] = in->lightmapStyles[j];
		}
		vertexStyles[j] = in->vertexStyles[j];
	}
	out->superLightStyle = R_AddSuperLightStyle( lightmaps, lightmapStyles, vertexStyles, lmRects );
}

/*
=================
Mod_Finish
=================
*/
static void Mod_Finish( const lump_t *faces, const lump_t *light, vec3_t gridSize, vec3_t ambient, vec3_t outline )
{
	int		i, j;
	msurface_t	*surf;
	mfog_t		*testFog;
	bool		globalFog;

	// set up lightgrid
	if( gridSize[0] < 1 || gridSize[1] < 1 || gridSize[2] < 1 )
		VectorSet( loadbmodel->gridSize, 64, 64, 128 );
	else VectorCopy( gridSize, loadbmodel->gridSize );

	for( j = 0; j < 3; j++ )
	{
		vec3_t maxs;

		loadbmodel->gridMins[j] = loadbmodel->gridSize[j] *ceil( ( loadbmodel->submodels[0].mins[j] + 1 ) / loadbmodel->gridSize[j] );
		maxs[j] = loadbmodel->gridSize[j] *floor( ( loadbmodel->submodels[0].maxs[j] - 1 ) / loadbmodel->gridSize[j] );
		loadbmodel->gridBounds[j] = ( maxs[j] - loadbmodel->gridMins[j] )/loadbmodel->gridSize[j] + 1;
	}
	loadbmodel->gridBounds[3] = loadbmodel->gridBounds[1] * loadbmodel->gridBounds[0];

	// ambient lighting
	for( i = 0; i < 3; i++ )
		mapConfig.ambient[i] = bound( 0, ambient[i]*( (float)( 1 << mapConfig.pow2MapOvrbr )/255.0f ), 1 );

	for( i = 0; i < 3; i++ )
		mapConfig.outlineColor[i] = (byte)(bound( 0, outline[i]*255.0f, 255 ));
	mapConfig.outlineColor[3] = 255;

	R_SortSuperLightStyles();

	// make shader links
	for( i = 0; i < loadmodel_numshaderrefs; i++ )
		loadmodel->shaders[i] = loadmodel_shaderrefs[i].shader;

	for( j = 0, testFog = loadbmodel->fogs; j < loadbmodel->numfogs; testFog++, j++, i++ )
	{
		// update fog shaders even if missing
		if( i < loadmodel->numshaders ) loadmodel->shaders[i] = testFog->shader;

		if( !testFog->shader ) continue;
		if( testFog->visibleplane ) continue;

		testFog->visibleplane = Mod_Malloc( loadmodel, sizeof( cplane_t ) );
		VectorSet( testFog->visibleplane->normal, 0, 0, 1 );
		testFog->visibleplane->type = PLANE_Z;
		testFog->visibleplane->dist = loadbmodel->submodels[0].maxs[0] + 1;
	}

	// make sure that the only fog in the map has valid shader
	globalFog = ( loadbmodel->numfogs == 1 );
	if( globalFog )
	{
		testFog = &loadbmodel->fogs[0];
		if( !testFog->shader )
			globalFog = false;
	}

	// apply super-lightstyles to map surfaces
	if( mod_bspFormat->flags & BSP_RAVEN )
	{
		dsurfacer_t *in = ( void * )( mod_base + faces->fileofs );

		for( i = 0, surf = loadbmodel->surfaces; i < loadbmodel->numsurfaces; i++, in++, surf++ )
		{
			if( globalFog && surf->mesh && surf->fog != testFog )
			{
				if( !( surf->shader->flags & SHADER_SKYPARMS ) && !surf->shader->fog_dist )
					globalFog = false;
			}

			if( !R_SurfPotentiallyVisible( surf ) )
				continue;
			Mod_ApplySuperStylesToFace( in, surf );
		}
	}
	else
	{
		dsurfacer_t	rdf;
		dsurfaceq_t	*in = ( void * )( mod_base + faces->fileofs );

		rdf.lightmapStyles[0] = rdf.vertexStyles[0] = 0;
		for( j = 1; j < LM_STYLES; j++ )
		{
			rdf.lm_texnum[j] = -1;
			rdf.lightmapStyles[j] = rdf.vertexStyles[j] = 255;
		}

		for( i = 0, surf = loadbmodel->surfaces; i < loadbmodel->numsurfaces; i++, in++, surf++ )
		{
			if( globalFog && surf->mesh && surf->fog != testFog )
			{
				if( !( surf->shader->flags & SHADER_SKYPARMS ) && !surf->shader->fog_dist )
					globalFog = false;
			}

			if( !R_SurfPotentiallyVisible( surf ) )
				continue;
			rdf.lm_texnum[0] = LittleLong( in->lm_texnum );
			Mod_ApplySuperStylesToFace( &rdf, surf );
		}
	}

	if( globalFog )
	{
		loadbmodel->globalfog = testFog;
		MsgDev( D_INFO, "Global fog detected: %s\n", testFog->shader->name );
	}

	if( loadmodel_xyz_array ) Mod_Free( loadmodel_xyz_array );
	if( loadmodel_surfelems ) Mod_Free( loadmodel_surfelems );
	if( loadmodel_lightmapRects ) Mod_Free( loadmodel_lightmapRects );
	if( loadmodel_shaderrefs ) Mod_Free( loadmodel_shaderrefs );

	Mod_SetParent( loadbmodel->nodes, NULL );
}

/*
=================
Mod_BrushLoadModel
=================
*/
void Mod_BrushLoadModel( ref_model_t *mod, const void *buffer )
{
	int	i, version;
	dheader_t	*header;
	mmodel_t	*bm;
	vec3_t	gridSize, ambient, outline;

	mod->type = mod_brush;
	if( mod != r_models ) Host_Error( "loaded a brush model after the world\n" );

	header = (dheader_t *)buffer;

	version = LittleLong( header->version );
	for( i = 0, mod_bspFormat = bspFormats; i < numBspFormats; i++, mod_bspFormat++ )
	{
		if( LittleLong(*(uint *)buffer) == mod_bspFormat->ident && ( version == mod_bspFormat->version ) )
			break;
	}
	if( i == numBspFormats )
		Host_Error( "Mod_LoadBrushModel: %s: unknown bsp format, version %i\n", mod->name, version );

	mod_base = (byte *)header;

	// swap all the lumps
	for( i = 0; i < sizeof( dheader_t )/4; i++ )
		( (int *)header )[i] = LittleLong( ( (int *)header )[i] );

	// load into heap
	Mod_LoadSubmodels( &header->lumps[LUMP_MODELS] );
	Mod_LoadEntities( &header->lumps[LUMP_ENTITIES], gridSize, ambient, outline );
	if( mod_bspFormat->flags & BSP_RAVEN )
		Mod_LoadVertexes_RBSP( &header->lumps[LUMP_VERTEXES] );
	else Mod_LoadVertexes( &header->lumps[LUMP_VERTEXES] );
	Mod_LoadElems( &header->lumps[LUMP_ELEMENTS] );
	Mod_LoadLighting( &header->lumps[LUMP_LIGHTING], &header->lumps[LUMP_SURFACES] );
	if( mod_bspFormat->flags & BSP_RAVEN )
		Mod_LoadLightgrid_RBSP( &header->lumps[LUMP_LIGHTGRID] );
	else Mod_LoadLightgrid( &header->lumps[LUMP_LIGHTGRID] );
	Mod_LoadShaderrefs( &header->lumps[LUMP_SHADERS] );
	Mod_LoadPlanes( &header->lumps[LUMP_PLANES] );
	Mod_LoadFogs( &header->lumps[LUMP_FOGS], &header->lumps[LUMP_BRUSHES], &header->lumps[LUMP_BRUSHSIDES] );
	if( mod_bspFormat->flags & (BSP_RAVEN|BSP_IGBSP))
		Mod_LoadFaces_RBSP( &header->lumps[LUMP_SURFACES] );
	else Mod_LoadFaces( &header->lumps[LUMP_SURFACES] );
	Mod_LoadLeafs( &header->lumps[LUMP_LEAFS], &header->lumps[LUMP_LEAFSURFACES] );
	Mod_LoadNodes( &header->lumps[LUMP_NODES] );
	if( mod_bspFormat->flags & BSP_RAVEN )
		Mod_LoadLightArray_RBSP( &header->lumps[LUMP_LIGHTARRAY] );
	else Mod_LoadLightArray();

	Mod_Finish( &header->lumps[LUMP_SURFACES], &header->lumps[LUMP_LIGHTING], gridSize, ambient, outline );
	mod->touchFrame = tr.registration_sequence; // register model

	// set up the submodels
	for( i = 0; i < loadbmodel->numsubmodels; i++ )
	{
		ref_model_t	*starmod;
		mbrushmodel_t	*bmodel;

		bm = &loadbmodel->submodels[i];
		starmod = &r_inlinemodels[i];
		bmodel = (mbrushmodel_t *)starmod->extradata;

		Mem_Copy( starmod, mod, sizeof( ref_model_t ));
		Mem_Copy( bmodel, mod->extradata, sizeof( mbrushmodel_t ));

		bmodel->firstmodelsurface = bmodel->surfaces + bm->firstface;
		bmodel->nummodelsurfaces = bm->numfaces;
		starmod->extradata = bmodel;

		VectorCopy( bm->maxs, starmod->maxs );
		VectorCopy( bm->mins, starmod->mins );
		starmod->radius = bm->radius;

		if( i == 0 ) *mod = *starmod;
		else bmodel->numsubmodels = 0;
	}
}
//=============================================================================

/*
=================
R_RegisterWorldModel

Specifies the model that will be used as the world
=================
*/
void R_BeginRegistration( const char *mapname, const dvis_t *visData )
{
	string	fullname;

	tr.registration_sequence++;
	mapConfig.pow2MapOvrbr = 0;
	mapConfig.lightmapsPacking = false;
	mapConfig.deluxeMaps = false;
	mapConfig.deluxeMappingEnabled = false;

	VectorClear( mapConfig.ambient );
	VectorClear( mapConfig.outlineColor );

	com.sprintf( fullname, "maps/%s.bsp", mapname );

	// explicitly free the old map if different
	if( com.strcmp( r_models[0].name, fullname ))
	{
		Mod_FreeModel( &r_models[0] );
		R_NewMap ();
	}
	else
	{
		// update progress bar
		Cvar_SetValue( "scr_loading", 50.0f );
		if( ri.UpdateScreen ) ri.UpdateScreen();
	}
	
	if( r_lighting_packlightmaps->integer )
	{
		string	lightmapsPath;
		char	*p;

		mapConfig.lightmapsPacking = true;

		com.strncpy( lightmapsPath, fullname, sizeof( lightmapsPath ));
		p = com.strrchr( lightmapsPath, '.' );
		if( p )
		{
			*p = 0;
			com.strncat( lightmapsPath, "/lm_0000.tga", sizeof( lightmapsPath ) );
			if( FS_FileExists( lightmapsPath ))
			{
				MsgDev( D_INFO, "External lightmap stage: lightmaps packing is disabled\n" );
				mapConfig.lightmapsPacking = false;
			}
		}
	}

	r_farclip_min = Z_NEAR;		// sky shaders will most likely modify this value
	r_environment_color->modified = true;

	r_worldmodel = Mod_ForName( fullname, true );
	r_worldbrushmodel = (mbrushmodel_t *)r_worldmodel->extradata;
	r_worldbrushmodel->vis = (dvis_t *)visData;
	r_worldmodel->type = mod_world;

	r_worldent->scale = 1.0f;
	r_worldent->model = r_worldmodel;
	r_worldent->rtype = RT_MODEL;
	r_worldent->ent_type = ED_NORMAL;
	r_worldent->renderamt = 255;		// i'm hope we don't want to see semisolid world :) 
	Matrix3x3_LoadIdentity( r_worldent->axis );
	Mod_UpdateShaders( r_worldmodel );

	r_framecount = r_framecount2 = 1;
	r_oldviewcluster = r_viewcluster = -1;  // force markleafs
}

void R_EndRegistration( const char *skyname )
{
	int		i;
	ref_model_t	*mod;

	if( skyname && com.strncmp( skyname, "<skybox>", 8 ))
	{
		// half-life or quake2 skybox-style
		R_SetupSky( skyname );
	}

	for( i = 0, mod = r_models; i < r_nummodels; i++, mod++ )
	{
		if( !mod->name ) continue;
		if( mod->touchFrame != tr.registration_sequence )
			Mod_FreeModel( mod );
	}
	R_ShaderFreeUnused();
}

/*
=================
R_RegisterModel
=================
*/
ref_model_t *R_RegisterModel( const char *name )
{
	ref_model_t	*mod;
	
	mod = Mod_ForName( name, false );
	Mod_UpdateShaders( mod );

	return mod;
}

/*
=================
R_ModelBounds
=================
*/
void R_ModelBounds( const ref_model_t *model, vec3_t mins, vec3_t maxs )
{
	if( model )
	{
		VectorCopy( model->mins, mins );
		VectorCopy( model->maxs, maxs );
	}
	else if( r_worldmodel )
	{
		VectorCopy( r_worldmodel->mins, mins );
		VectorCopy( r_worldmodel->maxs, maxs );
	}
}
