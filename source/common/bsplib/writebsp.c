//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        bsp_emit.c - emit bsp nodes and leafs
//=======================================================================

#include "bsplib.h"
#include "const.h"

int	c_nofaces;
int	c_facenodes;

/*
============
EmitPlanes

There is no oportunity to discard planes, because all of the original
brushes will be saved in the map.
============
*/
void EmitPlanes( void )
{
	int		i;
	dplane_t		*dp;
	plane_t		*mp;

	mp = mapplanes;
	for( i = 0; i < nummapplanes; i++, mp++ )
	{
		dp = &dplanes[numplanes];
		VectorCopy( mp->normal, dp->normal );
		dp->dist = mp->dist;
		numplanes++;
	}
}


//========================================================

void EmitMarkFace (dleaf_t *leaf_p, face_t *f)
{
	int			i;
	int			facenum;

	while (f->merged)
		f = f->merged;

	if (f->split[0])
	{
		EmitMarkFace (leaf_p, f->split[0]);
		EmitMarkFace (leaf_p, f->split[1]);
		return;
	}

	facenum = f->outputnumber;
	if (facenum == -1)
		return;	// degenerate face

	if( facenum < 0 || facenum >= numsurfaces)
		Sys_Error ("Bad leafface");
	for (i=leaf_p->firstleafsurface ; i<numleafsurfaces ; i++)
		if (dleafsurfaces[i] == facenum)
			break;		// merged out face
	if (i == numleafsurfaces)
	{
		if( numleafsurfaces >= MAX_MAP_LEAFFACES )
			Sys_Error( "MAX_MAP_LEAFFACES limit exceeded\n" );

		dleafsurfaces[numleafsurfaces] =  facenum;
		numleafsurfaces++;
	}

}


/*
==================
EmitLeaf
==================
*/
void EmitLeaf( node_t *node )
{
	dleaf_t		*leaf_p;
	portal_t		*p;
	int		s;
	face_t		*f;
	bspbrush_t	*b;
	int		i, brushnum;

	// emit a leaf
	if( numleafs >= MAX_MAP_LEAFS )
		Sys_Error( "MAX_MAP_LEAFS  limit exceeded\n" );

	leaf_p = &dleafs[numleafs];
	numleafs++;

	leaf_p->contents = node->contents;
	leaf_p->cluster = node->cluster;
	leaf_p->area = node->area;

	//
	// write bounding box info
	//	
	VectorCopy (node->mins, leaf_p->mins);
	VectorCopy (node->maxs, leaf_p->maxs);
	
	//
	// write the leafbrushes
	//
	leaf_p->firstleafbrush = numleafbrushes;
	for( b = node->brushlist; b; b = b->next )
	{
		if( numleafbrushes >= MAX_MAP_LEAFBRUSHES )
			Sys_Error( "MAX_MAP_LEAFBRUSHES limit exceeded\n" );

		brushnum = b->original - mapbrushes;
		for( i = leaf_p->firstleafbrush; i < numleafbrushes; i++ )
			if( dleafbrushes[i] == brushnum ) break;

		if( i == numleafbrushes )
		{
			dleafbrushes[numleafbrushes] = brushnum;
			numleafbrushes++;
		}
	}
	leaf_p->numleafbrushes = numleafbrushes - leaf_p->firstleafbrush;

	// write the leaffaces
	if( leaf_p->contents & CONTENTS_SOLID )
		return; // no leaffaces in solids

	leaf_p->firstleafsurface = numleafsurfaces;

	for (p = node->portals ; p ; p = p->next[s])	
	{
		s = (p->nodes[1] == node);
		f = p->face[s];
		if (!f)
			continue;	// not a visible portal

		EmitMarkFace (leaf_p, f);
	}
	
	leaf_p->numleafsurfaces = numleafsurfaces - leaf_p->firstleafsurface;
}


/*
==================
EmitFace
==================
*/
void EmitFace( face_t *f )
{
	dsurface_t	*df;
	int		i;
	int		e;

	f->outputnumber = -1;

	if( f->numpoints < 3 )
	{
		return;		// degenerated
	}
	if( f->merged || f->split[0] || f->split[1] )
	{
		return;		// not a final face
	}

	// save output number so leaffaces can use
	f->outputnumber = numsurfaces;

	if( numsurfaces >= MAX_MAP_SURFACES )
		Sys_Error( "MAX_MAP_SURFACES limit exceeded\n" );
	df = &dsurfaces[numsurfaces];
	numsurfaces++;

	// planenum is used by qlight, but not quake
	df->planenum = f->planenum & (~1);
	df->side = f->planenum & 1;

	df->firstedge = numsurfedges;
	df->numedges = f->numpoints;
	df->texinfo = f->texinfo;
	for( i = 0; i < f->numpoints; i++ )
	{
		e = GetEdge2( f->vertexnums[i], f->vertexnums[(i+1)%f->numpoints], f );
		if( numsurfedges >= MAX_MAP_SURFEDGES )
			Sys_Error( "MAX_MAP_SURFEDGES limit exceeded\n" );
		dsurfedges[numsurfedges] = e;
		numsurfedges++;
	}
}

/*
============
EmitDrawingNode_r
============
*/
int EmitDrawNode_r (node_t *node)
{
	dnode_t	*n;
	face_t	*f;
	int		i;

	if (node->planenum == PLANENUM_LEAF)
	{
		EmitLeaf (node);
		return -numleafs;
	}

	// emit a node	
	if( numnodes == MAX_MAP_NODES )
		Sys_Error( "MAX_MAP_NODES limit exceeded\n" );
	n = &dnodes[numnodes];
	numnodes++;

	VectorCopy( node->mins, n->mins );
	VectorCopy( node->maxs, n->maxs );

	if( node->planenum & 1 )
		Sys_Error ("WriteDrawNodes_r: odd planenum");
	n->planenum = node->planenum;
	n->firstsurface = numsurfaces;

	if (!node->faces)
		c_nofaces++;
	else
		c_facenodes++;

	for( f = node->faces; f; f = f->next )
		EmitFace (f);

	n->numsurfaces = numsurfaces - n->firstsurface;

	// recursively output the other nodes
	for( i = 0; i < 2; i++ )
	{
		if( node->children[i]->planenum == PLANENUM_LEAF )
		{
			n->children[i] = -(numleafs + 1);
			EmitLeaf( node->children[i] );
		}
		else
		{
			n->children[i] = numnodes;	
			EmitDrawNode_r( node->children[i] );
		}
	}
	return n - dnodes;
}

//=========================================================


/*
============
WriteBSP

Wirte Binary Spacing Partition Tree
============
*/
void WriteBSP( node_t *headnode )
{
	int	oldsurfaces;

	c_nofaces = 0;
	c_facenodes = 0;

	oldsurfaces = numsurfaces;
	dmodels[nummodels].headnode = EmitDrawNode_r( headnode );
	EmitAreaPortals( headnode );
}

//===========================================================

/*
============
SetModelNumbers
============
*/
void SetModelNumbers( void )
{
	int	i, models = 1;

	for( i = 1; i < num_entities; i++ )
	{
		if( entities[i].numbrushes )
		{
			SetKeyValue( &entities[i], "model", va( "*%i", models ));
			models++;
		}
	}
}

/*
============
SetLightStyles
============
*/
void SetLightStyles( void )
{
	char		*t;
	bsp_entity_t	*e;
	int		i, j, k, style, stylenum = 0;
	char		lightTargets[MAX_SWITCHED_LIGHTS][64];
	int		lightStyles[MAX_SWITCHED_LIGHTS];

	// any light that is controlled (has a targetname)
	// must have a unique style number generated for it
	for( i = 1; i < num_entities; i++ )
	{
		e = &entities[i];

		t = ValueForKey( e, "classname" );
		if( com.strncmp( t, "light", 5 ))
		{
			if(!com.strncmp( t, "func_light", 10 ))
			{
				// may create func_light family 
				// e.g. func_light_fluoro, func_light_broken etc
				k = com.atoi(ValueForKey( e, "spawnflags" ));
				if( k & SF_START_ON ) t = "-2"; // initially on
				else t = "-1"; // initially off
			}
			else t = ValueForKey( e, "style" );
			
			switch( com.atoi( t ))
			{
			case 0: continue; // not a light, no style, generally pretty boring
			case -1: // normal switchable texlight (start off)
				SetKeyValue( e, "style", va( "%i", 32 + stylenum ));
				stylenum++;
				continue;
			case -2: // backwards switchable texlight (start on)
				SetKeyValue(e, "style", va( "%i", -(32 + stylenum )));
				stylenum++;
				continue;
			default: continue; // nothing to set
			}
                    }
		t = ValueForKey( e, "targetname" );
		if( !t[0] ) continue;

		// get existing style
		style = IntForKey( e, "style" );
		if( style < LS_NORMAL || style > LS_NONE )
			Sys_Break( "Invalid lightstyle (%d) on entity %d\n", style, i );
		
		// find this targetname
		for( j = 0; j < stylenum; j++ )
		{
			if( lightStyles[j] == style && !com.strcmp( lightTargets[j], t ))
				break;
		}
		if( j == stylenum )
		{
			if( stylenum == MAX_SWITCHED_LIGHTS )
			{
				MsgDev( D_WARN, "switchable lightstyles limit (%i) exceeded at entity #%i\n", 64, i );
				break; // nothing to process
			}
			com.strncpy( lightTargets[j], t, MAX_SHADERPATH );
			lightStyles[j] = style;
			stylenum++;
		}
		SetKeyValue( e, "style", va( "%i", 32 + j ));
	}
}

//===========================================================

/*
============
EmitBrushes
============
*/
void EmitBrushes( void )
{
	int		i, j, bnum, s, x;
	dbrush_t		*db;
	mapbrush_t	*b;
	dbrushside_t	*cp;
	vec3_t		normal;
	vec_t		dist;
	int		planenum;

	numbrushsides = 0;
	numbrushes = nummapbrushes;

	for( bnum = 0; bnum < nummapbrushes; bnum++ )
	{
		b = &mapbrushes[bnum];
		db = &dbrushes[bnum];

		db->shadernum = b->shadernum;
		db->firstside = numbrushsides;
		db->numsides = b->numsides;
		for( j = 0; j < b->numsides; j++ )
		{
			if( numbrushsides == MAX_MAP_BRUSHSIDES )
				Sys_Error( "MAX_MAP_BRUSHSIDES limit exceeded\n" );
			cp = &dbrushsides[numbrushsides];
			numbrushsides++;
			cp->planenum = b->original_sides[j].planenum;
			cp->texinfo = b->original_sides[j].texinfo;
		}

		// add any axis planes not contained in the brush to bevel off corners
		for( x = 0; x < 3; x++ )
			for (s=-1 ; s<=1 ; s+=2)
			{
				// add the plane
				VectorCopy (vec3_origin, normal);
				normal[x] = s;
				if (s == -1)
					dist = -b->mins[x];
				else
					dist = b->maxs[x];
				planenum = FindFloatPlane (normal, dist);
				for( i = 0; i < b->numsides; i++ )
					if( b->original_sides[i].planenum == planenum )
						break;
				if( i == b->numsides )
				{
					if( numbrushsides >= MAX_MAP_BRUSHSIDES )
						Sys_Error( "MAX_MAP_BRUSHSIDES limit exceeded\n" );

					dbrushsides[numbrushsides].planenum = planenum;
					dbrushsides[numbrushsides].texinfo = dbrushsides[numbrushsides-1].texinfo;
					numbrushsides++;
					db->numsides++;
				}
			}

	}

}

//===========================================================

/*
==================
BeginBSPFile
==================
*/
void BeginBSPFile( void )
{
	// these values may actually be initialized
	// if the file existed when loaded, so clear them explicitly

	nummodels = 0;
	numsurfaces = 0;
	numnodes = 0;
	numbrushsides = 0;
	numvertexes = 0;
	numleafsurfaces = 0;
	numleafbrushes = 0;
	numsurfedges = 0;
	numedges = 1;	// edge 0 is not used, because 0 can't be negated
	numvertexes = 1;	// leave vertex 0 as an error
	numleafs = 1;	// leave leaf 0 as an error

	dleafs[0].contents = CONTENTS_SOLID;
}


/*
============
EndBSPFile
============
*/
void EndBSPFile( void )
{
	EmitBrushes ();
	EmitPlanes ();
	UnparseEntities ();

	// write the map
	WriteBSPFile();
}


/*
==================
BeginModel
==================
*/
int	firstmodleaf;
extern	int firstmodeledge;
extern	int firstmodelface;

void BeginModel( void )
{
	dmodel_t		*mod;
	mapbrush_t	*b;
	bsp_entity_t	*e;
	int		j, start, end;
	vec3_t		mins, maxs;

	if( nummodels == MAX_MAP_MODELS )
		Sys_Error( "MAX_MAP_MODELS limit exceeded\n" );
	mod = &dmodels[nummodels];

	mod->firstsurface = numsurfaces;
	firstmodleaf = numleafs;
	firstmodeledge = numedges;
	firstmodelface = numsurfaces;

	// bound the brushes
	e = &entities[entity_num];
	mod->firstbrush = start = e->firstbrush;
	mod->numbrushes = e->numbrushes;
	end = start + e->numbrushes;
	ClearBounds (mins, maxs);

	for( j = start; j < end; j++ )
	{
		b = &mapbrushes[j];
		if (!b->numsides) continue; // not a real brush (origin brush)
		AddPointToBounds( b->mins, mins, maxs );
		AddPointToBounds( b->maxs, mins, maxs );
	}
	VectorCopy( mins, mod->mins );
	VectorCopy( maxs, mod->maxs );
}


/*
==================
EndModel
==================
*/
void EndModel( void )
{
	dmodel_t	*mod;

	mod = &dmodels[nummodels];
	mod->numsurfaces = numsurfaces - mod->firstsurface;
	nummodels++;
}