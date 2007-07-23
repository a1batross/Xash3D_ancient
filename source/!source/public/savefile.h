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
#define SAVE_VERSION	2
#define IDSAVEHEADER	(('E'<<24)+('V'<<16)+('A'<<8)+'S') // little-endian "SAVE"

#define LUMP_COMMENTS	0	// map comments
#define LUMP_CFGSTRING	1	// client info strings
#define LUMP_AREASTATE	2	// area portals state
#define LUMP_GAMELEVEL	3	// server.dll game_locals_t struct and players info
#define LUMP_MAPCMDS	4	// map commands
#define LUMP_GAMECVARS	5	// contain game comment and all cvar state
#define LUMP_GAMELOCAL	6	// server.dll game_locals_t struct and players info
#define LUMP_SNAPSHOT	7	// dxt image snapshot
#define LUMP_HEADER		8	// header 

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