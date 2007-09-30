/*
+--------+
|Impulses|
+--------+-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
| Scratch                                      Http://www.admdev.com/scratch |
+=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
| Handle and execute "Impulse" commands - as entered from console.           |
+=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
*/      

void() CheckImpulses =
{
	if (pev->impulse == 15)
		CCam ();
	
	if(pev->impulse == 10)
	{
		local string a;
		a = ftoa(pev->items);
		sprint(pev, a);
		sprint(pev, "Items Printed\n");
	}
	if(pev->impulse == 11)
	{
		pev->items = pev->items + 8;
		sprint(pev, "Items added to\{136}\n");
	}

	if(pev->impulse == 150)
	{
		IEM_effects(TE_TELEPORT, pev->origin);
	}

	pev->impulse = 0;                              // Clear impulse list.
};