//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       physic_api.h - xash physic library api
//=======================================================================
#ifndef PHYSIC_API_H
#define PHYSIC_API_H

#include "trace_def.h"

// trace type
typedef enum
{
	TR_NONE,
	TR_AABB,
	TR_CAPSULE,
	TR_BISPHERE,
	TR_NUMTYPES
} trType_t;

/*
==============================================================================

PHYSIC.DLL INTERFACE
==============================================================================
*/
typedef struct physic_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(physic_exp_t)
	size_t	com_size;		// must matched with sizeof(stdlib_api_t)

	// initialize
	bool (*Init)( void );				// init all physic systems
	void (*Shutdown)( void );				// shutdown all render systems

	void (*DrawCollision)( cmdraw_t callback );		// debug draw world
	void (*Frame)( float time );				// physics frame

	// models loading
	void (*BeginRegistration)( const char *name, bool clientload, uint *checksum );
	bool (*RegisterModel)( const char *name, int sv_index ); // also build replacement index table
	void (*EndRegistration)( void );

	// areaportal management
	void (*SetAreaPortals)( byte *portals, size_t size );
	void (*GetAreaPortals)( byte **portals, size_t *size );
	void (*SetAreaPortalState)( int portalnum, int area, int otherarea, bool open );
	int (*BoxLeafnums)( vec3_t mins, vec3_t maxs, int *list, int listsize, int *lastleaf );
	int (*WriteAreaBits)( byte *buffer, int area, bool portal );
	bool (*AreasConnected)( int area1, int area2 );
	byte *(*ClusterPVS)( int cluster );
	byte *(*ClusterPHS)( int cluster );
	int (*LeafCluster)( int leafnum );
	int (*PointLeafnum)( const vec3_t p );
	int (*LeafArea)( int leafnum );

	// map data
	int (*NumShaders)( void );
	int (*NumBmodels)( void );
	void (*Mod_GetBounds)( model_t handle, vec3_t mins, vec3_t maxs );
	void (*Mod_GetFrames)( model_t handle, int *numFrames );
	void (*Mod_GetAttachment)( edict_t *e, int iAttachment, float *org, float *ang );
	void (*Mod_GetBonePos)( edict_t* e, int iBone, float *rgflOrigin, float *rgflAngles );
	modtype_t (*Mod_GetType)( model_t handle );
	const char *(*GetShaderName)( int index );
	void *(*Mod_Extradata)( model_t handle );
	script_t *(*GetEntityScript)( void );
	const void *(*VisData)( void );

	// trace heleper
	int (*PointContents1)( const vec3_t p, model_t model );
	int (*PointContents2)( const vec3_t p, model_t model, const vec3_t org, const vec3_t ang );
	void (*BoxTrace1)( trace_t *results, const vec3_t p1, const vec3_t p2, vec3_t mins, vec3_t maxs, model_t model, int mask, trType_t type );
	void (*BoxTrace2)( trace_t *results, const vec3_t p1, const vec3_t p2, vec3_t mins, vec3_t maxs, model_t model, int mask, const vec3_t org, const vec3_t ang, trType_t type );
	bool (*HitboxTrace)( trace_t *tr, edict_t *e, const vec3_t p1, const vec3_t p2 );
	model_t (*TempModel)( const vec3_t mins, const vec3_t maxs, bool capsule );

	// needs to be removed
	byte *(*FatPVS)( const vec3_t org, bool portal );
	byte *(*FatPHS)( int cluster, bool portal );
} physic_exp_t;

typedef struct physic_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(physic_imp_t)

} physic_imp_t;

#endif//PHYSIC_API_H