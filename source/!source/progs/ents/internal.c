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

void(string modelname) Precache_Set = // Precache model, and set myself to it
{
 precache_model(modelname);
 setmodel(self, modelname);
};

void(entity ent) SetViewPoint =  	// Alter the Viewpoint Entity
{
	msg_entity = self;        	 	// This message is to myself. 
 
 	MsgBegin(SVC_SETVIEW); 		// Network Protocol: Set Viewpoint Entity
 	WriteEntity(ent);       		// Write entity to clients.
	MsgEnd( MSG_ONE, '0 0 0', ent );
};

/*
QuakeEd only writes a single float for angles (bad idea), so up and down are
just constant angles.
*/
void() SetMovedir =
{
	if (self.angles == '0 -1 0')
		self.movedir = '0 0 1';
	else if (self.angles == '0 -2 0')
		self.movedir = '0 0 -1';
	else
	{
		makevectors (self.angles);
		self.movedir = v_forward;
	}
	
	self.angles = '0 0 0';
};

void() IEM_usetarget =
{
	local entity t, oldself, oldother;
	
	if(self.target)
	{
		t = find(world, targetname, self.target);

		while(t)
		{
			if(self.triggerer)
				t.triggerer = self.triggerer;
			
			oldself = self;
			oldother = other;
			self = t;
	
			if(t.use)
				t.use();
			
			self = oldself;
			other = oldother;	
			
			
			t = find(t, targetname, self.target);
		}
	
	}
};

void(float effect_type, vector effect_org) IEM_effects =
{
	MsgBegin(SVC_TEMPENTITY);
		WriteByte(effect_type);
		WriteCoord(effect_org);
	MsgEnd( MSG_BROADCAST, effect_org, self );
};