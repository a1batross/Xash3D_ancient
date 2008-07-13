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
// cl_parse.c  -- parse a message received from the server

#include "common.h"
#include "client.h"

//=============================================================================

void CL_DownloadFileName(char *dest, int destlen, char *fn)
{
	strncpy(dest, fn, destlen );
}

/*
===============
CL_CheckOrDownloadFile

Returns true if the file exists, otherwise it attempts
to start a download from the server.
===============
*/
bool	CL_CheckOrDownloadFile (char *filename)
{
	file_t	*fp;
	char	name[MAX_SYSPATH];

	if (strstr (filename, ".."))
	{
		Msg ("Refusing to download a path with ..\n");
		return true;
	}

	if (FS_LoadFile (filename, NULL))
	{
		// it exists, no need to download
		return true;
	}

	strcpy (cls.downloadname, filename);
	strcpy (cls.downloadtempname, filename);

	// download to a temp name, and only rename
	// to the real name when done, so if interrupted
	// a runt file wont be left
	FS_StripExtension (cls.downloadtempname);
	FS_DefaultExtension(cls.downloadtempname, ".tmp");

//ZOID
	// check to see if we already have a tmp for this file, if so, try to resume
	// open the file if not opened yet
	CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);


	fp = FS_Open(name, "r+b");
	if (fp)
	{
		// it exists
		int len;
		FS_Seek(fp, 0, SEEK_END);
		len = FS_Tell(fp);

		cls.download = fp;

		// give the server an offset to start the download
		Msg ("Resuming %s\n", cls.downloadname);
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va("download %s %i", cls.downloadname, len));
	}
	else
	{
		Msg ("Downloading %s\n", cls.downloadname);
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va("download %s", cls.downloadname));
	}

	cls.downloadnumber++;

	return false;
}

/*
===============
CL_Download_f

Request a download from the server
===============
*/
void	CL_Download_f (void)
{
	char filename[MAX_OSPATH];

	if (Cmd_Argc() != 2)
	{
		Msg("Usage: download <filename>\n");
		return;
	}

	com.sprintf(filename, "%s", Cmd_Argv(1));

	if (strstr (filename, ".."))
	{
		Msg ("Refusing to download a path with ..\n");
		return;
	}

	if (FS_LoadFile (filename, NULL))
	{
		// it exists, no need to download
		Msg("File already exists.\n");
		return;
	}

	strcpy (cls.downloadname, filename);
	strcpy (cls.downloadtempname, filename);

	Msg ("Downloading %s\n", cls.downloadname);

	// download to a temp name, and only rename
	// to the real name when done, so if interrupted
	// a runt file wont be left


	// download to a temp name, and only rename
	// to the real name when done, so if interrupted
	// a runt file wont be left
	FS_StripExtension (cls.downloadtempname);
	FS_DefaultExtension(cls.downloadtempname, ".tmp");

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message,
		va("download %s", cls.downloadname));

	cls.downloadnumber++;
}

/*
======================
CL_RegisterSounds
======================
*/
void CL_RegisterSounds (void)
{
	int	i;

	S_BeginRegistration();
	for (i = 1; i < MAX_SOUNDS; i++)
	{
		if (!cl.configstrings[CS_SOUNDS+i][0]) break;
		cl.sound_precache[i] = S_RegisterSound (cl.configstrings[CS_SOUNDS+i]);
		Sys_SendKeyEvents (); // pump message loop
	}
	S_EndRegistration();
}


/*
=====================
CL_ParseDownload

A download message has been received from the server
=====================
*/
void CL_ParseDownload( sizebuf_t *msg )
{
	int		size, percent;
	string		name;
	int		r;

	// read the data
	size = MSG_ReadShort( msg );
	percent = MSG_ReadByte( msg );
	if (size == -1)
	{
		Msg("Server does not have this file.\n");
		if (cls.download)
		{
			// if here, we tried to resume a file but the server said no
			FS_Close(cls.download);
			cls.download = NULL;
		}
		CL_RequestNextDownload ();
		return;
	}

	// open the file if not opened yet
	if (!cls.download)
	{
		CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);

		cls.download = FS_Open (name, "wb");
		if (!cls.download)
		{
			msg->readcount += size;
			Msg ("Failed to open %s\n", cls.downloadtempname);
			CL_RequestNextDownload ();
			return;
		}
	}

	FS_Write (cls.download, msg->data + msg->readcount, size );
	msg->readcount += size;

	if (percent != 100)
	{
		// request next block
		cls.downloadpercent = percent;

		MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
		MSG_WriteString(&cls.netchan.message, "nextdl");
	}
	else
	{
		char	oldn[MAX_OSPATH];
		char	newn[MAX_OSPATH];

		FS_Close (cls.download);

		// rename the temp file to it's final name
		CL_DownloadFileName(oldn, sizeof(oldn), cls.downloadtempname);
		CL_DownloadFileName(newn, sizeof(newn), cls.downloadname);
		r = rename (oldn, newn);
		if (r)
			Msg ("failed to rename.\n");

		cls.download = NULL;
		cls.downloadpercent = 0;

		// get another file if needed

		CL_RequestNextDownload ();
	}
}


/*
=====================================================================

  SERVER CONNECTING MESSAGES

=====================================================================
*/

/*
==================
CL_ParseServerData
==================
*/
void CL_ParseServerData( sizebuf_t *msg )
{
	char		*str;
	int		i;

	MsgDev(D_INFO, "Serverdata packet received.\n");

	// wipe the client_t struct
	CL_ClearState();
	cls.state = ca_connected;

	// parse protocol version number
	i = MSG_ReadLong( msg );
	cls.serverProtocol = i;

	if( i != PROTOCOL_VERSION ) Host_Error("Server returned version %i, not %i", i, PROTOCOL_VERSION );

	cl.servercount = MSG_ReadLong( msg );

	// parse player entity number
	cl.playernum = MSG_ReadShort( msg );

	// get the full level name
	str = MSG_ReadString( msg );

	if( cl.playernum == -1 )
	{	
		// playing a cinematic or showing a pic, not a level
		SCR_PlayCinematic( str, 0 );
	}
	else
	{
		// get splash name
		Cvar_Set( "cl_levelshot_name", va("background/%s.tga", str ));
		Cvar_SetValue("scr_loading", 0.0f ); // reset progress bar
		if(!FS_FileExists(va("gfx/%s", Cvar_VariableString("cl_levelshot_name")))) 
		{
			Cvar_Set("cl_levelshot_name", "common/black");
			cl.make_levelshot = true; // make levelshot
		}
		// seperate the printfs so the server message can have a color
		Msg("\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");
		// need to prep refresh at next oportunity
		cl.refresh_prepped = false;
	}
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline( sizebuf_t *msg )
{
	int		bits;
	int		newnum;
	entity_state_t	nullstate;
	edict_t		*ent;

	memset (&nullstate, 0, sizeof(nullstate));
	newnum = CL_ParseEntityBits( msg, &bits );

	// allocate edicts
	while( newnum >= prog->num_edicts ) PRVM_ED_Alloc();
	ent = PRVM_EDICT_NUM( newnum );

	MSG_ReadDeltaEntity( msg, &nullstate, &ent->priv.cl->baseline, newnum );
}


/*
================
CL_LoadClientinfo

================
*/
void CL_LoadClientinfo (clientinfo_t *ci, char *s)
{
	int i;
	char		*t;
	char		model_name[MAX_QPATH];
	char		skin_name[MAX_QPATH];
	char		model_filename[MAX_QPATH];
	char		weapon_filename[MAX_QPATH];

	strncpy(ci->cinfo, s, sizeof(ci->cinfo));
	ci->cinfo[sizeof(ci->cinfo)-1] = 0;

	// isolate the player's name
	strncpy(ci->name, s, sizeof(ci->name));
	ci->name[sizeof(ci->name)-1] = 0;
	t = strstr (s, "\\");
	if (t)
	{
		ci->name[t-s] = 0;
		s = t+1;
	}

	if( *s == 0)
	{
		com.sprintf (model_filename, "models/players/gordon/player.mdl");
		com.sprintf (weapon_filename, "models/weapons/w_glock.mdl");
		ci->model = re->RegisterModel (model_filename);
		memset(ci->weaponmodel, 0, sizeof(ci->weaponmodel));
		ci->weaponmodel[0] = re->RegisterModel (weapon_filename);
	}
	else
	{
		// isolate the model name
		com.strcpy (model_name, s);
		t = strstr(model_name, "/");
		if (!t) t = strstr(model_name, "\\");
		if (!t) t = model_name;
		*t = 0;

		// isolate the skin name
		strcpy (skin_name, s + strlen(model_name) + 1);

		// model file
		com.sprintf (model_filename, "models/players/%s/player.mdl", model_name);
		ci->model = re->RegisterModel (model_filename);
		if (!ci->model)
		{
			com.strcpy(model_name, "gordon");
			com.sprintf (model_filename, "models/players/gordon/player.mdl");
			ci->model = re->RegisterModel (model_filename);
		}

		// if we don't have the skin and the model wasn't male,
		// see if the male has it (this is for CTF's skins)
 		if (!ci->skin && strcasecmp(model_name, "male"))
		{
			// change model to male
			com.strcpy(model_name, "male");
			com.sprintf (model_filename, "models/players/gordon/player.mdl");
			ci->model = re->RegisterModel (model_filename);
		}

		// weapon file
		for (i = 0; i < num_cl_weaponmodels; i++)
		{
			com.sprintf (weapon_filename, "models/weapons/%s", cl_weaponmodels[i]);
			ci->weaponmodel[i] = re->RegisterModel(weapon_filename);
			if (!cl_vwep->value) break; // only one when vwep is off
		}
	}

	// must have loaded all data types to be valud
	if (!ci->skin || !ci->model || !ci->weaponmodel[0])
	{
		ci->skin = NULL;
		ci->model = NULL;
		ci->weaponmodel[0] = NULL;
		return;
	}
}

/*
================
CL_ParseClientinfo

Load the skin, icon, and model for a client
================
*/
void CL_ParseClientinfo (int player)
{
	char			*s;
	clientinfo_t	*ci;

	s = cl.configstrings[player+CS_PLAYERSKINS];

	ci = &cl.clientinfo[player];

	CL_LoadClientinfo (ci, s);
}


/*
================
CL_ParseConfigString
================
*/
void CL_ParseConfigString( sizebuf_t *msg )
{
	int		i;
	char		*s;
	string		olds;

	i = MSG_ReadShort( msg );
	if (i < 0 || i >= MAX_CONFIGSTRINGS) Host_Error("configstring > MAX_CONFIGSTRINGS\n");
	s = MSG_ReadString( msg );

	strncpy (olds, cl.configstrings[i], sizeof(olds));
	olds[sizeof(olds) - 1] = 0;

	strcpy (cl.configstrings[i], s);

	// do something apropriate 

	if (i >= CS_LIGHTS && i < CS_LIGHTS+MAX_LIGHTSTYLES)
	{
		CL_SetLightstyle (i - CS_LIGHTS);
	}
	else if (i >= CS_MODELS && i < CS_MODELS+MAX_MODELS)
	{
		if(cl.refresh_prepped)
		{
			cl.model_draw[i-CS_MODELS] = re->RegisterModel (cl.configstrings[i]);
			if (cl.configstrings[i][0] == '*')
				cl.model_clip[i-CS_MODELS] = pe->RegisterModel(cl.configstrings[i] );
			else cl.model_clip[i-CS_MODELS] = NULL;
		}
	}
	else if (i >= CS_SOUNDS && i < CS_SOUNDS+MAX_MODELS)
	{
		if (cl.refresh_prepped)
			cl.sound_precache[i-CS_SOUNDS] = S_RegisterSound (cl.configstrings[i]);
	}
	else if (i >= CS_IMAGES && i < CS_IMAGES+MAX_MODELS)
	{
		if (cl.refresh_prepped)
			cl.image_precache[i-CS_IMAGES] = re->RegisterPic (cl.configstrings[i]);
	}
	else if (i >= CS_PLAYERSKINS && i < CS_PLAYERSKINS+MAX_CLIENTS)
	{
		if (cl.refresh_prepped && strcmp(olds, s))
			CL_ParseClientinfo (i-CS_PLAYERSKINS);
	}
}


/*
=====================================================================

ACTION MESSAGES

=====================================================================
*/

/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket( sizebuf_t *msg )
{
	vec3_t	pos_v;
	float	*pos;
	int 	channel, ent;
	int 	sound_num;
	float 	volume;
	float 	attenuation;  
	int	flags;
	float	ofs;

	flags = MSG_ReadByte( msg );
	sound_num = MSG_ReadByte( msg );

	if (flags & SND_VOLUME) volume = MSG_ReadByte( msg ) / 255.0;
	else volume = DEFAULT_SOUND_PACKET_VOL;
	
	if (flags & SND_ATTENUATION) attenuation = MSG_ReadByte( msg ) / 64.0;
	else attenuation = DEFAULT_SOUND_PACKET_ATTN;	

	if (flags & SND_OFFSET) ofs = MSG_ReadByte( msg ) / 1000.0;
	else ofs = 0;

	if (flags & SND_ENT)
	{	
		// entity reletive
		channel = MSG_ReadShort( msg ); 
		ent = channel>>3;
		if (ent > MAX_EDICTS) Host_Error("CL_ParseStartSoundPacket: ent out of range\n" );
		channel &= 7;
	}
	else
	{
		ent = 0;
		channel = 0;
	}

	if (flags & SND_POS)
	{	
		// positioned in space
		MSG_ReadPos32( msg, pos_v);
		pos = pos_v;
	}
	else pos = NULL; // use entity number

	if (!cl.sound_precache[sound_num]) return;
	S_StartSound (pos, ent, channel, cl.sound_precache[sound_num]);
}       

void CL_ParseAmbientSound( sizebuf_t *msg )
{
	sound_t		loopSoundHandle;
	int		entityNum, soundNum;
	vec3_t		ambient_org;

	entityNum = MSG_ReadShort( msg );
	soundNum = MSG_ReadShort( msg );
	MSG_ReadPos32( msg, ambient_org);	
	loopSoundHandle = S_RegisterSound( cl.configstrings[CS_SOUNDS + soundNum] );

	// add ambient looping sound
	// S_AddRealLoopingSound( entityNum, ambient_org, vec3_origin, loopSoundHandle );
}

void SHOWNET( sizebuf_t *msg, char *s )
{
	if (cl_shownet->value >= 2) Msg ("%3i:%s\n", msg->readcount-1, s);
}

/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage( sizebuf_t *msg )
{
	char	*s;
	int	cmd;

	// if recording demos, copy the message out
	if (cl_shownet->value == 1) Msg ("%i ",msg->cursize);
	else if (cl_shownet->value >= 2) Msg ("------------------\n");

	MSG_UseHuffman( msg, true );
	cls.multicast = msg; // client progs can recivied messages too

	// parse the message
	while( 1 )
	{
		if( msg->readcount > msg->cursize )
		{
			Host_Error("CL_ParseServerMessage: Bad server message\n");
			break;
		}

		cmd = MSG_ReadByte( msg );
		if( cmd == -1 ) break;
	
		// other commands
		switch( cmd )
		{
		case svc_nop:
			MsgDev( D_ERROR, "CL_ParseServerMessage: user message out of bounds\n" );
			break;
		case svc_disconnect:
			CL_Drop ();
			Host_AbortCurrentFrame();
			break;
		case svc_reconnect:
			Msg( "Server disconnected, reconnecting\n" );
			if( cls.download )
			{
				FS_Close( cls.download );
				cls.download = NULL;
			}
			cls.state = ca_connecting;
			cls.connect_time = MAX_HEARTBEAT; // CL_CheckForResend() will fire immediately
			break;
		case svc_stufftext:
			s = MSG_ReadString( msg );
			Msg("CL<-svc_stufftext %s\n", s );
			Cbuf_AddText( s );
			break;
		case svc_serverdata:
			Cbuf_Execute();		// make sure any stuffed commands are done
			CL_ParseServerData( msg );
			break;
		case svc_configstring:
			CL_ParseConfigString( msg );
			break;
		case svc_sound:
			CL_ParseStartSoundPacket( msg );
			break;
		case svc_ambientsound:
			CL_ParseAmbientSound( msg );
			break;
		case svc_spawnbaseline:
			CL_ParseBaseline( msg );
			break;
		case svc_temp_entity:
			CL_ParseTempEnts( msg );
			break;
		case svc_download:
			CL_ParseDownload( msg );
			break;
		case svc_frame:
			CL_ParseFrame( msg );
			break;
		case svc_playerinfo:
		case svc_packetentities:
		case svc_deltapacketentities:
			Host_Error("CL_ParseServerMessage: out of place frame data\n");
			break;
		case svc_bad:
			Host_Error("CL_ParseServerMessage: svc_bad\n" );
			break;
		default:
			// parse user messages
			if(!CL_ParseUserMessage( cmd ))
				Host_Error("CL_ParseServerMessage: illegible server message %d\n", cmd );
			break;
		}
	}
}