//=======================================================================
//			Copyright XashXT Group 2009 ©
//		com_export.h - safe calls exports from other libraries
//=======================================================================
#ifndef COM_EXPORT_H
#define COM_EXPORT_H

// linked interfaces
extern stdlib_api_t		com;
extern physic_exp_t		*pe;
extern vsound_exp_t		*se;

//
// physic.dll exports
//
#define CM_RegisterModel		if( pe ) pe->RegisterModel
#define CM_SetAreaPortals		if( pe ) pe->SetAreaPortals
#define CM_GetAreaPortals		if( pe ) pe->GetAreaPortals
#define CM_SetAreaPortalState		if( pe ) pe->SetAreaPortalState
#define CM_GetAttachment		if( pe ) pe->Mod_GetAttachment
#define CM_GetBonePosition		if( pe ) pe->Mod_GetBonePos
#define CM_EndRegistration		if( pe ) pe->EndRegistration
#define CM_Frame			if( pe ) pe->Frame

_inline void CM_BeginRegistration( const char *name, bool clientload, uint *checksum )
{
	if( pe ) pe->BeginRegistration( name, clientload, checksum );
	else if( checksum ) *checksum = 0xBAD;
}

_inline void Mod_GetBounds( model_t handle, vec3_t mins, vec3_t maxs )
{
	if( pe ) pe->Mod_GetBounds( handle, mins, maxs );
	else mins[0] = mins[1] = mins[2] = maxs[0] = maxs[1] = maxs[2] = 0.0f;
}

_inline void Mod_GetFrames( model_t handle, int *numFrames )
{
	if( pe ) pe->Mod_GetFrames( handle, numFrames );
	else if( numFrames ) *numFrames = 0;
}

_inline int CM_NumShaders( void )
{
	if( !pe ) return 0;
	return pe->NumShaders();
}

_inline int CM_NumBmodels( void )
{
	if( !pe ) return 1;	// world
	return pe->NumBmodels();
}

_inline int CM_GetModelType( model_t handle )
{
	if( !pe ) return mod_bad;
	return pe->Mod_GetType( handle );
}

_inline const char *CM_GetShaderName( int index )
{
	if( !pe ) return "";
	return pe->GetShaderName( index );
}

_inline const void *CM_VisData( void )
{
	if( !pe ) return NULL;
	return pe->VisData();
}

_inline bool CM_AreasConnected( int area, int otherarea )
{
	if( !pe ) return true; // enable full visibility
	return pe->AreasConnected( area, otherarea );
}

_inline int CM_LeafArea( int leafnum )
{
	if( !pe )	return -1;
	return pe->LeafArea( leafnum );
}		

_inline int CM_LeafCluster( int leafnum )
{
	if( !pe )	return -1;
	return pe->LeafCluster( leafnum );
}

_inline int CM_PointLeafnum( const vec3_t origin )
{
	if( !pe )	return -1;
	return pe->PointLeafnum( origin );
}

_inline byte *CM_ClusterPVS( int cluster )
{
	if( !pe )	return NULL;
	return pe->ClusterPVS( cluster );
}

_inline byte *CM_ClusterPHS( int cluster )
{
	if( !pe )	return NULL;
	return pe->ClusterPHS( cluster );
}

_inline byte *CM_FatPVS( const vec3_t org, bool merge )
{
	if( !pe )	return NULL;
	return pe->FatPVS( org, merge );
}

_inline byte *CM_FatPHS( int cluster, bool merge )
{
	if( !pe )	return NULL;
	return pe->FatPHS( cluster, merge );
}

_inline int CM_WriteAreaBits( byte *buffer, int area, bool merge )
{
	if( !pe )	return 0;
	return pe->WriteAreaBits( buffer, area, merge );
}

_inline int CM_BoxLeafnums( vec3_t mins, vec3_t maxs, int *list, int listsize, int *lastleaf )
{
	if( !pe )
	{
		if( lastleaf ) *lastleaf = 0;
		return 0;
	}
	return pe->BoxLeafnums( mins, maxs, list, listsize, lastleaf );
}

_inline int CM_PointContents( const vec3_t p, model_t model )
{
	if( pe ) return pe->PointContents1( p, model );
	if( model == 1 ) return BASECONT_SOLID;
	return BASECONT_NONE;
}

_inline int CM_TransformedPointContents( const vec3_t p, model_t model, const vec3_t org, const vec3_t ang )
{
	if( pe ) return pe->PointContents2( p, model, org, ang );
	if( model == 1 ) return BASECONT_SOLID;
	return BASECONT_NONE;
}

_inline void CM_BoxTrace( trace_t *tr, const vec3_t p1, const vec3_t p2, vec3_t mins, vec3_t maxs, model_t model, int mask, trType_t type )
{
	if( pe ) pe->BoxTrace1( tr, p1, p2, mins, maxs, model, mask, type );
	else
	{
		Mem_Set( tr, 0, sizeof( *tr ));
		tr->fAllSolid = tr->fStartSolid = true; 
	}
}

_inline void CM_TransformedBoxTrace( trace_t *tr, const vec3_t p1, const vec3_t p2, vec3_t mins, vec3_t maxs, model_t model, int mask, const vec3_t org, const vec3_t ang, trType_t type )
{
	if( pe ) pe->BoxTrace2( tr, p1, p2, mins, maxs, model, mask, org, ang, type );
	else CM_BoxTrace( tr, p1, p2, mins, maxs, model, mask, type );
}

_inline bool CM_HitboxTrace( trace_t *tr, edict_t *e, const vec3_t p1, const vec3_t p2 )
{
	if( pe ) return pe->HitboxTrace( tr, e, p1, p2 );
	else tr->iHitgroup = -1;
	return false;
}

_inline model_t CM_TempModel( const vec3_t mins, const vec3_t maxs, bool capsule )
{
	if( !pe ) return 1; // world
	return pe->TempModel( mins, maxs, capsule );
}

_inline void *Mod_Extradata( model_t modelIndex )
{
	if( !pe )	return NULL;
	return pe->Mod_Extradata( modelIndex );
}

//
// vsound.dll exports
//
#define S_Shutdown			if( se ) se->Shutdown
#define S_StartStreaming		if( se ) se->StartStreaming
#define S_StartSound		if( se ) se->StartSound
#define S_StartLocalSound		if( se ) se->StartLocalSound
#define S_StartBackgroundTrack	if( se ) se->StartBackgroundTrack
#define S_StopBackgroundTrack		if( se ) se->StopBackgroundTrack
#define S_RawSamples 		if( se ) se->StreamRawSamples
#define S_FadeClientVolume		if( se ) se->FadeClientVolume
#define S_StopAllSounds		if( se ) se->StopAllSounds
#define S_AddLoopingSound		if( se ) se->AddLoopingSound
#define S_Activate			if( se ) se->Activate
#define S_Update			if( se ) se->RenderFrame
#define S_BeginRegistration		if( se ) se->BeginRegistration
#define S_EndRegistration		if( se ) se->EndRegistration

_inline sound_t S_RegisterSound( const char *name )
{
	if( !se ) return 0;
	return se->RegisterSound( name );
}

#endif//COM_EXPORT_H