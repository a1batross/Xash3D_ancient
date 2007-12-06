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

#include "LinearMath/btAlignedObjectArray.h"
#include "LinearMath/btVector3.h"
#include "LinearMath/btGeometryUtil.h"
#include "physic.h"

class BspLoader
{
	public:
		BspLoader(){ } // constructor

		bool loadBSPFile( uint* memoryBuffer);
		void convertBsp( float scale );

		int copyLump( dheader_t *header, int lump, void *dest, int size );
		void swapBlock( int *block, int sizeOfBlock );
		void addConvexVerticesCollider( btAlignedObjectArray<btVector3>& vertices );
		
		int 				m_nummodels;
		btAlignedObjectArray<dmodel_t>	m_dmodels;

		int				m_numleafs;
		btAlignedObjectArray<dleaf_t>		m_dleafs;

		int				m_numplanes;
		btAlignedObjectArray<dplane_t>	m_dplanes;

		int				m_numnodes;
		btAlignedObjectArray<dnode_t>		m_dnodes;

		int				m_numleafsurfaces;
		btAlignedObjectArray<short>		m_dleafsurfaces;

		int				m_numleafbrushes;
		btAlignedObjectArray<word>		m_dleafbrushes;

		int				m_numbrushes;
		btAlignedObjectArray<dbrush_t>	m_dbrushes;

		int				m_numbrushsides;
		btAlignedObjectArray<dbrushside_t>	m_dbrushsides;

		int				m_numPhysSurfaces;
		btAlignedObjectArray<csurface_t>	m_physSurfaces;
};
#endif//BSP_LOADER_H