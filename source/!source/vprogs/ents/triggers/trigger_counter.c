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
	pev->count = pev->count - 1;			//subtract my count by one; 'ive been triggered'
	
	if(pev->count < 0)
		return;
	
	if(pev->count != 0)
	{
			if (pev->count >= 4)
				centerprint (pev->triggerer, pev->targ1);
			else if (pev->count == 3)
				centerprint (pev->triggerer, pev->targ2);
			else if (pev->count == 2)
				centerprint (pev->triggerer, pev->targ3);
			else
				centerprint (pev->triggerer, pev->targ4);
	}
	
	IEM_usetarget();
	
	if(!(pev->spawnflags & TRIGGER_ONCE))		//if im a trigger_once, remove me;
		remove(pev);
	else
		pev->count = pev->wait1;				//restore old count
};

void() trigger_counter = 
{
	if(!pev->count)						//If my count not set in map default it to 1;
		pev->count = 1;
	
	pev->wait1 = pev->count; 				//store count levels;
	
	pev->use = trigger_counter_use;			//my use function;
		
	if(!pev->targ1)						//if a custom message is not set use the standard ones instead;
		pev->targ1 = "There are more to go...";
	if(!pev->targ2)
		pev->targ2 = "Only 3 more to go...";
	if(!pev->targ3)
		pev->targ3 = "Only 2 more to go...";
	if(!pev->targ4)
		pev->targ4 = "Only 1 more to go...";

	pev->classname = "t_counter";
};