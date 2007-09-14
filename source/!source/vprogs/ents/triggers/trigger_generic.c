/*
+===================+
|TRIGGER_GENERIC|
+===================+
*/

void() trigger_generic_think = 
{
	self.touched = FALSE;
	
	if(self.message)
			centerprint(self.triggerer , self.message);
	
	if(self.target)
		IEM_usetarget();

	if(self.spawnflags & TRIGGER_ONCE)
		remove(self);
};

void() trigger_generic_touch = 
{
	if(!(other.flags & FL_CLIENT))
		return;

	if(self.touched == FALSE)
	{
		self.triggerer = other;
		
		self.touched = TRUE;
		self.think = trigger_generic_think;
		self.nextthink = time + self.delay;
	}
};

void() trigger_generic_use = 
{
	if(self.touched == TRUE)
		return;

	self.touched = TRUE;
	
	if(self.message)
		centerprint(self.triggerer, self.message);
	
	self.think = trigger_generic_think;
	self.nextthink = time + self.delay;
};

void() trigger_generic =
{
	trigger_setup();
	
	self.touch = trigger_generic_touch;
	self.use = trigger_generic_use;	

	self.classname = "generic";

	if(!self.delay)
		self.delay = 0.1;
};