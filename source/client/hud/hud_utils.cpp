//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     hud_utils.cpp - client game utilities code
//=======================================================================

#include "extdll.h"
#include "hud_iface.h"
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
	return READ_FLOAT();
}

float READ_ANGLE16( void )
{
	return (float)(READ_SHORT() * (360.0 / 65536));
}

//
// Sprites draw stuff
//
typedef struct
{
	// temp handle
	HSPRITE	hSprite;

	// crosshair members
	HSPRITE	hCrosshair;
	wrect_t	rcCrosshair;
	Vector	rgbCrosshair;
} draw_stuff_t;

typedef struct
{
	float	fadeSpeed;	// How fast to fade (tics / second) (+ fade in, - fade out)
	float	fadeEnd;		// When the fading hits maximum
	float	fadeTotalEnd;	// Total End Time of the fade (used for FFADE_OUT)
	float	fadeReset;	// When to reset to not fading (for fadeout and hold)
	Vector	fadeColor;
	float	fadealpha;
	int	fadeFlags;	// Fading flags
} screenfade_t;

static draw_stuff_t ds;
static screenfade_t sf;
	
int SPR_Frames( HSPRITE hPic )
{
	// FIXME: engfuncs GetImageFrames
	return 1;
}

int SPR_Height( HSPRITE hPic, int frame )
{
	int Height;

	GetImageSize( NULL, &Height, hPic );

	return Height;
}

int SPR_Width( HSPRITE hPic, int frame )
{
	int Width;

	GetImageSize( &Width, NULL, hPic );

	return Width;
}

void SPR_Set( HSPRITE hPic, int r, int g, int b )
{
	ds.hSprite = hPic;
	SetColor((r / 255.0f), (g / 255.0f), (b / 255.0f), 1.0f );
}

void SPR_Draw( int frame, int x, int y, const wrect_t *prc )
{
	// FIXME: switch rendermode
	DrawImage( ds.hSprite, x, y, prc->right, prc->bottom, frame );
}

void SPR_Draw( int frame, int x, int y, int width, int height )
{
	// FIXME: switch rendermode
	DrawImage( ds.hSprite, x, y, width, height, frame );
}

void SPR_DrawHoles( int frame, int x, int y, const wrect_t *prc )
{
	// FIXME: switch rendermode
	DrawImage( ds.hSprite, x, y, prc->right, prc->bottom, frame );
}

void SPR_DrawHoles( int frame, int x, int y, int width, int height )
{
	// FIXME: switch rendermode
	DrawImage( ds.hSprite, x, y, width, height, frame );
}

void SPR_DrawAdditive( int frame, int x, int y, const wrect_t *prc )
{
	// FIXME: switch rendermode
	DrawImage( ds.hSprite, x, y, prc->right, prc->bottom, frame );
}

void SetCrosshair( HSPRITE hspr, wrect_t rc, int r, int g, int b )
{
	ds.rgbCrosshair.x = (float)(r / 255.0f);
	ds.rgbCrosshair.y = (float)(g / 255.0f);
	ds.rgbCrosshair.z = (float)(b / 255.0f);
	ds.hCrosshair = hspr;
	ds.rcCrosshair = rc;
}

void DrawCrosshair( void )
{
	if( ds.hCrosshair == 0 ) return;

	// find center of screen
	int x = (SCREEN_WIDTH - ds.rcCrosshair.right) / 2; 
	int y = (SCREEN_HEIGHT - ds.rcCrosshair.bottom) / 2;

	// FIXME: apply crosshair angles
	SetColor( ds.rgbCrosshair.x, ds.rgbCrosshair.y, ds.rgbCrosshair.z, 1.0f );
	DrawImage( ds.hCrosshair, x, y, ds.rcCrosshair.right, ds.rcCrosshair.bottom, 0 );
}

void SetScreenFade( Vector fadeColor, float alpha, float duration, float holdTime, int fadeFlags )
{
	sf.fadeColor = fadeColor;
	sf.fadealpha = alpha;
	sf.fadeFlags = fadeFlags;
	sf.fadeEnd = gHUD.m_flTime + duration;
	sf.fadeTotalEnd = sf.fadeEnd + holdTime;
	sf.fadeSpeed = duration / gHUD.m_flTimeDelta;
}

void DrawScreenFade( void )
{
	// FIXME: implement
}

/*
====================
Sys LoadGameDLL

====================
*/
BOOL Sys_LoadLibrary( const char* dllname, dllhandle_t* handle, const dllfunction_t *fcts )
{
	const dllfunction_t	*gamefunc;
	char dllpath[256], gamedir[256];
	dllhandle_t dllhandle = 0;

	if( handle == NULL ) return false;

	// Initializations
	for( gamefunc = fcts; gamefunc && gamefunc->name != NULL; gamefunc++ )
		*gamefunc->funcvariable = NULL;

	GET_GAME_DIR( gamedir );
	sprintf( dllpath, "%s/bin/%s", gamedir, dllname );
	dllhandle = LoadLibrary( dllpath );
        
	// No DLL found
	if( !dllhandle ) return false;

	// Get the function adresses
	for( gamefunc = fcts; gamefunc && gamefunc->name != NULL; gamefunc++ )
		if(!( *gamefunc->funcvariable = (void *) Sys_GetProcAddress( dllhandle, gamefunc->name )))
		{
			Sys_UnloadLibrary( &dllhandle );
			return false;
		}
          
	ALERT( at_loading, "%s loaded succesfully!\n", dllname );
	*handle = dllhandle;
	return true;
}

void Sys_UnloadLibrary( dllhandle_t *handle )
{
	if( handle == NULL || *handle == NULL )
		return;

	FreeLibrary( *handle );
	*handle = NULL;
}

void* Sys_GetProcAddress( dllhandle_t handle, const char* name )
{
	return (void *)GetProcAddress( handle, name );
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
	static char string[16][1024], *s;
	static int stringindex = 0;

	s = string[stringindex];
	stringindex = (stringindex + 1) & 15;
	va_start( argptr, format );
	_vsnprintf( s, sizeof( string[0] ), format, argptr );
	va_end( argptr );
	return s;
}