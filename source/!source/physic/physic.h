//=======================================================================
//			Copyright XashXT Group 2007 ©
//			physic.h - physics library header
//=======================================================================
#ifndef BASE_PHYSICS_H
#define BASE_PHYSICS_H

#include <stdio.h>
#include <windows.h>
#include "basetypes.h"
#include "mathlib.h"

#ifdef __cplusplus
extern "C" { 
#endif

extern physic_imp_t pi;
extern stdlib_api_t com;
extern byte *physpool;

extern void Phys_LoadBSP( uint *buffer );
extern void Phys_FreeBSP( void );
extern void DebugShowCollision ( cmdraw_t callback );
extern void Phys_Frame( float time );
extern void Phys_CreateBody( sv_edict_t *ed, vec3_t mins, vec3_t maxs, vec3_t org, vec3_t ang, int solid, NewtonCollision **newcol, NewtonBody **newbody );
extern void Phys_RemoveBody( NewtonBody *body );
extern bool InitPhysics( void );
extern void FreePhysics( void );

#ifdef __cplusplus
}
#endif

#define Host_Error com.error
#define Sys_Error com.error

#endif//BASE_PHYSICS_H