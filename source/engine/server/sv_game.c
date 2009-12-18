//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        sv_game.c - server dlls interaction
//=======================================================================

#include "common.h"
#include "server.h"
#include "net_sound.h"
#include "byteorder.h"
#include "matrix_lib.h"
#include "com_library.h"
#include "pm_defs.h"
#include "const.h"

void Sys_FsGetString( file_t *f, char *str )
{
	char	ch;

	while(( ch = FS_Getc( f )) != EOF )
	{
		*str++ = ch;
		if( !ch ) break;
	}
}

void Sys_FreeNameFuncGlobals( void )
{
	int	i;

	if( svgame.ordinals ) Mem_Free( svgame.ordinals );
	if( svgame.funcs ) Mem_Free( svgame.funcs );

	for( i = 0; i < svgame.num_ordinals; i++ )
	{
		if( svgame.names[i] )
			Mem_Free( svgame.names[i] );
	}

	svgame.num_ordinals = 0;
	svgame.ordinals = NULL;
	svgame.funcs = NULL;
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
			out_name = com.stralloc( svgame.private, in_name + 1, __FILE__, __LINE__ );
			out_name[len-1] = 0; // terminate string at the "@@"
			return out_name;
		}
	}
	return com.stralloc( svgame.private, in_name, __FILE__, __LINE__ );
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

	for( i = 0; i < svgame.num_ordinals; i++ )
		svgame.names[i] = NULL;

	f = FS_Open( filename, "rb" );
	if( !f )
	{
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s not found\n", filename );
		return false;
	}

	if( FS_Read( f, &dos_header, sizeof( dos_header )) != sizeof( dos_header ))
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
			edata_found = true;
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
		edata_delta = 0;
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

	svgame.num_ordinals = export_directory.NumberOfNames;	// also number of ordinals

	if( svgame.num_ordinals > 4096 )
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

	svgame.ordinals = Mem_Alloc( svgame.private, svgame.num_ordinals * sizeof( word ));

	if( FS_Read( f, svgame.ordinals, svgame.num_ordinals * sizeof( word )) != (svgame.num_ordinals * sizeof( word )))
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

	svgame.funcs = Mem_Alloc( svgame.private, svgame.num_ordinals * sizeof( dword ));

	if( FS_Read( f, svgame.funcs, svgame.num_ordinals * sizeof( dword )) != (svgame.num_ordinals * sizeof( dword )))
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

	p_Names = Mem_Alloc( svgame.private, svgame.num_ordinals * sizeof( dword ));

	if( FS_Read( f, p_Names, svgame.num_ordinals * sizeof( dword )) != (svgame.num_ordinals * sizeof( dword )))
	{
		Sys_FreeNameFuncGlobals();
		if( p_Names ) Mem_Free( p_Names );
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s error during reading names table\n", filename );
		FS_Close( f );
		return false;
	}

	for( i = 0; i < svgame.num_ordinals; i++ )
	{
		name_offset = p_Names[i] - edata_delta;

		if( name_offset != 0 )
		{
			if( FS_Seek( f, name_offset, SEEK_SET ) != -1 )
			{
				Sys_FsGetString( f, function_name );
				svgame.names[i] = Sys_GetMSVCName( function_name );
			}
			else break;
		}
	}

	if( i != svgame.num_ordinals )
	{
		Sys_FreeNameFuncGlobals();
		if( p_Names ) Mem_Free( p_Names );
		MsgDev( D_ERROR, "Sys_LoadSymbols: %s error during loading names section\n", filename );
		FS_Close( f );
		return false;
	}
	FS_Close( f );

	for( i = 0; i < svgame.num_ordinals; i++ )
	{
		if( !com.strcmp( "CreateAPI", svgame.names[i] ))
		{
			void	*fn_offset;

			index = svgame.ordinals[i];
			fn_offset = (void *)Com_GetProcAddress( svgame.hInstance, "CreateAPI" );
			svgame.funcBase = (dword)(fn_offset) - svgame.funcs[index];
			break;
		}
	}

	if( p_Names ) Mem_Free( p_Names );
	return true;
}

void SV_SetMinMaxSize( edict_t *e, const float *min, const float *max )
{
	int	i;

	for( i = 0; i < 3; i++ )
		if( min[i] > max[i] )
			Host_Error( "SV_SetMinMaxSize: backwards mins/maxs\n" );

	VectorCopy( min, e->v.mins );
	VectorCopy( max, e->v.maxs );
	VectorSubtract( max, min, e->v.size );

	SV_LinkEdict( e, false );
}

void SV_CopyTraceToGlobal( trace_t *trace )
{
	svgame.globals->trace_allsolid = trace->fAllSolid;
	svgame.globals->trace_startsolid = trace->fStartSolid;
	svgame.globals->trace_fraction = trace->flFraction;
	svgame.globals->trace_plane_dist = trace->flPlaneDist;
	svgame.globals->trace_ent = trace->pHit;
	svgame.globals->trace_flags = 0;
	svgame.globals->trace_inopen = trace->fInOpen;
	svgame.globals->trace_inwater = trace->fInWater;
	VectorCopy( trace->vecEndPos, svgame.globals->trace_endpos );
	VectorCopy( trace->vecPlaneNormal, svgame.globals->trace_plane_normal );
	svgame.globals->trace_hitgroup = trace->iHitgroup;
}

void SV_SetModel( edict_t *ent, const char *name )
{
	int	i;
	vec3_t	mins, maxs;

	i = SV_ModelIndex( name );
	if( i == 0 ) return;

	ent->v.model = MAKE_STRING( sv.configstrings[CS_MODELS+i] );
	ent->v.modelindex = i;

	Mod_GetBounds( ent->v.modelindex, mins, maxs );
	SV_SetMinMaxSize( ent, mins, maxs );
}

float SV_AngleMod( float ideal, float current, float speed )
{
	float	move;

	if( current == ideal ) // already there?
		return anglemod( current ); 

	move = ideal - current;
	if( ideal > current )
	{
		if( move >= 180 ) move = move - 360;
	}
	else
	{
		if( move <= -180 ) move = move + 360;
	}
	if( move > 0 )
	{
		if( move > speed ) move = speed;
	}
	else
	{
		if( move < -speed ) move = -speed;
	}
	return anglemod( current + move );
}

void SV_ConfigString( int index, const char *val )
{
	if( index < 0 || index >= MAX_CONFIGSTRINGS )
		Host_Error( "SV_ConfigString: bad index %i value %s\n", index, val );

	if( !val || !*val ) val = "";

	// change the string in sv
	com.strcpy( sv.configstrings[index], val );

	if( sv.state != ss_loading )
	{
		// send the update to everyone
		MSG_Clear( &sv.multicast );
		MSG_Begin( svc_configstring );
		MSG_WriteShort( &sv.multicast, index );
		MSG_WriteString( &sv.multicast, val );
		MSG_Send( MSG_ALL, vec3_origin, NULL );
	}
}

static bool SV_EntitiesIn( int mode, vec3_t v1, vec3_t v2 )
{
	int	leafnum, cluster;
	int	area1, area2;
	byte	*mask;

	leafnum = CM_PointLeafnum( v1 );
	cluster = CM_LeafCluster( leafnum );
	area1 = CM_LeafArea( leafnum );
	if( mode == DVIS_PHS ) mask = CM_ClusterPHS( cluster );
	else if( mode == DVIS_PVS ) mask = CM_ClusterPVS( cluster ); 
	else Host_Error( "SV_EntitiesIn ?\n" );

	leafnum = CM_PointLeafnum( v2 );
	cluster = CM_LeafCluster( leafnum );
	area2 = CM_LeafArea( leafnum );

	if( mask && (!( mask[cluster>>3] & (1<<( cluster & 7 )))))
		return false;
	else if( !CM_AreasConnected( area1, area2 ))
		return false;
	return true;
}

script_t *SV_GetEntityScript( const char *filename )
{
	file_t		*f;
	dheader_t		*header;
	string		entfilename;
	script_t		*ents = NULL;
	int		ver = -1, lumpofs = 0, lumplen = 0;
	byte		buf[MAX_SYSPATH]; // 1 kb
	bool		result = false;
			
	f = FS_Open( va( "maps/%s.bsp", filename ), "rb" );
	if( !f ) return NULL;

	Mem_Set( buf, 0, MAX_SYSPATH );
	FS_Read( f, buf, MAX_SYSPATH );
                              
	switch( LittleLong( *(uint *)buf ))
	{
	case IDBSPMODHEADER:
	case RBBSPMODHEADER:
	case QFBSPMODHEADER:
	case XRBSPMODHEADER:
		header = (dheader_t *)buf;
		ver = LittleLong(((int *)buf)[1]);
		switch( ver )
		{
		case Q3IDBSP_VERSION:	// quake3 arena
		case RTCWBSP_VERSION:	// return to castle wolfenstein
		case RFIDBSP_VERSION:	// raven or qfusion bsp
		case XRIDBSP_VERSION:	// x-real engine
			lumpofs = LittleLong( header->lumps[LUMP_ENTITIES].fileofs );
			lumplen = LittleLong( header->lumps[LUMP_ENTITIES].filelen );
			break;
		default:
			FS_Close( f );
			return NULL;
		}
		break;
	default:
		FS_Close( f );
		return NULL;
	}

	// check for entfile too
	com.strncpy( entfilename, va( "maps/%s.ent", filename ), sizeof( entfilename ));
	ents = Com_OpenScript( entfilename, NULL, 0 );

	if( !ents && lumplen >= 10 )
	{
		char	*entities = NULL;
		
		FS_Seek( f, lumpofs, SEEK_SET );
		entities = (char *)Z_Malloc( lumplen + 1 );
		FS_Read( f, entities, lumplen );
		ents = Com_OpenScript( "ents", entities, lumplen + 1 );
		Mem_Free( entities ); // no reason to keep it
	}
	FS_Close( f ); // all done

	return ents;
}

bool SV_MapIsValid( const char *filename, const char *spawn_entity )
{
	script_t	*ents = NULL;
	bool	result = false;

	ents = SV_GetEntityScript( filename );

	if( ents )
	{
		// if there are entities to parse, a missing message key just
		// means there is no title, so clear the message string now
		token_t	token;
		string	check_entity;

		if( host.developer >= 2 )
		{
			Com_CloseScript( ents );
			return true; // skip playerstart serach in devmode
		}

		check_entity[0] = 0;
		while( Com_ReadToken( ents, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token ))
		{
			if( !com.strcmp( token.string, "classname" ))
			{
				// check classname for spawn entity
				Com_ReadString( ents, false, check_entity );
				if( !com.stricmp( spawn_entity, check_entity ))
				{
					result = true;
					break;						
				}
			}
		}
		Com_CloseScript( ents );
	}
	return result;
}

void SV_InitEdict( edict_t *pEdict )
{
	Com_Assert( pEdict == NULL );
	Com_Assert( pEdict->pvPrivateData != NULL );
	Com_Assert( pEdict->pvServerData != NULL );

	pEdict->v.pContainingEntity = pEdict; // make cross-links for consistency
	pEdict->pvServerData = (sv_priv_t *)Mem_Alloc( svgame.mempool, sizeof( sv_priv_t ));
	pEdict->pvPrivateData = NULL;	// will be alloced later by pfnAllocPrivateData
	pEdict->serialnumber = NUM_FOR_EDICT( pEdict );
	pEdict->free = false;
}

void SV_FreeEdict( edict_t *pEdict )
{
	Com_Assert( pEdict == NULL );
	Com_Assert( pEdict->free );

	// unlink from world
	SV_UnlinkEdict( pEdict );

	if( pEdict->pvServerData ) Mem_Free( pEdict->pvServerData );
	if( pEdict->pvPrivateData )
	{
		svgame.dllFuncs.pfnOnFreeEntPrivateData( pEdict );
		Mem_Free( pEdict->pvPrivateData );
	}
	Mem_Set( pEdict, 0, sizeof( *pEdict ));

	// mark edict as freed
	pEdict->freetime = sv.time * 0.001f;
	pEdict->v.nextthink = -1;
	pEdict->free = true;
}

edict_t *SV_AllocEdict( void )
{
	edict_t	*pEdict;
	float	time = sv.time * 0.001;
	int	i;

	for( i = svgame.globals->maxClients + 1; i < svgame.globals->numEntities; i++ )
	{
		pEdict = EDICT_NUM( i );
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if( pEdict->free && ( pEdict->freetime < 2.0 || time - pEdict->freetime > 0.5 ))
		{
			SV_InitEdict( pEdict );
			return pEdict;
		}
	}

	if( i == svgame.globals->maxEntities )
		Host_Error( "SV_AllocEdict: no free edicts\n" );

	svgame.globals->numEntities++;
	pEdict = EDICT_NUM( i );
	SV_InitEdict( pEdict );

	return pEdict;
}

edict_t *SV_CopyEdict( const edict_t *in )
{
	size_t	pvdata_size = 0;
	edict_t	*out;

	if( in == NULL ) return NULL;	// failed to copy
	if( in->free ) return NULL;	// we can't proceed freed edicts
	if( in->v.flags & (FL_CLIENT|FL_FAKECLIENT)) return NULL; // never get copy of clients

	// must passed through dlls for correctly get all pointers
	out = SV_AllocPrivateData( NULL, in->v.classname );

	// important! we save copy off, not in sv.edicts
	if( out == NULL ) Host_Error( "SV_CopyEdict: dest == NULL\n" );

	if( in->pvServerData )
	{
		if( out->pvServerData == NULL )
			out->pvServerData = (sv_priv_t *)Mem_Alloc( svgame.mempool, sizeof( sv_priv_t ));
		Mem_Copy( out->pvServerData, in->pvServerData, sizeof( sv_priv_t ));
		pvdata_size = in->pvServerData->pvdata_size;
	}
	if( in->pvPrivateData )
	{
		if( pvdata_size > 0 )
		{
			out->pvPrivateData = (void *)Mem_Realloc( svgame.private, out->pvPrivateData, pvdata_size );
			Mem_Copy( out->pvPrivateData, in->pvPrivateData, pvdata_size );
		}
		else MsgDev( D_ERROR, "SV_CopyEdict: can't copy pvPrivateData\n" );
	}

	Mem_Copy( &out->v, &in->v, sizeof( entvars_t ));	// copy entvars
	out->v.pContainingEntity = out;		// merge contain entity

	return out;
}

edict_t* SV_AllocPrivateData( edict_t *ent, string_t className )
{
	const char	*pszClassName;
	LINK_ENTITY_FUNC	SpawnEdict;

	pszClassName = STRING( className );
	if( !ent ) ent = SV_AllocEdict();
	else if( ent->free )
	{
		MsgDev( D_ERROR, "SV_AllocPrivateData: entity %s is freed!\n", STRING( className ));
		Com_Assert( 1 );
	}
	ent->v.classname = className;
	ent->v.pContainingEntity = ent; // re-link

	// allocate edict private memory (passed by dlls)
	SpawnEdict = (LINK_ENTITY_FUNC)Com_GetProcAddress( svgame.hInstance, pszClassName );
	if( !SpawnEdict )
	{
		// attempt to create custom entity
		if( svgame.dllFuncs.pfnCreate( ent, pszClassName ) == -1 )
		{
			ent->v.flags |= FL_KILLME;
			MsgDev( D_ERROR, "No spawn function for %s\n", STRING( className ));
			return ent; // this edict will be removed from map
		}
	}
	else SpawnEdict( &ent->v );

	Com_Assert( ent->pvServerData == NULL )

	// also register classname to send for client
	ent->pvServerData->s.classname = SV_ClassIndex( pszClassName );

	return ent;
}

void SV_FreeEdicts( void )
{
	int	i = 0;
	edict_t	*ent;

	for( i = 0; i < svgame.globals->numEntities; i++ )
	{
		ent = EDICT_NUM( i );
		if( ent->free ) continue;
		SV_FreeEdict( ent );
	}
}

void SV_PlaybackEvent( sizebuf_t *msg, event_info_t *info )
{
	event_args_t	nullargs;

	Com_Assert( msg == NULL );
	Com_Assert( info == NULL );

	Mem_Set( &nullargs, 0, sizeof( nullargs ));

	MSG_WriteWord( msg, info->index );			// send event index
	MSG_WriteWord( msg, (int)( info->fire_time * 100.0f ));	// send event delay
	MSG_WriteDeltaEvent( msg, &nullargs, &info->args );	// FIXME: zero-compressing
}

bool SV_IsValidEdict( const edict_t *e )
{
	if( !e ) return false;
	if( e->free ) return false;
	if( e == EDICT_NUM( 0 )) return false;	// world is the read-only entity
	if( !e->pvServerData ) return false;
	// edict without pvPrivateData is valid edict
	// server.dll know how allocate it
	return true;
}

const char *SV_ClassName( const edict_t *e )
{
	if( !e ) return "(null)";
	if( e->free ) return "freed";
	return STRING( e->v.classname );
}

static bool SV_IsValidCmd( const char *pCmd )
{
	size_t	len;
                              	
	len = com.strlen( pCmd );

	// valid commands all have a ';' or newline '\n' as their last character
	if( len && ( pCmd[len-1] == '\n' || pCmd[len-1] == ';' ))
		return true;
	return false;
}

sv_client_t *SV_ClientFromEdict( const edict_t *pEdict, bool spawned_only )
{
	sv_client_t	*client;
	int		i;

	if( !SV_IsValidEdict( pEdict ))
		return NULL;

	i = NUM_FOR_EDICT( pEdict ) - 1;
	if( i < 0 || i >= sv_maxclients->integer )
		return NULL;

	if( spawned_only )
	{
		if( svs.clients[i].state != cs_spawned )
			return NULL;
	}
	else
	{
		if( svs.clients[i].state < cs_connected )
			return NULL;
	}

	client = svs.clients + i;

	return client;
}

/*
=========
SV_BaselineForEntity

assume pEdict is valid
=========
*/
void SV_BaselineForEntity( const edict_t *pEdict )
{
	sv_priv_t	*sv_ent;

	sv_ent = pEdict->pvServerData;
	if( !sv_ent ) return;

	// update baseline for new entity
	if( !sv_ent->s.number )
	{
		entity_state_t	*base;

		base = &sv_ent->s;

		if( pEdict->v.modelindex || pEdict->v.effects )
		{
			// take current state as baseline
			SV_UpdateEntityState( pEdict, true );
			svs.baselines[pEdict->serialnumber] = *base;
                    }

		if( sv.state == ss_active && ( base->modelindex || base->soundindex || base->effects ))
		{
			entity_state_t	nullstate;

			Mem_Set( &nullstate, 0, sizeof( nullstate ));
			MSG_WriteByte( &sv.multicast, svc_spawnbaseline );
			MSG_WriteDeltaEntity( &nullstate, base, &sv.multicast, true, true );
			MSG_DirectSend( MSG_ALL, vec3_origin, NULL );
		}
	}
}


/*
=================
SV_ClassifyEdict

sorting edict by type
=================
*/
void SV_ClassifyEdict( edict_t *ent, int m_iNewClass )
{
	sv_priv_t	*sv_ent;

	sv_ent = ent->pvServerData;
	if( !sv_ent ) return;

	// take baseline
	SV_BaselineForEntity( ent );

	if( m_iNewClass != ED_SPAWNED )
	{
		sv_ent->s.ed_type = m_iNewClass;
		return;
	}

	if( sv_ent->s.ed_type != ED_SPAWNED )
		return;

	// auto-classify
	sv_ent->s.ed_type = svgame.dllFuncs.pfnClassifyEdict( ent );

	if( sv_ent->s.ed_type != ED_SPAWNED )
	{
		// or leave unclassified, wait for next SV_LinkEdict...
		// Msg( "AutoClass: %s: <%s>\n", STRING( ent->v.classname ), ed_name[sv_ent->s.ed_type] );
	}
}

void SV_SetClientMaxspeed( sv_client_t *cl, float fNewMaxspeed )
{
	// fakeclients must be changed speed too
	fNewMaxspeed = bound( -svgame.movevars.maxspeed, fNewMaxspeed, svgame.movevars.maxspeed );

	cl->edict->v.maxspeed = fNewMaxspeed;
	if( Info_SetValueForKey( cl->physinfo, "maxspd", va( "%.f", fNewMaxspeed )))
		cl->physinfo_modified = true;
}

/*
===============================================================================

	Game Builtin Functions

===============================================================================
*/
/*
=========
pfnMemAlloc

=========
*/
static void *pfnMemAlloc( size_t cb, const char *filename, const int fileline )
{
	return com.malloc( svgame.private, cb, filename, fileline );
}

/*
=========
pfnMemFree

=========
*/
static void pfnMemFree( void *mem, const char *filename, const int fileline )
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
	int modelIndex = SV_ModelIndex( s );

	CM_RegisterModel( s, modelIndex );

	return modelIndex;
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
	if( !SV_IsValidEdict( e ))
	{
		MsgDev( D_WARN, "SV_SetModel: invalid entity %s\n", SV_ClassName( e ));
		return;
	}

	if( !m || m[0] <= ' ' )
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
	int	numFrames = 0;

	Mod_GetFrames( modelIndex, &numFrames );

	return numFrames;
}

/*
=================
pfnSetSize

=================
*/
void pfnSetSize( edict_t *e, const float *rgflMin, const float *rgflMax )
{
	if( !SV_IsValidEdict( e ))
	{
		MsgDev( D_WARN, "SV_SetSize: invalid entity %s\n", SV_ClassName( e ));
		return;
	}

	// ignore world silently
	if( e == EDICT_NUM( 0 ))
		return;

	SV_SetMinMaxSize( e, rgflMin, rgflMax );
}

/*
=================
pfnChangeLevel

=================
*/
void pfnChangeLevel( const char* s1, const char* s2 )
{
	// make sure we don't issue two changelevels
	if( sv.changelevel ) return;
	if( !s1 || s1[0] <= ' ' ) return;

	if( !s2 ) Cbuf_AddText( va( "changelevel %s\n", s1 ));	// Quake changlevel
	else Cbuf_AddText( va( "changelevel %s %s\n", s1, s2 ));	// Half-Life changelevel
}

/*
=================
pfnVecToYaw

=================
*/
float pfnVecToYaw( const float *rgflVector )
{
	if( !rgflVector ) return 0;
	return SV_VecToYaw( rgflVector );
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

	if( !rgflVectorIn )
	{
		if( rgflVectorOut ) VectorClear( rgflVectorOut );
		return;
	}

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

=================
*/
void pfnMoveToOrigin( edict_t *ent, const float *pflGoal, float dist, int iMoveType )
{
	if( !SV_IsValidEdict( ent ))
	{
		MsgDev( D_WARN, "SV_MoveToOrigin: invalid entity %s\n", SV_ClassName( ent ));
		return;
	}

	SV_MoveToOrigin( ent, pflGoal, dist, iMoveType );
}

/*
==============
pfnChangeYaw

==============
*/
void pfnChangeYaw( edict_t* ent )
{
	float	current;

	if( !SV_IsValidEdict( ent ))
	{
		MsgDev( D_WARN, "SV_ChangeYaw: invalid entity %s\n", SV_ClassName( ent ));
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

	if( !SV_IsValidEdict( ent ))
	{
		MsgDev( D_WARN, "SV_ChangePitch: invalid entity %s\n", SV_ClassName( ent ));
		return;
	}

	current = anglemod( ent->v.angles[PITCH] );
	ent->v.angles[PITCH] = SV_AngleMod( ent->v.ideal_pitch, current, ent->v.pitch_speed );	
}

/*
=========
pfnFindEntityByString

=========
*/
edict_t* pfnFindEntityByString( edict_t *pStartEdict, const char *pszField, const char *pszValue )
{
	int		index = 0, e = 0;
	int		m_iValue;
	float		m_flValue;
	const float	*m_vecValue;
	vec3_t		m_vecValue2;
	edict_t		*ed, *ed2;
	TYPEDESCRIPTION	*desc = NULL;
	const char	*t;

	if( pStartEdict ) e = NUM_FOR_EDICT( pStartEdict );
	if( !pszValue || !*pszValue ) return NULL;

	while((desc = svgame.dllFuncs.pfnGetEntvarsDescirption( index++ )) != NULL )
	{
		if( !com.strcmp( pszField, desc->fieldName ))
			break;
	}

	if( desc == NULL )
	{
		MsgDev( D_ERROR, "SV_FindEntityByString: field %s not found\n", pszField );
		return pStartEdict;
	}

	for( e++; e < svgame.globals->numEntities; e++ )
	{
		ed = EDICT_NUM( e );
		if( ed->free ) continue;

		switch( desc->fieldType )
		{
		case FIELD_STRING:
		case FIELD_MODELNAME:
		case FIELD_SOUNDNAME:
			t = STRING( *(string_t *)&((byte *)&ed->v)[desc->fieldOffset] );
			if( !t ) t = "";
			if( !com.strcmp( t, pszValue ))
				return ed;
			break;
		case FIELD_SHORT:
			m_iValue = *(short *)&((byte *)&ed->v)[desc->fieldOffset];
			if( m_iValue == com.atoi( pszValue ))
				return ed;
			break;
		case FIELD_INTEGER:
		case FIELD_BOOLEAN:
			m_iValue = *(int *)&((byte *)&ed->v)[desc->fieldOffset];
			if( m_iValue == com.atoi( pszValue ))
				return ed;
			break;						
		case FIELD_TIME:
		case FIELD_FLOAT:
			m_flValue = *(int *)&((byte *)&ed->v)[desc->fieldOffset];
			if( m_flValue == com.atof( pszValue ))
				return ed;
			break;
		case FIELD_VECTOR:
		case FIELD_POSITION_VECTOR:
			m_vecValue = (float *)&((byte *)&ed->v)[desc->fieldOffset];
			if( !m_vecValue ) m_vecValue = vec3_origin;
			com.atov( m_vecValue2, pszValue, 3 );
			if( VectorCompare( m_vecValue, m_vecValue2 ))
				return ed;
			break;
		case FIELD_EDICT:
			// NOTE: string must be contain an edict number
			ed2 = (edict_t *)&((byte *)&ed->v)[desc->fieldOffset];
			if( !ed2 ) ed2 = svgame.edicts;
			if( NUM_FOR_EDICT( ed2 ) == com.atoi( pszValue ))
				return ed;
			break;
		}
	}
	return NULL;
}

/*
==============
pfnGetEntityIllum

==============
*/
int pfnGetEntityIllum( edict_t* pEnt )
{
	if( !SV_IsValidEdict( pEnt ))
	{
		MsgDev( D_WARN, "SV_GetEntityIllum: invalid entity %s\n", SV_ClassName( pEnt ));
		return 255;
	}
	return 255; // FIXME: implement
}

/*
=================
pfnFindEntityInSphere

return NULL instead of world
=================
*/
edict_t* pfnFindEntityInSphere( edict_t *pStartEdict, const float *org, float flRadius )
{
	edict_t	*ent;
	float	distSquared;
	float	eorg;
	int	j, e = 0;

	flRadius *= flRadius;

	if( pStartEdict )
		e = NUM_FOR_EDICT( pStartEdict );

	for( e++; e < svgame.globals->numEntities; e++ )
	{
		ent = EDICT_NUM( e );
		if( ent->free ) continue;

		distSquared = 0;
		for( j = 0; j < 3 && distSquared <= flRadius; j++ )
		{
			if( org[j] < ent->v.absmin[j] )
				eorg = org[j] - ent->v.absmin[j];
			else if( org[j] > ent->v.absmax[j] )
				eorg = org[j] - ent->v.absmax[j];
			else eorg = 0;

			distSquared += eorg * eorg;
		}
		if( distSquared > flRadius )
			continue;
		return ent;
	}
	return NULL;
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

	for( i = 1; i < svgame.globals->maxClients; i++ )
	{
		pClient = EDICT_NUM( i );
		if( pClient->free ) continue;
		if( SV_EntitiesIn( DVIS_PVS, pEdict->v.origin, pClient->v.origin ))
			return pEdict;
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

	for( i = 1; i < svgame.globals->maxClients; i++ )
	{
		pClient = EDICT_NUM( i );
		if( pClient->free ) continue;
		if( SV_EntitiesIn( DVIS_PHS, pEdict->v.origin, pClient->v.origin ))
			return pEdict;
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
	int	i;

	chain = NULL;

	if( !pplayer || pplayer->free )
		return chain;

	for( i = svgame.globals->maxClients; i < svgame.globals->numEntities; i++ )
	{
		pEdict = EDICT_NUM( i );
		if( pEdict->free ) continue;
		if( SV_EntitiesIn( DVIS_PVS, pEdict->v.origin, pplayer->v.origin ))
		{
			pEdict->v.chain = chain;
			chain = pEdict;
		}
	}
	return chain;
}

/*
=================
pfnEntitiesInPHS

=================
*/
edict_t* pfnEntitiesInPHS( edict_t *pplayer )
{
	edict_t	*pEdict, *chain;
	int	i;

	chain = NULL;

	if( !pplayer || pplayer->free )
		return chain;

	for( i = svgame.globals->maxClients; i < svgame.globals->numEntities; i++ )
	{
		pEdict = EDICT_NUM( i );
		if( pEdict->free ) continue;
		if( SV_EntitiesIn( DVIS_PHS, pEdict->v.origin, pplayer->v.origin ))
		{
			pEdict->v.chain = chain;
			chain = pEdict;
		}
	}
	return chain;
}

/*
==============
pfnMakeVectors

==============
*/
void pfnMakeVectors( const float *rgflVector )
{
	AngleVectors( rgflVector, svgame.globals->v_forward, svgame.globals->v_right, svgame.globals->v_up );
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
	if( !e || e->free )
	{
		MsgDev( D_ERROR, "SV_RemoveEntity: entity already freed\n" );
		return;
	}

	// never free client or world entity
	if( NUM_FOR_EDICT( e ) < svgame.globals->maxClients )
	{
		MsgDev( D_ERROR, "SV_RemoveEntity: can't delete %s\n", (e == EDICT_NUM( 0 )) ? "world" : "client" );
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
	return SV_AllocPrivateData( NULL, className );
}

/*
=============
pfnMakeStatic

disable entity updates to client
=============
*/
static void pfnMakeStatic( edict_t *ent )
{
	if( !SV_IsValidEdict( ent ))
	{
		MsgDev( D_WARN, "SV_MakeStatic: invalid entity %s\n", SV_ClassName( ent ));
		return;
	}
	ent->pvServerData->s.ed_type = ED_STATIC;
}

/*
=============
pfnLinkEntity

Xash3D extension
=============
*/
static void pfnLinkEntity( edict_t *e, int touch_triggers )
{
	if( !SV_IsValidEdict( e ))
	{
		MsgDev( D_WARN, "SV_LinkEntity: invalid entity %s\n", SV_ClassName( e ));
		return;
	}
	SV_LinkEdict( e, touch_triggers );
}
	
/*
===============
pfnDropToFloor

===============
*/
int pfnDropToFloor( edict_t* e )
{
	vec3_t	end;
	trace_t	trace;

	if( !SV_IsValidEdict( e ))
	{
		MsgDev( D_WARN, "SV_DropToFloor: invalid entity %s\n", SV_ClassName( e ));
		return false;
	}

	VectorCopy( e->v.origin, end );
	end[2] -= 256;
	SV_UnstickEntity( e );

	trace = SV_Move( e->v.origin, e->v.mins, e->v.maxs, end, MOVE_NORMAL, e );

	if( trace.fStartSolid )
	{
		vec3_t	offset, org;

		VectorSet( offset, 0.5f * (e->v.mins[0] + e->v.maxs[0]), 0.5f * (e->v.mins[1] + e->v.maxs[1]), e->v.mins[2] );
		VectorAdd( e->v.origin, offset, org );
		trace = SV_Move( org, vec3_origin, vec3_origin, end, MOVE_NORMAL, e );
		VectorSubtract( trace.vecEndPos, offset, trace.vecEndPos );

		if( trace.fStartSolid )
		{
			MsgDev( D_LOAD, "SV_DropToFloor: startsolid at %g %g %g\n", e->v.origin[0], e->v.origin[1], e->v.origin[2] );
			SV_UnstickEntity( e );
			SV_LinkEdict( e, true );
			e->v.flags |= FL_ONGROUND;
			e->v.groundentity = NULL;
		}
		else if( trace.flFraction < 1.0f )
		{
			MsgDev( D_LOAD, "SV_DropToFloor: moved to %g %g %g\n", e->v.origin[0], e->v.origin[1], e->v.origin[2] );
			VectorCopy( trace.vecEndPos, e->v.origin );
			SV_UnstickEntity( e );
			SV_LinkEdict( e, true );
			e->v.flags |= FL_ONGROUND;
			e->v.groundentity = trace.pHit;
			return true;
		}

		MsgDev( D_LOAD, "SV_DropToFloor: startsolid at %g %g %g\n", e->v.origin[0], e->v.origin[1], e->v.origin[2] );
		pfnRemoveEntity( e );
		return false;
	}
	else
	{
		if( trace.flFraction != 1.0f )
		{
			if( trace.flFraction < 1.0f )
				VectorCopy( trace.vecEndPos, e->v.origin );
			SV_LinkEdict( e, true );
			e->v.flags |= FL_ONGROUND;
			e->v.groundentity = trace.pHit;
			return true;
		}
	}
	return false;
}

/*
===============
pfnWalkMove

===============
*/
int pfnWalkMove( edict_t *ent, float yaw, float dist, int iMode )
{
	vec3_t	move;

	if( !SV_IsValidEdict( ent ))
	{
		MsgDev( D_WARN, "SV_WalkMove: invalid entity %s\n", SV_ClassName( ent ));
		return false;
	}

	if(!( ent->v.flags & (FL_FLY|FL_SWIM|FL_ONGROUND)))
		return false;

	yaw = yaw * M_PI * 2 / 360;
	VectorSet( move, com.cos( yaw ) * dist, com.sin( yaw ) * dist, 0.0f );
	return SV_WalkMove( ent, move, iMode );
}

/*
=================
pfnSetOrigin

=================
*/
void pfnSetOrigin( edict_t *e, const float *rgflOrigin )
{
	if( !SV_IsValidEdict( e ))
	{
		MsgDev( D_WARN, "SV_SetOrigin: invalid entity %s\n", SV_ClassName( e ));
		return;
	}

	VectorCopy( rgflOrigin, e->v.origin );
	SV_LinkEdict( e, false );
}

/*
=================
SV_StartSound

=================
*/
void SV_StartSound( edict_t *ent, int chan, const char *sample, float vol, float attn, int flags, int pitch )
{
	int 	i, sound_idx;
	int	msg_dest = MSG_PAS_R;
	vec3_t	origin;

	if( attn < ATTN_NONE || attn > ATTN_IDLE )
	{
		MsgDev( D_ERROR, "SV_StartSound: attenuation must be in range 0-2\n" );
		return;
	}

	if( chan < 0 || chan > 7 )
	{
		MsgDev( D_ERROR, "SV_StartSound: channel must be in range 0-7\n" );
		return;
	}

	if( !SV_IsValidEdict( ent ))
	{
		MsgDev( D_ERROR, "SV_StartSound: edict == NULL\n" );
		return;
	}

	if( sv.state == ss_loading ) flags |= SND_SPAWNING;
	if( vol != VOL_NORM ) flags |= SND_VOLUME;
	if( attn != ATTN_NONE ) flags |= SND_SOUNDLEVEL;
	if( pitch != PITCH_NORM ) flags |= SND_PITCH;

	// ultimate method for detect bsp models with invalid solidity (e.g. func_pushable)
	if( CM_GetModelType( ent->v.modelindex ) == mod_brush )
	{
		for( i = 0; i < 3; i++ )
			origin[i] = ent->v.origin[i] + 0.5f * ( ent->v.mins[i] + ent->v.maxs[i] );

		if( flags & SND_SPAWNING )
			msg_dest = MSG_INIT;
		else msg_dest = MSG_ALL;
	}
	else
	{
		for( i = 0; i < 3; i++ )
			origin[i] = ent->v.origin[i];

		if( flags & SND_SPAWNING )
			msg_dest = MSG_INIT;
		else msg_dest = MSG_PAS;
	}

	// always sending stop sound command
	if( flags & SND_STOP ) msg_dest = MSG_ALL;

	// precache_sound can be used twice: cache sounds when loading
	// and return sound index when server is active
	sound_idx = SV_SoundIndex( sample );

	MSG_Begin( svc_sound );
	MSG_WriteWord( &sv.multicast, flags );
	MSG_WriteWord( &sv.multicast, sound_idx );
	MSG_WriteByte( &sv.multicast, chan );

	if ( flags & SND_VOLUME ) MSG_WriteByte( &sv.multicast, vol * 255 );
	if ( flags & SND_SOUNDLEVEL ) MSG_WriteByte( &sv.multicast, ATTN_TO_SNDLVL( attn ));
	if ( flags & SND_PITCH ) MSG_WriteByte( &sv.multicast, pitch );

	// plays from aiment
	if( ent->v.aiment && !ent->v.aiment->free )
		MSG_WriteWord( &sv.multicast, ent->v.aiment->serialnumber );
	else MSG_WriteWord( &sv.multicast, ent->serialnumber );

	MSG_Send( msg_dest, origin, NULL );
}

/*
=================
pfnEmitAmbientSound

=================
*/
void pfnEmitAmbientSound( edict_t *ent, float *pos, const char *samp, float vol, float attn, int flags, int pitch )
{
	int 	i, number, sound_idx;
	int	msg_dest = MSG_PAS_R;
	vec3_t	origin;

	if( attn < ATTN_NONE || attn > ATTN_IDLE )
	{
		MsgDev( D_ERROR, "SV_AmbientSound: attenuation must be in range 0-2\n" );
		return;
	}

	if( !pos )
	{
		MsgDev( D_ERROR, "SV_AmbientSound: pos == NULL!\n" );
		return;
	}

	if( sv.state == ss_loading ) flags |= SND_SPAWNING;
	if( vol != VOL_NORM ) flags |= SND_VOLUME;
	if( attn != ATTN_NONE ) flags |= SND_SOUNDLEVEL;
	if( pitch != PITCH_NORM ) flags |= SND_PITCH;

	// ultimate method for detect bsp models with invalid solidity (e.g. func_pushable)
	if( SV_IsValidEdict( ent ) && CM_GetModelType( ent->v.modelindex ) == mod_brush )
	{
		for( i = 0; i < 3; i++ )
			origin[i] = ent->v.origin[i] + 0.5f * ( ent->v.mins[i] + ent->v.maxs[i] );

		if( flags & SND_SPAWNING )
			msg_dest = MSG_INIT;
		else msg_dest = MSG_PAS;
		number = ent->serialnumber;
	}
	else
	{
		for( i = 0; i < 3; i++ )
			origin[i] = pos[i];

		if( flags & SND_SPAWNING )
			msg_dest = MSG_INIT;
		else msg_dest = MSG_PAS;
		number = 0;
	}

	// always sending stop sound command
	if( flags & SND_STOP ) msg_dest = MSG_ALL;
	flags |= SND_FIXED_ORIGIN;

	// precache_sound can be used twice: cache sounds when loading
	// and return sound index when server is active
	sound_idx = SV_SoundIndex( samp );

	MSG_Begin( svc_sound );
	MSG_WriteWord( &sv.multicast, flags );
	MSG_WriteWord( &sv.multicast, sound_idx );
	MSG_WriteByte( &sv.multicast, CHAN_AUTO );

	if ( flags & SND_VOLUME ) MSG_WriteByte( &sv.multicast, vol * 255 );
	if ( flags & SND_SOUNDLEVEL ) MSG_WriteByte( &sv.multicast, ATTN_TO_SNDLVL( attn ));
	if ( flags & SND_PITCH ) MSG_WriteByte( &sv.multicast, pitch );

	// plays from fixed position
	MSG_WriteWord( &sv.multicast, number );
	MSG_WritePos( &sv.multicast, pos );

	MSG_Send( msg_dest, origin, NULL );
}

/*
=================
pfnTraceLine

=================
*/
static void pfnTraceLine( const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr )
{
	trace_t	result;

	if( svgame.globals->trace_flags & 1 )
		fNoMonsters |= FTRACE_SIMPLEBOX;
	svgame.globals->trace_flags = 0;

	if( VectorIsNAN( v1 ) || VectorIsNAN( v2 ))
		Host_Error( "TraceLine: NAN errors detected '%f %f %f', '%f %f %f'\n", v1[0], v1[1], v1[2], v2[0], v2[1], v2[2] );
	result = SV_Move( v1, vec3_origin, vec3_origin, v2, fNoMonsters, pentToSkip );
	if( ptr ) Mem_Copy( ptr, &result, sizeof( *ptr ));
}

/*
=================
pfnTraceToss

=================
*/
static void pfnTraceToss( edict_t* pent, edict_t* pentToIgnore, TraceResult *ptr )
{
	trace_t	result;

	if( !SV_IsValidEdict( pent ))
	{
		MsgDev( D_WARN, "SV_MoveToss: invalid entity %s\n", SV_ClassName( pent ));
		return;
	}

	result = SV_MoveToss( pent, pentToIgnore );
	if( ptr ) Mem_Copy( ptr, &result, sizeof( *ptr ));
}

/*
=================
pfnTraceHull

=================
*/
static void pfnTraceHull( const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr )
{
	trace_t	result;
	float	*mins, *maxs;

	hullNumber = bound( 0, hullNumber, 3 );
	mins = GI->client_mins[hullNumber];
	maxs = GI->client_maxs[hullNumber];

	if( svgame.globals->trace_flags & 1 )
		fNoMonsters |= FTRACE_SIMPLEBOX;
	svgame.globals->trace_flags = 0;

	if( VectorIsNAN( v1 ) || VectorIsNAN( v2 ))
		Host_Error( "TraceHull: NAN errors detected '%f %f %f', '%f %f %f'\n", v1[0], v1[1], v1[2], v2[0], v2[1], v2[2] );
	result = SV_Move( v1, mins, maxs, v2, fNoMonsters, pentToSkip );
	if( ptr ) Mem_Copy( ptr, &result, sizeof( *ptr ));
}

/*
=============
pfnTraceMonsterHull

=============
*/
static int pfnTraceMonsterHull( edict_t *pEdict, const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr )
{
	trace_t	result;
	float	*mins, *maxs;

	if( !SV_IsValidEdict( pEdict ))
	{
		MsgDev( D_WARN, "SV_TraceMonsterHull: invalid entity %s\n", SV_ClassName( pEdict ));
		return 1;
	}

	if( svgame.globals->trace_flags & 1 )
		fNoMonsters |= FTRACE_SIMPLEBOX;
	svgame.globals->trace_flags = 0;

	mins = pEdict->v.mins;
	maxs = pEdict->v.maxs;

	if( VectorIsNAN( v1 ) || VectorIsNAN( v2 ))
		Host_Error( "TraceMonsterHull: NAN errors detected '%f %f %f', '%f %f %f'\n", v1[0], v1[1], v1[2], v2[0], v2[1], v2[2] );
	result = SV_Move( v1, mins, maxs, v2, fNoMonsters, pentToSkip );
	if( ptr ) Mem_Copy( ptr, &result, sizeof( *ptr ));

	return ptr->fAllSolid;
}

/*
=============
pfnTraceModel

=============
*/
static void pfnTraceModel( const float *v1, const float *v2, edict_t *pent, TraceResult *ptr )
{
	trace_t	result;

	if( !SV_IsValidEdict( pent ))
	{
		MsgDev( D_WARN, "SV_TraceModel: invalid entity %s\n", SV_ClassName( pent ));
		return;
	}

	if( VectorIsNAN( v1 ) || VectorIsNAN( v2 ))
		Host_Error( "TraceModel: NAN errors detected '%f %f %f', '%f %f %f'\n", v1[0], v1[1], v1[2], v2[0], v2[1], v2[2] );
	result = SV_ClipMoveToEntity( pent, v1, pent->v.mins, pent->v.maxs, v2, MASK_SOLID, 0 );
	if( ptr ) Mem_Copy( ptr, &result, sizeof( *ptr ));
}

/*
=============
pfnTraceTexture

returns texture basename
=============
*/
static const char *pfnTraceTexture( edict_t *pTextureEntity, const float *v1, const float *v2 )
{
	if( VectorIsNAN( v1 ) || VectorIsNAN( v2 ))
		Host_Error( "TraceTexture: NAN errors detected '%f %f %f', '%f %f %f'\n", v1[0], v1[1], v1[2], v2[0], v2[1], v2[2] );
	if( !pTextureEntity || pTextureEntity->free ) return NULL; 
	return SV_ClipMoveToEntity( pTextureEntity, v1, vec3_origin, vec3_origin, v2, MASK_SOLID, 0 ).pTexName;
}

/*
=============
pfnTraceSphere

FIXME: implement
=============
*/
static void pfnTraceSphere( const float *v1, const float *v2, int fNoMonsters, float radius, edict_t *pentToSkip, TraceResult *ptr )
{
	Host_Error( "not implemented\n" );
}

/*
=============
pfnGetAimVector

FIXME: use speed for reduce aiming accuracy
=============
*/
void pfnGetAimVector( edict_t* ent, float speed, float *rgflReturn )
{
	edict_t		*check, *bestent;
	vec3_t		start, dir, end, bestdir;
	float		dist, bestdist;
	bool		fNoFriendlyFire;
	int		i, j;
	trace_t		tr;

	// these vairable defined in server.dll	
	fNoFriendlyFire = Cvar_VariableValue( "mp_friendlyfire" );
	VectorCopy( svgame.globals->v_forward, rgflReturn );	// assume failure if it returns early

	if( !SV_IsValidEdict( ent ))
	{
		MsgDev( D_WARN, "SV_GetAimVector: invalid entity %s\n", SV_ClassName( ent ));
		return;
	}

	VectorCopy( ent->v.origin, start );
	start[2] += 20;

	// try sending a trace straight
	VectorCopy( svgame.globals->v_forward, dir );
	VectorMA( start, 2048, dir, end );
	tr = SV_Move( start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent );

	if( tr.pHit && (tr.pHit->v.takedamage == DAMAGE_AIM && fNoFriendlyFire || ent->v.team <= 0 || ent->v.team != tr.pHit->v.team ))
	{
		VectorCopy( svgame.globals->v_forward, rgflReturn );
		return;
	}

	// try all possible entities
	VectorCopy( dir, bestdir );
	bestdist = 0.5f;
	bestent = NULL;

	check = EDICT_NUM( 1 ); // start at first client
	for( i = 1; i < svgame.globals->numEntities; i++, check++ )
	{
		if( check->v.takedamage != DAMAGE_AIM ) continue;
		if( check == ent ) continue;
		if( fNoFriendlyFire && ent->v.team > 0 && ent->v.team == check->v.team )
			continue;	// don't aim at teammate
		for( j = 0; j < 3; j++ )
			end[j] = check->v.origin[j] + 0.5 * (check->v.mins[j] + check->v.maxs[j]);
		VectorSubtract( end, start, dir );
		VectorNormalize( dir );
		dist = DotProduct( dir, svgame.globals->v_forward );
		if( dist < bestdist ) continue; // to far to turn
		tr = SV_Move( start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent );
		if( tr.pHit == check )
		{	
			// can shoot at this one
			bestdist = dist;
			bestent = check;
		}
	}

	if( bestent )
	{
		VectorSubtract( bestent->v.origin, ent->v.origin, dir );
		dist = DotProduct( dir, svgame.globals->v_forward );
		VectorScale( svgame.globals->v_forward, dist, end );
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
	if( SV_IsValidCmd( str )) Cbuf_AddText( str );
	else MsgDev( D_ERROR, "bad server command %s\n", str );
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

	if( sv.state != ss_active )
	{
		MsgDev( D_ERROR, "SV_ClientCommand: server is not active!\n" );
		return;
	}

	client = SV_ClientFromEdict( pEdict, true );
	if( client == NULL )
	{
		MsgDev( D_ERROR, "SV_ClientCommand: client is not spawned!\n" );
		return;
	}

	if( pEdict->v.flags & FL_FAKECLIENT )
		return;

	va_start( args, szFmt );
	com.vsnprintf( buffer, MAX_STRING, szFmt, args );
	va_end( args );

	if( SV_IsValidCmd( buffer ))
	{
		MSG_WriteByte( &sv.multicast, svc_stufftext );
		MSG_WriteString( &sv.multicast, buffer );
		MSG_Send( MSG_ONE, NULL, client->edict );
	}
	else MsgDev( D_ERROR, "Tried to stuff bad command %s\n", buffer );
}

/*
=================
pfnParticleEffect

Make sure the event gets sent to all clients
=================
*/
void pfnParticleEffect( const float *org, const float *dir, float color, float count )
{
	int	i, v;

	if( !org || !dir )
	{
		if( !org ) MsgDev( D_ERROR, "SV_StartParticle: NULL origin. Ignored\n" );
		if( !dir ) MsgDev( D_ERROR, "SV_StartParticle: NULL dir. Ignored\n" );
		return;
	}

	MSG_WriteByte( &sv.multicast, svc_particle );
	MSG_WritePos( &sv.multicast, org );

	for( i = 0; i < 3; i++ )
	{
		v = bound( -128, dir[i] * 16, 127 );
		MSG_WriteChar( &sv.multicast, v );
	}
	MSG_WriteByte( &sv.multicast, count );
	MSG_WriteByte( &sv.multicast, color );
	MSG_Send( MSG_ALL, org, NULL );
}

/*
===============
pfnLightStyle

===============
*/
void pfnLightStyle( int style, const char* val )
{
	if( style < 0 ) style = 0;
	if( style >= MAX_LIGHTSTYLES )
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
static int pfnPointContents( const float *rgflVector )
{
	return SV_PointContents( rgflVector );
}

/*
=============
pfnMessageBegin

=============
*/
void pfnMessageBegin( int msg_dest, int msg_num, const float *pOrigin, edict_t *ed )
{
	if( svgame.msg_started )
		Host_Error( "MessageBegin: New message started when msg '%s' has not been sent yet\n", svgame.msg_name );
	svgame.msg_started = true;

	// some malicious users can send message with engine index
	// reduce number to avoid overflow problems or cheating
	svgame.msg_index = bound( svc_bad, msg_num, svc_nop );

	if( svgame.msg_index >= 0 && svgame.msg_index < MAX_USER_MESSAGES )
		svgame.msg_name = sv.configstrings[CS_USER_MESSAGES + svgame.msg_index];
	else svgame.msg_name = NULL;

	MSG_Begin( svgame.msg_index );

	// save message destination
	if( pOrigin ) VectorCopy( pOrigin, svgame.msg_org );
	else VectorClear( svgame.msg_org );

	if( svgame.msg_sizes[msg_num] == -1 )
	{
		// variable sized messages sent size as first byte
		svgame.msg_size_index = sv.multicast.cursize;
		MSG_WriteByte( &sv.multicast, 0 ); // reserve space for now
	}
	else svgame.msg_size_index = -1;

	svgame.msg_realsize = 0;
	svgame.msg_dest = msg_dest;
	svgame.msg_ent = ed;
}

/*
=============
pfnMessageEnd

=============
*/
void pfnMessageEnd( void )
{
	const char	*name = "Unknown";
	float		*org = NULL;

	if( svgame.msg_name ) name = svgame.msg_name;
	if( !svgame.msg_started )
		Host_Error( "MessageEnd: called with no active message\n" );
	svgame.msg_started = false;

	if( svgame.msg_sizes[svgame.msg_index] != -1 )
	{
		int expsize = svgame.msg_sizes[svgame.msg_index];
		int realsize = svgame.msg_realsize;
	
		// compare bounds
		if( expsize != realsize )
		{
			MsgDev( D_ERROR, "SV_Message: %s expected %i bytes, it written %i\n", name, expsize, realsize );
			MSG_Clear( &sv.multicast );
			return;
		}
	}
	else if( svgame.msg_size_index != -1 )
	{
		// variable sized message
		if( svgame.msg_realsize > 255 )
		{
			MsgDev( D_ERROR, "SV_Message: %s too long (more than 255 bytes)\n", name );
			MSG_Clear( &sv.multicast );
			return;
		}
		else if( svgame.msg_realsize <= 0 )
		{
			MsgDev( D_ERROR, "SV_Message: %s writes NULL message\n", name );
			MSG_Clear( &sv.multicast );
			return;
		}
		sv.multicast.data[svgame.msg_size_index] = svgame.msg_realsize;
	}
	else
	{
		// this should never happen
		MsgDev( D_ERROR, "SV_Message: %s have encountered error\n", name );
		MSG_Clear( &sv.multicast );
		return;
	}

	if( !VectorIsNull( svgame.msg_org )) org = svgame.msg_org;
	svgame.msg_dest = bound( MSG_BROADCAST, svgame.msg_dest, MSG_SPEC );
	MSG_Send( svgame.msg_dest, org, svgame.msg_ent );
}

/*
=============
pfnWriteByte

=============
*/
void pfnWriteByte( int iValue )
{
	if( iValue == -1 ) iValue = 0xFF; // convert char to byte 
	_MSG_WriteBits( &sv.multicast, (int)iValue, svgame.msg_name, NET_BYTE, __FILE__, __LINE__ );
	svgame.msg_realsize++;
}

/*
=============
pfnWriteChar

=============
*/
void pfnWriteChar( int iValue )
{
	_MSG_WriteBits( &sv.multicast, (int)iValue, svgame.msg_name, NET_CHAR, __FILE__, __LINE__ );
	svgame.msg_realsize++;
}

/*
=============
pfnWriteShort

=============
*/
void pfnWriteShort( int iValue )
{
	_MSG_WriteBits( &sv.multicast, (int)iValue, svgame.msg_name, NET_SHORT, __FILE__, __LINE__ );
	svgame.msg_realsize += 2;
}

/*
=============
pfnWriteLong

=============
*/
void pfnWriteLong( int iValue )
{
	_MSG_WriteBits( &sv.multicast, (int)iValue, svgame.msg_name, NET_LONG, __FILE__, __LINE__ );
	svgame.msg_realsize += 4;
}

/*
=============
pfnWriteAngle

=============
*/
void pfnWriteAngle( float flValue )
{
	MSG_WriteAngle16( &sv.multicast, flValue );
	svgame.msg_realsize += 2;
}

/*
=============
pfnWriteCoord

=============
*/
void pfnWriteCoord( float flValue )
{
	ftol_t	dat;

	dat.f = flValue;
	_MSG_WriteBits( &sv.multicast, dat.l, svgame.msg_name, NET_FLOAT, __FILE__, __LINE__ );
	svgame.msg_realsize += 4;
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

	// NOTE: some messages with constant string length can be marked as known sized
	svgame.msg_realsize += total_size;
}

/*
=============
pfnWriteEntity

=============
*/
void pfnWriteEntity( int iValue )
{
	// edict -1 it's a viewmodel entity
	if( iValue < -1 || iValue > svgame.globals->numEntities )
		Host_Error( "MSG_WriteEntity: invalid entnumber %d\n", iValue );
	MSG_WriteShort( &sv.multicast, iValue );
	svgame.msg_realsize += 2;
}

/*
=============
pfnPvAllocEntPrivateData

=============
*/
void *pfnPvAllocEntPrivateData( edict_t *pEdict, long cb )
{
	Com_Assert( pEdict == NULL );
	Com_Assert( pEdict->free );
	Com_Assert( pEdict->pvServerData == NULL );

	// to avoid multiple alloc
	pEdict->pvPrivateData = (void *)Mem_Realloc( svgame.private, pEdict->pvPrivateData, cb );
	pEdict->pvServerData->pvdata_size = cb;	

	return pEdict->pvPrivateData;
}

/*
=============
pfnPvEntPrivateData

=============
*/
void *pfnPvEntPrivateData( edict_t *pEdict )
{
	if( pEdict )
		return pEdict->pvPrivateData;
	return NULL;
}

/*
=============
pfnFreeEntPrivateData

=============
*/
void pfnFreeEntPrivateData( edict_t *pEdict )
{
	if( !pEdict ) return;
	if( pEdict->pvPrivateData )
		Mem_Free( pEdict->pvPrivateData );
	pEdict->pvPrivateData = NULL; // freed
}

/*
=============
SV_AllocString

=============
*/
string_t SV_AllocString( const char *szValue )
{
	return StringTable_SetString( svgame.hStringTable, szValue );
}		

/*
=============
SV_GetString

=============
*/
const char *SV_GetString( string_t iString )
{
	return StringTable_GetString( svgame.hStringTable, iString );
}

/*
=============
pfnGetVarsOfEnt

=============
*/
entvars_t *pfnGetVarsOfEnt( edict_t *pEdict )
{
	if( pEdict )
		return &pEdict->v;
	return NULL;
}

/*
=============
pfnPEntityOfEntOffset

=============
*/
edict_t* pfnPEntityOfEntOffset( int iEntOffset )
{
	return (&((edict_t*)svgame.vp)[iEntOffset]);
}

/*
=============
pfnEntOffsetOfPEntity

=============
*/
int pfnEntOffsetOfPEntity( const edict_t *pEdict )
{
	return ((byte *)pEdict - (byte *)svgame.vp);
}

/*
=============
pfnIndexOfEdict

=============
*/
int pfnIndexOfEdict( const edict_t *pEdict )
{
	if( !pEdict || pEdict->free )
		return NULLENT_INDEX;
	return NUM_FOR_EDICT( pEdict );
}

/*
=============
pfnPEntityOfEntIndex

=============
*/
edict_t* pfnPEntityOfEntIndex( int iEntIndex )
{
	if( iEntIndex < 0 || iEntIndex >= svgame.globals->numEntities )
		return NULL; // out of range
	return EDICT_NUM( iEntIndex );
}

/*
=============
pfnFindEntityByVars

debug routine
=============
*/
edict_t* pfnFindEntityByVars( entvars_t *pvars )
{
	edict_t	*e = EDICT_NUM( 0 );
	int	i;

	for( i = 0; i < svgame.globals->numEntities; i++, e++ )
	{
		if( e->free ) continue;
		if( !memcmp( &e->v, pvars, sizeof( entvars_t )))
			return e;	// found it
	}
	return NULL;
}

/*
=============
pfnGetModelPtr

returns pointer to a studiomodel
=============
*/
static void *pfnGetModelPtr( edict_t* pEdict )
{
	if( !pEdict || pEdict->free )
		return NULL;
	return Mod_Extradata( pEdict->v.modelindex );
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
	string	msg_name;

	// scan name for reserved symbol
	if( com.strchr( pszName, '@' ))
	{
		MsgDev( D_ERROR, "SV_RegisterUserMessage: invalid name %s\n", pszName );
		return svc_bad; // force error
	}

	// build message name, fmt: MsgName@size
	com.snprintf( msg_name, MAX_STRING, "%s@%i", pszName, iSize );
	msg_index = SV_UserMessageIndex( msg_name );
	svgame.msg_sizes[msg_index] = iSize;

	return msg_index;
}

/*
=============
pfnAreaPortal

changes area portal state
=============
*/
void pfnAreaPortal( edict_t *pEdict, int enable )
{
	if( pEdict == EDICT_NUM( 0 )) return;
	if( pEdict->free )
	{
		MsgDev( D_ERROR, "SV_AreaPortal: can't modify free entity\n" );
		return;
	}
	if( !pEdict->pvServerData->areanum || !pEdict->pvServerData->areanum2 ) return;
	CM_SetAreaPortalState( pEdict->serialnumber, pEdict->pvServerData->areanum, pEdict->pvServerData->areanum2, enable );
}

/*
=============
pfnGetBonePosition

=============
*/
static void pfnGetBonePosition( const edict_t* pEdict, int iBone, float *rgflOrigin, float *rgflAngles )
{
	if( !SV_IsValidEdict( pEdict ))
	{
		MsgDev( D_WARN, "SV_GetBonePos: invalid entity %s\n", SV_ClassName( pEdict ));
		return;
	}

	CM_GetBonePosition( (edict_t *)pEdict, iBone, rgflOrigin, rgflAngles );
}

/*
=============
pfnFunctionFromName

=============
*/
dword pfnFunctionFromName( const char *pName )
{
	int	i, index;

	for( i = 0; i < svgame.num_ordinals; i++ )
	{
		if( !com.strcmp( pName, svgame.names[i] ))
		{
			index = svgame.ordinals[i];
			return svgame.funcs[index] + svgame.funcBase;
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

	for( i = 0; i < svgame.num_ordinals; i++ )
	{
		index = svgame.ordinals[i];

		if((function - svgame.funcBase) == svgame.funcs[index] )
			return svgame.names[i];
	}

	// couldn't find the function address to return name
	return NULL;
}

/*
=============
pfnClientPrintf

=============
*/
void pfnClientPrintf( edict_t* pEdict, PRINT_TYPE ptype, const char *szMsg )
{
	sv_client_t	*client;
	bool		fake;

	if( sv.state != ss_active )
	{
		// send message into console during loading
		MsgDev( D_INFO, szMsg );
		return;
	}

	client = SV_ClientFromEdict( pEdict, true );
	if( client == NULL )
	{
		MsgDev( D_ERROR, "SV_ClientPrintf: client is not spawned!\n" );
		return;
	}

	fake = ( pEdict->v.flags & FL_FAKECLIENT ) ? true : false;

	switch( ptype )
	{
	case print_console:
		if( fake ) MsgDev( D_INFO, szMsg );
		else SV_ClientPrintf( client, PRINT_HIGH, "%s", szMsg );
		break;
	case print_chat:
		if( fake ) return;
		SV_ClientPrintf( client, PRINT_CHAT, "%s", szMsg );
		break;
	case print_center:
		if( fake ) return;
		MSG_Begin( svc_centerprint );
		MSG_WriteString( &sv.multicast, szMsg );
		MSG_Send( MSG_ONE, NULL, client->edict );
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
	// while loading in-progress we can sending message only for local client
	if( sv.state == ss_loading ) com.print( szMsg );	
	else SV_BroadcastPrintf( PRINT_HIGH, "%s", szMsg );
}

/*
=============
pfnGetAttachment

=============
*/
static void pfnGetAttachment( const edict_t *pEdict, int iAttachment, float *rgflOrigin, float *rgflAngles )
{
	if( !SV_IsValidEdict( pEdict ))
	{
		MsgDev( D_WARN, "SV_GetAttachment: invalid entity %s\n", SV_ClassName( pEdict ));
		return;
	}
	CM_GetAttachment(( edict_t *)pEdict, iAttachment, rgflOrigin, rgflAngles );
}

/*
=============
pfnCRC32_Init

=============
*/
void pfnCRC32_Init( CRC32_t *pulCRC )
{
	CRC32_Init( pulCRC );
}

/*
=============
pfnCRC32_ProcessBuffer

=============
*/
void pfnCRC32_ProcessBuffer( CRC32_t *pulCRC, void *p, int len )
{
	CRC32_ProcessBuffer( pulCRC, p, len );
}

/*
=============
pfnCRC32_ProcessByte

=============
*/
void pfnCRC32_ProcessByte( CRC32_t *pulCRC, byte ch )
{
	CRC32_ProcessByte( pulCRC, ch );
}

/*
=============
pfnCRC32_Final

=============
*/
CRC32_t pfnCRC32_Final( CRC32_t pulCRC )
{
	CRC32_Final( &pulCRC );

	return pulCRC;
}				

/*
=============
pfnCrosshairAngle

=============
*/
void pfnCrosshairAngle( const edict_t *pClient, float pitch, float yaw )
{
	sv_client_t	*client;

	client = SV_ClientFromEdict( pClient, true );
	if( client == NULL )
	{
		MsgDev( D_ERROR, "SV_SetCrosshairAngle: invalid client!\n" );
		return;
	}

	// fakeclients ignore it silently
	if( pClient->v.flags & FL_FAKECLIENT ) return;

	MSG_Begin( svc_crosshairangle );
	MSG_WriteAngle8( &sv.multicast, pitch );
	MSG_WriteAngle8( &sv.multicast, yaw );
	MSG_Send( MSG_ONE, vec3_origin, pClient );
}

/*
=============
pfnSetView

=============
*/
void pfnSetView( const edict_t *pClient, const edict_t *pViewent )
{
	sv_client_t	*client;

	if( pClient == NULL || pClient->free )
	{
		MsgDev( D_ERROR, "SV_SetView: invalid client!\n" );
		return;
	}

	client = pClient->pvServerData->client;
	if( !client )
	{
		MsgDev( D_ERROR, "SV_SetView: not a client!\n" );
		return;
	}

	if( pViewent == NULL || pViewent->free )
	{
		MsgDev( D_ERROR, "SV_SetView: invalid viewent!\n" );
		return;
	}

	if( pClient == pViewent ) client->pViewEntity = NULL;
	else client->pViewEntity = (edict_t *)pViewent;

	// fakeclients ignore to send client message
	if( pClient->v.flags & FL_FAKECLIENT ) return;

	MSG_WriteByte( &client->netchan.message, svc_setview );
	MSG_WriteWord( &client->netchan.message, NUM_FOR_EDICT( pViewent ));
	MSG_Send( MSG_ONE, NULL, client->edict );
}

/*
=============
pfnCompareFileTime

=============
*/
int pfnCompareFileTime( const char *filename1, const char *filename2, int *iCompare )
{
	int	bRet = 0;

	*iCompare = 0;

	if( filename1 && filename2 )
	{
		long ft1 = FS_FileTime( filename1 );
		long ft2 = FS_FileTime( filename2 );

		*iCompare = Host_CompareFileTime( ft1,  ft2 );
		bRet = 1;
	}
	return bRet;
}

/*
=============
pfnStaticDecal

=============
*/
void pfnStaticDecal( const float *origin, int decalIndex, int entityIndex, int modelIndex )
{
	if( !origin )
	{
		MsgDev( D_ERROR, "SV_StaticDecal: NULL origin. Ignored\n" );
		return;
	}

	// static decals are posters, it's always reliable
	MSG_WriteByte( &sv.multicast, svc_bspdecal );
	MSG_WritePos( &sv.multicast, origin );
	MSG_WriteWord( &sv.multicast, decalIndex );
	MSG_WriteShort( &sv.multicast, entityIndex );
	if( entityIndex > 0 )
		MSG_WriteWord( &sv.multicast, modelIndex );
	MSG_Send( MSG_INIT, NULL, NULL );
}

/*
=============
pfnPrecacheGeneric

can be used for precache scripts
=============
*/
int pfnPrecacheGeneric( const char *s )
{
	return SV_GenericIndex( s );
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
	char	*spawn_entity;

	// determine spawn entity classname
	if( Cvar_VariableInteger( "deathmatch" ))
		spawn_entity = GI->dm_entity;
	else if( Cvar_VariableInteger( "coop" ))
		spawn_entity = GI->coop_entity;
	else if( Cvar_VariableInteger( "teamplay" ))
		spawn_entity = GI->team_entity;
	else spawn_entity = GI->sp_entity;

	return SV_MapIsValid( filename, spawn_entity );
}

/*
=============
pfnClassifyEdict

classify edict for render and network usage
=============
*/
void pfnClassifyEdict( edict_t *pEdict, int class )
{
	if( !SV_IsValidEdict( pEdict ))
	{
		MsgDev( D_WARN, "SV_ClassifyEdict: invalid entity %s\n", SV_ClassName( pEdict ));
		return;
	}
	SV_ClassifyEdict( pEdict, class );
}

/*
=============
pfnFadeClientVolume

=============
*/
void pfnFadeClientVolume( const edict_t *pEdict, float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds )
{
	sv_client_t *cl;

	cl = SV_ClientFromEdict( pEdict, true );
	if( !cl )
	{
		MsgDev( D_ERROR, "SV_FadeClientVolume: client is not spawned!\n" );
		return;
	}

	MSG_WriteByte( &sv.multicast, svc_soundfade );
	MSG_WriteFloat( &sv.multicast, fadePercent );
	MSG_WriteFloat( &sv.multicast, fadeOutSeconds );
	MSG_WriteFloat( &sv.multicast, holdTime );
	MSG_WriteFloat( &sv.multicast, fadeInSeconds );
	MSG_Send( MSG_ONE, NULL, cl->edict );
}

/*
=============
pfnSetClientMaxspeed

=============
*/
void pfnSetClientMaxspeed( const edict_t *pEdict, float fNewMaxspeed )
{
	sv_client_t *cl;

	cl = SV_ClientFromEdict( pEdict, false ); // connected clients allowed
	if( !cl )
	{
		MsgDev( D_ERROR, "SV_SetClientMaxspeed: client is not spawned!\n" );
		return;
	}

	SV_SetClientMaxspeed( cl, fNewMaxspeed );
}

/*
=============
pfnCreateFakeClient

=============
*/
edict_t *pfnCreateFakeClient( const char *netname )
{
	return SV_FakeConnect( netname );
}

/*
=============
pfnThinkFakeClient

=============
*/
void pfnThinkFakeClient( edict_t *pClient, usercmd_t *cmd )
{
	sv_client_t	*cl;

	if( sv.paused ) return;

	cl = SV_ClientFromEdict( pClient, true );
	if( cl == NULL || cmd == NULL )
	{
		if( !cl ) MsgDev( D_ERROR, "SV_ClientThink: fakeclient is not spawned!\n" );
		if( !cmd )  MsgDev( D_ERROR, "SV_ClientThink: NULL usercmd_t!\n" );
		return;
	}

	if(!( pClient->v.flags & FL_FAKECLIENT ))
		return;	// only fakeclients allows

	SV_PreRunCmd( cl, cmd );
	SV_RunCmd( cl, cmd );
	SV_PostRunCmd( cl );

	cl->lastcmd = *cmd;
	cl->lastcmd.buttons = 0; // avoid multiple fires on lag
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
	sv_client_t	*cl;

	cl = SV_ClientFromEdict( e, false ); // pfnUserInfoChanged passed
	if( cl == NULL )
	{
		MsgDev( D_ERROR, "SV_GetClientUserinfo: client is not connected!\n" );
		return Cvar_Serverinfo(); // otherwise return ServerInfo
	}
	return cl->userinfo;
}

/*
=============
pfnSetClientKeyValue

=============
*/
void pfnSetClientKeyValue( int clientIndex, char *infobuffer, char *key, char *value )
{
	sv_client_t	*cl;

	if( clientIndex < 0 || clientIndex >= sv_maxclients->integer )
		return;
	if( svs.clients[clientIndex].state < cs_spawned )
		return;

	cl = svs.clients + clientIndex;
	Info_SetValueForKey( cl->userinfo, key, value );
	cl->sendinfo = true;
}

/*
=============
pfnGetPhysicsKeyValue

=============
*/
const char *pfnGetPhysicsKeyValue( const edict_t *pClient, const char *key )
{
	sv_client_t	*cl;

	cl = SV_ClientFromEdict( pClient, false ); // pfnUserInfoChanged passed
	if( cl == NULL )
	{
		MsgDev( D_ERROR, "SV_GetClientPhysKey: client is not connected!\n" );
		return "";
	}
	return Info_ValueForKey( cl->physinfo, key );
}

/*
=============
pfnSetPhysicsKeyValue

=============
*/
void pfnSetPhysicsKeyValue( const edict_t *pClient, const char *key, const char *value )
{
	sv_client_t	*cl;

	cl = SV_ClientFromEdict( pClient, false ); // pfnUserInfoChanged passed
	if( cl == NULL )
	{
		MsgDev( D_ERROR, "SV_SetClientPhysinfo: client is not connected!\n" );
		return;
	}

	if( Info_SetValueForKey( cl->physinfo, key, value ))
		cl->physinfo_modified = true;
}

/*
=============
pfnGetPhysicsInfoString

=============
*/
const char *pfnGetPhysicsInfoString( const edict_t *pClient )
{
	sv_client_t	*cl;

	cl = SV_ClientFromEdict( pClient, false ); // pfnUserInfoChanged passed
	if( cl == NULL )
	{
		MsgDev( D_ERROR, "SV_GetClientPhysinfo: client is not connected!\n" );
		return "";
	}
	return cl->physinfo;
}

/*
=============
pfnPrecacheEvent

register or returns already registered event id
=============
*/
word pfnPrecacheEvent( int type, const char *psz )
{
	return SV_FindIndex( psz, CS_EVENTS, MAX_EVENTS, type );
}

/*
=============
pfnPlaybackEvent

=============
*/
static void pfnPlaybackEvent( int flags, const edict_t *pInvoker, word eventindex, float delay, event_args_t *args )
{
	sv_client_t	*cl;
	event_state_t	*es;
	event_info_t	*ei = NULL;
	event_args_t	dummy; // in case send naked event without args
	int		j, leafnum, cluster, area1, area2;
	int		slot, bestslot, invokerIndex = 0;
	byte		*mask = NULL;
	vec3_t		pvspoint;

	// first check event for out of bounds
	if( eventindex < 1 || eventindex > MAX_EVENTS )
	{
		MsgDev( D_ERROR, "SV_PlaybackEvent: invalid eventindex %i\n", eventindex );
		return;
	}

	// check event for precached
	if( !SV_FindIndex( sv.configstrings[CS_EVENTS+eventindex], CS_EVENTS, MAX_EVENTS, false ))
	{
		MsgDev( D_ERROR, "SV_PlaybackEvent: event %i was not precached\n", eventindex );
		return;		
	}

	if(!( flags & FEV_GLOBAL ))
          {
		// PVS message - trying to get a pvspoint
		// args->origin always have higher priority than invoker->origin
		if( args && !VectorIsNull( args->origin ))
		{
			VectorCopy( args->origin, pvspoint );
		}
		else if( SV_IsValidEdict( pInvoker ))
		{
			VectorCopy( pInvoker->v.origin, pvspoint );
		}
		else
		{
			const char *ev_name = sv.configstrings[CS_EVENTS+eventindex];
			MsgDev( D_ERROR, "%s: not a FEV_GLOBAL event missing origin. Ignored.\n", ev_name );
			return;
		}
	}

	// check event for some user errors
	if( flags & (FEV_NOTHOST|FEV_HOSTONLY))
	{
		if( !SV_ClientFromEdict( pInvoker, true ))
		{
			const char *ev_name = sv.configstrings[CS_EVENTS+eventindex];
			if( flags & FEV_NOTHOST )
				MsgDev( D_WARN, "%s: specified FEV_NOTHOST when invoker not a client\n", ev_name );
			if( flags & FEV_HOSTONLY )
				MsgDev( D_WARN, "%s: specified FEV_HOSTONLY when invoker not a client\n", ev_name );
			// pInvoker isn't a client
			flags &= ~(FEV_NOTHOST|FEV_HOSTONLY);
		}
	}

	flags |= FEV_SERVER; // it's a server event

	if( delay < 0.0f )
		delay = 0.0f; // fixup negative delays

	if( SV_IsValidEdict( pInvoker ))
		invokerIndex = NUM_FOR_EDICT( pInvoker );

	if( args == NULL )
	{
		Mem_Set( &dummy, 0, sizeof( dummy ));
		args = &dummy;
	}

	if( flags & FEV_RELIABLE )
	{
		args->ducking = 0;
		VectorClear( args->velocity );
	}
	else if( invokerIndex )
	{
		// get up some info from invoker
		if( VectorIsNull( args->origin )) 
			VectorCopy( pInvoker->v.origin, args->origin );
		if( VectorIsNull( args->angles )) 
			VectorCopy( pInvoker->v.angles, args->angles );
		VectorCopy( pInvoker->v.velocity, args->velocity );
		args->ducking = (pInvoker->v.flags & FL_DUCKING) ? true : false;
	}

	if(!( flags & FEV_GLOBAL ))
	{
		// setup pvs cluster for invoker
		leafnum = CM_PointLeafnum( pvspoint );
		cluster = CM_LeafCluster( leafnum );
		mask = CM_ClusterPVS( cluster );
		area1 = CM_LeafArea( leafnum );
	}

	// process all the clients
	for( slot = 0, cl = svs.clients; slot < sv_maxclients->integer; slot++, cl++ )
	{
		if( cl->state != cs_spawned || !cl->edict || ( cl->edict->v.flags & FL_FAKECLIENT ))
			continue;

		if( flags & FEV_NOTHOST && cl->edict == pInvoker )
			continue;	// will be played on client side

		if( flags & FEV_HOSTONLY && cl->edict != pInvoker )
			continue;	// sending only to invoker

		if(!( flags & FEV_GLOBAL ))
		{
			area2 = CM_LeafArea( leafnum );
			cluster = CM_LeafCluster( leafnum );
			leafnum = CM_PointLeafnum( cl->edict->v.origin );
			if(!CM_AreasConnected( area1, area2 )) continue;
			if( mask && (!(mask[cluster>>3] & (1<<(cluster & 7)))))
				continue;
		}

		// all checks passed, send the event

		// reliable event
		if( flags & FEV_RELIABLE )
		{
			event_info_t	info;

			info.index = eventindex;
			info.fire_time = delay;
			info.args = *args;
			info.entity_index = invokerIndex;
			info.packet_index = -1;
			info.flags = 0; // server ignore flags

			// skipping queue, write in reliable datagram
			MSG_WriteByte( &cl->reliable, svc_event_reliable );
			SV_PlaybackEvent( &cl->reliable, &info );
			continue;
		}

		// unreliable event (stores in queue)
		es = &cl->events;
		bestslot = -1;

		for( j = 0; j < MAX_EVENT_QUEUE; j++ )
		{
			ei = &es->ei[j];
		
			if( ei->index == 0 )
			{
				// found an empty slot
				bestslot = j;
				break;
			}
		}
				
		// no slot found for this player, oh well
		if( bestslot == -1 ) continue;

		ei->index = eventindex;
		ei->fire_time = delay;
		ei->args = *args;
		ei->entity_index = invokerIndex;
		ei->packet_index = -1;
		ei->flags = 0; // server ignore flags
	}
}

/*
=============
pfnCopyEdict

returns NULL if failed to copy
=============
*/
edict_t *pfnCopyEdict( const edict_t *pEdict )
{
	return SV_CopyEdict( pEdict );
}

/*
=============
pfnCheckArea

=============
*/
int pfnCheckArea( const edict_t *entity, int clientarea )
{
	if( !SV_IsValidEdict( entity ))
	{
		MsgDev( D_WARN, "SV_AreasConnected: invalid entity %s\n", SV_ClassName( entity ));
		return 0;
	}

	// ignore if not touching a PV leaf check area
	if( !CM_AreasConnected( clientarea, entity->pvServerData->areanum ))
	{
		// doors can legally straddle two areas, so
		// we may need to check another one
		if( !CM_AreasConnected( clientarea, entity->pvServerData->areanum2 ))
			return 0;	// blocked by a door
	}
	return 1;	// visible
}

/*
=============
pfnCheckVisibility

=============
*/
int pfnCheckVisibility( const edict_t *entity, byte *pset )
{
	int	i, l;

	if( !SV_IsValidEdict( entity ))
	{
		MsgDev( D_WARN, "SV_CheckVisibility: invalid entity %s\n", SV_ClassName( entity ));
		return 0;
	}

	if( !pset ) return 1; // vis not set - fullvis enabled

	// never send entities that aren't linked in
	if( !entity->pvServerData->linked ) return 0;

	// check individual leafs
	if( !entity->pvServerData->num_clusters )
		return 0; // not a linked in
	
	for( i = l = 0; i < entity->pvServerData->num_clusters; i++ )
	{
		l = entity->pvServerData->clusternums[i];

		if( pset[l>>3] & (1<<(l & 7)))
			break;
	}

	// if we haven't found it to be visible,
	// check overflow clusters that coudln't be stored
	if( i == entity->pvServerData->num_clusters )
	{
		if( entity->pvServerData->lastcluster != -1 )
		{
			for( ; l <= entity->pvServerData->lastcluster; l++ )
			{
				if( pset[l>>3] & (1<<(l & 7)))
					break;
			}
			if( l == entity->pvServerData->lastcluster )
				return 0;	// not visible
		}
		else return 0;
	}
	return 1;
}

/*
=============
pfnCanSkipPlayer

=============
*/
int pfnCanSkipPlayer( const edict_t *player )
{
	sv_client_t	*cl;

	cl = SV_ClientFromEdict( player, true );
	if( cl == NULL )
	{
		MsgDev( D_ERROR, "SV_CanSkip: client is not connected!\n" );
		return false;
	}

	if( NET_IsLocalAddress( cl->netchan.remote_address ))
		return true;
	return false;
}

/*
=============
pfnSetGroupMask

=============
*/
void pfnSetGroupMask( int mask, int op )
{
	svs.groupmask = mask;
	svs.groupop = op;
}

/*
=============
pfnEndGame

=============
*/
void pfnEndGame( const char *engine_command )
{
	if( !com.stricmp( "credits", engine_command ))
		Host_Credits ();
	else Host_EndGame( engine_command );
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
	if( clientIndex < 0 || clientIndex >= svgame.globals->maxClients )
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

/*
=============
pfnGetPlayerPing

=============
*/
void pfnGetPlayerStats( const edict_t *pClient, int *ping, int *packet_loss )
{
	sv_client_t	*cl;

	cl = SV_ClientFromEdict( pClient, false );
	if( cl == NULL )
	{
		MsgDev( D_ERROR, "SV_GetPlayerStats: client is not connected!\n" );
		return;
	}

	if( ping ) *ping = cl->ping;
	if( packet_loss ) *packet_loss = cl->packet_loss;
}

/*
=============
pfnAddServerCommand

=============
*/
void pfnAddServerCommand( const char *cmd_name, void (*function)(void), const char *cmd_desc )
{
	Cmd_AddCommand( cmd_name, function, cmd_desc );
}
					
// engine callbacks
static enginefuncs_t gEngfuncs = 
{
	sizeof( enginefuncs_t ),
	pfnPrecacheModel,
	pfnPrecacheSound,	
	pfnSetModel,
	pfnModelIndex,
	pfnModelFrames,
	pfnSetSize,	
	pfnChangeLevel,
	pfnFindClientInPHS,
	pfnEntitiesInPHS,
	pfnVecToYaw,
	pfnVecToAngles,
	pfnMoveToOrigin,
	pfnChangeYaw,
	pfnChangePitch,
	pfnFindEntityByString,
	pfnGetEntityIllum,
	pfnFindEntityInSphere,
	pfnFindClientInPVS,
	pfnEntitiesInPVS,
	pfnMakeVectors,
	AngleVectors,
	pfnCreateEntity,
	pfnRemoveEntity,
	pfnCreateNamedEntity,
	pfnMakeStatic,
	pfnLinkEntity,
	pfnDropToFloor,
	pfnWalkMove,
	pfnSetOrigin,
	SV_StartSound,
	pfnEmitAmbientSound,
	pfnTraceLine,
	pfnTraceToss,
	pfnTraceMonsterHull,
	pfnTraceHull,
	pfnTraceModel,
	pfnTraceTexture,
	pfnTraceSphere,
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
	pfnCVarGetValue,
	pfnCVarGetString,
	pfnCVarSetValue,
	pfnCVarSetString,
	pfnAlertMessage,
	pfnEngineFprintf,
	pfnPvAllocEntPrivateData,
	pfnPvEntPrivateData,
	pfnFreeEntPrivateData,
	SV_GetString,
	SV_AllocString,
	pfnGetVarsOfEnt,
	pfnPEntityOfEntOffset,
	pfnEntOffsetOfPEntity,
	pfnIndexOfEdict,
	pfnPEntityOfEntIndex,
	pfnFindEntityByVars,
	pfnGetModelPtr,
	pfnRegUserMsg,
	pfnAreaPortal,
	pfnGetBonePosition,
	pfnFunctionFromName,
	pfnNameForFunction,	
	pfnClientPrintf,
	pfnServerPrint,	
	pfnCmd_Args,
	pfnCmd_Argv,
	pfnCmd_Argc,
	pfnGetAttachment,
	pfnCRC32_Init,
	pfnCRC32_ProcessBuffer,
	pfnCRC32_ProcessByte,
	pfnCRC32_Final,
	pfnRandomLong,
	pfnRandomFloat,
	pfnSetView,
	pfnTime,
	pfnCrosshairAngle,
	pfnLoadFile,
	pfnFreeFile,
	pfnEndGame,
	pfnCompareFileTime,
	pfnRemoveFile,
	pfnClassifyEdict,
	pfnFadeClientVolume,
	pfnSetClientMaxspeed,
	pfnCreateFakeClient,
	pfnThinkFakeClient,
	pfnFileExists,
	pfnGetInfoKeyBuffer,
	pfnInfoKeyValue,
	pfnSetKeyValue,
	pfnSetClientKeyValue,
	pfnIsMapValid,
	pfnStaticDecal,
	pfnPrecacheGeneric,
	pfnSetSkybox,
	pfnPlayMusic,
	pfnIsDedicatedServer,
	pfnMemAlloc,
	pfnMemFree,
	pfnInfo_RemoveKey,
	pfnGetPhysicsKeyValue,
	pfnSetPhysicsKeyValue,
	pfnGetPhysicsInfoString,
	pfnPrecacheEvent,
	pfnPlaybackEvent,
	pfnCopyEdict,
	pfnCheckArea,
	pfnCheckVisibility,
	pfnFOpen,
	pfnFRead,
	pfnFWrite,
	pfnFClose,
	pfnCanSkipPlayer,
	pfnFGets,
	pfnFSeek,
	pfnFTell,
	pfnSetGroupMask,	
	pfnDropClient,
	Host_Error,
	pfnParseToken,
	pfnGetPlayerStats,
	pfnAddServerCommand,
	pfnLoadLibrary,
	pfnGetProcAddress,
	pfnFreeLibrary,
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
		pkvd[numpairs].szKeyName = com.stralloc( svgame.temppool, keyname, __FILE__, __LINE__ );
		pkvd[numpairs].szValue = com.stralloc( svgame.temppool, token.string, __FILE__, __LINE__ );
		pkvd[numpairs].fHandled = false;		

		if( !com.strcmp( keyname, "classname" ) && classname == NULL )
			classname = pkvd[numpairs].szValue;
		if( ++numpairs >= 256 ) break;
	}
	
	ent = SV_AllocPrivateData( ent, MAKE_STRING( classname ));

	if( ent->v.flags & FL_KILLME )
		return false;

	for( i = 0; i < numpairs; i++ )
	{
		if( pkvd[i].fHandled ) continue;
		pkvd[i].szClassName = (char *)classname;
		svgame.dllFuncs.pfnKeyValue( ent, &pkvd[i] );
	}

	// check for rendercolor
	if( VectorIsNull( ent->v.rendercolor )) VectorSet( ent->v.rendercolor, 255, 255, 255 );

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
	int	current_skill = Cvar_VariableInteger( "skill" ); // lock skill level
	bool	deathmatch = Cvar_VariableInteger( "deathmatch" );
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
			ent = EDICT_NUM( 0 ); // already initialized
		}
		else ent = SV_AllocEdict();

		if( !SV_ParseEdict( entities, ent ))
			continue;

		// remove things from different skill levels or deathmatch
		if( deathmatch )
		{
			if( ent->v.spawnflags & SF_NOT_DEATHMATCH )
			{
				SV_FreeEdict( ent );
				inhibited++;
				continue;
			}
		}
		else if( current_skill == 0 && ent->v.spawnflags & SF_NOT_EASY )
		{
			SV_FreeEdict( ent );
			inhibited++;
			continue;
		}
		else if( current_skill == 1 && ent->v.spawnflags & SF_NOT_MEDIUM )
		{
			SV_FreeEdict( ent );
			inhibited++;
			continue;
		}
		else if( current_skill >= 2 && ent->v.spawnflags & SF_NOT_HARD )
		{
			SV_FreeEdict( ent );
			inhibited++;
			continue;
		}

		if( svgame.dllFuncs.pfnSpawn( ent ) == -1 )
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
	if( ent->free ) SV_InitEdict( ent );
	ent->v.model = MAKE_STRING( sv.configstrings[CS_MODELS+1] );
	ent->v.modelindex = 1;	// world model
	ent->v.solid = SOLID_BSP;
	ent->v.movetype = MOVETYPE_PUSH;

	svgame.globals->maxEntities = GI->max_edicts;
	svgame.globals->maxClients = sv_maxclients->integer;
	svgame.globals->mapname = MAKE_STRING( sv.name );
	svgame.globals->startspot = MAKE_STRING( sv.startspot );
	svgame.globals->time = sv.time * 0.001f;

	// spawn the rest of the entities on the map
	SV_LoadFromFile( entities );

	// set client fields on player ents
	if( !sv.loadgame && !sv.changelevel )
	{
		for( i = 0; i < svgame.globals->maxClients; i++ )
		{
			// setup all clients
			ent = EDICT_NUM( i + 1 );
			SV_InitEdict( ent );
			ent->pvServerData->client = svs.clients + i;
			ent->pvServerData->client->edict = ent;
			svgame.globals->numClients++;
		}
	}
	MsgDev( D_LOAD, "Total %i entities spawned\n", svgame.globals->numEntities );
}

void SV_UnloadProgs( void )
{
	SV_DeactivateServer ();

	svgame.dllFuncs.pfnGameShutdown ();
	StringTable_Delete( svgame.hStringTable );

	Sys_FreeNameFuncGlobals ();
	Com_FreeLibrary( svgame.hInstance );
	Mem_FreePool( &svgame.mempool );
	Mem_FreePool( &svgame.private );
	Mem_FreePool( &svgame.temppool );
	svgame.hInstance = NULL;

	Mem_Set( &svgame, 0, sizeof( svgame ));
}

void SV_LoadProgs( const char *name )
{
	static SERVERAPI		GetEntityAPI;
	static globalvars_t		gpGlobals;
	static playermove_t		gpMove;
	string			libpath;
	edict_t			*e;
	int			i;

	if( svgame.hInstance ) SV_UnloadProgs();

	// fill it in
	svgame.pmove = &gpMove;
	svgame.globals = &gpGlobals;
	Com_BuildPath( name, libpath );
	svgame.mempool = Mem_AllocPool( "Server Edicts Zone" );
	svgame.private = Mem_AllocPool( "Server Private Zone" );
	svgame.temppool = Mem_AllocPool( "Server Temp Strings" );
	svgame.hInstance = Com_LoadLibrary( libpath );

	if( !svgame.hInstance )
	{
		Host_Error( "SV_LoadProgs: can't initialize server.dll\n" );
		return;
	}

	GetEntityAPI = (SERVERAPI)Com_GetProcAddress( svgame.hInstance, "CreateAPI" );
	if( !GetEntityAPI )
	{
		Host_Error( "SV_LoadProgs: failed to get address of CreateAPI proc\n" );
		return;
	}

	if( !Sys_LoadSymbols( libpath ))
	{
		Host_Error( "SV_LoadProgs: can't loading export symbols\n" );
		return;
	}

	if( !GetEntityAPI( &svgame.dllFuncs, &gEngfuncs, svgame.globals ))
	{
		Host_Error( "SV_LoadProgs: couldn't get entity API\n" );
		return;
	}

	// 65535 unique strings should be enough ...
	if( !sv.loadgame && !sv.changelevel )
		svgame.hStringTable = StringTable_Create( "Server", 0x10000 );
	svgame.globals->maxEntities = GI->max_edicts;
	svgame.globals->maxClients = sv_maxclients->integer;
	svgame.edicts = Mem_Alloc( svgame.mempool, sizeof( edict_t ) * svgame.globals->maxEntities );
	svgame.globals->numEntities = svgame.globals->maxClients + 1; // clients + world
	svgame.globals->numClients = 0;
	for( i = 0, e = svgame.edicts; i < svgame.globals->maxEntities; i++, e++ )
		e->free = true; // mark all edicts as freed

	// all done, initialize game
	svgame.dllFuncs.pfnGameInit();

	// initialize pm_shared
	SV_InitClientMove();

	// fire once
	MsgDev( D_INFO, "Dll loaded for mod %s\n", svgame.dllFuncs.pfnGetGameDescription() );
}