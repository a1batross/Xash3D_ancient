//=======================================================================
//			Copyright (C) Shambler Team 2005
//		    	     utils.h - Utility code. 
//			  Really not optional after all.			    
//======================================================================= 

#ifndef UTIL_H
#define UTIL_H

#include <string.h>

#include "te_message.h"
#include "shake.h"

#ifndef ACTIVITY_H
#include "activity.h"
#endif

#ifndef ENGINECALLBACK_H
#include "enginecallback.h"
#endif

#define MSG_ONE_UNRELIABLE	0	// Send to one client, but don't put in reliable stream, put in unreliable datagram ( could be dropped )
#define MSG_BROADCAST	1	// unreliable to all
#define MSG_PAS		2	// Ents in PAS of org
#define MSG_PVS		3	// Ents in PVS of org
#define MSG_ONE		4	// reliable to one (msg_entity)
#define MSG_ALL		5	// reliable to all
#define MSG_PAS_R		6	// Reliable to PAS
#define MSG_PVS_R		7	// Reliable to PVS

inline void MESSAGE_BEGIN( int msg_dest, int msg_type, const float *pOrigin, entvars_t *ent );  // implementation later in this file

extern globalvars_t				*gpGlobals;

#define MAX_CHILDS 100


inline edict_t *FIND_ENTITY_BY_CLASSNAME(edict_t *entStart, const char *pszName) 
{
	return FIND_ENTITY_BY_STRING(entStart, "classname", pszName);
}	

inline edict_t *FIND_ENTITY_BY_TARGETNAME(edict_t *entStart, const char *pszName) 
{
	return FIND_ENTITY_BY_STRING(entStart, "targetname", pszName);
}	

// for doing a reverse lookup. Say you have a door, and want to find its button.
inline edict_t *FIND_ENTITY_BY_TARGET(edict_t *entStart, const char *pszName) 
{
	return FIND_ENTITY_BY_STRING(entStart, "target", pszName);
}	

// Keeps clutter down a bit, when writing key-value pairs
#define WRITEKEY_INT(pf, szKeyName, iKeyValue) ENGINE_FPRINTF(pf, "\"%s\" \"%d\"\n", szKeyName, iKeyValue)
#define WRITEKEY_FLOAT(pf, szKeyName, flKeyValue)								\
		ENGINE_FPRINTF(pf, "\"%s\" \"%f\"\n", szKeyName, flKeyValue)
#define WRITEKEY_STRING(pf, szKeyName, szKeyValue)								\
		ENGINE_FPRINTF(pf, "\"%s\" \"%s\"\n", szKeyName, szKeyValue)
#define WRITEKEY_VECTOR(pf, szKeyName, flX, flY, flZ)							\
		ENGINE_FPRINTF(pf, "\"%s\" \"%f %f %f\"\n", szKeyName, flX, flY, flZ)

// Keeps clutter down a bit, when using a float as a bit-vector
#define SetBits(flBitVector, bits)		((flBitVector) = (int)(flBitVector) | (bits))
#define ClearBits(flBitVector, bits)	((flBitVector) = (int)(flBitVector) & ~(bits))
#define FBitSet(flBitVector, bit)		((int)(flBitVector) & (bit))

// Makes these more explicit, and easier to find
#define FILE_GLOBAL static
#define DLL_GLOBAL

extern DLL_GLOBAL int DirToBits( const Vector dir );

extern DLL_GLOBAL int			g_sModelIndexErrorModel;
extern DLL_GLOBAL int			g_sModelIndexErrorSprite;
extern DLL_GLOBAL int			g_sModelIndexNullModel;
extern DLL_GLOBAL int			g_sModelIndexNullSprite;

// Until we figure out why "const" gives the compiler problems, we'll just have to use
// this bogus "empty" define to mark things as constant.
#define CONSTANT

// More explicit than "int"
typedef int EOFFSET;

// In case it's not alread defined
typedef int BOOL;

// Keeps clutter down a bit, when declaring external entity/global method prototypes
#define DECLARE_GLOBAL_METHOD(MethodName)  extern void DLLEXPORT MethodName( void )
#define GLOBAL_METHOD(funcname)					void DLLEXPORT funcname(void)

// This is the glue that hooks .MAP entity class names to our CPP classes
// The _declspec forces them to be exported by name so we can do a lookup with GetProcAddress()
// The function is used to intialize / allocate the object for the entity
#ifdef _WIN32
#define LINK_ENTITY_TO_CLASS(mapClassName,DLLClassName) \
	extern "C" _declspec( dllexport ) void mapClassName( entvars_t *pev ); \
	void mapClassName( entvars_t *pev ) { GetClassPtr( (DLLClassName *)pev ); }
#else
#define LINK_ENTITY_TO_CLASS(mapClassName,DLLClassName) extern "C" void mapClassName( entvars_t *pev ); void mapClassName( entvars_t *pev ) { GetClassPtr( (DLLClassName *)pev ); }
#endif


//
// Conversion among the three types of "entity", including identity-conversions.
//
#ifdef DEBUG
	extern edict_t *DBG_EntOfVars(const entvars_t *pev);
	inline edict_t *ENT(const entvars_t *pev)	{ return DBG_EntOfVars(pev); }
#else
	inline edict_t *ENT(const entvars_t *pev)	{ return pev->pContainingEntity; }
#endif
inline edict_t *ENT(edict_t *pent)		{ return pent; }
inline edict_t *ENT(EOFFSET eoffset)			{ return (*g_engfuncs.pfnPEntityOfEntOffset)(eoffset); }
inline EOFFSET OFFSET(EOFFSET eoffset)			{ return eoffset; }
inline EOFFSET OFFSET(const edict_t *pent)	
{ 
#if _DEBUG
	if ( !pent )
		ALERT( at_error, "Bad ent in OFFSET()\n" );
#endif
	return (*g_engfuncs.pfnEntOffsetOfPEntity)(pent); 
}
inline EOFFSET OFFSET(entvars_t *pev)				
{ 
#if _DEBUG
	if ( !pev )
		ALERT( at_error, "Bad pev in OFFSET()\n" );
#endif
	return OFFSET(ENT(pev)); 
}
inline entvars_t *VARS(entvars_t *pev)					{ return pev; }

inline entvars_t *VARS(edict_t *pent)			
{ 
	if ( !pent )
		return NULL;

	return &pent->v; 
}

inline entvars_t* VARS(EOFFSET eoffset)				{ return VARS(ENT(eoffset)); }
inline int	  ENTINDEX(edict_t *pEdict)			{ return (*g_engfuncs.pfnIndexOfEdict)(pEdict); }
inline edict_t* INDEXENT( int iEdictNum )		{ return (*g_engfuncs.pfnPEntityOfEntIndex)(iEdictNum); }
inline void MESSAGE_BEGIN( int msg_dest, int msg_type, const float *pOrigin, entvars_t *ent ) {
	(*g_engfuncs.pfnMessageBegin)(msg_dest, msg_type, pOrigin, ENT(ent));
}


class CBaseEntity;
class CBasePlayer;

//=========================================================
// RandomRange - for random values
//=========================================================
class RandomRange
{
public:
	float m_flMax, m_flMin; // class members

	RandomRange() { m_flMin = m_flMax = 0; }
	RandomRange( float fValue ) { m_flMin = m_flMax = fValue; }
	RandomRange( float fMin, float fMax ) { m_flMin = fMin; m_flMax = fMax; }
	RandomRange( const char *szToken )
	{
		char *cOneDot = NULL;
		m_flMin = m_flMax = 0;
	
		for( const char *c = szToken; *c; c++ )
		{
			if( *c == '.' )
			{
				if( cOneDot != NULL )
				{
					// found two dots in a row - it's a range
					*cOneDot = 0; // null terminate the first number
					m_flMin = atof( szToken ); // parse the first number
					*cOneDot = '.'; // change it back, just in case
					c++;
					m_flMax = atof( c ); // parse the second number
					return;
				}
				else cOneDot = (char *)c;
			}
			else cOneDot = NULL;
		}

		// no range, just record the number
		m_flMax = m_flMin = atof( szToken );
          }
	
	float Random() { return RANDOM_FLOAT( m_flMin, m_flMax ); }

	// array access...
	float operator[](int i) const { return ((float*)this)[i];}
	float& operator[](int i)  { return ((float*)this)[i];}
};

// Testing the three types of "entity" for nullity
//LRC- four types, rather; see cbase.h
#define eoNullEntity 0
extern BOOL FNullEnt(EOFFSET eoffset);
extern BOOL FNullEnt(const edict_t* pent);
extern BOOL FNullEnt(entvars_t* pev);
extern BOOL FNullEnt( CBaseEntity *ent );

// Testing strings for nullity
// g-cont. radnomize hull nullity yep!
// and Doom3 colloisation
#define iStringNull 0
inline BOOL FStringNull( int iString )		{ return iString == iStringNull; }
inline BOOL FStringNull( const char *szString )	{ return (szString && *szString) ? FALSE : TRUE; };
inline BOOL FStringNull( Vector vString )	{ return vString == Vector( 0, 0, 0); }

#define cchMapNameMost 32

typedef enum
{
	point_hull = 0,
	human_hull = 1,
	large_hull = 2,
	head_hull = 3
};

// Dot products for view cone checking
#define VIEW_FIELD_FULL		(float)-1.0 // +-180 degrees
#define VIEW_FIELD_WIDE		(float)-0.7 // +-135 degrees 0.1 // +-85 degrees, used for full FOV checks 
#define VIEW_FIELD_NARROW		(float)0.7 // +-45 degrees, more narrow check used to set up ranged attacks
#define VIEW_FIELD_ULTRA_NARROW	(float)0.9 // +-25 degrees, more narrow check used to set up ranged attacks

// All monsters need this data
#define		DONT_BLEED			-1
#define		BLOOD_COLOR_RED		(byte)247
#define		BLOOD_COLOR_YELLOW	(byte)195
#define		BLOOD_COLOR_GREEN	BLOOD_COLOR_YELLOW

typedef enum 
{
	MONSTERSTATE_NONE = 0,
	MONSTERSTATE_IDLE,
	MONSTERSTATE_COMBAT,
	MONSTERSTATE_ALERT,
	MONSTERSTATE_HUNT,
	MONSTERSTATE_PRONE,
	MONSTERSTATE_SCRIPT,
	MONSTERSTATE_PLAYDEAD,
	MONSTERSTATE_DEAD

} MONSTERSTATE;

//LRC- the values used for the new "global states" mechanism.
typedef enum
{
	STATE_OFF = 0,	// disabled, inactive, invisible, closed, or stateless. Or non-alert monster.
	STATE_ON,		// enabled, active, visisble, or open. Or alert monster.
	STATE_TURN_ON,  // door opening, env_fade fading in, etc.
	STATE_TURN_OFF, // door closing, monster dying (?).
	STATE_IN_USE,	// player is in control (train/tank/barney/scientist).
	STATE_DEAD,	// entity dead
} STATE;

typedef enum 
{
	GLOBAL_OFF = 0,
	GLOBAL_ON = 1,
	GLOBAL_DEAD = 2
} GLOBALESTATE;
 
 //list of use type
typedef enum
{
	USE_OFF 	 = 0,	//deactivate entity
	USE_ON 	 = 1,	//activate entity
	USE_SET 	 = 2,	//control over entity - e.g tracktrain, tank, camera etc.
	USE_RESET  = 3,	//reset entity to initial position
	USE_TOGGLE = 4,	//default command - toggle on/off
	USE_REMOVE = 5,	//remove entity from map
	USE_SHOWINFO = 6,	//show different info (debug mode)
} USE_TYPE;

extern char* GetStringForUseType( USE_TYPE useType );
extern char* GetStringForState( STATE state );
extern char* GetStringForGlobalState( GLOBALESTATE state );
extern char* GetContentsString( int contents );
extern void PrintStringForDamage( int dmgbits );
extern char* GetStringForDecalName( int decalname );
extern DLL_GLOBAL 
// Misc useful
inline BOOL FStrEq(const char*sz1, const char*sz2) { return (strcmp(sz1, sz2) == 0); }
inline BOOL FClassnameIs(edict_t* pent, const char* szClassname) { return FStrEq(STRING(VARS(pent)->classname), szClassname); }
inline BOOL FClassnameIs(entvars_t* pev, const char* szClassname) { return FStrEq(STRING(pev->classname), szClassname); }

// Ugly technique to override base member functions
// Normally it's illegal to cast a pointer to a member function of a derived class to a pointer to a
// member function of a base class.  static_cast is a sleezy way around that problem.
#ifdef _DEBUG

#define SetThink( a ) ThinkSet( static_cast <void (CBaseEntity::*)(void)> (a), #a )
#define SetTouch( a ) TouchSet( static_cast <void (CBaseEntity::*)(CBaseEntity *)> (a), #a )
#define SetUse( a ) UseSet( static_cast <void (CBaseEntity::*)(	CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )> (a), #a )
#define SetBlocked( a ) BlockedSet( static_cast <void (CBaseEntity::*)(CBaseEntity *)> (a), #a )

#else

#define SetThink( a ) m_pfnThink = static_cast <void (CBaseEntity::*)(void)> (a)
#define SetTouch( a ) m_pfnTouch = static_cast <void (CBaseEntity::*)(CBaseEntity *)> (a)
#define SetUse( a ) m_pfnUse = static_cast <void (CBaseEntity::*)( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )> (a)
#define SetBlocked( a ) m_pfnBlocked = static_cast <void (CBaseEntity::*)(CBaseEntity *)> (a)

#endif

// Misc. Prototypes
extern void			UTIL_SetSize			(entvars_t* pev, const Vector &vecMin, const Vector &vecMax);
extern float		UTIL_VecToYaw			(const Vector &vec);
extern Vector		UTIL_VecToAngles		(const Vector &vec);
extern float		UTIL_AngleMod			(float a);
extern float		UTIL_AngleDiff			( float destAngle, float srcAngle );

extern Vector		UTIL_AxisRotationToAngles	(const Vector &vec, float angle); //LRC
extern Vector		UTIL_AxisRotationToVec	(const Vector &vec, float angle); //LRC

extern CBaseEntity	*UTIL_FindEntityInSphere(CBaseEntity *pStartEntity, const Vector &vecCenter, float flRadius);
extern CBaseEntity	*UTIL_FindEntityByString(CBaseEntity *pStartEntity, const char *szKeyword, const char *szValue );
extern CBaseEntity	*UTIL_FindEntityByClassname(CBaseEntity *pStartEntity, const char *szName );
extern CBaseEntity	*UTIL_FindEntityByTargetname(CBaseEntity *pStartEntity, const char *szName );
extern CBaseEntity	*UTIL_FindEntityByTargetname(CBaseEntity *pStartEntity, const char *szName, CBaseEntity *pActivator ); //LRC - for $locus references
extern CBaseEntity	*UTIL_FindEntityByTarget(CBaseEntity *pStartEntity, const char *szName );
extern CBaseEntity	*UTIL_FindEntityGeneric(const char *szName, Vector &vecSrc, float flRadius );
extern CBaseEntity	*UTIL_FindGlobalEntity( string_t classname, string_t globalname );
extern CBaseEntity	*UTIL_FindPlayerInSphere( const Vector &vecCenter, float flRadius );
extern edict_t *UTIL_FindClientTransitions( edict_t *pClient );
extern CBasePlayer	*UTIL_FindPlayerInPVS( edict_t *pent );

// returns a CBaseEntity pointer to a player by index.  Only returns if the player is spawned and connected
// otherwise returns NULL
// Index is 1 based
extern CBaseEntity	*UTIL_PlayerByIndex( int playerIndex );


#define UTIL_EntitiesInPVS( pent )	(*g_engfuncs.pfnEntitiesInPVS)(pent)
extern void UTIL_MakeVectors( const Vector &vecAngles );

// Pass in an array of pointers and an array size, it fills the array and returns the number inserted
extern int			UTIL_MonstersInSphere( CBaseEntity **pList, int listMax, const Vector &center, float radius );
extern int			UTIL_EntitiesInBox( CBaseEntity **pList, int listMax, const Vector &mins, const Vector &maxs, int flagMask );

inline void UTIL_MakeVectorsPrivate( const Vector &vecAngles, float *p_vForward, float *p_vRight, float *p_vUp )
{
	g_engfuncs.pfnAngleVectors( vecAngles, p_vForward, p_vRight, p_vUp );
}

extern void	UTIL_MakeAimVectors		( const Vector &vecAngles ); // like MakeVectors, but assumes pitch isn't inverted
extern void	UTIL_MakeInvVectors		( const Vector &vec, globalvars_t *pgv );

extern void	UTIL_SetEdictOrigin		( edict_t *pEdict, const Vector &vecOrigin );

extern void	UTIL_EmitAmbientSound( edict_t *entity, const Vector &vecOrigin, const char *samp, float vol, float attenuation, int fFlags, int pitch );
extern void	UTIL_ParticleEffect( const Vector &vecOrigin, const Vector &vecDirection, ULONG ulColor, ULONG ulCount );
extern void	UTIL_ScreenShake( const Vector &center, float amplitude, float frequency, float duration, float radius );
extern void	UTIL_ScreenShake( const Vector &center, float amplitude, float frequency, float duration, float radius, ShakeCommand_t eCommand, BOOL bAirShake );
extern void	UTIL_ScreenShakeAll( const Vector &center, float amplitude, float frequency, float duration, ShakeCommand_t eCommand, BOOL bAirShake );
extern void	UTIL_ShowMessage( const char *pString, CBaseEntity *pPlayer );
extern void	UTIL_ShowMessageAll( const char *pString );

extern void UTIL_ScreenFadeAll( const Vector &color, float fadeTime, float fadeHold, int alpha, int flags );
extern void UTIL_ScreenFade( const Vector &color, float fadeTime, float fadeHold, int alpha, int flags, int playernum = 1);

extern void	UTIL_SetFog		( Vector color, int iFadeTime, int iStartDist, int iEndDist, int playernum = 1 );	
extern void	UTIL_SetFogAll		( Vector color, int iFadeTime, int iStartDist, int iEndDist );

typedef enum { ignore_monsters=1, dont_ignore_monsters=0, missile=2 } IGNORE_MONSTERS;
typedef enum { ignore_glass=1, dont_ignore_glass=0 } IGNORE_GLASS;
extern void			UTIL_TraceLine			(const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, edict_t *pentIgnore, TraceResult *ptr);
extern void			UTIL_TraceLine			(const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, IGNORE_GLASS ignoreGlass, edict_t *pentIgnore, TraceResult *ptr);
extern void			UTIL_TraceHull			(const Vector &vecStart, const Vector &vecEnd, IGNORE_MONSTERS igmon, int hullNumber, edict_t *pentIgnore, TraceResult *ptr);
extern TraceResult	UTIL_GetGlobalTrace		(void);
extern void			UTIL_TraceModel			(const Vector &vecStart, const Vector &vecEnd, int hullNumber, edict_t *pentModel, TraceResult *ptr);
extern Vector		UTIL_GetAimVector		(edict_t* pent, float flSpeed);
extern int			UTIL_PointContents		(const Vector &vec);

extern int			UTIL_IsMasterTriggered	(string_t sMaster, CBaseEntity *pActivator);
extern void			UTIL_BloodStream( const Vector &origin, const Vector &direction, int color, int amount );
extern void			UTIL_BloodDrips( const Vector &origin, const Vector &direction, int color, int amount );
extern Vector		UTIL_RandomBloodVector( void );
extern BOOL			UTIL_ShouldShowBlood( int bloodColor );
extern void			UTIL_BloodDecalTrace( TraceResult *pTrace, int bloodColor );
extern void			UTIL_DecalTrace( TraceResult *pTrace, int decalNumber );
extern void			UTIL_PlayerDecalTrace( TraceResult *pTrace, int playernum, int decalNumber, BOOL bIsCustom );
extern void			UTIL_GunshotDecalTrace( TraceResult *pTrace, int decalNumber );
extern void			UTIL_Sparks( const Vector &position );
extern void			UTIL_Explode( const Vector &center, edict_t *pOwner, int radius, int name = 0 );
extern void			UTIL_Ricochet( const Vector &position, float scale );
extern void			UTIL_StringToVector( float *pVector, const char *pString );
extern void			UTIL_StringToRandomVector( float *pVector, const char *pString ); //LRC
extern void			UTIL_StringToIntArray( int *pVector, int count, const char *pString );
extern Vector		UTIL_ClampVectorToBox( const Vector &input, const Vector &clampSize );
extern float		UTIL_Approach( float target, float value, float speed );
extern float		UTIL_ApproachAngle( float target, float value, float speed );
extern float		UTIL_AngleDistance( float next, float cur );

extern char			*UTIL_VarArgs( char *format, ... );
extern void			UTIL_Remove( CBaseEntity *pEntity );
extern BOOL			UTIL_IsValidEntity( edict_t *pent );
extern BOOL			UTIL_TeamsMatch( const char *pTeamName1, const char *pTeamName2 );
extern BOOL			UTIL_IsFacing( entvars_t *pevTest, const Vector &reference ); //LRC

// Use for ease-in, ease-out style interpolation (accel/decel)
extern float		UTIL_SplineFraction( float value, float scale );

// Search for water transition along a vertical line
extern float		UTIL_WaterLevel( const Vector &position, float minz, float maxz );
extern void			UTIL_Bubbles( Vector mins, Vector maxs, int count );
extern void			UTIL_BubbleTrail( Vector from, Vector to, int count );

// allows precacheing of other entities
// prints a message to each client
extern void			UTIL_ClientPrintAll( int msg_dest, const char *msg_name, const char *param1 = NULL, const char *param2 = NULL, const char *param3 = NULL, const char *param4 = NULL );
inline void			UTIL_CenterPrintAll( const char *msg_name, const char *param1 = NULL, const char *param2 = NULL, const char *param3 = NULL, const char *param4 = NULL ) 
{
	UTIL_ClientPrintAll( HUD_PRINTCENTER, msg_name, param1, param2, param3, param4 );
}

class CBasePlayerWeapon;
class CBasePlayer;
extern BOOL UTIL_GetNextBestWeapon( CBasePlayer *pPlayer, CBasePlayerWeapon *pCurrentWeapon );

// prints messages through the HUD
extern void ClientPrint( entvars_t *client, int msg_dest, const char *msg_name, const char *param1 = NULL, const char *param2 = NULL, const char *param3 = NULL, const char *param4 = NULL );

// prints a message to the HUD say (chat)
extern void			UTIL_SayText( const char *pText, CBaseEntity *pEntity );
extern void			UTIL_SayTextAll( const char *pText, CBaseEntity *pEntity );


typedef struct hudtextparms_s
{
	float		x;
	float		y;
	int			effect;
	byte		r1, g1, b1, a1;
	byte		r2, g2, b2, a2;
	float		fadeinTime;
	float		fadeoutTime;
	float		holdTime;
	float		fxTime;
	int			channel;
} hudtextparms_t;

// prints as transparent 'title' to the HUD
extern void			UTIL_HudMessageAll( const hudtextparms_t &textparms, const char *pMessage );
extern void			UTIL_HudMessage( CBaseEntity *pEntity, const hudtextparms_t &textparms, const char *pMessage );

// for handy use with ClientPrint params
extern char *UTIL_dtos1( int d );
extern char *UTIL_dtos2( int d );
extern char *UTIL_dtos3( int d );
extern char *UTIL_dtos4( int d );

// Writes message to console with timestamp and FragLog header.
extern void UTIL_LogPrintf( char *fmt, ... );
extern const char *UTIL_FileExtension( const char *in );

// Sorta like FInViewCone, but for nonmonsters. 
extern float UTIL_DotPoints ( const Vector &vecSrc, const Vector &vecCheck, const Vector &vecDir );

extern void UTIL_StripToken( const char *pKey, char *pDest );// for redundant keynames

// Misc functions
extern void UTIL_SetMovedir(entvars_t* pev);
extern void UTIL_SynchDoors( CBaseEntity *pEntity );
extern Vector UTIL_GetMovedir( Vector vecAngles );
extern Vector VecBModelOrigin( entvars_t* pevBModel );
extern int BuildChangeList( LEVELLIST *pLevelList, int maxList );

//
// How did I ever live without ASSERT?
//
#ifdef	DEBUG
void DBG_AssertFunction(BOOL fExpr, const char* szExpr, const char* szFile, int szLine, const char* szMessage);
#define ASSERT(f)		DBG_AssertFunction(f, #f, __FILE__, __LINE__, NULL)
#define ASSERTSZ(f, sz)	DBG_AssertFunction(f, #f, __FILE__, __LINE__, sz)
#else	// !DEBUG
#define ASSERT(f)
#define ASSERTSZ(f, sz)
#endif	// !DEBUG


extern DLL_GLOBAL const Vector g_vecZero;

//
// Constants that were used only by QC (maybe not used at all now)
//
// Un-comment only as needed
//
#define LANGUAGE_ENGLISH				0
#define LANGUAGE_GERMAN					1
#define LANGUAGE_FRENCH					2
#define LANGUAGE_BRITISH				3

extern DLL_GLOBAL int			g_Language;

#define AMBIENT_SOUND_STATIC			0	// medium radius attenuation
#define AMBIENT_SOUND_EVERYWHERE		1
#define AMBIENT_SOUND_SMALLRADIUS		2
#define AMBIENT_SOUND_MEDIUMRADIUS		4
#define AMBIENT_SOUND_LARGERADIUS		8
#define AMBIENT_SOUND_START_SILENT		16
#define AMBIENT_SOUND_NOT_LOOPING		32

#define SPEAKER_START_SILENT			1	// wait for trigger 'on' to start announcements

#define LFO_SQUARE				1
#define LFO_TRIANGLE			2
#define LFO_RANDOM				3

// func_rotating
#define SF_BRUSH_ROTATE_Y_AXIS		0 //!?! (LRC)
#define SF_BRUSH_ROTATE_INSTANT		1
#define SF_BRUSH_ROTATE_BACKWARDS		2
#define SF_BRUSH_ROTATE_Z_AXIS		4
#define SF_BRUSH_ROTATE_X_AXIS		8
#define SF_PENDULUM_AUTO_RETURN		16
#define SF_PENDULUM_PASSABLE			32

#define SF_BRUSH_ROTATE_SMALLRADIUS		128
#define SF_BRUSH_ROTATE_MEDIUMRADIUS		256
#define SF_BRUSH_ROTATE_LARGERADIUS		512

#define PUSH_BLOCK_ONLY_X	1
#define PUSH_BLOCK_ONLY_Y	2

#define VEC_HULL_MIN		Vector(-16, -16, -36)
#define VEC_HULL_MAX		Vector( 16,  16,  36)
#define VEC_HUMAN_HULL_MIN	Vector( -16, -16, 0 )
#define VEC_HUMAN_HULL_MAX	Vector( 16, 16, 72 )
#define VEC_HUMAN_HULL_DUCK	Vector( 16, 16, 36 )

#define VEC_VIEW			Vector( 0, 0, 28 )

#define VEC_DUCK_HULL_MIN	Vector(-16, -16, -18 )
#define VEC_DUCK_HULL_MAX	Vector( 16,  16,  18)
#define VEC_DUCK_VIEW		Vector( 0, 0, 12 )

// triggers
#define	SF_TRIGGER_ALLOWMONSTERS	1// monsters allowed to fire this trigger
#define	SF_TRIGGER_NOCLIENTS		2// players not allowed to fire this trigger
#define SF_TRIGGER_PUSHABLES		4// only pushables can fire this trigger
#define SF_TRIGGER_EVERYTHING		8// everything else can fire this trigger (e.g. gibs, rockets)

// func breakable
#define SF_BREAK_TRIGGER_ONLY		1// may only be broken by trigger
#define SF_BREAK_TOUCH		2// can be 'crashed through' by running player (plate glass)
#define SF_BREAK_PRESSURE		4// can be broken by a player standing on it
#define SF_BREAK_CROWBAR		256// instant break if hit with crowbar

// func_pushable (it's also func_breakable, so don't collide with those flags)
#define SF_PUSH_BREAKABLE		128
#define SF_PUSH_NOPULL			512//LRC
#define SF_PUSH_USECUSTOMSIZE	0x800000 //LRC, not yet used

#define SF_LIGHT_START_OFF		1

#define SPAWNFLAG_NOMESSAGE	1
#define SPAWNFLAG_NOTOUCH	1
#define SPAWNFLAG_DROIDONLY	4

#define SPAWNFLAG_USEONLY	1		// can't be touched, must be used (buttons)

#define TELE_PLAYER_ONLY	1
#define TELE_SILENT			2

#define SF_TRIG_PUSH_ONCE		1


// Sound Utilities

// sentence groups
#define CBSENTENCENAME_MAX 16
#define CVOXFILESENTENCEMAX		1536		// max number of sentences in game. NOTE: this must match

static unsigned short FixedUnsigned16( float value, float scale )
{
	int output;

	output = value * scale;
	if ( output < 0 ) output = 0;
	if ( output > 0xFFFF ) output = 0xFFFF;
	return (unsigned short)output;
}

static short FixedSigned16( float value, float scale )
{
	int output;

	output = value * scale;
	if ( output > 32767 ) output = 32767;
	if ( output < -32768 ) output = -32768;
	return (short)output;
}										// CVOXFILESENTENCEMAX in engine\sound.h!!!

extern char gszallsentencenames[CVOXFILESENTENCEMAX][CBSENTENCENAME_MAX];
extern int gcallsentences;

int USENTENCEG_Pick(int isentenceg, char *szfound);
int USENTENCEG_PickSequential(int isentenceg, char *szfound, int ipick, int freset);
void USENTENCEG_InitLRU(unsigned char *plru, int count);

void SENTENCEG_Init();
void SENTENCEG_Stop(edict_t *entity, int isentenceg, int ipick);
int SENTENCEG_PlayRndI(edict_t *entity, int isentenceg, float volume, float attenuation, int flags, int pitch);
int SENTENCEG_PlayRndSz(edict_t *entity, const char *szrootname, float volume, float attenuation, int flags, int pitch);
int SENTENCEG_PlaySequentialSz(edict_t *entity, const char *szrootname, float volume, float attenuation, int flags, int pitch, int ipick, int freset);
int SENTENCEG_GetIndex(const char *szrootname);
int SENTENCEG_Lookup(const char *sample, char *sentencenum);

void TEXTURETYPE_Init();
char TEXTURETYPE_Find(char *name);
float TEXTURETYPE_PlaySound(TraceResult *ptr,  Vector vecSrc, Vector vecEnd, int iBulletType);

// NOTE: use EMIT_SOUND_DYN to set the pitch of a sound. Pitch of 100
// is no pitch shift.  Pitch > 100 up to 255 is a higher pitch, pitch < 100
// down to 1 is a lower pitch.   150 to 70 is the realistic range.
// EMIT_SOUND_DYN with pitch != 100 should be used sparingly, as it's not quite as
// fast as EMIT_SOUND (the pitchshift mixer is not native coded).

void EMIT_SOUND_DYN(edict_t *entity, int channel, const char *sample, float volume, float attenuation,
						   int flags, int pitch);


inline void EMIT_SOUND(edict_t *entity, int channel, const char *sample, float volume, float attenuation)
{
	EMIT_SOUND_DYN(entity, channel, sample, volume, attenuation, 0, PITCH_NORM);
}

inline void STOP_SOUND(edict_t *entity, int channel, const char *sample)
{
	EMIT_SOUND_DYN(entity, channel, sample, 0, 0, SND_STOP, PITCH_NORM);
}

void EMIT_SOUND_SUIT(edict_t *entity, const char *sample);
void EMIT_GROUPID_SUIT(edict_t *entity, int isentenceg);
void EMIT_GROUPNAME_SUIT(edict_t *entity, const char *groupname);

#define PRECACHE_SOUND_ARRAY( a ) \
	{ for (int i = 0; i < ARRAYSIZE( a ); i++ ) PRECACHE_SOUND((char *) a [i]); }

#define EMIT_SOUND_ARRAY_DYN( chan, array ) \
	EMIT_SOUND_DYN ( ENT(pev), chan , array [ RANDOM_LONG(0,ARRAYSIZE( array )-1) ], 1.0, ATTN_NORM, 0, RANDOM_LONG(95,105) ); 

#define RANDOM_SOUND_ARRAY( array ) (array) [ RANDOM_LONG(0,ARRAYSIZE( (array) )-1) ]

int UTIL_SharedRandomLong( unsigned int seed, int low, int high );
float UTIL_SharedRandomFloat( unsigned int seed, float low, float high );

float UTIL_WeaponTimeBase( void );
int GetStdLightStyle (int iStyle); //LRC- declared here so it can be used by everything that
									// needs to deal with the standard lightstyles.
// for trigger_viewset
int HaveCamerasInPVS( edict_t* edict );
BOOL IsMultiplayer ( void );
BOOL IsDeatchmatch ( void );

void UTIL_SetView( int ViewEntity = 0, int flags = 0 );
void UTIL_SetView( CBaseEntity *pActivator, int ViewEntity = 0, int flags = 0 );
void UTIL_SetView( CBaseEntity *pActivator, CBaseEntity *pViewEnt = 0, int flags = 0 );

void UTIL_SetModel( edict_t *e, string_t s, const char *c );
void UTIL_SetModel( edict_t *e, const char *model );
void UTIL_SetModel( edict_t *e, string_t model );
int UTIL_PrecacheModel( const char* s );
int UTIL_PrecacheModel( string_t s, const char *e );
int UTIL_PrecacheSound( const char* s );
int UTIL_PrecacheSound( string_t s, const char *e );
int UTIL_PrecacheModel( string_t s );
int UTIL_PrecacheSound( string_t s );
void UTIL_PrecacheEntity( const char *szClassname );

int UTIL_PrecacheAurora( string_t s );
int UTIL_PrecacheAurora( const char *s );
void UTIL_SetAurora( CBaseEntity *pAttach, int aur, int attachment = 0 );
void UTIL_SetBeams( char *szFile, CBaseEntity *pStart, CBaseEntity *pEnd );

extern int giAmmoIndex;

Vector UTIL_GetAngleDistance( Vector vecAngles, float distance );
void UTIL_SetThink ( CBaseEntity *pEnt );

void UTIL_AssignOrigin( CBaseEntity* pEntity, const Vector vecOrigin, BOOL bInitiator = TRUE);
void UTIL_AssignAngles( CBaseEntity *pEntity, const Vector vecAngles, BOOL bInitiator = TRUE);
void UTIL_ComplexRotate( CBaseEntity *pParent, CBaseEntity *pChild, const Vector &dest, float m_flTravelTime );

void UTIL_SetAngles	( CBaseEntity* pEntity, const Vector &vecAngles );
void UTIL_SetOrigin ( CBaseEntity* pEntity, const Vector &vecOrigin );

void UTIL_SetVelocity ( CBaseEntity *pEnt, const Vector vecSet );
void UTIL_SetAvelocity( CBaseEntity *pEnt, const Vector vecSet );

void UTIL_SetChildVelocity( CBaseEntity *pEnt, const Vector vecSet, int loopbreaker );
void UTIL_SetChildAvelocity( CBaseEntity *pEnt, const Vector vecSet, int loopbreaker );
void UTIL_MergePos ( CBaseEntity *pEnt, int loopbreaker = MAX_CHILDS);

#define CHECK_FLAG(x) if(Ent->pFlags & x) strcat(szFlags,#x", ")
#define clamp(a,min,max)	((a < min)?(min):((a > max)?(max):(a)))
#define DECLARE_CLASS( className, baseClassName ) 	\
		typedef baseClassName BaseClass; 	\
		typedef className ThisClass;

//COM_Utils
void UTIL_PrecacheResourse( void );
void Msg( char *szText, ... );
void DevMsg( char *szText, ... );
char *COM_ParseFile( char *data, char *token );
char *COM_Parse( char *data );
void COM_FreeFile (char *buffer);


//Xash Parent System Flags
#define PF_AFFECT		(1<<0)		//this is child entity
#define PF_DESIRED		(1<<1)
#define PF_ACTION		(1<<2)		//this is chain entity (set automatically)
#define PF_MOVENONE		(1<<3)		//this entity has MOVETYPE_NONE before set parent (needs for return)
#define PF_CORECTSPEED	(1<<4)		//force child to parent coordintes e.g. sprite to attachment
#define PF_MERGEPOS		(1<<5)		//Merge current pos for entity
#define PF_POSTVELOCITY	(1<<6)		//apply and correct velocity from parent
#define PF_POSTAVELOCITY	(1<<7)		//apply and correct avelocity from parent
#define PF_SETTHINK		(1<<8)		//manually force entity to think
#define PF_POSTAFFECT	(1<<9)		//apply
#define PF_PARENTMOVE	(1<<10)		//parent give me velocity or avelocity
#define PF_ANGULAR		(1<<11)		//parent has angular moving
#define PF_POSTORG		(1<<12)		//this entity MUST changed origin only
#define PF_LINKCHILD	(PF_AFFECT|PF_DESIRED|PF_MERGEPOS|PF_POSTORG)

//new system flags
#define EFL_DIRTY_ABSTRANSFORM (1<<13)
#define EFL_DIRTY_ABSVELOCITY (1<<14)
#define EFL_DIRTY_ABSANGVELOCITY (1<<15)

#define POSITION_CHANGED	0x1
#define ANGLES_CHANGED	0x2
#define VELOCITY_CHANGED	0x4

BOOL FClassnameIs(CBaseEntity *pEnt, const char* szClassname);

//affects
int FrameAffect( CBaseEntity *pEnt );
int PostFrameAffect( CBaseEntity *pEnt );

//childs affect
void ChildAffect( CBaseEntity *pEnt, Vector vecAdjustVel, Vector vecAdjustAVel );
void ChildPostAffect ( CBaseEntity *pEnt );

//link operations
void LinkChild(CBaseEntity *pEnt);

void UnlinkFromParent( CBaseEntity *pRemove );
void TransferChildren( CBaseEntity *pOldParent, CBaseEntity *pNewParent );
void LinkChild( CBaseEntity *pParent, CBaseEntity *pChild );
void UnlinkAllChildren( CBaseEntity *pParent );
bool EntityIsParentOf( CBaseEntity *pParent, CBaseEntity *pEntity );

//handles
void HandleAffect( CBaseEntity *pEnt, Vector vecAdjustVel, Vector vecAdjustAVel );
void HandlePostAffect( CBaseEntity *pEnt );

//debug
void GetPFlags( CBaseEntity* Ent );
void GetPInfo( CBaseEntity* Ent );

//Parent utils
void UTIL_MarkChild ( CBaseEntity *pEnt, BOOL correctSpeed = FALSE, BOOL desired = TRUE);
void UTIL_SetPostAffect( CBaseEntity *pEnt );
void UTIL_SetThink ( CBaseEntity *pEnt );
void UTIL_SetAction( CBaseEntity *pEnt );
BOOL SynchLost( CBaseEntity *pEnt );//watching for synchronize

Vector UTIL_RandomVector(void);
Vector UTIL_RandomVector( Vector vmin, Vector vmax );
void UTIL_AngularVector( CBaseEntity *pEnt );
void UTIL_LinearVector( CBaseEntity *pEnt );
float UTIL_CalcDistance( Vector vecAngles );
void UTIL_WatchTarget( CBaseEntity *pWatcher, CBaseEntity *pTarget);
void UTIL_FindBreakable( CBaseEntity *Brush );
edict_t *UTIL_FindLandmark( string_t iLandmarkName );
edict_t *UTIL_FindLandmark( const char *pLandmarkName );
int UTIL_FindTransition( CBaseEntity *pEntity, const char *pVolumeName );
int UTIL_FindTransition( CBaseEntity *pEntity, string_t iVolumeName );
void UTIL_FireTargets( const char *targetName, CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value = 0);
void UTIL_FireTargets( int targetName, CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value = 0);
CBaseEntity *UTIL_FindEntityForward( CBaseEntity *pMe );
void UTIL_FindHullIntersection( const Vector &vecSrc, TraceResult &tr, float *mins, float *maxs, edict_t *pEntity );
int UTIL_LoadSoundPreset( string_t pString );
int UTIL_LoadSoundPreset( const char *pString );
int UTIL_LoadDecalPreset( string_t pString );
int UTIL_LoadDecalPreset( const char *pString );
void UTIL_ChangeLevel( string_t mapname, string_t spotname );
void UTIL_ChangeLevel( const char *szNextMap, const char *szNextSpot );
void AddAmmoName( string_t iAmmoName );

// precache utils
void UTIL_PrecacheEntity( string_t szClassname );
BOOL UTIL_EntIsVisible( entvars_t* pev, entvars_t* pevTarget);

#endif //UTIL_H