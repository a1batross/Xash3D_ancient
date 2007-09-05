/*
+================+
|TRIGGER_ONCE|
+================+
*/

void() trigger_once_think = 
{
	if(self.target)
		IEM_usetarget();
	
	remove(self);
};

void() trigger_once_touch = 
{
	if(!(other.flags & FL_CLIENT))
		return;

	if(self.touched == FALSE)
	{
		if(self.message)
			centerprint(other, self.message);

		self.think = trigger_once_think;
		self.nextthink = time + self.delay;
	}
};

void() trigger_once_use = 
{
	if(self.touched == TRUE)
		return;

	self.touched = TRUE;

	if(self.message)
	{
		bprint(self.message);
		dprint("\n");
	}
	
	self.think = trigger_once_think;
	self.nextthink = time + self.delay;
}

void() trigger_once = 
{
	trigger_setup();
	
	self.touch = trigger_once_touch;
	self.use = trigger_once_use;

	self.classname = "once";	

	if(!self.delay)
		self.delay = 0.1;
	
	if(!self.target)
		remove(self);	
};