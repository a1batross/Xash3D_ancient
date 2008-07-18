#include "common.h"
#include "server.h"

edict_t	*pm_passent;

// pmove doesn't need to know about passent and contentmask
trace_t PM_trace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	if( pm_passent->progs.sv->health > 0 )
		return SV_Trace (start, mins, maxs, end, pm_passent, MASK_PLAYERSOLID);
	return SV_Trace (start, mins, maxs, end, pm_passent, MASK_DEADSOLID);
}

int PM_pointcontents( vec3_t point )
{
	return SV_PointContents( point, pm_passent );
}

/*
============
SV_TouchTriggers

============
*/
void SV_TouchTriggers (edict_t *ent)
{
	int		i, num;
	edict_t		**touch, *hit;

	// list of pointers, not data
	touch = Z_Malloc( sizeof(*touch) * host.max_edicts );

	// dead things don't activate triggers!
	if ((ent->priv.sv->client || ((int)ent->progs.sv->flags & FL_MONSTER)) && (ent->progs.sv->health <= 0))
		return;

	num = SV_AreaEdicts(ent->progs.sv->absmin, ent->progs.sv->absmax, touch, host.max_edicts );

	PRVM_PUSH_GLOBALS;

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for (i = 0; i < num; i++)
	{
		hit = touch[i];
		if (hit->priv.sv->free) continue;

		prog->globals.sv->pev = PRVM_EDICT_TO_PROG(hit);
		prog->globals.sv->other = PRVM_EDICT_TO_PROG(ent);
		prog->globals.sv->time = sv.time;
		if( hit->progs.sv->touch )
		{
			PRVM_ExecuteProgram (hit->progs.sv->touch, "pev->touch");
		}
	}
	Mem_Free( touch );

	// restore state
	PRVM_POP_GLOBALS;
}

static edict_t	*current_player;
static sv_client_t	*current_client;
static vec3_t	forward, right, up;
float		xyspeed, bobmove, bobfracsin;	// sin(bobfrac*M_PI)
int		bobcycle;			// odd cycles are right foot going forward


/*
===============
SV_CalcRoll

===============
*/
float SV_CalcRoll (vec3_t angles, vec3_t velocity)
{
	float	sign;
	float	side;
	float	value;
	
	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs(side);
	
	value = 2;

	if (side < 200) side = side * value / 200;
	else side = value;
	
	return side*sign;
	
}

/*
==============
SV_CalcGunOffset
==============
*/
void SV_CalcGunOffset( edict_t *ent )
{
	int		i;
	float		delta;

	// gun angles from bobbing
	ent->priv.sv->client->ps.vmodel.angles[ROLL] = xyspeed * bobfracsin * 0.005;
	ent->priv.sv->client->ps.vmodel.angles[YAW] = xyspeed * bobfracsin * 0.01;
	if( bobcycle & 1 )
	{
		ent->priv.sv->client->ps.vmodel.angles[ROLL] = -ent->priv.sv->client->ps.vmodel.angles[ROLL];
		ent->priv.sv->client->ps.vmodel.angles[YAW] = -ent->priv.sv->client->ps.vmodel.angles[YAW];
	}
	ent->priv.sv->client->ps.vmodel.angles[PITCH] = xyspeed * bobfracsin * 0.005;

	// gun angles from delta movement
	for (i = 0; i < 3; i++)
	{
		delta = ent->priv.sv->client->ps.oldviewangles[i] - ent->priv.sv->client->ps.viewangles[i];
		if( delta > 180 ) delta -= 360;
		if( delta < -180 ) delta += 360;
		if( delta > 45 ) delta = 45;
		if( delta < -45 ) delta = -45;
		if( i == YAW ) ent->priv.sv->client->ps.vmodel.angles[ROLL] += 0.1f * delta;
		ent->priv.sv->client->ps.vmodel.angles[i] += 0.2 * delta;
	}

	// gun height
	VectorClear( ent->priv.sv->client->ps.vmodel.offset );

	for( i = 0; i < 3; i++ )
	{
		ent->priv.sv->client->ps.vmodel.offset[i] += forward[i];
		ent->priv.sv->client->ps.vmodel.offset[i] += right[i];
		ent->priv.sv->client->ps.vmodel.offset[i] += up[i] * -1;
	}
}


void SV_CalcViewOffset (edict_t *ent)
{
	float		*angles;
	float		bob;
	float		delta;
	vec3_t		v;

	// base angles
	angles = ent->priv.sv->client->ps.kick_angles;

	VectorCopy(ent->progs.sv->punchangle, angles);
	// add angles based on velocity
	delta = DotProduct (ent->progs.sv->velocity, forward);
	angles[PITCH] += delta * 0.002;
		
	delta = DotProduct (ent->progs.sv->velocity, right);
	angles[ROLL] += delta * 0.005;

	// add angles based on bob
	delta = bobfracsin * 0.002 * xyspeed;
	if (ent->priv.sv->client->ps.pm_flags & PMF_DUCKED)
		delta *= 6; // crouching
	angles[PITCH] += delta;
	delta = bobfracsin * 0.002 * xyspeed;

	if (ent->priv.sv->client->ps.pm_flags & PMF_DUCKED)
		delta *= 6; // crouching
	if (bobcycle & 1) delta = -delta;
	angles[ROLL] += delta;

	VectorCopy( ent->progs.sv->view_ofs, v ); // base origin
	if( ent->priv.sv->client->ps.pm_flags & PMF_DUCKED )
		v[2] = -2;

	// add bob height
	bob = bobfracsin * xyspeed * 0.005;
	if( bob > 6 ) bob = 6;
	v[2] += bob;

	v[0] = bound( -14, v[0], 14 );
	v[1] = bound( -14, v[1], 14 );
	v[2] = bound( -22, v[2], 48 );
	VectorCopy( v, ent->priv.sv->client->ps.viewoffset );
}

void SV_SetStats (edict_t *ent)
{
	ent->priv.sv->client->ps.stats[STAT_HEALTH] = ent->progs.sv->health;
}

void ClientEndServerFrame (edict_t *ent)
{
	float		bobtime;

	current_player = ent;
	current_client = ent->priv.sv->client;
	
	//
	// If the origin or velocity have changed since ClientThink(),
	// update the pmove values.  This will happen when the client
	// is pushed by a bmodel or kicked by an explosion.
	// 
	// If it wasn't updated here, the view position would lag a frame
	// behind the body position when pushed -- "sinking into plats"
	//
	VectorScale(ent->progs.sv->origin, SV_COORD_FRAC, current_client->ps.origin ); 
	VectorScale(ent->progs.sv->velocity, SV_COORD_FRAC, current_client->ps.velocity ); 
	AngleVectors (ent->priv.sv->client->ps.viewangles, forward, right, up);

	//
	// set model angles from view angles so other things in
	// the world can tell which direction you are looking
	//
	if (ent->priv.sv->client->ps.viewangles[PITCH] > 180) 
		ent->progs.sv->angles[PITCH] = (-360 + ent->priv.sv->client->ps.viewangles[PITCH])/3;
	else ent->progs.sv->angles[PITCH] = ent->priv.sv->client->ps.viewangles[PITCH]/3;

	ent->progs.sv->angles[YAW] = ent->priv.sv->client->ps.viewangles[YAW];
	ent->progs.sv->angles[ROLL] = 0;
	ent->progs.sv->angles[ROLL] = SV_CalcRoll (ent->progs.sv->angles, ent->progs.sv->velocity)*4;

	//
	// calculate speed and cycle to be used for
	// all cyclic walking effects
	//
	xyspeed = sqrt(ent->progs.sv->velocity[0] * ent->progs.sv->velocity[0] + ent->progs.sv->velocity[1] * ent->progs.sv->velocity[1]);

	if (xyspeed < 5)
	{
		bobmove = 0;
		current_client->ps.bobtime = 0;	// start at beginning of cycle again
	}
	else if (ent->progs.sv->groundentity)
	{	
		// so bobbing only cycles when on ground
		if (xyspeed > 210) bobmove = 0.25;
		else if (xyspeed > 100) bobmove = 0.125;
		else bobmove = 0.0625;
	}
	
	bobtime = (current_client->ps.bobtime += bobmove);

	if (current_client->ps.pm_flags & PMF_DUCKED)
		bobtime *= 4;

	bobcycle = (int)bobtime;
	bobfracsin = fabs(sin(bobtime*M_PI));

	// determine the view offsets
	SV_CalcViewOffset (ent);

	// determine the gun offsets
	SV_CalcGunOffset (ent);

	SV_SetStats( ent );

	// if the scoreboard is up, update it
	if (!(sv.framenum & 31))
	{
	}
}

/*
=================
ClientEndServerFrames
=================
*/
void ClientEndServerFrames (void)
{
	int		i;

	// calc the player views now that all pushing
	// and damage has been added
	for (i = 0; i < maxclients->value; i++)
	{
		if(svs.clients[i].state != cs_spawned) continue;
		ClientEndServerFrame (svs.clients[i].edict);
	}
}

/*
================
SV_RunFrame

Advances the world by 0.1 seconds
================
*/
void SV_RunFrame( void )
{
	int		i;
	edict_t		*ent;

	// let the progs know that a new frame has started
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.sv->other = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.sv->time = sv.time;
	prog->globals.sv->frametime = sv.frametime;
	PRVM_ExecuteProgram (prog->globals.sv->StartFrame, "StartFrame");

	for (i = 1; i < prog->num_edicts; i++ )
	{
		ent = PRVM_EDICT_NUM(i);
		if (ent->priv.sv->free) continue;

		VectorCopy (ent->progs.sv->origin, ent->progs.sv->old_origin);

		// don't apply phys on clients
		if (i > 0 && i <= maxclients->value) continue;
		SV_Physics( ent );
	}

	// build the playerstate_t structures for all players
	ClientEndServerFrames ();

	prog->globals.sv->pev = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.sv->other = PRVM_EDICT_TO_PROG(prog->edicts);
	prog->globals.sv->time = sv.time;
	PRVM_ExecuteProgram (prog->globals.sv->EndFrame, "EndFrame");

	// decrement prog->num_edicts if the highest number entities died
	for ( ;PRVM_EDICT_NUM(prog->num_edicts - 1)->priv.sv->free; prog->num_edicts-- );
}

bool SV_ClientConnect (edict_t *ent, char *userinfo)
{
	// they can connect
	ent->progs.sv->flags = 0; // make sure we start with known default
	ent->progs.sv->health = 100;

	MsgDev(D_NOTE, "SV_ClientConnect()\n");
	prog->globals.sv->time = sv.time;
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG(ent);
	PRVM_ExecuteProgram (prog->globals.sv->ClientConnect, "ClientConnect");

	return true;
}

void SV_ClientUserinfoChanged (edict_t *ent, char *userinfo)
{
	char	*s;
	int	playernum;
          
	// check for malformed or illegal info strings
	if (!Info_Validate(userinfo))
	{
		strcpy (userinfo, "\\name\\badinfo\\skin\\male/grunt");
	}

	// set skin
	s = Info_ValueForKey (userinfo, "skin");

	playernum = PRVM_NUM_FOR_EDICT(ent);

	// combine name and skin into a configstring
	SV_ConfigString (CS_PLAYERSKINS + playernum, va("%s\\%s", Info_ValueForKey (userinfo, "name"), Info_ValueForKey (userinfo, "skin")));

	ent->priv.sv->client->ps.fov = bound(1, atoi(Info_ValueForKey(userinfo, "fov")), 160);
}

/*
===========
SV_ClientBegin

called when a client has finished connecting, and is ready
to be placed into the game.  This will happen every level load.
============
*/
void SV_ClientBegin (edict_t *ent)
{
	int		i;

	// if there is already a body waiting for us (a loadgame), just
	// take it, otherwise spawn one from scratch
	if (ent->priv.sv->free)
	{
		// the client has cleared the client side viewangles upon
		// connecting to the server, which is different than the
		// state when the game is saved, so we need to compensate
		// with deltaangles
		for (i = 0; i < 3; i++)
			ent->priv.sv->client->ps.delta_angles[i] = ANGLE2SHORT(ent->priv.sv->client->ps.viewangles[i]);
	}
	else
	{
		// a spawn point will completely reinitialize the entity
		// except for the persistant data that was initialized at
		// ClientConnect() time
		SV_InitEdict (ent);
		SV_PutClientInServer (ent);
	}

	// make sure all view stuff is valid
	ClientEndServerFrame (ent);
}

/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame.
==============
*/
void ClientThink (edict_t *ent, usercmd_t *ucmd)
{
	sv_client_t	*client;
	edict_t		*other;
	pmove_t		pm;
	vec3_t		view;
	vec3_t		oldorigin, oldvelocity;
	int		i, j;

	client = ent->priv.sv->client;

	// call standard client pre-think
	prog->globals.sv->time = sv.time;
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG(ent);
	PRVM_ExecuteProgram (prog->globals.sv->PlayerPreThink, "PlayerPreThink");

	VectorCopy(ent->progs.sv->origin, oldorigin);
	VectorCopy(ent->progs.sv->velocity, oldvelocity);

	ent->priv.sv->client->ps.pm_flags &= ~PMF_NO_PREDICTION;

	VectorCopy(ent->progs.sv->origin, view);

	pm_passent = ent;

	// set up for pmove
	memset (&pm, 0, sizeof(pm));

	if (ent->progs.sv->movetype == MOVETYPE_NOCLIP) client->ps.pm_type = PM_SPECTATOR;
	else client->ps.pm_type = PM_NORMAL;
	client->ps.gravity = sv_gravity->value;

	if(ent->progs.sv->teleport_time) 
	{
		// next frame will be cleared teleport flag
		client->ps.pm_flags |= PMF_TIME_TELEPORT; 
		ent->progs.sv->teleport_time = 0;
	}
	else client->ps.pm_flags &= ~PMF_TIME_TELEPORT; 

	pm.ps = client->ps;
	memcpy( &client->lastcmd, ucmd, sizeof(usercmd_t));//IMPORTANT!!!

	VectorScale(ent->progs.sv->origin, SV_COORD_FRAC, pm.ps.origin );
	VectorScale(ent->progs.sv->velocity, SV_COORD_FRAC, pm.ps.velocity );

	pm.cmd = *ucmd;

	pm.trace = PM_trace; // adds default parms
	pm.pointcontents = PM_pointcontents;

	// perform a pmove
	pe->PlayerMove( &pm, false );

	// save results of pmove
	client->ps = pm.ps;

	VectorScale(pm.ps.origin, CL_COORD_FRAC, ent->progs.sv->origin);
	VectorScale(pm.ps.velocity, CL_COORD_FRAC, ent->progs.sv->velocity);
	VectorCopy(pm.mins, ent->progs.sv->mins);
	VectorCopy(pm.maxs, ent->progs.sv->maxs);
	VectorCopy(pm.ps.viewangles, ent->progs.sv->v_angle);
	VectorCopy(pm.ps.viewangles, ent->progs.sv->angles);
	ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG( pm.ps.groundentity );

	// copy viewmodel info
	client->ps.vmodel.frame = ent->progs.sv->v_frame;
	client->ps.vmodel.body = ent->progs.sv->v_body;
	client->ps.vmodel.skin = ent->progs.sv->v_skin;
	client->ps.vmodel.sequence = ent->progs.sv->v_sequence;
	VectorCopy(ent->progs.sv->v_offset, client->ps.vmodel.offset );
	VectorCopy(ent->progs.sv->v_angles, client->ps.vmodel.angles );

	SV_LinkEdict(ent);

	if (ent->progs.sv->movetype != MOVETYPE_NOCLIP)
		SV_TouchTriggers (ent);

	PRVM_PUSH_GLOBALS;
	for (i = 0; i < pm.numtouch; i++)
	{
		other = pm.touchents[i];
		for (j = 0; j < i; j++)
		{
			if (pm.touchents[j] == other)
				break;
		}
		if (j != i) continue; // duplicated
		if (!other->progs.sv->touch) continue;
		
		prog->globals.sv->pev = PRVM_EDICT_TO_PROG(other);
		prog->globals.sv->other = PRVM_EDICT_TO_PROG(ent);
		prog->globals.sv->time = sv.time;
		if( other->progs.sv->touch )
		{
			PRVM_ExecuteProgram (other->progs.sv->touch, "pev->touch");
		}
	}
	PRVM_POP_GLOBALS;

	// call standard player post-think
	prog->globals.sv->time = sv.time;
	prog->globals.sv->pev = PRVM_EDICT_TO_PROG(ent);
	PRVM_ExecuteProgram (prog->globals.sv->PlayerPostThink, "PlayerPostThink");
}

/*
===========
SV_ClientDisconnect

Called when a player drops from the server.
Will not be called between levels.
============
*/
void SV_ClientDisconnect (edict_t *ent)
{
	int	playernum;

	if (!ent->priv.sv->client) return;

	SV_UnlinkEdict(ent);
	ent->progs.sv->modelindex = 0;
	ent->progs.sv->solid = SOLID_NOT;
	ent->priv.sv->free = true;
	ent->progs.sv->classname = PRVM_SetEngineString("disconnected");

	playernum = PRVM_NUM_FOR_EDICT(ent) - 1;
	SV_ConfigString (CS_PLAYERSKINS + playernum, "");

}

/*
==================
SV_StartParticle

Make sure the event gets sent to all clients
==================
*/
void SV_StartParticle (vec3_t org, vec3_t dir, int color, int count)
{
	MsgDev( D_ERROR, "SV_StartParticle: implement me\n");
}

/*
===============
PF_cprintf

Print to a single client
===============
*/
void PF_cprintf (edict_t *ent, int level, char *fmt, ...)
{
	char		msg[1024];
	va_list		argptr;
	int			n;

	if (ent)
	{
		n = PRVM_NUM_FOR_EDICT(ent);
		if (n < 1 || n > maxclients->value)
			Host_Error("cprintf to a non-client\n");
	}

	va_start (argptr,fmt);
	com.vsprintf (msg, fmt, argptr);
	va_end (argptr);

	if (ent) SV_ClientPrintf (svs.clients+(n-1), level, "%s", msg);
	else Msg ("%s", msg);
}


/*
==================
Cmd_Say_f
==================
*/
void Cmd_Say_f (edict_t *ent, bool team, bool arg0)
{
	int		j;
	edict_t		*other;
	char		*p;
	char		text[2048];

	if (Cmd_Argc () < 2 && !arg0) return;

	com.sprintf (text, "%s: ", "all");

	if (arg0)
	{
		strcat (text, Cmd_Argv(0));
		strcat (text, " ");
		strcat (text, Cmd_Args());
	}
	else
	{
		p = Cmd_Args();

		if (*p == '"')
		{
			p++;
			p[strlen(p)-1] = 0;
		}
		strcat(text, p);
	}

	// don't let text be too long for malicious reasons
	if (strlen(text) > 150) text[150] = 0;

	com.strcat(text, "\n");

	if( host.type == HOST_DEDICATED )
		PF_cprintf(NULL, PRINT_CHAT, "%s", text);

	for (j = 1; j <= maxclients->value; j++)
	{
		other = PRVM_EDICT_NUM(j);
		if (other->priv.sv->free) continue;
		if (!other->priv.sv->client) continue;
		PF_cprintf(other, PRINT_CHAT, "%s", text);
	}
}

/*
=================
SV_ClientCommand
=================
*/
void SV_ClientCommand (edict_t *ent)
{
	char	*cmd;
	char	*parm;

	if (!ent->priv.sv->client) return; // not fully in game yet

	cmd = Cmd_Argv(0);

	if(Cmd_Argc() < 2) parm = NULL;
	else parm = Cmd_Argv(1);

	if (strcasecmp (cmd, "say") == 0)
	{
		Cmd_Say_f (ent, false, false);
		return;
	}
	if (strcasecmp (cmd, "say_team") == 0)
	{
		Cmd_Say_f (ent, true, false);
		return;
	}
}

void SV_Transform( sv_edict_t *ed, matrix4x3 transform )
{
	edict_t	*edict;
	vec3_t	origin, angles;
	matrix4x4	objmatrix;

	if(!ed) return;
	edict = PRVM_EDICT_NUM( ed->serialnumber );

	// save matrix (fourth value will be reset on save\load)
	VectorCopy( transform[0], edict->progs.sv->m_pmatrix[0] );
	VectorCopy( transform[1], edict->progs.sv->m_pmatrix[1] );
	VectorCopy( transform[2], edict->progs.sv->m_pmatrix[2] );
	VectorCopy( transform[3], edict->progs.sv->m_pmatrix[3] );

	MatrixLoadIdentity( objmatrix );
	VectorCopy( transform[0], objmatrix[0] );
	VectorCopy( transform[1], objmatrix[1] );
	VectorCopy( transform[2], objmatrix[2] );
	VectorCopy( transform[3], objmatrix[3] );
	MatrixAngles( objmatrix, origin, angles );

	VectorCopy( origin, edict->progs.sv->origin );
	VectorCopy( angles, edict->progs.sv->angles );

	// refresh force and torque
	pe->GetForce( ed->physbody, edict->progs.sv->velocity, edict->progs.sv->avelocity, edict->progs.sv->force, edict->progs.sv->torque );
	pe->GetMassCentre( ed->physbody, edict->progs.sv->m_pcentre );
}

/*
==============
CV_ClientMove

grab user cmd from player_state_t
send it to transform callback
==============
*/
void SV_PlayerMove( sv_edict_t *ed )
{
	pmove_t		pm;
	sv_client_t	*client;
	edict_t		*player;

	client = ed->client;
	player = PRVM_PROG_TO_EDICT( ed->serialnumber );
	memset( &pm, 0, sizeof(pm) );

	if( player->progs.sv->movetype == MOVETYPE_NOCLIP )
		client->ps.pm_type = PM_SPECTATOR;
	else client->ps.pm_type = PM_NORMAL;
	client->ps.gravity = sv_gravity->value;

	if( player->progs.sv->teleport_time )
		client->ps.pm_flags |= PMF_TIME_TELEPORT; 
	else client->ps.pm_flags &= ~PMF_TIME_TELEPORT; 

	pm.ps = client->ps;
	pm.cmd = client->lastcmd;
	pm.body = ed->physbody;	// member body ptr
	
	VectorCopy( player->progs.sv->origin, pm.ps.origin );
	VectorCopy( player->progs.sv->velocity, pm.ps.velocity );

	pe->ServerMove( &pm );

	// save results of pmove
	client->ps = pm.ps;

	VectorCopy(pm.ps.origin, player->progs.sv->origin);
	VectorCopy(pm.ps.velocity, player->progs.sv->velocity);
	VectorCopy(pm.mins, player->progs.sv->mins);
	VectorCopy(pm.maxs, player->progs.sv->maxs);
	VectorCopy(pm.ps.viewangles, client->ps.viewangles);
}

void SV_PlaySound( sv_edict_t *ed, float volume, const char *sample )
{
	float	vol = bound( 0.0f, volume/255.0f, 255.0f );
	int	sound_idx = SV_SoundIndex( sample );
	edict_t	*ent;
	if( !ed ) ed = prog->edicts->priv.sv;
	ent = PRVM_PROG_TO_EDICT( ed->serialnumber );

	//SV_StartSound( ent->progs.sv->origin, ent, CHAN_BODY, sound_idx, vol, 1.0f, 0 );
}  