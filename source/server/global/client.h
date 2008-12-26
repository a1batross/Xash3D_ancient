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

//messages affect only player
extern int gmsgShake;
extern int gmsgFade;
extern int gmsgSelAmmo;
extern int gmsgFlashlight;
extern int gmsgFlashBattery;
extern int gmsgResetHUD;
extern int gmsgInitHUD;
extern int gmsgHUDColor;
extern int gmsgCurWeapon;
extern int gmsgHealth;
extern int gmsgDamage;
extern int gmsgBattery;
extern int gmsgTrain;
extern int gmsgWeaponList;
extern int gmsgAmmoX;
extern int gmsgDeathMsg;
extern int gmsgScoreInfo;
extern int gmsgTeamInfo;
extern int gmsgTeamScore;
extern int gmsgGameMode;
extern int gmsgMOTD;
extern int gmsgServerName;
extern int gmsgAmmoPickup;
extern int gmsgWeapPickup;
extern int gmsgItemPickup;
extern int gmsgHideWeapon;
extern int gmsgSetCurWeap;
extern int gmsgSayText;
extern int gmsgSetFOV;
extern int gmsgShowMenu;
extern int gmsgGeigerRange;
extern int gmsgTeamNames;
extern int gmsgTextMsg;
extern int gmsgStatusText;
extern int gmsgStatusValue;
extern int gmsgSetBody;
extern int gmsgSetSkin;
extern int gmsgZoomHUD;
extern int gmsgWarHUD;

//entity messages
extern int gmsgSetFog;
extern int gmsgStatusIcon;
extern int gmsgSetSky;
extern int gmsgParticle;
extern int gmsgFsound;
extern int gmsgCamData;
extern int gmsgRainData;
extern int gmsgHudText;
extern int gmsgShowGameTitle;
extern int gmsgAddScreen;
extern int gmsgAddMirror;
extern int gmsgAddPortal;
extern int gmsgBeams;

extern BOOL MSGSended;

#endif // CLIENT_H