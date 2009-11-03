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
	if( basecontents & (BASECONT_SOLID|BASECONT_BODY|BASECONT_CLIP))
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
}