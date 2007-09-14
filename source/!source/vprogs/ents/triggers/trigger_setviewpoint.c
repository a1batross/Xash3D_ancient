/*
+=================================+
|TRIGGER_CHANGE_VIEWPOINT|
+=================================+
*/
void() info_viewpoint_destination = 
{
	trigger_setup();
	
	setorigin(self, self.origin);
	
	setmodel (self, "progs/eyes.mdl");
	self.modelindex = 0;	
};

void() trigger_change_viewpoint_touch;

void() trigger_change_viewpoint_think1 = 
{
	self.touched = FALSE;
	self.touch = trigger_change_viewpoint_touch;
};

void() trigger_change_viewpoint_think = 
{
	local entity e, oldself, oldtrig;
	
	if(self.touched == TRUE)
	{
		e = find(world, targetname, self.target);
		//e = self.triggerer.triggerer.triggerer;
		self.triggerer.triggerer = self.triggerer;
		
		msg_entity = self.triggerer;        	 	
		MsgBegin(SVC_SETVIEW); 	// Network Protocol: Set Viewpoint Entity
		WriteEntity( e);       		// Write entity to clients.
		MsgEnd(MSG_ONE, '0 0 0', self );
		
		//self.angles = e.mangles;
		//self.fixangle = 1;	
		
		//self.triggerer.movetype = MOVETYPE_NONE;
		
		self.think = trigger_change_viewpoint_think;
		self.nextthink = time + self.wait1;
			
		self.touched = FALSE;
		
		bprint("working\n");
	}
	else if(self.touched == FALSE)
	{
		//self.triggerer = self.triggerer.triggerer;
		
		msg_entity = self.triggerer;        	 	
		MsgBegin(SVC_SETVIEW); 	// Network Protocol: Set Viewpoint Entity
		WriteEntity(self.triggerer.triggerer);    
		MsgEnd(MSG_ONE, '0 0 0', self );
		
		self.triggerer.movetype = MOVETYPE_WALK;
		
		self.triggerer = world;
			
		self.touched = TRUE;
		
		self.think =	trigger_change_viewpoint_think1;
		self.nextthink = time + 2;
	}
};

void() trigger_change_viewpoint_touch = 
{
	if(!(other.flags & FL_CLIENT))
		return;
	
	if(self.touched == FALSE)
	{
		self.touched = TRUE;
		self.touch = SUB_Null;
		self.triggerer = other;
		
		self.think =	trigger_change_viewpoint_think;
		self.nextthink = time + self.delay;
		
		bprint("touched");
	}
};

void() trigger_change_viewpoint_use = 
{
	if(self.touched == TRUE)
		return;
	
	self.think =	trigger_change_viewpoint_think;
	self.nextthink = time + self.delay;
};

void() trigger_change_viewpoint = 
{
	trigger_setup();
		
	if(!self.delay)
		self.delay = 1;
	if(!self.wait)
		self.wait = 1;
	
	self.touch = trigger_change_viewpoint_touch;
	self.use = trigger_change_viewpoint_use ;
	
	if(!self.target)
	{
		objerror("trigger_change_viewpoint: NO TARGET");
		self.solid = SOLID_NOT;
	}
};
