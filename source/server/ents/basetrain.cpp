//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "sfx.h"
#include "cbase.h"
#include "basebeams.h"
#include "baseweapon.h"
#include "monsters.h"
#include "defaults.h"
#include "player.h"

// Tracktrain spawn flags
#define SF_TRACKTRAIN_NOPITCH		0x0001
#define SF_TRACKTRAIN_NOCONTROL		0x0002
#define SF_TRACKTRAIN_FORWARDONLY	0x0004
#define SF_TRACKTRAIN_PASSABLE		0x0008
#define SF_TRACKTRAIN_NOYAW			0x0010		//LRC
#define SF_TRACKTRAIN_AVELOCITY		0x800000	//LRC - avelocity has been set manually, don't turn.
#define SF_TRACKTRAIN_AVEL_GEARS	0x400000	//LRC - avelocity should be scaled up/down when the train changes gear.

// Spawnflag for CPathTrack
#define SF_PATH_DISABLED		0x00000001
#define SF_PATH_FIREONCE		0x00000002
#define SF_PATH_ALTREVERSE		0x00000004
#define SF_PATH_DISABLE_TRAIN	0x00000008
#define SF_PATH_ALTERNATE		0x00008000
#define SF_PATH_AVELOCITY		0x00080000 //LRC

//LRC - values in 'armortype'
#define PATHSPEED_SET			0
#define PATHSPEED_ACCEL			1
#define PATHSPEED_TIME			2
#define PATHSPEED_SET_MASTER	3


//#define PATH_SPARKLE_DEBUG		1	// This makes a particle effect around path_track entities for debugging
class CPathTrack : public CPointEntity
{
public:
	void		Spawn( void );
	void		Activate( void );
	void		KeyValue( KeyValueData* pkvd);
	
	void		SetPrevious( CPathTrack *pprevious );
	void		Link( void );
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	CBaseEntity	*ValidPath( CBaseEntity *ppath, int testFlag );		// Returns ppath if enabled, NULL otherwise
	void		Project( CBaseEntity *pstart, CBaseEntity *pend, Vector *origin, float dist );

	static CPathTrack *Instance( edict_t *pent );

	CBaseEntity	*LookAhead( Vector *origin, float dist, int move );
	CBaseEntity	*Nearest( Vector origin ); //notused

	CBaseEntity	*GetNext( void );
	CBaseEntity	*GetPrev( void );

	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	
	static	TYPEDESCRIPTION m_SaveData[];
#if PATH_SPARKLE_DEBUG
	void EXPORT Sparkle(void);
#endif

	float		m_length;
	string_t	m_altName;
	CPathTrack	*m_pnext;
	CPathTrack	*m_pprevious;
	CPathTrack	*m_paltpath;
};

class CFuncTrackTrain : public CBaseMover
{
public:
	void Spawn( void );
	void Precache( void );

	void Blocked( CBaseEntity *pOther );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void KeyValue( KeyValueData* pkvd );

	void EXPORT DesiredAction( void ); //LRC - used to be called Next!
          void PostActivate( void );
	void ClearPointers( void );

//	void EXPORT Next( void );
	void EXPORT PostponeNext( void );
	void EXPORT Find( void );
	void EXPORT NearestPath( void );
	void EXPORT DeadEnd( void );

	void NextThink( float thinkTime, BOOL alwaysThink );

	void SetTrack( CBaseEntity *track ) { pPath = ((CPathTrack *)track)->Nearest(pev->origin); }
	void SetControls( entvars_t *pevControls );
	BOOL OnControls( entvars_t *pev );

	void StopSound ( void );
	void UpdateSound ( void );
	
	static CFuncTrackTrain *Instance( edict_t *pent )
	{
		if ( FClassnameIs( pent, "func_tracktrain" ) )
			return (CFuncTrackTrain *)GET_PRIVATE(pent);
		return NULL;
	}

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	
	static TYPEDESCRIPTION m_SaveData[];
	virtual int ObjectCaps( void ) { return CBaseMover :: ObjectCaps() | FCAP_DIRECTIONAL_USE; }

	virtual void	OverrideReset( void );
	
	CBaseEntity	*pPath;
	float		m_length;
	float		m_height;
	// I get it... this records the train's max speed (as set by the level designer), whereas
	// pev->speed records the current speed (as set by the player). --LRC
	// m_speed is also stored, as an int, in pev->impulse.
	float		m_speed;
	float		m_dir;
	float		m_startSpeed;
	Vector		m_controlMins;
	Vector		m_controlMaxs;
	int		m_soundPlaying;
	float		m_flBank;
	float		m_oldSpeed;
	Vector		m_vecBaseAvel; // LRC - the underlying avelocity, superceded by normal turning behaviour where applicable
};

TYPEDESCRIPTION	CPathTrack::m_SaveData[] = 
{
	DEFINE_FIELD( CPathTrack, m_length, FIELD_FLOAT ),
	DEFINE_FIELD( CPathTrack, m_pnext, FIELD_CLASSPTR ),
	DEFINE_FIELD( CPathTrack, m_paltpath, FIELD_CLASSPTR ),
	DEFINE_FIELD( CPathTrack, m_pprevious, FIELD_CLASSPTR ),
	DEFINE_FIELD( CPathTrack, m_altName, FIELD_STRING ),
};

IMPLEMENT_SAVERESTORE( CPathTrack, CBaseEntity );
LINK_ENTITY_TO_CLASS( path_track, CPathTrack );

//
// Cache user-entity-field values until spawn is called.
//
void CPathTrack :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "altpath"))
	{
		m_altName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "turnspeed")) //LRC
	{
		if (pkvd->szValue[0]) // if the field is blank, don't set the spawnflag.
		{
			pev->spawnflags |= SF_PATH_AVELOCITY;
			UTIL_StringToVector( (float*)pev->avelocity, pkvd->szValue);
		}
		pkvd->fHandled = TRUE;
	}
	else
		CPointEntity::KeyValue( pkvd );
}

void CPathTrack :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	int on;

	// Use toggles between two paths
	if ( m_paltpath )
	{
		on = !FBitSet( pev->spawnflags, SF_PATH_ALTERNATE );
		if (useType == USE_TOGGLE)
		{
			if(on) useType = USE_OFF;
			else useType = USE_ON;
		}
		if (useType == USE_ON)ClearBits( pev->spawnflags, SF_PATH_ALTERNATE );
          	else if(useType == USE_OFF)SetBits( pev->spawnflags, SF_PATH_ALTERNATE );
	}
	else	// Use toggles between enabled/disabled
	{
		on = !FBitSet( pev->spawnflags, SF_PATH_DISABLED );
		if (useType == USE_TOGGLE)
		{
			if(on) useType = USE_OFF;
			else useType = USE_ON;
		}
		if (useType == USE_ON)ClearBits( pev->spawnflags, SF_PATH_DISABLED );
          	else if(useType == USE_OFF)SetBits( pev->spawnflags, SF_PATH_DISABLED );
	}
}


void CPathTrack :: Link( void  )
{
	CBaseEntity *pTarget;

	if ( !FStringNull(pev->target) )
	{
		pTarget = UTIL_FindEntityByTargetname( NULL, STRING(pev->target) );
		if ( pTarget )
		{
			m_pnext = (CPathTrack*)pTarget;
			m_pnext->SetPrevious( this );
		}
		else
			ALERT( at_console, "Dead end link %s\n", STRING(pev->target) );
	}

	// Find "alternate" path
	if ( m_altName )
	{
		pTarget = UTIL_FindEntityByTargetname( NULL, STRING(m_altName) );
		if ( pTarget )		// If no next pointer, this is the end of a path
		{
			m_paltpath = (CPathTrack*)pTarget;
			m_paltpath->SetPrevious( this );
		}
	}
}


void CPathTrack :: Spawn( void )
{
	pev->solid = SOLID_TRIGGER;
	UTIL_SetSize(pev, Vector(-8, -8, -8), Vector(8, 8, 8));

	m_pnext = NULL;
	m_pprevious = NULL;
// DEBUGGING CODE
#if PATH_SPARKLE_DEBUG
	SetThink( Sparkle );
	SetNextThink( 0.5 );
#endif
}


void CPathTrack::Activate( void )
{
	if ( !FStringNull( pev->targetname ) )		// Link to next, and back-link
		Link();

	CPointEntity::Activate();
}

CBaseEntity	*CPathTrack :: ValidPath( CBaseEntity	*ppath, int testFlag )
{
	if ( !ppath )
		return NULL;

	if ( testFlag && FBitSet( ppath->pev->spawnflags, SF_PATH_DISABLED ) )
		return NULL;

	return ppath;
}


void CPathTrack :: Project( CBaseEntity *pstart, CBaseEntity *pend, Vector *origin, float dist )
{
	if ( pstart && pend )
	{
		Vector dir = (pend->pev->origin - pstart->pev->origin);
		dir = dir.Normalize();
		*origin = pend->pev->origin + dir * dist;
	}
}

CBaseEntity *CPathTrack::GetNext( void )
{
	if ( m_paltpath && FBitSet( pev->spawnflags, SF_PATH_ALTERNATE ) && !FBitSet( pev->spawnflags, SF_PATH_ALTREVERSE ) )
		return m_paltpath;
	
	return m_pnext;
}



CBaseEntity *CPathTrack::GetPrev( void )
{
	if ( m_paltpath && FBitSet( pev->spawnflags, SF_PATH_ALTERNATE ) && FBitSet( pev->spawnflags, SF_PATH_ALTREVERSE ) )
		return m_paltpath;
	
	return m_pprevious;
}



void CPathTrack::SetPrevious( CPathTrack *pprev )
{
	// Only set previous if this isn't my alternate path
	if ( pprev && !FStrEq( STRING(pprev->pev->targetname), STRING(m_altName) ) )
		m_pprevious = pprev;
}


// Assumes this is ALWAYS enabled
CBaseEntity *CPathTrack :: LookAhead( Vector *origin, float dist, int move )
{
	CBaseEntity *pcurrent;
	float originalDist = dist;
	
	pcurrent = this;
	Vector currentPos = *origin;

	if ( dist < 0 )		// Travelling backwards through path
	{
		dist = -dist;
		while ( dist > 0 )
		{
			Vector dir = pcurrent->pev->origin - currentPos;
			float length = dir.Length();
			if ( !length )
			{
				if ( !ValidPath(pcurrent->GetPrev(), move) ) 	// If there is no previous node, or it's disabled, return now.
				{
					if ( !move )
						Project( pcurrent->GetNext(), pcurrent, origin, dist );
					return NULL;
				}
				pcurrent = pcurrent->GetPrev();
			}
			else if ( length > dist )	// enough left in this path to move
			{
				*origin = currentPos + (dir * (dist / length));
				return pcurrent;
			}
			else
			{
				dist -= length;
				currentPos = pcurrent->pev->origin;
				*origin = currentPos;
				if ( !ValidPath(pcurrent->GetPrev(), move) )	// If there is no previous node, or it's disabled, return now.
					return NULL;

				pcurrent = pcurrent->GetPrev();
			}
		}
		*origin = currentPos;
		return pcurrent;
	}
	else 
	{
		while ( dist > 0 )
		{
			if ( !ValidPath(pcurrent->GetNext(), move) )	// If there is no next node, or it's disabled, return now.
			{
				if ( !move )
					Project( pcurrent->GetPrev(), pcurrent, origin, dist );
				return NULL;
			}
			Vector dir = pcurrent->GetNext()->pev->origin - currentPos;
			float length = dir.Length();
			if ( !length  && !ValidPath( pcurrent->GetNext()->GetNext(), move ) )
			{
				if ( dist == originalDist ) // HACK -- up against a dead end
					return NULL;
				return pcurrent;
			}
			if ( length > dist )	// enough left in this path to move
			{
				*origin = currentPos + (dir * (dist / length));
				return pcurrent;
			}
			else
			{
				dist -= length;
				currentPos = pcurrent->GetNext()->pev->origin;
				pcurrent = pcurrent->GetNext();
				*origin = currentPos;
			}
		}
		*origin = currentPos;
	}

	return pcurrent;
}

	
// Assumes this is ALWAYS enabled
CBaseEntity *CPathTrack :: Nearest( Vector origin )
{
	int			deadCount;
	float		minDist, dist;
	Vector		delta;
	CBaseEntity	*ppath, *pnearest;


	delta = origin - pev->origin;
	delta.z = 0;
	minDist = delta.Length();
	pnearest = this;
	ppath = GetNext();

	// Hey, I could use the old 2 racing pointers solution to this, but I'm lazy :)
	deadCount = 0;
	while ( ppath && ppath != this )
	{
		deadCount++;
		if ( deadCount > 9999 )
		{
			ALERT( at_error, "Bad sequence of path_tracks from %s", STRING(pev->targetname) );
			return NULL;
		}
		delta = origin - ppath->pev->origin;
		delta.z = 0;
		dist = delta.Length();
		if ( dist < minDist )
		{
			minDist = dist;
			pnearest = ppath;
		}
		ppath = ppath->GetNext();
	}
	return pnearest;
}


CPathTrack *CPathTrack::Instance( edict_t *pent )
{ 
	if ( FClassnameIs( pent, "path_track" ) )
		return (CPathTrack *)GET_PRIVATE(pent);
	return NULL;
}


	// DEBUGGING CODE
#if PATH_SPARKLE_DEBUG
void CPathTrack :: Sparkle( void )
{

	SetNextThink( 0.2 );
	if ( FBitSet( pev->spawnflags, SF_PATH_DISABLED ) )
		UTIL_ParticleEffect(pev->origin, Vector(0,0,100), 210, 10);
	else
		UTIL_ParticleEffect(pev->origin, Vector(0,0,100), 84, 10);
}
#endif

//=======================================================================
// 		   func_tracktrain (controllable train)
//=======================================================================
void CFuncTrackTrain :: Precache( void )
{
	CBaseBrush::Precache();//precache damage sound

	int m_sounds = UTIL_LoadSoundPreset(m_iMoveSound);
	switch (m_sounds)
	{
	case 1: pev->noise = UTIL_PrecacheSound("plats/ttrain1.wav");break;
	case 2: pev->noise = UTIL_PrecacheSound("plats/ttrain2.wav");break;
	case 3: pev->noise = UTIL_PrecacheSound("plats/ttrain3.wav");break; 
	case 4: pev->noise = UTIL_PrecacheSound("plats/ttrain4.wav");break;
	case 5: pev->noise = UTIL_PrecacheSound("plats/ttrain6.wav");break;
	case 6: pev->noise = UTIL_PrecacheSound("plats/ttrain7.wav");break;
	default: pev->noise = UTIL_PrecacheSound(m_sounds); break;//custom sound or sentence
	}
          
	m_sounds = UTIL_LoadSoundPreset(m_iStopSound);
	switch (m_sounds)
	{
	case 1: pev->noise1 = UTIL_PrecacheSound("plats/ttrain_brake1.wav");break;
	default: pev->noise1 = UTIL_PrecacheSound(m_sounds); break;//custom sound or sentence
	}

	m_sounds = UTIL_LoadSoundPreset(m_iStartSound);
	switch (m_sounds)
	{
	case 1: pev->noise2 = UTIL_PrecacheSound("plats/ttrain_start1.wav");break;
	default: pev->noise2 = UTIL_PrecacheSound(m_sounds); break;//custom sound or sentence
	}
}

void CFuncTrackTrain :: Spawn( void )
{
	if ( pev->speed == 0 ) m_speed = 100;
	else m_speed = pev->speed;
	pev->speed = 0;
	pev->velocity = g_vecZero;
	m_vecBaseAvel = pev->avelocity; //LRC - save it for later
	pev->avelocity = g_vecZero;
	pev->impulse = m_speed;
	m_dir = 1;

	if ( FStringNull(pev->target) ) Msg("Warning: %s with no target!\n", STRING(pev->classname));

	//if ( pev->spawnflags & SF_NOTSOLID ) pev->solid = SOLID_NOT;
	if ( pev->spawnflags & 8 ) pev->solid = SOLID_NOT; //temp solution
	else pev->solid = SOLID_BSP;
	pev->movetype = MOVETYPE_PUSH;
	SetBits (pFlags, PF_ANGULAR);
	UTIL_SetModel( ENT(pev), pev->model );

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( this, pev->origin );
	pev->oldorigin = pev->origin; // Cache off placed origin for train controls

	m_controlMins = pev->mins;
	m_controlMaxs = pev->maxs;
	m_controlMaxs.z += 72;

	NextThink( 0.1, FALSE );
	SetThink( Find );
	Precache();
}

void CFuncTrackTrain :: NextThink( float thinkTime, BOOL alwaysThink )
{
	if ( alwaysThink ) pev->flags |= FL_ALWAYSTHINK;
	else pev->flags &= ~FL_ALWAYSTHINK;
	SetNextThink( thinkTime, TRUE );
}

void CFuncTrackTrain :: Blocked( CBaseEntity *pOther )
{
	// Blocker is on-ground on the train
	if ( FBitSet( pOther->pev->flags, FL_ONGROUND ) && VARS(pOther->pev->groundentity) == pev )
	{
		float deltaSpeed = fabs(pev->speed);
		if ( deltaSpeed > 50 ) deltaSpeed = 50;
		if ( !pOther->pev->velocity.z ) pOther->pev->velocity.z += deltaSpeed;
		return;
	}
	else pOther->pev->velocity = (pOther->pev->origin - pev->origin ).Normalize() * pev->dmg;
	if ( pev->dmg <= 0 ) return;// we can't hurt this thing, so we're not concerned with it
	pOther->TakeDamage( pev, pev, pev->dmg, DMG_CRUSH );
}

void CFuncTrackTrain::OverrideReset( void )
{
	NextThink( 0.1, FALSE );
	SetThink( NearestPath );
}

void CFuncTrackTrain :: Find( void )
{
	pPath = (CPathTrack*)UTIL_FindEntityByTargetname( NULL, STRING(pev->target) );
	if ( !pPath ) return;

	entvars_t *pevTarget = pPath->pev;
	if ( !FClassnameIs( pevTarget, "path_track" ) )
	{
		ALERT( at_error, "func_track_train must be on a path of path_track\n" );
		pPath = NULL;
		return;
	}

	Vector nextPos = pevTarget->origin;
	nextPos.z += m_height;

	Vector look = nextPos;
	look.z -= m_height;
	((CPathTrack *)pPath)->LookAhead( &look, m_length, 0 );
	look.z += m_height;

	Vector vTemp = UTIL_VecToAngles( look - nextPos );
	vTemp.y += 180;

	if ( pev->spawnflags & SF_TRACKTRAIN_NOPITCH )
	{
		vTemp.x = 0;
		//pev->angles.x = 0;
	}

	UTIL_AssignAngles(this, vTemp); 
	UTIL_AssignOrigin ( this, nextPos );

	NextThink( 0.1, FALSE );
	SetThink( PostponeNext );
	pev->speed = m_startSpeed;

	UpdateSound();
}

TYPEDESCRIPTION	CFuncTrackTrain::m_SaveData[] = 
{
	DEFINE_FIELD( CFuncTrackTrain, pPath, FIELD_CLASSPTR ),
	DEFINE_FIELD( CFuncTrackTrain, m_length, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_height, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_speed, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_dir, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_startSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_controlMins, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncTrackTrain, m_controlMaxs, FIELD_VECTOR ),
	DEFINE_FIELD( CFuncTrackTrain, m_flVolume, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_flBank, FIELD_FLOAT ),
	DEFINE_FIELD( CFuncTrackTrain, m_oldSpeed, FIELD_FLOAT ),
};
IMPLEMENT_SAVERESTORE( CFuncTrackTrain, CBaseMover );


void CFuncTrackTrain :: KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "wheels"))
	{
		m_length = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "height"))
	{
		m_height = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "startspeed"))
	{
		m_startSpeed = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "sounds"))
	{
		m_iMoveSound = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "bank"))
	{
		m_flBank = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else CBaseMover::KeyValue( pkvd );
}





void CFuncTrackTrain :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
//	ALERT(at_console, "TRAIN: use\n");

	m_hActivator = pActivator;	//AJH

	if (useType == USE_TOGGLE)
	{
		if(pev->speed != 0) useType = USE_OFF;//temp solution
		else useType = USE_ON;
	}
	if (useType == USE_ON)
	{
		pev->speed = m_speed * m_dir;
		PostponeNext();	
	}
	else if ( useType == USE_OFF )
	{
		pev->speed = 0;
		UTIL_SetVelocity(this, g_vecZero); //LRC
		UTIL_SetAvelocity(this, g_vecZero); //LRC
		StopSound();
		SetThink( NULL );
	}
	else if ( useType == USE_SET )
	{
		float delta = value;

		delta = ((int)(pev->speed * 4) / (int)m_speed)*0.25 + 0.25 * delta;
		if ( delta > 1 )
			delta = 1;
		else if ( delta < -1 )
			delta = -1;
		if ( pev->spawnflags & SF_TRACKTRAIN_FORWARDONLY )
		{
			if ( delta < 0 )delta = 0;
		}
		pev->speed = m_speed * delta;
                   
                    if(pev->speed == 0)
                    {
			UTIL_SetVelocity(this, g_vecZero); 
			UTIL_SetAvelocity(this, g_vecZero);
			StopSound();
			SetThink( NULL );
		}
	          if(pPath == NULL) 
	          {
			delta = 0; //G-Cont. Set speed to 0, and don't controls, if tracktrain on trackchange
			return;
	          }
		PostponeNext();	
		ALERT( at_aiconsole, "TRAIN(%s), speed to %.2f\n", STRING(pev->targetname), pev->speed );
	}
}

#define TRAIN_STARTPITCH	60
#define TRAIN_MAXPITCH		200
#define TRAIN_MAXSPEED		1000	// approx max speed for sound pitch calculation

void CFuncTrackTrain :: StopSound( void )
{
	// if sound playing, stop it
	if (m_soundPlaying && pev->noise)
	{
		STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise));
		EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "plats/ttrain_brake1.wav", m_flVolume, ATTN_NORM, 0, 100);
	}

	m_soundPlaying = 0;
}

void CFuncTrackTrain :: UpdateSound( void )
{
	float flpitch;
	
	if (!pev->noise)
		return;

	flpitch = TRAIN_STARTPITCH + (abs(pev->speed) * (TRAIN_MAXPITCH - TRAIN_STARTPITCH) / TRAIN_MAXSPEED);

	if (!m_soundPlaying)
	{
		// play startup sound for train
		EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "plats/ttrain_start1.wav", m_flVolume, ATTN_NORM, 0, 100);
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise), m_flVolume, ATTN_NORM, 0, (int) flpitch);
		m_soundPlaying = 1;
	} 
	else
	{
		// update pitch
		EMIT_SOUND_DYN(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noise), m_flVolume, ATTN_NORM, SND_CHANGE_PITCH, (int) flpitch);
	}
}

void CFuncTrackTrain::ClearPointers( void )
{
	CBaseEntity::ClearPointers();
	pPath = NULL;
}

void CFuncTrackTrain :: PostActivate( void )
{
}
void CFuncTrackTrain :: PostponeNext( void )
{
	UTIL_SetAction( this );
}

void CFuncTrackTrain :: DesiredAction( void ) // Next( void )
{
	float time = 0.5;

	if ( !pev->speed )
	{
		UTIL_SetVelocity(this, g_vecZero);
		DontThink();
		ALERT( at_aiconsole, "TRAIN(%s): Speed is 0\n", STRING(pev->targetname) );
		StopSound();
		return;
	}
          
	if ( !pPath )
	{	
		UTIL_SetVelocity(this, g_vecZero);
		DontThink();
		ALERT( at_aiconsole, "TRAIN(%s): Lost path\n", STRING(pev->targetname) );
		StopSound();
		return;
	}

	UpdateSound();

	Vector nextPos = pev->origin;
          
	nextPos.z -= m_height;
	CBaseEntity *pnext = ((CPathTrack *)pPath)->LookAhead( &nextPos, pev->speed * 0.1, 1 );
	nextPos.z += m_height;

	UTIL_SetVelocity( this, (nextPos - pev->origin) * 10 ); //LRC
	Vector nextFront = pev->origin;

	nextFront.z -= m_height;
	if ( m_length > 0 )
		((CPathTrack *)pPath)->LookAhead( &nextFront, m_length, 0 );
	else
		((CPathTrack *)pPath)->LookAhead( &nextFront, 100, 0 );
	nextFront.z += m_height;

	Vector delta = nextFront - pev->origin;

	Vector angles = UTIL_VecToAngles( delta );
	// The train actually points west
	angles.y += 180; //LRC, FIXME: add a 'built facing' field.

	angles.fixangle();
	pev->angles.fixangle();

	if ( !pnext || (delta.x == 0 && delta.y == 0) ) angles = pev->angles;

	float vy, vx, vz;
	if ( !(pev->spawnflags & SF_TRACKTRAIN_NOPITCH) ) vx = 10*UTIL_AngleDistance( angles.x, pev->angles.x );
	else vx = m_vecBaseAvel.x;

	if ( !(pev->spawnflags & SF_TRACKTRAIN_NOYAW) ) vy = 10*UTIL_AngleDistance( angles.y, pev->angles.y );
	else vy = m_vecBaseAvel.y;

	if ( m_flBank != 0 )
	{
		if ( pev->avelocity.y < -5 ) vz = UTIL_AngleDistance( UTIL_ApproachAngle( -m_flBank, pev->angles.z, m_flBank*2 ), pev->angles.z);
		else if ( pev->avelocity.y > 5 ) vz = UTIL_AngleDistance( UTIL_ApproachAngle( m_flBank, pev->angles.z, m_flBank*2 ), pev->angles.z);
		else vz = UTIL_AngleDistance( UTIL_ApproachAngle( 0, pev->angles.z, m_flBank*4 ), pev->angles.z) * 4;
	}
	else vz = m_vecBaseAvel.z;
	UTIL_SetAvelocity(this, Vector(vx, vy, vz));
		
	if ( pnext )
	{
		if ( pnext != pPath )
		{
			CBaseEntity *pFire;
			if ( pev->speed >= 0 ) // check whether we're going forwards or backwards
				pFire = pnext;
			else 
				pFire = pPath;

			pPath = pnext;
			// Fire the pass target if there is one
			if ( pFire->pev->message )
			{
				UTIL_FireTargets( pFire->pev->message, this, this, USE_TOGGLE );
				if ( FBitSet( pFire->pev->spawnflags, SF_PATH_FIREONCE ) )
					pFire->pev->message = 0;
			}

			if ( pFire->pev->spawnflags & SF_PATH_DISABLE_TRAIN )
				pev->spawnflags |= SF_TRACKTRAIN_NOCONTROL;

			float setting = ((int)(pev->speed*4) / (int)m_speed) / 4.0; //LRC - one of { 1, 0.75, 0.5, 0.25, 0, ... -1 }

			CBaseEntity* pDest; //LRC - the path_track we're heading for, after pFire.
			if (pev->speed > 0)
				pDest = pFire->GetNext();
			else
				pDest = pFire->GetPrev();

			if ( pFire->pev->speed != 0)
			{
				//ALERT( at_console, "TrackTrain setting is %d / %d = %.2f\n", (int)(pev->speed*4), (int)m_speed, setting );

				switch ( (int)(pFire->pev->armortype) )
				{
				case PATHSPEED_SET:
					// Don't override speed if under user control
					if (pev->spawnflags & SF_TRACKTRAIN_NOCONTROL)
					pev->speed = pFire->pev->speed;
					ALERT( at_aiconsole, "TrackTrain %s speed set to %4.2f\n", STRING(pev->targetname), pev->speed );
					break;
				case PATHSPEED_SET_MASTER:
					m_speed = pFire->pev->speed;
					pev->impulse = m_speed;
					pev->speed = setting * m_speed;
					ALERT( at_aiconsole, "TrackTrain %s master speed set to %4.2f\n", STRING(pev->targetname), pev->speed );
					break;
				case PATHSPEED_ACCEL:
					m_speed += pFire->pev->speed;
					pev->impulse = m_speed;
					pev->speed = setting * m_speed;
					ALERT( at_aiconsole, "TrackTrain %s speed accel to %4.2f\n", STRING(pev->targetname), pev->speed );
					break;
				case PATHSPEED_TIME:
					float distance = (pev->origin - pDest->pev->origin).Length();
					//ALERT(at_console, "pFire=%s, distance=%.2f, ospeed=%.2f, nspeed=%.2f\n", STRING(pFire->pev->targetname), distance, pev->speed, distance / pFire->pev->speed);
					m_speed = distance / pFire->pev->speed;
					pev->impulse = m_speed;
					pev->speed = setting * m_speed;
					ALERT( at_aiconsole, "TrackTrain %s speed to %4.2f (timed)\n", STRING(pev->targetname), pev->speed );
					break;
				}
			}
			//LRC- FIXME: add support, here, for a Teleport flag.
		}
		SetThink( PostponeNext );
		NextThink( time, TRUE );
	}
	else	// end of path, stop
	{
		Vector vecTemp; //LRC
		StopSound();
		vecTemp = (nextPos - pev->origin); //LRC

		UTIL_SetAvelocity(this, g_vecZero);
		float distance = vecTemp.Length(); //LRC
		m_oldSpeed = pev->speed;


		pev->speed = 0;
		
		// Move to the dead end
		
		// Are we there yet?
		if ( distance > 0 )
		{
			// no, how long to get there?
			time = distance / m_oldSpeed;
			UTIL_SetVelocity( this, vecTemp * (m_oldSpeed / distance) );
			SetThink( DeadEnd );
			NextThink( time, FALSE );
		}
		else
		{
			UTIL_SetVelocity( this, vecTemp );
			DeadEnd();
		}
	}
}


void CFuncTrackTrain::DeadEnd( void )
{
	// Fire the dead-end target if there is one
	CBaseEntity *pTrack, *pNext;

	pTrack = pPath;

	ALERT( at_aiconsole, "TRAIN(%s): Dead end ", STRING(pev->targetname) );
	// Find the dead end path node
	// HACKHACK -- This is bugly, but the train can actually stop moving at a different node depending on it's speed
	// so we have to traverse the list to it's end.
	if ( pTrack )
	{
		if ( m_oldSpeed < 0 )
		{
			do
			{
				pNext = ((CPathTrack *)pTrack)->ValidPath( pTrack->GetPrev(), TRUE );
				if ( pNext )
					pTrack = pNext;
			} while ( pNext );
		}
		else
		{
			do
			{
				pNext = ((CPathTrack *)pTrack)->ValidPath( pTrack->GetNext(), TRUE );
				if ( pNext )
					pTrack = pNext;
			} while ( pNext );
		}
	}

	UTIL_SetVelocity( this, g_vecZero );
	UTIL_SetAvelocity(this, g_vecZero );
	
	if ( pTrack )
	{
		ALERT( at_aiconsole, "at %s\n", STRING(pTrack->pev->targetname) );
		if ( pTrack->pev->netname )
			UTIL_FireTargets( pTrack->pev->netname, this, this, USE_TOGGLE );
	}
}


void CFuncTrackTrain :: SetControls( entvars_t *pevControls )
{
	Vector offset = pevControls->origin - pev->oldorigin;

	m_controlMins = pevControls->mins + offset;
	m_controlMaxs = pevControls->maxs + offset;
}


BOOL CFuncTrackTrain :: OnControls( entvars_t *pevTest )
{
	Vector offset = pevTest->origin - pev->origin;

	if ( pev->spawnflags & SF_TRACKTRAIN_NOCONTROL )
		return FALSE;

	// Transform offset into local coordinates
	UTIL_MakeVectors( pev->angles );
	Vector local;
	local.x = DotProduct( offset, gpGlobals->v_forward );
	local.y = -DotProduct( offset, gpGlobals->v_right );
	local.z = DotProduct( offset, gpGlobals->v_up );

	if ( local.x >= m_controlMins.x && local.y >= m_controlMins.y && local.z >= m_controlMins.z &&
		 local.x <= m_controlMaxs.x && local.y <= m_controlMaxs.y && local.z <= m_controlMaxs.z )
		 return TRUE;

	return FALSE;
}

void CFuncTrackTrain :: NearestPath( void )
{
	CBaseEntity *pTrack = NULL;
	CBaseEntity *pNearest = NULL;
	float dist, closest;

	closest = 1024;

	while ((pTrack = UTIL_FindEntityInSphere( pTrack, pev->origin, 1024 )) != NULL)
	{
		// filter out non-tracks
		if ( !(pTrack->pev->flags & (FL_CLIENT|FL_MONSTER)) && FClassnameIs( pTrack->pev, "path_track" ) )
		{
			dist = (pev->origin - pTrack->pev->origin).Length();
			if ( dist < closest )
			{
				closest = dist;
				pNearest = pTrack;
			}
		}
	}

	if ( !pNearest )
	{
		ALERT( at_console, "Can't find a nearby track !!!\n" );
		SetThink(NULL);
		return;
	}

	ALERT( at_aiconsole, "TRAIN: %s, Nearest track is %s\n", STRING(pev->targetname), STRING(pNearest->pev->targetname) );
	// If I'm closer to the next path_track on this path, then it's my real path
	pTrack = ((CPathTrack *)pNearest)->GetNext();
	if ( pTrack )
	{
		if ( (pev->origin - pTrack->pev->origin).Length() < (pev->origin - pNearest->pev->origin).Length() )
			pNearest = pTrack;
	}

	pPath = (CPathTrack *)pNearest;

	if ( pev->speed != 0 )
	{
		NextThink( 0.1, FALSE );
		SetThink( PostponeNext );
	}
}
LINK_ENTITY_TO_CLASS( func_tracktrain, CFuncTrackTrain );

//=======================================================================
// volume of space that the player must stand in to control the train
//=======================================================================
class CFuncTrainControls : public CPointEntity
{
public:
	void Spawn( void );
	void PostSpawn( void );
};
LINK_ENTITY_TO_CLASS( func_traincontrols, CFuncTrainControls );

void CFuncTrainControls :: PostSpawn( void )
{
	CBaseEntity *pTarget = NULL;

	do 
	{
	pTarget = UTIL_FindEntityByTargetname( pTarget, STRING(pev->target) );
	} while ( pTarget && !FClassnameIs(pTarget->pev, "func_tracktrain") );

	if ( !pTarget )
	{
		ALERT( at_console, "TrackTrainControls: No train %s\n", STRING(pev->target) );
		return;
	}

	CFuncTrackTrain *ptrain = (CFuncTrackTrain*)pTarget;
	ptrain->SetControls( pev );
	UTIL_Remove( this );
}

void CFuncTrainControls :: Spawn( void )
{
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	UTIL_SetModel( ENT(pev), pev->model );

	UTIL_SetSize( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( this, pev->origin );
}