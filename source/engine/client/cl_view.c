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
===============
V_SetupRefDef

update refdef values each frame
===============
*/
void V_SetupRefDef( void )
{
	edict_t		*clent;

	clent = EDICT_NUM( cl.playernum + 1 );

	// UNDONE: temporary place for detect waterlevel
	CL_CheckWater( clent );

	VectorCopy( clent->v.velocity, cl.refdef.simvel );
	VectorCopy( clent->v.origin, cl.refdef.simorg );
	VectorCopy( clent->v.view_ofs, cl.refdef.viewheight );
	VectorCopy( clent->v.punchangle, cl.refdef.punchangle );

	cl.refdef.movevars = &clgame.movevars;

	if( clent->v.flags & FL_ONGROUND )
		cl.refdef.onground = clent->v.groundentity;
	else cl.refdef.onground = NULL;

	cl.refdef.areabits = cl.frame.areabits;
	cl.refdef.health = clent->v.health;
	cl.refdef.movetype = clent->v.movetype;
	cl.refdef.idealpitch = clent->v.ideal_pitch;
	cl.refdef.num_entities = clgame.numEntities;
	cl.refdef.max_entities = clgame.maxEntities;
	cl.refdef.maxclients = clgame.maxClients;
	cl.refdef.time = cl.time * 0.001f;
	cl.refdef.frametime = cls.frametime;
	cl.refdef.demoplayback = cls.demoplayback;
	cl.refdef.paused = cl_paused->integer;
	cl.refdef.smoothing = cl_predict->integer;
	cl.refdef.waterlevel = clent->v.waterlevel;		
	cl.refdef.flags = cl.render_flags;
	cl.refdef.nextView = 0;

	// calculate the origin
	if( cl.refdef.smoothing && !cl.refdef.demoplayback )
	{	
		// use predicted values
		int	i, delta;
		float	backlerp = 1.0 - cl.lerpFrac;

		for( i = 0; i < 3; i++ )
			cl.refdef.vieworg[i] = cl.predicted_origin[i] + cl.refdef.viewheight[i] - backlerp * cl.prediction_error[i];

		// smooth out stair climbing
		delta = cls.realtime - cl.predicted_step_time;
		if( delta < cl.serverframetime )
			cl.refdef.vieworg[2] -= cl.predicted_step * ((cl.serverframetime - delta) / (float)cl.serverframetime);
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
	if( cl.viewent.v.modelindex && !cl.refdef.nextView )
		re->AddRefEntity( &cl.viewent, ED_VIEWMODEL );
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
	SCR_RSpeeds();
	SCR_DrawNet();
	SCR_DrawFPS();
	UI_UpdateMenu( cls.realtime );
	Con_DrawConsole();
	re->EndFrame();
}