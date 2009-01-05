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
// cl_view.c -- player rendering positioning

#include "common.h"
#include "client.h"
#include "const.h"

/*
====================
V_ClearScene

Specifies the model that will be used as the world
====================
*/
void V_ClearScene( void )
{
	re->ClearScene();
}

/*
=================
void V_CalcRect( void )

Sets scr_vrect, the coordinates of the rendered window
=================
*/
void V_CalcRect( void )
{
	scr_rect[2] = scr_width->integer;
	scr_rect[2] &= ~7;
	scr_rect[3] = scr_height->integer;
	scr_rect[3] &= ~1;
	scr_rect[0] = scr_rect[1] = 0;
}

/*
================
V_TestEntities

If cl_testentities is set, create 32 player models
================
*/
void V_TestEntities( void )
{
	int		i, j;
	float		f, r;
	edict_t		ent;

	Mem_Set( &ent, 0, sizeof( edict_t ));
	V_ClearScene();

	for( i = 0; i < 32; i++ )
	{
		r = 64 * ((i%4) - 1.5 );
		f = 64 * (i/4) + 128;

		for( j = 0; j < 3; j++ )
			ent.v.origin[j] = cl.refdef.vieworg[j]+cl.refdef.forward[j] * f + cl.refdef.right[j] * r;

		ent.v.scale = 1.0f;
		ent.serialnumber = cl.frame.ps.number;
		ent.v.controller[0] = ent.v.controller[1] = 90.0f;
		ent.v.controller[2] = ent.v.controller[3] = 180.0f;
		ent.v.modelindex = cl.frame.ps.model.index;
		re->AddRefEntity( &ent, ED_NORMAL, 1.0f );
	}
}

/*
================
V_TestLights

If cl_testlights is set, create 32 lights models
================
*/
void V_TestLights( void )
{
	int		i, j;
	float		f, r;
	cdlight_t		dl;

	Mem_Set( &dl, 0, sizeof( cdlight_t ));
	V_ClearScene();
	
	for( i = 0; i < 32; i++ )
	{
		r = 64 * ( (i%4) - 1.5 );
		f = 64 * (i/4) + 128;

		for( j = 0; j < 3; j++ )
			dl.origin[j] = cl.refdef.vieworg[j] + cl.refdef.forward[j] * f + cl.refdef.right[j] * r;

		dl.color[0] = ((i%6)+1) & 1;
		dl.color[1] = (((i%6)+1) & 2)>>1;
		dl.color[2] = (((i%6)+1) & 4)>>2;
		dl.radius = 200;
		re->AddDynLight( dl.origin, dl.color, dl.radius ); 
	}
}

/*
====================
V_CalcFov
====================
*/
float V_CalcFov( float fov_x, float width, float height )
{
	float	fov_y, x, rad = 360.0f * M_PI;

	// check to avoid division by zero
	if( fov_x == 0 ) Host_Error( "V_CalcFov: null fov!\n" );

	// make sure that fov in-range
	fov_x = bound( 1, fov_x, 179 );
	x = width / tan( fov_x / rad );
	fov_y = atan2( height, x );
	fov_y = (fov_y * rad);

	return fov_y;
}

//============================================================================

/*
==================
V_RenderView

==================
*/
void V_RenderView( void )
{
	if( !cl.video_prepped ) return; // still loading

	// an invalid frame will just use the exact previous refdef
	// we can't use the old frame if the video mode has changed, though...
	if ( cl.frame.valid && (cl.force_refdef || !cl_paused->value) )
	{
		cl.force_refdef = false;
		V_ClearScene();

		// build a refresh entity list and calc cl.sim*
		// this also calls CL_CalcViewValues which loads
		// refdef.forward, etc.
		CL_AddEntities ();

		if( cl_testentities->value ) V_TestEntities();
		if( cl_testlights->value ) V_TestLights();

		// never let it sit exactly on a node line, because a water plane can
		// dissapear when viewed with the eye exactly on it.
		// the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis
		cl.refdef.vieworg[0] += 1.0 / 32;
		cl.refdef.vieworg[1] += 1.0 / 32;
		cl.refdef.vieworg[2] += 1.0 / 32;

		Mem_Copy( &cl.refdef.viewport, &scr_rect, sizeof( cl.refdef.viewport ));
                    
                    cl.refdef.areabits = cl.frame.areabits;
                    cl.refdef.rdflags = cl.frame.ps.renderfx;
		cl.refdef.fov_y = V_CalcFov( cl.refdef.fov_x, cl.refdef.viewport[2], cl.refdef.viewport[3] );
		cl.refdef.oldtime = (cl.oldtime * 0.001f);
		cl.refdef.time = (cl.time * 0.001f); // cl.time for right lerping		
		cl.refdef.frametime = cls.frametime;

		if( cl.refdef.rdflags & RDF_UNDERWATER )
		{
			float f = com.sin( cl.time * 0.001 * 0.4 * (M_PI * 2.7));
			cl.refdef.fov_x += f;
			cl.refdef.fov_y -= f;
		} 
	}
	re->RenderFrame( &cl.refdef );
}

/*
==================
V_PreRender

==================
*/
bool V_PreRender( void )
{
	// too early
	if( !re ) return false;
		
	re->BeginFrame();
	SCR_FillRect( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, g_color_table[0] );
	return true;
}

/*
==================
V_PostRender

==================
*/
void V_PostRender( void )
{
	SCR_DrawNet();
	SCR_DrawFPS();
	UI_Draw();
	Con_DrawConsole();
	re->EndFrame();
}