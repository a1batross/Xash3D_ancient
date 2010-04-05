//=======================================================================
//			Copyright XashXT Group 2009 ©
//		   cl_tent.c - temp entity effects management
//=======================================================================

#include "common.h"
#include "client.h"
#include "effects_api.h"
#include "tmpent_def.h"

/*
==============================================================

TEMPENTS MANAGEMENT

==============================================================
*/
int CL_AddEntity( edict_t *pEnt, int ed_type, shader_t customShader )
{
	if( !re || !pEnt )
		return false;

	// let the render reject entity without model
	return re->AddRefEntity( pEnt, ed_type, customShader );
}

int CL_AddTempEntity( TEMPENTITY *pTemp, shader_t customShader )
{
	if( !re || !pTemp )
		return false;

	if( !pTemp->pvEngineData )
	{
		if( pTemp->modelindex && !( pTemp->flags & FTENT_NOMODEL ))
		{
			// check model			
			modtype_t	type = CM_GetModelType( pTemp->modelindex );

			if( type == mod_studio || type == mod_sprite )
			{
				// alloc engine data to holds lerping values for studiomdls and sprites
				pTemp->pvEngineData = Mem_Alloc( cls.mempool, sizeof( lerpframe_t ));
			}
			else
			{
				pTemp->flags |= FTENT_NOMODEL;
				pTemp->modelindex = 0;
			}
		}
	}		

	// let the render reject entity without model
	return re->AddTmpEntity( pTemp, ED_TEMPENTITY, customShader );
}

/*
================
CL_TestEntities

if cl_testentities is set, create 32 player models
================
*/
void CL_TestEntities( void )
{
	int	i, j;
	float	f, r;
	edict_t	ent, *pl;

	if( !cl_testentities->integer )
		return;

	pl = CL_GetLocalPlayer();
	Mem_Set( &ent, 0, sizeof( edict_t ));
	V_ClearScene();

	for( i = 0; i < 32; i++ )
	{
		r = 64 * ((i%4) - 1.5 );
		f = 64 * (i/4) + 128;

		for( j = 0; j < 3; j++ )
			ent.v.origin[j] = cl.refdef.vieworg[j]+cl.refdef.forward[j] * f + cl.refdef.right[j] * r;

		ent.v.scale = 1.0f;
		ent.serialnumber = pl->serialnumber;
		ent.v.controller[0] = ent.v.controller[1] = 90.0f;
		ent.v.controller[2] = ent.v.controller[3] = 180.0f;
		ent.v.modelindex = pl->v.modelindex;
		re->AddRefEntity( &ent, ED_NORMAL, -1 );
	}
}