/*
+------+
|Damage|
+------+-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
| Scratch                        http://www.inside3d.com/qctut/scratch.shtml |
+=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
| T_Damage and other like functions                                          |
+=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
*/
//DEFS
void(entity who_died, entity who_killed) ClientObiturary;
//END DEFS
/*
=-=-=-=-=
 Killed
=-=-=-=-=
*/

void(entity targ, entity attacker) Killed =
{
	local entity oldpev;

	if (targ.health < -99)
		targ.health = -99;		// don't let sbar look bad if a player

	targ.takedamage = DAMAGE_NO;
	targ.touch = SUB_Null;

	oldpev = pev;
	pev = targ; // pev must be targ for th_die
	pev->th_die ();
	pev = oldpev;
	
	ClientObiturary(targ, attacker);
};

/*
+=======+
|T_Heal|
+=======+
|Heal entity e for healamount possibly ignoring max health.|
+======================================================+
*/

void(entity e, float healamount, float ignore) T_Heal =
{
	if (e.health <= 0)
		return;
	
	if ((!ignore) && (e.health >= other.max_health))
		return;
	
	healamount = ceil(healamount);
	
	e.health = e.health + healamount;
	
	if ((!ignore) && (e.health >= other.max_health))
		e.health = other.max_health;
};

/*
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
T_Damage

The damage is coming from inflictor, but get mad at attacker
This should be the only function that ever reduces health.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/
void(entity targ, entity inflictor, entity attacker, float damage) T_Damage=
{
    local	vector	dir;
    local	entity	oldpev;

    if (!targ.takedamage)
        return;

// used by buttons and triggers to set activator for target firing
    damage_attacker = attacker;

// figure momentum add
    if ( (inflictor != world) && (targ.movetype == MOVETYPE_WALK) )
    {
        dir = targ.origin - (inflictor.absmin + inflictor.absmax) * 0.5;
        dir = normalize(dir);
        targ.velocity = targ.velocity + dir*damage*8;
    }

// check for godmode
    if (targ.aiflags & AI_GODMODE)
        return;

// add to the damage total for clients, which will be sent as a single
// message at the end of the frame
    if (targ.flags & FL_CLIENT)
    {
        targ.dmg_take = targ.dmg_take + damage;
        targ.dmg_save = targ.dmg_save + damage;
        targ.dmg_inflictor = inflictor;
    }

// team play damage avoidance
    if ( (teamplay == 1) && (targ.team > 0)&&(targ.team == attacker.team) )
        return;
		
// do the damage
    targ.health = targ.health - damage;

    if (targ.health <= 0)
    {
        Killed (targ, attacker);
        return;
    }

// react to the damage
    oldpev = pev;
    pev = targ;

    if (pev->th_pain)
        pev->th_pain (attacker, damage);

    pev = oldpev;
};


/*
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
WaterMove

Can be used for clients or monsters
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/
void() WaterMove =
{
    if (pev->movetype == MOVETYPE_NOCLIP)
        return;
    if (pev->health < 0)
        return;

    if (pev->waterlevel != 3)
    {
        pev->air_finished = time + 12;
        pev->dmg = 2;
    }
    else if (pev->air_finished < time && pev->pain_finished < time)
    {   // drown!
        pev->dmg = pev->dmg + 2;
        if (pev->dmg > 15)
            pev->dmg = 10;
        T_Damage (pev, world, world, pev->dmg);
        pev->pain_finished = time + 1;
    }

    if (pev->watertype == CONTENT_LAVA && pev->dmgtime < time)
    {   // do damage
        pev->dmgtime = time + 0.2;
        T_Damage (pev, world, world, 6*pev->waterlevel);
    }
    else if (pev->watertype == CONTENT_SLIME && pev->dmgtime < time)
    {   // do damage
        pev->dmgtime = time + 1;
        T_Damage (pev, world, world, 4*pev->waterlevel);
    }
};