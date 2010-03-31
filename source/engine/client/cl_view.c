//=======================================================================
//			Copyright XashXT Group 2009 ©
//		    cl_view.c - player rendering positioning
//=======================================================================

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
	edict_t	*clent;

	clent = CL_GetLocalPlayer ();

	// UNDONE: temporary place for detect waterlevel
	CL_CheckWater( clent );

	VectorCopy( clent->v.punchangle, cl.refdef.punchangle );
	cl.refdef.movevars = &clgame.movevars;
	cl.refdef.onground = clent->v.groundentity;
	cl.refdef.areabits = cl.frame.areabits;
	cl.refdef.health = clent->v.health;
	cl.refdef.movetype = clent->v.movetype;
	cl.refdef.idealpitch = clent->v.ideal_pitch;
	cl.refdef.num_entities = clgame.globals->numEntities;
	cl.refdef.max_entities = clgame.globals->maxEntities;
	cl.refdef.maxclients = clgame.globals->maxClients;
	cl.refdef.time = cl.time * 0.001f;
	cl.refdef.frametime = cls.frametime;
	cl.refdef.demoplayback = cls.demoplayback;
	cl.refdef.smoothing = CL_IsPredicted() ? false : true;
	cl.refdef.waterlevel = clent->v.waterlevel;		
	cl.refdef.flags = cl.render_flags;
	cl.refdef.viewsize = 120; // FIXME if you can
	cl.refdef.nextView = 0;

	// calculate the origin
	if( CL_IsPredicted( ) && !cl.refdef.demoplayback )
	{	
		// use predicted values
		uint	i, delta;
		float	backlerp = 1.0f - cl.lerpFrac;

		for( i = 0; i < 3; i++ )
		{
			cl.refdef.simorg[i] = cl.predicted_origin[i] - backlerp * cl.prediction_error[i];
			cl.refdef.viewheight[i] = cl.predicted_viewofs[i] - backlerp * cl.prediction_error[i];
                    }

		// smooth out stair climbing
		delta = cls.realtime - cl.predicted_step_time;
		if( delta < 150 )
			cl.refdef.simorg[2] -= cl.predicted_step * (150 - delta) / 150;
		VectorCopy( cl.predicted_velocity, cl.refdef.simvel );
	}
	else
	{
		VectorCopy( clent->v.origin, cl.refdef.simorg );
		VectorCopy( clent->v.view_ofs, cl.refdef.viewheight );
		VectorCopy( clent->v.velocity, cl.refdef.simvel );
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
	if( cl.refdef.nextView ) return; // add viewmodel only at firstperson pass
	if( !cl.frame.valid || cl.refdef.paused ) return;

	clgame.dllFuncs.pfnAddVisibleEntity( &clgame.viewent, ED_VIEWMODEL );
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

	// update cl_globalvars
	clgame.globals->time = cl.time * 0.001f; // clamped
	clgame.globals->frametime = cls.frametime; // used by input code

	if( cl.frame.valid && (cl.force_refdef || !cl.refdef.paused ))
	{
		cl.force_refdef = false;

		V_ClearScene ();
		CL_AddEntities ();
		V_SetupRefDef ();
	}

	V_CalcRefDef ();
}

/*
==================
V_PreRender

==================
*/
bool V_PreRender( void )
{
	bool	clearScene = true;

	// too early
	if( !re ) return false;

	if( host.state == HOST_NOFOCUS )
		return false;
	if( host.state == HOST_SLEEP )
		return false;

	if( cl.refdef.paused || !cls.drawplaque )
		clearScene = false;

	re->BeginFrame( clearScene );
	return cls.drawplaque;
}

/*
==================
V_PostRender

==================
*/
void V_PostRender( void )
{
	if( cls.scrshot_action == scrshot_inactive )
	{
		SCR_RSpeeds();
		SCR_DrawNet();
		SCR_DrawFPS();
		UI_UpdateMenu( cls.realtime );
		Con_DrawConsole();
	}
	SCR_MakeScreenShot();
	re->EndFrame();
}