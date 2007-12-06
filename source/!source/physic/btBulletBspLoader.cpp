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

		// swap everything
		//swapBSPFile();

		return true;

	}
	return false;
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