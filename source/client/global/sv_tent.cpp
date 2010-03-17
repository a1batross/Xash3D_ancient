//=======================================================================
//			Copyright XashXT Group 2008 ©
//		tempents.cpp - client side entity management functions
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "studio_event.h"
#include "effects_api.h"
#include "pm_movevars.h"
#include "r_particle.h"
#include "r_tempents.h"
#include "ev_hldm.h"
#include "r_beams.h"
#include "hud.h"

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

	g_engfuncs.pEfxAPI->R_RunParticleEffect( pos, g_vecZero, 0, 20 );

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
	float scale, magnitude, frameRate;
	int flags, spriteIndex;
	TEMPENTITY *pTemp;

	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();
	
	spriteIndex = READ_SHORT();
	magnitude = (float)(READ_BYTE() * 0.1f);
	frameRate = READ_BYTE();
	flags = READ_BYTE();

	UTIL_GetForceDirection( pos, magnitude, &dir, &scale );
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
		g_engfuncs.pEfxAPI->R_RunParticleEffect( pos, g_vecZero, 224, 200 );

	char	szDecal[32];

	sprintf( szDecal, "{scorch%i", RANDOM_LONG( 1, 3 ));
	g_pTempEnts->PlaceDecal( pos2, scale * 24, szDecal );

	if( !( flags & TE_EXPLFLAG_NOSOUND ))
	{
		CL_PlaySound( "weapons/explode3.wav", 1.0f, pos2 );
	}
}

/*
---------------
TE_ParseTarExplosion

Creates a Quake1 "tarbaby" explosion with sound
---------------
*/
void TE_ParseTarExplosion( void )
{
	Vector pos;
	
	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();

	g_engfuncs.pEfxAPI->R_BlobExplosion( pos );
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
	Vector pos;

	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();

	g_pParticles->SparkParticles( pos, g_vecZero );
}

/*
---------------
TE_ParseLavaSplash

Creates a Quake1 lava splash
---------------
*/
void TE_ParseLavaSplash( void )
{
	Vector pos;
	
	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();

	g_engfuncs.pEfxAPI->R_LavaSplash( pos );
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

	g_engfuncs.pEfxAPI->R_TeleportSplash( pos );
}

/*
---------------
TE_ParseExplosion2

Creates a Quake1 colormaped (base palette) particle explosion with sound
---------------
*/
void TE_ParseExplosion2( void )
{
	Vector pos;
	int colorIndex, numColors;
	
	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();

	colorIndex = READ_BYTE();
	numColors = READ_BYTE();

	g_engfuncs.pEfxAPI->R_ParticleExplosion2( pos, colorIndex, numColors );
	g_pTempEnts->AllocDLight( pos, 350, 0.5, DLIGHT_FADE );
	CL_PlaySound( "weapons/explode3.wav", 1.0f, pos );
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
	g_pTempEnts->PlaceDecal( pos, 5.0f, decalIndex );
}

/*
---------------
TE_ParseImplosion

Creates a tracers moving toward a point
---------------
*/
void TE_ParseImplosion( void )
{
	Vector pos;
	
	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();

	// FIXME: create tracers moving toward a point
	CL_PlaySound( "shambler/sattck1.wav", 1.0f, pos );
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
TE_ParseBeamRing

Creates a beam between two points
---------------
*/
void TE_ParseBeamRing( void )
{
	int startEnt, endEnt, modelIndex, startFrame;
	float frameRate, life, width;
	float brightness, noise, speed;
	Vector vecColor;

	startEnt = READ_SHORT();
	endEnt = READ_SHORT();

	modelIndex= READ_SHORT();
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

	g_pViewRenderBeams->CreateBeamRing( startEnt, endEnt, modelIndex, life, width, width, 0.0f, noise,
	brightness, speed, startFrame, frameRate, vecColor.x, vecColor.y, vecColor.z );
}

/*
---------------
TE_ParseDynamicLight

Creates a single source light
---------------
*/
void TE_ParseDynamicLight( int type )
{
	Vector	pos, color;
	int	flags, iAttachment = 0;
	edict_t	*pEnt = NULL;
	float	life, radius, decay;

	if( type == TE_ELIGHT )
	{
		int entIndex = READ_SHORT();
		pEnt = GetEntityByIndex( BEAMENT_ENTITY( entIndex ));
		iAttachment = BEAMENT_ATTACHMENT( entIndex );
	}
	
	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();

	if( type == TE_ELIGHT )
		radius = (float)READ_COORD();
	else radius = (float)(READ_BYTE() * 0.1f);

	color.x = (float)READ_BYTE();
	color.y = (float)READ_BYTE();
	color.z = (float)READ_BYTE();

	life = (float)(READ_BYTE() * 0.1f);

	if( type == TE_ELIGHT )
		decay = (float)READ_COORD();
	else decay = (float)(READ_BYTE() * 0.1f);

	if( type == TE_ELIGHT )
		flags = DLIGHT_ELIGHT;
	else flags = 0;

	g_engfuncs.pEfxAPI->CL_AllocDLight( pos, color, radius, life, flags, 0 );
}

/*
---------------
TE_AddClientMessage

show user message form server
---------------
*/
void TE_AddClientMessage( void )
{
	const int			MAX_CHANNELS = 4; // hl1 network specs
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
TE_ParseKillBeam

Kill all beams attached to entity
---------------
*/
void TE_ParseKillBeam( void )
{
	edict_t *pEntity = GetEntityByIndex( READ_SHORT() );

	g_pViewRenderBeams->KillDeadBeams( pEntity );
}

/*
---------------
TE_ParseFunnel

see env_funnel description for details
---------------
*/
void TE_ParseFunnel( void )
{
}

/*
---------------
TE_ParseBlood

create a particle spray
---------------
*/
void TE_ParseBlood( void )
{
	Vector pos, dir;
	int color, speed;

	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();
	dir.x = READ_COORD();
	dir.y = READ_COORD();
	dir.z = READ_COORD();
	color = READ_BYTE();
	speed = READ_BYTE();

	g_engfuncs.pEfxAPI->R_RunParticleEffect( pos, dir, color, speed );
}

/*
---------------
TE_ParseDecal

generic message for place static decal
---------------
*/
void TE_ParseDecal( int type )
{
	Vector pos;
	edict_t *pEnt;
	int decalIndex, entityIndex;

	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();
	decalIndex = READ_BYTE();

	if( type == TE_DECAL || type == TE_DECALHIGH )
	{
		entityIndex = READ_SHORT();
		pEnt = GetEntityByIndex( entityIndex );
	}
	if( type == TE_DECALHIGH || type == TE_WORLDDECALHIGH )
		decalIndex += 256;

	g_pTempEnts->PlaceDecal( pos, 5.0f, decalIndex );
}

/*
---------------
TE_ParseFizzEffect

Create alpha sprites inside of entity, float upwards
---------------
*/
void TE_ParseFizzEffect( void )
{
}

/*
---------------
TE_ParseTempModel

Create a moving model that bounces and makes a sound when it hits
---------------
*/
void TE_ParseTempModel( void )
{
	Vector pos, dir, rot;
	int modelIndex, soundType;
	float decay;

	pos.x = READ_COORD(); // pos
	pos.y = READ_COORD();
	pos.z = READ_COORD();
	dir.x = READ_COORD(); // velocity
	dir.y = READ_COORD();
	dir.z = READ_COORD();

	rot = Vector( 0.0f, READ_ANGLE(), 0.0f ); // yaw
	modelIndex = READ_SHORT();
	soundType = READ_BYTE();
	decay = (float)(READ_BYTE() * 0.1f);

	g_pTempEnts->TempModel( pos, dir, rot, decay, modelIndex, soundType );
}

/*
---------------
TE_ParseBreakModel

Create a box of models or sprites
---------------
*/
void TE_ParseBreakModel( void )
{
	Vector pos, size, dir;
	float random, decay;
	int modelIndex, count, flags;

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
}

/*
---------------
TE_ParseGunShotDecal

decal and ricochet sound
---------------
*/
void TE_ParseGunShotDecal( void )
{
	Vector pos, dir;
	int entityIndex, decalIndex;
	char soundpath[32];
	edict_t *pEnt;
	float dummy;

	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();
	entityIndex = READ_SHORT();
	pEnt = GetEntityByIndex( entityIndex );
	decalIndex = READ_BYTE();

	UTIL_GetForceDirection( pos, 2.0f, &dir, &dummy );

	g_pParticles->BulletParticles( pos, dir );
	g_pTempEnts->PlaceDecal( pos, 2.0f, decalIndex );

	if( RANDOM_LONG( 0, 32 ) <= 8 ) // 25% chanse
	{
		sprintf( soundpath, "weapons/ric%i.wav", RANDOM_LONG( 1, 5 ));
		CL_PlaySound( soundpath, RANDOM_FLOAT( 0.7f, 0.9f ), pos, RANDOM_FLOAT( 95.0f, 105.0f ));
	}
}

/*
---------------
TE_ParseSpriteSpray

spray of alpha sprites
---------------
*/
void TE_ParseSpriteSpray( void )
{
	Vector pos, vel;
	int spriteIndex, count, speed, noise;

	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();
	vel.x = READ_COORD();
	vel.y = READ_COORD();
	vel.z = READ_COORD();
	spriteIndex = READ_SHORT();
	count = READ_BYTE();
	speed = READ_BYTE();
	noise = READ_BYTE();

	g_pTempEnts->Sprite_Spray( pos, vel, spriteIndex, count, speed, noise );
}

/*
---------------
TE_ParseArmorRicochet

quick spark sprite, client ricochet sound. 
---------------
*/
void TE_ParseArmorRicochet( void )
{
	Vector pos;
	float radius;
	char soundpath[32];

	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();
	radius = (float)(READ_BYTE() * 0.1f);

	g_pParticles->RicochetSparks( pos, radius );

	sprintf( soundpath, "weapons/ric%i.wav", RANDOM_LONG( 1, 5 ));
	CL_PlaySound( soundpath, RANDOM_FLOAT( 0.7f, 0.9f ), pos, RANDOM_FLOAT( 95.0f, 105.0f ));
}

/*
---------------
TE_ParseBubblesEffect

Create alpha sprites inside of box, float upwards
---------------
*/
void TE_ParseBubblesEffect( void )
{
}

/*
---------------
TE_ParseBubbleTrail

Create alpha sprites along a line, float upwards
---------------
*/
void TE_ParseBubbleTrail( void )
{
}

/*
---------------
TE_ParseBloodSprite

Spray of opaque sprite1's that fall, single sprite2 for 1..2 secs
(this is a high-priority tent)
---------------
*/
void TE_ParseBloodSprite( void )
{
	Vector pos;
	int modelIndex, modelIndex2, iColor;
	float scale;

	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();
	modelIndex = READ_SHORT();
	modelIndex2 =READ_SHORT();
	iColor = READ_BYTE();
	scale = (float)READ_BYTE();

	g_pTempEnts->BloodSprite( pos, iColor, modelIndex, modelIndex2, scale );
}

/*
---------------
TE_ParseAttachTentToPlayer

Attaches a TENT to a player
(this is a high-priority tent)
---------------
*/
void TE_ParseAttachTentToPlayer( void )
{
	int clientIndex, modelIndex;
	float zoffset, life;

	clientIndex = READ_BYTE();
	zoffset = READ_COORD();
	modelIndex = READ_SHORT();
	life = (float)(READ_BYTE() * 0.1f);

//	g_pTempEnts->AttachTentToPlayer( clientIndex, modelIndex, zoffset, life );
}

/*
---------------
TE_KillAttachedTents

will expire all TENTS attached to a player.
---------------
*/
void TE_KillAttachedTents( void )
{
	int clientIndex = READ_BYTE();

//	g_pTempEnts->KillAttachedTents( clientIndex );
}

/*
---------------
HUD_ParseTempEntity

main switch for all tempentities
---------------
*/
void HUD_ParseTempEntity( void )
{
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
		TE_ParseTarExplosion();
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
		TE_ParseLavaSplash();
		break;
	case TE_TELEPORT:
		TE_ParseTeleport();
		break;
	case TE_EXPLOSION2:
		TE_ParseExplosion2();
		break;
	case TE_BSPDECAL:
		TE_ParseBSPDecal();
		break;
	case TE_IMPLOSION:
		TE_ParseImplosion();
		break;
	case TE_SPRITETRAIL:
		TE_ParseSpriteTrail();
		break;
	case TE_BEAMLASER:
		// FIXME: implement
		break;
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
		TE_ParseBeamRing();
		break;
	case TE_STREAK_SPLASH:
		// FIXME: implement
		break;
	case TE_BEAMRINGPOINT:
		// FIXME: implement
		break;
	case TE_ELIGHT:
		TE_ParseDynamicLight( TE_ELIGHT );
		break;
	case TE_DLIGHT:
		TE_ParseDynamicLight( TE_DLIGHT );
		break;
	case TE_TEXTMESSAGE:
		TE_AddClientMessage();
		break;
	case TE_LINE:
		// FIXME: implement
		break;
	case TE_BOX:
		// FIXME: implement
		break;
	case TE_KILLBEAM:
		TE_ParseKillBeam();
		break;
	case TE_LARGEFUNNEL:
		TE_ParseFunnel();
		break;
	case TE_BLOODSTREAM:
		// FIXME: implement
		break;
	case TE_SHOWLINE:
		// FIXME: implement
		break;
	case TE_BLOOD:
		TE_ParseBlood();
		break;
	case TE_DECAL:
		TE_ParseDecal( TE_DECAL );
		break;	
	case TE_FIZZ:
		TE_ParseFizzEffect();
		break;
	case TE_MODEL:
		TE_ParseTempModel();
		break;
	case TE_EXPLODEMODEL:
		// FIXME: implement
		break;
	case TE_BREAKMODEL:
		TE_ParseBreakModel();
		break;
	case TE_GUNSHOTDECAL:
		TE_ParseGunShotDecal();
		break;
	case TE_SPRITE_SPRAY:
		TE_ParseSpriteSpray();
		break;
	case TE_ARMOR_RICOCHET:
		TE_ParseArmorRicochet();
		break;
	case TE_PLAYERDECAL:
		// FIXME: implement
		break;
	case TE_BUBBLES:
		TE_ParseBubblesEffect();
		break;
	case TE_BUBBLETRAIL:
		TE_ParseBubbleTrail();
		break;
	case TE_BLOODSPRITE:
		TE_ParseBloodSprite();
		break;
	case TE_WORLDDECAL:
		TE_ParseDecal( TE_WORLDDECAL );
		break;	
	case TE_WORLDDECALHIGH:
		TE_ParseDecal( TE_WORLDDECALHIGH );
		break;	
	case TE_DECALHIGH:
		TE_ParseDecal( TE_DECALHIGH );
		break;	
	case TE_PROJECTILE:
		// FIXME: implement
		break;
	case TE_SPRAY:
		// FIXME: implement
		break;
	case TE_PLAYERSPRITES:
		// FIXME: implement
		break;	
	case TE_PARTICLEBURST:
		// FIXME: implement
		break;
	case TE_FIREFIELD:
		// FIXME: implement
		break;
	case TE_PLAYERATTACHMENT:
		TE_ParseAttachTentToPlayer();
		break;
	case TE_KILLPLAYERATTACHMENTS:
		TE_KillAttachedTents();
		break;
	case TE_MULTIGUNSHOT:
		// FIXME: implement
		break;
	case TE_USERTRACER:
		// FIXME: implement
		break;
	}
}