//=======================================================================
//			Copyright XashXT Group 2007 ©
//			physic.c - physics engine wrapper
//=======================================================================

#include "physic.h"

physic_imp_t	pi;
stdlib_api_t	com;
physic_t		ph;	// physic globalstate
byte		*physpool;
byte		*cmappool;
NewtonWorld	*gWorld;

cvar_t		*cm_use_triangles;
cvar_t		*cm_solver_model;
cvar_t		*cm_friction_model;

bool InitPhysics( void )
{
	physpool = Mem_AllocPool("Physics Pool");
	cmappool = Mem_AllocPool("CM Zone");
	gWorld = NewtonCreate (Palloc, Pfree); // alloc world

	cm_use_triangles = Cvar_Get("cm_convert_polygons", "1", CVAR_INIT|CVAR_SYSTEMINFO );//, "convert bsp polygons to triangles, slowly but more safety way" );
	cm_solver_model = Cvar_Get("cm_solver", "0", CVAR_ARCHIVE );//, "change solver model: 0 - precision, 1 - adaptive, 2 - fast. (changes need restart server to take effect)" );
	cm_friction_model = Cvar_Get("cm_friction", "0", CVAR_ARCHIVE );//, "change solver model: 0 - precision, 1 - adaptive. (changes need restart server to take effect)" );

	return true;
}

void PhysFrame( float time )
{
	NewtonUpdate( gWorld, time );
}

void FreePhysics( void )
{
	NewtonDestroy( gWorld );
	Mem_FreePool( &physpool );
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
	Phys.api_size = sizeof(physic_exp_t);

	Phys.Init = InitPhysics;
	Phys.Shutdown = FreePhysics;
	Phys.WriteCollisionLump = CM_SaveCollisionTree;
	Phys.DrawCollision = DebugShowCollision;
	Phys.LoadBSP = CM_LoadBSP;
	Phys.FreeBSP = CM_FreeBSP;

	Phys.BeginRegistration = CM_BeginRegistration;
	Phys.RegisterModel = CM_RegisterModel;
	Phys.EndRegistration = CM_EndRegistration;

	Phys.SetAreaPortals = CM_SetAreaPortals;
	Phys.GetAreaPortals = CM_GetAreaPortals;
	Phys.SetAreaPortalState = CM_SetAreaPortalState;

	Phys.NumClusters = CM_NumClusters;
	Phys.NumTextures = CM_NumTexinfo;
	Phys.NumBmodels = CM_NumInlineModels;
	Phys.GetEntityString = CM_EntityString;
	Phys.GetTextureName = CM_TexName;
	Phys.HeadnodeForBox = CM_HeadnodeForBox;
	Phys.PointContents = CM_PointContents;
	Phys.TransformedPointContents = CM_TransformedPointContents;
	Phys.BoxTrace = CM_BoxTrace;
	Phys.TransformedBoxTrace = CM_TransformedBoxTrace;
	
	Phys.ClusterPVS = CM_ClusterPVS;
	Phys.ClusterPHS = CM_ClusterPHS;
	Phys.PointLeafnum = CM_PointLeafnum;
	Phys.BoxLeafnums = CM_BoxLeafnums;
	Phys.LeafContents = CM_LeafContents;
	Phys.LeafCluster = CM_LeafCluster;
	Phys.LeafArea = CM_LeafArea;
	Phys.AreasConnected = CM_AreasConnected;
	Phys.WriteAreaBits = CM_WriteAreaBits;
	Phys.HeadnodeVisible = CM_HeadnodeVisible;

	Phys.Frame = PhysFrame;
	Phys.CreateBody = Phys_CreateBody;
	Phys.GetForce = Phys_GetForce;
	Phys.SetForce = Phys_SetForce;
	Phys.GetMassCentre = Phys_GetMassCentre;
	Phys.SetMassCentre = Phys_SetMassCentre;
	Phys.RemoveBody = Phys_RemoveBody;

	return &Phys;
}