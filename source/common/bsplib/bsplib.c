//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	bsplib.c - bsp level creator
//=======================================================================

#include "bsplib.h"

char path[MAX_SYSPATH];
bool full_compile = false;
bool onlyvis = false;
bool onlyrad = false;

// qbps settings
bool notjunc = false;
bool onlyents = false;
bool nosubdivide = false;

cvar_t	*bsp_lmsample_size;
cvar_t	*bsp_lightmap_size;

dll_info_t physic_dll = { "physic.dll", NULL, "CreateAPI", NULL, NULL, false, sizeof(physic_exp_t) };
physic_exp_t *pe;

static void AddCollision( void* handle, const void* buffer, size_t size )
{
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
	else memset( &pe, 0, sizeof(pe));
}

void Free_PhysicLibrary( void )
{
	if( physic_dll.link )
	{
		pe->Shutdown();
		memset( &pe, 0, sizeof(pe));
	}
	Sys_FreeLibrary( &physic_dll );
}

bool PrepareBSPModel ( const char *dir, const char *name, byte params )
{
	int	numshaders;
	
	if( dir ) com.strncpy(gs_basedir, dir, sizeof(gs_basedir));
	if( name ) com.strncpy(gs_filename, name, sizeof(gs_filename));

	// copy state
	onlyents = (params & BSP_ONLYENTS) ? true : false;
	onlyvis = (params & BSP_ONLYVIS) ? true : false ;
	onlyrad = (params & BSP_ONLYRAD) ? true : false;
	full_compile = (params & BSP_FULLCOMPILE) ? true : false;

	// register our cvars
	bsp_lmsample_size = Cvar_Get( "bsplib_lmsample_size", "16", CVAR_SYSTEMINFO, "bsplib lightmap sample size" );
	bsp_lightmap_size = Cvar_Get( "bsplib_lightmap_size", "128", CVAR_SYSTEMINFO, "bsplib lightmap size" );

	FS_LoadGameInfo( "gameinfo.txt" ); // same as normal gamemode

	Init_PhysicsLibrary();
	BeginMapShaderFile();
	SetDefaultSampleSize( bsp_lmsample_size->integer );

	numshaders = LoadShaderInfo();
	Msg( "%5i shaderInfo\n", numshaders );

	return true;
}

bool CompileBSPModel ( void )
{
	if( onlyents ) WbspMain( true ); // must be first!
	else if( onlyvis && !onlyrad ) WvisMain ( full_compile );
	else if( onlyrad && !onlyvis ) WradMain( full_compile );
	else if( onlyrad && onlyvis )
	{
		WbspMain( false );
		WvisMain( full_compile );
		WradMain( full_compile );
	}
          else
          {
		// delete portal and line files
		com.sprintf( path, "%s/maps/%s.prt", com.GameInfo->gamedir, gs_filename );
		remove( path );
		com.sprintf( path, "%s/maps/%s.lin", com.GameInfo->gamedir, gs_filename );
		remove( path );
		WbspMain( false ); // just create bsp
	}
	Free_PhysicLibrary();

	if( onlyrad && onlyvis && full_compile )
	{
		// delete all temporary files after final compile
		com.sprintf(path, "%s/maps/%s.prt", com.GameInfo->gamedir, gs_filename);
		remove(path);
		com.sprintf(path, "%s/maps/%s.lin", com.GameInfo->gamedir, gs_filename);
		remove(path);
		com.sprintf(path, "%s/maps/%s.log", com.GameInfo->gamedir, gs_filename);
		remove(path);
	}

	return true;
}