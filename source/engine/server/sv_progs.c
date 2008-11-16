//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        sv_progs.c - server.dat interface
//=======================================================================

#include "common.h"
#include "server.h"
#include "byteorder.h"
#include "const.h"

/*
============
SV_CalcBBox

FIXME: get to work
============
*/
void SV_CalcBBox( edict_t *ent, vec3_t mins, vec3_t maxs )
{
	vec3_t		rmin, rmax;
	int		i, j, k, l;
	float		a, *angles;
	vec3_t		bounds[2];
	float		xvector[2], yvector[2];
	vec3_t		base, transformed;

	// find min / max for rotations
	angles = ent->progs.sv->angles;
		
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

	VectorCopy( rmin, ent->progs.sv->mins );
	VectorCopy( rmax, ent->progs.sv->maxs );
}

void SV_SetMinMaxSize( edict_t *e, float *min, float *max, bool rotate )
{
	int		i;

	for( i = 0; i < 3; i++ )
		if( min[i] > max[i] )
			PRVM_ERROR("SV_SetMinMaxSize: backwards mins/maxs");

	rotate = false; // FIXME
			
	// set derived values
	if( rotate && e->progs.sv->solid == SOLID_BBOX )
	{
		SV_CalcBBox( e, min, max );
	}
	else
	{
		VectorCopy( min, e->progs.sv->mins);
		VectorCopy( max, e->progs.sv->maxs);
	}
	VectorSubtract( max, min, e->progs.sv->size );

	// TODO: fill also mass and density
	SV_LinkEdict (e);
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

	VectorCopy( tossent->progs.sv->origin, original_origin );
	VectorCopy( tossent->progs.sv->velocity, original_velocity );
	VectorCopy( tossent->progs.sv->angles, original_angles );
	VectorCopy( tossent->progs.sv->avelocity, original_avelocity );
	gravity = tossent->progs.sv->gravity * sv_gravity->value * 0.05;

	for( i = 0; i < 200; i++ )
	{
		// LordHavoc: sanity check; never trace more than 10 seconds
		SV_CheckVelocity( tossent );
		tossent->progs.sv->velocity[2] -= gravity;
		VectorMA( tossent->progs.sv->angles, 0.05, tossent->progs.sv->avelocity, tossent->progs.sv->angles );
		VectorScale( tossent->progs.sv->velocity, 0.05, move );
		VectorAdd( tossent->progs.sv->origin, move, end );
		trace = SV_Trace( tossent->progs.sv->origin, tossent->progs.sv->mins, tossent->progs.sv->maxs, end, MOVE_NORMAL, tossent, SV_ContentsMask( tossent ));
		VectorCopy( trace.endpos, tossent->progs.sv->origin );
		if( trace.fraction < 1 ) break;
	}
	VectorCopy( original_origin, tossent->progs.sv->origin );
	VectorCopy( original_velocity, tossent->progs.sv->velocity );
	VectorCopy( original_angles, tossent->progs.sv->angles );
	VectorCopy( original_avelocity, tossent->progs.sv->avelocity );

	return trace;
}

void SV_CreatePhysBody( edict_t *ent )
{
	if( !ent || ent->progs.sv->movetype != MOVETYPE_PHYSIC ) return;
	ent->priv.sv->physbody = pe->CreateBody( ent->priv.sv, SV_GetModelPtr(ent), ent->progs.sv->m_pmatrix, ent->progs.sv->solid );

	pe->SetParameters( ent->priv.sv->physbody, SV_GetModelPtr(ent), 0, ent->progs.sv->mass ); 
}

void SV_SetPhysForce( edict_t *ent )
{
	if( !ent || ent->progs.sv->movetype != MOVETYPE_PHYSIC ) return;
	pe->SetForce( ent->priv.sv->physbody, ent->progs.sv->velocity, ent->progs.sv->avelocity, ent->progs.sv->force, ent->progs.sv->torque );
}

void SV_SetMassCentre( edict_t *ent )
{
	if( !ent || ent->progs.sv->movetype != MOVETYPE_PHYSIC ) return;
	pe->SetMassCentre( ent->priv.sv->physbody, ent->progs.sv->m_pcentre );
}

void SV_SetModel (edict_t *ent, const char *name)
{
	int		i;
	cmodel_t		*mod;
	vec3_t		angles;

	i = SV_ModelIndex( name );
	if( i == 0 ) return;
	ent->progs.sv->model = PRVM_SetEngineString( sv.configstrings[CS_MODELS+i] );
	ent->progs.sv->modelindex = i;

	mod = pe->RegisterModel( name );
	if( mod ) SV_SetMinMaxSize( ent, mod->mins, mod->maxs, false );

	// FIXME: translate angles correctly
	angles[0] = ent->progs.sv->angles[0] - 90.0f;
	angles[1] = ent->progs.sv->angles[1];
	angles[2] = ent->progs.sv->angles[2] + 90.0f;

	AngleVectors( angles, ent->progs.sv->m_pmatrix[0], ent->progs.sv->m_pmatrix[1], ent->progs.sv->m_pmatrix[2] );
	VectorCopy( ent->progs.sv->origin, ent->progs.sv->m_pmatrix[3] );
	ConvertPositionToPhysic( ent->progs.sv->m_pmatrix[3] );
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
	for(i = 0; i < MAX_CONFIGSTRINGS; i++)
		csbuffer[i] = StringTable_SetString( s_table, sv.configstrings[i] );
	SV_AddSaveLump( f, LUMP_CFGSTRING, &csbuffer, sizeof(csbuffer), true );
}

void SV_WriteGlobal( wfile_t *f )
{
	dkeyvalue_t	globals[MAX_FIELDS];
	int		numpairs = 0;

	SV_VM_Begin();
	PRVM_ED_WriteGlobals( &globals, &numpairs, SV_SetPair );
	SV_VM_End();

	SV_AddSaveLump( f, LUMP_GAMESTATE, &globals, numpairs * sizeof(dkeyvalue_t), true );
}

void SV_WriteLocals( wfile_t *f )
{
	dkeyvalue_t	fields[MAX_FIELDS];
	vfile_t		*h = VFS_Open( NULL, "wb" );
	int		i, numpairs = 0;

	SV_VM_Begin();
	for( i = 0; i < prog->num_edicts; i++ )
	{
		numpairs = 0; // reset fields info
		PRVM_ED_Write( PRVM_EDICT_NUM( i ), &fields, &numpairs, SV_SetPair );
		VFS_Write( h, &numpairs, sizeof( int ));		// numfields
		VFS_Write( h, &fields, numpairs * sizeof( dkeyvalue_t ));	// fields
	}
	SV_VM_End();

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
	if(Host_MaxClients() == 1 && svs.clients[0].edict->progs.sv->health <= 0 )
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
	prog->num_edicts = entnum;
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

void SV_BeginIncreaseEdicts( void )
{
	int		i;
	edict_t		*ent;

	// links don't survive the transition, so unlink everything
	for( i = 0, ent = prog->edicts; i < prog->max_edicts; i++, ent++ )
	{
		if( !ent->priv.sv->free ) SV_UnlinkEdict( prog->edicts + i ); // free old entity
		Mem_Set( &ent->priv.sv->clusternums, 0, sizeof( ent->priv.sv->clusternums ));
	}
	SV_ClearWorld();
}

void SV_EndIncreaseEdicts(void)
{
	int		i;
	edict_t		*ent;

	for( i = 0, ent = prog->edicts; i < prog->max_edicts; i++, ent++ )
	{
		// link every entity except world
		if( !ent->priv.sv->free ) SV_LinkEdict(ent);
	}
}

/*
=================
SV_InitEdict

Alloc new edict from list
=================
*/
void SV_InitEdict (edict_t *e)
{
	e->priv.sv->serialnumber = PRVM_NUM_FOR_EDICT(e);
}

/*
=================
SV_FreeEdict

Marks the edict as free
=================
*/
void SV_FreeEdict( edict_t *ed )
{
	// unlink from world
	SV_UnlinkEdict( ed );

	ed->priv.sv->s.ed_type = ED_SPAWNED;
	ed->priv.sv->freetime = sv.time;
	ed->priv.sv->free = true;

	ed->progs.sv->model = 0;
	ed->progs.sv->takedamage = 0;
	ed->progs.sv->modelindex = 0;
	ed->progs.sv->skin = 0;
	ed->progs.sv->frame = 0;
	ed->progs.sv->solid = 0;

	pe->RemoveBody( ed->priv.sv->physbody );
	VectorClear( ed->progs.sv->origin );
	VectorClear( ed->progs.sv->angles );
	ed->progs.sv->nextthink = -1;
	ed->priv.sv->physbody = NULL;
}

void SV_CountEdicts( void )
{
	edict_t	*ent;
	int	i, active = 0, models = 0, solid = 0, step = 0;

	for (i = 0; i < prog->num_edicts; i++)
	{
		ent = PRVM_EDICT_NUM(i);
		if (ent->priv.sv->free) continue;
		active++;
		if (ent->progs.sv->solid) solid++;
		if (ent->progs.sv->model) models++;
		if (ent->progs.sv->movetype == MOVETYPE_STEP) step++;
	}

	Msg("num_edicts:%3i\n", prog->num_edicts);
	Msg("active    :%3i\n", active);
	Msg("view      :%3i\n", models);
	Msg("touch     :%3i\n", solid);
	Msg("step      :%3i\n", step);
}

bool SV_LoadEdict( edict_t *ent )
{
	int current_skill = (int)Cvar_VariableValue("skill");

	// remove things from different skill levels or deathmatch
	if(Cvar_VariableValue ("deathmatch"))
	{
		if( (int)ent->progs.sv->spawnflags & SPAWNFLAG_NOT_DEATHMATCH )
			return false;
	}
	else if(current_skill <= 0 && (int)ent->progs.sv->spawnflags & SPAWNFLAG_NOT_EASY  )
		return false;
	else if(current_skill == 1 && (int)ent->progs.sv->spawnflags & SPAWNFLAG_NOT_MEDIUM)
		return false;
	else if(current_skill >= 2 && (int)ent->progs.sv->spawnflags & SPAWNFLAG_NOT_HARD  )
		return false;

	// apply edict classname
	ent->priv.sv->s.classname = SV_ClassIndex(PRVM_GetString( ent->progs.sv->classname ));
	return true;
}

void SV_RestoreEdict( edict_t *ent )
{
	// link it into the bsp tree
	SV_LinkEdict( ent );
	SV_CreatePhysBody( ent );
	SV_SetPhysForce( ent ); // restore forces ...
	SV_SetMassCentre( ent ); // and mass force

	if( ent->progs.sv->loopsound ) // restore loopsound
		ent->priv.sv->s.soundindex = SV_SoundIndex(PRVM_GetString(ent->progs.sv->loopsound));
}

void SV_VM_Begin( void )
{
	PRVM_Begin;
	PRVM_SetProg( PRVM_SERVERPROG );

	if( prog ) *prog->time = sv.time;
}

void SV_VM_End( void )
{
	PRVM_End;
}



/*
===============================================================================
Game Builtin Functions

mathlib, debugger, and various misc helpers
===============================================================================
*/
/*
=========
PF_precache_model

float precache_model( string s )
=========
*/
void PF_precache_model( void )
{
	if(!VM_ValidateArgs( "precache_model", 1 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	PRVM_G_FLOAT(OFS_RETURN) = SV_ModelIndex(PRVM_G_STRING(OFS_PARM0));
}

/*
=========
PF_precache_model

float precache_sound( string s )
=========
*/
void PF_precache_sound( void )
{
	if(!VM_ValidateArgs( "precache_sound", 1 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	PRVM_G_FLOAT(OFS_RETURN) = SV_SoundIndex(PRVM_G_STRING(OFS_PARM0));
}

/*
=================
PF_setmodel

float setmodel( entity ent, string m )
=================
*/
void PF_setmodel( void )
{
	edict_t	*e;

	if(!VM_ValidateArgs( "setmodel", 2 ))
		return;

	e = PRVM_G_EDICT(OFS_PARM0);
	if(e == prog->edicts)
	{
		VM_Warning("setmodel: can't modify world entity\n");
		return;
	}
	if(e->priv.sv->free)
	{
		VM_Warning("setmodel: can't modify free entity\n");
		return;
	}

	if( PRVM_G_STRING(OFS_PARM1)[0] <= ' ' )
	{
		VM_Warning("setmodel: null name\n");
		return;
	}
	SV_SetModel( e, PRVM_G_STRING(OFS_PARM1)); 
}

/*
=================
PF_model_index

float model_index( string s )
=================
*/
void PF_modelindex( void )
{
	int	index;

	if(!VM_ValidateArgs( "model_index", 1 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	index = SV_FindIndex( PRVM_G_STRING(OFS_PARM0), CS_MODELS, MAX_MODELS, false );	

	if(!index) VM_Warning("modelindex: %s not precached\n", PRVM_G_STRING(OFS_PARM0));
	PRVM_G_FLOAT(OFS_RETURN) = index; 
}

/*
=================
PF_modelframes

float model_frames( float modelindex )
=================
*/
void PF_modelframes( void )
{
	cmodel_t	*mod;
	float	framecount = 0.0f;

	if(!VM_ValidateArgs( "model_frames", 1 ))
		return;

	// can be returned pointer on a registered model 	
	mod = pe->RegisterModel( sv.configstrings[CS_MODELS + (int)PRVM_G_FLOAT(OFS_PARM0)] );

	if( mod ) framecount = ( float )mod->numframes;
	PRVM_G_FLOAT(OFS_RETURN) = framecount;
}

/*
=================
PF_setsize

void setsize( entity e, vector min, vector max )
=================
*/
void PF_setsize( void )
{
	edict_t	*e;
	float	*min, *max;

	if(!VM_ValidateArgs( "setsize", 3 ))
		return;

	e = PRVM_G_EDICT(OFS_PARM0);
	if(!e)
	{
		VM_Warning("setsize: entity not exist\n");
		return;
	}
	else if(e == prog->edicts)
	{
		VM_Warning("setsize: can't modify world entity\n");
		return;
	}
	else if(e->priv.sv->free)
	{
		VM_Warning("setsize: can't modify free entity\n");
		return;
	}

	min = PRVM_G_VECTOR(OFS_PARM1);
	max = PRVM_G_VECTOR(OFS_PARM2);
	SV_SetMinMaxSize( e, min, max, !VectorIsNull(e->progs.sv->angles));
}

/*
=================
PF_changelevel

void changelevel( string mapname, string spotname )
=================
*/
void PF_changelevel( void )
{
	if(!VM_ValidateArgs( "changlevel", 2 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	Cbuf_ExecuteText(EXEC_APPEND, va("changelevel %s %s\n", PRVM_G_STRING(OFS_PARM0), PRVM_G_STRING(OFS_PARM1)));
}

/*
=================
PF_vectoyaw

float vectoyaw( vector )
=================
*/
void PF_vectoyaw( void )
{
	float	*value1, yaw;

	if(!VM_ValidateArgs( "vecToYaw", 1 ))
		return;

	value1 = PRVM_G_VECTOR(OFS_PARM0);

	if( value1[1] == 0 && value1[0] == 0 ) yaw = 0;
	else
	{
		yaw = (int)(atan2(value1[1], value1[0]) * 180 / M_PI);
		if( yaw < 0 ) yaw += 360;
	}
	PRVM_G_FLOAT(OFS_RETURN) = yaw;
}


/*
=================
VM_vectoangles

vector vectoangles( vector v )
=================
*/
void PF_vectoangles( void )
{
	float	*value1, forward;
	float	yaw, pitch;

	if(!VM_ValidateArgs( "vecToAngles", 1 ))
		return;

	value1 = PRVM_G_VECTOR(OFS_PARM0);

	if( value1[1] == 0 && value1[0] == 0 )
	{
		yaw = 0;
		if( value1[2] > 0 ) pitch = 90;
		else pitch = 270;
	}
	else
	{
		if( value1[0] )
		{
			yaw = (atan2(value1[1], value1[0]) * 180 / M_PI);
			if( yaw < 0 ) yaw += 360;
		}
		else if( value1[1] > 0 ) yaw = 90;
		else yaw = 270;

		forward = sqrt(value1[0] * value1[0] + value1[1] * value1[1]);
		pitch = (atan2(value1[2], forward) * 180 / M_PI);
		if( pitch < 0 ) pitch += 360;
	}
	VectorSet( PRVM_G_VECTOR(OFS_RETURN), pitch, yaw, 0 ); 
}

/*
==============
PF_changeyaw

void ChangeYaw( void )
==============
*/
void PF_changeyaw( void )
{
	edict_t		*ent;
	float		current;

	if(!VM_ValidateArgs( "ChangeYaw", 0 ))
		return;

	ent = PRVM_PROG_TO_EDICT( prog->globals.sv->pev );
	if( ent == prog->edicts )
	{
		VM_Warning("changeyaw: can't modify world entity\n");
		return;
	}
	if( ent->priv.sv->free )
	{
		VM_Warning("changeyaw: can't modify free entity\n");
		return;
	}

	current = anglemod( ent->progs.sv->angles[1] );
	ent->progs.sv->angles[1] = SV_AngleMod( ent->progs.sv->ideal_yaw, current, ent->progs.sv->yaw_speed );
}

/*
==============
PF_changepitch

void ChangePitch( void )
==============
*/
void PF_changepitch( void )
{
	edict_t		*ent;
	float		ideal_pitch = 30.0f; // constant

	if(!VM_ValidateArgs( "ChangePitch", 0 ))
		return;

	ent = PRVM_PROG_TO_EDICT( prog->globals.sv->pev );
	if( ent == prog->edicts )
	{
		VM_Warning("ChangePitch: can't modify world entity\n");
		return;
	}
	if (ent->priv.sv->free)
	{
		VM_Warning("changepitch: can't modify free entity\n");
		return;
	}
	ent->progs.sv->angles[0] = SV_AngleMod( ideal_pitch, anglemod(ent->progs.sv->angles[0]), ideal_pitch );	
}

/*
==============
PF_getlightlevel

float getEntityIllum( entity e )
==============
*/
void PF_getlightlevel( void )
{
	edict_t		*ent;

	if(!VM_ValidateArgs( "getEntityIllum", 1 )) return;
	ent = PRVM_G_EDICT(OFS_PARM0);

	if(ent == prog->edicts)
	{
		VM_Warning("getlightlevel: can't get light level at world entity\n");
		return;
	}
	if(ent->priv.sv->free)
	{
		VM_Warning("getlightlevel: can't get light level at free entity\n");
		return;
	}	

	PRVM_G_FLOAT(OFS_RETURN) = 1.0; //FIXME: implement
}

/*
=================
PF_findradius

entity FindInSphere( vector org, float rad )
=================
*/
void PF_findradius( void )
{
	edict_t	*ent, *chain;
	vec_t	radius, radius2;
	vec3_t	org, eorg;
	int	i;

	if(!VM_ValidateArgs( "FindInSphere", 2 )) return;
	VectorCopy(PRVM_G_VECTOR(OFS_PARM0), org);
	radius = PRVM_G_FLOAT(OFS_PARM1);
	radius2 = radius * radius;
	chain = (edict_t *)prog->edicts;

	ent = prog->edicts;
	for( i = 1; i < prog->num_edicts ; i++, ent = PRVM_NEXT_EDICT(ent))
	{
		if(ent->priv.sv->free) continue;
		if(ent->progs.sv->solid == SOLID_NOT) continue;
		VectorSubtract(org, ent->progs.sv->origin, eorg);
		VectorMAMAM( 1, eorg, 0.5f, ent->progs.sv->mins, 0.5f, ent->progs.sv->maxs, eorg );

		if(DotProduct(eorg, eorg) < radius2)
		{
			ent->progs.sv->chain = PRVM_EDICT_TO_PROG(chain);
			chain = ent;
		}
	}
	VM_RETURN_EDICT( chain ); // fisrt chain
}

/*
=================
PF_inpvs

entity EntitiesInPVS( entity ent, float player )
=================
*/
void PF_inpvs( void )
{
	edict_t	*ent, *chain;
	edict_t	*pvsent;
	bool	playeronly;
	int	i, numents;

	if(!VM_ValidateArgs( "EntitiesInPVS", 2 )) return;
	pvsent = PRVM_G_EDICT(OFS_PARM0);
	playeronly = (bool)PRVM_G_FLOAT(OFS_PARM1);
	chain = (edict_t *)prog->edicts;

	if( playeronly ) 
	{
		numents = prog->reserved_edicts;
		ent = prog->edicts, i = 1;
	}
	else
	{
		numents = prog->num_edicts - prog->reserved_edicts;
		ent = prog->edicts + prog->reserved_edicts;
		i = prog->reserved_edicts;
	}

	if( playeronly ) Msg("FindClientInPVS: startent %d, numents %d\n", i, numents );
	else Msg("FindEntitiesInPVS: startent %d, numents %d\n", i, numents );

	for(; i < numents; i++, ent = PRVM_NEXT_EDICT(ent))
	{
		if( ent->priv.sv->free ) continue;
		if(SV_EntitiesIn( DVIS_PVS, pvsent->progs.sv->origin, ent->progs.sv->origin ))
		{
			Msg("found entity %d\n", ent->priv.sv->serialnumber );
			ent->progs.sv->chain = PRVM_EDICT_TO_PROG(chain);
			chain = ent;
		}
	}
	VM_RETURN_EDICT( chain ); // fisrt chain
}

/*
=================
PF_inphs

entity EntitiesInPHS( entity ent, float player )
=================
*/
void PF_inphs( void )
{
	edict_t	*ent, *chain;
	edict_t	*pvsent;
	bool	playeronly;
	int	i, numents;

	if(!VM_ValidateArgs( "EntitiesInPHS", 2 )) return;
	pvsent = PRVM_G_EDICT(OFS_PARM0);
	playeronly = (bool)PRVM_G_FLOAT(OFS_PARM1);
	chain = (edict_t *)prog->edicts;

	if( playeronly ) 
	{
		numents = prog->reserved_edicts;
		ent = prog->edicts, i = 1;
	}
	else
	{
		numents = prog->num_edicts - prog->reserved_edicts;
		ent = prog->edicts + prog->reserved_edicts;
		i = prog->reserved_edicts;
	}

	if( playeronly ) Msg("FindClientInPHS: startent %d, numents %d\n", i, numents );
	else Msg("FindEntitiesInPHS: startent %d, numents %d\n", i, numents );

	for(; i < numents; i++, ent = PRVM_NEXT_EDICT(ent))
	{
		if( ent->priv.sv->free ) continue;
		if(SV_EntitiesIn( DVIS_PHS, pvsent->progs.sv->origin, ent->progs.sv->origin ))
		{
			Msg("found entity %d\n", ent->priv.sv->serialnumber );
			ent->progs.sv->chain = PRVM_EDICT_TO_PROG(chain);
			chain = ent;
		}
	}
	VM_RETURN_EDICT( chain ); // fisrt chain
}

/*
==============
PF_makevectors

void makevectors( vector dir, float hand )
==============
*/
void PF_makevectors( void )
{
	float	*v1;

	if(!VM_ValidateArgs( "makevectors", 2 ))
		return;

	v1 = PRVM_G_VECTOR(OFS_PARM0);
	if(PRVM_G_FLOAT(OFS_PARM1)) // left-handed coords
		AngleVectorsFLU( v1, prog->globals.sv->v_forward, prog->globals.sv->v_right, prog->globals.sv->v_up);
	else AngleVectors( v1, prog->globals.sv->v_forward, prog->globals.sv->v_right, prog->globals.sv->v_up);
}

/*
==============
PF_create

entity CreateNamedEntity( string classname, vector org, vector ang )
==============
*/
void PF_create( void )
{
	edict_t		*ed;
	mfunction_t	*func, *oldf;

	if(!VM_ValidateArgs( "CreateNamedEntity", 3 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	prog->xfunction->builtinsprofile += 20;
	ed = PRVM_ED_Alloc();

	VectorCopy( PRVM_G_VECTOR(OFS_PARM1), ed->progs.sv->origin );
	VectorCopy( PRVM_G_VECTOR(OFS_PARM2), ed->progs.sv->angles );

	// look for the spawn function
	func = PRVM_ED_FindFunction(PRVM_G_STRING(OFS_PARM0));

	if(!func)
	{
		Host_Error("CreateNamedEntity: no spawn function for: %s\n", PRVM_G_STRING(OFS_PARM0));
		return;
	}

	PRVM_PUSH_GLOBALS;
	oldf = prog->xfunction;

	PRVM_G_INT( prog->pev->ofs) = PRVM_EDICT_TO_PROG( ed );
	PRVM_ExecuteProgram( func - prog->functions, "" );

	PRVM_POP_GLOBALS;
	prog->xfunction = oldf;

	VM_RETURN_EDICT( ed );
}

/*
=============
PF_makestatic

void makestatic( entity e )
=============
*/
void PF_makestatic( void )
{
	if(!VM_ValidateArgs( "makestatic", 1 ))
		return;

	// FIXME: implement
}

/*
===============
PF_droptofloor

void droptofloor( void )
===============
*/
void PF_droptofloor( void )
{
	edict_t		*ent;
	vec3_t		end;
	trace_t		trace;

	if(!VM_ValidateArgs( "droptofloor", 0 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = 0;	// assume failure if it returns early
	ent = PRVM_PROG_TO_EDICT(prog->globals.sv->pev);

	if (ent == prog->edicts)
	{
		VM_Warning("droptofloor: can't modify world entity\n");
		return;
	}
	if (ent->priv.sv->free)
	{
		VM_Warning("droptofloor: can't modify free entity\n");
		return;
	}

	VectorCopy( ent->progs.sv->origin, end );
	end[2] -= 256;
	SV_UnstickEntity( ent );

	trace = SV_Trace(ent->progs.sv->origin, ent->progs.sv->mins, ent->progs.sv->maxs, end, MOVE_NORMAL, ent, SV_ContentsMask( ent ));

	if( trace.startsolid )
	{
		vec3_t offset, org;
		VectorSet( offset, 0.5f * (ent->progs.sv->mins[0] + ent->progs.sv->maxs[0]), 0.5f * (ent->progs.sv->mins[1] + ent->progs.sv->maxs[1]), ent->progs.sv->mins[2]);
		VectorAdd( ent->progs.sv->origin, offset, org );
		trace = SV_Trace( org, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent, SV_ContentsMask( ent ));
		VectorSubtract( trace.endpos, offset, trace.endpos );
		if( trace.startsolid )
		{
			VM_Warning( "droptofloor: startsolid at %g %g %g\n", ent->progs.sv->origin[0], ent->progs.sv->origin[1], ent->progs.sv->origin[2]);
			SV_UnstickEntity( ent );
			SV_LinkEdict( ent );
			ent->progs.sv->aiflags = (int)ent->progs.sv->aiflags | AI_ONGROUND;
			ent->progs.sv->groundentity = 0;
			PRVM_G_FLOAT(OFS_RETURN) = 1;
		}
		else if( trace.fraction < 1 )
		{
			VM_Warning( "droptofloor moved to %g %g %g\n", ent->progs.sv->origin[0], ent->progs.sv->origin[1], ent->progs.sv->origin[2]);
			VectorCopy( trace.endpos, ent->progs.sv->origin );
			SV_UnstickEntity( ent );
			SV_LinkEdict( ent );
			ent->progs.sv->aiflags = (int)ent->progs.sv->aiflags | AI_ONGROUND;
			ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG( trace.ent );
			PRVM_G_FLOAT(OFS_RETURN) = 1;
			// if support is destroyed, keep suspended (gross hack for floating items in various maps)
			ent->priv.sv->suspended = true;
		}

		VM_Warning( "droptofloor: startsolid at %g %g %g\n", ent->progs.sv->origin[0], ent->progs.sv->origin[1], ent->progs.sv->origin[2]);
		SV_FreeEdict( ent );
		return;
	}
	else
	{
		if( trace.fraction != 1 )
		{
			if( trace.fraction < 1 ) VectorCopy( trace.endpos, ent->progs.sv->origin );
			SV_LinkEdict( ent );
			ent->progs.sv->aiflags = (int)ent->progs.sv->aiflags | AI_ONGROUND;
			ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG( trace.ent );
			PRVM_G_FLOAT(OFS_RETURN) = 1;
			// if support is destroyed, keep suspended (gross hack for floating items in various maps)
			ent->priv.sv->suspended = true;
		}
	}
}

/*
===============
PF_walkmove

float walkmove( float yaw, float dist, float settrace )
===============
*/
void PF_walkmove( void )
{
	edict_t		*ent;
	float		yaw, dist;
	vec3_t		move;
	mfunction_t	*oldf;
	int 		oldpev;
	bool		settrace;

	if(!VM_ValidateArgs( "walkmove", 3 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = 0;	// assume failure if it returns early
	ent = PRVM_PROG_TO_EDICT( prog->globals.sv->pev );

	if (ent == prog->edicts)
	{
		VM_Warning("walkmove: can't modify world entity\n");
		return;
	}
	if (ent->priv.sv->free)
	{
		VM_Warning("walkmove: can't modify free entity\n");
		return;
	}

	yaw = PRVM_G_FLOAT(OFS_PARM0);
	dist = PRVM_G_FLOAT(OFS_PARM1);
	settrace = (int)PRVM_G_FLOAT(OFS_PARM2);

	if(!((int)ent->progs.sv->aiflags & (AI_ONGROUND|AI_FLY|AI_SWIM)))
		return;
	yaw = yaw * M_PI*2 / 360;

	VectorSet( move, (cos(yaw) * dist), (sin(yaw) * dist), 0.0f );

	// save program state, because SV_MoveStep may call other progs
	oldf = prog->xfunction;
	oldpev = prog->globals.sv->pev;

	PRVM_G_FLOAT(OFS_RETURN) = SV_movestep( ent, move, true, false, settrace );

	// restore program state
	prog->xfunction = oldf;
	prog->globals.sv->pev = oldpev;
}

/*
=================
PF_setorigin

void setorigin( entity e, vector org )
=================
*/
void PF_setorigin( void )
{
	edict_t	*e;

	if(!VM_ValidateArgs( "setorigin", 2 )) return;
	e = PRVM_G_EDICT(OFS_PARM0);
	if (e == prog->edicts)
	{
		VM_Warning("setorigin: can't modify world entity\n");
		return;
	}
	if (e->priv.sv->free)
	{
		VM_Warning("setorigin: can't modify free entity\n");
		return;
	}

	VectorCopy( PRVM_G_VECTOR(OFS_PARM1), e->progs.sv->origin );
	pe->SetOrigin( e->priv.sv->physbody, e->progs.sv->origin );
	SV_LinkEdict( e );
}

/*
=================
PF_sound

void EmitSound(entity e, float chan, string samp, float vol, float attn)
=================
*/
void PF_sound( void )
{
	const char	*sample;
	int		channel, sound_idx;
	edict_t		*entity;
	float		attenuation;
	int 		flags = 0, ent, volume;
	vec3_t		snd_origin;
	int		sendchan;
	bool		use_phs;

	if(!VM_ValidateArgs( "EmitSound", 5 )) return;
	entity = PRVM_G_EDICT(OFS_PARM0);
	channel = (int)PRVM_G_FLOAT(OFS_PARM1);
	sample = PRVM_G_STRING(OFS_PARM2);
	volume = PRVM_G_FLOAT(OFS_PARM3);
	attenuation = PRVM_G_FLOAT(OFS_PARM4);
	ent = PRVM_NUM_FOR_EDICT( entity );

	if( attenuation < 0 || attenuation > 4 )
	{
		VM_Warning("SV_StartSound: attenuation must be in range 0-4\n");
		return;
	}
	if( channel < 0 || channel > 7 )
	{
		VM_Warning("SV_StartSound: channel must be in range 0-7\n");
		return;
	}

	if( volume != 1.0f ) flags |= SND_VOL;
	if( attenuation != 1.0f ) flags |= SND_ATTN;
	flags |= SND_POS|SND_ENT;

	// use the entity origin unless it is a bmodel or explicitly specified
	if( entity->progs.sv->solid == SOLID_BSP || VectorIsNull( entity->progs.sv->origin ))
	{
		VectorAverage( entity->progs.sv->mins, entity->progs.sv->maxs, snd_origin );
		VectorAdd( snd_origin, entity->progs.sv->origin, snd_origin );
		channel |= CHAN_RELIABLE;
		use_phs = false;
		channel &= 7;
	}
	else
	{
		VectorCopy( entity->progs.sv->origin, snd_origin );
		use_phs = true;
	}
	sound_idx = SV_SoundIndex( sample );
	sendchan = (ent<<3)|(channel & 7);

	MSG_Begin( svc_sound );
	MSG_WriteByte( &sv.multicast, flags );
	MSG_WriteByte( &sv.multicast, sound_idx );

	if( flags & SND_VOL ) MSG_WriteBits( &sv.multicast, volume, "voulme", NET_COLOR );
	if( flags & SND_ATTN) MSG_WriteByte( &sv.multicast, attenuation );
	if( flags & SND_ENT ) MSG_WriteWord( &sv.multicast, sendchan );
	if( flags & SND_POS ) MSG_WritePos( &sv.multicast, snd_origin );

	if( channel & CHAN_RELIABLE )
	{
		if( use_phs ) MSG_Send( MSG_PHS_R, snd_origin, NULL );
		else MSG_Send( MSG_ALL_R, snd_origin, NULL );
	}
	else
	{
		if( use_phs ) MSG_Send( MSG_PHS, snd_origin, NULL );
		else MSG_Send( MSG_ALL, snd_origin, NULL );
	}
}

/*
=================
PF_ambientsound

void EmitAmbientSound(entity e, string samp)
=================
*/
void PF_ambientsound( void )
{
	const char	*samp;
	edict_t		*ent;

	if(!VM_ValidateArgs( "EmitAmbientSound", 2 )) return;
	ent = PRVM_G_EDICT(OFS_PARM0);
	samp = PRVM_G_STRING(OFS_PARM1);

	if( !ent ) return;
	// check to see if samp was properly precached
	ent->progs.sv->loopsound = PRVM_G_INT(OFS_PARM1);
	ent->priv.sv->s.soundindex = SV_SoundIndex( samp );
	if( !ent->progs.sv->modelindex ) SV_LinkEdict( ent );
}

/*
=================
PF_traceline

void traceline( vector v1, vector v2, float move, entity ignore )
=================
*/
void PF_traceline( void )
{
	float		*v1, *v2;
	trace_t		trace;
	int		move;
	edict_t		*ent;

	if(!VM_ValidateArgs( "traceline", 4 )) return;
	prog->xfunction->builtinsprofile += 30;

	v1 = PRVM_G_VECTOR(OFS_PARM0);
	v2 = PRVM_G_VECTOR(OFS_PARM1);
	move = (int)PRVM_G_FLOAT(OFS_PARM2);
	ent = PRVM_G_EDICT(OFS_PARM3);

	if (IS_NAN(v1[0]) || IS_NAN(v1[1]) || IS_NAN(v1[2]) || IS_NAN(v2[0]) || IS_NAN(v1[2]) || IS_NAN(v2[2]))
		PRVM_ERROR("%s: NAN errors detected in traceline('%f %f %f', '%f %f %f', %i, entity %i)\n",
		PRVM_NAME, v1[0], v1[1], v1[2], v2[0], v2[1], v2[2], move, PRVM_EDICT_TO_PROG(ent));

	trace = SV_Trace( v1, vec3_origin, vec3_origin, v2, move, ent, SV_ContentsMask(ent));

	VM_SetTraceGlobals( &trace );
}

/*
=================
PF_tracetoss

void tracetoss( entity e, entity ignore )
=================
*/
void PF_tracetoss( void )
{
	trace_t		trace;
	edict_t		*ent;
	edict_t		*ignore;

	if(!VM_ValidateArgs( "tracetoss", 2 )) return;
	prog->xfunction->builtinsprofile += 600;

	ent = PRVM_G_EDICT(OFS_PARM0);
	if (ent == prog->edicts)
	{
		VM_Warning("tracetoss: can not use world entity\n");
		return;
	}
	ignore = PRVM_G_EDICT(OFS_PARM1);

	trace = SV_TraceToss( ent, ignore );

	prog->globals.sv->trace_allsolid = trace.allsolid;
	prog->globals.sv->trace_startsolid = trace.startsolid;
	prog->globals.sv->trace_fraction = trace.fraction;
	prog->globals.sv->trace_contents = trace.contents;

	VectorCopy (trace.endpos, prog->globals.sv->trace_endpos);
	VectorCopy (trace.plane.normal, prog->globals.sv->trace_plane_normal);
	prog->globals.sv->trace_plane_dist =  trace.plane.dist;
	if (trace.ent) prog->globals.sv->trace_ent = PRVM_EDICT_TO_PROG(trace.ent);
	else prog->globals.sv->trace_ent = PRVM_EDICT_TO_PROG(prog->edicts);
}

/*
=================
PF_tracebox

void tracebox( vector v1, vector mins, vector maxs, vector v2, float move, entity ignore )
=================
*/
void PF_tracebox( void )
{
	float		*v1, *v2, *m1, *m2;
	trace_t		trace;
	int		move;
	edict_t		*ent;

	if(!VM_ValidateArgs( "tracebox", 6 )) return;
	prog->xfunction->builtinsprofile += 30;

	v1 = PRVM_G_VECTOR(OFS_PARM0);
	m1 = PRVM_G_VECTOR(OFS_PARM1);
	m2 = PRVM_G_VECTOR(OFS_PARM2);
	v2 = PRVM_G_VECTOR(OFS_PARM3);
	move = (int)PRVM_G_FLOAT(OFS_PARM4);
	ent = PRVM_G_EDICT(OFS_PARM5);

	if (IS_NAN(v1[0]) || IS_NAN(v1[1]) || IS_NAN(v1[2]) || IS_NAN(v2[0]) || IS_NAN(v1[2]) || IS_NAN(v2[2]))
		PRVM_ERROR("%s: NAN errors detected in tracebox('%f %f %f', '%f %f %f', '%f %f %f', '%f %f %f', %i, entity %i)\n", 
		PRVM_NAME, v1[0], v1[1], v1[2], m1[0], m1[1], m1[2], m2[0], m2[1], m2[2], v2[0], v2[1], v2[2], move, PRVM_EDICT_TO_PROG(ent));

	trace = SV_Trace( v1, m1, m2, v2, move, ent, SV_ContentsMask( ent ));

	VM_SetTraceGlobals( &trace );
}

/*
=============
PF_lookupactivity

float LookupActivity( string model, float activity )
=============
*/
void PF_lookupactivity( void )
{
	cmodel_t		*mod;
	dstudioseqdesc_t	*pseqdesc;
	dstudiohdr_t	*pstudiohdr;
	int		i, seq = -1;
	int		activity, weighttotal = 0;

	if(!VM_ValidateArgs( "LookupActivity", 2 )) return;	
	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	PRVM_G_FLOAT(OFS_RETURN) = 0;

	mod = pe->RegisterModel(PRVM_G_STRING(OFS_PARM0));
	if( !mod ) return;
	pstudiohdr = (dstudiohdr_t *)mod->extradata;
	if( !pstudiohdr ) return;

	pseqdesc = (dstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);
	activity = (int)PRVM_G_FLOAT(OFS_PARM1);

	for( i = 0; i < pstudiohdr->numseq; i++)
	{
		if( pseqdesc[i].activity == activity )
		{
			weighttotal += pseqdesc[i].actweight;
			if( !weighttotal || RANDOM_LONG( 0, weighttotal - 1 ) < pseqdesc[i].actweight )
				seq = i;
		}
	}
	PRVM_G_FLOAT(OFS_RETURN) = seq;
}

/*
=============
PF_geteyepos

vector GetEyePosition( string model )
=============
*/
void PF_geteyepos( void )
{
	cmodel_t	  *mod;
	dstudiohdr_t *pstudiohdr;

	if(!VM_ValidateArgs( "GetEyePosition", 1 )) return;	
	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	VectorCopy( vec3_origin, PRVM_G_VECTOR(OFS_RETURN));
		
	mod = pe->RegisterModel(PRVM_G_STRING(OFS_PARM0));
	if( !mod ) return;
	pstudiohdr = (dstudiohdr_t *)mod->extradata;
	if( !pstudiohdr ) return;

	VectorCopy( pstudiohdr->eyeposition, PRVM_G_VECTOR(OFS_RETURN));
}

/*
=============
PF_lookupsequence

float LookupSequence( string model, string label )
=============
*/
void PF_lookupsequence( void )
{
	cmodel_t		*mod;
	dstudiohdr_t	*pstudiohdr;
	dstudioseqdesc_t	*pseqdesc;
	int		i;

	if(!VM_ValidateArgs( "LookupSequence", 2 )) return;	
	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	VM_ValidateString(PRVM_G_STRING(OFS_PARM1));
	PRVM_G_FLOAT(OFS_RETURN) = -1;
	
	mod = pe->RegisterModel(PRVM_G_STRING(OFS_PARM0));
	if( !mod ) return;
	pstudiohdr = (dstudiohdr_t *)mod->extradata;
	if( !pstudiohdr ) return;

	pseqdesc = (dstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);
	for( i = 0; i < pstudiohdr->numseq; i++ )
	{
		if(!com.stricmp( pseqdesc[i].label, PRVM_G_STRING(OFS_PARM1)))
		{
			PRVM_G_FLOAT(OFS_RETURN) = i;
			break;
		}
	}
}

/*
=============
PF_getsequenceinfo

float GetSequenceInfo( string model, float sequence )
=============
*/
void PF_getsequenceinfo( void )
{
	cmodel_t		*mod;
	edict_t		*ent;
	dstudiohdr_t	*pstudiohdr;
	dstudioseqdesc_t	*pseqdesc;
	int		sequence;

	if(!VM_ValidateArgs( "GetSequenceInfo", 2 )) return;	
	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	PRVM_G_FLOAT(OFS_RETURN) = 0;

	sequence = (int)PRVM_G_FLOAT(OFS_PARM1);
	ent = PRVM_PROG_TO_EDICT( prog->globals.sv->pev );
	mod = pe->RegisterModel(PRVM_G_STRING(OFS_PARM0));
	if( !mod ) return;
	pstudiohdr = (dstudiohdr_t *)mod->extradata;
	if( !pstudiohdr ) return;

	if( sequence >= pstudiohdr->numseq )
	{
		ent->progs.sv->m_flFrameRate = 0.0;
		ent->progs.sv->m_flGroundSpeed = 0.0;
		return;
	}

	pseqdesc = (dstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + sequence;
	if( pseqdesc->numframes > 1 )
	{
		ent->progs.sv->m_flFrameRate = 256.0 * (pseqdesc->fps / (pseqdesc->numframes - 1));
		ent->progs.sv->m_flGroundSpeed = VectorLength( pseqdesc->linearmovement ); 
		ent->progs.sv->m_flGroundSpeed = ent->progs.sv->m_flGroundSpeed * pseqdesc->fps / (pseqdesc->numframes - 1);
	}
	else
	{
		ent->progs.sv->m_flFrameRate = 256.0;
		ent->progs.sv->m_flGroundSpeed = 0.0;
	}
	PRVM_G_FLOAT(OFS_RETURN) = 1;	// all done
}

/*
=============
PF_getsequenceflags

float GetSequenceFlags( string model, float sequence )
=============
*/
void PF_getsequenceflags( void )
{
	cmodel_t		*mod;
	edict_t		*ent;
	dstudiohdr_t	*pstudiohdr;
	dstudioseqdesc_t	*pseqdesc;
	int		sequence;

	if(!VM_ValidateArgs( "GetSequenceFlags", 2 )) return;	
	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	PRVM_G_FLOAT(OFS_RETURN) = -1;

	sequence = (int)PRVM_G_FLOAT(OFS_PARM1);
	ent = PRVM_PROG_TO_EDICT( prog->globals.sv->pev );
	mod = pe->RegisterModel(PRVM_G_STRING(OFS_PARM0));
	if( !mod ) return;
	pstudiohdr = (dstudiohdr_t *)mod->extradata;
	if( !pstudiohdr ) return;
	if( sequence >= pstudiohdr->numseq ) return;

	pseqdesc = (dstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + sequence;
	PRVM_G_FLOAT(OFS_RETURN) = (float )pseqdesc->flags;
}

/*
=============
PF_aim

vector aim( entity e, float speed )
=============
*/
void PF_aim( void )
{
	edict_t	*ent, *check, *bestent;
	vec3_t	start, dir, end, bestdir;
	int	i, j;
	trace_t	tr;
	float	dist, bestdist;
	float	speed;
          int	flags = Cvar_VariableValue( "dmflags" );

	if(!VM_ValidateArgs( "tracebox", 6 )) return;	
	VectorCopy(prog->globals.sv->v_forward, PRVM_G_VECTOR(OFS_RETURN)); // assume failure if it returns early

	ent = PRVM_G_EDICT(OFS_PARM0);
	if( ent == prog->edicts )
	{
		VM_Warning("aim: can't use world entity\n");
		return;
	}
	if( ent->priv.sv->free )
	{
		VM_Warning("aim: can't use free entity\n");
		return;
	}
	speed = PRVM_G_FLOAT(OFS_PARM1);

	VectorCopy (ent->progs.sv->origin, start);
	start[2] += 20;

	// try sending a trace straight
	VectorCopy(prog->globals.sv->v_forward, dir);
	VectorMA(start, 2048, dir, end);
	tr = SV_Trace( start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent, -1 );

	if( tr.ent && ((edict_t *)tr.ent)->progs.sv->takedamage == 2 && (flags & DF_NO_FRIENDLY_FIRE
	|| ent->progs.sv->team <=0 || ent->progs.sv->team != ((edict_t *)tr.ent)->progs.sv->team))
	{
		VectorCopy (prog->globals.sv->v_forward, PRVM_G_VECTOR(OFS_RETURN));
		return;
	}

	// try all possible entities
	VectorCopy( dir, bestdir );
	bestdist = 0.5f;
	bestent = NULL;

	check = PRVM_NEXT_EDICT(prog->edicts);
	for( i = 1; i < prog->num_edicts; i++, check = PRVM_NEXT_EDICT(check))
	{
		prog->xfunction->builtinsprofile++;
		if (check->progs.sv->takedamage != 2) // DAMAGE_AIM
			continue;
		if (check == ent) continue;
		if (flags & DF_NO_FRIENDLY_FIRE && ent->progs.sv->team > 0 && ent->progs.sv->team == check->progs.sv->team)
			continue;	// don't aim at teammate
		for (j = 0; j < 3; j++)
			end[j] = check->progs.sv->origin[j] + 0.5 * (check->progs.sv->mins[j] + check->progs.sv->maxs[j]);
		VectorSubtract (end, start, dir);
		VectorNormalize (dir);
		dist = DotProduct (dir, prog->globals.sv->v_forward);
		if (dist < bestdist) continue; // to far to turn
		tr = SV_Trace (start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent, -1 );
		if (tr.ent == check)
		{	
			// can shoot at this one
			bestdist = dist;
			bestent = check;
		}
	}

	if( bestent )
	{
		VectorSubtract (bestent->progs.sv->origin, ent->progs.sv->origin, dir);
		dist = DotProduct (dir, prog->globals.sv->v_forward);
		VectorScale (prog->globals.sv->v_forward, dist, end);
		end[2] = dir[2];
		VectorNormalize(end);
		VectorCopy(end, PRVM_G_VECTOR(OFS_RETURN));
	}
	else
	{
		VectorCopy(bestdir, PRVM_G_VECTOR(OFS_RETURN));
	}
}

/*
=========
PF_servercmd

void servercommand( string s, ... )
=========
*/
void PF_servercmd( void )
{
	Cbuf_AddText(VM_VarArgs( 0 ));
}

/*
=========
PF_serverexec

void server_execute( void )
=========
*/
void PF_serverexec( void )
{
	if(VM_ValidateArgs( "ServerExecute", 0 ))
		Cbuf_Execute();
}

/*
=========
PF_clientcmd

void clientcommand( float client, string s )
=========
*/
void PF_clientcmd( void )
{
	sv_client_t	*client;
	int		i;

	if(!VM_ValidateArgs( "ClientCommand", 2 )) return;
	VM_ValidateString(PRVM_G_STRING(OFS_PARM1));

	i = (int)PRVM_G_FLOAT(OFS_PARM0);
	if( sv.state != ss_active  || i < 0 || i >= Host_MaxClients() || svs.clients[i].state != cs_spawned)
	{
		VM_Warning( "ClientCommand: client/server is not active!\n" );
		return;
	}

	client = svs.clients + i;
	MSG_WriteByte(&client->netchan.message, svc_stufftext );
	MSG_WriteString( &client->netchan.message, PRVM_G_STRING(OFS_PARM1));
	MSG_Send(MSG_ONE_R, NULL, client->edict );
}

/*
=================
PF_particle

void particle( vector origin, vector dir, float color, float count )
=================
*/
void PF_particle( void )
{
	float		*org, *dir;
	float		color, count;

	if(!VM_ValidateArgs( "particle", 4 ))
		return;

	org = PRVM_G_VECTOR(OFS_PARM0);
	dir = PRVM_G_VECTOR(OFS_PARM1);
	color = PRVM_G_FLOAT(OFS_PARM2);
	count = PRVM_G_FLOAT(OFS_PARM3);
	
	SV_StartParticle( org, dir, (int)color, (int)count);
}

/*
===============
PF_lightstyle

void lightstyle( float style, string value ) 
===============
*/
void PF_lightstyle( void )
{
	int		style;
	const char	*val;

	if(!VM_ValidateArgs( "lightstyle", 2 )) return;
	VM_ValidateString(PRVM_G_STRING(OFS_PARM1));

	style = (int)PRVM_G_FLOAT(OFS_PARM0);
	val = PRVM_G_STRING(OFS_PARM1);

	if((uint)style >= MAX_LIGHTSTYLES )
		PRVM_ERROR( "PF_lightstyle: style: %i >= %d", style, MAX_LIGHTSTYLES );
	SV_ConfigString( CS_LIGHTSTYLES + style, val );
}

/*
=============
PF_pointcontents

float pointcontents( vector v ) 
=============
*/
void PF_pointcontents( void )
{
	if(!VM_ValidateArgs( "pointcontents", 1 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = SV_PointContents(PRVM_G_VECTOR(OFS_PARM0));
}

/*
=============
PF_BeginMessage

void MsgBegin( float dest )
=============
*/
void PF_BeginMessage( void )
{
	int	svc_dest = (int)PRVM_G_FLOAT(OFS_PARM0);

	if(!VM_ValidateArgs( "MsgBegin", 1 )) return;

	// some users can send message with engine index
	// reduce number to avoid overflow problems or cheating
	svc_dest = bound( svc_bad, svc_dest, svc_nop );

	MSG_Begin( svc_dest );
}

/*
=============
PF_EndMessage

void MsgEnd(float to, vector pos, entity e)
=============
*/
void PF_EndMessage( void )
{
	int	send_to = (int)PRVM_G_FLOAT(OFS_PARM0);
	edict_t	*ed = PRVM_G_EDICT(OFS_PARM2);

	if(!VM_ValidateArgs( "MsgEnd", 3 )) return;
	if(PRVM_NUM_FOR_EDICT(ed) > prog->num_edicts)
	{
		VM_Warning("MsgEnd: sending message from killed entity\n");
		return;
	}
	// align range
	send_to = bound( MSG_ONE, send_to, MSG_PVS_R );
	MSG_Send( send_to, PRVM_G_VECTOR(OFS_PARM1), ed );
}

// various network messages
void PF_WriteByte (void){ MSG_WriteByte(&sv.multicast, (int)PRVM_G_FLOAT(OFS_PARM0)); }
void PF_WriteChar (void){ MSG_WriteChar(&sv.multicast, (int)PRVM_G_FLOAT(OFS_PARM0)); }
void PF_WriteShort (void){ MSG_WriteShort(&sv.multicast, (int)PRVM_G_FLOAT(OFS_PARM0)); }
void PF_WriteLong (void){ MSG_WriteLong(&sv.multicast, (int)PRVM_G_FLOAT(OFS_PARM0)); }
void PF_WriteAngle (void){ MSG_WriteAngle32(&sv.multicast, PRVM_G_FLOAT(OFS_PARM0)); }
void PF_WriteCoord (void){ MSG_WriteCoord32(&sv.multicast, PRVM_G_FLOAT(OFS_PARM0)); }
void PF_WriteString (void){ MSG_WriteString(&sv.multicast, PRVM_G_STRING(OFS_PARM0)); }
void PF_WriteEntity (void){ MSG_WriteShort(&sv.multicast, PRVM_G_EDICTNUM(OFS_PARM1)); }

/*
=============
PF_checkbottom

float checkbottom( entity e )
=============
*/
void PF_checkbottom( void )
{
	if(!VM_ValidateArgs( "checkbottom", 1 )) return;
	PRVM_G_FLOAT(OFS_RETURN) = SV_CheckBottom(PRVM_G_EDICT(OFS_PARM0));
}

/*
=================
PF_ClientPrint

void ClientPrintf( float type, entity client, string s )
=================
*/
void PF_ClientPrint( void )
{
	sv_client_t	*client;
	int		num, type;
	const char	*s;

	num = PRVM_G_EDICTNUM( OFS_PARM1 );
	type = (int)PRVM_G_FLOAT( OFS_PARM0 );
	if( num < 1 || num > Host_MaxClients() || svs.clients[num - 1].state != cs_spawned )
	{
		VM_Warning("ClientPrint: tired print to a non-client!\n");
		return;
	}
	client = svs.clients + num - 1;
	s = VM_VarArgs( 2 );

	switch( type )
	{
	case PRINT_CONSOLE:
		SV_ClientPrintf (client, PRINT_CONSOLE, (char *)s );
		break;
	case PRINT_CENTER:
		MSG_Begin( svc_centerprint );
		MSG_WriteString( &sv.multicast, s );
		MSG_Send( MSG_ONE_R, NULL, client->edict );
		break;
	case PRINT_CHAT:
		SV_ClientPrintf (client, PRINT_CHAT, (char *)s );
		break;
	default:
		Msg("ClientPrintf: invalid destination\n" );
		break;
	}		
}

/*
=================
PF_ServerPrint

void ServerPrint( string s )
=================
*/
void PF_ServerPrint( void )
{
	const char *s = VM_VarArgs( 0 );

	if( sv.state == ss_loading )
	{
		// while loading in-progress we can sending message
		Msg( s ); // only for local client
	}
	else SV_BroadcastPrintf( PRINT_CONSOLE, (char *)s );

}

/*
=================
PF_AreaPortalState

void areaportal( entity ent, float state )
=================
*/
void PF_AreaPortalState( void )
{
	edict_t	*ent;
	bool	open;

	if( !VM_ValidateArgs( "areaportal", 2 ))
		return;

	ent = PRVM_G_EDICT( OFS_PARM0 );
	open = (bool)PRVM_G_FLOAT( OFS_PARM1 );
	pe->SetAreaPortalState( ent->progs.sv->style, open );
}

/*
=================
PF_InfoPrint

void Info_Print( entity client )
=================
*/
void PF_InfoPrint( void )
{
	sv_client_t	*client;
	int		num;

	if(!VM_ValidateArgs( "Info_Print", 1 )) return;
	num = PRVM_G_EDICTNUM(OFS_PARM0);
	if( num < 1 || num > Host_MaxClients())
	{
		VM_Warning( "Info_Print: not a client\n" );
		return;
	}
	client = svs.clients + num - 1;
	Info_Print( client->userinfo );
}

/*
=================
PF_InfoValueForKey

string Info_ValueForKey( entity client, string key )
=================
*/
void PF_InfoValueForKey( void )
{
	sv_client_t	*client;
	int		num;
	const char	*key;

	if(!VM_ValidateArgs( "Info_ValueForKey", 2 )) return;
	VM_ValidateString(PRVM_G_STRING(OFS_PARM1));

	num = PRVM_G_EDICTNUM(OFS_PARM0);
	if( num < 1 || num > Host_MaxClients())
	{
		VM_Warning("Info_ValueForKey: not a client\n" );
		return;
	}

	client = svs.clients + num - 1;
	key = PRVM_G_STRING(OFS_PARM1);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(Info_ValueForKey( client->userinfo, (char *)key)); 
}

/*
=================
PF_InfoRemoveKey

void Info_RemoveKey( entity client, string key )
=================
*/
void PF_InfoRemoveKey( void )
{
	sv_client_t	*client;
	int		num;
	const char	*key;

	if(!VM_ValidateArgs( "Info_RemoveKey", 2 )) return;
	VM_ValidateString(PRVM_G_STRING(OFS_PARM1));

	num = PRVM_G_EDICTNUM(OFS_PARM0);
	if( num < 1 || num > Host_MaxClients())
	{
		VM_Warning("Info_RemoveKey: not a client\n" );
		return;
	}

	client = svs.clients + num - 1;
	key = PRVM_G_STRING(OFS_PARM1);
	Info_RemoveKey( client->userinfo, (char *)key);
}

/*
=================
PF_InfoSetValueForKey

void Info_SetValueForKey( entity client, string key, string value )
=================
*/
void PF_InfoSetValueForKey( void )
{
	sv_client_t	*client;
	int		num;
	const char	*key, *value;

	if(!VM_ValidateArgs( "Info_SetValueForKey", 3 )) return;
	VM_ValidateString(PRVM_G_STRING(OFS_PARM1));
	VM_ValidateString(PRVM_G_STRING(OFS_PARM2));

	num = PRVM_G_EDICTNUM(OFS_PARM0);
	if( num < 1 || num > Host_MaxClients())
	{
		VM_Warning("InfoSetValueForKey: not a client\n" );
		return;
	}

	client = svs.clients + num - 1;
	key = PRVM_G_STRING(OFS_PARM1);
	value = PRVM_G_STRING(OFS_PARM2);
	Info_SetValueForKey( client->userinfo, (char *)key, (char *)value );
}

/*
===============
PF_setsky

void setsky( string name, vector angles, float speed )
===============
*/
void PF_setsky( void )
{
	if(!VM_ValidateArgs( "setsky", 1 ))
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM0));
	SV_ConfigString( CS_SKYNAME, PRVM_G_STRING(OFS_PARM0));
}

/*
==============
PF_dropclient

void dropclient( entity client )
==============
*/
void PF_dropclient( void )
{
	int clientnum = PRVM_G_EDICTNUM(OFS_PARM0) - 1;

	if( clientnum < 0 || clientnum >= Host_MaxClients())
	{
		VM_Warning("dropclient: not a client\n");
		return;
	}
	if( svs.clients[clientnum].state != cs_spawned )
	{
		VM_Warning("dropclient: that client slot is not connected\n");
		return;
	}
	SV_DropClient( svs.clients + clientnum );
}



//NOTE: intervals between various "interfaces" was leave for future expansions
prvm_builtin_t vm_sv_builtins[] = 
{
NULL,				// #0  (leave blank as default, but can include easter egg ) 

// system events
VM_ConPrintf,			// #1  void Con_Printf( ... )
VM_ConDPrintf,			// #2  void Con_DPrintf( float level, ... )
VM_HostError,			// #3  void Com_Error( ... )
VM_SysExit,			// #4  void Sys_Exit( void )
VM_CmdArgv,			// #5  string Cmd_Argv( float arg )
VM_CmdArgc,			// #6  float Cmd_Argc( void )
NULL,				// #7  -- reserved --
NULL,				// #8  -- reserved --
NULL,				// #9  -- reserved --
NULL,				// #10 -- reserved --

// common tools
VM_ComTrace,			// #11 void Com_Trace( float enable )
VM_ComFileExists,			// #12 float Com_FileExists( string filename )
VM_ComFileSize,			// #13 float Com_FileSize( string filename )
VM_ComFileTime,			// #14 float Com_FileTime( string filename )
VM_ComLoadScript,			// #15 float Com_LoadScript( string filename )
VM_ComResetScript,			// #16 void Com_ResetScript( void )
VM_ComReadToken,			// #17 string Com_ReadToken( float newline )
VM_ComFilterToken,			// #18 float Com_Filter( string mask, string s, float casecmp )
VM_ComSearchFiles,			// #19 float Com_Search( string mask, float casecmp )
VM_ComSearchNames,			// #20 string Com_SearchFilename( float num )
VM_RandomLong,			// #21 float RandomLong( float min, float max )
VM_RandomFloat,			// #22 float RandomFloat( float min, float max )
VM_RandomVector,			// #23 vector RandomVector( vector min, vector max )
VM_CvarRegister,			// #24 void Cvar_Register( string name, string value, float flags )
VM_CvarSetValue,			// #25 void Cvar_SetValue( string name, float value )
VM_CvarGetValue,			// #26 float Cvar_GetValue( string name )
VM_CvarSetString,			// #27 void Cvar_SetString( string name, string value )
VM_CvarGetString,			// #28 void VM_CvarGetString( void )
VM_ComVA,				// #29 string va( ... )
VM_ComStrlen,			// #30 float strlen( string text )
VM_TimeStamp,			// #31 string Com_TimeStamp( float format )
VM_LocalCmd,			// #32 void LocalCmd( ... )
VM_SubString,			// #33 string substring( string s, float start, float length )
VM_AddCommand,			// #34 void Add_Command( string s )
NULL,				// #35 -- reserved --
NULL,				// #36 -- reserved --
NULL,				// #37 -- reserved --
NULL,				// #38 -- reserved --
NULL,				// #39 -- reserved --
NULL,				// #40 -- reserved --

// quakec intrinsics ( compiler will be lookup this functions, don't remove or rename )
VM_SpawnEdict,			// #41 entity spawn( void )
VM_RemoveEdict,			// #42 void remove( entity ent )
VM_NextEdict,			// #43 entity nextent( entity ent )
VM_CopyEdict,			// #44 void copyentity( entity src, entity dst )
NULL,				// #45 -- reserved --
NULL,				// #46 -- reserved --
NULL,				// #47 -- reserved --
NULL,				// #48 -- reserved --
NULL,				// #49 -- reserved --
NULL,				// #50 -- reserved --

// filesystem
VM_FS_Open,			// #51 float fopen( string filename, float mode )
VM_FS_Close,			// #52 void fclose( float handle )
VM_FS_Gets,			// #53 string fgets( float handle )
VM_FS_Puts,			// #54 void fputs( float handle, string s )
NULL,				// #55 -- reserved --
NULL,				// #56 -- reserved --
NULL,				// #57 -- reserved --
NULL,				// #58 -- reserved --
NULL,				// #59 -- reserved --
NULL,				// #60 -- reserved --

// mathlib
VM_min,				// #61 float min(float a, float b )
VM_max,				// #62 float max(float a, float b )
VM_bound,				// #63 float bound(float min, float val, float max)
VM_pow,				// #64 float pow(float x, float y)
VM_sin,				// #65 float sin(float f)
VM_cos,				// #66 float cos(float f)
VM_tan,				// #67 float tan(float f)
VM_asin,				// #68 float asin(float f)
VM_acos,				// #69 float acos(float f)
VM_atan,				// #70 float atan(float f)
VM_sqrt,				// #71 float sqrt(float f)
VM_rint,				// #72 float rint (float v)
VM_floor,				// #73 float floor(float v)
VM_ceil,				// #74 float ceil (float v)
VM_fabs,				// #75 float fabs (float f)
VM_mod,				// #76 float fmod( float val, float m )
NULL,				// #77 -- reserved --
NULL,				// #78 -- reserved --
VM_VectorNormalize,			// #79 vector VectorNormalize( vector v )
VM_VectorLength,			// #80 float VectorLength( vector v )
e10, e10,				// #81 - #100 are reserved for future expansions

// engine functions (like half-life enginefuncs_s)
PF_precache_model,			// #101 float precache_model(string s) 
PF_precache_sound,			// #102 float precache_sound(string s) 
PF_setmodel,			// #103 float setmodel(entity e, string m) 
PF_modelindex,			// #104 float model_index(string s)
PF_modelframes,			// #105 float model_frames(float modelindex)
PF_setsize,			// #106 void setsize(entity e, vector min, vector max)
PF_changelevel,			// #107 void changelevel(string mapname, string spotname)
NULL,				// #108 getSpawnParms
NULL,				// #109 SaveSpawnParms
PF_vectoyaw,			// #110 float vectoyaw(vector v) 
PF_vectoangles,			// #111 vector vectoangles(vector v) 
NULL,				// #112 moveToOrigin
PF_changeyaw,			// #113 void ChangeYaw( void )
PF_changepitch,			// #114 void ChangePitch( void )
VM_FindEdict,			// #115 entity find(entity start, .string fld, string match)
PF_getlightlevel,			// #116 float getEntityIllum( entity e )
PF_findradius,			// #117 entity FindInSphere(vector org, float rad)
PF_inpvs,				// #118 entity EntitiesInPVS( entity ent, float player )
PF_inphs,				// #119 entity EntitiesInPHS( entity ent, float player )
PF_makevectors,			// #120 void makevectors( vector dir, float hand )
VM_EdictError,			// #121 void objerror( string s )
PF_dropclient,			// #122 void dropclient( entity client )
PF_lookupactivity,			// #123 float LookupActivity( string model, float activity )
PF_create,			// #124 void CreateNamedEntity( string classname, vector org, vector ang )
PF_makestatic,			// #125 void makestatic(entity e)
NULL,				// #126 isEntOnFloor
PF_droptofloor,			// #127 float droptofloor( void )
PF_walkmove,			// #128 float walkmove(float yaw, float dist)
PF_setorigin,			// #129 void setorigin(entity e, vector o)
PF_sound,				// #130 void EmitSound(entity e, float chan, string samp, float vol, float attn)
PF_ambientsound,			// #131 void EmitAmbientSound(entity e, string samp)
PF_traceline,			// #132 void traceline(vector v1, vector v2, float mask, entity ignore)
PF_tracetoss,			// #133 void tracetoss (entity e, entity ignore)
NULL,				// #134 traceMonsterHull
PF_tracebox,			// #135 void tracebox (vector v1, vector mins, vector maxs, vector v2, float mask, entity ignore)
NULL,				// #136 traceModel
NULL,				// #137 traceTexture
NULL,				// #138 traceSphere
PF_aim,				// #139 vector aim(entity e, float speed)
PF_servercmd,			// #140 void server_command( string command )
PF_serverexec,			// #141 void server_execute( void )
PF_clientcmd,			// #142 void client_command( entity e, string s)
PF_particle,			// #143 void particle( vector origin, vector dir, float color, float count )
PF_lightstyle,			// #144 void lightstyle(float style, string value)
PF_getsequenceinfo,			// #145 float GetSequenceInfo( string model, float sequence )
PF_pointcontents,			// #146 float pointcontents(vector v) 
PF_BeginMessage,			// #147 void MsgBegin (float dest)
PF_EndMessage,			// #148 void MsgEnd(float to, vector pos, entity e)
PF_WriteByte,			// #149 void WriteByte (float f)
PF_WriteChar,			// #150 void WriteChar (float f)
PF_WriteShort,			// #151 void WriteShort (float f)
PF_WriteLong,			// #152 void WriteLong (float f)
PF_WriteAngle,			// #153 void WriteAngle (float f)
PF_WriteCoord,			// #154 void WriteCoord (float f)
PF_WriteString,			// #155 void WriteString (string s)
PF_WriteEntity,			// #156 void WriteEntity (entity s)
PF_getsequenceflags,		// #157 float GetSequenceFlags( string model, float sequence )
NULL,				// #158 regUserMsg
PF_checkbottom,			// #159 float checkbottom(entity e) 
NULL,				// #160 getBonePosition
PF_ClientPrint,			// #161 void ClientPrint( float type, entity client, string s)
PF_ServerPrint,			// #162 void ServerPrint(string s)
NULL,				// #163 getAttachment
NULL,				// #164 setView
NULL,				// #165 crosshairangle
PF_AreaPortalState,			// #166 void areaportal( entity ent, float state )
NULL,				// #167 compareFileTime
PF_geteyepos,			// #168 vector GetEyePosition( string model )
PF_InfoPrint,      			// #169 void Info_Print( entity client )
PF_InfoValueForKey,			// #170 string Info_ValueForKey( entity client, string key )
PF_InfoRemoveKey,			// #171 void Info_RemoveKey( entity client, string key )
PF_InfoSetValueForKey,		// #172 void Info_SetValueForKey( entity client, string key, string value )
PF_setsky,			// #173 void setsky( string name, vector angles, float speed )
NULL,				// #174 staticDecal
PF_lookupsequence			// #175 float LookupSequence( string model, string label )
};

const int vm_sv_numbuiltins = sizeof(vm_sv_builtins) / sizeof(prvm_builtin_t); //num of builtins

/*
==============
SpawnEntities

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.
==============
*/
void SV_SpawnEntities( const char *mapname, const char *entities )
{
	edict_t	*ent;
	int	i;

	MsgDev( D_NOTE, "SV_SpawnEntities()\n" );

	prog->protect_world = false; // allow to change world parms

	ent = PRVM_EDICT_NUM( 0 );
	memset (ent->progs.sv, 0, prog->progs->entityfields * 4);
	ent->priv.sv->free = false;
	ent->progs.sv->model = PRVM_SetEngineString( sv.configstrings[CS_MODELS] );
	ent->progs.sv->modelindex = 1; // world model
	ent->progs.sv->solid = SOLID_BSP;
	ent->progs.sv->movetype = MOVETYPE_PUSH;

	SV_ConfigString (CS_MAXCLIENTS, va("%i", Host_MaxClients()));
	prog->globals.sv->mapname = PRVM_SetEngineString( sv.name );

	// spawn the rest of the entities on the map
	*prog->time = sv.time;

	// set client fields on player ents
	for( i = 0; i < Host_MaxClients(); i++ )
	{
		// setup all clients
		ent = PRVM_EDICT_NUM( i );
		ent->priv.sv->client = svs.clients + i;
	}

	PRVM_ED_LoadFromFile( entities );
	prog->protect_world = true; // make world read-only
}

/*
===============
SV_InitGameProgs

Init the game subsystem for a new map
===============
*/
void SV_InitServerProgs( void )
{
	Msg( "\n" );
	PRVM_Begin;
	PRVM_InitProg( PRVM_SERVERPROG );

	prog->reserved_edicts = Host_MaxClients();
	prog->loadintoworld = true;

	if( !prog->loaded )
	{        
		prog->progs_mempool = Mem_AllocPool("Server Progs" );
		prog->name = "server";
		prog->builtins = vm_sv_builtins;
		prog->numbuiltins = vm_sv_numbuiltins;
		prog->max_edicts = host.max_edicts<<2;
		prog->limit_edicts = host.max_edicts;
		prog->edictprivate_size = sizeof(sv_edict_t);
		prog->begin_increase_edicts = SV_BeginIncreaseEdicts;
		prog->end_increase_edicts = SV_EndIncreaseEdicts;
		prog->init_edict = SV_InitEdict;
		prog->free_edict = SV_FreeEdict;
		prog->count_edicts = SV_CountEdicts;
		prog->load_edict = SV_LoadEdict;
		prog->restore_edict = SV_RestoreEdict;
                    prog->filecrc = PROG_CRC_SERVER;
		
		// using default builtins
		prog->init_cmd = VM_Cmd_Init;
		prog->reset_cmd = VM_Cmd_Reset;
		prog->error_cmd = VM_Error;
		PRVM_LoadProgs( va("%s/server.dat", GI->vprogs_dir ));
	}

	// try to get custom movement function from qc code
	svs.ClientMove = PRVM_ED_FindFunctionOffset( "ClientMove" );
	PRVM_End;
}

/*
===============
SV_ShutdownGameProgs

Called when either the entire server is being killed, or
it is changing to a different game directory.
===============
*/
void SV_FreeServerProgs( void )
{
	edict_t	*ent;
	int	i;

	if(!PRVM_ProgLoaded( PRVM_SERVERPROG ))
		return;

	SV_VM_Begin();
	for (i = 1; prog && i < prog->num_edicts; i++)
	{
		ent = PRVM_EDICT_NUM(i);
		SV_FreeEdict( ent );// release physic
	}
	SV_VM_End();
}