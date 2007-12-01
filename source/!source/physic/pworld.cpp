//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         pworld.c - physic world wrapper
//=======================================================================

#include "physic.h"
#include "btBulletDynamicsCommon.h"

physic_imp_t	pi;
stdlib_api_t	com;
byte		*physpool;

void* Palloc (int size )
{
	return Mem_Alloc( physpool, size );
}

void Pfree (void *ptr, int size )
{
	Mem_Free( ptr );
}

bool InitPhysics( void )
{
	physpool = Mem_AllocPool("Physics Pool");

	return true;
}

void FreePhysics( void )
{
	Mem_FreePool( &physpool );
}

void Phys_LoadBSP( uint *buffer )
{
}

void Phys_FreeBSP( void )
{
}

void Phys_Frame( float time )
{
}

void Phys_CreateBody( sv_edict_t *ed, vec3_t mins, vec3_t maxs, vec3_t org, vec3_t ang, int solid, NewtonCollision **newcol, NewtonBody **newbody )
{
}

void Phys_RemoveBody( NewtonBody *body )
{
}

void DebugShowCollision ( cmdraw_t callback  ) 
{
}