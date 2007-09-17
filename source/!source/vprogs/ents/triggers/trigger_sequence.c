
/*
+======================+
|TRIGGER_SEQUENCE|
+======================+
*/

.string targ1;
.string targ2;
.string targ3;
.string targ4;
.string targ5;
.string targ6;
.string targ7;
.string targ8;
.string oldtarg1;
.string oldtarg2;
.string oldtarg3;
.string oldtarg4;
.string oldtarg5;
.string oldtarg6;
.string oldtarg7;
.string oldtarg8;

.float wait1;
.float wait2;
.float wait3;
.float wait4;
.float wait5;
.float wait6;
.float wait7;
.float wait8;
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
	local float b;

	if(pev->touched == FALSE)
	{
		pev->oldtarg1 = pev->targ1;
		pev->oldtarg2 = pev->targ2;
		pev->oldtarg3 = pev->targ3;
		pev->oldtarg4 = pev->targ4;
		pev->oldtarg5 = pev->targ5;
		pev->oldtarg6 = pev->targ6;
		pev->oldtarg7 = pev->targ7;
		pev->oldtarg8 = pev->targ8;
		pev->delay = pev->olddelay;
		return;
	}

	if(pev->targ1)
	{
		a = pev->targ1;
		b = pev->wait1;

		pev->targ1 = pev->oldtarg1;

		trigger_sequence_think1(a,b);
			
		return;
	}
	else if(pev->targ2)
	{
		a = pev->targ2;
		b = pev->wait2;

		pev->targ2 = pev->oldtarg2;
		trigger_sequence_think1(a,b);
		return;
	}
	else if(pev->targ3)
	{
		a = pev->targ3;
		b = pev->wait3;

		pev->targ3 = pev->oldtarg3;
		trigger_sequence_think1(a,b);
		return;
	}
	else if(pev->targ4)
	{
		a = pev->targ4;
		b = pev->wait4;

		pev->targ4 = pev->oldtarg4;
		trigger_sequence_think1(a,b);
		return;
	}
	else if(pev->targ5)
	{
		a = pev->targ5;
		b = pev->wait5;

		pev->targ5 = pev->oldtarg5;
		trigger_sequence_think1(a,b);
		return;
	}
	else if(pev->targ6)
	{
		a = pev->targ6;
		b = pev->wait6;

		pev->targ6 = pev->oldtarg6;
		trigger_sequence_think1(a,b);
		return;
	}
	else if(pev->targ7)
	{
		a = pev->targ7;
		b = pev->wait7;

		pev->targ7 = pev->oldtarg7;
		trigger_sequence_think1(a,b);
		return;
	}
	else if(pev->targ8)
	{
		a = pev->targ8;
		b = pev->wait8;

		pev->targ8 = pev->oldtarg8;
		trigger_sequence_think1(a,b);
		return;
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