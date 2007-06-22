//=======================================================================
//			Copyright XashXT Group 2007 ©
//		     r_backend.c - open gl backend utilites
//=======================================================================

#include "gl_local.h" 

#define NUM_GL_MODES (sizeof(modes) / sizeof (glmode_t))
#define NUM_GL_ALPHA_MODES (sizeof(gl_alpha_modes) / sizeof (gltmode_t))
#define NUM_GL_SOLID_MODES (sizeof(gl_solid_modes) / sizeof (gltmode_t))

//set initial values
int gl_tex_solid_format = 3;
int gl_tex_alpha_format = 4;
int gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int gl_filter_max = GL_LINEAR;

typedef struct
{
	char *name;
	int minimize;
	int maximize;
} glmode_t;

glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

typedef struct
{
	char *name;
	int mode;
}gltmode_t;

gltmode_t gl_alpha_modes[] = {
	{"default", 4},
	{"GL_RGBA", GL_RGBA},
	{"GL_RGBA8", GL_RGBA8},
	{"GL_RGB5_A1", GL_RGB5_A1},
	{"GL_RGBA4", GL_RGBA4},
	{"GL_RGBA2", GL_RGBA2},
};

gltmode_t gl_solid_modes[] = {
	{"default", 3},
	{"GL_RGB", GL_RGB},
	{"GL_RGB8", GL_RGB8},
	{"GL_RGB5", GL_RGB5},
	{"GL_RGB4", GL_RGB4},
	{"GL_R3_G3_B2", GL_R3_G3_B2},
};

/*
===============
GL_Strings_f
===============
*/
void GL_Strings_f( void )
{
	ri.Con_Printf (PRINT_ALL, "GL_VENDOR: %s\n", gl_config.vendor_string );
	ri.Con_Printf (PRINT_ALL, "GL_RENDERER: %s\n", gl_config.renderer_string );
	ri.Con_Printf (PRINT_ALL, "GL_VERSION: %s\n", gl_config.version_string );
	ri.Con_Printf (PRINT_ALL, "GL_EXTENSIONS: %s\n", gl_config.extensions_string );
}

#define GLSTATE_DISABLE_ALPHATEST
#define GLSTATE_ENABLE_ALPHATEST	

#define GLSTATE_DISABLE_BLEND		
#define GLSTATE_ENABLE_BLEND		

#define GLSTATE_DISABLE_TEXGEN		
#define GLSTATE_ENABLE_TEXGEN		

/*
===============
GL_ArraysState
===============
*/
void GL_LockArrays( int count )
{
	if (qglLockArraysEXT != 0)
		qglLockArraysEXT(0, count);
}

void GL_UnlockArrays( void )
{
	if (qglUnlockArraysEXT != 0)
		qglUnlockArraysEXT();
}

/*
===============
GL_StateAlphaTest
===============
*/
void GL_EnableAlphaTest ( void )
{
	if (!gl_state.alpha_test)
	{
		qglEnable(GL_ALPHA_TEST);
		gl_state.alpha_test=true;
	}
}

void GL_DisableAlphaTest ( void )
{
	if (gl_state.alpha_test)
	{
		qglDisable(GL_ALPHA_TEST);
		gl_state.alpha_test=false;
	}
}

/*
===============
GL_StateBlend
===============
*/
void GL_EnableBlend( void )
{
	if (!gl_state.blend)
	{
		qglEnable(GL_BLEND);
		gl_state.blend=true;
	}
}

void GL_DisableBlend( void )
{
	if (gl_state.blend)
	{
		qglDisable(GL_BLEND);
		gl_state.blend=false;
	}
}

/*
===============
GL_StateTexGen
===============
*/
void GL_EnableTexGen( void )
{
	if (gl_state.texgen) return;
	qglEnable(GL_TEXTURE_GEN_S);
	qglEnable(GL_TEXTURE_GEN_T);
	qglEnable(GL_TEXTURE_GEN_R);
	qglEnable(GL_TEXTURE_GEN_Q);
	gl_state.texgen = true;
}

void GL_DisableTexGen( void )
{
	if (!gl_state.texgen) return;
	qglDisable(GL_TEXTURE_GEN_S);
	qglDisable(GL_TEXTURE_GEN_T);
	qglDisable(GL_TEXTURE_GEN_R);
	qglDisable(GL_TEXTURE_GEN_Q);
	gl_state.texgen = false;
}

/*
===============
GL_TextureMode
===============
*/
void GL_TextureMode( char *string )
{
	int		i;
	image_t	*glt;

	for (i=0 ; i< NUM_GL_MODES ; i++)
	{
		if ( !strcasecmp( modes[i].name, string ) )
			break;
	}

	if (i == NUM_GL_MODES)
	{
		ri.Con_Printf (PRINT_ALL, "bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (glt->type != it_pic && glt->type != it_sky )
		{
			GL_Bind (glt->texnum);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

/*
===============
GL_TextureAlphaMode
===============
*/
void GL_TextureAlphaMode( char *string )
{
	int		i;

	for (i=0 ; i< NUM_GL_ALPHA_MODES ; i++)
	{
		if ( !strcasecmp( gl_alpha_modes[i].name, string ) )
			break;
	}

	if (i == NUM_GL_ALPHA_MODES)
	{
		ri.Con_Printf (PRINT_ALL, "bad alpha texture mode name\n");
		return;
	}

	gl_tex_alpha_format = gl_alpha_modes[i].mode;
}

/*
===============
GL_TextureSolidMode
===============
*/
void GL_TextureSolidMode( char *string )
{
	int		i;

	for (i=0 ; i< NUM_GL_SOLID_MODES ; i++)
	{
		if ( !strcasecmp( gl_solid_modes[i].name, string ) )
			break;
	}

	if (i == NUM_GL_SOLID_MODES)
	{
		ri.Con_Printf (PRINT_ALL, "bad solid texture mode name\n");
		return;
	}

	gl_tex_solid_format = gl_solid_modes[i].mode;
}

/*
===============
GL_EnableMultitexture
===============
*/
void GL_EnableMultitexture( bool enable )
{
	if ( !qglSelectTextureSGIS && !qglActiveTextureARB )
		return;

	if ( enable )
	{
		GL_SelectTexture( GL_TEXTURE1 );
		qglEnable( GL_TEXTURE_2D );
		GL_TexEnv( GL_REPLACE );
	}
	else
	{
		GL_SelectTexture( GL_TEXTURE1 );
		qglDisable( GL_TEXTURE_2D );
		GL_TexEnv( GL_REPLACE );
	}
	GL_SelectTexture( GL_TEXTURE0 );
	GL_TexEnv( GL_REPLACE );
}

/*
===============
GL_SelectTexture
===============
*/
void GL_SelectTexture( GLenum texture )
{
	int tmu;

	if ( !qglSelectTextureSGIS && !qglActiveTextureARB )
		return;

	if ( texture == GL_TEXTURE0 )
	{
		tmu = 0;
	}
	else
	{
		tmu = 1;
	}

	if ( tmu == gl_state.currenttmu )
	{
		return;
	}

	gl_state.currenttmu = tmu;

	if ( qglSelectTextureSGIS )
	{
		qglSelectTextureSGIS( texture );
	}
	else if ( qglActiveTextureARB )
	{
		qglActiveTextureARB( texture );
		qglClientActiveTextureARB( texture );
	}
}

/*
===============
GL_TexEnv
===============
*/
void GL_TexEnv( GLenum mode )
{
	static int lastmodes[2] = { -1, -1 };

	if ( mode != lastmodes[gl_state.currenttmu] )
	{
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode );
		lastmodes[gl_state.currenttmu] = mode;
	}
}

/*
===============
GL_Bind
===============
*/
void GL_Bind (int texnum)
{
	extern	image_t	*draw_chars;

	if (gl_nobind->value && draw_chars)		// performance evaluation option
		texnum = draw_chars->texnum;
	if ( gl_state.currenttextures[gl_state.currenttmu] == texnum)
		return;
	gl_state.currenttextures[gl_state.currenttmu] = texnum;
	qglBindTexture (GL_TEXTURE_2D, texnum);
}

/*
===============
GL_Bind
===============
*/
void GL_MBind( GLenum target, int texnum )
{
	GL_SelectTexture( target );
	if ( target == GL_TEXTURE0 )
	{
		if ( gl_state.currenttextures[0] == texnum )
			return;
	}
	else
	{
		if ( gl_state.currenttextures[1] == texnum )
			return;
	}
	GL_Bind( texnum );
}

/*
===============
GL_SetDefaultState
===============
*/
void GL_SetDefaultState( void )
{
	qglClearColor (1,0, 0.5 , 0.5);
	qglCullFace(GL_FRONT);
	qglEnable(GL_TEXTURE_2D);

	qglEnable(GL_ALPHA_TEST);
	qglAlphaFunc(GL_GREATER, 0.666);

	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);
	qglDisable (GL_BLEND);
	gl_state.blend=false;
	
	qglColor4f (1,1,1,1);

	qglHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);
	qglHint (GL_FOG_HINT, GL_FASTEST);
	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	qglShadeModel (GL_FLAT);

	GL_TextureMode( gl_texturemode->string );
	GL_TextureAlphaMode( gl_texturealphamode->string );
	GL_TextureSolidMode( gl_texturesolidmode->string );

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	GL_TexEnv( GL_REPLACE );

	gl_state.texgen = false;
	qglDisable(GL_TEXTURE_GEN_S);
	qglDisable(GL_TEXTURE_GEN_T);

	if ( qglPointParameterfEXT )
	{
		float attenuations[3];

		attenuations[0] = gl_particle_att_a->value;
		attenuations[1] = gl_particle_att_b->value;
		attenuations[2] = gl_particle_att_c->value;

		qglEnable( GL_POINT_SMOOTH );
		qglPointParameterfEXT( GL_POINT_SIZE_MIN_EXT, gl_particle_min_size->value );
		qglPointParameterfEXT( GL_POINT_SIZE_MAX_EXT, gl_particle_max_size->value );
		qglPointParameterfvEXT( GL_DISTANCE_ATTENUATION_EXT, attenuations );
	}

	GL_UpdateSwapInterval();
}

/*
===============
GL_UpdateSwapInterval
===============
*/
void GL_UpdateSwapInterval( void )
{
	if ( gl_swapinterval->modified )
	{
		gl_swapinterval->modified = false;

		if ( !gl_state.stereo_enabled ) 
		{
			if ( qwglSwapIntervalEXT )
				qwglSwapIntervalEXT( gl_swapinterval->value );
		}
	}
}

void qglPerspective( GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar )
{
	GLdouble xmin, xmax, ymin, ymax;

	ymax = zNear * tan( fovy * M_PI / 360.0 );
	ymin = -ymax;
	xmin = ymin * aspect;
	xmax = ymax * aspect;

	xmin += -( 2 * gl_state.camera_separation ) / zNear;
	xmax += -( 2 * gl_state.camera_separation ) / zNear;

	qglFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
}

static int gamma_forcenextframe = false;
static float cachegamma, cachebrightness, cachecontrast, cacheblack[3], cachegrey[3], cachewhite[3];
static int cachecolorenable, cachehwgamma;

bool vid_activewindow = true;
bool vid_usinghwgamma = false;
int vid_gammarampsize = 0;
word *vid_gammaramps = NULL;
word *vid_systemgammaramps = NULL;

cvar_t *vid_hardwaregammasupported;
cvar_t *v_hwgamma;
cvar_t *v_gamma;

void BuildGammaTable16(float prescale,float gamma,float scale,float base,unsigned short *out,int rampsize)
{
	int i,adjusted;
	double invgamma;

	invgamma = 1.0 / gamma;
	prescale /= (double) (rampsize - 1);
	for (i = 0;i < rampsize;i++)
	{
		adjusted = (int) (65535.0 * (pow((double) i * prescale,invgamma) * scale + base) + 0.5);
		out[i] = bound(0,adjusted,65535);
	}
}

int VID_SetGamma(word *ramps, int rampsize)
{
	HDC hdc = GetDC (NULL);
	int i = SetDeviceGammaRamp(hdc, ramps);
	ReleaseDC (NULL, hdc);
	return i; // return success or failure
}

int VID_GetGamma(word *ramps, int rampsize)
{
	HDC hdc = GetDC (NULL);
	int i = GetDeviceGammaRamp(hdc, ramps);
	ReleaseDC (NULL, hdc);
	return i; // return success or failure
}

void VID_UpdateGamma(bool force, int rampsize)
{
	float f;

	if (!force && !gamma_forcenextframe && cachehwgamma == (vid_activewindow ? v_hwgamma->value : 0) && v_gamma->value == cachegamma)
		return;

	f = bound(0.1, v_gamma->value, 2.5);
	if (v_gamma->value != f) ri.Cvar_SetValue("v_gamma", f);

	cachehwgamma = vid_activewindow ? v_hwgamma->value : 0;

	gamma_forcenextframe = false;

	if (cachehwgamma)
	{
		if (!vid_usinghwgamma)
		{
			vid_usinghwgamma = true;
			if (vid_gammarampsize != rampsize || !vid_gammaramps)
			{
				vid_gammarampsize = rampsize;
				if (vid_gammaramps) Free(vid_gammaramps);
				vid_gammaramps = (word *)Malloc(6 * vid_gammarampsize * sizeof(word));
				vid_systemgammaramps = vid_gammaramps + 3 * vid_gammarampsize;
			}
			VID_GetGamma(vid_systemgammaramps, vid_gammarampsize);
		}

		BuildGammaTable16(1.0f, cachegamma, 1.0f, 1.0f, vid_gammaramps, rampsize);
		BuildGammaTable16(1.0f, cachegamma, 1.0f, 1.0f, vid_gammaramps + vid_gammarampsize, rampsize);
		BuildGammaTable16(1.0f, cachegamma, 1.0f, 1.0f, vid_gammaramps + vid_gammarampsize * 2, rampsize);

		// set vid_hardwaregammasupported to true if VID_SetGamma succeeds, OR if vid_hwgamma is >= 2 (forced gamma - ignores driver return value)
		ri.Cvar_SetValue("vid_hardwaregammasupported", VID_SetGamma(vid_gammaramps, vid_gammarampsize) || cachehwgamma >= 2);
		// if custom gamma ramps failed (Windows stupidity), restore to system gamma
		if(!vid_hardwaregammasupported->value)
		{
			if (vid_usinghwgamma)
			{
				vid_usinghwgamma = false;
				VID_SetGamma(vid_systemgammaramps, vid_gammarampsize);
			}
		}
	}
	else
	{
		if (vid_usinghwgamma)
		{
			vid_usinghwgamma = false;
			VID_SetGamma(vid_systemgammaramps, vid_gammarampsize);
		}
	}
}

void VID_RestoreSystemGamma(void)
{
	if (vid_usinghwgamma)
	{
		vid_usinghwgamma = false;
		ri.Cvar_SetValue("vid_hardwaregammasupported", VID_SetGamma(vid_systemgammaramps, vid_gammarampsize));
		// force gamma situation to be reexamined next frame
		gamma_forcenextframe = true;
	}
}