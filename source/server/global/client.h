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
extern void ParmsChangeLevel( void );
extern void InitBodyQue(void);
extern void InitWorld( void );

extern const char *GetGameDescription( void );
extern void SpectatorConnect ( edict_t *pEntity );
extern void SpectatorDisconnect ( edict_t *pEntity );
extern void SpectatorThink ( edict_t *pEntity );

extern void SetupVisibility( edict_t *pViewEntity, edict_t *pClient, unsigned char **pvs, unsigned char **pas );
extern void UpdateClientData ( const struct edict_s *ent, int sendweapons, struct clientdata_s *cd );
extern int AddToFullPack( struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int hostflags, int player, unsigned char *pSet );
extern void CreateBaseline( int player, int eindex, struct entity_state_s *baseline, struct edict_s *entity, int playermodelindex, vec3_t player_mins, vec3_t player_maxs );
extern void RegisterEncoders( void );

extern int GetWeaponData( struct edict_s *player, struct weapon_data_s *info );

extern void CmdStart( const edict_t *player, const struct usercmd_s *cmd, unsigned int random_seed );
extern void CmdEnd ( const edict_t *player );

extern int  ConnectionlessPacket( const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size );
extern int GetHullBounds( int hullnumber, float *mins, float *maxs );
extern void CreateInstancedBaselines ( void );
extern int  InconsistentFile( const edict_t *player, const char *filename, char *disconnect_message );
extern int AllowLagCompensation( void );
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
	int	RoomType;
	int	SayText;
	int	SetFOV;
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
	int	AddScreen;
	int	AddMirror;
	int	AddPortal;
	int	Beams;
} user_messages_t;

extern user_messages_t gmsg;
extern BOOL MSGSended;

#endif // CLIENT_H