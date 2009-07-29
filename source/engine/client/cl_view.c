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

/*
===============
V_SetupRefDef

update refdef values each frame
===============
*/
void V_SetupRefDef( void )
{
	int		i;
	float		backlerp;
	frame_t		*oldframe;
	entity_state_t	*ps, *ops;
	edict_t		*clent;

	// find the previous frame to interpolate from
	ps = &cl.frame.ps;
	i = (cl.frame.serverframe - 1) & UPDATE_MASK;
	oldframe = &cl.frames[i];
	if( oldframe->serverframe != cl.frame.serverframe-1 || !oldframe->valid )
		oldframe = &cl.frame; // previous frame was dropped or invalid
	ops = &oldframe->ps;

	clent = EDICT_NUM( cl.playernum + 1 );

	// see if the player entity was teleported this frame
	if( ps->ed_flags & ESF_NO_PREDICTION )
		ops = ps;	// don't interpolate

	// UNDONE: temporary place for detect waterlevel
	CL_CheckWater( clent );

	// interpolate field of view
	cl.data.fov = ops->fov + cl.refdef.lerpfrac * ( ps->fov - ops->fov );

	VectorCopy( ps->velocity, cl.refdef.velocity );
	VectorCopy( ps->origin, cl.refdef.origin );
	VectorCopy( ops->origin, cl.refdef.prev.origin );
	VectorCopy( ps->angles, cl.refdef.angles );
	VectorCopy( ops->angles, cl.refdef.prev.angles );
	VectorCopy( ps->viewoffset, cl.refdef.viewheight );
	VectorCopy( ops->viewoffset, cl.refdef.prev.viewheight );
	VectorCopy( ps->punch_angles, cl.refdef.punchangle );
	VectorCopy( ops->punch_angles, cl.refdef.prev.punchangle );
	VectorCopy( cl.predicted_angles, cl.refdef.cl_viewangles );

	cl.refdef.movevars = &clgame.movevars;

	if( ps->flags & FL_ONGROUND )
		cl.refdef.onground = EDICT_NUM( ps->groundent );
	else cl.refdef.onground = NULL;
	cl.refdef.areabits = cl.frame.areabits;
	cl.refdef.clientnum = cl.playernum; // not a entity num
	cl.refdef.viewmodel = ps->viewmodel;
	cl.refdef.health = ps->health;
	cl.refdef.num_entities = clgame.numEntities;
	cl.refdef.max_entities = clgame.maxEntities;
	cl.refdef.max_clients = clgame.maxClients;
	cl.refdef.oldtime = cl.oldtime;
	cl.refdef.time = cl.time;		// cl.time for right lerping
	cl.refdef.frametime = cls.frametime;
	cl.refdef.demoplayback = cls.demoplayback;
	cl.refdef.demorecord = cls.demorecording;
	cl.refdef.paused = cl_paused->integer;
	cl.refdef.predicting = cl_predict->integer;
	cl.refdef.waterlevel = clent->v.waterlevel;		
	cl.refdef.rdflags = cl.render_flags;
	cl.refdef.nextView = 0;

	// calculate the origin
	if( cl.refdef.predicting && !cl.refdef.demoplayback )
	{	
		// use predicted values
		int delta;

		backlerp = 1.0 - cl.refdef.lerpfrac;
		for( i = 0; i < 3; i++ )
		{
			cl.refdef.vieworg[i] = cl.predicted_origin[i] + ops->viewoffset[i] 
				+ cl.refdef.lerpfrac * (ps->viewoffset[i] - ops->viewoffset[i]) - backlerp * cl.prediction_error[i];
		}

		// smooth out stair climbing
		delta = cls.realtime - cl.predicted_step_time;
		if( delta < cl.serverframetime ) cl.refdef.vieworg[2] -= cl.predicted_step * (cl.serverframetime - delta) * 0.01f;
	}
}

/*
===============
V_ApplyRefDef

apply pre-calculated values
===============
*/
void V_AddViewModel( void )
{
	if( !cl.viewent.v.modelindex || cl.refdef.nextView ) return;
	re->AddRefEntity( &cl.viewent, ED_VIEWMODEL, cl.refdef.lerpfrac );
}

/*
===============
V_CalcRefDef

sets cl.refdef view values
===============
*/
void V_CalcRefDef( void )
{
	do
	{
		clgame.dllFuncs.pfnCalcRefdef( &cl.refdef );
		V_AddViewModel();
		re->RenderFrame( &cl.refdef );
	} while( cl.refdef.nextView );
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

	if( !cl.frame.valid )
	{
		SCR_FillRect( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, g_color_table[0] );
		return;
	}

	V_ClearScene ();
	CL_AddEntities ();

	V_SetupRefDef ();
	V_CalcRefDef ();
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