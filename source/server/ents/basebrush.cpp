//=======================================================================
//			Copyright (C) Shambler Team 2004
//		         basebreak.cpp - base for all brush
//				entities. 			    
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "sfx.h"
#include "cbase.h"
#include "decals.h"
#include "client.h"
#include "soundent.h"
#include "saverestore.h"
#include "gamerules.h"
#include "basebrush.h"
#include "defaults.h"
#include "player.h"

//=======================================================================
// 			STATIC BREACABLE BRUSHES
//=======================================================================
#define DAMAGE_SOUND(x, y, z)EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, x[RANDOM_LONG(0, ARRAYSIZE(x)-1)], y, ATTN_NORM, 0, z)

//spawn objects
const char *CBaseBrush::pSpawnObjects[] =
{
	NULL,			// 0
	"item_battery",		// 1
	"item_healthkit",		// 2
	"weapon_9mmhandgun",	// 3
	"ammo_9mmclip",		// 4
	"weapon_9mmAR",		// 5
	"ammo_9mmAR",		// 6
	"ammo_m203",		// 7
	"weapon_shotgun",		// 8
	"ammo_buckshot",		// 9
	"weapon_crossbow",		// 10
	"ammo_crossbow",		// 11
	"weapon_357",		// 12
	"ammo_357",		// 13
	"weapon_rpg",		// 14
	"ammo_rpgclip",		// 15
	"ammo_gaussclip",		// 16
	"weapon_handgrenade",	// 17
	"weapon_gauss",		// 18
};

//material sounds
const char *CBaseBrush::pSoundsWood[] =
{
	"materials/wood/wood1.wav",
	"materials/wood/wood2.wav",
	"materials/wood/wood3.wav",
	"materials/wood/wood4.wav",
};

const char *CBaseBrush::pSoundsFlesh[] =
{
	"materials/flesh/flesh1.wav",
	"materials/flesh/flesh2.wav",
	"materials/flesh/flesh3.wav",
	"materials/flesh/flesh4.wav",
	"materials/flesh/flesh5.wav",
	"materials/flesh/flesh6.wav",
	"materials/flesh/flesh7.wav",
	"materials/flesh/flesh8.wav",
};

const char *CBaseBrush::pSoundsMetal[] =
{
	"materials/metal/metal1.wav",
	"materials/metal/metal2.wav",
	"materials/metal/metal3.wav",
	"materials/metal/metal4.wav",
	"materials/metal/metal5.wav",
};

const char *CBaseBrush::pSoundsCrete[] =
{
	"materials/crete/concrete1.wav",
	"materials/crete/concrete2.wav",
	"materials/crete/concrete3.wav",
	"materials/crete/concrete4.wav",
	"materials/crete/concrete5.wav",
	"materials/crete/concrete6.wav",
	"materials/crete/concrete7.wav",
	"materials/crete/concrete8.wav",
};

const char *CBaseBrush::pSoundsGlass[] =
{
	"materials/glass/glass1.wav",
	"materials/glass/glass2.wav",
	"materials/glass/glass3.wav",
	"materials/glass/glass4.wav",
	"materials/glass/glass5.wav",
	"materials/glass/glass6.wav",
	"materials/glass/glass7.wav",
	"materials/glass/glass8.wav",
};

const char *CBaseBrush::pSoundsCeil[] =
{
	"materials/ceil/ceiling1.wav",
	"materials/ceil/ceiling2.wav",
	"materials/ceil/ceiling3.wav",
	"materials/ceil/ceiling4.wav",
};

const char **CBaseBrush::MaterialSoundList( Materials precacheMaterial, int &soundCount )
{
	const char **pSoundList = NULL;

	switch ( precacheMaterial )
	{
	case None:
		soundCount = 0;
		break;
	case Bones:
	case Flesh:
		pSoundList = pSoundsFlesh;
		soundCount = ARRAYSIZE(pSoundsFlesh);
		break;	
	case CinderBlock:
	case Concrete:
	case Rocks:
		pSoundList = pSoundsCrete;
		soundCount = ARRAYSIZE(pSoundsCrete);
		break;
	case Glass:
	case Computer:
	case UnbreakableGlass:
		pSoundList = pSoundsGlass;
		soundCount = ARRAYSIZE(pSoundsGlass);
		break;
	case MetalPlate:
	case AirDuct:
	case Metal:
		pSoundList = pSoundsMetal;
		soundCount = ARRAYSIZE(pSoundsMetal);
		break;
	case CeilingTile:
		pSoundList = pSoundsCeil;
		soundCount = ARRAYSIZE(pSoundsCeil);
		break;
	case Wood:
		pSoundList = pSoundsWood;
		soundCount = ARRAYSIZE(pSoundsWood);
		break;
	default:
		soundCount = 0;
		break;
	}
	return pSoundList;
}

void CBaseBrush::MaterialSoundPrecache( Materials precacheMaterial )
{
	const char **pSoundList;
	int i, soundCount = 0;

	pSoundList = MaterialSoundList( precacheMaterial, soundCount );
	for ( i = 0; i < soundCount; i++ ) UTIL_PrecacheSound( (char *)pSoundList[i] );
}

void CBaseBrush::PlayRandomSound( edict_t *pEdict, Materials soundMaterial, float volume )
{
	const char **pSoundList;
	int soundCount = 0;

	pSoundList = MaterialSoundList( soundMaterial, soundCount );
	if ( soundCount ) EMIT_SOUND( pEdict, CHAN_BODY, pSoundList[ RANDOM_LONG(0, soundCount-1) ], volume, 1.0 );
}

void CBaseBrush :: AxisDir( void )
{
	//make backward compatibility
	if ( pev->movedir != g_vecZero) return;

	//Don't change this!
	if ( FBitSet(pev->spawnflags, SF_BRUSH_ROTATE_Z_AXIS))
		pev->movedir = Vector ( 0, 0, 1 );	// around z-axis
	else if ( FBitSet(pev->spawnflags, SF_BRUSH_ROTATE_X_AXIS))
		pev->movedir = Vector ( 1, 0, 0 );	// around x-axis
	else	pev->movedir = Vector ( 0, 1, 0 );	// around y-axis
}

void CBaseBrush::KeyValue( KeyValueData* pkvd )
{
	//as default all brushes can select material
	//and set strength (0 = unbreakable)
	if (FStrEq(pkvd->szKeyName, "material"))
	{
		int i = atoi( pkvd->szValue);

		if ((i < 0) || (i >= LastMaterial))
			m_Material = Wood;
		else	m_Material = (Materials)i;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "strength") )
	{
		pev->health = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "spawnobject") )
	{
		int namelen = strlen(pkvd->szValue) - 1;
                    int obj = atoi( pkvd->szValue );
		
		//custom spawn object
		if(namelen > 2) m_iSpawnObject = ALLOC_STRING( pkvd->szValue );
		else if ( obj > 0 && obj < ARRAYSIZE(pSpawnObjects))
			m_iSpawnObject = MAKE_STRING( pSpawnObjects[obj] );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "gibmodel"))
	{
		m_iGibModel = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "volume"))
	{
		//0.0 - silence, 1.0 - loudest( obsolete )
		m_flVolume = atof(pkvd->szValue);
		if (m_flVolume > 1.0) m_flVolume = 1.0;
		if (m_flVolume < 0.0) m_flVolume = 0.0;
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "movesound") || FStrEq(pkvd->szKeyName, "movesnd"))
	{
		m_iMoveSound = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "stopsound") || FStrEq(pkvd->szKeyName, "stopsnd"))
	{
		m_iStopSound = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "contents"))
	{
		pev->skin = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else	CBaseLogic::KeyValue( pkvd );
}

//Base functions
TYPEDESCRIPTION CBaseBrush::m_SaveData[] =
{
	DEFINE_FIELD( CBaseBrush, m_flVolume, FIELD_FLOAT ),	//volume of sounds
	DEFINE_FIELD( CBaseBrush, m_pitch, FIELD_FLOAT ),		//pitch of sound	
	DEFINE_FIELD( CBaseBrush, m_Material, FIELD_INTEGER ),	//brush material
	DEFINE_FIELD( CBaseBrush, m_iMagnitude, FIELD_INTEGER ),	//explosion magnitude
	DEFINE_FIELD( CBaseBrush, m_iMoveSound, FIELD_STRING ),	//sound scheme like Quake
	DEFINE_FIELD( CBaseBrush, m_iStartSound, FIELD_STRING ),	//sound scheme like Quake
	DEFINE_FIELD( CBaseBrush, m_iStopSound, FIELD_STRING ),     //sound scheme like Quake
	DEFINE_FIELD( CBaseBrush, m_iSpawnObject, FIELD_STRING ),	//spawnobject index
	DEFINE_FIELD( CBaseBrush, m_iGibModel, FIELD_STRING ),	//custom gibname
	DEFINE_FIELD( CBaseBrush, m_vecPlayerPos, FIELD_VECTOR ),	//for controllable entity like tank
	DEFINE_FIELD( CBaseBrush, m_pController, FIELD_CLASSPTR ),	//for controllable entity like tank	
};
IMPLEMENT_SAVERESTORE( CBaseBrush, CBaseLogic );

void CBaseBrush::Spawn( void )
{
    	Precache();

	if (!m_flVolume)m_flVolume = 1.0;//just enable full volume
	          
	//breacable brush (if mapmaker just set material - just play material sound)
	if(IsBreakable())
		pev->takedamage	= DAMAGE_YES;
	else	pev->takedamage	= DAMAGE_NO;

	pev->solid		= SOLID_BSP;
	pev->movetype		= MOVETYPE_PUSH;
	if (!m_pParent)pev->flags |= FL_WORLDBRUSH;
}

void CBaseBrush::Precache( void )
{
	const char *pGibName = "";

	switch (m_Material)
	{
	case Bones:
		pGibName = "models/gibs/bones.mdl";
		break;
	case Flesh:
		pGibName = "models/gibs/flesh.mdl";
		UTIL_PrecacheSound("materials/flesh/bustflesh1.wav");
		UTIL_PrecacheSound("materials/flesh/bustflesh2.wav");
		break;				
	case Concrete:
		pGibName = "models/gibs/concrete.mdl";
		UTIL_PrecacheSound("materials/crete/bustcrete1.wav");
		UTIL_PrecacheSound("materials/crete/bustcrete2.wav");
		break;
	case Rocks:
		pGibName = "models/gibs/rock.mdl";
		UTIL_PrecacheSound("materials/crete/bustcrete1.wav");
		UTIL_PrecacheSound("materials/crete/bustcrete2.wav");
		break;
	case CinderBlock:
		pGibName = "models/gibs/cinder.mdl";
		UTIL_PrecacheSound("materials/crete/bustcrete1.wav");
		UTIL_PrecacheSound("materials/crete/bustcrete2.wav");
		break;
	case Computer:
		pGibName = "models/gibs/computer.mdl";
		UTIL_PrecacheSound("materials/metal/bustmetal1.wav");
		UTIL_PrecacheSound("materials/metal/bustmetal2.wav");
		break;
	case Glass:
	case UnbreakableGlass:
		pGibName = "models/gibs/glass.mdl";
		UTIL_PrecacheSound("materials/glass/bustglass1.wav");
		UTIL_PrecacheSound("materials/glass/bustglass2.wav");
		break;
	case MetalPlate:
		pGibName = "models/gibs/metalplate.mdl";
		UTIL_PrecacheSound("materials/metal/bustmetal1.wav");
		UTIL_PrecacheSound("materials/metal/bustmetal2.wav");
		break;
	case Metal:
		pGibName = "models/gibs/metal.mdl";
		UTIL_PrecacheSound("materials/metal/bustmetal1.wav");
		UTIL_PrecacheSound("materials/metal/bustmetal2.wav");
		break;
	case AirDuct:
		pGibName = "models/gibs/vent.mdl";
		UTIL_PrecacheSound("materials/metal/bustmetal1.wav");
		UTIL_PrecacheSound("materials/metal/bustmetal2.wav");
		break;
	case CeilingTile:
		pGibName = "models/gibs/ceiling.mdl";
		UTIL_PrecacheSound("materials/ceil/bustceiling1.wav");
		UTIL_PrecacheSound("materials/ceil/bustceiling2.wav");
		break;
	case Wood:
		pGibName = "models/gibs/wood.mdl";
		UTIL_PrecacheSound("materials/wood/bustcrate1.wav");
		UTIL_PrecacheSound("materials/wood/bustcrate2.wav");
		break;
	case None:
	default:
		if(pev->health > 0)//mapmaker forget set material ?
		{
			DevMsg("\n======/Xash SmartFiled System/======\n\n");
			DevMsg("Please set material for %s,\n", STRING(pev->classname));
			DevMsg("if we want make this breakable\n");
		}
		break;
	}

	MaterialSoundPrecache( m_Material );
	if(IsBreakable())
	{
		m_idShard = UTIL_PrecacheModel( m_iGibModel, (char*)pGibName );//precache model
		if(m_iSpawnObject)UTIL_PrecacheEntity( m_iSpawnObject );
	}
	UTIL_PrecacheModel( pev->model );//can use *.mdl for any brush
}

void CBaseBrush :: DamageSound( void )
{
	float fvol;
	int pitch;

	if (RANDOM_LONG(0,2)) pitch = PITCH_NORM;
	else	pitch = 95 + RANDOM_LONG(0,34);

	fvol = RANDOM_FLOAT(m_flVolume/1.5, m_flVolume);

	switch (m_Material)
	{
	case None: break;
	case Bones:
	case Flesh:
		DAMAGE_SOUND(pSoundsFlesh, fvol, pitch );
		break;
	case CinderBlock:
	case Concrete:
	case Rocks:
		DAMAGE_SOUND(pSoundsCrete, fvol, pitch );
		break;
	case Computer:
	case Glass:
	case UnbreakableGlass:
		DAMAGE_SOUND(pSoundsGlass, fvol, pitch );
		break;
	case MetalPlate:
	case AirDuct:
	case Metal:
		DAMAGE_SOUND(pSoundsMetal, fvol, pitch );
		break;
	case CeilingTile:
		DAMAGE_SOUND(pSoundsCeil, fvol, pitch );
		break;
	case Wood:
		DAMAGE_SOUND(pSoundsWood, fvol, pitch );
		break;
	default:  break;
	}
}

void CBaseBrush::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	switch( m_Material )//apply effects for some materials (may be extended in future)
	{
	case Computer:
	{
		if(RANDOM_LONG(0,1))UTIL_Sparks( ptr->vecEndPos );

		float flVolume = RANDOM_FLOAT ( 0.7 , 1.0 );//random volume range
		switch ( RANDOM_LONG(0,1) )
		{
		case 0: EMIT_SOUND(ENT(pev), CHAN_VOICE, "materials/spark5.wav", flVolume, ATTN_NORM); break;
		case 1: EMIT_SOUND(ENT(pev), CHAN_VOICE, "materials/spark6.wav", flVolume, ATTN_NORM); break;
		}
	}
	break;
          case Glass:
          {
		if(pev->health == 0)//unbreakable glass
		{
			if( RANDOM_LONG(0,1))UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT(0.5,1.5) );
			UTIL_DecalTrace(ptr, DECAL_BPROOF1);	
		}
          	else UTIL_DecalTrace(ptr, DECAL_GLASSBREAK1 + RANDOM_LONG(0, 2));//breakable glass
  	}
          break;
          case UnbreakableGlass:
		UTIL_Ricochet( ptr->vecEndPos, RANDOM_FLOAT(0.5,1.5) );
          break;
          }
	CBaseLogic::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

int CBaseBrush :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	Vector	vecTemp;

	if ( pevAttacker == pevInflictor )
	{
		vecTemp = pevInflictor->origin - ( pev->absmin + ( pev->size * 0.5 ) );
		if((pevAttacker->flags & FL_CLIENT) && (pev->spawnflags & SF_BREAK_CROWBAR) && (bitsDamageType & DMG_CLUB))
			flDamage = pev->health; //old valve flag
	}
	else	vecTemp = pevInflictor->origin - ( pev->absmin + ( pev->size * 0.5 ) );

	if (!IsBreakable())
	{
		DamageSound();
		return 0;
          }
	// Breakables take double damage from the crowbar
	if ( bitsDamageType & DMG_CLUB ) flDamage *= 1.2;
          else if( bitsDamageType & DMG_CRUSH ) flDamage *= 2; 
	else if( bitsDamageType & DMG_BLAST ) flDamage *= 3;
	else if( bitsDamageType & DMG_BULLET && m_Material == Glass )flDamage *= 0.5;
	else if( bitsDamageType & DMG_BULLET && m_Material == Wood )flDamage *= 0.7;
	else flDamage = 0; //don't give any other damage	

	DmgType = bitsDamageType;//member damage type
	
	g_vecAttackDir = vecTemp.Normalize();
	pev->health -= flDamage;

	if (pev->health <= 0)
	{
		Killed( pevAttacker, GIB_NORMAL );
		Die();
		return 0;
	}
	DamageSound();

	return 1;
}

void CBaseBrush :: Blocked( CBaseEntity *pOther )
{
	if(m_pParent && m_pParent->edict() && pFlags & PF_PARENTMOVE)//tell parent
		m_pParent->Blocked( pOther );

	if(!pOther->IsPlayer() && !pOther->IsMonster())//crash breakable
	{
		if(IsBreakable())TakeDamage( pev, pev, 5, DMG_CRUSH );
	}
}

BOOL CBaseBrush :: IsBreakable( void )
{
	return (pev->health > 0 && m_Material != UnbreakableGlass);
}

void CBaseBrush::Die( void )
{
	Vector vecSpot, vecVelocity;
	char cFlag = 0;
	int pitch, soundbits = NULL;
	float fvol;

	pitch = 95 + RANDOM_LONG(0, 29);
	if(pitch > 97 && pitch < 103)pitch = 100;

	fvol = RANDOM_FLOAT(0.85, 1.0) + (abs(pev->health) / 100.0);
	if(fvol > 1.0)fvol = 1.0;

	switch (m_Material)
	{
	case None: break;	//just in case
	case Bones:
	case Flesh:
		if(RANDOM_LONG(0,1) )
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "materials/flesh/bustflesh1.wav", fvol, ATTN_NORM, 0, pitch);
		else	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "materials/flesh/bustflesh2.wav", fvol, ATTN_NORM, 0, pitch);
		cFlag = BREAK_FLESH;
		soundbits = bits_SOUND_MEAT;
		break;
	case CinderBlock:
	case Concrete:
	case Rocks:
		if(RANDOM_LONG(0,1) )
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "materials/crete/bustcrete1.wav", fvol, ATTN_NORM, 0, pitch);
		else	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "materials/crete/bustcrete2.wav", fvol, ATTN_NORM, 0, pitch);
		cFlag = BREAK_CONCRETE;
		soundbits = bits_SOUND_WORLD;
		break;
	case Computer:
	case Metal:
	case AirDuct:
	case MetalPlate:
		if(RANDOM_LONG(0,1) )
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "materials/metal/bustmetal1.wav", fvol, ATTN_NORM, 0, pitch);
		else	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "materials/metal/bustmetal2.wav", fvol, ATTN_NORM, 0, pitch);
		cFlag = BREAK_METAL;
		soundbits = bits_SOUND_WORLD;
		break;
	case Glass:
		if( RANDOM_LONG(0,1) )
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "materials/glass/bustglass1.wav", fvol, ATTN_NORM, 0, pitch);
		else	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "materials/glass/bustglass2.wav", fvol, ATTN_NORM, 0, pitch);
		cFlag = BREAK_GLASS;
		soundbits = bits_SOUND_WORLD;
		break;

	case Wood:
		if( RANDOM_LONG(0,1) )
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "materials/wood/bustcrate1.wav",  fvol, ATTN_NORM, 0, pitch);
		else	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "materials/wood/bustcrate2.wav",  fvol, ATTN_NORM, 0, pitch);
		cFlag = BREAK_WOOD;
		soundbits = bits_SOUND_WORLD;
		break;
	case CeilingTile:
		if( RANDOM_LONG(0,1) )
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "materials/ceil/bustceiling1.wav",fvol, ATTN_NORM, 0, pitch);
		else	EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, "materials/ceil/bustceiling1.wav",fvol, ATTN_NORM, 0, pitch);
		cFlag = BREAK_CONCRETE;
		soundbits = bits_SOUND_GARBAGE;
		break;
	}

	if (DmgType & DMG_CLUB)//direction from crowbar
		vecVelocity = g_vecAttackDir * clamp(pev->dmg * -10, -1024, 1024);
	else	vecVelocity = UTIL_RandomVector() * clamp(pev->dmg * 10, 1, 240);

	vecSpot = pev->origin + (pev->mins + pev->maxs) * 0.5;
	float time = RANDOM_FLOAT(25, 100);

	if(soundbits) CSoundEnt::InsertSound(soundbits, pev->origin, 128, 0.5);//make noise for ai
	SFX_MakeGibs( m_idShard, vecSpot, pev->size, vecVelocity, time, cFlag);

	//what the hell does this ?
	UTIL_FindBreakable( this );

	// If I'm getting removed, don't fire something that could fire myself
	pev->targetname = 0;

	pev->solid = SOLID_NOT;
	UTIL_FireTargets( pev->target, NULL, NULL, USE_TOGGLE );
	pev->target = 0;
	Msg("Fire!\n");
	SetThink( Remove );
	SetNextThink( 0.1 );
	
	//pev->effects |= EF_NODRAW;
	//pev->takedamage = DAMAGE_NO;
          
	//make explosion
          if(m_iMagnitude) UTIL_Explode( Center(), edict(), m_iMagnitude );
	if(m_iSpawnObject) CBaseEntity::Create( (char *)STRING(m_iSpawnObject), Center(), pev->angles, edict() );
	
	// Fire targets on break
 	
	//UTIL_Remove( this );
}

int CBaseBrush :: DamageDecal( int bitsDamageType )
{
	if ( m_Material == Glass  )
		return DECAL_GLASSBREAK1 + RANDOM_LONG(0,2);
	if ( m_Material == Glass && pev->health <= 0)
		return DECAL_BPROOF1;
	if ( m_Material == UnbreakableGlass )
		return DECAL_BPROOF1;
	if ( m_Material == Wood )
		return DECAL_GUNSHOT1 + RANDOM_LONG(0,4);
	if ( m_Material == Computer )
		return DECAL_BIGSHOT1 + RANDOM_LONG(0,4);
	return CBaseEntity::DamageDecal( bitsDamageType );
}

//=======================================================================
//		moving breakable brushes
//=======================================================================
//material moving sounds
const char *CPushable::pPushWood[] =
{
	"materials/wood/pushwood1.wav",
	"materials/wood/pushwood2.wav",
	"materials/wood/pushwood3.wav",
};

const char *CPushable::pPushFlesh[] =
{
	"materials/flesh/pushflesh1.wav",
	"materials/flesh/pushflesh2.wav",
	"materials/flesh/pushflesh3.wav",
};
const char *CPushable::pPushMetal[] =
{
	"materials/metal/pushmetal1.wav",
	"materials/metal/pushmetal2.wav",
	"materials/metal/pushmetal3.wav",
};
const char *CPushable::pPushCrete[] =
{
	"materials/crete/pushstone1.wav",
	"materials/crete/pushstone2.wav",
	"materials/crete/pushstone3.wav",
};
const char *CPushable::pPushGlass[] =
{
	"materials/glass/pushglass1.wav",
	"materials/glass/pushglass2.wav",
	"materials/glass/pushglass3.wav",
};
const char *CPushable::pPushCeil[] =
{
	"materials/ceil/ceiling1.wav",
	"materials/ceil/ceiling2.wav",
	"materials/ceil/ceiling3.wav",
};

void CPushable :: Spawn( void )
{
	Precache();

	pev->movetype	= MOVETYPE_PUSHSTEP;
	pev->solid	= SOLID_BBOX;
	UTIL_SetModel( ENT(pev), pev->model );
	UTIL_SetSize( pev, pev->absmin, pev->absmax );

	if (!m_flVolume)m_flVolume = 1.0;//just enable full volume
	          
	//breacable brush (if mapmaker just set material - just play material sound)
	if(m_Material != None)
		pev->takedamage	= DAMAGE_YES;
	else	pev->takedamage	= DAMAGE_NO;

	pev->speed = MatFrictionTable( m_Material );
	SetBits( pev->flags, FL_FLOAT );
	pev->origin.z += 1;	// Pick up off of the floor
	UTIL_SetOrigin( this, pev->origin );

	// Multiply by area of the box's cross-section (assume 1000 units^3 standard volume)
	pev->skin = MatByoancyTable( pev, m_Material );
	pev->frags = 0;
}

void CPushable :: Precache( void )
{
	CBaseBrush::Precache();
	PushSoundPrecache( m_Material );
}

const char **CPushable::PushSoundList( Materials precacheMaterial, int &soundCount )
{
	const char **pSoundList = NULL;

	switch ( precacheMaterial )
	{
	case None:
		soundCount = 0;
		break;
	case Bones:
	case Flesh:
		pSoundList = pPushFlesh;
		soundCount = ARRAYSIZE(pPushFlesh);
		break;	
	case CinderBlock:
	case Concrete:
	case Rocks:
		pSoundList = pPushCrete;
		soundCount = ARRAYSIZE(pPushCrete);
		break;
	case Computer:
	case Glass:
		pSoundList = pPushGlass;
		soundCount = ARRAYSIZE(pPushGlass);
		break;
	case MetalPlate:
	case AirDuct:
	case Metal:
		pSoundList = pPushMetal;
		soundCount = ARRAYSIZE(pPushMetal);
		break;
	case CeilingTile:
		pSoundList = pPushCeil;
		soundCount = ARRAYSIZE(pPushCeil);
		break;
	case Wood:
		pSoundList = pPushWood;
		soundCount = ARRAYSIZE(pPushWood);
		break;
	default:
		soundCount = 0;
		break;
	}
	return pSoundList;
}

void CPushable::PushSoundPrecache( Materials precacheMaterial )
{
	const char **pPushList;
	int i, soundCount = 0;

	pPushList = PushSoundList( precacheMaterial, soundCount );
	for ( i = 0; i < soundCount; i++ ) UTIL_PrecacheSound( (char *)pPushList[i] );
}

int CPushable::PlayPushSound( edict_t *pEdict, Materials soundMaterial, float volume )
{
	const char **pPushList;
	int soundCount = 0;
 	int m_lastPushSnd = 0;
 	
	pPushList = PushSoundList( soundMaterial, soundCount );
	if ( soundCount ) 
	{
		m_lastPushSnd = RANDOM_LONG(0, soundCount-1);
		EMIT_SOUND( pEdict, CHAN_WEAPON, pPushList[ m_lastPushSnd ], volume, 1.0 );
	}
	return m_lastPushSnd;
}

void CPushable::StopPushSound( edict_t *pEdict, Materials soundMaterial, int m_lastPushSnd )
{
	const char **pPushList;
	int soundCount = 0;
 	
	pPushList = PushSoundList( soundMaterial, soundCount );
	if ( soundCount ) STOP_SOUND( pEdict, CHAN_WEAPON, pPushList[ m_lastPushSnd ] );
}

void CPushable :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if(useType == USE_SHOWINFO)
	{
		DEBUGHEAD;
	}
	else if ( pActivator && pActivator->IsPlayer() && pActivator->pev->velocity != g_vecZero )
		Move( pActivator, 0 );
}

void CPushable :: Touch( CBaseEntity *pOther )
{
	if ( pOther == g_pWorld ) return;
	Move( pOther, 1 );
}

void CPushable :: Move( CBaseEntity *pOther, int push )
{
	entvars_t* pevToucher = pOther->pev;
	int playerTouch = 0;

	// Is entity standing on this pushable ?
	if ( FBitSet(pevToucher->flags, FL_ONGROUND) && pevToucher->groundentity && VARS(pevToucher->groundentity) == pev )
	{
		// Only push if floating
		if ( pev->waterlevel > 0 && pev->watertype & MASK_WATER )
			pev->velocity.z += pevToucher->velocity.z * 0.1;
		return;
	}


	if ( pOther->IsPlayer() )
	{
		// Don't push unless the player is pushing forward and NOT use (pull)
		if ( push && !(pevToucher->button & (IN_FORWARD|IN_USE)) ) return;
		playerTouch = 1;
	}

	float factor;

	if ( playerTouch )
	{	
		// Don't push away from jumping/falling players unless in water
		if( !(pevToucher->flags & FL_ONGROUND) )	
		{
			if( pev->waterlevel < 1 || !(pev->watertype & MASK_WATER) )
				return;
			else factor = 0.1;
		}
		else	factor = 1;
	}
	else	factor = 0.25;

	if (!push) factor = factor*0.5;

	pev->velocity.x += pevToucher->velocity.x * factor;
	pev->velocity.y += pevToucher->velocity.y * factor;

	float length = sqrt( pev->velocity.x * pev->velocity.x + pev->velocity.y * pev->velocity.y );
	if ( push && (length > MaxSpeed()) )
	{
		pev->velocity.x = (pev->velocity.x * MaxSpeed() / length );
		pev->velocity.y = (pev->velocity.y * MaxSpeed() / length );
	}
	if ( playerTouch )
	{
		pevToucher->velocity.x = pev->velocity.x;
		pevToucher->velocity.y = pev->velocity.y;
		if ( (gpGlobals->time - pev->frags) > 0.7 )
		{
			pev->frags = gpGlobals->time;
			if ( length > 0 && FBitSet(pev->flags, FL_ONGROUND) )
				m_lastSound = PlayPushSound( edict(), m_Material, m_flVolume );
			else	StopPushSound( edict(), m_Material, m_lastSound );
		}
	}
}
LINK_ENTITY_TO_CLASS( func_pushable, CPushable );