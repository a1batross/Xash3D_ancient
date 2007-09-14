/*
FUNC_BUTTON();

Description;
Points func_button to the unified func_mover(); code.
*/

void() func_button = 
{
	func_mover();
	self.classname = "button";	

};
