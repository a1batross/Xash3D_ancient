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
//
//  cdll_dll.h

// this file is included by both the game-dll and the client-dll,

#ifndef CDLL_DLL_H
#define CDLL_DLL_H

#define MAX_WEAPON_SLOTS		5	// hud item selection slots
#define MAX_ITEM_TYPES			6	// hud item selection slots
#define MAX_ITEMS				5	// hard coded item types

#define HIDEHUD_CROSSHAIR		( 1<<4 ) //LRC - probably not the right way to do this, but it's just an experiment.

#endif
