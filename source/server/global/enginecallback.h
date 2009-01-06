/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#ifndef ENGINECALLBACK_H
#define ENGINECALLBACK_H
#pragma once

// Must be provided by user of this code
extern enginefuncs_t g_engfuncs;

// The actual engine callbacks
#define MALLOC( x )		(*g_engfuncs.pfnMemAlloc)( x, __FILE__, __LINE__ )
#define CALLOC( x, y )	(*g_engfuncs.pfnMemAlloc)((x) * (y), __FILE__, __LINE__ )
#define FREE( x )		(*g_engfuncs.pfnMemFree)( x, __FILE__, __LINE__ )
#define PRECACHE_MODEL	(*g_engfuncs.pfnPrecacheModel)
#define PRECACHE_SOUND	(*g_engfuncs.pfnPrecacheSound)
#define PRECACHE_GENERIC	(*g_engfuncs.pfnPrecacheGeneric)
#define SET_MODEL		(*g_engfuncs.pfnSetModel)
#define MODEL_INDEX		(*g_engfuncs.pfnModelIndex)
#define MODEL_FRAMES	(*g_engfuncs.pfnModelFrames)
#define SET_SIZE		(*g_engfuncs.pfnSetSize)
#define CHANGE_LEVEL	(*g_engfuncs.pfnChangeLevel)
#define GET_SPAWN_PARMS	(*g_engfuncs.pfnGetSpawnParms)
#define SAVE_SPAWN_PARMS	(*g_engfuncs.pfnSaveSpawnParms)
#define VEC_TO_YAW		(*g_engfuncs.pfnVecToYaw)
#define VEC_TO_ANGLES	(*g_engfuncs.pfnVecToAngles)
#define MOVE_TO_ORIGIN 	(*g_engfuncs.pfnMoveToOrigin)
#define oldCHANGE_YAW	(*g_engfuncs.pfnChangeYaw)
#define CHANGE_PITCH	(*g_engfuncs.pfnChangePitch)
#define MAKE_VECTORS	(*g_engfuncs.pfnMakeVectors)
#define CREATE_ENTITY	(*g_engfuncs.pfnCreateEntity)
#define REMOVE_ENTITY	(*g_engfuncs.pfnRemoveEntity)
#define CREATE_NAMED_ENTITY	(*g_engfuncs.pfnCreateNamedEntity)
#define MAKE_STATIC		(*g_engfuncs.pfnMakeStatic)
#define ENT_IS_ON_FLOOR	(*g_engfuncs.pfnEntIsOnFloor)
#define DROP_TO_FLOOR	(*g_engfuncs.pfnDropToFloor)
#define WALK_MOVE		(*g_engfuncs.pfnWalkMove)
#define SET_ORIGIN		(*g_engfuncs.pfnSetOrigin)
#define EMIT_SOUND_DYN2	(*g_engfuncs.pfnEmitSound)
#define BUILD_SOUND_MSG	(*g_engfuncs.pfnBuildSoundMsg)
#define TRACE_LINE		(*g_engfuncs.pfnTraceLine)
#define TRACE_TOSS		(*g_engfuncs.pfnTraceToss)
#define TRACE_MONSTER_HULL	(*g_engfuncs.pfnTraceMonsterHull)
#define TRACE_HULL		(*g_engfuncs.pfnTraceHull)
#define GET_AIM_VECTOR	(*g_engfuncs.pfnGetAimVector)
#define SERVER_COMMAND	(*g_engfuncs.pfnServerCommand)
#define SERVER_EXECUTE	(*g_engfuncs.pfnServerExecute)
#define CLIENT_COMMAND	(*g_engfuncs.pfnClientCommand)
#define PARTICLE_EFFECT	(*g_engfuncs.pfnParticleEffect)
#define LIGHT_STYLE		(*g_engfuncs.pfnLightStyle)
#define DECAL_INDEX		(*g_engfuncs.pfnDecalIndex)
#define POINT_CONTENTS	(*g_engfuncs.pfnPointContents)
#define CRC_INIT		(*g_engfuncs.pfnCRC_Init)
#define CRC_PROCESS_BUFFER	(*g_engfuncs.pfnCRC_ProcessBuffer)
#define CRC_FINAL		(*g_engfuncs.pfnCRC_Final)
#define RANDOM_LONG		(*g_engfuncs.pfnRandomLong)
#define RANDOM_FLOAT	(*g_engfuncs.pfnRandomFloat)
#define CLASSIFY_EDICT	(*g_engfuncs.pfnClassifyEdict)

inline void MESSAGE_BEGIN( int msg_dest, int msg_type, const float *pOrigin = NULL, edict_t *ed = NULL )
{
	(*g_engfuncs.pfnMessageBegin)( msg_dest, msg_type, pOrigin, ed );
}

#define MESSAGE_END		(*g_engfuncs.pfnMessageEnd)
#define WRITE_BYTE		(*g_engfuncs.pfnWriteByte)
#define WRITE_CHAR		(*g_engfuncs.pfnWriteChar)
#define WRITE_SHORT		(*g_engfuncs.pfnWriteShort)
#define WRITE_LONG		(*g_engfuncs.pfnWriteLong)
#define WRITE_ANGLE		(*g_engfuncs.pfnWriteAngle)
#define WRITE_COORD		(*g_engfuncs.pfnWriteCoord)
#define WRITE_FLOAT		(*g_engfuncs.pfnWriteFloat)
#define WRITE_LONG64	(*g_engfuncs.pfnWriteLong64)
#define WRITE_DOUBLE	(*g_engfuncs.pfnWriteDouble)
#define WRITE_STRING	(*g_engfuncs.pfnWriteString)
#define WRITE_ENTITY	(*g_engfuncs.pfnWriteEntity)
#define CVAR_REGISTER	(*g_engfuncs.pfnCVarRegister)
#define CVAR_GET_FLOAT	(*g_engfuncs.pfnCVarGetFloat)
#define CVAR_GET_STRING	(*g_engfuncs.pfnCVarGetString)
#define CVAR_SET_FLOAT	(*g_engfuncs.pfnCVarSetFloat)
#define CVAR_SET_STRING	(*g_engfuncs.pfnCVarSetString)
#define ALERT		(*g_engfuncs.pfnAlertMessage)
#define ENGINE_FPRINTF	(*g_engfuncs.pfnEngineFprintf)
#define ALLOC_PRIVATE	(*g_engfuncs.pfnPvAllocEntPrivateData)
inline void *GET_PRIVATE( edict_t *pent )
{
	if ( pent )
		return pent->pvPrivateData;
	return NULL;
}

// NOTE: Xash3D using custom StringTable System that using safety methods for access to strings
// and never make duplicated strings, so it make no differences between ALLOC_STRING and MAKE_STRING
// leave macros as legacy  
#define ALLOC_STRING	(*g_engfuncs.pfnAllocString)
#define MAKE_STRING		(*g_engfuncs.pfnAllocString)
#define STRING		(*g_engfuncs.pfnGetString)

#define FREE_PRIVATE	(*g_engfuncs.pfnFreeEntPrivateData)
#define FIND_ENTITY_BY_STRING	(*g_engfuncs.pfnFindEntityByString)
#define GETENTITYILLUM	(*g_engfuncs.pfnGetEntityIllum)
#define FIND_ENTITY_IN_SPHERE	(*g_engfuncs.pfnFindEntityInSphere)
#define FIND_CLIENT_IN_PVS	(*g_engfuncs.pfnFindClientInPVS)
#define EMIT_AMBIENT_SOUND	(*g_engfuncs.pfnEmitAmbientSound)
#define GET_MODEL_PTR	(*g_engfuncs.pfnGetModelPtr)
#define REG_USER_MSG	(*g_engfuncs.pfnRegUserMsg)
#define GET_BONE_POSITION	(*g_engfuncs.pfnGetBonePosition)
#define FUNCTION_FROM_NAME	(*g_engfuncs.pfnFunctionFromName)
#define NAME_FOR_FUNCTION	(*g_engfuncs.pfnNameForFunction)
#define TRACE_TEXTURE	(*g_engfuncs.pfnTraceTexture)
#define CMD_ARGS		(*g_engfuncs.pfnCmd_Args)
#define CMD_ARGC		(*g_engfuncs.pfnCmd_Argc)
#define CMD_ARGV		(*g_engfuncs.pfnCmd_Argv)
#define GET_ATTACHMENT	(*g_engfuncs.pfnGetAttachment)
#define SET_VIEW		(*g_engfuncs.pfnSetView)
#define SET_CROSSHAIRANGLE	(*g_engfuncs.pfnCrosshairAngle)
#define SET_SKYBOX		(*g_engfuncs.pfnSetSkybox)
#define LOAD_FILE		(*g_engfuncs.pfnLoadFile)
#define FILE_EXISTS		(*g_engfuncs.pfnFileExists)
#define FREE_FILE		FREE
#define COMPARE_FILE_TIME	(*g_engfuncs.pfnCompareFileTime)
#define GET_GAME_DIR	(*g_engfuncs.pfnGetGameDir)
#define IS_MAP_VALID	(*g_engfuncs.pfnIsMapValid)
#define IS_DEDICATED_SERVER	(*g_engfuncs.pfnIsDedicatedServer)
#define HOST_ERROR		(*g_engfuncs.pfnHostError)

#endif		//ENGINECALLBACK_H