//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_studio.c - cm inline studio
//=======================================================================

#include "engine.h"
#include "server.h"

byte *SV_GetModelPtr( edict_t *ent )
{
	cmodel_t	*cmod;
	
	cmod = CM_RegisterModel( sv.configstrings[CS_MODELS + (int)ent->progs.sv->modelindex] );
	if(!cmod || !cmod->extradata) 
		return NULL;

	return cmod->extradata;
}

float *SV_GetModelVerts( sv_edict_t *ed, int *numvertices )
{
	cmodel_t	*cmod;
	edict_t	*ent;
	int	i;

	ent = PRVM_EDICT_NUM(ed->serialnumber);
	i = (int)ent->progs.sv->body;
         
	cmod = CM_RegisterModel( sv.configstrings[CS_MODELS + (int)ent->progs.sv->modelindex] );
	if( cmod && cmod->physmesh[i].verts )
	{
		*numvertices = cmod->physmesh[i].numverts;
		return (float *)cmod->physmesh[i].verts;
	}
	return NULL;
}

