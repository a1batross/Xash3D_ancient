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

#ifndef BSP_LOADER_H
#define BSP_LOADER_H

#include "LinearMath/btVector3.h"
#include "LinearMath/btAlignedObjectArray.h"
#include "LinearMath/btGeometryUtil.h"
#include "BulletCollision/CollisionShapes/btTriangleMesh.h"
#include "BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "btBulletWorld.h"

class BspLoader
{
	protected:
		btVector3				m_point;
		btTriangleMesh*			m_MeshBuffer;
		btTransform			m_transform;
		btBvhTriangleMeshShape*		m_CollisionTree;

		int 				m_nummodels;
		int				m_numleafs;
		int				m_numplanes;
		int				m_numverts;
		int				m_numedges;
		int				m_numsurfedges;
		int				m_numfaces;
		int				m_numleafbrushes;
		int				m_numbrushes;
		int				m_numbrushsides;

		btAlignedObjectArray<dmodel_t>	m_dmodels;
		btAlignedObjectArray<dleaf_t>		m_dleafs;
		btAlignedObjectArray<dplane_t>	m_dplanes;
		btAlignedObjectArray<dvertex_t>	m_vertices;
		btAlignedObjectArray<dedge_t>		m_edges;
		btAlignedObjectArray<int>		m_surfedges;
		btAlignedObjectArray<dface_t>		m_surfaces;
		btAlignedObjectArray<word>		m_dleafbrushes;
		btAlignedObjectArray<dbrush_t>	m_dbrushes;
		btAlignedObjectArray<dbrushside_t>	m_dbrushsides;

	private:
		// trimesh building private funcs
		btVector3 getPoint( int index )
		{
			int vert_index;
			int edge_index = m_surfedges[index];

			if(edge_index > 0) vert_index = m_edges[edge_index].v[0];
			else vert_index = m_edges[-edge_index].v[1];

			// get real vertex
			float *vec = m_vertices[vert_index].point;

			// rotate around 90 degrees and convert inches to meters
			m_point.setValue( INCH2METER(vec[0]), INCH2METER(vec[2]), INCH2METER(vec[1]));
			return m_point;
		}

		// triangle mesh concave hull method
		void BeginBuildTree( void )
		{
			m_MeshBuffer = new btTriangleMesh();			
		}

		void AddFaceToTree( int facenum )
		{
			const dface_t& pFace = m_surfaces[facenum];
			int k = pFace.firstedge;

			// convert polygon to triangles
			for(int j = 0; j < pFace.numedges - 2; j++)
				m_MeshBuffer->addTriangle(getPoint(k), getPoint(k+j+2), getPoint(k+j+1));
		}

		btRigidBody *EndBuildTree( bool is_world, btScalar mass = 0.0f )
		{
			btRigidBody*	bspModel;

			m_transform.setIdentity();
			m_CollisionTree = new btBvhTriangleMeshShape( m_MeshBuffer, true );

			if(is_world)
			{
				bspModel = g_PhysWorld->AddStaticRigidBody( m_transform, m_CollisionTree );
				bspModel->setCollisionFlags(bspModel->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
				bspModel->setCollisionFlags(bspModel->getCollisionFlags()  | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
			}
			else
			{
				// bsp submodels it's cinematic objects like doors, lifts e.t.c
				bspModel = g_PhysWorld->AddDynamicRigidBody( mass, m_transform, m_CollisionTree );
				if( mass ) bspModel->setCollisionFlags(bspModel->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
			}
			return bspModel;
		}
	public:
		BspLoader(){ } // constructor
		void loadBSPFile( uint* memoryBuffer );
		btRigidBody *buildCollisionTree( int modelnum = 0, btScalar mass = 0.0f );
		btBvhTriangleMeshShape* GetCollisionTree( void ){ return m_CollisionTree;}

		int copyLump( dheader_t *header, int lump, void *dest, int size );
		void swapBlock( int *block, int sizeOfBlock );
		void swapBSPFile( void );

};
#endif//BSP_LOADER_H