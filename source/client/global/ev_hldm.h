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
#define VEC_HULL_MIN	gpGlobals->hullmins[0]
#define VEC_HULL_MAX	gpGlobals->hullmaxs[0]
#define VEC_DUCK_HULL_MIN	gpGlobals->hullmins[1]
#define VEC_DUCK_HULL_MAX	gpGlobals->hullmaxs[1]


enum crowbar_e
{
	CROWBAR_IDLE = 0,
	CROWBAR_DRAW,
	CROWBAR_HOLSTER,
	CROWBAR_ATTACK1HIT,
	CROWBAR_ATTACK1MISS,
	CROWBAR_ATTACK2MISS,
	CROWBAR_ATTACK2HIT,
	CROWBAR_ATTACK3MISS,
	CROWBAR_ATTACK3HIT
};

enum shotgun_e
{
	SHOTGUN_IDLE = 0,
	SHOTGUN_DRAW,
	SHOTGUN_HOLSTER,
	SHOTGUN_FIRE,
	SHOTGUN_FIRE2,
	SHOTGUN_START_RELOAD,
	SHOTGUN_RELOAD,
	SHOTGUN_PUMP
};

enum glock_e
{
	GLOCK_IDLE1 = 0,
	GLOCK_IDLE2,
	GLOCK_IDLE3,
	GLOCK_DRAW,
	GLOCK_HOLSTER,
	GLOCK_SHOOT,
	GLOCK_SHOOT_EMPTY,
	GLOCK_RELOAD,
	GLOCK_RELOAD_NOT_EMPTY,
	GLOCK_HOLSTER2,
	GLOCK_ADD_SILENCER,
	GLOCK_DEL_SILENCER
};

enum mp5_e
{
	MP5_IDLE = 0,
	MP5_DEPLOY,
	MP5_HOLSTER,
	MP5_FIRE1,
	MP5_FIRE2,
	MP5_FIRE3,
	MP5_LAUNCH,
	MP5_RELOAD
};

enum python_e
{
	PYTHON_IDLE1 = 0,
	PYTHON_FIDGET,
	PYTHON_FIRE1,
	PYTHON_RELOAD,
	PYTHON_HOLSTER,
	PYTHON_DRAW,
	PYTHON_IDLE2,
	PYTHON_IDLE3
};

enum gauss_e
{
	GAUSS_IDLE = 0,
	GAUSS_IDLE2,
	GAUSS_FIDGET,
	GAUSS_SPINUP,
	GAUSS_SPIN,
	GAUSS_FIRE,
	GAUSS_FIRE2,
	GAUSS_HOLSTER,
	GAUSS_DRAW
};

enum egon_e
{
	EGON_IDLE1 = 0,
	EGON_FIDGET1,
	EGON_ALTFIREON,
	EGON_ALTFIREOFF,
	EGON_FIRESTOP,
	EGON_FIRECYCLE,
	EGON_DRAW,
	EGON_HOLSTER
};

enum squeak_e
{
	SQUEAK_IDLE1 = 0,
	SQUEAK_FIDGETFIT,
	SQUEAK_FIDGETNIP,
	SQUEAK_DOWN,
	SQUEAK_UP,
	SQUEAK_THROW
};

extern Vector previousorigin;

void EV_HookEvents( void );
extern void HUD_CmdStart( const edict_t *player, int runfuncs );
extern void HUD_CmdEnd( const edict_t *player, const usercmd_t *cmd, unsigned int random_seed );

void EV_HLDM_GunshotDecalTrace( TraceResult *pTrace, char *decalName );
void EV_HLDM_DecalGunshot( TraceResult *pTrace, int iBulletType );
int EV_HLDM_CheckTracer( int idx, float *vecSrc, float *end, float *forward, float *right, int iBulletType, int iTracerFreq, int *tracerCount );
void EV_HLDM_FireBullets( int idx, float *forward, float *right, float *up, int cShots, float *vecSrc, float *vecDirShooting, float flDistance, int iBulletType, int iTracerFreq, int *tracerCount, float flSpreadX, float flSpreadY );
void EV_UpdateLaserSpot( void );

int EV_IsLocal( int idx );
int EV_IsPlayer( int idx );
void EV_MuzzleFlash( void );
void EV_UpdateBeams ( void );
void EV_GetGunPosition( event_args_t *args, float *pos, float *origin );
void EV_CreateTracer( float *start, float *end );
void EV_EjectBrass( float *origin, float *velocity, float rotation, int model, int soundtype );
void EV_GetDefaultShellInfo( event_args_t *args, float *origin, float *velocity, float *ShellVelocity, float *ShellOrigin, float *forward, float *right, float *up, float forwardScale, float upScale, float rightScale );

#endif // EV_HLDMH