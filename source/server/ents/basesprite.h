//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================

#ifndef BASESPRITE_H
#define BASESPRITE_H

#define SF_TEMPSPRITE		0x8000 //play sequence and die
#include "baseworld.h"

class CSprite : public CBaseLogic
{
public:
	void Spawn( void );
	void Precache( void ) { UTIL_PrecacheModel( pev->model ); }
          void PostActivate( void )
          {
		//env_glow always enabled
		if( FClassnameIs( pev, "env_glow" ) || FStringNull( pev->targetname ) || pev->spawnflags & SF_START_ON )
		{
			TurnOn();
			ClearBits( pev->spawnflags, SF_START_ON );
		}
		else TurnOff();
	}
	
	void Think( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	void Move( void );
	void PostSpawn( void );
	void Animate( float frames )
	{
		if( Frames() > 1 )
		{
			pev->frame += frames;
			if( pev->frame > Frames() )
			{
				if( pev->spawnflags &  SF_FIREONCE ) TurnOff();
				else if( Frames() > 0 ) pev->frame = fmod( pev->frame, Frames() );
			}
		}
	}
	void TurnOff( void );
	void TurnOn( void );
	float Frames( void ) { return MODEL_FRAMES( pev->modelindex ) - 1; }
	inline void SetTransparency( int rendermode, int r, int g, int b, int a, int fx )
	{
		pev->rendermode = rendermode;
		pev->rendercolor.x = r;
		pev->rendercolor.y = g;
		pev->rendercolor.z = b;
		pev->renderamt = a;
		pev->renderfx = fx;
	}
	inline void SetTexture( int spriteIndex ) { pev->modelindex = spriteIndex; }
	inline void SetScale( float scale ) { pev->scale = scale; }
	inline void SetBrightness( int brightness ) { pev->renderamt = brightness; }

	inline void AnimateAndDie( float framerate ) 
	{ 
		SetBits( pev->spawnflags, SF_TEMPSPRITE );
		pev->framerate = framerate;
		pev->dmg_take = gpGlobals->time + (Frames() / framerate); 
		SetNextThink( 0 );
	}
	static CSprite *SpriteCreate( const char *pSpriteName, const Vector &origin, BOOL animate )
	{
		CSprite *pSprite = GetClassPtr( (CSprite *)NULL );
		pSprite->pev->classname = MAKE_STRING("env_sprite");
		pSprite->pev->model = MAKE_STRING(pSpriteName);
		pSprite->pev->origin = origin;
		pSprite->Spawn();
		if ( animate ) pSprite->TurnOn();
                    else ClearBits( pSprite->pev->effects, EF_NODRAW );
                    
		return pSprite;
	}
};

class CShot : public CSprite
{
public:
	void Touch ( CBaseEntity *pOther )
	{
		if (pev->teleport_time > gpGlobals->time) return;
		pev->teleport_time = gpGlobals->time + 0.1;
		if ( pOther != g_pWorld) UTIL_FireTargets( pev->target, pOther, this, USE_ON );
		else UTIL_FireTargets( pev->target, pOther, this, USE_SET ); //world touch
	}
	static CShot *CreateShot ( const char *szGibModel, Vector size )
	{
		CShot *pShot = GetClassPtr( (CShot*)NULL );
		pShot->pev->classname = MAKE_STRING("shot");
		pShot->pev->solid = SOLID_SLIDEBOX;
		UTIL_SetModel(ENT(pShot->pev), szGibModel );
		UTIL_SetSize(pShot->pev, -size, size );
		return pShot;
	}
};

class CGib : public CBaseEntity
{
public:
	void Spawn( const char *szGibModel );
	void EXPORT BounceGibTouch ( CBaseEntity *pOther );
	void EXPORT StickyGibTouch ( CBaseEntity *pOther );
	void EXPORT WaitTillLand( void );

	virtual int ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_DONT_SAVE; }
	static CGib *CreateGib ( CBaseEntity *pVictim, const char *szGibModel, int gibtype );
	
	//spawn gibs
	static void SpawnHeadGib( CBaseEntity *pVictim, string_t m_iGibModel = NULL )
	{
		if(FStringNull( m_iGibModel )) CreateGib( pVictim, "models/hgibs.mdl", 1 );
		else CreateGib( pVictim, STRING( m_iGibModel ), 1 );
	}
	static void SpawnRandomGibs( CBaseEntity *pVictim, int cGibs, int human, string_t m_iGibModel = NULL )
	{
		const char *model;
		if(FStringNull( m_iGibModel ))
		{
			if(human) model = "models/hgibs.mdl";
			else model = "models/agibs.mdl"; 
		}
		else model = (char *)STRING( m_iGibModel );
		for(int i = 0; i < cGibs; i++) CreateGib( pVictim, model, 0 );
	}
	static void SpawnStickyGibs( CBaseEntity *pVictim, int cGibs, string_t m_iGibModel = NULL )
	{
		if(!FStringNull( m_iGibModel )) for(int i = 0; i < cGibs; i++) CreateGib( pVictim, STRING( m_iGibModel ), 2 );
		else for(int i = 0; i < cGibs; i++) CreateGib( pVictim, "models/stickygib.mdl", 2 );
	}

	int m_bloodColor;
	int m_cBloodDecals;
	int m_material;
	float m_lifeTime;
};

#endif //BASESPRITE_H