/*
====================
|TRIGGER_CHANGLEVEL|
========================
|Changes level on touch|
========================
*/
void() trigger_changelevel_touch = 
{
	changelevel(pev->map, pev->landmark);
};

void() trigger_changelevel = 
{
	entity	lmark;

	if(!pev->map) Error("chagnelevel trigger doesn't have map\n");

	trigger_setup();

	lmark = find(world, classname, "info_landmark");
	if( lmark ) pev->landmark = lmark->targetname; 	

	pev->touch = trigger_changelevel_touch;	
};


