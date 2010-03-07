//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================

#ifndef BASEBEAMS_H
#define BASEBEAMS_H

#include "beam_def.h"

#define SF_BEAM_TEMPORARY		0x8000
#define SF_BEAM_TRIPPED		0x80000		// bit who indicated "trip" action

class CBeam : public CBaseLogic
{
public:
	void Spawn( void );
	int ObjectCaps( void )
	{ 
		int flags = 0;
		if ( pev->spawnflags & SF_BEAM_TEMPORARY )
			flags = FCAP_DONT_SAVE;
		return (CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | flags; 
	}

	void Touch( CBaseEntity *pOther );

	// These functions are here to show the way beams are encoded as entities.
	// Encoding beams as entities simplifies their management in the client/server architecture
	inline void SetType( int type ) { pev->rendermode = type; }
	inline void SetFlags( int flags ) { pev->renderfx |= flags; }
	inline void SetStartPos( const Vector& pos ) { pev->origin = pos; }
	inline void SetEndPos( const Vector& pos ) { pev->oldorigin = pos; }
	void SetStartEntity( edict_t *pEnt );
	void SetEndEntity( edict_t *pEnt );

	inline void SetStartAttachment( int attachment ) { pev->colormap = (pev->colormap & 0xFF00)>>8 | attachment; }
	inline void SetEndAttachment( int attachment ) { pev->colormap = (pev->colormap & 0xFF) | (attachment<<8); }

	inline void SetTexture( int spriteIndex ) { pev->modelindex = spriteIndex; }
	inline void SetWidth( int width ) { pev->scale = width; }
	inline void SetNoise( int amplitude ) { pev->body = amplitude; }
	inline void SetColor( int r, int g, int b ) { pev->rendercolor.x = r; pev->rendercolor.y = g; pev->rendercolor.z = b; }
	inline void SetBrightness( int brightness ) { pev->renderamt = brightness; }
	inline void SetFrame( float frame ) { pev->frame = frame; }
	inline void SetScrollRate( int speed ) { pev->animtime = speed; }

	inline int  GetType( void ) { return pev->rendermode; }
	inline int  GetFlags( void ) { return pev->renderfx; }
	inline edict_t *GetStartEntity( void ) { return pev->owner; }
	inline edict_t *GetEndEntity( void ) { return pev->aiment; }

	const Vector &GetStartPos( void );
	const Vector &GetEndPos( void );

	Vector Center( void ) { return (GetStartPos() + GetEndPos()) * 0.5; }; // center point of beam

	inline int GetTexture( void ) { return pev->modelindex; }
	inline int GetWidth( void ) { return pev->scale; }
	inline int GetNoise( void ) { return pev->body; }
	inline Vector GetColor( void ) { return Vector( pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z ); }
	inline int  GetBrightness( void ) { return pev->renderamt; }
	inline int  GetFrame( void ) { return pev->frame; }
	inline int  GetScrollRate( void ) { return pev->animtime; }

	CBaseEntity*	GetTripEntity( TraceResult *ptr );

	// Call after you change start/end positions
	void	RelinkBeam( void );
	void	SetObjectCollisionBox( void );

	void	DoSparks( const Vector &start, const Vector &end );
	CBaseEntity *RandomTargetname( const char *szName );
	void	BeamDamage( TraceResult *ptr );
	// Init after BeamCreate()
	void	BeamInit( const char *pSpriteName, int width );
	void	PointsInit( const Vector &start, const Vector &end );
	void	PointEntInit( const Vector &start, edict_t *pEnd );
	void	EntsInit( edict_t *pStart, edict_t *pEnd );
	void	HoseInit( const Vector &start, const Vector &direction );

	static CBeam *BeamCreate( const char *pSpriteName, int width );

	inline void LiveForTime( float time ) { SetThink( Remove ); SetNextThink( time ); }
	inline void BeamDamageInstant( TraceResult *ptr, float damage ) 
	{ 
		pev->dmg = damage; 
		pev->dmgtime = gpGlobals->time - 1;
		BeamDamage(ptr); 
	}
};

class CLaser : public CBeam
{
public:
	void	Spawn( void );
	void	PostSpawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
          void	Activate( void );
	void	SetObjectCollisionBox( void );
	void	TurnOn( void );
	void	TurnOff( void );
	void	FireAtPoint( Vector startpos, TraceResult &point );

	void	Think( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	CSprite	*m_pStartSprite;
	CSprite	*m_pEndSprite;
};

#endif		//BASEBEAMS_H