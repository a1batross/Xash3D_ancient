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
// sv_user.c -- server code for moving users

#include "engine.h"
#include "server.h"

edict_t	*sv_player;

/*
============================================================

USER STRINGCMD EXECUTION

sv_client and sv_player will be valid.
============================================================
*/

/*
==================
SV_BeginDemoServer
==================
*/
void SV_BeginDemoserver (void)
{
	char		name[MAX_OSPATH];

	sprintf (name, "demos/%s", sv.name);
	sv.demofile = FS_Open(name, "rb" );
	if (!sv.demofile) Host_Error("Couldn't open %s\n", name);
}

/*
================
SV_New_f

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
void SV_New_f (void)
{
	int		playernum;
	edict_t		*ent;

	if (sv_client->state != cs_connected)
	{
		Msg ("New not valid -- already spawned\n");
		return;
	}

	// demo servers just dump the file message
	if (sv.state == ss_demo)
	{
		SV_BeginDemoserver();
		return;
	}

	// send the serverdata
	MSG_WriteByte (&sv_client->netchan.message, svc_serverdata);
	MSG_WriteLong (&sv_client->netchan.message, PROTOCOL_VERSION);
	MSG_WriteLong (&sv_client->netchan.message, svs.spawncount);

	if(sv.state == ss_cinematic) playernum = -1;
	else playernum = sv_client - svs.clients;
	MSG_WriteShort (&sv_client->netchan.message, playernum );

	// send full levelname
	MSG_WriteString (&sv_client->netchan.message, sv.configstrings[CS_NAME]);

	// game server
	if (sv.state == ss_game)
	{
		// set up the entity for the client
		ent = PRVM_EDICT_NUM(playernum + 1);
		ent->priv.sv->serialnumber = playernum + 1;
		sv_client->edict = ent;
		memset (&sv_client->lastcmd, 0, sizeof(sv_client->lastcmd));

		// begin fetching configstrings
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("cmd configstrings %i 0\n",svs.spawncount) );
	}

}

/*
==================
SV_Configstrings_f
==================
*/
void SV_Configstrings_f (void)
{
	int			start;

	if (sv_client->state != cs_connected)
	{
		Msg ("configstrings not valid -- already spawned\n");
		return;
	}

	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		Msg ("SV_Configstrings_f from different level\n");
		SV_New_f ();
		return;
	}
	
	start = atoi(Cmd_Argv(2));

	// write a packet full of data
	while ( sv_client->netchan.message.cursize < MAX_MSGLEN/2 && start < MAX_CONFIGSTRINGS)
	{
		if (sv.configstrings[start][0])
		{
			MSG_WriteByte (&sv_client->netchan.message, svc_configstring);
			MSG_WriteShort (&sv_client->netchan.message, start);
			MSG_WriteString (&sv_client->netchan.message, sv.configstrings[start]);
		}
		start++;
	}

	// send next command

	if (start == MAX_CONFIGSTRINGS)
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("cmd baselines %i 0\n",svs.spawncount) );
	}
	else
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("cmd configstrings %i %i\n",svs.spawncount, start) );
	}
}

/*
==================
SV_Baselines_f
==================
*/
void SV_Baselines_f (void)
{
	int		start;
	entity_state_t	nullstate;
	entity_state_t	*base;

	if (sv_client->state != cs_connected)
	{
		Msg ("baselines not valid -- already spawned\n");
		return;
	}
	
	// handle the case of a level changing while a client was connecting
	if ( atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		Msg ("SV_Baselines_f from different level\n");
		SV_New_f ();
		return;
	}
	
	start = atoi(Cmd_Argv(2));

	memset (&nullstate, 0, sizeof(nullstate));

	// write a packet full of data

	while ( sv_client->netchan.message.cursize <  MAX_MSGLEN/2 && start < MAX_EDICTS)
	{
		base = &sv.baselines[start];
		if (base->modelindex || base->soundindex || base->effects)
		{
			MSG_WriteByte (&sv_client->netchan.message, svc_spawnbaseline);
			MSG_WriteDeltaEntity (&nullstate, base, &sv_client->netchan.message, true, true);
		}
		start++;
	}

	// send next command

	if (start == MAX_EDICTS)
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("precache %i\n", svs.spawncount) );
	}
	else
	{
		MSG_WriteByte (&sv_client->netchan.message, svc_stufftext);
		MSG_WriteString (&sv_client->netchan.message, va("cmd baselines %i %i\n",svs.spawncount, start) );
	}
}

/*
==================
SV_Begin_f
==================
*/
void SV_Begin_f (void)
{
	// handle the case of a level changing while a client was connecting
	if( atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		Msg ("SV_Begin_f from different level\n");
		SV_New_f ();
		return;
	}

	sv_client->state = cs_spawned;
	
	// call the game begin function
	SV_ClientBegin(sv_player);
}

//=============================================================================

/*
==================
SV_NextDownload_f
==================
*/
void SV_NextDownload_f (void)
{
	int		r;
	int		percent;
	int		size;

	if (!sv_client->download)
		return;

	r = sv_client->downloadsize - sv_client->downloadcount;
	if (r > 1024)
		r = 1024;

	MSG_WriteByte (&sv_client->netchan.message, svc_download);
	MSG_WriteShort (&sv_client->netchan.message, r);

	sv_client->downloadcount += r;
	size = sv_client->downloadsize;
	if (!size)
		size = 1;
	percent = sv_client->downloadcount*100/size;
	MSG_WriteByte (&sv_client->netchan.message, percent);
	SZ_Write (&sv_client->netchan.message,
		sv_client->download + sv_client->downloadcount - r, r);

	if (sv_client->downloadcount != sv_client->downloadsize)
		return;

	sv_client->download = NULL;
}

/*
==================
SV_BeginDownload_f
==================
*/
void SV_BeginDownload_f(void)
{
	char	*name;
	extern	cvar_t *allow_download;
	extern	cvar_t *allow_download_players;
	extern	cvar_t *allow_download_models;
	extern	cvar_t *allow_download_sounds;
	extern	cvar_t *allow_download_maps;
	int offset = 0;

	name = Cmd_Argv(1);

	if (Cmd_Argc() > 2)
		offset = atoi(Cmd_Argv(2)); // downloaded offset

	// hacked by zoid to allow more conrol over download
	// first off, no .. or global allow check
	if (strstr (name, "..") || !allow_download->value
		// leading dot is no good
		|| *name == '.' 
		// leading slash bad as well, must be in subdir
		|| *name == '/'
		// next up, skin check
		|| (strncmp(name, "players/", 6) == 0 && !allow_download_players->value)
		// now models
		|| (strncmp(name, "models/", 6) == 0 && !allow_download_models->value)
		// now sounds
		|| (strncmp(name, "sound/", 6) == 0 && !allow_download_sounds->value)
		// now maps (note special case for maps, must not be in pak)
		|| (strncmp(name, "maps/", 6) == 0 && !allow_download_maps->value)
		// MUST be in a subdirectory	
		|| !strstr (name, "/") )	
	{	// don't allow anything with .. path
		MSG_WriteByte (&sv_client->netchan.message, svc_download);
		MSG_WriteShort (&sv_client->netchan.message, -1);
		MSG_WriteByte (&sv_client->netchan.message, 0);
		return;
	}

          sv_client->download = FS_LoadFile (name, &sv_client->downloadsize);
	sv_client->downloadcount = offset;

	if (offset > sv_client->downloadsize)
		sv_client->downloadcount = sv_client->downloadsize;

	if (!sv_client->download)
	{
		MsgWarn("SV_BeginDownload_f: couldn't download %s to %s\n", name, sv_client->name);
		if (sv_client->download)
		{
			sv_client->download = NULL;
		}

		MSG_WriteByte (&sv_client->netchan.message, svc_download);
		MSG_WriteShort (&sv_client->netchan.message, -1);
		MSG_WriteByte (&sv_client->netchan.message, 0);
		return;
	}

	SV_NextDownload_f ();
	MsgDev (D_INFO, "Downloading %s to %s\n", name, sv_client->name);
}



//============================================================================


/*
=================
SV_Disconnect_f

The client is going to disconnect, so remove the connection immediately
=================
*/
void SV_Disconnect_f (void)
{
//	SV_EndRedirect ();
	SV_DropClient (sv_client);	
}


/*
==================
SV_ShowServerinfo_f

Dumps the serverinfo info string
==================
*/
void SV_ShowServerinfo_f (void)
{
	Info_Print (Cvar_Serverinfo());
}


void SV_Nextserver (void)
{
	char	*v;

	// can't nextserver while playing a normal game
	if (sv.state == ss_game) return;

	svs.spawncount++;	// make sure another doesn't sneak in
	v = Cvar_VariableString ("nextserver");
	if (!v[0]) Cbuf_AddText ("killserver\n");
	else
	{
		Cbuf_AddText (v);
		Cbuf_AddText ("\n");
	}
	Cvar_Set ("nextserver","");
}

/*
==================
SV_Nextserver_f

A cinematic has completed or been aborted by a client, so move
to the next server,
==================
*/
void SV_Nextserver_f (void)
{
	if ( atoi(Cmd_Argv(1)) != svs.spawncount )
	{
		MsgWarn("SV_Nextserver_f: loading wrong level, from %s\n", sv_client->name);
		return; // leftover from last server
	}
	SV_Nextserver ();
}

typedef struct
{
	char	*name;
	void	(*func) (void);
} ucmd_t;

ucmd_t ucmds[] =
{
	// auto issued
	{"new", SV_New_f},
	{"configstrings", SV_Configstrings_f},
	{"baselines", SV_Baselines_f},
	{"begin", SV_Begin_f},
	{"nextserver", SV_Nextserver_f},
	{"disconnect", SV_Disconnect_f},
	{"info", SV_ShowServerinfo_f},
	{"download", SV_BeginDownload_f},
	{"nextdl", SV_NextDownload_f},
	{NULL, NULL}
};

/*
==================
SV_ExecuteUserCommand
==================
*/
void SV_ExecuteUserCommand (char *s)
{
	ucmd_t	*u;
	
	Cmd_TokenizeString(s);
	sv_player = sv_client->edict;

	for (u = ucmds; u->name; u++)
	{
		if (!strcmp (Cmd_Argv(0), u->name) )
		{
			u->func();
			break;
		}
	}

	if (!u->name && sv.state == ss_game)
	{
		// custom client commands
		prog->globals.sv->pev = PRVM_EDICT_TO_PROG(sv_player);
		prog->globals.sv->time = sv.time;
		prog->globals.sv->frametime = sv.frametime;
		PRVM_ExecuteProgram (prog->globals.sv->ClientCommand, "QC function ClientCommand is missing");
	}
}

/*
===========================================================================

USER CMD EXECUTION

===========================================================================
*/

void SV_ClientThink (client_state_t *cl, usercmd_t *cmd)
{
	cl->commandMsec -= cmd->msec;

	if (cl->commandMsec < 0 && sv_enforcetime->value )
	{
		MsgWarn("SV_ClientThink: commandMsec underflow from %s\n", cl->name);
		return;
	}
	ClientThink (cl->edict, cmd);
}

/*
===================
SV_ExecuteClientMessage

The current net_message is parsed for the given client
===================
*/
#define	MAX_STRINGCMDS	8

void SV_ExecuteClientMessage (client_state_t *cl)
{
	int		c;
	char		*s;

	usercmd_t	nullcmd;
	usercmd_t	oldest, oldcmd, newcmd;
	int		net_drop;
	int		stringCmdCount;
	int		checksum, calculatedChecksum;
	int		checksumIndex;
	bool	move_issued;
	int		lastframe;

	sv_client = cl;
	sv_player = sv_client->edict;

	// only allow one move command
	move_issued = false;
	stringCmdCount = 0;

	while (1)
	{
		if (net_message.readcount > net_message.cursize)
		{
			MsgWarn("SV_ReadClientMessage: bad read\n");
			SV_DropClient (cl);
			return;
		}	

		c = MSG_ReadByte (&net_message);
		if (c == -1) break;
				
		switch (c)
		{
		default:
			MsgWarn("SV_ReadClientMessage: unknown command char\n");
			SV_DropClient (cl);
			return;
					
		case clc_nop:
			break;

		case clc_userinfo:
			strncpy (cl->userinfo, MSG_ReadString (&net_message), sizeof(cl->userinfo)-1);
			SV_UserinfoChanged (cl);
			break;

		case clc_move:
			if (move_issued) return; // someone is trying to cheat...

			move_issued = true;
			checksumIndex = net_message.readcount;
			checksum = MSG_ReadByte (&net_message);
			lastframe = MSG_ReadLong (&net_message);
			if (lastframe != cl->lastframe)
			{
				cl->lastframe = lastframe;
				if (cl->lastframe > 0)
				{
					cl->frame_latency[cl->lastframe&(LATENCY_COUNTS-1)] = svs.realtime - cl->frames[cl->lastframe & UPDATE_MASK].senttime;
				}
			}

			memset (&nullcmd, 0, sizeof(nullcmd));
			MSG_ReadDeltaUsercmd (&net_message, &nullcmd, &oldest);
			MSG_ReadDeltaUsercmd (&net_message, &oldest, &oldcmd);
			MSG_ReadDeltaUsercmd (&net_message, &oldcmd, &newcmd);

			if ( cl->state != cs_spawned )
			{
				cl->lastframe = -1;
				break;
			}

			// if the checksum fails, ignore the rest of the packet
			calculatedChecksum = CRC_Sequence(net_message.data + checksumIndex + 1, net_message.readcount - checksumIndex - 1, cl->netchan.incoming_sequence);

			if (calculatedChecksum != checksum)
			{
				MsgWarn("SV_ExecuteClientMessage: failed command checksum for %s (%d != %d)/%d\n", cl->name, calculatedChecksum, checksum,  cl->netchan.incoming_sequence);
				return;
			}

			if (!sv_paused->value)
			{
				net_drop = cl->netchan.dropped;
				if (net_drop < 20)
				{
					while (net_drop > 2)
					{
						SV_ClientThink (cl, &cl->lastcmd);
						net_drop--;
					}
					if (net_drop > 1) SV_ClientThink (cl, &oldest);
					if (net_drop > 0) SV_ClientThink (cl, &oldcmd);
				}
				SV_ClientThink (cl, &newcmd);
			}
			cl->lastcmd = newcmd;
			break;

		case clc_stringcmd:	
			s = MSG_ReadString (&net_message);

			// malicious users may try using too many string commands
			if (++stringCmdCount < MAX_STRINGCMDS) SV_ExecuteUserCommand (s);
			if (cl->state == cs_zombie) return; // disconnect command
			break;
		}
	}
}

