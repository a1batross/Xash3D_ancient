#include "engine.h"
#include "server.h"

#define	FOFS(x) (int)&(((edict_t *)0)->x)
#define	FFL_NOSPAWN		2

edict_t	*pm_passent;

char *single_statusbar = 
"yb -24 "

// health
"xv 0 "
"hnum "
"xv 50 "
"pic 0 "

// ammo
"if 2 "
"{ xv 100 "
"anum "
"xv 150 "
"pic 2 "
"} "

// armor
"if 4 "
"{ xv 200 "
"rnum "
"xv 250 "
"pic 4 "
"} "

// selected item
"if 6 "
"{ xv 296 "
"pic 6 "
"} "

"yb -50 "

// picked up item
"if 7 "
"{ xv 0 "
"pic 7 "
"xv 26 "
"yb -42 "
"stat_string 8 "
"yb -50 "
"} "

// timer (was xv 262)
"if 9 "
"{ xv 230 "
"num 4 10 "
"xv 296 "
"pic 9 "
"} "

//  help / weapon icon 
"if 11 "
"{ xv 148 "
"pic 11 "
"} "

// vehicle speed
"if 22 "
"{ yb -90 "
"xv 128 "
"pic 22 "
"} "

// zoom
"if 23 "
"{ yv 0 "
"xv 0 "
"pic 23 "
"} "
;


void SP_info_player_start (edict_t *ent){}
void SP_info_player_deathmatch (edict_t *ent){}
void SP_worldspawn (edict_t *ent)
{
	vec3_t		skyaxis;

	ent->movetype = MOVETYPE_PUSH;
	ent->solid = SOLID_BSP;
	ent->inuse = true;			// since the world doesn't use G_Spawn()
	ent->s.modelindex = 1;		// world model is always index 1

	//---------------

	VectorSet( skyaxis, 32, 180, 20 );

	PF_Configstring (CS_MAXCLIENTS, va("%i", (int)(maxclients->value)));
	PF_Configstring (CS_STATUSBAR, single_statusbar);
	PF_Configstring (CS_SKY, "sky" );
	PF_Configstring (CS_SKYROTATE, va("%f", 0.0f ));
	PF_Configstring (CS_SKYAXIS, va("%f %f %f", skyaxis[0], skyaxis[1], skyaxis[2]) );
	PF_Configstring (CS_CDTRACK, va("%i", 0 ));

	//---------------

	// help icon for statusbar
	SV_ImageIndex ("i_help");
	SV_ImageIndex ("help");
	SV_ImageIndex ("field_3");
}

void SP_misc_explobox (edict_t *self)
{
	self->solid = SOLID_BBOX;
	self->movetype = MOVETYPE_STEP;

	self->model = "models/barrel.mdl";
	self->s.modelindex = SV_ModelIndex (self->model);
	VectorSet (self->mins, -16, -16, 0);
	VectorSet (self->maxs, 16, 16, 40);

	if (!self->health) self->health = 10;
	self->monsterinfo.aiflags = AI_NOSTEP;

	self->think = SV_DropToFloor;
	self->nextthink = sv.time + 0.5;
	
	PF_setmodel (self, self->model);
}


spawn_t	spawns[] =
{
	{"info_player_start", SP_info_player_start},
	{"info_player_deathmatch", SP_info_player_deathmatch},


	{"misc_explobox", SP_misc_explobox},
	{"worldspawn", SP_worldspawn},
	{NULL, NULL}
};

field_t	fields[] = 
{
	{"classname", FOFS(classname), F_LSTRING},
	{"model", FOFS(model), F_LSTRING},
	{"spawnflags", FOFS(spawnflags), F_INT},
	{"health", FOFS(health), F_INT},
	{"origin", FOFS(s.origin), F_VECTOR},
	{"angles", FOFS(s.angles), F_VECTOR},
	{"angle", FOFS(s.angles), F_ANGLEHACK},

	{"owner", FOFS(owner), F_EDICT, FFL_NOSPAWN},

	{"prethink", FOFS(prethink), F_FUNCTION, FFL_NOSPAWN},
	{"think", FOFS(think), F_FUNCTION, FFL_NOSPAWN},
	{"blocked", FOFS(blocked), F_FUNCTION, FFL_NOSPAWN},
	{"touch", FOFS(touch), F_FUNCTION, FFL_NOSPAWN},
	{"use", FOFS(use), F_FUNCTION, FFL_NOSPAWN},

	{"skin", FOFS(s.skin), F_INT},
	{"body", FOFS(s.body), F_INT},

	{0, 0, F_INT, 0}

};

// pmove doesn't need to know about passent and contentmask
trace_t PM_trace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	if (pm_passent->health > 0)
		return SV_Trace (start, mins, maxs, end, pm_passent, MASK_PLAYERSOLID);
	return SV_Trace (start, mins, maxs, end, pm_passent, MASK_DEADSOLID);
}

/*
===============
ED_CallSpawn

Finds the spawn function for the entity and calls it
===============
*/
void ED_CallSpawn (edict_t *ent)
{
	spawn_t		*s;

	if (!ent->classname)
	{
		Msg("ED_CallSpawn: NULL classname\n");
		SV_FreeEdict(ent);
		return;
	}

	// check normal spawn functions
	for (s = spawns; s->name; s++)
	{
		if (!strcmp(s->name, ent->classname))
		{	
			// found it
			s->spawn (ent);
			return;
		}
	}

	Msg("%s doesn't have a spawn function\n", ent->classname);
	SV_FreeEdict(ent);
}

/*
=============
ED_NewString
=============
*/
char *ED_NewString (const char *string)
{
	char	*newb, *new_p;
	int	i, l;
	
	l = strlen(string) + 1;

	newb = (char *)Z_Malloc(l);

	new_p = newb;

	for (i = 0; i < l; i++)
	{
		if (string[i] == '\\' && i < l-1)
		{
			i++;
			if (string[i] == 'n') 
				*new_p++ = '\n';
			else *new_p++ = '\\';
		}
		else *new_p++ = string[i];
	}
	return newb;
}

/*
===============
ED_ParseField

Takes a key/value pair and sets the binary values
in an edict
===============
*/
void ED_ParseField (char *key, const char *value, edict_t *ent)
{
	field_t	*f;
	byte	*b;
	float	v;
	vec3_t	vec;

	for (f = fields; f->name; f++)
	{
		if (!(f->flags & FFL_NOSPAWN) && !strcasecmp(f->name, key))
		{	
			// found it
			b = (byte *)ent;

			switch (f->type)
			{
			case F_LSTRING:
				*(char **)(b+f->ofs) = ED_NewString (value);
				break;
			case F_VECTOR:
				sscanf (value, "%f %f %f", &vec[0], &vec[1], &vec[2]);
				((float *)(b+f->ofs))[0] = vec[0];
				((float *)(b+f->ofs))[1] = vec[1];
				((float *)(b+f->ofs))[2] = vec[2];
				break;
			case F_INT:
				*(int *)(b+f->ofs) = atoi(value);
				break;
			case F_FLOAT:
				*(float *)(b+f->ofs) = atof(value);
				break;
			case F_ANGLEHACK:
				v = atof(value);
				((float *)(b+f->ofs))[0] = 0;
				((float *)(b+f->ofs))[1] = v;
				((float *)(b+f->ofs))[2] = 0;
				break;
			case F_IGNORE:
				break;
			}
			return;
		}
	}
	Msg("%s is not a field\n", key);
}

/*
====================
ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
====================
*/
char *ED_ParseEdict (char *data, edict_t *ent)
{
	bool		init;
	char		keyname[256];
	char		*com_token;

	init = false;

	// go through all the dictionary pairs
	while (1)
	{	
		// parse key
		com_token = COM_Parse (&data);
		if (com_token[0] == '}') break;
		if (!data) PF_error ("ED_ParseEntity: EOF without closing brace");

		strncpy (keyname, com_token, sizeof(keyname)-1);
		
		// parse value	
		com_token = COM_Parse (&data);
		if (!data) PF_error ("ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			PF_error ("ED_ParseEntity: closing brace without data");

		init = true;	

		// keynames with a leading underscore are used for utility comments,
		// and are immediately discarded by quake
		if (keyname[0] == '_') continue;

		ED_ParseField (keyname, com_token, ent);
	}

	if (!init) memset (ent, 0, sizeof(*ent));

	return (char *)data;
}


/*
==============
SpawnEntities

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.
==============
*/
void SV_SpawnEntities (char *mapname, char *entities, char *spawnpoint)
{
	edict_t		*ent;
	int		inhibit;
	char		*com_token;
	int		i;

	Msg("====== SpawnEntities ========\n");

	memset (ge->edicts, 0, game.maxentities * sizeof(edict_t));

	// set client fields on player ents
	for (i = 0; i < game.maxclients; i++)
		ge->edicts[i+1].client = game.clients + i;

	ent = NULL;
	inhibit = 0;

	// parse ents
	while (1)
	{
		// parse the opening brace	
		com_token = COM_Parse (&entities);
		if (!entities) break;
		if (com_token[0] != '{')
			PF_error("ED_LoadFromFile: found %s when expecting {",com_token);

		if (!ent) ent = ge->edicts;
		else ent = SV_Spawn ();
		entities = ED_ParseEdict (entities, ent);

		ED_CallSpawn (ent);
		ent->s.renderfx |= RF_IR_VISIBLE;		//PGM
	}	

	Msg("%i entities inhibited\n", inhibit);
}

/*
=================
SV_Spawn

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
edict_t *SV_Spawn (void)
{
	int			i;
	edict_t		*e;

	e = EDICT_NUM((int)maxclients->value + 1);
	for ( i = maxclients->value + 1; i < ge->num_edicts; i++, e++)
	{
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (!e->inuse)
		{
			SV_InitEdict (e);
			return e;
		}
	}

	if (i == game.maxentities) PF_error ("ED_Alloc: no free edicts");

	ge->num_edicts++;

	SV_InitEdict (e);
	return e;
}

void SV_InitEdict (edict_t *e)
{
	e->inuse = true;
	e->classname = "noclass";
	e->s.number = NUM_FOR_EDICT(e);
}


/*
=================
SV_FreeEdict

Marks the edict as free
=================
*/
void SV_FreeEdict (edict_t *ed)
{
	SV_UnlinkEdict(ed);	// unlink from world

	if (NUM_FOR_EDICT(ed) <= maxclients->value)
		return;

	memset (ed, 0, sizeof(*ed));
	ed->classname = "freed";
	ed->freetime = sv.time;
	ed->inuse = false;
}

/*
============
SV_TouchTriggers

============
*/
void SV_TouchTriggers (edict_t *ent)
{
	int		i, num;
	edict_t		*touch[MAX_EDICTS], *hit;

	// dead things don't activate triggers!
	if ((ent->client || (ent->svflags & SVF_MONSTER)) && (ent->health <= 0))
		return;

	num = SV_AreaEdicts(ent->absmin, ent->absmax, touch, MAX_EDICTS, AREA_TRIGGERS);

	// be careful, it is possible to have an entity in this
	// list removed before we get to it (killtriggered)
	for (i = 0; i < num; i++)
	{
		hit = touch[i];
		if (!hit->inuse) continue;
		if (!hit->touch) continue;
		hit->touch (hit, ent, NULL, NULL);
	}
}

/*
=============
SV_Find

Searches all active entities for the next one that holds
the matching string at fieldofs (use the FOFS() macro) in the structure.

Searches beginning at the edict after from, or the beginning if NULL
NULL will be returned if the end of the list is reached.

=============
*/
edict_t *SV_Find (edict_t *from, int fieldofs, char *match)
{
	char		*s;
	int		i;
	edict_t		*ent;

	if (!from) i = NUM_FOR_EDICT(ge->edicts);
	else i = NUM_FOR_EDICT(from);

	for (; i < ge->num_edicts; i++)
	{
		ent = EDICT_NUM(i);

		if(!ent->inuse) continue;

		s = *(char **)((byte *)ent + fieldofs);
		if (!s) continue;
		if (!strcasecmp (s, match))
			return ent;
	}
	return NULL;
}


/*
===========
SelectSpawnPoint

Chooses a player start, deathmatch start, coop start, etc
============
*/
void SV_SelectSpawnPoint (edict_t *ent, vec3_t origin, vec3_t angles)
{
	edict_t	*spot = NULL;

	spot = SV_Find (spot, FOFS(classname), "info_player_start");
	if (!spot) PF_error ("Couldn't find spawn point\n");

	VectorCopy (spot->s.origin, origin);
	origin[2] += 9;
	VectorCopy (spot->s.angles, angles);
}


/*
===========
PutClientInServer

Called when a player connects to a server or respawns in
a deathmatch.
============
*/
void SV_PutClientInServer (edict_t *ent)
{
	vec3_t		mins = {-16, -16, -24};
	vec3_t		maxs = {16, 16, 32};
	int		index;
	vec3_t		spawn_origin, spawn_angles;
	gclient_t		*client;
	int		i;

	// find a spawn point
	// do it before setting health back up, so farthest
	// ranging doesn't count this client
	SV_SelectSpawnPoint (ent, spawn_origin, spawn_angles);

	index = NUM_FOR_EDICT(ent) - 1;
	
	client = ent->client;

	ent->client = &game.clients[index];
	ent->movetype = MOVETYPE_WALK;
	ent->inuse = true;
	ent->classname = "player";
	ent->solid = SOLID_BBOX;
	ent->model = "models/player.mdl";
	ent->svflags &= ~SVF_DEADMONSTER;

	VectorCopy (mins, ent->mins);
	VectorCopy (maxs, ent->maxs);
	VectorClear (ent->velocity);

	// clear playerstate values
	memset (&ent->client->ps, 0, sizeof(client->ps));

	client->ps.pmove.origin[0] = spawn_origin[0] * 8;
	client->ps.pmove.origin[1] = spawn_origin[1] * 8;
	client->ps.pmove.origin[2] = spawn_origin[2] * 8;

	client->ps.fov = 90;

	client->ps.fov = bound(1, client->ps.fov, 160);
	client->ps.gunindex = SV_ModelIndex("models/weapons/v_eagle.mdl");

	// clear entity state values
	ent->s.effects = 0;
	ent->s.modelindex = MAX_MODELS - 1;	// will use the skin specified model
	ent->s.weaponmodel = MAX_MODELS - 1;	// custom gun model

	// sknum is player num and weapon number
	// weapon number will be added in changeweapon
	ent->s.skin = NUM_FOR_EDICT(ent) - 1;

	ent->s.frame = 0;
	VectorCopy (spawn_origin, ent->s.origin);
	ent->s.origin[2] += 1;	// make sure off ground
	VectorCopy (ent->s.origin, ent->s.old_origin);

	// set the delta angle
	for (i=0 ; i<3 ; i++)
	{
		client->ps.pmove.delta_angles[i] = ANGLE2SHORT(spawn_angles[i]);
	}

	ent->s.angles[PITCH] = ent->s.angles[ROLL]  = 0;
	ent->s.angles[YAW] = spawn_angles[YAW];
	VectorCopy(ent->s.angles, client->ps.viewangles);
	VectorCopy (client->ps.viewangles, client->v_angle);

	SV_LinkEdict(ent);
}

static edict_t	*current_player;
static gclient_t	*current_client;
static vec3_t	forward, right, up;
float		xyspeed, bobmove, bobfracsin;	// sin(bobfrac*M_PI)
int		bobcycle;			// odd cycles are right foot going forward


/*
===============
SV_CalcRoll

===============
*/
float SV_CalcRoll (vec3_t angles, vec3_t velocity)
{
	float	sign;
	float	side;
	float	value;
	
	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs(side);
	
	value = 2;

	if (side < 200) side = side * value / 200;
	else side = value;
	
	return side*sign;
	
}

/*
==============
SV_CalcGunOffset
==============
*/
void SV_CalcGunOffset (edict_t *ent)
{
	int		i;
	float		delta;

	// gun angles from bobbing
	ent->client->ps.gunangles[ROLL] = xyspeed * bobfracsin * 0.005;
	ent->client->ps.gunangles[YAW] = xyspeed * bobfracsin * 0.01;
	if (bobcycle & 1)
	{
		ent->client->ps.gunangles[ROLL] = -ent->client->ps.gunangles[ROLL];
		ent->client->ps.gunangles[YAW] = -ent->client->ps.gunangles[YAW];
	}

	ent->client->ps.gunangles[PITCH] = xyspeed * bobfracsin * 0.005;

	// gun angles from delta movement
	for (i=0 ; i<3 ; i++)
	{
		delta = ent->client->oldviewangles[i] - ent->client->ps.viewangles[i];
		if (delta > 180) delta -= 360;
		if (delta < -180) delta += 360;
		if (delta > 45) delta = 45;
		if (delta < -45) delta = -45;
		if (i == YAW) ent->client->ps.gunangles[ROLL] += 0.1*delta;
		ent->client->ps.gunangles[i] += 0.2 * delta;
	}

	// gun height
	VectorClear (ent->client->ps.gunoffset);

	// gun_x / gun_y / gun_z are development tools
	for (i = 0; i < 3; i++)
	{
		ent->client->ps.gunoffset[i] += forward[i];
		ent->client->ps.gunoffset[i] += right[i];
		ent->client->ps.gunoffset[i] += up[i];
	}
}


void SV_CalcViewOffset (edict_t *ent)
{
	float		*angles;
	float		bob;
	float		delta;
	vec3_t		v;

	// base angles
	angles = ent->client->ps.kick_angles;


	// add angles based on velocity
	delta = DotProduct (ent->velocity, forward);
	angles[PITCH] += delta * 0.002;
		
	delta = DotProduct (ent->velocity, right);
	angles[ROLL] += delta * 0.005;

	// add angles based on bob
	delta = bobfracsin * 0.002 * xyspeed;
	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
		delta *= 6; // crouching
	angles[PITCH] += delta;
	delta = bobfracsin * 0.002 * xyspeed;

	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
		delta *= 6; // crouching
	if (bobcycle & 1) delta = -delta;
	angles[ROLL] += delta;


	// base origin
	VectorClear (v);

	// add view height
	v[2] += 22;

	// add bob height
	bob = bobfracsin * xyspeed * 0.005;
	if (bob > 6) bob = 6;
	v[2] += bob;


	v[0] = bound(-14, v[0], 14);
	v[1] = bound(-14, v[1], 14);
	v[2] = bound(-22, v[0], 30);

	VectorCopy (v, ent->client->ps.viewoffset);
}

void SV_SetStats (edict_t *ent)
{
	ent->client->ps.stats[STAT_HEALTH_ICON] = SV_ImageIndex("i_health");
	ent->client->ps.stats[STAT_HEALTH] = ent->health;
}

void ClientEndServerFrame (edict_t *ent)
{
	float		bobtime;
	int		i;

	current_player = ent;
	current_client = ent->client;
	
	//
	// If the origin or velocity have changed since ClientThink(),
	// update the pmove values.  This will happen when the client
	// is pushed by a bmodel or kicked by an explosion.
	// 
	// If it wasn't updated here, the view position would lag a frame
	// behind the body position when pushed -- "sinking into plats"
	//
	for (i = 0; i < 3; i++)
	{
		current_client->ps.pmove.origin[i] = ent->s.origin[i]*8.0;
		current_client->ps.pmove.velocity[i] = ent->velocity[i]*8.0;
	}

	AngleVectors (ent->client->v_angle, forward, right, up);

	//
	// set model angles from view angles so other things in
	// the world can tell which direction you are looking
	//
	if (ent->client->v_angle[PITCH] > 180) ent->s.angles[PITCH] = (-360 + ent->client->v_angle[PITCH])/3;
	else ent->s.angles[PITCH] = ent->client->v_angle[PITCH]/3;

	ent->s.angles[YAW] = ent->client->v_angle[YAW];
	ent->s.angles[ROLL] = 0;
	ent->s.angles[ROLL] = SV_CalcRoll (ent->s.angles, ent->velocity)*4;

	//
	// calculate speed and cycle to be used for
	// all cyclic walking effects
	//
	xyspeed = sqrt(ent->velocity[0] * ent->velocity[0] + ent->velocity[1] * ent->velocity[1]);

	if (xyspeed < 5)
	{
		bobmove = 0;
		current_client->bobtime = 0;	// start at beginning of cycle again
	}
	else
	{	
		// so bobbing only cycles when on ground
		if (xyspeed > 210) bobmove = 0.25;
		else if (xyspeed > 100) bobmove = 0.125;
		else bobmove = 0.0625;
	}
	
	bobtime = (current_client->bobtime += bobmove);

	if (current_client->ps.pmove.pm_flags & PMF_DUCKED)
		bobtime *= 4;

	bobcycle = (int)bobtime;
	bobfracsin = fabs(sin(bobtime*M_PI));

	// determine the view offsets
	SV_CalcViewOffset (ent);

	// determine the gun offsets
	SV_CalcGunOffset (ent);

	SV_SetStats( ent );
}

/*
=================
ClientEndServerFrames
=================
*/
void ClientEndServerFrames (void)
{
	int		i;
	edict_t	*ent;

	// calc the player views now that all pushing
	// and damage has been added
	for (i = 1; i < maxclients->value; i++)
	{
		ent = EDICT_NUM(i);
		if (!ent->inuse || !ent->client)
			continue;
		ClientEndServerFrame (ent);
	}
}

/*
================
SV_RunFrame

Advances the world by 0.1 seconds
================
*/
void SV_RunFrame (void)
{
	int		i;
	edict_t		*ent;

	//
	// treat each object in turn
	// even the world gets a chance to think
	//

	ent = EDICT_NUM(0);
	for (i = 0; i < ge->num_edicts; i++, ent++)
	{
		if (!ent->inuse) continue;

		VectorCopy (ent->s.origin, ent->s.old_origin);

		if (i > 0 && i <= maxclients->value)
			continue; //don't apply phys on clients
		SV_Physics(ent);
	}

	// build the playerstate_t structures for all players
	ClientEndServerFrames ();
}

bool SV_ClientConnect (edict_t *ent, char *userinfo)
{
	// they can connect
	ent->client = game.clients + NUM_FOR_EDICT(ent) - 1;
	ent->svflags = 0; // make sure we start with known default
	ent->health = 100;

	return true;
}

void SV_ClientUserinfoChanged (edict_t *ent, char *userinfo)
{
	char	*s;
	int	playernum;

	// check for malformed or illegal info strings
	if (!Info_Validate(userinfo))
	{
		strcpy (userinfo, "\\name\\badinfo\\skin\\male/grunt");
	}

	// set skin
	s = Info_ValueForKey (userinfo, "skin");

	playernum = NUM_FOR_EDICT(ent) - 1;

	// combine name and skin into a configstring
	PF_Configstring (CS_PLAYERSKINS + playernum, va("%s\\%s", Info_ValueForKey (userinfo, "name"), Info_ValueForKey (userinfo, "skin")));
		
	ent->client->ps.fov = bound(1, atoi(Info_ValueForKey(userinfo, "fov")), 160);
}

/*
===========
SV_ClientBegin

called when a client has finished connecting, and is ready
to be placed into the game.  This will happen every level load.
============
*/
void SV_ClientBegin (edict_t *ent)
{
	int		i;

	ent->client = game.clients + NUM_FOR_EDICT(ent) - 1;

	// if there is already a body waiting for us (a loadgame), just
	// take it, otherwise spawn one from scratch
	if (ent->inuse == true)
	{
		// the client has cleared the client side viewangles upon
		// connecting to the server, which is different than the
		// state when the game is saved, so we need to compensate
		// with deltaangles
		for (i = 0; i < 3; i++)
			ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(ent->client->ps.viewangles[i]);
	}
	else
	{
		// a spawn point will completely reinitialize the entity
		// except for the persistant data that was initialized at
		// ClientConnect() time
		SV_InitEdict (ent);
		ent->classname = "player";
		SV_PutClientInServer (ent);
	}

	// make sure all view stuff is valid
	ClientEndServerFrame (ent);
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
	gclient_t		*client;
	edict_t		*other;
	pmove_t		pm;
	vec3_t		view;
	vec3_t		oldorigin, oldvelocity;
	int		i, j;

	client = ent->client;

	VectorCopy(ent->s.origin, oldorigin);
	VectorCopy(ent->velocity, oldvelocity);

	ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;

	VectorCopy(ent->s.origin,view);
	view[2] += 22;

	pm_passent = ent;

	// set up for pmove
	memset (&pm, 0, sizeof(pm));

	if (ent->movetype == MOVETYPE_NOCLIP) client->ps.pmove.pm_type = PM_SPECTATOR;
	else if (ent->s.modelindex != MAX_MODELS - 1) client->ps.pmove.pm_type = PM_GIB;
	else client->ps.pmove.pm_type = PM_NORMAL;
	client->ps.pmove.gravity = 0;

	pm.s = client->ps.pmove;

	for (i = 0; i < 3; i++)
	{
		pm.s.origin[i] = ent->s.origin[i]*8;
		pm.s.velocity[i] = ent->velocity[i]*8;
	}

	if (memcmp(&client->old_pmove, &pm.s, sizeof(pm.s)))
		pm.snapinitial = true;

	pm.cmd = *ucmd;

	pm.trace = PM_trace; // adds default parms
	pm.pointcontents = SV_PointContents;

	// perform a pmove
	Pmove (&pm);

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

	SV_LinkEdict(ent);

	if (ent->movetype != MOVETYPE_NOCLIP)
		SV_TouchTriggers (ent);

	for (i = 0; i < pm.numtouch; i++)
	{
		other = pm.touchents[i];
		for (j = 0; j < i; j++)
		{
			if (pm.touchents[j] == other)
				break;
		}
		if (j != i) continue;	// duplicated
		if (!other->touch) continue;
		other->touch (other, ent, NULL, NULL);
	}
}

/*
===========
SV_ClientDisconnect

Called when a player drops from the server.
Will not be called between levels.
============
*/
void SV_ClientDisconnect (edict_t *ent)
{
	int	playernum;

	if (!ent->client) return;

	Msg("player disconnected\n");

	// send effect
	MSG_Begin( svc_muzzleflash );
		MSG_WriteShort( &sv.multicast, NUM_FOR_EDICT(ent) );
		MSG_WriteByte( &sv.multicast, MZ_LOGOUT );
	MSG_Send(MSG_PVS, ent->s.origin, NULL);

	SV_UnlinkEdict(ent);
	ent->s.modelindex = 0;
	ent->solid = SOLID_NOT;
	ent->inuse = false;
	ent->classname = "disconnected";

	playernum = NUM_FOR_EDICT(ent) - 1;
	PF_Configstring (CS_PLAYERSKINS + playernum, "");

}

/*
==================
Cmd_Say_f
==================
*/
void Cmd_Say_f (edict_t *ent, bool team, bool arg0)
{
	int		j;
	edict_t		*other;
	char		*p;
	char		text[2048];

	if (Cmd_Argc () < 2 && !arg0) return;

	sprintf (text, "%s: ", "all");

	if (arg0)
	{
		strcat (text, Cmd_Argv(0));
		strcat (text, " ");
		strcat (text, Cmd_Args());
	}
	else
	{
		p = Cmd_Args();

		if (*p == '"')
		{
			p++;
			p[strlen(p)-1] = 0;
		}
		strcat(text, p);
	}

	// don't let text be too long for malicious reasons
	if (strlen(text) > 150) text[150] = 0;

	strcat(text, "\n");

	if (dedicated->value)
		PF_cprintf(NULL, PRINT_CHAT, "%s", text);

	for (j = 1; j <= game.maxclients; j++)
	{
		other = EDICT_NUM(j);
		if (!other->inuse) continue;
		if (!other->client) continue;
		PF_cprintf(other, PRINT_CHAT, "%s", text);
	}
}

/*
==================
HelpComputer

Draw help computer.
==================
*/
void SV_HelpComputer (edict_t *ent)
{
	char	string[1024];
	char	*sk = "medium";

	sprintf (string, "xv 32 yv 8 picn help "	// background
		"xv 202 yv 12 string2 \"%s\" "	// skill
		"xv 0 yv 24 cstring2 \"%s\" "		// level name
		"xv 0 yv 54 cstring2 \"%s\" "		// help 1
		"xv 0 yv 110 cstring2 \"%s\" "	// help 2
		"xv 50 yv 164 string2 \" kills     goals    secrets\" "
		"xv 50 yv 172 string2 \"%3i/%3i     %i/%i       %i/%i\" ", 
		sk,
		sv.name,
		"",
		"",
		0, 0, 
		0, 0,
		0, 0);

	MSG_Begin (svc_layout);
		MSG_WriteString (&sv.multicast, string);
	MSG_Send (MSG_ONE_R, NULL, ent );
}

/*
==================
Cmd_Help_f

Display the current help message
==================
*/
void Cmd_Help_f (edict_t *ent)
{
	SV_HelpComputer (ent);
}

/*
=================
SV_ClientCommand
=================
*/
void SV_ClientCommand (edict_t *ent)
{
	char	*cmd;
	char	*parm;

	if (!ent->client) return; // not fully in game yet

	cmd = Cmd_Argv(0);

	if(Cmd_Argc() < 2) parm = NULL;
	else parm = Cmd_Argv(1);

	if (strcasecmp (cmd, "say") == 0)
	{
		Cmd_Say_f (ent, false, false);
		return;
	}
	if (strcasecmp (cmd, "say_team") == 0)
	{
		Cmd_Say_f (ent, true, false);
		return;
	}
	if (strcasecmp (cmd, "help") == 0)
	{
		Cmd_Help_f (ent);
		return;
	}
}  