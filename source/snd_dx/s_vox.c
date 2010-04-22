//=======================================================================
//			Copyright XashXT Group 2010 ©
//			   s_vox.c - npc sentences
//=======================================================================

#include "sound.h"
#include "const.h"

void VOX_SetChanVol( channel_t *ch )
{
	float scale = 1.0f; // FIXME: get volume from words
	if( scale == 1.0f ) return;

	ch->rightvol = (int)( ch->rightvol * scale );
	ch->leftvol = (int)( ch->leftvol * scale );
}


// link all sounds in sentence, start playing first word.
void VOX_LoadSound( channel_t *pchan, const char *pszin )
{
}