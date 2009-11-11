//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_portals.c - server visibility
//=======================================================================

#include "cm_local.h"

static byte fatpvs[MAX_MAP_LEAFS/8];
static byte fatphs[MAX_MAP_LEAFS/8];

/*
===============================================================================

PVS / PHS

===============================================================================
*/
/*
=================
CM_CalcPHS
=================
*/
void CM_CalcPHS( void )
{
	int	i, j, k, l, index;
	int	rowbytes, rowwords;
	int	bitbyte;
	uint	*dest, *src;
	byte	*scan;
	int	count, vcount;

	if( !cm.pvs ) return;

	MsgDev( D_NOTE, "Building PHS...\n" );

	cm.phs = Mem_Alloc( cms.mempool, cm.visdata_size );
	cm.phs->rowsize = cm.pvs->rowsize;
	cm.phs->numclusters = cm.pvs->numclusters;

	rowbytes = cm.pvs->rowsize;
	rowwords = rowbytes / sizeof( int );

	for( vcount = i = 0; i < cm.pvs->numclusters; i++ )
	{
		scan = CM_ClusterPVS( i );
		for( j = 0; j < cm.pvs->numclusters; j++ )
		{
			if( scan[j>>3] & (1<<(j & 7)))
				vcount++;
		}
	}

	count = 0;
	scan = (byte *)cm.pvs->data;
	dest = (uint *)((byte *)cm.phs->data);

	for( i = 0; i < cm.phs->numclusters; i++, dest += rowwords, scan += rowbytes )
	{
		Mem_Copy( dest, scan, rowbytes );

		for( j = 0; j < rowbytes; j++ )
		{
			bitbyte = scan[j];
			if( !bitbyte ) continue;

			for( k = 0; k < 8; k++ )
			{
				if(!( bitbyte & ( 1<<k )))
					continue;

				// OR this pvs row into the phs
				index = (j << 3) + k;
				if( index >= cm.phs->numclusters )
					Host_Error( "CM_CalcPHS: Bad bit in PVS\n" ); // pad bits should be 0

				src = (uint *)((byte * )cm.pvs->data) + index * rowwords;
				for( l = 0; l < rowwords; l++ ) dest[l] |= src[l];
			}
		}
		for( j = 0; j < cm.phs->numclusters; j++ )
			if( ((byte *)dest)[j>>3] & (1<<(j & 7)))
				count++;
	}

	MsgDev( D_NOTE, "Average clusters visible / hearable / total: %i / %i / %i\n",
		vcount/cm.phs->numclusters, count/cm.phs->numclusters, cm.phs->numclusters );
}

byte *CM_ClusterPVS( int cluster )
{
	if( cluster < 0 || cluster >= cm.numclusters || !cm.pvs )
		return cms.nullrow;
	return (byte *)cm.pvs->data + cluster * cm.pvs->rowsize;
}

byte *CM_ClusterPHS( int cluster )
{
	if( cluster < 0 || cluster >= cm.numclusters || !cm.phs )
		return cms.nullrow;
	return (byte *)cm.phs->data + cluster * cm.phs->rowsize;
}

/*
=================
CM_ClusterSize
=================
*/
int CM_ClusterSize( void )
{
	return cm.pvs ? cm.pvs->rowsize : (MAX_MAP_LEAFS / 8);
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

	count = CM_BoxLeafnums( mins, maxs, leafs, sizeof( leafs ) / sizeof( int ), NULL );
	if( count < 1 ) Host_Error( "CM_FatPVS: invalid leafnum count\n" );
	longs = CM_ClusterSize() / sizeof( int );

	// convert leafs to clusters
	for( i = 0; i < count; i++ ) leafs[i] = CM_LeafCluster( leafs[i] );

	if( !portal ) Mem_Copy( fatpvs, CM_ClusterPVS( leafs[0] ), CM_ClusterSize());

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
============
CM_FatPHS

The client will interpolate the hear position,
so we can't use a single PHS point
===========
*/
byte *CM_FatPHS( int cluster, bool portal )
{
	if( portal )
	{
		byte	*src;
		int	i, longs;

		longs = CM_ClusterSize() / sizeof( int );

		// or in all the other leaf bits
		src = CM_ClusterPHS( cluster );
		for( i = 0; i < longs; i++ )
			((int *)fatphs)[i] |= ((int *)src)[i];
	}
	else Mem_Copy( fatphs, CM_ClusterPHS( cluster ), CM_ClusterSize());

	return fatphs;
}

/*
===============================================================================

AREAPORTALS

===============================================================================
*/
/*
=================
CM_AddAreaPortal
=================
*/
bool CM_AddAreaPortal( int portalnum, int area, int otherarea )
{
	carea_t		*a;
	careaportal_t	*ap;

	if( portalnum >= MAX_MAP_AREAPORTALS )
		return false;
	if( !area || area > cm.numareas || !otherarea || otherarea > cm.numareas )
		return false;

	ap = &cm.areaportals[portalnum];
	ap->area = area;
	ap->otherarea = otherarea;

	a = &cm.areas[area];
	a->areaportals[a->numareaportals++] = portalnum;

	a = &cm.areas[otherarea];
	a->areaportals[a->numareaportals++] = portalnum;

	cm.numareaportals++;

	return true;
}

static void CM_FloodArea_r( int areanum, int floodnum )
{
	int		i;
	carea_t		*area;
	careaportal_t	*p;

	area = &cm.areas[areanum];
	if( area->floodvalid == cm.floodvalid )
	{
		if( area->floodnum == floodnum ) return;
		Host_Error( "FloodArea_r: reflooded\n" );
	}

	area->floodnum = floodnum;
	area->floodvalid = cm.floodvalid;

	for( i = 0; i < area->numareaportals; i++ )
	{
		p = &cm.areaportals[area->areaportals[i]];
		if( !p->open ) continue;

		if( p->area == areanum ) CM_FloodArea_r( p->otherarea, floodnum );
		else if( p->otherarea == areanum ) CM_FloodArea_r( p->area, floodnum );
	}
}

/*
====================
CM_FloodAreaConnections
====================
*/
void CM_FloodAreaConnections( void )
{
	int	i, floodnum = 0;

	// all current floods are now invalid
	cm.floodvalid++;

	// area 0 is not used
	for( i = 1; i < cm.numareas; i++ )
	{
		if( cm.areas[i].floodvalid == cm.floodvalid )
			continue; // already flooded into
		floodnum++;
		CM_FloodArea_r( i, floodnum );
	}
}

void CM_SetAreaPortals( byte *portals, size_t size )
{
	int	i, j;
	vfile_t	*f;

	f = VFS_Create( portals, size );

	VFS_Read( f, &cm.numareaportals, sizeof( int ));

	for( i = 1; i < cm.numareaportals; i++ )
	{
		VFS_Read( f, &j, sizeof( int ));
		VFS_Read( f, &cm.areaportals[j], sizeof( cm.areaportals[0] ));
	}

	VFS_Read( f, &cm.numareas, sizeof( int ));

	cm.areas = Mem_Realloc( cms.mempool, cm.areas, cm.numareas * sizeof( *cm.areas ));

	for( i = 1; i < cm.numareas; i++ )
	{
		VFS_Read( f, &cm.areas[i].numareaportals, sizeof( int ));

		for( j = 0; j < cm.areas[i].numareaportals; j++ )
			VFS_Read( f, &cm.areas[i].areaportals[j], sizeof( int ));
	}

	CM_FloodAreaConnections ();
	VFS_Close( f );
}

void CM_GetAreaPortals( byte **portals, size_t *size )
{
	int	i, j;
	vfile_t	*f;
	byte	*prt;

	f = VFS_Open( NULL, "w" ); 
	VFS_Write( f, &cm.numareaportals, sizeof( int ));

	for( i = 1; i < MAX_MAP_AREAPORTALS; i++ )
	{
		if( cm.areaportals[i].area )
		{
			VFS_Write( f, &i, sizeof( int ));
			VFS_Write( f, &cm.areaportals[i], sizeof( cm.areaportals[0] ));
		}
	}

	VFS_Write( f, &cm.numareas, sizeof( int ));

	for( i = 1; i < cm.numareas; i++ )
	{
		VFS_Write( f, &cm.areas[i].numareaportals, sizeof( int ));

		for( j = 0; j < cm.areas[i].numareaportals; j++ )
			VFS_Write( f, &cm.areas[i].areaportals[j], sizeof( int ));
	}

	// copy portals out
	prt = Mem_Alloc( cms.mempool, VFS_Tell( f ));
	Mem_Copy( prt, VFS_GetBuffer( f ), VFS_Tell( f ));

	if( size ) *size = VFS_Tell( f );
	*portals = prt;

	VFS_Close( f );
}

void CM_SetAreaPortalState( int portalnum, int area, int otherarea, bool open )
{
	if( portalnum >= MAX_MAP_AREAPORTALS )
		Host_Error( "CM_SetAreaPortalState: areaportal > cm.numareaportals\n" );

	if( !cm.areaportals[portalnum].area )
	{
		// add new areaportal if it doesn't exist
		if( !CM_AddAreaPortal( portalnum, area, otherarea ))
			return;
	}

	cm.areaportals[portalnum].open = open;
	CM_FloodAreaConnections();
}

bool CM_AreasConnected( int area, int otherarea )
{
	if( cm_noareas->integer ) return true;
	if( area > cm.numareas || otherarea > cm.numareas )
		Host_Error( "CM_AreasConnected: area >= cm.numareas\n" );

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
		for( i = 1; i < cm.numareas; i++ )
		{
			if( !area || CM_AreasConnected( i, area ))
			{
				if( !portal ) buffer[i>>3] |= 1 << (i & 7);
				else buffer[i>>3] |= 1 << (i & 7);
			}
		}
	}
	return size;
}