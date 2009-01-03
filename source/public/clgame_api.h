//=======================================================================
//			Copyright XashXT Group 2008 ©
//	      clgame_api.h - entity interface between engine and clgame
//=======================================================================
#ifndef CLGAME_API_H
#define CLGAME_API_H

typedef int HSPRITE;						// handle to a graphic
typedef int (*pfnUserMsgHook)( const char *pszName, int iSize, void *pbuf );	// user message handle

typedef enum 
{
	TRI_FRONT = 0,
	TRI_BACK,
	TRI_NONE,
} TRI_CULL;

typedef enum
{
	TRI_TRIANGLES = 0,
	TRI_TRIANGLE_FAN,
	TRI_TRIANGLE_STRIP,
	TRI_POLYGON,
	TRI_QUADS,
	TRI_LINES,	
} TRI_DRAW;

typedef struct triapi_s
{
	size_t	api_size;			// must match with sizeof( triapi_t );

	void	(*Bind)( HSPRITE shader );	// use handle that return pfnLoadShader
	void	(*Begin)( TRI_DRAW mode );
	void	(*End)( void );

	void	(*Vertex2f)( float x, float y );
	void	(*Vertex3f)( float x, float y, float z );
	void	(*Vertex2fv)( const float *v );
	void	(*Vertex3fv)( const float *v );
	void	(*Color3f)( float r, float g, float b );
	void	(*Color4f)( float r, float g, float b, float a );
	void	(*Color4ub)( byte r, byte g, byte b, byte a );
	void	(*TexCoord2f)( float u, float v );
	void	(*TexCoord2fv)( const float *v );
	void	(*CullFace)( TRI_CULL mode );
} triapi_t;

// FIXME: get rid of this
typedef struct
{
	char	*name;
	short	ping;
	byte	thisplayer;	// TRUE if this is the calling player

	// stuff that's unused at the moment,  but should be done
	byte	spectator;
	byte	packetloss;

	char	*model;
	short	topcolor;
	short	bottomcolor;

} hud_player_info_t;

// FIXME: get rid of this
typedef struct client_textmessage_s
{
	int	effect;
	byte	r1, g1, b1, a1;		// 2 colors for effects
	byte	r2, g2, b2, a2;
	float	x;
	float	y;
	float	fadein;
	float	fadeout;
	float	holdtime;
	float	fxtime;
	const char *pName;
	const char *pMessage;
} client_textmessage_t;

// NOTE: engine trace struct not matched with clgame trace
typedef struct
{
	BOOL		fAllSolid;	// if true, plane is not valid
	BOOL		fStartSolid;	// if true, the initial point was in a solid area
	BOOL		fStartStuck;	// if true, trace started from solid entity
	float		flFraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		vecEndPos;	// final position
	int		iStartContents;	// start pos conetnts
	int		iContents;	// final pos contents
	int		iHitgroup;	// 0 == generic, non zero is specific body part
	float		flPlaneDist;	// planes distance
	vec3_t		vecPlaneNormal;	// surface normal at impact
	const char	*pTexName;	// texture name that we hitting (brushes and studiomodels)
	edict_t		*pHit;		// entity the surface is on
} TraceResult;

typedef struct ref_params_s
{
	// output
	int	viewport[4];	// x, y, width, height
	vec3_t	vieworg;
	vec3_t	viewangles;
	float	fov_x;
	float	fov_y;		// fov_y = V_CalcFov( fov_x, viewport[2], viewport[3] );

	vec3_t	forward;
	vec3_t	right;
	vec3_t	up;
	
	float	frametime;	// client frametime
	float	lerpfrac;		// between oldframe and frame
	float	time;		// client time
	float	oldtime;		// studio lerping

	// misc
	BOOL	intermission;
	BOOL	demoplayback;
	BOOL	demorecord;
	BOOL	spectator;
	BOOL	paused;
	uint	rdflags;		// client view effects: RDF_UNDERWATER, RDF_MOTIONBLUR, etc
	int	iWeaponBits;	// pev->weapon
	int	iKeyBits;		// pev->button
	edict_t	*onground;	// pointer to onground entity
	byte	*areabits;	// come from server, contains visible areas list
	int	waterlevel;

	// input
	vec3_t	velocity;
	vec3_t	angles;		// input viewangles
	vec3_t	origin;		// origin + viewheight = vieworg
	vec3_t	old_angles;	// prev.state values to interpolate from
	vec3_t	old_origin;

	vec3_t	viewheight;
	float	idealpitch;
	float	v_idlescale;	// used for concussion effect
	float	mouse_sensitivity;

	int	health;
	vec3_t	crosshairangle;	// pfnCrosshairAngle values from server
	vec3_t	punchangle;	// recivied from server
	edict_t	*viewentity;
	int	clientnum;
	int	num_entities;
	int	max_entities;
	int	max_clients;
} ref_params_t;

typedef struct cl_enginefuncs_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(cl_enginefuncs_t)

	// engine memory manager
	void*	(*pfnMemAlloc)( size_t cb, const char *filename, const int fileline );
	void	(*pfnMemFree)( void *mem, const char *filename, const int fileline );

	// screen handlers
	HSPRITE	(*pfnLoadShader)( const char *szShaderName );
	void	(*pfnFillRGBA)( int x, int y, int width, int height, const float *color, float alpha );
	void	(*pfnDrawImage)( HSPRITE shader, int x, int y, int width, int height );
	void	(*pfnDrawImageExt)( HSPRITE shader, int x, int y, int w, int h, float s1, float t1, float s2, float t2 );
	void	(*pfnSetColor)( float r, float g, float b, float a );

	// cvar handlers
	void	(*pfnRegisterVariable)( const char *szName, const char *szValue, int flags, const char *szDesc );
	void	(*pfnCvarSetValue)( const char *cvar, float value );
	float	(*pfnGetCvarFloat)( const char *szName );
	char*	(*pfnGetCvarString)( const char *szName );

	// command handlers
	void	(*pfnAddCommand)( const char *cmd_name, void (*function)(void), const char *cmd_desc );
	void	(*pfnHookUserMsg)( const char *szMsgName, pfnUserMsgHook pfn );
	void	(*pfnServerCmd)( const char *szCmdString );
	void	(*pfnClientCmd)( const char *szCmdString );
	void	(*pfnGetPlayerInfo)( int player_num, hud_player_info_t *pinfo );
	client_textmessage_t *(*pfnTextMessageGet)( const char *pName );

	int       (*pfnCmdArgc)( void );	
	char	*(*pfnCmdArgv)( int argc );
	void	(*pfnAlertMessage)( ALERT_TYPE, char *szFmt, ... );
	
	// sound handlers (NULL origin == play at current client origin)
	void	(*pfnPlaySoundByName)( const char *szSound, float volume, const float *org );
	void	(*pfnPlaySoundByIndex)( int iSound, float volume, const float *org );

	// vector helpers
	void	(*pfnAngleVectors)( const float *rgflVector, float *forward, float *right, float *up );

	void	(*pfnDrawCenterPrint)( void );
	void	(*pfnCenterPrint)( const char *text, int y, int charWidth );
	int	(*pfnDrawCharacter)( int x, int y, int width, int height, int number );
	void	(*pfnDrawString)( int x, int y, int width, int height, const char *text );
	void	(*pfnGetImageSize)( int *w, int *h, int frame, shader_t shader );
	void	(*pfnSetParms)( shader_t handle, kRenderMode_t rendermode, int frame );

	// local client handlers
	void	(*pfnGetViewAngles)( float *angles );
	edict_t*	(*pfnGetEntityByIndex)( int idx );	// matched with entity serialnumber
	edict_t*	(*pfnGetLocalPlayer)( void );
	int	(*pfnIsSpectateOnly)( void );		// returns 1 if the client is a spectator only
	float	(*pfnGetClientTime)( void );
	int	(*pfnGetMaxClients)( void );
	edict_t*	(*pfnGetViewModel)( void );
	void	(*pfnMakeLevelShot)( void );		// level shot will be created at next frame

	int	(*pfnPointContents)( const float *rgflVector );
	void	(*pfnTraceLine)( const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr );

	long	(*pfnRandomLong)( long lLow, long lHigh );
	float	(*pfnRandomFloat)( float flLow, float flHigh );
	byte*	(*pfnLoadFile)( const char *filename, int *pLength );
	int	(*pfnFileExists)( const char *filename );
	void	(*pfnGetGameDir)( char *szGetGameDir );
	void	(*pfnHostError)( const char *szFmt, ... );		// invoke host error

	triapi_t	*pTriAPI;

} cl_enginefuncs_t;

typedef struct
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(HUD_FUNCTIONS)

	int	(*pfnVidInit)( void );
	void	(*pfnInit)( void );
	int	(*pfnRedraw)( float flTime, int state );
	int	(*pfnUpdateClientData)( ref_params_t *parms, float flTime );
	void	(*pfnReset)( void );
	void	(*pfnFrame)( double time );
	void 	(*pfnShutdown)( void );
	void	(*pfnDrawNormalTriangles)( void );
	void	(*pfnDrawTransparentTriangles)( void );
	void	(*pfnCreateEntities)( void );
	void	(*pfnStudioEvent)( const dstudioevent_t *event, edict_t *entity );
	void	(*pfnCalcRefdef)( ref_params_t *parms );

} HUD_FUNCTIONS;

typedef int (*CLIENTAPI)( HUD_FUNCTIONS *pFunctionTable, cl_enginefuncs_t* pEngfuncsFromEngine, int interfaceVersion );

#endif//CLGAME_API_H