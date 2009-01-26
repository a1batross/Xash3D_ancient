//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       physic_api.h - xash physic library api
//=======================================================================
#ifndef PHYSIC_API_H
#define PHYSIC_API_H

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

	void (*LoadBSP)( wfile_t *wad );			// load bspdata ( bsplib use this )
	void (*FreeBSP)( void );				// free bspdata
	void (*WriteCollisionLump)( file_t *f, cmsave_t callback );	// write collision data into LUMP_COLLISION

	void (*DrawCollision)( cmdraw_t callback );		// debug draw world
	void (*Frame)( float time );				// physics frame

	cmodel_t *(*BeginRegistration)( const char *name, bool clientload, uint *checksum );
	cmodel_t *(*RegisterModel)( const char *name );
	void (*EndRegistration)( void );

	void (*SetAreaPortals)( byte *portals, size_t size );
	void (*GetAreaPortals)( byte **portals, size_t *size );
	void (*SetAreaPortalState)( int portalnum, bool open );

	int (*NumClusters)( void );
	int (*NumTextures)( void );
	int (*NumBmodels )( void );
	script_t *(*GetEntityScript)( void );
	const char *(*GetTextureName)( int index );
	byte *(*ClusterPVS)( int cluster );
	byte *(*ClusterPHS)( int cluster );
	int (*PointLeafnum)( vec3_t p );
	byte *(*FatPVS)( const vec3_t org, bool portal );
	int (*BoxLeafnums)( vec3_t mins, vec3_t maxs, int *list, int listsize, int *lastleaf );
	int (*WriteAreaBits)( byte *buffer, int area, bool portal );
	bool (*HeadnodeVisible)( int nodenum, byte *visbits );
	int (*LeafCluster)( int leafnum );
	int (*LeafArea)( int leafnum );
	bool (*AreasConnected)( int area1, int area2 );

	void (*ClipToGenericEntity)( trace_t *trace, cmodel_t *model, const vec3_t bodymins, const vec3_t bodymaxs, int bodysupercontents, matrix4x4 matrix, matrix4x4 inversematrix, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int contentsmask );
	void (*ClipToWorld)( trace_t *trace, cmodel_t *model, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int contentsmask );
	void (*CombineTraces)( trace_t *cliptrace, const trace_t *trace, edict_t *touch, bool is_bmodel );

	// player movement code
	void (*PlayerMove)( struct entity_state_s *pmove, usercmd_t *cmd, physbody_t *body, bool clientmove );
	
	// simple objects
	physbody_t *(*CreateBody)( edict_t *ed, cmodel_t *mod, const vec3_t org, const matrix3x3 m, int solid, int move );
	physbody_t *(*CreatePlayer)( edict_t *ed, cmodel_t *mod, const vec3_t origin, const matrix3x3 matrix );

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

	void (*ClientMove)( edict_t *ed );
	void (*Transform)( edict_t *ed, const vec3_t org, const matrix3x3 matrix );
	void (*PlaySound)( edict_t *ed, float volume, float pitch, const char *sample );
	float *(*GetModelVerts)( edict_t *ent, int *numvertices );
} physic_imp_t;

#endif//PHYSIC_API_H