//=======================================================================
//			Copyright XashXT Group 2008 ©
//			   vis.c - map visibility
//=======================================================================

#include "bsplib.h"

int		numportals;
int		portalclusters;
visportal_t	*portals;
leaf_t		*leafs;
int		c_portaltest, c_portalpass, c_portalcheck;
byte		*uncompressedvis;
byte		*vismap, *vismap_p, *vismap_end;	// past visfile
int		originalvismapsize;
int		leafbytes;			// (portalclusters+63)>>3
int		leaflongs;
int		portalbytes, portallongs;
int		totalvis;
int		totalphs;
visportal_t	*sorted_portals[MAX_MAP_PORTALS*2];

/*
===============
CompressVis

===============
*/
int CompressVis( byte *vis, byte *dest )
{
	int	j, rep, visrow;
	byte	*dest_p;
	
	dest_p = dest;
	visrow = (dvis->numclusters + 7)>>3;
	
	for( j = 0; j < visrow; j++ )
	{
		*dest_p++ = vis[j];
		if( vis[j] ) continue;

		rep = 1;
		for( j++; j < visrow; j++ )
			if( vis[j] || rep == 255 )
				break;
			else rep++;
		*dest_p++ = rep;
		j--;
	}
	return dest_p - dest;
}


/*
===================
DecompressVis
===================
*/
void DecompressVis( byte *in, byte *decompressed )
{
	int		c;
	byte	*out;
	int		row;

	row = (dvis->numclusters+7)>>3;	
	out = decompressed;

	do
	{
		if( *in )
		{
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		if( !c ) Sys_Error( "DecompressVis: 0 repeat\n" );
		in += 2;
		while( c )
		{
			*out++ = 0;
			c--;
		}
	} while( out - decompressed < row );
}

int PointInLeafnum ( vec3_t point )
{
	float	dist;
	dnode_t	*node;
	dplane_t	*plane;
	int	nodenum = 0;

	while( nodenum >= 0 )
	{
		node = &dnodes[nodenum];
		plane = &dplanes[node->planenum];
		dist = DotProduct( point, plane->normal ) - plane->dist;
		if( dist > 0 ) nodenum = node->children[0];
		else nodenum = node->children[1];
	}
	return -nodenum - 1;
}

dleaf_t *PointInLeaf( vec3_t point )
{
	int	num;

	num = PointInLeafnum( point );
	return &dleafs[num];
}

bool PvsForOrigin( vec3_t org, byte *pvs )
{
	dleaf_t	*leaf;

	if( !visdatasize )
	{
		Mem_Set( pvs, 0xFF, (numleafs+7)/8 );
		return true;
	}

	leaf = PointInLeaf( org );
	if( leaf->cluster == -1 )
		return false;		// in solid leaf

	DecompressVis( dvisdata + dvis->bitofs[leaf->cluster][DVIS_PVS], pvs );
	return true;
}

//=============================================================================

void PlaneFromWinding( viswinding_t *w, visplane_t *plane )
{
	vec3_t		v1, v2;

	// calc plane
	VectorSubtract (w->points[2], w->points[1], v1);
	VectorSubtract (w->points[0], w->points[1], v2);
	CrossProduct (v2, v1, plane->normal);
	VectorNormalize (plane->normal);
	plane->dist = DotProduct (w->points[0], plane->normal);
}


/*
==================
NewVisWinding
==================
*/
viswinding_t *NewVisWinding (int points)
{
	viswinding_t	*w;
	int			size;
	
	if (points > MAX_POINTS_ON_WINDING)
		Sys_Error ("NewVisWinding: %i points", points);
	
	size = (int)((viswinding_t *)0)->points[points];
	w = Malloc (size);
	
	return w;
}



static void pw(viswinding_t *w)
{
	int		i;
	for (i=0 ; i<w->numpoints ; i++)
		Msg("(%5.1f, %5.1f, %5.1f)\n",w->points[i][0], w->points[i][1],w->points[i][2]);
}

static void prl(leaf_t *l)
{
	int			i;
	visportal_t	*p;
	visplane_t		pl;
	
	for (i=0 ; i<l->numportals ; i++)
	{
		p = l->portals[i];
		pl = p->plane;
		Msg ("portal %4i to leaf %4i : %7.1f : (%4.1f, %4.1f, %4.1f)\n",(int)(p-portals),p->leaf,pl.dist, pl.normal[0], pl.normal[1], pl.normal[2]);
	}
}


//=============================================================================

/*
=============
SortPortals

Sorts the portals from the least complex, so the later ones can reuse
the earlier information.
=============
*/
int PComp (const void *a, const void *b)
{
	if ( (*(visportal_t **)a)->nummightsee == (*(visportal_t **)b)->nummightsee)
		return 0;
	if ( (*(visportal_t **)a)->nummightsee < (*(visportal_t **)b)->nummightsee)
		return -1;
	return 1;
}
void SortPortals (void)
{
	int		i;
	
	for (i=0 ; i<numportals*2 ; i++)
		sorted_portals[i] = &portals[i];

	qsort (sorted_portals, numportals*2, sizeof(sorted_portals[0]), PComp);
}


/*
==============
LeafVectorFromPortalVector
==============
*/
int LeafVectorFromPortalVector (byte *portalbits, byte *leafbits)
{
	int			i;
	visportal_t	*p;
	int			c_leafs;


	memset (leafbits, 0, leafbytes);

	for (i=0 ; i<numportals*2 ; i++)
	{
		if (portalbits[i>>3] & (1<<(i&7)) )
		{
			p = portals+i;
			leafbits[p->leaf>>3] |= (1<<(p->leaf&7));
		}
	}

	c_leafs = CountBits (leafbits, portalclusters);

	return c_leafs;
}


/*
===============
ClusterMerge

Merges the portal visibility for a leaf
===============
*/
void ClusterMerge (int leafnum)
{
	leaf_t		*leaf;
	byte		portalvector[MAX_PORTALS/8];
	byte		uncompressed[MAX_MAP_LEAFS/8];
	byte		compressed[MAX_MAP_LEAFS/8];
	int			i, j;
	int			numvis;
	byte		*dest;
	visportal_t	*p;
	int			pnum;

	// OR together all the portalvis bits

	memset (portalvector, 0, portalbytes);
	leaf = &leafs[leafnum];
	for (i = 0; i < leaf->numportals; i++)
	{
		p = leaf->portals[i];
		if(p->status != stat_done)
			Sys_Error ("portal not done");
		for(j = 0; j < portallongs; j++)
			((long *)portalvector)[j] |= ((long *)p->portalvis)[j];
		pnum = p - portals;
		portalvector[pnum>>3] |= 1<<(pnum&7);
	}

	// convert portal bits to leaf bits
	numvis = LeafVectorFromPortalVector(portalvector, uncompressed);

	if (uncompressed[leafnum>>3] & (1<<(leafnum&7)))
		MsgDev( D_WARN, "Leaf portals saw into leaf\n");
		
	uncompressed[leafnum>>3] |= (1<<(leafnum&7));
	numvis++;	// count the leaf itself

	// save uncompressed for PHS calculation
	Mem_Copy(uncompressedvis + leafnum*leafbytes, uncompressed, leafbytes);

	// compress the bit string
	totalvis += numvis;

	i = CompressVis (uncompressed, compressed);

	dest = vismap_p;
	vismap_p += i;
	
	if (vismap_p > vismap_end)
		Sys_Error ("Vismap expansion overflow");

	dvis->bitofs[leafnum][DVIS_PVS] = dest - vismap;
	Mem_Copy(dest, compressed, i);	
}


/*
==================
CalcPortalVis
==================
*/
void CalcPortalVis( void )
{
	if( bsp_parms & BSPLIB_FULLCOMPILE )
	{
		RunThreadsOnIndividual( numportals * 2, true, PortalFlow );
	}
	else
	{
		int	i;
	
		// fastvis just uses mightsee for a very loose bound
		for( i = 0; i < numportals * 2; i++ )
		{
			portals[i].portalvis = portals[i].portalflood;
			portals[i].status = stat_done;
		}
	}
}


/*
==================
CalcPVS
==================
*/
void CalcPVS( void )
{
	int	i;
	
	Msg( "Building PVS...\n" );

	RunThreadsOnIndividual( numportals * 2, true, BasePortalVis );
	SortPortals();
	CalcPortalVis();

	// assemble the leaf vis lists by oring and compressing the portal lists
	for( i = 0; i < portalclusters; i++ )
		ClusterMerge (i);
		
	Msg( "Average clusters visible: %i\n", totalvis / portalclusters );
}


void SetPortalSphere (visportal_t *p)
{
	int		i;
	vec3_t	total, dist;
	viswinding_t	*w;
	float	r, bestr;

	w = p->winding;
	VectorCopy (vec3_origin, total);
	for (i=0 ; i<w->numpoints ; i++)
	{
		VectorAdd (total, w->points[i], total);
	}
	
	for (i=0 ; i<3 ; i++)
		total[i] /= w->numpoints;

	bestr = 0;		
	for (i=0 ; i<w->numpoints ; i++)
	{
		VectorSubtract (w->points[i], total, dist);
		r = VectorLength (dist);
		if (r > bestr)
			bestr = r;
	}
	VectorCopy (total, p->origin);
	p->radius = bestr;
}

/*
============
LoadPortals
============
*/
void LoadPortals( void )
{
	visportal_t	*p;
	leaf_t		*l;
	char		magic[80];
	char		path[MAX_SYSPATH];
	int		numpoints;
	script_t		*prtfile;
	viswinding_t	*w;
	int		i, j, leafnums[2];
	visplane_t	plane;
	
	com.sprintf( path, "maps/%s.prt", gs_filename );
	prtfile = Com_OpenScript( path, NULL, 0 );
	if( !prtfile ) Sys_Break( "LoadPortals: couldn't read %s\n", path );
	MsgDev( D_NOTE, "reading %s\n", path );
	
	Com_ReadString( prtfile, true, magic );
          Com_ReadLong( prtfile, true, &portalclusters );
          Com_ReadLong( prtfile, true, &numportals );

          if( !portalclusters && !numportals ) Sys_Break( "LoadPortals: failed to read header\n" );
	if( com.strcmp( magic, PORTALFILE )) Sys_Break( "LoadPortals: not a portal file\n" );
	
	Msg( "%4i portalclusters\n", portalclusters );
	Msg( "%4i numportals\n", numportals );

	// these counts should take advantage of 64 bit systems automatically
	leafbytes = ((portalclusters+63)&~63)>>3;
	leaflongs = leafbytes/sizeof(long);
	
	portalbytes = ((numportals*2+63)&~63)>>3;
	portallongs = portalbytes/sizeof(long);

	// each file portal is split into two memory portals
	portals = Malloc(2 * numportals * sizeof(visportal_t));
	
	leafs = Malloc(portalclusters*sizeof(leaf_t));

	originalvismapsize = portalclusters*leafbytes;
	uncompressedvis = Malloc(originalvismapsize);

	vismap = vismap_p = dvisdata;
	dvis->numclusters = portalclusters;
	vismap_p = (byte *)&dvis->bitofs[portalclusters];
	vismap_end = vismap + MAX_MAP_VISIBILITY;
		
	for (i = 0, p = portals; i < numportals; i++)
	{
		Com_ReadLong( prtfile, true, &numpoints );
		Com_ReadLong( prtfile, false, &leafnums[0] );
		Com_ReadLong( prtfile, false, &leafnums[1] );		
                    
		if( numpoints > MAX_POINTS_ON_WINDING )
			Sys_Break( "LoadPortals: portal %i has too many points\n", i );
		if(( uint )leafnums[0] > portalclusters || (uint)leafnums[1] > portalclusters )
			Sys_Break( "LoadPortals: reading portal %i\n", i );
		
		w = p->winding = NewVisWinding( numpoints );
		w->original = true;
		w->numpoints = numpoints;
		
		for( j = 0; j < numpoints; j++ )
		{
			token_t	token;
			vec3_t	v;
			int	k;

			// scanf into double, then assign to float
			// so we don't care what size float is
			Com_ReadToken( prtfile, 0, &token );	// get '(' symbol
			if( com.stricmp( token.string, "(" ))
				Sys_Break( "LoadPortals: not found ( reading portal %i\n", i );

			Com_ReadFloat( prtfile, false, &v[0] );
			Com_ReadFloat( prtfile, false, &v[1] );
			Com_ReadFloat( prtfile, false, &v[2] );			
				
			Com_ReadToken( prtfile, 0, &token );
			if( com.stricmp( token.string, ")" ))
				Sys_Error( "LoadPortals: not found ) reading portal %i", i );
		
			for (k = 0; k < 3; k++) w->points[j][k] = v[k];
		}
		
		// calc plane
		PlaneFromWinding (w, &plane);

		// create forward portal
		l = &leafs[leafnums[0]];
		if (l->numportals == MAX_PORTALS_ON_LEAF) Sys_Error ("Leaf with too many portals");
		l->portals[l->numportals] = p;
		l->numportals++;
		
		p->winding = w;
		VectorSubtract (vec3_origin, plane.normal, p->plane.normal);
		p->plane.dist = -plane.dist;
		p->leaf = leafnums[1];
		SetPortalSphere (p);
		p++;
		
		// create backwards portal
		l = &leafs[leafnums[1]];
		if (l->numportals == MAX_PORTALS_ON_LEAF) Sys_Error ("Leaf with too many portals");
		l->portals[l->numportals] = p;
		l->numportals++;
		
		p->winding = NewVisWinding(w->numpoints);
		p->winding->numpoints = w->numpoints;
		for (j=0 ; j<w->numpoints ; j++)
		{
			VectorCopy (w->points[w->numpoints-1-j], p->winding->points[j]);
		}

		p->plane = plane;
		p->leaf = leafnums[0];
		SetPortalSphere (p);
		p++;

	}
}

void ClusterPHS( int clusternum )
{
	int	j, k, l, index;
	int	bitbyte;
	long	*dest, *src;
	byte	*scan;
	byte	uncompressed[MAX_MAP_LEAFS/8];
	byte	compressed[MAX_MAP_LEAFS/8];
	
	scan = uncompressedvis + clusternum * leafbytes;
	Mem_Copy( uncompressed, scan, leafbytes );
	for (j = 0; j < leafbytes; j++)
	{
		bitbyte = scan[j];
		if (!bitbyte) continue;
		for (k = 0; k < 8; k++)
		{
			if (! (bitbyte & (1<<k))) continue;
			// OR this pvs row into the phs
			index = ((j<<3)+k);
			if (index >= portalclusters)
				Sys_Error ("Bad bit in PVS");	// pad bits should be 0
			src = (long *)(uncompressedvis + index*leafbytes);
			dest = (long *)uncompressed;
			for (l = 0; l < leaflongs; l++) ((long *)uncompressed)[l] |= src[l];
		}
	}
	for (j = 0; j < portalclusters; j++)
		if (uncompressed[j>>3] & (1<<(j&7)) )
			totalphs++;

	// compress the bit string
	j = CompressVis( uncompressed, compressed );

	dest = (long *)vismap_p;
	vismap_p += j;
		
	if( vismap_p > vismap_end )
		Sys_Error( "Vismap expansion overflow\n" );

	dvis->bitofs[clusternum][DVIS_PHS] = (byte *)dest - vismap;
	Mem_Copy( dest, compressed, j );	
} 

/*
================
CalcPHS

Calculate the PHS (Potentially Hearable Set)
by ORing together all the PVS visible from a leaf
================
*/
void CalcPHS( void )
{
	Msg( "Building PHS...\n" );
	RunThreadsOnIndividual( portalclusters, true, ClusterPHS );
	Msg( "Average clusters hearable: %i\n", totalphs / portalclusters );
}

/*
===========
main
===========
*/
void WvisMain( void )
{
	if( !LoadBSPFile( ))
	{
		// map not exist, create it
		WbspMain();
		LoadBSPFile();
	}
	if( numnodes == 0 || numsurfaces == 0 )
		Sys_Break( "Empty map %s.bsp\n", gs_filename );

	Msg ("\n---- vis ---- [%s]\n", (bsp_parms & BSPLIB_FULLCOMPILE) ? "full" : "fast" );

	LoadPortals();
	CalcPVS();
	CalcPHS();

	visdatasize = vismap_p - dvisdata;	
	MsgDev( D_INFO, "visdatasize:%i  compressed from %i\n", visdatasize, originalvismapsize * 2 );

	WriteBSPFile();	
}