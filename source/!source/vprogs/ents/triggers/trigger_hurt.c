/*
+============+
|TRIGGER_HURT|
+============+==============+
|Description;		    |
|Hurt player for self.dmg.  |
+===========================+
*/

void() trigger_hurt_think = 
{
	self.solid = SOLID_TRIGGER;
	self.touched = FALSE;
};

void() trigger_hurt_touch = 
{
	if(other.takedamage)
	{
		if(self.touched == FALSE)
		{
			T_Damage (other, self, self, self.dmg);
	
			self.touched = TRUE;
			self.solid = SOLID_NOT;
			self.think = trigger_hurt_think;
			self.nextthink = time + 1;
		}
	}
};

void() trigger_hurt = 
{
	trigger_setup();
	
	self.touch = trigger_hurt_touch;
		
	if(!self.dmg)
		self.dmg = 5;	

	self.classname = "t_hurt";
};