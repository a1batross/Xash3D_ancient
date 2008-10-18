//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       r_shader.h - render shader language
//=======================================================================

#ifndef R_SHADER_H
#define R_SHADER_H

#define MAX_SHADERS			1024	// can be blindly increased

#define Shader_ForKey( num, s )	((s) = r_shaders[((num) & 0xFFF)])		// returns shader ptr
#define Shader_Sortkey( shader, sort )	((( sort )<<26 )|((shader)->shaderNum))		// returns shader->sortKey


/*
=======================================================================

 SHADERS

=======================================================================
*/
#define SHADER_MAX_EXPRESSIONS		32
#define SHADER_MAX_STAGES			8
#define SHADER_MAX_TEXTURES			16		// max animation frames (increase if need)
#define SHADER_MAX_DEFORMVERTS		8
#define SHADER_MAX_TCMOD			8

// shader types used for shader loading
typedef enum
{
	SHADER_GENERIC = 0,			// internal system shader			
	SHADER_SURFACE,			// lightmapped bsp surface
	SHADER_VERTEX,
	SHADER_SKYDOME,			// skydome or skybox shader
	SHADER_STUDIO,			// studio meshes
	SHADER_SPRITE,			// sprites
	SHADER_NOMIP,			// 2d images
} shaderType_t;

// shader global flags
#define SHADER_EXTERNAL		(1<<0)	// shader have external script
#define SHADER_LIGHTMAP		(1<<1)	// shaderType should be equal SHADER_SURFACE
#define SHADER_NOSHADOWS		(1<<2)	// ignore shadows for this surface
#define SHADER_NODLIGHTS		(1<<3)	// ignore dynamic lights for this surface
#define SHADER_NOMARKS		(1<<4)	// ignore decals for this surface
#define SHADER_ENTITYMERGABLE		(1<<5)	// cl.entity can change color, alpha and frame
#define SHADER_POLYGONOFFSET		(1<<6)	// surface using POLYGON_OFFSET_FILL
#define SHADER_CULL			(1<<7)	// set GL_CullFace
#define SHADER_SORT			(1<<8)	// sort surface in 3d-mode
#define SHADER_AMMODISPLAY		(1<<9)	// special case for draw ammo counters on viewmodel
#define SHADER_TESSSIZE		(1<<10)	// patches custom subdivision level only
#define SHADER_DEFORMVERTEXES		(1<<11)	// CPU realtime vertex deformation
#define SHADER_SKYPARMS		(1<<12)	// shaderType should be equal SHADER_SKYDOME
#define SHADER_AUTOSPRITE		(1<<13)	// autosprites draw both faces
#define SHADER_FLARE		(1<<14)	// specail case for flare shaders
#define SHADER_PORTAL		(1<<15)	// shader rendering portal effects (portal, screen, mirror etc)
#define SHADER_REFLECTION		(1<<16)	// do reflection (glsl using)
#define SHADER_REFRACTION		(1<<17)	// do refraction (glsl using)

// shader stage flags
#define SHADERSTAGE_NEXTBUNDLE	(1<<0)	// for two stages blended with GL_TEXENV_COMBINE
#define SHADERSTAGE_PROGRAM		(1<<1)	// stage have a glsl program
#define SHADERSTAGE_ALPHAFUNC		(1<<2)	// stage have glAlphaFunc
#define SHADERSTAGE_BLENDFUNC		(1<<3)	// stage have glBlendFunc
#define SHADERSTAGE_DEPTHFUNC		(1<<4)	// stage have glDepthFunc
#define SHADERSTAGE_DEPTHWRITE	(1<<5)	// write depth
#define SHADERSTAGE_RGBGEN		(1<<6)	// generate color
#define SHADERSTAGE_ALPHAGEN		(1<<7)	// generate alpha
#define SHADERSTAGE_DETAIL		(1<<8)	// currently not used

// KILLME
#define SHADERSTAGE_NOCOLORARRAY	(1<<9)
#define SHADERSTAGE_DLIGHT		(1<<10)
#define SHADERSTAGE_STENCILSHADOW	(1<<11)

// stage bundle flags
#define STAGEBUNDLE_ANIMFREQUENCY	(1<<0)	// if not set and have bit ENTITYMERGABLE can switch frames manually 
#define STAGEBUNDLE_TEXENVCOMBINE	(1<<1)	// if set bit NEXTBUNDLE blends previous and surrent frames
#define STAGEBUNDLE_TCGEN		(1<<2)	// generate texCoords
#define STAGEBUNDLE_TCMOD		(1<<3)	// rotate, move, deform texCoords
#define STAGEBUNDLE_MAP		(1<<4)	// "map"
#define STAGEBUNDLE_BUMPMAP		(1<<5)	// "bumpMap"
#define STAGEBUNDLE_CUBEMAP		(1<<6)	// "cubeMap"
#define STAGEBUNDLE_VIDEOMAP		(1<<7)	// "videoMap"
#define STAGEBUNDLE_ANGLEMAP		(1<<8)	// frame = ((viewangles[1] - ent->angles[1]) / 360 * 8 + 0.5 - 4) & 7;
#define STAGEBUNDLE_NORMALMAP		(1<<9)	// currently not used

typedef enum
{
	WAVEFORM_NONE = 0,			// --------
	WAVEFORM_SINE,
	WAVEFORM_TRIANGLE,			// triangle approximation "Super Sand Castle"
	WAVEFORM_SQUARE,			// pure meander
	WAVEFORM_SAWTOOTH,
	WAVEFORM_INVERSESAWTOOTH,
	WAVEFORM_NOISE,
	WAVEFORM_CONSTANT
} waveForm_t;

typedef enum
{
	SORT_NONE		= 0,
	SORT_PORTAL,
	SORT_SKY,
	SORT_OPAQUE,
	SORT_DECAL,
	SORT_ALPHATEST,
	SORT_BANNER,
	SORT_UNDERWATER,
	SORT_WATER,
	SORT_INNERBLEND,
	SORT_BLEND1,
	SORT_BLEND2,
	SORT_BLEND3,
	SORT_BLEND4,
	SORT_OUTERBLEND,
	SORT_ADDITIVE,
	SORT_NEAREST
} sort_t;

typedef enum
{
	AMMODISPLAY_DIGIT1,
	AMMODISPLAY_DIGIT2,
	AMMODISPLAY_DIGIT3,
	AMMODISPLAY_WARNING
} ammoDisplayType_t;

typedef enum
{
	DEFORMVERTEXES_NONE = 0,
	DEFORMVERTEXES_WAVE,		// using waveForm_t
	DEFORMVERTEXES_MOVE,
	DEFORMVERTEXES_BULGE,
	DEFORMVERTEXES_NORMAL,
	DEFORMVERTEXES_AUTOSPRITE,
	DEFORMVERTEXES_AUTOSPRITE2,
	DEFORMVERTEXES_AUTOPARTICLE,		// used for particelShader
	DEFORMVERTEXES_PROJECTION_SHADOW	// currently not used
} deformVertsType_t;

typedef enum
{
	TCGEN_BASE = 0,			// use original texCoords as default
	TCGEN_LIGHTMAP,			// use lightMap texCoords (for TEX_LIGHTMAP)
	TCGEN_ENVIRONMENT,			// simple EnvMapping
	TCGEN_FOG,
	TCGEN_VECTOR,			// const vector specified in shader
	TCGEN_LIGHTVECTOR,			// vector == LightDir
	TCGEN_WARP,			// q1/q2 warp surfaces (water, slime, lava)
	TCGEN_HALFANGLE,			// halfAngle = lightVector + eyeVector;
	TCGEN_REFLECTION,			// enable GL_REFLECTION_MAP_ARB
	TCGEN_PROJECTION,			// project view onto plane (PORTAL used)
	TCGEN_NORMAL,
	TCGEN_SVECTORS,			// using sVectors array instead texCoordArray
	TCGEN_PROJECTION_SHADOW		// currently not used
} tcGenType_t;

typedef enum
{
	TCMOD_NONE = 0,
	TCMOD_TRANSLATE,			// same as glTranslate
	TCMOD_SCALE,			// same as glScale
	TCMOD_SCROLL,
	TCMOD_ROTATE,			// same as glRotate
	TCMOD_STRETCH,			// proporcional scaling
	TCMOD_TURB,			// warp tubulence
	TCMOD_TRANSFORM
} tcModType_t;

typedef enum
{
	RGBGEN_IDENTITY = 0,		// glColor( 1.0, 1.0f, 1.0f )
	RGBGEN_IDENTITYLIGHTING,		// rgb = 1.0f / pow( 2.0f, max( 0, floor( r_overbrightbits )))
	RGBGEN_WAVE,			// using waveForm_t
	RGBGEN_CONST,			// RGB values from shader
	RGBGEN_COLORWAVE,			// normalize RGBwave with RGB values from shader
	RGBGEN_VERTEX,			// get color from bsp vertexes or deformVertexes
	RGBGEN_EXACTVERTEX,
	RGBGEN_ONEMINUSVERTEX,		// 1.0 - vertexColor
	RGBGEN_ENTITY,			// ent->renderColor (need ENTITYMERGABLE bit)
	RGBGEN_ONEMINUSENTITY,		// 1.0 - ent->renderColor (need ENTITYMERGABLE bit)
	RGBGEN_LIGHTINGAMBIENT,		// ambient LightColor
	RGBGEN_LIGHTINGDIFFUSE,		// diffuse LightColor
	RGBGEN_LIGHTINGDIFFUSE_ONLY,		// ignore ambientColor
	RGBGEN_LIGHTINGAMBIENT_ONLY,		// ignore diffuseColor
	RGBGEN_ENVIRONMENT,			// simple EnvColorMapping
	RGBGEN_FOG
} rgbGenType_t;

typedef enum
{
	ALPHAGEN_IDENTITY = 0,		// glAlpha( 1.0 )
	ALPHAGEN_CONST,			// get alpha from shader
	ALPHAGEN_WAVE,			// using waveForm_t
	ALPHAGEN_ALPHAWAVE,			// normalize alphawave with alpha value from shader
	ALPHAGEN_VERTEX,			// get alpha from bsp vertexes or deformVerts
	ALPHAGEN_ONEMINUSVERTEX,		// 1.0 - vertexAlpha
	ALPHAGEN_ENTITY,			// ent->renderAmt (need ENTITYMERGABLE bit)
	ALPHAGEN_ONEMINUSENTITY,		// 1.0 - ent->renderAmt (need ENTITYMERGABLE bit)
	ALPHAGEN_SPECULAR,			// p = (vieworg - ent->origin) - vertex
	ALPHAGEN_DOT,			// v = DotProduct( vforward, normal )
	ALPHAGEN_ONEMINUSDOT,		// v = 1.0 - DotProduct( vforward, normal )
	ALPHAGEN_FADE,			// alpha = dist( ent->origin, vieworg ) * maxDist
	ALPHAGEN_ONEMINUSFADE,		// alpha = (1.0 - ALPHAGEN_FADE) (another name - ALPHAGEN_PORTAL)
} alphaGenType_t;

typedef enum
{
	TEX_GENERIC,			// vertexArray, texCoordArray
	TEX_LIGHTMAP,			// vertexArray, lmCoordArray
	TEX_CINEMATIC,			// RoQ frame
	TEX_RENDERVIEW,			// scene from another point (portals, mirrors etc)
	TEX_DLIGHT			// currently not used
} texType_t;


typedef struct
{
	waveForm_t	type;
	float		params[4];
} waveFunc_t;

typedef struct
{
	GLenum		mode;
} cullFunc_t;

typedef struct
{
	ammoDisplayType_t	type;
	int		lowAmmo;		// threshold before change color of digits
	string		shader[3];	// remap shaders
} ammoDisplay_t;

typedef struct
{
	texture_t		*farBox[6];
	float		cloudHeight;
	texture_t		*nearBox[6];
} skyParms_t;

typedef struct
{
	deformVertsType_t	type;
	waveFunc_t	func;
	float		params[3];
} deformVerts_t;

typedef struct
{
	GLint		rgbCombine;
	GLint		rgbSource[3];
	GLint		rgbOperand[3];
	GLint		rgbScale;

	GLint		alphaCombine;
	GLint		alphaSource[3];
	GLint		alphaOperand[3];
	GLint		alphaScale;

	GLfloat		constColor[4];
} texEnvCombine_t;

typedef struct
{
	tcGenType_t	type;
	float		params[8];
} tcGen_t;

typedef struct
{
	tcModType_t	type;
	waveFunc_t	func;
	float		params[6];
} tcMod_t;

typedef struct
{
	GLenum		func;
	GLclampf		ref;
} alphaFunc_t;

typedef struct
{
	GLenum		src;
	GLenum		dst;
} blendFunc_t;

typedef struct
{
	GLenum		func;
} depthFunc_t;

typedef struct
{
	rgbGenType_t	type;
	waveFunc_t	func;
	float		params[4];
} rgbGen_t;

typedef struct
{
	alphaGenType_t	type;
	waveFunc_t	func;
	float		params[3];
} alphaGen_t;

typedef struct
{
	texType_t		texType;
	uint		texFlags;		// TF_* flags
	uint		flags;

	texture_t		*textures[SHADER_MAX_TEXTURES];
	uint		numTextures;

	float		animFrequency;
	video_t		cinematicHandle;

	GLint		texEnv;
	texEnvCombine_t	texEnvCombine;

	tcGen_t		tcGen;
	tcMod_t		tcMod[SHADER_MAX_TCMOD];
	uint		tcModNum;
} stageBundle_t;

typedef struct
{
	bool		ignore;		// this stage ignored for some reasons (disabled, not needed etc)
	uint		flags;

	stageBundle_t	*bundles[MAX_TEXTURE_UNITS];
	uint		numBundles;

	const char	*program;		// GLSL program
	progType_t	progType;

	alphaFunc_t	alphaFunc;
	blendFunc_t	blendFunc;
	depthFunc_t	depthFunc;

	rgbGen_t		rgbGen;
	alphaGen_t	alphaGen;
} shaderStage_t;

typedef struct ref_shader_s
{
	string		name;
	int		shaderNum;	// shader_t handle
	shaderType_t	shaderType;
	uint		surfaceParm;	// dshader->surfaceFlags
	uint		contentFlags;	// dshader->contentFlags
	uint		features;		// renderMesh flags
	uint		flags;		// shader flags

	cullFunc_t	cull;
	sort_t		sort;
	ammoDisplay_t	ammoDisplay;	// only for SHADER_STUDIO type
	uint		tessSize;		// bsp patch subdivision level
	skyParms_t	skyParms;		// for realtime change sky

	float		fogDist;		// generic fog stuff
	float		fogClearDist;
	vec4_t		fogColor;

	deformVerts_t	deformVerts[SHADER_MAX_DEFORMVERTS];
	uint		deformVertsNum;

	shaderStage_t	*stages[SHADER_MAX_STAGES];
	uint		numStages;

	// link stuff
	uint		sortKey;
	struct ref_shader_s	*nextHash;
} ref_shader_t;

extern ref_shader_t	*r_shaders[MAX_SHADERS];
extern int	r_numShaders;

// built-in shaders
extern ref_shader_t	*r_defaultShader;
extern ref_shader_t	*r_lightmapShader;

ref_shader_t *R_FindShader( const char *name, shaderType_t shaderType, uint surfaceParm );
void R_SetInternalMap( texture_t *mipTex );		// internal textures (skins, spriteframes, etc)
void R_ShaderList_f( void );
void R_InitShaders( void );
void R_ShutdownShaders (void);

#endif//R_SHADER_H