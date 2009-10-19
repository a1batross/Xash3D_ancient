/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#ifndef ITEMS_H
#define ITEMS_H


class CItem : public CBaseLogic
{
public:
	void	Spawn( void );
	void	Precache( void );
	CBaseEntity*	Respawn( void );
	void EXPORT ItemTouch( CBaseEntity *pOther );
	void EXPORT Materialize( void );
	void EXPORT ItemFall( void );
	virtual BOOL AddItem( CBaseEntity *pOther ) { return TRUE; };
	virtual void ItemOnGround( void ) {};
	float ItemRespawnTime( CItem *pItem );
          
          virtual BOOL IsItem( void ) { return !strncmp( STRING(pev->classname), "item_", 5 ); }
          virtual BOOL IsAmmo( void ) { return !strncmp( STRING(pev->classname), "ammo_", 5 ); }
          virtual BOOL IsWeapon( void ) { return FALSE; }
                    
	//item_generic
	virtual const char *Model( void ){ return NULL; }
	virtual const char *PickSound( void ){ return "common/null.wav"; }
	virtual const char *FallSound( void ){ return "common/null.wav"; }
};

class CWeaponBox : public CBaseEntity
{
	void Precache( void );
	void Spawn( void );
	void Touch( CBaseEntity *pOther );
	void KeyValue( KeyValueData *pkvd );
	BOOL IsEmpty( void );
	int  GiveAmmo( int iCount, char *szName, int iMax, int *pIndex = NULL );
	void SetObjectCollisionBox( void );
public:
	void EXPORT Kill ( void );
	int 	Save( CSave &save );
	int 	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	BOOL HasWeapon( CBasePlayerWeapon *pCheckItem );
	BOOL PackWeapon( CBasePlayerWeapon *pWeapon );
	BOOL PackAmmo( int iszName, int iCount );
	
	CBasePlayerWeapon *m_rgpPlayerItems[MAX_ITEM_TYPES];// one slot for each 

	int MaxAmmoCarry( int iszName );
	int m_rgiszAmmo[MAX_AMMO_SLOTS];// ammo names
	int m_rgAmmo[MAX_AMMO_SLOTS];// ammo quantities
	int m_cAmmoTypes;// how many ammo types packed into this box (if packed by a level designer)
};

#endif // ITEMS_H
