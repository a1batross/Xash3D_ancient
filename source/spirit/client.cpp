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
#include "util.h"
#include "cbase.h"
#include "saverestore.h"
#include "player.h"
#include "spectator.h"
#include "client.h"
#include "soundent.h"
#include "gamerules.h"
#include "game.h"
#include "weapons.h"
#include "usercmd.h"
#include "movewith.h"

extern DLL_GLOBAL ULONG		g_ulModelIndexPlayer;
extern DLL_GLOBAL BOOL		g_fGameOver;
extern DLL_GLOBAL int		g_iSkillLevel;
extern DLL_GLOBAL ULONG		g_ulFrameCount;

extern void CopyToBodyQue(entvars_t* pev);
extern int giPrecacheGrunt;
extern int gmsgSayText;
extern int gmsgHUDColor;
extern int gmsgCamData; // for trigger_viewset

extern int g_teamplay;
DLL_GLOBAL int g_serveractive = 0;
void LinkUserMessages( void );
char text[256];

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
	MESSAGE_BEGIN( MSG_ALL, gmsgSayText, NULL );
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
	if (gpGlobals->teamplay || gpGlobals->coop || gpGlobals->deathmatch)
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
	pPlayer->m_flViewHeight = pPlayer->pev->view_ofs.z; // keep viewheight an actual
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

		MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, client->pev );
			WRITE_BYTE( ENTINDEX(pEntity) );
			WRITE_STRING( text );
		MESSAGE_END();

	}

	// print to the sending client
	MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, &pEntity->v );
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

	// team match?
	if ( g_teamplay )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" %s \"%s\"\n",
			STRING( pEntity->v.netname ),
			g_engfuncs.pfnInfoKeyValue( g_engfuncs.pfnGetInfoKeyBuffer( pEntity ), "model" ),
			temp,
			p );
	}
	else
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%i>\" %s \"%s\"\n",
			STRING( pEntity->v.netname ),
			temp,
			p );
	}
}


/*
===========
ClientCommand
called each time a player uses a "cmd" command
============
*/
extern float g_flWeaponCheat;
extern int gmsgPlayMP3; //AJH - Killars MP3player

// Use CMD_ARGV,  CMD_ARGV, and CMD_ARGC to get pointers the character string command.
void ClientCommand( edict_t *pEntity )
{
	const char *pcmd = CMD_ARGV(0);
	const char *pstr;

	// Is the client spawned yet?
	if ( !pEntity->pvPrivateData )
		return;

	entvars_t *pev = &pEntity->v;

	if( FStrEq( pcmd, "noclip" ))
	{
		if( pEntity->v.movetype == MOVETYPE_WALK )
		{
			pEntity->v.movetype = MOVETYPE_NOCLIP;
			ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "noclip on\n" );
		}
		else
		{
			pEntity->v.movetype =  MOVETYPE_WALK;
			ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "noclip off\n" );
		}
	}
	else if( FStrEq( pcmd, "god" ))
	{
		pEntity->v.flags = pEntity->v.flags ^ FL_GODMODE;

		if ( !( pEntity->v.flags & FL_GODMODE ))
			ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "godmode OFF\n" );
		else ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "godmode ON\n" );
	}
	else if( FStrEq( pcmd, "fly" ))
	{
		if ( pEntity->v.movetype == MOVETYPE_FLY )
		{
			pEntity->v.movetype = MOVETYPE_WALK;
			ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "flymode OFF\n" );
		}
		else
		{
			pEntity->v.movetype = MOVETYPE_FLY;
			ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "flymode ON\n" );
		}
	}
	else if( FStrEq( pcmd, "notarget" ))
	{
		pEntity->v.flags = pEntity->v.flags ^ FL_NOTARGET;

		if ( !( pEntity->v.flags & FL_NOTARGET ))
			ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "notarget OFF\n" );
		else ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, "notarget ON\n" );
	}
	else if ( FStrEq(pcmd, "hud_color") ) //LRC
	{
		if (CMD_ARGC() == 4)
		{
			int col = (atoi(CMD_ARGV(1)) & 255) << 16;
			col += (atoi(CMD_ARGV(2)) & 255) << 8;
			col += (atoi(CMD_ARGV(3)) & 255);
			MESSAGE_BEGIN( MSG_ONE, gmsgHUDColor, NULL, &pEntity->v );
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
				FireTargets(CMD_ARGV(1), pPlayer, pPlayer, USE_TOGGLE, 0);
			}
			else
			{
				TraceResult tr;
				UTIL_MakeVectors(pev->viewangles);
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
						ClientPrint( &pEntity->v, HUD_PRINTCONSOLE, UTIL_VarArgs( "Fired %s \"%s\"\n", STRING(pHitEnt->pev->classname), STRING(pHitEnt->pev->targetname) ) );
					}
				}
			}
		}
	}
	else if ( FStrEq(pcmd, "say" ) )
	{
		Host_Say( pEntity, 0 );
	}
	else if ( FStrEq(pcmd, "say_team" ) )
	{
		Host_Say( pEntity, 1 );
	}
	else if ( FStrEq(pcmd, "game_over" ) )
	{
		if(IsMultiplayer())  g_pGameRules->EndMultiplayerGame();	//loading next map
		else HOST_ENDGAME( CMD_ARGV( 1 ) );
	}
	else if( FStrEq( pcmd, "gametitle" ))
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgShowGameTitle, NULL, ENT( pev ));
		MESSAGE_END();
	}
	else if( FStrEq( pcmd, "intermission" ))
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgIntermission, NULL, ENT( pev ));
		MESSAGE_END();
	}
	else if ( FStrEq(pcmd, "fullupdate" ) )
	{
		GetClassPtr((CBasePlayer *)pev)->ForceClientDllUpdate();
	}
	else if ( FStrEq(pcmd, "playaudio" ) )  //AJH - MP3/OGG player (based on killars MP3)
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgPlayMP3, NULL, ENT(pev) );
			WRITE_STRING( (char *)CMD_ARGV(1) );
		MESSAGE_END();
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
		if ( g_flWeaponCheat && CMD_ARGC() > 1)
		{
			GetClassPtr((CBasePlayer *)pev)->pev->fov = atof( CMD_ARGV(1) );
		}
		else
		{
			CLIENT_PRINTF( pEntity, print_console, UTIL_VarArgs( "\"fov\" is \"%d\"\n", (int)GetClassPtr((CBasePlayer *)pev)->pev->fov ) );
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
	else if (FStrEq(pcmd, "lastinv" ))
	{
		GetClassPtr((CBasePlayer *)pev)->SelectLastItem();
	}
	else if ( FStrEq( pcmd, "spectate" ) && (pev->flags & FL_PROXY) )	// added for proxy support
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
		MESSAGE_BEGIN( MSG_ALL, gmsgSayText, NULL );
			WRITE_BYTE( ENTINDEX(pEntity) );
			WRITE_STRING( text );
		MESSAGE_END();

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" changed name to \"%s\"\n",
				STRING( pEntity->v.netname ),
				g_engfuncs.pfnInfoKeyValue( infobuffer, "model" ),
				g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" changed name to \"%s\"\n",
				STRING( pEntity->v.netname ),
				g_engfuncs.pfnInfoKeyValue( infobuffer, "name" ) );
		}
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
			pClass->Activate();
		}
		else
		{
			ALERT( at_debug, "Can't instance %s\n", STRING(pEdictList[i].v.classname) );
		}
	}

	// Link user messages here to make sure first client can get them...
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

	if (pPlayer)
		pPlayer->PreThink( );

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

	if (pPlayer)
		pPlayer->PostThink( );

	// use the old frametime, even if the engine has reset it
	gpGlobals->frametime = cached_frametime;

	//LRC - moved to here from CBasePlayer::PostThink, so that
	// things don't stop when the player dies
	CheckDesiredList( );
}

void BuildLevelList( void )
{
	// retrieve the pointer to the save data
	SAVERESTOREDATA *pSaveData = (SAVERESTOREDATA *)gpGlobals->pSaveData;

	if ( pSaveData )
		pSaveData->connectionCount = BuildChangeList( pSaveData->levelList, MAX_LEVEL_CONNECTIONS );
}


//
// GLOBALS ASSUMED SET:  g_ulFrameCount
//
void StartFrame( void )
{
	if ( g_pGameRules )
		g_pGameRules->Think();

	if ( g_fGameOver )
		return;

	gpGlobals->teamplay = teamplay->value;
	g_ulFrameCount++;

	if( g_psv_maxspeed->modified )
	{
		char	msg[64];

		// maxspeed is modified, refresh maxspeed for each client
		for( int i = 0; i < gpGlobals->maxClients; i++ )
		{
			CBaseEntity *pClient = UTIL_PlayerByIndex( i + 1 );
			if( FNullEnt( pClient )) continue;

			g_engfuncs.pfnSetClientMaxspeed( pClient->edict(), g_psv_maxspeed->value );
		}

		sprintf( msg, "sv_maxspeed is changed to %g\n", g_psv_maxspeed->value );
		g_engfuncs.pfnServerPrint( msg );
		g_psv_maxspeed->modified = false;
	}

//	CheckDesiredList(); //LRC
	CheckAssistList(); //LRC
}

void EndFrame( void )
{
}

void ClientPrecache( void )
{
	// setup precaches always needed
	PRECACHE_SOUND("player/sprayer.wav");			// spray paint sound for PreAlpha

	// PRECACHE_SOUND("player/pl_jumpland2.wav");		// UNDONE: play 2x step sound

	PRECACHE_SOUND("player/pl_fallpain2.wav");
	PRECACHE_SOUND("player/pl_fallpain3.wav");

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

	PRECACHE_SOUND("debris/wood1.wav");			// hit wood texture
	PRECACHE_SOUND("debris/wood2.wav");
	PRECACHE_SOUND("debris/wood3.wav");

	PRECACHE_SOUND("plats/train_use1.wav");		// use a train

	PRECACHE_SOUND("buttons/spark5.wav");		// hit computer texture
	PRECACHE_SOUND("buttons/spark6.wav");
	PRECACHE_SOUND("debris/glass1.wav");
	PRECACHE_SOUND("debris/glass2.wav");
	PRECACHE_SOUND("debris/glass3.wav");

	PRECACHE_SOUND( SOUND_FLASHLIGHT_ON );
	PRECACHE_SOUND( SOUND_FLASHLIGHT_OFF );

// player gib sounds
	PRECACHE_SOUND("common/bodysplat.wav");

// player pain sounds
	PRECACHE_SOUND("player/pl_pain2.wav");
	PRECACHE_SOUND("player/pl_pain4.wav");
	PRECACHE_SOUND("player/pl_pain5.wav");
	PRECACHE_SOUND("player/pl_pain6.wav");
	PRECACHE_SOUND("player/pl_pain7.wav");

	PRECACHE_MODEL("models/player.mdl");

	// hud sounds

	PRECACHE_SOUND("common/wpn_hudoff.wav");
	PRECACHE_SOUND("common/wpn_hudon.wav");
	PRECACHE_SOUND("common/wpn_moveselect.wav");
	PRECACHE_SOUND("common/wpn_select.wav");
	PRECACHE_SOUND("common/wpn_denyselect.wav");

	// geiger sounds

	PRECACHE_SOUND("player/geiger6.wav");
	PRECACHE_SOUND("player/geiger5.wav");
	PRECACHE_SOUND("player/geiger4.wav");
	PRECACHE_SOUND("player/geiger3.wav");
	PRECACHE_SOUND("player/geiger2.wav");
	PRECACHE_SOUND("player/geiger1.wav");

	if (giPrecacheGrunt)
		UTIL_PrecacheOther("monster_human_grunt");
}

/*
===============
GetGameDescription

Returns the descriptive name of this .dll.  E.g., Half-Life, or Team Fortress 2
===============
*/
const char *GetGameDescription()
{
#if 0
	if ( g_pGameRules ) // this function may be called before the world has spawned, and the game rules initialized
		return g_pGameRules->GetGameDescription();
	else
		return GAME_NAME;
#else
	// alternative handle come from Xash 0.4
	char	*token = NULL;
          char	szbuffer[128];
	char	*afile = (char *)LOAD_FILE( "gameinfo.txt", NULL );
	const char *pfile = afile; 

	memset( text, 0, sizeof( text ));

	if( pfile )
	{
		token = COM_ParseToken( &pfile );
			
		while( token )
		{
			if( !stricmp( token, "title" )) 
			{                                          
				token = COM_ParseToken( &pfile );
				sprintf( szbuffer, "%s ", token );
				strncat( text, szbuffer, sizeof( text ));
			}
			else if( !stricmp( token, "version" )) 
			{                                          
				token = COM_ParseToken( &pfile );
				strncat( text, token, sizeof( text ));
			}
			token = COM_ParseToken( &pfile );
		}

		FREE_FILE( afile );
		return text;
	}
	return GAME_NAME;
#endif
}

/*
================
SpectatorConnect

A spectator has joined the game
================
*/
void SpectatorConnect( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBaseSpectator *pPlayer = (CBaseSpectator *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorConnect( );
}

/*
================
SpectatorConnect

A spectator has left the game
================
*/
void SpectatorDisconnect( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBaseSpectator *pPlayer = (CBaseSpectator *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorDisconnect( );
}

/*
================
SpectatorConnect

A spectator has sent a usercmd
================
*/
void SpectatorThink( edict_t *pEntity )
{
	entvars_t *pev = &pEntity->v;
	CBaseSpectator *pPlayer = (CBaseSpectator *)GET_PRIVATE(pEntity);

	if (pPlayer)
		pPlayer->SpectatorThink( );
}

// FIXME: implement VirtualClass GetClass instead
int AutoClassify( edict_t *pentToClassify )
{
	CBaseEntity *pClass;

	pClass = CBaseEntity::Instance( pentToClassify );
	if( !pClass ) return ED_SPAWNED;

	const char *classname = STRING( pClass->pev->classname );

	if ( !strnicmp( "worldspawn", classname, 10 ))
	{
		return ED_WORLDSPAWN;
	}

	// first pass: determine type by explicit parms
	if ( pClass->pev->solid == SOLID_TRIGGER )
	{
		if ( FClassnameIs( pClass->pev, "ambient_generic" ) || FClassnameIs( pClass->pev, "target_speaker" ))	// FIXME
		{
			pClass->pev->flags |= FL_PHS_FILTER;
			return ED_AMBIENT;
		}
		else if( pClass->pev->movetype == MOVETYPE_TOSS )
			return ED_NORMAL; // it's item or weapon
		return ED_TRIGGER; // never sending to client
	}
	else if ( pClass->pev->movetype == MOVETYPE_PHYSIC )
		return ED_RIGIDBODY;
	else if ( pClass->pev->solid == SOLID_BSP || pClass->pev->origin == g_vecZero )
	{
		if ( pClass->pev->movetype == MOVETYPE_CONVEYOR )
			return ED_MOVER;
		else if ( pClass->pev->flags & FL_WORLDBRUSH )
			return ED_BSPBRUSH;
		else if ( pClass->pev->movetype == MOVETYPE_PUSH ) 
			return ED_MOVER;
		else if ( pClass->pev->movetype == MOVETYPE_NONE )
			return ED_BSPBRUSH;
	}
	else if ( pClass->pev->flags & FL_MONSTER )
		return ED_MONSTER;
	else if ( pClass->pev->flags & FL_CLIENT )
		return ED_CLIENT;
	else if ( !pClass->pev->modelindex && !pClass->pev->aiment )
	{	
		if ( pClass->pev->noise || pClass->pev->noise1 || pClass->pev->noise2 || pClass->pev->noise3 )
		{
			pClass->pev->flags |= FL_PHS_FILTER;
			return ED_AMBIENT;
		}
		return ED_STATIC; // never sending to client
	}

	// mark as normal
	if ( pClass->pev->modelindex || pClass->pev->noise )
		return ED_NORMAL;

	// fail to classify :-(
	return ED_SPAWNED;
}

int ServerClassifyEdict( edict_t *pentToClassify )
{
	if( FNullEnt( pentToClassify ))
		return ED_SPAWNED;

	CBaseEntity *pClass;

	pClass = CBaseEntity::Instance( pentToClassify );
	if( !pClass ) return ED_SPAWNED;

	// user-defined
	if( pClass->m_iClassType != ED_SPAWNED )
		return pClass->m_iClassType;

	int m_iNewClass = AutoClassify( pentToClassify );

	if( m_iNewClass != ED_SPAWNED )
	{
		// tell server.dll about new class
		pClass->SetObjectClass( m_iNewClass );
	}

	return m_iNewClass;
}

void UpdateEntityState( entity_state_t *to, edict_t *from, int baseline )
{
	int	i;

	if( !to || !from ) return;

	CBaseEntity	*pNet;

	pNet = CBaseEntity::Instance( from );
	if( !pNet ) return;

	// setup some special edict flags (engine will clearing them after the current frame)
	if( to->modelindex != pNet->pev->modelindex )
		to->ed_flags |= ESF_NODELTA;

	// always set nodelta's for baseline
	if( baseline ) to->ed_flags |= ESF_NODELTA;

	// copy progs values to state
	to->solid = (solid_t)pNet->pev->solid;

	to->origin = pNet->pev->origin;
	to->angles = pNet->pev->angles;
	to->modelindex = pNet->pev->modelindex;
	to->health = pNet->pev->health;
	to->skin = pNet->pev->skin;		// studio model skin
	to->body = pNet->pev->body;		// studio model submodel 
	to->effects = pNet->pev->effects;	// shared client and render flags
	to->soundindex = pNet->pev->soundindex;	// soundindex
	to->renderfx = pNet->pev->renderfx;	// renderer flags
	to->rendermode = pNet->pev->rendermode;	// rendering mode
	to->renderamt = pNet->pev->renderamt;	// alpha value
	to->animtime = (int)(1000.0 * pNet->pev->animtime) * 0.001; // sequence time
	to->scale = pNet->pev->scale;		// shared client and render flags
	to->movetype = (movetype_t)pNet->pev->movetype;
	to->frame = pNet->pev->frame;		// any model current frame
	to->contents = pNet->pev->contents;	// physic contents
	to->framerate = pNet->pev->framerate;
	to->mins = pNet->pev->mins;
	to->maxs = pNet->pev->maxs;
	to->flags = pNet->pev->flags;
	to->rendercolor = pNet->pev->rendercolor;
	to->oldorigin = pNet->pev->oldorigin;
	to->colormap = pNet->pev->colormap;	// attachments
		
	if( pNet->pev->groundentity )
		to->groundent = ENTINDEX( pNet->pev->groundentity );
	else to->groundent = NULLENT_INDEX;

	// translate attached entity
	if( pNet->pev->aiment ) 
		to->aiment = ENTINDEX( pNet->pev->aiment );
	else to->aiment = NULLENT_INDEX;

	// studio model sequence
	if( pNet->pev->sequence != -1 ) to->sequence = pNet->pev->sequence;

	for( i = 0; i < 16; i++ )
	{
		// copy blendings and bone ctrlrs
		to->blending[i] = pNet->pev->blending[i];
		to->controller[i] = pNet->pev->controller[i];
	}
	if( to->ed_type == ED_CLIENT )
	{
		if( pNet->pev->fixangle )
		{
			to->ed_flags |= ESF_NO_PREDICTION;
		}

		if( pNet->pev->teleport_time )
		{
			to->ed_flags |= ESF_NO_PREDICTION;
			to->ed_flags |= ESF_NODELTA;
			pNet->pev->teleport_time = 0.0f;
		}

		if( pNet->pev->viewmodel )
			to->viewmodel = MODEL_INDEX( STRING( pNet->pev->viewmodel ));
		else to->viewmodel = 0;

		if( pNet->pev->aiment ) 
			to->aiment = ENTINDEX( pNet->pev->aiment );
		else to->aiment = NULLENT_INDEX;

		to->viewoffset = pNet->pev->view_ofs; 
		to->viewangles = pNet->pev->viewangles;
		to->idealpitch = pNet->pev->ideal_pitch;
		to->punch_angles = pNet->pev->punchangle;
		to->velocity = pNet->pev->velocity;
		to->basevelocity = pNet->pev->clbasevelocity;
		to->iStepLeft = pNet->pev->iStepLeft;
		to->flFallVelocity = pNet->pev->flFallVelocity;
		
		// playermodel sequence, that will be playing on a client
		if( pNet->pev->gaitsequence != -1 )
			to->gaitsequence = pNet->pev->gaitsequence;
		if( pNet->pev->weaponmodel != iStringNull )
			to->weaponmodel = MODEL_INDEX( STRING( pNet->pev->weaponmodel ));
		else to->weaponmodel = 0;
		to->weapons = pNet->pev->weapons;
		to->maxspeed = pNet->pev->maxspeed;

		// clamp fov
		if( pNet->pev->fov < 0.0 ) pNet->pev->fov = 0.0;
		if( pNet->pev->fov > 160 ) pNet->pev->fov = 160;
		to->fov = pNet->pev->fov;

		// clear EF_NOINTERP flag each frame for client
		pNet->pev->effects &= ~EF_NOINTERP;
	}
	else if( to->ed_type == ED_AMBIENT )
	{
		to->soundindex = pNet->pev->soundindex;

		if( pNet->pev->solid == SOLID_TRIGGER )
		{
			Vector	midPoint;

			// NOTE: no reason to compute this shit on the client - save bandwidth
			midPoint = pNet->pev->mins + pNet->pev->maxs * 0.5f;
			to->origin += midPoint;
		}
	}
	else if( to->ed_type == ED_MOVER || to->ed_type == ED_BSPBRUSH || to->ed_type == ED_PORTAL )
	{
		to->body = DirToBits( pNet->pev->movedir );

		// FIXME: send mins\maxs for sound spatialization and entity prediction ?
	}
	else if( to->ed_type == ED_BEAM )
	{
		to->gaitsequence = pNet->pev->frags;	// beam type

		// translate StartBeamEntity
		if( pNet->pev->owner ) 
			to->owner = ENTINDEX( pNet->pev->owner );
		else to->owner = NULLENT_INDEX;
	}
}

////////////////////////////////////////////////////////
// PAS and PVS routines for client messaging
//

/*
================
SetupVisibility

A client can have a separate "view entity" indicating that his/her view should depend on the origin of that
view entity.  If that's the case, then pViewEntity will be non-NULL and will be used.  Otherwise, the current
entity's origin is used.  Either is offset by the view_ofs to get the eye position.

From the eye position, we set up the PAS and PVS to use for filtering network messages to the client.  At this point, we could
 override the actual PAS or PVS values, or use a different origin.

NOTE:  Do not cache the values of pas and pvs, as they depend on reusable memory in the engine, they are only good for this one frame
================
*/
int SetupVisibility( edict_t *pViewEntity, edict_t *pClient, int portal, float *rgflViewOrg )
{
	Vector	org = g_vecZero;
	edict_t	*pView = pClient;

	if( portal )
	{
		// Entity's added from portal camera PVS
		if( FNullEnt( pViewEntity )) return 0; // broken portal ?

		CBaseEntity *pCamera = (CBaseEntity *)CBaseEntity::Instance( pViewEntity );

		if( !pCamera ) return 0;

		// determine visible point
		if( pCamera->m_iClassType == ED_PORTAL )
		{
			// don't build visibility for mirrors
			if( pCamera->pev->origin == pCamera->pev->oldorigin )
				return 0;
			else org = pCamera->pev->oldorigin;
		}
		else if( pCamera->m_iClassType == ED_SKYPORTAL )
		{
			org = pCamera->pev->origin;
		}
		else return 0; // other edicts can't merge pvs
	}
	else
	{
		// calc point view from client eyes or client camera's
		if( FNullEnt( pClient )) HOST_ERROR( "SetupVisibility: client == NULL\n" );
		if( !FNullEnt( pViewEntity )) pView = pViewEntity;

		if( pClient->v.flags & FL_PROXY )
		{
			// the spectator proxy sees and hears everything
			return -1; // force engine to ignore vis
		}

		CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );

		// for trigger_viewset
		if( pPlayer && pPlayer->viewFlags & 1 )
		{

			CBaseEntity *pViewEnt = UTIL_FindEntityByString( NULL, "targetname", STRING(pPlayer->viewEntity) );

			if (!FNullEnt(pViewEnt)) pView = pViewEnt->edict();
			else
			{	
				// try to find entity by classname
				CBaseEntity *pViewEnt = UTIL_FindEntityByString( NULL, "classname", STRING(pPlayer->viewEntity) );

				if ( !FNullEnt ( pViewEnt ))
					pView = pViewEnt->edict();
				else pPlayer->viewFlags = 0;
			}
		}
		// NOTE: view offset always contains actual viewheight (set it in PM_Move)
		org = pView->v.origin + pView->v.view_ofs;
	}

	org.CopyToArray( rgflViewOrg );
	return 1;
}

/*
AddToFullPack

Return 1 if the entity state has been filled in for the ent and the entity will be propagated to the client, 0 otherwise

state is the server maintained copy of the state info that is transmitted to the client
a MOD could alter values copied into state to send the "host" a different look for a particular entity update, etc.
e and ent are the entity that is being added to the update, if 1 is returned
host is the player's edict of the player whom we are sending the update to
player is 1 if the ent/e is a player and 0 otherwise
pSet is either the PAS or PVS that we previous set up.  We can use it to ask the engine to filter the entity against the PAS or PVS.
we could also use the pas/ pvs that we set in SetupVisibility, if we wanted to.  Caching the value is valid in that case, but still only for the current frame
*/
int AddToFullPack( edict_t *pHost, edict_t *pClient, edict_t *pEdict, int hostflags, int hostarea, byte *pSet )
{
	if( FNullEnt( pEdict )) return 0; // never adding invalid entities

	// completely ignore dormant entity
	if( pEdict->v.flags & FL_DORMANT )
		return 0;

	BOOL	bIsPortalPass = FALSE;

	if( FNullEnt( pHost )) pHost = pClient;		// pfnCustomView not set
	if( FNullEnt( pClient )) bIsPortalPass = TRUE;	// portal pass detected

	// don't send spectators to other players
	if( !bIsPortalPass && (( pEdict->v.flags & FL_SPECTATOR ) && ( pEdict != pHost )))
	{
		return 0;
	}

	CBaseEntity *pEntity = (CBaseEntity *)CBaseEntity::Instance( pEdict );

	if( !pEntity ) return 0;

	// quick reject by type
	switch( pEntity->m_iClassType )
	{
	case ED_SKYPORTAL:
		return 1;	// no additional check requires
	case ED_BEAM:	// FIXME: add check for beam bounding box
	case ED_MOVER:
	case ED_NORMAL:
	case ED_PORTAL:
	case ED_CLIENT:
	case ED_MONSTER:
	case ED_AMBIENT:
	case ED_BSPBRUSH:
	case ED_RIGIDBODY: break;
	default: return 0; // skipped
	}

	if( !ENGINE_CHECK_AREA( pEdict, hostarea ))
	{
		// blocked by a door
		return 0;
	}

	Vector	delta = g_vecZero;	// for ambient sounds

	// check for ambients distance
	if( pEntity->m_iClassType == ED_AMBIENT )
	{	
		Vector	entorigin;

		// don't send sounds if they will be attenuated away
		if( pEntity->pev->origin == g_vecZero )
			entorigin = (pEntity->pev->mins + pEntity->pev->maxs) * 0.5f;
		else entorigin = pEntity->pev->origin;

		// precalc delta distance for sounds
		delta = pHost->v.origin - entorigin;
	}

	// check visibility
	if( !ENGINE_CHECK_PVS( pEdict, pSet ))
	{
		float	m_flRadius = 1024;	// g-cont: tune distance by taste :)

		if( pEntity->pev->armorvalue > 0 )
			m_flRadius = pEntity->pev->armorvalue;

		if( pEntity->pev->flags & FL_PHS_FILTER && delta.Length() > m_flRadius )
			return 0;
	}

	if( FNullEnt( pHost )) HOST_ERROR( "pHost == NULL\n" );

	// don't send entity to local client if the client says it's predicting the entity itself.
	if( pEntity->pev->flags & FL_SKIPLOCALHOST )
	{
		if(( hostflags & 1 ) && ( pEntity->pev->owner == pHost ))
			return 0;
	}

	if( !pEntity->pev->modelindex || FStringNull( pEntity->pev->model ))
	{
		// can't ignore ents withouts models, because portals and mirrors can't working otherwise
		// and null.mdl it's no more needs to be set: ED_CLASS rejection is working fine
		// so we wan't reject this entities here. 
	}

	if( pHost->v.groupinfo )
	{
		UTIL_SetGroupTrace( pHost->v.groupinfo, GROUP_OP_AND );

		// should always be set, of course
		if( pEdict->v.groupinfo )
		{
			if( g_groupop == GROUP_OP_AND )
			{
				if(!( pEdict->v.groupinfo & pHost->v.groupinfo ))
					return 0;
			}
			else if( g_groupop == GROUP_OP_NAND )
			{
				if( pEdict->v.groupinfo & pHost->v.groupinfo )
					return 0;
			}
		}
		UTIL_UnsetGroupTrace();
	}
	return 1;
}

/*
=================
CmdStart

We're about to run this usercmd for the specified player.  We can set up groupinfo and masking here, etc.
This is the time to examine the usercmd for anything extra.  This call happens even if think does not.
=================
*/
void CmdStart( const edict_t *player, const struct usercmd_s *cmd, unsigned int random_seed )
{
	entvars_t *pev = (entvars_t *)&player->v;
	CBasePlayer *pl = ( CBasePlayer *) CBasePlayer::Instance( pev );

	if( !pl )
		return;

	if ( pl->pev->groupinfo != 0 )
	{
		UTIL_SetGroupTrace( pl->pev->groupinfo, GROUP_OP_AND );
	}

	pl->random_seed = random_seed;
}

/*
=================
CmdEnd

Each cmdstart is exactly matched with a cmd end, clean up any group trace flags, etc. here
=================
*/
void CmdEnd ( const edict_t *player )
{
	entvars_t *pev = (entvars_t *)&player->v;
	CBasePlayer *pl = ( CBasePlayer *) CBasePlayer::Instance( pev );

	if( !pl )
		return;
	if ( pl->pev->groupinfo != 0 )
	{
		UTIL_UnsetGroupTrace();
	}
}

/*
================================
ShouldCollide

  Called when the engine believes two entities are about to collide. Return 0 if you
  want the two entities to just pass through each other without colliding or calling the
  touch function.
================================
*/
int ShouldCollide( edict_t *pentTouched, edict_t *pentOther )
{
	return 1;
}