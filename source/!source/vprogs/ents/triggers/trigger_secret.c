/*
+==============+
|TRIGGER_SECRET|
+==============+
*/

void() trigger_secret_think = 
{
	total_secrets++;
	
	if(self.target)
		IEM_usetarget();
	
};

void() trigger_secret_touch = 
{
	if(!(other.flags & FL_CLIENT))
		return;
	
	if(self.touched == FALSE)
	{	
		if (!self.message)
			self.message = "You found a secret area!";
		else
			centerprint(other, self.message);	

		if(self.delay)
		{
			self.think = trigger_secret_think;
			self.nextthink = time + self.delay;
		}
	
		self.touched = TRUE;
	}
};

void() trigger_secret =
{
	trigger_setup();

	self.touch = trigger_secret_touch;
	
	if(!self.delay)
		self.delay = 0.1;
};