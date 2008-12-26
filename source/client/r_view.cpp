//=======================================================================
//			Copyright (C) Shambler Team 2005
//			r_view.cpp - multipass renderer
//=======================================================================

#include "hud.h"
#include "r_main.h"
#include "r_view.h"
#include "r_util.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "ref_params.h"
#include "in_defs.h"
#include "pm_movevars.h"
#include "pm_shared.h"
#include "pm_defs.h"
#include "event_api.h"
#include "pmtrace.h"
#include "screenfade.h"
#include "shake.h"
#include "studio.h"
#include "r_studioint.h"
#include "r_studio.h"
#include "com_model.h"

//==============================================================================
//			INIT GLOBAL VARIABLES & CONSTANTS 
//==============================================================================

//fundamental engine callbacks
extern "C" 
{
	void DLLEXPORT V_CalcRefdef( struct ref_params_s *pparams );
	void DLLEXPORT CAM_Think( void );
	int  DLLEXPORT CL_IsThirdPerson( void );
	void DLLEXPORT CL_CameraOffset( float *ofs );
	float	vJumpOrigin[3];
	float	vJumpAngles[3];
}

//fog external variables
extern vec3_t FogColor;
extern float g_fStartDist;
extern float g_fEndDist;
extern engine_studio_api_t IEngineStudio;//"view at monster" camera use this
extern CGameStudioModelRenderer g_StudioRenderer;//control over player drawing

float	v_idlescale, v_lastDistance;

//global view containers
vec3_t v_origin, v_angles, v_cl_angles;//base client vectors
vec3_t ev_punchangle;	//client punchangles

extern int  PM_GetPhysEntInfo( int ent );//thirdperson clip camera use this 
extern float Distance(const vec3_t v1, const vec3_t v2);
static viewinterp_t	ViewInterp;  
cl_entity_t *spot;
int pause = 0;

//thirdperson camera
void DLLEXPORT CAM_Think( void ){}
void DLLEXPORT CL_CameraOffset( float *ofs ) {VectorCopy( vec3_origin, ofs );}
int  DLLEXPORT CL_IsThirdPerson( void ){return (gHUD.m_iCameraMode ? 1 : 0 );}

//View CVARS
cvar_t		*scr_ofsx;
cvar_t		*scr_ofsy;
cvar_t		*scr_ofsz;
extern cvar_t	*cl_forwardspeed;
extern cvar_t	*scr_ofsx, *scr_ofsy, *scr_ofsz;
extern cvar_t	*cl_vsmoothing;

cvar_t	*cl_bobcycle;
cvar_t	*cl_bob;
cvar_t	*cl_bobup;
cvar_t	*cl_waterdist;
cvar_t	*cl_chasedist;
cvar_t	*v_centermove;
cvar_t	*v_centerspeed;

cvar_t	v_iyaw_cycle	= {"v_iyaw_cycle", "2", 0, 2};
cvar_t	v_iroll_cycle	= {"v_iroll_cycle", "0.5", 0, 0.5};
cvar_t	v_ipitch_cycle	= {"v_ipitch_cycle", "1", 0, 1};
cvar_t	v_iyaw_level  	= {"v_iyaw_level", "0.3", 0, 0.3};
cvar_t	v_iroll_level 	= {"v_iroll_level", "0.1", 0, 0.1};
cvar_t	v_ipitch_level	= {"v_ipitch_level", "0.3", 0, 0.3};
//==============================================================================
//			      PASS MANAGER GLOBALS
//==============================================================================
//render manager global variables
bool g_FirstFrame		= false;
bool g_RenderReady		= false;
bool g_bFinalPass		= false;
bool g_bEndCalc		= false;
int m_RenderRefCount 	= 0;   //refcounter (use for debug)

//passes info
bool g_bMirrorShouldpass 	= false;
bool g_bScreenShouldpass 	= false;
bool g_bPortalShouldpass 	= false;
bool g_bSkyShouldpass 	= false;
bool g_bMirrorPass		= false;
bool g_bPortalPass		= false;
bool g_bScreenPass		= false;
bool g_bSkyPass		= false;

//base origin and angles
vec3_t g_vecBaseOrigin;       //base origin - transformed always
vec3_t g_vecBaseAngles;       //base angles - transformed always
vec3_t g_vecCurrentOrigin;	//current origin
vec3_t g_vecCurrentAngles;    //current angles
//============================================================================== 
//				VIEW RENDERING 
//============================================================================== 

//==========================
// V_ThirdPerson
//==========================
void V_ThirdPerson(void)
{
	if ( gEngfuncs.GetMaxClients() > 1 ) return;//no thirdperson in multiplayer
	if (!gHUD.viewFlags)
		gHUD.m_iLastCameraMode = gHUD.m_iCameraMode = 1;
	else gHUD.m_iLastCameraMode = 1; //set new view after release camera
}

//==========================
// V_FirstPerson
//==========================
void V_FirstPerson(void)
{
	if (!gHUD.viewFlags)
		gHUD.m_iLastCameraMode = gHUD.m_iCameraMode = 0;
	else gHUD.m_iLastCameraMode = 0; //set new view after release camera
}

//==========================
// V_Init
//==========================
void V_Init (void)
{
	gEngfuncs.pfnAddCommand ("centerview", V_StartPitchDrift );

	scr_ofsx		= gEngfuncs.pfnRegisterVariable( "scr_ofsx","0", 0 );
	scr_ofsy		= gEngfuncs.pfnRegisterVariable( "scr_ofsy","0", 0 );
	scr_ofsz		= gEngfuncs.pfnRegisterVariable( "scr_ofsz","0", 0 );

	v_centermove	= gEngfuncs.pfnRegisterVariable( "v_centermove", "0.15", 0 );
	v_centerspeed	= gEngfuncs.pfnRegisterVariable( "v_centerspeed","500", 0 );

	cl_bobcycle	= gEngfuncs.pfnRegisterVariable( "cl_bobcycle","0.8", 0 );
	cl_bob		= gEngfuncs.pfnRegisterVariable( "cl_bob","0.01", 0 );
	cl_bobup		= gEngfuncs.pfnRegisterVariable( "cl_bobup","0.5", 0 );
	cl_waterdist	= gEngfuncs.pfnRegisterVariable( "cl_waterdist","4", 0 );
	cl_chasedist	= gEngfuncs.pfnRegisterVariable( "cl_chasedist","112", 0 );
	gEngfuncs.pfnAddCommand ( "thirdperson", V_ThirdPerson );
	gEngfuncs.pfnAddCommand ( "firstperson", V_FirstPerson );
}

//==========================
// V_StartPitchDrift
//==========================
void V_StartPitchDrift( void )
{
	if ( pd.laststop == gEngfuncs.GetClientTime())return;
	if ( pd.nodrift || !pd.pitchvel )
	{
		pd.pitchvel = v_centerspeed->value;
		pd.nodrift = 0;
		pd.driftmove = 0;
	}
}

//==========================
// V_StopPitchDrift
//==========================
void V_StopPitchDrift ( void )
{
	pd.laststop = gEngfuncs.GetClientTime();
	pd.nodrift = 1;
	pd.pitchvel = 0;
}

//==========================
// V_DriftPitch
//==========================
void V_DriftPitch ( struct ref_params_s *pparams )
{
	float delta, move;

	if( gEngfuncs.IsNoClipping() || !pparams->onground || pparams->demoplayback )
	{
		pd.driftmove = 0;
		pd.pitchvel = 0;
		return;
	}

	if (pd.nodrift)
	{
		if ( fabs( pparams->cmd->forwardmove ) < cl_forwardspeed->value )
			pd.driftmove = 0;
		else pd.driftmove += pparams->frametime;
		if ( pd.driftmove > v_centermove->value)V_StartPitchDrift ();
		return;
	}
	
	delta = pparams->idealpitch - pparams->cl_viewangles[PITCH];

	if (!delta)
	{
		pd.pitchvel = 0;
		return;
	}

	move = pparams->frametime * pd.pitchvel;
	pd.pitchvel += pparams->frametime * v_centerspeed->value;
	
	if (delta > 0)
	{
		if (move > delta)
		{
			pd.pitchvel = 0;
			move = delta;
		}
		pparams->cl_viewangles[PITCH] += move;
	}
	else if (delta < 0)
	{
		if (move > -delta)
		{
			pd.pitchvel = 0;
			move = -delta;
		}
		pparams->cl_viewangles[PITCH] -= move;
	}
}

//==========================
// V_DropPunchAngle
//==========================
void V_DropPunchAngle( float frametime, float *ev_punchangle )
{
	float	len;
	len = VectorNormalize ( ev_punchangle );
	len -= (10.0 + len * 0.5) * frametime;
	len = max( len, 0.0 );
	VectorScale ( ev_punchangle, len, ev_punchangle );
}

//==========================
// V_CalcGunAngle
//==========================
void V_CalcGunAngle( struct ref_params_s *pparams )
{	
	cl_entity_t *viewent;
	
	viewent = gEngfuncs.GetViewModel();
	if ( !viewent )return;

	viewent->angles[YAW]   =  pparams->viewangles[YAW]   + pparams->crosshairangle[YAW];
	viewent->angles[PITCH] = -pparams->viewangles[PITCH] + pparams->crosshairangle[PITCH] * 0.25;
	viewent->angles[ROLL]  -= v_idlescale * sin(pparams->time*v_iroll_cycle.value) * v_iroll_level.value;
	
	// don't apply all of the v_ipitch to prevent normally unseen parts of viewmodel from coming into view.
	viewent->angles[PITCH] -= v_idlescale * sin(pparams->time*v_ipitch_cycle.value) * (v_ipitch_level.value * 0.5);
	viewent->angles[YAW]   -= v_idlescale * sin(pparams->time*v_iyaw_cycle.value) * v_iyaw_level.value;

	VectorCopy( viewent->angles, viewent->curstate.angles );
	VectorCopy( viewent->angles, viewent->latched.prevangles );
}

//==========================
// V_CalcGlobalFog
//==========================
void V_CalcGlobalFog( struct ref_params_s *pparams )
{
	float g_fFogColor[4] = { FogColor.x, FogColor.y, FogColor.z, 1.0 };
	bool bFog = pparams->waterlevel < 2 && g_fStartDist > 0 && g_fEndDist > 0;
	if (!bFog) return;

	//rendering global fog
	glEnable(GL_FOG);
	glFogf(GL_FOG_DENSITY, 0.0025);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogfv(GL_FOG_COLOR, g_fFogColor);
	glFogf(GL_FOG_START, g_fStartDist);
	glFogf(GL_FOG_END, g_fEndDist);
	glHint(GL_FOG_HINT, GL_NICEST);
}

//==========================
// V_CalcBob
//==========================
float V_CalcBob ( struct ref_params_s *pparams )
{
	static double bobtime;
	static float bob;
	float cycle;
	static float lasttime;
	vec3_t vel;

	if ( pparams->onground == -1 || pparams->time == lasttime ) return bob;	
	lasttime = pparams->time;

	bobtime += pparams->frametime;
	cycle = bobtime - (int)( bobtime / cl_bobcycle->value ) * cl_bobcycle->value;
	cycle /= cl_bobcycle->value;
	
	if ( cycle < cl_bobup->value )cycle = M_PI * cycle / cl_bobup->value;
	else cycle = M_PI + M_PI * ( cycle - cl_bobup->value )/( 1.0 - cl_bobup->value );

	VectorCopy( pparams->simvel, vel );
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
void V_AddIdle( struct ref_params_s *pparams )
{
	pparams->viewangles[ROLL] += v_idlescale * sin(pparams->time*v_iroll_cycle.value) * v_iroll_level.value;
	pparams->viewangles[PITCH] += v_idlescale * sin(pparams->time*v_ipitch_cycle.value) * v_ipitch_level.value;
	pparams->viewangles[YAW] += v_idlescale * sin(pparams->time*v_iyaw_cycle.value) * v_iyaw_level.value;
}

//==========================
// V_CalcViewRoll
//==========================
void V_CalcViewRoll( struct ref_params_s *pparams )
{
	float   sign, side, value;
	vec3_t  forward, right, up;
	cl_entity_t *viewentity;
	
	viewentity = gEngfuncs.GetEntityByIndex( pparams->viewentity );
	if ( !viewentity ) return;

	AngleVectors ( viewentity->angles, forward, right, up );
	side = DotProduct (pparams->simvel, right);
	sign = side < 0 ? -1 : 1;
	side = fabs( side );
	value = pparams->movevars->rollangle;
	if (side < pparams->movevars->rollspeed)side = side * value / pparams->movevars->rollspeed;
	else side = value;
	side = side * sign;		
	pparams->viewangles[ROLL] += side;

	if ( pparams->health <= 0 && ( pparams->viewheight[2] != 0 ) )
	{
		pparams->viewangles[ROLL] = 80; // dead view angle
		return;
	}
}

//==========================
// V_SetViewportRefdef
//==========================
void V_SetViewportRefdef (struct ref_params_s *pparams)
{
	pparams->viewport[0] = 0;
	pparams->viewport[1] = 0;
	pparams->viewport[2] = VIEWPORT_SIZE;
	pparams->viewport[3] = VIEWPORT_SIZE;
}

//==========================
// V_KillViewportRefdef
//==========================
void V_KillViewportRefdef(struct ref_params_s *pparams)
{
	pparams->viewport[0] = 0;
	pparams->viewport[1] = 0;
	pparams->viewport[2] = 1;
	pparams->viewport[3] = 1;
}

//==========================
// V_ResetViewportRefdef
//==========================
void V_ResetViewportRefdef (struct ref_params_s *pparams)
{
	pparams->viewport[0] = 0;
	pparams->viewport[1] = 0;
	pparams->viewport[2] = ScreenWidth;
	pparams->viewport[3] = ScreenHeight;
}

//==========================
// V_GetChaseOrigin
//==========================
void V_GetChaseOrigin( float * angles, float * origin, float distance, float * returnvec )
{
	vec3_t	vecEnd;
	vec3_t	forward;
	vec3_t	vecStart;
	pmtrace_t * trace;
	int maxLoops = 8;

	int ignoreent = -1;	// first, ignore no entity
	cl_entity_t*	ent = NULL;
	
	// Trace back from the target using the player's view angles
	AngleVectors(angles, forward, NULL, NULL);
	VectorScale(forward,-1,forward);
	VectorCopy( origin, vecStart );
	VectorMA(vecStart, distance , forward, vecEnd);

	while ( maxLoops > 0)
	{
		trace = gEngfuncs.PM_TraceLine( vecStart, vecEnd, PM_TRACELINE_PHYSENTSONLY, 2, ignoreent );
		if ( trace->ent <= 0) break;// we hit the world or nothing, stop trace

		ent = gEngfuncs.GetEntityByIndex( PM_GetPhysEntInfo( trace->ent ) );
		if ( ent == NULL ) break;

		// hit non-player solid BSP , stop here
		if ( ent->curstate.solid == SOLID_BSP && !ent->player ) break;

		// if close enought to end pos, stop, otherwise continue trace
		if( Distance(trace->endpos, vecEnd ) < 1.0f )break;
		else
		{
			ignoreent = trace->ent;	// ignore last hit entity
			VectorCopy( trace->endpos, vecStart);
		}
		maxLoops--;
	}  
	VectorMA( trace->endpos, 4, trace->plane.normal, returnvec );
	v_lastDistance = Distance(trace->endpos, origin);	// real distance without offset
}

//==========================
// V_GetChasePos
//==========================
void V_GetChasePos(cl_entity_t *ent, float *cl_angles, float *origin, float *angles)
{
	if (!ent)
	{
		// just copy a save in-map position
		VectorCopy ( vJumpAngles, angles );
		VectorCopy ( vJumpOrigin, origin );
		return;
	}
	if ( cl_angles == NULL )	// no mouse angles given, use entity angles ( locked mode )
	{
		VectorCopy ( ent->angles, angles);
		angles[0]*=-1;
	}
	else	VectorCopy ( cl_angles, angles);

	VectorCopy ( ent->origin, origin);
	origin[2]+= 28; // DEFAULT_VIEWHEIGHT - some offset
          V_GetChaseOrigin( angles, origin, cl_chasedist->value, origin );
}

//==========================
// V_CalcNextView
//==========================
void V_CalcNextView (struct ref_params_s *pparams)
{
	if (g_FirstFrame)//first time not actually
	{
		g_FirstFrame = false;
                    g_RenderReady = false;
		
		//First time in this function
		GL_PreRender();//this set g_RenderReady at true if all is'ok

		if (!g_RenderReady )//no hardware capable?
		{
			g_bMirrorShouldpass = false;
			g_bScreenShouldpass = false;
			g_bPortalShouldpass = false;
			g_bSkyShouldpass	= (gHUD.m_iSkyMode ? true : false);//sky must drawing in D3D
		}
		else //all is'ok
		{
			g_bMirrorShouldpass = (g_iTotalVisibleMirrors > 0 && r_mirrors->value);
			g_bScreenShouldpass = (g_iTotalVisibleScreens > 0 && r_screens->value);
			g_bPortalShouldpass = (g_iTotalVisiblePortals > 0 && r_portals->value);
			g_bSkyShouldpass	= (gHUD.m_iSkyMode ? true : false);
		}
		m_RenderRefCount	= 0;//reset debug counter
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
void V_CalcCameraRefdef (struct ref_params_s *pparams)
{
	if(pparams->intermission) return;//disable in intermission mode

	if (gHUD.viewFlags & CAMERA_ON )
	{         
		//get viewentity and monster eyeposition
		cl_entity_t *viewentity = UTIL_GetClientEntityWithServerIndex( gHUD.viewEntityIndex );
	 	if (viewentity)
		{		 
			studiohdr_t *viewmonster = (studiohdr_t *)IEngineStudio.Mod_Extradata (viewentity->model);
			VectorCopy ( pparams->viewangles, v_angles );//copy angles to transform
			VectorCopy ( pparams->vieworg, v_origin );   //copy origin to transform

			if(gHUD.viewFlags & MONSTER_VIEW && viewmonster)//calc monster view
			{
				VectorAdd(viewentity->origin, viewmonster->eyeposition, v_origin);
			}
			else
			{
				VectorCopy(viewentity->origin, v_origin);
			}
			VectorCopy(viewentity->angles, v_angles);

			if (gHUD.viewFlags & INVERSE_X)//inverse X coordinate
				v_angles[0] = -v_angles[0];
			pparams->crosshairangle[PITCH] = 100; //hide crosshair

			//refresh position
			VectorCopy ( v_angles, pparams->viewangles )
			VectorCopy ( v_origin, pparams->vieworg );
		}
	}
	else pparams->crosshairangle[PITCH] = 0;//show crosshair again
}

cl_entity_t *V_FindIntermisionSpot( struct ref_params_s *pparams )
{
	cl_entity_t *ent;
	int spotindex[9];//max number of intermission spot
	int k = 0, j = 0;

	//found intermission point
	for( int i = 0; i < pparams->max_entities; i++ )
	{
		ent = UTIL_GetClientEntityWithServerIndex( i );
		if(ent && ent->curstate.eflags & FL_INTERMISSION)
		{
			spotindex[j] = ent->index;//save entindex
			j++;
			if(j > 8) break;//spotlist is full
		}
	}	
	
	//ok, we have list of intermission spots
	if( j )
	{
		if(j > 1) k = gEngfuncs.pfnRandomLong(0, j-1);
		ent = UTIL_GetClientEntityWithServerIndex( spotindex[k] );
	}
	else ent = gEngfuncs.GetLocalPlayer();//just get view from player
	
	return ent;
}

//==========================
// V_CalcIntermisionRefdef
//==========================
void V_CalcIntermisionRefdef( struct ref_params_s *pparams )
{
	if(!pparams->intermission) return;//intermission mode
	cl_entity_t *view;

	float	  old;

          if(!spot)spot = V_FindIntermisionSpot( pparams );
	view = gEngfuncs.GetViewModel();

	VectorCopy ( spot->origin, pparams->vieworg );
	VectorCopy ( spot->angles, pparams->viewangles );
	
	view->model = NULL;

	// allways idle in intermission
	old = v_idlescale;
	v_idlescale = 1;
	V_AddIdle ( pparams );

	v_idlescale = old;
	v_cl_angles = pparams->cl_viewangles;
	v_origin = pparams->vieworg;
	v_angles = pparams->viewangles;
}

//==========================
// V_PrintDebugInfo
//==========================
void V_PrintDebugInfo (struct ref_params_s *pparams) //for future extensions
{
	if (!r_debug->value)return; //show OpenGL renderer debug info
	Msg("Xash Renderer Info: ");
	if(m_RenderRefCount > 1)Msg("Total %d passes\n", m_RenderRefCount);
	else Msg("Use normal view, make one pass\n");
	if(g_iTotalMirrors) Msg("Visible mirrors: %d from %d\n",g_iTotalVisibleMirrors, g_iTotalMirrors);
	if(g_iTotalScreens) Msg("Visible screens: %d from %d\n",g_iTotalVisibleScreens, g_iTotalScreens);
	if(g_iTotalPortals) Msg("Visible portals: %d from %d\n",g_iTotalVisiblePortals, g_iTotalPortals);
}

//==========================
// V_CalcFinalPass
//==========================
void V_CalcFinalPass ( struct ref_params_s *pparams )
{
	g_FirstFrame = true;//enable calc next passes
	g_StudioRenderer.g_bLocalPlayerDrawn = true;//hide thirdperson

	V_ResetViewportRefdef(pparams);//reset view port as default
	m_RenderRefCount++;//increase counter
}

//==========================
// V_CalcThirdPersonRefdef
//==========================
void V_CalcThirdPersonRefdef( struct ref_params_s * pparams )
{
	//passed only in third person
	if(gHUD.m_iCameraMode == 0 || pparams->intermission) return;

	// get old values
	VectorCopy ( pparams->cl_viewangles, v_cl_angles );
	VectorCopy ( pparams->viewangles, v_angles );
	VectorCopy ( pparams->vieworg, v_origin );

	V_GetChasePos(gEngfuncs.GetLocalPlayer(), v_cl_angles, v_origin, v_angles );
	// write back new values into pparams
	VectorCopy ( v_cl_angles, pparams->cl_viewangles );
	VectorCopy ( v_angles, pparams->viewangles )
	VectorCopy ( v_origin, pparams->vieworg );
}

//==========================
// V_CalcSendOrigin
//==========================
void V_CalcSendOrigin ( struct ref_params_s *pparams )
{
	// never let view origin sit exactly on a node line, because a water plane can
	// dissapear when viewed with the eye exactly on it.
	// FIXME, we send origin at 1/128 now, change this?
	// the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis

	pparams->vieworg[0] += 1.0/32;
	pparams->vieworg[1] += 1.0/32;
	pparams->vieworg[2] += 1.0/32;
}

//==========================
// V_CalcWaterLevel
//==========================
float V_CalcWaterLevel ( struct ref_params_s *pparams )
{
	cl_entity_t *pwater;
	float waterOffset = 0;
	
	if ( pparams->waterlevel >= 2 )
	{
		int i, contents, waterDist, waterEntity;
		vec3_t point;
		waterDist = cl_waterdist->value;

		if ( pparams->hardware )
		{
			waterEntity = gEngfuncs.PM_WaterEntity( pparams->simorg );
			if ( waterEntity >= 0 && waterEntity < pparams->max_entities )
			{
				pwater = gEngfuncs.GetEntityByIndex( waterEntity );
				if (pwater && (pwater->model != NULL)) waterDist += ( pwater->curstate.scale * 16 );
			}
		}
		else waterEntity = 0;
		VectorCopy( pparams->vieworg, point );

		// Eyes are above water, make sure we're above the waves
		if ( pparams->waterlevel == 2 )	
		{
			point[2] -= waterDist;
			for ( i = 0; i < waterDist; i++ )
			{
				contents = gEngfuncs.PM_PointContents( point, NULL );
				if ( contents > CONTENTS_WATER )break;
				point[2] += 1;
			}
			waterOffset = (point[2] + waterDist) - pparams->vieworg[2];
		}
		else
		{
			// eyes are under water.  Make sure we're far enough under
			point[2] += waterDist;
			for ( i = 0; i < waterDist; i++ )
			{
				contents = gEngfuncs.PM_PointContents( point, NULL );
				if ( contents <= CONTENTS_WATER )break;
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
void V_CalcScrOffset( struct ref_params_s *pparams )
{
	// don't allow cheats in multiplayer
	if ( pparams->maxclients <= 1 )
	{
		for (int i=0 ; i<3; i++ )
		{
			pparams->vieworg[i] += scr_ofsx->value*pparams->forward[i] + scr_ofsy->value*pparams->right[i] + scr_ofsz->value*pparams->up[i];
		}
	}
}

//==========================
// V_CalcSmoothAngles
//==========================
void V_CalcSmoothAngles( struct ref_params_s *pparams )
{
	cl_entity_t *view;

	// view is the weapon model (only visible from inside body )
	view = gEngfuncs.GetViewModel();

	if (cl_vsmoothing && cl_vsmoothing->value && ( pparams->smoothing && ( pparams->maxclients > 1 )))
	{
		int foundidx;
		int i;
		float t;

		if ( cl_vsmoothing->value < 0.0 ) gEngfuncs.Cvar_SetValue( "cl_vsmoothing", 0.0 );
		t = pparams->time - cl_vsmoothing->value;
		for ( i = 1; i < ORIGIN_MASK; i++ )
		{
			foundidx = ViewInterp.CurrentOrigin - 1 - i;
			if ( ViewInterp.OriginTime[ foundidx & ORIGIN_MASK ] <= t )	break;
		}

		if ( i < ORIGIN_MASK &&  ViewInterp.OriginTime[ foundidx & ORIGIN_MASK ] != 0.0 )
		{
			// Interpolate
			vec3_t delta;
			double frac;
			double dt;
			vec3_t neworg;

			dt = ViewInterp.OriginTime[ (foundidx + 1) & ORIGIN_MASK ] - ViewInterp.OriginTime[ foundidx & ORIGIN_MASK ];
			if ( dt > 0.0 )
			{
				frac = ( t - ViewInterp.OriginTime[ foundidx & ORIGIN_MASK] ) / dt;
				frac = min( 1.0, frac );
				VectorSubtract( ViewInterp.Origins[ ( foundidx + 1 ) & ORIGIN_MASK ], ViewInterp.Origins[ foundidx & ORIGIN_MASK ], delta );
				VectorMA( ViewInterp.Origins[ foundidx & ORIGIN_MASK ], frac, delta, neworg );

				// Dont interpolate large changes
				if ( Length( delta ) < 64 )
				{
					VectorSubtract( neworg, pparams->simorg, delta );
					VectorAdd( pparams->simorg, delta, pparams->simorg );
					VectorAdd( pparams->vieworg, delta, pparams->vieworg );
					VectorAdd( view->origin, delta, view->origin );

				}
			}
		}
	}
}

//==========================
// V_CalcFirstPersonRefdef
//==========================
void V_CalcFirstPersonRefdef( struct ref_params_s *pparams )
{
	//don't pass in thirdperson or intermission
	if(gHUD.m_iCameraMode || pparams->intermission) return;

	int i;
	vec3_t angles;
	float bob, waterOffset;
	static float lasttime;
	cl_entity_t *view = gEngfuncs.GetViewModel();

	if(g_bFinalPass)V_DriftPitch( pparams );//calculate this only in final pass
	bob = V_CalcBob( pparams );

	// refresh position
	VectorCopy ( pparams->simorg, pparams->vieworg );
	pparams->vieworg[2] += ( bob );
	VectorAdd( pparams->vieworg, pparams->viewheight, pparams->vieworg );
	VectorCopy ( pparams->cl_viewangles, pparams->viewangles );

	gEngfuncs.V_CalcShake();
	gEngfuncs.V_ApplyShake( pparams->vieworg, pparams->viewangles, 1.0 );

	V_CalcSendOrigin( pparams );
	waterOffset = V_CalcWaterLevel( pparams );
	V_CalcViewRoll( pparams );
	V_AddIdle( pparams );

	// offsets
	VectorCopy( pparams->cl_viewangles, angles );
	AngleVectors ( angles, pparams->forward, pparams->right, pparams->up );
	V_CalcScrOffset( pparams );

	VectorCopy ( pparams->cl_viewangles, view->angles );
	V_CalcGunAngle ( pparams );

	// Use predicted origin as view origin.
	VectorCopy ( pparams->simorg, view->origin );      
	view->origin[2] += ( waterOffset );
	VectorAdd( view->origin, pparams->viewheight, view->origin );

	// Let the viewmodel shake at about 10% of the amplitude
	gEngfuncs.V_ApplyShake( view->origin, view->angles, 0.9 );

	for ( i = 0; i < 3; i++)view->origin[ i ] += bob * 0.4 * pparams->forward[ i ];
	view->origin[2] += bob;

	view->angles[YAW]   -= bob * 0.5;
	view->angles[ROLL]  -= bob * 1;
	view->angles[PITCH] -= bob * 0.3;
	view->origin[2] -= 1;

	if(!g_bFinalPass)VectorInverse(pparams->punchangle); //make inverse for mirror
		
	VectorAdd ( pparams->viewangles, pparams->punchangle, pparams->viewangles );
	VectorAdd ( pparams->viewangles, (float *)&ev_punchangle, pparams->viewangles);
	V_DropPunchAngle ( pparams->frametime, (float *)&ev_punchangle );

	static float oldz = 0;

	if ( !pparams->smoothing && pparams->onground && pparams->simorg[2] - oldz > 0)
	{
		float steptime;
		
		steptime = pparams->time - lasttime;
		if (steptime < 0)steptime = 0;

		oldz += steptime * 150;
		if (oldz > pparams->simorg[2])
			oldz = pparams->simorg[2];
		if (pparams->simorg[2] - oldz > 18)
			oldz = pparams->simorg[2]- 18;
		pparams->vieworg[2] += oldz - pparams->simorg[2];
		view->origin[2] += oldz - pparams->simorg[2];
	}
	else	oldz = pparams->simorg[2];

	static float lastorg[3];
	vec3_t delta;

	VectorSubtract( pparams->simorg, lastorg, delta );

	if ( Length( delta ) != 0.0 )
	{
		VectorCopy( pparams->simorg, ViewInterp.Origins[ ViewInterp.CurrentOrigin & ORIGIN_MASK ] );
		ViewInterp.OriginTime[ ViewInterp.CurrentOrigin & ORIGIN_MASK ] = pparams->time;
		ViewInterp.CurrentOrigin++;

		VectorCopy( pparams->simorg, lastorg );
	}
	V_CalcSmoothAngles( pparams );//smooth angles in multiplayer

	lasttime = pparams->time;
	v_origin = pparams->vieworg;	
}

//==========================
// V_CalcMainRefdef
//==========================
void V_CalcMainRefdef (struct ref_params_s *pparams)
{
	if(g_FirstFrame)g_bFinalPass = true;
	V_CalcFirstPersonRefdef( pparams );
	V_CalcThirdPersonRefdef( pparams );
	V_CalcIntermisionRefdef( pparams );
	V_CalcCameraRefdef( pparams );
	VectorCopy(pparams->vieworg, g_vecBaseOrigin);
	VectorCopy(pparams->viewangles, g_vecBaseAngles);
}

//==========================
// V_CalcSkyRefdef
//==========================
bool V_CalcSkyRefdef(struct ref_params_s *pparams)
{
	if (g_bSkyShouldpass)
	{
		g_bSkyShouldpass = false;
		V_CalcMainRefdef( pparams );//refresh position
		VectorCopy(gHUD.m_vecSkyPos, pparams->vieworg);
		V_ResetViewportRefdef(pparams);
		pparams->nextView = 1;
		g_bSkyPass = true;
		m_RenderRefCount++;
		return true;//new pass
	}
	if(g_bSkyPass)
	{
		pparams->nextView = 0;//reset refcounter
		g_bSkyPass = false;	
		return false;
	}
	return false;//no drawing
}

//==========================
// V_CalcMirrorsRefdef
//==========================
bool V_CalcMirrorsRefdef (struct ref_params_s *pparams)
{
	if (g_bMirrorShouldpass)
	{
		if (pparams->nextView == 0)
		{
			g_bMirrorPass = true;
			//This is first pass rendering (setup mirror viewport and nextView's)
                              V_SetViewportRefdef(pparams);
			glEnable(GL_CLIP_PLANE0);//enable clip plane once in first mirror!
			g_StudioRenderer.g_bLocalPlayerDrawn = false;
			m_pCurrentMirror = NULL;
                              V_CalcMainRefdef( pparams );
			R_SetupNewMirror(pparams);
			R_SetupMirrorRenderPass(pparams);
			pparams->nextView = g_iTotalVisibleMirrors;

			m_RenderRefCount++;
			return true;//end of pass
		}
		else if (pparams->nextView == 1)
		{
			R_NewMirrorRenderPass();	//capture view to texture
			glDisable(GL_CLIP_PLANE0);
			m_pCurrentMirror = NULL;

			//restore for final pass
			glViewport(0,0,ScreenWidth,ScreenHeight);
			pparams->nextView = 0;

			g_bMirrorPass = false;
			g_bMirrorShouldpass = false;
			return false; //false nextview
		}
		else
		{
			R_NewMirrorRenderPass();	    //capture view to texture
			R_SetupNewMirror(pparams);
			R_SetupMirrorRenderPass(pparams); //setup origin and angles
			pparams->nextView--;
			g_StudioRenderer.g_bLocalPlayerDrawn = false;

			m_RenderRefCount++;
			return true;//end of pass
		}
	}
	return false;//no drawing
}

//==========================
// V_CalcScreensRefdef
//==========================
bool V_CalcScreensRefdef (struct ref_params_s *pparams)
{
	if (g_bScreenShouldpass)
	{
		if (pparams->nextView == 0)
		{
			//This is first pass rendering (setup screen viewport and nextView's)
			g_bScreenPass = true;
			V_SetViewportRefdef(pparams);
			
			g_StudioRenderer.g_bLocalPlayerDrawn = false;
			m_pCurrentScreen = NULL;
			R_SetupNewScreen(pparams);
			R_SetupScreenRenderPass(pparams);
			pparams->nextView = g_iTotalVisibleScreens;
			m_RenderRefCount++;
			return true;//end of pass
		}
		else if (pparams->nextView == 1)
		{
			R_NewScreenRenderPass();	//capture view to texture
			m_pCurrentScreen = NULL;
                              
			//restore for final pass
			V_ResetViewportRefdef(pparams);
			glViewport(0,0,ScreenWidth,ScreenHeight);
			pparams->nextView = 0;
			g_bScreenShouldpass = false;
			return false; //false nextview
		}
		else
		{
			R_NewScreenRenderPass();	    //capture view to texture
			R_SetupNewScreen(pparams);
			R_SetupScreenRenderPass(pparams); //setup origin and angles
			pparams->nextView--;
			g_StudioRenderer.g_bLocalPlayerDrawn = false;

			m_RenderRefCount++;
			return true;//end of pass
		}
	}
	return false; //false nextview
}

//==========================
// V_CalcPortalsRefdef
//==========================
bool V_CalcPortalsRefdef (struct ref_params_s *pparams)
{
	if (g_bPortalShouldpass)
	{
		if (pparams->nextView == 0)
		{
			//This is first pass rendering (setup mirror viewport and nextView's)
			g_bPortalPass = true;
                              V_SetViewportRefdef(pparams);

			g_StudioRenderer.g_bLocalPlayerDrawn = false;
			m_pCurrentPortal = NULL;
                              V_CalcMainRefdef( pparams );
			R_SetupNewPortal(pparams);
			R_SetupPortalRenderPass(pparams);
			pparams->nextView = g_iTotalVisiblePortals;
			m_RenderRefCount++;
			return true;//end of pass
		}
		else if (pparams->nextView == 1)
		{
			R_NewPortalRenderPass();	//capture view to texture
			m_pCurrentPortal = NULL;
                              
			//restore for final pass
			V_ResetViewportRefdef(pparams);
			glViewport(0,0,ScreenWidth,ScreenHeight);
			pparams->nextView = 0;
			g_bPortalShouldpass = false;
			return false; //false nextview
		}
		else
		{
			R_NewPortalRenderPass();	    //capture view to texture
			R_SetupNewPortal(pparams);
			R_SetupPortalRenderPass(pparams); //setup origin and angles
			pparams->nextView--;
			g_StudioRenderer.g_bLocalPlayerDrawn = false;

			m_RenderRefCount++;
			return true;//end of pass
		}
	}
	return false;//no drawing
}

//==========================
// V_CalcHardwareRefdef
//==========================
void V_CalcHardwareRefdef( struct ref_params_s *pparams )
{
	//check for renderer
	if(!pparams->hardware) return;

	V_CalcNextView( pparams );//calculate passes
	
	if(V_CalcSkyRefdef(pparams)) return;
	if(V_CalcScreensRefdef(pparams)) return;
	if(V_CalcPortalsRefdef(pparams)) return;
	if(V_CalcMirrorsRefdef(pparams)) return;
	
	V_CalcGlobalFog( pparams );
          V_CalcFinalPass ( pparams );
	V_CalcMainRefdef( pparams );
	
	V_PrintDebugInfo( pparams );
}

//==========================
// V_CalcSoftwareRefdef
//==========================
void V_CalcSoftwareRefdef( struct ref_params_s *pparams )
{         
	//check for renderer
	if(pparams->hardware) return;
	
	V_CalcNextView( pparams );

	V_CalcFinalPass ( pparams );
	V_CalcMainRefdef( pparams );

	V_PrintDebugInfo( pparams );	
}

//==========================
// V_CalcRefdef
//==========================
void DLLEXPORT V_CalcRefdef( struct ref_params_s *pparams )
{
	pause = pparams->paused;
	if ( pause ) return;

	V_CalcHardwareRefdef( pparams );
	V_CalcSoftwareRefdef( pparams );
}