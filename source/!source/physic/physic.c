//=======================================================================
//			Copyright XashXT Group 2007 ©
//			physic.c - physics engine wrapper
//=======================================================================

#include "physic.h"

physic_imp_t	pi;
byte		*physpool;
NewtonWorld	*gWorld;

bool InitPhysics( void )
{
	physpool = Mem_AllocPool("Physics Pool");
	gWorld = NewtonCreate (Palloc, Pfree); // alloc world

	return true;
}

void FreePhysics( void )
{
	NewtonDestroy( gWorld );
	Mem_FreePool( &physpool );
}

physic_exp_t DLLEXPORT *CreateAPI ( physic_imp_t *import )
{
	static physic_exp_t		Phys;

	// Sys_LoadLibrary can create fake instance, to check
	// api version and api size, but first argument will be 0
	// and always make exception, run simply check for avoid it
	if(import) pi = *import;

	// generic functions
	Phys.api_size = sizeof(physic_exp_t);

	Phys.Init = InitPhysics;
	Phys.Shutdown = FreePhysics;
	Phys.LoadBSP = Phys_LoadBSP;
	Phys.FreeBSP = Phys_FreeBSP;
	Phys.ShowCollision = DebugShowCollision;
	Phys.Frame = Phys_Frame;
	Phys.CreateBOX = Phys_CreateBOX;
	Phys.RemoveBOX = Phys_RemoveBOX;

	return &Phys;
}