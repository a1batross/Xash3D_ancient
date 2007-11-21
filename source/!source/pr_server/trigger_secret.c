/*
+==============+
|TRIGGER_SECRET|
+==============+
*/

void() trigger_secret_think = 
{
	total_secrets++;
	
	if(pev->target)
		IEM_usetarget();
	
};

void() trigger_secret_touch = 
{
	if(!(other.flags & FL_CLIENT))
		return;
	
	if(pev->touched == FALSE)
	{	
		if (!pev->message)
			pev->message = "You found a secret area!";
		else
			centerprint(other, pev->message);	

		if(pev->delay)
		{
			pev->think = trigger_secret_think;
			pev->nextthink = time + pev->delay;
		}
	
		pev->touched = TRUE;
	}
};

void() trigger_secret =
{
	trigger_setup();

	pev->touch = trigger_secret_touch;
	
	if(!pev->delay)
		pev->delay = 0.1;
};