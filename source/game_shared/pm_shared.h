//=======================================================================
//			Copyright XashXT Group 2009 ©
//		     pm_shared.h - pmove general interface
//=======================================================================
#ifndef PM_SHARED_H
#define PM_SHARED_H

typedef struct playermove_s	playermove_t;

void PM_Init( playermove_t *ppmove );
void PM_Move( playermove_t *ppmove, int server );
char PM_FindTextureType( const char *name );

// spectator Movement modes (stored in pev->iuser1, so the physics code can get at them)
#define OBS_NONE		0
#define OBS_CHASE_LOCKED	1
#define OBS_CHASE_FREE	2
#define OBS_ROAMING		3		
#define OBS_IN_EYE		4
#define OBS_MAP_FREE	5
#define OBS_MAP_CHASE	6

#endif//PM_SHARED_H