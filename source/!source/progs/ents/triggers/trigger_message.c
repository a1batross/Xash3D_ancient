/*
+===============+
|TRIGGER_MESSAGE|
+===============+
*/

.string message1;
.string message2;
.entity triggerer;

void() trigger_message_think = 
{
	self.touched = FALSE;	
	
	if(self.message)
		centerprint(self.triggerer, self.message);
	if(self.message1)
	{
		sprint(self.triggerer, self.message1);	
		sprint(self.triggerer, "\n");
	}
	if(self.message2)
	{
		dprint(self.message2);
		dprint("\n");
	}
	
	if(self.target)
		IEM_usetarget();

	if(self.spawnflags & TRIGGER_ONCE)
		remove(self);
}; 

void() trigger_message_touch = 
{
	if(!(other.flags & FL_CLIENT))
		return;

	if(self.touched == FALSE)
	{
		self.touched = TRUE;
	
		self.triggerer = other;

		self.think = trigger_message_think;
		self.nextthink = time + self.delay;
	}
};

void() trigger_message_use = 
{
	if(self.touched == TRUE)
		return;

	self.touched = TRUE;

	if(self.message)
		centerprint(self.triggerer, self.message);
	if(self.message1)
	{
		sprint(self.triggerer, self.message1);	
		sprint(self.triggerer, "\n");
	}
	if(self.message2)
	{
		dprint(self.message2);
		dprint("\n");
	}
	
	self.think = trigger_message_think;
	self.nextthink = time + self.delay;
};

void() trigger_message = 
{
	trigger_setup();
	
	if(!self.delay)
		self.delay = 1;
	
	self.touch = trigger_message_touch;
	self.use = trigger_message_use;

	self.classname = "t_message";
};