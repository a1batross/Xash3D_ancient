//=======================================================================
//			Copyright XashXT Group 2009 ©
//		         com_world.h - shared world trace
//=======================================================================
#ifndef COM_WORLD_H
#define COM_WORLD_H

#define MOVE_NORMAL		0	// normal trace
#define MOVE_NOMONSTERS	1	// ignore monsters (edicts with flags (FL_MONSTER|FL_FAKECLIENT|FL_CLIENT) set)
#define MOVE_WORLDONLY	2	// clip only world

/*
===============================================================================

ENTITY AREA CHECKING

===============================================================================
*/
#define EDICT_FROM_AREA( l )		EDICT_NUM( l->entnum )
#define MAX_TOTAL_ENT_LEAFS		128
#define AREA_NODES			64
#define AREA_DEPTH			5

// link_t is only used for entity area links now
typedef struct link_s
{
	struct link_s	*prev;
	struct link_s	*next;
	int		entnum;	// NUM_FOR_EDICT
} link_t;

typedef struct areanode_s
{
	int		axis;	// -1 = leaf node
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

typedef struct moveclip_s
{
	vec3_t		boxmins;	// enclose the test object along entire move
	vec3_t		boxmaxs;
	float		*mins;
	float		*maxs;	// size of the moving object
	const float	*start;
	const float	*end;
	trace_t		trace;
	edict_t		*passedict;
	trType_t		type;
} moveclip_t;

// linked list
void InsertLinkBefore( link_t *l, link_t *before, int entnum );
void RemoveLink( link_t *l );
void ClearLink( link_t *l );

// contents
int World_ConvertContents( int basecontents );

#endif//COM_WORLD_H