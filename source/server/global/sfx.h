//=======================================================================
//			Copyright (C) Shambler Team 2004
//		        sfx.h - different type special effects
//			e.g. explodes, sparks, smoke e.t.c			    
//=======================================================================
#ifndef SFX_H
#define SFX_H

#include "extdll.h"
#include "utils.h"
#include "cbase.h"
#include "baseweapon.h"
#include "player.h"
#include "defaults.h"
#include "shake.h"
#include "basebeams.h"
#include "te_temp.h"

// Break Model Defines

#define BREAK_TYPEMASK	0x4F
#define BREAK_GLASS		0x01
#define BREAK_METAL		0x02
#define BREAK_FLESH		0x04
#define BREAK_WOOD		0x08

#define BREAK_SMOKE		0x10
#define BREAK_TRANS		0x20
#define BREAK_CONCRETE	0x40
#define BREAK_2		0x80

// Colliding temp entity sounds
#define BOUNCE_GLASS	BREAK_GLASS
#define BOUNCE_METAL	BREAK_METAL
#define BOUNCE_FLESH	BREAK_FLESH
#define BOUNCE_WOOD		BREAK_WOOD
#define BOUNCE_SHRAP	0x10
#define BOUNCE_SHELL	0x20
#define BOUNCE_CONCRETE	BREAK_CONCRETE
#define BOUNCE_SHOTSHELL	0x80

// Temp entity bounce sound types
#define TE_BOUNCE_NULL	0
#define TE_BOUNCE_SHELL	1
#define TE_BOUNCE_SHOTSHELL	2

void SFX_Explode( short model, Vector origin, float scale, int flags = 0 );
void SFX_Trail( int entindex, short model, Vector color = Vector(200, 200, 200), float life = 30);
void SFX_MakeGibs( int shards, Vector pos, Vector size, Vector velocity, float time = 25, int flags = 0);
void SFX_EjectBrass ( const Vector &vecOrigin, const Vector &vecVelocity, float rotation, int model, int soundtype );
void SFX_Decal( const Vector &vecOrigin, int decalIndex, int entityIndex, int modelIndex );
void SFX_Light ( entvars_t *pev, float iTime, float decay, int attachment = 0 );
void SFX_Zap ( entvars_t *pev, const Vector &vecSrc, const Vector &vecDest );
void SFX_Ring ( entvars_t *pev, entvars_t *pev2 );

#endif //SFX_H