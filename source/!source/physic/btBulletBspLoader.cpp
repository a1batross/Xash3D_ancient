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

void BspLoader::loadBSPFile( uint* memoryBuffer)
{
	dheader_t	*header = (dheader_t*)memoryBuffer;

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
	swapBSPFile();
}

btRigidBody *BspLoader::buildCollisionTree( int idx, btScalar mass )
{
	if(idx < 0 || idx > m_nummodels)
	{
		MsgDev(D_WARN, "BspLoader: bad inline model index %d\n", idx );
		return NULL;
	}

	BeginBuildTree();	
	
	for( int i = m_dmodels[idx].firstface; i < m_dmodels[idx].numfaces; i++ )
		AddFaceToTree( i );
	return EndBuildTree( true );
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

void BspLoader::swapBSPFile( void )
{
	swapBlock((int *)&m_dmodels[0], m_nummodels * sizeof( m_dmodels[0]));
	swapBlock((int *)&m_dplanes[0], m_numplanes * sizeof( m_dplanes[0]));
	swapBlock((int *)&m_dleafs[0], m_numleafs * sizeof( m_dleafs[0] ));
	swapBlock((int *)&m_vertices[0], m_numverts * sizeof( m_vertices[0]));
	swapBlock((int *)&m_edges[0], m_numedges * sizeof( m_edges[0]));
	swapBlock((int *)&m_surfedges[0], m_numsurfedges * sizeof( m_surfedges[0]));
	swapBlock((int *)&m_surfaces[0], m_numfaces * sizeof( m_surfaces[0]));
	swapBlock( (int *)&m_dleafbrushes[0], m_numleafbrushes * sizeof( m_dleafbrushes[0] ) );
	swapBlock( (int *)&m_dbrushes[0], m_numbrushes * sizeof( m_dbrushes[0]));
	swapBlock( (int *)&m_dbrushsides[0], m_numbrushsides * sizeof( m_dbrushsides[0]));
}