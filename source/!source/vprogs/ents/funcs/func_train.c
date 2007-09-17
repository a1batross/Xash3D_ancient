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
	if (pev->wait)
	{
		pev->nextthink = time + pev->wait;
		//sound (pev, CHAN_VOICE, pev->noise, 1, ATTN_NORM);
	}
	else
		pev->nextthink = time + 0.1;
	
	pev->think = func_train_next;
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

	targ = find (world, targetname, pev->target);

	pev->target = targ.target;

	if (!pev->target)
		objerror ("train_next: no next target");

	if (targ.wait)
		pev->wait = targ.wait;
	else
		pev->wait = 0;

	//sound (pev, CHAN_VOICE, pev->noise1, 1, ATTN_NORM);

	func_mover_move (targ.origin - pev->mins, pev->speed, func_train_wait);
};

/*
FUNC_TRAIN_FIND();

Desciption;
Called on spawning, this function finds the trains first target and sets the trains origin and hence starting position to it. Then calls FUNC_TRAIN_NEXT();
*/

void() func_train_find =
{
	local entity	targ;

	targ = find (world, targetname, pev->target);

	pev->target = targ.target;

	setorigin (pev, targ.origin - pev->mins);

	if (!pev->targetname)				// not triggered, so start immediately
	{
		pev->nextthink = time + 0.1;
		pev->think = func_train_next;
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
	if(pev->touched == FALSE)
	{
		pev->touched = TRUE;
		pev->nextthink = time + 0.1;
		pev->think = func_train_next;
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
	
	setsize (pev, pev->mins , pev->maxs);

	pev->classname = "train";
	
	//func_train defaults;
	if (!pev->speed)
		pev->speed = 100;
	if (!pev->target)
		objerror ("func_train without a target");
	if (!pev->dmg)
		pev->dmg = 2;

	pev->nextthink = time + 0.1;
	pev->think = func_train_find;
};

