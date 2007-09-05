/*
====================
|TRIGGER_CHANGLEVEL|
========================
|Changes level on touch|
========================
*/
void() trigger_changelevel_touch = 
{
	changelevel(self.map);
};

void() trigger_changelevel = 
{
	if (!self.map)
		objerror ("chagnelevel trigger doesn't have map");

	trigger_setup();
	
	self.touch = trigger_changelevel_touch;	
};


