//=======================================================================
//			Copyright XashXT Group 2008 ©
//		         r_model.h - renderer model types
//=======================================================================

#ifndef R_MODEL_H
#define R_MODEL_H

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
	vec3_t		point;
	vec2_t		st;
	vec4_t		color;
} mstudiopoint_t;

typedef struct mstudiosurface_s
{
	struct mstudiosurface_s	*next;

	int		numIndices;
	int		numVertices;

	uint		*indices;
	mstudiopoint_t	*points;
} mstudiosurface_t;

typedef struct mstudiomesh_s
{
	mstudiosurface_t	*surfaces;
	int		numSurfaces;
	ref_shader_t	*shader;
} mstudiomesh_t;

#endif//R_MODEL_H