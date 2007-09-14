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
	if (self.impulse == 15)
		CCam ();
	
	if(self.impulse == 10)
	{
		local string a;
		a = ftos(self.items);
		sprint(self, a);
		sprint(self, "Items Printed\n");
	}
	if(self.impulse == 11)
	{
		self.items = self.items + 8;
		sprint(self, "Items added to\{136}\n");
	}

	if(self.impulse == 150)
	{
		IEM_effects(TE_TELEPORT, self.origin);
	}

	self.impulse = 0;                              // Clear impulse list.
};