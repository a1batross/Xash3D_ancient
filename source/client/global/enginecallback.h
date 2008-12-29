//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     enginecallback.h - actual engine callbacks
//=======================================================================

#ifndef ENGINECALLBACKS_H
#define ENGINECALLBACKS_H

// built-in memory manager
#define MALLOC( x )		(*g_engfuncs.pfnMemAlloc)( x, __FILE__, __LINE__ )
#define CALLOC( x, y )	(*g_engfuncs.pfnMemAlloc)((x) * (y), __FILE__, __LINE__ )
#define FREE( x )		(*g_engfuncs.pfnMemFree)( x, __FILE__, __LINE__ )

// screen handlers
#define LOAD_SHADER		(*g_engfuncs.pfnLoadShader)
#define DrawImage		(*g_engfuncs.pfnDrawImage)
#define SetColor		(*g_engfuncs.pfnSetColor)
#define SetParms		(*g_engfuncs.pfnSetParms)
#define CVAR_REGISTER	(*g_engfuncs.pfnRegisterVariable)
#define CVAR_SET_FLOAT	(*g_engfuncs.pfnCvarSetValue)
#define CVAR_GET_FLOAT	(*g_engfuncs.pfnGetCvarFloat)
#define CVAR_GET_STRING	(*g_engfuncs.pfnGetCvarString)

#define SERVER_COMMAND	(*g_engfuncs.pfnServerCmd)
#define CLIENT_COMMAND	(*g_engfuncs.pfnClientCmd)
#define GET_PLAYER_INFO	(*g_engfuncs.pfnGetPlayerInfo)
#define GET_GAME_MESSAGE	(*g_engfuncs.pfnTextMessageGet)
#define CMD_ARGC		(*g_engfuncs.pfnCmdArgc)
#define CMD_ARGV		(*g_engfuncs.pfnCmdArgv)
#define ALERT		(*g_engfuncs.pfnAlertMessage)

inline void CL_PlaySound( const char *szSound, float flVolume )
{
	g_engfuncs.pfnPlaySoundByName( szSound, flVolume, NULL );
}

inline void CL_PlaySound( int iSound, float flVolume )
{
	g_engfuncs.pfnPlaySoundByIndex( iSound, flVolume, NULL );
}

inline void CL_PlaySound( const char *szSound, float flVolume, Vector &pos )
{
	g_engfuncs.pfnPlaySoundByName( szSound, flVolume, pos );
}

inline void CL_PlaySound( int iSound, float flVolume, Vector &pos )
{
	g_engfuncs.pfnPlaySoundByIndex( iSound, flVolume, pos );
}

#define AngleVectors	(*g_engfuncs.pfnAngleVectors)
#define DrawCenterPrint	(*g_engfuncs.pfnDrawCenterPrint)
#define CenterPrint		(*g_engfuncs.pfnCenterPrint)
#define DrawChar		(*g_engfuncs.pfnDrawCharacter)
#define DrawString		(*g_engfuncs.pfnDrawString)
#define GetImageSize	(*g_engfuncs.pfnGetImageSize)
#define GetViewAngles	(*g_engfuncs.pfnGetViewAngles)
#define GetEntityByIndex	(*g_engfuncs.pfnGetEntityByIndex)
#define GetLocalPlayer	(*g_engfuncs.pfnGetLocalPlayer)
#define IsSpectateOnly	(*g_engfuncs.pfnIsSpectateOnly)
#define GetClientTime	(*g_engfuncs.pfnGetClientTime)
#define GetMaxClients	(*g_engfuncs.pfnGetMaxClients)
#define GetViewModel	(*g_engfuncs.pfnGetViewModel)
#define MAKE_LEVELSHOT	(*g_engfuncs.pfnMakeLevelShot)
#define POINT_CONTENTS	(*g_engfuncs.pfnPointContents)
#define TRACE_LINE		(*g_engfuncs.pfnTraceLine)
#define RANDOM_LONG		(*g_engfuncs.pfnRandomLong)
#define RANDOM_FLOAT	(*g_engfuncs.pfnRandomFloat)
#define LOAD_FILE		(*g_engfuncs.pfnLoadFile)
#define FREE_FILE		(*g_engfuncs.pfnFreeFile)
#define GET_GAME_DIR	(*g_engfuncs.pfnGetGameDir)
#define HOST_ERROR		(*g_engfuncs.pfnHostError)

// heavy legacy of Valve...
// tune char size by taste
inline void TextMessageDrawChar( int xpos, int ypos, int number, int r, int g, int b )
{
	SetColor((r / 255.0f), (g / 255.0f), (b / 255.0f), 1.0f );
	DrawChar( xpos, ypos, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, number );
}

inline void FillRGBA( int x, int y, int width, int height, int r, int g, int b, int a )
{
	Vector	RGB;

	RGB.x = (float)(r / 255.0f);
	RGB.y = (float)(g / 255.0f);
	RGB.z = (float)(b / 255.0f);

	g_engfuncs.pfnFillRGBA( x, y, width, height, RGB, (float)(a / 255.0f));
}

#endif//ENGINECALLBACKS_H