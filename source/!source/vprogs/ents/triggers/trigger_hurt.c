/*
+============+
|TRIGGER_HURT|
+============+==============+
|Description;		    |
|Hurt player for pev->dmg.  |
+===========================+
*/

void() trigger_hurt_think = 
{
	pev->solid = SOLID_TRIGGER;
	pev->touched = FALSE;
};

void() trigger_hurt_touch = 
{
	if(other.takedamage)
	{
		if(pev->touched == FALSE)
		{
			T_Damage (other, pev, pev, pev->dmg);
	
			pev->touched = TRUE;
			pev->solid = SOLID_NOT;
			pev->think = trigger_hurt_think;
			pev->nextthink = time + 1;
		}
	}
};

void() trigger_hurt = 
{
	trigger_setup();
	
	pev->touch = trigger_hurt_touch;
		
	if(!pev->dmg)
		pev->dmg = 5;	

	pev->classname = "t_hurt";
};