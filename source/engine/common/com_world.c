//=======================================================================
//			Copyright XashXT Group 2009 ©
//		      com_world.c - common worldtrace routines
//=======================================================================

#include "common.h"
#include "com_world.h"

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
	"physbody",
	"trigger",
	"portal",
	"skyportal",
	"error",
};

/*
===============================================================================

	ENTITY LINKING

===============================================================================
*/
/*
===============
ClearLink

ClearLink is used for new headnodes
===============
*/
void ClearLink( link_t *l )
{
	l->entnum = 0;
	l->prev = l->next = l;
}

/*
===============
RemoveLink

remove link from chain
===============
*/
void RemoveLink( link_t *l )
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

/*
===============
InsertLinkBefore

kept trigger and solid entities seperate
===============
*/
void InsertLinkBefore( link_t *l, link_t *before, int entnum )
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
	l->entnum = entnum;
}

int World_ConvertContents( int basecontents )
{
#if 0
	if( basecontents & ( BASECONT_SOLID|BASECONT_BODY ))
		return CONTENTS_SOLID;
	if( basecontents & BASECONT_SKY )
		return CONTENTS_SKY;
	if( basecontents & BASECONT_LAVA )
		return CONTENTS_LAVA;
	if( basecontents & BASECONT_SLIME )
		return CONTENTS_SLIME;
	if( basecontents & BASECONT_WATER )
		return CONTENTS_WATER;
	return CONTENTS_EMPTY;
#else
	if( basecontents & BASECONT_SKY )
		return CONTENTS_SKY;
	if( basecontents & BASECONT_LAVA )
		return CONTENTS_LAVA;
	if( basecontents & BASECONT_SLIME )
		return CONTENTS_SLIME;
	if( basecontents & BASECONT_WATER )
		return CONTENTS_WATER;
	if( basecontents & (BASECONT_SOLID|BASECONT_BODY|BASECONT_CLIP))
		return CONTENTS_SOLID;
	return CONTENTS_EMPTY;
#endif
}

uint World_MaskForEdict( const edict_t *e )
{
	if( e )
	{
		if( e->v.flags & FL_MONSTER )
		{
			if( e->v.health > 0.0f )
				return MASK_MONSTERSOLID;
			return MASK_DEADSOLID;
		}
		else if( e->v.flags & ( FL_CLIENT|FL_FAKECLIENT ))
		{
			if( e->v.health > 0.0f )
				return MASK_PLAYERSOLID;
			return MASK_DEADSOLID;
		}
		else if( e->v.solid == SOLID_TRIGGER )
		{
			return (BASECONT_SOLID|BASECONT_BODY);
		}
		return MASK_SOLID;
	}
	return MASK_SOLID;
}

uint World_ContentsForEdict( const edict_t *e )
{
	if( e )
	{
		if( e->v.flags & (FL_MONSTER|FL_CLIENT|FL_FAKECLIENT))
		{
			if( e->v.health > 0.0f )
				return BASECONT_BODY;
			return BASECONT_CORPSE;
		}
		else if( e->v.solid == SOLID_TRIGGER )
		{
			return BASECONT_TRIGGER;
		}
		else if( e->v.solid == SOLID_BSP )
		{
			if( CM_GetModelType( e->v.modelindex ) == mod_brush )
			{
				switch( e->v.skin )
				{
				case CONTENTS_WATER: return BASECONT_WATER;
				case CONTENTS_SLIME: return BASECONT_SLIME;
				case CONTENTS_LAVA: return BASECONT_LAVA;
				case CONTENTS_CLIP: return BASECONT_PLAYERCLIP;
				case CONTENTS_GRAVITY_FLYFIELD:
				case CONTENTS_FLYFIELD:
				case CONTENTS_FOG: return BASECONT_FOG;
				default: return BASECONT_SOLID;
				}
			}
			return BASECONT_SOLID; // world
		}
		else if( e->v.solid == SOLID_NOT )
		{
			return BASECONT_NONE;
		}
		return BASECONT_BODY;		
	}
	return BASECONT_NONE;
}

/*
================
World_HullForEntity

Returns a headnode that can be used for testing or clipping to a
given entity.  If the entity is a bsp model, the headnode will
be returned, otherwise a custom box tree will be constructed.
================
*/
model_t World_HullForEntity( const edict_t *ent )
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
==================
World_MoveBounds
==================
*/
void World_MoveBounds( const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end, vec3_t boxmins, vec3_t boxmaxs )
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