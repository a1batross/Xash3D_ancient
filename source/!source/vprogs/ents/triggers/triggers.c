/*
================
trigger_setup
================
*/
//DEFS

float TRIGGER_ONCE = 1;
float TRIGGER_NO_MODEL = 4;
float TRIGGER_INTTERUPTABLE = 8;
float TRIGGER_START_OFF = 16;

//END DEFS


void() trigger_setup =
{
	// trigger angles are used for one-way touches.  An angle of 0 is assumed
	// to mean no restrictions, so use a yaw of 360 instead.
	//if (self.angles != '0 0 0')
	//	SetMovedir ();
	//not at the moment they are not.... 

	self.solid = SOLID_TRIGGER;
	self.movetype = MOVETYPE_NONE;

	setmodel (self, self.model);
	setorigin(self, self.origin);	

	if(self.spawnflags & TRIGGER_NO_MODEL)
	{
		self.modelindex = 0;
		self.model = "";
	}
};

