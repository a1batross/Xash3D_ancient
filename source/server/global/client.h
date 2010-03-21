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
#ifndef CLIENT_H
#define CLIENT_H

extern void PhysicsPreFrame( void );
extern void PhysicsFrame( void );
extern void PhysicsPostFrame( void );

extern void respawn( entvars_t* pev, BOOL fCopyCorpse );
extern BOOL ClientConnect( edict_t *pEntity, const char *userinfo );
extern void ClientDisconnect( edict_t *pEntity );
extern void ClientKill( edict_t *pEntity );
extern void ClientPutInServer( edict_t *pEntity );
extern void ClientCommand( edict_t *pEntity );
extern void ClientUserInfoChanged( edict_t *pEntity, char *infobuffer );
extern void ServerActivate( edict_t *pEdictList, int edictCount, int clientMax );
extern void ServerDeactivate( void );
extern void StartFrame( void );
extern void EndFrame( void );
extern void PlayerPostThink( edict_t *pEntity );
extern void PlayerPreThink( edict_t *pEntity );
extern void BuildLevelList( void );
extern void InitBodyQue(void);
extern void InitWorld( void );

extern const char *GetGameDescription( void );
extern void SpectatorConnect ( edict_t *pEntity );
extern void SpectatorDisconnect ( edict_t *pEntity );
extern void SpectatorThink ( edict_t *pEntity );

extern int SetupVisibility( edict_t *pViewEntity, edict_t *pClient, int portal, float *rgflViewOrg );
extern int AddToFullPack( edict_t *pView, edict_t *pHost, edict_t *pEdict, int hostflags, int hostarea, byte *pSet );

extern void CmdStart( const edict_t *player, const usercmd_t *cmd, unsigned int random_seed );
extern void CmdEnd ( const edict_t *player );

extern int g_serveractive;

// messages affect only player
typedef struct user_messages_s
{
	int	Shake;
	int	Fade;
	int	SelAmmo;
	int	Intermission;
	int	Flashlight;
	int	FlashBattery;
	int	ResetHUD;
	int	InitHUD;
	int	HUDColor;
	int	CurWeapon;
	int	Health;
	int	Damage;
	int	Battery;
	int	Train;
	int	WeaponList;
	int	AmmoX;
	int	DeathMsg;
	int	ScoreInfo;
	int	TeamInfo;
	int	TeamScore;
	int	GameMode;
	int	MOTD;
	int	ServerName;
	int	AmmoPickup;
	int	WeapPickup;
	int	ItemPickup;
	int	HideWeapon;
	int	WeaponAnim;
	int	RoomType;
	int	SayText;
	int	ShowMenu;
	int	GeigerRange;
	int	TeamNames;
	int	TextMsg;
	int	StatusText;
	int	StatusValue;
	int	SetBody;
	int	SetSkin;
	int	ZoomHUD;
	int	WarHUD;

	// entity messages
	int	TempEntity;	// completely moved from engine to user dlls
	int	SetFog;
	int	StatusIcon;
	int	SetSky;
	int	Particle;
	int	Fsound;
	int	CamData;
	int	RainData;
	int	HudText;
	int	ShowGameTitle;
} user_messages_t;

extern user_messages_t gmsg;
extern BOOL MSGSended;

#endif // CLIENT_H