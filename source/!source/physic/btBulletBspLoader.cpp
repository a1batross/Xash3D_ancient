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

#include "btBulletBspLoader.h"
#include "btBulletWorld.h"
#include "btBulletDynamicsCommon.h"

int extrasize = 100;

bool BspLoader::loadBSPFile( uint* memoryBuffer)
{
	dheader_t	*header = (dheader_t*)memoryBuffer;

	// load the file header
	if (header)
	{
		// swap the header
		swapBlock( (int *)header, sizeof(*header));
		
		int length = (header->lumps[LUMP_MODELS].filelen) / sizeof(dmodel_t);
		m_dmodels.resize(length + extrasize);
		m_nummodels = copyLump( header, LUMP_MODELS, &m_dmodels[0], sizeof(dmodel_t));

		length = (header->lumps[LUMP_PLANES].filelen) / sizeof(dplane_t);
		m_dplanes.resize(length + extrasize);
		m_numplanes = copyLump( header, LUMP_PLANES, &m_dplanes[0], sizeof(dplane_t));

		length = (header->lumps[LUMP_LEAFS].filelen) / sizeof(dleaf_t);
		m_dleafs.resize(length + extrasize);
		m_numleafs = copyLump( header, LUMP_LEAFS, &m_dleafs[0], sizeof(dleaf_t));

		length = (header->lumps[LUMP_NODES].filelen) / sizeof(dnode_t);
		m_dnodes.resize(length + extrasize);
		m_numnodes = copyLump( header, LUMP_NODES, &m_dnodes[0], sizeof(dnode_t));

		length = (header->lumps[LUMP_LEAFFACES].filelen) / sizeof(m_dleafsurfaces[0]);
		m_dleafsurfaces.resize(length + extrasize);
		m_numleafsurfaces = copyLump( header, LUMP_LEAFFACES, &m_dleafsurfaces[0], sizeof(m_dleafsurfaces[0]));

		length = (header->lumps[LUMP_LEAFBRUSHES].filelen) / sizeof(m_dleafbrushes[0]) ;
		m_dleafbrushes.resize(length+extrasize);
		m_numleafbrushes = copyLump( header, LUMP_LEAFBRUSHES, &m_dleafbrushes[0], sizeof(m_dleafbrushes[0]));

		length = (header->lumps[LUMP_BRUSHES].filelen) / sizeof(dbrush_t);
		m_dbrushes.resize(length+extrasize);
		m_numbrushes = copyLump( header, LUMP_BRUSHES, &m_dbrushes[0], sizeof(dbrush_t));

		length = (header->lumps[LUMP_BRUSHSIDES].filelen) / sizeof(dbrushside_t);
		m_dbrushsides.resize(length+extrasize);
		m_numbrushsides = copyLump( header, LUMP_BRUSHSIDES, &m_dbrushsides[0], sizeof(dbrushside_t));

		length = (header->lumps[LUMP_VERTEXES].filelen) / sizeof(dvertex_t);
		m_vertices.resize(length + extrasize);
		m_numverts = copyLump( header, LUMP_VERTEXES, &m_vertices[0], sizeof(dvertex_t));

		length = (header->lumps[LUMP_EDGES].filelen) / sizeof(dedge_t);
		m_edges.resize(length + extrasize);
		m_numedges = copyLump( header, LUMP_EDGES, &m_edges[0], sizeof(dedge_t));

		length = (header->lumps[LUMP_SURFEDGES].filelen) / sizeof(m_surfedges[0]);
		m_surfedges.resize(length + extrasize);
		m_numsurfedges = copyLump( header, LUMP_SURFEDGES, &m_surfedges[0], sizeof(m_surfedges[0]));

		length = (header->lumps[LUMP_FACES].filelen) / sizeof(dface_t);
		m_surfaces.resize(length + extrasize);
		m_numfaces = copyLump( header, LUMP_FACES, &m_surfaces[0], sizeof(dface_t));

		// swap everything
		//swapBSPFile();

		return true;

	}
	return false;
}

void BspLoader::buildTriMesh( void )
{
	int vertStride = 3 * sizeof(btScalar);
	int indexStride = 3 * sizeof(int);
	int k;

	// convert from tri-fans to tri-list
	m_numTris = 0;

	// get number of triangles
	for(int i = 0; i < m_numfaces; i++)
	{
		if(m_surfaces[i].numedges)
		{ 
			m_numTris += m_surfaces[i].numedges - 2;
			Msg("face [%i] numverts[%i]\n", i, m_surfaces[i].numedges );
		}
	}

	g_vertices = new btScalar[m_numverts * 3];
	g_indices = new int[m_numTris * 3];

	// transform all vertices
	for( i = 0, k = 0; i < m_numverts; i++)
	{
		vec3_t temp;
		VectorCopy(m_vertices[i].point, temp );
		ConvertPositionToPhysic( temp );
		g_vertices[k++] = temp[0];
		g_vertices[k++] = temp[1];
		g_vertices[k++] = temp[2];
	}
	Msg("total verts %d\n", k );

	// build indices list
	for(k = 0, i = 0; i < m_numfaces; i++)
	{
		const dface_t& pFace = m_surfaces[i];
		int startVertex = pFace.firstedge;

		for(int j = 0; j < pFace.numedges - 2; j++)
		{
			int lindex = m_surfedges[startVertex + j];
			int realIndex;

			if(lindex > 0) realIndex = m_edges[lindex].v[0];
			else realIndex = m_edges[-lindex].v[1];

			g_indices[k++] = realIndex;
			g_indices[k++] = realIndex + j + 2;
			g_indices[k++] = realIndex + j + 1;
		}
	}
	Msg("total verts %d\n", k );

	btTriangleIndexVertexArray* indexVertexArrays = new btTriangleIndexVertexArray(m_numTris, g_indices, indexStride, m_numverts, &g_vertices[0], vertStride);
	CollisionTree = new btBvhTriangleMeshShape( indexVertexArrays, true );

	btTransform	startTransform;	
	btRigidBody*	staticBody;

	startTransform.setIdentity();
	staticBody = g_PhysWorld->AddStaticRigidBody( startTransform, CollisionTree );
	staticBody->setCollisionFlags(staticBody->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
	staticBody->setCollisionFlags(staticBody->getCollisionFlags()  | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
}

void BspLoader::convertBsp( float scale )
{
	for (int i = 0; i < m_numleafs; i++)
	{
		Msg("Reading bspLeaf %i from total %i (%f procent)\n", i, m_numleafs, (100.f*(float)i/float(m_numleafs)));
			
		bool isValidBrush = false;
		dleaf_t&	leaf = m_dleafs[i];
	
		for (int b = 0; b < leaf.numleafbrushes; b++)
		{
			btAlignedObjectArray<btVector3>	planeEquations;

			int brushid = m_dleafbrushes[leaf.firstleafbrush + b];
			dbrush_t& brush = m_dbrushes[brushid];
			if(brush.contents & CONTENTS_SOLID)
			{
				for (int p = 0; p < brush.numsides; p++)
				{
					int sideid = brush.firstside + p;
					dbrushside_t& brushside = m_dbrushsides[sideid];
					int planeid = brushside.planenum;
					dplane_t& plane = m_dplanes[planeid];
					btVector3 planeEq;
					planeEq.setValue( plane.normal[0], plane.normal[2], plane.normal[1], scale * -plane.dist);
					planeEquations.push_back(planeEq);
					isValidBrush = true;
				}
				if (isValidBrush)
				{
					btAlignedObjectArray<btVector3>	vertices;
					btGeometryUtil::getVerticesFromPlaneEquations(planeEquations, vertices);
					addConvexVerticesCollider( vertices );
				}
			}
		}
	}
}

void BspLoader::addConvexVerticesCollider( btAlignedObjectArray<btVector3>& vertices )
{
	// perhaps we can do something special with entities (isEntity)
	// like adding a collision Triggering (as example)
			
	if (vertices.size() > 0)
	{
		btTransform startTransform;
		startTransform.setIdentity();
		startTransform.setOrigin(btVector3(0, 0, 0));

		// this create an internal copy of the vertices
		btCollisionShape* shape = new btConvexHullShape(&(vertices[0].getX()), vertices.size());
		g_PhysWorld->AddStaticRigidBody(startTransform, shape);
	}
}

int BspLoader::copyLump( dheader_t *header, int lump, void *dest, int size )
{
	int length, ofs;

	length = header->lumps[lump].filelen;
	ofs = header->lumps[lump].fileofs;
	Mem_Copy( dest, (byte *)header + ofs, length );

	return length / size;
}

void BspLoader::swapBlock( int *block, int sizeOfBlock )
{
	int		i;

	sizeOfBlock >>= 2;
	for ( i = 0; i < sizeOfBlock; i++ )
	{
		block[i] = LittleLong( block[i] );
	}
}