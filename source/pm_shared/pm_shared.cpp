/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include <assert.h>
#include "basetypes.h"
#include "const.h"
#include "vector.h"
#include "usercmd.h"
#include "pm_defs.h"
#include "pm_local.h"
#include "pm_shared.h"
#include "pm_movevars.h"
#include "pm_materials.h"
#include <stdio.h>  // NULL
#include <math.h>   // sqrt
#include <string.h> // strcpy
#include <stdlib.h> // atoi
#include <ctype.h>  // isspace

static int pm_shared_initialized = 0;

#pragma warning( disable : 4305 )
#pragma warning( disable : 4244 )	// double to float warning

playermove_t *pmove = (playermove_t *)NULL;

// Ducking time
#define TIME_TO_DUCK		0.4
#define VEC_DUCK_HULL_MIN		-18
#define VEC_DUCK_HULL_MAX		18
#define VEC_DUCK_VIEW		12
#define PM_DEAD_VIEWHEIGHT		-8
#define MAX_CLIMB_SPEED		200
#define STUCK_MOVEUP 		1
#define STUCK_MOVEDOWN		-1
#define VEC_HULL_MIN		-36
#define VEC_HULL_MAX		36
#define VEC_VIEW			28
#define WJ_HEIGHT			8
#define STOP_EPSILON		0.1
#define CTEXTURESMAX		512	// max number of textures loaded
#define MAX_CLIENTS			32
#define pev			pmove->pev

#define BUNNYJUMP_MAX_SPEED_FACTOR	1.7f	// Only allow bunny jumping up to 1.7x server / player maxspeed setting
#define PM_CHECKSTUCK_MINTIME		0.05f	// Don't check again too quickly.
#define PLAYER_FATAL_FALL_SPEED	1024	// approx 60 feet
#define PLAYER_MAX_SAFE_FALL_SPEED	580	// approx 20 feet
#define DAMAGE_FOR_FALL_SPEED		(float)100 / ( PLAYER_FATAL_FALL_SPEED - PLAYER_MAX_SAFE_FALL_SPEED )// damage per unit per second.
#define PLAYER_MIN_BOUNCE_SPEED	200
#define PLAYER_FALL_PUNCH_THRESHHOLD	(float)350 // won't punch player's screen/make scrape noise unless player falling at least this fast.
#define PLAYER_LONGJUMP_SPEED		350 // how fast we longjump

#define max( a, b )  (((a) > (b)) ? (a) : (b))
#define min( a, b )  (((a) < (b)) ? (a) : (b))

enum { mod_bad, mod_world, mod_brush, mod_studio, mod_sprite };

static vec3_t	rgv3tStuckTable[54];
static int	rgStuckLast[MAX_CLIENTS][2];

// Texture names
static int gcTextures = 0;
static char grgszTextureName[CTEXTURESMAX][CBTEXTURENAMEMAX];	
static char grgchTextureType[CTEXTURESMAX];

int g_onladder = 0;

void PM_SwapTextures( int i, int j )
{
	char	chTemp;
	char	szTemp[CBTEXTURENAMEMAX];

	strcpy( szTemp, grgszTextureName[i] );
	chTemp = grgchTextureType[i];
	
	strcpy( grgszTextureName[i], grgszTextureName[j] );
	grgchTextureType[i] = grgchTextureType[j];

	strcpy( grgszTextureName[j], szTemp );
	grgchTextureType[j] = chTemp;
}

void PM_SortTextures( void )
{
	// Bubble sort, yuck, but this only occurs at startup and it's only 512 elements...
	int	i, j;

	for( i = 0 ; i < gcTextures; i++ )
	{
		for( j = i + 1; j < gcTextures; j++ )
		{
			if( stricmp( grgszTextureName[i], grgszTextureName[j] ) > 0 )
				PM_SwapTextures( i, j );	// swap
		}
	}
}

void PM_InitTextureTypes( void )
{
	int		i, j;
	byte		*pMemFile;
	char		buffer[512];
	int		fileSize, filePos;
	static BOOL	bTextureTypeInit = false;

	if( bTextureTypeInit )
		return;

	memset( grgszTextureName, 0, CTEXTURESMAX * CBTEXTURENAMEMAX );
	memset( grgchTextureType, 0, CTEXTURESMAX );

	gcTextures = 0;
	memset( buffer, 0, 512 );

	pMemFile = pmove->COM_LoadFile( "sound/materials.txt", &fileSize );
	if( !pMemFile ) return;

	filePos = 0;

	// for each line in the file...
	while( pmove->memfgets( pMemFile, fileSize, &filePos, buffer, 511 ) != NULL && (gcTextures < CTEXTURESMAX ))
	{
		// skip whitespace
		i = 0;
		while( buffer[i] && isspace( buffer[i] ))
			i++;
		
		if ( !buffer[i] )
			continue;

		// skip comment lines
		if( buffer[i] == '/' || !isalpha( buffer[i] ))
			continue;

		// get texture type
		grgchTextureType[gcTextures] = toupper( buffer[i++] );

		// skip whitespace
		while( buffer[i] && isspace( buffer[i] ))
			i++;
		
		if ( !buffer[i] )
			continue;

		// get sentence name
		j = i;
		while ( buffer[j] && !isspace( buffer[j] ))
			j++;

		if ( !buffer[j] )
			continue;

		// null-terminate name and save in sentences array
		j = min( j, CBTEXTURENAMEMAX-1 + i );
		buffer[j] = 0;
		strcpy( &( grgszTextureName[gcTextures++][0] ), &(buffer[i] ));
	}

	// Must use engine to free since we are in a .dll
	pmove->COM_FreeFile ( pMemFile );

	PM_SortTextures();

	bTextureTypeInit = true;
}

void PM_PlayGroupSound( const char* szValue, int irand, float fvol )
{
	static char	szBuf[128];

	for( int i = 0; szValue[i]; i++ )
	{
		if( szValue[i] == '?' )
		{
			strcpy( szBuf, szValue );
			switch( irand )
			{
			// right foot
			case 0: szBuf[i] = '1'; break;
			case 1: szBuf[i] = '3'; break;
			// left foot
			case 2: szBuf[i] = '2'; break;
			case 3: szBuf[i] = '4'; break;
			default: szBuf[i] = '#';break;
			}
			EmitSound( CHAN_BODY, szBuf, fvol, ATTN_NORM, PITCH_NORM );
			return;
		}
	}
	EmitSound( CHAN_BODY, szValue, fvol, ATTN_NORM, PITCH_NORM );
}

char PM_FindTextureType( const char *name )
{
	int	left, right, pivot;
	int	val;

	assert( pm_shared_initialized );

	left = 0;
	right = gcTextures - 1;

	while ( left <= right )
	{
		pivot = ( left + right ) / 2;

		val = strnicmp( name, grgszTextureName[pivot], CBTEXTURENAMEMAX - 1 );

		if ( val == 0 )
		{
			return grgchTextureType[ pivot ];
		}
		else if ( val > 0 )
		{
			left = pivot + 1;
		}
		else if ( val < 0 )
		{
			right = pivot - 1;
		}
	}
	return CHAR_TEX_CONCRETE;
}

void PM_PlayStepSound( int step, float fvol )
{
	static int	iSkipStep = 0;
	int		irand;
	Vector		hvel;
	const char	*szValue;
	int		iType;
	
	pev->iStepLeft = !pev->iStepLeft;

	if ( !pmove->runfuncs )
	{
		return;
	}
	
	irand = RANDOM_LONG( 0, 1 ) + ( pev->iStepLeft * 2 );

	// FIXME mp_footsteps needs to be a movevar
	if ( pmove->multiplayer && !pmove->movevars->footsteps )
		return;

	hvel = pev->velocity;
	hvel.z = 0.0f;

	if( pmove->multiplayer && ( !g_onladder && hvel.Length() <= 220 ))
		return;

	switch ( step )
	{
	case STEP_LADDER:
		szValue = Info_ValueForKey( "lsnd" );
		if( szValue[0] && szValue[1] )
		{
			PM_PlayGroupSound( szValue, irand, fvol );
			return;
		}
		break;
	case STEP_SLOSH:
		szValue = Info_ValueForKey( "psnd" );
		if( szValue[0] && szValue[1] )
		{
			PM_PlayGroupSound( szValue, irand, fvol );
			return;
		}
		break;
	case STEP_WADE:
		szValue = Info_ValueForKey( "wsnd" );
		if( szValue[0] && szValue[1] )
		{
			if( iSkipStep == 0 )
			{
				iSkipStep++;
				return;
			}

			if( iSkipStep++ == 3 ) iSkipStep = 0;

			PM_PlayGroupSound( szValue, irand, fvol );
			return;
		}
		break;
	default:
		szValue = Info_ValueForKey( "ssnd" );
		if( szValue[0] && szValue[1] )
		{
			PM_PlayGroupSound( szValue, irand, fvol );
			return;
		}
		iType = atoi( Info_ValueForKey( "stype" ));
		if( iType == -1 ) step = STEP_CONCRETE;
		else if( iType ) step = iType;
	}

	// irand - 0,1 for right foot, 2,3 for left foot
	// used to alternate left and right foot
	// FIXME, move to player state

	switch( step )
	{
	default:
	case STEP_CONCRETE:
		switch( irand )
		{
		// right foot
		case 0: EmitSound( CHAN_BODY, "player/pl_step1.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		case 1: EmitSound( CHAN_BODY, "player/pl_step3.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		// left foot
		case 2: EmitSound( CHAN_BODY, "player/pl_step2.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		case 3: EmitSound( CHAN_BODY, "player/pl_step4.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		}
		break;
	case STEP_METAL:
		switch( irand )
		{
		// right foot
		case 0: EmitSound( CHAN_BODY, "player/pl_metal1.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		case 1: EmitSound( CHAN_BODY, "player/pl_metal3.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		// left foot
		case 2: EmitSound( CHAN_BODY, "player/pl_metal2.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		case 3: EmitSound( CHAN_BODY, "player/pl_metal4.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		}
		break;
	case STEP_DIRT:
		switch( irand )
		{
		// right foot
		case 0: EmitSound( CHAN_BODY, "player/pl_dirt1.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		case 1: EmitSound( CHAN_BODY, "player/pl_dirt3.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		// left foot
		case 2: EmitSound( CHAN_BODY, "player/pl_dirt2.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		case 3: EmitSound( CHAN_BODY, "player/pl_dirt4.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		}
		break;
	case STEP_VENT:
		switch( irand )
		{
		// right foot
		case 0: EmitSound( CHAN_BODY, "player/pl_duct1.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		case 1: EmitSound( CHAN_BODY, "player/pl_duct3.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		// left foot
		case 2: EmitSound( CHAN_BODY, "player/pl_duct2.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		case 3: EmitSound( CHAN_BODY, "player/pl_duct4.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		}
		break;
	case STEP_GRATE:
		switch( irand )
		{
		// right foot
		case 0: EmitSound( CHAN_BODY, "player/pl_grate1.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		case 1: EmitSound( CHAN_BODY, "player/pl_grate3.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		// left foot
		case 2: EmitSound( CHAN_BODY, "player/pl_grate2.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		case 3: EmitSound( CHAN_BODY, "player/pl_grate4.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		}
		break;
	case STEP_TILE:
		if( !RANDOM_LONG( 0, 4 ))
			irand = 4;
		switch( irand )
		{
		// right foot
		case 0: EmitSound( CHAN_BODY, "player/pl_tile1.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		case 1: EmitSound( CHAN_BODY, "player/pl_tile3.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		// left foot
		case 2: EmitSound( CHAN_BODY, "player/pl_tile2.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		case 3: EmitSound( CHAN_BODY, "player/pl_tile4.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		case 4: EmitSound( CHAN_BODY, "player/pl_tile5.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		}
		break;
	case STEP_SLOSH:
		switch( irand )
		{
		// right foot
		case 0: EmitSound( CHAN_BODY, "player/pl_slosh1.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		case 1: EmitSound( CHAN_BODY, "player/pl_slosh3.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		// left foot
		case 2: EmitSound( CHAN_BODY, "player/pl_slosh2.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		case 3: EmitSound( CHAN_BODY, "player/pl_slosh4.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		}
		break;
	case STEP_WADE:
		if( iSkipStep == 0 )
		{
			iSkipStep++;
			break;
		}

		if( iSkipStep++ == 3 )
		{
			iSkipStep = 0;
		}

		switch( irand )
		{
		// right foot
		case 0: EmitSound( CHAN_BODY, "player/pl_wade1.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		case 1: EmitSound( CHAN_BODY, "player/pl_wade2.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		// left foot
		case 2: EmitSound( CHAN_BODY, "player/pl_wade3.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		case 3: EmitSound( CHAN_BODY, "player/pl_wade4.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		}
		break;
	case STEP_LADDER:
		switch( irand )
		{
		// right foot
		case 0: EmitSound( CHAN_BODY, "player/pl_ladder1.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		case 1: EmitSound( CHAN_BODY, "player/pl_ladder3.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		// left foot
		case 2: EmitSound( CHAN_BODY, "player/pl_ladder2.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		case 3: EmitSound( CHAN_BODY, "player/pl_ladder4.wav", fvol, ATTN_NORM, PITCH_NORM ); break;
		}
		break;
	}
}	

int PM_MapTextureTypeStepType( char chTextureType )
{
	switch( chTextureType )
	{
	default:
	case CHAR_TEX_CONCRETE: return STEP_CONCRETE;	
	case CHAR_TEX_METAL: return STEP_METAL;	
	case CHAR_TEX_DIRT: return STEP_DIRT;	
	case CHAR_TEX_VENT: return STEP_VENT;	
	case CHAR_TEX_GRATE: return STEP_GRATE;	
	case CHAR_TEX_TILE: return STEP_TILE;
	case CHAR_TEX_SLOSH: return STEP_SLOSH;
	}
}

/*
====================
PM_CatagorizeTextureType

Determine texture info for the texture we are standing on.
====================
*/
void PM_CatagorizeTextureType( void )
{
	Vector		start, end;
	const char	*pTextureName;

	start = end = pmove->origin;

	// Straight down
	end.x -= 64;

	// fill in default values, just in case.
	pmove->szTextureName[0] = '\0';
	pmove->chTextureType = CHAR_TEX_CONCRETE;

	pTextureName = TRACE_TEXTURE( pmove->onground, start, end );
	if ( !pTextureName )
		return;

	// strip leading '-0' or '+0~' or '{' or '!'
	if( *pTextureName == '-' || *pTextureName == '+' )
		pTextureName += 2;

	if( *pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ' )
		pTextureName++;
	// '}}'
	
	strcpy( pmove->szTextureName, pTextureName );
	pmove->szTextureName[CBTEXTURENAMEMAX - 1] = 0;
		
	// get texture type
	pmove->chTextureType = PM_FindTextureType( pmove->szTextureName );	
}

void PM_UpdateStepSound( void )
{
	int	fWalking;
	float	fvol;
	Vector	knee;
	Vector	feet;
	Vector	center;
	float	height;
	float	speed;
	float	velrun;
	float	velwalk;
	float	flduck;
	int	fLadder;
	int	step;

	if( pev->flTimeStepSound > 0 )
		return;

	if( pev->flags & FL_FROZEN )
		return;

	PM_CatagorizeTextureType();

	speed = pev->velocity.Length();

	// determine if we are on a ladder
	fLadder = ( pev->movetype == MOVETYPE_FLY );// IsOnLadder();

	// UNDONE: need defined numbers for run, walk, crouch, crouch run velocities!!!!	
	if(( pev->flags & FL_DUCKING) || fLadder )
	{
		velwalk = 60;	// These constants should be based on cl_movespeedkey * cl_forwardspeed somehow
		velrun = 80;	// UNDONE: Move walking to server
		flduck = 100;
	}
	else
	{
		velwalk = 120;
		velrun = 210;
		flduck = 0;
	}

	// If we're on a ladder or on the ground, and we're moving fast enough,
	// play step sound.  Also, if pev->flTimeStepSound is zero, get the new
	// sound right away - we just started moving in new level.
	if(( fLadder || ( pmove->onground != NULL )) && ( pev->velocity.Length() > 0.0f ) && ( speed >= velwalk || !pev->flTimeStepSound ))
	{
		fWalking = (speed < velrun);		

		center = knee = feet = pmove->origin;

		height = pmove->player_maxs[pmove->usehull].z - pmove->player_mins[pmove->usehull].z;

		knee[2] = pmove->origin.z - 0.3f * height;
		feet[2] = pmove->origin.z - 0.5f * height;

		// find out what we're stepping in or on...
		if( fLadder )
		{
			step = STEP_LADDER;
			fvol = 0.35f;
			pev->flTimeStepSound = 350;
		}
		else if( POINT_CONTENTS( knee ) == CONTENTS_WATER )
		{
			step = STEP_WADE;
			fvol = 0.65f;
			pev->flTimeStepSound = 600;
		}
		else if( POINT_CONTENTS( feet ) == CONTENTS_WATER )
		{
			step = STEP_SLOSH;
			fvol = fWalking ? 0.2 : 0.5;
			pev->flTimeStepSound = fWalking ? 400 : 300;		
		}
		else
		{
			// find texture under player, if different from current texture, 
			// get material type
			step = PM_MapTextureTypeStepType( pmove->chTextureType );

			switch( pmove->chTextureType )
			{
			default:
			case CHAR_TEX_CONCRETE:						
				fvol = fWalking ? 0.2 : 0.5;
				pev->flTimeStepSound = fWalking ? 400 : 300;
				break;
			case CHAR_TEX_METAL:	
				fvol = fWalking ? 0.2 : 0.5;
				pev->flTimeStepSound = fWalking ? 400 : 300;
				break;
			case CHAR_TEX_DIRT:	
				fvol = fWalking ? 0.25 : 0.55;
				pev->flTimeStepSound = fWalking ? 400 : 300;
				break;
			case CHAR_TEX_VENT:	
				fvol = fWalking ? 0.4 : 0.7;
				pev->flTimeStepSound = fWalking ? 400 : 300;
				break;
			case CHAR_TEX_GRATE:
				fvol = fWalking ? 0.2 : 0.5;
				pev->flTimeStepSound = fWalking ? 400 : 300;
				break;
			case CHAR_TEX_TILE:	
				fvol = fWalking ? 0.2 : 0.5;
				pev->flTimeStepSound = fWalking ? 400 : 300;
				break;
			case CHAR_TEX_SLOSH:
				fvol = fWalking ? 0.2 : 0.5;
				pev->flTimeStepSound = fWalking ? 400 : 300;
				break;
			}
		}
		
		pev->flTimeStepSound += flduck; // slower step time if ducking

		// play the sound
		// 35% volume if ducking
		if( pev->flags & FL_DUCKING )
			fvol *= 0.35;

		PM_PlayStepSound( step, fvol );
	}
}

/*
================
PM_AddToTouched

Add's the trace result to touch list, if contact is not already in list.
================
*/
BOOL PM_AddToTouched( edict_t *pHit )
{
	int	i;

	if( !pHit || pHit->free ) // sanity check
		return false;

	for( i = 0; i < pmove->numtouch; i++ )
	{
		if( pmove->touchents[i] == pHit )
			break;
	}
	if( i != pmove->numtouch )  // Already in list.
		return false;

	if( pmove->numtouch >= MAX_PHYSENTS )
		ALERT( at_error, "Too many entities were touched!\n" );

	pmove->touchents[pmove->numtouch++] = pHit;
	return true;
}

/*
================
PM_CheckVelocity

See if the player has a bogus velocity value.
================
*/
void PM_CheckVelocity ( void )
{
	int	i;

	// bound velocity
	for( i = 0; i < 3; i++ )
	{
		// See if it's bogus.
		if( IS_NAN( pev->velocity[i] ))
		{
			ALERT( at_console, "Got a NaN velocity on %i\n", i );
			pev->velocity[i] = 0;
		}
		if (IS_NAN( pmove->origin[i] ))
		{
			ALERT( at_console, "Got a NaN origin on %i\n", i );
			pmove->origin[i] = 0;
		}

		// Bound it.
		if( pev->velocity[i] > pmove->movevars->maxvelocity ) 
		{
			ALERT( at_error, "Got a velocity too high on %i\n", i );
			pev->velocity[i] = pmove->movevars->maxvelocity;
		}
		else if( pev->velocity[i] < -pmove->movevars->maxvelocity )
		{
			ALERT( at_error, "Got a velocity too low on %i\n", i );
			pev->velocity[i] = -pmove->movevars->maxvelocity;
		}
	}
}

/*
==================
PM_ClipVelocity

Slide off of the impacting object
returns the blocked flags:
0x01 == floor
0x02 == step / wall
==================
*/
int PM_ClipVelocity( Vector in, Vector normal, Vector out, float overbounce )
{
	float	backoff;
	float	change;
	float	angle;
	int	i, blocked;
	
	angle = normal.z;

	blocked = 0x00;	// Assume unblocked.
	if( angle > 0 )	// If the plane that is blocking us has a positive z component, then assume it's a floor.
		blocked |= 0x01; 
	if( !angle )         // If the plane has no Z, it is vertical (wall/step)
		blocked |= 0x02; 
	
	// Determine how far along plane to slide based on incoming direction.
	// Scale by overbounce factor.
	backoff = DotProduct( in, normal ) * overbounce;

	for( i = 0; i < 3; i++ )
	{
		change = normal[i] * backoff;
		out[i] = in[i] - change;

		// If out velocity is too small, zero it out.
		if( out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON )
			out[i] = 0;
	}
	
	// Return blocking flags.
	return blocked;
}

void PM_AddCorrectGravity( void )
{
	float	ent_gravity;

	if ( pmove->flWaterJumpTime )
		return;

	if ( pev->gravity )
		ent_gravity = pev->gravity;
	else ent_gravity = 1.0f;

	// Add gravity so they'll be in the correct position during movement
	// yes, this 0.5 looks wrong, but it's not.  
	pev->velocity.z -= (ent_gravity * pmove->movevars->gravity * 0.5f * pmove->frametime );
	pev->velocity.z += pev->basevelocity.z * pmove->frametime;
	pev->basevelocity.z = 0.0f;

	PM_CheckVelocity();
}


void PM_FixupGravityVelocity( void )
{
	float	ent_gravity;

	if ( pmove->flWaterJumpTime )
		return;

	if ( pev->gravity )
		ent_gravity = pev->gravity;
	else ent_gravity = 1.0f;

	// Get the correct velocity for the end of the dt 
  	pev->velocity.z -= (ent_gravity * pmove->movevars->gravity * pmove->frametime * 0.5f );

	PM_CheckVelocity();
}

/*
============
PM_FlyMove

The basic solid body movement clip that slides along multiple planes
============
*/
int PM_FlyMove( void )
{
	int		bumpcount, numbumps;
	Vector		dir;
	float		d;
	int		numplanes;
	Vector		planes[MAX_CLIP_PLANES];
	Vector		primal_velocity, original_velocity;
	Vector      	new_velocity;
	int		i, j;
	TraceResult	trace;
	Vector		end;
	float		time_left, allFraction;
	int		blocked;
		
	numbumps  = 4;	// Bump up to four times
	
	blocked   = 0;	// Assume not blocked
	numplanes = 0;	// and not sliding along any planes

	original_velocity = primal_velocity = pev->velocity;  // Store original velocity
	
	allFraction = 0;
	time_left = pmove->frametime;   // Total time for this movement operation.

	for( bumpcount = 0; bumpcount < numbumps; bumpcount++ )
	{
		if( pev->velocity == g_vecZero )
			break;

		// Assume we can move all the way from the current origin to the
		// end point.
		end = pmove->origin + time_left * pev->velocity;

		// See if we can make it from origin to end point.
		trace = TRACE_PLAYER( pmove->origin, end, PM_NORMAL, (edict_t *)NULL );

		allFraction += trace.flFraction;

		// If we started in a solid object, or we were in solid space
		// the whole way, zero out our velocity and return that we
		// are blocked by floor and wall.
		if( trace.fAllSolid )
		{	
			// entity is trapped in another solid
			pev->velocity = g_vecZero;
			return 4;
		}

		// If we moved some portion of the total distance, then
		//  copy the end position into the pmove->origin and 
		//  zero the plane counter.
		if( trace.flFraction > 0.0f )
		{	
			// actually covered some distance
			pmove->origin = trace.vecEndPos;
			original_velocity = pev->velocity;
			numplanes = 0;
		}

		// If we covered the entire distance, we are done
		//  and can return.
		if( trace.flFraction == 1 )
			 break;		// moved the entire distance

		if( !trace.pHit ) trace.pHit = ENTINDEX( 0 ); // world

		// Save entity that blocked us (since fraction was < 1.0)
		//  for contact
		// Add it if it's not already in the list!!!
		PM_AddToTouched( trace.pHit );

		// If the plane we hit has a high z component in the normal, then
		// it's probably a floor
		if( trace.vecPlaneNormal[2] > 0.7f )
		{
			blocked |= 1; // floor
		}
		// If the plane has a zero z component in the normal, then it's a 
		//  step or wall
		if( !trace.vecPlaneNormal[2] )
		{
			blocked |= 2; // step / wall
			//ALERT( at_console, "Blocked by %i\n", trace.ent);
		}

		// Reduce amount of pmove->frametime left by total time left * fraction
		//  that we covered.
		time_left -= time_left * trace.flFraction;
		
		// Did we run out of planes to clip against?
		if( numplanes >= MAX_CLIP_PLANES )
		{	
			// this shouldn't really happen
			// Stop our movement if so.
			pev->velocity = g_vecZero;
			//ALERT( at_console, "Too many planes 4\n" );
			break;
		}

		// Set up next clipping plane
		planes[numplanes] = trace.vecPlaneNormal;
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		if( pev->movetype == MOVETYPE_WALK && (( pmove->onground == NULL ) || ( pev->friction != 1.0f )))
		{
			// relfect player velocity
			for ( i = 0; i < numplanes; i++ )
			{
				if( planes[i][2] > 0.7f  )
				{
					// floor or slope
					PM_ClipVelocity( original_velocity, planes[i], new_velocity, 1.0f );
					original_velocity = new_velocity;
				}
				else
				{
					float overClip = 1.0f + pmove->movevars->bounce * (1.0f - pev->friction );
					PM_ClipVelocity( original_velocity, planes[i], new_velocity, overClip );
				}
			}
			pev->velocity = original_velocity = new_velocity;
		}
		else
		{
			for ( i = 0; i < numplanes; i++ )
			{
				PM_ClipVelocity( original_velocity, planes[i], pev->velocity, 1.0f );

				for ( j = 0; j < numplanes; j++ )
				{
					if ( j != i )
					{
						// Are we now moving against this plane?
						if( DotProduct( pev->velocity, planes[j] ) < 0.0f )
							break; // not ok
					}
				}
				if ( j == numplanes )  // Didn't have to clip, so we're ok
					break;
			}
			
			// Did we go all the way through plane set
			if ( i != numplanes )
			{	
				// go along this plane
				// pev->velocity is set in clipping call, no need to set again.
			}
			else
			{	
				// go along the crease
				if ( numplanes != 2 )
				{
					//ALERT( at_console, "clip velocity, numplanes == %i\n", numplanes );
					pev->velocity = g_vecZero;
					break;
				}

				dir = CrossProduct( planes[0], planes[1] );
				d = DotProduct( dir, pev->velocity );
				pev->velocity = dir * d;
			}

			// if original velocity is against the original velocity, stop dead
			// to avoid tiny occilations in sloping corners
			if ( DotProduct( pev->velocity, primal_velocity ) <= 0.0f )
			{
				pev->velocity = g_vecZero;
				break;
			}
		}
	}

	if ( allFraction == 0 )
		pev->velocity = g_vecZero;

	return blocked;
}

/*
==============
PM_Accelerate
==============
*/
void PM_Accelerate( Vector wishdir, float wishspeed, float accel )
{
	float	addspeed, accelspeed, currentspeed;

	// Dead player's don't accelerate
	if ( pmove->dead )
		return;

	// If waterjumping, don't accelerate
	if ( pmove->flWaterJumpTime )
		return;

	// See if we are changing direction a bit
	currentspeed = DotProduct( pev->velocity, wishdir );

	// Reduce wishspeed by the amount of veer.
	addspeed = wishspeed - currentspeed;

	// If not going to add any speed, done.
	if ( addspeed <= 0 )
		return;

	// Determine amount of accleration.
	accelspeed = accel * pmove->frametime * wishspeed * pev->friction;
	
	// Cap at addspeed
	if ( accelspeed > addspeed )
		accelspeed = addspeed;
	
	// Adjust velocity.
	pev->velocity += accelspeed * wishdir;	
}

/*
=====================
PM_WalkMove

Only used by players.  Moves along the ground when player is a MOVETYPE_WALK.
======================
*/
void PM_WalkMove( void )
{
	int		clip;
	edict_t		*oldonground;
	TraceResult	trace;

	Vector		wishvel;
	float		spd;
	float		fmove, smove;
	Vector		wishdir;
	float		wishspeed;

	Vector		dest, start;
	Vector		original, originalvel;
	Vector		down, downvel;
	float		downdist, updist;

	// copy movement amounts
	fmove = pmove->cmd.forwardmove;
	smove = pmove->cmd.sidemove;
	
	// Zero out z components of movement vectors
	pmove->forward[2] = 0;
	pmove->right[2]   = 0;
	
	pmove->forward = pmove->forward.Normalize(); 	// Normalize remainder of vectors.
	pmove->right = pmove->right.Normalize();	// 
				// Determine x and y parts of velocity
	wishvel.x = pmove->forward.x * fmove + pmove->right.x * smove;
	wishvel.y = pmove->forward.y * fmove + pmove->right.y * smove;	
	wishvel.z = 0.0f;		// Zero out z part of velocity

	wishdir = wishvel;	// Determine maginitude of speed of move
	wishspeed = wishdir.Length();
	wishdir = wishdir.Normalize();

	// Clamp to server defined max speed
	if ( wishspeed > pmove->maxspeed )
	{
		wishvel *= (pmove->maxspeed / wishspeed);
		wishspeed = pmove->maxspeed;
	}

	// set pmove velocity
	pev->velocity.z = 0.0f;
	PM_Accelerate( wishdir, wishspeed, pmove->movevars->accelerate );
	pev->velocity.z = 0.0f;

	// Add in any base velocity to the current velocity.
	pev->velocity += pev->basevelocity;

	spd = pev->velocity.Length();

	if ( spd < 1.0f )
	{
		pev->velocity = g_vecZero;
		return;
	}

	// If we are not moving, do nothing
	// if ( pev->velocity == g_vecZero )
	//	return;

	oldonground = pmove->onground;

	// first try just moving to the destination	
	dest.x = pmove->origin.x + pev->velocity.x * pmove->frametime;
	dest.y = pmove->origin.y + pev->velocity.y * pmove->frametime;	
	dest.z = pmove->origin.z;

	// first try moving directly to the next spot
	start = dest;
	trace = TRACE_PLAYER (pmove->origin, dest, PM_NORMAL, (edict_t *)NULL );
	// If we made it all the way, then copy trace end
	//  as new player position.
	if ( trace.flFraction == 1.0f )
	{
		pmove->origin = trace.vecEndPos;
		return;
	}

	if ( oldonground == NULL && pev->waterlevel == 0 )  // Don't walk up stairs if not on ground.
		return;

	if ( pmove->flWaterJumpTime )	// If we are jumping out of water, don't do anything more.
		return;

	// Try sliding forward both on ground and up 16 pixels
	// take the move that goes farthest
	original = pmove->origin;	// Save out original pos &
	originalvel = pev->velocity;	//  velocity.

	// Slide move
	clip = PM_FlyMove ();

	// Copy the results out
	down = pmove->origin;
	downvel = pev->velocity;

	// Reset original values.
	pmove->origin = original;
	pev->velocity = originalvel;

	// Start out up one stair height
	dest = pmove->origin;
	dest.z += pmove->movevars->stepsize;
	
	trace = TRACE_PLAYER( pmove->origin, dest, PM_NORMAL, (edict_t *)NULL );

	// If we started okay and made it part of the way at least,
	// copy the results to the movement start position and then
	// run another move try.
	if ( !trace.fStartSolid && !trace.fAllSolid )
	{
		pmove->origin = trace.vecEndPos;
	}

	// slide move the rest of the way.
	clip = PM_FlyMove ();

	// Now try going back down from the end point
	//  press down the stepheight
	dest = pmove->origin;
	dest.z -= pmove->movevars->stepsize;
	
	trace = TRACE_PLAYER( pmove->origin, dest, PM_NORMAL, (edict_t *)NULL );

	// If we are not on the ground any more then
	//  use the original movement attempt
	if ( trace.vecPlaneNormal.z < 0.7f )
		goto usedown;

	// If the trace ended up in empty space, copy the end
	// over to the origin.
	if ( !trace.fStartSolid && !trace.fAllSolid )
	{
		pmove->origin = trace.vecEndPos;
	}

	// Copy this origin to up.
	pmove->up = pmove->origin;

	// decide which one went farther
	downdist = (down[0]-original[0])*(down[0]-original[0]) + (down[1]-original[1]) * (down[1]-original[1]);
	updist   = (pmove->up[0]-original[0])*(pmove->up[0]-original[0])+(pmove->up[1]-original[1])*(pmove->up[1]-original[1]);

	if ( downdist > updist )
	{
usedown:
		pmove->origin = down;
		pev->velocity = downvel;
	}
	else pev->velocity.z = downvel.z; // copy z value from slide move
}

/*
==================
PM_Friction

Handles both ground friction and water friction
==================
*/
void PM_Friction( void )
{
	float	speed, newspeed, control;
	float	friction;
	float	drop;
	Vector	newvel;
	
	// If we are in water jump cycle, don't apply friction
	if ( pmove->flWaterJumpTime )
		return;

	// Calculate speed
	speed = pev->velocity.Length();
	
	// If too slow, return
	if ( speed < 0.1f )
	{
		return;
	}

	drop = 0;

	// apply ground friction
	if ( pmove->onground != NULL )  // On an entity that is the ground
	{
		Vector		start, stop;
		TraceResult	trace;

		start.x = stop.x = pmove->origin.x + pev->velocity.x / speed * 16;
		start.y = stop.y = pmove->origin.y + pev->velocity.y / speed * 16;
		start.z = pmove->origin.z + pmove->player_mins[pmove->usehull].z;
		stop.z = start.z - 34;

		trace = TRACE_PLAYER (start, stop, PM_NORMAL, (edict_t *)NULL );

		if ( trace.flFraction == 1.0 )
			friction = pmove->movevars->friction * pmove->movevars->edgefriction;
		else friction = pmove->movevars->friction;
		
		// Grab friction value.
		// friction = pmove->movevars->friction;      
		friction *= pev->friction;  // player friction?

		// Bleed off some speed, but if we have less than the bleed
		//  threshhold, bleed the theshold amount.
		control = (speed < pmove->movevars->stopspeed) ? pmove->movevars->stopspeed : speed;

		// Add the amount to t'he drop amount.
		drop += control * friction * pmove->frametime;
	}

	// apply water friction
//	if ( pev->waterlevel )
//		drop += speed * pmove->movevars->waterfriction * waterlevel * pmove->frametime;

	// scale the velocity
	newspeed = speed - drop;
	if ( newspeed < 0 ) newspeed = 0;

	// Determine proportion of old speed we are using.
	newspeed /= speed;

	// Adjust velocity according to proportion.
	pev->velocity *= newspeed;
}

void PM_AirAccelerate( Vector wishdir, float wishspeed, float accel )
{
	float	addspeed, accelspeed, currentspeed;
	float	wishspd = wishspeed;
		
	if ( pmove->dead )
		return;
	if ( pmove->flWaterJumpTime )
		return;

	// Cap speed
	if( wishspd > 30 ) wishspd = 30;
	// Determine veer amount
	currentspeed = DotProduct( pev->velocity, wishdir );
	// See how much to add
	addspeed = wishspd - currentspeed;
	// If not adding any, done.
	if ( addspeed <= 0 )
		return;
	// Determine acceleration speed after acceleration

	accelspeed = accel * wishspeed * pmove->frametime * pev->friction;
	// Cap it
	if ( accelspeed > addspeed )
		accelspeed = addspeed;
	
	// Adjust pmove vel.
	pev->velocity += accelspeed * wishdir;	
}

/*
===================
PM_WaterMove

===================
*/
void PM_WaterMove( void )
{
	Vector		temp;
	TraceResult	trace;
	Vector		start, dest;
	float		speed, newspeed;
	Vector		wishvel, wishdir;
	float		wishspeed, addspeed, accelspeed;

	// user intentions
	wishvel = pmove->forward * pmove->cmd.forwardmove + pmove->right * pmove->cmd.sidemove;

	// Sinking after no other movement occurs
	if( !pmove->cmd.forwardmove && !pmove->cmd.sidemove && !pmove->cmd.upmove && pev->watertype != CONTENTS_FLYFIELD )
		wishvel[2] -= 60;		// drift towards bottom
	else wishvel[2] += pmove->cmd.upmove;	// Go straight up by upmove amount.

	// Copy it over and determine speed
	wishdir = wishvel;
	wishspeed = wishdir.Length();
	wishdir = wishdir.Normalize();

	// Cap speed.
	if( wishspeed > pmove->maxspeed )
	{
		wishvel *= (pmove->maxspeed / wishspeed );
		wishspeed = pmove->maxspeed;
	}

	// Slow us down a bit.
	wishspeed *= 0.8f;

	pev->velocity += pev->basevelocity;

	// Water friction
	temp = pev->velocity;
	speed = temp.Length();
	temp = temp.Normalize();

	if( speed )
	{
		newspeed = speed - pmove->frametime * speed * pmove->movevars->friction * pev->friction;

		if( newspeed < 0.0f ) newspeed = 0.0f;
		pev->velocity *= ( newspeed / speed );
	}
	else newspeed = 0.0f;

	// water acceleration
	if ( wishspeed < 0.1f )
	{
		return;
	}

	addspeed = wishspeed - newspeed;
	if ( addspeed > 0.0f )
	{
		wishvel = wishvel.Normalize();
		accelspeed = pmove->movevars->accelerate * wishspeed * pmove->frametime * pev->friction;
		if( accelspeed > addspeed ) accelspeed = addspeed;

		pev->velocity += accelspeed * wishvel;
	}

	// Now move
	// assume it is a stair or a slope, so press down from stepheight above
	dest = pmove->origin + pev->velocity * pmove->frametime;
	start = dest;

	start.z += pmove->movevars->stepsize + 1;
	trace = TRACE_PLAYER( start, dest, PM_NORMAL, (edict_t *)NULL );

	if( !trace.fStartSolid && !trace.fAllSolid ) // FIXME: check steep slope?
	{	
		// walked up the step, so just keep result and exit
		pmove->origin = trace.vecEndPos;
		return;
	}
	
	// Try moving straight along out normal path.
	PM_FlyMove ();
}


/*
===================
PM_AirMove

===================
*/
void PM_AirMove( void )
{
	Vector	wishvel;
	float	fmove, smove;
	Vector	wishdir;
	float	wishspeed;

	// Copy movement amounts
	fmove = pmove->cmd.forwardmove;
	smove = pmove->cmd.sidemove;
	
	// Zero out z components of movement vectors
	pmove->forward.z = 0.0f;
	pmove->right.z = 0.0f;

	// Renormalize
	pmove->forward = pmove->forward.Normalize();
	pmove->right = pmove->right.Normalize();

	// Determine x and y parts of velocity
	wishvel.x = pmove->forward.x * fmove + pmove->right.x * smove;
	wishvel.y = pmove->forward.y * fmove + pmove->right.y * smove;

	// Zero out z part of velocity
	wishvel.z = 0.0f;             

	 // Determine maginitude of speed of move
	wishdir = wishvel;  
	wishspeed = wishdir.Length();
	wishdir = wishdir.Normalize();

	// Clamp to server defined max speed
	if( wishspeed > pmove->maxspeed )
	{
		wishvel *= (pmove->maxspeed / wishspeed );
		wishspeed = pmove->maxspeed;
	}
	
	PM_AirAccelerate( wishdir, wishspeed, pmove->movevars->airaccelerate );

	// Add in any base velocity to the current velocity.
	pev->velocity += pev->basevelocity;

	PM_FlyMove ();
}

BOOL PM_InWater( void )
{
	return ( pev->waterlevel > 1 );
}

/*
=============
PM_CheckWater

Sets pev->waterlevel and pev->watertype values.
=============
*/
BOOL PM_CheckWater( void )
{
	Vector	point;
	int	cont;
	float     height;
	float	heightover2;

	// Pick a spot just above the players feet.
	point[0] = pmove->origin[0] + (pmove->player_mins[pmove->usehull][0] + pmove->player_maxs[pmove->usehull][0]) * 0.5f;
	point[1] = pmove->origin[1] + (pmove->player_mins[pmove->usehull][1] + pmove->player_maxs[pmove->usehull][1]) * 0.5f;
	point[2] = pmove->origin[2] + (pmove->player_mins[pmove->usehull][2] + 1);
	
	// Assume that we are not in water at all.
	pev->waterlevel = 0;
	pev->watertype = CONTENTS_EMPTY;

	// Grab point contents.
	cont = POINT_CONTENTS( point );

	// Are we under water? (not solid and not empty?)
	if ( cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
	{
		// Set water type
		pev->watertype = cont;

		// We are at least at level one
		pev->waterlevel = 1;

		height = (pmove->player_mins[pmove->usehull][2] + pmove->player_maxs[pmove->usehull][2]);
		heightover2 = height * 0.5f;

		// Now check a point that is at the player hull midpoint.
		point[2] = pmove->origin[2] + heightover2;
		cont = POINT_CONTENTS (point );

		// If that point is also under water...
		if ( cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT )
		{
			// Set a higher water level.
			pev->waterlevel = 2;

			// Now check the eye position.  (view_ofs is relative to the origin)
			point[2] = pmove->origin[2] + pev->view_ofs[2];

			cont = POINT_CONTENTS( point );
			if ( cont <= CONTENTS_WATER && cont > CONTENTS_TRANSLUCENT ) 
				pev->waterlevel = 3; // In over our eyes
		}

		// Adjust velocity based on water current, if any.
		if (( cont <= CONTENTS_CURRENT_0 ) && ( cont >= CONTENTS_CURRENT_DOWN ))
		{
			// The deeper we are, the stronger the current.
			Vector current_table[] =
			{
			Vector( 1, 0, 0 ),
			Vector( 0, 1, 0 ),
			Vector(-1, 0, 0 ),
			Vector( 0,-1, 0 ),
			Vector( 0, 0, 1 ),
			Vector( 0, 0,-1 )
			};
                              
			pev->basevelocity += current_table[CONTENTS_CURRENT_0 - cont] * (50.0f * pev->waterlevel);
		}
	}
	return pev->waterlevel > 1;
}

/*
=============
PM_CatagorizePosition
=============
*/
void PM_CatagorizePosition( void )
{
	Vector		point;
	TraceResult	tr;

	// if the player hull point one unit down is solid, the player
	// is on ground

	// see if standing on something solid	

	// Doing this before we move may introduce a potential latency in water detection, but
	// doing it after can get us stuck on the bottom in water if the amount we move up
	// is less than the 1 pixel 'threshold' we're about to snap to.	Also, we'll call
	// this several times per frame, so we really need to avoid sticking to the bottom of
	// water on each call, and the converse case will correct itself if called twice.
	PM_CheckWater();

	point[0] = pmove->origin[0];
	point[1] = pmove->origin[1];
	point[2] = pmove->origin[2] - 2;

	if ( pev->velocity[2] > 180 ) // Shooting up really fast.  Definitely not on ground.
	{
		pmove->onground = (edict_t *)NULL;
	}
	else
	{
		// Try and move down.
		tr = TRACE_PLAYER( pmove->origin, point, PM_NORMAL, (edict_t *)NULL );

		// If we hit a steep plane, we are not on ground
		if ( tr.vecPlaneNormal[2] < 0.7f )
			pmove->onground = (edict_t *)NULL;	// too steep
		else pmove->onground = tr.pHit;		// Otherwise, point to index of ent under us.

		// If we are on something...
		if ( pmove->onground != NULL )
		{
			// Then we are not in water jump sequence
			pmove->flWaterJumpTime = 0;
			// If we could make the move, drop us down that 1 pixel
			if ( pev->waterlevel < 2 && !tr.fStartSolid && !tr.fAllSolid )
				pmove->origin = tr.vecEndPos;
		}

		// Standing on an entity other than the world
		if ( tr.pHit && tr.pHit != ENTINDEX( 0 ))
		{
			// So signal that we are touching something.
			PM_AddToTouched( tr.pHit );
		}
	}
}

/*
=================
PM_GetRandomStuckOffsets

When a player is stuck, it's costly to try and unstick them
Grab a test offset for the player based on a passed in index
=================
*/
int PM_GetRandomStuckOffsets( int nIndex, int server, Vector &offset )
{
	// Last time we did a full
	int	idx = rgStuckLast[nIndex][server]++;

	offset = rgv3tStuckTable[idx % 54];

	return (idx % 54);
}

void PM_ResetStuckOffsets( int nIndex, int server )
{
	rgStuckLast[nIndex][server] = 0;
}

/*
=================
NudgePosition

If pmove->origin is in a solid position,
try nudging slightly on all axis to
allow for the cut precision of the net coordinates
=================
*/
int PM_CheckStuck( void )
{
	Vector		base;
	Vector		offset;
	Vector		test;
	edict_t		*hitent;
	int		idx;
	float		fTime;
	int		i;
	static float	rgStuckCheckTime[MAX_CLIENTS][2]; // Last time we did a full

	// If position is okay, exit
	hitent = TEST_PLAYER( pmove->origin );

	if( hitent == NULL )
	{
		PM_ResetStuckOffsets( player_index(), pmove->server );
		return 0;
	}

	base = pmove->origin;

	// Deal with precision error in network.
	if ( !pmove->server )
	{
		// World or BSP model
		if (( hitent == NULL ) || ( hitent->v.model != 0 ))
		{
			int	nReps = 0;

			PM_ResetStuckOffsets( player_index(), pmove->server );

			do 
			{
				i = PM_GetRandomStuckOffsets(player_index(), pmove->server, offset);

				test = base + offset;
				if( TEST_PLAYER( test ) == NULL )
				{
					PM_ResetStuckOffsets( player_index(), pmove->server );
					pmove->origin = test;
					return 0;
				}
				nReps++;

			} while ( nReps < 54 );
		}
	}

	// Only an issue on the client.

	if ( pmove->server )
		idx = 0;
	else idx = 1;

	fTime = pmove->realtime;

	// Too soon?
	if ( rgStuckCheckTime[player_index()][idx] >= ( fTime - PM_CHECKSTUCK_MINTIME ))
		return 1;

	rgStuckCheckTime[player_index()][idx] = fTime;
	TEST_STUCK( hitent );

	i = PM_GetRandomStuckOffsets( player_index(), pmove->server, offset );
	test = base + offset;

	if(( hitent = TEST_PLAYER( test )) == NULL )
	{
		// ALERT( at_console, "Nudged\n" );

		PM_ResetStuckOffsets( player_index(), pmove->server );

		if( i >= 27 ) pmove->origin = test;
		return 0;
	}

	// If player is flailing while stuck in another player ( should never happen ), then see
	// if we can't "unstick" them forceably.
	if( pmove->cmd.buttons & (IN_JUMP|IN_DUCK|IN_ATTACK) && ( hitent && hitent->v.flags & FL_CLIENT ))
	{
		float	x, y, z;
		float	xystep = 8.0;
		float	zstep = 18.0;
		float	xyminmax = xystep;
		float	zminmax = 4 * zstep;
		
		for ( z = 0; z <= zminmax; z += zstep )
		{
			for ( x = -xyminmax; x <= xyminmax; x += xystep )
			{
				for ( y = -xyminmax; y <= xyminmax; y += xystep )
				{
					test = base;
					test[0] += x;
					test[1] += y;
					test[2] += z;

					if( TEST_PLAYER( test ) == NULL )
					{
						pmove->origin = test;
						return 0;
					}
				}
			}
		}
	}

	// pmove->origin = base;

	return 1;
}

/*
===============
PM_SpectatorMove
===============
*/
void PM_SpectatorMove( void )
{
	float		speed, drop, friction, control, newspeed;
	float		currentspeed, addspeed, accelspeed;
	Vector		wishvel;
	float		fmove, smove;
	Vector		wishdir;
	float		wishspeed;

	// this routine keeps track of the spectators position
	// there a two different main move types : track player or moce freely (OBS_ROAMING)
	// doesn't need excate track position, only to generate PVS, so just copy
	// targets position and real view position is calculated on client (saves server CPU)
	
	if ( pev->iSpecMode == OBS_ROAMING )
	{
		// Move around in normal spectator method
		speed = pev->velocity.Length();
		if( speed < 1.0f )
		{
			pev->velocity = g_vecZero;
		}
		else
		{
			drop = 0;
			friction = pmove->movevars->friction * 1.5f; // extra friction
			control = speed < pmove->movevars->stopspeed ? pmove->movevars->stopspeed : speed;
			drop += control*friction*pmove->frametime;

			// scale the velocity
			newspeed = speed - drop;
			if( newspeed < 0.0f ) newspeed = 0.0f;
			newspeed /= speed;

			pev->velocity *= newspeed;
		}

		// accelerate
		fmove = pmove->cmd.forwardmove;
		smove = pmove->cmd.sidemove;
		
		pmove->forward = pmove->forward.Normalize();
		pmove->right = pmove->right.Normalize();

		wishvel = pmove->forward * fmove + pmove->right * smove;
		wishvel.z += pmove->cmd.upmove;

		wishdir = wishvel;
		wishspeed = wishdir.Length();
		wishdir = wishdir.Normalize();

		// clamp to server defined max speed
		if( wishspeed > pmove->movevars->spectatormaxspeed )
		{
			wishvel *= ( pmove->movevars->spectatormaxspeed / wishspeed );
			wishspeed = pmove->movevars->spectatormaxspeed;
		}

		currentspeed = DotProduct( pev->velocity, wishdir );
		addspeed = wishspeed - currentspeed;
		if( addspeed <= 0.0f ) return;

		accelspeed = pmove->movevars->accelerate * pmove->frametime * wishspeed;
		if( accelspeed > addspeed ) accelspeed = addspeed;
		pev->velocity += accelspeed * wishdir;	

		// move
		pmove->origin += pev->velocity * pmove->frametime;
	}
	else
	{
		// all other modes just track some kind of target, so spectator PVS = target PVS

		// no valid target ?
		if( !pev->aiment || pev->aiment->free )
			return;

		// use targets position as own origin for PVS
		pev->angles = pev->aiment->v.angles;
		pmove->origin = pev->aiment->v.origin;

		// no velocity
		pev->velocity = g_vecZero;
	}
}

/*
==================
PM_SplineFraction

Use for ease-in, ease-out style interpolation (accel/decel)
Used by ducking code.
==================
*/
float PM_SplineFraction( float value, float scale )
{
	float valueSquared;

	value = scale * value;
	valueSquared = value * value;

	// Nice little ease-in, ease-out spline-like curve
	return 3 * valueSquared - 2 * valueSquared * value;
}

void PM_FixPlayerCrouchStuck( int direction )
{
	edict_t	*hitent;
	int	i;
	Vector	test;

	hitent = TEST_PLAYER ( pmove->origin );
	if( hitent == NULL ) return;
	
	test = pmove->origin;	
	for ( i = 0; i < 36; i++ )
	{
		pmove->origin.z += direction;
		hitent = TEST_PLAYER( pmove->origin );
		if( hitent == (edict_t *)NULL ) return;
	}

	pmove->origin = test; // Failed
}

void PM_UnDuck( void )
{
	TraceResult	trace;
	Vector		newOrigin;

	newOrigin = pmove->origin;

	if( pmove->onground != NULL )
	{
		newOrigin += (pmove->player_mins[1] - pmove->player_mins[0]);
	}
	
	trace = TRACE_PLAYER( newOrigin, newOrigin, PM_NORMAL, (edict_t *)NULL );

	if ( !trace.fStartSolid )
	{
		pmove->usehull = 0;

		// Oh, no, changing hulls stuck us into something, try unsticking downward first.
		trace = TRACE_PLAYER( newOrigin, newOrigin, PM_NORMAL, (edict_t *)NULL  );

		if( trace.fStartSolid )
		{
			// See if we are stuck?  If so, stay ducked with the duck hull until we have a clear spot
			// ALERT( at_console, "unstick got stuck\n" );
			pmove->usehull = 1;
			return;
		}

		pev->flags &= ~FL_DUCKING;
		pev->bInDuck = false;
		pev->view_ofs.z = VEC_VIEW;
		pev->flDuckTime = 0.0f;
		
		pmove->origin = newOrigin;

		// Recatagorize position since ducking can change origin
		PM_CatagorizePosition();
	}
}

void PM_Duck( void )
{
	float	time;
	float	duckFraction;
	int	buttonsChanged = ( pev->oldbuttons ^ pmove->cmd.buttons );	// These buttons have changed this frame
	int	nButtonPressed =  buttonsChanged & pmove->cmd.buttons; // The changed ones still down are "pressed"
	int	duckchange = buttonsChanged & IN_DUCK ? 1 : 0;
	int	duckpressed = nButtonPressed & IN_DUCK ? 1 : 0;

	if( pmove->cmd.buttons & IN_DUCK )
		pev->oldbuttons |= IN_DUCK;
	else pev->oldbuttons &= ~IN_DUCK;

	// Prevent ducking if the iuser3 variable is set
	if( pmove->dead )
	{
		// Try to unduck
		if( pev->flags & FL_DUCKING )
			PM_UnDuck();
		return;
	}

	if( pev->flags & FL_DUCKING )
	{
		pmove->cmd.forwardmove *= 0.333;
		pmove->cmd.sidemove *= 0.333;
		pmove->cmd.upmove *= 0.333;
	}

	if(( pmove->cmd.buttons & IN_DUCK ) || ( pev->bInDuck ) || ( pev->flags & FL_DUCKING ))
	{
		if ( pmove->cmd.buttons & IN_DUCK )
		{
			if ( (nButtonPressed & IN_DUCK ) && !( pev->flags & FL_DUCKING ) )
			{
				// Use 1 second so super long jump will work
				pev->flDuckTime = 1000;
				pev->bInDuck = true;
			}

			time = max( 0.0f, ( 1.0f - (float)pev->flDuckTime / 1000.0f ));
			
			if( pev->bInDuck )
			{
				// Finish ducking immediately if duck time is over or not on ground
				if(((float)pev->flDuckTime / 1000.0f <= ( 1.0f - TIME_TO_DUCK )) || ( pmove->onground == NULL ))
				{
					pmove->usehull = 1;
					pev->view_ofs[2] = VEC_DUCK_VIEW;
					pev->flags |= FL_DUCKING;
					pev->bInDuck = false;

					// HACKHACK - Fudge for collision bug - no time to fix this properly
					if( pmove->onground != NULL )
					{
						pmove->origin -= (pmove->player_mins[1] - pmove->player_mins[0]);

						// See if we are stuck?
						PM_FixPlayerCrouchStuck( STUCK_MOVEUP );

						// Recatagorize position since ducking can change origin
						PM_CatagorizePosition();
					}
				}
				else
				{
					float	fMore = (VEC_DUCK_HULL_MIN - VEC_HULL_MIN);

					// Calc parametric time
					duckFraction = PM_SplineFraction( time, (1.0f / TIME_TO_DUCK ));
					pev->view_ofs.z = ((VEC_DUCK_VIEW - fMore) * duckFraction) + (VEC_VIEW * (1.0f - duckFraction));
				}
			}
		}
		else
		{
			// Try to unduck
			PM_UnDuck();
		}
	}
}

void PM_LadderMove( edict_t *pLadder )
{
	Vector		ladderCenter;
	TraceResult	trace;
	BOOL		onFloor;
	Vector		floor;
	Vector		modelmins, modelmaxs;

	if( pev->movetype == MOVETYPE_NOCLIP )
		return;

	Mod_GetBounds( pLadder->v.modelindex, modelmins, modelmaxs );

	ladderCenter = (modelmins + modelmaxs) * 0.5f;
	ladderCenter += pLadder->v.origin; // allow for ladders moving around
	pev->movetype = MOVETYPE_FLY;

	// On ladder, convert movement to be relative to the ladder
	floor = pmove->origin;
	floor.z += pmove->player_mins[pmove->usehull].z - 1;

	if( POINT_CONTENTS( floor ) == CONTENTS_SOLID )
		onFloor = true;
	else onFloor = false;

	pev->gravity = 0;
	trace = TRACE_MODEL( pLadder, pmove->origin, ladderCenter );

	if( trace.flFraction != 1.0f )
	{
		float	forward = 0, right = 0;
		Vector	vpn, v_right;

		AngleVectors( pev->angles, vpn, v_right, (float *)NULL );

		if( pmove->cmd.buttons & IN_BACK ) forward -= MAX_CLIMB_SPEED;
		if( pmove->cmd.buttons & IN_FORWARD ) forward += MAX_CLIMB_SPEED;
		if( pmove->cmd.buttons & IN_MOVELEFT ) right -= MAX_CLIMB_SPEED;
		if( pmove->cmd.buttons & IN_MOVERIGHT ) right += MAX_CLIMB_SPEED;

		if( pmove->cmd.buttons & IN_JUMP )
		{
			pev->movetype = MOVETYPE_WALK;
			pev->velocity = trace.vecPlaneNormal * 270;
		}
		else
		{
			if( forward != 0 || right != 0 )
			{
				Vector	velocity, perp, cross, lateral, tmp;
				float	normal;

				// ALERT( at_console, "pev %.2f %.2f %.2f - ", pev->velocity.x, pev->velocity.y, pev->velocity.z );
				// Calculate player's intended velocity
				// Vector velocity = (forward * pmove->forward) + (right * pmove->right);
				velocity = vpn * forward;
				velocity += v_right * right;

				
				// Perpendicular in the ladder plane
				perp = CrossProduct( Vector( 0, 0, 1 ), trace.vecPlaneNormal );
				perp = perp.Normalize();

				// decompose velocity into ladder plane
				normal = DotProduct( velocity, trace.vecPlaneNormal );
				// This is the velocity into the face of the ladder
				cross = trace.vecPlaneNormal * normal;

				// This is the player's additional velocity
				lateral = velocity - cross;

				// This turns the velocity into the face of the ladder into velocity that
				// is roughly vertically perpendicular to the face of the ladder.
				// NOTE: It IS possible to face up and move down or face down and move up
				// because the velocity is a sum of the directional velocity and the converted
				// velocity through the face of the ladder -- by design.
				tmp = CrossProduct( trace.vecPlaneNormal, perp );
				pev->velocity = lateral + (tmp * -normal);

				if( onFloor && normal > 0.0f ) // On ground moving away from the ladder
				{
					pev->velocity += trace.vecPlaneNormal * MAX_CLIMB_SPEED;
				}
				// pev->velocity = lateral - (CrossProduct( trace.vecPlaneNormal, perp ) * normal);
			}
			else pev->velocity = g_vecZero;
		}
	}
}

edict_t *PM_Ladder( void )
{
	edict_t	*e;
	Vector	test;
	int	i;

	for( i = 0; i < pmove->numladders; i++ )
	{
		e = pmove->ladders[i];
		
		if( Mod_GetType( e->v.modelindex ) == mod_brush && e->v.skin == CONTENTS_LADDER )
		{
			// Offset the test point appropriately for this hull.
			test = pmove->origin - test;

			// Test the player's hull for intersection with this model
			if( POINT_CONTENTS( test ) == CONTENTS_EMPTY )
				continue;
			
			return e;
		}
	}
	return (edict_t *)NULL;
}

void PM_WaterJump( void )
{
	if( pmove->flWaterJumpTime > 10000.0f )
		pmove->flWaterJumpTime = 10000.0f;

	if( !pmove->flWaterJumpTime )
		return;

	pmove->flWaterJumpTime -= pmove->cmd.msec;
	if ( pmove->flWaterJumpTime < 0 || !pev->waterlevel )
	{
		pmove->flWaterJumpTime = 0;
		pev->flags &= ~FL_WATERJUMP;
	}

	pev->velocity.x = pev->movedir.x;
	pev->velocity.y = pev->movedir.y;
}

/*
============
PM_AddGravity

============
*/
void PM_AddGravity( void )
{
	float	ent_gravity;

	if( pev->gravity )
		ent_gravity = pev->gravity;
	else ent_gravity = 1.0f;

	// Add gravity incorrectly
	pev->velocity.z -= (ent_gravity * pmove->movevars->gravity * pmove->frametime );
	pev->velocity.z += pev->basevelocity.z * pmove->frametime;
	pev->basevelocity.z = 0.0f;

	PM_CheckVelocity();
}

/*
============
PM_PushEntity

Does not change the entities velocity at all
============
*/
TraceResult PM_PushEntity( Vector push )
{
	TraceResult	trace;
	Vector		end;
		
	end = pmove->origin + push;
	trace = TRACE_PLAYER( pmove->origin, end, PM_NORMAL, (edict_t *)NULL );
	pmove->origin = trace.vecEndPos;

	// So we can run impact function afterwards.
	if( trace.flFraction < 1.0f && !trace.fAllSolid )
		PM_AddToTouched( trace.pHit );
	return trace;
}	

/*
============
PM_Physics_Toss

Dead player flying through air., e.g.
============
*/
void PM_Physics_Toss( void )
{
	TraceResult	trace;
	Vector		move;
	float		backoff;

	PM_CheckWater();

	if( pev->velocity.z > 0 )
		pmove->onground = (edict_t *)NULL;

	// If on ground and not moving, return.
	if( pmove->onground != NULL )
	{
		if( pev->basevelocity == g_vecZero && pev->velocity == g_vecZero )
			return;
	}

	PM_CheckVelocity ();

	// add gravity
	if( pev->movetype != MOVETYPE_FLY &&  pev->movetype != MOVETYPE_BOUNCEMISSILE && pev->movetype != MOVETYPE_FLYMISSILE )
		PM_AddGravity ();

	// move origin
	// Base velocity is not properly accounted for since this entity will move again
	// after the bounce without taking it into account
	pev->velocity += pev->basevelocity;
	
	PM_CheckVelocity();
	move = pev->velocity * pmove->frametime;
	pev->velocity = pev->basevelocity - pev->velocity;

	trace = PM_PushEntity( move ); // Should this clear basevelocity

	PM_CheckVelocity();

	if( trace.fAllSolid )
	{	
		// entity is trapped in another solid
		pmove->onground = trace.pHit;
		pev->velocity = g_vecZero;
		return;
	}
	
	if( trace.flFraction == 1.0f )
	{
		PM_CheckWater();
		return;
	}

	if( pev->movetype == MOVETYPE_BOUNCE )
		backoff = 2.0f - pev->friction;
	else if( pev->movetype == MOVETYPE_BOUNCEMISSILE )
		backoff = 2.0f;
	else backoff = 1.0f;

	PM_ClipVelocity( pev->velocity, trace.vecPlaneNormal, pev->velocity, backoff );

	// stop if on ground
	if( trace.vecPlaneNormal.z > 0.7f )
	{		
		float	vel;
		Vector	base;

		base = g_vecZero;
		if( pev->velocity.z < pmove->movevars->gravity * pmove->frametime )
		{
			// we're rolling on the ground, add static friction.
			pmove->onground = trace.pHit;
			pev->velocity.z = 0.0f;
		}

		vel = DotProduct( pev->velocity, pev->velocity );

		// ALERT( at_console, "%f %f: %.0f %.0f %.0f\n", vel, trace.flFraction, pev->velocity.x, pev->velocity.y, pev->velocity.z );

		if( vel < 900 || ( pev->movetype != MOVETYPE_BOUNCE && pev->movetype != MOVETYPE_BOUNCEMISSILE ))
		{
			pmove->onground = trace.pHit;
			pev->velocity = g_vecZero;
		}
		else
		{
			move = pev->velocity * ((1.0f - trace.flFraction) * pmove->frametime * 0.9f);
			trace = PM_PushEntity (move);
		}
		pev->velocity -= base;
	}
	
	// check for in water
	PM_CheckWater();
}

/*
====================
PM_NoClip

====================
*/
void PM_NoClip( void )
{
	Vector	wishvel;
	float	fmove, smove;

	// Copy movement amounts
	fmove = pmove->cmd.forwardmove;
	smove = pmove->cmd.sidemove;
	
	pmove->forward = pmove->forward.Normalize(); 
	pmove->right = pmove->right.Normalize();

	// Determine x and y parts of velocity
	wishvel = pmove->forward * fmove + pmove->right * smove;
	wishvel.z += pmove->cmd.upmove;

	pmove->origin += wishvel * pmove->frametime;
	
	// Zero out the velocity so that we don't accumulate a huge downward velocity from
	// gravity, etc.
	pev->velocity = g_vecZero;
}

//-----------------------------------------------------------------------------
// Purpose: Corrects bunny jumping ( where player initiates a bunny jump before other
//  movement logic runs, thus making onground == -1 thus making PM_Friction get skipped and
//  running PM_AirMove, which doesn't crop velocity to maxspeed like the ground / other
//  movement logic does.
//-----------------------------------------------------------------------------
void PM_PreventMegaBunnyJumping( void )
{
	float	spd;		// Current player speed
	float	fraction;		// If we have to crop, apply this cropping fraction to velocity
	float	maxscaledspeed;	// Speed at which bunny jumping is limited

	maxscaledspeed = BUNNYJUMP_MAX_SPEED_FACTOR * pmove->maxspeed;

	// Don't divide by zero
	if( maxscaledspeed <= 0.0f )
		return;

	spd = pev->velocity.Length();

	if( spd <= maxscaledspeed )
		return;

	fraction = ( maxscaledspeed / spd ) * 0.65f;	// Returns the modifier for the velocity
	pev->velocity *= fraction;			// Crop it down!.
}

/*
=============
PM_Jump
=============
*/
void PM_Jump( void )
{
	if( pmove->dead )
	{
		pev->oldbuttons |= IN_JUMP; // don't jump again until released
		return;
	}

	BOOL tfc = atoi( Info_ValueForKey( "tfc" )) == 1 ? true : false;

	// Spy that's feigning death cannot jump
	if( tfc && ( pev->deadflag == ( DEAD_DISCARDBODY + 1 )))
	{
		return;
	}

	// See if we are waterjumping.  If so, decrement count and return.
	if( pmove->flWaterJumpTime )
	{
		pmove->flWaterJumpTime -= pmove->cmd.msec;
		if( pmove->flWaterJumpTime < 0.0f )
			pmove->flWaterJumpTime = 0.0f;
		return;
	}

	// If we are in the water most of the way...
	if( pev->waterlevel >= 2 )
	{	
		// swimming, not jumping
		pmove->onground = (edict_t *)NULL;

		if( pev->watertype == CONTENTS_WATER ) // We move up a certain amount
			pev->velocity.z = 100;
		else if( pev->watertype == CONTENTS_SLIME )
			pev->velocity.z = 80;
		else pev->velocity.z = 50; // LAVA

		// play swiming sound
		if( pev->flSwimTime <= 0.0f )
		{
			// Don't play sound again for 1 second
			pev->flSwimTime = 1000;

			switch( RANDOM_LONG( 0, 3 ))
			{ 
			case 0:
				EmitSound( CHAN_BODY, "player/pl_wade1.wav", 1, ATTN_NORM, PITCH_NORM );
				break;
			case 1:
				EmitSound( CHAN_BODY, "player/pl_wade2.wav", 1, ATTN_NORM, PITCH_NORM );
				break;
			case 2:
				EmitSound( CHAN_BODY, "player/pl_wade3.wav", 1, ATTN_NORM, PITCH_NORM );
				break;
			case 3:
				EmitSound( CHAN_BODY, "player/pl_wade4.wav", 1, ATTN_NORM, PITCH_NORM );
				break;
			}
		}
 		return;
	}

	// No more effect
 	if( pmove->onground == NULL )
	{
		// Flag that we jumped.
		// HACK HACK HACK
		// Remove this when the game.dll no longer does physics code!!!!
		pev->oldbuttons |= IN_JUMP;	// don't jump again until released
		return; // in air, so no effect
	}

	if( pev->oldbuttons & IN_JUMP )
		return; // don't pogo stick

	// In the air now.
	pmove->onground = (edict_t *)NULL;

	PM_PreventMegaBunnyJumping();

	if( tfc ) EmitSound( CHAN_BODY, "player/plyrjmp8.wav", 0.5, ATTN_NORM, PITCH_NORM );
	else PM_PlayStepSound( PM_MapTextureTypeStepType( pmove->chTextureType ), 1.0 );

	// See if user can super long jump?
	BOOL cansuperjump = atoi( Info_ValueForKey( "slj" )) == 1 ? true : false;

	// Acclerate upward
	// If we are ducking...
	if(( pev->bInDuck ) || ( pev->flags & FL_DUCKING ))
	{
		// Adjust for super long jump module
		// UNDONE -- note this should be based on forward angles, not current velocity.
		if( cansuperjump && ( pmove->cmd.buttons & IN_DUCK ) && ( pev->flDuckTime > 0.0f ) && pev->velocity.Length() > 50.0f )
		{
			pev->punchangle.x = -5.0f;
			pev->velocity.x = pmove->forward.x * PLAYER_LONGJUMP_SPEED * 1.6f;
			pev->velocity.y = pmove->forward.y * PLAYER_LONGJUMP_SPEED * 1.6f;
			pev->velocity.z = sqrt( 2 * 800 * 56.0f );
		}
		else pev->velocity.z = sqrt( 2 * 800 * 45.0f );
	}
	else pev->velocity.z = sqrt( 2 * 800 * 45.0f );

	// Decay it for simulation
	PM_FixupGravityVelocity();

	// Flag that we jumped.
	pev->oldbuttons |= IN_JUMP;	// don't jump again until released
}

/*
=============
PM_CheckWaterJump
=============
*/
void PM_CheckWaterJump( void )
{
	Vector		vecStart, vecEnd;
	Vector		flatforward;
	Vector		flatvelocity;
	float		curspeed;
	int		savehull;
	TraceResult	tr;

	// Already water jumping.
	if( pmove->flWaterJumpTime )
		return;

	// Don't hop out if we just jumped in
	if( pev->velocity.z < -180 )
		return; // only hop out if we are moving up

	// See if we are backing up
	flatvelocity.x = pev->velocity.x;
	flatvelocity.y = pev->velocity.y;
	flatvelocity.z = 0.0f;

	// Must be moving
	curspeed = flatvelocity.Length();
	flatvelocity = flatvelocity.Normalize();
	
	// see if near an edge
	flatforward.x = pmove->forward.x;
	flatforward.y = pmove->forward.y;
	flatforward.z = 0.0f;
	flatforward = flatforward.Normalize();

	// Are we backing into water from steps or something?  If so, don't pop forward
	if( curspeed != 0.0 && ( DotProduct( flatvelocity, flatforward ) < 0.0f ))
		return;

	vecStart = pmove->origin;
	vecStart.z += WJ_HEIGHT;

	vecEnd = vecStart + flatforward * 24;
	
	// Trace, this trace should use the point sized collision hull
	savehull = pmove->usehull;
	pmove->usehull = 2;
	tr = TRACE_PLAYER( vecStart, vecEnd, PM_NORMAL, (edict_t *)NULL );

	if( tr.flFraction < 1.0 && fabs( tr.vecPlaneNormal.z ) < 0.1f ) // Facing a near vertical wall?
	{
		vecStart.z += pmove->player_maxs[savehull].z - WJ_HEIGHT;
		vecEnd = vecStart + flatforward * 24;
		pev->movedir = tr.vecPlaneNormal * -50;

		tr = TRACE_PLAYER( vecStart, vecEnd, PM_NORMAL, (edict_t *)NULL );
		if( tr.flFraction == 1.0f )
		{
			pmove->flWaterJumpTime = 2000;
			pev->velocity.z = 225;
			pev->oldbuttons |= IN_JUMP;
			pev->flags |= FL_WATERJUMP;
		}
	}
	// Reset the collision hull
	pmove->usehull = savehull;
}

void PM_CheckFalling( void )
{
	if( pmove->onground != NULL && !pmove->dead && pev->flFallVelocity >= PLAYER_FALL_PUNCH_THRESHHOLD )
	{
		float	fvol = 0.5;

		if( pev->waterlevel > 0 )
		{
			// does nothing
		}
		else if( pev->flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED )
		{
			// NOTE:  In the original game dll, there were no breaks after these cases, causing the first one to 
			// cascade into the second
#if 0
			switch ( RandomLong(0,1) )
			{
			case 0:
				EmitSound( CHAN_VOICE, "player/pl_fallpain2.wav", 1, ATTN_NORM, PITCH_NORM );
				break;
			case 1:
				EmitSound( CHAN_VOICE, "player/pl_fallpain3.wav", 1, ATTN_NORM, PITCH_NORM );
				break;
			}
#endif
			fvol = 1.0f;
		}
		else if( pev->flFallVelocity > PLAYER_MAX_SAFE_FALL_SPEED / 2.0f )
		{
			if( atoi( Info_ValueForKey( "tfc" )))
			{
				EmitSound( CHAN_VOICE, "player/pl_fallpain3.wav", 1, ATTN_NORM, PITCH_NORM );
			}
			fvol = 0.85f;
		}
		else if( pev->flFallVelocity < PLAYER_MIN_BOUNCE_SPEED )
		{
			fvol = 0.0f;
		}

		if( fvol > 0.0f )
		{
			// Play landing step right away
			pev->flTimeStepSound = 0;
			
			PM_UpdateStepSound();
			
			// play step sound for current texture
			PM_PlayStepSound( PM_MapTextureTypeStepType( pmove->chTextureType ), fvol );

			// Knock the screen around a little bit, temporary effect
			pev->punchangle.z = pev->flFallVelocity * 0.013f; // punch z axis

			if( pev->punchangle.x > 8.0f ) pev->punchangle.x = 8.0f;
		}
	}

	if( pmove->onground != NULL ) pev->flFallVelocity = 0.0f;
}

/*
=================
PM_PlayWaterSounds

=================
*/
void PM_PlayWaterSounds( void )
{
	// Did we enter or leave water?
	if(( pmove->oldwaterlevel == 0 && pev->waterlevel != 0 ) || ( pmove->oldwaterlevel != 0 && pev->waterlevel == 0 ))
	{
		switch( RANDOM_LONG( 0, 3 ))
		{
		case 0:
			EmitSound( CHAN_BODY, "player/pl_wade1.wav", 1, ATTN_NORM, PITCH_NORM );
			break;
		case 1:
			EmitSound( CHAN_BODY, "player/pl_wade2.wav", 1, ATTN_NORM, PITCH_NORM );
			break;
		case 2:
			EmitSound( CHAN_BODY, "player/pl_wade3.wav", 1, ATTN_NORM, PITCH_NORM );
			break;
		case 3:
			EmitSound( CHAN_BODY, "player/pl_wade4.wav", 1, ATTN_NORM, PITCH_NORM );
			break;
		}
	}
}

/*
===============
PM_CalcRoll

===============
*/
float PM_CalcRoll( Vector angles, Vector velocity, float rollangle, float rollspeed )
{
	Vector	forward, right, up;
    
	AngleVectors( angles, forward, right, up );
    
	float	side = DotProduct (velocity, right);
	float	sign = fabs( side < 0 ? -1 : 1 );
	float	value = rollangle;
    
	if( side < rollspeed )
	{
		side = side * value / rollspeed;
	}
	else
	{
		side = value;
	}
	return side * sign;
}

/*
=============
PM_DropPunchAngle

=============
*/
void PM_DropPunchAngle( Vector &punchangle )
{
	float	len = punchangle.Length();
	
	punchangle = punchangle.Normalize();
	len -= (10.0f + len * 0.5f) * pmove->frametime;
	len = max( len, 0.0f );
	punchangle *= len;
}

/*
==============
PM_CheckParamters

==============
*/
void PM_CheckParamters( void )
{
	float	spd = 0;
	float	maxspeed;
	Vector	v_angle;

	spd += ( pmove->cmd.forwardmove * pmove->cmd.forwardmove );
	spd += ( pmove->cmd.sidemove * pmove->cmd.sidemove );
	spd += ( pmove->cmd.upmove * pmove->cmd.upmove );
	spd = sqrt( spd );

	maxspeed = pmove->clientmaxspeed; //atof( Info_ValueForKey( "maxspd" ));
	if( maxspeed != 0.0f ) pmove->maxspeed = min( maxspeed, pmove->maxspeed );

	if(( spd != 0.0f ) && ( spd > pmove->maxspeed ))
	{
		float	fRatio = pmove->maxspeed / spd;

		pmove->cmd.forwardmove *= fRatio;
		pmove->cmd.sidemove *= fRatio;
		pmove->cmd.upmove *= fRatio;
	}

	if ( pev->flags & FL_FROZEN || pev->flags & FL_ONTRAIN || pmove->dead )
	{
		pmove->cmd.forwardmove = 0;
		pmove->cmd.sidemove    = 0;
		pmove->cmd.upmove      = 0;

		if(!( pev->flags & FL_ONTRAIN ))
			pmove->cmd.buttons = 0;
	}

	PM_DropPunchAngle( pev->punchangle );

	// Take angles from command.
	if( !pmove->dead )
	{
		v_angle = pmove->cmd.angles + pev->punchangle;         

		// Set up view angles.
		pev->angles[ROLL] = PM_CalcRoll( v_angle, pev->velocity, pmove->movevars->rollangle, pmove->movevars->rollspeed ) * 4;
		pev->angles[PITCH] = v_angle[PITCH];
		pev->angles[YAW] = v_angle[YAW];
	}
//	else pev->angles = pmove->oldangles;	// ???

	// Set dead player view_offset
	if( pmove->dead ) pev->view_ofs.z = PM_DEAD_VIEWHEIGHT;

	// Adjust client view angles to match values used on server.
	if( pev->angles[YAW] > 180.0f ) pev->angles[YAW] -= 360.0f;
}

void PM_ReduceTimers( void )
{
	if( pev->flTimeStepSound > 0.0f )
	{
		pev->flTimeStepSound -= pmove->cmd.msec;
		if( pev->flTimeStepSound < 0.0f )
			pev->flTimeStepSound = 0.0f;
	}
	if( pev->flDuckTime > 0.0f )
	{
		pev->flDuckTime -= pmove->cmd.msec;
		if( pev->flDuckTime < 0.0f )
			pev->flDuckTime = 0.0f;
	}
	if( pev->flSwimTime > 0.0f )
	{
		pev->flSwimTime -= pmove->cmd.msec;
		if( pev->flSwimTime < 0.0f )
			pev->flSwimTime = 0.0f;
	}
}

/*
=============
PlayerMove

Returns with origin, angles, and velocity modified in place.

Numtouch and touchindex[] will be set if any of the physents
were contacted during the move.
=============
*/
void PM_PlayerMove( BOOL server )
{
	// Are we running server code?
	pmove->server = server;                

	// Adjust speeds etc.
	PM_CheckParamters();

	// Assume we don't touch anything
	pmove->numtouch = 0;                    
	pmove->dead = (pev->health <= 0.0f) ? true : false; 

	// # of msec to apply movement
	pmove->frametime = pmove->cmd.msec * 0.001f;    

	PM_ReduceTimers();

	// Convert view angles to vectors
	AngleVectors( pev->angles, pmove->forward, pmove->right, pmove->up );

	// PM_ShowClipBox();

	// Special handling for spectator and observers. (iuser1 is set if the player's in observer mode)
	if(( pev->flags & FL_SPECTATOR ) || pev->iSpecMode > 0 )
	{
		PM_SpectatorMove();
		PM_CatagorizePosition();
		return;
	}

	// Always try and unstick us unless we are in NOCLIP mode
	if( pev->movetype != MOVETYPE_NOCLIP && pev->movetype != MOVETYPE_NONE )
	{
		if( PM_CheckStuck( ))
		{
			return;  // Can't move, we're stuck
		}
	}

	// Now that we are "unstuck", see where we are ( waterlevel and type, pmove->onground ).
	PM_CatagorizePosition ();

	// Store off the starting water level
	pmove->oldwaterlevel = pev->waterlevel;

	// If we are not on ground, store off how fast we are moving down
	if( pmove->onground == NULL ) pev->flFallVelocity = -pev->velocity.z;

	edict_t	*pLadder = (edict_t *)NULL;

	g_onladder = false;

	// Don't run ladder code if dead or on a train
	if( !pmove->dead && !( pev->flags & FL_ONTRAIN ))
	{
		pLadder = PM_Ladder();
		if( pLadder )
		{
			g_onladder = true;
		}
	}

	PM_UpdateStepSound ();
	PM_Duck ();
	
	// Don't run ladder code if dead or on a train
	if( !pmove->dead && !( pev->flags & FL_ONTRAIN ))
	{
		if( pLadder )
		{
			PM_LadderMove( pLadder );
		}
		else if( pev->movetype != MOVETYPE_WALK && pev->movetype != MOVETYPE_NOCLIP )
		{
			// Clear ladder stuff unless player is noclipping
			// it will be set immediately again next frame if necessary
			pev->movetype = MOVETYPE_WALK;
		}
	}

	// Slow down, I'm pulling it! (a box maybe) but only when I'm standing on ground
	if(( pmove->onground != NULL ) && ( pmove->cmd.buttons & IN_USE ))
	{
		// FIXME: check for really pulling!!!
		pev->velocity *= 0.3f;
	}

	// Handle movement
	switch( pev->movetype )
	{
	default:
		ALERT( at_error, "Bogus pmove player movetype %i on %s\n", pev->movetype, pmove->server ? "server" : "client" );
		break;
	case MOVETYPE_NONE:
		break;
	case MOVETYPE_NOCLIP:
		PM_NoClip();
		break;
	case MOVETYPE_TOSS:
	case MOVETYPE_BOUNCE:
		PM_Physics_Toss();
		break;
	case MOVETYPE_FLY:
		PM_CheckWater();

		// Was jump button pressed?
		// If so, set velocity to 270 away from ladder.  This is currently wrong.
		// Also, set MOVE_TYPE to walk, too.
		if( pmove->cmd.buttons & IN_JUMP )
		{
			if( !pLadder ) PM_Jump();
		}
		else
		{
			pev->oldbuttons &= ~IN_JUMP;
		}
		
		// Perform the move accounting for any base velocity.
		pev->velocity += pev->basevelocity;
		PM_FlyMove ();
		pev->velocity -= pev->basevelocity;
		break;
	case MOVETYPE_WALK:
		if( !PM_InWater( )) PM_AddCorrectGravity();

		// If we are leaping out of the water, just update the counters.
		if( pmove->flWaterJumpTime )
		{
			PM_WaterJump();
			PM_FlyMove();

			// Make sure waterlevel is set correctly
			PM_CheckWater();
			return;
		}

		// If we are swimming in the water, see if we are nudging against a place we can jump up out
		//  of, and, if so, start out jump.  Otherwise, if we are not moving up, then reset jump timer to 0
		if( pev->waterlevel >= 2 ) 
		{
			if( pev->waterlevel == 2 )
			{
				PM_CheckWaterJump();
			}

			// If we are falling again, then we must not trying to jump out of water any more.
			if( pev->velocity.z < 0 && pmove->flWaterJumpTime )
			{
				pmove->flWaterJumpTime = 0;
			}

			// Was jump button pressed?
			if( pmove->cmd.buttons & IN_JUMP )
			{
				PM_Jump ();
			}
			else
			{
				pev->oldbuttons &= ~IN_JUMP;
			}

			// Perform regular water movement
			PM_WaterMove();
			
			pev->velocity -= pev->basevelocity;

			// Get a final position
			PM_CatagorizePosition();
		}
		else	// Not underwater
		{
			// Was jump button pressed?
			if( pmove->cmd.buttons & IN_JUMP )
			{
				if( !pLadder )
				{
					PM_Jump ();
				}
			}
			else
			{
				pev->oldbuttons &= ~IN_JUMP;
			}

			// Fricion is handled before we add in any base velocity. That way, if we are on a conveyor, 
			//  we don't slow when standing still, relative to the conveyor.
			if( pmove->onground != NULL )
			{
				pev->velocity.z = 0.0f;
				PM_Friction();
			}

			// Make sure velocity is valid.
			PM_CheckVelocity();

			// Are we on ground now
			if( pmove->onground != NULL )
			{
				PM_WalkMove();
			}
			else
			{
				PM_AirMove();  // Take into account movement when in air.
			}

			// Set final flags.
			PM_CatagorizePosition();

			// Now pull the base velocity back out.
			// Base velocity is set if you are on a moving object, like
			// a conveyor (or maybe another monster?)
			pev->velocity -= pev->basevelocity;
				
			// Make sure velocity is valid.
			PM_CheckVelocity();

			// Add any remaining gravitational component.
			if( !PM_InWater( ))
			{
				PM_FixupGravityVelocity();
			}

			// If we are on ground, no downward velocity.
			if( pmove->onground != NULL )
			{
				pev->velocity.z = 0.0f;
			}

			// See if we landed on the ground with enough force to play
			//  a landing sound.
			PM_CheckFalling();
		}

		// Did we enter or leave the water?
		PM_PlayWaterSounds();
		break;
	}
}

void PM_CreateStuckTable( void )
{
	float	x, y, z;
	int	idx = 0;
	int	i;
	Vector	zi;

	memset( rgv3tStuckTable, 0, 54 * sizeof( Vector ));

	// Little Moves.
	x = y = 0.0f;

	// Z moves
	for( z = -0.125f; z <= 0.125f; z += 0.125f )
	{
		rgv3tStuckTable[idx].x = x;
		rgv3tStuckTable[idx].y = y;
		rgv3tStuckTable[idx].z = z;
		idx++;
	}

	x = z = 0.0f;
	// Y moves
	for( y = -0.125f; y <= 0.125f; y += 0.125f )
	{
		rgv3tStuckTable[idx].x = x;
		rgv3tStuckTable[idx].y = y;
		rgv3tStuckTable[idx].z = z;
		idx++;
	}

	y = z = 0.0f;
	// X moves
	for( x = -0.125f; x <= 0.125f; x += 0.125f )
	{
		rgv3tStuckTable[idx].x = x;
		rgv3tStuckTable[idx].y = y;
		rgv3tStuckTable[idx].z = z;
		idx++;
	}

	// Remaining multi axis nudges.
	for( x = -0.125f; x <= 0.125f; x += 0.250f )
	{
		for( y = - 0.125f; y <= 0.125f; y += 0.250f )
		{
			for( z = -0.125f; z <= 0.125f; z += 0.250f )
			{
				rgv3tStuckTable[idx].x = x;
				rgv3tStuckTable[idx].y = y;
				rgv3tStuckTable[idx].z = z;
				idx++;
			}
		}
	}

	// Big Moves.
	x = y = 0.0f;
	zi[0] = 0.0f;
	zi[1] = 1.0f;
	zi[2] = 6.0f;

	for( i = 0; i < 3; i++ )
	{
		// Z moves
		z = zi[i];
		rgv3tStuckTable[idx].x = x;
		rgv3tStuckTable[idx].y = y;
		rgv3tStuckTable[idx].z = z;
		idx++;
	}

	x = z = 0.0f;
	// Y moves
	for( y = -2.0f; y <= 2.0f; y += 2.0f )
	{
		rgv3tStuckTable[idx].x = x;
		rgv3tStuckTable[idx].y = y;
		rgv3tStuckTable[idx].z = z;
		idx++;
	}

	y = z = 0.0f;
	// X moves
	for( x = -2.0f; x <= 2.0f; x += 2.0f )
	{
		rgv3tStuckTable[idx].x = x;
		rgv3tStuckTable[idx].y = y;
		rgv3tStuckTable[idx].z = z;
		idx++;
	}

	// Remaining multi axis nudges.
	for( i = 0; i < 3; i++ )
	{
		z = zi[i];
		
		for( x = -2.0f; x <= 2.0f; x += 2.0f )
		{
			for( y = -2.0f; y <= 2.0f; y += 2.0f )
			{
				rgv3tStuckTable[idx][0] = x;
				rgv3tStuckTable[idx][1] = y;
				rgv3tStuckTable[idx][2] = z;
				idx++;
			}
		}
	}
}



/*
=============================================================
This module implements the shared player physics code between any particular game and 
the engine.  The same PM_Move routine is built into the game .dll and the client .dll and is
invoked by each side as appropriate.  There should be no distinction, internally, between server
and client.  This will ensure that prediction behaves appropriately.
=============================================================
*/
void PM_Move( playermove_t *ppmove, int server )
{
	assert( pm_shared_initialized );

	pmove = ppmove;
	
	PM_PlayerMove(( server != 0 ) ? true : false );

	if( pmove->onground != NULL )
	{
		pev->flags |= FL_ONGROUND;
	}
	else
	{
		pev->flags &= ~FL_ONGROUND;
	}

	// In single player, reset friction after each movement to FrictionModifier Triggers work still.
	if( !pmove->multiplayer && ( pev->movetype == MOVETYPE_WALK  ))
	{
		pev->friction = 1.0f;
	}
}

void PM_Init( playermove_t *ppmove )
{
	assert( !pm_shared_initialized );

	pmove = ppmove;

	PM_CreateStuckTable();
	PM_InitTextureTypes();

	pm_shared_initialized = 1;
}