/*
+====================+
|TRIGGER_TELEPORT|
+====================+
*/
void(entity ono, entity duo)setangles =
{
	ono.angles = duo.angles;
	ono.fixangle = 1;		// turn this way immediately	
};

void() spawn_tdeath_touch =
{
	if (other == pev->owner)
		return;

	// frag anyone who teleports in on top of an invincible player
	if (other.classname == "player")
	{
		if (pev->owner.classname != "player")
		{	
			T_Damage (pev->owner, pev, pev, 50000);
			return;
		}
		
	}

	if (other.health)
	{
		T_Damage (other, pev, pev, 50000);
	}
};


void(vector death_org, entity death_owner) spawn_tdeath =
{
	local entity	death;

	death = create("teledeath", "", death_org);
	death.classname = "teledeath";
	death.owner = death_owner;
	
	death.movetype = MOVETYPE_NONE;
	death.solid = SOLID_TRIGGER;
	
	setorigin (death, death_org);
	setsize (death, death_owner.mins - '1 1 1', death_owner.maxs + '1 1 1');
	
	death.touch = spawn_tdeath_touch;
	
	death.think = SUB_Remove;
	death.nextthink = time + 0.2;
};

void() trigger_teleport_think = 
{
	local entity t;

	t = find (world, targetname, pev->target);
	
	if (!t) Error("couldn't find target\n");
	
	IEM_effects(TE_TELEPORT, pev->triggerer.origin);
	
	spawn_tdeath(t.origin, pev->triggerer);

	setorigin(pev->triggerer, t.origin);
	setangles(pev->triggerer, t);

	IEM_effects(TE_TELEPORT, t.origin);
	
	if(pev->message)
		centerprint(pev->triggerer, pev->message);
	
	pev->touched = FALSE;	
};

void() trigger_teleport_touch = 
{
	if(!(other.flags & FL_CLIENT))
		return;

	if(pev->touched == FALSE)
	{
		pev->touched = TRUE;
	
		pev->triggerer = other;

		pev->think = trigger_teleport_think;
		pev->nextthink = time + pev->delay;
	}
};

void() trigger_teleport_use =
{	
	//if(pev->touched == TRUE)
	//	return;
	if(pev->touched == TRUE)
		pev->touched = FALSE;
	//pev->think = trigger_teleport_think;
	//pev->nextthink = time + pev->delay;
};


void() trigger_teleport =
{
	trigger_setup();

	pev->touch = trigger_teleport_touch;
	pev->use = trigger_teleport_use;

	if (!pev->target) Error("no target\n");
		
	if(pev->spawnflags & TRIGGER_START_OFF)
		pev->touched = TRUE;
	
	if(!pev->delay)
		pev->delay = 0.1;
	
	pev->classname = "t_teleport";

	/*
	if (!(pev->spawnflags & SILENT))
	{
		//precache_sound ("ambience/hum1.wav");
		//o = (pev->mins + pev->maxs)*0.5;
		//ambientsound (o, "ambience/hum1.wav",0.5 , ATTN_STATIC);
	}
	*/
};

void() info_teleport_destination =
{
	// this does nothing, just serves as a target spot
	pev->mangle = pev->angles;
	pev->angles = '0 0 0';
	pev->model = "";
	pev->origin = pev->origin + '0 0 27';
	if (!pev->targetname) Error("no targetname\n");
};


