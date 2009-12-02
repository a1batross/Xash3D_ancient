//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================

#ifndef BASEINFO_H
#define BASEINFO_H

class CPointEntity : public CBaseEntity
{
public:
	void Spawn( void ){ SetBits( pev->flags, FL_POINTENTITY ); pev->solid = SOLID_NOT; }
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
};

class CNullEntity : public CBaseEntity
{
public:
	void Spawn( void ){ REMOVE_ENTITY(ENT(pev)); }
};

class CBaseDMStart : public CPointEntity
{
public:
	void	KeyValue( KeyValueData *pkvd );
	STATE	GetState( CBaseEntity *pEntity );

private:
};

class CLaserSpot : public CBaseEntity//laser spot for different weapons
{
	void Spawn( void );
	void Precache( void );
	int ObjectCaps( void ) { return FCAP_DONT_SAVE; }
public:
	void Suspend( float flSuspendTime );
	void Update( CBasePlayer *m_pPlayer );
	void EXPORT Revive( void );
	void Killed( void ){ UTIL_Remove( this ); }
	
	static CLaserSpot *CreateSpot( void );
};

#endif //BASEINFO_H