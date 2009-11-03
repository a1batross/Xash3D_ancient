//=======================================================================
//			Copyright (C) XashXT Group 2008
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "cbase.h"
#include "client.h"

class CPhysEntity : public CBaseEntity
{
public:
	void Precache( void )
	{
		UTIL_PrecacheModel( pev->model, Model() );
	}
	void Spawn( void )
	{
		Precache();

		pev->movetype = MOVETYPE_PHYSIC;
		pev->solid = SolidType();
		pev->owner = ENT( pev ); // g-cont. i'm forget why needs this stuff :)

		UTIL_SetOrigin( this, pev->origin );
		UTIL_SetModel( ENT( pev ), pev->model, (char *)Model() );

		SetObjectClass( ED_RIGIDBODY );
	}
	virtual const char *Model( void ){ return NULL; }
	virtual int SolidType( void ){ return SOLID_MESH; } // generic but slow
	void PostActivate( void ) { } // stub
};
LINK_ENTITY_TO_CLASS( func_physbox, CPhysEntity );
