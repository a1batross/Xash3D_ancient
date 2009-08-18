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
#define LEAKFILE_C



/* dependencies */
#include "q3map2.h"



/*
==============================================================================

LEAK FILE GENERATION

Save out name.line for qe3 to read
==============================================================================
*/


/*
=============
LeakFile

Finds the shortest possible chain of portals
that leads from the outside leaf to a specifically
occupied leaf

TTimo: builds a polyline xml node
=============
*/
void LeakFile( tree_t *tree )
{
	vec3_t	mid;
	file_t	*linefile;
	char	filename[MAX_SYSPATH];
	node_t	*node;
	int	count;

	if( !tree->outside_node.occupied )
		return;

	MsgDev( D_NOTE, "--- LeakFile ---\n" );

	// write the points to the file
	com.sprintf( filename, "%s.lin", source );
	linefile = FS_Open( filename, "w" );
	if( !linefile ) Sys_Error( "couldn't open %s\n", filename );

	count = 0;
	node = &tree->outside_node;
	while( node->occupied > 1 )
	{
		int		next;
		portal_t		*p, *nextportal;
		node_t		*nextnode;
		int		s;

		// find the best portal exit
		next = node->occupied;
		for( p = node->portals; p; p = p->next[!s] )
		{
			s = (p->nodes[0] == node);
			if( p->nodes[s]->occupied && p->nodes[s]->occupied < next )
			{
				nextportal = p;
				nextnode = p->nodes[s];
				next = nextnode->occupied;
			}
		}
		node = nextnode;
		WindingCenter( nextportal->winding, mid );
		FS_Printf( linefile, "%f %f %f\n", mid[0], mid[1], mid[2] );
		count++;
	}
	// add the occupant center
	GetVectorForKey( node->occupant, "origin", mid );

	FS_Printf( linefile, "%f %f %f\n", mid[0], mid[1], mid[2] );
	MsgDev( D_INFO, "%9d point linefile\n", count + 1 );

	FS_Close( linefile );
}