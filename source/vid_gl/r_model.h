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
#ifndef R_MODEL_H
#define R_MODEL_H

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
	int		contents;

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
	int		contents;

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
typedef struct ref_model_s
{
	char		*name;
	modtype_t		type;

	int		touchFrame;	// registration sequence

	// volume occupied by the model graphics
	vec3_t		mins, maxs;
	float		radius;
	int		flags;		// effect flags

	// memory representation pointer
	byte		*mempool;
	void		*extradata;

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
void		Mod_Modellist_f( void );

#endif // R_MODEL_H