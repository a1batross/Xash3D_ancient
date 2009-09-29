//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     utils.cpp - client game utilities code
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "hud.h"

// NOTE: modify these functions with caution

typedef struct
{
	char	*name;
	byte	*buf;
	int	size;
	int	read;
	BOOL	badRead;
	char	string[2048];	// using for store strings
} user_message_t;

static user_message_t gMsg;

void BEGIN_READ( const char *pszName, int iSize, void *pBuf )
{
	memset( &gMsg, 0, sizeof( gMsg ));

	gMsg.size = iSize;
	gMsg.buf = (byte *)pBuf;
}

void END_READ( void )
{
	if( gMsg.badRead )
	{
		ALERT( at_console, "%s was received with errors\n", gMsg.name );
	}
}

int READ_CHAR( void )
{
	int     c;
	
	if( gMsg.read + 1 > gMsg.size )
	{
		gMsg.badRead = true;
		return -1;
	}
		
	c = (signed char)gMsg.buf[gMsg.read];
	gMsg.read++;
	
	return c;
}

int READ_BYTE( void )
{
	int     c;
	
	if( gMsg.read+1 > gMsg.size )
	{
		gMsg.badRead = true;
		return -1;
	}
		
	c = (unsigned char)gMsg.buf[gMsg.read];
	gMsg.read++;
	
	return c;
}

int READ_SHORT( void )
{
	int     c;
	
	if( gMsg.read + 2 > gMsg.size )
	{
		gMsg.badRead = true;
		return -1;
	}
		
	c = (short)( gMsg.buf[gMsg.read] + ( gMsg.buf[gMsg.read+1] << 8 ));
	gMsg.read += 2;
	
	return c;
}

int READ_WORD( void ) { return READ_SHORT(); }

int READ_LONG( void )
{
	int     c;
	
	if( gMsg.read + 4 > gMsg.size )
	{
		gMsg.badRead = true;
		return -1;
	}
 	c = gMsg.buf[gMsg.read]+(gMsg.buf[gMsg.read+1]<<8)+(gMsg.buf[gMsg.read+2]<<16)+(gMsg.buf[gMsg.read+3]<<24);
	gMsg.read += 4;
	
	return c;
}

float READ_FLOAT( void )
{
	union { float f; int l; } dat;
	
	dat.l = READ_LONG();
	return dat.f;   
}

char* READ_STRING( void )
{
	int	l, c;

	gMsg.string[0] = 0;

	l = 0;
	do
	{
		if( gMsg.read+1 > gMsg.size ) break; // no more characters

		c = READ_CHAR();
		if( c == -1 || c == '\0' )
			break;

		// translate all fmt spec to avoid crash bugs
		if( c == '%' ) c = '.';

		gMsg.string[l] = c;
		l++;
	} while( l < sizeof( gMsg.string ) - 1 );
	
	gMsg.string[l] = 0; // terminator

	return gMsg.string;
}

//
// Xash3D network specs. Don't modify!
//
float READ_COORD( void )
{
	return READ_FLOAT();
}

float READ_ANGLE( void )
{
	return (float)(READ_SHORT() * (360.0 / 65536));
}

Vector READ_DIR( void )
{
	Vector	dir;

	return dir.BitsToDir( READ_SHORT() ); 
}

//
// Sprites draw stuff
//
typedef struct
{
	// temp handle
	HSPRITE	hSprite;
	HSPRITE	hPause;		// pause pic

	// crosshair members
	HSPRITE	hCrosshair;
	wrect_t	rcCrosshair;
	byte	rgbCrosshair[3];
	bool	noCrosshair;
} draw_stuff_t;

static draw_stuff_t ds;
	
int SPR_Frames( HSPRITE hPic )
{
	int Frames;

	GetParms( NULL, NULL, &Frames, 0, hPic );

	return Frames;
}

int SPR_Height( HSPRITE hPic, int frame )
{
	int Height;

	GetParms( NULL, &Height, NULL, frame, hPic );

	return Height;
}

int SPR_Width( HSPRITE hPic, int frame )
{
	int Width;

	GetParms( &Width, NULL, NULL, frame, hPic );

	return Width;
}

void Draw_VidInit( void )
{
	memset( &ds, 0, sizeof( ds ));
}

/*
====================
SPRITE_GetList

====================
*/
void ParseHudSprite( const char **pfile, char *psz, client_sprite_t *result )
{
	int x = 0, y = 0, width = 0, height = 0;
	client_sprite_t p;
	int section = 0;
	char *token;
		
	memset( &p, 0, sizeof( client_sprite_t ));
          
	while(( token = COM_ParseToken( pfile )) != NULL )
	{
		if( !stricmp( token, psz ))
		{
			token = COM_ParseToken( pfile );
			if( !stricmp( token, "{" )) section = 1;
		}
		if( section ) // parse section
		{
			if( !stricmp( token, "}" )) break; // end section
			
			if( !stricmp( token, "file" )) 
			{                                          
				token = COM_ParseToken( pfile );
				strncpy( p.szSprite, token, 64 );

				// fill structure at default
				p.hSprite = SPR_Load( p.szSprite );
				width = SPR_Width( p.hSprite, 0 );
				height = SPR_Height( p.hSprite, 0 );
				x = y = 0;
			}
			else if ( !stricmp( token, "name" )) 
			{                                          
				token = COM_ParseToken( pfile );
				strncpy( p.szName, token, 64 );
			}
			else if ( !stricmp( token, "x" )) 
			{                                          
				token = COM_ParseToken( pfile );
				x = atoi( token );
			}
			else if ( !stricmp( token, "y" )) 
			{                                          
				token = COM_ParseToken( pfile );
				y = atoi( token );
			}
			else if ( !stricmp( token, "width" )) 
			{                                          
				token = COM_ParseToken( pfile );
				width = atoi( token );
			}
			else if ( !stricmp( token, "height" )) 
			{                                          
				token = COM_ParseToken( pfile );
				height = atoi( token );
			}
		}
	}

	if( !section ) return; // data not found

	// calculate sprite position
	p.rc.left = x;
	p.rc.right = x + width; 
	p.rc.top = y;
	p.rc.bottom = y + height;

	memcpy( result, &p, sizeof( client_sprite_t ));
}

client_sprite_t *SPR_GetList( const char *psz, int *piCount )
{
	char *pfile = (char *)LOAD_FILE( psz, NULL );
	int iSprCount = 0;

	if( !pfile )
	{
		*piCount = iSprCount;
		return NULL;
	}

	char *token;
	const char *plist = pfile;
	int depth = 0;

	while(( token = COM_ParseToken( &plist )) != NULL ) // calculate count of sprites
	{
		if( !stricmp( token, "{" )) depth++;
		else if( !stricmp( token, "}" )) depth--;
		else if( depth == 0 && !strcmp( token, "hudsprite" ))
			iSprCount++;
	}

	client_sprite_t *phud;
	plist = pfile;

	phud = new client_sprite_t[iSprCount];

	if( depth != 0 ) ALERT( at_console, "%s EOF without closing brace\n", psz );

	for( int i = 0; i < iSprCount; i++ ) //parse structures
	{
		ParseHudSprite( &plist, "hudsprite", &phud[i] );
	}

	if( !iSprCount ) ALERT( at_console, "SPR_GetList: %s doesn't have sprites\n", psz );
	FREE_FILE( pfile );
          
          *piCount = iSprCount;
	return phud;
}

void SPR_Set( HSPRITE hPic, int r, int g, int b )
{
	ds.hSprite = hPic;
	SetColor( r, g, b, 255 );
}

void SPR_Set( HSPRITE hPic, int r, int g, int b, int a )
{
	ds.hSprite = hPic;
	SetColor( r, g, b, a );
}

inline static void SPR_AdjustSize( float *x, float *y, float *w, float *h )
{
	if( !x && !y && !w && !h ) return;

	// scale for screen sizes
	float xscale = gHUD.m_scrinfo.iRealWidth / (float)gHUD.m_scrinfo.iWidth;
	float yscale = gHUD.m_scrinfo.iRealHeight / (float)gHUD.m_scrinfo.iHeight;

	if( x ) *x *= xscale;
	if( y ) *y *= yscale;
	if( w ) *w *= xscale;
	if( h ) *h *= yscale;
}

inline static void SPR_DrawChar( HSPRITE hFont, int xpos, int ypos, int width, int height, int ch )
{
	float	size, frow, fcol;
	float	ax, ay, aw, ah;
	int	fontWidth, fontHeight;

	ch &= 255;

	if( ch == ' ' ) return;
	if( ypos < -height ) return;

	ax = xpos;
	ay = ypos;
	aw = width;
	ah = height;

	SPR_AdjustSize( &ax, &ay, &aw, &ah ); 
	GetParms( &fontWidth, &fontHeight, NULL, 0, hFont );

	frow = (ch >> 4) * 0.0625f + (0.5f / (float)fontWidth);
	fcol = (ch & 15) * 0.0625f + (0.5f / (float)fontHeight);
	size = 0.0625f - (1.0f / (float)fontWidth);

	DrawImageExt( hFont, ax, ay, aw, ah, fcol, frow, fcol + size, frow + size );
}

inline static void SPR_DrawGeneric( int frame, float x, float y, float width, float height, const wrect_t *prc )
{
	float	s1, s2, t1, t2;

	if( width == -1 && height == -1 )
	{
		int w, h;
		GetParms( &w, &h, NULL, frame, ds.hSprite );
		width = w;
		height = h;
	}

	if( prc )
	{
		// calc rectangle
		s1 = (float)prc->left / width;
		t1 = (float)prc->top / height;
		s2 = (float)prc->right / width;
		t2 = (float)prc->bottom / height;
		width = prc->right - prc->left;
		height = prc->bottom - prc->top;
	}
	else
	{
		s1 = t1 = 0.0f;
		s2 = t2 = 1.0f;
	}

	// scale for screen sizes
	SPR_AdjustSize( &x, &y, &width, &height );
	DrawImageExt( ds.hSprite, x, y, width, height, s1, t1, s2, t2 );
} 

void TextMessageDrawChar( int xpos, int ypos, int number, int r, int g, int b )
{
	// tune char size by taste
	SetColor( r, g, b, 255 );
	SPR_DrawChar( gHUD.m_hHudFont, xpos, ypos, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, number );
}

void FillRGBA( float x, float y, float width, float height, int r, int g, int b, int a )
{
	SPR_AdjustSize( &x, &y, &width, &height );
	g_engfuncs.pfnFillRGBA( x, y, width, height, r, g, b, a );
}

void SPR_Draw( int frame, int x, int y, const wrect_t *prc )
{
	SetParms( ds.hSprite, kRenderNormal, frame );
	SPR_DrawGeneric( frame, x, y, -1, -1, prc );
}

void SPR_Draw( int frame, int x, int y, int width, int height )
{
	SetParms( ds.hSprite, kRenderNormal, frame );
	SPR_DrawGeneric( frame, x, y, width, height, NULL );
}

void SPR_DrawTransColor( int frame, int x, int y, const wrect_t *prc )
{
	SetParms( ds.hSprite, kRenderTransColor, frame );
	SPR_DrawGeneric( frame, x, y, -1, -1, prc );
}

void SPR_DrawTransColor( int frame, int x, int y, int width, int height )
{
	SetParms( ds.hSprite, kRenderTransColor, frame );
	SPR_DrawGeneric( frame, x, y, width, height, NULL );
}

void SPR_DrawHoles( int frame, int x, int y, const wrect_t *prc )
{
	SetParms( ds.hSprite, kRenderTransAlpha, frame );
	SPR_DrawGeneric( frame, x, y, -1, -1, prc );
}

void SPR_DrawHoles( int frame, int x, int y, int width, int height )
{
	SetParms( ds.hSprite, kRenderTransAlpha, frame );
	SPR_DrawGeneric( frame, x, y, width, height, NULL );
}

void SPR_DrawAdditive( int frame, int x, int y, const wrect_t *prc )
{
	SetParms( ds.hSprite, kRenderTransAdd, frame );
	SPR_DrawGeneric( frame, x, y, -1, -1, prc );
}

void SPR_DrawAdditive( int frame, int x, int y, int width, int height )
{
	SetParms( ds.hSprite, kRenderTransAdd, frame );
	SPR_DrawGeneric( frame, x, y, width, height, NULL );
}

void SetCrosshair( HSPRITE hspr, wrect_t rc, int r, int g, int b )
{
	ds.rgbCrosshair[0] = (byte)r;
	ds.rgbCrosshair[1] = (byte)g;
	ds.rgbCrosshair[2] = (byte)b;
	ds.hCrosshair = hspr;
	ds.rcCrosshair = rc;
}

void HideCrosshair( bool hide )
{
	ds.noCrosshair = hide;
}

void DrawCrosshair( void )
{
	if( ds.hCrosshair == 0 || ds.noCrosshair || !CVAR_GET_FLOAT( "cl_crosshair" ))
		return;

	int x = (ScreenWidth - (ds.rcCrosshair.right - ds.rcCrosshair.left)) / 2; 
	int y = (ScreenHeight - (ds.rcCrosshair.bottom - ds.rcCrosshair.top)) / 2;

	// FIXME: apply crosshair angles properly
	x += gHUD.m_CrosshairAngles.x;
	y += gHUD.m_CrosshairAngles.y;

	ds.hSprite = ds.hCrosshair;
	SetParms( ds.hCrosshair, kRenderTransAlpha, 0 );
	SetColor( ds.rgbCrosshair[0], ds.rgbCrosshair[1], ds.rgbCrosshair[2], 255 );
	SPR_DrawGeneric( 0, x, y, -1, -1, &ds.rcCrosshair );
}

void DrawPause( void )
{
	// pause image
	if( !CVAR_GET_FLOAT( "paused" ) || !CVAR_GET_FLOAT( "scr_showpause" ))
		return;

	DrawImageBar( 100, "m_pause" ); // HACKHACK
}

void DrawImageRectangle( HSPRITE hImage )
{
	DrawImageExt( hImage, 0, 0, ActualWidth, ActualHeight, 0, 0, 1, 1 );
}

void DrawImageBar( float percent, const char *szSpriteName )
{
	int m_loading = gHUD.GetSpriteIndex( szSpriteName );
	wrect_t rcSize = gHUD.GetSpriteRect( m_loading );

	int w = rcSize.right - rcSize.left;
	int h = rcSize.bottom - rcSize.top;
	DrawImageBar( percent, szSpriteName, (ScreenWidth - w)/2, (ScreenHeight - h)/2 );
}

void DrawImageBar( float percent, const char *szSpriteName, int x, int y )
{
	int	m_loading = gHUD.GetSpriteIndex( szSpriteName );
	HSPRITE	hLoading = gHUD.GetSprite( m_loading );
	wrect_t	rcBar, rcBack;
	float	step;

	rcBar = rcBack = gHUD.GetSpriteRect( m_loading );
	step = (float)(rcBack.right - rcBack.left) / 100;
	rcBar.right = rcBar.left + (int)ceil(percent * step);

	SPR_Set( hLoading, 128, 128, 128 );
	SPR_DrawAdditive( 0, x, y, &rcBack );	// background

	SPR_Set( hLoading, 255, 160, 0 );
	SPR_DrawAdditive( 0, x, y, &rcBar );	// progress bar
}

//
// 27/12/08 moved here from cl_view.c
//
void V_RenderPlaque( void )
{
	if( gHUD.m_iDrawPlaque )
	{
		const char *levelshot;

		// logo that shows up while upload next level
		levelshot = CVAR_GET_STRING( "cl_levelshot_name" );
		DrawImageRectangle( SPR_Load( levelshot ));
		DrawImageBar( CVAR_GET_FLOAT( "scr_loading" ), "m_loading" );
	}

	if( !CVAR_GET_FLOAT( "scr_download" )) return;
	DrawImageBar( CVAR_GET_FLOAT( "scr_download" ), "m_download", (ScreenWidth-128)/2, ScreenHeight-60 );
}

//
// hl2 fade - this supports multiple fading
// FIXME: make Class CHudFade instead of C-style code?
//
void SetScreenFade( Vector fadeColor, float alpha, float duration, float holdTime, int fadeFlags )
{
	ScreenFade *sf = NULL;

	for( int i = 0; i < HUD_MAX_FADES; i++ )
	{
		// search for free spot
		if( gHUD.m_FadeList[i].fadeFlags == 0 )
		{
			sf = &gHUD.m_FadeList[i];
			break;
		}
	}

	if( !sf ) return; // no free fades found 

	sf->fadeEnd = duration;
	sf->fadeReset = holdTime;	
	sf->fadeColor = fadeColor;
	sf->fadeAlpha = alpha;
	sf->fadeFlags = fadeFlags;
	sf->fadeSpeed = 0;

	// calc fade speed
	if( duration > 0 )
	{
		if( fadeFlags & FFADE_OUT )
		{
			if( sf->fadeEnd )
			{
				sf->fadeSpeed = -(float)sf->fadeAlpha / sf->fadeEnd;
			}

			sf->fadeEnd += gHUD.m_flTime;
			sf->fadeReset += sf->fadeEnd;
		}
		else
		{
			if( sf->fadeEnd )
			{
				sf->fadeSpeed = (float)sf->fadeAlpha / sf->fadeEnd;
			}

			sf->fadeReset += gHUD.m_flTime;
			sf->fadeEnd += sf->fadeReset;
		}
	}

	if( fadeFlags & FFADE_PURGE )
	{
		ClearAllFades();
	}
}

void DrawScreenFade( void )
{
	int	i, numFades = 0;
	
	// Cycle through all fades and remove any that have finished (work backwards)
	for( i = HUD_MAX_FADES - 1; i >= 0; i-- )
	{
		ScreenFade *pFade = &gHUD.m_FadeList[i];

		if( pFade->fadeFlags == 0 ) continue;	// free slot

		// Keep pushing reset time out indefinitely
		if( pFade->fadeFlags & FFADE_STAYOUT )
		{
			pFade->fadeReset = gHUD.m_flTime + 0.1f;
		}

		// All done?
		if(( gHUD.m_flTime > pFade->fadeReset ) && ( gHUD.m_flTime > pFade->fadeEnd ))
		{
			// remove this Fade from the list
			memset( pFade, 0, sizeof( ScreenFade ));
		}
	}

	gHUD.m_bModulate = false;
	gHUD.m_vFadeColor = Vector( 0, 0, 0 );
	gHUD.m_fFadeAlpha = 0.0f;

	// Cycle through all fades in the list and calculate the overall color/alpha
	for ( i = 0; i < HUD_MAX_FADES; i++ )
	{
		ScreenFade *pFade = &gHUD.m_FadeList[i];

		if( pFade->fadeFlags == 0 ) continue;	// free slot

		// Color
		gHUD.m_vFadeColor += pFade->fadeColor;

		// Fading...
		int iFadeAlpha;
		if( pFade->fadeFlags & ( FFADE_OUT|FFADE_IN ))
		{
			iFadeAlpha = pFade->fadeSpeed * ( pFade->fadeEnd - gHUD.m_flTime );
			if( pFade->fadeFlags & FFADE_OUT )
			{
				iFadeAlpha += pFade->fadeAlpha;
			}
			iFadeAlpha = min( iFadeAlpha, pFade->fadeAlpha );
			iFadeAlpha = max( 0, iFadeAlpha );
		}
		else
		{
			iFadeAlpha = pFade->fadeAlpha;
		}

		// Use highest alpha
		if( iFadeAlpha > gHUD.m_fFadeAlpha )
		{
			gHUD.m_fFadeAlpha = iFadeAlpha;
		}

		// Modulate?
		if( pFade->fadeFlags & FFADE_MODULATE )
		{
			gHUD.m_bModulate = true;
		}
		numFades++;
	}

	// Divide colors
	if( numFades ) gHUD.m_vFadeColor /= numFades;

	if( gHUD.m_vFadeColor == Vector( 0, 0, 0 )) return;
	const float *RGB = gHUD.m_vFadeColor;

	FillRGBA( 0, 0, ScreenWidth, ScreenHeight, RGB[0], RGB[1], RGB[2], gHUD.m_fFadeAlpha );
}

void ClearPermanentFades( void )
{
	for( int i = HUD_MAX_FADES - 1; i >= 0; i-- )
	{
		ScreenFade *pFade = &gHUD.m_FadeList[i];
		if( pFade->fadeFlags == 0 ) continue;	// free slot

		if( pFade->fadeFlags & FFADE_STAYOUT )
		{
			// remove this Fade from the list
			ALERT( at_console, "remove fade %i\n", i );
			memset( pFade, 0, sizeof( ScreenFade ));
		}
	}
}

void ClearAllFades( void )
{
	memset( gHUD.m_FadeList, 0, sizeof( gHUD.m_FadeList ));
}

/*
====================
Sys LoadGameDLL

====================
*/
BOOL Sys_LoadLibrary( const char* dllname, dllhandle_t* handle, const dllfunction_t *fcts )
{
	const dllfunction_t	*gamefunc;
	dllhandle_t dllhandle = 0;

	if( handle == NULL ) return false;

	// Initializations
	for( gamefunc = fcts; gamefunc && gamefunc->name != NULL; gamefunc++ )
		*gamefunc->funcvariable = NULL;

	dllhandle = (dllhandle_t)LOAD_LIBRARY( dllname );
        
	// No DLL found
	if( !dllhandle ) return false;

	// Get the function adresses
	for( gamefunc = fcts; gamefunc && gamefunc->name != NULL; gamefunc++ )
	{
		if(!( *gamefunc->funcvariable = (void *) Sys_GetProcAddress( dllhandle, gamefunc->name )))
		{
			Sys_UnloadLibrary( &dllhandle );
			return false;
		}
	}          

	ALERT( at_loading, "%s loaded succesfully!\n", dllname );
	*handle = dllhandle;
	return true;
}

void Sys_UnloadLibrary( dllhandle_t *handle )
{
	if( handle == NULL || *handle == NULL )
		return;

	FREE_LIBRARY( *handle );
	*handle = NULL;
}

void* Sys_GetProcAddress( dllhandle_t handle, const char* name )
{
	return (void *)GET_PROC_ADDRESS( handle, name );
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
char *va( const char *format, ... )
{
	va_list argptr;
	static char string[32][1024], *s;
	static int stringindex = 0;

	s = string[stringindex];
	stringindex = (stringindex + 1) & 31;
	va_start( argptr, format );
	_vsnprintf( s, sizeof( string[0] ), format, argptr );
	va_end( argptr );
	return s;
}

/*
==============
COM_ParseToken

Parse a token out of a string
==============
*/
char *COM_ParseToken( const char **data_p )
{
	int		c;
	int		len = 0;
	const char	*data;
	static char	token[512];
	
	token[0] = 0;
	data = *data_p;
	
	if( !data ) 
	{
		*data_p = NULL;
		return NULL;
	}		

	// skip whitespace
skipwhite:
	while(( c = *data) <= ' ' )
	{
		if( c == 0 )
		{
			*data_p = NULL;
			return NULL; // end of file;
		}
		data++;
	}
	
	// skip // comments
	if( c=='/' && data[1] == '/' )
	{
		while( *data && *data != '\n' )
			data++;
		goto skipwhite;
	}

	// skip /* comments
	if( c=='/' && data[1] == '*' )
	{
		while( data[1] && (data[0] != '*' || data[1] != '/' ))
			data++;
		data += 2;
		goto skipwhite;
	}
	

	// handle quoted strings specially
	if( *data == '\"' || *data == '\'' )
	{
		data++;
		while( 1 )
		{
			c = *data++;
			if( c=='\"' || c=='\0' )
			{
				token[len] = 0;
				*data_p = data;
				return token;
			}
			token[len] = c;
			len++;
		}
	}

	// parse single characters
	if( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':' || c == ',' )
	{
		token[len] = c;
		data++;
		len++;
		token[len] = 0;
		*data_p = data;
		return token;
	}

	// parse a regular word
	do
	{
		token[len] = c;
		data++;
		len++;
		c = *data;
		if( c == '{' || c == '}'|| c == ')'|| c == '(' || c == '\'' || c == ':' || c == ',' )
			break;
	} while( c > 32 );
	
	token[len] = 0;
	*data_p = data;
	return token;
}