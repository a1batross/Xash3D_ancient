//=======================================================================
//			Copyright (C) Shambler Team 2006
//		       weapons.cpp - player weapon baseclass
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "cbase.h"
#include "baseweapon.h"

LINK_ENTITY_TO_CLASS( weapon_m249, CBasePlayerWeapon );
LINK_ENTITY_TO_CLASS( weapon_crossbow, CBasePlayerWeapon );
LINK_ENTITY_TO_CLASS( weapon_crowbar, CBasePlayerWeapon );
LINK_ENTITY_TO_CLASS( weapon_redeemer, CBasePlayerWeapon );
LINK_ENTITY_TO_CLASS( weapon_eagle, CBasePlayerWeapon );
LINK_ENTITY_TO_CLASS( weapon_glock, CBasePlayerWeapon );
LINK_ENTITY_TO_CLASS( weapon_mp5, CBasePlayerWeapon );
LINK_ENTITY_TO_CLASS( weapon_shotgun, CBasePlayerWeapon );
LINK_ENTITY_TO_CLASS( weapon_m40a1, CBasePlayerWeapon );
LINK_ENTITY_TO_CLASS( weapon_handgrenade, CBasePlayerWeapon );
LINK_ENTITY_TO_CLASS( weapon_debug, CBasePlayerWeapon );
LINK_ENTITY_TO_CLASS( weapon_rpg, CBasePlayerWeapon );

//***********************************************************
// 		      world_items
//***********************************************************
class CWorldItem : public CBaseEntity
{
public:
	void	KeyValue(KeyValueData *pkvd ); 
	void	Spawn( void );
};

LINK_ENTITY_TO_CLASS(world_items, CWorldItem);

void CWorldItem::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "type"))
	{
		pev->impulse = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else CBaseEntity::KeyValue( pkvd );
}

void CWorldItem::Spawn( void )
{
	CBaseEntity *pEntity = NULL;

	switch (pev->impulse) 
	{
	case 44:  //ITEM_BATTERY:
		pEntity = CBaseEntity::Create( "item_battery", pev->origin, pev->angles );
		break;
	case 45: //ITEM_SUIT:
		pEntity = CBaseEntity::Create( "item_suit", pev->origin, pev->angles );
		break;
	}

	if (!pEntity) Msg( "unable to create world_item %d\n", pev->impulse );
	else
	{
		pEntity->pev->target = pev->target;
		pEntity->pev->targetname = pev->targetname;
		pEntity->pev->spawnflags = pev->spawnflags;
	}
	REMOVE_ENTITY(edict());
}
