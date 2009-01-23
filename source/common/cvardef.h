//=======================================================================
//			Copyright XashXT Group 2008 ©
//		     cvardef.h - pointer to console variable
//=======================================================================
#ifndef CVARDEF_H
#define CVARDEF_H

// cvar flags
#define FCVAR_ARCHIVE	BIT(0)	// set to cause it to be saved to vars.rc
#define FCVAR_USERINFO	BIT(1)	// added to userinfo  when changed
#define FCVAR_SERVERINFO	BIT(2)	// added to serverinfo when changed
#define FCVAR_LATCH		BIT(3)	// create latched cvar

/*
========================================================================
console variables
external and internal cvars struct have some differences
========================================================================
*/
struct cvar_s
{
	char	*name;
	char	*string;		// normal string
	float	value;		// com.atof( string )
	int	integer;		// com.atoi( string )
	bool	modified;		// set each time the cvar is changed
};

#endif//CVARDEF_H