//=======================================================================
//			Copyright XashXT Group 2009 ©
//		     pm_local.h - local variables and constants
//=======================================================================
#ifndef PM_LOCAL_H
#define PM_LOCAL_H

// helper macros
#define g_vecZero		Vector( 0, 0, 0 )
#define EmitSound		(*pmove->PM_PlaySound)
#define Info_ValueForKey( x )	(*pmove->PM_Info_ValueForKey)( pmove->physinfo, x )
#define RANDOM_LONG		(*pmove->RandomLong)
#define TRACE_TEXTURE	(*pmove->PM_TraceTexture)
#define TRACE_PLAYER	(*pmove->PM_PlayerTrace)
#define TRACE_MODEL		(*pmove->PM_TraceModel)
#define POINT_CONTENTS	(*pmove->PM_PointContents)
#define ALERT		(*pmove->AlertMessage)
#define STRING		(*pmove->PM_GetString)
#define ENTINDEX		(*pmove->PM_GetEntityByIndex)
#define TEST_PLAYER		(*pmove->PM_TestPlayerPosition)
#define TEST_STUCK		(*pmove->PM_StuckTouch)
#define Mod_GetBounds	(*pmove->PM_GetModelBounds)
#define Mod_GetType		(*pmove->PM_GetModelType)
#define AngleVectors	(*pmove->AngleVectors)
#define player_index()	(pev->pContainingEntity->serialnumber - 1)

#endif//PM_LOCAL_H