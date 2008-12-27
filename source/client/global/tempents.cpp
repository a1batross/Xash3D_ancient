//=======================================================================
//			Copyright XashXT Group 2008 ©
//		tempents.cpp - client side entity management functions
//=======================================================================

#include "extdll.h"
#include "hud_iface.h"

void HUD_CreateEntities( void )
{
}

void HUD_StudioEvent( const dstudioevent_t *event, edict_t *entity )
{
	switch( event->event )
	{
	case 5001:
		// MullzeFlash at attachment 0
		break;
	case 5011:
		// MullzeFlash at attachment 1
		break;
	case 5021:
		// MullzeFlash at attachment 2
		break;
	case 5031:
		// MullzeFlash at attachment 3
		break;
	case 5002:
		// SparkEffect at attachment 0
		break;
	case 5004:		
		// Client side sound
		break;
	case 5005:		
		// Client side sound with random pitch
		break;
	case 5050:
		// Special event for displacer
		break;
	case 5060:
	          // eject shellEV_EjectShell( event, entity );
		break;
	default:
		break;
	}
}