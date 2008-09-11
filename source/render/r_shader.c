//=======================================================================
//			Copyright XashXT Group 2007 ©
//		 r_shader.c - shader script parsing and loading
//=======================================================================

#include "r_local.h"
#include "const.h"

#define SHADERS_HASHSIZE		256

typedef struct
{
	const char		*name;
	int			surfaceFlags;
	int			contents;
} shaderParm_t;

typedef struct shaderScript_s
{
	struct shaderScript_s	*nextHash;
	string			name;
	shaderType_t		shaderType;
	uint			surfaceParm;
	int			contents;
	char			script[1];	// variable sized
} shaderScript_t;

static shader_t		r_parseShader;
static shaderStage_t	r_parseShaderStages[SHADER_MAX_STAGES];
static stageBundle_t	r_parseStageTMU[SHADER_MAX_STAGES][MAX_TEXTURE_UNITS];

static shaderScript_t	*r_shaderScriptsHash[SHADERS_HASHSIZE];
static shader_t		*r_shadersHash[SHADERS_HASHSIZE];
static texture_t		*r_internalMiptex;

shader_t			*r_shaders[MAX_SHADERS];
int			r_numShaders = 0;
byte			*r_shaderpool;

// builtin shaders
shader_t *r_defaultShader;
shader_t *r_lightmapShader;
shader_t *r_waterCausticsShader;
shader_t *r_slimeCausticsShader;
shader_t *r_lavaCausticsShader;

shaderParm_t infoParms[] =
{
	// server relevant contents
	{"water",		0,		CONTENTS_WATER		},
	{"slime",		0,		CONTENTS_SLIME,		}, // mildly damaging
	{"lava",		0,		CONTENTS_LAVA,		}, // very damaging
	{"playerclip",	0,		CONTENTS_PLAYERCLIP,	},
	{"monsterclip",	0,		CONTENTS_MONSTERCLIP,	},
	{"clip",		0,		CONTENTS_CLIP,		},
	{"nonsolid",	0,		0,			}, // just clears the solid flag

	// utility relevant attributes
	{"origin",	0,		CONTENTS_ORIGIN,		}, // center of rotating brushes
	{"trans",		0,		CONTENTS_TRANSLUCENT,	}, // don't eat contained surfaces
	{"detail",	0,		CONTENTS_DETAIL,		}, // carves surfaces entering
	{"world",		0,		CONTENTS_STRUCTURAL,	}, // force into world even if trans
	{"areaportal",	0,		CONTENTS_AREAPORTAL,	},
	{"fog",		0,		CONTENTS_FOG,		}, // carves surfaces entering
	{"sky",		SURF_SKY,		0,			}, // emit light from environment map
	{"skyroom",	SURF_SKYROOM,	0,			}, // env_sky surface
	{"lightfilter",	SURF_LIGHTFILTER,	0,			}, // filter light going through it
	{"alphashadow",	SURF_ALPHASHADOW,	0,			}, // test light on a per-pixel basis
	{"hint",		SURF_HINT,	0,			}, // use as a primary splitter
	{"skip",		SURF_NODRAW,	0,			}, // use as a secondary splitter
	{"null",		SURF_NODRAW,	0,			}, // don't generate a drawsurface
	{"nodraw",	SURF_NODRAW,	0,			}, // don't generate a drawsurface

	// server attributes
	{"slick",		0,		SURF_SLICK,		},
	{"noimpact",	0,		SURF_NOIMPACT,		}, // no impact explosions or marks
	{"nomarks",	0,		SURF_NOMARKS,		}, // no impact marks, but explodes
	{"ladder",	0,		CONTENTS_LADDER,		},
	{"nodamage",	SURF_NODAMAGE,	0,			},
	{"nosteps",	SURF_NOSTEPS,	0,			},

	// drawsurf attributes
	{"nolightmap",	SURF_NOLIGHTMAP,	0,			}, // don't generate a lightmap
	{"nodlight",	SURF_NODLIGHT,	0,			}, // don't ever add dynamic lights
	{"alpha",		SURF_ALPHA,	CONTENTS_TRANSLUCENT,	}, // alpha surface preset
	{"additive",	SURF_ADDITIVE,	CONTENTS_TRANSLUCENT,	}, // additive surface preset
	{"blend",		SURF_BLEND,	CONTENTS_TRANSLUCENT,	}, // blend surface preset
	{"mirror",	SURF_PORTAL,	0,			}, // mirror surface
	{"portal",	SURF_PORTAL,	0,			}, // portal surface
};

/*
=======================================================================

SHADER PARSING

=======================================================================
*/
/*
=================
R_ParseWaveFunc
=================
*/
static bool R_ParseWaveFunc( shader_t *shader, waveFunc_t *func, char **script )
{
	char	*tok;
	int	i;

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
		return false;

	if(Com_MatchToken( "sin" )) func->type = WAVEFORM_SIN;
	else if(Com_MatchToken( "triangle" )) func->type = WAVEFORM_TRIANGLE;
	else if(Com_MatchToken( "square" )) func->type = WAVEFORM_SQUARE;
	else if(Com_MatchToken( "sawtooth" )) func->type = WAVEFORM_SAWTOOTH;
	else if(Com_MatchToken( "inverseSawtooth" )) func->type = WAVEFORM_INVERSESAWTOOTH;
	else if(Com_MatchToken( "noise" )) func->type = WAVEFORM_NOISE;
	else
	{
		MsgDev( D_WARN, "unknown waveform '%s' in shader '%s', defaulting to sin\n", tok, shader->name );
		func->type = WAVEFORM_SIN;
	}

	for( i = 0; i < 4; i++ )
	{
		tok = Com_ParseToken(script, false);
		if( !tok[0] ) return false;
		func->params[i] = com.atof( tok );
	}
	return true;
}

/*
=================
R_ParseHeightToNormal
=================
*/
static bool R_ParseHeightToNormal( shader_t *shader, char *heightMap, int heightMapLen, float *bumpScale, char **script )
{
	char	*tok;

	tok = Com_ParseToken( script, false );
	if(!Com_MatchToken( "(" ))
	{
		MsgDev( D_WARN, "missing '(' for 'heightToNormal' in shader '%s'\n", shader->name );
		return false;
	}

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'heightToNormal' in shader '%s'\n", shader->name );
		return false;
	}
	com.strncpy( heightMap, tok, heightMapLen );

	tok = Com_ParseToken( script, false );
	if(!Com_MatchToken( "," ))
	{
		MsgDev( D_WARN, "expected ',', found '%s' instead in 'heightToNormal' in shader '%s'\n", tok, shader->name );
		return false;
	}

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'heightToNormal' in shader '%s'\n", shader->name );
		return false;
	}
	*bumpScale = com.atof( tok );

	tok = Com_ParseToken( script, false );
	if(!Com_MatchToken( ")" ))
	{
		MsgDev( D_WARN, "missing ')' for 'heightToNormal' in shader '%s'\n", shader->name );
		return false;
	}
	return true;
}

/*
=================
R_ParseGeneralSurfaceParm
=================
*/
static bool R_ParseGeneralSurfaceParm( shader_t *shader, char **script )
{
	char	*tok;
	int	i, numInfoParms = sizeof(infoParms) / sizeof(infoParms[0]);
	
	switch( shader->shaderType )
	{
	case SHADER_TEXTURE:
	case SHADER_STUDIO:
	case SHADER_SPRITE:
		break;
	default:
		MsgDev( D_WARN, "'surfaceParm' not allowed in shader '%s'\n", shader->name );
		return false;
	}

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'surfaceParm' in shader '%s'\n", shader->name );
		return false;
	}

	for( i = 0; i < numInfoParms; i++ )
	{
		if(Com_MatchToken( infoParms[i].name ))
		{
			shader->surfaceParm |= infoParms[i].surfaceFlags;
			shader->contentFlags |= infoParms[i].contents;
			break;
		}
	}

	if( i == numInfoParms )
	{
		MsgDev( D_WARN, "unknown 'surfaceParm' parameter '%s' in shader '%s'\n", tok, shader->name );
		return false;
	}

	shader->flags |= SHADER_SURFACEPARM;
	return true;
}

/*
=================
R_ParseGeneralNoMipmaps
=================
*/
static bool R_ParseGeneralNoMipmaps( shader_t *shader, char **script )
{
	shader->flags |= (SHADER_NOMIPMAPS|SHADER_NOPICMIP);
	return true;
}

/*
=================
R_ParseGeneralNoPicmip
=================
*/
static bool R_ParseGeneralNoPicmip( shader_t *shader, char **script )
{
	shader->flags |= SHADER_NOPICMIP;
	return true;
}

/*
=================
R_ParseGeneralNoCompress
=================
*/
static bool R_ParseGeneralNoCompress( shader_t *shader, char **script )
{
	shader->flags |= SHADER_NOCOMPRESS;
	return true;
}

/*
=================
R_ParseGeneralNoShadows
=================
*/
static bool R_ParseGeneralNoShadows( shader_t *shader, char **script )
{
	shader->flags |= SHADER_NOSHADOWS;
	return true;
}

/*
=================
R_ParseGeneralNoFragments
=================
*/
static bool R_ParseGeneralNoFragments( shader_t *shader, char **script )
{
	shader->flags |= SHADER_NOFRAGMENTS;
	return true;
}

/*
=================
R_ParseGeneralEntityMergable
=================
*/
static bool R_ParseGeneralEntityMergable( shader_t *shader, char **script )
{
	shader->flags |= SHADER_ENTITYMERGABLE;
	return true;
}

/*
=================
R_ParseGeneralPolygonOffset
=================
*/
static bool R_ParseGeneralPolygonOffset( shader_t *shader, char **script )
{
	shader->flags |= SHADER_POLYGONOFFSET;
	return true;
}

/*
=================
R_ParseGeneralCull
=================
*/
static bool R_ParseGeneralCull( shader_t *shader, char **script )
{
	char	*tok;

	tok = Com_ParseToken( script, false );
	if( !tok[0] ) shader->cull.mode = GL_FRONT;
	else
	{
		if(Com_MatchToken( "front")) shader->cull.mode = GL_FRONT;
		else if(Com_MatchToken( "back") || Com_MatchToken( "backSide") || Com_MatchToken( "backSided"))
			shader->cull.mode = GL_BACK;
		else if(Com_MatchToken( "disable") || Com_MatchToken( "none") || Com_MatchToken( "twoSided"))
			shader->cull.mode = 0;
		else
		{
			MsgDev( D_WARN, "unknown 'cull' parameter '%s' in shader '%s'\n", tok, shader->name );
			return false;
		}
	}
	shader->flags |= SHADER_CULL;
	return true;
}

/*
=================
R_ParseGeneralSort
=================
*/
static bool R_ParseGeneralSort( shader_t *shader, char **script )
{
	char	*tok;

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'sort' in shader '%s'\n", shader->name );
		return false;
	}

	if(Com_MatchToken( "sky" )) shader->sort = SORT_SKY;
	else if(Com_MatchToken( "opaque")) shader->sort = SORT_OPAQUE;
	else if(Com_MatchToken( "decal")) shader->sort = SORT_DECAL;
	else if(Com_MatchToken( "seeThrough")) shader->sort = SORT_SEETHROUGH;
	else if(Com_MatchToken( "banner")) shader->sort = SORT_BANNER;
	else if(Com_MatchToken( "underwater")) shader->sort = SORT_UNDERWATER;
	else if(Com_MatchToken( "water")) shader->sort = SORT_WATER;
	else if(Com_MatchToken( "innerBlend")) shader->sort = SORT_INNERBLEND;
	else if(Com_MatchToken( "blend")) shader->sort = SORT_BLEND;
	else if(Com_MatchToken( "blend2")) shader->sort = SORT_BLEND2;
	else if(Com_MatchToken( "blend3")) shader->sort = SORT_BLEND3;
	else if(Com_MatchToken( "blend4")) shader->sort = SORT_BLEND4;
	else if(Com_MatchToken( "outerBlend")) shader->sort = SORT_OUTERBLEND;
	else if(Com_MatchToken( "additive")) shader->sort = SORT_ADDITIVE;
	else if(Com_MatchToken( "nearest")) shader->sort = SORT_NEAREST;
	else
	{
		shader->sort = com.atoi( tok );

		if( shader->sort < 1 || shader->sort > 15 )
		{
			MsgDev( D_WARN, "unknown 'sort' parameter '%s' in shader '%s'\n", tok, shader->name );
			return false;
		}
	}
	shader->flags |= SHADER_SORT;
	return true;
}

/*
=================
R_ParseGeneralTessSize
=================
*/
static bool R_ParseGeneralTessSize( shader_t *shader, char **script )
{
	char	*tok;
	int	i = 8;

	if( shader->shaderType != SHADER_TEXTURE )
	{
		MsgDev( D_WARN, "'tessSize' not allowed in shader '%s'\n", shader->name );
		return false;
	}

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'tessSize' in shader '%s'\n", shader->name );
		return false;
	}

	shader->tessSize = com.atoi( tok );

	if( shader->tessSize < 8 || shader->tessSize > 256 )
	{
		MsgDev( D_WARN, "out of range size value of %i for 'tessSize' in shader '%s', defaulting to 64\n", shader->tessSize, shader->name );
		shader->tessSize = 64;
	}
	else
	{
		while( i <= shader->tessSize ) i<<=1;
		shader->tessSize = i>>1;
	}

	shader->flags |= SHADER_TESSSIZE;
	return true;
}

/*
=================
R_ParseGeneralSkyParms
=================
*/
static bool R_ParseGeneralSkyParms( shader_t *shader, char **script )
{
	string	name;
	char	*tok;
	int	i;

	if( shader->shaderType != SHADER_SKY )
	{
		MsgDev( D_WARN, "'skyParms' not allowed in shader '%s'\n", shader->name );
		return false;
	}

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'skyParms' in shader '%s'\n", shader->name );
		return false;
	}

	if(!Com_MatchToken( "-"))
	{
		for( i = 0; i < 6; i++ )
		{
			if( shader->skyParms.farBox[i] )
				shader->skyParms.farBox[i]->flags &= ~TF_STATIC; // old skybox will be removed on next loading
			com.snprintf( name, sizeof(name), "%s%s", tok, r_skyBoxSuffix[i] );
			shader->skyParms.farBox[i] = R_FindTexture( name, NULL, 0, TF_CLAMP|TF_SKYSIDE|TF_STATIC, 0 );
			if( !shader->skyParms.farBox[i] )
			{
				MsgDev( D_WARN, "couldn't find texture '%s' in shader '%s'\n", name, shader->name );
				return false;
			}
		}
	}

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'skyParms' in shader '%s'\n", shader->name );
		return false;
	}

	if(!Com_MatchToken( "-"))
	{
		shader->skyParms.cloudHeight = com.atof(tok);
		if( shader->skyParms.cloudHeight < 8.0 || shader->skyParms.cloudHeight > 1024.0 )
		{
			MsgDev( D_WARN, "out of range cloudHeight value of %f for 'skyParms' in shader '%s', defaulting to 128\n", shader->skyParms.cloudHeight, shader->name );
			shader->skyParms.cloudHeight = 128.0;
		}
	}
	else shader->skyParms.cloudHeight = 128.0;

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'skyParms' in shader '%s'\n", shader->name );
		return false;
	}

	if(!Com_MatchToken( "-"))
	{
		for( i = 0; i < 6; i++ )
		{
			if( shader->skyParms.nearBox[i] )
				shader->skyParms.nearBox[i]->flags &= ~TF_STATIC; // old skybox will be removed on next loading
			com.snprintf( name, sizeof(name), "%s%s", tok, r_skyBoxSuffix[i] );
			shader->skyParms.nearBox[i] = R_FindTexture( name, NULL, 0, TF_CLAMP|TF_SKYSIDE|TF_STATIC, 0 );
			if( !shader->skyParms.nearBox[i] )
			{
				MsgDev( D_WARN, "couldn't find texture '%s' in shader '%s'\n", name, shader->name );
				return false;
			}
		}
	}
	shader->flags |= SHADER_SKYPARMS;
	return true;
}

/*
=================
R_ParseGeneralDeformVertexes
=================
*/
static bool R_ParseGeneralDeformVertexes( shader_t *shader, char **script )
{
	deformVerts_t	*deformVertexes;
	char		*tok;
	int		i;

	if( shader->deformVertexesNum == SHADER_MAX_DEFORMVERTEXES )
	{
		MsgDev( D_WARN, "SHADER_MAX_DEFORMVERTEXES hit in shader '%s'\n", shader->name );
		return false;
	}
	deformVertexes = &shader->deformVertexes[shader->deformVertexesNum++];

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'deformVertexes' in shader '%s'\n", shader->name );
		return false;
	}

	if(Com_MatchToken( "wave" ))
	{
		tok = Com_ParseToken( script, false );
		if( !tok[0] )
		{
			MsgDev( D_WARN, "missing parameters for 'deformVertexes wave' in shader '%s'\n", shader->name );
			return false;
		}

		deformVertexes->params[0] = com.atof( tok );
		if( deformVertexes->params[0] == 0.0 )
		{
			MsgDev( D_WARN, "illegal div value of 0 for 'deformVertexes wave' in shader '%s', defaulting to 100\n", shader->name );
			deformVertexes->params[0] = 100.0;
		}
		deformVertexes->params[0] = 1.0 / deformVertexes->params[0];

		if(!R_ParseWaveFunc( shader, &deformVertexes->func, script ))
		{
			MsgDev( D_WARN, "missing waveform parameters for 'deformVertexes wave' in shader '%s'\n", shader->name );
			return false;
		}
		deformVertexes->type = DEFORMVERTEXES_WAVE;
	}
	else if( Com_MatchToken( "move" ))
	{
		// g-cont. replace this stuff with com.atov ?
		for( i = 0; i < 3; i++ )
		{
			tok = Com_ParseToken( script, false );
			if( !tok[0] )
			{
				MsgDev( D_WARN, "missing parameters for 'deformVertexes move' in shader '%s'\n", shader->name );
				return false;
			}
			deformVertexes->params[i] = com.atof( tok );
		}

		if(!R_ParseWaveFunc( shader, &deformVertexes->func, script ))
		{
			MsgDev( D_WARN, "missing waveform parameters for 'deformVertexes move' in shader '%s'\n", shader->name );
			return false;
		}
		deformVertexes->type = DEFORMVERTEXES_MOVE;
	}
	else if(Com_MatchToken( "normal" ))
	{
		for( i = 0; i < 2; i++ )
		{
			tok = Com_ParseToken( script, false );
			if( !tok[0] )
			{
				MsgDev( D_WARN, "missing parameters for 'deformVertexes normal' in shader '%s'\n", shader->name );
				return false;
			}
			deformVertexes->params[i] = atof(tok);
		}
		deformVertexes->type = DEFORMVERTEXES_NORMAL;
	}
	else
	{
		MsgDev( D_WARN, "unknown 'deformVertexes' parameter '%s' in shader '%s'\n", tok, shader->name );
		return false;
	}
	shader->flags |= SHADER_DEFORMVERTEXES;
	return true;
}

/*
=================
R_ParseStageRequires
=================
*/
static bool R_ParseStageRequires( shader_t *shader, shaderStage_t *stage, char **script )
{
	MsgDev( D_WARN, "'requires' is not the first command in the stage in shader '%s'\n", shader->name );
	return false;
}

/*
=================
R_ParseStageNoMipmaps
=================
*/
static bool R_ParseStageNoMipmaps( shader_t *shader, shaderStage_t *stage, char **script )
{
	stageBundle_t *bundle = stage->bundles[stage->numBundles - 1];
	bundle->flags |= (STAGEBUNDLE_NOMIPMAPS|STAGEBUNDLE_NOPICMIP);
	return true;
}

/*
=================
R_ParseStageNoPicmip
=================
*/
static bool R_ParseStageNoPicmip( shader_t *shader, shaderStage_t *stage, char **script )
{
	stageBundle_t *bundle = stage->bundles[stage->numBundles - 1];
	bundle->flags |= STAGEBUNDLE_NOPICMIP;
	return true;
}

/*
=================
R_ParseStageNoCompress
=================
*/
static bool R_ParseStageNoCompress( shader_t *shader, shaderStage_t *stage, char **script )
{
	stageBundle_t *bundle = stage->bundles[stage->numBundles - 1];
	bundle->flags |= STAGEBUNDLE_NOCOMPRESS;
	return true;
}

/*
=================
R_ParseStageClampTexCoords
=================
*/
static bool R_ParseStageClampTexCoords( shader_t *shader, shaderStage_t *stage, char **script )
{
	stageBundle_t *bundle = stage->bundles[stage->numBundles - 1];
	bundle->flags |= STAGEBUNDLE_CLAMPTEXCOORDS;
	return true;
}

/*
=================
R_ParseStageAnimFrequency
=================
*/
static bool R_ParseStageAnimFrequency( shader_t *shader, shaderStage_t *stage, char **script )
{
	stageBundle_t	*bundle = stage->bundles[stage->numBundles - 1];
	char		*tok;

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'animFrequency' in shader '%s\n", shader->name );
		return false;
	}

	bundle->animFrequency = com.atof( tok );
	bundle->flags |= STAGEBUNDLE_ANIMFREQUENCY;

	return true;
}

/*
=================
R_ParseStageMap
=================
*/
static bool R_ParseStageMap( shader_t *shader, shaderStage_t *stage, char **script )
{
	stageBundle_t	*bundle = stage->bundles[stage->numBundles - 1];
	unsigned		flags = 0;
	char		*tok;

	if( bundle->numTextures )
	{
		if( bundle->numTextures == SHADER_MAX_TEXTURES )
		{
			MsgDev( D_WARN, "SHADER_MAX_TEXTURES hit in shader '%s'\n", shader->name );
			return false;
		}

		if(!(bundle->flags & STAGEBUNDLE_MAP))
		{
			MsgDev( D_WARN, "animation with mixed texture types in shader '%s'\n", shader->name );
			return false;
		}

		if(!(bundle->flags & STAGEBUNDLE_ANIMFREQUENCY))
		{
			MsgDev( D_WARN, "multiple 'map' specifications without preceding 'animFrequency' in shader '%s'\n", shader->name );
			return false;
		}
	}

	if( bundle->cinematicHandle )
	{
		MsgDev( D_WARN, "animation with mixed texture types in shader '%s'\n", shader->name );
		return false;
	}

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'map' in shader '%s'\n", shader->name );
		return false;
	}

	if(Com_MatchToken( "$lightmap"))
	{
		if( shader->shaderType != SHADER_TEXTURE )
		{
			MsgDev( D_WARN, "'map $lightmap' not allowed in shader '%s'\n", shader->name );
			return false;
		}

		if( bundle->flags & STAGEBUNDLE_ANIMFREQUENCY )
		{
			MsgDev( D_WARN, "'map $lightmap' not allowed with 'animFrequency' in shader '%s'\n", shader->name );
			return false;
		}

		bundle->texType = TEX_LIGHTMAP;
		bundle->flags |= STAGEBUNDLE_MAP;
		shader->flags |= SHADER_HASLIGHTMAP;

		return true;
	}

	if( Com_MatchToken( "$whiteImage" )) bundle->textures[bundle->numTextures++] = r_whiteTexture;
	else if( Com_MatchToken( "$blackImage")) bundle->textures[bundle->numTextures++] = r_blackTexture;
	else if( Com_MatchToken( "$internal")) bundle->textures[bundle->numTextures++] = r_internalMiptex;
	else
	{
		if(!(shader->flags & SHADER_NOMIPMAPS) && !(bundle->flags & STAGEBUNDLE_NOMIPMAPS))
			flags |= TF_MIPMAPS;
		if(!(shader->flags & SHADER_NOPICMIP) && !(bundle->flags & STAGEBUNDLE_NOPICMIP))
			flags |= TF_IMAGE2D;
		if(!(shader->flags & SHADER_NOCOMPRESS) && !(bundle->flags & STAGEBUNDLE_NOCOMPRESS))
			flags |= TF_COMPRESS;

		if( bundle->flags & STAGEBUNDLE_CLAMPTEXCOORDS)
			flags |= TF_CLAMP;

		bundle->textures[bundle->numTextures] = R_FindTexture( tok, NULL, 0, flags, 0 );
		if( !bundle->textures[bundle->numTextures])
		{
			MsgDev( D_WARN, "couldn't find texture '%s' in shader '%s'\n", tok, shader->name );
			return false;
		}
		bundle->numTextures++;
	}
	bundle->texType = TEX_GENERIC;
	bundle->flags |= STAGEBUNDLE_MAP;

	return true;
}

/*
=================
R_ParseStageBumpMap
=================
*/
static bool R_ParseStageBumpMap( shader_t *shader, shaderStage_t *stage, char **script )
{
	stageBundle_t	*bundle = stage->bundles[stage->numBundles - 1];
	uint		flags = TF_NORMALMAP;
	char		heightMap[MAX_QPATH];
	float		bumpScale;
	char		*tok;

	if( bundle->numTextures )
	{
		if( bundle->numTextures == SHADER_MAX_TEXTURES )
		{
			MsgDev( D_WARN, "SHADER_MAX_TEXTURES hit in shader '%s'\n", shader->name );
			return false;
		}
		if(!(bundle->flags & STAGEBUNDLE_BUMPMAP))
		{
			MsgDev( D_WARN, "animation with mixed texture types in shader '%s'\n", shader->name );
			return false;
		}
		if(!(bundle->flags & STAGEBUNDLE_ANIMFREQUENCY))
		{
			MsgDev( D_WARN, "multiple 'bumpMap' specifications without preceding 'animFrequency' in shader '%s'\n", shader->name );
			return false;
		}
	}

	if( bundle->cinematicHandle )
	{
		MsgDev( D_WARN, "animation with mixed texture types in shader '%s'\n", shader->name );
		return false;
	}

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'bumpMap' in shader '%s'\n", shader->name );
		return false;
	}

	if(!(shader->flags & SHADER_NOMIPMAPS) && !(bundle->flags & STAGEBUNDLE_NOMIPMAPS))
		flags |= TF_MIPMAPS;
	if(!(shader->flags & SHADER_NOPICMIP) && !(bundle->flags & STAGEBUNDLE_NOPICMIP))
		flags |= TF_IMAGE2D;
	if(!(shader->flags & SHADER_NOCOMPRESS) && !(bundle->flags & STAGEBUNDLE_NOCOMPRESS))
		flags |= TF_COMPRESS;

	if(bundle->flags & STAGEBUNDLE_CLAMPTEXCOORDS)
		flags |= TF_CLAMP;

	if(Com_MatchToken( "heightToNormal"))
	{
		if(!R_ParseHeightToNormal( shader, heightMap, sizeof(heightMap), &bumpScale, script ))
			return false;

		bundle->textures[bundle->numTextures] = R_FindTexture(heightMap, NULL, 0, flags|TF_HEIGHTMAP, bumpScale );
		if( !bundle->textures[bundle->numTextures] )
		{
			MsgDev( D_WARN, "couldn't find texture '%s' in shader '%s'\n", heightMap, shader->name );
			return false;
		}
		bundle->numTextures++;
	}
	else
	{
		bundle->textures[bundle->numTextures] = R_FindTexture( tok, NULL, 0, flags, 0 );
		if( !bundle->textures[bundle->numTextures] )
		{
			MsgDev( D_WARN, "couldn't find texture '%s' in shader '%s'\n", tok, shader->name );
			return false;
		}
		bundle->numTextures++;
	}
	bundle->texType = TEX_GENERIC;
	bundle->flags |= STAGEBUNDLE_BUMPMAP;

	return true;
}

/*
=================
R_ParseStageCubeMap
=================
*/
static bool R_ParseStageCubeMap( shader_t *shader, shaderStage_t *stage, char **script )
{
	stageBundle_t	*bundle = stage->bundles[stage->numBundles - 1];
	unsigned		flags = TF_CLAMP | TF_CUBEMAP;
	char			*tok;

	if( !GL_Support( R_TEXTURECUBEMAP_EXT ))
	{
		MsgDev( D_WARN, "shader '%s' uses 'cubeMap' without 'requires GL_ARB_texture_cube_map'\n", shader->name );
		return false;
	}

	if( bundle->numTextures )
	{
		if( bundle->numTextures == SHADER_MAX_TEXTURES )
		{
			MsgDev( D_WARN, "SHADER_MAX_TEXTURES hit in shader '%s'\n", shader->name );
			return false;
		}

		if(!(bundle->flags & STAGEBUNDLE_CUBEMAP))
		{
			MsgDev( D_WARN, "animation with mixed texture types in shader '%s'\n", shader->name );
			return false;
		}
		if(!(bundle->flags & STAGEBUNDLE_ANIMFREQUENCY))
		{
			MsgDev( D_WARN, "multiple 'cubeMap' specifications without preceding 'animFrequency' in shader '%s'\n", shader->name );
			return false;
		}
	}

	if( bundle->cinematicHandle )
	{
		MsgDev( D_WARN, "animation with mixed texture types in shader '%s'\n", shader->name );
		return false;
	}

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'cubeMap' in shader '%s'\n", shader->name );
		return false;
	}

	if( Com_MatchToken( "$normalize" ))
	{
		if( bundle->flags & STAGEBUNDLE_ANIMFREQUENCY )
		{
			MsgDev( D_WARN, "'cubeMap $normalize' not allowed with 'animFrequency' in shader '%s'\n", shader->name );
			return false;
		}
		bundle->textures[bundle->numTextures++] = r_normalizeTexture;
	}
	else
	{
		if(!(shader->flags & SHADER_NOMIPMAPS) && !(bundle->flags & STAGEBUNDLE_NOMIPMAPS))
			flags |= TF_MIPMAPS;
		if(!(shader->flags & SHADER_NOPICMIP) && !(bundle->flags & STAGEBUNDLE_NOPICMIP))
			flags |= TF_IMAGE2D;
		if(!(shader->flags & SHADER_NOCOMPRESS) && !(bundle->flags & STAGEBUNDLE_NOCOMPRESS))
			flags |= TF_COMPRESS;

		if(bundle->flags & STAGEBUNDLE_CLAMPTEXCOORDS)
			flags |= TF_CLAMP;

		bundle->textures[bundle->numTextures] = R_FindCubeMapTexture( tok, flags, 0 );
		if( !bundle->textures[bundle->numTextures] )
		{
			MsgDev( D_WARN, "couldn't find texture '%s' in shader '%s'\n", tok, shader->name );
			return false;
		}
		bundle->numTextures++;
	}
	bundle->texType = TEX_GENERIC;
	bundle->flags |= STAGEBUNDLE_CUBEMAP;

	return true;
}

/*
=================
R_ParseStageVideoMap
=================
*/
static bool R_ParseStageVideoMap( shader_t *shader, shaderStage_t *stage, char **script )
{
	stageBundle_t	*bundle = stage->bundles[stage->numBundles - 1];
	char		*tok;

	if( bundle->numTextures )
	{
		MsgDev( D_WARN, "animation with mixed texture types in shader '%s\n", shader->name );
		return false;
	}

	if( bundle->cinematicHandle )
	{
		MsgDev( D_WARN, "multiple 'videoMap' specifications in shader '%s'\n", shader->name );
		return false;
	}

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'videoMap' in shader '%s'\n", shader->name );
		return false;
	}

	//FIXME: implement
	//bundle->cinematicHandle = CIN_PlayCinematic(tok, 0, 0, 0, 0, CIN_LOOPED|CIN_SILENT|CIN_SHADER);
	if( !bundle->cinematicHandle )
	{
		MsgDev( D_WARN, "couldn't find video '%s' in shader '%s'\n", tok, shader->name );
		return false;
	}
	bundle->texType = TEX_CINEMATIC;
	bundle->flags |= STAGEBUNDLE_VIDEOMAP;

	return true;
}

/*
=================
R_ParseStageTexEnvCombine
=================
*/
static bool R_ParseStageTexEnvCombine( shader_t *shader, shaderStage_t *stage, char **script )
{
	stageBundle_t	*bundle = stage->bundles[stage->numBundles - 1];
	int		numArgs;
	char		*tok;
	int		i;

	if(!GL_Support( R_COMBINE_EXT ))
	{
		MsgDev( D_WARN, "shader '%s' uses 'texEnvCombine' without 'requires GL_ARB_texture_env_combine'\n", shader->name );
		return false;
	}

	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		if(!(bundle->flags & STAGEBUNDLE_TEXENVCOMBINE))
		{
			MsgDev( D_WARN, "shader '%s' uses 'texEnvCombine' in a bundle without 'nextBundle combine'\n", shader->name );
			return false;
		}
	}

	// setup default params
	bundle->texEnv = GL_COMBINE_ARB;
	bundle->texEnvCombine.rgbCombine = GL_MODULATE;
	bundle->texEnvCombine.rgbSource[0] = GL_TEXTURE;
	bundle->texEnvCombine.rgbSource[1] = GL_PREVIOUS_ARB;
	bundle->texEnvCombine.rgbSource[2] = GL_CONSTANT_ARB;
	bundle->texEnvCombine.rgbOperand[0] = GL_SRC_COLOR;
	bundle->texEnvCombine.rgbOperand[1] = GL_SRC_COLOR;
	bundle->texEnvCombine.rgbOperand[2] = GL_SRC_ALPHA;
	bundle->texEnvCombine.rgbScale = 1;
	bundle->texEnvCombine.alphaCombine = GL_MODULATE;
	bundle->texEnvCombine.alphaSource[0] = GL_TEXTURE;
	bundle->texEnvCombine.alphaSource[1] = GL_PREVIOUS_ARB;
	bundle->texEnvCombine.alphaSource[2] = GL_CONSTANT_ARB;
	bundle->texEnvCombine.alphaOperand[0] = GL_SRC_ALPHA;
	bundle->texEnvCombine.alphaOperand[1] = GL_SRC_ALPHA;
	bundle->texEnvCombine.alphaOperand[2] = GL_SRC_ALPHA;
	bundle->texEnvCombine.alphaScale = 1;
	bundle->texEnvCombine.constColor[0] = 1.0;
	bundle->texEnvCombine.constColor[1] = 1.0;
	bundle->texEnvCombine.constColor[2] = 1.0;
	bundle->texEnvCombine.constColor[3] = 1.0;

	tok = Com_ParseToken( script, true );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'texEnvCombine' in shader '%s'\n", shader->name );
		return false;
	}

	if( Com_MatchToken( "{"))
	{
		while( 1 )
		{
			tok = Com_ParseToken( script, true );
			if( !tok[0] )
			{
				MsgDev( D_WARN, "no concluding '}' in 'texEnvCombine' in shader '%s'\n", shader->name );
				return false;
			}

			if(Com_MatchToken( "}")) break;

			if(Com_MatchToken( "rgb"))
			{
				tok = Com_ParseToken( script, false );
				if(!Com_MatchToken( "="))
				{
					MsgDev( D_WARN, "expected '=', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok, shader->name );
					return false;
				}

				tok = Com_ParseToken( script, false );
				if( !tok[0] )
				{
					MsgDev( D_WARN, "missing 'rgb' equation name for 'texEnvCombine' in shader '%s'\n", shader->name );
					return false;
				}

				if(Com_MatchToken( "REPLACE"))
				{
					bundle->texEnvCombine.rgbCombine = GL_REPLACE;
					numArgs = 1;
				}
				else if (Com_MatchToken( "MODULATE"))
				{
					bundle->texEnvCombine.rgbCombine = GL_MODULATE;
					numArgs = 2;
				}
				else if (Com_MatchToken( "ADD"))
				{
					bundle->texEnvCombine.rgbCombine = GL_ADD;
					numArgs = 2;
				}
				else if (Com_MatchToken( "ADD_SIGNED"))
				{
					bundle->texEnvCombine.rgbCombine = GL_ADD_SIGNED_ARB;
					numArgs = 2;
				}
				else if (Com_MatchToken( "INTERPOLATE"))
				{
					bundle->texEnvCombine.rgbCombine = GL_INTERPOLATE_ARB;
					numArgs = 3;
				}
				else if (Com_MatchToken( "SUBTRACT"))
				{
					bundle->texEnvCombine.rgbCombine = GL_SUBTRACT_ARB;
					numArgs = 2;
				}
				else if (Com_MatchToken( "DOT3_RGB"))
				{
					if(!GL_Support( R_DOT3_ARB_EXT ))
					{
						MsgDev( D_WARN, "shader '%s' uses 'DOT3_RGB' in 'texEnvCombine' without 'requires GL_ARB_texture_env_dot3'\n", shader->name );
						return false;
					}
					bundle->texEnvCombine.rgbCombine = GL_DOT3_RGB_ARB;
					numArgs = 2;
				}
				else if (Com_MatchToken( "DOT3_RGBA"))
				{
					if(!GL_Support( R_DOT3_ARB_EXT ))
					{
						MsgDev( D_WARN, "shader '%s' uses 'DOT3_RGBA' in 'texEnvCombine' without 'requires GL_ARB_texture_env_dot3'\n", shader->name );
						return false;
					}
					bundle->texEnvCombine.rgbCombine = GL_DOT3_RGBA_ARB;
					numArgs = 2;
				}
				else
				{
					MsgDev( D_WARN, "unknown 'rgb' equation name '%s' for 'texEnvCombine' in shader '%s'\n", tok, shader->name );
					return false;
				}

				tok = Com_ParseToken( script, false );
				if(!Com_MatchToken( "("))
				{
					MsgDev( D_WARN, "expected '(', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok, shader->name );
					return false;
				}

				for( i = 0; i < numArgs; i++ )
				{
					tok = Com_ParseToken( script, false );
					if( !tok[0] )
					{
						MsgDev( D_WARN, "missing 'rgb' equation arguments for 'texEnvCombine' in shader '%s'\n", shader->name );
						return false;
					}

					if (Com_MatchToken( "Ct"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_TEXTURE;
						bundle->texEnvCombine.rgbOperand[i] = GL_SRC_COLOR;
					}
					else if (Com_MatchToken( "1-Ct"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_TEXTURE;
						bundle->texEnvCombine.rgbOperand[i] = GL_ONE_MINUS_SRC_COLOR;
					}
					else if (Com_MatchToken( "At"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_TEXTURE;
						bundle->texEnvCombine.rgbOperand[i] = GL_SRC_ALPHA;
					}
					else if (Com_MatchToken( "1-At"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_TEXTURE;
						bundle->texEnvCombine.rgbOperand[i] = GL_ONE_MINUS_SRC_ALPHA;
					}
					else if (Com_MatchToken( "Cc"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_CONSTANT_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_SRC_COLOR;
					}
					else if (Com_MatchToken( "1-Cc"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_CONSTANT_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_ONE_MINUS_SRC_COLOR;
					}
					else if (Com_MatchToken( "Ac"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_CONSTANT_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_SRC_ALPHA;
					}
					else if (Com_MatchToken( "1-Ac"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_CONSTANT_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_ONE_MINUS_SRC_ALPHA;
					}
					else if (Com_MatchToken( "Cf"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_PRIMARY_COLOR_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_SRC_COLOR;
					}
					else if (Com_MatchToken( "1-Cf"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_PRIMARY_COLOR_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_ONE_MINUS_SRC_COLOR;
					}
					else if (Com_MatchToken( "Af"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_PRIMARY_COLOR_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_SRC_ALPHA;
					}
					else if (Com_MatchToken( "1-Af"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_PRIMARY_COLOR_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_ONE_MINUS_SRC_ALPHA;
					}
					else if (Com_MatchToken( "Cp"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_PREVIOUS_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_SRC_COLOR;
					}
					else if (Com_MatchToken( "1-Cp"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_PREVIOUS_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_ONE_MINUS_SRC_COLOR;
					}
					else if (Com_MatchToken( "Ap"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_PREVIOUS_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_SRC_ALPHA;
					}
					else if (Com_MatchToken( "1-Ap"))
					{
						bundle->texEnvCombine.rgbSource[i] = GL_PREVIOUS_ARB;
						bundle->texEnvCombine.rgbOperand[i] = GL_ONE_MINUS_SRC_ALPHA;
					}
					else
					{
						MsgDev( D_WARN, "unknown 'rgb' equation argument '%s' for 'texEnvCombine' in shader '%s'\n", tok, shader->name );
						return false;
					}

					if( i < numArgs - 1 )
					{
						tok = Com_ParseToken(script, false);
						if(!Com_MatchToken( ","))
						{
							MsgDev( D_WARN, "expected ',', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok, shader->name );
							return false;
						}
					}
				}

				tok = Com_ParseToken( script, false );
				if(Com_MatchToken( ")"))
				{
					MsgDev( D_WARN, "expected ')', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok, shader->name );
					return false;
				}

				tok = Com_ParseToken( script, false );
				if( tok[0] )
				{
					if(!Com_MatchToken( "*"))
					{
						MsgDev( D_WARN, "expected '*', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok, shader->name );
						return false;
					}

					tok = Com_ParseToken( script, false );
					if( !tok[0] )
					{
						MsgDev( D_WARN, "missing scale value for 'texEnvCombine' equation in shader '%s'\n", shader->name );
						return false;
					}
					bundle->texEnvCombine.rgbScale = com.atoi( tok );

					if( bundle->texEnvCombine.rgbScale != 1 && bundle->texEnvCombine.rgbScale != 2 && bundle->texEnvCombine.rgbScale != 4 )
					{
						MsgDev( D_WARN, "invalid scale value of %i for 'texEnvCombine' equation in shader '%s'\n", bundle->texEnvCombine.rgbScale, shader->name );
						return false;
					}
				}
			}
			else if (Com_MatchToken( "alpha"))
			{
				tok = Com_ParseToken( script, false );
				if (!Com_MatchToken( "="))
				{
					MsgDev( D_WARN, "expected '=', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok, shader->name );
					return false;
				}

				tok = Com_ParseToken( script, false );
				if( !tok[0] )
				{
					MsgDev( D_WARN, "missing 'alpha' equation name for 'texEnvCombine' in shader '%s'\n", shader->name );
					return false;
				}

				if (Com_MatchToken( "REPLACE"))
				{
					bundle->texEnvCombine.alphaCombine = GL_REPLACE;
					numArgs = 1;
				}
				else if (Com_MatchToken( "MODULATE"))
				{
					bundle->texEnvCombine.alphaCombine = GL_MODULATE;
					numArgs = 2;
				}
				else if (Com_MatchToken( "ADD"))
				{
					bundle->texEnvCombine.alphaCombine = GL_ADD;
					numArgs = 2;
				}
				else if (Com_MatchToken( "ADD_SIGNED"))
				{
					bundle->texEnvCombine.alphaCombine = GL_ADD_SIGNED_ARB;
					numArgs = 2;
				}
				else if (Com_MatchToken( "INTERPOLATE"))
				{
					bundle->texEnvCombine.alphaCombine = GL_INTERPOLATE_ARB;
					numArgs = 3;
				}
				else if (Com_MatchToken( "SUBTRACT"))
				{
					bundle->texEnvCombine.alphaCombine = GL_SUBTRACT_ARB;
					numArgs = 2;
				}
				else if (Com_MatchToken( "DOT3_RGB"))
				{
					if (!GL_Support( R_DOT3_ARB_EXT ))
					{
						MsgDev( D_WARN, "shader '%s' uses 'DOT3_RGB' in 'texEnvCombine' without 'requires GL_ARB_texture_env_dot3'\n", shader->name );
						return false;
					}
					MsgDev( D_WARN, "'DOT3_RGB' is not a valid 'alpha' equation for 'texEnvCombine' in shader '%s'\n", shader->name );
					return false;
				}
				else if (Com_MatchToken( "DOT3_RGBA"))
				{
					if (!GL_Support( R_DOT3_ARB_EXT ))
					{
						MsgDev( D_WARN, "shader '%s' uses 'DOT3_RGBA' in 'texEnvCombine' without 'requires GL_ARB_texture_env_dot3'\n", shader->name );
						return false;
					}
					MsgDev( D_WARN, "'DOT3_RGBA' is not a valid 'alpha' equation for 'texEnvCombine' in shader '%s'\n", shader->name );
					return false;
				}
				else
				{
					MsgDev( D_WARN, "unknown 'alpha' equation name '%s' for 'texEnvCombine' in shader '%s'\n", tok, shader->name );
					return false;
				}

				tok = Com_ParseToken( script, false );
				if (!Com_MatchToken( "("))
				{
					MsgDev( D_WARN, "expected '(', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok, shader->name );
					return false;
				}

				for( i = 0; i < numArgs; i++ )
				{
					tok = Com_ParseToken( script, false );
					if (!tok[0])
					{
						MsgDev( D_WARN, "missing 'alpha' equation arguments for 'texEnvCombine' in shader '%s'\n", shader->name );
						return false;
					}

					if (Com_MatchToken( "At"))
					{
						bundle->texEnvCombine.alphaSource[i] = GL_TEXTURE;
						bundle->texEnvCombine.alphaOperand[i] = GL_SRC_ALPHA;
					}
					else if (Com_MatchToken( "1-At"))
					{
						bundle->texEnvCombine.alphaSource[i] = GL_TEXTURE;
						bundle->texEnvCombine.alphaOperand[i] = GL_ONE_MINUS_SRC_ALPHA;
					}
					else if (Com_MatchToken( "Ac"))
					{
						bundle->texEnvCombine.alphaSource[i] = GL_CONSTANT_ARB;
						bundle->texEnvCombine.alphaOperand[i] = GL_SRC_ALPHA;
					}
					else if (Com_MatchToken( "1-Ac"))
					{
						bundle->texEnvCombine.alphaSource[i] = GL_CONSTANT_ARB;
						bundle->texEnvCombine.alphaOperand[i] = GL_ONE_MINUS_SRC_ALPHA;
					}
					else if (Com_MatchToken( "Af"))
					{
						bundle->texEnvCombine.alphaSource[i] = GL_PRIMARY_COLOR_ARB;
						bundle->texEnvCombine.alphaOperand[i] = GL_SRC_ALPHA;
					}
					else if (Com_MatchToken( "1-Af"))
					{
						bundle->texEnvCombine.alphaSource[i] = GL_PRIMARY_COLOR_ARB;
						bundle->texEnvCombine.alphaOperand[i] = GL_ONE_MINUS_SRC_ALPHA;
					}
					else if (Com_MatchToken( "Ap"))
					{
						bundle->texEnvCombine.alphaSource[i] = GL_PREVIOUS_ARB;
						bundle->texEnvCombine.alphaOperand[i] = GL_SRC_ALPHA;
					}
					else if (Com_MatchToken( "1-Ap"))
					{
						bundle->texEnvCombine.alphaSource[i] = GL_PREVIOUS_ARB;
						bundle->texEnvCombine.alphaOperand[i] = GL_ONE_MINUS_SRC_ALPHA;
					}
					else
					{
						MsgDev( D_WARN, "unknown 'alpha' equation argument '%s' for 'texEnvCombine' in shader '%s'\n", tok, shader->name );
						return false;
					}

					if( i < numArgs - 1 )
					{
						tok = Com_ParseToken( script, false );
						if (!Com_MatchToken( ","))
						{
							MsgDev( D_WARN, "expected ',', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok, shader->name );
							return false;
						}
					}
				}

				tok = Com_ParseToken( script, false );
				if (!Com_MatchToken( ")"))
				{
					MsgDev( D_WARN, "expected ')', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok, shader->name );
					return false;
				}

				tok = Com_ParseToken( script, false );
				if( tok[0] )
				{
					if (!Com_MatchToken( "*"))
					{
						MsgDev( D_WARN, "expected '*', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok, shader->name );
						return false;
					}

					tok = Com_ParseToken( script, false );
					if( !tok[0] )
					{
						MsgDev( D_WARN, "missing scale value for 'texEnvCombine' equation in shader '%s'\n", shader->name );
						return false;
					}
					bundle->texEnvCombine.alphaScale = com.atoi( tok );

					if( bundle->texEnvCombine.alphaScale != 1 && bundle->texEnvCombine.alphaScale != 2 && bundle->texEnvCombine.alphaScale != 4 )
					{
						MsgDev( D_WARN, "invalid scale value of %i for 'texEnvCombine' equation in shader '%s'\n", bundle->texEnvCombine.alphaScale, shader->name );
						bundle->texEnvCombine.alphaScale = 1;
					}
				}
			}
			else if (Com_MatchToken( "const"))
			{
				tok = Com_ParseToken( script, false );
				if(!Com_MatchToken( "="))
				{
					MsgDev( D_WARN, "expected '=', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok, shader->name );
					return false;
				}

				tok = Com_ParseToken( script, false );
				if (!Com_MatchToken( "("))
				{
					MsgDev( D_WARN, "expected '(', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok, shader->name );
					return false;
				}

				for( i = 0; i < 4; i++ )
				{
					tok = Com_ParseToken( script, false );
					if( !tok[0] )
					{
						MsgDev( D_WARN, "missing 'const' color value for 'texEnvCombine' in shader '%s'\n", shader->name );
						return false;
					}
					bundle->texEnvCombine.constColor[i] = bound( 0.0, com.atof( tok ), 1.0 );
				}

				tok = Com_ParseToken( script, false );
				if (!Com_MatchToken( ")"))
				{
					MsgDev( D_WARN, "expected ')', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok, shader->name );
					return false;
				}

				tok = Com_ParseToken( script, false );
				if( tok[0] )
				{
					if (!Com_MatchToken( "*"))
					{
						MsgDev( D_WARN, "expected '*', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok, shader->name );
						return false;
					}

					tok = Com_ParseToken( script, false );
					if (!Com_MatchToken( "identityLighting"))
					{
						MsgDev( D_WARN, "'const' color for 'texEnvCombine' can only be scaled by 'identityLighting' in shader '%s'\n", shader->name );
						return false;
					}

					if( gl_config.deviceSupportsGamma )
					{
						for( i = 0; i < 3; i++ )
							bundle->texEnvCombine.constColor[i] *= (1.0 / (float)(1<<r_overbrightbits->integer));
					}
				}
			}
			else
			{
				MsgDev( D_WARN, "unknown 'texEnvCombine' parameter '%s' in shader '%s'\n", tok, shader->name );
				return false;
			}
		}
	}
	else
	{
		MsgDev( D_WARN, "expected '{', found '%s' instead in 'texEnvCombine' in shader '%s'\n", tok, shader->name );
		return false;
	}
	bundle->flags |= STAGEBUNDLE_TEXENVCOMBINE;

	return true;
}

/*
=================
R_ParseStageTcGen
=================
*/
static bool R_ParseStageTcGen( shader_t *shader, shaderStage_t *stage, char **script )
{
	stageBundle_t	*bundle = stage->bundles[stage->numBundles - 1];
	char		*tok;
	int		i, j;

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'tcGen' in shader '%s'\n", shader->name );
		return false;
	}

	if( Com_MatchToken( "base" ) || Com_MatchToken( "texture" ))
		bundle->tcGen.type = TCGEN_BASE;
	else if( Com_MatchToken( "lightmap"))
		bundle->tcGen.type = TCGEN_LIGHTMAP;
	else if( Com_MatchToken( "environment"))
		bundle->tcGen.type = TCGEN_ENVIRONMENT;
	else if( Com_MatchToken( "vector"))
	{
		for( i = 0; i < 2; i++ )
		{
			tok = Com_ParseToken( script, false );
			if(!Com_MatchToken( "("))
			{
				MsgDev( D_WARN, "missing '(' for 'tcGen vector' in shader '%s'\n", shader->name );
				return false;
			}

			for( j = 0; j < 3; j++ )
			{
				tok = Com_ParseToken( script, false );
				if( !tok[0] )
				{
					MsgDev( D_WARN, "missing parameters for 'tcGen vector' in shader '%s'\n", shader->name );
					return false;
				}
				bundle->tcGen.params[i*3+j] = com.atof( tok );
			}

			tok = Com_ParseToken( script, false );
			if (!Com_MatchToken( ")"))
			{
				MsgDev( D_WARN, "missing ')' for 'tcGen vector' in shader '%s'\n", shader->name );
				return false;
			}
		}
		bundle->tcGen.type = TCGEN_VECTOR;
	}
	else if (Com_MatchToken( "warp")) bundle->tcGen.type = TCGEN_WARP;
	else if (Com_MatchToken( "lightVector")) bundle->tcGen.type = TCGEN_LIGHTVECTOR;
	else if (Com_MatchToken( "halfAngle")) bundle->tcGen.type = TCGEN_HALFANGLE;
	else if (Com_MatchToken( "reflection"))
	{
		if( !GL_Support( R_TEXTURECUBEMAP_EXT ))
		{
			MsgDev( D_WARN, "shader '%s' uses 'tcGen reflection' without 'requires GL_ARB_texture_cube_map'\n", shader->name );
			return false;
		}
		bundle->tcGen.type = TCGEN_REFLECTION;
	}
	else if (Com_MatchToken( "normal"))
	{
		if( !GL_Support( R_TEXTURECUBEMAP_EXT ))
		{
			MsgDev( D_WARN, "shader '%s' uses 'tcGen normal' without 'requires GL_ARB_texture_cube_map'\n", shader->name );
			return false;
		}
		bundle->tcGen.type = TCGEN_NORMAL;
	}
	else
	{
		MsgDev( D_WARN, "unknown 'tcGen' parameter '%s' in shader '%s'\n", tok, shader->name );
		return false;
	}
	bundle->flags |= STAGEBUNDLE_TCGEN;
	return true;
}

/*
=================
R_ParseStageTcMod
=================
*/
static bool R_ParseStageTcMod( shader_t *shader, shaderStage_t *stage, char **script )
{
	stageBundle_t	*bundle = stage->bundles[stage->numBundles - 1];
	tcMod_t		*tcMod;
	char		*tok;
	int		i;

	if( bundle->tcModNum == SHADER_MAX_TCMOD )
	{
		MsgDev( D_WARN, "SHADER_MAX_TCMOD hit in shader '%s'\n", shader->name );
		return false;
	}
	tcMod = &bundle->tcMod[bundle->tcModNum++];

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'tcMod' in shader '%s'\n", shader->name );
		return false;
	}

	if( Com_MatchToken( "translate" ))
	{
		for( i = 0; i < 2; i++ )
		{
			tok = Com_ParseToken( script, false );
			if( !tok[0] )
			{
				MsgDev( D_WARN, "missing parameters for 'tcMod translate' in shader '%s'\n", shader->name );
				return false;
			}
			tcMod->params[i] = com.atof( tok );
		}
		tcMod->type = TCMOD_TRANSLATE;
	}
	else if (Com_MatchToken( "scale" ))
	{
		for( i = 0; i < 2; i++ )
		{
			tok = Com_ParseToken( script, false );
			if( !tok[0] )
			{
				MsgDev( D_WARN, "missing parameters for 'tcMod scale' in shader '%s'\n", shader->name );
				return false;
			}
			tcMod->params[i] = com.atof( tok );
		}
		tcMod->type = TCMOD_SCALE;
	}
	else if (Com_MatchToken( "scroll"))
	{
		for( i = 0; i < 2; i++ )
		{
			tok = Com_ParseToken( script, false );
			if( !tok[0] )
			{
				MsgDev( D_WARN, "missing parameters for 'tcMod scroll' in shader '%s'\n", shader->name );
				return false;
			}
			tcMod->params[i] = com.atof( tok );
		}
		tcMod->type = TCMOD_SCROLL;
	}
	else if (Com_MatchToken( "rotate" ))
	{
		tok = Com_ParseToken( script, false );
		if( !tok[0] )
		{
			MsgDev( D_WARN, "missing parameters for 'tcMod rotate' in shader '%s'\n", shader->name );
			return false;
		}
		tcMod->params[0] = com.atof( tok );
		tcMod->type = TCMOD_ROTATE;
	}
	else if (Com_MatchToken( "stretch" ))
	{
		if(!R_ParseWaveFunc( shader, &tcMod->func, script ))
		{
			MsgDev( D_WARN, "missing waveform parameters for 'tcMod stretch' in shader '%s'\n", shader->name );
			return false;
		}
		tcMod->type = TCMOD_STRETCH;
	}
	else if (Com_MatchToken( "turb" ))
	{
		tcMod->func.type = WAVEFORM_SIN;

		for( i = 0; i < 4; i++ )
		{
			tok = Com_ParseToken( script, false );
			if( !tok[0] )
			{
				MsgDev( D_WARN, "missing parameters for 'tcMod turb' in shader '%s'\n", shader->name );
				return false;
			}
			tcMod->func.params[i] = com.atof( tok );
		}
		tcMod->type = TCMOD_TURB;
	}
	else if (Com_MatchToken( "transform" ))
	{
		for( i = 0; i < 6; i++ )
		{
			tok = Com_ParseToken( script, false );
			if( !tok[0] )
			{
				MsgDev( D_WARN, "missing parameters for 'tcMod transform' in shader '%s'\n", shader->name );
				return false;
			}
			tcMod->params[i] = com.atof( tok );
		}
		tcMod->type = TCMOD_TRANSFORM;
	}
	else
	{	
		MsgDev( D_WARN, "unknown 'tcMod' parameter '%s' in shader '%s'\n", tok, shader->name );
		return false;
	}
	bundle->flags |= STAGEBUNDLE_TCMOD;

	return true;
}

/*
=================
R_ParseStageNextBundle
=================
*/
static bool R_ParseStageNextBundle( shader_t *shader, shaderStage_t *stage, char **script )
{
	stageBundle_t	*bundle;
	char		*tok;

	if( !GL_Support( R_ARB_MULTITEXTURE ))
	{
		MsgDev( D_WARN, "shader '%s' uses 'nextBundle' without 'requires GL_ARB_multitexture'\n", shader->name );
		return false;
	}

	if( stage->flags & SHADERSTAGE_FRAGMENTPROGRAM )
	{
		if( stage->numBundles == gl_config.texturecoords )
		{
			MsgDev( D_WARN, "shader '%s' has %i or more bundles without suitable 'requires GL_MAX_TEXTURE_COORDS_ARB'\n", shader->name, stage->numBundles + 1);
			return false;
		}

		if( stage->numBundles == gl_config.imageunits )
		{
			MsgDev( D_WARN, "shader '%s' has %i or more bundles without suitable 'requires GL_MAX_TEXTURE_IMAGE_UNITS_ARB'\n", shader->name, stage->numBundles + 1);
			return false;
		}
	}
	else
	{
		if( stage->numBundles == gl_config.textureunits )
		{
			MsgDev( D_WARN, "shader '%s' has %i or more bundles without suitable 'requires GL_MAX_TEXTURE_UNITS_ARB'\n", shader->name, stage->numBundles + 1);
			return false;
		}
	}

	if( stage->numBundles == MAX_TEXTURE_UNITS )
	{
		MsgDev( D_WARN, "MAX_TEXTURE_UNITS hit in shader '%s'\n", shader->name );
		return false;
	}
	bundle = stage->bundles[stage->numBundles++];

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		if(!(stage->bundles[0]->flags & STAGEBUNDLE_TEXENVCOMBINE))
		{
			if((stage->flags & SHADERSTAGE_BLENDFUNC) && ((stage->blendFunc.src == GL_ONE && stage->blendFunc.dst == GL_ONE) && GL_Support( R_TEXTURE_ENV_ADD_EXT )))
				bundle->texEnv = GL_ADD;
			else bundle->texEnv = GL_MODULATE;
		}
		else
		{
			if((stage->flags & SHADERSTAGE_BLENDFUNC) && (stage->blendFunc.src == GL_ONE && stage->blendFunc.dst == GL_ONE))
			{
				bundle->texEnv = GL_COMBINE_ARB;
				bundle->texEnvCombine.rgbCombine = GL_ADD;
				bundle->texEnvCombine.rgbSource[0] = GL_TEXTURE;
				bundle->texEnvCombine.rgbSource[1] = GL_PREVIOUS_ARB;
				bundle->texEnvCombine.rgbSource[2] = GL_CONSTANT_ARB;
				bundle->texEnvCombine.rgbOperand[0] = GL_SRC_COLOR;
				bundle->texEnvCombine.rgbOperand[1] = GL_SRC_COLOR;
				bundle->texEnvCombine.rgbOperand[2] = GL_SRC_ALPHA;
				bundle->texEnvCombine.rgbScale = 1;
				bundle->texEnvCombine.alphaCombine = GL_ADD;
				bundle->texEnvCombine.alphaSource[0] = GL_TEXTURE;
				bundle->texEnvCombine.alphaSource[1] = GL_PREVIOUS_ARB;
				bundle->texEnvCombine.alphaSource[2] = GL_CONSTANT_ARB;
				bundle->texEnvCombine.alphaOperand[0] = GL_SRC_ALPHA;
				bundle->texEnvCombine.alphaOperand[1] = GL_SRC_ALPHA;
				bundle->texEnvCombine.alphaOperand[2] = GL_SRC_ALPHA;
				bundle->texEnvCombine.alphaScale = 1;
				bundle->texEnvCombine.constColor[0] = 1.0;
				bundle->texEnvCombine.constColor[1] = 1.0;
				bundle->texEnvCombine.constColor[2] = 1.0;
				bundle->texEnvCombine.constColor[3] = 1.0;
				bundle->flags |= STAGEBUNDLE_TEXENVCOMBINE;
			}
			else
			{
				bundle->texEnv = GL_COMBINE_ARB;
				bundle->texEnvCombine.rgbCombine = GL_MODULATE;
				bundle->texEnvCombine.rgbSource[0] = GL_TEXTURE;
				bundle->texEnvCombine.rgbSource[1] = GL_PREVIOUS_ARB;
				bundle->texEnvCombine.rgbSource[2] = GL_CONSTANT_ARB;
				bundle->texEnvCombine.rgbOperand[0] = GL_SRC_COLOR;
				bundle->texEnvCombine.rgbOperand[1] = GL_SRC_COLOR;
				bundle->texEnvCombine.rgbOperand[2] = GL_SRC_ALPHA;
				bundle->texEnvCombine.rgbScale = 1;
				bundle->texEnvCombine.alphaCombine = GL_MODULATE;
				bundle->texEnvCombine.alphaSource[0] = GL_TEXTURE;
				bundle->texEnvCombine.alphaSource[1] = GL_PREVIOUS_ARB;
				bundle->texEnvCombine.alphaSource[2] = GL_CONSTANT_ARB;
				bundle->texEnvCombine.alphaOperand[0] = GL_SRC_ALPHA;
				bundle->texEnvCombine.alphaOperand[1] = GL_SRC_ALPHA;
				bundle->texEnvCombine.alphaOperand[2] = GL_SRC_ALPHA;
				bundle->texEnvCombine.alphaScale = 1;
				bundle->texEnvCombine.constColor[0] = 1.0;
				bundle->texEnvCombine.constColor[1] = 1.0;
				bundle->texEnvCombine.constColor[2] = 1.0;
				bundle->texEnvCombine.constColor[3] = 1.0;
				bundle->flags |= STAGEBUNDLE_TEXENVCOMBINE;
			}
		}
	}
	else
	{
		if (Com_MatchToken( "modulate" )) bundle->texEnv = GL_MODULATE;
		else if (Com_MatchToken( "add" ))
		{
			if( !GL_Support( R_TEXTURE_ENV_ADD_EXT ))
			{
				MsgDev( D_WARN, "shader '%s' uses 'nextBundle add' without 'requires GL_ARB_texture_env_add'\n", shader->name );
				return false;
			}
			bundle->texEnv = GL_ADD;
		}
		else if (Com_MatchToken( "combine"))
		{
			if (!GL_Support( R_COMBINE_EXT ))
			{
				MsgDev( D_WARN, "shader '%s' uses 'nextBundle combine' without 'requires GL_ARB_texture_env_combine'\n", shader->name );
				return false;
			}

			bundle->texEnv = GL_COMBINE_ARB;
			bundle->texEnvCombine.rgbCombine = GL_MODULATE;
			bundle->texEnvCombine.rgbSource[0] = GL_TEXTURE;
			bundle->texEnvCombine.rgbSource[1] = GL_PREVIOUS_ARB;
			bundle->texEnvCombine.rgbSource[2] = GL_CONSTANT_ARB;
			bundle->texEnvCombine.rgbOperand[0] = GL_SRC_COLOR;
			bundle->texEnvCombine.rgbOperand[1] = GL_SRC_COLOR;
			bundle->texEnvCombine.rgbOperand[2] = GL_SRC_ALPHA;
			bundle->texEnvCombine.rgbScale = 1;
			bundle->texEnvCombine.alphaCombine = GL_MODULATE;
			bundle->texEnvCombine.alphaSource[0] = GL_TEXTURE;
			bundle->texEnvCombine.alphaSource[1] = GL_PREVIOUS_ARB;
			bundle->texEnvCombine.alphaSource[2] = GL_CONSTANT_ARB;
			bundle->texEnvCombine.alphaOperand[0] = GL_SRC_ALPHA;
			bundle->texEnvCombine.alphaOperand[1] = GL_SRC_ALPHA;
			bundle->texEnvCombine.alphaOperand[2] = GL_SRC_ALPHA;
			bundle->texEnvCombine.alphaScale = 1;
			bundle->texEnvCombine.constColor[0] = 1.0;
			bundle->texEnvCombine.constColor[1] = 1.0;
			bundle->texEnvCombine.constColor[2] = 1.0;
			bundle->texEnvCombine.constColor[3] = 1.0;
			bundle->flags |= STAGEBUNDLE_TEXENVCOMBINE;
		}
		else
		{
			MsgDev( D_WARN, "unknown 'nextBundle' parameter '%s' in shader '%s'\n", tok, shader->name );
			return false;
		}
	}
	stage->flags |= SHADERSTAGE_NEXTBUNDLE;
	return true;
}

/*
=================
R_ParseStageVertexProgram
=================
*/
static bool R_ParseStageVertexProgram( shader_t *shader, shaderStage_t *stage, char **script )
{
	char	*tok;

	if( !GL_Support( R_VERTEX_PROGRAM_EXT ))
	{
		MsgDev( D_WARN, "shader '%s' uses 'vertexProgram' without 'requires GL_ARB_vertex_program'\n", shader->name );
		return false;
	}

	if( shader->shaderType == SHADER_NOMIP )
	{
		MsgDev( D_WARN, "'vertexProgram' not allowed in shader '%s'\n", shader->name );
		return false;
	}

	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		MsgDev( D_WARN, "'vertexProgram' not allowed in 'nextBundle' in shader '%s'\n", shader->name );
		return false;
	}

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'vertexProgram' in shader '%s'\n", shader->name );
		return false;
	}

	stage->vertexProgram = R_FindProgram( tok, GL_VERTEX_PROGRAM_ARB );
	if( !stage->vertexProgram )
	{
		MsgDev( D_WARN, "couldn't find vertex program '%s' in shader '%s'\n", tok, shader->name );
		return false;
	}
	stage->flags |= SHADERSTAGE_VERTEXPROGRAM;

	return true;
}

/*
=================
R_ParseStageFragmentProgram
=================
*/
static bool R_ParseStageFragmentProgram( shader_t *shader, shaderStage_t *stage, char **script )
{
	char	*tok;

	if (!GL_Support( R_FRAGMENT_PROGRAM_EXT ))
	{
		MsgDev( D_WARN, "shader '%s' uses 'fragmentProgram' without 'requires GL_ARB_fragment_program'\n", shader->name );
		return false;
	}

	if( shader->shaderType == SHADER_NOMIP )
	{
		MsgDev( D_WARN, "'fragmentProgram' not allowed in shader '%s'\n", shader->name );
		return false;
	}

	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		MsgDev( D_WARN, "'fragmentProgram' not allowed in 'nextBundle' in shader '%s'\n", shader->name );
		return false;
	}

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'fragmentProgram' in shader '%s'\n", shader->name );
		return false;
	}

	stage->fragmentProgram = R_FindProgram( tok, GL_FRAGMENT_PROGRAM_ARB );
	if( !stage->fragmentProgram )
	{
		MsgDev( D_WARN, "couldn't find fragment program '%s' in shader '%s'\n", tok, shader->name );
		return false;
	}
	stage->flags |= SHADERSTAGE_FRAGMENTPROGRAM;

	return true;
}

/*
=================
R_ParseStageAlphaFunc
=================
*/
static bool R_ParseStageAlphaFunc( shader_t *shader, shaderStage_t *stage, char **script )
{
	char	*tok;

	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		MsgDev( D_WARN, "'alphaFunc' not allowed in 'nextBundle' in shader '%s'\n", shader->name );
		return false;
	}

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'alphaFunc' in shader '%s'\n", shader->name );
		return false;
	}

	if( Com_MatchToken( "GT0" ))
	{
		stage->alphaFunc.func = GL_GREATER;
		stage->alphaFunc.ref = 0.0;
	}
	else if (Com_MatchToken( "LT128"))
	{
		stage->alphaFunc.func = GL_LESS;
		stage->alphaFunc.ref = 0.5;
	}
	else if (Com_MatchToken( "GE128"))
	{
		stage->alphaFunc.func = GL_GEQUAL;
		stage->alphaFunc.ref = 0.5;
	}
	else
	{
		if (Com_MatchToken( "GL_NEVER"))
			stage->alphaFunc.func = GL_NEVER;
		else if (Com_MatchToken( "GL_ALWAYS"))
			stage->alphaFunc.func = GL_ALWAYS;
		else if (Com_MatchToken( "GL_EQUAL"))
			stage->alphaFunc.func = GL_EQUAL;
		else if (Com_MatchToken( "GL_NOTEQUAL"))
			stage->alphaFunc.func = GL_NOTEQUAL;
		else if (Com_MatchToken( "GL_LEQUAL"))
			stage->alphaFunc.func = GL_LEQUAL;
		else if (Com_MatchToken( "GL_GEQUAL"))
			stage->alphaFunc.func = GL_GEQUAL;
		else if (Com_MatchToken( "GL_LESS"))
			stage->alphaFunc.func = GL_LESS;
		else if (Com_MatchToken( "GL_GREATER"))
			stage->alphaFunc.func = GL_GREATER;
		else
		{
			MsgDev( D_WARN, "unknown 'alphaFunc' parameter '%s' in shader '%s', defaulting to GL_GREATER\n", tok, shader->name );
			stage->alphaFunc.func = GL_GREATER;
		}

		tok = Com_ParseToken( script, false );
		if( !tok[0] )
		{
			MsgDev( D_WARN, "missing parameters for 'alphaFunc' in shader '%s'\n", shader->name );
			return false;
		}
		stage->alphaFunc.ref = bound( 0.0, com.atof( tok ), 1.0 );
	}
	stage->flags |= SHADERSTAGE_ALPHAFUNC;

	return true;
}

/*
=================
R_ParseStageBlendFunc
=================
*/
static bool R_ParseStageBlendFunc( shader_t *shader, shaderStage_t *stage, char **script )
{
	char	*tok;

	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		MsgDev( D_WARN, "'blendFunc' not allowed in 'nextBundle' in shader '%s'\n", shader->name );
		return false;
	}

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'blendFunc' in shader '%s'\n", shader->name );
		return false;
	}

	if (Com_MatchToken( "add"))
	{
		stage->blendFunc.src = GL_ONE;
		stage->blendFunc.dst = GL_ONE;
	}
	else if (Com_MatchToken( "filter"))
	{
		stage->blendFunc.src = GL_DST_COLOR;
		stage->blendFunc.dst = GL_ZERO;
	}
	else if (Com_MatchToken( "blend"))
	{
		stage->blendFunc.src = GL_SRC_ALPHA;
		stage->blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
	}
	else
	{
		if (Com_MatchToken( "GL_ZERO"))
			stage->blendFunc.src = GL_ZERO;
		else if (Com_MatchToken( "GL_ONE"))
			stage->blendFunc.src = GL_ONE;
		else if (Com_MatchToken( "GL_DST_COLOR"))
			stage->blendFunc.src = GL_DST_COLOR;
		else if (Com_MatchToken( "GL_ONE_MINUS_DST_COLOR"))
			stage->blendFunc.src = GL_ONE_MINUS_DST_COLOR;
		else if (Com_MatchToken( "GL_SRC_ALPHA"))
			stage->blendFunc.src = GL_SRC_ALPHA;
		else if (Com_MatchToken( "GL_ONE_MINUS_SRC_ALPHA"))
			stage->blendFunc.src = GL_ONE_MINUS_SRC_ALPHA;
		else if (Com_MatchToken( "GL_DST_ALPHA"))
			stage->blendFunc.src = GL_DST_ALPHA;
		else if (Com_MatchToken( "GL_ONE_MINUS_DST_ALPHA"))
			stage->blendFunc.src = GL_ONE_MINUS_DST_ALPHA;
		else if (Com_MatchToken( "GL_SRC_ALPHA_SATURATE"))
			stage->blendFunc.src = GL_SRC_ALPHA_SATURATE;
		else
		{
			MsgDev( D_WARN, "unknown 'blendFunc' parameter '%s' in shader '%s', defaulting to GL_ONE\n", tok, shader->name );
			stage->blendFunc.src = GL_ONE;
		}

		tok = Com_ParseToken( script, false );
		if( !tok[0] )
		{
			MsgDev( D_WARN, "missing parameters for 'blendFunc' in shader '%s'\n", shader->name );
			return false;
		}

		if (Com_MatchToken( "GL_ZERO"))
			stage->blendFunc.dst = GL_ZERO;
		else if (Com_MatchToken( "GL_ONE"))
			stage->blendFunc.dst = GL_ONE;
		else if (Com_MatchToken( "GL_SRC_COLOR"))
			stage->blendFunc.dst = GL_SRC_COLOR;
		else if (Com_MatchToken( "GL_ONE_MINUS_SRC_COLOR"))
			stage->blendFunc.dst = GL_ONE_MINUS_SRC_COLOR;
		else if (Com_MatchToken( "GL_SRC_ALPHA"))
			stage->blendFunc.dst = GL_SRC_ALPHA;
		else if (Com_MatchToken( "GL_ONE_MINUS_SRC_ALPHA"))
			stage->blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
		else if (Com_MatchToken( "GL_DST_ALPHA"))
			stage->blendFunc.dst = GL_DST_ALPHA;
		else if (Com_MatchToken( "GL_ONE_MINUS_DST_ALPHA"))
			stage->blendFunc.dst = GL_ONE_MINUS_DST_ALPHA;
		else
		{
			MsgDev( D_WARN, "unknown 'blendFunc' parameter '%s' in shader '%s', defaulting to GL_ONE\n", tok, shader->name );
			stage->blendFunc.dst = GL_ONE;
		}
	}
	stage->flags |= SHADERSTAGE_BLENDFUNC;

	return true;
}

/*
 =================
 R_ParseStageDepthFunc
 =================
*/
static bool R_ParseStageDepthFunc( shader_t *shader, shaderStage_t *stage, char **script )
{
	char	*tok;

	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		MsgDev( D_WARN, "'depthFunc' not allowed in 'nextBundle' in shader '%s'\n", shader->name );
		return false;
	}

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'depthFunc' in shader '%s'\n", shader->name );
		return false;
	}

	if (Com_MatchToken( "lequal" ))
		stage->depthFunc.func = GL_LEQUAL;
	else if (Com_MatchToken( "equal" ))
		stage->depthFunc.func = GL_EQUAL;
	else
	{
		MsgDev( D_WARN, "unknown 'depthFunc' parameter '%s' in shader '%s'\n", tok, shader->name );
		return false;
	}
	stage->flags |= SHADERSTAGE_DEPTHFUNC;

	return true;
}

/*
=================
R_ParseStageDepthWrite
=================
*/
static bool R_ParseStageDepthWrite( shader_t *shader, shaderStage_t *stage, char **script )
{
	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		MsgDev( D_WARN, "'depthWrite' not allowed in 'nextBundle' in shader '%s'\n", shader->name );
		return false;
	}
	stage->flags |= SHADERSTAGE_DEPTHWRITE;
	return true;
}

/*
=================
R_ParseStageDetail
=================
*/
static bool R_ParseStageDetail( shader_t *shader, shaderStage_t *stage, char **script )
{
	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		MsgDev( D_WARN, "'detail' not allowed in 'nextBundle' in shader '%s'\n", shader->name );
		return false;
	}
	stage->flags |= SHADERSTAGE_DETAIL;

	return true;
}

/*
=================
R_ParseStageRgbGen
=================
*/
static bool R_ParseStageRgbGen( shader_t *shader, shaderStage_t *stage, char **script )
{
	char	*tok;
	int	i;

	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		MsgDev( D_WARN, "'rgbGen' not allowed in 'nextBundle' in shader '%s'\n", shader->name );
		return false;
	}

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'rgbGen' in shader '%s'\n", shader->name );
		return false;
	}

	if (Com_MatchToken( "identity" ))
		stage->rgbGen.type = RGBGEN_IDENTITY;
	else if (Com_MatchToken( "identityLighting" ))
		stage->rgbGen.type = RGBGEN_IDENTITYLIGHTING;
	else if (Com_MatchToken( "wave" ))
	{
		if(!R_ParseWaveFunc( shader, &stage->rgbGen.func, script ))
		{
			MsgDev( D_WARN, "missing waveform parameters for 'rgbGen wave' in shader '%s'\n", shader->name );
			return false;
		}
		stage->rgbGen.type = RGBGEN_WAVE;
	}
	else if (Com_MatchToken( "colorWave"))
	{
		for( i = 0; i < 3; i++ )
		{
			tok = Com_ParseToken( script, false );
			if( !tok[0] )
			{
				MsgDev( D_WARN, "missing parameters for 'rgbGen colorWave' in shader '%s'\n", shader->name );
				return false;
			}
			stage->rgbGen.params[i] = bound( 0.0, com.atof( tok ), 1.0 );
		}

		if( !R_ParseWaveFunc(shader, &stage->rgbGen.func, script ))
		{
			MsgDev( D_WARN, "missing waveform parameters for 'rgbGen colorWave' in shader '%s'\n", shader->name );
			return false;
		}
		stage->rgbGen.type = RGBGEN_COLORWAVE;
	}
	else if (Com_MatchToken( "vertex"))
		stage->rgbGen.type = RGBGEN_VERTEX;
	else if (Com_MatchToken( "oneMinusVertex"))
		stage->rgbGen.type = RGBGEN_ONEMINUSVERTEX;
	else if (Com_MatchToken( "entity"))
		stage->rgbGen.type = RGBGEN_ENTITY;
	else if (Com_MatchToken( "oneMinusEntity"))
		stage->rgbGen.type = RGBGEN_ONEMINUSENTITY;
	else if (Com_MatchToken( "lightingAmbient"))
		stage->rgbGen.type = RGBGEN_LIGHTINGAMBIENT;
	else if (Com_MatchToken( "lightingDiffuse"))
		stage->rgbGen.type = RGBGEN_LIGHTINGDIFFUSE;
	else if (Com_MatchToken( "const") || Com_MatchToken( "constant"))
	{
		for( i = 0; i < 3; i++ )
		{
			tok = Com_ParseToken( script, false );
			if( !tok[0] )
			{
				MsgDev( D_WARN, "missing parameters for 'rgbGen const' in shader '%s'\n", shader->name );
				return false;
			}
			stage->rgbGen.params[i] = bound( 0.0, com.atof( tok ), 1.0 );
		}
		stage->rgbGen.type = RGBGEN_CONST;
	}
	else
	{
		MsgDev( D_WARN, "unknown 'rgbGen' parameter '%s' in shader '%s'\n", tok, shader->name );
		return false;
	}
	stage->flags |= SHADERSTAGE_RGBGEN;

	return true;
}

/*
=================
R_ParseStageAlphaGen
=================
*/
static bool R_ParseStageAlphaGen( shader_t *shader, shaderStage_t *stage, char **script )
{
	char	*tok;

	if( stage->flags & SHADERSTAGE_NEXTBUNDLE )
	{
		MsgDev( D_WARN, "'alphaGen' not allowed in 'nextBundle' in shader '%s'\n", shader->name );
		return false;
	}

	tok = Com_ParseToken( script, false );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "missing parameters for 'alphaGen' in shader '%s'\n", shader->name );
		return false;
	}

	if (Com_MatchToken( "identity" ))
		stage->alphaGen.type = ALPHAGEN_IDENTITY;
	else if (Com_MatchToken( "wave" ))
	{
		if(!R_ParseWaveFunc( shader, &stage->alphaGen.func, script ))
		{
			MsgDev( D_WARN, "missing waveform parameters for 'alphaGen wave' in shader '%s'\n", shader->name );
			return false;
		}
		stage->alphaGen.type = ALPHAGEN_WAVE;
	}
	else if (Com_MatchToken( "alphaWave"))
	{
		tok = Com_ParseToken( script, false );
		if( !tok[0] )
		{
			MsgDev( D_WARN, "missing parameters for 'alphaGen alphaWave' in shader '%s'\n", shader->name );
			return false;
		}

		stage->alphaGen.params[0] = bound( 0.0, com.atof( tok ), 1.0 );

		if(!R_ParseWaveFunc( shader, &stage->alphaGen.func, script ))
		{
			MsgDev( D_WARN, "missing waveform parameters for 'alphaGen alphaWave' in shader '%s'\n", shader->name );
			return false;
		}
		stage->alphaGen.type = ALPHAGEN_ALPHAWAVE;
	}
	else if (Com_MatchToken( "vertex"))
		stage->alphaGen.type = ALPHAGEN_VERTEX;
	else if (Com_MatchToken( "oneMinusVertex"))
		stage->alphaGen.type = ALPHAGEN_ONEMINUSVERTEX;
	else if (Com_MatchToken( "entity"))
		stage->alphaGen.type = ALPHAGEN_ENTITY;
	else if (Com_MatchToken( "oneMinusEntity"))
		stage->alphaGen.type = ALPHAGEN_ONEMINUSENTITY;
	else if (Com_MatchToken( "dot"))
	{
		tok = Com_ParseToken( script, false );
		if( !tok[0] )
		{
			stage->alphaGen.params[0] = 0.0;
			stage->alphaGen.params[1] = 1.0;
		}
		else
		{
			stage->alphaGen.params[0] = bound( 0.0, com.atof(tok), 1.0 );
			tok = Com_ParseToken( script, false );

			if( !tok[0] )
			{
				MsgDev( D_WARN, "missing parameters for 'alphaGen dot' in shader '%s'\n", shader->name );
				return false;
			}
			stage->alphaGen.params[1] = bound( 0.0, com.atof(tok), 1.0 );
		}
		stage->alphaGen.type = ALPHAGEN_DOT;
	}
	else if (Com_MatchToken( "oneMinusDot" ))
	{
		tok = Com_ParseToken( script, false );
		if( !tok[0] )
		{
			stage->alphaGen.params[0] = 0.0;
			stage->alphaGen.params[1] = 1.0;
		}
		else
		{
			stage->alphaGen.params[0] = bound( 0.0, com.atof(tok), 1.0 );

			tok = Com_ParseToken( script, false );
			if( !tok[0] )
			{
				MsgDev( D_WARN, "missing parameters for 'alphaGen oneMinusDot' in shader '%s'\n", shader->name );
				return false;
			}
			stage->alphaGen.params[1] = bound( 0.0, com.atof(tok), 1.0 );
		}
		stage->alphaGen.type = ALPHAGEN_ONEMINUSDOT;
	}
	else if (Com_MatchToken( "fade" ))
	{
		tok = Com_ParseToken( script, false );
		if( !tok[0] )
		{
			stage->alphaGen.params[0] = 0.0;
			stage->alphaGen.params[1] = 256.0;
			stage->alphaGen.params[2] = 1.0 / 256.0;
		}
		else
		{
			stage->alphaGen.params[0] = com.atof( tok );
			tok = Com_ParseToken( script, false );

			if( !tok[0] )
			{
				MsgDev( D_WARN, "missing parameters for 'alphaGen fade' in shader '%s'\n", shader->name );
				return false;
			}

			stage->alphaGen.params[1] = com.atof( tok );
			stage->alphaGen.params[2] = stage->alphaGen.params[1] - stage->alphaGen.params[0];
			if( stage->alphaGen.params[2] ) stage->alphaGen.params[2] = 1.0 / stage->alphaGen.params[2];
		}
		stage->alphaGen.type = ALPHAGEN_FADE;
	}
	else if (Com_MatchToken( "oneMinusFade" ))
	{
		tok = Com_ParseToken( script, false );
		if( !tok[0] )
		{
			stage->alphaGen.params[0] = 0.0;
			stage->alphaGen.params[1] = 256.0;
			stage->alphaGen.params[2] = 1.0 / 256.0;
		}
		else {
			stage->alphaGen.params[0] = com.atof( tok );

			tok = Com_ParseToken( script, false );
			if( !tok[0] )
			{
				MsgDev( D_WARN, "missing parameters for 'alphaGen oneMinusFade' in shader '%s'\n", shader->name );
				return false;
			}

			stage->alphaGen.params[1] = com.atof( tok );
			stage->alphaGen.params[2] = stage->alphaGen.params[1] - stage->alphaGen.params[0];
			if( stage->alphaGen.params[2] ) stage->alphaGen.params[2] = 1.0 / stage->alphaGen.params[2];
		}
		stage->alphaGen.type = ALPHAGEN_ONEMINUSFADE;
	}
	else if (Com_MatchToken( "lightingSpecular" ))
	{
		tok = Com_ParseToken( script, false );
		if( !tok[0] ) stage->alphaGen.params[0] = 5.0;
		else
		{
			stage->alphaGen.params[0] = com.atof( tok );
			if( stage->alphaGen.params[0] <= 0.0 )
			{
				MsgDev( D_WARN, "invalid exponent value of %f for 'alphaGen lightingSpecular' in shader '%s', defaulting to 5\n", stage->alphaGen.params[0], shader->name );
				stage->alphaGen.params[0] = 5.0;
			}
		}
		stage->alphaGen.type = ALPHAGEN_LIGHTINGSPECULAR;
	}
	else if (Com_MatchToken( "const" ) || Com_MatchToken( "constant" ))
	{
		tok = Com_ParseToken( script, false );
		if( !tok[0] )
		{
			MsgDev( D_WARN, "missing parameters for 'alphaGen const' in shader '%s'\n", shader->name );
			return false;
		}
		stage->alphaGen.params[0] = bound( 0.0, com.atof(tok), 1.0 );
		stage->alphaGen.type = ALPHAGEN_CONST;
	}
	else
	{
		MsgDev( D_WARN, "unknown 'alphaGen' parameter '%s' in shader '%s'\n", tok, shader->name );
		return false;
	}
	stage->flags |= SHADERSTAGE_ALPHAGEN;

	return true;
}


// =====================================================================

typedef struct
{
	const char	*name;
	bool		(*parseFunc)( shader_t *shader, char **script );
} shaderGeneralCmd_t;

typedef struct
{
	const char	*name;
	bool		(*parseFunc)( shader_t *shader, shaderStage_t *stage, char **script );
} shaderStageCmd_t;

static shaderGeneralCmd_t r_shaderGeneralCmds[] =
{
{"surfaceParm",	R_ParseGeneralSurfaceParm	},
{"noMipmaps",	R_ParseGeneralNoMipmaps	},
{"noPicmip",	R_ParseGeneralNoPicmip	},
{"noCompress",	R_ParseGeneralNoCompress	},
{"noShadows",	R_ParseGeneralNoShadows	},
{"noFragments",	R_ParseGeneralNoFragments	},
{"entityMergable",	R_ParseGeneralEntityMergable	},
{"polygonOffset",	R_ParseGeneralPolygonOffset	},
{"cull",		R_ParseGeneralCull		},
{"sort",		R_ParseGeneralSort		},
{"tessSize",	R_ParseGeneralTessSize	},
{"skyParms",	R_ParseGeneralSkyParms	},
{"deformVertexes",	R_ParseGeneralDeformVertexes	},
{NULL, NULL }	// terminator
};

static shaderStageCmd_t r_shaderStageCmds[] =
{
{"requires",	R_ParseStageRequires	},
{"noMipmaps",	R_ParseStageNoMipmaps	},
{"noPicmip",	R_ParseStageNoPicmip	},
{"noCompress",	R_ParseStageNoCompress	},
{"clampTexCoords",	R_ParseStageClampTexCoords	},
{"animFrequency",	R_ParseStageAnimFrequency	},
{"map",		R_ParseStageMap		},
{"bumpMap",	R_ParseStageBumpMap		},
{"cubeMap",	R_ParseStageCubeMap		},
{"videoMap",	R_ParseStageVideoMap	},
{"texEnvCombine",	R_ParseStageTexEnvCombine	},
{"tcGen",		R_ParseStageTcGen		},
{"tcMod",		R_ParseStageTcMod		},
{"nextBundle",	R_ParseStageNextBundle	},
{"vertexProgram",	R_ParseStageVertexProgram	},
{"fragmentProgram",	R_ParseStageFragmentProgram	},
{"alphaFunc",	R_ParseStageAlphaFunc	},
{"blendFunc",	R_ParseStageBlendFunc	},
{"depthFunc",	R_ParseStageDepthFunc	},
{"depthWrite",	R_ParseStageDepthWrite	},
{"detail",	R_ParseStageDetail		},
{"rgbGen",	R_ParseStageRgbGen		},
{"alphaGen",	R_ParseStageAlphaGen	},
{NULL, NULL }	// terminator
};

/*
=================
R_ParseShaderCommand
=================
*/
static bool R_ParseShaderCommand( shader_t *shader, char **script, char *command )
{
	shaderGeneralCmd_t	*cmd;

	for( cmd = r_shaderGeneralCmds; cmd->name != NULL; cmd++ )
	{
		if( !com.stricmp( cmd->name, command ))
			return cmd->parseFunc( shader, script );
	}

	// compiler commands ignored silently
	if( !com.strnicmp( "q3map_", command, 6 )) return true;
	MsgDev( D_WARN, "unknown general command '%s' in shader '%s'\n", command, shader->name );
	return false;
}

/*
=================
R_ParseShaderStageCommand
=================
*/
static bool R_ParseShaderStageCommand( shader_t *shader, shaderStage_t *stage, char **script, char *command )
{
	shaderStageCmd_t	*cmd;

	for( cmd = r_shaderStageCmds; cmd->name != NULL; cmd++ )
	{
		if(!com.stricmp( cmd->name, command ))
			return cmd->parseFunc( shader, stage, script );
	}
	MsgDev( D_WARN, "unknown stage command '%s' in shader '%s'\n", command, shader->name );
	return false;
}

/*
=================
R_EvaluateRequires
=================
*/
static bool R_EvaluateRequires( shader_t *shader, char **script )
{
	bool	results[SHADER_MAX_EXPRESSIONS];
	bool	logicAnd = false, logicOr = false;
	bool	negate, expectingExpression = false;
	char	*tok;
	char	cmpOperator[8];
	int	cmpOperand1, cmpOperand2;
	int	i, count = 0;

	// Parse the expressions
	while( 1 )
	{
		tok = Com_ParseToken( script, false );
		if( !tok[0] )
		{
			if( count == 0 || expectingExpression )
			{
				MsgDev( D_WARN, "missing expression for 'requires' in shader '%s', discarded stage\n", shader->name );
				return false;
			}
			break; // end of data
		}

		if( Com_MatchToken( "&&" ))
		{
			if( count == 0 || expectingExpression )
			{
				MsgDev( D_WARN, "'requires' has logical operator without preceding expression in shader '%s', discarded stage\n", shader->name );
				return false;
			}
			logicAnd = true;
			expectingExpression = true;
			continue;
		}
		if( Com_MatchToken( "||" ))
		{
			if( count == 0 || expectingExpression )
			{
				MsgDev( D_WARN, "'requires' has logical operator without preceding expression in shader '%s', discarded stage\n", shader->name );
				return false;
			}
			logicOr = true;
			expectingExpression = true;
			continue;
		}

		expectingExpression = false;
		if( count == SHADER_MAX_EXPRESSIONS )
		{
			MsgDev( D_WARN, "SHADER_MAX_EXPRESSIONS hit in shader '%s', discarded stage\n", shader->name );
			return false;
		}

		if( Com_MatchToken( "GL_MAX_TEXTURE_UNITS_ARB") || Com_MatchToken( "GL_MAX_TEXTURE_COORDS_ARB") || Com_MatchToken( "GL_MAX_TEXTURE_IMAGE_UNITS_ARB"))
		{
			if( Com_MatchToken( "GL_MAX_TEXTURE_UNITS_ARB" ))
				cmpOperand1 = gl_config.textureunits;
			else if( Com_MatchToken( "GL_MAX_TEXTURE_COORDS_ARB"))
				cmpOperand1 = gl_config.texturecoords;
			else if (Com_MatchToken( "GL_MAX_TEXTURE_IMAGE_UNITS_ARB" ))
				cmpOperand1 = gl_config.imageunits;

			tok = Com_ParseToken( script, false );
			if( !tok[0] )
			{
				MsgDev( D_WARN, "missing operator for 'requires' expression in shader '%s', discarded stage\n", shader->name );
				return false;
			}
			com.strncpy( cmpOperator, tok, sizeof( cmpOperator ));

			tok = Com_ParseToken( script, false );
			if( !tok[0] )
			{
				MsgDev( D_WARN, "missing operand for 'requires' expression in shader '%s', discarded stage\n", shader->name );
				return false;
			}
			cmpOperand2 = com.atoi( tok );

			if( !com.stricmp( cmpOperator, "==" ))
				results[count] = (cmpOperand1 == cmpOperand2);
			else if (!com.stricmp( cmpOperator, "!=" ))
				results[count] = (cmpOperand1 != cmpOperand2);
			else if (!com.stricmp( cmpOperator, ">=" ))
				results[count] = (cmpOperand1 >= cmpOperand2);
			else if (!com.stricmp( cmpOperator, "<=" ))
				results[count] = (cmpOperand1 <= cmpOperand2);
			else if (!com.stricmp( cmpOperator, ">" ))
				results[count] = (cmpOperand1 > cmpOperand2);
			else if (!com.stricmp( cmpOperator, "<" ))
				results[count] = (cmpOperand1 < cmpOperand2);
			else
			{
				MsgDev( D_WARN, "unknown 'requires' operator '%s' in shader '%s', discarded stage\n", cmpOperator, shader->name );
				return false;
			}
		}
		else
		{
			if( Com_MatchToken( "!" ))
			{
				negate = true;
				tok = Com_ParseToken( script, false );
				if( !tok[0] )
				{
					MsgDev( D_WARN, "missing expression for 'requires' in shader '%s', discarded stage\n", shader->name );
					return false;
				}
			}
			else
			{
				if( tok[0] == '!' )
				{
					tok++;
					negate = true;
				}
				else negate = false;
			}

			if(Com_MatchToken( "GL_ARB_multitexture" ))
				results[count] = GL_Support( R_ARB_MULTITEXTURE );
			else if (Com_MatchToken( "GL_ARB_texture_env_add"))
				results[count] = GL_Support( R_TEXTURE_ENV_ADD_EXT );
			else if (Com_MatchToken( "GL_ARB_texture_env_combine"))
				results[count] = GL_Support( R_COMBINE_EXT );
			else if (Com_MatchToken( "GL_ARB_texture_env_dot3"))
				results[count] = GL_Support( R_DOT3_ARB_EXT );
			else if (Com_MatchToken( "GL_ARB_texture_cube_map"))
				results[count] = GL_Support( R_TEXTURECUBEMAP_EXT );
			else if (Com_MatchToken( "GL_ARB_vertex_program"))
				results[count] = GL_Support( R_VERTEX_PROGRAM_EXT );
			else if (Com_MatchToken( "GL_ARB_fragment_program"))
				results[count] = GL_Support( R_FRAGMENT_PROGRAM_EXT );
			else
			{
				MsgDev( D_WARN, "unknown 'requires' expression '%s' in shader '%s', discarded stage\n", tok, shader->name );
				return false;
			}
			if( negate ) results[count] = !results[count];
		}
		count++;
	}

	// evaluate expressions
	if( logicAnd && logicOr )
	{
		MsgDev( D_WARN, "different logical operators used for 'requires' in shader '%s', discarded stage\n", shader->name );
		return false;
	}

	if( logicAnd )
	{
		// all expressions must evaluate to true
		for( i = 0; i < count; i++ )
		{
			if( results[i] == false )
				return false;
		}
		return true;
	}
	else if( logicOr )
	{
		// at least one of the expressions must evaluate to true
		for( i = 0; i < count; i++ )
		{
			if( results[i] == true )
				return true;
		}
		return false;
	}
	return results[0];
}

/*
=================
R_SkipShaderStage
=================
*/
static bool R_SkipShaderStage( shader_t *shader, char **script )
{
	char	*tok;

	while( 1 )
	{
		Com_PushScript( script );

		tok = Com_ParseToken( script, true );
		if( Com_MatchToken( "requires" ))
		{
			if( !R_EvaluateRequires( shader, script ))
			{
				Com_SkipBracedSection( script, 1 );
				return true;
			}
			continue;
		}
		Com_PopScript( script );
		break;
	}
	return false;
}

/*
=================
R_ParseShader
=================
*/
static bool R_ParseShader( shader_t *shader, char *script )
{
	shaderStage_t	*stage;
	char		*tok;

	tok = Com_ParseToken( &script, true );
	if( !tok[0] )
	{
		MsgDev( D_WARN, "shader '%s' has an empty script\n", shader->name );
		return false;
	}

	// parse the shader
	if( Com_MatchToken( "{" ))
	{
		while( 1 )
		{
			tok = Com_ParseToken( &script, true );
			if( !tok[0] )
			{
				MsgDev( D_WARN, "no concluding '}' in shader '%s'\n", shader->name );
				return false;	// End of data
			}

			if( Com_MatchToken( "}" )) break; // end of shader

			// parse a stage
			if( Com_MatchToken( "{" ))
			{
				// check if we need to skip this stage
				if( R_SkipShaderStage( shader, &script ))
					continue;

				// create a new stage
				if( shader->numStages == SHADER_MAX_STAGES )
				{
					MsgDev( D_WARN, "SHADER_MAX_STAGES hit in shader '%s'\n", shader->name );
					return false;
				}
				stage = shader->stages[shader->numStages++];
				stage->numBundles++;

				// parse it
				while( 1 )
				{
					tok = Com_ParseToken( &script, true );
					if( !tok[0] )
					{
						MsgDev( D_WARN, "no matching '}' in shader '%s'\n", shader->name );
						return false; // end of data
					}

					if( Com_MatchToken( "}" )) break; // end of stage

					// parse the command
					if( !R_ParseShaderStageCommand( shader, stage, &script, tok ))
						return false;
				}
				continue;
			}

			// parse the command
			if( !R_ParseShaderCommand( shader, &script, tok ))
				return false;
		}
		return true;
	}
	else
	{
		MsgDev( D_WARN, "expected '{', found '%s' instead in shader '%s'\n", shader->name );
		return false;
	}
}

/*
=================
R_ParseShaderFile
=================
*/
static void R_ParseShaderFile( char *buffer, int size )
{
	shaderScript_t	*shaderScript;
	int		numInfoParms = sizeof(infoParms) / sizeof(infoParms[0]);
	char		*buf, *tok;
	char		*ptr1, *ptr2;
	char		*script;
	shaderType_t	shaderType;
	uint		surfaceParm;
	uint		contentFlags;
	uint		i, hashKey;
	string		name;

	buf = buffer;
	while( 1 )
	{
		// parse the name
		tok = Com_ParseToken( &buf, true );
		if( !tok[0] ) break; // end of data

		com.strncpy( name, tok, sizeof( name ));

		// parse the script
		ptr1 = buf;
		Com_SkipBracedSection( &buf, 0 );
		ptr2 = buf;

		if( !ptr1 ) ptr1 = buffer;
		if( !ptr2 ) ptr2 = buffer + size;

		// we must parse surfaceParm commands here, because R_FindShader
		// needs this for correct shader loading.
		// proper syntax checking is done when the shader is loaded.
		shaderType = -1;
		surfaceParm = contentFlags = 0;

		script = ptr1;
		while( script < ptr2 )
		{
			tok = Com_ParseToken( &script, true );
			if( !tok[0] ) break; // end of data

			if( Com_MatchToken( "surfaceparm" ))
			{
				tok = Com_ParseToken( &script, false );
				if( !tok[0] ) continue;

				for( i = 0; i < numInfoParms; i++ )
				{
					if(Com_MatchToken( infoParms[i].name ))
					{
						surfaceParm |= infoParms[i].surfaceFlags;
						contentFlags |= infoParms[i].contents;
						break;
					}
				}
			}
		}

		// store the script
		shaderScript = Mem_Alloc( r_shaderpool, sizeof(shaderScript_t) + (ptr2 - ptr1) + 1 );
		com.strncpy( shaderScript->name, name, sizeof( shaderScript->name ));
		shaderScript->shaderType = shaderType;
		shaderScript->surfaceParm = surfaceParm;
		shaderScript->contents = contentFlags;
		Mem_Copy( shaderScript->script, ptr1, (ptr2 - ptr1));

		// add to hash table
		hashKey = Com_HashKey( shaderScript->name, SHADERS_HASHSIZE );

		shaderScript->nextHash = r_shaderScriptsHash[hashKey];
		r_shaderScriptsHash[hashKey] = shaderScript;
	}
}

/*
=======================================================================

 SHADER INITIALIZATION AND LOADING

=======================================================================
*/
/*
=================
R_NewShader
=================
*/
static shader_t *R_NewShader( void )
{
	shader_t	*shader;
	int	i, j;

	shader = &r_parseShader;
	memset( shader, 0, sizeof( shader_t ));

	for( i = 0; i < SHADER_MAX_STAGES; i++ )
	{
		shader->stages[i] = &r_parseShaderStages[i];
		memset( shader->stages[i], 0, sizeof( shaderStage_t ));

		for( j = 0; j < MAX_TEXTURE_UNITS; j++ )
		{
			shader->stages[i]->bundles[j] = &r_parseStageTMU[i][j];
			memset( shader->stages[i]->bundles[j], 0, sizeof( stageBundle_t ));
		}
	}
	return shader;
}

/*
=================
R_CreateShader
=================
*/
static shader_t *R_CreateShader( const char *name, shaderType_t shaderType, uint surfaceParm, char *script )
{
	shader_t		*shader;
	shaderStage_t	*stage;
	stageBundle_t	*bundle;
	int		i, j;

	// clear static shader
	shader = R_NewShader();

	// fill it in
	com.strncpy( shader->name, name, sizeof( shader->name ));
	shader->shaderNum = r_numShaders;
	shader->shaderType = shaderType;
	shader->surfaceParm = surfaceParm;
	shader->flags = SHADER_EXTERNAL;

	if( shaderType == SHADER_NOMIP ) shader->flags |= (SHADER_NOMIPMAPS | SHADER_NOPICMIP);

	if( !R_ParseShader( shader, script ))
	{
		// invalid script, so make sure we stop cinematics
		for( i = 0; i < shader->numStages; i++ )
		{
			stage = shader->stages[i];

			for( j = 0; j < stage->numBundles; j++ )
			{
				bundle = stage->bundles[j];

				//FIXME: implement
				//if( bundle->flags & STAGEBUNDLE_VIDEOMAP )
				//	CIN_StopCinematic( bundle->cinematicHandle );
			}
		}

		// use a default shader instead
		shader = R_NewShader();

		com.strncpy( shader->name, name, sizeof( shader->name ));
		shader->shaderNum = r_numShaders;
		shader->shaderType = shaderType;
		shader->surfaceParm = surfaceParm;
		shader->flags = SHADER_EXTERNAL|SHADER_DEFAULTED;
		shader->stages[0]->bundles[0]->textures[0] = r_defaultTexture;
		shader->stages[0]->bundles[0]->numTextures++;
		shader->stages[0]->numBundles++;
		shader->numStages++;
	}
	return shader;
}

/*
=================
R_CreateDefaultShader
=================
*/
static shader_t *R_CreateDefaultShader( const char *name, shaderType_t shaderType, uint surfaceParm )
{
	shader_t		*shader;
	const byte	*buffer = NULL; // default image buffer
	size_t		bufsize = 0;
	int		i;

	// clear static shader
	shader = R_NewShader();

	// fill it in
	com.strncpy( shader->name, name, sizeof( shader->name ));
	shader->shaderNum = r_numShaders;
	shader->shaderType = shaderType;
	shader->surfaceParm = surfaceParm;

	switch( shader->shaderType )
	{
	case SHADER_SKY:
		shader->flags |= SHADER_SKYPARMS;

		for( i = 0; i < 6; i++ )
		{
			if( shader->skyParms.farBox[i] )
				shader->skyParms.farBox[i]->flags &= ~TF_STATIC; // old skybox will be removed on next loading
			shader->skyParms.farBox[i] = R_FindTexture(va("gfx/env/%s%s", shader->name, r_skyBoxSuffix[i]), NULL, 0, TF_CLAMP|TF_SKYSIDE|TF_STATIC, 0 );
			if( !shader->skyParms.farBox[i] )
			{
				MsgDev( D_WARN, "couldn't find texture for shader '%s', using default...\n", shader->name );
				shader->skyParms.farBox[i] = r_defaultTexture;
			}
		}
		shader->skyParms.cloudHeight = 128.0;
		break;
	case SHADER_TEXTURE:
		shader->stages[0]->bundles[0]->flags |= STAGEBUNDLE_MAP;
		shader->stages[0]->bundles[0]->textures[0] = R_FindTexture( shader->name, buffer, bufsize, TF_MIPMAPS|TF_COMPRESS, 0 );
		if( !shader->stages[0]->bundles[0]->textures[0] )
		{
			MsgDev( D_WARN, "couldn't find texture for shader '%s', using default...\n", shader->name );
			shader->stages[0]->bundles[0]->textures[0] = r_defaultTexture;
		}
		shader->stages[0]->bundles[0]->numTextures++;

		// fast presets
		if( shader->surfaceParm & SURF_BLEND )
		{
			shader->stages[0]->flags |= SHADERSTAGE_BLENDFUNC;
			shader->stages[0]->blendFunc.src = GL_SRC_ALPHA;
			shader->stages[0]->blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
			shader->flags |= SHADER_ENTITYMERGABLE; // using renderamt
		}
		else if( shader->surfaceParm & SURF_ALPHA )
		{
			shader->stages[0]->flags |= SHADERSTAGE_ALPHAFUNC;
			shader->stages[0]->blendFunc.src = GL_SRC_ALPHA;
			shader->stages[0]->blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
			shader->stages[0]->alphaFunc.func = GL_GREATER;
			shader->stages[0]->alphaFunc.ref = 0.666;
			shader->flags |= SHADER_ENTITYMERGABLE; // using renderamt
		}
		shader->stages[0]->numBundles++;
		shader->numStages++;

		if(!( shader->surfaceParm & SURF_NOLIGHTMAP ))
		{
			shader->flags |= SHADER_HASLIGHTMAP;
			shader->stages[1]->bundles[0]->flags |= STAGEBUNDLE_MAP;
			shader->stages[1]->bundles[0]->texType = TEX_LIGHTMAP;
			shader->stages[1]->flags |= SHADERSTAGE_BLENDFUNC;
			shader->stages[1]->blendFunc.src = GL_DST_COLOR;
			shader->stages[1]->blendFunc.dst = GL_ONE; //FIXME: was GL_ZERO
			shader->stages[1]->numBundles++;
			shader->numStages++;
		}
		break;
	case SHADER_STUDIO:
		shader->stages[0]->bundles[0]->flags |= STAGEBUNDLE_MAP;
		shader->stages[0]->bundles[0]->textures[0] = r_internalMiptex; // internal spriteframe
		if( !shader->stages[0]->bundles[0]->textures[0] )
		{
			MsgDev( D_WARN, "couldn't find texture for shader '%s', using default...\n", shader->name );
			shader->stages[0]->bundles[0]->textures[0] = r_defaultTexture;
		}
		// fast presets
		if( shader->surfaceParm & SURF_BLEND )
		{
			shader->stages[0]->flags |= SHADERSTAGE_BLENDFUNC;
			shader->stages[0]->blendFunc.src = GL_SRC_ALPHA;
			shader->stages[0]->blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
			shader->flags |= SHADER_ENTITYMERGABLE; // using renderamt
	         		shader->sort = SORT_BLEND;
		}
		if( shader->surfaceParm & SURF_ADDITIVE )
		{
			shader->stages[0]->flags |= SHADERSTAGE_BLENDFUNC;
			shader->stages[0]->blendFunc.src = GL_SRC_ALPHA;
			shader->stages[0]->blendFunc.dst = GL_ONE;
			shader->flags |= SHADER_ENTITYMERGABLE; // using renderamt
	         		shader->sort = SORT_BLEND;
		}
		if( shader->surfaceParm & SURF_ALPHA )
		{
			shader->stages[0]->flags |= SHADERSTAGE_ALPHAFUNC;
			shader->stages[0]->blendFunc.src = GL_SRC_ALPHA;
			shader->stages[0]->blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
			shader->stages[0]->alphaFunc.func = GL_GREATER;
			shader->stages[0]->alphaFunc.ref = 0.666;
			shader->flags |= SHADER_ENTITYMERGABLE; // using renderamt
			shader->sort = SORT_SEETHROUGH;
		}
		shader->stages[0]->bundles[0]->numTextures++;
		shader->stages[0]->numBundles++;
		shader->numStages++;
		break;
	case SHADER_SPRITE:
		shader->stages[0]->bundles[0]->flags |= STAGEBUNDLE_MAP;
		shader->stages[0]->bundles[0]->textures[0] = r_internalMiptex; // internal spriteframe
		shader->stages[0]->bundles[0]->texType = TEX_GENERIC;
		if( !shader->stages[0]->bundles[0]->textures[0] )
		{
			MsgDev( D_WARN, "couldn't find spriteframe for shader '%s', using default...\n", shader->name );
			shader->stages[0]->bundles[0]->textures[0] = r_defaultTexture;
		}
		if( shader->surfaceParm & SURF_BLEND )
		{
			shader->stages[0]->flags |= SHADERSTAGE_BLENDFUNC;
			shader->stages[0]->blendFunc.src = GL_SRC_ALPHA;
			shader->stages[0]->blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
			shader->flags |= SHADER_ENTITYMERGABLE; // using renderamt
	         		shader->sort = SORT_BLEND;
		}
		if( shader->surfaceParm & SURF_ALPHA )
		{
			shader->stages[0]->flags |= SHADERSTAGE_ALPHAFUNC;
			shader->stages[0]->blendFunc.src = GL_SRC_ALPHA;
			shader->stages[0]->blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
			shader->stages[0]->alphaFunc.func = GL_GREATER;
			shader->stages[0]->alphaFunc.ref = 0.666;
			shader->flags |= SHADER_ENTITYMERGABLE; // using renderamt
			shader->sort = SORT_SEETHROUGH;
		}
		shader->stages[0]->bundles[0]->numTextures++;
		shader->stages[0]->numBundles++;
		shader->numStages++;
		break;
	case SHADER_NOMIP:
		// don't let user set invalid font
		// FIXME: make case SHADER_FONT and func RegisterShaderFont
		if(com.stristr( shader->name, "fonts/" )) buffer = FS_LoadInternal( "default.dds", &bufsize );
		shader->stages[0]->bundles[0]->flags |= STAGEBUNDLE_MAP;
		shader->stages[0]->bundles[0]->textures[0] = R_FindTexture( shader->name, buffer, bufsize, TF_COMPRESS|TF_IMAGE2D, 0 );
		if( !shader->stages[0]->bundles[0]->textures[0] )
		{
			MsgDev( D_WARN, "couldn't find texture for shader '%s', using default...\n", shader->name );
			shader->stages[0]->bundles[0]->textures[0] = r_defaultTexture;
		}
		shader->stages[0]->bundles[0]->numTextures++;
		shader->stages[0]->flags |= SHADERSTAGE_BLENDFUNC;
		shader->stages[0]->blendFunc.src = GL_SRC_ALPHA;
		shader->stages[0]->blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
		shader->stages[0]->numBundles++;
		shader->numStages++;
		break;
	case SHADER_GENERIC:
		shader->stages[0]->bundles[0]->flags |= STAGEBUNDLE_MAP;
		shader->stages[0]->bundles[0]->textures[0] = R_FindTexture( shader->name, buffer, bufsize, TF_MIPMAPS|TF_COMPRESS, 0 );
		if( !shader->stages[0]->bundles[0]->textures[0] )
		{
			MsgDev( D_WARN, "couldn't find texture for shader '%s', using default...\n", shader->name );
			shader->stages[0]->bundles[0]->textures[0] = r_defaultTexture;
		}
		shader->stages[0]->bundles[0]->numTextures++;
		shader->stages[0]->numBundles++;
		shader->numStages++;
		break;
	}
	return shader;
}

/*
=================
R_FinishShader
=================
*/
static void R_FinishShader( shader_t *shader )
{
	shaderStage_t	*stage;
	stageBundle_t	*bundle;
	int		i, j;

	// remove entityMergable from 2D shaders
	if( shader->shaderType == SHADER_NOMIP )
		shader->flags &= ~SHADER_ENTITYMERGABLE;

	// remove polygonOffset from 2D shaders
	if( shader->shaderType == SHADER_NOMIP )
		shader->flags &= ~SHADER_POLYGONOFFSET;

	// set cull if unset
	if(!(shader->flags & SHADER_CULL ))
	{
		shader->flags |= SHADER_CULL;
		shader->cull.mode = GL_FRONT;
	}
	else
	{
		// remove if undefined (disabled)
		if( shader->cull.mode == 0 )
			shader->flags &= ~SHADER_CULL;
	}

	// remove cull from 2D shaders
	if( shader->shaderType == SHADER_NOMIP )
	{
		shader->flags &= ~SHADER_CULL;
		shader->cull.mode = 0;
	}

	// make sure sky shaders have a cloudHeight value
	if( shader->shaderType == SHADER_SKY )
	{
		if(!(shader->flags & SHADER_SKYPARMS))
		{
			shader->flags |= SHADER_SKYPARMS;
			shader->skyParms.cloudHeight = 128.0;
		}
	}

	// remove deformVertexes from 2D shaders
	if( shader->shaderType == SHADER_NOMIP )
	{
		if( shader->flags & SHADER_DEFORMVERTEXES )
		{
			shader->flags &= ~SHADER_DEFORMVERTEXES;
			for( i = 0; i < shader->deformVertexesNum; i++ )
				memset( &shader->deformVertexes[i], 0, sizeof( deformVerts_t ));
			shader->deformVertexesNum = 0;
		}
	}

	// lightmap but no lightmap stage?
	if( shader->shaderType == SHADER_TEXTURE && !(shader->surfaceParm & SURF_NOLIGHTMAP ))
	{
		if(!(shader->flags & SHADER_DEFAULTED) && !(shader->flags & SHADER_HASLIGHTMAP))
			MsgDev( D_WARN, "shader '%s' has lightmap but no lightmap stage!\n", shader->name );
	}

	// check stages
	for( i = 0; i < shader->numStages; i++ )
	{
		stage = shader->stages[i];

		// check bundles
		for( j = 0; j < stage->numBundles; j++ )
		{
			bundle = stage->bundles[j];

			// make sure it has a texture
			if( bundle->texType == TEX_GENERIC && !bundle->numTextures )
			{
				if( j == 0 ) MsgDev( D_WARN, "shader '%s' has a stage with no texture!\n", shader->name );
				else MsgDev( D_WARN, "shader '%s' has a bundle with no texture!\n", shader->name );
				bundle->textures[bundle->numTextures++] = r_defaultTexture;
			}

			// set tcGen if unset
			if(!(bundle->flags & STAGEBUNDLE_TCGEN))
			{
				if( bundle->texType == TEX_LIGHTMAP )
					bundle->tcGen.type = TCGEN_LIGHTMAP;
				else bundle->tcGen.type = TCGEN_BASE;
				bundle->flags |= STAGEBUNDLE_TCGEN;
			}
			else
			{
				// only allow tcGen lightmap on world surfaces
				if( bundle->tcGen.type == TCGEN_LIGHTMAP )
				{
					if( shader->shaderType != SHADER_TEXTURE )
						bundle->tcGen.type = TCGEN_BASE;
				}
			}

			// check keywords that reference the current entity
			if( bundle->flags & STAGEBUNDLE_TCGEN )
			{
				switch( bundle->tcGen.type )
				{
				case TCGEN_ENVIRONMENT:
				case TCGEN_LIGHTVECTOR:
				case TCGEN_HALFANGLE:
					if( shader->shaderType == SHADER_NOMIP )
						bundle->tcGen.type = TCGEN_BASE;
					shader->flags &= ~SHADER_ENTITYMERGABLE;
					break;
				}
			}
		}

		// blendFunc GL_ONE GL_ZERO is non-blended
		if( stage->blendFunc.src == GL_ONE && stage->blendFunc.dst == GL_ZERO )
		{
			stage->flags &= ~SHADERSTAGE_BLENDFUNC;
			stage->blendFunc.src = 0;
			stage->blendFunc.dst = 0;
		}

		// set depthFunc if unset
		if(!(stage->flags & SHADERSTAGE_DEPTHFUNC))
		{
			stage->flags |= SHADERSTAGE_DEPTHFUNC;
			stage->depthFunc.func = GL_LEQUAL;
		}

		// Remove depthFunc from 2D shaders
		if( shader->shaderType == SHADER_NOMIP )
		{
			stage->flags &= ~SHADERSTAGE_DEPTHFUNC;
			stage->depthFunc.func = 0;
		}

		// set depthWrite for non-blended stages
		if(!(stage->flags & SHADERSTAGE_BLENDFUNC))
			stage->flags |= SHADERSTAGE_DEPTHWRITE;

		// remove depthWrite from sky and 2D shaders
		if( shader->shaderType == SHADER_SKY || shader->shaderType == SHADER_NOMIP )
			stage->flags &= ~SHADERSTAGE_DEPTHWRITE;

		// ignore detail stages if detail textures are disabled
		if( stage->flags & SHADERSTAGE_DETAIL )
		{
			if( !r_detailtextures->integer )
				stage->ignore = true;
		}

		// set rgbGen if unset
		if(!(stage->flags & SHADERSTAGE_RGBGEN))
		{
			switch( shader->shaderType )
			{
			case SHADER_SKY:
				stage->rgbGen.type = RGBGEN_IDENTITY;
				break;
			case SHADER_TEXTURE:
				if((stage->flags & SHADERSTAGE_BLENDFUNC) && (stage->bundles[0]->texType != TEX_LIGHTMAP))
					stage->rgbGen.type = RGBGEN_IDENTITYLIGHTING;
				else stage->rgbGen.type = RGBGEN_IDENTITY;
				break;
			case SHADER_STUDIO:
				if( shader->surfaceParm & SURF_ADDITIVE )
					stage->rgbGen.type = RGBGEN_IDENTITY;
				else stage->rgbGen.type = RGBGEN_IDENTITYLIGHTING;
				break;
			case SHADER_SPRITE:
				if( shader->surfaceParm & SURF_ALPHA|SURF_BLEND )
					stage->rgbGen.type = RGBGEN_IDENTITYLIGHTING; // sprite colormod
				else if( shader->surfaceParm & SURF_ADDITIVE )
					stage->rgbGen.type = RGBGEN_IDENTITY;
				break;
			case SHADER_NOMIP:
			case SHADER_GENERIC:
				stage->rgbGen.type = RGBGEN_VERTEX;
				break;
			}
			stage->flags |= SHADERSTAGE_RGBGEN;
		}

		// set alphaGen if unset
		if(!(stage->flags & SHADERSTAGE_ALPHAGEN))
		{
			switch( shader->shaderType )
			{
			case SHADER_SKY:
				stage->alphaGen.type = ALPHAGEN_IDENTITY;
				break;
			case SHADER_TEXTURE:
				if((stage->flags & SHADERSTAGE_BLENDFUNC) && (stage->bundles[0]->texType != TEX_LIGHTMAP))
				{
					if( shader->surfaceParm & SURF_ADDITIVE )
						stage->alphaGen.type = ALPHAGEN_VERTEX;
					else stage->alphaGen.type = ALPHAGEN_IDENTITY;
				}
				else stage->alphaGen.type = ALPHAGEN_IDENTITY;
				break;
			case SHADER_STUDIO:
				stage->alphaGen.type = ALPHAGEN_IDENTITY;
				break;
			case SHADER_SPRITE:
				stage->alphaGen.type = ALPHAGEN_IDENTITY;
				break;
			case SHADER_NOMIP:
			case SHADER_GENERIC:
				stage->alphaGen.type = ALPHAGEN_VERTEX;
				break;
			}
			stage->flags |= SHADERSTAGE_ALPHAGEN;
		}

		// check keywords that reference the current entity
		if( stage->flags & SHADERSTAGE_RGBGEN )
		{
			switch( stage->rgbGen.type )
			{
			case RGBGEN_ENTITY:
			case RGBGEN_ONEMINUSENTITY:
			case RGBGEN_LIGHTINGAMBIENT:
			case RGBGEN_LIGHTINGDIFFUSE:
				if( shader->shaderType == SHADER_NOMIP )
					stage->rgbGen.type = RGBGEN_VERTEX;
				shader->flags &= ~SHADER_ENTITYMERGABLE;
				break;
			}
		}
		if( stage->flags & SHADERSTAGE_ALPHAGEN )
		{
			switch( stage->alphaGen.type )
			{
			case ALPHAGEN_ENTITY:
			case ALPHAGEN_ONEMINUSENTITY:
			case ALPHAGEN_DOT:
			case ALPHAGEN_ONEMINUSDOT:
			case ALPHAGEN_FADE:
			case ALPHAGEN_ONEMINUSFADE:
			case ALPHAGEN_LIGHTINGSPECULAR:
				if( shader->shaderType == SHADER_NOMIP )
					stage->alphaGen.type = ALPHAGEN_VERTEX;
				shader->flags &= ~SHADER_ENTITYMERGABLE;
				break;
			}
		}

		// set texEnv for first bundle
		if(!(stage->bundles[0]->flags & STAGEBUNDLE_TEXENVCOMBINE))
		{
			if(!(stage->flags & SHADERSTAGE_BLENDFUNC))
			{
				if( stage->rgbGen.type == RGBGEN_IDENTITY && stage->alphaGen.type == ALPHAGEN_IDENTITY)
					stage->bundles[0]->texEnv = GL_REPLACE;
				else stage->bundles[0]->texEnv = GL_MODULATE;
			}
			else
			{
				if((stage->blendFunc.src == GL_DST_COLOR && stage->blendFunc.dst == GL_ZERO) || (stage->blendFunc.src == GL_ZERO && stage->blendFunc.dst == GL_SRC_COLOR))
					stage->bundles[0]->texEnv = GL_MODULATE;
				else if(stage->blendFunc.src == GL_ONE && stage->blendFunc.dst == GL_ONE)
					stage->bundles[0]->texEnv = GL_ADD;
			}
		}

		// if this stage is ignored, make sure we stop cinematics
		if( stage->ignore )
		{
			for( j = 0; j < stage->numBundles; j++ )
			{
				bundle = stage->bundles[j];

				//FIXME: implement
				//if( bundle->flags & STAGEBUNDLE_VIDEOMAP )
				//	CIN_StopCinematic( bundle->cinematicHandle );
			}
		}
	}

	// set sort if unset
	if( !(shader->flags & SHADER_SORT ))
	{
		if( shader->shaderType == SHADER_SKY )
			shader->sort = SORT_SKY;
		else
		{
			stage = shader->stages[0];
			if( !(stage->flags & SHADERSTAGE_BLENDFUNC ))
				shader->sort = SORT_OPAQUE;
			else
			{
				if( shader->flags & SHADER_POLYGONOFFSET )
					shader->sort = SORT_DECAL;
				else if( stage->flags & SHADERSTAGE_ALPHAFUNC )
					shader->sort = SORT_SEETHROUGH;
				else
				{
					if((stage->blendFunc.src == GL_SRC_ALPHA && stage->blendFunc.dst == GL_ONE) || (stage->blendFunc.src == GL_ONE && stage->blendFunc.dst == GL_ONE))
						shader->sort = SORT_ADDITIVE;
					else shader->sort = SORT_BLEND;
				}
			}
		}
	}
}

/*
=================
R_MergeShaderStages
=================
*/
static bool R_MergeShaderStages( shaderStage_t *stage1, shaderStage_t *stage2 )
{
	if( GL_Support( R_ARB_MULTITEXTURE ))
		return false;

	if( stage1->numBundles == gl_config.textureunits || stage1->numBundles == MAX_TEXTURE_UNITS )
		return false;

	if( stage1->flags & SHADERSTAGE_NEXTBUNDLE || stage2->flags & SHADERSTAGE_NEXTBUNDLE )
		return false;

	if( stage1->flags & (SHADERSTAGE_VERTEXPROGRAM | SHADERSTAGE_FRAGMENTPROGRAM) || stage2->flags & (SHADERSTAGE_VERTEXPROGRAM | SHADERSTAGE_FRAGMENTPROGRAM))
		return false;

	if( stage2->flags & SHADERSTAGE_ALPHAFUNC )
		return false;

	if( stage1->flags & SHADERSTAGE_ALPHAFUNC || stage1->depthFunc.func == GL_EQUAL )
	{
		if( stage2->depthFunc.func != GL_EQUAL )
			return false;
	}

	if(!(stage1->flags & SHADERSTAGE_DEPTHWRITE))
	{
		if( stage2->flags & SHADERSTAGE_DEPTHWRITE )
			return false;
	}

	if( stage2->rgbGen.type != RGBGEN_IDENTITY || stage2->alphaGen.type != ALPHAGEN_IDENTITY )
		return false;

	switch( stage1->bundles[0]->texEnv )
	{
	case GL_REPLACE:
		if( stage2->bundles[0]->texEnv == GL_REPLACE )
			return true;
		if( stage2->bundles[0]->texEnv == GL_MODULATE )
			return true;
		if( stage2->bundles[0]->texEnv == GL_ADD && GL_Support( R_TEXTURE_ENV_ADD_EXT ))
			return true;
		break;
	case GL_MODULATE:
		if( stage2->bundles[0]->texEnv == GL_REPLACE )
			return true;
		if( stage2->bundles[0]->texEnv == GL_MODULATE )
			return true;
		break;
	case GL_ADD:
		if( stage2->bundles[0]->texEnv == GL_ADD && GL_Support( R_TEXTURE_ENV_ADD_EXT ))
			return true;
		break;
	}
	return false;
}

/*
=================
R_OptimizeShader
=================
*/
static void R_OptimizeShader( shader_t *shader )
{
	shaderStage_t	*curStage, *prevStage = NULL;
	int		i;

	// try to merge multiple stages for multitexturing
	for( i = 0; i < shader->numStages; i++ )
	{
		curStage = shader->stages[i];

		if( curStage->ignore ) continue;

		if( !prevStage )
		{
			// no previous stage to merge with
			prevStage = curStage;
			continue;
		}

		if( !R_MergeShaderStages( prevStage, curStage ))
		{
			// couldn't merge the stages.
			prevStage = curStage;
			continue;
		}

		// merge with previous stage and ignore it
		Mem_Copy( prevStage->bundles[prevStage->numBundles++], curStage->bundles[0], sizeof( stageBundle_t ));
		curStage->ignore = true;
	}

	// make sure texEnv is valid (left-over from R_FinishShader)
	for( i = 0; i < shader->numStages; i++ )
	{
		if( shader->stages[i]->ignore ) continue;
		if( shader->stages[i]->bundles[0]->flags & STAGEBUNDLE_TEXENVCOMBINE )
			continue;
		if( shader->stages[i]->bundles[0]->texEnv != GL_REPLACE && shader->stages[i]->bundles[0]->texEnv != GL_MODULATE )
			shader->stages[i]->bundles[0]->texEnv = GL_MODULATE;
	}
}

/*
=================
R_LoadShader
=================
*/
shader_t *R_LoadShader( shader_t *newShader )
{
	shader_t	*shader;
	uint	hashKey;
	int	i, j;

	if( r_numShaders == MAX_SHADERS )
		Host_Error( "R_LoadShader: MAX_SHADERS limit exceeded\n" );

	r_shaders[r_numShaders++] = shader = Mem_Alloc( r_shaderpool, sizeof( shader_t ));

	// make sure the shader is valid and set all the unset parameters
	R_FinishShader( newShader );

	// try to merge multiple stages for multitexturing
	R_OptimizeShader( newShader );

	// copy the shader
	Mem_Copy( shader, newShader, sizeof( shader_t ));
	shader->numStages = 0;

	// allocate and copy the stages
	for( i = 0; i < newShader->numStages; i++ )
	{
		if( newShader->stages[i]->ignore )
			continue;

		shader->stages[shader->numStages] = Mem_Alloc( r_shaderpool, sizeof( shaderStage_t ));
		Mem_Copy(shader->stages[shader->numStages], newShader->stages[i], sizeof(shaderStage_t));

		// allocate and copy the bundles
		for( j = 0; j < shader->stages[shader->numStages]->numBundles; j++ )
		{
			shader->stages[shader->numStages]->bundles[j] = Mem_Alloc( r_shaderpool, sizeof( stageBundle_t ));
			Mem_Copy(shader->stages[shader->numStages]->bundles[j], newShader->stages[i]->bundles[j], sizeof(stageBundle_t));
		}
		shader->numStages++;
	}

	// add to hash table
	hashKey = Com_HashKey( shader->name, SHADERS_HASHSIZE );
	shader->nextHash = r_shadersHash[hashKey];
	r_shadersHash[hashKey] = shader;

	return shader;
}

/*
=================
R_FindShader
=================
*/
shader_t *R_FindShader( const char *name, shaderType_t shaderType, uint surfaceParm )
{
	shader_t		*shader;
	shaderScript_t	*shaderScript;
	char		*script = NULL;
	uint		hashKey;

	if( !name || !name[0] ) Host_Error( "R_FindShader: NULL shader name\n" );

	// see if already loaded
	hashKey = Com_HashKey( name, SHADERS_HASHSIZE );

	for( shader = r_shadersHash[hashKey]; shader; shader = shader->nextHash )
	{
		if(!com.stricmp( shader->name, name ) || !com.stricmp( shader->name, va( "textures/%s", name )))
		{
			if((shader->shaderType == shaderType) && (shader->surfaceParm == surfaceParm))
				return shader;
		}
	}

	// see if there's a script for this shader
	for( shaderScript = r_shaderScriptsHash[hashKey]; shaderScript; shaderScript = shaderScript->nextHash )
	{
		if(!com.stricmp( shaderScript->name, name ) || !com.stricmp( shaderScript->name, va( "textures/%s", name )))
		{
			if((shaderScript->shaderType == shaderType && shaderScript->surfaceParm == surfaceParm) || (shaderScript->shaderType == -1))
			{
				script = shaderScript->script;
				break;
			}
		}
	}
 
 	// create the shader
	if( script ) shader = R_CreateShader( name, shaderType, surfaceParm, script );
	else shader = R_CreateDefaultShader( name, shaderType, surfaceParm );

	// load it in
	return R_LoadShader( shader );
}

void R_SetInternalMap( texture_t *mipTex )
{
	// never replace with NULL
	if( mipTex ) r_internalMiptex = mipTex;
}

/*
=================
R_RegisterShader
=================
*/
shader_t *R_RegisterShader( const char *name )
{
	return R_FindShader( name, SHADER_GENERIC, 0 );
}

/*
=================
R_RegisterShaderSkin
=================
*/
shader_t *R_RegisterShaderSkin( const char *name )
{
	return R_FindShader( name, SHADER_STUDIO, 0 );
}

/*
=================
R_RegisterShaderNoMip
=================
*/
shader_t *R_RegisterShaderNoMip( const char *name )
{
	return R_FindShader( name, SHADER_NOMIP, 0 );
}

/*
=================
R_ShaderRegisterImages

many many included cycles ...
=================
*/
void R_ShaderRegisterImages( rmodel_t *mod )
{
	int		i, j, k, l;
	int		c_total = 0;
	shader_t		*shader;
	shaderStage_t	*stage;
	stageBundle_t	*bundle;
	texture_t		*texture;

	if( !mod ) return;
	for( i = 0; i < mod->numShaders; i++ )
	{
		shader = mod->shaders[i].shader;
		for( j = 0; j < shader->numStages; j++ )
		{
			stage = shader->stages[j];
			for( k = 0; k < stage->numBundles; k++ )
			{
				bundle = stage->bundles[k];
				for( l = 0; l < bundle->numTextures; l++ )
				{
					// prolonge registration for all shader textures
					texture = bundle->textures[l];
					texture->registration_sequence = registration_sequence;
					c_total++; // just for debug
				}
			}
		}
	}
}

/*
=================
R_CreateBuiltInShaders
=================
*/
static void R_CreateBuiltInShaders( void )
{
	shader_t	*shader;

	// default shader
	shader = R_NewShader();

	com.strncpy( shader->name, "<default>", sizeof( shader->name ));
	shader->shaderNum = r_numShaders;
	shader->shaderType = SHADER_TEXTURE;
	shader->surfaceParm = 0;
	shader->stages[0]->bundles[0]->textures[0] = r_defaultTexture;
	shader->stages[0]->bundles[0]->numTextures++;
	shader->stages[0]->numBundles++;
	shader->numStages++;

	r_defaultShader = R_LoadShader( shader );

	// lightmap shader
	shader = R_NewShader();

	com.strncpy( shader->name, "<lightmap>", sizeof( shader->name ));
	shader->shaderNum = r_numShaders;
	shader->shaderType = SHADER_TEXTURE;
	shader->flags = SHADER_HASLIGHTMAP;
	shader->stages[0]->bundles[0]->texType = TEX_LIGHTMAP;
	shader->stages[0]->numBundles++;
	shader->numStages++;

	r_lightmapShader = R_LoadShader( shader );
}

/*
=================
R_ShaderList_f
=================
*/
void R_ShaderList_f( void )
{
	shader_t		*shader;
	int		i, j;
	int		passes;

	Msg( "\n" );
	Msg( "-----------------------------------\n" );

	for( i = 0; i < r_numShaders; i++ )
	{
		shader = r_shaders[i];
		for( passes = j = 0; j < shader->numStages; j++ )
			passes += shader->stages[j]->numBundles;

		Msg( "%i/%i ", passes, shader->numStages );

		if( shader->flags & SHADER_EXTERNAL )
			Msg("E ");
		else Msg("  ");

		switch( shader->shaderType )
		{
		case SHADER_SKY:
			Msg( "sky " );
			break;
		case SHADER_TEXTURE:
			Msg( "bsp " );
			break;
		case SHADER_STUDIO:
			Msg( "mdl " );
			break;
		case SHADER_SPRITE:
			Msg( "spr " );
			break;
		case SHADER_NOMIP:
			Msg( "pic " );
			break;
		case SHADER_GENERIC:
			Msg( "gen " );
			break;
		}

		if( shader->surfaceParm )
			Msg( "%02X ", shader->surfaceParm );
		else Msg("   ");

		Msg( "%2i ", shader->sort );
		Msg( ": %s%s\n", shader->name, (shader->flags & SHADER_DEFAULTED) ? " (DEFAULTED)" : "" );
	}

	Msg( "-----------------------------------\n" );
	Msg( "%i total shaders\n", r_numShaders );
	Msg( "\n" );
}

/*
=================
R_InitShaders
=================
*/
void R_InitShaders( void )
{
	byte	*buffer;
	size_t	size;
	search_t	*t;
	int	i;

	MsgDev( D_NOTE, "R_InitShaders()\n" );

	r_shaderpool = Mem_AllocPool( "Shader Zone" );
	t = FS_Search( "scripts/shaders/*.txt", true );
	if( !t ) MsgDev( D_WARN, "no shader files found!\n");

	// Load them
	for( i = 0; t && i < t->numfilenames; i++ )
	{
		buffer = FS_LoadFile( t->filenames[i], &size );
		if( !buffer )
		{
			MsgDev( D_ERROR, "Couldn't load '%s'\n", t->filenames[i] );
			continue;
		}

		// parse this file
		R_ParseShaderFile( buffer, size );
		Mem_Free( buffer );
	}
	if( t ) Mem_Free( t ); // free search

	// create built-in shaders
	R_CreateBuiltInShaders();
	r_internalMiptex = r_defaultTexture;
}

/*
=================
R_ShutdownShaders
=================
*/
void R_ShutdownShaders( void )
{
	shader_t		*shader;
	shaderStage_t	*stage;
	stageBundle_t	*bundle;
	int		i, j, k;

	for( i = 0; i < r_numShaders; i++ )
	{
		shader = r_shaders[i];
		for( j = 0; j < shader->numStages; j++ )
		{
			stage = shader->stages[j];
			for( k = 0; k < stage->numBundles; k++ )
			{
				bundle = stage->bundles[k];

				//FIXME: implement
				//if( bundle->flags & STAGEBUNDLE_VIDEOMAP )
				//	CIN_StopCinematic( bundle->cinematicHandle );
			}
		}
	}

	Mem_FreePool( &r_shaderpool ); // free all data allocated by shaders
	memset( r_shaderScriptsHash, 0, sizeof( r_shaderScriptsHash ));
	memset( r_shadersHash, 0, sizeof( r_shadersHash ));
	memset( r_shaders, 0, sizeof( r_shaders ));
	r_numShaders = 0;
}
