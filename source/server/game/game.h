/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#ifndef GAME_H
#define GAME_H

// cvar flags
#define CVAR_ARCHIVE	BIT(0)	// set to cause it to be saved to vars.rc
#define CVAR_USERINFO	BIT(1)	// added to userinfo  when changed
#define CVAR_SERVERINFO	BIT(2)	// added to serverinfo when changed
	
extern void GameDLLInit( void );
extern void GameDLLShutdown( void );

#endif		// GAME_H
