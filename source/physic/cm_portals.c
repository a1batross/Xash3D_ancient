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
		memset( cm.pvsrow, 0xFF, (cm.numclusters + 31) & ~31 );
	else CM_DecompressVis( cm.visbase + cm.vis->bitofs[cluster][DVIS_PVS], cm.pvsrow );
	return cm.pvsrow;
}

byte *CM_ClusterPHS( int cluster )
{
	if( cluster < 0 || cluster >= cm.numclusters || !cm.vis )
		memset( cm.phsrow, 0xFF, (cm.numclusters + 31) & ~31 );
	else CM_DecompressVis( cm.visbase + cm.vis->bitofs[cluster][DVIS_PHS], cm.phsrow );
	return cm.phsrow;
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