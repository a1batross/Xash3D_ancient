//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       ev_common.cpp - events common code
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "ev_hldm.h"
#include "effects_api.h"

extern "C"
{
	void EV_EjectBrass( event_args_t *args );
	void EV_FireNull( event_args_t *args );
}

//======================
//    Game_HookEvents
//======================
void EV_HookEvents( void )
{
	g_engfuncs.pEventAPI->EV_HookEvent( "evEjectBrass", EV_EjectBrass );
}

//=================
// EV_CreateTracer
//=================
void EV_CreateTracer( float *start, float *end )
{
	g_engfuncs.pEfxAPI->R_TracerEffect( start, end );
}


//=================
//   EV_IsPlayer
//=================
int EV_IsPlayer( int idx )
{
	if ( idx >= 1 && idx <= gpGlobals->maxClients )
		return true;
	return false;
}


//=================
//     EV_IsLocal
//=================
int EV_IsLocal( int idx )
{
	return g_engfuncs.pEventAPI->EV_IsLocal( idx - 1 ) ? true : false;
}


//=================
//  EV_GetGunPosition
//=================
void EV_GetGunPosition( event_args_t *args, float *pos, float *origin )
{
	int	idx;
	Vector	view_ofs;

	idx = args->entindex;

	view_ofs = Vector( 0, 0, 0 );
	view_ofs.z = DEFAULT_VIEWHEIGHT;

	if ( EV_IsPlayer( idx ))
	{
		// in spec mode use entity viewheigh, not own
		if ( EV_IsLocal( idx ))
		{
			// Grab predicted result for local player
			g_engfuncs.pEventAPI->EV_LocalPlayerViewheight( view_ofs );
		}
		else if ( args->ducking == 1 )
		{
			view_ofs[2] = VEC_DUCK_VIEW;
		}
	}

	pos[0] = origin[0] + view_ofs.x;
	pos[1] = origin[1] + view_ofs.y;
	pos[2] = origin[2] + view_ofs.z;
}


//=================
//  EV_EjectBrass
//=================
void EV_EjectBrass( float *origin, float *velocity, float rotation, int model, int soundtype )
{
	Vector endpos = Vector( 0.0f, rotation, 0.0f );

	g_engfuncs.pEfxAPI->R_TempModel( origin, velocity, endpos, RANDOM_LONG( 30, 50 ), model, soundtype );
}


//=================
// EV_GetDefaultShellInfo
//=================
void EV_GetDefaultShellInfo( event_args_t *args, float *origin, float *velocity, float *ShellVelocity, float *ShellOrigin, float *forward, float *right, float *up, float forwardScale, float upScale, float rightScale )
{
	int idx;
	Vector view_ofs;
	float fR, fU;

	idx = args->entindex;
	view_ofs = Vector( 0, 0, 0 );
	view_ofs.z = DEFAULT_VIEWHEIGHT;

	if ( EV_IsPlayer( idx ) )
	{
		if ( EV_IsLocal( idx ) )
		{
			g_engfuncs.pEventAPI->EV_LocalPlayerViewheight( view_ofs );
		}
		else if ( args->ducking == 1 )
		{
			view_ofs[2] = VEC_DUCK_VIEW;
		}
	}

	fR = RANDOM_FLOAT( 50, 70 );
	fU = RANDOM_FLOAT( 100, 150 );

	for ( int i = 0; i < 3; i++ )
	{
		ShellVelocity[i] = velocity[i] + right[i] * fR + up[i] * fU + forward[i] * 25;
		ShellOrigin[i]   = origin[i] + view_ofs[i] + up[i] * upScale + forward[i] * forwardScale + right[i] * rightScale;
	}
}

//=================
//  EV_MuzzleFlash
//=================
void EV_MuzzleFlash( void )
{
	// Add muzzle flash to current weapon model
	edict_t *ent = GetViewModel();
	if ( !ent ) return;

	// Or in the muzzle flash
	ent->v.effects |= EF_MUZZLEFLASH;
}

void HUD_CmdStart( const edict_t *player, int runfuncs )
{
}

void HUD_CmdEnd( const edict_t *player, const usercmd_t *cmd, unsigned int random_seed )
{
}