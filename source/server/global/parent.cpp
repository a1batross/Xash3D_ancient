//=======================================================================
//			Copyright (C) Shambler Team 2005
//		         	 parent.cpp - xash parent system 
//=======================================================================

#include	"extdll.h"
#include  "utils.h"
#include	"cbase.h"
#include	"saverestore.h"
#include	"client.h"
#include	"decals.h"
#include	"gamerules.h"
#include	"game.h"
#include	"defaults.h"

CWorld *g_pWorld = NULL; //world pointer
extern BOOL CanAffect;
BOOL NeedUpdate( CBaseEntity *pEnt );

//=======================================================================
//			debug utils
//=======================================================================
void GetPFlags( CBaseEntity* Ent ) //get parent system flags
{
	char szFlags[256] = "";//clear line
	
	CHECK_FLAG(PF_AFFECT);
	CHECK_FLAG(PF_DESIRED);
	CHECK_FLAG(PF_ACTION);
	CHECK_FLAG(PF_MOVENONE);
	CHECK_FLAG(PF_CORECTSPEED);
	CHECK_FLAG(PF_MERGEPOS);
	CHECK_FLAG(PF_ANGULAR);
	CHECK_FLAG(PF_POSTVELOCITY);
	CHECK_FLAG(PF_POSTAVELOCITY);
	CHECK_FLAG(PF_SETTHINK);
	CHECK_FLAG(PF_POSTAFFECT);
          CHECK_FLAG(PF_LINKCHILD);
          
	ALERT(at_console, "=========/Xash Parent System Info:/=========\n");
	if(!Ent->pev->targetname)ALERT(at_console, "INFO: %s has follow flags:\n", STRING(Ent->pev->classname) );
	else ALERT(at_console, "INFO: %s with name %s has follow flags:\n", STRING(Ent->pev->classname), STRING(Ent->pev->targetname) );
	ALERT(at_console, "%s\n", szFlags );
	SHIFT;
}

void GetPInfo( CBaseEntity* Ent ) //get parent system parameters
{
	int name		= 0;
	int parentname	= 0;
	int childsname	= 0;
	char parent[32]	= "";
	char chname[256]	= "";
	char buffer[32];
	
	Vector pVelocity, pAvelocity, cVelocity, cAvelocity;
	
	if(Ent->pev->targetname)name = Ent->pev->targetname;
          else name = Ent->pev->classname; 
	
	if(Ent->m_pParent)
	{
		if(Ent->m_pParent->pev->targetname)parentname = Ent->m_pParent->pev->targetname;
		else parentname = Ent->m_pParent->pev->classname;
		pVelocity = Ent->m_pParent->pev->velocity;
		pAvelocity = Ent->m_pParent->pev->avelocity;
		sprintf( parent, "with parent \"%s\"", STRING(parentname) );
	}
	else sprintf(parent, "without parent");
	
	if( Ent->m_pChild )
	{
		CBaseEntity *pInfo = Ent->m_pChild;
		int sloopbreaker = MAX_CHILDS;
		
		while (pInfo)
		{
			if(pInfo->pev->targetname) childsname = pInfo->pev->targetname;
			else childsname = pInfo->pev->classname;
			pInfo = pInfo->m_pNextChild;
			if(pInfo)sprintf(buffer, "\"%s\", ", STRING(childsname));
			else sprintf(buffer, "\"%s\"", STRING(childsname));//it,s last child
			strcat( chname, buffer);
			sloopbreaker--;
			if (sloopbreaker <= 0)break;
		}
	}
	else sprintf(chname, "this entity not have childs");

	cVelocity = Ent->pev->velocity;
	cAvelocity = Ent->pev->avelocity;

	//print info
	Msg("=========/Xash Parent System Info:/=========\n");
	Msg("INFO: Entity \"%s\" %s, have following parameters:\n", STRING(name), parent );
	Msg("Child names: %s. Self Velocity: %g %g %g. Self Avelocity %g %g %g\n", chname, cVelocity.x, cVelocity.y, cVelocity.z, cAvelocity.x, cAvelocity.y, cAvelocity.z );
	if(Ent->m_pParent)Msg("Parent Velocity: %g %g %g. Parent Avelocity: %g %g %g.\n", pVelocity.x, pVelocity.y, pVelocity.z, pAvelocity.x, pAvelocity.y, pAvelocity.z );
	else SHIFT;
}

//=======================================================================
//			physics frame
//=======================================================================

void PhysicsFrame( void )
{
	CBaseEntity *pListMember;

	if ( !g_pWorld )return;
	pListMember = g_pWorld;

	while ( pListMember->m_pLinkList )// handle the remaining entries in the list
	{
		FrameAffect(pListMember->m_pLinkList);
		if (!(pListMember->m_pLinkList->pFlags & PF_LINKCHILD))
		{
			CBaseEntity *pTemp = pListMember->m_pLinkList;
			pListMember->m_pLinkList = pListMember->m_pLinkList->m_pLinkList;
			pTemp->m_pLinkList = NULL;
		}
		else	pListMember = pListMember->m_pLinkList;
	}
}

void PhysicsPostFrame( void )
{
	CBaseEntity *pListMember;
	int loopbreaker = 1024; //max edicts
	if (CanAffect) ALERT(at_console, "Affect already applied ?!\n");
	CanAffect = TRUE;
	if (!g_pWorld) return;

	pListMember = g_pWorld;
	CBaseEntity *pNext;

	pListMember = g_pWorld->m_pLinkList;

	while (pListMember)
	{
		pNext = pListMember->m_pLinkList;
		PostFrameAffect( pListMember );
		pListMember = pNext;
		loopbreaker--;
		if (loopbreaker <= 0)break;
	}
	CanAffect = FALSE;
}

//=======================================================================
//			affect & postaffect
//=======================================================================

int FrameAffect( CBaseEntity *pEnt )
{
	if (gpGlobals->frametime == 0)return 0;
	if (!(pEnt->pFlags & PF_AFFECT)) return 0;
	if (pEnt->m_fNextThink <= 0)
	{
		ClearBits(pEnt->pFlags, PF_AFFECT);
		return 0; // cancelling think
	}
	float fFraction = 0;
	if (pEnt->pev->movetype == MOVETYPE_PUSH)
	{
		if (pEnt->m_fNextThink <= pEnt->pev->ltime + gpGlobals->frametime)
			fFraction = (pEnt->m_fNextThink - pEnt->pev->ltime)/gpGlobals->frametime;
	}

	else if (pEnt->m_fNextThink <= gpGlobals->time + gpGlobals->frametime)
		fFraction = (pEnt->m_fNextThink - gpGlobals->time)/gpGlobals->frametime;
	if (fFraction)
	{
		if (pEnt->pFlags & PF_CORECTSPEED)
		{
			if (!(pEnt->pFlags & PF_POSTVELOCITY))
			{
				pEnt->PostVelocity = pEnt->pev->velocity;
				SetBits (pEnt->pFlags, PF_POSTVELOCITY);
			}

			if (!(pEnt->pFlags & PF_POSTAVELOCITY))
			{
				pEnt->PostAvelocity = pEnt->pev->avelocity;
				SetBits (pEnt->pFlags, PF_POSTAVELOCITY);
			}

			Vector vecVelTemp = pEnt->pev->velocity;
			Vector vecAVelTemp = pEnt->pev->avelocity;

			if (pEnt->m_pParent)
			{
				pEnt->pev->velocity = (pEnt->pev->velocity - pEnt->m_pParent->pev->velocity)*fFraction + pEnt->m_pParent->pev->velocity;
				pEnt->pev->avelocity = (pEnt->pev->avelocity - pEnt->m_pParent->pev->avelocity)*fFraction + pEnt->m_pParent->pev->avelocity;
			}
			else
			{
				pEnt->pev->velocity = pEnt->pev->velocity*fFraction;
				pEnt->pev->avelocity = pEnt->pev->avelocity*fFraction;
			}

			HandleAffect( pEnt, vecVelTemp - pEnt->pev->velocity, vecAVelTemp - pEnt->pev->avelocity );
			UTIL_SetPostAffect( pEnt );
		}

		UTIL_SetThink( pEnt );
		ClearBits(pEnt->pFlags, PF_AFFECT);
	}
	return 1;
}

int PostFrameAffect( CBaseEntity *pEnt )
{
	if (pEnt->pFlags & PF_DESIRED)ClearBits(pEnt->pFlags, PF_DESIRED);
	else return 0;
	if (pEnt->pFlags & PF_ACTION)
	{
		ClearBits(pEnt->pFlags, PF_ACTION);
		pEnt->DesiredAction();
		if(NeedUpdate(pEnt)) SetBits(pEnt->m_pChild->pFlags, PF_MERGEPOS);
	}
	if (pEnt->pFlags & PF_POSTAFFECT)
	{
		ClearBits(pEnt->pFlags, PF_POSTAFFECT);
		HandlePostAffect( pEnt );
	}
	if (pEnt->pFlags & PF_SETTHINK)
	{
		ClearBits(pEnt->pFlags, PF_SETTHINK);
		pEnt->Think();
	}
	return 1;
}

//=======================================================================
//			affect handles
//=======================================================================

void HandlePostAffect( CBaseEntity *pEnt )
{
	if (pEnt->pFlags & PF_POSTVELOCITY)
	{
		pEnt->pev->velocity = pEnt->PostVelocity;
		pEnt->PostVelocity = g_vecZero;
		ClearBits (pEnt->pFlags, PF_POSTVELOCITY);
	}
	if (pEnt->pFlags & PF_POSTAVELOCITY)
	{
		pEnt->pev->avelocity = pEnt->PostAvelocity;
		pEnt->PostAvelocity = g_vecZero;
		ClearBits (pEnt->pFlags, PF_POSTAVELOCITY);
	}
	if (pEnt->pFlags & PF_MERGEPOS || pEnt->pFlags & PF_POSTORG)
	{
		UTIL_MergePos( pEnt );
	}
	CBaseEntity *pChild;
	for (pChild = pEnt->m_pChild; pChild != NULL; pChild = pChild->m_pNextChild)
		HandlePostAffect( pChild );
}

void HandleAffect( CBaseEntity *pEnt, Vector vecAdjustVel, Vector vecAdjustAVel )
{
	CBaseEntity *pChild;
	for(pChild = pEnt->m_pChild; pChild != NULL; pChild = pChild->m_pNextChild)
	{
		if (!(pEnt->pFlags & PF_POSTVELOCITY))
		{
			pChild->PostVelocity = pChild->pev->velocity;
			SetBits (pEnt->pFlags, PF_POSTVELOCITY);
		}
		if (!(pEnt->pFlags & PF_POSTAVELOCITY))
		{
			pChild->PostAvelocity = pChild->pev->avelocity;
			SetBits (pEnt->pFlags, PF_POSTAVELOCITY);
		}
		pChild->pev->velocity = pChild->pev->velocity - vecAdjustVel;
		pChild->pev->avelocity = pChild->pev->avelocity - vecAdjustAVel;
		HandleAffect( pChild, vecAdjustVel, vecAdjustAVel );
	}
}

//=======================================================================
//			link operations
//=======================================================================
void LinkChild(CBaseEntity *pEnt) //add children to our list
{
	if (pEnt->m_pLinkList)return;

	if ( !g_pWorld )return;//too early ?
	CBaseEntity *pListMember = g_pWorld;

	// find the last entry in the list
	while (pListMember->m_pLinkList != NULL) pListMember = pListMember->m_pLinkList;
	if (pListMember == pEnt)return; //entity has in list

	pListMember->m_pLinkList = pEnt;//add entity to the list.
}

BOOL SynchLost( CBaseEntity *pEnt )
{
	if(pEnt->pFlags & PF_PARENTMOVE)//moving parent
	{
		if(!pEnt->m_pParent) return FALSE;
		if(pEnt->m_pParent->pev->origin != pEnt->PostOrigin) 
		{
			if(pEnt->pev->solid != SOLID_BSP) return TRUE;
		}
		if(pEnt->m_pParent->pev->angles != pEnt->PostAngles)
		{
			if(pEnt->pev->solid != SOLID_BSP) return TRUE;
		}
	}
	return FALSE;
}

BOOL NeedUpdate( CBaseEntity *pEnt )
{
	if( pEnt->m_pChild && pEnt->m_pChild->OffsetOrigin == g_vecZero)//potentially loser
	{
		if(pEnt->pev->origin != pEnt->m_pChild->pev->origin)
			return TRUE;
	}
	return FALSE;
}