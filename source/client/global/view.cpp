//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     view.cpp - view/refresh setup functions
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "ref_params.h"
#include "triangle_api.h"
#include "pm_movevars.h"
#include "studio_ref.h"
#include "hud.h"

#define ORIGIN_BACKUP	64
#define ORIGIN_MASK		( ORIGIN_BACKUP - 1 )

// global view containers
Vector	v_origin, v_angles, v_cl_angles;	// base client vectors
float	v_idlescale, v_lastDistance;		// misc variables
Vector	ev_punchangle;			// client punchangles
edict_t	*spot;

typedef struct 
{
	Vector	Origins[ORIGIN_BACKUP];
	float	OriginTime[ORIGIN_BACKUP];
	Vector	Angles[ORIGIN_BACKUP];
	float	AngleTime[ORIGIN_BACKUP];
	int	CurrentOrigin;
	int	CurrentAngle;
} viewinterp_t;

static viewinterp_t ViewInterp;

// view CVARS
cvar_t	*scr_ofsx;
cvar_t	*scr_ofsy;
cvar_t	*scr_ofsz;
cvar_t	*cl_vsmoothing;
cvar_t	*r_mirrors;
cvar_t	*r_portals;
cvar_t	*r_screens;
cvar_t	*r_debug;

cvar_t	*cl_bobcycle;
cvar_t	*cl_bob;
cvar_t	*cl_bobup;
cvar_t	*cl_waterdist;
cvar_t	*cl_chasedist;
cvar_t	*v_centermove;
cvar_t	*v_centerspeed;

cvar_t	*v_iyaw_cycle;
cvar_t	*v_iroll_cycle;
cvar_t	*v_ipitch_cycle;
cvar_t	*v_iyaw_level;
cvar_t	*v_iroll_level;
cvar_t	*v_ipitch_level;

//==============================================================================
//			      PASS MANAGER GLOBALS
//==============================================================================
// render manager global variables
bool g_FirstFrame		= false;
bool g_RenderReady		= false;
bool g_bFinalPass		= false;
bool g_bEndCalc		= false;
int m_RenderRefCount = 0;	// refcounter (use for debug)

// passes info
bool g_bMirrorShouldpass 	= false;
bool g_bScreenShouldpass 	= false;
bool g_bPortalShouldpass 	= false;
bool g_bSkyShouldpass 	= false;
bool g_bMirrorPass		= false;
bool g_bPortalPass		= false;
bool g_bScreenPass		= false;
bool g_bSkyPass		= false;

// rendering info
int g_iTotalVisibleMirrors	= 0;
int g_iTotalVisibleScreens	= 0;
int g_iTotalVisiblePortals	= 0;
int g_iTotalMirrors	= 0;
int g_iTotalScreens = 0;
int g_iTotalPortals = 0;

// base origin and angles
Vector g_vecBaseOrigin;       // base origin - transformed always
Vector g_vecBaseAngles;       // base angles - transformed always
Vector g_vecCurrentOrigin;	// current origin
Vector g_vecCurrentAngles;    // current angles



//============================================================================== 
//				VIEW RENDERING 
//============================================================================== 

//==========================
// V_ThirdPerson
//==========================
void V_ThirdPerson( void )
{
	// no thirdperson in multiplayer
	if( GetMaxClients() > 1 ) return;
	if( !gHUD.viewFlags )
		gHUD.m_iLastCameraMode = gHUD.m_iCameraMode = 1;
	else gHUD.m_iLastCameraMode = 1; // set new view after release camera
}

//==========================
// V_FirstPerson
//==========================
void V_FirstPerson( void )
{
	if( !gHUD.viewFlags )
		gHUD.m_iLastCameraMode = gHUD.m_iCameraMode = 0;
	else gHUD.m_iLastCameraMode = 0; // set new view after release camera
}

//==========================
// V_Init
//==========================
void V_Init( void )
{
	scr_ofsx = g_engfuncs.pfnRegisterVariable( "scr_ofsx", "0", 0, "screen offset by X" );
	scr_ofsy = g_engfuncs.pfnRegisterVariable( "scr_ofsy", "0", 0, "screen offset by Y" );
	scr_ofsz = g_engfuncs.pfnRegisterVariable( "scr_ofsz", "0", 0, "screen offset by Z" );

	cl_vsmoothing = g_engfuncs.pfnRegisterVariable( "cl_vsmoothing", "0", 0, "enables lepring in multiplayer" );

	v_iyaw_cycle = g_engfuncs.pfnRegisterVariable( "v_iyaw_cycle", "2", 0, "viewing inverse yaw cycle" );
	v_iroll_cycle = g_engfuncs.pfnRegisterVariable( "v_iroll_cycle", "0.5", 0, "viewing inverse roll cycle" );
	v_ipitch_cycle = g_engfuncs.pfnRegisterVariable( "v_ipitch_cycle", "1", 0, "viewing inverse pitch cycle" );
	v_iyaw_level = g_engfuncs.pfnRegisterVariable( "v_iyaw_level", "0.3", 0, "viewing inverse yaw level" );
	v_iroll_level = g_engfuncs.pfnRegisterVariable( "v_iroll_level", "0.1", 0, "viewing inverse roll level" );
	v_ipitch_level = g_engfuncs.pfnRegisterVariable( "v_iyaw_level", "0.3", 0, "viewing inverse pitch level" );

	v_centermove = g_engfuncs.pfnRegisterVariable( "v_centermove", "0.15", 0, "center moving scale" );
	v_centerspeed = g_engfuncs.pfnRegisterVariable( "v_centerspeed","500", 0, "center moving speed" );

	cl_bobcycle = g_engfuncs.pfnRegisterVariable( "cl_bobcycle","0.8", 0, "bob full cycle" );
	cl_bob = g_engfuncs.pfnRegisterVariable( "cl_bob", "0.01", 0, "bob value" );
	cl_bobup = g_engfuncs.pfnRegisterVariable( "cl_bobup", "0.5", 0, "bob upper limit" );
	cl_waterdist = g_engfuncs.pfnRegisterVariable( "cl_waterdist", "4", 0, "distance between viewofs and water plane" );
	cl_chasedist = g_engfuncs.pfnRegisterVariable( "cl_chasedist", "112", 0, "max distance to chase camera" );
	g_engfuncs.pfnAddCommand( "thirdperson", V_ThirdPerson, "change camera to thirdperson" );
	g_engfuncs.pfnAddCommand( "firstperson", V_FirstPerson, "change camera to firstperson" );

	r_mirrors = g_engfuncs.pfnRegisterVariable( "r_mirrors", "0", FCVAR_ARCHIVE, "enable mirrors rendering" );
	r_portals = g_engfuncs.pfnRegisterVariable( "r_portals", "0", FCVAR_ARCHIVE, "enable portals rendering" );
	r_screens = g_engfuncs.pfnRegisterVariable( "r_screens", "0", FCVAR_ARCHIVE, "enable screens rendering" );
	r_debug = g_engfuncs.pfnRegisterVariable( "r_debug", "0", 0, "show renderer state" );
}

void V_PreRender( ref_params_t *pparams )
{
	pparams->intermission = gHUD.m_iIntermission;
	pparams->thirdperson = gHUD.m_iCameraMode;
}

//==========================
// V_DropPunchAngle
//==========================
void V_DropPunchAngle( float frametime, Vector &ev_punchangle )
{
	float	len;

	len = ev_punchangle.Length();
	ev_punchangle.Normalize();

	len -= (10.0 + len * 0.5) * frametime;
	len = max( len, 0.0 );
	ev_punchangle *= len;
}

//==========================
// V_CalcGunAngle
//==========================
void V_CalcGunAngle( ref_params_t *pparams )
{	
	if( !pparams->viewmodel || pparams->fov_x > 135 ) return;

	edict_t	*viewent = GetViewModel();

	viewent->serialnumber = -1;	// viewentity are handled with special number. don't change this
	viewent->v.effects |= EF_MINLIGHT;
	viewent->v.modelindex = pparams->viewmodel;

	viewent->v.angles[YAW] = pparams->viewangles[YAW] + pparams->crosshairangle[YAW];
	viewent->v.angles[PITCH] = pparams->viewangles[PITCH] + pparams->crosshairangle[PITCH] * 0.25;
	viewent->v.angles[ROLL] -= v_idlescale * sin(pparams->time * v_iroll_cycle->value) * v_iroll_level->value;
	
	// don't apply all of the v_ipitch to prevent normally unseen parts of viewmodel from coming into view.
	viewent->v.angles[PITCH] -= v_idlescale * sin(pparams->time * v_ipitch_cycle->value) * (v_ipitch_level->value * 0.5);
	viewent->v.angles[YAW]   -= v_idlescale * sin(pparams->time * v_iyaw_cycle->value) * v_iyaw_level->value;

	viewent->v.oldangles = viewent->v.angles;
	viewent->v.oldorigin = viewent->v.origin;
}

//==========================
// V_CalcGlobalFog
//==========================
void V_CalcGlobalFog( ref_params_t *pparams )
{
	int bOn = (pparams->waterlevel < 2) && (gHUD.m_fStartDist > 0) && (gHUD.m_fEndDist > 0);
	g_engfuncs.pTriAPI->Fog( gHUD.m_FogColor, gHUD.m_fStartDist, gHUD.m_fEndDist, bOn );
}

//==========================
// V_CalcBob
//==========================
float V_CalcBob( ref_params_t *pparams )
{
	static double bobtime;
	static float bob;
	static float lasttime;
	float cycle;
	Vector vel;

	if( !pparams->onground || pparams->time == lasttime )
		return bob;	
	lasttime = pparams->time;

	bobtime += pparams->frametime;
	cycle = bobtime - (int)( bobtime / cl_bobcycle->value ) * cl_bobcycle->value;
	cycle /= cl_bobcycle->value;
	
	if ( cycle < cl_bobup->value )cycle = M_PI * cycle / cl_bobup->value;
	else cycle = M_PI + M_PI * ( cycle - cl_bobup->value )/( 1.0 - cl_bobup->value );

	vel = pparams->velocity;
	vel[2] = 0;

	bob = sqrt( vel[0] * vel[0] + vel[1] * vel[1] ) * cl_bob->value;
	bob = bob * 0.3 + bob * 0.7 * sin(cycle);
	bob = min( bob, 4 );
	bob = max( bob, -7 );
	return bob;
}

//==========================
// V_AddIdle
//==========================
void V_AddIdle( ref_params_t *pparams )
{
	pparams->viewangles[ROLL] += v_idlescale * sin(pparams->time*v_iroll_cycle->value) * v_iroll_level->value;
	pparams->viewangles[PITCH] += v_idlescale * sin(pparams->time*v_ipitch_cycle->value) * v_ipitch_level->value;
	pparams->viewangles[YAW] += v_idlescale * sin(pparams->time*v_iyaw_cycle->value) * v_iyaw_level->value;
}

//==========================
// V_CalcViewRoll
//==========================
void V_CalcViewRoll( ref_params_t *pparams )
{
	float   sign, side, value;
	Vector  forward, right, up;
	
	if( !pparams->viewentity ) return;

	AngleVectors( pparams->viewentity->v.angles, forward, right, up );
	side = pparams->velocity.Dot( right );
	sign = side < 0 ? -1 : 1;
	side = fabs( side );
	value = pparams->movevars->rollangle;
	if( side < pparams->movevars->rollspeed )
		side = side * value / pparams->movevars->rollspeed;
	else side = value;
	side = side * sign;		
	pparams->viewangles[ROLL] += side;

	if( pparams->health <= 0 && ( pparams->viewheight[2] != 0 ))
	{
		pparams->viewangles[ROLL] = 80; // dead view angle
		return;
	}
}

//==========================
// V_SetViewportRefdef
//==========================
void V_SetViewportRefdef( ref_params_t *pparams )
{
	pparams->viewport[0] = 0;
	pparams->viewport[1] = 0;
	pparams->viewport[2] = VIEWPORT_SIZE;
	pparams->viewport[3] = VIEWPORT_SIZE;
}

//==========================
// V_KillViewportRefdef
//==========================
void V_KillViewportRefdef( ref_params_t *pparams )
{
	pparams->viewport[0] = 0;
	pparams->viewport[1] = 0;
	pparams->viewport[2] = 1;
	pparams->viewport[3] = 1;
}

//==========================
// V_ResetViewportRefdef
//==========================
void V_ResetViewportRefdef( ref_params_t *pparams )
{
	pparams->viewport[0] = 0;
	pparams->viewport[1] = 0;
	pparams->viewport[2] = ScreenWidth;
	pparams->viewport[2] &= ~7;
	pparams->viewport[3] = ScreenHeight;
	pparams->viewport[3] &= ~1;
}

//==========================
// V_GetChaseOrigin
//==========================
void V_GetChaseOrigin( Vector angles, Vector origin, float distance, Vector &result )
{
	Vector	vecEnd;
	Vector	forward;
	Vector	vecStart;
	TraceResult tr;
	int maxLoops = 8;

	edict_t *ent = NULL;
	edict_t *ignoreent = NULL;
	
	// trace back from the target using the player's view angles
	AngleVectors( angles, forward, NULL, NULL );
	forward *= -1;
	vecStart = origin;

	vecEnd.MA( distance, vecStart, forward );

	while( maxLoops > 0 )
	{
		g_engfuncs.pfnTraceLine( vecStart, vecEnd, true, ignoreent, &tr );
		if( tr.pHit == NULL ) break; // we hit the world or nothing, stop trace

		ent = tr.pHit;
		if( ent == NULL ) break;

		// hit non-player solid BSP, stop here
		if( ent->v.solid == SOLID_BSP && !( ent->v.flags & FL_CLIENT )) break;

		// if close enought to end pos, stop, otherwise continue trace
		if( tr.vecEndPos.Distance( vecEnd ) < 1.0f ) break;
		else
		{
			ignoreent = tr.pHit; // ignore last hit entity
			vecStart = tr.vecEndPos;
		}
		maxLoops--;
	}  

	result.MA( 4, tr.vecEndPos, tr.vecPlaneNormal );
	v_lastDistance = tr.vecEndPos.Distance( origin );	// real distance without offset
}

//==========================
// V_GetChasePos
//==========================
void V_GetChasePos( edict_t *ent, float *cl_angles, Vector &origin, Vector &angles )
{
	if( !ent )
	{
		// just copy a save in-map position
		angles = Vector( 0, 0, 0 );
		origin = Vector( 0, 0, 0 );
		return;
	}
	if( cl_angles == NULL )
	{
		// no mouse angles given, use entity angles ( locked mode )
		angles = ent->v.angles;
		angles.x *= -1;
	}
	else angles = cl_angles;

	origin = ent->v.origin;
	origin[2] += 28; // DEFAULT_VIEWHEIGHT - some offset
          V_GetChaseOrigin( angles, origin, cl_chasedist->value, origin );
}

//==========================
// V_CalcNextView
//==========================
void V_CalcNextView( ref_params_t *pparams )
{
	if( g_FirstFrame )//first time not actually
	{
		g_FirstFrame = false;
                    g_RenderReady = false;
		
		// first time in this function
		V_PreRender( pparams ); // this set g_RenderReady at true if all is'ok

		if( !g_RenderReady ) // no hardware capable?
		{
			g_bMirrorShouldpass = false;
			g_bScreenShouldpass = false;
			g_bPortalShouldpass = false;
			g_bSkyShouldpass	= (gHUD.m_iSkyMode ? true : false); // sky must drawing always
		}
		else // all is'ok
		{
			g_bMirrorShouldpass = (g_iTotalVisibleMirrors > 0 && r_mirrors->value);
			g_bScreenShouldpass = (g_iTotalVisibleScreens > 0 && r_screens->value);
			g_bPortalShouldpass = (g_iTotalVisiblePortals > 0 && r_portals->value);
			g_bSkyShouldpass	= (gHUD.m_iSkyMode ? true : false);
		}
		m_RenderRefCount	= 0;	// reset debug counter
		g_bMirrorPass	= false;
		g_bScreenPass	= false;
		g_bPortalPass	= false;
		g_bSkyPass	= false;
		g_bFinalPass	= false;
	}
}

//==========================
// V_CalcCameraRefdef
//==========================
void V_CalcCameraRefdef( ref_params_t *pparams )
{
	if( pparams->intermission ) return;	// disable in intermission mode

	if( gHUD.viewFlags & CAMERA_ON )
	{         
		// get viewentity and monster eyeposition
		edict_t	*viewentity = GetEntityByIndex( gHUD.viewEntityIndex );
	 	if( viewentity )
		{		 
			dstudiohdr_t *viewmonster = (dstudiohdr_t *)GetModelPtr( viewentity );

			if( gHUD.viewFlags & MONSTER_VIEW && viewmonster ) // calc monster view
				v_origin = viewentity->v.origin + viewmonster->eyeposition;
			else v_origin = viewentity->v.origin;
			v_angles = viewentity->v.angles;

			if( gHUD.viewFlags & INVERSE_X )	// inverse X coordinate
				v_angles[0] = -v_angles[0];
			pparams->crosshairangle[PITCH] = 100;	// hide crosshair

			// refresh position
			pparams->viewangles = v_angles;
			pparams->vieworg = v_origin;
		}
	}
	else pparams->crosshairangle[PITCH] = 0; // show crosshair again
}

edict_t *V_FindIntermisionSpot( ref_params_t *pparams )
{
	edict_t *ent;
	int spotindex[16];	// max number of intermission spot
	int k = 0, j = 0;

	// found intermission point
	for( int i = 0; i < pparams->max_entities; i++ )
	{
		ent = GetEntityByIndex( i );
		if( ent && !stricmp( STRING( ent->v.classname ), "info_intermission" ))
		{
			if( j > 15 ) break; // spotlist is full
			spotindex[j] = ent->serialnumber; // save entindex
			j++;
		}
	}	
	
	// ok, we have list of intermission spots
	if( j )
	{
		if( j > 1 ) k = g_engfuncs.pfnRandomLong( 0, j - 1 );
		ent = GetEntityByIndex( spotindex[k] );
	}
	else ent = GetLocalPlayer(); // just get view from player
	
	return ent;
}

//==========================
// V_CalcIntermisionRefdef
//==========================
void V_CalcIntermisionRefdef( ref_params_t *pparams )
{
	if( !pparams->intermission ) return;

	edict_t	*view;
	float	  old;

          if( !spot ) spot = V_FindIntermisionSpot( pparams );
	view = GetViewModel();

	pparams->vieworg = spot->v.origin;
	pparams->viewangles = spot->v.angles;
	view->v.modelindex = 0;

	// allways idle in intermission
	old = v_idlescale;
	v_idlescale = 1;
	V_AddIdle( pparams );

	v_idlescale = old;
	v_cl_angles = pparams->angles;
	v_origin = pparams->vieworg;
	v_angles = pparams->viewangles;
}

//==========================
// V_PrintDebugInfo
// FIXME: use custom text drawing ?
//==========================
void V_PrintDebugInfo (ref_params_t *pparams) //for future extensions
{
	if( !r_debug->value ) return; //show OpenGL renderer debug info
	ALERT( at_console, "Xash Renderer Info: ");
	if( m_RenderRefCount > 1 ) ALERT( at_console, "Total %d passes\n", m_RenderRefCount );
	else ALERT( at_console, "Use normal view, make one pass\n" );
	if( g_iTotalMirrors ) ALERT( at_console, "Visible mirrors: %d from %d\n", g_iTotalVisibleMirrors, g_iTotalMirrors );
	if( g_iTotalScreens ) ALERT( at_console, "Visible screens: %d from %d\n", g_iTotalVisibleScreens, g_iTotalScreens );
	if( g_iTotalPortals ) ALERT( at_console, "Visible portals: %d from %d\n", g_iTotalVisiblePortals, g_iTotalPortals );
}

//==========================
// V_CalcFinalPass
//==========================
void V_CalcFinalPass( ref_params_t *pparams )
{
	g_FirstFrame = true; // enable calc next passes
	V_ResetViewportRefdef( pparams ); // reset view port as default
	m_RenderRefCount++; // increase counter
}

//==========================
// V_CalcThirdPersonRefdef
//==========================
void V_CalcThirdPersonRefdef( ref_params_t * pparams )
{
	// passed only in third person
	if( gHUD.m_iCameraMode == 0 || pparams->intermission ) return;

	// get current values
	v_cl_angles = pparams->angles;
	v_angles = pparams->viewangles;
	v_origin = pparams->vieworg;

	V_GetChasePos( GetLocalPlayer(), v_cl_angles, v_origin, v_angles );

	// write back new values
	pparams->angles = v_cl_angles;
	pparams->viewangles = v_angles;
	pparams->vieworg = v_origin;
}

//==========================
// V_CalcSendOrigin
//==========================
void V_CalcSendOrigin( ref_params_t *pparams )
{
	// never let view origin sit exactly on a node line, because a water plane can
	// dissapear when viewed with the eye exactly on it.
	pparams->vieworg[0] += 1.0 / 32;
	pparams->vieworg[1] += 1.0 / 32;
	pparams->vieworg[2] += 1.0 / 32;
}

//==========================
// V_CalcWaterLevel
//==========================
float V_CalcWaterLevel( ref_params_t *pparams )
{
	float waterOffset = 0;
	
	if( pparams->waterlevel >= 2 )
	{
		int i, contents, waterDist;
		waterDist = cl_waterdist->value;
		TraceResult tr;
		Vector point;

		TRACE_HULL( pparams->origin, Vector(-16,-16,-24), Vector(16,16,32), pparams->origin, 1, GetLocalPlayer(), &tr );

		if( tr.pHit && !stricmp( STRING( tr.pHit->v.classname ), "func_water" ))
			waterDist += ( tr.pHit->v.scale * 16 );
		point = pparams->vieworg;

		// eyes are above water, make sure we're above the waves
		if( pparams->waterlevel == 2 )	
		{
			point[2] -= waterDist;
			for( i = 0; i < waterDist; i++ )
			{
				contents = POINT_CONTENTS( point );
				if( contents > CONTENTS_WATER ) break;
				point[2] += 1;
			}
			waterOffset = (point[2] + waterDist) - pparams->vieworg[2];
		}
		else
		{
			// eyes are under water. Make sure we're far enough under
			point[2] += waterDist;
			for( i = 0; i < waterDist; i++ )
			{
				contents = POINT_CONTENTS( point );
				if( contents <= CONTENTS_WATER ) break;
				point[2] -= 1;
			}
			waterOffset = (point[2] - waterDist) - pparams->vieworg[2];
		}
	}
	pparams->vieworg[2] += waterOffset;
	return waterOffset;
}

//==========================
// V_CalcScrOffset
//==========================
void V_CalcScrOffset( ref_params_t *pparams )
{
	// don't allow cheats in multiplayer
	if( pparams->max_clients > 1 ) return;

	for( int i = 0; i < 3; i++ )
	{
		pparams->vieworg[i] += scr_ofsx->value * pparams->forward[i];
		pparams->vieworg[i] += scr_ofsy->value * pparams->right[i];
		pparams->vieworg[i] += scr_ofsz->value * pparams->up[i];
	}
}

//==========================
// V_CalcShake
//==========================
void V_CalcShake( void )
{
	float	frametime;
	int	i;
	float	fraction, freq;

	if( gHUD.m_Shake.time == 0 )
		return;

	if(( gHUD.m_flTime > gHUD.m_Shake.time ) || gHUD.m_Shake.duration <= 0 || gHUD.m_Shake.amplitude <= 0 || gHUD.m_Shake.frequency <= 0 )
	{
		memset( &gHUD.m_Shake, 0, sizeof( gHUD.m_Shake ));
		return;
	}

	frametime = gHUD.m_flTimeDelta;

	if( gHUD.m_flTime > gHUD.m_Shake.nextShake )
	{
		// higher frequency means we recalc the extents more often and perturb the display again
		gHUD.m_Shake.nextShake = gHUD.m_flTime + (1.0f / gHUD.m_Shake.frequency);

		// Compute random shake extents (the shake will settle down from this)
		for( i = 0; i < 3; i++ )
		{
			gHUD.m_Shake.offset[i] = RANDOM_FLOAT( -gHUD.m_Shake.amplitude, gHUD.m_Shake.amplitude );
		}
		gHUD.m_Shake.angle = RANDOM_FLOAT( -gHUD.m_Shake.amplitude * 0.25, gHUD.m_Shake.amplitude * 0.25 );
	}

	// ramp down amplitude over duration (fraction goes from 1 to 0 linearly with slope 1/duration)
	fraction = ( gHUD.m_Shake.time - gHUD.m_flTime ) / gHUD.m_Shake.duration;

	// ramp up frequency over duration
	if( fraction )
	{
		freq = (gHUD.m_Shake.frequency / fraction);
	}
	else
	{
		freq = 0;
	}

	// square fraction to approach zero more quickly
	fraction *= fraction;

	// Sine wave that slowly settles to zero
	fraction = fraction * sin( gHUD.m_flTime * freq );
	
	// add to view origin
	gHUD.m_Shake.appliedOffset = gHUD.m_Shake.offset * fraction;
	
	// add to roll
	gHUD.m_Shake.appliedAngle = gHUD.m_Shake.angle * fraction;

	// drop amplitude a bit, less for higher frequency shakes
	float localAmp = gHUD.m_Shake.amplitude * ( frametime / ( gHUD.m_Shake.duration * gHUD.m_Shake.frequency ));
	gHUD.m_Shake.amplitude -= localAmp;
}

//==========================
// V_ApplyShake
//==========================
void V_ApplyShake( Vector& origin, Vector& angles, float factor )
{
	origin.MA( factor, origin, gHUD.m_Shake.appliedOffset );
	angles.z += gHUD.m_Shake.appliedAngle * factor;
}

//==========================
// V_CalcSmoothAngles
//==========================
void V_CalcSmoothAngles( ref_params_t *pparams )
{
	edict_t	*view;

	// view is the weapon model (only visible from inside body )
	view = GetViewModel();

	if( cl_vsmoothing->value && ( pparams->smoothing && pparams->max_clients > 1 ))
	{
		int i, foundidx;
		float t;

		if( cl_vsmoothing->value < 0.0 ) CVAR_SET_FLOAT( "cl_vsmoothing", 0 );
		t = pparams->time - cl_vsmoothing->value;
		for( i = 1; i < ORIGIN_MASK; i++ )
		{
			foundidx = ViewInterp.CurrentOrigin - 1 - i;
			if( ViewInterp.OriginTime[foundidx & ORIGIN_MASK] <= t ) break;
		}

		if( i < ORIGIN_MASK && ViewInterp.OriginTime[foundidx & ORIGIN_MASK] != 0.0 )
		{
			// interpolate
			Vector delta;
			double frac;
			double dt;
			Vector neworg;

			dt = ViewInterp.OriginTime[ (foundidx + 1) & ORIGIN_MASK ] - ViewInterp.OriginTime[foundidx & ORIGIN_MASK];
			if ( dt > 0.0 )
			{
				frac = ( t - ViewInterp.OriginTime[ foundidx & ORIGIN_MASK] ) / dt;
				frac = min( 1.0, frac );
				delta = ViewInterp.Origins[(foundidx + 1) & ORIGIN_MASK] - ViewInterp.Origins[foundidx & ORIGIN_MASK];
				neworg.MA( frac, ViewInterp.Origins[foundidx & ORIGIN_MASK], delta );

				// don't interpolate large changes
				if( delta.Length() < 64 )
				{
					delta = neworg - pparams->origin;
					pparams->origin += delta;
					pparams->vieworg += delta;
					view->v.origin += delta;

				}
			}
		}
	}
}

//==========================
// V_CalcFirstPersonRefdef
//==========================
void V_CalcFirstPersonRefdef( ref_params_t *pparams )
{
	// don't pass in thirdperson or intermission
	if( gHUD.m_iCameraMode || pparams->intermission )
		return;

	Vector angles;
	float bob, waterOffset;
	static float lasttime;
	edict_t *view = GetViewModel();
	int i;

	bob = V_CalcBob( pparams );

	// refresh position
	if( !pparams->predicting )
	{
		// use interpolated values
		for( i = 0; i < 3; i++ )
		{
			pparams->vieworg[i] = LerpPoint( pparams->prev.origin[i], pparams->origin[i], pparams->lerpfrac );
			pparams->vieworg[i] += LerpPoint( pparams->prev.viewheight[i], pparams->viewheight[i], pparams->lerpfrac );
			pparams->viewangles[i] = LerpAngle( pparams->prev.angles[i], pparams->angles[i], pparams->lerpfrac );
		}
	}

	pparams->viewangles[PITCH] = -pparams->viewangles[PITCH];

	pparams->vieworg[2] += ( bob );

	V_CalcShake();
	V_ApplyShake( pparams->vieworg, pparams->viewangles, 1.0 );

	V_CalcSendOrigin( pparams );
	waterOffset = V_CalcWaterLevel( pparams );
	V_CalcViewRoll( pparams );
	V_AddIdle( pparams );

	// offsets
	angles = pparams->viewangles;
	AngleVectors( angles, pparams->forward, pparams->right, pparams->up );
	V_CalcScrOffset( pparams );

	view->v.angles = pparams->viewangles;
	view->v.oldangles = pparams->viewangles;
	V_CalcGunAngle( pparams );

	// use predicted origin as view origin.
	view->v.origin = pparams->vieworg;      
	view->v.oldorigin = pparams->vieworg;
	view->v.origin[2] += ( waterOffset );

	// Let the viewmodel shake at about 10% of the amplitude
	V_ApplyShake( view->v.origin, view->v.angles, 0.9 );

	for( i = 0; i < 3; i++ )
		view->v.origin[i] += bob * 0.4 * pparams->forward[i];
	view->v.origin[2] += bob;

	view->v.angles[YAW] -= bob * 0.5;
	view->v.angles[ROLL] -= bob * 1;
	view->v.angles[PITCH] -= bob * 0.3;
//	view->v.origin[2] -= 1;

	if( !g_bFinalPass ) pparams->punchangle = -pparams->punchangle; // make inverse for mirror

	for( i = 0; i < 3; i++ )
		pparams->viewangles[i] += LerpAngle( pparams->prev.punchangle[i], pparams->punchangle[i], pparams->lerpfrac );
		
	pparams->viewangles += ev_punchangle;
	V_DropPunchAngle( pparams->frametime, ev_punchangle );

	static float oldz = 0;

/*
	if( !pparams->smoothing && pparams->onground && pparams->origin[2] - oldz > 0 )
	{
		float steptime;
		
		steptime = pparams->time - lasttime;
		if( steptime < 0 ) steptime = 0;

		oldz += steptime * 150;
		if( oldz > pparams->origin[2])
			oldz = pparams->origin[2];
		if( pparams->origin[2] - oldz > pparams->movevars->stepheight )
			oldz = pparams->origin[2] - pparams->movevars->stepheight;
		pparams->vieworg[2] += oldz - pparams->origin[2];
		view->v.origin[2] += oldz - pparams->origin[2];
	}
	else oldz = pparams->origin[2];
*/
	static Vector lastorg;
	Vector delta;

	delta = pparams->origin - lastorg;

	if( delta.Length() != 0.0 )
	{
		ViewInterp.Origins[ViewInterp.CurrentOrigin & ORIGIN_MASK] = pparams->origin;
		ViewInterp.OriginTime[ViewInterp.CurrentOrigin & ORIGIN_MASK] = pparams->time;
		ViewInterp.CurrentOrigin++;

		lastorg = pparams->origin;
	}
	V_CalcSmoothAngles( pparams ); // smooth angles in multiplayer

	lasttime = pparams->time;
	v_origin = pparams->vieworg;	
}

//==========================
// V_CalcMainRefdef
//==========================
void V_CalcMainRefdef( ref_params_t *pparams )
{
	if( g_FirstFrame ) g_bFinalPass = true;
	V_CalcFirstPersonRefdef( pparams );
	V_CalcThirdPersonRefdef( pparams );
	V_CalcIntermisionRefdef( pparams );
	V_CalcCameraRefdef( pparams );

	g_vecBaseOrigin = pparams->vieworg;
	g_vecBaseAngles = pparams->viewangles;
}

//==========================
// V_CalcSkyRefdef
//==========================
bool V_CalcSkyRefdef( ref_params_t *pparams )
{
	if( g_bSkyShouldpass )
	{
		g_bSkyShouldpass = false;
		V_CalcMainRefdef( pparams );	// refresh position
		pparams->vieworg = gHUD.m_vecSkyPos;
		V_ResetViewportRefdef( pparams );
		pparams->nextView = 1;
		g_bSkyPass = true;
		m_RenderRefCount++;
		return true;
	}
	if( g_bSkyPass )
	{
		pparams->nextView = 0;
		g_bSkyPass = false;	
		return false;
	}
	return false;
}

//==========================
// V_CalcMirrorsRefdef
//==========================
bool V_CalcMirrorsRefdef( ref_params_t *pparams )
{
	if( g_bMirrorShouldpass )
	{
		if( pparams->nextView == 0 )
		{
			g_bMirrorPass = true;

			// this is first pass rendering (setup mirror viewport and nextView's)
                              V_SetViewportRefdef( pparams );

			// enable clip plane once in first mirror!
			g_engfuncs.pTriAPI->Enable( TRI_CLIP_PLANE );
                              V_CalcMainRefdef( pparams );
#ifdef XASH_RENDER
			m_pCurrentMirror = NULL;
			R_SetupNewMirror( pparams );
			R_SetupMirrorRenderPass( pparams );
#endif
			pparams->nextView = g_iTotalVisibleMirrors;

			m_RenderRefCount++;
			return true;
		}
		else if( pparams->nextView == 1 )
		{
#ifdef XASH_RENDER
			R_NewMirrorRenderPass();	// capture view to texture
			m_pCurrentMirror = NULL;
#endif
			g_engfuncs.pTriAPI->Disable( TRI_CLIP_PLANE );
			V_ResetViewportRefdef( pparams );
			pparams->nextView = 0;

			g_bMirrorPass = false;
			g_bMirrorShouldpass = false;
			return false;
		}
		else
		{
#ifdef XASH_RENDER
			R_NewMirrorRenderPass();	// capture view to texture
			R_SetupNewMirror( pparams );
			R_SetupMirrorRenderPass( pparams );
#endif
			pparams->nextView--;
			m_RenderRefCount++;
			return true;
		}
	}
	return false;
}

//==========================
// V_CalcScreensRefdef
//==========================
bool V_CalcScreensRefdef( ref_params_t *pparams )
{
	if( g_bScreenShouldpass )
	{
		if( pparams->nextView == 0 )
		{
			// this is first pass rendering (setup screen viewport and nextView's)
			g_bScreenPass = true;
			V_SetViewportRefdef( pparams );
#ifdef XASH_RENDER			
			m_pCurrentScreen = NULL;
			R_SetupNewScreen(pparams);
			R_SetupScreenRenderPass(pparams);
#endif
			pparams->nextView = g_iTotalVisibleScreens;
			m_RenderRefCount++;
			return true;//end of pass
		}
		else if( pparams->nextView == 1 )
		{
#ifdef XASH_RENDER
			R_NewScreenRenderPass();	// capture view to texture
			m_pCurrentScreen = NULL;
#endif                              
			V_ResetViewportRefdef( pparams );
			pparams->nextView = 0;
			g_bScreenShouldpass = false;
			return false;
		}
		else
		{
#ifdef XASH_RENDER
			R_NewScreenRenderPass();	// capture view to texture
			R_SetupNewScreen( pparams );
			R_SetupScreenRenderPass( pparams );
#endif
			pparams->nextView--;
			m_RenderRefCount++;
			return true;
		}
	}
	return false;
}

//==========================
// V_CalcPortalsRefdef
//==========================
bool V_CalcPortalsRefdef( ref_params_t *pparams )
{
	if( g_bPortalShouldpass )
	{
		if( pparams->nextView == 0 )
		{
			// this is first pass rendering (setup mirror viewport and nextView's)
			g_bPortalPass = true;
                              V_SetViewportRefdef( pparams );

                              V_CalcMainRefdef( pparams );
#ifdef XASH_RENDER
			m_pCurrentPortal = NULL;
			R_SetupNewPortal( pparams );
			R_SetupPortalRenderPass( pparams );
#endif
			pparams->nextView = g_iTotalVisiblePortals;
			m_RenderRefCount++;
			return true;
		}
		else if( pparams->nextView == 1 )
		{
#ifdef XASH_RENDER
			R_NewPortalRenderPass();	// capture view to texture
			m_pCurrentPortal = NULL;
#endif                              
			// restore for final pass
			V_ResetViewportRefdef( pparams );
			pparams->nextView = 0;
			g_bPortalShouldpass = false;
			return false;
		}
		else
		{
#ifdef XASH_RENDER
			R_NewPortalRenderPass();	// capture view to texture
			R_SetupNewPortal( pparams );
			R_SetupPortalRenderPass( pparams );
#endif
			pparams->nextView--;
			m_RenderRefCount++;
			return true;
		}
	}
	return false;
}

void V_CalcRefdef( ref_params_t *pparams )
{
	// can't use a paused refdef
	if( pparams->paused ) return;

	V_CalcNextView( pparams );
	
	if( V_CalcSkyRefdef( pparams )) return;
	if( V_CalcScreensRefdef( pparams )) return;
	if( V_CalcPortalsRefdef( pparams )) return;
	if( V_CalcMirrorsRefdef( pparams )) return;
	
	V_CalcGlobalFog( pparams );
          V_CalcFinalPass ( pparams );
	V_CalcMainRefdef( pparams );
	
	V_PrintDebugInfo( pparams );
}