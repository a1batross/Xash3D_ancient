//=======================================================================
//			Copyright XashXT Group 2009 ©
//		       effects_api.h - client temp entities
//=======================================================================
#ifndef EFFECTS_API_H
#define EFFECTS_API_H

struct particle_s
{
	byte		color;	// colorIndex for R_GetPaletteColor
	vec3_t		org;
	vec3_t		vel;
	float		die;
	float		ramp;
	ptype_t		type;
};

typedef struct efxapi_s
{
	size_t	api_size;	 // must match with sizeof( efxapi_t );

	particle_t *(*R_AllocParticle)( void ); 
	void	(*R_BlobExplosion)( const float *org );
	void	(*R_EntityParticles)( edict_t *ent );
	void	(*R_LavaSplash)( const float *org );
	void	(*R_ParticleExplosion)( const float *org );
	void	(*R_ParticleExplosion2)( const float *org, int colorStart, int colorLength );
	void	(*R_RocketTrail)( const float *start, const float *end, int type );
	void	(*R_RunParticleEffect)( const float *org, const float *dir, int color, int count );
	void	(*R_TeleportSplash)( const float *org );
	void	(*R_GetPaletteColor)( int colorIndex, float *outColor );
	int	(*CL_DecalIndex)( int id );
	int	(*CL_DecalIndexFromName)( const char *szDecalName );
	int	(*R_ShootDecal)( HSPRITE hSpr, edict_t *pEnt, const float *pos, int color, float roll, float rad );
	void	(*CL_AllocDLight)( const float *org, float *rgb, float rad, float lifetime, int flags, int key );
	void	(*CL_FindExplosionPlane)( const float *origin, float radius, float *result );
	void	(*R_LightForPoint)( const float *rgflOrigin, float *lightValue );
	int	(*CL_IsBoxVisible)( const float *mins, const float *maxs );
	int	(*R_CullBox)( const float *mins, const float *maxs );
	int	(*R_AddEntity)( edict_t *pEnt, int ed_type, HSPRITE customShader );
	int	(*R_AddTempEntity)( TEMPENTITY *pTemp, HSPRITE customShader );
} efxapi_t;

#endif//EFFECTS_API_H