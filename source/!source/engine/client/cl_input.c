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
// cl.input.c  -- builds an intended movement command to send to the server

#include "client.h"

cvar_t	*cl_nodelta;
cvar_t	*v_centermove;
cvar_t	*v_centerspeed;
cvar_t	*m_filter;	// mouse sensetivity
cvar_t	*m_mouse;

bool	in_appactive;

uint	frame_msec;
uint	old_sys_frame_time;

int	mouse_buttons;
int	mouse_oldbuttonstate;
POINT	current_pos;
int	mouse_x, mouse_y, old_mouse_x, old_mouse_y, mx_accum, my_accum;

bool	mouseactive;	// false when not focus app

bool	restore_spi;
bool	mouseinitialized;
int	originalmouseparms[3], newmouseparms[3] = {0, 0, 1};
bool	mouseparmsvalid;

int	window_center_x, window_center_y;
RECT	window_rect;

/*
============================================================

  MOUSE CONTROL

============================================================
*/

// mouse variables


bool	mlooking;

void IN_MLookDown (void)
{
	mlooking = true;
}

void IN_MLookUp (void)
{
	mlooking = false;
	if(!freelook->value && lookspring->value)
		IN_CenterView ();
}




/*
===========
IN_ActivateMouse

Called when the window gains focus or changes in some way
===========
*/
void IN_ActivateMouse (void)
{
	int	width, height;

	if(!mouseinitialized) return;

	if(!m_mouse->integer)
	{
		mouseactive = false;
		return;
	}

	if(mouseactive) return;

	mouseactive = true;

	if( mouseparmsvalid ) restore_spi = SystemParametersInfo (SPI_SETMOUSE, 0, newmouseparms, 0);

	width = GetSystemMetrics(SM_CXSCREEN);
	height = GetSystemMetrics(SM_CYSCREEN);

	GetWindowRect( host.hWnd, &window_rect);
	if (window_rect.left < 0) window_rect.left = 0;
	if (window_rect.top < 0) window_rect.top = 0;
	if (window_rect.right >= width) window_rect.right = width - 1;
	if (window_rect.bottom >= height-1) window_rect.bottom = height - 1;

	window_center_x = (window_rect.right + window_rect.left)/2;
	window_center_y = (window_rect.top + window_rect.bottom)/2;
	SetCursorPos (window_center_x, window_center_y);

	SetCapture( host.hWnd );
	ClipCursor(&window_rect);
	while( ShowCursor(false) >= 0 );
}


/*
===========
IN_DeactivateMouse

Called when the window loses focus
===========
*/
void IN_DeactivateMouse (void)
{
	if(!mouseinitialized || !mouseactive)
		return;

	if( restore_spi ) SystemParametersInfo (SPI_SETMOUSE, 0, originalmouseparms, 0);

	mouseactive = false;

	ClipCursor( NULL );
	ReleaseCapture();
	while( ShowCursor(true) < 0 );
}



/*
===========
IN_StartupMouse
===========
*/
void IN_StartupMouse (void)
{
	cvar_t		*cv;

	cv = Cvar_Get ("in_initmouse", "1", CVAR_INIT);
	if ( !cv->value ) return; 

	mouseinitialized = true;
	mouseparmsvalid = SystemParametersInfo(SPI_GETMOUSE, 0, originalmouseparms, 0);
	mouse_buttons = 3;
}

/*
===========
IN_MouseEvent
===========
*/
void IN_MouseEvent (int mstate)
{
	int		i;

	if (!mouseinitialized)
		return;

	// perform button actions
	for (i=0 ; i<mouse_buttons ; i++)
	{
		if ( (mstate & (1<<i)) &&
			!(mouse_oldbuttonstate & (1<<i)) )
		{
			Key_Event (K_MOUSE1 + i, true, host.sv_timer);
		}

		if ( !(mstate & (1<<i)) &&
			(mouse_oldbuttonstate & (1<<i)) )
		{
				Key_Event (K_MOUSE1 + i, false, host.sv_timer);
		}
	}	
		
	mouse_oldbuttonstate = mstate;
}


/*
===========
CL_MouseMove
===========
*/
void CL_MouseMove (usercmd_t *cmd)
{
	int		mx, my;

	if (!mouseactive)
		return;

	// find mouse movement
	if (!GetCursorPos (&current_pos))
		return;

	mx = current_pos.x - window_center_x;
	my = current_pos.y - window_center_y;

#if 0
	if (!mx && !my)
		return;
#endif

	if (m_filter->value)
	{
		mouse_x = (mx + old_mouse_x) * 0.5;
		mouse_y = (my + old_mouse_y) * 0.5;
	}
	else
	{
		mouse_x = mx;
		mouse_y = my;
	}

	old_mouse_x = mx;
	old_mouse_y = my;

	mouse_x *= sensitivity->value;
	mouse_y *= sensitivity->value;

	// add mouse X/Y movement to cmd
	if ( (in_strafe.state & 1) || (lookstrafe->value && mlooking ))
		cmd->sidemove += m_side->value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw->value * mouse_x;

	if ( (mlooking || freelook->value) && !(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch->value * mouse_y;
	}
	else
	{
		cmd->forwardmove -= m_forward->value * mouse_y;
	}

	// force the mouse to the center, so there's room to move
	if (mx || my) SetCursorPos (window_center_x, window_center_y);
}


/*
=========================================================================

VIEW CENTERING

=========================================================================
*/


/*
===========
IN_Activate

Called when the main window gains or loses focus.
The window may have been destroyed and recreated
between a deactivate and an activate.
===========
*/
void IN_Activate (bool active)
{
	in_appactive = active;
	mouseactive = !active; // force a new window check or turn off
}


/*
==================
CL_UpdateMouse

Called every frame, even if not generating commands
==================
*/
void CL_UpdateMouse( void )
{
	if(!mouseinitialized) return;

	if(!m_mouse || !in_appactive)
	{
		IN_DeactivateMouse ();
		return;
	}

	if( !cl.refresh_prepped || cls.key_dest == key_console || cls.key_dest == key_menu )
	{
		// temporarily deactivate if in fullscreen
		if(!Cvar_VariableValue ("r_fullscreen"))
		{
			IN_DeactivateMouse ();
			return;
		}
	}
	IN_ActivateMouse ();
}

/*
===================
IN_ClearStates
===================
*/
void IN_ClearStates (void)
{
	mx_accum = 0;
	my_accum = 0;
	mouse_oldbuttonstate = 0;
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


Key_Event (int key, bool down, unsigned time);

  +mlook src time

===============================================================================
*/


kbutton_t	in_klook;
kbutton_t	in_left, in_right, in_forward, in_back;
kbutton_t	in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t	in_strafe, in_speed, in_use, in_attack;
kbutton_t	in_up, in_down;

int in_impulse;

void KeyDown (kbutton_t *b)
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
	b->downtime = atoi(c);

	if (!b->downtime) b->downtime = host.cl_timer - 100;
	b->state |= 1 + 2;	// down + impulse down
}

void KeyUp (kbutton_t *b)
{
	int	k;
	char	*c;
	uint	uptime;

	c = Cmd_Argv(1);
	if (c[0]) k = atoi(c);
	else
	{
		// typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state = 4; // impulse up
		return;
	}

	if (b->down[0] == k) b->down[0] = 0;
	else if (b->down[1] == k) b->down[1] = 0;
	else return; // key up without coresponding down (menu pass through)

	if (b->down[0] || b->down[1])
		return; // some other key is still holding it down

	if (!(b->state & 1))
		return; // still up (this should not happen)

	// save timestamp
	c = Cmd_Argv(2);
	uptime = atoi(c);
	if (uptime) b->msec += uptime - b->downtime;
	else b->msec += 10;

	b->state &= ~1; // now up
	b->state |= 4; // impulse up
}

void IN_KLookDown (void) {KeyDown(&in_klook);}
void IN_KLookUp (void) {KeyUp(&in_klook);}
void IN_UpDown(void) {KeyDown(&in_up);}
void IN_UpUp(void) {KeyUp(&in_up);}
void IN_DownDown(void) {KeyDown(&in_down);}
void IN_DownUp(void) {KeyUp(&in_down);}
void IN_LeftDown(void) {KeyDown(&in_left);}
void IN_LeftUp(void) {KeyUp(&in_left);}
void IN_RightDown(void) {KeyDown(&in_right);}
void IN_RightUp(void) {KeyUp(&in_right);}
void IN_ForwardDown(void) {KeyDown(&in_forward);}
void IN_ForwardUp(void) {KeyUp(&in_forward);}
void IN_BackDown(void) {KeyDown(&in_back);}
void IN_BackUp(void) {KeyUp(&in_back);}
void IN_LookupDown(void) {KeyDown(&in_lookup);}
void IN_LookupUp(void) {KeyUp(&in_lookup);}
void IN_LookdownDown(void) {KeyDown(&in_lookdown);}
void IN_LookdownUp(void) {KeyUp(&in_lookdown);}
void IN_MoveleftDown(void) {KeyDown(&in_moveleft);}
void IN_MoveleftUp(void) {KeyUp(&in_moveleft);}
void IN_MoverightDown(void) {KeyDown(&in_moveright);}
void IN_MoverightUp(void) {KeyUp(&in_moveright);}

void IN_SpeedDown(void) {KeyDown(&in_speed);}
void IN_SpeedUp(void) {KeyUp(&in_speed);}
void IN_StrafeDown(void) {KeyDown(&in_strafe);}
void IN_StrafeUp(void) {KeyUp(&in_strafe);}

void IN_AttackDown(void) {KeyDown(&in_attack);}
void IN_AttackUp(void) {KeyUp(&in_attack);}

void IN_UseDown (void) {KeyDown(&in_use);}
void IN_UseUp (void) {KeyUp(&in_use);}

void IN_Impulse (void) {in_impulse=atoi(Cmd_Argv(1));}

/*
===============
CL_KeyState

Returns the fraction of the frame that the key was down
===============
*/
float CL_KeyState (kbutton_t *key)
{
	float		val;
	int			msec;

	key->state &= 1;		// clear impulses

	msec = key->msec;
	key->msec = 0;

	if (key->state)
	{	// still down
		msec += host.cl_timer - key->downtime;
		key->downtime = host.cl_timer;
	}

#if 0
	if (msec)
	{
		Msg ("%i ", msec);
	}
#endif

	val = (float)msec / frame_msec;
	if (val < 0)
		val = 0;
	if (val > 1)
		val = 1;

	return val;
}




//==========================================================================

cvar_t	*cl_upspeed;
cvar_t	*cl_forwardspeed;
cvar_t	*cl_sidespeed;

cvar_t	*cl_yawspeed;
cvar_t	*cl_pitchspeed;

cvar_t	*cl_run;

cvar_t	*cl_anglespeedkey;


/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
void CL_AdjustAngles( void )
{
	float	speed;
	float	up, down;
	
	if (in_speed.state & 1)
		speed = cls.frametime * cl_anglespeedkey->value;
	else
		speed = cls.frametime;

	if (!(in_strafe.state & 1))
	{
		cl.viewangles[YAW] -= speed*cl_yawspeed->value*CL_KeyState (&in_right);
		cl.viewangles[YAW] += speed*cl_yawspeed->value*CL_KeyState (&in_left);
	}
	if (in_klook.state & 1)
	{
		cl.viewangles[PITCH] -= speed*cl_pitchspeed->value * CL_KeyState (&in_forward);
		cl.viewangles[PITCH] += speed*cl_pitchspeed->value * CL_KeyState (&in_back);
	}
	
	up = CL_KeyState (&in_lookup);
	down = CL_KeyState(&in_lookdown);
	
	cl.viewangles[PITCH] -= speed*cl_pitchspeed->value * up;
	cl.viewangles[PITCH] += speed*cl_pitchspeed->value * down;
}

/*
================
CL_BaseMove

Send the intended movement message to the server
================
*/
void CL_BaseMove (usercmd_t *cmd)
{	
	CL_AdjustAngles ();
	
	memset (cmd, 0, sizeof(*cmd));
	
	VectorCopy (cl.viewangles, cmd->angles);
	if (in_strafe.state & 1)
	{
		cmd->sidemove += cl_sidespeed->value * CL_KeyState (&in_right);
		cmd->sidemove -= cl_sidespeed->value * CL_KeyState (&in_left);
	}

	cmd->sidemove += cl_sidespeed->value * CL_KeyState (&in_moveright);
	cmd->sidemove -= cl_sidespeed->value * CL_KeyState (&in_moveleft);

	cmd->upmove += cl_upspeed->value * CL_KeyState (&in_up);
	cmd->upmove -= cl_upspeed->value * CL_KeyState (&in_down);

	if (! (in_klook.state & 1) )
	{	
		cmd->forwardmove += cl_forwardspeed->value * CL_KeyState (&in_forward);
		cmd->forwardmove -= cl_forwardspeed->value * CL_KeyState (&in_back);
	}	

//
// adjust for speed key / running
//
	if ( (in_speed.state & 1) ^ (int)(cl_run->value) )
	{
		cmd->forwardmove *= 2;
		cmd->sidemove *= 2;
		cmd->upmove *= 2;
	}	
}

void CL_ClampPitch (void)
{
	float	pitch;

	pitch = SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[PITCH]);
	if (pitch > 180)
		pitch -= 360;

	if (cl.viewangles[PITCH] + pitch < -360)
		cl.viewangles[PITCH] += 360; // wrapped
	if (cl.viewangles[PITCH] + pitch > 360)
		cl.viewangles[PITCH] -= 360; // wrapped

	if (cl.viewangles[PITCH] + pitch > 89)
		cl.viewangles[PITCH] = 89 - pitch;
	if (cl.viewangles[PITCH] + pitch < -89)
		cl.viewangles[PITCH] = -89 - pitch;
}

/*
==============
CL_FinishMove
==============
*/
void CL_FinishMove (usercmd_t *cmd)
{
	int		ms;
	int		i;

//
// figure button bits
//	
	if ( in_attack.state & 3 )
		cmd->buttons |= BUTTON_ATTACK;
	in_attack.state &= ~2;
	
	if (in_use.state & 3)
		cmd->buttons |= BUTTON_USE;
	in_use.state &= ~2;

	if (anykeydown && cls.key_dest == key_game)
		cmd->buttons |= BUTTON_ANY;

	// send milliseconds of time to apply the move
	ms = cls.frametime * 1000;
	if (ms > 250)
		ms = 100;		// time was unreasonable
	cmd->msec = ms;

	CL_ClampPitch ();
	for (i=0 ; i<3 ; i++)
		cmd->angles[i] = ANGLE2SHORT(cl.viewangles[i]);

	cmd->impulse = in_impulse;
	in_impulse = 0;

// send the ambient light level at the player's current position
	cmd->lightlevel = (byte)cl_lightlevel->value;
}

/*
=================
CL_CreateCmd
=================
*/
usercmd_t CL_CreateCmd (void)
{
	usercmd_t	cmd;

	frame_msec = host.cl_timer - old_sys_frame_time;
	if (frame_msec < 1) frame_msec = 1;
	if (frame_msec > 200) frame_msec = 200;
	
	// get basic movement from keyboard
	CL_BaseMove(&cmd);
	CL_MouseMove(&cmd);

	CL_FinishMove (&cmd);

	old_sys_frame_time = host.cl_timer;

	//cmd.impulse = cls.framecount;

	return cmd;
}


void IN_CenterView (void)
{
	cl.viewangles[PITCH] = -SHORT2ANGLE(cl.frame.playerstate.pmove.delta_angles[PITCH]);
}

/*
============
CL_InitInput
============
*/
void CL_InitInput (void)
{
	// mouse variables
	m_filter = Cvar_Get("m_filter", "0", 0);
	m_mouse = Cvar_Get("mouse", "1", CVAR_ARCHIVE);

	// centering
	v_centermove = Cvar_Get ("v_centermove", "0.15", 0);
	v_centerspeed = Cvar_Get ("v_centerspeed", "500",	0);
	cl_nodelta = Cvar_Get ("cl_nodelta", "0", 0);

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
	Cmd_AddCommand ("+use", IN_UseDown, "use item (doors, monsters, inventory, etc" );
	Cmd_AddCommand ("-use", IN_UseUp, "stop using item" );
	Cmd_AddCommand ("impulse", IN_Impulse, "send an impulse number to server (select weapon, use item, etc)");
	Cmd_AddCommand ("+klook", IN_KLookDown, "activate keyboard looking mode, do not recenter view");
	Cmd_AddCommand ("-klook", IN_KLookUp, "deactivate keyboard looking mode");
	Cmd_AddCommand ("+mlook", IN_MLookDown, "activate mouse looking mode, do not recenter view" );
	Cmd_AddCommand ("-mlook", IN_MLookUp, "deactivate mouse looking mode" );

	IN_StartupMouse(); // init mouse
}

/*
============
CL_ShutdownInput
============
*/
void CL_ShutdownInput( void )
{
	IN_DeactivateMouse();
}

/*
=================
CL_SendCmd
=================
*/
void CL_SendCmd (void)
{
	sizebuf_t		buf;
	byte		data[128];
	int		i;
	usercmd_t		*cmd, *oldcmd;
	usercmd_t		nullcmd;
	int		checksumIndex;

	// build a command even if not connected

	// save this command off for prediction
	i = cls.netchan.outgoing_sequence & (CMD_BACKUP-1);
	cmd = &cl.cmds[i];
	cl.cmd_time[i] = cls.realtime; // for netgraph ping calculation

	*cmd = CL_CreateCmd ();

	cl.cmd = *cmd;

	SZ_Init (&buf, data, sizeof(data));

	if(cls.state == ca_disconnected || cls.state == ca_connecting)
		return;

	// ignore commands for demo mode
	if(cls.state == ca_cinematic || cl.servercount > 0x10000)
		return;

	if( cls.state == ca_connected)
	{
		if (cls.netchan.message.cursize || host.realtime - cls.netchan.last_sent > 1.0f )
			Netchan_Transmit (&cls.netchan, 0, buf.data);	
		return;
	}

	// send a userinfo update if needed
	if (userinfo_modified)
	{
		CL_FixUpGender();
		userinfo_modified = false;
		MSG_WriteByte (&cls.netchan.message, clc_userinfo);
		MSG_WriteString (&cls.netchan.message, Cvar_Userinfo() );
	}

	// begin a client move command
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
	buf.data[checksumIndex] = CRC_Sequence(
		buf.data + checksumIndex + 1, buf.cursize - checksumIndex - 1,
		cls.netchan.outgoing_sequence);

	//
	// deliver the message
	//
	Netchan_Transmit (&cls.netchan, buf.cursize, buf.data);	
}


