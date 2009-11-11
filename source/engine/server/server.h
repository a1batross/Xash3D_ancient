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
#include "svgame_api.h"
#include "com_world.h"

//=============================================================================
#define AREA_SOLID			1
#define AREA_TRIGGERS		2

#define MAX_MASTERS			8 			// max recipients for heartbeat packets
#define LATENCY_COUNTS		16
#define MAX_ENT_CLUSTERS		16
#define RATE_MESSAGES		10

#define NUM_FOR_EDICT(e)	((int)((edict_t *)(e) - svgame.edicts))
#define EDICT_NUM( num )	SV_EDICT_NUM( num, __FILE__, __LINE__ )
#define STRING( offset )	SV_GetString( offset )
#define MAKE_STRING(str)	SV_AllocString( str )

typedef enum
{
	ss_dead,		// no map loaded
	ss_loading,	// spawning level edicts
	ss_active		// actively running
} sv_state_t;

typedef enum
{
	cs_free = 0,	// can be reused for a new connection
	cs_zombie,	// client has been disconnected, but don't reuse connection for a couple seconds
	cs_connected,	// has been assigned to a sv_client_t, but not in game yet
	cs_spawned	// client is fully in game

} cl_state_t;

typedef struct server_s
{
	sv_state_t	state;		// precache commands are only valid during load

	bool		loadgame;		// client begins should reuse existing entity
	bool		changelevel;	// chnage map, reconnect clients, transfer entities, merge globals

	int		time;		// sv.time += sv.frametime
	int		frametime;
	int		framenum;
	int		net_framenum;

	int		lastcheck;	// used by PF_checkclient
	int		lastchecktime;	// for monster ai 

	string		name;		// map name, or cinematic name
	string		startspot;	// player_start name on nextmap

	char		configstrings[MAX_CONFIGSTRINGS][CS_SIZE];

	// the multicast buffer is used to send a message to a set of clients
	// it is only used to marshall data until SV_Message is called
	sizebuf_t		multicast;
	byte		multicast_buf[MAX_MSGLEN];

	bool		autosaved;
	bool		cphys_prepped;
	bool		paused;
} server_t;

typedef struct
{
	entity_state_t	ps;			// player state
	byte 		areabits[MAX_MAP_AREA_BYTES];	// portalarea visibility bits
	int  		areabits_size;
	int  		num_entities;
	int  		first_entity;		// into the circular sv_packet_entities[]
	int		senttime;			// time the message was transmitted

	int		index;			// client edict index
} client_frame_t;

typedef struct sv_client_s
{
	cl_state_t	state;

	char		userinfo[MAX_INFO_STRING];	// name, etc
	int		lastframe;		// for delta compression
	usercmd_t		lastcmd;			// for filling in big drops

	int		spectator;		// non-interactive

	int		commandMsec;		// every seconds this is reset, if user
	   					// commands exhaust it, assume time cheating

	int		frame_latency[LATENCY_COUNTS];
	int		ping;

	int		message_size[RATE_MESSAGES];	// used to rate drop packets
	int		rate;

	int		surpressCount;		// number of messages rate supressed

	edict_t		*edict;			// EDICT_NUM(clientnum+1)
	char		name[32];			// extracted from userinfo, color string allowed
	int		messagelevel;		// for filtering printed messages

	// The reliable buf contain reliable user messages that must be followed
	// after pvs frame
	sizebuf_t		reliable;
	byte		reliable_buf[MAX_MSGLEN];

	// the datagram is written to by sound calls, prints, temp ents, etc.
	// it can be harmlessly overflowed.
	sizebuf_t		datagram;
	byte		datagram_buf[MAX_MSGLEN];

	client_frame_t	frames[UPDATE_BACKUP];	// updates can be delta'd from here

	byte		*download;		// file being downloaded
	int		downloadsize;		// total bytes (can't use EOF because of paks)
	int		downloadcount;		// bytes sent

	int		lastmessage;		// sv.framenum when packet was last received
	int		lastconnect;

	int		challenge;		// challenge of this user, randomly generated

	netchan_t		netchan;
} sv_client_t;

// sv_private_edict_t
struct sv_priv_s
{
	link_t		area;		// linked to a division node or leaf
	sv_client_t	*client;		// filled for player ents
	int		lastcluster;	// unused if num_clusters != -1
	int		linkcount;
	int		num_clusters;	// if -1, use headnode instead
	int		clusternums[MAX_ENT_CLUSTERS];
	int		framenum;		// update framenumber
	int		areanum, areanum2;
	bool		linked;		// passed through SV_LinkEdict
	bool		stuck;		// entity stucked in brush
	size_t		pvdata_size;	// member size of alloceed pvPrivateData
					// (used by SV_CopyEdict)
	entity_state_t	s;		// baseline (this is a player_state too)
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
	// user messages stuff
	const char	*msg_name;		// just for debug
	int		msg_sizes[MAX_USER_MESSAGES];	// user messages bounds checker
	int		msg_size_index;		// write message size at this pos in sizebuf
	int		msg_realsize;		// left in bytes
	int		msg_index;		// for debug messages
	int		msg_dest;			// msg destination ( MSG_ONE, MSG_ALL etc )
	bool		msg_started;		// to avoid include messages
	edict_t		*msg_ent;			// user message member entity
	vec3_t		msg_org;			// user message member origin

	void		*hInstance;		// pointer to server.dll

	union
	{
		edict_t	*edicts;			// acess by edict number
		void	*vp;			// acess by offset in bytes
	};

	movevars_t	movevars;			// curstate
	movevars_t	oldmovevars;		// oldstate
	playermove_t	*pmove;			// pmove state

	globalvars_t	*globals;			// server globals
	DLL_FUNCTIONS	dllFuncs;			// dll exported funcs
	byte		*private;			// server.dll private pool
	byte		*temppool;		// for parse, save and restore edicts
	byte		*mempool;			// server premamnent pool: edicts etc

	// library exports table
	word		*ordinals;
	dword		*funcs;
	char		*names[4096];		// max 4096 exports supported
	int		num_ordinals;		// actual exports count
	dword		funcBase;			// base offset

	int		hStringTable;		// stringtable handle
	SAVERESTOREDATA	SaveData;			// shared struct, used for save data
} svgame_static_t;

typedef struct
{
	bool		initialized;		// sv_init has completed
	int		realtime;			// always increasing, no clamping, etc
	int		timestart;		// just for profiling

	char		mapname[CS_SIZE];		// current mapname 
	string		comment;			// map name, e.t.c. 
	int		spawncount;		// incremented each server start
						// used to check late spawns
	sv_client_t	*clients;			// [sv_maxclients->integer]
	int		num_client_entities;	// sv_maxclients->integer*UPDATE_BACKUP*MAX_PACKET_ENTITIES
	int		next_client_entities;	// next client_entity to use
	entity_state_t	*client_entities;		// [num_client_entities]
	entity_state_t	*baselines;		// [GI->max_edicts]

	int		last_heartbeat;
	challenge_t	challenges[MAX_CHALLENGES];	// to prevent invalid IPs from connecting
} server_static_t;

//=============================================================================

extern	netadr_t	master_adr[MAX_MASTERS];		// address of the master server
extern	const char	*ed_name[];
extern	server_static_t	svs;			// persistant server info
extern	server_t		sv;			// local server
extern	svgame_static_t	svgame;			// persistant game info

extern	cvar_t		*sv_pausable;		// allows pause in multiplayer
extern	cvar_t		*sv_noreload;		// don't reload level state when reentering
extern	cvar_t		*sv_airaccelerate;
extern	cvar_t		*sv_accelerate;
extern	cvar_t		*sv_friction;
extern	cvar_t		*sv_edgefriction;
extern	cvar_t		*sv_idealpitchscale;
extern	cvar_t		*sv_maxvelocity;
extern	cvar_t		*sv_gravity;
extern	cvar_t		*sv_stopspeed;
extern	cvar_t		*sv_fps;			// running server at
extern	cvar_t		*sv_enforcetime;
extern	cvar_t		*sv_reconnect_limit;
extern	cvar_t		*allow_download;
extern	cvar_t		*rcon_password;
extern	cvar_t		*hostname;
extern	cvar_t		*sv_stepheight;
extern	cvar_t		*sv_playersonly;
extern	cvar_t		*sv_rollangle;
extern	cvar_t		*sv_rollspeed;
extern	cvar_t		*sv_maxspeed;
extern	cvar_t		*sv_maxclients;
extern	sv_client_t	*sv_client;

//===========================================================
//
// sv_main.c
//
void SV_FinalMessage (char *message, bool reconnect);
void SV_DropClient (sv_client_t *drop);

int SV_ModelIndex (const char *name);
int SV_SoundIndex (const char *name);
int SV_ClassIndex (const char *name);
int SV_DecalIndex (const char *name);
int SV_UserMessageIndex (const char *name);

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
void SV_InitGame( void );
void SV_PrepModels( void );
void SV_ActivateServer( void );
void SV_DeactivateServer( void );
void SV_LevelInit( const char *newmap, const char *oldmap, const char *savename );
void SV_SpawnServer( const char *server, const char *startspot );
int SV_FindIndex( const char *name, int start, int end, bool create );
void SV_ClassifyEdict( edict_t *ent );

//
// sv_phys.c
//
void SV_Physics( void );
void SV_CheckVelocity( edict_t *ent );
bool SV_CheckWater( edict_t *ent );
int SV_TryUnstick( edict_t *ent, vec3_t oldvel );
bool SV_RunThink( edict_t *ent );

//
// sv_move.c
//
bool SV_movestep( edict_t *ent, vec3_t move, bool relink, bool noenemy, bool settrace );
bool SV_CheckBottom( edict_t *ent );

//
// sv_send.c
//
void SV_SendClientMessages( void );
void SV_AmbientSound( edict_t *entity, int soundindex, float volume, float attenuation );
void SV_ClientPrintf( sv_client_t *cl, int level, char *fmt, ... );
void SV_BroadcastPrintf( int level, char *fmt, ... );
void SV_BroadcastCommand( char *fmt, ... );

//
// sv_client.c
//
char *SV_StatusString( void );
void SV_GetChallenge( netadr_t from );
void SV_DirectConnect( netadr_t from );
void SV_TogglePause( const char *msg );
void SV_PutClientInServer( edict_t *ent );
bool SV_ClientConnect( edict_t *ent, char *userinfo );
void SV_ClientThink( sv_client_t *cl, usercmd_t *cmd );
void SV_ExecuteClientMessage( sv_client_t *cl, sizebuf_t *msg );
void SV_ConnectionlessPacket( netadr_t from, sizebuf_t *msg );
void SV_SetIdealPitch( sv_client_t *cl );
void SV_InitClientMove( void );

//
// sv_cmds.c
//
void SV_Status_f( void );
void SV_Newgame_f( void );

//
// sv_frame.c
//
void SV_WriteFrameToClient( sv_client_t *client, sizebuf_t *msg );
void SV_BuildClientFrame( sv_client_t *client );
void SV_UpdateEntityState( edict_t *ent, bool baseline );

//
// sv_game.c
//
void SV_LoadProgs( const char *name );
void SV_UnloadProgs( void );
void SV_FreeEdicts( void );
edict_t *SV_AllocEdict( void );
void SV_FreeEdict( edict_t *pEdict );
void SV_InitEdict( edict_t *pEdict );
bool SV_CopyEdict( edict_t *out, edict_t *in );
void SV_ConfigString( int index, const char *val );
void SV_SetModel( edict_t *ent, const char *name );
void SV_CopyTraceToGlobal( trace_t *trace );
script_t *SV_GetEntityScript( const char *filename );
float SV_AngleMod( float ideal, float current, float speed );
void SV_SpawnEntities( const char *mapname, script_t *entities );
edict_t* SV_AllocPrivateData( edict_t *ent, string_t className );
string_t SV_AllocString( const char *szValue );
const char *SV_GetString( string_t iString );
bool SV_MapIsValid( const char *filename, const char *spawn_entity );
void SV_StartSound( edict_t *ent, int chan, const char *sample, float vol, float attn, int flags, int pitch );
script_t *CM_GetEntityScript( void );

_inline edict_t *SV_EDICT_NUM( int n, const char * file, const int line )
{
	if((n >= 0) && (n < svgame.globals->maxEntities))
		return svgame.edicts + n;
	Host_Error( "SV_EDICT_NUM: bad number %i (called at %s:%i)\n", n, file, line );
	return NULL;	
}

//
// sv_save.c
//
void SV_MergeLevelFile( const char *name );
void SV_ChangeLevel( bool bUseLandmark, const char *mapname, const char *start );
void SV_WriteSaveFile( const char *name, bool autosave, bool bUseLandmark );
void SV_ReadSaveFile( const char *name );
void SV_ReadLevelFile( const char *name );
const char *SV_GetLatestSave( void );
//============================================================

// high level object sorting to reduce interaction tests

// called after the world model has been loaded, before linking any entities

void SV_UnlinkEdict (edict_t *ent);
// call before removing an entity, and before trying to move one,
// so it doesn't clip against itself


// Needs to be called any time an entity changes origin, mins, maxs,
// or solid.  Automatically unlinks if needed.
// sets ent->v.absmin and ent->v.absmax
// sets ent->leafnums[] for pvs determination even if the entity
// is not solid

int SV_AreaEdicts( const vec3_t mins, const vec3_t maxs, edict_t **list, int maxcount, int areatype );
// fills in a table of edict pointers with edicts that have bounding boxes
// that intersect the given area.  It is possible for a non-axial bmodel
// to be returned that doesn't actually intersect the area on an exact
// test.
// returns the number of pointers filled in
// ??? does this always return the world?

//
// sv_world.c
//

extern areanode_t	sv_areanodes[];

void SV_ClearWorld( void );
trace_t SV_Move( const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int type, edict_t *e );
trace_t SV_MoveToss( edict_t *tossent, edict_t *ignore );
void SV_LinkEdict( edict_t *ent, bool touch_triggers );
void SV_TouchLinks( edict_t *ent, areanode_t *node );
edict_t *SV_TestPlayerPosition( const vec3_t origin );
int SV_PointContents( const vec3_t p );
trace_t SV_ClipMoveToEntity( edict_t *ent, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end );
// mins and maxs are relative

// if the entire move stays in a solid volume, trace.allsolid will be set,
// trace.startsolid will be set, and trace.fraction will be 0

// if the starting point is in a solid, it will be allowed to move out
// to an open area

// passedict is explicitly excluded from clipping checks (normally NULL)
#endif//SERVER_H