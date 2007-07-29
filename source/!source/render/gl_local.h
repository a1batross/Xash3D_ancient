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

#ifdef _WIN32
#  include <windows.h>
#endif

#include <stdio.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>

#include "basetypes.h"
#include <basemath.h>
#include "ref_system.h"
#include "materials.h"
#include "qfiles.h"
#include "const.h"

#ifndef GL_COLOR_INDEX8_EXT
#define GL_COLOR_INDEX8_EXT GL_COLOR_INDEX
#endif

#define RENDERPASS_SOLID	1
#define RENDERPASS_ALPHA	2

#define Q_ftol( f ) ( long ) (f)

#define SOLID_FORMAT	3
#define ALPHA_FORMAT	4

#define DLLEXPORT __declspec(dllexport)
#define WINDOW_STYLE (WS_OVERLAPPED|WS_BORDER|WS_CAPTION|WS_VISIBLE)

#include "r_opengl.h"

#define	REF_VERSION		"GL 0.01"

/*
===========================================
memory manager
===========================================
*/
//z_malloc-free
#define Z_Malloc(size) Mem_Alloc(r_temppool, size)
#define Z_Free(data) Mem_Free(data)

//malloc-free
#define Mem_Alloc(pool,size) ri.Mem.Alloc(pool, size, __FILE__, __LINE__)
#define Mem_Free(mem) ri.Mem.Free(mem, __FILE__, __LINE__)

//Hunk_AllocName
#define Mem_AllocPool(name) ri.Mem.AllocPool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool) ri.Mem.FreePool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool) ri.Mem.EmptyPool(pool, __FILE__, __LINE__)

extern byte *r_temppool;

//r_utils.c
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );
void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal );
void PerpendicularVector( vec3_t dst, const vec3_t src );
void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4]);

#define Msg ri.Stdio.printf
#define MsgDev ri.Stdio.dprintf
#define Sys_Error ri.Stdio.error

/*
===========================================
filesystem manager
===========================================
*/
#define FS_LoadFile(name, size) ri.Fs.LoadFile(name, size)
#define FS_LoadImage(name, data, size) ri.Fs.LoadImage(name, data, size)
#define FS_FreeImage(data) ri.Fs.FreeImage(data)
#define FS_Search(path) ri.Fs.Search( path, true )
#define FS_WriteFile(name, data, size) ri.Fs.WriteFile(name, data, size )
#define FS_Open( path, mode ) ri.Fs.Open( path, mode )
#define FS_Read( file, buffer, size ) ri.Fs.Read( file, buffer, size )
#define FS_Write( file, buffer, size ) ri.Fs.Write( file, buffer, size )
#define FS_StripExtension( path ) ri.Fs.StripExtension( path )
#define FS_DefaultExtension( path, ext ) ri.Fs.DefaultExtension( path, ext )
#define FS_FileExtension( ext ) ri.Fs.FileExtension( ext )
#define FS_FileExists( file ) ri.Fs.FileExists( file )
#define FS_Close( file ) ri.Fs.Close( file )
#define FS_FileBase( x, y ) ri.Fs.FileBase( x, y )
#define FS_Printf ri.Fs.Printf
#define FS_Seek ri.Fs.Seek
#define FS_Tell ri.Fs.Tell
#define FS_Gets ri.Fs.Gets
char *FS_Gamedir( void );
char *FS_Title( void );

// up / down
#define	PITCH	0

// left / right
#define	YAW		1

// fall over
#define	ROLL	2


#ifndef __VIDDEF_T
#define __VIDDEF_T
typedef struct
{
	unsigned		width, height;			// coordinates from main game
} viddef_t;
#endif

extern	viddef_t	vid;

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

// texnum supported cubemaps
// 0 = rt or normal image
// 1 = bk
// 2 = lf
// 3 = ft
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

	//new stuff starts here
} image_t;

#define	TEXNUM_LIGHTMAPS	1024
#define	TEXNUM_SCRAPS	1152
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
extern	int			numgltextures;


extern	image_t		*r_notexture;
extern	image_t		*r_particletexture;
extern	entity_t	*currententity;
extern	model_t		*currentmodel;
extern	int			r_visframecount;
extern	int			r_framecount;
extern	cplane_t	frustum[4];
extern	int			c_brush_polys, c_alias_polys;


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
extern	int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

extern	cvar_t	*r_norefresh;
extern	cvar_t	*r_lefthand;
extern	cvar_t	*r_drawentities;
extern	cvar_t	*r_drawworld;
extern	cvar_t	*r_speeds;
extern	cvar_t	*r_fullbright;
extern	cvar_t	*r_novis;
extern	cvar_t	*r_nocull;
extern	cvar_t	*r_lerpmodels;

extern	cvar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level

extern cvar_t	*gl_vertex_arrays;

extern cvar_t	*gl_ext_swapinterval;
extern cvar_t	*gl_ext_palettedtexture;
extern cvar_t	*gl_ext_multitexture;
extern cvar_t	*gl_ext_pointparameters;
extern cvar_t	*gl_ext_compiled_vertex_array;

extern cvar_t	*gl_particle_min_size;
extern cvar_t	*gl_particle_max_size;
extern cvar_t	*gl_particle_size;
extern cvar_t	*gl_particle_att_a;
extern cvar_t	*gl_particle_att_b;
extern cvar_t	*gl_particle_att_c;

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
extern	cvar_t	*gl_mode;
extern	cvar_t	*gl_log;
extern	cvar_t	*gl_lightmap;
extern	cvar_t	*gl_shadows;
extern	cvar_t	*gl_dynamic;
extern	cvar_t	*gl_nobind;
extern	cvar_t	*gl_round_down;
extern	cvar_t	*gl_picmip;
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
extern  cvar_t  *gl_driver;
extern	cvar_t	*gl_swapinterval;
extern	cvar_t	*gl_texturemode;
extern	cvar_t	*gl_texturealphamode;
extern	cvar_t	*gl_texturesolidmode;
extern  cvar_t  *gl_saturatelighting;
extern  cvar_t  *gl_lockpvs;

extern	cvar_t	*vid_fullscreen;
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

void R_LightPoint (vec3_t p, vec3_t color);
void R_PushDlights (void);

//====================================================================

extern	model_t	*r_worldmodel;

extern	unsigned	d_8to24table[256];

extern	int		registration_sequence;


void V_AddBlend (float r, float g, float b, float a, float *v_blend);

int 	R_Init( void *hinstance, void *hWnd );
void	R_Shutdown( void );

void R_RenderView (refdef_t *fd);
void GL_ScreenShot_f (void);
void R_DrawAliasModel( int passnum );
void R_DrawStudioModel( int passnum );
void R_DrawBrushModel( int passnum );
void R_SpriteDrawModel( int passnum );
void R_StudioLoadModel (model_t *mod, void *buffer );
void R_SpriteLoadModel( model_t *mod, void *buffer );
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

void R_BloomBlend ( refdef_t *fd );
void R_Bloom_InitTextures( void );

glpoly_t *WaterWarpPolyVerts (glpoly_t *p);
void EmitWaterPolys (msurface_t *fa);
void R_AddSkySurface (msurface_t *fa);
void R_ClearSkyBox (void);
void R_DrawSkyBox (void);
void R_MarkLights (dlight_t *light, int bit, mnode_t *node);

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
void	Draw_StretchPic (int x, int y, int w, int h, char *name);
void	Draw_Char (int x, int y, int c);
void	Draw_String (int x, int y, char *str);
void	Draw_TileClear (int x, int y, int w, int h, char *name);
void	Draw_Fill (int x, int y, int w, int h, int c);
void	Draw_FadeScreen (void);
void	Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data);

void	R_BeginFrame( float camera_separation );
void	R_SwapBuffers( int );
void	R_SetPalette ( const unsigned char *palette);
void	R_GetPalette (void);

struct image_s *R_RegisterSkin (char *name);

byte *LoadTGA (char *filename, int *width, int *height);
byte *LoadJPG (char *filename, int *width, int *height);

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

	int     prev_mode;

	unsigned char *d_16to8table;

	int lightmap_textures;

	int	currenttextures[2];
	int currenttmu;

	float camera_separation;
	bool stereo_enabled;

	bool	alpha_test;
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

	byte	originalRedGammaTable[256];
	byte	originalGreenGammaTable[256];
	byte	originalBlueGammaTable[256];
} glstate_t;

extern glconfig_t  gl_config;
extern glstate_t   gl_state;

/*
====================================================================

VERTEX ARRAYS

====================================================================
*/

#define MAX_ARRAY MAX_PARTICLES*4

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

extern renderer_imp_t	ri;

/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

void		GLimp_BeginFrame( float camera_separation );
void		GLimp_EndFrame( void );
int 		GLimp_Init( void *hinstance, void *hWnd );
void		GLimp_Shutdown( void );
int		GLimp_SetMode( int *pwidth, int *pheight, int mode, bool fullscreen );
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