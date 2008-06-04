//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    cm_materials.c - server materials system
//=======================================================================

#include "cm_local.h"

bool CM_ParseMaterial( void )
{
	uint	num = cm.num_materials;

	if( num > MAX_MATERIALS - 1 )
	{
		Msg( "materials limit exceeded\n" );
		return false;
	}

	if(!Com_MatchToken( "materialgroup" ))
		return false; // not a material description

	Com_GetToken( false );
	if(Com_MatchToken( "default" ))
		num = 0; // write default material first

	// member material name	
	com.strncpy( cm.mat[num].name, com_token, MAX_STRING );

	// setup default values
	while(!Com_MatchToken( "}" ))
	{
		if(!Com_GetToken( true ))
			return false; // unfinished material description

		if( Com_MatchToken( "elasticity" ))
		{
			Com_GetToken( false );		
			cm.mat[num].elasticity = com.atof( com_token );
		}
		else if( Com_MatchToken( "softness" ))
		{
			Com_GetToken( false );		
			cm.mat[num].softness = com.atof( com_token );
		}
		else if( Com_MatchToken( "friction" ))
		{
			Com_GetToken( false );		
			cm.mat[num].friction_static = com.atof( com_token );

			if(Com_TryToken())
				cm.mat[num].friction_kinetic = com.atof( com_token );
			else cm.mat[num].friction_kinetic = cm.mat[num].friction_static; // same as static friction
		}
	}

	// set default values if needed
	if( cm.mat[num].softness == 0.0f ) cm.mat[num].softness = cm.mat[0].softness;
	if( cm.mat[num].elasticity == 0.0f ) cm.mat[num].elasticity = cm.mat[0].elasticity;
	if( cm.mat[num].friction_static == 0.0f ) cm.mat[num].friction_static = cm.mat[0].friction_static;
	if( cm.mat[num].friction_kinetic == 0.0f ) cm.mat[num].friction_kinetic = cm.mat[0].friction_kinetic;	

	cm.num_materials++;

	// material will be parsed sucessfully
	return true;
}

void CM_InitMaterials( void )
{
	bool	load = Com_LoadScript( "scripts/materials.txt", NULL, 0 );
	int	i, j;

	if( !load )
	{
		// nothing to parse
		MsgDev( D_WARN, "scripts/materials.txt not found!\n" );
		return;
	}
	cm.num_materials = 0;
	memset( cm.mat, 0, sizeof(material_info_t) * MAX_MATERIALS );

	while( Com_GetToken( true ))
		CM_ParseMaterial();

	// assume IDs are in order and we don't need to remember them
	for ( i = 1; i < cm.num_materials; i++ )
		NewtonMaterialCreateGroupID( gWorld );

	for ( i = 0; i < cm.num_materials; i++ )
	{
		for ( j = 0; j < cm.num_materials; j++ )
		{
			NewtonMaterialSetDefaultCollidable( gWorld, i, j, true );
			NewtonMaterialSetDefaultSoftness( gWorld, i, j, cm.mat[i].softness / 2.0f + cm.mat[j].softness / 2.0f );
			NewtonMaterialSetDefaultElasticity( gWorld, i, j, cm.mat[i].elasticity / 2.0f + cm.mat[j].elasticity / 2.0f );
			NewtonMaterialSetDefaultFriction( gWorld, i, j, cm.mat[i].friction_static / 2.0f + cm.mat[j].friction_static / 2.0f, cm.mat[i].friction_kinetic / 2.0f + cm.mat[j].friction_kinetic / 2.0f );
			NewtonMaterialSetCollisionCallback( gWorld, i, j, NULL, Callback_ContactBegin, Callback_ContactProcess, Callback_ContactEnd );
		}
	}
	Msg( "num materials %d\n", cm.num_materials );
}