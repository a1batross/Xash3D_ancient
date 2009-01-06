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
#define AREA_NODES			32
#define AREA_DEPTH			4

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
	cmodel_t	*world = sv.models[1];

	sv_numareanodes = 0;
	Mem_Set( sv_areanodes, 0, sizeof( sv_areanodes ));
	SV_CreateAreaNode( 0, world->mins, world->maxs );
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
	const char	*classname;

	sv_ent = ent->pvServerData;
	if( !sv_ent || sv_ent->s.ed_type != ED_SPAWNED )
		return;

	// update baseline for new entity
	if( !sv_ent->s.number )
	{
		// take current state as baseline
		SV_UpdateEntityState( ent );
		svs.baselines[ent->serialnumber] = ent->pvServerData->s;
	}
	classname = STRING( ent->v.classname );

	if( !com.strnicmp( "worldspawn", classname, 10 ))
	{
		sv_ent->s.ed_type = ED_WORLDSPAWN;
		return;
	}
	// first pass: determine type by explicit parms
	if( ent->v.solid == SOLID_TRIGGER )
	{
		if( sv_ent->s.soundindex )
			sv_ent->s.ed_type = ED_AMBIENT;	// e.g. trigger_teleport
		else sv_ent->s.ed_type = ED_TRIGGER;		// never sending to client
	}
	else if( ent->v.movetype == MOVETYPE_PHYSIC )
		sv_ent->s.ed_type = ED_RIGIDBODY;
	else if( ent->v.solid == SOLID_BSP || VectorIsNull( ent->v.origin ))
	{
		if( ent->v.movetype == MOVETYPE_CONVEYOR )
			sv_ent->s.ed_type = ED_MOVER;
		else if( ent->v.flags & FL_WORLDBRUSH )
			sv_ent->s.ed_type = ED_BSPBRUSH;
		else if( ent->v.movetype == MOVETYPE_PUSH ) 
			sv_ent->s.ed_type = ED_MOVER;
		else if( ent->v.movetype == MOVETYPE_NONE )
			sv_ent->s.ed_type = ED_BSPBRUSH;
	}
	else if( ent->v.flags & FL_MONSTER )
		sv_ent->s.ed_type = ED_MONSTER;
	else if( ent->v.flags & FL_CLIENT )
		sv_ent->s.ed_type = ED_CLIENT;
	else if( !sv_ent->s.modelindex && !sv_ent->s.aiment )
	{	
		if( sv_ent->s.soundindex )
			sv_ent->s.ed_type = ED_AMBIENT;
		else sv_ent->s.ed_type = ED_STATIC; // never sending to client
	}

	if( sv_ent->s.ed_type == ED_SPAWNED )
	{
		// mark as normal
		if( sv_ent->s.modelindex || sv_ent->s.soundindex )
			sv_ent->s.ed_type = ED_NORMAL;
	}
	
	// or leave unclassified, wait for next SV_LinkEdict...
	// Msg( "%s: <%s>\n", STRING( ent->v.classname ), ed_name[sv_ent->s.ed_type] );
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
	int		topnode;
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
	num_leafs = pe->BoxLeafnums( ent->v.absmin, ent->v.absmax, leafs, MAX_TOTAL_ENT_LEAFS, &topnode );

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

	if( num_leafs >= MAX_TOTAL_ENT_LEAFS )
	{
		// assume we missed some leafs, and mark by headnode
		sv_ent->num_clusters = -1;
		sv_ent->headnode = topnode;
	}
	else
	{
		sv_ent->num_clusters = 0;
		for( i = 0; i < num_leafs; i++ )
		{
			if( clusters[i] == -1 ) continue; // not a visible leaf
			for( j = 0; j < i; j++ )
			{
				if( clusters[j] == clusters[i] )
					break;
			}
			if( j == i )
			{
				if( sv_ent->num_clusters == MAX_ENT_CLUSTERS )
				{
					// assume we missed some leafs, and mark by headnode
					sv_ent->num_clusters = -1;
					sv_ent->headnode = topnode;
					break;
				}
				sv_ent->clusternums[sv_ent->num_clusters++] = clusters[i];
			}
		}
	}

	ent->pvServerData->linkcount++;

	// update ambient sound here
	if( ent->v.ambient )
	{
		ent->pvServerData->s.soundindex = SV_SoundIndex( STRING( ent->v.ambient ));
	}

	// don't link not solid or rigid bodies
	if( ent->v.solid == SOLID_NOT || ent->v.solid >= SOLID_BOX )
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
		if( check->v.absmin[0] > ap->maxs[0] || check->v.absmin[1] > ap->maxs[1]
		 || check->v.absmin[2] > ap->maxs[2] || check->v.absmax[0] < ap->mins[0]
		 || check->v.absmax[1] < ap->mins[1] || check->v.absmax[2] < ap->mins[2] )
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