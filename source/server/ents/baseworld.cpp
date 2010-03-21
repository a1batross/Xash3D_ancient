//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "cbase.h"
#include "client.h"


void CWorld :: Precache( void )
{
	g_pWorld = this;
	m_pLinkList = NULL;
	InitWorld();
}

void CWorld :: KeyValue( KeyValueData *pkvd )
{
	if( FStrEq( pkvd->szKeyName, "skyname" ))
	{
		pev->target = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "chaptertitle") )
	{
		pev->netname = ALLOC_STRING( pkvd->szValue ); 
		CVAR_SET_FLOAT( "sv_newunit", 1 ); //new chapter
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "gametitle") )
	{
		SetBits( pev->spawnflags, SF_START_ON );
		pkvd->fHandled = TRUE;
	}
}

void CWorld :: Spawn( void )
{
	g_fGameOver = FALSE;
	Precache();
}

void CWorld :: PostActivate( void )
{
	// run post messages
	if ( pev->netname )
	{
		UTIL_ShowMessageAll( STRING(pev->netname) );
		pev->netname = iStringNull;
	}

	if ( pev->spawnflags & SF_START_ON )
	{
		SERVER_COMMAND( "gametitle\n" );
		ClearBits( pev->spawnflags, SF_START_ON );
	}

	if ( pev->target )
	{
		// Sent over net now.
		SET_SKYBOX( STRING( pev->target ));
	}
	else
	{
		// tell engine there is no skybox set
		SET_SKYBOX( "<skybox>" );
	}
}
LINK_ENTITY_TO_CLASS( worldspawn, CWorld );