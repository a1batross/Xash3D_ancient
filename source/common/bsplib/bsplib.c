//=======================================================================
//			Copyright XashXT Group 2008 ©
//		    	bsplib.c - bsp level creator
//=======================================================================

#include "bsplib.h"

byte	*checkermate_dds;
size_t	checkermate_dds_size;
char	path[MAX_SYSPATH];
uint	bsp_parms;

dll_info_t physic_dll = { "physic.dll", NULL, "CreateAPI", NULL, NULL, false, sizeof(physic_exp_t) };
physic_exp_t *pe;

static void AddCollision( void* handle, const void* buffer, size_t size )
{
	if((dcollisiondatasize + size) > MAX_MAP_COLLISION )
		Sys_Error( "MAX_MAP_COLLISION limit exceeded\n" );
	Mem_Copy( dcollision + dcollisiondatasize, (void *)buffer, size );
	dcollisiondatasize += size;
}

void ProcessCollisionTree( void )
{
	if( !physic_dll.link ) return;

	dcollisiondatasize = 0;
	pe->WriteCollisionLump( NULL, AddCollision );
}

void Init_PhysicsLibrary( void )
{
	static physic_imp_t		pi;
	launch_t			CreatePhysic;

	pi.api_size = sizeof(physic_imp_t);
	Sys_LoadLibrary( &physic_dll );

	if( physic_dll.link )
	{
		CreatePhysic = (void *)physic_dll.main;
		pe = CreatePhysic( &com, &pi ); // sys_error not overrided
		pe->Init(); // initialize phys callback
	}
	else Mem_Set( &pe, 0, sizeof( pe ));
}

void Free_PhysicLibrary( void )
{
	if( physic_dll.link )
	{
		pe->Shutdown();
		Mem_Set( &pe, 0, sizeof( pe ));
	}
	Sys_FreeLibrary( &physic_dll );
}

bool PrepareBSPModel( const char *dir, const char *name )
{
	int	numshaders;

	bsp_parms = 0;

	// get global parms
	if( FS_CheckParm( "-vis" )) bsp_parms |= BSPLIB_MAKEVIS;
	if( FS_CheckParm( "-qrad" )) bsp_parms |= BSPLIB_MAKEQ2RAD;
	if( FS_CheckParm( "-hlrad" )) bsp_parms |= BSPLIB_MAKEHLRAD;
	if( FS_CheckParm( "-full" )) bsp_parms |= BSPLIB_FULLCOMPILE;
	if( FS_CheckParm( "-onlyents" )) bsp_parms |= BSPLIB_ONLYENTS;

	// famous q1 "notexture" image: purple-black checkerboard
	checkermate_dds = FS_LoadInternal( "checkerboard.dds", &checkermate_dds_size );
	Image_Init( NULL, IL_ALLOW_OVERWRITE|IL_IGNORE_MIPS );

	// merge parms
	if( bsp_parms & BSPLIB_ONLYENTS )
	{
		bsp_parms = (BSPLIB_ONLYENTS|BSPLIB_MAKEBSP);
	}
	else
	{
		if( bsp_parms & BSPLIB_MAKEQ2RAD && bsp_parms & BSPLIB_MAKEHLRAD )
		{
			MsgDev( D_WARN, "both type 'hlrad' and 'qrad' specified\ndefaulting to 'qrad'\n" );
			bsp_parms &= ~BSPLIB_MAKEHLRAD;
		}
		if( bsp_parms & BSPLIB_FULLCOMPILE )
		{
			if((bsp_parms & BSPLIB_MAKEVIS) && (bsp_parms & (BSPLIB_MAKEQ2RAD|BSPLIB_MAKEHLRAD)))
			{
				bsp_parms |= BSPLIB_MAKEBSP;	// rebuild bsp file for final compile
				bsp_parms |= BSPLIB_DELETE_TEMP;
			}
		}
		if(!(bsp_parms & (BSPLIB_MAKEVIS|BSPLIB_MAKEQ2RAD|BSPLIB_MAKEHLRAD)))
		{
			// -vis -light or -rad not specified, just create a .bsp
			bsp_parms |= BSPLIB_MAKEBSP;
		}
	}

	FS_LoadGameInfo( "gameinfo.txt" ); // same as normal gamemode
	Init_PhysicsLibrary();
	numshaders = LoadShaderInfo();
	Msg( "%5i shaderInfo\n", numshaders );

	return true;
}

bool CompileBSPModel ( void )
{
	// now run specified utils
	if( bsp_parms & BSPLIB_MAKEBSP )
		WbspMain();

	if( bsp_parms & BSPLIB_MAKEVIS )
		WvisMain();

	if( bsp_parms & (BSPLIB_MAKEQ2RAD|BSPLIB_MAKEHLRAD))
		WradMain();

	Free_PhysicLibrary();
	PrintBSPFileSizes();

	if( bsp_parms & BSPLIB_DELETE_TEMP )
	{
		// delete all temporary files after final compile
		com.sprintf( path, "%s/maps/%s.prt", com.GameInfo->gamedir, gs_filename );
		FS_Delete( path );
		com.sprintf( path, "%s/maps/%s.lin", com.GameInfo->gamedir, gs_filename );
		FS_Delete( path );
		com.sprintf( path, "%s/maps/%s.log", com.GameInfo->gamedir, gs_filename );
		FS_Delete( path );
	}
	return true;
}