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
// client.h -- primary header for client

#ifndef CLIENT_H
#define CLIENT_H

#include "mathlib.h"
#include "cl_edict.h"

#define MAX_EDIT_LINE	256
#define COMMAND_HISTORY	32
#define MAX_SERVERS		64
#define ColorIndex(c)	(((c) - '0') & 7)

//=============================================================================
typedef struct frame_s
{
	bool		valid;			// cleared if delta parsing was invalid
	int		serverframe;
	int		servertime;		// server time the message is valid for (in msec)
	int		deltaframe;
	byte		areabits[MAX_MAP_AREAS/8];	// portalarea visibility bits
	int		num_entities;
	int		parse_entities;		// non-masked index into cl_parse_entities array
	entity_state_t	ps;
} frame_t;

// console stuff
typedef struct field_s
{
	int	cursor;
	int	scroll;
	int	widthInChars;
	char	buffer[MAX_EDIT_LINE];
	int	maxchars; // menu stuff
} field_t;

#define	CMD_BACKUP		64	// allow a lot of command backups for very fast systems
#define	CMD_MASK			(CMD_BACKUP - 1)

//
// the client_t structure is wiped completely at every
// server map change
//
typedef struct
{
	int		timeoutcount;

	bool		video_prepped;		// false if on new level or new ref dll
	bool		audio_prepped;		// false if on new level or new snd dll
	bool		force_refdef;		// vid has changed, so we can't use a paused refdef

	int		parse_entities;		// index (not anded off) into cl_parse_entities[]

	usercmd_t		cmds[CMD_BACKUP];			// each mesage will send several old cmds
	int		cmd_number;
	int		predicted_origins[CMD_BACKUP][3];	// for debug comparing against server

	float		predicted_step;				// for stair up smoothing
	uint		predicted_step_time;

	vec3_t		predicted_origin;	// generated by CL_PredictMovement
	vec3_t		predicted_angles;
	vec3_t		prediction_error;

	frame_t		frame;		// received from server
	int		surpressCount;	// number of messages rate supressed
	frame_t		frames[UPDATE_BACKUP];

	// mouse current position
	int		mouse_x[2];
	int		mouse_y[2];
	int		mouse_step;
	float		mouse_sens;

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  It is cleared to 0 upon entering each level.
	// the server sends a delta each frame which is added to the locally
	// tracked view angles to account for standing on rotating objects,
	// and teleport direction changes
	vec3_t		viewangles;

	dword		time;		// this is the time value that the client
					// is rendering at.  always <= cls.realtime
	float		lerpfrac;		// between oldframe and frame

	refdef_t		refdef;

	vec3_t		v_forward, v_right, v_left, v_up; // set when refdef.angles is set

	// centerprint stuff
	int		centerPrintTime;
	int		centerPrintCharWidth;
	int		centerPrintY;
	char		centerPrint[1024];
	int		centerPrintLines;		

	bool		make_levelshot;

	//
	// server state information
	//
	int		servercount;		// server identification for prespawns
	int		playernum;
	char		configstrings[MAX_CONFIGSTRINGS][MAX_QPATH];

	//
	// locally derived information from server state
	//
	cmodel_t		*models[MAX_MODELS];
	cmodel_t		*worldmodel;

	string_t		edict_classnames[MAX_CLASSNAMES];
	sound_t		sound_precache[MAX_SOUNDS];
} client_t;

extern client_t	cl;

/*
==================================================================

the client_static_t structure is persistant through an arbitrary number
of server connections

==================================================================
*/

typedef enum
{
	ca_uninitialized = 0,
	ca_disconnected, 	// not talking to a server
	ca_connecting,	// sending request packets to the server
	ca_connected,	// netchan_t established, waiting for svc_serverdata
	ca_active,	// game views should be displayed
	ca_cinematic,	// playing a cinematic, not connected to a server
} connstate_t;

typedef enum
{
	dl_none,
	dl_model,
	dl_sound,
	dl_generic,
} dltype_t;		// download type

struct cl_edict_s
{
	// generic_edict_t (don't move these fields!)
	bool		free;
	float		freetime;	 	// cl.time when the object was freed
	int		serverframe;	// if not current, this ent isn't in the frame
	int		serialnumber;	// client serialnumber

	// cl_private_edict_t
	entity_state_t	baseline;		// delta from this if not from a previous frame
	entity_state_t	current;
	entity_state_t	prev;		// will always be valid, but might just be a copy of current
};

typedef struct serverinfo_s
{
	char		*mapname;
	char		*hostname;
	char		*shortname;
	char		*gamename;
	char		*netaddress;

	char		*playerstr;
	int		numplayers;
	int		maxplayers;

	char		*pingstring;
	bool		statusPacket;
	int		ping;

} serverinfo_t;

typedef enum {key_game, key_console, key_message, key_menu} keydest_t;

typedef struct
{
	connstate_t	state;
	bool		initialized;

	keydest_t		key_dest;
	byte		*mempool;

	int		framecount;
	dword		realtime;			// always increasing, no clamping, etc
	float		frametime;		// seconds since last frame

	// connection information
	string		servername;		// name of server from original connect
	float		connect_time;		// for connection retransmits

	netchan_t		netchan;
	sizebuf_t		*multicast;		// ptr for current message buffer (net or demo flow)
	int		serverProtocol;		// in case we are doing some kind of version hack

	int		challenge;		// from the server to use for connecting

	file_t		*download;		// file transfer from server
	string		downloadname;
	string		downloadtempname;
	int		downloadnumber;
	dltype_t		downloadtype;

	// demo recording info must be here, so it isn't clearing on level change
	bool		demorecording;
	bool		demoplayback;
	bool		demowaiting;		// don't record until a non-delta message is received
	string		demoname;			// for demo looping

	file_t		*demofile;
	serverinfo_t	serverlist[MAX_SERVERS];	// servers to join
	int		numservers;
	int		pingtime;			// servers timebase

	// cursor mouse pos for uimenu
	int		mouse_x;
	int		mouse_y;
} client_static_t;

extern client_static_t	cls;

/*
==============================================================

SCREEN CONSTS

==============================================================
*/
// all drawing is done to a 640*480 virtual screen size
// and will be automatically scaled to the real resolution
#define SCREEN_WIDTH	640
#define SCREEN_HEIGHT	480

#define TINYCHAR_WIDTH	(SMALLCHAR_WIDTH)
#define TINYCHAR_HEIGHT	(SMALLCHAR_HEIGHT/2)
#define SMALLCHAR_WIDTH	8
#define SMALLCHAR_HEIGHT	16
#define BIGCHAR_WIDTH	16
#define BIGCHAR_HEIGHT	24
#define GIANTCHAR_WIDTH	32
#define GIANTCHAR_HEIGHT	48

extern vrect_t scr_vrect;	// position of render window
extern vec4_t g_color_table[8];

//
// cvars
//
extern	cvar_t	*cl_gun;
extern	cvar_t	*cl_add_blend;
extern	cvar_t	*cl_add_lights;
extern	cvar_t	*cl_add_particles;
extern	cvar_t	*cl_add_entities;
extern	cvar_t	*cl_predict;
extern	cvar_t	*cl_footsteps;
extern	cvar_t	*cl_showfps;
extern	cvar_t	*cl_upspeed;
extern	cvar_t	*cl_forwardspeed;
extern	cvar_t	*cl_sidespeed;
extern	cvar_t	*cl_shownet;
extern	cvar_t	*cl_yawspeed;
extern	cvar_t	*cl_pitchspeed;

extern	cvar_t	*cl_run;

extern	cvar_t	*cl_anglespeedkey;

extern	cvar_t	*cl_showmiss;
extern	cvar_t	*cl_showclamp;

extern	cvar_t	*lookspring;
extern	cvar_t	*lookstrafe;
extern	cvar_t	*cl_sensitivity;
extern	cvar_t	*ui_sensitivity;

extern	cvar_t	*m_pitch;
extern	cvar_t	*m_yaw;
extern	cvar_t	*m_forward;
extern	cvar_t	*m_side;
extern	cvar_t	*cl_mouselook;
extern	cvar_t	*cl_testentities;
extern	cvar_t	*cl_testlights;
extern	cvar_t	*cl_testblend;
extern	cvar_t	*cl_lightlevel;	// FIXME HACK
extern	cvar_t	*cl_paused;
extern	cvar_t	*cl_levelshot_name;

extern cvar_t *scr_centertime;
extern cvar_t *scr_showpause;

typedef struct
{
	// these values shared with dlight_t so don't move them
	vec3_t	origin;
	vec3_t	color;
	float	radius;

	// cdlight_t private starts here
	int	key;				// so entities can reuse same entry
	float	die;				// stop lighting after this time
	float	decay;				// drop this each second
	float	minlight;			// don't add when contributing less
} cdlight_t;

extern	cdlight_t	cl_dlights[MAX_DLIGHTS];

// the cl_parse_entities must be large enough to hold UPDATE_BACKUP frames of
// entities, so that when a delta compressed message arives from the server
// it can be un-deltad from the original 
#define	MAX_PARSE_ENTITIES	1024
extern	entity_state_t	cl_parse_entities[MAX_PARSE_ENTITIES];

//=============================================================================

bool CL_CheckOrDownloadFile( const char *filename );

void CL_TeleporterParticles (entity_state_t *ent);
void CL_ParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void CL_ParticleEffect2 (vec3_t org, vec3_t dir, int color, int count);

// RAFAEL
void CL_ParticleEffect3 (vec3_t org, vec3_t dir, int color, int count);


//=================================================

// ========
// PGM
typedef struct cparticle_s
{
	struct cparticle_s	*next;

	float		time;

	vec3_t		org;
	vec3_t		vel;
	vec3_t		accel;
	float		color;
	float		colorvel;
	float		alpha;
	float		alphavel;
} cparticle_t;

//
// cinematic.c
//
bool CIN_PlayCinematic( const char *filename );		// play cinematic with specified name
void CIN_DrawCinematic( void );			// draw current frame
void CIN_RunCinematic( void );			// decompress next frame
void CIN_StopCinematic( void );			// stop video playing

#define PARTICLE_GRAVITY	40
#define INSTANT_PARTICLE	-10000.0
void CL_TeleportSplash( vec3_t org );
int CL_ParseEntityBits( sizebuf_t *msg, uint *bits );
void CL_ParseFrame( sizebuf_t *msg );

void CL_ParseTempEnts( sizebuf_t *msg );
void CL_ParseConfigString( sizebuf_t *msg );
void CL_WriteConfiguration( void );
void CL_SetLightstyle (int i);
void CL_RunParticles (void);
void CL_RunDLights (void);
void CL_RunLightStyles (void);

void CL_AddEntities (void);
void CL_AddDLights (void);
void CL_AddLightStyles (void);

//=================================================

void CL_PrepVideo( void );
void CL_PrepSound( void );

//
// cl_cmds.c
//
void CL_Quit_f (void);
void CL_ScreenShot_f( void );
void CL_LevelShot_f( void );
void CL_SetSky_f( void );
void CL_SetFont_f( void );
void SCR_Viewpos_f( void );

//
// cl_main
//
extern render_exp_t		*re;

void CL_Init( void );
void CL_Disconnect (void);
void CL_Disconnect_f (void);
void CL_GetChallengePacket (void);
void CL_PingServers_f (void);
void CL_Snd_Restart_f (void);
void CL_RequestNextDownload (void);

//
// cl_input.c
//
void CL_InitInput( void );
void CL_ShutdownInput( void );
void CL_UpdateMouse( void );
void CL_SendCmd (void);
void CL_SendMove (usercmd_t *cmd);

void CL_ClearState (void);

void CL_ReadPackets (void);

int  CL_ReadFromServer (void);
void CL_WriteToServer (usercmd_t *cmd);
void CL_BaseMove (usercmd_t *cmd);

void IN_CenterView (void);

void CL_CharEvent( int key );
char *Key_KeynumToString (int keynum);

//
// cl_demo.c
//
void CL_DrawDemoRecording( void );
void CL_WriteDemoMessage( sizebuf_t *msg, int head_size );
void CL_ReadDemoMessage( void );
void CL_StopPlayback( void );
void CL_StopRecord( void );
void CL_PlayDemo_f( void );
void CL_Record_f( void );
void CL_Stop_f( void );

//
// cl_progs.c
//
void CL_InitClientProgs( void );
void CL_FreeClientProgs( void );
int CL_GetMaxClients( void );
void CL_DrawHUD( void );
edict_t *CL_GetEdict( int entnum );
float *CL_FadeColor( float starttime, float endtime );
bool CL_ParseUserMessage( int svc_number );
void CL_FreeEdicts( void );
void CL_VM_Begin( void );
void CL_VM_End( void );

_inline edict_t *CLVM_EDICT_NUM( int entnum )
{
	edict_t	*ent;

	while( entnum >= prog->max_edicts ) PRVM_MEM_IncreaseEdicts();
	ent = PRVM_EDICT_NUM( entnum );
	memset(ent->progs.cl, 0, prog->progs->entityfields * 4);
	ent->priv.cl->free = false;

	return ent;
}

//
// cl_sound.c
//
#define S_Shutdown			se->Shutdown

// if origin is NULL, the sound will be dynamically sourced from the entity
#define S_StartStreaming		se->StartStreaming
#define S_RegisterSound		se->RegisterSound
#define S_StartSound( a,b,c,d,e,f )	se->StartSound( a, b, c, d, e, f, true );
#define S_StartLocalSound		se->StartLocalSound
#define S_StartBackgroundTrack	se->StartBackgroundTrack
#define S_StopBackgroundTrack		se->StopBackgroundTrack
#define S_RawSamples 		se->StreamRawSamples
#define S_StopAllSounds		se->StopAllSounds

// recompute the reletive volumes for all running sounds
// reletive to the given entityNum / orientation
#define S_Update			se->Frame
#define S_BeginRegistration		se->BeginRegistration
#define S_EndRegistration		se->EndRegistration

//
// cl_parse.c
//
void CL_ParseServerMessage( sizebuf_t *msg );
void CL_Download_f( void );

//
// cl_scrn.c
//
void SCR_Init( void );
void SCR_UpdateScreen( void );
void VID_Init( void );
void SCR_Shutdown( void );
void SCR_AdjustSize( float *x, float *y, float *w, float *h );
void SCR_DrawPic( float x, float y, float width, float height, const char *picname );
void SCR_FillRect( float x, float y, float width, float height, const float *color );
void SCR_DrawSmallChar( int x, int y, int ch );
void SCR_DrawChar( int x, int y, float w, float h, int ch );
void SCR_DrawSmallStringExt( int x, int y, const char *string, float *setColor, bool forceColor );
void SCR_DrawStringExt( int x, int y, float w, float h, const char *string, float *setColor, bool forceColor );
void SCR_DrawBigString( int x, int y, const char *s, float alpha );
void SCR_DrawBigStringColor( int x, int y, const char *s, vec4_t color );
void SCR_DrawFPS( void );
void SCR_DrawNet( void );

//
// cl_view.c
//

void V_Init (void);
void V_CalcRect( void );
void V_Shutdown( void );
bool V_PreRender( void );
void V_RenderHUD( void );
void V_PostRender( void );
void V_RenderView( void );
void V_RenderLogo( void );
void V_RenderSplash( void );
float V_CalcFov( float fov_x, float width, float height );

//
// cl_pred.c
//
void CL_InitPrediction (void);
void CL_PredictMove (void);
void CL_CheckPredictionError (void);

//
// cl_ents.c
//
void CL_GetEntitySoundSpatialization( int ent, vec3_t origin, vec3_t velocity );
void CL_AddLoopingSounds( void );

//
// cl_fx.c
//
cdlight_t *CL_AllocDlight (int key);
void CL_AddParticles (void);
void CL_ClearEffects( void );
void CL_StudioEvent( mstudioevent_t *event, entity_state_t *ent );
entity_state_t *CL_GetEdictByIndex( int index );
entity_state_t *CL_GetLocalPlayer( void );

//
// cl_pred.c
//
void CL_PredictMovement (void);

//
// cl_con.c
//
int Con_PrintStrlen( const char *string );
bool Con_Active( void );
void Con_CheckResize( void );
void Con_Print( const char *txt );
void Con_Init( void );
void Con_Clear_f( void );
void Con_ToggleConsole_f( void );
void Con_DrawNotify( void );
void Con_ClearNotify( void );
void Con_RunConsole( void );
void Con_DrawConsole( void );
void Con_PageUp( void );
void Con_PageDown( void );
void Con_Top( void );
void Con_Bottom( void );
void Con_Close( void );

extern bool chat_team;
extern bool anykeydown;
extern int g_console_field_width;
extern field_t historyEditLines[COMMAND_HISTORY];
extern field_t g_consoleField;
extern field_t chatField;

//
// cl_menu.c
//
extern bool ui_active;
extern const int vm_ui_numbuiltins;
extern prvm_builtin_t vm_ui_builtins[];

void UI_Init( void );
void UI_DrawCredits( void );
void UI_KeyEvent( int key );
void UI_ShowMenu( void );
void UI_HideMenu( void );
void UI_Shutdown( void );
void UI_Draw( void );

//
// cl_keys.c
//
void Field_Clear( field_t *edit );
void Field_CharEvent( field_t *edit, int ch );
void Field_KeyDownEvent( field_t *edit, int key );
void Field_Draw( field_t *edit, int x, int y, int width, bool showCursor );
void Field_BigDraw( field_t *edit, int x, int y, int width, bool showCursor );

bool Key_IsDown( int keynum );
char *Key_IsBind( int keynum );
void Key_Event (int key, bool down, uint time);
void Key_Init (void);
void Key_WriteBindings (file_t *f);
void Key_SetBinding (int keynum, char *binding);
void Key_ClearStates (void);
char *Key_KeynumToString (int keynum);
int Key_StringToKeynum (char *str);
int Key_GetKey( char *binding );
void Key_EnumCmds_f( void );

//
// cl_cin.c
//
bool SCR_PlayCinematic( char *name, int bits );
void SCR_DrawCinematic( void );
void SCR_RunCinematic( void );
void SCR_StopCinematic( void );
void SCR_ResetCinematic( void );
int SCR_GetCinematicState( void );
void CL_PlayVideo_f( void );

#endif//CLIENT_H