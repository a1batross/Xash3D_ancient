
#include "bsplib.h"

/*
==============================================================================

Leaf file generation

Save out name.line for qe3 to read
==============================================================================
*/


/*
=============
LeakFile

Finds the shortest possible chain of portals
that leads from the outside leaf to a specifically
occupied leaf
=============
*/
void LeakFile (tree_t *tree)
{
	vec3_t	mid;
	file_t	*linefile;
	char	path[MAX_SYSPATH];
	node_t	*node;
	int	count;

	if (!tree->outside_node.occupied)
		return;

	// write the points to the file
	sprintf (path, "%s.lin", gs_filename);
	linefile = FS_Open (path, "w" );
	if (!linefile) Sys_Error ("Couldn't open %s\n", path);

	count = 0;
	node = &tree->outside_node;
	while (node->occupied > 1)
	{
		int	next;
		portal_t	*p, *nextportal;
		node_t	*nextnode;
		int	s;

		// find the best portal exit
		next = node->occupied;
		for (p=node->portals ; p ; p = p->next[!s])
		{
			s = (p->nodes[0] == node);
			if (p->nodes[s]->occupied && p->nodes[s]->occupied < next)
			{
				nextportal = p;
				nextnode = p->nodes[s];
				next = nextnode->occupied;
			}
		}
		node = nextnode;
		WindingCenter (nextportal->winding, mid);
		FS_Printf(linefile, "%f %f %f\n", mid[0], mid[1], mid[2]);
		count++;
	}
	// add the occupant center
	GetVectorForKey (node->occupant, "origin", mid);

	FS_Printf (linefile, "%f %f %f\n", mid[0], mid[1], mid[2]);
	FS_Close (linefile);
}

