/*
+------+
|Dummys|
+------+-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
| Scratch                                      Http://www.admdev.com/scratch |
+=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
| This file contains remove(pev); statements for entities not yet coded.    |
| This avoids Quake spewing out pages and pages of error messages when       |
| loading maps.                                                              |
+=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-+
*/
// General Junk

void() event_lightning          = {remove(pev);};
void() misc_fireball            = {remove(pev);};
void() misc_explobox2           = {remove(pev);};
void() trap_spikeshooter        = {remove(pev);};
void() trap_shooter             = {remove(pev);};
void() func_bossgate            = {remove(pev);};
void() func_episodegate         = {remove(pev);};
//void() func_illusionary         = {remove(pev);};
//void() func_train               = {remove(pev);};
//void() func_button              = {remove(pev);};
//void() func_door                = {remove(pev);};
void() func_door_secret         = {remove(pev);};
void() func_plat                = {remove(pev);};
void() info_intermission        = {remove(pev);};
void() info_null                = {remove(pev);};
//void() info_teleport_destination= {remove(pev);};
//void() path_corner              = {remove(pev);};

// Triggers
//void() trigger_relay            = {remove(pev);};
//void() trigger_multiple         = {remove(pev);};
//void() trigger_once             = {remove(pev);};
//void() trigger_changelevel      = {remove(pev);};
//void() trigger_counter          = {remove(pev);};
//void() trigger_teleport         = {remove(pev);};
//void() trigger_secret           = {remove(pev);};
//void() trigger_setskill         = {remove(pev);};
void() trigger_monsterjump      = {remove(pev);};
void() trigger_onlyregistered   = {remove(pev);};
//void() trigger_push             = {remove(pev);};
//void() trigger_hurt             = {remove(pev);};

// Player Starts
void() info_player_start        = {};
//void() info_player_start2       = {};
void() info_player_deathmatch   = {};
void() info_player_coop         = {};
void() info_target = {};

// Weapons
void() weapon_supershotgun      = {remove(pev);};
void() weapon_nailgun           = {remove(pev);};
void() weapon_supernailgun      = {remove(pev);};
void() weapon_grenadelauncher   = {remove(pev);};
void() weapon_rocketlauncher    = {remove(pev);};
void() weapon_lightning         = {remove(pev);};

// Monsters
void() monster_enforcer         = {remove(pev);};
void() monster_ogre             = {remove(pev);};
void() monster_demon1           = {remove(pev);};
void() monster_shambler         = {remove(pev);};
void() monster_knight           = {remove(pev);};
void() monster_army             = {remove(pev);};
void() monster_wizard           = {remove(pev);};
void() monster_dog              = {remove(pev);};
void() monster_zombie           = {remove(pev);};
void() monster_tarbaby          = {remove(pev);};
void() monster_hell_knight      = {remove(pev);};
void() monster_fish             = {remove(pev);};
void() monster_shalrath         = {remove(pev);};
void() monster_oldone           = {remove(pev);};

void() item_health              = {remove(pev);};
void() item_megahealth_rot      = {remove(pev);};
void() item_armor1              = {remove(pev);};
void() item_armor2              = {remove(pev);};
void() item_armorInv            = {remove(pev);};
void() item_shells              = {remove(pev);};
void() item_spikes              = {remove(pev);};
void() item_rockets             = {remove(pev);};
void() item_cells               = {remove(pev);};
void() item_key1                = {remove(pev);};
void() item_key2                = {remove(pev);};
void() item_artifact_invulnerability = {remove(pev);};
void() item_artifact_envirosuit = {remove(pev);};
void() item_artifact_invisibility = {remove(pev);};
void() item_artifact_super_damage = {remove(pev);};

void barrel_touch( void )
{
	float	ratio;
	vector	v;

	// only players can move barrel
	if (!(other->flags & FL_CLIENT))
		return;

	ratio = (float)other->mass / (float)pev->mass;
	v = pev->origin - other->origin;
	walkmove(vectoyaw(v), 20 * ratio * frametime);
}

void barrel_spawn(string netname1, string model1, string deathmessage, float damage)
{
	local float oldz;

	precache_model (model1);
	precache_sound ("weapons/r_exp3.wav");

	if (!pev->dmg) pev->dmg = damage;
	pev->netname = netname1;

	pev->owner = pev;
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_STEP;
	setmodel (pev, model1);
	pev->health = 20;
	pev->th_die = SUB_Null;
	pev->takedamage = DAMAGE_AIM;
	pev->think = SUB_Null;
	pev->nextthink = -1;
	pev->touch = barrel_touch;
	pev->flags = 0;
	if(!pev->mass) pev->mass = 25;

	pev->origin_z = pev->origin_z + 2;
	oldz = pev->origin_z;

	droptofloor();

	if (oldz - pev->origin_z > 250)
	{
		MsgWarn ("explosive box fell out of level at", vtoa(pev->origin), "\n" );
		remove(pev);
	}
}

void() misc_explobox =
{	
	barrel_spawn("Large exploding box", "models/barrel2.mdl", " was blown up by an explosive box", 750);
};

void misc_physbox ( void )
{
	precache_model ("models/box2.mdl");
	pev->owner = pev;
	pev->solid = SOLID_BOX;
	pev->movetype = MOVETYPE_PHYSIC;
	setmodel (pev, "models/box2.mdl");
}

void misc_barrel( void )
{
	string name = "models/barrel1.mdl";

	precache_model( name );
	pev->owner = pev;
	pev->solid = SOLID_MESH; //test
	pev->movetype = MOVETYPE_PHYSIC;
	setmodel (pev, name );
}

void misc_sphere( void )
{
	string name = "models/nexplode.mdl";

	precache_model( name );
	pev->owner = pev;
	pev->solid = SOLID_SPHERE; //test
	pev->movetype = MOVETYPE_PHYSIC;
	setmodel (pev, name );
}

void item_healthkit( void )
{
	precache_model( "models/w_medkit.mdl" );
	pev->owner = pev;
	pev->solid = SOLID_MESH;
	pev->movetype = MOVETYPE_PHYSIC;
	setmodel (pev, "models/w_medkit.mdl" );
}

void env_sprite( void )
{
	precache_model ("sprites/explode01.spr");
	pev->owner = pev;
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
	setmodel (pev, "sprites/explode01.spr");
	pev->frame = 0;
}

void walk_sprite( void )
{
	pev->frame++;
	if(pev->frame > 3) pev->frame = 0;
	pev->nextthink = time + 0.1;
	makevectors( pev->angles );
	pev->velocity = v_forward * 60;
}

void env_monster( void )
{
	precache_model ("sprites/boss.spr");
	pev->owner = pev;
	pev->solid = SOLID_BBOX;
	pev->movetype = MOVETYPE_STEP;
	setmodel (pev, "sprites/boss.spr");

	pev->nextthink = time;
	pev->think = walk_sprite;
	pev->frame = 0;
}