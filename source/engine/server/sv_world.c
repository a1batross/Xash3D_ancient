//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        sv_world.c - world query functions
//=======================================================================

#include "common.h"
#include "server.h"
#include "const.h"

/*
===============================================================================

ENTITY AREA CHECKING

FIXME: this use of "area" is different from the bsp file use
===============================================================================
*/
#define EDICT_FROM_AREA( l )		EDICT_NUM( l->entnum )
#define MAX_TOTAL_ENT_LEAFS		128
#define AREA_NODES			64
#define AREA_DEPTH			5

typedef struct areanode_s
{
	int		axis;		// -1 = leaf node
	float		dist;
	struct areanode_s	*children[2];
	link_t		trigger_edicts;
	link_t		solid_edicts;
} areanode_t;

typedef struct area_s
{
	const float	*mins;
	const float	*maxs;
	edict_t		**list;
	int		count;
	int		maxcount;
	int		type;
} area_t;

const char *ed_name[] =
{
	"unknown",
	"world",
	"static",
	"ambient",
	"normal",
	"brush",
	"player",
	"monster",
	"tempent",
	"beam",
	"mover",
	"viewmodel",
	"item",
	"ragdoll",
	"physbody",
	"trigger",
	"portal",
	"missile",
	"decal",
	"vehicle",
	"error",
};

areanode_t	sv_areanodes[AREA_NODES];
int		sv_numareanodes;

/*
===============
SV_ClearLink

SV_ClearLink is used for new headnodes
===============
*/
void SV_ClearLink( link_t *l )
{
	l->prev = l->next = l;
}

/*
===============
SV_RemoveLink

remove link from chain
===============
*/
void SV_RemoveLink( link_t *l )
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

/*
===============
SV_InsertLinkBefore

kept trigger and solid entities seperate
===============
*/
void SV_InsertLinkBefore( link_t *l, link_t *before, edict_t *ent )
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
	l->entnum = NUM_FOR_EDICT( ent );
}

/*
===============
SV_CreateAreaNode

builds a uniformly subdivided tree for the given world size
===============
*/
areanode_t *SV_CreateAreaNode( int depth, vec3_t mins, vec3_t maxs )
{
	areanode_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1;
	vec3_t		mins2, maxs2;

	anode = &sv_areanodes[sv_numareanodes++];

	SV_ClearLink( &anode->trigger_edicts );
	SV_ClearLink( &anode->solid_edicts );
	
	if( depth == AREA_DEPTH )
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}
	
	VectorSubtract( maxs, mins, size );
	if( size[0] > size[1] )
		anode->axis = 0;
	else anode->axis = 1;
	
	anode->dist = 0.5f * (maxs[anode->axis] + mins[anode->axis]);
	VectorCopy( mins, mins1 );	
	VectorCopy( mins, mins2 );	
	VectorCopy( maxs, maxs1 );	
	VectorCopy( maxs, maxs2 );	
	
	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;
	anode->children[0] = SV_CreateAreaNode( depth+1, mins2, maxs2 );
	anode->children[1] = SV_CreateAreaNode( depth+1, mins1, maxs1 );

	return anode;
}

/*
===============
SV_ClearWorld

===============
*/
void SV_ClearWorld( void )
{
	vec3_t	mins, maxs;

	sv_numareanodes = 0;
	Mod_GetBounds( sv.models[1], mins, maxs );
	Mem_Set( sv_areanodes, 0, sizeof( sv_areanodes ));
	SV_CreateAreaNode( 0, mins, maxs );
}

/*
================
SV_HullForEntity

Returns a headnode that can be used for testing or clipping to a
given entity.  If the entity is a bsp model, the headnode will
be returned, otherwise a custom box tree will be constructed.
================
*/
model_t SV_HullForEntity( const edict_t *ent )
{
	if( ent->v.solid == SOLID_BSP )
	{
		// explicit hulls in the BSP model
		return ent->v.modelindex;
	}
	if( ent->v.solid == SOLID_CAPSULE )
	{
		// create a temp capsule from bounding box sizes
		return pe->TempModel( ent->v.mins, ent->v.maxs, true );
	}
	// create a temp tree from bounding box sizes
	return pe->TempModel( ent->v.mins, ent->v.maxs, false );
}


/*
=================
SV_ClassifyEdict

sorting edict by type
=================
*/
void SV_ClassifyEdict( edict_t *ent )
{
	sv_priv_t		*sv_ent;

	sv_ent = ent->pvServerData;
	if( !sv_ent || sv_ent->s.ed_type != ED_SPAWNED )
		return;

	// update baseline for new entity
	if( !sv_ent->s.number )
	{
		// take current state as baseline
		SV_UpdateEntityState( ent, true );
		svs.baselines[ent->serialnumber] = ent->pvServerData->s;
	}

	sv_ent->s.ed_type = svgame.dllFuncs.pfnClassifyEdict( ent );

	if( sv_ent->s.ed_type != ED_SPAWNED )
	{
		// or leave unclassified, wait for next SV_LinkEdict...
		// Msg( "%s: <%s>\n", STRING( ent->v.classname ), ed_name[sv_ent->s.ed_type] );
	}
}

/*
===============
SV_UnlinkEdict
===============
*/
void SV_UnlinkEdict( edict_t *ent )
{
	// not linked in anywhere
	if( !ent->pvServerData->area.prev ) return;

	SV_RemoveLink( &ent->pvServerData->area );
	ent->pvServerData->area.prev = ent->pvServerData->area.next = NULL;
}

/*
===============
SV_LinkEntity
===============
*/
void SV_LinkEdict( edict_t *ent )
{
	areanode_t	*node;
	int		leafs[MAX_TOTAL_ENT_LEAFS];
	int		clusters[MAX_TOTAL_ENT_LEAFS];
	int		num_leafs;
	int		i, j;
	int		area;
	int		lastleaf;
	sv_priv_t		*sv_ent;

	sv_ent = ent->pvServerData;

	if( !sv_ent ) return;
	if( sv_ent->area.prev ) SV_UnlinkEdict( ent ); // unlink from old position
	if( ent == EDICT_NUM( 0 )) return; // don't add the world
	if( ent->free ) return;

	// trying to classify unclassified edicts
	if( sv.state == ss_active && sv_ent->s.ed_type == ED_SPAWNED )
		SV_ClassifyEdict( ent );

	// set the size
	VectorSubtract( ent->v.maxs, ent->v.mins, ent->v.size );

	// set the abs box
	svgame.dllFuncs.pfnSetAbsBox( ent );

	// link to PVS leafs
	sv_ent->num_clusters = 0;
	sv_ent->areanum = 0;
	sv_ent->areanum2 = 0;

	// get all leafs, including solids
	num_leafs = pe->BoxLeafnums( ent->v.absmin, ent->v.absmax, leafs, MAX_TOTAL_ENT_LEAFS, &lastleaf );

	// if none of the leafs were inside the map, the
	// entity is outside the world and can be considered unlinked
	if( !num_leafs ) return;

	// set areas, even from clusters that don't fit in the entity array
	for( i = 0; i < num_leafs; i++ )
	{
		clusters[i] = pe->LeafCluster( leafs[i] );
		area = pe->LeafArea( leafs[i] );
		if( area )
		{	
			// doors may legally straggle two areas,
			// but nothing should evern need more than that
			if( sv_ent->areanum && sv_ent->areanum != area )
			{
				if( sv_ent->areanum2 && sv_ent->areanum2 != area && sv.state == ss_loading )
				{
					float *v = ent->v.absmin;
					MsgDev( D_WARN, "SV_LinkEdict: object touching 3 areas at %f %f %f\n", v[0], v[1], v[2] );
				}
				sv_ent->areanum2 = area;
			}
			else sv_ent->areanum = area;
		}
	}

	sv_ent->lastcluster = -1;
	sv_ent->num_clusters = 0;

	for( i = 0; i < num_leafs; i++ )
	{
		if( clusters[i] == -1 )
			continue;		// not a visible leaf
		for( j = 0; j < i; j++ )
		{
			if( clusters[j] == clusters[i] )
				break;
		}
		if( j == i )
		{
			if( sv_ent->num_clusters == MAX_ENT_CLUSTERS )
			{	
				// we missed some leafs, so store the last visible cluster
				sv_ent->lastcluster = pe->LeafCluster( lastleaf );
				break;
			}
			sv_ent->clusternums[sv_ent->num_clusters++] = clusters[i];
		}
	}

	// if first time, make sure old_origin is valid
	if( !sv_ent->linkcount && sv_ent->s.ed_type != ED_PORTAL )
		VectorCopy( ent->v.origin, ent->v.oldorigin );

	ent->pvServerData->linkcount++;
	ent->pvServerData->s.ed_flags |= ESF_LINKEDICT;	// change edict state on a client too...

	// don't link not solid bodies
	if( ent->v.solid == SOLID_NOT )
	{
		sv_ent->linked = true;
		return;
	}

	// find the first node that the ent's box crosses
	node = sv_areanodes;
	while( 1 )
	{
		if( node->axis == -1 ) break;
		if( ent->v.absmin[node->axis] > node->dist )
			node = node->children[0];
		else if( ent->v.absmax[node->axis] < node->dist )
			node = node->children[1];
		else break; // crosses the node
	}
	
	// link it in	
	if( ent->v.solid == SOLID_TRIGGER )
		SV_InsertLinkBefore( &sv_ent->area, &node->trigger_edicts, ent );
	else SV_InsertLinkBefore (&sv_ent->area, &node->solid_edicts, ent );
	sv_ent->linked = true;
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
void SV_AreaEdicts_r( areanode_t *node, area_t *ap )
{
	link_t	*l, *next, *start;
	edict_t	*check;
	int	count = 0;

	// touch linked edicts
	if( ap->type == AREA_SOLID )
		start = &node->solid_edicts;
	else start = &node->trigger_edicts;

	for( l = start->next; l != start; l = next )
	{
		next = l->next;
		check = EDICT_FROM_AREA( l );

		if( check->v.solid == SOLID_NOT ) continue; // deactivated
		if( !BoundsIntersect( check->v.absmin, check->v.absmax, ap->mins, ap->maxs ))
			continue;	// not touching

		if( ap->count == ap->maxcount )
		{
			MsgDev( D_WARN, "SV_AreaEdicts: maxcount hit\n" );
			return;
		}

		ap->list[ap->count] = check;
		ap->count++;
	}
	
	if( node->axis == -1 ) return; // terminal node

	// recurse down both sides
	if( ap->maxs[node->axis] > node->dist ) SV_AreaEdicts_r( node->children[0], ap );
	if( ap->mins[node->axis] < node->dist ) SV_AreaEdicts_r( node->children[1], ap );
}

/*
================
SV_AreaEdicts
================
*/
int SV_AreaEdicts( const vec3_t mins, const vec3_t maxs, edict_t **list, int maxcount, int areatype )
{
	area_t	ap;

	ap.mins = mins;
	ap.maxs = maxs;
	ap.list = list;
	ap.count = 0;
	ap.maxcount = maxcount;
	ap.type = areatype;

	SV_AreaEdicts_r( sv_areanodes, &ap );

	return ap.count;
}

/*
====================
SV_ClipToEntity

====================
*/
void SV_ClipToEntity( TraceResult *trace, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, edict_t *e, int mask, trType_t type )
{
	model_t	handle;
	float	*origin, *angles;

	if( !e || e->free ) return;

	Mem_Set( trace, 0, sizeof( TraceResult ));

	// if it doesn't have any brushes of a type we
	// are looking for, ignore it
	if(!( mask & e->v.contents ))
	{
		trace->flFraction = 1.0f;
		return;
	}

	// might intersect, so do an exact clip
	handle = SV_HullForEntity( e );

	origin = e->v.origin;
	angles = e->v.angles;

	if( e->v.solid != SOLID_BSP )
		angles = vec3_origin;	// boxes don't rotate

	pe->BoxTrace2( trace, start, end, (float *)mins, (float *)maxs, handle, mask, origin, angles, type );

	if( trace->flFraction < 1.0f ) trace->pHit = e;
}


/*
====================
SV_ClipMoveToEntities

====================
*/
void SV_ClipMoveToEntities( host_clip_t *clip )
{
	int		i, num;
	edict_t		*touchlist[MAX_EDICTS];
	edict_t		*touch;
	edict_t		*passOwner;
	TraceResult	trace;
	model_t		handle;
	float		*origin, *angles;

	num = SV_AreaEdicts( clip->boxmins, clip->boxmaxs, touchlist, MAX_EDICTS, AREA_SOLID );

	if( clip->passEntity )
		passOwner = clip->passEntity->v.owner;
	else passOwner = NULL;

	for( i = 0; i < num; i++ )
	{
		if( clip->trace.fAllSolid )
			return;
		touch = touchlist[i];

		// see if we should ignore this entity
		if( clip->passEntity )
		{
			if( touchlist[i] == clip->passEntity )
			{
				continue; // don't clip against the pass entity
			}
			if( touch->v.owner == clip->passEntity )
			{
				continue;	// don't clip against own missiles
			}
			if( touch->v.owner == passOwner )
			{
				continue;	// don't clip against other missiles from our owner
			}
		}

		// if it doesn't have any brushes of a type we
		// are looking for, ignore it
		if(!( clip->mask & touch->v.contents ))
		{
			continue;
		}

		// might intersect, so do an exact clip
		handle = SV_HullForEntity( touch );

		origin = touch->v.origin;
		angles = touch->v.angles;


		if( touch->v.solid != SOLID_BSP )
			angles = vec3_origin;	// boxes don't rotate

		pe->BoxTrace2( &trace, clip->start, clip->end, (float *)clip->mins, (float *)clip->maxs, handle, clip->mask, origin, angles, clip->trType );

		if( trace.fAllSolid )
		{
			clip->trace.fAllSolid = true;
			trace.pHit = touch;
		}
		else if( trace.fStartSolid )
		{
			clip->trace.fStartSolid = true;
			trace.pHit = touch;
		}

		if( trace.flFraction < clip->trace.flFraction )
		{
			bool	oldStart;

			// make sure we keep a startsolid from a previous trace
			oldStart = clip->trace.fStartSolid;

			trace.pHit = touch;
			clip->trace = trace;
			clip->trace.fStartSolid |= oldStart;
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
TraceResult SV_Trace( const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int type, edict_t *e, int mask )
{
	host_clip_t	clip;
	int		i;

	if( !mins ) mins = vec3_origin;
	if( !maxs ) maxs = vec3_origin;

	Mem_Set( &clip, 0, sizeof( clip ));

	// clip to world
	pe->BoxTrace1( &clip.trace, start, end, mins, maxs, 0, mask, TR_AABB );
	clip.trace.pHit = (clip.trace.flFraction != 1.0f) ? EDICT_NUM( 0 ) : NULL;

	if( clip.trace.flFraction == 0.0f )
		return clip.trace; // blocked immediately by the world

	// Tr3B: HACK extension that would work with the ETPub trap_Trace(Capsule)NoEnts code
	if( type == MOVE_WORLDONLY )
	{
		// skip tracing against entities
		return clip.trace;
	}

	clip.mask = mask;
	clip.start = start;
	VectorCopy( end, clip.end );
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passEntity = e;
	clip.trType = TR_AABB;

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



/*
=============
SV_PointContents
=============
*/
int SV_PointContents( const vec3_t p )
{
	model_t		handle;
	float		*angles;
	int		i, num, contents, c2;
	edict_t		*touch[MAX_EDICTS];
	edict_t		*hit;

	// get base contents from world
	contents = pe->PointContents1( p, 0 );

	// or in contents from all the other entities
	num = SV_AreaEdicts( p, p, touch, MAX_EDICTS, AREA_SOLID );

	for( i = 0; i < num; i++ )
	{
		hit = touch[i];

		// might intersect, so do an exact clip
		handle = SV_HullForEntity( hit );
		angles = hit->v.angles;
		if( hit->v.solid != SOLID_BSP )
			angles = vec3_origin;	// boxes don't rotate
		c2 = pe->PointContents2( p, handle, hit->v.origin, hit->v.angles );
		contents |= c2;
	}
	return contents;
}