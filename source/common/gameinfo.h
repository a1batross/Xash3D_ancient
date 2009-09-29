//=======================================================================
//			Copyright XashXT Group 2009 ©
//	      gameinfo.h - shared struct that contains the global gameinfo
//=======================================================================
#ifndef GAMEINFO_H
#define GAMEINFO_H

/*
========================================================================

GAMEINFO stuff

dll shared gameinfo structure
========================================================================
*/
typedef struct dll_gameinfo_s
{
	// filesystem info
	string_t	basedir;	 	// main game directory (like 'id1' for Quake or 'valve' for Half-Life)
	string_t	gamedir;	 	// gamedir (can be match with basedir, used as primary dir and as write path
	string_t	startmap;	 	// map to start singleplayer game
	string_t	title;	 	// Game Main Title

	float	version;	 	// game version (optional)
	int	viewmode;		// 0 - third and first person, 1 - firstperson only, 2 - thirdperson only

	string_t	sp_entity; 	// e.g. info_player_start
	string_t	dm_entity; 	// e.g. info_player_deathmatch
	string_t	coop_entity;	// e.g. info_player_coop
	string_t	ctf_entity;	// e.g. info_player_ctf
	string_t	team_entity;	// e.g. info_player_team

	vec3_t	client_mins[2];	// 0 - normal, 1 - ducked
	vec3_t	client_maxs[2];	// 0 - normal, 1 - ducked
} dll_gameinfo_t;

#endif//GAMEINFO_H