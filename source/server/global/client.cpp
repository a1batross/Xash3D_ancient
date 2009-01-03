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
// Robin, 4-22-98: Moved set_suicide_frame() here from player.cpp to allow us to
//				   have one without a hardcoded player.mdl in tf_client.cpp

/*

===== client.cpp ========================================================

  client/server game specific stuff

*/

#include <ctype.h>

#include "extdll.h"
#include "utils.h"
#include "cbase.h"
#include "saverestore.h"
#include "shake.h"
#include "player.h"
#include "spectator.h"
#include "client.h"
#include "gamerules.h"
#include "game.h"
#include "soundent.h"
#include "baseweapon.h"
#include "defaults.h"
#include "decals.h"
#include "nodes.h"

extern DLL_GLOBAL ULONG	g_ulModelIndexPlayer;
extern DLL_GLOBAL BOOL	g_fGameOver;
extern DLL_GLOBAL ULONG	g_ulFrameCount;
extern CBaseEntity		*g_pLastSpawn;
extern CSoundEnt 		*pSoundEnt;
DLL_GLOBAL edict_t		*g_pBodyQueueHead;
DLL_GLOBAL int g_serveractive = 0;
BOOL MSGSended;
BOOL NewLevel = FALSE;
float MsgDelay;
user_messages_t gmsg;
void LinkUserMessages( void );
char text[128];
extern int g_teamplay;

//messages affect only player
typedef struct _SelAmmo
{
	BYTE	Ammo1Type;
	BYTE	Ammo1;
	BYTE	Ammo2Type;
	BYTE	Ammo2;
} SelAmmo;

/*
 * used by kill command and disconnect command
 * ROBIN: Moved here from player.cpp, to allow multiple player models
 */
void set_suicide_frame(entvars_t* pev)
{
	if (!FStrEq(STRING(pev->model), "models/player.mdl"))
		return; // allready gibbed

//	pev->frame		= $deatha11;
	pev->solid		= SOLID_NOT;
	pev->movetype	= MOVETYPE_TOSS;
	pev->deadflag	= DEAD_DEAD;
	pev->nextthink	= -1;
}

void InitBodyQue(void)
{
	string_t	istrClassname = MAKE_STRING("bodyque");

	g_pBodyQueueHead = CREATE_NAMED_ENTITY( istrClassname );
	entvars_t *pev = VARS(g_pBodyQueueHead);
	
	// Reserve 3 more slots for dead bodies
	for ( int i = 0; i < 3; i++ )
	{
		pev->owner = CREATE_NAMED_ENTITY( istrClassname );
		pev = VARS(pev->owner);
	}
	
	pev->owner = g_pBodyQueueHead;
}

//
// make a body que entry for the given ent so the ent can be respawned elsewhere
//
// GLOBALS ASSUMED SET:  g_eoBodyQueueHead
//
void CopyToBodyQue(entvars_t *pev) 
{
	if (pev->effects & EF_NODRAW)
		return;

	entvars_t *pevHead	= VARS(g_pBodyQueueHead);

	pevHead->angles	= pev->angles;
	pevHead->model	= pev->model;
	pevHead->modelindex	= pev->modelindex;
	pevHead->frame	= pev->frame;
	pevHead->colormap	= pev->colormap;
	pevHead->movetype	= MOVETYPE_TOSS;
	pevHead->velocity	= pev->velocity;
	pevHead->flags	= 0;
	pevHead->deadflag	= pev->deadflag;
	pevHead->renderfx	= kRenderFxDeadPlayer;
	pevHead->renderamt	= ENTINDEX( ENT( pev ) );

	pevHead->effects    = pev->effects | EF_NOINTERP;
	//pevHead->goalstarttime = pev->goalstarttime;
	//pevHead->goalframe	= pev->goalframe;
	//pevHead->goalendtime = pev->goalendtime ;
	
	pevHead->sequence = pev->sequence;
	pevHead->animtime = pev->animtime;

	UTIL_SetEdictOrigin(g_pBodyQueueHead, pev->origin);
	UTIL_SetSize(pevHead, pev->mins, pev->maxs);
	g_pBodyQueueHead = pevHead->owner;
}


/*
===========
ClientConnect

called when a player connects to a server
============
*/
BOOL ClientConnect( edict_t *pEntity, const char *userinfo  )
{
	return g_pGameRules->ClientConnected( pEntity, userinfo );

// a client connecting during an intermission can cause problems
//	if (intermission_running)
//		ExitIntermission ();

}


/*
===========
ClientDisconnect

called when a player disconnects from a server

GLOBALS ASSUMED SET:  g_fGameOver
============
*/
void ClientDisconnect( edict_t *pEntity )
{
	if (g_fGameOver)
		return;

	char text[256];
	sprintf( text, "- %s has left the game\n", STRING(pEntity->v.netname) );
	MESSAGE_BEGIN( MSG_ALL, gmsg.SayText, NULL );
		WRITE_BYTE( ENTINDEX(pEntity) );
		WRITE_STRING( text );
	MESSAGE_END();

	CSound *pSound;
	pSound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( pEntity ) );
	{
		// since this client isn't around to think anymore, reset their sound.
		if ( pSound )
		{
			pSound->Reset();
		}
	}

// since the edict doesn't get deleted, fix it so it doesn't interfere.
	pEntity->v.takedamage = DAMAGE_NO;// don't attract autoaim
	pEntity->v.solid = SOLID_NOT;// nonsolid
	UTIL_SetEdictOrigin ( pEntity, pEntity->v.origin );

	g_pGameRules->ClientDisconnected( pEntity );
}


// called by ClientKill and DeadThink
void respawn(entvars_t* pev, BOOL fCopyCorpse)
{
	if (gpGlobals->coop || gpGlobals->deathmatch)
	{
		if ( fCopyCorpse )
		{
			// make a copy of the dead body for appearances sake
			CopyToBodyQue(pev);
		}

		// respawn player
		GetClassPtr( (CBasePlayer *)pev)->Spawn( );
	}
	else
	{       // restart the entire server
		SERVER_COMMAND("reload\n");
	}
}

/*
============
ClientKill

Player entered the suicide command

GLOBALS ASSUMED SET:  g_ulModelIndexPlayer
============
*/
void ClientKill( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;

	CBasePlayer *pl = (CBasePlayer*) CBasePlayer::Instance( pev );

	if ( pl->m_fNextSuicideTime > gpGlobals->time )
		return;  // prevent suiciding too ofter

	pl->m_fNextSuicideTime = gpGlobals->time + 1;  // don't let them suicide for 5 seconds after suiciding

	// have the player kill themself
	pev->health = 0;
	pl->Killed( pev, GIB_NEVER );

//	pev->modelindex = g_ulModelIndexPlayer;
//	pev->frags -= 2;		// extra penalty
//	respawn( pev );
}

/*
===========
ClientPutInServer

called each time a player is spawned
============
*/
void ClientPutInServer( edict_t *pEntity )
{
	CBasePlayer *pPlayer;

	entvars_t *pev = &pEntity->v;

	pPlayer = GetClassPtr((CBasePlayer *)pev);
	pPlayer->SetCustomDecalFrames(-1); // Assume none;

	// Allocate a CBasePlayer for pev, and call spawn
	pPlayer->Spawn() ;

	// Reset interpolation during first frame
	pPlayer->pev->effects |= EF_NOINTERP;
}

//// HOST_SAY
// String comes in as
// say blah blah blah
// or as
// blah blah blah
//
void Host_Say( edict_t *pEntity, int teamonly )
{
	CBasePlayer *client;
	int		j;
	char	*p;
	char	text[128];
	char    szTemp[256];
	const char *cpSay = "say";
	const char *cpSayTeam = "say_team";
	const char *pcmd = CMD_ARGV(0);

	// We can get a raw string now, without the "say " prepended
	if ( CMD_ARGC() == 0 )
		return;

	entvars_t *pev = &pEntity->v;
	CBasePlayer* player = GetClassPtr((CBasePlayer *)pev);

	//Not yet.
	if ( player->m_flNextChatTime > gpGlobals->time )
		 return;

	if ( !stricmp( pcmd, cpSay) || !stricmp( pcmd, cpSayTeam ) )
	{
		if ( CMD_ARGC() >= 2 )
		{
			p = (char *)CMD_ARGS();
		}
		else
		{
			// say with a blank message, nothing to do
			return;
		}
	}
	else  // Raw text, need to prepend argv[0]
	{
		if ( CMD_ARGC() >= 2 )
		{
			sprintf( szTemp, "%s %s", ( char * )pcmd, (char *)CMD_ARGS() );
		}
		else
		{
			// Just a one word command, use the first word...sigh
			sprintf( szTemp, "%s", ( char * )pcmd );
		}
		p = szTemp;
	}

// remove quotes if present
	if (*p == '"')
	{
		p++;
		p[strlen(p)-1] = 0;
	}

// make sure the text has content
	char *pc;
	for ( pc = p; pc != NULL && *pc != 0; pc++ )
	{
		if ( isprint( *pc ) && !isspace( *pc ) )
		{
			pc = NULL;	// we've found an alphanumeric character,  so text is valid
			break;
		}
	}
	if ( pc != NULL )
		return;  // no character found, so say nothing

// turn on color set 2  (color on,  no sound)
	if ( teamonly )
		sprintf( text, "%c(TEAM) %s: ", 2, STRING( pEntity->v.netname ) );
	else
		sprintf( text, "%c%s: ", 2, STRING( pEntity->v.netname ) );

	j = sizeof(text) - 2 - strlen(text);  // -2 for /n and null terminator
	if ( (int)strlen(p) > j )
		p[j] = 0;

	strcat( text, p );
	strcat( text, "\n" );


	player->m_flNextChatTime = gpGlobals->time + CHAT_INTERVAL;

	// loop through all players
	// Start with the first player.
	// This may return the world in single player if the client types something between levels or during spawn
	// so check it, or it will infinite loop

	client = NULL;
	while ( ((client = (CBasePlayer*)UTIL_FindEntityByClassname( client, "player" )) != NULL) && (!FNullEnt(client->edict())) )
	{
		if ( !client->pev )
			continue;

		if ( client->edict() == pEntity )
			continue;

		if ( !(client->IsNetClient()) )	// Not a client ? (should never be true)
			continue;

		if ( teamonly && g_pGameRules->PlayerRelationship(client, CBaseEntity::Instance(pEntity)) != GR_TEAMMATE )
			continue;

		MESSAGE_BEGIN( MSG_ONE, gmsg.SayText, NULL, client->pev );
			WRITE_BYTE( ENTINDEX(pEntity) );
			WRITE_STRING( text );
		MESSAGE_END();

	}

	// print to the sending client
	MESSAGE_BEGIN( MSG_ONE, gmsg.SayText, NULL, &pEntity->v );
		WRITE_BYTE( ENTINDEX(pEntity) );
		WRITE_STRING( text );
	MESSAGE_END();

	// echo to server console
	g_engfuncs.pfnServerPrint( text );

	char * temp;
	if ( teamonly )
		temp = "say_team";
	else
		temp = "say";
}


/*
===========
ClientCommand
called each time a player uses a "cmd" command
============
*/
extern float g_flWeaponCheat;

// Use CMD_ARGV,  CMD_ARGV, and CMD_ARGC to get pointers the character string command.
void ClientCommand( edict_t *pEntity )
{
	const char *pcmd = CMD_ARGV(0);
	const char *pstr;

	// Is the client spawned yet?
	if( !pEntity->pvPrivateData )
		return;

	entvars_t *pev = &pEntity->v;

	if( FStrEq( pcmd, "noclip" ))
	{
		if( pEntity->v.movetype == MOVETYPE_WALK )
		{
			pEntity->v.movetype = MOVETYPE_NOCLIP;
			Msg( "noclip on\n" );
		}
		else
		{
			pEntity->v.movetype =  MOVETYPE_WALK;
			Msg( "noclip off\n" );
		}
	}
	else if ( FStrEq( pcmd, "hud_color") ) //LRC
	{
		if (CMD_ARGC() == 4)
		{
			int col = (atoi(CMD_ARGV(1)) & 255) << 16;
			col += (atoi(CMD_ARGV(2)) & 255) << 8;
			col += (atoi(CMD_ARGV(3)) & 255);
			MESSAGE_BEGIN( MSG_ONE, gmsg.HUDColor, NULL, &pEntity->v );
				WRITE_LONG(col);
			MESSAGE_END();
		}
		else
		{
			ALERT(at_console, "Syntax: hud_color RRR GGG BBB\n");
		}
	}
	else if ( FStrEq(pcmd, "fire") ) //LRC - trigger entities manually
	{
		if (g_flWeaponCheat)
		{
			CBaseEntity *pPlayer = CBaseEntity::Instance(pEntity);
			if (CMD_ARGC() > 1)
			{
				UTIL_FireTargets(CMD_ARGV(1), pPlayer, pPlayer, USE_TOGGLE);
			}
			else
			{
				TraceResult tr;
				UTIL_MakeVectors(pev->v_angle);
				UTIL_TraceLine(
					pev->origin + pev->view_ofs,
					pev->origin + pev->view_ofs + gpGlobals->v_forward * 1000,
					dont_ignore_monsters, pEntity, &tr
				);

				if (tr.pHit)
				{
					CBaseEntity *pHitEnt = CBaseEntity::Instance(tr.pHit);
					if (pHitEnt)
					{
						pHitEnt->Use(pPlayer, pPlayer, USE_TOGGLE, 0);
						ALERT(at_aiconsole, "Fired %s \"%s\"\n", STRING(pHitEnt->pev->classname), STRING(pHitEnt->pev->targetname));
					}
				}
			}
		}
	}
	else if ( FStrEq(pcmd, "debug") )
	{
		Msg("debug called\n" );
		if (g_flWeaponCheat)
		{
			CBaseEntity *pPlayer = CBaseEntity::Instance(pEntity);
			if (CMD_ARGC() > 1)
			{
				UTIL_FireTargets(CMD_ARGV(1), pPlayer, pPlayer, USE_SHOWINFO);
			}
			else
			{
				TraceResult tr;
				UTIL_MakeVectors(pev->v_angle);
				UTIL_TraceLine(
					pev->origin + pev->view_ofs,
					pev->origin + pev->view_ofs + gpGlobals->v_forward * 1000,
					dont_ignore_monsters, pEntity, &tr
				);

				if (tr.pHit)
				{
					CBaseEntity *pHitEnt = CBaseEntity::Instance(tr.pHit);
					if (pHitEnt)
					{
						pHitEnt->Use(pPlayer, pPlayer, USE_SHOWINFO, 0);
						ALERT(at_aiconsole, "Fired %s \"%s\"\n", STRING(pHitEnt->pev->classname), STRING(pHitEnt->pev->targetname));
					}
				}
			}
		}
	}
	else if ( FStrEq(pcmd, "say" ) )
	{
		Host_Say( pEntity, 0 );
	}
	else if ( FStrEq(pcmd, "game_over" ) )
	{
		if(IsMultiplayer())  g_pGameRules->EndMultiplayerGame();	//loading next map
		// FIXME: return to main menu here
	}
	else if( FStrEq( pcmd, "gametitle" ))
	{
		MESSAGE_BEGIN( MSG_ONE, gmsg.ShowGameTitle, NULL, ENT( pev ));
			WRITE_BYTE( 0 );
		MESSAGE_END();
	}
	else if( FStrEq( pcmd, "intermission" ))
	{
		MESSAGE_BEGIN( MSG_ONE, gmsg.Intermission, NULL, ENT( pev ));
		MESSAGE_END();
	}
	else if( FStrEq( pcmd, "fullupdate" ))
	{
		GetClassPtr((CBasePlayer *)pev)->ForceClientDllUpdate();
	}
	else if ( FStrEq(pcmd, "give" ) )
	{
		if ( g_flWeaponCheat != 0.0)
		{
			int iszItem = ALLOC_STRING( CMD_ARGV(1) );	// Make a copy of the classname
			GetClassPtr((CBasePlayer *)pev)->GiveNamedItem( STRING(iszItem) );
		}
	}

	else if ( FStrEq(pcmd, "drop" ) )
	{
		// player is dropping an item.
		GetClassPtr((CBasePlayer *)pev)->DropPlayerItem((char *)CMD_ARGV(1));
	}
	else if ( FStrEq(pcmd, "fov" ) )
	{
		if( g_flWeaponCheat && CMD_ARGC() > 1)
		{
			GetClassPtr((CBasePlayer *)pev)->m_iFOV = atoi( CMD_ARGV(1) );
		}
		else
		{
			ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, UTIL_VarArgs( "\"fov\" is \"%d\"\n", (int)GetClassPtr((CBasePlayer *)pev)->m_iFOV ) );
		}
	}
	else if ( FStrEq(pcmd, "use" ) )
	{
		GetClassPtr((CBasePlayer *)pev)->SelectItem((char *)CMD_ARGV(1));
	}
          else if (((pstr = strstr(pcmd, "weapon_")) != NULL)  && (pstr == pcmd))
	{
		GetClassPtr((CBasePlayer *)pev)->SelectItem(pcmd);
	}
	else if( FStrEq( pcmd, "lastinv" ))
	{
		GetClassPtr((CBasePlayer *)pev)->SelectLastItem();
	}
	else if( FStrEq( pcmd, "spectate" ))	// added for proxy support
	{
		CBasePlayer * pPlayer = GetClassPtr((CBasePlayer *)pev);

		edict_t *pentSpawnSpot = g_pGameRules->GetPlayerSpawnSpot( pPlayer );
		pPlayer->StartObserver( pev->origin, VARS(pentSpawnSpot)->angles);
	}
	else if ( g_pGameRules->ClientCommand( GetClassPtr((CBasePlayer *)pev), pcmd ) )
	{
		// MenuSelect returns true only if the command is properly handled,  so don't print a warning
	}
	else
	{
		// tell the user they entered an unknown command
		char command[128];

		// check the length of the command (prevents crash)
		// max total length is 192 ...and we're adding a string below ("Unknown command: %s\n")
		strncpy( command, pcmd, 127 );
		command[127] = '\0';

		// tell the user they entered an unknown command
		ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, UTIL_VarArgs( "Unknown command: %s\n", command ) );
	}
}


/*
========================
ClientUserInfoChanged

called after the player changes
userinfo - gives dll a chance to modify it before
it gets sent into the rest of the engine.
========================
*/
void ClientUserInfoChanged( edict_t *pEntity, char *infobuffer )
{
	// Is the client spawned yet?
	if ( !pEntity->pvPrivateData )
		return;

	// msg everyone if someone changes their name,  and it isn't the first time (changing no name to current name)
	if ( pEntity->v.netname && STRING(pEntity->v.netname)[0] != 0 && !FStrEq( STRING(pEntity->v.netname), g_engfuncs.pfnInfoKeyValue( infobuffer, "name" )) )
	{
		char sName[256];
		char *pName = g_engfuncs.pfnInfoKeyValue( infobuffer, "name" );
		strncpy( sName, pName, sizeof(sName) - 1 );
		sName[ sizeof(sName) - 1 ] = '\0';

		// First parse the name and remove any %'s
		for ( char *pApersand = sName; pApersand != NULL && *pApersand != 0; pApersand++ )
		{
			// Replace it with a space
			if ( *pApersand == '%' )
				*pApersand = ' ';
		}

		// Set the name
		g_engfuncs.pfnSetClientKeyValue( ENTINDEX(pEntity), infobuffer, "name", sName );

		char text[256];
		sprintf( text, "* %s changed name to %s\n", STRING(pEntity->v.netname), g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
		MESSAGE_BEGIN( MSG_ALL, gmsg.SayText, NULL );
			WRITE_BYTE( ENTINDEX(pEntity) );
			WRITE_STRING( text );
		MESSAGE_END();
	}
	g_pGameRules->ClientUserInfoChanged( GetClassPtr((CBasePlayer *)&pEntity->v), infobuffer );
}

void ServerDeactivate( void )
{
	// make sure they reinitialise the World in the next server
	g_pWorld = NULL;

	// It's possible that the engine will call this function more times than is necessary
	//  Therefore, only run it one time for each call to ServerActivate
	if ( g_serveractive != 1 )
	{
		return;
	}

	g_serveractive = 0;

	// Peform any shutdown operations here...
	//
}

void ServerActivate( edict_t *pEdictList, int edictCount, int clientMax )
{
	int				i;
	CBaseEntity		*pClass;

	// Every call to ServerActivate should be matched by a call to ServerDeactivate
	g_serveractive = 1;
	// Clients have not been initialized yet
	for ( i = 0; i < edictCount; i++ )
	{
		if ( pEdictList[i].free )
			continue;

		// Clients aren't necessarily initialized until ClientPutInServer()
		if ( i < clientMax || !pEdictList[i].pvPrivateData )
			continue;

		pClass = CBaseEntity::Instance( &pEdictList[i] );
		// Activate this entity if it's got a class & isn't dormant
		if ( pClass && !(pClass->pev->flags & FL_DORMANT) )
		{
			pClass->SetupPhysics();
			pClass->Activate();
		}
		else Msg( "Can't instance %s\n", STRING(pEdictList[i].v.classname) );
	}

	// Link user messages here to make sure first client can get them...
	NewLevel = FALSE;
	LinkUserMessages();
}

// a cached version of gpGlobals->frametime. The engine sets frametime to 0 if the player is frozen... so we just cache it in prethink,
// allowing it to be restored later and used by CheckDesiredList.
float cached_frametime = 0.0f;

/*
================
PlayerPreThink

Called every frame before physics are run
================
*/
void PlayerPreThink( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE(pEntity);

	if (pPlayer)pPlayer->PreThink( );
	cached_frametime = gpGlobals->frametime;
}

/*
================
PlayerPostThink

Called every frame after physics are run
================
*/
void PlayerPostThink( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBasePlayer *pPlayer = (CBasePlayer *)GET_PRIVATE(pEntity);

	if (pPlayer)pPlayer->PostThink( );

	// use the old frametime, even if the engine has reset it
	gpGlobals->frametime = cached_frametime;
}

void ParmsChangeLevel( void )
{
	// retrieve the pointer to the save data
	SAVERESTOREDATA *pSaveData = (SAVERESTOREDATA *)gpGlobals->pSaveData;

	if ( pSaveData )
		pSaveData->connectionCount = BuildChangeList( pSaveData->levelList, MAX_LEVEL_CONNECTIONS );
}

//=======================================================================
//		Build ChangeLevel List 
//=======================================================================
int AddTransitionToList( LEVELLIST *pLevelList, int listCount, const char *pMapName, const char *pLandmarkName, edict_t *pentLandmark )
{
	int i;

	if ( !pLevelList || !pMapName || !pLandmarkName || !pentLandmark ) return 0;

	for ( i = 0; i < listCount; i++ )
	{
		if ( pLevelList[i].pentLandmark == pentLandmark && strcmp( pLevelList[i].mapName, pMapName ) == 0 )
			return 0;
	}
	strcpy( pLevelList[listCount].mapName, pMapName );
	strcpy( pLevelList[listCount].landmarkName, pLandmarkName );
	pLevelList[listCount].pentLandmark = pentLandmark;
	pLevelList[listCount].vecLandmarkOrigin = VARS(pentLandmark)->origin;

	return 1;
}

int BuildChangeList( LEVELLIST *pLevelList, int maxList )
{
	edict_t *pentLandmark;
	int i, count;

	count = 0;

	// Find all of the possible level changes on this BSP
	CBaseEntity *pChangelevel = UTIL_FindEntityByClassname( NULL, "trigger_changelevel" );

	if ( !pChangelevel ) return NULL;

	while ( pChangelevel )
	{
		// Find the corresponding landmark
		pentLandmark = UTIL_FindLandmark( pChangelevel->pev->message );
		if ( pentLandmark )
		{
			// Build a list of unique transitions
			DevMsg("Map name %s, landmark name %s\n", STRING(pChangelevel->pev->netname), STRING(pChangelevel->pev->message));
			if ( AddTransitionToList( pLevelList, count, (char *)STRING(pChangelevel->pev->netname), (char *)STRING(pChangelevel->pev->message), pentLandmark ))
			{
				count++;
				if ( count >= maxList )break;//List is FULL!!!
			}
		}
		pChangelevel = UTIL_FindEntityByClassname( pChangelevel, "trigger_changelevel" );
	}

	if ( gpGlobals->pSaveData && ((SAVERESTOREDATA *)gpGlobals->pSaveData)->pTable )
	{
		CSave saveHelper( (SAVERESTOREDATA *)gpGlobals->pSaveData );

		for ( i = 0; i < count; i++ )
		{
			int j, entityCount = 0;
			CBaseEntity *pEntList[ MAX_TRANSITION_ENTITY ];
			int entityFlags[ MAX_TRANSITION_ENTITY ];

			// Follow the linked list of entities in the PVS of the transition landmark
			edict_t *pent = UTIL_EntitiesInPVS( pLevelList[i].pentLandmark );

			// Build a list of valid entities in this linked list (we're going to use pent->v.chain again)
			while ( !FNullEnt( pent ) )
			{
				CBaseEntity *pEntity = CBaseEntity::Instance(pent);
				if ( pEntity )
				{
					int caps = pEntity->ObjectCaps();
					if ( !(caps & FCAP_DONT_SAVE) )
					{
						int flags = 0;

						// If this entity can be moved or is global, mark it
						if ( caps & FCAP_ACROSS_TRANSITION ) flags |= FENTTABLE_MOVEABLE;
						if ( pEntity->pev->globalname && !pEntity->IsDormant() ) flags |= FENTTABLE_GLOBAL;
						if ( flags )
						{
							pEntList[ entityCount ] = pEntity;
							entityFlags[ entityCount ] = flags;
							entityCount++;
							if ( entityCount > MAX_TRANSITION_ENTITY ) Msg("ERROR: Too many entities across a transition!" );
						}
					}
				}
				pent = pent->v.chain;
			}

			for ( j = 0; j < entityCount; j++ )
			{
				// Check to make sure the entity isn't screened out by a trigger_transition
				if ( entityFlags[j] && UTIL_FindTransition( pEntList[j], pLevelList[i].landmarkName ) )
				{
					// Mark entity table with 1<<i
					int index = saveHelper.EntityIndex( pEntList[j] );
					// Flag it with the level number
					saveHelper.EntityFlagsSet( index, entityFlags[j] | (1<<i) );
				}
			}
		}
	}
	return count;
}

//=======================================================================
// send messages - update all messages, at spawned player 
//=======================================================================
void ServerPostActivate( void )
{
	edict_t		*pEdict = INDEXENT( 0 );
	CBasePlayer 	*plr = (CBasePlayer*)UTIL_PlayerByIndex( 1 );
	CBaseEntity	*pClass;

	if( !pEdict || MSGSended ) return; // player spawned ?
	// NOTE: Time to affect is obsolete delay before sending message
	// Tune multiplayer time if need

	if( MsgDelay > gpGlobals->time ) return;
	if( plr && !plr->m_fInitHUD && !gInitHUD )
	{
		for ( int i = 0; i < gpGlobals->maxEntities; i++, pEdict++ )
		{
			if( pEdict->free )	continue;
			pClass = CBaseEntity::Instance( pEdict );
			if( pClass && !( pClass->pev->flags & FL_DORMANT ))
			{
				pClass->PostActivate();
				pClass->ClassifyEdict();
			}
		}
		MSGSended = TRUE;//messages sucessfully sended
	}
}

//
// GLOBALS ASSUMED SET:  g_ulFrameCount
//
void StartFrame( void )
{
	//Msg(" frametime %g\n", gpGlobals->frametime );
	if ( g_pGameRules )g_pGameRules->Think();
	if ( g_fGameOver ) return;

	gpGlobals->teamplay = (CVAR_GET_FLOAT( "mp_teamplay" )  == 1.0f) ? TRUE : FALSE;
	g_ulFrameCount++;

	ServerPostActivate();//called once
	PhysicsFrame();
	PhysicsPostFrame();
	
}

void EndFrame( void )
{
}

void ClientPrecache( void )
{
	// Material System!!! move this in next versions
	PRECACHE_SOUND("player/pl_step1.wav");		// walk on concrete
	PRECACHE_SOUND("player/pl_step2.wav");
	PRECACHE_SOUND("player/pl_step3.wav");
	PRECACHE_SOUND("player/pl_step4.wav");

	PRECACHE_SOUND("common/npc_step1.wav");		// NPC walk on concrete
	PRECACHE_SOUND("common/npc_step2.wav");
	PRECACHE_SOUND("common/npc_step3.wav");
	PRECACHE_SOUND("common/npc_step4.wav");

	PRECACHE_SOUND("player/pl_metal1.wav");		// walk on metal
	PRECACHE_SOUND("player/pl_metal2.wav");
	PRECACHE_SOUND("player/pl_metal3.wav");
	PRECACHE_SOUND("player/pl_metal4.wav");

	PRECACHE_SOUND("player/pl_dirt1.wav");		// walk on dirt
	PRECACHE_SOUND("player/pl_dirt2.wav");
	PRECACHE_SOUND("player/pl_dirt3.wav");
	PRECACHE_SOUND("player/pl_dirt4.wav");

	PRECACHE_SOUND("player/pl_duct1.wav");		// walk in duct
	PRECACHE_SOUND("player/pl_duct2.wav");
	PRECACHE_SOUND("player/pl_duct3.wav");
	PRECACHE_SOUND("player/pl_duct4.wav");

	PRECACHE_SOUND("player/pl_grate1.wav");		// walk on grate
	PRECACHE_SOUND("player/pl_grate2.wav");
	PRECACHE_SOUND("player/pl_grate3.wav");
	PRECACHE_SOUND("player/pl_grate4.wav");

	PRECACHE_SOUND("player/pl_slosh1.wav");		// walk in shallow water
	PRECACHE_SOUND("player/pl_slosh2.wav");
	PRECACHE_SOUND("player/pl_slosh3.wav");
	PRECACHE_SOUND("player/pl_slosh4.wav");

	PRECACHE_SOUND("player/pl_tile1.wav");		// walk on tile
	PRECACHE_SOUND("player/pl_tile2.wav");
	PRECACHE_SOUND("player/pl_tile3.wav");
	PRECACHE_SOUND("player/pl_tile4.wav");
	PRECACHE_SOUND("player/pl_tile5.wav");

	PRECACHE_SOUND("player/pl_swim1.wav");		// breathe bubbles
	PRECACHE_SOUND("player/pl_swim2.wav");
	PRECACHE_SOUND("player/pl_swim3.wav");
	PRECACHE_SOUND("player/pl_swim4.wav");

	PRECACHE_SOUND("player/pl_ladder1.wav");	// climb ladder rung
	PRECACHE_SOUND("player/pl_ladder2.wav");
	PRECACHE_SOUND("player/pl_ladder3.wav");
	PRECACHE_SOUND("player/pl_ladder4.wav");

	PRECACHE_SOUND("player/pl_wade1.wav");		// wade in water
	PRECACHE_SOUND("player/pl_wade2.wav");
	PRECACHE_SOUND("player/pl_wade3.wav");
	PRECACHE_SOUND("player/pl_wade4.wav");

	PRECACHE_SOUND("debris/wood1.wav");		// hit wood texture
	PRECACHE_SOUND("debris/wood2.wav");
	PRECACHE_SOUND("debris/wood3.wav");

	PRECACHE_SOUND("buttons/spark5.wav");		// hit computer texture
	PRECACHE_SOUND("buttons/spark6.wav");
	PRECACHE_SOUND("debris/glass1.wav");
	PRECACHE_SOUND("debris/glass2.wav");
	PRECACHE_SOUND("debris/glass3.wav");
}

/*
===============
GetGameDescription

Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/
const char *GetGameDescription()
{
	char token[256];
          char szbuffer[128];
	
	char *pfile = (char *)LOAD_FILE( "liblist.gam", NULL );
	if(pfile)
	{
		while ( pfile )
		{
			if ( !stricmp( token, "game" )) 
			{                                          
				pfile = COM_ParseFile(pfile, token);
				sprintf( szbuffer, "%s ", token );
				strcat( text, szbuffer );
			}
			else if ( !stricmp( token, "version" )) 
			{                                          
				pfile = COM_ParseFile(pfile, token);
				strcat( text, token );
			}
			pfile = COM_ParseFile(pfile, token);
		}
		COM_FreeFile( pfile );
		return text;
	}
	return "Half-Life";
}

//=======================================================================
// Link User Messages - register new client messages here
//=======================================================================
void LinkUserMessages( void )
{
	// already taken care of?
	if( gmsg.SelAmmo ) return;

	memset( &gmsg, 0, sizeof( gmsg ));

	//messages affect only player
	gmsg.SelAmmo = REG_USER_MSG("SelAmmo", sizeof(SelAmmo));
	gmsg.Intermission = REG_USER_MSG( "Intermission", 0 );
	gmsg.CurWeapon = REG_USER_MSG("CurWeapon", 3);
	gmsg.GeigerRange = REG_USER_MSG("Geiger", 1);
	gmsg.Flashlight = REG_USER_MSG("Flashlight", 2);
	gmsg.FlashBattery = REG_USER_MSG("FlashBat", 1);
	gmsg.Health = REG_USER_MSG( "Health", 1 );
	gmsg.Damage = REG_USER_MSG( "Damage", 18 );
	gmsg.Battery = REG_USER_MSG( "Battery", 2);
	gmsg.Train = REG_USER_MSG( "Train", 1);
	gmsg.SayText = REG_USER_MSG( "SayText", -1 );
	gmsg.TextMsg = REG_USER_MSG( "TextMsg", -1 );
	gmsg.WeaponList = REG_USER_MSG("WeaponList", -1);
	gmsg.ResetHUD = REG_USER_MSG("ResetHUD", 1);
	gmsg.InitHUD = REG_USER_MSG("InitHUD", 0 );
	gmsg.HUDColor = REG_USER_MSG( "HUDColor", 4 );
	gmsg.DeathMsg = REG_USER_MSG( "DeathMsg", -1 );
	gmsg.ScoreInfo = REG_USER_MSG( "ScoreInfo", 9 );
	gmsg.TeamInfo = REG_USER_MSG( "TeamInfo", -1 );
	gmsg.TeamScore = REG_USER_MSG( "TeamScore", -1 );
	gmsg.GameMode = REG_USER_MSG( "GameMode", 1 );
	gmsg.MOTD = REG_USER_MSG( "MOTD", -1 );
	gmsg.ServerName = REG_USER_MSG( "ServerName", -1 );
	gmsg.AmmoPickup = REG_USER_MSG( "AmmoPickup", 2 );
	gmsg.WeapPickup = REG_USER_MSG( "WeapPickup", 1 );
	gmsg.ItemPickup = REG_USER_MSG( "ItemPickup", -1 );
	gmsg.RoomType = REG_USER_MSG( "RoomType", 2 );
	gmsg.HideWeapon = REG_USER_MSG( "HideWeapon", 1 );
	gmsg.WeaponAnim = REG_USER_MSG( "WeaponAnim", 2 );
	gmsg.SetFOV = REG_USER_MSG( "SetFOV", 1 );
	gmsg.ShowMenu = REG_USER_MSG( "ShowMenu", -1 );
	gmsg.Shake = REG_USER_MSG("ScreenShake", 13 );
	gmsg.Fade = REG_USER_MSG("ScreenFade", sizeof(ScreenFade));
	gmsg.AmmoX = REG_USER_MSG("AmmoX", 2);
	gmsg.TeamNames = REG_USER_MSG( "TeamNames", -1 );
	gmsg.StatusText = REG_USER_MSG("StatusText", -1);
	gmsg.StatusValue = REG_USER_MSG("StatusValue", 3);
	gmsg.SetBody = REG_USER_MSG("SetBody", 1);
	gmsg.SetSkin = REG_USER_MSG("SetSkin", 1);
	gmsg.ZoomHUD = REG_USER_MSG("ZoomHUD", 1);
	gmsg.WarHUD = REG_USER_MSG("WarHUD", 1);

	//entity messages
	gmsg.StatusIcon = REG_USER_MSG("StatusIcon", -1);
	gmsg.CamData = REG_USER_MSG("CamData", -1);
	gmsg.Fsound = REG_USER_MSG("Fsound", -1);
	gmsg.RainData = REG_USER_MSG("RainData", 16);
	gmsg.AddScreen = REG_USER_MSG( "AddScreen", 1);
	gmsg.AddPortal = REG_USER_MSG( "AddPortal", 1);
	gmsg.HudText = REG_USER_MSG( "HudText", -1 );
	gmsg.ShowGameTitle = REG_USER_MSG("GameTitle", 1);
	gmsg.SetFog = REG_USER_MSG("SetFog", 7 );
	gmsg.SetSky = REG_USER_MSG( "SetSky", 13 );
	gmsg.Particle = REG_USER_MSG( "Particle", -1);
	gmsg.Beams = REG_USER_MSG( "Beams", -1 );
	gmsg.AddMirror = REG_USER_MSG( "AddMirror", 2);
}

/*
================================
AllowLagCompensation

 The game .dll should return 1 if lag compensation should be allowed ( could also just set
  the sv_unlag cvar.
 Most games right now should return 0, until client-side weapon prediction code is written
  and tested for them ( note you can predict weapons, but not do lag compensation, too,
  if you want.
================================
*/
int AllowLagCompensation( void )
{
	return 1;
}


/*
================
InitWorld

Called ever time when worldspawn will precached
================
*/

void InitWorld( void )
{
	int i;
	
	CVAR_SET_STRING("sv_gravity", "800"); // 67ft/sec
	CVAR_SET_STRING("sv_stepsize", "18");
          CVAR_SET_STRING("room_type", "0");// clear DSP
          
	g_pLastSpawn = NULL;
          
	// Set up game rules
	if (g_pGameRules) delete g_pGameRules;
	g_pGameRules = InstallGameRules( );

	if ( WorldGraph.m_fGraphPresent && !WorldGraph.m_fGraphPointersSet )
	{
		if ( !WorldGraph.FSetGraphPointers()) Msg( "Graph pointers were not set!\n");
		else Msg( "**Graph Pointers Set!\n" );
	}

	UTIL_PrecacheResourse();	//precache all resource
      
	pSoundEnt = GetClassPtr( ( CSoundEnt *)NULL );
	pSoundEnt->Spawn();
	if ( !pSoundEnt ) Msg( "Couldn create soundent!\n" );

	InitBodyQue();

	SENTENCEG_Init();
	TEXTURETYPE_Init();
	
	MSGSended = FALSE;
	if(IsMultiplayer()) MsgDelay = 0.5 + gpGlobals->time;
	else		MsgDelay = 0.03 + gpGlobals->time;
	
	ClientPrecache();

	// Setup light animation tables. 'a' is total darkness, 'z' is maxbright.
	for (i = 0; i <= 13; i++) LIGHT_STYLE(i, (char*)STRING(GetStdLightStyle(i)));
	for (i = 0; i < DECAL_COUNT; i++ ) gDecals[i].index = DECAL_INDEX( gDecals[i].name );
	
	// init the WorldGraph.
	WorldGraph.InitGraph();

	if ( !WorldGraph.CheckNODFile ( ( char * )STRING( gpGlobals->mapname ))) WorldGraph.AllocNodes();
	else
	{
		if ( !WorldGraph.FLoadGraph ( (char *)STRING( gpGlobals->mapname )))WorldGraph.AllocNodes();
		else Msg("\n*Graph Loaded!\n" );
	}

	CVAR_SET_FLOAT( "sv_zmax", MAP_SIZE );
}