//=======================================================================
//			Copyright (C) Shambler Team 2004
//		          basebrush.h - base for all brush
//				entities. 			    
//=======================================================================
#ifndef BASEBRUSH_H
#define BASEBRUSH_H

//ANY BRUSH MAY BE SET MATERIAL
typedef enum {
	Glass = 0,	//glass.mdl
	Wood,		//wood.mdl
	Metal,		//metal.mdl
	Flesh,              //flesh.mdl
	CinderBlock,        //cinder.mdl
	CeilingTile,        //ceiling.mdl
	Computer,		//computer.mdl
	UnbreakableGlass,	//galss.mdl
	Rocks,		//rock.mdl
	Bones,		//bones.mdl
	Concrete,		//concrete.mdl
	MetalPlate,	//metalplate.mdl
	AirDuct,		//vent.mdl
	None,		//very strange pos
	LastMaterial,	//just in case
}Materials;

//gibs physics
typedef enum {
	Bounce = 0,	// bounce at collision
	Noclip,		// no collisions
	Sticky,        	// sticky physic
	Fly,		// fly
	Toss,		// toss
	WalkStep,		// monsters walk
} Physics;

static float MatFrictionTable( Materials mat)
{
	float friction = 0;

	switch(mat)
	{
	case CinderBlock:
	case Concrete:
	case Rocks: friction = 300; break;
	case Glass: friction = 200; break;
	case MetalPlate:
	case Metal: friction = 60; break;
	case Wood: friction = 250; break;
	case None: 
	default: friction = 100; break;
	}
	return friction;
}

static float MatByoancyTable( entvars_t *pev, Materials mat)
{
	float byoancy = 0;

	switch(mat)
	{
	case CinderBlock:
	case Concrete:
	case Rocks: byoancy = 0; break;
	case Glass: byoancy = 5; break;
	case MetalPlate:
	case Metal: byoancy = 1; break;
	case Wood: byoancy = 30; break;
	case None: 
	default: byoancy = 100; break;
	}
	return ( byoancy * (pev->maxs.x - pev->mins.x) * (pev->maxs.y - pev->mins.y) ) * 0.0005;
}

static float MatMassTable( entvars_t *pev, Materials mat)
{
	float mass = 0;

	switch(mat)
	{
	case CinderBlock:
	case Concrete:
	case Rocks: mass = 2; break;
	case Glass: mass = 1.2; break;
	case MetalPlate:
	case Metal: mass = 1.5; break;
	case Wood: mass = 0.8; break;
	case None: 
	default: mass = 1; break;
	}
	return (pev->size.Length() * mass);
}

static float MatVolume( Materials mat )
{
	float volume = 0;
	switch(mat)
	{
	case None: volume = 0.9;		//unbreakable
	case Bones:			//bones.mdl
	case Flesh: volume = 0.2; break;	//flesh.mdl
	case CinderBlock:        		//cinder.mdl
	case Concrete:			//concrete.mdl
	case Rocks: volume = 0.25; break;	//rock.mdl
	case Computer:			//computer.mdl
	case Glass: volume = 0.3; break;	//glass.mdl
	case MetalPlate:			//metalplate.mdl
	case Metal:			//metal.mdl
	case AirDuct: volume = 0.1; break;	//vent.mdl
	case CeilingTile:        		//ceiling.mdl
	case Wood: volume = 0.15; break;	//wood.mdl
	default: volume = 0.2; break;
	}

	return volume;
}

extern DLL_GLOBAL Vector g_vecAttackDir;
#define SetMoveDone( a ) m_pfnCallWhenMoveDone = static_cast <void (CBaseMover::*)(void)> (a)

class CBaseBrush : public CBaseLogic
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData* pkvd);

	void Blocked( CBaseEntity *pOther );
	virtual void AxisDir( void );
	virtual int TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	int DamageDecal( int bitsDamageType );

	BOOL IsBreakable( void );

	void EXPORT Die( void );
	virtual CBaseBrush *MyBrushPointer( void ) { return this; }
	virtual int ObjectCaps( void ) { return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }
	static	TYPEDESCRIPTION m_SaveData[];
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	
	static void MaterialSoundPrecache( Materials precacheMaterial );
	static void PlayRandomSound( edict_t *pEdict, Materials soundMaterial, float volume );
	static const char **MaterialSoundList( Materials precacheMaterial, int &soundCount );

	static const char *pSoundsWood[];
	static const char *pSoundsFlesh[];
	static const char *pSoundsMetal[];
	static const char *pSoundsCrete[];
	static const char *pSoundsGlass[];
	static const char *pSoundsCeil[];
          
	//spawnobject name
	static const char *pSpawnObjects[];

	void DamageSound( void );

	Materials	m_Material;
	CBasePlayer* m_pController;	// player pointer
	Vector	m_vecPlayerPos;	// player position
	int 	m_iMoveSound;	// move sound or preset
	int	m_iStartSound;	// start sound or preset
	int	m_iStopSound;	// stop sound or preset
	int	m_idShard;	// index of gibs
	int	m_iMagnitude;	// explosion magnitude
	int	m_iSpawnObject;	// spawnobject name
	int	m_iGibModel;	// custom gib model
	int	DmgType;		// temp container for right calculate damage
	float	m_flVolume;	// moving brushes has volume of move sound
	float	m_flBlockedTime;	// don't save this
	float	m_flRadius;	// sound radius or pendulum radius
	float	m_flAccel;          // declared here because rotating brushes use this
	float	m_flDecel;	// declared here because rotating brushes use this
	int	m_pitch;		// sound pitch
};

class CPushable : public CBaseBrush
{
public:
	void	Spawn ( void );
	void	Precache( void );
	void	Touch ( CBaseEntity *pOther );
	void	Move( CBaseEntity *pMover, int push );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int ObjectCaps( void ) { return (CBaseBrush :: ObjectCaps() | FCAP_CONTINUOUS_USE); }
	virtual BOOL IsPushable( void ) { return TRUE; }
          	
	inline float MaxSpeed( void ) { return pev->speed; }

	static const char *pPushWood[];
	static const char *pPushFlesh[];
	static const char *pPushMetal[];
	static const char *pPushCrete[];
	static const char *pPushGlass[];
	static const char *pPushCeil[];	//fixme

	static void PushSoundPrecache( Materials precacheMaterial );
	static int  PlayPushSound( edict_t *pEdict, Materials soundMaterial, float volume );
	static void StopPushSound( edict_t *pEdict, Materials soundMaterial, int m_lastPushSnd );
	static const char **PushSoundList( Materials precacheMaterial, int &soundCount );

	int m_lastSound; // no need to save/restore, just keeps the same sound from playing twice in a row
};

#include "basemover.h"

#endif	// BASEBRUSH_H
