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
	pev->touched = FALSE;	
	
	if(pev->message)
		centerprint(pev->triggerer, pev->message);
	if(pev->message1)
	{
		sprint(pev->triggerer, pev->message1);	
		sprint(pev->triggerer, "\n");
	}
	if(pev->message2)
	{
		MsgWarn("%s\n", pev->message2);
	}
	
	if(pev->target)
		IEM_usetarget();

	if(pev->spawnflags & TRIGGER_ONCE)
		remove(pev);
}; 

void() trigger_message_touch = 
{
	if(!(other.flags & FL_CLIENT))
		return;

	if(pev->touched == FALSE)
	{
		pev->touched = TRUE;
	
		pev->triggerer = other;

		pev->think = trigger_message_think;
		pev->nextthink = time + pev->delay;
	}
};

void() trigger_message_use = 
{
	if(pev->touched == TRUE)
		return;

	pev->touched = TRUE;

	if(pev->message)
		centerprint(pev->triggerer, pev->message);
	if(pev->message1)
	{
		sprint(pev->triggerer, pev->message1);	
		sprint(pev->triggerer, "\n");
	}
	if(pev->message2)
	{
		MsgWarn("%s\n", pev->message2);
	}
	
	pev->think = trigger_message_think;
	pev->nextthink = time + pev->delay;
};

void() trigger_message = 
{
	trigger_setup();
	
	if(!pev->delay)
		pev->delay = 1;
	
	pev->touch = trigger_message_touch;
	pev->use = trigger_message_use;

	pev->classname = "t_message";
};