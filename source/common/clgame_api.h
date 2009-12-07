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

#include "trace_def.h"
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

	byte		spectator;
	byte		packetloss;
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

	int		serverflags;	// shared serverflags
	int		maxClients;
	int		numClients;
	int		maxEntities;
	int		numEntities;	// actual ents count
} cl_globalvars_t;

typedef struct cl_enginefuncs_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(cl_enginefuncs_t)

	// sprite handlers
	HSPRITE	(*pfnSPR_Load)( const char *szPicName );
	int	(*pfnSPR_Frames)( HSPRITE hPic );
	int	(*pfnSPR_Height)( HSPRITE hPic, int frame );
	int	(*pfnSPR_Width)( HSPRITE hPic, int frame );
	void	(*pfnSPR_Set)( HSPRITE hPic, int r, int g, int b, int a );
	void	(*pfnSPR_Draw)( int frame, int x, int y, int width, int height, const wrect_t *prc );
	void	(*pfnSPR_DrawHoles)( int frame, int x, int y, int width, int height, const wrect_t *prc );
	void	(*pfnSPR_DrawTrans)( int frame, int x, int y, int width, int height, const wrect_t *prc );	// Xash3D ext
	void	(*pfnSPR_DrawAdditive)( int frame, int x, int y, int width, int height, const wrect_t *prc );
	void	(*pfnSPR_EnableScissor)( int x, int y, int width, int height );
	void	(*pfnSPR_DisableScissor)( void );
	client_sprite_t *(*pfnSPR_GetList)( char *psz, int *piCount );

	// screen handlers
	void	(*pfnFillRGBA)( int x, int y, int width, int height, int r, int g, int b, int a );
	int	(*pfnGetScreenInfo)( SCREENINFO *pscrinfo );
	void	(*pfnSetCrosshair)( HSPRITE hspr, wrect_t rc, int r, int g, int b );

	// cvar handlers
	cvar_t*	(*pfnRegisterVariable)( const char *szName, const char *szValue, int flags, const char *szDesc );
	float	(*pfnGetCvarFloat)( const char *szName );
	char*	(*pfnGetCvarString)( const char *szName );

	// command handlers
	void	(*pfnAddCommand)( const char *cmd_name, void (*function)(void), const char *cmd_desc );
	void	(*pfnHookUserMsg)( const char *szMsgName, pfnUserMsgHook pfn );
	void	(*pfnServerCmd)( const char *szCmdString );
	void	(*pfnClientCmd)( const char *szCmdString );

	void	(*pfnGetPlayerInfo)( int player_num, hud_player_info_t *pinfo );

	// sound handlers (NULL origin == play at current client origin)
	void	(*pfnPlaySoundByName)( const char *szSound, float volume, int pitch, const float *org );
	void	(*pfnPlaySoundByIndex)( int iSound, float volume, int pitch, const float *org );

	// vector helpers
	void	(*pfnAngleVectors)( const float *rgflVector, float *forward, float *right, float *up );

	// text message system
	client_textmessage_t *(*pfnTextMessageGet)( const char *pName );
	int	(*pfnDrawCharacter)( int x, int y, int number, int r, int g, int b );
	int	(*pfnDrawConsoleString)( int x, int y, char *string );
	void	(*pfnDrawSetTextColor)( float r, float g, float b );
	void	(*pfnDrawConsoleStringLen)(  const char *string, int *length, int *height );

	void	(*pfnConsolePrint)( const char *string );
	void	(*pfnCenterPrint)( const char *string );

	// engine memory manager
	void*	(*pfnMemAlloc)( size_t cb, const char *filename, const int fileline );// was GetWindowCenterX
	void	(*pfnMemFree)( void *mem, const char *filename, const int fileline );	// was GetWindowCenterY

	// added for user input processing
	void	(*pfnGetViewAngles)( float *rgflAngles );
	void	(*pfnSetViewAngles)( float *rgflAngles );
	void	(*pfnCvarSetString)( const char *szName, const char *szValue );	// was GetMaxClients (see gpGlobals->maxClients)
	void	(*pfnCvarSetValue)( const char *szName, float flValue );

	int       (*pfnCmdArgc)( void );	
	const char* (*pfnCmdArgv)( int argc );
	const char *(*pfnCmd_Args)( void );					// was Con_Printf
	float	(*pfnGetLerpFrac)( void );					// was Con_DPrintf
	void	(*pfnDelCommand)( const char *cmd_name );			// was Con_NPrintf
	void	(*pfnAlertMessage)( ALERT_TYPE, char *szFmt, ... );		// was Con_NXPrintf

	const char* (*pfnPhysInfo_ValueForKey)( const char *key );
	const char* (*pfnServerInfo_ValueForKey)( const char *key );
	float	(*pfnGetClientMaxspeed)( void );
	void*	(*pfnGetModelPtr)( edict_t* pEdict );				// was CheckParm
	void	(*pfnGetBonePosition)( const edict_t* pEdict, int iBone, float *rgflOrigin, float *rgflAngles );	// was Key_Event
	string_t	(*pfnAllocString)( const char *szValue );			// was GetMousePosition
	const char *(*pfnGetString)( string_t iString );				// was IsNoClipping

	// edict handlers
	edict_t*	(*pfnGetLocalPlayer)( void );
	edict_t*	(*pfnGetViewModel)( void );
	edict_t*	(*pfnGetEntityByIndex)( int idx );	// matched with entity serialnumber

	float	(*pfnGetClientTime)( void );		// can use gpGlobals->time instead
	int	(*pfnIsSpectateOnly)( void );		// was V_CalcShake
	void	(*pfnGetAttachment)( const edict_t *pEdict, int iAttachment, float *rgflOrg, float *rgflAng );	// was V_ApplyShake

	int	(*pfnPointContents)( const float *rgflPos );
	edict_t*	(*pfnWaterEntity)( const float *rgflPos );
	void	(*pfnTraceLine)( const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr );
	void	(*pfnTraceToss)( edict_t* pent, edict_t* pentToIgnore, TraceResult *ptr );	// was CL_LoadModel
	void	(*pfnTraceHull)( const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr );
	void	(*pfnTraceModel)( const float *v1, const float *v2, edict_t *pent, TraceResult *ptr );	// was GetSpritePointer
	const char *(*pfnTraceTexture)( edict_t *pTextureEntity, const float *v1, const float *v2 ); // was pfnPlaySoundByNameAtLocation

	// filesystem handlers (event calls is completely moved to pEventAPI. see common\event_api.h for details)
	void*	(*pfnFOpen)( const char* path, const char* mode );	// was pfnPrecacheEvent
	long	(*pfnFRead)( void *file, void* buffer, size_t buffersize );	// was pfnPlaybackEvent
	long	(*pfnFWrite)(void *file, const void* data, size_t datasize);// was pfnWeaponAnim
	int	(*pfnFClose)( void *file );				// was pfnRandomFloat
	int	(*pfnFGets)( void *file, byte *string, size_t bufsize );	// was pfnRandomLong
	int	(*pfnFSeek)( void *file, long offset, int whence );	// was pfnHookEvent
	long	(*pfnFTell)( void *file );				// was Con_IsVisible

	// dlls managemenet
	void*	(*pfnLoadLibrary)( const char *name );			// was pfnGetGameDirectory
	void*	(*pfnGetProcAddress)( void *hInstance, const char *name );	// was pfnGetCvarPointer
	void	(*pfnFreeLibrary)( void *hInstance );			// was Key_LookupBinding
	void	(*pfnHostError)( const char *szFmt, ... );		// was pfnGetLevelName
	int	(*pfnFileExists)( const char *filename );		// was pfnGetScreenFade
	void	(*pfnRemoveFile)( const char *szFilename );		// was pfnSetScreenFade

	// vgui handlers
	void*	(*VGui_GetPanel)( void );				// UNDONE: wait for version 0.75
	void	(*VGui_ViewportPaintBackground)( int extents[4] );

	// parse txt files
	byte*	(*pfnLoadFile)( const char *filename, int *pLength );	// was COM_LoadFile, like it
	char 	*(*pfnParseToken)( const char **data_p );		// was COM_ParseFile, like it
	void	(*pfnFreeFile)( void *buffer );			// was COM_FreeFile, like it

	struct triapi_s	*pTriAPI;
	struct efxapi_s	*pEfxAPI;
	struct event_api_s	*pEventAPI;	

} cl_enginefuncs_t;

typedef struct
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(HUD_FUNCTIONS)

	int	(*pfnVidInit)( void );
	void	(*pfnInit)( void );
	int	(*pfnRedraw)( float flTime, int state );
	void	(*pfnUpdateEntityVars)( edict_t *out, skyportal_t *sky, const struct entity_state_s *in1, const struct entity_state_s *in2 );
	void	(*pfnReset)( void );
	void	(*pfnFrame)( double time );
	void 	(*pfnShutdown)( void );
	void	(*pfnDrawTriangles)( int fTrans );
	void	(*pfnCreateEntities)( void );
	void	(*pfnStudioEvent)( const dstudioevent_t *event, edict_t *entity );
	void	(*pfnStudioFxTransform)( edict_t *pEdict, float transform[4][4] );
	void	(*pfnCalcRefdef)( ref_params_t *parms );
	void	(*pfnPM_Move)( playermove_t *ppmove, int server );
	void	(*pfnPM_Init)( playermove_t *ppmove );
	char	(*pfnPM_FindTextureType)( const char *name );
	void	(*pfnCmdStart)( const edict_t *player, int runfuncs );
	void	(*pfnCmdEnd)( const edict_t *player, const usercmd_t *cmd, unsigned int random_seed );
	void	(*pfnCreateMove)( usercmd_t *cmd, int frametime, int active );
	void	(*pfnMouseEvent)( int mx, int my );
	int	(*pfnKeyEvent)( int down, int keynum, const char *pszBind );
	void	(*VGui_ConsolePrint)( const char *text );
} HUD_FUNCTIONS;

typedef int (*CLIENTAPI)( HUD_FUNCTIONS *pFunctionTable, cl_enginefuncs_t* engfuncs, cl_globalvars_t *pGlobals );

#endif//CLGAME_API_H