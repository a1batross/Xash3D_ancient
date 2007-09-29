/*
FUNC_PATH_CORNER();

Description;
This is the spawning function to the map entity 'func_path_corner'
Currently only trains use this to tell their next stop position;
*/

void() path_corner = 
{
	if (!pev->targetname)
		objerror ("monster_movetarget: no targetname");
		
	pev->solid = SOLID_NOT;
	setsize (pev, '-8 -8 -8', '8 8 8');
};