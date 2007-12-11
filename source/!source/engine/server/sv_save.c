//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_save.c - server save files
//=======================================================================

#include "engine.h"
#include "server.h"

byte	*sav_base;

void SV_AddSaveLump( dsavehdr_t *hdr, file_t *f, int lumpnum, void *data, int len )
{
	lump_t	*lump;

	lump = &hdr->lumps[lumpnum];
	lump->fileofs = LittleLong( FS_Tell(f));
	lump->filelen = LittleLong(len);
	FS_Write(f, data, (len + 3) & ~3 );
}

static void Cvar_AddToBuffer(const char *name, const char *string, const char *buffer, int *bufsize)
{
	if (strlen(name) >= 64 || strlen(string) >= 64)
	{
		MsgDev(D_NOTE, "cvar too long: %s = %s\n", name, string);
		return;
	}
	*bufsize = com.strpack((char *)buffer, *bufsize, (char *)name, strlen(name)); 
	*bufsize = com.strpack((char *)buffer, *bufsize, (char *)string, strlen(string));
}

void SV_AddCvarLump( dsavehdr_t *hdr, file_t *f )
{
	int	bufsize = 1; // null terminator 
	char	*cvbuffer = Z_Malloc( MAX_INPUTLINE );

	Cvar_LookupVars( CVAR_LATCH, cvbuffer, &bufsize, Cvar_AddToBuffer );
	SV_AddSaveLump( hdr, f, LUMP_GAMECVARS, cvbuffer, bufsize );

	Z_Free( cvbuffer ); // free buffer
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
	Z_Free( csbuffer ); // free memory
}

void SV_WriteGlobal( dsavehdr_t *hdr, file_t *f )
{
	vfile_t	*h = VFS_Open( f, "wz" );
	lump_t	*lump;
	int	len, pos;

	lump = &hdr->lumps[LUMP_GAMESTATE];
	lump->fileofs = LittleLong( FS_Tell(f));

	SV_VM_Begin();
	PRVM_ED_WriteGlobals( h );
	SV_VM_End();
	pos = VFS_Tell(h);

	FS_Write(f, &pos, sizeof(int)); // first four bytes is real filelength
	f = VFS_Close(h);
	len = LittleLong(FS_Tell(f));
	lump->filelen = LittleLong( len - lump->fileofs );
}

void SV_WriteLocals( dsavehdr_t *hdr, file_t *f )
{
	vfile_t	*h = VFS_Open( f, "wz" );
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
	pos = VFS_Tell(h);
	
	FS_Write(f, &pos, sizeof(int)); // first four bytes is real filelength
	f = VFS_Close(h);
	len = LittleLong(FS_Tell(f));
	lump->filelen = LittleLong( len - lump->fileofs );
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

	if(sv.state != ss_game) return;
	if(Cvar_VariableValue("deathmatch"))
	{
		MsgDev(D_ERROR, "SV_WriteSaveFile: can't savegame in a deathmatch\n");
		return;
	}
	if(maxclients->value == 1 && svs.clients[0].edict->priv.sv->client->ps.stats[STAT_HEALTH] <= 0)
	{
		MsgDev(D_ERROR, "SV_WriteSaveFile: can't savegame while dead!\n");
		return;
	}
	
	if(!com.strcmp(name, "save0.bin")) autosave = true;
	sprintf (path, "save/%s", name );
	savfile = FS_Open( path, "wb");

	if (!savfile)
	{
		MsgDev(D_ERROR, "SV_WriteSaveFile: failed to open %s\n", path );
		return;
	}

	MsgDev (D_INFO, "Saving game..." );
	sprintf (comment, "%s - %s", sv.configstrings[CS_NAME], timestamp(TIME_FULL));

	header = (dsavehdr_t *)Z_Malloc( sizeof(dsavehdr_t));
	header->ident = LittleLong (IDSAVEHEADER);
	header->version = LittleLong (SAVE_VERSION);
	FS_Write( savfile, header, sizeof(dsavehdr_t));
          
	// write lumps
	SV_AddSaveLump( header, savfile, LUMP_COMMENTS, comment, sizeof(comment));
          SV_AddCStrLump( header, savfile );
	SV_AddSaveLump( header, savfile, LUMP_AREASTATE, portalopen, sizeof(portalopen));
	SV_WriteGlobal( header, savfile );
	SV_AddSaveLump( header, savfile, LUMP_MAPNAME, svs.mapcmd, sizeof(svs.mapcmd));
	SV_AddCvarLump( header, savfile );
	SV_WriteLocals( header, savfile );
	
	// merge header
	FS_Seek( savfile, 0, SEEK_SET );
	FS_Write( savfile, header, sizeof(dsavehdr_t));
	FS_Close( savfile );
	Z_Free( header );
	MsgDev(D_INFO, "done.\n");
}

void Sav_LoadComment( lump_t *l )
{
	byte	*in;
	int	size;

	in = (void *)(sav_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("Sav_LoadComment: funny lump size\n" );

	size = l->filelen / sizeof(*in);
	strncpy(svs.comment, in, size );
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
		Cvar_SetLatched(name, string);
	}
}

void Sav_LoadMapCmds( lump_t *l )
{
	byte	*in;
	int	size;

	in = (void *)(sav_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("Sav_LoadMapCmds: funny lump size\n" );

	size = l->filelen / sizeof(*in);
	strncpy (svs.mapcmd, in, size );
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
	// CM_ReadPortalState
	byte	*in;
	int	size;

	in = (void *)(sav_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("Sav_LoadAreaPortals: funny lump size\n" );

	size = l->filelen / sizeof(*in);
	Mem_Copy(portalopen, in, size);
	CM_FloodAreaConnections ();
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
			if (entnum >= MAX_EDICTS)
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
				//SV_SetModel( ent, PRVM_GetString( ent->progs.sv->model ));
				SV_LinkEdict( ent );
				if(ent->progs.sv->movetype == MOVETYPE_PHYSIC)
				{
					pe->CreateBody( ent->priv.sv, SV_GetModelPtr(ent), ent->progs.sv->origin, ent->progs.sv->angles );
				}
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

	sprintf(path, "save/%s", name );
	savfile = FS_LoadFile(path, &size );

	header = (dsavehdr_t *)savfile;
	i = LittleLong (header->version);
	id = LittleLong (header->ident);

	if(id != IDSAVEHEADER) Host_Error("SV_ReadSaveFile: file %s is corrupted\n", path );
	if (i != SAVE_VERSION) Host_Error("file %s from an older save version\n", path );

	sav_base = (byte *)header;

	Sav_LoadComment(&header->lumps[LUMP_COMMENTS]);
	Sav_LoadCvars(&header->lumps[LUMP_GAMECVARS]);
	
	SV_InitGame(); // start a new game fresh with new cvars
	Sav_LoadMapCmds(&header->lumps[LUMP_MAPNAME]);
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

	sprintf (path, "save/%s", name );
	savfile = FS_LoadFile(path, &size );

	if(!savfile)
	{
		Msg("can't open %s\n", path );
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
}

bool Menu_ReadComment( char *comment, int savenum )
{
	dsavehdr_t	*header;
	byte		*savfile;
	int		i, id, size;

	if(!comment) return false;
	savfile = FS_LoadFile(va("save/save%i.bin", savenum), &size );

	if(!savfile) 
	{
		strncpy( comment, "<empty>", MAX_QPATH );
		return false;
	}

	header = (dsavehdr_t *)savfile;
	i = LittleLong (header->version);
	id = LittleLong (header->ident);

	if(id != IDSAVEHEADER || i != SAVE_VERSION)
	{
		strncpy( comment, "<corrupted>", MAX_QPATH );
		return false;
	}

	sav_base = (byte *)header;
	Sav_LoadComment(&header->lumps[LUMP_COMMENTS]);
	com.strncpy( comment, svs.comment, MAX_QPATH );
	Mem_Free( savfile );

	return true;
}