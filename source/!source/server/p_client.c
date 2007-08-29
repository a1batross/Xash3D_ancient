#include "baseentity.h"
#include "m_player.h"

#define MUD1BASE 0.20
#define MUD1AMP  0.08
#define MUD3     0.08

void ClientUserinfoChanged (edict_t *ent, char *userinfo);

//
// Gross, ugly, disgustuing hack section
//

// this function is an ugly as hell hack to fix some map flaws
//
// the coop spawn spots on some maps are SNAFU.  There are coop spots
// with the wrong targetname as well as spots with no name at all
//
// we use carnal knowledge of the maps to fix the coop spot targetnames to match
// that of the nearest named single player spot

static void SP_FixCoopSpots (edict_t *self)
{
	edict_t	*spot;
	vec3_t	d;

	spot = NULL;

	while(1)
	{
		spot = G_Find(spot, FOFS(classname), "info_player_start");
		if (!spot)
			return;
		if (!spot->targetname)
			continue;
		VectorSubtract(self->s.origin, spot->s.origin, d);
		if (VectorLength(d) < 384)
		{
			if ((!self->targetname) || strcasecmp(self->targetname, spot->targetname) != 0)
			{
//				gi.dprintf("FixCoopSpots changed %s at %s targetname from %s to %s\n", self->classname, vtos(self->s.origin), self->targetname, spot->targetname);
				self->targetname = spot->targetname;
			}
			return;
		}
	}
}

// now if that one wasn't ugly enough for you then try this one on for size
// some maps don't have any coop spots at all, so we need to create them
// where they should have been

static void SP_CreateCoopSpots (edict_t *self)
{
	edict_t	*spot;

	if(strcasecmp(level.mapname, "security") == 0)
	{
		spot = G_Spawn();
		spot->classname = "info_player_coop";
		spot->s.origin[0] = 188 - 64;
		spot->s.origin[1] = -164;
		spot->s.origin[2] = 80;
		spot->targetname = "jail3";
		spot->s.angles[1] = 90;

		spot = G_Spawn();
		spot->classname = "info_player_coop";
		spot->s.origin[0] = 188 + 64;
		spot->s.origin[1] = -164;
		spot->s.origin[2] = 80;
		spot->targetname = "jail3";
		spot->s.angles[1] = 90;

		spot = G_Spawn();
		spot->classname = "info_player_coop";
		spot->s.origin[0] = 188 + 128;
		spot->s.origin[1] = -164;
		spot->s.origin[2] = 80;
		spot->targetname = "jail3";
		spot->s.angles[1] = 90;

		return;
	}
}


/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
The normal starting point for a level.
*/
void SP_info_player_start(edict_t *self)
{
	self->class_id = ENTITY_INFO_PLAYER_START;
	if (!coop->value)
		return;
	if(strcasecmp(level.mapname, "security") == 0)
	{
		// invoke one of our gross, ugly, disgusting hacks
		self->think = SP_CreateCoopSpots;
		self->nextthink = level.time + FRAMETIME;
	}
}

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for deathmatch games
*/
void SP_info_player_deathmatch(edict_t *self)
{
	if (!deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}
	self->class_id = ENTITY_INFO_PLAYER_DEATHMATCH;
}

/*QUAKED info_player_coop (1 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for coop games
*/

void SP_info_player_coop(edict_t *self)
{
	if (!coop->value)
	{
		G_FreeEdict (self);
		return;
	}
	self->class_id = ENTITY_INFO_PLAYER_COOP;
	if((strcasecmp(level.mapname, "jail2") == 0)   ||
	   (strcasecmp(level.mapname, "jail4") == 0)   ||
	   (strcasecmp(level.mapname, "mine1") == 0)   ||
	   (strcasecmp(level.mapname, "mine2") == 0)   ||
	   (strcasecmp(level.mapname, "mine3") == 0)   ||
	   (strcasecmp(level.mapname, "mine4") == 0)   ||
	   (strcasecmp(level.mapname, "lab") == 0)     ||
	   (strcasecmp(level.mapname, "boss1") == 0)   ||
	   (strcasecmp(level.mapname, "fact3") == 0)   ||
	   (strcasecmp(level.mapname, "biggun") == 0)  ||
	   (strcasecmp(level.mapname, "space") == 0)   ||
	   (strcasecmp(level.mapname, "command") == 0) ||
	   (strcasecmp(level.mapname, "power2") == 0) ||
	   (strcasecmp(level.mapname, "strike") == 0))
	{
		// invoke one of our gross, ugly, disgusting hacks
		self->think = SP_FixCoopSpots;
		self->nextthink = level.time + FRAMETIME;
	}
}


/*QUAKED info_player_intermission (1 0 1) (-16 -16 -24) (16 16 32)
The deathmatch intermission point will be at one of these
Use 'angles' instead of 'angle', so you can set pitch or roll as well as yaw.  'pitch yaw roll'
*/
void SP_info_player_intermission(edict_t *self)
{
	self->class_id = ENTITY_INFO_PLAYER_INTERMISSION;
}


//=======================================================================


void player_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	// player pain is handled at the end of the frame in P_DamageFeedback
}


bool IsFemale (edict_t *ent)
{
	char		*info;

	if (!ent->client)
		return false;

	info = Info_ValueForKey (ent->client->pers.userinfo, "gender");
	if (info[0] == 'f' || info[0] == 'F')
		return true;
	return false;
}

bool IsNeutral (edict_t *ent)
{
	char		*info;

	if (!ent->client)
		return false;

	info = Info_ValueForKey (ent->client->pers.userinfo, "gender");
	if (info[0] != 'f' && info[0] != 'F' && info[0] != 'm' && info[0] != 'M')
		return true;
	return false;
}

void ClientObituary (edict_t *self, edict_t *inflictor, edict_t *attacker)
{
	int			mod;
	char		*message;
	char		*message2;
	bool	ff;

	if (coop->value && attacker->client)
		meansOfDeath |= MOD_FRIENDLY_FIRE;

	if (deathmatch->value || coop->value)
	{
		ff = meansOfDeath & MOD_FRIENDLY_FIRE;
		mod = meansOfDeath & ~MOD_FRIENDLY_FIRE;
		message = NULL;
		message2 = "";

		switch (mod)
		{
		case MOD_SUICIDE:
			message = "suicides";
			break;
		case MOD_FALLING:
			message = "cratered";
			break;
		case MOD_CRUSH:
			message = "was squished";
			break;
		case MOD_WATER:
			message = "sank like a rock";
			break;
		case MOD_SLIME:
			message = "melted";
			break;
		case MOD_LAVA:
			message = "does a back flip into the lava";
			break;
		case MOD_EXPLOSIVE:
		case MOD_BARREL:
			message = "blew up";
			break;
		case MOD_EXIT:
			message = "found a way out";
			break;
		case MOD_TARGET_LASER:
			message = "saw the light";
			break;
		case MOD_TARGET_BLASTER:
			message = "got blasted";
			break;
		case MOD_BOMB:
		case MOD_SPLASH:
		case MOD_TRIGGER_HURT:
		case MOD_VEHICLE:
			message = "was in the wrong place";
			break;
		}
		if (attacker == self)
		{
			switch (mod)
			{
			case MOD_HELD_GRENADE:
				message = "tried to put the pin back in";
				break;
			case MOD_HG_SPLASH:
			case MOD_G_SPLASH:
				if (IsNeutral(self))
					message = "tripped on its own grenade";
				else if (IsFemale(self))
					message = "tripped on her own grenade";
				else
					message = "tripped on his own grenade";
				break;
			case MOD_R_SPLASH:
				if (IsNeutral(self))
					message = "blew itself up";
				else if (IsFemale(self))
					message = "blew herself up";
				else
					message = "blew himself up";
				break;
			case MOD_BFG_BLAST:
				message = "should have used a smaller gun";
				break;
			default:
				if (IsNeutral(self))
					message = "killed itself";
				else if (IsFemale(self))
					message = "killed herself";
				else
					message = "killed himself";
				break;
			}
		}
		if (message)
		{
			gi.bprintf (PRINT_MEDIUM, "%s %s.\n", self->client->pers.netname, message);
			if (deathmatch->value)
				self->client->resp.score--;
			self->enemy = NULL;
			return;
		}

		self->enemy = attacker;
		if (attacker && attacker->client)
		{
			switch (mod)
			{
			case MOD_BLASTER:
				message = "was blasted by";
				break;
			case MOD_SHOTGUN:
				message = "was gunned down by";
				break;
			case MOD_SSHOTGUN:
				message = "was blown away by";
				message2 = "'s super shotgun";
				break;
			case MOD_MACHINEGUN:
				message = "was machinegunned by";
				break;
			case MOD_CHAINGUN:
				message = "was cut in half by";
				message2 = "'s chaingun";
				break;
			case MOD_GRENADE:
				message = "was popped by";
				message2 = "'s grenade";
				break;
			case MOD_G_SPLASH:
				message = "was shredded by";
				message2 = "'s shrapnel";
				break;
			case MOD_ROCKET:
				message = "ate";
				message2 = "'s rocket";
				break;
			case MOD_R_SPLASH:
				message = "almost dodged";
				message2 = "'s rocket";
				break;
			case MOD_HYPERBLASTER:
				message = "was melted by";
				message2 = "'s hyperblaster";
				break;
			case MOD_RAILGUN:
				message = "was railed by";
				break;
			case MOD_BFG_LASER:
				message = "saw the pretty lights from";
				message2 = "'s BFG";
				break;
			case MOD_BFG_BLAST:
				message = "was disintegrated by";
				message2 = "'s BFG blast";
				break;
			case MOD_BFG_EFFECT:
				message = "couldn't hide from";
				message2 = "'s BFG";
				break;
			case MOD_HANDGRENADE:
				message = "caught";
				message2 = "'s handgrenade";
				break;
			case MOD_HG_SPLASH:
				message = "didn't see";
				message2 = "'s handgrenade";
				break;
			case MOD_HELD_GRENADE:
				message = "feels";
				message2 = "'s pain";
				break;
			case MOD_TELEFRAG:
				message = "tried to invade";
				message2 = "'s personal space";
				break;
			case MOD_VEHICLE:
				message = "was splattered by";
				message2 = "'s vehicle";
				break;
			case MOD_KICK:
				message = "was booted by";
				message2 = "'s foot";
				break;
			}
			if (message)
			{
				gi.bprintf (PRINT_MEDIUM,"%s %s %s%s\n", self->client->pers.netname, message, attacker->client->pers.netname, message2);
				if (deathmatch->value)
				{
					if (ff)
						attacker->client->resp.score--;
					else
						attacker->client->resp.score++;
				}
				return;
			}
		}
	}

	gi.bprintf (PRINT_MEDIUM,"%s died.\n", self->client->pers.netname);
	if (deathmatch->value)
		self->client->resp.score--;
}


void Touch_Item (edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf);

void TossClientWeapon (edict_t *self)
{
	gitem_t		*item;
	edict_t		*drop;
	bool	quad;
	float		spread;

	if (!deathmatch->value)
		return;

	item = self->client->pers.weapon;
	if (! self->client->pers.inventory[self->client->ammo_index] )
		item = NULL;
	if (item && (strcmp (item->pickup_name, "Blaster") == 0))
		item = NULL;
	if (item && (strcmp (item->pickup_name, "No Weapon") == 0))
		item = NULL;

	if (!((int)(dmflags->value) & DF_QUAD_DROP))
		quad = false;
	else
		quad = (self->client->quad_framenum > (level.framenum + 10));

	if (item && quad)
		spread = 22.5;
	else
		spread = 0.0;

	if (item)
	{
		self->client->v_angle[YAW] -= spread;
		drop = Drop_Item (self, item);
		self->client->v_angle[YAW] += spread;
		drop->spawnflags = DROPPED_PLAYER_ITEM;
	}

	if (quad)
	{
		self->client->v_angle[YAW] += spread;
		drop = Drop_Item (self, FindItemByClassname ("item_quad"));
		self->client->v_angle[YAW] -= spread;
		drop->spawnflags |= DROPPED_PLAYER_ITEM;

		drop->touch = Touch_Item;
		drop->nextthink = level.time + (self->client->quad_framenum - level.framenum) * FRAMETIME;
		drop->think = G_FreeEdict;
	}
}


/*
==================
LookAtKiller
==================
*/
void LookAtKiller (edict_t *self, edict_t *inflictor, edict_t *attacker)
{
	vec3_t		dir;

	if (attacker && attacker != world && attacker != self)
	{
		VectorSubtract (attacker->s.origin, self->s.origin, dir);
	}
	else if (inflictor && inflictor != world && inflictor != self)
	{
		VectorSubtract (inflictor->s.origin, self->s.origin, dir);
	}
	else
	{
		self->client->killer_yaw = self->s.angles[YAW];
		return;
	}

	if (dir[0])
		self->client->killer_yaw = 180/M_PI*atan2(dir[1], dir[0]);
	else {
		self->client->killer_yaw = 0;
		if (dir[1] > 0)
			self->client->killer_yaw = 90;
		else if (dir[1] < 0)
			self->client->killer_yaw = -90;
	}
	if (self->client->killer_yaw < 0)
		self->client->killer_yaw += 360;
	

}

/*
==================
player_die
==================
*/
void player_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		n;

	self->client->pers.spawn_landmark = false; // paranoia check
	self->client->pers.spawn_levelchange = false;
	self->client->zooming = 0;
	self->client->zoomed = false;

	VectorClear (self->avelocity);

	self->takedamage = DAMAGE_YES;
	self->movetype = MOVETYPE_TOSS;

	self->s.weaponmodel = 0;	// remove linked weapon model

	self->s.angles[0] = 0;
	self->s.angles[2] = 0;

	self->s.sound = 0;
	self->client->weapon_sound = 0;

	self->maxs[2] = -8;

//	self->solid = SOLID_NOT;
	self->svflags |= SVF_DEADMONSTER;

	if (!self->deadflag)
	{
		self->client->respawn_time = level.time + 1.0;
		LookAtKiller (self, inflictor, attacker);
		self->client->ps.pmove.pm_type = PM_DEAD;
		ClientObituary (self, inflictor, attacker);
		TossClientWeapon (self);
		if (deathmatch->value)
			Cmd_Help_f (self);		// show scores

		// clear inventory
		// this is kind of ugly, but it's how we want to handle keys in coop
		for (n = 0; n < game.num_items; n++)
		{
			if (coop->value && itemlist[n].flags & IT_KEY)
				self->client->resp.coop_respawn.inventory[n] = self->client->pers.inventory[n];
			self->client->pers.inventory[n] = 0;
		}
	}

	// remove powerups
	self->client->quad_framenum = 0;
	self->client->invincible_framenum = 0;
	self->client->breather_framenum = 0;
	self->client->enviro_framenum = 0;
	self->flags &= ~FL_POWER_ARMOR;
	self->client->flashlight = false;

	// turn off alt-fire mode if on
	self->client->pers.fire_mode=0;
	self->client->nNewLatch &= ~BUTTON_ATTACK2;

	if (self->health < self->gib_health)
	{	// gib
		gi.sound (self, CHAN_BODY, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		for (n= 0; n < 4; n++)
			ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
		ThrowClientHead (self, damage);

		self->takedamage = DAMAGE_NO;
	}
	else
	{	// normal death
		if (!self->deadflag)
		{
			static int i;

			i = (i+1)%3;
			// start a death animation
			self->client->anim_priority = ANIM_DEATH;
			if (self->client->ps.pmove.pm_flags & PMF_DUCKED)
			{
				self->s.frame = FRAME_crdeath1-1;
				self->client->anim_end = FRAME_crdeath5;
			}
			else switch (i)
			{
			case 0:
				self->s.frame = FRAME_death101-1;
				self->client->anim_end = FRAME_death106;
				break;
			case 1:
				self->s.frame = FRAME_death201-1;
				self->client->anim_end = FRAME_death206;
				break;
			case 2:
				self->s.frame = FRAME_death301-1;
				self->client->anim_end = FRAME_death308;
				break;
			}
			gi.sound (self, CHAN_VOICE, gi.soundindex(va("*death%i.wav", (rand()%4)+1)), 1, ATTN_NORM, 0);
		}
	}
	self->deadflag = DEAD_DEAD;
	gi.linkentity (self);

}

//=======================================================================

void SelectStartWeapon (gclient_t *client, int style)
{
	gitem_t	*item;
	int		n;

	// Lazarus: We allow choice of weapons (or no weapon) at startup
	// If style is non-zero, first clear player inventory of all
	// weapons and ammo that might have been passed over through
	// target_changelevel or acquired when previously called by 
	// InitClientPersistant
	if(style)
	{
		for (n = 0; n < MAX_ITEMS; n++)
		{
			if (itemlist[n].flags & IT_WEAPON)
				client->pers.inventory[n] = 0;
		}
		client->pers.inventory[shells_index]   = 0;
		client->pers.inventory[bullets_index]  = 0;
		client->pers.inventory[grenades_index] = 0;
		client->pers.inventory[rockets_index]  = 0;
		client->pers.inventory[cells_index]    = 0;
		client->pers.inventory[slugs_index]    = 0;
		client->pers.inventory[homing_index]   = 0;
	}

	switch(style)
	{
	case -1:
		item = FindItem("No Weapon");
		break;
	case -2:
	case  2:
		item = FindItem("Shotgun");
		break;
	case -3:
	case  3:
		item = FindItem("Super Shotgun");
		break;
	case -4:
	case  4:
		item = FindItem("Machinegun");
		break;
	case -5:
	case  5:
		item = FindItem("Chaingun");
		break;
	case -6:
	case  6:
		item = FindItem("Grenade Launcher");
		break;
	case -7:
	case  7:
		item = FindItem("Rocket Launcher");
		break;
	case -8:
	case  8:
		item = FindItem("HyperBlaster");
		break;
	case -9:
	case  9:
		item = FindItem("Railgun");
		break;
	case -10:
	case  10:
		item = FindItem("BFG10K");
		break;
	default:
		item = FindItem("Blaster");
		break;
	}
	client->pers.selected_item = ITEM_INDEX(item);
	client->pers.inventory[client->pers.selected_item] = 1;
	client->pers.weapon = item;

	// Lazarus: If default weapon is NOT "No Weapon", then give player
	//          a blaster
	if(style > 1)
		client->pers.inventory[ITEM_INDEX(FindItem("Blaster"))] = 1;

	// and give him standard ammo
	if (item->ammo)
	{
		gitem_t	*ammo;

		ammo = FindItem (item->ammo);
		if ( deathmatch->value && ((int)dmflags->value & DF_INFINITE_AMMO) )
			client->pers.inventory[ITEM_INDEX(ammo)] += 1000;
		else
			client->pers.inventory[ITEM_INDEX(ammo)] += ammo->quantity;
	}
}

/*
==============
InitClientPersistant

This is only called when the game first initializes in single player,
but is called after each death and level change in deathmatch
==============
*/
void InitClientPersistant (gclient_t *client, int style)
{
	memset (&client->pers, 0, sizeof(client->pers));

	client->homing_rocket = NULL;
	SelectStartWeapon (client, style);

	client->pers.health			= 100;
	client->pers.max_health		= 100;

	client->pers.max_bullets	= 200;
	client->pers.max_shells		= 100;
	client->pers.max_rockets	= 50;
	client->pers.max_grenades	= 50;
	client->pers.max_cells		= 200;
	client->pers.max_slugs		= 50;
	client->pers.max_fuel       = 1000;
	client->pers.max_homing_missiles = 50;
	client->pers.fire_mode      = 0;  // Lazarus alternate fire mode

	client->pers.connected = true;

	// Lazarus
	client->zooming = 0;
	client->zoomed = false;
	client->spycam = NULL;
	client->pers.spawn_landmark = false;
	client->pers.spawn_levelchange = false;
}


void InitClientResp (gclient_t *client)
{
	memset (&client->resp, 0, sizeof(client->resp));
	client->resp.enterframe = level.framenum;
	client->resp.coop_respawn = client->pers;
}

/*
==================
SaveClientData

Some information that should be persistant, like health, 
is still stored in the edict structure, so it needs to
be mirrored out to the client structure before all the
edicts are wiped.
==================
*/
void SaveClientData (void)
{
	int		i;
	edict_t	*ent;

	for (i=0 ; i<game.maxclients ; i++)
	{
		ent = &g_edicts[1+i];
		if (!ent->inuse)
			continue;
		game.clients[i].pers.newweapon = ent->client->newweapon;
		game.clients[i].pers.health = ent->health;
		game.clients[i].pers.max_health = ent->max_health;
		game.clients[i].pers.savedFlags = (ent->flags & (FL_GODMODE|FL_NOTARGET|FL_POWER_ARMOR));
		if (coop->value)
			game.clients[i].pers.score = ent->client->resp.score;
	}
}

void FetchClientEntData (edict_t *ent)
{
	ent->health = ent->client->pers.health;
	ent->gib_health = -40;
	ent->max_health = ent->client->pers.max_health;
	ent->flags |= ent->client->pers.savedFlags;
	if (coop->value)
		ent->client->resp.score = ent->client->pers.score;
}



/*
=======================================================================

  SelectSpawnPoint

=======================================================================
*/

/*
================
PlayersRangeFromSpot

Returns the distance to the nearest player from the given spot
================
*/
float	PlayersRangeFromSpot (edict_t *spot)
{
	edict_t	*player;
	float	bestplayerdistance;
	vec3_t	v;
	int		n;
	float	playerdistance;


	bestplayerdistance = 9999999;

	for (n = 1; n <= maxclients->value; n++)
	{
		player = &g_edicts[n];

		if (!player->inuse)
			continue;

		if (player->health <= 0)
			continue;

		VectorSubtract (spot->s.origin, player->s.origin, v);
		playerdistance = VectorLength (v);

		if (playerdistance < bestplayerdistance)
			bestplayerdistance = playerdistance;
	}

	return bestplayerdistance;
}

/*
================
SelectRandomDeathmatchSpawnPoint

go to a random point, but NOT the two points closest
to other players
================
*/
edict_t *SelectRandomDeathmatchSpawnPoint (void)
{
	edict_t	*spot, *spot1, *spot2;
	int		count = 0;
	int		selection;
	float	range, range1, range2;

	spot = NULL;
	range1 = range2 = 99999;
	spot1 = spot2 = NULL;

	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL)
	{
		count++;
		range = PlayersRangeFromSpot(spot);
		if (range < range1)
		{
			range1 = range;
			spot1 = spot;
		}
		else if (range < range2)
		{
			range2 = range;
			spot2 = spot;
		}
	}

	if (!count)
		return NULL;

	if (count <= 2)
	{
		spot1 = spot2 = NULL;
	}
	// Lazarus: This is wrong. If there is no spot1 or spot2, all spots should
	// be valid.
//	else
//		count -= 2;
	else
	{
		if(spot1) count--;
		if(spot2) count--;
	}

	selection = rand() % count;

	spot = NULL;
	do
	{
		spot = G_Find (spot, FOFS(classname), "info_player_deathmatch");
		if (spot == spot1 || spot == spot2)
			selection++;
	} while(selection--);

	return spot;
}

/*
================
SelectFarthestDeathmatchSpawnPoint

================
*/
edict_t *SelectFarthestDeathmatchSpawnPoint (void)
{
	edict_t	*bestspot;
	float	bestdistance, bestplayerdistance;
	edict_t	*spot;


	spot = NULL;
	bestspot = NULL;
	bestdistance = 0;
	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL)
	{
		bestplayerdistance = PlayersRangeFromSpot (spot);

		if (bestplayerdistance > bestdistance)
		{
			bestspot = spot;
			bestdistance = bestplayerdistance;
		}
	}

	if (bestspot)
	{
		return bestspot;
	}

	// if there is a player just spawned on each and every start spot
	// we have no choice to turn one into a telefrag meltdown
	spot = G_Find (NULL, FOFS(classname), "info_player_deathmatch");

	return spot;
}

edict_t *SelectDeathmatchSpawnPoint (void)
{
	if ( (int)(dmflags->value) & DF_SPAWN_FARTHEST)
		return SelectFarthestDeathmatchSpawnPoint ();
	else
		return SelectRandomDeathmatchSpawnPoint ();
}


edict_t *SelectCoopSpawnPoint (edict_t *ent)
{
	int		index;
	edict_t	*spot = NULL;
	char	*target;

	index = ent->client - game.clients;

	// player 0 starts in normal player spawn point
	if (!index)
		return NULL;

	spot = NULL;

	// assume there are four coop spots at each spawnpoint
	while (1)
	{
		spot = G_Find (spot, FOFS(classname), "info_player_coop");
		if (!spot)
			return NULL;	// we didn't have enough...

		target = spot->targetname;
		if (!target)
			target = "";
		if ( strcasecmp(game.spawnpoint, target) == 0 )
		{	// this is a coop spawn point for one of the clients here
			index--;
			if (!index)
				return spot;		// this is it
		}
	}
	return spot;
}


/*
===========
SelectSpawnPoint

Chooses a player start, deathmatch start, coop start, etc
============
*/
void	SelectSpawnPoint (edict_t *ent, vec3_t origin, vec3_t angles, int *style, int *health)
{
	edict_t	*spot = NULL;

	if (deathmatch->value)
		spot = SelectDeathmatchSpawnPoint ();
	else if (coop->value)
		spot = SelectCoopSpawnPoint (ent);

	// find a single player start spot
	if (!spot)
	{
		while ((spot = G_Find (spot, FOFS(classname), "info_player_start")) != NULL)
		{
			if (!game.spawnpoint[0] && !spot->targetname)
				break;

			if (!game.spawnpoint[0] || !spot->targetname)
				continue;

			if (strcasecmp(game.spawnpoint, spot->targetname) == 0)
				break;
		}

		if (!spot)
		{
			if (!game.spawnpoint[0])
			{	// there wasn't a spawnpoint without a target, so use any
				spot = G_Find (spot, FOFS(classname), "info_player_start");
			}
			if (!spot)
				gi.error ("Couldn't find spawn point %s\n", game.spawnpoint);
		}
	}

	*style = spot->style;
	*health = spot->health;
	VectorCopy (spot->s.origin, origin);
	origin[2] += 9;
	VectorCopy (spot->s.angles, angles);

	if(!deathmatch->value && !coop->value) {

		spot->count--;
		if(!spot->count) {
			spot->think = G_FreeEdict;
			spot->nextthink = level.time + 1;
		}
	}
}

//======================================================================


void InitBodyQue (void)
{
	// DWH: bodyque isn't used in SP, so why reserve space for it?
	if (deathmatch->value || coop->value) {

		int		i;
		edict_t	*ent;

		level.body_que = 0;
		for (i=0; i<BODY_QUEUE_SIZE ; i++)
		{
			ent = G_Spawn();
			ent->classname = "bodyque";
		}
	}
}

void body_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int	n;

	if (self->health < self->gib_health)
	{
		gi.sound (self, CHAN_BODY, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		for (n= 0; n < 4; n++)
			ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
		self->s.origin[2] -= 48;
		ThrowClientHead (self, damage);
		self->takedamage = DAMAGE_NO;
	}
}

void CopyToBodyQue (edict_t *ent)
{
	edict_t		*body;

	// grab a body que and cycle to the next one
	body = &g_edicts[(int)maxclients->value + level.body_que + 1];
	level.body_que = (level.body_que + 1) % BODY_QUEUE_SIZE;

	// FIXME: send an effect on the removed body

	gi.unlinkentity (ent);

	gi.unlinkentity (body);
	body->s = ent->s;
	body->s.number = body - g_edicts;

	body->svflags = ent->svflags;
	VectorCopy (ent->mins, body->mins);
	VectorCopy (ent->maxs, body->maxs);
	VectorCopy (ent->absmin, body->absmin);
	VectorCopy (ent->absmax, body->absmax);
	VectorCopy (ent->size, body->size);
	body->solid = ent->solid;
	body->clipmask = ent->clipmask;
	body->owner = ent->owner;
	body->movetype = ent->movetype;

	body->die = body_die;
	body->takedamage = DAMAGE_YES;

	gi.linkentity (body);
}


void respawn (edict_t *self)
{
	if (deathmatch->value || coop->value)
	{
		// spectator's don't leave bodies
		if (self->movetype != MOVETYPE_NOCLIP)
			CopyToBodyQue (self);
		self->svflags &= ~SVF_NOCLIENT;
		PutClientInServer (self);

		// add a teleportation effect
		self->s.event = EV_PLAYER_TELEPORT;

		// hold in place briefly
		self->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
		self->client->ps.pmove.pm_time = 14;

		self->client->respawn_time = level.time;

		return;
	}
	// restart the entire server
	gi.AddCommandString ("menu_loadgame\n");
}

/* 
 * only called when pers.spectator changes
 * note that resp.spectator should be the opposite of pers.spectator here
 */
void spectator_respawn (edict_t *ent)
{
	int i, numspec;

	// if the user wants to become a spectator, make sure he doesn't
	// exceed max_spectators

	if (ent->client->pers.spectator) {
		char *value = Info_ValueForKey (ent->client->pers.userinfo, "spectator");
		if (*spectator_password->string && strcmp(spectator_password->string, "none") && strcmp(spectator_password->string, value)) 
		{
			gi.cprintf(ent, PRINT_HIGH, "Spectator password incorrect.\n");
			ent->client->pers.spectator = false;
			MESSAGE_BEGIN (svc_stufftext);
				WRITE_STRING ("spectator 0\n");
			MESSAGE_SEND(MSG_ONE_R, NULL, ent);
			return;
		}

		// count spectators
		for (i = 1, numspec = 0; i <= maxclients->value; i++)
			if (g_edicts[i].inuse && g_edicts[i].client->pers.spectator)
				numspec++;

		if (numspec >= maxspectators->value)
		{
			gi.cprintf(ent, PRINT_HIGH, "Server spectator limit is full.");
			ent->client->pers.spectator = false;
			// reset his spectator var
			MESSAGE_BEGIN (svc_stufftext);
			WRITE_STRING ("spectator 0\n");
			MESSAGE_SEND(MSG_ONE_R, NULL, ent);
			return;
		}
	}
	else
	{
		// he was a spectator and wants to join the game
		// he must have the right password
		char *value = Info_ValueForKey (ent->client->pers.userinfo, "password");
		if (*password->string && strcmp(password->string, "none") && 
			strcmp(password->string, value)) {
			gi.cprintf(ent, PRINT_HIGH, "Password incorrect.\n");
			ent->client->pers.spectator = true;
			MESSAGE_BEGIN (svc_stufftext);
				WRITE_STRING ("spectator 1\n");
			MESSAGE_SEND(MSG_ONE_R, NULL, ent );
			return;
		}
	}

	// clear client on respawn
	ent->client->resp.score = ent->client->pers.score = 0;

	ent->svflags &= ~SVF_NOCLIENT;
	PutClientInServer (ent);

	// add a teleportation effect
	if (!ent->client->pers.spectator)  {
		// send effect
		MESSAGE_BEGIN (svc_muzzleflash);
			WRITE_SHORT (ent-g_edicts);
			WRITE_BYTE (MZ_LOGIN);
		MESSAGE_SEND (MSG_PVS, ent->s.origin, NULL);

		// hold in place briefly
		ent->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
		ent->client->ps.pmove.pm_time = 14;
	}

	ent->client->respawn_time = level.time;

	if (ent->client->pers.spectator) 
		gi.bprintf (PRINT_HIGH, "%s has moved to the sidelines\n", ent->client->pers.netname);
	else
		gi.bprintf (PRINT_HIGH, "%s joined the game\n", ent->client->pers.netname);
}

//==============================================================


/*
===========
PutClientInServer

Called when a player connects to a server or respawns in
a deathmatch.
============
*/
void PutClientInServer (edict_t *ent)
{
	gitem_t				*newweapon;	
	extern	int			nostatus;
	vec3_t				mins = {-16, -16, -24};
	vec3_t				maxs = {16, 16, 32};
	int					index;
	vec3_t				spawn_origin, spawn_angles, spawn_viewangles;
	gclient_t			*client;
	int		i;
	bool			spawn_landmark;
	bool			spawn_levelchange;
	int					spawn_gunframe;
	int					spawn_modelframe;
	int					spawn_anim_end;
	int					spawn_pm_flags;
	int					spawn_style;
	int					spawn_health;
	client_persistant_t	saved;
	client_respawn_t	resp;

	// find a spawn point
	// do it before setting health back up, so farthest
	// ranging doesn't count this client
	SelectSpawnPoint (ent, spawn_origin, spawn_angles, &spawn_style, &spawn_health);

	index = ent-g_edicts-1;
	client = ent->client;
	newweapon = client->pers.newweapon;
	spawn_landmark   = client->pers.spawn_landmark;
	spawn_levelchange= client->pers.spawn_levelchange;
	spawn_gunframe   = client->pers.spawn_gunframe;
	spawn_modelframe = client->pers.spawn_modelframe;
	spawn_anim_end   = client->pers.spawn_anim_end;
	client->pers.spawn_landmark = false;
	client->pers.spawn_levelchange = false;

	if(spawn_landmark)
	{
		spawn_origin[2] -= 9;
		VectorAdd(spawn_origin,client->pers.spawn_offset,spawn_origin);
		VectorCopy(client->pers.spawn_angles,spawn_angles);
		VectorCopy(client->pers.spawn_viewangles,spawn_viewangles);
		VectorCopy(client->pers.spawn_velocity,ent->velocity);
		spawn_pm_flags = client->pers.spawn_pm_flags;
	}

	// deathmatch wipes most client data every spawn
	if (deathmatch->value)
	{
		char		userinfo[MAX_INFO_STRING];

		resp = client->resp;
		memcpy (userinfo, client->pers.userinfo, sizeof(userinfo));
		InitClientPersistant (client,spawn_style);
		ClientUserinfoChanged (ent, userinfo);
	}
	else if (coop->value)
	{
//		int			n;
		char		userinfo[MAX_INFO_STRING];

		resp = client->resp;
		memcpy (userinfo, client->pers.userinfo, sizeof(userinfo));
		resp.coop_respawn.game_helpchanged = client->pers.game_helpchanged;
		resp.coop_respawn.helpchanged = client->pers.helpchanged;
		client->pers = resp.coop_respawn;
		ClientUserinfoChanged (ent, userinfo);
		if (resp.score > client->pers.score)
			client->pers.score = resp.score;
	}
	else
	{
		memset (&resp, 0, sizeof(resp));
	}

	// clear everything but the persistant data
	saved = client->pers;
	memset (client, 0, sizeof(*client));
	client->pers = saved;
	if (client->pers.health <= 0)
		InitClientPersistant(client,spawn_style);
	else if(spawn_style)
		SelectStartWeapon(client,spawn_style);

	client->resp = resp;
	client->pers.newweapon = newweapon;

	// copy some data from the client to the entity
	FetchClientEntData (ent);

	// Lazarus: Starting health < max. Presumably player was hurt in a crash
	if( (spawn_health > 0) && !deathmatch->value && !coop->value)
		ent->health = min(ent->health, spawn_health);

	// clear entity values
	ent->groundentity = NULL;
	ent->client = &game.clients[index];
	ent->takedamage = DAMAGE_AIM;
	ent->movetype = MOVETYPE_WALK;
	ent->viewheight = 22;
	ent->inuse = true;
	ent->classname = "player";
	ent->mass = 200;
	ent->solid = SOLID_BBOX;
	ent->deadflag = DEAD_NO;
	ent->air_finished = level.time + 12;
	ent->clipmask = MASK_PLAYERSOLID;
	ent->model = "players/male/tris.md2";
	ent->pain = player_pain;
	ent->die = player_die;
	ent->waterlevel = 0;
	ent->watertype = 0;
	ent->flags &= ~FL_NO_KNOCKBACK;
	ent->svflags &= ~SVF_DEADMONSTER;
	ent->client->spycam = NULL;
	ent->client->camplayer = NULL;

	VectorCopy (mins, ent->mins);
	VectorCopy (maxs, ent->maxs);

	if(!spawn_landmark)
		VectorClear (ent->velocity);

	// clear playerstate values
	memset (&ent->client->ps, 0, sizeof(client->ps));

	if(spawn_landmark)
		client->ps.pmove.pm_flags = spawn_pm_flags;

	client->ps.pmove.origin[0] = spawn_origin[0]*8;
	client->ps.pmove.origin[1] = spawn_origin[1]*8;
	client->ps.pmove.origin[2] = spawn_origin[2]*8;

	if (deathmatch->value && ((int)dmflags->value & DF_FIXED_FOV))
	{
		client->ps.fov = 90;
	}
	else
	{
		client->ps.fov = atoi(Info_ValueForKey(client->pers.userinfo, "fov"));
		if (client->ps.fov < 1)
			client->ps.fov = 90;
		else if (client->ps.fov > 160)
			client->ps.fov = 160;
	}
	// DWH
	client->original_fov  = client->ps.fov;
	// end DWH

	client->ps.gunindex = gi.modelindex(client->pers.weapon->view_model);

	// clear entity state values
	ent->s.effects = 0;
	ent->s.modelindex = MAX_MODELS-1;		// will use the skin specified model

	if(ITEM_INDEX(client->pers.weapon) == noweapon_index)
		ent->s.weaponmodel = 0;
	else ent->s.weaponmodel = MAX_MODELS - 1;	// custom gun model

	// sknum is player num and weapon number
	// weapon number will be added in changeweapon
	ent->s.skin = ent - g_edicts - 1;

	ent->s.frame = 0;
	VectorCopy (spawn_origin, ent->s.origin);
	ent->s.origin[2] += 1;	// make sure off ground
	VectorCopy (ent->s.origin, ent->s.old_origin);

	// set the delta angle
	for (i=0 ; i<3 ; i++)
	{
		client->ps.pmove.delta_angles[i] = ANGLE2SHORT(spawn_angles[i] - client->resp.cmd_angles[i]);
	}

	ent->s.angles[PITCH] = ent->s.angles[ROLL]  = 0;
	ent->s.angles[YAW]   = spawn_angles[YAW];
	if(spawn_landmark)
	{
		VectorCopy(spawn_viewangles, client->ps.viewangles);
//		client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
	}
	else
		VectorCopy(ent->s.angles,    client->ps.viewangles);
	VectorCopy (client->ps.viewangles, client->v_angle);

	client->resp.spectator = false;

	// DWH:
	client->flashlight = false;
	client->secs_per_frame = 0.025;		// assumed 40 fps until we know better
	client->fps_time_start = level.time;

	if (!KillBox (ent))
	{	// could't spawn in?
	}

	gi.linkentity (ent);

	if(spawn_levelchange && !client->pers.newweapon)
	{
		// we already had a weapon when the level changed... no need to bring it up
		int	i;

		client->pers.lastweapon  = client->pers.weapon;
		client->newweapon        = NULL;
		client->machinegun_shots = 0;
		i = ((client->pers.weapon->weapmodel & 0xff) << 8);
		ent->s.skin = (ent - g_edicts - 1) | i;
		if (client->pers.weapon->ammo)
			client->ammo_index = ITEM_INDEX(FindItem(client->pers.weapon->ammo));
		else
			client->ammo_index = 0;
		client->weaponstate = WEAPON_READY;
		client->ps.gunframe = 0;
		client->ps.gunindex = gi.modelindex(client->pers.weapon->view_model);
		client->ps.gunframe = spawn_gunframe;
		ent->s.frame        = spawn_modelframe;
		client->anim_end    = spawn_anim_end;
	}
	else
	{
		// force the current weapon up
		client->newweapon = client->pers.weapon;
		ChangeWeapon (ent);
	}
}

/*
=====================
ClientBeginDeathmatch

A client has just connected to the server in 
deathmatch mode, so clear everything out before starting them.
=====================
*/
void ClientBeginDeathmatch (edict_t *ent)
{
	G_InitEdict (ent);

	InitClientResp (ent->client);

	// locate ent at a spawn point
	PutClientInServer (ent);

	if (level.intermissiontime)
	{
		MoveClientToIntermission (ent);
	}
	else
	{
		// send effect
		MESSAGE_BEGIN (svc_muzzleflash);
			WRITE_SHORT (ent-g_edicts);
			WRITE_BYTE (MZ_LOGIN);
		MESSAGE_SEND (MSG_PVS, ent->s.origin, NULL);
	}

	gi.bprintf (PRINT_HIGH, "%s entered the game\n", ent->client->pers.netname);

	// make sure all view stuff is valid
	ClientEndServerFrame (ent);
}


/*
===========
ClientBegin

called when a client has finished connecting, and is ready
to be placed into the game.  This will happen every level load.
============
*/
void ClientBegin (edict_t *ent)
{
	int		i;

	ent->client = game.clients + (ent - g_edicts - 1);

	// Lazarus: Set the alias for our alternate attack
	stuffcmd(ent, "alias +attack2 attack2_on; alias -attack2 attack2_off\n");
	
	if (deathmatch->value)
	{
		ClientBeginDeathmatch (ent);
		return;
	}

	stuffcmd(ent,"alias +zoomin zoomin;alias -zoomin zoominstop\n");
	stuffcmd(ent,"alias +zoomout zoomout;alias -zoomout zoomoutstop\n");
	stuffcmd(ent,"alias +zoom zoomon;alias -zoom zoomoff\n");

	// if there is already a body waiting for us (a loadgame), just
	// take it, otherwise spawn one from scratch
	if (ent->inuse == true)
	{
		// the client has cleared the client side viewangles upon
		// connecting to the server, which is different than the
		// state when the game is saved, so we need to compensate
		// with deltaangles
		for (i=0 ; i<3 ; i++)
			ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(ent->client->ps.viewangles[i]);
	}
	else
	{
		// a spawn point will completely reinitialize the entity
		// except for the persistant data that was initialized at
		// ClientConnect() time
		G_InitEdict (ent);
		ent->classname = "player";
		InitClientResp (ent->client);
		PutClientInServer (ent);
	}

	if (level.intermissiontime)
	{
		MoveClientToIntermission (ent);
	}
	else
	{
		// send effect if in a multiplayer game
		if (game.maxclients > 1)
		{
			MESSAGE_BEGIN (svc_muzzleflash);
				WRITE_SHORT (ent-g_edicts);
				WRITE_BYTE (MZ_LOGIN);
			MESSAGE_SEND (MSG_PVS, ent->s.origin, NULL);

			gi.bprintf (PRINT_HIGH, "%s entered the game\n", ent->client->pers.netname);
		}
	}

	// make sure all view stuff is valid
	ClientEndServerFrame (ent);
}

/*
===========
ClientUserInfoChanged

called whenever the player updates a userinfo variable.

The game can override any of the settings in place
(forcing skins or names, etc) before copying it off.
============
*/
void ClientUserinfoChanged (edict_t *ent, char *userinfo)
{
	char	*s;
	int		playernum;

	// check for malformed or illegal info strings
	if (!Info_Validate(userinfo))
	{
		strcpy (userinfo, "\\name\\badinfo\\skin\\male/grunt");
	}

	// set name
	s = Info_ValueForKey (userinfo, "name");
	strncpy (ent->client->pers.netname, s, sizeof(ent->client->pers.netname)-1);

	// set spectator
	s = Info_ValueForKey (userinfo, "spectator");
	// spectators are only supported in deathmatch
	if (deathmatch->value && *s && strcmp(s, "0"))
		ent->client->pers.spectator = true;
	else
		ent->client->pers.spectator = false;

	// set skin
	s = Info_ValueForKey (userinfo, "skin");

	playernum = ent - g_edicts - 1;

	// combine name and skin into a configstring
	gi.configstring (CS_PLAYERSKINS + playernum, va("%s\\%s", ent->client->pers.netname, s) );

	// fov
	if (deathmatch->value && ((int)dmflags->value & DF_FIXED_FOV))
	{
		ent->client->ps.fov = 90;
		ent->client->original_fov = ent->client->ps.fov;
	}
	else
	{
		float	new_fov;

		new_fov = atoi(Info_ValueForKey(userinfo, "fov"));
		if (new_fov < 1)
			new_fov = 90;
		else if (new_fov > 160)
			new_fov = 160;
		if(new_fov != ent->client->original_fov) {
			ent->client->ps.fov = new_fov;
			ent->client->original_fov = new_fov;
		}
	}

	// handedness
	s = Info_ValueForKey (userinfo, "hand");
	if (strlen(s))
		ent->client->pers.hand = atoi(s);

	// save off the userinfo in case we want to check something later
	strncpy (ent->client->pers.userinfo, userinfo, sizeof(ent->client->pers.userinfo)-1);
}

/*
===========
ClientDisconnect

Called when a player drops from the server.
Will not be called between levels.
============
*/
void ClientDisconnect (edict_t *ent)
{
	int		playernum;

	if (!ent->client)
		return;

	// DWH
	ent->client->zooming = 0;
	ent->client->zoomed = false;
	// end DWH

	gi.bprintf (PRINT_HIGH, "%s disconnected\n", ent->client->pers.netname);

	// send effect
	MESSAGE_BEGIN (svc_muzzleflash);
		WRITE_SHORT (ent-g_edicts);
		WRITE_BYTE (MZ_LOGOUT);
	MESSAGE_SEND (MSG_PVS, ent->s.origin, NULL);

	gi.unlinkentity (ent);
	ent->s.modelindex = 0;
	ent->solid = SOLID_NOT;
	ent->inuse = false;
	ent->classname = "disconnected";
	ent->client->pers.connected = false;

	playernum = ent-g_edicts-1;
	gi.configstring (CS_PLAYERSKINS+playernum, "");

}


//==============================================================


edict_t	*pm_passent;

// pmove doesn't need to know about passent and contentmask
trace_t	PM_trace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	if (pm_passent->health > 0)
		return gi.trace (start, mins, maxs, end, pm_passent, MASK_PLAYERSOLID);
	else
		return gi.trace (start, mins, maxs, end, pm_passent, MASK_DEADSOLID);
}

unsigned CheckBlock (void *b, int c)
{
	int	v,i;
	v = 0;
	for (i=0 ; i<c ; i++)
		v+= ((byte *)b)[i];
	return v;
}
void PrintPmove (pmove_t *pm)
{
	unsigned	c1, c2;

	c1 = CheckBlock (&pm->s, sizeof(pm->s));
	c2 = CheckBlock (&pm->cmd, sizeof(pm->cmd));
	gi.dprintf("sv %3i:%i %i\n", pm->cmd.impulse, c1, c2);
}

// DWH
//==========================================================================
// DWH: PM_CmdScale was ripped from Q3 source
//==========================================================================
float PM_CmdScale( usercmd_t *cmd ) {
	int		max;
	float	total;
	float	scale;

	max = abs( cmd->forwardmove );
	if ( abs( cmd->sidemove ) > max ) {
		max = abs( cmd->sidemove );
	}
	if ( abs( cmd->upmove ) > max ) {
		max = abs( cmd->upmove );
	}
	if ( !max ) {
		return 0;
	}

	total = sqrt( cmd->forwardmove * cmd->forwardmove
		+ cmd->sidemove * cmd->sidemove + cmd->upmove * cmd->upmove );
	scale = max / total;

	return scale;
}
void RemovePush(edict_t *ent)
{
	ent->client->push->s.sound = 0;
	ent->client->push->activator = NULL;
	ent->client->push = NULL;
	ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
}

void ClientPushPushable(edict_t *ent)
{
	edict_t		*box = ent->client->push;
	vec_t		dist;
	vec3_t		new_origin, v, vbox;

	VectorAdd (box->absmax,box->absmin,vbox);
	VectorScale(vbox,0.5,vbox);
	if (point_infront(ent,vbox))
	{
		VectorSubtract(ent->s.origin,box->offset,new_origin);
		VectorSubtract(new_origin,box->s.origin,v);
		v[2] = 0;
		dist = VectorLength(v);
		if(dist > 8)
		{
			// func_pushable got hung up somehow. Break off contact
			RemovePush(ent);
		}
		else if(dist > 0)
		{
			if(!box->speaker) box->s.sound = box->noise_index;
			//box_walkmove( box, vectoyaw(v), dist );
		}
		else
			box->s.sound = 0;
	}
	else
		RemovePush(ent);
}
/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame.
==============
*/
void ClientThink (edict_t *ent, usercmd_t *ucmd)
{
	gclient_t	*client;
	edict_t		*other;
	edict_t		*ground;
	pmove_t		pm;
	vec_t		t;
	vec3_t		view;
	vec3_t		oldorigin, oldvelocity;
	int			i, j;
	float		ground_speed;
//	short		save_forwardmove;

	level.current_entity = ent;
	client = ent->client;
	// Lazarus: Copy latest usercmd stuff for use in other routines
	client->ucmd = *ucmd;

	VectorCopy(ent->s.origin,oldorigin);
	VectorCopy(ent->velocity,oldvelocity);
	ground = ent->groundentity;

	if(ground && (ground->movetype == MOVETYPE_PUSH) && (ground != world) && ground->turn_rider)
		ground_speed = VectorLength(ground->velocity);
	else
		ground_speed = 0;

	if( (ent->in_mud)               ||
		(ent->client->push)         ||
		(ent->vehicle)              ||
		(ent->client->spycam)       ||
		(ground_speed > 0)            )
		ent->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
	else
		ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;

	if(client->startframe == 0)
		client->startframe = level.framenum;

	client->fps_frames++;
	if(client->fps_frames >= 100) {
		client->secs_per_frame = (level.time-client->fps_time_start)/100;
		client->fps_frames = 0;
		client->fps_time_start = level.time;
		client->frame_zoomrate = zoomrate->value * client->secs_per_frame;
	}
	VectorCopy(ent->s.origin,view);
	view[2] += ent->viewheight;

	if( client->push )
	{
		// currently pushing or pulling a func_pushable
		if( (!ent->groundentity) && (ent->waterlevel==0 || client->push->waterlevel == 0 ) )
		{
			// oops, we fall down
			RemovePush(ent);
		}
		else
		{
			// Scale client velocity by mass of func_pushable
			t = VectorLength(ent->velocity);
			if( t > client->maxvelocity )
				VectorScale(ent->velocity, client->maxvelocity/t, ent->velocity);
			client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
			t = 200./client->push->mass;
			ucmd->forwardmove *= t;
			ucmd->sidemove    *= t;
		}
	}

	// INTERMISSION
	if (level.intermissiontime)
	{
		client->ps.pmove.pm_type = PM_FREEZE;
		// can exit intermission after five seconds
		if (level.time > level.intermissiontime + 5.0 
			&& (ucmd->buttons & BUTTON_ANY) )
			level.exitintermission = true;

		return;
	}

	if (ent->target_ent && (ent->target_ent->class_id == ENTITY_TARGET_MONITOR))
	{
		edict_t	*monitor = ent->target_ent;
		if(monitor->target_ent && monitor->target_ent->inuse)
		{
			if(monitor->spawnflags & 2)
			{
				VectorCopy(monitor->target_ent->s.angles,client->ps.viewangles);
			}
			else
			{
				vec3_t	dir;
				VectorSubtract(monitor->target_ent->s.origin,monitor->s.origin,dir);
				vectoangles(dir,client->ps.viewangles);
			}
		}
		else
			VectorCopy (monitor->s.angles, client->ps.viewangles);
		VectorCopy(monitor->s.origin,ent->s.origin);
		client->ps.pmove.pm_type = PM_FREEZE;
		return;
	}

// ZOOM
	if (client->zooming) {
		client->pers.hand = 2;
		if(client->zooming > 0) {
			if(client->ps.fov > 5) {
				client->ps.fov -= client->frame_zoomrate;
				if(client->ps.fov < 5)
					client->ps.fov = 5;
			} else {
				client->ps.fov = 5;
			}
			client->zoomed = true;
		} else {
			if(client->ps.fov < client->original_fov) {
				client->ps.fov += client->frame_zoomrate;
				if(client->ps.fov > client->original_fov) {
					client->ps.fov = client->original_fov;
					client->zoomed = false;
				} else
					client->zoomed = true;
			} else {
				client->ps.fov = client->original_fov;
				client->zoomed = false;
			}
		}
	}

	pm_passent = ent;

	// set up for pmove
	memset (&pm, 0, sizeof(pm));

	if (ent->movetype == MOVETYPE_NOCLIP)
		client->ps.pmove.pm_type = PM_SPECTATOR;
	else if (ent->s.modelindex != MAX_MODELS-1)
		client->ps.pmove.pm_type = PM_GIB;
	else if (ent->deadflag)
		client->ps.pmove.pm_type = PM_DEAD;
	else
		client->ps.pmove.pm_type = PM_NORMAL;
	if(level.time > ent->gravity_debounce_time)
		client->ps.pmove.gravity = sv_gravity->value;
	else
		client->ps.pmove.gravity = 0;

	pm.s = client->ps.pmove;

		for (i=0 ; i<3 ; i++)
		{
			pm.s.origin[i] = ent->s.origin[i]*8;
			pm.s.velocity[i] = ent->velocity[i]*8;
		}

		if (memcmp(&client->old_pmove, &pm.s, sizeof(pm.s)))
		{
			pm.snapinitial = true;
	//		gi.dprintf ("pmove changed!\n");
		}

		pm.cmd = *ucmd;

		pm.trace = PM_trace;	// adds default parms
		pm.pointcontents = gi.pointcontents;

		if(ent->vehicle)
			pm.s.pm_flags |= PMF_ON_GROUND;

		// perform a pmove
		gi.Pmove (&pm);

		// save results of pmove
		client->ps.pmove = pm.s;
		client->old_pmove = pm.s;

		for (i=0 ; i<3 ; i++)
		{
			ent->s.origin[i] = pm.s.origin[i]*0.125;
			ent->velocity[i] = pm.s.velocity[i]*0.125;
		}
		VectorCopy (pm.mins, ent->mins);
		VectorCopy (pm.maxs, ent->maxs);

		client->resp.cmd_angles[0] = SHORT2ANGLE(ucmd->angles[0]);
		client->resp.cmd_angles[1] = SHORT2ANGLE(ucmd->angles[1]);
		client->resp.cmd_angles[2] = SHORT2ANGLE(ucmd->angles[2]);

// MUD - "correct" Pmove physics
		if(pm.waterlevel && ent->in_mud)
		{
			vec3_t	point;
			vec3_t	end;
			
			vec3_t	deltapos, deltavel;
			float	frac;

			pm.watertype |= CONTENTS_MUD;
			ent->in_mud  = pm.waterlevel;
			VectorSubtract(ent->s.origin,oldorigin,deltapos);
			VectorSubtract(ent->velocity,oldvelocity,deltavel);
			if(pm.waterlevel == 1)
			{
				frac = MUD1BASE + MUD1AMP*sin( (float)(level.framenum%10)/10.*2*M_PI);
				ent->s.origin[0] = oldorigin[0]   + frac*deltapos[0];
				ent->s.origin[1] = oldorigin[1]   + frac*deltapos[1];
				ent->s.origin[2] = oldorigin[2]   + 0.75*deltapos[2];
				ent->velocity[0] = oldvelocity[0] + frac*deltavel[0];
				ent->velocity[1] = oldvelocity[1] + frac*deltavel[1];
				ent->velocity[2] = oldvelocity[2] + 0.75*deltavel[2];
			}
			else if(pm.waterlevel == 2)
			{
				trace_t	tr;
				float	dist;
				
				VectorCopy(oldorigin,point);
				point[2] += ent->maxs[2];
				end[0] = point[0]; end[1] = point[1]; end[2] = oldorigin[2] + ent->mins[2];
				tr = gi.trace(point,NULL,NULL,end,ent,CONTENTS_WATER);
				dist = point[2] - tr.endpos[2];
				// frac = waterlevel 1 frac at dist=32 or more,
				//      = waterlevel 3 frac at dist=10 or less
				if(dist <= 10)
					frac = MUD3;
				else
					frac = MUD3 + (dist-10)/22.*(MUD1BASE-MUD3);
				ent->s.origin[0] = oldorigin[0]   + frac*deltapos[0];
				ent->s.origin[1] = oldorigin[1]   + frac*deltapos[1];
				ent->s.origin[2] = oldorigin[2]   + frac*deltapos[2];
				ent->velocity[0] = oldvelocity[0] + frac*deltavel[0];
				ent->velocity[1] = oldvelocity[1] + frac*deltavel[1];
				ent->velocity[2] = oldvelocity[2] + frac*deltavel[2];
				if(!ent->groundentity)
				{
					// Player can't possibly move up
					ent->s.origin[2] = min(oldorigin[2], ent->s.origin[2]);
					ent->velocity[2] = min(oldvelocity[2],ent->velocity[2]);
					ent->velocity[2] = min(-10,ent->velocity[2]);
				}
			}
			else
			{
				ent->s.origin[0] = oldorigin[0]   + MUD3*deltapos[0];
				ent->s.origin[1] = oldorigin[1]   + MUD3*deltapos[1];
				ent->velocity[0] = oldvelocity[0] + MUD3*deltavel[0];
				ent->velocity[1] = oldvelocity[1] + MUD3*deltavel[1];
				if(ent->groundentity)
				{
					ent->s.origin[2] = oldorigin[2]   + MUD3*deltapos[2];
					ent->velocity[2] = oldvelocity[2] + MUD3*deltavel[2];
				}
				else
				{
					ent->s.origin[2] = min(oldorigin[2],ent->s.origin[2]);
					ent->velocity[2] = min(oldvelocity[2], 0);
				}
			}
		}
		else
			ent->in_mud = 0;
// end MUD

		if (ent->groundentity && !pm.groundentity && (pm.cmd.upmove >= 10) && (pm.waterlevel == 0))
		{
			gi.sound(ent, CHAN_VOICE, gi.soundindex("*jump1.wav"), 1, ATTN_NORM, 0);
			PlayerNoise(ent, ent->s.origin, PNOISE_SELF);
			// Lazarus: temporarily match velocities with entity we just
			//          jumped from
			VectorAdd(ent->groundentity->velocity,ent->velocity,ent->velocity);
		}

		if (ent->groundentity && !pm.groundentity && (pm.cmd.upmove >= 10) && (pm.waterlevel == 0))
			ent->client->jumping = 1;

		if (ent->deadflag != DEAD_FROZEN)
			ent->viewheight = pm.viewheight;
		ent->waterlevel = pm.waterlevel;
		ent->watertype = pm.watertype;
		ent->groundentity = pm.groundentity;
		if (pm.groundentity)
			ent->groundentity_linkcount = pm.groundentity->linkcount;

		// Lazarus - lie about ground when driving a vehicle.
		//           Pmove apparently doesn't think the ground
		//           can be "owned"
		if (ent->vehicle && !ent->groundentity)
		{
			ent->groundentity = ent->vehicle;
			ent->groundentity_linkcount = ent->vehicle->linkcount;
		}


		if (ent->deadflag)
		{
			if (ent->deadflag != DEAD_FROZEN)
			{
				client->ps.viewangles[ROLL] = 40;
				client->ps.viewangles[PITCH] = -15;
				client->ps.viewangles[YAW] = client->killer_yaw;
			}
		}
		else
		{
			VectorCopy (pm.viewangles, client->v_angle);
			VectorCopy (pm.viewangles, client->ps.viewangles);
		}

		gi.linkentity (ent);

		if (ent->movetype != MOVETYPE_NOCLIP)
			G_TouchTriggers (ent);

		if( (world->effects & FX_WORLDSPAWN_JUMPKICK) && (ent->client->jumping) && (ent->solid != SOLID_NOT))
			kick_attack(ent);

		// touch other objects
		// Lazarus: but NOT if game is frozen
		if(!level.freeze)
		{
			for (i=0 ; i<pm.numtouch ; i++)
			{
				other = pm.touchents[i];
				for (j=0 ; j<i ; j++)
					if (pm.touchents[j] == other)
						break;
					if (j != i)
						continue;	// duplicated
					if (!other->touch)
						continue;
					other->touch (other, ent, NULL, NULL);
			}
		}

	client->oldbuttons = client->buttons;
	client->buttons = ucmd->buttons;
	client->latched_buttons |= client->buttons & ~client->oldbuttons;

	// save light level the player is standing on for
	// monster sighting AI
	ent->light_level = ucmd->lightlevel;

	// fire weapon from final position if needed
	if (client->latched_buttons & BUTTONS_ATTACK)
	{
		if (client->resp.spectator) {

			client->latched_buttons = 0;

		} else if (!client->weapon_thunk) {
			client->weapon_thunk = true;
			Think_Weapon (ent);
		}
	}

	if (client->resp.spectator) {
		if (ucmd->upmove >= 10) {
			if (!(client->ps.pmove.pm_flags & PMF_JUMP_HELD)) {
				client->ps.pmove.pm_flags |= PMF_JUMP_HELD;
			}
		} else
			client->ps.pmove.pm_flags &= ~PMF_JUMP_HELD;
	}

	if(client->push != NULL)
	{
		client->push->s.sound = 0;
	}

}


/*
==============
ClientBeginServerFrame

This will be called once for each server frame, before running
any other entities in the world.
==============
*/
void ClientBeginServerFrame (edict_t *ent)
{
	gclient_t	*client;
	int			buttonMask;

	if (level.intermissiontime)
		return;

	client = ent->client;

	// DWH
	if(client->spycam)
		client = client->camplayer->client;

	if (deathmatch->value &&
		client->pers.spectator != client->resp.spectator &&
		(level.time - client->respawn_time) >= 5) {
		spectator_respawn(ent);
		return;
	}

	// run weapon animations if it hasn't been done by a ucmd_t
	if (!client->weapon_thunk && !client->resp.spectator)
		Think_Weapon (ent);
	else
		client->weapon_thunk = false;

	if (ent->deadflag)
	{
		// wait for any button just going down
		if ( level.time > client->respawn_time)
		{
			// in deathmatch, only wait for attack button
			if (deathmatch->value)
				buttonMask = BUTTONS_ATTACK;
			else
				buttonMask = -1;

			if ( ( client->latched_buttons & buttonMask ) ||
				(deathmatch->value && ((int)dmflags->value & DF_FORCE_RESPAWN) ) )
			{
				respawn(ent);
				client->latched_buttons = 0;
			}
		}
		return;
	}
	client->latched_buttons = 0;
}

