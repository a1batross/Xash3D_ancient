//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================

#ifndef WEAPONS_H
#define WEAPONS_H

#include "basebeams.h"
#include "player.h"
#include "bullets.h"
#include "sfx.h"
#include "baserockets.h"
#include "damage.h"
#include "defaults.h"

class CLaserSpot;

// weapon flags
#define ITEM_FLAG_SELECTONEMPTY	1	// this weapon can choose without ammo
#define ITEM_FLAG_NOAUTORELOAD	2	// only manual reload
#define ITEM_FLAG_NOAUTOSWITCHEMPTY	4	// don't switch from this weapon
#define ITEM_FLAG_LIMITINWORLD	8 	// limit in world
#define ITEM_FLAG_EXHAUSTIBLE		16	// A player can totally exhaust their ammo supply and lose this weapon
#define ITEM_FLAG_NODUPLICATE		32	// player can't give this weapon again
#define ITEM_FLAG_USEAUTOAIM		64	// weapon uses autoaim
#define ITEM_FLAG_HIDEAMMO		128	// hide ammo in round

// suit definitions
#define BARNEY_SUIT			0	// just in case
#define GORDON_SUIT			1	// gordon suit
#define NUM_HANDS			2	// number of hands: barney and gordon

#define PLAYER_HAS_SUIT		(m_pPlayer->pev->weapons & ITEM_SUIT)
#define PLAYER_DRAW_SUIT		(pev->body & GORDON_SUIT)
#define MAX_SHOOTSOUNDS		3 // max of random shoot sounds

enum {
	NONE = 0,
	AMMO1,			// fire primary ammo
	AMMO2,			// fire seondary ammo
	LASER_DOT,		// enable laser dot
	ZOOM,			// enable zoom
	FLASHLIGHT,		// enable flashlight
	SWITCHMODE,		// play two beetwen anims and change body
	SWING,			// crowbar swing
};

#define BATTACK_FIREMODE0	0	//both attack firemode 0
#define PATTACK_FIREMODE1	1         //primary attack firemode 1
#define SATTACK_FIREMODE1	2         //secondary attack firemode 2

#define EMPTY_RELOAD	3
#define NORMAL_RELOAD	4

typedef struct
{
	int		iSlot;			// hud slot
	int		iPosition;                    // hud position
	int		iViewModel;         	// path to viewmodel
	int		iWorldModel;        	// path to worldmodel
	char		szAnimExt[16];		// player anim postfix	
	string_t		iszAmmo1;			// ammo 1 type
	int		iMaxAmmo1;		// max ammo 1
	string_t		iszAmmo2;			// ammo 2 type
	int		iMaxAmmo2;		// max ammo 2
	string_t		iszName;			// weapon name
	int		iMaxClip;			// clip size
	int		iId;			// unique weapon Id number
	int		iFlags;			// weapon flags
	int		iWeight;			// for autoselect weapon
	int		attack1;			// attack1 type
	int		attack2;			// attack2 type
	float		fNextAttack;		// nextattack
	float		fNextAttack2;		// next secondary attack
	string_t		firesound[MAX_SHOOTSOUNDS];	// firesounds
	string_t		sfxsound[MAX_SHOOTSOUNDS];	// sfxsound
	int		sndcount;			// fire sound count
	int		sfxcount;			// sfx sound count
	int		emptysnd;			// empty sound
	RandomRange	punchangle1;		// punchangle 1 attack
	RandomRange	punchangle2;		// punchangle 2 attack
	RandomRange	recoil1;			// recoil 1 attack
	RandomRange	recoil2;			// recoil 2 attack
} ItemInfo;

typedef struct
{
	string_t iszName;
	int iMaxCarry;
	int iId;
} AmmoInfo;

class CBasePlayerWeapon : public CBaseAnimating
{
	DECLARE_CLASS( CBasePlayerWeapon, CBaseAnimating );
public:
	virtual void SetObjectCollisionBox( void );

	virtual int Save( CSave &save );
	virtual int Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];
          
	virtual int ObjectCaps( void ) { return CBaseEntity :: ObjectCaps() | FCAP_ACROSS_TRANSITION; }
	virtual int AddToPlayer( CBasePlayer *pPlayer );		// return TRUE if the item you want the item added to the player inventory
	virtual int AddDuplicate( CBasePlayerWeapon *pItem );	// return TRUE if you want your duplicate removed from world
	void EXPORT DestroyItem( void );
	void EXPORT DefaultTouch( CBaseEntity *pOther );		// default weapon touch
	void EXPORT FallThink ( void );			// when an item is first spawned, this think is run to determine when the object has hit the ground.
	void EXPORT Materialize( void );			// make a weapon visible and tangible
	void EXPORT AttemptToMaterialize( void );  		// the weapon desires to become visible and tangible, if the game rules allow for it
          BOOL IsWeapon( void ) { return !strncmp( STRING(pev->classname), "weapon_", 5 ); }
	CBaseEntity* Respawn ( void );// copy a weapon
	void CheckRespawn( void );
	virtual int GetItemInfo(ItemInfo *p);			// get weapon default info
	virtual BOOL CanDeploy( void );
	virtual BOOL CanHolster( void ) { return ( m_iOnControl == 0 ); }; // can this weapon be put away right now?
	virtual void SetNextThink( float delay );
	virtual void Precache( void );
	void EXPORT ItemTouch( CBaseEntity *pOther );
	virtual void Drop( void );
	virtual void AttachToPlayer ( CBasePlayer *pPlayer );
	virtual void Spawn(void);
	virtual int UpdateClientData( CBasePlayer *pPlayer );	// sends hud info to client dll, if things have changed
	virtual CBasePlayerWeapon *GetWeaponPtr( void ) { return (CBasePlayerWeapon *)this; };	
	int FoundAlly( void );
	inline BOOL CanAttack( float attack_time ) { return ( attack_time <= gpGlobals->time ) ? TRUE : FALSE; }

	//ammo operations
	virtual int ExtractAmmo( CBasePlayerWeapon *pWeapon ); 	// Return TRUE if you can add ammo to yourself when picked up
	virtual int ExtractClipAmmo( CBasePlayerWeapon *pWeapon );	// Return TRUE if you can add ammo to yourself when picked up
	virtual int AddWeapon( void ) { ExtractAmmo( this ); return TRUE; };	// Return TRUE if you want to add yourself to the player
	BOOL AddPrimaryAmmo( int iCount, char *szName, int iMaxClip, int iMaxCarry );
	BOOL AddSecondaryAmmo( int iCount, char *szName, int iMaxCarry );
          int GetAmmoType( const char *ammo );//determine ammo type primary or secondary
	int ReturnAmmoIndex( const char *ammo );//return primary or secondary ammo index
	int UseAmmo( const char *ammo, int count = 1 );    	//Determine and use ammo type
	
          // parse weapon script files	    
	int ParseWeaponFile( ItemInfo *II, const char *pfile );		// parse weapon_*.txt
	int ParseWeaponData( ItemInfo *II, const char *file );	     	// parse WeaponData {}
	int ParsePrimaryAttack( ItemInfo *II, const char *pfile );  	// parse PrimaryAttack {}
	int ParseSecondaryAttack( ItemInfo *II, const char *pfile );  	// parse SeconadryAttack {}
	int ParseSoundData( ItemInfo *II, const char *pfile );  		// parse SoundData {}
	// HudData parses on client side	
	void ResetParse( ItemInfo *II );			//reset ItemInfo
          void GenerateID( void );//generate unicum ID number for each weapon
          
          
	//Play Animation methods
	int SetAnimation( Activity activity, float fps = 0 );
          int SetAnimation( char *name, float fps = 0 ); 
	int PlaySequence( Activity activity, float fps = 0 );
	void SetPlayerEffects( const char *ammo, int firemode );
	void PlayAttackSound( int firemode );
          void SendWeaponAnim( int sequence, float fps = 0 );
	float SetNextAttack( float delay ) { return m_pPlayer->m_flNextAttack = gpGlobals->time + delay; }
	float SetNextIdle( float delay ) { return m_flTimeWeaponIdle = gpGlobals->time + delay; }
	float SequenceDuration( void ) { return CBaseAnimating :: SequenceDuration( m_iSequence ); }
          int GetNumBodies( void )
          {
		dstudiohdr_t *pstudiohdr = (dstudiohdr_t *)(GET_MODEL_PTR( ENT( pev )));
		if( pstudiohdr )
		{
			dstudiobodyparts_t *pbodypart = (dstudiobodyparts_t *)((byte *)pstudiohdr + pstudiohdr->bodypartindex) + NUM_HANDS;
			return pbodypart->nummodels;
		}
		return 0;
	}
	
	//main frame
	virtual void ItemPreFrame( void );	// called each frame by the player PreThink
 	virtual void ItemPostFrame( void );	// called each frame by the player PostThink

	//global weapon info struct (refresh on save\load)
	static ItemInfo ItemInfoArray[ MAX_WEAPONS ];	
	static AmmoInfo AmmoInfoArray[ MAX_AMMO_SLOTS ];

	CBasePlayer	*m_pPlayer;
	CBasePlayerWeapon	*m_pNext;
	int		m_iId;		// Weapon unique Id (0 - MAX_WEAPONS)

	//don't save this
	CLaserSpot	*m_pSpot;     	// LTD spot

	//virtual methods for ItemInfo acess
	int		iItemPosition(void) { return ItemInfoArray[ m_iId ].iPosition;		}
	int		iItemSlot( void )	{ return ItemInfoArray[ m_iId ].iSlot + 1;		}
	int		iViewModel( void )	{ return ItemInfoArray[ m_iId ].iViewModel;		}
	int		iWorldModel( void )	{ return ItemInfoArray[ m_iId ].iWorldModel;		}
	int		iMaxAmmo1( void )	{ return ItemInfoArray[ m_iId ].iMaxAmmo1;		}
	int		iMaxAmmo2( void )	{ return ItemInfoArray[ m_iId ].iMaxAmmo2;		}
	int		iMaxClip( void )	{ return ItemInfoArray[ m_iId ].iMaxClip;		}
	int		iWeight( void )	{ return ItemInfoArray[ m_iId ].iWeight;		}
	int		iFlags( void )	{ return ItemInfoArray[ m_iId ].iFlags;			}
	int		iAttack1( void )	{ return ItemInfoArray[ m_iId ].attack1;		}
	int		FireSound( int i )	{ return ItemInfoArray[ m_iId ].firesound[i];		}
	int		SfxSound( int i )	{ return ItemInfoArray[ m_iId ].sfxsound[i];		}
	int		EmptySnd( void )	{ return ItemInfoArray[ m_iId ].emptysnd;		}
	int		sndcnt( void )	{ return ItemInfoArray[ m_iId ].sndcount;		}
	int		sfxcnt( void )	{ return ItemInfoArray[ m_iId ].sfxcount;		}
	int		iAttack2( void )	{ return ItemInfoArray[ m_iId ].attack2;		}
	char		*szAnimExt( void )	{ return ItemInfoArray[ m_iId ].szAnimExt;		}
	float		fNextAttack1( void ){ return ItemInfoArray[ m_iId ].fNextAttack;		}
	float		fNextAttack2( void ){ return ItemInfoArray[ m_iId ].fNextAttack2;		}
	float		fPunchAngle1( void ){ return ItemInfoArray[ m_iId ].punchangle1.Random();	}
	float		fPunchAngle2( void ){ return ItemInfoArray[ m_iId ].punchangle2.Random();	}
	float		fRecoil1( void )	{ return ItemInfoArray[ m_iId ].recoil1.Random();		}
	float		fRecoil2( void )	{ return ItemInfoArray[ m_iId ].recoil2.Random();		}
	const char	*pszAmmo1( void )	{ return STRING( ItemInfoArray[ m_iId ].iszAmmo1 );	}
	const char	*pszAmmo2( void )	{ return STRING( ItemInfoArray[ m_iId ].iszAmmo2 );	}
	const char	*pszName( void )	{ return STRING( ItemInfoArray[ m_iId ].iszName );	}

	BOOL PlayEmptySound( void );//universal empty sound

	// default functions
	BOOL DefaultDeploy( Activity sequence );
	BOOL DefaultHolster( Activity sequence );
          BOOL DefaultReload( Activity sequence );
	void DefaultIdle( void );
	int Shoot ( const char *ammo, Vector vecSpread, int firemode = 0, int cShots = 1 ); //bullet shoot
	int Launch ( const char *ammo, int type = 0 );				    //rocket launch
	int Swing( int fFirst );						    //crowbar swing

	void ZoomUpdate( void );
	void ZoomReset( void );

	// virtaul weapon functions
	int PlayCurrentAttack( int action, int firemode );
	int GetCurrentAttack( const char *ammo, int firemode );
	Vector GetCurrentSpread( const char *ammo );
	int GetBulletType( const char *ammo );	

	inline int PlayRangeAttack( float fps = 0 )
	{
		//play random "range" animation
		float flRand = RANDOM_FLOAT(0.0, 1.0);
		if( flRand < 0.5 && SetAnimation( ACT_VM_RANGE_ATTACK1, fps ) == -1) return 0;
		if( flRand > 0.8 && SetAnimation( ACT_VM_RANGE_ATTACK2, fps ) == -1)
		{
			if(SetAnimation( ACT_VM_RANGE_ATTACK1, fps ) == -1) return 0;
		}
		if( flRand < 0.8 && flRand > 0.5 && SetAnimation( ACT_VM_RANGE_ATTACK3, fps ) == -1)
		{
			if(SetAnimation( ACT_VM_RANGE_ATTACK1, fps ) == -1) return 0;
		}
		return 1;
	}

	inline int PlayMeleeAttack( float fps = 0 )
	{
		//play random "range" animation
		float flRand = RANDOM_FLOAT(0.0, 1.0);
		if( flRand < 0.5 && SetAnimation( ACT_VM_MELEE_ATTACK1, fps ) == -1) return 0;
		if( flRand > 0.8 && SetAnimation( ACT_VM_MELEE_ATTACK2, fps ) == -1)
		{
			if(SetAnimation( ACT_VM_MELEE_ATTACK1, fps ) == -1) return 0;
		}
		if( flRand < 0.8 && flRand > 0.5 && SetAnimation( ACT_VM_MELEE_ATTACK3, fps ) == -1)
		{
			if(SetAnimation( ACT_VM_MELEE_ATTACK1, fps ) == -1) return 0;
		}
		return 1;
	}

	inline int PlayEmptyFire( float fps = 0 )
	{
		if(iMaxClip() && !m_iClip)//last round
		{
			return SetAnimation( ACT_VM_SHOOT_EMPTY, fps ) == -1 ? FALSE : TRUE;
		}
		return 0;
	}

	//GENERIC WEAPON FUNCTIONS
	virtual void PrimaryPostAttack( void ) {}
	virtual void SecondaryPostAttack( void ) {}
	virtual void PostReload( void ) {}
	inline int IsEmptyReload( void ) { return m_iStepReload == EMPTY_RELOAD ? TRUE : FALSE; }
	
	virtual void PrimaryAttack( void )   // do "+ATTACK"
	{
		int iResult = PlayCurrentAttack( iAttack1(), FBitSet(pev->impulse, PATTACK_FIREMODE1) ? 1 : 0 );
		if(iResult == 1)
		{
			m_pPlayer->pev->punchangle.x = fPunchAngle1();
			m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - gpGlobals->v_forward * fRecoil1();
			if ( (iFlags() & ITEM_FLAG_HIDEAMMO ) && m_iClip < GetNumBodies() && m_iClip ) m_iBody++;
                             
			PrimaryPostAttack(); //run post effects
			if ( m_flNextPrimaryAttack < UTIL_WeaponTimeBase() )
				m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + fNextAttack1() + 0.02;
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + fNextAttack1();
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_LONG(10, 15);
		}
		else if(iResult == 0)
		{
			PlayEmptySound( );
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.5;
		}
		m_iStepReload = 0;//reset reload
	}
	virtual void SecondaryAttack( void ) // do "+ATTACK2"
	{
		int iResult = PlayCurrentAttack( iAttack2(), FBitSet(pev->impulse, SATTACK_FIREMODE1) ? 1 : 0 );
		if(iResult == 1)
		{
			m_pPlayer->pev->punchangle.x = fPunchAngle2();
			m_pPlayer->pev->velocity = m_pPlayer->pev->velocity - gpGlobals->v_forward * fRecoil2();
			
			SecondaryPostAttack(); //run post effects
			if ( m_flNextSecondaryAttack < UTIL_WeaponTimeBase() )
				m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + fNextAttack2() + 0.02;
			m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + fNextAttack2();
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + RANDOM_LONG(10, 15);
		}
		else if(iResult == 0)
		{
			PlayEmptySound( );
			m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.5;
		}
		m_iStepReload = 0;//reset reload
	}			
	
	virtual void Reload( void ){ DefaultReload( ACT_VM_RELOAD ); } 	// do "+RELOAD"
	virtual void PostIdle( void ) {}				// calling every frame
	virtual void WeaponIdle( void )				// called when no buttons pressed
	{
		if( !stricmp( pszAmmo1(), "grenade" ))
		{
			if(m_flHoldTime) Launch( pszAmmo1());
			else DefaultIdle();
		}
		else DefaultIdle();
	}
	virtual void Deploy( void );					// deploy function
	virtual void Holster( void );					// holster function
          
	//weapon saved variables
	float	m_flNextPrimaryAttack;	// soonest time ItemPostFrame will call PrimaryAttack
	float	m_flNextSecondaryAttack;	// soonest time ItemPostFrame will call SecondaryAttack
	float	m_flTimeWeaponIdle;		// soonest time ItemPostFrame will call WeaponIdle
	float	m_flTimeUpdate;		// special time for additional effects
	int	m_iPrimaryAmmoType;		// "primary" ammo index into players m_rgAmmo[]
	int	m_iSecondaryAmmoType;	// "secondary" ammo index into players m_rgAmmo[]
	int	m_iDefaultAmmo;		// how much primary ammo you get
	int	m_iDefaultAmmo2;		// how much secondary ammo you get
	int	m_cActiveRocket; 		// how many rockets is now active ?
	int	m_iOnControl;		// controllable rocket is on control now
	int 	m_iStepReload;		// reload state
	int	m_iSequence;		// current weaponmodel sequence
	int	m_iClip;			// current clip state
	int	m_iBody;			// viewmodel body
	int	m_iSkin;			// viewmodel skin
	int	m_iSpot;			// enable laser dot
          int	m_iZoom;			// zoom current level
	
	//weapon nonsaved variables
          float	m_flHoldTime;		// button holdtime
	int	m_iClientClip;	 	// the last version of m_iClip sent to hud dll
	int	m_iClientFov;		// g-cont. just to right update crosshairs
	int	m_iClientWeaponState;	// the last version of the weapon state sent to hud dll (is current weapon, is on target)
	int	m_iPlayEmptySound;		// trigger for empty sound
	int	m_fInReload;	 	// Are we in the middle of a reload;
	int	m_iClientSkin;		// synch server and client skin
	int	m_iClientBody;		// synch server and client body
	int	b_restored;		// restore body and skin
};

#endif // WEAPONS_H
