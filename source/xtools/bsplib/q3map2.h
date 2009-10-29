/* -------------------------------------------------------------------------------

Copyright (C) 1999-2007 id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

----------------------------------------------------------------------------------

This code has been altered significantly from its original form, to support
several games based on the Quake III Arena engine, in the form of "Q3Map2."

------------------------------------------------------------------------------- */

#ifndef Q3MAP2_H
#define Q3MAP2_H

/*
-------------------------------------------------------------------------------

dependencies

-------------------------------------------------------------------------------
*/

/* platform-specific */
#include <windows.h>

/* general */
#include "xtools.h"
#include "engine_api.h"
#include "utils.h"
#include "mathlib.h"
#include "picomodel.h"
#include "polylib.h"

/*
-------------------------------------------------------------------------------

constants

-------------------------------------------------------------------------------
*/

/* general */
#define MAX_SHADERPATH				64

#define MAX_IMAGES				512
#define DEFAULT_IMAGE			"*default"

#define DEF_BACKSPLASH_FRACTION		0.05f	/* 5% backsplash by default */
#define DEF_BACKSPLASH_DISTANCE		23

#define DEF_RADIOSITY_BOUNCE			1.0f	/* ydnar: default to 100% re-emitted light */

#define MAX_SHADER_INFO			8192
#define MAX_CUST_SURFACEPARMS	64

#define SHADER_MAX_VERTEXES			1000
#define SHADER_MAX_INDEXES			(6 * SHADER_MAX_VERTEXES)

#define MAX_JITTERS				256

// supported map formats
enum
{
	BRUSH_UNKNOWN = 0,
	BRUSH_WORLDCRAFT_21,	// quake worldcraft  <= 2.1
	BRUSH_WORLDCRAFT_22,	// half-life worldcraft >= 2.2
	BRUSH_RADIANT,
	BRUSH_QUARK,
	BRUSH_COUNT
};

// compiler flags, because games have widely varying content/surface flags
#define C_SOLID					0x00000001
#define C_TRANSLUCENT			0x00000002
#define C_STRUCTURAL			0x00000004
#define C_HINT					0x00000008
#define C_NODRAW				0x00000010
#define C_LIGHTGRID				0x00000020
#define C_ALPHASHADOW			0x00000040
#define C_LIGHTFILTER			0x00000080
#define C_VERTEXLIT				0x00000100
#define C_LIQUID				0x00000200
#define C_FOG					0x00000400
#define C_SKY					0x00000800
#define C_ORIGIN				0x00001000
#define C_AREAPORTAL			0x00002000
#define C_ANTIPORTAL			0x00004000	/* like hint, but doesn't generate portals */
#define C_SKIP					0x00008000	/* like hint, but skips this face (doesn't split bsp) */
#define C_NOMARKS				0x00010000	/* no decals */
#define C_DETAIL				0x08000000	/* THIS MUST BE THE SAME AS IN RADIANT! */


/* shadow flags */
#define WORLDSPAWN_CAST_SHADOWS	1
#define WORLDSPAWN_RECV_SHADOWS	1
#define ENTITY_CAST_SHADOWS		0
#define ENTITY_RECV_SHADOWS		1


/* bsp */
#define MAX_PATCH_SIZE		32
#define MAX_BRUSH_SIDES		1024

#define MAX_EXPANDED_AXIS		128

#define CLIP_EPSILON		0.1f
#define PLANESIDE_EPSILON		0.001f
#define PLANENUM_LEAF		-1

#define	HINT_PRIORITY			1000		/* ydnar: force hint splits first and antiportal/areaportal splits last */
#define ANTIPORTAL_PRIORITY		-1000
#define AREAPORTAL_PRIORITY		-1000

#define	PSIDE_FRONT				1
#define	PSIDE_BACK				2
#define	PSIDE_BOTH				(PSIDE_FRONT | PSIDE_BACK)
#define	PSIDE_FACING			4

/* vis */
#define	VIS_HEADER_SIZE			8
#define	PORTALFILE				"PRT1"

#define	MAX_PORTALS				32768
#define MAX_SEPERATORS			MAX_POINTS_ON_WINDING
#define	MAX_POINTS_ON_FIXED_WINDING	24	/* ydnar: increased this from 12 at the expense of more memory */
#define	MAX_PORTALS_ON_LEAF		128


/* light */
#define EMIT_POINT				0
#define EMIT_AREA				1
#define EMIT_SPOT				2
#define EMIT_SUN				3

#define LIGHT_ATTEN_LINEAR		1
#define LIGHT_ATTEN_ANGLE		2
#define LIGHT_ATTEN_DISTANCE	4
#define LIGHT_TWOSIDED			8
#define LIGHT_GRID				16
#define LIGHT_SURFACES			32
#define LIGHT_DARK				64		/* probably never use this */
#define LIGHT_FAST				256
#define LIGHT_FAST_TEMP			512
#define LIGHT_FAST_ACTUAL		(LIGHT_FAST | LIGHT_FAST_TEMP)
#define LIGHT_NEGATIVE			1024

#define LIGHT_SUN_DEFAULT		(LIGHT_ATTEN_ANGLE | LIGHT_GRID | LIGHT_SURFACES)
#define LIGHT_AREA_DEFAULT		(LIGHT_ATTEN_ANGLE | LIGHT_ATTEN_DISTANCE | LIGHT_GRID | LIGHT_SURFACES)	/* q3a and wolf are the same */
#define LIGHT_Q3A_DEFAULT		(LIGHT_ATTEN_ANGLE | LIGHT_ATTEN_DISTANCE | LIGHT_GRID | LIGHT_SURFACES | LIGHT_FAST)
#define LIGHT_WOLF_DEFAULT		(LIGHT_ATTEN_LINEAR | LIGHT_ATTEN_DISTANCE | LIGHT_GRID | LIGHT_SURFACES | LIGHT_FAST)

#define MAX_TRACE_TEST_NODES	256
#define DEFAULT_INHIBIT_RADIUS	1.5f

#define LUXEL_EPSILON			0.125f
#define VERTEX_EPSILON			-0.125f
#define GRID_EPSILON			0.0f

#define DEFAULT_LIGHTMAP_SAMPLE_SIZE	16
#define DEFAULT_LIGHTMAP_SAMPLE_OFFSET	1.0f
#define DEFAULT_SUBDIVIDE_THRESHOLD		1.0f

#define	EXTRA_SCALE				2	/* -extrawide = -super 2 */
#define	EXTRAWIDE_SCALE			2	/* -extrawide = -super 2 -filter */

#define CLUSTER_UNMAPPED		-1
#define CLUSTER_OCCLUDED		-2
#define CLUSTER_FLOODED			-3

#define VERTEX_LUXEL_SIZE		3
#define BSP_LUXEL_SIZE			3
#define RAD_LUXEL_SIZE			3
#define SUPER_LUXEL_SIZE		4
#define SUPER_ORIGIN_SIZE		3
#define SUPER_NORMAL_SIZE		4
#define SUPER_DELUXEL_SIZE		3
#define BSP_DELUXEL_SIZE		3

#define VERTEX_LUXEL( s, v )	(vertexLuxels[ s ] + ((v) * VERTEX_LUXEL_SIZE))
#define RAD_VERTEX_LUXEL( s, v )(radVertexLuxels[ s ] + ((v) * VERTEX_LUXEL_SIZE))
#define BSP_LUXEL( s, x, y )	(lm->bspLuxels[ s ] + ((((y) * lm->w) + (x)) * BSP_LUXEL_SIZE))
#define RAD_LUXEL( s, x, y )	(lm->radLuxels[ s ] + ((((y) * lm->w) + (x)) * RAD_LUXEL_SIZE))
#define SUPER_LUXEL( s, x, y )	(lm->superLuxels[ s ] + ((((y) * lm->sw) + (x)) * SUPER_LUXEL_SIZE))
#define SUPER_DELUXEL( x, y )	(lm->superDeluxels + ((((y) * lm->sw) + (x)) * SUPER_DELUXEL_SIZE))
#define BSP_DELUXEL( x, y )		(lm->bspDeluxels + ((((y) * lm->w) + (x)) * BSP_DELUXEL_SIZE))
#define SUPER_CLUSTER( x, y )	(lm->superClusters + (((y) * lm->sw) + (x)))
#define SUPER_ORIGIN( x, y )	(lm->superOrigins + ((((y) * lm->sw) + (x)) * SUPER_ORIGIN_SIZE))
#define SUPER_NORMAL( x, y )	(lm->superNormals + ((((y) * lm->sw) + (x)) * SUPER_NORMAL_SIZE))
#define SUPER_DIRT( x, y )		(lm->superNormals + ((((y) * lm->sw) + (x)) * SUPER_NORMAL_SIZE) + 3)	/* stash dirtyness in normal[ 3 ] */



/* -------------------------------------------------------------------------------

abstracted bsp file

------------------------------------------------------------------------------- */

#define EXTERNAL_LIGHTMAP		"lm_%04d.tga"

#define MAX_LIGHTMAPS			4			/* RBSP */
#define MAX_LIGHT_STYLES		64
#define	MAX_SWITCHED_LIGHTS		32
#define LS_NORMAL				0x00
#define LS_UNUSED				0xFE
#define	LS_NONE					0xFF

#define MAX_LIGHTMAP_SHADERS	256

/* ok to increase these at the expense of more memory */
#define	MAX_MAP_BRUSHES			0x8000
#define	MAX_MAP_ENTSTRING		0x80000		//%	0x40000	/* ydnar */

#define	MAX_MAP_AREAS			0x100		/* MAX_MAP_AREA_BYTES in q_shared must match! */
#define	MAX_MAP_NODES			0x20000
#define	MAX_MAP_LEAFS			0x20000
#define	MAX_MAP_LEAFBRUSHES		0x40000
#define	MAX_MAP_PORTALS			0x20000
#define	MAX_MAP_LIGHTING		0x800000
#define	MAX_MAP_LIGHTGRID		0x100000	//%	0x800000 /* ydnar: set to points, not bytes */

#define	MAX_MAP_DRAW_SURFS		0x20000
#define	MAX_MAP_DRAW_VERTS		0x80000
#define	MAX_MAP_DRAW_INDEXES	0x80000

#define MAX_MAP_ADVERTISEMENTS	30


/* the editor uses these predefined yaw angles to orient entities up or down */
#define	ANGLE_UP				-1
#define	ANGLE_DOWN				-2

#define	LIGHTMAP_WIDTH			128
#define	LIGHTMAP_HEIGHT			128

typedef void					(*bspFunc)( const char * );


typedef struct
{
	int			offset, length;
}
bspLump_t;


typedef struct
{
	char		ident[ 4 ];
	int			version;
	
	bspLump_t	lumps[ 100 ];	/* theoretical maximum # of bsp lumps */
}
bspHeader_t;


typedef struct
{
	float		mins[ 3 ], maxs[ 3 ];
	int			firstBSPSurface, numBSPSurfaces;
	int			firstBSPBrush, numBSPBrushes;
}
bspModel_t;


typedef struct
{
	char		shader[ MAX_SHADERPATH ];
	int			surfaceFlags;
	int			contentFlags;
}
bspShader_t;


/* planes x^1 is allways the opposite of plane x */

typedef struct
{
	float		normal[ 3 ];
	float		dist;
}
bspPlane_t;


typedef struct
{
	int			planeNum;
	int			children[ 2 ];		/* negative numbers are -(leafs+1), not nodes */
	int			mins[ 3 ];			/* for frustom culling */
	int			maxs[ 3 ];
}
bspNode_t;


typedef struct
{
	int			cluster;			/* -1 = opaque cluster (do I still store these?) */
	int			area;
	
	int			mins[ 3 ];			/* for frustum culling */
	int			maxs[ 3 ];
	
	int			firstBSPLeafSurface;
	int			numBSPLeafSurfaces;
	
	int			firstBSPLeafBrush;
	int			numBSPLeafBrushes;
}
bspLeaf_t;


typedef struct
{
	int			planeNum;			/* positive plane side faces out of the leaf */
	int			shaderNum;
	int			surfaceNum;			/* RBSP */
}
bspBrushSide_t;


typedef struct
{
	int			firstSide;
	int			numSides;
	int			shaderNum;			/* the shader that determines the content flags */
}
bspBrush_t;


typedef struct
{
	char		shader[ MAX_SHADERPATH ];
	int			brushNum;
	int			visibleSide;		/* the brush side that ray tests need to clip against (-1 == none) */
}
bspFog_t;


typedef struct
{
	vec3_t		xyz;
	float		st[ 2 ];
	float		lightmap[ MAX_LIGHTMAPS ][ 2 ];	/* RBSP */
	vec3_t		normal;
	byte		color[ MAX_LIGHTMAPS ][ 4 ];	/* RBSP */
}
bspDrawVert_t;

typedef struct bspGridPoint_s
{
	byte		ambient[ MAX_LIGHTMAPS ][ 3 ];
	byte		directed[ MAX_LIGHTMAPS ][ 3 ];
	byte		styles[ MAX_LIGHTMAPS ];
	byte		latLong[ 2 ];
}
bspGridPoint_t;


typedef struct
{
	int			shaderNum;
	int			fogNum;
	int			surfaceType;
	
	int			firstVert;
	int			numVerts;
	
	int			firstIndex;
	int			numIndexes;
	
	byte		lightmapStyles[ MAX_LIGHTMAPS ];						/* RBSP */
	byte		vertexStyles[ MAX_LIGHTMAPS ];							/* RBSP */
	int			lightmapNum[ MAX_LIGHTMAPS ];							/* RBSP */
	int			lightmapX[ MAX_LIGHTMAPS ], lightmapY[ MAX_LIGHTMAPS ];	/* RBSP */
	int			lightmapWidth, lightmapHeight;
	
	vec3_t		lightmapOrigin;
	vec3_t		lightmapVecs[ 3 ];	/* on patches, [ 0 ] and [ 1 ] are lodbounds */
	
	int			patchWidth;
	int			patchHeight;
}
bspDrawSurface_t;


/* advertisements */
typedef struct {
	int			cellId;
	vec3_t		normal;
	vec3_t		rect[4];
	char		model[ MAX_SHADERPATH ];
} bspAdvertisement_t;


/* -------------------------------------------------------------------------------

general types

------------------------------------------------------------------------------- */

/* ydnar: for smaller structs */
typedef char	qb_t;


/* ydnar: for q3map_tcMod */
typedef float	tcMod_t[ 3 ][ 3 ];


/* ydnar: for multiple game support */
typedef struct surfaceParm_s
{
	char		*name;
	int			contentFlags, contentFlagsClear;
	int			surfaceFlags, surfaceFlagsClear;
	int			compileFlags, compileFlagsClear;
} surfaceParm_t;

typedef struct game_s
{
	int		maxLMSurfaceVerts;		/* default maximum meta surface verts */
	int		maxSurfaceVerts;		/* default maximum surface verts */
	int		maxSurfaceIndexes;		/* default maximum surface indexes (tris * 3) */
	bool		emitFlares;		/* when true, emit flare surfaces */
	char		*flareShader;		/* default flare shader (MUST BE SET) */
	bool		wolfLight;		/* when true, lights work like wolf q3map  */
	int		lightmapSize;		/* bsp lightmap width/height */
	float		lightmapGamma;		/* default lightmap gamma */
	float		lightmapCompensate;		/* default lightmap compensate value */
	char		*bspIdent;		/* 4-letter bsp file prefix */
	int		bspVersion;		/* bsp version to use */
	bspFunc		load, write;		/* load/write function pointers */
	surfaceParm_t	surfaceParms[128];		/* surfaceparm array */
} game_t;


typedef struct image_s
{
	char		*name;
	int		refCount;
	rgbdata_t		*pic;
} image_t;

typedef struct sun_s
{
	struct sun_s		*next;
	vec3_t				direction, color;
	float				photons, deviance, filterRadius;
	int					numSamples, style;
}
sun_t;


typedef struct surfaceModel_s 
{
	struct surfaceModel_s	*next;
	char				model[ MAX_SHADERPATH ];
	float				density, odds;
	float				minScale, maxScale;
	float				minAngle, maxAngle;
	bool			oriented;
}
surfaceModel_t;


/* ydnar/sd: foliage stuff for wolf et (engine-supported optimization of the above) */
typedef struct foliage_s 
{
	struct foliage_s	*next;
	char				model[ MAX_SHADERPATH ];
	float				scale, density, odds;
	bool			inverseAlpha;
}
foliage_t;

typedef struct foliageInstance_s
{
	vec3_t				xyz, normal;
}
foliageInstance_t;


typedef struct remap_s
{
	struct remap_s		*next;
	char				from[ 1024 ];
	char				to[ MAX_SHADERPATH ];
}
remap_t;


/* wingdi.h hack, it's the same: 0 */
#undef CM_NONE

typedef enum
{
	CM_NONE,
	CM_VOLUME,
	CM_COLOR_SET,
	CM_ALPHA_SET,
	CM_COLOR_SCALE,
	CM_ALPHA_SCALE,
	CM_COLOR_DOT_PRODUCT,
	CM_ALPHA_DOT_PRODUCT,
	CM_COLOR_DOT_PRODUCT_2,
	CM_ALPHA_DOT_PRODUCT_2
}
colorModType_t;


typedef struct colorMod_s
{
	struct colorMod_s	*next;
	colorModType_t		type;
	vec_t				data[ 16 ];
}
colorMod_t;


typedef enum
{
	IM_NONE,
	IM_OPAQUE,
	IM_MASKED,
	IM_BLEND
}
implicitMap_t;


typedef struct shaderInfo_s
{
	char				shader[ MAX_SHADERPATH ];
	int					surfaceFlags;
	int					contentFlags;
	int					compileFlags;
	float				value;							/* light value */
	
	char				*flareShader;					/* for light flares */
	char				*damageShader;					/* ydnar: sof2 damage shader name */
	char				*backShader;					/* for surfaces that generate different front and back passes */
	char				*cloneShader;					/* ydnar: for cloning of a surface */
	char				*remapShader;					/* ydnar: remap a shader in final stage */

	surfaceModel_t		*surfaceModel;					/* ydnar: for distribution of models */
	foliage_t			*foliage;						/* ydnar/splash damage: wolf et foliage */
	
	float				subdivisions;					/* from a "tesssize xxx" */
	float				backsplashFraction;				/* floating point value, usually 0.05 */
	float				backsplashDistance;				/* default 16 */
	float				lightSubdivide;					/* default 999 */
	float				lightFilterRadius;				/* ydnar: lightmap filtering/blurring radius for lights created by this shader (default: 0) */
	
	int					lightmapSampleSize;				/* lightmap sample size */
	float				lightmapSampleOffset;			/* ydnar: lightmap sample offset (default: 1.0) */
	
	float				bounceScale;					/* ydnar: radiosity re-emission [0,1.0+] */
	float				offset;							/* ydnar: offset in units */
	float				shadeAngleDegrees;				/* ydnar: breaking angle for smooth shading (degrees) */
	
	vec3_t				mins, maxs;						/* ydnar: for particle studio vertexDeform move support */
	
	qb_t				legacyTerrain;					/* ydnar: enable legacy terrain crutches */
	qb_t				indexed;						/* ydnar: attempt to use indexmap (terrain alphamap style) */
	qb_t				forceMeta;						/* ydnar: force metasurface path */
	qb_t				noClip;							/* ydnar: don't clip into bsp, preserve original face winding */
	qb_t				noFast;							/* ydnar: supress fast lighting for surfaces with this shader */
	qb_t				invert;							/* ydnar: reverse facing */
	qb_t				nonplanar;						/* ydnar: for nonplanar meta surface merging */
	qb_t				tcGen;							/* ydnar: has explicit texcoord generation */
	vec3_t				vecs[ 2 ];						/* ydnar: explicit texture vectors for [0,1] texture space */
	tcMod_t				mod;							/* ydnar: q3map_tcMod matrix for djbob :) */
	vec3_t				lightmapAxis;					/* ydnar: explicit lightmap axis projection */
	colorMod_t			*colorMod;						/* ydnar: q3map_rgb/color/alpha/Set/Mod support */
	
	int					furNumLayers;					/* ydnar: number of fur layers */
	float				furOffset;						/* ydnar: offset of each layer */
	float				furFade;						/* ydnar: alpha fade amount per layer */

	qb_t				splotchFix;						/* ydnar: filter splotches on lightmaps */
	
	qb_t				hasPasses;						/* false if the shader doesn't define any rendering passes */
	qb_t				globalTexture;					/* don't normalize texture repeats */
	qb_t				twoSided;						/* cull none */
	qb_t				autosprite;						/* autosprite shaders will become point lights instead of area lights */
	qb_t				polygonOffset;					/* ydnar: don't face cull this or against this */
	qb_t				patchShadows;					/* have patches casting shadows when using -light for this surface */
	qb_t				vertexShadows;					/* shadows will be casted at this surface even when vertex lit */
	qb_t				forceSunlight;					/* force sun light at this surface even tho we might not calculate shadows in vertex lighting */
	qb_t				notjunc;						/* don't use this surface for tjunction fixing */
	qb_t				fogParms;						/* ydnar: has fogparms */
	qb_t				noFog;							/* ydnar: supress fogging */
	qb_t				clipModel;						/* ydnar: solid model hack */
	qb_t				noVertexLight;					/* ydnar: leave vertex color alone */
	
	byte				styleMarker;					/* ydnar: light styles hack */
	
	float				vertexScale;					/* vertex light scale */
	
	char				skyParmsImageBase[ MAX_SHADERPATH ];	/* ydnar: for skies */
	
	char				editorImagePath[ MAX_SHADERPATH ];	/* use this image to generate texture coordinates */
	char				lightImagePath[ MAX_SHADERPATH ];	/* use this image to generate color / averageColor */
	char				normalImagePath[ MAX_SHADERPATH ];	/* ydnar: normalmap image for bumpmapping */
	
	implicitMap_t		implicitMap;					/* ydnar: enemy territory implicit shaders */
	char				implicitImagePath[ MAX_SHADERPATH ];
	
	image_t				*shaderImage;
	image_t				*lightImage;
	image_t				*normalImage;
	
	float				skyLightValue;					/* ydnar */
	int					skyLightIterations;				/* ydnar */
	sun_t				*sun;							/* ydnar */
	
	vec3_t				color;							/* normalized color */
	vec3_t				averageColor;
	byte				lightStyle;
	
	qb_t				lmMergable;						/* ydnar */
	int					lmCustomWidth, lmCustomHeight;	/* ydnar */
	float				lmBrightness;					/* ydnar */
	float				lmFilterRadius;					/* ydnar: lightmap filtering/blurring radius for this shader (default: 0) */
	
	int					shaderWidth, shaderHeight;		/* ydnar */
	float				stFlat[ 2 ];
	
	vec3_t				fogDir;							/* ydnar */
	
	char				*shaderText;					/* ydnar */
	qb_t				custom;
	qb_t				finished;
}
shaderInfo_t;



/* -------------------------------------------------------------------------------

bsp structures

------------------------------------------------------------------------------- */

typedef struct face_s
{
	struct face_s		*next;
	int					planenum;
	int					priority;
	bool			checked;
	int					compileFlags;
	winding_t			*w;
}
face_t;

typedef struct plane_s
{
	vec3_t				normal;
	vec_t				dist;
	int					type;
	struct plane_s		*hash_chain;
} plane_t;

typedef struct
{
	vec3_t		UAxis;
	vec3_t		VAxis;
	float		shift[2];
	float		rotate;
	float		scale[2];
} wrl_vecs;

typedef struct
{
	float		vecs[2][4];
} qrk_vecs;

typedef union
{
	qrk_vecs		quark;
	wrl_vecs		hammer;
} vects_t;

typedef struct side_s
{
	int		planenum;
	int		outputNum;	// set when the side is written to the file list
	
	float		texMat[2][3];	// quake3 brush primitive texture matrix
	float		vecs[2][4];	// classic texture coordinate mapping

	winding_t		*winding;
	winding_t		*visibleHull;	// convex hull of all visible fragments

	shaderInfo_t	*shaderInfo;

	int		contentFlags;	// from shaderInfo
	int		surfaceFlags;	// from shaderInfo
	int		compileFlags;	// from shaderInfo
	int		value;		// from shaderInfo

	bool		visible;		// choose visble planes first
	bool		bevel;		// don't ever use for bsp splitting, and don't bother making windings for it
	bool		culled;		// face culling
} side_t;

typedef struct sideRef_s
{
	struct sideRef_s	*next;
	side_t				*side;
}
sideRef_t;


/* ydnar: generic index mapping for entities (natural extension of terrain texturing) */
typedef struct indexMap_s
{
	int					w, h, numLayers;
	char				name[ MAX_SHADERPATH ], shader[ MAX_SHADERPATH ];
	float				offsets[ 256 ];
	byte				*pixels;
}
indexMap_t;


typedef struct brush_s
{
	struct brush_s		*next;
	struct brush_s		*nextColorModBrush;	/* ydnar: colorMod volume brushes go here */
	struct brush_s		*original;			/* chopped up brushes will reference the originals */
	
	int					entityNum, brushNum;/* editor numbering */
	int					outputNum;			/* set when the brush is written to the file list */
	
	/* ydnar: for shadowcasting entities */
	int					castShadows;
	int					recvShadows;
	
	shaderInfo_t		*contentShader;
	shaderInfo_t		*celShader;			/* :) */
	
	/* ydnar: gs mods */
	float				lightmapScale;
	vec3_t				eMins, eMaxs;
	indexMap_t			*im;

	int					contentFlags;
	int					compileFlags;		/* ydnar */
	bool			detail;
	bool			opaque;

	int					portalareas[ 2 ];

	vec3_t				mins, maxs;
	int					numsides;
	
	side_t				sides[ 6 ];			/* variably sized */
}
brush_t;


typedef struct fog_s
{
	shaderInfo_t		*si;
	brush_t				*brush;
	int					visibleSide;		/* the brush side that ray tests need to clip against (-1 == none) */
}
fog_t;


typedef struct
{
	int					width, height;
	bspDrawVert_t		*verts;
}
mesh_t;


typedef struct parseMesh_s
{
	struct parseMesh_s	*next;
	
	int					entityNum, brushNum;	/* ydnar: editor numbering */
	
	/* ydnar: for shadowcasting entities */
	int					castShadows;
	int					recvShadows;
	
	mesh_t				mesh;
	shaderInfo_t		*shaderInfo;
	shaderInfo_t		*celShader;				/* :) */
	
	/* ydnar: gs mods */
	float				lightmapScale;
	vec3_t				eMins, eMaxs;
	indexMap_t			*im;
	
	/* grouping */
	bool			grouped;
	float				longestCurve;
	int					maxIterations;
}
parseMesh_t;


/*
	ydnar: the drawsurf struct was extended to allow for:
	- non-convex planar surfaces
	- non-planar brushface surfaces
	- lightmapped terrain
	- planar patches
*/

typedef enum
{
	/* ydnar: these match up exactly with bspSurfaceType_t */
	SURFACE_BAD,
	SURFACE_FACE,
	SURFACE_PATCH,
	SURFACE_TRIANGLES,
	SURFACE_FLARE,
	SURFACE_FOLIAGE,	/* wolf et */
	
	/* ydnar: compiler-relevant surface types */
	SURFACE_FORCED_META,
	SURFACE_META,
	SURFACE_FOGHULL,
	SURFACE_DECAL,
	SURFACE_SHADER,
	
	NUM_SURFACE_TYPES
}
surfaceType_t;

char			*surfaceTypes[ NUM_SURFACE_TYPES ]
#ifndef MAIN_C
				;
#else
				=
				{
					"SURFACE_BAD",
					"SURFACE_FACE",
					"SURFACE_PATCH",
					"SURFACE_TRIANGLES",
					"SURFACE_FLARE",
					"SURFACE_FOLIAGE",
					"SURFACE_FORCED_META",
					"SURFACE_META",
					"SURFACE_FOGHULL",
					"SURFACE_DECAL",
					"SURFACE_SHADER"
				};
#endif


/* ydnar: this struct needs an overhaul (again, heh) */
typedef struct mapDrawSurface_s
{
	surfaceType_t		type;
	bool			planar;
	int					outputNum;			/* ydnar: to match this sort of thing up */
	
	bool			fur;				/* ydnar: this is kind of a hack, but hey... */
	bool			skybox;				/* ydnar: yet another fun hack */
	bool			backSide;			/* ydnar: q3map_backShader support */
	
	struct mapDrawSurface_s	*parent;		/* ydnar: for cloned (skybox) surfaces to share lighting data */
	struct mapDrawSurface_s	*clone;			/* ydnar: for cloned surfaces */
	struct mapDrawSurface_s	*cel;			/* ydnar: for cloned cel surfaces */
	
	shaderInfo_t		*shaderInfo;
	shaderInfo_t		*celShader;
	brush_t				*mapBrush;
	parseMesh_t			*mapMesh;
	sideRef_t			*sideRef;
	
	int					fogNum;
	
	int					numVerts;			/* vertexes and triangles */
	bspDrawVert_t		*verts;
	int					numIndexes;
	int					*indexes;
	
	int					planeNum;
	vec3_t				lightmapOrigin;		/* also used for flares */
	vec3_t				lightmapVecs[ 3 ];	/* also used for flares */
	int					lightStyle;			/* used for flares */
	
	/* ydnar: per-surface (per-entity, actually) lightmap sample size scaling */
	float				lightmapScale;
	
	/* ydnar: surface classification */
	vec3_t				mins, maxs;
	vec3_t				lightmapAxis;
	int					sampleSize;
	
	/* ydnar: shadow group support */
	int					castShadows, recvShadows;
	
	/* ydnar: texture coordinate range monitoring for hardware with limited texcoord precision (in texel space) */
	float				bias[ 2 ];
	int					texMins[ 2 ], texMaxs[ 2 ], texRange[ 2 ];
		
	/* ydnar: for patches */
	float				longestCurve;
	int					maxIterations;
	int					patchWidth, patchHeight;
	vec3_t				bounds[ 2 ];
	
	/* ydnar/sd: for foliage */
	int					numFoliageInstances;
	
	/* ydnar: editor/useful numbering */
	int					entityNum;
	int					surfaceNum;
}
mapDrawSurface_t;


typedef struct drawSurfRef_s
{
	struct drawSurfRef_s	*nextRef;
	int					outputNum;
}
drawSurfRef_t;


/* ydnar: metasurfaces are constructed from lists of metatriangles so they can be merged in the best way */
typedef struct metaTriangle_s
{
	shaderInfo_t		*si;
	side_t				*side;
	int					entityNum, surfaceNum, planeNum, fogNum, sampleSize, castShadows, recvShadows;
	vec4_t				plane;
	vec3_t				lightmapAxis;
	int					indexes[ 3 ];
}
metaTriangle_t;


typedef struct epair_s
{
	struct epair_s		*next;
	char				*key, *value;
}
epair_t;


typedef struct
{
	vec3_t				origin;
	brush_t				*brushes, *lastBrush, *colorModBrushes;
	parseMesh_t			*patches;
	int					mapEntityNum, firstDrawSurf;
	int					firstBrush, numBrushes;		/* only valid during BSP compile */
	epair_t				*epairs;
}
entity_t;


typedef struct node_s
{
	/* both leafs and nodes */
	int					planenum;		/* -1 = leaf node */
	struct node_s		*parent;
	vec3_t				mins, maxs;		/* valid after portalization */
	brush_t				*volume;		/* one for each leaf/node */

	/* nodes only */
	side_t				*side;			/* the side that created the node */
	struct node_s		*children[ 2 ];
	int					compileFlags;	/* ydnar: hint, antiportal */
	int					tinyportals;
	vec3_t				referencepoint;

	/* leafs only */
	bool			opaque;			/* view can never be inside */
	bool			areaportal;
	bool			skybox;			/* ydnar: a skybox leaf */
	bool			sky;			/* ydnar: a sky leaf */
	int					cluster;		/* for portalfile writing */
	int					area;			/* for areaportals */
	brush_t				*brushlist;		/* fragments of all brushes in this leaf */
	drawSurfRef_t		*drawSurfReferences;

	int					occupied;		/* 1 or greater can reach entity */
	entity_t			*occupant;		/* for leak file testing */

	struct portal_s		*portals;		/* also on nodes during construction */
}
node_t;


typedef struct portal_s
{
	plane_t				plane;
	node_t				*onnode;		/* NULL = outside box */
	node_t				*nodes[ 2 ];	/* [ 0 ] = front side of plane */
	struct portal_s		*next[ 2 ];
	winding_t			*winding;

	bool			sidefound;		/* false if ->side hasn't been checked */
	int					compileFlags;	/* from original face that caused the split */
	side_t				*side;			/* NULL = non-visible */
}
portal_t;


typedef struct
{
	node_t				*headnode;
	node_t				outside_node;
	vec3_t				mins, maxs;
}
tree_t;



/* -------------------------------------------------------------------------------

vis structures

------------------------------------------------------------------------------- */

typedef struct
{
	vec3_t				normal;
	float				dist;
}
visPlane_t;


typedef struct
{
	int					numpoints;
	vec3_t				points[	MAX_POINTS_ON_FIXED_WINDING	];		/* variable sized */
} 
fixedWinding_t;


typedef struct passage_s
{
	struct passage_s	*next;
	byte				cansee[ 1 ];	/* all portals that can be seen through this passage */
} passage_t;


typedef enum
{
	stat_none,
	stat_working,
	stat_done
}
vstatus_t;


typedef struct
{
	int					num;
	bool			hint;			/* true if this portal was created from a hint splitter */
	bool			removed;
	visPlane_t			plane;			/* normal pointing into neighbor */
	int					leaf;			/* neighbor */
	
	vec3_t				origin;			/* for fast clip testing */
	float				radius;

	fixedWinding_t		*winding;
	vstatus_t			status;
	byte				*portalfront;	/* [portals], preliminary */
	byte				*portalflood;	/* [portals], intermediate */
	byte				*portalvis;		/* [portals], final */

	int					nummightsee;	/* bit count on portalflood for sort */
	passage_t			*passages;		/* there are just as many passages as there */
										/* are portals in the leaf this portal leads */
}
vportal_t;


typedef struct leaf_s
{
	int					numportals;
	int					merged;
	vportal_t			*portals[MAX_PORTALS_ON_LEAF];
}
leaf_t;

	
typedef struct pstack_s
{
	byte				mightsee[ MAX_PORTALS / 8 ];
	struct pstack_s		*next;
	leaf_t				*leaf;
	vportal_t			*portal;		/* portal exiting */
	fixedWinding_t		*source;
	fixedWinding_t		*pass;

	fixedWinding_t		windings[ 3 ];	/* source, pass, temp in any order */
	int					freewindings[ 3 ];

	visPlane_t			portalplane;
	int depth;
	visPlane_t			seperators[ 2 ][ MAX_SEPERATORS ];
	int					numseperators[ 2 ];
}
pstack_t;


typedef struct
{
	vportal_t			*base;
	int					c_chains;
	pstack_t			pstack_head;
}
threaddata_t;



/* -------------------------------------------------------------------------------

light structures

------------------------------------------------------------------------------- */

/* ydnar: new light struct with flags */
typedef struct light_s
{
	struct light_s	*next;
	
	int		type;
	int		flags;		// condensed all the booleans into one flags int
	shaderInfo_t	*si;
	
	vec3_t		origin;
	vec3_t		normal;		// for surfaces, spotlights, and suns
	float		dist;		// plane location along normal
	
	float		photons;
	int		style;

	union
	{
		vec3_t	color;
		vec3_t	intensity;
	};

	float		radiusByDist;	// for spotlights
	float		fade;		// from wolf, for linear lights
	float		angleScale;	// stolen from vlight for K

	float		add;		// used for area lights
	float		envelope;		// units until falloff < tolerance
	float		envelope2;	// envelope squared (tiny optimization)
	vec3_t		mins, maxs;	// pvs envelope
	int		cluster;		// cluster light falls into
	
	winding_t		*w;
	vec3_t		emitColor;	// full out-of-gamut value
	
	float		falloffTolerance;	// minimum attenuation threshold
	float		filterRadius;	// lightmap filter radius in world units, 0 == default
} light_t;


typedef struct
{
	/* constant input */
	bool			testOcclusion, forceSunlight, testAll;
	int					recvShadows;
	
	int					numSurfaces;
	int					*surfaces;
	
	int					numLights;
	light_t				**lights;
	
	bool			twoSided;
	
	/* per-sample input */
	int					cluster;
	vec3_t				origin, normal;
	vec_t				inhibitRadius;	/* sphere in which occluding geometry is ignored */
	
	/* per-light input */
	light_t				*light;
	vec3_t				end;
	
	/* calculated input */
	vec3_t				displacement, direction;
	vec_t				distance;
	
	/* input and output */
	vec3_t				color;			/* starts out at full color, may be reduced if transparent surfaces are crossed */
	
	/* output */
	vec3_t				hit;
	int					compileFlags;	/* for determining surface compile flags traced through */
	bool			passSolid;
	bool			opaque;
	
	/* working data */
	int					numTestNodes;
	int					testNodes[ MAX_TRACE_TEST_NODES ]; 
} light_trace_t;



/* must be identical to bspDrawVert_t except for float color! */
typedef struct
{
	vec3_t				xyz;
	float				st[ 2 ];
	float				lightmap[ MAX_LIGHTMAPS ][ 2 ];
	vec3_t				normal;
	float				color[ MAX_LIGHTMAPS ][ 4 ];
}
radVert_t;


typedef struct
{
	int					numVerts;
	radVert_t			verts[ MAX_POINTS_ON_WINDING ];
}
radWinding_t;


/* crutch for poor local allocations in win32 smp */
typedef struct
{
	vec_t				dists[ MAX_POINTS_ON_WINDING + 4 ];
	int					sides[ MAX_POINTS_ON_WINDING + 4 ];
}
clipWork_t;


/* ydnar: new lightmap handling code */
typedef struct outLightmap_s
{
	int					lightmapNum, extLightmapNum;
	int					customWidth, customHeight;
	int					numLightmaps;
	int					freeLuxels;
	int					numShaders;
	shaderInfo_t		*shaders[ MAX_LIGHTMAP_SHADERS ];
	byte				*lightBits;
	byte				*bspLightBytes;
	byte				*bspDirBytes;
}
outLightmap_t;


typedef struct rawLightmap_s
{
	bool				finished, splotchFix, wrap[ 2 ];
	int						customWidth, customHeight;
	float					brightness;
	float					filterRadius;
	
	int						firstLightSurface, numLightSurfaces;	/* index into lightSurfaces */
	int						numLightClusters, *lightClusters;
	
	int						sampleSize, actualSampleSize, axisNum;
	int						entityNum;
	int						recvShadows;
	vec3_t					mins, maxs, axis, origin, *vecs;
	float					*plane;
	int						w, h, sw, sh, used;
	
	bool				solid[ MAX_LIGHTMAPS ];
	vec3_t					solidColor[ MAX_LIGHTMAPS ];
	
	int						numStyledTwins;
	struct rawLightmap_s	*twins[ MAX_LIGHTMAPS ];

	int						outLightmapNums[ MAX_LIGHTMAPS ];
	int						twinNums[ MAX_LIGHTMAPS ];
	int						lightmapX[ MAX_LIGHTMAPS ], lightmapY[ MAX_LIGHTMAPS ];
	byte					styles[ MAX_LIGHTMAPS ];
	float					*bspLuxels[ MAX_LIGHTMAPS ];
	float					*radLuxels[ MAX_LIGHTMAPS ];
	float					*superLuxels[ MAX_LIGHTMAPS ];
	float					*superOrigins;
	float					*superNormals;
	int						*superClusters;
	
	float					*superDeluxels;	/* average light direction */
	float					*bspDeluxels;
}
rawLightmap_t;


typedef struct rawGridPoint_s
{
	vec3_t				ambient[ MAX_LIGHTMAPS ];
	vec3_t				directed[ MAX_LIGHTMAPS ];
	vec3_t				dir;
	byte				styles[ MAX_LIGHTMAPS ];
}
rawGridPoint_t;


typedef struct surfaceInfo_s
{
	bspModel_t	*model;
	shaderInfo_t	*si;
	rawLightmap_t	*lm;
	int		parentSurfaceNum, childSurfaceNum;
	int		entityNum, castShadows, recvShadows, sampleSize, patchIterations;
	float		longestCurve;
	float		*plane;
	vec3_t		axis, mins, maxs;
	bool		hasLightmap, approximated;
	int		firstSurfaceCluster, numSurfaceClusters;
} surfaceInfo_t;

/*
-------------------------------------------------------------------------------

prototypes

-------------------------------------------------------------------------------
*/

/* bsplib.c */
float		Random( void );
int		BSPInfo( int count, char **fileNames );
int		ScaleBSPMain( int argc, char **argv );
int		ConvertMain( int argc, char **argv );

/* md5.c */
typedef struct
{
	uint	state[4];
	uint	bits[2];
	byte	in[64];
} MD5_CTX;

void MD5_Init( MD5_CTX *ctx );
void MD5_Update( MD5_CTX *ctx, const byte *buf, uint len );
void MD5_Final( MD5_CTX *ctx, byte digest[16] );
dword MD5_BlockChecksum( const void *data, int length );

/* bsp.c */
int	BSPMain( int argc, char **argv );


/* convert_map.c */
int	ConvertBSPToMap( char *bspName );


/* convert_ase.c */
int	ConvertBSPToASE( char *bspName );


/* brush.c */
sideRef_t					*AllocSideRef( side_t *side, sideRef_t *next );
int							CountBrushList( brush_t *brushes );
brush_t						*AllocBrush( int numsides );
void						FreeBrush( brush_t *brushes );
void						FreeBrushList( brush_t *brushes );
brush_t						*CopyBrush( brush_t *brush );
bool					BoundBrush( brush_t *brush );
bool					CreateBrushWindings( brush_t *brush );
brush_t						*BrushFromBounds( vec3_t mins, vec3_t maxs );
vec_t						BrushVolume( brush_t *brush );
void						WriteBSPBrushMap( char *name, brush_t *list );

void						FilterDetailBrushesIntoTree( entity_t *e, tree_t *tree );
void						FilterStructuralBrushesIntoTree( entity_t *e, tree_t *tree );

bool					WindingIsTiny( winding_t *w );

void						SplitBrush( brush_t *brush, int planenum, brush_t **front, brush_t **back);

tree_t						*AllocTree( void );
node_t						*AllocNode( void );


/* mesh.c */
void						LerpDrawVert( bspDrawVert_t *a, bspDrawVert_t *b, bspDrawVert_t *out );
void						LerpDrawVertAmount( bspDrawVert_t *a, bspDrawVert_t *b, float amount, bspDrawVert_t *out );
void						FreeMesh( mesh_t *m );
mesh_t						*CopyMesh( mesh_t *mesh );
void						PrintMesh( mesh_t *m );
mesh_t						*TransposeMesh( mesh_t *in );
void						InvertMesh( mesh_t *m );
mesh_t						*SubdivideMesh( mesh_t in, float maxError, float minLength );
int							IterationsForCurve( float len, int subdivisions );
mesh_t						*SubdivideMesh2( mesh_t in, int iterations );
mesh_t						*SubdivideMeshQuads( mesh_t *in, float minLength, int maxsize, int *widthtable, int *heighttable );
mesh_t						*RemoveLinearMeshColumnsRows( mesh_t *in );
void						MakeMeshNormals( mesh_t in );
void						PutMeshOnCurve( mesh_t in );

// map.c
void	LoadMapFile( const char *filename, bool onlyLights );
int	FindFloatPlane( vec3_t normal, vec_t dist, int numPoints, vec3_t *points );
bool	PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c );
int	MapPlaneTypeForNormal( const vec3_t normal );
void	AddBrushBevels( void );
brush_t	*FinishBrush( void );


/* portals.c */
void						MakeHeadnodePortals( tree_t *tree );
void						MakeNodePortal( node_t *node );
void						SplitNodePortals( node_t *node );

bool					PortalPassable( portal_t *p );

bool					FloodEntities( tree_t *tree );
void						FillOutside( node_t *headnode);
void						FloodAreas( tree_t *tree);
face_t						*VisibleFaces( entity_t *e, tree_t *tree );
void						FreePortal( portal_t *p );

void						MakeTreePortals( tree_t *tree );


/* leakfile.c */
void						LeakFile( tree_t *tree );

/* prtfile.c */
void						NumberClusters( tree_t *tree );
void						WritePortalFile( tree_t *tree );


/* writebsp.c */
void						SetModelNumbers( void );
void						SetLightStyles( void );

int							EmitShader( const char *shader, int *contentFlags, int *surfaceFlags );

void						BeginBSPFile( void );
void						EndBSPFile( void );
void						EmitBrushes( brush_t *brushes, int *firstBrush, int *numBrushes );
void						EmitFogs( void );

void						BeginModel( void );
void						EndModel( entity_t *e, node_t *headnode );


/* tree.c */
void						FreeTree( tree_t *tree );
void						FreeTree_r( node_t *node );
void						PrintTree_r( node_t *node, int depth );
void						FreeTreePortals_r( node_t *node );


/* patch.c */
void						ParsePatch( bool onlyLights );
mesh_t						*SubdivideMesh( mesh_t in, float maxError, float minLength );
void						PatchMapDrawSurfs( entity_t *e );


/* tjunction.c */
void						FixTJunctions( entity_t *e );


/* fog.c */
winding_t					*WindingFromDrawSurf( mapDrawSurface_t *ds );
void						FogDrawSurfaces( entity_t *e );
int							FogForPoint( vec3_t point, float epsilon );
int							FogForBounds( vec3_t mins, vec3_t maxs, float epsilon );
void						CreateMapFogs( void );


/* facebsp.c */
face_t						*MakeStructuralBSPFaceList( brush_t *list );
face_t						*MakeVisibleBSPFaceList( brush_t *list );
tree_t						*FaceBSP( face_t *list );


/* model.c */
void		*PicoMalloc( size_t size );
void		PicoFree( void *mem );
void		PicoPrintFunc( int level, const char *str );
void		PicoLoadFileFunc( char *name, byte **buffer, int *bufSize );
picoModel_t	*FindModel( char *name, int frame );
picoModel_t	*LoadModel( char *name, int frame );
void		InsertModel( char *name, int frame, matrix4x4 transform, remap_t *remap, shaderInfo_t *celShader, int eNum, int castShadows, int recvShadows, int spawnFlags, float lightmapScale );
void		AddTriangleModels( entity_t *e );


/* surface.c */
mapDrawSurface_t			*AllocDrawSurface( surfaceType_t type );
void						FinishSurface( mapDrawSurface_t *ds );
void						StripFaceSurface( mapDrawSurface_t *ds );
bool					CalcSurfaceTextureRange( mapDrawSurface_t *ds );
bool					CalcLightmapAxis( vec3_t normal, vec3_t axis );
void						ClassifySurfaces( int numSurfs, mapDrawSurface_t *ds );
void						ClassifyEntitySurfaces( entity_t *e );
void						TidyEntitySurfaces( entity_t *e );
mapDrawSurface_t			*CloneSurface( mapDrawSurface_t *src, shaderInfo_t *si );
mapDrawSurface_t			*MakeCelSurface( mapDrawSurface_t *src, shaderInfo_t *si );
bool					IsTriangleDegenerate( bspDrawVert_t *points, int a, int b, int c );
void						ClearSurface( mapDrawSurface_t *ds );
void						AddEntitySurfaceModels( entity_t *e );
mapDrawSurface_t			*DrawSurfaceForSide( entity_t *e, brush_t *b, side_t *s, winding_t *w );
mapDrawSurface_t			*DrawSurfaceForMesh( entity_t *e, parseMesh_t *p, mesh_t *mesh );
mapDrawSurface_t			*DrawSurfaceForFlare( int entNum, vec3_t origin, vec3_t normal, vec3_t color, char *flareShader, int lightStyle );
mapDrawSurface_t			*DrawSurfaceForShader( char *shader );
void						ClipSidesIntoTree( entity_t *e, tree_t *tree );
void						MakeDebugPortalSurfs( tree_t *tree );
void						MakeFogHullSurfs( entity_t *e, tree_t *tree, char *shader );
void						SubdivideFaceSurfaces( entity_t *e, tree_t *tree );
void						AddEntitySurfaceModels( entity_t *e );
int							AddSurfaceModels( mapDrawSurface_t *ds );
void						FilterDrawsurfsIntoTree( entity_t *e, tree_t *tree );


/* surface_fur.c */
void						Fur( mapDrawSurface_t *src );


/* surface_foliage.c */
void						Foliage( mapDrawSurface_t *src );


/* ydnar: surface_meta.c */
void						ClearMetaTriangles( void );
int							FindMetaTriangle( metaTriangle_t *src, bspDrawVert_t *a, bspDrawVert_t *b, bspDrawVert_t *c, int planeNum );
void						MakeEntityMetaTriangles( entity_t *e );
void						FixMetaTJunctions( void );
void						SmoothMetaTriangles( void );
void						MergeMetaTriangles( void );


/* surface_extra.c */
void						SetDefaultSampleSize( int sampleSize );

void						SetSurfaceExtra( mapDrawSurface_t *ds, int num );

shaderInfo_t				*GetSurfaceExtraShaderInfo( int num );
int							GetSurfaceExtraParentSurfaceNum( int num );
int							GetSurfaceExtraEntityNum( int num );
int							GetSurfaceExtraCastShadows( int num );
int							GetSurfaceExtraRecvShadows( int num );
int							GetSurfaceExtraSampleSize( int num );
float						GetSurfaceExtraLongestCurve( int num );
void						GetSurfaceExtraLightmapAxis( int num, vec3_t lightmapAxis );

void						WriteSurfaceExtraFile( const char *path );
void						LoadSurfaceExtraFile( const char *path );


/* decals.c */
void						ProcessDecals( void );
void						MakeEntityDecals( entity_t *e );


/* brush_primit.c */
void						ComputeAxisBase( vec3_t normal, vec3_t texX, vec3_t texY);


/* vis.c */
fixedWinding_t		*NewFixedWinding( int points );
int			VisMain( int argc, char **argv );

/* visflow.c */
int			CountBits( byte *bits, int numbits );
void						PassageFlow( int portalnum );
void						CreatePassages( int portalnum );
void						PassageMemory( void );
void						BasePortalVis( int portalnum );
void						BetterPortalVis( int portalnum );
void						PortalFlow( int portalnum );
void						PassagePortalFlow( int portalnum );



/* light.c  */
float		PointToPolygonFormFactor( const vec3_t point, const vec3_t normal, const winding_t *w );
int		LightContributionToSample( light_trace_t *trace );
void		LightingAtSample( light_trace_t *trace, byte styles[MAX_LIGHTMAPS], vec3_t colors[MAX_LIGHTMAPS] );
int		LightContributionToPoint( light_trace_t *trace );
int		LightMain( int argc, char **argv );


/* light_trace.c */
void						SetupTraceNodes( void );
void						TraceLine( light_trace_t *trace );
float						SetupTrace( light_trace_t *trace );


/* light_bounce.c */
bool						RadSampleImage( rgbdata_t *pic, float st[2], float color[4] );
void						RadLightForTriangles( int num, int lightmapNum, rawLightmap_t *lm, shaderInfo_t *si, float scale, float subdivide, clipWork_t *cw );
void						RadLightForPatch( int num, int lightmapNum, rawLightmap_t *lm, shaderInfo_t *si, float scale, float subdivide, clipWork_t *cw );
void						RadCreateDiffuseLights( void );
void						RadFreeLights();


/* light_ydnar.c */
void						ColorToBytes( const float *color, byte *colorBytes, float scale );
void						SmoothNormals( void );

void						MapRawLightmap( int num );

void						SetupDirt();
float						DirtForSample( light_trace_t *trace );
void						DirtyRawLightmap( int num );

void						IlluminateRawLightmap( int num );
void						IlluminateVertexes( int num );

void						SetupBrushes( void );
void						SetupClusters( void );
bool					ClusterVisible( int a, int b );
bool					ClusterVisibleToPoint( vec3_t point, int cluster );
int							ClusterForPoint( vec3_t point );
int							ClusterForPointExt( vec3_t point, float epsilon );
int							ClusterForPointExtFilter( vec3_t point, float epsilon, int numClusters, int *clusters );
int							ShaderForPointInLeaf( vec3_t point, int leafNum, float epsilon, int wantContentFlags, int wantSurfaceFlags, int *contentFlags, int *surfaceFlags );
void						SetupEnvelopes( bool forGrid, bool fastFlag );
void						FreeTraceLights( light_trace_t *trace );
void						CreateTraceLightsForBounds( vec3_t mins, vec3_t maxs, vec3_t normal, int numClusters, int *clusters, int flags, light_trace_t *trace );
void						CreateTraceLightsForSurface( int num, light_trace_t *trace );


/* lightmaps_ydnar.c */
void						ExportLightmaps( void );

int		ExportLightmapsMain( int argc, char **argv );
int		ImportLightmapsMain( int argc, char **argv );

void						SetupSurfaceLightmaps( void );
void						StitchSurfaceLightmaps( void );
void						StoreSurfaceLightmaps( void );


/* image.c */
void						ImageFree( image_t *image );
image_t						*ImageFind( const char *filename );
image_t						*ImageLoad( const char *filename );


/* shaders.c */
void						ColorMod( colorMod_t *am, int numVerts, bspDrawVert_t *drawVerts );

void						TCMod( tcMod_t mod, float st[ 2 ] );
void						TCModIdentity( tcMod_t mod );
void						TCModMultiply( tcMod_t a, tcMod_t b, tcMod_t out );
void						TCModTranslate( tcMod_t mod, float s, float t );
void						TCModScale( tcMod_t mod, float s, float t );
void						TCModRotate( tcMod_t mod, float euler );

bool					ApplySurfaceParm( char *name, int *contentFlags, int *surfaceFlags, int *compileFlags );

void						BeginMapShaderFile( const char *mapFile );
void						WriteMapShaderFile( void );
shaderInfo_t				*CustomShader( shaderInfo_t *si, char *find, char *replace );
void						EmitVertexRemapShader( char *from, char *to );

void						LoadShaderInfo( void );
shaderInfo_t				*ShaderInfoForShader( const char *shader );


/* bspfile_abstract.c */
void						SetGridPoints( int n );
void						SetDrawVerts( int n );
void						IncDrawVerts();
void						SetDrawSurfaces(int n);
void						SetDrawSurfacesBuffer();
void						BSPFilesCleanup();

void						SwapBlock( int *block, int size );

int							GetLumpElements( bspHeader_t *header, int lump, int size );
void						*GetLump( bspHeader_t *header, int lump );
int							CopyLump( bspHeader_t *header, int lump, void *dest, int size );
void						AddLump( file_t *file, bspHeader_t *header, int lumpNum, const void *data, int length );

void						LoadBSPFile( const char *filename );
void						WriteBSPFile( const char *filename );
void						PrintBSPFileSizes( void );

epair_t 		*ParseEpair( script_t *script, token_t *token );
void		ParseEntities( void );
void		UnparseEntities( void );
void		PrintEntity( const entity_t *ent );
void		SetKeyValue( entity_t *ent, const char *key, const char *value );
const char	*ValueForKey( const entity_t *ent, const char *key );
int		IntForKey( const entity_t *ent, const char *key );
float		FloatForKey( const entity_t *ent, const char *key );
void		GetVectorForKey( const entity_t *ent, const char *key, vec3_t vec );
entity_t		*FindTargetEntity( const char *target );
void		GetEntityShadowFlags( const entity_t *ent, const entity_t *ent2, int *castShadows, int *recvShadows );


/* bspfile_ibsp.c */
void						LoadIBSPFile( const char *filename );
void						WriteIBSPFile( const char *filename );


/* bspfile_rbsp.c */
void						LoadRBSPFile( const char *filename );
void						WriteRBSPFile( const char *filename );



/* -------------------------------------------------------------------------------

bsp/general global variables

------------------------------------------------------------------------------- */

#ifdef MAIN_C
	#define Q_EXTERN
	#define Q_ASSIGN( a )	= a
#else
	#define Q_EXTERN extern
	#define Q_ASSIGN( a )	
#endif

/* game support */
Q_EXTERN game_t				games[]
#ifndef MAIN_C
;
#else
=
{
#include "game_quake3.h"
,
#include "game_xash.h"
,
{ 0 }	// terminator
};
#endif

Q_EXTERN game_t				*game Q_ASSIGN( &games[ 0 ] );

/* general */
Q_EXTERN int				numImages Q_ASSIGN( 0 );
Q_EXTERN image_t			images[ MAX_IMAGES ];

Q_EXTERN int				numPicoModels Q_ASSIGN( 0 );
Q_EXTERN picoModel_t		*picoModels[ MAX_MODELS ];

Q_EXTERN shaderInfo_t		*shaderInfo Q_ASSIGN( NULL );
Q_EXTERN int				numShaderInfo Q_ASSIGN( 0 );
Q_EXTERN int				numVertexRemaps Q_ASSIGN( 0 );

Q_EXTERN surfaceParm_t		custSurfaceParms[ MAX_CUST_SURFACEPARMS ];
Q_EXTERN int				numCustSurfaceParms Q_ASSIGN( 0 );

Q_EXTERN char				mapName[ MAX_SHADERPATH ];	/* ydnar: per-map custom shaders for larger lightmaps */
Q_EXTERN char				mapShaderFile[ 1024 ];
Q_EXTERN bool			warnImage Q_ASSIGN( true );

/* ydnar: sinusoid samples */
Q_EXTERN float				jitters[ MAX_JITTERS ];


/* commandline arguments */
Q_EXTERN bool			force Q_ASSIGN( false );
Q_EXTERN bool			infoMode Q_ASSIGN( false );
Q_EXTERN bool			useCustomInfoParms Q_ASSIGN( false );
Q_EXTERN bool			noprune Q_ASSIGN( false );
Q_EXTERN bool			leaktest Q_ASSIGN( false );
Q_EXTERN bool			nodetail Q_ASSIGN( false );
Q_EXTERN bool			nosubdivide Q_ASSIGN( false );
Q_EXTERN bool			notjunc Q_ASSIGN( false );
Q_EXTERN bool			fulldetail Q_ASSIGN( false );
Q_EXTERN bool			nowater Q_ASSIGN( false );
Q_EXTERN bool			noCurveBrushes Q_ASSIGN( false );
Q_EXTERN bool			fakemap Q_ASSIGN( false );
Q_EXTERN bool			coplanar Q_ASSIGN( false );
Q_EXTERN bool			nofog Q_ASSIGN( false );
Q_EXTERN bool			noHint Q_ASSIGN( false );				/* ydnar */
Q_EXTERN bool			renameModelShaders Q_ASSIGN( false );	/* ydnar */
Q_EXTERN bool			skyFixHack Q_ASSIGN( false );			/* ydnar */

Q_EXTERN cvar_t			*patch_subdivide;

Q_EXTERN int				maxLMSurfaceVerts Q_ASSIGN( 64 );		/* ydnar */
Q_EXTERN int				maxSurfaceVerts Q_ASSIGN( 999 );		/* ydnar */
Q_EXTERN int				maxSurfaceIndexes Q_ASSIGN( 6000 );		/* ydnar */
Q_EXTERN float				npDegrees Q_ASSIGN( 0.0f );				/* ydnar: nonplanar degrees */
Q_EXTERN int				bevelSnap Q_ASSIGN( 0 );				/* ydnar: bevel plane snap */
Q_EXTERN int				texRange Q_ASSIGN( 0 );
Q_EXTERN bool			flat Q_ASSIGN( false );
Q_EXTERN bool			meta Q_ASSIGN( false );
Q_EXTERN bool			patchMeta Q_ASSIGN( false );
Q_EXTERN bool			emitFlares Q_ASSIGN( false );
Q_EXTERN bool			debugSurfaces Q_ASSIGN( false );
Q_EXTERN bool			debugInset Q_ASSIGN( false );
Q_EXTERN bool			debugPortals Q_ASSIGN( false );

Q_EXTERN double				normalEpsilon Q_ASSIGN( 0.00001 );
Q_EXTERN double				distanceEpsilon Q_ASSIGN( 0.01 );


/* bsp */
Q_EXTERN int				numMapEntities Q_ASSIGN( 0 );

Q_EXTERN int				blockSize[ 3 ]					/* should be the same as in radiant */
#ifndef MAIN_C
							;
#else
							= { 1024, 1024, 1024 };
#endif

Q_EXTERN char				name[ 1024 ];
Q_EXTERN char				source[ 1024 ];
Q_EXTERN char				outbase[ 32 ];

Q_EXTERN int				sampleSize;						/* lightmap sample size in units */

Q_EXTERN int				mapEntityNum Q_ASSIGN( 0 );

Q_EXTERN int				entitySourceBrushes;

Q_EXTERN plane_t			mapplanes[ MAX_MAP_PLANES ];	/* mapplanes[ num ^ 1 ] will always be the mirror or mapplanes[ num ] */
Q_EXTERN int				nummapplanes;					/* nummapplanes will always be even */
Q_EXTERN int				numMapPatches;
Q_EXTERN vec3_t				mapMins, mapMaxs, mapSize;

Q_EXTERN int				defaultFogNum Q_ASSIGN( -1 );	/* ydnar: cleaner fog handling */
Q_EXTERN int				numMapFogs Q_ASSIGN( 0 );
Q_EXTERN fog_t				mapFogs[ MAX_MAP_FOGS ];

Q_EXTERN entity_t			*mapEnt;
Q_EXTERN brush_t			*buildBrush;
Q_EXTERN int				numActiveBrushes;
Q_EXTERN int				g_bBrushPrimit;

/* surface stuff */
Q_EXTERN mapDrawSurface_t	*mapDrawSurfs Q_ASSIGN( NULL );
Q_EXTERN int				numMapDrawSurfs;

Q_EXTERN int				numSurfacesByType[ NUM_SURFACE_TYPES ];
Q_EXTERN int				numClearedSurfaces;
Q_EXTERN int				numStripSurfaces;
Q_EXTERN int				numFanSurfaces;
Q_EXTERN int				numMergedSurfaces;
Q_EXTERN int				numMergedVerts;

Q_EXTERN int				numRedundantIndexes;

Q_EXTERN int				numSurfaceModels Q_ASSIGN( 0 );

Q_EXTERN byte				debugColors[ 12 ][ 3 ]
#ifndef MAIN_C
							;
#else
							=
							{
								{ 255, 0, 0 },
								{ 192, 128, 128 },
								{ 255, 255, 0 },
								{ 192, 192, 128 },
								{ 0, 255, 255 },
								{ 128, 192, 192 },
								{ 0, 0, 255 },
								{ 128, 128, 192 },
								{ 255, 0, 255 },
								{ 192, 128, 192 },
								{ 0, 255, 0 },
								{ 128, 192, 128 }
							};
#endif

Q_EXTERN bool			skyboxPresent Q_ASSIGN( false );
Q_EXTERN int				skyboxArea Q_ASSIGN( -1 );
Q_EXTERN matrix4x4				skyboxTransform;



/* -------------------------------------------------------------------------------

vis global variables

------------------------------------------------------------------------------- */

/* commandline arguments */
Q_EXTERN bool			fastvis;
Q_EXTERN bool			noPassageVis;
Q_EXTERN bool			passageVisOnly;
Q_EXTERN bool			mergevis;
Q_EXTERN bool			nosort;
Q_EXTERN bool			saveprt;
Q_EXTERN bool			hint;	/* ydnar */
Q_EXTERN char				inbase[ MAX_SHADERPATH ];

/* other bits */
Q_EXTERN int				totalvis;

Q_EXTERN float				farPlaneDist;	/* rr2do2, rf, mre, ydnar all contributed to this one... */

Q_EXTERN int				numportals;
Q_EXTERN int				portalclusters;

Q_EXTERN vportal_t			*portals;
Q_EXTERN leaf_t				*leafs;

Q_EXTERN vportal_t			*faces;
Q_EXTERN leaf_t				*faceleafs;

Q_EXTERN int				numfaces;

Q_EXTERN int				c_portaltest, c_portalpass, c_portalcheck;
Q_EXTERN int				c_portalskip, c_leafskip;
Q_EXTERN int				c_vistest, c_mighttest;
Q_EXTERN int				c_chains;

Q_EXTERN byte				*vismap, *vismap_p, *vismap_end;

Q_EXTERN int				testlevel;

Q_EXTERN byte				*uncompressed;

Q_EXTERN int				leafbytes, leaflongs;
Q_EXTERN int				portalbytes, portallongs;

Q_EXTERN vportal_t			*sorted_portals[ MAX_MAP_PORTALS * 2 ];



/* -------------------------------------------------------------------------------

light global variables

------------------------------------------------------------------------------- */

/* commandline arguments */
Q_EXTERN bool			wolfLight Q_ASSIGN( false );
Q_EXTERN bool			loMem Q_ASSIGN( false );
Q_EXTERN bool			noStyles Q_ASSIGN( false );

Q_EXTERN int				sampleSize Q_ASSIGN( DEFAULT_LIGHTMAP_SAMPLE_SIZE );
Q_EXTERN bool			noVertexLighting Q_ASSIGN( false );
Q_EXTERN bool			noGridLighting Q_ASSIGN( false );

Q_EXTERN bool			noTrace Q_ASSIGN( false );
Q_EXTERN bool			noSurfaces Q_ASSIGN( false );
Q_EXTERN bool			patchShadows Q_ASSIGN( false );
Q_EXTERN bool			cpmaHack Q_ASSIGN( false );

Q_EXTERN bool			deluxemap Q_ASSIGN( false );
Q_EXTERN bool			debugDeluxemap Q_ASSIGN( false );

Q_EXTERN bool			fast Q_ASSIGN( false );
Q_EXTERN bool			faster Q_ASSIGN( false );
Q_EXTERN bool			fastgrid Q_ASSIGN( false );
Q_EXTERN bool			fastbounce Q_ASSIGN( false );
Q_EXTERN bool			cheap Q_ASSIGN( false );
Q_EXTERN bool			cheapgrid Q_ASSIGN( false );
Q_EXTERN int				bounce Q_ASSIGN( 0 );
Q_EXTERN bool			bounceOnly Q_ASSIGN( false );
Q_EXTERN bool			bouncing Q_ASSIGN( false );
Q_EXTERN bool			bouncegrid Q_ASSIGN( false );
Q_EXTERN bool			normalmap Q_ASSIGN( false );
Q_EXTERN bool			trisoup Q_ASSIGN( false );
Q_EXTERN bool			shade Q_ASSIGN( false );
Q_EXTERN float				shadeAngleDegrees Q_ASSIGN( 0.0f );
Q_EXTERN int				superSample Q_ASSIGN( 0 );
Q_EXTERN int				lightSamples Q_ASSIGN( 1 );
Q_EXTERN bool			filter Q_ASSIGN( false );
Q_EXTERN bool			dark Q_ASSIGN( false );
Q_EXTERN bool			sunOnly Q_ASSIGN( false );
Q_EXTERN int				approximateTolerance Q_ASSIGN( 0 );
Q_EXTERN bool			noCollapse Q_ASSIGN( false );
Q_EXTERN bool			exportLightmaps Q_ASSIGN( false );
Q_EXTERN bool			externalLightmaps Q_ASSIGN( false );
Q_EXTERN int				lmCustomSize Q_ASSIGN( LIGHTMAP_WIDTH );

Q_EXTERN bool			dirty Q_ASSIGN( false );
Q_EXTERN bool			dirtDebug Q_ASSIGN( false );
Q_EXTERN int				dirtMode Q_ASSIGN( 0 );
Q_EXTERN float				dirtDepth Q_ASSIGN( 128.0f );
Q_EXTERN float				dirtScale Q_ASSIGN( 1.0f );
Q_EXTERN float				dirtGain Q_ASSIGN( 1.0f );

Q_EXTERN bool			dump Q_ASSIGN( false );
Q_EXTERN bool			debug Q_ASSIGN( false );
Q_EXTERN bool			debugUnused Q_ASSIGN( false );
Q_EXTERN bool			debugAxis Q_ASSIGN( false );
Q_EXTERN bool			debugCluster Q_ASSIGN( false );
Q_EXTERN bool			debugOrigin Q_ASSIGN( false );
Q_EXTERN bool			lightmapBorder Q_ASSIGN( false );

/* longest distance across the map */
Q_EXTERN float				maxMapDistance Q_ASSIGN( 0 );

/* for run time tweaking of light sources */
Q_EXTERN float				pointScale Q_ASSIGN( 7500.0f );
Q_EXTERN float				areaScale Q_ASSIGN( 0.25f );
Q_EXTERN float				skyScale Q_ASSIGN( 1.0f );
Q_EXTERN float				bounceScale Q_ASSIGN( 0.25f );

/* ydnar: lightmap gamma/compensation */
Q_EXTERN float				lightmapGamma Q_ASSIGN( 1.0f );
Q_EXTERN float				lightmapCompensate Q_ASSIGN( 1.0f );

/* ydnar: for runtime tweaking of falloff tolerance */
Q_EXTERN float				falloffTolerance Q_ASSIGN( 1.0f );
Q_EXTERN bool			exactPointToPolygon Q_ASSIGN( true );
Q_EXTERN float				formFactorValueScale Q_ASSIGN( 3.0f );
Q_EXTERN float				linearScale Q_ASSIGN( 1.0f / 8000.0f );

Q_EXTERN light_t			*lights;
Q_EXTERN int				numPointLights;
Q_EXTERN int				numSpotLights;
Q_EXTERN int				numSunLights;
Q_EXTERN int				numAreaLights;

/* ydnar: for luxel placement */
Q_EXTERN int				numSurfaceClusters, maxSurfaceClusters;
Q_EXTERN int				*surfaceClusters;

/* ydnar: for radiosity */
Q_EXTERN int				numDiffuseLights;
Q_EXTERN int				numBrushDiffuseLights;
Q_EXTERN int				numTriangleDiffuseLights;
Q_EXTERN int				numPatchDiffuseLights;

/* ydnar: general purpose extra copy of drawvert list */
Q_EXTERN bspDrawVert_t		*yDrawVerts;

/* ydnar: for tracing statistics */
Q_EXTERN int				minSurfacesTested;
Q_EXTERN int				maxSurfacesTested;
Q_EXTERN int				totalSurfacesTested;
Q_EXTERN int				totalTraces;

Q_EXTERN file_t				*dumpFile;

Q_EXTERN int				c_visible, c_occluded;
Q_EXTERN int				c_subsampled;	/* ydnar */

Q_EXTERN int				defaultLightSubdivide Q_ASSIGN( 999 );

Q_EXTERN vec3_t				ambientColor;
Q_EXTERN vec3_t				minLight, minVertexLight, minGridLight;

Q_EXTERN int				*entitySurface;
Q_EXTERN vec3_t				*surfaceOrigin;

Q_EXTERN vec3_t				sunDirection;
Q_EXTERN vec3_t				sunLight;

/* tracing */
Q_EXTERN int				c_totalTrace;
Q_EXTERN int				c_cullTrace, c_testTrace;
Q_EXTERN int				c_testFacets;

/* ydnar: light optimization */
Q_EXTERN float				subdivideThreshold Q_ASSIGN( DEFAULT_SUBDIVIDE_THRESHOLD );

Q_EXTERN int				numOpaqueBrushes, maxOpaqueBrush;
Q_EXTERN byte				*opaqueBrushes;

Q_EXTERN int				numLights;
Q_EXTERN int				numCulledLights;

Q_EXTERN int				gridBoundsCulled;
Q_EXTERN int				gridEnvelopeCulled;

Q_EXTERN int				lightsBoundsCulled;
Q_EXTERN int				lightsEnvelopeCulled;
Q_EXTERN int				lightsPlaneCulled;
Q_EXTERN int				lightsClusterCulled;

/* ydnar: radiosity */
Q_EXTERN float				diffuseSubdivide Q_ASSIGN( 256.0f );
Q_EXTERN float				minDiffuseSubdivide Q_ASSIGN( 64.0f );
Q_EXTERN int				numDiffuseSurfaces Q_ASSIGN( 0 );

/* ydnar: list of surface information necessary for lightmap calculation */
Q_EXTERN surfaceInfo_t		*surfaceInfos Q_ASSIGN( NULL );

/* ydnar: sorted list of surfaces */
Q_EXTERN int				*sortSurfaces Q_ASSIGN( NULL );

/* clumps of surfaces that share a raw lightmap */
Q_EXTERN int				numLightSurfaces Q_ASSIGN( 0 );
Q_EXTERN int				*lightSurfaces Q_ASSIGN( NULL );

/* raw lightmaps */
Q_EXTERN int				numRawSuperLuxels Q_ASSIGN( 0 );
Q_EXTERN int				numRawLightmaps Q_ASSIGN( 0 );
Q_EXTERN rawLightmap_t		*rawLightmaps Q_ASSIGN( NULL );
Q_EXTERN int				*sortLightmaps Q_ASSIGN( NULL );

/* vertex luxels */
Q_EXTERN float				*vertexLuxels[ MAX_LIGHTMAPS ];
Q_EXTERN float				*radVertexLuxels[ MAX_LIGHTMAPS ];

/* bsp lightmaps */
Q_EXTERN int				numLightmapShaders Q_ASSIGN( 0 );
Q_EXTERN int				numSolidLightmaps Q_ASSIGN( 0 );
Q_EXTERN int				numOutLightmaps Q_ASSIGN( 0 );
Q_EXTERN int				numBSPLightmaps Q_ASSIGN( 0 );
Q_EXTERN int				numExtLightmaps Q_ASSIGN( 0 );
Q_EXTERN outLightmap_t		*outLightmaps Q_ASSIGN( NULL );

/* grid points */
Q_EXTERN int				numRawGridPoints Q_ASSIGN( 0 );
Q_EXTERN rawGridPoint_t		*rawGridPoints Q_ASSIGN( NULL );

Q_EXTERN int				numSurfsVertexLit Q_ASSIGN( 0 );
Q_EXTERN int				numSurfsVertexForced Q_ASSIGN( 0 );
Q_EXTERN int				numSurfsVertexApproximated Q_ASSIGN( 0 );
Q_EXTERN int				numSurfsLightmapped Q_ASSIGN( 0 );
Q_EXTERN int				numPlanarsLightmapped Q_ASSIGN( 0 );
Q_EXTERN int				numNonPlanarsLightmapped Q_ASSIGN( 0 );
Q_EXTERN int				numPatchesLightmapped Q_ASSIGN( 0 );
Q_EXTERN int				numPlanarPatchesLightmapped Q_ASSIGN( 0 );

Q_EXTERN int				numLuxels Q_ASSIGN( 0 );
Q_EXTERN int				numLuxelsMapped Q_ASSIGN( 0 );
Q_EXTERN int				numLuxelsOccluded Q_ASSIGN( 0 );
Q_EXTERN int				numLuxelsIlluminated Q_ASSIGN( 0 );
Q_EXTERN int				numVertsIlluminated Q_ASSIGN( 0 );

/* lightgrid */
Q_EXTERN vec3_t				gridMins;
Q_EXTERN int				gridBounds[ 3 ];
Q_EXTERN vec3_t				gridSize
#ifndef MAIN_C
							;
#else
							= { 64, 64, 128 };
#endif



/* -------------------------------------------------------------------------------

abstracted bsp globals

------------------------------------------------------------------------------- */

Q_EXTERN int				numEntities Q_ASSIGN( 0 );
Q_EXTERN int				numBSPEntities Q_ASSIGN( 0 );
Q_EXTERN entity_t				entities[MAX_MAP_ENTITIES];
Q_EXTERN script_t				*mapfile;

Q_EXTERN int				numBSPModels Q_ASSIGN( 0 );
Q_EXTERN bspModel_t			bspModels[ MAX_MAP_MODELS ];

Q_EXTERN int				numBSPShaders Q_ASSIGN( 0 );
Q_EXTERN bspShader_t		bspShaders[ MAX_MAP_MODELS ];

Q_EXTERN int				bspEntDataSize Q_ASSIGN( 0 );
Q_EXTERN char				bspEntData[ MAX_MAP_ENTSTRING ];

Q_EXTERN int				numBSPLeafs Q_ASSIGN( 0 );
Q_EXTERN bspLeaf_t			bspLeafs[ MAX_MAP_LEAFS ];

Q_EXTERN int				numBSPPlanes Q_ASSIGN( 0 );
Q_EXTERN bspPlane_t			bspPlanes[ MAX_MAP_PLANES ];

Q_EXTERN int				numBSPNodes Q_ASSIGN( 0 );
Q_EXTERN bspNode_t			bspNodes[ MAX_MAP_NODES ];

Q_EXTERN int				numBSPLeafSurfaces Q_ASSIGN( 0 );
Q_EXTERN int				bspLeafSurfaces[ MAX_MAP_LEAFFACES ];

Q_EXTERN int				numBSPLeafBrushes Q_ASSIGN( 0 );
Q_EXTERN int				bspLeafBrushes[ MAX_MAP_LEAFBRUSHES ];

Q_EXTERN int				numBSPBrushes Q_ASSIGN( 0 );
Q_EXTERN bspBrush_t			bspBrushes[ MAX_MAP_BRUSHES ];

Q_EXTERN int				numBSPBrushSides Q_ASSIGN( 0 );
Q_EXTERN bspBrushSide_t		bspBrushSides[ MAX_MAP_BRUSHSIDES ];

Q_EXTERN int				numBSPLightBytes Q_ASSIGN( 0 );
Q_EXTERN byte				*bspLightBytes Q_ASSIGN( NULL );

//%	Q_EXTERN int				numBSPGridPoints Q_ASSIGN( 0 );
//%	Q_EXTERN byte				*bspGridPoints Q_ASSIGN( NULL );

Q_EXTERN int				numBSPGridPoints Q_ASSIGN( 0 );
Q_EXTERN bspGridPoint_t		*bspGridPoints Q_ASSIGN( NULL );

Q_EXTERN int				numBSPVisBytes Q_ASSIGN( 0 );
Q_EXTERN byte				bspVisBytes[ MAX_MAP_VISIBILITY ];

Q_EXTERN int				numBSPDrawVerts Q_ASSIGN( 0 );
Q_EXTERN bspDrawVert_t		*bspDrawVerts Q_ASSIGN( NULL );

Q_EXTERN int				numBSPDrawIndexes Q_ASSIGN( 0 );
Q_EXTERN int				bspDrawIndexes[ MAX_MAP_DRAW_INDEXES ];

Q_EXTERN int				numBSPDrawSurfaces Q_ASSIGN( 0 );
Q_EXTERN bspDrawSurface_t	*bspDrawSurfaces Q_ASSIGN( NULL );

Q_EXTERN int				numBSPFogs Q_ASSIGN( 0 );
Q_EXTERN bspFog_t			bspFogs[ MAX_MAP_FOGS ];

Q_EXTERN int				numBSPAds Q_ASSIGN( 0 );
Q_EXTERN bspAdvertisement_t	bspAds[ MAX_MAP_ADVERTISEMENTS ];

// parselib extensions (wroted especially for bsplib)
void		Com_CheckToken( script_t *script, const char *match );
void		Com_Parse1DMatrix( script_t *script, int x, float *m );
void		Com_Parse2DMatrix( script_t *script, int y, int x, float *m );

#endif