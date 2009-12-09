//=======================================================================
//			Copyright XashXT Group 2009 �
//		       effects_api.h - client temp entities
//=======================================================================
#ifndef EFFECTS_API_H
#define EFFECTS_API_H

typedef void (*HITCALLBACK)( TEMPENTITY *ent, TraceResult *ptr );
typedef void (*ENTCALLBACK)( TEMPENTITY *ent );

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

struct tempent_s
{
	int		flags;
	float		die;
	float		frameMax;
	vec3_t		origin;
	float		fadeSpeed;
	float		bounceFactor;
	sound_t		hitSound;

	HITCALLBACK	hitcallback;
	ENTCALLBACK	callback;

	TEMPENTITY	*next;
	int		priority;
};

typedef struct efxapi_s
{
	size_t	api_size;	 // must match with sizeof( efxapi_t );

	int	(*R_AllocParticle)( cparticle_t *src, HSPRITE shader, int flags ); 
	void	(*R_SetDecal)( float *org, float *dir, float *rgba, float rot, float rad, HSPRITE hSpr, int flags );
	void	(*CL_AllocDLight)( const float *org, float *rgb, float rad, float time, int flags, int key ); 
	void	(*CL_FindExplosionPlane)( const float *origin, float radius, float *result );
	int	(*CL_DecalIndexFromName)( const char *szDecalName );
	int	(*CL_DecalIndex)( int id );
	void	(*R_LightForPoint)( const float *rgflOrigin, float *lightValue );
} efxapi_t;

#endif//EFFECTS_API_H