//=======================================================================
//			Copyright XashXT Group 2009 ©
//	input.cpp - builds an intended movement command to send to the server
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "usercmd.h"
#include "hud.h"

#define mlook_active	(freelook->integer || (in_mlook.state & 1))

typedef struct
{
	uint	msec;		// msec down this frame if both a down and up happened
	uint	downtime;		// msec timestamp
	int	down[2];		// key nums holding it down
	int	state;
} kbutton_t;

cvar_t	*cl_lw;
cvar_t	*cl_run;
cvar_t	*cl_upspeed;
cvar_t	*cl_yawspeed;
cvar_t	*cl_sidespeed;
cvar_t	*cl_pitchspeed;
cvar_t	*cl_forwardspeed;
cvar_t	*cl_backspeed;
cvar_t	*cl_anglespeedkey;
cvar_t	*cl_movespeedkey;
cvar_t	*cl_pitchup;
cvar_t	*cl_pitchdown;
cvar_t	*v_centermove;
cvar_t	*v_centerspeed;
cvar_t	*cl_mouseaccel;
cvar_t	*cl_particlelod;
cvar_t	*cl_particles;

cvar_t	*m_sensitivity;
cvar_t	*m_filter;		// mouse filtering
cvar_t	*m_pitch;
cvar_t	*m_yaw;
cvar_t	*m_forward;
cvar_t	*m_side;

cvar_t	*lookspring;
cvar_t	*lookstrafe;
cvar_t	*freelook;

kbutton_t	in_mlook, in_klook, in_left, in_right, in_forward, in_back;
kbutton_t	in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t	in_strafe, in_speed, in_use, in_jump, in_attack, in_attack2;
kbutton_t	in_up, in_down, in_duck, in_reload, in_alt1, in_score, in_break;
int in_cancel = 0, in_impulse = 0;

int		g_iAlive;		// indicates alive local client or not
static int	mouse_x[2];	// use two values for filtering
static int	mouse_y[2];
static int	mouse_step;
static int	frame_msec;	// input frame msecs
static int	frametime;	// used by CL_KeyState
static float	cl_viewangles[3];	// process viewangles
static float	cl_oldviewangles[3];
	
/*
============
IN_MouseEvent

accumulate mouse move between calls
============
*/
void IN_MouseEvent( int mx, int my )
{
	// accumulate mouse moves
	mouse_x[mouse_step] += mx;
	mouse_y[mouse_step] += my;
}

/*
============
IN_KeyEvent

Return 1 to allow engine to process the key, otherwise, act on it as needed
============
*/
int IN_KeyEvent( int down, int keynum, const char *pszBind )
{
	return 1;
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
	const char *c;
	int k;	

	c = CMD_ARGV( 1 );

	if( c[0] )
	{
		k = atoi( c );
	}
	else
	{
		k = -1; // typed manually at the console for continuous down
	}

	if( k == b->down[0] || k == b->down[1] )
		return; // repeating key
	
	if ( !b->down[0] )
	{
		b->down[0] = k;
	}
	else if ( !b->down[1] )
	{
		b->down[1] = k;
	}
	else
	{
		ALERT( at_error, "three keys down for a button '%c' '%c' '%c'!\n", b->down[0], b->down[1], c );
		return;
	}
	
	if( b->state & 1 ) return; // still down

	// save timestamp
	c = CMD_ARGV( 2 );
	b->downtime = atoi( c );

	b->state |= 1 + 2;	// down + impulse down
}

void IN_KeyUp( kbutton_t *b )
{
	int		k;
	const char	*c;
	uint		uptime;

	c = CMD_ARGV( 1 );

	if( c[0] )
	{
		k = atoi( c );
	}
	else
	{
		// typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state = 4; // impulse up
		return;
	}

	if( b->down[0] == k )
	{
		b->down[0] = 0;
	}
	else if( b->down[1] == k )
	{
		b->down[1] = 0;
	}
	else return; // key up without coresponding down (menu pass through)

	if( b->down[0] || b->down[1] )
		return; // some other key is still holding it down

	if( !( b->state & 1 )) return; // still up (this should not happen)

	// save timestamp for partial frame summing
	c = CMD_ARGV( 2 );
	uptime = atoi( c );

	if( uptime )
	{
		b->msec += uptime - b->downtime;
	}
	else
	{
		b->msec += frame_msec / 2;
	}

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
static float CL_KeyState( kbutton_t *key )
{
	float	val;
	int	msec;

	key->state &= 1;	// clear impulses

	msec = key->msec;
	key->msec = 0;

	if( key->state )
	{
		// still down
		if( !key->downtime ) msec = frametime;
		else msec += frametime - key->downtime;
		key->downtime = frametime;
	}

#if 0
	if( msec )
	{
		ALERT( at_console, "%i ", msec );
	}
#endif
	val = bound( 0, (float)msec / frame_msec, 1 );

	return val;
}

void IN_KLookDown( void ){ IN_KeyDown( &in_klook ); }
void IN_KLookUp( void ){ IN_KeyUp( &in_klook ); }
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
void IN_JumpDown(void) {IN_KeyDown( &in_jump ); }
void IN_JumpUp(void) {IN_KeyUp( &in_jump ); }
void IN_DuckDown(void){ IN_KeyDown( &in_duck ); }
void IN_DuckUp(void){ IN_KeyUp( &in_duck ); }
void IN_Alt1Down(void){ IN_KeyDown( &in_alt1 ); }
void IN_Alt1Up(void) {IN_KeyUp( &in_alt1 ); }
void IN_ScoreDown(void) {IN_KeyDown( &in_score ); }
void IN_ScoreUp(void) {IN_KeyUp( &in_score ); }
void IN_BreakDown(void) {IN_KeyDown( &in_break ); }
void IN_BreakUp(void) {IN_KeyUp( &in_break ); }
void IN_Cancel(void) { in_cancel = 1; } // special handling
void IN_Impulse(void) { in_impulse = atoi( CMD_ARGV( 1 )); }
void IN_MLookDown(void) {IN_KeyDown( &in_mlook ); }

void IN_CenterView( void )
{
	if( !mlook_active )
	{
		V_StartPitchDrift ();
	}
	else
	{
		float	viewangles[3];

		// just reset pitch
		GetViewAngles( viewangles );
		viewangles[PITCH] = 0.0f;
		SetViewAngles( viewangles );
	}
}

void IN_MLookUp(void)
{
	IN_KeyUp( &in_mlook );

	if( !mlook_active && lookspring->integer )
	{
		V_StopPitchDrift ();
	}
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
		mx = (mouse_x[0] + mouse_x[1]) * 0.5f;
		my = (mouse_y[0] + mouse_y[1]) * 0.5f;
	}
	else
	{
		mx = mouse_x[mouse_step];
		my = mouse_y[mouse_step];
	}

	mouse_step ^= 1;
	mouse_x[mouse_step] = 0;
	mouse_y[mouse_step] = 0;

	rate = sqrt( mx * mx + my * my ) / (float)frame_msec;

	// check for dead
	if( !g_iAlive ) return;

	accel_sensitivity = m_sensitivity->value + rate * cl_mouseaccel->value;
	if ( gHUD.GetSensitivity( )) accel_sensitivity *= gHUD.GetSensitivity(); // scale by fov

	mx *= accel_sensitivity;
	my *= accel_sensitivity;

	// add mouse X/Y movement to cmd
	if(( in_strafe.state & 1 ) || (lookstrafe->integer && mlook_active))
		cmd->sidemove += m_side->value * mx;
	else cl_viewangles[YAW] -= m_yaw->value * mx;

	if( mlook_active ) V_StopPitchDrift ();		

	if( mlook_active && !( in_strafe.state & 1 ))
	{
		cl_viewangles[PITCH] += m_pitch->value * my;
		cl_viewangles[PITCH] = bound( -cl_pitchup->value, cl_viewangles[PITCH], cl_pitchdown->value );
	}
	else
	{
		if(( in_strafe.state & 1 ) && gHUD.IsNoClipping() )
			cmd->upmove -= m_forward->value * my;
		else cmd->forwardmove -= m_forward->value * my;
	}
}

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

	if( !g_iAlive ) return;

	if ( in_speed.state & 1 )
	{
		speed = gpGlobals->frametime * cl_anglespeedkey->value;
	}
	else
	{
		speed = gpGlobals->frametime;
	}

	if (!( in_strafe.state & 1 ))
	{
		cl_viewangles[YAW] -= speed * cl_yawspeed->value * CL_KeyState( &in_right );
		cl_viewangles[YAW] += speed * cl_yawspeed->value * CL_KeyState( &in_left );
		cl_viewangles[YAW] = anglemod( cl_viewangles[YAW] );
	}

	if( in_klook.state & 1 )
	{
		V_StopPitchDrift ();

		cl_viewangles[PITCH] -= speed * cl_pitchspeed->value * CL_KeyState( &in_forward );
		cl_viewangles[PITCH] += speed * cl_pitchspeed->value * CL_KeyState( &in_back );
	}

	up = CL_KeyState( &in_lookup );
	down = CL_KeyState( &in_lookdown );
	
	cl_viewangles[PITCH] -= speed * cl_pitchspeed->value * up;
	cl_viewangles[PITCH] += speed * cl_pitchspeed->value * down;

	if( up || down ) V_StopPitchDrift();

	cl_viewangles[PITCH] = bound( -70, cl_viewangles[PITCH], 80 );
	cl_viewangles[ROLL] = bound( -50, cl_viewangles[ROLL], 50 );	
}

/*
================
CL_BaseMove

Send the intended movement message to the server
================
*/
void CL_BaseMove( usercmd_t *cmd )
{	
	if( in_strafe.state & 1 )
	{
		cmd->sidemove += cl_sidespeed->value * CL_KeyState( &in_right );
		cmd->sidemove -= cl_sidespeed->value * CL_KeyState( &in_left );
	}

	cmd->sidemove += cl_sidespeed->value * CL_KeyState( &in_moveright );
	cmd->sidemove -= cl_sidespeed->value * CL_KeyState( &in_moveleft );

	cmd->upmove += cl_upspeed->value * CL_KeyState( &in_up );
	cmd->upmove -= cl_upspeed->value * CL_KeyState( &in_down );

	if(!( in_klook.state & 1 ))
	{	
		cmd->forwardmove += cl_forwardspeed->value * CL_KeyState( &in_forward );
		cmd->forwardmove -= cl_backspeed->value * CL_KeyState( &in_back );
	}

	// adjust for speed key / running
	if( in_speed.state & 1 ^ cl_run->integer )
	{
		cmd->forwardmove *= cl_movespeedkey->value;
		cmd->sidemove *= cl_movespeedkey->value;
		cmd->upmove *= cl_movespeedkey->value;
	}

	// clip to maxspeed
	float spd = GetClientMaxspeed();

	if( spd != 0.0f )
	{
		// scale the 3 speeds so that the total velocity is not > cl.maxspeed
		float	fmove = 0;

		fmove += (cmd->forwardmove * cmd->forwardmove);
		fmove += (cmd->sidemove * cmd->sidemove);
		fmove += (cmd->upmove * cmd->upmove);
		fmove = sqrt( fmove );

		if ( fmove > spd )
		{
			float	fratio = spd / fmove;

			cmd->forwardmove *= fratio;
			cmd->sidemove *= fratio;
			cmd->upmove *= fratio;
		}
	}
}

void IN_CreateMove( usercmd_t *cmd, int host_frametime, int active )
{
	static int	last_frametime = 0;	// from previous frame

	frametime = host_frametime;
	frame_msec = bound( 1, frametime - last_frametime, 200 );

	g_iAlive = gHUD.UpdateClientData();

	if ( active )
	{
		GetViewAngles( cl_viewangles );

		CL_AdjustAngles( );

		CL_BaseMove( cmd );

		// allow mice and other controllers to add their inputs
		CL_MouseMove( cmd );

		SetViewAngles( cl_viewangles );
	}

	cmd->impulse = in_impulse;
	in_impulse = 0;

	cmd->weaponselect = g_weaponselect;
	g_weaponselect = 0;

	// set button and flag bits
	cmd->buttons = CL_ButtonBits( 1 );

	// random seed for predictable random values
	cmd->random_seed = RANDOM_LONG( 0, 0x7fffffff ); // full range

	last_frametime = frametime;
	
	GetViewAngles( cl_viewangles );

	// set current view angles.
	if ( g_iAlive )
	{
		cmd->viewangles[0] = cl_oldviewangles[0] = cl_viewangles[0]; 
		cmd->viewangles[1] = cl_oldviewangles[1] = cl_viewangles[1]; 
		cmd->viewangles[2] = cl_oldviewangles[2] = cl_viewangles[2]; 

		if( freelook->integer )
		{
			if( !mlook_active && lookspring->value )
				V_StartPitchDrift();
		}
	}
	else
	{
		cmd->viewangles[0] = cl_oldviewangles[0];
		cmd->viewangles[1] = cl_oldviewangles[1];
		cmd->viewangles[2] = cl_oldviewangles[2];
	}
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

	if( in_speed.state & 3 )
		bits |= IN_RUN;

	if( in_duck.state & 3 )
		bits |= IN_DUCK;

	if( in_down.state & 3 )
		bits |= IN_DUCK;

	if( in_jump.state & 3 )
		bits |= IN_JUMP;

	if( in_up.state & 3 )
		bits |= IN_JUMP;
		
	if( in_reload.state & 3)
		bits |= IN_RELOAD;

	if( in_forward.state & 3 )
		bits |= IN_FORWARD;

	if( in_back.state & 3 )
		bits |= IN_BACK;

	if( in_left.state & 3 )
		bits |= IN_LEFT;
	
	if( in_right.state & 3 )
		bits |= IN_RIGHT;
	
	if( in_moveleft.state & 3 )
		bits |= IN_MOVELEFT;
	
	if( in_moveright.state & 3 )
		bits |= IN_MOVERIGHT;

	if( in_alt1.state & 3 )
		bits |= IN_ALT1;

	if( in_score.state & 3 )
		bits |= IN_SCORE;

	if( in_cancel )
		bits |= IN_CANCEL;

	if( bResetState )
	{
		in_speed.state &= ~2;
		in_attack.state &= ~2;
		in_duck.state &= ~2;
		in_jump.state &= ~2;
		in_forward.state &= ~2;
		in_back.state &= ~2;
		in_use.state &= ~2;
		in_left.state &= ~2;
		in_right.state &= ~2;
		in_up.state &= ~2;
		in_down.state &= ~2;
		in_moveleft.state &= ~2;
		in_moveright.state &= ~2;
		in_attack2.state &= ~2;
		in_reload.state &= ~2;
		in_alt1.state &= ~2;
		in_score.state &= ~2;
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

void IN_Init( void )
{
	// mouse variables
	m_filter = CVAR_REGISTER("m_filter", "0", FCVAR_ARCHIVE, "enable mouse filter" );
	m_sensitivity = CVAR_REGISTER( "m_sensitivity", "3", FCVAR_ARCHIVE, "mouse in-game sensitivity" );
	cl_mouseaccel = CVAR_REGISTER( "cl_mouseaccelerate", "0", FCVAR_ARCHIVE, "mouse accelerate factor" ); 

	// centering
	v_centermove = CVAR_REGISTER ("v_centermove", "0.15", 0, "client center moving" );
	v_centerspeed = CVAR_REGISTER ("v_centerspeed", "500", 0, "client center speed" );

	cl_upspeed = CVAR_REGISTER( "cl_upspeed", "400", 0, "client upspeed limit" );
	cl_forwardspeed = CVAR_REGISTER( "cl_forwardspeed", "400", 0, "client forward speed limit" );
	cl_backspeed = CVAR_REGISTER( "cl_backspeed", "400", 0, "client bask speed limit" );
	cl_sidespeed = CVAR_REGISTER( "cl_sidespeed", "400", 0, "client side-speed limit" );
	cl_yawspeed = CVAR_REGISTER( "cl_yawspeed", "140", 0, "client yaw speed" );
	cl_pitchspeed = CVAR_REGISTER( "cl_pitchspeed", "150", 0, "client pitch speed" );
	cl_anglespeedkey = CVAR_REGISTER( "cl_anglespeedkey", "1.5", 0, "client anglespeed" );
	cl_run = CVAR_REGISTER( "cl_run", "0", FCVAR_ARCHIVE, "keep client for always run mode" );
	cl_lw = CVAR_REGISTER( "cl_lw", "1", FCVAR_ARCHIVE|FCVAR_USERINFO, "enable client weapon predicting" );

	cl_movespeedkey = CVAR_REGISTER ( "cl_movespeedkey", "2", 0, "global scale factor between run and walk" );
	cl_pitchup = CVAR_REGISTER ( "cl_pitchup", "70", 0, "how many pitch up" );
	cl_pitchdown = CVAR_REGISTER ( "cl_pitchdown", "80", 0, "how many pitch down" );

	freelook = CVAR_REGISTER( "freelook", "1", FCVAR_ARCHIVE, "enables mouse look" );
	lookspring = CVAR_REGISTER( "lookspring", "0", FCVAR_ARCHIVE, "allow look spring" );
	lookstrafe = CVAR_REGISTER( "lookstrafe", "0", FCVAR_ARCHIVE, "allow look strafe" );

	m_pitch = CVAR_REGISTER ("m_pitch", "0.022", FCVAR_ARCHIVE, "mouse pitch value" );
	m_yaw = CVAR_REGISTER ("m_yaw", "0.022", 0, "mouse yaw value" );
	m_forward = CVAR_REGISTER ("m_forward", "1", 0, "mouse forward speed" );
	m_side = CVAR_REGISTER ("m_side", "1", 0, "mouse side speed" );

	cl_particles = CVAR_REGISTER ( "cl_particles", "1", FCVAR_ARCHIVE, "disables particle effects" );
	cl_particlelod = CVAR_REGISTER ( "cl_lod_particle", "0", FCVAR_ARCHIVE, "enables particle LOD (1, 2, 3)" );
	
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
	Cmd_AddCommand ("+use", IN_UseDown, "use item (doors, monsters, inventory, etc)" );
	Cmd_AddCommand ("-use", IN_UseUp, "stop using item" );
	Cmd_AddCommand ("+jump", IN_JumpDown, "jump" );
	Cmd_AddCommand ("-jump", IN_JumpUp, "end jump (so you can jump again)");
	Cmd_AddCommand ("+duck", IN_DuckDown, "duck" );
	Cmd_AddCommand ("-duck", IN_DuckUp, "end duck (so you can duck again)");
	Cmd_AddCommand ("+klook", IN_KLookDown, "activate keyboard looking mode, do not recenter view");
	Cmd_AddCommand ("-klook", IN_KLookUp, "deactivate keyboard looking mode");
	Cmd_AddCommand ("+reload", IN_ReloadDown, "reload current weapon" );
	Cmd_AddCommand ("-reload", IN_ReloadUp, "continue reload weapon" );
	Cmd_AddCommand ("+mlook", IN_MLookDown, "activate mouse looking mode, do not recenter view" );
	Cmd_AddCommand ("-mlook", IN_MLookUp, "deactivate mouse looking mode" );
	Cmd_AddCommand ("+alt1", IN_Alt1Down, "hold modyifycator" );
	Cmd_AddCommand ("-alt1", IN_Alt1Up, "release modifycator" );
	Cmd_AddCommand ("+break",IN_BreakDown, "cancel" );
	Cmd_AddCommand ("-break",IN_BreakUp, "stop cancel" );
	Cmd_AddCommand ( "impulse", IN_Impulse, "send impulse to a client" );

	V_Init ();
}

void IN_Shutdown( void )
{
	Cmd_RemoveCommand ("centerview" );
	Cmd_RemoveCommand ("impulse" );

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