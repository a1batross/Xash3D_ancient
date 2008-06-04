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

#include "progsvm.h"
#include "net_msg.h"
#include "mathlib.h"
#include "basefiles.h"

//=============================================================================

#define MAX_MASTERS		8 // max recipients for heartbeat packets
#define LATENCY_COUNTS	16
#define RATE_MESSAGES	10

// classic quake flags
#define SPAWNFLAG_NOT_EASY		0x00000100
#define SPAWNFLAG_NOT_MEDIUM		0x00000200
#define SPAWNFLAG_NOT_HARD		0x00000400
#define SPAWNFLAG_NOT_DEATHMATCH	0x00000800

// client printf level
#define PRINT_LOW			0	// pickup messages
#define PRINT_MEDIUM		1	// death messages
#define PRINT_HIGH			2	// critical messages
#define PRINT_CHAT			3	// chat messages

#define FL_CLIENT			(1<<0)	// this is client
#define FL_MONSTER			(1<<1)	// this is npc
#define FL_DEADMONSTER		(1<<2)	// dead npc or dead player
#define FL_WORLDBRUSH		(1<<3)	// Not moveable/removeable brush entity
#define FL_DORMANT			(1<<4)	// Entity is dormant, no updates to client
#define FL_FRAMETHINK		(1<<5)	// entity will be thinking every frame
#define FL_GRAPHED			(1<<6)	// worldgraph has this ent listed as something that blocks a conection
#define FL_FLOAT			(1<<7)	// this entity can be floating. FIXME: remove this ?
#define FL_TRACKTRAIN		(1<<8)	// old stuff...

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
	cs_connected,	// has been assigned to a client_state_t, but not in game yet
	cs_spawned	// client is fully in game

} cl_state_t;

typedef struct
{
	sv_state_t	state;		// precache commands are only valid during load

	bool		loadgame;		// client begins should reuse existing entity

	float		time;		// always sv.framenum * 100 msec
	int		framenum;
	float		frametime;

	char		name[MAX_QPATH];	// map name, or cinematic name
	cmodel_t		*models[MAX_MODELS];

	char		configstrings[MAX_CONFIGSTRINGS][MAX_QPATH];
	entity_state_t	baselines[MAX_EDICTS];
	edict_t		**moved_edicts;	// [MAX_EDICTS]

	// the multicast buffer is used to send a message to a set of clients
	// it is only used to marshall data until SV_Message is called
	sizebuf_t		multicast;
	byte		multicast_buf[MAX_MSGLEN];

	float		lastchecktime;
	int		lastcheck;

	bool		autosaved;
} server_t;

typedef struct
{
	int  		areabytes;
	byte 		areabits[MAX_MAP_AREAS/8];	// portalarea visibility bits
	player_state_t	ps;
	int  		num_entities;
	int  		first_entity;		// into the circular sv_packet_entities[]
	float		senttime;			// for ping calculations

} client_frame_t;

typedef struct client_state_s
{
	cl_state_t	state;

	char		userinfo[MAX_INFO_STRING];	// name, etc

	int		lastframe;		// for delta compression
	usercmd_t		lastcmd;			// for filling in big drops

	int		commandMsec;		// every seconds this is reset, if user
						// commands exhaust it, assume time cheating

	int		frame_latency[LATENCY_COUNTS];
	int		ping;

	int		message_size[RATE_MESSAGES];	// used to rate drop packets
	int		rate;
	int		surpressCount;		// number of messages rate supressed

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

	float		lastmessage;		// sv.framenum when packet was last received
	float		lastconnect;

	int		challenge;		// challenge of this user, randomly generated

	netchan_t		netchan;
} client_state_t;

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
	float		time;

} challenge_t;

typedef struct
{
	bool		initialized;		// sv_init has completed
	float		realtime;			// always increasing, no clamping, etc

	char		mapcmd[MAX_TOKEN_CHARS];	// ie: *intro.cin+base 
	char		comment[MAX_TOKEN_CHARS];	// map name, e.t.c. 

	int		spawncount;		// incremented each server start
						// used to check late spawns
	gclient_t		*gclients;		// [maxclients->value]
	client_state_t	*clients;			// [maxclients->value]
	int		num_client_entities;	// maxclients->value*UPDATE_BACKUP*MAX_PACKET_ENTITIES
	int		next_client_entities;	// next client_entity to use
	entity_state_t	*client_entities;		// [num_client_entities]

	float		last_heartbeat;

	challenge_t	challenges[MAX_CHALLENGES];	// to prevent invalid IPs from connecting
} server_static_t;

//=============================================================================

extern	netadr_t	net_from;
extern	sizebuf_t	net_message;

extern	netadr_t	master_adr[MAX_MASTERS];		// address of the master server

extern	server_static_t	svs;			// persistant server info
extern	server_t		sv;			// local server

extern	cvar_t		*sv_paused;
extern	cvar_t		*maxclients;
extern	cvar_t		*sv_noreload;		// don't reload level state when reentering
extern	cvar_t		*sv_airaccelerate;		// don't reload level state when reentering
extern	cvar_t		*sv_maxvelocity;
extern	cvar_t		*sv_gravity;
						// development tool
extern	cvar_t		*sv_enforcetime;

extern	client_state_t		*sv_client;
extern	edict_t		*sv_player;


//===========================================================
//
// sv_main.c
//
void SV_FinalMessage (char *message, bool reconnect);
void SV_DropClient (client_state_t *drop);

int SV_ModelIndex (const char *name);
int SV_SoundIndex (const char *name);
int SV_ImageIndex (const char *name);
int SV_DecalIndex (const char *name);

void SV_WriteClientdataToMessage (client_state_t *client, sizebuf_t *msg);
void SV_ExecuteUserCommand (char *s);
void SV_InitOperatorCommands( void );
void SV_KillOperatorCommands( void );
void SV_SendServerinfo (client_state_t *client);
void SV_UserinfoChanged (client_state_t *cl);
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
void SV_PlayerMove( sv_edict_t *ed );
void SV_PrepWorldFrame (void);
void SV_Physics (edict_t *ent);
void SV_DropToFloor (edict_t *ent);
void SV_CheckGround (edict_t *ent);
bool SV_MoveStep (edict_t *ent, vec3_t move, bool relink);
void SV_CheckVelocity (edict_t *ent);
bool SV_CheckBottom (edict_t *ent);

//
// sv_send.c
//
typedef enum {RD_NONE, RD_CLIENT, RD_PACKET} redirect_t;
#define	SV_OUTPUTBUF_LENGTH	(MAX_MSGLEN - 16)

extern char sv_outputbuf[SV_OUTPUTBUF_LENGTH];

void SV_FlushRedirect (int sv_redirected, char *outputbuf);

void SV_SendClientMessages (void);
void SV_AmbientSound( edict_t *entity, int soundindex, float volume, float attenuation );
void SV_StartSound (vec3_t origin, edict_t *entity, int channel, int index, float vol, float attn, float timeofs);
void SV_ClientPrintf (client_state_t *cl, int level, char *fmt, ...);
void SV_BroadcastPrintf (int level, char *fmt, ...);
void SV_BroadcastCommand (char *fmt, ...);

//
// sv_user.c
//
void SV_Nextserver (void);
void SV_ExecuteClientMessage (client_state_t *cl);

//
// sv_ccmds.c
//
void SV_Status_f( void );
void SV_Newgame_f( void );
void SV_SectorList_f( void );

//
// sv_ents.c
//
void SV_WriteFrameToClient (client_state_t *client, sizebuf_t *msg);
void SV_BuildClientFrame (client_state_t *client);
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
void SV_Transform( sv_edict_t *ed, matrix4x3 transform );
void SV_PlaySound( sv_edict_t *ed, float volume, const char *sample );
void SV_FreeEdict (edict_t *ed);
void SV_InitEdict (edict_t *e);
edict_t *SV_Spawn (void);
void SV_RunFrame (void);
void SV_ClientUserinfoChanged (edict_t *ent, char *userinfo);
bool SV_ClientConnect (edict_t *ent, char *userinfo);
void SV_ClientBegin (edict_t *ent);
void ClientThink (edict_t *ent, usercmd_t *ucmd);
void SV_ClientDisconnect (edict_t *ent);
void SV_ClientCommand (edict_t *ent);
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
int SV_PointContents( const vec3_t p, edict_t *passedict );
// returns the CONTENTS_* value from the world at the given point.
// Quake 2 extends this to also check entities, to allow moving liquids


trace_t SV_ClipMoveToEntity(edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int contentsmask );
trace_t SV_Trace( const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, edict_t *passedict, int contentmask );
trace_t SV_TraceToss (edict_t *tossent, edict_t *ignore);
// mins and maxs are relative

// if the entire move stays in a solid volume, trace.allsolid will be set,
// trace.startsolid will be set, and trace.fraction will be 0

// if the starting point is in a solid, it will be allowed to move out
// to an open area

// passedict is explicitly excluded from clipping checks (normally NULL)
#endif//SERVER_H