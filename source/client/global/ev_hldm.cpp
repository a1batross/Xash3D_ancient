//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       ev_common.cpp - events common code
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "ev_hldm.h"
#include "bullets.h"			// server\client shared enum

static int tracerCount[ 32 ];
extern char PM_FindTextureType( char *name );
void V_PunchAxis( int axis, float punch );
void VectorAngles( const float *forward, float *angles );

extern "C" void EV_EjectBrass( event_args_t *args );

//======================
//      ShellEject START
//======================
void EV_EjectBrass( event_args_t *args )
{
	int idx;
	int bullet;
	int firemode;
	Vector ShellVelocity;
	Vector ShellOrigin;
	Vector origin;
	Vector angles;
	Vector velocity;
	Vector up, right, forward;

	idx = args->entindex;
	origin = args->origin;
	angles = args->angles;
	velocity = args->velocity;
	bullet = args->iparam1;
	firemode = args->bparam1;
#if 0		
	AngleVectors( angles, forward, right, up );
	if ( EV_IsLocal( idx ) ) EV_MuzzleFlash();

	model_t	shell;

	switch(Bullet(bullet))
	{
	case BULLET_9MM: 	shell = g_engfuncs.pEventAPI->EV_FindModelIndex ("models/shell9mm.mdl"); break; 
	case BULLET_MP5:	shell = g_engfuncs.pEventAPI->EV_FindModelIndex ("models/shellMp5.mdl"); break;
	case BULLET_357:	shell = g_engfuncs.pEventAPI->EV_FindModelIndex ("models/shell357.mdl"); break;
	case BULLET_12MM:	shell = g_engfuncs.pEventAPI->EV_FindModelIndex ("models/shell762.mdl"); break;
	case BULLET_556:    shell = g_engfuncs.pEventAPI->EV_FindModelIndex ("models/shell556.mdl"); break;
	case BULLET_762:    shell = g_engfuncs.pEventAPI->EV_FindModelIndex ("models/shell762.mdl"); break;
	case BULLET_BUCKSHOT: shell = g_engfuncs.pEventAPI->EV_FindModelIndex ("models/shellBuck.mdl"); break;
	default:
	case BULLET_NONE: shell = 0; break;
	}

	if( bullet == BULLET_BUCKSHOT)
	     EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 32, -12, 6 );
	else EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4 );

	EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL ); 
#endif
}
//======================
//        ShellEject END
//======================