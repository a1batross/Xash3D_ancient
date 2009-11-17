//=======================================================================
//			Copyright XashXT Group 2009 ©
//		      com_world.c - common worldtrace routines
//=======================================================================

#include "common.h"
#include "com_world.h"

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
		else if( e->v.solid == SOLID_NOT )
		{
			return BASECONT_NONE;
		}
		return BASECONT_SOLID;		
	}
	return BASECONT_NONE;
}