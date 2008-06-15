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
// cl_main.c  -- client main loop

#include "common.h"
#include "client.h"

cvar_t	*freelook;

cvar_t	*adr0;
cvar_t	*adr1;
cvar_t	*adr2;
cvar_t	*adr3;
cvar_t	*adr4;
cvar_t	*adr5;
cvar_t	*adr6;
cvar_t	*adr7;
cvar_t	*adr8;

cvar_t	*rcon_client_password;
cvar_t	*rcon_address;

cvar_t	*cl_noskins;
cvar_t	*cl_autoskins;
cvar_t	*cl_footsteps;
cvar_t	*cl_timeout;
cvar_t	*cl_predict;
cvar_t	*cl_showfps;
cvar_t	*cl_maxfps;
cvar_t	*cl_gun;

cvar_t	*cl_add_particles;
cvar_t	*cl_add_lights;
cvar_t	*cl_add_entities;
cvar_t	*cl_add_blend;

cvar_t	*cl_shownet;
cvar_t	*cl_showmiss;
cvar_t	*cl_showclamp;

cvar_t	*cl_paused;

cvar_t	*lookspring;
cvar_t	*lookstrafe;

cvar_t	*m_pitch;
cvar_t	*m_yaw;
cvar_t	*m_forward;
cvar_t	*m_side;

cvar_t	*cl_lightlevel;

//
// userinfo
//
cvar_t	*info_password;
cvar_t	*info_spectator;
cvar_t	*name;
cvar_t	*skin;
cvar_t	*rate;
cvar_t	*fov;
cvar_t	*hand;
cvar_t	*gender;
cvar_t	*gender_auto;

cvar_t	*cl_vwep;

client_static_t	cls;
client_t		cl;

centity_t		cl_entities[MAX_EDICTS];

entity_state_t	cl_parse_entities[MAX_PARSE_ENTITIES];

extern	cvar_t *allow_download;
extern	cvar_t *allow_download_players;
extern	cvar_t *allow_download_models;
extern	cvar_t *allow_download_sounds;
extern	cvar_t *allow_download_maps;
//======================================================================

//======================================================================

/*
===================
Cmd_ForwardToServer

adds the current command line as a clc_stringcmd to the client message.
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void Cmd_ForwardToServer (void)
{
	char	*cmd;

	if( cls.demoplayback ) return; // not really connected

	cmd = Cmd_Argv(0);
	if (cls.state <= ca_connected || *cmd == '-' || *cmd == '+')
	{
		Msg ("Unknown command \"%s\"\n", cmd);
		return;
	}

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, cmd);
	if (Cmd_Argc() > 1)
	{
		SZ_Print (&cls.netchan.message, " ");
		SZ_Print (&cls.netchan.message, Cmd_Args());
	}
}

/*
==================
CL_ForwardToServer_f
==================
*/
void CL_ForwardToServer_f (void)
{
	if( cls.demoplayback ) return; // not really connected
	if (cls.state != ca_connected && cls.state != ca_active)
	{
		Msg("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}
	
	// don't forward the first argument
	if (Cmd_Argc() > 1)
	{
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, Cmd_Args());
	}
}


/*
==================
CL_Pause_f
==================
*/
void CL_Pause_f (void)
{
	if(!Host_ServerState() && !cls.demoplayback )
		return; // but allow pause in demos

	// never pause in multiplayer
	if( Cvar_VariableValue ("maxclients") > 1 )
	{
		Cvar_SetValue ("paused", 0);
		return;
	}

	Cvar_SetValue( "paused", !cl_paused->value );
}

/*
==================
CL_Quit_f
==================
*/
void CL_Quit_f (void)
{
	CL_Disconnect();
	Sys_Quit();
}

/*
================
CL_Drop

Called after an Host_Error was thrown
================
*/
void CL_Drop (void)
{
	if (cls.state == ca_uninitialized) return;
	if (cls.state == ca_disconnected) return;

	CL_Disconnect();
}

/*
=======================
CL_SendConnectPacket

We have gotten a challenge from the server, so try and
connect.
======================
*/
void CL_SendConnectPacket (void)
{
	netadr_t	adr;
	int		port;

	if(!NET_StringToAdr (cls.servername, &adr))
	{
		MsgDev( D_INFO, "CL_SendConnectPacket: bad server address\n");
		cls.connect_time = 0;
		return;
	}
	if(adr.port == 0) adr.port = BigShort (PORT_SERVER);
	port = Cvar_VariableValue ("qport");

	userinfo_modified = false;
	Netchan_OutOfBandPrint(NS_CLIENT, adr, "connect %i %i %i \"%s\"\n", PROTOCOL_VERSION, port, cls.challenge, Cvar_Userinfo() );
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
void CL_CheckForResend (void)
{
	netadr_t	adr;

	// if the local server is running and we aren't
	// then connect
	if (cls.state == ca_disconnected && Host_ServerState())
	{
		cls.state = ca_connecting;
		com.strncpy (cls.servername, "localhost", sizeof(cls.servername)-1);
		// we don't need a challenge on the localhost
		CL_SendConnectPacket ();
		return;
	}

	// resend if we haven't gotten a reply yet
	if(cls.demoplayback || cls.state != ca_connecting)
		return;
	if(cls.realtime - cls.connect_time < 3.0f) return;

	if(!NET_StringToAdr (cls.servername, &adr))
	{
		MsgDev(D_INFO, "CL_CheckForResend: bad server address\n");
		cls.state = ca_disconnected;
		return;
	}
	if (adr.port == 0) adr.port = BigShort (PORT_SERVER);
	cls.connect_time = cls.realtime; // for retransmit requests
	Msg ("Connecting to %s...\n", cls.servername);
	Netchan_OutOfBandPrint (NS_CLIENT, adr, "getchallenge\n");
}


/*
================
CL_Connect_f

================
*/
void CL_Connect_f (void)
{
	char	*server;

	if (Cmd_Argc() != 2)
	{
		Msg ("usage: connect <server>\n");
		return;	
	}
	
	if(Host_ServerState())
	{	
		// if running a local server, kill it and reissue
		com.strncpy( host.finalmsg, "Server quit\n", MAX_STRING );
		SV_Shutdown( false );
	}
	else CL_Disconnect();

	server = Cmd_Argv (1);

	NET_Config( true );	// allow remote
	CL_Disconnect();

	cls.state = ca_connecting;
	strncpy (cls.servername, server, sizeof(cls.servername)-1);
	cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
}


/*
=====================
CL_Rcon_f

  Send the rest of the command line over as
  an unconnected command.
=====================
*/
void CL_Rcon_f (void)
{
	char	message[1024];
	int		i;
	netadr_t	to;

	if (!rcon_client_password->string)
	{
		Msg ("You must set 'rcon_password' before\n"
					"issuing an rcon command.\n");
		return;
	}

	message[0] = (char)255;
	message[1] = (char)255;
	message[2] = (char)255;
	message[3] = (char)255;
	message[4] = 0;

	NET_Config (true);		// allow remote

	strcat (message, "rcon ");

	strcat (message, rcon_client_password->string);
	strcat (message, " ");

	for (i=1 ; i<Cmd_Argc() ; i++)
	{
		strcat (message, Cmd_Argv(i));
		strcat (message, " ");
	}

	if (cls.state >= ca_connected)
		to = cls.netchan.remote_address;
	else
	{
		if (!strlen(rcon_address->string))
		{
			Msg ("You must either be connected,\n"
						"or set the 'rcon_address' cvar\n"
						"to issue rcon commands\n");

			return;
		}
		NET_StringToAdr (rcon_address->string, &to);
		if (to.port == 0)
			to.port = BigShort (PORT_SERVER);
	}
	
	NET_SendPacket (NS_CLIENT, strlen(message)+1, message, to);
}


/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState (void)
{
	S_StopAllSounds ();
	CL_ClearEffects ();
	CL_ClearTEnts ();

// wipe the entire cl structure
	memset (&cl, 0, sizeof(cl));
	memset (&cl_entities, 0, sizeof(cl_entities));

	SZ_Clear (&cls.netchan.message);

}

/*
=====================
CL_Disconnect

Goes from a connected state to full screen console state
Sends a disconnect message to the server
This is also called on Host_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect (void)
{
	byte	final[32];

	if (cls.state == ca_disconnected)
		return;

	VectorClear (cl.refdef.blend);
	M_ForceMenuOff ();

	cls.connect_time = 0;

	SCR_StopCinematic();

	CL_Stop_f();

	// send a disconnect message to the server
	final[0] = clc_stringcmd;
	com.strcpy ((char *)final+1, "disconnect");
	Netchan_Transmit(&cls.netchan, strlen(final), final);
	Netchan_Transmit(&cls.netchan, strlen(final), final);
	Netchan_Transmit(&cls.netchan, strlen(final), final);

	CL_ClearState ();

	// stop download
	if (cls.download)
	{
		FS_Close(cls.download);
		cls.download = NULL;
	}

	cls.state = ca_disconnected;
}

void CL_Disconnect_f (void)
{
	Host_Error("Disconnected from server\n");
}


/*
====================
CL_Packet_f

packet <destination> <contents>

Contents allows \n escape character
====================
*/
void CL_Packet_f (void)
{
	char	send[2048];
	int		i, l;
	char	*in, *out;
	netadr_t	adr;

	if (Cmd_Argc() != 3)
	{
		Msg ("packet <destination> <contents>\n");
		return;
	}

	NET_Config (true);		// allow remote

	if (!NET_StringToAdr (Cmd_Argv(1), &adr))
	{
		Msg ("Bad address\n");
		return;
	}
	if (!adr.port)
		adr.port = BigShort (PORT_SERVER);

	in = Cmd_Argv(2);
	out = send+4;
	send[0] = send[1] = send[2] = send[3] = (char)0xff;

	l = strlen (in);
	for (i=0 ; i<l ; i++)
	{
		if (in[i] == '\\' && in[i+1] == 'n')
		{
			*out++ = '\n';
			i++;
		}
		else
			*out++ = in[i];
	}
	*out = 0;

	NET_SendPacket (NS_CLIENT, out-send, send, adr);
}

/*
=================
CL_Changing_f

Just sent as a hint to the client that they should
drop to full console
=================
*/
void CL_Changing_f (void)
{
	//ZOID
	//if we are downloading, we don't change!  This so we don't suddenly stop downloading a map
	if (cls.download) return;

	S_StopAllSounds();
	cls.state = ca_connected;	// not active anymore, but not disconnected
	Msg("\nChanging map...\n");
}


/*
=================
CL_Reconnect_f

The server is changing levels
=================
*/
void CL_Reconnect_f (void)
{
	//ZOID
	//if we are downloading, we don't change!  This so we don't suddenly stop downloading a map
	if (cls.download) return;

	S_StopAllSounds ();
	if (cls.state == ca_connected)
	{
		Msg ("reconnecting...\n");
		cls.state = ca_connected;
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");		
		return;
	}

	if (*cls.servername)
	{
		if (cls.state >= ca_connected)
		{
			CL_Disconnect();
			cls.connect_time = cls.realtime - 1.5f;
		}
		else cls.connect_time = -99999; // fire immediately

		cls.state = ca_connecting;
		Msg ("reconnecting...\n");
	}
}

/*
=================
CL_ParseStatusMessage

Handle a reply from a ping
=================
*/
void CL_ParseStatusMessage (void)
{
	char	*s;

	s = MSG_ReadString(&net_message);

	Msg ("%s\n", s);
	M_AddToServerList (net_from, s);
}


/*
=================
CL_PingServers_f
=================
*/
void CL_PingServers_f (void)
{
	int			i;
	netadr_t	adr;
	char		name[32];
	char		*adrstring;
	cvar_t		*noudp;
	cvar_t		*noipx;

	NET_Config (true);		// allow remote

	// send a broadcast packet
	Msg ("pinging broadcast...\n");

	noudp = Cvar_Get ("noudp", "0", CVAR_INIT);
	if (!noudp->value)
	{
		adr.type = NA_BROADCAST;
		adr.port = BigShort(PORT_SERVER);
		Netchan_OutOfBandPrint (NS_CLIENT, adr, va("info %i", PROTOCOL_VERSION));
	}

	noipx = Cvar_Get ("noipx", "0", CVAR_INIT);
	if (!noipx->value)
	{
		adr.type = NA_BROADCAST_IPX;
		adr.port = BigShort(PORT_SERVER);
		Netchan_OutOfBandPrint (NS_CLIENT, adr, va("info %i", PROTOCOL_VERSION));
	}

	// send a packet to each address book entry
	for (i = 0; i < 16; i++)
	{
		com.sprintf (name, "adr%i", i);
		adrstring = Cvar_VariableString (name);
		if (!adrstring || !adrstring[0])
			continue;

		Msg ("pinging %s...\n", adrstring);
		if (!NET_StringToAdr (adrstring, &adr))
		{
			Msg ("Bad address: %s\n", adrstring);
			continue;
		}
		if (!adr.port)
			adr.port = BigShort(PORT_SERVER);
		Netchan_OutOfBandPrint (NS_CLIENT, adr, va("info %i", PROTOCOL_VERSION));
	}
}

/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
void CL_ConnectionlessPacket (void)
{
	char	*s;
	char	*c;
	
	MSG_BeginReading (&net_message);
	MSG_ReadLong (&net_message); // skip the -1

	s = MSG_ReadStringLine (&net_message);

	Cmd_TokenizeString(s);

	c = Cmd_Argv(0);

	MsgDev(D_INFO, "%s: %s\n", NET_AdrToString (net_from), c);

	// server connection
	if (!strcmp(c, "client_connect"))
	{
		if (cls.state == ca_connected)
		{
			Msg ("Dup connect received.  Ignored.\n");
			return;
		}
		Netchan_Setup (NS_CLIENT, &cls.netchan, net_from, cls.quakePort);
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");	
		cls.state = ca_connected;
		return;
	}

	// server responding to a status broadcast
	if (!strcmp(c, "info"))
	{
		CL_ParseStatusMessage ();
		return;
	}

	// remote command from gui front end
	if (!strcmp(c, "cmd"))
	{
		if(!NET_IsLocalAddress(net_from))
		{
			Msg ("Command packet from remote host.  Ignored.\n");
			return;
		}
		ShowWindow( host.hWnd, SW_RESTORE);
		SetForegroundWindow ( host.hWnd );
		s = MSG_ReadString (&net_message);
		Cbuf_AddText(s);
		Cbuf_AddText("\n");
		return;
	}
	// print command from somewhere
	if (!strcmp(c, "print"))
	{
		s = MSG_ReadString (&net_message);
		Msg ("%s", s);
		return;
	}

	// ping from somewhere
	if (!strcmp(c, "ping"))
	{
		Netchan_OutOfBandPrint (NS_CLIENT, net_from, "ack");
		return;
	}

	// challenge from the server we are connecting to
	if (!strcmp(c, "challenge"))
	{
		cls.challenge = atoi(Cmd_Argv(1));
		CL_SendConnectPacket ();
		return;
	}

	// echo request from server
	if (!strcmp(c, "echo"))
	{
		Netchan_OutOfBandPrint (NS_CLIENT, net_from, "%s", Cmd_Argv(1) );
		return;
	}

	Msg ("Unknown command.\n");
}


/*
=================
CL_DumpPackets

A vain attempt to help bad TCP stacks that cause problems
when they overflow
=================
*/
void CL_DumpPackets (void)
{
	while (NET_GetPacket (NS_CLIENT, &net_from, &net_message))
	{
		Msg ("dumnping a packet\n");
	}
}

void CL_ReadNetMessage( void )
{
	while (NET_GetPacket (NS_CLIENT, &net_from, &net_message))
	{
		// remote command packet
		if(*(int *)net_message.data == -1)
		{
			CL_ConnectionlessPacket();
			continue;
		}

		if( cls.state == ca_disconnected || cls.state == ca_connecting )
			continue;	// dump it if not connected

		if( net_message.cursize < 8 )
		{
			Msg( "%s: Runt packet\n", NET_AdrToString(net_from));
			continue;
		}

		// packet from server
		if (!NET_CompareAdr (net_from, cls.netchan.remote_address))
		{
			MsgWarn("CL_ReadPackets: %s:sequenced packet without connection\n",NET_AdrToString(net_from));
			continue;
		}
		if(!Netchan_Process(&cls.netchan, &net_message))
			continue;	// wasn't accepted for some reason
		CL_ParseServerMessage( &net_message );
	}
}

/*
=================
CL_ReadPackets
=================
*/
void CL_ReadPackets (void)
{
	if( cls.demoplayback )
		CL_ReadDemoMessage();
	else CL_ReadNetMessage();
          
	//
	// check timeout
	//
	if( cls.state >= ca_connected && !cls.demoplayback )
	{
		if( cls.realtime - cls.netchan.last_received > cl_timeout->value)
		{
			cl.timeoutcount++;	// timeoutcount saves debugger

			if( cl.timeoutcount > 5 )
			{
				Msg ("\nServer connection timed out.\n");
				CL_Disconnect ();
				return;
			}
		}
	}
	else cl.timeoutcount = 0;
	
}


//=============================================================================

/*
==============
CL_FixUpGender_f
==============
*/
void CL_FixUpGender(void)
{
	char *p;
	char sk[80];

	if (gender_auto->value) {

		if (gender->modified) {
			// was set directly, don't override the user
			gender->modified = false;
			return;
		}

		strncpy(sk, skin->string, sizeof(sk) - 1);
		if ((p = strchr(sk, '/')) != NULL)
			*p = 0;
		if (strcasecmp(sk, "male") == 0 || strcasecmp(sk, "cyborg") == 0)
			Cvar_Set ("gender", "male");
		else if (strcasecmp(sk, "female") == 0 || strcasecmp(sk, "crackhor") == 0)
			Cvar_Set ("gender", "female");
		else
			Cvar_Set ("gender", "none");
		gender->modified = false;
	}
}

/*
==============
CL_Userinfo_f
==============
*/
void CL_Userinfo_f (void)
{
	Msg ("User info settings:\n");
	Info_Print (Cvar_Userinfo());
}

int precache_check; // for autodownload of precache items
int precache_spawncount;
int precache_tex;
int precache_model_skin;

byte *precache_model; // used for skin checking in alias models

#define PLAYER_MULT 5

// ENV_CNT is map load, ENV_CNT+1 is first env map
#define ENV_CNT (CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
#define TEXTURE_CNT (ENV_CNT+13)

static const char *env_suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};

void CL_RequestNextDownload (void)
{
	uint		map_checksum;	// for detecting cheater maps
	char		fn[MAX_OSPATH];
	studiohdr_t	*pheader;

	if(cls.state != ca_connected)
		return;

	if (!allow_download->value && precache_check < ENV_CNT)
		precache_check = ENV_CNT;

//ZOID
	if (precache_check == CS_MODELS)
	{
		// confirm map
		precache_check = CS_MODELS+2; // 0 isn't used
		if (allow_download_maps->value)
			if (!CL_CheckOrDownloadFile(cl.configstrings[CS_MODELS+1]))
				return; // started a download
	}
	if (precache_check >= CS_MODELS && precache_check < CS_MODELS+MAX_MODELS)
	{
		if (allow_download_models->value)
		{
			while (precache_check < CS_MODELS+MAX_MODELS &&
				cl.configstrings[precache_check][0]) {
				if (cl.configstrings[precache_check][0] == '*' ||
					cl.configstrings[precache_check][0] == '#') {
					precache_check++;
					continue;
				}
				if (precache_model_skin == 0) {
					if (!CL_CheckOrDownloadFile(cl.configstrings[precache_check])) {
						precache_model_skin = 1;
						return; // started a download
					}
					precache_model_skin = 1;
				}

				// checking for skins in the model
				if (!precache_model) {

					precache_model = FS_LoadFile (cl.configstrings[precache_check], NULL);
					if (!precache_model)
					{
						precache_model_skin = 0;
						precache_check++;
						continue; // couldn't load it
					}
					if (LittleLong(*(uint *)precache_model) != IDSTUDIOHEADER)
					{
						// not an studio model
						precache_model = 0;
						precache_model_skin = 0;
						precache_check++;
						continue;
					}
					pheader = (studiohdr_t *)precache_model;
					if (LittleLong (pheader->version) != STUDIO_VERSION)
					{
						precache_check++;
						precache_model_skin = 0;
						continue; // couldn't load it
					}
				}

				pheader = (studiohdr_t *)precache_model;

				if (precache_model)
				{ 
					precache_model = 0;
				}
				precache_model_skin = 0;
				precache_check++;
			}
		}
		precache_check = CS_SOUNDS;
	}
	if (precache_check >= CS_SOUNDS && precache_check < CS_SOUNDS+MAX_SOUNDS)
	{ 
		if (allow_download_sounds->value)
		{
			if (precache_check == CS_SOUNDS) precache_check++; // zero is blank
			while (precache_check < CS_SOUNDS+MAX_SOUNDS && cl.configstrings[precache_check][0])
			{
				if (cl.configstrings[precache_check][0] == '*')
				{
					precache_check++;
					continue;
				}
				com.sprintf(fn, "sound/%s", cl.configstrings[precache_check++]);
				if (!CL_CheckOrDownloadFile(fn)) return; // started a download
			}
		}
		precache_check = CS_IMAGES;
	}
	if (precache_check >= CS_IMAGES && precache_check < CS_IMAGES+MAX_IMAGES)
	{
		if (precache_check == CS_IMAGES) precache_check++; // zero is blank
		while (precache_check < CS_IMAGES+MAX_IMAGES && cl.configstrings[precache_check][0])
		{
			com.sprintf(fn, "pics/%s.pcx", cl.configstrings[precache_check++]);
			if (!CL_CheckOrDownloadFile(fn)) return; // started a download
		}
		precache_check = CS_PLAYERSKINS;
	}
	// skins are special, since a player has three things to download:
	// model, weapon model and skin
	// so precache_check is now *3
	if (precache_check >= CS_PLAYERSKINS && precache_check < CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
	{
		if (allow_download_players->value)
		{
			while (precache_check < CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
			{
				int i, n;
				char model[MAX_QPATH], skin[MAX_QPATH], *p;

				i = (precache_check - CS_PLAYERSKINS)/PLAYER_MULT;
				n = (precache_check - CS_PLAYERSKINS)%PLAYER_MULT;

				if (!cl.configstrings[CS_PLAYERSKINS+i][0]) {
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
					continue;
				}

				if ((p = strchr(cl.configstrings[CS_PLAYERSKINS+i], '\\')) != NULL)
					p++;
				else p = cl.configstrings[CS_PLAYERSKINS+i];
				strcpy(model, p);
				p = strchr(model, '/');
				if (!p)
					p = strchr(model, '\\');
				if (p)
				{
					*p++ = 0;
					strcpy(skin, p);
				}
				else *skin = 0;

				switch (n)
				{
				case 0: // model
					com.sprintf(fn, "models/players/%s/player.mdl", model);
					if (!CL_CheckOrDownloadFile(fn))
					{
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 1;
						return; // started a download
					}
					n++;
					/*FALL THROUGH*/

				case 1: // weapon model
					com.sprintf(fn, "weapons/%s.mdl", model);
					if (!CL_CheckOrDownloadFile(fn))
					{
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 2;
						return; // started a download
					}
					n++;
					/*FALL THROUGH*/
				default:
					break;
				}
			}
		}
		// precache phase completed
		precache_check = ENV_CNT;
	}

	if (precache_check == ENV_CNT)
	{
		precache_check = ENV_CNT + 1;

		pe->BeginRegistration( cl.configstrings[CS_MODELS+1], true, &map_checksum );
		if( map_checksum != com.atoi(cl.configstrings[CS_MAPCHECKSUM]))
		{
			Host_Error("Local map version differs from server: %i != '%s'\n", map_checksum, cl.configstrings[CS_MAPCHECKSUM]);
			return;
		}
	}

	if (precache_check > ENV_CNT && precache_check < TEXTURE_CNT)
	{
		if (allow_download->value && allow_download_maps->value)
		{
			while (precache_check < TEXTURE_CNT)
			{
				int n = precache_check++ - ENV_CNT - 1;

				if (n & 1) com.sprintf(fn, "textures/env_skybox/%s.dds", cl.configstrings[CS_SKY] ); // cubemap pack
				else com.sprintf(fn, "textures/env_skybox/%s%s.tga", cl.configstrings[CS_SKY], env_suf[n/2]);
				if (!CL_CheckOrDownloadFile(fn)) return; // started a download
			}
		}
		precache_check = TEXTURE_CNT;
	}

	if (precache_check == TEXTURE_CNT)
	{
		precache_check = TEXTURE_CNT+1;
		precache_tex = 0;
	}

	// confirm existance of textures, download any that don't exist
	if (precache_check == TEXTURE_CNT+1)
	{
		if (allow_download->value && allow_download_maps->value)
		{
			while( precache_tex < pe->NumTextures())
			{
				char fn[MAX_OSPATH];

				com.sprintf(fn, "textures/%s.tga", pe->GetTextureName( precache_tex++ ));
				if(!CL_CheckOrDownloadFile(fn)) return; // started a download
			}
		}
		precache_check = TEXTURE_CNT+999;
	}

//ZOID
	CL_RegisterSounds();
	CL_PrepRefresh();

	if( cls.demoplayback ) return; // not really connected
	MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
	MSG_WriteString( &cls.netchan.message, va("begin %i\n", precache_spawncount));
}

/*
=================
CL_Precache_f

The server will send this command right
before allowing the client into the server
=================
*/
void CL_Precache_f (void)
{
	precache_check = CS_MODELS;
	precache_spawncount = com.atoi(Cmd_Argv(1));
	precache_model = 0;
	precache_model_skin = 0;

	CL_RequestNextDownload();
}


/*
=================
CL_InitLocal
=================
*/
void CL_InitLocal (void)
{
	cls.state = ca_disconnected;
	cls.realtime = 1.0f;
	CL_InitInput();

	adr0 = Cvar_Get( "adr0", "", CVAR_ARCHIVE );
	adr1 = Cvar_Get( "adr1", "", CVAR_ARCHIVE );
	adr2 = Cvar_Get( "adr2", "", CVAR_ARCHIVE );
	adr3 = Cvar_Get( "adr3", "", CVAR_ARCHIVE );
	adr4 = Cvar_Get( "adr4", "", CVAR_ARCHIVE );
	adr5 = Cvar_Get( "adr5", "", CVAR_ARCHIVE );
	adr6 = Cvar_Get( "adr6", "", CVAR_ARCHIVE );
	adr7 = Cvar_Get( "adr7", "", CVAR_ARCHIVE );
	adr8 = Cvar_Get( "adr8", "", CVAR_ARCHIVE );

	// register our variables
	cl_add_blend = Cvar_Get ("cl_blend", "1", 0);
	cl_add_lights = Cvar_Get ("cl_lights", "1", 0);
	cl_add_particles = Cvar_Get ("cl_particles", "1", 0);
	cl_add_entities = Cvar_Get ("cl_entities", "1", 0);
	cl_gun = Cvar_Get ("cl_gun", "1", 0);
	cl_footsteps = Cvar_Get ("cl_footsteps", "1", 0);
	cl_noskins = Cvar_Get ("cl_noskins", "0", 0);
	cl_autoskins = Cvar_Get ("cl_autoskins", "0", 0);
	cl_predict = Cvar_Get ("cl_predict", "1", 0);
//	cl_minfps = Cvar_Get ("cl_minfps", "5", 0);
	cl_maxfps = Cvar_Get ("cl_maxfps", "1000", 0);

	cl_upspeed = Cvar_Get ("cl_upspeed", "200", 0);
	cl_forwardspeed = Cvar_Get ("cl_forwardspeed", "200", 0);
	cl_sidespeed = Cvar_Get ("cl_sidespeed", "200", 0);
	cl_yawspeed = Cvar_Get ("cl_yawspeed", "140", 0);
	cl_pitchspeed = Cvar_Get ("cl_pitchspeed", "150", 0);
	cl_anglespeedkey = Cvar_Get ("cl_anglespeedkey", "1.5", 0);

	cl_run = Cvar_Get ("cl_run", "0", CVAR_ARCHIVE);
	freelook = Cvar_Get( "freelook", "0", CVAR_ARCHIVE );
	lookspring = Cvar_Get ("lookspring", "0", CVAR_ARCHIVE);
	lookstrafe = Cvar_Get ("lookstrafe", "0", CVAR_ARCHIVE);

	m_pitch = Cvar_Get ("m_pitch", "0.022", CVAR_ARCHIVE);
	m_yaw = Cvar_Get ("m_yaw", "0.022", 0);
	m_forward = Cvar_Get ("m_forward", "1", 0);
	m_side = Cvar_Get ("m_side", "1", 0);

	cl_shownet = Cvar_Get ("cl_shownet", "0", 0);
	cl_showmiss = Cvar_Get ("cl_showmiss", "0", 0);
	cl_showclamp = Cvar_Get ("showclamp", "0", 0);
	cl_timeout = Cvar_Get ("cl_timeout", "120", 0);

	rcon_client_password = Cvar_Get ("rcon_password", "", 0);
	rcon_address = Cvar_Get ("rcon_address", "", 0);

	cl_lightlevel = Cvar_Get ("r_lightlevel", "0", 0);

	//
	// userinfo
	//
	info_password = Cvar_Get ("password", "", CVAR_USERINFO);
	info_spectator = Cvar_Get ("spectator", "0", CVAR_USERINFO);
	name = Cvar_Get ("name", "unnamed", CVAR_USERINFO | CVAR_ARCHIVE);
	skin = Cvar_Get ("skin", "male/grunt", CVAR_USERINFO | CVAR_ARCHIVE);
	rate = Cvar_Get ("rate", "25000", CVAR_USERINFO | CVAR_ARCHIVE);	// FIXME
	hand = Cvar_Get ("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	fov = Cvar_Get ("fov", "90", CVAR_USERINFO | CVAR_ARCHIVE);
	gender = Cvar_Get ("gender", "male", CVAR_USERINFO | CVAR_ARCHIVE);
	gender_auto = Cvar_Get ("gender_auto", "1", CVAR_ARCHIVE);
	gender->modified = false; // clear this so we know when user sets it manually
	cl_vwep = Cvar_Get ("cl_vwep", "1", CVAR_ARCHIVE);
	cl_showfps = Cvar_Get ("cl_showfps", "1", CVAR_ARCHIVE );

	// register our commands
	Cmd_AddCommand ("cmd", CL_ForwardToServer_f, "send a console commandline to the server" );
	Cmd_AddCommand ("pause", CL_Pause_f, "pause the game (if the server allows pausing)" );
	Cmd_AddCommand ("pingservers", CL_PingServers_f, "send a broadcast packet" );

	Cmd_AddCommand ("userinfo", CL_Userinfo_f, "print current client userinfo" );
	Cmd_AddCommand ("changing", CL_Changing_f, "sent by server to tell client to wait for level change" );
	Cmd_AddCommand ("disconnect", CL_Disconnect_f, "disconnect from server" );
	Cmd_AddCommand ("record", CL_Record_f, "record a demo" );
	Cmd_AddCommand ("playdemo", CL_PlayDemo_f, "playing a demo" );
	Cmd_AddCommand ("stop", CL_Stop_f, "stop playing or recording a demo" );

	Cmd_AddCommand ("quit", CL_Quit_f, "quit from game" );
	Cmd_AddCommand ("exit", CL_Quit_f, "quit from game" );

	Cmd_AddCommand ("screenshot", CL_ScreenShot_f, "takes a screenshot of the next rendered frame" );
	Cmd_AddCommand ("levelshot", CL_LevelShot_f, "same as \"screenshot\", used for create plaque images" );

	Cmd_AddCommand ("connect", CL_Connect_f, "connect to a server by hostname" );
	Cmd_AddCommand ("reconnect", CL_Reconnect_f, "reconnect to current level" );

	Cmd_AddCommand ("rcon", CL_Rcon_f, "sends a command to the server console (rcon_password and rcon_address required)" );

	// this is dangerous to leave in
// 	Cmd_AddCommand ("packet", CL_Packet_f, "send a packet with custom contents" );

	Cmd_AddCommand ("precache", CL_Precache_f, "precache specified resource (by index)" );
	Cmd_AddCommand ("download", CL_Download_f, "download specified resource (by name)" );

	//
	// forward to server commands
	//
	// the only thing this does is allow command completion
	// to work -- all unknown commands are automatically
	// forwarded to the server
	Cmd_AddCommand ("wave", NULL, NULL );
	Cmd_AddCommand ("inven", NULL, NULL );
	Cmd_AddCommand ("kill", NULL, NULL );
	Cmd_AddCommand ("use", NULL, NULL );
	Cmd_AddCommand ("drop", NULL, NULL );
	Cmd_AddCommand ("say", NULL, NULL );
	Cmd_AddCommand ("say_team", NULL, NULL );
	Cmd_AddCommand ("info", NULL, NULL );
	Cmd_AddCommand ("prog", NULL, NULL );
	Cmd_AddCommand ("give", NULL, NULL );
	Cmd_AddCommand ("god", NULL, NULL );
	Cmd_AddCommand ("notarget", NULL, NULL );
	Cmd_AddCommand ("noclip", NULL, NULL );
	Cmd_AddCommand ("invuse", NULL, NULL );
	Cmd_AddCommand ("invprev", NULL, NULL );
	Cmd_AddCommand ("invnext", NULL, NULL );
	Cmd_AddCommand ("invdrop", NULL, NULL );
	Cmd_AddCommand ("weapnext", NULL, NULL );
	Cmd_AddCommand ("weapprev", NULL, NULL );
}



/*
===============
CL_WriteConfiguration

Writes key bindings and archived cvars to config.cfg
===============
*/
void CL_WriteConfiguration (void)
{
	file_t	*f;

	if (cls.state == ca_uninitialized) return;

	f = FS_Open("scripts/config/keys.rc", "w");
	if(f)
	{
		FS_Printf (f, "//=======================================================================\n");
		FS_Printf (f, "//\t\t\tCopyright XashXT Group 2007 ©\n");
		FS_Printf (f, "//\t\t\tkeys.rc - current key bindings\n");
		FS_Printf (f, "//=======================================================================\n");
		Key_WriteBindings(f);
		FS_Close (f);
	}
	else MsgWarn("Couldn't write keys.rc.\n");

	f = FS_Open("scripts/config/vars.rc", "w");
	if(f)
	{
		FS_Printf (f, "//=======================================================================\n");
		FS_Printf (f, "//\t\t\tCopyright XashXT Group 2007 ©\n");
		FS_Printf (f, "//\t\t\tvars.rc - archive of cvars\n");
		FS_Printf (f, "//=======================================================================\n");
		Cmd_WriteVariables(f);
		FS_Close (f);	
	}
	else MsgWarn("Couldn't write vars.rc.\n");
}


/*
==================
CL_FixCvarCheats

==================
*/

typedef struct
{
	char	*name;
	char	*value;
	cvar_t	*var;
} cheatvar_t;

cheatvar_t	cheatvars[] = {
	{"timescale", "1"},
	{"r_drawworld", "1"},
	{"cl_testlights", "0"},
	{"r_fullbright", "0"},
	{"r_drawflat", "0"},
	{"paused", "0"},
	{"fixedtime", "0"},
	{"sw_draworder", "0"},
	{"gl_lightmap", "0"},
	{"gl_saturatelighting", "0"},
	{NULL, NULL}
};

int		numcheatvars;

void CL_FixCvarCheats (void)
{
	int			i;
	cheatvar_t	*var;

	if ( !strcmp(cl.configstrings[CS_MAXCLIENTS], "1") 
		|| !cl.configstrings[CS_MAXCLIENTS][0] )
		return;		// single player can cheat

	// find all the cvars if we haven't done it yet
	if (!numcheatvars)
	{
		while (cheatvars[numcheatvars].name)
		{
			cheatvars[numcheatvars].var = Cvar_Get (cheatvars[numcheatvars].name,
					cheatvars[numcheatvars].value, 0);
			numcheatvars++;
		}
	}

	// make sure they are all set to the proper values
	for (i=0, var = cheatvars ; i<numcheatvars ; i++, var++)
	{
		if ( strcmp (var->var->string, var->value) )
		{
			Cvar_Set (var->name, var->value);
		}
	}
}

//============================================================================

/*
==================
CL_SendCommand

==================
*/
void CL_SendCommand (void)
{
	// get new key events
	Sys_SendKeyEvents ();

	// process console commands
	Cbuf_Execute ();

	// fix any cheating cvars
	CL_FixCvarCheats ();

	// send intentions now
	CL_SendCmd ();

	// resend a connection request if necessary
	CL_CheckForResend ();
}

/*
==================
CL_Frame

==================
*/
void CL_Frame( float time )
{
	static float	extratime;
	static float  	lasttimecalled;

	if( dedicated->integer ) return;

	extratime += time;

	if (cls.state == ca_connected && extratime < 0.01f)
		return;	// don't flood packets out while connecting
	if (extratime < 1.0f / cl_maxfps->value)
		return;	// framerate is too high

	// let the mouse activate or deactivate
	CL_UpdateMouse();

	// decide the simulation time
	cls.frametime = extratime;
	cl.time += extratime;
	cls.realtime = host.realtime;
	extratime = 0;

	// if in the debugger last frame, don't timeout
	if (time > 5.0f) cls.netchan.last_received = host.realtime;

	// fetch results from server
	CL_ReadPackets();

	// send a new command message to the server
	CL_SendCommand();

	// predict all unacknowledged movements
	CL_PredictMovement ();

	// allow rendering DLL change
	if(!cl.refresh_prepped && cls.state == ca_active)
		CL_PrepRefresh();

	// update the screen
	SCR_UpdateScreen();

	// update audio
	S_Update( cl.playernum + 1, cl.refdef.vieworg, vec3_origin, cl.v_forward, cl.v_up );

	// advance local effects for next frame
	CL_RunDLights ();
	CL_RunLightStyles ();

	SCR_RunCinematic();
	Con_RunConsole();

	cls.framecount++;
}


//============================================================================

/*
====================
CL_Init
====================
*/
void CL_Init( void )
{
	if (dedicated->value) return; // nothing running on the client

	// all archived variables will now be loaded
	scr_loading = Cvar_Get("scr_loading", "0", 0 );
	cl_paused = Cvar_Get( "paused", "0", 0 );

	Con_Init();	
	VID_Init();
	V_Init();
	CL_InitClientProgs();
	CG_Init();

	net_message.data = net_message_buffer;
	net_message.maxsize = sizeof(net_message_buffer);

	M_Init ();	
	
	SCR_Init();
	CL_InitLocal();
}


/*
===============
CL_Shutdown

FIXME: this is a callback from Sys_Quit and Host_Error.  It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void CL_Shutdown(void)
{
	// already freed
	if(host.state == HOST_ERROR) return;

	CL_WriteConfiguration(); 
	CL_FreeClientProgs();
	S_Shutdown();
	CL_ShutdownInput();
}


