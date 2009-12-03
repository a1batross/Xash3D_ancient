//=======================================================================
//			Copyright (C) Shambler Team 2004
//		        bullets.h - shared bullets enum
//=======================================================================
#ifndef BULLETS_H
#define BULLETS_H

// bullet types
typedef	enum
{
	BULLET_NONE = 0,
	BULLET_9MM, 	// glock
	BULLET_MP5, 	// mp5
	BULLET_357, 	// python
	BULLET_12MM,	// sentry turret
	BULLET_556,	// m249 bullet
	BULLET_762,	// sniper bullet
	BULLET_BUCKSHOT,	// shotgun
	BULLET_CROWBAR,	// crowbar swipe
} Bullet;

#endif //BULLETS_H