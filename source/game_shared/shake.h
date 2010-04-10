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
#ifndef SHAKE_H
#define SHAKE_H

// Screen / View effects

//
// Commands for the screen shake effect.
//
enum ShakeCommand_t
{
	SHAKE_START = 0,	// Starts the screen shake for all players within the radius.
	SHAKE_STOP,	// Stops the screen shake for all players within the radius.
	SHAKE_AMPLITUDE,	// Modifies the amplitude of an active screen shake for all players within the radius.
	SHAKE_FREQUENCY,	// Modifies the frequency of an active screen shake for all players within the radius.
};

typedef struct
{
	unsigned short	command;		// ShakeCommand_t
	unsigned short	amplitude;	// FIXED 4.12 amount of shake
	unsigned short 	duration;		// FIXED 4.12 seconds duration
	unsigned short	frequency;	// FIXED 8.8 low frequency is a jerk,high frequency is a rumble
} ScreenShake;

#define FFADE_IN		0x0001 // Fade in (not out)
#define FFADE_OUT		0x0002 // Fade out (not in)
#define FFADE_MODULATE	0x0004 // Modulate (don't blend)
#define FFADE_STAYOUT	0x0008 // ignores the duration, stays faded out until new ScreenFade message received
#define FFADE_CUSTOMVIEW	0x0010 // fading only at custom viewing (don't sending this to engine )
#define FFADE_PURGE		0x0020 // Purges all other fades, replacing them with this one

// Fade in/out
// This structure is sent over the net to describe a screen fade event
typedef struct
{
	unsigned short	duration;		// FIXED 4.12 seconds duration
	unsigned short	holdTime;		// FIXED 4.12 seconds duration until reset (fade & hold)
	short		fadeFlags;	// flags
	byte		r, g, b, a;	// fade to color ( max alpha )
} ScreenFade;

#endif		// SHAKE_H

