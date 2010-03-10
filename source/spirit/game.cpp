/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#include "extdll.h"
#include "util.h"
#include "game.h"

// special macros for handle skill data
#define CVAR_REGISTER_SKILL( x )	(*g_engfuncs.pfnCVarRegister)( #x, "0", 0, "skill config cvar" )

// Engine Cvars
cvar_t 	*g_psv_gravity = NULL;
cvar_t	*g_psv_aim = NULL;
cvar_t	*g_psv_maxspeed = NULL;
cvar_t	*g_footsteps = NULL;

cvar_t	*displaysoundlist;

// multiplayer server rules
cvar_t	*teamplay;
cvar_t	*fraglimit;
cvar_t	*timelimit;
cvar_t	*friendlyfire;
cvar_t	*falldamage;
cvar_t	*weaponstay;
cvar_t	*forcerespawn;
cvar_t	*flashlight;
cvar_t	*aimcrosshair;
cvar_t	*decalfrequency;
cvar_t	*teamlist;
cvar_t	*timeleft;
cvar_t	*teamoverride;
cvar_t	*defaultteam;
cvar_t	*allowmonsters;
cvar_t	*mp_chattime;

// Register your console variables here
// This gets called one time when the game is initialied
void GameDLLInit( void )
{
	// Register cvars here:
	ALERT( at_aiconsole, "GameDLLInit();\n" );

	g_psv_maxspeed = CVAR_REGISTER( "sv_maxspeed", "320", 0, "maximum speed a player can accelerate to when on ground" );
	g_psv_gravity = CVAR_REGISTER( "sv_gravity", "800", 0, "world gravity" );
	g_psv_aim = CVAR_REGISTER( "sv_aim", "1", 0, "enable auto-aiming" );
	g_footsteps = CVAR_REGISTER( "mp_footsteps", "0", FCVAR_SERVERINFO, "can hear footsteps from other players" );

	displaysoundlist = CVAR_REGISTER( "displaysoundlist", "0", 0, "show monster sounds that actually playing" );

	teamplay = CVAR_REGISTER( "mp_teamplay", "0", FCVAR_SERVERINFO, "sets to 1 to indicate teamplay" );
	fraglimit = CVAR_REGISTER( "mp_fraglimit", "0", FCVAR_SERVERINFO, "limit of frags for current server" );
	timelimit = CVAR_REGISTER( "mp_timelimit", "0", FCVAR_SERVERINFO, "server timelimit" );
	CVAR_REGISTER( "mp_footsteps", "0", FCVAR_SERVERINFO, "can hear footsteps from other players" );

	CVAR_REGISTER( "mp_fragsleft", "0", FCVAR_SERVERINFO, "counter that indicated how many frags remaining" );
	timeleft = CVAR_REGISTER( "mp_timeleft", "0" , FCVAR_SERVERINFO, "counter that indicated how many time remaining" );

	friendlyfire = CVAR_REGISTER( "mp_friendlyfire", "0", FCVAR_SERVERINFO, "enables firedlyfire for teamplay" );
	falldamage = CVAR_REGISTER( "mp_falldamage", "0", FCVAR_SERVERINFO, "falldamage multiplier" );
	weaponstay = CVAR_REGISTER( "mp_weaponstay", "0", FCVAR_SERVERINFO, "weapon leave stays on ground" );
	forcerespawn = CVAR_REGISTER( "mp_forcerespawn", "1", FCVAR_SERVERINFO, "force client respawn after his death" );
	flashlight = CVAR_REGISTER( "mp_flashlight", "0", FCVAR_SERVERINFO, "attempt to use flashlight in multiplayer" );
	aimcrosshair = CVAR_REGISTER( "mp_autocrosshair", "1", FCVAR_SERVERINFO, "enables auto-aim in multiplayer" );
	decalfrequency = CVAR_REGISTER( "decalfrequency", "30", FCVAR_SERVERINFO, "how many decals can be spawned" );
	teamlist = CVAR_REGISTER( "mp_teamlist", "hgrunt,scientist", FCVAR_SERVERINFO, "names of default teams" );
	teamoverride = CVAR_REGISTER( "mp_teamoverride", "1", 0, "can ovveride teams from map settings ?" );
	defaultteam = CVAR_REGISTER( "mp_defaultteam", "0", 0, "use default team instead ?" );
	allowmonsters = CVAR_REGISTER( "mp_allowmonsters", "0", FCVAR_SERVERINFO, "allow monsters in multiplayer" );
	CVAR_REGISTER( "sohl_impulsetarget", "0", FCVAR_SERVERINFO, "trigger ents manually" ); //LRC
	CVAR_REGISTER( "sohl_mwdebug", "0", FCVAR_SERVERINFO, "debug info. for MoveWith" ); //LRC
	mp_chattime = CVAR_REGISTER( "mp_chattime", "10", FCVAR_SERVERINFO, "time beetween messages" );

// REGISTER CVARS FOR SKILL LEVEL STUFF
	// Agrunt
	CVAR_REGISTER_SKILL ( sk_agrunt_health1 );// {"sk_agrunt_health1","0"};
	CVAR_REGISTER_SKILL ( sk_agrunt_health2 );// {"sk_agrunt_health2","0"};
	CVAR_REGISTER_SKILL ( sk_agrunt_health3 );// {"sk_agrunt_health3","0"};

	CVAR_REGISTER_SKILL ( sk_agrunt_dmg_punch1 );// {"sk_agrunt_dmg_punch1","0"};
	CVAR_REGISTER_SKILL ( sk_agrunt_dmg_punch2 );// {"sk_agrunt_dmg_punch2","0"};
	CVAR_REGISTER_SKILL ( sk_agrunt_dmg_punch3 );// {"sk_agrunt_dmg_punch3","0"};

	// Apache
	CVAR_REGISTER_SKILL ( sk_apache_health1 );// {"sk_apache_health1","0"};
	CVAR_REGISTER_SKILL ( sk_apache_health2 );// {"sk_apache_health2","0"};
	CVAR_REGISTER_SKILL ( sk_apache_health3 );// {"sk_apache_health3","0"};

	// Barney
	CVAR_REGISTER_SKILL ( sk_barney_health1 );// {"sk_barney_health1","0"};
	CVAR_REGISTER_SKILL ( sk_barney_health2 );// {"sk_barney_health2","0"};
	CVAR_REGISTER_SKILL ( sk_barney_health3 );// {"sk_barney_health3","0"};

	// Bullsquid
	CVAR_REGISTER_SKILL ( sk_bullsquid_health1 );// {"sk_bullsquid_health1","0"};
	CVAR_REGISTER_SKILL ( sk_bullsquid_health2 );// {"sk_bullsquid_health2","0"};
	CVAR_REGISTER_SKILL ( sk_bullsquid_health3 );// {"sk_bullsquid_health3","0"};

	CVAR_REGISTER_SKILL ( sk_bullsquid_dmg_bite1 );// {"sk_bullsquid_dmg_bite1","0"};
	CVAR_REGISTER_SKILL ( sk_bullsquid_dmg_bite2 );// {"sk_bullsquid_dmg_bite2","0"};
	CVAR_REGISTER_SKILL ( sk_bullsquid_dmg_bite3 );// {"sk_bullsquid_dmg_bite3","0"};

	CVAR_REGISTER_SKILL ( sk_bullsquid_dmg_whip1 );// {"sk_bullsquid_dmg_whip1","0"};
	CVAR_REGISTER_SKILL ( sk_bullsquid_dmg_whip2 );// {"sk_bullsquid_dmg_whip2","0"};
	CVAR_REGISTER_SKILL ( sk_bullsquid_dmg_whip3 );// {"sk_bullsquid_dmg_whip3","0"};

	CVAR_REGISTER_SKILL ( sk_bullsquid_dmg_spit1 );// {"sk_bullsquid_dmg_spit1","0"};
	CVAR_REGISTER_SKILL ( sk_bullsquid_dmg_spit2 );// {"sk_bullsquid_dmg_spit2","0"};
	CVAR_REGISTER_SKILL ( sk_bullsquid_dmg_spit3 );// {"sk_bullsquid_dmg_spit3","0"};


	CVAR_REGISTER_SKILL ( sk_bigmomma_health_factor1 );// {"sk_bigmomma_health_factor1","1.0"};
	CVAR_REGISTER_SKILL ( sk_bigmomma_health_factor2 );// {"sk_bigmomma_health_factor2","1.0"};
	CVAR_REGISTER_SKILL ( sk_bigmomma_health_factor3 );// {"sk_bigmomma_health_factor3","1.0"};

	CVAR_REGISTER_SKILL ( sk_bigmomma_dmg_slash1 );// {"sk_bigmomma_dmg_slash1","50"};
	CVAR_REGISTER_SKILL ( sk_bigmomma_dmg_slash2 );// {"sk_bigmomma_dmg_slash2","50"};
	CVAR_REGISTER_SKILL ( sk_bigmomma_dmg_slash3 );// {"sk_bigmomma_dmg_slash3","50"};

	CVAR_REGISTER_SKILL ( sk_bigmomma_dmg_blast1 );// {"sk_bigmomma_dmg_blast1","100"};
	CVAR_REGISTER_SKILL ( sk_bigmomma_dmg_blast2 );// {"sk_bigmomma_dmg_blast2","100"};
	CVAR_REGISTER_SKILL ( sk_bigmomma_dmg_blast3 );// {"sk_bigmomma_dmg_blast3","100"};

	CVAR_REGISTER_SKILL ( sk_bigmomma_radius_blast1 );// {"sk_bigmomma_radius_blast1","250"};
	CVAR_REGISTER_SKILL ( sk_bigmomma_radius_blast2 );// {"sk_bigmomma_radius_blast2","250"};
	CVAR_REGISTER_SKILL ( sk_bigmomma_radius_blast3 );// {"sk_bigmomma_radius_blast3","250"};

	// Gargantua
	CVAR_REGISTER_SKILL ( sk_gargantua_health1 );// {"sk_gargantua_health1","0"};
	CVAR_REGISTER_SKILL ( sk_gargantua_health2 );// {"sk_gargantua_health2","0"};
	CVAR_REGISTER_SKILL ( sk_gargantua_health3 );// {"sk_gargantua_health3","0"};

	CVAR_REGISTER_SKILL ( sk_gargantua_dmg_slash1 );// {"sk_gargantua_dmg_slash1","0"};
	CVAR_REGISTER_SKILL ( sk_gargantua_dmg_slash2 );// {"sk_gargantua_dmg_slash2","0"};
	CVAR_REGISTER_SKILL ( sk_gargantua_dmg_slash3 );// {"sk_gargantua_dmg_slash3","0"};

	CVAR_REGISTER_SKILL ( sk_gargantua_dmg_fire1 );// {"sk_gargantua_dmg_fire1","0"};
	CVAR_REGISTER_SKILL ( sk_gargantua_dmg_fire2 );// {"sk_gargantua_dmg_fire2","0"};
	CVAR_REGISTER_SKILL ( sk_gargantua_dmg_fire3 );// {"sk_gargantua_dmg_fire3","0"};

	CVAR_REGISTER_SKILL ( sk_gargantua_dmg_stomp1 );// {"sk_gargantua_dmg_stomp1","0"};
	CVAR_REGISTER_SKILL ( sk_gargantua_dmg_stomp2 );// {"sk_gargantua_dmg_stomp2","0"};
	CVAR_REGISTER_SKILL ( sk_gargantua_dmg_stomp3	);// {"sk_gargantua_dmg_stomp3","0"};


	// Hassassin
	CVAR_REGISTER_SKILL ( sk_hassassin_health1 );// {"sk_hassassin_health1","0"};
	CVAR_REGISTER_SKILL ( sk_hassassin_health2 );// {"sk_hassassin_health2","0"};
	CVAR_REGISTER_SKILL ( sk_hassassin_health3 );// {"sk_hassassin_health3","0"};


	// Headcrab
	CVAR_REGISTER_SKILL ( sk_headcrab_health1 );// {"sk_headcrab_health1","0"};
	CVAR_REGISTER_SKILL ( sk_headcrab_health2 );// {"sk_headcrab_health2","0"};
	CVAR_REGISTER_SKILL ( sk_headcrab_health3 );// {"sk_headcrab_health3","0"};

	CVAR_REGISTER_SKILL ( sk_headcrab_dmg_bite1 );// {"sk_headcrab_dmg_bite1","0"};
	CVAR_REGISTER_SKILL ( sk_headcrab_dmg_bite2 );// {"sk_headcrab_dmg_bite2","0"};
	CVAR_REGISTER_SKILL ( sk_headcrab_dmg_bite3 );// {"sk_headcrab_dmg_bite3","0"};


	// Hgrunt 
	CVAR_REGISTER_SKILL ( sk_hgrunt_health1 );// {"sk_hgrunt_health1","0"};
	CVAR_REGISTER_SKILL ( sk_hgrunt_health2 );// {"sk_hgrunt_health2","0"};
	CVAR_REGISTER_SKILL ( sk_hgrunt_health3 );// {"sk_hgrunt_health3","0"};

	CVAR_REGISTER_SKILL ( sk_hgrunt_kick1 );// {"sk_hgrunt_kick1","0"};
	CVAR_REGISTER_SKILL ( sk_hgrunt_kick2 );// {"sk_hgrunt_kick2","0"};
	CVAR_REGISTER_SKILL ( sk_hgrunt_kick3 );// {"sk_hgrunt_kick3","0"};

	CVAR_REGISTER_SKILL ( sk_hgrunt_pellets1 );
	CVAR_REGISTER_SKILL ( sk_hgrunt_pellets2 );
	CVAR_REGISTER_SKILL ( sk_hgrunt_pellets3 );

	CVAR_REGISTER_SKILL ( sk_hgrunt_gspeed1 );
	CVAR_REGISTER_SKILL ( sk_hgrunt_gspeed2 );
	CVAR_REGISTER_SKILL ( sk_hgrunt_gspeed3 );

	// Houndeye
	CVAR_REGISTER_SKILL ( sk_houndeye_health1 );// {"sk_houndeye_health1","0"};
	CVAR_REGISTER_SKILL ( sk_houndeye_health2 );// {"sk_houndeye_health2","0"};
	CVAR_REGISTER_SKILL ( sk_houndeye_health3 );// {"sk_houndeye_health3","0"};

	CVAR_REGISTER_SKILL ( sk_houndeye_dmg_blast1 );// {"sk_houndeye_dmg_blast1","0"};
	CVAR_REGISTER_SKILL ( sk_houndeye_dmg_blast2 );// {"sk_houndeye_dmg_blast2","0"};
	CVAR_REGISTER_SKILL ( sk_houndeye_dmg_blast3 );// {"sk_houndeye_dmg_blast3","0"};


	// ISlave
	CVAR_REGISTER_SKILL ( sk_islave_health1 );// {"sk_islave_health1","0"};
	CVAR_REGISTER_SKILL ( sk_islave_health2 );// {"sk_islave_health2","0"};
	CVAR_REGISTER_SKILL ( sk_islave_health3 );// {"sk_islave_health3","0"};

	CVAR_REGISTER_SKILL ( sk_islave_dmg_claw1 );// {"sk_islave_dmg_claw1","0"};
	CVAR_REGISTER_SKILL ( sk_islave_dmg_claw2 );// {"sk_islave_dmg_claw2","0"};
	CVAR_REGISTER_SKILL ( sk_islave_dmg_claw3 );// {"sk_islave_dmg_claw3","0"};

	CVAR_REGISTER_SKILL ( sk_islave_dmg_clawrake1	);// {"sk_islave_dmg_clawrake1","0"};
	CVAR_REGISTER_SKILL ( sk_islave_dmg_clawrake2	);// {"sk_islave_dmg_clawrake2","0"};
	CVAR_REGISTER_SKILL ( sk_islave_dmg_clawrake3	);// {"sk_islave_dmg_clawrake3","0"};
		
	CVAR_REGISTER_SKILL ( sk_islave_dmg_zap1 );// {"sk_islave_dmg_zap1","0"};
	CVAR_REGISTER_SKILL ( sk_islave_dmg_zap2 );// {"sk_islave_dmg_zap2","0"};
	CVAR_REGISTER_SKILL ( sk_islave_dmg_zap3 );// {"sk_islave_dmg_zap3","0"};


	// Icthyosaur
	CVAR_REGISTER_SKILL ( sk_ichthyosaur_health1	);// {"sk_ichthyosaur_health1","0"};
	CVAR_REGISTER_SKILL ( sk_ichthyosaur_health2	);// {"sk_ichthyosaur_health2","0"};
	CVAR_REGISTER_SKILL ( sk_ichthyosaur_health3	);// {"sk_ichthyosaur_health3","0"};

	CVAR_REGISTER_SKILL ( sk_ichthyosaur_shake1	);// {"sk_ichthyosaur_health3","0"};
	CVAR_REGISTER_SKILL ( sk_ichthyosaur_shake2	);// {"sk_ichthyosaur_health3","0"};
	CVAR_REGISTER_SKILL ( sk_ichthyosaur_shake3	);// {"sk_ichthyosaur_health3","0"};



	// Leech
	CVAR_REGISTER_SKILL ( sk_leech_health1 );// {"sk_leech_health1","0"};
	CVAR_REGISTER_SKILL ( sk_leech_health2 );// {"sk_leech_health2","0"};
	CVAR_REGISTER_SKILL ( sk_leech_health3 );// {"sk_leech_health3","0"};

	CVAR_REGISTER_SKILL ( sk_leech_dmg_bite1 );// {"sk_leech_dmg_bite1","0"};
	CVAR_REGISTER_SKILL ( sk_leech_dmg_bite2 );// {"sk_leech_dmg_bite2","0"};
	CVAR_REGISTER_SKILL ( sk_leech_dmg_bite3 );// {"sk_leech_dmg_bite3","0"};


	// Controller
	CVAR_REGISTER_SKILL ( sk_controller_health1 );
	CVAR_REGISTER_SKILL ( sk_controller_health2 );
	CVAR_REGISTER_SKILL ( sk_controller_health3 );

	CVAR_REGISTER_SKILL ( sk_controller_dmgzap1 );
	CVAR_REGISTER_SKILL ( sk_controller_dmgzap2 );
	CVAR_REGISTER_SKILL ( sk_controller_dmgzap3 );

	CVAR_REGISTER_SKILL ( sk_controller_speedball1 );
	CVAR_REGISTER_SKILL ( sk_controller_speedball2 );
	CVAR_REGISTER_SKILL ( sk_controller_speedball3 );

	CVAR_REGISTER_SKILL ( sk_controller_dmgball1 );
	CVAR_REGISTER_SKILL ( sk_controller_dmgball2 );
	CVAR_REGISTER_SKILL ( sk_controller_dmgball3 );

	// Nihilanth
	CVAR_REGISTER_SKILL ( sk_nihilanth_health1 );// {"sk_nihilanth_health1","0"};
	CVAR_REGISTER_SKILL ( sk_nihilanth_health2 );// {"sk_nihilanth_health2","0"};
	CVAR_REGISTER_SKILL ( sk_nihilanth_health3 );// {"sk_nihilanth_health3","0"};

	CVAR_REGISTER_SKILL ( sk_nihilanth_zap1 );
	CVAR_REGISTER_SKILL ( sk_nihilanth_zap2 );
	CVAR_REGISTER_SKILL ( sk_nihilanth_zap3 );

	// Scientist
	CVAR_REGISTER_SKILL ( sk_scientist_health1 );// {"sk_scientist_health1","0"};
	CVAR_REGISTER_SKILL ( sk_scientist_health2 );// {"sk_scientist_health2","0"};
	CVAR_REGISTER_SKILL ( sk_scientist_health3 );// {"sk_scientist_health3","0"};


	// Snark
	CVAR_REGISTER_SKILL ( sk_snark_health1 );// {"sk_snark_health1","0"};
	CVAR_REGISTER_SKILL ( sk_snark_health2 );// {"sk_snark_health2","0"};
	CVAR_REGISTER_SKILL ( sk_snark_health3 );// {"sk_snark_health3","0"};

	CVAR_REGISTER_SKILL ( sk_snark_dmg_bite1 );// {"sk_snark_dmg_bite1","0"};
	CVAR_REGISTER_SKILL ( sk_snark_dmg_bite2 );// {"sk_snark_dmg_bite2","0"};
	CVAR_REGISTER_SKILL ( sk_snark_dmg_bite3 );// {"sk_snark_dmg_bite3","0"};

	CVAR_REGISTER_SKILL ( sk_snark_dmg_pop1 );// {"sk_snark_dmg_pop1","0"};
	CVAR_REGISTER_SKILL ( sk_snark_dmg_pop2 );// {"sk_snark_dmg_pop2","0"};
	CVAR_REGISTER_SKILL ( sk_snark_dmg_pop3 );// {"sk_snark_dmg_pop3","0"};



	// Zombie
	CVAR_REGISTER_SKILL ( sk_zombie_health1 );// {"sk_zombie_health1","0"};
	CVAR_REGISTER_SKILL ( sk_zombie_health2 );// {"sk_zombie_health3","0"};
	CVAR_REGISTER_SKILL ( sk_zombie_health3 );// {"sk_zombie_health3","0"};

	CVAR_REGISTER_SKILL ( sk_zombie_dmg_one_slash1 );// {"sk_zombie_dmg_one_slash1","0"};
	CVAR_REGISTER_SKILL ( sk_zombie_dmg_one_slash2 );// {"sk_zombie_dmg_one_slash2","0"};
	CVAR_REGISTER_SKILL ( sk_zombie_dmg_one_slash3 );// {"sk_zombie_dmg_one_slash3","0"};

	CVAR_REGISTER_SKILL ( sk_zombie_dmg_both_slash1 );// {"sk_zombie_dmg_both_slash1","0"};
	CVAR_REGISTER_SKILL ( sk_zombie_dmg_both_slash2 );// {"sk_zombie_dmg_both_slash2","0"};
	CVAR_REGISTER_SKILL ( sk_zombie_dmg_both_slash3 );// {"sk_zombie_dmg_both_slash3","0"};


	//Turret
	CVAR_REGISTER_SKILL ( sk_turret_health1 );// {"sk_turret_health1","0"};
	CVAR_REGISTER_SKILL ( sk_turret_health2 );// {"sk_turret_health2","0"};
	CVAR_REGISTER_SKILL ( sk_turret_health3 );// {"sk_turret_health3","0"};


	// MiniTurret
	CVAR_REGISTER_SKILL ( sk_miniturret_health1 );// {"sk_miniturret_health1","0"};
	CVAR_REGISTER_SKILL ( sk_miniturret_health2 );// {"sk_miniturret_health2","0"};
	CVAR_REGISTER_SKILL ( sk_miniturret_health3 );// {"sk_miniturret_health3","0"};


	// Sentry Turret
	CVAR_REGISTER_SKILL ( sk_sentry_health1 );// {"sk_sentry_health1","0"};
	CVAR_REGISTER_SKILL ( sk_sentry_health2 );// {"sk_sentry_health2","0"};
	CVAR_REGISTER_SKILL ( sk_sentry_health3 );// {"sk_sentry_health3","0"};


	// PLAYER WEAPONS

	// Crowbar whack
	CVAR_REGISTER_SKILL ( sk_plr_crowbar1 );// {"sk_plr_crowbar1","0"};
	CVAR_REGISTER_SKILL ( sk_plr_crowbar2 );// {"sk_plr_crowbar2","0"};
	CVAR_REGISTER_SKILL ( sk_plr_crowbar3 );// {"sk_plr_crowbar3","0"};

	// Glock Round
	CVAR_REGISTER_SKILL ( sk_plr_9mm_bullet1 );// {"sk_plr_9mm_bullet1","0"};
	CVAR_REGISTER_SKILL ( sk_plr_9mm_bullet2 );// {"sk_plr_9mm_bullet2","0"};
	CVAR_REGISTER_SKILL ( sk_plr_9mm_bullet3 );// {"sk_plr_9mm_bullet3","0"};

	// 357 Round
	CVAR_REGISTER_SKILL ( sk_plr_357_bullet1 );// {"sk_plr_357_bullet1","0"};
	CVAR_REGISTER_SKILL ( sk_plr_357_bullet2 );// {"sk_plr_357_bullet2","0"};
	CVAR_REGISTER_SKILL ( sk_plr_357_bullet3 );// {"sk_plr_357_bullet3","0"};

	// MP5 Round
	CVAR_REGISTER_SKILL ( sk_plr_9mmAR_bullet1 );// {"sk_plr_9mmAR_bullet1","0"};
	CVAR_REGISTER_SKILL ( sk_plr_9mmAR_bullet2 );// {"sk_plr_9mmAR_bullet2","0"};
	CVAR_REGISTER_SKILL ( sk_plr_9mmAR_bullet3 );// {"sk_plr_9mmAR_bullet3","0"};


	// M203 grenade
	CVAR_REGISTER_SKILL ( sk_plr_9mmAR_grenade1 );// {"sk_plr_9mmAR_grenade1","0"};
	CVAR_REGISTER_SKILL ( sk_plr_9mmAR_grenade2 );// {"sk_plr_9mmAR_grenade2","0"};
	CVAR_REGISTER_SKILL ( sk_plr_9mmAR_grenade3 );// {"sk_plr_9mmAR_grenade3","0"};


	// Shotgun buckshot
	CVAR_REGISTER_SKILL ( sk_plr_buckshot1 );// {"sk_plr_buckshot1","0"};
	CVAR_REGISTER_SKILL ( sk_plr_buckshot2 );// {"sk_plr_buckshot2","0"};
	CVAR_REGISTER_SKILL ( sk_plr_buckshot3 );// {"sk_plr_buckshot3","0"};


	// Crossbow
	CVAR_REGISTER_SKILL ( sk_plr_xbow_bolt_monster1 );// {"sk_plr_xbow_bolt1","0"};
	CVAR_REGISTER_SKILL ( sk_plr_xbow_bolt_monster2 );// {"sk_plr_xbow_bolt2","0"};
	CVAR_REGISTER_SKILL ( sk_plr_xbow_bolt_monster3 );// {"sk_plr_xbow_bolt3","0"};

	CVAR_REGISTER_SKILL ( sk_plr_xbow_bolt_client1 );// {"sk_plr_xbow_bolt1","0"};
	CVAR_REGISTER_SKILL ( sk_plr_xbow_bolt_client2 );// {"sk_plr_xbow_bolt2","0"};
	CVAR_REGISTER_SKILL ( sk_plr_xbow_bolt_client3 );// {"sk_plr_xbow_bolt3","0"};


	// RPG
	CVAR_REGISTER_SKILL ( sk_plr_rpg1 );// {"sk_plr_rpg1","0"};
	CVAR_REGISTER_SKILL ( sk_plr_rpg2 );// {"sk_plr_rpg2","0"};
	CVAR_REGISTER_SKILL ( sk_plr_rpg3 );// {"sk_plr_rpg3","0"};


	// Gauss Gun
	CVAR_REGISTER_SKILL ( sk_plr_gauss1 );// {"sk_plr_gauss1","0"};
	CVAR_REGISTER_SKILL ( sk_plr_gauss2 );// {"sk_plr_gauss2","0"};
	CVAR_REGISTER_SKILL ( sk_plr_gauss3 );// {"sk_plr_gauss3","0"};


	// Egon Gun
	CVAR_REGISTER_SKILL ( sk_plr_egon_narrow1 );// {"sk_plr_egon_narrow1","0"};
	CVAR_REGISTER_SKILL ( sk_plr_egon_narrow2 );// {"sk_plr_egon_narrow2","0"};
	CVAR_REGISTER_SKILL ( sk_plr_egon_narrow3 );// {"sk_plr_egon_narrow3","0"};

	CVAR_REGISTER_SKILL ( sk_plr_egon_wide1 );// {"sk_plr_egon_wide1","0"};
	CVAR_REGISTER_SKILL ( sk_plr_egon_wide2 );// {"sk_plr_egon_wide2","0"};
	CVAR_REGISTER_SKILL ( sk_plr_egon_wide3 );// {"sk_plr_egon_wide3","0"};


	// Hand Grendade
	CVAR_REGISTER_SKILL ( sk_plr_hand_grenade1 );// {"sk_plr_hand_grenade1","0"};
	CVAR_REGISTER_SKILL ( sk_plr_hand_grenade2 );// {"sk_plr_hand_grenade2","0"};
	CVAR_REGISTER_SKILL ( sk_plr_hand_grenade3 );// {"sk_plr_hand_grenade3","0"};


	// Satchel Charge
	CVAR_REGISTER_SKILL ( sk_plr_satchel1 );// {"sk_plr_satchel1","0"};
	CVAR_REGISTER_SKILL ( sk_plr_satchel2 );// {"sk_plr_satchel2","0"};
	CVAR_REGISTER_SKILL ( sk_plr_satchel3 );// {"sk_plr_satchel3","0"};


	// Tripmine
	CVAR_REGISTER_SKILL ( sk_plr_tripmine1 );// {"sk_plr_tripmine1","0"};
	CVAR_REGISTER_SKILL ( sk_plr_tripmine2 );// {"sk_plr_tripmine2","0"};
	CVAR_REGISTER_SKILL ( sk_plr_tripmine3 );// {"sk_plr_tripmine3","0"};


	// WORLD WEAPONS
	CVAR_REGISTER_SKILL ( sk_12mm_bullet1 );// {"sk_12mm_bullet1","0"};
	CVAR_REGISTER_SKILL ( sk_12mm_bullet2 );// {"sk_12mm_bullet2","0"};
	CVAR_REGISTER_SKILL ( sk_12mm_bullet3 );// {"sk_12mm_bullet3","0"};

	CVAR_REGISTER_SKILL ( sk_9mmAR_bullet1 );// {"sk_9mm_bullet1","0"};
	CVAR_REGISTER_SKILL ( sk_9mmAR_bullet2 );// {"sk_9mm_bullet1","0"};
	CVAR_REGISTER_SKILL ( sk_9mmAR_bullet3 );// {"sk_9mm_bullet1","0"};

	CVAR_REGISTER_SKILL ( sk_9mm_bullet1 );// {"sk_9mm_bullet1","0"};
	CVAR_REGISTER_SKILL ( sk_9mm_bullet2 );// {"sk_9mm_bullet2","0"};
	CVAR_REGISTER_SKILL ( sk_9mm_bullet3 );// {"sk_9mm_bullet3","0"};


	// HORNET
	CVAR_REGISTER_SKILL ( sk_hornet_dmg1 );// {"sk_hornet_dmg1","0"};
	CVAR_REGISTER_SKILL ( sk_hornet_dmg2 );// {"sk_hornet_dmg2","0"};
	CVAR_REGISTER_SKILL ( sk_hornet_dmg3 );// {"sk_hornet_dmg3","0"};

	// HEALTH/SUIT CHARGE DISTRIBUTION
	CVAR_REGISTER_SKILL ( sk_suitcharger1 );
	CVAR_REGISTER_SKILL ( sk_suitcharger2 );
	CVAR_REGISTER_SKILL ( sk_suitcharger3 );

	CVAR_REGISTER_SKILL ( sk_battery1 );
	CVAR_REGISTER_SKILL ( sk_battery2 );
	CVAR_REGISTER_SKILL ( sk_battery3 );

	CVAR_REGISTER_SKILL ( sk_healthcharger1 );
	CVAR_REGISTER_SKILL ( sk_healthcharger2 );
	CVAR_REGISTER_SKILL ( sk_healthcharger3 );

	CVAR_REGISTER_SKILL ( sk_healthkit1 );
	CVAR_REGISTER_SKILL ( sk_healthkit2 );
	CVAR_REGISTER_SKILL ( sk_healthkit3 );

	CVAR_REGISTER_SKILL ( sk_scientist_heal1 );
	CVAR_REGISTER_SKILL ( sk_scientist_heal2 );
	CVAR_REGISTER_SKILL ( sk_scientist_heal3 );
	
	CVAR_REGISTER_SKILL ( sk_flashcharge1 );
	CVAR_REGISTER_SKILL ( sk_flashcharge2 );
	CVAR_REGISTER_SKILL ( sk_flashcharge3 );
	
// monster damage adjusters
	CVAR_REGISTER_SKILL ( sk_monster_head1 );
	CVAR_REGISTER_SKILL ( sk_monster_head2 );
	CVAR_REGISTER_SKILL ( sk_monster_head3 );

	CVAR_REGISTER_SKILL ( sk_monster_chest1 );
	CVAR_REGISTER_SKILL ( sk_monster_chest2 );
	CVAR_REGISTER_SKILL ( sk_monster_chest3 );

	CVAR_REGISTER_SKILL ( sk_monster_stomach1 );
	CVAR_REGISTER_SKILL ( sk_monster_stomach2 );
	CVAR_REGISTER_SKILL ( sk_monster_stomach3 );

	CVAR_REGISTER_SKILL ( sk_monster_arm1 );
	CVAR_REGISTER_SKILL ( sk_monster_arm2 );
	CVAR_REGISTER_SKILL ( sk_monster_arm3 );

	CVAR_REGISTER_SKILL ( sk_monster_leg1 );
	CVAR_REGISTER_SKILL ( sk_monster_leg2 );
	CVAR_REGISTER_SKILL ( sk_monster_leg3 );

// player damage adjusters
	CVAR_REGISTER_SKILL ( sk_player_head1 );
	CVAR_REGISTER_SKILL ( sk_player_head2 );
	CVAR_REGISTER_SKILL ( sk_player_head3 );

	CVAR_REGISTER_SKILL ( sk_player_chest1 );
	CVAR_REGISTER_SKILL ( sk_player_chest2 );
	CVAR_REGISTER_SKILL ( sk_player_chest3 );

	CVAR_REGISTER_SKILL ( sk_player_stomach1 );
	CVAR_REGISTER_SKILL ( sk_player_stomach2 );
	CVAR_REGISTER_SKILL ( sk_player_stomach3 );

	CVAR_REGISTER_SKILL ( sk_player_arm1 );
	CVAR_REGISTER_SKILL ( sk_player_arm2 );
	CVAR_REGISTER_SKILL ( sk_player_arm3 );

	CVAR_REGISTER_SKILL ( sk_player_leg1 );
	CVAR_REGISTER_SKILL ( sk_player_leg2 );
	CVAR_REGISTER_SKILL ( sk_player_leg3 );
// END REGISTER CVARS FOR SKILL LEVEL STUFF

	SERVER_COMMAND( "exec skill.rc\n" );
}

// perform any shutdown operations
void GameDLLShutdown( void )
{
}