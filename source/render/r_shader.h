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
#ifndef __R_SHADER_H__
#define __R_SHADER_H__

#define MAX_SHADERS			4096
#define SHADERS_HASH_SIZE		256
#define MAX_STAGE_TEXTURES		256	// same as MAX_SPRITE_FRAMES
#define MAX_SHADER_STAGES		8
#define MAX_SHADER_DEFORMS		8
#define MAX_SHADER_TCMODS		8

#define MAX_TABLES			4096
#define TABLES_HASH_SIZE		1024

// shader types
#define SHADER_INVALID		-1
#define SHADER_UNKNOWN		0
#define SHADER_SKY			1
#define SHADER_FONT			2
#define SHADER_NOMIP		3
#define SHADER_GENERIC		4
#define SHADER_TEXTURE		5
#define SHADER_VERTEX		6
#define SHADER_FLARE		7
#define SHADER_ALIAS		8
#define SHADER_STUDIO		9
#define SHADER_SPRITE		10
#define SHADER_FARBOX		11
#define SHADER_NEARBOX		12
#define SHADER_PLANAR_SHADOW		13
#define SHADER_OPAQUE_OCCLUDER	14
#define SHADER_OUTLINE		15

// shader flags
typedef enum
{
	SHADER_STATIC		= BIT(0),	// never freed by R_ShaderFreeUnused
	SHADER_DEPTHWRITE		= BIT(1),
	SHADER_SKYPARMS		= BIT(2),
	SHADER_SURFACEPARM		= BIT(3),	// shader has surface and contents parms
	SHADER_POLYGONOFFSET	= BIT(4),
	SHADER_CULL_FRONT		= BIT(5),
	SHADER_CULL_BACK		= BIT(6),
	SHADER_VIDEOMAP		= BIT(7),
	SHADER_MATERIAL		= BIT(8),
	SHADER_DEFORM_NORMAL	= BIT(9),
	SHADER_ENTITY_MERGABLE	= BIT(10),
	SHADER_AUTOSPRITE		= BIT(11),
	SHADER_NO_MODULATIVE_DLIGHTS	= BIT(12),
	SHADER_HASLIGHTMAP		= BIT(13),
	SHADER_PORTAL		= BIT(14),
	SHADER_PORTAL_CAPTURE1	= BIT(15),
	SHADER_PORTAL_CAPTURE2	= BIT(16),
	SHADER_RENDERMODE		= BIT(17),
	SHADER_PORTAL_CAPTURE	= (SHADER_PORTAL_CAPTURE1|SHADER_PORTAL_CAPTURE1),
	SHADER_CULL		= (SHADER_CULL_FRONT|SHADER_CULL_BACK)
} shaderFlags_t;

// shaderstage flags
enum
{
	SHADERSTAGE_ANIMFREQUENCY	= BIT(0),		// auto-animate value
	SHADERSTAGE_FRAMES		= BIT(1),		// bundle have anim frames, thats can be switching manually
	SHADERSTAGE_ANGLEDMAP	= BIT(2),		// doom1 eight angle-aligned ( 360 / 8 ) textures
	SHADERSTAGE_LIGHTMAP	= BIT(3),
	SHADERSTAGE_DETAIL		= BIT(4),
	SHADERSTAGE_NOCOLORARRAY	= BIT(5),
	SHADERSTAGE_DLIGHT		= BIT(6),
	SHADERSTAGE_PORTALMAP	= BIT(7),
	SHADERSTAGE_STENCILSHADOW	= BIT(8),
	SHADERSTAGE_RENDERMODE	= BIT(9),
	SHADERSTAGE_BLEND_REPLACE	= BIT(10),
	SHADERSTAGE_BLEND_MODULATE	= BIT(11),
	SHADERSTAGE_BLEND_ADD	= BIT(12),
	SHADERSTAGE_BLEND_DECAL	= BIT(13)
} stageFlags_t;

#define SHADERSTAGE_BLENDMODE		(SHADERSTAGE_BLEND_REPLACE|SHADERSTAGE_BLEND_MODULATE|SHADERSTAGE_BLEND_ADD|SHADERSTAGE_BLEND_DECAL)

// sorting
typedef enum
{
	SORT_NONE		= 0,
	SORT_PORTAL	= 1,
	SORT_SKY		= 2,
	SORT_OPAQUE	= 3,
	SORT_DECAL	= 4,
	SORT_ALPHATEST	= 5,
	SORT_BANNER	= 6,
	SORT_UNDERWATER	= 7,
	SORT_WATER	= 8,
	SORT_ADDITIVE	= 9,
	SORT_NEAREST	= 16
} sort_t;

// transform functions
typedef enum
{
	WAVEFORM_NONE = 0,
	WAVEFORM_SIN,
	WAVEFORM_TRIANGLE,
	WAVEFORM_SQUARE,
	WAVEFORM_SAWTOOTH,
	WAVEFORM_INVERSESAWTOOTH,
	WAVEFORM_NOISE,
	WAVEFORM_CONSTANT
} waveForm_t;

// RGB colors generation
typedef enum
{
	RGBGEN_UNKNOWN = 0,
	RGBGEN_IDENTITY,
	RGBGEN_IDENTITY_LIGHTING,
	RGBGEN_CONST,
	RGBGEN_WAVE,
	RGBGEN_COLORWAVE,
	RGBGEN_ENTITY,
	RGBGEN_ONE_MINUS_ENTITY,
	RGBGEN_VERTEX,
	RGBGEN_ONE_MINUS_VERTEX,
	RGBGEN_LIGHTING_DIFFUSE,
	RGBGEN_EXACT_VERTEX,
	RGBGEN_LIGHTING_DIFFUSE_ONLY,
	RGBGEN_LIGHTING_AMBIENT_ONLY,
	RGBGEN_CUSTOM,
	RGBGEN_FOG,		// followed extensions only for internal use
	RGBGEN_OUTLINE,
	RGBGEN_ENVIRONMENT
} rgbGenType_t;

// alpha channel generation
typedef enum
{
	ALPHAGEN_UNKNOWN = 0,
	ALPHAGEN_IDENTITY,
	ALPHAGEN_CONST,
	ALPHAGEN_PORTAL,
	ALPHAGEN_VERTEX,
	ALPHAGEN_ONE_MINUS_VERTEX,
	ALPHAGEN_ENTITY,
	ALPHAGEN_ONE_MINUS_ENTITY,
	ALPHAGEN_SPECULAR,
	ALPHAGEN_WAVE,
	ALPHAGEN_ALPHAWAVE,
	ALPHAGEN_FADE,			// same as portal but for other things
	ALPHAGEN_ONE_MINUS_FADE,
	ALPHAGEN_DOT,
	ALPHAGEN_ONE_MINUS_DOT,
	ALPHAGEN_OUTLINE			// only for internal use
} alphaGenType_t;

// texture coordinates generation
typedef enum
{
	TCGEN_NONE = 0,
	TCGEN_BASE,
	TCGEN_LIGHTMAP,
	TCGEN_ENVIRONMENT,
	TCGEN_VECTOR,
	TCGEN_LIGHTVECTOR,
	TCGEN_HALFANGLE,
	TCGEN_WARP,
	TCGEN_REFLECTION,
	TCGEN_FOG,
	TCGEN_REFLECTION_CELLSHADE,
	TCGEN_SVECTORS,
	TCGEN_PROJECTION,
	TCGEN_PROJECTION_SHADOW,
	TCGEN_NORMAL
} tcGenType_t;

// tcmod functions
typedef enum
{
	TCMOD_NONE = 0,
	TCMOD_TRANSLATE,
	TCMOD_SCALE,
	TCMOD_SCROLL,
	TCMOD_ROTATE,
	TCMOD_STRETCH,
	TCMOD_TRANSFORM,
	TCMOD_TURB,
	TCMOD_CONVEYOR		// same as TCMOD_SCROLL, but can be controlled by entity
} tcModType_t;

// vertices deformation
typedef enum
{
	DEFORM_NONE = 0,
	DEFORM_WAVE,
	DEFORM_NORMAL,
	DEFORM_BULGE,
	DEFORM_MOVE,
	DEFORM_AUTOSPRITE,
	DEFORM_AUTOSPRITE2,
	DEFORM_PROJECTION_SHADOW,
	DEFORM_AUTOPARTICLE,
	DEFORM_OUTLINE
} deformType_t;

typedef enum
{
	TABLE_SNAP	= BIT(0),
	TABLE_CLAMP	= BIT(1)
} tableFlags_t;

typedef struct table_s
{
	char		*name;		// table name
	int		index;
	tableFlags_t	flags;

	int		size;
	float		*values;		// float[]
	struct table_s	*nextHash;
} table_t;

typedef struct
{
	waveForm_t	type;		// SHADER_FUNC enum
	float		args[4];		// offset, amplitude, phase_offset, rate
} waveFunc_t;

typedef struct
{
	tcModType_t	type;
	float		args[6];
} tcMod_t;

typedef struct
{
	rgbGenType_t	type;
	float		*args;
	waveFunc_t	*func;
} rgbGen_t;

typedef struct
{
	alphaGenType_t	type;
	float		*args;
	waveFunc_t	*func;
} alphaGen_t;

typedef struct
{
	deformType_t	type;
	float		args[4];
	waveFunc_t	func;
} deform_t;

// Per-pass rendering state information
typedef struct ref_stage_s
{
	uint		flags;
	uint		glState;			// GLSTATE_ flags

	rgbGen_t		rgbGen;
	alphaGen_t	alphaGen;

	word		tcgen;
	vec_t		*tcgenVec;

	word		numtcMods;
	tcMod_t		*tcMods;

	uint		cinHandle;

	const char	*program;
	word		program_type;

	float		animFrequency;			// animation frames per sec
	word		num_textures;
	texture_t		*textures[MAX_STAGE_TEXTURES];	// texture refs
} ref_stage_t;

// Shader information
typedef struct ref_shader_s
{
	char		*name;
	word		type;

	shader_t		shadernum;	// 0 = free	
	uint		flags;
	word		features;
	sort_t		sort;
	sort_t		realsort;		// member original sort type until rendermode is reset
	uint		sortkey;
	uint		touchFrame;	// 0 = free

	word		num_stages;
	ref_stage_t	*stages;

	word		numDeforms;
	deform_t		*deforms;

	skydome_t		*skyParms;
	rgba_t		fog_color;
	float		fog_dist;
	float		fog_clearDist;

	float		offsetmapping_scale;

	struct ref_script_s	*cache;
	struct ref_shader_s	*nextHash;
} ref_shader_t;

// memory management
extern byte		*r_shaderpool;
extern ref_shader_t		r_shaders[MAX_SHADERS];
extern skydome_t		*r_skydomes[MAX_SHADERS];

#define Shader_CopyString( str )	com.stralloc( r_shaderpool, str, __FILE__, __LINE__ )
#define Shader_Sortkey( shader, sort )	((( sort )<<26 )|( shader - r_shaders ))
#define Shader_Malloc( size )		Mem_Alloc( r_shaderpool, size )
#define Shader_Free( data )		Mem_Free( data )


void R_InitShaders( void );
void R_ShutdownShaders( void );
void R_UploadCinematicShader( const ref_shader_t *shader );
void R_DeformvBBoxForShader( const ref_shader_t *shader, vec3_t ebbox );
void R_ShaderList_f( void );
ref_shader_t *R_LoadShader( const char *name, int type, bool forceDefault, int addFlags, int ignoreType );


#endif /*__R_SHADER_H__*/
