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
	int	state;
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
		if( cl_mouselook->value ) cl.viewangles[YAW] -= m_yaw->value * mx;
		else if( in_strafe.state & 1 ) cmd->sidemove = cmd->sidemove + m_side->value * mx;
		

		if( cl_mouselook->value ) cl.viewangles[PITCH] += m_pitch->value * my;
		else if( in_strafe.state & 1 ) cmd->forwardmove = cmd->forwardmove - m_forward->value * my;
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
	
	c = Cmd_Argv(1);
	if (c[0]) k = atoi(c);
	else k = -1; // typed manually at the console for continuous down

	if (k == b->down[0] || k == b->down[1])
		return; // repeating key
	
	if (!b->down[0]) b->down[0] = k;
	else if (!b->down[1]) b->down[1] = k;
	else
	{
		Msg ("Three keys down for a button!\n");
		return;
	}
	
	if (b->state & 1) return; // still down

	// save timestamp
	c = Cmd_Argv(2);
	b->downtime = com.atoi(c);

	if (!b->downtime) b->downtime = host.frametime[0] - HOST_FRAMETIME;
	b->state |= 1 + 2;	// down + impulse down
}

void IN_KeyUp( kbutton_t *b )
{
	int	k;
	char	*c;
	uint	uptime;

	c = Cmd_Argv( 1 );
	if( c[0] ) k = com.atoi( c );
	else
	{
		// typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state = 4; // impulse up
		return;
	}
	if( b->down[0] == k ) b->down[0] = 0;
	else if( b->down[1] == k ) b->down[1] = 0;
	else return; // key up without coresponding down (menu pass through)

	if( b->down[0] || b->down[1] )
		return; // some other key is still holding it down

	if(!(b->state & 1)) return; // still up (this should not happen)

	// save timestamp
	c = Cmd_Argv( 2 );
	uptime = com.atoi(c);
	if( uptime ) b->msec += uptime - b->downtime;
	else b->msec += 10;
          
	b->state &= ~1; // now up
	b->state |= 4; // impulse up
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

	key->state &= 1;		// clear impulses

	msec = key->msec;
	key->msec = 0;

	if( key->state )
	{	
		// still down
		msec += host.frametime[0] - key->downtime;
		key->downtime = host.frametime[0];
	}

#if 0
	if (msec)
	{
		Msg ("%i \n", msec);
	}
#endif

	val = (float)msec / frame_msec;
	if (val < 0)
		val = 0;
	if (val > 1)
		val = 1;

	return val;
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
void IN_Impulse (void) {in_impulse = com.atoi(Cmd_Argv(1));}
void IN_MLookDown( void ){Cvar_SetValue( "cl_mouselook", 1 );}
void IN_MLookUp( void ){ IN_CenterView(); Cvar_SetValue( "cl_mouselook", 0 );}
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
	
	if( in_speed.state & 1 )
		speed = cls.frametime * cl_anglespeedkey->value;
	else speed = cls.frametime;

	if(!(in_strafe.state & 1))
	{
		cl.viewangles[YAW] -= speed * cl_yawspeed->value * CL_KeyState( &in_right );
		cl.viewangles[YAW] += speed * cl_yawspeed->value * CL_KeyState( &in_left );
	}
	
	cl.viewangles[PITCH] -= speed * cl_pitchspeed->value * CL_KeyState( &in_lookup );
	cl.viewangles[PITCH] += speed * cl_pitchspeed->value * CL_KeyState( &in_lookdown );
}

/*
================
CL_BaseMove

Send the intended movement message to the server
================
*/
void CL_BaseMove( usercmd_t *cmd )
{	
	CL_AdjustAngles ();
	
	memset (cmd, 0, sizeof(*cmd));
	
	VectorCopy (cl.viewangles, cmd->angles);
	if( in_strafe.state & 1 )
	{
		cmd->sidemove += cl_sidespeed->value * CL_KeyState (&in_right);
		cmd->sidemove -= cl_sidespeed->value * CL_KeyState (&in_left);
	}

	cmd->sidemove += cl_sidespeed->value * CL_KeyState (&in_moveright);
	cmd->sidemove -= cl_sidespeed->value * CL_KeyState (&in_moveleft);

	cmd->upmove += cl_upspeed->value * CL_KeyState (&in_up);
	cmd->upmove -= cl_upspeed->value * CL_KeyState (&in_down);

	cmd->forwardmove += cl_forwardspeed->value * CL_KeyState (&in_forward);
	cmd->forwardmove -= cl_forwardspeed->value * CL_KeyState (&in_back);

	// adjust for speed key / running
	if( in_speed.state & 1 ^ cl_run->integer )
	{
		cmd->buttons &= ~BUTTON_WALKING;
		cmd->forwardmove *= 2;
		cmd->sidemove *= 2;
		cmd->upmove *= 2;
	}
	else cmd->buttons |= BUTTON_WALKING;	
}

void CL_ClampPitch (void)
{
	float	pitch;

	pitch = SHORT2ANGLE(cl.frame.ps.delta_angles[PITCH]);
	if( pitch > 180 ) pitch -= 360;

	if( cl.viewangles[PITCH] + pitch < -360 ) cl.viewangles[PITCH] += 360; // wrapped
	if( cl.viewangles[PITCH] + pitch > 360 ) cl.viewangles[PITCH] -= 360; // wrapped
	if( cl.viewangles[PITCH] + pitch > 89 ) cl.viewangles[PITCH] = 89 - pitch;
	if( cl.viewangles[PITCH] + pitch < -89 ) cl.viewangles[PITCH] = -89 - pitch;
}

/*
==============
CL_CmdButtons
==============
*/
void CL_CmdButtons( usercmd_t *cmd )
{
	if ( in_attack.state & 3 )
		cmd->buttons |= BUTTON_ATTACK;
	in_attack.state &= ~2;

	if ( in_attack2.state & 3 )
		cmd->buttons |= BUTTON_ATTACK2;
	in_attack2.state &= ~2;	

	if (in_use.state & 3)
		cmd->buttons |= BUTTON_USE;
	in_use.state &= ~2;

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
	int	i, ms;

	// send milliseconds of time to apply the move
	ms = cls.frametime * 1000;
	if( ms > 250 ) ms = 100; // time was unreasonable
	cmd->msec = ms;

	CL_ClampPitch();

	for( i = 0; i < 3; i++ )
		cmd->angles[i] = ANGLE2SHORT(cl.viewangles[i]);

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

	frame_msec = host.frametime[0] - host.frametime[1];
	if( frame_msec < 1 ) frame_msec = 1;
	if( frame_msec > 200 ) frame_msec = 200;

	// get basic movement from mouse
	CL_BaseMove( &cmd );
	CL_MouseMove( &cmd );
	CL_CmdButtons( &cmd );
	CL_FinishMove( &cmd );

	host.frametime[1] = host.frametime[0];

	return cmd;
}

/*
=================
CL_SendCmd

Called every frame to builds and sends a command packet to the server.
=================
*/
void CL_SendCmd( void )
{
	sizebuf_t		buf;
	byte		data[128];
	int		i;
	usercmd_t		*cmd, *oldcmd;
	usercmd_t		nullcmd;
	int		checksumIndex;
	int		curtime;

	// build a command even if not connected

	// save this command off for prediction
	i = cls.netchan.outgoing_sequence & (CMD_BACKUP-1);
	cmd = &cl.cmds[i];
	curtime = Sys_Milliseconds();
	*cmd = CL_CreateCmd ();

	if(cls.state == ca_disconnected || cls.state == ca_connecting)
		return;

	// ignore commands for demo mode
	if(cls.state == ca_cinematic || cls.demoplayback)
		return;

	if( cls.state == ca_connected )
	{
		if (cls.netchan.message.cursize || cls.realtime - cls.netchan.last_sent > 1000 )
			Netchan_Transmit( &cls.netchan, 0, NULL );	
		return;
	}

	// send a userinfo update if needed
	if (userinfo_modified)
	{
		userinfo_modified = false;
		MSG_WriteByte (&cls.netchan.message, clc_userinfo);
		MSG_WriteString (&cls.netchan.message, Cvar_Userinfo());
	}

	// begin a client move command
	MSG_Init(&buf, data, sizeof(data));
	MSG_WriteByte (&buf, clc_move);

	// save the position for a checksum byte
	checksumIndex = buf.cursize;
	MSG_WriteByte (&buf, 0);

	// let the server know what the last frame we
	// got was, so the next message can be delta compressed
	if (cl_nodelta->value || !cl.frame.valid || cls.demowaiting)
	{
		MSG_WriteLong (&buf, -1);	// no compression
	}
	else
	{
		MSG_WriteLong (&buf, cl.frame.serverframe);
	}

	// send this and the previous cmds in the message, so
	// if the last packet was dropped, it can be recovered
	i = (cls.netchan.outgoing_sequence-2) & (CMD_BACKUP-1);
	cmd = &cl.cmds[i];
	memset (&nullcmd, 0, sizeof(nullcmd));
	MSG_WriteDeltaUsercmd (&buf, &nullcmd, cmd);
	oldcmd = cmd;

	i = (cls.netchan.outgoing_sequence-1) & (CMD_BACKUP-1);
	cmd = &cl.cmds[i];
	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);
	oldcmd = cmd;

	i = (cls.netchan.outgoing_sequence) & (CMD_BACKUP-1);
	cmd = &cl.cmds[i];
	MSG_WriteDeltaUsercmd (&buf, oldcmd, cmd);

	// calculate a checksum over the move commands
	buf.data[checksumIndex] = CRC_Sequence( buf.data + checksumIndex + 1, buf.cursize - checksumIndex - 1, cls.netchan.outgoing_sequence);

	// deliver the message
	Netchan_Transmit( &cls.netchan, buf.cursize, buf.data );
}

/*
============
CL_InitInput
============
*/
void CL_InitInput( void )
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