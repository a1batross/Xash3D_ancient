#include "common.h"
#include "server.h"

/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame.
==============
*/
void ClientThink (edict_t *ent, usercmd_t *ucmd)
{
	edict_t		*other;
	int		i, j;
	pmove_t		pm;

	for (i = 0; i < pm.numtouch; i++)
	{
		other = pm.touchents[i];
		for (j = 0; j < i; j++)
		{
			if (pm.touchents[j] == other)
				break;
		}
		if (j != i) continue; // duplicated
		if (!other->progs.sv->touch) continue;
		
		prog->globals.sv->pev = PRVM_EDICT_TO_PROG(other);
		prog->globals.sv->other = PRVM_EDICT_TO_PROG(ent);
		prog->globals.sv->time = sv.time;
		if( other->progs.sv->touch )
		{
			PRVM_ExecuteProgram (other->progs.sv->touch, "pev->touch");
		}
	}
}

/*
==================
SV_StartParticle

Make sure the event gets sent to all clients
==================
*/
void SV_StartParticle( vec3_t org, vec3_t dir, int color, int count )
{
	MsgDev( D_ERROR, "SV_StartParticle: implement me\n");
}