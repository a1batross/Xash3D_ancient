//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         pworld.c - physic world wrapper
//=======================================================================

#include "physic.h"
#include "btBulletWorld.h"

physic_imp_t	pi;
stdlib_api_t	com;
byte		*physpool;

btBulletPhysic *g_PhysWorld;

void* Palloc( int size )
{
	return Mem_Alloc( physpool, size );
}

void Pfree( void *ptr )
{
	Mem_Free( ptr );
}

bool InitPhysics( void )
{
	physpool = Mem_AllocPool("Physics Pool");

	// create world
	g_PhysWorld = new btBulletPhysic();

	return true;
}

void FreePhysics( void )
{
	delete g_PhysWorld;
	Mem_FreePool( &physpool );
}

void Phys_LoadBSP( uint *buffer )
{
	BspLoader	WorldHull;

	WorldHull.loadBSPFile( buffer );
	WorldHull.convertBsp( g_PhysWorld->getScale());
}

void Phys_FreeBSP( void )
{
	g_PhysWorld->DeleteAllBodies();
}

void Phys_Frame( float time )
{
	g_PhysWorld->UpdateWorld( time );
}

void Phys_WorldUpdate( void )
{
	g_PhysWorld->PhysicFrame();
}

physbody_t *Phys_CreateBody( sv_edict_t *ed, vec3_t mins, vec3_t maxs, vec3_t org, vec3_t ang, int solid )
{
	btCollisionShape	*col;
	vec3_t		size, ofs;

	VectorSubtract (maxs, mins, size );
	ConvertPositionToPhysic( size );
	VectorAdd( mins, maxs, ofs );
	ConvertPositionToPhysic( ofs );

	switch( (solid_t)solid )
	{          
	case SOLID_BOX:
		col = new btBoxShape(btVector3( size[0]/2, size[1]/2, size[2]/2 ));
		break;
	case SOLID_SPHERE:
		col = new btSphereShape( size[0]/2 );
		break;
	case SOLID_CYLINDER:
		col = new btCylinderShape(btVector3( size[0]/2, size[1]/2, size[0]/2 ));
		break;
	case SOLID_MESH:
	default:
		Host_Error("Phys_CreateBody: unsupported solid type %d\n", solid );
		return NULL;
	}

	btTransform offset;
	offset.setIdentity();
	offset.setOrigin(btVector3(0, 0, 0));

	btRigidBody* body = g_PhysWorld->AddDynamicRigidBody( 10.0f, offset, col ); // 10 kg
	g_PhysWorld->SetUserData( body, ed );

	// move to position         
	g_PhysWorld->SetTransform( body, org, ang );
	return (physbody_t *)body;
}

void Phys_RemoveBody( physbody_t *object )
{
	btRigidBody* body = reinterpret_cast< btRigidBody* >(object);
	g_PhysWorld->DelRigidBody( body );
}

void DebugShowCollision ( void ) 
{
	g_PhysWorld->DrawDebug();
}