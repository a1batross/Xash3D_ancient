//=======================================================================
//			Copyright XashXT Group 2009 ©
//		        cl_world.c - world query functions
//=======================================================================

#include "common.h"
#include "client.h"
#include "const.h"

/*
==================
CL_Trace

UNDONE: trace worldonly
==================
*/
TraceResult CL_Trace( const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int type, edict_t *e, int mask )
{
	host_clip_t	clip;

	if( !mins ) mins = vec3_origin;
	if( !maxs ) maxs = vec3_origin;

	Mem_Set( &clip, 0, sizeof( clip ));

	// clip to world
	pe->BoxTrace1( &clip.trace, start, end, mins, maxs, 0, mask, TR_AABB );
	clip.trace.pHit = (clip.trace.flFraction != 1.0f) ? EDICT_NUM( 0 ) : NULL;

	return clip.trace; // blocked immediately by the world
}

/*
=============
CL_PointContents

UNDONE: contents of worldonly
=============
*/
int CL_PointContents( const vec3_t p )
{
	// get base contents from world
	return pe->PointContents1( p, 0 );
}