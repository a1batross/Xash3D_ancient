//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     cvardef.h - pointer to console variable
//=======================================================================
#ifndef PM_MOVEVARS_H
#define PM_MOVEVARS_H

struct movevars_s
{
	float	gravity;		// gravity for map
	float	maxvelocity;	// maximum server velocity.
	float	rollangle;	// client rollangle
	float	rollspeed;	// cleint rollspeed
	float	maxspeed;		// max allowed speed
	float	stepheight;	// sv_stepheight
	float	accelerate;	// acceleration factor
	float	airaccelerate;	// same for when in open air
	float	friction;		// sv_friction
};

#endif//PM_MOVEVARS_H