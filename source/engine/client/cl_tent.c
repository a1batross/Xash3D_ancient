//=======================================================================
//			Copyright XashXT Group 2009 ©
//		   cl_tent.c - temp entity effects management
//=======================================================================

#include "common.h"
#include "client.h"
#include "effects_api.h"
#include "te_message.h"	// grab TE_EXPLFLAGS
#include "const.h"

/*
==============================================================

TEMPENTS MANAGEMENT

==============================================================
*/

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