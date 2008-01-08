/*
+------+
|Client|
+------+-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
| Scratch                                      Http://www.admdev.com/scratch         |
+=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
| Handle's "clients" (eg, Players) connecting, disconnecting, etc.                  |
+=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
*/

//DEFS;

void() CCamChasePlayer;      // From Ccam.qc	
void() CheckImpulses;	     // From Impulses.QC
void() PutClientInServer;  //From Client.QC

//END DEFS;
.float	showhelp;
.float	showinventory;

/*
+=========+
|PPRINT():|    
+=========+==============================================================================+
|Description:                                                                                                                   |
|This function prints a server wide(bprint) 'entity' 'did' 'what" message... v useful for all those|
|client joining, leaving msgs etc.. saves a fair amount of code i hope.. [8 lines i think..]               |
+========================================================================================+
*/

void(entity dude, string did, string what)pprint = 
{
	bprint("\n");
	bprint(dude.netname);
	bprint(did);
	bprint(what);
	bprint("\n");
};

/*
CLIENTRESPAWN();
*/

void() ClientRespawn = 
{
	if (coop)
	{
		// get the spawn parms as they were at level start
		GetLevelParms();
		// respawn		
		PutClientInServer();
	}
	else if (deathmatch)
	{
		// set default spawn parms
		SetNewParms();
		// respawn		
		PutClientInServer();
	}
	else
	{	// restart the entire server
		server_command ("restart\n");
	}
};

/*
CLIENTOBITURARY()

Description;
Describes the entity 'who_died' in relation to enitity 'who_killed'.
Called when a player gets 'killed' by KILLED(); [DAMAGE.QC]
*/

void(entity who_died, entity who_killed) ClientObiturary = 
{
	local string deathstring;
	local float rnum, msgdt;
	
	rnum = random_float(0, 1);
	
	if(who_died.flags & FL_CLIENT)
	{
		if(who_killed == world)
		{
			deathstring = "was killed";
			
			if(who_died.watertype == CONTENT_WATER)
				deathstring = " drowned";
			else if(who_died.watertype == CONTENT_SLIME)
				deathstring = " melted";
			else if(who_died.watertype == CONTENT_LAVA)
				deathstring = " got incinerated";
			
			msgdt = TRUE;
		}
		
		if(who_killed.classname == "door")
		{
			if(rnum < 0.25)
			{
				deathstring = " got crushed";
			}
			else
				deathstring = " angered the ";
		}
		
		if(who_killed.classname == "button")
		{
			if(rnum < 0.25)
			{
				deathstring = " pushed it the wrong way";
				msgdt = TRUE;
			}
			else
				deathstring = " angered the ";
		}
		
		if(who_killed.classname == "train")
		{
			deathstring = " jumped infront the ";
		}
		
		if(who_killed.classname == "teledeath")
		{
			deathstring = " was telefragged by ";
		}
		
		if(who_killed.classname == "t_hurt")
		{
			deathstring = " got hurt too much...";
		}
		
		if(who_killed.classname == "t_push")
		{
			deathstring = " got pushed too far...";
		}
		
		if(who_killed == who_died)
		{
			deathstring = " killed themselves...";
			msgdt = TRUE;
		}
		
		bprint(who_died.netname);
		bprint(deathstring);
		
		if(msgdt != TRUE)
		{
			if(who_killed.flags & FL_CLIENT)
				bprint(who_killed.netname);
			else 
				bprint(who_killed.classname);
		}
		
		bprint("\n");
	}
};

/*
===============
|CLIENTKILL():|
=================================================================================
Description:
This function is called when the player enters the 'kill' command in the console.
=================================================================================
*/

void() ClientKill = 
{
	//pprint(pev, " has", " killed themselves.");
	T_Damage(pev, pev, pev, pev->health);
	ClientRespawn();
};

/*
==================
|CLIENTCONNECT():|
=================================================================================
Description:
This function is called when the player connects to the server.
=================================================================================
*/

void() ClientConnect = 
{
	pprint(pev, " has", " joined the game.");
}; 

/*
==================
|CLIENTDISCONNECT():|
=================================================================================
Description:
This function is called when the player disconnects from the server.
=================================================================================
*/

void() ClientDisconnect = 
{
	pprint(pev, " has", " left the game.");
};

/*
====================
|PLAYERPRETHINK():|
===========================================================
Description:
This function is called every frame *BEFORE* world physics.
===========================================================
*/


void() PlayerPreThink = 
{
	WaterMove ();
	SetClientFrame ();
	CheckImpulses(); 
};

/*
====================
|PLAYERPOSTTHINK():|
===========================================================
Description:
This function is called every frame *AFTER* world physics.
===========================================================
*/

void() PlayerPostThink = {};

/*
======================
|PUTCLIENTINSERVER():|
===========================================================
Description:
This function is called whenever a client enters the world.
It sets up the player entity.
===========================================================
*/

entity() find_spawnspot = 
{
	local entity spot;
	local string a;
	
	if(deathmatch == 1)
		a = "info_player_deathmatch";
	else if(coop == 1)
		a = "info_player_coop";

	else if(!deathmatch || !coop)
		a = "info_player_start";
	
	spot = find (world, classname, a);
	
	return spot;
};

void() PutClientInServer =
{
	local entity spawn_spot;             // This holds where we want to spawn
	spawn_spot = find_spawnspot(); //find (world, classname, "info_player_start"); // Find it :)

	pev->classname = "player";           // I'm a player!
	pev->health = pev->max_health = 100; // My health (and my max) is 100
	pev->takedamage = DAMAGE_AIM;        // I can be fired at
	pev->solid = SOLID_BBOX;         // Things sort of 'slide' past me
	pev->movetype = MOVETYPE_WALK;       // Yep, I want to walk.
	pev->flags = FL_CLIENT;              // Yes, I'm a client.

	pev->origin = spawn_spot.origin + '0 0 1'; // Move to the spawnspot location
	pev->angles = spawn_spot.angles;     // Face the angle the spawnspot indicates
	pev->fixangle = TRUE;                // Turn this way immediately
	pev->weaponmodel = "models/weapons/v_eagle.mdl"; // FIXME: rename to viewmodel

	MsgWarn("PutClientInServer()\n");

	setmodel (pev, "models/player.mdl"); // Set my player to the player model
	setsize (pev, VEC_HULL_MIN, VEC_HULL_MAX); // Set my size

	pev->view_ofs = '0 0 22';            // Center my view

	setsize(pev, '-16 -16 -32', '16 16 32' );
	
	if(pev->aflag) CCamChasePlayer ();

	pev->velocity = '0 0 0';             // Stop any old movement

	pev->mass = 90;
	pev->th_pain = PlayerPain;
	pev->th_die = PlayerDie;

	setstats( pev, STAT_HEALTH_ICON, "hud/i_health");
	setstats( pev, STAT_HEALTH, ftoa(pev->health));
	setstats( pev, STAT_HELPICON, "hud/i_help");

	image_index( "hud/help" );		
	GetLevelParms();
};

void ShowInventory( void )
{
	float	layout;

	if(pev->showinventory == TRUE) pev->showinventory = FALSE;
	else pev->showinventory = TRUE;

	if(pev->showinventory == TRUE) layout |= 2;
	else layout = 0;

	setstats( pev, STAT_LAYOUTS, ftoa(layout));

	MsgBegin( SVC_LAYOUT );
	WriteString( "" ); // build-in inventory
	MsgEnd(MSG_ONE, '0 0 0', pev );
}

void HelpComputer( void )
{
	float	layout;

	if(pev->showhelp == TRUE) pev->showhelp = FALSE;
	else pev->showhelp = TRUE;

	if(pev->showhelp == TRUE) layout |= 1;
	else layout = 0;

	setstats( pev, STAT_LAYOUTS, ftoa(layout));

	MsgBegin( SVC_LAYOUT );
	WriteString( "Hud_HelpComputer" );
	MsgEnd(MSG_ONE, '0 0 0', pev );
}

void ClientCommand( void )
{
	string	cmd;

	cmd = argv(0);

	if(cmd == "help")
	{
		HelpComputer();
	}
	if(cmd == "inven")
	{
		ShowInventory();
	}
	if(cmd == "say")
	{
		Msg("say me now ", argv(1), "\n");
	}
	if(cmd == "say_team")
	{
		Msg("say me now ", argv(1), "\n");
	}
}