//=======================================================================
//			Copyright XashXT Group 2007 ©
//			physic.c - physics engine wrapper
//=======================================================================

#include "physic.h"

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
	Phys.LoadBSP = Phys_LoadBSP;
	Phys.FreeBSP = Phys_FreeBSP;
	Phys.DrawCollision = DebugShowCollision;
	Phys.Frame = Phys_Frame;
	Phys.CreateBody = Phys_CreateBody;
	Phys.RemoveBody = Phys_RemoveBody;

	return &Phys;
}