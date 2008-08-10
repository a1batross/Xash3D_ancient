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

/*
====================
V_ClearScene

Specifies the model that will be used as the world
====================
*/
void V_ClearScene( void )
{
	re->ClearScene( &cl.refdef );
}

/*
=================
void V_CalcRect( void )

Sets scr_vrect, the coordinates of the rendered window
=================
*/
void V_CalcRect( void )
{
	scr_vrect.width = scr_width->integer;
	scr_vrect.width &= ~7;
	scr_vrect.height = scr_height->integer;
	scr_vrect.height &= ~1;
	scr_vrect.y = scr_vrect.x = 0;
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
	entity_state_t	ent;

	memset( &ent, 0, sizeof(entity_state_t));
	cl.refdef.num_entities = 0;

	for( i = 0; i < 32; i++ )
	{
		r = 64 * ((i%4) - 1.5 );
		f = 64 * (i/4) + 128;

		for( j = 0; j < 3; j++ )
			ent.origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j] * f + cl.v_right[j] * r;

		ent.model.controller[0] = ent.model.controller[1] = 90.0f;
		ent.model.controller[2] = ent.model.controller[3] = 180.0f;
		ent.model.index = cl.frame.ps.model.index;
		re->AddRefEntity( &cl.refdef, &ent, NULL, 1.0f );
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

	cl.refdef.num_dlights = 0;
	memset( &dl, 0, sizeof(cdlight_t));

	for( i = 0; i < 32; i++ )
	{
		r = 64 * ( (i%4) - 1.5 );
		f = 64 * (i/4) + 128;

		for( j = 0; j < 3; j++ )
			dl.origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j] * f + cl.v_right[j] * r;

		dl.color[0] = ((i%6)+1) & 1;
		dl.color[1] = (((i%6)+1) & 2)>>1;
		dl.color[2] = (((i%6)+1) & 4)>>2;
		dl.radius = 200;
		re->AddDynLight( &cl.refdef, dl.origin, dl.color, dl.radius ); 
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
		// v_forward, etc.
		CL_VM_Begin();
		CL_AddEntities ();

		if( cl_testentities->value ) V_TestEntities();
		if( cl_testlights->value ) V_TestLights();
		if( cl_testblend->value)
		{
			cl.refdef.blend[0] = 1;
			cl.refdef.blend[1] = 0.5;
			cl.refdef.blend[2] = 0.25;
			cl.refdef.blend[3] = 0.8;
		}

		// never let it sit exactly on a node line, because a water plane can
		// dissapear when viewed with the eye exactly on it.
		// the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis
		cl.refdef.vieworg[0] += 1.0 / 32;
		cl.refdef.vieworg[1] += 1.0 / 32;
		cl.refdef.vieworg[2] += 1.0 / 32;

		Mem_Copy( &cl.refdef.rect, &scr_vrect, sizeof(vrect_t));
                    
                    cl.refdef.areabits = cl.frame.areabits;
                    cl.refdef.rdflags = cl.frame.ps.renderfx;
		cl.refdef.fov_y = V_CalcFov( cl.refdef.fov_x, cl.refdef.rect.width, cl.refdef.rect.height );
		cl.refdef.time = cl.time * 0.001f; // cl.time for right lerping		
		cl.refdef.oldtime = (cl.time * 0.001f) - 0.003;
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
	UI_Draw();
	Con_DrawConsole();
	re->EndFrame();
}