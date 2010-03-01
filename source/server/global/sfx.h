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
#include "te_message.h"

void SFX_Explode( short model, Vector origin, float scale, int flags = 0 );
void SFX_Trail( int entindex, short model, Vector color = Vector(200, 200, 200), float life = 30);
void SFX_MakeGibs( int shards, Vector pos, Vector size, Vector velocity, float time = 25, int flags = 0);
void SFX_EjectBrass ( const Vector &vecOrigin, const Vector &vecVelocity, float rotation, int model, int soundtype );
void SFX_Decal( const Vector &vecOrigin, int decalIndex, int entityIndex, int modelIndex );
void SFX_Light ( entvars_t *pev, float iTime, float decay, int attachment = 0 );
void SFX_Zap ( entvars_t *pev, const Vector &vecSrc, const Vector &vecDest );
void SFX_Ring ( entvars_t *pev, entvars_t *pev2 );

#endif //SFX_H