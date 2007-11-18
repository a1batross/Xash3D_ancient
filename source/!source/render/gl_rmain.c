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

void R_Clear (void);

viddef_t	vid;

render_imp_t	ri;
stdlib_api_t	std;

byte *r_temppool;

int GL_TEXTURE0, GL_TEXTURE1;

model_t *r_worldmodel;

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

entity_t	*currententity;
model_t	*currentmodel;

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
refdef_t	r_newrefdef;

int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_speeds;
cvar_t	*r_fullbright;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_lerpmodels;
cvar_t	*r_lefthand;
cvar_t	*r_loading;

cvar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level
cvar_t	*r_emboss_bump;

cvar_t	*gl_nosubimage;
cvar_t	*gl_allow_software;

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
cvar_t	*gl_ext_palettedtexture;
cvar_t	*gl_ext_multitexture;
cvar_t	*gl_ext_pointparameters;
cvar_t	*gl_ext_compiled_vertex_array;
cvar_t	*r_motionblur_intens;
cvar_t	*r_motionblur;
cvar_t	*gl_log;
cvar_t	*gl_bitdepth;
cvar_t	*gl_drawbuffer;
cvar_t	*gl_lightmap;
cvar_t	*gl_shadows;
cvar_t	*gl_mode;
cvar_t	*gl_dynamic;
cvar_t	*gl_modulate;
cvar_t	*gl_nobind;
cvar_t	*gl_round_down;
cvar_t	*gl_picmip;
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

cvar_t	*r_fullscreen;
cvar_t	*vid_gamma;
cvar_t	*r_pause;

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
		if ( BoxOnPlaneSide(mins, maxs, &frustum[i]) == SIDE_ON)
			return true;
	}
	return false;
}


void R_RotateForEntity (entity_t *e)
{
    qglTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

    qglRotatef (e->angles[1],  0, 0, 1);
    qglRotatef (-e->angles[0],  0, 1, 0);
    qglRotatef (-e->angles[2],  1, 0, 0);
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

	if ( currententity->flags & RF_FULLBRIGHT )
		shadelight[0] = shadelight[1] = shadelight[2] = 1.0F;
	else
		R_LightPoint (currententity->origin, shadelight);

    qglPushMatrix ();
	R_RotateForEntity (currententity);

	qglDisable (GL_TEXTURE_2D);
	qglColor3fv (shadelight);

	qglBegin (GL_TRIANGLE_FAN);
	qglVertex3f (0, 0, -16);
	for (i=0 ; i<=4 ; i++)
		qglVertex3f (16*cos(i*M_PI/2), 16*sin(i*M_PI/2), 0);
	qglEnd ();

	qglBegin (GL_TRIANGLE_FAN);
	qglVertex3f (0, 0, 16);
	for (i=4 ; i>=0 ; i--)
		qglVertex3f (16*cos(i*M_PI/2), 16*sin(i*M_PI/2), 0);
	qglEnd ();

	qglColor3f (1,1,1);
	qglPopMatrix ();
	qglEnable (GL_TEXTURE_2D);
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
		currententity = &r_newrefdef.entities[i];

		if ( currententity->flags & RF_BEAM )
		{
			R_DrawBeam( currententity );
		}
		else
		{
			currentmodel = currententity->model;
			if (!currentmodel)
			{
				R_DrawNullModel ();
				continue;
			}
			switch (currentmodel->type)
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
				Sys_Error ("Bad modeltype. Pass: solid");
				break;
			}
		}
	}

	// draw transparent entities
	// we could sort these if it ever becomes a problem...
	qglDepthMask (0); // no z writes
	for (i = 0; i < r_newrefdef.num_entities; i++)
	{
		currententity = &r_newrefdef.entities[i];

		if ( currententity->flags & RF_BEAM )
		{
			R_DrawBeam( currententity );
		}
		else
		{
			currentmodel = currententity->model;

			if (!currentmodel)
			{
				R_DrawNullModel ();
				continue;
			}
			switch (currentmodel->type)
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
				Sys_Error ("Bad modeltype. Pass: alpha");
				break;
			}
		}
	}
	qglDepthMask(1);// back to writing

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
	qglDepthMask( GL_FALSE );		// no z buffering
	qglEnable( GL_BLEND );
	GL_TexEnv( GL_MODULATE );
	qglBegin( GL_TRIANGLES );

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

		qglColor4ubv( color );

		qglTexCoord2f( 0.0625, 0.0625 );
		qglVertex3fv( p->origin );

		qglTexCoord2f( 1.0625, 0.0625 );
		qglVertex3f( p->origin[0] + up[0]*scale, 
			         p->origin[1] + up[1]*scale, 
					 p->origin[2] + up[2]*scale);

		qglTexCoord2f( 0.0625, 1.0625 );
		qglVertex3f( p->origin[0] + right[0]*scale, 
			         p->origin[1] + right[1]*scale, 
					 p->origin[2] + right[2]*scale);
	}

	qglEnd ();
	qglDisable( GL_BLEND );
	qglColor4f( 1,1,1,1 );
	qglDepthMask( 1 );		// back to normal Z buffering
	GL_TexEnv( GL_REPLACE );
}

/*
===============
R_DrawParticles
===============
*/
void R_DrawParticles (void)
{
	if ( gl_ext_pointparameters->value && qglPointParameterfEXT )
	{
		int i;
		unsigned char color[4];
		const particle_t *p;

		qglDepthMask( GL_FALSE );
		qglEnable( GL_BLEND );
		qglDisable( GL_TEXTURE_2D );

		qglPointSize( gl_particle_size->value );

		qglBegin( GL_POINTS );
		for ( i = 0, p = r_newrefdef.particles; i < r_newrefdef.num_particles; i++, p++ )
		{
			*(int *)color = d_8to24table[p->color];
			color[3] = p->alpha*255;

			qglColor4ubv( color );

			qglVertex3fv( p->origin );
		}
		qglEnd();

		qglDisable( GL_BLEND );
		qglColor4f( 1.0F, 1.0F, 1.0F, 1.0F );
		qglDepthMask( GL_TRUE );
		qglEnable( GL_TEXTURE_2D );

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

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
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
void R_SetupFrame (void)
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
		qglEnable( GL_SCISSOR_TEST );
		qglClearColor( 0.3, 0.3, 0.3, 1 );
		qglScissor( r_newrefdef.x, vid.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height );
		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		qglClearColor( 1, 0, 0.5, 0.5 );
		qglDisable( GL_SCISSOR_TEST );
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
	x = floor(r_newrefdef.x * vid.width / vid.width);
	x2 = ceil((r_newrefdef.x + r_newrefdef.width) * vid.width / vid.width);
	y = floor(vid.height - r_newrefdef.y * vid.height / vid.height);
	y2 = ceil(vid.height - (r_newrefdef.y + r_newrefdef.height) * vid.height / vid.height);

	w = x2 - x;
	h = y - y2;

	qglViewport (x, y2, w, h);

	//
	// set up projection matrix
	//
    screenaspect = (float)r_newrefdef.width/r_newrefdef.height;
//	yfov = 2*atan((float)r_newrefdef.height/r_newrefdef.width)*180/M_PI;
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity ();
	qglPerspective (r_newrefdef.fov_y,  screenaspect,  4,  4096);

	qglCullFace(GL_FRONT);

	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();

    qglRotatef (-90,  1, 0, 0);	    // put Z going up
    qglRotatef (90,  0, 0, 1);	    // put Z going up
    qglRotatef (-r_newrefdef.viewangles[2],  1, 0, 0);
    qglRotatef (-r_newrefdef.viewangles[0],  0, 1, 0);
    qglRotatef (-r_newrefdef.viewangles[1],  0, 0, 1);
    qglTranslatef (-r_newrefdef.vieworg[0],  -r_newrefdef.vieworg[1],  -r_newrefdef.vieworg[2]);

	qglGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	//
	// set drawing parms
	//
	if (gl_cull->value)
		qglEnable(GL_CULL_FACE);
	else
		qglDisable(GL_CULL_FACE);

	qglDisable(GL_BLEND);
	qglDisable(GL_ALPHA_TEST);
	qglEnable(GL_DEPTH_TEST);
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
			qglClear (GL_COLOR_BUFFER_BIT);

		trickframe++;
		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999;
			qglDepthFunc (GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5;
			qglDepthFunc (GL_GEQUAL);
		}
	}
	else
	{
		if (gl_clear->value)
			qglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			qglClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		qglDepthFunc (GL_LEQUAL);
	}

	qglDepthRange (gldepthmin, gldepthmax);
}

void R_Flash( void )
{
	R_PolyBlend ();
}

/*
================
R_RenderView

r_newrefdef must be set before the first call
================
*/
void R_RenderView (refdef_t *fd)
{
	if (r_norefresh->value)
		return;

	r_newrefdef = *fd;

	if (!r_worldmodel && !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) )
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds->value)
	{
		c_brush_polys = 0;
		c_studio_polys = 0;
	}

	R_PushDlights ();

	if (gl_finish->value) qglFinish ();
	
	R_SetupFrame ();
	R_SetFrustum ();
	R_SetupGL ();
	R_MarkLeaves ();	// done here so we know if we're in water
	R_DrawWorld ();
	R_DrawEntitiesOnList ();
	R_RenderDlights ();
	R_DrawParticles ();
	R_DrawAlphaSurfaces ();
	R_Flash();
	R_BloomBlend (fd);
	GL_DrawRadar();

	if(!( r_newrefdef.rdflags & RDF_NOWORLDMODEL ))
		ri.ShowCollision(); // physic debug
}

void R_DrawPauseScreen( void )
{
	// don't apply post effects for custom window
	if(r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	// temporary disabled
	return;

	if(r_pause->modified )
	{
		// reset saturation value
		if(!r_pause->value)
			r_pause_alpha = 0.0f;
		r_pause->modified = false;
	}
	if(!r_pause->value) return;          
	if (r_pause_alpha < 1.0f) r_pause_alpha += 0.03;

	if (r_pause_alpha <= 1.0f || r_lefthand->modified)
	{
		int	k = r_pause_alpha * 255.0f;
		int	i, s, r, g, b;

		qglFlush();
		qglReadPixels(0, 0, vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, r_framebuffer);
		for (i = 0; i < vid.width * vid.height * 3; i+=3)
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
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity ();
	qglOrtho(0, vid.width, 0, vid.height, 0, 1.0f);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity ();

	qglDisable(GL_TEXTURE_2D);
	qglRasterPos2f(0, 0);
	qglDrawPixels(vid.width, vid.height, GL_RGB, GL_UNSIGNED_BYTE, r_framebuffer);
	qglFlush();
	qglEnable(GL_TEXTURE_2D);
}

int R_DrawRSpeeds(char *S)
{
	return sprintf(S, "%4i wpoly %4i epoly %i tex %i lmaps", c_brush_polys, c_studio_polys, c_visible_textures, c_visible_lightmaps );
}

dword blurtex = 0;
dword blur_shader = 0;

void R_SetGL2D (void)
{
	R_DrawPauseScreen();
		
	// set 2D virtual screen size
	qglViewport (0,0, vid.width, vid.height);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity ();
	qglOrtho(0, vid.width, vid.height, 0, -99999, 99999);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity ();
	qglDisable(GL_DEPTH_TEST);
	qglDisable(GL_CULL_FACE);

	GL_DisableAlphaTest();
	GL_EnableBlend();

	if (gl_state.nv_tex_rectangle ) gl_state.tex_rectangle_type = GL_TEXTURE_RECTANGLE_NV;
	if (gl_state.ati_tex_rectangle) gl_state.tex_rectangle_type = GL_TEXTURE_RECTANGLE_EXT;

	if(!r_motionblur->value && v_blend[3])
	{	
		qglDisable (GL_TEXTURE_2D);
		qglColor4fv (v_blend);

		VA_SetElem2(vert_array[0], 0, 0);
		VA_SetElem2(vert_array[1], vid.width, 0);
		VA_SetElem2(vert_array[2], vid.width, vid.height);
		VA_SetElem2(vert_array[3], 0, vid.height);

		GL_LockArrays( 4 );
		qglDrawArrays(GL_QUADS, 0, 4);
		GL_UnlockArrays();

		qglColor4f (1,1,1,1);
		qglEnable (GL_TEXTURE_2D);
	}
	else if (r_motionblur->value && (r_newrefdef.rdflags & RDF_PAIN))
	{
		if(!gl_state.nv_tex_rectangle && !gl_state.ati_tex_rectangle) return;
		if (blurtex)
		{
	     		GL_TexEnv(GL_MODULATE);
			qglDisable(GL_TEXTURE_2D);
			
			qglEnable(gl_state.tex_rectangle_type);
			qglDisable (GL_ALPHA_TEST);
			qglEnable (GL_BLEND);
			qglBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
			
			if(r_newrefdef.rdflags & (RDF_PAIN))
				qglColor4f (1, 0, 0, r_motionblur_intens->value);

			qglBegin(GL_QUADS);
			qglTexCoord2f(0,vid.height);
			qglVertex2f(0,0);
			qglTexCoord2f(vid.width,vid.height);
			qglVertex2f(vid.width,0);
			qglTexCoord2f(vid.width,0);
			qglVertex2f(vid.width,vid.height);
			qglTexCoord2f(0,0);
			qglVertex2f(0,vid.height);
			qglEnd();
  
			qglDisable(gl_state.tex_rectangle_type);
			qglEnable(GL_TEXTURE_2D);
		}
		if (!blurtex) qglGenTextures(1,&blurtex);
		qglBindTexture(gl_state.tex_rectangle_type,blurtex);
		qglCopyTexImage2D(gl_state.tex_rectangle_type,0,GL_RGB,0,0,vid.width,vid.height,0);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	} 

	GL_DisableBlend();
	GL_EnableAlphaTest();
	qglColor4f (1,1,1,1);

	if (r_speeds->value)
	{
		char	S[128];
		int	n = 0;
		int	i;

		n = R_DrawRSpeeds(S);
		qglColor4f(1, 0, 1, 1);
		for (i = 0; i < n; i++)
			Draw_Char(r_newrefdef.width - 60 + ((i - n) * 8), r_newrefdef.height - 60, 128 + S[i]);
		qglColor4f (1, 1, 1, 1);
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

/*
@@@@@@@@@@@@@@@@@@@@@
R_RenderFrame

@@@@@@@@@@@@@@@@@@@@@
*/
void R_RenderFrame (refdef_t *fd)
{
	R_RenderView( fd );
	R_SetLightLevel ();
	R_SetGL2D ();
}


void R_Register( void )
{
	r_lefthand = Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );
	r_norefresh = Cvar_Get ("r_norefresh", "0", 0);
	r_fullbright = Cvar_Get ("r_fullbright", "0", 0);
	r_drawentities = Cvar_Get ("r_drawentities", "1", 0);
	r_drawworld = Cvar_Get ("r_drawworld", "1", 0);
	r_novis = Cvar_Get ("r_novis", "0", 0);
	r_nocull = Cvar_Get ("r_nocull", "0", 0);
	r_lerpmodels = Cvar_Get ("r_lerpmodels", "1", 0);
	r_speeds = Cvar_Get ("r_speeds", "0", 0);
	r_pause = Cvar_Get("paused", "0", 0);

	r_loading = Cvar_Get("scr_loading", "0", 0 );

	r_lightlevel = Cvar_Get ("r_lightlevel", "0", 0);
	r_emboss_bump = Cvar_Get ("r_emboss_bump", "0", 0);

 	r_motionblur_intens = Cvar_Get( "r_motionblur_intens", "0.65", CVAR_ARCHIVE );
	r_motionblur = Cvar_Get( "r_motionblur", "0", CVAR_ARCHIVE );

	gl_nosubimage = Cvar_Get( "gl_nosubimage", "0", 0 );
	gl_allow_software = Cvar_Get( "gl_allow_software", "0", 0 );

	gl_particle_min_size = Cvar_Get( "gl_particle_min_size", "2", CVAR_ARCHIVE );
	gl_particle_max_size = Cvar_Get( "gl_particle_max_size", "40", CVAR_ARCHIVE );
	gl_particle_size = Cvar_Get( "gl_particle_size", "40", CVAR_ARCHIVE );
	gl_particle_att_a = Cvar_Get( "gl_particle_att_a", "0.01", CVAR_ARCHIVE );
	gl_particle_att_b = Cvar_Get( "gl_particle_att_b", "0.0", CVAR_ARCHIVE );
	gl_particle_att_c = Cvar_Get( "gl_particle_att_c", "0.01", CVAR_ARCHIVE );

	r_bloom = Cvar_Get( "r_bloom", "0", CVAR_ARCHIVE );
	r_bloom_alpha = Cvar_Get( "r_bloom_alpha", "0.5", CVAR_ARCHIVE );
	r_bloom_diamond_size = Cvar_Get( "r_bloom_diamond_size", "8", CVAR_ARCHIVE );
	r_bloom_intensity = Cvar_Get( "r_bloom_intensity", "0.6", CVAR_ARCHIVE );
	r_bloom_darken = Cvar_Get( "r_bloom_darken", "4", CVAR_ARCHIVE );
	r_bloom_sample_size = Cvar_Get( "r_bloom_sample_size", "128", CVAR_ARCHIVE );
	r_bloom_fast_sample = Cvar_Get( "r_bloom_fast_sample", "0", CVAR_ARCHIVE );

	r_minimap_size = Cvar_Get ("r_minimap_size", "256", CVAR_ARCHIVE );
	r_minimap_zoom = Cvar_Get ("r_minimap_zoom", "1", CVAR_ARCHIVE );
	r_minimap_style = Cvar_Get ("r_minimap_style", "1", CVAR_ARCHIVE );  
	r_minimap = Cvar_Get("r_minimap", "0", CVAR_ARCHIVE ); 

	gl_modulate = Cvar_Get ("gl_modulate", "1", CVAR_ARCHIVE );
	gl_log = Cvar_Get( "gl_log", "0", 0 );
	gl_bitdepth = Cvar_Get( "gl_bitdepth", "0", 0 );
	gl_mode = Cvar_Get( "gl_mode", "3", CVAR_ARCHIVE );
	gl_lightmap = Cvar_Get ("gl_lightmap", "0", 0);
	gl_shadows = Cvar_Get ("gl_shadows", "0", CVAR_ARCHIVE );
	gl_dynamic = Cvar_Get ("gl_dynamic", "1", 0);
	gl_nobind = Cvar_Get ("gl_nobind", "0", 0);
	gl_round_down = Cvar_Get ("gl_round_down", "1", 0);
	gl_picmip = Cvar_Get ("gl_picmip", "0", 0);
	gl_skymip = Cvar_Get ("gl_skymip", "0", 0);
	gl_showtris = Cvar_Get ("gl_showtris", "0", 0);
	gl_ztrick = Cvar_Get ("gl_ztrick", "0", 0);
	gl_finish = Cvar_Get ("gl_finish", "0", CVAR_ARCHIVE);
	gl_clear = Cvar_Get ("gl_clear", "0", 0);
	gl_cull = Cvar_Get ("gl_cull", "1", 0);
	gl_polyblend = Cvar_Get ("gl_polyblend", "1", 0);
	gl_flashblend = Cvar_Get ("gl_flashblend", "0", 0);
	gl_playermip = Cvar_Get ("gl_playermip", "0", 0);
	gl_texturemode = Cvar_Get( "gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE );
	gl_texturealphamode = Cvar_Get( "gl_texturealphamode", "default", CVAR_ARCHIVE );
	gl_texturesolidmode = Cvar_Get( "gl_texturesolidmode", "default", CVAR_ARCHIVE );
	gl_lockpvs = Cvar_Get( "gl_lockpvs", "0", 0 );

	gl_vertex_arrays = Cvar_Get( "gl_vertex_arrays", "0", CVAR_ARCHIVE );

	gl_ext_swapinterval = Cvar_Get( "gl_ext_swapinterval", "1", CVAR_ARCHIVE );
	gl_ext_palettedtexture = Cvar_Get( "gl_ext_palettedtexture", "0", CVAR_ARCHIVE );
	gl_ext_multitexture = Cvar_Get( "gl_ext_multitexture", "1", CVAR_ARCHIVE );
	gl_ext_pointparameters = Cvar_Get( "gl_ext_pointparameters", "1", CVAR_ARCHIVE );
	gl_ext_compiled_vertex_array = Cvar_Get( "gl_ext_compiled_vertex_array", "1", CVAR_ARCHIVE );

	gl_drawbuffer = Cvar_Get( "gl_drawbuffer", "GL_BACK", 0 );
	gl_swapinterval = Cvar_Get( "gl_swapinterval", "1", CVAR_ARCHIVE );

	gl_saturatelighting = Cvar_Get( "gl_saturatelighting", "0", 0 );

	gl_3dlabs_broken = Cvar_Get( "gl_3dlabs_broken", "1", CVAR_ARCHIVE );

	r_fullscreen = Cvar_Get( "r_fullscreen", "0", CVAR_ARCHIVE );
	vid_gamma = Cvar_Get( "vid_gamma", "1.0", CVAR_ARCHIVE );

	Cmd_AddCommand( "imagelist", R_ImageList_f );
	Cmd_AddCommand( "modellist", Mod_Modellist_f );
	Cmd_AddCommand( "gl_strings", GL_Strings_f );
}

/*
==================
R_SetMode
==================
*/
bool R_SetMode (void)
{
	rserr_t err;
	bool fullscreen;

	if ( r_fullscreen->modified && !gl_config.allow_cds )
	{
		MsgWarn("R_SetMode: CDS not allowed with this driver\n" );
		ri.Cvar_SetValue( "r_fullscreen", !r_fullscreen->value );
		r_fullscreen->modified = false;
	}

	fullscreen = r_fullscreen->value;

	r_fullscreen->modified = false;
	gl_mode->modified = false;

	if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_mode->value, fullscreen ) ) == rserr_ok )
	{
		gl_state.prev_mode = gl_mode->value;
	}
	else
	{
		if ( err == rserr_invalid_fullscreen )
		{
			ri.Cvar_SetValue( "r_fullscreen", 0);
			r_fullscreen->modified = false;
			MsgWarn("R_SetMode: fullscreen unavailable in this mode\n" );
			if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_mode->value, false ) ) == rserr_ok )
				return true;
		}
		else if ( err == rserr_invalid_mode )
		{
			ri.Cvar_SetValue( "gl_mode", gl_state.prev_mode );
			gl_mode->modified = false;
			MsgWarn("R_SetMode: invalid mode\n" );
		}

		// try setting it back to something safe
		if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_state.prev_mode, false ) ) != rserr_ok )
		{
			MsgWarn("R_SetMode: could not revert to safe mode\n" );
			return false;
		}
	}
	return true;
}

/*
===============
R_Init
===============
*/
int R_Init( void *hinstance, void *hWnd )
{	
	char		renderer_buffer[1000];
	char		vendor_buffer[1000];
	int		err;
	int		j;
	extern float	r_turbsin[256];
          
	r_temppool = Mem_AllocPool( "Render Memory" );
	
	for ( j = 0; j < 256; j++ )
	{
		r_turbsin[j] *= 0.5;
	}

	R_GetPalette ();
	R_Register();

	// initialize our QGL dynamic bindings
	if ( !QGL_Init( "opengl32" ) )
	{
		QGL_Shutdown();
		return false;
	}

	// initialize OS-specific parts of OpenGL
	if ( !GLimp_Init( hinstance, hWnd ) )
	{
		QGL_Shutdown();
		return false;
	}

	// set our "safe" modes
	gl_state.prev_mode = 3;

	// create the window and set up the context
	if ( !R_SetMode () )
	{
		QGL_Shutdown();
        		MsgWarn("R_Init: could not R_SetMode()\n" );
		return false;
	}

	// FIXME: don't remove - renderer immediately crash without it
	MsgDev(D_INFO, "Initializing render\n" );
	ri.Vid_MenuInit();

	// get our various GL strings
	
	gl_config.vendor_string = qglGetString (GL_VENDOR);
	MsgDev(D_INFO, "GL_VENDOR: %s\n", gl_config.vendor_string );
	gl_config.renderer_string = qglGetString (GL_RENDERER);
	MsgDev(D_INFO, "GL_RENDERER: %s\n", gl_config.renderer_string );
	gl_config.version_string = qglGetString (GL_VERSION);
	MsgDev(D_INFO, "GL_VERSION: %s\n", gl_config.version_string );
	gl_config.extensions_string = qglGetString (GL_EXTENSIONS);
	MsgDev(D_INFO, "GL_EXTENSIONS: %s\n", gl_config.extensions_string );
          
	strcpy( renderer_buffer, gl_config.renderer_string );
	strlwr( renderer_buffer );

	strcpy( vendor_buffer, gl_config.vendor_string );
	strlwr( vendor_buffer );
        
	if ( strstr( renderer_buffer, "voodoo" ) )
		gl_config.renderer = GL_RENDERER_VOODOO;
	if ( strstr( vendor_buffer, "ati" ) )
		gl_config.renderer = GL_RENDERER_ATI;
	else if ( strstr( vendor_buffer, "nvidia" ) )
		gl_config.renderer = GL_RENDERER_ATI;
	else gl_config.renderer = GL_RENDERER_DEFAULT;
          
	gl_config.allow_cds = true;

	/*
	** grab extensions
	*/
	if ( strstr( gl_config.extensions_string, "GL_EXT_compiled_vertex_array" ) || strstr( gl_config.extensions_string, "GL_SGI_compiled_vertex_array" ) )
	{
		MsgDev(D_INFO, "...enabling GL_EXT_compiled_vertex_array\n" );
		qglLockArraysEXT = ( void * ) qwglGetProcAddress( "glLockArraysEXT" );
		qglUnlockArraysEXT = ( void * ) qwglGetProcAddress( "glUnlockArraysEXT" );
	}
	else MsgDev(D_WARN, "...GL_EXT_compiled_vertex_array not found\n" );

	if ( strstr( gl_config.extensions_string, "WGL_EXT_swap_control" ) )
	{
		qwglSwapIntervalEXT = ( BOOL (WINAPI *)(int)) qwglGetProcAddress( "wglSwapIntervalEXT" );
		MsgDev(D_INFO, "...enabling WGL_EXT_swap_control\n" );
	}
	else
	{
		MsgDev(D_WARN, "...WGL_EXT_swap_control not found\n" );
	}

	if (strstr( gl_config.extensions_string, "GL_ARB_texture_compression" ))
	{
		if (strstr( gl_config.extensions_string, "GL_EXT_texture_compression_s3tc" ))
		{
			qglCompressedTexImage2D = ( void *)qwglGetProcAddress("glCompressedTexImage2DARB");
			qglGetCompressedTexImage = ( void *)qwglGetProcAddress("glGetCompressedTexImageARB");
		}

		if (qglCompressedTexImage2D && qglGetCompressedTexImage)
			gl_config.arb_compressed_teximage = true;
		else gl_config.arb_compressed_teximage = false;
	}
 
	if ( strstr( gl_config.extensions_string, "GL_EXT_point_parameters" ) )
	{
		if ( gl_ext_pointparameters->value )
		{
			qglPointParameterfEXT = ( void (APIENTRY *)( GLenum, GLfloat ) ) qwglGetProcAddress( "glPointParameterfEXT" );
			qglPointParameterfvEXT = ( void (APIENTRY *)( GLenum, const GLfloat * ) ) qwglGetProcAddress( "glPointParameterfvEXT" );
			MsgDev(D_INFO, "...using GL_EXT_point_parameters\n" );
		}
		else
		{
			MsgDev(D_INFO, "...ignoring GL_EXT_point_parameters\n" );
		}
	}
	else
	{
		MsgDev(D_WARN, "...GL_EXT_point_parameters not found\n" );
	}

	if ( strstr( gl_config.extensions_string, "GL_ARB_multitexture" ) )
	{
		if ( gl_ext_multitexture->value )
		{
			MsgDev(D_INFO, "...using GL_ARB_multitexture\n" );
			qglMTexCoord2fSGIS = ( void * ) qwglGetProcAddress( "glMultiTexCoord2fARB" );
			qglActiveTextureARB = ( void * ) qwglGetProcAddress( "glActiveTextureARB" );
			qglClientActiveTextureARB = ( void * ) qwglGetProcAddress( "glClientActiveTextureARB" );
			GL_TEXTURE0 = GL_TEXTURE0_ARB;
			GL_TEXTURE1 = GL_TEXTURE1_ARB;
		}
		else
		{
			MsgDev(D_INFO, "...ignoring GL_ARB_multitexture\n" );
		}
	}
	else
	{
		MsgDev(D_WARN, "...GL_ARB_multitexture not found\n" );
	}

	if ( strstr( gl_config.extensions_string, "GL_NV_texture_rectangle" ) )
	{
		MsgDev(D_INFO, "...using GL_NV_texture_rectangle\n");
		gl_state.nv_tex_rectangle = true;
	}
	else
	{
		MsgDev(D_WARN, "...GL_NV_texture_rectangle not found\n");
		gl_state.nv_tex_rectangle = false;
	}

	if ( strstr( gl_config.extensions_string, "GL_EXT_texture_rectangle" ) )
	{
		MsgDev(D_INFO, "...using GL_EXT_texture_rectangle\n");
		gl_state.ati_tex_rectangle = true;
	}
	else
	{
		MsgDev(D_WARN, "...GL_EXT_texture_rectangle not found\n");
		gl_state.ati_tex_rectangle = false;
	}

	if ( strstr( gl_config.extensions_string, "GL_SGIS_multitexture" ) )
	{
		if ( qglActiveTextureARB )
		{
			MsgDev(D_INFO, "...GL_SGIS_multitexture deprecated in favor of ARB_multitexture\n" );
		}
		else if ( gl_ext_multitexture->value )
		{
			MsgDev(D_INFO, "...using GL_SGIS_multitexture\n" );
			qglMTexCoord2fSGIS = ( void * ) qwglGetProcAddress( "glMTexCoord2fSGIS" );
			qglSelectTextureSGIS = ( void * ) qwglGetProcAddress( "glSelectTextureSGIS" );
			GL_TEXTURE0 = GL_TEXTURE0_SGIS;
			GL_TEXTURE1 = GL_TEXTURE1_SGIS;
		}
		else
		{
			MsgDev(D_INFO, "...ignoring GL_SGIS_multitexture\n" );
		}
	}
	else
	{
		MsgDev(D_WARN, "...GL_SGIS_multitexture not found\n" );
	}

	GL_SetDefaultState();

	R_InitTextures();
	Mod_Init ();
	R_InitParticleTexture ();
	Draw_InitLocal ();
          R_StudioInit();

	if(!r_framebuffer) r_framebuffer = Z_Malloc(vid.width*vid.height*3);
	
	err = qglGetError();
	if ( err != GL_NO_ERROR ) MsgWarn("glGetError = 0x%x\n", err);

	return 1;
}

/*
===============
R_Shutdown
===============
*/
void R_Shutdown (void)
{	
	ri.Cmd_RemoveCommand ("modellist");
	ri.Cmd_RemoveCommand ("imagelist");
	ri.Cmd_RemoveCommand ("gl_strings");

	if(r_framebuffer) Z_Free(r_framebuffer);

	Mod_FreeAll ();
          R_StudioShutdown();
	R_ShutdownTextures();

	/*
	** shut down OS specific OpenGL stuff like contexts, etc.
	*/
	GLimp_Shutdown();

	/*
	** shutdown our QGL subsystem
	*/
	QGL_Shutdown();
}



/*
@@@@@@@@@@@@@@@@@@@@@
R_BeginFrame
@@@@@@@@@@@@@@@@@@@@@
*/
void R_BeginFrame( void )
{
	if ( gl_log->modified )
	{
		GLimp_EnableLogging( gl_log->value );
		gl_log->modified = false;
	}

	if ( gl_log->value )
	{
		GLimp_LogNewFrame();
	}

	if ( vid_gamma->modified )
	{
		vid_gamma->modified = false;
          	//put here update gamma
	}

	GLimp_BeginFrame();

	/*
	** go into 2D mode
	*/
	qglViewport (0,0, vid.width, vid.height);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity ();
	qglOrtho  (0, vid.width, vid.height, 0, -99999, 99999);
	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity ();
	qglDisable (GL_DEPTH_TEST);
	qglDisable (GL_CULL_FACE);
	GL_DisableBlend();
	GL_EnableAlphaTest();
	qglColor4f (1,1,1,1);

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

	qglClearColor (0,0,0,0);
	qglClear (GL_COLOR_BUFFER_BIT);
	qglClearColor (1, 0, 0.5, 0.5);
}

/*
** R_DrawBeam
*/
void R_DrawBeam( entity_t *e )
{
#define NUM_BEAM_SEGS 6

	int	i;
	float r, g, b;

	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t	start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if ( VectorNormalize( normalized_direction ) == 0 )
		return;

	PerpendicularVector( perpvec, normalized_direction );
	VectorScale( perpvec, e->frame / 2, perpvec );

	for ( i = 0; i < 6; i++ )
	{
		RotatePointAroundVector( start_points[i], normalized_direction, perpvec, (360.0/NUM_BEAM_SEGS)*i );
		VectorAdd( start_points[i], origin, start_points[i] );
		VectorAdd( start_points[i], direction, end_points[i] );
	}

	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_BLEND );
	qglDepthMask( GL_FALSE );

	r = ( d_8to24table[e->skinnum & 0xFF] ) & 0xFF;
	g = ( d_8to24table[e->skinnum & 0xFF] >> 8 ) & 0xFF;
	b = ( d_8to24table[e->skinnum & 0xFF] >> 16 ) & 0xFF;

	r *= 1/255.0F;
	g *= 1/255.0F;
	b *= 1/255.0F;

	qglColor4f( r, g, b, e->alpha );

	qglBegin( GL_TRIANGLE_STRIP );
	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		qglVertex3fv( start_points[i] );
		qglVertex3fv( end_points[i] );
		qglVertex3fv( start_points[(i+1)%NUM_BEAM_SEGS] );
		qglVertex3fv( end_points[(i+1)%NUM_BEAM_SEGS] );
	}
	qglEnd();

	qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_BLEND );
	qglDepthMask( GL_TRUE );
}

/*
===============
R_RegisterSkin
===============
*/
image_t *R_RegisterSkin (char *name)
{
	return R_FindImage (name, NULL, 0, it_skin);
}

//===================================================================


void R_BeginRegistration (char *map);
model_t	*R_RegisterModel (char *name);
void R_SetSky (char *name, float rotate, vec3_t axis);
void	R_EndRegistration (void);

void	R_RenderFrame (refdef_t *fd);

image_t	*Draw_FindPic (char *name);

/*
@@@@@@@@@@@@@@@@@@@@@
CreateAPI

@@@@@@@@@@@@@@@@@@@@@
*/
render_exp_t DLLEXPORT *CreateAPI(stdlib_api_t *input, render_imp_t *engfuncs )
{
	static render_exp_t re;

	std = *input;
	// Sys_LoadLibrary can create fake instance, to check
	// api version and api size, but second argument will be 0
	// and always make exception, run simply check for avoid it
	if(engfuncs) ri = *engfuncs;

	// generic functions
	re.api_size = sizeof(render_exp_t);

	re.Init = R_Init;
	re.Shutdown = R_Shutdown;
	re.AppActivate = GLimp_AppActivate;
	
	re.BeginRegistration = R_BeginRegistration;
	re.RegisterModel = R_RegisterModel;
	re.RegisterSkin = R_RegisterSkin;
	re.RegisterPic = Draw_FindPic;
	re.SetSky = R_SetSky;
	re.EndRegistration = R_EndRegistration;

	re.BeginFrame = R_BeginFrame;
	re.RenderFrame = R_RenderFrame;
	re.EndFrame = GLimp_EndFrame;

	re.SetColor = GL_SetColor;
	re.ScrShot = VID_ScreenShot;
	re.DrawFill = Draw_Fill;
	re.DrawStretchRaw = Draw_StretchRaw;
	re.DrawStretchPic = Draw_StretchPic;

	// get rid of this
	re.DrawGetPicSize = Draw_GetPicSize;

	return &re;
}