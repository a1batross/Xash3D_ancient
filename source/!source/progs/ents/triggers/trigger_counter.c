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
	self.count = self.count - 1;			//subtract my count by one; 'ive been triggered'
	
	if(self.count < 0)
		return;
	
	if(self.count != 0)
	{
			if (self.count >= 4)
				centerprint (self.triggerer, self.targ1);
			else if (self.count == 3)
				centerprint (self.triggerer, self.targ2);
			else if (self.count == 2)
				centerprint (self.triggerer, self.targ3);
			else
				centerprint (self.triggerer, self.targ4);
	}
	
	IEM_usetarget();
	
	if(!(self.spawnflags & TRIGGER_ONCE))		//if im a trigger_once, remove me;
		remove(self);
	else
		self.count = self.wait1;				//restore old count
};

void() trigger_counter = 
{
	if(!self.count)						//If my count not set in map default it to 1;
		self.count = 1;
	
	self.wait1 = self.count; 				//store count levels;
	
	self.use = trigger_counter_use;			//my use function;
		
	if(!self.targ1)						//if a custom message is not set use the standard ones instead;
		self.targ1 = "There are more to go...";
	if(!self.targ2)
		self.targ2 = "Only 3 more to go...";
	if(!self.targ3)
		self.targ3 = "Only 2 more to go...";
	if(!self.targ4)
		self.targ4 = "Only 1 more to go...";

	self.classname = "t_counter";
};