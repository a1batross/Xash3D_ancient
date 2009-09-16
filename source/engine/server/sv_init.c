/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

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
	int		i = 0;
	
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
	com.strncpy (sv.configstrings[start+i], name, sizeof(sv.configstrings[i]));

	if( sv.state != ss_loading )
	{	
		// send the update to everyone
		MSG_Clear( &sv.multicast );
		MSG_Begin( svc_configstring );
		MSG_WriteShort( &sv.multicast, start + i );
		MSG_WriteString( &sv.multicast, name );
		MSG_Send( MSG_ALL_R, vec3_origin, NULL );
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

int SV_ClassIndex( const char *name )
{
	return SV_FindIndex( name, CS_CLASSNAMES, MAX_CLASSNAMES, true );
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
	edict_t	*svent;
	int	entnum;	

	for( entnum = 1; entnum < svgame.globals->numEntities; entnum++ )
	{
		svent = EDICT_NUM( entnum );
		if( svent->free ) continue;
		if( !svent->v.modelindex && !svent->pvServerData->s.soundindex && !svent->v.effects )
			continue;
		svent->serialnumber = entnum;

		// take current state as baseline
		SV_UpdateEntityState( svent, true );

		svs.baselines[entnum] = svent->pvServerData->s;
	}

	// classify edicts for quick network sorting
	for( entnum = 0; entnum < svgame.globals->numEntities; entnum++ )
	{
		svent = EDICT_NUM( entnum );
		SV_ClassifyEdict( svent );
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
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.

================
*/
void SV_SpawnServer( const char *server, const char *savename )
{
	uint	i, checksum;

	Msg( "SpawnServer [%s]\n", server );

	svs.timestart = Sys_DoubleTime ();
	svs.spawncount++; // any partially connected client will be restarted
	sv.state = ss_dead;
	Host_SetServerState( sv.state );

	// wipe the entire per-level structure
	Mem_Set( &sv, 0, sizeof( sv ));
	svs.realtime = 0.0f;

	// save name for levels that don't set message
	com.strncpy( sv.configstrings[CS_NAME], server, CS_SIZE );
	MSG_Init( &sv.multicast, sv.multicast_buf, sizeof( sv.multicast_buf ));
	com.strcpy( sv.name, server );

	// leave slots at start for clients only
	for( i = 0; i < Host_MaxClients(); i++ )
	{
		// needs to reconnect
		if( svs.clients[i].state > cs_connected )
			svs.clients[i].state = cs_connected;
		svs.clients[i].deltamessage = -1;
		svs.clients[i].nextsnapshot = svs.realtime;	// generate a snapshot immediately
	}

	sv.time = 1.0;
	sv.frametime = 0.1f;
	
	com.strncpy( sv.name, server, MAX_STRING );
	FS_FileBase(server, sv.configstrings[CS_NAME]);

	com.sprintf( sv.configstrings[CS_MODELS+1], "maps/%s", server );
	sv.worldmodel = sv.models[1] = pe->BeginRegistration( sv.configstrings[CS_MODELS+1], false, &checksum );
	com.sprintf( sv.configstrings[CS_MAPCHECKSUM], "%i", checksum );
	com.strncpy( sv.configstrings[CS_SKYNAME], "<skybox>", 64 );

	// clear physics interaction links
	SV_ClearWorld();

	for( i = 1; i < pe->NumBmodels(); i++ )
	{
		com.sprintf( sv.configstrings[CS_MODELS+1+i], "*%i", i );
		sv.models[i+1] = pe->RegisterModel(sv.configstrings[CS_MODELS+1+i] );
	}

	//
	// spawn the rest of the entities on the map
	//	

	// precache and static commands can be issued during
	// map initialization
	sv.state = ss_loading;
	Host_SetServerState( sv.state );

	// check for a savegame
	SV_CheckForSavegame( savename );

	if( sv.loadgame ) SV_ReadLevelFile( savename );
	else SV_SpawnEntities( sv.name, pe->GetEntityScript());

	svgame.dllFuncs.pfnServerActivate( EDICT_NUM( 0 ), svgame.globals->numEntities, svgame.globals->maxClients );

	// run two frames to allow everything to settle
	for( i = 0; i < 2; i++ ) SV_Physics();

	// all precaches are complete
	sv.state = ss_active;
	Host_SetServerState( sv.state );

	// create a baseline for more efficient communications
	SV_CreateBaseline();

	// set serverinfo variable
	Cvar_FullSet( "mapname", sv.name, CVAR_SERVERINFO|CVAR_INIT );
	pe->EndRegistration(); // free unused models
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
		// make sure the client is down
		CL_Drop();
	}

	MsgDev( D_INFO, "Dll loaded for mod %s\n", svgame.dllFuncs.pfnGetGameDescription() );

	Cmd_ExecuteString( "latch\n" );
	svs.initialized = true;

	if( Cvar_VariableValue ("coop") && Cvar_VariableValue ( "deathmatch" ) && Cvar_VariableValue( "teamplay" ))
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
		if( Host_MaxClients() <= 1 )
			Cvar_FullSet( "host_maxclients", "8", CVAR_SERVERINFO|CVAR_LATCH );
		else if( Host_MaxClients() > 255 )
			Cvar_FullSet( "host_maxclients", va("%i", 255 ), CVAR_SERVERINFO|CVAR_LATCH );
	}
	else if( Cvar_VariableValue( "coop" ))
	{
		if( Host_MaxClients() <= 1 || Host_MaxClients() > 4 )
			Cvar_FullSet( "host_maxclients", "4", CVAR_SERVERINFO|CVAR_LATCH );
	}
	else	
	{
		// non-deathmatch, non-coop is one player
		Cvar_FullSet( "host_maxclients", "1", CVAR_SERVERINFO|CVAR_LATCH );
	}

	svs.spawncount = RANDOM_LONG( 0, 65535 );
	svs.clients = Z_Malloc( sizeof(sv_client_t) * Host_MaxClients());
	svs.num_client_entities = Host_MaxClients() * UPDATE_BACKUP * 64; // g-cont: what a mem waster ???????
	svs.client_entities = Z_Malloc( sizeof(entity_state_t) * svs.num_client_entities );
	svs.baselines = Z_Malloc( sizeof(entity_state_t) * host.max_edicts );

	// copy gamemode into svgame.globals
	svgame.globals->deathmatch = (int)Cvar_VariableValue( "deathmatch" );
	svgame.globals->teamplay = (int)Cvar_VariableValue( "teamplay" );
	svgame.globals->coop = (int)Cvar_VariableValue( "coop" );

	svgame.dllFuncs.pfnResetGlobalState();

	// heartbeats will always be sent to the id master
	svs.last_heartbeat = MAX_HEARTBEAT; // send immediately
	com.sprintf( idmaster, "192.246.40.37:%i", PORT_MASTER );
	NET_StringToAdr( idmaster, &master_adr[0] );

	for( i = 0; i < Host_MaxClients(); i++ )
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