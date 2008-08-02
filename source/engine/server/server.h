/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// server.h
#ifndef SERVER_H
#define SERVER_H

#include "mathlib.h"
#include "sv_edict.h"

//=============================================================================

#define MAX_MASTERS			8 	// max recipients for heartbeat packets
#define LATENCY_COUNTS		16
#define MAX_ENT_CLUSTERS		16
#define DF_NO_FRIENDLY_FIRE		0x00000001		//FIXME: move to server.dat

// classic quake flags
#define SPAWNFLAG_NOT_EASY		0x00000100
#define SPAWNFLAG_NOT_MEDIUM		0x00000200
#define SPAWNFLAG_NOT_HARD		0x00000400
#define SPAWNFLAG_NOT_DEATHMATCH	0x00000800

#define AI_FLY				(1<<0)		// monster is flying
#define AI_SWIM				(1<<1)		// swimming monster
#define AI_ONGROUND				(1<<2)		// monster is onground
#define AI_PARTIALONGROUND			(1<<3)		// monster is partially onground
#define AI_GODMODE				(1<<4)		// monster don't give damage at all
#define AI_NOTARGET				(1<<5)		// monster will no searching enemy's
#define AI_NOSTEP				(1<<6)		// Lazarus stuff
#define AI_DUCKED				(1<<7)		// monster (or player) is ducked
#define AI_JUMPING				(1<<8)		// monster (or player) is jumping
#define AI_FROZEN				(1<<9)		// stop moving, but continue thinking
#define AI_ACTOR                		(1<<10)		// disable ai for actor
#define AI_DRIVER				(1<<11)		// npc or player driving vehcicle or train
#define AI_SPECTATOR			(1<<12)		// spectator mode for clients
#define AI_WATERJUMP			(1<<13)		// npc or player take out of water

typedef enum
{
	ss_dead,		// no map loaded
	ss_loading,	// spawning level edicts
	ss_active,	// actively running
	ss_cinematic
} sv_state_t;

typedef enum
{
	cs_free,		// can be reused for a new connection
	cs_zombie,	// client has been disconnected, but don't reuse connection for a couple seconds
	cs_connected,	// has been assigned to a sv_client_t, but not in game yet
	cs_spawned	// client is fully in game

} cl_state_t;

typedef struct server_s
{
	sv_state_t	state;		// precache commands are only valid during load

	bool		loadgame;		// client begins should reuse existing entity

	float		time;		// always sv.framenum * 50 msec
	float		frametime;
	int		framenum;

	char		name[MAX_QPATH];	// map name, or cinematic name
	cmodel_t		*models[MAX_MODELS];
	cmodel_t		*worldmodel;

	char		configstrings[MAX_CONFIGSTRINGS][MAX_QPATH];

	// the multicast buffer is used to send a message to a set of clients
	// it is only used to marshall data until SV_Message is called
	sizebuf_t		multicast;
	byte		multicast_buf[MAX_MSGLEN];

	int		lastchecktime;
	int		lastcheck;

	bool		autosaved;
} server_t;

typedef struct
{
	entity_state_t	ps;
	int  		areabytes;
	byte 		areabits[MAX_MAP_AREAS/8];	// portalarea visibility bits
	int  		num_entities;
	int  		first_entity;		// into the circular sv_packet_entities[]
	int		msg_sent;			// time the message was transmitted
	int		msg_size;			// used to rate drop packets
	int		latency;			// message latency time
} client_frame_t;

typedef struct sv_client_s
{
	cl_state_t	state;

	char		userinfo[MAX_INFO_STRING];	// name, etc
	int		lastframe;		// for delta compression
	usercmd_t		lastcmd;			// for filling in big drops
	usercmd_t		cmd;			// current user commands

	vec3_t		fix_angles;		// q1 legacy
	bool		fixangle;
						// commands exhaust it, assume time cheating
	int		ping;
	int		rate;
	int		surpressCount;		// number of messages rate supressed
	int		sendtime;			// time before send next packet

	edict_t		*edict;			// EDICT_NUM(clientnum+1)
	char		name[32];			// extracted from userinfo, high bits masked
	int		messagelevel;		// for filtering printed messages

	// The datagram is written to by sound calls, prints, temp ents, etc.
	// It can be harmlessly overflowed.
	sizebuf_t		datagram;
	byte		datagram_buf[MAX_MSGLEN];

	client_frame_t	frames[UPDATE_BACKUP];	// updates can be delta'd from here

	byte		*download;		// file being downloaded
	int		downloadsize;		// total bytes (can't use EOF because of paks)
	int		downloadcount;		// bytes sent

	int		skipframes;		// client synchronyze with phys frame
	int		lastmessage;		// sv.framenum when packet was last received
	int		lastconnect;

	int		challenge;		// challenge of this user, randomly generated

	netchan_t		netchan;
} sv_client_t;


typedef struct worldsector_s
{
	int			axis;		// -1 = leaf node
	float			dist;
	struct worldsector_s	*children[2];
	sv_edict_t		*entities;
} worldsector_t;

struct sv_edict_s
{
	// generic_edict_t (don't move these fields!)
	bool			free;
	float			freetime;	 	// sv.time when the object was freed

	// sv_private_edict_t
	worldsector_t		*worldsector;	// member of current wolrdsector
	struct sv_edict_s 		*nextedict;	// next edict in world sector
	struct sv_client_s		*client;		// filled for player ents
	int			clipmask;		// trace info
	int			lastcluster;	// unused if num_clusters != -1
	int			linkcount;
	int			num_clusters;	// if -1, use headnode instead
	int			clusternums[MAX_ENT_CLUSTERS];
	int			areanum, areanum2;
	bool			forceupdate;	// physic_push force update
	bool			suspended;	// suspended in air toss object

	vec3_t			water_origin;	// step old origin
	vec3_t			moved_origin;	// push old origin
	vec3_t			moved_angles;	// push old angles

	int			serialnumber;	// unical entity #id
	int			solid;		// see entity_state_t for details
	physbody_t		*physbody;	// ptr to phys body

	// baselines
	entity_state_t		s;		// this is a player_state too
};

/*
=============================================================================
 a client can leave the server in one of four ways:
 dropping properly by quiting or disconnecting
 timing out if no valid messages are received for timeout.value seconds
 getting kicked off by the server operator
 a program error, like an overflowed reliable buffer
=============================================================================
*/

// MAX_CHALLENGES is made large to prevent a denial
// of service attack that could cycle all of them
// out before legitimate users connected
#define	MAX_CHALLENGES	1024

typedef struct
{
	netadr_t		adr;
	int		challenge;
	int		time;
	bool		connected;
} challenge_t;

typedef struct
{
	bool		initialized;		// sv_init has completed
	dword		realtime;			// always increasing, no clamping, etc
	dword		timeleft;

	string		mapcmd;				// ie: *intro.cin+base 
	string		comment;			// map name, e.t.c. 

	int		spawncount;		// incremented each server start
						// used to check late spawns
	sv_client_t	*clients;			// [host_maxclients->integer]
	int		num_client_entities;	// host_maxclients->integer*UPDATE_BACKUP
	int		next_client_entities;	// next client_entity to use
	entity_state_t	*client_entities;		// [num_client_entities]
	entity_state_t	*baselines;		// [host.max_edicts]
	func_t		ClientMove;		// qc client physic

	int		last_heartbeat;

	challenge_t	challenges[MAX_CHALLENGES];	// to prevent invalid IPs from connecting
} server_static_t;

//=============================================================================

extern	netadr_t	master_adr[MAX_MASTERS];		// address of the master server

extern	server_static_t	svs;			// persistant server info
extern	server_t		sv;			// local server

extern	cvar_t		*sv_paused;
extern	cvar_t		*sv_noreload;		// don't reload level state when reentering
extern	cvar_t		*sv_airaccelerate;		// don't reload level state when reentering
extern	cvar_t		*sv_accelerate;
extern	cvar_t		*sv_friction;
extern	cvar_t		*sv_maxvelocity;
extern	cvar_t		*sv_gravity;
extern	cvar_t		*sv_fps;			// running server at
extern	cvar_t		*sv_enforcetime;
extern	cvar_t		*sv_reconnect_limit;
extern	cvar_t		*allow_download;
extern	cvar_t		*rcon_password;
extern	cvar_t		*sv_fatpvs;
extern	cvar_t		*hostname;
extern	cvar_t		*sv_stepheight;
extern	cvar_t		*sv_playersonly;
extern	cvar_t		*sv_rollangle;
extern	cvar_t		*sv_rollspeed;
extern	cvar_t		*sv_maxspeed;
extern	cvar_t		*host_frametime;

extern	sv_client_t	*sv_client;

//===========================================================
//
// sv_main.c
//
void SV_FinalMessage (char *message, bool reconnect);
void SV_DropClient (sv_client_t *drop);

int SV_ModelIndex (const char *name);
int SV_SoundIndex (const char *name);
int SV_ImageIndex (const char *name);
int SV_DecalIndex (const char *name);

void SV_WriteClientdataToMessage (sv_client_t *client, sizebuf_t *msg);
void SV_ExecuteUserCommand (char *s);
void SV_InitOperatorCommands( void );
void SV_KillOperatorCommands( void );
void SV_SendServerinfo (sv_client_t *client);
void SV_UserinfoChanged (sv_client_t *cl);
void Master_Heartbeat (void);
void Master_Packet (void);

//
// sv_init.c
//
void SV_InitGame (void);
void SV_Map(char *levelstring, char *savename );
void SV_SpawnServer (char *server, char *savename, sv_state_t serverstate );
int SV_FindIndex (const char *name, int start, int end, bool create);
void SV_VM_Begin(void);
void SV_VM_End(void);

//
// sv_phys.c
//
void SV_Physics( void );
void SV_PlayerMove( sv_edict_t *ed );
void SV_DropToFloor (edict_t *ent);
void SV_CheckGround (edict_t *ent);
int SV_ContentsMask( const edict_t *passedict );
bool SV_MoveStep (edict_t *ent, vec3_t move, bool relink);
void SV_Physics_ClientMove(  sv_client_t *cl, usercmd_t *cmd );
void SV_CheckVelocity (edict_t *ent);
bool SV_CheckBottom (edict_t *ent);

//
// sv_move.c
//
void SV_Transform( sv_edict_t *ed, matrix4x3 transform );
void SV_PlaySound( sv_edict_t *ed, float volume, const char *sample );
bool SV_movestep( edict_t *ent, vec3_t move, bool relink, bool noenemy, bool settrace );

//
// sv_send.c
//
void SV_SendClientMessages (void);
void SV_AmbientSound( edict_t *entity, int soundindex, float volume, float attenuation );
void SV_StartSound (vec3_t origin, edict_t *entity, int channel, int index, float vol, float attn, float timeofs);
void SV_ClientPrintf (sv_client_t *cl, int level, char *fmt, ...);
void SV_BroadcastPrintf (int level, char *fmt, ...);
void SV_BroadcastCommand (char *fmt, ...);

//
// sv_client.c
//
char *SV_StatusString( void );
void SV_GetChallenge( netadr_t from );
void SV_DirectConnect( netadr_t from );
void SV_PutClientInServer( edict_t *ent );
void SV_ClientThink( sv_client_t *cl, usercmd_t *cmd );
void SV_ExecuteClientMessage( sv_client_t *cl, sizebuf_t *msg );
void SV_ConnectionlessPacket( netadr_t from, sizebuf_t *msg );

//
// sv_ccmds.c
//
void SV_Status_f( void );
void SV_Newgame_f( void );
void SV_SectorList_f( void );

//
// sv_ents.c
//
void SV_WriteFrameToClient (sv_client_t *client, sizebuf_t *msg);
void SV_BuildClientFrame (sv_client_t *client);
void SV_UpdateEntityState( edict_t *ent);
void SV_FatPVS ( vec3_t org );

void SV_Error (char *error, ...);

//
// sv_game.c
//
void SV_InitServerProgs( void );
void SV_FreeServerProgs( void );

void SV_InitEdict (edict_t *e);
void SV_ConfigString (int index, const char *val);
void SV_SetModel (edict_t *ent, const char *name);
void SV_CreatePhysBody( edict_t *ent );
void SV_SetPhysForce( edict_t *ent );
void SV_SetMassCentre( edict_t *ent);
float SV_AngleMod( float ideal, float current, float speed );

//
// sv_studio.c
//
cmodel_t *SV_GetModelPtr( edict_t *ent );
float *SV_GetModelVerts( sv_edict_t *ent, int *numvertices );
int SV_StudioExtractBbox( studiohdr_t *phdr, int sequence, float *mins, float *maxs );
bool SV_CreateMeshBuffer( edict_t *in, cmodel_t *out );

//
// sv_spawn.c
//
void SV_SpawnEntities( const char *mapname, const char *entities );
void SV_StartParticle (vec3_t org, vec3_t dir, int color, int count);
void SV_FreeEdict (edict_t *ed);
void SV_InitEdict (edict_t *e);
edict_t *SV_Spawn (void);
bool SV_ClientConnect (edict_t *ent, char *userinfo);
void SV_TouchTriggers (edict_t *ent);

//
// sv_save.c
//
void SV_WriteSaveFile( char *name );
void SV_ReadSaveFile( char *name );
void SV_ReadLevelFile( char *name );
//============================================================

//
// high level object sorting to reduce interaction tests
//
void SV_ClearWorld (void);
// called after the world model has been loaded, before linking any entities

void SV_UnlinkEdict (edict_t *ent);
// call before removing an entity, and before trying to move one,
// so it doesn't clip against itself

void SV_LinkEdict (edict_t *ent);
// Needs to be called any time an entity changes origin, mins, maxs,
// or solid.  Automatically unlinks if needed.
// sets ent->v.absmin and ent->v.absmax
// sets ent->leafnums[] for pvs determination even if the entity
// is not solid

int SV_AreaEdicts( const vec3_t mins, const vec3_t maxs, edict_t **list, int maxcount );
// fills in a table of edict pointers with edicts that have bounding boxes
// that intersect the given area.  It is possible for a non-axial bmodel
// to be returned that doesn't actually intersect the area on an exact
// test.
// returns the number of pointers filled in
// ??? does this always return the world?

//===================================================================

//
// functions that interact with everything apropriate
//
int SV_PointContents( const vec3_t p );
// returns the CONTENTS_* value from the world at the given point.
// Quake 2 extends this to also check entities, to allow moving liquids

trace_t SV_Trace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int type, edict_t *passedict, int contentmask );
trace_t SV_TraceToss( edict_t *tossent, edict_t *ignore );
// mins and maxs are relative

// if the entire move stays in a solid volume, trace.allsolid will be set,
// trace.startsolid will be set, and trace.fraction will be 0

// if the starting point is in a solid, it will be allowed to move out
// to an open area

// passedict is explicitly excluded from clipping checks (normally NULL)
#endif//SERVER_H