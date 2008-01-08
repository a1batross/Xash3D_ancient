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
	Msg("GL_VENDOR: %s\n", gl_config.vendor_string );
	Msg("GL_RENDERER: %s\n", gl_config.renderer_string );
	Msg("GL_VERSION: %s\n", gl_config.version_string );
	Msg("GL_EXTENSIONS: %s\n", gl_config.extensions_string );
}

void GL_UpdateGammaRamp( void )
{
	int          i, j, v;

	Mem_Copy( gl_config.gamma_ramp, gl_config.original_ramp, sizeof(gl_config.gamma_ramp));

	for(j = 0; j < 3; j++)
	{
		for( i = 0; i < 256; i++)
		{
			v = 255 * pow((float)(i + 0.5) / 255, vid_gamma->value ) + 0.5;
			v = bound(v, 0, 255);
			gl_config.gamma_ramp[j][i] = (WORD)v << 8;
		}
	}
	SetDeviceGammaRamp(glw_state.hDC, gl_config.gamma_ramp );
}

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
		gl_state.alpha_test = true;
	}
}

void GL_DisableAlphaTest ( void )
{
	if (gl_state.alpha_test)
	{
		qglDisable(GL_ALPHA_TEST);
		gl_state.alpha_test = false;
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
		gl_state.blend = true;
	}
}

void GL_DisableBlend( void )
{
	if (gl_state.blend)
	{
		qglDisable(GL_BLEND);
		gl_state.blend = false;
	}
}

/*
===============
GL_StateDepthTest
===============
*/
void GL_EnableDepthTest( void )
{
	if (!gl_state.depth_test)
	{
		qglEnable( GL_DEPTH_TEST );
		gl_state.depth_test = true;
	}
}

void GL_DisableDepthTest( void )
{
	if (gl_state.depth_test)
	{
		qglDisable( GL_DEPTH_TEST );
		gl_state.depth_test = false;
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
		Msg("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (glt->type != it_pic && glt->type != it_sky )
		{
			GL_Bind (glt->texnum[0]);
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
		Msg("bad alpha texture mode name\n");
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
		Msg("bad solid texture mode name\n");
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
GL_TexFilter
===============
*/
void GL_TexFilter( void )
{
	if(R_ImageHasMips())
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
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

	if (gl_nobind->value && draw_chars) // performance evaluation option
		texnum = draw_chars->texnum[0];
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
=============
GL_SetColor

=============
*/
void GL_SetColor( const void *data )
{
	float	*color = (float *)data;

	if(color) 
	{
		Vector4Set(gl_state.draw_color, color[0], color[1], color[2], color[3] );
	}
	else
	{
		Vector4Set(gl_state.draw_color, 1.0f, 1.0f, 1.0f, 1.0f );
	}
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
	gl_state.blend = false;
	
	qglColor4f (1,1,1,1);

	Vector4Set(gl_state.draw_color, 1.0f, 1.0f, 1.0f, 1.0f );

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

		if ( qwglSwapIntervalEXT )
			qwglSwapIntervalEXT( gl_swapinterval->value );
	}
}

void qglPerspective( GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar )
{
	GLdouble xmin, xmax, ymin, ymax;

	ymax = zNear * tan( fovy * M_PI / 360.0 );
	ymin = -ymax;
	xmin = ymin * aspect;
	xmax = ymax * aspect;
	xmin += -2.0f / zNear;
	xmax += -2.0f / zNear;

	qglFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
}

bool VID_ScreenShot( const char *filename, bool levelshot )
{
	rgbdata_t 	*r_shot;

	// shared framebuffer not init
	if(!r_framebuffer) return false;

	// get screen frame
	qglReadPixels(0, 0, r_width->integer, r_height->integer, GL_RGB, GL_UNSIGNED_BYTE, r_framebuffer );

	r_shot  = Z_Malloc( sizeof(rgbdata_t));
	r_shot->width = r_width->integer;
	r_shot->height = r_height->integer;
	r_shot->type = PF_RGB_24_FLIP;
	r_shot->numMips = 1;
	r_shot->palette = NULL;
	r_shot->buffer = r_framebuffer;

	// levelshot always have const size
	if( levelshot ) Image_Processing( filename, &r_shot, 512, 384 );

	// write image
	FS_SaveImage( filename, r_shot );
	FS_FreeImage( r_shot );
	return true;
}