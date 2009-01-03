//=======================================================================
//			Copyright (C) Shambler Team 2004
//		         item_.cpp - items entities: suit, 
//			       helmet, lighter, etc			    
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "cbase.h"
#include "saverestore.h"
#include "baseweapon.h"
#include "client.h"
#include "player.h"
#include "gamerules.h"
#include "defaults.h"

extern int gEvilImpulse101;

//***********************************************************
// 		   main functions ()
//***********************************************************

void CItem::Spawn( void )
{
	Precache();
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;

	UTIL_SetOrigin( this, pev->origin );
	UTIL_SetSize(pev, g_vecZero, g_vecZero );

	SetTouch( ItemTouch );
          SetThink( ItemFall );

	UTIL_SetModel(ENT(pev), pev->model, (char *)Model() );
	SetNextThink( 0.1 );
}

void CItem::Precache( void )
{
	UTIL_PrecacheModel( pev->model, (char *)Model() );
	UTIL_PrecacheSound((char *)PickSound());
	UTIL_PrecacheSound((char *)FallSound());
}

void CItem::ItemTouch( CBaseEntity *pOther )
{
	//removed this limitation for monsters
	if ( !pOther->IsPlayer() ) return;
	CBasePlayer *pPlayer = (CBasePlayer *)pOther;
	if (!UTIL_IsMasterTriggered(m_sMaster, pPlayer)) return;
	if (pPlayer->pev->deadflag != DEAD_NO) return;

	if (AddItem( pPlayer ))
	{
		UTIL_FireTargets( pev->target, pOther, this, USE_TOGGLE );
		SetTouch( NULL );
                  
		if (IsItem() && gmsg.ItemPickup)
		{
			//show icon for item
			MESSAGE_BEGIN( MSG_ONE, gmsg.ItemPickup, NULL, pPlayer->pev );
				WRITE_STRING( STRING(pev->classname) );
			MESSAGE_END();
		}
		
		//play pickup sound
		EMIT_SOUND( pPlayer->edict(), CHAN_ITEM, (char *)PickSound(), 1, ATTN_NORM );
		
		// tell the owner item was taken
 		if (!FNullEnt( pev->owner )) pev->owner->v.impulse = 0;
		
		//enable respawn in multiplayer
		if ( g_pGameRules->IsMultiplayer()) Respawn(); 
		else UTIL_Remove( this );
	}
	else if (gEvilImpulse101) UTIL_Remove( this );
}

void CItem::ItemFall ( void )
{
	SetNextThink( 0.1 );

	if ( pev->flags & FL_ONGROUND )
	{
		// clatter if we have an owner (i.e., dropped by someone)
		// don't clatter if the item is waiting to respawn (if it's waiting, it is invisible!)
		if ( !FNullEnt( pev->owner )) 
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, (char *)FallSound(), 1, ATTN_NORM);
                    	ItemOnGround(); //do somewhat if needed
		}
		
		// lie flat
		pev->angles.x = 0;
		pev->angles.z = 0;

		pev->solid = SOLID_TRIGGER;

		if (IsAmmo()) UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));
		if (IsItem()) UTIL_SetSize ( pev, Vector ( -8, -8, 0 ), Vector ( 8, 8, 8 ) );
		else UTIL_AutoSetSize();
		
		UTIL_SetOrigin( this, pev->origin );// link into world.

		SetTouch( ItemTouch );
		SetThink (NULL); 
	}
}

CBaseEntity* CItem::Respawn( void )
{
	SetTouch( NULL );
	pev->effects |= EF_NODRAW;

	SetThink( Materialize );
	AbsoluteNextThink( ItemRespawnTime( this ) );
	return this;
}

float CItem::ItemRespawnTime( CItem *pItem )
{
	//makes different time to respawn for weapons and items
	float flRespawnTime;

	if (IsAmmo()) flRespawnTime = RESPAWN_TIME_30SEC;
	if (IsItem()) flRespawnTime = RESPAWN_TIME_120SEC;
	return gpGlobals->time + flRespawnTime;
}


void CItem::Materialize( void )
{
	if ( pev->effects & EF_NODRAW )
	{
		// changing from invisible state to visible.
		pev->effects &= ~EF_NODRAW;
		pev->renderfx = kRenderFxGlowShell;
		pev->renderamt = 40;
          	pev->frags = 0;
          	pev->rendercolor.x = RANDOM_LONG(25, 255);
          	pev->rendercolor.y = RANDOM_LONG(25, 255);
          	pev->rendercolor.z = RANDOM_LONG(25, 255);
		pev->scale = 0.01;
		SetNextThink (0.001);
	}
	if( pev->scale > 1.2 ) pev->frags = 1;
	if ( pev->frags == 1 )
	{         //set effects for respawn item
		if(pev->scale > 1.0) pev->scale -= 0.05;
		else
		{
			pev->renderfx = kRenderFxNone;
			pev->frags = 0;
			SetTouch( ItemTouch );
			EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "items/respawn.wav", 1, ATTN_NORM, 0, 150 );
			SetThink( NULL );
			DontThink();
		} 
	}
	else pev->scale += 0.05;
	SetNextThink (0.001);
	
}

//***********************************************************
// 		      generic item
//***********************************************************
class CGenericItem : public CItem
{
	const char *Model( void ){ return "models/w_adrenaline.mdl"; }
	BOOL AddItem( CBaseEntity *pOther ) 
	{ 
		CBasePlayer *pPlayer = (CBasePlayer *)pOther;
		if (pPlayer->pev->deadflag != DEAD_NO) return FALSE;
		if (pOther->GiveAmmo( AMMO_GLOCKCLIP_GIVE, "9mm", _9MM_MAX_CARRY ) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "weapons/glock/clip_in.wav", 1, ATTN_NORM);
			return TRUE;
		}
		MESSAGE_BEGIN( MSG_ONE, gmsg.ItemPickup, NULL, pPlayer->pev );
			WRITE_STRING( STRING(pev->classname) );
		MESSAGE_END();
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( item_generic, CGenericItem );

//***********************************************************
// 		   	items
//***********************************************************
class CItemSuit : public CItem
{
	const char *Model( void ){ return  "models/w_suit.mdl"; }
	BOOL AddItem( CBaseEntity *pOther )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)pOther;
		if ( pPlayer->pev->deadflag != DEAD_NO ) return FALSE;

		if ( pPlayer->m_iHideHUD & ITEM_SUIT )
			return FALSE;

		if ( pev->spawnflags & 1 )//SF_SUIT_SHORTLOGON
			EMIT_SOUND_SUIT(pPlayer->edict(), "!HEV_A0");	// short version of suit logon,
		else	EMIT_SOUND_SUIT(pPlayer->edict(), "!HEV_AAx");	// long version of suit logon
                    
		pPlayer->m_iHideHUD |= ITEM_SUIT;
		return TRUE;
	}
};
LINK_ENTITY_TO_CLASS(item_suit, CItemSuit);

class CItemLongJump : public CItem
{
	const char *Model( void ){ return  "models/w_longjump.mdl"; }
	const char *PickSound( void ){ return "buttons/bell1.wav"; }
	BOOL AddItem( CBaseEntity *pOther )
	{
		CBasePlayer *pPlayer = (CBasePlayer *)pOther;
		if ( pPlayer->m_iHideHUD & ITEM_SUIT && !pPlayer->m_fLongJump)
		{
			pPlayer->m_fLongJump = TRUE;// player now has longjump module
			EMIT_SOUND_SUIT( pPlayer->edict(), "!HEV_A1" );
			return TRUE;		
		}
		return FALSE;
	}
};
LINK_ENTITY_TO_CLASS( item_longjump, CItemLongJump );


class CItemBattery : public CItem
{
	const char *Model( void ){ return "models/items/w_battery.mdl"; }
	const char *PickSound( void ){ return "items/gunpickup2.wav"; }
	BOOL AddItem( CBaseEntity *pOther ) { return pOther->TakeArmor( BATTERY_CHARGE, TRUE ); }
};
LINK_ENTITY_TO_CLASS(item_battery, CItemBattery);

class CHealthKit : public CItem
{
	const char *Model( void ){ return "models/items/w_medkit.mdl"; }
	const char *PickSound( void ){ return "items/smallmedkit1.wav"; }
	BOOL AddItem( CBaseEntity *pOther ) { return pOther->TakeHealth( MEDKIT_CAP, DMG_GENERIC ); }
};
LINK_ENTITY_TO_CLASS( item_healthkit, CHealthKit );

class CItemSoda : public CItem
{
public:
	const char *Model( void ){ return "models/can.mdl"; }
	const char *FallSound( void ){ return "weapons/g_bounce3.wav"; }
	BOOL AddItem( CBaseEntity *pOther ) { pOther->TakeHealth( pev->skin * 1, DMG_GENERIC ); return 1; }
};
LINK_ENTITY_TO_CLASS( item_sodacan, CItemSoda );

class CItemSecurity : public CItem
{
	const char *Model( void ){ return  "models/w_security.mdl"; }
	const char *PickSound( void ){ return "items/gunpickup2.wav"; }
	BOOL AddItem( CBaseEntity *pOther ) { return TRUE; }
};
LINK_ENTITY_TO_CLASS(item_security, CItemSecurity);

class CItemArmorVest : public CItem
{
	const char *Model( void ){ return  "models/w_vest.mdl"; }
	const char *PickSound( void ){ return "items/gunpickup2.wav"; }
	BOOL AddItem( CBaseEntity *pOther ) { return pOther->TakeArmor( 60 ); }
};
LINK_ENTITY_TO_CLASS(item_armorvest, CItemArmorVest);

class CItemHelmet : public CItem
{
	const char *Model( void ){ return  "models/w_helmet.mdl"; }
	const char *PickSound( void ){ return "items/gunpickup2.wav"; }
	BOOL AddItem( CBaseEntity *pOther ) { return pOther->TakeArmor( 40 ); }
};
LINK_ENTITY_TO_CLASS(item_helmet, CItemHelmet);

//***********************************************************
// 		   	ammo
//***********************************************************
class CGlockAmmo : public CItem
{
	const char *Model( void ){ return "models/w_9mmclip.mdl"; }
	const char *PickSound( void ){ return "weapons/glock/clip_in.wav"; }
	BOOL AddItem( CBaseEntity *pOther ) { return pOther->GiveAmmo( AMMO_GLOCKCLIP_GIVE, "9mm", 250 ); }
};
LINK_ENTITY_TO_CLASS( ammo_9mmclip, CGlockAmmo );
LINK_ENTITY_TO_CLASS( ammo_glockclip, CGlockAmmo );

class CPythonAmmo : public CItem
{
	const char *Model( void ){ return "models/w_357ammobox.mdl"; }
	const char *PickSound( void ){ return "weapons/glock/clip_in.wav"; }
	BOOL AddItem( CBaseEntity *pOther ) { return pOther->GiveAmmo( AMMO_357BOX_GIVE, "357", 21 ); }
};
LINK_ENTITY_TO_CLASS( ammo_357, CPythonAmmo );

class CSniperAmmo : public CItem
{
	const char *Model( void ){ return "models/w_m40a1clip.mdl"; }
	const char *PickSound( void ){ return "weapons/glock/clip_in.wav"; }
	BOOL AddItem( CBaseEntity *pOther ) { return pOther->GiveAmmo( AMMO_357BOX_GIVE, "762", 21 ); }
};
LINK_ENTITY_TO_CLASS( ammo_762, CSniperAmmo );

class CRpgAmmo : public CItem
{
	const char *Model( void ){ return "models/ammo/w_rpgammo.mdl"; }
	const char *PickSound( void ){ return "weapons/glock/clip_in.wav"; }
	BOOL AddItem( CBaseEntity *pOther ) { return pOther->GiveAmmo( AMMO_RPGCLIP_GIVE, "rockets", 3 ); }
};
LINK_ENTITY_TO_CLASS( ammo_rpgclip, CRpgAmmo );

class CMP5AmmoClip : public CItem
{
	const char *Model( void ){ return "models/w_9mmARclip.mdl"; }
	const char *PickSound( void ){ return "weapons/glock/clip_in.wav"; }
	BOOL AddItem( CBaseEntity *pOther ) { return pOther->GiveAmmo( AMMO_MP5CLIP_GIVE, "9mm", 250); }
};
LINK_ENTITY_TO_CLASS( ammo_9mmAR, CMP5AmmoClip );
LINK_ENTITY_TO_CLASS( ammo_mp5clip, CMP5AmmoClip );

class CSawAmmo : public CItem
{
	const char *Model( void ){ return "models/w_saw_clip.mdl"; }
	const char *PickSound( void ){ return "weapons/glock/clip_in.wav"; }
	BOOL AddItem( CBaseEntity *pOther ) { return pOther->GiveAmmo( AMMO_CHAINBOX_GIVE, "556", SAW_MAX_CLIP); }
};
LINK_ENTITY_TO_CLASS( ammo_556, CSawAmmo );

class CMP5AmmoGrenade : public CItem
{
	const char *Model( void ){ return "models/ammo/w_argrenade.mdl"; }
	const char *PickSound( void ){ return "weapons/glock/clip_in.wav"; }
	BOOL AddItem( CBaseEntity *pOther ) { return pOther->GiveAmmo( AMMO_M203BOX_GIVE, "m203", 10 ); }
};
LINK_ENTITY_TO_CLASS( ammo_m203, CMP5AmmoGrenade );

class CGaussAmmo : public CItem
{
	const char *Model( void ){ return "models/w_gaussammo.mdl"; }
	const char *PickSound( void ){ return "weapons/glock/clip_in.wav"; }
	BOOL AddItem( CBaseEntity *pOther ) { return pOther->GiveAmmo( AMMO_URANIUMBOX_GIVE, "uranium", 100 ); }
};
LINK_ENTITY_TO_CLASS( ammo_gaussclip, CGaussAmmo );

class CShotgunAmmoBox : public CItem
{
	const char *Model( void ){ return "models/w_shotbox.mdl"; }
	const char *PickSound( void ){ return "weapons/glock/clip_in.wav"; }
	BOOL AddItem( CBaseEntity *pOther ) { return pOther->GiveAmmo( 12, "buckshot", 125 ); }
};
LINK_ENTITY_TO_CLASS( ammo_buckshot, CShotgunAmmoBox );

class CCrossbowAmmo : public CItem
{
	const char *Model( void ){ return "models/w_crossbow_clip.mdl"; }
	const char *PickSound( void ){ return "weapons/glock/clip_in.wav"; }
	BOOL AddItem( CBaseEntity *pOther ) { return pOther->GiveAmmo( 5, "bolts", 50 ); }
};
LINK_ENTITY_TO_CLASS( ammo_crossbow, CCrossbowAmmo );

class CShotgunAmmoShell : public CItem
{
	const char *Model( void ){ return "models/shellBuck.mdl"; }
	const char *PickSound( void ){ return "weapons/glock/clip_in.wav"; }
	BOOL AddItem( CBaseEntity *pOther ) { return pOther->GiveAmmo( 1, "buckshot", 125 ); }
};
LINK_ENTITY_TO_CLASS( ammo_buckshell, CShotgunAmmoShell );

//***********************************************************
//   item_weaponbox - a single entity that can store weapons
//			and ammo. 
//***********************************************************
LINK_ENTITY_TO_CLASS( item_weaponbox, CWeaponBox );

TYPEDESCRIPTION	CWeaponBox::m_SaveData[] = 
{
	DEFINE_ARRAY( CWeaponBox, m_rgAmmo, FIELD_INTEGER, MAX_AMMO_SLOTS ),
	DEFINE_ARRAY( CWeaponBox, m_rgiszAmmo, FIELD_STRING, MAX_AMMO_SLOTS ),
	DEFINE_ARRAY( CWeaponBox, m_rgpPlayerItems, FIELD_CLASSPTR, MAX_ITEM_TYPES ),
	DEFINE_FIELD( CWeaponBox, m_cAmmoTypes, FIELD_INTEGER ),
}; IMPLEMENT_SAVERESTORE( CWeaponBox, CBaseEntity );

void CWeaponBox::Precache( void )
{
	UTIL_PrecacheModel("models/w_weaponbox.mdl");
}

void CWeaponBox :: KeyValue( KeyValueData *pkvd )
{
	if ( m_cAmmoTypes < MAX_AMMO_SLOTS )
	{
		PackAmmo( ALLOC_STRING(pkvd->szKeyName), atoi(pkvd->szValue) );
		m_cAmmoTypes++;// count this new ammo type.

		pkvd->fHandled = TRUE;
	}
	else Msg( "WeaponBox too full! only %d ammotypes allowed\n", MAX_AMMO_SLOTS );
}

void CWeaponBox::Spawn( void )
{
	Precache( );

	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_TRIGGER;

	UTIL_SetSize( pev, g_vecZero, g_vecZero );
	UTIL_SetModel( ENT(pev), "models/w_weaponbox.mdl");
}

void CWeaponBox::Kill( void )
{
	CBasePlayerWeapon *pWeapon;
	int i;

	// destroy the weapons
	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		pWeapon = m_rgpPlayerItems[ i ];

		while ( pWeapon )
		{
			pWeapon->SetThink( Remove );
			pWeapon->SetNextThink( 0.1 );
			pWeapon = pWeapon->m_pNext;
		}
	}

	// remove the box
	UTIL_Remove( this );
}

void CWeaponBox::Touch( CBaseEntity *pOther )
{
	if ( !(pev->flags & FL_ONGROUND ) ) return;
	if ( !pOther->IsPlayer() ) return;
	if ( !pOther->IsAlive() ) return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;
	int i;

	// dole out ammo
	for ( i = 0 ; i < MAX_AMMO_SLOTS ; i++ )
	{
		if ( !FStringNull( m_rgiszAmmo[ i ] ) )
		{
			// there's some ammo of this type. 
			pPlayer->GiveAmmo( m_rgAmmo[ i ], (char *)STRING( m_rgiszAmmo[ i ] ), MaxAmmoCarry( m_rgiszAmmo[ i ] ) );

			// now empty the ammo from the weaponbox since we just gave it to the player
			m_rgiszAmmo[ i ] = iStringNull;
			m_rgAmmo[ i ] = 0;
		}
	}

	// go through my weapons and try to give the usable ones to the player. 
	// it's important the the player be given ammo first, so the weapons code doesn't refuse 
	// to deploy a better weapon that the player may pick up because he has no ammo for it.

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( m_rgpPlayerItems[ i ] )
		{
			CBasePlayerWeapon *pItem;

			// have at least one weapon in this slot
			while ( m_rgpPlayerItems[ i ] )
			{
				//ALERT ( at_console, "trying to give %s\n", STRING( m_rgpPlayerItems[ i ]->pev->classname ) );

				pItem = m_rgpPlayerItems[ i ];
				m_rgpPlayerItems[ i ] = m_rgpPlayerItems[ i ]->m_pNext;// unlink this weapon from the box

				if ( pPlayer->AddPlayerItem( pItem ) ) pItem->AttachToPlayer( pPlayer );
			}
		}
	}

	EMIT_SOUND( pOther->edict(), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM );
	SetTouch(NULL);
	UTIL_Remove(this);
}

int CWeaponBox::MaxAmmoCarry( int iszName )
{
	for( int i = 0;  i < MAX_WEAPONS; i++ )
	{
		if( CBasePlayerWeapon::ItemInfoArray[i].iszAmmo1 == iszName )
			return CBasePlayerWeapon::ItemInfoArray[i].iMaxAmmo1;
		if( CBasePlayerWeapon::ItemInfoArray[i].iszAmmo2 == iszName )
			return CBasePlayerWeapon::ItemInfoArray[i].iMaxAmmo2;
	}

	ALERT( at_error, "MaxAmmoCarry() doesn't recognize '%s'!\n", STRING( iszName ));
	return -1;
}

BOOL CWeaponBox::PackWeapon( CBasePlayerWeapon *pWeapon )
{
	// is one of these weapons already packed in this box?
	if ( HasWeapon( pWeapon ) ) return FALSE;// box can only hold one of each weapon type

	if ( pWeapon->m_pPlayer )
	{
		// failed to unhook the weapon from the player!
		if ( !pWeapon->m_pPlayer->RemovePlayerItem( pWeapon ) ) return FALSE;
	}

	int iWeaponSlot = pWeapon->iItemSlot();
	
	if ( m_rgpPlayerItems[ iWeaponSlot ] )
	{
		// there's already one weapon in this slot, so link this into the slot's column
		pWeapon->m_pNext = m_rgpPlayerItems[ iWeaponSlot ];	
		m_rgpPlayerItems[ iWeaponSlot ] = pWeapon;
	}
	else
	{
		// first weapon we have for this slot
		m_rgpPlayerItems[ iWeaponSlot ] = pWeapon;
		pWeapon->m_pNext = NULL;	
	}

	pWeapon->pev->spawnflags |= SF_NORESPAWN;// never respawn
	pWeapon->pev->movetype = MOVETYPE_NONE;
	pWeapon->pev->solid = SOLID_NOT;
	pWeapon->pev->effects = EF_NODRAW;
	pWeapon->pev->modelindex = 0;
	pWeapon->pev->model = iStringNull;
	pWeapon->pev->owner = edict();
	pWeapon->SetThink( NULL );// crowbar may be trying to swing again, etc.
	pWeapon->SetTouch( NULL );
	pWeapon->m_pPlayer = NULL;

	return TRUE;
}

BOOL CWeaponBox::PackAmmo( int iszName, int iCount )
{
	int iMaxCarry;

	if ( FStringNull( iszName ) )
	{
		// error here
		Msg( "NULL String in PackAmmo!\n" );
		return FALSE;
	}
	
	iMaxCarry = MaxAmmoCarry( iszName );

	if ( iMaxCarry != -1 && iCount > 0 )
	{
		GiveAmmo( iCount, (char *)STRING( iszName ), iMaxCarry );
		return TRUE;
	}
	return FALSE;
}

int CWeaponBox::GiveAmmo( int iCount, char *szName, int iMax, int *pIndex/* = NULL*/ )
{
	int i;

	for (i = 1; i < MAX_AMMO_SLOTS && !FStringNull( m_rgiszAmmo[i] ); i++)
	{
		if (stricmp( szName, STRING( m_rgiszAmmo[i])) == 0)
		{
			if (pIndex) *pIndex = i;

			int iAdd = min( iCount, iMax - m_rgAmmo[i]);
			if (iCount == 0 || iAdd > 0)
			{
				m_rgAmmo[i] += iAdd;
				return i;
			}
			return -1;
		}
	}
	if (i < MAX_AMMO_SLOTS)
	{
		if (pIndex) *pIndex = i;

		m_rgiszAmmo[i] = MAKE_STRING( szName );
		m_rgAmmo[i] = iCount;
		return i;
	}

	Msg("out of named ammo slots\n");
	return i;
}

BOOL CWeaponBox::HasWeapon( CBasePlayerWeapon *pCheckItem )
{
	CBasePlayerWeapon *pItem = m_rgpPlayerItems[pCheckItem->iItemSlot()];

	while (pItem)
	{
		if (FClassnameIs( pItem->pev, STRING( pCheckItem->pev->classname) )) return TRUE;
		pItem = pItem->m_pNext;
	}
	return FALSE;
}

BOOL CWeaponBox::IsEmpty( void )
{
	int i;

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		if ( m_rgpPlayerItems[ i ] ) return FALSE;
	}

	for ( i = 0 ; i < MAX_AMMO_SLOTS ; i++ )
	{
		if ( !FStringNull( m_rgiszAmmo[ i ] )) 	return FALSE; // still have a bit of this type of ammo
	}
	return TRUE;
}

void CWeaponBox::SetObjectCollisionBox( void )
{
	pev->absmin = pev->origin + Vector(-16, -16, 0);
	pev->absmax = pev->origin + Vector(16, 16, 16); 
}