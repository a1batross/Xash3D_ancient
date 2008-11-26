//=======================================================================
//			Copyright XashXT Group 2007 ©
//			const.h - shared engine constants
//=======================================================================
#ifndef CONST_H
#define CONST_H

// edict_t->spawnflags
#define SF_START_ON			0x1

// entity_state_t->effects
#define EF_BRIGHTFIELD		(1<<0)	// swirling cloud of particles
#define EF_MUZZLEFLASH		(1<<1)	// single frame ELIGHT on entity attachment 0
#define EF_BRIGHTLIGHT		(1<<2)	// DLIGHT centered at entity origin
#define EF_DIMLIGHT			(1<<3)	// player flashlight
#define EF_INVLIGHT			(1<<4)	// get lighting from ceiling
#define EF_NOINTERP			(1<<5)	// don't interpolate the next frame
#define EF_NODRAW			(1<<6)	// don't draw entity
#define EF_ROTATE			(1<<7)	// rotate bonus item
#define EF_MINLIGHT			(1<<8)	// allways have some light (viewmodel)

// rendering constants
enum 
{	
	kRenderNormal,		// src
	kRenderTransColor,		// c*a+dest*(1-a)
	kRenderTransTexture,	// src*a+dest*(1-a)
	kRenderGlow,		// src*a+dest -- no Z buffer checks
	kRenderTransAlpha,		// src*srca+dest*(1-srca)
	kRenderTransAdd,		// src*a+dest
} kRenderMode_t;

enum 
{	
	kRenderFxNone = 0, 
	kRenderFxHologram,			// kRenderFxDistort + distance fade
	kRenderFxGlowShell,			// Glowing Shell
	kRenderFxNoReflect,			// Don't reflecting in mirrors 
} kRenderFx_t;

#endif//CONST_H