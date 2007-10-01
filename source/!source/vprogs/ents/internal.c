/*
+--------+
|Internal|
+--------+-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
| Scratch                                      Http://www.admdev.com/scratch |
+=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
| Internal Entity Management stuff for Quake.                                |
+=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
*/
.float touched; //used to tell if an entity has been touched or not.
.string name; //
.entity triggerer;

void(string modelname) Precache_Set = // Precache model, and set mypev to it
{
 precache_model(modelname);
 setmodel(pev, modelname);
};

/*
QuakeEd only writes a single float for angles (bad idea), so up and down are
just constant angles.
*/
void() SetMovedir =
{
	if (pev->angles == '0 -1 0')
		pev->movedir = '0 0 1';
	else if (pev->angles == '0 -2 0')
		pev->movedir = '0 0 -1';
	else
	{
		makevectors (pev->angles);
		pev->movedir = v_forward;
	}
	
	pev->angles = '0 0 0';
};

void() IEM_usetarget =
{
	local entity t, oldpev, oldother;

	if(pev->target)
	{
		t = find(world, targetname, pev->target);

		while(t)
		{
			if(pev->triggerer)
				t.triggerer = pev->triggerer;
			
			oldpev = pev;
			oldother = other;
			pev = t;
	
			if(t.use)
				t.use();
			
			pev = oldpev;
			other = oldother;	
			
			
			t = find(t, targetname, pev->target);
		}
	
	}
};

void(float effect_type, vector effect_org) IEM_effects =
{
	MsgBegin(SVC_TEMP_ENTITY);
		WriteByte(effect_type);
		WriteCoord(effect_org_x);
		WriteCoord(effect_org_y);
		WriteCoord(effect_org_z);
	MsgEnd( MSG_BROADCAST, effect_org, pev );
};