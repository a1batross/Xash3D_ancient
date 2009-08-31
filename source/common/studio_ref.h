//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       studio_ref.h - studio model reference
//=======================================================================
#ifndef STUDIO_REF_H
#define STUDIO_REF_H

/*
==============================================================================

STUDIO MODELS

Studio models are position independent, so the cache manager can move them.
==============================================================================
*/

// header
#define STUDIO_VERSION	10
#define IDSTUDIOHEADER	(('T'<<24)+('S'<<16)+('D'<<8)+'I') // little-endian "IDST"
#define IDSEQGRPHEADER	(('Q'<<24)+('S'<<16)+('D'<<8)+'I') // little-endian "IDSQ"

// studio limits
#define MAXSTUDIOTRIANGLES		32768	// max triangles per model
#define MAXSTUDIOVERTS		4096	// max vertices per submodel
#define MAXSTUDIOSEQUENCES		256	// total animation sequences
#define MAXSTUDIOSKINS		128	// total textures
#define MAXSTUDIOSRCBONES		512	// bones allowed at source movement
#define MAXSTUDIOBONES		128	// total bones actually used
#define MAXSTUDIOMODELS		32	// sub-models per model
#define MAXSTUDIOBODYPARTS		32	// body parts per submodel
#define MAXSTUDIOGROUPS		16	// sequence groups (e.g. barney01.mdl, barney02.mdl, e.t.c)
#define MAXSTUDIOANIMATIONS		512	// max frames per sequence
#define MAXSTUDIOMESHES		256	// max textures per model
#define MAXSTUDIOEVENTS		1024	// events per model
#define MAXSTUDIOPIVOTS		256	// pivot points
#define MAXSTUDIOBLENDS		16	// max anim blends
#define MAXSTUDIOCONTROLLERS		16	// max controllers per model
#define MAXSTUDIOATTACHMENTS		16	// max attachments per model

// model global flags
#define STUDIO_STATIC		0x0001	// model without anims
#define STUDIO_RAGDOLL		0x0002	// ragdoll animation pose

// lighting & rendermode options
#define STUDIO_NF_FLATSHADE		0x0001
#define STUDIO_NF_CHROME		0x0002
#define STUDIO_NF_FULLBRIGHT		0x0004
#define STUDIO_NF_COLORMAP		0x0008	// can changed by colormap command
#define STUDIO_NF_BLENDED		0x0010	// rendering as semiblended
#define STUDIO_NF_ADDITIVE		0x0020	// rendering with additive mode
#define STUDIO_NF_TRANSPARENT		0x0040	// use texture with alpha channel
#define STUDIO_NF_NORMALMAP		0x0080	// indexed normalmap
#define STUDIO_NF_BUMPMAP		0x0100	// heightmap that can be used for parallax or normalmap
#define STUDIO_NF_GLOSSMAP		0x0200	// glossmap
#define STUDIO_NF_DECALMAP		0x0400	// decalmap (currently unused)
#define STUDIO_NF_GLOWMAP		0x0800	// optional luma_texture
#define STUDIO_NF_QUAKESKIN		0x8000	// special hack for determine alias skins

// motion flags
#define STUDIO_X			0x0001
#define STUDIO_Y			0x0002	
#define STUDIO_Z			0x0004
#define STUDIO_XR			0x0008
#define STUDIO_YR			0x0010
#define STUDIO_ZR			0x0020
#define STUDIO_LX			0x0040
#define STUDIO_LY			0x0080
#define STUDIO_LZ			0x0100
#define STUDIO_AX			0x0200
#define STUDIO_AY			0x0400
#define STUDIO_AZ			0x0800
#define STUDIO_AXR			0x1000
#define STUDIO_AYR			0x2000
#define STUDIO_AZR			0x4000
#define STUDIO_TYPES		0x7FFF
#define STUDIO_RLOOP		0x8000	// controller that wraps shortest distance

// bonecontroller types
#define STUDIO_MOUTH		4

// sequence flags
#define STUDIO_LOOPING		0x0001

// render flags
#define STUDIO_RENDER		0x0001
#define STUDIO_EVENTS		0x0002
#define STUDIO_MIRROR		0x0004	// a local player in mirror 

// bone flags
#define STUDIO_HAS_NORMALS		0x0001
#define STUDIO_HAS_VERTICES		0x0002
#define STUDIO_HAS_BBOX		0x0004
#define STUDIO_HAS_CHROME		0x0008	// if any of the textures have chrome on them

typedef struct
{
	int		ident;
	int		version;

	char		name[64];
	int		length;

	vec3_t		eyeposition;	// ideal eye position
	vec3_t		min;		// ideal movement hull size
	vec3_t		max;			

	vec3_t		bbmin;		// clipping bounding box
	vec3_t		bbmax;		

	int		flags;

	int		numbones;		// bones
	int		boneindex;

	int		numbonecontrollers;	// bone controllers
	int		bonecontrollerindex;

	int		numhitboxes;	// complex bounding boxes
	int		hitboxindex;			
	
	int		numseq;		// animation sequences
	int		seqindex;

	int		numseqgroups;	// demand loaded sequences
	int		seqgroupindex;

	int		numtextures;	// raw textures
	int		textureindex;
	int		texturedataindex;

	int		numskinref;	// replaceable textures
	int		numskinfamilies;
	int		skinindex;

	int		numbodyparts;		
	int		bodypartindex;

	int		numattachments;	// queryable attachable points
	int		attachmentindex;

	int		soundtable;
	int		soundindex;
	int		soundgroups;
	int		soundgroupindex;

	int		numtransitions;	// animation node to animation node transition graph
	int		transitionindex;
} dstudiohdr_t;

// header for demand loaded sequence group data
typedef struct 
{
	int		id;
	int		version;

	char		name[64];
	int		length;
} dstudioseqhdr_t;

// bones
typedef struct 
{
	char		name[32];		// bone name for symbolic links
	int		parent;		// parent bone
	int		flags;		// ??
	int		bonecontroller[6];	// bone controller index, -1 == none
	float		value[6];		// default DoF values
	float		scale[6];		// scale for delta DoF values
} dstudiobone_t;

// bone controllers
typedef struct 
{
	int		bone;		// -1 == 0
	int		type;		// X, Y, Z, XR, YR, ZR, M
	float		start;
	float		end;
	int		rest;		// byte index value at rest
	int		index;		// 0-3 user set controller, 4 mouth
} dstudiobonecontroller_t;

// intersection boxes
typedef struct
{
	int		bone;
	int		group;		// intersection group
	vec3_t		bbmin;		// bounding box
	vec3_t		bbmax;		
} dstudiobbox_t;

// demand loaded sequence groups
typedef struct
{
	char		label[32];	// textual name
	char		name[64];		// file name
	void		*cache;		// cache index pointer (only in memory)
	int		data;		// hack for group 0
} dstudioseqgroup_t;

// sequence descriptions
typedef struct
{
	char		label[32];	// sequence label (name)

	float		fps;		// frames per second	
	int		flags;		// looping/non-looping flags

	int		activity;
	int		actweight;

	int		numevents;
	int		eventindex;

	int		numframes;	// number of frames per sequence

	int		numpivots;	// number of foot pivots
	int		pivotindex;

	int		motiontype;	
	int		motionbone;
	vec3_t		linearmovement;
	int		automoveposindex;
	int		automoveangleindex;

	vec3_t		bbmin;		// per sequence bounding box
	vec3_t		bbmax;		

	int		numblends;
	int		animindex;	// mstudioanim_t pointer relative to start of sequence group data
					// [blend][bone][X, Y, Z, XR, YR, ZR]

	int		blendtype[2];	// X, Y, Z, XR, YR, ZR
	float		blendstart[2];	// starting value
	float		blendend[2];	// ending value
	int		blendparent;

	int		seqgroup;		// sequence group for demand loading

	int		entrynode;	// transition node at entry
	int		exitnode;		// transition node at exit
	int		nodeflags;	// transition rules
	
	int		nextseq;		// auto advancing sequences
} dstudioseqdesc_t;

// events
#include "studio_event.h"

// pivots
typedef struct 
{
	vec3_t		org;		// pivot point
	int		start;
	int		end;
} dstudiopivot_t;

// attachment
typedef struct 
{
	char		name[32];
	int		type;
	int		bone;
	vec3_t		org;		// attachment point
	vec3_t		vectors[3];
} dstudioattachment_t;

typedef struct
{
	unsigned short	offset[6];
} dstudioanim_t;

// animation frames
typedef union 
{
	struct
	{
		byte	valid;
		byte	total;
	} num;
	short		value;
} dstudioanimvalue_t;


// body part index
typedef struct
{
	char		name[64];
	int		nummodels;
	int		base;
	int		modelindex;	// index into models array
} dstudiobodyparts_t;

// skin info
typedef struct
{
	char		name[64];
	int		flags;
	int		width;
	int		height;

	union
	{
		int	index;		// disk: offset at start of buffer
		shader_t	shader;		// ref: shader number
	};
} dstudiotexture_t;

// skin families
// short	index[skinfamilies][skinref]		// skingroup info

// studio models
typedef struct
{
	char		name[64];

	int		type;
	float		boundingradius;	// software stuff

	int		nummesh;
	int		meshindex;

	int		numverts;		// number of unique vertices
	int		vertinfoindex;	// vertex bone info
	int		vertindex;	// vertex vec3_t
	int		numnorms;		// number of unique surface normals
	int		norminfoindex;	// normal bone info
	int		normindex;	// normal vec3_t

	int		numgroups;	// deformation groups
	int		groupindex;
} dstudiomodel_t;

// meshes
typedef struct 
{
	int		numtris;
	int		triindex;
	int		skinref;
	int		numnorms;		// per mesh normals
	int		normindex;	// normal vec3_t
} dstudiomesh_t;

#endif//STUDIO_REF_H