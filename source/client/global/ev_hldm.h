//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#if !defined ( EV_HLDMH )
#define EV_HLDMH

#define DEFAULT_VIEWHEIGHT	gpGlobals->viewheight[0]
#define VEC_DUCK_VIEW	gpGlobals->viewheight[1]

void EV_HookEvents( void );
extern void HUD_CmdStart( const edict_t *player, int runfuncs );
extern void HUD_CmdEnd( const edict_t *player, const usercmd_t *cmd, unsigned int random_seed );


void EV_HLDM_GunshotDecalTrace( TraceResult *pTrace, char *decalName );
void EV_HLDM_DecalGunshot( TraceResult *pTrace, int iBulletType );
int EV_HLDM_CheckTracer( int idx, float *vecSrc, float *end, float *forward, float *right, int iBulletType, int iTracerFreq, int *tracerCount );
void EV_HLDM_FireBullets( int idx, float *forward, float *right, float *up, int cShots, float *vecSrc, float *vecDirShooting, float flDistance, int iBulletType, int iTracerFreq, int *tracerCount, float flSpreadX, float flSpreadY );

int EV_IsLocal( int idx );
int EV_IsPlayer( int idx );
void EV_MuzzleFlash( void );
void EV_GetGunPosition( event_args_t *args, float *pos, float *origin );
void EV_CreateTracer( float *start, float *end );
void EV_EjectBrass( float *origin, float *velocity, float rotation, int model, int soundtype );
void EV_GetDefaultShellInfo( event_args_t *args, float *origin, float *velocity, float *ShellVelocity, float *ShellOrigin, float *forward, float *right, float *up, float forwardScale, float upScale, float rightScale );

#endif // EV_HLDMH