//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        cm_pmove.c - player movement code
//=======================================================================

#include "cm_local.h"

int		characterID; 
uint		m_jumpTimer;
bool		m_isStopped;
bool		m_isAirBorne;
float		m_maxStepHigh;
float		m_yawAngle;
float		m_maxTranslation;
vec3_t		m_size;
vec3_t		m_stepContact;
matrix4x4		m_matrix;

#define	STEPSIZE	18

// all of the locals will be zeroed before each
// pmove, just to make damn sure we don't have
// any differences when running on client or server

typedef struct
{
	vec3_t		origin;
	vec3_t		velocity;
	vec3_t		previous_origin;
	vec3_t		previous_velocity;
	int		previous_waterlevel;

	vec3_t		movedir;	// already aligned by world
	vec3_t		forward, right, up;
	float		frametime;
	int		msec;

	bool		walking;
	bool		onladder;
	trace_t		groundtrace;
	float		impact_speed;

	csurface_t	*groundsurface;
	cplane_t		groundplane;
	int		groundcontents;


} pml_t;

pmove_t		*pm;
pml_t		pml;


// movement parameters
float	pm_stopspeed = 100;
float	pm_maxspeed = 300;
float	pm_duckspeed = 100;
float	pm_accelerate = 10;
float	pm_airaccelerate = 0;
float	pm_wateraccelerate = 10;
float	pm_friction = 6;
float	pm_waterfriction = 1;
float	pm_waterspeed = 400;

/*

  walking up a step should kill some velocity

*/

/*
===============
PM_AddTouchEnt
===============
*/
void PM_AddTouchEnt( edict_t *entity )
{
	int		i;

	if( pm->numtouch == PM_MAXTOUCH )
		return;

	// see if it is already added
	for( i = 0; i < pm->numtouch; i++ )
	{
		if( pm->touchents[i] == entity )
			return;
	}

	// add it
	pm->touchents[pm->numtouch] = entity;
	pm->numtouch++;
}

/*
==================
PM_ClipVelocity

Slide off of the impacting object
returns the blocked flags (1 = floor, 2 = step / wall)
==================
*/
void PM_ClipVelocity (vec3_t in, vec3_t normal, vec3_t out, float overbounce)
{
	float	backoff;
	float	change;
	int		i;
	
	backoff = DotProduct (in, normal);

	if( backoff < 0 ) backoff *= overbounce;
	else backoff /= overbounce;

	for( i = 0; i < 3; i++ )
	{
		change = normal[i]*backoff;
		out[i] = in[i] - change;
	}
}




/*
==================
PM_StepSlideMove

Each intersection will try to step over the obstruction instead of
sliding along it.

Returns a new origin, velocity, and contact entity
Does not modify any world state?
==================
*/
#define	MIN_STEP_NORMAL	0.7		// can't step up onto very steep slopes
#define	MAX_CLIP_PLANES	5
void PM_StepSlideMove_ (void)
{
	int			bumpcount, numbumps;
	vec3_t		dir;
	float		d;
	int			numplanes;
	vec3_t		planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity;
	int			i, j;
	trace_t	trace;
	vec3_t		end;
	float		time_left;
	
	numbumps = 4;
	
	VectorCopy (pml.velocity, primal_velocity);
	numplanes = 0;
	
	time_left = pml.frametime;

	for( bumpcount = 0; bumpcount < numbumps; bumpcount++ )
	{
		for (i=0 ; i<3 ; i++)
			end[i] = pml.origin[i] + time_left * pml.velocity[i];

		pm->trace( pml.origin, pm->mins, pm->maxs, end, &trace );

		if (trace.allsolid)
		{	// entity is trapped in another solid
			pml.velocity[2] = 0;	// don't build up falling damage
			return;
		}

		if (trace.fraction > 0)
		{	// actually covered some distance
			VectorCopy (trace.endpos, pml.origin);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			 break;		// moved the entire distance

		// save entity for contact
		PM_AddTouchEnt( trace.ent );
		
		time_left -= time_left * trace.fraction;

		// slide along this plane
		if (numplanes >= MAX_CLIP_PLANES)
		{	// this shouldn't really happen
			VectorCopy (vec3_origin, pml.velocity);
			break;
		}

		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		for (i=0 ; i<numplanes ; i++)
		{
			PM_ClipVelocity (pml.velocity, planes[i], pml.velocity, 1.01);
			for (j=0 ; j<numplanes ; j++)
			{
				if (j != i)
				{
					if (DotProduct (pml.velocity, planes[j]) < 0)
						break;	// not ok
				}
			}
			if (j == numplanes) break;
		}
		
		if (i != numplanes)
		{	
			// go along this plane
		}
		else
		{	// go along the crease
			if (numplanes != 2)
			{
				VectorCopy (vec3_origin, pml.velocity);
				break;
			}
			CrossProduct (planes[0], planes[1], dir);
			d = DotProduct (dir, pml.velocity);
			VectorScale (dir, d, pml.velocity);
		}

		// if velocity is against the original velocity, stop dead
		// to avoid tiny occilations in sloping corners
		if (DotProduct (pml.velocity, primal_velocity) <= 0)
		{
			VectorCopy (vec3_origin, pml.velocity);
			break;
		}
	}

	if (pm->ps.pm_time)
	{
		VectorCopy (primal_velocity, pml.velocity);
	}
}

/*
==================
PM_StepSlideMove

==================
*/
void PM_StepSlideMove (void)
{
	vec3_t		start_o, start_v;
	vec3_t		down_o, down_v;
	trace_t		trace;
	float		down_dist, up_dist;
	vec3_t		up, down;

	VectorCopy (pml.origin, start_o);
	VectorCopy (pml.velocity, start_v);

	PM_StepSlideMove_ ();

	VectorCopy (pml.origin, down_o);
	VectorCopy (pml.velocity, down_v);

	VectorCopy (start_o, up);
	up[2] += STEPSIZE;

	pm->trace( up, pm->mins, pm->maxs, up, &trace );
	if( trace.allsolid ) return; // can't step up

	// try sliding above
	VectorCopy (up, pml.origin);
	VectorCopy (start_v, pml.velocity);

	PM_StepSlideMove_ ();

	// push down the final amount
	VectorCopy (pml.origin, down);
	down[2] -= STEPSIZE;
	pm->trace( pml.origin, pm->mins, pm->maxs, down, &trace );
	if (!trace.allsolid)
	{
		VectorCopy (trace.endpos, pml.origin);
	}

	VectorCopy(pml.origin, up);

	// decide which one went farther
	down_dist = (down_o[0] - start_o[0])*(down_o[0] - start_o[0]) + (down_o[1] - start_o[1])*(down_o[1] - start_o[1]);
	up_dist = (up[0] - start_o[0])*(up[0] - start_o[0]) + (up[1] - start_o[1])*(up[1] - start_o[1]);

	if (down_dist > up_dist || trace.plane.normal[2] < MIN_STEP_NORMAL)
	{
		VectorCopy (down_o, pml.origin);
		VectorCopy (down_v, pml.velocity);
		return;
	}

	// Special case
	// if we were walking along a plane, then we need to copy the Z over
	pml.velocity[2] = down_v[2];
}


/*
==================
PM_Friction

Handles both ground friction and water friction
==================
*/

void PM_Friction( void )
{
	float	*vel;
	float	speed, newspeed, control;
	float	drop;
	
	vel = pml.velocity;
	
	speed = sqrt(vel[0]*vel[0] +vel[1]*vel[1] + vel[2]*vel[2]);
	if (speed < 1)
	{
		vel[0] = 0;
		vel[1] = 0;
		return;
	}

	drop = 0;

	// apply ground friction
	if((pm->ps.groundentity && pml.groundsurface && !(pml.groundsurface->flags & SURF_SLICK)) || (pml.onladder))
	{
		control = speed < pm_stopspeed ? pm_stopspeed : speed;
		drop += control * pm_friction * pml.frametime;
	}

	// apply water friction
	if (pm->waterlevel && !pml.onladder)
		drop += speed*pm_waterfriction*pm->waterlevel*pml.frametime;

	// scale the velocity
	newspeed = speed - drop;
	if( newspeed < 0 ) newspeed = 0;
	newspeed /= speed;

	VectorScale( vel, newspeed, vel );
}

/*
==============
PM_Accelerate

Handles user intended acceleration
==============
*/
void PM_Accelerate (vec3_t wishdir, float wishspeed, float accel)
{
	int	i;
	float	addspeed, accelspeed, currentspeed;

	currentspeed = DotProduct (pml.velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if( addspeed <= 0 ) return;
	accelspeed = accel * pml.frametime * wishspeed;
	if( accelspeed > addspeed ) accelspeed = addspeed;
	
	for( i = 0; i < 3; i++ )
		pml.velocity[i] += accelspeed * wishdir[i];	
}

void PM_AirAccelerate (vec3_t wishdir, float wishspeed, float accel)
{
	int			i;
	float		addspeed, accelspeed, currentspeed, wishspd = wishspeed;
		
	if (wishspd > 30)
		wishspd = 30;
	currentspeed = DotProduct (pml.velocity, wishdir);
	addspeed = wishspd - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = accel * wishspeed * pml.frametime;
	if (accelspeed > addspeed)
		accelspeed = addspeed;
	
	for (i=0 ; i<3 ; i++)
		pml.velocity[i] += accelspeed*wishdir[i];	
}

/*
=============
PM_AddCurrents
=============
*/
void PM_AddCurrents (vec3_t	wishvel)
{
	vec3_t	v;
	float	s;

	// account for onladders
	if (pml.onladder && fabs(pml.velocity[2]) <= 200)
	{
		if ((pm->ps.viewangles[PITCH] <= -15) && (pm->cmd.forwardmove > 0))
			wishvel[2] = 200;
		else if ((pm->ps.viewangles[PITCH] >= 15) && (pm->cmd.forwardmove > 0))
			wishvel[2] = -200;
		else if (pm->cmd.upmove > 0)
			wishvel[2] = 200;
		else if (pm->cmd.upmove < 0)
			wishvel[2] = -200;
		else wishvel[2] = 0;

		// limit horizontal speed when on a onladder
		if (wishvel[0] < -25)
			wishvel[0] = -25;
		else if (wishvel[0] > 25)
			wishvel[0] = 25;

		if (wishvel[1] < -25)
			wishvel[1] = -25;
		else if (wishvel[1] > 25)
			wishvel[1] = 25;
	}


	//
	// add water currents
	//

	if (pm->watertype & MASK_CURRENT)
	{
		VectorClear (v);

		if (pm->watertype & CONTENTS_CURRENT_0)
			v[0] += 1;
		if (pm->watertype & CONTENTS_CURRENT_90)
			v[1] += 1;
		if (pm->watertype & CONTENTS_CURRENT_180)
			v[0] -= 1;
		if (pm->watertype & CONTENTS_CURRENT_270)
			v[1] -= 1;
		if (pm->watertype & CONTENTS_CURRENT_UP)
			v[2] += 1;
		if (pm->watertype & CONTENTS_CURRENT_DOWN)
			v[2] -= 1;

		s = pm_waterspeed;
		if ((pm->waterlevel == 1) && (pm->ps.groundentity))
			s /= 2;

		VectorMA (wishvel, s, v, wishvel);
	}

	//
	// add conveyor belt velocities
	//

	if (pm->ps.groundentity)
	{
		VectorClear (v);

		if (pml.groundcontents & CONTENTS_CURRENT_0)
			v[0] += 1;
		if (pml.groundcontents & CONTENTS_CURRENT_90)
			v[1] += 1;
		if (pml.groundcontents & CONTENTS_CURRENT_180)
			v[0] -= 1;
		if (pml.groundcontents & CONTENTS_CURRENT_270)
			v[1] -= 1;
		if (pml.groundcontents & CONTENTS_CURRENT_UP)
			v[2] += 1;
		if (pml.groundcontents & CONTENTS_CURRENT_DOWN)
			v[2] -= 1;

		VectorMA (wishvel, 100 /* pm->ps.groundentity->speed */, v, wishvel);
	}
}


/*
===================
PM_WaterMove

===================
*/
void PM_WaterMove( void )
{
	int		i;
	vec3_t	wishvel;
	float	wishspeed;
	vec3_t	wishdir;

	// user intentions
	for (i=0 ; i<3 ; i++)
		wishvel[i] = pml.forward[i]*pm->cmd.forwardmove + pml.right[i]*pm->cmd.sidemove;

	if (!pm->cmd.forwardmove && !pm->cmd.sidemove && !pm->cmd.upmove)
		wishvel[2] -= 60;		// drift towards bottom
	else
		wishvel[2] += pm->cmd.upmove;

	PM_AddCurrents (wishvel);

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalizeLength( wishdir );

	if (wishspeed > pm_maxspeed)
	{
		VectorScale (wishvel, pm_maxspeed/wishspeed, wishvel);
		wishspeed = pm_maxspeed;
	}
	wishspeed *= 0.5;

	PM_Accelerate (wishdir, wishspeed, pm_wateraccelerate);
	PM_StepSlideMove ();
}


/*
===================
PM_AirMove

===================
*/
void PM_AirMove (void)
{
	int			i;
	vec3_t		wishvel;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;
	float		maxspeed;

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;
	
	for (i = 0; i < 2; i++)
		wishvel[i] = pml.forward[i]*fmove + pml.right[i]*smove;
	wishvel[2] = 0;

	PM_AddCurrents (wishvel);

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalizeLength( wishdir );

	// clamp to server defined max speed
	maxspeed = (pm->ps.pm_flags & PMF_DUCKED) ? pm_duckspeed : pm_maxspeed;

	if (wishspeed > maxspeed)
	{
		VectorScale (wishvel, maxspeed/wishspeed, wishvel);
		wishspeed = maxspeed;
	}
	
	if ( pml.onladder )
	{
		PM_Accelerate (wishdir, wishspeed, pm_accelerate);
		if (!wishvel[2])
		{
			if (pml.velocity[2] > 0)
			{
				pml.velocity[2] -= pm->ps.gravity * pml.frametime;
				if (pml.velocity[2] < 0)
					pml.velocity[2]  = 0;
			}
			else
			{
				pml.velocity[2] += pm->ps.gravity * pml.frametime;
				if (pml.velocity[2] > 0)
					pml.velocity[2]  = 0;
			}
		}
		PM_StepSlideMove ();
	}
	else if ( pm->ps.groundentity )
	{	// walking on ground
		pml.velocity[2] = 0; //!!! this is before the accel
		PM_Accelerate (wishdir, wishspeed, pm_accelerate);

		if(pm->ps.gravity > 0) pml.velocity[2] = 0;
		else pml.velocity[2] -= pm->ps.gravity * pml.frametime;

		if (!pml.velocity[0] && !pml.velocity[1]) return;
		PM_StepSlideMove ();
	}
	else
	{	// not on ground, so little effect on velocity
		if (pm_airaccelerate) PM_AirAccelerate (wishdir, wishspeed, pm_accelerate);
		else PM_Accelerate (wishdir, wishspeed, 1);

		// add gravity
		pml.velocity[2] -= pm->ps.gravity * pml.frametime;
		PM_StepSlideMove ();
	}
}



/*
=============
PM_CatagorizePosition
=============
*/
void PM_CatagorizePosition( void )
{
	vec3_t		point;
	int	      	cont;
	trace_t		trace;
	int	      	sample1;
	int	      	sample2;

	// if the player hull point one unit down is solid, the player
	// is on ground

	// see if standing on something solid	
	point[0] = pml.origin[0];
	point[1] = pml.origin[1];
	point[2] = pml.origin[2] - 0.25;

	if( pml.velocity[2] > 180 )
	{
		pm->ps.pm_flags &= ~PMF_ON_GROUND;
		pm->ps.groundentity = NULL;
	}
	else
	{
		pm->trace( pml.origin, pm->mins, pm->maxs, point, &trace );
		pml.groundtrace = trace;
		pml.groundplane = trace.plane;
		pml.groundsurface = trace.surface;
		pml.groundcontents = trace.contents;

		if( !trace.ent || (trace.plane.normal[2] < 0.7 && !trace.startsolid ))
		{
			pm->ps.groundentity = NULL;
			pm->ps.pm_flags &= ~PMF_ON_GROUND;
		}
		else
		{
			pm->ps.groundentity = trace.ent;
			// hitting solid ground will end a waterjump
			if (pm->ps.pm_flags & PMF_TIME_WATERJUMP)
			{
				pm->ps.pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
				pm->ps.pm_time = 0;
			}

			if(!(pm->ps.pm_flags & PMF_ON_GROUND) )
			{
				// just hit the ground
				pm->ps.pm_flags |= PMF_ON_GROUND;
				// don't do landing time if we were just going down a slope
				if (pml.velocity[2] < -200)
				{
					pm->ps.pm_flags |= PMF_TIME_LAND;
					// don't allow another jump for a little while
					if (pml.velocity[2] < -400) pm->ps.pm_time = 25;	
					else pm->ps.pm_time = 18;
				}
			}
		}

		if (pm->numtouch < PM_MAXTOUCH && trace.ent)
		{
			pm->touchents[pm->numtouch] = trace.ent;
			pm->numtouch++;
		}
	}

	// get waterlevel, accounting for ducking
	pm->waterlevel = 0;
	pm->watertype = 0;

	sample2 = pm->ps.viewheight - pm->mins[2];
	sample1 = sample2 / 2;

	point[2] = pml.origin[2] + pm->mins[2] + 1;	
	cont = pm->pointcontents (point);

	if (cont & MASK_WATER)
	{
		pm->watertype = cont;
		pm->waterlevel = 1;
		point[2] = pml.origin[2] + pm->mins[2] + sample1;
		cont = pm->pointcontents (point);
		if (cont & MASK_WATER)
		{
			pm->waterlevel = 2;
			point[2] = pml.origin[2] + pm->mins[2] + sample2;
			cont = pm->pointcontents (point);
			if (cont & MASK_WATER) pm->waterlevel = 3;
		}
	}

}


/*
=============
PM_CheckJump
=============
*/
void PM_CheckJump (void)
{
	if (pm->ps.pm_flags & PMF_TIME_LAND)
	{
		// hasn't been long enough since landing to jump again
		return;
	}

	if (pm->cmd.upmove < 10)
	{
		// not holding jump
		pm->ps.pm_flags &= ~PMF_JUMP_HELD;
		return;
	}

	// must wait for jump to be released
	if (pm->ps.pm_flags & PMF_JUMP_HELD) return;
	if (pm->ps.pm_type == PM_DEAD) return;

	if (pm->waterlevel >= 2)
	{
		// swimming, not jumping
		pm->ps.groundentity = NULL;

		if (pml.velocity[2] <= -300) return;

		if (pm->watertype == CONTENTS_WATER)
			pml.velocity[2] = 100;
		else if (pm->watertype == CONTENTS_SLIME)
			pml.velocity[2] = 80;
		else pml.velocity[2] = 50;
		return;
	}

	if (pm->ps.groundentity == NULL)
		return; // in air, so no effect

	pm->ps.pm_flags |= PMF_JUMP_HELD;

	pm->ps.groundentity = NULL;
	pml.velocity[2] += 270;
	if (pml.velocity[2] < 270)
		pml.velocity[2] = 270;
}


/*
=============
PM_CheckSpecialMovement
=============
*/
void PM_CheckSpecialMovement (void)
{
	vec3_t	spot;
	int	cont;
	vec3_t	flatforward;
	trace_t	trace;

	if (pm->ps.pm_time) return;

	pml.onladder = false;

	// check for onladder
	flatforward[0] = pml.forward[0];
	flatforward[1] = pml.forward[1];
	flatforward[2] = 0;
	VectorNormalize( flatforward );

	VectorMA (pml.origin, 1, flatforward, spot);
	pm->trace( pml.origin, pm->mins, pm->maxs, spot, &trace );
	if ((trace.fraction < 1) && (trace.contents & CONTENTS_LADDER))
		pml.onladder = true;

	// check for water jump
	if (pm->waterlevel != 2) return;

	VectorMA (pml.origin, 30, flatforward, spot);
	spot[2] += 4;
	cont = pm->pointcontents (spot);
	if (!(cont & CONTENTS_SOLID)) return;

	spot[2] += 16;
	cont = pm->pointcontents (spot);
	if (cont) return;

	// jump out of water
	VectorScale (flatforward, 50, pml.velocity);
	pml.velocity[2] = 350;

	pm->ps.pm_flags |= PMF_TIME_WATERJUMP;
	pm->ps.pm_time = 255;
}


/*
===============
PM_FlyMove
===============
*/
void PM_FlyMove (bool doclip)
{
	float	speed, drop, friction, control, newspeed;
	float	currentspeed, addspeed, accelspeed;
	int			i;
	vec3_t		wishvel;
	float		fmove, smove;
	vec3_t		wishdir;
	float		wishspeed;
	vec3_t		end;
	trace_t	trace;

	pm->ps.viewheight = 22;

	// friction
	speed = VectorLength (pml.velocity);
	if (speed < 1)
	{
		VectorCopy (vec3_origin, pml.velocity);
	}
	else
	{
		drop = 0;

		friction = pm_friction*1.5;	// extra friction
		control = speed < pm_stopspeed ? pm_stopspeed : speed;
		drop += control*friction*pml.frametime;

		// scale the velocity
		newspeed = speed - drop;
		if (newspeed < 0)
			newspeed = 0;
		newspeed /= speed;

		VectorScale (pml.velocity, newspeed, pml.velocity);
	}

	// accelerate
	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;
	
	VectorNormalize (pml.forward);
	VectorNormalize (pml.right);

	for (i=0 ; i<3 ; i++)
		wishvel[i] = pml.forward[i]*fmove + pml.right[i]*smove;
	wishvel[2] += pm->cmd.upmove;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalizeLength( wishdir );

	// clamp to server defined max speed
	if( wishspeed > pm_maxspeed )
	{
		VectorScale (wishvel, pm_maxspeed/wishspeed, wishvel);
		wishspeed = pm_maxspeed;
	}


	currentspeed = DotProduct(pml.velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0) return;
	accelspeed = pm_accelerate*pml.frametime*wishspeed;
	if (accelspeed > addspeed) accelspeed = addspeed;
	
	for (i = 0; i < 3; i++) pml.velocity[i] += accelspeed*wishdir[i];	

	if (doclip)
	{
		for (i=0 ; i<3 ; i++) end[i] = pml.origin[i] + pml.frametime * pml.velocity[i];
		pm->trace( pml.origin, pm->mins, pm->maxs, end, &trace );
		VectorCopy (trace.endpos, pml.origin);
	}
	else
	{
		// move
		VectorMA (pml.origin, pml.frametime, pml.velocity, pml.origin);
	}
}


/*
==============
PM_CheckDuck

Sets mins, maxs, and pm->ps.viewheight
==============
*/
void PM_CheckDuck (void)
{
	trace_t	trace;

	pm->mins[0] = -16;
	pm->mins[1] = -16;

	pm->maxs[0] = 16;
	pm->maxs[1] = 16;

	if (pm->ps.pm_type == PM_GIB)
	{
		pm->mins[2] = 0;
		pm->maxs[2] = 16;
		pm->ps.viewheight = 8;
		return;
	}

	pm->mins[2] = -24;

	if (pm->ps.pm_type == PM_DEAD)
	{
		pm->ps.pm_flags |= PMF_DUCKED;
	}
	else if (pm->cmd.upmove < 0 && (pm->ps.pm_flags & PMF_ON_GROUND))
	{
		// duck
		pm->ps.pm_flags |= PMF_DUCKED;
	}
	else
	{
		// stand up if possible
		if( pm->ps.pm_flags & PMF_DUCKED )
		{
			// try to stand up
			pm->maxs[2] = 32;
			pm->trace( pml.origin, pm->mins, pm->maxs, pml.origin, &trace );
			if(!trace.allsolid) pm->ps.pm_flags &= ~PMF_DUCKED;
		}
	}

	if (pm->ps.pm_flags & PMF_DUCKED)
	{
		pm->maxs[2] = 4;
		pm->ps.viewheight = -2;
	}
	else
	{
		pm->maxs[2] = 32;
		pm->ps.viewheight = 22;
	}
}


/*
==============
PM_DeadMove
==============
*/
void PM_DeadMove (void)
{
	float	forward;

	if (!pm->ps.groundentity) return;

	// extra friction
	forward = VectorLength (pml.velocity);
	forward -= 20;
	if (forward <= 0)
	{
		VectorClear (pml.velocity);
	}
	else
	{
		VectorNormalize (pml.velocity);
		VectorScale (pml.velocity, forward, pml.velocity);
	}
}


bool PM_GoodPosition (void)
{
	trace_t	trace;
	vec3_t	origin, end;
	int		i;

	if (pm->ps.pm_type == PM_SPECTATOR) return true;

	for (i = 0; i < 3; i++) origin[i] = end[i] = pm->ps.origin[i] * CL_COORD_FRAC;
	pm->trace( origin, pm->mins, pm->maxs, end, &trace );
	pml.groundtrace = trace;

	return !trace.allsolid;
}

/*
================
PM_SnapPosition

On exit, the origin will have a value that is pre-quantized to the 0.125
precision of the network channel and in a valid position.
================
*/
void PM_SnapPosition (void)
{
	int		sign[3];
	int		i, j, bits;
	short		base[3];
	// try all single bits first
	static int jitterbits[8] = {0,4,1,2,3,5,6,7};

	// snap velocity to eigths
	for (i = 0; i < 3; i++) pm->ps.velocity[i] = (int)(pml.velocity[i]*SV_COORD_FRAC);

	for (i = 0; i < 3; i++)
	{
		if (pml.origin[i] >= 0) sign[i] = 1;
		else sign[i] = -1;
		pm->ps.origin[i] = (int)(pml.origin[i]*SV_COORD_FRAC);
		if (pm->ps.origin[i]*CL_COORD_FRAC == pml.origin[i]) sign[i] = 0;
	}
	VectorCopy (pm->ps.origin, base);

	// try all combinations
	for (j = 0; j < 8; j++)
	{
		bits = jitterbits[j];
		VectorCopy (base, pm->ps.origin);
		for (i=0 ; i<3 ; i++)
		{
			if (bits & (1<<i) ) pm->ps.origin[i] += sign[i];
		}
		if (PM_GoodPosition()) return;
	}

	// go back to the last position
	VectorCopy (pml.previous_origin, pm->ps.origin);
}

/*
================
PM_InitialSnapPosition

================
*/
void PM_InitialSnapPosition(void)
{
	int        x, y, z;
	short      base[3];
	static int offset[3] = { 0, -1, 1 };

	VectorCopy (pm->ps.origin, base);

	for ( z = 0; z < 3; z++ )
	{
		pm->ps.origin[2] = base[2] + offset[ z ];
		for ( y = 0; y < 3; y++ )
		{
			pm->ps.origin[1] = base[1] + offset[ y ];
			for ( x = 0; x < 3; x++ )
			{
				pm->ps.origin[0] = base[0] + offset[ x ];
				if (PM_GoodPosition ())
				{
					VectorScale(pm->ps.origin, CL_COORD_FRAC, pml.origin);
					VectorCopy(pm->ps.origin, pml.previous_origin);
					return;
				}
			}
		}
	}
	MsgDev( D_WARN, "PM_InitialSnapPosition: bad position\n");
}

/*
================
PM_UpdateViewAngles

This can be used as another entry point when only the viewangles
are being updated isntead of a full move
================
*/
void PM_UpdateViewAngles( player_state_t *ps, const usercmd_t *cmd )
{
	short		temp;
	int		i;

	if( ps->pm_type == PM_INTERMISSION )
	{
		return;		// no view changes at all
	}
	if( ps->pm_type == PM_DEAD )
	{
		return;		// no view changes at all
	}
	if( pm->ps.pm_flags & PMF_TIME_TELEPORT)
	{
		ps->viewangles[YAW] = SHORT2ANGLE( pm->cmd.angles[YAW] - pm->ps.delta_angles[YAW] );
		ps->viewangles[PITCH] = 0;
		ps->viewangles[ROLL] = 0;
	}

	// circularly clamp the angles with deltas
	for( i = 0; i < 3; i++ )
	{
		temp = pm->cmd.angles[i] + pm->ps.delta_angles[i];
		ps->viewangles[i] = SHORT2ANGLE(temp);
	}

	// don't let the player look up or down more than 90 degrees
	if( ps->viewangles[PITCH] > 89 && ps->viewangles[PITCH] < 180 )
		ps->viewangles[PITCH] = 89;
	else if( ps->viewangles[PITCH] < 271 && ps->viewangles[PITCH] >= 180 )
		ps->viewangles[PITCH] = 271;
}

/*
================
Pmove

Can be called by either the server or the client
================
*/
void Quake_PMove( pmove_t *pmove )
{
	pm = pmove;

	// clear results
	pm->numtouch = 0;
	VectorClear (pm->ps.viewangles);
	pm->ps.viewheight = 0;
	pm->ps.groundentity = 0;
	pm->watertype = 0;
	pm->waterlevel = 0;

	// clear all pmove local vars
	memset (&pml, 0, sizeof(pml));

	// convert origin and velocity to float values
	VectorScale(pm->ps.origin, CL_COORD_FRAC, pml.origin );
	VectorScale(pm->ps.velocity, CL_COORD_FRAC, pml.velocity);  
	VectorCopy (pm->ps.origin, pml.previous_origin); // save old org in case we get stuck
	pml.frametime = pm->cmd.msec * 0.001;

	// update the viewangles
	PM_UpdateViewAngles( &pm->ps, &pm->cmd );
	AngleVectors( pm->ps.viewangles, pml.forward, pml.right, pml.up );

	if (pm->ps.pm_type == PM_SPECTATOR)
	{
		PM_FlyMove (false);
		PM_SnapPosition ();
		return;
	}

	if (pm->ps.pm_type >= PM_DEAD)
	{
		pm->cmd.forwardmove = 0;
		pm->cmd.sidemove = 0;
		pm->cmd.upmove = 0;
	}

	if (pm->ps.pm_type == PM_FREEZE) return;	// no movement at all

	// set mins, maxs, and viewheight
	PM_CheckDuck ();

	//if (pm->snapinitial) PM_InitialSnapPosition ();

	// set groundentity, watertype, and waterlevel
	PM_CatagorizePosition ();

	if (pm->ps.pm_type == PM_DEAD) PM_DeadMove ();

	PM_CheckSpecialMovement ();

	// drop timing counter
	if (pm->ps.pm_time)
	{
		int		msec;

		msec = pm->cmd.msec >> 3;
		if (!msec) msec = 1;

		if ( msec >= pm->ps.pm_time) 
		{
			pm->ps.pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
			pm->ps.pm_time = 0;
		}
		else pm->ps.pm_time -= msec;
	}

	if (pm->ps.pm_flags & PMF_TIME_TELEPORT)
	{
		// teleport pause stays exactly in place
	}
	else if (pm->ps.pm_flags & PMF_TIME_WATERJUMP)
	{	
		// waterjump has no control, but falls
		pml.velocity[2] -= pm->ps.gravity * pml.frametime;
		if (pml.velocity[2] < 0)
		{	
			// cancel as soon as we are falling down again
			pm->ps.pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
			pm->ps.pm_time = 0;
		}

		PM_StepSlideMove ();
	}
	else
	{
		PM_CheckJump ();
		PM_Friction ();

		if (pm->waterlevel >= 2) PM_WaterMove ();
		else
		{
			vec3_t	angles;

			VectorCopy(pm->ps.viewangles, angles);
			if (angles[PITCH] > 180)
				angles[PITCH] = angles[PITCH] - 360;
			angles[PITCH] /= 3;

			AngleVectors (angles, pml.forward, pml.right, pml.up);
			PM_AirMove ();
		}
	}

	// set groundentity, watertype, and waterlevel for final spot
	PM_CatagorizePosition ();
	PM_SnapPosition ();
}


void CM_CmdUpdateForce( void )
{
	float	fmove, smove;
	int	i;

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;

	// project moves down to flat plane
	pml.forward[2] = pml.right[2] = 0;
	VectorNormalize( pml.forward );
	VectorNormalize( pml.right );

	for( i = 0; i < 3; i++ )
		pml.movedir[i] = pml.forward[i] * fmove + pml.right[i] * smove;

	ConvertDirectionToPhysic( pml.movedir );

	if( pm->cmd.upmove > 0.0f )
	{
		m_jumpTimer = 4;
	}
}

void CM_ServerMove( pmove_t *pmove )
{
	float	mass, Ixx, Iyy, Izz, dist, floor;
	float	deltaHeight, steerAngle, accelY, timestepInv;
	vec3_t	force, omega, torque, heading, velocity, step;
	matrix4x4	matrix, collisionPadding, transpose;

	pm = pmove;

	PM_UpdateViewAngles( &pm->ps, &pm->cmd );
	AngleVectors( pm->ps.viewangles, pml.forward, pml.right, pml.up );
	CM_CmdUpdateForce(); // get movement direction

	// Get the current world timestep
	pml.frametime = NewtonGetTimeStep( gWorld );
	timestepInv = 1.0f / pml.frametime;

	// get the character mass
	NewtonBodyGetMassMatrix( pm->body, &mass, &Ixx, &Iyy, &Izz );

	// apply the gravity force, cheat a little with the character gravity
	VectorSet( force, 0.0f, mass * -9.8f, 0.0f );

	// Get the velocity vector
	NewtonBodyGetVelocity( pm->body, &velocity[0] );

	// determine if the character have to be snap to the ground
	NewtonBodyGetMatrix( pm->body, &matrix[0][0] );

	// if the floor is with in reach then the character must be snap to the ground
	// the max allow distance for snapping i 0.25 of a meter
	if( m_isAirBorne && !m_jumpTimer )
	{ 
		floor = CM_FindFloor( matrix[3], m_size[2] + 0.25f );
		deltaHeight = ( matrix[3][1] - m_size[2] ) - floor;

		if((deltaHeight < (0.25f - 0.001f)) && (deltaHeight > 0.01f))
		{
			// snap to floor only if the floor is lower than the character feet		
			accelY = - (deltaHeight * timestepInv + velocity[1]) * timestepInv;
			force[1] = mass * accelY;
		}
	}
	else if( m_jumpTimer == 4 )
	{
		vec3_t	veloc = { 0.0f, 0.3f, 0.0f };
		NewtonAddBodyImpulse( pm->body, &veloc[0], &matrix[3][0] );
	}

	m_jumpTimer = m_jumpTimer ? m_jumpTimer - 1 : 0;

	{
		float	speed_factor;
		vec3_t	tmp1, tmp2, result;

		speed_factor = sqrt( DotProduct( pml.movedir, pml.movedir ) + 1.0e-6f );  
		VectorScale( pml.movedir, 1.0f / speed_factor, heading ); 

		VectorScale( heading, mass * 20.0f, tmp1 );
		VectorScale( heading, 2.0f * DotProduct( velocity, heading ), tmp2 );

		VectorSubtract( tmp1, tmp2, result );
		VectorAdd( force, result, force );
		NewtonBodySetForce( pm->body, &force[0] );

		VectorScale( force, pml.frametime / mass, tmp1 );
		VectorAdd( tmp1, velocity, step );
		VectorScale( step, pml.frametime, step );
	}

	VectorSet(step, DotProduct(step,cm.matrix[0]),DotProduct(step,cm.matrix[1]),DotProduct(step,cm.matrix[2])); 	
	MatrixLoadIdentity( collisionPadding );

	step[1] = 0.0f;

	dist = DotProduct( step, step );

	if( dist > m_maxTranslation * m_maxTranslation )
	{
		// when the velocity is high enough that can miss collision we will enlarge the collision 
		// long the vector velocity
		dist = sqrt( dist );
		VectorScale( step, 1.0f / dist, step );

		// make a rotation matrix that will align the velocity vector with the front vector
		collisionPadding[0][0] =  step[0];
		collisionPadding[0][2] = -step[2];
		collisionPadding[2][0] =  step[2];
		collisionPadding[2][2] =  step[0];

		// get the transpose of the matrix                    
		MatrixTranspose( transpose, collisionPadding );
		VectorScale( transpose[0], dist / m_maxTranslation, transpose[0] ); // scale factor

		// calculate and oblique scale matrix by using a similar transformation matrix of the for, R'* S * R
		MatrixConcat( collisionPadding, collisionPadding, transpose );
	}

	// set the collision modifierMatrix;
//NewtonConvexHullModifierSetMatrix( NewtonBodyGetCollision(pm->body), &collisionPadding[0][0]);
          
	steerAngle = asin( bound( -1.0f, pml.forward[2], 1.0f ));
	
	// calculate the torque vector
	NewtonBodyGetOmega( pm->body, &omega[0]);

	VectorSet( torque, 0.0f, 0.5f * Iyy * (steerAngle * timestepInv - omega[1] ) * timestepInv, 0.0f );
	NewtonBodySetTorque( pm->body, &torque[0] );


	// assume the character is on the air. this variable will be set to false if the contact detect 
	// the character is landed 
	m_isAirBorne = true;
	VectorSet( m_stepContact, 0.0f, -m_size[2], 0.0f );   

	pm->ps.viewheight = 22;
	NewtonUpVectorSetPin( cm.upVector, &vec3_up[0] );
}

void CM_ClientMove( pmove_t *pmove )
{

}

physbody_t *Phys_CreatePlayer( sv_edict_t *ed, cmodel_t *mod, matrix4x3 transform )
{
	NewtonCollision	*col, *hull;
	NewtonBody	*body;

	matrix4x4		trans;
	vec3_t		radius, mins, maxs, upDirection;

	if( !cm_physics_model->integer )
		return NULL;

	// setup matrixes
	MatrixLoadIdentity( trans );

	if( mod )
	{
		// player m_size
		VectorCopy( mod->mins, mins );
		VectorCopy( mod->maxs, maxs );
		VectorSubtract( maxs, mins, m_size );
		VectorScale( m_size, 0.5f, radius );
	}
	ConvertDimensionToPhysic( m_size );
	ConvertDimensionToPhysic( radius );

	VectorSet( m_stepContact, 0.0f, -m_size[2], 0.0f );   
	m_maxTranslation = m_size[0] * 0.25f;
	m_maxStepHigh = -m_size[2] * 0.5f;

	VectorCopy( transform[3], trans[3] );	// copy position only

	trans[3][1] = CM_FindFloor( trans[3], 32768 ) + radius[2]; // merge floor position

	// place a sphere at the center
	col = NewtonCreateSphere( gWorld, radius[0], radius[2], radius[1], NULL ); 

	// wrap the character collision under a transform, modifier for tunneling trught walls avoidance
	hull = NewtonCreateConvexHullModifier( gWorld, col );
	NewtonReleaseCollision( gWorld, col );		
	body = NewtonCreateBody( gWorld, hull );	// create the rigid body
	NewtonBodySetAutoFreeze( body, 0 );		// disable auto freeze management for the player
	NewtonWorldUnfreezeBody( gWorld, body );	// keep the player always active 

	// reset angular and linear damping
	NewtonBodySetLinearDamping( body, 0.0f );
	NewtonBodySetAngularDamping( body, vec3_origin );
	NewtonBodySetMaterialGroupID( body, characterID );// set material Id for this object

	// setup generic callback to engine.dll
	NewtonBodySetUserData( body, ed );
	NewtonBodySetTransformCallback( body, Callback_ApplyTransform );
	NewtonBodySetForceAndTorqueCallback( body, Callback_PmoveApplyForce );
	NewtonBodySetMassMatrix( body, 20.0f, m_size[0], m_size[1], m_size[2] ); // 20 kg
	NewtonBodySetMatrix(body, &trans[0][0] );// origin

	// release the collision geometry when not need it
	NewtonReleaseCollision( gWorld, hull );

  	// add and up vector constraint to help in keeping the body upright
	VectorSet( upDirection, 0.0f, 1.0f, 0.0f );
	cm.upVector = NewtonConstraintCreateUpVector( gWorld, &upDirection[0], body ); 
	NewtonBodySetContinuousCollisionMode( body, 1 );

	return (physbody_t *)body;
}

/*
===============================================================================
			PMOVE ENTRY POINT
===============================================================================
*/
void CM_PlayerMove( pmove_t *pmove, bool clientmove )
{
	if( !cm_physics_model->integer )
		Quake_PMove( pmove );
}