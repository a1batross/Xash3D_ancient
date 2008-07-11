//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cl_input.c - movement commands
//=======================================================================

#include "common.h"
#include "client.h"

typedef struct
{
	int	down[2];		// key nums holding it down
	uint	downtime;		// msec timestamp
	uint	msec;		// msec down this frame
	bool	active;		// button is active ?
	bool	was_pressed;	// set when down, not cleared when up
} kbutton_t;

cvar_t	*cl_run;
cvar_t	*cl_upspeed;
cvar_t	*cl_yawspeed;
cvar_t	*cl_sidespeed;
cvar_t	*cl_pitchspeed;
cvar_t	*cl_forwardspeed;
cvar_t	*cl_anglespeedkey;
cvar_t	*cl_nodelta;
cvar_t	*v_centermove;
cvar_t	*v_centerspeed;
cvar_t	*cl_mouseaccel;
cvar_t	*cl_sensitivity;
cvar_t	*ui_sensitivity;
cvar_t	*m_filter;		// mouse filtering
uint	frame_msec;
int	in_impulse;

kbutton_t	in_left, in_right, in_forward, in_back;
kbutton_t	in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t	in_strafe, in_speed, in_use, in_attack, in_attack2;
kbutton_t	in_up, in_down;

/*
============================================================

	MOUSE EVENTS

============================================================
*/
/*
=================
CL_MouseEvent
=================
*/
void CL_MouseEvent( int mx, int my, int time )
{
	cl.mouse_x[cl.mouse_step] += mx;
	cl.mouse_y[cl.mouse_step] += my;
}

/*
=================
CL_MouseMove
=================
*/
void CL_MouseMove( usercmd_t *cmd )
{
	float	mx, my, rate;
	float	accel_sensitivity;

	// allow mouse smoothing
	if( m_filter->integer )
	{
		mx = (cl.mouse_x[0] + cl.mouse_x[1]) * 0.5;
		my = (cl.mouse_y[0] + cl.mouse_y[1]) * 0.5;
	}
	else
	{
		mx = cl.mouse_x[cl.mouse_step];
		my = cl.mouse_y[cl.mouse_step];
	}
	cl.mouse_step ^= 1;
	cl.mouse_x[cl.mouse_step] = 0;
	cl.mouse_y[cl.mouse_step] = 0;

	rate = sqrt( mx * mx + my * my ) / (float)frame_msec;

	if( cls.key_dest != key_menu )
	{
		accel_sensitivity = cl_sensitivity->value + rate * cl_mouseaccel->value;
//accel_sensitivity *= cl.mouse_sens; // scale by fov
		mx *= accel_sensitivity;
		my *= accel_sensitivity;

		if( !mx && !my ) return;

		// add mouse X/Y movement to cmd
		if( in_strafe.active )
			cmd->sidemove = bound( -128, cmd->sidemove + m_side->value * mx, 127 );
		else cl.viewangles[YAW] -= m_yaw->value * mx;

		if( cl_mouselook->value && !in_strafe.active )
			cl.viewangles[PITCH] += m_pitch->value * my;
		else cmd->forwardmove = bound( -128, cmd->forwardmove - m_forward->value * my, 127 );
	}
	else
	{
		accel_sensitivity = ui_sensitivity->value + rate * cl_mouseaccel->value;
		cls.mouse_x = mx * accel_sensitivity;
		cls.mouse_y = my * accel_sensitivity;
	}
}

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition


CL_KeyEvent( int key, bool down, uint time );

+mlook src time
===============================================================================
*/
void IN_KeyDown( kbutton_t *b )
{
	int	k;
	char	*c;
	
	c = Cmd_Argv( 1 );
	if (c[0]) k = com.atoi(c);
	else k = -1; // typed manually at the console for continuous down

	if (k == b->down[0] || k == b->down[1])
		return; // repeating key
	
	if( !b->down[0] ) b->down[0] = k;
	else if( !b->down[1] ) b->down[1] = k;
	else
	{
		MsgDev( D_ERROR, "three keys down for a button!\n");
		return;
	}
	
	if( b->active & 1 ) return; // still down

	// save timestamp
	c = Cmd_Argv(2);
	b->downtime = com.atoi(c);
	b->active = true;
	b->was_pressed = true;
}

void IN_KeyUp( kbutton_t *b )
{
	int	k;
	char	*c;
	uint	uptime;

	c = Cmd_Argv( 1 );
	if (c[0]) k = com.atoi(c);
	else
	{
		// typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->active = false; // impulse up
		return;
	}

	if( b->down[0] == k ) b->down[0] = 0;
	else if( b->down[1] == k ) b->down[1] = 0;
	else return; // key up without coresponding down (menu pass through)

	if( b->down[0] || b->down[1] )
		return; // some other key is still holding it down

	// save timestamp for partial frame summing
	c = Cmd_Argv( 2 );
	uptime = com.atoi( c );
	if( uptime ) b->msec += uptime - b->downtime;
	else b->msec += frame_msec / 2;
	b->active = false;
}

/*
===============
CL_KeyState

Returns the fraction of the frame that the key was down
===============
*/
float CL_KeyState( kbutton_t *key )
{
	float	val;
	int	msec;

	msec = key->msec;
	key->msec = 0;

	if( key->active )
	{	
		// still down
		if( !key->downtime ) msec = host.frametime[0];
		else msec += host.frametime[0] - key->downtime;
		key->downtime = host.frametime[0];
	}
	val = ( float )msec / frame_msec;
	return bound( 0, val, 1 );
}

void IN_UpDown(void) {IN_KeyDown(&in_up);}
void IN_UpUp(void) {IN_KeyUp(&in_up);}
void IN_DownDown(void) {IN_KeyDown(&in_down);}
void IN_DownUp(void) {IN_KeyUp(&in_down);}
void IN_LeftDown(void) {IN_KeyDown(&in_left);}
void IN_LeftUp(void) {IN_KeyUp(&in_left);}
void IN_RightDown(void) {IN_KeyDown(&in_right);}
void IN_RightUp(void) {IN_KeyUp(&in_right);}
void IN_ForwardDown(void) {IN_KeyDown(&in_forward);}
void IN_ForwardUp(void) {IN_KeyUp(&in_forward);}
void IN_BackDown(void) {IN_KeyDown(&in_back);}
void IN_BackUp(void) {IN_KeyUp(&in_back);}
void IN_LookupDown(void) {IN_KeyDown(&in_lookup);}
void IN_LookupUp(void) {IN_KeyUp(&in_lookup);}
void IN_LookdownDown(void) {IN_KeyDown(&in_lookdown);}
void IN_LookdownUp(void) {IN_KeyUp(&in_lookdown);}
void IN_MoveleftDown(void) {IN_KeyDown(&in_moveleft);}
void IN_MoveleftUp(void) {IN_KeyUp(&in_moveleft);}
void IN_MoverightDown(void) {IN_KeyDown(&in_moveright);}
void IN_MoverightUp(void) {IN_KeyUp(&in_moveright);}
void IN_MLookDown( void ){Cvar_SetValue( "cl_mouselook", 1 );}
void IN_MLookUp( void ){Cvar_SetValue( "cl_mouselook", 0 );}
void IN_Impulse (void) {in_impulse = com.atoi(Cmd_Argv(1));}
void IN_SpeedDown(void) {IN_KeyDown(&in_speed);}
void IN_SpeedUp(void) {IN_KeyUp(&in_speed);}
void IN_StrafeDown(void) {IN_KeyDown(&in_strafe);}
void IN_StrafeUp(void) {IN_KeyUp(&in_strafe);}
void IN_AttackDown(void) {IN_KeyDown(&in_attack);}
void IN_AttackUp(void) {IN_KeyUp(&in_attack);}
void IN_Attack2Down(void) {IN_KeyDown(&in_attack2);}
void IN_Attack2Up(void) {IN_KeyUp(&in_attack2);}
void IN_UseDown (void) {IN_KeyDown(&in_use);}
void IN_UseUp (void) {IN_KeyUp(&in_use);}
void IN_CenterView (void) {cl.viewangles[PITCH] = -SHORT2ANGLE(cl.frame.ps.delta_angles[PITCH]);}


//==========================================================================
/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
void CL_AdjustAngles( void )
{
	float	speed;
	
	if( in_speed.active )
		speed = cls.frametime * cl_anglespeedkey->value;
	else speed = cls.frametime;

	if(!in_strafe.active )
	{
		cl.viewangles[YAW] -= speed * cl_yawspeed->value * CL_KeyState( &in_right );
		cl.viewangles[YAW] += speed * cl_yawspeed->value * CL_KeyState( &in_left );
	}
	
	cl.viewangles[PITCH] -= speed * cl_pitchspeed->value * CL_KeyState( &in_lookup );
	cl.viewangles[PITCH] += speed * cl_pitchspeed->value * CL_KeyState( &in_lookdown );
}

/*
================
CL_KeyMove

Sets the usercmd_t based on key states
================
*/
void CL_KeyMove( usercmd_t *cmd )
{
	int	movespeed;
	int	forward = 0, side = 0, up = 0;

	// adjust for speed key / running
	// the walking flag is to keep animations consistant
	// even during acceleration and develeration
	if( in_speed.active ^ cl_run->integer )
	{
		movespeed = 127;
		cmd->buttons &= ~BUTTON_WALKING;
	}
	else
	{
		cmd->buttons |= BUTTON_WALKING;
		movespeed = 64;
	}

	if( in_strafe.active )
	{
		side += movespeed * CL_KeyState( &in_right );
		side -= movespeed * CL_KeyState( &in_left );
	}
	side += movespeed * CL_KeyState( &in_moveright );
	side -= movespeed * CL_KeyState( &in_moveleft );
	up += movespeed * CL_KeyState( &in_up );
	up -= movespeed * CL_KeyState( &in_down );
	forward += movespeed * CL_KeyState( &in_forward );
	forward -= movespeed * CL_KeyState( &in_back );

	cmd->forwardmove = bound( -128, forward, 127 );
	cmd->sidemove = bound( -128, side, 127 );
	cmd->upmove = bound( -128, up, 127 );
}

/*
==============
CL_CmdButtons
==============
*/
void CL_CmdButtons( usercmd_t *cmd )
{
	if( in_attack.active ) cmd->buttons |= BUTTON_ATTACK;
	if( in_attack2.active ) cmd->buttons |= BUTTON_ATTACK2;
	if( in_use.active ) cmd->buttons |= BUTTON_USE;

	if( anykeydown && cls.key_dest == key_game)
		cmd->buttons |= BUTTON_ANY;
}

/*
==============
CL_FinishMove
==============
*/
void CL_FinishMove( usercmd_t *cmd )
{
	int	i;

	for( i = 0; i < 3; i++ )
		cmd->angles[i] = ANGLE2SHORT(cl.viewangles[i]);

	cmd->servertime = cl.frame.servertime;
	cmd->impulse = in_impulse;
	in_impulse = 0;

	// FIXME: send the ambient light level for all! entities properly
	cmd->lightlevel = (byte)cl_lightlevel->value;
}


/*
=================
CL_CreateCmd
=================
*/
usercmd_t CL_CreateCmd( void )
{
	usercmd_t		cmd;
	vec3_t		old_angles;

	VectorCopy( cl.viewangles, old_angles );

	// keyboard angle adjustment
	CL_AdjustAngles ();
	
	memset( &cmd, 0, sizeof( cmd ));

	CL_CmdButtons( &cmd );

	// get basic movement from keyboard
	CL_KeyMove( &cmd );

	// get basic movement from mouse
	CL_MouseMove( &cmd );

	// check to make sure the angles haven't wrapped
	if( cl.viewangles[PITCH] - old_angles[PITCH] > 90 )
		cl.viewangles[PITCH] = old_angles[PITCH] + 90;
	else if ( old_angles[PITCH] - cl.viewangles[PITCH] > 90 )
		cl.viewangles[PITCH] = old_angles[PITCH] - 90;

	// store out the final values
	CL_FinishMove( &cmd );

	return cmd;
}

/*
=================
CL_CreateNewCommands

Create a new usercmd_t structure for this frame
=================
*/
void CL_CreateNewCommands( void )
{
	int	cmd_num;

	if( cls.state < ca_connected ) return;

	frame_msec = host.frametime[0] - host.frametime[1];
	if( frame_msec > 200 ) frame_msec = 200;
	host.frametime[1] = host.frametime[0];

	// generate a command for this frame
	cl.cmd_number++;
	cmd_num = cl.cmd_number & CMD_MASK;
	cl.cmds[cmd_num] = CL_CreateCmd();
}

/*
=================
CL_ReadyToSendPacket

Returns qfalse if we are over the maxpackets limit
and should choke back the bandwidth a bit by not sending
a packet this frame.  All the commands will still get
delivered in the next packet, but saving a header and
getting more delta compression will reduce total bandwidth.
=================
*/
bool CL_ReadyToSendPacket( void )
{
	int	old_packetnum;
	int	delta;

	// don't send anything if playing back a demo
	if( cls.demoplayback || cls.state == ca_cinematic )
		return false;

	// If we are downloading, we send no less than 50ms between packets
	if( *cls.downloadtempname && cls.realtime - cls.netchan.last_sent < 50 )
		return false;

	// if we don't have a valid gamestate yet, only send
	// one packet a second
	if( cls.state < ca_connected && !*cls.downloadtempname && cls.realtime - cls.netchan.last_sent < 1000 )
		return false;

	// send every frame for loopbacks
	if( cls.netchan.remote_address.type == NA_LOOPBACK )
		return true;

	// send every frame for LAN
	if( NET_IsLANAddress( cls.netchan.remote_address ))
		return true;

	// check for exceeding cl_maxpackets
	if ( cl_maxpackets->integer < 15 )
		Cvar_Set( "cl_maxpackets", "15" );
	else if( cl_maxpackets->integer > 125 )
		Cvar_Set( "cl_maxpackets", "125" );

	old_packetnum = (cls.netchan.outgoing_sequence - 1) & PACKET_MASK;
	delta = cls.realtime - cl.packets[old_packetnum].realtime;

	// the accumulated commands will go out in the next packet
	if( delta < 1000 / cl_maxpackets->integer )
		return false;
	return true;
}

/*
===================
CL_WritePacket

Create and send the command packet to the server
Including both the reliable commands and the usercmds

During normal gameplay, a client packet will contain something like:

1	clc_move
1	checksum byte
4	serverframe	( -1 disable delta compression )
4	command_count
<count * usercmds>
===================
*/
void CL_WritePacket( void )
{
	int	i, j;
	usercmd_t	*cmd, *oldcmd, nullcmd;
	int	packet_num, old_packetnum;
	int	count, checksumIndex, crc;
	byte	data[MAX_MSGLEN];
	sizebuf_t	usercmd;

	// don't send anything if playing back a demo
	if( cls.demoplayback || cls.state == ca_cinematic )
		return;

	if( cls.state == ca_connected )
	{
		if (cls.netchan.message.cursize || cls.realtime - cls.netchan.last_sent > 1000 )
			Netchan_Transmit (&cls.netchan, 0, NULL );	
		return;
	}

	SZ_Init(&usercmd, data, sizeof(data));
	memset( &nullcmd, 0, sizeof(nullcmd));
	oldcmd = &nullcmd;

	// send a userinfo update if needed
	if( userinfo_modified )
	{
		userinfo_modified = false;
		MSG_WriteByte( &cls.netchan.message, clc_userinfo);
		MSG_WriteString (&cls.netchan.message, Cvar_Userinfo());
	}

	old_packetnum = ( cls.netchan.outgoing_sequence - 2 ) & PACKET_MASK;
	count = cl.cmd_number - cl.packets[old_packetnum].cmd_number;
	if( count > PACKET_BACKUP ) count = PACKET_BACKUP;

	if( count >= 1 )
	{
		// begin a client move command
		MSG_WriteByte (&usercmd, clc_move);
		// save the position for a checksum byte
		checksumIndex = usercmd.cursize;
		MSG_WriteByte( &usercmd, 0 );

		// let the server know what the last frame we
		// got was, so the next message can be delta compressed
		if( cl_nodelta->value || !cl.frame.valid || cls.demowaiting )
		{
			MSG_WriteLong( &usercmd, -1 );	// no compression
		}
		else MSG_WriteLong( &usercmd, cl.frame.serverframe );
		MSG_WriteByte( &usercmd, count );		// write the command count

		// write all the commands, including the predicted command
		for( i = 0; i < count; i++ )
		{
			j = (cls.netchan.outgoing_sequence - count + i + 1) & CMD_MASK;
			cmd = &cl.cmds[j];
			MSG_WriteDeltaUsercmd( &usercmd, oldcmd, cmd );
			oldcmd = cmd;
		}

		// calculate a checksum over the move commands
		crc = CRC_Sequence( usercmd.data+checksumIndex+1, usercmd.cursize-checksumIndex-1, cls.netchan.outgoing_sequence);
		usercmd.data[checksumIndex] = crc;
	}

	// deliver the message
	cls.netchan.last_sent = cls.realtime;
	Netchan_Transmit( &cls.netchan, usercmd.cursize, usercmd.data );
}

/*
=================
CL_SendCmd

Called every frame to builds and sends a command packet to the server.
=================
*/
void CL_SendCmd( void )
{
	// we create commands even if a demo is playing,
	CL_CreateNewCommands();

	// don't send a packet if the last packet was sent too recently
	if(CL_ReadyToSendPacket()) CL_WritePacket();
}

/*
============
CL_InitInput
============
*/
void CL_InitInput (void)
{
	// mouse variables
	m_filter = Cvar_Get("m_filter", "0", 0, "enable mouse filter" );
	cl_sensitivity = Cvar_Get( "cl_sensitivity", "3", CVAR_ARCHIVE, "mouse in-game sensitivity" );
	ui_sensitivity = Cvar_Get( "ui_sensitivity", "0.5", CVAR_ARCHIVE, "mouse in-menu sensitivity" );
	cl_mouseaccel = Cvar_Get( "cl_mouseaccelerate", "0", CVAR_ARCHIVE, "mouse accelerate factor" ); 

	// centering
	v_centermove = Cvar_Get ("v_centermove", "0.15", 0, "client center moving" );
	v_centerspeed = Cvar_Get ("v_centerspeed", "500",	0, "client center speed" );
	cl_nodelta = Cvar_Get ("cl_nodelta", "0", 0, "disable delta-compression for usercommnds" );
	
	Cmd_AddCommand ("centerview", IN_CenterView, "gradually recenter view (stop looking up/down)" );

	// input commands
	Cmd_AddCommand ("+moveup",IN_UpDown, "swim upward");
	Cmd_AddCommand ("-moveup",IN_UpUp, "stop swimming upward");
	Cmd_AddCommand ("+movedown",IN_DownDown, "swim downward");
	Cmd_AddCommand ("-movedown",IN_DownUp, "stop swimming downward");
	Cmd_AddCommand ("+left",IN_LeftDown, "turn left");
	Cmd_AddCommand ("-left",IN_LeftUp, "stop turning left");
	Cmd_AddCommand ("+right",IN_RightDown, "turn right");
	Cmd_AddCommand ("-right",IN_RightUp, "stop turning right");
	Cmd_AddCommand ("+forward",IN_ForwardDown, "move forward");
	Cmd_AddCommand ("-forward",IN_ForwardUp, "stop moving forward");
	Cmd_AddCommand ("+back",IN_BackDown, "move backward");
	Cmd_AddCommand ("-back",IN_BackUp, "stop moving backward");
	Cmd_AddCommand ("+lookup", IN_LookupDown, "look upward");
	Cmd_AddCommand ("-lookup", IN_LookupUp, "stop looking upward");
	Cmd_AddCommand ("+lookdown", IN_LookdownDown, "look downward");
	Cmd_AddCommand ("-lookdown", IN_LookdownUp, "stop looking downward");
	Cmd_AddCommand ("+strafe", IN_StrafeDown, "activate strafing mode (move instead of turn)\n");
	Cmd_AddCommand ("-strafe", IN_StrafeUp, "deactivate strafing mode");
	Cmd_AddCommand ("+moveleft", IN_MoveleftDown, "strafe left");
	Cmd_AddCommand ("-moveleft", IN_MoveleftUp, "stop strafing left");
	Cmd_AddCommand ("+moveright", IN_MoverightDown, "strafe right");
	Cmd_AddCommand ("-moveright", IN_MoverightUp, "stop strafing right");
	Cmd_AddCommand ("+speed", IN_SpeedDown, "activate run mode (faster movement and turning)");
	Cmd_AddCommand ("-speed", IN_SpeedUp, "deactivate run mode");
	Cmd_AddCommand ("+attack", IN_AttackDown, "begin firing");
	Cmd_AddCommand ("-attack", IN_AttackUp, "stop firing");
	Cmd_AddCommand ("+attack2", IN_Attack2Down, "begin alternate firing");
	Cmd_AddCommand ("-attack2", IN_Attack2Up, "stop alternate firing");
	Cmd_AddCommand ("+use", IN_UseDown, "use item (doors, monsters, inventory, etc" );
	Cmd_AddCommand ("-use", IN_UseUp, "stop using item" );
	Cmd_AddCommand ("impulse", IN_Impulse, "send an impulse number to server (select weapon, use item, etc)");
	Cmd_AddCommand ("+mlook", IN_MLookDown, "activate mouse looking mode, do not recenter view" );
	Cmd_AddCommand ("-mlook", IN_MLookUp, "deactivate mouse looking mode" );
}

/*
============
CL_ShutdownInput
============
*/
void CL_ShutdownInput( void )
{
	Cmd_RemoveCommand ("centerview" );

	// input commands
	Cmd_RemoveCommand ("+moveup" );
	Cmd_RemoveCommand ("-moveup" );
	Cmd_RemoveCommand ("+movedown" );
	Cmd_RemoveCommand ("-movedown" );
	Cmd_RemoveCommand ("+left" );
	Cmd_RemoveCommand ("-left" );
	Cmd_RemoveCommand ("+right" );
	Cmd_RemoveCommand ("-right" );
	Cmd_RemoveCommand ("+forward" );
	Cmd_RemoveCommand ("-forward" );
	Cmd_RemoveCommand ("+back" );
	Cmd_RemoveCommand ("-back" );
	Cmd_RemoveCommand ("+lookup" );
	Cmd_RemoveCommand ("-lookup" );
	Cmd_RemoveCommand ("+lookdown" );
	Cmd_RemoveCommand ("-lookdown" );
	Cmd_RemoveCommand ("+strafe" );
	Cmd_RemoveCommand ("-strafe" );
	Cmd_RemoveCommand ("+moveleft" );
	Cmd_RemoveCommand ("-moveleft" );
	Cmd_RemoveCommand ("+moveright" );
	Cmd_RemoveCommand ("-moveright" );
	Cmd_RemoveCommand ("+speed" );
	Cmd_RemoveCommand ("-speed" );
	Cmd_RemoveCommand ("+attack" );
	Cmd_RemoveCommand ("-attack" );
	Cmd_RemoveCommand ("+attack2" );
	Cmd_RemoveCommand ("-attack2" );
	Cmd_RemoveCommand ("+use" );
	Cmd_RemoveCommand ("-use" );
	Cmd_RemoveCommand ("impulse" );
	Cmd_RemoveCommand ("+mlook" );
	Cmd_RemoveCommand ("-mlook" );
}