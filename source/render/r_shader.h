//=======================================================================
//			Copyright XashXT Group 2008 �
//		         r_shader.h - shader definitions
//=======================================================================

#ifndef R_SHADER_H
#define R_SHADER_H

/*
=======================================================================

 SHADER MANAGER

=======================================================================
*/
#define TABLES_HASH_SIZE		1024
#define MAX_TABLES			4096

#define MAX_SHADERS			1024
#define SHADERS_HASH_SIZE		256
#define MAX_EXPRESSION_OPS		4096
#define MAX_EXPRESSION_REGISTERS	4096
#define MAX_PROGRAM_PARMS		16
#define MAX_PROGRAM_MAPS		8
#define SHADER_MAX_EXPRESSIONS	16
#define SHADER_MAX_STAGES		8
#define SHADER_MAX_TRANSFORMS		8
#define SHADER_MAX_TEXTURES		16	// max frames
#define SHADER_MAX_TCMOD		8

// shader flags
#define SHADER_EXTERNAL			0x00000001
#define SHADER_DEFAULTED			0x00000002
#define SHADER_HASLIGHTMAP			0x00000004
#define SHADER_SURFACEPARM			0x00000008
#define SHADER_NOMIPMAPS			0x00000010
#define SHADER_NOPICMIP			0x00000020
#define SHADER_NOCOMPRESS			0x00000040
#define SHADER_NOSHADOWS			0x00000080
#define SHADER_NOFRAGMENTS			0x00000100
#define SHADER_ENTITYMERGABLE			0x00000200
#define SHADER_POLYGONOFFSET			0x00000400
#define SHADER_CULL				0x00000800
#define SHADER_SORT				0x00001000
#define SHADER_TESSSIZE			0x00002000
#define SHADER_SKYPARMS			0x00004000
#define SHADER_DEFORMVERTEXES			0x00008000

// shader stage flags
typedef enum
{
	SHADERSTAGE_NEXTBUNDLE	= BIT(0),
	SHADERSTAGE_VERTEXPROGRAM	= BIT(1),
	SHADERSTAGE_FRAGMENTPROGRAM	= BIT(2),
	SHADERSTAGE_ALPHAFUNC	= BIT(3),
	SHADERSTAGE_BLENDFUNC	= BIT(4),
	SHADERSTAGE_DEPTHFUNC	= BIT(5),
	SHADERSTAGE_DEPTHWRITE	= BIT(6),
	SHADERSTAGE_DETAIL		= BIT(7),
	SHADERSTAGE_RGBGEN		= BIT(8),
	SHADERSTAGE_ALPHAGEN	= BIT(10),
} stageFlags_t;

// stage bundle flags
typedef enum
{
	STAGEBUNDLE_ANIMFREQUENCY	= BIT(0),
	STAGEBUNDLE_MAP		= BIT(1),
	STAGEBUNDLE_BUMPMAP		= BIT(2),
	STAGEBUNDLE_CUBEMAP		= BIT(3),
	STAGEBUNDLE_VIDEOMAP	= BIT(4),
	STAGEBUNDLE_TEXENVCOMBINE	= BIT(5),
	STAGEBUNDLE_TCGEN		= BIT(6),
	STAGEBUNDLE_TCMOD		= BIT(7),
} bundleFlags_t;

typedef enum
{
	OP_TYPE_MULTIPLY = 0,
	OP_TYPE_DIVIDE,
	OP_TYPE_MOD,
	OP_TYPE_ADD,
	OP_TYPE_SUBTRACT,
	OP_TYPE_GREATER,
	OP_TYPE_LESS,
	OP_TYPE_GEQUAL,
	OP_TYPE_LEQUAL,
	OP_TYPE_EQUAL,
	OP_TYPE_NOTEQUAL,
	OP_TYPE_AND,
	OP_TYPE_OR,
	OP_TYPE_TABLE
} opType_t;

typedef enum
{
	EXP_REGISTER_ZERO = 0,
	EXP_REGISTER_ONE,
	EXP_REGISTER_TIME,
	EXP_REGISTER_PARM0,
	EXP_REGISTER_PARM1,
	EXP_REGISTER_PARM2,
	EXP_REGISTER_PARM3,
	EXP_REGISTER_PARM4,
	EXP_REGISTER_PARM5,
	EXP_REGISTER_PARM6,
	EXP_REGISTER_PARM7,
	EXP_REGISTER_GLOBAL0,
	EXP_REGISTER_GLOBAL1,
	EXP_REGISTER_GLOBAL2,
	EXP_REGISTER_GLOBAL3,
	EXP_REGISTER_GLOBAL4,
	EXP_REGISTER_GLOBAL5,
	EXP_REGISTER_GLOBAL6,
	EXP_REGISTER_GLOBAL7,
	EXP_REGISTER_NUM_PREDEFINED
} expRegister_t;

// shader types used for shader loading
typedef enum
{
	SHADER_SKY = 0,		// sky box shader
	SHADER_FONT,		// speical case for displayed fonts
	SHADER_NOMIP,		// 2d images
	SHADER_TEXTURE,		// bsp polygon
	SHADER_STUDIO,		// studio skins
	SHADER_SPRITE,		// sprite frames
	SHADER_GENERIC		// generic shader
} shaderType_t;

typedef enum
{
	AMMO_DIGIT1 = 0,
	AMMO_DIGIT2,
	AMMO_DIGIT3,
	AMMO_WARNING
} ammoDigit_t;

typedef enum
{
	WAVEFORM_SIN,
	WAVEFORM_TRIANGLE,
	WAVEFORM_SQUARE,
	WAVEFORM_SAWTOOTH,
	WAVEFORM_INVERSESAWTOOTH,
	WAVEFORM_NOISE
} waveForm_t;

typedef enum
{
	SUBVIEW_NONE,
	SUBVIEW_MIRROR,
	SUBVIEW_REMOTE
} subview_t;

typedef enum
{
	SORT_BAD = 0,
	SORT_SUBVIEW,
	SORT_OPAQUE,
	SORT_SKY,
	SORT_DECAL,
	SORT_SEETHROUGH,
	SORT_BANNER,
	SORT_UNDERWATER,
	SORT_WATER,
	SORT_FARTHEST,
	SORT_FAR,
	SORT_MEDIUM,
	SORT_CLOSE,
	SORT_ADDITIVE,
	SORT_ALMOST_NEAREST,
	SORT_NEAREST,
	SORT_POST_PROCESS
} sort_t;

typedef enum
{
	DEFORM_WAVE,
	DEFORM_MOVE,
	DEFORM_NORMAL,
	DEFORM_AUTOSPRITE,
	DEFORM_AUTOSPRITE2
} deformType_t;

typedef enum
{
	TCGEN_BASE,
	TCGEN_LIGHTMAP,
	TCGEN_ENVIRONMENT,
	TCGEN_VECTOR,
	TCGEN_WARP,
	TCGEN_LIGHTVECTOR,
	TCGEN_HALFANGLE,
	TCGEN_REFLECTION,
	TCGEN_NORMAL
} tcGenType_t;

typedef enum
{
	TCMOD_TRANSLATE,
	TCMOD_SCALE,
	TCMOD_SCROLL,
	TCMOD_ROTATE,
	TCMOD_STRETCH,
	TCMOD_TURB,
	TCMOD_TRANSFORM
} tcModType_t;

typedef enum
{
	RGBGEN_IDENTITY,
	RGBGEN_IDENTITYLIGHTING,
	RGBGEN_WAVE,
	RGBGEN_COLORWAVE,
	RGBGEN_VERTEX,
	RGBGEN_ONEMINUSVERTEX,
	RGBGEN_ENTITY,
	RGBGEN_ONEMINUSENTITY,
	RGBGEN_LIGHTINGAMBIENT,
	RGBGEN_LIGHTINGDIFFUSE,
	RGBGEN_CONST
} rgbGenType_t;

typedef enum
{
	ALPHAGEN_IDENTITY,
	ALPHAGEN_WAVE,
	ALPHAGEN_ALPHAWAVE,
	ALPHAGEN_VERTEX,
	ALPHAGEN_ONEMINUSVERTEX,
	ALPHAGEN_ENTITY,
	ALPHAGEN_ONEMINUSENTITY,
	ALPHAGEN_DOT,
	ALPHAGEN_ONEMINUSDOT,
	ALPHAGEN_FADE,
	ALPHAGEN_ONEMINUSFADE,
	ALPHAGEN_LIGHTINGSPECULAR,
	ALPHAGEN_CONST
} alphaGenType_t;

typedef enum
{
	TEX_GENERIC,
	TEX_LIGHTMAP,
	TEX_CINEMATIC
} texType_t;

typedef struct table_s
{
	string		name;		// table name
	int		index;

	bool		clamp;
	bool		snap;

	int		size;
	float		*values;		// float[]
	struct table_s	*nextHash;
} table_t;

typedef struct
{
	ammoDigit_t	type;
	int		lowAmmo;
	string		shaders[3];
} ammoDisplay_t;

typedef struct
{
	opType_t		opType;
	int		a, b, c;
} statement_t;

typedef struct
{
	waveForm_t	type;
	float		params[4];
} waveFunc_t;

typedef struct
{
	GLenum		mode;
} cull_t;

typedef struct
{
	texture_t		*farBox[6];
	float		cloudHeight;
	texture_t		*nearBox[6];
} skyParms_t;

typedef struct
{
	deformType_t	type;
	waveFunc_t	func;
	float		params[3];
} deform_t;

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
	float		params[6];
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
	float		params[3];
} rgbGen_t;

typedef struct
{
	alphaGenType_t	type;
	waveFunc_t	func;
	float		params[3];
} alphaGen_t;

typedef struct stageBundle_s
{
	texType_t		texType;
	texFlags_t	texFlags;
	texFilter_t	texFilter;
	texWrap_t		texWrap;

	bundleFlags_t	flags;
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

typedef struct shaderStage_s
{
	bool		ignore;
	int		conditionRegister;
	uint		flags;

	stageBundle_t	*bundles[MAX_TEXTURE_UNITS];
	uint		numBundles;

	program_t		*vertexProgram;
	program_t		*fragmentProgram;

	alphaFunc_t	alphaFunc;
	blendFunc_t	blendFunc;
	depthFunc_t	depthFunc;

	rgbGen_t		rgbGen;
	alphaGen_t	alphaGen;
} shaderStage_t;

typedef struct ref_shader_s
{
	string		name;
	int		index;
	shaderType_t	type;
	uint		surfaceParm;
	int		conditionRegister;
	uint		flags;

	cull_t		cull;
	sort_t		sort;
	uint		tessSize;
	skyParms_t	skyParms;
	deform_t		deform[SHADER_MAX_TRANSFORMS];
	uint		numDeforms;
	shaderStage_t	*stages[SHADER_MAX_STAGES];
	uint	   	numStages;

	subview_t		subview;
	int		subviewWidth;
	int		subviewHeight;
	ammoDisplay_t	ammoDisplay;
	bool		constantExpressions;
	statement_t	*statements;
	int		numstatements;
	float		*expressions;
	int		numRegisters;

	struct ref_script_s	*shaderScript;
	struct ref_shader_s	*nextHash;
} ref_shader_t;

#endif//R_SHADER_H