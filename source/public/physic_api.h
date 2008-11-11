//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       physic_api.h - xash physic library api
//=======================================================================
#ifndef PHYSIC_API_H
#define PHYSIC_API_H

// FIXME: killme
typedef struct pmove_s
{
	entity_state_t	ps;		// state (in / out)

	// command (in)
	usercmd_t		cmd;
	physbody_t	*body;		// pointer to physobject

	// results (out)
	int		numtouch;
	edict_t		*touchents[32];	// max touch
	edict_t		*groundentity;

	vec3_t		mins, maxs;	// bounding box size
	int		watertype;
	int		waterlevel;
	float		xyspeed;		// avoid to compute it twice

	// callbacks to test the world
	void		(*trace)( vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, trace_t *tr );
	int		(*pointcontents)( vec3_t point );
} pmove_t;

/*
==============================================================================

PHYSIC.DLL INTERFACE
==============================================================================
*/
typedef struct physic_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(physic_exp_t)

	// initialize
	bool (*Init)( void );				// init all physic systems
	void (*Shutdown)( void );				// shutdown all render systems

	void (*LoadBSP)( const void *buf );			// load bspdata ( bsplib use this )
	void (*FreeBSP)( void );				// free bspdata
	void (*WriteCollisionLump)( file_t *f, cmsave_t callback );	// write collision data into LUMP_COLLISION

	void (*DrawCollision)( cmdraw_t callback );		// debug draw world
	void (*Frame)( float time );				// physics frame

	cmodel_t *(*BeginRegistration)( const char *name, bool clientload, uint *checksum );
	cmodel_t *(*RegisterModel)( const char *name );
	void (*EndRegistration)( void );

	void (*SetAreaPortals)( byte *portals, size_t size );
	void (*GetAreaPortals)( byte **portals, size_t *size );
	void (*SetAreaPortalState)( int area1, int area2, bool open );

	int (*NumClusters)( void );
	int (*NumTextures)( void );
	int (*NumBmodels )( void );
	const char *(*GetEntityString)( void );
	const char *(*GetTextureName)( int index );
	byte *(*ClusterPVS)( int cluster );
	byte *(*ClusterPHS)( int cluster );
	int (*PointLeafnum)( vec3_t p );
	int (*BoxLeafnums)( vec3_t mins, vec3_t maxs, int *list, int listsize, int *lastleaf );
	int (*LeafCluster)( int leafnum );
	int (*LeafArea)( int leafnum );
	bool (*AreasConnected)( int area1, int area2 );
	int (*WriteAreaBits)( byte *buffer, int area );

	void (*ClipToGenericEntity)( trace_t *trace, cmodel_t *model, const vec3_t bodymins, const vec3_t bodymaxs, int bodysupercontents, matrix4x4 matrix, matrix4x4 inversematrix, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int contentsmask );
	void (*ClipToWorld)( trace_t *trace, cmodel_t *model, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int contentsmask );
	void (*CombineTraces)( trace_t *cliptrace, const trace_t *trace, edict_t *touch, bool is_bmodel );

	// player movement code
	void (*PlayerMove)( pmove_t *pmove, bool clientmove );
	
	// simple objects
	physbody_t *(*CreateBody)( sv_edict_t *ed, cmodel_t *mod, matrix4x3 transform, int solid );
	physbody_t *(*CreatePlayer)( sv_edict_t *ed, cmodel_t *mod, matrix4x3 transform );

	void (*SetOrigin)( physbody_t *body, vec3_t origin );
	void (*SetParameters)( physbody_t *body, cmodel_t *mod, int material, float mass );
	bool (*GetForce)(physbody_t *body, vec3_t vel, vec3_t avel, vec3_t force, vec3_t torque );
	void (*SetForce)(physbody_t *body, vec3_t vel, vec3_t avel, vec3_t force, vec3_t torque );
	bool (*GetMassCentre)( physbody_t *body, matrix3x3 mass );
	void (*SetMassCentre)( physbody_t *body, matrix3x3 mass );
	void (*RemoveBody)( physbody_t *body );
} physic_exp_t;

typedef struct physic_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(physic_imp_t)

	void (*ClientMove)( sv_edict_t *ed );
	void (*Transform)( sv_edict_t *ed, matrix4x3 transform );
	void (*PlaySound)( sv_edict_t *ed, float volume, const char *sample );
	float *(*GetModelVerts)( sv_edict_t *ent, int *numvertices );
} physic_imp_t;

#endif//PHYSIC_API_H