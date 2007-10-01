
/*
+======================+
|TRIGGER_SEQUENCE|
+======================+
*/

.string targ[8];
.string oldtarg[8];

.float twait[8];
.float olddelay;

void() trigger_sequence_think;

void(string seq_a, float seq_b) trigger_sequence_think1 = 
{
	pev->target = seq_a;
	pev->delay = seq_b;

	if(pev->target)
		IEM_usetarget();
	else
		pev->touched = FALSE;

	pev->think = trigger_sequence_think;
	pev->nextthink = time + pev->delay;
};

void() trigger_sequence_think = 
{
	local string a;
	local float b, i;

	if(pev->touched == FALSE)
	{
		for( i = 0; i < 8; i++)
		{
			pev->oldtarg[i] = pev->targ[i];
		}
		pev->delay = pev->olddelay;
		return;
	}

	for(i = 0; i < 8; i++)
	{
		if(pev->targ[i])
		{
			a = pev->targ[i];
			b = pev->twait[i];

			pev->targ[i] = pev->oldtarg[i];
			trigger_sequence_think1(a, b);
			return;
		}
	}
	
	pev->touched = FALSE;
	
	if(pev->spawnflags & TRIGGER_ONCE)
		remove(pev);
};

void() trigger_sequence_touch = 
{
	if(!(other.flags & FL_CLIENT))
		return;
		
	if(pev->spawnflags & TRIGGER_INTTERUPTABLE)
	{
		if(pev->touched == TRUE)
		{
			pev->touched = FALSE;	
			
			pev->delay = pev->olddelay;
	
			pev->triggerer = other;
			
			pev->think = trigger_sequence_think;
			pev->nextthink = time + pev->delay;
			
			return;
		}
	}

	if(pev->touched == FALSE)
	{
		pev->touched = TRUE;	

		if(pev->message)
			centerprint(other, pev->message);
		
		pev->triggerer = other;
		
		pev->think = trigger_sequence_think;
		pev->nextthink = time + pev->delay;	
	}
};

void() trigger_sequence_use = 
{
	if(pev->spawnflags & TRIGGER_INTTERUPTABLE)
	{
		if(pev->touched == TRUE)
		{
			pev->touched = FALSE;	
			
			pev->delay = pev->olddelay;
	
			pev->think = trigger_sequence_think;
			pev->nextthink = time + pev->delay;
			
			return;
		}
	}

	if(pev->touched == TRUE)
		return;
	
	pev->touched = TRUE;

	pev->think = trigger_sequence_think;
	pev->nextthink = time + pev->delay;
};

void() trigger_sequence = 
{
	trigger_setup();
	
	if(!pev->delay)
		pev->delay = 0.1;
	
	pev->delay = pev->olddelay;
	
	pev->classname = "t_sequence";

	pev->touch = trigger_sequence_touch;
	pev->use = trigger_sequence_use;
};