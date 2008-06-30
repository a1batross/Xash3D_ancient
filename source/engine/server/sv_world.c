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

#include "common.h"
#include "server.h"

/*
===============================================================================

ENTITY CHECKING

To avoid linearly searching through lists of entities during environment testing,
the world is carved up with an evenly spaced, axially aligned bsp tree.  Entities
are kept in chains either at the final leafs, or at the first node that splits
them, which prevents having to deal with multiple fragments of a single entity.
===============================================================================
*/
#define	AREA_DEPTH	4
#define	AREA_NODES	64
#define	MAX_TOTAL_ENT_LEAFS	128

typedef struct area_s
{
	const float	*mins;
	const float	*maxs;
	edict_t		**list;
	int		count;
	int		maxcount;
	int		type;
} area_t;

worldsector_t	sv_worldsectors[AREA_NODES];
int		sv_numworldsectors;

/*
===============
SV_SectorList_f
===============
*/
void SV_SectorList_f( void )
{
	int		i, c;
	worldsector_t	*sec;
	sv_edict_t	*ent;

	for ( i = 0; i < sv_numworldsectors; i++ )
	{
		sec = &sv_worldsectors[i], c = 0;
		for( ent = sec->entities; ent; ent = ent->nextedict ) c++;
		if(c) Msg( "sector %i: %i entities\n", i, c );
	}
}

/*
===============
SV_CreateworldSector

Builds a uniformly subdivided tree for the given world size
===============
*/
worldsector_t *SV_CreateWorldSector( int depth, vec3_t mins, vec3_t maxs )
{
	worldsector_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1, mins2, maxs2;

	anode = &sv_worldsectors[sv_numworldsectors];
	sv_numworldsectors++;

	if( depth == AREA_DEPTH )
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}
	
	VectorSubtract( maxs, mins, size );
	if( size[0] > size[1] ) anode->axis = 0;
	else anode->axis = 1;

	anode->dist = 0.5f * (maxs[anode->axis] + mins[anode->axis]);
	VectorCopy(mins, mins1);	
	VectorCopy(mins, mins2);	
	VectorCopy(maxs, maxs1);	
	VectorCopy(maxs, maxs2);	
	
	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;
	anode->children[0] = SV_CreateWorldSector( depth + 1, mins2, maxs2);
	anode->children[1] = SV_CreateWorldSector( depth + 1, mins1, maxs1);

	return anode;
}

/*
===============
SV_ClearWorld

===============
*/
void SV_ClearWorld (void)
{
	cmodel_t	*world = sv.models[1];
	memset( sv_worldsectors, 0, sizeof(sv_worldsectors));
	sv_numworldsectors = 0;
	SV_CreateWorldSector( 0, world->mins, world->maxs );
}

/*
===============
SV_UnlinkEdict
===============
*/
void SV_UnlinkEdict( edict_t *ent )
{
	sv_edict_t	*sv_ent, *scan;
	worldsector_t	*ws;

	sv_ent = ent->priv.sv;
	ws = sv_ent->worldsector;
	if( !ws ) return;		// not linked in anywhere
	sv_ent->worldsector = NULL;

	if( ws->entities == sv_ent )
	{
		ws->entities = sv_ent->nextedict;
		return;
	}

	for( scan = ws->entities; scan; scan = scan->nextedict )
	{
		if( scan->nextedict == sv_ent )
		{
			scan->nextedict = sv_ent->nextedict;
			return;
		}
	}
	MsgDev( D_ERROR, "SV_UnlinkEdict: not found in worldSector\n" );
}

/*
===============
SV_LinkEntity
===============
*/
void SV_LinkEdict( edict_t *ent )
{
	worldsector_t	*node;
	int		leafs[MAX_TOTAL_ENT_LEAFS];
	int		cluster;
	int		num_leafs;
	int		i, j, k;
	int		area;
	int		lastleaf;
	sv_edict_t	*sv_ent;

	sv_ent = ent->priv.sv;

	if( sv_ent->worldsector ) SV_UnlinkEdict( ent ); // unlink from old position
	if (ent == prog->edicts) return; // don't add the world
	if (ent->priv.sv->free) return;

	// set the size
	VectorSubtract( ent->progs.sv->maxs, ent->progs.sv->mins, ent->progs.sv->size );

	// encode the size into the entity_state for client prediction
	if (ent->progs.sv->solid == SOLID_BBOX && !((int)ent->progs.sv->flags & FL_DEADMONSTER))
	{
		// assume that x/y are equal and symetric
		i = ent->progs.sv->maxs[0];
		i = bound( 1, i, 255 );

		// z is not symetric
		j = (-ent->progs.sv->mins[2]);
		j = bound( 1, j, 255 );

		// and z maxs can be negative...
		k = (ent->progs.sv->maxs[2] + 32);
		k = bound( 1, k, 255 );
		sv_ent->solid = (k<<16) | (j<<8) | i;
	}
	else if (ent->progs.sv->solid == SOLID_BSP)
	{
		sv_ent->solid = SOLID_BMODEL;	// a solid_bbox will never create this value
	}
	else sv_ent->solid = 0;

	// set the abs box
	if (ent->progs.sv->solid == SOLID_BSP && !VectorIsNull(ent->progs.sv->angles))
	{
		// expand for rotation
		int		i;
		float max = RadiusFromBounds( ent->progs.sv->mins, ent->progs.sv->maxs );

		for (i = 0; i < 3; i++)
		{
			ent->progs.sv->absmin[i] = ent->progs.sv->origin[i] - max;
			ent->progs.sv->absmax[i] = ent->progs.sv->origin[i] + max;
		}
	}
	else
	{	// normal
		VectorAdd (ent->progs.sv->origin, ent->progs.sv->mins, ent->progs.sv->absmin);	
		VectorAdd (ent->progs.sv->origin, ent->progs.sv->maxs, ent->progs.sv->absmax);
	}

	// because movement is clipped an epsilon away from an actual edge,
	// we must fully check even when bounding boxes don't quite touch
	ent->progs.sv->absmin[0] -= 1;
	ent->progs.sv->absmin[1] -= 1;
	ent->progs.sv->absmin[2] -= 1;
	ent->progs.sv->absmax[0] += 1;
	ent->progs.sv->absmax[1] += 1;
	ent->progs.sv->absmax[2] += 1;

	// link to PVS leafs
	sv_ent->num_clusters = 0;
	sv_ent->lastcluster = 0;
	sv_ent->areanum = 0;
	sv_ent->areanum2 = 0;

	// get all leafs, including solids
	num_leafs = pe->BoxLeafnums( ent->progs.sv->absmin, ent->progs.sv->absmax, leafs, MAX_TOTAL_ENT_LEAFS, &lastleaf );

	// if none of the leafs were inside the map, the
	// entity is outside the world and can be considered unlinked
	if( !num_leafs ) return;

	// set areas, even from clusters that don't fit in the entity array
	for( i = 0; i < num_leafs; i++ )
	{
		area = pe->LeafArea( leafs[i] );
		if( area )
		{	
			// doors may legally straggle two areas,
			// but nothing should evern need more than that
			if (ent->priv.sv->areanum && ent->priv.sv->areanum != area)
			{
				if (ent->priv.sv->areanum2 && ent->priv.sv->areanum2 != area && sv.state == ss_loading )
				{
					float *v = ent->progs.sv->absmin;
					MsgDev( D_WARN, "SV_LinkEdict: object touching 3 areas at %f %f %f\n", v[0], v[1], v[2]);
				}
				ent->priv.sv->areanum2 = area;
			}
			else ent->priv.sv->areanum = area;
		}
	}

	// store as many explicit clusters as we can
	sv_ent->num_clusters = 0;
	for (i = 0; i < num_leafs; i++)
	{
		cluster = pe->LeafCluster( leafs[i] );
		if( cluster )
		{
			sv_ent->clusternums[sv_ent->num_clusters++] = cluster;
			if( sv_ent->num_clusters == MAX_ENT_CLUSTERS )
				break; // list is full
		}
	}

	// store off a last cluster if we need to
	if( i != num_leafs ) sv_ent->lastcluster = pe->LeafCluster( lastleaf );

	// if first time, make sure old_origin is valid
	if(!ent->priv.sv->linkcount)
	{
		VectorCopy (ent->progs.sv->origin, ent->progs.sv->old_origin);
	}
	ent->priv.sv->linkcount++;

	// don't link not solid or rigid bodies
	if (ent->progs.sv->solid == SOLID_NOT || ent->progs.sv->solid >= SOLID_BOX)
		return;

	// find the first world sector node that the ent's box crosses
	node = sv_worldsectors;
	while( 1 )
	{
		if (node->axis == -1) break;
		if (ent->progs.sv->absmin[node->axis] > node->dist)
			node = node->children[0];
		else if (ent->progs.sv->absmax[node->axis] < node->dist)
			node = node->children[1];
		else break; // crosses the node
	}
	
	// link it in
	sv_ent->worldsector = node;
	sv_ent->nextedict = node->entities;
	node->entities = sv_ent;
}

/*
============================================================================

AREA QUERY

Fills in a list of all entities who's absmin / absmax intersects the given
bounds.  This does NOT mean that they actually touch in the case of bmodels.
============================================================================
*/
/*
====================
SV_AreaEdicts_r
====================
*/
void SV_AreaEdicts_r( worldsector_t *node, area_t *ap )
{
	sv_edict_t	*check, *next;
	edict_t		*gcheck;
	int		count = 0;

	for( check = node->entities; check; check = next )
	{
		next = check->nextedict;
		gcheck = PRVM_EDICT_NUM( check->serialnumber );

		if (gcheck->progs.sv->absmin[0] > ap->maxs[0] || gcheck->progs.sv->absmin[1] > ap->maxs[1] || gcheck->progs.sv->absmin[2] > ap->maxs[2]
		|| gcheck->progs.sv->absmax[0] < ap->mins[0] || gcheck->progs.sv->absmax[1] < ap->mins[1] || gcheck->progs.sv->absmax[2] < ap->mins[2])
			continue;	// not touching

		if( ap->count == ap->maxcount )
		{
			MsgDev(D_NOTE, "SV_AreaEdicts_r: maxcount!\n");
			return;
		}
		ap->list[ap->count] = PRVM_EDICT_NUM( check->serialnumber );
		ap->count++;
	}
	
	if( node->axis == -1 ) return; // terminal node

	// recurse down both sides
	if( ap->maxs[node->axis] > node->dist ) SV_AreaEdicts_r ( node->children[0], ap );
	if( ap->mins[node->axis] < node->dist ) SV_AreaEdicts_r ( node->children[1], ap );
}

/*
================
SV_AreaEdicts
================
*/
int SV_AreaEdicts( const vec3_t mins, const vec3_t maxs, edict_t **list, int maxcount )
{
	area_t	ap;

	ap.mins = mins;
	ap.maxs = maxs;
	ap.list = list;
	ap.count = 0;
	ap.maxcount = maxcount;

	SV_AreaEdicts_r( sv_worldsectors, &ap );

	return ap.count;
}

//===========================================================================
typedef struct
{
	vec3_t		boxmins, boxmaxs;// enclose the test object along entire move
	const float	*mins, *maxs;
	const float	*start;
	vec3_t		end;
	trace_t		trace;
	edict_t		*passedict;
	int		contentmask;
} moveclip_t;

/*
====================
SV_ClipMoveToEntities
====================
*/
void SV_ClipMoveToEntities( moveclip_t *clip )
{
	int		i, num;
	edict_t		*touchlist[MAX_EDICTS], *touch;
	trace_t		trace;
	cmodel_t		*cmodel;
	float		*origin, *angles;

	num = SV_AreaEdicts( clip->boxmins, clip->boxmaxs, touchlist, MAX_EDICTS );

	for( i = 0; i < num; i++ )
	{
		if( clip->trace.allsolid ) return;

		touch = touchlist[i];
		if( touch == clip->passedict ) continue;
		if( touch->progs.sv->solid == SOLID_NOT ) continue;
		if( clip->passedict )
		{
		 	if (PRVM_PROG_TO_EDICT(touch->progs.sv->owner) == clip->passedict)
		 		continue; // don't clip against own missiles
			if (PRVM_PROG_TO_EDICT(clip->passedict->progs.sv->owner) == touch)
				continue; // don't clip against owner
		}

		if(!(clip->contentmask & CONTENTS_DEADMONSTER) && ((int)touch->progs.sv->flags & FL_DEADMONSTER))
			continue;

		cmodel = SV_GetModelPtr( touch );

		origin = touch->progs.sv->origin;
		angles = touch->progs.sv->angles;

		if( !touch->progs.sv->solid == SOLID_BSP ) angles = vec3_origin; // boxes don't rotate
		trace = pe->TransformedBoxTrace((float *)clip->start, (float *)clip->end, (float *)clip->mins, (float *)clip->maxs, cmodel, clip->contentmask, origin, angles, false );

		if( trace.allsolid )
		{
			clip->trace.allsolid = true;
			trace.ent = touch;
		} 
		else if( trace.startsolid )
		{
			clip->trace.startsolid = true;
			trace.ent = touch;
		}
		if( trace.fraction < clip->trace.fraction )
		{
			bool	oldStart;

			// make sure we keep a startsolid from a previous trace
			oldStart = clip->trace.startsolid;
			trace.ent = touch;
			clip->trace = trace;
			clip->trace.startsolid |= oldStart;
		}
	}
}

/*
==================
SV_Trace

Moves the given mins/maxs volume through the world from start to end.
passEntityNum and entities owned by passEntityNum are explicitly not checked.
==================
*/
trace_t SV_Trace( const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, edict_t *passedict, int contentmask )
{
	moveclip_t	clip;
	int		i;

	if( !mins ) mins = vec3_origin;
	if( !maxs ) maxs = vec3_origin;
	memset( &clip, 0, sizeof( moveclip_t ));

	// clip to world
	clip.trace = pe->BoxTrace( start, end, mins, maxs, NULL, contentmask, true );
	clip.trace.ent = clip.trace.fraction != 1.0 ? prog->edicts : NULL;
	if( clip.trace.fraction == 0 ) return clip.trace;	// blocked immediately by the world

	clip.contentmask = contentmask;
	clip.start = start;
	VectorCopy( end, clip.end );
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passedict = passedict;

	// create the bounding box of the entire move
	// we can limit it to the part of the move not
	// already clipped off by the world, which can be
	// a significant savings for line of sight and shot traces
	for( i = 0; i < 3; i++ )
	{
		if( end[i] > start[i] )
		{
			clip.boxmins[i] = clip.start[i] + clip.mins[i] - 1;
			clip.boxmaxs[i] = clip.end[i] + clip.maxs[i] + 1;
		}
		else
		{
			clip.boxmins[i] = clip.end[i] + clip.mins[i] - 1;
			clip.boxmaxs[i] = clip.start[i] + clip.maxs[i] + 1;
		}
	}

	// clip to other solid entities
	SV_ClipMoveToEntities( &clip );

	return clip.trace;
}

trace_t SV_TraceToss (edict_t *tossent, edict_t *ignore)
{
	int	i;
	float	gravity = 1.0;
	vec3_t	move, end;
	vec3_t	original_origin;
	vec3_t	original_velocity;
	vec3_t	original_angles;
	vec3_t	original_avelocity;
	trace_t	trace;

	VectorCopy(tossent->progs.sv->origin, original_origin);
	VectorCopy(tossent->progs.sv->velocity, original_velocity);
	VectorCopy(tossent->progs.sv->angles, original_angles);
	VectorCopy(tossent->progs.sv->avelocity, original_avelocity);

	gravity *= sv_gravity->value * 0.05;

	for (i = 0; i < 200; i++) // LordHavoc: sanity check; never trace more than 10 seconds
	{
		SV_CheckVelocity (tossent);
		tossent->progs.sv->velocity[2] -= gravity;
		VectorMA (tossent->progs.sv->angles, 0.05, tossent->progs.sv->avelocity, tossent->progs.sv->angles);
		VectorScale (tossent->progs.sv->velocity, 0.05, move);
		VectorAdd (tossent->progs.sv->origin, move, end);
		trace = SV_Trace(tossent->progs.sv->origin, tossent->progs.sv->mins, tossent->progs.sv->maxs, end, tossent, MASK_SOLID );
		VectorCopy (trace.endpos, tossent->progs.sv->origin);
		if (trace.fraction < 1) break;
	}

	VectorCopy(original_origin, tossent->progs.sv->origin);
	VectorCopy(original_velocity, tossent->progs.sv->velocity);
	VectorCopy(original_angles, tossent->progs.sv->angles);
	VectorCopy(original_avelocity, tossent->progs.sv->avelocity);

	return trace;
}

trace_t SV_ClipMoveToEntity(edict_t *ent, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int contentsmask)
{
	edict_t		*touch;
	cmodel_t		*cmodel;
	float		*origin, *angles;
	trace_t		trace;

	touch = ent;

	memset( &trace, 0, sizeof(trace_t));
	// if it doesn't have any brushes of a type we
	// are looking for, ignore it
	if(!(contentsmask & CONTENTS_DEADMONSTER) && ((int)touch->progs.sv->flags & FL_DEADMONSTER))
	{
		trace.fraction = 1.0;
		return trace;
	}
	// might intersect, so do an exact clip
	cmodel = SV_GetModelPtr( touch );
	origin = touch->progs.sv->origin;
	angles = touch->progs.sv->angles;
          if( !touch->progs.sv->solid == SOLID_BSP ) angles = vec3_origin; // boxes don't rotate

	trace = pe->TransformedBoxTrace( start, end, mins, maxs, cmodel, contentsmask, origin, angles, true );
	if( trace.fraction < 1.0f ) trace.ent = touch;

	return trace;
}

/*
=============
SV_PointContents
=============
*/
int SV_PointContents( const vec3_t p, edict_t *passedict )
{
	edict_t		*touch[MAX_EDICTS], *hit;
	int		i, num;
	int		contents, c2;
	cmodel_t		*cmodel;
	float		*angles;

	// get base contents from world
	contents = pe->PointContents( p, NULL );

	// or in contents from all the other entities
	num = SV_AreaEdicts( p, p, touch, MAX_EDICTS );

	for( i = 0; i < num; i++ )
	{
		if ( touch[i] == passedict ) continue;
		hit = touch[i];

		// might intersect, so do an exact clip
		cmodel = SV_GetModelPtr( hit );
		angles = hit->progs.sv->angles;
          	if( !hit->progs.sv->solid == SOLID_BSP ) angles = vec3_origin; // boxes don't rotate
		c2 = pe->TransformedPointContents( p, cmodel, hit->progs.sv->origin, angles );
		contents |= c2;
	}
	return contents;
}