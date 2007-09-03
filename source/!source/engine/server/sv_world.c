/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// world.c -- world query functions

#include "engine.h"
#include "server.h"

/*
===============================================================================

ENTITY AREA CHECKING

FIXME: this use of "area" is different from the bsp file use
===============================================================================
*/
typedef struct areanode_s
{
	int		axis;		// -1 = leaf node
	float	dist;
	struct areanode_s	*children[2];
	link_t	trigger_edicts;
	link_t	solid_edicts;
} areanode_t;

#define	AREA_DEPTH	4
#define	AREA_NODES	32

areanode_t	sv_areanodes[AREA_NODES];
int			sv_numareanodes;

float		*area_mins, *area_maxs;
prvm_edict_t	**area_list;
int		area_count, area_maxcount;
int		area_type;

int SV_HullForEntity (prvm_edict_t *ent);


// ClearLink is used for new headnodes
void ClearLink (link_t *l)
{
	l->prev = l->next = l;
}

void RemoveLink (link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void InsertLinkBefore (link_t *l, link_t *before)
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

/*
===============
SV_CreateAreaNode

Builds a uniformly subdivided tree for the given world size
===============
*/
areanode_t *SV_CreateAreaNode (int depth, vec3_t mins, vec3_t maxs)
{
	areanode_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1, mins2, maxs2;

	anode = &sv_areanodes[sv_numareanodes];
	sv_numareanodes++;

	ClearLink (&anode->trigger_edicts);
	ClearLink (&anode->solid_edicts);
	
	if (depth == AREA_DEPTH)
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}
	
	VectorSubtract (maxs, mins, size);
	if (size[0] > size[1])
		anode->axis = 0;
	else
		anode->axis = 1;
	
	anode->dist = 0.5 * (maxs[anode->axis] + mins[anode->axis]);
	VectorCopy (mins, mins1);	
	VectorCopy (mins, mins2);	
	VectorCopy (maxs, maxs1);	
	VectorCopy (maxs, maxs2);	
	
	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;
	
	anode->children[0] = SV_CreateAreaNode (depth+1, mins2, maxs2);
	anode->children[1] = SV_CreateAreaNode (depth+1, mins1, maxs1);

	return anode;
}

/*
===============
SV_ClearWorld

===============
*/
void SV_ClearWorld (void)
{
	memset (sv_areanodes, 0, sizeof(sv_areanodes));
	sv_numareanodes = 0;
	SV_CreateAreaNode (0, sv.models[1]->mins, sv.models[1]->maxs);
}


/*
===============
SV_UnlinkEdict

===============
*/
void SV_UnlinkEdict (prvm_edict_t *ent)
{
	if (!ent->priv.sv->area.prev) return; // not linked in anywhere
	RemoveLink (&ent->priv.sv->area);
	ent->priv.sv->area.prev = ent->priv.sv->area.next = NULL;
}


/*
===============
SV_LinkEdict

===============
*/
#define MAX_TOTAL_ENT_LEAFS		128
void SV_LinkEdict (prvm_edict_t *ent)
{
	areanode_t	*node;
	int			leafs[MAX_TOTAL_ENT_LEAFS];
	int			clusters[MAX_TOTAL_ENT_LEAFS];
	int			num_leafs;
	int			i, j, k;
	int			area;
	int			topnode;

	if (ent->priv.sv->area.prev) SV_UnlinkEdict (ent); // unlink from old position
	if (ent == prog->edicts) return; // don't add the world
	if (!ent->priv.sv->free) return;

	// set the size
	VectorSubtract (ent->priv.sv->maxs, ent->priv.sv->mins, ent->priv.sv->size);
	
	// encode the size into the entity_state for client prediction
	if (ent->priv.sv->solid == SOLID_BBOX && !(ent->priv.sv->flags & SVF_DEADMONSTER))
	{
		// assume that x/y are equal and symetric
		i = ent->priv.sv->maxs[0]/8;
		if (i<1) i = 1;
		if (i>31) i = 31;

		// z is not symetric
		j = (-ent->priv.sv->mins[2])/8;
		if (j < 1) j = 1;
		if (j > 31) j = 31;

		// and z maxs can be negative...
		k = (ent->priv.sv->maxs[2]+32)/8;
		if (k<1) k = 1;
		if (k>63) k = 63;
		ent->priv.sv->state.solid = (k<<10) | (j<<5) | i;
	}
	else if (ent->priv.sv->solid == SOLID_BSP)
	{
		ent->priv.sv->state.solid = 31; // a solid_bbox will never create this value
	}
	else ent->priv.sv->state.solid = 0;

	// set the abs box
	if (ent->priv.sv->solid == SOLID_BSP && (ent->priv.sv->state.angles[0] || ent->priv.sv->state.angles[1] || ent->priv.sv->state.angles[2]) )
	{
		// expand for rotation
		float		max = 0, v;
		int		i;

		for (i = 0; i < 3; i++)
		{
			v =fabs( ent->priv.sv->mins[i]);
			if (v > max) max = v;
			v =fabs( ent->priv.sv->maxs[i]);
			if (v > max) max = v;
		}
		for (i = 0; i < 3; i++)
		{
			ent->priv.sv->absmin[i] = ent->priv.sv->state.origin[i] - max;
			ent->priv.sv->absmax[i] = ent->priv.sv->state.origin[i] + max;
		}
	}
	else
	{	// normal
		VectorAdd (ent->priv.sv->state.origin, ent->priv.sv->mins, ent->priv.sv->absmin);	
		VectorAdd (ent->priv.sv->state.origin, ent->priv.sv->maxs, ent->priv.sv->absmax);
	}

	// because movement is clipped an epsilon away from an actual edge,
	// we must fully check even when bounding boxes don't quite touch
	ent->priv.sv->absmin[0] -= 1;
	ent->priv.sv->absmin[1] -= 1;
	ent->priv.sv->absmin[2] -= 1;
	ent->priv.sv->absmax[0] += 1;
	ent->priv.sv->absmax[1] += 1;
	ent->priv.sv->absmax[2] += 1;

	// link to PVS leafs
	ent->priv.sv->num_clusters = 0;
	ent->priv.sv->areanum = 0;
	ent->priv.sv->areanum2 = 0;

	//get all leafs, including solids
	num_leafs = CM_BoxLeafnums (ent->priv.sv->absmin, ent->priv.sv->absmax, leafs, MAX_TOTAL_ENT_LEAFS, &topnode);

	// set areas
	for (i = 0; i < num_leafs; i++)
	{
		clusters[i] = CM_LeafCluster (leafs[i]);
		area = CM_LeafArea (leafs[i]);
		if (area)
		{	// doors may legally straggle two areas,
			// but nothing should evern need more than that
			if (ent->priv.sv->areanum && ent->priv.sv->areanum != area)
			{
				if (ent->priv.sv->areanum2 && ent->priv.sv->areanum2 != area && sv.state == ss_loading)
					MsgWarn("SV_LinkEdict: object touching 3 areas at %f %f %f\n", ent->priv.sv->absmin[0], ent->priv.sv->absmin[1], ent->priv.sv->absmin[2]);
				ent->priv.sv->areanum2 = area;
			}
			else ent->priv.sv->areanum = area;
		}
	}

	if (num_leafs >= MAX_TOTAL_ENT_LEAFS)
	{
		// assume we missed some leafs, and mark by headnode
		ent->priv.sv->num_clusters = -1;
		ent->priv.sv->headnode = topnode;
	}
	else
	{
		ent->priv.sv->num_clusters = 0;
		for (i = 0; i < num_leafs; i++)
		{
			if (clusters[i] == -1) continue; // not a visible leaf
			for (j = 0; j < i; j++)
			{
				if (clusters[j] == clusters[i]) break;
			}
			if (j == i)
			{
				if (ent->priv.sv->num_clusters == MAX_ENT_CLUSTERS)
				{
					// assume we missed some leafs, and mark by headnode
					ent->priv.sv->num_clusters = -1;
					ent->priv.sv->headnode = topnode;
					break;
				}
				ent->priv.sv->clusternums[ent->priv.sv->num_clusters++] = clusters[i];
			}
		}
	}

	// if first time, make sure old_origin is valid
	if (!ent->priv.sv->linkcount)
	{
		VectorCopy (ent->priv.sv->state.origin, ent->priv.sv->state.old_origin);
	}
	ent->priv.sv->linkcount++;

	if (ent->priv.sv->solid == SOLID_NOT) return;

	// find the first node that the ent's box crosses
	node = sv_areanodes;
	while (1)
	{
		if (node->axis == -1) break;
		if (ent->priv.sv->absmin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->priv.sv->absmax[node->axis] < node->dist)
			node = node->children[1];
		else break; // crosses the node
	}
	
	// link it in	
	if (ent->priv.sv->solid == SOLID_TRIGGER) InsertLinkBefore (&ent->priv.sv->area, &node->trigger_edicts);
	else InsertLinkBefore (&ent->priv.sv->area, &node->solid_edicts);

}


/*
====================
SV_AreaEdicts_r

====================
*/
void SV_AreaEdicts_r (areanode_t *node)
{
	link_t		*l, *next, *start;
	prvm_edict_t	*check;
	int		count = 0;

	// touch linked edicts
	if (area_type == AREA_SOLID)
		start = &node->solid_edicts;
	else start = &node->trigger_edicts;

	for (l = start->next; l != start; l = next)
	{
		next = l->next;
		check = PRVM_EDICT_FROM_AREA(l);

		if (check->priv.sv->solid == SOLID_NOT) continue; // deactivated
		if (check->priv.sv->absmin[0] > area_maxs[0] || check->priv.sv->absmin[1] > area_maxs[1] || check->priv.sv->absmin[2] > area_maxs[2]
		|| check->priv.sv->absmax[0] < area_mins[0] || check->priv.sv->absmax[1] < area_mins[1] || check->priv.sv->absmax[2] < area_mins[2])
			continue;	// not touching

		if (area_count == area_maxcount)
		{
			MsgWarn("SV_AreaEdicts: maxcount\n");
			return;
		}

		area_list[area_count] = check;
		area_count++;
	}
	
	if (node->axis == -1) return;	// terminal node

	// recurse down both sides
	if ( area_maxs[node->axis] > node->dist )
		SV_AreaEdicts_r ( node->children[0] );
	if ( area_mins[node->axis] < node->dist )
		SV_AreaEdicts_r ( node->children[1] );
}

/*
================
SV_AreaEdicts
================
*/
int SV_AreaEdicts (vec3_t mins, vec3_t maxs, prvm_edict_t **list, int maxcount, int areatype)
{
	area_mins = mins;
	area_maxs = maxs;
	area_list = list;
	area_count = 0;
	area_maxcount = maxcount;
	area_type = areatype;

	SV_AreaEdicts_r (sv_areanodes);

	return area_count;
}


//===========================================================================

/*
=============
SV_PointContents
=============
*/
int SV_PointContents (vec3_t p)
{
	prvm_edict_t	*touch[MAX_EDICTS], *hit;
	int		i, num;
	int		contents, c2;
	int		headnode;
	float		*angles;

	// get base contents from world
	contents = CM_PointContents (p, sv.models[1]->headnode);

	// or in contents from all the other entities
	num = SV_AreaEdicts (p, p, touch, MAX_EDICTS, AREA_SOLID);

	for (i = 0; i < num; i++)
	{
		hit = touch[i];

		// might intersect, so do an exact clip
		headnode = SV_HullForEntity (hit);
		angles = hit->priv.sv->state.angles;
		if (hit->priv.sv->solid != SOLID_BSP) angles = vec3_origin; // boxes don't rotate
		c2 = CM_TransformedPointContents (p, headnode, hit->priv.sv->state.origin, hit->priv.sv->state.angles);
		contents |= c2;
	}
	return contents;
}

/*
================
SV_HullForEntity

Returns a headnode that can be used for testing or clipping an
object of mins/maxs size.
Offset is filled in to contain the adjustment that must be added to the
testing object's origin to get a point to use with the returned hull.
================
*/
int SV_HullForEntity (prvm_edict_t *ent)
{
	cmodel_t	*model;

	// decide which clipping hull to use, based on the size
	if (ent->priv.sv->solid == SOLID_BSP)
	{	
		// explicit hulls in the BSP model
		model = sv.models[ ent->priv.sv->state.modelindex ];

		if (!model) 
		{
			MsgWarn("SV_HullForEntity: movetype_push with a non bsp model\n");
			return -1;
		}
		return model->headnode;
	}

	// create a temp hull from bounding box sizes
	return CM_HeadnodeForBox (ent->priv.sv->mins, ent->priv.sv->maxs);
}

/*
====================
SV_ClipMoveToEntities

====================
*/
void SV_ClipMoveToEntities ( moveclip_t *clip )
{
	int		i, num;
	prvm_edict_t	*touchlist[MAX_EDICTS], *touch;
	trace_t		trace;
	int		headnode;
	float		*angles;

	num = SV_AreaEdicts (clip->boxmins, clip->boxmaxs, touchlist, MAX_EDICTS, AREA_SOLID);

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for (i = 0; i < num; i++)
	{
		touch = touchlist[i];
		if (touch->priv.sv->solid == SOLID_NOT) continue;
		if (touch == clip->passedict) continue;
		if (clip->trace.allsolid) return;
		if (clip->passedict)
		{
		 	if (touch->priv.sv->owner == clip->passedict->priv.sv) continue; // don't clip against own missiles
			if (clip->passedict->priv.sv->owner == touch->priv.sv) continue; // don't clip against owner
		}

		if ( !(clip->contentmask & CONTENTS_DEADMONSTER) && (touch->priv.sv->flags & SVF_DEADMONSTER) )
			continue;

		// might intersect, so do an exact clip
		headnode = SV_HullForEntity (touch);
		angles = touch->priv.sv->state.angles;
		if (touch->priv.sv->solid != SOLID_BSP) angles = vec3_origin; // boxes don't rotate

		if (touch->priv.sv->flags & SVF_MONSTER)
		{
			trace = CM_TransformedBoxTrace (clip->start, clip->end, clip->mins2, clip->maxs2, headnode, clip->contentmask, touch->priv.sv->state.origin, angles);
		}
		else
		{
			trace = CM_TransformedBoxTrace (clip->start, clip->end, clip->mins, clip->maxs, headnode,  clip->contentmask, touch->priv.sv->state.origin, angles);
		}
		if (trace.allsolid || trace.startsolid || trace.fraction < clip->trace.fraction)
		{
			trace.ent = touch;
		 	if (clip->trace.startsolid)
			{
				clip->trace = trace;
				clip->trace.startsolid = true;
			}
			else clip->trace = trace;
		}
		else if (trace.startsolid) 
		{
			clip->trace.startsolid = true;
			//clip->trace.startstuck = true;
		}
	}
}


/*
==================
SV_TraceBounds
==================
*/
void SV_TraceBounds (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
{
	int		i;
	
	for (i = 0; i < 3; i++)
	{
		if (end[i] > start[i])
		{
			boxmins[i] = start[i] + mins[i] - 1;
			boxmaxs[i] = end[i] + maxs[i] + 1;
		}
		else
		{
			boxmins[i] = end[i] + mins[i] - 1;
			boxmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
}

/*
==================
SV_Trace

Moves the given mins/maxs volume through the world from start to end.

Passedict and edicts owned by passedict are explicitly not checked.

==================
*/
trace_t SV_Trace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, prvm_edict_t *passedict, int contentmask)
{
	moveclip_t	clip;

	if (!mins) mins = vec3_origin;
	if (!maxs) maxs = vec3_origin;

	memset ( &clip, 0, sizeof ( moveclip_t ) );

	// clip to world
	clip.trace = CM_BoxTrace (start, end, mins, maxs, 0, contentmask);
	clip.trace.ent = prog->edicts;
	if (clip.trace.fraction == 0) return clip.trace; // blocked by the world

	clip.contentmask = contentmask;
	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passedict = passedict;

	VectorCopy (mins, clip.mins2);
	VectorCopy (maxs, clip.maxs2);
	
	// create the bounding box of the entire move
	SV_TraceBounds ( start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs );

	// clip to other solid entities
	SV_ClipMoveToEntities ( &clip );

	return clip.trace;
}

trace_t SV_ClipMoveToEntity(prvm_edict_t *ent, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int contentsmask)
{
	moveclip_t	clip;

	memset( &clip, 0, sizeof(moveclip_t));
	
	clip.passedict = ent;
	clip.contentmask = contentsmask;
	
	VectorCopy(start, clip.start);
	VectorCopy(end, clip.end);

	VectorCopy(mins, clip.mins);
	VectorCopy(maxs, clip.maxs);
	VectorCopy(mins, clip.mins2);
	VectorCopy(maxs, clip.maxs2);

	// create the bounding box of the entire move
	SV_TraceBounds ( clip.start, clip.mins2, clip.maxs2, clip.end, clip.boxmins, clip.boxmaxs );

	// all prepares finished			
	SV_ClipMoveToEntities( &clip );

	return clip.trace;
}