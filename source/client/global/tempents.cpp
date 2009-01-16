//=======================================================================
//			Copyright XashXT Group 2008 ©
//		tempents.cpp - client side entity management functions
//=======================================================================

#include "extdll.h"
#include "hud_iface.h"
#include "studio_event.h"
#include "effects_api.h"
#include "te_message.h"
#include "hud.h"

void HUD_CreateEntities( void )
{
}

void HUD_StudioEvent( const dstudioevent_t *event, edict_t *entity )
{
	float	pitch;

	switch( event->event )
	{
	case 5001:
		// MullzeFlash at attachment 0
		break;
	case 5011:
		// MullzeFlash at attachment 1
		break;
	case 5021:
		// MullzeFlash at attachment 2
		break;
	case 5031:
		// MullzeFlash at attachment 3
		break;
	case 5002:
		// SparkEffect at attachment 0
		break;
	case 5004:		
		// Client side sound
		CL_PlaySound( event->options, 1.0f, entity->v.attachment[0] );
		ALERT( at_console, "CL_PlaySound( %s )\n", event->options );
		break;
	case 5005:		
		// Client side sound with random pitch
		pitch = 85 + RANDOM_LONG( 0, 0x1F );
		ALERT( at_console, "CL_PlaySound( %s )\n", event->options );
		CL_PlaySound( event->options, RANDOM_FLOAT( 0.7f, 0.9f ), entity->v.attachment[0], pitch );
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

		if( !g_engfuncs.pEfxAPI->R_AllocParticle( &src, gHUD.m_hSparks, flags ))
			return;
	}
}

void CL_PlaceDecal( Vector pos, Vector dir, float scale, HSPRITE hDecal )
{
	float	rgba[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	int	flags = DECAL_FADE;

	g_engfuncs.pEfxAPI->R_SetDecal( pos, dir, rgba, RANDOM_LONG( 0, 360 ), scale, hDecal, flags );
}

void CL_AllocDLight( Vector pos, float radius, float decay, float time )
{
	float	rgb[3] = { 1.0f, 1.0f, 1.0f };

	g_engfuncs.pEfxAPI->CL_AllocDLight( pos, rgb, radius, decay, time, 0 );
}

void HUD_ParseTempEntity( void )
{
	Vector	pos, dir;
	int	flags;

	switch( READ_BYTE() )
	{
	case TE_EXPLOSION:
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		READ_SHORT(); // FIXME: use sprite index as shader index
		int scale = READ_BYTE();
		READ_BYTE(); // FIXME: use framerate for shader
		flags = READ_BYTE();

		g_engfuncs.pEfxAPI->CL_FindExplosionPlane( pos, scale, dir );
		if(!(flags & TE_EXPLFLAG_NOPARTICLES )) CL_ExplosionParticles( pos );
		CL_PlaceDecal( pos, dir, scale, SPR_Load( "decals/{scorch" ));
		if(!(flags & TE_EXPLFLAG_NODLIGHTS )) CL_AllocDLight( pos, 250.0f, 0.28f, 0.8f );
		if(!(flags & TE_EXPLFLAG_NOSOUND )) CL_PlaySound( "weapons/explosde3.wav", 1.0f, pos );
		break;
	}
}