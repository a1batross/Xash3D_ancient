//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        hud_iface.h - client exported funcs
//=======================================================================

#ifndef HUD_IFACE_H
#define HUD_IFACE_H

extern cl_enginefuncs_t g_engfuncs;

#include "enginecallback.h"

extern int HUD_VidInit( void );
extern void HUD_Init( void );
extern int HUD_Redraw( float flTime, int state );
extern int HUD_UpdateClientData( client_data_t *cdata, float flTime );
extern void HUD_Reset( void );
extern void HUD_Frame( double time );
extern void HUD_Shutdown( void );
extern void HUD_DrawNormalTriangles( void );
extern void HUD_DrawTransparentTriangles( void );
extern void HUD_CreateEntities( void );
extern void HUD_StudioEvent( const dstudioevent_t *event, edict_t *entity );
extern void V_CalcRefdef( ref_params_t *parms );

typedef struct rect_s
{
	int	left;
	int	right;
	int	top;
	int	bottom;
} wrect_t;

typedef struct client_sprite_s
{
	char	szName[64]; // shader name and sprite name are matched
	char	szSprite[64];
	HSPRITE	hSprite;
	wrect_t	rc;
} client_sprite_t;

typedef HMODULE dllhandle_t;
typedef struct dllfunction_s
{
	const char *name;
	void **funcvariable;
} dllfunction_t;

// cvar flags
#define CVAR_ARCHIVE	BIT(0)	// set to cause it to be saved to vars.rc
#define CVAR_USERINFO	BIT(1)	// added to userinfo  when changed
#define CVAR_SERVERINFO	BIT(2)	// added to serverinfo when changed
#define CVAR_LATCH		BIT(5)

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
#define DECLARE_COMMAND( y, x ) void __CmdFunc_##x( void ) \
{ \
	gHUD.##y.UserCmd_##x( ); \
}

#define bound( min, num, max )	((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))

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

inline int ConsoleStringLen( const char *string )
{
	// console using fixed font size
	return strlen( string ) * SMALLCHAR_WIDTH;
}

inline void GetConsoleStringSize( const char *string, int *width, int *height )
{
	// console using fixed font size
	if( width ) *width = ConsoleStringLen( string );
	if( height ) *height = SMALLCHAR_HEIGHT;
}

// simple huh ?
inline int DrawConsoleString( int x, int y, const char *string )
{
	DrawString( x, y, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, string );

	return ConsoleStringLen( string );
}

inline void ConsolePrint( const char *string )
{
	ALERT( at_console, "%s", string );
}

// returns the players name of entity no.
inline void GetPlayerInfo( int ent_num, hud_player_info_t *pinfo )
{
	GET_PLAYER_INFO( ent_num, pinfo );
}

inline client_textmessage_t *TextMessageGet( const char *pName )
{
	return GET_GAME_MESSAGE( pName );
}

// message reading
extern void BEGIN_READ( const char *pszName, int iSize, void *pBuf );
extern int READ_CHAR( void );
extern int READ_BYTE( void );
extern int READ_SHORT( void );
extern int READ_WORD( void );
extern int READ_LONG( void );
extern int64 READ_LONG64( void );
extern float READ_FLOAT( void );
extern double READ_DOUBLE( void );
extern char* READ_STRING( void );
extern float READ_COORD( void );
extern float READ_ANGLE( void );
extern void END_READ( void );

// drawing stuff
#define SPR_Load( x ) LOAD_SHADER( x )
extern int SPR_Frames( HSPRITE hPic );
extern int SPR_Height( HSPRITE hPic, int frame );
extern int SPR_Width( HSPRITE hPic, int frame );
extern client_sprite_t *SPR_GetList( const char *name, int *count );
extern void ParseHudSprite( const char **pfile, char *psz, client_sprite_t *result );
extern void SPR_Set( HSPRITE hPic, int r, int g, int b );
extern void SPR_Set( HSPRITE hPic, int r, int g, int b, int a );
extern void SPR_Draw( int frame, int x, int y, const wrect_t *prc );
extern void SPR_Draw( int frame, int x, int y, int width, int height );
extern void SPR_DrawHoles( int frame, int x, int y, const wrect_t *prc );
extern void SPR_DrawHoles( int frame, int x, int y, int width, int height );
extern void SPR_DrawTransColor( int frame, int x, int y, int width, int height );
extern void SPR_DrawTransColor( int frame, int x, int y, const wrect_t *prc );
extern void SPR_DrawAdditive( int frame, int x, int y, const wrect_t *prc );
extern void SPR_DrawAdditive( int frame, int x, int y, int width, int height );
extern void TextMessageDrawChar( int xpos, int ypos, int number, int r, int g, int b );
extern void FillRGBA( float x, float y, float width, float height, int r, int g, int b, int a );
extern void SetCrosshair( HSPRITE hspr, wrect_t rc, int r, int g, int b );
extern void DrawCrosshair( void );
extern void DrawPause( void );
extern void SetScreenFade( Vector fadeColor, float alpha, float duration, float holdTime, int fadeFlags );
extern void DrawScreenFade( void );
extern void DrawImageBar( float percent, const char *szSpriteName );
extern void DrawImageBar( float percent, const char *szSpriteName, int x, int y );
extern void DrawGenericBar( float percent, int w, int h );
extern void DrawGenericBar( float percent, int x, int y, int w, int h );
extern void Draw_VidInit( void );

// from cl_view.c
extern void V_RenderSplash( void );
extern void V_RenderPlaque( void );

// stdio stuff
extern char *va( const char *format, ... );
char *COM_ParseToken( const char **data_p );

// dlls stuff
BOOL Sys_LoadLibrary( const char* dllname, dllhandle_t* handle, const dllfunction_t *fcts );
void* Sys_GetProcAddress( dllhandle_t handle, const char* name );
void Sys_UnloadLibrary( dllhandle_t* handle );

#endif//HUD_IFACE_H