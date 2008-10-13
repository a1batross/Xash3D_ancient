//=======================================================================
//			Copyright XashXT Group 2008 ©
//		r_program.c - OpenGL shading language support
//=======================================================================

#include "r_local.h"

#define MAX_PROGRAMS		1024
#define MAX_DEFINES_FEATURES		255

// FIXME: test
bool GLSL_no_half_types = false;
bool GLSL_branching = true;

typedef struct
{
	int		bit;
	const char	*define;
	const char	*suffix;
} prog_feature_t;

typedef struct
{
	char		*name;
	uint		features;
	const char	*string;

	int		object;
	int		vertexShader;
	int		fragmentShader;

	// program uniforms
	int		locEyeOrigin;
	int		locLightDir;
	int		locLightOrigin;
	int		locLightAmbient;
	int		locLightDiffuse;
	int		locGlossIntensity;
	int		locGlossExponent;
	int		locOffsetMappingScale;
	int		locFrontPlane;
	int		locTextureWidth;
	int		locTextureHeight;
	int		locProjDistance;
	int		locDeluxemapOffset[LM_STYLES];
	int		loclsColor[LM_STYLES];
} program_t;

static program_t r_programs[MAX_PROGRAMS];
static byte *r_progpool;

static void R_GetProgramUniformLocations( program_t *program );
static const char *r_defaultProgram;
static const char *r_defaultDistortionProgram;
static const char *r_defaultShadowmapProgram;

/*
================
R_InitPrograms
================
*/
void R_InitPrograms( void )
{
	int	features = 0;

	Mem_Set( r_programs, 0, sizeof( r_programs ));

	if( !GL_Support( R_SHADER_GLSL100_EXT ))
		return;

	r_progpool = Mem_AllocPool( "GLSL Programs" );

	if( GLSL_branching ) features |= PROGRAM_APPLY_BRANCHING;
	if( GLSL_no_half_types ) features |= PROGRAM_APPLY_NO_HALF_TYPES;

	// register programs that are most likely to be used
	R_RegisterProgram( DEFAULT_PROGRAM, NULL, 0|features );
	R_RegisterProgram( DEFAULT_PROGRAM, NULL, PROGRAM_APPLY_FB_LIGHTMAP|PROGRAM_APPLY_LIGHTSTYLE0|features );
	R_RegisterProgram( DEFAULT_PROGRAM, NULL, PROGRAM_APPLY_FB_LIGHTMAP|PROGRAM_APPLY_LIGHTSTYLE0|PROGRAM_APPLY_SPECULAR|features );
	R_RegisterProgram( DEFAULT_PROGRAM, NULL, PROGRAM_APPLY_FB_LIGHTMAP|PROGRAM_APPLY_LIGHTSTYLE0|PROGRAM_APPLY_SPECULAR|PROGRAM_APPLY_AMBIENT_COMPENSATION|features );

	R_RegisterProgram( DEFAULT_PROGRAM, NULL, PROGRAM_APPLY_DIRECTIONAL_LIGHT|features );
//	R_RegisterProgram( DEFAULT_PROGRAM, NULL, PROGRAM_APPLY_DIRECTIONAL_LIGHT|PROGRAM_APPLY_SPECULAR|features );
	R_RegisterProgram( DEFAULT_PROGRAM, NULL, PROGRAM_APPLY_DIRECTIONAL_LIGHT|PROGRAM_APPLY_OFFSETMAPPING|features );

	R_RegisterProgram( DEFAULT_DISTORTION_PROGRAM, r_defaultDistortionProgram, 0|features );
	R_RegisterProgram( DEFAULT_SHADOWMAP_PROGRAM, r_defaultShadowmapProgram, 0|features );
}

/*
================
R_ProgramCopyString
================
*/
static char *R_ProgramCopyString( const char *in )
{
	char	*out;

	out = Mem_Alloc( r_progpool, ( com.strlen( in ) + 1 ));
	com.strcpy( out, in );

	return out;
}

/*
================
R_DeleteProgram
================
*/
static void R_DeleteProgram( program_t *program )
{
	if( program->vertexShader )
	{
		pglDetachObjectARB( program->object, program->vertexShader );
		pglDeleteObjectARB( program->vertexShader );
		program->vertexShader = 0;
	}

	if( program->fragmentShader )
	{
		pglDetachObjectARB( program->object, program->fragmentShader );
		pglDeleteObjectARB( program->fragmentShader );
		program->fragmentShader = 0;
	}

	if( program->object )
		pglDeleteObjectARB( program->object );

	if( program->name )
		Mem_Free( program->name );

	Mem_Set( program, 0, sizeof( program_t ));
}

/*
================
R_CompileShader
================
*/
static int R_CompileShader( int program, const char *programName, const char *shaderName, int shaderType, const char **strings, int numStrings )
{
	int	shader, compiled;

	shader = pglCreateShaderObjectARB((GLenum)shaderType );
	if( !shader ) return 0;
          
	// if lengths is NULL, then each string is assumed to be null-terminated
	pglShaderSourceARB( shader, numStrings, strings, NULL );
	pglCompileShaderARB( shader );
	pglGetObjectParameterivARB( shader, GL_OBJECT_COMPILE_STATUS_ARB, &compiled );

	if( !compiled )
	{
		char	log[4096];

		pglGetInfoLogARB( shader, sizeof( log ) - 1, NULL, log );
		log[sizeof( log ) - 1] = 0;

		if( log[0] ) MsgDev( D_INFO, "Failed to compile %s shader for program %s:\n%s\n", shaderName, programName, log );

		pglDeleteObjectARB( shader );
		return 0;
	}

	pglAttachObjectARB( program, shader );

	return shader;
}

/*
================
R_FindProgram
================
*/
int R_FindProgram( const char *name )
{
	int	i;
	program_t	*program;

	for( i = 0, program = r_programs; i < MAX_PROGRAMS; i++, program++ )
	{
		if( !program->name ) break;
		if( !com.stricmp( program->name, name ))
			return ( i+1 );
	}
	return 0;
}

static const prog_feature_t prog_features[] =
{
	{ PROGRAM_APPLY_LIGHTSTYLE0, "#define APPLY_LIGHTSTYLE0\n", "_ls0" },
	{ PROGRAM_APPLY_FB_LIGHTMAP, "#define APPLY_FBLIGHTMAP\n", "_fb" },
	{ PROGRAM_APPLY_LIGHTSTYLE1, "#define APPLY_LIGHTSTYLE1\n", "_ls1" },
	{ PROGRAM_APPLY_LIGHTSTYLE2, "#define APPLY_LIGHTSTYLE2\n", "_ls2" },
	{ PROGRAM_APPLY_LIGHTSTYLE3, "#define APPLY_LIGHTSTYLE3\n", "_ls3" },
	{ PROGRAM_APPLY_DIRECTIONAL_LIGHT, "#define APPLY_DIRECTIONAL_LIGHT\n", "_dirlight" },
	{ PROGRAM_APPLY_SPECULAR, "#define APPLY_SPECULAR\n", "_gloss" },
	{ PROGRAM_APPLY_OFFSETMAPPING, "#define APPLY_OFFSETMAPPING\n", "_offmap" },
	{ PROGRAM_APPLY_RELIEFMAPPING, "#define APPLY_RELIEFMAPPING\n", "_relmap" },
	{ PROGRAM_APPLY_AMBIENT_COMPENSATION, "#define APPLY_AMBIENT_COMPENSATION\n", "_amb" },
	{ PROGRAM_APPLY_DECAL, "#define APPLY_DECAL\n", "_decal" },
	{ PROGRAM_APPLY_BASETEX_ALPHA_ONLY, "#define APPLY_BASETEX_ALPHA_ONLY\n", "_alpha" },

	{ PROGRAM_APPLY_EYEDOT, "#define APPLY_EYEDOT\n", "_eyedot" },
	{ PROGRAM_APPLY_DISTORTION_ALPHA, "#define APPLY_DISTORTION_ALPHA\n", "_alpha" },

	{ PROGRAM_APPLY_PCF2x2, "#define APPLY_PCF2x2\n", "_pcf2x2" },
	{ PROGRAM_APPLY_PCF3x3, "#define APPLY_PCF3x3\n", "_pcf3x3" },

	{ PROGRAM_APPLY_BRANCHING, "#define APPLY_BRANCHING\n", "_branch" },
	{ PROGRAM_APPLY_CLIPPING, "#define APPLY_CLIPPING\n", "_clip" },
	{ PROGRAM_APPLY_NO_HALF_TYPES, "#define APPLY_NO_HALF_TYPES\n", "_nohalf" }
};

// FIXME: move to baserc.dll ?
static const char *r_defaultProgram =
"// \" APPLICATION \" GLSL shader\n"
"\n"
"#if !defined(__GLSL_CG_DATA_TYPES) || defined(APPLY_NO_HALF_TYPES)\n"
"#define myhalf float\n"
"#define myhalf2 vec2\n"
"#define myhalf3 vec3\n"
"#define myhalf4 vec4\n"
"#else\n"
"#define myhalf half\n"
"#define myhalf2 half2\n"
"#define myhalf3 half3\n"
"#define myhalf4 half4\n"
"#endif\n"
"\n"
"varying vec2 TexCoord;\n"
"#ifdef APPLY_LIGHTSTYLE0\n"
"varying vec4 LightmapTexCoord01;\n"
"#ifdef APPLY_LIGHTSTYLE2\n"
"varying vec4 LightmapTexCoord23;\n"
"#endif\n"
"#endif\n"
"\n"
"#if defined(APPLY_SPECULAR) || defined(APPLY_OFFSETMAPPING) || defined(APPLY_RELIEFMAPPING)\n"
"varying vec3 EyeVector;\n"
"#endif\n"
"\n"
"#ifdef APPLY_DIRECTIONAL_LIGHT\n"
"varying vec3 LightVector;\n"
"#endif\n"
"\n"
"varying mat3 strMatrix; // directions of S/T/R texcoords (tangent, binormal, normal)\n"
"\n"
"#ifdef VERTEX_SHADER\n"
"// Vertex shader\n"
"\n"
"uniform vec3 EyeOrigin;\n"
"\n"
"#ifdef APPLY_DIRECTIONAL_LIGHT\n"
"uniform vec3 LightDir;\n"
"#endif\n"
"\n"
"void main()\n"
"{\n"
"gl_FrontColor = gl_Color;\n"
"\n"
"TexCoord = vec2 (gl_TextureMatrix[0] * gl_MultiTexCoord0);\n"
"\n"
"#ifdef APPLY_LIGHTSTYLE0\n"
"LightmapTexCoord01.st = gl_MultiTexCoord4.st;\n"
"#ifdef APPLY_LIGHTSTYLE1\n"
"LightmapTexCoord01.pq = gl_MultiTexCoord5.st;\n"
"#ifdef APPLY_LIGHTSTYLE2\n"
"LightmapTexCoord23.st = gl_MultiTexCoord6.st;\n"
"#ifdef APPLY_LIGHTSTYLE3\n"
"LightmapTexCoord23.pq = gl_MultiTexCoord7.st;\n"
"#endif\n"
"#endif\n"
"#endif\n"
"#endif\n"
"\n"
"strMatrix[0] = gl_MultiTexCoord1.xyz;\n"
"strMatrix[2] = gl_Normal.xyz;\n"
"strMatrix[1] = gl_MultiTexCoord1.w * cross (strMatrix[2], strMatrix[0]);\n"
"\n"
"#if defined(APPLY_SPECULAR) || defined(APPLY_OFFSETMAPPING) || defined(APPLY_RELIEFMAPPING)\n"
"vec3 EyeVectorWorld = EyeOrigin - gl_Vertex.xyz;\n"
"EyeVector = EyeVectorWorld * strMatrix;\n"
"#endif\n"
"\n"
"#ifdef APPLY_DIRECTIONAL_LIGHT\n"
"LightVector = LightDir * strMatrix;\n"
"#endif\n"
"\n"
"gl_Position = ftransform ();\n"
"#ifdef APPLY_CLIPPING\n"
"#ifdef __GLSL_CG_DATA_TYPES\n"
"gl_ClipVertex = gl_ModelViewMatrix * gl_Vertex;\n"
"#endif\n"
"#endif\n"
"}\n"
"\n"
"#endif // VERTEX_SHADER\n"
"\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"// Fragment shader\n"
"\n"
"#ifdef APPLY_LIGHTSTYLE0\n"
"uniform sampler2D LightmapTexture0;\n"
"uniform float DeluxemapOffset0; // s-offset for LightmapTexCoord\n"
"uniform myhalf3 lsColor0; // lightstyle color\n"
"\n"
"#ifdef APPLY_LIGHTSTYLE1\n"
"uniform sampler2D LightmapTexture1;\n"
"uniform float DeluxemapOffset1;\n"
"uniform myhalf3 lsColor1;\n"
"\n"
"#ifdef APPLY_LIGHTSTYLE2\n"
"uniform sampler2D LightmapTexture2;\n"
"uniform float DeluxemapOffset2;\n"
"uniform myhalf3 lsColor2;\n"
"\n"
"#ifdef APPLY_LIGHTSTYLE3\n"
"uniform sampler2D LightmapTexture3;\n"
"uniform float DeluxemapOffset3;\n"
"uniform myhalf3 lsColor3;\n"
"\n"
"#endif\n"
"#endif\n"
"#endif\n"
"#endif\n"
"\n"
"uniform sampler2D BaseTexture;\n"
"uniform sampler2D NormalmapTexture;\n"
"uniform sampler2D GlossTexture;\n"
"#ifdef APPLY_DECAL\n"
"uniform sampler2D DecalTexture;\n"
"#endif\n"
"\n"
"#if defined(APPLY_OFFSETMAPPING) || defined(APPLY_RELIEFMAPPING)\n"
"uniform float OffsetMappingScale;\n"
"#endif\n"
"\n"
"uniform myhalf3 LightAmbient;\n"
"#ifdef APPLY_DIRECTIONAL_LIGHT\n"
"uniform myhalf3 LightDiffuse;\n"
"#endif\n"
"\n"
"uniform myhalf GlossIntensity; // gloss scaling factor\n"
"uniform myhalf GlossExponent; // gloss exponent factor\n"
"\n"
"#if defined(APPLY_OFFSETMAPPING) || defined(APPLY_RELIEFMAPPING)\n"
"// The following reliefmapping and offsetmapping routine was taken from DarkPlaces\n"
"// The credit goes to LordHavoc (as always)\n"
"vec2 OffsetMapping(vec2 TexCoord)\n"
"{\n"
"#ifdef APPLY_RELIEFMAPPING\n"
"// 14 sample relief mapping: linear search and then binary search\n"
"// this basically steps forward a small amount repeatedly until it finds\n"
"// itself inside solid, then jitters forward and back using decreasing\n"
"// amounts to find the impact\n"
"//vec3 OffsetVector = vec3(EyeVector.xy * ((1.0 / EyeVector.z) * OffsetMappingScale) * vec2(-1, 1), -1);\n"
"//vec3 OffsetVector = vec3(normalize(EyeVector.xy) * OffsetMappingScale * vec2(-1, 1), -1);\n"
"vec3 OffsetVector = vec3(normalize(EyeVector).xy * OffsetMappingScale * vec2(-1, 1), -1);\n"
"vec3 RT = vec3(TexCoord, 1);\n"
"OffsetVector *= 0.1;\n"
"RT += OffsetVector *  step(texture2D(NormalmapTexture, RT.xy).a, RT.z);\n"
"RT += OffsetVector *  step(texture2D(NormalmapTexture, RT.xy).a, RT.z);\n"
"RT += OffsetVector *  step(texture2D(NormalmapTexture, RT.xy).a, RT.z);\n"
"RT += OffsetVector *  step(texture2D(NormalmapTexture, RT.xy).a, RT.z);\n"
"RT += OffsetVector *  step(texture2D(NormalmapTexture, RT.xy).a, RT.z);\n"
"RT += OffsetVector *  step(texture2D(NormalmapTexture, RT.xy).a, RT.z);\n"
"RT += OffsetVector *  step(texture2D(NormalmapTexture, RT.xy).a, RT.z);\n"
"RT += OffsetVector *  step(texture2D(NormalmapTexture, RT.xy).a, RT.z);\n"
"RT += OffsetVector *  step(texture2D(NormalmapTexture, RT.xy).a, RT.z);\n"
"RT += OffsetVector * (step(texture2D(NormalmapTexture, RT.xy).a, RT.z)          - 0.5);\n"
"RT += OffsetVector * (step(texture2D(NormalmapTexture, RT.xy).a, RT.z) * 0.5    - 0.25);\n"
"RT += OffsetVector * (step(texture2D(NormalmapTexture, RT.xy).a, RT.z) * 0.25   - 0.125);\n"
"RT += OffsetVector * (step(texture2D(NormalmapTexture, RT.xy).a, RT.z) * 0.125  - 0.0625);\n"
"RT += OffsetVector * (step(texture2D(NormalmapTexture, RT.xy).a, RT.z) * 0.0625 - 0.03125);\n"
"return RT.xy;\n"
"#else\n"
"// 2 sample offset mapping (only 2 samples because of ATI Radeon 9500-9800/X300 limits)\n"
"// this basically moves forward the full distance, and then backs up based\n"
"// on height of samples\n"
"//vec2 OffsetVector = vec2(EyeVector.xy * ((1.0 / EyeVector.z) * OffsetMappingScale) * vec2(-1, 1));\n"
"//vec2 OffsetVector = vec2(normalize(EyeVector.xy) * OffsetMappingScale * vec2(-1, 1));\n"
"vec2 OffsetVector = vec2(normalize(EyeVector).xy * OffsetMappingScale * vec2(-1, 1));\n"
"TexCoord += OffsetVector;\n"
"OffsetVector *= 0.5;\n"
"TexCoord -= OffsetVector * texture2D(NormalmapTexture, TexCoord).a;\n"
"TexCoord -= OffsetVector * texture2D(NormalmapTexture, TexCoord).a;\n"
"return TexCoord;\n"
"#endif\n"
"}\n"
"#endif\n"
"\n"
"void main()\n"
"{\n"
"#if defined(APPLY_OFFSETMAPPING) || defined(APPLY_RELIEFMAPPING)\n"
"// apply offsetmapping\n"
"vec2 TexCoordOffset = OffsetMapping(TexCoord);\n"
"#define TexCoord TexCoordOffset\n"
"#endif\n"
"myhalf3 surfaceNormal;\n"
"myhalf3 diffuseNormalModelspace;\n"
"myhalf3 diffuseNormal = myhalf3 (0.0, 0.0, -1.0);\n"
"float diffuseProduct;\n"
"\n"
"myhalf3 weightedDiffuseNormal;\n"
"myhalf3 specularNormal;\n"
"float specularProduct;\n"
"\n"
"#if !defined(APPLY_DIRECTIONAL_LIGHT) && !defined(APPLY_LIGHTSTYLE0)\n"
"myhalf4 color = myhalf4 (1.0, 1.0, 1.0, 1.0);\n"
"#else\n"
"myhalf4 color = myhalf4 (0.0, 0.0, 0.0, 1.0);\n"
"#endif\n"
"\n"
"// get the surface normal\n"
"surfaceNormal = normalize (myhalf3 (texture2D (NormalmapTexture, TexCoord)) - myhalf3 (0.5));\n"
"\n"
"#ifdef APPLY_DIRECTIONAL_LIGHT\n"
"diffuseNormal = myhalf3 (LightVector);\n"
"weightedDiffuseNormal = diffuseNormal;\n"
"diffuseProduct = float (dot (surfaceNormal, diffuseNormal));\n"
"color.rgb += LightDiffuse.rgb * myhalf(max (diffuseProduct, 0.0)) + LightAmbient.rgb;\n"
"#endif\n"
"\n"
"// deluxemapping using light vectors in modelspace\n"
"\n"
"#ifdef APPLY_LIGHTSTYLE0\n"
"\n"
"// get light normal\n"
"diffuseNormalModelspace = myhalf3 (texture2D(LightmapTexture0, vec2(LightmapTexCoord01.s+DeluxemapOffset0,LightmapTexCoord01.t)).rgb) - myhalf3 (0.5);\n"
"diffuseNormal = normalize (myhalf3(dot(diffuseNormalModelspace,myhalf3(strMatrix[0])),dot(diffuseNormalModelspace,myhalf3(strMatrix[1])),dot(diffuseNormalModelspace,myhalf3(strMatrix[2]))));\n"
"// calculate directional shading\n"
"diffuseProduct = float (dot (surfaceNormal, diffuseNormal));\n"
"\n"
"#ifdef APPLY_FBLIGHTMAP\n"
"weightedDiffuseNormal = diffuseNormal;\n"
"// apply lightmap color\n"
"color.rgb += myhalf3 (max (diffuseProduct, 0.0) * texture2D (LightmapTexture0, LightmapTexCoord01.st).rgb);\n"
"#else\n"
"\n"
"#define NORMALIZE_DIFFUSE_NORMAL\n"
"\n"
"weightedDiffuseNormal = lsColor0.rgb * diffuseNormal;\n"
"// apply lightmap color\n"
"color.rgb += lsColor0 * myhalf(max (diffuseProduct, 0.0)) * texture2D (LightmapTexture0, LightmapTexCoord01.st).rgb;\n"
"#endif\n"
"\n"
"#ifdef APPLY_AMBIENT_COMPENSATION\n"
"// compensate for ambient lighting\n"
"color.rgb += myhalf((1.0 - max (diffuseProduct, 0.0))) * LightAmbient;\n"
"#endif\n"
"\n"
"#ifdef APPLY_LIGHTSTYLE1\n"
"diffuseNormalModelspace = myhalf3 (texture2D (LightmapTexture1, vec2(LightmapTexCoord01.p+DeluxemapOffset1,LightmapTexCoord01.q))) - myhalf3 (0.5);\n"
"diffuseNormal = normalize (myhalf3(dot(diffuseNormalModelspace,myhalf3(strMatrix[0])),dot(diffuseNormalModelspace,myhalf3(strMatrix[1])),dot(diffuseNormalModelspace,myhalf3(strMatrix[2]))));\n"
"diffuseProduct = float (dot (surfaceNormal, diffuseNormal));\n"
"weightedDiffuseNormal += lsColor1.rgb * diffuseNormal;\n"
"color.rgb += lsColor1 * myhalf(max (diffuseProduct, 0.0)) * texture2D (LightmapTexture1, LightmapTexCoord01.pq).rgb;\n"
"\n"
"#ifdef APPLY_LIGHTSTYLE2\n"
"diffuseNormalModelspace = myhalf3 (texture2D (LightmapTexture2, vec2(LightmapTexCoord23.s+DeluxemapOffset2,LightmapTexCoord23.t))) - myhalf3 (0.5);\n"
"diffuseNormal = normalize (myhalf3(dot(diffuseNormalModelspace,myhalf3(strMatrix[0])),dot(diffuseNormalModelspace,myhalf3(strMatrix[1])),dot(diffuseNormalModelspace,myhalf3(strMatrix[2]))));\n"
"diffuseProduct = float (dot (surfaceNormal, diffuseNormal));\n"
"weightedDiffuseNormal += lsColor2.rgb * diffuseNormal;\n"
"color.rgb += lsColor2 * myhalf(max (diffuseProduct, 0.0)) * texture2D (LightmapTexture2, LightmapTexCoord23.st).rgb;\n"
"\n"
"#ifdef APPLY_LIGHTSTYLE3\n"
"diffuseNormalModelspace = myhalf3 (texture2D (LightmapTexture3, vec2(LightmapTexCoord23.p+DeluxemapOffset3,LightmapTexCoord23.q))) - myhalf3 (0.5);;\n"
"diffuseNormal = normalize (myhalf3(dot(diffuseNormalModelspace,myhalf3(strMatrix[0])),dot(diffuseNormalModelspace,myhalf3(strMatrix[1])),dot(diffuseNormalModelspace,myhalf3(strMatrix[2]))));\n"
"diffuseProduct = float (dot (surfaceNormal, diffuseNormal));\n"
"weightedDiffuseNormal += lsColor3.rgb * diffuseNormal;\n"
"color.rgb += lsColor3 * myhalf(max (diffuseProduct, 0.0)) * texture2D (LightmapTexture3, LightmapTexCoord23.pq).rgb;\n"
"\n"
"#endif\n"
"#endif\n"
"#endif\n"
"#endif\n"
"\n"
"#ifdef APPLY_SPECULAR\n"
"\n"
"#ifdef NORMALIZE_DIFFUSE_NORMAL\n"
"specularNormal = normalize (myhalf3 (normalize (weightedDiffuseNormal)) + myhalf3 (normalize (EyeVector)));\n"
"#else\n"
"specularNormal = normalize (weightedDiffuseNormal + myhalf3 (normalize (EyeVector)));\n"
"#endif\n"
"\n"
"specularProduct = float (dot (surfaceNormal, specularNormal));\n"
"color.rgb += (myhalf3(texture2D(GlossTexture, TexCoord).rgb) * GlossIntensity) * pow(myhalf(max(specularProduct, 0.0)), GlossExponent);\n"
"#endif\n"
"\n"
"#ifdef APPLY_BASETEX_ALPHA_ONLY\n"
"color = min(color, myhalf4(texture2D(BaseTexture, TexCoord).a));\n"
"#else\n"
"color = min(color, myhalf4(1.0)) * myhalf4(texture2D(BaseTexture, TexCoord).rgba);\n"
"#endif\n"
"\n"
"#ifdef APPLY_DECAL\n"
"myhalf4 decal = myhalf4(gl_Color.rgba);\n"
"#ifdef APPLY_BRANCHING\n"
"if (decal.a > 0.0)\n"
"{\n"
"#endif\n"
"decal = decal * myhalf4(texture2D(DecalTexture, TexCoord).rgba);\n"
"color.rgb = decal.rgb * decal.a + color.rgb * (1.0-decal.a);\n"
"#ifdef APPLY_BRANCHING\n"
"}\n"
"#endif\n"
"#else\n"
"color = color * myhalf4(gl_Color.rgba);\n"
"#endif\n"
"\n"
"gl_FragColor = vec4(color);\n"
"}\n"
"\n"
"#endif // FRAGMENT_SHADER\n"
"\n";

static const char *r_defaultDistortionProgram =
"// \" APPLICATION \" GLSL shader\n"
"\n"
"#if !defined(__GLSL_CG_DATA_TYPES) || defined(APPLY_NO_HALF_TYPES)\n"
"#define myhalf float\n"
"#define myhalf2 vec2\n"
"#define myhalf3 vec3\n"
"#define myhalf4 vec4\n"
"#else\n"
"#define myhalf half\n"
"#define myhalf2 half2\n"
"#define myhalf3 half3\n"
"#define myhalf4 half4\n"
"#endif\n"
"\n"
"varying vec4 TexCoord;\n"
"varying vec4 ProjVector;\n"
"#ifdef APPLY_EYEDOT\n"
"varying vec3 EyeVector;\n"
"#endif\n"
"\n"
"#ifdef VERTEX_SHADER\n"
"// Vertex shader\n"
"\n"
"#ifdef APPLY_EYEDOT\n"
"uniform vec3 EyeOrigin;\n"
"uniform float FrontPlane;\n"
"#endif\n"
"\n"
"void main(void)\n"
"{\n"
"gl_FrontColor = gl_Color;\n"
"\n"
"mat4 textureMatrix;\n"
"\n"
"textureMatrix = gl_TextureMatrix[0];\n"
"TexCoord.st = vec2 (textureMatrix * gl_MultiTexCoord0);\n"
"\n"
"textureMatrix = gl_TextureMatrix[0];\n"
"textureMatrix[0] = -textureMatrix[0];\n"
"textureMatrix[1] = -textureMatrix[1];\n"
"TexCoord.pq = vec2 (textureMatrix * gl_MultiTexCoord0);\n"
"\n"
"#ifdef APPLY_EYEDOT\n"
"mat3 strMatrix;\n"
"strMatrix[0] = gl_MultiTexCoord1.xyz;\n"
"strMatrix[2] = gl_Normal.xyz;\n"
"strMatrix[1] = gl_MultiTexCoord1.w * cross (strMatrix[2], strMatrix[0]);\n"
"\n"
"vec3 EyeVectorWorld = (EyeOrigin - gl_Vertex.xyz) * FrontPlane;\n"
"EyeVector = EyeVectorWorld * strMatrix;\n"
"#endif\n"
"\n"
"gl_Position = ftransform();\n"
"ProjVector = gl_Position;\n"
"#ifdef APPLY_CLIPPING\n"
"#ifdef __GLSL_CG_DATA_TYPES\n"
"gl_ClipVertex = gl_ModelViewMatrix * gl_Vertex;\n"
"#endif\n"
"#endif\n"
"}\n"
"\n"
"#endif // VERTEX_SHADER\n"
"\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"// Fragment shader\n"
"\n"
"uniform sampler2D DuDvMapTexture;\n"
"#ifdef APPLY_EYEDOT\n"
"uniform sampler2D NormalmapTexture;\n"
"#endif\n"
"uniform sampler2D ReflectionTexture;\n"
"uniform sampler2D RefractionTexture;\n"
"uniform float TextureWidth, TextureHeight;\n"
"\n"
"void main(void)\n"
"{\n"
"myhalf3 color;\n"
"\n"
"vec3 displacement = vec3(texture2D(DuDvMapTexture, vec2(TexCoord.pq) * vec2(0.25)));\n"
"vec2 coord = vec2(TexCoord.st) + vec2(displacement) * vec2 (0.2);\n"
"\n"
"vec3 fdist = vec3 (normalize(vec3(texture2D(DuDvMapTexture, coord)) - vec3 (0.5))) * vec3(0.005);\n"
"\n"
"// get projective texcoords\n"
"float scale = float(1.0 / float(ProjVector.w));\n"
"float inv2NW = 1.0 / (2.0 * float (TextureWidth));\n"
"float inv2NH = 1.0 / (2.0 * float (TextureHeight));\n"
"vec3 projCoord = (vec3(ProjVector.xyz) * scale + vec3 (1.0)) * vec3 (0.5) + fdist;\n"
"projCoord.s = float (clamp (float(projCoord.s), inv2NW, 1.0 - inv2NW));\n"
"projCoord.t = float (clamp (float(projCoord.t), inv2NH, 1.0 - inv2NH));\n"
"projCoord.r = float (clamp (float(projCoord.r), 0.0, 1.0));\n"
"\n"
"#ifdef APPLY_EYEDOT\n"
"// calculate dot product between the surface normal and eye vector\n"
"// great for simulating varying water translucency based on the view angle\n"
"myhalf3 surfaceNormal = normalize(myhalf3(texture2D(NormalmapTexture, coord).rgb) - myhalf3 (0.5));\n"
"vec3 eyeNormal = normalize(myhalf3(EyeVector));\n"
"\n"
"float refrdot = float(dot(surfaceNormal, eyeNormal));\n"
"//refrdot = float (clamp (refrdot, 0.0, 1.0));\n"
"float refldot = 1.0 - refrdot;\n"
"// get refraction and reflection\n"
"myhalf3 refr = (myhalf3(texture2D(RefractionTexture, vec2(projCoord.st))).rgb) * refrdot;\n"
"myhalf3 refl = (myhalf3(texture2D(ReflectionTexture, vec2(projCoord.st))).rgb) * refldot;\n"
"#else\n"
"myhalf3 refr = (myhalf3(texture2D(RefractionTexture, vec2(projCoord.st))).rgb);\n"
"myhalf3 refl = (myhalf3(texture2D(ReflectionTexture, vec2(projCoord.st))).rgb);\n"
"#endif\n"
"\n"
"\n"
"// add reflection and refraction\n"
"#ifdef APPLY_DISTORTION_ALPHA\n"
"color = myhalf3(gl_Color.rgb) + myhalf3(mix (refr, refl, float(gl_Color.a)));\n"
"#else\n"
"color = myhalf3(gl_Color.rgb) + refr + refl;\n"
"#endif\n"
"\n"
"gl_FragColor = vec4(vec3(color), 1.0);\n"
"}\n"
"\n"
"#endif // FRAGMENT_SHADER\n"
"\n";

static const char *r_defaultShadowmapProgram =
"// \" APPLICATION \" GLSL shader\n"
"\n"
"\n"
"varying vec4 ProjVector;\n"
"\n"
"#ifdef VERTEX_SHADER\n"
"// Vertex shader\n"
"\n"
"void main(void)\n"
"{\n"
"gl_FrontColor = gl_Color;\n"
"\n"
"\n"
"mat4 textureMatrix;\n"
"\n"
"textureMatrix = gl_TextureMatrix[0];\n"
"\n"
"gl_Position = ftransform();\n"
"ProjVector = textureMatrix * gl_Vertex;\n"
"}\n"
"\n"
"#endif // VERTEX_SHADER\n"
"\n"
"\n"
"#ifdef FRAGMENT_SHADER\n"
"// Fragment shader\n"
"\n"
"uniform float TextureWidth, TextureHeight;\n"
"uniform float ProjDistance;\n"
"uniform sampler2DShadow ShadowmapTexture;\n"
"\n"
"void main(void)\n"
"{\n"
"vec4 color = vec4 (1.0);\n"
"\n"
"#ifdef APPLY_BRANCHING\n"
"\n"
"if (ProjVector.w > 0.0)\n"
"{\n"
"\n"
"if (ProjVector.w < ProjDistance)\n"
"{\n"
"#endif\n"
"\n"
"float dtW  = 1.0 / TextureWidth;\n"
"float dtH  = 1.0 / TextureHeight;\n"
"vec3 coord = vec3 (ProjVector.xyz / ProjVector.w);\n"
"coord = (coord + vec3 (1.0)) * vec3 (0.5);\n"
"coord.s = float (clamp (float(coord.s), dtW, 1.0 - dtW));\n"
"coord.t = float (clamp (float(coord.t), dtH, 1.0 - dtH));\n"
"coord.r = float (clamp (float(coord.r), 0.0, 1.0));\n"
"\n"
"float shadow0 = float(shadow2D(ShadowmapTexture, coord).r);\n"
"float shadow = shadow0;\n"
"\n"
"#if defined(APPLY_PCF2x2) || defined(APPLY_PCF3x3)\n"
"\n"
"vec3 coord2 = coord + vec3(0.0, dtH, 0.0);\n"
"float shadow1 = float (shadow2D (ShadowmapTexture, coord2).r);\n"
"\n"
"coord2 = coord + vec3(dtW, dtH, 0.0);\n"
"float shadow2 = float (shadow2D (ShadowmapTexture, coord2).r);\n"
"\n"
"coord2 = coord + vec3(dtW, 0.0, 0.0);\n"
"float shadow3 = float (shadow2D (ShadowmapTexture, coord2).r);\n"
"\n"
"#if defined(APPLY_PCF3x3)\n"
"coord2 = coord + vec3(-dtW, 0.0, 0.0);\n"
"float shadow4 = float (shadow2D (ShadowmapTexture, coord2).r);\n"
"\n"
"coord2 = coord + vec3(-dtW, -dtH, 0.0);\n"
"float shadow5 = float (shadow2D (ShadowmapTexture, coord2).r);\n"
"\n"
"coord2 = coord + vec3(0.0, -dtH, 0.0);\n"
"float shadow6 = float (shadow2D (ShadowmapTexture, coord2).r);\n"
"\n"
"coord2 = coord + vec3(dtW, -dtH, 0.0);\n"
"float shadow7 = float (shadow2D (ShadowmapTexture, coord2).r);\n"
"\n"
"coord2 = coord + vec3(-dtW, dtH, 0.0);\n"
"float shadow8 = float (shadow2D (ShadowmapTexture, coord2).r);\n"
"\n"
"shadow = (shadow0 + shadow1 + shadow2 + shadow3 + shadow4 + shadow5 + shadow6 + shadow7 + shadow8) * 0.11;\n"
"#else\n"
"shadow = (shadow0 + shadow1 + shadow2 + shadow3) * 0.25;\n"
"#endif\n"
"#else\n"
"shadow = shadow0;\n"
"#endif\n"
"\n"
"float attenuation = float (ProjVector.w) / ProjDistance;\n"
"#ifdef APPLY_BRANCHING\n"
"shadow = shadow + attenuation;\n"
"#else\n"
"shadow = shadow + 1.0 - step (0.0, float(ProjVector.w)) + max(attenuation, 0.0);\n"
"#endif\n"
"color.rgb = vec3 (shadow);\n"
"\n"
"#ifdef APPLY_BRANCHING\n"
"}\n"
"}\n"
"#endif\n"
"\n"
"gl_FragColor = vec4(color);\n"
"}\n"
"\n"
"#endif // FRAGMENT_SHADER\n"
"\n";

/*
================
R_ProgramFeatures2Defines

Return an array of strings for bitflags
================
*/
static const char **R_ProgramFeatures2Defines( int features, char *name, size_t size )
{
	int		i, p;
	static const char	*headers[MAX_DEFINES_FEATURES+1]; // +1 for NULL safe-guard

	for( i = 0, p = 0; i < sizeof( prog_features ) / sizeof( prog_features[0] ); i++ )
	{
		if( features & prog_features[i].bit )
		{
			headers[p++] = prog_features[i].define;
			if( name ) com.strncat( name, prog_features[i].suffix, size );
			if( p == MAX_DEFINES_FEATURES ) break;
		}
	}

	if( p )
	{
		headers[p] = NULL;
		return headers;
	}

	return NULL;
}

/*
================
R_RegisterProgram
================
*/
int R_RegisterProgram( const char *name, const char *progstring, uint features )
{
	uint		minfeatures;
	program_t		*program, *parent;
	string		fullName;
	const char	**header;
	int		i, linked, body, error = 0;
	const char	*vertexShaderStrings[MAX_DEFINES_FEATURES+2];
	const char	*fragmentShaderStrings[MAX_DEFINES_FEATURES+2];

	if( !GL_Support( R_SHADER_GLSL100_EXT )) return 0; // fail early

	parent = NULL;
	minfeatures = features;
	for( i = 0, program = r_programs; i < MAX_PROGRAMS; i++, program++ )
	{
		if( !program->name ) break;

		if( !com.stricmp( program->name, name ))
		{
			if ( program->features == features )
				return ( i+1 );

			if( !parent || (program->features < minfeatures) )
			{
				parent = program;
				minfeatures = program->features;
			}
		}
	}

	if( i == MAX_PROGRAMS )
	{
		MsgDev( D_ERROR, "R_RegisterProgram: GLSL programs limit exceeded\n" );
		return 0;
	}

	if( !progstring )
	{
		if( parent ) progstring = parent->string;
		else progstring = r_defaultProgram;
	}

	program = r_programs + i;
	program->object = pglCreateProgramObjectARB();
	if( !program->object )
	{
		error = 1;
		goto done;
	}

	body = 1;
	com.strncpy( fullName, name, sizeof( fullName ));
	header = R_ProgramFeatures2Defines( features, fullName, sizeof( fullName ));

	if( header ) for( ; header[body-1] && *header[body-1]; body++ );

	vertexShaderStrings[0] = "#define VERTEX_SHADER\n";
	for( i = 1; i < body; i++ )
		vertexShaderStrings[i] = (char *)header[i-1];
	// find vertex shader header
	vertexShaderStrings[body] = progstring;

	fragmentShaderStrings[0] = "#define FRAGMENT_SHADER\n";
	for( i = 1; i < body; i++ )
		fragmentShaderStrings[i] = (char *)header[i-1];
	// find fragment shader header
	fragmentShaderStrings[body] = progstring;

	// compile vertex shader
	program->vertexShader = R_CompileShader( program->object, fullName, "vertex", GL_VERTEX_SHADER_ARB, vertexShaderStrings, body+1 );
	if( !program->vertexShader )
	{
		error = 1;
		goto done;
	}

	// compile fragment shader
	program->fragmentShader = R_CompileShader( program->object, fullName, "fragment", GL_FRAGMENT_SHADER_ARB, fragmentShaderStrings, body+1 );
	if( !program->fragmentShader )
	{
		error = 1;
		goto done;
	}

	// link
	pglLinkProgramARB( program->object );
	pglGetObjectParameterivARB( program->object, GL_OBJECT_LINK_STATUS_ARB, &linked );
	if( !linked )
	{
		char	log[8192];

		pglGetInfoLogARB( program->object, sizeof( log ), NULL, log );
		log[sizeof( log ) - 1] = 0;

		if( log[0] ) MsgDev( D_WARN, "Failed to compile link object for program %s:\n%s\n", fullName, log );

		error = 1;
		goto done;
	}

done:
	if( error ) R_DeleteProgram( program );

	program->features = features;
	program->name = R_ProgramCopyString( name );
	program->string = progstring;

	if( program->object )
	{
		pglUseProgramObjectARB( program->object );
		R_GetProgramUniformLocations( program );
		pglUseProgramObjectARB( 0 );
	}
	return ( program - r_programs ) + 1;
}

/*
================
R_GetProgramObject
================
*/
int R_GetProgramObject( int elem )
{
	return r_programs[elem - 1].object;
}

/*
================
R_ProgramList_f
================
*/
void R_ProgramList_f( void )
{
	int		i;
	program_t		*program;
	string		fullName;
	const char	**header;

	Msg( "------------------\n" );
	for( i = 0, program = r_programs; i < MAX_PROGRAMS; i++, program++ )
	{
		if( !program->name ) break;
		com.strncpy( fullName, program->name, sizeof( fullName ));
		header = R_ProgramFeatures2Defines( program->features, fullName, sizeof( fullName ));

		Msg( " %3i %s\n", i+1, fullName );
	}
	Msg( "%i programs total\n", i );
}

/*
================
R_ProgramDump_f
================
*/
#define DUMP_PROGRAM( p )\
{\
	file_t *file = FS_Open( va("programs/%s.txt", #p), "wb" );\
	if( file ) \
	{\
		FS_Print( file, r_##p );\
		FS_Close( file );\
	}\
}\

void R_ProgramDump_f( void )
{
	DUMP_PROGRAM( defaultProgram );
	DUMP_PROGRAM( defaultDistortionProgram );
	DUMP_PROGRAM( defaultShadowmapProgram );
}
#undef DUMP_PROGRAM

/*
================
R_UpdateProgramUniforms
================
*/
void R_UpdateProgramUniforms( int elem, vec3_t eyeOrigin, vec3_t lightOrigin, vec3_t lightDir, vec4_t ambient, vec4_t diffuse, superLightStyle_t *superLightStyle, bool frontPlane, int TexWidth, int TexHeight, float projDistance, float offsetmappingScale )
{
	program_t	*program = r_programs + elem - 1;

	if( program->locEyeOrigin >= 0 && eyeOrigin )
		pglUniform3fARB( program->locEyeOrigin, eyeOrigin[0], eyeOrigin[1], eyeOrigin[2] );

	if( program->locLightOrigin >= 0 && lightOrigin )
		pglUniform3fARB( program->locLightOrigin, lightOrigin[0], lightOrigin[1], lightOrigin[2] );
	if( program->locLightDir >= 0 && lightDir )
		pglUniform3fARB( program->locLightDir, lightDir[0], lightDir[1], lightDir[2] );

	if( program->locLightAmbient >= 0 && ambient )
		pglUniform3fARB( program->locLightAmbient, ambient[0], ambient[1], ambient[2] );
	if( program->locLightDiffuse >= 0 && diffuse )
		pglUniform3fARB( program->locLightDiffuse, diffuse[0], diffuse[1], diffuse[2] );

	if( program->locGlossIntensity >= 0 )
		pglUniform1fARB( program->locGlossIntensity, r_lighting_glossintensity->value );
	if( program->locGlossExponent >= 0 )
		pglUniform1fARB( program->locGlossExponent, r_lighting_glossexponent->value );

	if( program->locOffsetMappingScale >= 0 )
		pglUniform1fARB( program->locOffsetMappingScale, offsetmappingScale );

	if( program->locFrontPlane >= 0 )
		pglUniform1fARB( program->locFrontPlane, frontPlane ? 1 : -1 );

	if( program->locTextureWidth >= 0 )
		pglUniform1fARB( program->locTextureWidth, TexWidth );
	if( program->locTextureHeight >= 0 )
		pglUniform1fARB( program->locTextureHeight, TexHeight );

	if( program->locProjDistance >= 0 )
		pglUniform1fARB( program->locProjDistance, projDistance );

	if( superLightStyle )
	{
		int	i;

		for( i = 0; i < LM_STYLES && superLightStyle->lStyles[i] != 255; i++ )
		{
			vec_t	*rgb = r_lightStyles[superLightStyle->lStyles[i]].rgb;

			if( program->locDeluxemapOffset[i] >= 0 )
				pglUniform1fARB( program->locDeluxemapOffset[i], superLightStyle->stOffset[i][0] );
			if( program->loclsColor[i] >= 0 )
				pglUniform3fARB( program->loclsColor[i], rgb[0], rgb[1], rgb[2] );
		}

		for( ; i < MAX_LIGHTMAPS; i++ )
		{
			if( program->loclsColor[i] >= 0 )
				pglUniform3fARB( program->loclsColor[i], 0, 0, 0 );
		}
	}
}

/*
================
R_GetProgramUniformLocations
================
*/
static void R_GetProgramUniformLocations( program_t *program )
{
	int	i;
	int	locBaseTexture;
	int	locNormalmapTexture;
	int	locGlossTexture;
	int	locDecalTexture;
	int	locLightmapTexture[LM_STYLES];
	int	locDuDvMapTexture;
	int	locReflectionTexture;
	int	locRefractionTexture;
	int	locShadowmapTexture;
	string	uniformName;

	program->locEyeOrigin = pglGetUniformLocationARB( program->object, "EyeOrigin" );
	program->locLightDir = pglGetUniformLocationARB( program->object, "LightDir" );
	program->locLightOrigin = pglGetUniformLocationARB( program->object, "LightOrigin" );
	program->locLightAmbient = pglGetUniformLocationARB( program->object, "LightAmbient" );
	program->locLightDiffuse = pglGetUniformLocationARB( program->object, "LightDiffuse" );

	locBaseTexture = pglGetUniformLocationARB( program->object, "BaseTexture" );
	locNormalmapTexture = pglGetUniformLocationARB( program->object, "NormalmapTexture" );
	locGlossTexture = pglGetUniformLocationARB( program->object, "GlossTexture" );
	locDecalTexture = pglGetUniformLocationARB( program->object, "DecalTexture" );

	locDuDvMapTexture = pglGetUniformLocationARB( program->object, "DuDvMapTexture" );
	locReflectionTexture = pglGetUniformLocationARB( program->object, "ReflectionTexture" );
	locRefractionTexture = pglGetUniformLocationARB( program->object, "RefractionTexture" );
	locShadowmapTexture = pglGetUniformLocationARB( program->object, "ShadowmapTexture" );

	for( i = 0; i < LM_STYLES; i++ )
	{
		com.snprintf( uniformName, sizeof( uniformName ), "LightmapTexture%i", i );
		locLightmapTexture[i] = pglGetUniformLocationARB( program->object, uniformName );

		com.snprintf( uniformName, sizeof( uniformName ), "DeluxemapOffset%i", i );
		program->locDeluxemapOffset[i] = pglGetUniformLocationARB( program->object, uniformName );

		com.snprintf( uniformName, sizeof( uniformName ), "lsColor%i", i );
		program->loclsColor[i] = pglGetUniformLocationARB( program->object, uniformName );
	}

	program->locGlossIntensity = pglGetUniformLocationARB( program->object, "GlossIntensity" );
	program->locGlossExponent = pglGetUniformLocationARB( program->object, "GlossExponent" );

	program->locOffsetMappingScale = pglGetUniformLocationARB( program->object, "OffsetMappingScale" );
	program->locFrontPlane = pglGetUniformLocationARB( program->object, "FrontPlane" );

	program->locTextureWidth = pglGetUniformLocationARB( program->object, "TextureWidth" );
	program->locTextureHeight = pglGetUniformLocationARB( program->object, "TextureHeight" );

	program->locProjDistance = pglGetUniformLocationARB( program->object, "ProjDistance" );

	if( locBaseTexture >= 0 ) pglUniform1iARB( locBaseTexture, 0 );
	if( locDuDvMapTexture >= 0 ) pglUniform1iARB( locDuDvMapTexture, 0 );

	if( locNormalmapTexture >= 0 ) pglUniform1iARB( locNormalmapTexture, 1 );
	if( locGlossTexture >= 0 ) pglUniform1iARB( locGlossTexture, 2 );
	if( locDecalTexture >= 0 ) pglUniform1iARB( locDecalTexture, 3 );

	if( locReflectionTexture >= 0 ) pglUniform1iARB( locReflectionTexture, 2 );
	if( locRefractionTexture >= 0 ) pglUniform1iARB( locRefractionTexture, 3 );

	if( locShadowmapTexture >= 0 ) pglUniform1iARB( locShadowmapTexture, 0 );

	for( i = 0; i < LM_STYLES; i++ )
	{
		if( locLightmapTexture[i] >= 0 )
			pglUniform1iARB( locLightmapTexture[i], i+4 );
	}
}

/*
================
R_ShutdownPrograms
================
*/
void R_ShutdownPrograms( void )
{
	int	i;
	program_t	*program;

	if( !r_progpool ) return;
	if( !GL_Support( R_SHADER_GLSL100_EXT ))
		return;

	for( i = 0, program = r_programs; i < MAX_PROGRAMS; i++, program++ )
	{
		if( !program->object ) break;
		R_DeleteProgram( program );
	}

	Mem_FreePool( &r_progpool );
}