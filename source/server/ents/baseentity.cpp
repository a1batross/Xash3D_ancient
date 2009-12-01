//=======================================================================
//			Copyright (C) Shambler Team 2005
//		         	 baseentity.cpp - base class 
//=======================================================================

#include	"extdll.h"
#include  "utils.h"
#include	"cbase.h"
#include	"saverestore.h"
#include	"client.h"
#include  "nodes.h"
#include	"decals.h"
#include	"gamerules.h"
#include	"game.h"
#include	"damage.h"
#include	"defaults.h"
#include	"bullets.h"

extern Vector VecBModelOrigin( entvars_t* pevBModel );
extern DLL_GLOBAL Vector g_vecAttackDir;
extern void SetObjectCollisionBox( entvars_t *pev );
extern BOOL NewLevel;
extern CGraph WorldGraph;

//=======================================================================
//			decent mechanisms
//=======================================================================
void CBaseEntity::DontThink( void )
{
	m_fNextThink = 0;
	if (m_pParent == NULL && m_pChild == NULL)
	{
		pev->nextthink = 0;
		m_fPevNextThink = 0;
	}
}

void CBaseEntity :: SetEternalThink( void )
{
	if (pev->movetype == MOVETYPE_PUSH)
	{
		pev->nextthink = pev->ltime + 1E6;
		m_fPevNextThink = pev->nextthink;
	}
	CBaseEntity *pChild;
	for (pChild = m_pChild; pChild != NULL; pChild = pChild->m_pNextChild)
		pChild->SetEternalThink( );
}

void CBaseEntity :: SetNextThink( float delay, BOOL correctSpeed )
{
	if (m_pParent || m_pChild )
	{
		if (pev->movetype == MOVETYPE_PUSH) 
			m_fNextThink = pev->ltime + delay;
		else m_fNextThink = gpGlobals->time + delay;

		SetEternalThink( );
		UTIL_MarkChild( this, correctSpeed, FALSE );
	}
	else
	{
		// set nextthink as normal.
		if (pev->movetype == MOVETYPE_PUSH)pev->nextthink = pev->ltime + delay;
		else pev->nextthink = gpGlobals->time + delay;
		m_fPevNextThink = m_fNextThink = pev->nextthink;
	}
}

void CBaseEntity :: AbsoluteNextThink( float time, BOOL correctSpeed )
{
	if (m_pParent || m_pChild)
	{
		m_fNextThink = time;
		SetEternalThink( );
		UTIL_MarkChild( this, correctSpeed, FALSE );
	}
	else
	{
		// set nextthink as normal.
		pev->nextthink = time;
		m_fPevNextThink = m_fNextThink = pev->nextthink;
	}
}

void CBaseEntity :: ThinkCorrection( void )
{
	if (pev->nextthink != m_fPevNextThink)
	{
		m_fNextThink += pev->nextthink - m_fPevNextThink;
		m_fPevNextThink = pev->nextthink;
	}
}

//=======================================================================
//	set parent (void ) dinamically link parents
//=======================================================================
void CBaseEntity :: SetParent( int m_iNewParent, int m_iAttachment )
{
	if(!m_iNewParent) //unlink entity from chain
	{
                    ResetParent();
		return;//disable
          }

	CBaseEntity* pParent;

	if(!m_iAttachment) //try to extract aiment from name
	{
		char *name  = (char*)STRING(m_iNewParent);
		for (char *c = name; *c; c++)
		{
			if (*c == '.')
			{
				m_iAttachment = atoi(c+1);	
				name[strlen(name)-2] = 0;
				pParent = UTIL_FindEntityByTargetname( NULL, name);
				SetParent( pParent, m_iAttachment);
				return;
			}
		}
	}
	pParent = UTIL_FindEntityByTargetname( NULL, STRING(m_iNewParent));	
	SetParent( pParent, m_iAttachment );
}

//=======================================================================
//		set parent main function
//=======================================================================
void CBaseEntity :: SetParent( CBaseEntity* pParent, int m_iAttachment )
{
	m_pParent = pParent;

	if(!m_pParent)
	{
		Msg("=========/Xash Parent System Info:/=========\n");
		if( pev->targetname )
			ALERT( at_warning, "Not found parent for %s with name %s\n", STRING(pev->classname), STRING(pev->targetname) );
		else ALERT( at_warning, "Not found parent for %s\n", STRING(pev->classname) );
		SHIFT;
		ResetParent();//lose parent or not found parent
		return;
	}

	//check for himself parent
	if(m_pParent == this) 
	{
		ALERT(at_console, "=========/Xash Parent System Info:/=========\n");
		if(pev->targetname) Msg( "ERROR! %s with name %s has illegal parent\n", STRING(pev->classname), STRING(pev->targetname) );
		else		Msg( "ERROR! %s has illegal parent\n", STRING(pev->classname) );
		SHIFT;
		ResetParent();//clear parent
		return;
	}
	
	CBaseEntity *pCurChild = m_pParent->m_pChild;
	while (pCurChild) //check that this entity isn't already in the list of children
	{
		if (pCurChild == this) break;
		pCurChild = pCurChild->m_pNextChild;
	}
	if(!pCurChild)
	{
		m_pNextChild = m_pParent->m_pChild; // may be null: that's fine by me.
		m_pParent->m_pChild = this;

          	if( m_iAttachment )
          	{
			if( pev->flags & FL_POINTENTITY || pev->flags & FL_MONSTER )
			{         
				pev->colormap = ((pev->colormap & 0xFF00)>>8) | m_iAttachment;
				pev->aiment = m_pParent->edict();
				pev->movetype = MOVETYPE_FOLLOW;
			}
			else //error
			{
				ALERT(at_console, "=========/Xash Parent System Info:/=========\n");
				if( pev->targetname )
					ALERT( at_error, "%s with name %s not following with aiment %d!(yet)\n", STRING(pev->classname), STRING(pev->targetname), m_iAttachment );
				else ALERT( at_error, "%s not following with aiment %d!(yet)\n", STRING(pev->classname), m_iAttachment );
				SHIFT;
			} 
			return;
		}
		else//appllayed to origin
		{
			if (pev->movetype == MOVETYPE_NONE)
			{
				if (pev->solid == SOLID_BSP)
					pev->movetype = MOVETYPE_PUSH;
				else	pev->movetype = MOVETYPE_NOCLIP;
				SetBits (pFlags, PF_MOVENONE);//member movetype
			}
			if(m_pParent->pev->movetype == MOVETYPE_WALK)//parent is walking monster?
			{
				SetBits (pFlags, PF_POSTORG);//copy pos from parent every frame
				pev->solid = SOLID_NOT;//set non solid
			}
			pParentOrigin = m_pParent->pev->origin;
			pParentAngles = m_pParent->pev->angles;
		}

		if (m_pParent->m_vecSpawnOffset != g_vecZero)
		{
			UTIL_AssignOrigin(this, pev->origin + m_pParent->m_vecSpawnOffset);
			m_vecSpawnOffset = m_vecSpawnOffset + m_pParent->m_vecSpawnOffset;
		}
		OffsetOrigin = pev->origin - pParentOrigin;
		OffsetAngles = pev->angles - pParentAngles;

		if((m_pParent->pFlags & PF_ANGULAR && OffsetOrigin != g_vecZero) || m_pParent->pev->flags & FL_POINTENTITY)
		{
			SetBits (pFlags, PF_POSTORG);//magic stuff
			//GetPInfo( this );
		}
		
		if(g_serveractive)//maybe parent is moving ?
		{
			pev->velocity += m_pParent->pev->velocity;
			pev->avelocity += m_pParent->pev->avelocity;
		}
	}
}

//=======================================================================
//	reset parent (void ) dinamically unlink parents
//=======================================================================
void CBaseEntity :: ResetParent( void )
{
	if( pFlags & PF_MOVENONE ) // this entity was static e.g. func_wall
	{
		ClearBits( pFlags, PF_MOVENONE );
		pev->movetype = MOVETYPE_NONE;
	}

	if( !g_pWorld ) return; //???

	CBaseEntity* pTemp;

	for( pTemp = g_pWorld; pTemp->m_pLinkList != NULL; pTemp = pTemp->m_pLinkList )
	{
		if( this == pTemp->m_pLinkList )
		{
			pTemp->m_pLinkList = this->m_pLinkList; // save pointer
			this->m_pLinkList = NULL;
			break;
		}
	}

	if (m_pParent)
	{
		pTemp = m_pParent->m_pChild;

		if (pTemp == this)m_pParent->m_pChild = this->m_pNextChild;
		else
		{
			while (pTemp->m_pNextChild)
			{
				if (pTemp->m_pNextChild == this)
				{
					pTemp->m_pNextChild = this->m_pNextChild;
					break;
				}
				pTemp = pTemp->m_pNextChild;
			}
		}
	}

	if (m_pNextChild)
	{
		CBaseEntity* pCur = m_pChild;
		CBaseEntity* pNext;
		while (pCur != NULL)
		{
			pNext = pCur->m_pNextChild;
			//bring children to a stop
			UTIL_SetChildVelocity (pCur, g_vecZero, MAX_CHILDS);
			UTIL_SetChildAvelocity(pCur, g_vecZero, MAX_CHILDS);
			pCur->m_pParent = NULL;
			pCur->m_pNextChild = NULL;
			pCur = pNext;
		}
	}
}

//=======================================================================
//		setup physics (execute once at spawn)
//=======================================================================
void CBaseEntity :: SetupPhysics( void )
{
	//rebuild all parents
	if( pFlags & PF_LINKCHILD ) LinkChild( this );

	if( m_physinit ) return;
	SetParent(); //set all parents
	m_physinit = true;
	PostSpawn();//post spawn
}

void CBaseEntity :: RestorePhysics( void )
{
	if(m_iParent) SetParent();
}

void CBaseEntity :: ClearPointers( void )
{
	m_pChild = NULL;
	m_pNextChild = NULL;
	m_pLinkList = NULL;
}

//=========================================================
// FVisible - returns true if a line can be traced from
// the caller's eyes to the target vector
//=========================================================
BOOL CBaseEntity :: FVisible ( const Vector &vecOrigin )
{
	TraceResult	tr;
	Vector		vecLookerOrigin;
	
	vecLookerOrigin = EyePosition(); // look through the caller's 'eyes'

	UTIL_TraceLine( vecLookerOrigin, vecOrigin, ignore_monsters, ignore_glass, ENT( pev ), &tr );
	
	if (tr.flFraction != 1.0)
		return FALSE;
	return TRUE;// line of sight is valid.
}

//=========================================================
// FVisible - returns true if a line can be traced from
// the caller's eyes to the target
//=========================================================
BOOL CBaseEntity :: FVisible( CBaseEntity *pEntity )
{
	TraceResult	tr;
	Vector		vecLookerOrigin;
	Vector		vecTargetOrigin;
	
	if( FBitSet( pEntity->pev->flags, FL_NOTARGET ))
		return FALSE;

	// don't look through water
	if(( pev->waterlevel != 3 && pEntity->pev->waterlevel == 3 ) || ( pev->waterlevel == 3 && pEntity->pev->waterlevel != 3 ))
		return FALSE;

	vecLookerOrigin = pev->origin + pev->view_ofs; // look through the caller's 'eyes'
	vecTargetOrigin = pEntity->EyePosition();

	UTIL_TraceLine( vecLookerOrigin, vecTargetOrigin, ignore_monsters, ignore_glass, ENT( pev ), &tr );
	
	if( tr.flFraction != 1.0 && tr.pHit != ENT( pEntity->pev ))
		return FALSE;
	return TRUE;
}

//=======================================================================
//			fire bullets
//=======================================================================
void CBaseEntity::FireBullets(ULONG cShots, Vector vecSrc, Vector vecDirShooting, Vector vecSpread, float flDistance, int iBulletType, int iTracerFreq, int iDamage, entvars_t *pevAttacker )
{
	static int tracerCount;
	int tracer;
	TraceResult tr;
	Vector vecRight = gpGlobals->v_right;
	Vector vecUp = gpGlobals->v_up;

	if ( pevAttacker == NULL )
		pevAttacker = pev;  // the default attacker is ourselves

	ClearMultiDamage();
	gMultiDamage.type = DMG_BULLET | DMG_NEVERGIB;

	for (ULONG iShot = 1; iShot <= cShots; iShot++)
	{
		// get circular gaussian spread
		float x, y, z;
		do {
			x = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
			y = RANDOM_FLOAT(-0.5,0.5) + RANDOM_FLOAT(-0.5,0.5);
			z = x*x+y*y;
		} while (z > 1);

		Vector vecDir = vecDirShooting +
						x * vecSpread.x * vecRight +
						y * vecSpread.y * vecUp;
		Vector vecEnd;

		vecEnd = vecSrc + vecDir * flDistance;
		UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, ENT(pev)/*pentIgnore*/, &tr);

		tracer = 0;
		if (iTracerFreq != 0 && (tracerCount++ % iTracerFreq) == 0)
		{
			Vector vecTracerSrc;

			if ( IsPlayer() )
			{// adjust tracer position for player
				vecTracerSrc = vecSrc + Vector ( 0 , 0 , -4 ) + gpGlobals->v_right * 2 + gpGlobals->v_forward * 16;
			}
			else
			{
				vecTracerSrc = vecSrc;
			}
			
			if ( iTracerFreq != 1 )		// guns that always trace also always decal
				tracer = 1;
			switch( iBulletType )
			{
			case BULLET_MP5:
			case BULLET_9MM:
			case BULLET_12MM:
			case BULLET_357:
			case BULLET_556:
			case BULLET_762:
			case BULLET_BUCKSHOT:
			default:
				MESSAGE_BEGIN( MSG_PAS, gmsg.TempEntity, vecTracerSrc );
					WRITE_BYTE( TE_TRACER );
					WRITE_COORD( vecTracerSrc.x );
					WRITE_COORD( vecTracerSrc.y );
					WRITE_COORD( vecTracerSrc.z );
					WRITE_COORD( tr.vecEndPos.x );
					WRITE_COORD( tr.vecEndPos.y );
					WRITE_COORD( tr.vecEndPos.z );
				MESSAGE_END();
				break;
			}
		}
		// do damage, paint decals
		if (tr.flFraction != 1.0)
		{
			CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

			if ( iDamage )
			{
				pEntity->TraceAttack(pevAttacker, iDamage, vecDir, &tr, DMG_BULLET | ((iDamage > 16) ? DMG_ALWAYSGIB : DMG_NEVERGIB) );
				
				TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
				DecalGunshot( &tr, iBulletType );
			} 
			else switch(iBulletType)
			{
			default:
			case BULLET_9MM:
				pEntity->TraceAttack(pevAttacker, _9MM_DMG, vecDir, &tr, DMG_BULLET);
				
				TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
				DecalGunshot( &tr, iBulletType );

				break;

			case BULLET_MP5:
				pEntity->TraceAttack(pevAttacker, _MP5_DMG, vecDir, &tr, DMG_BULLET);
				
				TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
				DecalGunshot( &tr, iBulletType );

				break;

			case BULLET_12MM:		
				pEntity->TraceAttack(pevAttacker, _12MM_DMG, vecDir, &tr, DMG_BULLET); 
				if ( !tracer )
				{
					TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
					DecalGunshot( &tr, iBulletType );
				}
				break;
				
			case BULLET_556:		
				pEntity->TraceAttack(pevAttacker, _556_DMG, vecDir, &tr, DMG_BULLET); 
				if ( !tracer )
				{
					TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
					DecalGunshot( &tr, iBulletType );
				}
				break;
				
			case BULLET_762:		
				pEntity->TraceAttack(pevAttacker, _762_DMG, vecDir, &tr, DMG_BULLET); 
				if ( !tracer )
				{
					TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
					DecalGunshot( &tr, iBulletType );
				}
				break;
				
			case BULLET_BUCKSHOT:		
				pEntity->TraceAttack(pevAttacker, BUCKSHOT_DMG, vecDir, &tr, DMG_BULLET); 
				if ( !tracer )
				{
					TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
					DecalGunshot( &tr, iBulletType );
				}
				break;
															
			case BULLET_357:
				pEntity->TraceAttack(pevAttacker, _357_DMG, vecDir, &tr, DMG_BULLET);
				if ( !tracer )
				{
					TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
					DecalGunshot( &tr, iBulletType );
				}
				break;

			case BULLET_NONE: // FIX 
				pEntity->TraceAttack(pevAttacker, 50, vecDir, &tr, DMG_CLUB);
				TEXTURETYPE_PlaySound(&tr, vecSrc, vecEnd, iBulletType);
				// only decal glass
				if ( !FNullEnt(tr.pHit) && VARS(tr.pHit)->rendermode != 0)
				{
					UTIL_DecalTrace( &tr, DECAL_GLASSBREAK1 + RANDOM_LONG(0,2) );
				}

				break;
			}
		}
		// make bullet trails
		UTIL_BubbleTrail( vecSrc, tr.vecEndPos, (flDistance * tr.flFraction) / 64.0 );
	}
	ApplyMultiDamage(pev, pevAttacker);
}

//=======================================================================
//			traceing operations
//=======================================================================
void CBaseEntity :: TraceBleed( float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if (BloodColor() == DONT_BLEED)
		return;
	
	if (flDamage == 0)
		return;

	if (!(bitsDamageType & (DMG_CRUSH | DMG_BULLET | DMG_SLASH | DMG_BLAST | DMG_CLUB | DMG_MORTAR)))
		return;
	
	// make blood decal on the wall! 
	TraceResult Bloodtr;
	Vector vecTraceDir; 
	float flNoise;
	int cCount;
	int i;

	if (flDamage < 10)
	{
		flNoise = 0.1;
		cCount = 1;
	}
	else if (flDamage < 25)
	{
		flNoise = 0.2;
		cCount = 2;
	}
	else
	{
		flNoise = 0.3;
		cCount = 4;
	}

	for ( i = 0 ; i < cCount ; i++ )
	{
		vecTraceDir = vecDir * -1;// trace in the opposite direction the shot came from (the direction the shot is going)

		vecTraceDir.x += RANDOM_FLOAT( -flNoise, flNoise );
		vecTraceDir.y += RANDOM_FLOAT( -flNoise, flNoise );
		vecTraceDir.z += RANDOM_FLOAT( -flNoise, flNoise );

		UTIL_TraceLine( ptr->vecEndPos, ptr->vecEndPos + vecTraceDir * -172, ignore_monsters, ENT(pev), &Bloodtr);

		if ( Bloodtr.flFraction != 1.0 )
		{
			UTIL_BloodDecalTrace( &Bloodtr, BloodColor() );
		}
	}
}

void CBaseEntity::TraceAttack(entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	Vector vecOrigin = ptr->vecEndPos - vecDir * 4;

	if ( pev->takedamage )
	{
		AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );

		int blood = BloodColor();
		
		if ( blood != DONT_BLEED )
		{
			SpawnBlood(vecOrigin, blood, flDamage);// a little surface blood.
			TraceBleed( flDamage, vecDir, ptr, bitsDamageType );
		}
	}
}

//=======================================================================
//			take damage\health
//=======================================================================
int CBaseEntity :: TakeHealth( float flHealth, int bitsDamageType )
{
	if(!pev->takedamage) return 0;
	if ( pev->health >= pev->max_health ) return 0;

	pev->health += flHealth;
	pev->health = min(pev->health, pev->max_health);

	return 1;
}

int CBaseEntity :: TakeArmor( float flArmor, int suit )
{
	if(!pev->takedamage) return 0;
	if (pev->armorvalue >= MAX_NORMAL_BATTERY) return 0;

	pev->armorvalue += flArmor;
	pev->armorvalue = min(pev->armorvalue, MAX_NORMAL_BATTERY);

	return 1;
}

int CBaseEntity :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	Vector	vecTemp;

	if (!pev->takedamage) return 0;

	// if Attacker == Inflictor, the attack was a melee or other instant-hit attack.
	// (that is, no actual entity projectile was involved in the attack so use the shooter's origin). 
	if ( pevAttacker == pevInflictor )	
	{
		vecTemp = pevInflictor->origin - ( VecBModelOrigin(pev) );
	}
	else // an actual missile was involved.
	{
		vecTemp = pevInflictor->origin - ( VecBModelOrigin(pev) );
	}

	// this global is still used for glass and other non-monster killables, along with decals.
	g_vecAttackDir = vecTemp.Normalize();
		
	// save damage based on the target's armor level

	// figure momentum add (don't let hurt brushes or other triggers move player)
	if ((!FNullEnt(pevInflictor)) && (pev->movetype == MOVETYPE_WALK || pev->movetype == MOVETYPE_STEP) && (pevAttacker->solid != SOLID_TRIGGER) )
	{
		Vector vecDir = pev->origin - (pevInflictor->absmin + pevInflictor->absmax) * 0.5;
		vecDir = vecDir.Normalize();

		float flForce = flDamage * ((32 * 32 * 72.0) / (pev->size.x * pev->size.y * pev->size.z)) * 5;
		
		if (flForce > 1000.0) flForce = 1000.0;
		pev->velocity = pev->velocity + vecDir * flForce;
	}

	// do the damage
	pev->health -= flDamage;
	if (pev->health <= 0)
	{
		Killed( pevAttacker, GIB_NORMAL );
		return 0;
	}
	return 1;
}

int CBaseEntity :: DamageDecal( int bitsDamageType )
{
	if ( pev->rendermode == kRenderTransAlpha )
		return -1;

	if ( pev->rendermode != kRenderNormal )
		return DECAL_BPROOF1;

	return DECAL_GUNSHOT1 + RANDOM_LONG(0, 4);
}

//=======================================================================
//			killed/create operations
//=======================================================================
CBaseEntity * CBaseEntity::Create( char *szName, const Vector &vecOrigin, const Vector &vecAngles, edict_t *pentOwner )
{
	edict_t	*pent;
	int istr = ALLOC_STRING(szName);
	CBaseEntity *pEntity;
          
	pent = CREATE_NAMED_ENTITY( istr );
	if( FNullEnt( pent )) return NULL;

	pEntity = Instance( pent );
	pEntity->SetObjectClass( );
	pEntity->pev->owner = pentOwner;
	pEntity->pev->origin = vecOrigin;
	pEntity->pev->angles = vecAngles;
	DispatchSpawn( pEntity->edict() );

	return pEntity;
}

CBaseEntity * CBaseEntity::CreateGib( char *szName, char *szModel )
{
	edict_t	*pent;
	CBaseEntity *pEntity;
          string_t model = MAKE_STRING( szModel );
	int istr = ALLOC_STRING(szName);
          
	pent = CREATE_NAMED_ENTITY( istr );
	if( FNullEnt( pent )) return NULL;
	
	pEntity = Instance( pent );
	DispatchSpawn( pEntity->edict() );
	pEntity->SetObjectClass( ED_NORMAL );

	if( !FStringNull( model )) 
	{
		UTIL_SetModel( pEntity->edict(), szModel );
		Msg( "szModel %s\n", szModel );
	}
	return pEntity;
}

void CBaseEntity :: Killed( entvars_t *pevAttacker, int iGib )
{
	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DEAD;
	UTIL_Remove( this );
}

void CBaseEntity::UpdateOnRemove( void )
{
          ResetParent();

	if ( FBitSet( pev->flags, FL_GRAPHED ) )
	{
		for (int i = 0 ; i < WorldGraph.m_cLinks ; i++ )
		{
			if ( WorldGraph.m_pLinkPool [ i ].m_pLinkEnt == pev )
				WorldGraph.m_pLinkPool [ i ].m_pLinkEnt = NULL;
		}
	}
	if( pev->globalname ) gGlobalState.EntitySetState( pev->globalname, GLOBAL_DEAD );
}

//=======================================================================
//		three methods of remove entity
//=======================================================================
void CBaseEntity :: Remove( void )
{
	UpdateOnRemove();
	if( pev->health > 0 )
		pev->health = 0;
	REMOVE_ENTITY( ENT( pev ));
}

void CBaseEntity :: PVSRemove( void )
{
	if ( FNullEnt( FIND_CLIENT_IN_PVS( edict()))) SetThink( Remove );
	SetNextThink( 0.1 );
}

void CBaseEntity :: Fadeout( void )
{
	if (pev->rendermode == kRenderNormal)
	{
		pev->renderamt = 255;
		pev->rendermode = kRenderTransTexture;
	}

	pev->solid = SOLID_NOT;
	pev->avelocity = g_vecZero;	

	if ( pev->renderamt > 7 )
	{
		pev->renderamt -= 7;
		SetNextThink( 0.1 );
	}
	else 
	{
		pev->renderamt = 0;
		SetNextThink( 0.2 );
		SetThink( Remove );
	}
}

//=======================================================================
//			global save\restore data
//=======================================================================
TYPEDESCRIPTION	CBaseEntity::m_SaveData[] = 
{
	DEFINE_FIELD( CBaseEntity, m_pGoalEnt, FIELD_CLASSPTR ),

	//parent system saves
	DEFINE_FIELD( CBaseEntity, m_iParent, FIELD_STRING ),
	DEFINE_FIELD( CBaseEntity, m_pParent, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBaseEntity, m_pChild, FIELD_CLASSPTR ),	
         	DEFINE_FIELD( CBaseEntity, m_pNextChild, FIELD_CLASSPTR ),	
	DEFINE_FIELD( CBaseEntity, OffsetOrigin, FIELD_VECTOR ), 
	DEFINE_FIELD( CBaseEntity, OffsetAngles, FIELD_VECTOR ),
          DEFINE_FIELD( CBaseEntity, pFlags, FIELD_INTEGER ),
          DEFINE_FIELD( CBaseEntity, m_physinit, FIELD_BOOLEAN ),
          
	//local child coordinates
	DEFINE_FIELD( CBaseEntity, PostOrigin, FIELD_VECTOR ),
	DEFINE_FIELD( CBaseEntity, PostAngles, FIELD_VECTOR ),
	DEFINE_FIELD( CBaseEntity, PostVelocity, FIELD_VECTOR ),
	DEFINE_FIELD( CBaseEntity, PostAvelocity, FIELD_VECTOR ),

	//think time
	DEFINE_FIELD( CBaseEntity, m_fNextThink, FIELD_TIME ),
	DEFINE_FIELD( CBaseEntity, m_fPevNextThink, FIELD_TIME ),
		
	DEFINE_FIELD( CBaseEntity, m_iStyle, FIELD_INTEGER ),
	DEFINE_FIELD( CBaseEntity, m_iClassType, FIELD_INTEGER ),
          DEFINE_FIELD( CBaseEntity, m_iAcessLevel, FIELD_INTEGER ),
	
	DEFINE_FIELD( CBaseEntity, m_pfnThink, FIELD_FUNCTION ),
	DEFINE_FIELD( CBaseEntity, m_pfnTouch, FIELD_FUNCTION ),
	DEFINE_FIELD( CBaseEntity, m_pfnUse, FIELD_FUNCTION ),
	DEFINE_FIELD( CBaseEntity, m_pfnBlocked, FIELD_FUNCTION ),
};

int CBaseEntity::Save( CSave &save )
{
	ThinkCorrection();

	if ( save.WriteEntVars( "ENTVARS", pev ) )
	{
		if (pev->targetname)
			return save.WriteFields( STRING(pev->targetname), "BASE", this, m_SaveData, ARRAYSIZE(m_SaveData) );
		else	return save.WriteFields( STRING(pev->classname ), "BASE", this, m_SaveData, ARRAYSIZE(m_SaveData) );
	}
	return 0;
}

int CBaseEntity :: Restore( CRestore &restore )
{
	int status;

	status = restore.ReadEntVars( "ENTVARS", pev );
	if( status ) status = restore.ReadFields( "BASE", this, m_SaveData, ARRAYSIZE( m_SaveData ));

	// restore edict class here
	SetObjectClass( m_iClassType );

	if( pev->modelindex != 0 && !FStringNull( pev->model ))
	{
		Vector	mins = pev->mins;
		Vector	maxs = pev->maxs; // Set model is about to destroy these
		
		UTIL_PrecacheModel( pev->model );
		UTIL_SetModel( ENT( pev ), pev->model );
		UTIL_SetSize( pev, mins, maxs );
	}

	if( pev->soundindex != 0 && !FStringNull( pev->noise ))
	{
		UTIL_PrecacheSound( pev->noise );
		LINK_ENTITY( ENT( pev ), false );
	}

	return status;
}

//=======================================================================
//		collisoin boxes
//=======================================================================
void CBaseEntity::SetObjectCollisionBox( void )
{
	::SetObjectCollisionBox( pev );
}

int CBaseEntity :: Intersects( CBaseEntity *pOther )
{
	if ( pOther->pev->absmin.x > pev->absmax.x ||
	     pOther->pev->absmin.y > pev->absmax.y ||
	     pOther->pev->absmin.z > pev->absmax.z ||
	     pOther->pev->absmax.x < pev->absmin.x ||
	     pOther->pev->absmax.y < pev->absmin.y ||
	     pOther->pev->absmax.z < pev->absmin.z )
		return FALSE;
	return TRUE;
}

//=======================================================================
//		Dormant operations
//=======================================================================
void CBaseEntity :: MakeDormant( void )
{
	SetBits( pev->flags, FL_DORMANT );
	
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	SetBits( pev->effects, EF_NODRAW );
	DontThink();
	UTIL_SetOrigin( this, pev->origin );
}

int CBaseEntity :: IsDormant( void )
{
	return FBitSet( pev->flags, FL_DORMANT );
}

BOOL CBaseEntity :: IsInWorld( void )
{
	if (pev->origin.x >=  MAP_HALFSIZE) return FALSE;
	if (pev->origin.y >=  MAP_HALFSIZE) return FALSE;
	if (pev->origin.z >=  MAP_HALFSIZE) return FALSE;
	if (pev->origin.x <= -MAP_HALFSIZE) return FALSE;
	if (pev->origin.y <= -MAP_HALFSIZE) return FALSE;
	if (pev->origin.z <= -MAP_HALFSIZE) return FALSE;
	if (pev->velocity.x >=  MAX_VELOCITY) return FALSE;
	if (pev->velocity.y >=  MAX_VELOCITY) return FALSE;
	if (pev->velocity.z >=  MAX_VELOCITY) return FALSE;
	if (pev->velocity.x <= -MAX_VELOCITY) return FALSE;
	if (pev->velocity.y <= -MAX_VELOCITY) return FALSE;
	if (pev->velocity.z <= -MAX_VELOCITY) return FALSE;

	return TRUE;
}