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


#include "btBulletStudioLoader.h"
#include "btBulletWorld.h"
#include "btBulletDynamicsCommon.h"

void StudioLoader::SetUpTransform ( void )
{
	btVector3 modelpos;

	mstudioseqdesc_t	*pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex);

	// calculate center
	btVector3 mins( pseqdesc[m_Anim].bbmin[0], pseqdesc[m_Anim].bbmin[1], pseqdesc[m_Anim].bbmin[2] );
	btVector3 maxs( pseqdesc[m_Anim].bbmax[0], pseqdesc[m_Anim].bbmax[1], pseqdesc[m_Anim].bbmax[2] );
	modelpos = (mins + maxs) * 0.5f;

	// setup matrix
	m_pRotationMatrix[0][0] = 1;
	m_pRotationMatrix[1][0] = 0;
	m_pRotationMatrix[2][0] = 0;

	m_pRotationMatrix[0][1] = 0;
	m_pRotationMatrix[1][1] = 1;
	m_pRotationMatrix[2][1] = 0;

	m_pRotationMatrix[0][2] = 0;
	m_pRotationMatrix[1][2] = 0;
	m_pRotationMatrix[2][2] = -1;

	m_pRotationMatrix[0][3] = modelpos.getX();
	m_pRotationMatrix[1][3] = modelpos.getY();
	m_pRotationMatrix[2][3] = 0;
}

void StudioLoader::SetupBones( void )
{
	mstudiobone_t	*pbones;
	matrix3x4		bonematrix;

	CalcRotations( pos, q );
	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	for (int i = 0; i < m_pStudioHeader->numbones; i++) 
	{
		QuaternionMatrix( q[i], bonematrix );

		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if (pbones[i].parent == -1) 
			R_ConcatTransforms (m_pRotationMatrix, bonematrix, m_pBonesTransform[i]);
		else R_ConcatTransforms( m_pBonesTransform[pbones[i].parent], bonematrix, m_pBonesTransform[i]);
	}
}

void StudioLoader::CalcBoneQuaterion( mstudiobone_t *pbone, float *q )
{
	int	i;
	vec3_t	angle1;

	for (i = 0; i < 3; i++) 
		angle1[i] = pbone->value[i+3];
	AngleQuaternion( angle1, q );
}

void StudioLoader::CalcBonePosition( mstudiobone_t *pbone, float *pos )
{
	int	i;

	for (i = 0; i < 3; i++) pos[i] = pbone->value[i];
}

void StudioLoader::CalcRotations ( float pos[][3], vec4_t *q )
{
	int		i;
	mstudiobone_t	*pbone;

	pbone = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	for (i = 0; i < m_pStudioHeader->numbones; i++, pbone++ ) 
	{
		CalcBoneQuaterion( pbone, q[i] );
		CalcBonePosition( pbone, pos[i] );
	}
}

void StudioLoader::SetupModel ( int bodypart, int body )
{
	int index;

	if(bodypart > m_pStudioHeader->numbodyparts) bodypart = 0;
	m_pBodyPart = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + bodypart;

	index = body / m_pBodyPart->base;
	index = index % m_pBodyPart->nummodels;
	m_pSubModel = (mstudiomodel_t *)((byte *)m_pStudioHeader + m_pBodyPart->modelindex) + index;
}

void StudioLoader::GetVertices( void )
{
	int		i;
	vec3_t		*pstudioverts;
	byte		*pvertbone;

	pvertbone = ((byte *)m_pStudioHeader + m_pSubModel->vertinfoindex);
	pstudioverts = (vec3_t *)((byte *)m_pStudioHeader + m_pSubModel->vertindex);

	for (i = 0; i < m_pSubModel->numverts; i++)
	{
		VectorTransform(pstudioverts[i], m_pBonesTransform[pvertbone[i]], m_pVertsTransform[i]);
	}
	LookupMeshes();
}

void StudioLoader::AddMesh( int mesh )
{
	mstudiomesh_t	*pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + mesh;
	short		*ptricmds = (short *)((byte *)m_pStudioHeader + pmesh->triindex);
	int		i;

	while(i = *(ptricmds++))
	{
		if(i < 0) i = -i;
		for(; i > 0; i--, ptricmds += 4)
		{
			m_vertices.push_back( getPoint( *ptricmds ));
			m_numverts++;
		}
	}
}

void StudioLoader::LookupMeshes( void )
{
	int	i;

	for (i = 0; i < m_pSubModel->nummesh; i++) 
		AddMesh( i );
}

void StudioLoader::loadModel( void* memoryBuffer )
{
	// setup pointers
	m_pStudioHeader = (studiohdr_t *)memoryBuffer;
	m_pBodyPart = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex);
	m_BodyCount = m_pBodyPart->nummodels;
	m_Anim = 0;

	SetUpTransform();
	SetupBones();
}

btRigidBody *StudioLoader::buildCollisionTree( int bodynum, btScalar mass )
{
	btRigidBody*	StudioModel;
			
	// create new mesh buffer
	m_MeshBuffer = new btTriangleMesh();

	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		SetupModel( i, bodynum );
		GetVertices();
	}

	m_transform.setIdentity();
	m_CollisionTree = new btConvexHullShape(&(m_vertices[0].getX()), m_vertices.size());
	StudioModel = g_PhysWorld->AddDynamicRigidBody( mass, m_transform, m_CollisionTree );
	m_vertices.clear();

	return StudioModel;
}