//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        sv_game.c - server.dat interface
//=======================================================================

#include "engine.h"
#include "server.h"

void SV_BeginIncreaseEdicts(void)
{
	int		i;
	edict_t		*ent;

	PRVM_Free( sv.moved_edicts );
	sv.moved_edicts = (edict_t **)PRVM_Alloc(prog->max_edicts * sizeof(edict_t *));

	// links don't survive the transition, so unlink everything
	for (i = 0, ent = prog->edicts; i < prog->max_edicts; i++, ent++)
	{
		if (!ent->priv.sv->free) SV_UnlinkEdict(prog->edicts + i); //free old entity
		memset(&ent->priv.sv->clusternums, 0, sizeof(ent->priv.sv->clusternums));
	}
	SV_ClearWorld();
}

void SV_EndIncreaseEdicts(void)
{
	int		i;
	edict_t		*ent;

	for (i = 0, ent = prog->edicts; i < prog->max_edicts; i++, ent++)
	{
		// link every entity except world
		if (!ent->priv.sv->free) SV_LinkEdict(ent);
	}
}

/*
=================
SV_InitEdict

Alloc new edict from list
=================
*/
void SV_InitEdict (edict_t *e)
{
	e->priv.sv->serialnumber = PRVM_NUM_FOR_EDICT(e);
}

/*
=================
SV_FreeEdict

Marks the edict as free
=================
*/
void SV_FreeEdict( edict_t *ed )
{
	// unlink from world
	SV_UnlinkEdict( ed );

	ed->priv.sv->freetime = sv.time;
	ed->priv.sv->free = true;

	ed->progs.sv->model = 0;
	ed->progs.sv->takedamage = 0;
	ed->progs.sv->modelindex = 0;
	ed->progs.sv->skin = 0;
	ed->progs.sv->frame = 0;
	ed->progs.sv->solid = 0;

	pe->RemoveBody( ed->priv.sv->physbody );
	VectorClear(ed->progs.sv->origin);
	VectorClear(ed->progs.sv->angles);
	ed->progs.sv->nextthink = -1;
	ed->priv.sv->physbody = NULL;
}

void SV_CountEdicts( void )
{
	edict_t	*ent;
	int	i, active = 0, models = 0, solid = 0, step = 0;

	for (i = 0; i < prog->num_edicts; i++)
	{
		ent = PRVM_EDICT_NUM(i);
		if (ent->priv.sv->free) continue;
		active++;
		if (ent->progs.sv->solid) solid++;
		if (ent->progs.sv->model) models++;
		if (ent->progs.sv->movetype == MOVETYPE_STEP) step++;
	}

	Msg("num_edicts:%3i\n", prog->num_edicts);
	Msg("active    :%3i\n", active);
	Msg("view      :%3i\n", models);
	Msg("touch     :%3i\n", solid);
	Msg("step      :%3i\n", step);
}

bool SV_LoadEdict( edict_t *ent )
{
	int current_skill = (int)Cvar_VariableValue("skill");

	// remove things from different skill levels or deathmatch
	if(Cvar_VariableValue ("deathmatch"))
	{
		if( (int)ent->progs.sv->spawnflags & SPAWNFLAG_NOT_DEATHMATCH )
			return false;
	}
	else if(current_skill <= 0 && (int)ent->progs.sv->spawnflags & SPAWNFLAG_NOT_EASY  )
		return false;
	else if(current_skill == 1 && (int)ent->progs.sv->spawnflags & SPAWNFLAG_NOT_MEDIUM)
		return false;
	else if(current_skill >= 2 && (int)ent->progs.sv->spawnflags & SPAWNFLAG_NOT_HARD  )
		return false;
	return true;
}

void SV_VM_Begin( void )
{
	PRVM_Begin;
	PRVM_SetProg( PRVM_SERVERPROG );

	if( prog ) *prog->time = sv.time;
}

void SV_VM_End( void )
{
	PRVM_End;
}

/*
==============
SpawnEntities

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.
==============
*/
void SV_SpawnEntities( const char *mapname, const char *entities )
{
	edict_t	*ent;
	int	i;

	MsgDev( D_NOTE, "SV_SpawnEntities()\n" );

	// used by PushMove to move back pushed entities
	sv.moved_edicts = (edict_t **)PRVM_Alloc(prog->max_edicts * sizeof(edict_t *));

	prog->protect_world = false; // allow to change world parms

	ent = PRVM_EDICT_NUM( 0 );
	memset (ent->progs.sv, 0, prog->progs->entityfields * 4);
	ent->priv.sv->free = false;
	ent->progs.sv->model = PRVM_SetEngineString( sv.configstrings[CS_MODELS] );
	ent->progs.sv->modelindex = 1; // world model
	ent->progs.sv->solid = SOLID_BSP;
	ent->progs.sv->movetype = MOVETYPE_PUSH;

	SV_ConfigString (CS_MAXCLIENTS, va("%i", maxclients->integer ));
	prog->globals.sv->mapname = PRVM_SetEngineString( sv.name );

	// spawn the rest of the entities on the map
	*prog->time = sv.time;

	// set client fields on player ents
	for (i = 1; i < maxclients->value; i++)
	{
		// setup all clients
		ent = PRVM_EDICT_NUM( i );
		ent->priv.sv->client = svs.gclients + i - 1;
	}

	PRVM_ED_LoadFromFile( entities );
	prog->protect_world = true; // make world read-only
}

/*
===============
SV_InitGameProgs

Init the game subsystem for a new map
===============
*/
void SV_InitServerProgs( void )
{
	Msg("\n");
	PRVM_Begin;
	PRVM_InitProg( PRVM_SERVERPROG );

	prog->reserved_edicts = maxclients->integer;
	prog->loadintoworld = true;
		
	if( !prog->loaded )
	{        
		prog->progs_mempool = Mem_AllocPool("Server Progs" );
		prog->name = "server";
		prog->builtins = vm_sv_builtins;
		prog->numbuiltins = vm_sv_numbuiltins;
		prog->max_edicts = MAX_EDICTS<<2;
		prog->limit_edicts = MAX_EDICTS;
		prog->edictprivate_size = sizeof(sv_edict_t);
		prog->begin_increase_edicts = SV_BeginIncreaseEdicts;
		prog->end_increase_edicts = SV_EndIncreaseEdicts;
		prog->init_edict = SV_InitEdict;
		prog->free_edict = SV_FreeEdict;
		prog->count_edicts = SV_CountEdicts;
		prog->load_edict = SV_LoadEdict;
		prog->extensionstring = "";

		// using default builtins
		prog->init_cmd = VM_Cmd_Init;
		prog->reset_cmd = VM_Cmd_Reset;
		prog->error_cmd = VM_Error;
		prog->flag |= PRVM_OP_STATE; // enable op_state feature
		PRVM_LoadProgs( "server.dat", 0, NULL, SV_NUM_REQFIELDS, sv_reqfields );
	}
	PRVM_End;
}

/*
===============
SV_ShutdownGameProgs

Called when either the entire server is being killed, or
it is changing to a different game directory.
===============
*/
void SV_FreeServerProgs( void )
{
	edict_t	*ent;
	int	i;

	SV_VM_Begin();

	for (i = 1; prog && i < prog->num_edicts; i++)
	{
		ent = PRVM_EDICT_NUM(i);
		SV_FreeEdict( ent );// release physic
	}
	SV_VM_End();

	if(!svs.gclients) return;
	Mem_Free( svs.gclients );
	svs.gclients = NULL;

}