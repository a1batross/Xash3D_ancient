//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        sv_game.c - server.dat interface
//=======================================================================

#include "engine.h"
#include "server.h"

byte	*sav_base;

/*
============
SV_CalcBBox

Returns the actual bounding box of a bmodel. This is a big improvement over
what q2 normally does with rotating bmodels - q2 sets absmin, absmax to a cube
that will completely contain the bmodel at *any* rotation on *any* axis, whether
the bmodel can actually rotate to that angle or not. This leads to a lot of
false block tests in SV_Push if another bmodel is in the vicinity.
============
*/
void SV_CalcBBox(edict_t *ent, vec3_t mins, vec3_t maxs)
{
	vec3_t		forward, left, up, f1, l1, u1;
	vec3_t		p[8];
	int		i, j, k, j2, k4;

	for(k = 0; k < 2; k++)
	{
		k4 = k * 4;
		if(k) p[k4][2] = maxs[2];
		else p[k4][2] = mins[2];

		p[k4 + 1][2] = p[k4][2];
		p[k4 + 2][2] = p[k4][2];
		p[k4 + 3][2] = p[k4][2];

		for(j = 0; j < 2; j++)
		{
			j2 = j * 2;
			if(j) p[j2+k4][1] = maxs[1];
			else p[j2+k4][1] = mins[1];
			p[j2 + k4 + 1][1] = p[j2 + k4][1];

			for(i = 0; i < 2; i++)
			{
				if(i) p[i + j2 + k4][0] = maxs[0];
				else p[i + j2 + k4][0] = mins[0];
			}
		}
	}

	AngleVectors(ent->progs.sv->angles, forward, left, up);

	for(i = 0; i < 8; i++)
	{
		VectorScale(forward, p[i][0], f1);
		VectorScale(left, -p[i][1], l1);
		VectorScale(up, p[i][2], u1);
		VectorAdd(ent->progs.sv->origin, f1, p[i]);
		VectorAdd(p[i], l1, p[i]);
		VectorAdd(p[i], u1, p[i]);
	}

	VectorCopy(p[0], ent->progs.sv->mins);
	VectorCopy(p[0], ent->progs.sv->maxs);

	for(i = 1; i < 8; i++)
	{
		ent->progs.sv->mins[0] = min(ent->progs.sv->mins[0], p[i][0]);
		ent->progs.sv->mins[1] = min(ent->progs.sv->mins[1], p[i][1]);
		ent->progs.sv->mins[2] = min(ent->progs.sv->mins[2], p[i][2]);
		ent->progs.sv->maxs[0] = max(ent->progs.sv->maxs[0], p[i][0]);
		ent->progs.sv->maxs[1] = max(ent->progs.sv->maxs[1], p[i][1]);
		ent->progs.sv->maxs[2] = max(ent->progs.sv->maxs[2], p[i][2]);
	}
}

void SV_SetMinMaxSize (edict_t *e, float *min, float *max, bool rotate)
{
	int		i;

	for (i = 0; i < 3; i++)
		if (min[i] > max[i])
			PRVM_ERROR("SV_SetMinMaxSize: backwards mins/maxs");

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
	VectorSubtract (max, min, e->progs.sv->size );

	// TODO: fill also mass and density
	SV_LinkEdict (e);
}

void SV_CreatePhysBody( edict_t *ent )
{
	if( !ent || ent->progs.sv->movetype != MOVETYPE_PHYSIC ) return;
	ent->priv.sv->physbody = pe->CreateBody( ent->priv.sv, SV_GetModelPtr(ent), ent->progs.sv->m_pmatrix, ent->progs.sv->solid );
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
	if(i == 0) return;
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

void SV_ConfigString (int index, const char *val)
{
	if (index < 0 || index >= MAX_CONFIGSTRINGS)
		Host_Error ("configstring: bad index %i value %s\n", index, val);

	if (!*val) val = "";

	// change the string in sv
	strcpy (sv.configstrings[index], val);

	if (sv.state != ss_loading)
	{
		// send the update to everyone
		SZ_Clear (&sv.multicast);
		MSG_Begin(svc_configstring);
		MSG_WriteShort (&sv.multicast, index);
		MSG_WriteString (&sv.multicast, (char *)val);
		MSG_Send(MSG_ALL_R, vec3_origin, NULL );
	}
}

/*
============
Save\Load gamestate

savefile operations
============
*/
void SV_AddSaveLump( dsavehdr_t *hdr, file_t *f, int lumpnum, void *data, int len )
{
	lump_t	*lump;

	lump = &hdr->lumps[lumpnum];
	lump->fileofs = LittleLong( FS_Tell(f));
	lump->filelen = LittleLong(len);
	FS_Write(f, data, (len + 3) & ~3 ); // align
}

static void Cvar_AddToBuffer( const char *name, const char *string, const char *buffer, int *bufsize )
{
	if( com.strlen(name) >= 64 || com.strlen(string) >= 64 )
	{
		MsgDev(D_NOTE, "cvar too long: %s = %s\n", name, string);
		return;
	}
	*bufsize = com.strpack((char *)buffer, *bufsize, (char *)name, com.strlen(name)); 
	*bufsize = com.strpack((char *)buffer, *bufsize, (char *)string, com.strlen(string));
}

void SV_AddCvarLump( dsavehdr_t *hdr, file_t *f )
{
	int	bufsize = 1; // null terminator 
	char	*cvbuffer = Z_Malloc( MAX_INPUTLINE );

	Cvar_LookupVars( CVAR_LATCH, cvbuffer, &bufsize, Cvar_AddToBuffer );
	SV_AddSaveLump( hdr, f, LUMP_GAMECVARS, cvbuffer, bufsize );

	Mem_Free( cvbuffer ); // free buffer
}

void SV_AddCStrLump( dsavehdr_t *hdr, file_t *f )
{
	int	i, stringsize, bufsize = 1; // null terminator
	char	*csbuffer = Z_Malloc( MAX_CONFIGSTRINGS * MAX_QPATH );

	// pack the cfg string data
	for(i = 0; i < MAX_CONFIGSTRINGS; i++)
	{
		stringsize = bound(0, com.strlen(sv.configstrings[i]), MAX_QPATH);
		bufsize = com.strpack(csbuffer, bufsize, sv.configstrings[i], stringsize ); 
	}	
	SV_AddSaveLump( hdr, f, LUMP_CFGSTRING, csbuffer, bufsize );
	Mem_Free( csbuffer ); // free memory
}

void SV_WriteGlobal( dsavehdr_t *hdr, file_t *f )
{
	vfile_t	*h = VFS_Open( f, "wz" ); // zip-compression
	lump_t	*lump;
	int	len, pos;

	lump = &hdr->lumps[LUMP_GAMESTATE];
	lump->fileofs = LittleLong( FS_Tell(f));

	SV_VM_Begin();
	PRVM_ED_WriteGlobals( h );
	SV_VM_End();
	pos = VFS_Tell( h );

	FS_Write(f, &pos, sizeof(int)); // first four bytes is real filelength
	f = VFS_Close( h );

	len = LittleLong(FS_Tell(f));
	lump->filelen = LittleLong( len - lump->fileofs ); // store compressed file length
}

void SV_WriteLocals( dsavehdr_t *hdr, file_t *f )
{
	vfile_t	*h = VFS_Open( f, "wz" ); // zip-compression
	lump_t	*lump;
	int	i, len, pos;

	lump = &hdr->lumps[LUMP_GAMEENTS];
	lump->fileofs = LittleLong( FS_Tell(f));

	SV_VM_Begin();
	for(i = 0; i < prog->num_edicts; i++)
	{
		PRVM_ED_Write(h, PRVM_EDICT_NUM(i));
	}
	SV_VM_End();
	pos = VFS_Tell( h );
	
	FS_Write(f, &pos, sizeof(int)); // first four bytes is real filelength
	f = VFS_Close(h);
	len = LittleLong(FS_Tell(f));
	lump->filelen = LittleLong( len - lump->fileofs ); // store compressed file length
}

/*
=============
SV_WriteSaveFile
=============
*/
void SV_WriteSaveFile( char *name )
{
	char		path[MAX_SYSPATH];
	char		comment[32];
	dsavehdr_t	*header;
	file_t		*savfile;
	bool		autosave = false;
	byte		*portalopen = Z_Malloc( MAX_MAP_AREAPORTALS );
	int		portalsize;

	if( sv.state != ss_active )
		return;

	if(Cvar_VariableValue("deathmatch"))
	{
		MsgDev(D_ERROR, "SV_WriteSaveFile: can't savegame in a deathmatch\n");
		return;
	}
	if(maxclients->integer == 1 && svs.clients[0].edict->priv.sv->client->ps.stats[STAT_HEALTH] <= 0)
	{
		MsgDev(D_ERROR, "SV_WriteSaveFile: can't savegame while dead!\n");
		return;
	}

	if(!com.strcmp(name, "save0.bin")) autosave = true;
	com.sprintf (path, "save/%s", name );
	savfile = FS_Open( path, "wb");

	if(!savfile)
	{
		MsgDev(D_ERROR, "SV_WriteSaveFile: failed to open %s\n", path );
		return;
	}

	MsgDev (D_INFO, "Saving game..." );
	com.sprintf (comment, "%s - %s", sv.configstrings[CS_NAME], timestamp(TIME_FULL));

	header = (dsavehdr_t *)Z_Malloc( sizeof(dsavehdr_t));
	header->ident = LittleLong (IDSAVEHEADER);
	header->version = LittleLong (SAVE_VERSION);
	FS_Write( savfile, header, sizeof(dsavehdr_t));
          
	// write lumps
	pe->GetAreaPortals( &portalopen, &portalsize );
	SV_AddSaveLump( header, savfile, LUMP_COMMENTS, comment, sizeof(comment));
	SV_AddCStrLump( header, savfile );
	SV_AddSaveLump( header, savfile, LUMP_AREASTATE, portalopen, portalsize );
	SV_WriteGlobal( header, savfile );
	SV_AddSaveLump( header, savfile, LUMP_MAPNAME, svs.mapcmd, sizeof(svs.mapcmd));
	SV_AddCvarLump( header, savfile );
	SV_WriteLocals( header, savfile );
	
	// merge header
	FS_Seek( savfile, 0, SEEK_SET );
	FS_Write( savfile, header, sizeof(dsavehdr_t));
	FS_Close( savfile );
	Mem_Free( portalopen);
	Mem_Free( header );
	MsgDev(D_INFO, "done.\n");
}

void Sav_LoadComment( lump_t *l )
{
	byte	*in;
	int	size;

	in = (void *)(sav_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("Sav_LoadComment: funny lump size\n" );

	size = l->filelen / sizeof(*in);
	com.strncpy(svs.comment, in, size );
}

void Sav_LoadCvars( lump_t *l )
{
	char	name[64], string[64];
	int	size, pos = 0;
	byte	*in;

	in = (void *)(sav_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("Sav_LoadCvars: funny lump size\n" );
	size = l->filelen / sizeof(*in);

	while(pos < size)
	{
		pos = com.strunpack( in, pos, name );  
		pos = com.strunpack( in, pos, string );  
		Cvar_SetLatched( name, string );
	}
}

void Sav_LoadMapCmds( lump_t *l )
{
	byte	*in;
	int	size;

	in = (void *)(sav_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("Sav_LoadMapCmds: funny lump size\n" );

	size = l->filelen / sizeof(*in);
	com.strncpy(svs.mapcmd, in, size );
}

void Sav_LoadCfgString( lump_t *l )
{
	char	*in;
	int	i, pos = 0;

	in = (void *)(sav_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("Sav_LoadCfgString: funny lump size\n" );

	// unpack the cfg string data
	for(i = 0; i < MAX_CONFIGSTRINGS; i++)
	{
		pos = com.strunpack( in, pos, sv.configstrings[i] );  
	}
}

void Sav_LoadAreaPortals( lump_t *l )
{
	byte	*in;
	int	size;

	in = (void *)(sav_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("Sav_LoadAreaPortals: funny lump size\n" );

	size = l->filelen / sizeof(*in);
	pe->SetAreaPortals( in, size ); // CM_ReadPortalState
}

void Sav_LoadGlobal( lump_t *l )
{
	byte	*in, *globals, *ptr;
	int	size, realsize;

	in = (void *)(sav_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("Sav_LoadGlobal: funny lump size\n" );
	size = l->filelen / sizeof(*in);
	realsize = LittleLong(((int *)in)[0]);

	ptr = globals = Z_Malloc( realsize );
	VFS_Unpack( in+4, size, &globals, realsize );
	PRVM_ED_ParseGlobals( globals );
	Mem_Free( ptr ); // free globals
}

void Sav_LoadLocals( lump_t *l )
{
	byte	*in, *ents, *ptr;
	int	size, realsize, entnum = 0;

	in = (void *)(sav_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("Sav_LoadLocals: funny lump size\n" );
	size = l->filelen / sizeof(*in);
	realsize = LittleLong(((int *)in)[0]);

	ptr = ents = Z_Malloc( realsize );
	VFS_Unpack( in + sizeof(int), size, &ents, realsize );

	while(Com_SimpleGetToken(&ents))
	{
		edict_t	*ent;

		if(com_token[0] == '{')
		{
			if( entnum >= MAX_EDICTS )
				Host_Error("Sav_LoadLocals: too many edicts in save file\n" );
			while(entnum >= prog->max_edicts) PRVM_MEM_IncreaseEdicts();
			ent = PRVM_EDICT_NUM( entnum );
			memset(ent->progs.sv, 0, prog->progs->entityfields * 4);
			ent->priv.sv->free = false;

			// parse an edict
			PRVM_ED_ParseEdict(ents, ent);
			ent->priv.sv->serialnumber = entnum++; // increase serialnumber

			// link it into the bsp tree
			if (!ent->priv.sv->free) 
			{
				SV_LinkEdict( ent );
				SV_CreatePhysBody( ent );
				SV_SetPhysForce( ent ); // restore forces ...
				SV_SetMassCentre( ent ); // and mass force
			}
		}
	}

	prog->num_edicts = entnum;
	Mem_Free( ptr ); // free ents
}

/*
=============
SV_ReadSaveFile
=============
*/
void SV_ReadSaveFile( char *name )
{
	char		path[MAX_SYSPATH];
	dsavehdr_t	*header;
	byte		*savfile;
	int		i, id, size;

	com.sprintf(path, "save/%s", name );
	savfile = FS_LoadFile(path, &size );

	if(!savfile)
	{
		MsgDev(D_ERROR, "SV_ReadSaveFile: can't open %s\n", path );
		return;
	}

	header = (dsavehdr_t *)savfile;
	i = LittleLong (header->version);
	id = LittleLong (header->ident);

	if(id != IDSAVEHEADER) Host_Error("SV_ReadSaveFile: file %s is corrupted\n", path );
	if(i != SAVE_VERSION ) Host_Error("file %s from an older save version\n", path );

	sav_base = (byte *)header;

	Sav_LoadComment(&header->lumps[LUMP_COMMENTS]);
	Sav_LoadCvars(&header->lumps[LUMP_GAMECVARS]);
	
	SV_InitGame(); // start a new game fresh with new cvars
	Sav_LoadMapCmds(&header->lumps[LUMP_MAPNAME]);
	Mem_Free( savfile );
	CL_Drop();
}

/*
=============
SV_ReadLevelFile
=============
*/
void SV_ReadLevelFile( char *name )
{
	char		path[MAX_SYSPATH];
	dsavehdr_t	*header;
	byte		*savfile;
	int		i, id, size;

	com.sprintf (path, "save/%s", name );
	savfile = FS_LoadFile(path, &size );

	if(!savfile)
	{
		MsgDev(D_ERROR, "SV_ReadLevelFile: can't open %s\n", path );
		return;
	}

	header = (dsavehdr_t *)savfile;
	i = LittleLong (header->version);
	id = LittleLong (header->ident);

	if(id != IDSAVEHEADER) Host_Error("SV_ReadSaveFile: file %s is corrupted\n", path );
	if (i != SAVE_VERSION) Host_Error("file %s from an older save version\n", path );

	sav_base = (byte *)header;
	Sav_LoadCfgString(&header->lumps[LUMP_CFGSTRING]);
	Sav_LoadAreaPortals(&header->lumps[LUMP_AREASTATE]);
	Sav_LoadGlobal(&header->lumps[LUMP_GAMESTATE]);
	Sav_LoadLocals(&header->lumps[LUMP_GAMEENTS]);
	Mem_Free( savfile );
}

//FIXME: move into ui_cmds.c
bool Menu_ReadComment( char *comment, int savenum )
{
	dsavehdr_t	*header;
	byte		*savfile;
	int		i, id, size;

	if(!comment) return false;
	savfile = FS_LoadFile(va("save/save%i.bin", savenum), &size );

	if(!savfile) 
	{
		com.strncpy( comment, "<empty>", MAX_QPATH );
		return false;
	}

	header = (dsavehdr_t *)savfile;
	i = LittleLong (header->version);
	id = LittleLong (header->ident);

	if(id != IDSAVEHEADER || i != SAVE_VERSION)
	{
		com.strncpy( comment, "<corrupted>", MAX_QPATH );
		return false;
	}

	sav_base = (byte *)header;
	Sav_LoadComment(&header->lumps[LUMP_COMMENTS]);
	com.strncpy( comment, svs.comment, MAX_QPATH );
	Mem_Free( savfile );

	return true;
}

void SV_BeginIncreaseEdicts(void)
{
	int		i;
	edict_t		*ent;

	PRVM_Free( sv.moved_edicts );
	sv.moved_edicts = (edict_t **)PRVM_Alloc(prog->max_edicts * sizeof(edict_t *));

	// links don't survive the transition, so unlink everything
	for (i = 0, ent = prog->edicts; i < prog->max_edicts; i++, ent++)
	{
		if (!ent->priv.sv->free) SV_UnlinkEdict(prog->edicts + i); //free old entity
		memset(&ent->priv.sv->clusternums, 0, sizeof(ent->priv.sv->clusternums));
	}
	SV_ClearWorld();
}

void SV_EndIncreaseEdicts(void)
{
	int		i;
	edict_t		*ent;

	for (i = 0, ent = prog->edicts; i < prog->max_edicts; i++, ent++)
	{
		// link every entity except world
		if (!ent->priv.sv->free) SV_LinkEdict(ent);
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

	ed->priv.sv->freetime = sv.time;
	ed->priv.sv->free = true;

	ed->progs.sv->model = 0;
	ed->progs.sv->takedamage = 0;
	ed->progs.sv->modelindex = 0;
	ed->progs.sv->skin = 0;
	ed->progs.sv->frame = 0;
	ed->progs.sv->solid = 0;

	pe->RemoveBody( ed->priv.sv->physbody );
	VectorClear(ed->progs.sv->origin);
	VectorClear(ed->progs.sv->angles);
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
	return true;
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
===============================================================================

NETWORK MESSAGE WRITING

===============================================================================
*/
void PF_BeginMessage(void)
{
	int	svc_dest = (int)PRVM_G_FLOAT(OFS_PARM0);

	// some users can send message with engine index
	// reduce number to avoid overflow problems or cheating
	svc_dest = bound(svc_bad, svc_dest, svc_nop);

	MSG_Begin( svc_dest );
}

void PF_WriteByte (void){ MSG_WriteByte(&sv.multicast, (int)PRVM_G_FLOAT(OFS_PARM0)); }
void PF_WriteChar (void){ MSG_WriteChar(&sv.multicast, (int)PRVM_G_FLOAT(OFS_PARM0)); }
void PF_WriteShort (void){ MSG_WriteShort(&sv.multicast, (int)PRVM_G_FLOAT(OFS_PARM0)); }
void PF_WriteLong (void){ MSG_WriteLong(&sv.multicast, (int)PRVM_G_FLOAT(OFS_PARM0)); }
void PF_WriteFloat (void){ MSG_WriteFloat(&sv.multicast, PRVM_G_FLOAT(OFS_PARM0)); }
void PF_WriteAngle (void){ MSG_WriteAngle32(&sv.multicast, PRVM_G_FLOAT(OFS_PARM0)); }
void PF_WriteCoord (void){ MSG_WriteCoord32(&sv.multicast, PRVM_G_FLOAT(OFS_PARM0)); }
void PF_WriteString (void){ MSG_WriteString(&sv.multicast, PRVM_G_STRING(OFS_PARM0)); }
void PF_WriteEntity (void){ MSG_WriteShort(&sv.multicast, PRVM_G_EDICTNUM(OFS_PARM1)); } // entindex

void PF_EndMessage (void)
{
	int	send_to = (int)PRVM_G_FLOAT(OFS_PARM0);
	edict_t	*ed = PRVM_G_EDICT(OFS_PARM2);

	if(PRVM_NUM_FOR_EDICT(ed) > prog->num_edicts)
	{
		VM_Warning("MsgEnd: sending message from killed entity\n");
		return;
	}
	// align range
	send_to = bound(MSG_ONE, send_to, MSG_PVS_R);
	MSG_Send( send_to, PRVM_G_VECTOR(OFS_PARM1), ed );
}

/*
=================
PF_sprint

single print to a specific client

sprint(clientent, value)
=================
*/
void PF_sprint( void )
{
	client_state_t	*client;
	int		num;
	char		string[VM_STRINGTEMP_LENGTH];

	num = PRVM_G_EDICTNUM(OFS_PARM0);

	if( num < 1 || num > maxclients->value || svs.clients[num - 1].state != cs_spawned )
	{
		VM_Warning("tried to centerprint to a non-client\n");
		return;
	}

	client = svs.clients + num - 1;

	VM_VarString(1, string, sizeof(string));
	SV_ClientPrintf (client, PRINT_CHAT, "%s", string );
}


/*
=================
PF_centerprint

single print to a specific client

centerprint(clientent, value)
=================
*/
void PF_centerprint( void )
{
	client_state_t	*client;
	int		num;
	char 		string[VM_STRINGTEMP_LENGTH];

	num = PRVM_G_EDICTNUM(OFS_PARM0);

	if(num < 1 || num > maxclients->value || svs.clients[num-1].state != cs_spawned)
	{
		VM_Warning("tried to centerprint to a non-client\n");
		return;
	}

	client = svs.clients + num - 1;

	VM_VarString(1, string, sizeof(string));
	MSG_Begin( svc_centerprint );
	MSG_WriteString (&sv.multicast, string );
	MSG_Send(MSG_ONE_R, NULL, client->edict );
}

/*
=================
PF_inpvs

Also checks portalareas so that doors block sight
=================
*/
void PF_inpvs( void )
{
	float	*p1, *p2;
	int	leafnum, cluster;
	int	area1, area2;
	byte	*mask;

	p1 = PRVM_G_VECTOR(OFS_PARM0);
	p2 = PRVM_G_VECTOR(OFS_PARM1);

	leafnum = pe->PointLeafnum (p1);
	cluster = pe->LeafCluster (leafnum);
	area1 = pe->LeafArea (leafnum);
	mask = pe->ClusterPVS (cluster);

	leafnum = pe->PointLeafnum (p2);
	cluster = pe->LeafCluster (leafnum);
	area2 = pe->LeafArea (leafnum);

	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)))))
	{
		PRVM_G_FLOAT(OFS_RETURN) = 0;
	}
	else if (!pe->AreasConnected (area1, area2))
	{
		PRVM_G_FLOAT(OFS_RETURN) = 0;	// a door blocks sight
	}
	else PRVM_G_FLOAT(OFS_RETURN) = 1;
}

/*
=================
PF_inphs

Also checks portalareas so that doors block sound
=================
*/
void PF_inphs( void )
{
	float	*p1, *p2;
	int	leafnum, cluster;
	int	area1, area2;
	byte	*mask;

	p1 = PRVM_G_VECTOR(OFS_PARM0);
	p2 = PRVM_G_VECTOR(OFS_PARM1);

	leafnum = pe->PointLeafnum (p1);
	cluster = pe->LeafCluster (leafnum);
	area1 = pe->LeafArea (leafnum);
	mask = pe->ClusterPHS (cluster);

	leafnum = pe->PointLeafnum (p2);
	cluster = pe->LeafCluster (leafnum);
	area2 = pe->LeafArea (leafnum);

	if ( mask && (!(mask[cluster>>3] & (1<<(cluster&7)))))
	{
		PRVM_G_FLOAT(OFS_RETURN) = 0; // more than one bounce away
	}
	else if (!pe->AreasConnected (area1, area2))
	{
		PRVM_G_FLOAT(OFS_RETURN) = 0;	// a door blocks hearing
	}
	else PRVM_G_FLOAT(OFS_RETURN) = 1;
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

	// assume failure if it returns early
	PRVM_G_FLOAT(OFS_RETURN) = 0;

	ent = PRVM_PROG_TO_EDICT(prog->globals.sv->pev);
	if (ent == prog->edicts)
	{
		VM_Warning("droptofloor: can not modify world entity\n");
		return;
	}
	if (ent->priv.sv->free)
	{
		VM_Warning("droptofloor: can not modify free entity\n");
		return;
	}

	VectorCopy( ent->progs.sv->origin, end );
	end[2] -= 256;
	trace = SV_Trace(ent->progs.sv->origin, ent->progs.sv->mins, ent->progs.sv->maxs, end, ent, MASK_SOLID );

	if (trace.startsolid)
	{
		VM_Warning("droptofloor: %s startsolid at %g %g %g\n", PRVM_G_STRING(ent->progs.sv->classname), ent->progs.sv->origin[0], ent->progs.sv->origin[1], ent->progs.sv->origin[2]);
		SV_FreeEdict (ent);
		return;
	}

	if (trace.fraction != 1)
	{
		VectorCopy (trace.endpos, ent->progs.sv->origin);
		SV_LinkEdict (ent);
		ent->progs.sv->aiflags = (int)ent->progs.sv->aiflags | AI_ONGROUND;
		ent->progs.sv->groundentity = PRVM_EDICT_TO_PROG(trace.ent);
		PRVM_G_FLOAT(OFS_RETURN) = 1;
	}
}

/*
=================
PF_sound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.

=================
*/
void PF_sound (void)
{
	const char	*sample;
	int		channel, sound_idx;
	edict_t		*entity;
	int 		volume;
	float		attenuation;

	entity = PRVM_G_EDICT(OFS_PARM0);
	channel = (int)PRVM_G_FLOAT(OFS_PARM1);
	sample = PRVM_G_STRING(OFS_PARM2);
	volume = (int)(PRVM_G_FLOAT(OFS_PARM3) * 255);
	attenuation = PRVM_G_FLOAT(OFS_PARM4);

	if (volume < 0 || volume > 255)
	{
		VM_Warning("SV_StartSound: volume must be in range 0 - 255\n");
		return;
	}

	if (attenuation < 0 || attenuation > 4)
	{
		VM_Warning("SV_StartSound: attenuation must be in range 0-4\n");
		return;
	}

	if (channel < 0 || channel > 7)
	{
		VM_Warning("SV_StartSound: channel must be in range 0-7\n");
		return;
	}
          
          sound_idx = SV_SoundIndex( sample );
	SV_StartSound (NULL, entity, channel, sound_idx, volume / 255.0f, attenuation, 0 );
}

void PF_event( void )
{
	edict_t	*ent = PRVM_G_EDICT(OFS_PARM0);

	// event effects
	ent->priv.sv->event = (int)PRVM_G_FLOAT(OFS_PARM1);
}

/*
=================
PF_ambientsound

void ambientsound( entity e, string sample)
=================
*/
void PF_ambientsound( void )
{
	const char	*samp;
	edict_t		*soundent;

	soundent = PRVM_G_EDICT(OFS_PARM0);
	samp = PRVM_G_STRING(OFS_PARM1);

	if( soundent ) SV_AmbientSound( soundent, SV_SoundIndex( samp ), 0.0f, 0.0f ); // unused parms
}

/*
=================
PF_particle

particle(origin, color, count)
=================
*/
void PF_particle( void )
{
	float		*org, *dir;
	float		color;
	float		count;

	org = PRVM_G_VECTOR(OFS_PARM0);
	dir = PRVM_G_VECTOR(OFS_PARM1);
	color = PRVM_G_FLOAT(OFS_PARM2);
	count = PRVM_G_FLOAT(OFS_PARM3);
	
	SV_StartParticle (org, dir, (int)color, (int)count);
}

/*
=================
PF_traceline

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.

traceline (vector v1, v2, float mask, entity ignore)
=================
*/
void PF_traceline( void )
{
	float		*v1, *v2;
	trace_t		trace;
	int		mask;
	edict_t		*ent;

	prog->xfunction->builtinsprofile += 30;

	v1 = PRVM_G_VECTOR(OFS_PARM0);
	v2 = PRVM_G_VECTOR(OFS_PARM1);
	mask = (int)PRVM_G_FLOAT(OFS_PARM2);
	ent = PRVM_G_EDICT(OFS_PARM3);

	if(mask == 1) mask = MASK_SOLID;
	else if(mask == 2) mask = MASK_SHOT;
	else if(mask == 3) mask = MASK_MONSTERSOLID;
	else if(mask == 4) mask = MASK_WATER;
	else mask = MASK_ALL;

	if (IS_NAN(v1[0]) || IS_NAN(v1[1]) || IS_NAN(v1[2]) || IS_NAN(v2[0]) || IS_NAN(v1[2]) || IS_NAN(v2[2]))
		PRVM_ERROR("%s: NAN errors detected in traceline('%f %f %f', '%f %f %f', %i, entity %i)\n", PRVM_NAME, v1[0], v1[1], v1[2], v2[0], v2[1], v2[2], mask, PRVM_EDICT_TO_PROG(ent));

	trace = SV_Trace (v1, vec3_origin, vec3_origin, v2, ent, mask );

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

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.

tracebox (vector v1, vector mins, vector maxs, vector v2, float mask, entity ignore)
=================
*/
void PF_tracebox( void )
{
	float		*v1, *v2, *m1, *m2;
	trace_t		trace;
	int		mask;
	edict_t		*ent;

	prog->xfunction->builtinsprofile += 30;

	v1 = PRVM_G_VECTOR(OFS_PARM0);
	m1 = PRVM_G_VECTOR(OFS_PARM1);
	m2 = PRVM_G_VECTOR(OFS_PARM2);
	v2 = PRVM_G_VECTOR(OFS_PARM3);
	mask = (int)PRVM_G_FLOAT(OFS_PARM4);
	ent = PRVM_G_EDICT(OFS_PARM5);

	if(mask == 1) mask = MASK_SOLID;
	else if(mask == 2) mask = MASK_SHOT;
	else if(mask == 3) mask = MASK_MONSTERSOLID;
	else if(mask == 4) mask = MASK_WATER;
	else mask = MASK_ALL;

	if (IS_NAN(v1[0]) || IS_NAN(v1[1]) || IS_NAN(v1[2]) || IS_NAN(v2[0]) || IS_NAN(v1[2]) || IS_NAN(v2[2]))
		PRVM_ERROR("%s: NAN errors detected in tracebox('%f %f %f', '%f %f %f', '%f %f %f', '%f %f %f', %i, entity %i)\n", PRVM_NAME, v1[0], v1[1], v1[2], m1[0], m1[1], m1[2], m2[0], m2[1], m2[2], v2[0], v2[1], v2[2], mask, PRVM_EDICT_TO_PROG(ent));

	trace = SV_Trace (v1, m1, m2, v2, ent, mask );

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
PF_tracetoss

tracetoss (entity e, entity ignore)
=================
*/
void PF_tracetoss( void )
{
	trace_t		trace;
	edict_t		*ent;
	edict_t		*ignore;

	prog->xfunction->builtinsprofile += 600;

	ent = PRVM_G_EDICT(OFS_PARM0);
	if (ent == prog->edicts)
	{
		VM_Warning("tracetoss: can not use world entity\n");
		return;
	}
	ignore = PRVM_G_EDICT(OFS_PARM1);

	trace = SV_TraceToss (ent, ignore);

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

void PF_create( void )
{
	//FIXME: apply classname, origin and angles for new entity
	VM_create();
}

void PF_modelframes( void )
{
	cmodel_t	*mod;
	float	framecount = 0.0f;
	
	mod = pe->RegisterModel( sv.configstrings[CS_MODELS + (int)PRVM_G_FLOAT(OFS_PARM0)] );

	if( mod ) framecount = ( float )mod->numframes;
	PRVM_G_FLOAT(OFS_RETURN) = framecount;
}

void PF_changelevel( void )
{
	Cbuf_ExecuteText(EXEC_APPEND, va("changelevel %s\n", PRVM_G_STRING(OFS_PARM0)));
}

/*
=============
PF_checkbottom
=============
*/
void PF_checkbottom( void )
{
	PRVM_G_FLOAT(OFS_RETURN) = SV_CheckBottom(PRVM_G_EDICT(OFS_PARM0));
}

/*
=============
PF_pointcontents
=============
*/
void PF_pointcontents( void )
{
	PRVM_G_FLOAT(OFS_RETURN) = SV_PointContents(PRVM_G_VECTOR(OFS_PARM0), NULL );
}

/*
=============
PF_makestatic
=============
*/
void PF_makestatic( void )
{
	// quake1 legacy
	PRVM_ED_Free(PRVM_G_EDICT(OFS_PARM0));
}

/*
===============
PF_walkmove

float walkmove(float yaw, float dist)
===============
*/
void PF_walkmove( void )
{
	edict_t		*ent;
	float		yaw, dist;
	vec3_t		move;
	mfunction_t	*oldf;
	int 		oldpev;

	// assume failure if it returns early
	PRVM_G_FLOAT(OFS_RETURN) = 0;

	ent = PRVM_PROG_TO_EDICT(prog->globals.sv->pev);
	if (ent == prog->edicts)
	{
		VM_Warning("walkmove: can not modify world entity\n");
		return;
	}
	if (ent->priv.sv->free)
	{
		VM_Warning("walkmove: can not modify free entity\n");
		return;
	}

	yaw = PRVM_G_FLOAT(OFS_PARM0);
	dist = PRVM_G_FLOAT(OFS_PARM1);

	if (!((int)ent->progs.sv->aiflags & (AI_ONGROUND|AI_FLY|AI_SWIM)))
		return;

	yaw = yaw*M_PI*2 / 360;

	move[0] = cos(yaw)*dist;
	move[1] = sin(yaw)*dist;
	move[2] = 0;

	// save program state, because SV_movestep may call other progs
	oldf = prog->xfunction;
	oldpev = prog->globals.sv->pev;

	PRVM_G_FLOAT(OFS_RETURN) = SV_MoveStep(ent, move, true);

	// restore program state
	prog->xfunction = oldf;
	prog->globals.sv->pev = oldpev;
}

/*
===============
PF_lightstyle

void lightstyle(float style, string value) 
===============
*/
void PF_lightstyle( void )
{
	int		style;
	const char	*val;

	style = (int)PRVM_G_FLOAT(OFS_PARM0);
	val = PRVM_G_STRING(OFS_PARM1);

	if( (uint) style >= MAX_LIGHTSTYLES )
	{
		PRVM_ERROR( "PF_lightstyle: style: %i >= 64", style );
	}
	SV_ConfigString (CS_LIGHTS + style, val );
}

/*
=============
PF_aim

Pick a vector for the player to shoot along
vector aim(entity, missilespeed)
=============
*/
void PF_aim( void )
{
	edict_t		*ent, *check, *bestent;
	vec3_t		start, dir, end, bestdir;
	int		i, j;
	trace_t		tr;
	float		dist, bestdist;
	float		speed;
          int		flags = Cvar_VariableValue( "dmflags" );
	
	// assume failure if it returns early
	VectorCopy(prog->globals.sv->v_forward, PRVM_G_VECTOR(OFS_RETURN));

	ent = PRVM_G_EDICT(OFS_PARM0);
	if (ent == prog->edicts)
	{
		VM_Warning("aim: can not use world entity\n");
		return;
	}
	if (ent->priv.sv->free)
	{
		VM_Warning("aim: can not use free entity\n");
		return;
	}
	speed = PRVM_G_FLOAT(OFS_PARM1);

	VectorCopy (ent->progs.sv->origin, start);
	start[2] += 20;

	// try sending a trace straight
	VectorCopy (prog->globals.sv->v_forward, dir);
	VectorMA (start, 2048, dir, end);
	tr = SV_Trace (start, vec3_origin, vec3_origin, end, ent, MASK_ALL );

	if (tr.ent && ((edict_t *)tr.ent)->progs.sv->takedamage == 2 && (flags & DF_NO_FRIENDLY_FIRE || ent->progs.sv->team <=0 || ent->progs.sv->team != ((edict_t *)tr.ent)->progs.sv->team))
	{
		VectorCopy (prog->globals.sv->v_forward, PRVM_G_VECTOR(OFS_RETURN));
		return;
	}

	// try all possible entities
	VectorCopy (dir, bestdir);
	bestdist = 0.5f;
	bestent = NULL;

	check = PRVM_NEXT_EDICT(prog->edicts);
	for (i = 1; i < prog->num_edicts; i++, check = PRVM_NEXT_EDICT(check))
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
		tr = SV_Trace (start, vec3_origin, vec3_origin, end, ent, MASK_ALL );
		if (tr.ent == check)
		{	
			// can shoot at this one
			bestdist = dist;
			bestent = check;
		}
	}

	if (bestent)
	{
		VectorSubtract (bestent->progs.sv->origin, ent->progs.sv->origin, dir);
		dist = DotProduct (dir, prog->globals.sv->v_forward);
		VectorScale (prog->globals.sv->v_forward, dist, end);
		end[2] = dir[2];
		VectorNormalize (end);
		VectorCopy (end, PRVM_G_VECTOR(OFS_RETURN));
	}
	else
	{
		VectorCopy (bestdir, PRVM_G_VECTOR(OFS_RETURN));
	}
}

/*
===============
PF_Configstring

===============
*/
void PF_configstring( void )
{
	SV_ConfigString((int)PRVM_G_FLOAT(OFS_PARM0), PRVM_G_STRING(OFS_PARM1));
}

void PF_areaportalstate( void )
{
	pe->SetAreaPortalState((int)PRVM_G_FLOAT(OFS_PARM0), (bool)PRVM_G_FLOAT(OFS_PARM1));
}

/*
==============
PF_changeyaw

This was a major timewaster in progs, so it was converted to C
==============
*/
void PF_changeyaw( void )
{
	edict_t		*ent;

	ent = PRVM_PROG_TO_EDICT(prog->globals.sv->pev);
	if (ent == prog->edicts)
	{
		VM_Warning("changeyaw: can not modify world entity\n");
		return;
	}
	if (ent->priv.sv->free)
	{
		VM_Warning("changeyaw: can not modify free entity\n");
		return;
	}

	ent->progs.sv->angles[1] = SV_AngleMod( ent->progs.sv->ideal_yaw, anglemod(ent->progs.sv->angles[1]), ent->progs.sv->yaw_speed );
}

/*
==============
PF_changepitch
==============
*/
void PF_changepitch( void )
{
	edict_t		*ent;

	ent = PRVM_PROG_TO_EDICT(prog->globals.sv->pev);
	if (ent == prog->edicts)
	{
		VM_Warning("changepitch: can not modify world entity\n");
		return;
	}
	if (ent->priv.sv->free)
	{
		VM_Warning("changepitch: can not modify free entity\n");
		return;
	}
	ent->progs.sv->angles[0] = SV_AngleMod( 30, anglemod(ent->progs.sv->angles[0]), 30 );	
}

/*
=================
PF_findradius

Returns a chain of entities that have origins within a spherical area

findradius (origin, radius)
=================
*/
void PF_findradius( void )
{
	edict_t *ent, *chain;
	vec_t radius, radius2;
	vec3_t org, eorg;
	int i;
	chain = (edict_t *)prog->edicts;

	VectorCopy(PRVM_G_VECTOR(OFS_PARM0), org);
	radius = PRVM_G_FLOAT(OFS_PARM1);
	radius2 = radius * radius;

	ent = prog->edicts;
	for (i = 1; i < prog->num_edicts ; i++, ent = PRVM_NEXT_EDICT(ent))
	{
		if (ent->priv.sv->free) continue;
		if (ent->progs.sv->solid == SOLID_NOT) continue;
		VectorSubtract(org, ent->progs.sv->origin, eorg);
		VectorMAMAM(1, eorg, 0.5f, ent->progs.sv->mins, 0.5f, ent->progs.sv->maxs, eorg);

		if (DotProduct(eorg, eorg) < radius2)
		{
			ent->progs.sv->chain = PRVM_EDICT_TO_PROG(chain);
			chain = ent;
		}
	}
	VM_RETURN_EDICT(chain);
}

void PF_precache_model( void )
{
	PRVM_G_FLOAT(OFS_RETURN) = SV_ModelIndex(PRVM_G_STRING(OFS_PARM0));
}

void PF_precache_sound( void )
{
	PRVM_G_FLOAT(OFS_RETURN) = SV_SoundIndex(PRVM_G_STRING(OFS_PARM0));
}

void PF_modelindex( void )
{
	int index = SV_FindIndex (PRVM_G_STRING(OFS_PARM0), CS_MODELS, MAX_MODELS, false);	

	if(!index) VM_Warning("modelindex: %s not precached\n", PRVM_G_STRING(OFS_PARM0));
	PRVM_G_FLOAT(OFS_RETURN) = index; 
}

void PF_decalindex( void )
{
	// it will precache new decals too
	PRVM_G_FLOAT(OFS_RETURN) = SV_DecalIndex(PRVM_G_STRING(OFS_PARM0));
}

void PF_imageindex( void )
{
	// it will precache new images too
	PRVM_G_FLOAT(OFS_RETURN) = SV_ImageIndex(PRVM_G_STRING(OFS_PARM0));
}

void PF_getlightlevel( void )
{
	edict_t		*ent;

	ent = PRVM_G_EDICT(OFS_PARM0);

	if (ent == prog->edicts)
	{
		VM_Warning("getlightlevel: can't get light level at world entity\n");
		return;
	}
	if (ent->priv.sv->free)
	{
		VM_Warning("getlightlevel: can't get light level at free entity\n");
		return;
	}	

	PRVM_G_FLOAT(OFS_RETURN) = 1.0; //FIXME: implement
}

/*
=================
PF_setstats

void setstats(entity client, float stat_num, float value)
=================
*/
void PF_setstats( void )
{
	edict_t		*e;	
	int		stat_num;
	const char	*string;
	short		value;

	e = PRVM_G_EDICT(OFS_PARM0);
	if(!e->priv.sv->client)
	{
		VM_Warning("setstats: stats applied only for players\n");
		return;
	}
	stat_num = (int)PRVM_G_FLOAT(OFS_PARM1);
	if(stat_num < 0 || stat_num > MAX_STATS)
	{
		VM_Warning("setstats: invalid stats number\n");
		return;
	}
	string = PRVM_G_STRING(OFS_PARM2);

	switch(stat_num)
	{
	case STAT_ZOOM:
	case STAT_SPEED:
	case STAT_CHASE:
	case STAT_HELPICON:
	case STAT_AMMO_ICON:
	case STAT_ARMOR_ICON:
	case STAT_TIMER_ICON:
	case STAT_HEALTH_ICON:
	case STAT_PICKUP_ICON:
	case STAT_SELECTED_ICON:
	case STAT_SELECTED_ITEM:
		value = SV_ImageIndex( string );
		break;
	case STAT_AMMO:
	case STAT_FRAGS:
	case STAT_TIMER:
	case STAT_ARMOR:
	case STAT_HEALTH:
	case STAT_FLASHES:
	case STAT_LAYOUTS:
	case STAT_SPECTATOR:
		value = atoi( string );
		break;
	default:
		MsgWarn("unknown stat type %d\n", stat_num );
		return;
	}
	e->priv.sv->client->ps.stats[stat_num] = value;
}

/*
=================
PF_setmodel

setmodel(entity, model)
=================
*/
void PF_setmodel( void )
{
	edict_t		*e;

	e = PRVM_G_EDICT(OFS_PARM0);
	if (e == prog->edicts)
	{
		VM_Warning("setmodel: can not modify world entity\n");
		return;
	}
	if (e->priv.sv->free)
	{
		VM_Warning("setmodel: can not modify free entity\n");
		return;
	}
	SV_SetModel( e, PRVM_G_STRING(OFS_PARM1)); 
}

/*
=================
PF_setsize

the size box is rotated by the current angle
setsize (entity, minvector, maxvector)
=================
*/
void PF_setsize( void )
{
	edict_t	*e;
	float	*min, *max;

	e = PRVM_G_EDICT(OFS_PARM0);
	if (e == prog->edicts)
	{
		VM_Warning("setsize: can not modify world entity\n");
		return;
	}
	if (e->priv.sv->free)
	{
		VM_Warning("setsize: can not modify free entity\n");
		return;
	}

	min = PRVM_G_VECTOR(OFS_PARM1);
	max = PRVM_G_VECTOR(OFS_PARM2);
	SV_SetMinMaxSize (e, min, max, !VectorIsNull(e->progs.sv->angles));
}

/*
=================
PF_setorigin

This is the only valid way to move an object without using the physics of the world (setting velocity and waiting).  
Directly changing origin will not set internal links correctly, so clipping would be messed up.  
This should be called when an object is spawned, and then only if it is teleported.

setorigin (entity, origin)
=================
*/
void PF_setorigin( void )
{
	edict_t	*e;
	float	*org;

	e = PRVM_G_EDICT(OFS_PARM0);
	if (e == prog->edicts)
	{
		VM_Warning("setorigin: can not modify world entity\n");
		return;
	}
	if (e->priv.sv->free)
	{
		VM_Warning("setorigin: can not modify free entity\n");
		return;
	}
	org = PRVM_G_VECTOR(OFS_PARM1);
	VectorCopy (org, e->progs.sv->origin);
	SV_LinkEdict (e);
}

/*
==============
PF_dropclient
==============
*/
void PF_dropclient( void )
{
	int clientnum = PRVM_G_EDICTNUM(OFS_PARM0) - 1;

	if (clientnum < 0 || clientnum >= host.maxclients)
	{
		VM_Warning("dropclient: not a client\n");
		return;
	}
	if (svs.clients[clientnum].state != cs_spawned)
	{
		VM_Warning("dropclient: that client slot is not connected\n");
		return;
	}
	SV_DropClient(svs.clients + clientnum);
}

/*
==============
PF_spawnclient
==============
*/
void PF_spawnclient( void )
{
	int i;
	edict_t	*ed;
	prog->xfunction->builtinsprofile += 2;
	ed = prog->edicts;

	for (i = 0; i < maxclients->value; i++)
	{
		if (svs.clients[i].state != cs_spawned)
		{
			prog->xfunction->builtinsprofile += 100;
			svs.clients[i].state = cs_connected;
			ed = PRVM_EDICT_NUM(i + 1);
			SV_ClientConnect(ed, "" );
			break;
		}
	}
	VM_RETURN_EDICT(ed);
}

//NOTE: intervals between various "interfaces" was leave for future expansions
prvm_builtin_t vm_sv_builtins[] = 
{
NULL,					// #0

// network messaging
PF_BeginMessage,				// #1 void MsgBegin (float dest)
PF_WriteByte,				// #2 void WriteByte (float f)
PF_WriteChar,				// #3 void WriteChar (float f)
PF_WriteShort,				// #4 void WriteShort (float f)
PF_WriteLong,				// #5 void WriteLong (float f)
PF_WriteFloat,				// #6 void WriteFloat (float f)
PF_WriteAngle,				// #7 void WriteAngle (float f)
PF_WriteCoord,				// #8 void WriteCoord (float f)
PF_WriteString,				// #9 void WriteString (string s)
PF_WriteEntity,				// #10 void WriteEntity (entity s)
PF_EndMessage,				// #11 void MsgEnd(float to, vector pos, entity e)

// mathlib
VM_min,					// #12 float min(float a, float b )
VM_max,					// #13 float max(float a, float b )
VM_bound,					// #14 float bound(float min, float val, float max)
VM_pow,					// #15 float pow(float x, float y)
VM_sin,					// #16 float sin(float f)
VM_cos,					// #17 float cos(float f)
VM_sqrt,					// #18 float sqrt(float f)
VM_rint,					// #19 float rint (float v)
VM_floor,					// #20 float floor(float v)
VM_ceil,					// #21 float ceil (float v)
VM_fabs,					// #22 float fabs (float f)
VM_random_long,				// #23 float random_long( void )
VM_random_float,				// #24 float random_float( void )
NULL,                                             // #25
NULL,                                             // #26
NULL,                                             // #27
NULL,                                             // #28
NULL,                                             // #29
NULL,                                             // #30

// vector mathlib
VM_normalize,				// #31 vector normalize(vector v) 
VM_veclength,				// #32 float veclength(vector v) 
VM_vectoyaw,				// #33 float vectoyaw(vector v) 
VM_vectoangles,				// #34 vector vectoangles(vector v) 
VM_randomvec,				// #35 vector randomvec( void )
VM_vectorvectors,				// #36 void vectorvectors(vector dir)
VM_makevectors,				// #37 void makevectors(vector dir)
VM_makevectors2,				// #38 void makevectors2(vector dir)
NULL,					// #39
NULL,					// #40

// stdlib functions
VM_atof,					// #41 float atof(string s)
VM_ftoa,					// #42 string ftoa(float s)
VM_vtoa,					// #43 string vtoa(vector v)
VM_atov,					// #44 vector atov(string s)
VM_print,					// #45 void Msg( ... )
VM_wprint,				// #46 void MsgWarn( ... )
VM_objerror,				// #47 void Error( ... )
VM_bprint,				// #48 void bprint(string s)
PF_sprint,				// #49 void sprint(entity client, string s)
PF_centerprint,				// #50 void centerprint(entity client, strings) 
VM_cvar,					// #51 float cvar(string s)
VM_cvar_set,				// #52 void cvar_set(string var, string val)
VM_allocstring,				// #53 string AllocString(string s)
VM_freestring,				// #54 void FreeString(string s)
VM_strlen,				// #55 float strlen(string s)
VM_strcat,				// #56 string strcat(string s1, string s2)
VM_argv,					// #57 string argv( float parm )
NULL,					// #58
NULL,					// #59
NULL,					// #60

// internal debugger
VM_break,					// #61 void break( void )
VM_crash,					// #62 void crash( void )
VM_coredump,				// #63 void coredump( void )
VM_stackdump,				// #64 void stackdump( void )
VM_traceon,				// #65 void trace_on( void )
VM_traceoff,				// #66 void trace_off( void )
VM_eprint,				// #67 void dump_edict(entity e) 
VM_nextent,				// #68 entity nextent(entity e)
NULL,					// #69
NULL,					// #70

// engine functions (like half-life enginefuncs_s)
PF_precache_model,				// #71 float precache_model(string s) 
PF_precache_sound,				// #72 float precache_sound(string s) 
PF_setmodel,				// #73 float setmodel(entity e, string m) 
PF_modelindex,				// #74 float model_index(string s)
PF_decalindex,				// #75 float decal_index(string s)
PF_imageindex,				// #76 float image_index(string s)
PF_setsize,				// #77 void setsize(entity e, vector min, vector max)
PF_changelevel,				// #78 void changelevel(string mapname, string spotname)
PF_changeyaw,				// #79 void ChangeYaw( void )
PF_changepitch,				// #80 void ChangePitch( void )
VM_find,					// #81 entity find(entity start, .string fld, string match)
PF_getlightlevel,				// #82 float getEntityIllum( entity e )
PF_findradius,				// #83 entity FindInSphere(vector org, float rad)
PF_inpvs,					// #84 float InPVS( vector v1, vector v2 )				
PF_inphs,					// #85 float InPHS( vector v1, vector v2 )
PF_create,				// #86 entity create( string name, string model, vector org )
VM_remove,				// #87 void remove( entity e )
PF_droptofloor,				// #88 float droptofloor( void )
PF_walkmove,				// #89 float walkmove(float yaw, float dist)
PF_setorigin,				// #90 void setorigin(entity e, vector o)
PF_sound,					// #91 void sound(entity e, float chan, string samp, float vol, float attn)
PF_ambientsound,				// #92 void ambientsound(entity e, string samp)
PF_traceline,				// #93 void traceline(vector v1, vector v2, float mask, entity ignore)
PF_tracetoss,				// #94 void tracetoss (entity e, entity ignore)
PF_tracebox,				// #95 void tracebox (vector v1, vector mins, vector maxs, vector v2, float mask, entity ignore)
PF_checkbottom,				// #96 float checkbottom(entity e) 
PF_lightstyle,				// #97 void lightstyle(float style, string value)
PF_pointcontents,				// #98 float pointcontents(vector v) 
PF_aim,					// #99 vector aim(entity e, float speed)
VM_servercmd,				// #100 void server_command( string command )
VM_clientcmd,				// #101 void client_command( entity e, string s)
PF_particle,				// #102 void particle(vector o, vector d, float color, float count)
PF_areaportalstate,				// #103 void areaportal_state( float num, float state )
PF_setstats,				// #104 void setstats(entity e, float f, string stats)
PF_configstring,				// #105 void configstring(float num, string s)
PF_makestatic,				// #106 void makestatic(entity e)
PF_modelframes,				// #107 float model_frames(float modelindex)
PF_event,					// #108 void set_effect( entity e, float effect )
NULL,					// #109
NULL,					// #110
NULL,					// #111
NULL,					// #112
NULL,					// #113
NULL,					// #114
NULL,					// #115
NULL,					// #116
NULL,					// #117
NULL,					// #118
NULL,					// #119
e10, e10, e10, e10, e10, e10, e10, e10,		// #120-199
NULL,					// #200
NULL,					// #201
NULL,					// #202
NULL,					// #203
NULL,					// #204
NULL,					// #205
NULL,					// #206
NULL,					// #207
NULL,					// #208
NULL,					// #209
NULL,					// #210
NULL,					// #211
NULL,					// #212
NULL,					// #213
NULL,					// #214
NULL,					// #215
NULL,					// #216
NULL,					// #217
NULL,					// #218
NULL,					// #219
e10,					// #220-#229
e10,					// #230-#239
e10,					// #240-#249
e10,					// #250-#259
e10,					// #260-#269
e10,					// #270-#279
e10,					// #280-#289
e10,					// #290-#299
e10, e10, e10, e10, e10, e10, e10, e10, e10, e10,	// #300-399
NULL,					// #400
NULL,					// #401
VM_findchain,				// #402 entity(.string fld, string match) findchain (DP_QC_FINDCHAIN)
VM_findchainfloat,				// #403 entity(.float fld, float match) findchainfloat (DP_QC_FINDCHAINFLOAT)
NULL,					// #404 void(vector org, string modelname, float startframe, float endframe, float framerate) effect (DP_SV_EFFECT)
NULL,					// #405 void(vector org, vector velocity, float howmany) te_blood (DP_TE_BLOOD)
NULL,					// #406 void(vector mincorner, vector maxcorner, float explosionspeed, float howmany) te_bloodshower (DP_TE_BLOODSHOWER)
NULL,					// #407 void(vector org, vector color) te_explosionrgb (DP_TE_EXPLOSIONRGB)
NULL,					// #408 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color, float gravityflag, float randomveljitter) te_particlecube (DP_TE_PARTICLECUBE)
NULL,					// #409 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlerain (DP_TE_PARTICLERAIN)
NULL,					// #410 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlesnow (DP_TE_PARTICLESNOW)
NULL,					// #411 void(vector org, vector vel, float howmany) te_spark (DP_TE_SPARK)
NULL,					// #412 void(vector org) te_gunshotquad (DP_QUADEFFECTS1)
NULL,					// #413 void(vector org) te_spikequad (DP_QUADEFFECTS1)
NULL,					// #414 void(vector org) te_superspikequad (DP_QUADEFFECTS1)
NULL,					// #415 void(vector org) te_explosionquad (DP_QUADEFFECTS1)
NULL,					// #416 void(vector org) te_smallflash (DP_TE_SMALLFLASH)
NULL,					// #417 void(vector org, float radius, float lifetime, vector color) te_customflash (DP_TE_CUSTOMFLASH)
NULL,					// #418 void(vector org) te_gunshot (DP_TE_STANDARDEFFECTBUILTINS)
NULL,					// #419 void(vector org) te_spike (DP_TE_STANDARDEFFECTBUILTINS)
NULL,					// #420 void(vector org) te_superspike (DP_TE_STANDARDEFFECTBUILTINS)
NULL,					// #421 void(vector org) te_explosion (DP_TE_STANDARDEFFECTBUILTINS)
NULL,					// #422 void(vector org) te_tarexplosion (DP_TE_STANDARDEFFECTBUILTINS)
NULL,					// #423 void(vector org) te_wizspike (DP_TE_STANDARDEFFECTBUILTINS)
NULL,					// #424 void(vector org) te_knightspike (DP_TE_STANDARDEFFECTBUILTINS)
NULL,					// #425 void(vector org) te_lavasplash (DP_TE_STANDARDEFFECTBUILTINS)
NULL,					// #426 void(vector org) te_teleport (DP_TE_STANDARDEFFECTBUILTINS)
NULL,					// #427 void(vector org, float colorstart, float colorlength) te_explosion2 (DP_TE_STANDARDEFFECTBUILTINS)
NULL,					// #428 void(entity own, vector start, vector end) te_lightning1 (DP_TE_STANDARDEFFECTBUILTINS)
NULL,					// #429 void(entity own, vector start, vector end) te_lightning2 (DP_TE_STANDARDEFFECTBUILTINS)
NULL,					// #430 void(entity own, vector start, vector end) te_lightning3 (DP_TE_STANDARDEFFECTBUILTINS)
NULL,					// #431 void(entity own, vector start, vector end) te_beam (DP_TE_STANDARDEFFECTBUILTINS)
NULL,					// #432
NULL,					// #433 void(vector org) te_plasmaburn (DP_TE_PLASMABURN)
NULL,					// #434 float(entity e, float s) getsurfacenumpoints (DP_QC_GETSURFACE)
NULL,					// #435 vector(entity e, float s, float n) getsurfacepoint (DP_QC_GETSURFACE)
NULL,					// #436 vector(entity e, float s) getsurfacenormal (DP_QC_GETSURFACE)
NULL,					// #437 string(entity e, float s) getsurfacetexture (DP_QC_GETSURFACE)
NULL,					// #438 float(entity e, vector p) getsurfacenearpoint (DP_QC_GETSURFACE)
NULL,					// #439 vector(entity e, float s, vector p) getsurfaceclippedpoint (DP_QC_GETSURFACE)
NULL,					// #440
VM_tokenize,				// #441 float(string s) tokenize (KRIMZON_SV_PARSECLIENTCOMMAND)
VM_argv,					// #442 string(float n) argv (KRIMZON_SV_PARSECLIENTCOMMAND)
NULL,					// #443 void(entity e, entity tagentity, string tagname) setattachment (DP_GFX_QUAKE3MODELTAGS)
VM_search_begin,				// #444 float(string pattern, float caseinsensitive, float quiet) search_begin (DP_FS_SEARCH)
VM_search_end,				// #445 void(float handle) search_end (DP_FS_SEARCH)
VM_search_getsize,				// #446 float(float handle) search_getsize (DP_FS_SEARCH)
VM_search_getfilename,			// #447 string(float handle, float num) search_getfilename (DP_FS_SEARCH)
VM_cvar_string,				// #448 string(string s) cvar_string (DP_QC_CVAR_STRING)
VM_findflags,				// #449 entity(entity start, .float fld, float match) findflags (DP_QC_FINDFLAGS)
VM_findchainflags,				// #450 entity(.float fld, float match) findchainflags (DP_QC_FINDCHAINFLAGS)
NULL,					// #451 float(entity ent, string tagname) gettagindex (DP_QC_GETTAGINFO)
NULL,					// #452 vector(entity ent, float tagindex) gettaginfo (DP_QC_GETTAGINFO)
PF_dropclient,				// #453 void(entity clent) dropclient (DP_SV_DROPCLIENT)
PF_spawnclient,				// #454 entity() spawnclient (DP_SV_BOTCLIENT)
NULL,					// #455
NULL,					// #456
NULL,					// #457 void(vector org, vector vel, float howmany) te_flamejet = #457 (DP_TE_FLAMEJET)
NULL,					// #458
NULL,					// #459
VM_buf_create,				// #460 float() buf_create (DP_QC_STRINGBUFFERS)
VM_buf_del,				// #461 void(float bufhandle) buf_del (DP_QC_STRINGBUFFERS)
VM_buf_getsize,				// #462 float(float bufhandle) buf_getsize (DP_QC_STRINGBUFFERS)
VM_buf_copy,				// #463 void(float bufhandle_from, float bufhandle_to) buf_copy (DP_QC_STRINGBUFFERS)
VM_buf_sort,				// #464 void(float bufhandle, float sortpower, float backward) buf_sort (DP_QC_STRINGBUFFERS)
VM_buf_implode,				// #465 string(float bufhandle, string glue) buf_implode (DP_QC_STRINGBUFFERS)
VM_bufstr_get,				// #466 string(float bufhandle, float string_index) bufstr_get (DP_QC_STRINGBUFFERS)
VM_bufstr_set,				// #467 void(float bufhandle, float string_index, string str) bufstr_set (DP_QC_STRINGBUFFERS)
VM_bufstr_add,				// #468 float(float bufhandle, string str, float order) bufstr_add (DP_QC_STRINGBUFFERS)
VM_bufstr_free,				// #469 void(float bufhandle, float string_index) bufstr_free (DP_QC_STRINGBUFFERS)
NULL,					// #470
NULL,					// #471
NULL,					// #472
NULL,					// #473
NULL,					// #474
NULL,					// #475
NULL,					// #476
NULL,					// #477
NULL,					// #478
NULL,					// #479
e10, e10					// #480-499 (LordHavoc)
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

	// used by PushMove to move back pushed entities
	sv.moved_edicts = (edict_t **)PRVM_Alloc(prog->max_edicts * sizeof(edict_t *));

	prog->protect_world = false; // allow to change world parms

	ent = PRVM_EDICT_NUM( 0 );
	memset (ent->progs.sv, 0, prog->progs->entityfields * 4);
	ent->priv.sv->free = false;
	ent->progs.sv->model = PRVM_SetEngineString( sv.configstrings[CS_MODELS] );
	ent->progs.sv->modelindex = 1; // world model
	ent->progs.sv->solid = SOLID_BSP;
	ent->progs.sv->movetype = MOVETYPE_PUSH;

	SV_ConfigString (CS_MAXCLIENTS, va("%i", maxclients->integer ));
	prog->globals.sv->mapname = PRVM_SetEngineString( sv.name );

	// spawn the rest of the entities on the map
	*prog->time = sv.time;

	// set client fields on player ents
	for (i = 1; i < maxclients->value; i++)
	{
		// setup all clients
		ent = PRVM_EDICT_NUM( i );
		ent->priv.sv->client = svs.gclients + i - 1;
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
	Msg("\n");
	PRVM_Begin;
	PRVM_InitProg( PRVM_SERVERPROG );

	prog->reserved_edicts = maxclients->integer;
	prog->loadintoworld = true;
		
	if( !prog->loaded )
	{        
		prog->progs_mempool = Mem_AllocPool("Server Progs" );
		prog->name = "server";
		prog->builtins = vm_sv_builtins;
		prog->numbuiltins = vm_sv_numbuiltins;
		prog->max_edicts = MAX_EDICTS<<2;
		prog->limit_edicts = MAX_EDICTS;
		prog->edictprivate_size = sizeof(sv_edict_t);
		prog->begin_increase_edicts = SV_BeginIncreaseEdicts;
		prog->end_increase_edicts = SV_EndIncreaseEdicts;
		prog->init_edict = SV_InitEdict;
		prog->free_edict = SV_FreeEdict;
		prog->count_edicts = SV_CountEdicts;
		prog->load_edict = SV_LoadEdict;
		prog->extensionstring = "";

		// using default builtins
		prog->init_cmd = VM_Cmd_Init;
		prog->reset_cmd = VM_Cmd_Reset;
		prog->error_cmd = VM_Error;
		prog->flag |= PRVM_OP_STATE; // enable op_state feature
		PRVM_LoadProgs( "server.dat", 0, NULL, SV_NUM_REQFIELDS, sv_reqfields );
	}
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

	SV_VM_Begin();

	for (i = 1; prog && i < prog->num_edicts; i++)
	{
		ent = PRVM_EDICT_NUM(i);
		SV_FreeEdict( ent );// release physic
	}
	SV_VM_End();

	if(!svs.gclients) return;
	Mem_Free( svs.gclients );
	svs.gclients = NULL;

}