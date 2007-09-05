/*
+====================+
|TRIGGER_TELEPORT|
+====================+
*/
void(entity ono, entity duo)setangles =
{
	ono.angles = duo.mangle;
	ono.fixangle = 1;		// turn this way immediately	
};

void() spawn_tdeath_touch =
{
	if (other == self.owner)
		return;

	// frag anyone who teleports in on top of an invincible player
	if (other.classname == "player")
	{
		if (self.owner.classname != "player")
		{	
			T_Damage (self.owner, self, self, 50000);
			return;
		}
		
	}

	if (other.health)
	{
		T_Damage (other, self, self, 50000);
	}
};


void(vector death_org, entity death_owner) spawn_tdeath =
{
	local entity	death;

	death = spawn();
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

	t = find (world, targetname, self.target);
	
	if (!t)
		objerror ("couldn't find target");
	
	IEM_effects(TE_TELEPORT, self.triggerer.origin);
	
	spawn_tdeath(t.origin, self.triggerer);

	setorigin(self.triggerer, t.origin);
	setangles(self.triggerer, t);

	IEM_effects(TE_TELEPORT, t.origin);
	
	if(self.message)
		centerprint(self.triggerer, self.message);
	
	self.touched = FALSE;	
};

void() trigger_teleport_touch = 
{
	if(!(other.flags & FL_CLIENT))
		return;

	if(self.touched == FALSE)
	{
		self.touched = TRUE;
	
		self.triggerer = other;

		self.think = trigger_teleport_think;
		self.nextthink = time + self.delay;
	}
};

void() trigger_teleport_use =
{	
	//if(self.touched == TRUE)
	//	return;
	if(self.touched == TRUE)
		self.touched = FALSE;
	//self.think = trigger_teleport_think;
	//self.nextthink = time + self.delay;
};


void() trigger_teleport =
{
	trigger_setup();

	self.touch = trigger_teleport_touch;
	self.use = trigger_teleport_use;

	if (!self.target)
		objerror ("no target");
		
	if(self.spawnflags & TRIGGER_START_OFF)
		self.touched = TRUE;
	
	if(!self.delay)
		self.delay = 0.1;
	
	self.classname = "t_teleport";

	/*
	if (!(self.spawnflags & SILENT))
	{
		//precache_sound ("ambience/hum1.wav");
		//o = (self.mins + self.maxs)*0.5;
		//ambientsound (o, "ambience/hum1.wav",0.5 , ATTN_STATIC);
	}
	*/
};

void() info_teleport_destination =
{
// this does nothing, just serves as a target spot
	self.mangle = self.angles;
	self.angles = '0 0 0';
	self.model = "";
	self.origin = self.origin + '0 0 27';
	if (!self.targetname)
		objerror ("no targetname");
};


