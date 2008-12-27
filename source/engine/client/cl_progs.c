//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        cl_progs.c - client.dat interface
//=======================================================================

#include "common.h"
#include "client.h"

/*
=================
PF_findexplosionplane

vector CL_FindExplosionPlane( vector org, float radius )
=================
*/
static void PF_findexplosionplane( void )
{
	static vec3_t	planes[6] = {{0, 0, 1}, {0, 1, 0}, {1, 0, 0}, {0, 0, -1}, {0, -1, 0}, {-1, 0, 0}};
	trace_t		trace;
	float		best = 1.0, radius;
	vec3_t		point, dir;
	const float	*org;
	int		i;

	if( !VM_ValidateArgs( "CL_FindExplosionPlane", 2 ))
		return;

	org = PRVM_G_VECTOR(OFS_PARM0);
	radius = PRVM_G_FLOAT(OFS_PARM1);
	VectorClear( dir );

	for( i = 0; i < 6; i++ )
	{
		VectorMA( org, radius, planes[i], point );

		trace = CL_Trace( org, vec3_origin, vec3_origin, point, MOVE_WORLDONLY, NULL, MASK_SOLID );
		if( trace.allsolid || trace.fraction == 1.0 )
			continue;

		if( trace.fraction < best )
		{
			best = trace.fraction;
			VectorCopy( trace.plane.normal, dir );
		}
	}
	VectorCopy( dir, PRVM_G_VECTOR( OFS_RETURN ));
}