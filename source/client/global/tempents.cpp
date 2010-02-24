//=======================================================================
//			Copyright XashXT Group 2008 ©
//		tempents.cpp - client side entity management functions
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "studio_event.h"
#include "effects_api.h"
#include "pm_movevars.h"
#include "te_message.h"
#include "hud.h"

#define TENT_WIND_ACCEL		50

model_t	g_muzzleFlash[4];	// custom muzzleflashes

void HUD_MuzzleFlash( edict_t *m_pEnt, int m_iAttachment, const char *event )
{
	Vector	pos;
	int	type, index;

	type = atoi( event );
	index = bound( 0, type % 10, 3 );

	GET_ATTACHMENT( m_pEnt, m_iAttachment, pos, NULL );
	g_engfuncs.pEfxAPI->R_MuzzleFlash( pos, g_muzzleFlash[index], type );
}

void HUD_CreateEntities( void )
{
	// add in any game specific objects here
}

int HUD_UpdateEntity( TEMPENTITY *pTemp, int framenumber )
{
	// before first frame when movevars not initialized
	if( !gpMovevars ) return true;

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
// FIXME: implement
//			g_engfuncs.pEfxAPI->R_SparkEffect( pTemp->origin, 8, -200, 200 );

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
		pTemp->m_flSpriteScale += pTemp->m_flFrameRate += 20.0 * (frametime / pTemp->m_flFrameRate);
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
// FIXME: implement
//				g_engfuncs.pEfxAPI->CL_PlaySound( pTemp, damp );
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

	if(( pTemp->flags & FTENT_FLICKER ) && framenumber == pTemp->m_nFlickerFrame )
	{
		float	rgb[3] = { 1.0f, 0.47f, 0.0f };
		g_engfuncs.pEfxAPI->CL_AllocDLight( pTemp->origin, rgb, 60, gpGlobals->time + 0.01f, 0, 0 );
	}

	if( pTemp->flags & FTENT_SMOKETRAIL )
	{
// FIXME: implement
//		g_engfuncs.pEfxAPI->R_RocketTrail( pTemp->oldorigin, pTemp->entity.origin, 1 );
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

void HUD_StudioEvent( const dstudioevent_t *event, edict_t *entity )
{
	float	pitch;
	Vector	pos;

	switch( event->event )
	{
	case 5001:
		// MullzeFlash at attachment 1
		HUD_MuzzleFlash( entity, 1, event->options );
		break;
	case 5011:
		// MullzeFlash at attachment 2
		HUD_MuzzleFlash( entity, 2, event->options );
		break;
	case 5021:
		// MullzeFlash at attachment 3
		HUD_MuzzleFlash( entity, 3, event->options );
		break;
	case 5031:
		// MullzeFlash at attachment 4
		HUD_MuzzleFlash( entity, 4, event->options );
		break;
	case 5002:
		// SparkEffect at attachment 1
		break;
	case 5004:		
		// Client side sound
		GET_ATTACHMENT( entity, 1, pos, NULL ); 
		CL_PlaySound( event->options, 1.0f, pos );
		break;
	case 5005:		
		// Client side sound with random pitch
		pitch = 85 + RANDOM_LONG( 0, 0x1F );
		GET_ATTACHMENT( entity, 1, pos, NULL ); 
		CL_PlaySound( event->options, RANDOM_FLOAT( 0.7f, 0.9f ), pos, pitch );
		break;
	case 5050:
		// Special event for displacer
		break;
	case 5060:
	          // eject shellEV_EjectShell( event, entity );
		break;
	default:
		break;
	}
}

void HUD_StudioFxTransform( edict_t *ent, float transform[4][4] )
{
	switch( ent->v.renderfx )
	{
	case kRenderFxDistort:
	case kRenderFxHologram:
		if(!RANDOM_LONG( 0, 49 ))
		{
			int	axis = RANDOM_LONG( 0, 1 );
			float	scale = RANDOM_FLOAT( 1, 1.484 );

			if( axis == 1 ) axis = 2; // choose between x & z
			transform[axis][0] *= scale;
			transform[axis][1] *= scale;
			transform[axis][2] *= scale;
		}
		else if(!RANDOM_LONG( 0, 49 ))
		{
			float offset;
			int axis = RANDOM_LONG( 0, 1 );
			if( axis == 1 ) axis = 2; // choose between x & z
			offset = RANDOM_FLOAT( -10, 10 );
			transform[RANDOM_LONG( 0, 2 )][3] += offset;
		}
		break;
	case kRenderFxExplode:
		float	scale;

		scale = 1.0f + (gHUD.m_flTime - ent->v.animtime) * 10.0;
		if( scale > 2 ) scale = 2; // don't blow up more than 200%
		
		transform[0][1] *= scale;
		transform[1][1] *= scale;
		transform[2][1] *= scale;
		break;
	}
}

/*
=================
CL_ExplosionParticles
=================
*/
void CL_ExplosionParticles( const Vector pos )
{
	cparticle_t	src;
	int		flags;

	if( !CVAR_GET_FLOAT( "cl_particles" ))
		return;

	flags = (PARTICLE_STRETCH|PARTICLE_BOUNCE|PARTICLE_FRICTION);

	for( int i = 0; i < 384; i++ )
	{
		src.origin.x = pos.x + RANDOM_LONG( -16, 16 );
		src.origin.y = pos.y + RANDOM_LONG( -16, 16 );
		src.origin.z = pos.z + RANDOM_LONG( -16, 16 );
		src.velocity.x = RANDOM_LONG( -256, 256 );
		src.velocity.y = RANDOM_LONG( -256, 256 );
		src.velocity.z = RANDOM_LONG( -256, 256 );
		src.accel.x = src.accel.y = 0;
		src.accel.z = -60 + RANDOM_FLOAT( -30, 30 );
		src.color = Vector( 1, 1, 1 );
		src.colorVelocity = Vector( 0, 0, 0 );
		src.alpha = 1.0;
		src.alphaVelocity = -3.0;
		src.radius = 0.5 + RANDOM_FLOAT( -0.2, 0.2 );
		src.radiusVelocity = 0;
		src.length = 8 + RANDOM_FLOAT( -4, 4 );
		src.lengthVelocity = 8 + RANDOM_FLOAT( -4, 4 );
		src.rotation = 0;
		src.bounceFactor = 0.2;

		if( !g_engfuncs.pEfxAPI->R_AllocParticle( &src, gHUD.m_hSparks, flags ))
			return;
	}

	// smoke
	flags = PARTICLE_VERTEXLIGHT;

	for( i = 0; i < 5; i++ )
	{
		src.origin.x = pos.x + RANDOM_FLOAT( -10, 10 );
		src.origin.y = pos.y + RANDOM_FLOAT( -10, 10 );
		src.origin.z = pos.z + RANDOM_FLOAT( -10, 10 );
		src.velocity.x = RANDOM_FLOAT( -10, 10 );
		src.velocity.y = RANDOM_FLOAT( -10, 10 );
		src.velocity.z = RANDOM_FLOAT( -10, 10 ) + RANDOM_FLOAT( -5, 5 ) + 25;
		src.accel = Vector( 0, 0, 0 );
		src.color = Vector( 0, 0, 0 );
		src.colorVelocity = Vector( 0.75, 0.75, 0.75 );
		src.alpha = 0.5;
		src.alphaVelocity = RANDOM_FLOAT( -0.1, -0.2 );
		src.radius = 30 + RANDOM_FLOAT( -15, 15 );
		src.radiusVelocity = 15 + RANDOM_FLOAT( -7.5, 7.5 );
		src.length = 1;
		src.lengthVelocity = 0;
		src.rotation = RANDOM_LONG( 0, 360 );

		if( !g_engfuncs.pEfxAPI->R_AllocParticle( &src, gHUD.m_hSmoke, flags ))
			return;
	}
}

/*
=================
CL_BubbleParticles
=================
*/
void CL_BubbleParticles( const Vector org, int count, float magnitude )
{
	cparticle_t	src;
	int		i;

	if( !CVAR_GET_FLOAT( "cl_particles" ))
		return;

	for( i = 0; i < count; i++ )
	{
		src.origin.x = org[0] + RANDOM_FLOAT( -magnitude, magnitude );
		src.origin.y = org[1] + RANDOM_FLOAT( -magnitude, magnitude );
		src.origin.z = org[2] + RANDOM_FLOAT( -magnitude, magnitude );
		src.velocity.x = RANDOM_FLOAT( -5, 5 );
		src.velocity.y = RANDOM_FLOAT( -5, 5 );
		src.velocity.z = RANDOM_FLOAT( -5, 5 ) + (25 + RANDOM_FLOAT( -5, 5 ));
		src.accel = Vector( 0, 0, 0 );
		src.color = Vector( 1, 1, 1 );
		src.colorVelocity = Vector( 0, 0, 0 );
		src.alpha = 1.0;
		src.alphaVelocity = -(0.4 + RANDOM_FLOAT( 0, 0.2 ));
		src.radius = 1 + RANDOM_FLOAT( -0.5, 0.5 );
		src.radiusVelocity = 0;
		src.length = 1;
		src.lengthVelocity = 0;
		src.rotation = 0;

		if( !g_engfuncs.pEfxAPI->R_AllocParticle( &src, gHUD.m_hBubble, PARTICLE_UNDERWATER ))
			return;
	}
}

/*
=================
CL_BulletParticles
=================
*/
void CL_SparkParticles( const Vector org, const Vector dir )
{
	cparticle_t	src;
	int		i, flags;

	// sparks
	flags = (PARTICLE_STRETCH|PARTICLE_BOUNCE|PARTICLE_FRICTION);

	for( i = 0; i < 16; i++ )
	{
		src.origin.x = org[0] + dir[0] * 2 + RANDOM_FLOAT( -1, 1 );
		src.origin.y = org[1] + dir[1] * 2 + RANDOM_FLOAT( -1, 1 );
		src.origin.z = org[2] + dir[2] * 2 + RANDOM_FLOAT( -1, 1 );
		src.velocity.x = dir[0] * 180 + RANDOM_FLOAT( -60, 60 );
		src.velocity.y = dir[1] * 180 + RANDOM_FLOAT( -60, 60 );
		src.velocity.z = dir[2] * 180 + RANDOM_FLOAT( -60, 60 );
		src.accel.x = src.accel.y = 0;
		src.accel.z = -120 + RANDOM_FLOAT( -60, 60 );
		src.color = Vector( 1.0, 1.0f, 1.0f );
		src.colorVelocity = Vector( 0, 0, 0 );
		src.alpha = 1.0;
		src.alphaVelocity = -8.0;
		src.radius = 0.4 + RANDOM_FLOAT( -0.2, 0.2 );
		src.radiusVelocity = 0;
		src.length = 8 + RANDOM_FLOAT( -4, 4 );
		src.lengthVelocity = 8 + RANDOM_FLOAT( -4, 4 );
		src.rotation = 0;
		src.bounceFactor = 0.2;

		if( !g_engfuncs.pEfxAPI->R_AllocParticle( &src, gHUD.m_hSparks, flags ))
			return;
	}
}

/*
=================
CL_RicochetSparks
=================
*/
void CL_RicochetSparks( const Vector org, float scale )
{
	cparticle_t	src;
	int		i, flags;

	// sparks
	flags = (PARTICLE_STRETCH|PARTICLE_BOUNCE|PARTICLE_FRICTION);

	for( i = 0; i < 16; i++ )
	{
		src.origin.x = org[0] + RANDOM_FLOAT( -1, 1 );
		src.origin.y = org[1] + RANDOM_FLOAT( -1, 1 );
		src.origin.z = org[2] + RANDOM_FLOAT( -1, 1 );
		src.velocity.x = RANDOM_FLOAT( -60, 60 );
		src.velocity.y = RANDOM_FLOAT( -60, 60 );
		src.velocity.z = RANDOM_FLOAT( -60, 60 );
		src.accel.x = src.accel.y = 0;
		src.accel.z = -120 + RANDOM_FLOAT( -60, 60 );
		src.color = Vector( 1.0, 1.0f, 1.0f );
		src.colorVelocity = Vector( 0, 0, 0 );
		src.alpha = 1.0;
		src.alphaVelocity = -8.0;
		src.radius = scale + RANDOM_FLOAT( -0.2, 0.2 );
		src.radiusVelocity = 0;
		src.length = scale + RANDOM_FLOAT( -0.2, 0.2 );
		src.lengthVelocity = scale + RANDOM_FLOAT( -0.2, 0.2 );
		src.rotation = 0;
		src.bounceFactor = 0.2;

		if( !g_engfuncs.pEfxAPI->R_AllocParticle( &src, gHUD.m_hSparks, flags ))
			return;
	}
}

void CL_SmokeParticles( const Vector pos, int count )
{
	cparticle_t	src;
	int		i, flags;

	if( !CVAR_GET_FLOAT( "cl_particles" ))
		return;

	// smoke
	flags = PARTICLE_VERTEXLIGHT;

	for( i = 0; i < count; i++ )
	{
		src.origin.x = pos.x + RANDOM_FLOAT( -10, 10 );
		src.origin.y = pos.y + RANDOM_FLOAT( -10, 10 );
		src.origin.z = pos.z + RANDOM_FLOAT( -10, 10 );
		src.velocity.x = RANDOM_FLOAT( -10, 10 );
		src.velocity.y = RANDOM_FLOAT( -10, 10 );
		src.velocity.z = RANDOM_FLOAT( -10, 10 ) + RANDOM_FLOAT( -5, 5 ) + 25;
		src.accel = Vector( 0, 0, 0 );
		src.color = Vector( 0, 0, 0 );
		src.colorVelocity = Vector( 0.75, 0.75, 0.75 );
		src.alpha = 0.5;
		src.alphaVelocity = RANDOM_FLOAT( -0.1, -0.2 );
		src.radius = 30 + RANDOM_FLOAT( -15, 15 );
		src.radiusVelocity = 15 + RANDOM_FLOAT( -7.5, 7.5 );
		src.length = 1;
		src.lengthVelocity = 0;
		src.rotation = RANDOM_LONG( 0, 360 );

		if( !g_engfuncs.pEfxAPI->R_AllocParticle( &src, gHUD.m_hSmoke, flags ))
			return;
	}
}

/*
=================
CL_BulletParticles
=================
*/
void CL_BulletParticles( const Vector org, const Vector dir )
{
	cparticle_t	src;
	int		flags;
	int		i, cnt, count;

	if( !CVAR_GET_FLOAT( "cl_particles" ))
		return;

	count = RANDOM_LONG( 3, 8 );
	cnt = POINT_CONTENTS( org );

	if( cnt == CONTENTS_WATER )
	{
		CL_BubbleParticles( org, count, 0 );
		return;
	}

	// sparks
	flags = (PARTICLE_STRETCH|PARTICLE_BOUNCE|PARTICLE_FRICTION);

	for( i = 0; i < count; i++ )
	{
		src.origin.x = org[0] + dir[0] * 2 + RANDOM_FLOAT( -1, 1 );
		src.origin.y = org[1] + dir[1] * 2 + RANDOM_FLOAT( -1, 1 );
		src.origin.z = org[2] + dir[2] * 2 + RANDOM_FLOAT( -1, 1 );
		src.velocity.x = dir[0] * 180 + RANDOM_FLOAT( -60, 60 );
		src.velocity.y = dir[1] * 180 + RANDOM_FLOAT( -60, 60 );
		src.velocity.z = dir[2] * 180 + RANDOM_FLOAT( -60, 60 );
		src.accel.x = src.accel.y = 0;
		src.accel.z = -120 + RANDOM_FLOAT( -60, 60 );
		src.color = Vector( 1.0, 1.0f, 1.0f );
		src.colorVelocity = Vector( 0, 0, 0 );
		src.alpha = 1.0;
		src.alphaVelocity = -8.0;
		src.radius = 0.4 + RANDOM_FLOAT( -0.2, 0.2 );
		src.radiusVelocity = 0;
		src.length = 8 + RANDOM_FLOAT( -4, 4 );
		src.lengthVelocity = 8 + RANDOM_FLOAT( -4, 4 );
		src.rotation = 0;
		src.bounceFactor = 0.2;

		if( !g_engfuncs.pEfxAPI->R_AllocParticle( &src, gHUD.m_hSparks, flags ))
			return;
	}

	// smoke
	flags = PARTICLE_VERTEXLIGHT;

	for( i = 0; i < 3; i++ )
	{
		src.origin.x = org[0] + dir[0] * 5 + RANDOM_FLOAT( -1, 1 );
		src.origin.y = org[1] + dir[1] * 5 + RANDOM_FLOAT( -1, 1 );
		src.origin.z = org[2] + dir[2] * 5 + RANDOM_FLOAT( -1, 1 );
		src.velocity.x = RANDOM_FLOAT( -2.5, 2.5 );
		src.velocity.y = RANDOM_FLOAT( -2.5, 2.5 );
		src.velocity.z = RANDOM_FLOAT( -2.5, 2.5 ) + (25 + RANDOM_FLOAT( -5, 5 ));
		src.accel = Vector( 0, 0, 0 );
		src.color = Vector( 0.4, 0.4, 0.4 );
		src.colorVelocity = Vector( 0.2, 0.2, 0.2 );
		src.alpha = 0.5;
		src.alphaVelocity = -(0.4 + RANDOM_FLOAT( 0, 0.2 ));
		src.radius = 3 + RANDOM_FLOAT( -1.5, 1.5 );
		src.radiusVelocity = 5 + RANDOM_FLOAT( -2.5, 2.5 );
		src.length = 1;
		src.lengthVelocity = 0;
		src.rotation = RANDOM_LONG( 0, 360 );

		if( !g_engfuncs.pEfxAPI->R_AllocParticle( &src, gHUD.m_hSmoke, flags ))
			return;
	}
}

/*
=================
CL_TeleportParticles

creates a particle box
=================
*/
void CL_TeleportParticles( const Vector org )
{
	cparticle_t	src;
	vec3_t		dir;
	float		vel, color;
	int		x, y, z;

	if( !CVAR_GET_FLOAT( "cl_particles" ))
		return;

	for( x = -16; x <= 16; x += 4 )
	{
		for( y = -16; y <= 16; y += 4 )
		{
			for( z = -16; z <= 32; z += 4 )
			{
				dir = Vector( y*8, x*8, z*8 ).Normalize();

				vel = 50 + RANDOM_LONG( 0, 64 );
				color = RANDOM_FLOAT( 0.1, 0.3 );
				src.origin.x = org[0] + x + RANDOM_LONG( 0, 4 );
				src.origin.y = org[1] + y + RANDOM_LONG( 0, 4 );
				src.origin.z = org[2] + z + RANDOM_LONG( 0, 4 );
				src.velocity[0] = dir[0] * vel;
				src.velocity[1] = dir[1] * vel;
				src.velocity[2] = dir[2] * vel;
				src.accel[0] = 0;
				src.accel[1] = 0;
				src.accel[2] = -40;
				src.color = Vector( color, color, color );
				src.colorVelocity = Vector( 0, 0, 0 );
				src.alpha = 1.0;
				src.alphaVelocity = -1.0 / (0.3 + RANDOM_LONG( 0, 0.16 ));
				src.radius = 2;
				src.radiusVelocity = 0;
				src.length = 1;
				src.lengthVelocity = 0;
				src.rotation = 0;

				if( !g_engfuncs.pEfxAPI->R_AllocParticle( &src, gHUD.m_hGlowParticle, 0 ))
					return;

			}
		}
	}
}

void CL_PlaceDecal( Vector pos, Vector dir, float scale, HSPRITE hDecal )
{
	float	rgba[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	int	flags = DECAL_FADEALPHA;

	g_engfuncs.pEfxAPI->R_SetDecal( pos, dir, rgba, RANDOM_LONG( 0, 360 ), scale, hDecal, flags );
}

void CL_AllocDLight( Vector pos, float radius, float time, int flags )
{
	float	rgb[3] = { 1.0f, 1.0f, 1.0f };

	if( radius <= 0 ) return;
	g_engfuncs.pEfxAPI->CL_AllocDLight( pos, rgb, radius, time, flags, 0 );
}

void HUD_ParseTempEntity( void )
{
	Vector	pos, dir, color;
	float	time, radius, decay;
	int	flags, scale;

	switch( READ_BYTE() )
	{
	case TE_GUNSHOT:
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		CL_BulletParticles( pos, Vector( 0, 0, -1 ));
		break;
	case TE_GUNSHOTDECAL:
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		dir = READ_DIR();
		READ_SHORT(); // FIXME: skip entindex
		CL_BulletParticles( pos, dir );
		CL_PlaceDecal( pos, dir, 2, g_engfuncs.pEfxAPI->CL_DecalIndex( READ_BYTE() ));
		break;
	case TE_DECAL:
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		g_engfuncs.pEfxAPI->CL_FindExplosionPlane( pos, 10, dir );
		CL_PlaceDecal( pos, dir, 2, g_engfuncs.pEfxAPI->CL_DecalIndex( READ_BYTE() ));
		READ_SHORT(); // FIXME: skip entindex
		break;	
	case TE_EXPLOSION:
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		READ_SHORT(); // FIXME: use sprite index as shader index
		scale = READ_BYTE();
		READ_BYTE(); // FIXME: use framerate for shader
		flags = READ_BYTE();

		g_engfuncs.pEfxAPI->CL_FindExplosionPlane( pos, scale, dir );
		if(!(flags & TE_EXPLFLAG_NOPARTICLES )) CL_ExplosionParticles( pos );
		if( RANDOM_LONG( 0, 1 ))
			CL_PlaceDecal( pos, dir, scale, g_engfuncs.pEfxAPI->CL_DecalIndexFromName( "{scorch1" ));
		else CL_PlaceDecal( pos, dir, scale, g_engfuncs.pEfxAPI->CL_DecalIndexFromName( "{scorch2" )); 
		if(!(flags & TE_EXPLFLAG_NODLIGHTS )) CL_AllocDLight( pos, 250.0f, 0.8f, DLIGHT_FADE );
		if(!(flags & TE_EXPLFLAG_NOSOUND )) CL_PlaySound( "weapons/explode3.wav", 1.0f, pos );
		break;
	case TE_SPARKS:
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		g_engfuncs.pEfxAPI->CL_FindExplosionPlane( pos, 1.0f, dir );
		CL_SparkParticles( pos, dir );
		break;
	case TE_ARMOR_RICOCHET:
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		radius = READ_BYTE() / 10.0f;
		CL_RicochetSparks( pos, radius );
		break;
	case TE_SMOKE:
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		READ_SHORT(); // FIXME: use sprite index as shader index
		scale = READ_BYTE();
		READ_BYTE();  // FIMXE: use framerate
		CL_SmokeParticles( pos, scale );
		break;
	case TE_TELEPORT:
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		break;
	case TE_DLIGHT:
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		radius = (float)READ_BYTE() * 10.0f;
		color.x = (float)READ_BYTE() / 255.0f;
		color.y = (float)READ_BYTE() / 255.0f;
		color.z = (float)READ_BYTE() / 255.0f;
		time = (float)READ_BYTE() * 0.1f;
		decay = (float)READ_BYTE() * 0.1f;
		g_engfuncs.pEfxAPI->CL_AllocDLight( pos, color, radius, time, 0, 0 );
		break;
	}
}