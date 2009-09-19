//=======================================================================
//			Copyright XashXT Group 2009 ©
//		      ref_params.h - client rendering state
//=======================================================================
#ifndef REF_PARAMS_H
#define REF_PARAMS_H

typedef struct skyportal_s
{
	float		fov;
	float		scale;
	vec3_t		vieworg;
	vec3_t		viewanglesOffset;
} skyportal_t;

typedef struct ref_params_s
{
	// output
	vec3_t		vieworg;
	vec3_t		viewangles;

	vec3_t		forward;
	vec3_t		right;
	vec3_t		up;
	
	float		frametime;	// client frametime
	float		time;		// client time

	int		intermission;
	int		paused;
	int		spectator;
	edict_t		*onground;	// pointer to onground entity
	int		waterlevel;

	vec3_t		simvel;		// client velocity (came from server)
	vec3_t		simorg;		// client origin (without viewheight)

	vec3_t		viewheight;
	float		idealpitch;

	vec3_t		cl_viewangles;	// predicted angles

	int		health;
	vec3_t		crosshairangle;	// pfnCrosshairAngle values from server
	float		viewsize;

	vec3_t		punchangle;	// receivied from server
	int		maxclients;
	int		viewentity;	// entity that set with svc_setview else localclient
	int		num_entities;	// entities actual count (was int playernum;)
	int		max_entities;
	int		demoplayback;	
	int		movetype;		// client movetype (was int hardware;)
	int		smoothing;	// client movement predicting is running

	usercmd_t		*cmd;		// last issued usercmd
	movevars_t	*movevars;	// sv.movevars

	int		viewport[4];	// x, y, width, height
	int		nextView;		// the renderer calls ClientDLL_CalcRefdef() and Renderview
					// so long in cycles until this value is 0 (multiple views)
	int		flags;		// renderer setup flags (was int onlyClientDraw;)

	// Xash Renderer Specifics
	byte		*areabits;	// come from server, contains visible areas list
					// set it to NULL for disable area visibility
	skyportal_t	skyportal;	// sky protal setup is done in HUD_UpdateEntityVars
	float		blend[4];		// rgba 0-1 full screen blend

	float		fov_x;
	float		fov_y;		// fov_y = V_CalcFov( fov_x, viewport[2], viewport[3] );
} ref_params_t;

#endif//REF_PARAMS_H