//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    cm_materials.c - server materials system
//=======================================================================

#include "cm_local.h"

static script_t	*materials = NULL;

bool CM_ParseMaterial( token_t *token )
{
	uint	i, num = cms.num_materials;
	string	sndmask;

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
	com.strncpy( cms.mat[num].name, token->string, MAX_STRING );

	// setup default values
	while( com.stricmp( token->string, "}" ))
	{
		if(!Com_ReadToken( materials, SC_ALLOW_NEWLINES, token ))
			return false; // unfinished material description

		if( !com.stricmp( token->string, "elasticity" ))
		{
			Com_ReadFloat( materials, false, &cms.mat[num].elasticity );
		}
		else if( !com.stricmp( token->string, "softness" ))
		{
			Com_ReadFloat( materials, false, &cms.mat[num].softness );
		}
		else if( !com.stricmp( token->string, "friction" ))
		{
			Com_ReadFloat( materials, false, &cms.mat[num].friction_static );

			if(!Com_ReadFloat( materials, false, &cms.mat[num].friction_kinetic ))
				cms.mat[num].friction_kinetic = cms.mat[num].friction_static; // same as static friction
		}
		else if( !com.stricmp( token->string, "bust_sndmask" ))
		{
			string	bustsnd;

			if( !Com_ReadString( materials, false, sndmask ))
				continue;

			for( i = 0; i < MAX_MAT_SOUNDS; i++ )
			{
				com.snprintf( bustsnd, MAX_STRING, "materials/%s%i.wav", sndmask );
				if( FS_FileExists( va( "sound/%s", bustsnd )))
					com.strncpy( cms.mat[num].bust_sounds[cms.mat[num].num_bustsounds++], bustsnd, MAX_STRING );
			}
		}
		else if( !com.stricmp( token->string, "push_sndmask" ))
		{
			string	pushsnd;

			if( !Com_ReadString( materials, false, sndmask ))
				continue;

			for( i = 0; i < MAX_MAT_SOUNDS; i++ )
			{
				com.snprintf( pushsnd, MAX_STRING, "materials/%s%i.wav", sndmask );
				if( FS_FileExists( va( "sound/%s", pushsnd )))
					com.strncpy( cms.mat[num].push_sounds[cms.mat[num].num_pushsounds++], pushsnd, MAX_STRING );
			}
		}
		else if( !com.stricmp( token->string, "impact_sndmask" ))
		{
			string	impactsnd;

			if( !Com_ReadString( materials, false, sndmask ))
				continue;

			for( i = 0; i < MAX_MAT_SOUNDS; i++ )
			{
				com.snprintf( impactsnd, MAX_STRING, "materials/%s%i.wav", sndmask );
				if( FS_FileExists( va( "sound/%s", impactsnd )))
					com.strncpy( cms.mat[num].impact_sounds[cms.mat[num].num_impactsounds++], impactsnd, MAX_STRING );
			}
		}
	}

	// set default values if needed
	if( cms.mat[num].softness == 0.0f ) cms.mat[num].softness = cms.mat[0].softness;
	if( cms.mat[num].elasticity == 0.0f ) cms.mat[num].elasticity = cms.mat[0].elasticity;
	if( cms.mat[num].friction_static == 0.0f ) cms.mat[num].friction_static = cms.mat[0].friction_static;
	if( cms.mat[num].friction_kinetic == 0.0f ) cms.mat[num].friction_kinetic = cms.mat[0].friction_kinetic;	

	cms.num_materials++;

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
	cms.num_materials = 0;
	Mem_Set( cms.mat, 0, sizeof(material_info_t) * MAX_MATERIALS );

	while( Com_ReadToken( materials, SC_ALLOW_NEWLINES, &token ))
		CM_ParseMaterial( &token );

	// assume IDs are in order and we don't need to remember them
	for ( i = 1; i < cms.num_materials; i++ )
		NewtonMaterialCreateGroupID( gWorld );

	for ( i = 0; i < cms.num_materials; i++ )
	{
		for ( j = 0; j < cms.num_materials; j++ )
		{
			NewtonMaterialSetDefaultCollidable( gWorld, i, j, true );
			NewtonMaterialSetDefaultSoftness( gWorld, i, j, cms.mat[i].softness / 2.0f + cms.mat[j].softness / 2.0f );
			NewtonMaterialSetDefaultElasticity( gWorld, i, j, cms.mat[i].elasticity / 2.0f + cms.mat[j].elasticity / 2.0f );
			NewtonMaterialSetDefaultFriction( gWorld, i, j, cms.mat[i].friction_static / 2.0f + cms.mat[j].friction_static / 2.0f, cms.mat[i].friction_kinetic / 2.0f + cms.mat[j].friction_kinetic / 2.0f );
			NewtonMaterialSetCollisionCallback( gWorld, i, j, NULL, Callback_ContactBegin, Callback_ContactProcess, Callback_ContactEnd );
		}
	}
	Msg( "num materials %d\n", cms.num_materials );
}