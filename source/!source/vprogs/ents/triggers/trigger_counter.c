/*
TRIGGER_COUNTER();

Description;
Counts the number of times it gets fired.. then fires its targets..
*/

//DEFS;
.float count;
//END DEFS;

void() trigger_counter_use = 
{
	pev->count = pev->count - 1;				// subtract my count by one; 'ive been triggered'
	
	if(pev->count != 0)
	{
		if (pev->count >= 4) centerprint (pev->triggerer, pev->targ[0]);
		else if (pev->count == 3) centerprint (pev->triggerer, pev->targ[1]);
		else if (pev->count == 2) centerprint (pev->triggerer, pev->targ[2]);
		else centerprint (pev->triggerer, pev->targ[3]);
	}

	if(pev->count <= 0)
	{	
		IEM_usetarget();
	
		if(pev->spawnflags & TRIGGER_ONCE)		// if im a trigger_once, remove me;
			remove(pev);
		else pev->count = pev->twait[0];		// restore old count
	}
};

void() trigger_counter = 
{
	if(!pev->count)						//If my count not set in map default it to 1;
		pev->count = 1;
	
	pev->twait[0] = pev->count; 				//store count levels;
	
	pev->use = trigger_counter_use;			//my use function;
		
	if(!pev->targ[0])						//if a custom message is not set use the standard ones instead;
		pev->targ[0] = "There are more to go...";
	if(!pev->targ[1])
		pev->targ[1] = "Only 3 more to go...";
	if(!pev->targ[2])
		pev->targ[2] = "Only 2 more to go...";
	if(!pev->targ[3])
		pev->targ[3] = "Only 1 more to go...";

	pev->classname = "t_counter";
};