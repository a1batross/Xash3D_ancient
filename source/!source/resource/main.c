/*
+----+
|Main|
+----+-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
| Scratch                                      Http://www.admdev.com/scratch |
+=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
| Contains some 'base' subroutines. As a general rule nothing in this file   |
| does much, except to setup basic variables and entities.                   |
+=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
*/

//INCLUDES;

void() LightStyles_setup;       // Entities/Lights.QC
void() precaches;

//END INCLUDES;

void() main = {};

/*
================
|SETNEWPARMS():|
=================================================================================
Description:
This function is called to set up new player info.
=================================================================================
*/

void() SetNewParms = //Sets up start game parms
{
	parm1 = 10;
	parm2 = 100;
};

/*
==================
|SETCHANGEPARMS():|
=================================================================================
Description:

This function is called when the player hits a change level. Stores player info for
loading upon next level.
=================================================================================
*/

void() SetChangeParms = //called on changelevel; command 
{
	if (self.health <= 0)
	{
		SetNewParms();
		return;
	}

	parm1 = self.items;
	parm2 = self.health;
};

/*
==================
|GETLEVELPARMS():|
=================================================================================
Description:

This function is called by 'PUTCLIENTINSERVER(); in (CLIENT.QC)' and carries over
information in between level loads, or sets new parms at start map.

Information stored in .parms which are the ONLY floats to be stored in memory in
between level loads.
=================================================================================
*/

void() GetLevelParms = 
{
	if (world.model == "maps/start.bsp")
		SetNewParms ();		// take away all stuff on starting new episode

	self.items = parm1;
	self.health = parm2;
};


void() StartFrame = {};
void() EndFrame = {};

/*
===============
|WORLDSPAWN():|
=================================================================================
Description:
This function is called when the world spawns.
=================================================================================
*/

void() worldspawn = 
{
	precaches(); 
	LightStyles_setup();
};

/*
==============
|PRECACHES():|
=================================================================================
Description:
Precaches for the game.
=================================================================================
*/

void() precaches =
{
 precache_model ("progs/player.mdl");
 precache_model("progs/s_bubble.spr");
 precache_model("progs/eyes.mdl");

 precache_model ("progs/enforcer.mdl"); //testing only

// pain sounds
    precache_sound ("player/drown1.wav");    // drowning pain
    precache_sound ("player/drown2.wav");    // drowning pain
    precache_sound ("player/lburn1.wav");    // slime/lava burn
    precache_sound ("player/lburn2.wav");    // slime/lava burn
    precache_sound ("player/pain1.wav");
    precache_sound ("player/pain2.wav");
    precache_sound ("player/pain3.wav");
    precache_sound ("player/pain4.wav");
    precache_sound ("player/pain5.wav");
    precache_sound ("player/pain6.wav");

// death sounds
    precache_sound ("player/h2odeath.wav");    // drowning death
    precache_sound ("player/death1.wav");
    precache_sound ("player/death2.wav");
    precache_sound ("player/death3.wav");
    precache_sound ("player/death4.wav");
    precache_sound ("player/death5.wav");
};
