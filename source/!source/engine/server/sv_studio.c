//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_studio.c - cm inline studio
//=======================================================================

#include "engine.h"
#include "server.h"

int SV_StudioExtractBbox( studiohdr_t *phdr, int sequence, float *mins, float *maxs )
{
	mstudioseqdesc_t	*pseqdesc;
	pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);

	if(sequence == -1) return 0;
	
	VectorCopy( pseqdesc[ sequence ].bbmin, mins );
	VectorCopy( pseqdesc[ sequence ].bbmax, maxs );

	return 1;
}

byte *SV_GetModelPtr(edict_t *ent)
{
	if(!ent) return NULL;
	return NULL;
}