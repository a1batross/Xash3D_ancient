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
	rgba_t		color;
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
	float		s;
	float		t;
	int		flags;	// alternative texcoords, etc
	vec3_t		*verts;	// pointer to globals vertices array
	vec2_t		*chrome;	// pointer to global chrome coords array
	short		*tricmds;	// triangle commands
	int		numVerts;	// to avoid overflow
	int		numTris;
} mstudiomesh_t;

#endif//R_MODEL_H