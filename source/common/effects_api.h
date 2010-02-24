//=======================================================================
//			Copyright XashXT Group 2009 ©
//		       effects_api.h - client temp entities
//=======================================================================
#ifndef EFFECTS_API_H
#define EFFECTS_API_H

// temporary entity array
#define TENTPRIORITY_LOW		0
#define TENTPRIORITY_HIGH		1

// TEMPENTITY flags
#define FTENT_NONE			0
#define FTENT_SINEWAVE		(1<<0)
#define FTENT_GRAVITY		(1<<1)
#define FTENT_ROTATE		(1<<2)
#define FTENT_SLOWGRAVITY		(1<<3)
#define FTENT_SMOKETRAIL		(1<<4)
#define FTENT_COLLIDEWORLD		(1<<5)
#define FTENT_FLICKER		(1<<6)
#define FTENT_FADEOUT		(1<<7)
#define FTENT_SPRANIMATE		(1<<8)
#define FTENT_HITSOUND		(1<<9)
#define FTENT_SPIRAL		(1<<10)
#define FTENT_SPRCYCLE		(1<<11)
#define FTENT_COLLIDEALL		(1<<12)	// will collide with world and slideboxes
#define FTENT_PERSIST		(1<<13)	// tent is not removed when unable to draw 
#define FTENT_COLLIDEKILL		(1<<14)	// tent is removed upon collision with anything
#define FTENT_PLYRATTACHMENT		(1<<15)	// tent is attached to a player (owner)
#define FTENT_SPRANIMATELOOP		(1<<16)	// animating sprite doesn't die when last frame is displayed
#define FTENT_SCALE			(1<<17)	// an experiment
#define FTENT_SPARKSHOWER		(1<<18)
#define FTENT_NOMODEL		(1<<19)	// doesn't have a model, never draw (it just triggers other things)
#define FTENT_CLIENTCUSTOM		(1<<20)	// Must specify callback. Callback function is responsible
					// for killing tempent and updating fields
					// ( unless other flags specify how to do things )
#define FTENT_WINDBLOWN		(1<<21)	// This is set when the temp entity is blown by the wind
#define FTENT_NEVERDIE		(1<<22)	// Don't die as long as die != 0
#define FTENT_BEOCCLUDED		(1<<23)	// Don't draw if my specified normal's facing away from the view

struct tempent_s
{
	int		flags;
	float		die;
	float		m_flFrameMax;
	float		x, y;
	vec3_t		m_vecVelocity;	// tent velocity
	vec3_t		m_vecAvelocity;	// tent avelocity
	vec3_t		oldorigin;	// attachment offset, beam endpoint etc
	vec3_t		tentOffset;	// if attached, client origin + tentOffset = tent origin.
	float		fadeSpeed;
	float		bounceFactor;
	int		startAlpha;	// starting fadeout intensity
	sound_t		hitSound;
	byte		priority;		// 0 - low, 1 - high
	short		clientIndex;	// if attached, this is the index of the client to stick to
					// if COLLIDEALL, this is the index of the client to ignore
					// TENTS with FTENT_PLYRATTACHMENT MUST set the clientindex! 
	void		*pvEngineData;	// private data that alloced, freed and used by engine only
	HITCALLBACK	hitcallback;
	ENTCALLBACK	callback;

	TEMPENTITY	*next;

	// render-dependent fields
	model_t		modelindex;	// index of precached model
	vec3_t		origin;		// tent position
	vec3_t		angles;		// tent angles
	float		m_flFrameRate;	// studio, sprite framerate
	float		m_flFrame;	// studio, sprite current frame
	float		m_flSpriteScale;	// and model scale too
	int		m_nFlickerFrame;	// for keyed dlights
	int		m_iAttachment;	// sprite attachment
	int		m_iSequence;	// studiomodel sequence
	byte		renderAmt;	// actual lpha value
	byte		renderMode;	// fast method to set various effects
	byte		renderFX;		// matched with entity renderfx
	byte		skin;		// studiomodel skin
	byte		body;		// studiomodel body
};

struct cparticle_s
{
	vec3_t		origin;
	vec3_t		velocity;
	vec3_t		accel;
	vec3_t		color;
	vec3_t		colorVelocity;
	float		alpha;
	float		alphaVelocity;
	float		radius;
	float		radiusVelocity;
	float		length;
	float		lengthVelocity;
	float		rotation;
	float		bounceFactor;
};

typedef struct efxapi_s
{
	size_t	api_size;	 // must match with sizeof( efxapi_t );

	int	(*R_AllocParticle)( cparticle_t *src, HSPRITE shader, int flags ); 
	void	(*R_MuzzleFlash)( float *pos1, int modelIndex, int type );
	TEMPENTITY *(*R_TempModel)( float *pos, float *dir, float *ang, float life, int modelIndex, int soundtype );
	TEMPENTITY *(*R_DefaultSprite)( float *pos, int spriteIndex, float framerate );
	TEMPENTITY *(*R_TempSprite)( float *pos, float *dir, float scale, int modelIndex, int rendermode, int renderfx, float a, float life, int flags );
	int	(*CL_DecalIndex)( int id );
	int	(*CL_DecalIndexFromName)( const char *szDecalName );
	void	(*R_SetDecal)( float *org, float *dir, float *rgba, float rot, float rad, HSPRITE hSpr, int flags );
	void	(*CL_AllocDLight)( const float *org, float *rgb, float rad, float time, int flags, int key );
	TEMPENTITY *(*CL_TempEntAlloc)( float *org, int modelIndex );
	TEMPENTITY *(*CL_TempEntAllocNoModel)( float *org );
	TEMPENTITY *(*CL_TempEntAllocHigh)( float *org, int modelIndex );
	TEMPENTITY *(*CL_TentEntAllocCustom)( float *org, int modelIndex, int high, ENTCALLBACK pfnCallback );
	void	(*CL_FindExplosionPlane)( const float *origin, float radius, float *result );
	void	(*R_LightForPoint)( const float *rgflOrigin, float *lightValue );
} efxapi_t;

#endif//EFFECTS_API_H