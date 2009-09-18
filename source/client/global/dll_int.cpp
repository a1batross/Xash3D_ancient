//=======================================================================
//			Copyright XashXT Group 2008 ©
//		          dll_int.cpp - dll entry points 
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "ref_params.h"
#include "studio_ref.h"
#include "hud.h"

cl_enginefuncs_t g_engfuncs;
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
	HUD_DrawNormalTriangles,
	HUD_DrawTransparentTriangles,
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
int CreateAPI( HUD_FUNCTIONS *pFunctionTable, cl_enginefuncs_t* pEngfuncsFromEngine )
{
	if( !pFunctionTable || !pEngfuncsFromEngine )
	{
		return FALSE;
	}

	// copy HUD_FUNCTIONS table to engine, copy engfuncs table from engine
	memcpy( pFunctionTable, &gFunctionTable, sizeof( HUD_FUNCTIONS ));
	memcpy( &g_engfuncs, pEngfuncsFromEngine, sizeof( cl_enginefuncs_t ));

	return TRUE;
}

int HUD_VidInit( void )
{
	gHUD.VidInit();

	return 1;
}

void HUD_Init( void )
{
	gHUD.Init();

	V_Init();
}

int HUD_Redraw( float flTime, int state )
{
	switch( state )
	{
	case CL_DISCONNECTED:
		break;
	case CL_LOADING:
		V_RenderPlaque();
		break;
	case CL_ACTIVE:
	case CL_PAUSED:
		gHUD.Redraw( flTime );
		DrawCrosshair();
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
	float	m_fLerp = GetLerpFrac();

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

	if( state->aiment != 0 )
		ent->v.aiment = GetEntityByIndex( state->aiment );
	else ent->v.aiment = NULL;

	// lerping some fields
	for( i = 0; i < 3; i++ )
	{
		ent->v.angles[i] = LerpAngle( prev->angles[i], state->angles[i], m_fLerp );
		ent->v.origin[i] = LerpPoint( prev->origin[i], state->origin[i], m_fLerp );
		ent->v.rendercolor[i] = LerpPoint( prev->rendercolor[i], state->rendercolor[i], m_fLerp );
	}

	// interpolate scale, renderamount etc
	ent->v.renderamt = LerpPoint( prev->renderamt, state->renderamt, m_fLerp );
	ent->v.scale = LerpPoint( prev->scale, state->scale, m_fLerp );

	if(!( ent->v.flags & EF_ANIMATE )) // auto-animation uses v.frame for hold ending frame
		ent->v.frame = LerpPoint( prev->frame, state->frame, m_fLerp ); // FIXME: this is really need ?

	switch( state->ed_type )
	{
	case ED_CLIENT:
		for( i = 0; i < 3; i++ )
		{
			ent->v.punchangle[i] = LerpAngle( prev->punch_angles[i], state->punch_angles[i], m_fLerp);
			ent->v.viewangles[i] = LerpAngle( prev->viewangles[i], state->viewangles[i], m_fLerp);
			ent->v.view_ofs[i] = LerpAngle( prev->viewoffset[i], state->viewoffset[i], m_fLerp);
		}
		ent->v.fov = LerpPoint( prev->fov, state->fov, m_fLerp ); 
		break;
	case ED_PORTAL:
	case ED_MOVER:
	case ED_BSPBRUSH:
		ent->v.movedir.BitsToDir( state->skin );
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
		ent->v.blending[i] = LerpPoint( prev->blending[i], state->blending[i], m_fLerp ); 
	for( i = 0; i < MAXSTUDIOCONTROLLERS; i++ )
		ent->v.controller[i] = LerpPoint( prev->controller[i], state->controller[i], m_fLerp ); 

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
	// no shutdown operations
}