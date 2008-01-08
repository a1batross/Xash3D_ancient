/*
+============+
|TRIGGER_PUSH|
+============+==============+
|Description;		    |
|Push player for pev speed.|
+===========================+
*/
void() trigger_push_think = 
{
	pev->touched = FALSE;
};

void() trigger_push_touch = 
{
	if(other.flags & FL_CLIENT)
		other.velocity = pev->speed * pev->movedir * 10;

	if (pev->spawnflags & TRIGGER_ONCE)
		remove(pev);
	
	if(pev->dmg)
	{
		if(pev->touched == FALSE)
		{
			T_Damage (other, pev, pev, pev->dmg);
			pev->touched = TRUE;
			
			pev->think = trigger_push_think;
			pev->nextthink = time + 0.1;
		}
	}
};

void() trigger_push = 
{
	trigger_setup();

	pev->touch = trigger_push_touch;

	if (!pev->speed)
		pev->speed = 1000;

	pev->classname = "t_push";
};


