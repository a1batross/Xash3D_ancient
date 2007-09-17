/*
====================
|TRIGGER_CHANGLEVEL|
========================
|Changes level on touch|
========================
*/
void() trigger_changelevel_touch = 
{
	changelevel(pev->map);
};

void() trigger_changelevel = 
{
	if (!pev->map)
		objerror ("chagnelevel trigger doesn't have map");

	trigger_setup();
	
	pev->touch = trigger_changelevel_touch;	
};


