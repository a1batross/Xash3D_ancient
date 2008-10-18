//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        r_local.h - render internal types
//=======================================================================

#ifndef R_LOCAL_H
#define R_LOCAL_H

#include <windows.h>
#include "basetypes.h"
#include "ref_dllapi.h"
#include "r_opengl.h"

extern stdlib_api_t com;		// engine toolbox
extern render_imp_t	 ri;
extern byte *r_temppool;
extern byte *r_shaderpool;
#define Host_Error			com.error

// limits
#define MAX_TEXTURE_UNITS		8
#define MAX_LIGHTMAPS		128
#define MAX_ENTITIES		1024
#define MAX_VERTEX_BUFFERS		2048
#define MAX_TEXTURES		4096
#define MAX_POLYS			4096
#define MAX_POLY_VERTS		16384
#define MAX_SURF_QUERIES		0x1E0
#define MAX_SUPER_STYLES		1023
#define MAX_SHADOWGROUPS		32
#define MAX_CLIPFLAGS		15	// all sides of bbox are valid

#define MAX_ARRAY_VERTS		4096
#define MAX_ARRAY_ELEMENTS		MAX_ARRAY_VERTS*6
#define MAX_ARRAY_TRIANGLES		MAX_ARRAY_ELEMENTS/3
#define MAX_ARRAY_NEIGHBORS		MAX_ARRAY_TRIANGLES*3
#define MIN_RENDER_MESHES		2048

#define FOG_TEXTURE_WIDTH		256
#define FOG_TEXTURE_HEIGHT		32
#define BACKFACE_EPSILON		0.01
#define DLIGHT_SCALE	    	0.5f

/*
=======================================================================

 TEXTURE MANAGER

=======================================================================
*/

// image flags
#define TF_CLAMP			(1<<0)	// clamp texture to edge
#define TF_NOMIPMAP			(1<<1)	// don't generate or load mips
#define TF_NOPICMIP			(1<<2)	// no pic mips
#define TF_SKYBOX			(1<<3)	// skybox image
#define TF_CUBEMAP			(1<<4)	// cubemap texture
#define TF_HEIGHTMAP		(1<<5)	// it's a heightmap
#define TF_ALPHAMAP			(1<<6)	// image contain only alpha-channel
#define TF_HAS_ALPHA		(1<<7)	// rgb image has alpha
#define TF_COMPRESS			(1<<8)	// want to compress this image in video memory
#define TF_DEPTH			(1<<9)	// probably depthmap ?
#define TF_NORMALMAP		(1<<10)	// normalmap texture
#define TF_LUMA			(1<<11)	// auto-luma image
#define TF_STATIC			(1<<12)	// never freed by R_ImageFreeUnused()

// custom flags
#define TF_CINEMATIC		( TF_NOPICMIP|TF_NOMIPMAP|TF_CLAMP )
#define TF_PORTALMAP		( TF_NOMIPMAP|TF_NOPICMIP|TF_CLAMP )
#define TF_SHADOWMAP		( TF_NOMIPMAP|TF_NOPICMIP|TF_CLAMP|TF_DEPTH )

typedef struct texture_s
{
	string		name;		// game path, including extension
	int		width;		// source dims, used for mipmap loading
	int		height;
	int		depth;

	int		type;		// PFDesc[type]
	uint		flags;		// texture flags
	float		bumpScale;
	GLint		texnum;		// gl texture binding
	GLuint		target;		// texture target (GL_TEXTURE_2D, GL_TEXTURE_3D etc)
	uint		sequence;		// usage threshold
	struct texture_s	*hash;		// for fast search
} texture_t;

// 3DS studio skybox order
enum
{
	SKYBOX_RIGHT,
	SKYBOX_LEFT,
	SKYBOX_FRONT,
	SKYBOX_BACK,
	SKYBOX_TOP,
	SKYBOX_BOTTOM		// not used for skydome, but is used for skybox
};

extern const char	*r_cubeMapSuffix[6];
extern const char	*r_skyBoxSuffix[6];
extern vec3_t	r_cubeMapAngles[6];
extern vec3_t	r_skyBoxAngles[6];

extern texture_t	*r_defaultTexture;
extern texture_t	*r_whiteTexture;
extern texture_t	*r_blackTexture;
extern texture_t	*r_rawTexture;
extern texture_t	*r_dlightTexture;
extern texture_t	*r_normalizeTexture;
extern texture_t	*r_radarMap;
extern texture_t	*r_aroundMap;
extern texture_t	*r_portaltexture;
extern texture_t	*r_portaltexture2;
extern texture_t	*r_lightmapTextures[];
extern texture_t	*r_shadowmapTextures[];
extern byte	*r_framebuffer;

void		R_TextureFilter( void );
void		R_TextureList_f( void );
texture_t		*R_LoadTexture( const char *name, rgbdata_t *pic, uint flags, float bumpScale );
texture_t		*R_FindTexture( const char *name, const byte *buffer, size_t size, uint flags, float bumpScale );
texture_t		*R_CreateTexture( const char *name, byte *buf, int width, int height, uint flags, uint tflags );
texture_t		*R_FindCubeMapTexture( const char *name, uint flags, float bumpScale );
void		R_InitPortalTexture( texture_t **texture, int id, int width, int height );
void		R_InitTextures( void );
void		R_ShutdownTextures( void );
void		R_ImageFreeUnused( void );

/*
=======================================================================

	RENDER PARMS	(changed for each passage)

=======================================================================
*/
#define RP_NONE			0
#define RP_MIRRORVIEW		(1<<0)	// lock pvs at vieworg
#define RP_PORTALVIEW		(1<<1)
#define RP_ENVVIEW			(1<<2)	// render only bmodels (for envshots)
#define RP_NOSKY			(1<<3)
#define RP_SKYPORTALVIEW		(1<<4)	// view from sky pass
#define RP_REFLECTED		(1<<5)	// base portal image
#define RP_REFRACTED		(1<<6)	// refracted portal image ?
#define RP_OLDVIEWCLUSTER		(1<<7)
#define RP_SHADOWMAPVIEW		(1<<8)	// shadowmap pass
#define RP_FLIPFRONTFACE		(1<<9)
#define RP_WORLDSURFVISIBLE		(1<<10)
#define RP_CLIPPLANE		(1<<11)	// mirror clip plane
#define RP_TRISOUTLINES		(1<<12)
#define RP_SHOWNORMALS		(1<<13)
#define RP_SHOWTANGENTS		(1<<14)

// base mask for other passes
#define RP_NONVIEWERREF		( RP_PORTALVIEW|RP_MIRRORVIEW|RP_ENVVIEW|RP_SKYPORTALVIEW|RP_SHADOWMAPVIEW )

#define R_FOGNUM( fog )		((fog) ? ((((int)((fog) - r_worldBrushModel->fogs))+1) << 2) : 0 )
#define R_FOG_FOR_KEY( num, fog )	((fog) = r_worldBrushModel->fogs+(((num)>>2) & 0xFF),\
				(fog) = ((fog) == r_worldBrushModel->fogs ? NULL : (fog)  -1 ))

#define R_EDICTNUM( ent )		((int)((ent) - r_entities)<<20 )
#define R_EDICT_FOR_KEY( num, ent )	( ent = r_entities + (((num)>>20) & (MAX_ENTITIES-1)))

// GLSTATE machine flags
#define GLSTATE_DEFAULT			0	// reset state to default
#define GLSTATE_SRCBLEND_ZERO			(1<<0)
#define GLSTATE_SRCBLEND_ONE			(1<<1)
#define GLSTATE_SRCBLEND_ONE_MINUS_DST_COLOR	(1<<2)
#define GLSTATE_SRCBLEND_ONE_MINUS_DST_ALPHA	(1<<3)
#define GLSTATE_DSTBLEND_ZERO			(1<<4)
#define GLSTATE_DSTBLEND_ONE			(1<<5)
#define GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR	(1<<6)
#define GLSTATE_DSTBLEND_ONE_MINUS_DST_ALPHA	(1<<7)
#define GLSTATE_AFUNC_GT0			(1<<8)
#define GLSTATE_AFUNC_LT128			(1<<9)
#define GLSTATE_AFUNC_GE128			(1<<10)
#define GLSTATE_DEPTHWRITE			(1<<11)
#define GLSTATE_DEPTHFUNC_EQ			(1<<12)
#define GLSTATE_OFFSET_FILL			(1<<13)
#define GLSTATE_NO_DEPTH_TEST			(1<<14)
#define GLSTATE_BLEND_MTEX			(1<<15)

// GLSTATE masks
#define GLSTATE_MASK			((1<<16) - 1 )
#define GLSTATE_SRCBLEND_DST_COLOR		(GLSTATE_SRCBLEND_ZERO|GLSTATE_SRCBLEND_ONE)
#define GLSTATE_SRCBLEND_SRC_ALPHA		(GLSTATE_SRCBLEND_ZERO|GLSTATE_SRCBLEND_ONE_MINUS_DST_COLOR)
#define GLSTATE_SRCBLEND_ONE_MINUS_SRC_ALPHA	(GLSTATE_SRCBLEND_ONE|GLSTATE_SRCBLEND_ONE_MINUS_DST_COLOR)
#define GLSTATE_SRCBLEND_DST_ALPHA		(GLSTATE_SRCBLEND_ZERO|GLSTATE_SRCBLEND_ONE|GLSTATE_SRCBLEND_ONE_MINUS_DST_COLOR)
#define GLSTATE_DSTBLEND_SRC_COLOR		(GLSTATE_DSTBLEND_ZERO|GLSTATE_DSTBLEND_ONE)
#define GLSTATE_DSTBLEND_SRC_ALPHA		(GLSTATE_DSTBLEND_ZERO|GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR)
#define GLSTATE_DSTBLEND_ONE_MINUS_SRC_ALPHA	(GLSTATE_DSTBLEND_ONE|GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR)
#define GLSTATE_DSTBLEND_DST_ALPHA		(GLSTATE_DSTBLEND_ZERO|GLSTATE_DSTBLEND_ONE|GLSTATE_DSTBLEND_ONE_MINUS_SRC_COLOR)
#define GLSTATE_ALPHAFUNC			(GLSTATE_AFUNC_GT0|GLSTATE_AFUNC_LT128|GLSTATE_AFUNC_GE128)
#define GLSTATE_SRCBLEND_MASK			(((GLSTATE_SRCBLEND_DST_ALPHA)<<1)-GLSTATE_SRCBLEND_ZERO)
#define GLSTATE_DSTBLEND_MASK			(((GLSTATE_DSTBLEND_DST_ALPHA)<<1)-GLSTATE_DSTBLEND_ZERO)

enum
{
	OQ_NONE = -1,
	OQ_ENTITY,
	OQ_PLANARSHADOW,
	OQ_SHADOWGROUP,
	OQ_CUSTOM
};

enum
{
	SHADOW_PLANAR = 1,
	SHADOW_MAPPING,
};

#define OCCLUSION_QUERIES_CVAR_HACK( ri ) (!(r_occlusion_queries->integer == 2 && r_shadows->integer != SHADOW_MAPPING) \
				|| ((ri).refdef.rdflags & RDF_PORTALINVIEW))
#define OCCLUSION_QUERIES_ENABLED( ri )	(GL_Support( R_OCCLUSION_QUERY ) && r_occlusion_queries->integer \
				&& r_drawentities->integer && !((ri).params & RP_NONVIEWERREF) \
				&& !((Ref).refdef.rdflags & RDF_NOWORLDMODEL) \
				&& OCCLUSION_QUERIES_CVAR_HACK( Ref ))
#define OCCLUSION_OPAQUE_SHADER( s )	(((s)->sort == SORT_OPAQUE ) && ((s)->stages[0]->flags & SHADERSTAGE_DEPTHWRITE ) \
				&& !(s)->deformVertsNum )
#define OCCLUSION_TEST_ENTITY( e )	(((e)->flags & (RF_OCCLUSIONTEST|RF_WEAPONMODEL)) == RF_OCCLUSIONTEST )

/*
=======================================================================

 PROGRAM MANAGER

=======================================================================
*/
#define DEFAULT_PROGRAM		"*r_defaultProgram"
#define DEFAULT_DISTORTION_PROGRAM	"*r_defaultDistortionProgram"
#define DEFAULT_SHADOWMAP_PROGRAM	"*r_defaultShadowmapProgram"

typedef enum
{
	PROGRAM_NONE,
	PROGRAM_MATERIAL,		// apply bump, normalmap, glossmap and other...
	PROGRAM_DISTORTION,
	PROGRAM_SHADOWMAP,
} progType_t;

typedef struct
{
	int		features;		// ???
	int		lmapNum[LM_STYLES];
	int		lStyles[LM_STYLES];
	int		vStyles[LM_STYLES];
	float		stOffset[LM_STYLES][2];
} superLightStyle_t;

enum
{
	PROGRAM_APPLY_LIGHTSTYLE0		= 1<<0,
	PROGRAM_APPLY_LIGHTSTYLE1		= 1<<1,
	PROGRAM_APPLY_LIGHTSTYLE2		= 1<<2,
	PROGRAM_APPLY_LIGHTSTYLE3		= 1<<3,
	PROGRAM_APPLY_SPECULAR		= 1<<4,
	PROGRAM_APPLY_DIRECTIONAL_LIGHT	= 1<<5,
	PROGRAM_APPLY_FB_LIGHTMAP		= 1<<6,
	PROGRAM_APPLY_OFFSETMAPPING		= 1<<7,
	PROGRAM_APPLY_RELIEFMAPPING		= 1<<8,
	PROGRAM_APPLY_AMBIENT_COMPENSATION	= 1<<9,
	PROGRAM_APPLY_DECAL			= 1<<10,
	PROGRAM_APPLY_BASETEX_ALPHA_ONLY	= 1<<11,
	PROGRAM_APPLY_EYEDOT		= 1<<12,
	PROGRAM_APPLY_DISTORTION_ALPHA	= 1<<13,
	PROGRAM_APPLY_PCF2x2		= 1<<14,
	PROGRAM_APPLY_PCF3x3		= 1<<15,
	PROGRAM_APPLY_BRANCHING		= 1<<16,
	PROGRAM_APPLY_CLIPPING		= 1<<17,
	PROGRAM_APPLY_NO_HALF_TYPES		= 1<<18
};

void		R_InitPrograms( void );
int		R_FindProgram( const char *name );
int		R_RegisterProgram( const char *name, const char *string, uint features );
int		R_GetProgramObject( int elem );
void		R_UpdateProgramUniforms( int elem, vec3_t eyeOrigin, vec3_t lightOrigin, vec3_t lightDir,
		vec4_t ambient, vec4_t diffuse, superLightStyle_t *superLightStyle, bool frontPlane,
		int TexWidth, int TexHeight, float projDistance, float offsetmappingScale );

void		R_ShutdownPrograms( void );
void		R_ProgramList_f( void );
void		R_ProgramDump_f( void );

#include "r_shader.h"

/*
=======================================================================

BRUSH MODELS

=======================================================================
*/
#define SURF_PLANEBACK		1 // fast surface culling
#define CONTENTS_NODE		-1

#define SURF_WATERCAUSTICS		2
#define SURF_SLIMECAUSTICS		4
#define SURF_LAVACAUSTICS		8

typedef struct dlight_s
{
	vec3_t		origin;
	vec3_t		color;
	vec3_t		mins;
	vec3_t		maxs;
	float		intensity;
	const ref_shader_t	*shader;		// for some effects
} dlight_t;

typedef struct lightstyle_s
{
	float		rgb[3];		// 0.0 - 2.0
	float		white;		// highest of rgb
} lightstyle_t;

typedef struct particle_s
{
	ref_shader_t		*shader;
	vec3_t		origin;
	vec3_t		old_origin;
	float		radius;
	float		length;
	float		rotation;
	vec4_t		modulate;
} particle_t;

typedef struct mesh_s
{
	int		numVerts;
	vec4_t		*points;
	vec4_t		*normal;
	vec4_t		*sVectors;
	vec2_t		*st;
	vec2_t		*lm[LM_STYLES];
	vec4_t		*color[LM_STYLES];
	int		numIndexes;
	uint		*indexes;
} rb_mesh_t;

typedef struct meshbuffer_s
{
	int		shaderKey;
	uint		sortKey;
	int		infoKey;			// surface number or mesh number
	union
	{
		int	lastPoly;
		uint	dlightbits;
	};
	uint		shadowbits;
} meshbuffer_t;

typedef struct
{
	int		num_opaque_meshes;
	int		max_opaque_meshes;
	meshbuffer_t	*meshbuffer_opaque;

	int		num_translucent_meshes;
	int		max_translucent_meshes;
	meshbuffer_t	*meshbuffer_translucent;

	int	  	num_portal_opaque_meshes;
	int		num_portal_translucent_meshes;
} meshlist_t;

typedef struct rshadow_s
{
	uint		bit;
	texture_t		*depthTexture;

	vec3_t		origin;
	byte		*vis;

	float		projDist;
	vec3_t		mins, maxs;

	matrix4x4		worldProjectionMatrix;
	struct rshadow_s	*hashNext;
} shadowGroup_t;

typedef struct
{
	rb_mesh_t		*meshes;
	vec2_t		*sphereStCoords[5];		// sky dome coords
	vec2_t		*linearStCoords[6];
	ref_shader_t		*farboxShaders[6];
	ref_shader_t		*nearboxShaders[6];
} skydome_t;

typedef struct
{
	int		firstVert;
	int		numVerts;		// can't exceed MAX_POLY_VERTS
	int		fogNum;		// -1 - do not bother adding fog later at rendering stage
					//  0 - determine fog later
					// >0 - valid fog volume number returned by R_GetClippedFragments
	vec3_t		normal;
} fragment_t;

typedef struct
{
	int		numverts;
	vec3_t		*verts;
	vec2_t		*st;
	vec4_t		*colors;
	ref_shader_t		*shader;
	int		fognum;
	vec3_t		normal;
} poly_t;


/*
==============================================================================

BRUSH MODELS

==============================================================================
*/
typedef struct
{
	ref_shader_t		*shader;
	cplane_t		*visible;

	int		numplanes;
	cplane_t		*planes;
} mfog_t;

typedef struct msurface_s
{
	int		visFrame;	
	dmst_t		faceType;
	int		flags;

	ref_shader_t		*shader;
	rb_mesh_t		*mesh;
	mfog_t		*fog;
	cplane_t		*plane;

	union
	{
		vec3_t	origin;
		vec3_t	mins;
	};
	union
	{
		vec3_t	color;
		vec3_t	maxs;
	};

	int		superLightStyle;
	int		fragmentframe;		// for multi-check avoidance
} msurface_t;

typedef struct mnode_s
{
	// common with leaf
	cplane_t		*plane;
	int		visFrame;		// Node needs to be traversed if current
	vec3_t		mins, maxs;	// for bounding box culling
	struct mnode_s	*parent;

	// node specific
	struct mnode_s	*children[2];
} mnode_t;

typedef struct mleaf_s
{
	// common with node
	cplane_t		*plane;
	int		visFrame;
	vec3_t		mins, maxs;	// for bounding box culling
	mnode_t		*parent;

	// leaf specific
	int		visframe;
	int		cluster, area;

	msurface_t	**firstVisSurface;
	msurface_t	**firstFragmentSurface;
} mleaf_t;

typedef struct msubmodel_s
{
	vec3_t		mins;
	vec3_t		maxs;
	float		radius;
	int		firstFace;
	int		numFaces;
} msubmodel_t;

typedef struct mlightgrid_s
{
	byte		ambient[LM_STYLES][3];
	byte		diffuse[LM_STYLES][3];
	byte		styles[LM_STYLES];
	byte		direction[2];
} mlightgrid_t;

typedef struct lmrect_s
{
	int		texNum;
	float		texMatrix[2][2];
} lmrect_t;

typedef struct mbrushmodel_s
{
	dvis_t		*vis;			// may be passed in by CM_LoadMap to save space	

	int		numSubmodels;
	msubmodel_t	*submodels;

	// brush model
	int		numModelSurfaces;
	msurface_t	*firstModelSurface;

	int		numPlanes;
	cplane_t		*planes;

	int		numleafs;			// number of visible leafs, not counting 0
	mleaf_t		*leafs;
	mleaf_t		**visleafs;

	int		numnodes;
	mnode_t		*nodes;

	int		numsurfaces;
	msurface_t	*surfaces;

	int		numGridPoints;
	mlightgrid_t	*lightgrid;

	int		numLightArrayPoints;
	mlightgrid_t	**lightarray;

	int		numFogs;
	mfog_t		*fogs;
	mfog_t		*globalfog;

	vec3_t		gridSize;
	vec3_t		gridMins;
	int		gridBounds[4];
} mbrushmodel_t;

/*
==============================================================================

STUDIO MODELS

==============================================================================
*/
typedef struct
{
	uint		index[3];
} mstudiotriangle_t;

typedef struct
{
	int		index[3];
} mstudioneighbor_t;

typedef struct
{
	short		point[3];
	byte		tangent[2];
	byte		binormal[2];
	byte		normal[2];
} mstudiopoint_t;

typedef struct
{
	vec2_t		st;
} mstudiost_t;

typedef struct
{
	mstudiotriangle_t	*triangles;
	mstudioneighbor_t	*neighbors;
	mstudiopoint_t	*points;
	mstudiost_t	*st;
	ref_shader_t		*shaders;

	int		numTriangles;
	int		numVertices;
	int		numShaders;
} mstudiosurface_t;

typedef struct mstudiomodel_s
{
	// studio model
          dstudiohdr_t	*phdr;
	dstudiohdr_t	*thdr;
} mstudiomodel_t;

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
	ref_shader_t		*shader;
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

typedef struct
{

	vec3_t		point;
	vec3_t		normal;
	float		st[2];
	float		lm[2];
	vec4_t		color;	
} vertex_t;


typedef struct rmodel_s
{
	string		name;
	int		sequence;			// registration sequence
	modtype_t		type;
	byte		*mempool;

	// volume occupied by the model
	vec3_t		mins, maxs;
	float		radius;

	int		numShaders;
	ref_shader_t		*shaders[MAX_SHADERS];

	// memory representation pointer
	void		*extradata;		// mbrushmodel_t\mstudiomodel_t\mspritemodel_t
	void		*sequences;		// studiomodel sequences

	// FIXME: remove
          dstudiohdr_t	*phdr;
	dstudiohdr_t	*thdr;
} rmodel_t;

typedef struct
{
	byte		open;		// 0 = mouth closed, 255 = mouth agape
	byte		sndcount;		// counter for running average
	int		sndavg;		// running average
} mouth_t;

typedef struct latchedvars_s
{
	float		animtime;
	float		sequencetime;
	vec3_t		origin;
	vec3_t		angles;		

	vec3_t		gaitorigin;
	int		sequence;
	float		frame;

	float		blending[MAXSTUDIOBLENDS];
	byte		seqblending[MAXSTUDIOBLENDS];
	float		controller[MAXSTUDIOCONTROLLERS];

} latchedvars_t;

// client entity
typedef struct ref_entity_s
{
	edtype_t		ent_type;		// entity type
	int		index;		// entity index
	rmodel_t		*model;		// opaque type outside refresh
	rmodel_t		*weaponmodel;	// opaque type outside refresh	
	matrix3x3		matrix;		// entity transformation matrix

	latchedvars_t	prev;		// previous frame values for lerping
	
	vec3_t		angles;
	vec3_t		origin;		// position
	vec3_t		lightOrg;		// R_LightForPoint origin
	vec3_t		infoangles;	// portal angles
	vec3_t		infotarget;	// portal origin

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
	ref_shader_t		*shader;
	float		shaderTime;	// subtracted from refdef time to control effect start times
	float		radius;		// bbox approximate radius
	float		rotation;		// what the hell ???
} ref_entity_t;

const char *R_GetStringFromTable( int index );
mleaf_t	*R_PointInLeaf( const vec3_t p );
byte	*R_ClusterPVS( int cluster );

void	R_ModelList_f( void );
void	R_StudioInit( void );
void	R_StudioShutdown( void );
bool	R_StudioComputeBBox( vec3_t bbox[8] );	// for drawing bounds
void	R_StudioSetupModel( int body, int bodypart );
void	R_InitModels( void );
void	R_ShutdownModels( void );
bool	Mod_RegisterShader( const char *unused, int index );
rmodel_t	*Mod_ForName( const char *name, bool crash );
void	Mod_Free( rmodel_t *mod );

#define RENDERPASS_SOLID		1
#define RENDERPASS_ALPHA		2

// renderer current state
typedef struct ref_state_s
{
	int		params;		// rendering parameters

	refdef_t		refdef;		// current refdef state
	int		scissor[4];	// scissor recatngle
	int		viewport[4];	// view port rectangle
	meshlist_t	*meshlist;	// meshes to be rendered
	meshbuffer_t	**surfmbuffers;	// pointers to meshbuffers of world surfaces

	uint		shadowBits;
	shadowGroup_t	*shadowGroup;

	ref_entity_t	*m_pCurrentEntity;
	rmodel_t		*m_pCurrentModel;
	ref_entity_t	*m_pPrevEntity;
	ref_shader_t		*m_pCurrentShader;

	//
	// view origin
	//
	vec3_t		vieworg;
	vec3_t		forward;
	vec3_t		right;
	vec3_t		up;
	cplane_t		frustum[6];
	float		farClip;
	uint		clipFlags;
	vec3_t		visMins;
	vec3_t		visMaxs;

	matrix4x4		entityMatrix;
	matrix4x4		worldMatrix;
	matrix4x4		modelViewMatrix;		// worldviewMatrix * entityMatrix

	matrix4x4		projectionMatrix;
	matrix4x4		worldProjectionMatrix;	// worldviewMatrix * projectionMatrix

	float		skyMins[2][6];
	float		skyMaxs[2][6];

	float		fog_dist_to_eye[MAX_MAP_FOGS];

	vec3_t		pvsOrigin;
	cplane_t		clipPlane;
	cplane_t		portalPlane;
} ref_state_t;

typedef struct
{
	int		pow2MapOvrbr;

	vec3_t		ambient;
	bool		lightmapsPacking;
	bool		deluxeMaps;            // true if there are valid deluxemaps in the .bsp
	bool		deluxeMappingEnabled;  // true if deluxeMaps is true and r_lighting_deluxemaps->integer != 0
} mapconfig_t;

/*
=======================================================================

DOOM1 STYLE AUTOMAP

FIXME: move to ref_state_t
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

/*
 =======================================================================

 GL STATE MANAGER

 =======================================================================
*/

typedef enum
{
	R_OPENGL_110 = 0,		// base
	R_SGIS_MIPMAPS_EXT,
	R_WGL_SWAPCONTROL,
	R_COMBINE_EXT,
	R_DRAW_RANGEELEMENTS_EXT,
	R_ARB_MULTITEXTURE,
	R_LOCKARRAYS_EXT,
	R_TEXTURE_3D_EXT,
	R_TEXTURECUBEMAP_EXT,
	R_DOT3_ARB_EXT,
	R_CLAMPTOEDGE_EXT,
	R_ANISOTROPY_EXT,
	R_BLEND_MINMAX_EXT,
	R_STENCILTWOSIDE_EXT,
	R_BLEND_SUBTRACT_EXT,
	R_SHADER_OBJECTS_EXT,
	R_SHADER_GLSL100_EXT,
	R_VERTEX_SHADER_EXT,	// glsl vertex program
	R_FRAGMENT_SHADER_EXT,	// glsl fragment program	
	R_EXT_POINTPARAMETERS,
	R_SEPARATESTENCIL_EXT,
	R_ARB_TEXTURE_NPOT_EXT,
	R_ARB_VERTEX_BUFFER_OBJECT_EXT,
	R_CUSTOM_VERTEX_ARRAY_EXT,
	R_TEXTURE_COMPRESSION_EXT,
	R_TEXTURE_ENV_ADD_EXT,
	R_OCCLUSION_QUERY,
	R_EXTCOUNT
} r_opengl_extensions;

typedef struct glstate_s
{
	bool		orthogonal;
	word		gammaRamp[768];		// current gamma ramp
	word		stateRamp[768];		// original gamma ramp
	uint		screenTexture;

	vec4_t		draw_color;		// current color
	uint		activeTMU;
	int		texNum[MAX_TEXTURE_UNITS];
	int		texEnv[MAX_TEXTURE_UNITS];
	int		texGen[MAX_TEXTURE_UNITS];
	int		texMod[MAX_TEXTURE_UNITS];	// 0 - disabled, 1 - enabled, 2 - cubemap
	bool		texMat[MAX_TEXTURE_UNITS];	// 0 - identity, 1 - custom matrix

	// render current state
	uint		flags;
	bool		cullFace;
	bool		polygonOffsetFill;
	bool		alpha_test;
	bool		depth_test;
	bool		blend;

	// OpenGL current state
	GLenum		cullMode;
	GLfloat		offsetFactor;
	GLfloat		offsetUnits;
	GLenum		alphaFunc;
	GLclampf		alphaRef;
	GLenum		blendSrc;
	GLenum		blendDst;
	GLenum		depthFunc;
	GLboolean		depthMask;
	GLfloat		polygonoffset[2];
	GLboolean		frontFace;
} glstate_t;

// contains constant values that are always
// setting by R_Init and using as read-only
typedef struct glconfig_s
{
	const char	*renderer_string;
	const char	*vendor_string;
	const char	*version_string;

	// list of supported extensions
	const char	*extensions_string;
	byte		extension[R_EXTCOUNT];
	uint		max_entities;

	int		textureunits;
	GLint		max_2d_texture_size;
	GLint		max_2d_rectangle_size;
	GLint		max_3d_texture_size;
	GLint		max_cubemap_texture_size;
	GLint		max_anisotropy;
	GLint		texRectangle;

	bool		deviceSupportsGamma;
	bool		fullscreen;
	int		prev_mode;
} glconfig_t;

extern glconfig_t	gl_config;
extern glstate_t	gl_state;
extern mapconfig_t	mapConfig;
extern ref_state_t	Ref, oldRef;

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
void		GL_EnableTexGen( GLint coord, GLint mode );
void		GL_PolygonOffset( GLfloat factor, GLfloat units );
void		GL_AlphaFunc( GLenum func, GLclampf ref );
void		GL_BlendFunc( GLenum src, GLenum dst );
void		GL_DepthFunc( GLenum func );
void		GL_DepthMask( GLboolean mask );
void		GL_SetColor( const void *data );
void		GL_TexCoordMode( GLenum mode );
void		GL_LoadMatrix( const matrix4x4 source );
void		GL_SaveMatrix( GLenum target, matrix4x4 dest );
void		GL_LoadTexMatrix( const matrix4x4 source );
void		GL_LoadIdentityTexMatrix( void );
void		GL_FrontFace( GLboolean front );
void		GL_SetDefaultState( void );
void		GL_SetState( GLint state );
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
void		GL_TexCoord2f( GLfloat s, GLfloat t );
void		GL_TexCoord4f( GLfloat s, GLfloat t, GLfloat ls, GLfloat lt );
void		GL_TexCoord4fv( const GLfloat *v );
void		GL_Color3f( GLfloat r, GLfloat g, GLfloat b );
void		GL_Color3fv( const GLfloat *v );
void		GL_Color4f( GLfloat r, GLfloat g, GLfloat b, GLfloat a );
void		GL_Color4fv( const GLfloat *v );
void		GL_Color4ub( GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha );
void		GL_Color4ubv( const GLubyte *v );

void		R_CheckForErrors( void );
void		R_EndFrame( void );
bool		R_Init_OpenGL( void );
void		R_Free_OpenGL( void );
void		R_CheckForErrors( void );

/*
 =======================================================================

 BACKEND

 =======================================================================
*/

#define VBO_OFFSET(i)		((char *)NULL + (i))
#define MAX_INDICES			16384 * 3
#define MAX_VERTICES		8192
#define MAX_MESHES			32768

typedef struct
{
	uint		indexBuffer;
	uint		vertexBuffer;
	uint		normalBuffer;
	uint		colorBuffer;
	uint		texCoordBuffer[MAX_TEXTURE_UNITS];
} vbo_t;

typedef enum
{
	MESH_MODEL,
	MESH_SPRITE,
	MESH_BEAM,
	MESH_PARTICLE,
	MESH_CORONA,
	MESH_POLY
} meshType_t;

typedef enum
{
	MF_NONE		= 0,
	MF_NORMALS	= 1<<0,
	MF_STCOORDS	= 1<<1,
	MF_LMCOORDS	= 1<<2,
	MF_LMCOORDS1	= 1<<3,
	MF_LMCOORDS2	= 1<<4,
	MF_LMCOORDS3	= 1<<5,
	MF_COLORS		= 1<<6,
	MF_COLORS1	= 1<<7,
	MF_COLORS2	= 1<<8,
	MF_COLORS3	= 1<<9,
	MF_ENABLENORMALS    = 1<<10,
	MF_DEFORMVS	= 1<<11,
	MF_SVECTORS	= 1<<12,

	// global features
	MF_NOCULL		= 1<<16,
	MF_TRIFAN		= 1<<17,
	MF_NONBATCHED	= 1<<18,
	MF_KEEPLOCK	= 1<<19
} meshFlags_t;

extern vbo_t	rb_vbo;
extern int	m_iInfoKey;
extern float	m_fShaderTime;
extern int	registration_sequence;

#define MAX_ARRAY_VERTS		4096
#define MAX_ARRAY_ELEMENTS		MAX_ARRAY_VERTS*6
#define MAX_ARRAY_TRIANGLES		MAX_ARRAY_ELEMENTS/3
#define MAX_ARRAY_NEIGHBORS		MAX_ARRAY_TRIANGLES*3

extern ALIGN	vec4_t inVertsArray[MAX_ARRAY_VERTS];
extern ALIGN	vec4_t inNormalArray[MAX_ARRAY_VERTS];
extern vec4_t	inSVectorsArray[MAX_ARRAY_VERTS];
extern uint	inElemsArray[MAX_ARRAY_ELEMENTS];
extern vec2_t	inTexCoordArray[MAX_ARRAY_VERTS];
extern vec2_t	inLMCoordsArray[LM_STYLES][MAX_ARRAY_VERTS];
extern vec4_t	inColorArray[LM_STYLES][MAX_ARRAY_VERTS];
extern vec4_t	colorArray[MAX_ARRAY_VERTS];

extern uint	*indexArray;
extern vec4_t	*vertexArray;
extern vec4_t	*normalArray;
extern vec4_t	*sVectorArray;
extern vec2_t	*texCoordArray;
extern vec2_t	*lmCoordArray[LM_STYLES];

void		RB_ShowTextures( void );
void		RB_DebugGraphics( void );
void		RB_CheckMeshOverflow( int numIndices, int numVertices );
void		RB_RenderMesh( void );
void		RB_DrawStretchPic( float x, float y, float w, float h, float sl, float tl, float sh, float th, ref_shader_t *shader );
void		RB_DrawTriangleOutlines( bool showTris, bool showNormals );
void		RB_BeginTriangleOutlines( void );
void		RB_EndTriangleOutlines( void );
void		R_LatLongToNorm( const byte latlong[2], vec3_t out );

void		RB_VBOInfo_f( void );
uint		RB_AllocStaticBuffer( uint target, int size );
uint		RB_AllocStreamBuffer( uint target, int size );
void		RB_InitBackend( void );
void		RB_ShutdownBackend( void );
void		RB_ResetCounters( void );
void		RB_SetPassMask( int mask );
void		RB_ResetPassMask( void );
void		RB_StartFrame( void );
void		RB_EndFrame( void );

typedef struct
{
	// backend counters
	int	numShaders;
	int	numStages;
	int	numMeshes;
	int	numLeafs;
	int	numVertices;
	int	numIndices;
	int	numColors;

	// stat counters
	int	totalTris;
	int	totalVerts;
	int	totalIndices;
	int	totalFlushes;
	int	totalKeptLocks;
	int	brushPolys;
	int	worldLeafs;

	int	numEntities;
	int	numDLights;
	int	numParticles;
	int	numPolys;
} refstats_t;

extern rmodel_t    	*r_worldModel;
extern ref_entity_t	*r_worldEntity;
extern mbrushmodel_t *r_worldBrushModel;

extern vec3_t      	r_worldMins, r_worldMaxs;
extern float	gldepthmin, gldepthmax;

extern int         	r_frameCount;
extern int         	r_visFrameCount;
extern int         	r_viewCluster;
extern int	r_oldViewCluster;
extern refdef_t	r_lastRefdef;
extern float	r_farclip_min, r_farclip_bias;
extern int	r_features;

extern skydome_t	*r_skydomes[MAX_SHADERS];
extern int	r_entShadowBits[MAX_ENTITIES];
extern ref_entity_t	r_entities[MAX_ENTITIES];
extern int	r_numEntities;
extern dlight_t	r_dlights[MAX_DLIGHTS];
extern int	r_numDLights;
extern particle_t	r_particles[MAX_PARTICLES];
extern int	r_numParticles;
extern poly_t	r_polys[MAX_POLYS];
extern int	r_numPolys;

extern meshlist_t	r_worldlist, r_shadowlist;
extern lightstyle_t	r_lightStyles[MAX_LIGHTSTYLES];
extern int	r_numSuperLightStyles;
extern superLightStyle_t r_superLightStyles[MAX_SUPER_STYLES];
extern refdef_t	r_refdef;
extern refstats_t	r_stats;

// r_main.c
void		R_TransformEntityBBox( ref_entity_t *e, vec3_t mins, vec3_t maxs, vec3_t bbox[8], bool local );
void		R_LoadIdentity( void );

void		R_DrawStudioModel( const meshbuffer_t *mb );
void		R_AddStudioModelToList( ref_entity_t *entity );
void		R_StudioLoadModel( rmodel_t *mod, const void *buffer );

void		R_DrawSpriteModel( void );
void		R_AddSpriteModelToList( ref_entity_t *entity );
void		R_SpriteLoadModel( rmodel_t *mod, const void *buffer );
mspriteframe_t	*R_GetSpriteFrame( ref_entity_t *ent );

// r_light.c
bool		R_SurfPotentiallyLit( msurface_t *surf );
void		R_LightBounds( const vec3_t origin, float intensity, vec3_t mins, vec3_t maxs );
uint		R_AddSurfDlighbits( msurface_t *surf, uint dlightbits );
void		R_AddDynamicLights( uint dlightbits, int state );
void		R_InitCoronas( void );
void		R_DrawCoronas( void );
void		R_LightForPoint( const vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius );
void		R_LightForEntity( ref_entity_t *e, float *bArray );
void		R_BuildLightmaps( int numLightmaps, int w, int h, const byte *data, lmrect_t *rects );
int		R_AddSuperLightStyle( const int *lightmaps, const byte *lStyles, const byte *vStyles, lmrect_t **lmRects );
void		R_SortSuperLightStyles( void );

void		R_SetupModelViewMatrix( const refdef_t *rd, matrix4x4 m );
void		R_SetupProjectionMatrix( const refdef_t *rd, matrix4x4 m );

bool		R_SurfPotentiallyFragmented( msurface_t *surf );
bool		R_SurfPotentiallyVisible( msurface_t *surf );
bool		R_CullBox( const vec3_t mins, const vec3_t maxs, const uint clipFlags );
bool		R_CullSphere( const vec3_t origin, const float radius, const uint clipFlags );
bool		R_VisCullBox( const vec3_t mins, const vec3_t maxs );
bool		R_VisCullSphere( const vec3_t origin, float radius );
int		R_CullModel( ref_entity_t *e, vec3_t mins, vec3_t maxs, float radius );
bool		R_CullBrushModel( ref_entity_t *e );
bool		R_CullStudioModel( ref_entity_t *e );
void		R_DrawSkyPortal( skyportal_t *skyportal, vec3_t mins, vec3_t maxs );
mfog_t		*R_FogForSphere( const vec3_t centre, const float radius );
bool		R_CompletelyFogged( mfog_t *fog, vec3_t origin, float radius );
void		R_TranslateForEntity( ref_entity_t *ent );
void		R_RotateForEntity( ref_entity_t *entity );
void		R_CleanUpTextureUnits( void );
ref_shader_t		*R_OcclusionShader( void );
void		R_SurfIssueOcclusionQueries( void );
void		R_ClearSurfOcclusionQueryKeys( void );
int		R_GetOcclusionQueryNum( int type, int key );
void		R_AddOccludingSurface( msurface_t *surf, ref_shader_t *shader );
int		R_IssueOcclusionQuery( int query, ref_entity_t *e, vec3_t mins, vec3_t maxs );
int		R_SurfOcclusionQueryKey( ref_entity_t *e, msurface_t *surf );
bool		R_GetOcclusionQueryResultBool( int type, int key, bool wait );
void		R_BeginOcclusionPass( void );
void		R_EndOcclusionPass( void );
meshbuffer_t	*R_AddMeshToList( meshType_t meshType, mfog_t *fog, ref_shader_t *shader, int infoKey );
void		R_RenderMeshBuffer( const meshbuffer_t *mb );
void		R_AddModelMeshToList( mfog_t *fog, ref_shader_t *shader, int meshnum );
void		R_MarkLeaves( void );
void		R_DrawWorld( void );
void		R_DrawSprite( void );
void		R_DrawBeam( void );
void		R_DrawParticle( void );
void		R_DrawPoly( void );
void		R_SortMeshes( void );
void		R_RenderView( const refdef_t *fd );
void		R_RenderScene( refdef_t *rd );
void		R_AddShadowToList( ref_entity_t *entity );
void		R_RenderShadows( void );
void		R_BloomBlend ( const refdef_t *fd );
bool		R_AddSkySurface( msurface_t *fa );
void		R_DrawSky( ref_shader_t *shader );
void		R_ClearSky( void );
void		R_ClipSkySurface( msurface_t *surf );
void		R_AddSkyToList( void );
void		R_PushPoly( const meshbuffer_t *mb );
void		R_AddPolysToList( void );

// r_sprite.c
bool		R_SpriteOverflow( void );
bool		R_PushSpritePoly( const meshbuffer_t *mb );
bool		R_PushSpriteModel( const meshbuffer_t *mb );

void		R_DrawSurface( void );
void		R_AddBrushModelToList( ref_entity_t *entity );
void		R_AddWorldToList( void );

void		R_TransformWorldToScreen( vec3_t in, vec3_t out );
void		R_TransformVectorToScreen( const refdef_t *rd, const vec3_t in, vec2_t out );
void		RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );
void		R_DXTReadColor( word data, color32* out );
void		R_DXTReadColors( const byte* data, color32* out );
void		R_GetBitsFromMask( uint Mask, uint *ShiftLeft, uint *ShiftRight );

void		R_ClearMeshList( meshlist_t *meshlist );
void		R_BuildTangentVectors( int numVerts, vec4_t *points, vec4_t *normals, vec2_t *st, int numTris, uint *elems, vec4_t *sVectors );

void R_InitMeshLists( void );
void R_AllocMeshbufPointers( ref_state_t *ref );
void R_FreeMeshLists( void );
void R_DrawMeshes( void );
void R_DrawPortals( void );

// exported funcs
void		R_BeginRegistration( const char *map );
rmodel_t		*R_RegisterModel( const char *name );
void		R_SetupSky( const char *name, float rotate, const vec3_t axis );
void		R_EndRegistration( void );
void		R_ShaderRegisterImages( ref_shader_t *shader );	// prolonge registration
ref_shader_t		*R_RegisterShader( const char *name );
ref_shader_t		*R_RegisterShaderSkin( const char *name );
ref_shader_t		*R_RegisterShaderNoMip( const char *name );
void		R_DeformVertexesBBoxForShader( const ref_shader_t *shader, vec3_t ebbox );
bool		VID_ScreenShot( const char *filename, bool levelshot );
void		R_DrawFill( float x, float y, float w, float h );
void		R_DrawStretchRaw( int x, int y, int w, int h, int width, int height, const byte *raw, bool dirty );
void		R_DrawStretchPic( float x, float y, float w, float h, float sl, float tl, float sh, float th, const char *name );
void		R_GetPicSize( int *w, int *h, const char *pic );

// r_utils.c (test)
msurface_t *R_TraceLine( trace_t *tr, const vec3_t start, const vec3_t end, int surfumask );
msurface_t *R_TransformedTraceLine( trace_t *tr, const vec3_t start, const vec3_t end, ref_entity_t *test, int surfumask );
void Patch_GetFlatness( float maxflat, const float *points, int comp, const int *patch_cp, int *flat );
void Patch_Evaluate( const vec_t *p, int *numcp, const int *tess, vec_t *dest, int comp );

// cvars
extern cvar_t	*r_check_errors;
extern cvar_t	*r_hwgamma;		// use hardware gamma
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
extern cvar_t	*r_showtangentspace;
extern cvar_t	*r_showmodelbounds;
extern cvar_t	*r_showshadowvolumes;
extern cvar_t	*r_showlightmaps;
extern cvar_t	*r_showtextures;
extern cvar_t	*r_offsetfactor;
extern cvar_t	*r_offsetunits;
extern cvar_t	*r_debugsort;
extern cvar_t	*r_speeds;
extern cvar_t	*r_singleshader;
extern cvar_t	*r_skipbackend;
extern cvar_t	*r_skipfrontend;
extern cvar_t	*r_swapInterval;
extern cvar_t	*r_mode;
extern cvar_t	*r_testmode;
extern cvar_t	*r_fullscreen;
extern cvar_t	*r_minimap;
extern cvar_t	*r_minimap_size;
extern cvar_t	*r_minimap_zoom;
extern cvar_t	*r_minimap_style;
extern cvar_t	*r_pause;
extern cvar_t	*r_width;
extern cvar_t	*r_height;
extern cvar_t	*r_refreshrate;
extern cvar_t	*r_bitdepth;
extern cvar_t	*r_mapoverbrightbits;
extern cvar_t	*r_overbrightbits;
extern cvar_t	*r_shadows;
extern cvar_t	*r_occlusion_queries;
extern cvar_t	*r_occlusion_queries_finish;
extern cvar_t	*r_caustics;
extern cvar_t	*r_dynamiclights;
extern cvar_t	*r_modulate;
extern cvar_t	*r_ambientscale;
extern cvar_t	*r_directedscale;
extern cvar_t	*r_lmblocksize;
extern cvar_t	*r_fastsky;
extern cvar_t	*r_polyblend;
extern cvar_t	*r_flares;
extern cvar_t	*r_flarefade;
extern cvar_t	*r_flaresize;
extern cvar_t	*r_coronascale;
extern cvar_t	*r_intensity;
extern cvar_t	*r_texturebits;
extern cvar_t	*r_texturefilter;
extern cvar_t	*r_texturefilteranisotropy;
extern cvar_t	*r_lighting_glossintensity;
extern cvar_t	*r_lighting_glossexponent;
extern cvar_t	*r_detailtextures;
extern cvar_t	*r_portalmaps;
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