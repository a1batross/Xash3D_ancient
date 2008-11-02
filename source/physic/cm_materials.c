//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    cm_materials.c - server materials system
//=======================================================================

#include "cm_local.h"

static script_t	*materials = NULL;

bool CM_ParseMaterial( token_t *token )
{
	uint	num = cm.num_materials;

	if( num > MAX_MATERIALS - 1 )
	{
		Msg( "materials limit exceeded\n" );
		return false;
	}

	if( com.stricmp( token->string, "materialgroup" ))
		return false; // not a material description

	Com_ReadToken( materials, 0, token );
	if( !com.stricmp( token->string, "default" ))
		num = 0; // write default material first

	// member material name	
	com.strncpy( cm.mat[num].name, token->string, MAX_STRING );

	// setup default values
	while( com.stricmp( token->string, "}" ))
	{
		if(!Com_ReadToken( materials, SC_ALLOW_NEWLINES, token ))
			return false; // unfinished material description

		if( !com.stricmp( token->string, "elasticity" ))
		{
			Com_ReadFloat( materials, false, &cm.mat[num].elasticity );
		}
		else if( !com.stricmp( token->string, "softness" ))
		{
			Com_ReadFloat( materials, false, &cm.mat[num].softness );
		}
		else if( !com.stricmp( token->string, "friction" ))
		{
			Com_ReadFloat( materials, false, &cm.mat[num].friction_static );

			if(!Com_ReadFloat( materials, false, &cm.mat[num].friction_kinetic ))
				cm.mat[num].friction_kinetic = cm.mat[num].friction_static; // same as static friction
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
	int	i, j;
	token_t	token;

	materials = Com_OpenScript( "scripts/materials.txt", NULL, 0 );

	if( !materials )
	{
		// nothing to parse
		MsgDev( D_WARN, "scripts/materials.txt not found!\n" );
		return;
	}
	cm.num_materials = 0;
	Mem_Set( cm.mat, 0, sizeof(material_info_t) * MAX_MATERIALS );

	while( Com_ReadToken( materials, SC_ALLOW_NEWLINES, &token ))
		CM_ParseMaterial( &token );

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