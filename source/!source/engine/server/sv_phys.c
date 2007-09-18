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
	
	VectorAdd (ent->priv.sv->s.origin, ent->progs.sv->mins, mins);
	VectorAdd (ent->priv.sv->s.origin, ent->progs.sv->maxs, maxs);

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

	VectorClear(ent->progs.sv->origin_offset);
	ent->progs.sv->solid = SOLID_BBOX;
	ent->priv.sv->clipmask |= MASK_MONSTERSOLID;
	if(!ent->progs.sv->health) ent->progs.sv->health = 20;
	ent->progs.sv->takedamage = DAMAGE_YES;

	ent->progs.sv->movetype = MOVETYPE_TOSS;  

	VectorSet(v, 0, 0, -128);
	VectorAdd (ent->priv.sv->s.origin, v, dest);

	tr = SV_Trace (ent->priv.sv->s.origin, ent->progs.sv->mins, ent->progs.sv->maxs, dest, ent, MASK_SOLID);
	if (tr.startsolid)
	{
		Msg("SV_DropToFloor: %s startsolid at %s\n", PRVM_G_STRING(ent->progs.sv->classname), ent->priv.sv->s.origin[0], ent->priv.sv->s.origin[1], ent->priv.sv->s.origin[2]);
		SV_FreeEdict (ent);
		return;
	}
	tr.endpos[2] += 1;
	ent->progs.sv->mins[2] -= 1;
	ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG(tr.ent);
	VectorCopy (tr.endpos, ent->priv.sv->s.origin);

	SV_LinkEdict (ent);
}

void SV_CheckGround (edict_t *ent)
{
	vec3_t		point;
	trace_t		trace;

	if ((int)ent->progs.sv->aiflags & (AI_SWIM|AI_FLY)) return;
	if (ent->progs.sv->velocity[2] > 100)
	{
		ent->progs.sv->groundentity = 0;
		return;
	}

	// if the hull point one-quarter unit down is solid the entity is on ground
	point[0] = ent->priv.sv->s.origin[0];
	point[1] = ent->priv.sv->s.origin[1];
	point[2] = ent->priv.sv->s.origin[2] - 0.25;

	trace = SV_Trace (ent->priv.sv->s.origin, ent->progs.sv->mins, ent->progs.sv->maxs, point, ent, MASK_MONSTERSOLID);

	// check steepness
	if ( trace.plane.normal[2] < 0.7 && !trace.startsolid)
	{
		ent->progs.sv->groundentity = 0;
		return;
	}

	// Lazarus: The following 2 lines were in the original code and commented out
	//          by id. However, the effect of this is that a player walking over
	//          a dead monster who is laying on a brush model will cause the 
	//          dead monster to drop through the brush model. This change *may*
	//          have other consequences, though, so watch out for this.

	ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
	if (!trace.startsolid && !trace.allsolid)
	{
		VectorCopy (trace.endpos, ent->priv.sv->s.origin);
		ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
		ent->progs.sv->velocity[2] = trace.ent->progs.sv->velocity[2];
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

	if (ent->progs.sv->movetype == MOVETYPE_NOCLIP)
		return;

	if ((ent->progs.sv->post_velocity[2] < 0) && (ent->progs.sv->velocity[2] > ent->progs.sv->post_velocity[2]) && (!ent->progs.sv->groundentity))
	{
		delta = ent->progs.sv->post_velocity[2];
	}
	else
	{
		if (!ent->progs.sv->groundentity)
			return;
		delta = ent->progs.sv->velocity[2] - ent->progs.sv->post_velocity[2];
	}
	delta = delta*delta * 0.0001;

	// never take falling damage if completely underwater
	if (ent->progs.sv->waterlevel == 3) return;
	if (ent->progs.sv->waterlevel == 2) delta *= 0.25;
	if (ent->progs.sv->waterlevel == 1) delta *= 0.5;

	if (delta < 1) return;

	if (delta < 15)
	{
		ent->priv.sv->s.event = EV_FOOTSTEP;
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

		T_Damage (ent, prog->edicts, prog->edicts, dir, ent->priv.sv->s.origin, vec3_origin, damage, 0, 0, DMG_FALL);
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
	VectorCopy (ent->priv.sv->s.origin, oldorg);
	VectorAdd (ent->priv.sv->s.origin, move, neworg);

	AngleVectors(ent->priv.sv->s.angles,forward,NULL,up);
	if(ent->progs.sv->enemy)
		target = PRVM_PROG_TO_EDICT(ent->progs.sv->enemy);
	else if(ent->progs.sv->movetarget)
		target = PRVM_PROG_TO_EDICT(ent->progs.sv->movetarget);
	else
		target = NULL;

	// flying monsters don't step up
	if ((int)ent->progs.sv->aiflags & (AI_SWIM | AI_FLY) )
	{
		// try one move with vertical motion, then one without
		for (i = 0; i < 2; i++)
		{
			VectorAdd (ent->priv.sv->s.origin, move, neworg);
			if (i == 0 && ent->progs.sv->enemy)
			{
				if (!ent->progs.sv->goalentity)
					ent->progs.sv->goalentity = ent->progs.sv->enemy;
				dz = ent->priv.sv->s.origin[2] - PRVM_PROG_TO_EDICT(ent->progs.sv->goalentity)->priv.sv->s.origin[2];
				if (PRVM_PROG_TO_EDICT(ent->progs.sv->goalentity)->priv.sv->client)
				{
					if (dz > 40) neworg[2] -= 8;
					if (!(((int)ent->progs.sv->aiflags & AI_SWIM) && (ent->progs.sv->waterlevel < 2)))
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
			trace = SV_Trace (ent->priv.sv->s.origin, ent->progs.sv->mins, ent->progs.sv->maxs, neworg, ent, MASK_MONSTERSOLID);
	
			// fly monsters don't enter water voluntarily
			if ((int)ent->progs.sv->aiflags & AI_FLY)
			{
				if (!ent->progs.sv->waterlevel)
				{
					test[0] = trace.endpos[0];
					test[1] = trace.endpos[1];
					test[2] = trace.endpos[2] + ent->progs.sv->mins[2] + 1;
					contents = SV_PointContents(test);
					if (contents & MASK_WATER)
						return false;
				}
			}

			// swim monsters don't exit water voluntarily
			if ((int)ent->progs.sv->aiflags & AI_SWIM)
			{
				if (ent->progs.sv->waterlevel < 2)
				{
					test[0] = trace.endpos[0];
					test[1] = trace.endpos[1];
					test[2] = trace.endpos[2] + ent->progs.sv->mins[2] + 1;
					contents = SV_PointContents(test);
					if (!(contents & MASK_WATER))
						return false;
				}
			}

			if (trace.fraction == 1)
			{
				VectorCopy (trace.endpos, ent->priv.sv->s.origin);
				if (relink)
				{
					SV_LinkEdict(ent);
					SV_TouchTriggers (ent);
				}
				return true;
			}
			
			if (!ent->progs.sv->enemy)
				break;
		}
		
		return false;
	}

	// push down from a step height above the wished position
	if (!((int)ent->progs.sv->aiflags & AI_NOSTEP))
		stepsize = STEPSIZE;
	else stepsize = 1;

	neworg[2] += stepsize;
	VectorCopy (neworg, end);
	end[2] -= stepsize*2;

	trace = SV_Trace (neworg, ent->progs.sv->mins, ent->progs.sv->maxs, end, ent, MASK_MONSTERSOLID);

	// Determine whether monster is capable of and/or should jump
	jump = 0;
	if((ent->progs.sv->jump) && !((int)ent->progs.sv->aiflags & AI_DUCKED))
	{
		// Don't jump if path is blocked by monster or player. Otherwise,
		// monster might attempt to jump OVER the monster/player, which 
		// ends up looking a bit goofy. Also don't jump if the monster's
		// movement isn't deliberate (target=NULL)
		if(trace.ent && (trace.ent->priv.sv->client || ((int)trace.ent->progs.sv->flags & FL_MONSTER)))
			canjump = false;
		else if(target)
		{
			// Never jump unless it places monster closer to his goal
			vec3_t	dir;
			VectorSubtract(target->priv.sv->s.origin, oldorg, dir);
			d1 = VectorLength(dir);
			VectorSubtract(target->priv.sv->s.origin, trace.endpos, dir);
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
		if(canjump && (ent->progs.sv->jumpup > 0))
		{
			neworg[2] += ent->progs.sv->jumpup - stepsize;
			trace = SV_Trace (neworg, ent->progs.sv->mins, ent->progs.sv->maxs, end, ent, MASK_MONSTERSOLID);
			if (!trace.allsolid && !trace.startsolid && trace.fraction > 0 && (trace.plane.normal[2] > 0.9))
			{
				if(!trace.ent || (!trace.ent->priv.sv->client && !((int)trace.ent->progs.sv->flags & FL_MONSTER) && !((int)trace.ent->progs.sv->flags & FL_DEADMONSTER)))
				{
					// Good plane to jump on. Make sure monster is more or less facing
					// the obstacle to avoid cutting-corners jumps
					trace_t	tr;
					vec3_t	p2;

					VectorMA(ent->priv.sv->s.origin,1024,forward,p2);
					tr = SV_Trace(ent->priv.sv->s.origin,ent->progs.sv->mins,ent->progs.sv->maxs,p2,ent,MASK_MONSTERSOLID);
					if(DotProduct(tr.plane.normal,forward) < -0.95)
					{
						jump = 1;
						jumpheight = trace.endpos[2] - ent->priv.sv->s.origin[2];
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
		trace = SV_Trace (neworg, ent->progs.sv->mins, ent->progs.sv->maxs, end, ent, MASK_MONSTERSOLID);
		if (trace.allsolid || trace.startsolid)
			return false;
	}


	// don't go in to water
	// Lazarus: misc_actors don't go swimming, but wading is fine
	if ((int)ent->progs.sv->aiflags & AI_ACTOR)
	{
		// First check for lava/slime under feet - but only if we're not already in
		// a liquid
		test[0] = trace.endpos[0];
		test[1] = trace.endpos[1];
		if (ent->progs.sv->waterlevel == 0)
		{
			test[2] = trace.endpos[2] + ent->progs.sv->mins[2] + 1;	
			contents = SV_PointContents(test);
			if (contents & (CONTENTS_LAVA | CONTENTS_SLIME))
				return false;
		}
		test[2] = trace.endpos[2] + ent->progs.sv->view_ofs[2] - 1;	//FIXME
		contents = SV_PointContents(test);
		if (contents & MASK_WATER)
			return false;
	}
	else if (ent->progs.sv->waterlevel == 0)
	{
		test[0] = trace.endpos[0];
		test[1] = trace.endpos[1];
		test[2] = trace.endpos[2] + ent->progs.sv->mins[2] + 1;	
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

		for(i = maxclients->value + 1; i < prog->num_edicts; i++)
		{
			e = PRVM_EDICT_NUM(i);
			if(e->priv.sv->free) continue;
			if(!PRVM_G_STRING(e->progs.sv->classname)) continue;
			if(strcmp(PRVM_G_STRING(e->progs.sv->classname), "env_laser")) continue;
			if(!PF_inpvs(ent->priv.sv->s.origin,e->priv.sv->s.origin))
				continue;
			// Check to see if monster is ALREADY in the path of this laser.
			// If so, allow the move so he can get out.
			VectorMA(e->priv.sv->s.origin, 2048, e->progs.sv->movedir, laser_end);
			laser_trace = SV_Trace(e->priv.sv->s.origin,NULL,NULL,laser_end,NULL,CONTENTS_SOLID|CONTENTS_MONSTER);
			if(laser_trace.ent == ent)
				continue;
			VectorCopy(laser_trace.endpos,laser_end);
			laser_mins[0] = min(e->priv.sv->s.origin[0],laser_end[0]);
			laser_mins[1] = min(e->priv.sv->s.origin[1],laser_end[1]);
			laser_mins[2] = min(e->priv.sv->s.origin[2],laser_end[2]);
			laser_maxs[0] = max(e->priv.sv->s.origin[0],laser_end[0]);
			laser_maxs[1] = max(e->priv.sv->s.origin[1],laser_end[1]);
			laser_maxs[2] = max(e->priv.sv->s.origin[2],laser_end[2]);
			monster_mins[0] = min(oldorg[0],trace.endpos[0]) + ent->progs.sv->mins[0];
			monster_mins[1] = min(oldorg[1],trace.endpos[1]) + ent->progs.sv->mins[1];
			monster_mins[2] = min(oldorg[2],trace.endpos[2]) + ent->progs.sv->mins[2];
			monster_maxs[0] = max(oldorg[0],trace.endpos[0]) + ent->progs.sv->maxs[0];
			monster_maxs[1] = max(oldorg[1],trace.endpos[1]) + ent->progs.sv->maxs[1];
			monster_maxs[2] = max(oldorg[2],trace.endpos[2]) + ent->progs.sv->maxs[2];
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
			if ( (e->progs.sv->movedir[0] == 1.) || (e->progs.sv->movedir[1] == 1.) || (e->progs.sv->movedir[2] == 1.))
				return false;
			// Shift psuedo laser towards monster's current position up to
			// the total distance he's proposing moving.
			delta = min(16,dist);
			VectorCopy(move,dir);
			VectorNormalize(dir);
			while(delta < dist+15.875)
			{
				if(delta > dist) delta = dist;
				VectorMA(e->priv.sv->s.origin,    -delta,dir,laser_start);
				VectorMA(e->priv.sv->s.old_origin,-delta,dir,laser_end);
				laser_trace = SV_Trace(laser_start,NULL,NULL,laser_end,prog->edicts,CONTENTS_SOLID|CONTENTS_MONSTER);
				if(laser_trace.ent == ent)
					return false;
				delta += 16;
			}
		}
	}
	if ((trace.fraction == 1) && !jump && canjump && (ent->progs.sv->jumpdn > 0))
	{
		end[2] = oldorg[2] + move[2] - ent->progs.sv->jumpdn;
		trace = SV_Trace (neworg, ent->progs.sv->mins, ent->progs.sv->maxs, end, ent, MASK_MONSTERSOLID | MASK_WATER);
		if(trace.fraction < 1 && (trace.plane.normal[2] > 0.9) && (trace.contents & MASK_SOLID) && (neworg[2] - 16 > trace.endpos[2]))
		{
			if(!trace.ent || (!trace.ent->priv.sv->client && !((int)trace.ent->progs.sv->flags & FL_MONSTER) && !((int)trace.ent->progs.sv->flags & FL_DEADMONSTER)))
				jump = -1;
		}
	}


	if ((trace.fraction == 1) && !jump)
	{
		// if monster had the ground pulled out, go ahead and fall
		if ((int)ent->progs.sv->aiflags & AI_PARTIALONGROUND )
		{
			VectorAdd (ent->priv.sv->s.origin, move, ent->priv.sv->s.origin);
			if (relink)
			{
				SV_LinkEdict(ent);
				SV_TouchTriggers (ent);
			}
			ent->progs.sv->groundentity = 0;
			return true;
		}
		return false;		// walked off an edge
	}

	// check point traces down for dangling corners
	VectorCopy (trace.endpos, ent->priv.sv->s.origin);

	if(!jump)
	{
		bool	skip = false;
		// if monster CAN jump down, and a position just a bit forward would be 
		// a good jump-down spot, allow (briefly) !M_CheckBottom
		if (canjump && target && (target->priv.sv->s.origin[2] < ent->priv.sv->s.origin[2]) && (ent->progs.sv->jumpdn > 0))
		{
			vec3_t		p1, p2;
			trace_t		tr;

			VectorMA(oldorg,48,forward,p1);
			tr = SV_Trace(ent->priv.sv->s.origin, ent->progs.sv->mins, ent->progs.sv->maxs, p1, ent, MASK_MONSTERSOLID);
			if(tr.fraction == 1)
			{
				p2[0] = p1[0];
				p2[1] = p1[1];
				p2[2] = p1[2] - ent->progs.sv->jumpdn;
				tr = SV_Trace(p1,ent->progs.sv->mins,ent->progs.sv->maxs,p2,ent,MASK_MONSTERSOLID | MASK_WATER);
				if(tr.fraction < 1 && (tr.plane.normal[2] > 0.9) && (tr.contents & MASK_SOLID) && (p1[2] - 16 > tr.endpos[2]))
				{
					if(!tr.ent || (!tr.ent->priv.sv->client && !((int)tr.ent->progs.sv->flags & FL_MONSTER) && !((int)tr.ent->progs.sv->flags & FL_DEADMONSTER)))
					{
						VectorSubtract(target->priv.sv->s.origin, tr.endpos, dir);
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
				if ( (int)ent->progs.sv->aiflags & AI_PARTIALONGROUND )
				{	// entity had floor mostly pulled out from underneath it
					// and is trying to correct
					if (relink)
					{
						SV_LinkEdict (ent);
						SV_TouchTriggers (ent);
					}
					return true;
				}
				VectorCopy (oldorg, ent->priv.sv->s.origin);
				return false;
			}
		}
	}

	if ((int)ent->progs.sv->aiflags & AI_PARTIALONGROUND )
	{
		(int)ent->progs.sv->aiflags &= ~AI_PARTIALONGROUND;
	}
	ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG(trace.ent);

	// the move is ok
	if(jump)
	{
		VectorScale(move, 10, ent->progs.sv->velocity);
		if(jump > 0)
		{
//ent->progs.sv->jump(ent);
			ent->progs.sv->velocity[2] = 2.5*jumpheight + 80;
		}
		else
		{
			ent->progs.sv->velocity[2] = max(ent->progs.sv->velocity[2],100);
			if(oldorg[2] - ent->priv.sv->s.origin[2] > 48)
				ent->priv.sv->s.origin[2] = oldorg[2] + ent->progs.sv->velocity[2]*0.1f;
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
	
	if (!ent->progs.sv->groundentity && !((int)ent->progs.sv->aiflags & (AI_FLY|AI_SWIM)))
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

	if (ent->priv.sv->clipmask) mask = ent->priv.sv->clipmask;
	else mask = MASK_SOLID;

	if(ent->progs.sv->solid == SOLID_BSP)
	{
		vec3_t	org, mins, maxs;
		VectorAdd(ent->priv.sv->s.origin, ent->progs.sv->origin_offset, org);
		VectorSubtract(ent->progs.sv->mins, ent->progs.sv->origin_offset, mins);
		VectorSubtract(ent->progs.sv->maxs, ent->progs.sv->origin_offset, maxs);
		trace = SV_Trace (org, mins, maxs, org, ent, mask);
	}
	else trace = SV_Trace (ent->priv.sv->s.origin, ent->progs.sv->mins, ent->progs.sv->maxs, ent->priv.sv->s.origin, ent, mask);

	if (trace.startsolid)
	{
		// Lazarus - work around for players/monsters standing on dead monsters causing
		// those monsters to gib when rotating brush models are in the vicinity
		if ( ((int)ent->progs.sv->flags & FL_DEADMONSTER) && (trace.ent->priv.sv->client || ((int)trace.ent->progs.sv->flags & FL_MONSTER)))
			return NULL;

		// Lazarus - return a bit more useful info than simply "g_edicts"
		if(trace.ent) return trace.ent;
		else return prog->edicts;
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
	if(VectorLength(ent->progs.sv->velocity) > sv_maxvelocity->value)
	{
		VectorNormalize(ent->progs.sv->velocity);
		VectorScale(ent->progs.sv->velocity, sv_maxvelocity->value, ent->progs.sv->velocity);
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
	float thinktime;

	thinktime = ent->progs.sv->nextthink;
	if (thinktime <= 0 || thinktime > sv.time + sv.frametime)
		return true;

	// don't let things stay in the past.
	// it is possible to start that way by a trigger with a local time.
	if (thinktime < sv.time) thinktime = sv.time;

	ent->progs.sv->nextthink = 0; //reset thinktime
	prog->globals.server->time = thinktime;
	prog->globals.server->pev = PRVM_EDICT_TO_PROG(ent);
	prog->globals.server->other = PRVM_EDICT_TO_PROG(prog->edicts);
	PRVM_ExecuteProgram (ent->progs.sv->think, "QC function pev->think is missing");

	return !ent->priv.sv->free;
}

/*
==================
SV_Impact

Two entities have touched, so run their touch functions
==================
*/
void SV_Impact (edict_t *e1, trace_t *trace)
{
	edict_t		*e2 = (edict_t *)trace->ent;

	PRVM_PUSH_GLOBALS;

	prog->globals.server->time = sv.time;
	Msg("touch %d and %d\n", e2->priv.sv->s.number, e1->priv.sv->s.number ); 
	if (!e1->priv.sv->free && !e2->priv.sv->free && e1->progs.sv->touch && e1->progs.sv->solid != SOLID_NOT)
	{
		prog->globals.server->pev = PRVM_EDICT_TO_PROG(e1);
		prog->globals.server->other = PRVM_EDICT_TO_PROG(e2);
		prog->globals.server->time = sv.time;
		prog->globals.server->trace_allsolid = trace->allsolid;
		prog->globals.server->trace_startsolid = trace->startsolid;
		prog->globals.server->trace_fraction = trace->fraction;
		prog->globals.server->trace_contents = trace->contents;
		VectorCopy (trace->endpos, prog->globals.server->trace_endpos);
		VectorCopy (trace->plane.normal, prog->globals.server->trace_plane_normal);
		prog->globals.server->trace_plane_dist =  trace->plane.dist;
		if (trace->ent) prog->globals.server->trace_ent = PRVM_EDICT_TO_PROG(trace->ent);
		else prog->globals.server->trace_ent = PRVM_EDICT_TO_PROG(prog->edicts);
		PRVM_ExecuteProgram (e1->progs.sv->touch, "QC function pev->touch is missing");
	}
	if (!e1->priv.sv->free && !e2->priv.sv->free && e2->progs.sv->touch && e2->progs.sv->solid != SOLID_NOT)
	{
		prog->globals.server->pev = PRVM_EDICT_TO_PROG(e2);
		prog->globals.server->other = PRVM_EDICT_TO_PROG(e1);
		prog->globals.server->time = sv.time;
		prog->globals.server->trace_allsolid = false;
		prog->globals.server->trace_startsolid = false;
		prog->globals.server->trace_fraction = 1;
		prog->globals.server->trace_contents = trace->contents;
		VectorCopy (e2->progs.sv->origin, prog->globals.server->trace_endpos);
		VectorSet (prog->globals.server->trace_plane_normal, 0, 0, 1);
		prog->globals.server->trace_plane_dist = 0;
		prog->globals.server->trace_ent = PRVM_EDICT_TO_PROG(e1);
		PRVM_ExecuteProgram (e2->progs.sv->touch, "QC function pev->touch is missing");
	}

	PRVM_POP_GLOBALS;
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
	VectorCopy (ent->progs.sv->velocity, original_velocity);
	VectorCopy (ent->progs.sv->velocity, primal_velocity);
	numplanes = 0;
	
	time_left = time;

	ent->progs.sv->groundentity = 0;
	for (bumpcount = 0; bumpcount < numbumps; bumpcount++)
	{
		for (i = 0; i < 3; i++) end[i] = ent->priv.sv->s.origin[i] + time_left * ent->progs.sv->velocity[i];

		trace = SV_Trace (ent->priv.sv->s.origin, ent->progs.sv->mins, ent->progs.sv->maxs, end, ent, mask);

		if (trace.allsolid)
		{	
			// entity is trapped in another solid
			VectorCopy (vec3_origin, ent->progs.sv->velocity);
			return 3;
		}

		if (trace.fraction > 0)
		{	
			// actually covered some distance
			VectorCopy (trace.endpos, ent->priv.sv->s.origin);
			VectorCopy (ent->progs.sv->velocity, original_velocity);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			 break;		// moved the entire distance

		hit = trace.ent;

		// Lazarus: If the pushed entity is a conveyor, raise us up and
		// try again
		if (!num_retries && wasonground)
		{
			if ((hit->progs.sv->movetype == MOVETYPE_CONVEYOR) && (trace.plane.normal[2] > 0.7))
			{
				vec3_t	above;

				VectorCopy(end,above);
				above[2] += 32;
				trace = SV_Trace (above, ent->progs.sv->mins, ent->progs.sv->maxs, end, ent, mask);
				VectorCopy (trace.endpos,end);
				end[2] += 1;
				VectorSubtract (end,ent->priv.sv->s.origin,ent->progs.sv->velocity);
				VectorScale (ent->progs.sv->velocity,1.0/time_left,ent->progs.sv->velocity);
				num_retries++;
				goto retry;
			}
		}

		// if blocked by player AND on a conveyor
		if (hit && hit->priv.sv->client && onconveyor)
		{
			vec3_t	player_dest;
			trace_t	ptrace;

			if(ent->progs.sv->mass > hit->progs.sv->mass)
			{
				VectorMA (hit->priv.sv->s.origin,time_left,ent->progs.sv->velocity,player_dest);
				ptrace = SV_Trace(hit->priv.sv->s.origin,hit->progs.sv->mins,hit->progs.sv->maxs,player_dest,hit,hit->priv.sv->clipmask);
				if(ptrace.fraction == 1.0)
				{
					VectorCopy(player_dest,hit->priv.sv->s.origin);
					SV_LinkEdict(hit);
					goto retry;
				}
			}
			blocked |= 8;
		}

		if (trace.plane.normal[2] > 0.7)
		{
			blocked |= 1; // floor
			if ( hit->progs.sv->solid == SOLID_BSP)
			{
				ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG(hit);
			}
		}
		if (!trace.plane.normal[2])
		{
			blocked |= 2;		// step
		}

		// run the impact function
		SV_Impact (ent, &trace);
		if (ent->priv.sv->free) break; // removed by the impact function

		
		time_left -= time_left * trace.fraction;
		
		// cliped to another plane
		if (numplanes >= MAX_CLIP_PLANES)
		{	
			// this shouldn't really happen
			VectorCopy (vec3_origin, ent->progs.sv->velocity);
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
			VectorCopy (new_velocity, ent->progs.sv->velocity);
		}
		else
		{	// go along the crease
			if (numplanes != 2)
			{
//				gi.dprintf ("clip velocity, numplanes == %i\n",numplanes);
				VectorCopy (vec3_origin, ent->progs.sv->velocity);
				blocked |= 7;
				return blocked;
			}
			CrossProduct (planes[0], planes[1], dir);
			d = DotProduct (dir, ent->progs.sv->velocity);
			VectorScale (dir, d, ent->progs.sv->velocity);
		}

		// if original velocity is against the original velocity, stop dead
		// to avoid tiny occilations in sloping corners
		if (DotProduct (ent->progs.sv->velocity, primal_velocity) <= 0)
		{
			VectorCopy (vec3_origin, ent->progs.sv->velocity);
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
	ent->progs.sv->bouncetype = 0;
	
	blocked = 0;
	VectorCopy (ent->progs.sv->velocity, original_velocity);
	VectorCopy (ent->progs.sv->velocity, primal_velocity);
	numplanes = 0;
	
	time_left = time;

	VectorAdd(ent->priv.sv->s.origin,ent->progs.sv->origin_offset,origin);
	VectorCopy(ent->progs.sv->size,maxs);
	VectorScale(maxs,0.5,maxs);
	VectorNegate(maxs,mins);

	ent->progs.sv->groundentity = 0;

	for (bumpcount=0 ; bumpcount<numbumps ; bumpcount++)
	{
		for (i = 0; i < 3; i++) end[i] = origin[i] + time_left * ent->progs.sv->velocity[i];

		trace = SV_Trace (origin, mins, maxs, end, ent, mask);

		if (trace.allsolid)
		{	
			// entity is trapped in another solid
			VectorCopy (vec3_origin, ent->progs.sv->velocity);
			return 3;
		}

		if (trace.fraction > 0)
		{	
			// actually covered some distance
			VectorCopy (trace.endpos, origin);
			VectorSubtract (origin, ent->progs.sv->origin_offset, ent->priv.sv->s.origin);
			VectorCopy (ent->progs.sv->velocity, original_velocity);
			numplanes = 0;
		}

		if (trace.fraction == 1)
			break;		// moved the entire distance

		hit = trace.ent;

		// Lazarus: If the pushed entity is a conveyor, raise us up and
		// try again
		if (!num_retries && wasonground)
		{
			if ((hit->progs.sv->movetype == MOVETYPE_CONVEYOR) && (trace.plane.normal[2] > 0.7))
			{
				vec3_t	above;

				VectorCopy(end,above);
				above[2] += 32;
				trace = SV_Trace (above, mins, maxs, end, ent, mask);
				VectorCopy (trace.endpos,end);
				VectorSubtract (end,origin,ent->progs.sv->velocity);
				VectorScale (ent->progs.sv->velocity,1.0/time_left,ent->progs.sv->velocity);
				num_retries++;
				goto retry;
			}
		}

		// if blocked by player AND on a conveyor
		if (hit->priv.sv->client && onconveyor)
		{
			vec3_t	player_dest;
			trace_t	ptrace;

			if(ent->progs.sv->mass > hit->progs.sv->mass)
			{
				VectorMA (hit->priv.sv->s.origin,time_left,ent->progs.sv->velocity,player_dest);
				ptrace = SV_Trace(hit->priv.sv->s.origin,hit->progs.sv->mins,hit->progs.sv->maxs,player_dest,hit,hit->priv.sv->clipmask);
				if(ptrace.fraction == 1.0)
				{
					VectorCopy(player_dest,hit->priv.sv->s.origin);
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
			if((hit->progs.sv->movetype != MOVETYPE_PUSHABLE) || ((ent->progs.sv->waterlevel==0) && (hit->progs.sv->waterlevel==0)))
			{
				blocked |= 1;		// floor
				if ( hit->progs.sv->solid == SOLID_BSP)
				{
					ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG(hit);
				}
			}
		}
		if (!trace.plane.normal[2])
		{
			blocked |= 2;		// step
		}

		// run the impact function
		SV_Impact (ent, &trace);
		if (ent->priv.sv->free) break; // removed by the impact function

		time_left -= time_left * trace.fraction;

		// clipped to another plane
		if (numplanes >= MAX_CLIP_PLANES)
		{	
			// this shouldn't really happen
			VectorCopy (vec3_origin, ent->progs.sv->velocity);
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
			if(ent->progs.sv->bouncetype == 1)
			{
				ClipVelocity (original_velocity, planes[i], new_velocity, 1.4);
				// stop small oscillations
				if (new_velocity[2] < 60)
				{
					ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
					VectorCopy (vec3_origin, new_velocity);
				} 
				else
				{
					// add a bit of random horizontal motion
					if(!new_velocity[0]) new_velocity[0] = crandom() * new_velocity[2]/4;
					if(!new_velocity[1]) new_velocity[1] = crandom() * new_velocity[2]/4;
				}
			}
			else if(ent->progs.sv->bouncetype == 2)
			{
				VectorCopy(ent->progs.sv->velocity, new_velocity);
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
			VectorCopy (new_velocity, ent->progs.sv->velocity);
		}
		else
		{	// go along the crease
			if (numplanes != 2)
			{
				VectorCopy (vec3_origin, ent->progs.sv->velocity);
				blocked |= 7;
				return blocked;
			}
			CrossProduct (planes[0], planes[1], dir);
			d = DotProduct (dir, ent->progs.sv->velocity);
			VectorScale (dir, d, ent->progs.sv->velocity);
		}

		// if velocity is against the original velocity, stop dead
		// to avoid tiny occilations in sloping corners
		if( !ent->progs.sv->bouncetype )
		{
			if (DotProduct (ent->progs.sv->velocity, primal_velocity) <= 0)
			{
				VectorCopy (vec3_origin, ent->progs.sv->velocity);
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
	ent->progs.sv->velocity[2] -= ent->progs.sv->gravity * sv_gravity->value * 0.1;
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

	VectorCopy (ent->priv.sv->s.origin, start);
	VectorAdd (start, push, end);

	if (ent->priv.sv->clipmask)
		mask = ent->priv.sv->clipmask;
	else
		mask = MASK_SOLID;

retry:
	trace = SV_Trace (start, ent->progs.sv->mins, ent->progs.sv->maxs, end, ent, mask);
	
	VectorCopy (trace.endpos, ent->priv.sv->s.origin);
	SV_LinkEdict (ent);

	if (trace.fraction != 1.0)
	{
		SV_Impact (ent, &trace);

		// if the pushed entity went away and the pusher is still there
		if (trace.ent->priv.sv->free && !ent->priv.sv->free)
		{
			// move the pusher back and try again
			VectorCopy (start, ent->priv.sv->s.origin);
			SV_LinkEdict (ent);
			goto retry;
		}

		// Lazarus: If the pushed entity is a conveyor, raise us up and
		// try again
		if (!num_retries && wasonground)
		{
			if ((trace.ent->progs.sv->movetype == MOVETYPE_CONVEYOR) && (trace.plane.normal[2] > 0.7) && !trace.startsolid)
			{
				vec3_t	above;
				VectorCopy(end,above);
				above[2] += 32;
				trace = SV_Trace (above, ent->progs.sv->mins, ent->progs.sv->maxs, end, ent, mask);
				VectorCopy (trace.endpos, end);
				VectorCopy (start, ent->priv.sv->s.origin);
				SV_LinkEdict(ent);
				num_retries++;
				goto retry;
			}
		}
		if(onconveyor && !trace.ent->priv.sv->client)
		{
			// If blocker can be damaged, destroy it. Otherwise destroy blockee.
			if(trace.ent->progs.sv->takedamage == DAMAGE_YES)
				T_Damage(trace.ent, ent, ent, vec3_origin, trace.ent->priv.sv->s.origin, vec3_origin, 100000, 1, 0, DMG_CRUSH);
			else T_Damage(ent, trace.ent, trace.ent, vec3_origin, ent->priv.sv->s.origin, vec3_origin, 100000, 1, 0, DMG_CRUSH);
		}

	}
	if (!ent->priv.sv->free) SV_TouchTriggers (ent);

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

	for(i = 1, rider = PRVM_EDICT_NUM(i); i <= prog->num_edicts; i++, rider = PRVM_EDICT_NUM(i))
	{
		if((rider->progs.sv->groundentity == PRVM_EDICT_TO_PROG(platform)) && (rider != ignore))
		{
			VectorAdd(rider->priv.sv->s.origin, move, rider->priv.sv->s.origin);
			if (turn && (amove[YAW] != 0.))
			{
				if(!rider->priv.sv->client) rider->priv.sv->s.angles[YAW] += amove[YAW];
				else
				{
					rider->priv.sv->s.angles[YAW] += amove[YAW];
					rider->priv.sv->client->ps.pmove.delta_angles[YAW] += ANGLE2SHORT(amove[YAW]);
					rider->priv.sv->client->ps.pmove.pm_type = PM_FREEZE;
					rider->priv.sv->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
				}
			}
			SV_LinkEdict(rider);

			if(SV_TestEntityPosition(rider))
			{
				// Move is blocked. Since this is for riders, not pushees,
				// it should be ok to just back the move for this rider off
				VectorSubtract(rider->priv.sv->s.origin,move,rider->priv.sv->s.origin);
				if(turn && (amove[YAW] != 0.))
				{
					rider->priv.sv->s.angles[YAW] -= amove[YAW];
					if(rider->priv.sv->client)
					{
						rider->priv.sv->client->ps.pmove.delta_angles[YAW] -= ANGLE2SHORT(amove[YAW]);
						rider->priv.sv->client->ps.viewangles[YAW] -= amove[YAW];
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
		if(k) p[k4][2] = ent->progs.sv->maxs[2];
		else p[k4][2] = ent->progs.sv->mins[2];

		p[k4 + 1][2] = p[k4][2];
		p[k4 + 2][2] = p[k4][2];
		p[k4 + 3][2] = p[k4][2];

		for(j = 0; j < 2; j++)
		{
			j2 = j * 2;
			if(j) p[j2+k4][1] = ent->progs.sv->maxs[1];
			else p[j2+k4][1] = ent->progs.sv->mins[1];
			p[j2 + k4 + 1][1] = p[j2 + k4][1];

			for(i = 0; i < 2; i++)
			{
				if(i) p[i + j2 + k4][0] = ent->progs.sv->maxs[0];
				else p[i + j2 + k4][0] = ent->progs.sv->mins[0];
			}
		}
	}

	AngleVectors(ent->priv.sv->s.angles, forward, left, up);

	for(i = 0; i < 8; i++)
	{
		VectorScale(forward, p[i][0], f1);
		VectorScale(left, -p[i][1], l1);
		VectorScale(up, p[i][2], u1);
		VectorAdd(ent->priv.sv->s.origin, f1, p[i]);
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
		mins[i] = pusher->progs.sv->absmin[i] + move[i];
		maxs[i] = pusher->progs.sv->absmax[i] + move[i];
	}

	// we need this for pushing things later
	VectorSubtract (vec3_origin, amove, org);
	AngleVectors (org, forward, right, up);

	// save the pusher's original position
	pushed_p->ent = pusher;
	VectorCopy (pusher->priv.sv->s.origin, pushed_p->origin);
	VectorCopy (pusher->priv.sv->s.angles, pushed_p->angles);
	if (pusher->priv.sv->client) pushed_p->deltayaw = pusher->priv.sv->client->ps.pmove.delta_angles[YAW];
	pushed_p++;

	// move the pusher to it's final position
	VectorAdd (pusher->priv.sv->s.origin, move, pusher->priv.sv->s.origin);
	VectorAdd (pusher->priv.sv->s.angles, amove, pusher->priv.sv->s.angles);
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
	check = PRVM_EDICT_NUM( 1 );

	for (e = 1; e < prog->num_edicts; e++, check = PRVM_EDICT_NUM(e))
	{
		if (check->priv.sv->free) continue;
		if (check == PRVM_PROG_TO_EDICT(pusher->progs.sv->owner)) continue;
		if (check->progs.sv->movetype == MOVETYPE_PUSH || check->progs.sv->movetype == MOVETYPE_NONE || check->progs.sv->movetype == MOVETYPE_NOCLIP)
			continue;

		if (!check->priv.sv->area.prev)
			continue;		// not linked in anywhere

		// if the entity is standing on the pusher, it will definitely be moved
		if (check->progs.sv->groundentity != PRVM_EDICT_TO_PROG(pusher))
		{
			// see if the ent needs to be tested
			if ( check->progs.sv->absmin[0] >= realmaxs[0]
			|| check->progs.sv->absmin[1] >= realmaxs[1]
			|| check->progs.sv->absmin[2] >= realmaxs[2]
			|| check->progs.sv->absmax[0] <= realmins[0]
			|| check->progs.sv->absmax[1] <= realmins[1]
			|| check->progs.sv->absmax[2] <= realmins[2] )
				continue;

			// see if the ent's bbox is inside the pusher's final position
			if (!SV_TestEntityPosition (check))
				continue;
		}

		// Lazarus: func_tracktrain-specific stuff
		// If train is *driven*, then hurt monsters/players it touches NOW
		// rather than waiting to be blocked.
		if (((int)pusher->progs.sv->flags & FL_TRACKTRAIN) && pusher->progs.sv->owner && (((int)check->progs.sv->flags & FL_MONSTER) || check->priv.sv->client) && (check->progs.sv->groundentity != PRVM_EDICT_TO_PROG(pusher)))
		{
			vec3_t	dir;
			VectorSubtract(check->priv.sv->s.origin,pusher->priv.sv->s.origin,dir);
			dir[2] += 16;
			VectorNormalize(dir);
			T_Damage (check, pusher, pusher, dir, check->priv.sv->s.origin, vec3_origin, pusher->progs.sv->dmg, 1, 0, DMG_CRUSH);
		}

		if ((pusher->progs.sv->movetype == MOVETYPE_PUSH) || (check->progs.sv->groundentity == PRVM_EDICT_TO_PROG(pusher)))
		{
			// move this entity
			pushed_p->ent = check;
			VectorCopy (check->priv.sv->s.origin, pushed_p->origin);
			VectorCopy (check->priv.sv->s.angles, pushed_p->angles);
			pushed_p++;

			// try moving the contacted entity 
			VectorAdd (check->priv.sv->s.origin, move, check->priv.sv->s.origin);
			// Lazarus: if turn_rider is set, do it. We don't do this by default
			//          'cause it can be a fairly drastic change in gameplay
			if (turn && (check->progs.sv->groundentity == PRVM_EDICT_TO_PROG(pusher)))
			{
				if(!check->priv.sv->client)
				{
					check->priv.sv->s.angles[YAW] += amove[YAW];
				}
				else
				{
					if(amove[YAW] != 0.)
					{
						check->priv.sv->client->ps.pmove.delta_angles[YAW] += ANGLE2SHORT(amove[YAW]);
						check->priv.sv->client->ps.viewangles[YAW] += amove[YAW];

						// PM_FREEZE makes the turn smooth, even though it will
						// be turned off by ClientThink in the very next video frame
						check->priv.sv->client->ps.pmove.pm_type = PM_FREEZE;
						// PMF_NO_PREDICTION overrides .exe's client physics, which
						// really doesn't like for us to change player angles. Note
						// that this isn't strictly necessary, since Lazarus 1.7 and
						// later automatically turn prediction off (in ClientThink) when 
						// player is riding a MOVETYPE_PUSH
						check->priv.sv->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
					}
					if(amove[PITCH] != 0.)
					{
						float	delta_yaw;
						float	pitch = amove[PITCH];

						delta_yaw = check->priv.sv->s.angles[YAW] - pusher->priv.sv->s.angles[YAW];
						delta_yaw *= M_PI / 180.;
						pitch *= cos(delta_yaw);
						check->priv.sv->client->ps.pmove.delta_angles[PITCH] += ANGLE2SHORT(pitch);
						check->priv.sv->client->ps.viewangles[PITCH] += pitch;
						check->priv.sv->client->ps.pmove.pm_type = PM_FREEZE;
						check->priv.sv->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
					}
				}
			}

			// Lazarus: This is where we attempt to move check due to a rotation, WITHOUT embedding
			//          check in pusher (or anything else)
			if(check->progs.sv->groundentity == PRVM_EDICT_TO_PROG(pusher))
			{
				if((amove[PITCH] != 0) || (amove[YAW] != 0) || (amove[ROLL] != 0))
				{
					// figure movement due to the pusher's amove
					VectorAdd(check->priv.sv->s.origin,check->progs.sv->origin_offset,org_check);
					VectorSubtract (org_check, pusher->priv.sv->s.origin, org);
					org2[0] = DotProduct (org, forward);
					org2[1] = -DotProduct (org, right);
					org2[2] = DotProduct (org, up);
					VectorSubtract (org2, org, move2);
					VectorAdd (check->priv.sv->s.origin, move2, check->priv.sv->s.origin);
					if((amove[PITCH] != 0) || (amove[ROLL] != 0))
					{
						VectorCopy(check->priv.sv->s.origin,org);
						org[2] += 2*check->progs.sv->mins[2];
						tr = SV_Trace(check->priv.sv->s.origin,vec3_origin,vec3_origin,org,check,MASK_SOLID);
						if(!tr.startsolid && tr.fraction < 1)
							check->priv.sv->s.origin[2] = tr.endpos[2] - check->progs.sv->mins[2] + fabs(tr.plane.normal[0])*check->progs.sv->size[0]/2 + fabs(tr.plane.normal[1])*check->progs.sv->size[1]/2;
						
						// Lazarus: func_tracktrain is a special case. Since we KNOW (if the map was
						//          constructed properly) that "post_origin" is a safe position, we
						//          can infer that there should be a safe (not embedded) position
						//          somewhere between post_origin and the proposed new location.
						if(((int)pusher->progs.sv->flags & FL_TRACKTRAIN) && (check->priv.sv->client || ((int)check->progs.sv->flags & FL_MONSTER)))
						{
							vec3_t	f,l,u;
							
							AngleVectors(pusher->priv.sv->s.angles, f, l, u);
							VectorScale(f,pusher->progs.sv->post_origin[0],f);
							VectorScale(l,-pusher->progs.sv->post_origin[1],l);
							VectorAdd(pusher->priv.sv->s.origin,f,org);
							VectorAdd(org,l,org);
							org[2] += pusher->progs.sv->post_origin[2] + 1;
							org[2] += 16 * ( fabs(u[0]) + fabs(u[1]) );
							tr = SV_Trace(org,check->progs.sv->mins,check->progs.sv->maxs,check->priv.sv->s.origin,check,MASK_SOLID);
							if(!tr.startsolid)
							{
								VectorCopy(tr.endpos,check->priv.sv->s.origin);
								VectorCopy(check->priv.sv->s.origin,org);
								org[2] -= 128;
								tr = SV_Trace(check->priv.sv->s.origin,check->progs.sv->mins,check->progs.sv->maxs,org,check,MASK_SOLID);
								if(tr.fraction > 0)
									VectorCopy(tr.endpos,check->priv.sv->s.origin);
							}
						}
					}
				}
			}
			
			// may have pushed them off an edge
			if (check->progs.sv->groundentity != PRVM_EDICT_TO_PROG(pusher)) check->progs.sv->groundentity = 0;

			block = SV_TestEntityPosition (check);

			if (block && ((int)pusher->progs.sv->flags & FL_TRACKTRAIN) && (check->priv.sv->client || ((int)check->progs.sv->flags & FL_MONSTER)) && (check->progs.sv->groundentity == PRVM_EDICT_TO_PROG(pusher)))
			{
				// Lazarus: Last hope. If this doesn't get rider out of the way he's
				// gonna be stuck.
				vec3_t	f,l,u;

				AngleVectors(pusher->priv.sv->s.angles, f, l, u);
				VectorScale(f,pusher->progs.sv->post_origin[0],f);
				VectorScale(l,-pusher->progs.sv->post_origin[1],l);
				VectorAdd(pusher->priv.sv->s.origin,f,org);
				VectorAdd(org,l,org);
				org[2] += pusher->progs.sv->post_origin[2] + 1;
				org[2] += 16 * ( fabs(u[0]) + fabs(u[1]) );
				tr = SV_Trace(org,check->progs.sv->mins,check->progs.sv->maxs,check->priv.sv->s.origin,check,MASK_SOLID);
				if(!tr.startsolid)
				{
					VectorCopy(tr.endpos,check->priv.sv->s.origin);
					VectorCopy(check->priv.sv->s.origin,org);
					org[2] -= 128;
					tr = SV_Trace(check->priv.sv->s.origin,check->progs.sv->mins,check->progs.sv->maxs,org,check,MASK_SOLID);
					if(tr.fraction > 0)
						VectorCopy(tr.endpos,check->priv.sv->s.origin);
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
			VectorSubtract (check->priv.sv->s.origin, move,  check->priv.sv->s.origin);
			VectorSubtract (check->priv.sv->s.origin, move2, check->priv.sv->s.origin);
			if(turn)
			{
				// Argh! - angle
				check->priv.sv->s.angles[YAW] -= amove[YAW];
				if(check->priv.sv->client)
				{
					check->priv.sv->client->ps.pmove.delta_angles[YAW] -= ANGLE2SHORT(amove[YAW]);
					check->priv.sv->client->ps.viewangles[YAW] -= amove[YAW];
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
			VectorCopy (p->origin, p->ent->priv.sv->s.origin);
			VectorCopy (p->angles, p->ent->priv.sv->s.angles);
			if (p->ent->priv.sv->client)
			{
				p->ent->priv.sv->client->ps.pmove.delta_angles[YAW] = p->deltayaw;
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

	if (part->progs.sv->velocity[0] || part->progs.sv->velocity[1] || part->progs.sv->velocity[2] || part->progs.sv->avelocity[0] || part->progs.sv->avelocity[1] || part->progs.sv->avelocity[2])
	{	
		// object is moving
		VectorScale (part->progs.sv->velocity, sv.frametime, move);
		VectorScale (part->progs.sv->avelocity, sv.frametime, amove);

		SV_Push(part, move, amove); // move was blocked
	}

	if (pushed_p > &pushed[MAX_EDICTS])
		PRVM_ERROR("pushed_p > &pushed[MAX_EDICTS], memory corrupted");

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
	
	VectorMA (ent->priv.sv->s.angles, 0.1f, ent->progs.sv->avelocity, ent->priv.sv->s.angles);
	VectorMA (ent->priv.sv->s.origin, 0.1f, ent->progs.sv->velocity, ent->priv.sv->s.origin);

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

	if (ent->progs.sv->groundentity) wasonground = true;

	if (ent->progs.sv->velocity[2] > 0) ent->progs.sv->groundentity = 0;

	// check for the groundentity going away
	if (ent->progs.sv->groundentity)
		if (PRVM_PROG_TO_EDICT(ent->progs.sv->groundentity)->priv.sv->free)
			ent->progs.sv->groundentity = 0;

	// Lazarus: conveyor
	if (ent->progs.sv->groundentity && (PRVM_PROG_TO_EDICT(ent->progs.sv->groundentity)->progs.sv->movetype == MOVETYPE_CONVEYOR))
	{
		vec3_t	point, end;
		trace_t	tr;
		edict_t	*ground = PRVM_PROG_TO_EDICT(ent->progs.sv->groundentity);

		VectorCopy(ent->priv.sv->s.origin,point);
		point[2] += 1;
		VectorCopy(point,end);
		end[2] -= 256;
		tr = SV_Trace (point, ent->progs.sv->mins, ent->progs.sv->maxs, end, ent, MASK_SOLID);
		// tr.ent HAS to be ground, but just in case we screwed something up:
		if(tr.ent == ground)
		{
			onconveyor = true;
			ent->progs.sv->velocity[0] = ground->progs.sv->movedir[0] * ground->progs.sv->speed;
			ent->progs.sv->velocity[1] = ground->progs.sv->movedir[1] * ground->progs.sv->speed;
			if(tr.plane.normal[2] > 0)
			{
				ent->progs.sv->velocity[2] = ground->progs.sv->speed * sqrt(1.0 - tr.plane.normal[2]*tr.plane.normal[2]) / tr.plane.normal[2];
				if(DotProduct(ground->progs.sv->movedir,tr.plane.normal) > 0)
				{
					// then we're moving down
					ent->progs.sv->velocity[2] = -ent->progs.sv->velocity[2];
				}
			}
			VectorScale (ent->progs.sv->velocity, 0.1f, move);
			trace = SV_PushEntity (ent, move);
			if (ent->priv.sv->free) return;
			SV_CheckGround(ent);
		}
	}

	// if onground, return without moving
	if ( ent->progs.sv->groundentity )
		return;

	VectorCopy (ent->priv.sv->s.origin, old_origin);

	SV_CheckVelocity (ent);

	// add gravity
	if (ent->progs.sv->movetype != MOVETYPE_FLY)
		SV_AddGravity (ent);

	// move angles
	VectorMA (ent->priv.sv->s.angles, 0.1f, ent->progs.sv->avelocity, ent->priv.sv->s.angles);

	// move origin
	VectorScale (ent->progs.sv->velocity, 0.1f, move);
	trace = SV_PushEntity (ent, move);
	if (ent->priv.sv->free) return;

	if (trace.fraction < 1 )
	{
		if (ent->progs.sv->movetype == MOVETYPE_BOUNCE)
			backoff = 1.0 + 0.5f;
		else if(trace.plane.normal[2] <= 0.7)	// Lazarus - don't stop on steep incline
			backoff = 1.5;
		else
			backoff = 1;

		ClipVelocity (ent->progs.sv->velocity, trace.plane.normal, ent->progs.sv->velocity, backoff);

		// stop if on ground
		if (trace.plane.normal[2] > 0.7)
		{		
			if (ent->progs.sv->velocity[2] < 60.0f || (ent->progs.sv->movetype != MOVETYPE_BOUNCE) )
			{
				ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
				VectorCopy (vec3_origin, ent->progs.sv->velocity);
				VectorCopy (vec3_origin, ent->progs.sv->avelocity);
			}
		}
	}

	// check for water transition
	wasinwater = ((int)ent->progs.sv->watertype & MASK_WATER);
	ent->progs.sv->watertype = SV_PointContents (ent->priv.sv->s.origin);
	isinwater = (int)ent->progs.sv->watertype & MASK_WATER;

	if (isinwater) ent->progs.sv->waterlevel = 1;
	else ent->progs.sv->waterlevel = 0;

	if (!wasinwater && isinwater)
		SV_StartSound(old_origin, prog->edicts, CHAN_AUTO, SV_SoundIndex("misc/h2ohit1.wav"), 1, 1, 0);
	else if (wasinwater && !isinwater)
		SV_StartSound(ent->priv.sv->s.origin, prog->edicts, CHAN_AUTO, SV_SoundIndex("misc/h2ohit1.wav"), 1, 1, 0);
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

	VectorMA (ent->priv.sv->s.angles, 0.1f, ent->progs.sv->avelocity, ent->priv.sv->s.angles);
	adjustment = 0.1f * sv_stopspeed * sv_friction;

	for (n = 0; n < 3; n++)
	{
		if (ent->progs.sv->avelocity[n] > 0)
		{
			ent->progs.sv->avelocity[n] -= adjustment;
			if (ent->progs.sv->avelocity[n] < 0)
				ent->progs.sv->avelocity[n] = 0;
		}
		else
		{
			ent->progs.sv->avelocity[n] += adjustment;
			if (ent->progs.sv->avelocity[n] > 0)
				ent->progs.sv->avelocity[n] = 0;
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

	for(i = 1, rider = PRVM_EDICT_NUM(i); i <= prog->num_edicts; i++, rider = PRVM_EDICT_NUM(i)) 
	{
		if(rider == platform) continue;
		if(rider->priv.sv->free) continue;
		if(rider->progs.sv->groundentity == PRVM_EDICT_TO_PROG(platform))
		{
			mass += rider->progs.sv->mass;
			mass += RiderMass(rider);
		}
		else if (rider->progs.sv->movetype == MOVETYPE_PUSHABLE )
		{
			// Bah - special case for func_pushable riders. Swimming
			// func_pushables don't really have a groundentity, even
			// though they may be sitting on another swimming
			// func_pushable, which is what we need to know.
			VectorCopy(rider->priv.sv->s.origin,point);
			point[2] -= 0.25;
			trace = SV_Trace (rider->priv.sv->s.origin, rider->progs.sv->mins, rider->progs.sv->maxs, point, rider, MASK_MONSTERSOLID);
			if ( trace.plane.normal[2] < 0.7 && !trace.startsolid)
				continue;
			if (!trace.startsolid && !trace.allsolid)
			{
				if(trace.ent == platform)
				{
					mass += rider->progs.sv->mass;
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
	float		friction, volume;
	edict_t		*ground;
	edict_t     	*e;
	int         	cont;
	int		mask;
	int         	i;
	int		oldwaterlevel;
	vec3_t      	point, end;
	vec3_t		old_origin, move;

	// airborne monsters should always check for ground
	if (!ent->progs.sv->groundentity) SV_CheckGround (ent);

	oldwaterlevel = ent->progs.sv->waterlevel;

	VectorCopy(ent->priv.sv->s.origin, old_origin);

	// Lazarus: If density hasn't been calculated yet, do so now
	if (ent->progs.sv->mass > 0 && ent->progs.sv->density == 0.)
	{
		volume = ent->progs.sv->size[0] * ent->progs.sv->size[1] * ent->progs.sv->size[2];
		ent->progs.sv->density = ent->progs.sv->mass/volume;
	}

	// If not a monster, then determine whether we're in water.
	// (monsters take care of this in g_monster.c)
	if (!((int)ent->progs.sv->flags & FL_MONSTER) && ((int)ent->progs.sv->aiflags && AI_SWIM) )
	{
		point[0] = (ent->progs.sv->absmax[0] + ent->progs.sv->absmin[0])/2;
		point[1] = (ent->progs.sv->absmax[1] + ent->progs.sv->absmin[1])/2;
		point[2] = ent->progs.sv->absmin[2] + 1;
		cont = SV_PointContents (point);
		if (!(cont & MASK_WATER))
		{
			ent->progs.sv->waterlevel = 0;
			ent->progs.sv->watertype = 0;
		}
		else {
			ent->progs.sv->watertype = cont;
			ent->progs.sv->waterlevel = 1;
			point[2] = ent->progs.sv->absmin[2] + ent->progs.sv->size[2]/2;
			cont = SV_PointContents (point);
			if (cont & MASK_WATER)
			{
				ent->progs.sv->waterlevel = 2;
				point[2] = ent->progs.sv->absmax[2];
				cont = SV_PointContents (point);
				if (cont & MASK_WATER) ent->progs.sv->waterlevel = 3;
			}
		}
	}
	
	ground = PRVM_PROG_TO_EDICT(ent->progs.sv->groundentity);

	SV_CheckVelocity (ent);

	if (ground) wasonground = true;
		
	if (ent->progs.sv->avelocity[0] || ent->progs.sv->avelocity[1] || ent->progs.sv->avelocity[2])
		SV_AddRotationalFriction (ent);

	// add gravity except:
	// flying monsters
	// swimming monsters who are in the water
	if (!wasonground)
	{
		if (!((int)ent->progs.sv->aiflags & AI_FLY))
		{
			if (!(((int)ent->progs.sv->aiflags & AI_SWIM) && (ent->progs.sv->waterlevel > 2)))
			{
				if (ent->progs.sv->velocity[2] < sv_gravity->value*-0.1)
					hitsound = true;
				if (ent->progs.sv->waterlevel == 0)
					SV_AddGravity (ent);
			}
		}
	}

	// friction for flying monsters that have been given vertical velocity
	if (((int)ent->progs.sv->aiflags & AI_FLY) && (ent->progs.sv->velocity[2] != 0))
	{
		speed = fabs(ent->progs.sv->velocity[2]);
		control = speed < sv_stopspeed ? sv_stopspeed : speed;
		friction = sv_friction/3;
		newspeed = speed - (0.1f * control * friction);
		if (newspeed < 0) newspeed = 0;
		newspeed /= speed;
		ent->progs.sv->velocity[2] *= newspeed;
	}

	// friction for swimming monsters that have been given vertical velocity
	if (ent->progs.sv->movetype != MOVETYPE_PUSHABLE)
	{
		// Lazarus: This is id's swag at drag. It works mostly, but for submerged 
		//          crates we can do better.
		if (((int)ent->progs.sv->aiflags & AI_SWIM) && (ent->progs.sv->velocity[2] != 0))
		{
			speed = fabs(ent->progs.sv->velocity[2]);
			control = speed < sv_stopspeed ? sv_stopspeed : speed;
			newspeed = speed - (0.1f * control * sv_waterfriction * ent->progs.sv->waterlevel);
			if (newspeed < 0) newspeed = 0;
			newspeed /= speed;
			ent->progs.sv->velocity[2] *= newspeed;
		}
	}

	// Lazarus: Floating stuff
	if ((ent->progs.sv->movetype == MOVETYPE_PUSHABLE) && ((int)ent->progs.sv->aiflags && AI_SWIM) && (ent->progs.sv->waterlevel))
	{
		float	waterlevel;
		float	rider_mass, total_mass;
		trace_t	tr;
		float	Accel, Area, Drag, Force;

		VectorCopy(point,end);
		if(ent->progs.sv->waterlevel < 3)
		{
			point[2] = ent->progs.sv->absmax[2];
			end[2]   = ent->progs.sv->absmin[2];
			tr = SV_Trace(point,NULL,NULL,end,ent,MASK_WATER);
			waterlevel = tr.endpos[2];
		}
		else
		{
			// Not right, but really all we need to know
			waterlevel = ent->progs.sv->absmax[2] + 1;
		}
		rider_mass = RiderMass(ent);
		total_mass = rider_mass + ent->progs.sv->mass;
		Area  = ent->progs.sv->size[0] * ent->progs.sv->size[1];
		if(waterlevel < ent->progs.sv->absmax[2])
		{
			// A portion of crate is above water
			// For partially submerged crates, use same psuedo-friction thing used
			// on other entities. This isn't really correct, but then neither is
			// our drag calculation used for fully submerged crates good for this
			// situation
			if (ent->progs.sv->velocity[2] != 0)
			{
				speed = fabs(ent->progs.sv->velocity[2]);
				control = speed < sv_stopspeed ? sv_stopspeed : speed;
				newspeed = speed - (0.1f * control * sv_waterfriction * ent->progs.sv->waterlevel);
				if (newspeed < 0) newspeed = 0;
				newspeed /= speed;
				ent->progs.sv->velocity[2] *= newspeed;
			}

			// Apply physics and bob AFTER friction, or the damn thing will never move.
			Force = -total_mass + ((waterlevel-ent->progs.sv->absmin[2]) * Area * WATER_DENSITY);
			Accel = Force * sv_gravity->value/total_mass;
			ent->progs.sv->velocity[2] += Accel*0.1f;
		} 
		else
		{
			// Crate is fully submerged
			Force = -total_mass + volume * WATER_DENSITY;
			if(sv_gravity->value)
			{
				Drag  = 0.00190735 * 1.05 * Area * (ent->progs.sv->velocity[2]*ent->progs.sv->velocity[2])/sv_gravity->value;
				if(Drag > fabs(Force))
				{
					// Drag actually CAN be > total weight, but if we do this we tend to
					// get crates flying back out of the water after being dropped from some
					// height
					Drag = fabs(Force);
				}
				if(ent->progs.sv->velocity[2] > 0)
					Drag = -Drag;
				Force += Drag;
			}
			Accel = Force * sv_gravity->value/total_mass;
			ent->progs.sv->velocity[2] += Accel*0.1f;
		}

		if((int)ent->progs.sv->watertype & MASK_CURRENT)
		{
			// Move with current, relative to mass. Mass=400 or less
			// will move at 50 units/sec.
			float v;
			int   current;

			if(ent->progs.sv->mass > 400) v = 0.125 * ent->progs.sv->mass;
			else v = 50.;
			current = (int)ent->progs.sv->watertype & MASK_CURRENT;
			switch (current)
			{
			case CONTENTS_CURRENT_0:    ent->progs.sv->velocity[0] = v;  break;
			case CONTENTS_CURRENT_90:   ent->progs.sv->velocity[1] = v;  break;
			case CONTENTS_CURRENT_180:  ent->progs.sv->velocity[0] = -v; break;
			case CONTENTS_CURRENT_270:  ent->progs.sv->velocity[1] = -v; break;
			case CONTENTS_CURRENT_UP :  ent->progs.sv->velocity[2] = max(v, ent->progs.sv->velocity[2]);
			case CONTENTS_CURRENT_DOWN: ent->progs.sv->velocity[2] = min(-v, ent->progs.sv->velocity[2]);
			}
		}
	}

	// Conveyor
	if (wasonground && (ground->progs.sv->movetype == MOVETYPE_CONVEYOR))
	{
		trace_t	tr;

		VectorCopy(ent->priv.sv->s.origin,point);
		point[2] += 1;
		VectorCopy(point,end);
		end[2] -= 256;
		tr = SV_Trace (point, ent->progs.sv->mins, ent->progs.sv->maxs, end, ent, MASK_SOLID);
		// tr.ent HAS to be ground, but just in case we screwed something up:
		if(tr.ent == ground)
		{
			onconveyor = true;
			ent->progs.sv->velocity[0] = ground->progs.sv->movedir[0] * ground->progs.sv->speed;
			ent->progs.sv->velocity[1] = ground->progs.sv->movedir[1] * ground->progs.sv->speed;
			if(tr.plane.normal[2] > 0)
			{
				ent->progs.sv->velocity[2] = ground->progs.sv->speed * sqrt(1.0 - tr.plane.normal[2]*tr.plane.normal[2]) / tr.plane.normal[2];
				if(DotProduct(ground->progs.sv->movedir,tr.plane.normal) > 0)
				{
					// Then we're moving down.
					ent->progs.sv->velocity[2] = -ent->progs.sv->velocity[2] + 2;
				}
			}
		}
	}

	if (ent->progs.sv->velocity[2] || ent->progs.sv->velocity[1] || ent->progs.sv->velocity[0])
	{
		int	block;
		// apply friction
		// let dead monsters who aren't completely onground slide

		if ((wasonground) || ((int)ent->progs.sv->aiflags & (AI_SWIM|AI_FLY)))

			if (!onconveyor)
			{
				if (!(ent->progs.sv->health <= 0.0 && !SV_CheckBottom(ent)))
				{
					vel = ent->progs.sv->velocity;
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

		if ((int)ent->progs.sv->flags & FL_MONSTER) mask = MASK_MONSTERSOLID;
		else if(ent->progs.sv->movetype == MOVETYPE_PUSHABLE) mask = MASK_MONSTERSOLID | MASK_PLAYERSOLID;
		else if(ent->priv.sv->clipmask) mask = ent->priv.sv->clipmask; // Lazarus edition
		else mask = MASK_SOLID;

		if (ent->progs.sv->movetype == MOVETYPE_PUSHABLE)
		{
			block = SV_PushableMove (ent, 0.1f, mask);
			if(block && !(block & 8) && onconveyor)
			{
				T_Damage(ent, prog->edicts, prog->edicts, vec3_origin, ent->priv.sv->s.origin, vec3_origin, 100000, 1, 0, DMG_CRUSH);
				if(ent->priv.sv->free) return;
			}
		}
		else
		{
			block = SV_FlyMove (ent, 0.1f, mask);
			if(block && !(block & 8) && onconveyor)
			{
				T_Damage (ent, prog->edicts, prog->edicts, vec3_origin, ent->priv.sv->s.origin, vec3_origin, 100000, 1, 0, DMG_CRUSH);
				if(ent->priv.sv->free) return;
			}
		}
		SV_LinkEdict (ent);
		SV_TouchTriggers (ent);
		if (ent->priv.sv->free) return;

		if (ent->progs.sv->groundentity && !wasonground && hitsound)
		{
			SV_StartSound (NULL, ent, 0, SV_SoundIndex("world/land.wav"), 1, 1, 0);
		}

		// Move func_pushable riders
		if(ent->progs.sv->movetype == MOVETYPE_PUSHABLE)
		{
			trace_t	tr;

			if(ent->progs.sv->bouncetype == 2) VectorMA(old_origin,0.1f,ent->progs.sv->velocity,ent->priv.sv->s.origin);
			VectorSubtract(ent->priv.sv->s.origin,old_origin,move);
			for(i = 1, e = PRVM_EDICT_NUM(i); i < prog->num_edicts; i++, e = PRVM_EDICT_NUM(i))
			{
				if(e==ent) continue;
				if(e->progs.sv->groundentity == PRVM_EDICT_TO_PROG(ent))
				{
					VectorAdd(e->priv.sv->s.origin,move,end);
					tr = SV_Trace(e->priv.sv->s.origin, e->progs.sv->mins, e->progs.sv->maxs, end, ent, MASK_SOLID);
					VectorCopy(tr.endpos,e->priv.sv->s.origin);
					SV_LinkEdict(e);
				}
			}
		}
	}
	else if(ent->progs.sv->movetype == MOVETYPE_PUSHABLE)
	{
		// We run touch function for non-moving func_pushables every frame
		// to see if they are touching, for example, a trigger_mass
		SV_TouchTriggers(ent);
		if(ent->priv.sv->free) return;
	}


	// Lazarus: Add falling damage for entities that can be damaged
	if( ent->progs.sv->takedamage )
	{
		SV_FallingDamage(ent);
		VectorCopy(ent->progs.sv->velocity,ent->progs.sv->post_velocity);
	}

	if ((!oldwaterlevel && ent->progs.sv->waterlevel) && !ent->progs.sv->groundentity)
	{
		if( ((int)ent->progs.sv->watertype & CONTENTS_SLIME) || ((int)ent->progs.sv->watertype & CONTENTS_WATER) )
			SV_StartSound (NULL, ent, CHAN_BODY, SV_SoundIndex("player/watr_in.wav"), 1, ATTN_NORM, 0);
		else if((int)ent->progs.sv->watertype & CONTENTS_MUD)
			SV_StartSound (NULL, ent, CHAN_BODY, SV_SoundIndex("mud/mud_in2.wav"), 1, ATTN_NORM, 0);
	}

	// regular thinking
	SV_RunThink (ent);
	VectorCopy(ent->progs.sv->velocity,ent->progs.sv->post_velocity);
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

	VectorCopy (ent->priv.sv->s.origin, start);
	VectorAdd (start, push, end);
	if(ent->priv.sv->clipmask) mask = ent->priv.sv->clipmask;
	else mask = MASK_SHOT;

	trace = SV_Trace (start, ent->progs.sv->mins, ent->progs.sv->maxs, end, ent, mask);
	VectorCopy (trace.endpos, ent->priv.sv->s.origin);
	SV_LinkEdict (ent);

	if (trace.fraction != 1.0)
	{
		if( (trace.surface) && (trace.surface->flags & SURF_SKY))
		{
			SV_FreeEdict(ent);
			return trace;
		}
		if(trace.ent->priv.sv->client || ((int)trace.ent->progs.sv->flags & FL_MONSTER) )
		{
			// touching a player or monster
			// if rock has no mass we really don't care who it hits
			if(!ent->progs.sv->mass) return trace;
			speed1 = VectorLength(ent->progs.sv->velocity);
			if(!speed1) return trace;
			speed2 = VectorLength(trace.ent->progs.sv->velocity);
			VectorCopy(ent->progs.sv->velocity,v1);
			VectorNormalize(v1);
			VectorCopy(trace.ent->progs.sv->velocity,v2);
			VectorNormalize(v2);
			dot = -DotProduct(v1,v2);
			speed1 += speed2 * dot;
			if(speed1 <= 0) return trace;
			scale = (float)ent->progs.sv->mass/200.*speed1;
			VectorMA(trace.ent->progs.sv->velocity,scale,v1,trace.ent->progs.sv->velocity);
			// Take a swag at it... 

			if(speed1 > 100)
			{
				damage = (int)(ent->progs.sv->mass * speed1 / 5000.);
				if(damage) T_Damage(trace.ent, prog->edicts, prog->edicts, v1, trace.ent->priv.sv->s.origin, vec3_origin, damage, 0, 0, DMG_CRUSH);
			}

			PRVM_PUSH_GLOBALS;

			prog->globals.server->pev = PRVM_EDICT_TO_PROG(ent);
			prog->globals.server->other = PRVM_EDICT_TO_PROG(trace.ent); //correct ?
			prog->globals.server->time = sv.time;
			prog->globals.server->trace_allsolid = trace.allsolid;
			prog->globals.server->trace_startsolid = trace.startsolid;
			prog->globals.server->trace_fraction = trace.fraction;
			prog->globals.server->trace_contents = trace.contents;
			VectorCopy (trace.endpos, prog->globals.server->trace_endpos);
			VectorCopy (trace.plane.normal, prog->globals.server->trace_plane_normal);
			prog->globals.server->trace_plane_dist =  trace.plane.dist;
			if (trace.ent) prog->globals.server->trace_ent = PRVM_EDICT_TO_PROG(trace.ent);
			else prog->globals.server->trace_ent = PRVM_EDICT_TO_PROG(prog->edicts);
			PRVM_ExecuteProgram (ent->progs.sv->touch, "QC function pev->touch is missing");
			PRVM_POP_GLOBALS;

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

	if (ent->progs.sv->velocity[2] > 0)
		ent->progs.sv->groundentity = 0;

	// check for the groundentity going away
	if (ent->progs.sv->groundentity)
		if (PRVM_PROG_TO_EDICT(ent->progs.sv->groundentity)->priv.sv->free)
			ent->progs.sv->groundentity = 0;

	// if onground, return without moving
	if ( ent->progs.sv->groundentity )
		return;

	VectorCopy (ent->priv.sv->s.origin, old_origin);
	SV_CheckVelocity (ent);
	SV_AddGravity (ent);

	// move angles
	VectorMA (ent->priv.sv->s.angles, 0.1f, ent->progs.sv->avelocity, ent->priv.sv->s.angles);

	// move origin
	VectorScale (ent->progs.sv->velocity, 0.1f, move);
	trace = SV_DebrisEntity (ent, move);
	if (ent->priv.sv->free) return;

	if (trace.fraction < 1)
	{
		backoff = 1.0;
		ClipVelocity (ent->progs.sv->velocity, trace.plane.normal, ent->progs.sv->velocity, backoff);

		// stop if on ground
		if (trace.plane.normal[2] > 0.3)
		{		
			if (ent->progs.sv->velocity[2] < 60)
			{
				ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
				VectorCopy (vec3_origin, ent->progs.sv->velocity);
				VectorCopy (vec3_origin, ent->progs.sv->avelocity);
			}
		}
	}
	
	// check for water transition
	wasinwater = ((int)ent->progs.sv->watertype & MASK_WATER);
	ent->progs.sv->watertype = SV_PointContents (ent->priv.sv->s.origin);
	isinwater = (int)ent->progs.sv->watertype & MASK_WATER;

	if (isinwater) ent->progs.sv->waterlevel = 1;
	else ent->progs.sv->waterlevel = 0;

	if (!wasinwater && isinwater) SV_StartSound (old_origin, prog->edicts, CHAN_AUTO, SV_SoundIndex("misc/h2ohit1.wav"), 1, 1, 0);
	else if (wasinwater && !isinwater) SV_StartSound (ent->priv.sv->s.origin, prog->edicts, CHAN_AUTO, SV_SoundIndex("misc/h2ohit1.wav"), 1, 1, 0);

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

	VectorScale(ent->progs.sv->movedir,ent->progs.sv->speed,v);
	VectorScale(v, 0.1f, move);

	for(i = 0; i < maxclients->value; i++)
	{
		player = PRVM_EDICT_NUM(i) + 1;
		if(player->priv.sv->free) continue;
		if(!player->progs.sv->groundentity) continue;
		if(player->progs.sv->groundentity != PRVM_EDICT_TO_PROG(ent)) continue;

		// Look below player; make sure he's on a conveyor
		VectorCopy(player->priv.sv->s.origin,point);
		point[2] += 1;
		VectorCopy(point,end);
		end[2] -= 256;

		tr = SV_Trace (point, player->progs.sv->mins, player->progs.sv->maxs, end, player, MASK_SOLID);
		// tr.ent HAS to be conveyor, but just in case we screwed something up:
		if(tr.ent == ent)
		{
			if(tr.plane.normal[2] > 0)
			{
				v[2] = ent->progs.sv->speed * sqrt(1.0 - tr.plane.normal[2]*tr.plane.normal[2]) / tr.plane.normal[2];
				if(DotProduct(ent->progs.sv->movedir,tr.plane.normal) > 0)
				{
					// then we're moving down
					v[2] = -v[2];
				}
				move[2] = v[2] * 0.1f;
			}
			VectorAdd(player->priv.sv->s.origin,move,end);
			tr = SV_Trace(player->priv.sv->s.origin,player->progs.sv->mins,player->progs.sv->maxs,end,player,player->priv.sv->clipmask);
			VectorCopy(tr.endpos,player->priv.sv->s.origin);
			SV_LinkEdict(player);
		}
	}
}

void SV_Physics(edict_t *ent)
{
	wasonground = false;
	onconveyor = false;

	switch ((int)ent->progs.sv->movetype)
	{
	case MOVETYPE_NONE:
		SV_Physics_None (ent);
		break;
	case MOVETYPE_PUSH:
		SV_MovetypePush (ent);
		//SV_Physics_Pusher (ent);
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
	case MOVETYPE_WALK:
		SV_Physics_None(ent);
		break;
	case MOVETYPE_CONVEYOR:
		SV_Physics_Conveyor(ent);
		break;
	default:
		PRVM_ERROR ("SV_Physics: bad movetype %i", (int)ent->progs.sv->movetype);			
	}
}