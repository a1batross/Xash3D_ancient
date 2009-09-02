/*
Copyright (C) 1997-2001 Id Software, Inc.
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
#ifndef __R_MODEL_H__
#define __R_MODEL_H__

/*

d*_t structures are on-disk representations
m*_t structures are in-memory

*/

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/
//
// in memory representation
//
typedef struct
{
	vec3_t		mins, maxs;
	float		radius;
	int		firstface, numfaces;
} mmodel_t;

typedef struct
{
	ref_shader_t	*shader;
	cplane_t		*visibleplane;

	int		numplanes;
	cplane_t		*planes;
} mfog_t;

typedef struct
{
	int		visframe;			// should be drawn when node is crossed
	int		facetype;
	int		flags;

	ref_shader_t	*shader;
	mesh_t		*mesh;
	mfog_t		*fog;
	cplane_t		*plane;

	union
	{
		float	origin[3];
		float	mins[3];
	};
	union
	{
		float	maxs[3];
		float	color[3];
	};

	int		superLightStyle;
	int		fragmentframe;		// for multi-check avoidance
} msurface_t;

typedef struct mnode_s
{
	// common with leaf
	cplane_t		*plane;
	int		pvsframe;

	float		mins[3];
	float		maxs[3];		// for bounding box culling

	struct mnode_s	*parent;

	// node specific
	struct mnode_s	*children[2];
} mnode_t;

typedef struct mleaf_s
{
	// common with node
	cplane_t		*plane;
	int		pvsframe;

	float		mins[3];
	float		maxs[3];		// for bounding box culling

	struct mnode_s	*parent;

	// leaf specific
	int		visframe;
	int		cluster, area;

	msurface_t	**firstVisSurface;
	msurface_t	**firstFragmentSurface;
} mleaf_t;

typedef struct
{
	byte		ambient[LM_STYLES][3];
	byte		diffuse[LM_STYLES][3];
	byte		styles[LM_STYLES];
	byte		direction[2];
} mgridlight_t;

typedef struct
{
	int		texNum;
	float		texMatrix[2][2];
} mlightmapRect_t;

typedef struct
{
	dvis_t		*vis;

	int		numsubmodels;
	mmodel_t		*submodels;

	int		nummodelsurfaces;
	msurface_t	*firstmodelsurface;

	int		numplanes;
	cplane_t		*planes;

	int		numleafs;			// number of visible leafs, not counting 0
	mleaf_t		*leafs;
	mleaf_t		**visleafs;

	int		numnodes;
	mnode_t		*nodes;

	int		numsurfaces;
	msurface_t	*surfaces;

	int		numlightgridelems;
	mgridlight_t	*lightgrid;

	int		numlightarrayelems;
	mgridlight_t	**lightarray;

	int		numfogs;
	mfog_t		*fogs;
	mfog_t		*globalfog;

	vec3_t		gridSize;
	vec3_t		gridMins;
	int		gridBounds[4];
} mbrushmodel_t;

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/
#define MAX_SKINS		32

typedef struct
{
	int		firstpose;
	int		numposes;
	float		interval;
	daliastrivertx_t	bboxmin;
	daliastrivertx_t	bboxmax;
	int		frame;
	char		name[16];
} maliasframedesc_t;

typedef struct
{
	daliastrivertx_t	bboxmin;
	daliastrivertx_t	bboxmax;
	int		frame;
} maliasgroupdesc_t;

typedef struct
{
	int		numframes;
	int		intervals;
	maliasgroupdesc_t	frames[1];	// variable sized
} maliasgroup_t;

typedef struct mtriangle_s
{
	int		facesfront;
	int		vertindex[3];
} mtriangle_t;

typedef struct
{
	int		ident;
	int		version;
	vec3_t		scale;
	vec3_t		scale_origin;
	float		boundingradius;
	vec3_t		eyeposition;
	int		numskins;
	int		skinwidth;
	int		skinheight;
	int		numverts;
	int		numtris;
	int		numframes;
	synctype_t	synctype;
	int		flags;
	float		size;

	int		numposes;
	int		poseverts;
	int		posedata;		// numposes * poseverts trivert_t
	int		commands;		// gl command list with embedded s/t
	ref_shader_t	*skins[MAX_SKINS][4];
	maliasframedesc_t	frames[1];	// variable sized
} maliashdr_t;

extern	maliashdr_t	*pheader;
extern	daliastexcoord_t	stverts[MAXALIASVERTS];
extern	mtriangle_t	triangles[MAXALIASTRIS];
extern	daliastrivertx_t	*poseverts[MAXALIASFRAMES];

//
// in memory representation
//
typedef struct
{
	short			point[3];
	byte			latlong[2];				// use bytes to keep 8-byte alignment
} maliasvertex_t;

typedef struct
{
	vec3_t			mins, maxs;
	vec3_t			scale;
	vec3_t			translate;
	float			radius;
} maliasframe_t;

typedef struct
{
	char			name[MD3_MAX_PATH];
	quat_t			quat;
	vec3_t			origin;
} maliastag_t;

typedef struct
{
	ref_shader_t		*shader;
} maliasskin_t;

typedef struct
{
	char			name[MD3_MAX_PATH];

	int				numverts;
	maliasvertex_t *vertexes;
	vec2_t			*stArray;

	vec4_t			*xyzArray;
	vec4_t			*normalsArray;
	vec4_t			*sVectorsArray;

	int				numtris;
	elem_t			*elems;

	int				numskins;
	maliasskin_t	*skins;
} maliasmesh_t;

typedef struct
{
	int		numframes;
	maliasframe_t	*frames;

	int	 	numtags;
	maliastag_t	*tags;

	int	 	nummeshes;
	maliasmesh_t	*meshes;

	int numskins;
	maliasskin_t	*skins;
} maliasmodel_t;

/*
==============================================================================

STUDIO MODELS

==============================================================================
*/
typedef struct mstudiomodel_s
{
	dstudiohdr_t	*phdr;
          dstudiohdr_t	*thdr;

	void		*submodels;
	int		numsubmodels;
	vec3_t		*m_pSVectors;	// UNDONE: calc SVectors on loading, simple transform on rendering

} mstudiomodel_t;

/*
==============================================================================

SPRITE MODELS

==============================================================================
*/
//
// in memory representation
//
typedef struct mspriteframe_s
{
	int		width;
	int		height;
	float		up, down, left, right;
	float		radius;
	shader_t		shader;
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

//===================================================================

//
// Whole model
//

#define MOD_MAX_LODS	4

typedef struct ref_model_s
{
	char		*name;
	modtype_t		type;

	// volume occupied by the model graphics
	vec3_t		mins, maxs;
	float		radius;
	int		flags;		// effect flags

	// memory representation pointer
	byte		*mempool;
	void		*extradata;
	
	int		touchFrame;
	int		numlods;
	struct ref_model_s	*lods[MOD_MAX_LODS];

	// shader pointers for refresh registration_sequence
	int		numshaders;
	ref_shader_t	**shaders;
} ref_model_t;

//============================================================================

void		R_InitModels( void );
void		R_ShutdownModels( void );

void		Mod_ClearAll( void );
ref_model_t	*Mod_ForName( const char *name, bool crash );
mleaf_t		*Mod_PointInLeaf( float *p, ref_model_t *model );
byte		*Mod_ClusterPVS( int cluster, ref_model_t *model );
uint		Mod_Handle( ref_model_t *mod );
ref_model_t	*Mod_ForHandle( unsigned int elem );
ref_model_t	*R_RegisterModel( const char *name );
void		R_BeginRegistration( const char *model, const dvis_t *visData );
void		R_EndRegistration( const char *skyname );

#define		Mod_Malloc( mod, size ) Mem_Alloc(( mod )->mempool, size )
#define		Mod_Realloc( mod, data, size ) Mem_Realloc(( mod )->mempool, data, size )
#define		Mod_Free( data ) Mem_Free( data )
void		Mod_StripLODSuffix( char *name );
void		Mod_Modellist_f( void );

#endif /*__R_MODEL_H__*/
