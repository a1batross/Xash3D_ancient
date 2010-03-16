//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_move.c - monsters movement
//=======================================================================

#include "common.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "server.h"
#include "const.h"
#include "pm_defs.h"

/*
=============
SV_CheckBottom

Returns false if any part of the bottom of the entity is off an edge that
is not a staircase.

=============
*/
bool SV_CheckBottom( edict_t *ent, int iMode )
{
	vec3_t	mins, maxs, start, stop;
	bool	realcheck = false;
	float	mid, bottom;
	trace_t	trace;
	int	x, y;

	VectorAdd( ent->v.origin, ent->v.mins, mins );
	VectorAdd( ent->v.origin, ent->v.maxs, maxs );

	// if all of the points under the corners are solid world, don't bother
	// with the tougher checks
	// the corners must be within 16 of the midpoint
	start[2] = mins[2] - 1;
	for( x = 0; x <= 1; x++ )
	{
		for( y = 0; y <= 1; y++ )
		{
			start[0] = x ? maxs[0] : mins[0];
			start[1] = y ? maxs[1] : mins[1];
			if( SV_BaseContents( start, ent ) & MASK_SOLID )
			{
				realcheck = true;
				break;
			}
		}
		if( realcheck )
			break;
	}

	if( !realcheck )
		return true; // we got out easy

	// check it for real...
	start[2] = mins[2] + svgame.movevars.stepsize;

	// the midpoint must be within 16 of the bottom
	start[0] = stop[0] = (mins[0] + maxs[0]) * 0.5f;
	start[1] = stop[1] = (mins[1] + maxs[1]) * 0.5f;
	stop[2] = start[2] - 2 * svgame.movevars.stepsize;

	if( iMode == WALKMOVE_WORLDONLY )
		trace = SV_Move( start, vec3_origin, vec3_origin, stop, MOVE_WORLDONLY, ent );
	else trace = SV_Move( start, vec3_origin, vec3_origin, stop, MOVE_NORMAL|FTRACE_SIMPLEBOX, ent );

	if( trace.flFraction == 1.0f )
		return false;
	mid = bottom = trace.vecEndPos[2];

	// the corners must be within 16 of the midpoint
	for( x = 0; x <= 1; x++ )
	{
		for( y = 0; y <= 1; y++ )
		{
			start[0] = stop[0] = x ? maxs[0] : mins[0];
			start[1] = stop[1] = y ? maxs[1] : mins[1];

			if( iMode == WALKMOVE_WORLDONLY )
				trace = SV_Move( start, vec3_origin, vec3_origin, stop, MOVE_WORLDONLY, ent );
			else trace = SV_Move( start, vec3_origin, vec3_origin, stop, MOVE_NORMAL|FTRACE_SIMPLEBOX, ent );

			if( trace.flFraction != 1.0f && trace.vecEndPos[2] > bottom )
				bottom = trace.vecEndPos[2];
			if( trace.flFraction == 1.0f || mid - trace.vecEndPos[2] > svgame.movevars.stepsize )
				return false;
		}
	}
	return true;
}

/*
=============
SV_VecToYaw

converts dir to yaw
=============
*/
float SV_VecToYaw( const vec3_t src )
{
	float	yaw;

	if( src[1] == 0 && src[0] == 0 )
	{
		yaw = 0;
	}
	else
	{
		yaw = (int)( com.atan2( src[1], src[0] ) * 180 / M_PI );
		if( yaw < 0 ) yaw += 360;
	}
	return yaw;
}

//============================================================================
/*
======================
SV_WalkMove

======================
*/
bool SV_WalkMove( edict_t *ent, vec3_t move, int iMode )
{
	trace_t	trace;
	vec3_t	oldorg, neworg, end;
	edict_t	*groundent = NULL;
	bool	relink;

	if( iMode == WALKMOVE_NORMAL )
		relink = true;
	else relink = false;

	// try the move
	VectorCopy( ent->v.origin, oldorg );
   
	// flying pawns don't step up
	if( ent->v.flags & ( FL_SWIM|FL_FLY ))
	{
		VectorAdd( oldorg, move, neworg );

		if( iMode == WALKMOVE_WORLDONLY )
			trace = SV_Move( oldorg, ent->v.mins, ent->v.maxs, neworg, MOVE_WORLDONLY, ent );
		else trace = SV_Move( oldorg, ent->v.mins, ent->v.maxs, neworg, MOVE_NORMAL|FTRACE_SIMPLEBOX, ent );

		if( trace.flFraction == 1.0f )
		{
			if(( ent->v.flags & FL_SWIM ) && !( SV_BaseContents(trace.vecEndPos, ent) & MASK_WATER ))
				return false; // swimming pawn left water

			VectorCopy( trace.vecEndPos, ent->v.origin );

			if( !VectorCompare( ent->v.origin, oldorg ))
				SV_LinkEdict( ent, relink );

			return true;
		}
		return false;
	}

	VectorAdd( oldorg, move, neworg );

	// push down from a step height above the wished position
	neworg[2] += svgame.movevars.stepsize;
	VectorCopy( neworg, end );
	end[2] -= svgame.movevars.stepsize * 2;

	if( iMode == WALKMOVE_WORLDONLY )
		trace = SV_Move( neworg, ent->v.mins, ent->v.maxs, end, MOVE_WORLDONLY, ent );
	else trace = SV_Move( neworg, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL|FTRACE_SIMPLEBOX, ent );
    
	if( trace.fAllSolid )
	{
		Msg( "WalkMove: all solid\n" );
		return false;
	}
	if( trace.fStartSolid )
	{
		neworg[2] -= svgame.movevars.stepsize;

		if( iMode == WALKMOVE_WORLDONLY )
			trace = SV_Move( neworg, ent->v.mins, ent->v.maxs, end, MOVE_WORLDONLY, ent );
		else trace = SV_Move( neworg, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL|FTRACE_SIMPLEBOX, ent );

		if( trace.fAllSolid || trace.fStartSolid )
		{
			Msg( "WalkMove: all solid || start solid\n" );
			return false;
		}
	}

	if( trace.flFraction == 1.0f )
	{
		// if monster had the ground pulled out, go ahead and fall
		if( ent->v.flags & FL_PARTIALGROUND )
		{
			VectorAdd( ent->v.origin, move, ent->v.origin );

			if( !VectorCompare( ent->v.origin, oldorg ))
				SV_LinkEdict( ent, relink );

			ent->v.flags &= ~FL_ONGROUND;
			return true;
		}
		Msg( "WalkMove: walked off and edge\n" );
		return false; // walked off an edge
	}

	// check point traces down for dangling corners
	VectorCopy( trace.vecEndPos, ent->v.origin );
	groundent = trace.pHit;

	// check our pos
	if( iMode == WALKMOVE_WORLDONLY )
		trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_WORLDONLY, ent );
	else trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_NORMAL|FTRACE_SIMPLEBOX, ent );

	if( trace.fStartSolid )
	{
		VectorCopy( oldorg, ent->v.origin );
		Msg( "WalkMove: start solid\n" );
		return false;
	}

	if( !SV_CheckBottom( ent, iMode ))
	{
		if( ent->v.flags & FL_PARTIALGROUND )
		{    
			// actor had floor mostly pulled out from underneath it
			// and is trying to correct
			if( !VectorCompare( ent->v.origin, oldorg ))
				SV_LinkEdict( ent, relink );
			Msg( "WalkMove: partialground - ok\n" );
			return true;
		}

		ent->v.flags |= FL_PARTIALGROUND;
		VectorCopy( oldorg, ent->v.origin );
		Msg( "WalkMove: restore old pos\n" );
		return false;
	}

	if( ent->v.flags & FL_PARTIALGROUND )
		ent->v.flags &= ~FL_PARTIALGROUND;

	ent->v.groundentity = groundent;

	// the move is ok
	if( !VectorCompare( ent->v.origin, oldorg ))
		SV_LinkEdict( ent, relink );
	return true;
}

/*
======================
SV_StepDirection

Turns to the movement direction, and walks the current distance if
facing it.
======================
*/
bool SV_StepDirection( edict_t *ent, float yaw, float dist, int iMode )
{
	vec3_t	move, oldorigin;
	float	delta;

	ent->v.ideal_yaw = yaw;
	pfnChangeYaw( ent );

	yaw = yaw * M_PI*2 / 360;
	VectorSet( move, com.cos( yaw ) * dist, com.sin( yaw ) * dist, 0.0f );
	VectorCopy( ent->v.origin, oldorigin );

	if( SV_WalkMove( ent, move, WALKMOVE_NORMAL ))
	{
		if( iMode != MOVE_STRAFE )
		{
			delta = ent->v.angles[YAW] - ent->v.ideal_yaw;
			if( delta > 45 && delta < 315 )
			{		
				// not turned far enough, so don't take the step
				VectorCopy( oldorigin, ent->v.origin );
			}
		}
		SV_LinkEdict( ent, false );
		return true;
	}

	SV_LinkEdict( ent, false );
	return false;
}

/*
======================
SV_MoveToOrigin

Turns to the movement direction, and walks the current distance if
facing it.
======================
*/  
void SV_MoveToOrigin( edict_t *ed, const vec3_t goal, float dist, int iMode )
{
	float	yaw, distToGoal;
	vec3_t	vecDist;

	if( iMode == MOVE_STRAFE )
	{
		vec3_t	delta;
		
		VectorSubtract( goal, ed->v.origin, delta );
		VectorNormalizeFast( delta );
		yaw = SV_VecToYaw( delta );
	}
	else
	{
		yaw = ed->v.ideal_yaw;
	}


	VectorSubtract( ed->v.origin, goal, vecDist );
	distToGoal = com.sqrt( vecDist[0] * vecDist[0] + vecDist[1] * vecDist[1] );
	if( dist > distToGoal ) dist = distToGoal;

	SV_StepDirection( ed, yaw, dist, iMode );
}

/*
============================================================

CLIENT MOVEMENT CODE

============================================================
*/
// builtins
/*
=================
PM_ClientPrintf

Sends text across to be displayed
=================
*/
static void PM_ClientPrintf( int index, char *fmt, ... )
{
	va_list		argptr;
	char		string[MAX_SYSPATH];
	sv_client_t	*cl;

	if( index < 0 || index >= sv_maxclients->integer )
		return;

	cl = svs.clients + index;

	if( cl->edict && (cl->edict->v.flags & FL_FAKECLIENT ))
		return;
	
	va_start( argptr, fmt );
	com.vsprintf( string, fmt, argptr );
	va_end( argptr );
	
	MSG_WriteByte( &cl->netchan.message, svc_print );
	MSG_WriteByte( &cl->netchan.message, PRINT_HIGH );
	MSG_WriteString( &cl->netchan.message, string );
}

/*
===============
PM_PlayerTrace

===============
*/
static TraceResult PM_PlayerTrace( const vec3_t start, const vec3_t end, int trace_type )
{
	float		*mins;
	float		*maxs;
	trace_t		result;
	TraceResult	out;

	if( VectorIsNAN( start ) || VectorIsNAN( end ))
		Host_Error( "PlayerTrace: NAN errors detected ('%f %f %f', '%f %f %f'\n", start[0], start[1], start[2], end[0], end[1], end[2] );

	svgame.pmove->usehull = bound( 0, svgame.pmove->usehull, 3 );
	mins = svgame.pmove->player_mins[svgame.pmove->usehull];
	maxs = svgame.pmove->player_maxs[svgame.pmove->usehull];

	result = SV_Move( start, mins, maxs, end, trace_type, svgame.pmove->player );
	Mem_Copy( &out, &result, sizeof( TraceResult ));

	return out;
}

/*
===============
PM_TraceTexture

===============
*/
static const char *PM_TraceTexture( edict_t *pTextureEntity, const float *v1, const float *v2 )
{
	if( VectorIsNAN( v1 ) || VectorIsNAN( v2 ))
		Host_Error( "TraceTexture: NAN errors detected ('%f %f %f', '%f %f %f'\n", v1[0], v1[1], v1[2], v2[0], v2[1], v2[2] );

	if( !pTextureEntity || pTextureEntity->free ) return NULL; 
	return SV_ClipMoveToEntity( pTextureEntity, v1, vec3_origin, vec3_origin, v2, MASK_SOLID, 0 ).pTexName;
}

/*
===============
PM_TraceModel

===============
*/
static TraceResult PM_TraceModel( edict_t *pEnt, const vec3_t start, const vec3_t end )
{
	float		*mins;
	float		*maxs;
	trace_t		result;
	TraceResult	out;
	uint		umask;

	if( VectorIsNAN( start ) || VectorIsNAN( end ))
		Host_Error( "TraceModel: NAN errors detected ('%f %f %f', '%f %f %f'\n", start[0], start[1], start[2], end[0], end[1], end[2] );

	umask = World_MaskForEdict( svgame.pmove->player );
	svgame.pmove->usehull = bound( 0, svgame.pmove->usehull, 3 );
	mins = svgame.pmove->player_mins[svgame.pmove->usehull];
	maxs = svgame.pmove->player_maxs[svgame.pmove->usehull];
	result = SV_ClipMoveToEntity( pEnt, start, mins, maxs, end, umask, FTRACE_SIMPLEBOX );
	Mem_Copy( &out, &result, sizeof( TraceResult ));

	return out;
}

/*
===============
PM_GetEntityByIndex

safe version of SV_EDICT_NUM
===============
*/
static edict_t *PM_GetEntityByIndex( int index )
{
	if( index < 0 || index > svgame.globals->numEntities )
	{
		if( index == VIEWENT_INDEX ) return svgame.pmove->player->v.aiment; // current weapon
		if( index == NULLENT_INDEX ) return NULL;
		MsgDev( D_ERROR, "PM_GetEntityByIndex: invalid entindex %i\n", index );
		return NULL;
	}

	if( EDICT_NUM( index )->free )
		return NULL;
	return EDICT_NUM( index );
}

static void PM_PlaySound( int chan, const char *sample, float vol, float attn, int pitch )
{
	if( !svgame.pmove->runfuncs ) return; // ignored
	SV_StartSound( svgame.pmove->player, chan, sample, vol, attn, 0, pitch );
}

static edict_t *PM_TestPlayerPosition( const vec3_t origin, TraceResult *trace )
{
	return SV_TestPlayerPosition( origin, svgame.pmove->player, trace );
}

static int PM_PointContents( const vec3_t p )
{
	return World_ConvertContents( SV_BaseContents( p, svgame.pmove->player ));
}

static void PM_CheckMovingGround( edict_t *ent, float frametime )
{
	SV_UpdateBaseVelocity( ent );

	if(!( ent->v.flags & FL_BASEVELOCITY ))
	{
		// apply momentum (add in half of the previous frame of velocity first)
		VectorMA( ent->v.velocity, 1.0f + (frametime * 0.5f), ent->v.basevelocity, ent->v.velocity );
		VectorClear( ent->v.basevelocity );
	}
	ent->v.flags &= ~FL_BASEVELOCITY;
}

static void PM_SetupMove( playermove_t *pmove, edict_t *clent, usercmd_t *ucmd, const char *physinfo )
{
	edict_t	*ent;
	float	eorg, distSquared;
	float	flRadius = 12.0f;
	int	j, e;

	pmove->multiplayer = (sv_maxclients->integer > 1) ? true : false;
	pmove->serverflags = svgame.globals->serverflags;	// shared serverflags
	pmove->maxspeed = svgame.movevars.maxspeed;
	pmove->realtime = svgame.globals->time;
	pmove->frametime = ucmd->msec * 0.001f;
	com.strncpy( pmove->physinfo, physinfo, MAX_INFO_STRING );
	pmove->clientmaxspeed = clent->v.maxspeed;
	pmove->cmd = *ucmd;				// setup current cmds
	pmove->player = clent;			// ptr to client state
	pmove->numtouch = 0;			// reset touchents
	pmove->dead = (clent->v.health <= 0.0f) ? true : false;
	pmove->flWaterJumpTime = clent->v.teleport_time;
	pmove->onground = clent->v.groundentity;
	pmove->usehull = (clent->v.flags & FL_DUCKING) ? 1 : 0; // reset hull
	pmove->bInDuck = clent->v.bInDuck;
	VectorCopy( clent->v.origin, pmove->origin );
	VectorCopy( clent->v.movedir, pmove->movedir );
	VectorCopy( clent->v.velocity, pmove->velocity );
	VectorCopy( clent->v.basevelocity, pmove->basevelocity );

	flRadius *= flRadius;
	pmove->numladders = 0;

	// build list of ladders around player
	for( e = 1; e < svgame.globals->numEntities; e++ )
	{
		if( pmove->numladders >= MAX_LADDERS )
		{
			MsgDev( D_ERROR, "PM_PlayerMove: too many ladders in PVS\n" );
			break;
		}

		ent = EDICT_NUM( e );
		if( ent->free ) continue;

		distSquared = 0;
		for( j = 0; j < 3 && distSquared <= flRadius; j++ )
		{
			if( pmove->origin[j] < ent->v.absmin[j] )
				eorg = pmove->origin[j] - ent->v.absmin[j];
			else if( pmove->origin[j] > ent->v.absmax[j] )
				eorg = pmove->origin[j] - ent->v.absmax[j];
			else eorg = 0;

			distSquared += eorg * eorg;
		}
		if( distSquared > flRadius )
			continue;
		if( ent->v.skin != CONTENTS_LADDER )
			continue;
		pmove->ladders[pmove->numladders++] = ent;
	}
}

static void PM_FinishMove( playermove_t *pmove, edict_t *clent )
{
	clent->v.teleport_time = pmove->flWaterJumpTime;
	clent->v.groundentity = pmove->onground;
	VectorCopy( pmove->angles, clent->v.viewangles );
	VectorCopy( pmove->origin, clent->v.origin );
	VectorCopy( pmove->movedir, clent->v.movedir );
	VectorCopy( pmove->velocity, clent->v.velocity );
	VectorCopy( pmove->basevelocity, clent->v.basevelocity );
	clent->v.bInDuck = pmove->bInDuck;
}

/*
===============
SV_InitClientMove

===============
*/
void SV_InitClientMove( void )
{
	int	i;

	svgame.pmove->movevars = &svgame.movevars;

	// init hulls
	VectorCopy( GI->client_mins[0], svgame.pmove->player_mins[2] ); // copy point hull
	VectorCopy( GI->client_maxs[0], svgame.pmove->player_maxs[2] );
	VectorCopy( GI->client_mins[1], svgame.pmove->player_mins[0] ); // copy human hull
	VectorCopy( GI->client_maxs[1], svgame.pmove->player_maxs[0] );
	VectorCopy( GI->client_mins[2], svgame.pmove->player_mins[3] ); // copy large hull
	VectorCopy( GI->client_maxs[2], svgame.pmove->player_maxs[3] );
	VectorCopy( GI->client_mins[3], svgame.pmove->player_mins[1] ); // copy head hull
	VectorCopy( GI->client_maxs[3], svgame.pmove->player_maxs[1] );

	for( i = 0; i < PM_MAXHULLS; i++ )
		svgame.pmove->player_view[i] = GI->viewheight[i];

	// common utilities
	svgame.pmove->PM_Info_ValueForKey = Info_ValueForKey;
	svgame.pmove->PM_TestPlayerPosition = PM_TestPlayerPosition;
	svgame.pmove->ClientPrintf = PM_ClientPrintf;
	svgame.pmove->AlertMessage = pfnAlertMessage;
	svgame.pmove->PM_GetString = SV_GetString;
	svgame.pmove->PM_PointContents = PM_PointContents;
	svgame.pmove->PM_PlayerTrace = PM_PlayerTrace;
	svgame.pmove->PM_TraceTexture = PM_TraceTexture;
	svgame.pmove->PM_GetEntityByIndex = PM_GetEntityByIndex;
	svgame.pmove->AngleVectors = AngleVectors;
	svgame.pmove->RandomLong = pfnRandomLong;
	svgame.pmove->RandomFloat = pfnRandomFloat;
	svgame.pmove->PM_GetModelType = CM_GetModelType;
	svgame.pmove->PM_GetModelBounds = Mod_GetBounds;
	svgame.pmove->PM_ModExtradata = Mod_Extradata;
	svgame.pmove->PM_TraceModel = PM_TraceModel;
	svgame.pmove->COM_LoadFile = pfnLoadFile;
	svgame.pmove->COM_ParseToken = pfnParseToken;
	svgame.pmove->COM_FreeFile = pfnFreeFile;
	svgame.pmove->memfgets = pfnMemFgets;
	svgame.pmove->PM_PlaySound = PM_PlaySound;

	// initalize pmove
	svgame.dllFuncs.pfnPM_Init( svgame.pmove );
}

void SV_PreRunCmd( sv_client_t *cl, usercmd_t *ucmd )
{
	svgame.pmove->runfuncs = true;
	svgame.dllFuncs.pfnCmdStart( cl->edict, ucmd, ucmd->random_seed );
}

/*
===========
SV_RunCmd
===========
*/
void SV_RunCmd( sv_client_t *cl, usercmd_t *ucmd )
{
	edict_t		*clent;
	int		i, oldmsec;
	static usercmd_t	cmd;
	vec3_t		oldvel;

	cl->commandMsec -= ucmd->msec;

	if( cl->commandMsec < 0 && sv_enforcetime->integer )
	{
		MsgDev( D_INFO, "SV_ClientThink: commandMsec underflow from %s\n", cl->name );
		return;
	}

	clent = cl->edict;
	cmd = *ucmd;
	if( !clent || clent->free ) return;

	// chop up very long commands
	if( cmd.msec > 50 )
	{
		oldmsec = ucmd->msec;
		cmd.msec = oldmsec / 2;
		SV_RunCmd( cl, &cmd );
		cmd.msec = oldmsec / 2;
		cmd.impulse = 0;
		SV_RunCmd( cl, &cmd );
		return;
	}

	PM_CheckMovingGround( clent, ucmd->msec * 0.001f );

	VectorCopy( clent->v.viewangles, svgame.pmove->oldangles ); // save oldangles
	if( !clent->v.fixangle )
		VectorCopy( ucmd->viewangles, clent->v.viewangles );

	// copy player buttons
	clent->v.button = ucmd->buttons;
	if( ucmd->impulse ) clent->v.impulse = ucmd->impulse;

	// angles
	// show 1/3 the pitch angle and all the roll angle	
	if( clent->v.health > 0.0f )
	{
		if( !clent->v.fixangle )
		{
			clent->v.angles[PITCH] = -clent->v.viewangles[PITCH] / 3;
			clent->v.angles[YAW] = clent->v.viewangles[YAW];
		}
	}

	if(!( clent->v.flags & FL_SPECTATOR ))
	{
		svgame.globals->time = sv.time * 0.001f;
		svgame.globals->frametime = 0.0f;
		svgame.dllFuncs.pfnPlayerPreThink( clent );
		svgame.globals->frametime = ucmd->msec * 0.001f;

		SV_RunThink( clent );

		// If conveyor, or think, set basevelocity, then send to client asap too.
		if( VectorLength( clent->v.basevelocity ) > 0.0f )
			VectorCopy( clent->v.basevelocity, clent->v.clbasevelocity );
	}

	// setup playermove state
	PM_SetupMove( svgame.pmove, clent, ucmd, cl->physinfo );

	// motor!
	svgame.dllFuncs.pfnPM_Move( svgame.pmove, true );

	// copy results back to client
	PM_FinishMove( svgame.pmove, clent );
	VectorCopy( clent->v.velocity, oldvel ); // save velocity
		
	if(!( clent->v.flags & FL_SPECTATOR ))
	{
		// link into place and touch triggers
		SV_LinkEdict( clent, true );

		// NOTE: one of triggers apply new velocity to client
		// e.g trigger_teleport resets it or add new
		// so we need to apply new velocity immediately here
		if( clent->v.fixangle || clent->v.flJumpPadTime )
			VectorCopy( clent->v.velocity, oldvel );

		// touch other objects
		for( i = 0; i < svgame.pmove->numtouch; i++ )
		{
			if( i == MAX_PHYSENTS ) break;
			if( svgame.pmove->touchents[i] == clent ) continue;
			VectorCopy( svgame.pmove->touchvels[i], clent->v.velocity );

			svgame.dllFuncs.pfnTouch( svgame.pmove->touchents[i], clent );
		}
	}

	// restore velocity
	VectorCopy( oldvel, clent->v.velocity );
	svgame.pmove->numtouch = 0;
}

/*
===========
SV_PostRunCmd

Done after running a player command.
===========
*/
void SV_PostRunCmd( sv_client_t *cl )
{
	edict_t	*clent;

	clent = cl->edict;
	if( !clent || clent->free ) return;
	svgame.pmove->runfuncs = false; // all next calls ignore footstep sounds

	// run post-think
	if(!( clent->v.flags & FL_SPECTATOR ))
	{
		svgame.globals->time = sv.time * 0.001f;
		svgame.globals->frametime = 0;
		svgame.dllFuncs.pfnPlayerPostThink( clent );
	}
	else
	{
		svgame.globals->time = sv.time * 0.001f;
		svgame.globals->frametime = 0;
		svgame.dllFuncs.pfnSpectatorThink( clent );
	}

	// restore frametime
	svgame.globals->frametime = sv.frametime * 0.001f;
	svgame.dllFuncs.pfnCmdEnd( cl->edict );
}