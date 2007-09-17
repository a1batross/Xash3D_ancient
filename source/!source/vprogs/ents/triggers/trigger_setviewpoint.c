/*
+=================================+
|TRIGGER_CHANGE_VIEWPOINT|
+=================================+
*/
void() info_viewpoint_destination = 
{
	trigger_setup();
	
	setorigin(pev, pev->origin);
	
	setmodel (pev, "progs/eyes.mdl");
	pev->modelindex = 0;	
};

void() trigger_change_viewpoint_touch;

void() trigger_change_viewpoint_think1 = 
{
	pev->touched = FALSE;
	pev->touch = trigger_change_viewpoint_touch;
};

void() trigger_change_viewpoint_think = 
{
	local entity e, oldpev, oldtrig;
	
	if(pev->touched == TRUE)
	{
		e = find(world, targetname, pev->target);
		//e = pev->triggerer.triggerer.triggerer;
		pev->triggerer.triggerer = pev->triggerer;
		
		MsgBegin(SVC_SETVIEW); 	// Network Protocol: Set Viewpoint Entity
		WriteEntity( e);       		// Write entity to clients.
		MsgEnd(MSG_ONE, '0 0 0', pev );
		
		//pev->angles = e.mangles;
		//pev->fixangle = 1;	
		
		//pev->triggerer.movetype = MOVETYPE_NONE;
		
		pev->think = trigger_change_viewpoint_think;
		pev->nextthink = time + pev->wait1;
			
		pev->touched = FALSE;
		
		bprint("working\n");
	}
	else if(pev->touched == FALSE)
	{
		//pev->triggerer = pev->triggerer.triggerer;
		
		MsgBegin(SVC_SETVIEW); 	// Network Protocol: Set Viewpoint Entity
		WriteEntity(pev->triggerer.triggerer);    
		MsgEnd(MSG_ONE, '0 0 0', pev );
		
		pev->triggerer.movetype = MOVETYPE_WALK;
		
		pev->triggerer = world;
			
		pev->touched = TRUE;
		
		pev->think =	trigger_change_viewpoint_think1;
		pev->nextthink = time + 2;
	}
};

void() trigger_change_viewpoint_touch = 
{
	if(!(other.flags & FL_CLIENT))
		return;
	
	if(pev->touched == FALSE)
	{
		pev->touched = TRUE;
		pev->touch = SUB_Null;
		pev->triggerer = other;
		
		pev->think =	trigger_change_viewpoint_think;
		pev->nextthink = time + pev->delay;
		
		bprint("touched");
	}
};

void() trigger_change_viewpoint_use = 
{
	if(pev->touched == TRUE)
		return;
	
	pev->think =	trigger_change_viewpoint_think;
	pev->nextthink = time + pev->delay;
};

void() trigger_change_viewpoint = 
{
	trigger_setup();
		
	if(!pev->delay)
		pev->delay = 1;
	if(!pev->wait)
		pev->wait = 1;
	
	pev->touch = trigger_change_viewpoint_touch;
	pev->use = trigger_change_viewpoint_use ;
	
	if(!pev->target)
	{
		objerror("trigger_change_viewpoint: NO TARGET");
		pev->solid = SOLID_NOT;
	}
};
