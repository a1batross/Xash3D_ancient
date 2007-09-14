/*
+============+
|TRIGGER_PUSH|
+============+==============+
|Description;		    |
|Push player for self speed.|
+===========================+
*/
void() trigger_push_think = 
{
	self.touched = FALSE;
};

void() trigger_push_touch = 
{
	if(other.flags & FL_CLIENT)
		other.velocity = self.speed * self.movedir * 10;

	if (self.spawnflags & TRIGGER_ONCE)
		remove(self);
	
	if(self.dmg)
	{
		if(self.touched == FALSE)
		{
			T_Damage (other, self, self, self.dmg);
			self.touched = TRUE;
			
			self.think = trigger_push_think;
			self.nextthink = time + 0.1;
		}
	}
};

void() trigger_push = 
{
	trigger_setup();

	self.touch = trigger_push_touch;

	if (!self.speed)
		self.speed = 1000;

	self.classname = "t_push";
};


