//=======================================================================
//			Copyright XashXT Group 2008 ©
//	      clgame_api.h - entity interface between engine and clgame
//=======================================================================
#ifndef CLGAME_API_H
#define CLGAME_API_H

typedef int		HSPRITE;					// handle to a graphic
typedef struct tempent_s	TEMPENTITY;
typedef struct usercmd_s	usercmd_t;
typedef struct cparticle_s	cparticle_t;
typedef struct skyportal_s	skyportal_t;
typedef struct ref_params_s	ref_params_t;
typedef struct dstudioevent_s	dstudioevent_t;
typedef int (*pfnUserMsgHook)( const char *pszName, int iSize, void *pbuf );	// user message handle
typedef void (*pfnEventHook)( struct event_args_s *args );

#include "trace_def.h"
#include "event_api.h"
#include "pm_shared.h"

#define SCRINFO_VIRTUALSPACE	1

typedef struct
{
	int		iFlags;
	int		iRealWidth;
	int		iRealHeight;
	int		iWidth;
	int		iHeight;
	int		iCharHeight;
	byte		charWidths[256];
} SCREENINFO;

typedef struct wrect_s
{
	int		left;
	int		right;
	int		top;
	int		bottom;
} wrect_t;

typedef struct
{
	char		*name;
	short		ping;
	byte		thisplayer;	// TRUE if this is the calling player
	byte		packetloss;	// TRUE if current packet is loose
	const char	*model;
	short		topcolor;
	short		bottomcolor;
} hud_player_info_t;

typedef struct client_textmessage_s
{
	int		effect;
	byte		r1, g1, b1, a1;	// 2 colors for effects
	byte		r2, g2, b2, a2;
	float		x;
	float		y;
	float		fadein;
	float		fadeout;
	float		holdtime;
	float		fxtime;
	const char	*pName;
	const char	*pMessage;
} client_textmessage_t;

typedef struct client_data_s
{
	vec3_t		origin;		// cl.origin
	vec3_t		angles;		// cl.viewangles

	int		iKeyBits;		// Keyboard bits
	int		iWeaponBits;	// came from pev->weapons
	float		v_idlescale;	// view shake/rotate
	float		mouse_sensitivity;	// used for menus and zoomed weapons
} client_data_t;

typedef struct client_sprite_s
{
	char		szName[64];
	char		szSprite[64];
	HSPRITE		hSprite;
	int		iRes;
	wrect_t		rc;
} client_sprite_t;

typedef struct cl_globalvars_s
{	
	float		time;		// time from server
	float		frametime;
	string_t		mapname;

	BOOL		deathmatch;
	BOOL		coop;
	BOOL		teamplay;

	int		serverflags;
	int		maxClients;
	int		maxEntities;
	int		numEntities;	// actual ents count
} cl_globalvars_t;

typedef struct cl_enginefuncs_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(cl_enginefuncs_t)

	// engine memory manager
	void*	(*pfnMemAlloc)( size_t cb, const char *filename, const int fileline );
	void	(*pfnMemCopy)( void *dest, const void *src, size_t cb, const char *filename, const int fileline );
	void	(*pfnMemFree)( void *mem, const char *filename, const int fileline );

	// screen handlers
	HSPRITE	(*pfnLoadShader)( const char *szShaderName, int fShaderNoMip );
	void	(*pfnFillRGBA)( int x, int y, int width, int height, byte r, byte g, byte b, byte alpha );
	void	(*pfnDrawImageExt)( HSPRITE shader, float x, float y, float w, float h, float s1, float t1, float s2, float t2 );
	void	(*pfnSetColor)( byte r, byte g, byte b, byte a );

	// cvar handlers
	cvar_t*	(*pfnRegisterVariable)( const char *szName, const char *szValue, int flags, const char *szDesc );
	void	(*pfnCvarSetString)( const char *szName, const char *szValue );
	void	(*pfnCvarSetValue)( const char *szName, float flValue );
	float	(*pfnGetCvarFloat)( const char *szName );
	char*	(*pfnGetCvarString)( const char *szName );

	// command handlers
	void	(*pfnAddCommand)( const char *cmd_name, void (*function)(void), const char *cmd_desc );
	void	(*pfnHookUserMsg)( const char *szMsgName, pfnUserMsgHook pfn );
	void	(*pfnDelCommand)( const char *cmd_name );
	void	(*pfnServerCmd)( const char *szCmdString );
	void	(*pfnClientCmd)( const char *szCmdString );
	void	(*pfnSetKeyDest)( int key_dest );

	void	(*pfnGetPlayerInfo)( int player_num, hud_player_info_t *pinfo );
	client_textmessage_t *(*pfnTextMessageGet)( const char *pName );

	int       (*pfnCmdArgc)( void );	
	char*	(*pfnCmdArgv)( int argc );
	void	(*pfnAlertMessage)( ALERT_TYPE, char *szFmt, ... );
	
	// sound handlers (NULL origin == play at current client origin)
	void	(*pfnPlaySoundByName)( const char *szSound, float volume, int pitch, const float *org );
	void	(*pfnPlaySoundByIndex)( int iSound, float volume, int pitch, const float *org );

	// vector helpers
	void	(*pfnAngleVectors)( const float *rgflVector, float *forward, float *right, float *up );

	void	(*pfnDrawCenterPrint)( void );
	void	(*pfnCenterPrint)( const char *text, int y, int charWidth );
	void	(*pfnDrawString)( int x, int y, int width, int height, const char *text );
	void	(*pfnGetParms)( int *w, int *h, int *frames, int frame, shader_t shader );
	void	(*pfnSetParms)( shader_t handle, kRenderMode_t rendermode, int frame );

	// local client handlers
	void	(*pfnGetViewAngles)( float *angles );
	edict_t*	(*pfnGetEntityByIndex)( int idx );	// matched with entity serialnumber
	edict_t*	(*pfnGetLocalPlayer)( void );
	int	(*pfnIsSpectateOnly)( void );		// returns 1 if the client is a spectator only
	float	(*pfnGetClientTime)( void );
	float	(*pfnGetLerpFrac)( void );
	int	(*pfnGetMaxClients)( void );
	edict_t*	(*pfnGetViewModel)( void );
	void*	(*pfnGetModelPtr)( edict_t* pEdict );

	int	(*pfnGetScreenInfo)( SCREENINFO *pscrinfo );
	void	(*pfnGetAttachment)( const edict_t *pEdict, int iAttachment, float *rgflOrigin, float *rgflAngles );

	int	(*pfnPointContents)( const float *rgflVector );
	void	(*pfnTraceLine)( const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr );
	void	(*pfnTraceToss)( edict_t* pent, edict_t* pentToIgnore, TraceResult *ptr );
	void	(*pfnTraceHull)( const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr );
	void	(*pfnTraceModel)( const float *v1, const float *v2, edict_t *pent, TraceResult *ptr );
	const char *(*pfnTraceTexture)( edict_t *pTextureEntity, const float *v1, const float *v2 );

	word	(*pfnPrecacheEvent)( int type, const char* psz );
	void	(*pfnHookEvent)( const char *name, pfnEventHook pfn );
	void	(*pfnPlaybackEvent)( int flags, const edict_t *pInvoker, word eventindex, float delay, event_args_t *args );
	void	(*pfnKillEvent)( word eventindex );

	string_t	(*pfnAllocString)( const char *szValue );
	const char *(*pfnGetString)( string_t iString );

	long	(*pfnRandomLong)( long lLow, long lHigh );
	float	(*pfnRandomFloat)( float flLow, float flHigh );
	byte*	(*pfnLoadFile)( const char *filename, int *pLength );
	int	(*pfnFileExists)( const char *filename );
	void	(*pfnRemoveFile)( const char *szFilename );
	void*	(*pfnLoadLibrary)( const char *name );
	void*	(*pfnGetProcAddress)( void *hInstance, const char *name );
	void	(*pfnFreeLibrary)( void *hInstance );
	void	(*pfnHostError)( const char *szFmt, ... );		// invoke host error

	struct triapi_s *pTriAPI;
	struct efxapi_s *pEfxAPI;

} cl_enginefuncs_t;

typedef struct
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(HUD_FUNCTIONS)

	int	(*pfnVidInit)( void );
	void	(*pfnInit)( void );
	int	(*pfnRedraw)( float flTime, int state );
	int	(*pfnUpdateClientData)( client_data_t *cdata, float flTime );
	void	(*pfnUpdateEntityVars)( edict_t *out, skyportal_t *sky, const struct entity_state_s *in1, const struct entity_state_s *in2 );
	void	(*pfnReset)( void );
	void	(*pfnFrame)( double time );
	void 	(*pfnShutdown)( void );
	void	(*pfnDrawTriangles)( void );
	void	(*pfnCreateEntities)( void );
	void	(*pfnStudioEvent)( const dstudioevent_t *event, edict_t *entity );
	void	(*pfnStudioFxTransform)( edict_t *pEdict, float transform[4][4] );
	void	(*pfnCalcRefdef)( ref_params_t *parms );
	void	(*pfnStartPitchDrift)( void );
	void	(*pfnStopPitchDrift)( void );
} HUD_FUNCTIONS;

typedef int (*CLIENTAPI)( HUD_FUNCTIONS *pFunctionTable, cl_enginefuncs_t* engfuncs, cl_globalvars_t *pGlobals );

#endif//CLGAME_API_H