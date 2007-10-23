//=======================================================================
//			Copyright XashXT Group 2007 �
//			physic.c - physics engine wrapper
//=======================================================================

#include "physic.h"

physic_imp_t pi;

bool InitPhysics( void )
{
	return true;
}

void FreePhysics( void )
{
}

physic_exp_t DLLEXPORT *CreateAPI ( physic_imp_t *import )
{
	static physic_exp_t		Phys;

	// Sys_LoadLibrary can create fake instance, to check
	// api version and api size, but first argument will be 0
	// and always make exception, run simply check for avoid it
	if(import) pi = *import;

	//generic functions
	Phys.apiversion = PHYSIC_API_VERSION;
	Phys.api_size = sizeof(physic_exp_t);

	Phys.Init = InitPhysics;
	Phys.Shutdown = FreePhysics;

	return &Phys;
}