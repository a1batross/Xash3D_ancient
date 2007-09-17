/*
+==========+
|FUNCS.QC|
+==========+=============================================================================+
|Description;
|Handles our basic func_setup stuff, aswell as containing base defintions for the 'func_' class;
+========================================================================================+
*/

//DEFINITIONS FOR FILE;
.float state;
.float used;

float STATE_OPEN = 1;		
float STATE_CLOSED = 0;
//END DEFS;

/*
FUNC_SETUP();

Description;
This function is used by ALL our map func_'s. It sets up the func_'s basic and most common properties;
With the only exception being 'pev->movetype = MOVETYPE_PUSH;' which usually gets set to something else
in the func_'s spawning function after this has been called. It's included because out of the func_'s we have so far
more require this field to be set to that than any other.
*/
void() func_setup = 
{
	SetMovedir ();
	
	pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;
	
	setorigin (pev, pev->origin);	
	setmodel (pev, pev->model);
	setsize (pev, pev->mins , pev->maxs);
};

/*
FUNC_ILLUSIONARY();

Description;
This is the spawning func for the map placeable 'func_illusionary'.
It is to allow our mapper to place what looks like a wall in the map, but which the player can walk through.
*/

void() func_illusionary = 
{
	func_setup();
	
	pev->solid = SOLID_TRIGGER;
};

/*
FUNC_PLACE_MODEL();

Description;
This is the spawning func for the map placeable 'func_place_model'.
Quite simply it allows our mapper to place custom models within the map.

Credits;
Jon Eriksson - For the idea and base code!
*/

void() func_place_model = 
{
	precache_model (pev->model);
	
	func_setup();
	
	pev->movetype = MOVETYPE_NONE;
};

