//=======================================================================
//			Copyright XashXT Group 2009 ©
//			game_shared.h - user constants
//=======================================================================
#ifndef GAME_SHARED_H
#define GAME_SHARED_H

#define HUD_PRINTNOTIFY		1
#define HUD_PRINTCONSOLE		2
#define HUD_PRINTTALK		3
#define HUD_PRINTCENTER		4

#define MAX_WEAPONS			64		// special for Ghoul[BB] mod support 
#define MAX_AMMO_SLOTS  		32
#define ITEM_SUIT			BIT( 31 )

#define HIDEHUD_WEAPONS		BIT( 0 )
#define HIDEHUD_FLASHLIGHT		BIT( 1 )
#define HIDEHUD_ALL			BIT( 2 )
#define HIDEHUD_HEALTH		BIT( 3 )

enum ShakeCommand_t
{
	SHAKE_START = 0,	// Starts the screen shake for all players within the radius.
	SHAKE_STOP,	// Stops the screen shake for all players within the radius.
	SHAKE_AMPLITUDE,	// Modifies the amplitude of an active screen shake for all players within the radius.
	SHAKE_FREQUENCY,	// Modifies the frequency of an active screen shake for all players within the radius.
};

#define FFADE_IN		0x0000 // Just here so we don't pass 0 into the function
#define FFADE_OUT		0x0001 // Fade out (not in)
#define FFADE_MODULATE	0x0002 // Modulate (don't blend)
#define FFADE_STAYOUT	0x0004 // ignores the duration, stays faded out until new ScreenFade message received
#define FFADE_CUSTOMVIEW	0x0008 // fading only at custom viewing (don't sending this to engine )

// camera flags
#define CAMERA_ON		1
#define DRAW_HUD		2
#define INVERSE_X		4
#define MONSTER_VIEW	8

#endif//GAME_SHARED_H