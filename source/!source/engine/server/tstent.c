void SP_info_player_start (edict_t *ent){}
void SP_info_player_deathmatch (edict_t *ent){}
void SP_worldspawn (edict_t *ent)
{
	vec3_t		skyaxis;

	ent->priv.sv->movetype = MOVETYPE_PUSH;
	ent->priv.sv->solid = SOLID_BSP;
	ent->priv.sv->free = false;			// since the world doesn't use G_Spawn()
	ent->priv.sv->s.modelindex = 1;		// world model is always index 1

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
	self->priv.sv->solid = SOLID_BBOX;
	self->priv.sv->movetype = MOVETYPE_STEP;

	self->priv.sv->model = "models/barrel.mdl";
	self->priv.sv->s.modelindex = SV_ModelIndex (self->priv.sv->model);
	VectorSet (self->priv.sv->mins, -16, -16, 0);
	VectorSet (self->priv.sv->maxs, 16, 16, 40);

	if (!self->priv.sv->health) self->priv.sv->health = 10;
	self->priv.sv->monsterinfo.aiflags = AI_NOSTEP;

	self->priv.sv->think = SV_DropToFloor;
	self->priv.sv->nextthink = sv.time + 0.5;
	
	PF_setmodel (self, self->priv.sv->model);
}

void SP_func_wall(edict_t *self)
{
	self->priv.sv->solid = SOLID_BSP;
	PF_setmodel (self, self->priv.sv->model);
	SV_LinkEdict(self);
}