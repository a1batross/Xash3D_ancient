/*
+====================
|MOVERS.QC
+--------
|Description;
+------------
|This file handles movers in BaseQC.
+----------------------------------
|What each void() does;
+----------------------

This is a unified door_button function...

FUNC_MOVER(); This is the spawning function for movers;

FUNC_MOVER_TOUCH();	Called when our mover is touched by something;
			Works out whether a player is touching it.
			Then sets up next think function to happen in 0.1 secs;
			Makes mover un touchable;
FUNC_MOVER_THINK();	Decides whether mover is open or closed;
			Then moves mover appropiately;

FUNC_MOVER_MOVE();	Called upon our mover wishing to move;
			Works out maths of moving mover to destination;
			Sets a think for when journey is completed;

FUNC_MOVER_STOP();	Called when our mover wishes to stop;
			Especially if i want to return;

FUNC_MOVER_STOP_DEAD():	"				"
			This one just stops it dead with no return;


FUNC_MOVER_USE();	Called when our mover is targeted by IEM_usetargets;
			Basically does the same as touch function but fakes it;

FUNC_MOVER_BLOCKED():	Called when our mover is blocked by an entity;
			Hurts whatever is blocking it and goes back to where it was coming from;

FUNC_MOVER_DIE();	Called upon our movers death(when health is zero);
			Toggles mover upon death;

FUNC_MOVER_FIRE();	allows me to have a target that is triggered after movers delay..

=====================
*/

//DEFINITIONS FOR FILE;

void() func_mover_think;	//from movers.QC

.vector dest;
.string target_dest;

//Floats below this line are used in the mapping .fgd file and inputted by the mapper. (FLAGS)

float MOVER_START_OPEN = 1;
float MOVER_DONT_LINK = 4;
float MOVER_GOLD_KEY = 8;
float MOVER_SILVER_KEY = 16;
float MOVER_TOGGLE = 32;

//END DEFS;

void() func_mover_stop_general =
{
	self.velocity = '0 0 0';				//Stop me!
	self.touched = FALSE;					//Touch me!
	sound (self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);	//make a sound!
	setorigin(self, self.dest);				//set my origin exactly to dest.
};

void() func_mover_stop =
{
	func_mover_stop_general();

	if(self.wait >= 0)					//Return upon wait over!
	{
		self.think = func_mover_think;
		self.nextthink = self.ltime + self.wait;
	}
};

void() func_mover_stop_dead = 
{
	func_mover_stop_general();
};

void() func_mover_blocked = 
{
	T_Damage (other, self, self, self.dmg);			//Do my damage;
		
	func_mover_think();					//Return;
};

void(vector destination, float movespeed, void() dest_func) func_mover_move = 
{
	local vector path;
	local float pathlength, traveltime;
	
	//Calculate movement vector
	path = destination - self.origin;
	
	//Calculate length of movement vector;
	pathlength = vlen(path);
	
	//Divide length of movement vector by desired movespeed to get travel time.
	traveltime = (pathlength) / (movespeed);

	// scale the destdelta vector by the time spent traveling to get velocity
	self.velocity = path * (1/traveltime);	

	if(traveltime < 0.1 || self.origin == destination)
	{
		self.think = dest_func;
		self.nextthink = self.ltime + 0.1;
		self.dest = destination;
		return;
	}

	self.think = dest_func;
	self.nextthink = self.ltime + traveltime;
	
	self.dest = destination;
};

void() func_mover_think = 
{
	local vector a;
	local void() b;

	if(self.state == STATE_OPEN)				//Am i open?
	{
		self.state = STATE_CLOSED;			//Now im closing!
		a = self.pos1;					//My first position!
		b = func_mover_stop_dead;			//Stopping func to use!
	}
	else if (self.state == STATE_CLOSED)
	{
		self.state = STATE_OPEN;
		a = self.pos2;
		
		if(self.spawnflags & MOVER_TOGGLE)		//Am i toggable?
			b = func_mover_stop_dead;		//if yes.. stop me dead.
		else
			b = func_mover_stop;			//if no.. return me;
	}
		
	func_mover_move(a, self.speed, b);			//Loaded move function;

	sound (self, CHAN_VOICE, self.noise2, 1, ATTN_NORM);
};

void() func_mover_fire = 
{
	IEM_usetarget();				//Use my targets;
		
	func_mover_think();			//Run think function straight away!
};

void() func_mover_touch = 
{
	if(other.classname != "player")			//Are you a player?
		return;
	if(other == world)				//Are you the world?
		return;
	
	if(self.touched == FALSE)
	{	
		self.triggerer = other;
		
		self.touched = TRUE;				//stop touching me!
		self.think = func_mover_fire;			//set me next think
		self.nextthink = self.ltime + self.delay;	//set it so it happens in 0.1 secs from now.
	}
};

void() func_mover_use = 
{
	if(self.message)
		centerprint(self.triggerer, self.message);
	
	self.touched = TRUE;					//Fake touch!
	
	self.think = func_mover_fire;				//set me next think!
	self.nextthink = self.ltime + self.delay;		//in delay
};

void() func_mover_die = 
{
	self.health = self.max_health;			//reset health on death;
		
	self.takedamage = DAMAGE_YES;			//These two are set to no and null in killed function;
	self.touch = func_mover_touch;			//[Damage.QC]
};

void() func_mover = 
{
	func_setup();					//Sets up some basic func properties;[funcs.qc]
	self.classname = "mover";
	
	if(self.health)
	{
		self.max_health = self.health;
		self.takedamage = DAMAGE_YES;
	}

	self.blocked = func_mover_blocked;
	self.use = func_mover_use;
	self.touch = func_mover_touch;
	self.th_die = func_mover_die;

	//func_mover; DEFAULTS;
	if (!self.speed)
		self.speed = 100;
	if (!self.wait)
		self.wait = 3;
	if (!self.lip)
		self.lip = 8;
	if (!self.dmg)
		self.dmg = 2;
	
	if(!self.delay)
		self.delay = 0.1;	//stuff with 0 delay dont move! (or work ;) )
	

	//func_mover; positions; .pos1 == closed; .pos2 == open;

	self.pos1 = self.origin;
	self.pos2 = self.pos1 + self.movedir*(fabs(self.movedir*self.size) - self.lip);
	
	if(self.spawnflags & MOVER_START_OPEN)
	{	
		self.state = STATE_OPEN;
		setorigin(self, self.pos2);
	}
	else
		self.state = STATE_CLOSED;
};

