//=======================================================================
//			Copyright XashXT Group 2009 ©
//		        usercmd.h - usercmd communication
//=======================================================================
#ifndef USERCMD_H
#define USERCMD_H

// usercmd_t communication (a part of network protocol)
typedef struct usercmd_s
{
	int		msec;		// duration in ms of command
	vec3_t		viewangles;	// command view angles
	float		forwardmove;	// forward velocity
	float		sidemove;		// sideways velocity
	float		upmove;		// upward velocity
	int		lightlevel;	// light level at spot where we are standing.
	int		buttons;		// attack and move buttons
	int		impulse;		// impulse command issued
	int		weaponselect;	// current weapon id
	int		random_seed;	// shared random seed
	int		target_edict;	// mouse captured edict
} usercmd_t;

#endif//USERCMD_H