/*
+==================+
|FUNC_TRAIN.QC |
+==================+=====================================================================+
|Description;
|This file handles the func_train map entity;
+========================================================================================+
*/

//DEFINITIONS FOR FILE;
void() func_train_next; //[func_train.qc]
//END DEFS;

/*
FUNC_TRAIN_WAIT();

Desciption;
Called after the train has reached its target.
If the train has the 'wait' field set on its spawn it waits the alloted time then finds its next target.
*/

void() func_train_wait =
{
	if (self.wait)
	{
		self.nextthink = self.ltime + self.wait;
		//sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
	}
	else
		self.nextthink = self.ltime + 0.1;
	
	self.think = func_train_next;
};

/*
FUNC_TRAIN_NEXT();

Desciption;
Finds the trains next target;
Then sets it moving in its direction;
*/

void() func_train_next =
{
	local entity	targ;

	targ = find (world, targetname, self.target);

	self.target = targ.target;

	if (!self.target)
		objerror ("train_next: no next target");

	if (targ.wait)
		self.wait = targ.wait;
	else
		self.wait = 0;

	//sound (self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);

	func_mover_move (targ.origin - self.mins, self.speed, func_train_wait);
};

/*
FUNC_TRAIN_FIND();

Desciption;
Called on spawning, this function finds the trains first target and sets the trains origin and hence starting position to it. Then calls FUNC_TRAIN_NEXT();
*/

void() func_train_find =
{
	local entity	targ;

	targ = find (world, targetname, self.target);

	self.target = targ.target;

	setorigin (self, targ.origin - self.mins);

	if (!self.targetname)				// not triggered, so start immediately
	{
		self.nextthink = self.ltime + 0.1;
		self.think = func_train_next;
	}
};

/*
FUNC_TRAIN_USE();

Desciption;
The use function for trains;
If targeted the train starts moving then cannot be used/targetted again;
*/

void() func_train_use = 
{
	if(self.touched == FALSE)
	{
		self.touched = TRUE;
		self.nextthink = self.ltime + 0.1;
		self.think = func_train_next;
	}
};

/*
FUNC_TRAIN();

Desciption;
The spawning function for the map entity func_train;
*/

void() func_train = 
{
	func_setup();					
	
	setsize (self, self.mins , self.maxs);

	self.classname = "train";
	
	//func_train defaults;
	if (!self.speed)
		self.speed = 100;
	if (!self.target)
		objerror ("func_train without a target");
	if (!self.dmg)
		self.dmg = 2;

	self.nextthink = self.ltime + 0.1;
	self.think = func_train_find;
};

