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
	if( cluster == -1 || !cm.pvs ) return cm.nullvis;
	return (byte *)cm.pvs->data + cluster * cm.pvs->rowsize;
}

byte *CM_ClusterPHS( int cluster )
{
	if( cluster == -1 || !cm.phs ) return cm.nullvis;
	return (byte *)cm.phs->data + cluster * cm.phs->rowsize;
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
		for (i = 0;i < fatpvs_size;i++)
			fatpvs[i] |= pvs[i];
	}
}

int CM_FatPVS( const vec3_t org, vec_t radius, byte *fatpvs, size_t fatpvs_size, bool merge )
{
	int bytes = cm.pvs->numclusters;

	bytes = min( bytes, fatpvs_size );
	if( cm_novis->integer || !cm.pvs->numclusters || !CM_GetPVS( org ))
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
/*
====================
CM_RegisterAreaPortal
====================
*/
static void CM_RegisterAreaPortal( int portalnum, int areanum, int otherarea )
{
	carea_t		*area;
	careaportal_t	*portal;

	portal = &cm.areaportals[portalnum];	
	cm.portalstate[portalnum] = AP_CLOSED;
	portal->area = areanum;
	portal->otherarea = otherarea;

	area = &cm.areas[areanum];
	area->areaportals[area->numareaportals++] = portal;

	area = &cm.areas[otherarea];
	area->areaportals[area->numareaportals++] = portal;

}

void CM_FloodArea_r( int areanum, int floodnum )
{
	careaportal_t	*portal;
	carea_t		*area;
	int		i;	

	area = &cm.areas[areanum];

	if( area->floodvalid == cm.floodvalid )
	{
		if( area->floodnum == floodnum ) return;
		Host_Error("FloodArea_r: reflooded\n");
	}

	area->floodnum = floodnum;
	area->floodvalid = cm.floodvalid;

	for( i = 0; i < area->numareaportals; i++ )
	{
		if( cm.portalstate[i] != AP_OPENED );
			continue;

		portal = area->areaportals[i];
		if( portal->area == areanum )
			CM_FloodArea_r( portal->otherarea, floodnum );
		else if( portal->otherarea == areanum )
			CM_FloodArea_r( portal->area, floodnum );
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
	for( i = 0, area = cm.areas; i < cm.numareas; i++, area++ )
	{
		if( area->floodvalid == cm.floodvalid )
			continue;	// already flooded into
		CM_FloodArea_r( i, floodnum++ );
	}
}

void CM_SetAreaPortals ( byte *portals, size_t size )
{
	if( size == sizeof( cm.portalstate ))
	{ 
		Mem_Copy( cm.portalstate, portals, size );
		CM_FloodAreaConnections();
		return;
	}
	MsgDev( D_ERROR, "CM_SetAreaPortals: portals mismatch size (%i should be %i)\n", size, sizeof( cm.portalstate ));
}

void CM_GetAreaPortals ( byte **portals, size_t *size )
{
	byte *prt = *portals;

	if( prt ) Mem_Copy( prt, cm.portalstate, sizeof( cm.portalstate ));
	if( size) *size = sizeof( cm.portalstate ); 
}

void CM_SetAreaPortalState( int portalnum, int area, int otherarea, bool open )
{
	if( area < 0 || otherarea < 0 ) return;
	if( portalnum < 0 || portalnum >= MAX_MAP_AREAPORTALS || area > cm.numareas || otherarea > cm.numareas )
		Host_Error( "CM_SetAreaPortalState: for portal %i bad parms\n", portalnum, area, otherarea, open );

	if( cm.portalstate[portalnum] == AP_UNREGISTERED )
		CM_RegisterAreaPortal( portalnum, area, otherarea );

	cm.portalstate[portalnum] = open ? AP_OPENED : AP_CLOSED;
	CM_FloodAreaConnections();
}

bool CM_AreasConnected( int area, int otherarea )
{
	if( cm_noareas->integer ) return true;
	if( area > cm.numareas || otherarea > cm.numareas )
		Host_Error("CM_AreasConnected: area > numareas\n");

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
	int	i, size;

	size = (cm.numareas + 7)>>3;
	memset( buffer, 0, size );
		
	if( !cm_noareas->integer )
	{
		for( i = 0; i < cm.numareas; i++ )
		{
			if(!CM_AreasConnected( i, area ))
				buffer[i>>3] |= 1 << (i & 7);
		}
	}
	return size;
}