//=======================================================================
//			Copyright XashXT Group 2010 ©
//		    studio.cpp - client side studio callbacks
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "studio_event.h"
#include "effects_api.h"
#include "pm_movevars.h"
#include "r_tempents.h"
#include "ev_hldm.h"
#include "r_beams.h"
#include "hud.h"

//======================
//	DRAW BEAM EVENT
//======================
void EV_DrawBeam ( void )
{
	// special effect for displacer
	edict_t *view = GetViewModel();

	float life = 1.05;	// animtime
	int m_iBeam = g_engfuncs.pEventAPI->EV_FindModelIndex( "sprites/plasma.spr" );
	int idx = GetLocalPlayer()->serialnumber; // link with client

	g_pViewRenderBeams->CreateBeamEnts( idx | 0x1000, idx | 0x2000, m_iBeam, life, 0.8, 0.5, 127, 0.6, 0, 10, 20, 100, 0 );
	g_pViewRenderBeams->CreateBeamEnts( idx | 0x1000, idx | 0x3000, m_iBeam, life, 0.8, 0.5, 127, 0.6, 0, 10, 20, 100, 0 );
	g_pViewRenderBeams->CreateBeamEnts( idx | 0x1000, idx | 0x4000, m_iBeam, life, 0.8, 0.5, 127, 0.6, 0, 10, 20, 100, 0 );
}

//======================
//	Eject Shell
//======================
void EV_EjectShell( const dstudioevent_t *event, edict_t *entity )
{
	vec3_t view_ofs, ShellOrigin, ShellVelocity, forward, right, up;
	vec3_t origin = entity->v.origin;
	vec3_t angles = entity->v.angles;
	vec3_t velocity = entity->v.velocity;
	
	float fR, fU;

          int shell = g_engfuncs.pEventAPI->EV_FindModelIndex( event->options );
	origin.z = origin.z - entity->v.view_ofs[2];

	for( int j = 0; j < 3; j++ )
	{
		if( angles[j] < -180 )
			angles[j] += 360; 
		else if( angles[j] > 180 )
			angles[j] -= 360;
          }

	angles.x = -angles.x;
	AngleVectors( angles, forward, right, up );
          
	fR = RANDOM_FLOAT( 50, 70 );
	fU = RANDOM_FLOAT( 100, 150 );

	for ( int i = 0; i < 3; i++ )
	{
		ShellVelocity[i] = velocity[i] + right[i] * fR + up[i] * fU + forward[i] * 25;
		ShellOrigin[i]   = origin[i] + view_ofs[i] + up[i] * -12 + forward[i] * 20 + right[i] * 4;
	}
	EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL );
}

void HUD_StudioEvent( const dstudioevent_t *event, edict_t *entity )
{
	float	pitch;
	Vector	pos;

	switch( event->event )
	{
	case 5001:
		// MullzeFlash at attachment 1
		g_pTempEnts->MuzzleFlash( entity, 1, atoi( event->options ));
		break;
	case 5011:
		// MullzeFlash at attachment 2
		g_pTempEnts->MuzzleFlash( entity, 2, atoi( event->options ));
		break;
	case 5021:
		// MullzeFlash at attachment 3
		g_pTempEnts->MuzzleFlash( entity, 3, atoi( event->options ));
		break;
	case 5031:
		// MullzeFlash at attachment 4
		g_pTempEnts->MuzzleFlash( entity, 4, atoi( event->options ));
		break;
	case 5002:
		// SparkEffect at attachment 1
		break;
	case 5004:		
		// Client side sound
		GET_ATTACHMENT( entity, 1, pos, NULL ); 
		CL_PlaySound( event->options, 1.0f, pos );
		break;
	case 5005:		
		// Client side sound with random pitch
		pitch = 85 + RANDOM_LONG( 0, 0x1F );
		GET_ATTACHMENT( entity, 1, pos, NULL ); 
		CL_PlaySound( event->options, RANDOM_FLOAT( 0.7f, 0.9f ), pos, pitch );
		break;
	case 5050:
		// Special event for displacer
		EV_DrawBeam ();
		break;
	case 5060:
		EV_EjectShell( event, entity );
		break;
	default:
		ALERT( at_console, "Unhandled client-side attachment %i ( %s )\n", event->event, event->options );
		break;
	}
}

void HUD_StudioFxTransform( edict_t *ent, float transform[4][4] )
{
	switch( ent->v.renderfx )
	{
	case kRenderFxDistort:
	case kRenderFxHologram:
		if(!RANDOM_LONG( 0, 49 ))
		{
			int	axis = RANDOM_LONG( 0, 1 );
			float	scale = RANDOM_FLOAT( 1, 1.484 );

			if( axis == 1 ) axis = 2; // choose between x & z
			transform[axis][0] *= scale;
			transform[axis][1] *= scale;
			transform[axis][2] *= scale;
		}
		else if(!RANDOM_LONG( 0, 49 ))
		{
			float offset;
			int axis = RANDOM_LONG( 0, 1 );
			if( axis == 1 ) axis = 2; // choose between x & z
			offset = RANDOM_FLOAT( -10, 10 );
			transform[RANDOM_LONG( 0, 2 )][3] += offset;
		}
		break;
	case kRenderFxExplode:
		float	scale;

		scale = 1.0f + (gHUD.m_flTime - ent->v.animtime) * 10.0;
		if( scale > 2 ) scale = 2; // don't blow up more than 200%
		
		transform[0][1] *= scale;
		transform[1][1] *= scale;
		transform[2][1] *= scale;
		break;
	}
}

int HUD_StudioDoInterp( edict_t *e )
{
	if( r_studio_lerping->integer )
	{
		return (e->v.flags & EF_NOINTERP) ? false : true;
	}
	return false;
}