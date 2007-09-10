#include "engine.h"
#include "server.h"

#define	STEPSIZE	18
bool	wasonground;
bool	onconveyor;

//damage types
#define DMG_CRUSH			20
#define DMG_FALL			22

#define random()	((rand () & 0x7fff) / ((float)0x7fff))
#define crandom()	(2.0 * (random() - 0.5))

bool CanDamage (edict_t *targ, edict_t *inflictor)
{
	return false;
}

void T_Damage (edict_t *targ, edict_t *inflictor, edict_t *attacker, vec3_t dir, vec3_t point, vec3_t normal, int damage, int knockback, int dflags, int mod)
{
}
void T_RadiusDamage (edict_t *inflictor, edict_t *attacker, float damage, edict_t *ignore, float radius, int mod, double dmg_slope)
{
}


/*
=============
SV_CheckBottom

Returns false if any part of the bottom of the entity is off an edge that
is not a staircase.

=============
*/
int c_yes, c_no;

bool SV_CheckBottom (edict_t *ent)
{
	vec3_t		mins, maxs, start, stop;
	trace_t		trace;
	int		x, y;
	float		mid, bottom;
	
	VectorAdd (ent->s.origin, ent->mins, mins);
	VectorAdd (ent->s.origin, ent->maxs, maxs);

	// if all of the points under the corners are solid world, don't bother
	// with the tougher checks
	// the corners must be within 16 of the midpoint
	start[2] = mins[2] - 1;

	for (x = 0; x <= 1; x++)
	{
		for (y = 0; y <= 1; y++)
		{
			start[0] = x ? maxs[0] : mins[0];
			start[1] = y ? maxs[1] : mins[1];
			if (SV_PointContents(start) != CONTENTS_SOLID)
				goto realcheck;
		}
	}

	c_yes++;
	return true; // we got out easy

realcheck:
	c_no++;

	// check it for real...
	start[2] = mins[2];
	
	// the midpoint must be within 16 of the bottom
	start[0] = stop[0] = (mins[0] + maxs[0])*0.5;
	start[1] = stop[1] = (mins[1] + maxs[1])*0.5;
	stop[2] = start[2] - 2*STEPSIZE;
	trace = SV_Trace (start, vec3_origin, vec3_origin, stop, ent, MASK_MONSTERSOLID);

	if (trace.fraction == 1.0)
		return false;
	mid = bottom = trace.endpos[2];
	
	// the corners must be within 16 of the midpoint	
	for (x = 0; x <= 1; x++)
	{
		for ( y = 0; y <= 1; y++)
		{
			start[0] = stop[0] = x ? maxs[0] : mins[0];
			start[1] = stop[1] = y ? maxs[1] : mins[1];
			
			trace = SV_Trace (start, vec3_origin, vec3_origin, stop, ent, MASK_MONSTERSOLID);
			
			if (trace.fraction != 1.0 && trace.endpos[2] > bottom)
				bottom = trace.endpos[2];
			if (trace.fraction == 1.0 || mid - trace.endpos[2] > STEPSIZE)
				return false;
		}
	}

	c_yes++;
	return true;
}

/*
================
droptofloor
================
*/
void SV_DropToFloor (edict_t *ent)
{
	trace_t		tr;
	vec3_t		v, dest;

	VectorSet(v, -15,-15,-15);
	VectorCopy (v, ent->mins);
	VectorSet(v, 15, 15, 15);
	VectorCopy (v, ent->maxs);

	if (ent->model) PF_setmodel (ent, ent->model);

	// Lazarus:
	// origin_offset is wrong - absmin and absmax weren't set soon enough.
	// Fortunately we KNOW what the "offset" is - nada.
	VectorClear(ent->origin_offset);

	ent->solid = SOLID_BBOX;
	ent->clipmask |= MASK_MONSTERSOLID;
	if(!ent->health) ent->health = 20;
	ent->takedamage = DAMAGE_YES;

	ent->movetype = MOVETYPE_TOSS;  

	VectorSet(v, 0, 0, -128);
	VectorAdd (ent->s.origin, v, dest);

	tr = SV_Trace (ent->s.origin, ent->mins, ent->maxs, dest, ent, MASK_SOLID);
	if (tr.startsolid)
	{
		Msg("SV_DropToFloor: %s startsolid at %s\n", ent->classname, ent->s.origin[0], ent->s.origin[1], ent->s.origin[2]);
		SV_FreeEdict (ent);
		return;
	}
	tr.endpos[2] += 1;
	ent->mins[2] -= 1;
	VectorCopy (tr.endpos, ent->s.origin);

	SV_LinkEdict (ent);
}

void SV_CheckGround (edict_t *ent)
{
	vec3_t		point;
	trace_t		trace;

	if(sv.time < ent->gravity_debounce_time)
		return;

	if (ent->flags & (FL_SWIM|FL_FLY)) return;

	if (ent->velocity[2] > 100)
	{
		ent->groundentity = NULL;
		return;
	}

	// if the hull point one-quarter unit down is solid the entity is on ground
	point[0] = ent->s.origin[0];
	point[1] = ent->s.origin[1];
	point[2] = ent->s.origin[2] - 0.25;

	trace = SV_Trace (ent->s.origin, ent->mins, ent->maxs, point, ent, MASK_MONSTERSOLID);

	// check steepness
	if ( trace.plane.normal[2] < 0.7 && !trace.startsolid)
	{
		ent->groundentity = NULL;
		return;
	}

	// Lazarus: The following 2 lines were in the original code and commented out
	//          by id. However, the effect of this is that a player walking over
	//          a dead monster who is laying on a brush model will cause the 
	//          dead monster to drop through the brush model. This change *may*
	//          have other consequences, though, so watch out for this.

	ent->groundentity = trace.ent;
	ent->groundentity_linkcount = trace.ent->linkcount;
	if (!trace.startsolid && !trace.allsolid)
	{
		VectorCopy (trace.endpos, ent->s.origin);
		ent->groundentity = trace.ent;
		ent->groundentity_linkcount = trace.ent->linkcount;
		ent->velocity[2] = trace.ent->velocity[2];
	}
}

//
//=================
// other_FallingDamage
// Identical to player's P_FallingDamage... except of course ent doesn't have to be a player
//=================
//
void SV_FallingDamage (edict_t *ent)
{
	float	delta;
	float   	fall_time, fall_value;
	int	damage;
	vec3_t	dir;

	if (ent->movetype == MOVETYPE_NOCLIP)
		return;

	if ((ent->oldvelocity[2] < 0) && (ent->velocity[2] > ent->oldvelocity[2]) && (!ent->groundentity))
	{
		delta = ent->oldvelocity[2];
	}
	else
	{
		if (!ent->groundentity)
			return;
		delta = ent->velocity[2] - ent->oldvelocity[2];
	}
	delta = delta*delta * 0.0001;

	// never take falling damage if completely underwater
	if (ent->waterlevel == 3) return;
	if (ent->waterlevel == 2) delta *= 0.25;
	if (ent->waterlevel == 1) delta *= 0.5;

	if (delta < 1) return;

	if (delta < 15)
	{
		ent->s.event = EV_FOOTSTEP;
		return;
	}

	fall_value = delta*0.5;
	if (fall_value > 40) fall_value = 40;
	fall_time = sv.time + 0.3f; //FALL_TIME

	if (delta > 30)
	{
		damage = (delta-30)/2;
		if (damage < 1) damage = 1;
		VectorSet (dir, 0, 0, 1);

		T_Damage (ent, ge->edicts, ge->edicts, dir, ent->s.origin, vec3_origin, damage, 0, 0, DMG_FALL);
	}
}

/*
=============
SV_MoveStep

Called by monster program code.
The move will be adjusted for slopes and stairs, but if the move isn't
possible, no move is done, false is returned, and
pr_global_struct->trace_normal is set to the normal of the blocking wall
=============
*/
//FIXME since we need to test end position contents here, can we avoid doing
//it again later in catagorize position?
bool SV_MoveStep (edict_t *ent, vec3_t move, bool relink)
{
	float		dz;
	vec3_t		oldorg, neworg, end;
	trace_t		trace;
	int		i;
	float		stepsize;
	float		jumpheight;
	vec3_t		test;
	int		contents;

	bool		canjump;
	float		d1, d2;
	int		jump;	// 1=jump up, -1=jump down
	vec3_t		forward, up;
	vec3_t		dir;
	vec_t		dist;
	edict_t		*target;

	// try the move	
	VectorCopy (ent->s.origin, oldorg);
	VectorAdd (ent->s.origin, move, neworg);

	AngleVectors(ent->s.angles,forward,NULL,up);
	if(ent->enemy)
		target = ent->enemy;
	else if(ent->movetarget)
		target = ent->movetarget;
	else
		target = NULL;

	// flying monsters don't step up
	if ( ent->flags & (FL_SWIM | FL_FLY) )
	{
		// try one move with vertical motion, then one without
		for (i = 0; i < 2; i++)
		{
			VectorAdd (ent->s.origin, move, neworg);
			if (i == 0 && ent->enemy)
			{
				if (!ent->goalentity)
					ent->goalentity = ent->enemy;
				dz = ent->s.origin[2] - ent->goalentity->s.origin[2];
				if (ent->goalentity->client)
				{
					if (dz > 40) neworg[2] -= 8;
					if (!((ent->flags & FL_SWIM) && (ent->waterlevel < 2)))
						if (dz < 30) neworg[2] += 8;
				}
				else
				{
					if (dz > 8) neworg[2] -= 8;
					else if (dz > 0) neworg[2] -= dz;
					else if (dz < -8) neworg[2] += 8;
					else neworg[2] += dz;
				}
			}
			trace = SV_Trace (ent->s.origin, ent->mins, ent->maxs, neworg, ent, MASK_MONSTERSOLID);
	
			// fly monsters don't enter water voluntarily
			if (ent->flags & FL_FLY)
			{
				if (!ent->waterlevel)
				{
					test[0] = trace.endpos[0];
					test[1] = trace.endpos[1];
					test[2] = trace.endpos[2] + ent->mins[2] + 1;
					contents = SV_PointContents(test);
					if (contents & MASK_WATER)
						return false;
				}
			}

			// swim monsters don't exit water voluntarily
			if (ent->flags & FL_SWIM)
			{
				if (ent->waterlevel < 2)
				{
					test[0] = trace.endpos[0];
					test[1] = trace.endpos[1];
					test[2] = trace.endpos[2] + ent->mins[2] + 1;
					contents = SV_PointContents(test);
					if (!(contents & MASK_WATER))
						return false;
				}
			}

			if (trace.fraction == 1)
			{
				VectorCopy (trace.endpos, ent->s.origin);
				if (relink)
				{
					SV_LinkEdict(ent);
					SV_TouchTriggers (ent);
				}
				return true;
			}
			
			if (!ent->enemy)
				break;
		}
		
		return false;
	}

	// push down from a step height above the wished position
	if (!(ent->monsterinfo.aiflags & AI_NOSTEP))
		stepsize = STEPSIZE;
	else stepsize = 1;

	neworg[2] += stepsize;
	VectorCopy (neworg, end);
	end[2] -= stepsize*2;

	trace = SV_Trace (neworg, ent->mins, ent->maxs, end, ent, MASK_MONSTERSOLID);

	// Determine whether monster is capable of and/or should jump
	jump = 0;
	if((ent->monsterinfo.jump) && !(ent->monsterinfo.aiflags & AI_DUCKED))
	{
		// Don't jump if path is blocked by monster or player. Otherwise,
		// monster might attempt to jump OVER the monster/player, which 
		// ends up looking a bit goofy. Also don't jump if the monster's
		// movement isn't deliberate (target=NULL)
		if(trace.ent && (trace.ent->client || (trace.ent->svflags & SVF_MONSTER)))
			canjump = false;
		else if(target)
		{
			// Never jump unless it places monster closer to his goal
			vec3_t	dir;
			VectorSubtract(target->s.origin, oldorg, dir);
			d1 = VectorLength(dir);
			VectorSubtract(target->s.origin, trace.endpos, dir);
			d2 = VectorLength(dir);
			if(d2 < d1)
				canjump = true;
			else
				canjump = false;
		}
		else canjump = false;
	}
	else canjump = false;

	if (trace.allsolid)
	{
		if(canjump && (ent->monsterinfo.jumpup > 0))
		{
			neworg[2] += ent->monsterinfo.jumpup - stepsize;
			trace = SV_Trace (neworg, ent->mins, ent->maxs, end, ent, MASK_MONSTERSOLID);
			if (!trace.allsolid && !trace.startsolid && trace.fraction > 0 && (trace.plane.normal[2] > 0.9))
			{
				if(!trace.ent || (!trace.ent->client && !(trace.ent->svflags & SVF_MONSTER) && !(trace.ent->svflags & SVF_DEADMONSTER)))
				{
					// Good plane to jump on. Make sure monster is more or less facing
					// the obstacle to avoid cutting-corners jumps
					trace_t	tr;
					vec3_t	p2;

					VectorMA(ent->s.origin,1024,forward,p2);
					tr = SV_Trace(ent->s.origin,ent->mins,ent->maxs,p2,ent,MASK_MONSTERSOLID);
					if(DotProduct(tr.plane.normal,forward) < -0.95)
					{
						jump = 1;
						jumpheight = trace.endpos[2] - ent->s.origin[2];
					}
					else return false;
				}
			}
			else return false;
		}
		else return false;
	}

	if (trace.startsolid)
	{
		neworg[2] -= stepsize;
		trace = SV_Trace (neworg, ent->mins, ent->maxs, end, ent, MASK_MONSTERSOLID);
		if (trace.allsolid || trace.startsolid)
			return false;
	}


	// don't go in to water
	// Lazarus: misc_actors don't go swimming, but wading is fine
	if (ent->monsterinfo.aiflags & AI_ACTOR)
	{
		// First check for lava/slime under feet - but only if we're not already in
		// a liquid
		test[0] = trace.endpos[0];
		test[1] = trace.endpos[1];
		if (ent->waterlevel == 0)
		{
			test[2] = trace.endpos[2] + ent->mins[2] + 1;	
			contents = SV_PointContents(test);
			if (contents & (CONTENTS_LAVA | CONTENTS_SLIME))
				return false;
		}
		test[2] = trace.endpos[2] + ent->viewheight - 1;
		contents = SV_PointContents(test);
		if (contents & MASK_WATER)
			return false;
	}
	else if (ent->waterlevel == 0)
	{
		test[0] = trace.endpos[0];
		test[1] = trace.endpos[1];
		test[2] = trace.endpos[2] + ent->mins[2] + 1;	
		contents = SV_PointContents(test);

		if (contents & MASK_WATER)
			return false;
	}

	// Lazarus: Don't intentionally walk into lasers.
	dist = VectorLength(move);
	if(dist > 0.)
	{
		edict_t		*e;
		trace_t		laser_trace;
		vec_t		delta;
		vec3_t		laser_mins, laser_maxs;
		vec3_t		laser_start, laser_end;
		vec3_t		monster_mins, monster_maxs;

		for(i = maxclients->value + 1; i < ge->num_edicts; i++)
		{
			e = EDICT_NUM(i);
			if(!e->inuse) continue;
			if(!e->classname) continue;
			if(strcmp(e->classname, "env_laser")) continue;
			if(e->svflags & SVF_NOCLIENT) continue;
			if(!PF_inPVS(ent->s.origin,e->s.origin))
				continue;
			// Check to see if monster is ALREADY in the path of this laser.
			// If so, allow the move so he can get out.
			VectorMA(e->s.origin, 2048, e->movedir, laser_end);
			laser_trace = SV_Trace(e->s.origin,NULL,NULL,laser_end,NULL,CONTENTS_SOLID|CONTENTS_MONSTER);
			if(laser_trace.ent == ent)
				continue;
			VectorCopy(laser_trace.endpos,laser_end);
			laser_mins[0] = min(e->s.origin[0],laser_end[0]);
			laser_mins[1] = min(e->s.origin[1],laser_end[1]);
			laser_mins[2] = min(e->s.origin[2],laser_end[2]);
			laser_maxs[0] = max(e->s.origin[0],laser_end[0]);
			laser_maxs[1] = max(e->s.origin[1],laser_end[1]);
			laser_maxs[2] = max(e->s.origin[2],laser_end[2]);
			monster_mins[0] = min(oldorg[0],trace.endpos[0]) + ent->mins[0];
			monster_mins[1] = min(oldorg[1],trace.endpos[1]) + ent->mins[1];
			monster_mins[2] = min(oldorg[2],trace.endpos[2]) + ent->mins[2];
			monster_maxs[0] = max(oldorg[0],trace.endpos[0]) + ent->maxs[0];
			monster_maxs[1] = max(oldorg[1],trace.endpos[1]) + ent->maxs[1];
			monster_maxs[2] = max(oldorg[2],trace.endpos[2]) + ent->maxs[2];
			if( monster_maxs[0] < laser_mins[0] ) continue;
			if( monster_maxs[1] < laser_mins[1] ) continue;
			if( monster_maxs[2] < laser_mins[2] ) continue;
			if( monster_mins[0] > laser_maxs[0] ) continue;
			if( monster_mins[1] > laser_maxs[1] ) continue;
			if( monster_mins[2] > laser_maxs[2] ) continue;
			// If we arrive here, some part of the bounding box surrounding
			// monster's total movement intersects laser bounding box.
			// If laser is parallel to x, y, or z, we definitely
			// know this move will put monster in path of laser
			if ( (e->movedir[0] == 1.) || (e->movedir[1] == 1.) || (e->movedir[2] == 1.))
				return false;
			// Shift psuedo laser towards monster's current position up to
			// the total distance he's proposing moving.
			delta = min(16,dist);
			VectorCopy(move,dir);
			VectorNormalize(dir);
			while(delta < dist+15.875)
			{
				if(delta > dist) delta = dist;
				VectorMA(e->s.origin,    -delta,dir,laser_start);
				VectorMA(e->s.old_origin,-delta,dir,laser_end);
				laser_trace = SV_Trace(laser_start,NULL,NULL,laser_end,ge->edicts,CONTENTS_SOLID|CONTENTS_MONSTER);
				if(laser_trace.ent == ent)
					return false;
				delta += 16;
			}
		}
	}
	if ((trace.fraction == 1) && !jump && canjump && (ent->monsterinfo.jumpdn > 0))
	{
		end[2] = oldorg[2] + move[2] - ent->monsterinfo.jumpdn;
		trace = SV_Trace (neworg, ent->mins, ent->maxs, end, ent, MASK_MONSTERSOLID | MASK_WATER);
		if(trace.fraction < 1 && (trace.plane.normal[2] > 0.9) && (trace.contents & MASK_SOLID) && (neworg[2] - 16 > trace.endpos[2]))
		{
			if(!trace.ent || (!trace.ent->client && !(trace.ent->svflags & SVF_MONSTER) && !(trace.ent->svflags & SVF_DEADMONSTER)))
				jump = -1;
		}
	}


	if ((trace.fraction == 1) && !jump)
	{
		// if monster had the ground pulled out, go ahead and fall
		if ( ent->flags & FL_PARTIALGROUND )
		{
			VectorAdd (ent->s.origin, move, ent->s.origin);
			if (relink)
			{
				SV_LinkEdict(ent);
				SV_TouchTriggers (ent);
			}
			ent->groundentity = NULL;
			return true;
		}
		return false;		// walked off an edge
	}

	// check point traces down for dangling corners
	VectorCopy (trace.endpos, ent->s.origin);

	if(!jump)
	{
		bool	skip = false;
		// if monster CAN jump down, and a position just a bit forward would be 
		// a good jump-down spot, allow (briefly) !M_CheckBottom
		if (canjump && target && (target->s.origin[2] < ent->s.origin[2]) && (ent->monsterinfo.jumpdn > 0))
		{
			vec3_t		p1, p2;
			trace_t		tr;

			VectorMA(oldorg,48,forward,p1);
			tr = SV_Trace(ent->s.origin, ent->mins, ent->maxs, p1, ent, MASK_MONSTERSOLID);
			if(tr.fraction == 1)
			{
				p2[0] = p1[0];
				p2[1] = p1[1];
				p2[2] = p1[2] - ent->monsterinfo.jumpdn;
				tr = SV_Trace(p1,ent->mins,ent->maxs,p2,ent,MASK_MONSTERSOLID | MASK_WATER);
				if(tr.fraction < 1 && (tr.plane.normal[2] > 0.9) && (tr.contents & MASK_SOLID) && (p1[2] - 16 > tr.endpos[2]))
				{
					if(!tr.ent || (!tr.ent->client && !(tr.ent->svflags & SVF_MONSTER) && !(tr.ent->svflags & SVF_DEADMONSTER)))
					{
						VectorSubtract(target->s.origin, tr.endpos, dir);
						d2 = VectorLength(dir);
						if(d2 < d1)
							skip = true;
					}
				}
			}

		}
		if (!skip)
		{
			if (!SV_CheckBottom (ent))
			{
				if ( ent->flags & FL_PARTIALGROUND )
				{	// entity had floor mostly pulled out from underneath it
					// and is trying to correct
					if (relink)
					{
						SV_LinkEdict (ent);
						SV_TouchTriggers (ent);
					}
					return true;
				}
				VectorCopy (oldorg, ent->s.origin);
				return false;
			}
		}
	}

	if ( ent->flags & FL_PARTIALGROUND )
	{
		ent->flags &= ~FL_PARTIALGROUND;
	}
	ent->groundentity = trace.ent;
	if(trace.ent) ent->groundentity_linkcount = trace.ent->linkcount;

	// the move is ok
	if(jump)
	{
		VectorScale(move, 10, ent->velocity);
		if(jump > 0)
		{
			ent->monsterinfo.jump(ent);
			ent->velocity[2] = 2.5*jumpheight + 80;
		}
		else
		{
			ent->velocity[2] = max(ent->velocity[2],100);
			if(oldorg[2] - ent->s.origin[2] > 48)
				ent->s.origin[2] = oldorg[2] + ent->velocity[2]*0.1f;
		}
		if(relink)
		{
			SV_LinkEdict (ent);
			SV_TouchTriggers (ent);
		}
	}
	else if (relink)
	{
		SV_LinkEdict (ent);
		SV_TouchTriggers (ent);
	}
	return true;
}

/*
===============
SV_WalkMove
===============
*/
bool SV_WalkMove (edict_t *ent, float yaw, float dist)
{
	vec3_t	move;
	
	if (!ent->groundentity && !(ent->flags & (FL_FLY|FL_SWIM)))
		return false;

	yaw = yaw * M_PI*2 / 360;
	
	move[0] = cos(yaw)*dist;
	move[1] = sin(yaw)*dist;
	move[2] = 0;

	return SV_MoveStep(ent, move, true);
}

/*
============
SV_TestEntityPosition

============
*/
edict_t *SV_TestEntityPosition (edict_t *ent)
{
	trace_t		trace;
	int		mask;

	if (ent->clipmask) mask = ent->clipmask;
	else mask = MASK_SOLID;

	if(ent->solid == SOLID_BSP)
	{
		vec3_t	org, mins, maxs;
		VectorAdd(ent->s.origin, ent->origin_offset, org);
		VectorSubtract(ent->mins, ent->origin_offset, mins);
		VectorSubtract(ent->maxs, ent->origin_offset, maxs);
		trace = SV_Trace (org, mins, maxs, org, ent, mask);
	}
	else trace = SV_Trace (ent->s.origin, ent->mins, ent->maxs, ent->s.origin, ent, mask);

	if (trace.startsolid)
	{
		// Lazarus - work around for players/monsters standing on dead monsters causing
		// those monsters to gib when rotating brush models are in the vicinity
		if ( (ent->svflags & SVF_DEADMONSTER) && (trace.ent->client || (trace.ent->svflags & SVF_MONSTER)))
			return NULL;

		// Lazarus - return a bit more useful info than simply "g_edicts"
		if(trace.ent) return trace.ent;
		else return ge->edicts;
	}
	
	return NULL;
}

/*
================
SV_CheckVelocity
================
*/
void SV_CheckVelocity (edict_t *ent)
{
	if(VectorLength(ent->velocity) > sv_maxvelocity->value)
	{
		VectorNormalize(ent->velocity);
		VectorScale(ent->velocity, sv_maxvelocity->value, ent->velocity);
	}
}

/*
=============
SV_RunThink

Runs thinking code for this frame if necessary
=============
*/
bool SV_RunThink (edict_t *ent)
{
	float	thinktime;

	thinktime = ent->nextthink;
	if(thinktime <= 0) return true;

	if(thinktime > sv.time + 0.001)
		return true;
	
	ent->nextthink = 0;
	if (!ent->think) 
		PF_error ("NULL ent->think for %s",ent->classname);
	ent->think (ent);

	return false;
}

/*
==================
SV_Impact

Two entities have touched, so run their touch functions
==================
*/
void SV_Impact (edict_t *e1, trace_t *trace)
{
	edict_t		*e2;

	e2 = trace->ent;

	if (e1->touch && e1->solid != SOLID_NOT)
		e1->touch (e1, e2, &trace->plane, trace->surface);
	
	if (e2->touch && e2->solid != SOLID_NOT)
		e2->touch (e2, e1, NULL, NULL);
}


/*
==================
ClipVelocity

Slide off of the impacting object
returns the blocked flags (1 = floor, 2 = step / wall)
==================
*/
int ClipVelocity (vec3_t in, vec3_t normal, vec3_t out, float overbounce)
{
	float		backoff;
	float		change;
	int		i, blocked;
	
	blocked = 0;
	if (normal[2] > 0) blocked |= 1;	// floor
	if (!normal[2]) blocked |= 2;		// step
	
	backoff = DotProduct (in, normal) * overbounce;

	for (i=0 ; i<3 ; i++)
	{
		change = normal[i]*backoff;
		out[i] = in[i] - change;
		if (out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
			out[i] = 0;
	}

	return blocked;
}

/*
============
SV_FlyMove

The basic solid body movement clip that slides along multiple planes
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
============
*/
#define	MAX_CLIP_PLANES	5
int SV_FlyMove (edict_t *ent, float time, int mask)
{
	edict_t		*hit;
	int		bumpcount, numbumps;
	vec3_t		dir;
	float		d;
	int		numplanes;
	vec3_t		planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity, original_velocity, new_velocity;
	int		i, j;
	trace_t		trace;
	vec3_t		end;
	float		time_left;
	int		blocked;
	int		num_retries = 0;

retry:
	numbumps = 4;
	
	blocked = 0;
	VectorCopy (ent->velocity, original_velocity);
	VectorCopy (ent->velocity, primal_velocity);
	numplanes = 0;
	
	time_left = time;

	ent->groundentity = NULL;
	for (bumpcount = 0; bumpcount < numbumps; bumpcount++)
	{
		for (i = 0; i < 3; i++) end[i] = ent->s.origin[i] + time_left * ent->velocity[i];

		trace = SV_Trace (ent->s.origin, ent->mins, ent->maxs, end, ent, mask);

		if (trace.allsolid)
		{	
			// entity is trapped in another solid
			VectorCopy (vec3_origin, ent->velocity);
			return 3;
		}

		if (trace.fraction > 0)
		{	
			// actually covered some distance
			VectorCopy (trace.endpos, ent->s.origin);
			VectorCopy (ent->velocity, original_velocity);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			 break;		// moved the entire distance

		hit = trace.ent;

		// Lazarus: If the pushed entity is a conveyor, raise us up and
		// try again
		if (!num_retries && wasonground)
		{
			if ((hit->movetype == MOVETYPE_CONVEYOR) && (trace.plane.normal[2] > 0.7))
			{
				vec3_t	above;

				VectorCopy(end,above);
				above[2] += 32;
				trace = SV_Trace (above, ent->mins, ent->maxs, end, ent, mask);
				VectorCopy (trace.endpos,end);
				end[2] += 1;
				VectorSubtract (end,ent->s.origin,ent->velocity);
				VectorScale (ent->velocity,1.0/time_left,ent->velocity);
				num_retries++;
				goto retry;
			}
		}

		// if blocked by player AND on a conveyor
		if (hit && hit->client && onconveyor)
		{
			vec3_t	player_dest;
			trace_t	ptrace;

			if(ent->mass > hit->mass)
			{
				VectorMA (hit->s.origin,time_left,ent->velocity,player_dest);
				ptrace = SV_Trace(hit->s.origin,hit->mins,hit->maxs,player_dest,hit,hit->clipmask);
				if(ptrace.fraction == 1.0)
				{
					VectorCopy(player_dest,hit->s.origin);
					SV_LinkEdict(hit);
					goto retry;
				}
			}
			blocked |= 8;
		}

		if (trace.plane.normal[2] > 0.7)
		{
			blocked |= 1; // floor
			if ( hit->solid == SOLID_BSP)
			{
				ent->groundentity = hit;
				ent->groundentity_linkcount = hit->linkcount;
			}
		}
		if (!trace.plane.normal[2])
		{
			blocked |= 2;		// step
		}

		// run the impact function
		SV_Impact (ent, &trace);
		if (!ent->inuse)
			break;		// removed by the impact function

		
		time_left -= time_left * trace.fraction;
		
		// cliped to another plane
		if (numplanes >= MAX_CLIP_PLANES)
		{	
			// this shouldn't really happen
			VectorCopy (vec3_origin, ent->velocity);
			blocked |= 3;
			return blocked;
		}

		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		for (i=0 ; i<numplanes ; i++)
		{
			ClipVelocity (original_velocity, planes[i], new_velocity, 1);

			for (j=0 ; j<numplanes ; j++)
				if ((j != i) && !VectorCompare (planes[i], planes[j]))
				{
					if (DotProduct (new_velocity, planes[j]) < 0)
						break;	// not ok
				}
			if (j == numplanes)
				break;
		}
		
		if (i != numplanes)
		{	// go along this plane
			VectorCopy (new_velocity, ent->velocity);
		}
		else
		{	// go along the crease
			if (numplanes != 2)
			{
//				gi.dprintf ("clip velocity, numplanes == %i\n",numplanes);
				VectorCopy (vec3_origin, ent->velocity);
				blocked |= 7;
				return blocked;
			}
			CrossProduct (planes[0], planes[1], dir);
			d = DotProduct (dir, ent->velocity);
			VectorScale (dir, d, ent->velocity);
		}

		// if original velocity is against the original velocity, stop dead
		// to avoid tiny occilations in sloping corners
		if (DotProduct (ent->velocity, primal_velocity) <= 0)
		{
			VectorCopy (vec3_origin, ent->velocity);
			return blocked;
		}
	}

	return blocked;
}

/*
============
SV_PushableMove

The basic solid body movement clip that slides along multiple planes
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
============
*/
#define	MAX_CLIP_PLANES	5
int SV_PushableMove (edict_t *ent, float time, int mask)
{
	edict_t		*hit;
	int		bumpcount, numbumps;
	vec3_t		dir;
	float		d;
	int		numplanes;
	vec3_t		planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity, original_velocity, new_velocity;
	int		i, j;
	trace_t		trace;
	vec3_t		end;
	float		time_left;
	int		blocked;
	int		num_retries = 0;

	// Corrective stuff added for bmodels with no origin brush
	vec3_t		mins, maxs;
	vec3_t      	origin;

retry:

	numbumps = 4;
	ent->bouncetype = 0;
	
	blocked = 0;
	VectorCopy (ent->velocity, original_velocity);
	VectorCopy (ent->velocity, primal_velocity);
	numplanes = 0;
	
	time_left = time;

	VectorAdd(ent->s.origin,ent->origin_offset,origin);
	VectorCopy(ent->size,maxs);
	VectorScale(maxs,0.5,maxs);
	VectorNegate(maxs,mins);

	ent->groundentity = NULL;

	for (bumpcount=0 ; bumpcount<numbumps ; bumpcount++)
	{
		for (i = 0; i < 3; i++) end[i] = origin[i] + time_left * ent->velocity[i];

		trace = SV_Trace (origin, mins, maxs, end, ent, mask);

		if (trace.allsolid)
		{	
			// entity is trapped in another solid
			VectorCopy (vec3_origin, ent->velocity);
			return 3;
		}

		if (trace.fraction > 0)
		{	
			// actually covered some distance
			VectorCopy (trace.endpos, origin);
			VectorSubtract (origin, ent->origin_offset, ent->s.origin);
			VectorCopy (ent->velocity, original_velocity);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			break;		// moved the entire distance

		hit = trace.ent;

		// Lazarus: If the pushed entity is a conveyor, raise us up and
		// try again
		if (!num_retries && wasonground)
		{
			if ((hit->movetype == MOVETYPE_CONVEYOR) && (trace.plane.normal[2] > 0.7))
			{
				vec3_t	above;

				VectorCopy(end,above);
				above[2] += 32;
				trace = SV_Trace (above, mins, maxs, end, ent, mask);
				VectorCopy (trace.endpos,end);
				VectorSubtract (end,origin,ent->velocity);
				VectorScale (ent->velocity,1.0/time_left,ent->velocity);
				num_retries++;
				goto retry;
			}
		}

		// if blocked by player AND on a conveyor
		if (hit->client && onconveyor)
		{
			vec3_t	player_dest;
			trace_t	ptrace;

			if(ent->mass > hit->mass)
			{
				VectorMA (hit->s.origin,time_left,ent->velocity,player_dest);
				ptrace = SV_Trace(hit->s.origin,hit->mins,hit->maxs,player_dest,hit,hit->clipmask);
				if(ptrace.fraction == 1.0)
				{
					VectorCopy(player_dest,hit->s.origin);
					SV_LinkEdict(hit);
					goto retry;
				}
			}
			blocked |= 8;
		}

		if (trace.plane.normal[2] > 0.7)
		{
			// Lazarus: special case - if this ent or the impact ent is
			//          in water, motion is NOT blocked.
			if((hit->movetype != MOVETYPE_PUSHABLE) || ((ent->waterlevel==0) && (hit->waterlevel==0)))
			{
				blocked |= 1;		// floor
				if ( hit->solid == SOLID_BSP)
				{
					ent->groundentity = hit;
					ent->groundentity_linkcount = hit->linkcount;
				}
			}
		}
		if (!trace.plane.normal[2])
		{
			blocked |= 2;		// step
		}

		// run the impact function
		SV_Impact (ent, &trace);
		if (!ent->inuse) break; // removed by the impact function

		time_left -= time_left * trace.fraction;

		// clipped to another plane
		if (numplanes >= MAX_CLIP_PLANES)
		{	
			// this shouldn't really happen
			VectorCopy (vec3_origin, ent->velocity);
			blocked |= 3;
			return blocked;
		}

		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		for (i = 0; i < numplanes; i++)
		{
			// DH: experimenting here. 1 is no bounce,
			// 1.5 bounces like a grenade, 2 is a superball
			if(ent->bouncetype == 1)
			{
				ClipVelocity (original_velocity, planes[i], new_velocity, 1.4);
				// stop small oscillations
				if (new_velocity[2] < 60)
				{
					ent->groundentity = trace.ent;
					ent->groundentity_linkcount = trace.ent->linkcount;
					VectorCopy (vec3_origin, new_velocity);
				} 
				else
				{
					// add a bit of random horizontal motion
					if(!new_velocity[0]) new_velocity[0] = crandom() * new_velocity[2]/4;
					if(!new_velocity[1]) new_velocity[1] = crandom() * new_velocity[2]/4;
				}
			}
			else if(ent->bouncetype == 2)
			{
				VectorCopy(ent->velocity, new_velocity);
			}
			else
			{
				ClipVelocity (original_velocity, planes[i], new_velocity, 1);
                              }
			for (j = 0; j < numplanes; j++)
			{
				if ((j != i) && !VectorCompare (planes[i], planes[j]))
				{
					if (DotProduct (new_velocity, planes[j]) < 0)
						break; // not ok
				}
			}
			if (j == numplanes) break;
		}
		
		if (i != numplanes)
		{	
			// go along this plane
			VectorCopy (new_velocity, ent->velocity);
		}
		else
		{	// go along the crease
			if (numplanes != 2)
			{
				VectorCopy (vec3_origin, ent->velocity);
				blocked |= 7;
				return blocked;
			}
			CrossProduct (planes[0], planes[1], dir);
			d = DotProduct (dir, ent->velocity);
			VectorScale (dir, d, ent->velocity);
		}

		// if velocity is against the original velocity, stop dead
		// to avoid tiny occilations in sloping corners
		if( !ent->bouncetype )
		{
			if (DotProduct (ent->velocity, primal_velocity) <= 0)
			{
				VectorCopy (vec3_origin, ent->velocity);
				return blocked;
			}
		}
	}
	return blocked;
}

/*
============
SV_AddGravity

============
*/
void SV_AddGravity (edict_t *ent)
{
	if(sv.time > ent->gravity_debounce_time)
		ent->velocity[2] -= ent->gravity * sv_gravity->value * 0.1;
}

/*
===============================================================================

PUSHMOVE

===============================================================================
*/

/*
=================================================
SV_PushEntity

Does not change the entities velocity at all

called for	MOVETYPE_TOSS
		MOVETYPE_BOUNCE
		MOVETYPE_FLY
=================================================
*/
trace_t SV_PushEntity (edict_t *ent, vec3_t push)
{
	trace_t		trace;
	vec3_t		start;
	vec3_t		end;
	int		mask;
	int		num_retries=0;

	VectorCopy (ent->s.origin, start);
	VectorAdd (start, push, end);

	if (ent->clipmask)
		mask = ent->clipmask;
	else
		mask = MASK_SOLID;

retry:
	trace = SV_Trace (start, ent->mins, ent->maxs, end, ent, mask);
	
	VectorCopy (trace.endpos, ent->s.origin);
	SV_LinkEdict (ent);

	if (trace.fraction != 1.0)
	{
		SV_Impact (ent, &trace);

		// if the pushed entity went away and the pusher is still there
		if (!trace.ent->inuse && ent->inuse)
		{
			// move the pusher back and try again
			VectorCopy (start, ent->s.origin);
			SV_LinkEdict (ent);
			goto retry;
		}

		// Lazarus: If the pushed entity is a conveyor, raise us up and
		// try again
		if (!num_retries && wasonground)
		{
			if ((trace.ent->movetype == MOVETYPE_CONVEYOR) && (trace.plane.normal[2] > 0.7) && !trace.startsolid)
			{
				vec3_t	above;
				VectorCopy(end,above);
				above[2] += 32;
				trace = SV_Trace (above, ent->mins, ent->maxs, end, ent, mask);
				VectorCopy (trace.endpos, end);
				VectorCopy (start, ent->s.origin);
				SV_LinkEdict(ent);
				num_retries++;
				goto retry;
			}
		}
		if(onconveyor && !trace.ent->client)
		{
			// If blocker can be damaged, destroy it. Otherwise destroy blockee.
			if(trace.ent->takedamage == DAMAGE_YES)
				T_Damage(trace.ent, ent, ent, vec3_origin, trace.ent->s.origin, vec3_origin, 100000, 1, 0, DMG_CRUSH);
			else T_Damage(ent, trace.ent, trace.ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 1, 0, DMG_CRUSH);
		}

	}
	if (ent->inuse) SV_TouchTriggers (ent);

	return trace;
}					

typedef struct
{
	edict_t	*ent;
	vec3_t	origin;
	vec3_t	angles;
	float	deltayaw;

} pushed_t;

pushed_t	pushed[MAX_EDICTS], *pushed_p;
edict_t	*obstacle;

void MoveRiders_r(edict_t *platform, edict_t *ignore, vec3_t move, vec3_t amove, bool turn)
{
	int		i;
	edict_t		*rider;

	for(i = 1, rider = EDICT_NUM(i); i <= ge->num_edicts; i++, rider = EDICT_NUM(i))
	{
		if((rider->groundentity == platform) && (rider != ignore))
		{
			VectorAdd(rider->s.origin, move, rider->s.origin);
			if (turn && (amove[YAW] != 0.))
			{
				if(!rider->client) rider->s.angles[YAW] += amove[YAW];
				else
				{
					rider->s.angles[YAW] += amove[YAW];
					rider->client->ps.pmove.delta_angles[YAW] += ANGLE2SHORT(amove[YAW]);
					rider->client->ps.pmove.pm_type = PM_FREEZE;
					rider->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
				}
			}
			SV_LinkEdict(rider);

			if(SV_TestEntityPosition(rider))
			{
				// Move is blocked. Since this is for riders, not pushees,
				// it should be ok to just back the move for this rider off
				VectorSubtract(rider->s.origin,move,rider->s.origin);
				if(turn && (amove[YAW] != 0.))
				{
					rider->s.angles[YAW] -= amove[YAW];
					if(rider->client)
					{
						rider->client->ps.pmove.delta_angles[YAW] -= ANGLE2SHORT(amove[YAW]);
						rider->client->ps.viewangles[YAW] -= amove[YAW];
					}
				}
				SV_LinkEdict(rider);
			} 
			else
			{
				// move this rider's riders
				MoveRiders_r(rider, ignore, move, amove, turn);
			}
		}
	}
}

/*
============
RealBoundingBox

Returns the actual bounding box of a bmodel. This is a big improvement over
what q2 normally does with rotating bmodels - q2 sets absmin, absmax to a cube
that will completely contain the bmodel at *any* rotation on *any* axis, whether
the bmodel can actually rotate to that angle or not. This leads to a lot of
false block tests in SV_Push if another bmodel is in the vicinity.
============
*/
void RealBoundingBox(edict_t *ent, vec3_t mins, vec3_t maxs)
{
	vec3_t		forward, left, up, f1, l1, u1;
	vec3_t		p[8];
	int		i, j, k, j2, k4;

	for(k = 0; k < 2; k++)
	{
		k4 = k * 4;
		if(k) p[k4][2] = ent->maxs[2];
		else p[k4][2] = ent->mins[2];

		p[k4 + 1][2] = p[k4][2];
		p[k4 + 2][2] = p[k4][2];
		p[k4 + 3][2] = p[k4][2];

		for(j = 0; j < 2; j++)
		{
			j2 = j * 2;
			if(j) p[j2+k4][1] = ent->maxs[1];
			else p[j2+k4][1] = ent->mins[1];
			p[j2 + k4 + 1][1] = p[j2 + k4][1];

			for(i = 0; i < 2; i++)
			{
				if(i) p[i + j2 + k4][0] = ent->maxs[0];
				else p[i + j2 + k4][0] = ent->mins[0];
			}
		}
	}

	AngleVectors(ent->s.angles, forward, left, up);

	for(i = 0; i < 8; i++)
	{
		VectorScale(forward, p[i][0], f1);
		VectorScale(left, -p[i][1], l1);
		VectorScale(up, p[i][2], u1);
		VectorAdd(ent->s.origin, f1, p[i]);
		VectorAdd(p[i], l1, p[i]);
		VectorAdd(p[i], u1, p[i]);
	}

	VectorCopy(p[0],mins);
	VectorCopy(p[0],maxs);

	for(i = 1; i < 8; i++)
	{
		mins[0] = min(mins[0], p[i][0]);
		mins[1] = min(mins[1], p[i][1]);
		mins[2] = min(mins[2], p[i][2]);
		maxs[0] = max(maxs[0], p[i][0]);
		maxs[1] = max(maxs[1], p[i][1]);
		maxs[2] = max(maxs[2], p[i][2]);
	}
}

/*
============
SV_Push

Objects need to be moved back on a failed push,
otherwise riders would continue to slide.
============
*/
bool SV_Push (edict_t *pusher, vec3_t move, vec3_t amove)
{
	int		i, e;
	edict_t		*check, *block;
	vec3_t		mins, maxs;
	pushed_t		*p;
	vec3_t		org, org2, org_check, forward, right, up;
	vec3_t		move2={0,0,0};
	vec3_t		move3={0,0,0};
	vec3_t		realmins, realmaxs;
	bool		turn = true;
	trace_t		tr;

	// clamp the move to 1/8 units, so the position will
	// be accurate for client side prediction
	for (i = 0; i < 3; i++)
	{
		float	temp;
		temp = move[i]*8.0;
		if (temp > 0.0) temp += 0.5;
		else temp -= 0.5;
		move[i] = 0.125 * (int)temp;
	}

	// find the bounding box
	for (i = 0; i < 3; i++)
	{
		mins[i] = pusher->absmin[i] + move[i];
		maxs[i] = pusher->absmax[i] + move[i];
	}

	// we need this for pushing things later
	VectorSubtract (vec3_origin, amove, org);
	AngleVectors (org, forward, right, up);

	// save the pusher's original position
	pushed_p->ent = pusher;
	VectorCopy (pusher->s.origin, pushed_p->origin);
	VectorCopy (pusher->s.angles, pushed_p->angles);
	if (pusher->client) pushed_p->deltayaw = pusher->client->ps.pmove.delta_angles[YAW];
	pushed_p++;

	// move the pusher to it's final position
	VectorAdd (pusher->s.origin, move, pusher->s.origin);
	VectorAdd (pusher->s.angles, amove, pusher->s.angles);
	SV_LinkEdict (pusher);

	// Lazarus: Standard Q2 takes a horrible shortcut
	//          with rotating brush models, setting
	//          absmin and absmax to a cube that would
	//          contain the brush model if it could
	//          rotate around ANY axis. The result is
	//          a lot of false hits on intersections,
	//          particularly when you have multiple
	//          rotating brush models in the same area.
	//          RealBoundingBox gives us the actual
	//          bounding box at the current angles.
	RealBoundingBox(pusher,realmins,realmaxs);

	// see if any solid entities are inside the final position
	check = EDICT_NUM( 1 );

	for (e = 1; e < ge->num_edicts; e++, check = EDICT_NUM(e))
	{
		if (!check->inuse) continue;
		if (check == pusher->owner) continue;
		if (check->movetype == MOVETYPE_PUSH || check->movetype == MOVETYPE_NONE || check->movetype == MOVETYPE_NOCLIP)
			continue;

		if (!check->area.prev)
			continue;		// not linked in anywhere

		// if the entity is standing on the pusher, it will definitely be moved
		if (check->groundentity != pusher)
		{
			// see if the ent needs to be tested
			if ( check->absmin[0] >= realmaxs[0]
			|| check->absmin[1] >= realmaxs[1]
			|| check->absmin[2] >= realmaxs[2]
			|| check->absmax[0] <= realmins[0]
			|| check->absmax[1] <= realmins[1]
			|| check->absmax[2] <= realmins[2] )
				continue;

			// see if the ent's bbox is inside the pusher's final position
			if (!SV_TestEntityPosition (check))
				continue;
		}

		// Lazarus: func_tracktrain-specific stuff
		// If train is *driven*, then hurt monsters/players it touches NOW
		// rather than waiting to be blocked.
		if ((pusher->flags & FL_TRACKTRAIN) && pusher->owner && ((check->svflags & SVF_MONSTER) || check->client) && (check->groundentity != pusher))
		{
			vec3_t	dir;
			VectorSubtract(check->s.origin,pusher->s.origin,dir);
			dir[2] += 16;
			VectorNormalize(dir);
			T_Damage (check, pusher, pusher, dir, check->s.origin, vec3_origin, pusher->dmg, 1, 0, DMG_CRUSH);
		}

		if ((pusher->movetype == MOVETYPE_PUSH) || (pusher->movetype == MOVETYPE_PENDULUM) || (check->groundentity == pusher))
		{
			// move this entity
			pushed_p->ent = check;
			VectorCopy (check->s.origin, pushed_p->origin);
			VectorCopy (check->s.angles, pushed_p->angles);
			pushed_p++;

			// try moving the contacted entity 
			VectorAdd (check->s.origin, move, check->s.origin);
			// Lazarus: if turn_rider is set, do it. We don't do this by default
			//          'cause it can be a fairly drastic change in gameplay
			if (turn && (check->groundentity == pusher))
			{
				if(!check->client)
				{
					check->s.angles[YAW] += amove[YAW];
				}
				else
				{
					if(amove[YAW] != 0.)
					{
						check->client->ps.pmove.delta_angles[YAW] += ANGLE2SHORT(amove[YAW]);
						check->client->ps.viewangles[YAW] += amove[YAW];

						// PM_FREEZE makes the turn smooth, even though it will
						// be turned off by ClientThink in the very next video frame
						check->client->ps.pmove.pm_type = PM_FREEZE;
						// PMF_NO_PREDICTION overrides .exe's client physics, which
						// really doesn't like for us to change player angles. Note
						// that this isn't strictly necessary, since Lazarus 1.7 and
						// later automatically turn prediction off (in ClientThink) when 
						// player is riding a MOVETYPE_PUSH
						check->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
					}
					if(amove[PITCH] != 0.)
					{
						float	delta_yaw;
						float	pitch = amove[PITCH];

						delta_yaw = check->s.angles[YAW] - pusher->s.angles[YAW];
						delta_yaw *= M_PI / 180.;
						pitch *= cos(delta_yaw);
						check->client->ps.pmove.delta_angles[PITCH] += ANGLE2SHORT(pitch);
						check->client->ps.viewangles[PITCH] += pitch;
						check->client->ps.pmove.pm_type = PM_FREEZE;
						check->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
					}
				}
			}

			// Lazarus: This is where we attempt to move check due to a rotation, WITHOUT embedding
			//          check in pusher (or anything else)
			if(check->groundentity == pusher)
			{
				if((amove[PITCH] != 0) || (amove[YAW] != 0) || (amove[ROLL] != 0))
				{
					// figure movement due to the pusher's amove
					VectorAdd(check->s.origin,check->origin_offset,org_check);
					VectorSubtract (org_check, pusher->s.origin, org);
					org2[0] = DotProduct (org, forward);
					org2[1] = -DotProduct (org, right);
					org2[2] = DotProduct (org, up);
					VectorSubtract (org2, org, move2);
					VectorAdd (check->s.origin, move2, check->s.origin);
					if((amove[PITCH] != 0) || (amove[ROLL] != 0))
					{
						VectorCopy(check->s.origin,org);
						org[2] += 2*check->mins[2];
						tr = SV_Trace(check->s.origin,vec3_origin,vec3_origin,org,check,MASK_SOLID);
						if(!tr.startsolid && tr.fraction < 1)
							check->s.origin[2] = tr.endpos[2] - check->mins[2] + fabs(tr.plane.normal[0])*check->size[0]/2 + fabs(tr.plane.normal[1])*check->size[1]/2;
						
						// Lazarus: func_tracktrain is a special case. Since we KNOW (if the map was
						//          constructed properly) that "move_origin" is a safe position, we
						//          can infer that there should be a safe (not embedded) position
						//          somewhere between move_origin and the proposed new location.
						if((pusher->flags & FL_TRACKTRAIN) && (check->client || (check->svflags & SVF_MONSTER)))
						{
							vec3_t	f,l,u;
							
							AngleVectors(pusher->s.angles, f, l, u);
							VectorScale(f,pusher->move_origin[0],f);
							VectorScale(l,-pusher->move_origin[1],l);
							VectorAdd(pusher->s.origin,f,org);
							VectorAdd(org,l,org);
							org[2] += pusher->move_origin[2] + 1;
							org[2] += 16 * ( fabs(u[0]) + fabs(u[1]) );
							tr = SV_Trace(org,check->mins,check->maxs,check->s.origin,check,MASK_SOLID);
							if(!tr.startsolid)
							{
								VectorCopy(tr.endpos,check->s.origin);
								VectorCopy(check->s.origin,org);
								org[2] -= 128;
								tr = SV_Trace(check->s.origin,check->mins,check->maxs,org,check,MASK_SOLID);
								if(tr.fraction > 0)
									VectorCopy(tr.endpos,check->s.origin);
							}
						}
					}
				}
			}
			
			// may have pushed them off an edge
			if (check->groundentity != pusher) check->groundentity = NULL;

			block = SV_TestEntityPosition (check);

			if (block && (pusher->flags & FL_TRACKTRAIN) && (check->client || (check->svflags & SVF_MONSTER)) && (check->groundentity == pusher) )
			{
				// Lazarus: Last hope. If this doesn't get rider out of the way he's
				// gonna be stuck.
				vec3_t	f,l,u;

				AngleVectors(pusher->s.angles, f, l, u);
				VectorScale(f,pusher->move_origin[0],f);
				VectorScale(l,-pusher->move_origin[1],l);
				VectorAdd(pusher->s.origin,f,org);
				VectorAdd(org,l,org);
				org[2] += pusher->move_origin[2] + 1;
				org[2] += 16 * ( fabs(u[0]) + fabs(u[1]) );
				tr = SV_Trace(org,check->mins,check->maxs,check->s.origin,check,MASK_SOLID);
				if(!tr.startsolid)
				{
					VectorCopy(tr.endpos,check->s.origin);
					VectorCopy(check->s.origin,org);
					org[2] -= 128;
					tr = SV_Trace(check->s.origin,check->mins,check->maxs,org,check,MASK_SOLID);
					if(tr.fraction > 0)
						VectorCopy(tr.endpos,check->s.origin);
					block = SV_TestEntityPosition (check);
				}
			}

			if (!block)
			{	// pushed ok
				SV_LinkEdict (check);
				// Lazarus: Move check riders, and riders of riders, and... well, you get the pic
				VectorAdd(move, move2, move3);
				MoveRiders_r(check,NULL,move3,amove,turn);
				continue; // impact?
			}

			// if it is ok to leave in the old position, do it
			// this is only relevent for riding entities, not pushed
			VectorSubtract (check->s.origin, move,  check->s.origin);
			VectorSubtract (check->s.origin, move2, check->s.origin);
			if(turn)
			{
				// Argh! - angle
				check->s.angles[YAW] -= amove[YAW];
				if(check->client)
				{
					check->client->ps.pmove.delta_angles[YAW] -= ANGLE2SHORT(amove[YAW]);
					check->client->ps.viewangles[YAW] -= amove[YAW];
				}
			}

			block = SV_TestEntityPosition (check);
			if (!block)
			{
				pushed_p--;
				continue;
			}
		}
		
		// save off the obstacle so we can call the block function
		obstacle = check;

		// move back any entities we already moved
		// go backwards, so if the same entity was pushed
		// twice, it goes back to the original position
		for (p=pushed_p-1 ; p>=pushed ; p--)
		{
			VectorCopy (p->origin, p->ent->s.origin);
			VectorCopy (p->angles, p->ent->s.angles);
			if (p->ent->client)
			{
				p->ent->client->ps.pmove.delta_angles[YAW] = p->deltayaw;
			}
			SV_LinkEdict (p->ent);
		}
		return false;
	}

	//FIXME: is there a better way to handle this?
	// see if anything we moved has touched a trigger
	for (p = pushed_p - 1; p >= pushed; p--)
		SV_TouchTriggers (p->ent);
	return true;
}

/*
================
SV_Physics_Pusher

Bmodel objects don't interact with each other, but
push all box objects
================
*/
void SV_Physics_Pusher (edict_t *ent)
{
	vec3_t		move, amove;
	edict_t		*part = ent;

	// make sure all team slaves can move before commiting
	// any moves or calling any think functions
	// if the move is blocked, all moved objects will be backed out
	pushed_p = pushed;

	if (part->velocity[0] || part->velocity[1] || part->velocity[2] || part->avelocity[0] || part->avelocity[1] || part->avelocity[2])
	{	
		// object is moving
		VectorScale (part->velocity, 0.1f, move);
		VectorScale (part->avelocity, 0.1f, amove);

		SV_Push(part, move, amove); // move was blocked
	}

	if (pushed_p > &pushed[MAX_EDICTS])
		PF_error("pushed_p > &pushed[MAX_EDICTS], memory corrupted");

	SV_RunThink (part);
}

//==================================================================

/*
=============
SV_Physics_None

Non moving objects can only think
=============
*/
void SV_Physics_None (edict_t *ent)
{
	// regular thinking
	SV_RunThink (ent);
}

/*
=============
SV_Physics_Noclip

A moving object that doesn't obey physics
=============
*/
void SV_Physics_Noclip (edict_t *ent)
{
	// regular thinking
	if (!SV_RunThink (ent)) return;
	
	VectorMA (ent->s.angles, 0.1f, ent->avelocity, ent->s.angles);
	VectorMA (ent->s.origin, 0.1f, ent->velocity, ent->s.origin);

	SV_LinkEdict(ent);
}

/*
==============================================================================

TOSS / BOUNCE

==============================================================================
*/

/*
=============
SV_Physics_Toss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
void SV_Physics_Toss (edict_t *ent)
{
	trace_t		trace;
	vec3_t		move;
	float		backoff;
	bool		wasinwater;
	bool		isinwater;
	vec3_t		old_origin;

	// regular thinking
	SV_RunThink (ent);

	if (ent->groundentity) wasonground = true;

	if (ent->velocity[2] > 0) ent->groundentity = NULL;

	// check for the groundentity going away
	if (ent->groundentity)
		if (!ent->groundentity->inuse)
			ent->groundentity = NULL;

	// Lazarus: conveyor
	if (ent->groundentity && (ent->groundentity->movetype == MOVETYPE_CONVEYOR))
	{
		vec3_t	point, end;
		trace_t	tr;
		edict_t	*ground = ent->groundentity;

		VectorCopy(ent->s.origin,point);
		point[2] += 1;
		VectorCopy(point,end);
		end[2] -= 256;
		tr = SV_Trace (point, ent->mins, ent->maxs, end, ent, MASK_SOLID);
		// tr.ent HAS to be ground, but just in case we screwed something up:
		if(tr.ent == ground)
		{
			onconveyor = true;
			ent->velocity[0] = ground->movedir[0] * ground->speed;
			ent->velocity[1] = ground->movedir[1] * ground->speed;
			if(tr.plane.normal[2] > 0)
			{
				ent->velocity[2] = ground->speed * sqrt(1.0 - tr.plane.normal[2]*tr.plane.normal[2]) / tr.plane.normal[2];
				if(DotProduct(ground->movedir,tr.plane.normal) > 0)
				{
					// then we're moving down
					ent->velocity[2] = -ent->velocity[2];
				}
			}
			VectorScale (ent->velocity, 0.1f, move);
			trace = SV_PushEntity (ent, move);
			if (!ent->inuse) return;
			SV_CheckGround(ent);
		}
	}

	// if onground, return without moving
	if ( ent->groundentity )
		return;

	VectorCopy (ent->s.origin, old_origin);

	SV_CheckVelocity (ent);

	// add gravity
	if (ent->movetype != MOVETYPE_FLY)
		SV_AddGravity (ent);

	// move angles
	VectorMA (ent->s.angles, 0.1f, ent->avelocity, ent->s.angles);

	// move origin
	VectorScale (ent->velocity, 0.1f, move);
	trace = SV_PushEntity (ent, move);
	if (!ent->inuse)
		return;

	if (trace.fraction < 1 )
	{
		if (ent->movetype == MOVETYPE_BOUNCE)
			backoff = 1.0 + 0.5f;
		else if(trace.plane.normal[2] <= 0.7)	// Lazarus - don't stop on steep incline
			backoff = 1.5;
		else
			backoff = 1;

		ClipVelocity (ent->velocity, trace.plane.normal, ent->velocity, backoff);

		// stop if on ground
		if (trace.plane.normal[2] > 0.7)
		{		
			if (ent->velocity[2] < 60.0f || (ent->movetype != MOVETYPE_BOUNCE) )
			{
				ent->groundentity = trace.ent;
				ent->groundentity_linkcount = trace.ent->linkcount;
				VectorCopy (vec3_origin, ent->velocity);
				VectorCopy (vec3_origin, ent->avelocity);
			}
		}
	}

	// check for water transition
	wasinwater = (ent->watertype & MASK_WATER);
	ent->watertype = SV_PointContents (ent->s.origin);
	isinwater = ent->watertype & MASK_WATER;

	if (isinwater) ent->waterlevel = 1;
	else ent->waterlevel = 0;

	if (!wasinwater && isinwater)
		SV_StartSound(old_origin, ge->edicts, CHAN_AUTO, SV_SoundIndex("misc/h2ohit1.wav"), 1, 1, 0);
	else if (wasinwater && !isinwater)
		SV_StartSound(ent->s.origin, ge->edicts, CHAN_AUTO, SV_SoundIndex("misc/h2ohit1.wav"), 1, 1, 0);
}

/*
===============================================================================

STEPPING MOVEMENT

===============================================================================
*/

/*
=============
SV_Physics_Step

Monsters freefall when they don't have a ground entity, otherwise
all movement is done with discrete steps.

This is also used for objects that have become still on the ground, but
will fall if the floor is pulled out from under them.
FIXME: is this true?
=============
*/
//FIXME: hacked in for E3 demo
#define sv_stopspeed		100
#define sv_friction			6
#define sv_waterfriction		1

void SV_AddRotationalFriction (edict_t *ent)
{
	int		n;
	float		adjustment;

	VectorMA (ent->s.angles, 0.1f, ent->avelocity, ent->s.angles);
	adjustment = 0.1f * sv_stopspeed * sv_friction;

	for (n = 0; n < 3; n++)
	{
		if (ent->avelocity[n] > 0)
		{
			ent->avelocity[n] -= adjustment;
			if (ent->avelocity[n] < 0)
				ent->avelocity[n] = 0;
		}
		else
		{
			ent->avelocity[n] += adjustment;
			if (ent->avelocity[n] > 0)
				ent->avelocity[n] = 0;
		}
	}
}

#define WATER_DENSITY 0.00190735

float RiderMass(edict_t *platform)
{
	float		mass = 0;
	int		i;
	edict_t		*rider;
	trace_t		trace;
	vec3_t		point;

	for(i = 1, rider = EDICT_NUM(i); i <= ge->num_edicts; i++, rider = EDICT_NUM(i)) 
	{
		if(rider == platform) continue;
		if(!rider->inuse) continue;
		if(rider->groundentity == platform)
		{
			mass += rider->mass;
			mass += RiderMass(rider);
		}
		else if (rider->movetype == MOVETYPE_PUSHABLE )
		{
			// Bah - special case for func_pushable riders. Swimming
			// func_pushables don't really have a groundentity, even
			// though they may be sitting on another swimming
			// func_pushable, which is what we need to know.
			VectorCopy(rider->s.origin,point);
			point[2] -= 0.25;
			trace = SV_Trace (rider->s.origin, rider->mins, rider->maxs, point, rider, MASK_MONSTERSOLID);
			if ( trace.plane.normal[2] < 0.7 && !trace.startsolid)
				continue;
			if (!trace.startsolid && !trace.allsolid)
			{
				if(trace.ent == platform)
				{
					mass += rider->mass;
					mass += RiderMass(rider);
				}
			}
		}
	}
	return mass;
}

void SV_Physics_Step (edict_t *ent)
{
	bool		hitsound = false;
	float		*vel;
	float		speed, newspeed, control;
	float		friction;
	edict_t		*ground;
	edict_t     	*e;
	int         	cont;
	int		mask;
	int         	i;
	int		oldwaterlevel;
	vec3_t      	point, end;
	vec3_t		old_origin, move;

	// airborne monsters should always check for ground
	if (!ent->groundentity) SV_CheckGround (ent);

	oldwaterlevel = ent->waterlevel;

	VectorCopy(ent->s.origin, old_origin);

	// Lazarus: If density hasn't been calculated yet, do so now
	if (ent->mass > 0 && ent->density == 0.)
	{
		ent->volume = ent->size[0] * ent->size[1] * ent->size[2];
		ent->density = ent->mass/ent->volume;

		if(ent->movetype == MOVETYPE_PUSHABLE)
		{
			// This stuff doesn't apply to anything else, and... heh...
			// caused monster_flipper to sink

			ent->bob      = min(2.0,300.0/ent->mass);
			ent->duration = max(2.0,1.0 + ent->mass/100);
			
			// Figure out neutral bouyancy line for this entity
			// This isn't entirely realistic, but helps gameplay:
			// Arbitrary mass limit for func_pushable that can be pushed on
			// land is 500. So make a mass=500+, 64x64x64 crate sink.
			// (Otherwise, player might cause a 501 crate to leave
			// water and expect to be able to push it.)
			// Max floating density is then 0.0019073486328125
			if(ent->density > WATER_DENSITY)
				ent->flags &= ~FL_SWIM; // sinks like a rock
		}
	}

	// If not a monster, then determine whether we're in water.
	// (monsters take care of this in g_monster.c)
	if (!(ent->svflags & SVF_MONSTER) && (ent->flags && FL_SWIM) )
	{
		point[0] = (ent->absmax[0] + ent->absmin[0])/2;
		point[1] = (ent->absmax[1] + ent->absmin[1])/2;
		point[2] = ent->absmin[2] + 1;
		cont = SV_PointContents (point);
		if (!(cont & MASK_WATER))
		{
			ent->waterlevel = 0;
			ent->watertype = 0;
		}
		else {
			ent->watertype = cont;
			ent->waterlevel = 1;
			point[2] = ent->absmin[2] + ent->size[2]/2;
			cont = SV_PointContents (point);
			if (cont & MASK_WATER)
			{
				ent->waterlevel = 2;
				point[2] = ent->absmax[2];
				cont = SV_PointContents (point);
				if (cont & MASK_WATER)
					ent->waterlevel = 3;
			}
		}
	}
	
	ground = ent->groundentity;

	SV_CheckVelocity (ent);

	if (ground) wasonground = true;
		
	if (ent->avelocity[0] || ent->avelocity[1] || ent->avelocity[2])
		SV_AddRotationalFriction (ent);

	// add gravity except:
	// flying monsters
	// swimming monsters who are in the water
	if (!wasonground)
	{
		if (!(ent->flags & FL_FLY))
		{
			if (!((ent->flags & FL_SWIM) && (ent->waterlevel > 2)))
			{
				if (ent->velocity[2] < sv_gravity->value*-0.1)
					hitsound = true;
				if (ent->waterlevel == 0)
					SV_AddGravity (ent);
			}
		}
	}

	// friction for flying monsters that have been given vertical velocity
	if ((ent->flags & FL_FLY) && (ent->velocity[2] != 0))
	{
		speed = fabs(ent->velocity[2]);
		control = speed < sv_stopspeed ? sv_stopspeed : speed;
		friction = sv_friction/3;
		newspeed = speed - (0.1f * control * friction);
		if (newspeed < 0) newspeed = 0;
		newspeed /= speed;
		ent->velocity[2] *= newspeed;
	}

	// friction for swimming monsters that have been given vertical velocity
	if (ent->movetype != MOVETYPE_PUSHABLE)
	{
		// Lazarus: This is id's swag at drag. It works mostly, but for submerged 
		//          crates we can do better.
		if ((ent->flags & FL_SWIM) && (ent->velocity[2] != 0))
		{
			speed = fabs(ent->velocity[2]);
			control = speed < sv_stopspeed ? sv_stopspeed : speed;
			newspeed = speed - (0.1f * control * sv_waterfriction * ent->waterlevel);
			if (newspeed < 0) newspeed = 0;
			newspeed /= speed;
			ent->velocity[2] *= newspeed;
		}
	}

	// Lazarus: Floating stuff
	if ((ent->movetype == MOVETYPE_PUSHABLE) && (ent->flags && FL_SWIM) && (ent->waterlevel))
	{
		float	waterlevel;
		float	rider_mass, total_mass;
		trace_t	tr;
		float	Accel, Area, Drag, Force;

		VectorCopy(point,end);
		if(ent->waterlevel < 3)
		{
			point[2] = ent->absmax[2];
			end[2]   = ent->absmin[2];
			tr = SV_Trace(point,NULL,NULL,end,ent,MASK_WATER);
			waterlevel = tr.endpos[2];
		}
		else
		{
			// Not right, but really all we need to know
			waterlevel = ent->absmax[2] + 1;
		}
		rider_mass = RiderMass(ent);
		total_mass = rider_mass + ent->mass;
		Area  = ent->size[0] * ent->size[1];
		if(waterlevel < ent->absmax[2])
		{
			// A portion of crate is above water
			int	time;
			float	t0, t1, z0, z1;

			// For partially submerged crates, use same psuedo-friction thing used
			// on other entities. This isn't really correct, but then neither is
			// our drag calculation used for fully submerged crates good for this
			// situation
			if (ent->velocity[2] != 0)
			{
				speed = fabs(ent->velocity[2]);
				control = speed < sv_stopspeed ? sv_stopspeed : speed;
				newspeed = speed - (0.1f * control * sv_waterfriction * ent->waterlevel);
				if (newspeed < 0) newspeed = 0;
				newspeed /= speed;
				ent->velocity[2] *= newspeed;
			}

			// Apply physics and bob AFTER friction, or the damn thing will never move.
			Force = -total_mass + ((waterlevel-ent->absmin[2]) * Area * WATER_DENSITY);
			Accel = Force * sv_gravity->value/total_mass;
			ent->velocity[2] += Accel*0.1f;

			time = ent->duration*10;
			t0 = ent->bobframe%time;
			t1 = (ent->bobframe+1)%time;
			z0 = sin(2*M_PI*t0/time);
			z1 = sin(2*M_PI*t1/time);
			ent->velocity[2] += ent->bob * (z1-z0) * 10;
			ent->bobframe = (ent->bobframe+1)%time;
		} 
		else
		{
			// Crate is fully submerged
			Force = -total_mass + ent->volume * WATER_DENSITY;
			if(sv_gravity->value)
			{
				Drag  = 0.00190735 * 1.05 * Area * (ent->velocity[2]*ent->velocity[2])/sv_gravity->value;
				if(Drag > fabs(Force))
				{
					// Drag actually CAN be > total weight, but if we do this we tend to
					// get crates flying back out of the water after being dropped from some
					// height
					Drag = fabs(Force);
				}
				if(ent->velocity[2] > 0)
					Drag = -Drag;
				Force += Drag;
			}
			Accel = Force * sv_gravity->value/total_mass;
			ent->velocity[2] += Accel*0.1f;
		}

		if(ent->watertype & MASK_CURRENT)
		{
			// Move with current, relative to mass. Mass=400 or less
			// will move at 50 units/sec.
			float v;
			int   current;

			if(ent->mass > 400) v = 0.125 * ent->mass;
			else v = 50.;
			current = ent->watertype & MASK_CURRENT;
			switch (current)
			{
			case CONTENTS_CURRENT_0:    ent->velocity[0] = v;  break;
			case CONTENTS_CURRENT_90:   ent->velocity[1] = v;  break;
			case CONTENTS_CURRENT_180:  ent->velocity[0] = -v; break;
			case CONTENTS_CURRENT_270:  ent->velocity[1] = -v; break;
			case CONTENTS_CURRENT_UP :  ent->velocity[2] = max(v, ent->velocity[2]);
			case CONTENTS_CURRENT_DOWN: ent->velocity[2] = min(-v, ent->velocity[2]);
			}
		}
	}

	// Conveyor
	if (wasonground && (ground->movetype == MOVETYPE_CONVEYOR))
	{
		trace_t	tr;

		VectorCopy(ent->s.origin,point);
		point[2] += 1;
		VectorCopy(point,end);
		end[2] -= 256;
		tr = SV_Trace (point, ent->mins, ent->maxs, end, ent, MASK_SOLID);
		// tr.ent HAS to be ground, but just in case we screwed something up:
		if(tr.ent == ground)
		{
			onconveyor = true;
			ent->velocity[0] = ground->movedir[0] * ground->speed;
			ent->velocity[1] = ground->movedir[1] * ground->speed;
			if(tr.plane.normal[2] > 0)
			{
				ent->velocity[2] = ground->speed * sqrt(1.0 - tr.plane.normal[2]*tr.plane.normal[2]) / tr.plane.normal[2];
				if(DotProduct(ground->movedir,tr.plane.normal) > 0)
				{
					// Then we're moving down.
					ent->velocity[2] = -ent->velocity[2] + 2;
				}
			}
		}
	}

	if (ent->velocity[2] || ent->velocity[1] || ent->velocity[0])
	{
		int	block;
		// apply friction
		// let dead monsters who aren't completely onground slide

		if ((wasonground) || (ent->flags & (FL_SWIM|FL_FLY)))

			if (!onconveyor)
			{
				if (!(ent->health <= 0.0 && !SV_CheckBottom(ent)))
				{
					vel = ent->velocity;
					speed = sqrt(vel[0]*vel[0] +vel[1]*vel[1]);
					if (speed)
					{
						friction = sv_friction;
	
						control = speed < sv_stopspeed ? sv_stopspeed : speed;
						newspeed = speed - 0.1f*control*friction;
	
						if (newspeed < 0) newspeed = 0;
						newspeed /= speed;
	
						vel[0] *= newspeed;
						vel[1] *= newspeed;
					}
				}
			}

		if (ent->svflags & SVF_MONSTER) mask = MASK_MONSTERSOLID;
		else if(ent->movetype == MOVETYPE_PUSHABLE) mask = MASK_MONSTERSOLID | MASK_PLAYERSOLID;
		else if(ent->clipmask) mask = ent->clipmask; // Lazarus edition
		else mask = MASK_SOLID;

		if (ent->movetype == MOVETYPE_PUSHABLE)
		{
			block = SV_PushableMove (ent, 0.1f, mask);
			if(block && !(block & 8) && onconveyor)
			{
				T_Damage(ent, ge->edicts, ge->edicts, vec3_origin, ent->s.origin, vec3_origin, 100000, 1, 0, DMG_CRUSH);
				if(!ent->inuse) return;
			}
		}
		else
		{
			block = SV_FlyMove (ent, 0.1f, mask);
			if(block && !(block & 8) && onconveyor)
			{
				T_Damage (ent, ge->edicts, ge->edicts, vec3_origin, ent->s.origin, vec3_origin, 100000, 1, 0, DMG_CRUSH);
				if(!ent->inuse) return;
			}
		}
		SV_LinkEdict (ent);
		SV_TouchTriggers (ent);
		if (!ent->inuse) return;

		if (ent->groundentity && !wasonground && hitsound)
		{
			PF_StartSound (ent, 0, SV_SoundIndex("world/land.wav"), 1, 1, 0);
		}

		// Move func_pushable riders
		if(ent->movetype == MOVETYPE_PUSHABLE)
		{
			trace_t	tr;

			if(ent->bouncetype == 2) VectorMA(old_origin,0.1f,ent->velocity,ent->s.origin);
			VectorSubtract(ent->s.origin,old_origin,move);
			for(i = 1, e = EDICT_NUM(i); i < ge->num_edicts; i++, e = EDICT_NUM(i))
			{
				if(e==ent) continue;
				if(e->groundentity == ent)
				{
					VectorAdd(e->s.origin,move,end);
					tr = SV_Trace(e->s.origin, e->mins, e->maxs, end, ent, MASK_SOLID);
					VectorCopy(tr.endpos,e->s.origin);
					SV_LinkEdict(e);
				}
			}
		}
	}
	else if(ent->movetype == MOVETYPE_PUSHABLE)
	{
		// We run touch function for non-moving func_pushables every frame
		// to see if they are touching, for example, a trigger_mass
		SV_TouchTriggers(ent);
		if(!ent->inuse) return;
	}


	// Lazarus: Add falling damage for entities that can be damaged
	if( ent->takedamage )
	{
		SV_FallingDamage(ent);
		VectorCopy(ent->velocity,ent->oldvelocity);
	}

	if ((!oldwaterlevel && ent->waterlevel) && !ent->groundentity)
	{
		if( (ent->watertype & CONTENTS_SLIME) || (ent->watertype & CONTENTS_WATER) )
			PF_StartSound (ent, CHAN_BODY, SV_SoundIndex("player/watr_in.wav"), 1, ATTN_NORM, 0);
		else if(ent->watertype & CONTENTS_MUD)
			PF_StartSound (ent, CHAN_BODY, SV_SoundIndex("mud/mud_in2.wav"), 1, ATTN_NORM, 0);
	}

	// regular thinking
	SV_RunThink (ent);
	VectorCopy(ent->velocity,ent->oldvelocity);
}

//============================================================================
/*
============
SV_DebrisEntity

Does not change the entities velocity at all
============
*/
trace_t SV_DebrisEntity (edict_t *ent, vec3_t push)
{
	trace_t		trace;
	vec3_t		start;
	vec3_t		end;
	vec3_t		v1, v2;
	vec_t		dot, speed1, speed2;
	float		scale;
	int		damage;
	int		mask;

	VectorCopy (ent->s.origin, start);
	VectorAdd (start, push, end);
	if(ent->clipmask) mask = ent->clipmask;
	else mask = MASK_SHOT;

	trace = SV_Trace (start, ent->mins, ent->maxs, end, ent, mask);
	VectorCopy (trace.endpos, ent->s.origin);
	SV_LinkEdict (ent);

	if (trace.fraction != 1.0)
	{
		if( (trace.surface) && (trace.surface->flags & SURF_SKY))
		{
			SV_FreeEdict(ent);
			return trace;
		}
		if(trace.ent->client || (trace.ent->svflags & SVF_MONSTER) )
		{
			// touching a player or monster
			// if rock has no mass we really don't care who it hits
			if(!ent->mass) return trace;
			speed1 = VectorLength(ent->velocity);
			if(!speed1) return trace;
			speed2 = VectorLength(trace.ent->velocity);
			VectorCopy(ent->velocity,v1);
			VectorNormalize(v1);
			VectorCopy(trace.ent->velocity,v2);
			VectorNormalize(v2);
			dot = -DotProduct(v1,v2);
			speed1 += speed2 * dot;
			if(speed1 <= 0) return trace;
			scale = (float)ent->mass/200.*speed1;
			VectorMA(trace.ent->velocity,scale,v1,trace.ent->velocity);
			// Take a swag at it... 

			if(speed1 > 100)
			{
				damage = (int)(ent->mass * speed1 / 5000.);
				if(damage) T_Damage(trace.ent, ge->edicts, ge->edicts, v1, trace.ent->s.origin, vec3_origin, damage, 0, 0, DMG_CRUSH);
			}
			if(ent->touch) ent->touch (ent, trace.ent, &trace.plane, trace.surface);

			SV_LinkEdict(trace.ent);
		}
		else
		{
			SV_Impact (ent, &trace);
		}
	}
	return trace;
}					

/*
=============
SV_Physics_Debris

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
void SV_Physics_Debris (edict_t *ent)
{
	trace_t		trace;
	vec3_t		move;
	float		backoff;
	bool		wasinwater;
	bool		isinwater;
	vec3_t		old_origin;

	// regular thinking
	SV_RunThink (ent);

	if (ent->velocity[2] > 0)
		ent->groundentity = NULL;

	// check for the groundentity going away
	if (ent->groundentity)
		if (!ent->groundentity->inuse)
			ent->groundentity = NULL;

	// if onground, return without moving
	if ( ent->groundentity )
		return;

	VectorCopy (ent->s.origin, old_origin);
	SV_CheckVelocity (ent);
	SV_AddGravity (ent);

	// move angles
	VectorMA (ent->s.angles, 0.1f, ent->avelocity, ent->s.angles);

	// move origin
	VectorScale (ent->velocity, 0.1f, move);
	trace = SV_DebrisEntity (ent, move);
	if (!ent->inuse)
		return;

	if (trace.fraction < 1)
	{
		backoff = 1.0;
		ClipVelocity (ent->velocity, trace.plane.normal, ent->velocity, backoff);

		// stop if on ground
		if (trace.plane.normal[2] > 0.3)
		{		
			if (ent->velocity[2] < 60)
			{
				ent->groundentity = trace.ent;
				ent->groundentity_linkcount = trace.ent->linkcount;
				VectorCopy (vec3_origin, ent->velocity);
				VectorCopy (vec3_origin, ent->avelocity);
			}
		}
	}
	
	// check for water transition
	wasinwater = (ent->watertype & MASK_WATER);
	ent->watertype = SV_PointContents (ent->s.origin);
	isinwater = ent->watertype & MASK_WATER;

	if (isinwater) ent->waterlevel = 1;
	else ent->waterlevel = 0;

	if (!wasinwater && isinwater) SV_StartSound (old_origin, ge->edicts, CHAN_AUTO, SV_SoundIndex("misc/h2ohit1.wav"), 1, 1, 0);
	else if (wasinwater && !isinwater) SV_StartSound (ent->s.origin, ge->edicts, CHAN_AUTO, SV_SoundIndex("misc/h2ohit1.wav"), 1, 1, 0);

}

/*
====================
SV_Physics_Conveyor

REAL simple - all we do is check for player riders and adjust their position.
Only gotcha here is we have to make sure we don't end up embedding player in
*another* object that's being moved by the conveyor.

====================
*/
void SV_Physics_Conveyor(edict_t *ent)
{
	edict_t		*player;
	int		i;
	trace_t		tr;
	vec3_t		v, move;
	vec3_t		point, end;

	VectorScale(ent->movedir,ent->speed,v);
	VectorScale(v, 0.1f, move);

	for(i = 0; i < maxclients->value; i++)
	{
		player = EDICT_NUM(i) + 1;
		if(!player->inuse) continue;
		if(!player->groundentity) continue;
		if(player->groundentity != ent) continue;

		// Look below player; make sure he's on a conveyor
		VectorCopy(player->s.origin,point);
		point[2] += 1;
		VectorCopy(point,end);
		end[2] -= 256;

		tr = SV_Trace (point, player->mins, player->maxs, end, player, MASK_SOLID);
		// tr.ent HAS to be conveyor, but just in case we screwed something up:
		if(tr.ent == ent)
		{
			if(tr.plane.normal[2] > 0)
			{
				v[2] = ent->speed * sqrt(1.0 - tr.plane.normal[2]*tr.plane.normal[2]) / tr.plane.normal[2];
				if(DotProduct(ent->movedir,tr.plane.normal) > 0)
				{
					// then we're moving down
					v[2] = -v[2];
				}
				move[2] = v[2] * 0.1f;
			}
			VectorAdd(player->s.origin,move,end);
			tr = SV_Trace(player->s.origin,player->mins,player->maxs,end,player,player->clipmask);
			VectorCopy(tr.endpos,player->s.origin);
			SV_LinkEdict(player);
		}
	}
}

void SV_Physics(edict_t *ent)
{
	if (ent->prethink) ent->prethink (ent);
	wasonground = false;
	onconveyor = false;

	switch ( (int)ent->movetype)
	{
	case MOVETYPE_PUSH:
		SV_Physics_Pusher (ent);
		break;
	case MOVETYPE_NONE:
		SV_Physics_None (ent);
		break;
	case MOVETYPE_NOCLIP:
		SV_Physics_Noclip (ent);
		break;
	case MOVETYPE_STEP:
		SV_Physics_Step (ent);
		break;
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
	case MOVETYPE_FLY:
		SV_Physics_Toss (ent);
		break;
	case MOVETYPE_DEBRIS:
		SV_Physics_Debris (ent);
		break;
	case MOVETYPE_WALK:
		SV_Physics_None(ent);
		break;
	case MOVETYPE_CONVEYOR:
		SV_Physics_Conveyor(ent);
		break;
	default:
		PF_error ("SV_Physics: bad movetype %i", (int)ent->movetype);			
	}
}