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

// mem allocations
void* Palloc( int size ) { return Mem_Alloc( physpool, size ); }
void Pfree( void *ptr ) { Mem_Free( ptr ); }

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
	g_PhysWorld->LoadWorld( buffer );
}

void Phys_FreeBSP( void )
{
	g_PhysWorld->FreeWorld();
}

void Phys_Frame( float time )
{
	g_PhysWorld->UpdateWorld( time );
}

void Phys_WorldUpdate( void )
{
	g_PhysWorld->PhysicFrame();
}

physbody_t *Phys_CreateBody( sv_edict_t *ed, void *buffer, vec3_t org, vec3_t ang )
{
	btRigidBody* body = g_PhysWorld->AddDynamicRigidBody( buffer, 0, 10.0f ); // 10 kg
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