//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_save.c - server save file
//=======================================================================


#include "engine.h"
#include "server.h"
#include "savefile.h"

dsavehdr_t	*header;

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
	strftime (timestring, sizeof (timestring), "%b%Y-%d(%H.%M.%S)", crt_tm);
          strcpy( timestamp, timestring );

	return timestamp;
}

void SV_AddSaveLump( file_t *f, int lumpnum, void *data, int len )
{
	lump_t	*lump;

	lump = &header->lumps[lumpnum];
	lump->fileofs = LittleLong( FS_Tell(f));
	lump->filelen = LittleLong(len);
	FS_Write(f, data, (len + 3) & ~3 );
}

void SV_AddCvarLump( file_t *f )
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

	SV_AddSaveLump( f, LUMP_GAMECVARS, dout, numsavedcvars * sizeof(dsavecvar_t));
	Z_Free( dout );
}

/*
=============
SV_WriteSaveFile
=============
*/

void SV_WriteSaveFile( int mode )
{
	char path[MAX_SYSPATH];
	char	comment[32];

	file_t	*savfile;
	
	switch(mode)
	{
	case QUICK: 
		sprintf (path, "save/quick.bin" );
		sprintf (comment, "Quick save %s", sv.configstrings[CS_NAME]);
		break;
	case AUTOSAVE:
		sprintf (path, "save/%s/auto.bin", sv.name );
		sprintf (comment, "Autosave %s", sv.configstrings[CS_NAME]);
		break;
	case REGULAR:
		sprintf (path, "save/%s/%s.bin", sv.name, SV_CurTime());
		strlcpy (comment, SV_CurTime(), sizeof(comment));
		strlcat (comment, sv.configstrings[CS_NAME], sizeof(comment) - 1 - strlen(comment));
		break;
	}
          
	savfile = FS_Open( path, "wb");
	if (!savfile)
	{
		MsgDev ("Failed to open %s\n", path );
		return;
	}
	MsgDev("SV_WriteLevelFile()\n");

	header = (dsavehdr_t *)Z_Malloc( sizeof(dsavehdr_t));

	header->ident = LittleLong (IDSAVEHEADER);
	header->version = LittleLong (SAVE_VERSION);
	FS_Write( savfile, header, sizeof(dsavehdr_t));

	//write lumps
	SV_AddSaveLump( savfile, LUMP_COMMENTS, comment, sizeof(comment));
	SV_AddSaveLump( savfile, LUMP_MAPCMDS, svs.mapcmd, sizeof(svs.mapcmd));
	SV_AddSaveLump( savfile, LUMP_CFGSTRING, sv.configstrings, sizeof(sv.configstrings));
	SV_AddCvarLump( savfile );
	SV_AddSaveLump( savfile, LUMP_AREASTATE, portalopen, sizeof(portalopen));	
	//ge->WriteLump (f, LUMP_GAMELOCAL);
	//ge->WriteLump (f, LUMP_ENTSSTATE);
	//ge->WriteLump (f, LUMP_GAMETRANS);
	
	//merge header
	FS_Seek( savfile, 0, SEEK_SET );
	FS_Write( savfile, header, sizeof(dsavehdr_t));
	FS_Close( savfile );

	Z_Free( header );
}