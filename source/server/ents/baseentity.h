//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================

#ifndef BASEENTITY_H
#define BASEENTITY_H

#include "entity_state.h"

class CBaseEntity
{
public:
	// Constructor.  Set engine to use C/C++ callback functions
	// pointers to engine data
	entvars_t *pev;	// Don't need to save/restore this pointer, the engine resets it

	// path corners
	CBaseEntity	*m_pGoalEnt;// path corner we are heading towards
	CBaseEntity	*m_pLink;// used for temporary link-list operations.

	float		m_fNextThink;
	float		m_fPevNextThink;
	float		flTravelTime; // time to moving brushes
	int		m_iClassType; // edict classtype
	int		m_iStyle;
	int		m_iAcessLevel;// acess level for retinal sacners

	//===================================================================================================
	//			Xash BaseEntity
	//===================================================================================================

	// Gets the interface to the collideable representation of the entity
	virtual void		SetModelIndex( int index );
	virtual int		GetModelIndex( void ) const;

	// Returns a CBaseAnimating if the entity is derived from CBaseAnimating.
	virtual CBaseAnimating*	GetBaseAnimating() { return NULL; }

	void SetName( string_t newTarget );
	void SetParent( string_t newParent, CBaseEntity *pActivator );
	virtual void ChangeCamera( string_t newcamera ) {}

public:
	//===================================================================================================
	//			Xash Parent System 0.2 beta
	//===================================================================================================
	
	Vector	PostOrigin;	//child postorigin
	Vector	PostAngles;	//child postangles
	Vector	PostVelocity;	//child postvelocity
	Vector	PostAvelocity;	//child postavelocity	
	Vector	OffsetOrigin;	//spawn offset origin
	Vector	OffsetAngles;	//spawn offset angles
	Vector 	pParentAngles;	//temp container
	Vector 	pParentOrigin;	//temp container
	Vector	m_vecSpawnOffset;	//temp container
	int	pFlags;		//xash flags
          BOOL	m_physinit;	//physics initializator
	
	virtual void SetParent ( void ) { SetParent(m_iParent); }
	void SetParent ( int m_iNewParent, int m_iAttachment = 0 );
	void SetParent ( CBaseEntity* pParent, int m_iAttachment = 0 );
	void ResetParent( void );

	virtual void SetupPhysics( void );	// setup parent system and physics

	CBaseEntity *m_pParent;		// pointer to parent entity
	CBaseEntity *m_pChild;		// pointer to children(may be this)
	CBaseEntity *m_pNextChild;		// link to next chlidren
	CBaseEntity *m_pLinkList;		// list of linked childrens
	
	string_t m_iParent;//name of parent
	virtual void SetNextThink( float delay, BOOL correctSpeed = FALSE );
	virtual void AbsoluteNextThink( float time, BOOL correctSpeed = FALSE );
	void	   SetEternalThink( void );
	void	   DontThink( void );
	virtual void ThinkCorrection( void );

	//phys metods
	virtual void SetAngularImpulse( float impulse ){}
	virtual void SetLinearImpulse( float impulse ) {}
	
	// initialization functions
	virtual void	Spawn( void ) { return; }
	virtual void	Precache( void ) { return; }
	virtual void	KeyValue( KeyValueData* pkvd)
	{
		if (FStrEq(pkvd->szKeyName, "parent"))
		{
			m_iParent = ALLOC_STRING(pkvd->szValue);
			pkvd->fHandled = TRUE;
		}
		else if (FStrEq(pkvd->szKeyName, "style"))
		{
			m_iStyle = atoi(pkvd->szValue);
			pkvd->fHandled = TRUE;
		}
		else if (FStrEq(pkvd->szKeyName, "angle"))
		{
			// quake legacy
			if( pev->angles == g_vecZero )
				pev->angles[1] = atof(pkvd->szValue);
			pkvd->fHandled = TRUE;
		}
		else pkvd->fHandled = FALSE;
	}
	virtual int  Save( CSave &save );
	virtual int  Restore( CRestore &restore );
	virtual int  ObjectCaps( void ) { return m_pParent?m_pParent->ObjectCaps()&FCAP_ACROSS_TRANSITION:FCAP_ACROSS_TRANSITION; }
	virtual void Activate( void ) {}
	virtual void PostActivate( void ) {}
	virtual void PostSpawn( void ) {}
	virtual void DesiredAction( void ) {}
	virtual void StartMessage( CBasePlayer *pPlayer ) {}
	virtual void SetObjectClass( int iClassType = ED_SPAWNED )
	{
		m_iClassType = iClassType;
	}

          virtual BOOL IsItem( void ) { return FALSE; }
          virtual BOOL IsAmmo( void ) { return FALSE; }
          virtual BOOL IsWeapon( void ) { return FALSE; }

	// Setup the object->object collision box (pev->mins / pev->maxs is the object->world collision box)
	virtual void	SetObjectCollisionBox( void );

	void UTIL_AutoSetSize( void )//automatically set collision box
	{
		dstudiohdr_t *pstudiohdr;
		pstudiohdr = (dstudiohdr_t*)GET_MODEL_PTR( ENT(pev) );

		if (pstudiohdr == NULL)
		{
			ALERT(at_console,"Unable to fetch model pointer!\n");
			return;
		}
		dstudioseqdesc_t    *pseqdesc;
		pseqdesc = (dstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);
		UTIL_SetSize(pev,pseqdesc[ pev->sequence ].bbmin,pseqdesc[ pev->sequence ].bbmax);
	}

	// Classify - returns the type of group (e.g., "alien monster", or "human military" so that monsters
	// on the same side won't attack each other, even if they have different classnames.
	virtual int Classify ( void ) { return CLASS_NONE; };
	virtual void DeathNotice ( entvars_t *pevChild ) {}// monster maker children use this to tell the monster maker that they have died.


	// global concept of "entities with states", so that state_watchers and
	// mastership (mastery? masterhood?) can work universally.
	virtual STATE GetState ( void ) { return STATE_OFF; };

	// For team-specific doors in multiplayer, etc: a master's state depends on who wants to know.
	virtual STATE GetState ( CBaseEntity* pEnt ) { return GetState(); };

	static	TYPEDESCRIPTION m_SaveData[];

	virtual void	ClearPointers( void );
	virtual void	TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	virtual int	TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType );
	virtual int	TakeHealth( float flHealth, int bitsDamageType );
	virtual int	TakeArmor( float flArmor, int suit = 0 );
	virtual int	TakeItem( int iItem ) { return 0; }
	virtual void	Killed( entvars_t *pevAttacker, int iGib );
	virtual int	BloodColor( void ) { return DONT_BLEED; }
	virtual void	TraceBleed( float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	virtual CBaseMonster *MyMonsterPointer( void ) { return NULL;}
	virtual CSquadMonster *MySquadMonsterPointer( void ) { return NULL;}
	virtual CBaseBrush *MyBrushPointer( void ) { return NULL; }
	virtual void	AddPoints( int score, BOOL bAllowNegativeScore ) {}
	virtual void	AddPointsToTeam( int score, BOOL bAllowNegativeScore ) {}
	virtual BOOL	AddPlayerItem( CBasePlayerWeapon *pItem ) { return 0; }
	virtual BOOL	RemovePlayerItem( CBasePlayerWeapon *pItem ) { return 0; }
	virtual int 	GiveAmmo( int iAmount, char *szName, int iMax ) { return -1; };
	virtual float	GetDelay( void ) { return 0; }
	virtual int	IsMoving( void ) { return pev->velocity != g_vecZero; }
	virtual void	RestorePhysics( void );
	virtual void	OverrideReset( void ) {}
	virtual int	DamageDecal( int bitsDamageType );
	// This is ONLY used by the node graph to test movement through a door
	virtual void	SetToggleState( int state ) {}
	virtual void    	StartSneaking( void ) {}
	virtual void    	StopSneaking( void ) {}
	virtual BOOL	OnControls( entvars_t *pev ) { return FALSE; }
	virtual BOOL    	IsSneaking( void ) { return FALSE; }
	virtual BOOL	IsAlive( void ) { return (pev->deadflag == DEAD_NO) && pev->health > 0; }
	virtual BOOL	IsBSPModel( void ) { return pev->solid == SOLID_BSP || pev->movetype == MOVETYPE_PUSHSTEP; }
	virtual BOOL	ReflectGauss( void ) { return ( IsBSPModel() && !pev->takedamage ); }
	virtual BOOL	HasTarget( string_t targetname ) { return FStrEq(STRING(targetname), STRING(pev->targetname) ); }
	virtual BOOL    	IsInWorld( void );
	virtual BOOL	IsPlayer( void ) { return FALSE; }
	virtual BOOL	IsPushable( void ) { return FALSE; }
	virtual BOOL	IsMonster( void ) { return (pev->flags & FL_MONSTER ? TRUE : FALSE); }
	virtual BOOL	IsNetClient( void ) { return FALSE; }
	virtual BOOL	IsFuncScreen( void ) { return FALSE; }
	virtual const char *TeamID( void ) { return ""; }
          
	virtual CBaseEntity *GetNext( void ) { return NULL; }
	virtual CBaseEntity *GetPrev( void ) { return NULL; }
	
	// fundamental callbacks
	void (CBaseEntity ::*m_pfnThink)(void);
	void (CBaseEntity ::*m_pfnTouch)( CBaseEntity *pOther );
	void (CBaseEntity ::*m_pfnUse)( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void (CBaseEntity ::*m_pfnBlocked)( CBaseEntity *pOther );

	virtual void Think( void ) { if (m_pfnThink) (this->*m_pfnThink)(); };
	virtual void Touch( CBaseEntity *pOther ) { if (m_pfnTouch) (this->*m_pfnTouch)( pOther ); };
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
	{
		if (m_pfnUse) (this->*m_pfnUse)( pActivator, pCaller, useType, value );
	}
	virtual void Blocked( CBaseEntity *pOther ) { if (m_pfnBlocked) (this->*m_pfnBlocked)( pOther ); };

	// allow engine to allocate instance data
	void *operator new( size_t stAllocateBlock, entvars_t *pev )
	{
		return (void *)ALLOC_PRIVATE(ENT(pev), stAllocateBlock);
	};

	// don't use this.
#if _MSC_VER >= 1200 // only build this code if MSVC++ 6.0 or higher
	void operator delete(void *pMem, entvars_t *pev)
	{
		pev->flags |= FL_KILLME;
	};
#endif

	virtual void UpdateOnRemove( void );

	// common member functions
	void EXPORT Remove( void );
	void EXPORT Fadeout( void );
	void EXPORT PVSRemove( void );
	void EXPORT SUB_CallUseToggle( void ) { this->Use( this, this, USE_TOGGLE, 0 ); }

	void   FireBullets( ULONG cShots, Vector  vecSrc, Vector	vecDirShooting, Vector vecSpread, float flDistance, int iBulletType, int iTracerFreq = 4, int iDamage = 0, entvars_t *pevAttacker = NULL  );

	virtual CBaseEntity *Respawn( void ) { return NULL; }

	// Do the bounding boxes of these two intersect?
	int	Intersects( CBaseEntity *pOther );
	void	MakeDormant( void );
	int	IsDormant( void );
	BOOL	IsLockedByMaster( void ) { return FALSE; }

	static CBaseEntity *Instance( edict_t *pent )
	{
		if ( !pent ) pent = ENT(0);
		CBaseEntity *pEnt = (CBaseEntity *)GET_PRIVATE(pent);
		return pEnt;
	}

	static CBaseEntity *Instance( entvars_t *pev ) { return Instance( ENT( pev ) ); }
	static CBaseEntity *Instance( int eoffset) { return Instance( ENT( eoffset) ); }

	CBaseMonster *GetMonsterPointer( entvars_t *pevMonster )
	{
		CBaseEntity *pEntity = Instance( pevMonster );
		if ( pEntity ) return pEntity->MyMonsterPointer();
		return NULL;
	}
	CBaseMonster *GetMonsterPointer( edict_t *pentMonster )
	{
		CBaseEntity *pEntity = Instance( pentMonster );
		if ( pEntity ) return pEntity->MyMonsterPointer();
		return NULL;
	}


	// Ugly code to lookup all functions to make sure they are exported when set.
#ifdef _DEBUG
	void FunctionCheck( void *pFunction, char *name )
	{
		if (pFunction && !NAME_FOR_FUNCTION((unsigned long)(pFunction)) )
			ALERT( at_error, "No EXPORT: %s:%s (%08lx)\n", STRING(pev->classname), name, (unsigned long)pFunction );
	}

	BASEPTR	ThinkSet( BASEPTR func, char *name )
	{
		m_pfnThink = func;
		FunctionCheck( (void *)*((int *)((char *)this + ( offsetof(CBaseEntity,m_pfnThink)))), name );
		return func;
	}
	ENTITYFUNCPTR TouchSet( ENTITYFUNCPTR func, char *name )
	{
		m_pfnTouch = func;
		FunctionCheck( (void *)*((int *)((char *)this + ( offsetof(CBaseEntity,m_pfnTouch)))), name );
		return func;
	}
	USEPTR	UseSet( USEPTR func, char *name )
	{
		m_pfnUse = func;
		FunctionCheck( (void *)*((int *)((char *)this + ( offsetof(CBaseEntity,m_pfnUse)))), name );
		return func;
	}
	ENTITYFUNCPTR	BlockedSet( ENTITYFUNCPTR func, char *name )
	{
		m_pfnBlocked = func;
		FunctionCheck( (void *)*((int *)((char *)this + ( offsetof(CBaseEntity,m_pfnBlocked)))), name );
		return func;
	}

#endif
	// used by monsters that are created by the MonsterMaker
	virtual	void UpdateOwner( void ) { return; };
	static CBaseEntity *Create( char *szName, const Vector &vecOrigin, const Vector &vecAngles, edict_t *pentOwner = NULL );
          static CBaseEntity *CBaseEntity::CreateGib( char *szName, char *szModel );
          
	virtual BOOL FBecomeProne( void ) {return FALSE;};
	edict_t *edict() { return ENT( pev ); };
	EOFFSET eoffset( ) { return OFFSET( pev ); };
	int entindex( ) { return ENTINDEX( edict() ); };

	virtual Vector Center( ) { return (pev->absmax + pev->absmin) * 0.5; };  // center point of entity
	virtual Vector EyePosition( ) { return pev->origin + pev->view_ofs; };   // position of eyes
	virtual Vector EarPosition( ) { return pev->origin + pev->view_ofs; };   // position of ears
	virtual Vector BodyTarget( const Vector &posSrc ) { return Center( ); }; // position to shoot at

	virtual int Illumination( ) { return GETENTITYILLUM( ENT( pev ) ); };

	virtual BOOL FVisible ( CBaseEntity *pEntity );
	virtual BOOL FVisible ( const Vector &vecOrigin );
};

inline void CBaseEntity::SetModelIndex( int index )
{
	pev->modelindex = index;
}

inline int CBaseEntity::GetModelIndex( void ) const
{
	return pev->modelindex;
}

#endif //BASEENTITY_H