//=======================================================================
//			Copyright XashXT Group 2008 ©
//			  utils.h - client utilities
//=======================================================================

#ifndef UTILS_H
#define UTILS_H

extern cl_enginefuncs_t g_engfuncs;

#include "event_api.h"
#include "enginecallback.h"

#define FILE_GLOBAL	static
#define DLL_GLOBAL

//
// How did I ever live without ASSERT?
//
#ifdef _DEBUG
void DBG_AssertFunction( BOOL fExpr, const char* szExpr, const char* szFile, int szLine, const char* szMessage );
#define ASSERT( f )		DBG_AssertFunction( f, #f, __FILE__, __LINE__, NULL )
#define ASSERTSZ( f, sz )	DBG_AssertFunction( f, #f, __FILE__, __LINE__, sz )
#else
#define ASSERT( f )
#define ASSERTSZ( f, sz )
#endif

extern DLL_GLOBAL const Vector	g_vecZero;
extern cl_globalvars_t		*gpGlobals;
extern movevars_t			*gpMovevars;

extern int HUD_VidInit( void );
extern void HUD_Init( void );
extern int HUD_Redraw( float flTime, int state );
extern void HUD_UpdateEntityVars( edict_t *out, const entity_state_t *s, const entity_state_t *p );
extern void HUD_UpdateOnRemove( edict_t *pEdict );
extern void HUD_Reset( void );
extern void HUD_StartFrame( void );
extern void HUD_Frame( double time );
extern void HUD_Shutdown( void );
extern void HUD_RenderCallback( int fTrans );
extern void HUD_CreateEntities( void );
extern int  HUD_AddVisibleEntity( edict_t *pEnt, int ed_type );
extern void HUD_StudioEvent( const dstudioevent_t *event, edict_t *entity );
extern void HUD_StudioFxTransform( edict_t *ent, float transform[4][4] );
extern void HUD_ParseTempEntity( void );
extern void V_CalcRefdef( ref_params_t *parms );
extern void V_StartPitchDrift( void );
extern void V_StopPitchDrift( void );
extern void V_Init( void );
extern void VGui_ConsolePrint( const char *text );
extern void IN_Init( void );
extern void IN_Shutdown( void );
extern void IN_CreateMove( usercmd_t *cmd, int frametime, int active );
extern int  IN_KeyEvent( int down, int keynum, const char *pszBind );
extern void IN_MouseEvent( int mx, int my );

#define VIEWPORT_SIZE	512

typedef void* dllhandle_t;
typedef struct dllfunction_s
{
	const char *name;
	void **funcvariable;
} dllfunction_t;

#include "cvardef.h"

// macros to hook function calls into the HUD object
#define HOOK_MESSAGE( x ) (*g_engfuncs.pfnHookUserMsg)( #x, __MsgFunc_##x );

#define DECLARE_MESSAGE( y, x ) int __MsgFunc_##x(const char *pszName, int iSize, void *pbuf) \
{ \
	return gHUD.##y.MsgFunc_##x(pszName, iSize, pbuf ); \
}

#define DECLARE_HUDMESSAGE( x ) int __MsgFunc_##x( const char *pszName, int iSize, void *pbuf ) \
{ \
	return gHUD.MsgFunc_##x(pszName, iSize, pbuf ); \
}

#define HOOK_COMMAND( x, y ) (*g_engfuncs.pfnAddCommand)( x, __CmdFunc_##y, "user-defined command" );
#define DECLARE_HUDCOMMAND( x ) void __CmdFunc_##x( void ) \
{ \
	gHUD.UserCmd_##x( ); \
}
#define DECLARE_COMMAND( y, x ) void __CmdFunc_##x( void ) \
{ \
	gHUD.##y.UserCmd_##x( ); \
}

// Use this to set any co-ords in 640x480 space
#define XRES(x)		((int)(float(x)  * ((float)ScreenWidth / 640.0f) + 0.5f))
#define YRES(y)		((int)(float(y)  * ((float)ScreenHeight / 480.0f) + 0.5f))

// ScreenHeight returns the virtual height of the screen, in pixels
#define ScreenHeight	(gHUD.m_scrinfo.iHeight)
// ScreenWidth returns the virtual width of the screen, in pixels
#define ScreenWidth		(gHUD.m_scrinfo.iWidth)

// ScreenHeight returns the height of the screen, in pixels
#define ActualHeight	(gHUD.m_scrinfo.iRealHeight)
// ScreenWidth returns the width of the screen, in pixels
#define ActualWidth		(gHUD.m_scrinfo.iRealWidth)

inline void UnpackRGB( int &r, int &g, int &b, dword ulRGB )
{
	r = (ulRGB & 0xFF0000) >>16;\
	g = (ulRGB & 0xFF00) >> 8;\
	b = ulRGB & 0xFF;\
}

inline void ScaleColors( int &r, int &g, int &b, int a )
{
	float x = (float)a / 255;
	r = (int)(r * x);
	g = (int)(g * x);
	b = (int)(b * x);
}

inline float LerpAngle( float a2, float a1, float frac )
{
	if( a1 - a2 > 180 ) a1 -= 360;
	if( a1 - a2 < -180 ) a1 += 360;
	return a2 + frac * (a1 - a2);
}

inline float LerpView( float org1, float org2, float ofs1, float ofs2, float frac )
{
	return org1 + ofs1 + frac * (org2 + ofs2 - (org1 + ofs1));
}

inline float LerpPoint( float oldpoint, float curpoint, float frac )
{
	return oldpoint + frac * (curpoint - oldpoint);
}

inline Vector LerpPoint( Vector oldpoint, Vector curpoint, float frac )
{
	return oldpoint + frac * (curpoint - oldpoint);
}

inline byte LerpByte( byte oldpoint, byte curpoint, float frac )
{
	return bound( 0, oldpoint + frac * (curpoint - oldpoint), 255 );
}

_inline float anglemod( const float a )
{
	return( 360.0f / 65536) * ((int)(a * (65536 / 360.0f)) & 65535);
}

inline int ConsoleStringLen( const char *string )
{
	int	_width, _height;
	GetConsoleStringSize( string, &_width, &_height );
	return _width;
}

extern Vector BitsToDir( int bits );

// message reading
extern void BEGIN_READ( const char *pszName, int iSize, void *pBuf );
extern int READ_CHAR( void );
extern int READ_BYTE( void );
extern int READ_SHORT( void );
extern int READ_WORD( void );
extern int READ_LONG( void );
extern float READ_FLOAT( void );
extern char* READ_STRING( void );
extern float READ_COORD( void );
extern float READ_ANGLE( void );
extern Vector READ_DIR( void );
extern void END_READ( void );

// engine traces
extern void UTIL_TraceLine( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, edict_t *pentIgnore, TraceResult *ptr);
extern void UTIL_TraceLine( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, IGNORE_GLASS ignoreGlass, edict_t *pentIgnore, TraceResult *ptr);
extern void UTIL_TraceHull( const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, int hullNumber, edict_t *pentIgnore, TraceResult *ptr);

// misc utilities
extern float UTIL_Probe( const Vector &origin, Vector *vecDirection, float strength );
extern void UTIL_GetForceDirection( const Vector &origin, float magnitude, Vector *resultDirection, float *resultForce );
extern void RotatePointAroundVector( Vector &dst, const Vector &dir, const Vector &point, float degrees );

// drawing stuff
extern client_sprite_t *SPR_GetList( const char *name, int *count );
extern void ParseHudSprite( const char **pfile, char *psz, client_sprite_t *result );
extern void DrawPause( void );
extern void SetScreenFade( Vector fadeColor, float alpha, float duration, float holdTime, int fadeFlags );
extern void ClearAllFades( void );
extern void ClearPermanentFades( void );
extern void DrawScreenFade( void );
extern void DrawImageBar( float percent, const char *szSpriteName );
extern void DrawImageBar( float percent, const char *szSpriteName, int x, int y );
extern void DrawGenericBar( float percent, int w, int h );
extern void DrawGenericBar( float percent, int x, int y, int w, int h );
extern void Draw_VidInit( void );

// mathlib
extern void AngleMatrix( const vec3_t angles, float (*matrix)[4] );

// from cl_view.c
extern void DrawProgressBar( void );
extern edict_t *spot;
extern float v_idlescale;
extern int g_weaponselect;
extern int g_iAlive;		// indicates alive local client or not

// input.cpp
extern cvar_t	*v_centerspeed;
extern cvar_t	*v_centermove;
extern cvar_t	*cl_forwardspeed;
extern cvar_t	*cl_particlelod;
extern cvar_t	*cl_particles;
extern cvar_t	*cl_draw_beams;
extern cvar_t	*cl_lw;

extern int CL_ButtonBits( int bResetState );
extern void CL_ResetButtonBits( int bits );

// dlls stuff
BOOL Sys_LoadLibrary( const char* dllname, dllhandle_t* handle, const dllfunction_t *fcts );
void* Sys_GetProcAddress( dllhandle_t handle, const char* name );
void Sys_UnloadLibrary( dllhandle_t* handle );

// misc stuff
const char *UTIL_FileExtension( const char *in );

#endif//UTILS_H