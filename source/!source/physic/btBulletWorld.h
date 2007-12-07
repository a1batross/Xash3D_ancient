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

#ifndef BT_BULLET_WORLD_H
#define BT_BULLET_WORLD_H

#include "btBulletBspLoader.h"
#include "btBulletDynamicsCommon.h"

class CM_Debug : public btIDebugDraw
{
	int	m_debugMode;

public:
	virtual void drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
	{
		if(m_debugMode > 0)
		{
			vec3_t start, end, col;

			// convert to Xash vec3_t
			VectorSet( start, from.getX(), from.getY(), from.getZ());
			VectorSet( end, to.getX(), to.getY(), to.getZ());
			VectorSet( col, color.getX(), color.getY(), color.getZ());

			TransformRGB( col, col );
			ConvertPositionToGame( start );
			ConvertPositionToGame( end );

			pi.DrawLine( col, start, end );
		}
	}

	virtual void drawContactPoint(const btVector3& pointOnB,const btVector3& normalOnB,btScalar distance,int lifeTime,const btVector3& color)
	{
		if (m_debugMode & btIDebugDraw::DBG_DrawContactPoints)
		{
			btVector3 to = pointOnB+normalOnB*distance;
			const btVector3&from = pointOnB;
			vec3_t start, end, col;

			// convert to Xash vec3_t
			VectorSet( start, from.getX(), from.getY(), from.getZ());
			VectorSet( end, to.getX(), to.getY(), to.getZ());
			VectorSet( col, color.getX(), color.getY(), color.getZ());

			TransformRGB( col, col );
			ConvertPositionToGame( start );
			ConvertPositionToGame( end );

			pi.DrawLine( col, start, end );
		}
	}

	virtual void reportErrorWarning(const char* warningString)
	{
		MsgWarn( warningString ); 
	}

	virtual void setDebugMode(int debugMode) { m_debugMode = debugMode; }
	virtual int getDebugMode() const { return m_debugMode;}
};

class btBulletPhysic
{
protected:
	// physic world
	btDynamicsWorld*	m_World;		
	btClock		m_clock;

	int		m_debugMode;	// replace with cvar
	float		m_WorldScale;	// convert units to meters
public:
		
	btBulletPhysic();
	virtual ~btBulletPhysic();

	btDynamicsWorld* getWorld( void )
	{
		return m_World;
	}

	int getDebugMode( void )
	{
		return m_debugMode ;
	}

	void setDebugMode( int mode )
	{
		m_debugMode = mode;
	}

	void DrawDebug( void )
	{
		m_World->DrawDebug();
	}

	float getScale( void )
	{
		return m_WorldScale;
	}
	
	btRigidBody* AddStaticRigidBody(const btTransform& startTransform,btCollisionShape* shape); // bodies with infinite mass
	btRigidBody* AddDynamicRigidBody(float mass, const btTransform& startTransform,btCollisionShape* shape);
	void DelRigidBody(btRigidBody* body)
	{
		if( body )m_World->removeRigidBody( body );
	} 

	void SetUserData( btRigidBody* body, sv_edict_t *entity )
	{
		body->setUserPointer( entity );
	}

	sv_edict_t *GetUserData( btRigidBody* body )
	{
		return (sv_edict_t *)body->getUserPointer();
	}

	// set body angles and origin
	void SetOrigin(btRigidBody* body, const vec3_t origin )
	{
		btVector3 pos(INCH2METER(origin[0]), INCH2METER(origin[2]), INCH2METER(origin[1]));

		btTransform worldTrans = body->getWorldTransform();
		worldTrans.setOrigin(pos);
		body->setWorldTransform(worldTrans);
	}

	void SetAngles(btRigidBody* body, const vec3_t angles )
	{
		btQuaternion	q;
		q.setEuler(angles[YAW], angles[PITCH], angles[ROLL]);

		btTransform worldTrans = body->getWorldTransform();
		worldTrans.setRotation(q);
		body->setWorldTransform(worldTrans);
	}

	void SetTransform( btRigidBody* body, vec3_t origin, vec3_t angles )
	{
		matrix4x4 matrix;

		MatrixLoadIdentity( matrix );
		AnglesMatrix( origin, angles, matrix );
		ConvertPositionToPhysic( matrix[3] );
		body->getWorldTransform().setFromOpenGLMatrix((float *)matrix);
	}

	// get body angles and origin
	void GetTransform( btRigidBody* body, vec3_t origin, vec3_t angles )
	{
		matrix4x4 matrix;

		body->getWorldTransform().getOpenGLMatrix((float *)matrix);
		MatrixAngles( matrix, origin, angles );
		ConvertPositionToGame( origin );
	}

	void UpdateWorld( float timescale );	// update simulation state
	void DeleteAllBodies( void );		// clear scene
	void PhysicFrame( void );		// update entities transform
	bool ApplyTransform( btRigidBody* body);// apply transform to current body
};

extern btBulletPhysic *g_PhysWorld;
extern void* Palloc( int size );
extern void Pfree( void *ptr );

#endif//BT_BULLET_WORLD_H