//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	prtfile.c - writes portal file
//=======================================================================

#include "bsplib.h"
#include "const.h"

/*
==============================================================================

Portal file generation

Save out name.prt for qvis to read
==============================================================================
*/


#define PORTALFILE		"PRT1"

file_t	*pf;
int	num_visclusters;				// clusters the player can be in
int	num_visportals;
int	num_solidfaces;

void WriteFloat( file_t *f, vec_t v )
{
	if ( fabs(v - floor(v + 0.5)) < 0.001 )
		FS_Printf(f,"%i ",(int)floor(v + 0.5));
	else FS_Printf(f,"%f ",v);
}

/*
=================
WritePortalFile_r
=================
*/
void WritePortalFile_r( node_t *node )
{
	int		i, s;	
	portal_t		*p;
	winding_t		*w;
	vec3_t		normal;
	vec_t		dist;

	// decision node
	if( node->planenum != PLANENUM_LEAF )
	{
		WritePortalFile_r (node->children[0]);
		WritePortalFile_r (node->children[1]);
		return;
	}
	
	if( node->opaque ) return;

	for( p = node->portals; p; p = p->next[s] )
	{
		w = p->winding;
		s = (p->nodes[1] == node);
		if( w && p->nodes[0] == node )
		{
			if( !Portal_Passable( p )) continue;

			// write out to the file
		
			// sometimes planes get turned around when they are very near
			// the changeover point between different axis.  interpret the
			// plane the same way vis will, and flip the side orders if needed
			// FIXME: is this still relevent?
			WindingPlane( w, normal, &dist );
			if( DotProduct( p->plane.normal, normal ) < 0.99 ) // backwards...
				FS_Printf( pf,"%i %i %i ",w->numpoints, p->nodes[1]->cluster, p->nodes[0]->cluster );
			else FS_Printf( pf,"%i %i %i ",w->numpoints, p->nodes[0]->cluster, p->nodes[1]->cluster );

			if( p->hint ) FS_Printf( pf, "1 " );
			else FS_Printf( pf, "0 " );

			for( i = 0; i < w->numpoints; i++ )
			{
				FS_Printf( pf,"( " );
				WriteFloat( pf, w->p[i][0] );
				WriteFloat( pf, w->p[i][1] );
				WriteFloat( pf, w->p[i][2] );
				FS_Printf( pf,") ");
			}
			FS_Printf( pf,"\n" );
		}
	}

}

/*
=================
WriteFaceFile_r
=================
*/
void WriteFaceFile_r( node_t *node )
{
	int		i, s;	
	portal_t		*p;
	winding_t		*w;

	// decision node
	if( node->planenum != PLANENUM_LEAF )
	{
		WriteFaceFile_r( node->children[0] );
		WriteFaceFile_r( node->children[1] );
		return;
	}
	
	if( node->opaque ) return;

	for( p = node->portals; p; p = p->next[s] )
	{
		w = p->winding;
		s = (p->nodes[1] == node);
		if( w )
		{
			if( Portal_Passable( p )) continue;

			// write out to the file
			if( p->nodes[0] == node )
			{
				FS_Printf( pf, "%i %i ", w->numpoints, p->nodes[0]->cluster );
				for( i = 0; i < w->numpoints; i++ )
				{
					FS_Printf( pf, "( " );
					WriteFloat( pf, w->p[i][0] );
					WriteFloat( pf, w->p[i][1] );
					WriteFloat( pf, w->p[i][2] );
					FS_Printf( pf,") " );
				}
				FS_Printf( pf,"\n" );
			}
			else
			{
				FS_Printf( pf, "%i %i ", w->numpoints, p->nodes[1]->cluster );
				for( i = w->numpoints - 1; i >= 0; i-- )
				{
					FS_Printf( pf, "( " );
					WriteFloat( pf, w->p[i][0] );
					WriteFloat( pf, w->p[i][1] );
					WriteFloat( pf, w->p[i][2] );
					FS_Printf( pf,") " );
				}
				FS_Printf( pf,"\n" );
			}
		}
	}
}

/*
================
NumberLeafs_r
================
*/
void NumberLeafs_r( node_t *node )
{
	portal_t	*p;

	if( node->planenum != PLANENUM_LEAF )
	{
		// decision node
		node->cluster = -99;
		NumberLeafs_r( node->children[0] );
		NumberLeafs_r( node->children[1] );
		return;
	}
	
	node->area = -1;

	if( node->opaque )
	{
		// solid block, viewpoint never inside
		node->cluster = -1;
		return;
	}

	node->cluster = num_visclusters;
	num_visclusters++;

	// count the portals
	for( p = node->portals; p; )
	{
		// only write out from first leaf
		if( p->nodes[0] == node )
		{
			if( Portal_Passable( p ))
				num_visportals++;
			else num_solidfaces++;
			p = p->next[0];
		}
		else
		{
			if( !Portal_Passable( p ))
				num_solidfaces++;
			p = p->next[1];		
		}
	}
}


/*
================
NumberClusters
================
*/
void NumberClusters( tree_t *tree )
{
	num_visclusters = 0;
	num_visportals = 0;
	num_solidfaces = 0;

	MsgDev( D_NOTE, "--- NumberClusters ---\n" );
	
	// set the cluster field in every leaf and count the total number of portals
	NumberLeafs_r( tree->headnode );

	MsgDev( D_INFO, "%5i visclusters\n", num_visclusters );
	MsgDev( D_INFO, "%5i visportals\n", num_visportals );
	MsgDev( D_INFO, "%5i solidfaces\n", num_solidfaces );
}

/*
================
WritePortalFile
================
*/
void WritePortalFile( tree_t *tree )
{
	Msg("--- WritePortalFile ---\n");

	// write the file
	pf = FS_Open(va( "maps/%s.prt", gs_filename ), "w" );
	if( !pf ) Sys_Break( "can't write %s.prt\n", gs_filename );

	// write header		
	FS_Printf( pf, "%s\n", PORTALFILE );
	FS_Printf( pf, "%i\n", num_visclusters );
	FS_Printf( pf, "%i\n", num_visportals );
	FS_Printf( pf, "%i\n", num_solidfaces );

	WritePortalFile_r( tree->headnode );
	WriteFaceFile_r( tree->headnode );
	FS_Close( pf );
}