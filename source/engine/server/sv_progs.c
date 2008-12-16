//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        sv_progs.c - server.dat interface
//=======================================================================

#include "common.h"
#include "server.h"
#include "byteorder.h"
#include "matrix_lib.h"
#include "const.h"

void SV_BeginIncreaseEdicts( void )
{
	int		i;
	edict_t		*ent;

	// links don't survive the transition, so unlink everything
	for( i = 0, ent = svg.edicts; i < svs.globals->maxEntities; i++, ent++ )
	{
		if( !ent->free ) SV_UnlinkEdict( svg.edicts + i ); // free old entity
		Mem_Set( &ent->pvEngineData->clusternums, 0, sizeof( ent->pvEngineData->clusternums ));
	}
	SV_ClearWorld();
}

void SV_EndIncreaseEdicts(void)
{
	int		i;
	edict_t		*ent;

	for( i = 0, ent = svg.edicts; i < svs.globals->maxEntities; i++, ent++ )
	{
		// link every entity except world
		if( !ent->free ) SV_LinkEdict(ent);
	}
}

void SV_RestoreEdict( edict_t *ent )
{
	// link it into the bsp tree
	SV_LinkEdict( ent );
	SV_CreatePhysBody( ent );
	SV_SetPhysForce( ent ); // restore forces ...
	SV_SetMassCentre( ent ); // and mass force

	if( ent->v.ambient ) // restore loopsound
		ent->pvEngineData->s.soundindex = SV_SoundIndex( STRING( ent->v.ambient ));
}