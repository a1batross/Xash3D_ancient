//=======================================================================
//			Copyright (C) Shambler Team 2006
//		       baseweapon.cpp - player weapon baseclass
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "cbase.h"
#include "client.h"
#include "player.h"
#include "monsters.h"
#include "baseweapon.h"
#include "soundent.h"
#include "decals.h"
#include "gamerules.h"
#include "defaults.h"

// base defines
extern int gEvilImpulse101;
ItemInfo CBasePlayerWeapon::ItemInfoArray[MAX_WEAPONS];
AmmoInfo CBasePlayerWeapon::AmmoInfoArray[MAX_AMMO_SLOTS];
char NameItems[32][MAX_WEAPONS];
int ID[MAX_WEAPONS];
int GlobalID = 0;
int g_iSwing;

// replacement table for classic half-life weapons
// add animation names if we need
const char *NAME_VM_PUMP[] = { "pump", };
const char *NAME_VM_IDLE1[] = { "idle", "idle1", };
const char *NAME_VM_IDLE2[] = { "fidget", "fidget1", };
const char *NAME_VM_IDLE3[] = { "idle3", };
const char *NAME_VM_RELOAD[] = { "reload", };
const char *NAME_VM_DEPLOY[] = { "draw", "draw1", "deploy" };
const char *NAME_VM_HOLSTER[] = { "holster", "holster1" };
const char *NAME_VM_TURNON[] = { "altfireon", };
const char *NAME_VM_TURNOFF[] = { "altfireoff", };
const char *NAME_VM_IDLE_EMPTY[] = { "idle2", };
const char *NAME_VM_START_RELOAD[] = { "start_reload" };
const char *NAME_VM_DEPLOY_EMPTY[] = { "draw2", };
const char *NAME_VM_HOLSTER_EMPTY[] = { "holster2" };
const char *NAME_VM_RANGE_ATTACK1[] = { "fire", "fire1", "shoot", };
const char *NAME_VM_RANGE_ATTACK2[] = { "fire3", };
const char *NAME_VM_RANGE_ATTACK3[] = { "fire4", };
const char *NAME_VM_MELEE_ATTACK1[] = { "fire2", "shoot_big", "grenade" };
const char *NAME_VM_MELEE_ATTACK2[] = { "fire3", };
const char *NAME_VM_MELEE_ATTACK3[] = { "fire4", };


//***********************************************************
// 		   main functions ()
//***********************************************************
TYPEDESCRIPTION	CBasePlayerWeapon::m_SaveData[] = 
{
	DEFINE_FIELD( CBasePlayerWeapon, m_pPlayer, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBasePlayerWeapon, m_pNext, FIELD_CLASSPTR ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iId, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flNextPrimaryAttack, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flNextSecondaryAttack, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flTimeWeaponIdle, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_flTimeUpdate, FIELD_TIME ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iPrimaryAmmoType, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iSecondaryAmmoType, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iClip, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iDefaultAmmo, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iDefaultAmmo2, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iZoom, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_cActiveRocket, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iOnControl, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iStepReload, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iBody, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iSkin, FIELD_INTEGER ),
	DEFINE_FIELD( CBasePlayerWeapon, m_iSpot, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CBasePlayerWeapon, CBaseAnimating );

//generic weapon entity
LINK_ENTITY_TO_CLASS( weapon_generic, CBasePlayerWeapon );

//=========================================================
// AttemptToMaterialize - the item is trying to rematerialize,
// should it do so now or wait longer?
//=========================================================
void CBasePlayerWeapon::AttemptToMaterialize( void )
{
	float time = g_pGameRules->FlWeaponTryRespawn( this );

	if ( time == 0 )
	{
		// materialize
		SetThink( Materialize );
		SetNextThink( 0 );
		return;
	}
	SetNextThink( time );
}

void CBasePlayerWeapon::Materialize( void )
{
	if ( pev->effects & EF_NODRAW )
	{
		// changing from invisible state to visible.

		SetObjectClass( ED_NORMAL );		
		pev->effects &= ~EF_NODRAW;
		pev->renderfx = kRenderFxGlowShell;
		pev->renderamt = 40;
          	pev->frags = 0;
          	pev->rendercolor.x = RANDOM_LONG( 25, 255 );
          	pev->rendercolor.y = RANDOM_LONG( 25, 255 );
          	pev->rendercolor.z = RANDOM_LONG( 25, 255 );
		pev->scale = 0.01;
		SetNextThink( 0.001 );
	}

	if( pev->scale > 1.2 )
		pev->frags = 1;
	if ( pev->frags == 1 )
	{         
		// set effects for respawn item
		if( pev->scale > 1.0 )
			pev->scale -= 0.05;
		else
		{
			pev->renderfx = kRenderFxNone;
			pev->frags = 0;
			pev->solid = SOLID_TRIGGER;
			UTIL_SetOrigin( this, pev->origin );// link into world.
			SetTouch( DefaultTouch );
			EMIT_SOUND_DYN( ENT(pev), CHAN_WEAPON, "items/respawn.wav", 1, ATTN_NORM, 0, 150 );
			SetThink( NULL );
			DontThink();
		} 
	}
	else pev->scale += 0.05;
	SetNextThink (0.001);
}

//=========================================================
// CheckRespawn - a player is taking this weapon, should 
// it respawn?
//=========================================================
void CBasePlayerWeapon :: CheckRespawn ( void )
{
	switch ( g_pGameRules->WeaponShouldRespawn( this ) )
	{
	case GR_WEAPON_RESPAWN_YES:
		Respawn();
		break;
	case GR_WEAPON_RESPAWN_NO: return;
	}
}

//=========================================================
// Respawn- this item is already in the world, but it is
// invisible and intangible. Make it visible and tangible.
//=========================================================
CBaseEntity* CBasePlayerWeapon::Respawn( void )
{
	// make a copy of this weapon that is invisible and inaccessible to players (no touch function). The weapon spawn/respawn code
	// will decide when to make the weapon visible and touchable.
	CBaseEntity *pNewWeapon = NULL;
	edict_t *pent;

	pent = CREATE_NAMED_ENTITY( pev->classname );
	if( FNullEnt( pent )) return NULL;
	pNewWeapon = Instance( pent );
	
	if( pNewWeapon )
	{
		pNewWeapon->SetObjectClass( ED_NORMAL );
		pNewWeapon->pev->owner = pev->owner;
		pNewWeapon->pev->origin = g_pGameRules->VecWeaponRespawnSpot( this );
		pNewWeapon->pev->angles = pev->angles;
		pNewWeapon->pev->netname = pev->netname; //copy weapon_script name
		DispatchSpawn( pNewWeapon->edict() );
		pNewWeapon->pev->effects |= EF_NODRAW;// invisible for now
		pNewWeapon->SetTouch( NULL );// no touch
		pNewWeapon->SetThink( AttemptToMaterialize );

		DROP_TO_FLOOR( pNewWeapon->edict( ));

		// not a typo! We want to know when the weapon the player just picked up should respawn! This new entity we created is the replacement,
		// but when it should respawn is based on conditions belonging to the weapon that was taken.
		pNewWeapon->AbsoluteNextThink( g_pGameRules->FlWeaponRespawnTime( this ));
	}
	else ALERT( at_error, "failed to respawn %s!\n", STRING( pev->classname ));

	return pNewWeapon;
}

//=========================================================
// FallThink - Items that have just spawned run this think
// to catch them when they hit the ground. Once we're sure
// that the object is grounded, we change its solid type
// to trigger and set it in a large box that helps the
// player get it.
//=========================================================
void CBasePlayerWeapon::FallThink ( void )
{
	SetNextThink( 0.1 );

	if ( pev->flags & FL_ONGROUND )
	{
		// clatter if we have an owner (i.e., dropped by someone)
		// don't clatter if the gun is waiting to respawn (if it's waiting, it is invisible!)
		if ( !FNullEnt( pev->owner ) )
		{
			int pitch = 95 + RANDOM_LONG(0,29);
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "items/weapondrop1.wav", 1, ATTN_NORM, 0, pitch);	
		}

		// lie flat
		pev->angles.x = 0;
		pev->angles.z = 0;

		pev->solid = SOLID_TRIGGER;

		UTIL_SetOrigin( this, pev->origin );// link into world.
		SetTouch( DefaultTouch );
		SetThink (NULL); 
	}
}

BOOL CBasePlayerWeapon :: CanDeploy( void )
{
	BOOL bHasAmmo = 0;
	if(iFlags() & ITEM_FLAG_SELECTONEMPTY) return TRUE;

	// this weapon doesn't use ammo, can always deploy.
	if ( !FStriCmp( pszAmmo1(), "none" )) return TRUE;

	if ( pszAmmo1() ) bHasAmmo |= (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0);
	if ( pszAmmo2() ) bHasAmmo |= (m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] != 0);
	if (m_iClip > 0) bHasAmmo |= 1;

	if (!bHasAmmo) return FALSE;
	return TRUE;
}

//=========================================================
//	Precache weapons and all resources
//=========================================================
void CBasePlayerWeapon :: Precache( void )
{
	ItemInfo II;

	if( GetItemInfo( &II ))
	{
		ItemInfoArray[II.iId] = II;

		if( II.iszAmmo1 ) AddAmmoName( II.iszAmmo1 );
		if( II.iszAmmo2 ) AddAmmoName( II.iszAmmo2 );

		memset( &II, 0, sizeof( II ));
	
		UTIL_PrecacheModel( iViewModel() );
		UTIL_PrecacheModel( iWorldModel() );
	}
}

//=========================================================
//	Get Weapon Info from Script File
//=========================================================
int CBasePlayerWeapon :: GetItemInfo( ItemInfo *p )
{
	if( FStringNull( pev->netname ))
		pev->netname = pev->classname;

	if( ParseWeaponFile( p, STRING( pev->netname )))
	{                                                     
		// make unical weapon number
		GenerateID();
		p->iId = m_iId;
 		return 1;
	}
	return 0;
}

//=========================================================
//	Base Spawn
//=========================================================
void CBasePlayerWeapon :: Spawn( void )
{
	Precache();            

	// don't let killed entity continue spawning
	if( pev->flags & FL_KILLME )
		return;
          
	pev->movetype = MOVETYPE_TOSS;
	pev->solid = SOLID_BBOX;
          pev->sequence = 1; // set world animation
	
	UTIL_SetOrigin( this, pev->origin );
	UTIL_SetModel( ENT( pev ), iWorldModel( ));
	SetObjectClass( ED_NORMAL );
	
	SetTouch( DefaultTouch );
	SetThink( FallThink );

	// pointsize until it lands on the ground.
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ));

	m_iSpot = 0;
	pev->animtime = gpGlobals->time + 0.1;
	b_restored = TRUE; // already restored
	
	SetNextThink( 0.1 );
}

//=========================================================
//	Make unical ID number for each weapon
//=========================================================
void CBasePlayerWeapon :: GenerateID( void )
{
	for( int i = 0; i < GlobalID; i++ )
	{
		if( FStrEq( STRING( pev->netname ), NameItems[i] ))
		{               
			m_iId = ID[i];
			return;
 		}
	}
	
	GlobalID++;
	m_iId = GlobalID;
	strcpy( NameItems[GlobalID-1], STRING( pev->netname ));
	ID[GlobalID-1] = m_iId;
}

//=========================================================
//	Default Touch
//=========================================================
void CBasePlayerWeapon::DefaultTouch( CBaseEntity *pOther )
{
	// if it's not a player, ignore
	if ( !pOther->IsPlayer() ) return;

	CBasePlayer *pPlayer = (CBasePlayer *)pOther;

	// can I have this?
	if ( !g_pGameRules->CanHavePlayerItem( pPlayer, this ) )
	{
		if ( gEvilImpulse101 ) UTIL_Remove( this );
		return;
	}

	if( pOther->AddPlayerItem( this ))
	{
		AttachToPlayer( pPlayer );
		EMIT_SOUND(ENT(pPlayer->pev), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM);
	}
}

//=========================================================
//	Reset ItemInfo as default
//=========================================================
void CBasePlayerWeapon :: ResetParse( ItemInfo *II )
{
	m_iId = MAX_WEAPONS; //Will be owerwrite with GenerateID()
	II->iSlot = 0;
	II->iPosition = 0;
	II->iViewModel = iStringNull;
	II->iWorldModel = iStringNull;
	strcpy( II->szAnimExt, "onehanded");
 	II->iszAmmo1 = MAKE_STRING( "none" );
	II->iMaxAmmo1 = WEAPON_NOAMMO;
 	II->iszAmmo2 = MAKE_STRING( "none" );
 	II->iMaxAmmo2 = WEAPON_NOAMMO;
	II->iMaxClip = -1;
	II->iFlags = 0;
	II->iWeight = 0;
	II->attack1 = NONE;
	II->attack2 = NONE;
	II->fNextAttack = 0.5;
	II->fNextAttack2 = 0.5;
	memset( II->firesound, 0, sizeof( MAX_SHOOTSOUNDS ));
	memset( II->sfxsound, 0, sizeof( MAX_SHOOTSOUNDS ));
	II->sndcount = 0;
	II->sfxcount = 0;
	II->emptysnd = iStringNull;
	II->punchangle1 = RandomRange(0.0);
	II->punchangle2 = RandomRange(0.0);
	II->recoil1 = RandomRange(0.0);
	II->recoil2 = RandomRange(0.0);
	m_iDefaultAmmo = 0;
	m_iDefaultAmmo2 = 0;
}

//=========================================================
//	Parse Script File
//=========================================================
int CBasePlayerWeapon :: ParseWeaponFile( ItemInfo *II, const char *filename ) 
{
	char path[256];
	int iResult = 0;	

	sprintf( path, "scripts/items/%s.txt", filename );
	char *afile = (char *)LOAD_FILE( path, NULL );
	const char *pfile = afile;

	ResetParse( II );
	
	if( !pfile )
	{
 		ALERT( at_warning, "Weapon info file for %s not found!\n", STRING( pev->netname ));
 		UTIL_Remove( this );
 		return 0;
	}
	else
	{

		II->iszName = pev->netname;
		ALERT( at_aiconsole, "\nparse %s.txt\n", STRING( II->iszName ));
		// parses the type, moves the file pointer
		iResult = ParseWeaponData( II, pfile );
		ALERT( at_aiconsole, "Parsing: WeaponData{} %s\n", iResult ? "^3OK" : "^1ERROR" );
		iResult = ParsePrimaryAttack( II, pfile );
		ALERT( at_aiconsole, "Parsing: PrimaryAttack{} %s\n", iResult ? "^3OK" : "^1ERROR" );
		iResult = ParseSecondaryAttack( II, pfile );
		ALERT( at_aiconsole, "Parsing: SecondaryAttack{} %s\n", iResult ? "^3OK" : "^1ERROR" );
		iResult = ParseSoundData( II, pfile );
		ALERT( at_aiconsole, "Parsing: SoundData{} %s\n", iResult ? "^3OK" : "^1ERROR" );
		COM_FreeFile( afile );
		return 1;
 	}
}

//=========================================================
//	Parse SoundData {} section
//=========================================================
int CBasePlayerWeapon :: ParseSoundData( ItemInfo *II, const char *pfile )
{
	char *token = NULL;

	while ( FStriCmp( token, "SoundData") )		// search SoundData header
	{
		if( !pfile ) return 0;
		token = COM_ParseToken( &pfile );
	}
	while ( FStriCmp( token, "{") )		// search {
	{
		if( !pfile ) return 0;
		token = COM_ParseToken( &pfile );
	}

	token = COM_ParseToken( &pfile );
	while ( FStriCmp( token, "}" ) )
	{
		if ( !token ) break;

		if ( !FStriCmp( token, "firesound" )) 
		{                                          
			token = COM_ParseToken( &pfile );
			if( II->sndcount < MAX_SHOOTSOUNDS ) 
			{
				II->firesound[II->sndcount] = UTIL_PrecacheSound( token );	// precache sound
				II->sndcount++;
			}
			else 
			{
				ALERT( at_warning, "Too many shoot sounds for %s\n", STRING( pev->netname ));
				break; // syntax error, stop parsing
			}
		}
		else if ( !FStriCmp( token, "sfxsound" )) 
		{                                          
			token = COM_ParseToken( &pfile );
			if( II->sfxcount < MAX_SHOOTSOUNDS ) 
			{
				II->sfxsound[II->sfxcount] = UTIL_PrecacheSound( token );	// precache sound
				II->sfxcount++;
			}
			else 
			{
				ALERT( at_warning, "Too many sfx sounds for %s\n", STRING( pev->netname ));
				break;	// syntax error, stop parsing
			}
		}
		else if ( !FStriCmp( token, "emptysound" )) 
		{
			token = COM_ParseToken( &pfile );
			II->emptysnd = UTIL_PrecacheSound( token );//precache sound
		}
		token = COM_ParseToken( &pfile );
	}
	return 1;
}

//=========================================================
//	Parse SecondaryAttack {} section
//=========================================================
int CBasePlayerWeapon :: ParseSecondaryAttack( ItemInfo *II, const char *pfile )
{
	char	*token = NULL;

	while ( FStriCmp( token, "SecondaryAttack") )	//search SecondaryAttack header
	{
		if( !pfile ) return 0;
		token = COM_ParseToken( &pfile );
	}
	while ( FStriCmp( token, "{") )		//search {
	{
		if( !pfile ) return 0;
		token = COM_ParseToken( &pfile );
	}
	
	token = COM_ParseToken( &pfile );
	while ( FStriCmp( token, "}" ) )
	{
		if ( !token ) break;

		if ( !FStriCmp( token, "action" )) 
		{                                          
  			token = COM_ParseToken( &pfile );    
			if ( !FStriCmp( token, "none" )) II->attack2 = NONE;
			else if ( !FStriCmp( token, "ammo1" )) II->attack2 = AMMO1;
			else if ( !FStriCmp( token, "ammo1." ))
			{
				II->attack2 = AMMO1;
				SetBits(pev->impulse, SATTACK_FIREMODE1);
			}
			else if ( !FStriCmp( token, "ammo2" )) II->attack2 = AMMO2;
			else if ( !FStriCmp( token, "ammo2." ))
			{
				II->attack2 = AMMO2;
				SetBits(pev->impulse, SATTACK_FIREMODE1);
			}
			else if ( !FStriCmp( token, "laserdot" )) II->attack2 = LASER_DOT;
			else if ( !FStriCmp( token, "zoom" )) II->attack2 = ZOOM;
			else if ( !FStriCmp( token, "flashlight" )) II->attack2 = FLASHLIGHT;
			else if ( !FStriCmp( token, "switchmode" )) II->attack2 = SWITCHMODE;
			else if ( !FStriCmp( token, "swing" )) II->attack2 = SWING;
			else II->attack2 = ALLOC_STRING( token ); //client command
		}
		else if ( !FStriCmp( token, "punchangle" )) 
		{                                          
			token = COM_ParseToken( &pfile );
			II->punchangle2 = RandomRange((char *)STRING(ALLOC_STRING(token)));
		}
		else if ( !FStriCmp( token, "recoil" )) 
		{                                          
			token = COM_ParseToken( &pfile );
			II->recoil2 = RandomRange((char *)STRING(ALLOC_STRING(token)));
		}
		else if ( !FStriCmp( token, "nextattack" )) 
		{                                          
			token = COM_ParseToken( &pfile );
			II->fNextAttack2 = atof(token);
		}
		token = COM_ParseToken( &pfile );
	}
	return 1;
}

//=========================================================
//	Parse PrimaryAttack {} section
//=========================================================
int CBasePlayerWeapon :: ParsePrimaryAttack( ItemInfo *II, const char *pfile )
{
	char	*token = NULL;

	while ( FStriCmp( token, "PrimaryAttack") )	//search PrimaryAttack header
	{
		if(!pfile) return 0;
		token = COM_ParseToken( &pfile );
	}
	while ( FStriCmp( token, "{") )		//search {
	{
		if(!pfile) return 0;
		token = COM_ParseToken( &pfile );
	}
	
	token = COM_ParseToken( &pfile );
	while ( FStriCmp( token, "}" ) )
	{
		if ( !token ) break;

		if ( !FStriCmp( token, "action" )) 
		{                                          
  			token = COM_ParseToken( &pfile );    
			if ( !FStriCmp( token, "none" )) II->attack1 = NONE;
			else if ( !FStriCmp( token, "ammo1" )) II->attack1 = AMMO1;
			else if ( !FStriCmp( token, "ammo1." ))
			{
				II->attack1 = AMMO1;
				SetBits(pev->impulse, PATTACK_FIREMODE1);
			}
			else if ( !FStriCmp( token, "ammo2" )) II->attack1 = AMMO2;
			else if ( !FStriCmp( token, "ammo2." ))
			{
				II->attack1 = AMMO2;
				SetBits(pev->impulse, PATTACK_FIREMODE1);
			}
			else if ( !FStriCmp( token, "laserdot" )) II->attack1 = LASER_DOT;
			else if ( !FStriCmp( token, "zoom" )) II->attack1 = ZOOM;
			else if ( !FStriCmp( token, "flashlight" )) II->attack1 = FLASHLIGHT;
			else if ( !FStriCmp( token, "switchmode" )) II->attack1 = SWITCHMODE;
			else if ( !FStriCmp( token, "swing" )) II->attack1 = SWING;
			else II->attack1 = ALLOC_STRING( token ); //client command
		}
		else if ( !FStriCmp( token, "punchangle" )) 
		{                                          
			token = COM_ParseToken( &pfile );
			II->punchangle1 = RandomRange((char *)STRING(ALLOC_STRING(token)));
		}
		else if ( !FStriCmp( token, "recoil" )) 
		{                                          
			token = COM_ParseToken( &pfile );
			II->recoil1 = RandomRange((char *)STRING(ALLOC_STRING(token)));
		}
		else if ( !FStriCmp( token, "nextattack" )) 
		{                                          
			token = COM_ParseToken( &pfile );
			II->fNextAttack = atof(token);
		}
		token = COM_ParseToken( &pfile );
	}
	return 1;
}

//=========================================================
//	Parse WeaponData {} section
//=========================================================
int CBasePlayerWeapon :: ParseWeaponData( ItemInfo *II, const char *pfile )
{
	char	*token = NULL;

	while ( FStriCmp( token, "WeaponData") )
	{
		if ( !pfile ) return 0;
		token = COM_ParseToken( &pfile );
	}
	while ( FStriCmp( token, "{") )		// search {
	{
		if ( !pfile ) return 0;
		token = COM_ParseToken( &pfile );
	}
		
	token = COM_ParseToken( &pfile );
	while ( FStriCmp( token, "}" ))
	{
		if ( !token ) break;
		
		if ( !FStriCmp( token, "viewmodel" )) 
		{                                          
			token = COM_ParseToken( &pfile );
			II->iViewModel = ALLOC_STRING(token);
		}
		else if( !FStriCmp( token, "playermodel" )) 
		{                                          
			token = COM_ParseToken( &pfile );
			II->iWorldModel = ALLOC_STRING(token);			
		}
		else if( !FStriCmp( token, "anim_prefix" )) 
		{                                          
			token = COM_ParseToken( &pfile );
			strncpy( II->szAnimExt, token, sizeof( II->szAnimExt ));
		}
		else if( !FStriCmp( token, "bucket" )) 
		{                                          
			token = COM_ParseToken( &pfile );
			II->iSlot = atoi( token );  
		} 
		else if( !FStriCmp( token, "bucket_position" )) 
		{                                          
			token = COM_ParseToken( &pfile );
			II->iPosition = atoi( token ); 
		} 
		else if( !FStriCmp( token, "clip_size" )) 
		{                                             
  			token = COM_ParseToken( &pfile );
			if( !FStriCmp( token, "noclip" ))
				II->iMaxClip = -1; 
			else II->iMaxClip = atoi( token ); 
  	 	}  
		else if( !FStriCmp( token, "primary_ammo" )) 
		{                                   
  			token = COM_ParseToken( &pfile );    
			
			if ( !FStriCmp( token, "none" )) II->iMaxAmmo1 = WEAPON_NOAMMO;
			else if ( !FStriCmp( token, "357" )) II->iMaxAmmo1 = DESERT_MAX_CARRY;
			else if ( !FStriCmp( token, "9mm" )) II->iMaxAmmo1 = GLOCK_MAX_CARRY;
			else if ( !FStriCmp( token, "12mm" )) II->iMaxAmmo1 = GLOCK_MAX_CARRY;
			else if ( !FStriCmp( token, "762" )) II->iMaxAmmo1 = M40A1_MAX_CARRY;
			else if ( !FStriCmp( token, "m203" )) II->iMaxAmmo1 = M203_GRENADE_MAX_CARRY;
			else if ( !FStriCmp( token, "nuke" )) II->iMaxAmmo1 = ROCKET_MAX_CARRY;
			else if ( !FStriCmp( token, "rockets" )) II->iMaxAmmo1 = ROCKET_MAX_CARRY;
			else if ( !FStriCmp( token, "556" )) II->iMaxAmmo1 = GLOCK_MAX_CARRY;
			else if ( !FStriCmp( token, "buckshot" )) II->iMaxAmmo1 = BUCKSHOT_MAX_CARRY;
			else if ( !FStriCmp( token, "grenade" )) II->iMaxAmmo1 = M203_GRENADE_MAX_CARRY;
			else if ( !FStriCmp( token, "bolts" )) II->iMaxAmmo1 = BOLT_MAX_CARRY;

			II->iszAmmo1 = ALLOC_STRING( token );
		}
		else if ( !FStriCmp( token, "secondary_ammo" )) 
		{                                   
			token = COM_ParseToken( &pfile ); 

			if ( !FStriCmp( token, "none" )) II->iMaxAmmo2 = WEAPON_NOAMMO;
			else if ( !FStriCmp( token, "357" )) II->iMaxAmmo2 = DESERT_MAX_CARRY;
			else if ( !FStriCmp( token, "9mm" )) II->iMaxAmmo2 = GLOCK_MAX_CARRY;
			else if ( !FStriCmp( token, "12mm" )) II->iMaxAmmo2 = GLOCK_MAX_CARRY;
			else if ( !FStriCmp( token, "762" )) II->iMaxAmmo2 = M40A1_MAX_CARRY;
			else if ( !FStriCmp( token, "m203" )) II->iMaxAmmo2 = M203_GRENADE_MAX_CARRY;
			else if ( !FStriCmp( token, "nuke" )) II->iMaxAmmo2 = ROCKET_MAX_CARRY;
			else if ( !FStriCmp( token, "rockets" )) II->iMaxAmmo2 = ROCKET_MAX_CARRY;
			else if ( !FStriCmp( token, "556" )) II->iMaxAmmo2 = GLOCK_MAX_CARRY;
			else if ( !FStriCmp( token, "buckshot" )) II->iMaxAmmo2 = BUCKSHOT_MAX_CARRY;
			else if ( !FStriCmp( token, "grenade" )) II->iMaxAmmo1 = M203_GRENADE_MAX_CARRY;//don't change this!
			else if ( !FStriCmp( token, "bolts" )) II->iMaxAmmo2 = BOLT_MAX_CARRY;
			
			II->iszAmmo2 = ALLOC_STRING( token );
		} 
		else if ( !FStriCmp( token, "defaultammo" )) 
		{                                                      
  			token = COM_ParseToken( &pfile );
			m_iDefaultAmmo = RandomRange( token ).Random();
		}
		else if ( !FStriCmp( token, "defaultammo2" )) 
		{                                                      
  			token = COM_ParseToken( &pfile );
			m_iDefaultAmmo2 = RandomRange( token ).Random();
		}
		else if ( !FStriCmp( token, "weight" )) 
		{                                          
			token = COM_ParseToken( &pfile );
			II->iWeight = atoi(token); 
		}
		else if ( !FStriCmp( token, "item_flags" )) 
		{                                          
			token = COM_ParseToken( &pfile );
			II->iFlags = atoi(token); 
		}
 		token = COM_ParseToken( &pfile );
 	}
	return 1;
}

//=========================================================
//	Set Animation at ACT
//=========================================================
int CBasePlayerWeapon :: PlaySequence( Activity activity, float fps )
{
	if(!m_pPlayer->pev->viewmodel) return -1;    
	UTIL_SetModel( ENT(pev), STRING( m_pPlayer->pev->viewmodel ));
	int iSequence = LookupActivity( activity );
	if( iSequence != -1 ) SendWeaponAnim( iSequence, fps );
	else DevMsg("Warning: %s not found\n", activity_map[activity - 1].name);

	return iSequence;
} 

//=========================================================
//	Set Animation at name
//=========================================================
int CBasePlayerWeapon :: SetAnimation( char *name, float fps )
{
	if( !m_pPlayer->pev->viewmodel ) return -1;    
	UTIL_SetModel( ENT( pev ), STRING( m_pPlayer->pev->viewmodel ));
 	int iSequence = LookupSequence( name );
	if( iSequence != -1 ) SendWeaponAnim( iSequence, fps );
	else ALERT( at_warning, "sequence \"%s\" not found\n", name );

	return iSequence;
}

//=========================================================
//	Found sequence at act or name (generic method)
//=========================================================
int CBasePlayerWeapon :: SetAnimation( Activity activity, float fps )
{
	int iSequence, m_iAnimCount;
          const char **pAnimsList = NULL;
          
	if(!m_pPlayer->pev->viewmodel) return -1;
	UTIL_SetModel( ENT(pev), STRING( m_pPlayer->pev->viewmodel ));
	
	// try to playing ACT sequence
	iSequence = LookupActivity( activity );
	if( iSequence != -1 )
	{
		SendWeaponAnim( iSequence, fps ); // activity method
		return iSequence;
	}
                        
	// ACT not found, translate name to ACT
	switch ( activity )
	{
	case ACT_VM_IDLE1:
		pAnimsList = NAME_VM_IDLE1;
		m_iAnimCount = ARRAYSIZE( NAME_VM_IDLE1 );
		break;
	case ACT_VM_IDLE2:
		pAnimsList = NAME_VM_IDLE2;
		m_iAnimCount = ARRAYSIZE( NAME_VM_IDLE2 );
		break;
	case ACT_VM_IDLE3:
		pAnimsList = NAME_VM_IDLE3;
		m_iAnimCount = ARRAYSIZE( NAME_VM_IDLE3 );
		break;
	case ACT_VM_IDLE_EMPTY:
		pAnimsList = NAME_VM_IDLE_EMPTY;
		m_iAnimCount = ARRAYSIZE( NAME_VM_IDLE_EMPTY );
		break;		
	case ACT_VM_RANGE_ATTACK1:
		pAnimsList = NAME_VM_RANGE_ATTACK1;
		m_iAnimCount = ARRAYSIZE( NAME_VM_RANGE_ATTACK1 );
		break;
	case ACT_VM_RANGE_ATTACK2:
		pAnimsList = NAME_VM_RANGE_ATTACK2;
		m_iAnimCount = ARRAYSIZE( NAME_VM_RANGE_ATTACK2 );
		break;
	case ACT_VM_RANGE_ATTACK3:
		pAnimsList = NAME_VM_RANGE_ATTACK3;
		m_iAnimCount = ARRAYSIZE( NAME_VM_RANGE_ATTACK3 );
		break;
	case ACT_VM_MELEE_ATTACK1:
		pAnimsList = NAME_VM_MELEE_ATTACK1;
		m_iAnimCount = ARRAYSIZE( NAME_VM_MELEE_ATTACK1 );
		break;
	case ACT_VM_MELEE_ATTACK2:
		pAnimsList = NAME_VM_MELEE_ATTACK2;
		m_iAnimCount = ARRAYSIZE( NAME_VM_MELEE_ATTACK2 );
		break;
	case ACT_VM_MELEE_ATTACK3:
		pAnimsList = NAME_VM_MELEE_ATTACK3;
		m_iAnimCount = ARRAYSIZE( NAME_VM_MELEE_ATTACK3 );
		break;
	case ACT_VM_PUMP:
		pAnimsList = NAME_VM_PUMP;
		m_iAnimCount = ARRAYSIZE( NAME_VM_PUMP );
		break;
	case ACT_VM_START_RELOAD:
		pAnimsList = NAME_VM_START_RELOAD;
		m_iAnimCount = ARRAYSIZE( NAME_VM_START_RELOAD );
		break;
	case ACT_VM_RELOAD:
		pAnimsList = NAME_VM_RELOAD;
		m_iAnimCount = ARRAYSIZE( NAME_VM_RELOAD );
		break;
	case ACT_VM_DEPLOY:
		pAnimsList = NAME_VM_DEPLOY;
		m_iAnimCount = ARRAYSIZE( NAME_VM_DEPLOY );
		break;
	case ACT_VM_DEPLOY_EMPTY:
		pAnimsList = NAME_VM_DEPLOY_EMPTY;
		m_iAnimCount = ARRAYSIZE( NAME_VM_DEPLOY_EMPTY );
		break;
	case ACT_VM_HOLSTER:
		pAnimsList = NAME_VM_HOLSTER;
		m_iAnimCount = ARRAYSIZE( NAME_VM_HOLSTER );
		break;
	case ACT_VM_HOLSTER_EMPTY:
		pAnimsList = NAME_VM_HOLSTER_EMPTY;
		m_iAnimCount = ARRAYSIZE( NAME_VM_HOLSTER_EMPTY );
		break;
	case ACT_VM_TURNON:
		pAnimsList = NAME_VM_TURNON;
		m_iAnimCount = ARRAYSIZE( NAME_VM_TURNON );
		break;
	case ACT_VM_TURNOFF:
		pAnimsList = NAME_VM_TURNOFF;
		m_iAnimCount = ARRAYSIZE( NAME_VM_TURNOFF );
		break;
	default:
		m_iAnimCount = 0;
		break;	
	}
 
	// lookup all names
	for(int i = 0; i < m_iAnimCount; i++ )
	{
		iSequence = LookupSequence( pAnimsList[i] );
		if( iSequence != -1 )
		{
			SendWeaponAnim( iSequence, fps ); // names method
			return iSequence;
		}
	}
 	
	return -1; //no matches found
}

float CBasePlayerWeapon :: SequenceFPS( int iSequence )
{
	dstudiohdr_t *pstudiohdr = (dstudiohdr_t *)GET_MODEL_PTR( ENT( pev ));

	if( pstudiohdr )
	{
		dstudioseqdesc_t *pseqdesc;

		if( iSequence == -1 ) iSequence = m_iSequence;			
		pseqdesc = (dstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + iSequence;
		return pseqdesc->fps;
	}
	return 0.0f;
}

float CBasePlayerWeapon :: SequenceDuration( int iSequence, float fps )
{               
	dstudiohdr_t *pstudiohdr = (dstudiohdr_t *)GET_MODEL_PTR( ENT( pev ));

	if( pstudiohdr )
	{
		dstudioseqdesc_t *pseqdesc;

		if( iSequence == -1 ) iSequence = m_iSequence;			
		pseqdesc = (dstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + iSequence;
		if( fps == 0.0f ) fps = pseqdesc->fps;
		return (float)pseqdesc->numframes / fps;
	}
	return 0.0f;
}

//=========================================================
//	Set Weapon Anim
//=========================================================
void CBasePlayerWeapon :: SendWeaponAnim( int sequence, float fps )
{                
	float	framerate = 1.0f;	// fps multiplier
	m_iSequence = sequence;	// member current sequence

	if( fps )
	{
		dstudiohdr_t *pstudiohdr = (dstudiohdr_t *)GET_MODEL_PTR( ENT( pev ));
		if( pstudiohdr )
		{
 			dstudioseqdesc_t *pseqdesc;
			
			pseqdesc = (dstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + m_iSequence;
			framerate = fps / pseqdesc->fps;
		}
	}

	// calculate additional body for special effects
	pev->body = (pev->body % NUM_HANDS) + NUM_HANDS * m_iBody;
 	
	MESSAGE_BEGIN( MSG_ONE, gmsg.WeaponAnim, NULL, m_pPlayer->pev );
		WRITE_BYTE( m_iSequence );						
		WRITE_BYTE( pev->body );
		WRITE_BYTE( framerate * 16 );
	MESSAGE_END();                                    
	
	SetNextIdle( SequenceDuration( ));
}

//=========================================================
//	generic base functions
//=========================================================
BOOL CBasePlayerWeapon :: DefaultDeploy( Activity activity )
{                                                       
	if( PLAYER_HAS_SUIT )
		pev->body |= GORDON_SUIT;
	else pev->body &= ~GORDON_SUIT;

	m_iClientSkin = -1; // reset last skin info for new weapon
	m_iClientBody = -1; // reset last body info for new weapon
	m_iClientFov = -1;	// reset last fov info for new weapon
	
	m_pPlayer->pev->viewmodel = iViewModel();
	m_pPlayer->pev->weaponmodel = iWorldModel();
	strcpy( m_pPlayer->m_szAnimExtention, szAnimExt());

	m_iSequence = LookupActivity( activity );		
	float fps = IsMultiplayer() ? SequenceFPS() * 2.0f : 0.0f;
	if( SetAnimation( activity, fps ) != -1 )
	{
		SetNextAttack( SequenceDuration( m_iSequence, fps )); // delay before idle playing       
		return TRUE;        
	}

	// animation not found
	return FALSE;
}

BOOL CBasePlayerWeapon :: DefaultHolster( Activity activity )
{
	m_fInReload = FALSE;
	int iResult = 0;

	m_iSequence = LookupActivity( activity );
	float fps = IsMultiplayer() ? SequenceFPS() * 2.0f : 0.0f;

          ZoomReset();

	iResult = SetAnimation( activity, fps );
	if( iResult == -1 ) return 0;

	SetNextAttack( SequenceDuration( m_iSequence, fps )); // delay before switching
	if( m_pSpot ) // disable laser dot
	{
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/spot_off.wav", 1, ATTN_NORM );
		m_pSpot->Killed();
		m_pSpot = NULL;
	}
	m_iSkin = 0; // reset screen

	if( iAttack1() == FLASHLIGHT || iAttack2() == FLASHLIGHT )
		ClearBits( m_pPlayer->pev->effects, EF_DIMLIGHT );

	if(( iFlags() & ITEM_FLAG_EXHAUSTIBLE ) && (m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] <= 0 ))
	{
		// no more ammo!
		m_pPlayer->pev->weapons &= ~(1<<m_iId);
		m_pPlayer->pev->viewmodel = iStringNull;
		m_pPlayer->pev->weaponmodel = iStringNull;
		SetThink( DestroyItem );
		SetNextThink( 0.5 );
	}

	// animation not found
	return 1;
}

void CBasePlayerWeapon :: DefaultIdle( void )
{ 
	// weapon have clip and ammo or just have ammo
	if ((iMaxClip() && m_iClip) || m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0 || iMaxAmmo1() == -1)
	{
		// play random idle animation
		int iAnim;
		float flRand = RANDOM_FLOAT(0, 1.2);
		if (flRand < 0.2) iAnim = ACT_VM_IDLE1;
		else if ( 0.4 > flRand && flRand > 0.2 ) iAnim = ACT_VM_IDLE2;
		else if ( 0.8 > flRand && flRand > 0.5 ) iAnim = ACT_VM_IDLE3;
		else
		{ 
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_LONG(10, 15);
                    	return;//don't execute SetAnimation
                    }
		SetAnimation((Activity)iAnim);
	}
	else
	{
		SetAnimation( ACT_VM_IDLE_EMPTY );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_LONG(20, 45);
	}
}

BOOL CBasePlayerWeapon :: DefaultReload( Activity sequence )
{
	if( m_cActiveRocket && m_iSpot )
		return FALSE;
	if( m_flNextPrimaryAttack > UTIL_WeaponTimeBase())
		return FALSE;
	if( m_flNextSecondaryAttack > UTIL_WeaponTimeBase())
		return FALSE;
	if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0 ) // have ammo?
	{ 
		if( iMaxClip() == m_iClip ) return FALSE;
		if( m_iStepReload == 0 ) // try to playing step reload
		{
			if( SetAnimation( ACT_VM_START_RELOAD ) != -1 )
			{
				// found anim, continue
				m_iStepReload = 1;
				m_fInReload = TRUE; // disable reload button
				return TRUE; // start reload cycle. See also ItemPostFrame 
			}
			else // init default reload
			{
				int i = min(iMaxClip() - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]);	
				if( i == 0 ) return FALSE;
				int iResult = -1;
				ZoomReset(); // reset zoom
				if( m_iClip <= 0 ) // empty clip ?
				{
					// iResult is error code
					iResult = SetAnimation( ACT_VM_RELOAD_EMPTY );
					m_iStepReload = EMPTY_RELOAD; // it's empty reload
				}
				if( iResult == -1 ) 
				{
					SetAnimation( sequence );
					m_iStepReload = NORMAL_RELOAD; // it's not empty reload
				}
				if( m_pSpot ) m_pSpot->Suspend( SequenceDuration( )); // suspend laserdot
				SetNextAttack( SequenceDuration( ));
				m_fInReload = TRUE; // disable reload button
				
				return TRUE; // that's all right
			}
		}
		else if( m_iStepReload == 1 ) // continue step reload
		{
			if( m_flTimeWeaponIdle > UTIL_WeaponTimeBase( ))
				return FALSE;
			// was waiting for gun to move to side
			SetAnimation( sequence );
			m_iStepReload = 2;
		}
		else if( m_iStepReload == 2 ) // continue step reload
		{
			// add them to the clip
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
			m_iClip++;
			m_iStepReload = 1;
			if( m_iClip == iMaxClip()) m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.1;
		}
		return TRUE;
	}            
	return FALSE;
}            

BOOL CBasePlayerWeapon :: PlayEmptySound( void )
{
	if( m_iPlayEmptySound )
	{
		if( EmptySnd( ))
			EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, STRING( EmptySnd( )), 1, ATTN_NORM );
		m_iPlayEmptySound = 0;
		return TRUE;
	}
	return FALSE;
}
//=========================================================
//	Get Atatck Info by AmmoInfo
//=========================================================
Vector CBasePlayerWeapon :: GetCurrentSpread( const char *ammo )
{
	switch( GetBulletType( ammo ))
	{
	case BULLET_9MM: return VECTOR_CONE_2DEGREES;
	case BULLET_357: return VECTOR_CONE_2DEGREES;
	case BULLET_556: return VECTOR_CONE_3DEGREES;
	case BULLET_762: return VECTOR_CONE_1DEGREES;
	case BULLET_12MM: return VECTOR_CONE_15DEGREES;
	case BULLET_BUCKSHOT: return VECTOR_CONE_10DEGREES;
	default: return VECTOR_CONE_0DEGREES;
	}
}

int CBasePlayerWeapon :: GetBulletType( const char *ammo )
{
	if( !FStriCmp( ammo, "9mm" ))		return BULLET_9MM;
	else if( !FStriCmp( ammo, "357" ))	return BULLET_357;
	else if( !FStriCmp( ammo, "556" ))	return BULLET_556;
	else if( !FStriCmp( ammo, "762" ))	return BULLET_762;
	else if( !FStriCmp( ammo, "12mm" ))	return BULLET_12MM;
	else if( !FStriCmp( ammo, "buckshot" ))	return BULLET_BUCKSHOT;

	return BULLET_NONE;
}

int CBasePlayerWeapon :: GetAmmoType( const char *ammo )
{
	const char *ammoname;
	ammoname = m_pPlayer->GetAmmoName(m_iPrimaryAmmoType);
	if(!FStriCmp( ammo, ammoname )) return 1;
	ammoname = m_pPlayer->GetAmmoName(m_iSecondaryAmmoType);
	if(!FStriCmp( ammo, ammoname )) return 2;
	return 0;
}

int CBasePlayerWeapon :: ReturnAmmoIndex( const char *ammo )
{
	const char *ammoname;
	ammoname = m_pPlayer->GetAmmoName( m_iPrimaryAmmoType );
	if( !FStriCmp( ammo, ammoname )) return m_iPrimaryAmmoType;
	ammoname = m_pPlayer->GetAmmoName( m_iSecondaryAmmoType );
	if( !FStriCmp( ammo, ammoname )) return m_iSecondaryAmmoType;
	return 0;
}

int CBasePlayerWeapon :: GetCurrentAttack( const char *ammo, int firemode )
{
	int iResult;
	if( !FStriCmp( ammo, "none" ))	//no ammo
	{
		//just play animation and sound
		if(!firemode) iResult = PlayRangeAttack(); // anim is present ?
		else iResult = PlayMeleeAttack();

		SetPlayerEffects( ammo, firemode );
                    PlayAttackSound( firemode );
	}
	else if( !FStriCmp( ammo, "9mm" )) iResult = Shoot ( ammo, GetCurrentSpread(ammo), firemode );
	else if( !FStriCmp( ammo, "357" )) iResult = Shoot ( ammo, GetCurrentSpread(ammo), firemode );
	else if( !FStriCmp( ammo, "556" )) iResult = Shoot ( ammo, GetCurrentSpread(ammo), firemode );
	else if( !FStriCmp( ammo, "762" )) iResult = Shoot ( ammo, GetCurrentSpread(ammo), firemode );
	else if( !FStriCmp( ammo, "12mm" )) iResult = Shoot ( ammo, GetCurrentSpread(ammo), firemode );
	else if( !FStriCmp( ammo, "buckshot" ))
	{
		if(firemode) iResult = Shoot ( ammo, GetCurrentSpread(ammo), m_iClip == 1 ? 0 : 1, 2 );//HACK
		else iResult = Shoot ( ammo, GetCurrentSpread(ammo) );
	}
	else if( !FStriCmp( ammo, "rockets" ))	iResult = Launch( ammo, firemode );
	else if( !FStriCmp( ammo, "m203" ))	iResult = Launch( ammo, firemode );
	else if( !FStriCmp( ammo, "nuke" ))	iResult = Launch( ammo, firemode );
	else if( !FStriCmp( ammo, "bolts" ))	iResult = Launch( ammo, firemode ); 
	else if( !FStriCmp( ammo, "uranium" ))	iResult = -1;
	else if( !FStriCmp( ammo, "grenade" ))
	{
		if ( !m_flHoldTime && m_pPlayer->m_rgAmmo[ ReturnAmmoIndex( ammo ) ] > 0 )
		{
			m_flHoldTime = UTIL_WeaponTimeBase();// set hold time
			m_iOnControl = 3;		       // grenade "on control"
			SetAnimation( ACT_VM_START_CHARGE ); // pinpull
		}
		iResult = -1;
	}
	return iResult;
}

//=========================================================
//	Play Action
//=========================================================
int CBasePlayerWeapon :: PlayCurrentAttack( int action, int firemode )
{
	int iResult = 0;
	if(pev->button)firemode = m_iBody; // enable switching firemode	

	if( action == ZOOM ) iResult = 1; // See void ZoomUpdate( void ); for details
	else if( action == AMMO1 ) iResult = GetCurrentAttack( pszAmmo1(), firemode );
	else if( action == AMMO2 ) iResult = GetCurrentAttack( pszAmmo2(), firemode );
	else if( action == LASER_DOT )
	{
		m_iSpot = !m_iSpot;
		if( !m_iSpot && m_pSpot ) // disable laser dot
		{
			EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/spot_off.wav", 1, ATTN_NORM );
			m_pSpot->Killed();
			m_pSpot = NULL;
			m_iSkin = 0; // disable screen
		}
		iResult = 1;
	}
	else if ( action == FLASHLIGHT )
	{
		if(FBitSet(m_pPlayer->pev->effects, EF_DIMLIGHT))
		{
			ClearBits(m_pPlayer->pev->effects, EF_DIMLIGHT);
			EMIT_SOUND_DYN( ENT(m_pPlayer->pev), CHAN_WEAPON, SOUND_FLASHLIGHT_OFF, 1.0, ATTN_NORM, 0, PITCH_NORM );
		}
		else 
		{
			SetBits(m_pPlayer->pev->effects, EF_DIMLIGHT);
			EMIT_SOUND_DYN( ENT(m_pPlayer->pev), CHAN_WEAPON, SOUND_FLASHLIGHT_ON, 1.0, ATTN_NORM, 0, PITCH_NORM );
		}
		iResult = 1;
	}
	else if( action == SWITCHMODE)
	{
		if(!m_iBody)
		{
			m_iBody = 1;
			pev->button = 1;//enable custom firemode
			SetAnimation(ACT_VM_TURNON);
		}
		else
		{
			SetAnimation( ACT_VM_TURNOFF );
			pev->button = 0;//disable custom firemode
			m_iClientBody = m_iBody = 0;//make delay before change
		}
		iResult = 1;
	}
	else if( action == SWING)
	{
		if (!Swing(TRUE)) Swing (FALSE);
		iResult = 1;
	}
	else if( action == NONE) return -1;
	else //just command
	{
		char command[64];
		if(!strncmp( STRING(action), "fire ", 5))
		{
			char *target  = (char*)STRING(action);
			target = target + 5;//remove "fire "
			UTIL_FireTargets( target, m_pPlayer, this, USE_TOGGLE );//activate target
		}
		else
		{
			sprintf( command, "%s\n", STRING(action));
			SERVER_COMMAND( command );
		}		
		//just play animation and sound
		if(!firemode) iResult = PlayRangeAttack();//anim is present ?
		else iResult = PlayMeleeAttack();

		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );//player animation
                    PlayAttackSound( firemode );
	}
	return iResult;
}

int CBasePlayerWeapon :: UseAmmo( const char *ammo, int count )
{
	int ammoType = GetAmmoType( ammo );

	if( !ammoType ) return -1;//uncnown ammo ?
	else if( ammoType == 1)//it's primary ammo
	{
		if(iMaxClip() == -1 && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] > 0)//may be we have clip ?
		{
			if( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < count) count = 1;
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= count;			
		}
		else if(m_iClip > 0)//have clip 
		{
			if( m_iClip < count) count = 1;
			m_iClip -= count;
		}
		else return 0;//ammo is out
	}
	else if( ammoType == 2 ) //it's secondary ammo
	{
		if(m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] > 0) //we have ammo ?
		{
			if( m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] < count) count = 1;
			m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] -= count;
		}
		else return 0;//ammo is out
	}
	return 1;//thats all right, mr.freeman
}

void CBasePlayerWeapon :: SetPlayerEffects( const char *ammo, int firemode )
{
	if( !FStriCmp( ammo, "9mm" ))
	{
		if(firemode)//add silencer
		{
			m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
			m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;
		}
		else
		{
			m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
			m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
		}
	}
	else if( !FStriCmp( ammo, "762" )) //sniper rifle
	{
		m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = NO_GUN_FLASH;
	}
	else if( !FStriCmp( ammo, "buckshot" ))
	{
		if(firemode) //double barrels attack
		{
			m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
			m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
		}
		else
		{
			m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
			m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;
		}
	}
	else 
	{
		m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
		m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;
	}
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );//player animation
}

void CBasePlayerWeapon :: PlayAttackSound( int firemode )
{
	if(firemode) //play sfx random sounds 
	{
		if(sfxcnt())
		{
			int sfxsound = RANDOM_LONG(0, sfxcnt() - 1);
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, STRING(SfxSound(sfxsound)), 1.0, ATTN_NORM);
		}
	}
	else	//play normal random fire sound
	{
		if(sndcnt())
		{
			int firesound = RANDOM_LONG(0, sndcnt() - 1);
			EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, STRING(FireSound(firesound)), 1.0, ATTN_NORM);
		}
	}
}

//=========================================================
//	Shoot & Launch functions
//=========================================================
int CBasePlayerWeapon :: Shoot ( const char *ammo, Vector vecSpread, int firemode, int cShots )
{
	if ( m_pPlayer->pev->waterlevel != 3)//have ammo and player not underwater
	{
		if(!UseAmmo( ammo, cShots )) return 0;

		SetPlayerEffects( ammo, firemode );
                    PlayAttackSound( firemode );
	
		// viewmodel animation
		ZoomReset();
		if( !PlayEmptyFire( ))
		{
			if ( !firemode )
				PlayRangeAttack ();
			else PlayMeleeAttack ();
		}

		float flDistance;//set max distance
		
		Vector vecSrc = m_pPlayer->GetGunPosition( );
		Vector vecAiming, vecDir;

		if( iFlags() & ITEM_FLAG_USEAUTOAIM )
			vecAiming = m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );
		else vecAiming = gpGlobals->v_forward;

		// eject brass
		for( int i = 0; cShots > i; i++ )
		{
			UTIL_PlaybackEvent( 0, m_pPlayer->edict(), m_usEjectBrass, 0.0, g_vecZero, g_vecZero, 0, 0, GetBulletType( ammo ), 0, firemode, 0 );
		}
		if( !FStriCmp( ammo, "buckshot" ))//HACK 
		{
			flDistance = 2048;
			cShots *= 4;
		}
		else flDistance = MAP_SIZE;
		FireBullets( cShots, vecSrc, vecAiming, vecSpread, flDistance, (Bullet)GetBulletType(ammo), 0, 0, m_pPlayer->pev );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_FLOAT ( 10, 15 );
		return 1;
	}
	return 0;
}

int CBasePlayerWeapon :: Launch ( const char *ammo, int type )
{
	if(!UseAmmo(ammo)) return 0;
          float fps = 0;//custom animation speed for handgrenade

	//viewmodel animation
	ZoomReset();
          
	if ( !FStriCmp( ammo, "m203" )) 
	{
		// we don't add in player velocity anymore.
         		UTIL_MakeVectors( m_pPlayer->pev->viewangles + m_pPlayer->pev->punchangle );
		CGrenade::ShootContact( m_pPlayer->pev, m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16, gpGlobals->v_forward * 800 );
	}	
	else if( !FStriCmp( ammo, "rockets" ))
	{
		UTIL_MakeVectors( m_pPlayer->pev->viewangles );
		Vector vecSrc = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -8;

		CRpgRocket *pRocket = CRpgRocket::Create ( vecSrc, m_pPlayer->pev->viewangles, m_pPlayer, this );
		UTIL_MakeVectors( m_pPlayer->pev->viewangles );// RpgRocket::Create stomps on globals, so remake.
		pRocket->pev->velocity = pRocket->pev->velocity + gpGlobals->v_forward * DotProduct( m_pPlayer->pev->velocity, gpGlobals->v_forward );
	} 
	else if( !FStriCmp( ammo, "bolts" ))
	{
		// we don't add in player velocity anymore.
		Vector anglesAim = m_pPlayer->pev->viewangles + m_pPlayer->pev->punchangle;
		UTIL_MakeVectors( anglesAim );
	
		anglesAim.x = -anglesAim.x;
		Vector vecSrc = m_pPlayer->GetGunPosition( ) - gpGlobals->v_up * 2;
		Vector vecDir = gpGlobals->v_forward;

		CBolt *pBolt = CBolt::Create( vecSrc, anglesAim, m_pPlayer );
		if (m_pPlayer->pev->waterlevel == 3) pBolt->pev->velocity = vecDir * 1000;
		else pBolt->pev->velocity = vecDir * 2000;
	}
	else if( !FStriCmp( ammo, "nuke" ))
	{
		UTIL_MakeVectors( m_pPlayer->pev->viewangles );
		Vector vecSrc = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_up * 2;
		CWHRocket *pRocket = CWHRocket::Create ( vecSrc, m_pPlayer->pev->viewangles, m_pPlayer, this, type );
	}
	else if( !FStriCmp( ammo, "grenade" ))
	{
		Vector angThrow = m_pPlayer->pev->viewangles + m_pPlayer->pev->punchangle;

		if ( angThrow.x < 0 ) angThrow.x = -10 + angThrow.x * ( ( 90 - 10 ) / 90.0 );
		else angThrow.x = -10 + angThrow.x * ( ( 90 + 10 ) / 90.0 );

		float flVel = ( 90 - angThrow.x ) * 4;
		if ( flVel > 500 ) flVel = 500;

		UTIL_MakeVectors( angThrow );
		Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16;
		Vector vecThrow = gpGlobals->v_forward * flVel + m_pPlayer->pev->velocity;

		// alway explode 3 seconds after the pin was pulled
		float time = m_flHoldTime - UTIL_WeaponTimeBase() + 3.0;
		if (time < 0) time = 0;

		CGrenade::ShootTimed( m_pPlayer->pev, vecSrc, vecThrow, time );
		fps = flVel * 0.08;
		if(fps < 12) fps = 12;
		
		m_flHoldTime = 0;
		m_iOnControl = 4;
	}		

	m_pPlayer->m_iWeaponVolume = LOUD_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = BRIGHT_GUN_FLASH;

	m_pPlayer->m_iExtraSoundTypes = bits_SOUND_DANGER;
	m_pPlayer->m_flStopExtraSoundTime = UTIL_WeaponTimeBase() + 0.2;

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
 
	//play random shooting animation
	if(!PlayEmptyFire()) PlayMeleeAttack( fps );

	return 1;
}

int CBasePlayerWeapon::Swing( int fFirst )
{
	int fDidHit = FALSE;
	BOOL bHit = FALSE;
	TraceResult tr;
          TraceResult m_trHit;
          
	if ( m_flTimeUpdate > UTIL_WeaponTimeBase() ) return fDidHit;

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 3;
          
	UTIL_MakeVectors( m_pPlayer->pev->viewangles );
	Vector vecSrc = m_pPlayer->GetGunPosition( );
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				UTIL_FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	if ( tr.flFraction >= 1.0)
	{
		if(fFirst)
		{
			switch( (g_iSwing++) % 3 )
			{
				case 0: SetAnimation ( ACT_VM_MELEE_ATTACK1 ); break;
				case 1: SetAnimation ( ACT_VM_MELEE_ATTACK2 ); break;
				case 2: SetAnimation ( ACT_VM_MELEE_ATTACK3 ); break;
			}
			
			if(EmptySnd()) EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_WEAPON, STRING(EmptySnd()), 1, ATTN_NORM, 0, 94 + RANDOM_LONG(0,0xF));
			m_flNextPrimaryAttack =  m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		}
	}
	else
	{
		// hit
		fDidHit = TRUE;
		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		switch( (g_iSwing++) % 3 )
		{
			case 0: SetAnimation ( ACT_VM_RANGE_ATTACK1 ); break;
			case 1: SetAnimation ( ACT_VM_RANGE_ATTACK2 ); break;
			case 2: SetAnimation ( ACT_VM_RANGE_ATTACK3 ); break;
		}		

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		ClearMultiDamage( );
		pEntity->TraceAttack(m_pPlayer->pev, CROWBAR_DMG, gpGlobals->v_forward, &tr, DMG_CLUB ); 
		ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );

		m_flNextPrimaryAttack =  m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.25;

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		int fHitWorld = TRUE;

		if (pEntity)
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				if(sndcnt())//HitBody sound
				{
					int firesound = RANDOM_LONG(0, sndcnt() - 1);
					EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_ITEM, STRING(FireSound(firesound)), 1, ATTN_NORM);
				}
				m_pPlayer->m_iWeaponVolume = CROWBAR_BODYHIT_VOLUME;
				if ( !pEntity->IsAlive() )
				{
					m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
					return TRUE;
				}
				else flVol = 0.1;
				fHitWorld = FALSE;
			}
		}

		if (fHitWorld)
		{
			float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd-vecSrc)*2, BULLET_CROWBAR);

			if ( IsMultiplayer() ) fvolbar = 1;

			// also play crowbar strike
			if(sndcnt())//CBAR Hit sound
			{
				int sfxsound = RANDOM_LONG(0, sfxcnt() - 1);
				EMIT_SOUND_DYN(ENT(m_pPlayer->pev), CHAN_ITEM, STRING(SfxSound(sfxsound)), fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
			}
		}

		// delay the decal a bit
		m_trHit = tr;
		
		m_pPlayer->m_iWeaponVolume = flVol * CROWBAR_WALLHIT_VOLUME;

		DecalGunshot( &m_trHit, BULLET_CROWBAR );
		m_flTimeUpdate = UTIL_WeaponTimeBase() + 0.2;	
	}
	return fDidHit;
}

//=========================================================
//	Zoom In\Out
//=========================================================
void CBasePlayerWeapon::ZoomUpdate( void )
{
	if(( iAttack1() == ZOOM && m_pPlayer->pev->button & IN_ATTACK ) || ( iAttack2() == ZOOM && m_pPlayer->pev->button & IN_ATTACK2 ))
	{
		if( m_iZoom == 0 )
		{
			if( m_flHoldTime > UTIL_WeaponTimeBase( )) return;
			m_iZoom = 1;
			EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/zoom.wav", 1, ATTN_NORM );
			m_flHoldTime = UTIL_WeaponTimeBase() + 0.8;
		}
		if( m_iZoom == 1 )
		{
			m_pPlayer->pev->fov = 50.0f;
			m_pPlayer->pev->viewmodel = NULL;
			m_iZoom = 2; // ready to zooming, wait for 0.8 secs
		}
		if( m_iZoom == 2 && m_pPlayer->pev->fov > MAX_ZOOM )
		{
			if( m_flHoldTime < UTIL_WeaponTimeBase( ))
			{
				m_pPlayer->pev->fov = UTIL_Approach( 20.0f, m_pPlayer->pev->fov, 0.8f );
				m_flHoldTime = UTIL_WeaponTimeBase();
			}
		}
		if( m_iZoom == 3 ) ZoomReset();
	}
	else if( m_iZoom > 1 ) m_iZoom = 3;
	
	MESSAGE_BEGIN( MSG_ONE, gmsg.ZoomHUD, NULL, m_pPlayer->pev );
		WRITE_BYTE( m_iZoom );
	MESSAGE_END();
}

void CBasePlayerWeapon::ZoomReset( void )
{
	// return viewmodel
	if( m_iZoom )
	{
		m_pPlayer->pev->viewmodel = iViewModel();
		m_flHoldTime = UTIL_WeaponTimeBase() + 0.5;
		m_pPlayer->pev->fov = 90.0f;
		m_iZoom = 0; // clear zoom
		MESSAGE_BEGIN( MSG_ONE, gmsg.ZoomHUD, NULL, m_pPlayer->pev );
			WRITE_BYTE( m_iZoom );
		MESSAGE_END();
		m_pPlayer->UpdateClientData(); // update client data manually
	}
}

//=========================================================
//	Virtual base functions
//=========================================================
void CBasePlayerWeapon :: Deploy( void )
{                              
	m_iStepReload = 0;
	if( iMaxClip() && m_iClip <= 0 )		 // weapon have clip and clip is empty ?
	{
		if(!DefaultDeploy( ACT_VM_DEPLOY_EMPTY ))// try to playing "deploy_empty" anim
			DefaultDeploy( ACT_VM_DEPLOY );// custom animation not found, play standard animation
	}
	else DefaultDeploy( ACT_VM_DEPLOY );		 // just playing standard anim
}

void CBasePlayerWeapon :: Holster( void )
{                              
	if( iMaxClip() && m_iClip <= 0 )		 // weapon have clip and clip is empty ?
	{         
		if(!DefaultHolster(ACT_VM_HOLSTER_EMPTY))// try to playing "holster_empty" anim
			DefaultHolster( ACT_VM_HOLSTER );
	}
	else DefaultHolster(ACT_VM_HOLSTER);		 // just playing standard anim
}

//=========================================================
//	NOT USED:HoldDown weapon
//=========================================================
int CBasePlayerWeapon :: FoundAlly( void )
{
	CBaseEntity *pEntity = UTIL_FindEntityForward( m_pPlayer );
	if( pEntity )
	{
		CBaseMonster *pMonster = pEntity->MyMonsterPointer();
		if( pMonster )
		{
			int m_class = pMonster->Classify();
			if( m_class == CLASS_PLAYER_ALLY || m_class == CLASS_HUMAN_PASSIVE )
			{
				return 1;
			}
		}
	}
	return 0;
}

//=========================================================
//	Weapon Frame - main function
//=========================================================
void CBasePlayerWeapon::ItemPreFrame( void )
{

}

void CBasePlayerWeapon::ItemPostFrame( void )
{
	if( !b_restored )
	{
		m_iClientSkin = -1; // reset last skin info for new weapon
		m_iClientBody = -1; // reset last body info for new weapon
		m_iClientFov = -1;	// reset last fov info for new weapon

		if( iMaxClip() && !m_iClip )
			SetAnimation( ACT_VM_IDLE_EMPTY ); // restore last animation
		else SetAnimation( ACT_VM_IDLE1 );
		b_restored = TRUE;
	}
	if( !m_iSpot && m_pSpot ) // disable laser dot
	{
		m_iSkin = 0; // disable screen
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/spot_off.wav", 1, ATTN_NORM );
		m_pSpot->Killed();
		m_pSpot = NULL;
	}
	if( m_iSpot && !m_pSpot ) // enable laser dot
	{
		m_pSpot = CLaserSpot::CreateSpot( m_pPlayer->pev );
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/spot_on.wav", 1, ATTN_NORM );
		m_iSkin = 1; // enable screen
	}
          if( m_pSpot ) m_pSpot->Update( m_pPlayer );

          ZoomUpdate(); // update zoom

	if( !m_iStepReload ) m_fInReload = FALSE; // reload done          
	if( m_flTimeUpdate < UTIL_WeaponTimeBase()) PostIdle(); // catch all
	if( m_fInReload && m_pPlayer->m_flNextAttack <= UTIL_WeaponTimeBase() )
	{
		if( m_iStepReload > 2 )
		{
			// complete the reload. 
			int j = min( iMaxClip() - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]);	

			// add them to the clip
			m_iClip += j;
			m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= j;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.01; // play PostReload
			// m_iStepReload = 1;
			m_fInReload = FALSE;
		}
	}

	if((m_pPlayer->pev->button & IN_ATTACK2) && CanAttack( m_flNextSecondaryAttack ) && !m_iOnControl )
	{
		SecondaryAttack();
		m_pPlayer->pev->button &= ~IN_ATTACK2;
	}
	else if((m_pPlayer->pev->button & IN_ATTACK) && CanAttack( m_flNextPrimaryAttack ))
	{
		if( m_iOnControl == 1 ) // destroy controllable rocket
		{
			m_iOnControl = 2; // destroy nuke
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 1.5;
			return;
		}
		PrimaryAttack();
		m_pPlayer->pev->button &= ~IN_ATTACK;
	}
	else if( m_pPlayer->pev->button & IN_RELOAD && iMaxClip() != WEAPON_NOCLIP && !m_fInReload ) 
	{
		// reload when reload is pressed, or if no buttons are down and weapon is empty.
		Reload();
	}
	else if(!(m_pPlayer->pev->button & (IN_ATTACK|IN_ATTACK2)))
	{
		// play sequence holster/deploy if player find or lose suit
		if((PLAYER_HAS_SUIT && !PLAYER_DRAW_SUIT) || (!PLAYER_HAS_SUIT && PLAYER_DRAW_SUIT))
		{
			m_pPlayer->m_pActiveItem->Holster( );
			m_pPlayer->QueueItem( this );
			if( m_pPlayer->m_pActiveItem ) m_pPlayer->m_pActiveItem->Deploy( );
		}
		if( !CanDeploy() && m_flNextPrimaryAttack < gpGlobals->time ) 
		{
			// weapon isn't useable, switch.
			if( !(iFlags() & ITEM_FLAG_NOAUTOSWITCHEMPTY) && g_pGameRules->GetNextBestWeapon( m_pPlayer, this ))
			{
				m_flNextPrimaryAttack = gpGlobals->time + 0.3;
				return;
			}
		}
		else
		{
			// weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
			if( m_iClip == 0 && !(iFlags() & ITEM_FLAG_NOAUTORELOAD) && m_flNextPrimaryAttack < gpGlobals->time )
			{
				Reload();
				return;
			}
		}
		m_iPlayEmptySound = 1; // reset empty sound
		if( iFlags() & ITEM_FLAG_USEAUTOAIM )
			m_pPlayer->GetAutoaimVector( AUTOAIM_5DEGREES );

		if(m_flTimeWeaponIdle < UTIL_WeaponTimeBase()) // step reload
		{
			if (m_iStepReload > 0 )
			{
				if (m_iClip != iMaxClip() && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
				{
					Reload();
				}
				else
				{
					// reload debounce has timed out
					if( IsEmptyReload())
					{
						if( SetAnimation( ACT_VM_PUMP_EMPTY ) == -1 )
							SetAnimation( ACT_VM_PUMP );
					}
					else SetAnimation( ACT_VM_PUMP );
					if( iFlags() & ITEM_FLAG_HIDEAMMO )
						m_iBody = 0; // reset ammo
					PostReload(); // post effects
					m_iStepReload = 0;
				}
			}
			else if( m_iOnControl > 3 ) // item deploy
			{
				m_iOnControl = 0;
				if( m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] )
				{
					SetAnimation( ACT_VM_DEPLOY );
					m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_LONG( 10, 15 );
				}
				else Holster();
			}
			else WeaponIdle();
		}
	}
}

int CBasePlayerWeapon::UpdateClientData( CBasePlayer *pPlayer )
{
	BOOL bSend = FALSE;
	int state = 0;

	// update weapon body
	if( m_iClientBody != m_iBody )
	{
		pev->body = (pev->body % NUM_HANDS) + NUM_HANDS * m_iBody;	// calculate body
		MESSAGE_BEGIN( MSG_ONE, gmsg.SetBody, NULL, m_pPlayer->pev );
			WRITE_BYTE( pev->body ); // weaponmodel body
		MESSAGE_END();
		m_iClientBody = m_iBody;//synched
	}

	// update weapon skin
	if( m_iClientSkin != m_iSkin )
	{
		pev->skin = m_iSkin;				// calculate skin
		MESSAGE_BEGIN( MSG_ONE, gmsg.SetSkin, NULL, m_pPlayer->pev );
			WRITE_BYTE( pev->skin ); //weaponmodel skin.
		MESSAGE_END();
		m_iClientSkin = m_iSkin;//synched
	}

	if( pPlayer->m_pActiveItem == this )
	{
		if( pPlayer->m_fOnTarget )
			state = 0x40; // weapon is ontarget
		else state = 1;
	}

	// forcing send of all data!
	if( !pPlayer->m_fWeapon ) bSend = TRUE;
	
	// This is the current or last weapon, so the state will need to be updated
	if( this == pPlayer->m_pActiveItem || this == pPlayer->m_pClientActiveItem )
	{
		if( pPlayer->m_pActiveItem != pPlayer->m_pClientActiveItem ) bSend = TRUE;
	}

	// if the ammo, state, or fov has changed, update the weapon
	if( m_iClip != m_iClientClip || state != m_iClientWeaponState || Q_rint( m_iClientFov ) != Q_rint( m_pPlayer->pev->fov ))
	{
		bSend = TRUE;
	}

	if( bSend )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsg.CurWeapon, NULL, pPlayer->pev );
			WRITE_BYTE( state );
			WRITE_BYTE( m_iId );
			WRITE_BYTE( m_iClip );
		MESSAGE_END();

		m_iClientFov = m_pPlayer->pev->fov;
		m_iClientClip = m_iClip;
		m_iClientWeaponState = state;
		pPlayer->m_fWeapon = TRUE;
	}

	if( m_pNext ) m_pNext->UpdateClientData( pPlayer );
	return 1;
}

void CBasePlayerWeapon :: SetNextThink( float delay )
{
	m_fNextThink = UTIL_WeaponTimeBase() + delay;
	pev->nextthink = m_fNextThink;
}

//=========================================================
//	Attach\drop operations
//=========================================================
void CBasePlayerWeapon::DestroyItem( void )
{
	// if attached to a player, remove. 
	if ( m_pPlayer ) m_pPlayer->RemovePlayerItem( this );
	Drop();
}

void CBasePlayerWeapon::Drop( void )
{
	SetTouch( NULL );
	SetThink( Remove );
	SetNextThink( 0.1 );
}

void CBasePlayerWeapon::AttachToPlayer( CBasePlayer *pPlayer )
{
	SetObjectClass( ED_STATIC );		// disable client updates

	// make cross-links for consitency
	pev->aiment = pPlayer->edict();
	pev->owner = pPlayer->edict();

	pev->movetype = MOVETYPE_FOLLOW;
	pev->solid = SOLID_NOT;
	pev->effects |= EF_NODRAW; 
	pev->modelindex = 0;		// server won't send down to clients if modelindex == 0
	pev->targetname = iStringNull;	// don't try remove this weapon from map
	pev->model = iStringNull;
	SetNextThink( 0.1 );
	SetTouch( NULL );
	SetThink( NULL );
}

int CBasePlayerWeapon::AddDuplicate( CBasePlayerWeapon *pOriginal )
{
	if(iFlags() & ITEM_FLAG_NODUPLICATE) return FALSE;
	
	if ( m_iDefaultAmmo || m_iDefaultAmmo2 )
		return ExtractAmmo( (CBasePlayerWeapon *)pOriginal );
	return ExtractClipAmmo( (CBasePlayerWeapon *)pOriginal );
}

int CBasePlayerWeapon::AddToPlayer( CBasePlayer *pPlayer )
{
	m_pPlayer = pPlayer; // Give global pointer to player
	pPlayer->pev->weapons |= (1<<m_iId);

	if( !m_iPrimaryAmmoType || !m_iSecondaryAmmoType )
	{
		m_iPrimaryAmmoType =   pPlayer->GetAmmoIndex( pszAmmo1() );
		m_iSecondaryAmmoType = pPlayer->GetAmmoIndex( pszAmmo2() );
	}

	MESSAGE_BEGIN( MSG_ONE, gmsg.WeapPickup, NULL, pPlayer->pev );
		WRITE_BYTE( m_iId );
	MESSAGE_END();

	return AddWeapon();
}

//=========================================================
//		Ammo base operations
//=========================================================
BOOL CBasePlayerWeapon :: AddPrimaryAmmo( int iCount, char *szName, int iMaxClip, int iMaxCarry )
{
	int iIdAmmo;
	if (iMaxClip < 1)
	{
		m_iClip = -1;
		iIdAmmo = m_pPlayer->GiveAmmo( iCount, szName, iMaxCarry );
	}
	else if (m_iClip == 0 ) //add ammo in clip only for new weapons
	{
		int i;
		i = min( m_iClip + iCount, iMaxClip ) - m_iClip;
		m_iClip += i;
		iIdAmmo = m_pPlayer->GiveAmmo( iCount - i, szName, iMaxCarry );
	}
	else iIdAmmo = m_pPlayer->GiveAmmo( iCount, szName, iMaxCarry );

	if (iIdAmmo > 0)
	{
		m_iPrimaryAmmoType = iIdAmmo;
		if( m_pPlayer->HasPlayerItem( this ))
			EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/glock/clip_in.wav", 1, ATTN_NORM );
	}
	return iIdAmmo > 0 ? TRUE : FALSE;
}

BOOL CBasePlayerWeapon :: AddSecondaryAmmo( int iCount, char *szName, int iMax )
{
	int iIdAmmo;

	iIdAmmo = m_pPlayer->GiveAmmo( iCount, szName, iMax );

	if (iIdAmmo > 0)
	{
		m_iSecondaryAmmoType = iIdAmmo;
		EMIT_SOUND( ENT( m_pPlayer->pev ), CHAN_ITEM, "weapons/glock/clip_in.wav", 1, ATTN_NORM );
	}
	return iIdAmmo > 0 ? TRUE : FALSE;
}

int CBasePlayerWeapon::ExtractAmmo( CBasePlayerWeapon *pWeapon )
{
	int iReturn;
	if ( pszAmmo1() != NULL )
	{
		iReturn = pWeapon->AddPrimaryAmmo( m_iDefaultAmmo, (char *)pszAmmo1(), iMaxClip(), iMaxAmmo1() );
		m_iDefaultAmmo = 0;
	}

	if ( pszAmmo2() != NULL )
	{
		iReturn = pWeapon->AddSecondaryAmmo( m_iDefaultAmmo2, (char *)pszAmmo2(), iMaxAmmo2() );
	}
	return iReturn;
}

int CBasePlayerWeapon::ExtractClipAmmo( CBasePlayerWeapon *pWeapon )
{
	int iAmmo;

	if ( m_iClip == WEAPON_NOCLIP ) iAmmo = 0;// guns with no clips always come empty if they are second-hand
	else iAmmo = m_iClip;
	return pWeapon->m_pPlayer->GiveAmmo( iAmmo, (char *)pszAmmo1(), iMaxAmmo1() );
}

void CBasePlayerWeapon :: SetObjectCollisionBox( void )
{
	pev->absmin = pev->origin + Vector(-24, -24, 0);
	pev->absmax = pev->origin + Vector(24, 24, 16); 
}       