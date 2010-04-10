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

#define MAX_WEAPONS			32
#define WEAPON_ALLWEAPONS		(~(1<<WEAPON_SUIT))
#define MAX_AMMO_SLOTS  		32
#define ITEM_SUIT			BIT( 31 )
#define WEAPON_SUIT			31

#define HIDEHUD_WEAPONS		BIT( 0 )
#define HIDEHUD_FLASHLIGHT		BIT( 1 )
#define HIDEHUD_ALL			BIT( 2 )
#define HIDEHUD_HEALTH		BIT( 3 )

// camera flags
#define CAMERA_ON		1
#define DRAW_HUD		2
#define INVERSE_X		4
#define MONSTER_VIEW	8

#endif//GAME_SHARED_H