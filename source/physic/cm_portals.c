//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_portals.c - server visibility
//=======================================================================

#include "cm_local.h"

/*
===============================================================================

PVS / PHS

===============================================================================
*/
byte *CM_ClusterPVS( int cluster )
{
	if( cluster < 0 || cluster >= cm.pvs.numClusters || !cm.vised )
		return cm.pvs.data;
	return (byte *)cm.pvs.data + cluster * cm.pvs.clusterBytes;
}

byte *CM_ClusterPHS( int cluster )
{
	if( cluster < 0 || cluster >= cm.phs.numClusters || !cm.vised )
		return cm.phs.data;
	return (byte *)cm.phs.data + cluster * cm.phs.clusterBytes;
}

static byte *CM_GetPVS( const vec3_t p )
{
	cnode_t *node = cm.nodes;

	while( node->plane )
		node = node->children[(node->plane->type < 3 ? p[node->plane->type] : DotProduct(p, node->plane->normal)) < node->plane->dist];
	if(((cleaf_t *)node)->cluster >= 0 )
		return CM_ClusterPVS( ((cleaf_t *)node)->cluster );
	return NULL;
}


static void CM_FatPVS_RecursiveBSPNode( const vec3_t org, float radius, byte *fatpvs, size_t fatpvs_size, cnode_t *node )
{
	while( node->plane )
	{
		float d = PlaneDiff( org, node->plane );
		if( d > radius ) node = node->children[0];
		else if( d < -radius ) node = node->children[1];
		else
		{
			// go down both sides
			CM_FatPVS_RecursiveBSPNode( org, radius, fatpvs, fatpvs_size, node->children[0] );
			node = node->children[1];
		}
	}
	// if this leaf is in a cluster, accumulate the pvs bits
	if(((cleaf_t *)node)->cluster >= 0)
	{
		int i;
		byte *pvs = CM_ClusterPVS(((cleaf_t *)node)->cluster ); 
		for( i = 0; i < fatpvs_size; i++ ) fatpvs[i] |= pvs[i];
	}
}

int CM_FatPVS( const vec3_t org, vec_t radius, byte *fatpvs, size_t fatpvs_size, bool merge )
{
	int bytes = cm.pvs.numClusters;

	bytes = min( bytes, fatpvs_size );
	if( cm_novis->integer || !cm.vised || !CM_GetPVS( org ))
	{
		memset( fatpvs, 0xFF, bytes );
		return bytes;
	}
	if( !merge ) memset( fatpvs, 0, bytes );
	CM_FatPVS_RecursiveBSPNode( org, radius, fatpvs, bytes, cm.nodes );
	return bytes;
}

/*
===============================================================================

AREAPORTALS

===============================================================================
*/
void CM_FloodArea_r( int areanum, int floodnum )
{
	carea_t		*area;
	int		i, *con;	

	area = &cm.areas[areanum];

	if( area->floodvalid == cm.floodvalid )
	{
		if( area->floodnum == floodnum ) return;
		Host_Error("FloodArea_r: reflooded\n");
	}

	area->floodnum = floodnum;
	area->floodvalid = cm.floodvalid;
	con = cm.areaportals + areanum * cm.numareas;

	for( i = 0; i < cm.numareas; i++ )
		if( con[i] > 0 ) CM_FloodArea_r( i, floodnum );
}

/*
====================
CM_FloodAreaConnections
====================
*/
void CM_FloodAreaConnections( void )
{
	carea_t		*area;
	int		i, floodnum = 0;

	// all current floods are now invalid
	cm.floodvalid++;

	// area 0 is not used
	for( i = 0, area = cm.areas; i < cm.numareas; i++, area++ )
	{
		if( area->floodvalid == cm.floodvalid )
			continue;	// already flooded into
		floodnum++;
		CM_FloodArea_r( i, floodnum );
	}
}

void CM_SetAreaPortals ( byte *portals, size_t size )
{
	if( size == cm.areaportals_size )
	{ 
		Mem_Copy( cm.areaportals, portals, cm.areaportals_size );
		CM_FloodAreaConnections();
		return;
	}
	MsgDev( D_ERROR, "CM_SetAreaPortals: portals mismatch size (%i should be %i)\n", size, cm.areaportals_size );
}

void CM_GetAreaPortals ( byte **portals, size_t *size )
{
	byte *prt = *portals;

	if( prt ) Mem_Copy( prt, cm.areaportals, cm.areaportals_size );
	if( size) *size = cm.areaportals_size; 
}

void CM_SetAreaPortalState( int area1, int area2, bool open )
{
	if( area1 < 0 || area2 < 0 ) return;
	if( area1 >= cm.numareas || area2 >= cm.numareas )
		Host_Error( "CM_SetAreaPortalState: bad area numbers %i or %i\n", area1, area2 );

	if( open )
	{
		cm.areaportals[area1*cm.numareas+area2]++;
		cm.areaportals[area2*cm.numareas+area1]++;
	}
	else
	{
		cm.areaportals[area1*cm.numareas+area2]--;
		cm.areaportals[area2*cm.numareas+area1]--;
		if(cm.areaportals[area2*cm.numareas+area1] < 0 )
			Host_Error( "CM_SetAreaPortalState: negative reference count\n" );
	}
	CM_FloodAreaConnections();
}

bool CM_AreasConnected( int area, int otherarea )
{
	if( cm_noareas->integer ) return true;
	if( area > cm.numareas || otherarea > cm.numareas )
		Host_Error("CM_AreasConnected: area >= cm.numareas\n" );

	if( area < 0 || otherarea < 0 ) return false;
	if( area == otherarea ) return true; // quick test

	if( cm.areas[area].floodnum == cm.areas[otherarea].floodnum )
		return true;
	return false;
}

/*
=================
CM_WriteAreaBits

Writes a length byte followed by a bit vector of all the areas
that area in the same flood as the area parameter
This is used by the client refreshes to cull visibility
=================
*/
int CM_WriteAreaBits( byte *buffer, int area )
{
	int	i, size, floodnum;

	size = (cm.numareas + 7)>>3;

	if( cm_noareas->integer || area == -1 )
	{
		memset( buffer, 0xFF, size );
	}
	else		
	{
		floodnum = cm.areas[area].floodnum;
		for( i = 0; i < cm.numareas; i++ )
		{
			if( cm.areas[i].floodnum == floodnum || area == -1 )
				buffer[i>>3] |= 1 << (i & 7);
		}
	}
	return size;
}