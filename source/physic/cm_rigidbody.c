//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         cm_rigidbody.c - rigid body stuff
//=======================================================================

#include "physic.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "cm_local.h"
#include "entity_def.h"

physbody_t *Phys_CreateBody( edict_t *ed, cmodel_t *mod, const vec3_t org, const matrix3x3 m, int solid, int move )
{
	NewtonCollision	*col;
	NewtonBody	*body;
	matrix4x4		trans, offset;
	float		*vertices, mass = 10.0f;
	vec3_t		size, mins, maxs;
	int		numvertices;

	// setup matrixes
	Matrix4x4_LoadIdentity( trans );
	Matrix4x4_LoadIdentity( offset );

	// setup translation matrix
	VectorCopy( m[0], trans[0] );
	VectorCopy( m[1], trans[1] );
	VectorCopy( m[2], trans[2] );
	VectorCopy( org, trans[3] );

	if( mod )
	{
		VectorCopy( mod->mins, mins );
		VectorCopy( mod->maxs, maxs );

		switch( solid )
		{
		case SOLID_BOX:
			CM_RoundUpHullSize( mins );
			CM_RoundUpHullSize( maxs );
			break;
		case SOLID_CYLINDER:
			// barrel always stay on top side
			VectorSet( vec3_angles, 90.0f, 0.0f, 0.0f );
			AngleVectors( vec3_angles, offset[0], offset[2], offset[1] );
			VectorClear( vec3_angles ); // don't forget reset angles
			break;
		case SOLID_BSP:
			if( VectorIsNull( org ))
				VectorAverage( mins, maxs, trans[3] );
			VectorSet( vec3_angles, -180.0f, 0.0f, 90.0f );
			AngleVectors( vec3_angles, offset[0], offset[1], offset[2] );
			VectorClear( vec3_angles ); // don't forget reset angles
			mass = 0.0f;
			break;
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
	ConvertDimensionToPhysic( size );

	if( solid != SOLID_BSP || !VectorCompare( offset[3], trans[3] ) && VectorIsNull( org ))
		VectorAverage( mins, maxs, offset[3] );

	ConvertDimensionToPhysic( offset[3] );
	ConvertPositionToPhysic( trans[3] );

	switch( solid )
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
	case SOLID_BSP:
	case SOLID_MESH:
		if( mod->col[bound( 0, ed->v.body, mod->numbodies )] == NULL )
			CM_CreateMeshBuffer( mod );
		if( mod->col[bound( 0, ed->v.body, mod->numbodies )] == NULL ) return NULL; // I_Error()
		vertices = (float *)mod->col[bound( 0, ed->v.body, mod->numbodies )]->verts;
		numvertices = mod->col[bound( 0, ed->v.body, mod->numbodies )]->numverts;
		col = NewtonCreateConvexHull( gWorld, numvertices, vertices, sizeof(vec3_t), &offset[0][0] );
		break;
	default:
		Host_Error("Phys_CreateBody: unsupported solid type %i\n", solid );
		return NULL;
	}

	body = NewtonCreateBody( gWorld, col );
	NewtonBodySetUserData( body, ed );
	NewtonBodySetMassMatrix( body, mass, size[0], size[1], size[2] ); // 10 kg

	// setup callbacks to engine.dll
	switch( move )
	{
	case MOVETYPE_NONE:
	case MOVETYPE_PUSH:
		NewtonBodySetTransformCallback( body, Callback_Static );
		NewtonBodySetForceAndTorqueCallback( body, Callback_ApplyForce_NoGravity );
		break;
	case MOVETYPE_PHYSIC:
		NewtonBodySetTransformCallback( body, Callback_ApplyTransform );
		NewtonBodySetForceAndTorqueCallback( body, Callback_ApplyForce );
		break;
	default:
		Host_Error("Phys_CreateBody: unsupported movetype %i\n", move );
		return NULL;
	}
	NewtonBodySetMatrix( body, &trans[0][0] );// origin
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