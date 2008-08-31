//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         cm_rigidbody.c - rigid body stuff
//=======================================================================

#include "physic.h"
#include "mathlib.h"
#include "matrixlib.h"
#include "cm_utils.h"

physbody_t *Phys_CreateBody( sv_edict_t *ed, cmodel_t *mod, matrix4x3 transform, int solid )
{
	NewtonCollision	*col;
	NewtonBody	*body;
	matrix4x4		trans, offset;
	float		*vertices;
	int		numvertices;		
	vec3_t		size, center, mins, maxs;

	// setup matrixes
	Matrix4x4_LoadIdentity( trans );
	Matrix4x4_LoadIdentity( offset );

	if( mod )
	{
		VectorCopy( mod->mins, mins );
		VectorCopy( mod->maxs, maxs );

		if( solid == SOLID_BOX )
		{
			CM_RoundUpHullSize( mins );
			CM_RoundUpHullSize( maxs );
		}
		else if( solid == SOLID_CYLINDER )
		{
			// barrel always stay on top side
			VectorSet( vec3_angles, 90.0f, 0.0f, 0.0f );
			AngleVectors( vec3_angles, offset[0], offset[2], offset[1] );
			VectorSet( vec3_angles, 0.0f, 0.0f, 0.0f ); // don't forget reset angles
		}
	}
	else
	{
		// default size
		VectorSet( mins, -32, -32, -32 );
		VectorSet( maxs,  32,  32,  32 );
		MsgDev( D_WARN, "can't compute bounding box, use default size\n");
	}

	// setup offset matrix
	VectorSubtract( maxs, mins, size );
	VectorAdd( mins, maxs, center );
	ConvertDimensionToPhysic( size );
	ConvertDimensionToPhysic( center );
	VectorScale( center, 0.5, offset[3] );
	
	// setup translation matrix
	VectorCopy( transform[0], trans[0] );
	VectorCopy( transform[1], trans[1] );
	VectorCopy( transform[2], trans[2] );
	VectorCopy( transform[3], trans[3] );

	switch(solid)
	{          
	case SOLID_BOX:
		col = NewtonCreateBox( gWorld, size[0], size[1], size[2], &offset[0][0] );
		break;
	case SOLID_SPHERE:
		col = NewtonCreateSphere( gWorld, size[0]/2, size[1]/2, size[2]/2, &offset[0][0] );
		break;
	case SOLID_CYLINDER:
		col = NewtonCreateCylinder( gWorld, size[1]/2, size[2], &offset[0][0] );
		break;
	case SOLID_MESH:
		vertices = pi.GetModelVerts( ed, &numvertices );
		if(!vertices || !numvertices ) return NULL;
		col = NewtonCreateConvexHull( gWorld, numvertices, vertices, sizeof(vec3_t), &offset[0][0] );
		break;
	default:
		Host_Error("Phys_CreateBody: unsupported solid type\n");
		return NULL;
	}
	body = NewtonCreateBody( gWorld, col );
	NewtonBodySetUserData( body, ed );
	NewtonBodySetMassMatrix( body, 10.0f, size[0], size[1], size[2] ); // 10 kg

	// setup generic callback to engine.dll
	NewtonBodySetTransformCallback(body, Callback_ApplyTransform );
	NewtonBodySetForceAndTorqueCallback( body, Callback_ApplyForce );
	NewtonBodySetMatrix(body, &trans[0][0]);// origin
	NewtonReleaseCollision( gWorld, col );

	return (physbody_t *)body;
}

void Phys_RemoveBody( physbody_t *body )
{
	if( body ) NewtonDestroyBody( gWorld, (NewtonBody*)body );
}

void Phys_SetParameters( physbody_t *body, cmodel_t *mod, int material, float mass )
{
	vec3_t	size;

	if( !body || !mod) return;

	VectorSubtract( mod->maxs, mod->mins, size );
	ConvertDimensionToPhysic( size );
	if( !mass ) mass = VectorLength2( size ) * 9.8f; // volume factor
	NewtonBodySetMassMatrix( body, mass, size[0], size[1], size[2] );
}

void CM_SetOrigin( physbody_t *body, vec3_t origin )
{
	matrix4x4		trans;

	if( !body ) return;

	NewtonBodyGetMatrix( body, &trans[0][0] );
	CM_ConvertPositionToMeters( trans[3], origin );	// merge position
	NewtonBodySetMatrix( body, &trans[0][0] );	// set new origin
}

bool Phys_GetForce( physbody_t *body, vec3_t velocity, vec3_t avelocity, vec3_t force, vec3_t torque )
{
	if(!body) return false;

	NewtonBodyGetOmega( body, avelocity );
	NewtonBodyGetVelocity( body, velocity );
	NewtonBodyGetForce( body, force );
	NewtonBodyGetTorque( body, torque );

	ConvertPositionToGame( avelocity );
	ConvertPositionToGame( velocity );	
	return true;
}

void Phys_SetForce( physbody_t *body, vec3_t velocity, vec3_t avelocity, vec3_t force, vec3_t torque )
{
	if(!body) return;
	ConvertPositionToPhysic( avelocity );
	ConvertPositionToPhysic( velocity );
	NewtonBodySetOmega( body, avelocity );
	NewtonBodySetVelocity( body, velocity );
	NewtonBodySetForce( body, force );
	NewtonBodySetTorque( body, torque );
}

bool Phys_GetMassCentre( physbody_t *body, matrix3x3 mass )
{
	if(!body) return false;
	NewtonBodyGetCentreOfMass( body, (float *)mass );
	return true;
}

void Phys_SetMassCentre( physbody_t *body, matrix3x3 mass )
{
	if(!body) return;
	NewtonBodySetCentreOfMass( body, (float *)mass );
}