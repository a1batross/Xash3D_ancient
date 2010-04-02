//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     pm_movevars.h - shared movement variables
//=======================================================================
#ifndef PM_MOVEVARS_H
#define PM_MOVEVARS_H

struct movevars_s
{
	float	gravity;		// gravity for map
	float	stopspeed;	// Deceleration when not moving
	float	maxspeed;		// max allowed speed
	float	spectatormaxspeed;	// max spectator allowed speed
	float	accelerate;	// acceleration factor
	float	airaccelerate;	// same for when in open air
	float	wateraccelerate;	// Same for when in water
	float	friction;		// sv_friction
	float	edgefriction;	// Extra friction near dropofs 
	float	waterfriction;	// Less in water
	float	bounce;		// Wall bounce value. 1.0
	float	stepsize;		// sv_stepheight 
	float	maxvelocity;	// maximum server velocity.
	int	footsteps;	// mp_footsteps Play footstep sounds
	float	rollangle;	// client rollangle
	float	rollspeed;	// cleint rollspeed
};

#endif//PM_MOVEVARS_H