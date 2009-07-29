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
#define MAX_SHADER_STAGES		8
#define MAX_SHADER_DEFORMS		8
#define MAX_STAGE_TEXTURES		256
#define MAX_SHADER_TCMODS		8

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
#define SHADER_MD3			8
#define SHADER_FARBOX		9
#define SHADER_NEARBOX		10
#define SHADER_PLANAR_SHADOW		11
#define SHADER_OPAQUE_OCCLUDER	12
#ifdef HARDWARE_OUTLINES
#define SHADER_OUTLINE		13
#endif

// shader flags
enum
{
	SHADER_DEPTHWRITE		= BIT(0),
	SHADER_SKYPARMS		= BIT(1),
	SHADER_POLYGONOFFSET	= BIT(2),
	SHADER_CULL_FRONT		= BIT(3),
	SHADER_CULL_BACK		= BIT(4),
	SHADER_VIDEOMAP		= BIT(5),
	SHADER_MATERIAL		= BIT(6),
	SHADER_DEFORM_NORMAL	= BIT(7),
	SHADER_ENTITY_MERGABLE	= BIT(8),
	SHADER_AUTOSPRITE		= BIT(9),
	SHADER_NO_MODULATIVE_DLIGHTS	= BIT(10),
	SHADER_LIGHTMAP		= BIT(11),
	SHADER_PORTAL		= BIT(12),
	SHADER_PORTAL_CAPTURE1	= BIT(13),
	SHADER_PORTAL_CAPTURE2	= BIT(14),
	SHADER_PORTAL_CAPTURE	= SHADER_PORTAL_CAPTURE1|SHADER_PORTAL_CAPTURE1,
};

// sorting
enum
{
	SORT_NONE		= 0,
	SORT_PORTAL	= 1,
	SORT_SKY		= 2,
	SORT_OPAQUE	= 3,
	SORT_DECAL	= 4,
	SORT_ALPHATEST	= 5,
	SORT_BANNER	= 6,
	SORT_UNDERWATER	= 8,
	SORT_ADDITIVE	= 9,
	SORT_NEAREST	= 16
};

// shaderpass flags
#define SHADERSTAGE_MARK_BEGIN		0x10000 // same as GLSTATE_MARK_END
enum
{
	SHADERSTAGE_LIGHTMAP		= SHADERSTAGE_MARK_BEGIN,
	SHADERSTAGE_DETAIL			= SHADERSTAGE_MARK_BEGIN << 1,
	SHADERSTAGE_NOCOLORARRAY		= SHADERSTAGE_MARK_BEGIN << 2,
	SHADERSTAGE_DLIGHT			= SHADERSTAGE_MARK_BEGIN << 3,
	SHADERSTAGE_DELUXEMAP		= SHADERSTAGE_MARK_BEGIN << 4,
	SHADERSTAGE_PORTALMAP		= SHADERSTAGE_MARK_BEGIN << 5,
	SHADERSTAGE_STENCILSHADOW		= SHADERSTAGE_MARK_BEGIN << 6,

	SHADERSTAGE_BLEND_REPLACE		= SHADERSTAGE_MARK_BEGIN << 7,
	SHADERSTAGE_BLEND_MODULATE		= SHADERSTAGE_MARK_BEGIN << 8,
	SHADERSTAGE_BLEND_ADD		= SHADERSTAGE_MARK_BEGIN << 9,
	SHADERSTAGE_BLEND_DECAL		= SHADERSTAGE_MARK_BEGIN << 10
};

#define SHADERSTAGE_BLENDMODE	(SHADERSTAGE_BLEND_REPLACE|SHADERSTAGE_BLEND_MODULATE|SHADERSTAGE_BLEND_ADD|SHADERSTAGE_BLEND_DECAL)

// transform functions
typedef enum
{
	WAVEFORM_SIN			= 1,
	WAVEFORM_TRIANGLE			= 2,
	WAVEFORM_SQUARE			= 3,
	WAVEFORM_SAWTOOTH			= 4,
	WAVEFORM_INVERSESAWTOOTH		= 5,
	WAVEFORM_NOISE			= 6,
	WAVEFORM_CONSTANT			= 7
} waveForm_t;

// RGB colors generation
enum
{
	RGBGEN_UNKNOWN,
	RGBGEN_IDENTITY,
	RGBGEN_IDENTITY_LIGHTING,
	RGBGEN_CONST,
	RGBGEN_WAVE,
	RGBGEN_ENTITY,
	RGBGEN_ONE_MINUS_ENTITY,
	RGBGEN_VERTEX,
	RGBGEN_ONE_MINUS_VERTEX,
	RGBGEN_LIGHTING_DIFFUSE,
	RGBGEN_EXACT_VERTEX,
	RGBGEN_LIGHTING_DIFFUSE_ONLY,
	RGBGEN_LIGHTING_AMBIENT_ONLY,
	RGBGEN_FOG,
	RGBGEN_CUSTOM,
#ifdef HARDWARE_OUTLINES
	RGBGEN_OUTLINE,
#endif
	RGBGEN_ENVIRONMENT,
};

// alpha channel generation
enum
{
	ALPHAGEN_UNKNOWN,
	ALPHAGEN_IDENTITY,
	ALPHAGEN_CONST,
	ALPHAGEN_PORTAL,
	ALPHAGEN_VERTEX,
	ALPHAGEN_ONE_MINUS_VERTEX,
	ALPHAGEN_ENTITY,
	ALPHAGEN_SPECULAR,
	ALPHAGEN_WAVE,
	ALPHAGEN_DOT,
	ALPHAGEN_ONE_MINUS_DOT,
#ifdef HARDWARE_OUTLINES
	ALPHAGEN_OUTLINE
#endif
};

// texture coordinates generation
enum
{
	TCGEN_NONE,
	TCGEN_BASE,
	TCGEN_LIGHTMAP,
	TCGEN_ENVIRONMENT,
	TCGEN_VECTOR,
	TCGEN_REFLECTION,
	TCGEN_FOG,
	TCGEN_REFLECTION_CELLSHADE,
	TCGEN_SVECTORS,
	TCGEN_PROJECTION,
	TCGEN_PROJECTION_SHADOW
};

// tcmod functions
enum
{
	TCMOD_NONE,
	TCMOD_SCALE,
	TCMOD_SCROLL,
	TCMOD_ROTATE,
	TCMOD_TRANSFORM,
	TCMOD_TURB,
	TCMOD_STRETCH
};

// vertices deformation
enum
{
	DEFORM_NONE,
	DEFORM_WAVE,
	DEFORM_NORMAL,
	DEFORM_BULGE,
	DEFORM_MOVE,
	DEFORM_AUTOSPRITE,
	DEFORM_AUTOSPRITE2,
	DEFORM_PROJECTION_SHADOW,
	DEFORM_AUTOPARTICLE,
#ifdef HARDWARE_OUTLINES
	DEFORM_OUTLINE
#endif
};

typedef struct
{
	waveForm_t	type;		// SHADER_FUNC enum
	float		args[4];		// offset, amplitude, phase_offset, rate
} waveFunc_t;

typedef struct
{
	word		type;
	float		args[6];
} tcmod_t;

typedef struct
{
	word		type;
	float		*args;
	waveFunc_t	*func;
} colorgen_t;

typedef struct
{
	word		type;
	float		args[4];
	waveFunc_t	func;
} deformv_t;

// Per-pass rendering state information
typedef struct ref_stage_s
{
	uint		flags;
	uint		glstate;			// GLSTATE_ flags

	colorgen_t	rgbgen;
	colorgen_t	alphagen;

	word		tcgen;
	vec_t		*tcgenVec;

	word		numtcmods;
	tcmod_t		*tcmods;

	uint		cin;

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

	word		flags;
	word		features;
	uint		sort;
	uint		sortkey;

	int		type;

	word		numpasses;
	ref_stage_t	*passes;

	word		numdeforms;
	deformv_t		*deforms;

	rgba_t		fog_color;
	float		fog_dist, fog_clearDist;

	float		offsetmapping_scale;

	struct ref_shader_s		*hash_next;
} ref_shader_t;

// memory management
extern byte *r_shadersmempool;

extern ref_shader_t	r_shaders[MAX_SHADERS];
extern int r_numShaders;
extern skydome_t *r_skydomes[MAX_SHADERS];

#define		Shader_Malloc( size ) Mem_Alloc( r_shadersmempool, size )
#define		Shader_Free( data ) Mem_Free( data )
#define		Shader_Sortkey( shader, sort ) ( ( ( sort )<<26 )|( shader-r_shaders ) )

void		R_InitShaders( bool silent );
void		R_ShutdownShaders( void );

void		R_UploadCinematicShader( const ref_shader_t *shader );

void		R_DeformvBBoxForShader( const ref_shader_t *shader, vec3_t ebbox );

ref_shader_t	*R_LoadShader( const char *name, int type, bool forceDefault, int addFlags, int ignoreType );

ref_shader_t	*R_RegisterPic( const char *name );
ref_shader_t	*R_RegisterShader( const char *name );
ref_shader_t	*R_RegisterSkin( const char *name );

void		R_ShaderList_f( void );
void		R_ShaderDump_f( void );

#endif /*__R_SHADER_H__*/
