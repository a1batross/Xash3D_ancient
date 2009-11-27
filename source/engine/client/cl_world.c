//=======================================================================
//			Copyright XashXT Group 2009 ©
//		        cl_world.c - world query functions
//=======================================================================

#include "common.h"
#include "client.h"
#include "const.h"
#include "pm_defs.h"

areanode_t	cl_areanodes[AREA_NODES];
int		cl_numareanodes;

/*
===============
CL_CreateAreaNode

builds a uniformly subdivided tree for the given world size
===============
*/
areanode_t *CL_CreateAreaNode( int depth, vec3_t mins, vec3_t maxs )
{
	areanode_t	*anode;
	vec3_t		size;
	vec3_t		mins1, maxs1;
	vec3_t		mins2, maxs2;

	anode = &cl_areanodes[cl_numareanodes++];

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
	anode->children[0] = CL_CreateAreaNode( depth+1, mins2, maxs2 );
	anode->children[1] = CL_CreateAreaNode( depth+1, mins1, maxs1 );

	return anode;
}

/*
===============
CL_ClearWorld

===============
*/
void CL_ClearWorld( void )
{
	vec3_t	mins, maxs;
	int	worldIndex = 1;

	cl_numareanodes = 0;
	Mod_GetBounds( worldIndex, mins, maxs );
	Mem_Set( cl_areanodes, 0, sizeof( cl_areanodes ));
	CL_CreateAreaNode( 0, mins, maxs );
}

/*
=================
CL_ClassifyEdict

sorting edict by type
=================
*/
void CL_ClassifyEdict( edict_t *ent )
{
	cl_priv_t		*cl_ent;

	cl_ent = ent->pvClientData;
	if( !cl_ent || cl_ent->current.ed_type != ED_SPAWNED )
		return;

	cl_ent->current.ed_type = ED_TEMPENTITY; // it's client entity

	// display type
	// Msg( "%s: <%s>\n", STRING( ent->v.classname ), ed_name[cl_ent->s.ed_type] );
}

/*
===============
CL_UnlinkEdict
===============
*/
void CL_UnlinkEdict( edict_t *ent )
{
	// not linked in anywhere
	if( !ent->pvClientData->area.prev )
		return;

	RemoveLink( &ent->pvClientData->area );
	ent->pvClientData->area.prev = NULL;
	ent->pvClientData->area.next = NULL;
	ent->pvClientData->linked = false;
}

void CL_SetAbsBbox( edict_t *ent )
{
	if (( ent->v.solid == SOLID_BSP ) && !VectorIsNull( ent->v.angles ))
	{	
		// expand for rotation
		float	max = 0, v;
		int	i;

		for ( i = 0; i < 3; i++ )
		{
			v = fabs( ent->v.mins[i] );
			if ( v > max ) max = v;
			v = fabs( ent->v.maxs[i] );
			if ( v > max ) max = v;
		}

		for ( i = 0; i < 3; i++ )
		{
			ent->v.absmin[i] = ent->v.origin[i] - max;
			ent->v.absmax[i] = ent->v.origin[i] + max;
		}
	}
	else
	{
		VectorAdd( ent->v.origin, ent->v.mins, ent->v.absmin );
		VectorAdd( ent->v.origin, ent->v.maxs, ent->v.absmax );
	}

	ent->v.absmin[0] -= 1;
	ent->v.absmin[1] -= 1;
	ent->v.absmin[2] -= 1;
	ent->v.absmax[0] += 1;
	ent->v.absmax[1] += 1;
	ent->v.absmax[2] += 1;
}

/*
===============
CL_LinkEntity
===============
*/
void CL_LinkEdict( edict_t *ent, bool touch_triggers )
{
	areanode_t	*node;
	cl_priv_t		*cl_ent;

	cl_ent = ent->pvClientData;

	if( !cl_ent ) return;
	if( cl_ent->area.prev ) CL_UnlinkEdict( ent ); // unlink from old position
	if( ent == EDICT_NUM( 0 )) return; // don't add the world
	if( ent->free ) return;

	// trying to classify unclassified edicts
	if( cls.state == ca_active && cl_ent->current.ed_type == ED_SPAWNED )
		CL_ClassifyEdict( ent );

	// set the abs box
	CL_SetAbsBbox( ent );

	cl_ent->linked = true;
		
	// ignore not solid bodies
	if( ent->v.solid == SOLID_NOT )
		return;

	// find the first node that the ent's box crosses
	node = cl_areanodes;

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
		InsertLinkBefore( &cl_ent->area, &node->trigger_edicts, NUM_FOR_EDICT( ent ));
	else InsertLinkBefore (&cl_ent->area, &node->solid_edicts, NUM_FOR_EDICT( ent ));

	if( touch_triggers ) Host_Error( "CL_LinkEdict: touch_tiggers\n" );
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
CL_AreaEdicts_r
====================
*/
void CL_AreaEdicts_r( areanode_t *node, area_t *ap )
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
			MsgDev( D_WARN, "CL_AreaEdicts: maxcount hit\n" );
			return;
		}

		ap->list[ap->count] = check;
		ap->count++;
	}
	
	if( node->axis == -1 ) return; // terminal node

	// recurse down both sides
	if( ap->maxs[node->axis] > node->dist ) CL_AreaEdicts_r( node->children[0], ap );
	if( ap->mins[node->axis] < node->dist ) CL_AreaEdicts_r( node->children[1], ap );
}

/*
================
CL_AreaEdicts
================
*/
int CL_AreaEdicts( const vec3_t mins, const vec3_t maxs, edict_t **list, int maxcount, int areatype )
{
	area_t	ap;

	ap.mins = mins;
	ap.maxs = maxs;
	ap.list = list;
	ap.count = 0;
	ap.maxcount = maxcount;
	ap.type = areatype;

	CL_AreaEdicts_r( cl_areanodes, &ap );

	return ap.count;
}

/*
==================
CL_ClipMoveToEntity

Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
trace_t CL_ClipMoveToEntity( edict_t *ent, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, uint umask, int flags )
{
	trace_t	trace;
	model_t	handle;
	float	*origin, *angles;

	// fill in a default trace
	Mem_Set( &trace, 0, sizeof( trace_t ));

	// if it doesn't have any brushes of a type we
	// are looking for, ignore it
	if(!( umask & World_ContentsForEdict( ent )))
	{
		trace.flFraction = 1.0f;
		trace.fInOpen = true;
		return trace;
	}

	// might intersect, so do an exact clip
	handle = World_HullForEntity( ent );

	if( ent->v.solid == SOLID_BSP )
		angles = ent->v.angles;
	else angles = vec3_origin; // boxes don't rotate
	origin = ent->v.origin;

	if( ent == clgame.edicts )
		CM_BoxTrace( &trace, start, end, mins, maxs, handle, umask, TR_AABB );
	else if( !(flags & FTRACE_SIMPLEBOX) && CM_GetModelType( ent->v.modelindex ) == mod_studio )
	{
		if( CM_HitboxTrace( &trace, ent, start, end )); // continue tracing bbox if hitbox missing
		else CM_TransformedBoxTrace( &trace, start, end, mins, maxs, handle, umask, origin, angles, TR_AABB );
	}
	else CM_TransformedBoxTrace( &trace, start, end, mins, maxs, handle, umask, origin, angles, TR_AABB );

	// did we clip the move?
	if( trace.flFraction < 1.0f || trace.fStartSolid )
		trace.pHit = ent;
	return trace;
}

static trace_t CL_CombineTraces( trace_t *cliptrace, trace_t *trace, edict_t *touch, bool is_bmodel )
{
	if( trace->fAllSolid )
	{
		cliptrace->fAllSolid = true;
		trace->pHit = touch;
	}
	else if( trace->fStartSolid )
	{
		if( is_bmodel )
			cliptrace->fStartStuck = true;
		cliptrace->fStartSolid = true;
		trace->pHit = touch;
	}

	if( trace->flFraction < cliptrace->flFraction )
	{
		bool	oldStart;

		// make sure we keep a startsolid from a previous trace
		oldStart = cliptrace->fStartSolid;

		trace->pHit = touch;
		cliptrace = trace;
		cliptrace->fStartSolid |= oldStart;
	}
	return *cliptrace;
}

/*
====================
CL_ClipToLinks

Mins and maxs enclose the entire area swept by the move
====================
*/
static void CL_ClipToLinks( areanode_t *node, moveclip_t *clip )
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

		if( clip->type == MOVE_NOMONSTERS && touch->v.solid != SOLID_BSP )
			continue;

		if( clip->type == MOVE_WORLDONLY )
		{
			// accept only real bsp models with FL_WORLDBRUSH set
			if( CM_GetModelType( touch->v.modelindex ) == mod_brush && touch->v.flags & FL_WORLDBRUSH );
			else continue;
		}

		if( !BoundsIntersect( clip->boxmins, clip->boxmaxs, touch->v.absmin, touch->v.absmax ))
			continue;

		if( clip->passedict && !VectorIsNull( clip->passedict->v.size ) && VectorIsNull( touch->v.size ))
			continue;	// points never interact

		if( clip->flags & FTRACE_IGNORE_GLASS && CM_GetModelType( touch->v.modelindex ) == mod_brush )
		{
			vec3_t	point;

			// we can ignore brushes with rendermode != kRenderNormal
			switch( touch->v.rendermode )
			{
			case kRenderTransTexture:
			case kRenderTransAlpha:
			case kRenderTransAdd:
				if( touch->v.renderamt < 200 )
					continue;
				// check for translucent contents
				if( VectorIsNull( touch->v.origin ))
					VectorAverage( touch->v.absmin, touch->v.absmax, point );
				else VectorCopy( touch->v.origin, point );
				if( CL_BaseContents( point, NULL ) & BASECONT_TRANSLUCENT )
					continue; // glass detected
			default:	break;
			}
		}

		// might intersect, so do an exact clip
		if( clip->trace.fAllSolid ) return;

		if( clip->passedict )
		{
		 	if( touch->v.owner == clip->passedict )
				continue;	// don't clip against own missiles
			if( clip->passedict->v.owner == touch )
				continue;	// don't clip against owner
		}

		if( touch->v.flags & FL_MONSTER )
			trace = CL_ClipMoveToEntity( touch, clip->start, clip->mins2, clip->maxs2, clip->end, clip->umask, clip->flags );
		else trace = CL_ClipMoveToEntity( touch, clip->start, clip->mins, clip->maxs, clip->end, clip->umask, clip->flags );
		clip->trace = CL_CombineTraces( &clip->trace, &trace, touch, touch->v.solid == SOLID_BSP );
	}
	
	// recurse down both sides
	if( node->axis == -1 ) return;

	if( clip->boxmaxs[node->axis] > node->dist )
		CL_ClipToLinks( node->children[0], clip );
	if( clip->boxmins[node->axis] < node->dist )
		CL_ClipToLinks( node->children[1], clip );
}

/*
==================
CL_Move
==================
*/
trace_t CL_Move( const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, int type, edict_t *e )
{
	moveclip_t	clip;
	int		i;

	Mem_Set( &clip, 0, sizeof( moveclip_t ));

	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.type = (type & 0xFF);
	clip.flags = (type & 0xFF00);
	clip.passedict = e;
	clip.umask = World_MaskForEdict( e );

	// clip to world
	clip.trace = CL_ClipMoveToEntity( EDICT_NUM( 0 ), start, mins, maxs, end, clip.umask, 0 );

	if( type == MOVE_MISSILE )
	{
		for( i = 0; i < 3; i++ )
		{
			clip.mins2[i] = -15;
			clip.maxs2[i] = 15;
		}
	}
	else
	{
		VectorCopy( mins, clip.mins2 );
		VectorCopy( maxs, clip.maxs2 );
	}

	// create the bounding box of the entire move
	World_MoveBounds( start, clip.mins2, clip.maxs2, end, clip.boxmins, clip.boxmaxs );

	// clip to entities
	CL_ClipToLinks( cl_areanodes, &clip );

	return clip.trace;
}

/*
==================
CL_MoveToss
==================
*/
trace_t CL_MoveToss( edict_t *tossent, edict_t *ignore )
{
	float	gravity;
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
	gravity = tossent->v.gravity * clgame.movevars.gravity * 0.05f;

	for( i = 0; i < 200; i++ )
	{
		CL_CheckVelocity( tossent );
		tossent->v.velocity[2] -= gravity;
		VectorMA( tossent->v.angles, 0.05f, tossent->v.avelocity, tossent->v.angles );
		VectorScale( tossent->v.velocity, 0.05f, move );
		VectorAdd( tossent->v.origin, move, end );
		trace = CL_Move( tossent->v.origin, tossent->v.mins, tossent->v.maxs, end, MOVE_NORMAL, tossent );
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
CL_PointContents

=============
*/
int CL_BaseContents( const vec3_t p, edict_t *e )
{
	model_t		handle;
	float		*angles;
	int		i, num, contents, c2;
	edict_t		*touch[MAX_EDICTS];
	edict_t		*hit;

	// sanity check
	if( !p ) return 0;

	// get base contents from world
	contents = CM_PointContents( p, 0 );

	// or in contents from all the other entities
	num = CL_AreaEdicts( p, p, touch, MAX_EDICTS, AREA_SOLID );

	for( i = 0; i < num; i++ )
	{
		hit = touch[i];

		if( hit == e ) continue;
		if( hit->v.flags & (FL_CLIENT|FL_FAKECLIENT|FL_MONSTER))
		{
			// never get contents from alives
			if( hit->v.health > 0.0f ) continue;
		}

		// might intersect, so do an exact clip
		handle = World_HullForEntity( hit );
		if( hit->v.solid == SOLID_BSP )
			angles = hit->v.angles;
		else angles = vec3_origin;	// boxes don't rotate

		c2 = CM_TransformedPointContents( p, handle, hit->v.origin, angles );
		c2 |= World_ContentsForEdict( hit ); // user-defined contents
		contents |= c2;
	}
	return contents;
}

int CL_PointContents( const vec3_t p )
{
	return World_ConvertContents( CL_BaseContents( p, NULL ));
}

/*
============
CL_TestPlayerPosition

============
*/
edict_t *CL_TestPlayerPosition( const vec3_t origin, edict_t *pass, TraceResult *tr )
{
	float	*mins, *maxs;
	trace_t	result;

	clgame.pmove->usehull = bound( 0, clgame.pmove->usehull, 3 );
	mins = clgame.pmove->player_mins[clgame.pmove->usehull];
	maxs = clgame.pmove->player_maxs[clgame.pmove->usehull];

	result = CL_Move( origin, mins, maxs, origin, MOVE_NORMAL, pass );
	if( tr ) Mem_Copy( tr, &result, sizeof( *tr ));

	if(( result.iContents & World_MaskForEdict( pass )) && result.pHit )
		return result.pHit;
	return NULL;
}