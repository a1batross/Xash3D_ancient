//=======================================================================
//			Copyright XashXT Group 2007 ©
//			savefile.h - game save file
//=======================================================================
#ifndef SAVEFILE_H
#define SAVEFILE_H

/*
==============================================================================

SAVE FILE
==============================================================================
*/
//save mode
#define REGULAR	0
#define AUTOSAVE	1
#define QUICK	2

//header
#define SAVE_VERSION	1
#define IDSAVEHEADER	(('E'<<24)+('V'<<16)+('A'<<8)+'S') // little-endian "SAVE"

#define LUMP_SNAPSHOT	0	// jpg image snapshot
#define LUMP_COMMENTS	1	// map comments
#define LUMP_MAPCMDS	2	// map commands
#define LUMP_GAMECVARS	3	// contain game comment and all cvar state
#define LUMP_GAMELOCAL	4	// server.dll game_locals_t struct and players info
#define LUMP_GAMETRANS	5	// level transition info
#define LUMP_CFGSTRING	6	// client info strings
#define LUMP_AREASTATE	7	// area portals state
#define LUMP_ENTSSTATE	8	// server.dll state of all entities
#define LUMP_HEADER		9	// header 

typedef struct
{
	int	ident;
	int	version;	
	lump_t	lumps[LUMP_HEADER];
} dsavehdr_t;

typedef struct
{
	char	name[64];
	char	value[64];
} dsavecvar_t;

#endif//SAVEFILE_H