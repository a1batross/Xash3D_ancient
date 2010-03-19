//=======================================================================
//			Copyright XashXT Group 2010 ©
//		     r_tempents.cpp - tempentity management
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "studio_event.h"
#include "triangle_api.h"
#include "effects_api.h"
#include "pm_movevars.h"
#include "r_particle.h"
#include "r_tempents.h"
#include "ev_hldm.h"
#include "r_beams.h"
#include "hud.h"

extern TEMPENTITY *m_pEndFlare;
extern TEMPENTITY *m_pLaserSpot;

CTempEnts *g_pTempEnts;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTempEnts::CTempEnts( void )
{
	memset( m_TempEnts, 0, sizeof( TEMPENTITY ) * MAX_TEMP_ENTITIES );

	m_pFreeTempEnts = m_TempEnts;
	m_pActiveTempEnts = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTempEnts::~CTempEnts( void )
{
	// free pvEngineData pointers
	Clear();
}

void CTempEnts::TE_Prepare( TEMPENTITY *pTemp, model_t modelIndex )
{
	int	frameCount;

	// Use these to set per-frame and termination conditions / actions
	pTemp->flags = FTENT_NONE;		
	pTemp->die = gpGlobals->time + 0.75f;

	// pvEngineData is allocated on first call CL_AddTempEntity, freed in client.dll
	ASSERT( pTemp->pvEngineData == NULL );

	frameCount = GetModelFrames( modelIndex );

	pTemp->modelindex = modelIndex;
	pTemp->renderMode = kRenderNormal;
	pTemp->renderFX = kRenderFxNone;
	pTemp->renderColor = Vector( 255, 255, 255 );
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

int CTempEnts::TE_Active( TEMPENTITY *pTemp )
{
	bool	active = true;
	float	life;

	if( !pTemp ) return false;
	life = pTemp->die - gpGlobals->time;
	
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

int CTempEnts::TE_Update( TEMPENTITY *pTemp )
{
	// before first frame when movevars not initialized
	if( !gpMovevars )
	{
		ALERT( at_error, "TempEntUpdate: no movevars!!!\n" );
		return true;
	}

	float	gravity, gravitySlow, fastFreq;
	float	frametime = gpGlobals->frametime;

	fastFreq = gpGlobals->time * 5.5;
	gravity = -frametime * gpMovevars->gravity;
	gravitySlow = gravity * 0.5;

	// save oldorigin
	pTemp->oldorigin = pTemp->origin;

	if( pTemp->flags & FTENT_SPARKSHOWER )
	{
		// adjust speed if it's time
		// scale is next think time
		if( gpGlobals->time > pTemp->m_flSpriteScale )
		{
			// show Sparks
			g_pParticles->SparkParticles( pTemp->origin, g_vecZero );

			// reduce life
			pTemp->m_flFrameRate -= 0.1f;

			if( pTemp->m_flFrameRate <= 0.0f )
			{
				pTemp->die = gpGlobals->time;
			}
			else
			{
				// so it will die no matter what
				pTemp->die = gpGlobals->time + 0.5f;

				// next think
				pTemp->m_flSpriteScale = gpGlobals->time + 0.1f;
			}
		}
	}
	else if( pTemp->flags & FTENT_PLYRATTACHMENT )
	{
		edict_t *pClient = GetEntityByIndex( pTemp->clientIndex );

		if( pClient )
		{
			pTemp->origin = pClient->v.origin + pTemp->tentOffset;
		}
	}
	else if( pTemp->flags & FTENT_SINEWAVE )
	{
		pTemp->x += pTemp->m_vecVelocity.x * frametime;
		pTemp->y += pTemp->m_vecVelocity.y * frametime;

		pTemp->origin.x = pTemp->x + sin( pTemp->m_vecVelocity.z + gpGlobals->time ) * ( 10 * pTemp->m_flSpriteScale );
		pTemp->origin.y = pTemp->y + sin( pTemp->m_vecVelocity.z + fastFreq + 0.7f ) * ( 8 * pTemp->m_flSpriteScale);
		pTemp->origin.z = pTemp->origin.z + pTemp->m_vecVelocity[2] * frametime;
	}
	else if( pTemp->flags & FTENT_SPIRAL )
	{
		float s, c;
		s = sin( pTemp->m_vecVelocity.z + fastFreq );
		c = cos( pTemp->m_vecVelocity.z + fastFreq );

		pTemp->origin.x = pTemp->origin.x + pTemp->m_vecVelocity.x * frametime + 8 * sin( gpGlobals->time * 20 );
		pTemp->origin.y = pTemp->origin.y + pTemp->m_vecVelocity.y * frametime + 4 * sin( gpGlobals->time * 30 );
		pTemp->origin.z = pTemp->origin.z + pTemp->m_vecVelocity.z * frametime;
	}
	else
	{
		// just add linear velocity
		pTemp->origin = pTemp->origin + pTemp->m_vecVelocity * frametime;
	}
	
	if( pTemp->flags & FTENT_SPRANIMATE )
	{
		pTemp->m_flFrame += frametime * pTemp->m_flFrameRate;
		if( pTemp->m_flFrame >= pTemp->m_flFrameMax )
		{
			pTemp->m_flFrame = pTemp->m_flFrame - (int)(pTemp->m_flFrame);

			if(!( pTemp->flags & FTENT_SPRANIMATELOOP ))
			{
				// this animating sprite isn't set to loop, so destroy it.
				pTemp->die = 0.0f;
				return false;
			}
		}
	}
	else if( pTemp->flags & FTENT_SPRCYCLE )
	{
		pTemp->m_flFrame += frametime * 10;
		if( pTemp->m_flFrame >= pTemp->m_flFrameMax )
		{
			pTemp->m_flFrame = pTemp->m_flFrame - (int)(pTemp->m_flFrame);
		}
	}

	if( pTemp->flags & FTENT_SCALE )
	{
		// this only used for Egon effect
		pTemp->m_flSpriteScale += frametime * 10;
	}

	if( pTemp->flags & FTENT_ROTATE )
	{
		// just add angular velocity
		pTemp->angles += pTemp->m_vecAvelocity * frametime;
	}

	if( pTemp->flags & ( FTENT_COLLIDEALL|FTENT_COLLIDEWORLD ))
	{
		Vector	traceNormal;
		float	traceFraction = 1.0f;

		traceNormal.Init();

		if( pTemp->flags & FTENT_COLLIDEALL )
		{
			TraceResult	tr;		
			TRACE_LINE( pTemp->oldorigin, pTemp->origin, false, NULL, &tr );

			// Make sure it didn't bump into itself... (?!?)
			if(( tr.flFraction != 1.0f ) && ( tr.pHit == GetEntityByIndex( 0 ) || tr.pHit != GetEntityByIndex( pTemp->clientIndex ))) 
			{
				traceFraction = tr.flFraction;
				traceNormal = tr.vecPlaneNormal;

				if( pTemp->hitcallback )
					(*pTemp->hitcallback)( pTemp, &tr );
			}
		}
		else if( pTemp->flags & FTENT_COLLIDEWORLD )
		{
			TraceResult	tr;
			TRACE_LINE( pTemp->oldorigin, pTemp->origin, true, NULL, &tr );

			if( tr.flFraction != 1.0f )
			{
				traceFraction = tr.flFraction;
				traceNormal = tr.vecPlaneNormal;

				if ( pTemp->flags & FTENT_SPARKSHOWER )
				{
					// chop spark speeds a bit more
					pTemp->m_vecVelocity *= 0.6f;

					if( pTemp->m_vecVelocity.Length() < 10.0f )
						pTemp->m_flFrameRate = 0.0f;
				}

				if( pTemp->hitcallback )
					(*pTemp->hitcallback)( pTemp, &tr );
			}
		}
		
		if( traceFraction != 1.0f )	// Decent collision now, and damping works
		{
			float	proj, damp;

			// Place at contact point
			pTemp->origin = pTemp->oldorigin + (traceFraction * frametime) * pTemp->m_vecVelocity;
			
			// Damp velocity
			damp = pTemp->bounceFactor;
			if( pTemp->flags & ( FTENT_GRAVITY|FTENT_SLOWGRAVITY ))
			{
				damp *= 0.5f;
				if( traceNormal[2] > 0.9f ) // Hit floor?
				{
					if( pTemp->m_vecVelocity[2] <= 0 && pTemp->m_vecVelocity[2] >= gravity * 3 )
					{
						pTemp->flags &= ~(FTENT_SLOWGRAVITY|FTENT_ROTATE|FTENT_GRAVITY);
						pTemp->flags &= ~(FTENT_COLLIDEWORLD|FTENT_SMOKETRAIL);
						pTemp->angles.x = pTemp->angles.z = 0;
						damp = 0;	// stop
					}
				}
			}

			if( pTemp->hitSound )
			{
				PlaySound( pTemp, damp );
			}

			if( pTemp->flags & FTENT_COLLIDEKILL )
			{
				// die on impact
				pTemp->flags &= ~FTENT_FADEOUT;	
				pTemp->die = gpGlobals->time;			
			}
			else
			{
				// reflect velocity
				if( damp != 0 )
				{
					proj = DotProduct( pTemp->m_vecVelocity, traceNormal );
					pTemp->m_vecVelocity += (-proj * 2) * traceNormal;

					// reflect rotation (fake)
					pTemp->angles.y = -pTemp->angles.y;
				}
				
				if( damp != 1.0f )
				{
					pTemp->m_vecVelocity *= damp;
					pTemp->angles *= 0.9f;
				}
			}
		}
	}

	if(( pTemp->flags & FTENT_FLICKER ) && m_iTempEntFrame == pTemp->m_nFlickerFrame )
	{
		float	rgb[3] = { 1.0f, 0.47f, 0.0f };
		g_engfuncs.pEfxAPI->CL_AllocDLight( pTemp->origin, rgb, 60, gpGlobals->time + 0.01f, 0, 0 );
	}

	if( pTemp->flags & FTENT_SMOKETRAIL )
	{
		g_engfuncs.pEfxAPI->R_RocketTrail( pTemp->oldorigin, pTemp->origin, 1 );
	}

	if( pTemp->flags & FTENT_GRAVITY )
		pTemp->m_vecVelocity.z += gravity;
	else if( pTemp->flags & FTENT_SLOWGRAVITY )
		pTemp->m_vecVelocity.z += gravitySlow;

	if( pTemp->flags & FTENT_CLIENTCUSTOM )
	{
		if( pTemp->callback )
			(*pTemp->callback)( pTemp );
	}

	if( pTemp->flags & FTENT_WINDBLOWN )
	{
		Vector	vecWind = gHUD.m_vecWindVelocity;

		for( int i = 0 ; i < 2 ; i++ )
		{
			if( pTemp->m_vecVelocity[i] < vecWind[i] )
			{
				pTemp->m_vecVelocity[i] += ( frametime * TENT_WIND_ACCEL );

				// clamp
				if( pTemp->m_vecVelocity[i] > vecWind[i] )
					pTemp->m_vecVelocity[i] = vecWind[i];
			}
			else if( pTemp->m_vecVelocity[i] > vecWind[i] )
			{
				pTemp->m_vecVelocity[i] -= ( frametime * TENT_WIND_ACCEL );

				// clamp
				if( pTemp->m_vecVelocity[i] < vecWind[i] )
					pTemp->m_vecVelocity[i] = vecWind[i];
			}
		}
	}
	return true;
}

void CTempEnts :: Update( void )
{
	TEMPENTITY *current, *pnext, *pprev;

	if ( !m_pActiveTempEnts )		
	{
		return;
	}

	// !!!BUGBUG -- This needs to be time based
	// g-cont. it's used only for flickering dlights, what difference ?
	m_iTempEntFrame = ( m_iTempEntFrame + 1 ) & 31;

	current = m_pActiveTempEnts;

	// !!! Don't simulate while paused....  This is sort of a hack, revisit.
	if( IN_GAME() == 0 )
	{
		while( current )
		{
			CL_AddTempEntity( current, -1 );
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
			if( !TE_Active( current ) || !TE_Update( current ))
			{
				TempEntFree( current, pprev );
				fTempEntFreed = true;
			}
			else
			{
				// renderer rejected entity for some reasons...
				if( !CL_AddTempEntity( current, -1 ))
				{
					if(!( current->flags & FTENT_PERSIST )) 
					{
						// If we can't draw it this frame, just dump it.
						current->die = gpGlobals->time;
						// don't fade out, just die
						current->flags &= ~FTENT_FADEOUT;

						TempEntFree( current, pprev );
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

//-----------------------------------------------------------------------------
// Purpose: Clear existing temp entities
//-----------------------------------------------------------------------------
void CTempEnts::Clear( void )
{
	m_iTempEntFrame = 0;

	// update muzzleflash indexes
	m_iMuzzleFlash[0] = g_engfuncs.pEventAPI->EV_FindModelIndex( "sprites/muzzleflash1.spr" );
	m_iMuzzleFlash[1] = g_engfuncs.pEventAPI->EV_FindModelIndex( "sprites/muzzleflash2.spr" );
	m_iMuzzleFlash[2] = g_engfuncs.pEventAPI->EV_FindModelIndex( "sprites/muzzleflash3.spr" );
	m_iMuzzleFlash[3] = g_engfuncs.pEventAPI->EV_FindModelIndex( "sprites/muzzleflash.spr" );

	hSprGlowShell = TEX_Load( "renderfx/glowshell" );

	for ( int i = 0; i < MAX_TEMP_ENTITIES-1; i++ )
	{
		m_TempEnts[i].next = &m_TempEnts[i+1];

		if( m_TempEnts[i].pvEngineData )
			FREE( m_TempEnts[i].pvEngineData );
		m_TempEnts[i].pvEngineData = NULL;
	}

	m_TempEnts[MAX_TEMP_ENTITIES-1].next = NULL;
	m_pFreeTempEnts = m_TempEnts;
	m_pActiveTempEnts = NULL;

	// clearing user pointers
	m_pLaserSpot = NULL;
	m_pEndFlare = NULL;
}

void CTempEnts::TempEntFree( TEMPENTITY *pTemp, TEMPENTITY *pPrev )
{
	// Remove from the active list.
	if( pPrev )	
	{
		pPrev->next = pTemp->next;
	}
	else
	{
		m_pActiveTempEnts = pTemp->next;
	}

	// Cleanup its data.
	if( pTemp->pvEngineData )
	{
		// its engine memory block with extradata
		// if we will be used 'free' or 'delete'
		// engine will be crashed on next memory checking!!!
		FREE( pTemp->pvEngineData );
	}

	memset( pTemp, 0, sizeof( TEMPENTITY ));

	// Add to the free list.
	pTemp->next = m_pFreeTempEnts;
	m_pFreeTempEnts = pTemp;
}

// free the first low priority tempent it finds.
bool CTempEnts::FreeLowPriorityTempEnt( void )
{
	TEMPENTITY	*pActive = m_pActiveTempEnts;
	TEMPENTITY	*pPrev = NULL;

	while( pActive )
	{
		if( pActive->priority == TENTPRIORITY_LOW )
		{
			TempEntFree( pActive, pPrev );
			return true;
		}

		pPrev = pActive;
		pActive = pActive->next;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Allocate temp entity ( normal/low priority )
// Input  : *org - 
//			*model - 
// Output : C_LocalTempEntity
//-----------------------------------------------------------------------------
TEMPENTITY *CTempEnts::TempEntAlloc( const Vector& org, model_t model )
{
	TEMPENTITY	*pTemp;

	if ( !m_pFreeTempEnts )
	{
		ALERT( at_console, "Overflow %d temporary ents!\n", MAX_TEMP_ENTITIES );
		return NULL;
	}

	pTemp = m_pFreeTempEnts;
	m_pFreeTempEnts = pTemp->next;

	TE_Prepare( pTemp, model );

	pTemp->priority = TENTPRIORITY_LOW;
	if( org ) pTemp->origin = org;

	pTemp->next = m_pActiveTempEnts;
	m_pActiveTempEnts = pTemp;

	return pTemp;
}

//-----------------------------------------------------------------------------
// Purpose: Allocate a temp entity, if there are no slots, kick out a low priority
//  one if possible
// Input  : *org - 
//			*model - 
// Output : C_LocalTempEntity
//-----------------------------------------------------------------------------
TEMPENTITY *CTempEnts::TempEntAllocHigh( const Vector& org, model_t model )
{
	TEMPENTITY	*pTemp;

	if ( !m_pFreeTempEnts )
	{
		// no temporary ents free, so find the first active low-priority temp ent 
		// and overwrite it.
		FreeLowPriorityTempEnt();
	}

	if ( !m_pFreeTempEnts )
	{
		// didn't find anything? The tent list is either full of high-priority tents
		// or all tents in the list are still due to live for > 10 seconds. 
		ALERT( at_console, "Couldn't alloc a high priority TENT!\n" );
		return NULL;
	}

	// Move out of the free list and into the active list.
	pTemp = m_pFreeTempEnts;
	m_pFreeTempEnts = pTemp->next;

	pTemp->next = m_pActiveTempEnts;
	m_pActiveTempEnts = pTemp;

	ASSERT( pTemp->pvEngineData == NULL );
	TE_Prepare( pTemp, model );

	pTemp->priority = TENTPRIORITY_HIGH;
	if( org ) pTemp->origin = org;

	return pTemp;
}


TEMPENTITY *CTempEnts::TempEntAllocNoModel( const Vector& org )
{
	return TempEntAlloc( org, 0 );
}

TEMPENTITY *CTempEnts::TempEntAllocCustom( const Vector& org, int modelIndex, int high, ENTCALLBACK pfnCallback )
{
	TEMPENTITY	*pTemp;

	if( high )
	{
		pTemp = TempEntAllocHigh( org, modelIndex );
	}
	else
	{
		pTemp = TempEntAlloc( org, modelIndex );
	}

	if( pTemp && pfnCallback )
	{
		pTemp->callback = pfnCallback;
		pTemp->flags |= FTENT_CLIENTCUSTOM;
	}

	return pTemp;
}

/*
==============================================================

	BASE TEMPENTS CODE

==============================================================
*/
//-----------------------------------------------------------------------------
// Purpose: Create a fizz effect
// Input  : *pent - 
//			modelIndex - 
//			density - 
//-----------------------------------------------------------------------------
void CTempEnts::FizzEffect( edict_t *pent, int modelIndex, int density )
{
	TEMPENTITY	*pTemp;
	int		i, width, depth, count, frameCount;
	float		angle, maxHeight, speed, xspeed, yspeed;
	Vector		origin;
	Vector		mins, maxs;

	if( !pent || pent->free || GetModelType( modelIndex ) == mod_bad )
		return;

	count = density + 1;
	density = count * 3 + 6;

	GetModelBounds( modelIndex, mins, maxs );

	maxHeight = maxs[2] - mins[2];
	width = maxs[0] - mins[0];
	depth = maxs[1] - mins[1];
	speed = ((int)pent->v.rendercolor.y<<8|(int)pent->v.rendercolor.x);
	if( pent->v.rendercolor.z ) speed = -speed;

	ALERT( at_console, "speed %g\n", speed );

	angle = pent->v.angles[YAW] * M_PI / 180;
	yspeed = sin( angle );
	xspeed = cos( angle );

	xspeed *= speed;
	yspeed *= speed;
	frameCount = GetModelFrames( modelIndex );

	for ( i = 0; i < count; i++ )
	{
		origin[0] = mins[0] + RANDOM_LONG( 0, width - 1 );
		origin[1] = mins[1] + RANDOM_LONG( 0, depth - 1 );
		origin[2] = mins[2];
		pTemp = TempEntAlloc( origin, modelIndex );

		if ( !pTemp ) return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];

		float zspeed = RANDOM_LONG( 80, 140 );
		pTemp->m_vecVelocity = Vector( xspeed, yspeed, zspeed );
		pTemp->die = gpGlobals->time + ( maxHeight / zspeed ) - 0.1f;
		pTemp->m_flFrame = RANDOM_LONG( 0, frameCount - 1 );
		// Set sprite scale
		pTemp->m_flSpriteScale = 1.0f / RANDOM_FLOAT( 2, 5 );
		pTemp->renderMode = kRenderTransAlpha;
		pTemp->renderAmt = pTemp->startAlpha = 255;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create bubbles
// Input  : *mins - 
//			*maxs - 
//			height - 
//			modelIndex - 
//			count - 
//			speed - 
//-----------------------------------------------------------------------------
void CTempEnts::Bubbles( const Vector &mins, const Vector &maxs, float height, int modelIndex, int count, float speed )
{
	TEMPENTITY	*pTemp;
	int		i, frameCount;
	float		sine, cosine;
	Vector		origin;

	if( GetModelType( modelIndex ) == mod_bad )
		return;

	frameCount = GetModelFrames( modelIndex );

	for ( i = 0; i < count; i++ )
	{
		origin[0] = RANDOM_LONG( mins[0], maxs[0] );
		origin[1] = RANDOM_LONG( mins[1], maxs[1] );
		origin[2] = RANDOM_LONG( mins[2], maxs[2] );
		pTemp = TempEntAlloc( origin, modelIndex );
		if ( !pTemp ) return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];
		float angle = RANDOM_LONG( -M_PI, M_PI );
		sine = sin( angle );
		cosine = cos( angle );
		
		float zspeed = RANDOM_LONG( 80, 140 );
		pTemp->m_vecVelocity = Vector( speed * cosine, speed * sine, zspeed );
		pTemp->die = gpGlobals->time + ((height - (origin[2] - mins[2])) / zspeed) - 0.1f;
		pTemp->m_flFrame = RANDOM_LONG( 0, frameCount - 1 );
		
		// Set sprite scale
		pTemp->m_flSpriteScale = 1.0 / RANDOM_FLOAT( 4, 16 );
		pTemp->renderMode = kRenderTransAlpha;
		pTemp->renderAmt = pTemp->startAlpha = 192;	// g-cont. why difference with FizzEffect ???		
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create bubble trail
// Input  : *start - 
//			*end - 
//			height - 
//			modelIndex - 
//			count - 
//			speed - 
//-----------------------------------------------------------------------------
void CTempEnts::BubbleTrail( const Vector &start, const Vector &end, float flWaterZ, int modelIndex, int count, float speed )
{
	TEMPENTITY	*pTemp;
	int		i, frameCount;
	float		dist, angle;
	Vector		origin;

	if( GetModelType( modelIndex ) == mod_bad )
		return;

	frameCount = GetModelFrames( modelIndex );

	for ( i = 0; i < count; i++ )
	{
		dist = RANDOM_FLOAT( 0, 1.0 );	// g-cont. hmm may be use GetLerpFrac instead ?

		origin = LerpPoint( start, end, dist );
		pTemp = TempEntAlloc( origin, modelIndex );
		if ( !pTemp ) return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];
		angle = RANDOM_LONG( -M_PI, M_PI );

		float zspeed = RANDOM_LONG( 80, 140 );
		pTemp->m_vecVelocity = Vector( speed * cos( angle ), speed * sin( angle ), zspeed );
		pTemp->die = gpGlobals->time + (( flWaterZ - origin[2]) / zspeed ) - 0.1f;
		pTemp->m_flFrame = RANDOM_LONG( 0, frameCount - 1 );
		// Set sprite scale
		pTemp->m_flSpriteScale = 1.0 / RANDOM_FLOAT( 4, 8 );
		pTemp->renderMode = kRenderTransAlpha;
		pTemp->renderAmt = pTemp->startAlpha = 192;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Attaches entity to player
// Input  : client - 
//			modelIndex - 
//			zoffset - 
//			life - 
//-----------------------------------------------------------------------------
void CTempEnts::AttachTentToPlayer( int client, int modelIndex, float zoffset, float life )
{
	TEMPENTITY	*pTemp;
	Vector		position;
	int		frameCount;

	if ( client <= 0 || client > gpGlobals->maxClients )
	{
		ALERT( at_warning, "Bad client in AttachTentToPlayer()!\n" );
		return;
	}

	edict_t	*pClient = GetEntityByIndex( client );
	if ( !pClient )
	{
		ALERT( at_warning, "Couldn't get ClientEntity for %i\n", client );
		return;
	}

	if( GetModelType( modelIndex ) == mod_bad )
	{
		ALERT( at_console, "No model %d!\n", modelIndex );
		return;
	}

	position = pClient->v.origin;
	position[2] += zoffset;

	pTemp = TempEntAllocHigh( position, modelIndex );
	if ( !pTemp )
	{
		ALERT( at_warning, "No temp ent.\n" );
		return;
	}

	pTemp->renderMode = kRenderNormal;
	pTemp->renderAmt = pTemp->startAlpha = 192;
	pTemp->renderFX = kRenderFxNoDissipation;
	
	pTemp->clientIndex = client;
	pTemp->tentOffset[0] = 0;
	pTemp->tentOffset[1] = 0;
	pTemp->tentOffset[2] = zoffset;
	pTemp->die = gpGlobals->time + life;
	pTemp->flags |= FTENT_PLYRATTACHMENT|FTENT_PERSIST;

	// is the model a sprite?
	if ( GetModelType( pTemp->modelindex ) == mod_sprite )
	{
		frameCount = GetModelFrames( pTemp->modelindex );
		pTemp->m_flFrameMax = frameCount - 1;
		pTemp->flags |= FTENT_SPRANIMATE|FTENT_SPRANIMATELOOP;
		pTemp->m_flFrameRate = 10;
	}
	else
	{
		// no animation support for attached clientside studio models.
		pTemp->m_flFrameMax = 0;
	}

	pTemp->m_flFrame = 0;
}
#define FOR_EACH_LL( listName, iteratorName ) \
	for( int iteratorName=listName.Head(); iteratorName != listName.InvalidIndex(); iteratorName = listName.Next( iteratorName ) )

//-----------------------------------------------------------------------------
// Purpose: Detach entity from player
//-----------------------------------------------------------------------------
void CTempEnts::KillAttachedTents( int client )
{
	if ( client <= 0 || client > gpGlobals->maxClients )
	{
		ALERT( at_warning, "Bad client in KillAttachedTents()!\n" );
		return;
	}

	for( int i = 0; i < MAX_TEMP_ENTITIES; i++ )
	{
		TEMPENTITY *pTemp = &m_TempEnts[i];

		if ( pTemp->flags & FTENT_PLYRATTACHMENT )
		{
			// this TENT is player attached.
			// if it is attached to this client, set it to die instantly.
			if ( pTemp->clientIndex == client )
			{
				pTemp->die = gpGlobals->time; // good enough, it will die on next tent update. 
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create ricochet sprite
// Input  : *pos - 
//			*pmodel - 
//			duration - 
//			scale - 
//-----------------------------------------------------------------------------
void CTempEnts::RicochetSprite( const Vector &pos, model_t modelIndex, float scale )
{
	TEMPENTITY	*pTemp;

	pTemp = TempEntAlloc( pos, modelIndex );
	if (!pTemp)
		return;

	pTemp->renderMode = kRenderGlow;
	pTemp->renderAmt = pTemp->startAlpha = 200;
	pTemp->renderFX = kRenderFxNoDissipation;
	pTemp->m_flSpriteScale = scale;
	pTemp->flags = FTENT_FADEOUT;
	pTemp->fadeSpeed = 8;
	pTemp->die = gpGlobals->time;

	pTemp->m_flFrame = 0;
	pTemp->angles[ROLL] = 45 * RANDOM_LONG( 0, 7 );
}

void CTempEnts::PlaySound( TEMPENTITY *pTemp, float damp )
{
	float fvol;
	char soundname[32];
	bool isshellcasing = false;
	int zvel;

	switch ( pTemp->hitSound )
	{
	default:
		return;	// null sound
	case BOUNCE_GLASS:
		{
			sprintf( soundname, "debris/glass%i.wav", RANDOM_LONG( 1, 4 ));
		}
		break;
	case BOUNCE_METAL:
		{
			sprintf( soundname, "debris/metal%i.wav", RANDOM_LONG( 1, 6 ));
		}
		break;
	case BOUNCE_FLESH:
		{
			sprintf( soundname, "debris/flesh%i.wav", RANDOM_LONG( 1, 7 ));
		}
		break;
	case BOUNCE_WOOD:
		{
			sprintf( soundname, "debris/wood%i.wav", RANDOM_LONG( 1, 4 ));
		}
		break;
	case BOUNCE_SHRAP:
		{
			sprintf( soundname, "weapons/ric%i.wav", RANDOM_LONG( 1, 5 ));
		}
		break;
	case BOUNCE_SHOTSHELL:
		{
			sprintf( soundname, "weapons/sshell%i.wav", RANDOM_LONG( 1, 3 ));
			isshellcasing = true; // shell casings have different playback parameters
		}
		break;
	case BOUNCE_SHELL:
		{
			sprintf( soundname, "player/pl_shell%i.wav", RANDOM_LONG( 1, 3 ));
			isshellcasing = true; // shell casings have different playback parameters
		}
		break;
	case BOUNCE_CONCRETE:
		{
			sprintf( soundname, "debris/concrete%i.wav", RANDOM_LONG( 1, 3 ));
		}
		break;
	}

	zvel = abs( pTemp->m_vecVelocity[2] );
		
	// only play one out of every n

	if ( isshellcasing )
	{	
		// play first bounce, then 1 out of 3		
		if ( zvel < 200 && RANDOM_LONG( 0, 3 ))
			return;
	}
	else
	{
		if ( RANDOM_LONG( 0, 5 )) 
			return;
	}

	fvol = 1.0f;

	if ( damp > 0.0 )
	{
		int pitch;
		
		if ( isshellcasing )
		{
			fvol *= min ( 1.0f, ((float)zvel) / 350.0 ); 
		}
		else
		{
			fvol *= min ( 1.0f, ((float)zvel) / 450.0 ); 
		}
		
		if ( !RANDOM_LONG( 0, 3 ) && !isshellcasing )
		{
			pitch = RANDOM_LONG( 95, 105 );
		}
		else
		{
			pitch = 100; // FIXME
		}
		CL_PlaySound( soundname, fvol, pTemp->origin, pitch );
	}
}

void CTempEnts::RocketFlare( const Vector& pos )
{
	TEMPENTITY	*pTemp;
	model_t		modelIndex;
	int		nframeCount;

	modelIndex = g_engfuncs.pEventAPI->EV_FindModelIndex( "sprites/animglow01.spr" );
	if( !modelIndex ) return;

	nframeCount = GetModelFrames( modelIndex );

	pTemp = TempEntAlloc( pos, modelIndex );
	if ( !pTemp ) return;

	pTemp->m_flFrameMax = nframeCount - 1;
	pTemp->renderMode = kRenderGlow;
	pTemp->renderFX = kRenderFxNoDissipation;
	pTemp->renderAmt = 200;
	pTemp->m_flFrameRate = 1.0;
	pTemp->m_flFrame = RANDOM_LONG( 0, nframeCount - 1 );
	pTemp->m_flSpriteScale = 0.5;
	pTemp->die = gpGlobals->time + 0.01f;	// when 100 fps die at next frame
	pTemp->flags |= FTENT_SPRANIMATE;
}

void CTempEnts::MuzzleFlash( edict_t *pEnt, int iAttachment, int type )
{
	Vector pos;
	TEMPENTITY *pTemp;
	int index, modelIndex, frameCount;
	float scale;

	index = bound( 0, type % 10, MAX_MUZZLEFLASH - 1 );
	scale = (type / 10) * 0.1f;
	if( scale == 0.0f ) scale = 0.5f;

	modelIndex = m_iMuzzleFlash[index];
	if( !modelIndex ) return;

	frameCount = GetModelFrames( modelIndex );

	if( !GET_ATTACHMENT( pEnt, iAttachment, pos, NULL ))
	{
		ALERT( at_error, "Invalid muzzleflash entity!\n" );
		return;
	}

	// must set position for right culling on render
	pTemp = TempEntAlloc( pos, modelIndex );
	if( !pTemp ) return;
	
	pTemp->renderMode = kRenderTransAdd;
	pTemp->renderAmt = 255;
	pTemp->renderFX = 0;
	pTemp->die = gpGlobals->time + 0.015; // die at next frame
	pTemp->m_flFrame = RANDOM_LONG( 0, frameCount - 1 );
	pTemp->m_flFrameMax = frameCount - 1;
	pTemp->clientIndex = pEnt->serialnumber;
	pTemp->m_iAttachment = iAttachment;

	if( index == 0 )
	{
		// Rifle flash
		pTemp->m_flSpriteScale = scale * RANDOM_FLOAT( 0.5f, 0.6f );
		pTemp->angles[2] = 90 * RANDOM_LONG( 0, 3 );
	}
	else
	{
		pTemp->m_flSpriteScale = scale;
		pTemp->angles[2] = RANDOM_LONG( 0, 359 );
	}

	// render now (guranteed that muzzleflash will be draw)
	CL_AddTempEntity( pTemp, -1 );
}

void CTempEnts::BloodSprite( const Vector &org, int colorIndex, int modelIndex, int modelIndex2, float size )
{
	TEMPENTITY	*pTemp;

	if( GetModelType( modelIndex ) == mod_bad )
		return;

	// Large, single blood sprite is a high-priority tent
	if( pTemp = TempEntAllocHigh( org, modelIndex ))
	{
		int	frameCount;

		frameCount = GetModelFrames( modelIndex );
		pTemp->renderMode = kRenderTransTexture;
		pTemp->renderFX = kRenderFxClampMinScale;
		pTemp->m_flSpriteScale = RANDOM_FLOAT(( size / 25.0f), ( size / 35.0f ));
		pTemp->flags = FTENT_SPRANIMATE;
 
		CL_GetPaletteColor( colorIndex, pTemp->renderColor );
		pTemp->m_flFrameRate = frameCount * 4; // Finish in 0.250 seconds
		// play the whole thing once
		pTemp->die = gpGlobals->time + (frameCount / pTemp->m_flFrameRate);

		pTemp->angles[2] = RANDOM_LONG( 0, 360 );
		pTemp->bounceFactor = 0;
	}
}

void CTempEnts::BreakModel( const Vector &pos, const Vector &size, const Vector &dir, float random, float life, int count, int modelIndex, char flags )
{
	int		i, frameCount;
	TEMPENTITY	*pTemp;
	char		type;

	if( !modelIndex ) return;

	type = flags & BREAK_TYPEMASK;

	if( GetModelType( modelIndex ) == mod_bad )
		return;

	frameCount = GetModelFrames( modelIndex );
		
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
		vecSpot[0] = pos[0] + RANDOM_FLOAT( -0.5f, 0.5f ) * size[0];
		vecSpot[1] = pos[1] + RANDOM_FLOAT( -0.5f, 0.5f ) * size[1];
		vecSpot[2] = pos[2] + RANDOM_FLOAT( -0.5f, 0.5f ) * size[2];

		pTemp = TempEntAlloc( vecSpot, modelIndex );
		if( !pTemp ) return;

		// keep track of break_type, so we know how to play sound on collision
		pTemp->hitSound = type;
		
		if( GetModelType( modelIndex ) == mod_sprite )
			pTemp->m_flFrame = RANDOM_LONG( 0, frameCount - 1 );
		else if( GetModelType( modelIndex ) == mod_studio )
			pTemp->body = RANDOM_LONG( 0, frameCount - 1 );

		pTemp->flags |= FTENT_COLLIDEWORLD | FTENT_FADEOUT | FTENT_SLOWGRAVITY;

		if( RANDOM_LONG( 0, 255 ) < 200 ) 
		{
			pTemp->flags |= FTENT_ROTATE;
			pTemp->m_vecAvelocity[0] = RANDOM_FLOAT( -256, 255 );
			pTemp->m_vecAvelocity[1] = RANDOM_FLOAT( -256, 255 );
			pTemp->m_vecAvelocity[2] = RANDOM_FLOAT( -256, 255 );
		}

		if(( RANDOM_LONG( 0, 255 ) < 100 ) && ( flags & BREAK_SMOKE ))
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

		pTemp->m_vecVelocity[0] = dir[0] + RANDOM_FLOAT( -random, random );
		pTemp->m_vecVelocity[1] = dir[1] + RANDOM_FLOAT( -random, random );
		pTemp->m_vecVelocity[2] = dir[2] + RANDOM_FLOAT( 0, random );

		pTemp->die = gpGlobals->time + life + RANDOM_FLOAT( 0, 1 ); // Add an extra 0-1 secs of life
	}
}

void CTempEnts::TempModel( const Vector &pos, const Vector &dir, const Vector &ang, float life, int modelIndex, int soundtype )
{
	// alloc a new tempent
	TEMPENTITY *pTemp = TempEntAlloc( pos, modelIndex );
	if( !pTemp ) return;

	// keep track of shell type
	switch( soundtype )
	{
	case TE_BOUNCE_SHELL: pTemp->hitSound = BOUNCE_SHELL; break;
	case TE_BOUNCE_SHOTSHELL: pTemp->hitSound = BOUNCE_SHOTSHELL; break;
	}

	pTemp->origin  = pos;
	pTemp->m_vecVelocity = dir;
	pTemp->angles = ang;

	pTemp->body = 0;
	pTemp->flags = (FTENT_COLLIDEWORLD|FTENT_FADEOUT|FTENT_GRAVITY|FTENT_ROTATE);
	pTemp->m_vecAvelocity[0] = RANDOM_FLOAT( -255, 255 );
	pTemp->m_vecAvelocity[1] = RANDOM_FLOAT( -255, 255 );
	pTemp->m_vecAvelocity[2] = RANDOM_FLOAT( -255, 255 );
	pTemp->renderMode = kRenderNormal;
	pTemp->startAlpha = 255;
	pTemp->die = gpGlobals->time + life;
}

TEMPENTITY *CTempEnts::DefaultSprite( const Vector &pos, int spriteIndex, float framerate )
{
	TEMPENTITY	*pTemp;
	int		frameCount;

	if( !spriteIndex || GetModelType( spriteIndex ) != mod_sprite )
	{
		ALERT( at_console, "No Sprite %d!\n", spriteIndex );
		return NULL;
	}

	frameCount = GetModelFrames( spriteIndex );

	pTemp = TempEntAlloc( pos, spriteIndex );
	if( !pTemp ) return NULL;

	pTemp->m_flFrameMax = frameCount - 1;
	pTemp->m_flSpriteScale = 1.0f;
	pTemp->flags |= FTENT_SPRANIMATE;
	if( framerate == 0 ) framerate = 10;

	pTemp->m_flFrameRate = framerate;
	pTemp->die = gpGlobals->time + (float)frameCount / framerate;
	pTemp->m_flFrame = 0;

	return pTemp;
}

/*
===============
CL_TempSprite
===============
*/
TEMPENTITY *CTempEnts::TempSprite( const Vector &pos, const Vector &dir, float scale, int modelIndex, int rendermode, int renderfx, float a, float life, int flags )
{
	TEMPENTITY	*pTemp;
	int		frameCount;

	if( !modelIndex ) 
		return NULL;

	if( GetModelType( modelIndex ) == mod_bad )
	{
		ALERT( at_console, "No model %d!\n", modelIndex );
		return NULL;
	}

	frameCount = GetModelFrames( modelIndex );

	pTemp = TempEntAlloc( pos, modelIndex );
	if( !pTemp ) return NULL;

	pTemp->m_flFrameMax = frameCount - 1;
	pTemp->m_flFrameRate = 10;
	pTemp->renderMode = rendermode;
	pTemp->renderFX = renderfx;
	pTemp->m_flSpriteScale = scale;
	pTemp->startAlpha = a * 255;
	pTemp->renderAmt = a * 255;
	pTemp->flags |= flags;

	pTemp->origin = pos;
	pTemp->m_vecVelocity = dir;

	if( life ) pTemp->die = gpGlobals->time + life;
	else pTemp->die = gpGlobals->time + (frameCount * 0.1f) + 1.0f;
	pTemp->m_flFrame = 0;

	return pTemp;
}

void CTempEnts::Sprite_Explode( TEMPENTITY *pTemp, float scale, int flags )
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
		pTemp->angles[2] = RANDOM_LONG( 0, 360 );
	}

	pTemp->renderFX = kRenderFxNone;
	pTemp->m_vecVelocity[2] = 8;
	pTemp->origin[2] += 10;
	pTemp->m_flSpriteScale = scale;
}

void CTempEnts::Sprite_Smoke( TEMPENTITY *pTemp, float scale )
{
	int	iColor;

	if( !pTemp ) return;

	iColor = RANDOM_LONG( 20, 35 );
	pTemp->renderMode = kRenderTransAlpha;
	pTemp->renderFX = kRenderFxNone;
	pTemp->m_vecVelocity[2] = 30;
	pTemp->renderColor = Vector( iColor, iColor, iColor );
	pTemp->origin[2] += 20;
	pTemp->m_flSpriteScale = scale;
	pTemp->flags |= FTENT_WINDBLOWN;

}

void CTempEnts::Sprite_Spray( const Vector &pos, const Vector &dir, int modelIndex, int count, int speed, int iRand, int renderMode )
{
	TEMPENTITY	*pTemp;
	float		noise;
	float		znoise;
	int		i, frameCount;

	noise = (float)iRand / 100;

	// more vertical displacement
	znoise = noise * 1.5f;
	
	if( znoise > 1 ) znoise = 1;

	if( GetModelType( modelIndex ) == mod_bad )
	{
		ALERT( at_console, "No model %d!\n", modelIndex );
		return;
	}

	frameCount = GetModelFrames( modelIndex );

	for( i = 0; i < count; i++ )
	{
		vec3_t	velocity;
		float	scale;

		pTemp = TempEntAlloc( pos, modelIndex );
		if( !pTemp ) return;

		pTemp->renderMode = renderMode;
		pTemp->renderFX = kRenderFxNoDissipation;
		pTemp->m_flSpriteScale = 0.5f;
		pTemp->flags |= FTENT_FADEOUT|FTENT_SLOWGRAVITY;
		pTemp->fadeSpeed = 2.0f;

		// make the spittle fly the direction indicated, but mix in some noise.
		velocity[0] = dir[0] + RANDOM_FLOAT( -noise, noise );
		velocity[1] = dir[1] + RANDOM_FLOAT( -noise, noise );
		velocity[2] = dir[2] + RANDOM_FLOAT( 0, znoise );
		scale = RANDOM_FLOAT(( speed * 0.8f ), ( speed * 1.2f ));
		pTemp->m_vecVelocity = velocity * scale;
		pTemp->die = gpGlobals->time + 0.35f;
		pTemp->m_flFrame = RANDOM_LONG( 0, frameCount - 1 );
	}
}

void CTempEnts::Sprite_Trail( int type, const Vector &vecStart, const Vector &vecEnd, int modelIndex, int nCount, float flLife, float flSize, float flAmplitude, int nRenderamt, float flSpeed )
{
	TEMPENTITY	*pTemp;
	vec3_t		vecDelta, vecDir;
	int		i, flFrameCount;

	if( GetModelType( modelIndex ) == mod_bad )
	{
		ALERT( at_console, "No model %d!\n", modelIndex );
		return;
	}	

	flFrameCount = GetModelFrames( modelIndex );

	vecDelta = vecEnd - vecStart;
	vecDir = vecDelta.Normalize();

	flAmplitude /= 256.0;

	for ( i = 0; i < nCount; i++ )
	{
		Vector vecPos, vecVel;

		// Be careful of divide by 0 when using 'count' here...
		if ( i == 0 )
		{
			vecPos = vecStart;
		}
		else
		{
			vecPos = vecStart + vecDelta * ( i / ( nCount - 1.0f ));
		}

		pTemp = TempEntAlloc( vecPos, modelIndex );
		if( !pTemp ) return;

		pTemp->flags |= FTENT_COLLIDEWORLD | FTENT_SPRCYCLE | FTENT_FADEOUT | FTENT_SLOWGRAVITY;

		vecVel = vecDir * flSpeed;
		vecVel[0] += RANDOM_FLOAT( -127, 128 ) * flAmplitude;
		vecVel[1] += RANDOM_FLOAT( -127, 128 ) * flAmplitude;
		vecVel[2] += RANDOM_FLOAT( -127, 128 ) * flAmplitude;
		pTemp->m_vecVelocity = vecVel;
		pTemp->origin = vecPos;

		pTemp->m_flSpriteScale = flSize;
		pTemp->renderMode = kRenderGlow;
		pTemp->renderFX = kRenderFxNoDissipation;
		pTemp->renderAmt = pTemp->startAlpha = nRenderamt;

		pTemp->m_flFrame = RANDOM_LONG( 0, flFrameCount - 1 );
		pTemp->m_flFrameMax	= flFrameCount - 1;
		pTemp->die = gpGlobals->time + flLife + RANDOM_FLOAT( 0, 4 );
	}
}

void CTempEnts::TracerEffect( const Vector &start, const Vector &end )
{
	vec3_t vec;
	CParticle	src;
	int pal = 238; // nice color for tracers

	if( !start || !end ) return;

	vec = ( end - start ).Normalize();

	CL_GetPaletteColor( pal, src.color );

	src.colorVelocity.Init();
	src.velocity.Init();
	src.accel.Init();

	src.alpha = 1.0f;
	src.alphaVelocity = -12.0;	// lifetime
	src.radius = 1.2f;
	src.radiusVelocity = 0;
	src.length = 12;
	src.lengthVelocity = 0;
	src.rotation = 0;

	src.origin = start;
	src.velocity[0] = 6000.0f * vec[0];
	src.velocity[1] = 6000.0f * vec[1];
	src.velocity[2] = 6000.0f * vec[2];

	// 0 is a quake particle
	g_pParticles->AddParticle( &src, 0, FPART_STRETCH );
}

void CTempEnts::WeaponFlash( edict_t *pEnt, int iAttachment )
{
	Vector	pos;

	GET_ATTACHMENT( pEnt, iAttachment, pos, NULL );
	if( pos == pEnt->v.origin ) return; // missing attachment

	AllocDLight( pos, 255, 180, 64, 100, 0.05f, 0 );
}

void CTempEnts::PlaceDecal( Vector pos, float scale, int decalIndex )
{
	HSPRITE	hDecal;

	hDecal = g_engfuncs.pEfxAPI->CL_DecalIndex( decalIndex );
	g_engfuncs.pEfxAPI->R_ShootDecal( hDecal, NULL, pos, 0, RANDOM_LONG( 0, 359 ), scale );
}

void CTempEnts::PlaceDecal( Vector pos, float scale, const char *decalname )
{
	HSPRITE	hDecal;

	hDecal = g_engfuncs.pEfxAPI->CL_DecalIndexFromName( decalname );
	g_engfuncs.pEfxAPI->R_ShootDecal( hDecal, NULL, pos, 0, RANDOM_LONG( 0, 359 ), scale );
}

void CTempEnts::AllocDLight( Vector pos, float r, float g, float b, float radius, float time, int flags )
{
	if( radius <= 0 ) return;

	float	rgb[3];
	
	rgb[0] = r / 255.0f;
	rgb[1] = r / 255.0f;
	rgb[2] = r / 255.0f;

	g_engfuncs.pEfxAPI->CL_AllocDLight( pos, rgb, radius, time, flags, 0 );
}

void CTempEnts::AllocDLight( Vector pos, float radius, float time, int flags )
{
	AllocDLight( pos, 255, 255, 255, radius, time, flags );
}

void CTempEnts::RocketTrail( Vector start, Vector end, int type )
{
}