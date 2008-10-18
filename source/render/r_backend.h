//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        r_backend.h - rendering backend
//=======================================================================

#ifndef R_BACKEND_H
#define R_BACKEND_H

#define MAX_VERTEXES	4096
#define MAX_ELEMENTS	MAX_VERTEXES * 6	// quad
#define MAX_TRIANGLES	MAX_ELEMENTS / 3
#define MAX_NEIGHBORS	MAX_TRIANGLES *3

typedef struct
{
	vec3_t		xyz;
	vec3_t		normal;
	vec3_t		tangents[2];
	vec2_t		st;
	vec3_t		color;
	float		alpha;
} vertexArray_t;

typedef struct ref_backend_s
{
	// src arrays
	vec4_t		*inColors[LM_STYLES];	// RGBA[4]
	vec2_t		*inTexCoords;
	elem_t		*inIndices;
	vec2_t		*inLMCoords;
	vec3_t		*inNormals;
	vec3_t		*inSVectors;
	vec3_t		*inTVectors;
	vec3_t		*inVertices;

	// dst arrays
	elem_t		indexArray[MAX_ELEMENTS];			// vertsArray[num]
	vec3_t		vertsArray[MAX_VERTEXES];			// XYZ point
	vec3_t		normalArray[MAX_VERTEXES];			// R point
	vec3_t		tangentArray[MAX_VERTEXES];			// S point
	vec3_t		binormalArray[MAX_VERTEXES];			// T point
	vec3_t		texCoordArray[MAX_TEXTURE_UNITS][MAX_VERTEXES];	// STR (tangent, binormal, normal)
	vec2_t		lmsCoordArray[LM_STYLES][MAX_VERTEXES];		// ST for all lightmaps
	vec4_t		ColorArray[MAX_VERTEXES];			// sum of all lightstyles
} ref_backend_t;

#endif//R_BACKEND_H