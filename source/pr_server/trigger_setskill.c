/*
+================+
|TRIGGER_SETSKILL|
+================+====================+
|Description;		              |
|Set skill to value of pev->message.  |
+=====================================+
*/

void() trigger_setskill_touch =
{
	if (other.classname != "player")
		return;
	
	if(pev->touched == FALSE)
	{
		cvar_set ("skill", pev->message);
	}
};

void() trigger_setskill =
{
	trigger_setup();

	pev->classname = "setskill";

	pev->touch = trigger_setskill_touch;
};

