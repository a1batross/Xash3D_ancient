/*
+===================+
|TRIGGER_GENERIC|
+===================+
*/

void() trigger_generic_think = 
{
	pev->touched = FALSE;
	
	if(pev->message)
			centerprint(pev->triggerer , pev->message);
	
	if(pev->target)
		IEM_usetarget();

	if(pev->spawnflags & TRIGGER_ONCE)
		remove(pev);
};

void() trigger_generic_touch = 
{
	if(!(other.flags & FL_CLIENT))
		return;

	if(pev->touched == FALSE)
	{
		pev->triggerer = other;
		
		pev->touched = TRUE;
		pev->think = trigger_generic_think;
		pev->nextthink = time + pev->delay;
	}
};

void() trigger_generic_use = 
{
	if(pev->touched == TRUE)
		return;

	pev->touched = TRUE;
	
	if(pev->message)
		centerprint(pev->triggerer, pev->message);
	
	pev->think = trigger_generic_think;
	pev->nextthink = time + pev->delay;
};

void() trigger_generic =
{
	trigger_setup();
	
	pev->touch = trigger_generic_touch;
	pev->use = trigger_generic_use;	

	pev->classname = "generic";

	if(!pev->delay)
		pev->delay = 0.1;
};