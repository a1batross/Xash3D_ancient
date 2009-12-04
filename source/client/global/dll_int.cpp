//=======================================================================
//			Copyright XashXT Group 2008 ©
//		          dll_int.cpp - dll entry points 
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "ref_params.h"
#include "studio_ref.h"
#include "hud.h"
#include "r_particle.h"
#include "ev_hldm.h"

cl_enginefuncs_t g_engfuncs;
cl_globalvars_t  *gpGlobals;
CHud gHUD;

// main DLL entry point
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
	return TRUE;
}

static HUD_FUNCTIONS gFunctionTable = 
{
	sizeof( HUD_FUNCTIONS ),
	HUD_VidInit,
	HUD_Init,
	HUD_Redraw,
	HUD_UpdateClientData,
	HUD_UpdateEntityVars,
	HUD_Reset,
	HUD_Frame,
	HUD_Shutdown,
	HUD_DrawTriangles,
	HUD_CreateEntities,
	HUD_StudioEvent,
	HUD_StudioFxTransform,
	V_CalcRefdef,
	V_StartPitchDrift,
	V_StopPitchDrift,
};

//=======================================================================
//			GetApi
//=======================================================================
int CreateAPI( HUD_FUNCTIONS *pFunctionTable, cl_enginefuncs_t* pEngfuncsFromEngine, cl_globalvars_t *pGlobals )
{
	if( !pFunctionTable || !pEngfuncsFromEngine )
	{
		return FALSE;
	}

	// copy HUD_FUNCTIONS table to engine, copy engfuncs table from engine
	memcpy( pFunctionTable, &gFunctionTable, sizeof( HUD_FUNCTIONS ));
	memcpy( &g_engfuncs, pEngfuncsFromEngine, sizeof( cl_enginefuncs_t ));
	gpGlobals = pGlobals;

	return TRUE;
}

int HUD_VidInit( void )
{
	if( g_pParticleSystems )
		g_pParticleSystems->ClearSystems();
	
	gHUD.VidInit();

	return 1;
}

void HUD_Init( void )
{
	g_engfuncs.pfnAddCommand ("noclip", NULL, "enable or disable no clipping mode" );
	g_engfuncs.pfnAddCommand ("notarget", NULL, "notarget mode (monsters do not see you)" );
	g_engfuncs.pfnAddCommand ("fullupdate", NULL, "re-init HUD on start demo recording" );
	g_engfuncs.pfnAddCommand ("give", NULL, "give specified item or weapon" );
	g_engfuncs.pfnAddCommand ("drop", NULL, "drop current/specified item or weapon" );
	g_engfuncs.pfnAddCommand ("intermission", NULL, "go to intermission" );
	g_engfuncs.pfnAddCommand ("spectate", NULL, "enable spectator mode" );
	g_engfuncs.pfnAddCommand ("gametitle", NULL, "show game logo" );
	g_engfuncs.pfnAddCommand ("god", NULL, "classic cheat" );
	g_engfuncs.pfnAddCommand ("fov", NULL, "set client field of view" );
	g_engfuncs.pfnAddCommand ("fly", NULL, "fly mode (flight)" );

          if ( g_pParticleSystems )
          {
          	// init partsystem
		delete g_pParticleSystems;
		g_pParticleSystems = NULL;
	}
	g_pParticleSystems = new ParticleSystemManager();

	gHUD.Init();

	V_Init();

	// link all events
	EV_HookEvents ();
}

int HUD_Redraw( float flTime, int state )
{
	static int oldstate;

	if( oldstate == CL_ACTIVE && state == CL_LOADING )
	{
		// fire only once to prevent multiply GL_BLEND each frame
		DrawImageBar( 100, "m_loading" ); // HACKHACK
	}
	oldstate = state;

	switch( state )
	{
	case CL_LOADING:
		DrawProgressBar();
		break;
	case CL_ACTIVE:
	case CL_PAUSED:
		gHUD.Redraw( flTime );
		DrawPause();
		break;
	}
	return 1;
}

int HUD_UpdateClientData( client_data_t *cdata, float flTime )
{
	return gHUD.UpdateClientData( cdata, flTime );
}

void HUD_UpdateEntityVars( edict_t *ent, skyportal_t *sky, const entity_state_t *state, const entity_state_t *prev )
{
	int	i;
	float	m_fLerp;

	if( state->ed_type == ED_CLIENT && state->ed_flags & ESF_NO_PREDICTION )
		m_fLerp = 1.0f;
	else m_fLerp = GetLerpFrac();

	// copy state to progs
	ent->v.modelindex = state->modelindex;
	ent->v.weaponmodel = state->weaponmodel;
	ent->v.frame = state->frame;
	ent->v.sequence = state->sequence;
	ent->v.gaitsequence = state->gaitsequence;
	ent->v.body = state->body;
	ent->v.skin = state->skin;
	ent->v.effects = state->effects;
	ent->v.velocity = state->velocity;
	ent->v.basevelocity = state->basevelocity;
	ent->v.oldorigin = ent->v.origin;
	ent->v.mins = state->mins;
	ent->v.maxs = state->maxs;
	ent->v.framerate = state->framerate;
	ent->v.colormap = state->colormap; 
	ent->v.rendermode = state->rendermode; 
	ent->v.renderfx = state->renderfx; 
	ent->v.fov = state->fov; 
	ent->v.scale = state->scale; 
	ent->v.weapons = state->weapons;
	ent->v.gravity = state->gravity;
	ent->v.health = state->health;
	ent->v.solid = state->solid;
	ent->v.movetype = state->movetype;
	ent->v.flags = state->flags;
	ent->v.ideal_pitch = state->idealpitch;
	ent->v.oldangles = ent->v.angles;	// just a delta between frames
	ent->v.animtime = state->animtime;

//	ent->v.animtime = bound( gpGlobals->time - gpGlobals->frametime, ent->v.animtime, gpGlobals->time );

	if( state->groundent != -1 )
		ent->v.groundentity = GetEntityByIndex( state->groundent );
	else ent->v.groundentity = NULL;

	if( state->aiment != -1 )
		ent->v.aiment = GetEntityByIndex( state->aiment );
	else ent->v.aiment = NULL;

	switch( ent->v.movetype )
	{
	case MOVETYPE_NONE:
		ent->v.origin = state->origin;
		break;
	default:
		ent->v.origin = LerpPoint( prev->origin, state->origin, m_fLerp );
		break;
	}

	// lerping some fields
	for( i = 0; i < 3; i++ )
	{
		ent->v.angles[i] = LerpAngle( prev->angles[i], state->angles[i], m_fLerp );
		ent->v.rendercolor[i] = LerpPoint( prev->rendercolor[i], state->rendercolor[i], m_fLerp );
	}

	// interpolate scale, renderamount etc
	ent->v.renderamt = LerpPoint( prev->renderamt, state->renderamt, m_fLerp );
	ent->v.scale = LerpPoint( prev->scale, state->scale, m_fLerp );

	if(!( ent->v.effects & EF_ANIMATE )) // auto-animation uses v.frame for hold ending frame
	{
		float	frame = state->frame;
		float	prevframe = prev->frame;
		float	lerpTime = (frame - prevframe);

		if( ent->v.animtime )
		{
			if( ent->v.effects & EF_NOINTERP )
			{
				// adjust lerping values if animation restarted
				if( lerpTime < 0 ) prevframe = 1.001;
				ent->v.frame = LerpPoint( prevframe, frame, m_fLerp );
			}
			else ent->v.frame = state->frame; // use normal studio lerping
		}
		else ent->v.frame = Q_rint( state->frame ); // sprite frames will be interpolated in other place
	}

	switch( state->ed_type )
	{
	case ED_CLIENT:
		for( i = 0; i < 3; i++ )
		{
			ent->v.punchangle[i] = LerpAngle( prev->punch_angles[i], state->punch_angles[i], m_fLerp);
			ent->v.viewangles[i] = LerpAngle( prev->viewangles[i], state->viewangles[i], m_fLerp);
			ent->v.view_ofs[i] = LerpAngle( prev->viewoffset[i], state->viewoffset[i], m_fLerp);
		}

		if( prev->fov < 90 && state->fov == 90 ) ent->v.fov = state->fov; // fov is reset, so don't lerping
		else ent->v.fov = LerpPoint( prev->fov, state->fov, m_fLerp ); 
		if( ent == GetLocalPlayer())
		{
			edict_t	*viewent = GetViewModel();

			// setup player viewmodel (only for local player!)
			viewent->v.modelindex = state->viewmodel;
			gHUD.m_flFOV = ent->v.fov; // keep client fov an actual 
		}
		break;
	case ED_PORTAL:
	case ED_MOVER:
	case ED_BSPBRUSH:
		ent->v.movedir = BitsToDir( state->body );
		ent->v.oldorigin = state->oldorigin;
		break;
	case ED_SKYPORTAL:
		// setup sky portal
		sky->vieworg = ent->v.origin; 
		sky->viewanglesOffset.x = sky->viewanglesOffset.z = 0.0f;
		sky->viewanglesOffset.y = gHUD.m_flTime * ent->v.angles[1];
		sky->fov = ent->v.fov;
		sky->scale = (ent->v.scale ? 1.0f / ent->v.scale : 0);	// critical stuff
		break;
	default:
		ent->v.movedir = Vector( 0, 0, 0 );
		break;
	}

	for( i = 0; i < MAXSTUDIOBLENDS; i++ )
	{
		ent->v.blending[i] = LerpByte( prev->blending[i], state->blending[i], m_fLerp ); 
	}
	for( i = 0; i < MAXSTUDIOCONTROLLERS; i++ )
	{
		if( abs( prev->controller[i] - state->controller[i] ) > 128 )
			ent->v.controller[i] = state->controller[i];
		else ent->v.controller[i] = LerpByte( prev->controller[i], state->controller[i], m_fLerp );	
	}
	// g-cont. moved here because we may needs apply null scale to skyportal
	if( ent->v.scale == 0.0f ) ent->v.scale = 1.0f;	
	ent->v.pContainingEntity = ent;
}

void HUD_Reset( void )
{
	gHUD.VidInit();
}

void HUD_Frame( double time )
{
	// place to call vgui_frame
}

void HUD_Shutdown( void )
{
	// perform shutdown operations
	g_engfuncs.pfnDelCommand ("noclip" );
	g_engfuncs.pfnDelCommand ("notarget" );
	g_engfuncs.pfnDelCommand ("fullupdate" );
	g_engfuncs.pfnDelCommand ("give" );
	g_engfuncs.pfnDelCommand ("drop" );
	g_engfuncs.pfnDelCommand ("intermission" );
	g_engfuncs.pfnDelCommand ("spectate" );
	g_engfuncs.pfnDelCommand ("gametitle" );
	g_engfuncs.pfnDelCommand ("god" );
	g_engfuncs.pfnDelCommand ("fov" );
	g_engfuncs.pfnDelCommand ("fly" );
}