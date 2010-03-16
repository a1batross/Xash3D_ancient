//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       ev_common.cpp - events common code
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "ev_hldm.h"
#include "r_tempents.h"

extern "C"
{
	void EV_EjectBrass( event_args_t *args );
	void EV_FireNull( event_args_t *args );
	void EV_FireCrowbar( event_args_t *args );
	void EV_PlayEmptySound( event_args_t *args );
	void EV_FireGlock1( event_args_t *args  );
	void EV_FireMP5( event_args_t *args  );
	void EV_FirePython( event_args_t *args  );
	void EV_FireGauss( event_args_t *args  );
	void EV_SpinGauss( event_args_t *args  );
	void EV_EgonFire( event_args_t *args );
	void EV_EgonStop( event_args_t *args );
	void EV_FireShotGunSingle( event_args_t *args  );
	void EV_FireShotGunDouble( event_args_t *args  );
	void EV_SnarkFire( event_args_t *args );
	void EV_TrainPitchAdjust( event_args_t *args );
	void EV_Decals( event_args_t *args );
}

//======================
//    Game_HookEvents
//======================
void EV_HookEvents( void )
{
	g_engfuncs.pEventAPI->EV_HookEvent( "evEjectBrass", EV_EjectBrass );
	g_engfuncs.pEventAPI->EV_HookEvent( "evNull", EV_FireNull );
	g_engfuncs.pEventAPI->EV_HookEvent( "evCrowbar", EV_FireCrowbar );
	g_engfuncs.pEventAPI->EV_HookEvent( "evEmptySound", EV_PlayEmptySound );
	g_engfuncs.pEventAPI->EV_HookEvent( "evGlock1", EV_FireGlock1 );
	g_engfuncs.pEventAPI->EV_HookEvent( "evShotgun1", EV_FireShotGunSingle );
	g_engfuncs.pEventAPI->EV_HookEvent( "evShotgun2", EV_FireShotGunDouble );
	g_engfuncs.pEventAPI->EV_HookEvent( "evMP5", EV_FireMP5 );
	g_engfuncs.pEventAPI->EV_HookEvent( "evPython", EV_FirePython );
	g_engfuncs.pEventAPI->EV_HookEvent( "evGauss", EV_FireGauss );
	g_engfuncs.pEventAPI->EV_HookEvent( "evGaussSpin", EV_SpinGauss );
	g_engfuncs.pEventAPI->EV_HookEvent( "evEgonFire", EV_EgonFire );
	g_engfuncs.pEventAPI->EV_HookEvent( "evEgonStop", EV_EgonStop );
	g_engfuncs.pEventAPI->EV_HookEvent( "evTrain", EV_TrainPitchAdjust );
	g_engfuncs.pEventAPI->EV_HookEvent( "evSnarkFire", EV_SnarkFire );
	g_engfuncs.pEventAPI->EV_HookEvent( "evDecals", EV_Decals );
}

//=================
// EV_CreateTracer
//=================
void EV_CreateTracer( float *start, float *end )
{
	g_pTempEnts->TracerEffect( start, end );
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

	g_pTempEnts->TempModel( origin, velocity, endpos, RANDOM_LONG( 30, 50 ), model, soundtype );
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
		ShellOrigin[i] = origin[i] + view_ofs[i] + up[i] * upScale + forward[i] * forwardScale + right[i] * rightScale;
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

//=================
//  EV_FlashLight
//=================
void EV_UpadteFlashlight( edict_t *pEnt )
{
	Vector vecSrc, vecEnd, vecPos, forward;
	float rgba[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	TraceResult tr;

	AngleVectors( pEnt->v.viewangles, forward, NULL, NULL );
	vecSrc = pEnt->v.origin +pEnt->v.view_ofs;
	vecEnd = vecSrc + forward * 256;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, pEnt, &tr );

	if( tr.flFraction != 1.0f )
		vecPos = tr.vecEndPos + (tr.vecPlaneNormal * -16.0f);
	else vecPos = tr.vecEndPos;

	// update flashlight endpos
	g_engfuncs.pEfxAPI->CL_AllocDLight( vecPos, rgba, 96, 0.001f, 0, pEnt->serialnumber );
}

void HUD_CmdStart( const edict_t *player, int runfuncs )
{
}

void HUD_CmdEnd( const edict_t *player, const usercmd_t *cmd, unsigned int random_seed )
{
	// Offset final origin by view_offset
	if( cl_lw->integer )
	{
		previousorigin = player->v.origin + player->v.view_ofs;
	}
}