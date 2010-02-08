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
#ifndef __R_BACKEND_H__
#define __R_BACKEND_H__

#define MAX_TEXTURES		4096
#define MAX_ARRAY_VERTS		4096
#define MAX_ARRAY_ELEMENTS		MAX_ARRAY_VERTS*6
#define MAX_ARRAY_TRIANGLES		MAX_ARRAY_ELEMENTS/3
#define MAX_ARRAY_NEIGHBORS		MAX_ARRAY_TRIANGLES*3

enum
{
	VBO_VERTS,
	VBO_NORMALS,
	VBO_COLORS,
	VBO_TC0,
	VBO_ENDMARKER
};

#define MAX_SHADOWGROUPS    		32
#define MAX_VERTEX_BUFFER_OBJECTS	VBO_ENDMARKER+MAX_TEXTURE_UNITS-1

extern ALIGN( 16 ) vec4_t inVertsArray[MAX_ARRAY_VERTS];
extern ALIGN( 16 ) vec4_t inNormalsArray[MAX_ARRAY_VERTS];
extern vec4_t inSVectorsArray[MAX_ARRAY_VERTS];
extern elem_t inElemsArray[MAX_ARRAY_ELEMENTS];
extern vec2_t inCoordsArray[MAX_ARRAY_VERTS];
extern vec2_t inLightmapCoordsArray[LM_STYLES][MAX_ARRAY_VERTS];
extern rgba_t inColorsArray[LM_STYLES][MAX_ARRAY_VERTS];

extern elem_t *elemsArray;
extern vec4_t *vertsArray;
extern vec4_t *normalsArray;
extern vec4_t *sVectorsArray;
extern vec2_t *coordsArray;
extern vec2_t *lightmapCoordsArray[LM_STYLES];
extern rgba_t colorArray[MAX_ARRAY_VERTS];
extern int r_features;

//===================================================================
typedef struct ref_buffer_s
{
	byte		*pointer;
	int		size;
	uint		usage;
	uint		bufNum;
} ref_buffer_t;

typedef struct
{
	uint		numVerts;
	uint		numElems;
	uint		numColors;

	uint		c_totalVerts;
	uint		c_totalTris;
	uint		c_totalFlushes;
	uint		c_totalKeptLocks;
} ref_backacc_t;

typedef struct
{
	// renderer global variables
	int		registration_sequence;
	kRenderMode_t	iRenderMode;

	// vbo stuff
	int		numVertexBufferObjects;
	ref_buffer_t	vertexBufferObjects[MAX_VERTEX_BUFFER_OBJECTS];
	ref_buffer_t	*vertexBuffer;
	ref_buffer_t	*normalBuffer;
	ref_buffer_t	*colorsBuffer;
	ref_buffer_t	*tcoordBuffer[MAX_TEXTURE_UNITS];

	// OpenGL matrix states
	bool		modelviewIdentity;

	// builtin textures
	texture_t		*cinTexture;      	// cinematic texture
	texture_t		*portaltexture1;   	// portal view
	texture_t		*portaltexture2;  	// refraction image for distortions
	texture_t		*defaultTexture;   	// use for bad textures
	texture_t		*particleTexture; 	// little dot for particles
	texture_t		*whiteTexture;
	texture_t		*blackTexture;
	texture_t		*blankbumpTexture;
	texture_t		*dlightTexture;
	texture_t		*dspotlightTexture;
	texture_t		*normalizeTexture;	// cubemap
	texture_t		*fogTexture;
	texture_t		*skyTexture;
	texture_t		*coronaTexture;
	texture_t		*defaultConchars;
	texture_t		*shadowmapTextures[MAX_SHADOWGROUPS];
	texture_t		*lightmapTextures[MAX_TEXTURES];

	// utility shaders
	ref_shader_t	*defaultShader;	// generic black texture
	ref_shader_t	*fillShader;	// generic white texture
	ref_shader_t	*currentSkyShader;	// ponter to sky shader for current map 
} ref_globals_t;

extern ref_globals_t tr;
extern ref_backacc_t r_backacc;

//===================================================================

void R_BackendInit( void );
void R_BackendShutdown( void );
void R_BackendStartFrame( void );
void R_BackendEndFrame( void );
void R_BackendResetCounters( void );

void R_BackendBeginTriangleOutlines( void );
void R_BackendEndTriangleOutlines( void );

void R_BackendCleanUpTextureUnits( void );

void R_BackendSetPassMask( int mask );
void R_BackendResetPassMask( void );

void R_DrawPhysDebug( void );

void R_InitVertexBuffers( void );
void R_ShutdownVertexBuffers( void );
ref_buffer_t *R_AllocVertexBuffer( size_t size, GLuint usage );
void R_UpdateVertexBuffer( ref_buffer_t *vertexBuffer, const void *data, size_t size );

void R_LockArrays( int numverts );
void R_UnlockArrays( void );
void R_UnlockArrays( void );
void R_FlushArrays( void );
void R_FlushArraysMtex( void );
void R_ClearArrays( void );

static _inline void R_PushElems( elem_t *elems, int count, int features )
{
	elem_t	*currentElem;

	// this is a fast path for non-batched geometry, use carefully
	// used on pics, sprites, .dpm, .md3 and .md2 models
	if( features & MF_NONBATCHED )
	{
		// simply change elemsArray to point at elems
		r_backacc.numElems = count;
		elemsArray = currentElem = elems;
	}
	else
	{
		currentElem = elemsArray + r_backacc.numElems;
		r_backacc.numElems += count;

		// the following code assumes that R_PushElems is fed with triangles...
		for(; count > 0; count -= 3, elems += 3, currentElem += 3 )
		{
			currentElem[0] = r_backacc.numVerts + elems[0];
			currentElem[1] = r_backacc.numVerts + elems[1];
			currentElem[2] = r_backacc.numVerts + elems[2];
		}
	}
}

static _inline void R_PushTrifanElems( int numverts )
{
	int count;
	elem_t *currentElem;

	currentElem = elemsArray + r_backacc.numElems;
	r_backacc.numElems += numverts + numverts + numverts - 6;

	for( count = 2; count < numverts; count++, currentElem += 3 )
	{
		currentElem[0] = r_backacc.numVerts;
		currentElem[1] = r_backacc.numVerts + count - 1;
		currentElem[2] = r_backacc.numVerts + count;
	}
}

static _inline void R_PushMesh( const mesh_t *mesh, int features )
{
	int numverts;

	if( !mesh || !( mesh->elems || ( features & MF_TRIFAN ) ) || !mesh->xyzArray )
		return;

	r_features = features;

	if( features & MF_TRIFAN )
		R_PushTrifanElems( mesh->numVertexes );
	else
		R_PushElems( mesh->elems, mesh->numElems, features );

	numverts = mesh->numVertexes;

	if( features & MF_NONBATCHED )
	{
		if( features & MF_DEFORMVS )
		{
			if( mesh->xyzArray != inVertsArray )
				Mem_Copy( inVertsArray, mesh->xyzArray, numverts * sizeof( vec4_t ) );

			if( ( features & MF_NORMALS ) && mesh->normalsArray && ( mesh->normalsArray != inNormalsArray ) )
				Mem_Copy( inNormalsArray, mesh->normalsArray, numverts * sizeof( vec4_t ) );
		}
		else
		{
			vertsArray = mesh->xyzArray;

			if( ( features & MF_NORMALS ) && mesh->normalsArray )
				normalsArray = mesh->normalsArray;
		}

		if( ( features & MF_STCOORDS ) && mesh->stArray )
			coordsArray = mesh->stArray;

		if( ( features & MF_LMCOORDS ) && mesh->lmstArray[0] )
		{
			lightmapCoordsArray[0] = mesh->lmstArray[0];
			if( features & MF_LMCOORDS1 )
			{
				lightmapCoordsArray[1] = mesh->lmstArray[1];
				if( features & MF_LMCOORDS2 )
				{
					lightmapCoordsArray[2] = mesh->lmstArray[2];
					if( features & MF_LMCOORDS3 )
						lightmapCoordsArray[3] = mesh->lmstArray[3];
				}
			}
		}

		if( ( features & MF_SVECTORS ) && mesh->sVectorsArray )
			sVectorsArray = mesh->sVectorsArray;
	}
	else
	{
		if( mesh->xyzArray != inVertsArray )
			Mem_Copy( inVertsArray[r_backacc.numVerts], mesh->xyzArray, numverts * sizeof( vec4_t ) );

		if( ( features & MF_NORMALS ) && mesh->normalsArray && (mesh->normalsArray != inNormalsArray ) )
			Mem_Copy( inNormalsArray[r_backacc.numVerts], mesh->normalsArray, numverts * sizeof( vec4_t ) );

		if( ( features & MF_STCOORDS ) && mesh->stArray && (mesh->stArray != inCoordsArray ) )
			Mem_Copy( inCoordsArray[r_backacc.numVerts], mesh->stArray, numverts * sizeof( vec2_t ) );

		if( ( features & MF_LMCOORDS ) && mesh->lmstArray[0] )
		{
			Mem_Copy( inLightmapCoordsArray[0][r_backacc.numVerts], mesh->lmstArray[0], numverts * sizeof( vec2_t ) );
			if( features & MF_LMCOORDS1 )
			{
				Mem_Copy( inLightmapCoordsArray[1][r_backacc.numVerts], mesh->lmstArray[1], numverts * sizeof( vec2_t ) );
				if( features & MF_LMCOORDS2 )
				{
					Mem_Copy( inLightmapCoordsArray[2][r_backacc.numVerts], mesh->lmstArray[2], numverts * sizeof( vec2_t ) );
					if( features & MF_LMCOORDS3 )
						Mem_Copy( inLightmapCoordsArray[3][r_backacc.numVerts], mesh->lmstArray[3], numverts * sizeof( vec2_t ) );
				}
			}
		}

		if( ( features & MF_SVECTORS ) && mesh->sVectorsArray && (mesh->sVectorsArray != inSVectorsArray ) )
			Mem_Copy( inSVectorsArray[r_backacc.numVerts], mesh->sVectorsArray, numverts * sizeof( vec4_t ) );
	}

	if( ( features & MF_COLORS ) && mesh->colorsArray[0] )
	{
		Mem_Copy( inColorsArray[0][r_backacc.numVerts], mesh->colorsArray[0], numverts * sizeof( rgba_t ) );
		if( features & MF_COLORS1 )
		{
			Mem_Copy( inColorsArray[1][r_backacc.numVerts], mesh->colorsArray[1], numverts * sizeof( rgba_t ) );
			if( features & MF_COLORS2 )
			{
				Mem_Copy( inColorsArray[2][r_backacc.numVerts], mesh->colorsArray[2], numverts * sizeof( rgba_t ) );
				if( features & MF_COLORS3 )
					Mem_Copy( inColorsArray[3][r_backacc.numVerts], mesh->colorsArray[3], numverts * sizeof( rgba_t ) );
			}
		}
	}

	r_backacc.numVerts += numverts;
	r_backacc.c_totalVerts += numverts;
}

static _inline bool R_MeshOverflow( const mesh_t *mesh )
{
	if( !mesh ) return false;
	return ( r_backacc.numVerts + mesh->numVertexes > MAX_ARRAY_VERTS ||
		r_backacc.numElems + mesh->numElems > MAX_ARRAY_ELEMENTS );
}

static _inline bool R_MeshOverflow2( const mesh_t *mesh1, const mesh_t *mesh2 )
{
	return ( r_backacc.numVerts + mesh1->numVertexes + mesh2->numVertexes > MAX_ARRAY_VERTS ||
		r_backacc.numElems + mesh1->numElems + mesh2->numElems > MAX_ARRAY_ELEMENTS );
}

static _inline bool R_InvalidMesh( const mesh_t *mesh )
{
	return ( !mesh->numVertexes || !mesh->numElems ||
		mesh->numVertexes > MAX_ARRAY_VERTS || mesh->numElems > MAX_ARRAY_ELEMENTS );
}

void R_RenderMeshBuffer( const meshbuffer_t *mb );

/*
=======================================================================

 GL STATE MACHINE

=======================================================================
*/
// render supported extensions
// this part are shared with engine.dll
#define R_OPENGL_110		0	// base
#define R_WGL_SWAPCONTROL		1		
#define R_HARDWARE_GAMMA_CONTROL	2	
#define R_ARB_VERTEX_BUFFER_OBJECT_EXT	3
#define R_ENV_COMBINE_EXT		4
#define R_ARB_MULTITEXTURE		5
#define R_TEXTURECUBEMAP_EXT		6
#define R_DOT3_ARB_EXT		7
#define R_ANISOTROPY_EXT		8
#define R_TEXTURE_LODBIAS		9
#define R_OCCLUSION_QUERIES_EXT	10
#define R_TEXTURE_COMPRESSION_EXT	11
#define R_SHADER_GLSL100_EXT		12

// this part are private for render.dll
#define R_WGL_3DFX_GAMMA_CONTROL	13
#define R_SGIS_MIPMAPS_EXT		14
#define R_DRAW_RANGEELEMENTS_EXT	15
#define R_LOCKARRAYS_EXT		16
#define R_TEXTURE_3D_EXT		17
#define R_CLAMPTOEDGE_EXT		18
#define R_BLEND_MINMAX_EXT		19
#define R_STENCILTWOSIDE_EXT		20
#define R_BLEND_SUBTRACT_EXT		21
#define R_SHADER_OBJECTS_EXT		22
#define R_VERTEX_SHADER_EXT		23	// glsl vertex program
#define R_FRAGMENT_SHADER_EXT		24	// glsl fragment program	
#define R_EXT_POINTPARAMETERS		25
#define R_SEPARATESTENCIL_EXT		26
#define R_ARB_TEXTURE_NPOT_EXT	27
#define R_CUSTOM_VERTEX_ARRAY_EXT	28
#define R_TEXTURE_ENV_ADD_EXT		29
#define R_CLAMP_TEXBORDER_EXT		30
#define R_DEPTH_TEXTURE		31
#define R_SHADOW_EXT		32
#define R_GLSL_NO_HALF_TYPES		33	// fake extension
#define R_GLSL_BRANCHING		34	// fake extension
#define R_EXTCOUNT			35

typedef struct
{
	const char	*renderer_string;		// ptrs to OpenGL32.dll, use with caution
	const char	*vendor_string;
	const char	*version_string;

	// list of supported extensions
	const char	*extensions_string;
	byte		extension[R_EXTCOUNT];

	int		max_texture_units;
	GLint		max_2d_texture_size;
	GLint		max_2d_rectangle_size;
	GLint		max_3d_texture_size;
	GLint		max_cubemap_texture_size;
	GLint		texRectangle;

	GLfloat		max_texture_anisotropy;
	GLfloat		max_texture_lodbias;

	int		color_bits;
	int		depth_bits;
	int		stencil_bits;

	bool		allowCDS;
	bool		deviceSupportsGamma;
	int		prev_mode;
} glconfig_t;

typedef struct
{
	int				flags;

	word		gammaRamp[768];		// current gamma ramp
	word		stateRamp[768];		// original gamma ramp

	int		width, height;
	bool		fullScreen;
	bool		wideScreen;

	bool		initializedMedia;

	int		activeTMU;
	GLuint		currentTextures[MAX_TEXTURE_UNITS];
	GLenum		currentEnvModes[MAX_TEXTURE_UNITS];
	GLboolean		texIdentityMatrix[MAX_TEXTURE_UNITS];
	GLint		genSTEnabled[MAX_TEXTURE_UNITS];	// 0 - disabled, OR 1 - S, OR 2 - T, OR 4 - R
	GLint		texCoordArrayMode[MAX_TEXTURE_UNITS];	// 0 - disabled, 1 - enabled, 2 - cubemap

	rgba_t		draw_color;
	kRenderMode_t	draw_rendermode;			// rendermode for drawing
	int		draw_frame;			// will be reset after each drawing

	int		faceCull;
	int		frontFace;

	bool		stencilEnabled;
	bool		in2DMode;
} glstate_t;

typedef struct
{
	bool		fActive;		// drawing in progress
	kRenderMode_t	currentRenderMode;
	shader_t		currentShader;
	int		vertexState;
	int		drawMode;
	rgba_t		color;

	int		features;
	int		numVertex;
	int		numColor;
	int		numIndex;

	bool		noCulling;
	bool		checkFlush;
	bool		hasNormals;
} tristate_t;

extern glconfig_t	glConfig;
extern glstate_t	glState;
extern tristate_t	triState;
	
// r_register.c
void GL_InitBackend( void );
bool GL_Support( int r_ext );
void GL_InitExtensions( void );
void GL_ShutdownBackend( void );
void GL_UpdateSwapInterval( void );
void GL_UpdateGammaRamp( void );
void GL_SetExtension( int r_ext, int enable );
void GL_BuildGammaTable( void );

bool R_Init_OpenGL( void );
void R_Free_OpenGL( void );

#endif /*__R_BACKEND_H__*/
