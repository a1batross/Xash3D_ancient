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

#include "physic.h"
#include "btBulletDynamicsCommon.h"

class BspLoader;
class StudioLoader;

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
	btDynamicsWorld*	m_World;		// dynamics world pointer		
	btRigidBody*	g_World;		// collision mesh world ptr
	btClock		m_clock;
	BspLoader*	g_Level;
	StudioLoader*	g_Studio;

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
	btRigidBody *AddDynamicRigidBody( int num, btScalar mass = 0.0f ); // collision shape is bsp data
	btRigidBody *AddDynamicRigidBody( void *buffer, int body, btScalar mass = 0.0f ); // collision shape is stduio model
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

	void MatrixAngles( const matrix4x4 matrix, vec3_t origin, vec3_t angles )
	{ 
		vec3_t	forward, right, up;
		float	xyDist;

		forward[0] = matrix[0][0];
		forward[1] = matrix[0][2];
		forward[2] = matrix[0][1];
		right[0] = matrix[1][0];
		right[1] = matrix[1][2];
		right[2] = matrix[1][1];
		up[2] = matrix[2][1];
	
		xyDist = sqrt( forward[0] * forward[0] + forward[1] * forward[1] );
	
		if ( xyDist > EQUAL_EPSILON )	// enough here to get angles?
		{
			angles[1] = RAD2DEG( atan2( forward[1], forward[0] ));
			angles[0] = RAD2DEG( atan2( -forward[2], xyDist ));
			angles[2] = RAD2DEG( atan2( -right[2], up[2] )) + 180;
		}
		else
		{
			angles[1] = RAD2DEG( atan2( right[0], -right[1] ) );
			angles[0] = RAD2DEG( atan2( -forward[2], xyDist ) );
			angles[2] = 180;
		}
		VectorCopy(matrix[3], origin );// extract origin
		ConvertPositionToGame( origin );
	}

	void AnglesMatrix(const vec3_t origin, const vec3_t angles, matrix4x4 matrix )
	{
		float		angle;
		float		sr, sp, sy, cr, cp, cy;

		angle = DEG2RAD(angles[YAW]);
		sy = sin(angle);
		cy = cos(angle);
		angle = DEG2RAD(angles[PITCH]);
		sp = sin(angle);
		cp = cos(angle);

		MatrixLoadIdentity( matrix );

		// forward
		matrix[0][0] = cp*cy;
		matrix[0][2] = cp*sy;
		matrix[0][1] = -sp;

		if (angles[ROLL] == 180)
		{
			angle = DEG2RAD(angles[ROLL]);
			sr = sin(angle);
			cr = cos(angle);

			// right
			matrix[1][0] = -1*(sr*sp*cy+cr*-sy);
			matrix[1][2] = -1*(sr*sp*sy+cr*cy);
			matrix[1][1] = -1*(sr*cp);

			// up
			matrix[2][0] = (cr*sp*cy+-sr*-sy);
			matrix[2][2] = (cr*sp*sy+-sr*cy);
			matrix[2][1] = cr*cp;
		}
		else
		{
			// right
			matrix[1][0] = sy;
			matrix[1][2] = -cy;
			matrix[1][1] = 0;

			// up
			matrix[2][0] = (sp*cy);
			matrix[2][2] = (sp*sy);
			matrix[2][1] = cp;
		}
		VectorCopy(origin, matrix[3] ); // pack origin
		ConvertPositionToPhysic( matrix[3] );
	}

	// set body angles and origin
	void GetOrigin(btRigidBody* body, vec3_t origin )
	{
		btVector3 pos = body->getWorldTransform().getOrigin();
		VectorSet( origin, METER2INCH( pos.getX()), METER2INCH( pos.getZ()), METER2INCH( pos.getY()));		
	}

	void GetAngles(btRigidBody* body, vec3_t angles )
	{
		body->getWorldTransform().getEuler( angles[0], angles[1], angles[2] );
		VectorSet( angles, RAD2DEG(angles[2]), RAD2DEG(angles[1]), RAD2DEG(angles[0]));
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

		AnglesMatrix( origin, angles, matrix );
		body->getWorldTransform().setFromOpenGLMatrix((float *)matrix);
	}

	// get body angles and origin
	void GetTransform( btRigidBody* body, vec3_t origin, vec3_t angles )
	{
		matrix4x4 matrix;

		body->getWorldTransform().getOpenGLMatrix((float *)matrix);
		MatrixAngles( matrix, origin, angles );
	}

	// bsploader operations
	void LoadWorld( uint *buffer );
	void SaveWorld( void );
	void FreeWorld( void );

	void UpdateWorld( float timescale );	// update simulation state
	void DeleteAllBodies( void );		// clear scene
	void PhysicFrame( void );		// update entities transform
	bool ApplyTransform( btRigidBody* body);// apply transform to current body
};

extern btBulletPhysic *g_PhysWorld;
extern void* Palloc( int size );
extern void Pfree( void *ptr );

#endif//BT_BULLET_WORLD_H