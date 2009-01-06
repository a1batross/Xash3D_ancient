//=======================================================================
//			Copyright XashXT Group 2008 ©
//		          dll_int.cpp - dll entry points 
//=======================================================================

#include "extdll.h"
#include "hud_iface.h"
#include "hud.h"

cl_enginefuncs_t g_engfuncs;
CHud gHUD;

// main DLL entry point
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
	return TRUE;
}

static HUD_FUNCTIONS gFunctionTable = 
{
	sizeof( HUD_FUNCTIONS ),
	HUD_VidInit,
	HUD_Init,
	HUD_Redraw,
	HUD_UpdateClientData,
	HUD_Reset,
	HUD_Frame,
	HUD_Shutdown,
	HUD_DrawNormalTriangles,
	HUD_DrawTransparentTriangles,
	HUD_CreateEntities,
	HUD_StudioEvent,
	V_CalcRefdef,
};

//=======================================================================
//			GetApi
//=======================================================================
int CreateAPI( HUD_FUNCTIONS *pFunctionTable, cl_enginefuncs_t* pEngfuncsFromEngine, int interfaceVersion )
{
	if( !pFunctionTable || !pEngfuncsFromEngine || interfaceVersion != INTERFACE_VERSION )
	{
		return FALSE;
	}

	// copy HUD_FUNCTIONS table to engine, copy engfuncs table from engine
	memcpy( pFunctionTable, &gFunctionTable, sizeof( HUD_FUNCTIONS ));
	memcpy( &g_engfuncs, pEngfuncsFromEngine, sizeof( cl_enginefuncs_t ));

	return TRUE;
}

int HUD_VidInit( void )
{
	gHUD.VidInit();

	return 1;
}

void HUD_Init( void )
{
	gHUD.Init();
}

int HUD_Redraw( float flTime, int state )
{
	switch( state )
	{
	case CL_DISCONNECTED:
		V_RenderSplash();
		break;
	case CL_LOADING:
		V_RenderPlaque();
		break;
	case CL_ACTIVE:
		gHUD.Redraw( flTime );
		DrawCrosshair();
		DrawPause();
		break;
	}
	return 1;
}

int HUD_UpdateClientData( client_data_t *cdata, float flTime )
{
	return gHUD.UpdateClientData( cdata, flTime );
}

void HUD_Reset( void )
{
	gHUD.VidInit();
}

void HUD_Frame( double time )
{
	// place to call vgui_frame
}

void HUD_Shutdown( void )
{
	// no shutdown operations
}