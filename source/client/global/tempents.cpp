//=======================================================================
//			Copyright XashXT Group 2008 ©
//		tempents.cpp - client side entity management functions
//=======================================================================

#include "extdll.h"
#include "hud_iface.h"

void HUD_CreateEntities( void )
{
}

//======================
//	DRAW BEAM EVENT
//======================
void EV_DrawBeam( void )
{
	// special effect for displacer
	cl_entity_t *view = gEngfuncs.GetViewModel();

	if( view != NULL )
	{
		float life = 1.05;
		int m_iBeam = gEngfuncs.pEventAPI->EV_FindModelIndex( "sprites/plasma.spr" );

		gEngfuncs.pEfxAPI->R_BeamEnts(view->index | 0x1000, view->index | 0x2000, m_iBeam, life, 0.8, 0.5, 0.5, 0.6, 0, 10, 2, 10, 0);
		gEngfuncs.pEfxAPI->R_BeamEnts(view->index | 0x1000, view->index | 0x3000, m_iBeam, life, 0.8, 0.5, 0.5, 0.6, 0, 10, 2, 10, 0);
		gEngfuncs.pEfxAPI->R_BeamEnts(view->index | 0x1000, view->index | 0x4000, m_iBeam, life, 0.8, 0.5, 0.5, 0.6, 0, 10, 2, 10, 0);
	}
}

//======================
//	Eject Shell
//======================
void EV_EjectShell( const dstudioevent_t *event, edict_t *entity )
{
	vec3_t view_ofs, ShellOrigin, ShellVelocity, forward, right, up;
	vec3_t origin = entity->origin;
	vec3_t angles = entity->angles;
	
	float fR, fU;

          int shell = g_engfuncs.pEventAPI->EV_FindModelIndex (event->options);
	g_engfuncs.pEventAPI->EV_LocalPlayerViewheight( view_ofs );
	origin.z = origin.z - view_ofs[2];

	for(int j = 0; j < 3; j++ )
	{
		if(angles[j] < -180) angles[j] += 360; 
		else if(angles[j] > 180) angles[j] -= 360;
          }
          angles.x = -angles.x;
	AngleVectors( angles, forward, right, up );
          
	fR = gEngfuncs.pfnRandomFloat( 50, 70 );
	fU = gEngfuncs.pfnRandomFloat( 100, 150 );

	for (int i = 0; i < 3; i++ )
	{
		ShellVelocity[i] = p_velocity[i] + right[i] * fR + up[i] * fU + forward[i] * 25;
		ShellOrigin[i]   = origin[i] + view_ofs[i] + up[i] * -12 + forward[i] * 20 + right[i] * 4;
	}
	EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL );
}

void HUD_StudioEvent( const dstudioevent_t *event, edict_t *entity )
{
	switch( event->event )
	{
	case 5001:
		gEngfuncs.pEfxAPI->R_MuzzleFlash( (float *)&entity->attachment[0], atoi( event->options) );
		break;
	case 5011:
		gEngfuncs.pEfxAPI->R_MuzzleFlash( (float *)&entity->attachment[1], atoi( event->options) );
		break;
	case 5021:
		gEngfuncs.pEfxAPI->R_MuzzleFlash( (float *)&entity->attachment[2], atoi( event->options) );
		break;
	case 5031:
		gEngfuncs.pEfxAPI->R_MuzzleFlash( (float *)&entity->attachment[3], atoi( event->options) );
		break;
	case 5002:
		gEngfuncs.pEfxAPI->R_SparkEffect( (float *)&entity->attachment[0], atoi( event->options), -100, 100 );
		break;
	// Client side sound
	case 5004:		
		gEngfuncs.pfnPlaySoundByNameAtLocation( (char *)event->options, 1.0, (float *)&entity->attachment[0] );
		break;
	// Client side sound with random pitch
	case 5005:		
		gEngfuncs.pEventAPI->EV_PlaySound( entity->index, (float *)&entity->attachment[0], CHAN_WEAPON, (char *)event->options, gEngfuncs.pfnRandomFloat(0.7, 0.9), ATTN_NORM, 0, 85 + gEngfuncs.pfnRandomLong(0,0x1f) );
		break;
	// Special event for displacer
	case 5050:
		EV_DrawBeam();
		break;
	case 5060:
	          EV_EjectShell( event, entity );
		break;
	default:
		break;
	}
}