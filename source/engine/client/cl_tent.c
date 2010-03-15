//=======================================================================
//			Copyright XashXT Group 2009 ©
//		   cl_tent.c - temp entity effects management
//=======================================================================

#include "common.h"
#include "client.h"
#include "effects_api.h"
#include "triangle_api.h"
#include "beam_def.h"
#include "const.h"

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
				pTemp->pvEngineData = Mem_Alloc( cls.mempool, sizeof( studioframe_t ));
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
