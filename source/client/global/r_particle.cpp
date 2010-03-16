//=======================================================================
//			Copyright XashXT Group 2010 ©
//	       r_particle.cpp - particle manager (come from q2e 0.40)
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "triangle_api.h"
#include "effects_api.h"
#include "ref_params.h"
#include "pm_movevars.h"
#include "ev_hldm.h"
#include "hud.h"
#include "r_particle.h"

CParticleSystem	*g_pParticles = NULL;

bool CParticle :: Evaluate( float gravity )
{
	float	curAlpha, curRadius, curLength;
	Vector	curColor, curOrigin, lastOrigin, curVelocity;
	float	time, time2;

	time = gpGlobals->time - flTime;
	time2 = time * time;

	curAlpha = alpha + alphaVelocity * time;
	curRadius = radius + radiusVelocity * time;
	curLength = length + lengthVelocity * time;

	if( curAlpha <= 0.0f || curRadius <= 0.0f || curLength <= 0.0f )
	{
		// faded out
		return false;
	}

	curColor = color + colorVelocity * time;
	curOrigin.x = origin.x + velocity.x * time + accel.x * time2;
	curOrigin.y = origin.y + velocity.y * time + accel.y * time2;
	curOrigin.z = origin.z + velocity.z * time + accel.z * time2 * gravity;

	if( flags & FPART_UNDERWATER )
	{
		// underwater particle
		lastOrigin.Init( curOrigin.x, curOrigin.y, curOrigin.z + curRadius );

		int contents = POINT_CONTENTS( lastOrigin );

		if( contents != CONTENTS_WATER && contents != CONTENTS_SLIME && contents != CONTENTS_LAVA )
		{
			// not underwater
			return false;
		}
	}

	if( flags & FPART_FRICTION )
	{
		// water friction affected particle
		int contents = POINT_CONTENTS( curOrigin );

		if( contents <= CONTENTS_WATER && contents >= CONTENTS_LAVA )
		{
			// add friction		
			switch( contents )
			{
			case CONTENTS_WATER:
				velocity *= 0.25f;
				accel *= 0.25f;
				break;
			case CONTENTS_SLIME:
				velocity *= 0.20f;
				accel *= 0.20f;
				break;
			case CONTENTS_LAVA:
				velocity *= 0.10f;
				accel *= 0.10f;
				break;
			}
			
			// don't add friction again
			flags &= ~FPART_FRICTION;
			curLength = 1.0f;
				
			// reset
			flTime = gpGlobals->time;
			origin = curOrigin;
			color = curColor;
			alpha = curAlpha;
			radius = curRadius;

			// don't stretch
			flags &= ~FPART_STRETCH;
			length = curLength;
			lengthVelocity = 0.0f;
		}
	}

	if( flags & FPART_BOUNCE )
	{
		edict_t	*clent = GetLocalPlayer();

		Vector	mins( -radius, -radius, -radius );
		Vector	maxs(  radius,  radius,  radius );

		// bouncy particle
		TraceResult tr;

		// FIXME: Xash3D support trace with custom hulls only for internal purposes
		UTIL_TraceLine( oldorigin, origin, ignore_monsters, clent, &tr );

		if( tr.flFraction > 0.0f && tr.flFraction < 1.0f )
		{
			// reflect velocity
			time = gpGlobals->time - (gpGlobals->frametime + gpGlobals->frametime * tr.flFraction);
			time = (time - flTime);

			curVelocity.x = velocity.x;
			curVelocity.y = velocity.y;
			curVelocity.z = velocity.z + accel.z * gravity * time;

			float d = DotProduct( curVelocity, tr.vecPlaneNormal ) * -1.0f;
			velocity = curVelocity + tr.vecPlaneNormal * d;
			velocity *= bounceFactor;

			// check for stop or slide along the plane
			if( tr.vecPlaneNormal.z > 0 && velocity.z < 1.0f )
			{
				if( tr.vecPlaneNormal.z == 1.0f )
				{
					velocity = g_vecZero;
					accel = g_vecZero;
					flags &= ~FPART_BOUNCE;
				}
				else
				{
					// FIXME: check for new plane or free fall
					float dot = DotProduct( velocity, tr.vecPlaneNormal );
					velocity = velocity + (tr.vecPlaneNormal * -dot);

					dot = DotProduct( accel, tr.vecPlaneNormal );
					accel = accel + (tr.vecPlaneNormal * -dot);
				}
			}

			curOrigin = tr.vecEndPos;
			curLength = 1;

			// reset
			flTime = gpGlobals->time;
			origin = curOrigin;
			color = curColor;
			alpha = curAlpha;
			radius = curRadius;

			// don't stretch
			flags &= ~FPART_STRETCH;
			length = curLength;
			lengthVelocity = 0.0f;
		}
	}
	
	// save current origin if needed
	if( flags & ( FPART_BOUNCE|FPART_STRETCH ))
	{
		lastOrigin = oldorigin;
		oldorigin = curOrigin;
	}

	if( flags & FPART_VERTEXLIGHT )
	{
		Vector	ambientLight;

		// vertex lit particle
		g_engfuncs.pEfxAPI->R_LightForPoint( curOrigin, ambientLight );
		curColor *= ambientLight;
	}

	if( flags & FPART_INSTANT )
	{
		// instant particle
		alpha = 0.0f;
		alphaVelocity = 0.0f;
	}

	if( curRadius == 1.0f )
	{
		float	scale = 0.0f;

		// hack a scale up to keep quake particles from disapearing
		scale += (curOrigin[0] - gpViewParams->vieworg[0]) * gpViewParams->forward[0];
		scale += (curOrigin[1] - gpViewParams->vieworg[1]) * gpViewParams->forward[1];
		scale += (curOrigin[2] - gpViewParams->vieworg[2]) * gpViewParams->forward[2];
		if( scale >= 20 ) curRadius = 1.0f + scale * 0.004f;
	}

	Vector	axis[3], verts[4];

	// prepare to draw
	if( curLength != 1.0f )
	{
		// find orientation vectors
		axis[0] = gpViewParams->vieworg - origin;
		axis[1] = oldorigin - origin;
		axis[2] = CrossProduct( axis[0], axis[1] );

		VectorNormalizeFast( axis[1] );
		VectorNormalizeFast( axis[2] );

		// find normal
		axis[0] = CrossProduct( axis[1], axis[2] );
		VectorNormalizeFast( axis[0] );

		oldorigin = origin + ( axis[1] * -curLength );
		axis[2] *= radius;

		// setup vertexes
		verts[0].x = lastOrigin.x + axis[2].x;
		verts[0].y = lastOrigin.y + axis[2].y;
		verts[0].z = lastOrigin.z + axis[2].z;
		verts[1].x = curOrigin.x + axis[2].x;
		verts[1].y = curOrigin.y + axis[2].y;
		verts[1].z = curOrigin.z + axis[2].z;
		verts[2].x = curOrigin.x - axis[2].x;
		verts[2].y = curOrigin.y - axis[2].y;
		verts[2].z = curOrigin.z - axis[2].z;
		verts[3].x = lastOrigin.x - axis[2].x;
		verts[3].y = lastOrigin.y - axis[2].y;
		verts[3].z = lastOrigin.z - axis[2].z;
	}
	else
	{
		if( rotation )
		{
			// Rotate it around its normal
			RotatePointAroundVector( axis[1], gpViewParams->forward, gpViewParams->right, rotation );
			axis[2] = CrossProduct( gpViewParams->forward, axis[1] );

			// the normal should point at the viewer
			axis[0] = -gpViewParams->forward;

			// Scale the axes by radius
			axis[1] *= curRadius;
			axis[2] *= curRadius;
		}
		else
		{
			// the normal should point at the viewer
			axis[0] = -gpViewParams->forward;

			// scale the axes by radius
			axis[1] = gpViewParams->right * curRadius;
			axis[2] = gpViewParams->up * curRadius;
		}

		verts[0].x = curOrigin.x + axis[1].x + axis[2].x;
		verts[0].y = curOrigin.y + axis[1].y + axis[2].y;
		verts[0].z = curOrigin.z + axis[1].z + axis[2].z;
		verts[1].x = curOrigin.x - axis[1].x + axis[2].x;
		verts[1].y = curOrigin.y - axis[1].y + axis[2].y;
		verts[1].z = curOrigin.z - axis[1].z + axis[2].z;
		verts[2].x = curOrigin.x - axis[1].x - axis[2].x;
		verts[2].y = curOrigin.y - axis[1].y - axis[2].y;
		verts[2].z = curOrigin.z - axis[1].z - axis[2].z;
		verts[3].x = curOrigin.x + axis[1].x - axis[2].x;
		verts[3].y = curOrigin.y + axis[1].y - axis[2].y;
		verts[3].z = curOrigin.z + axis[1].z - axis[2].z;
	}

	// draw the particle
	g_engfuncs.pTriAPI->Enable( TRI_SHADER );

	g_engfuncs.pTriAPI->RenderMode( kRenderNormal );	// use shader settings
	g_engfuncs.pTriAPI->Color4f( curColor.x, curColor.y, curColor.z, curAlpha );
			
	g_engfuncs.pTriAPI->Bind( m_hSprite, 0 );
	g_engfuncs.pTriAPI->Begin( TRI_QUADS );
	g_engfuncs.pTriAPI->TexCoord2f ( 0, 0 );
	g_engfuncs.pTriAPI->Vertex3fv( verts[0] );

	g_engfuncs.pTriAPI->TexCoord2f ( 1, 0 );
	g_engfuncs.pTriAPI->Vertex3fv ( verts[1] );

	g_engfuncs.pTriAPI->TexCoord2f ( 1, 1 );
	g_engfuncs.pTriAPI->Vertex3fv ( verts[2] );

	g_engfuncs.pTriAPI->TexCoord2f ( 0, 1 );
	g_engfuncs.pTriAPI->Vertex3fv ( verts[3] );
	g_engfuncs.pTriAPI->End();
		
	g_engfuncs.pTriAPI->Disable( TRI_SHADER );
		
	return true;
}

CParticleSystem :: CParticleSystem( void )
{
	memset( m_pParticles, 0, sizeof( CParticle ) * MAX_PARTICLES );

	m_pFreeParticles = m_pParticles;
	m_pActiveParticles = NULL;
}

CParticleSystem :: ~CParticleSystem( void )
{
	Clear ();
}

void CParticleSystem :: Clear( void )
{
	m_pFreeParticles = m_pParticles;
	m_pActiveParticles = NULL;

	for( int i = 0; i < MAX_PARTICLES; i++ )
		m_pParticles[i].next = &m_pParticles[i+1];

	m_pParticles[MAX_PARTICLES-1].next = NULL;

	// loading TE shaders
	m_hDefaultParticle = TEX_Load( "textures/particles/default" );
	m_hGlowParticle = TEX_Load( "textures/particles/glow" );
	m_hDroplet = TEX_Load( "textures/particles/droplet" );
	m_hBubble = TEX_Load( "textures/particles/bubble" );
	m_hSparks = TEX_Load( "textures/particles/spark" );
	m_hSmoke = TEX_Load( "textures/particles/smoke" );
}

void CParticleSystem :: FreeParticle( CParticle *pCur )
{
	pCur->next = m_pFreeParticles;
	m_pFreeParticles = pCur;
}

CParticle	*CParticleSystem :: AllocParticle( void )
{
	CParticle	*p;

	if( !m_pFreeParticles )
	{
		ALERT( at_console, "Overflow %d particles\n", MAX_PARTICLES );
		return NULL;
	}

	if( cl_particlelod->integer > 1 )
	{
		if(!( RANDOM_LONG( 0, 1 ) % cl_particlelod->integer ))
			return NULL;
	}

	p = m_pFreeParticles;
	m_pFreeParticles = p->next;
	p->next = m_pActiveParticles;
	m_pActiveParticles = p;

	return p;
}
	
void CParticleSystem :: Update( void )
{
	CParticle	*p, *next;
	CParticle	*active = NULL, *tail = NULL;

	if( !cl_particles->integer ) return;

	float gravity = -gpGlobals->frametime * gpMovevars->gravity;

	for( p = m_pActiveParticles; p; p = next )
	{
		// grab next now, so if the particle is freed we still have it
		next = p->next;

		if( !p->Evaluate( gravity ))
		{
			FreeParticle( p );
			continue;
		}

		p->next = NULL;

		if( !tail )
		{
			active = tail = p;
		}
		else
		{
			tail->next = p;
			tail = p;
		}
	}

	m_pActiveParticles = active;
}

bool CParticleSystem :: AddParticle( CParticle *src, HSPRITE shader, int flags )
{
	if( !src ) return false;

	CParticle	*dst = AllocParticle();

	if( !dst ) return false;

	if( shader ) dst->m_hSprite = shader;
	else dst->m_hSprite = m_hDefaultParticle;
	dst->flTime = gpGlobals->time;
	dst->flags = flags;

	dst->origin = src->origin;
	dst->velocity = src->velocity;
	dst->accel = src->accel; 
	dst->color = src->color;
	dst->colorVelocity = src->colorVelocity;
	dst->alpha = src->alpha;
	dst->scale = 1.0f;

	dst->radius = src->radius;
	dst->length = src->length;
	dst->rotation = src->rotation;
	dst->alphaVelocity = src->alphaVelocity;
	dst->radiusVelocity = src->radiusVelocity;
	dst->lengthVelocity = src->lengthVelocity;
	dst->bounceFactor = src->bounceFactor;

	// needs to save old origin
	if( flags & ( FPART_BOUNCE|FPART_FRICTION ))
		dst->oldorigin = dst->origin;

	return true;
}

/*
=================
CL_ExplosionParticles
=================
*/
void CParticleSystem :: ExplosionParticles( const Vector pos )
{
	CParticle	src;
	int	flags;

	if( !cl_particles->integer ) return;

	flags = (FPART_STRETCH|FPART_BOUNCE|FPART_FRICTION);

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

		if( !AddParticle( &src, m_hSparks, flags ))
			return;
	}

	// smoke
	flags = FPART_VERTEXLIGHT;

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

		if( !AddParticle( &src, m_hSmoke, flags ))
			return;
	}
}

/*
=================
CL_BubbleParticles
=================
*/
void CParticleSystem :: BubbleParticles( const Vector org, int count, float magnitude )
{
	CParticle	src;

	if( !cl_particles->integer ) return;

	for( int i = 0; i < count; i++ )
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

		if( !AddParticle( &src, m_hBubble, FPART_UNDERWATER ))
			return;
	}
}

/*
=================
CL_BulletParticles
=================
*/
void CParticleSystem :: SparkParticles( const Vector org, const Vector dir )
{
	CParticle	src;

	if( !cl_particles->integer ) return;

	// sparks
	int flags = (FPART_STRETCH|FPART_BOUNCE|FPART_FRICTION);

	for( int i = 0; i < 16; i++ )
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

		if( !AddParticle( &src, m_hSparks, flags ))
			return;
	}
}

/*
=================
CL_RicochetSparks
=================
*/
void CParticleSystem :: RicochetSparks( const Vector org, float scale )
{
	CParticle	src;

	if( !cl_particles->integer ) return;

	// sparks
	int flags = (FPART_STRETCH|FPART_BOUNCE|FPART_FRICTION);

	for( int i = 0; i < 16; i++ )
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

		if( !AddParticle( &src, m_hSparks, flags ))
			return;
	}
}

void CParticleSystem :: SmokeParticles( const Vector pos, int count )
{
	CParticle	src;

	if( !cl_particles->integer ) return;

	// smoke
	int flags = FPART_VERTEXLIGHT;

	for( int i = 0; i < count; i++ )
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

		if( !AddParticle( &src, m_hSmoke, flags ))
			return;
	}
}

/*
=================
CL_BulletParticles
=================
*/
void CParticleSystem :: BulletParticles( const Vector org, const Vector dir )
{
	CParticle	src;
	int cnt, count;

	if( !cl_particles->integer ) return;

	count = RANDOM_LONG( 3, 8 );
	cnt = POINT_CONTENTS( org );

	if( cnt == CONTENTS_WATER )
	{
		BubbleParticles( org, count, 0 );
		return;
	}

	// sparks
	int flags = (FPART_STRETCH|FPART_BOUNCE|FPART_FRICTION);

	for( int i = 0; i < count; i++ )
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

		if( !AddParticle( &src, m_hSparks, flags ))
			return;
	}

	// smoke
	flags = FPART_VERTEXLIGHT;

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

		if( !AddParticle( &src, m_hSmoke, flags ))
			return;
	}
}

/*
=================
CL_TeleportParticles

creates a particle box
=================
*/
void CParticleSystem :: TeleportParticles( const Vector org )
{
	CParticle	src;
	vec3_t dir;
	float vel, color;
	int x, y, z;

	if( !cl_particles->integer ) return;

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

				if( !AddParticle( &src, m_hGlowParticle, 0 ))
					return;

			}
		}
	}
}