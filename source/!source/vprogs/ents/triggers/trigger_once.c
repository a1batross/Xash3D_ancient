/*
+================+
|TRIGGER_ONCE|
+================+
*/

void() trigger_once_think = 
{
	if(pev->target)
		IEM_usetarget();
	
	remove(pev);
};

void() trigger_once_touch = 
{
	if(!(other.flags & FL_CLIENT))
		return;

	if(pev->touched == FALSE)
	{
		if(pev->message)
			centerprint(other, pev->message);

		pev->think = trigger_once_think;
		pev->nextthink = time + pev->delay;
	}
};

void() trigger_once_use = 
{
	if(pev->touched == TRUE)
		return;

	pev->touched = TRUE;

	if(pev->message)
	{
		bprint(pev->message);
		dprint("\n");
	}
	
	pev->think = trigger_once_think;
	pev->nextthink = time + pev->delay;
}

void() trigger_once = 
{
	trigger_setup();
	
	pev->touch = trigger_once_touch;
	pev->use = trigger_once_use;

	pev->classname = "once";	

	if(!pev->delay)
		pev->delay = 0.1;
	
	if(!pev->target)
		remove(pev);	
};