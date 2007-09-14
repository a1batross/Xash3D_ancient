/*
+================+
|TRIGGER_SETSKILL|
+================+====================+
|Description;		              |
|Set skill to value of self.message.  |
+=====================================+
*/

void() trigger_setskill_touch =
{
	if (other.classname != "player")
		return;
	
	if(self.touched == FALSE)
	{
		cvar_set ("skill", self.message);
	}
};

void() trigger_setskill =
{
	trigger_setup();

	self.classname = "setskill";

	self.touch = trigger_setskill_touch;
};

