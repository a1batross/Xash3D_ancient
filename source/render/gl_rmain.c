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
// r_main.c
#include "gl_local.h"
#include "r_mirror.h"
#include "const.h"

void R_Clear (void);

render_imp_t	ri;
stdlib_api_t	com;

byte *r_temppool;

int GL_TEXTURE0, GL_TEXTURE1;

rmodel_t *r_worldmodel;
rmodel_t *r_models[MAX_MODELS];

float gldepthmin, gldepthmax;

glconfig_t gl_config;
glstate_t  gl_state;

// vertex arrays
float tex_array[MAX_ARRAY][2];
float vert_array[MAX_ARRAY][3];
float col_array[MAX_ARRAY][4];

image_t *r_notexture; // use for bad textures
image_t *r_particletexture;// little dot for particles
image_t *r_radarmap; // wall texture for radar texgen
image_t *r_around;

rmodel_t *m_pRenderModel;
ref_entity_t *m_pCurrentEntity;

cplane_t	frustum[4];

int	r_visframecount;	// bumped when going to a new PVS
int	r_framecount;		// used for dlight push checking

int	c_brush_polys, c_studio_polys;

float	v_blend[4];			// final blending color

void GL_Strings_f( void );
byte *r_framebuffer;			// pause frame buffer
float r_pause_alpha;

//
// view origin
//
vec3_t	vup;
vec3_t	vright;
vec3_t	vforward;
vec3_t	r_origin;

float	r_world_matrix[16];
float	r_base_world_matrix[16];

//
// screen size info
//
refdef_t	r_newrefdef;	// proceeed refdef

int	r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

cvar_t	*r_check_errors;
cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_speeds;
cvar_t	*r_fullbright;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_lerpmodels;
cvar_t	*r_lefthand;
cvar_t	*r_testmode;
cvar_t	*r_hqmodels;

cvar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level
cvar_t	*r_mirroralpha;
cvar_t	*gl_nosubimage;

cvar_t	*gl_vertex_arrays;

cvar_t	*gl_particle_min_size;
cvar_t	*gl_particle_max_size;
cvar_t	*gl_particle_size;
cvar_t	*gl_particle_att_a;
cvar_t	*gl_particle_att_b;
cvar_t	*gl_particle_att_c;

// doom1\2 style map, based on GLOOM radar code
cvar_t	*r_minimap;
cvar_t	*r_minimap_size; 
cvar_t	*r_minimap_zoom;
cvar_t	*r_minimap_style;

cvar_t	*gl_ext_swapinterval;
cvar_t	*gl_ext_multitexture;
cvar_t	*gl_ext_compiled_vertex_array;
cvar_t	*r_motionblur_intens;
cvar_t	*r_motionblur;
cvar_t	*gl_log;
cvar_t	*gl_bitdepth;
cvar_t	*gl_drawbuffer;
cvar_t	*gl_lightmap;
cvar_t	*gl_shadows;
cvar_t	*gl_dynamic;
cvar_t	*gl_modulate;
cvar_t	*gl_nobind;
cvar_t	*gl_round_down;
cvar_t	*gl_skymip;
cvar_t	*gl_showtris;
cvar_t	*gl_ztrick;
cvar_t	*gl_finish;
cvar_t	*gl_clear;
cvar_t	*gl_cull;
cvar_t	*gl_polyblend;
cvar_t	*gl_flashblend;
cvar_t	*gl_playermip;
cvar_t  *gl_saturatelighting;
cvar_t	*gl_swapinterval;
cvar_t	*gl_texturemode;
cvar_t	*gl_texturealphamode;
cvar_t	*gl_texturesolidmode;
cvar_t	*gl_lockpvs;
cvar_t	*r_bloom;
cvar_t	*r_bloom_alpha;
cvar_t	*r_bloom_diamond_size;
cvar_t	*r_bloom_intensity;
cvar_t	*r_bloom_darken;
cvar_t	*r_bloom_sample_size;
cvar_t	*r_bloom_fast_sample;

cvar_t	*gl_3dlabs_broken;

cvar_t	*r_pause_bw;
cvar_t	*r_fullscreen;
cvar_t	*vid_gamma;
cvar_t	*r_pause;
cvar_t	*r_width;
cvar_t	*r_height;
cvar_t	*r_mode;

cvar_t	*r_physbdebug;
cvar_t	*r_interpolate;

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
bool R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;

	if (r_nocull->value)
		return false;

	for (i = 0; i < 4; i++)
	{
		if ( BoxOnPlaneSide(mins, maxs, &frustum[i]) == SIDE_ON )
			return true;
	}
	return false;
}


void R_RotateForEntity( ref_entity_t *e )
{
    pglTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

    pglRotatef (e->angles[1],  0, 0, 1);
    pglRotatef (-e->angles[0],  0, 1, 0);
    pglRotatef (-e->angles[2],  1, 0, 0);
}

//==================================================================================

/*
=============
R_DrawNullModel
=============
*/
void R_DrawNullModel (void)
{
	vec3_t	shadelight;
	int		i;

	if( m_pCurrentEntity->renderfx & RF_FULLBRIGHT )
		shadelight[0] = shadelight[1] = shadelight[2] = 1.0F;
	else R_LightPoint( m_pCurrentEntity->origin, shadelight );

	pglPushMatrix ();
	R_RotateForEntity( m_pCurrentEntity );

	pglDisable (GL_TEXTURE_2D);
	pglColor3fv (shadelight);

	pglBegin (GL_TRIANGLE_FAN);
	pglVertex3f (0, 0, -16);
	for (i=0 ; i<=4 ; i++)
		pglVertex3f (16*cos(i*M_PI/2), 16*sin(i*M_PI/2), 0);
	pglEnd ();

	pglBegin (GL_TRIANGLE_FAN);
	pglVertex3f (0, 0, 16);
	for (i=4 ; i>=0 ; i--)
		pglVertex3f (16*cos(i*M_PI/2), 16*sin(i*M_PI/2), 0);
	pglEnd ();

	pglColor3f (1,1,1);
	pglPopMatrix ();
	pglEnable (GL_TEXTURE_2D);
}

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities->value) return;

	// draw non-transparent first
	for (i = 0; i < r_newrefdef.num_entities; i++)
	{
		m_pCurrentEntity = &r_newrefdef.entities[i];
		m_pRenderModel = m_pCurrentEntity->model;

		if (!m_pRenderModel)
		{
			R_DrawNullModel();
			continue;
		}
		switch (m_pRenderModel->type)
		{
		case mod_brush:
			R_DrawBrushModel( RENDERPASS_SOLID );
			break;
		case mod_sprite:
			R_DrawSpriteModel( RENDERPASS_SOLID );
			break;
		case mod_studio:
			R_DrawStudioModel( RENDERPASS_SOLID );
			break;
		default:
			Host_Error ("Bad modeltype. Pass: solid");
			break;
		}
	}

	// draw transparent entities
	// we could sort these if it ever becomes a problem...
	pglDepthMask (0); // no z writes
	for (i = 0; i < r_newrefdef.num_entities; i++)
	{
		m_pCurrentEntity = &r_newrefdef.entities[i];
		m_pRenderModel = m_pCurrentEntity->model;

		if (!m_pRenderModel)
		{
			R_DrawNullModel ();
			continue;
		}
		switch (m_pRenderModel->type)
		{
		case mod_brush:
			R_DrawBrushModel( RENDERPASS_ALPHA );
			break;
		case mod_sprite:
			R_DrawSpriteModel( RENDERPASS_ALPHA );
			break;
		case mod_studio:
			R_DrawStudioModel( RENDERPASS_ALPHA );
			break;
		default:
			Host_Error ("Bad modeltype. Pass: alpha");
			break;
		}
	}
	pglDepthMask(1);// back to writing

}

/*
** GL_DrawParticles
**
*/
void GL_DrawParticles( int num_particles, const particle_t particles[], const unsigned colortable[768] )
{
	const particle_t *p;
	int				i;
	vec3_t			up, right;
	float			scale;
	byte			color[4];

    GL_Bind(r_particletexture->texnum[0]);
	pglDepthMask( GL_FALSE );		// no z buffering
	pglEnable( GL_BLEND );
	GL_TexEnv( GL_MODULATE );
	pglBegin( GL_TRIANGLES );

	VectorScale (vup, 1.5, up);
	VectorScale (vright, 1.5, right);

	for ( p = particles, i=0 ; i < num_particles ; i++,p++)
	{
		// hack a scale up to keep particles from disapearing
		scale = ( p->origin[0] - r_origin[0] ) * vforward[0] + 
			    ( p->origin[1] - r_origin[1] ) * vforward[1] +
			    ( p->origin[2] - r_origin[2] ) * vforward[2];

		if (scale < 20)
			scale = 1;
		else
			scale = 1 + scale * 0.004;

		*(int *)color = colortable[p->color];
		color[3] = p->alpha*255;

		pglColor4ubv( color );

		pglTexCoord2f( 0.0625, 0.0625 );
		pglVertex3fv( p->origin );

		pglTexCoord2f( 1.0625, 0.0625 );
		pglVertex3f( p->origin[0] + up[0]*scale, 
			         p->origin[1] + up[1]*scale, 
					 p->origin[2] + up[2]*scale);

		pglTexCoord2f( 0.0625, 1.0625 );
		pglVertex3f( p->origin[0] + right[0]*scale, 
			         p->origin[1] + right[1]*scale, 
					 p->origin[2] + right[2]*scale);
	}

	pglEnd ();
	pglDisable( GL_BLEND );
	pglColor4f( 1,1,1,1 );
	pglDepthMask( 1 );		// back to normal Z buffering
	GL_TexEnv( GL_REPLACE );
}

/*
===============
R_DrawParticles
===============
*/
void R_DrawParticles (void)
{
	if( pglPointParameterfEXT )
	{
		int i;
		unsigned char color[4];
		const particle_t *p;

		pglDepthMask( GL_FALSE );
		pglEnable( GL_BLEND );
		pglDisable( GL_TEXTURE_2D );

		pglPointSize( gl_particle_size->value );

		pglBegin( GL_POINTS );
		for ( i = 0, p = r_newrefdef.particles; i < r_newrefdef.num_particles; i++, p++ )
		{
			*(int *)color = d_8to24table[p->color];
			color[3] = p->alpha*255;

			pglColor4ubv( color );

			pglVertex3fv( p->origin );
		}
		pglEnd();

		pglDisable( GL_BLEND );
		pglColor4f( 1.0F, 1.0F, 1.0F, 1.0F );
		pglDepthMask( GL_TRUE );
		pglEnable( GL_TEXTURE_2D );

	}
	else
	{
		GL_DrawParticles( r_newrefdef.num_particles, r_newrefdef.particles, d_8to24table );
	}
}

/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
}

//=======================================================================

int SignbitsForPlane (cplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}


void R_SetFrustum (void)
{
	int		i;

#if 0
	/*
	** this code is wrong, since it presume a 90 degree FOV both in the
	** horizontal and vertical plane
	*/
	// front side is visible
	VectorAdd (vforward, vright, frustum[0].normal);
	VectorSubtract (vforward, vright, frustum[1].normal);
	VectorAdd (vforward, vup, frustum[2].normal);
	VectorSubtract (vforward, vup, frustum[3].normal);

	// we theoretically don't need to normalize these vectors, but I do it
	// anyway so that debugging is a little easier
	VectorNormalize( frustum[0].normal );
	VectorNormalize( frustum[1].normal );
	VectorNormalize( frustum[2].normal );
	VectorNormalize( frustum[3].normal );
#else
	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector( frustum[0].normal, vup, vforward, -(90-r_newrefdef.fov_x / 2 ) );
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector( frustum[1].normal, vup, vforward, 90-r_newrefdef.fov_x / 2 );
	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector( frustum[2].normal, vright, vforward, 90-r_newrefdef.fov_y / 2 );
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector( frustum[3].normal, vright, vforward, -( 90 - r_newrefdef.fov_y / 2 ) );
#endif

	for (i = 0; i < 4; i++)
	{
		frustum[i].type = 3;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}

//=======================================================================

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame( void )
{
	int i;
	mleaf_t	*leaf;

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_newrefdef.vieworg, r_origin);

	AngleVectors(r_newrefdef.viewangles, vforward, vright, vup);

	// current viewcluster
	if (!( r_newrefdef.rdflags & RDF_NOWORLDMODEL ))
	{
		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		leaf = Mod_PointInLeaf (r_origin, r_worldmodel);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		// check above and below so crossing solid water doesn't draw wrong
		if (!leaf->contents)
		{	// look down a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] -= 16;
			leaf = Mod_PointInLeaf (temp, r_worldmodel);
			if ( !(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
		else
		{	// look up a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] += 16;
			leaf = Mod_PointInLeaf (temp, r_worldmodel);
			if ( !(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != r_viewcluster2) )
				r_viewcluster2 = leaf->cluster;
		}
	}

	for (i=0 ; i<4 ; i++)
		v_blend[i] = r_newrefdef.blend[i];

	c_brush_polys = 0;
	c_studio_polys = 0;
	numRadarEnts = 0;

	// clear out the portion of the screen that the NOWORLDMODEL defines
	if ( r_newrefdef.rdflags & RDF_NOWORLDMODEL )
	{
		pglEnable( GL_SCISSOR_TEST );
		pglClearColor( 0.3f, 0.3f, 0.3f, 1 );
		pglScissor( r_newrefdef.rect.x, r_height->integer - r_newrefdef.rect.height - r_newrefdef.rect.y, r_newrefdef.rect.width, r_newrefdef.rect.height );
		pglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		pglClearColor( 1, 0, 0.5, 0.5 );
		pglDisable( GL_SCISSOR_TEST );
	}
}

/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
//	float	yfov;
	int		x, x2, y2, y, w, h;

	//
	// set up viewport
	//
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();
	x = floor(r_newrefdef.rect.x * r_width->integer / r_width->integer);
	x2 = ceil((r_newrefdef.rect.x + r_newrefdef.rect.width) * r_width->integer / r_width->integer);
	y = floor(r_height->integer - r_newrefdef.rect.y * r_height->integer / r_height->integer);
	y2 = ceil(r_height->integer - (r_newrefdef.rect.y + r_newrefdef.rect.height) * r_height->integer / r_height->integer);

	w = x2 - x;
	h = y - y2;

	if( mirror ) Mirror_Scale();
	else pglCullFace(GL_FRONT);
	pglViewport (x, y2, w, h);

	//
	// set up projection matrix
	//
	screenaspect = (float)r_newrefdef.rect.width/r_newrefdef.rect.height;
//	yfov = 2*atan((float)r_newrefdef.rect.height/r_newrefdef.rect.width)*180/M_PI;

	pglPerspective (r_newrefdef.fov_y,  screenaspect,  4,  131072 );

	pglMatrixMode(GL_MODELVIEW);
	pglLoadIdentity ();

    pglRotatef (-90,  1, 0, 0);	    // put Z going up
    pglRotatef (90,  0, 0, 1);	    // put Z going up
    pglRotatef (-r_newrefdef.viewangles[2],  1, 0, 0);
    pglRotatef (-r_newrefdef.viewangles[0],  0, 1, 0);
    pglRotatef (-r_newrefdef.viewangles[1],  0, 0, 1);
    pglTranslatef (-r_newrefdef.vieworg[0],  -r_newrefdef.vieworg[1],  -r_newrefdef.vieworg[2]);

	pglGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	//
	// set drawing parms
	//
	if (gl_cull->value)
		pglEnable(GL_CULL_FACE);
	else pglDisable(GL_CULL_FACE);

	pglDisable(GL_BLEND);
	pglDisable(GL_ALPHA_TEST);
	pglEnable(GL_DEPTH_TEST);
}

/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	if (gl_ztrick->value)
	{
		static int trickframe;

		if (gl_clear->value)
		{
			pglClearColor( 0.5, 0.5, 0.5, 1 );
			pglClear (GL_COLOR_BUFFER_BIT);
		}
		trickframe++;
		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999f;
			pglDepthFunc (GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5f;
			pglDepthFunc (GL_GEQUAL);
		}
	}
	else
	{
		if (gl_clear->value)
		{
			pglClearColor( 0.5, 0.5, 0.5, 1 );
			pglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}
		else pglClear (GL_DEPTH_BUFFER_BIT);

		gldepthmin = 0;
		if( r_mirroralpha->value < 1.0 )
			gldepthmax = 0.5;
		else gldepthmax = 1;
		pglDepthFunc (GL_LEQUAL);
	}
	pglDepthRange (gldepthmin, gldepthmax);
}

void R_Flash( void )
{
	R_PolyBlend ();
}

static void R_DrawLine( int color, int numpoints, const float *points )
{
	int	i = numpoints - 1;
	vec3_t	p0, p1;

	VectorSet( p0, points[i*3+0], points[i*3+1], points[i*3+2] );
	if( r_physbdebug->integer == 1 ) ConvertPositionToGame( p0 );

	for (i = 0; i < numpoints; i ++)
	{
		VectorSet( p1, points[i*3+0], points[i*3+1], points[i*3+2] );
		if( r_physbdebug->integer == 1 ) ConvertPositionToGame( p1 );
 
		pglColor4fv(UnpackRGBA( color ));
		pglVertex3fv( p0 );
		pglVertex3fv( p1 );
 
 		VectorCopy( p1, p0 );
 	}
}

void R_DebugGraphics( void )
{
	if(r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	if(!r_physbdebug->integer)
		return;

	// physic debug
	pglDisable(GL_TEXTURE_2D); 
	GL_PolygonOffset( -1, 1 );
	pglBegin( GL_LINES );

	ri.ShowCollision( R_DrawLine );

	pglEnd();
	GL_PolygonOffset( 0, 0 );
	pglEnable(GL_TEXTURE_2D);
}

/*
================
R_RenderView

r_newrefdef must be set before the first call
================
*/
void R_RenderView( refdef_t *fd )
{
	if (r_norefresh->value)
		return;

	r_newrefdef = *fd;

	if (!r_worldmodel && !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
		Host_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds->value)
	{
		c_brush_polys = 0;
		c_studio_polys = 0;
	}
	R_PushDlights ();

	if (gl_finish->value) pglFinish ();

	R_SetupFrame ();
	R_SetFrustum ();
	R_SetupGL ();
	R_MarkLeaves ();	// done here so we know if we're in water
	R_DrawWorld ();
	R_DrawEntitiesOnList ();
	R_RenderDlights ();
	R_DrawParticles ();
	R_DrawAlphaSurfaces ();
	R_DebugGraphics();
	R_Flash();

	R_BloomBlend (fd);
}

void R_DrawPauseScreen( void )
{
	// don't apply post effects for custom window
	if(r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	if(!r_pause_bw->integer )
		return;

	if(r_pause->modified )
	{
		// reset saturation value
		if(!r_pause->value)
			r_pause_alpha = 0.0f;
		r_pause->modified = false;
	}
	if(!r_pause->value) return;          
	if( r_pause_alpha < 1.0f ) r_pause_alpha += 0.03f;

	if( r_pause_alpha <= 1.0f || r_lefthand->modified )
	{
		int	k = r_pause_alpha * 255.0f;
		int	i, s, r, g, b;

		pglFlush();
		pglReadPixels(0, 0, r_width->integer, r_height->integer, GL_RGB, GL_UNSIGNED_BYTE, r_framebuffer);
		for (i = 0; i < r_width->integer * r_height->integer * 3; i+=3)
		{
			r = r_framebuffer[i+0];
			g = r_framebuffer[i+1];
			b = r_framebuffer[i+2];
			s = (r + 2 * g + b) * k>>2; // simply bw recomputing
			r_framebuffer[i+0] = (r*(255-k)+s)>>8;
			r_framebuffer[i+1] = (g*(255-k)+s)>>8;
			r_framebuffer[i+2] = (b*(255-k)+s)>>8;
		}
		r_lefthand->modified = false;
	}
	// set custom orthogonal mode
	pglMatrixMode(GL_PROJECTION);
	pglLoadIdentity ();
	pglOrtho(0, r_width->integer, 0, r_height->integer, 0, 1.0f);
	pglMatrixMode(GL_MODELVIEW);
	pglLoadIdentity ();

	pglDisable(GL_TEXTURE_2D);
	pglRasterPos2f(0, 0);
	pglDrawPixels(r_width->integer, r_height->integer, GL_RGB, GL_UNSIGNED_BYTE, r_framebuffer);
	pglFlush();
	pglEnable(GL_TEXTURE_2D);
}

int R_DrawRSpeeds(char *S)
{
	return sprintf(S, "%4i wpoly %4i epoly %i tex %i lmaps", c_brush_polys, c_studio_polys, c_visible_textures, c_visible_lightmaps );
}

dword blurtex = 0;
dword blur_shader = 0;

void R_SetGL2D( void )
{
	R_DrawPauseScreen();
		
	// set 2D virtual screen size
	pglViewport (0,0, r_width->integer, r_height->integer);
	pglMatrixMode(GL_PROJECTION);
	pglLoadIdentity ();
	pglOrtho(0, r_width->integer, r_height->integer, 0, -99999, 99999);
	pglMatrixMode(GL_MODELVIEW);
	pglLoadIdentity ();
	pglDisable(GL_DEPTH_TEST);
	pglDisable(GL_CULL_FACE);

	GL_DisableAlphaTest();
	GL_EnableBlend();

	if(!r_motionblur->value && v_blend[3])
	{	
		pglDisable (GL_TEXTURE_2D);
		pglColor4fv (v_blend);

		VA_SetElem2(vert_array[0], 0, 0);
		VA_SetElem2(vert_array[1], r_width->integer, 0);
		VA_SetElem2(vert_array[2], r_width->integer, r_height->integer);
		VA_SetElem2(vert_array[3], 0, r_height->integer);

		GL_LockArrays( 4 );
		pglDrawArrays(GL_QUADS, 0, 4);
		GL_UnlockArrays();

		pglColor4f (1,1,1,1);
		pglEnable (GL_TEXTURE_2D);
	}
	else if (r_motionblur->value && (r_newrefdef.rdflags & RDF_PAIN))
	{
		if( !gl_config.texRectangle ) return;
		if( blurtex )
		{
	     		GL_TexEnv(GL_MODULATE);
			pglDisable(GL_TEXTURE_2D);
			
			pglEnable( gl_config.texRectangle );
			pglDisable (GL_ALPHA_TEST);
			pglEnable (GL_BLEND);
			pglBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
			
			if(r_newrefdef.rdflags & (RDF_PAIN))
				pglColor4f (1, 0, 0, r_motionblur_intens->value);

			pglBegin(GL_QUADS);
			pglTexCoord2f(0,r_height->integer);
			pglVertex2f(0,0);
			pglTexCoord2f(r_width->integer,r_height->integer);
			pglVertex2f(r_width->integer,0);
			pglTexCoord2f(r_width->integer,0);
			pglVertex2f(r_width->integer,r_height->integer);
			pglTexCoord2f(0,0);
			pglVertex2f(0,r_height->integer);
			pglEnd();
  
			pglDisable( gl_config.texRectangle );
			pglEnable( GL_TEXTURE_2D );
		}
		if(!blurtex) pglGenTextures(1,&blurtex);
		pglBindTexture( gl_config.texRectangle, blurtex );
		pglCopyTexImage2D( gl_config.texRectangle, 0, GL_RGB, 0, 0, r_width->integer, r_height->integer, 0 );
		pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	} 

	GL_DisableBlend();
	GL_EnableAlphaTest();
	pglColor4f (1,1,1,1);

	if (r_speeds->value)
	{
		char	S[128];
		int	n = 0;
		int	i;

		n = R_DrawRSpeeds(S);
		pglColor4f(1, 0, 1, 1);
		for (i = 0; i < n; i++)
			Draw_Char(r_newrefdef.rect.width - 60 + ((i - n) * 8), r_newrefdef.rect.height - 60, 128 + S[i]);
		pglColor4f (1, 1, 1, 1);
	} 

}

/*
====================
R_SetLightLevel

====================
*/
void R_SetLightLevel (void)
{
	vec3_t		shadelight;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	// save off light value for server to look at (BIG HACK!)

	R_LightPoint (r_newrefdef.vieworg, shadelight);

	// pick the greatest component, which should be the same
	// as the mono value returned by software
	if (shadelight[0] > shadelight[1])
	{
		if (shadelight[0] > shadelight[2])
			r_lightlevel->value = 150*shadelight[0];
		else
			r_lightlevel->value = 150*shadelight[2];
	}
	else
	{
		if (shadelight[1] > shadelight[2])
			r_lightlevel->value = 150*shadelight[1];
		else
			r_lightlevel->value = 150*shadelight[2];
	}

}

static bool R_AddEntityToScene( refdef_t *fd, entity_state_t *s1, entity_state_t *s2, float lerpfrac )
{
	ref_entity_t	*refent;
	int		i, max_edicts = Cvar_VariableValue( "prvm_maxedicts" );

	if( !fd || !fd->entities ) return false; // not init
	if( !s1 || !s1->model.index ) return false; // if set to invisible, skip
	
	if( fd->num_entities >= max_edicts )
		return false;

	refent = &fd->entities[fd->num_entities];
	if( !s2 ) s2 = s1; // no lerping state

	// copy state to render
	refent->frame = s1->model.frame;
	refent->index = s1->number;
	refent->ent_type = s1->ed_type;
	refent->backlerp = 1.0f - lerpfrac;
	refent->renderamt = s1->renderamt;
	refent->body = s1->model.body;
	refent->sequence = s1->model.sequence;		
	refent->movetype = s1->movetype;
	refent->scale = s1->model.scale ? s1->model.scale : 1.0f;
	refent->colormap = s1->model.colormap;
	refent->framerate = s1->model.framerate;
	refent->effects = s1->effects;
	refent->animtime = s1->model.animtime;
	VectorCopy( s1->rendercolor, refent->rendercolor );

	// setup latchedvars
	refent->prev.frame = s2->model.frame;
	refent->prev.animtime = s2->model.animtime;
	VectorCopy( s2->origin, refent->prev.origin );
	VectorCopy( s2->angles, refent->prev.angles );
	refent->prev.sequence = s2->model.sequence;
		
	// interpolate origin
	for( i = 0; i < 3; i++ )
		refent->origin[i] = LerpPoint( s2->origin[i], s1->origin[i], lerpfrac );

	// set skin
	refent->skin = s1->model.skin;
	refent->model = r_models[s1->model.index];
	refent->weaponmodel = r_models[s1->pmodel.index];
	refent->renderfx = s1->renderfx;
	refent->prev.sequencetime = s1->model.animtime - s2->model.animtime;

	// calculate angles
	if( refent->effects & EF_ROTATE )
	{	
		// some bonus items auto-rotate
		VectorSet( refent->angles, 0, anglemod(fd->time / 10), 0 );
	}
	else
	{	
		// interpolate angles
		for( i = 0; i < 3; i++ )
			refent->angles[i] = LerpAngle( s2->angles[i], s1->angles[i], lerpfrac );
	}

	// copy controllers
	for( i = 0; i < MAXSTUDIOCONTROLLERS; i++ )
	{
		refent->controller[i] = s1->model.controller[i];
		refent->prev.controller[i] = s2->model.controller[i];
	}

	// copy blends
	for( i = 0; i < MAXSTUDIOBLENDS; i++ )
	{
		refent->blending[i] = s1->model.blending[i];
		refent->prev.blending[i] = s2->model.blending[i];
	}

	if( refent->ent_type == ED_CLIENT )
	{
		// only draw from mirrors
		refent->renderfx |= RF_PLAYERMODEL;
		refent->gaitsequence = s1->model.gaitsequence;
	}

	// add entity
	fd->num_entities++;
	r_newrefdef = *fd;

	return true;
}

static bool R_AddParticleToChain( refdef_t *fd, vec3_t org, float alpha, int color )
{
	particle_t	*p;

	if( !fd || !fd->particles ) return false;
	if( fd->num_particles >= MAX_PARTICLES)
		return false;

	p = &fd->particles[fd->num_particles];
	VectorCopy( org, p->origin );
	p->color = color;
	p->alpha = alpha;
	fd->num_particles++;
	r_newrefdef = *fd;

	return true;
}

static bool R_AddLightStyle( refdef_t *fd, int style, vec3_t color )
{
	lightstyle_t	*ls;

	if( !fd || !fd->lightstyles ) return false;
	if( style < 0 || style > MAX_LIGHTSTYLES )
		return false; // invalid lightstyle

	ls = &fd->lightstyles[style];
	ls->white = color[0] + color[1] + color[2];
	VectorCopy( color, ls->rgb );
	r_newrefdef = *fd;

	return true;
}

static bool R_AddDynamicLight( refdef_t *fd, vec3_t org, vec3_t color, float intensity )
{
	dlight_t	*dl;

	if( !fd || !fd->dlights ) return false;
	if( fd->num_dlights >= MAX_DLIGHTS )
		return false;

	dl = &fd->dlights[fd->num_dlights];
	VectorCopy( org, dl->origin );
	VectorCopy( color, dl->color );
	dl->intensity = intensity;
	fd->num_dlights++;
	r_newrefdef = *fd;

	return true;
}


/*
@@@@@@@@@@@@@@@@@@@@@
R_RenderFrame

@@@@@@@@@@@@@@@@@@@@@
*/
void R_RenderFrame( refdef_t *fd )
{
	r_mirroralpha->value = bound( 0.0f, r_mirroralpha->value, 1.0f );	
	mirror = false;
	mirror_render = false;

	R_RenderView( fd );
	if( mirror ) R_Mirror( fd );
	GL_DrawRadar( fd );
	R_SetLightLevel();
	R_SetGL2D();
}

/*
===============
R_Init
===============
*/
int R_Init( void )
{	
	GL_InitBackend();

	// create the window and set up the context
	if(!R_Init_OpenGL())
	{
		R_Free_OpenGL();
		return false;
	}

	GL_InitExtensions();

	// vertex arrays (get rid of this)
	pglEnableClientState( GL_VERTEX_ARRAY );
	pglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	pglTexCoordPointer( 2, GL_FLOAT, sizeof(tex_array[0]), tex_array[0] );
	pglVertexPointer( 3, GL_FLOAT, sizeof(vert_array[0]),vert_array[0] );
	pglColorPointer( 4, GL_FLOAT, sizeof(col_array[0]), col_array[0] );

	GL_SetDefaultState();
	R_InitTextures();
	Mod_Init();
	R_InitParticleTexture();
	Draw_InitLocal();
          R_StudioInit();
	R_CheckForErrors();

	return true;
}

/*
===============
R_Shutdown
===============
*/
void R_Shutdown( void )
{	
	Mod_FreeAll ();
          R_StudioShutdown();
	R_ShutdownTextures();
	GL_ShutdownBackend();

	// shut down OS specific OpenGL stuff like contexts, etc.
	R_Free_OpenGL();
}



/*
@@@@@@@@@@@@@@@@@@@@@
R_BeginFrame
@@@@@@@@@@@@@@@@@@@@@
*/
void R_BeginFrame( void )
{
	if( vid_gamma->modified )
	{
		vid_gamma->modified = false;
		GL_UpdateGammaRamp();
	}

	// go into 2D mode
	pglDrawBuffer( GL_BACK );
	pglViewport( 0, 0, r_width->integer, r_height->integer );
	pglMatrixMode( GL_PROJECTION );
	pglLoadIdentity();
	pglOrtho ( 0, r_width->integer, r_height->integer, 0, -99999, 99999 );
	pglMatrixMode( GL_MODELVIEW );
	pglLoadIdentity();
	pglDisable( GL_DEPTH_TEST );
	pglDisable( GL_CULL_FACE );
	GL_DisableBlend();
	GL_EnableAlphaTest();
	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	/*
	** draw buffer stuff
	*/
	if ( gl_drawbuffer->modified )
	{
		gl_drawbuffer->modified = false;
	}

	/*
	** texturemode stuff
	*/
	if ( gl_texturemode->modified )
	{
		GL_TextureMode( gl_texturemode->string );
		gl_texturemode->modified = false;
	}

	if ( gl_texturealphamode->modified )
	{
		GL_TextureAlphaMode( gl_texturealphamode->string );
		gl_texturealphamode->modified = false;
	}

	if ( gl_texturesolidmode->modified )
	{
		GL_TextureSolidMode( gl_texturesolidmode->string );
		gl_texturesolidmode->modified = false;
	}

	/*
	** swapinterval stuff
	*/
	GL_UpdateSwapInterval();

	//
	// clear screen if desired
	//
	R_Clear ();
}

void R_EndFrame( void )
{
	if( stricmp( gl_drawbuffer->string, "GL_BACK" ) == 0 )
	{
		if( !pwglSwapBuffers( glw_state.hDC ) )
			Host_Error("GLimp_EndFrame() - SwapBuffers() failed!\n" );
	}
}

void R_ClearScene( refdef_t *fd )
{
	if( !fd ) return;

	if( !fd->mempool )
	{
		// init arrays
		if( fd->rdflags & RDF_NOWORLDMODEL )
		{
			// menu viewport not needs to many objects
			fd->mempool = Mem_AllocPool( "Viewport Zone" );
			fd->entities = (ref_entity_t *)Mem_Alloc( fd->mempool, sizeof(ref_entity_t) * 32 );
			fd->particles = (particle_t *)Mem_Alloc( fd->mempool, sizeof(particle_t) * 512 );
			fd->dlights = (dlight_t *)Mem_Alloc( fd->mempool, sizeof(dlight_t) * 4 );
		}
		else
		{
			fd->mempool = Mem_AllocPool("Refdef Zone");
			fd->entities = (ref_entity_t *)Mem_Alloc( fd->mempool, sizeof(ref_entity_t) * Cvar_VariableValue("prvm_maxedicts"));
			fd->particles = (particle_t *)Mem_Alloc( fd->mempool, sizeof(particle_t) * MAX_PARTICLES );
			fd->lightstyles = (lightstyle_t *)Mem_Alloc( fd->mempool, sizeof(lightstyle_t) * MAX_LIGHTSTYLES );	
			fd->dlights = (dlight_t *)Mem_Alloc( fd->mempool, sizeof(dlight_t) * MAX_DLIGHTS );
		}
	}	

	// clear scene
	fd->num_dlights = 0;
	fd->num_entities = 0;
	fd->num_particles = 0;
	r_newrefdef = *fd;
}

/*
=============
R_SetPalette
=============
*/
uint r_rawpalette[256];

void R_SetPalette ( const byte *palette)
{
	int		i;

	byte *rp = (byte *)r_rawpalette;

	if ( palette )
	{
		for ( i = 0; i < 256; i++ )
		{
			rp[i*4+0] = palette[i*3+0];
			rp[i*4+1] = palette[i*3+1];
			rp[i*4+2] = palette[i*3+2];
			rp[i*4+3] = 0xff;
		}
	}
	else
	{
		for ( i = 0; i < 256; i++ )
		{
			rp[i*4+0] = ( d_8to24table[i] >> 0 ) & 0xff;
			rp[i*4+1] = ( d_8to24table[i] >> 8 ) & 0xff;
			rp[i*4+2] = ( d_8to24table[i] >> 16) & 0xff;
			rp[i*4+3] = 0xff;
		}
	}

	pglClearColor (0,0,0,0);
	pglClear (GL_COLOR_BUFFER_BIT);
	pglClearColor (1, 0, 0.5, 0.5);
}

//===================================================================


bool R_UploadModel( const char *name, int index )
{
	rmodel_t	*mod;

	// this array used by AddEntityToScene
	mod = R_RegisterModel( name );
	r_models[index] = mod;

	return (mod != NULL);	
}

bool R_UploadImage( const char *name, int index )
{
	string	filename;
	image_t	*texture;

	// nothing to load
	if( !r_worldmodel ) 
		return false;
	loadmodel = r_worldmodel;

	com.strncpy( filename, Mod_GetStringFromTable( loadmodel->texinfo[index].texid ), sizeof(filename));
	texture = R_FindImage( filename, NULL, 0, it_wall );
	loadmodel->texinfo[index].image = texture;

	return true;
}

bool R_PrecachePic( const char *name )
{
	image_t *pic = Draw_FindPic( name );

	if( !pic || pic == r_notexture )
		return false;
	return true;	
}

/*
@@@@@@@@@@@@@@@@@@@@@
CreateAPI

@@@@@@@@@@@@@@@@@@@@@
*/
render_exp_t DLLEXPORT *CreateAPI(stdlib_api_t *input, render_imp_t *engfuncs )
{
	static render_exp_t re;

	com = *input;
	// Sys_LoadLibrary can create fake instance, to check
	// api version and api size, but second argument will be 0
	// and always make exception, run simply check for avoid it
	if( engfuncs ) ri = *engfuncs;

	// generic functions
	re.api_size = sizeof(render_exp_t);

	re.Init = R_Init;
	re.Shutdown = R_Shutdown;
	
	re.BeginRegistration = R_BeginRegistration;
	re.RegisterModel = R_UploadModel;
	re.RegisterImage = R_UploadImage;
	re.PrecacheImage = R_PrecachePic;
	re.SetSky = R_SetSky;
	re.EndRegistration = R_EndRegistration;

	re.AddLightStyle = R_AddLightStyle;
	re.AddRefEntity = R_AddEntityToScene;
	re.AddDynLight = R_AddDynamicLight;
	re.AddParticle = R_AddParticleToChain;
	re.ClearScene = R_ClearScene;

	re.BeginFrame = R_BeginFrame;
	re.RenderFrame = R_RenderFrame;
	re.EndFrame = R_EndFrame;

	re.SetColor = GL_SetColor;
	re.ScrShot = VID_ScreenShot;
	re.DrawFill = Draw_Fill;
	re.DrawStretchRaw = Draw_StretchRaw;
	re.DrawStretchPic = Draw_StretchPic;

	// get rid of this
	re.DrawGetPicSize = Draw_GetPicSize;

	return &re;
}