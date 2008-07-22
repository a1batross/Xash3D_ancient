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
byte gammatable[256];

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

void GL_InitCommands( void )
{
	// system screen width and height (don't suppose for change from console at all)
	r_width = Cvar_Get("width", "640", 0, "screen width" );
	r_height = Cvar_Get("height", "480", 0, "screen height" );
	r_mode = Cvar_Get( "r_mode", "0", CVAR_ARCHIVE, "display resolution mode" );

	r_check_errors = Cvar_Get("r_check_errors", "1", CVAR_ARCHIVE, "ignore video engine errors" );
	r_lefthand = Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE, "viewmodel handedness" );
	r_norefresh = Cvar_Get ("r_norefresh", "0", 0, "no description" );
	r_fullbright = Cvar_Get ("r_fullbright", "0", 0, "disable lightmaps" );
	r_drawentities = Cvar_Get ("r_drawentities", "1", CVAR_ARCHIVE, "render entities" );
	r_drawworld = Cvar_Get ("r_drawworld", "1", 0, "render world" );
	r_novis = Cvar_Get ("r_novis", "0", 0, "ignore vis information (perfomance test)");
	r_nocull = Cvar_Get ("r_nocull", "0", 0, "ignore frustrum culling (perfomance test)");
	r_lerpmodels = Cvar_Get ("r_lerpmodels", "1", 0, "lerping model animations" );
	r_speeds = Cvar_Get ("r_speeds", "0", 0, "shows r_speeds" );
	r_pause = Cvar_Get("paused", "0", 0, "renderer pause" );
	r_pause_bw = Cvar_Get("r_pause_effect", "0", CVAR_ARCHIVE, "allow pause effect" );
	r_physbdebug = Cvar_Get( "cm_debugdraw", "0", CVAR_ARCHIVE, "draw physics hulls" );

	r_loading = Cvar_Get("scr_loading", "0", 0, "loading bar progress" );
	r_lightlevel = Cvar_Get ("r_lightlevel", "0", 0, "no description" );

 	r_motionblur_intens = Cvar_Get( "r_motionblur_intens", "0.65", CVAR_ARCHIVE, "no description" );
	r_motionblur = Cvar_Get( "r_motionblur", "0", CVAR_ARCHIVE, "no description" );

	gl_nosubimage = Cvar_Get( "gl_nosubimage", "0", 0, "no description" );
	gl_particle_min_size = Cvar_Get( "gl_particle_min_size", "2", CVAR_ARCHIVE, "no description" );
	gl_particle_max_size = Cvar_Get( "gl_particle_max_size", "40", CVAR_ARCHIVE, "no description" );
	gl_particle_size = Cvar_Get( "gl_particle_size", "40", CVAR_ARCHIVE, "no description" );
	gl_particle_att_a = Cvar_Get( "gl_particle_att_a", "0.01", CVAR_ARCHIVE, "no description" );
	gl_particle_att_b = Cvar_Get( "gl_particle_att_b", "0.0", CVAR_ARCHIVE, "no description" );
	gl_particle_att_c = Cvar_Get( "gl_particle_att_c", "0.01", CVAR_ARCHIVE, "no description" );

	r_bloom = Cvar_Get( "r_bloom", "0", CVAR_ARCHIVE, "no description" );
	r_bloom_alpha = Cvar_Get( "r_bloom_alpha", "0.5", CVAR_ARCHIVE, "no description" );
	r_bloom_diamond_size = Cvar_Get( "r_bloom_diamond_size", "8", CVAR_ARCHIVE, "no description" );
	r_bloom_intensity = Cvar_Get( "r_bloom_intensity", "0.6", CVAR_ARCHIVE, "no description" );
	r_bloom_darken = Cvar_Get( "r_bloom_darken", "4", CVAR_ARCHIVE, "no description" );
	r_bloom_sample_size = Cvar_Get( "r_bloom_sample_size", "128", CVAR_ARCHIVE, "no description" );
	r_bloom_fast_sample = Cvar_Get( "r_bloom_fast_sample", "0", CVAR_ARCHIVE, "no description" );

	r_minimap_size = Cvar_Get ("r_minimap_size", "256", CVAR_ARCHIVE, "no description" );
	r_minimap_zoom = Cvar_Get ("r_minimap_zoom", "1", CVAR_ARCHIVE, "no description" );
	r_minimap_style = Cvar_Get ("r_minimap_style", "1", CVAR_ARCHIVE, "no description" );  
	r_minimap = Cvar_Get("r_minimap", "0", CVAR_ARCHIVE, "no description" ); 

	r_mirroralpha = Cvar_Get( "r_mirroralpha", "0.5", CVAR_ARCHIVE, "no description" );
	r_interpolate = Cvar_Get( "r_interpolate", "0", CVAR_ARCHIVE, "no description" );

	gl_modulate = Cvar_Get ("gl_modulate", "1", CVAR_ARCHIVE, "no description" );
	gl_log = Cvar_Get( "gl_log", "0", 0, "no description" );
	gl_bitdepth = Cvar_Get( "gl_bitdepth", "0", 0, "no description" );

	gl_lightmap = Cvar_Get ("gl_lightmap", "0", 0, "no description" );
	gl_shadows = Cvar_Get ("gl_shadows", "0", CVAR_ARCHIVE, "no description" );
	gl_dynamic = Cvar_Get ("gl_dynamic", "1", 0, "no description" );
	gl_nobind = Cvar_Get ("gl_nobind", "0", 0, "no description" );
	gl_round_down = Cvar_Get ("gl_round_down", "1", 0, "no description" );
	gl_skymip = Cvar_Get ("gl_skymip", "0", 0, "no description" );
	gl_showtris = Cvar_Get ("gl_showtris", "0", 0, "no description" );
	gl_ztrick = Cvar_Get ("gl_ztrick", "0", 0, "no description" );
	gl_finish = Cvar_Get ("gl_finish", "0", CVAR_ARCHIVE, "no description" );
	gl_clear = Cvar_Get ("gl_clear", "0", 0, "no description" );
	gl_cull = Cvar_Get ("gl_cull", "1", 0, "no description" );
	gl_polyblend = Cvar_Get ("gl_polyblend", "1", 0, "no description" );
	gl_flashblend = Cvar_Get ("gl_flashblend", "0", 0, "no description" );
	gl_playermip = Cvar_Get ("gl_playermip", "0", 0, "no description" );
	gl_texturemode = Cvar_Get( "gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE, "no description" );
	gl_texturealphamode = Cvar_Get( "gl_texturealphamode", "default", CVAR_ARCHIVE, "no description" );
	gl_texturesolidmode = Cvar_Get( "gl_texturesolidmode", "default", CVAR_ARCHIVE, "no description" );
	gl_lockpvs = Cvar_Get( "gl_lockpvs", "0", 0, "no description" );

	gl_vertex_arrays = Cvar_Get( "gl_vertex_arrays", "0", CVAR_ARCHIVE, "no description" );

	gl_ext_swapinterval = Cvar_Get( "gl_ext_swapinterval", "1", CVAR_ARCHIVE, "no description" );
	gl_ext_multitexture = Cvar_Get( "gl_ext_multitexture", "1", CVAR_ARCHIVE, "no description" );
	gl_ext_compiled_vertex_array = Cvar_Get( "gl_ext_compiled_vertex_array", "1", CVAR_ARCHIVE, "no description" );

	gl_drawbuffer = Cvar_Get( "gl_drawbuffer", "GL_BACK", 0, "no description" );
	gl_swapinterval = Cvar_Get( "gl_swapinterval", "1", CVAR_ARCHIVE, "no description" );

	gl_saturatelighting = Cvar_Get( "gl_saturatelighting", "0", 0, "no description" );

	gl_3dlabs_broken = Cvar_Get( "gl_3dlabs_broken", "1", CVAR_ARCHIVE, "no description" );

	r_fullscreen = Cvar_Get( "fullscreen", "0", CVAR_ARCHIVE, "set in 1 to enable fullscreen mode" );
	vid_gamma = Cvar_Get( "vid_gamma", "1", CVAR_ARCHIVE, "screen gamma" );

	Cmd_AddCommand( "imagelist", R_ImageList_f, "display loaded images list" );
	Cmd_AddCommand( "modellist", Mod_Modellist_f, "display loaded models list" );
	Cmd_AddCommand( "gl_strings", GL_Strings_f, "display openGL supported extensions" );
}

void GL_InitBackend( void )
{
	int	i;

	GL_InitCommands();

	glw_state.wndproc = ri.WndProc;
	glw_state.hInst = GetModuleHandle( NULL );
	r_temppool = Mem_AllocPool( "Render Memory" );
	if( !r_framebuffer ) r_framebuffer = Z_Malloc( r_width->integer * r_height->integer * 3 );

	// init tables
	for( i = 0; i < 256; i++ ) r_turbsin[i] *= 0.5f;
}

void GL_SetExtension( int r_ext, int enable )
{
	if( r_ext >= 0 && r_ext < R_EXTCOUNT )
		gl_config.extension[r_ext] = enable ? GL_TRUE : GL_FALSE;
	else MsgDev( D_ERROR, "GL_SetExtension: invalid extension %d\n", r_ext );
}

bool GL_Support( int r_ext )
{
	if( r_ext >= 0 && r_ext < R_EXTCOUNT )
		return gl_config.extension[r_ext] ? true : false;
	MsgDev( D_ERROR, "GL_Support: invalid extension %d\n", r_ext );
	return false;		
}

void *GL_GetProcAddress( const char *name )
{
	void	*p = NULL;

	if( pwglGetProcAddress != NULL )
		p = (void *)pwglGetProcAddress( name );
	if( !p )  p = (void *)Sys_GetProcAddress( &opengl_dll, name );

	return p;
}

void GL_CheckExtension( const char *name, const dllfunc_t *funcs, const char *cvarname, int r_ext )
{
	const dllfunc_t	*func;
	cvar_t		*parm;

	MsgDev( D_NOTE, "GL_CheckExtension: %s ", name );

	for( func = funcs; func && func->name; func++ )
		*func->func = NULL;

	if( cvarname )
	{
		// system config disable extensions
		parm = Cvar_Get( cvarname, "1", CVAR_SYSTEMINFO, "enable or disable gl_extension" );
		GL_SetExtension( r_ext, parm->integer );	// update render info
		if( parm->integer == 0 )
		{
			MsgDev( D_NOTE, "- disabled\n");
			return; // nothing to process at
		}
	}

	if((name[2] == '_' || name[3] == '_') && !com.strstr( gl_config.extensions_string, name ))
	{
		GL_SetExtension( r_ext, false );	// update render info
		MsgDev( D_NOTE, "- failed\n");
		return;
	}

	GL_SetExtension( r_ext, true ); // predict extension state
	for( func = funcs; func && func->name != NULL; func++ )
	{
		// functions are cleared before all the extensions are evaluated
		if(!(*func->func = (void *)GL_GetProcAddress( func->name )))
			GL_SetExtension( r_ext, false ); // one or more functions are invalid, extension will be disabled
	}
	if(GL_Support( r_ext )) MsgDev( D_NOTE, "- enabled\n");
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
	if (pglLockArraysEXT != 0)
		pglLockArraysEXT(0, count);
}

void GL_UnlockArrays( void )
{
	if (pglUnlockArraysEXT != 0)
		pglUnlockArraysEXT();
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
		pglEnable(GL_ALPHA_TEST);
		gl_state.alpha_test = true;
	}
}

void GL_DisableAlphaTest ( void )
{
	if (gl_state.alpha_test)
	{
		pglDisable(GL_ALPHA_TEST);
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
		pglEnable(GL_BLEND);
		gl_state.blend = true;
	}
}

void GL_DisableBlend( void )
{
	if (gl_state.blend)
	{
		pglDisable(GL_BLEND);
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
		pglEnable( GL_DEPTH_TEST );
		gl_state.depth_test = true;
	}
}

void GL_DisableDepthTest( void )
{
	if (gl_state.depth_test)
	{
		pglDisable( GL_DEPTH_TEST );
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
	pglEnable(GL_TEXTURE_GEN_S);
	pglEnable(GL_TEXTURE_GEN_T);
	pglEnable(GL_TEXTURE_GEN_R);
	pglEnable(GL_TEXTURE_GEN_Q);
	gl_state.texgen = true;
}

void GL_DisableTexGen( void )
{
	if (!gl_state.texgen) return;
	pglDisable(GL_TEXTURE_GEN_S);
	pglDisable(GL_TEXTURE_GEN_T);
	pglDisable(GL_TEXTURE_GEN_R);
	pglDisable(GL_TEXTURE_GEN_Q);
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
			pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
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
	if(!GL_Support( R_ARB_MULTITEXTURE ))
		return;

	if( enable )
	{
		GL_SelectTexture( GL_TEXTURE1 );
		pglEnable( GL_TEXTURE_2D );
		GL_TexEnv( GL_REPLACE );
	}
	else
	{
		GL_SelectTexture( GL_TEXTURE1 );
		pglDisable( GL_TEXTURE_2D );
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

	if(!GL_Support( R_ARB_MULTITEXTURE ))
		return;

	if( texture == GL_TEXTURE0 )
		tmu = 0;
	else tmu = 1;

	if( tmu == gl_state.currenttmu )
		return;
	gl_state.currenttmu = tmu;

	pglActiveTextureARB( texture );
	pglClientActiveTextureARB( texture );
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
		pglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode );
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
		pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
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
	pglBindTexture (GL_TEXTURE_2D, texnum);
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
	pglClearColor (1,0, 0.5 , 0.5);
	pglCullFace(GL_FRONT);
	pglEnable(GL_TEXTURE_2D);

	pglEnable(GL_ALPHA_TEST);
	pglAlphaFunc(GL_GREATER, 0.666);

	pglDisable (GL_DEPTH_TEST);
	pglDisable (GL_CULL_FACE);
	pglDisable (GL_BLEND);
	gl_state.blend = false;
	pglColor4f (1,1,1,1);

	Vector4Set(gl_state.draw_color, 1.0f, 1.0f, 1.0f, 1.0f );

	pglHint (GL_FOG_HINT, GL_FASTEST);
	pglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	pglShadeModel (GL_FLAT);
	
	GL_TextureMode( gl_texturemode->string );
	GL_TextureAlphaMode( gl_texturealphamode->string );
	GL_TextureSolidMode( gl_texturesolidmode->string );

	pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
	pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

	pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	pglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_TexEnv( GL_REPLACE );

	gl_state.texgen = false;
	pglDisable(GL_TEXTURE_GEN_S);
	pglDisable(GL_TEXTURE_GEN_T);

	if ( pglPointParameterfEXT )
	{
		float attenuations[3];

		attenuations[0] = gl_particle_att_a->value;
		attenuations[1] = gl_particle_att_b->value;
		attenuations[2] = gl_particle_att_c->value;

		pglEnable( GL_POINT_SMOOTH );
		pglPointParameterfEXT( GL_POINT_SIZE_MIN_EXT, gl_particle_min_size->value );
		pglPointParameterfEXT( GL_POINT_SIZE_MAX_EXT, gl_particle_max_size->value );
		pglPointParameterfvEXT( GL_DISTANCE_ATTENUATION_EXT, attenuations );
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

		if ( pwglSwapIntervalEXT )
			pwglSwapIntervalEXT( gl_swapinterval->value );
	}
}

void pglPerspective( GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar )
{
	GLdouble xmin, xmax, ymin, ymax;

	ymax = zNear * tan( fovy * M_PI / 360.0 );
	ymin = -ymax;
	xmin = ymin * aspect;
	xmax = ymax * aspect;

	pglFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
}

/*
================
VID_ImageAdjustGamma
================
*/
void VID_ImageAdjustGamma( byte *in, uint width, uint height )
{
	int	i, c = width * height;
	float	g = vid_gamma->value;
	byte	*p = in;

	// screenshots gamma	
	for ( i = 0; i < 256; i++ )
	{
		if ( g == 1 ) gammatable[i] = i;
		else gammatable[i] = bound(0, 255 * pow((i + 0.5)/255.5 , g ) + 0.5, 255);
	}
	for (i = 0; i < c; i++, p += 3 )
	{
		p[0] = gammatable[p[0]];
		p[1] = gammatable[p[1]];
		p[2] = gammatable[p[2]];
	}
}

bool VID_ScreenShot( const char *filename, bool levelshot )
{
	rgbdata_t 	*r_shot;

	// shared framebuffer not init
	if(!r_framebuffer) return false;

	// get screen frame
	pglReadPixels( 0, 0, r_width->integer, r_height->integer, GL_RGB, GL_UNSIGNED_BYTE, r_framebuffer );

	r_shot = Z_Malloc( sizeof(rgbdata_t));
	r_shot->width = r_width->integer;
	r_shot->height = r_height->integer;
	r_shot->type = PF_RGB_24_FLIP;
	r_shot->hint = PF_RGB_24; // save format
	r_shot->size = r_shot->width * r_shot->height * 3;
	r_shot->numLayers = 1;
	r_shot->numMips = 1;
	r_shot->palette = NULL;
	r_shot->buffer = r_framebuffer;

	if( levelshot ) Image->ResampleImage( filename, &r_shot, 512, 384, false ); // resample to 512x384
	else VID_ImageAdjustGamma( r_shot->buffer, r_shot->width, r_shot->height ); // adjust brightness

	// write image
	Image->SaveImage( filename, r_shot );
	Mem_Free( r_shot );
	return true;
}