//=======================================================================
//			Copyright XashXT Group 2008 ©
//		tempents.cpp - client side entity management functions
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "studio_event.h"
#include "effects_api.h"
#include "pm_movevars.h"
#include "r_tempents.h"
#include "ev_hldm.h"
#include "r_beams.h"
#include "hud.h"

#define MAX_CHANNELS		4	// hl1 and hl2 supported 4 channels for messges

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

void HUD_AddClientMessage( void )
{
	static client_textmessage_t	gTextMsg[MAX_CHANNELS], *msg;
	static char 		text[MAX_CHANNELS][512];
	static int		msgindex = 0;

	int channel = READ_BYTE(); // channel

	if( channel <= 0 || channel > ( MAX_CHANNELS - 1 ))
	{
		// invalid channel specified, use internal counter		
		if( channel != 0 ) ALERT( at_warning, "HUD_Message: invalid channel %i\n", channel );
		channel = msgindex;
		msgindex = (msgindex + 1) & (MAX_CHANNELS - 1);
	}	

	// grab message channel
	msg = &gTextMsg[channel];
	msg->pMessage = text[channel];

	ASSERT( msg != NULL );

	msg->pName = "Text Message";
	msg->x = (float)(READ_SHORT() / 8192.0f);
	msg->y = (float)(READ_SHORT() / 8192.0f);
	msg->effect = READ_BYTE();
	msg->r1 = READ_BYTE();
	msg->g1 = READ_BYTE();
	msg->b1 = READ_BYTE();
	msg->a1 = READ_BYTE();
	msg->r2 = READ_BYTE();
	msg->g2 = READ_BYTE();
	msg->b2 = READ_BYTE();
	msg->a2 = READ_BYTE();
	msg->fadein = (float)( READ_SHORT() / 256.0f );
	msg->fadeout = (float)( READ_SHORT() / 256.0f );
	msg->holdtime = (float)( READ_SHORT() / 256.0f );

	if( msg->effect == 2 )
		msg->fxtime = (float)( READ_SHORT() / 256.0f );
	else msg->fxtime = 0.0f;

	strcpy( (char *)msg->pMessage, READ_STRING()); 		
	gHUD.m_Message.MessageAdd( msg );
}

/*
---------------
TE_ParseBeamFollow

Create a line of decaying beam segments until entity stops moving
---------------
*/
void TE_ParseBeamFollow( void )
{
	int entityIndex, modelIndex;
	float life, width, brightness;
	Vector color;

	entityIndex = READ_SHORT();
	modelIndex = READ_SHORT();
	life = (float)(READ_BYTE() * 0.1f);
	width = (float)READ_BYTE();

	// parse color
	color.x = READ_BYTE();
	color.y = READ_BYTE();
	color.z = READ_BYTE();
	brightness = READ_BYTE();

	g_pViewRenderBeams->CreateBeamFollow( entityIndex, modelIndex, life, width, width, 0.0f, color.x, color.y, color.z, brightness );
}

/*
---------------
TE_ParseBeamPoints

Creates a beam between two points
---------------
*/
void TE_ParseBeamPoints( void )
{
	Vector vecStart, vecEnd, vecColor;
	int modelIndex, startFrame;
	float frameRate, life, width;
	float brightness, noise, speed;

	// beam position	
	vecStart.x = READ_COORD();
	vecStart.y = READ_COORD();
	vecStart.z = READ_COORD();
	vecEnd.x = READ_COORD();
	vecEnd.y = READ_COORD();
	vecEnd.z = READ_COORD();

	// beam info
	modelIndex = READ_SHORT();
	startFrame = READ_BYTE();
	frameRate = (float)(READ_BYTE());
	life = (float)(READ_BYTE() * 0.1f);
	width = (float)(READ_BYTE() * 0.1f);
	noise = (float)(READ_BYTE() * 0.1f);

	// renderinfo
	vecColor.x = (float)READ_BYTE();
	vecColor.y = (float)READ_BYTE();
	vecColor.z = (float)READ_BYTE();
	brightness = (float)READ_BYTE();
	speed = (float)(READ_BYTE() * 0.1f);

	g_pViewRenderBeams->CreateBeamPoints( vecStart, vecEnd, modelIndex, life, width, noise, brightness, speed,
	startFrame, frameRate, vecColor.x, vecColor.y, vecColor.z );
}

/*
---------------
TE_ParseBeamEntPoint

Creates a beam between two points
---------------
*/
void TE_ParseBeamEntPoint( void )
{
	Vector vecEnd, vecColor;
	int entityIndex, modelIndex, startFrame;
	float frameRate, life, width;
	float brightness, noise, speed;

	// beam position	
	entityIndex = READ_SHORT();
	vecEnd.x = READ_COORD();
	vecEnd.y = READ_COORD();
	vecEnd.z = READ_COORD();

	// beam info
	modelIndex = READ_SHORT();
	startFrame = READ_BYTE();
	frameRate = (float)(READ_BYTE());
	life = (float)(READ_BYTE() * 0.1f);
	width = (float)(READ_BYTE() * 0.1f);
	noise = (float)(READ_BYTE() * 0.1f);

	// renderinfo
	vecColor.x = (float)READ_BYTE();
	vecColor.y = (float)READ_BYTE();
	vecColor.z = (float)READ_BYTE();
	brightness = (float)READ_BYTE();
	speed = (float)(READ_BYTE() * 0.1f);

	g_pViewRenderBeams->CreateBeamEntPoint( entityIndex, vecEnd, modelIndex, life, width, noise, brightness,
	speed, startFrame, frameRate, vecColor.x, vecColor.y, vecColor.z );
}

/*
---------------
TE_ParseGunShot

Particle effect plus ricochet sound
---------------
*/
void TE_ParseGunShot( void )
{
	char soundpath[32];
	Vector pos;

	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();

	CL_BulletParticles( pos, Vector( 0, 0, -1 ));

	if( RANDOM_LONG( 0, 32 ) <= 8 ) // 25% chanse
	{
		sprintf( soundpath, "weapons/ric%i.wav", RANDOM_LONG( 1, 5 ));
		CL_PlaySound( soundpath, RANDOM_FLOAT( 0.7f, 0.9f ), pos, RANDOM_FLOAT( 95.0f, 105.0f ));
	}
}

/*
---------------
TE_ParseExplosion

Creates additive sprite, 2 dynamic lights, flickering particles, explosion sound, move vertically 8 pps
---------------
*/
void TE_ParseExplosion( void )
{
	Vector dir, pos, pos2;
	float scale, frameRate;
	int flags, spriteIndex;
	TEMPENTITY *pTemp;

	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();
	
	spriteIndex = READ_SHORT();
	scale = (float)(READ_BYTE() * 0.1f);
	frameRate = READ_BYTE();
	flags = READ_BYTE();

	g_engfuncs.pEfxAPI->CL_FindExplosionPlane( pos, scale, dir );
	pos2 = pos + (dir * scale);

	if( scale != 0.0f )
	{
		// create explosion sprite
		pTemp = g_pTempEnts->DefaultSprite( pos2, spriteIndex, frameRate );
		g_pTempEnts->Sprite_Explode( pTemp, scale, flags );

		if( !( flags & TE_EXPLFLAG_NODLIGHTS ))
		{
			g_pTempEnts->AllocDLight( pos2, 250, 250, 150, 200, 0.01f, 0 ); // big flash
			g_pTempEnts->AllocDLight( pos2, 255, 190, 40, 150, 1.0f, DLIGHT_FADE );// red glow
		}
	}

	if(!( flags & TE_EXPLFLAG_NOPARTICLES ))
		CL_ExplosionParticles( pos );
	if( RANDOM_LONG( 0, 1 ))
		g_pTempEnts->PlaceDecal( pos, dir, scale, g_engfuncs.pEfxAPI->CL_DecalIndexFromName( "{scorch1" ));
	else g_pTempEnts->PlaceDecal( pos, dir, scale, g_engfuncs.pEfxAPI->CL_DecalIndexFromName( "{scorch2" )); 

	if( !( flags & TE_EXPLFLAG_NOSOUND ))
	{
		CL_PlaySound( "weapons/explode3.wav", 1.0f, pos2 );
	}
}

/*
---------------
TE_ParseSmoke

Creates alphablend sprite, move vertically 30 pps
---------------
*/
void TE_ParseSmoke( void )
{
	TEMPENTITY *pTemp;
	float scale, framerate;
	int modelIndex;
	Vector pos;

	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();
	modelIndex = READ_SHORT();
	scale = (float)(READ_BYTE() * 0.1f);
	framerate = (float)READ_BYTE();

	// Half-Life style smoke
	// create smoke sprite
	pTemp = g_pTempEnts->DefaultSprite( pos, modelIndex, framerate );
	g_pTempEnts->Sprite_Smoke( pTemp, scale );
}

/*
---------------
TE_ParseTracer

Creates tracer effect from point to point
---------------
*/
void TE_ParseTracer( void )
{
	Vector start, end;

	start.x = READ_COORD();	// tracer start
	start.y = READ_COORD();
	start.z = READ_COORD();

	end.x = READ_COORD();	// tracer end
	end.y = READ_COORD();
	end.z = READ_COORD();

	EV_CreateTracer( start, end );
}

/*
---------------
TE_ParseLighting

version of TE_BEAMPOINTS with simplified parameters
---------------
*/
void TE_ParseLighting( void )
{
	Vector start, end;
	float life, width, noise;
	int modelIndex;

	start.x = READ_COORD();
	start.y = READ_COORD();
	start.z = READ_COORD();

	end.x = READ_COORD();
	end.y = READ_COORD();
	end.z = READ_COORD();

	life = (float)(READ_BYTE() * 0.1f);
	width = (float)(READ_BYTE() * 0.1f);
	noise = (float)(READ_BYTE() * 0.1f);
	modelIndex = READ_SHORT();

	g_pViewRenderBeams->CreateBeamPoints( start, end, modelIndex, life, width, noise, 255, 1.0f, 0, 0, 255, 255, 255 );
}

/*
---------------
TE_ParseBeamEnts

Creates a beam between origins of two entities
---------------
*/
void TE_ParseBeamEnts( void )
{
	int modelIndex, startEntity, endEntity, startFrame;
	float life, width, noise, framerate, brightness, speed;
	Vector vecColor;

	startEntity = READ_SHORT();
	endEntity = READ_SHORT();
	modelIndex = READ_SHORT();

	startFrame = READ_BYTE();
	framerate = (float)(READ_BYTE() * 0.1f);
	life = (float)(READ_BYTE() * 0.1f);
	width = (float)(READ_BYTE() * 0.1f);
	noise = (float)(READ_BYTE() * 0.1f);

	// renderinfo
	vecColor.x = (float)READ_BYTE();
	vecColor.y = (float)READ_BYTE();
	vecColor.z = (float)READ_BYTE();
	brightness = (float)READ_BYTE();
	speed = (float)(READ_BYTE() * 0.1f);

	g_pViewRenderBeams->CreateBeamEnts( startEntity, endEntity, modelIndex, life, width, noise, brightness,
	speed, startFrame, framerate, vecColor.x, vecColor.y, vecColor.z );
}

/*
---------------
TE_ParseSparks

Creates a 8 random tracers with gravity, ricochet sprite
---------------
*/
void TE_ParseSparks( void )
{
	Vector pos, dir;

	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();

	g_engfuncs.pEfxAPI->CL_FindExplosionPlane( pos, 1.0f, dir );
	CL_SparkParticles( pos, dir );
}

/*
---------------
TE_ParseTeleport

Creates a Quake1 teleport splash
---------------
*/
void TE_ParseTeleport( void )
{
	Vector pos;
	
	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();

	CL_TeleportParticles( pos );
}

/*
---------------
TE_ParseBSPDecal

Creates a decal from the .BSP file 
---------------
*/
void TE_ParseBSPDecal( void )
{
	Vector pos;
	int decalIndex, entityIndex, modelIndex;
	edict_t *pEntity;
	
	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();

	decalIndex = READ_SHORT();
	entityIndex = READ_SHORT();

	if( entityIndex != 0 )
	{
		modelIndex = READ_SHORT();
	}

	pEntity = GetEntityByIndex( entityIndex );
	g_pTempEnts->PlaceDecal( pos, pEntity, g_engfuncs.pEfxAPI->CL_DecalIndex( decalIndex ));
}

/*
---------------
TE_ParseSpriteTrail

Creates a line of moving glow sprites with gravity, fadeout, and collisions
---------------
*/
void TE_ParseSpriteTrail( void )
{
	Vector start, end;
	float life, scale, vel, random;
	int spriteIndex, count;

	start.x = READ_COORD();
	start.y = READ_COORD();
	start.z = READ_COORD();

	end.x = READ_COORD();
	end.y = READ_COORD();
	end.z = READ_COORD();

	spriteIndex = READ_SHORT();
	count = READ_BYTE();
	life = (float)(READ_BYTE() * 0.1f);
	scale = (float)(READ_BYTE() * 0.1f);
	vel = (float)READ_BYTE();
	random = (float)READ_BYTE();

	g_pTempEnts->Sprite_Trail( TE_SPRITETRAIL, start, end, spriteIndex, count, life, scale, random, 255, vel );
}

/*
---------------
TE_ParseSprite

Creates a additive sprite, plays 1 cycle
---------------
*/
void TE_ParseSprite( void )
{
	Vector pos;
	TEMPENTITY *pTemp;
	float scale, brightness;
	int spriteIndex;
	
	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();

	spriteIndex = READ_SHORT();
	scale = (float)(READ_BYTE() * 0.1f);
	brightness = (float)READ_BYTE();

	pTemp = g_pTempEnts->DefaultSprite( pos, spriteIndex, 0 );

	if( pTemp )
	{
		pTemp->m_flSpriteScale = scale;
		pTemp->startAlpha = brightness;
		pTemp->renderAmt = brightness;
	}
}

/*
---------------
TE_ParseBeamRing

Generic function to parse various beam rings
---------------
*/
void TE_ParseBeamRing( int type )
{
	Vector start, end, vecColor;
	float life, framerate, width, noise, speed, brightness;
	int spriteIndex, startFrame;

	start.x = READ_COORD();
	start.y = READ_COORD();
	start.z = READ_COORD();

	end.x = READ_COORD();
	end.y = READ_COORD();
	end.z = READ_COORD();

	spriteIndex = READ_SHORT();
	startFrame = READ_BYTE();
	framerate = (float)(READ_BYTE() * 0.1f);
	life = (float)(READ_BYTE() * 0.1f);
	width = (float)(READ_BYTE());
	noise = (float)(READ_BYTE() * 0.1f);

	// renderinfo
	vecColor.x = (float)READ_BYTE();
	vecColor.y = (float)READ_BYTE();
	vecColor.z = (float)READ_BYTE();
	brightness = (float)READ_BYTE();
	speed = (float)(READ_BYTE() * 0.1f);

	g_pViewRenderBeams->CreateBeamCirclePoints( type, start, end, spriteIndex, life, width, width, 0.0f,
	noise, brightness, speed, startFrame, framerate, vecColor.x, vecColor.y, vecColor.z );
}

/*
---------------
TE_ParseKillBeam

Kill all beams attached to entity
---------------
*/
void TE_ParseKillBeam( void )
{
	edict_t *pEntity = GetEntityByIndex( READ_SHORT() );

	g_pViewRenderBeams->KillDeadBeams( pEntity );
}

void HUD_ParseTempEntity( void )
{
	char soundpath[32];
	Vector pos, pos2, dir, size, color;
	float time, random, radius, scale, decay;
	int flags, modelIndex, modelIndex2, soundType, count, iColor;

	switch( READ_BYTE() )
	{
	case TE_BEAMPOINTS:
		TE_ParseBeamPoints();
		break;
	case TE_BEAMENTPOINT:
		TE_ParseBeamEntPoint();
		break;
	case TE_GUNSHOT:
		TE_ParseGunShot();
		break;
	case TE_EXPLOSION:
		TE_ParseExplosion();
		break;
	case TE_TAREXPLOSION:
		// FIXME: implement
		break;
	case TE_SMOKE:
		TE_ParseSmoke();
		break;
	case TE_TRACER:
		TE_ParseTracer();
		break;
	case TE_LIGHTNING:
		TE_ParseLighting();
		break;
	case TE_BEAMENTS:
		TE_ParseBeamEnts();
		break;
	case TE_SPARKS:
		TE_ParseSparks();
		break;
	case TE_LAVASPLASH:
		// FIXME: implement
		break;
	case TE_TELEPORT:
		TE_ParseTeleport();
		break;
	case TE_EXPLOSION2:
		// FIXME: implement
		break;
	case TE_BSPDECAL:
		TE_ParseBSPDecal();
		break;
	case TE_IMPLOSION:
		// FIXME: implement
		break;
	case TE_SPRITETRAIL:
		TE_ParseSpriteTrail();
		break;
	case TE_BEAM: break; // obsolete
	case TE_SPRITE:
		TE_ParseSprite();
		break;
	case TE_BEAMSPRITE:
		// FIXME: implement
		break;
	case TE_BEAMTORUS:
		TE_ParseBeamRing( TE_BEAMTORUS );
		break;
	case TE_BEAMDISK:
		TE_ParseBeamRing( TE_BEAMDISK );
		break;
	case TE_BEAMCYLINDER:
		TE_ParseBeamRing( TE_BEAMCYLINDER );
		break;
	case TE_BEAMFOLLOW:
		TE_ParseBeamFollow();
		break;
	case TE_GLOWSPRITE:
		// FIXME: implement
		break;
	case TE_BEAMRING:
		break;
	case TE_KILLBEAM:
		TE_ParseKillBeam();
		break;
	case TE_GUNSHOTDECAL:
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		dir = READ_DIR();
		READ_SHORT(); // FIXME: skip entindex
		CL_BulletParticles( pos, dir );
		g_pTempEnts->PlaceDecal( pos, dir, 2, g_engfuncs.pEfxAPI->CL_DecalIndex( READ_BYTE() ));

		if( RANDOM_LONG( 0, 32 ) <= 8 ) // 25% chanse
		{
			sprintf( soundpath, "weapons/ric%i.wav", RANDOM_LONG( 1, 5 ));
			CL_PlaySound( soundpath, RANDOM_FLOAT( 0.7f, 0.9f ), pos, RANDOM_FLOAT( 95.0f, 105.0f ));
		}
		break;
	case TE_DECAL:
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		g_engfuncs.pEfxAPI->CL_FindExplosionPlane( pos, 10, dir );
		g_pTempEnts->PlaceDecal( pos, dir, 2, g_engfuncs.pEfxAPI->CL_DecalIndex( READ_BYTE() ));
		READ_SHORT(); // FIXME: skip entindex
		break;	
	case TE_ARMOR_RICOCHET:
		pos.x = READ_COORD();
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		radius = READ_BYTE() / 10.0f;
		CL_RicochetSparks( pos, radius );
		break;
	case TE_ELIGHT:
		READ_SHORT(); // skip attachment
		// FIXME: need to match with dlight message
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
	case TE_MODEL:
		pos.x = READ_COORD();	// tracer start
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		dir.x = READ_COORD();	// velocity
		dir.y = READ_COORD();
		dir.z = READ_COORD();
		pos2 = Vector( 0.0f, READ_ANGLE(), 0.0f );
		modelIndex = READ_SHORT();
		soundType = READ_BYTE();
		decay = (float)(READ_BYTE() * 0.1f);
		g_pTempEnts->TempModel( pos, dir, pos2, decay, modelIndex, soundType );
		break;
	case TE_BLOODSPRITE:
		pos.x = READ_COORD();	// sprite pos
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		modelIndex = READ_SHORT();
		modelIndex2 =READ_SHORT();
		iColor = READ_BYTE();
		scale = READ_BYTE();	// scale
		g_pTempEnts->BloodSprite( pos, iColor, modelIndex, modelIndex2, scale );
		break;
	case TE_BREAKMODEL:
		pos.x = READ_COORD();	// breakmodel center
		pos.y = READ_COORD();
		pos.z = READ_COORD();
		size.x = READ_COORD();	// bmodel size
		size.y = READ_COORD();
		size.z = READ_COORD();
		dir.x = READ_COORD();	// damage direction with velocity
		dir.y = READ_COORD();
		dir.z = READ_COORD();
		random = (float)(READ_BYTE());
		modelIndex = READ_SHORT();	// shard model
		count = READ_BYTE();	// # of shards ( 0 - autodetect by size )
		decay = (float)(READ_BYTE() * 0.1f);
		flags = READ_BYTE();	// material flags
		g_pTempEnts->BreakModel( pos, size, dir, random, decay, count, modelIndex, flags );
		break;
	case TE_TEXTMESSAGE:
		HUD_AddClientMessage();
		break;
	}
}