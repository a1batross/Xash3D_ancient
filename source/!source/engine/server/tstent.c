void SP_info_player_start (edict_t *ent){}
void SP_info_player_deathmatch (edict_t *ent){}
void SP_worldspawn (edict_t *ent)
{
	// help icon for statusbar
	SV_ImageIndex ("i_help");
	SV_ImageIndex ("help");
	SV_ImageIndex ("field_3");
}

void SP_func_wall(edict_t *self)
{
	self->priv.sv->solid = SOLID_BSP;
	PF_setmodel (self, self->priv.sv->model);
	SV_LinkEdict(self);
}