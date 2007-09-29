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
	pev->velocity = '0 0 0';				//Stop me!
	pev->touched = FALSE;					//Touch me!
	sound (pev, CHAN_VOICE, pev->noise1, 1, ATTN_NORM);	//make a sound!
	setorigin(pev, pev->dest);				//set my origin exactly to dest.
};

void() func_mover_stop =
{
	func_mover_stop_general();

	if(pev->wait >= 0)					//Return upon wait over!
	{
		pev->think = func_mover_think;
		pev->nextthink = pev->ltime + pev->wait;
	}
};

void() func_mover_stop_dead = 
{
	entity	t;

	// lookup all areaportals
	t = find(world, targetname, pev->target);
	while(t)
	{
		if(t->classname == "func_areaportal")
			areaportal_state( t->style, FALSE );
		t = find(t, targetname, pev->target);
	}

	func_mover_stop_general();
};

void() func_mover_blocked = 
{
	T_Damage (other, pev, pev, pev->dmg);			//Do my damage;
		
	func_mover_think();					//Return;
};

void(vector destination, float movespeed, void() dest_func) func_mover_move = 
{
	local vector path;
	local float pathlength, traveltime;
	
	//Calculate movement vector
	path = destination - pev->origin;
	
	//Calculate length of movement vector;
	pathlength = vlen(path);
	
	//Divide length of movement vector by desired movespeed to get travel time.
	traveltime = (pathlength) / (movespeed);

	// scale the destdelta vector by the time spent traveling to get velocity
	pev->velocity = path * (1/traveltime);	

	if(traveltime < 0.1 || pev->origin == destination)
	{
		pev->think = dest_func;
		pev->nextthink = pev->ltime + 0.1;
		pev->dest = destination;
		return;
	}

	pev->think = dest_func;
	pev->nextthink = pev->ltime + traveltime;
	
	pev->dest = destination;
};

void() func_mover_think = 
{
	local vector a;
	local void() b;

	if(pev->state == STATE_OPEN)				//Am i open?
	{
		pev->state = STATE_CLOSED;			//Now im closing!
		a = pev->pos1;					//My first position!
		b = func_mover_stop_dead;			//Stopping func to use!
	}
	else if (pev->state == STATE_CLOSED)
	{
		pev->state = STATE_OPEN;
		a = pev->pos2;
		
		if(pev->spawnflags & MOVER_TOGGLE)		//Am i toggable?
			b = func_mover_stop_dead;		//if yes.. stop me dead.
		else
			b = func_mover_stop;			//if no.. return me;
	}
		
	func_mover_move(a, pev->speed, b);			//Loaded move function;

	sound (pev, CHAN_VOICE, pev->noise2, 1, ATTN_NORM);
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
	
	if(pev->touched == FALSE)
	{	
		pev->triggerer = other;
		
		pev->touched = TRUE;			//stop touching me!
		pev->think = func_mover_fire;			//set me next think
		pev->nextthink = pev->ltime + pev->delay;	//set it so it happens in 0.1 secs from now.
	}
};

void() func_mover_use = 
{
	if(pev->message)
		centerprint(pev->triggerer, pev->message);
	
	pev->touched = TRUE;					//Fake touch!
	
	pev->think = func_mover_fire;				//set me next think!
	pev->nextthink = pev->ltime + pev->delay;		//in delay
};

void() func_mover_die = 
{
	pev->health = pev->max_health;			//reset health on death;
		
	pev->takedamage = DAMAGE_YES;			//These two are set to no and null in killed function;
	pev->touch = func_mover_touch;			//[Damage.QC]
};

void() func_mover = 
{
	func_setup();					//Sets up some basic func properties;[funcs.qc]
	pev->classname = "mover";
	
	if(pev->health)
	{
		pev->max_health = pev->health;
		pev->takedamage = DAMAGE_YES;
	}

	pev->blocked = func_mover_blocked;
	pev->use = func_mover_use;
	pev->th_die = func_mover_die;

	if(!pev->targetname) pev->touch = func_mover_touch;

	//func_mover; DEFAULTS;
	if (!pev->speed)
		pev->speed = 100;
	if (!pev->wait)
		pev->wait = 3;
	if (!pev->lip)
		pev->lip = 8;
	if (!pev->dmg)
		pev->dmg = 2;
	
	if(!pev->delay)
		pev->delay = 0.1;	//stuff with 0 delay dont move! (or work ;) )
	

	//func_mover; positions; .pos1 == closed; .pos2 == open;

	pev->pos1 = pev->origin;
	pev->pos2 = pev->pos1 + pev->movedir*(fabs(pev->movedir*pev->size) - pev->lip);
	
	if(pev->spawnflags & MOVER_START_OPEN)
	{	
		pev->state = STATE_OPEN;
		setorigin(pev, pev->pos2);
	}
	else
		pev->state = STATE_CLOSED;
};

