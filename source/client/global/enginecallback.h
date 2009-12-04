//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     enginecallback.h - actual engine callbacks
//=======================================================================

#ifndef ENGINECALLBACKS_H
#define ENGINECALLBACKS_H

// built-in memory manager
#define MALLOC( x )		(*g_engfuncs.pfnMemAlloc)( x, __FILE__, __LINE__ )
#define CALLOC( x, y )	(*g_engfuncs.pfnMemAlloc)((x) * (y), __FILE__, __LINE__ )
#define MEMCPY( x, y, z )	(*g_engfuncs.pfnMemCopy)( x, y, z, __FILE__, __LINE__ )
#define FREE( x )		(*g_engfuncs.pfnMemFree)( x, __FILE__, __LINE__ )

// screen handlers
#define SPR_Frames		(*g_engfuncs.pfnSPR_Frames)
#define SPR_Width		(*g_engfuncs.pfnSPR_Width)
#define SPR_Height		(*g_engfuncs.pfnSPR_Height)
#define SPR_EnableScissor	(*g_engfuncs.pfnSPR_EnableScissor)
#define SPR_DisableScissor	(*g_engfuncs.pfnSPR_DisableScissor)
#define FillRGBA		(*g_engfuncs.pfnFillRGBA)
#define GetScreenInfo	(*g_engfuncs.pfnGetScreenInfo)

#define SPR_Load		(*g_engfuncs.pfnSPR_Load)
#define TEX_Load( x )	(*g_engfuncs.pfnLoadShader)( x, false )
#define DrawImageExt	(*g_engfuncs.pfnDrawImageExt)
#define SetColor		(*g_engfuncs.pfnSetColor)
#define SetParms		(*g_engfuncs.pfnSetParms)
#define SetCrosshair	(*g_engfuncs.pfnSetCrosshair)

#define CVAR_REGISTER	(*g_engfuncs.pfnRegisterVariable)
#define CVAR_SET_FLOAT	(*g_engfuncs.pfnCvarSetValue)
#define CVAR_GET_FLOAT	(*g_engfuncs.pfnGetCvarFloat)
#define CVAR_GET_STRING	(*g_engfuncs.pfnGetCvarString)
#define SET_KEYDEST		(*g_engfuncs.pfnSetKeyDest)
#define SERVER_COMMAND	(*g_engfuncs.pfnServerCmd)
#define CLIENT_COMMAND	(*g_engfuncs.pfnClientCmd)
#define GET_PLAYER_INFO	(*g_engfuncs.pfnGetPlayerInfo)
#define GET_GAME_MESSAGE	(*g_engfuncs.pfnTextMessageGet)
#define CMD_ARGC		(*g_engfuncs.pfnCmdArgc)
#define CMD_ARGV		(*g_engfuncs.pfnCmdArgv)
#define ALERT		(*g_engfuncs.pfnAlertMessage)

inline void SPR_Set( HSPRITE hPic, int r, int g, int b )
{
	g_engfuncs.pfnSPR_Set( hPic, r, g, b, 255 );
}

inline void SPR_Set( HSPRITE hPic, int r, int g, int b, int a )
{
	g_engfuncs.pfnSPR_Set( hPic, r, g, b, a );
}

inline void SPR_Draw( int frame, int x, int y, const wrect_t *prc )
{
	g_engfuncs.pfnSPR_Draw( frame, x, y, -1, -1, prc );
}

inline void SPR_Draw( int frame, int x, int y, int width, int height )
{
	g_engfuncs.pfnSPR_Draw( frame, x, y, width, height, NULL );
}

inline void SPR_DrawTransColor( int frame, int x, int y, const wrect_t *prc )
{
	g_engfuncs.pfnSPR_DrawTrans( frame, x, y, -1, -1, prc );
}

inline void SPR_DrawTransColor( int frame, int x, int y, int width, int height )
{
	g_engfuncs.pfnSPR_DrawTrans( frame, x, y, width, height, NULL );
}

inline void SPR_DrawHoles( int frame, int x, int y, const wrect_t *prc )
{
	g_engfuncs.pfnSPR_DrawHoles( frame, x, y, -1, -1, prc );
}

inline void SPR_DrawHoles( int frame, int x, int y, int width, int height )
{
	g_engfuncs.pfnSPR_DrawHoles( frame, x, y, width, height, NULL );
}

inline void SPR_DrawAdditive( int frame, int x, int y, const wrect_t *prc )
{
	g_engfuncs.pfnSPR_DrawAdditive( frame, x, y, -1, -1, prc );
}

inline void SPR_DrawAdditive( int frame, int x, int y, int width, int height )
{
	g_engfuncs.pfnSPR_DrawAdditive( frame, x, y, width, height, NULL );
}

inline void CL_PlaySound( const char *szSound, float flVolume, float pitch = PITCH_NORM )
{
	g_engfuncs.pfnPlaySoundByName( szSound, flVolume, pitch, NULL );
}

inline void CL_PlaySound( int iSound, float flVolume, float pitch = PITCH_NORM )
{
	g_engfuncs.pfnPlaySoundByIndex( iSound, flVolume, pitch, NULL );
}

inline void CL_PlaySound( const char *szSound, float flVolume, Vector &pos, float pitch = PITCH_NORM )
{
	g_engfuncs.pfnPlaySoundByName( szSound, flVolume, pitch, pos );
}

inline void CL_PlaySound( int iSound, float flVolume, Vector &pos, float pitch = PITCH_NORM )
{
	g_engfuncs.pfnPlaySoundByIndex( iSound, flVolume, pitch, pos );
}

#define AngleVectors	(*g_engfuncs.pfnAngleVectors)
#define DrawCenterPrint	(*g_engfuncs.pfnDrawCenterPrint)
#define CenterPrint		(*g_engfuncs.pfnCenterPrint)
#define DrawString		(*g_engfuncs.pfnDrawString)
#define GetParms		(*g_engfuncs.pfnGetParms)
#define GetViewAngles	(*g_engfuncs.pfnGetViewAngles)
#define GetEntityByIndex	(*g_engfuncs.pfnGetEntityByIndex)
#define GetLocalPlayer	(*g_engfuncs.pfnGetLocalPlayer)
#define IsSpectateOnly	(*g_engfuncs.pfnIsSpectateOnly)
#define GetClientTime	(*g_engfuncs.pfnGetClientTime)
#define GetLerpFrac		(*g_engfuncs.pfnGetLerpFrac)
#define GetMaxClients	(*g_engfuncs.pfnGetMaxClients)
#define GetViewModel	(*g_engfuncs.pfnGetViewModel)
#define GetModelPtr		(*g_engfuncs.pfnGetModelPtr)
#define GET_ATTACHMENT	(*g_engfuncs.pfnGetAttachment)
#define POINT_CONTENTS	(*g_engfuncs.pfnPointContents)
#define TRACE_LINE		(*g_engfuncs.pfnTraceLine)
#define TRACE_HULL		(*g_engfuncs.pfnTraceHull)
#define ALLOC_STRING	(*g_engfuncs.pfnAllocString)
#define STRING		(*g_engfuncs.pfnGetString)
#define RANDOM_LONG		(*g_engfuncs.pfnRandomLong)
#define RANDOM_FLOAT	(*g_engfuncs.pfnRandomFloat)
#define LOAD_FILE		(*g_engfuncs.pfnLoadFile)
#define FILE_EXISTS		(*g_engfuncs.pfnFileExists)
#define FREE_FILE		FREE
#define DELETE_FILE		(*g_engfuncs.pfnRemoveFile)
#define LOAD_LIBRARY	(*g_engfuncs.pfnLoadLibrary)
#define GET_PROC_ADDRESS	(*g_engfuncs.pfnGetProcAddress)
#define FREE_LIBRARY	(*g_engfuncs.pfnFreeLibrary)
#define HOST_ERROR		(*g_engfuncs.pfnHostError)

#endif//ENGINECALLBACKS_H