//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_main.c - collision interface
//=======================================================================

#include "cm_local.h"

physic_imp_t	pi;
stdlib_api_t	com;

cvar_t		*cm_noareas;
cvar_t		*cm_nomeshes;
cvar_t		*cm_nocurves;
cvar_t		*cm_debugsize;

bool CM_InitPhysics( void )
{
	cms.mempool = Mem_AllocPool( "CM Zone" );
	Mem_Set( cms.nullrow, 0xFF, MAX_MAP_LEAFS / 8 );

	cm_noareas = Cvar_Get( "cm_noareas", "0", 0, "ignore clipmap areas" );
	cm_nomeshes = Cvar_Get( "cm_nomeshes", "1", CVAR_ARCHIVE, "make meshes uncollidable" );	// q3a compatible
	cm_nocurves = Cvar_Get( "cm_nocurves", "0", CVAR_ARCHIVE, "make curves uncollidable" );
	cm_debugsize = Cvar_Get( "cm_debugsize", "2", 0, "adjust the debug lines scale" );
	sv_models[0] = NULL; // 0 modelindex isn't used

	return true;
}

void CM_PhysFrame( float frametime )
{
}

void CM_FreePhysics( void )
{
	CM_FreeWorld();
	CM_FreeModels();
	Mem_FreePool( &cms.mempool );
}

physic_exp_t DLLEXPORT *CreateAPI ( stdlib_api_t *input, physic_imp_t *engfuncs )
{
	static physic_exp_t		Phys;

	com = *input;

	// Sys_LoadLibrary can create fake instance, to check
	// api version and api size, but second argument will be 0
	// and always make exception, run simply check for avoid it
	if(engfuncs) pi = *engfuncs;

	// generic functions
	Phys.api_size = sizeof( physic_exp_t );
	Phys.com_size = sizeof( stdlib_api_t );
	
	Phys.Init = CM_InitPhysics;
	Phys.Shutdown = CM_FreePhysics;

	Phys.DrawCollision = CM_DrawCollision;
	Phys.Frame = CM_PhysFrame;

	Phys.BeginRegistration = CM_BeginRegistration;
	Phys.RegisterModel = CM_RegisterModel;
	Phys.EndRegistration = CM_EndRegistration;

	Phys.SetAreaPortals = CM_SetAreaPortals;
	Phys.GetAreaPortals = CM_GetAreaPortals;
	Phys.SetAreaPortalState = CM_SetAreaPortalState;
	Phys.BoxLeafnums = CM_BoxLeafnums;
	Phys.WriteAreaBits = CM_WriteAreaBits;
	Phys.AreasConnected = CM_AreasConnected;
	Phys.ClusterPVS = CM_ClusterPVS;
	Phys.ClusterPHS = CM_ClusterPHS;
	Phys.LeafCluster = CM_LeafCluster;
	Phys.PointLeafnum = CM_PointLeafnum;
	Phys.LeafArea = CM_LeafArea;

	Phys.NumShaders = CM_NumShaders;
	Phys.NumBmodels = CM_NumInlineModels;
	Phys.Mod_GetType = CM_ModelType;
	Phys.Mod_GetBounds = CM_ModelBounds;
	Phys.Mod_GetFrames = CM_ModelFrames;
	Phys.GetShaderName = CM_ShaderName;
	Phys.Mod_Extradata = CM_Extradata;
	Phys.GetEntityScript = CM_EntityScript;
	Phys.VisData = CM_VisData;

	Phys.PointContents1 = CM_PointContents;
	Phys.PointContents2 = CM_TransformedPointContents;
	Phys.BoxTrace1 = CM_BoxTrace;
	Phys.BoxTrace2 = CM_TransformedBoxTrace;

	Phys.TempModel = CM_TempBoxModel;

	// needs to be removed
	Phys.FatPVS = CM_FatPVS;
	Phys.FatPHS = CM_FatPHS;

	return &Phys;
}