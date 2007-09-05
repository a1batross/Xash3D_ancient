
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
	self.target = seq_a;
	self.delay = seq_b;

	if(self.target)
		IEM_usetarget();
	else
		self.touched = FALSE;

	self.think = trigger_sequence_think;
	self.nextthink = time + self.delay;
};

void() trigger_sequence_think = 
{
	local string a;
	local float b;

	if(self.touched == FALSE)
	{
		self.oldtarg1 = self.targ1;
		self.oldtarg2 = self.targ2;
		self.oldtarg3 = self.targ3;
		self.oldtarg4 = self.targ4;
		self.oldtarg5 = self.targ5;
		self.oldtarg6 = self.targ6;
		self.oldtarg7 = self.targ7;
		self.oldtarg8 = self.targ8;
		self.delay = self.olddelay;
		return;
	}

	if(self.targ1)
	{
		a = self.targ1;
		b = self.wait1;

		self.targ1 = self.oldtarg1;

		trigger_sequence_think1(a,b);
			
		return;
	}
	else if(self.targ2)
	{
		a = self.targ2;
		b = self.wait2;

		self.targ2 = self.oldtarg2;
		trigger_sequence_think1(a,b);
		return;
	}
	else if(self.targ3)
	{
		a = self.targ3;
		b = self.wait3;

		self.targ3 = self.oldtarg3;
		trigger_sequence_think1(a,b);
		return;
	}
	else if(self.targ4)
	{
		a = self.targ4;
		b = self.wait4;

		self.targ4 = self.oldtarg4;
		trigger_sequence_think1(a,b);
		return;
	}
	else if(self.targ5)
	{
		a = self.targ5;
		b = self.wait5;

		self.targ5 = self.oldtarg5;
		trigger_sequence_think1(a,b);
		return;
	}
	else if(self.targ6)
	{
		a = self.targ6;
		b = self.wait6;

		self.targ6 = self.oldtarg6;
		trigger_sequence_think1(a,b);
		return;
	}
	else if(self.targ7)
	{
		a = self.targ7;
		b = self.wait7;

		self.targ7 = self.oldtarg7;
		trigger_sequence_think1(a,b);
		return;
	}
	else if(self.targ8)
	{
		a = self.targ8;
		b = self.wait8;

		self.targ8 = self.oldtarg8;
		trigger_sequence_think1(a,b);
		return;
	}
	
	self.touched = FALSE;
	
	if(self.spawnflags & TRIGGER_ONCE)
		remove(self);
};

void() trigger_sequence_touch = 
{
	if(!(other.flags & FL_CLIENT))
		return;
		
	if(self.spawnflags & TRIGGER_INTTERUPTABLE)
	{
		if(self.touched == TRUE)
		{
			self.touched = FALSE;	
			
			self.delay = self.olddelay;
	
			self.triggerer = other;
			
			self.think = trigger_sequence_think;
			self.nextthink = time + self.delay;
			
			return;
		}
	}

	if(self.touched == FALSE)
	{
		self.touched = TRUE;	

		if(self.message)
			centerprint(other, self.message);
		
		self.triggerer = other;
		
		self.think = trigger_sequence_think;
		self.nextthink = time + self.delay;	
	}
};

void() trigger_sequence_use = 
{
	if(self.spawnflags & TRIGGER_INTTERUPTABLE)
	{
		if(self.touched == TRUE)
		{
			self.touched = FALSE;	
			
			self.delay = self.olddelay;
	
			self.think = trigger_sequence_think;
			self.nextthink = time + self.delay;
			
			return;
		}
	}

	if(self.touched == TRUE)
		return;
	
	self.touched = TRUE;

	self.think = trigger_sequence_think;
	self.nextthink = time + self.delay;
};

void() trigger_sequence = 
{
	trigger_setup();
	
	if(!self.delay)
		self.delay = 0.1;
	
	self.delay = self.olddelay;
	
	self.classname = "t_sequence";

	self.touch = trigger_sequence_touch;
	self.use = trigger_sequence_use;
};