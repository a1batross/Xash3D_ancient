//=======================================================================
//			Copyright XashXT Group 2009 ©
//		     sv_init.c - server initialize operations
//=======================================================================

#include "common.h"
#include "server.h"

server_static_t	svs;	// persistant server info
svgame_static_t	svgame;	// persistant game info
server_t		sv;	// local server

/*
================
SV_FindIndex

================
*/
int SV_FindIndex( const char *name, int start, int end, bool create )
{
	int	i;
	
	if( !name || !name[0] ) return 0;

	for( i = 1; i < end && sv.configstrings[start+i][0]; i++ )
		if(!com.strcmp( sv.configstrings[start+i], name ))
			return i;
	if( !create ) return 0;
	if( i == end ) 
	{
		MsgDev( D_WARN, "SV_FindIndex: %d out of range [%d - %d]\n", start, end );
		return 0;
	}

	// register new resource
	com.strncpy( sv.configstrings[start+i], name, sizeof( sv.configstrings[i] ));

	if( sv.state != ss_loading )
	{	
		// send the update to everyone
		MSG_Clear( &sv.multicast );
		MSG_Begin( svc_configstring );
		MSG_WriteShort( &sv.multicast, start + i );
		MSG_WriteString( &sv.multicast, name );
		MSG_Send( MSG_ALL, vec3_origin, NULL );
	}
	return i;
}

int SV_ModelIndex( const char *name )
{
	return SV_FindIndex( name, CS_MODELS, MAX_MODELS, true );
}

int SV_SoundIndex( const char *name )
{
	return SV_FindIndex( name, CS_SOUNDS, MAX_SOUNDS, true );
}

int SV_UserMessageIndex( const char *name )
{
	return SV_FindIndex( name, CS_USER_MESSAGES, MAX_USER_MESSAGES, true );
}

int SV_DecalIndex( const char *name )
{
	return SV_FindIndex( name, CS_DECALS, MAX_DECALS, true );
}

int SV_EventIndex( const char *name )
{
	return SV_FindIndex( name, CS_EVENTS, MAX_EVENTS, true );
}

int SV_GenericIndex( const char *name )
{
	return SV_FindIndex( name, CS_GENERICS, MAX_GENERICS, true );
}

int SV_ClassIndex( const char *name )
{
	return SV_FindIndex( name, CS_CLASSNAMES, MAX_CLASSNAMES, true );
}

script_t *CM_GetEntityScript( void )
{
	if( !pe ) return SV_GetEntityScript( sv.name );
	return pe->GetEntityScript();
}

/*
================
SV_PrepModels

Called after changing physic.dll
================
*/
void SV_PrepModels( void )
{
	string	name;
	int	i;

	CM_BeginRegistration( sv.configstrings[CS_MODELS+1], false, NULL );

	for( i = 0; i < MAX_MODELS && sv.configstrings[CS_MODELS+1+i][0]; i++ )
	{
		com.strncpy( name, sv.configstrings[CS_MODELS+1+i], MAX_STRING );
		CM_RegisterModel( name, i+1 );
	}
	CM_EndRegistration (); // free unused models

	sv.cphys_prepped = true;
}

/*
================
SV_CreateBaseline

Entity baselines are used to compress the update messages
to the clients -- only the fields that differ from the
baseline will be transmitted
================
*/
void SV_CreateBaseline( void )
{
	edict_t	*pEdict;
	int	entnum;	

	for( entnum = 1; entnum < svgame.globals->numEntities; entnum++ )
	{
		pEdict = EDICT_NUM( entnum );
		if( pEdict->free ) continue;
		SV_ClassifyEdict( pEdict, ED_SPAWNED );
	}
}

/*
=================
SV_CheckForSavegame
=================
*/
void SV_CheckForSavegame( const char *savename )
{
	sv.loadgame = false;

	if( savename )
	{
		sv.loadgame = true; // predicting state
		if( sv_noreload->value ) sv.loadgame = false;
		if( svgame.globals->deathmatch || svgame.globals->coop || svgame.globals->teamplay )
			sv.loadgame = false;
		if( !savename ) sv.loadgame = false;
		if( !FS_FileExists( va( "save/%s", savename )))
			sv.loadgame = false;
	}
}

/*
================
SV_ActivateServer

activate server on changed map, run physics
================
*/
void SV_ActivateServer( void )
{
	int	i;

	// Activate the DLL server code
	svgame.dllFuncs.pfnServerActivate( svgame.edicts, svgame.globals->numEntities, svgame.globals->maxClients );

	// all precaches are complete
	sv.state = ss_active;
	Host_SetServerState( sv.state );
	
	// create a baseline for more efficient communications
	SV_CreateBaseline();

	// run two frames to allow everything to settle
	for( i = 0; i < 2; i++ )
	{
		sv.frametime = 100;
		SV_Physics();
	}

	// invoke to refresh all movevars
	Mem_Set( &svgame.oldmovevars, 0, sizeof( movevars_t ));

	// setup hostflags
	sv.hostflags = 0;

	// tell what kind of server has been started.
	if( svgame.globals->maxClients > 1 )
	{
		MsgDev( D_INFO, "%i player server started\n", svgame.globals->maxClients );
	}
	else
	{
		MsgDev( D_INFO, "Game started\n" );
	}

	// set serverinfo variable
	Cvar_FullSet( "mapname", sv.name, CVAR_SERVERINFO|CVAR_INIT );

	CM_EndRegistration (); // free unused models
	sv.cphys_prepped = true;
}

/*
================
SV_DeactivateServer

deactivate server, free edicts stringtables etc
================
*/
void SV_DeactivateServer( void )
{
	SV_FreeEdicts ();
	sv.paused = false;

	// leave unchanged, because we wan't load it twice
	if( !sv.loadgame ) StringTable_Clear( svgame.hStringTable );

	svgame.dllFuncs.pfnServerDeactivate();

	svgame.globals->maxEntities = GI->max_edicts;
	svgame.globals->maxClients = sv_maxclients->integer;
	svgame.globals->numEntities = svgame.globals->maxClients + 1; // clients + world
	svgame.globals->numClients = 0;
	svgame.globals->mapname = 0;
}

/*
================
SV_LevelInit

Spawn all entities
================
*/
void SV_LevelInit( const char *newmap, const char *oldmap, const char *savename )
{
	if( sv.loadgame )
	{
		if( savename ) SV_ReadLevelFile( savename );
		else SV_SpawnEntities( newmap, CM_GetEntityScript( ));
	}
	else if( sv.changelevel )
	{
		SV_ReadSaveFile( savename );	// initialize StringTable and globals
		SV_MergeLevelFile( savename );// combine moveable entities with newmap
	}
	else SV_SpawnEntities( newmap, CM_GetEntityScript());
}

/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.

================
*/
void SV_SpawnServer( const char *server, const char *startspot )
{
	uint	i, checksum;
	int	current_skill;
	int	loadgame, changelevel;

	Msg( "SpawnServer [^2%s^7]\n", server );

	Cmd_ExecuteString( "latch\n" );

	if( sv.state == ss_dead && !sv.loadgame )
		SV_InitGame(); // the game is just starting

	SV_BroadcastCommand( "changing\n" );

	if( sv.state == ss_active )
	{
		SV_BroadcastCommand( "reconnect\n" );
		SV_SendClientMessages();
		SV_DeactivateServer (); // server is shutting down ...
	}

	svs.timestart = Sys_Milliseconds();
	svs.spawncount++; // any partially connected client will be restarted

	// save state
	loadgame = sv.loadgame;
	changelevel = sv.changelevel;

	sv.state = ss_dead;
	Host_SetServerState( sv.state );
	Mem_Set( &sv, 0, sizeof( sv ));	// wipe the entire per-level structure

	// restore state
	sv.paused = false;
	sv.loadgame = loadgame;
	sv.changelevel = changelevel;

	// initialize buffers
	MSG_Init( &sv.multicast, sv.multicast_buf, sizeof( sv.multicast_buf ));
	MSG_Init( &sv.signon, sv.signon_buf, sizeof( sv.signon_buf ));

	// leave slots at start for clients only
	for( i = 0; i < sv_maxclients->integer; i++ )
	{
		// needs to reconnect
		if( svs.clients[i].state > cs_connected )
			svs.clients[i].state = cs_connected;
		svs.clients[i].lastframe = -1;
	}

	// make cvars consistant
	if( Cvar_VariableInteger( "coop" )) Cvar_SetValue( "deathmatch", 0 );
	current_skill = Q_rint( Cvar_VariableValue( "skill" ));
	if( current_skill < 0 ) current_skill = 0;
	if( current_skill > 3 ) current_skill = 3;

	Cvar_SetValue( "skill", (float)current_skill );

	sv.time = 1000;

	FS_FileBase( server, sv.name ); // make sure what server name doesn't contain path and extension
	com.strncpy( svs.mapname, sv.name, sizeof( svs.mapname ));
	com.strncpy( sv.configstrings[CS_NAME], sv.name, CS_SIZE);
	if( startspot ) com.strncpy( sv.startspot, startspot, sizeof( sv.startspot ));
	else sv.startspot[0] = '\0';

	com.sprintf( sv.configstrings[CS_MODELS+1], "maps/%s.bsp", sv.name );
	CM_BeginRegistration( sv.configstrings[CS_MODELS+1], false, &checksum );
	com.sprintf( sv.configstrings[CS_MAPCHECKSUM], "%i", checksum );
	com.strncpy( sv.configstrings[CS_SKYNAME], "<skybox>", 64 );

	if( CM_VisData() == NULL ) MsgDev( D_WARN, "map ^2%s^7 has no visibility\n", server );

	// clear physics interaction links
	SV_ClearWorld();

	for( i = 1; i < CM_NumBmodels(); i++ )
	{
		com.sprintf( sv.configstrings[CS_MODELS+1+i], "*%i", i );
		CM_RegisterModel( sv.configstrings[CS_MODELS+1+i], i+1 );
	}

	// precache and static commands can be issued during map initialization
	sv.state = ss_loading;
	Host_SetServerState( sv.state );
}

/*
==============
SV_InitGame

A brand new game has been started
==============
*/
void SV_InitGame( void )
{
	char	i, idmaster[32];
	edict_t	*ent;
	
	if( svs.initialized )
	{
		// cause any connected clients to reconnect
		com.strncpy( host.finalmsg, "Server restarted\n", MAX_STRING );
		SV_Shutdown( true );
	}
	else
	{
		// init game after host error
		if( !svgame.hInstance )
			SV_LoadProgs( "server" );

		// fire once
		MsgDev( D_INFO, "Dll loaded for mod %s\n", svgame.dllFuncs.pfnGetGameDescription() );

		// make sure the client is down
		CL_Drop();
	}

	svs.initialized = true;

	if( Cvar_VariableValue( "coop" ) && Cvar_VariableValue ( "deathmatch" ) && Cvar_VariableValue( "teamplay" ))
	{
		Msg("Deathmatch, Teamplay and Coop set, defaulting to Deathmatch\n");
		Cvar_FullSet( "coop", "0",  CVAR_SERVERINFO|CVAR_LATCH );
		Cvar_FullSet( "teamplay", "0",  CVAR_SERVERINFO|CVAR_LATCH );
	}

	// dedicated servers are can't be single player and are usually DM
	// so unless they explicity set coop, force it to deathmatch
	if( host.type == HOST_DEDICATED )
	{
		if(!Cvar_VariableValue( "coop" ) && !Cvar_VariableValue( "teamplay" ))
			Cvar_FullSet( "deathmatch", "1",  CVAR_SERVERINFO|CVAR_LATCH );
	}

	// init clients
	if( Cvar_VariableValue( "deathmatch" ) || Cvar_VariableValue( "teamplay" ))
	{
		if( sv_maxclients->integer <= 1 )
			Cvar_FullSet( "sv_maxclients", "8", CVAR_SERVERINFO|CVAR_LATCH );
		else if( sv_maxclients->integer > 255 )
			Cvar_FullSet( "sv_maxclients", "255", CVAR_SERVERINFO|CVAR_LATCH );
	}
	else if( Cvar_VariableValue( "coop" ))
	{
		if( sv_maxclients->integer <= 1 || sv_maxclients->integer > 4 )
			Cvar_FullSet( "sv_maxclients", "4", CVAR_SERVERINFO|CVAR_LATCH );
	}
	else	
	{
		// non-deathmatch, non-coop is one player
		Cvar_FullSet( "sv_maxclients", "1", CVAR_SERVERINFO|CVAR_LATCH );
	}

	svs.spawncount = RANDOM_LONG( 0, 65535 );
	svs.clients = Z_Malloc( sizeof( sv_client_t ) * sv_maxclients->integer );
	svs.num_client_entities = sv_maxclients->integer * UPDATE_BACKUP * 64; // g-cont: what a mem waster ???
	svs.client_entities = Z_Malloc( sizeof( entity_state_t ) * svs.num_client_entities );
	svs.baselines = Z_Malloc( sizeof( entity_state_t ) * GI->max_edicts );

	// copy gamemode into svgame.globals
	svgame.globals->deathmatch = Cvar_VariableInteger( "deathmatch" );
	svgame.globals->teamplay = Cvar_VariableInteger( "teamplay" );
	svgame.globals->coop = Cvar_VariableInteger( "coop" );

	svgame.dllFuncs.pfnResetGlobalState();

	// heartbeats will always be sent to the id master
	svs.last_heartbeat = MAX_HEARTBEAT; // send immediately
	com.sprintf( idmaster, "192.246.40.37:%i", PORT_MASTER );
	NET_StringToAdr( idmaster, &master_adr[0] );

	for( i = 0; i < sv_maxclients->integer; i++ )
	{
		ent = EDICT_NUM( i + 1 );
		ent->serialnumber = i + 1;
		svs.clients[i].edict = ent;
		Mem_Set( &svs.clients[i].lastcmd, 0, sizeof( svs.clients[i].lastcmd ));
	}
}

bool SV_Active( void )
{
	return svs.initialized;
}

void SV_ForceMod( void )
{
	sv.cphys_prepped = false;
}