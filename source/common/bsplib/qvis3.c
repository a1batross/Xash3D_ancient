//=======================================================================
//			Copyright XashXT Group 2007 ©
//			visibility.c - map visibility
//=======================================================================

#include "bsplib.h"

#define WCONVEX_EPSILON		0.2
#define CONTINUOUS_EPSILON		0.005

int		numportals;
int		portalclusters;
vportal_t		*portals;
leaf_t		*leafs;
vportal_t		*faces;
leaf_t		*faceleafs;
int		c_portaltest, c_portalpass, c_portalcheck;
int		leafbytes; // (portalclusters+63)>>3
int		leaflongs;
int		portalbytes;
int		portallongs;
int		numvisfaces;

bool		fastvis;		// fast calc visibility
bool		noPassageVis;	// ignore passages
bool		passageVisOnly;	// visibility from passages only
bool		mergevis;		// merge visibility
bool		nosort;		// no sorting portals
bool		saveprt;		// don't remove prt file

byte		*vismap, *vismap_p, *vismap_end;	// past visfile
int		originalvismapsize;
byte		*uncompressedvis;

int		totalpvs;
int		totalphs;

vportal_t		*sorted_portals[MAX_MAP_PORTALS*2];
void PassageMemory( void );

//=============================================================================
void PlaneFromWinding( winding_t *w, vplane_t *plane )
{
	vec3_t	v1, v2;

	// calc plane
	VectorSubtract( w->p[2], w->p[1], v1 );
	VectorSubtract( w->p[0], w->p[1], v2 );
	CrossProduct( v2, v1, plane->normal );
	VectorNormalize( plane->normal );
	plane->dist = DotProduct( w->p[0], plane->normal );
}


/*
==================
NewWinding
==================
*/
winding_t *NewWinding( int points )
{
	int	size;
	
	if( points > MAX_POINTS_ON_WINDING )
		Sys_Error( "NewWinding: %i points\n", points );
	
	size = (int)((winding_t *)0)->p[points];
	return BSP_Malloc( size );
}

static void prl( leaf_t *l )
{
	int		i;
	vportal_t		*p;
	vplane_t		pl;
	
	for( i = 0; i < l->numportals; i++ )
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
int PComp( const void *a, const void *b )
{
	if((*(vportal_t **)a)->nummightsee == (*(vportal_t **)b)->nummightsee)
		return 0;
	if((*(vportal_t **)a)->nummightsee  < (*(vportal_t **)b)->nummightsee)
		return -1;
	return 1;
}

void SortPortals( void )
{
	int	i;
	
	for( i = 0; i < numportals * 2; i++ )
		sorted_portals[i] = &portals[i];

	if( nosort ) return;
	qsort( sorted_portals, numportals * 2, sizeof(sorted_portals[0]), PComp );
}


/*
==============
LeafVectorFromPortalVector
==============
*/
int LeafVectorFromPortalVector( byte *portalbits, byte *leafbits )
{
	int		i, j, leafnum;
	vportal_t		*p;

	for( i = 0; i < numportals * 2; i++ )
	{
		if( portalbits[i>>3] & (1<<( i & 7 )))
		{
			p = portals + i;
			leafbits[p->leaf>>3] |= (1<<( p->leaf & 7 ));
		}
	}

	for( j = 0; j < portalclusters; j++ )
	{
		leafnum = j;
		while( leafs[leafnum].merged >= 0 )
			leafnum = leafs[leafnum].merged;
		// if the merged leaf is visible then the original leaf is visible
		if( leafbits[leafnum>>3] & (1<<( leafnum & 7 )))
		{
			leafbits[j>>3] |= (1<<( j & 7 ));
		}
	}
	return CountBits( leafbits, portalclusters );
}


/*
===============
ClusterPVS

Merges the portal visibility for a leaf
===============
*/
void ClusterPVS( int leafnum )
{
	leaf_t		*leaf;
	byte		portalvector[MAX_PORTALS/8];
	byte		uncompressed[MAX_MAP_LEAFS/8];
	byte		compressed[MAX_MAP_LEAFS/8];
	int		i, j;
	int		numvis, mergedleafnum;
	byte		*dest;
	vportal_t		*p;
	int		pnum;

	// OR together all the portalvis bits

	mergedleafnum = leafnum;
	while( leafs[mergedleafnum].merged >= 0 )
		mergedleafnum = leafs[mergedleafnum].merged;

	memset( portalvector, 0, portalbytes );
	leaf = &leafs[mergedleafnum];
	for( i = 0; i < leaf->numportals; i++ )
	{
		p = leaf->portals[i];
		if( p->removed ) continue;

		if( p->status != stat_done ) Sys_Error( "portal not done\n" );
		for( j = 0; j < portallongs; j++ )
			((long *)portalvector)[j] |= ((long *)p->portalvis)[j];
		pnum = p - portals;
		portalvector[pnum>>3] |= 1<<( pnum & 7 );
	}

	memset( uncompressed, 0, leafbytes );
	uncompressed[mergedleafnum>>3] |= (1<<( mergedleafnum & 7 ));

	// convert portal bits to leaf bits
	numvis = LeafVectorFromPortalVector( portalvector, uncompressed );
	numvis++;	 // count the leaf itself

	// save uncompressed for PHS calculation
	Mem_Copy( uncompressedvis + leafnum * leafbytes, uncompressed, leafbytes );

	totalpvs += numvis;
	i = CompressVis( uncompressed, compressed );

	dest = vismap_p;
	vismap_p += i;
	
	if( vismap_p > vismap_end )
		Sys_Error( "ClusterPVS: vismap expansion overflow at cluster %i\n", leafnum );

	dvis->bitofs[leafnum][DVIS_PVS] = dest - vismap;
	Mem_Copy( dest, compressed, i );	
}


/*
==================
CalcPortalVis
==================
*/
void CalcPortalVis( void )
{
	RunThreadsOnIndividual( numportals * 2, true, PortalFlow );
}

/*
==================
CalcPassageVis
==================
*/
void CalcPassageVis( void )
{
	PassageMemory();

	RunThreadsOnIndividual( numportals * 2, true, CreatePassages );
	RunThreadsOnIndividual( numportals * 2, true, PassageFlow );
}

/*
==================
CalcPassagePortalVis
==================
*/
void CalcPassagePortalVis( void )
{
	PassageMemory();

	RunThreadsOnIndividual( numportals * 2, true, CreatePassages );
	RunThreadsOnIndividual( numportals * 2, true, PassagePortalFlow );
}

/*
==================
CalcFastVis
==================
*/
void CalcFastVis( void )
{
	int	i;

	// fastvis just uses mightsee for a very loose bound
	for( i = 0; i < numportals * 2; i++ )
	{
		portals[i].portalvis = portals[i].portalflood;
		portals[i].status = stat_done;
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
	
	Msg ("Building PVS...\n");

	RunThreadsOnIndividual( numportals * 2, true, BasePortalVis );
	SortPortals ();

	if( fastvis ) CalcFastVis();
	else if( noPassageVis )
		CalcPortalVis();
	else if( passageVisOnly )
		CalcPassageVis();
	else CalcPassagePortalVis();

	// assemble the leaf vis lists by oring and compressing the portal lists
	for( i = 0; i < portalclusters; i++ )
		ClusterPVS( i );

	Msg( "Total visible clusters: %i\n", totalpvs );		
	Msg( "Average clusters visible: %i\n", totalpvs / portalclusters );
}

/*
==================
SetPortalSphere
==================
*/
void SetPortalSphere( vportal_t *p )
{
	int		i;
	vec3_t		total, dist;
	winding_t	*w;
	float		r, bestr;

	w = p->winding;
	VectorCopy( vec3_origin, total );
	for( i = 0; i < w->numpoints; i++ )
	{
		VectorAdd( total, w->p[i], total );
	}
	
	for( i = 0; i < 3; i++ ) total[i] /= w->numpoints;

	bestr = 0;		
	for( i = 0; i < w->numpoints; i++ )
	{
		VectorSubtract( w->p[i], total, dist );
		r = VectorLength( dist );
		if( r > bestr ) bestr = r;
	}
	VectorCopy( total, p->origin );
	p->radius = bestr;
}

/*
=============
Winding_PlanesConcave
=============
*/
int Winding_PlanesConcave( winding_t *w1, winding_t *w2, vec3_t normal1, vec3_t normal2, float dist1, float dist2 )
{
	int i;

	if( !w1 || !w2 ) return false;

	// check if one of the points of winding 1 is at the front of the plane of winding 2
	for( i = 0; i < w1->numpoints; i++ )
	{
		if( DotProduct( normal2, w1->p[i] ) - dist2 > WCONVEX_EPSILON ) return true;
	}
	// check if one of the points of winding 2 is at the front of the plane of winding 1
	for( i = 0; i < w2->numpoints; i++ )
	{
		if( DotProduct( normal1, w2->p[i] ) - dist1 > WCONVEX_EPSILON ) return true;
	}
	return false;
}

/*
============
TryMergeLeaves
============
*/
int TryMergeLeaves( int l1num, int l2num )
{
	int		i, j, k, n, numportals;
	vplane_t		plane1, plane2;
	leaf_t		*l1, *l2;
	vportal_t 	*p1, *p2;
	vportal_t 	*portals[MAX_PORTALS_ON_LEAF];

	for( k = 0; k < 2; k++ )
	{
		if( k ) l1 = &leafs[l1num];
		else l1 = &faceleafs[l1num];
		for( i = 0; i < l1->numportals; i++ )
		{
			p1 = l1->portals[i];
			if( p1->leaf == l2num ) continue;
			for( n = 0; n < 2; n++ )
			{
				if( n ) l2 = &leafs[l2num];
				else l2 = &faceleafs[l2num];
				for( j = 0; j < l2->numportals; j++ )
				{
					p2 = l2->portals[j];
					if( p2->leaf == l1num )
						continue;
					plane1 = p1->plane;
					plane2 = p2->plane;
					if( Winding_PlanesConcave( p1->winding, p2->winding, plane1.normal, plane2.normal, plane1.dist, plane2.dist ))
						return false;
				}
			}
		}
	}
	for( k = 0; k < 2; k++ )
	{
		if( k )
		{
			l1 = &leafs[l1num];
			l2 = &leafs[l2num];
		}
		else
		{
			l1 = &faceleafs[l1num];
			l2 = &faceleafs[l2num];
		}
		numportals = 0;
		// the leaves can be merged now
		for( i = 0; i < l1->numportals; i++ )
		{
			p1 = l1->portals[i];
			if( p1->leaf == l2num )
			{
				p1->removed = true;
				continue;
			}
			portals[numportals++] = p1;
		}
		for( j = 0; j < l2->numportals; j++ )
		{
			p2 = l2->portals[j];
			if( p2->leaf == l1num )
			{
				p2->removed = true;
				continue;
			}
			portals[numportals++] = p2;
		}
		for( i = 0; i < numportals; i++ )
		{
			l2->portals[i] = portals[i];
		}
		l2->numportals = numportals;
		l1->merged = l2num;
	}
	return true;
}

/*
============
UpdatePortals
============
*/
void UpdatePortals( void )
{
	int	i;
	vportal_t *p;

	for( i = 0; i < numportals * 2; i++ )
	{
		p = &portals[i];
		if( p->removed ) continue;
		while( leafs[p->leaf].merged >= 0 )
			p->leaf = leafs[p->leaf].merged;
	}
}

/*
============
MergeLeaves

try to merge leaves but don't merge through hint splitters
============
*/
void MergeLeaves( void )
{
	int	i, j, nummerges, totalnummerges;
	leaf_t	*leaf;
	vportal_t *p;

	totalnummerges = 0;
	do
	{
		nummerges = 0;
		for( i = 0; i < portalclusters; i++ )
		{
			leaf = &leafs[i];
			// if this leaf is merged already
			if( leaf->merged >= 0 ) continue;

			for( j = 0; j < leaf->numportals; j++ )
			{
				p = leaf->portals[j];

				if( p->removed ) continue;
				// never merge through hint portals
				if( p->hint ) continue;
				if( TryMergeLeaves( i, p->leaf ))
				{
					UpdatePortals();
					nummerges++;
					break;
				}
			}
		}
		totalnummerges += nummerges;
	} while( nummerges );

	Msg( "%6d leaves merged\n", totalnummerges );
}

/*
============
TryMergeWinding
============
*/
winding_t *TryMergeWinding( winding_t *f1, winding_t *f2, vec3_t planenormal )
{
	float		*p1, *p2, *p3, *p4, *back;
	winding_t	*newf;
	int		i, j, k, l;
	vec3_t		normal, delta;
	float		dot;
	bool		keep1, keep2;
	

	// find a common edge
	p1 = p2 = NULL;	// stop compiler warning
	j = 0;		// 
	
	for( i = 0; i < f1->numpoints; i++ )
	{
		p1 = f1->p[i];
		p2 = f1->p[(i+1) % f1->numpoints];
		for( j = 0; j < f2->numpoints; j++ )
		{
			p3 = f2->p[j];
			p4 = f2->p[(j+1) % f2->numpoints];
			for( k = 0; k < 3; k++ )
			{
				if(fabs( p1[k] - p4[k]) > 0.1 ) break;
				if(fabs( p2[k] - p3[k]) > 0.1 ) break;
			}
			if( k == 3 ) break;
		}
		if( j < f2->numpoints ) break;
	}
	
	if( i == f1->numpoints ) return NULL; // no matching edges

	// check slope of connected lines
	// if the slopes are colinear, the point can be removed
	back = f1->p[(i+f1->numpoints-1)%f1->numpoints];
	VectorSubtract( p1, back, delta );
	CrossProduct( planenormal, delta, normal );
	VectorNormalize( normal );
	
	back = f2->p[(j+2)%f2->numpoints];
	VectorSubtract( back, p1, delta );
	dot = DotProduct( delta, normal );
	if( dot > CONTINUOUS_EPSILON ) return NULL; // not a convex polygon
	keep1 = (bool)(dot < -CONTINUOUS_EPSILON );
	
	back = f1->p[(i+2)%f1->numpoints];
	VectorSubtract( back, p2, delta );
	CrossProduct( planenormal, delta, normal );
	VectorNormalize( normal );

	back = f2->p[(j+f2->numpoints-1)%f2->numpoints];
	VectorSubtract( back, p2, delta );
	dot = DotProduct( delta, normal );
	if( dot > CONTINUOUS_EPSILON ) return NULL; // not a convex polygon
	keep2 = (bool)( dot < -CONTINUOUS_EPSILON );

	// build the new polygon
	newf = NewWinding( f1->numpoints + f2->numpoints );
	
	// copy first polygon
	for( k = (i+1)%f1->numpoints; k != i; k = (k+1)%f1->numpoints)
	{
		if( k == (i+1)%f1->numpoints && !keep2) continue;
		VectorCopy( f1->p[k], newf->p[newf->numpoints] );
		newf->numpoints++;
	}
	
	// copy second polygon
	for( l = (j+1)%f2->numpoints; l != j; l = (l+1)%f2->numpoints )
	{
		if( l == (j+1)%f2->numpoints && !keep1 ) continue;
		VectorCopy( f2->p[l], newf->p[newf->numpoints] );
		newf->numpoints++;
	}
	return newf;
}

/*
============
MergeLeafPortals
============
*/
void MergeLeafPortals( void )
{
	int		i, j, k, nummerges, hintsmerged;
	leaf_t		*leaf;
	vportal_t		*p1, *p2;
	winding_t 	*w;

	nummerges = 0;
	hintsmerged = 0;
	for( i = 0; i < portalclusters; i++ )
	{
		leaf = &leafs[i];
		if( leaf->merged >= 0 ) continue;
		for( j = 0; j < leaf->numportals; j++ )
		{
			p1 = leaf->portals[j];
			if( p1->removed ) continue;
			for( k = j+1; k < leaf->numportals; k++ )
			{
				p2 = leaf->portals[k];
				if( p2->removed ) continue;
				if( p1->leaf == p2->leaf )
				{
					w = TryMergeWinding( p1->winding, p2->winding, p1->plane.normal );
					if( w )
					{
						FreeWinding( p1->winding );
						p1->winding = w;
						if( p1->hint && p2->hint )
							hintsmerged++;
						p1->hint |= p2->hint;
						SetPortalSphere( p1 );
						p2->removed = true;
						nummerges++;
						i--;
						break;
					}
				}
			}
			if( k < leaf->numportals ) break;
		}
	}

	Msg( "%6d portals merged\n", nummerges );
	Msg( "%6d hint portals merged\n", hintsmerged );
}

/*
============
WritePortals
============
*/
int CountActivePortals( void )
{
	int	num, hints, j;
	vportal_t *p;

	num = 0;
	hints = 0;
	for( j = 0; j < numportals * 2; j++ )
	{
		p = portals + j;
		if( p->removed ) continue;
		if( p->hint ) hints++;
		num++;
	}
	Msg( "%6d active portals\n", num );
	Msg( "%6d hint portals\n", hints );
	return num;
}

/*
============
LoadPortals
============
*/
void LoadPortals( void )
{
	int		i, j, hint;
	vportal_t		*p;
	leaf_t		*l;
	char		magic[80];
	char		path[MAX_SYSPATH];
	int		numpoints;
	script_t		*prtfile;
	int		leafnums[2];
	vplane_t		plane;
	winding_t		*w;	

	com.sprintf( path, "maps/%s.prt", gs_filename );
	prtfile = Com_OpenScript( path, NULL, 0 );
	if( !prtfile ) Sys_Break( "LoadPortals: couldn't read %s\n", path );
	Msg( "reading %s\n", path );
	
	Com_ReadString( prtfile, true, magic );
          Com_ReadLong( prtfile, true, &portalclusters );
          Com_ReadLong( prtfile, true, &numportals );
          Com_ReadLong( prtfile, true, &numvisfaces );
          
          if( !portalclusters && !numportals ) Sys_Break( "LoadPortals: failed to read header\n" );
	if( com.strcmp( magic, PORTALFILE )) Sys_Break( "LoadPortals: not a portal file\n" );
	
	Msg( "%4i portalclusters\n", portalclusters );
	Msg( "%4i numportals\n", numportals );
	Msg( "%6i numfaces\n", numvisfaces );
	
	// these counts should take advantage of 64 bit systems automatically
	leafbytes = ((portalclusters+63)&~63)>>3;
	leaflongs = leafbytes/sizeof(long);
	
	portalbytes = ((numportals*2+63)&~63)>>3;
	portallongs = portalbytes/sizeof(long);

	// each file portal is split into two memory portals
	portals = BSP_Malloc( 2 * numportals * sizeof( vportal_t ));
	leafs = BSP_Malloc( portalclusters * sizeof(leaf_t));

	originalvismapsize = portalclusters * leafbytes;
	uncompressedvis = BSP_Malloc( originalvismapsize );
	vismap = vismap_p = dvisdata;
	dvis->numclusters = portalclusters;
	vismap_p = (byte *)&dvis->bitofs[portalclusters];
	vismap_end = vismap + MAX_MAP_VISIBILITY;
	
	for( i = 0; i < portalclusters; i++ )
		leafs[i].merged = -1;
			
	for( i = 0, p = portals; i < numportals; i++ )
	{
		Com_ReadLong( prtfile, true, &numpoints );
		Com_ReadLong( prtfile, false, &leafnums[0] );
		Com_ReadLong( prtfile, false, &leafnums[1] );		

		if( numpoints > MAX_POINTS_ON_WINDING )
			Sys_Break( "LoadPortals: portal %i has too many points\n", i );
		if(( uint )leafnums[0] > portalclusters || (uint)leafnums[1] > portalclusters )
			Sys_Break( "LoadPortals: reading portal %i\n", i );

		Com_ReadLong( prtfile, false, &hint );		
		w = p->winding = NewWinding( numpoints );
		w->numpoints = numpoints;

		for (j = 0; j < numpoints; j++)
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
		          
			for( k = 0; k < 3; k++ ) w->p[j][k] = v[k];
		}
		
		// calc plane
		PlaneFromWinding( w, &plane );

		// create forward portal
		l = &leafs[leafnums[0]];
		if( l->numportals == MAX_PORTALS_ON_LEAF ) Sys_Break( "Leaf with too many portals\n" );
		l->portals[l->numportals] = p;
		l->numportals++;
		
		p->num = i + 1;
		p->hint = hint;
		p->winding = w;
		VectorNegate( plane.normal, p->plane.normal );
		p->plane.dist = -plane.dist;
		p->leaf = leafnums[1];
		SetPortalSphere( p );
		p++;
		
		// create backwards portal
		l = &leafs[leafnums[1]];
		if (l->numportals == MAX_PORTALS_ON_LEAF) Sys_Error ("Leaf with too many portals");
		l->portals[l->numportals] = p;
		l->numportals++;
		
		p->num = i + 1;
		p->hint = hint;
		p->winding = NewWinding( w->numpoints );
		p->winding->numpoints = w->numpoints;
		for( j = 0; j < w->numpoints; j++ )
		{
			VectorCopy( w->p[w->numpoints-1-j], p->winding->p[j] );
		}

		p->plane = plane;
		p->leaf = leafnums[0];
		SetPortalSphere( p );
		p++;
	}

	faces = BSP_Malloc( 2 * numvisfaces * sizeof( vportal_t ));
	faceleafs = BSP_Malloc( portalclusters * sizeof( leaf_t ));

	for( i = 0, p = faces; i < numvisfaces; i++ )
	{
		Com_ReadLong( prtfile, true, &numpoints ); // newline
		Com_ReadLong( prtfile, false, &leafnums[0] );

		w = p->winding = NewWinding( numpoints );
		w->numpoints = numpoints;
		
		for( j = 0; j < numpoints; j++ )
		{
			token_t	token;
			vec3_t	v;
			int	k;

			// scanf into double, then assign to vec_t
			// so we don't care what size vec_t is
			Com_ReadToken( prtfile, 0, &token );	// get '(' symbol
			if( com.stricmp( token.string, "(" ))
				Sys_Break( "LoadPortals: not found ( reading surface %i\n", i );

			Com_ReadFloat( prtfile, false, &v[0] );
			Com_ReadFloat( prtfile, false, &v[1] );
			Com_ReadFloat( prtfile, false, &v[2] );
			
			Com_ReadToken( prtfile, 0, &token );
			if( com.stricmp( token.string, ")" ))
				Sys_Break( "LoadPortals: not found ) reading surface %i", i );
			for( k = 0; k < 3; k++ ) w->p[j][k] = v[k];
		}
		// calc plane
		PlaneFromWinding( w, &plane );

		l = &faceleafs[leafnums[0]];
		l->merged = -1;
		if( l->numportals == MAX_PORTALS_ON_LEAF )
			Sys_Break( "Leaf with too many faces\n" );
		l->portals[l->numportals] = p;
		l->numportals++;
		
		p->num = i + 1;
		p->winding = w;
		// normal pointing out of the leaf
		VectorSubtract( vec3_origin, plane.normal, p->plane.normal );
		p->plane.dist = -plane.dist;
		p->leaf = -1;
		SetPortalSphere( p );
		p++;
	}
	Com_CloseScript( prtfile );
}

void MergePortals( void )
{
	if( !mergevis ) return;
	MergeLeaves();
	MergeLeafPortals();
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
	for( j = 0; j < leafbytes; j++ )
	{
		bitbyte = scan[j];
		if( !bitbyte ) continue;
		for( k = 0; k < 8; k++ )
		{
			if(!( bitbyte & (1<<k))) continue;
			// OR this pvs row into the phs
			index = ((j<<3)+k);
			if( index >= portalclusters )
				Sys_Error( "Bad bit in PVS\n" ); // pad bits should be 0
			src = (long *)(uncompressedvis + index * leafbytes);
			dest = (long *)uncompressed;
			for (l = 0; l < leaflongs; l++) ((long *)uncompressed)[l] |= src[l];
		}
	}
	for( j = 0; j < portalclusters; j++ )
		if( uncompressed[j>>3] & (1<<( j & 7 )))
			totalphs++;

	// compress the bit string
	j = CompressVis( uncompressed, compressed );

	dest = (long *)vismap_p;
	vismap_p += j;
		
	if( vismap_p > vismap_end )
		Sys_Error( "ClusterPHS: vismap expansion overflow at cluster %i", clusternum );

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
	Msg ("Building PHS...\n");
	RunThreadsOnIndividual( portalclusters, true, ClusterPHS );

	Msg( "Total hearable clusters: %i\n", totalphs );
	Msg( "Average clusters hearable: %i\n", totalphs / portalclusters );
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
		// map not exist, create it
		WbspMain( false );
		LoadBSPFile();
	}

	if( numnodes == 0 || numsurfaces == 0 ) Sys_Error( "Empty map" );
	Msg ("---- Visibility ---- [%s]\n", fastvis ? "fast" : "full" );
	LoadPortals ();
	MergePortals ();
	CountActivePortals ();

	CalcPVS ();
	CalcPHS ();

	visdatasize = vismap_p - dvisdata;	
	Msg( "visdatasize: %i  compressed from %i\n", visdatasize, originalvismapsize * 2 );
          
	WriteBSPFile();	
}

