//=======================================================================
//			Copyright XashXT Group 2009 ©
//		trace_def.h - shared client\server trace struct
//=======================================================================
#ifndef TRACE_DEF_H
#define TRACE_DEF_H

typedef struct
{
	int		fAllSolid;	// if true, plane is not valid
	int		fStartSolid;	// if true, the initial point was in a solid area
	int		fInOpen;		// if true trace is open
	int		fInWater;		// if true trace is in water
	float		flFraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		vecEndPos;	// final position
	float		flPlaneDist;	// planes distance
	vec3_t		vecPlaneNormal;	// surface normal at impact
	edict_t		*pHit;		// entity the surface is on
	int		iHitgroup;	// 0 == generic, non zero is specific body part
} TraceResult;

#endif//TRACE_DEF_H