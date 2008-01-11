//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       r_mirror.h - realtime stencil mirror
//=======================================================================
#ifndef R_MIRROR_H
#define R_MIRROR_H

#include "gl_model.h"

void Mirror_Scale( void );
void Mirror_Clear( void );
void R_Mirror( refdef_t *fd );
extern bool mirror;
extern cplane_t *mirror_plane;
extern msurface_t *mirrorchain;
extern bool mirror_render;

#endif//R_MIRROR_H