//=======================================================================
//			Copyright XashXT Group 2009 ©
//		   cl_tent.c - temp entity effects management
//=======================================================================

#include "common.h"
#include "client.h"
#include "effects_api.h"
#include "te_message.h"	// grab TE_EXPLFLAGS
#include "beam_def.h"
#include "const.h"

/*
==============================================================

TEMPENTS MANAGEMENT

==============================================================
*/

#define SHARD_VOLUME	12.0	// on shard ever n^3 units
#define MAX_TEMPENTS	1024
TEMPENTITY		*cl_active_tents, *cl_free_tents;
static TEMPENTITY		cl_tents_list[MAX_TEMPENTS];

/*
=================
CL_FreeTempEntity
=================
*/
static void CL_FreeTempEnt( TEMPENTITY *pTemp, TEMPENTITY *pPrev )
{
	// remove from the active list.
	if( pPrev )	
	{
		pPrev->next = pTemp->next;
	}
	else
	{
		cl_active_tents = pTemp->next;
	}

	// cleanup its data.
	if( pTemp->pvEngineData )
		Mem_Free( pTemp->pvEngineData );
	Mem_Set( pTemp, 0, sizeof( TEMPENTITY ));

	// add to the free list.
	pTemp->next = cl_free_tents;
	cl_free_tents = pTemp;
}

// free the first low priority tempent it finds.
static bool CL_FreeLowPriorityTempEnt( void )
{
	TEMPENTITY	*pActive = cl_active_tents;
	TEMPENTITY	*pPrev = NULL;

	while( pActive )
	{
		if( pActive->priority == TENTPRIORITY_LOW )
		{
			CL_FreeTempEnt( pActive, pPrev );
			return true;
		}
		pPrev = pActive;
		pActive = pActive->next;
	}
	return false;
}

static void CL_PrepareTempEnt( TEMPENTITY *pTemp, int modelIndex )
{
	int	frameCount;

	// Use these to set per-frame and termination conditions / actions
	pTemp->flags = FTENT_NONE;		
	pTemp->die = clgame.globals->time + 0.75f;

	if( modelIndex != 0 ) // allocate prevframe only for tents with valid model
		pTemp->pvEngineData = Mem_Alloc( cls.mempool, sizeof( studioframe_t ));
	else pTemp->flags |= FTENT_NOMODEL;

	Mod_GetFrames( modelIndex, &frameCount );

	pTemp->modelindex = modelIndex;
	pTemp->renderMode = kRenderNormal;
	pTemp->renderFX = kRenderFxNone;
	VectorSet( pTemp->renderColor, 255, 255, 255 );
	pTemp->m_flFrameMax = max( 0, frameCount - 1 );
	pTemp->renderAmt = 255;
	pTemp->body = 0;
	pTemp->skin = 0;
	pTemp->fadeSpeed = 0.5f;
	pTemp->hitSound = 0;
	pTemp->clientIndex = NULLENT_INDEX;
	pTemp->bounceFactor = 1;
	pTemp->m_flSpriteScale = 1.0f;
}

/*
=================
CL_AllocTempEntity
=================
*/
TEMPENTITY *CL_TempEntAlloc( float *org, int modelIndex )
{
	TEMPENTITY	*pTemp;

	if( !cl_free_tents )
	{
		MsgDev( D_WARN, "Overflow %d temporary ents!\n", MAX_TEMPENTS );
		return NULL;
	}

	pTemp = cl_free_tents;
	cl_free_tents = pTemp->next;

	Com_Assert( pTemp->pvEngineData );
	CL_PrepareTempEnt( pTemp, modelIndex );

	pTemp->priority = TENTPRIORITY_LOW;
	if( org ) VectorCopy( org, pTemp->origin );

	pTemp->next = cl_active_tents;
	cl_active_tents = pTemp;

	return pTemp;
}

TEMPENTITY *CL_TempEntAllocNoModel( float *org )
{
	return CL_TempEntAlloc( org, 0 );
}

TEMPENTITY *CL_TempEntAllocHigh( float *org, int modelIndex )
{
	TEMPENTITY	*pTemp;

	if( !cl_free_tents )
	{
		// no temporary ents free, so find the first active low-priority temp ent 
		// and overwrite it.
		CL_FreeLowPriorityTempEnt();
	}

	if( !cl_free_tents )
	{
		// didn't find anything? The tent list is either full of high-priority tents
		// or all tents in the list are still due to live for > 10 seconds. 
		MsgDev( D_ERROR, "Couldn't alloc a high priority TENT!\n" );
		return NULL;
	}

	// Move out of the free list and into the active list.
	pTemp = cl_free_tents;
	cl_free_tents = pTemp->next;

	Com_Assert( pTemp->pvEngineData );
	CL_PrepareTempEnt( pTemp, modelIndex );

	pTemp->priority = TENTPRIORITY_HIGH;
	if( org ) VectorCopy( org, pTemp->origin );

	pTemp->next = cl_active_tents;
	cl_active_tents = pTemp;

	return pTemp;
}

TEMPENTITY *CL_TentEntAllocCustom( float *org, int modelIndex, int high, ENTCALLBACK pfnCallback )
{
	TEMPENTITY	*pTemp;

	if( high ) pTemp = CL_TempEntAllocHigh( org, modelIndex );
	else CL_TempEntAlloc( org, modelIndex );

	if( pTemp && pfnCallback )
	{
		pTemp->callback = pfnCallback;
		pTemp->flags |= FTENT_CLIENTCUSTOM;
	}
	return pTemp;
}

/*
===============
CL_ClearTempEnts
===============
*/
void CL_ClearTempEnts( void )
{
	int	i;

	cl_active_tents = NULL;
	cl_free_tents = cl_tents_list;

	for( i = 0; i < MAX_TEMPENTS; i++ )
	{
		cl_tents_list[i].next = &cl_tents_list[i+1];
		if( cl_tents_list[i].pvEngineData )
			Mem_Free( cl_tents_list[i].pvEngineData );
		cl_tents_list[i].pvEngineData = NULL;
	}
	cl_tents_list[MAX_TEMPENTS-1].next = NULL;
}

/*
===============
CL_IsActiveTempEnt
===============
*/
bool CL_IsActiveTempEnt( TEMPENTITY *pTemp )
{
	bool	active = true;
	float	life;

	if( !pTemp ) return false;
	life = pTemp->die - clgame.globals->time;
	
	if( life < 0.0f )
	{
		if( pTemp->flags & FTENT_FADEOUT )
		{
			int	alpha;

			if( pTemp->renderMode == kRenderNormal )
				pTemp->renderMode = kRenderTransTexture;
			
			alpha = pTemp->startAlpha * ( 1.0f + life * pTemp->fadeSpeed );
			
			if( alpha <= 0 )
			{
				active = false;
				alpha = 0;
			}
			pTemp->renderAmt = alpha;
		}
		else 
		{
			active = false;
		}
	}

	// never die tempents only die when their die is cleared
	if( pTemp->flags & FTENT_NEVERDIE )
	{
		active = (pTemp->die != 0.0f);
	}
	return active;
}

bool CL_UpdateTempEnt( TEMPENTITY *pTemp, int framenumber )
{
	if( !pTemp ) return false;
	return clgame.dllFuncs.pfnUpdateEntity( pTemp, framenumber );
}

/*
===============
CL_AddTempEnts
===============
*/
void CL_AddTempEnts( void )
{
	static int	gTempEntFrame = 0;
	TEMPENTITY	*current, *pnext, *pprev;

	// nothing to simulate ?
	if( !cl_active_tents )		
		return;

	// !!!BUGBUG -- This needs to be time based
	// g-cont. it's used only for flickering dlights, what difference ?
	gTempEntFrame = (gTempEntFrame + 1) & 31;

	current = cl_active_tents;

	// !!! Don't simulate while paused....  This is sort of a hack, revisit.
	if( cls.key_dest != key_game )
	{
		while( current )
		{
			re->AddTmpEntity( current, ED_TEMPENTITY );
			current = current->next;
		}
	}
	else
	{
		pprev = NULL;

		while( current )
		{
			bool	fTempEntFreed = false;

			pnext = current->next;

			// Kill it
			if( !CL_IsActiveTempEnt( current ) || !CL_UpdateTempEnt( current, gTempEntFrame ))
			{
				CL_FreeTempEnt( current, pprev );
				fTempEntFreed = true;
			}
			else
			{
				// renderer rejected entity for some reasons...
				if( !re->AddTmpEntity( current, ED_TEMPENTITY ))
				{
					if(!( current->flags & FTENT_PERSIST )) 
					{
						// If we can't draw it this frame, just dump it.
						current->die = clgame.globals->time;
						// don't fade out, just die
						current->flags &= ~FTENT_FADEOUT;

						CL_FreeTempEnt( current, pprev );
						fTempEntFreed = true;
					}
				}
			}

			if( !fTempEntFreed )
				pprev = current;
			current = pnext;
		}
	}
}

void CL_BloodSprite( float *org, int colorIndex, int modelIndex, float size )
{
	TEMPENTITY	*pTemp;

	if( CM_GetModelType( modelIndex ) == mod_bad )
		return;

	// Large, single blood sprite is a high-priority tent
	if( pTemp = CL_TempEntAllocHigh( org, modelIndex ))
	{
		int	frameCount;

		Mod_GetFrames( modelIndex, &frameCount );
		pTemp->renderMode = kRenderTransTexture;
		pTemp->renderFX = kRenderFxClampMinScale;
		pTemp->m_flSpriteScale = Com_RandomFloat(( size / 25.0f), ( size / 35.0f ));
		pTemp->flags = FTENT_SPRANIMATE;
 
		CL_GetPaletteColor( colorIndex, pTemp->renderColor );
		pTemp->m_flFrameRate = frameCount * 4; // Finish in 0.250 seconds
		// play the whole thing once
		pTemp->die = clgame.globals->time + (frameCount / pTemp->m_flFrameRate);

		pTemp->angles[2] = Com_RandomLong( 0, 360 );
		pTemp->bounceFactor = 0;
	}
}

void CL_BreakModel( float *pos, float *size, float *dir, float random, float life, int count, int modelIndex, char flags )
{
	int		i, frameCount;
	TEMPENTITY	*pTemp;
	char		type;

	if( !modelIndex ) return;

	type = flags & BREAK_TYPEMASK;

	if( CM_GetModelType( modelIndex ) == mod_bad )
		return;

	Mod_GetFrames( modelIndex, &frameCount );
		
	if( count == 0 )
	{
		// assume surface (not volume)
		count = (size[0] * size[1] + size[1] * size[2] + size[2] * size[0]) / (3 * SHARD_VOLUME * SHARD_VOLUME);
	}

	// limit to 100 pieces
	if( count > 100 ) count = 100;

	for( i = 0; i < count; i++ ) 
	{
		vec3_t	vecSpot;

		// fill up the box with stuff
		vecSpot[0] = pos[0] + Com_RandomFloat( -0.5f, 0.5f ) * size[0];
		vecSpot[1] = pos[1] + Com_RandomFloat( -0.5f, 0.5f ) * size[1];
		vecSpot[2] = pos[2] + Com_RandomFloat( -0.5f, 0.5f ) * size[2];

		pTemp = CL_TempEntAlloc( vecSpot, modelIndex );
		if( !pTemp ) return;

		// keep track of break_type, so we know how to play sound on collision
		pTemp->hitSound = type;
		
		if( CM_GetModelType( modelIndex ) == mod_sprite )
			pTemp->m_flFrame = Com_RandomLong( 0, frameCount - 1 );
		else if( CM_GetModelType( modelIndex ) == mod_studio )
			pTemp->body = Com_RandomLong( 0, frameCount - 1 );

		pTemp->flags |= FTENT_COLLIDEWORLD | FTENT_FADEOUT | FTENT_SLOWGRAVITY;

		if( Com_RandomLong( 0, 255 ) < 200 ) 
		{
			pTemp->flags |= FTENT_ROTATE;
			pTemp->m_vecAvelocity[0] = Com_RandomFloat( -256, 255 );
			pTemp->m_vecAvelocity[1] = Com_RandomFloat( -256, 255 );
			pTemp->m_vecAvelocity[2] = Com_RandomFloat( -256, 255 );
		}

		if(( Com_RandomLong( 0, 255 ) < 100 ) && ( flags & BREAK_SMOKE ))
			pTemp->flags |= FTENT_SMOKETRAIL;

		if(( type == BREAK_GLASS ) || ( flags & BREAK_TRANS ))
		{
			pTemp->renderMode = kRenderTransTexture;
			pTemp->renderAmt = pTemp->startAlpha = 128;
		}
		else
		{
			pTemp->renderMode = kRenderNormal;
			pTemp->renderAmt = 255; // set this for fadeout
		}

		pTemp->m_vecVelocity[0] = dir[0] + Com_RandomFloat( -random, random );
		pTemp->m_vecVelocity[1] = dir[1] + Com_RandomFloat( -random, random );
		pTemp->m_vecVelocity[2] = dir[2] + Com_RandomFloat( 0, random );

		pTemp->die = clgame.globals->time + life + Com_RandomFloat( 0, 1 ); // Add an extra 0-1 secs of life
	}
}

/*
===============
CL_TempModel
===============
*/
TEMPENTITY *CL_TempModel( float *pos, float *dir, float *ang, float life, int modelIndex, int type )
{
	// alloc a new tempent
	TEMPENTITY *pTemp = CL_TempEntAlloc( pos, modelIndex );
	if( !pTemp ) return NULL;

	// keep track of shell type
	switch( type )
	{
	case 1: pTemp->hitSound = TE_BOUNCE_SHELL; break;
	case 2: pTemp->hitSound = TE_BOUNCE_SHOTSHELL; break;
	}

	if( pos ) VectorCopy( pos, pTemp->origin );
	if( dir ) VectorCopy( dir, pTemp->m_vecVelocity );
	if( ang ) VectorCopy( ang, pTemp->angles );
	pTemp->body = 0;
	pTemp->flags = (FTENT_COLLIDEWORLD|FTENT_FADEOUT|FTENT_GRAVITY|FTENT_ROTATE);
	pTemp->m_vecAvelocity[0] = Com_RandomFloat( -255, 255 );
	pTemp->m_vecAvelocity[1] = Com_RandomFloat( -255, 255 );
	pTemp->m_vecAvelocity[2] = Com_RandomFloat( -255, 255 );
	pTemp->renderMode = kRenderNormal;
	pTemp->startAlpha = 255;
	pTemp->die = clgame.globals->time + life;

	return pTemp;
}

/*
===============
CL_DefaultSprite
===============
*/
TEMPENTITY *CL_DefaultSprite( float *pos, int spriteIndex, float framerate )
{
	TEMPENTITY	*pTemp;
	int		frameCount;

	if( !spriteIndex || CM_GetModelType( spriteIndex ) != mod_sprite )
	{
		MsgDev( D_ERROR, "No Sprite %d!\n", spriteIndex );
		return NULL;
	}

	Mod_GetFrames( spriteIndex, &frameCount );

	pTemp = CL_TempEntAlloc( pos, spriteIndex );
	if( !pTemp ) return NULL;

	pTemp->m_flFrameMax = frameCount - 1;
	pTemp->m_flSpriteScale = 1.0f;
	pTemp->flags |= FTENT_SPRANIMATE;
	if( framerate == 0 ) framerate = 10;

	pTemp->m_flFrameRate = framerate;
	pTemp->die = clgame.globals->time + (float)frameCount / framerate;
	pTemp->m_flFrame = 0;

	return pTemp;
}

/*
===============
CL_TempSprite
===============
*/
TEMPENTITY *CL_TempSprite( float *pos, float *dir, float scale, int modelIndex, int rendermode, int renderfx, float a, float life, int flags )
{
	TEMPENTITY	*pTemp;
	int		frameCount;

	if( !modelIndex ) 
		return NULL;

	if( CM_GetModelType( modelIndex ) == mod_bad )
	{
		MsgDev( D_ERROR, "No model %d!\n", modelIndex );
		return NULL;
	}

	Mod_GetFrames( modelIndex, &frameCount );

	pTemp = CL_TempEntAlloc( pos, modelIndex );
	if( !pTemp ) return NULL;

	pTemp->m_flFrameMax = frameCount - 1;
	pTemp->m_flFrameRate = 10;
	pTemp->renderMode = rendermode;
	pTemp->renderFX = renderfx;
	pTemp->m_flSpriteScale = scale;
	pTemp->startAlpha = a * 255;
	pTemp->renderAmt = a * 255;
	pTemp->flags |= flags;

	if( pos ) VectorCopy( pos, pTemp->origin );
	if( dir ) VectorCopy( dir, pTemp->m_vecVelocity );
	if( life ) pTemp->die = clgame.globals->time + life;
	else pTemp->die = clgame.globals->time + (frameCount * 0.1f) + 1.0f;
	pTemp->m_flFrame = 0;

	return pTemp;
}

/*
===============
CL_MuzzleFlash

NOTE: this temp entity live once one frame
so we wan't add it to tent list
===============
*/
void CL_MuzzleFlash( int modelIndex, int entityIndex, int iAttachment, int type )
{
	vec3_t		pos;
	TEMPENTITY	*pTemp;
	int		frameCount;
	float		scale;
	int		index;

	if( CM_GetModelType( modelIndex ) == mod_bad )
	{
		MsgDev( D_ERROR, "Invalid muzzleflash %d!\n", modelIndex );
		return;
	}

	Mod_GetFrames( modelIndex, &frameCount );
	if( !CL_GetAttachment( entityIndex, iAttachment, pos, NULL ))
	{
		MsgDev( D_ERROR, "Invalid muzzleflash entity %d!\n", entityIndex );
		return;
	}

	// must set position for right culling on render
	pTemp = CL_TempEntAlloc( pos, modelIndex );
	if( !pTemp ) return;

	index = type % 10;
	scale = (type / 10) * 0.1f;
	if( scale == 0.0f ) scale = 0.5f;
	
	pTemp->renderMode = kRenderTransAdd;
	pTemp->renderAmt = 255;
	pTemp->renderFX = 0;
	pTemp->die = clgame.globals->time + 0.01; // die at next frame
	pTemp->m_flFrame = Com_RandomLong( 0, frameCount - 1 );
	pTemp->m_flFrameMax = frameCount - 1;
	pTemp->clientIndex = entityIndex;
	pTemp->m_iAttachment = iAttachment;

	if( index == 0 )
	{
		// Rifle flash
		pTemp->m_flSpriteScale = scale * Com_RandomFloat( 0.5f, 0.6f );
		pTemp->angles[2] = 90 * Com_RandomLong( 0, 3 );
	}
	else
	{
		pTemp->m_flSpriteScale = scale;
		pTemp->angles[2] = Com_RandomLong( 0, 359 );
	}

	// render now (guranteed that muzzleflash will be draw)
	re->AddTmpEntity( pTemp, ED_TEMPENTITY );
}

/*
===============
CL_SpriteExplode

just a preset for exploding sprite
===============
*/
void CL_SpriteExplode( TEMPENTITY *pTemp, float scale, int flags )
{
	if( !pTemp ) return;

	// NOTE: Xash3D doesn't needs this stuff, because sprites
	// have right rendermodes already at loading point
	// but i'm leave it for backward compatibility
	if( flags & TE_EXPLFLAG_NOADDITIVE )
	{
		// solid sprite
		pTemp->renderMode = kRenderNormal;
		pTemp->renderAmt = 255; 
	}
	else if( flags & TE_EXPLFLAG_DRAWALPHA )
	{
		// alpha sprite
		pTemp->renderMode = kRenderTransAlpha;
		pTemp->renderAmt = 180;
	}
	else
	{
		// additive sprite
		pTemp->renderMode = kRenderTransAdd;
		pTemp->renderAmt = 120;
	}

	if( flags & TE_EXPLFLAG_ROTATE )
	{
		pTemp->angles[2] = Com_RandomLong( 0, 360 );
	}

	pTemp->renderFX = kRenderFxNone;
	pTemp->m_vecVelocity[2] = 8;
	pTemp->origin[2] += 10;
	pTemp->m_flSpriteScale = scale;
}

/*
===============
CL_SpriteExplode

just a preset for smoke sprite
===============
*/
void CL_SpriteSmoke( TEMPENTITY *pTemp, float scale )
{
	int	iColor;

	if( !pTemp ) return;

	iColor = Com_RandomLong( 20, 35 );
	pTemp->renderMode = kRenderTransAlpha;
	pTemp->renderFX = kRenderFxNone;
	pTemp->m_vecVelocity[2] = 30;
	VectorSet( pTemp->renderColor, iColor, iColor, iColor );
	pTemp->origin[2] += 20;
	pTemp->m_flSpriteScale = scale;
	pTemp->flags = FTENT_WINDBLOWN;

}

/*
===============
CL_SpriteSpray

just a preset for smoke sprite
===============
*/
void CL_SpriteSpray( float *pos, float *dir, int modelIndex, int count, int speed, int iRand )
{
	TEMPENTITY	*pTemp;
	float		noise;
	float		znoise;
	int		i, frameCount;

	noise = (float)iRand / 100;

	// more vertical displacement
	znoise = noise * 1.5f;
	
	if( znoise > 1 ) znoise = 1;

	if( CM_GetModelType( modelIndex ) == mod_bad )
	{
		MsgDev( D_ERROR, "No model %d!\n", modelIndex );
		return;
	}

	Mod_GetFrames( modelIndex, &frameCount );

	for( i = 0; i < count; i++ )
	{
		vec3_t	velocity;
		float	scale;

		pTemp = CL_TempEntAlloc( pos, modelIndex );
		if( !pTemp ) return;

		pTemp->renderMode = kRenderTransAlpha;
		pTemp->renderFX = kRenderFxNoDissipation;
		pTemp->m_flSpriteScale = 0.5f;
		pTemp->flags |= FTENT_FADEOUT|FTENT_SLOWGRAVITY;
		pTemp->fadeSpeed = 2.0f;

		// make the spittle fly the direction indicated, but mix in some noise.
		velocity[0] = dir[0] + Com_RandomFloat( -noise, noise );
		velocity[1] = dir[1] + Com_RandomFloat( -noise, noise );
		velocity[2] = dir[2] + Com_RandomFloat( 0, znoise );
		scale = Com_RandomFloat(( speed * 0.8f ), ( speed * 1.2f ));
		VectorScale( velocity, scale, pTemp->m_vecVelocity );
		pTemp->die = clgame.globals->time + 0.35f;
		pTemp->m_flFrame = Com_RandomLong( 0, frameCount - 1 );
	}
}

/*
==============================================================

BEAMS MANAGEMENT

==============================================================
*/

#define MAX_BEAMS		128
#define MAX_BEAMTRAILS	(MAX_BEAMS * 16)

struct beamtrail_s
{
	BEAMTRAIL		*next;
	float		die;
	vec3_t		org;
	vec3_t		vel;

	poly_t		poly;	// pointer to render
	vec3_t		verts[4];	// quad description
	vec2_t		coord[4];
	rgba_t		color[4];
};

BEAM			*cl_active_beams, *cl_free_beams;
static BEAM		cl_beams_list[MAX_BEAMS];
BEAMTRAIL			*cl_active_trails, *cl_free_trails;
static BEAMTRAIL		cl_beam_trails[MAX_BEAMTRAILS];
	
// freq2 += step * 0.1;
// Fractal noise generator, power of 2 wavelength
static void CL_BeamNoise( float *noise, int divs, float scale )
{
	int	div2;
	
	div2 = divs>>1;
	if( divs < 2 ) return;

	// noise is normalized to +/- scale
	noise[div2] = ( noise[0] + noise[divs] ) * 0.5f + scale * Com_RandomFloat( -1.0f, 1.0f );

	if( div2 > 1.0f )
	{
		CL_BeamNoise( &noise[div2], div2, scale * 0.5f );
		CL_BeamNoise( noise, div2, scale * 0.5f );
	}
}

static void CL_SineNoise( float *noise, int divs )
{
	int	i;
	float	freq = 0;
	float	step = M_PI / (float)divs;

	for( i = 0; i < divs; i++ )
	{
		noise[i] = com.sin( freq );
		freq += step;
	}
}

bool CL_ComputeBeamEntPosition( edict_t *pEnt, int nAttachment, vec3_t pt )
{
	if( !pEnt )
	{
		if( pt ) VectorClear( pt );
		return false;
	}

	if( !CL_GetAttachment( pEnt->serialnumber, nAttachment, pt, NULL ))
		VectorCopy( pEnt->v.origin, pt );
	return true;
}

void CL_GetBeamOrigin( BEAM *pBeam, vec3_t org )
{
	if( !org ) return;
	if( pBeam->type == TE_BEAMRING || pBeam->type == TE_BEAMRINGPOINT )
	{
		// return the center of the ring
		VectorMA( pBeam->attachment[0], 0.5f, pBeam->delta, org );
		return;
	}
	VectorCopy( pBeam->attachment[0], org );
}

void CL_BeamComputeBounds( BEAM *pBeam )
{
	vec3_t	attachmentDelta, followDelta, org;
	BEAMTRAIL	*pFollow;
	int	i, radius;

	switch( pBeam->type )
	{
	case TE_BEAMSPLINE:
		// here, we gotta look at all the attachments....
		VectorClear( pBeam->mins );
		VectorClear( pBeam->maxs );

		for( i = 1; i < pBeam->numAttachments; i++)
		{
			VectorSubtract( pBeam->attachment[i], pBeam->attachment[0], attachmentDelta );
			AddPointToBounds( attachmentDelta, pBeam->mins, pBeam->maxs );
		}
		break;
	case TE_BEAMDISK:
	case TE_BEAMCYLINDER:
		// FIXME: This isn't quite right for the cylinder
		// Here, delta[2] is the radius
		radius = pBeam->delta[2];
		VectorSet( pBeam->mins, -radius, -radius, -radius );
		VectorSet( pBeam->maxs, radius, radius, radius );
		break;
	case TE_BEAMRING:
	case TE_BEAMRINGPOINT:
		radius = VectorLength( pBeam->delta ) * 0.5f;
		VectorSet( pBeam->mins, -radius, -radius, -radius );
		VectorSet( pBeam->maxs, radius, radius, radius );
		break;
	case TE_BEAMPOINTS:
	default:	// just use the delta
		for( i = 0; i < 3; i++ )
		{
			if( pBeam->delta[i] > 0.0f )
			{
				pBeam->mins[i] = 0.0f;
				pBeam->maxs[i] = pBeam->delta[i];
			}
			else
			{
				pBeam->mins[i] = pBeam->delta[i];
				pBeam->maxs[i] = 0.0f;
			}
		}
		break;
	}

	// deal with beam follow
	CL_GetBeamOrigin( pBeam, org );
	pFollow = pBeam->trail;

	while( pFollow )
	{
		VectorSubtract( pFollow->org, org, followDelta );
		AddPointToBounds( followDelta, pBeam->mins, pBeam->maxs );
		pFollow = pFollow->next;
	}
}

void CL_ClearBeams( void )
{
	int	i;

	cl_free_beams = &cl_beams_list[0];
	cl_active_beams = NULL;

	for( i = 0; i < MAX_BEAMS; i++ )
	{
		Mem_Set( &cl_beams_list[i], 0, sizeof( BEAM ));
		cl_beams_list[i].next = &cl_beams_list[i+1];
	}

	cl_beams_list[MAX_BEAMS-1].next = NULL;

	// also clear any particles used by beams
	cl_free_trails = &cl_beam_trails[0];
	cl_active_trails = NULL;

	for( i = 0; i < MAX_BEAMTRAILS; i++ )
		cl_beam_trails[i].next = &cl_beam_trails[i+1];
	cl_beam_trails[MAX_BEAMTRAILS-1].next = NULL;
}

static void CL_FreeDeadTrails( BEAMTRAIL **trail )
{
	BEAMTRAIL	*p, *kill;

	// kill all the ones hanging direcly off the base pointer
	while( 1 ) 
	{
		kill = *trail;
		if( kill && kill->die < clgame.globals->time )
		{
			*trail = kill->next;
			kill->next = cl_free_trails;
			cl_free_trails = kill;
			continue;
		}
		break;
	}

	// kill off all the others
	for( p = *trail; p; p = p->next )
	{
		while( 1 )
		{
			kill = p->next;
			if( kill && kill->die < clgame.globals->time )
			{
				p->next = kill->next;
				kill->next = cl_free_trails;
				cl_free_trails = kill;
				continue;
			}
			break;
		}
	}
}

BEAM *CL_AllocBeam( void )
{
	BEAM	*pBeam;
	
	if( !cl_free_beams )
		return NULL;

	pBeam   = cl_free_beams;
	cl_free_beams = pBeam->next;
	pBeam->next = cl_active_beams;
	cl_active_beams = pBeam;

	return pBeam;
}

void CL_FreeBeam( BEAM *pBeam )
{
	// free particles that have died off.
	CL_FreeDeadTrails( &pBeam->trail );

	// Clear us out
	Mem_Set( &pBeam, 0, sizeof( BEAM ));

	// now link into free list;
	pBeam->next = cl_free_beams;
	cl_free_beams = pBeam;
}

void CL_KillDeadBeams( edict_t *pDeadEntity )
{
	BEAM	*pbeam, *pnewlist, *pnext;
	BEAMTRAIL	*pHead; // build a new list to replace cl_active_beams.

	pbeam = cl_active_beams;	// old list.
	pnewlist = NULL;		// new list.
	
	while( pbeam )
	{
		pnext = pbeam->next;
		if( pbeam->entity[0] != pDeadEntity ) // link into new list.
		{
			pbeam->next = pnewlist;
			pnewlist = pbeam;

			pbeam = pnext;
			continue;
		}

		pbeam->flags &= ~(FBEAM_STARTENTITY|FBEAM_ENDENTITY);

		if( pbeam->type != TE_BEAMFOLLOW )
		{
			// Die Die Die!
			pbeam->die = clgame.globals->time - 0.1f;  

			// Kill off particles
			pHead = pbeam->trail;
			while( pHead )
			{
				pHead->die = clgame.globals->time - 0.1f;
				pHead = pHead->next;
			}

			// Free the beam
			CL_FreeBeam( pbeam );
		}
		else
		{
			// stay active
			pbeam->next = pnewlist;
			pnewlist = pbeam;
		}
		pbeam = pnext;
	}

	// we now have a new list with the bogus stuff released.
	cl_active_beams = pnewlist;
}