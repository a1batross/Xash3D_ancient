//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_save.c - server save file
//=======================================================================


#include "engine.h"
#include "server.h"
#include "savefile.h"

/*
====================
CurTime()

returned a time stamp
====================
*/
const char* SV_CurTime( void )
{
	static char timestamp [128];
	time_t crt_time;
	const struct tm *crt_tm;
	char timestring [64];

	// Build the time stamp (ex: "Apr2007-03(23.31.55)");
	time (&crt_time);
	crt_tm = localtime (&crt_time);
	strftime (timestring, sizeof (timestring), "%b%d-%Y (%H:%M)", crt_tm);
          strcpy( timestamp, timestring );

	return timestamp;
}

void SV_AddSaveLump( dsavehdr_t *hdr, file_t *f, int lumpnum, void *data, int len )
{
	lump_t	*lump;

	lump = &hdr->lumps[lumpnum];
	lump->fileofs = LittleLong( FS_Tell(f));
	lump->filelen = LittleLong(len);
	FS_Write(f, data, (len + 3) & ~3 );
}

void SV_AddCvarLump( dsavehdr_t *hdr, file_t *f )
{
	cvar_t	*var;
	int	numsavedcvars = 0;
	dsavecvar_t *dcvars, *dout;
	
	for(var = cvar_vars; var; var = var->next)
	{
		if(!(var->flags & CVAR_LATCH)) continue;
		if (strlen(var->name) >= 64 || strlen(var->string) >= 64)
		{
			Msg ("Cvar too long: %s = %s\n", var->name, var->string);
			continue;
		}
		numsavedcvars++;
	}

	dout = dcvars = (dsavecvar_t *)Z_Malloc( numsavedcvars * sizeof(dsavecvar_t));

	//second pass
	for(var = cvar_vars; var; var = var->next)
	{
		if(!(var->flags & CVAR_LATCH)) continue;
		if (strlen(var->name) >= 64 || strlen(var->string) >= 64)
		{
			Msg ("Cvar too long: %s = %s\n", var->name, var->string);
			continue;
		}

		strlcpy(dcvars->name, var->name, sizeof(dcvars->name));
		strlcpy(dcvars->value, var->string, sizeof(dcvars->value));
		dcvars++;
	}

	Msg("numcvars %d\n", numsavedcvars );

	SV_AddSaveLump( hdr, f, LUMP_GAMECVARS, dout, numsavedcvars * sizeof(dsavecvar_t));
	Z_Free( dout );
}

byte	*sav_base;

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
	
	sprintf (path, "save/%s.bin", name );

	savfile = FS_Open( path, "wb");
	if (!savfile)
	{
		MsgDev ("Failed to open %s\n", path );
		return;
	}

	MsgDev ("Saving game... %s\n", name );
	sprintf (comment, "%s - %s", sv.configstrings[CS_NAME], SV_CurTime());

	header = (dsavehdr_t *)Z_Malloc( sizeof(dsavehdr_t));
	header->ident = LittleLong (IDSAVEHEADER);
	header->version = LittleLong (SAVE_VERSION);
	FS_Write( savfile, header, sizeof(dsavehdr_t));
          
	//write lumps
	SV_AddSaveLump( header, savfile, LUMP_COMMENTS, comment, sizeof(comment));
	SV_AddSaveLump( header, savfile, LUMP_CFGSTRING, sv.configstrings, sizeof(sv.configstrings));
	SV_AddSaveLump( header, savfile, LUMP_AREASTATE, portalopen, sizeof(portalopen));
	ge->WriteLump ( header, savfile, LUMP_GAMELEVEL);
	SV_AddSaveLump( header, savfile, LUMP_MAPCMDS, svs.mapcmd, sizeof(svs.mapcmd));
	SV_AddCvarLump( header, savfile );
	ge->WriteLump ( header, savfile, LUMP_GAMELOCAL );
	
	//merge header
	FS_Seek( savfile, 0, SEEK_SET );
	FS_Write( savfile, header, sizeof(dsavehdr_t));
	FS_Close( savfile );
	Z_Free( header );
}

void Sav_LoadComment( lump_t *l )
{
	byte	*in;
	int	size;

	in = (void *)(sav_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Com_Error (ERR_DROP, "Sav_LoadComment: funny lump size\n" );

	size = l->filelen / sizeof(*in);
	strlcpy(svs.comment, in, size );
}

void Sav_LoadCvars( lump_t *l )
{
	char	name[64], string[64];
	int	size, curpos = 0;
	byte	*in;

	in = (void *)(sav_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Com_Error (ERR_DROP, "Sav_LoadCvars: funny lump size\n" );
	size = l->filelen / sizeof(*in);

	while(curpos < size)
	{
		strlcpy( name, in + curpos, MAX_OSPATH ); curpos += 64;
		strlcpy( string, in + curpos, 128 ); curpos += 64;
		Cvar_ForceSet (name, string);
	}
}

void Sav_LoadMapCmds( lump_t *l )
{
	byte	*in;
	int	size;

	in = (void *)(sav_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Com_Error (ERR_DROP, "Sav_LoadMapCmds: funny lump size\n" );

	size = l->filelen / sizeof(*in);
	strncpy (svs.mapcmd, in, size );
}

void Sav_LoadCfgString( lump_t *l )
{
	char	*in;
	int	size;

	in = (void *)(sav_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Com_Error (ERR_DROP, "Sav_LoadCfgString: funny lump size\n" );

	size = l->filelen / sizeof(*in);
	strlcpy((char *)sv.configstrings, in, size );
}

void Sav_LoadAreaPortals( lump_t *l )
{
	//CM_ReadPortalState
	byte	*in;
	int	size;

	in = (void *)(sav_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Com_Error (ERR_DROP, "Sav_LoadAreaPortals: funny lump size\n" );

	size = l->filelen / sizeof(*in);
	Mem_Copy(portalopen, in, size);
	CM_FloodAreaConnections ();
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

	sprintf (path, "save/%s.bin", name );
	savfile = FS_LoadFile(path, &size );

	if(!savfile)
	{
		Msg("can't open %s\n", path );
		return;
	}

	header = (dsavehdr_t *)savfile;
	i = LittleLong (header->version);
	id = LittleLong (header->ident);

	if(id != IDSAVEHEADER) Com_Error(ERR_DROP, "SV_ReadSaveFile: file %s is corrupted\n", path );
	if (i != SAVE_VERSION) Com_Error(ERR_DROP, "file %s from an older save version\n", path );

	sav_base = (byte *)header;

	Sav_LoadComment(&header->lumps[LUMP_COMMENTS]);
	Sav_LoadCvars(&header->lumps[LUMP_GAMECVARS]);
	
	SV_InitGame (); // start a new game fresh with new cvars
	Sav_LoadMapCmds(&header->lumps[LUMP_MAPCMDS]);
	ge->Sav_LoadGame( sav_base, &header->lumps[LUMP_GAMELOCAL] );
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

	sprintf (path, "save/%s.bin", name );
	savfile = FS_LoadFile(path, &size );

	if(!savfile)
	{
		Msg("can't open %s\n", path );
		return;
	}

	header = (dsavehdr_t *)savfile;
	i = LittleLong (header->version);
	id = LittleLong (header->ident);

	if(id != IDSAVEHEADER) Com_Error(ERR_DROP, "SV_ReadSaveFile: file %s is corrupted\n", path );
	if (i != SAVE_VERSION) Com_Error(ERR_DROP, "file %s from an older save version\n", path );

	sav_base = (byte *)header;

	Sav_LoadCfgString(&header->lumps[LUMP_CFGSTRING]);
	Sav_LoadAreaPortals(&header->lumps[LUMP_AREASTATE]);
	ge->Sav_LoadLevel( sav_base, &header->lumps[LUMP_GAMELEVEL] );
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
		strlcpy( comment, "<EMPTY>", 32 );
		return false;
	}

	header = (dsavehdr_t *)savfile;
	i = LittleLong (header->version);
	id = LittleLong (header->ident);

	if(id != IDSAVEHEADER || i != SAVE_VERSION)
	{
		strlcpy( comment, "<CORRUPTED>", 32 );
		return false;
	}

	sav_base = (byte *)header;
	Sav_LoadComment(&header->lumps[LUMP_COMMENTS]);
	strlcpy( comment, svs.comment, 32 );

	return true;
}