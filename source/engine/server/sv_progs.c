//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        sv_progs.c - server.dat interface
//=======================================================================

#include "common.h"
#include "server.h"
#include "byteorder.h"
#include "matrix_lib.h"
#include "const.h"

void SV_RestoreEdict( edict_t *ent )
{
	// link it into the bsp tree
	SV_LinkEdict( ent );
	SV_CreatePhysBody( ent );
	SV_SetPhysForce( ent ); // restore forces ...
	SV_SetMassCentre( ent ); // and mass force
}