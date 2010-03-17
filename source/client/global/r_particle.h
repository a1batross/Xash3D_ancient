//=======================================================================
//			Copyright XashXT Group 2010 ©
//	    r_particle.h - another particle manager (come from q2e 0.40)
//=======================================================================

#ifndef R_PARTICLE_H
#define R_PARTICLE_H

#define MAX_PARTICLES		2048

// built-in particle-system flags
#define FPART_BOUNCE		(1<<0)	// makes a bouncy particle
#define FPART_FRICTION		(1<<1)
#define FPART_VERTEXLIGHT		(1<<2)	// give some ambient light for it
#define FPART_STRETCH		(1<<3)
#define FPART_UNDERWATER		(1<<4)
#define FPART_INSTANT		(1<<5)

class CParticle
{
public:
	Vector		origin;
	Vector		oldorigin;

	Vector		velocity;		// linear velocity
	Vector		accel;
	Vector		color;
	Vector		colorVelocity;
	float		alpha;
	float		alphaVelocity;
	float		radius;
	float		radiusVelocity;
	float		length;
	float		lengthVelocity;
	float		rotation;		// texture ROLL angle
	float		bounceFactor;
	float		scale;

	CParticle		*next;
	HSPRITE		m_hSprite;

	float		flTime;
	int		flags;

	bool		Evaluate( float gravity );
};

class CParticleSystem
{
	CParticle		*m_pActiveParticles;
	CParticle		*m_pFreeParticles;
	CParticle		m_pParticles[MAX_PARTICLES];

	// private partsystem shaders
	HSPRITE		m_hDefaultParticle;
	HSPRITE		m_hGlowParticle;
	HSPRITE		m_hDroplet;
	HSPRITE		m_hBubble;
	HSPRITE		m_hSparks;
	HSPRITE		m_hSmoke;
public:
			CParticleSystem( void );
	virtual		~CParticleSystem( void );

	void		Clear( void );
	void		Update( void );
	void		FreeParticle( CParticle *pCur );
	CParticle		*AllocParticle( void );
	bool		AddParticle( CParticle *src, HSPRITE shader, int flags );

	// example presets
	void		ExplosionParticles( const Vector pos );
	void		BulletParticles( const Vector org, const Vector dir );
	void		BubbleParticles( const Vector org, int count, float magnitude );
	void		SparkParticles( const Vector org, const Vector dir );
	void		RicochetSparks( const Vector org, float scale );
	void		SmokeParticles( const Vector pos, int count );
};

extern CParticleSystem	*g_pParticles;
extern ref_params_t		*gpViewParams;

#endif//R_PARTICLE_H