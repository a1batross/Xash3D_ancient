//=======================================================================
//			Copyright XashXT Group 2007 �
//		       cl_demo.c - demo record & playback
//=======================================================================

#include "common.h"
#include "client.h"

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length
====================
*/
void CL_WriteDemoMessage( void )
{
	int len, swlen;

	if( !cls.demofile ) return;
	if( cl_paused->value ) return;

	// the first eight bytes are just packet sequencing stuff
	len = net_message.cursize - 8;
	swlen = LittleLong(len);

	FS_Write( cls.demofile, &swlen, 4);
	FS_Write( cls.demofile, net_message.data + 8, len );
}

void CL_WriteDemoHeader( const char *name )
{
	char		buf_data[MAX_MSGLEN];
	entity_state_t	*state, nullstate;
	edict_t		*ent;
	sizebuf_t		buf;
	int		i, len;

	MsgDev(D_INFO, "recording to %s.\n", name );
	cls.demofile = FS_Open( name, "wb" );
	if( !cls.demofile )
	{
		MsgDev(D_ERROR, "CL_Record: unable to create %s\n", name );
		return;
	}

	cls.demorecording = true;

	// don't start saving messages until a non-delta compressed message is received
	cls.demowaiting = true;

	// write out messages to hold the startup information
	SZ_Init( &buf, buf_data, sizeof(buf_data));

	// send the serverdata
	MSG_WriteByte( &buf, svc_serverdata );
	MSG_WriteLong( &buf, PROTOCOL_VERSION );
	MSG_WriteLong( &buf, 0x10000 + cl.servercount );
	MSG_WriteShort( &buf, cl.playernum );
	MSG_WriteString( &buf, cl.configstrings[CS_NAME] );

	// configstrings
	for( i = 0; i < MAX_CONFIGSTRINGS; i++ )
	{
		if( cl.configstrings[i][0] )
		{
			if( buf.cursize + com.strlen(cl.configstrings[i]) + 32 > buf.maxsize )
			{	
				// write it out
				len = LittleLong (buf.cursize);
				FS_Write( cls.demofile, &len, 4 );
				FS_Write( cls.demofile, buf.data, buf.cursize );
				buf.cursize = 0;
			}
			MSG_WriteByte( &buf, svc_configstring );
			MSG_WriteShort( &buf, i);
			MSG_WriteString( &buf, cl.configstrings[i] );
		}

	}

	CL_VM_Begin();

	// baselines
	memset(&nullstate, 0, sizeof(nullstate));

	for( i = 0; i < prog->num_edicts; i++ )
	{
		ent = PRVM_EDICT_NUM( i );
		state = &ent->priv.cl->baseline;
		if(!state->modelindex) continue;

		if( buf.cursize + 64 > buf.maxsize )
		{	
			// write it out
			len = LittleLong (buf.cursize);
			FS_Write( cls.demofile, &len, 4 );
			FS_Write( cls.demofile, buf.data, buf.cursize );
			buf.cursize = 0;
		}
		MSG_WriteByte( &buf, svc_spawnbaseline );		
		MSG_WriteDeltaEntity (&nullstate, &ent->priv.cl->baseline, &buf, true, true);
	}

	MSG_WriteByte( &buf, svc_stufftext );
	MSG_WriteString( &buf, "precache\n");

	// write it to the demo file
	len = LittleLong( buf.cursize );
	FS_Write( cls.demofile, &len, 4 );
	FS_Write( cls.demofile, buf.data, buf.cursize );

	CL_VM_End();
}

/*
=======================================================================

CLIENT SIDE DEMO PLAYBACK

=======================================================================
*/

/*
==================
CL_NextDemo

Called when a demo or cinematic finishes
If the "nextdemo" cvar is set, that command will be issued
==================
*/
void CL_NextDemo( void )
{
	string	v;

	com.strncpy( v, Cvar_VariableString( "playdemo" ), sizeof(v));

	MsgDev(D_INFO, "CL_NextDemo: %s\n", v );
	if(!v[0]) return;

	Cvar_Set( "nextdemo","" );
	Cbuf_AddText(va("%s\n", v));
	Cbuf_Execute();
}

/*
=================
CL_DemoCompleted
=================
*/
void CL_DemoCompleted( void )
{
	CL_StopPlayback();
	CL_NextDemo();
}

/*
=================
CL_ReadDemoMessage

reads demo data and write it to client
=================
*/
void CL_ReadDemoMessage( void )
{
	sizebuf_t		buf;
	char		bufData[MAX_MSGLEN];
	int		r;

	if( !cls.demofile )
	{
		CL_DemoCompleted();
		return;
	}
	if( cl_paused->value ) return;

	// init the message
	SZ_Init( &buf, bufData, sizeof( bufData ));

	// get the length
	r = FS_Read( cls.demofile, &buf.cursize, 4 );
	if( r != 4 )
	{
		CL_DemoCompleted();
		return;
	}

	buf.cursize = LittleLong( buf.cursize );
	if( buf.cursize == -1 )
	{
		CL_DemoCompleted();
		return;
	}

	if( buf.cursize > buf.maxsize )
	{
		Host_Error( "CL_ReadDemoMessage: demoMsglen > MAX_MSGLEN\n" );
	}
	r = FS_Read( cls.demofile, buf.data, buf.cursize );

	if( r != buf.cursize )
	{
		MsgDev( D_ERROR, "CL_ReadDemoMessage: demo file was truncated\n");
		CL_DemoCompleted();
		return;
	}

	//cl.time = cls.realtime;//CHECK VALID
	buf.readcount = 0;
	CL_ParseServerMessage( &buf );
}

/*
==============
CL_StopPlayback

Called when a demo file runs out, or the user starts a game
==============
*/
void CL_StopPlayback( void )
{
	if( !cls.demoplayback ) return;

	// release demofile
	FS_Close( cls.demofile );
	cls.demoplayback = false;
	cls.demofile = NULL;

	// let game known about movie state	
	cls.state = ca_disconnected;
}

void CL_StopRecord( void )
{
	int	len = -1;

	if (!cls.demorecording) return;

	// finish up
	FS_Write( cls.demofile, &len, 4 );
	FS_Close( cls.demofile );
	cls.demofile = NULL;
	cls.demorecording = false;
}

/*
====================
CL_Record_f

record <demoname>
Begins recording a demo from the current position
====================
*/
void CL_Record_f( void )
{
	string		name;

	if( Cmd_Argc() != 2 )
	{
		Msg ("record <demoname>\n");
		return;
	}

	if( cls.demorecording )
	{
		Msg("CL_Record: already recording.\n");
		return;
	}

	if( cls.state != ca_active )
	{
		Msg ("You must be in a level to record.\n");
		return;
	}

	// open the demo file
	com.sprintf (name, "demos/%s.dem", Cmd_Argv(1));

	CL_WriteDemoHeader( name );
}

/*
====================
CL_PlayDemo_f

playdemo <demoname>
====================
*/
void CL_PlayDemo_f( void )
{
	string	filename;

	if(Cmd_Argc() != 2)
	{
		Msg( "playdemo <demoname>\n" );
		return;
	}

	// shutdown any game or cinematic server
	CL_Disconnect();
	Cbuf_ExecuteText(EXEC_APPEND, "killserver\n");

	com.snprintf( filename, MAX_STRING, "demos/%s.dem", Cmd_Argv(1));
	if(!FS_FileExists( filename ))
	{
		Msg("Can't loading %s\n", filename );
		return;
	}

	cls.demofile = FS_Open( filename, "rb" );
	com.strncpy( cls.demoname, Cmd_Argv(1), sizeof(cls.demoname));

	Con_Close();

	cls.state = ca_connected;
	cls.demoplayback = true;
	com.strncpy( cls.servername, Cmd_Argv(1), sizeof(cls.servername));

	// begin a playback demo
}

/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void CL_Stop_f( void )
{
	// stop all
	CL_StopRecord();
	CL_StopPlayback();
}

