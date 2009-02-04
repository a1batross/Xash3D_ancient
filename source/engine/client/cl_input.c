//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cl_input.c - movement commands
//=======================================================================

#include "common.h"
#include "client.h"

typedef struct
{
	int	down[2];		// key nums holding it down
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

kbutton_t	in_left, in_right, in_forward, in_back;
kbutton_t	in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t	in_strafe, in_speed, in_use, in_attack, in_attack2;
kbutton_t	in_up, in_down, in_reload;

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
void CL_MouseEvent( int mx, int my )
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

	rate = com.sqrt( mx * mx + my * my ) / 0.5f;

	if( cls.key_dest != key_menu )
	{
		if( cl.frame.ps.health <= 0 ) return;
		if( cl.mouse_sens == 0.0f ) cl.mouse_sens = 1.0f;
		accel_sensitivity = cl_sensitivity->value + rate * cl_mouseaccel->value;
		accel_sensitivity *= cl.mouse_sens; // scale by fov
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
	if (c[0]) k = com.atoi(c);
	else k = -1; // typed manually at the console for continuous down

	if (k == b->down[0] || k == b->down[1])
		return; // repeating key
	
	if( !b->down[0] ) b->down[0] = k;
	else if( !b->down[1] ) b->down[1] = k;
	else
	{
		MsgDev( D_ERROR, "IN_KeyDown: three keys down for a button!\n" );
		return;
	}
	
	if( b->state & 1 ) return; // still down
	b->state |= 1 + 2;	// down + impulse down
}

void IN_KeyUp( kbutton_t *b )
{
	int	k;
	char	*c;

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

	b->state &= ~1; // now up
	b->state |= 4; // impulse up
}

/*
===============
CL_KeyState

Returns 0.25 if a key was pressed and released during the frame,
0.5 if it was pressed and held
0 if held then released, and
1.0 if held for the entire time
===============
*/
float CL_KeyState( kbutton_t *key )
{
	float	val;
	bool	impulsedown, impulseup, down;

	impulsedown = key->state & 2;
	impulseup = key->state & 4;
	down = key->state & 1;
	val = 0.0f;

	if( impulsedown && !impulseup )
	{
		if( down ) val = 0.5;	// pressed and held this frame
		else val = 0;		// I_Error ();
	}
	if( impulseup && !impulsedown )
	{
		if( down ) val = 0;		// I_Error ();
		else val = 0;		// released this frame
	}
	if( !impulsedown && !impulseup )
	{
		if( down ) val = 1.0;	// held the entire frame
		else val = 0;		// up the entire frame
	}
	if( impulsedown && impulseup )
	{
		if( down ) val = 0.75;	// released and re-pressed this frame
		else val = 0.25;		// pressed and released this frame
	}

	key->state &= 1;		// clear impulses

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
void IN_AttackUp(void) {IN_KeyUp(&in_attack); }
void IN_Attack2Down(void) {IN_KeyDown(&in_attack2);}
void IN_Attack2Up(void) {IN_KeyUp(&in_attack2);}
void IN_UseDown (void) {IN_KeyDown(&in_use);}
void IN_UseUp (void) {IN_KeyUp(&in_use);}
void IN_ReloadDown(void) {IN_KeyDown(&in_reload);}
void IN_ReloadUp(void) {IN_KeyUp(&in_reload);}
void IN_MLookDown( void ){Cvar_SetValue( "cl_mouselook", 1 );}
void IN_MLookUp( void ){ IN_CenterView(); Cvar_SetValue( "cl_mouselook", 0 );}
void IN_CenterView (void){ cl.viewangles[PITCH] = -cl.frame.ps.delta_angles[PITCH]; }


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

	if( cl.frame.ps.health <= 0 ) return;
	
	if( in_speed.state & 1 )
		speed = host.frametime * cl_anglespeedkey->value;
	else speed = host.frametime;

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
	
	memset( cmd, 0, sizeof( *cmd ));
	
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
		cmd->forwardmove *= 2;
		cmd->sidemove *= 2;
		cmd->upmove *= 2;
	}
}

void CL_ClampPitch (void)
{
	float	pitch;

	pitch = cl.frame.ps.delta_angles[PITCH];
	if( pitch > 180 ) pitch -= 360;

	if( cl.viewangles[PITCH] + pitch < -360 ) cl.viewangles[PITCH] += 360; // wrapped
	if( cl.viewangles[PITCH] + pitch > 360 ) cl.viewangles[PITCH] -= 360; // wrapped
	if( cl.viewangles[PITCH] + pitch > 89 ) cl.viewangles[PITCH] = 89 - pitch;
	if( cl.viewangles[PITCH] + pitch < -89 ) cl.viewangles[PITCH] = -89 - pitch;
}

/*
==============
CL_ButtonBits
==============
*/
int CL_ButtonBits( int bResetState )
{
	int bits = 0;

	if( in_attack.state & 3 )
		bits |= IN_ATTACK;

	if( in_attack2.state & 3 )
		bits |= IN_ATTACK2;

	if( in_use.state & 3 )
		bits |= IN_USE;

	if (in_speed.state & 3)
		bits |= IN_RUN;

	if( in_reload.state & 3)
		bits |= IN_RELOAD;

	if( bResetState )
	{
		in_attack.state &= ~2;
		in_attack2.state &= ~2;	
		in_use.state &= ~2;
		in_speed.state &= ~2;
		in_reload.state &= ~2;
	}
	return bits;
}

/*
============
CL_ResetButtonBits

============
*/
void CL_ResetButtonBits( int bits )
{
	int bitsNew = CL_ButtonBits( 0 ) ^ bits;

	// has the attack button been changed
	if( bitsNew & IN_ATTACK )
	{
		// was it pressed? or let go?
		if( bits & IN_ATTACK )
		{
			IN_KeyDown( &in_attack );
		}
		else
		{
			// totally clear state
			in_attack.state &= ~7;
		}
	}
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
	// FIXME: return sv.time to let server calculate ping
	ms = host.frametime * 1000;
	if( ms > 250 ) ms = 100; // time was unreasonable
	cmd->msec = ms;

	CL_ClampPitch();

	for( i = 0; i < 3; i++ )
		cmd->angles[i] = cl.viewangles[i];

	// HACKHACK: client lightlevel
	cmd->lightlevel = (byte)cl_lightlevel->value;
	if( cls.key_dest == key_game )
		cmd->buttons = CL_ButtonBits( 1 );

	// process commands with user dll's
	cl.data.fov = cl.frame.ps.fov;
	cl.data.iKeyBits = CL_ButtonBits( 0 );

	cl.data.iWeaponBits = cl.frame.ps.weapons;
	cl.data.mouse_sensitivity = cl.mouse_sens;
	VectorCopy( cl.viewangles, cl.data.angles );
	VectorCopy( cl.refdef.origin, cl.data.origin );

	cls.dllFuncs.pfnUpdateClientData( &cl.data, ( cl.time * 0.001f ));

	CL_ResetButtonBits( cl.data.iKeyBits );
	cl.refdef.fov_x = cl.data.fov;
	cl.mouse_sens = cl.data.mouse_sensitivity;
}


/*
=================
CL_CreateCmd
=================
*/
usercmd_t CL_CreateCmd( void )
{
	usercmd_t		cmd;

	// get basic movement from mouse
	CL_BaseMove( &cmd );
	CL_MouseMove( &cmd );
	CL_FinishMove( &cmd );

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
		if( cls.netchan.message.cursize || host.realtime - cls.netchan.last_sent > 1.0 )
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
	MSG_Init( &buf, data, sizeof( data ));
	MSG_WriteByte( &buf, clc_move );

	// save the position for a checksum byte
	checksumIndex = buf.cursize;
	MSG_WriteByte( &buf, 0 );

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
	Cmd_AddCommand ("+reload", IN_ReloadDown, "reload current weapon" );
	Cmd_AddCommand ("-reload", IN_ReloadUp, "continue reload weapon" );
	Cmd_AddCommand ("+mlook", IN_MLookDown, "activate mouse looking mode, do not recenter view" );
	Cmd_AddCommand ("-mlook", IN_MLookUp, "deactivate mouse looking mode" );
}

/*
============
CL_InitServerCommands
============
*/
void CL_InitServerCommands( void )
{
	Cmd_AddCommand ("impulse", NULL, "send impulse to a client" );
	Cmd_AddCommand ("noclip", NULL, "enable or disable no clipping mode" );
	Cmd_AddCommand ("fullupdate", NULL, "re-init HUD on start demo recording" );
	Cmd_AddCommand ("give", NULL, "give specified item or weapon" );
	Cmd_AddCommand ("intermission", NULL, "go to intermission" );
	Cmd_AddCommand ("god", NULL, "classic cheat" );
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
	Cmd_RemoveCommand ("+reload" );
	Cmd_RemoveCommand ("-reload" );
	Cmd_RemoveCommand ("+use" );
	Cmd_RemoveCommand ("-use" );
	Cmd_RemoveCommand ("+mlook" );
	Cmd_RemoveCommand ("-mlook" );
}