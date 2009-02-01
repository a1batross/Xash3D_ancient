//=======================================================================
//			Copyright XashXT Group 2007 ©
//	studiomdl.h - generates a studio .mdl file from a .qc script
//=======================================================================
#ifndef STUDIOMDL_H
#define STUDIOMDL_H

#include "xtools.h"
#include "utils.h"
#include "mathlib.h"

#define FILEBUFFER		(1024 * 1024 * 16)	// 16 megs filebuffer
#define FLOAT_START		99999.0
#define FLOAT_END		-99999.0
#define MAGIC		123322
#define Kalloc(size)	Mem_Alloc(studiopool, size)		
#define Realloc(ptr, size)	Mem_Realloc(studiopool, ptr, size)

typedef struct
{
	int		vertindex;
	int		normindex;	// index into normal array
	int		s,t;
	float		u,v;
} s_trianglevert_t;

typedef struct 
{
	int		bone;		// bone transformation index
	vec3_t		org;		// original position
} s_vertex_t;

typedef struct 
{
	int		skinref;
	int		bone;		// bone transformation index
	vec3_t		org;		// original position
} s_normal_t;

typedef struct 
{
	vec3_t		worldorg;
	matrix4x4		m;
	matrix4x4		im;
	float		length;
} s_bonefixup_t;

typedef struct
{
	vec3_t		verts[3];
} triangle_t;

typedef struct 
{
	char		name[32];		// bone name for symbolic links
	int		parent;		// parent bone
	int		bonecontroller;	// -1 == 0
	int		flags;		// X, Y, Z, XR, YR, ZR
	vec3_t		pos;		// default pos
	vec3_t		posscale;		// pos values scale
	vec3_t		rot;		// default pos
	vec3_t		rotscale;		// rotation values scale
	int		group;		// hitgroup
	vec3_t		bmin, bmax;	// bounding box
} s_bonetable_t;

typedef struct 
{
	char		from[32];
	char		to[32];
} s_renamebone_t;

typedef struct
{
	char		name[32];		// bone name
	int		bone;
	int		group;		// hitgroup
	int		model;
	vec3_t		bmin, bmax;	// bounding box
} s_bbox_t;

typedef struct
{
	int		models;
	int		group;
	char		name[32];		// bone name
} s_hitgroup_t;

typedef struct 
{
	char		name[32];
	int		bone;
	int		type;
	int		index;
	float		start;
	float		end;
} s_bonecontroller_t;

typedef struct 
{
	char		name[32];
	char		bonename[32];
	int		index;
	int		bone;
	int		type;
	vec3_t		org;
} s_attachment_t;

typedef struct 
{
	char		name[64];
	int		parent;
	int		mirrored;
} s_node_t;

typedef struct 
{
	char		name[64];
	int		startframe;
	int		endframe;
	int		flags;
	int		numbones;
	s_node_t		node[MAXSTUDIOSRCBONES];
	int		bonemap[MAXSTUDIOSRCBONES];
	int		boneimap[MAXSTUDIOSRCBONES];
	vec3_t		*pos[MAXSTUDIOSRCBONES];
	vec3_t		*rot[MAXSTUDIOSRCBONES];
	int		numanim[MAXSTUDIOSRCBONES][6];
	dstudioanimvalue_t	*anim[MAXSTUDIOSRCBONES][6];
} s_animation_t;

typedef struct 
{
	int		event;
	int		frame;
	char		options[64];
} s_event_t;

typedef struct 
{
	int		index;
	vec3_t		org;
	int		start;
	int		end;
} s_pivot_t;

typedef struct 
{
	int		motiontype;
	vec3_t		linearmovement;

	char		name[64];
	int		flags;
	float		fps;
	int		numframes;

	int		activity;
	int		actweight;

	int		frameoffset;	// used to adjust frame numbers

	int		numevents;
	s_event_t		event[MAXSTUDIOEVENTS];

	int		numpivots;
	s_pivot_t		pivot[MAXSTUDIOPIVOTS];

	int		numblends;
	s_animation_t	*panim[MAXSTUDIOGROUPS];
	float		blendtype[2];
	float		blendstart[2];
	float		blendend[2];

	vec3_t		automovepos[MAXSTUDIOANIMATIONS];
	vec3_t		automoveangle[MAXSTUDIOANIMATIONS];

	int		seqgroup;
	int		animindex;

	vec3_t 		bmin;
	vec3_t		bmax;
	int		entrynode;
	int		exitnode;
	int		nodeflags;
} s_sequence_t;

typedef struct
{
	char		label[32];
	char		name[64];
} s_sequencegroup_t;

typedef struct 
{
	char		name[64];
	int		flags;
	int		srcwidth;
	int		srcheight;
	byte		*ppicture;
	rgb_t 		*ppal;
	float		max_s;
	float		min_s;
	float		max_t;
	float		min_t;
	int		skintop;
	int		skinleft;
	int		skinwidth;
	int		skinheight;
	float		fskintop;
	float		fskinleft;
	float		fskinwidth;
	float		fskinheight;
	int		size;
	void		*pdata;
	int		parent;
} s_texture_t;

typedef struct 
{
	int		numtris;
	int		alloctris;
	s_trianglevert_t	(*triangle)[3];

	int		skinref;
	int		numnorms;
} s_mesh_t;

typedef struct 
{
	vec3_t		pos;
	vec3_t		rot;
} s_bone_t;

typedef struct s_model_s 
{
	char		name[64];

	int		numbones;
	s_node_t		node[MAXSTUDIOSRCBONES];
	s_bone_t		skeleton[MAXSTUDIOSRCBONES];
	int		boneref[MAXSTUDIOSRCBONES];	// is local bone (or child) referenced with a vertex
	int		bonemap[MAXSTUDIOSRCBONES];	// local bone to world bone mapping
	int		boneimap[MAXSTUDIOSRCBONES];	// world bone to local bone mapping

	vec3_t		boundingbox[MAXSTUDIOSRCBONES][2];

	s_mesh_t		*trimesh[MAXSTUDIOTRIANGLES];
	int		trimap[MAXSTUDIOTRIANGLES];

	int		numverts;
	s_vertex_t	vert[MAXSTUDIOVERTS];

	int		numnorms;
	s_normal_t	normal[MAXSTUDIOVERTS];

	int		nummesh;
	s_mesh_t		*pmesh[MAXSTUDIOMESHES];

	float		boundingradius;

	int		numframes;
	float		interval;
	struct		s_model_s *next;
} s_model_t;

typedef struct
{
	char		name[32];
	int		nummodels;
	int		base;
	s_model_t		*pmodel[MAXSTUDIOMODELS];
} s_bodypart_t;

typedef struct
{
	vec3_t		n;		// normal
	vec3_t		p;		// point
	vec3_t		c;		// color
	float		u;		// u
	float		v;		// v
} aliaspoint_t;

typedef struct
{
	aliaspoint_t	pt[3];
} tf_triangle;

extern int numseq;
extern int numani;
extern int numbones;
extern int numxnodes;
extern int numhitboxes;
extern int nummirrored;
extern int numseqgroups;
extern int numhitgroups;
extern int numattachments;
extern int numrenamedbones;
extern int numcommandnodes;
extern int numtextures;
extern int numbonecontrollers;
extern int numcommands;
extern int stripcount;
extern int clip_texcoords;
extern int xnode[100][100];
extern byte *studiopool;

extern s_bbox_t hitbox[MAXSTUDIOSRCBONES];
extern s_texture_t *texture[MAXSTUDIOSKINS];
extern string mirrored[MAXSTUDIOSRCBONES];
extern s_hitgroup_t hitgroup[MAXSTUDIOSRCBONES];
extern s_sequence_t *sequence[MAXSTUDIOSEQUENCES];
extern s_bonefixup_t bonefixup[MAXSTUDIOSRCBONES];
extern s_bonetable_t bonetable[MAXSTUDIOSRCBONES];
extern s_attachment_t attachment[MAXSTUDIOSRCBONES];
extern s_renamebone_t renamedbone[MAXSTUDIOSRCBONES];
extern s_animation_t *panimation[MAXSTUDIOSEQUENCES*MAXSTUDIOBLENDS];
extern s_bonecontroller_t bonecontroller[MAXSTUDIOSRCBONES];

extern float normal_blend;
extern float scale_up;
extern float gamma;
extern vec3_t adjust;

extern byte *studiopool;
extern s_mesh_t *pmesh;

void LoadTriangleList (char *filename, triangle_t **pptri, int *numtriangles);
int BuildTris (s_trianglevert_t (*x)[3], s_mesh_t *y, byte **ppdata );
void ResetTextureCoordRanges( s_mesh_t *pmesh, s_texture_t *ptexture );
void TextureCoordRanges( s_mesh_t *pmesh, s_texture_t *ptexture );
s_trianglevert_t *lookup_triangle( s_mesh_t *pmesh, int index );
s_mesh_t *lookup_mesh( s_model_t *pmodel, char *texturename );
int lookup_normal( s_model_t *pmodel, s_normal_t *pnormal );
int lookup_vertex( s_model_t *pmodel, s_vertex_t *pv );
void ResizeTexture( s_texture_t *ptexture );
int lookup_texture( char *texturename );
int lookupActivity( char *szActivity );
void clip_rotations( vec3_t rot );
void scale_vertex( float *org );
void adjust_vertex( float *org );
int lookupControl( char *string );
void OptimizeAnimations(void);
void MakeTransitions( void );
int findNode( char *name );
void ExtractMotion( void );

#endif//STUDIOMDL_H