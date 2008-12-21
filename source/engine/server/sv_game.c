//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        sv_game.c - server dlls interaction
//=======================================================================

#include "common.h"
#include "server.h"
#include "byteorder.h"
#include "matrix_lib.h"
#include "com_library.h"
#include "const.h"

#define EOFS( x )	(int)&(((entvars_t *)0)->x)

void Sys_FsGetString( file_t *f, char *str )
{
	char	ch;

	while( (ch = FS_Getc( f )) != EOF )
	{
		*str++ = ch;
		if( !ch ) break;
	}
}

void Sys_FreeNameFuncGlobals( void )
{
	int	i;

	if( game.ordinals ) Mem_Free( game.ordinals );
	if( game.funcs) Mem_Free( game.funcs);

	for( i = 0; i < game.num_ordinals; i++ )
	{
		if( game.names[i] )
			Mem_Free( game.names[i] );
	}

	game.num_ordinals = 0;
	game.ordinals = NULL;
	game.funcs = NULL;
}

char *Sys_GetMSVCName( const char *in_name )
{
	char	*pos, *out_name;

	if( in_name[0] == '?' )  // is this a MSVC C++ mangled name?
	{
		if(( pos = com.strstr( in_name, "@@" )) != NULL )
		{
			int	len = pos - in_name;

			// strip off the leading '?'
			out_name = com.stralloc( svs.private, in_name + 1, __FILE__, __LINE__ );
			out_name[len-1] = 0; // terminate string at the "@@"
			return out_name;
		}
	}
	return com.stralloc( svs.private, in_name, __FILE__, __LINE__ );
}

bool Sys_LoadSymbols( const char *filename )
{
	file_t		*f;
	DOS_HEADER	dos_header;
	LONG		nt_signature;
	PE_HEADER		pe_header;
	SECTION_HEADER	section_header;
	bool		edata_found;
	OPTIONAL_HEADER	optional_header;
	long		edata_offset;
	long		edata_delta;
	EXPORT_DIRECTORY	export_directory;
	long		name_offset;
	long		ordinal_offset;
	long		function_offset;
	string		function_name;
	dword		*p_Names = NULL;
	int		i, index;

	for( i = 0; i < game.num_ordinals; i++ )
		game.names[i] = NULL;

	f = FS_Open( filename, "rb" );
	if( !f )
	{
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s not found\n", filename );
		return false;
	}

	if( FS_Read( f, &dos_header, sizeof(dos_header)) != sizeof( dos_header ))
	{
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s has corrupted EXE header\n", filename );
		FS_Close( f );
		return false;
	}

	if( dos_header.e_magic != DOS_SIGNATURE )
	{
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s does not have a valid dll signature\n", filename );
		FS_Close( f );
		return false;
	}

	if( FS_Seek( f, dos_header.e_lfanew, SEEK_SET ) == -1 )
	{
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s error seeking to new exe header\n", filename );
		FS_Close( f );
		return false;
	}

	if( FS_Read( f, &nt_signature, sizeof( nt_signature )) != sizeof( nt_signature ))
	{
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s has corrupted NT header\n", filename );
		FS_Close( f );
		return false;
	}

	if( nt_signature != NT_SIGNATURE )
	{
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s does not have a valid NT signature\n", filename );
		FS_Close( f );
		return false;
	}

	if( FS_Read( f, &pe_header, sizeof( pe_header )) != sizeof( pe_header ))
	{
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s does not have a valid PE header\n", filename );
		FS_Close( f );
		return false;
	}

	if( !pe_header.SizeOfOptionalHeader )
	{
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s does not have an optional header\n", filename );
		FS_Close( f );
		return false;
	}

	if( FS_Read( f, &optional_header, sizeof( optional_header )) != sizeof( optional_header ))
	{
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s optional header probably corrupted\n", filename );
		FS_Close( f );
		return false;
	}

	edata_found = false;

	for( i = 0; i < pe_header.NumberOfSections; i++ )
	{

		if( FS_Read( f, &section_header, sizeof( section_header )) != sizeof( section_header ))
		{
			MsgDev( D_ERROR, "Sys_LoadSymbols: %s error during reading section header\n", filename );
			FS_Close( f );
			return false;
		}

		if( !com.strcmp((char *)section_header.Name, ".edata" ))
		{
			edata_found = TRUE;
			break;
		}
	}

	if( edata_found )
	{
		edata_offset = section_header.PointerToRawData;
		edata_delta = section_header.VirtualAddress - section_header.PointerToRawData; 
	}
	else
	{
		edata_offset = optional_header.DataDirectory[0].VirtualAddress;
		edata_delta = 0L;
	}

	if( FS_Seek( f, edata_offset, SEEK_SET ) == -1 )
	{
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s does not have a valid exports section\n", filename );
		FS_Close( f );
		return false;
	}

	if( FS_Read( f, &export_directory, sizeof( export_directory )) != sizeof( export_directory ))
	{
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s does not have a valid optional header\n", filename );
		FS_Close( f );
		return false;
	}

	game.num_ordinals = export_directory.NumberOfNames;	// also number of ordinals

	if( game.num_ordinals > MAX_SYSPATH )
	{
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s too many exports\n", filename );
		FS_Close( f );
		return false;
	}

	ordinal_offset = export_directory.AddressOfNameOrdinals - edata_delta;

	if( FS_Seek( f, ordinal_offset, SEEK_SET ) == -1 )
	{
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s does not have a valid ordinals section\n", filename );
		FS_Close( f );
		return false;
	}

	game.ordinals = Z_Malloc( game.num_ordinals * sizeof( word ));

	if( FS_Read( f, game.ordinals, game.num_ordinals * sizeof( word )) != (game.num_ordinals * sizeof( word )))
	{
		Sys_FreeNameFuncGlobals();
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s error during reading ordinals table\n", filename );
		FS_Close( f );
		return false;
	}

	function_offset = export_directory.AddressOfFunctions - edata_delta;
	if( FS_Seek( f, function_offset, SEEK_SET ) == -1 )
	{
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s does not have a valid export address section\n", filename );
		FS_Close( f );
		return false;
	}

	game.funcs = Z_Malloc( game.num_ordinals * sizeof( dword ));

	if( FS_Read( f, game.funcs, game.num_ordinals * sizeof( dword )) != (game.num_ordinals * sizeof( dword )))
	{
		Sys_FreeNameFuncGlobals();
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s error during reading export address section\n", filename );
		FS_Close( f );
		return false;
	}

	name_offset = export_directory.AddressOfNames - edata_delta;

	if( FS_Seek( f, name_offset, SEEK_SET ) == -1 )
	{
		Sys_FreeNameFuncGlobals();
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s file does not have a valid names section\n", filename );
		FS_Close( f );
		return false;
	}

	p_Names = Z_Malloc( game.num_ordinals * sizeof( dword ));

	if( FS_Read( f, p_Names, game.num_ordinals * sizeof( dword )) != (game.num_ordinals * sizeof( dword )))
	{
		Sys_FreeNameFuncGlobals();
		if( p_Names ) Mem_Free( p_Names );
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s error during reading names table\n", filename );
		FS_Close( f );
		return false;
	}

	for( i = 0; i < game.num_ordinals; i++ )
	{
		name_offset = p_Names[i] - edata_delta;

		if( name_offset != 0 )
		{
			if( FS_Seek( f, name_offset, SEEK_SET ) != -1 )
			{
				Sys_FsGetString( f, function_name );
				game.names[i] = Sys_GetMSVCName( function_name );
			}
			else break;
		}
	}

	if( i != game.num_ordinals )
	{
		Sys_FreeNameFuncGlobals();
		if( p_Names ) Mem_Free( p_Names );
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s error during loading names section\n", filename );
		FS_Close( f );
		return false;
	}
	FS_Close( f );

	for( i = 0; i < game.num_ordinals; i++ )
	{
		if( !com.strcmp( "GiveFnptrsToDll", game.names[i] ))
		{
			void	*fn_offset;

			index = game.ordinals[i];
			fn_offset = (void *)Com_GetProcAddress( svs.game, "GiveFnptrsToDll" );
			game.funcBase = (dword)(fn_offset) - game.funcs[index];
			break;
		}
	}
	if( p_Names ) Mem_Free( p_Names );
	return true;
}

/*
============
SV_CalcBBox

FIXME: get to work
============
*/
void SV_CalcBBox( edict_t *ent, const float *mins, const float *maxs )
{
	vec3_t		rmin, rmax;
	int		i, j, k, l;
	float		a, *angles;
	vec3_t		bounds[2];
	float		xvector[2], yvector[2];
	vec3_t		base, transformed;

	// find min / max for rotations
	angles = ent->v.angles;
		
	a = angles[1]/180 * M_PI;
		
	xvector[0] = cos(a);
	xvector[1] = sin(a);
	yvector[0] = -sin(a);
	yvector[1] = cos(a);
		
	VectorCopy( mins, bounds[0] );
	VectorCopy( maxs, bounds[1] );
		
	rmin[0] = rmin[1] = rmin[2] = 9999;
	rmax[0] = rmax[1] = rmax[2] = -9999;
		
	for( i = 0; i <= 1; i++ )
	{
		base[0] = bounds[i][0];
		for( j = 0; j <= 1; j++ )
		{
			base[1] = bounds[j][1];
			for( k = 0; k <= 1; k++ )
			{
				base[2] = bounds[k][2];
					
				// transform the point
				transformed[0] = xvector[0] * base[0] + yvector[0] * base[1];
				transformed[1] = xvector[1] * base[0] + yvector[1] * base[1];
				transformed[2] = base[2];
					
				for( l = 0; l < 3; l++ )
				{
					if( transformed[l] < rmin[l] ) rmin[l] = transformed[l];
					if( transformed[l] > rmax[l] ) rmax[l] = transformed[l];
				}
			}
		}
	}

	VectorCopy( rmin, ent->v.mins );
	VectorCopy( rmax, ent->v.maxs );
}

void SV_SetMinMaxSize( edict_t *e, const float *min, const float *max, bool rotate )
{
	int		i;

	for( i = 0; i < 3; i++ )
		if( min[i] > max[i] )
			Host_Error( "SV_SetMinMaxSize: backwards mins/maxs\n" );

	rotate = false; // FIXME
			
	// set derived values
	if( rotate && e->v.solid == SOLID_BBOX )
	{
		SV_CalcBBox( e, min, max );
	}
	else
	{
		VectorCopy( min, e->v.mins);
		VectorCopy( max, e->v.maxs);
	}
	VectorSubtract( max, min, e->v.size );

	// TODO: fill also mass and density
	SV_LinkEdict (e);
}

void SV_CopyTraceResult( TraceResult *out, trace_t trace )
{
	if( !out ) return;

	out->fAllSolid = trace.allsolid;
	out->fStartSolid = trace.startsolid;
	out->fStartStuck = trace.startstuck;
	out->flFraction = trace.fraction;
	out->iStartContents = trace.startcontents;
	out->iContents = trace.contents;
	out->iHitgroup = trace.hitgroup;
	out->flPlaneDist = trace.plane.dist;
	VectorCopy( trace.endpos, out->vecEndPos );
	VectorCopy( trace.plane.normal, out->vecPlaneNormal );

	if( trace.surface )
		out->pTexName = trace.surface->name;
	else out->pTexName = NULL;
	out->pHit = trace.ent;
}

void SV_CopyTraceToGlobal( trace_t *trace )
{
	svs.globals->trace_allsolid = trace->allsolid;
	svs.globals->trace_startsolid = trace->startsolid;
	svs.globals->trace_startstuck = trace->startstuck;
	svs.globals->trace_contents = trace->contents;
	svs.globals->trace_start_contents = trace->startcontents;
	svs.globals->trace_fraction = trace->fraction;
	svs.globals->trace_plane_dist = trace->plane.dist;
	svs.globals->trace_ent = trace->ent;
	VectorCopy( trace->endpos, svs.globals->trace_endpos );
	VectorCopy( trace->plane.normal, svs.globals->trace_plane_normal );

	if( trace->surface )
		svs.globals->trace_texture = trace->surface->name;
	else svs.globals->trace_texture = NULL;
	svs.globals->trace_hitgroup = trace->hitgroup;
}

static trace_t SV_TraceToss( edict_t *tossent, edict_t *ignore)
{
	int	i;
	float	gravity;
	vec3_t	move, end;
	vec3_t	original_origin;
	vec3_t	original_velocity;
	vec3_t	original_angles;
	vec3_t	original_avelocity;
	trace_t	trace;

	VectorCopy( tossent->v.origin, original_origin );
	VectorCopy( tossent->v.velocity, original_velocity );
	VectorCopy( tossent->v.angles, original_angles );
	VectorCopy( tossent->v.avelocity, original_avelocity );
	gravity = tossent->v.gravity * sv_gravity->value * 0.05;

	for( i = 0; i < 200; i++ )
	{
		// LordHavoc: sanity check; never trace more than 10 seconds
		SV_CheckVelocity( tossent );
		tossent->v.velocity[2] -= gravity;
		VectorMA( tossent->v.angles, 0.05, tossent->v.avelocity, tossent->v.angles );
		VectorScale( tossent->v.velocity, 0.05, move );
		VectorAdd( tossent->v.origin, move, end );
		trace = SV_Trace( tossent->v.origin, tossent->v.mins, tossent->v.maxs, end, MOVE_NORMAL, tossent, SV_ContentsMask( tossent ));
		VectorCopy( trace.endpos, tossent->v.origin );
		if( trace.fraction < 1 ) break;
	}
	VectorCopy( original_origin, tossent->v.origin );
	VectorCopy( original_velocity, tossent->v.velocity );
	VectorCopy( original_angles, tossent->v.angles );
	VectorCopy( original_avelocity, tossent->v.avelocity );

	return trace;
}

bool SV_CheckForPhysobject( edict_t *ent )
{
	if( !ent ) return false;

	switch( (int)ent->v.solid )
	{
	case SOLID_BSP:
		switch( (int)ent->v.movetype )
		{
		case MOVETYPE_NONE:		// static physobject (no gravity)
		case MOVETYPE_PUSH:		// dynamic physobject (no gravity)
		case MOVETYPE_CONVEYOR:
			// return true;
		default: return false;
		}
		break;
	case SOLID_BOX:
	case SOLID_SPHERE:
	case SOLID_CYLINDER:
	case SOLID_MESH:
		switch( (int)ent->v.movetype )
		{
		case MOVETYPE_PHYSIC:	// dynamic physobject (with gravity)
			return true;
		default: return false;
		}
		break;
	default: return false;
	}
}

void SV_CreatePhysBody( edict_t *ent )
{
	if( !SV_CheckForPhysobject( ent )) return;
	ent->pvEngineData->physbody = pe->CreateBody( ent, SV_GetModelPtr(ent), ent->v.origin, ent->v.m_pmatrix, ent->v.solid, ent->v.movetype );

	pe->SetParameters( ent->pvEngineData->physbody, SV_GetModelPtr(ent), 0, ent->v.mass ); 
}

void SV_SetPhysForce( edict_t *ent )
{
	if( !SV_CheckForPhysobject( ent )) return;
	pe->SetForce( ent->pvEngineData->physbody, ent->v.velocity, ent->v.avelocity, ent->v.force, ent->v.torque );
}

void SV_SetMassCentre( edict_t *ent )
{
	if( !SV_CheckForPhysobject( ent )) return;
	pe->SetMassCentre( ent->pvEngineData->physbody, ent->v.m_pcentre );
}

void SV_SetModel( edict_t *ent, const char *name )
{
	int		i;
	cmodel_t		*mod;
	vec3_t		angles;

	i = SV_ModelIndex( name );
	if( i == 0 ) return;
	// we can accept configstring as non moveable memory ?
	ent->v.model = MAKE_STRING( sv.configstrings[CS_MODELS+i] );
	ent->v.modelindex = i;

	mod = pe->RegisterModel( name );
	if( mod ) SV_SetMinMaxSize( ent, mod->mins, mod->maxs, false );

	switch( (int)ent->v.movetype )
	{
	case MOVETYPE_PHYSIC:
		// FIXME: translate angles correctly
		angles[0] = ent->v.angles[0] - 180;
		angles[1] = ent->v.angles[1];
		angles[2] = ent->v.angles[2] - 90;
		break;
	case MOVETYPE_NONE:
	case MOVETYPE_PUSH:
	case MOVETYPE_CONVEYOR:
		VectorClear( angles );
		ent->v.mass = 0.0f;
		break;
	}

	Matrix3x3_FromAngles( angles, ent->v.m_pmatrix );
	Matrix3x3_Transpose( ent->v.m_pmatrix, ent->v.m_pmatrix );
	SV_CreatePhysBody( ent );
}

float SV_AngleMod( float ideal, float current, float speed )
{
	float	move;

	if (current == ideal) // already there?
		return anglemod( current ); 

	move = ideal - current;
	if (ideal > current)
	{
		if (move >= 180) move = move - 360;
	}
	else
	{
		if (move <= -180) move = move + 360;
	}
	if (move > 0)
	{
		if (move > speed) move = speed;
	}
	else
	{
		if (move < -speed) move = -speed;
	}
	return anglemod(current + move);
}

void SV_ConfigString( int index, const char *val )
{
	if(index < 0 || index >= MAX_CONFIGSTRINGS)
		Host_Error ("configstring: bad index %i value %s\n", index, val);

	if(!*val) val = "";

	// change the string in sv
	com.strcpy( sv.configstrings[index], val );

	if( sv.state != ss_loading )
	{
		// send the update to everyone
		MSG_Clear( &sv.multicast );
		MSG_Begin(svc_configstring);
		MSG_WriteShort (&sv.multicast, index);
		MSG_WriteString (&sv.multicast, (char *)val);
		MSG_Send(MSG_ALL_R, vec3_origin, NULL );
	}
}

bool SV_EntitiesIn( bool mode, vec3_t v1, vec3_t v2 )
{
	int	leafnum, cluster;
	int	area1, area2;
	byte	*mask;

	leafnum = pe->PointLeafnum (v1);
	cluster = pe->LeafCluster (leafnum);
	area1 = pe->LeafArea (leafnum);
	if( mode == DVIS_PHS ) mask = pe->ClusterPHS( cluster );
	else if( mode == DVIS_PVS ) mask = pe->ClusterPVS( cluster ); 
	else Host_Error( "SV_EntitiesIn: unsupported mode\n" );

	leafnum = pe->PointLeafnum (v2);
	cluster = pe->LeafCluster (leafnum);
	area2 = pe->LeafArea (leafnum);

	if( mask && (!(mask[cluster>>3] & (1<<(cluster&7)))))
		return false;
	else if (!pe->AreasConnected (area1, area2))
		return false;
	return true;
}

void SV_InitEdict( edict_t *pEdict )
{
	Com_Assert( pEdict == NULL );
	Com_Assert( pEdict->pvServerData != NULL );

	pEdict->v.pContainingEntity = pEdict; // make cross-links for consistency
	pEdict->pvEngineData = (ed_priv_t *)Mem_Alloc( svs.mempool,  sizeof( ed_priv_t ));
	pEdict->pvServerData = NULL;	// will be alloced later by pfnAllocPrivateData
	pEdict->serialnumber = pEdict->pvEngineData->s.number = NUM_FOR_EDICT( pEdict );
	pEdict->free = false;
}

void SV_FreeEdict( edict_t *pEdict )
{
	Com_Assert( pEdict == NULL );
	Com_Assert( pEdict->free );

	// unlink from world
	SV_UnlinkEdict( pEdict );
	pe->RemoveBody( pEdict->pvEngineData->physbody );

	if( pEdict->pvEngineData ) Mem_Free( pEdict->pvEngineData );
	if( pEdict->pvServerData ) Mem_Free( pEdict->pvServerData );
	Mem_Set( &pEdict->v, 0, sizeof( entvars_t ));

	pEdict->pvEngineData = NULL;
	pEdict->pvServerData = NULL;

	// mark edict as freed
	pEdict->freetime = sv.time;
	pEdict->v.nextthink = -1;
	pEdict->serialnumber = 0;
	pEdict->free = true;
}

edict_t *SV_AllocEdict( void )
{
	edict_t	*pEdict;
	int	i;

	for( i = svs.globals->maxClients + 1; i < svs.globals->numEntities; i++ )
	{
		pEdict = EDICT_NUM( i );
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if( pEdict->free && ( pEdict->freetime < 2.0f || (sv.time - pEdict->freetime) > 0.5f ))
		{
			SV_InitEdict( pEdict );
			return pEdict;
		}
	}

	if( i == svs.globals->maxEntities )
		Host_Error( "SV_AllocEdict: no free edicts\n" );

	svs.globals->numEntities++;
	pEdict = EDICT_NUM( i );
	SV_InitEdict( pEdict );

	return pEdict;
}

/*
============
Save\Load gamestate

savegame operations
============
*/
static int s_table;

void SV_AddSaveLump( wfile_t *f, const char *lumpname, void *data, size_t len, bool compress )
{
	if( f ) WAD_Write( f, lumpname, data, len, TYPE_BINDATA, ( compress ? CMP_ZLIB : CMP_NONE ));
}

static void SV_SetPair( const char *name, const char *value, dkeyvalue_t *cvars, int *numpairs )
{
	if( !name || !value ) return; // SetString can't register null strings
	cvars[*numpairs].epair[DENT_KEY] = StringTable_SetString( s_table, name );
	cvars[*numpairs].epair[DENT_VAL] = StringTable_SetString( s_table, value);
	(*numpairs)++; // increase epairs
}

void SV_AddCvarLump( wfile_t *f )
{
	dkeyvalue_t	cvbuffer[MAX_FIELDS];
	int		numpairs = 0;
	
	Cvar_LookupVars( CVAR_LATCH, cvbuffer, &numpairs, SV_SetPair );
	SV_AddSaveLump( f, LUMP_GAMECVARS, cvbuffer, numpairs * sizeof(dkeyvalue_t), true );
}

void SV_AddCStrLump( wfile_t *f )
{
	string_t		csbuffer[MAX_CONFIGSTRINGS];
	int		i;

	// pack the cfg string data
	for( i = 0; i < MAX_CONFIGSTRINGS; i++ )
		csbuffer[i] = StringTable_SetString( s_table, sv.configstrings[i] );
	SV_AddSaveLump( f, LUMP_CFGSTRING, &csbuffer, sizeof(csbuffer), true );
}

void SV_WriteGlobal( wfile_t *f )
{
	dkeyvalue_t	globals[MAX_FIELDS];
	int		numpairs = 0;

	PRVM_ED_WriteGlobals( &globals, &numpairs, SV_SetPair );
	SV_AddSaveLump( f, LUMP_GAMESTATE, &globals, numpairs * sizeof(dkeyvalue_t), true );
}

void SV_WriteLocals( wfile_t *f )
{
	dkeyvalue_t	fields[MAX_FIELDS];
	vfile_t		*h = VFS_Open( NULL, "wb" );
	int		i, numpairs = 0;

	for( i = 0; i < svs.globals->numEntities; i++ )
	{
		numpairs = 0; // reset fields info
		PRVM_ED_Write( PRVM_EDICT_NUM( i ), &fields, &numpairs, SV_SetPair );
		VFS_Write( h, &numpairs, sizeof( int ));		// numfields
		VFS_Write( h, &fields, numpairs * sizeof( dkeyvalue_t ));	// fields
	}

	// all allocated memory will be freed at end of SV_WriteSaveFile
	SV_AddSaveLump( f, LUMP_GAMEENTS, VFS_GetBuffer( h ), VFS_Tell( h ), true );
	VFS_Close( h ); // release virtual file
}

/*
=============
SV_WriteSaveFile
=============
*/
void SV_WriteSaveFile( const char *name )
{
	string		comment;
	wfile_t		*savfile = NULL;
	bool		autosave = false;
	char		path[MAX_SYSPATH];
	byte		*portalstate = Z_Malloc( MAX_MAP_AREAPORTALS );
	int		portalsize;

	if( sv.state != ss_active )
		return;

	if(Cvar_VariableValue("deathmatch") || Cvar_VariableValue("coop"))
	{
		MsgDev(D_ERROR, "SV_WriteSaveFile: can't savegame in a multiplayer\n");
		return;
	}
	if(Host_MaxClients() == 1 && svs.clients[0].edict->v.health <= 0 )
	{
		MsgDev(D_ERROR, "SV_WriteSaveFile: can't savegame while dead!\n");
		return;
	}

	if(!com.strcmp(name, "save0.bin")) autosave = true;
	com.sprintf( path, "save/%s", name );
	savfile = WAD_Open( path, "wb" );

	if( !savfile )
	{
		MsgDev(D_ERROR, "SV_WriteSaveFile: failed to open %s\n", path );
		return;
	}

	MsgDev( D_INFO, "Saving game..." );
	com.sprintf (comment, "%s - %s", sv.configstrings[CS_NAME], timestamp( TIME_FULL ));
	s_table = StringTable_Create( name, 0x10000 ); // 65535 unique strings
          
	// write lumps
	pe->GetAreaPortals( &portalstate, &portalsize );
	SV_AddSaveLump( savfile, LUMP_AREASTATE, portalstate, portalsize, true );
	SV_AddSaveLump( savfile, LUMP_COMMENTS, comment, sizeof(comment), false );
	SV_AddSaveLump( savfile, LUMP_MAPCMDS, svs.mapcmd, sizeof(svs.mapcmd), false );
	SV_AddCStrLump( savfile );
	SV_AddCvarLump( savfile );
	SV_WriteGlobal( savfile );
	SV_WriteLocals( savfile );
	StringTable_Save( s_table, savfile );	// now system released
	Mem_Free( portalstate );		// release portalinfo
	WAD_Close( savfile );

	MsgDev( D_INFO, "done.\n" );
}

void Sav_LoadComment( wfile_t *l )
{
	byte	*in;
	int	size;

	in = WAD_Read( l, LUMP_COMMENTS, &size, TYPE_BINDATA );
	com.strncpy( svs.comment, in, size );
}

void Sav_LoadCvars( wfile_t *l )
{
	dkeyvalue_t	*in;
	int		i, numpairs;
	const char	*name, *value;

	in = (dkeyvalue_t *)WAD_Read( l, LUMP_GAMECVARS, &numpairs, TYPE_BINDATA );
	if( numpairs % sizeof(*in)) Host_Error( "Sav_LoadCvars: funny lump size\n" );
	numpairs /= sizeof( dkeyvalue_t );

	for( i = 0; i < numpairs; i++ )
	{
		name = StringTable_GetString( s_table, in[i].epair[DENT_KEY] );
		value = StringTable_GetString( s_table, in[i].epair[DENT_VAL] );
		Cvar_SetLatched( name, value );
	}
}

void Sav_LoadMapCmds( wfile_t *l )
{
	byte	*in;
	int	size;

	in = WAD_Read( l, LUMP_MAPCMDS, &size, TYPE_BINDATA );
	com.strncpy( svs.mapcmd, in, size );
}

void Sav_LoadCfgString( wfile_t *l )
{
	string_t	*in;
	int	i, numstrings;

	in = (string_t *)WAD_Read( l, LUMP_CFGSTRING, &numstrings, TYPE_BINDATA );
	if( numstrings % sizeof(*in)) Host_Error( "Sav_LoadCfgString: funny lump size\n" );
	numstrings /= sizeof( string_t ); // because old saves can contain last values of MAX_CONFIGSTRINGS

	// unpack the cfg string data
	for( i = 0; i < numstrings; i++ )
		com.strncpy( sv.configstrings[i], StringTable_GetString( s_table, in[i] ), CS_SIZE );  
}

void Sav_LoadAreaPortals( wfile_t *l )
{
	byte	*in;
	int	size;

	in = WAD_Read( l, LUMP_AREASTATE, &size, TYPE_BINDATA );
	pe->SetAreaPortals( in, size ); // CM_ReadPortalState
}

void Sav_LoadGlobal( wfile_t *l )
{
	dkeyvalue_t	*globals;
	int		numpairs;

	globals = (dkeyvalue_t *)WAD_Read( l, LUMP_GAMESTATE, &numpairs, TYPE_BINDATA );
	if( numpairs % sizeof(*globals)) Host_Error( "Sav_LoadGlobal: funny lump size\n" );
	numpairs /= sizeof( dkeyvalue_t );
	PRVM_ED_ReadGlobals( s_table, globals, numpairs );
}

void Sav_LoadLocals( wfile_t *l )
{
	dkeyvalue_t	fields[MAX_FIELDS];
	int		numpairs, entnum = 0;
	byte		*buff;
	size_t		size;
	vfile_t		*h;

	buff = WAD_Read( l, LUMP_GAMEENTS, &size, TYPE_BINDATA );
	h = VFS_Create( buff, size );

	while(!VFS_Eof( h ))
	{
		VFS_Read( h, &numpairs, sizeof( int ));
		VFS_Read( h, &fields, numpairs * sizeof( dkeyvalue_t ));
		PRVM_ED_Read( s_table, entnum, fields, numpairs );
		entnum++;
	}
	svs.globals->numEntities = entnum;
}

/*
=============
SV_ReadSaveFile
=============
*/
void SV_ReadSaveFile( const char *name )
{
	char		path[MAX_SYSPATH];
	wfile_t		*savfile;
	int		s_table;

	com.sprintf(path, "save/%s", name );
	savfile = WAD_Open( path, "rb" );

	if( !savfile )
	{
		MsgDev(D_ERROR, "SV_ReadSaveFile: can't open %s\n", path );
		return;
	}

	s_table = StringTable_Load( savfile, name );
	Sav_LoadComment( savfile );
	Sav_LoadCvars( savfile );
	StringTable_Delete( s_table );
	
	SV_InitGame(); // start a new game fresh with new cvars
	Sav_LoadMapCmds( savfile );
	WAD_Close( savfile );
	CL_Drop();
}

/*
=============
SV_ReadLevelFile
=============
*/
void SV_ReadLevelFile( const char *name )
{
	char		path[MAX_SYSPATH];
	wfile_t		*savfile;
	int		s_table;

	com.sprintf (path, "save/%s", name );
	savfile = WAD_Open( path, "rb" );

	if( !savfile )
	{
		MsgDev( D_ERROR, "SV_ReadLevelFile: can't open %s\n", path );
		return;
	}

	s_table = StringTable_Load( savfile, name );
	Sav_LoadCfgString( savfile );
	Sav_LoadAreaPortals( savfile );
	Sav_LoadGlobal( savfile );
	Sav_LoadLocals( savfile );
	StringTable_Delete( s_table );
	WAD_Close( savfile );
}

bool SV_ReadComment( char *comment, int savenum )
{
	wfile_t		*savfile;
	int		result;

	if( !comment ) return false;
	result = WAD_Check( va( "save/save%i.bin", savenum )); 

	switch( result )
	{
	case 0:
		com.strncpy( comment, "<empty>", MAX_STRING );
		return false;
	case 1:	break;
	default:
		com.strncpy( comment, "<corrupted>", MAX_STRING );
		return false;
	}

	savfile = WAD_Open( va("save/save%i.bin", savenum), "rb" );
	Sav_LoadComment( savfile );
	com.strncpy( comment, svs.comment, MAX_STRING );
	WAD_Close( savfile );

	return true;
}

/*
===============================================================================
	Game Builtin Functions

===============================================================================
*/

void *pfnMemAlloc( size_t cb, const char *filename, const int fileline )
{
	return com.malloc( svs.mempool, cb, filename, fileline );
}

void pfnMemFree( void *mem, const char *filename, const int fileline )
{
	com.free( mem, filename, fileline );
}

/*
=========
pfnPrecacheModel

=========
*/
int pfnPrecacheModel( const char *s )
{
	return SV_ModelIndex( s );
}

/*
=========
pfnPrecacheSound

=========
*/
int pfnPrecacheSound( const char *s )
{
	return SV_SoundIndex( s );
}

/*
=================
pfnSetModel

=================
*/
void pfnSetModel( edict_t *e, const char *m )
{
	if( e == game.edicts )
	{
		MsgDev( D_WARN, "SV_SetModel: can't modify world entity\n" );
		return;
	}

	if( e->free )
	{
		MsgDev( D_WARN, "SV_SetModel: can't modify free entity\n" );
		return;
	}

	if( m[0] <= ' ' )
	{
		MsgDev( D_WARN, "SV_SetModel: null name\n" );
		return;
	}
	SV_SetModel( e, m ); 
}

/*
=================
pfnModelIndex

=================
*/
int pfnModelIndex( const char *m )
{
	int	index;

	if( !com.strcmp( m, "" )) return 0;
	index = SV_FindIndex( m, CS_MODELS, MAX_MODELS, false );	

	if( !index ) MsgDev( D_WARN, "SV_ModelIndex: %s not precached\n", m );
	return index; 
}

/*
=================
pfnModelFrames

=================
*/
int pfnModelFrames( int modelIndex )
{
	cmodel_t	*mod;

	// can be returned pointer on a registered model 	
	mod = pe->RegisterModel( sv.configstrings[CS_MODELS + modelIndex] );

	if( mod ) return mod->numframes;
	MsgDev( D_WARN, "SV_ModelFrames: %s not found\n", sv.configstrings[CS_MODELS + modelIndex] );

	return 0;
}

/*
=================
pfnSetSize

=================
*/
void pfnSetSize( edict_t *e, const float *rgflMin, const float *rgflMax )
{
	if( !e || !e->pvServerData )
	{
		MsgDev( D_ERROR, "SV_SetSize: entity not exist\n" );
		return;
	}
	else if( e == game.edicts )
	{
		MsgDev( D_ERROR, "SV_SetSize: can't modify world entity\n" );
		return;
	}
	else if( e->free )
	{
		MsgDev( D_ERROR, "SV_SetSize: can't modify free entity\n" );
		return;
	}

	SV_SetMinMaxSize( e, rgflMin, rgflMax, !VectorIsNull( e->v.angles ));
}

/*
=================
pfnChangeLevel

=================
*/
void pfnChangeLevel( const char* s1, const char* s2 )
{
	string 	changelevel_cmd;

	com.snprintf( changelevel_cmd, MAX_STRING, "changelevel %s %s\n", s1, s2 ); 
	Cbuf_ExecuteText( EXEC_APPEND, changelevel_cmd );
}

/*
=================
pfnVecToYaw

=================
*/
float pfnVecToYaw( const float *rgflVector )
{
	float	yaw;

	if( rgflVector[1] == 0 && rgflVector[0] == 0 )
	{
		yaw = 0;
	}
	else
	{
		yaw = (int)( com.atan2(rgflVector[1], rgflVector[0]) * 180 / M_PI );
		if( yaw < 0 ) yaw += 360;
	}
	return yaw;
}

/*
=================
pfnVecToAngles

=================
*/
void pfnVecToAngles( const float *rgflVectorIn, float *rgflVectorOut )
{
	float	forward;
	float	yaw, pitch;

	if( rgflVectorIn[1] == 0 && rgflVectorIn[0] == 0 )
	{
		yaw = 0;
		if( rgflVectorIn[2] > 0 )
			pitch = 90;
		else pitch = 270;
	}
	else
	{
		if( rgflVectorIn[0] )
		{
			yaw = (int)( com.atan2( rgflVectorIn[1], rgflVectorIn[0] ) * 180 / M_PI );
			if( yaw < 0 ) yaw += 360;
		}
		else if( rgflVectorIn[1] > 0 )
			yaw = 90;
		else yaw = 270;

		forward = com.sqrt( rgflVectorIn[0] * rgflVectorIn[0] + rgflVectorIn[1] * rgflVectorIn[1] );
		pitch = (int)( com.atan2( rgflVectorIn[2], forward ) * 180 / M_PI );
		if( pitch < 0 ) pitch += 360;
	}

	if( rgflVectorOut ) VectorSet( rgflVectorOut, pitch, yaw, 0 ); 
	else MsgDev( D_ERROR, "SV_VecToAngles: no output vector specified\n" );
}

/*
=================
pfnMoveToOrigin

FIXME: i'm not sure what is what you want
=================
*/
void pfnMoveToOrigin( edict_t *ent, const float *pflGoal, float dist, int iMoveType )
{
	vec3_t	targetDir;
	float	accelXY = 300.0f, accelZ = 600.0f;
	float	decay = 9.0f, flInterval = 0.1f;	// FIXME

	VectorCopy( pflGoal, targetDir );
	VectorNormalize( targetDir );

	decay = exp( log( decay ) / 1.0f * flInterval );
	accelXY *= flInterval;
	accelZ  *= flInterval;

	ent->v.velocity[0] = ( decay * ent->v.velocity[0] + accelXY * targetDir[0] );
	ent->v.velocity[1] = ( decay * ent->v.velocity[1] + accelXY * targetDir[1] );
	ent->v.velocity[2] = ( decay * ent->v.velocity[2] + accelZ  * targetDir[2] );
}

/*
==============
pfnChangeYaw

==============
*/
void pfnChangeYaw( edict_t* ent )
{
	float	current;

	if( ent == game.edicts )
	{
		MsgDev( D_WARN, "SV_ChangeYaw: can't modify world entity\n" );
		return;
	}
	if( ent->free )
	{
		MsgDev( D_WARN, "SV_ChangeYaw: can't modify free entity\n" );
		return;
	}

	current = anglemod( ent->v.angles[YAW] );
	ent->v.angles[YAW] = SV_AngleMod( ent->v.ideal_yaw, current, ent->v.yaw_speed );
}

/*
==============
pfnChangePitch

==============
*/
void pfnChangePitch( edict_t* ent )
{
	float	current;

	if( ent == game.edicts )
	{
		MsgDev( D_WARN, "SV_ChangePitch: can't modify world entity\n" );
		return;
	}
	if( ent->free )
	{
		MsgDev( D_WARN, "SV_ChangePitch: can't modify free entity\n" );
		return;
	}

	current = anglemod( ent->v.angles[PITCH] );
	ent->v.angles[PITCH] = SV_AngleMod( ent->v.idealpitch, current, ent->v.pitch_speed );	
}

/*
=========
pfnFindEntityByString

=========
*/
edict_t* pfnFindEntityByString( edict_t *pStartEdict, const char *pszField, const char *pszValue )
{
	int		e, f;
	const char	*t;
	edict_t		*ed;

	Msg("SV_FindEntityByString: %s\n", pszField );

	if( !pStartEdict )
		e = NUM_FOR_EDICT( game.edicts );
	else e = NUM_FOR_EDICT( pStartEdict );
	if( !pszValue ) pszValue = "";

	if( !com.strcmp( pszField, "classname" ))
		f = EOFS( classname );
	else if( !com.strcmp( pszField, "globalname" ))
		f = EOFS( globalname );
	else if( !com.strcmp( pszField, "targetname" ))
		f = EOFS( targetname );
	else
	{
		// FIXME: make cases for all fileds
		MsgDev( D_ERROR, "field %s unsupported\n", pszField );
		return pStartEdict;
	}

	for( e++; e < svs.globals->numEntities; e++ )
	{
		ed = EDICT_NUM( e );
		if( ed->free ) continue;

		t = STRING( *(string_t *)&((int *)&ed->v)[f] );
		if( !t ) t = "";
		if( !com.strcmp( t, pszValue ))
			return ed;
	}
	return game.edicts;
}

/*
==============
pfnGetEntityIllum

==============
*/
int pfnGetEntityIllum( edict_t* pEnt )
{
	if( pEnt == game.edicts )
	{
		MsgDev( D_WARN, "SV_GetEntityIllum: can't get light level at world entity\n" );
		return 0;
	}
	if( pEnt->free )
	{
		MsgDev( D_WARN, "SV_GetEntityIllum: can't get light level at free entity\n" );
		return 0;
	}	
	return 255; // FIXME: implement
}

/*
=================
pfnFindEntityInSphere

return NULL instead of world
=================
*/
edict_t* pfnFindEntityInSphere( edict_t *pStartEdict, const float *org, float rad )
{
	edict_t	*ent;
	float	radSquare;
	vec3_t	eorg;
	int	e = 0;

	radSquare = rad * rad;
	if( pStartEdict )
		e = NUM_FOR_EDICT( pStartEdict );
	
	for( e++; e < svs.globals->numEntities; e++ )
	{
		ent = EDICT_NUM( e );
		if( ent->free ) continue;

		VectorSubtract( org, ent->v.origin, eorg );
		VectorMAMAM( 1, eorg, 0.5f, ent->v.mins, 0.5f, ent->v.maxs, eorg );

		if( DotProduct( eorg, eorg ) < radSquare )
			return ent;
	}
	return NULL; // fisrt chain
}

/*
=================
pfnFindClientInPVS

return NULL instead of world
=================
*/
edict_t* pfnFindClientInPVS( edict_t *pEdict )
{
	edict_t	*pClient;
	int	i;

	for( i = 1; i < svs.globals->maxClients; i++ )
	{
		pClient = game.edicts + i;
		if( pClient->free ) continue;
		if( SV_EntitiesIn( DVIS_PVS, pEdict->v.origin, pClient->v.origin ))
		{
			Msg( "found client %d\n", pClient->serialnumber );
			return pEdict;
		}
	}
	return NULL;
}

/*
=================
pfnFindClientInPHS

return NULL instead of world
=================
*/
edict_t* pfnFindClientInPHS( edict_t *pEdict )
{
	edict_t	*pClient;
	int	i;

	for( i = 1; i < svs.globals->maxClients; i++ )
	{
		pClient = game.edicts + i;
		if( pClient->free ) continue;
		if( SV_EntitiesIn( DVIS_PHS, pEdict->v.origin, pClient->v.origin ))
		{
			Msg( "found client %d\n", pClient->serialnumber );
			return pEdict;
		}
	}
	return NULL;
}

/*
=================
pfnEntitiesInPVS

=================
*/
edict_t* pfnEntitiesInPVS( edict_t *pplayer )
{
	edict_t	*pEdict, *chain;
	int	i, numents;

	chain = NULL;
	numents = svs.globals->numEntities;

	for( i = svs.globals->maxClients; i < numents; i++ )
	{
		pEdict = game.edicts + i;
		if( pEdict->free ) continue;
		if( SV_EntitiesIn( DVIS_PVS, pEdict->v.origin, pplayer->v.origin ))
		{
			Msg( "found entity %d\n", pEdict->serialnumber );
			pEdict->v.chain = chain;
			chain = pEdict;
		}
	}
	return chain; // fisrt entry
}

/*
=================
pfnEntitiesInPHS

=================
*/
edict_t* pfnEntitiesInPHS( edict_t *pplayer )
{
	edict_t	*pEdict, *chain;
	int	i, numents;

	chain = NULL;
	numents = svs.globals->numEntities;

	for( i = svs.globals->maxClients; i < numents; i++ )
	{
		pEdict = game.edicts + i;
		if( pEdict->free ) continue;
		if( SV_EntitiesIn( DVIS_PHS, pEdict->v.origin, pplayer->v.origin ))
		{
			Msg( "found entity %d\n", pEdict->serialnumber );
			pEdict->v.chain = chain;
			chain = pEdict;
		}
	}
	return chain; // fisrt entry
}

/*
==============
pfnMakeVectors

==============
*/
void pfnMakeVectors( const float *rgflVector )
{
	AngleVectors( rgflVector, svs.globals->v_forward, svs.globals->v_right, svs.globals->v_up );
}

/*
==============
pfnAngleVectors

can receive NULL output args
==============
*/
void pfnAngleVectors( const float *rgflVector, float *forward, float *right, float *up )
{
	AngleVectors( rgflVector, forward, right, up );
}

/*
==============
pfnCreateEntity

just allocate new one
==============
*/
edict_t* pfnCreateEntity( void )
{
	return SV_AllocEdict();
}

/*
==============
pfnRemoveEntity

free edict private mem, unlink physics etc
==============
*/
void pfnRemoveEntity( edict_t* e )
{
	// never free client or world entity
	if( NUM_FOR_EDICT( e ) < svs.globals->maxClients )
	{
		MsgDev( D_ERROR, "SV_RemoveEntity: can't delete %s\n", (e == game.edicts) ? "world" : "client" );
		return;
	}
	SV_FreeEdict( e );
}

/*
==============
pfnCreateNamedEntity

==============
*/
edict_t* pfnCreateNamedEntity( string_t className )
{
	edict_t		*ed;
	const char	*pszClassName;

	pszClassName = STRING( className );
	ed = pfnCreateEntity();
	ed->v.classname = className;
	pszClassName = STRING( className );

	// also register classname to send for client
	ed->pvEngineData->s.classname = SV_ClassIndex( pszClassName );

	return ed;
}

/*
=============
pfnMakeStatic

disable entity updates to client
=============
*/
void pfnMakeStatic( edict_t *ent )
{
	ent->pvEngineData->s.ed_type = ED_STATIC;
}

/*
=============
pfnEntIsOnFloor

stupid unused bulletin
=============
*/
int pfnEntIsOnFloor( edict_t *e )
{
	return (e->v.flags & FL_ONGROUND) ? true : false;
}
	
/*
===============
pfnDropToFloor

===============
*/
int pfnDropToFloor( edict_t* e )
{
	vec3_t		end;
	trace_t		trace;

	// ignore world silently
	if( e == game.edicts ) return false;
	if( e->free )
	{
		MsgDev( D_ERROR, "SV_DropToFloor: can't modify free entity\n" );
		return false;
	}

	VectorCopy( e->v.origin, end );
	end[2] -= 256;
	SV_UnstickEntity( e );

	trace = SV_Trace( e->v.origin, e->v.mins, e->v.maxs, end, MOVE_NORMAL, e, SV_ContentsMask( e ));

	if( trace.startsolid )
	{
		vec3_t	offset, org;

		VectorSet( offset, 0.5f * (e->v.mins[0] + e->v.maxs[0]), 0.5f * (e->v.mins[1] + e->v.maxs[1]), e->v.mins[2] );
		VectorAdd( e->v.origin, offset, org );
		trace = SV_Trace( org, vec3_origin, vec3_origin, end, MOVE_NORMAL, e, SV_ContentsMask( e ));
		VectorSubtract( trace.endpos, offset, trace.endpos );

		if( trace.startsolid )
		{
			MsgDev( D_WARN, "SV_DropToFloor: startsolid at %g %g %g\n", e->v.origin[0], e->v.origin[1], e->v.origin[2] );
			SV_UnstickEntity( e );
			SV_LinkEdict( e );
			e->v.flags |= FL_ONGROUND;
			e->v.groundentity = 0;
		}
		else if( trace.fraction < 1 )
		{
			MsgDev( D_WARN, "SV_DropToFloor: moved to %g %g %g\n", e->v.origin[0], e->v.origin[1], e->v.origin[2] );
			VectorCopy( trace.endpos, e->v.origin );
			SV_UnstickEntity( e );
			SV_LinkEdict( e );
			e->v.flags |= FL_ONGROUND;
			e->v.groundentity = trace.ent;
			// if support is destroyed, keep suspended
			// (gross hack for floating items in various maps)
			e->pvEngineData->suspended = true;
			return true;
		}

		MsgDev( D_WARN, "SV_DropToFloor: startsolid at %g %g %g\n", e->v.origin[0], e->v.origin[1], e->v.origin[2] );
		pfnRemoveEntity( e );
		return false;
	}
	else
	{
		if( trace.fraction != 1 )
		{
			if( trace.fraction < 1 )
				VectorCopy( trace.endpos, e->v.origin );
			SV_LinkEdict( e );
			e->v.flags |= FL_ONGROUND;
			e->v.groundentity = trace.ent;
			// if support is destroyed, keep suspended
			// (gross hack for floating items in various maps)
			e->pvEngineData->suspended = true;
			return true;
		}
	}
	return false;
}

/*
===============
pfnWalkMove

FIXME: tune modes
===============
*/
int pfnWalkMove( edict_t *ent, float yaw, float dist, int iMode )
{
	vec3_t		move;

	// ignore world silently
	if( ent == game.edicts ) return false;
	if( ent->free )
	{
		MsgDev( D_WARN, "SV_DropToFloor: can't modify free entity\n" );
		return false;
	}

	if(!(ent->v.aiflags & AI_FLY|AI_SWIM) || !(ent->v.flags & FL_ONGROUND))
		return false;
	yaw = yaw * M_PI * 2 / 360;

	VectorSet( move, com.cos( yaw ) * dist, com.sin( yaw ) * dist, 0.0f );
	return SV_movestep( ent, move, true, iMode, false );
}

/*
=================
pfnSetOrigin

=================
*/
void pfnSetOrigin( edict_t *e, const float *rgflOrigin )
{
	// ignore world silently
	if( e == game.edicts ) return;
	if( e->free )
	{
		MsgDev( D_ERROR, "SV_SetOrigin: can't modify free entity\n" );
		return;
	}

	if( VectorCompare( rgflOrigin, e->v.origin ))
		return; // already there ?

	VectorCopy( rgflOrigin, e->v.origin );
	pe->SetOrigin( e->pvEngineData->physbody, e->v.origin );
	SV_LinkEdict( e );
}

/*
=================
pfnSetAngles

=================
*/
void pfnSetAngles( edict_t *e, const float *rgflAngles )
{
	if( e == game.edicts ) return;
	if( e->free )
	{
		MsgDev( D_ERROR, "SV_SetAngles: can't modify free entity\n" );
		return;
	}

	if( VectorCompare( rgflAngles, e->v.angles ))
		return; // already there ?

	VectorCopy( rgflAngles, e->v.angles );
	Matrix3x3_FromAngles( e->v.angles, e->v.m_pmatrix );
	Matrix3x3_Transpose( e->v.m_pmatrix, e->v.m_pmatrix );
}

/*
=================
pfnEmitSound

=================
*/
void pfnEmitSound( edict_t *ent, int chan, const char *sample, float vol, float attn, int flags, int pitch )
{
	// FIXME: implement
}

/*
=================
pfnEmitAmbientSound

=================
*/
void pfnEmitAmbientSound( edict_t *ent, float *pos, const char *samp, float vol, float attn, int flags, int pitch )
{
	// FIXME: implement
}

/*
=================
pfnTraceLine

=================
*/
void pfnTraceLine( const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr )
{
	trace_t		trace;
	int		move;

	move = (fNoMonsters) ? MOVE_NOMONSTERS : MOVE_NORMAL;

	if( IS_NAN(v1[0]) || IS_NAN(v1[1]) || IS_NAN(v1[2]) || IS_NAN(v2[0]) || IS_NAN(v1[2]) || IS_NAN(v2[2] ))
		Host_Error( "SV_Trace: NAN errors detected ('%f %f %f', '%f %f %f'\n", v1[0], v1[1], v1[2], v2[0], v2[1], v2[2] );

	trace = SV_Trace( v1, vec3_origin, vec3_origin, v2, move, pentToSkip, SV_ContentsMask( pentToSkip ));
	SV_CopyTraceResult( ptr, trace );
}

/*
=================
pfnTraceToss

=================
*/
void pfnTraceToss( edict_t* pent, edict_t* pentToIgnore, TraceResult *ptr )
{
	trace_t		trace;

	if( pent == game.edicts ) return;
	trace = SV_TraceToss( pent, pentToIgnore );
	SV_CopyTraceResult( ptr, trace );
}

/*
=================
pfnTraceHull

FIXME: replace constant hulls with mins/maxs
=================
*/
void pfnTraceHull( const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr )
{
	vec3_t		mins, maxs;
	trace_t		trace;
	int		move;

	move = (fNoMonsters) ? MOVE_NOMONSTERS : MOVE_NORMAL;

	switch( hullNumber )
	{
	case human_hull:
		VectorSet( mins, -16, -16, 0 );
		VectorSet( maxs,  16,  16, 72);
		break; 
	case large_hull:
		VectorSet( mins, -32, -32,-32);
		VectorSet( maxs,  32,  32, 32);
		break;
	case head_hull:	// ducked
		VectorSet( mins, -16, -16,-18);
		VectorSet( maxs,  16,  16, 18);
		break;
	case point_hull:
	default:	VectorCopy( vec3_origin, mins );
		VectorCopy( vec3_origin, maxs );
		break; 
	}

	if( IS_NAN(v1[0]) || IS_NAN(v1[1]) || IS_NAN(v1[2]) || IS_NAN(v2[0]) || IS_NAN(v1[2]) || IS_NAN(v2[2] ))
		Host_Error( "SV_TraceHull: NAN errors detected ('%f %f %f', '%f %f %f'\n", v1[0], v1[1], v1[2], v2[0], v2[1], v2[2] );

	trace = SV_Trace( v1, mins, mins, v2, move, pentToSkip, SV_ContentsMask( pentToSkip ));
	SV_CopyTraceResult( ptr, trace );
}

void pfnTraceModel( const float *v1, const float *v2, edict_t *pent, TraceResult *ptr )
{
	// FIXME: implement
}

const char *pfnTraceTexture( edict_t *pTextureEntity, const float *v1, const float *v2 )
{
	trace_t	trace;

	if( IS_NAN(v1[0]) || IS_NAN(v1[1]) || IS_NAN(v1[2]) || IS_NAN(v2[0]) || IS_NAN(v1[2]) || IS_NAN(v2[2] ))
		Host_Error( "SV_TraceTexture: NAN errors detected ('%f %f %f', '%f %f %f'\n", v1[0], v1[1], v1[2], v2[0], v2[1], v2[2] );

	trace = SV_Trace( v1, vec3_origin, vec3_origin, v2, MOVE_NOMONSTERS, NULL, SV_ContentsMask( pTextureEntity ));

	if( trace.surface )
		return trace.surface->name;
	return NULL;
}

/*
=============
pfnGetAimVector

FIXME: use speed for reduce aiming accuracy
=============
*/
void pfnGetAimVector( edict_t* ent, float speed, float *rgflReturn )
{
	edict_t	*check, *bestent;
	vec3_t	start, dir, end, bestdir;
	float	dist, bestdist;
	bool	fNoFriendlyFire;
	int	i, j;
	trace_t	tr;

	// these vairable defined in server.dll	
	fNoFriendlyFire = Cvar_VariableValue( "mp_friendlyfire" );
	VectorCopy( svs.globals->v_forward, rgflReturn );		// assume failure if it returns early

	if( ent == game.edicts ) return;
	if( ent->free )
	{
		MsgDev( D_ERROR, "SV_GetAimVector: can't aiming at free entity\n" );
		return;
	}

	VectorCopy( ent->v.origin, start );
	start[2] += 20;

	// try sending a trace straight
	VectorCopy( svs.globals->v_forward, dir );
	VectorMA( start, 2048, dir, end );
	tr = SV_Trace( start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent, -1 );

	if( tr.ent && (tr.ent->v.takedamage == DAMAGE_AIM && fNoFriendlyFire || ent->v.team <= 0 || ent->v.team != tr.ent->v.team ))
	{
		VectorCopy( svs.globals->v_forward, rgflReturn );
		return;
	}

	// try all possible entities
	VectorCopy( dir, bestdir );
	bestdist = 0.5f;
	bestent = NULL;

	check = game.edicts + 1; // start at first client
	for( i = 1; i < svs.globals->numEntities; i++, check++ )
	{
		if( check->v.takedamage != DAMAGE_AIM ) continue;
		if( check == ent ) continue;
		if( fNoFriendlyFire && ent->v.team > 0 && ent->v.team == check->v.team )
			continue;	// don't aim at teammate
		for( j = 0; j < 3; j++ )
			end[j] = check->v.origin[j] + 0.5 * (check->v.mins[j] + check->v.maxs[j]);
		VectorSubtract( end, start, dir );
		VectorNormalize( dir );
		dist = DotProduct( dir, svs.globals->v_forward );
		if( dist < bestdist ) continue; // to far to turn
		tr = SV_Trace( start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent, -1 );
		if( tr.ent == check )
		{	
			// can shoot at this one
			bestdist = dist;
			bestent = check;
		}
	}

	if( bestent )
	{
		VectorSubtract( bestent->v.origin, ent->v.origin, dir );
		dist = DotProduct( dir, svs.globals->v_forward );
		VectorScale( svs.globals->v_forward, dist, end );
		end[2] = dir[2];
		VectorNormalize( end );
		VectorCopy( end, rgflReturn );
	}
	else VectorCopy( bestdir, rgflReturn );
}

/*
=========
pfnServerCommand

=========
*/
void pfnServerCommand( const char* str )
{
	Cbuf_AddText( str );
}

/*
=========
pfnServerExecute

=========
*/
void pfnServerExecute( void )
{
	Cbuf_Execute();
}

/*
=========
pfnClientCommand

=========
*/
void pfnClientCommand( edict_t* pEdict, char* szFmt, ... )
{
	sv_client_t	*client;
	string		buffer;
	va_list		args;
	int		i;

	i = NUM_FOR_EDICT( pEdict );
	if( sv.state != ss_active  || i < 0 || i >= Host_MaxClients() || svs.clients[i].state != cs_spawned )
	{
		MsgDev( D_ERROR, "SV_ClientCommand: client/server is not active!\n" );
		return;
	}

	client = svs.clients + i;
	va_start( args, szFmt );
	com.vsnprintf( buffer, MAX_STRING, szFmt, args );
	va_end( args );


	MSG_WriteByte( &client->netchan.message, svc_stufftext );
	MSG_WriteString( &client->netchan.message, buffer );
	MSG_Send( MSG_ONE_R, NULL, client->edict );
}

/*
=================
pfnParticleEffect

=================
*/
void pfnParticleEffect( const float *org, const float *dir, float color, float count )
{
	// FIXME: implement
}

/*
===============
pfnLightStyle

===============
*/
void pfnLightStyle( int style, char* val )
{
	if((uint)style >= MAX_LIGHTSTYLES )
		Host_Error( "SV_LightStyle: style: %i >= %d", style, MAX_LIGHTSTYLES );
	SV_ConfigString( CS_LIGHTSTYLES + style, val );
}

/*
=================
pfnDecalIndex

register decal shader on client
=================
*/
int pfnDecalIndex( const char *m )
{
	return SV_DecalIndex( m );	
}

/*
=============
pfnPointContents

=============
*/
int pfnPointContents( const float *rgflVector )
{
	return SV_PointContents( rgflVector );
}

/*
=============
pfnMessageBegin

=============
*/
void pfnMessageBegin( int msg_dest, int msg_type, const float *pOrigin, edict_t *ed )
{
	// some users can send message with engine index
	// reduce number to avoid overflow problems or cheating
	game.msg_index = bound( svc_bad, msg_type, svc_nop );

	MSG_Begin( game.msg_index );

	// save message destination
	if( pOrigin ) VectorCopy( pOrigin, svs.msg_org );
	else VectorClear( svs.msg_org );
	game.msg_leftsize = game.msg_sizes[msg_type];
	svs.msg_dest = msg_dest;
	svs.msg_ent = ed;
}

/*
=============
pfnMessageEnd

=============
*/
void pfnMessageEnd( void )
{
	return;//
	
	if( game.msg_leftsize != 0xFFFF && game.msg_leftsize != 0 )
	{
		const char *name = sv.configstrings[CS_USER_MESSAGES + game.msg_index];
		int size = game.msg_sizes[game.msg_index];
		int realsize = size - game.msg_leftsize;

		MsgDev( D_ERROR, "SV_Message: %s expected %i bytes, it written %i\n", name, size, realsize );
		MSG_Clear( &sv.multicast );
		return;
	}

	svs.msg_dest = bound( MSG_ONE, svs.msg_dest, MSG_PVS_R );
	MSG_Send( svs.msg_dest, svs.msg_org, svs.msg_ent );
}

/*
=============
pfnWriteByte

=============
*/
void pfnWriteByte( int iValue )
{
	MSG_WriteByte( &sv.multicast, (int)iValue );
	if( game.msg_leftsize != 0xFFFF ) game.msg_leftsize--;
}

/*
=============
pfnWriteChar

=============
*/
void pfnWriteChar( int iValue )
{
	MSG_WriteChar( &sv.multicast, (int)iValue );
	if( game.msg_leftsize != 0xFFFF ) game.msg_leftsize--;
}

/*
=============
pfnWriteShort

=============
*/
void pfnWriteShort( int iValue )
{
	MSG_WriteShort( &sv.multicast, (int)iValue );
	if( game.msg_leftsize != 0xFFFF ) game.msg_leftsize -= 2;
}

/*
=============
pfnWriteLong

=============
*/
void pfnWriteLong( int iValue )
{
	MSG_WriteLong( &sv.multicast, (int)iValue );
	if( game.msg_leftsize != 0xFFFF ) game.msg_leftsize -= 4;
}

/*
=============
pfnWriteAngle

=============
*/
void pfnWriteAngle( float flValue )
{
	MSG_WriteAngle32( &sv.multicast, flValue );
	if( game.msg_leftsize != 0xFFFF ) game.msg_leftsize -= 4;
}

/*
=============
pfnWriteCoord

=============
*/
void pfnWriteCoord( float flValue )
{
	MSG_WriteCoord32( &sv.multicast, flValue );
	if( game.msg_leftsize != 0xFFFF ) game.msg_leftsize -= 4;
}

/*
=============
pfnWriteString

=============
*/
void pfnWriteString( const char *sz )
{
	int	cur_size = sv.multicast.cursize;
	int	total_size;

	MSG_WriteString( &sv.multicast, sz );
	total_size = sv.multicast.cursize - cur_size;

	// some messages with constant strings can be marked as known sized
	if( game.msg_leftsize != 0xFFFF )
		game.msg_leftsize -= total_size;
}

/*
=============
pfnWriteEntity

=============
*/
void pfnWriteEntity( int iValue )
{
	if( iValue < 0 || iValue > svs.globals->numEntities )
		Host_Error( "MSG_WriteEntity: invalid entnumber %d\n", iValue );
	MSG_WriteShort( &sv.multicast, iValue );
	if( game.msg_leftsize != 0xFFFF ) game.msg_leftsize -= 2;
}

/*
=============
pfnCVarRegister

=============
*/
void pfnCVarRegister( const char *name, const char *value, int flags, const char *desc )
{
	// FIXME: translate server.dll flags to real cvar flags
	Cvar_Get( name, value, flags, desc );
}

/*
=============
pfnCVarGetFloat

=============
*/
float pfnCVarGetFloat( const char *szVarName )
{
	return Cvar_VariableValue( szVarName );
}

/*
=============
pfnCVarGetString

=============
*/
const char* pfnCVarGetString( const char *szVarName )
{
	return Cvar_VariableString( szVarName );
}

/*
=============
pfnCVarSetFloat

=============
*/
void pfnCVarSetFloat( const char *szVarName, float flValue )
{
	Cvar_SetValue( szVarName, flValue );
}

/*
=============
pfnCVarSetString

=============
*/
void pfnCVarSetString( const char *szVarName, const char *szValue )
{
	Cvar_Set( szVarName, szValue );
}

/*
=============
pfnAlertMessage

=============
*/
void pfnAlertMessage( ALERT_TYPE level, char *szFmt, ... )
{
	char		buffer[2048];	// must support > 1k messages
	va_list		args;

	va_start( args, szFmt );
	com.vsnprintf( buffer, 2048, szFmt, args );
	va_end( args );

	// FIXME: implement message filter
	com.print( buffer );
}

/*
=============
pfnPvAllocEntPrivateData

=============
*/
void *pfnPvAllocEntPrivateData( edict_t *pEdict, long cb )
{
	Com_Assert( pEdict == NULL );

	// to avoid multiple alloc
	pEdict->pvServerData = (void *)Mem_Realloc( svs.private, pEdict->pvServerData, cb );

	return pEdict->pvServerData;
}

/*                                 
=============
pfnFreeEntPrivateData

=============
*/
void pfnFreeEntPrivateData( edict_t *pEdict )
{
	if( pEdict->pvServerData )
		Mem_Free( pEdict->pvServerData );
	pEdict->pvServerData = NULL; // freed
}

/*
=============
pfnAllocString

=============
*/
string_t pfnAllocString( const char *szValue )
{
	return StringTable_SetString( game.hStringTable, szValue );
}		

/*
=============
pfnGetString

=============
*/
const char *pfnGetString( string_t iString )
{
	return StringTable_GetString( game.hStringTable, iString );
}

/*
=============
pfnPEntityOfEntOffset

=============
*/
edict_t* pfnPEntityOfEntOffset( int iEntOffset )
{
	return (&((edict_t*)game.vp)[iEntOffset]);
}

/*
=============
pfnEntOffsetOfPEntity

=============
*/
int pfnEntOffsetOfPEntity( const edict_t *pEdict )
{
	return ((byte *)pEdict - (byte *)game.vp);
}

/*
=============
pfnIndexOfEdict

=============
*/
int pfnIndexOfEdict( const edict_t *pEdict )
{
	return NUM_FOR_EDICT( pEdict );
}

/*
=============
pfnPEntityOfEntIndex

=============
*/
edict_t* pfnPEntityOfEntIndex( int iEntIndex )
{
	return EDICT_NUM( iEntIndex );
}

/*
=============
pfnFindEntityByVars

slow linear brute force
=============
*/
edict_t* pfnFindEntityByVars( entvars_t* pvars )
{
	edict_t	*e = game.edicts;
	int	i;

	Msg("FindEntity by VARS()\n" );

	// HACKHACK
	if( pvars ) return pvars->pContainingEntity;

	for( i = 0; i < svs.globals->numEntities; i++, e++ )
	{
		if( e->free ) continue;
		if( !memcmp( &e->v, pvars, sizeof( entvars_t )))
			return e;
	}
	return NULL;
}

/*
=============
pfnGetModelPtr

returns pointer to a studiomodel
=============
*/
void* pfnGetModelPtr( edict_t* pEdict )
{
	cmodel_t	*mod;

	mod = pe->RegisterModel( sv.configstrings[CS_MODELS + pEdict->v.modelindex] );

	if( !mod ) return NULL;
	return mod->extradata;
}

/*
=============
pfnRegUserMsg

=============
*/
int pfnRegUserMsg( const char *pszName, int iSize )
{
	// register message first
	int	msg_index;

	msg_index = SV_UserMessageIndex( pszName );
	if( iSize == -1 )
		game.msg_sizes[msg_index] = 0xFFFF;
	else game.msg_sizes[msg_index] = iSize;

	return msg_index;
}

/*
=============
pfnAnimationAutomove

=============
*/
void pfnAnimationAutomove( const edict_t* pEdict, float flTime )
{
	// FIXME: implement
}

/*
=============
pfnGetBonePosition

=============
*/
void pfnGetBonePosition( const edict_t* pEdict, int iBone, float *rgflOrigin, float *rgflAngles )
{
	// FIXME: implement
}

/*
=============
pfnFunctionFromName

=============
*/
dword pfnFunctionFromName( const char *pName )
{
	int	i, index;

	for( i = 0; i < game.num_ordinals; i++ )
	{
		if( !com.strcmp( pName, game.names[i] ))
		{
			index = game.ordinals[i];
			return game.funcs[index] + game.funcBase;
		}
	}

	// couldn't find the function name to return address
	return 0;
}

/*
=============
pfnNameForFunction

=============
*/
const char *pfnNameForFunction( dword function )
{
	int	i, index;

	for( i = 0; i < game.num_ordinals; i++ )
	{
		index = game.ordinals[i];

		if((function - game.funcBase) == game.funcs[index] )
			return game.names[i];
	}

	// couldn't find the function address to return name
	return NULL;
}

/*
=============
pfnClientPrintf

=============
*/
void pfnClientPrintf( edict_t* pEdict, int ptype, const char *szMsg )
{
	sv_client_t	*client;
	int		num;

	num = NUM_FOR_EDICT( pEdict );
	if( num < 1 || num > svs.globals->maxClients || svs.clients[num - 1].state != cs_spawned )
	{
		MsgDev( D_WARN, "SV_ClientPrintf: tired print to a non-client!\n" );
		return;
	}
	client = svs.clients + num - 1;

	switch( ptype )
	{
	case PRINT_CONSOLE:
		SV_ClientPrintf (client, PRINT_CONSOLE, (char *)szMsg );
		break;
	case PRINT_CENTER:
		MSG_Begin( svc_centerprint );
		MSG_WriteString( &sv.multicast, szMsg );
		MSG_Send( MSG_ONE_R, NULL, pEdict );
		break;
	case PRINT_CHAT:
		SV_ClientPrintf( client, PRINT_CHAT, (char *)szMsg );
		break;
	default:
		MsgDev( D_ERROR, "SV_ClientPrintf: invalid destination\n" );
		break;
	}
}

/*
=============
pfnServerPrint

=============
*/
void pfnServerPrint( const char *szMsg )
{
	if( sv.state == ss_loading )
	{
		// while loading in-progress we can sending message
		com.print( szMsg );	// only for local client
	}
	else SV_BroadcastPrintf( PRINT_CONSOLE, (char *)szMsg );
}

/*
=============
pfnAreaPortal

changes area portal state
=============
*/
void pfnAreaPortal( edict_t *pEdict, bool enable )
{
	if( pEdict == game.edicts ) return;
	if( pEdict->free )
	{
		MsgDev( D_ERROR, "SV_AreaPortal: can't modify free entity\n" );
		return;
	}
	pe->SetAreaPortalState( pEdict->v.skin, enable );
}

/*
=============
pfnCmd_Args

=============
*/
const char *pfnCmd_Args( void )
{
	return Cmd_Args();
}

/*
=============
pfnCmd_Argv

=============
*/
const char *pfnCmd_Argv( int argc )
{
	if( argc >= 0 && argc < Cmd_Argc())
		return Cmd_Argv( argc );
	return "";
}

/*
=============
pfnCmd_Argc

=============
*/
int pfnCmd_Argc( void )
{
	return Cmd_Argc();
}

/*
=============
pfnGetAttachment

=============
*/
void pfnGetAttachment( const edict_t *pEdict, int iAttachment, float *rgflOrigin, float *rgflAngles )
{
	// FIXME: implement
}

/*
=============
pfnCRC_Init

=============
*/
void pfnCRC_Init( word *pulCRC )
{
	CRC_Init( pulCRC );
}

/*
=============
pfnCRC_ProcessBuffer

=============
*/
void pfnCRC_ProcessBuffer( word *pulCRC, void *p, int len )
{
	byte	*start = (byte *)p;

	while( len-- ) CRC_ProcessByte( pulCRC, *start++ );
}

/*
=============
pfnCRC_Final

=============
*/
word pfnCRC_Final( word pulCRC )
{
	return pulCRC ^ 0x0000;
}				

/*
=============
pfnRandomLong

=============
*/
long pfnRandomLong( long lLow, long lHigh )
{
	return Com_RandomLong( lLow, lHigh );
}

/*
=============
pfnRandomFloat

=============
*/
float pfnRandomFloat( float flLow, float flHigh )
{
	return Com_RandomFloat( flLow, flHigh );
}

/*
=============
pfnSetView

=============
*/
void pfnSetView( const edict_t *pClient, const edict_t *pViewent )
{
	// FIXME: implement
}

/*
=============
pfnCrosshairAngle

=============
*/
void pfnCrosshairAngle( const edict_t *pClient, float pitch, float yaw )
{
	// FIXME: implement
}

/*
=============
pfnLoadFile

=============
*/
byte* pfnLoadFile( const char *filename, int *pLength )
{
	return FS_LoadFile( filename, pLength );
}

/*
=============
pfnFreeFile

=============
*/
void pfnFreeFile( void *buffer )
{
	if( buffer ) Mem_Free( buffer );
}

/*
=============
pfnCompareFileTime

=============
*/
int pfnCompareFileTime( const char *filename1, const char *filename2, int *iCompare )
{
	int time1 = FS_FileTime( filename1 );
	int time2 = FS_FileTime( filename2 );

	if( iCompare )
	{
		if( time1 > time2 )
			*iCompare = 1;			
		else if( time1 < time2 )
			*iCompare = -1;
		else *iCompare = 0;
	}
	return (time1 != time2);
}

/*
=============
pfnGetGameDir

=============
*/
void pfnGetGameDir( char *szGetGameDir )
{
	// FIXME: potentially crashpoint
	com.strcpy( szGetGameDir, FS_Gamedir );
}

/*
=============
pfnGetGameDir

=============
*/
void pfnStaticDecal( const float *origin, int decalIndex, int entityIndex, int modelIndex )
{
	// FIXME: implement
}

/*
=============
pfnPrecacheGeneric

can be used for precache scripts
=============
*/
int pfnPrecacheGeneric( const char* s )
{
	// FIXME: implement
	return 0;
}

/*
=============
pfnIsDedicatedServer

=============
*/
int pfnIsDedicatedServer( void )
{
	return (host.type == HOST_DEDICATED);
}

/*
=============
pfnIsMapValid

vaild map must contain one info_player_deatchmatch
=============
*/
int pfnIsMapValid( char *filename )
{
	wfile_t	*wad;
	dheader_t	*hdr;
	bool	valid;
	
	wad = WAD_Open( filename, "rb" );
	if( !wad ) return false; 

	hdr = (dheader_t *)WAD_Read( wad, LUMP_MAPINFO, NULL, TYPE_BINDATA );
	valid = (LittleLong( hdr->flags ) & MAP_DEATHMATCH) ? true : false;
	WAD_Close( wad );

	return valid;
}

/*
=============
pfnInfo_RemoveKey

=============
*/
void pfnInfo_RemoveKey( char *s, char *key )
{
	Info_RemoveKey( s, key );
}

/*
=============
pfnInfoKeyValue

=============
*/
char *pfnInfoKeyValue( char *infobuffer, char *key )
{
	return Info_ValueForKey( infobuffer, key );
}

/*
=============
pfnSetKeyValue

=============
*/
void pfnSetKeyValue( char *infobuffer, char *key, char *value )
{
	Info_SetValueForKey( infobuffer, key, value );
}

/*
=============
pfnGetInfoKeyBuffer

=============
*/
char *pfnGetInfoKeyBuffer( edict_t *e )
{
	int	index;

	index = NUM_FOR_EDICT( e );
	if( index > 0 && index < svs.globals->numClients )
		return e->pvEngineData->client->userinfo;
	return Cvar_Serverinfo(); // otherwise return ServerInfo
}

/*
=============
pfnSetClientKeyValue

=============
*/
void pfnSetClientKeyValue( int clientIndex, char *infobuffer, char *key, char *value )
{
	if( clientIndex > 0 && clientIndex < svs.globals->numClients )
	{
		sv_client_t *client = svs.clients + clientIndex;
		Info_SetValueForKey( client->userinfo, key, value );
	}
}

/*
=============
pfnSetSkybox

=============
*/
void pfnSetSkybox( const char *name )
{
	SV_ConfigString( CS_SKYNAME, name );
}

/*
=============
pfnPlayMusic

=============
*/
void pfnPlayMusic( const char *trackname, int flags )
{
	SV_ConfigString( CS_BACKGROUND_TRACK, trackname );
}

/*
=============
pfnDropClient

=============
*/
void pfnDropClient( int clientIndex )
{
	if( clientIndex < 0 || clientIndex >= svs.globals->maxClients )
	{
		MsgDev( D_ERROR, "SV_DropClient: not a client\n" );
		return;
	}
	if( svs.clients[clientIndex].state != cs_spawned )
	{
		MsgDev( D_ERROR, "SV_DropClient: that client slot is not connected\n" );
		return;
	}
	SV_DropClient( svs.clients + clientIndex );
}
				
// engine callbacks
static enginefuncs_t gEngfuncs = 
{
	sizeof( enginefuncs_t ),
	pfnMemAlloc,
	pfnMemFree,
	pfnPrecacheModel,
	pfnPrecacheSound,	
	pfnSetModel,
	pfnModelIndex,
	pfnModelFrames,
	pfnSetSize,	
	pfnChangeLevel,
	pfnVecToYaw,
	pfnVecToAngles,		
	pfnMoveToOrigin,
	pfnChangeYaw,
	pfnChangePitch,
	pfnFindEntityByString,
	pfnGetEntityIllum,
	pfnFindEntityInSphere,
	pfnFindClientInPVS,
	pfnFindClientInPHS,
	pfnEntitiesInPVS,
	pfnEntitiesInPHS,		
	pfnMakeVectors,
	pfnAngleVectors,
	pfnCreateEntity,
	pfnRemoveEntity,
	pfnCreateNamedEntity,				
	pfnMakeStatic,
	pfnEntIsOnFloor,
	pfnDropToFloor,		
	pfnWalkMove,
	pfnSetOrigin,
	pfnSetAngles,		
	pfnEmitSound,
	pfnEmitAmbientSound,
	pfnTraceLine,
	pfnTraceToss,
	pfnTraceHull,
	pfnTraceModel,
	pfnTraceTexture,
	pfnGetAimVector,		
	pfnServerCommand,
	pfnServerExecute,
	pfnClientCommand,
	pfnParticleEffect,
	pfnLightStyle,
	pfnDecalIndex,
	pfnPointContents,
	pfnMessageBegin,
	pfnMessageEnd,
	pfnWriteByte,
	pfnWriteChar,
	pfnWriteShort,
	pfnWriteLong,
	pfnWriteAngle,
	pfnWriteCoord,
	pfnWriteString,
	pfnWriteEntity,
	pfnCVarRegister,
	pfnCVarGetFloat,
	pfnCVarGetString,
	pfnCVarSetFloat,
	pfnCVarSetString,
	pfnAlertMessage,		
	pfnPvAllocEntPrivateData,
	pfnFreeEntPrivateData,
	pfnAllocString,
	pfnGetString,
	pfnPEntityOfEntOffset,
	pfnEntOffsetOfPEntity,
	pfnIndexOfEdict,
	pfnPEntityOfEntIndex,					
	pfnFindEntityByVars,
	pfnGetModelPtr,
	pfnRegUserMsg,
	pfnAnimationAutomove,
	pfnGetBonePosition,				
	pfnFunctionFromName,
	pfnNameForFunction,	
	pfnClientPrintf,
	pfnServerPrint,	
	pfnAreaPortal,
	pfnCmd_Args,	
	pfnCmd_Argv,
	pfnCmd_Argc,	
	pfnGetAttachment,
	pfnCRC_Init,
	pfnCRC_ProcessBuffer,
	pfnCRC_Final,		
	pfnRandomLong,
	pfnRandomFloat,
	pfnSetView,
	pfnCrosshairAngle,
	pfnLoadFile,
	pfnFreeFile,
	pfnCompareFileTime,
	pfnGetGameDir,							
	pfnStaticDecal,
	pfnPrecacheGeneric,
	pfnIsDedicatedServer,		
	pfnIsMapValid,
	pfnInfo_RemoveKey,
	pfnInfoKeyValue,
	pfnSetKeyValue,		
	pfnGetInfoKeyBuffer,
	pfnSetClientKeyValue,	
	pfnSetSkybox,
	pfnPlayMusic,
	pfnDropClient,		
	Host_Error
};

/*
====================
SV_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
====================
*/
bool SV_ParseEdict( script_t *script, edict_t *ent )
{
	KeyValueData	pkvd[256]; // per one entity
	int		i, numpairs = 0;
	const char	*classname = NULL;
	LINK_ENTITY_FUNC	SpawnEdict;
	token_t		token;

	// go through all the dictionary pairs
	while( 1 )
	{	
		string	keyname;

		// parse key
		if( !Com_ReadToken( script, SC_ALLOW_NEWLINES, &token ))
			Host_Error( "SV_ParseEdict: EOF without closing brace\n" );
		if( token.string[0] == '}' ) break; // end of desc
		com.strncpy( keyname, token.string, sizeof( keyname ) - 1 );

		// parse value	
		if( !Com_ReadToken( script, SC_ALLOW_PATHNAMES2, &token ))
			Host_Error( "SV_ParseEdict: EOF without closing brace\n" );

		if( token.string[0] == '}' )
			Host_Error( "SV_ParseEdict: closing brace without data\n" );

		// keynames with a leading underscore are used for utility comments,
		// and are immediately discarded by engine
		if( keyname[0] == '_' ) continue;

		// create keyvalue strings
		pkvd[numpairs].szClassName = (char *)classname;	// unknown at this moment
		pkvd[numpairs].szKeyName = ED_NewString( keyname, svs.stringpool );
		pkvd[numpairs].szValue = ED_NewString( token.string, svs.stringpool );
		pkvd[numpairs].fHandled = false;		

		if( !com.strcmp( keyname, "classname" ) && classname == NULL )
			classname = pkvd[numpairs].szValue;
		if( ++numpairs >= 256 ) break;
	}

	// allocate edict private memory (passed by dlls)
	SpawnEdict = (LINK_ENTITY_FUNC)Com_GetProcAddress( svs.game, classname );
	if( !SpawnEdict )
	{
		MsgDev( D_ERROR, "No spawn function for %s\n", classname );
		return false;
	}

	// apply edict classnames
	ent->pvEngineData->s.classname = SV_ClassIndex( classname );
	ent->v.classname = pfnAllocString( classname );

	SpawnEdict( &ent->v );

	// apply classname to keyvalue containers and parse fields			
	for( i = 0; i < numpairs; i++ )
	{
		if( pkvd[i].fHandled ) continue;
		pkvd[i].szClassName = (char *)classname;
		svs.dllFuncs.pfnKeyValue( ent, &pkvd[i] );
	}
	return true;
}

/*
================
SV_LoadFromFile

The entities are directly placed in the array, rather than allocated with
ED_Alloc, because otherwise an error loading the map would have entity
number references out of order.

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.
================
*/
void SV_LoadFromFile( script_t *entities )
{
	token_t	token;
	int	inhibited, spawned, died;
	bool	create_world = true;
	edict_t	*ent;

	inhibited = 0;
	spawned = 0;
	died = 0;

	// parse ents
	while( Com_ReadToken( entities, SC_ALLOW_NEWLINES, &token ))
	{
		if( token.string[0] != '{' )
			Host_Error( "SV_LoadFromFile: found %s when expecting {\n", token.string );

		if( create_world )
		{
			create_world = false;
			ent = EDICT_NUM( 0 );
			SV_InitEdict( ent );
		}
		else ent = SV_AllocEdict();

		if( !SV_ParseEdict( entities, ent ))
			continue;

		if( svs.dllFuncs.pfnSpawn( ent ) == -1 )
			died++;
		else spawned++;
	}
	MsgDev( D_INFO, "%i entities inhibited\n", inhibited );
}

/*
==============
SpawnEntities

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.
==============
*/
void SV_SpawnEntities( const char *mapname, script_t *entities )
{
	edict_t	*ent;
	int	i;

	MsgDev( D_NOTE, "SV_SpawnEntities()\n" );

	ent = EDICT_NUM( 0 );
	SV_InitEdict( ent );
	ent->v.model = pfnAllocString( sv.configstrings[CS_MODELS] );
	ent->v.modelindex = 1;	// world model
	ent->v.solid = SOLID_BSP;
	ent->v.movetype = MOVETYPE_PUSH;
	ent->free = false;
			
	SV_ConfigString( CS_MAXCLIENTS, va("%i", Host_MaxClients()));
	svs.globals->mapname = pfnAllocString( sv.name );
	svs.globals->time = sv.time;

	// spawn the rest of the entities on the map
	SV_LoadFromFile( entities );

	// set client fields on player ents
	for( i = 0; i < svs.globals->maxClients; i++ )
	{
		// setup all clients
		ent = EDICT_NUM( i + 1 );
		SV_InitEdict( ent );
		ent->pvEngineData->client = svs.clients + i;
		svs.globals->numClients++;
	}
	Msg("Total %i entities spawned\n", svs.globals->numEntities );
}

void SV_UnloadProgs( void )
{
	Sys_FreeNameFuncGlobals();
	StringTable_Delete( game.hStringTable );
	Com_FreeLibrary( svs.game );
	Mem_FreePool( &svs.mempool );
	Mem_FreePool( &svs.stringpool );
	Mem_FreePool( &svs.private );
}

bool SV_LoadProgs( const char *name )
{
	static APIFUNCTION		GetEntityAPI;
	static GIVEFNPTRSTODLL	GiveFnptrsToDll;
	static globalvars_t		gpGlobals;
	string			libname;
	edict_t			*e;
	int			i;

	if( svs.game ) SV_UnloadProgs();

	// fill it in
	svs.globals = &gpGlobals;
	com.snprintf( libname, MAX_STRING, "bin/%s.dll", name );
	svs.mempool = Mem_AllocPool( "Server Edicts Zone" );
	svs.stringpool = Mem_AllocPool( "Server Strings Zone" );
	svs.private = Mem_AllocPool( "Server Private Zone" );

	svs.game = Com_LoadLibrary( libname );
	if( !svs.game ) return false;

	GetEntityAPI = (APIFUNCTION)Com_GetProcAddress( svs.game, "GetEntityAPI" );

	if( !GetEntityAPI )
	{
          	MsgDev( D_ERROR, "SV_LoadProgs: failed to get address of GetEntityAPI proc\n" );
		return false;
	}

	GiveFnptrsToDll = (GIVEFNPTRSTODLL)Com_GetProcAddress( svs.game, "GiveFnptrsToDll" );

	if( !GiveFnptrsToDll )
	{
		// can't find GiveFnptrsToDll!
		MsgDev( D_ERROR, "SV_LoadProgs: failed to get address of GiveFnptrsToDll proc\n" );
		return false;
	}

	if( !Sys_LoadSymbols( va( "bin/%s.dll", name )))
		return false;

	GiveFnptrsToDll( &gEngfuncs, svs.globals );

	if( !GetEntityAPI( &svs.dllFuncs, INTERFACE_VERSION ))
	{
		MsgDev( D_ERROR, "SV_LoadProgs: couldn't get entity API\n" );
		return false;
	}

	// 65535 unique strings should be enough ...
	game.hStringTable = StringTable_Create( "Game Strings", 0x10000 );
	StringTable_SetString( game.hStringTable, "" ); // make NULL string
	svs.globals->maxEntities = host.max_edicts;
	svs.globals->maxClients = Host_MaxClients();
	game.edicts = Mem_Alloc( svs.mempool, sizeof( edict_t ) * svs.globals->maxEntities );
	svs.globals->numEntities = svs.globals->maxClients + 1; // clients + world
	svs.globals->numClients = 0;
	for( i = 0, e = game.edicts; i < svs.globals->maxEntities; i++, e++ )
		e->free = true; // mark all edicts as freed

	// initialize game
	svs.dllFuncs.pfnGameInit();

	return true;
}