//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        r_local.h - render internal types
//=======================================================================

#ifndef R_LOCAL_H
#define R_LOCAL_H

#include <windows.h>
#include "launch_api.h"
#include "qfiles_ref.h"
#include "engine_api.h"
#include "entity_def.h"
#include "clgame_api.h"
#include "render_api.h"
#include "r_opengl.h"

extern stdlib_api_t com;		// engine toolbox
extern render_imp_t	 ri;
extern byte *r_temppool;
#define Host_Error			com.error

// limits
#define MAX_TEXTURE_UNITS		8
#define MAX_LIGHTMAPS		128
#define MAX_PROGRAMS		512
#define MAX_ENTITIES		1024
#define MAX_VERTEX_BUFFERS		2048
#define MAX_POLYS			4096
#define MAX_POLY_VERTS		16384
#define MAX_CLIPFLAGS		15	// all sides of bbox are valid
#define LM_SIZE			256	// LM_SIZE x LM_SIZE (width x height)

/*
=======================================================================

 TEXTURE MANAGER

=======================================================================
*/

#define MAX_TEXTURES		8192
#define TEXTURES_HASH_SIZE		2048

typedef enum
{
	TF_STATIC		= BIT(0),
	TF_NOPICMIP	= BIT(1),
	TF_UNCOMPRESSED	= BIT(2),
	TF_INTENSITY	= BIT(3),
	TF_ALPHA		= BIT(4),
	TF_NORMALMAP	= BIT(5),
	TF_CUBEMAP	= BIT(6),
	TF_LIGHTMAP	= BIT(7),
} texFlags_t;

typedef enum
{
	TF_DEFAULT = 0,
	TF_NEAREST,
	TF_LINEAR
} texFilter_t;

typedef enum
{
	TW_REPEAT = 0,
	TW_CLAMP,
	TW_CLAMP_TO_ZERO,
	TW_CLAMP_TO_ZERO_ALPHA
} texWrap_t;

typedef struct texture_s
{
	string		name;		// game path, including extension
	int		srcWidth;		// source dims, used for mipmap loading
	int		srcHeight;
	int		numLayers;	// framecount

	int		width;		// upload width\height
	int		height;

	texFlags_t	flags;
	texFilter_t	filter;
	texWrap_t		wrap;
	pixformat_t	type;		// PFDesc[type].glType
	size_t		size;		// upload size for debug targets

	GLint		format;		// PFDesc[type].glType
	GLuint		target;		// glTarget
	GLint		texnum;		// gl texture binding
	GLint		samples;		// gl samples

	int		touchFrame;	// 0 = free
	struct texture_s	*nextHash;
} texture_t;

extern texture_t	*r_defaultTexture;
extern texture_t	*r_defaultConchars;
extern texture_t	*r_whiteTexture;
extern texture_t	*r_blackTexture;
extern texture_t	*r_skyTexture;
extern texture_t	*r_rawTexture;
extern texture_t	*r_dlightTexture;
extern texture_t	*r_particleTexture;
extern texture_t	*r_lightmapTextures[MAX_LIGHTMAPS];
extern texture_t	*r_normalizeTexture;
extern texture_t	*r_radarMap;
extern texture_t	*r_aroundMap;
extern byte	*r_framebuffer;

void		R_SetTextureParameters( void );
void		R_TextureList_f( void );
texture_t		*R_CreateImage( const char *name, byte *buf, int w, int h, texFlags_t texFlags, texFilter_t filter, texWrap_t wrap );
texture_t		*R_LoadTexture( const char *name, rgbdata_t *pic, int samples, texFlags_t flags, texFilter_t filter, texWrap_t wrap );
texture_t		*R_FindTexture( const char *name, const byte *buf, size_t size, texFlags_t flags, texFilter_t filter, texWrap_t wrap );
texture_t		*R_FindCubeMapTexture( const char *name, const byte *buf, size_t size, texFlags_t flags, texFilter_t filter, texWrap_t wrap );
void		R_InitTextures( void );
void		R_ShutdownTextures( void );
void		R_FreeImage( texture_t *image );

/*
 =======================================================================

 PROGRAM MANAGER

 =======================================================================
*/

typedef struct program_s
{
	string		name;

	uint		target;
	uint		progNum;

	struct program_s	*nextHash;
} program_t;

extern program_t	*r_defaultVertexProgram;
extern program_t	*r_defaultFragmentProgram;

void		R_ProgramList_f( void );
program_t		*R_FindProgram( const char *name, uint target );
void		R_InitPrograms( void );
void		R_ShutdownPrograms( void );

#include "r_shader.h"

extern ref_shader_t	r_shaders[MAX_SHADERS];
extern int	r_numShaders;

void	R_EvaluateRegisters( ref_shader_t *shader, float time, const float *entityParms, const float *globalParms );
ref_shader_t *R_FindShader( const char *name, int shaderType, uint surfaceParm );
void	R_ShaderSetSpriteTexture( texture_t *mipTex );
void	R_ShaderAddSpriteIntervals( float interval );
void	R_ShaderFreeUnused( void );
void	R_ShaderList_f( void );
void	R_InitShaders( void );
void	R_ShutdownShaders( void );

/*
=======================================================================

BRUSH MODELS

=======================================================================
*/
#define SURF_PLANEBACK		1 // fast surface culling
#define SURF_WATERCAUSTICS		2
#define SURF_SLIMECAUSTICS		4
#define SURF_LAVACAUSTICS		8

#define CONTENTS_NODE		-1
#define SKY_SIZE			16
#define SKY_INDICES			(SKY_SIZE * SKY_SIZE * 6)
#define SKY_VERTICES		((SKY_SIZE+1) * (SKY_SIZE+1))

#define SURF_WATERCAUSTICS		2
#define SURF_SLIMECAUSTICS		4
#define SURF_LAVACAUSTICS		8

typedef struct dlight_s
{
	vec3_t	origin;
	vec3_t	color;
	float	intensity;
} dlight_t;

typedef struct lightstyle_s
{
	float		rgb[3];		// 0.0 - 2.0
	float		white;		// highest of rgb
} lightstyle_t;

typedef struct particle_s
{
	ref_shader_t	*shader;
	vec3_t		origin1;
	vec3_t		origin2;
	float		radius;
	float		length;
	float		rotation;
	vec4_t		modulate;
} particle_t;

typedef struct
{
	vec3_t		xyz;
	vec2_t		st;
	vec2_t		lm;
	vec4_t		color;
} surfPolyVert_t;

typedef struct surfPoly_s
{
	struct surfPoly_s	*next;

	int		numIndices;
	int		numVertices;

	uint		*indices;
	surfPolyVert_t	*vertices;
} surfPoly_t;

typedef struct
{
	ref_shader_t	*shader;
	int		numVerts;
	polyVert_t	*verts;
} poly_t;

typedef struct texInfo_s
{
	float		vecs[2][4];
	int		contentFlags;
	int		surfaceFlags;
	uint		width;
	uint		height;
	ref_shader_t	*shader;		// texture shader
} texInfo_t;

typedef struct
{
	int		flags;

	int		firstEdge;	// look up in model->edges[]. negative
	int		numEdges;		// numbers are backwards edges

	cplane_t		*plane;

	vec3_t		mins;
	vec3_t		maxs;

	short		textureMins[2];
	short		extents[2];

	surfPoly_t	*poly;			// multiple if subdivided

	vec3_t		tangent;
	vec3_t		binormal;
	vec3_t		normal;

	texInfo_t		*texInfo;

	int		visFrame;
	int		fragmentFrame;

	// lighting info
	int		dlightFrame;
	int		dlightBits;

	int		lmWidth;
	int		lmHeight;
	int		lmS;
	int		lmT;
	int		lmNum;
	byte		*lmSamples;
	int		numStyles;
	byte		styles[MAX_LIGHTSTYLES];
	float		cachedLight[MAX_LIGHTSTYLES];	// values currently used in lightmap
} surface_t;

typedef struct node_s
{
	// common with leaf
	int		contents;		// -1, to differentiate from leafs
	int		visFrame;		// Node needs to be traversed if current
	vec3_t		mins, maxs;	// for bounding box culling
	struct node_s	*parent;

	// node specific
	cplane_t		*plane;
	struct node_s	*children[2];
	uint		firstSurface;
	uint		numSurfaces;
} node_t;

typedef struct leaf_s
{
	// common with node
	int		contents;		// will be a negative contents number
	int		visFrame;		// node needs to be traversed if current
	vec3_t		mins, maxs;	// for bounding box culling
	struct node_s	*parent;

	// leaf specific
	int		cluster;
	int		area;
	surface_t		**firstMarkSurface;
	int		numMarkSurfaces;
} leaf_t;

typedef struct
{
	vec3_t		point;
} vertex_t;

typedef struct
{
	uint		v[2];
} edge_t;

typedef struct
{

	vec3_t		mins;
	vec3_t		maxs;
	vec3_t		origin;		// for sounds or lights
	float		radius;
	int		visLeafs;		// not including the solid leaf 0
	int		firstFace;
	int		numFaces;
} submodel_t;

typedef struct
{
	vec3_t		xyz;
	vec2_t		st;
	vec3_t		normal;
	vec2_t		sphere;
} skySideVert_t;

typedef struct
{
	int		numIndices;
	int		numVertices;
	uint		indices[SKY_INDICES];
	skySideVert_t	vertices[SKY_VERTICES];
} skySide_t;

typedef struct
{
	ref_shader_t	*shader;
	float		rotate;
	vec3_t		axis;
	float		mins[2][6];
	float		maxs[2][6];
	struct ref_buffer_s	*vbo[6];
	skySide_t		skySides[6];
} sky_t;

typedef struct
{
	vec3_t		lightDir;
} lightGrid_t;

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/
typedef struct mspriteframe_s
{
	int		width;
	int		height;
	float		up, down, left, right;
	float		radius;
	shader_t		shader;
	texture_t		*texture;
} mspriteframe_t;

typedef struct
{
	int		numframes;
	float		*intervals;
	mspriteframe_t	*frames[1];
} mspritegroup_t;

typedef struct
{
	frametype_t	type;
	mspriteframe_t	*frameptr;
} mspriteframedesc_t;

typedef struct
{
	int		type;
	int		rendermode;
	int		numframes;
	mspriteframedesc_t	frames[1];
} msprite_t;

/*
==============================================================================

STUDIO MODELS

==============================================================================
*/
#include "r_model.h"

typedef struct rmodel_s
{
	string		name;
	int		registration_sequence;

	modtype_t		type;
	byte		*mempool;

	// simple lighting for sprites and models
	vec3_t		lightcolor;
	int		flags;

	// volume occupied by the model
	vec3_t		mins;
	vec3_t		maxs;
	float		radius;

	// brush model
	int		numModelSurfaces;
	int		firstModelSurface;

	int		numSubmodels;
	submodel_t	*submodels;

	int		numVertexes;
	vertex_t		*vertexes;

	int		numSurfEdges;
	int		*surfEdges;

	int		numEdges;
	edge_t		*edges;

	int		numSurfaces;
	surface_t		*surfaces;

	int		numMarkSurfaces;
	surface_t		**markSurfaces;

	int		numPlanes;
	cplane_t		*planes;

	int		numNodes;
	int		firstNode;
	node_t		*nodes;

	int		numLeafs;
	leaf_t		*leafs;

	int		numClusters;		// used for create novis lump
	byte		*novis;			// clusterBytes of 0xff

	sky_t		*sky;
	dvis_t		*vis;			// may be passed in by CM_LoadMap to save space
	byte		*lightData;

	int		numTexInfo;
	texInfo_t		*texInfo;

	int		numShaders;
	ref_shader_t	**shaders;

	vec3_t		gridMins;
	vec3_t		gridSize;
	int		gridBounds[4];
	int		numGridPoints;
	lightGrid_t	*lightGrid;

	// studio model
	dstudiohdr_t	*phdr;
          dstudiohdr_t	*thdr;
	mstudiosurface_t	*studiofaces;

	void		*extradata;	// model buffer

} rmodel_t;

typedef struct
{
	byte		open;		// 0 = mouth closed, 255 = mouth agape
	byte		sndcount;		// counter for running average
	int		sndavg;		// running average
} mouth_t;

typedef struct latchedvars_s
{
	float		animtime;		// ???
	float		sequencetime;
	vec3_t		origin;		// edict->v.old_origin
	vec3_t		angles;		

	vec3_t		gaitorigin;
	int		sequence;		// ???
	float		frame;

	float		blending[MAXSTUDIOBLENDS];
	byte		seqblending[MAXSTUDIOBLENDS];
	float		controller[MAXSTUDIOCONTROLLERS];

} latchedvars_t;

// client entity
typedef struct ref_entity_s
{
	edtype_t		ent_type;		// entity type
	int		index;		// viewmodel has entindex -1
	rmodel_t		*model;		// opaque type outside refresh
	rmodel_t		*weaponmodel;	// opaque type outside refresh	

	latchedvars_t	prev;		// previous frame values for lerping
	
	vec3_t		angles;
	vec3_t		origin;		// position
	vec3_t		movedir;		// forward vector that computed on a server
	matrix3x3		matrix;		// rotation vectors

	float		framerate;	// custom framerate
          float		animtime;		// lerping animtime	
	float		frame;		// also used as RF_BEAM's diameter

	int		body;
	int		skin;
	
	float		blending[MAXSTUDIOBLENDS];
	vec3_t		attachment[MAXSTUDIOATTACHMENTS];
	float		controller[MAXSTUDIOCONTROLLERS];
	mouth_t		mouth;		// for synchronizing mouth movements.
	
          int		movetype;		// entity moving type
	int		sequence;
	float		scale;
	
	// misc
	float		backlerp;		// 0.0 = current, 1.0 = old
	vec3_t		rendercolor;	// hl1 rendercolor
	float		renderamt;	// hl1 alphavalues
	int		rendermode;	// hl1 rendermode
	int		renderfx;		// server will be translate hl1 values into flags
	int		colormap;		// q1 and hl1 model colormap (can applied for sprites)
	int		effects;		// q1 effect flags, EF_ROTATE, EF_DIMLIGHT etc

	// these values will be calculated locally, not from entity_state
	vec3_t		realangles;
	int		m_fSequenceLoops;
	int		m_fSequenceFinished;
	int		renderframe;	// using for gait cycle	
	int		gaitsequence;	// client->sequence + yaw
	float		gaitframe;	// client->frame + yaw
	float		gaityaw;		// local value

	// shader information
	ref_shader_t	*shader;
	float		shaderTime;	// subtracted from refdef time to control effect start times
	float		radius;		// bbox approximate radius
	float		rotation;		// what the hell ???
} ref_entity_t;

const char *R_GetStringFromTable( int index );
leaf_t	*R_PointInLeaf( const vec3_t p );
byte	*R_ClusterPVS( int cluster );

void	R_ModelList_f( void );
void	R_StudioInit( void );
void	R_StudioShutdown( void );
bool	R_StudioComputeBBox( vec3_t bbox[8] );	// for drawing bounds
void	R_StudioResetSequenceInfo( ref_entity_t *ent, dstudiohdr_t *hdr );
void	R_StudioSetupModel( int body, int bodypart );
void	R_InitModels( void );
void	R_ShutdownModels( void );
rmodel_t	*Mod_ForName( const char *name, bool crash );
void	Mod_Free( rmodel_t *mod );

#define RENDERPASS_SOLID		1
#define RENDERPASS_ALPHA		2

/*
=======================================================================

DOOM1 STYLE AUTOMAP

=======================================================================
*/
#define MAX_RADAR_ENTS		1024

typedef struct radar_ent_s
{
	vec4_t	color;
	vec3_t	origin;  
	vec3_t	angles;
} radar_ent_t;

extern int numRadarEnts;
extern radar_ent_t RadarEnts[MAX_RADAR_ENTS];

void		GL_InitBackend( void );
bool		GL_Support( int r_ext );
void		GL_InitExtensions( void );
void		GL_ShutdownBackend( void );
void		GL_SetExtension( int r_ext, int enable );
void		GL_SelectTexture( uint tmu );
void		GL_BindTexture( texture_t *texture );
void		GL_TexEnv( GLint texEnv );
void		GL_Enable( GLenum cap );
void		GL_Disable( GLenum cap );
void		GL_CullFace( GLenum mode );
void		GL_PolygonOffset( GLfloat factor, GLfloat units );
void		GL_AlphaFunc( GLenum func, GLclampf ref );
void		GL_BlendFunc( GLenum src, GLenum dst );
void		GL_DepthFunc( GLenum func );
void		GL_DepthMask( GLboolean mask );
void		GL_SetColor( const void *data );
void		GL_LoadMatrix( matrix4x4 source );
void		GL_SaveMatrix( GLenum target, matrix4x4 dest );
void		GL_SetDefaultState( void );
void		GL_BuildGammaTable( void );
void		GL_UpdateGammaRamp( void );
void		GL_Setup3D( void );
void		GL_Setup2D( void );

// simple gl interface
void		GL_Begin( GLuint drawMode );
void		GL_End( void );
void		GL_Vertex2f( GLfloat x, GLfloat y );
void		GL_Vertex3f( GLfloat x, GLfloat y, GLfloat z );
void		GL_Vertex3fv( const GLfloat *v );
void		GL_Normal3f( GLfloat x, GLfloat y, GLfloat z );
void		GL_Normal3fv( const GLfloat *v );
void		GL_Tangent3f( GLfloat x, GLfloat y, GLfloat z );
void		GL_Tangent3fv( const GLfloat *v );
void		GL_Binormal3f( GLfloat x, GLfloat y, GLfloat z );
void		GL_Binormal3fv( const GLfloat *v );
void		GL_TexCoord2f( GLfloat s, GLfloat t );
void		GL_TexCoord4f( GLfloat s, GLfloat t, GLfloat ls, GLfloat lt );
void		GL_TexCoord4fv( const GLfloat *v );
void		GL_Color3f( GLfloat r, GLfloat g, GLfloat b );
void		GL_Color3fv( const GLfloat *v );
void		GL_Color4f( GLfloat r, GLfloat g, GLfloat b, GLfloat a );
void		GL_Color4fv( const GLfloat *v );
void		GL_Color4ub( GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha );
void		GL_Color4ubv( const GLubyte *v );

void		RB_CheckForErrors( const char *filename, const int fileline );
void		R_EndFrame( void );
bool		R_Init_OpenGL( void );
void		R_Free_OpenGL( void );

#define R_CheckForErrors() RB_CheckForErrors( __FILE__, __LINE__ )

/*
 =======================================================================

 BACKEND

 =======================================================================
*/
#define MAX_MESHES			32768

typedef enum
{
	MESH_SKY,
	MESH_SURFACE,
	MESH_STUDIO,
	MESH_SPRITE,
	MESH_BEAM,
	MESH_PARTICLE,
	MESH_POLY
} meshType_t;

typedef struct
{
	uint		sortKey;
	meshType_t	meshType;
	void		*mesh;
} mesh_t;

extern int	m_iInfoKey;
extern float	m_fShaderTime;
extern rmodel_t	*m_pLoadModel;
extern mesh_t	*m_pRenderMesh;
extern rmodel_t	*m_pRenderModel;
extern ref_shader_t	*m_pCurrentShader;
extern ref_entity_t *m_pCurrentEntity;
extern int	m_iStudioModelCount;
extern int	registration_sequence;

void		RB_DebugGraphics( void );
void		RB_CheckMeshOverflow( int numIndices, int numVertices );
void		RB_RenderMesh( void );
void		RB_RenderMeshes( mesh_t *meshes, int numMeshes );
void		RB_DrawStretchPic( float x, float y, float w, float h, float sl, float tl, float sh, float th, ref_shader_t *shader );

void		RB_InitBackend( void );
void		RB_ShutdownBackend( void );

#include "r_backend.h"

typedef struct
{
	int	numShaders;
	int	numStages;
	int	numMeshes;
	int	numLeafs;
	int	numVertices;
	int	numIndices;
	int	totalIndices;

	int	numEntities;
	int	numDLights;
	int	numParticles;
	int	numPolys;
} refstats_t;

extern rmodel_t    	*r_worldModel;
extern ref_entity_t	*r_worldEntity;

extern vec3_t      	r_worldMins, r_worldMaxs;

extern int         	r_frameCount;
extern int         	r_visFrameCount;
extern int         	r_viewCluster;
extern int	r_oldViewCluster;
extern vec3_t	r_origin;				// same as r_refdef.vieworg
extern vec3_t	r_forward;
extern vec3_t	r_right;
extern vec3_t	r_up;

extern matrix4x4	r_worldMatrix;
extern matrix4x4	r_entityMatrix;

extern gl_matrix   	gl_projectionMatrix;
extern gl_matrix   	gl_entityMatrix;
extern gl_matrix	gl_textureMatrix;

extern cplane_t	r_frustum[4];
extern float	r_frameTime;

extern mesh_t	r_solidMeshes[MAX_MESHES];
extern int	r_numSolidMeshes;
extern mesh_t	r_transMeshes[MAX_MESHES];
extern int	r_numTransMeshes;
extern ref_entity_t	r_entities[MAX_ENTITIES];
extern int	r_numEntities;
extern dlight_t	r_dlights[MAX_DLIGHTS];
extern int	r_numDLights;
extern particle_t	r_particles[MAX_PARTICLES];
extern int	r_numParticles;
extern poly_t	r_polys[MAX_POLYS];
extern int	r_numPolys;
extern polyVert_t	r_polyVerts[MAX_POLY_VERTS];
extern int	r_numPolyVerts;
extern ref_entity_t	*r_nullModels[MAX_ENTITIES];
extern int	r_numNullModels;

extern lightstyle_t	r_lightStyles[MAX_LIGHTSTYLES];
extern ref_params_t	r_refdef;
extern refstats_t	r_stats;

void		R_DrawStudioModel( void );
void		R_AddStudioModelToList( ref_entity_t *entity );
void		R_StudioLoadModel( rmodel_t *mod, const void *buffer );

void		R_DrawSpriteModel( void );
void		R_AddSpriteModelToList( ref_entity_t *entity );
void		R_SpriteLoadModel( rmodel_t *mod, const void *buffer );
mspriteframe_t	*R_GetSpriteFrame( ref_entity_t *ent );

void		R_MarkLights( void );
void		R_LightDir( const vec3_t origin, vec3_t lightDir );
void		R_LightForPoint( const vec3_t point, vec3_t ambientLight );
void		R_LightingAmbient( void );
void		R_LightingDiffuse( void );
void		R_BeginBuildingLightmaps( void );
void		R_EndBuildingLightmaps( void );
void		R_BuildSurfaceLightmap( surface_t *surf );
void		R_UpdateSurfaceLightmap( surface_t *surf );
bool		R_CullBox( const vec3_t mins, const vec3_t maxs, int clipFlags );
bool		R_CullSphere( const vec3_t origin, float radius, int clipFlags );
void		R_RotateForEntity( ref_entity_t *entity );
void		R_AddMeshToList( meshType_t meshType, void *mesh, ref_shader_t *shader, ref_entity_t *entity, int infoKey );
bool		R_AddPolyToScene( shader_t shader, int numVerts, const polyVert_t *verts );
void		R_ImpactMark( vec3_t org, vec3_t dir, float rot, float radius, vec4_t rgba, bool fade, shader_t shader, bool temp );
void		R_DrawSprite( void );
void		R_DrawBeam( void );
void		R_DrawParticle( void );
void		R_DrawPoly( void );
void		R_RenderView( const ref_params_t *fd );
void		R_AddShadowToList( ref_entity_t *entity );
void		R_RenderShadows( void );
void		R_BloomBlend ( const ref_params_t *fd );
void		R_DrawSky( void );
void		R_ClearSky( void );
void		R_ClipSkySurface( surface_t *surf );
void		R_AddSkyToList( void );

void		R_DrawSurface( void );
void		R_AddBrushModelToList( ref_entity_t *entity );
void		R_AddWorldToList( void );

void		RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );
void		PerpendicularVector( vec3_t dst, const vec3_t src );

// exported funcs
void		R_BeginRegistration( const char *map );
rmodel_t		*R_RegisterModel( const char *name );
shader_t		Mod_RegisterShader( const char *name, int shaderType );
void		R_SetupSky( const char *name, float rotate, const vec3_t axis );
void		R_EndRegistration( const char *skyname );
void		R_ModRegisterShaders( rmodel_t *mod );	// prolonge registration
bool		VID_ScreenShot( const char *filename, bool levelshot );
bool		VID_CubemapShot( const char *base, uint size, bool skyshot );
void		R_DrawFill( float x, float y, float w, float h );
void		R_DrawSetParms( shader_t handle, kRenderMode_t rendermode, int frame );
void		R_DrawStretchRaw( int x, int y, int w, int h, int width, int height, const byte *raw, bool dirty );
void		R_DrawStretchPic( float x, float y, float w, float h, float sl, float tl, float sh, float th, shader_t shader );
void		R_GetPicSize( int *w, int *h, int frame, shader_t shader );

// r_utils.c (test)
void MatrixGL_MultiplyFast (const gl_matrix m1, const gl_matrix m2, gl_matrix out);	// FIXME: remove

// cvars
extern cvar_t	*r_check_errors;
extern cvar_t	*r_himodels;
extern cvar_t	*r_norefresh;
extern cvar_t	*r_novis;
extern cvar_t	*r_nocull;
extern cvar_t	*r_nobind;
extern cvar_t	*r_drawworld;
extern cvar_t	*r_drawentities;
extern cvar_t	*r_drawparticles;
extern cvar_t	*r_drawpolys;
extern cvar_t	*r_fullbright;
extern cvar_t	*r_lightmap;
extern cvar_t	*r_lockpvs;
extern cvar_t	*r_frontbuffer;
extern cvar_t	*r_showcluster;
extern cvar_t	*r_showtris;
extern cvar_t	*r_shownormals;
extern cvar_t	*r_showtextures;
extern cvar_t	*r_showtangentspace;
extern cvar_t	*r_showmodelbounds;
extern cvar_t	*r_showshadowvolumes;
extern cvar_t	*r_offsetfactor;
extern cvar_t	*r_offsetunits;
extern cvar_t	*r_vertexbuffers;
extern cvar_t	*r_debugsort;
extern cvar_t	*r_speeds;
extern cvar_t	*r_showlightmaps;
extern cvar_t	*r_singleshader;
extern cvar_t	*r_skipbackend;
extern cvar_t	*r_skipfrontend;
extern cvar_t	*r_swapInterval;
extern cvar_t	*r_mode;
extern cvar_t	*r_testmode;
extern cvar_t	*r_fullscreen;
extern cvar_t	*r_caustics;
extern cvar_t	*r_minimap;
extern cvar_t	*r_minimap_size;
extern cvar_t	*r_minimap_zoom;
extern cvar_t	*r_minimap_style;
extern cvar_t	*r_pause;
extern cvar_t	*r_width;
extern cvar_t	*r_height;
extern cvar_t	*r_refreshrate;
extern cvar_t	*r_bitdepth;
extern cvar_t	*r_overbrightbits;
extern cvar_t	*r_shadows;
extern cvar_t	*r_caustics;
extern cvar_t	*r_dynamiclights;
extern cvar_t	*r_modulate;
extern cvar_t	*r_ambientscale;
extern cvar_t	*r_directedscale;
extern cvar_t	*r_intensity;
extern cvar_t	*r_texturebits;
extern cvar_t	*r_texturefilter;
extern cvar_t	*r_texturefilteranisotropy;
extern cvar_t	*r_texturelodbias;
extern cvar_t	*r_max_normal_texsize;
extern cvar_t	*r_max_texsize;
extern cvar_t	*r_round_down;
extern cvar_t	*r_detailtextures;
extern cvar_t	*r_compress_normal_textures;
extern cvar_t	*r_compress_textures;
extern cvar_t	*r_lefthand;
extern cvar_t	*r_bloom;
extern cvar_t	*r_bloom_alpha;
extern cvar_t	*r_bloom_diamond_size;
extern cvar_t	*r_bloom_intensity;
extern cvar_t	*r_bloom_darken;
extern cvar_t	*r_bloom_sample_size;
extern cvar_t	*r_bloom_fast_sample;
extern cvar_t	*r_motionblur_intens;
extern cvar_t	*r_motionblur;
extern cvar_t	*r_mirroralpha;
extern cvar_t	*r_interpolate;
extern cvar_t	*r_physbdebug;
extern cvar_t	*r_pause_bw;
extern cvar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level

extern cvar_t	*gl_finish;
extern cvar_t	*gl_clear;
extern cvar_t	*vid_gamma;

#endif//R_LOCAL_H