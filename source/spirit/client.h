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
extern void ParmsChangeLevel( void );

extern void ClientPrecache( void );

extern const char *GetGameDescription( void );

extern void SpectatorConnect ( edict_t *pEntity );
extern void SpectatorDisconnect ( edict_t *pEntity );
extern void SpectatorThink ( edict_t *pEntity );

extern void Sys_Error( const char *error_string );

extern int SetupVisibility( edict_t *pViewEntity, edict_t *pClient, int portal, float *rgflViewOrg );
extern int AddToFullPack( edict_t *pHost, edict_t *pClient, edict_t *pEdict, int hostflags, int hostarea, byte *pSet );

extern void	CmdStart( const edict_t *player, const struct usercmd_s *cmd, unsigned int random_seed );
extern void	CmdEnd ( const edict_t *player );

extern int	g_serveractive;

#endif		// CLIENT_H
