/*
Copyright (c) 2003-2006 Gino van den Bergen / Erwin Coumans  http://continuousphysics.com/Bullet/

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/


#include "LinearMath/btIDebugDraw.h"
#include "BulletDynamics/Dynamics/btDynamicsWorld.h"
#include "BulletCollision/CollisionShapes/btCollisionShape.h"
#include "BulletCollision/CollisionShapes/btBoxShape.h"
#include "BulletCollision/CollisionShapes/btSphereShape.h"
#include "BulletCollision/CollisionShapes/btCompoundShape.h"
#include "BulletCollision/CollisionShapes/btUniformScalingShape.h"
#include "LinearMath/btQuickprof.h"
#include "LinearMath/btDefaultMotionState.h"
#include "btBulletWorld.h"

CM_Debug DebugDrawer;

btBulletPhysic::btBulletPhysic(): m_World(0), m_debugMode(0), m_WorldScale(METERS_PER_INCH)
{
	btVector3 m_worldAabbMin(-1000, -1000, -1000), m_worldAabbMax(1000, 1000, 1000);

	btDefaultCollisionConfiguration* collisionConfiguration = new btDefaultCollisionConfiguration();
	btDispatcher* dispatcher = new btCollisionDispatcher(collisionConfiguration);
	btBroadphaseInterface* pairCache = new btAxisSweep3(m_worldAabbMin, m_worldAabbMax);
	btConstraintSolver*	constraintSolver = new btSequentialImpulseConstraintSolver();

	m_World = new btDiscreteDynamicsWorld(dispatcher, pairCache, constraintSolver);
	m_World->getDispatchInfo().m_enableSPU = true;
	m_World->setGravity(btVector3(0, -9.8, 0));
	m_World->setDebugDrawer(&DebugDrawer);

	m_debugMode |= btIDebugDraw::DBG_DrawAabb;
	m_debugMode |= btIDebugDraw::DBG_DrawWireframe;
}

btBulletPhysic::~btBulletPhysic()
{
	DeleteAllBodies();
}

void btBulletPhysic::DeleteAllBodies( void )
{
	int numObj = m_World->getNumCollisionObjects();

	for(int i = numObj; i > 0; i--)
	{
		btCollisionObject* obj = m_World->getCollisionObjectArray()[i-1];
		btRigidBody* body = btRigidBody::upcast(obj);
		m_World->removeRigidBody( body );
		m_World->removeCollisionObject(obj);
	}
}

btRigidBody* btBulletPhysic::AddDynamicRigidBody(float mass, const btTransform& startTransform, btCollisionShape* shape)
{
	// rigidbody is dynamic if and only if mass is non zero, otherwise static
	if(!mass) return AddStaticRigidBody(startTransform, shape);

	btVector3	 localInertia(0,0,0);
	shape->calculateLocalInertia(mass, localInertia);
	btDefaultMotionState* m_State = new btDefaultMotionState( startTransform );
	btRigidBody* m_body = new btRigidBody(mass, m_State, shape, localInertia );
	m_World->addRigidBody(m_body);
	
	return m_body;
}

btRigidBody* btBulletPhysic::AddStaticRigidBody(const btTransform& startTransform,btCollisionShape* shape)
{
	btVector3 localInertia(0,0,0);
	btDefaultMotionState* m_State = new btDefaultMotionState(startTransform);
	btRigidBody* m_body = new btRigidBody(0.f, m_State, shape);
	m_World->addRigidBody(m_body);
	
	return m_body;
}

bool btBulletPhysic::ApplyTransform( btRigidBody* body)
{
	if(!body) return false;

	sv_edict_t *edict = GetUserData( body );
	if(!edict) return false;
	vec3_t origin, angles;

	GetTransform( body, origin, angles );

	pi.Transform( edict, origin, angles );
	return true;
}

void btBulletPhysic::PhysicFrame( void )
{
	if(m_World)
	{
		int numObjects = m_World->getNumCollisionObjects();
		for (int i = 0; i < numObjects; i++)
		{
			btCollisionObject* colObj = m_World->getCollisionObjectArray()[i];
			btRigidBody* body = btRigidBody::upcast(colObj);

			if(body && body->getMotionState()) ApplyTransform( body );
			else
			{
				// world
			}

			if(colObj->getActivationState() == 1) // active
			{
			}
			if(colObj->getActivationState() == 2) // sleeping
			{
			}
		}
	}
}

void btBulletPhysic::UpdateWorld( float timeStep )
{
	m_World->stepSimulation( 0.1 );

	if (m_World->getDebugDrawer())
		m_World->getDebugDrawer()->setDebugMode( m_debugMode );
}