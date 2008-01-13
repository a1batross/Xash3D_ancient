//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_callback.c - game callbacks
//=======================================================================

#include "physic.h"


void Callback_ApplyForce( const NewtonBody* body ) 
{ 
	float	mass; 
	vec3_t	size, force, torque; 

	NewtonBodyGetMassMatrix (body, &mass, &size[0], &size[1], &size[2]); 

	VectorSet( torque, 0.0f, 0.0f, 0.0f );
	VectorSet( force, 0.0f, -9.8f * mass, 0.0f );

	NewtonBodyAddForce (body, force); 
	NewtonBodyAddTorque (body, torque); 
}

void Callback_ApplyTransform( const NewtonBody* body, const float* matrix )
{
	sv_edict_t	*edict = (sv_edict_t *)NewtonBodyGetUserData( body );
	matrix4x4		objcoords;
	matrix4x3		translate;// obj matrix

	// convert matrix4x4 into 4x3
	Mem_Copy( objcoords, (float *)matrix, sizeof(matrix4x4));
	VectorCopy( objcoords[0], translate[0] );
	VectorCopy( objcoords[1], translate[1] );
	VectorCopy( objcoords[2], translate[2] );
	VectorCopy( objcoords[3], translate[3] );

	pi.Transform( edict, translate );
}
