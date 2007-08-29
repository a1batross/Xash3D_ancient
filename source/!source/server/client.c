//=======================================================================
//			Copyright (C) XashXT Group 2007
//		   client.cpp - client/server game specific stuff
//=======================================================================

#include "baseentity.h"

void ClientUserinfoChanged (edict_t *ent, char *userinfo);

/*
===========
ClientConnect

Called when a player begins connecting to the server.
============
*/
bool ClientConnect (edict_t *ent, char *userinfo)
{
	char	*value;

	// check to see if they are on the banned IP list
	value = Info_ValueForKey (userinfo, "ip");

	if (SV_FilterPacket(value))
	{
		Info_SetValueForKey(userinfo, "rejmsg", "Banned.");
		return false;
	}

	// check for a spectator
	value = Info_ValueForKey (userinfo, "spectator");
	if (deathmatch->value && *value && strcmp(value, "0"))
	{
		int i, numspec;

		if (*spectator_password->string && strcmp(spectator_password->string, "none") && strcmp(spectator_password->string, value))
		{
			Info_SetValueForKey(userinfo, "rejmsg", "Spectator password required or incorrect.");
			return false;
		}

		// count spectators
		for (i = numspec = 0; i < maxclients->value; i++)
		{
			if (g_edicts[i+1].inuse && g_edicts[i+1].client->pers.spectator)
				numspec++;
		}
		if (numspec >= maxspectators->value) 
		{
			Info_SetValueForKey(userinfo, "rejmsg", "Server spectator limit is full.");
			return false;
		}
	} 
	else
	{
		// check for a password
		value = Info_ValueForKey (userinfo, "password");
		if (*password->string && strcmp(password->string, "none") && strcmp(password->string, value))
		{
			Info_SetValueForKey(userinfo, "rejmsg", "Password required or incorrect.");
			return false;
		}
	}


	// they can connect
	ent->client = game.clients + (ent - g_edicts - 1);

	// if there is already a body waiting for us (a loadgame), just
	// take it, otherwise spawn one from scratch
	if (ent->inuse == false)
	{
		// clear the respawning variables
		InitClientResp (ent->client);
		if (!game.autosaved || !ent->client->pers.weapon)
		{
			InitClientPersistant (ent->client,world->style);
		}
	}

	ClientUserinfoChanged (ent, userinfo);

	if (game.maxclients > 1) gi.dprintf ("%s connected\n", ent->client->pers.netname);

	ent->svflags = 0; // make sure we start with known default
	ent->client->pers.connected = true;

	return true;
}
