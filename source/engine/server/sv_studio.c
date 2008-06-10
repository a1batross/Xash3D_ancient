//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_studio.c - cm inline studio
//=======================================================================

#include "common.h"
#include "server.h"

cmodel_t *SV_GetModelPtr( edict_t *ent )
{
	return pe->RegisterModel( sv.configstrings[CS_MODELS + (int)ent->progs.sv->modelindex] );
}

float *SV_GetModelVerts( sv_edict_t *ed, int *numvertices )
{
	cmodel_t	*cmod;
	edict_t	*ent;

	ent = PRVM_EDICT_NUM(ed->serialnumber);
	cmod = pe->RegisterModel( sv.configstrings[CS_MODELS + (int)ent->progs.sv->modelindex] );
	if( cmod )
	{
		int i = (int)ent->progs.sv->body;
		i = bound( 0, i, cmod->numbodies ); // make sure what body exist
		
		if( cmod->physmesh[i].verts )
		{
			*numvertices = cmod->physmesh[i].numverts;
			return (float *)cmod->physmesh[i].verts;
		}
	}
	return NULL;
}

