/*
+==========+
|ITEMS.QC|
+==========+=============================================================================+
|Description;
|Handles our basic item_setup stuff, aswell as containing base defintions for the 'item_' class;
|Also contains item respawning and pickup routines;
+========================================================================================+
*/

//DEFINITIONS FOR FILE;
.string oldmodel;
float ITEM_PICKUP_ONCE = 1;
//END DEFS

/*
ITEM_SETUP();

Description;
All our items will call this bit of code first in their spawning functions.
It sets up all the required and most basic item parimeters.
*/

void() item_setup = 
{
	if(!self.think)				//Items should start after other solids....
	{
		self.think = item_setup;
		self.nextthink = time + 0.2;
		return;
	}
	
	self.oldmodel = self.model;		// so it can be restored on respawn
	
	self.flags = FL_ITEM;				// make extra wide
	self.solid = SOLID_TRIGGER;			//Im a TRIGGER!
	self.movetype = MOVETYPE_TOSS;		//Toss me baby!... erm..
	self.velocity = '0 0 0';				//Stop me moving!
	self.origin_z = self.origin_z + 6;		//Raise me a bit off the floor
};

/*
IPRINT()

Description;
Prints the item pickup stuff to the player
*/

void(entity picker, string pickupmessage, string pickupoptional) iprint =
{
	sprint(picker, pickupmessage);
	sprint(picker, pickupoptional);
	sprint(picker, "\n");  
};

/*
ITEM_RESPAWN();

Description;
Simply resets the item fields that get set in item_pickup() code.
Makes the item pickupable again.
*/

void() item_respawn =
{
		setorigin(self, self.origin);
		self.solid = SOLID_TRIGGER;
		self.model = self.oldmodel;
};

/*
ITEM_PICKUP();

Description;
Should get called when an item is touched by a player. 

It makes the item FIRE/USE its target.
Stops the item getting picked up for a while, nextthink at bottom of code.
*/

void() item_pickup = 
{
		self.solid = SOLID_NOT;
		self.model = string_null;
	
		IEM_usetarget();
		
		if(self.spawnflags & ITEM_PICKUP_ONCE)
			return;
	
		self.think = item_respawn;
	
		if(deathmatch)
			self.nextthink = time + 20;			//This ideally would be a CVAR
};


