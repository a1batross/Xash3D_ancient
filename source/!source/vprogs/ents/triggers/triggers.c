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
	//if (pev->angles != '0 0 0')
	//	SetMovedir ();
	//not at the moment they are not.... 

	pev->solid = SOLID_TRIGGER;
	pev->movetype = MOVETYPE_NONE;

	setmodel (pev, pev->model);
	setorigin(pev, pev->origin);	

	pev->modelindex = 0;
	pev->model = "";
};

