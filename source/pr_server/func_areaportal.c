.float	count;
.float	style;

void use_areaportal( void )
{
	pev->count = 1; // toggle state
	areaportal_state( pev->style, pev->count);
}

/*QUAKED func_areaportal (0 0 0) ?

This is a non-visible object that divides the world into
areas that are seperated when this portal is not activated.
Usually enclosed in the middle of a door.
*/
void func_areaportal( void )
{
	pev->use = use_areaportal;
	pev->count = 0; // always start closed;
}