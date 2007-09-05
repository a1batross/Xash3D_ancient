/*
FUNC_PATH_CORNER();

Description;
This is the spawning function to the map entity 'func_path_corner'
Currently only trains use this to tell their next stop position;
*/

void() func_path_corner = 
{
	if (!self.targetname)
		objerror ("monster_movetarget: no targetname");
		
	self.solid = SOLID_TRIGGER;
	setsize (self, '-8 -8 -8', '8 8 8');
};