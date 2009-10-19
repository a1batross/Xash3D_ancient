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
#include "entity_def.h"
#include "clgame_api.h"
#include "render_api.h"
#include "pm_movevars.h"

#define MAX_DEMOS		32
#define MAX_EDIT_LINE	256
#define COMMAND_HISTORY	32
#define MAX_SERVERS		64
#define ColorIndex(c)	(((c) - '0') & 7)

#define NUM_FOR_EDICT(e) ((int)((edict_t *)(e) - clgame.edicts))
#define EDICT_NUM( num ) CL_EDICT_NUM( num, __FILE__, __LINE__ )
#define STRING( offset ) CL_GetString( offset )
#define MAKE_STRING(str) CL_AllocString( str )

//=============================================================================
typedef struct frame_s
{
	bool		valid;			// cleared if delta parsing was invalid
	int		serverframe;
	int		servertime;
	int		deltaframe;
	byte		areabits[MAX_MAP_AREA_BYTES];	// portalarea visibility bits
	int		num_entities;
	int		parse_entities;		// non-masked index into cl_parse_entities array
	entity_state_t	ps;			// player state
} frame_t;

typedef struct
{
	int		cmd_number;		// cl.cmd_number when packet was sent
	int		servertime;		// usercmd->servertime when packet was sent
	int		realtime;			// cls.realtime when packet was sent
} cmdframe_t;

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

// the cl_parse_entities must be large enough to hold UPDATE_BACKUP frames of
// entities, so that when a delta compressed message arives from the server
// it can be un-deltad from the original 
#define MAX_PARSE_ENTITIES		2048

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

	int		cmd_number;
	usercmd_t		cmds[CMD_BACKUP];		// each mesage will send several old cmds
	cmdframe_t	cmdframes[UPDATE_BACKUP];

	frame_t		frame;			// received from server
	frame_t		*oldframe;		// previous frame to lerping from
	int		surpressCount;		// number of messages rate supressed
	frame_t		frames[UPDATE_BACKUP];

	// mouse current position
	int		mouse_x[2];
	int		mouse_y[2];
	int		mouse_step;

	int		mtime[2];		// the timestamp of the last two messages
	int		time;		// this is the time value that the client
					// is rendering at.  always <= cls.realtime
	int		render_flags;	// clearing at end of frame
	float		lerpFrac;		// interpolation value
	ref_params_t	refdef;		// shared refdef
	client_data_t	data;		// hud data

	// predicting stuff
	int		predicted_origins[CMD_BACKUP][3];// for debug comparing against server

	float		predicted_step;		// for stair up smoothing
	uint		predicted_step_time;

	vec3_t		predicted_origin;		// generated by CL_PredictMovement
	vec3_t		predicted_angles;
	vec3_t		prediction_error;

	//
	// server state information
	//
	int		playernum;
	int		servercount;			// server identification for prespawns
	int		serverframetime;			// server frametime
	char		configstrings[MAX_CONFIGSTRINGS][CS_SIZE];

	entity_state_t	entity_curstates[MAX_PARSE_ENTITIES];
	entity_state_t	entity_baselines[MAX_EDICTS];		// keep all baselines in one global array

	// locally derived information from server state
	cmodel_t		*models[MAX_MODELS];
	cmodel_t		*worldmodel;
	string_t		edict_classnames[MAX_CLASSNAMES];
	sound_t		sound_precache[MAX_SOUNDS];
	shader_t		decal_shaders[MAX_DECALS];
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

typedef enum
{
	scrshot_inactive,
	scrshot_plaque,  	// levelshot
	scrshot_savegame,	// saveshot
} e_scrshot;

typedef struct
{
	byte		open;		// 0 = mouth closed, 255 = mouth agape
	byte		sndcount;		// counter for running average
	int		sndavg;		// running average
} mouth_t;

// cl_private_edict_t
struct cl_priv_s
{
	int		serverframe;	// if not current, this ent isn't in the frame

	entity_state_t	current;
	entity_state_t	prev;		// will always be valid, but might just be a copy of current
	prevframe_t	latched;		// previous frame to lerping from

	// studiomodels attachments
	vec3_t		origin[MAXSTUDIOATTACHMENTS];
	vec3_t		angles[MAXSTUDIOATTACHMENTS];

	mouth_t		mouth;		// shared mouth info
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

typedef enum { key_console = 0, key_game, key_hudmenu, key_message, key_menu } keydest_t;

typedef struct
{
	char		name[CS_SIZE];
	int		number;	// svc_ number
	int		size;	// if size == -1, size come from first byte after svcnum
	pfnUserMsgHook	func;	// user-defined function	
} user_message_t;

typedef struct
{
	void		*hInstance;		// pointer to client.dll
	HUD_FUNCTIONS	dllFuncs;			// dll exported funcs
	byte		*private;			// client.dll private pool
	string		maptitle;			// display map title

	union
	{
		edict_t	*edicts;			// acess by edict number
		void	*vp;			// acess by offset in bytes
	};

	// misc 2d drawing stuff
	float		centerPrintTime;
	int		centerPrintCharWidth;
	int		centerPrintY;
	char		centerPrint[1024];
	int		centerPrintLines;

	cl_globalvars_t	*globals;
	user_message_t	*msg[MAX_USER_MESSAGES];

	edict_t		viewent;			// viewmodel or playermodel in UI_PlayerSetup
	edict_t		playermodel;		// uiPlayerSetup latched vars
	
	int		numMessages;		// actual count of user messages
	int		hStringTable;		// stringtable handle

	// movement values from server
	movevars_t	movevars;
} clgame_static_t;

typedef struct
{
	connstate_t	state;
	bool		initialized;

	keydest_t		key_dest;

	byte		*mempool;			// client premamnent pool: edicts etc
	
	int		framecount;
	float		frametime;		// seconds since last frame
	int		realtime;

	int		quakePort;		// a 16 bit value that allows quake servers
						// to work around address translating routers

	// connection information
	string		servername;		// name of server from original connect
	int		connect_time;		// for connection retransmits

	netchan_t		netchan;
	int		serverProtocol;		// in case we are doing some kind of version hack

	int		challenge;		// from the server to use for connecting
	shader_t		consoleFont;		// current console font
	shader_t		clientFont;		// current client font
	shader_t		consoleBack;		// console background
	shader_t		fillShader;		// used for emulate FillRGBA to avoid wrong draw-sort
	shader_t		netIcon;			// netIcon displayed bad network connection
	
	file_t		*download;		// file transfer from server
	string		downloadname;
	string		downloadtempname;
	int		downloadnumber;
	dltype_t		downloadtype;

	e_scrshot		scrshot_request;		// request for screen shot
	e_scrshot		scrshot_action;		// in-action
	string		shotname;

	// demo loop control
	int		demonum;			// -1 = don't play demos
	string		demos[MAX_DEMOS];		// when not playing

	// demo recording info must be here, so it isn't clearing on level change
	bool		demorecording;
	bool		demoplayback;
	bool		demowaiting;		// don't record until a non-delta message is received
	bool		drawplaque;		// draw plaque when level is loading
	string		demoname;			// for demo looping

	file_t		*demofile;
	serverinfo_t	serverlist[MAX_SERVERS];	// servers to join
	int		numservers;
	int		pingtime;			// servers timebase
} client_static_t;

extern client_static_t	cls;
extern clgame_static_t	clgame;

/*
==============================================================

SCREEN CONSTS

==============================================================
*/
extern rgba_t g_color_table[8];

//
// cvars
//
extern cvar_t	*cl_predict;
extern cvar_t	*cl_maxpackets;
extern cvar_t	*cl_packetdup;
extern cvar_t	*cl_showfps;
extern cvar_t	*cl_upspeed;
extern cvar_t	*cl_forwardspeed;
extern cvar_t	*cl_backspeed;
extern cvar_t	*cl_sidespeed;
extern cvar_t	*cl_shownet;
extern cvar_t	*cl_yawspeed;
extern cvar_t	*cl_pitchspeed;
extern cvar_t	*cl_envshot_size;
extern cvar_t	*cl_run;
extern cvar_t	*cl_font;
extern cvar_t	*cl_anglespeedkey;
extern cvar_t	*cl_showmiss;
extern cvar_t	*cl_particles;
extern cvar_t	*cl_particlelod;
extern cvar_t	*cl_testentities;
extern cvar_t	*cl_testlights;
extern cvar_t	*cl_testflashlight;
extern cvar_t	*cl_paused;
extern cvar_t	*cl_levelshot_name;
extern cvar_t	*scr_centertime;
extern cvar_t	*scr_showpause;
extern cvar_t	*con_font;

//=============================================================================

bool CL_CheckOrDownloadFile( const char *filename );

//=================================================
//
// cinematic.c
//
bool CIN_PlayCinematic( const char *filename );		// play cinematic with specified name
void CIN_DrawCinematic( void );			// draw current frame
void CIN_RunCinematic( void );			// decompress next frame
void CIN_StopCinematic( void );			// stop video playing

void CL_TeleportSplash( vec3_t org );
int CL_ParseEntityBits( sizebuf_t *msg, uint *bits );
void CL_ParseFrame( sizebuf_t *msg );

void CL_ParseTempEnts( sizebuf_t *msg );
void CL_ParseConfigString( sizebuf_t *msg );
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
void CL_EnvShot_f( void );
void CL_SkyShot_f( void );
void CL_SaveShot_f( void );
void CL_LevelShot_f( void );
void CL_SetSky_f( void );
void CL_SetFont_f( void );
void SCR_Viewpos_f( void );

//
// cl_main
//
extern render_exp_t		*re;

void CL_Init( void );
void CL_SendCommand( void );
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
void CL_StartDemos_f( void );
void CL_NextDemo( void );
void CL_Demos_f( void );
void CL_Record_f( void );
void CL_Stop_f( void );

//
// cl_progs.c
//
void CL_InitClientProgs( void );
void CL_FreeClientProgs( void );
int CL_GetMaxClients( void );
void CL_DrawHUD( int state );
edict_t *CL_GetEdict( int entnum );
void CL_FadeAlpha( float starttime, float endtime, rgba_t color );
void CL_InitEdicts( void );
void CL_FreeEdicts( void );

//
// cl_game.c
//
void CL_UnloadProgs( void );
bool CL_LoadProgs( const char *name );
void CL_ParseUserMessage( sizebuf_t *msg, int svc_num );
void CL_LinkUserMessage( char *pszName, const int svc_num );
void CL_SortUserMessages( void );
int CL_GetServerTime( void );
float CL_GetLerpFrac( void );
edict_t *CL_AllocEdict( void );
void CL_InitEdict( edict_t *pEdict );
void CL_FreeEdict( edict_t *pEdict );
bool CL_GetAttachment( int entityIndex, int number, vec3_t origin, vec3_t angles );
bool CL_SetAttachment( int entityIndex, int number, vec3_t origin, vec3_t angles );
prevframe_t *CL_GetPrevFrame( int entityIndex );
float CL_GetMouthOpen( int entityIndex );
string_t CL_AllocString( const char *szValue );
const char *CL_GetString( string_t iString );
bool CL_RenderTrace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end );

_inline edict_t *CL_EDICT_NUM( int n, const char *file, const int line )
{
	if((n >= 0) && (n < clgame.globals->maxEntities))
		return clgame.edicts + n;
	Host_Error( "CL_EDICT_NUM: bad number %i (called at %s:%i)\n", n, file, line );
	return NULL;	
}

//
// cl_sound.c
//
#define S_Shutdown			if( se ) se->Shutdown

// if origin is NULL, the sound will be dynamically sourced from the entity
#define S_StartStreaming		if( se ) se->StartStreaming
#define S_StartSound		if( se ) se->StartSound
#define S_StartLocalSound		if( se ) se->StartLocalSound
#define S_StartBackgroundTrack	if( se ) se->StartBackgroundTrack
#define S_StopBackgroundTrack		if( se ) se->StopBackgroundTrack
#define S_RawSamples 		if( se ) se->StreamRawSamples
#define S_StopAllSounds		if( se ) se->StopAllSounds
#define S_AddLoopingSound		if( se ) se->AddLoopingSound

_inline sound_t S_RegisterSound( const char *name )
{
	if( se )
		return se->RegisterSound( name );
	return 0;
} 

// recompute the reletive volumes for all running sounds
// reletive to the given entityNum / orientation
#define S_Activate			if( se ) se->Activate
#define S_Update			if( se ) se->RenderFrame
#define S_BeginRegistration		if( se ) se->BeginRegistration
#define S_EndRegistration		if( se ) se->EndRegistration

//
// cl_parse.c
//
void CL_ParseServerMessage( sizebuf_t *msg );
void CL_RunBackgroundTrack( void );
void CL_Download_f( void );

//
// cl_scrn.c
//
void SCR_Init( void );
void SCR_UpdateScreen( void );
void SCR_Shutdown( void );
void SCR_RegisterShaders( void );
void SCR_AdjustSize( float *x, float *y, float *w, float *h );
void SCR_DrawPic( float x, float y, float width, float height, shader_t shader );
void SCR_FillRect( float x, float y, float width, float height, const rgba_t color );
void SCR_DrawSmallChar( int x, int y, int ch );
void SCR_DrawChar( int x, int y, float w, float h, int ch );
void SCR_DrawSmallStringExt( int x, int y, const char *string, rgba_t setColor, bool forceColor );
void SCR_DrawStringExt( int x, int y, float w, float h, const char *string, rgba_t setColor, bool forceColor );
void SCR_DrawBigString( int x, int y, const char *s, byte alpha );
void SCR_DrawBigStringColor( int x, int y, const char *s, rgba_t color );
void SCR_MakeScreenShot( void );
void SCR_MakeLevelShot( void );
void SCR_RSpeeds( void );
void SCR_DrawFPS( void );
void SCR_DrawNet( void );

//
// cl_view.c
//

void V_Init (void);
void V_Shutdown( void );
void V_ClearScene( void );
bool V_PreRender( void );
void V_PostRender( void );
void V_RenderView( void );
float V_CalcFov( float fov_x, float width, float height );

//
// cl_phys.c
//
void CL_InitPrediction (void);
void CL_PredictMove (void);
void CL_CheckPredictionError( void );
void CL_CheckVelocity( edict_t *ent );
bool CL_CheckWater( edict_t *ent );
int CL_PointContents( const vec3_t point );
int CL_ContentsMask( const edict_t *passedict );
trace_t CL_Trace( const vec3_t s1, const vec3_t m1, const vec3_t m2, const vec3_t s2, int type, edict_t *e, int mask );

//
// cl_frame.c
//
void CL_GetEntitySoundSpatialization( int ent, vec3_t origin, vec3_t velocity );
void CL_AddLoopingSounds( void );

//
// cl_fx.c
//
void CL_AddParticles( void );
void CL_AddDecals( void );
void CL_ClearEffects( void );
void CL_TestLights( void );
void CL_TestEntities( void );
void CL_StudioEvent( dstudioevent_t *event, edict_t *ent );
edict_t *CL_GetEdictByIndex( int index );
edict_t *CL_GetLocalPlayer( void );
void CL_StudioFxTransform( edict_t *ent, float transform[4][4] );
bool pfnAddParticle( cparticle_t *src, HSPRITE shader, int flags );
void pfnAddDecal( float *org, float *dir, float *rgba, float rot, float rad, HSPRITE hSpr, int flags );
void pfnAddDLight( const float *org, const float *rgb, float radius, float time, int flags, int key );

//
// cl_pred.c
//
void CL_PredictMovement (void);

//
// cl_con.c
//
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
typedef enum { UI_CLOSEMENU, UI_MAINMENU } uiActiveMenu_t;

void UI_UpdateMenu( int realtime );
void UI_KeyEvent( int key );
void UI_MouseMove( int x, int y );
void UI_SetActiveMenu( uiActiveMenu_t activeMenu );
void UI_AddServerToList( netadr_t adr, const char *info );
bool UI_CreditsActive( void );
bool UI_IsVisible( void );
void UI_Precache( void );
void UI_Init( void );
void UI_Shutdown( void );

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
void Key_Event( int key, bool down, int time );
void Key_Init( void );
void Key_WriteBindings( file_t *f );
void Key_SetBinding( int keynum, char *binding );
void Key_ClearStates( void );
char *Key_KeynumToString( int keynum );
int Key_StringToKeynum( char *str );
int Key_GetKey( const char *binding );
void Key_EnumCmds_f( void );
void Key_SetKeyDest( int key_dest );

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