//=======================================================================
//			Copyright XashXT Group 2009 ©
//		      ref_params.h - client rendering state
//=======================================================================
#ifndef REF_PARAMS_H
#define REF_PARAMS_H

typedef struct ref_params_s
{
	// output
	int		viewport[4];	// x, y, width, height
	vec3_t		vieworg;
	vec3_t		viewangles;
	float		fov_x;
	float		fov_y;		// fov_y = V_CalcFov( fov_x, viewport[2], viewport[3] );

	vec3_t		forward;
	vec3_t		right;
	vec3_t		up;
	
	float		frametime;	// client frametime
	float		lerpfrac;		// between oldframe and frame
	float		time;		// client time
	float		oldtime;		// studio lerping

	// misc
	BOOL		intermission;
	BOOL		demoplayback;
	BOOL		demorecord;
	BOOL		spectator;
	BOOL		paused;
	int		onlyClientDraw;	// 1 - don't draw worldmodel
	edict_t		*onground;	// pointer to onground entity
	byte		*areabits;	// come from server, contains visible areas list
	int		waterlevel;

	// input
	vec3_t		velocity;
	vec3_t		angles;		// input viewangles
	vec3_t		origin;		// origin + viewheight = vieworg
	vec3_t		old_angles;	// prev.state values to interpolate from
	vec3_t		old_origin;

	vec3_t		viewheight;
	float		idealpitch;

	int		health;
	vec3_t		crosshairangle;	// pfnCrosshairAngle values from server
	vec3_t		punchangle;	// recivied from server
	edict_t		*viewentity;
	int		clientnum;
	int		num_entities;
	int		max_entities;
	int		max_clients;
} ref_params_t;

#endif//REF_PARAMS_H