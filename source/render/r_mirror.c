//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        r_sprite.c - render sprite models
//=======================================================================

#include "gl_local.h"
#include "r_mirror.h"

// glmsurface_t == msurface_t
bool	mirror;
cplane_t	*mirror_plane;
msurface_t *mirrorchain = NULL;
bool	mirror_render;	// true when reflections are being rendered
float	r_base_world_matrix[16];

void Mirror_Scale( void )
{
	if( mirror_plane->normal[2] )
		qglScalef( 1, -1, 1);
	else qglScalef(-1, 1, 1);
	qglCullFace( GL_BACK );
}

/*
=============
R_Mirror
=============
*/
void R_Mirror( refdef_t *fd )
{
	float		d, *v, mirror_alpha;
	vec3_t		old_vieworg;
	vec3_t		old_viewangles;
	vec3_t		old_vforward;
	msurface_t	*s;
	glpoly_t		*p;
	int		i;

	// don't have infinite reflections if 2 mirrors are facing each other (looks ugly but
	// better than hanging the engine)
	mirror_render = true;

	Mem_Copy( r_base_world_matrix, r_world_matrix, sizeof(r_base_world_matrix));
	VectorCopy(r_newrefdef.vieworg, old_vieworg);
	VectorCopy(r_newrefdef.viewangles, old_viewangles);
	VectorCopy(vforward, old_vforward);

	// r_mirroralpha values of more than about 0.65 don't really look well or have any effect
	mirror_alpha = r_mirroralpha->value * (1.0 / 1.5);

	for( s = mirrorchain; s; s = s->texturechain )
	{
		mirror_plane = s->plane;

		d = PlaneDiff( old_vieworg, mirror_plane);
		VectorMA( old_vieworg, -2 * d, mirror_plane->normal, r_newrefdef.vieworg);

		d = DotProduct( old_vforward, mirror_plane->normal);
		VectorMA( old_vforward, -2 * d, mirror_plane->normal, vforward);

		r_newrefdef.viewangles[0] = -asin(vforward[2]) / M_PI * 180;
		r_newrefdef.viewangles[1] = atan2(vforward[1], vforward[0]) / M_PI * 180;
		r_newrefdef.viewangles[2] = -old_viewangles[2];

		gldepthmin = 0.5;
		gldepthmax = 1;
		qglDepthRange( gldepthmin, gldepthmax );
		qglDepthFunc( GL_LEQUAL );

		R_RenderView( &r_newrefdef ); // do reflection

		gldepthmin = 0;
		gldepthmax = 0.5;
		qglDepthRange( gldepthmin, gldepthmax );
		qglDepthFunc( GL_LEQUAL );
		qglMatrixMode( GL_PROJECTION );

		if (mirror_plane->normal[2])
			qglScalef (1, -1, 1);
		else qglScalef (-1, 1, 1);

		qglCullFace( GL_FRONT );
		qglMatrixMode( GL_MODELVIEW );
		qglLoadMatrixf( r_base_world_matrix );
		GL_EnableBlend();
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE);
		qglColor4f( 1.0f, 1.0f, 1.0f, mirror_alpha );
		GL_Bind(s->texinfo->image->texnum[0]);

		p = s->polys;
		v = p->verts[0];

		qglBegin( GL_POLYGON );

		// draw mirror surface
		for (i = 0; i < p->numverts; i++, v += VERTEXSIZE)
		{
			qglTexCoord2fv(&v[3]);
			qglVertex3fv(v);
		}
		qglEnd();
		qglColor4f( 1.0, 1.0f, 1.0f, 1.0f );
		GL_DisableBlend();
	}

	mirrorchain = NULL;
	mirror = false;
	mirror_render = false;
}