// vis.c
#include "bsplib.h"

int		numportals;
int		portalclusters;

char		inbase[32];
char		outbase[32];

visportal_t	*portals;
leaf_t		*leafs;

int		c_portaltest, c_portalpass, c_portalcheck;

byte		*uncompressedvis;

byte	*vismap, *vismap_p, *vismap_end;	// past visfile
int	originalvismapsize;

int	leafbytes;				// (portalclusters+63)>>3
int	leaflongs;

int		portalbytes, portallongs;

bool		fastvis;

int		testlevel = 2;

int		totalvis;
int		totalphs;

visportal_t	*sorted_portals[MAX_MAP_PORTALS*2];


//=============================================================================

void PlaneFromWinding (viswinding_t *w, visplane_t *plane)
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
	for (i=0 ; i<leaf->numportals ; i++)
	{
		p = leaf->portals[i];
		if (p->status != stat_done)
			Sys_Error ("portal not done");
		for (j=0 ; j<portallongs ; j++)
			((long *)portalvector)[j] |= ((long *)p->portalvis)[j];
		pnum = p - portals;
		portalvector[pnum>>3] |= 1<<(pnum&7);
	}

	// convert portal bits to leaf bits
	numvis = LeafVectorFromPortalVector (portalvector, uncompressed);

	if (uncompressed[leafnum>>3] & (1<<(leafnum&7)))
		Msg ("WARNING: Leaf portals saw into leaf\n");
		
	uncompressed[leafnum>>3] |= (1<<(leafnum&7));
	numvis++;		// count the leaf itself

	// save uncompressed for PHS calculation
	memcpy (uncompressedvis + leafnum*leafbytes, uncompressed, leafbytes);

	// compress the bit string
	totalvis += numvis;

	i = CompressVis (uncompressed, compressed);

	dest = vismap_p;
	vismap_p += i;
	
	if (vismap_p > vismap_end)
		Sys_Error ("Vismap expansion overflow");

	dvis->bitofs[leafnum][DVIS_PVS] = dest-vismap;

	memcpy (dest, compressed, i);	
}


/*
==================
CalcPortalVis
==================
*/
void CalcPortalVis (void)
{
	int		i;

// fastvis just uses mightsee for a very loose bound
	if (fastvis)
	{
		for (i=0 ; i<numportals*2 ; i++)
		{
			portals[i].portalvis = portals[i].portalflood;
			portals[i].status = stat_done;
		}
		return;
	}
	
	RunThreadsOnIndividual (numportals*2, true, PortalFlow);

}


/*
==================
CalcVis
==================
*/
void CalcVis (void)
{
	int		i;
	
	Msg ("Building PVS...\n");

	RunThreadsOnIndividual (numportals*2, true, BasePortalVis);
	SortPortals ();
	CalcPortalVis ();

//
// assemble the leaf vis lists by oring and compressing the portal lists
//
	for (i=0 ; i<portalclusters ; i++)
		ClusterMerge (i);
		
	Msg ("Average clusters visible: %i\n", totalvis / portalclusters);
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
void LoadPortals (void)
{
	int		i, j;
	bool		load;
	visportal_t	*p;
	leaf_t		*l;
	char		magic[80];
	char		path[MAX_SYSPATH];
	int		numpoints;
	viswinding_t	*w;
	int		leafnums[2];
	visplane_t	plane;
	
	sprintf (path, "maps/%s.prt", gs_mapname );
	load = FS_LoadScript( path, NULL, 0 );
	if (!load) Sys_Error ("LoadPortals: couldn't read %s\n", path);
	Msg ("reading %s\n", path);
	
	strcpy(magic, SC_GetToken( true ));
          portalclusters = atoi(SC_GetToken( true ));
          numportals = atoi(SC_GetToken( true ));

          if (!portalclusters && !numportals) Sys_Error ("LoadPortals: failed to read header");
	if (strcmp(magic, PORTALFILE)) Sys_Error ("LoadPortals: not a portal file");
	
	Msg ("%4i portalclusters\n", portalclusters);
	Msg ("%4i numportals\n", numportals);

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
		numpoints = atoi(SC_GetToken( true )); //newline
		leafnums[0] = atoi(SC_GetToken( false ));
		leafnums[1] = atoi(SC_GetToken( false ));		
                    
                    //Msg("%d %d %d ", numpoints, leafnums[0], leafnums[1] );
		
		if (numpoints > MAX_POINTS_ON_WINDING)
			Sys_Error ("LoadPortals: portal %i has too many points", i);
		if ((unsigned)leafnums[0] > portalclusters || (unsigned)leafnums[1] > portalclusters)
			Sys_Error ("LoadPortals: reading portal %i", i);
		
		w = p->winding = NewVisWinding(numpoints);
		w->original = true;
		w->numpoints = numpoints;
		
		for (j = 0; j < numpoints; j++)
		{
			double	v[3];
			int	k;

			// scanf into double, then assign to vec_t
			// so we don't care what size vec_t is
			SC_GetToken( false ); //get '(' symbol
			if(!SC_MatchToken( "(" )) Sys_Error ("LoadPortals: not found ( reading portal %i", i);
			v[0] = atof(SC_GetToken( false ));
			v[1] = atof(SC_GetToken( false ));
			v[2] = atof(SC_GetToken( false ));			
			
			//Msg("( %g %g %g )", v[0], v[1], v[2] );
						
			SC_GetToken( false );
			if(!SC_MatchToken( ")" )) Sys_Error ("LoadPortals: not found ) reading portal %i", i);
		
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
	Mem_Copy (uncompressed, scan, leafbytes);
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
	j = CompressVis (uncompressed, compressed);

	dest = (long *)vismap_p;
	vismap_p += j;
		
	if (vismap_p > vismap_end)
		Sys_Error ("Vismap expansion overflow");

	dvis->bitofs[clusternum][DVIS_PHS] = (byte *)dest-vismap;
	Mem_Copy (dest, compressed, j);	
} 

/*
================
CalcPHS

Calculate the PHS (Potentially Hearable Set)
by ORing together all the PVS visible from a leaf
================
*/
void CalcPHS (void)
{
	Msg ("Building PHS...\n");
	RunThreadsOnIndividual (portalclusters, true, ClusterPHS);
	Msg("Average clusters hearable: %i\n", totalphs/portalclusters);
}

/*
===========
main
===========
*/
void WvisMain ( bool option )
{
	fastvis = !option;
	
	if(!LoadBSPFile())
	{
		//map not exist, create it
		WbspMain( false );
		LoadBSPFile();
	}
	if (numnodes == 0 || numfaces == 0) Sys_Error ("Empty map");

	Msg ("---- Visibility ---- [%s]\n", fastvis ? "fast" : "full" );
	LoadPortals ();
	CalcVis ();
	CalcPHS ();

	visdatasize = vismap_p - dvisdata;	
	Msg ("visdatasize:%i  compressed from %i\n", visdatasize, originalvismapsize*2);

	WriteBSPFile();	
}

