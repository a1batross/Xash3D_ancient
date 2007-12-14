//=======================================================================
//			Copyright XashXT Group 2007 ©
//			 utils.c - physics callbacks
//=======================================================================

#include "physic.h"
#include "transform.h"

#include <math.h>
#include <gl/gl.h>

long _ftol( double );
long _ftol2( double dblSource ) 
{
	return _ftol( dblSource );
}

void* Palloc (int size )
{
	return Mem_Alloc( physpool, size );
}

void Pfree (void *ptr, int size )
{
	Mem_Free( ptr );
}

void Phys_Frame( float time )
{
	NewtonUpdate(gWorld, time );
}

void Phys_ApplyForce(const NewtonBody* body) 
{ 
	float	mass; 
	vec3_t	size, force, torque; 

	NewtonBodyGetMassMatrix (body, &mass, &size[0], &size[1], &size[2]); 

	VectorSet( torque, 0.0f, 0.0f, 0.0f );
	VectorSet( force, 0.0f, -9.8f * mass, 0.0f );

	NewtonBodyAddForce (body, force); 
	NewtonBodyAddTorque (body, torque); 
}

void Phys_ApplyTransform( const NewtonBody* body, const float* matrix )
{
	sv_edict_t	*edict = (sv_edict_t *)NewtonBodyGetUserData( body );
	matrix4x4		translate;// obj matrix
	vec3_t		origin, angles;

	Mem_Copy(translate, (float *)matrix, sizeof(matrix4x4));
	MatrixAngles( translate, origin, angles );
	pi.Transform( edict, origin, angles );
}

physbody_t *Phys_CreateBody( sv_edict_t *ed, void *buffer, vec3_t org, vec3_t ang, int solid )
{
	NewtonCollision	*col;
	NewtonBody	*body;
	matrix4x4		trans, offset;
	vec3_t		size, center;
	float		*vertices;
	int		numvertices;		
	vec3_t		rot, ang2, org2, mins, maxs;
	studiohdr_t	*phdr = (studiohdr_t *)buffer;
	mstudioseqdesc_t	*pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);

	VectorCopy( pseqdesc[ 0 ].bbmin, mins );
	VectorCopy( pseqdesc[ 0 ].bbmax, maxs );

	VectorSubtract( maxs, mins, size );
	VectorAdd( mins, maxs, center );

	ConvertDimensionToPhysic( size );
	ConvertDimensionToPhysic( center );

	VectorScale( center, 0.5, center );
	VectorNegate( center, center );

	MatrixLoadIdentity( trans );
	MatrixLoadIdentity( offset );

	AnglesMatrix( org, ang, trans );
	VectorCopy( center, offset[3] );

	switch(solid)
	{          
	case SOLID_BOX:
		col = NewtonCreateBox( gWorld, size[0], size[1], size[2], &offset[0][0] );
		break;
	case SOLID_SPHERE:
		col = NewtonCreateSphere( gWorld, size[0]/2, size[1]/2, size[2]/2, &offset[0][0] );
		break;
	case SOLID_CYLINDER:
		col = NewtonCreateCylinder( gWorld, size[0], size[1], &offset[0][0] );
		break;
	case SOLID_MESH:
/*		vertices = pi.GetModelVerts( ed, &numvertices );
		if(!vertices) return;
		col = NewtonCreateConvexHull(gWorld, numvertices, vertices, sizeof(vec3_t), &offset[0][0] );
		break;
*/
	default:
		Host_Error("Phys_CreateBody: unsupported solid type\n");
		return NULL;
	}
	body = NewtonCreateBody( gWorld, col );
	NewtonBodySetUserData( body, ed );
	NewtonBodySetMassMatrix( body, 10.0f, size[0], size[1], size[2] ); // 10 kg

	// Setup generic callback to engine.dll
	NewtonBodySetTransformCallback(body, Phys_ApplyTransform );
	NewtonBodySetForceAndTorqueCallback(body, Phys_ApplyForce );
	NewtonBodySetMatrix(body, &trans[0][0]);// origin
	//NewtonReleaseCollision( gWorld, col );

	return (physbody_t *)body;
}

void Phys_RemoveBody( physbody_t *body )
{
	if(body) 
	{
		Msg("remove body\n");
		NewtonDestroyBody( gWorld, (NewtonBody*)body );
	}
} 

void DebugShowGeometryCollision(const NewtonBody* body, int vertexCount, const float* faceVertec, int id) 
{    
	if(ph.debug_line) ph.debug_line( 0, vertexCount, faceVertec );
} 

void DebugShowBodyCollision (const NewtonBody* body) 
{ 
	NewtonBodyForEachPolygonDo(body, DebugShowGeometryCollision); 
} 

// show all collision geometry 
void DebugShowCollision ( cmdraw_t callback  ) 
{ 
	ph.debug_line = callback; // member draw function
	NewtonWorldForEachBodyDo(gWorld, DebugShowBodyCollision); 
}