//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_portals.c - server visibility
//=======================================================================

#include "cm_local.h"

static byte fatpvs[MAX_MAP_LEAFS/8];

/*
===============================================================================

PVS / PHS

===============================================================================
*/
/*
===================
CM_DecompressVis
===================
*/
void CM_DecompressVis( byte *in, byte *out )
{
	byte	*out_p;
	int	c, row;

	row = (cm.numclusters + 7)>>3;	
	out_p = out;

	if( !in )
	{	
		// no vis info, so make all visible
		while( row )
		{
			*out_p++ = 0xff;
			row--;
		}
		return;		
	}
	do
	{
		if(*in)
		{
			*out_p++ = *in++;
			continue;
		}
	
		c = in[1];
		in += 2;
		if((out_p - out) + c > row)
		{
			c = row - (out_p - out);
			MsgDev(D_WARN, "CM_DecompressVis: decompression overrun\n");
		}
		while( c )
		{
			*out_p++ = 0;
			c--;
		}
	} while( out_p - out < row );
}

byte *CM_ClusterPVS( int cluster )
{
	if( cluster < 0 || cluster >= cm.numclusters || !cm.vis )
		Mem_Set( cms.pvsrow, 0xFF, (cm.numclusters + 31) & ~31 );
	else CM_DecompressVis( cm.visbase + cm.vis->bitofs[cluster][DVIS_PVS], cms.pvsrow );
	return cms.pvsrow;
}

byte *CM_ClusterPHS( int cluster )
{
	if( cluster < 0 || cluster >= cm.numclusters || !cm.vis )
		Mem_Set( cms.phsrow, 0xFF, (cm.numclusters + 31) & ~31 );
	else CM_DecompressVis( cm.visbase + cm.vis->bitofs[cluster][DVIS_PHS], cms.phsrow );
	return cms.phsrow;
}

/*
============
CM_FatPVS

The client will interpolate the view position,
so we can't use a single PVS point
===========
*/
byte *CM_FatPVS( const vec3_t org, bool portal )
{
	int	leafs[128];
	int	i, j, count;
	int	longs;
	byte	*src;
	vec3_t	mins, maxs;
	int	snap = portal ? 1 : 8;

	for( i = 0; i < 3; i++ )
	{
		mins[i] = org[i] - snap;
		maxs[i] = org[i] + snap;
	}

	count = CM_BoxLeafnums( mins, maxs, leafs, 128, NULL );
	if( count < 1 ) Host_Error( "CM_FatPVS: invalid leafnum count\n" );
	longs = (CM_NumClusters() + 31)>>5;

	// convert leafs to clusters
	for( i = 0; i < count; i++ ) leafs[i] = CM_LeafCluster( leafs[i] );

	if( !portal ) Mem_Copy( fatpvs, CM_ClusterPVS( leafs[0] ), longs<<2 );

	// or in all the other leaf bits
	for( i = portal ? 0 : 1; i < count; i++ )
	{
		for( j = 0; j < i; j++ )
		{
			if( leafs[i] == leafs[j] )
				break;
		}

		if( j != i ) continue;	// already have the cluster we want
		src = CM_ClusterPVS( leafs[i] );
		for( j = 0; j < longs; j++ ) ((long *)fatpvs)[j] |= ((long *)src)[j];
	}

	return fatpvs;
}

/*
===============================================================================

AREAPORTALS

===============================================================================
*/
void CM_FloodArea_r( carea_t *area, int floodnum )
{
	int		i;
	dareaportal_t	*p;

	if( area->floodvalid == cm.floodvalid )
	{
		if( area->floodnum == floodnum ) return;
		Host_Error( "CM_FloodArea_r: reflooded\n" );
	}

	area->floodnum = floodnum;
	area->floodvalid = cm.floodvalid;
	p = &cm.areaportals[area->firstareaportal];

	for( i = 0; i < area->numareaportals; i++, p++ )
	{
		if( cms.portalopen[p->portalnum] )
			CM_FloodArea_r( &cm.areas[p->otherarea], floodnum );
	}
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
	for( i = 1; i < cm.numareas; i++ )
	{
		area = &cm.areas[i];
		if( area->floodvalid == cm.floodvalid )
			continue;	// already flooded into
		floodnum++;
		CM_FloodArea_r( area, floodnum );
	}
}

void CM_SetAreaPortals ( byte *portals, size_t size )
{
	if( size == sizeof( cms.portalopen ))
	{ 
		Mem_Copy( cms.portalopen, portals, size );
		CM_FloodAreaConnections();
		return;
	}
	MsgDev( D_ERROR, "CM_SetAreaPortals: portals mismatch size (%i should be %i)\n", size, cm.numareaportals );
}

void CM_GetAreaPortals ( byte **portals, size_t *size )
{
	byte *prt = *portals;

	if( prt ) Mem_Copy( prt, cms.portalopen, sizeof( cms.portalopen ));
	if( size) *size = sizeof( cms.portalopen ); 
}

void CM_SetAreaPortalState( int portalnum, bool open )
{
	if( portalnum > cm.numareaportals )
		Host_Error( "CM_SetAreaPortalState: areaportal > numareaportals\n" );

	cms.portalopen[portalnum] = open;
	CM_FloodAreaConnections();
}

bool CM_AreasConnected( int area, int otherarea )
{
	if( cm_noareas->integer ) return true;
	if( area > cm.numareas || otherarea > cm.numareas )
		Host_Error("CM_AreasConnected: area >= cm.numareas\n" );

	if( !area || !otherarea ) return false;
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
int CM_WriteAreaBits( byte *buffer, int area, bool portal )
{
	int	i, size;

	size = (cm.numareas + 7)>>3;

	if( cm_noareas->integer )
	{
		if( !portal ) Mem_Set( buffer, 0xFF, size );
	}
	else		
	{
		if( !portal ) Mem_Set( buffer, 0x00, size );
		for( i = 0; i < cm.numareas; i++ )
		{
			if(CM_AreasConnected( i, area ) || !area )
			{
				if( !portal ) buffer[i>>3] |= 1 << (i & 7);
				else buffer[i>>3] |= 1 << (i & 7) ^ ~0;
			}
		}
	}
	return size;
}

/*
=============
CM_HeadnodeVisible

Returns true if any leaf under headnode has a cluster that
is potentially visible
=============
*/
bool CM_HeadnodeVisible( int nodenum, byte *visbits )
{
	int	leafnum, cluster;
	cnode_t	*node;

	if( nodenum < 0 )
	{
		leafnum = -1-nodenum;
		cluster = CM_LeafCluster( leafnum );
		if( cluster == -1 ) return false;
		if( visbits[cluster>>3] & (1<<(cluster&7)))
			return true;
		return false;
	}

	if( nodenum < 0 || nodenum >= cm.numnodes )
		Host_Error( "CM_HeadnodeVisible: bad number %i >= %i\n", nodenum, cm.numnodes );
	
	node = cm.nodes + nodenum;
	if( CM_HeadnodeVisible( node->children[0] - cm.nodes, visbits ))
		return true;
	return CM_HeadnodeVisible( node->children[1] - cm.nodes, visbits );
}