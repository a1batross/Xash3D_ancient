/*
FUNC_DOOR();

Description;
Points func_door to the unified func_mover(); code.
*/

void() func_door = 
{
	func_mover();
	pev->classname = "door";	
};