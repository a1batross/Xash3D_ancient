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
	pev->items = 10;
	pev->health = 100;
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
	if (pev->health <= 0)
	{
		SetNewParms();
		return;
	}
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
};


void() StartFrame = {};
void() EndFrame = {};

// hud program string
string single_statusbar = 
"yb -24 xv 0 hnum xv 50 pic 0 \
if 2 { xv 100 anum xv 150 pic 2 } \
if 4 { xv 200 rnum xv 250 pic 4 } \
if 6 { xv 296 pic 6 } yb -50 \
if 7 { xv 0 pic 7 xv 26 yb -42 stat_string 8 yb -50 } \
if 9 { xv 230 num 4 10 xv 296 pic 9 } \
if 11 { xv 148 pic 11 } \
if 22 { yb -90 xv 128 pic 22 } \
if 23 { yv 0 xv 0 pic 23 }";

/*
===============
|WORLDSPAWN():|
=================================================================================
Description:
This function is called when the world spawns.
=================================================================================
*/

void worldspawn( void ) 
{
	MsgWarn("world spawned\n");
	precaches(); 
	LightStyles_setup();

	// CS_MAXCLIENTS already sended by engine
	configstring (CS_STATUSBAR, single_statusbar );
	configstring (CS_SKY, "sky" );
	configstring (CS_SKYROTATE, ftoa( pev->speed ));		// rotate speed
	configstring (CS_SKYAXIS, vtoa( pev->angles ));	// rotate axis
	configstring (CS_CDTRACK, ftoa( 0 ));
}

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
	precache_model ("models/player.mdl");
	precache_model("models/supp1.mdl");

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
