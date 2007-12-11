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

#ifndef BT_BULLET_STUDIO_H
#define BT_BULLET_STUDIO_H

#include "physic.h"
#include "btBulletDynamicsCommon.h"

class StudioLoader
{
protected:
	btVector3			m_point;
	btTriangleMesh*		m_MeshBuffer;
	btTransform		m_transform;
	btConvexHullShape*		m_CollisionTree;	
	studiohdr_t		*m_pStudioHeader;
	mstudiomodel_t		*m_pSubModel;
	mstudiobodyparts_t		*m_pBodyPart;
	matrix3x4			m_pRotationMatrix;
	vec3_t			m_pVertsTransform[MAXSTUDIOVERTS];
	matrix3x4			m_pBonesTransform[MAXSTUDIOBONES];

	float			pos[MAXSTUDIOBONES][3];
	vec4_t			q[MAXSTUDIOBONES];

	int			m_BodyCount;
	int			m_Anim;	// sequence number
	int			m_numverts;
private:
	btAlignedObjectArray<btVector3>	m_vertices;
	btVector3	getPoint( short index )
	{
		btScalar x = INCH2METER(m_pVertsTransform[index][0]); 
		btScalar y = INCH2METER(m_pVertsTransform[index][1]); 
		btScalar z = INCH2METER(m_pVertsTransform[index][2]); 
		m_point.setValue( x, y, z );
		return m_point;
	}

	void SetUpTransform ( void );
	void SetupBones( void );
	void CalcBoneQuaterion( mstudiobone_t *pbone, float *q );
	void CalcBonePosition( mstudiobone_t *pbone, float *pos );
	void CalcRotations ( float pos[][3], vec4_t *q );
	void SetupModel ( int bodypart, int body );
	void GetVertices( void );
	void LookupMeshes( void );
	void AddMesh( int mesh );
public:
	StudioLoader(){ } // constructor
	void loadModel( void* memoryBuffer );

	btRigidBody *buildCollisionTree( int bodynum = 0, btScalar mass = 0.0f );
};

#endif//BT_BULLET_STUDIO_H