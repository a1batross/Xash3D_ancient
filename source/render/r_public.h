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
#ifndef __R_PUBLIC_H__
#define __R_PUBLIC_H__

// FIXME: move these to r_local.h?
#define MAX_SKINFILES			256
#define MAX_ENTITIES			2048
#define MAX_POLY_VERTS			3000
#define MAX_POLYS				2048

// refdef flags
#define RDF_UNDERWATER		1 	// warp the screen as apropriate
#define RDF_NOWORLDMODEL		2 	// used for player configuration screen
#define RDF_BLOOM			4 	// FIXME?
#define RDF_OLDAREABITS		8 	// forces R_MarkLeaves if not set
#define RDF_PORTALINVIEW		16	// cull entities using vis too because pvs\areabits are merged serverside
#define RDF_SKYPORTALINVIEW		32	// draw skyportal instead of regular sky
#define RDF_NOFOVADJUSTMENT		64	// do not adjust fov for widescreen
#define RDF_WORLDOUTLINES		128	// draw cell outlines for world surfaces

// skm flags
#define SKM_ATTACHMENT_BONE	1

typedef struct 
{
	vec3_t			axis[3];
	vec3_t			origin;
} orientation_t;

typedef struct
{
	quat_t			quat;
	vec3_t			origin;
} bonepose_t;

typedef struct
{
	float			rgb[3];			// 0.0 - 2.0
} lightstyle_t;

typedef enum
{
	RT_MODEL,
	RT_SPRITE,
	RT_PORTALSURFACE,

	NUM_RTYPES
} refEntityType_t;

typedef struct
{
	byte		open;		// 0 = mouth closed, 255 = mouth agape
	byte		sndcount;		// counter for running average
	int		sndavg;		// running average
} mouth_t;

typedef struct latchedvars_s
{
	float		animtime;		// ???
	float		sequencetime;
	vec3_t		origin;		// edict->v.old_origin
	vec3_t		angles;		

	vec3_t		gaitorigin;
	int		sequence;		// ???
	float		frame;

	float		blending[MAXSTUDIOBLENDS];
	float		seqblending[MAXSTUDIOBLENDS];
	float		controller[MAXSTUDIOCONTROLLERS];

} latchedvars_t;

typedef struct entity_s
{
	edtype_t			ent_type;		// entity type
	int			index;		// viewmodel has entindex -1
	refEntityType_t		rtype;

	struct ref_model_s		*model;			// opaque type outside refresh
	struct ref_model_s		*weaponmodel;		// opaque type outside refresh

	latchedvars_t	prev;		// previous frame values for lerping

	int		flags;

	float		framerate;	// custom framerate
          float		animtime;		// lerping animtime	
	int		frame;		// also used as RF_BEAM's diameter

	int		body;
	int		skin;

	float		blending[MAXSTUDIOBLENDS];
	vec3_t		attachment[MAXSTUDIOATTACHMENTS];
	float		controller[MAXSTUDIOCONTROLLERS];
	mouth_t		mouth;		// for synchronizing mouth movements.

	int		gaitsequence;	// client->sequence + yaw
	float		gaitframe;	// client->frame + yaw
	float		gaityaw;		// local value

          int		movetype;		// entity moving type
	int		sequence;
	float		scale;

	// misc
	float		backlerp;		// 0.0 = current, 1.0 = old
	rgb_t		rendercolor;	// hl1 rendercolor
	byte		renderamt;	// hl1 alphavalues
	int		rendermode;	// hl1 rendermode
	int		renderfx;		// server will be translate hl1 values into flags
	int		colormap;		// q1 and hl1 model colormap (can applied for sprites)
	int		effects;		// q1 effect flags, EF_ROTATE, EF_DIMLIGHT etc

	int		m_fSequenceLoops;
	int		m_fSequenceFinished;

	/*
	** most recent data
	*/
	vec3_t			axis[3];
	vec3_t			angles;
	vec3_t			movedir;		// forward vector that computed on a server
	vec3_t			origin, origin2;
	vec3_t			lightingOrigin;
	bonepose_t		*boneposes;		// pretransformed boneposes for current frame

	/*
	** previous data for lerping
	*/
	int			oldframe;
	bonepose_t		*oldboneposes;	// pretransformed boneposes for old frame

	/*
	** texturing
	*/
	int			skinNum;
	struct skinfile_s		*customSkin;	// registered .skin file
	struct ref_shader_s		*customShader;	// NULL for inline skin

	/*
	** misc
	*/
	float			shaderTime;
	union
	{
		rgba_t		color;
		rgba_t		shaderRGBA;
	};

	float			radius;			// used as RT_SPRITE's radius
	float			rotation;

#ifdef HARDWARE_OUTLINES
	float			outlineHeight;
	union
	{
		rgba_t		outlineColor;
		rgba_t		outlineRGBA;
	};
#endif
} ref_entity_t;

void		R_ModelBounds( const struct ref_model_s *model, vec3_t mins, vec3_t maxs );

struct ref_model_s *R_RegisterModel( const char *name );
struct ref_shader_s *R_RegisterPic( const char *name );
struct ref_shader_s *R_RegisterShader( const char *name );
struct ref_shader_s *R_RegisterSkin( const char *name );
struct skinfile_s *R_RegisterSkinFile( const char *name );

void		R_ClearScene( void );
void		R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b, const struct ref_shader_s *shader );
bool		R_AddPolyToScene( const poly_t *poly );
void		R_AddLightStyleToScene( int style, float r, float g, float b );
void		R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, shader_t shader );
void		R_DrawStretchRaw( int x, int y, int w, int h, int cols, int rows, byte *data, int frame );

void		R_SetCustomColor( int num, int r, int g, int b );
void		R_LightForOrigin( const vec3_t origin, vec3_t dir, vec4_t ambient, vec4_t diffuse, float radius );

bool	R_LerpTag( orientation_t *orient, const struct ref_model_s *mod, int oldframe, int frame, float lerpfrac,
					  const char *name );

int			R_SkeletalGetNumBones( const struct ref_model_s *mod, int *numFrames );
int			R_SkeletalGetBoneInfo( const struct ref_model_s *mod, int bone, char *name, size_t name_size, int *flags );
void		R_SkeletalGetBonePose( const struct ref_model_s *mod, int bone, int frame, bonepose_t *bonepose );

int		R_GetClippedFragments( const vec3_t origin, float radius, vec3_t axis[3], int maxfverts, vec3_t *fverts, 
								  int maxfragments, fragment_t *fragments );

void		R_TransformVectorToScreen( const ref_params_t *rd, const vec3_t in, vec2_t out );

const char	*R_SpeedsMessage( char *out, size_t size );
void		GLimp_AppActivate( bool active );

// Xash renderer exports
bool R_Init( bool full );
void R_Shutdown( bool full );
void R_BeginRegistration( const char *model, const dvis_t *visData );
shader_t Mod_RegisterShader( const char *name, int shaderType );
void R_EndRegistration( const char *skyname );
void R_BeginFrame( void );
void R_RenderScene( const ref_params_t *fd );
void R_EndFrame( void );

bool R_AddEntityToScene( edict_t *pRefEntity, int ed_type, float lerp );
bool VID_ScreenShot( const char *filename, bool levelshot );
bool VID_CubemapShot( const char *base, uint size, bool skyshot );


#endif /*__R_PUBLIC_H__*/