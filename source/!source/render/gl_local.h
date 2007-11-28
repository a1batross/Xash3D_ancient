/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include <windows.h>
#include <stdio.h>
#include "basetypes.h"
#include "mathlib.h"

#define RENDERPASS_SOLID	1
#define RENDERPASS_ALPHA	2

#define Q_ftol( f ) ( long ) (f)

#define SOLID_FORMAT	3
#define ALPHA_FORMAT	4

#define WINDOW_STYLE (WS_OVERLAPPED|WS_BORDER|WS_CAPTION|WS_VISIBLE)

#include "r_opengl.h"

/*
===========================================
memory manager
===========================================
*/
//z_malloc-free
#define Z_Malloc(size) Mem_Alloc(r_temppool, size)
#define Z_Free(data) Mem_Free(data)

extern byte *r_temppool;
extern stdlib_api_t std;

// r_utils.c
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );
void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal );
void PerpendicularVector( vec3_t dst, const vec3_t src );
void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4]);

#define Msg std.printf
#define MsgDev std.dprintf
#define MsgWarn std.wprintf
#define Sys_Error std.error

#define MAX_RADAR_ENTS	1024
typedef struct radar_ent_s
{
	vec4_t	color;
	vec3_t	org;  
	vec3_t	ang;
} radar_ent_t;

int numRadarEnts;
extern radar_ent_t RadarEnts[MAX_RADAR_ENTS];

typedef struct rect_s
{
	int left;
	int right;
	int top;
	int bottom;
}wrect_t;

/*

  skins will be outline flood filled and mip mapped
  pics and sprites with alpha will be outline flood filled
  pic won't be mip mapped

  model skin
  sprite frame
  wall texture
  pic

*/

typedef enum 
{
	it_skin,
	it_sprite,
	it_wall,
	it_pic,
	it_sky,
	it_cubemap,
} imagetype_t;

// texnum cubemap order
// 0 = ft or normal image
// 1 = bk
// 2 = rt
// 3 = lf
// 4 = up
// 5 = dn

typedef struct image_s
{
	char		name[MAX_QPATH];		// game path, including extension
	imagetype_t	type;			// image type
	int		width, height;		// source image
	int		registration_sequence;	// 0 = free
	struct msurface_s	*texturechain;		// for sort-by-texture world drawing
	int		texnum[6];		// gl texture binding
	bool 		paletted;
	int		texorder[6];		// drawing order pattern
};

#define	TEXNUM_LIGHTMAPS	1024
#define	TEXNUM_IMAGES	1152
#define	MAX_GLTEXTURES	1024

//===================================================================

typedef enum
{
	rserr_ok,

	rserr_invalid_fullscreen,
	rserr_invalid_mode,

	rserr_unknown
} rserr_t;

#include "gl_model.h"

void GL_BeginRendering (int *x, int *y, int *width, int *height);
void GL_EndRendering (void);

void GL_SetDefaultState( void );
void GL_UpdateSwapInterval( void );

extern	float	gldepthmin, gldepthmax;

typedef struct
{
	float	x, y, z;
	float	s, t;
	float	r, g, b;
} glvert_t;


#define	MAX_LBM_HEIGHT		480

#define BACKFACE_EPSILON	0.01

//====================================================

extern	image_t		gltextures[MAX_GLTEXTURES];
extern	int		numgltextures;
extern	byte		*r_framebuffer;

extern image_t	*r_notexture;
extern image_t	*r_particletexture;
extern image_t	*r_radarmap;
extern image_t	*r_around;
extern	entity_t	*currententity;
extern	model_t		*currentmodel;
extern	int			r_visframecount;
extern	int			r_framecount;
extern	cplane_t	frustum[4];
extern	int			c_brush_polys;


extern	int			gl_filter_min, gl_filter_max;

//
// view origin
//
extern	vec3_t	vup;
extern	vec3_t	vright;
extern	vec3_t	vforward;
extern	vec3_t	r_origin;

//
// screen size info
//
extern	refdef_t	r_newrefdef;
extern	int	r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

extern	cvar_t	*r_norefresh;
extern	cvar_t	*r_lefthand;
extern	cvar_t	*r_drawentities;
extern	cvar_t	*r_drawworld;
extern	cvar_t	*r_speeds;
extern	cvar_t	*r_fullbright;
extern	cvar_t	*r_novis;
extern	cvar_t	*r_nocull;
extern	cvar_t	*r_lerpmodels;
extern	cvar_t	*r_loading;
extern	cvar_t	*r_pause;
extern	cvar_t	*r_width;
extern	cvar_t	*r_height;
extern	cvar_t	*r_mode;

extern	cvar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level

extern cvar_t	*gl_vertex_arrays;

extern cvar_t	*gl_ext_swapinterval;
extern cvar_t	*gl_ext_multitexture;
extern cvar_t	*gl_ext_pointparameters;
extern cvar_t	*gl_ext_compiled_vertex_array;

extern cvar_t	*gl_particle_min_size;
extern cvar_t	*gl_particle_max_size;
extern cvar_t	*gl_particle_size;
extern cvar_t	*gl_particle_att_a;
extern cvar_t	*gl_particle_att_b;
extern cvar_t	*gl_particle_att_c;

extern  cvar_t	*r_minimap;
extern  cvar_t	*r_minimap_size;
extern  cvar_t	*r_minimap_zoom;
extern  cvar_t	*r_minimap_style;

extern  cvar_t	*r_bloom;
extern  cvar_t	*r_bloom_alpha;
extern  cvar_t	*r_bloom_diamond_size;
extern  cvar_t	*r_bloom_intensity;
extern  cvar_t	*r_bloom_darken;
extern  cvar_t	*r_bloom_sample_size;
extern  cvar_t	*r_bloom_fast_sample;

extern	cvar_t	*r_emboss_bump;

extern	cvar_t	*r_motionblur_intens;
extern	cvar_t	*r_motionblur;

extern	cvar_t	*gl_nosubimage;
extern	cvar_t	*gl_bitdepth;
extern	cvar_t	*gl_log;
extern	cvar_t	*gl_lightmap;
extern	cvar_t	*gl_shadows;
extern	cvar_t	*gl_dynamic;
extern	cvar_t	*gl_nobind;
extern	cvar_t	*gl_round_down;
extern	cvar_t	*gl_skymip;
extern	cvar_t	*gl_showtris;
extern	cvar_t	*gl_finish;
extern	cvar_t	*gl_ztrick;
extern	cvar_t	*gl_clear;
extern	cvar_t	*gl_cull;
extern	cvar_t	*gl_poly;
extern	cvar_t	*gl_texsort;
extern	cvar_t	*gl_polyblend;
extern	cvar_t	*gl_flashblend;
extern	cvar_t	*gl_lightmaptype;
extern	cvar_t	*gl_modulate;
extern	cvar_t	*gl_playermip;
extern	cvar_t	*gl_drawbuffer;
extern	cvar_t	*gl_3dlabs_broken;
extern	cvar_t	*gl_swapinterval;
extern	cvar_t	*gl_texturemode;
extern	cvar_t	*gl_texturealphamode;
extern	cvar_t	*gl_texturesolidmode;
extern  cvar_t  *gl_saturatelighting;
extern  cvar_t  *gl_lockpvs;

extern	cvar_t	*r_fullscreen;
extern	cvar_t	*vid_gamma;

extern	cvar_t		*intensity;

extern	int		gl_lightmap_format;
extern	int		gl_tex_solid_format;
extern	int		gl_tex_alpha_format;

extern	int		c_visible_lightmaps;
extern	int		c_visible_textures;

extern	float	r_world_matrix[16];

void R_TranslatePlayerSkin (int playernum);
void GL_Bind (int texnum);
void GL_MBind( GLenum target, int texnum );
void GL_TexEnv( GLenum value );
void GL_EnableMultitexture( bool enable );
void GL_SelectTexture( GLenum );
void GL_TexFilter( GLboolean mipmap );
void GL_SetColor( const void *data );
void R_SetGL2D ( void );
void R_LightPoint (vec3_t p, vec3_t color);
void R_PushDlights (void);
bool VID_ScreenShot( const char *filename, bool force_gamma );
void VID_BuildGammaTable( void );
void VID_ImageLightScale (uint *in, int inwidth, int inheight );
void VID_ImageBaseScale (uint *in, int inwidth, int inheight );

//====================================================================

extern	model_t	*r_worldmodel;

extern	unsigned	d_8to24table[256];

extern	int		registration_sequence;


void V_AddBlend (float r, float g, float b, float a, float *v_blend);

int 	R_Init( void *hinstance, void *hWnd );
void	R_Shutdown( void );

void R_RenderView (refdef_t *fd);
void R_DrawStudioModel( int passnum );
void R_DrawBrushModel( int passnum );
void R_DrawSpriteModel( int passnum );
void R_StudioLoadModel (model_t *mod, void *buffer );
void R_SpriteLoadModel( model_t *mod, void *buffer );
void R_DrawPauseScreen( void );
char *R_ExtName( model_t *mod );
void R_DrawBeam( entity_t *e );
void R_DrawWorld (void);
void R_RenderDlights (void);
void R_DrawAlphaSurfaces (void);
void R_RenderBrushPoly (msurface_t *fa);
void R_InitParticleTexture (void);
void Draw_InitLocal (void);
void GL_SubdivideSurface (msurface_t *fa);
bool R_CullBox (vec3_t mins, vec3_t maxs);
void R_RotateForEntity (entity_t *e);
void R_MarkLeaves (void);

void GL_DrawRadar( void );
void R_BloomBlend ( refdef_t *fd );
void R_Bloom_InitTextures( void );

glpoly_t *WaterWarpPolyVerts (glpoly_t *p);
void EmitWaterPolys (msurface_t *fa);
void R_AddSkySurface (msurface_t *fa);
void R_ClearSkyBox (void);
void R_DrawSkyBox (void);
void R_MarkLights (dlight_t *light, int bit, mnode_t *node);
void R_RoundImageDimensions(int *scaled_width, int *scaled_height);

#if 0
short LittleShort (short l);
short BigShort (short l);
int	LittleLong (int l);
float LittleFloat (float f);

char	*va(char *format, ...);
// does a varargs printf into a temp buffer
#endif

void COM_StripExtension (char *in, char *out);
void COM_FileBase (char *in, char *out);

void	Draw_GetPicSize (int *w, int *h, char *name);
void	Draw_Pic (int x, int y, char *name);
void	Draw_StretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, char *pic);
void	Draw_Char (float x, float y, int c);
void	Draw_String (int x, int y, char *str);
void	Draw_TileClear (int x, int y, int w, int h, char *name);
void	Draw_Fill(float x, float y, float w, float h );
void	Draw_FadeScreen (void);
void	Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data, bool dirty );

void	R_BeginFrame( void );
void	R_SwapBuffers( int );
void	R_SetPalette ( const unsigned char *palette);
void	R_GetPalette (void);

image_t	*R_RegisterSkin (char *name);

void R_ImageList_f (void);

image_t *R_LoadImage(char *name, rgbdata_t *pic, imagetype_t type );
image_t *R_FindImage (char *name, char *buffer, int size, imagetype_t type);

void	R_InitTextures( void );
void	R_ShutdownTextures (void);
void	R_ImageFreeUnused(void);

//backend funcs
void GL_TextureMode( char *string );
void GL_TextureAlphaMode( char *string );
void GL_TextureSolidMode( char *string );

void GL_EnableTexGen( void );
void GL_DisableTexGen( void );
void GL_EnableBlend( void );
void GL_DisableBlend( void );
void GL_EnableAlphaTest ( void );
void GL_DisableAlphaTest ( void );
void GL_EnableDepthTest( void );
void GL_DisableDepthTest( void );

/*
** GL extension emulation functions
*/
void GL_DrawParticles( int n, const particle_t particles[], const unsigned colortable[768] );

/*
** GL config stuff
*/
#define GL_RENDERER_VOODOO		0x00000001
#define GL_RENDERER_ATI		0x00000002
#define GL_RENDERER_NVIDIA		0x00000004
#define GL_RENDERER_DEFAULT		0x80000000

typedef struct
{
	int         renderer;
	const char *renderer_string;
	const char *vendor_string;
	const char *version_string;
	const char *extensions_string;

	bool	allow_cds;
	bool	arb_compressed_teximage;
} glconfig_t;

typedef struct
{
	float inverse_intensity;
	bool fullscreen;

	int	prev_mode;
	byte	*d_16to8table;

	int lightmap_textures;

	int	currenttextures[2];
	int	currenttmu;

	bool	alpha_test;
	bool	depth_test;
	bool	blend;
	bool	texgen;

	bool	reg_combiners;
	bool	texshaders;
	bool	sgis_mipmap;
	dword	dst_texture;
	bool	gammaramp;

	int	tex_rectangle_type;
	int	stencil_warp;
	int	stencil_two_side;
	int	ati_separate_stencil;
	bool	nv_tex_rectangle;
	bool	ati_tex_rectangle;

	vec4_t	draw_color;	// using with Draw_* functions

} glstate_t;

extern glconfig_t  gl_config;
extern glstate_t   gl_state;

/*
====================================================================

VERTEX ARRAYS

====================================================================
*/

#define MAX_ARRAY MAX_INPUTLINE	// sorry ...

#define VA_SetElem2(v, a, b) ((v)[0]=(a),(v)[1]=(b))
#define VA_SetElem3(v, a, b, c) ((v)[0]=(a),(v)[1]=(b),(v)[2]=(c))
#define VA_SetElem4(v, a, b, c, d) ((v)[0]=(a),(v)[1]=(b),(v)[2]=(c),(v)[3]=(d))

extern float tex_array [MAX_ARRAY][2];
extern float vert_array[MAX_ARRAY][3];
extern float col_array [MAX_ARRAY][4];

void GL_LockArrays( int count );
void GL_UnlockArrays( void );

/*
====================================================================

STUDIO FUNCTIONS

====================================================================
*/
void R_StudioInit( void );
void R_StudioShutdown( void );
/*
====================================================================

IMPORTED FUNCTIONS

====================================================================
*/

extern render_imp_t		ri;

/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

void		GLimp_BeginFrame( void );
void		GLimp_EndFrame( void );
int 		GLimp_Init( void *hinstance, void *hWnd );
void		GLimp_Shutdown( void );
int		GLimp_SetMode( int vid_mode, bool fullscreen );
void		GLimp_AppActivate( bool active );
void		GLimp_EnableLogging( bool enable );
void		GLimp_LogNewFrame( void );

/*
====================================================================

BUILT-IN MATHLIB FUNCTIONS

====================================================================
*/

void VectorTransform (const vec3_t in1, matrix3x4 in2, vec3_t out);
void AngleQuaternion( float *angles, vec4_t quaternion );
void QuaternionMatrix( vec4_t quaternion, float (*matrix)[4] );
void QuaternionSlerp( vec4_t p, vec4_t q, float t, vec4_t qt );
void AngleMatrix (const float *angles, float (*matrix)[4] );
void MatrixCopy( matrix3x4 in, matrix3x4 out );

uint ShortToFloat( word y );
void R_DXTReadColor(word data, color32* out);
void R_DXTReadColors(const byte* data, color32* out);
void R_GetBitsFromMask(uint Mask, uint *ShiftLeft, uint *ShiftRight);