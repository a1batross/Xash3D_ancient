//=======================================================================
//			Copyright XashXT Group 2007 ©
//			physic.c - physics engine wrapper
//=======================================================================

#include "physic.h"

physic_imp_t	pi;
stdlib_api_t	com;
physic_t		ph;	// physic globalstate
byte		*physpool;
NewtonWorld	*gWorld;

cvar_t		*cm_use_triangles;
cvar_t		*cm_solver_model;
cvar_t		*cm_friction_model;

bool InitPhysics( void )
{
	physpool = Mem_AllocPool("Physics Pool");
	gWorld = NewtonCreate (Palloc, Pfree); // alloc world

	cm_use_triangles = Cvar_Get("cm_convert_polygons", "1", CVAR_INIT|CVAR_SYSTEMINFO );//, "convert bsp polygons to triangles, slowly but more safety way" );
	cm_solver_model = Cvar_Get("cm_solver", "0", CVAR_ARCHIVE );//, "change solver model: 0 - precision, 1 - adaptive, 2 - fast. (changes need restart server to take effect)" );
	cm_friction_model = Cvar_Get("cm_friction", "0", CVAR_ARCHIVE );//, "change solver model: 0 - precision, 1 - adaptive. (changes need restart server to take effect)" );

	return true;
}

void FreePhysics( void )
{
	NewtonDestroy( gWorld );
	Mem_FreePool( &physpool );
}

physic_exp_t DLLEXPORT *CreateAPI ( stdlib_api_t *input, physic_imp_t *engfuncs )
{
	static physic_exp_t		Phys;

	com = *input;

	// Sys_LoadLibrary can create fake instance, to check
	// api version and api size, but second argument will be 0
	// and always make exception, run simply check for avoid it
	if(engfuncs) pi = *engfuncs;

	// generic functions
	Phys.api_size = sizeof(physic_exp_t);

	Phys.Init = InitPhysics;
	Phys.Shutdown = FreePhysics;
	Phys.LoadBSP = CM_LoadBSP;
	Phys.FreeBSP = CM_FreeBSP;
	Phys.WriteCollisionLump = CM_SaveCollisionTree;
	Phys.DrawCollision = DebugShowCollision;
	Phys.LoadWorld = CM_LoadWorld;
	Phys.FreeWorld = CM_FreeWorld;
	Phys.Frame = Phys_Frame;
	Phys.CreateBody = Phys_CreateBody;
	Phys.RemoveBody = Phys_RemoveBody;

	return &Phys;
}