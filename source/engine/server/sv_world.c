//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        sv_world.c - world query functions
//=======================================================================

#include "common.h"
#include "server.h"
#include "const.h"
#include "pm_defs.h"

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

	ClearLink( &anode->trigger_edicts );
	ClearLink( &anode->solid_edicts );
	
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
	int	worldIndex = 1;

	sv_numareanodes = 0;
	Mod_GetBounds( worldIndex, mins, maxs );
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
	if( ent->v.flags & (FL_MONSTER|FL_CLIENT|FL_FAKECLIENT))
	{
		// create a temp capsule from bounding box sizes
		return CM_TempModel( ent->v.mins, ent->v.maxs, true );
	}
	// create a temp tree from bounding box sizes
	return CM_TempModel( ent->v.mins, ent->v.maxs, false );
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
====================
SV_TouchLinks
====================
*/
void SV_TouchLinks( edict_t *ent, areanode_t *node )
{
	link_t	*l, *next;
	edict_t	*touch;

	// touch linked edicts
	for( l = node->trigger_edicts.next; l != &node->trigger_edicts; l = next )
	{
		next = l->next;
		touch = EDICT_FROM_AREA( l );
		if( touch == ent ) continue;
		if( touch->free || touch->v.solid != SOLID_TRIGGER )
			continue;
		if( !BoundsIntersect( ent->v.absmin, ent->v.absmax, touch->v.absmin, touch->v.absmax ))
			continue;

		svgame.globals->time = sv.time * 0.001f;
		svgame.dllFuncs.pfnTouch( touch, ent );

		if( ent->free ) break; // killtarget issues
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;
	
	if( ent->v.absmax[node->axis] > node->dist )
		SV_TouchLinks( ent, node->children[0] );
	if( ent->v.absmin[node->axis] < node->dist )
		SV_TouchLinks( ent, node->children[1] );
}

/*
===============
SV_UnlinkEdict
===============
*/
void SV_UnlinkEdict( edict_t *ent )
{
	// not linked in anywhere
	if( !ent->pvServerData->area.prev )
		return;

	RemoveLink( &ent->pvServerData->area );
	ent->pvServerData->area.prev = NULL;
	ent->pvServerData->area.next = NULL;
}

/*
===============
SV_LinkEntity
===============
*/
void SV_LinkEdict( edict_t *ent, bool touch_triggers )
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

	// set the abs box
	svgame.dllFuncs.pfnSetAbsBox( ent );

	// link to PVS leafs
	sv_ent->num_clusters = 0;
	sv_ent->areanum = 0;
	sv_ent->areanum2 = 0;

	// get all leafs, including solids
	num_leafs = CM_BoxLeafnums( ent->v.absmin, ent->v.absmax, leafs, MAX_TOTAL_ENT_LEAFS, &lastleaf );

	// if none of the leafs were inside the map, the
	// entity is outside the world and can be considered unlinked
	if( !num_leafs ) return;

	// set areas, even from clusters that don't fit in the entity array
	for( i = 0; i < num_leafs; i++ )
	{
		clusters[i] = CM_LeafCluster( leafs[i] );
		area = CM_LeafArea( leafs[i] );

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
				sv_ent->lastcluster = CM_LeafCluster( lastleaf );
				break;
			}
			sv_ent->clusternums[sv_ent->num_clusters++] = clusters[i];
		}
	}

	ent->pvServerData->linkcount++;
	ent->pvServerData->s.ed_flags |= ESF_LINKEDICT;	// change edict state on a client too...
	sv_ent->linked = true;
		
	// ignore not solid bodies
	if( ent->v.solid == SOLID_NOT )
		return;

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
		InsertLinkBefore( &sv_ent->area, &node->trigger_edicts, NUM_FOR_EDICT( ent ));
	else InsertLinkBefore (&sv_ent->area, &node->solid_edicts, NUM_FOR_EDICT( ent ));

	if( touch_triggers ) SV_TouchLinks( ent, sv_areanodes );
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
==================
SV_ClipMoveToEntity

Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
trace_t SV_ClipMoveToEntity( edict_t *ent, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end )
{
	trace_t	trace;
	model_t	handle;
	float	*origin, *angles;
	int	umask;

	// fill in a default trace
	Mem_Set( &trace, 0, sizeof( trace_t ));

	// might intersect, so do an exact clip
	handle = SV_HullForEntity( ent );

	if( ent->v.solid == SOLID_BSP )
		angles = ent->v.angles;
	else angles = vec3_origin; // boxes don't rotate
	origin = ent->v.origin;

	if( ent->v.flags & (FL_CLIENT|FL_FAKECLIENT))
		umask = MASK_PLAYERSOLID;
	else if( ent->v.flags & FL_MONSTER )
		umask = MASK_MONSTERSOLID;
	else umask = MASK_SOLID;

	if( ent == svgame.edicts )
		CM_BoxTrace( &trace, start, end, mins, maxs, handle, umask, TR_AABB );
	else CM_TransformedBoxTrace( &trace, start, end, mins, maxs, handle, umask, origin, angles, TR_AABB );

	// did we clip the move?
	if( trace.flFraction < 1.0f || trace.fStartSolid )
		trace.pHit = ent;

	return trace;
}

/*
====================
SV_ClipToLinks

Mins and maxs enclose the entire area swept by the move
====================
*/
void SV_ClipToLinks( areanode_t *node, moveclip_t *clip )
{
	link_t	*l, *next;
	edict_t	*touch;
	trace_t	trace;

	// touch linked edicts
	for( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;

		touch = EDICT_FROM_AREA( l );

		if( touch->v.solid == SOLID_NOT )
			continue;
		if( touch == clip->passedict )
			continue;
		if( touch->v.solid == SOLID_TRIGGER )
			Host_Error( "trigger in clipping list\n" );

		if( clip->type == MOVE_NOMONSTERS && touch->v.flags & FL_MONSTER )
			continue;

		if( !BoundsIntersect( clip->boxmins, clip->boxmaxs, touch->v.absmin, touch->v.absmax ))
			continue;

		if( clip->passedict && !VectorIsNull( clip->passedict->v.size ) && VectorIsNull( touch->v.size ))
			continue;	// points never interact

		// might intersect, so do an exact clip
		if( clip->trace.fAllSolid ) return;

		if( clip->passedict )
		{
		 	if( touch->v.owner == clip->passedict )
				continue;	// don't clip against own missiles
			if( clip->passedict->v.owner == touch )
				continue;	// don't clip against owner
		}

		trace = SV_ClipMoveToEntity( touch, clip->start, clip->mins, clip->maxs, clip->end );

		if( trace.fAllSolid || trace.fStartSolid || trace.flFraction < clip->trace.flFraction )
		{
			trace.pHit = touch;
		 	if( clip->trace.fStartSolid )
			{
				clip->trace = trace;
				clip->trace.fStartSolid = true;
			}
			else clip->trace = trace;
		}
		else if( trace.fStartSolid )
			clip->trace.fStartSolid = true;
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;

	if( clip->boxmaxs[node->axis] > node->dist )
		SV_ClipToLinks( node->children[0], clip );
	if( clip->boxmins[node->axis] < node->dist )
		SV_ClipToLinks( node->children[1], clip );
}

/*
==================
SV_MoveBounds
==================
*/
void SV_MoveBounds( const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, vec3_t boxmins, vec3_t boxmaxs )
{
	int	i;
	
	for( i = 0; i < 3; i++ )
	{
		if( end[i] > start[i] )
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
SV_Move
==================
*/
trace_t SV_Move( const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int type, edict_t *e )
{
	moveclip_t	clip;

	Mem_Set( &clip, 0, sizeof( moveclip_t ));

	// clip to world
	clip.trace = SV_ClipMoveToEntity( EDICT_NUM( 0 ), start, mins, maxs, end );

	// skip tracing against entities
	if( type == MOVE_WORLDONLY )
		return clip.trace;

	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.type = type;
	clip.passedict = e;

	// create the bounding box of the entire move
	SV_MoveBounds( start, clip.mins, clip.maxs, end, clip.boxmins, clip.boxmaxs );

	// clip to entities
	SV_ClipToLinks( sv_areanodes, &clip );

	return clip.trace;
}

/*
==================
SV_MoveToss
==================
*/
trace_t SV_MoveToss( edict_t *tossent, edict_t *ignore )
{
	float 	gravity;
	vec3_t	move, end;
	vec3_t	original_origin;
	vec3_t	original_velocity;
	vec3_t	original_angles;
	vec3_t	original_avelocity;
	trace_t	trace;
	int	i;

	VectorCopy( tossent->v.origin, original_origin );
	VectorCopy( tossent->v.velocity, original_velocity );
	VectorCopy( tossent->v.angles, original_angles );
	VectorCopy( tossent->v.avelocity, original_avelocity );
	gravity = tossent->v.gravity * sv_gravity->value * 0.05f;

	for( i = 0; i < 200; i++ )
	{
		SV_CheckVelocity( tossent );
		tossent->v.velocity[2] -= gravity;
		VectorMA( tossent->v.angles, 0.05f, tossent->v.avelocity, tossent->v.angles );
		VectorScale( tossent->v.velocity, 0.05f, move );
		VectorAdd( tossent->v.origin, move, end );
		trace = SV_Move( tossent->v.origin, tossent->v.mins, tossent->v.maxs, end, MOVE_NORMAL, tossent );
		VectorCopy( trace.vecEndPos, tossent->v.origin );
		if( trace.flFraction < 1.0f ) break;
	}

	VectorCopy( original_origin, tossent->v.origin );
	VectorCopy( original_velocity, tossent->v.velocity );
	VectorCopy( original_angles, tossent->v.angles );
	VectorCopy( original_avelocity, tossent->v.avelocity );

	return trace;
}

/*
=============
SV_PointContents

FIXME: get contents from pev->skin
=============
*/
int SV_BaseContents( const vec3_t p )
{
	model_t		handle;
	float		*angles;
	int		i, num, contents, c2;
	edict_t		*touch[MAX_EDICTS];
	edict_t		*hit;

	// get base contents from world
	contents = CM_PointContents( p, 0 );

	// or in contents from all the other entities
	num = SV_AreaEdicts( p, p, touch, MAX_EDICTS, AREA_SOLID );

	for( i = 0; i < num; i++ )
	{
		hit = touch[i];

		// might intersect, so do an exact clip
		handle = SV_HullForEntity( hit );
		if( hit->v.solid == SOLID_BSP )
			angles = hit->v.angles;
		else angles = vec3_origin;	// boxes don't rotate

		c2 = CM_TransformedPointContents( p, handle, hit->v.origin, hit->v.angles );
		contents |= c2;
	}
	return contents;
}

int SV_PointContents( const vec3_t p )
{
	return World_ConvertContents( SV_BaseContents( p ));
}

/*
============
SV_TestPlayerPosition

============
*/
edict_t *SV_TestPlayerPosition( const vec3_t origin )
{
	edict_t	*check;
	model_t	handle;
	vec3_t	boxmins, boxmaxs;
	float	*angles;
	int	e, cont;
	
	// check world first
	if( World_ConvertContents( CM_PointContents( origin, 0 )) != CONTENTS_EMPTY )
		return EDICT_NUM( 0 );

	// check all entities
	VectorAdd( origin, svgame.pmove->player_mins[svgame.pmove->usehull], boxmins );
	VectorAdd( origin, svgame.pmove->player_maxs[svgame.pmove->usehull], boxmaxs );
	
	for( e = 1; e < svgame.globals->numEntities; e++ )
	{
		check = EDICT_NUM( e );

		if( check->free ) continue;
		if( check->v.solid != SOLID_BSP && check->v.solid != SOLID_BBOX && check->v.solid != SOLID_SLIDEBOX )
			continue;

		if( !BoundsIntersect( boxmins, boxmaxs, check->v.absmin, check->v.absmax ))
			continue;

		if( check == svgame.pmove->pev->pContainingEntity )
			continue;

		// get the clipping hull
		handle = SV_HullForEntity( check );
	
		if( check->v.solid == SOLID_BSP )
			angles = check->v.angles;
		else angles = vec3_origin;	// boxes don't rotate

		cont = CM_TransformedPointContents( origin, handle, check->v.origin, angles );
		cont = World_ConvertContents( cont );
	
		// test the point
		if( cont != CONTENTS_EMPTY )
			return check;
	}
	return NULL;
}