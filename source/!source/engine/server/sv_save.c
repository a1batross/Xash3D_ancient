//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_save.c - server save files
//=======================================================================


#include "engine.h"
#include "server.h"
#include "savefile.h"

byte	*sav_base;

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

size_t COM_PackString( byte *buffer, int pos, char *string )
{
	int strsize;

	if(!buffer || !string) return 0;

	strsize = strlen( string );
	if(strsize > MAX_QPATH) strsize = MAX_QPATH; //critical stuff
	strsize++; //get space for terminator	

	strlcpy(buffer + pos, string, strsize ); 
	return pos + strsize;
}

size_t COM_UnpackString( byte *buffer, int pos, char *string )
{
	int	strsize = 0;
	char	*in;

	if(!buffer || !string) return 0;
	in = buffer + pos;

	do { in++, strsize++; } while(in && *in != '\0');

	strlcpy( string, in - (strsize - 1), strsize ); 
	return pos + strsize;
}

void SV_AddCvarLump( dsavehdr_t *hdr, file_t *f )
{
	cvar_t	*var;
	int	bufsize = 1; // null terminator 
	int	numsavedcvars = 0;
	char	*cvbuffer;
	
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

	cvbuffer = (char *)Z_Malloc( numsavedcvars * MAX_QPATH );

	//second pass
	for(var = cvar_vars; var; var = var->next)
	{
		if(!(var->flags & CVAR_LATCH)) continue;
		if (strlen(var->name) >= 64 || strlen(var->string) >= 64)
		{
			Msg ("Cvar too long: %s = %s\n", var->name, var->string);
			continue;
		}

		bufsize = COM_PackString(cvbuffer, bufsize, var->name ); 
		bufsize = COM_PackString(cvbuffer, bufsize, var->string );
	}

	SV_AddSaveLump( hdr, f, LUMP_GAMECVARS, cvbuffer, bufsize );
	Z_Free( cvbuffer );
}

void SV_AddCStrLump( dsavehdr_t *hdr, file_t *f )
{
	int	i, bufsize = 1; //null terminator
	char	*csbuffer = Z_Malloc( MAX_CONFIGSTRINGS * MAX_QPATH );

	//pack the cfg string data
	for(i = 0; i < MAX_CONFIGSTRINGS; i++)
	{
		bufsize = COM_PackString(csbuffer, bufsize, sv.configstrings[i] ); 
	}	
	SV_AddSaveLump( hdr, f, LUMP_CFGSTRING, csbuffer, bufsize );
	Z_Free( csbuffer ); // free memory
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
	
	if(!strcmp(name, "save0")) autosave = true;
	sprintf (path, "save/%s.bin", name );

	savfile = FS_Open( path, "wb");
	if (!savfile)
	{
		MsgWarn("SV_WriteSaveFile: failed to open %s\n", path );
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
          SV_AddCStrLump( header, savfile );
	SV_AddSaveLump( header, savfile, LUMP_AREASTATE, portalopen, sizeof(portalopen));
//ge->WriteLump ( header, savfile, LUMP_GAMELEVEL, autosave );
	SV_AddSaveLump( header, savfile, LUMP_MAPCMDS, svs.mapcmd, sizeof(svs.mapcmd));
	SV_AddCvarLump( header, savfile );
//ge->WriteLump ( header, savfile, LUMP_GAMELOCAL, autosave );
	
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
	int	size, pos = 0;
	byte	*in;

	in = (void *)(sav_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Com_Error (ERR_DROP, "Sav_LoadCvars: funny lump size\n" );
	size = l->filelen / sizeof(*in);

	while(pos < size)
	{
		pos = COM_UnpackString( in, pos, name );  
		pos = COM_UnpackString( in, pos, string );  
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
	int	i, pos = 0;

	in = (void *)(sav_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Com_Error (ERR_DROP, "Sav_LoadCfgString: funny lump size\n" );

	//unpack the cfg string data
	for(i = 0; i < MAX_CONFIGSTRINGS; i++)
	{
		pos = COM_UnpackString( in, pos, sv.configstrings[i] );  
	}
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
//ge->ReadLump( sav_base, &header->lumps[LUMP_GAMELOCAL], LUMP_GAMELOCAL );
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
//ge->ReadLump( sav_base, &header->lumps[LUMP_GAMELEVEL], LUMP_GAMELEVEL );
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