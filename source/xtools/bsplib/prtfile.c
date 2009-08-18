/* -------------------------------------------------------------------------------

Copyright (C) 1999-2007 id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

----------------------------------------------------------------------------------

This code has been altered significantly from its original form, to support
several games based on the Quake III Arena engine, in the form of "Q3Map2."

------------------------------------------------------------------------------- */



/* marker */
#define PRTFILE_C



/* dependencies */
#include "q3map2.h"



/*
==============================================================================

PORTAL file_t GENERATION

Save out name.prt for qvis to read
==============================================================================
*/


#define	PORTALfile_t	"PRT1"

file_t		*pf;
int		num_visclusters;				// clusters the player can be in
int		num_visportals;
int		num_solidfaces;

void WriteFloat (file_t *f, vec_t v)
{
	if ( fabs(v - Q_rint(v)) < 0.001 )
		FS_Printf (f,"%i ",(int)Q_rint(v));
	else
		FS_Printf (f,"%f ",v);
}

/*
=================
WritePortalFile_r
=================
*/
void WritePortalFile_r (node_t *node)
{
	int			i, s;	
	portal_t	*p;
	winding_t	*w;
	vec3_t		normal;
	vec_t		dist;

	// decision node
	if (node->planenum != PLANENUM_LEAF) {
		WritePortalFile_r (node->children[0]);
		WritePortalFile_r (node->children[1]);
		return;
	}
	
	if (node->opaque) {
		return;
	}

	for (p = node->portals ; p ; p=p->next[s])
	{
		w = p->winding;
		s = (p->nodes[1] == node);
		if (w && p->nodes[0] == node)
		{
			if (!PortalPassable(p))
				continue;
			// write out to the file
			
			// sometimes planes get turned around when they are very near
			// the changeover point between different axis.  interpret the
			// plane the same way vis will, and flip the side orders if needed
			// FIXME: is this still relevent?
			WindingPlane (w, normal, &dist);
			if ( DotProduct (p->plane.normal, normal) < 0.99 )
			{	// backwards...
				FS_Printf (pf,"%i %i %i ",w->numpoints, p->nodes[1]->cluster, p->nodes[0]->cluster);
			}
			else
				FS_Printf (pf,"%i %i %i ",w->numpoints, p->nodes[0]->cluster, p->nodes[1]->cluster);
			
			/* ydnar: added this change to make antiportals work */
			if( p->compileFlags & C_HINT )
				FS_Printf( pf, "1 " );
			else
				FS_Printf( pf, "0 " );
			
			/* write the winding */
			for (i=0 ; i<w->numpoints ; i++)
			{
				FS_Printf (pf,"(");
				WriteFloat (pf, w->p[i][0]);
				WriteFloat (pf, w->p[i][1]);
				WriteFloat (pf, w->p[i][2]);
				FS_Printf (pf,") ");
			}
			FS_Printf (pf,"\n");
		}
	}

}

/*
=================
WriteFaceFile_r
=================
*/
void WriteFaceFile_r (node_t *node)
{
	int			i, s;	
	portal_t	*p;
	winding_t	*w;

	// decision node
	if (node->planenum != PLANENUM_LEAF) {
		WriteFaceFile_r (node->children[0]);
		WriteFaceFile_r (node->children[1]);
		return;
	}
	
	if (node->opaque) {
		return;
	}

	for (p = node->portals ; p ; p=p->next[s])
	{
		w = p->winding;
		s = (p->nodes[1] == node);
		if (w)
		{
			if (PortalPassable(p))
				continue;
			// write out to the file

			if (p->nodes[0] == node)
			{
				FS_Printf (pf,"%i %i ",w->numpoints, p->nodes[0]->cluster);
				for (i=0 ; i<w->numpoints ; i++)
				{
					FS_Printf (pf,"(");
					WriteFloat (pf, w->p[i][0]);
					WriteFloat (pf, w->p[i][1]);
					WriteFloat (pf, w->p[i][2]);
					FS_Printf (pf,") ");
				}
				FS_Printf (pf,"\n");
			}
			else
			{
				FS_Printf (pf,"%i %i ",w->numpoints, p->nodes[1]->cluster);
				for (i = w->numpoints-1; i >= 0; i--)
				{
					FS_Printf (pf,"(");
					WriteFloat (pf, w->p[i][0]);
					WriteFloat (pf, w->p[i][1]);
					WriteFloat (pf, w->p[i][2]);
					FS_Printf (pf,") ");
				}
				FS_Printf (pf,"\n");
			}
		}
	}
}

/*
================
NumberLeafs_r
================
*/
void NumberLeafs_r (node_t *node)
{
	portal_t	*p;

	if ( node->planenum != PLANENUM_LEAF ) {
		// decision node
		node->cluster = -99;
		NumberLeafs_r (node->children[0]);
		NumberLeafs_r (node->children[1]);
		return;
	}
	
	node->area = -1;

	if ( node->opaque ) {
		// solid block, viewpoint never inside
		node->cluster = -1;
		return;
	}

	node->cluster = num_visclusters;
	num_visclusters++;

	// count the portals
	for (p = node->portals ; p ; )
	{
		if (p->nodes[0] == node)		// only write out from first leaf
		{
			if (PortalPassable(p))
				num_visportals++;
			else
				num_solidfaces++;
			p = p->next[0];
		}
		else
		{
			if (!PortalPassable(p))
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
void NumberClusters(tree_t *tree) {
	num_visclusters = 0;
	num_visportals = 0;
	num_solidfaces = 0;

	MsgDev( D_NOTE, "--- NumberClusters ---\n");
	
	// set the cluster field in every leaf and count the total number of portals
	NumberLeafs_r (tree->headnode);

	MsgDev( D_INFO, "%9d visclusters\n", num_visclusters );
	MsgDev( D_INFO, "%9d visportals\n", num_visportals );
	MsgDev( D_INFO, "%9d solidfaces\n", num_solidfaces );
}

/*
================
WritePortalFile
================
*/
void WritePortalFile( tree_t *tree )
{
	char	filename[MAX_SYSPATH];

	MsgDev( D_NOTE, "--- WritePortalFile ---\n" );
	
	// write the file
	com.sprintf( filename, "%s.prt", source );
	Msg( "writing %s\n", filename );
	pf = FS_Open( filename, "w" );

	if( !pf ) Sys_Error( "error opening %s\n", filename );
		
	FS_Printf (pf, "%s\n", PORTALfile_t);
	FS_Printf (pf, "%i\n", num_visclusters);
	FS_Printf (pf, "%i\n", num_visportals);
	FS_Printf (pf, "%i\n", num_solidfaces);

	WritePortalFile_r(tree->headnode);
	WriteFaceFile_r(tree->headnode);

	FS_Close( pf );
}