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
#include "entity_def.h"
#include "svgame_api.h"

//=============================================================================
#define AREA_SOLID			1
#define AREA_TRIGGERS		2

#define MAX_MASTERS			8 			// max recipients for heartbeat packets
#define LATENCY_COUNTS		16
#define MAX_ENT_CLUSTERS		16

// classic quake flags
#define SPAWNFLAG_NOT_EASY		0x00000100
#define SPAWNFLAG_NOT_MEDIUM		0x00000200
#define SPAWNFLAG_NOT_HARD		0x00000400
#define SPAWNFLAG_NOT_DEATHMATCH	0x00000800

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

	float		time;		// always sv.framenum * 50 msec
	float		frametime;
	int		framenum;
	int		net_framenum;

	string		name;		// map name, or cinematic name
	cmodel_t		*models[MAX_MODELS];
	cmodel_t		*worldmodel;

	char		configstrings[MAX_CONFIGSTRINGS][CS_SIZE];

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
	byte 		areabits[MAX_MAP_AREA_BYTES];	// portalarea visibility bits
	int  		areabits_size;
	int  		num_entities;
	int  		first_entity;		// into the circular sv_packet_entities[]
	int		msg_sent;			// time the message was transmitted
	int		msg_size;			// used to rate drop packets
	int		latency;			// message latency time
	int		index;			// client edict index
} client_frame_t;

typedef struct sv_client_s
{
	cl_state_t	state;

	char		userinfo[MAX_INFO_STRING];	// name, etc
	int		lastframe;		// for delta compression
	usercmd_t		lastcmd;			// for filling in big drops
	usercmd_t		cmd;			// current user commands

	int		ping;
	int		rate;
	int		surpressCount;		// number of messages rate supressed
	int		sendtime;			// time before send next packet

	edict_t		*edict;			// EDICT_NUM(clientnum+1)
	char		name[32];			// extracted from userinfo, high bits masked

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

// link_t is only used for entity area links now
typedef struct link_s
{
	struct link_s	*prev;
	struct link_s	*next;
	int		entnum;			// NUM_FOR_EDICT
} link_t;

// sv_private_edict_t
struct sv_priv_s
{
	link_t			area;		// linked to a division node or leaf
	struct sv_client_s		*client;		// filled for player ents
	int			clipmask;		// trace info
	int			headnode;		// unused if num_clusters != -1
	int			linkcount;
	int			num_clusters;	// if -1, use headnode instead
	int			clusternums[MAX_ENT_CLUSTERS];
	int			framenum;		// update framenumber
	int			areanum, areanum2;
	bool			forceupdate;	// physic_push force update
	bool			suspended;	// suspended in air toss object
	bool			linked;		// passed through SV_LinkEdict
	bool			stuck;		// entity stucked in brush

	vec3_t			water_origin;	// step old origin
	vec3_t			moved_origin;	// push old origin
	vec3_t			moved_angles;	// push old angles

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
	// user messages stuff
	const char	*msg_name;		// just for debug
	int		msg_sizes[MAX_USER_MESSAGES];	// user messages bounds checker
	int		msg_size_index;		// write message size at this pos in sizebuf
	int		msg_realsize;		// left in bytes
	int		msg_index;		// for debug messages
	int		msg_dest;			// msg destination ( MSG_ONE, MSG_ALL etc )
	edict_t		*msg_ent;
	vec3_t		msg_org;

	void		*hInstance;		// pointer to server.dll

	union
	{
		edict_t	*edicts;			// acess by edict number
		void	*vp;			// acess by offset in bytes
	};

	globalvars_t	*globals;			// server globals
	DLL_FUNCTIONS	dllFuncs;			// dll exported funcs
	byte		*mempool;			// edicts pool
	byte		*private;			// server.dll private pool

	// library exports table
	word		*ordinals;
	dword		*funcs;
	char		*names[MAX_SYSPATH];	// max 1024 exports supported
	int		num_ordinals;		// actual exports count
	dword		funcBase;			// base offset

	int		hStringTable;		// stringtable handle
} svgame_static_t;

typedef struct
{
	bool		initialized;		// sv_init has completed
	dword		realtime;			// always increasing, no clamping, etc
	float		timeleft;			// frametime * game_frames

	string		mapcmd;			// ie: *intro.cin+base 
	string		comment;			// map name, e.t.c. 

	int		spawncount;		// incremented each server start
						// used to check late spawns
	sv_client_t	*clients;			// [host_maxclients->integer]
	int		num_client_entities;	// host_maxclients->integer*UPDATE_BACKUP*MAX_PACKET_ENTITIES
	int		next_client_entities;	// next client_entity to use
	entity_state_t	*client_entities;		// [num_client_entities]
	entity_state_t	*baselines;		// [host.max_edicts]

	int		last_heartbeat;

	challenge_t	challenges[MAX_CHALLENGES];	// to prevent invalid IPs from connecting
} server_static_t;

//=============================================================================

extern	netadr_t	master_adr[MAX_MASTERS];		// address of the master server
extern	const char	*ed_name[];
extern	server_static_t	svs;			// persistant server info
extern	server_t		sv;			// local server
extern	svgame_static_t	svgame;			// persistant game info

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
extern	cvar_t		*hostname;
extern	cvar_t		*sv_stepheight;
extern	cvar_t		*sv_playersonly;
extern	cvar_t		*sv_rollangle;
extern	cvar_t		*sv_rollspeed;
extern	cvar_t		*sv_maxspeed;
extern	cvar_t		*sv_physics;
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
void SV_InitGame (void);
void SV_Map( char *levelstring, char *savename );
void SV_SpawnServer( const char *server, const char *savename );
int SV_FindIndex (const char *name, int start, int end, bool create);

//
// sv_phys.c
//
void SV_Physics( void );
void SV_PlayerMove( edict_t *ed );
void SV_DropToFloor( edict_t *ent );
void SV_CheckGround( edict_t *ent );
bool SV_UnstickEntity( edict_t *ent );
int SV_ContentsMask( const edict_t *passedict );
bool SV_MoveStep (edict_t *ent, vec3_t move, bool relink);
void SV_Physics_ClientMove(  sv_client_t *cl, usercmd_t *cmd );
void SV_CheckVelocity (edict_t *ent);
bool SV_CheckBottom (edict_t *ent);

//
// sv_move.c
//
void SV_Transform( edict_t *ed, const vec3_t origin, const matrix3x3 transform );
void SV_PlaySound( edict_t *ed, float volume, float pitch, const char *sample );
bool SV_movestep( edict_t *ent, vec3_t move, bool relink, bool noenemy, bool settrace );

//
// sv_send.c
//
void SV_SendClientMessages( void );
void SV_AmbientSound( edict_t *entity, int soundindex, float volume, float attenuation );
void SV_ClientPrintf( sv_client_t *cl, char *fmt, ... );
void SV_BroadcastPrintf( char *fmt, ... );
void SV_BroadcastCommand( char *fmt, ... );

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

//
// sv_ents.c
//
void SV_WriteFrameToClient (sv_client_t *client, sizebuf_t *msg);
void SV_BuildClientFrame (sv_client_t *client);
void SV_UpdateEntityState( edict_t *ent);
void SV_Error (char *error, ...);

//
// sv_game.c
//
void SV_LoadProgs( const char *name );
void SV_UnloadProgs( void );
void SV_FreeEdicts( void );
void SV_InitEdict( edict_t *pEdict );
void SV_ConfigString (int index, const char *val);
void SV_SetModel (edict_t *ent, const char *name);
void SV_CreatePhysBody( edict_t *ent );
void SV_SetPhysForce( edict_t *ent );
void SV_SetMassCentre( edict_t *ent);
void SV_CopyTraceToGlobal( trace_t *trace );
void SV_CopyTraceResult( TraceResult *out, trace_t trace );
float SV_AngleMod( float ideal, float current, float speed );
void SV_SpawnEntities( const char *mapname, script_t *entities );
string_t SV_AllocString( const char *szValue );
const char *SV_GetString( string_t iString );

_inline edict_t *SV_EDICT_NUM( int n, const char * file, const int line )
{
	if((n >= 0) && (n < svgame.globals->maxEntities))
		return svgame.edicts + n;
	Host_Error( "SV_EDICT_NUM: bad number %i (called at %s:%i)\n", n, file, line );
	return NULL;	
}

//
// sv_studio.c
//
cmodel_t *SV_GetModelPtr( edict_t *ent );
float *SV_GetModelVerts( edict_t *ent, int *numvertices );
int SV_StudioExtractBbox( dstudiohdr_t *phdr, int sequence, float *mins, float *maxs );
bool SV_CreateMeshBuffer( edict_t *in, cmodel_t *out );

//
// sv_spawn.c
//
edict_t *SV_AllocEdict( void );
void SV_FreeEdict( edict_t *pEdict );
bool SV_ClientConnect (edict_t *ent, char *userinfo);
void SV_TouchTriggers (edict_t *ent);

//
// sv_save.c
//
void SV_WriteSaveFile( const char *name );
void SV_ReadSaveFile( const char *name );
void SV_ReadLevelFile( const char *name );
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

int SV_AreaEdicts( const vec3_t mins, const vec3_t maxs, edict_t **list, int maxcount, int areatype );
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

trace_t SV_Trace(const vec3_t start, const vec3_t min, const vec3_t max, const vec3_t end, int t, edict_t *e, int mask);
trace_t SV_TraceToss( edict_t *tossent, edict_t *ignore );
// mins and maxs are relative

// if the entire move stays in a solid volume, trace.allsolid will be set,
// trace.startsolid will be set, and trace.fraction will be 0

// if the starting point is in a solid, it will be allowed to move out
// to an open area

// passedict is explicitly excluded from clipping checks (normally NULL)
#endif//SERVER_H