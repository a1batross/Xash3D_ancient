//=======================================================================
//			Copyright XashXT Group 2009 ©
//		        pm_defs.h - pmove general structs
//=======================================================================
#ifndef PM_DEFS_H
#define PM_DEFS_H

#define MAX_PHYSENTS		128   	// max touchentities
#define MAX_LADDERS			128
#define MAX_CLIP_PLANES		5

#define PM_NORMAL			0	// normal trace
#define PM_NOMONSTERS		1	// Ignore monsters (studiomodels)
#define PM_WORLD_ONLY		2	// Only trace against the world
#define PM_GLASS_IGNORE		3	// Ignore entities with non-normal rendermode

#include "event_api.h"
#include "entity_def.h"
#include "trace_def.h"
#include "usercmd.h"

typedef struct playermove_s
{
	// Global Input Values
	BOOL		server;		// for debugging, are we running physics code on server side?
	BOOL		multiplayer;	// 1 == multiplayer server
	float		realtime;		// realtime on host, for reckoning duck timing
	float		frametime;	// Duration of this frame
	BOOL		runfuncs;		// run functions for this frame?
	movevars_t	*movevars;	// shared movement variables
	char		physinfo[512];	// Physics info string
	int		serverflags;	// shared serverflags

	// constants (came from gameinfo.txt)
	vec3_t		player_mins[PM_MAXHULLS];
	vec3_t		player_maxs[PM_MAXHULLS];
	float		player_viewheight;	// from gameinfo.txt

	// just a list of ladders
	edict_t		*ladders[MAX_LADDERS];	// filled by engine
	int		numladders;

	// (Input) to run through physics.
	usercmd_t		cmd;

	// (Input\Output) player state
	edict_t		*player;		// pev = &pmove->player->v;

	// (Output) Trace results for objects we collided with.
	edict_t		*touchents[MAX_PHYSENTS];
	vec3_t		touchvels[MAX_PHYSENTS];	// touch velocities for ents
	int		numtouch;
	
	// Local values
	vec3_t		forward;		// Vectors for angles
	vec3_t		right;
	vec3_t		up;

	// player state
	vec3_t		origin;		// Movement origin.
	vec3_t		angles;		// Movement view angles.
	vec3_t		oldangles;	// Angles before movement view angles were looked at.
	
	BOOL		dead;		// Are we a dead player?
	float		maxspeed;		// current maxspeed
	float		clientmaxspeed;	// current client maxspeed
	int		oldwaterlevel;	// holds the last waterlevel
	float		flWaterJumpTime;	// holds in teleport_time
	char		szTextureName[256];	// current texname
	char		chTextureType;	// current textype
	edict_t		*onground;	// groundentity
	int		usehull;		// 0 = regular player hull, 1 = ducked player hull, 2 = point hull
	
	// common api functions
	const char	*(*PM_Info_ValueForKey)( const char *s, const char *key );
	edict_t		*(*PM_TestPlayerPosition)( const float *pos, TraceResult *trace );
	void		(*ClientPrintf)( int idx, char *fmt, ... );
	void		(*AlertMessage)( ALERT_TYPE level, char *fmt, ... );
	const char	*(*PM_GetString)( string_t iString );	// used for reading string_t edict fields
	int		(*PM_PointContents)( const float *p );
	TraceResult	(*PM_PlayerTrace)( const float *start, const float *end, int trace_type );
	const char	*(*PM_TraceTexture)( edict_t *pTextureEntity, const float *v1, const float *v2 );
	edict_t		*(*PM_GetEntityByIndex)( int entIndex ); // use VIEWENT_INDEX to get weapon entity
	void		(*AngleVectors)( const float *rgflVector, float *forward, float *right, float *up );
	long		(*RandomLong)( long lLow, long lHigh );
	float		(*RandomFloat)( float flLow, float flHigh );
	int		(*PM_GetModelType)( model_t handle );
	void		(*PM_GetModelBounds)( model_t handle, float *mins, float *maxs );
	void		*(*PM_ModExtradata)( model_t handle );
	TraceResult	(*PM_TraceModel)( edict_t *pEnt, const float *start, const float *end );
	byte		*(*COM_LoadFile)( const char *filepath, int *pLength );
	char		*(*COM_ParseToken)( const char **data_p );
	void		(*COM_FreeFile)( void *buffer );
	char		*(*memfgets)( byte *pMemFile, int fileSize, int *pFilePos, char *pBuffer, int bufSize );
	void		(*PM_PlaySound)( int chan, const char *sample, float vol, float attn, int pitch );
} playermove_t;

#endif//PM_DEFS_H