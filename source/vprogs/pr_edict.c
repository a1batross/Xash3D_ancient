//=======================================================================
//			Copyright XashXT Group 2007 ©
//			pr_edict.c - vm runtime base
//=======================================================================

#include "vprogs.h"

static prvm_prog_t prog_list[PRVM_MAXPROGS];
sizebuf_t vm_tempstringsbuf;

int prvm_type_size[8] = {1, sizeof(string_t)/4,1,3,1,1, sizeof(func_t)/4, sizeof(void *)/4};

ddef_t *PRVM_ED_FieldAtOfs(int ofs);
bool PRVM_ED_ParseEpair(edict_t *ent, ddef_t *key, const char *s);

//============================================================================
// mempool handling

/*
===============
PRVM_MEM_Alloc
===============
*/
void PRVM_MEM_Alloc(void)
{
	int i;

	// reserve space for the null entity aka world
	// check bound of max_edicts
	vm.prog->max_edicts = bound(1 + vm.prog->reserved_edicts, vm.prog->max_edicts, vm.prog->limit_edicts);
	vm.prog->num_edicts = bound(1 + vm.prog->reserved_edicts, vm.prog->num_edicts, vm.prog->max_edicts);

	// edictprivate_size has to be min as big prvm_edict_private_t
	vm.prog->edictprivate_size = max(vm.prog->edictprivate_size, (int)sizeof(vm_edict_t));

	// alloc edicts
	vm.prog->edicts = (edict_t *)Mem_Alloc(vm.prog->progs_mempool, vm.prog->limit_edicts * sizeof(edict_t));

	// alloc edict private space
	vm.prog->edictprivate = Mem_Alloc(vm.prog->progs_mempool, vm.prog->max_edicts * vm.prog->edictprivate_size);

	// alloc edict fields
	vm.prog->edictsfields = Mem_Alloc(vm.prog->progs_mempool, vm.prog->max_edicts * vm.prog->edict_size);

	// set edict pointers
	for(i = 0; i < vm.prog->max_edicts; i++)
	{
		vm.prog->edicts[i].priv.ed = (vm_edict_t *)((byte *)vm.prog->edictprivate + i * vm.prog->edictprivate_size);
		vm.prog->edicts[i].progs.vp = (void*)((byte *)vm.prog->edictsfields + i * vm.prog->edict_size);
	}
}

/*
===============
PRVM_MEM_IncreaseEdicts
===============
*/
void PRVM_MEM_IncreaseEdicts( void )
{
	int	i;
	int	oldmaxedicts = vm.prog->max_edicts;
	void	*oldedictsfields = vm.prog->edictsfields;
	void	*oldedictprivate = vm.prog->edictprivate;

	if(vm.prog->max_edicts >= vm.prog->limit_edicts)
		return;

	PRVM_GCALL(begin_increase_edicts)();

	// increase edicts
	vm.prog->max_edicts = min(vm.prog->max_edicts + 256, vm.prog->limit_edicts);

	vm.prog->edictsfields = Mem_Alloc(vm.prog->progs_mempool, vm.prog->max_edicts * vm.prog->edict_size);
	vm.prog->edictprivate = Mem_Alloc(vm.prog->progs_mempool, vm.prog->max_edicts * vm.prog->edictprivate_size);

	Mem_Copy(vm.prog->edictsfields, oldedictsfields, oldmaxedicts * vm.prog->edict_size);
	Mem_Copy(vm.prog->edictprivate, oldedictprivate, oldmaxedicts * vm.prog->edictprivate_size);

	// set e and v pointers
	for(i = 0; i < vm.prog->max_edicts; i++)
	{
		vm.prog->edicts[i].priv.ed  = (vm_edict_t *)((byte  *)vm.prog->edictprivate + i * vm.prog->edictprivate_size);
		vm.prog->edicts[i].progs.vp = (void*)((byte *)vm.prog->edictsfields + i * vm.prog->edict_size);
	}

	PRVM_GCALL(end_increase_edicts)();

	Mem_Free(oldedictsfields);
	Mem_Free(oldedictprivate);
}

//============================================================================
// normal prvm

int PRVM_ED_FindFieldOffset(const char *field)
{
	ddef_t *d;
	d = PRVM_ED_FindField(field);
	if (!d) return 0;
	return d->ofs * 4;
}

int PRVM_ED_FindGlobalOffset(const char *global)
{
	ddef_t *d;
	d = PRVM_ED_FindGlobal(global);
	if (!d) return 0;
	return d->ofs * 4;
}

func_t PRVM_ED_FindFunctionOffset(const char *function)
{
	mfunction_t *f;
	f = PRVM_ED_FindFunction(function);
	if (!f)
		return 0;
	return (func_t)(f - vm.prog->functions);
}

bool PRVM_ProgLoaded( int prognr )
{
	if(prognr < 0 || prognr >= PRVM_MAXPROGS)
		return false;

	return (prog_list[prognr].loaded ? true : false);
}

/*
=================
PRVM_SetProgFromString
=================
*/
// perhaps add a return value when the str doesnt exist
bool PRVM_SetProgFromString(const char *str)
{
	int i = 0;
	for(; i < PRVM_MAXPROGS ; i++)
		if(prog_list[i].name && !com.strcmp(prog_list[i].name,str))
		{
			if(prog_list[i].loaded)
			{
				vm.prog = &prog_list[i];
				return true;
			}
			else
			{
				Msg("%s not loaded !\n", PRVM_NAME);
				return false;
			}
		}

	Msg("Invalid program name %s !\n", str);
	return false;
}

/*
=================
PRVM_SetProg
=================
*/
void PRVM_SetProg(int prognr)
{
	if(0 <= prognr && prognr < PRVM_MAXPROGS)
	{
		if(prog_list[prognr].loaded)
			vm.prog = &prog_list[prognr];
		else PRVM_ERROR("%i not loaded!\n", prognr);
		return;
	}
	PRVM_ERROR("Invalid program number %i", prognr);
}

/*
=================
PRVM_ED_ClearEdict

Sets everything to NULL
=================
*/
void PRVM_ED_ClearEdict (edict_t *e)
{
	Mem_Set (e->progs.vp, 0, vm.prog->progs->entityfields * 4);
	e->priv.ed->free = false;

	// AK: Let the init_edict function determine if something needs to be initialized
	PRVM_GCALL(init_edict)(e);
}

/*
=================
PRVM_ED_Alloc

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
edict_t *PRVM_ED_Alloc (void)
{
	int			i;
	edict_t		*e;

	// the client qc dont need maxclients
	// thus it doesnt need to use svs.maxclients
	// AK:	changed i=svs.maxclients+1
	// AK:	changed so the edict 0 wont spawn -> used as reserved/world entity
	//		although the menu/client has no world
	for (i = vm.prog->reserved_edicts + 1; i < vm.prog->num_edicts; i++)
	{
		e = PRVM_EDICT_NUM(i);
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (e->priv.ed->free && ( e->priv.ed->freetime < 2 || (*vm.prog->time - e->priv.ed->freetime) > 0.5 ) )
		{
			PRVM_ED_ClearEdict (e);
			return e;
		}
	}

	if (i == vm.prog->limit_edicts)
		PRVM_ERROR ("%s: PRVM_ED_Alloc: no free edicts",PRVM_NAME);

	vm.prog->num_edicts++;
	if (vm.prog->num_edicts >= vm.prog->max_edicts)
		PRVM_MEM_IncreaseEdicts();

	e = PRVM_EDICT_NUM(i);
	PRVM_ED_ClearEdict (e);

	return e;
}

/*
=================
PRVM_ED_Free

Marks the edict as free
FIXME: walk all entities and NULL out references to this entity
=================
*/
void PRVM_ED_Free (edict_t *ed)
{
	// dont delete the null entity (world) or reserved edicts
	if(PRVM_NUM_FOR_EDICT(ed) <= vm.prog->reserved_edicts )
		return;

	PRVM_GCALL(free_edict)(ed);

	ed->priv.ed->free = true;
	ed->priv.ed->freetime = *vm.prog->time;
}

//===========================================================================

/*
============
PRVM_ED_GlobalAtOfs
============
*/
ddef_t *PRVM_ED_GlobalAtOfs( int ofs )
{
	ddef_t		*def;
	int			i;

	for( i = 0; i < vm.prog->progs->numglobaldefs; i++ )
	{
		def = &vm.prog->globaldefs[i];
		if( def->ofs == ofs )
			return def;
	}
	return NULL;
}

/*
============
PRVM_ED_FieldAtOfs
============
*/
ddef_t *PRVM_ED_FieldAtOfs (int ofs)
{
	ddef_t		*def;
	int			i;

	for (i=0 ; i<vm.prog->progs->numfielddefs ; i++)
	{
		def = &vm.prog->fielddefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

/*
============
PRVM_ED_FindField
============
*/
ddef_t *PRVM_ED_FindField( const char *name )
{
	ddef_t *def;
	int i;

	for (i=0 ; i<vm.prog->progs->numfielddefs ; i++)
	{
		def = &vm.prog->fielddefs[i];
		if (!com.strcmp(PRVM_GetString(def->s_name), name))
			return def;
	}
	return NULL;
}

/*
============
PRVM_ED_FindGlobal
============
*/
ddef_t *PRVM_ED_FindGlobal (const char *name)
{
	ddef_t *def;
	int i;

	for (i=0 ; i<vm.prog->progs->numglobaldefs ; i++)
	{
		def = &vm.prog->globaldefs[i];
		if (!com.strcmp(PRVM_GetString(def->s_name), name))
			return def;
	}
	return NULL;
}


/*
============
PRVM_ED_FindFunction
============
*/
mfunction_t *PRVM_ED_FindFunction (const char *name)
{
	mfunction_t		*func;
	int				i;

	for (i=0 ; i<vm.prog->progs->numfunctions ; i++)
	{
		func = &vm.prog->functions[i];
		if (!com.strcmp(PRVM_GetString(func->s_name), name))
			return func;
	}
	return NULL;
}


/*
============
PRVM_ValueString

Returns a string describing *data in a type specific manner
=============
*/
char *PRVM_ValueString( etype_t type, prvm_eval_t *val )
{
	static char line[MAX_MSGLEN];
	ddef_t *def;
	mfunction_t *f;
	int n;

	type = (etype_t)type & ~DEF_SAVEGLOBAL;

	switch( type )
	{
	case ev_struct:
		com.strncpy ( line, "struct", sizeof (line));
		break;
	case ev_union:
		com.strncpy ( line, "union", sizeof (line));
		break;
	case ev_string:
		com.strncpy (line, PRVM_GetString (val->string), sizeof (line));
		break;
	case ev_entity:
		n = val->edict;
		if (n < 0 || n >= vm.prog->limit_edicts)
			com.sprintf (line, "entity %i (invalid!)", n);
		else com.sprintf (line, "entity %i", n);
		break;
	case ev_function:
		f = vm.prog->functions + val->function;
		com.sprintf (line, "%s()", PRVM_GetString(f->s_name));
		break;
	case ev_field:
		def = PRVM_ED_FieldAtOfs ( val->_int );
		if(!def) com.sprintf (line, ".???", val->_int );
		else com.sprintf (line, ".%s", PRVM_GetString(def->s_name));
		break;
	case ev_void:
		com.sprintf (line, "void");
		break;
	case ev_float:
		com.sprintf (line, "%10.4f", val->_float);
		break;
	case ev_integer:
		com.sprintf (line, "%i", val->_int);
		break;
	case ev_vector:
		com.sprintf (line, "'%10.4f %10.4f %10.4f'", val->vector[0], val->vector[1], val->vector[2]);
		break;
	case ev_pointer:
		com.sprintf (line, "pointer");
		break;
	default:
		com.sprintf (line, "bad type %i", (int) type);
		break;
	}

	return line;
}

/*
============
PRVM_UglyValueString

Returns a string describing *data in a type specific manner
Easier to parse than PR_ValueString
=============
*/
char *PRVM_UglyValueString (etype_t type, prvm_eval_t *val)
{
	static char line[MAX_MSGLEN];
	int i;
	const char *s;
	ddef_t *def;
	mfunction_t *f;

	type = (etype_t)type & ~DEF_SAVEGLOBAL;

	switch( type )
	{
	case ev_string:
		// Parse the string a bit to turn special characters
		// (like newline, specifically) into escape codes,
		// this fixes saving games from various mods
		s = PRVM_GetString ( val->string );
		for( i = 0;i < (int)sizeof(line) - 2 && *s; )
		{
			if( *s == '\n' )
			{
				line[i++] = '\\';
				line[i++] = 'n';
			}
			else if( *s == '\r' )
			{
				line[i++] = '\\';
				line[i++] = 'r';
			}
			else line[i++] = *s;
			s++;
		}
		line[i] = '\0';
		break;
	case ev_entity:
		com.sprintf( line, "%i", PRVM_NUM_FOR_EDICT(PRVM_PROG_TO_EDICT(val->edict)));
		break;
	case ev_function:
		f = vm.prog->functions + val->function;
		com.strncpy (line, PRVM_GetString (f->s_name), sizeof (line));
		break;
	case ev_field:
		def = PRVM_ED_FieldAtOfs ( val->_int );
		com.sprintf (line, ".%s", PRVM_GetString(def->s_name));
		break;
	case ev_void:
		com.sprintf (line, "void");
		break;
	case ev_float:
		com.sprintf (line, "%f", val->_float);
		break;
	case ev_vector:
		com.sprintf (line, "%f %f %f", val->vector[0], val->vector[1], val->vector[2]);
		break;
	case ev_integer:
		com.sprintf (line, "%i", val->_int);
		break;
	case ev_pointer:
	case ev_variant:
	case ev_struct:
	case ev_union:
		com.sprintf (line, "skip new type %i", type);
		break;
	default:
		com.sprintf (line, "bad type %i", type);
		break;
	}

	return line;
}

/*
============
PRVM_GlobalString

Returns a string with a description and the contents of a global,
padded to 20 field width
============
*/
char *PRVM_GlobalString (int ofs)
{
	char	*s;
	ddef_t	*def;
	void	*val;
	static char	line[128];

	val = (void *)&vm.prog->globals.gp[ofs];
	def = PRVM_ED_GlobalAtOfs(ofs);
	if( !def ) com.sprintf (line,"GLOBAL%i", ofs);
	else
	{
		s = PRVM_ValueString((etype_t)def->type, (prvm_eval_t *)val);
		com.sprintf (line,"%s (=%s)", PRVM_GetString(def->s_name), s);
	}

	return line;
}

char *PRVM_GlobalStringNoContents (int ofs)
{
	//size_t	i;
	ddef_t	*def;
	static char	line[128];

	def = PRVM_ED_GlobalAtOfs(ofs);
	if (!def)
		com.sprintf (line,"GLOBAL%i", ofs);
	else
		com.sprintf (line,"%s", PRVM_GetString(def->s_name));

	return line;
}


/*
=============
PRVM_ED_Print

For debugging
=============
*/
// LordHavoc: optimized this to print out much more quickly (tempstring)
// LordHavoc: changed to print out every 4096 characters (incase there are a lot of fields to print)
void PRVM_ED_Print(edict_t *ed)
{
	size_t		l;
	ddef_t		*d;
	int		*v;
	int		i, j;
	const char	*name;
	int		type;
	char		tempstring[MAX_MSGLEN], tempstring2[260]; // temporary string buffers

	if( ed->priv.ed->free )
	{
		Msg( "%s: FREE\n", PRVM_NAME );
		return;
	}

	tempstring[0] = 0;
	com.sprintf(tempstring, "\n%s EDICT %i:\n", PRVM_NAME, PRVM_NUM_FOR_EDICT(ed));
	for( i = 1; i < vm.prog->progs->numfielddefs; i++ )
	{
		d = &vm.prog->fielddefs[i];
		name = PRVM_GetString(d->s_name);
		if(name[com.strlen(name)-2] == '_')
			continue;	// skip _x, _y, _z vars

		v = (int *)((char *)ed->progs.vp + d->ofs*4);

		// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;

		for (j=0 ; j<prvm_type_size[type] ; j++)
			if (v[j])
				break;
		if (j == prvm_type_size[type])
			continue;

		if (com.strlen(name) > sizeof(tempstring2)-4)
		{
			Mem_Copy (tempstring2, name, sizeof(tempstring2)-4);
			tempstring2[sizeof(tempstring2)-4] = tempstring2[sizeof(tempstring2)-3] = tempstring2[sizeof(tempstring2)-2] = '.';
			tempstring2[sizeof(tempstring2)-1] = 0;
			name = tempstring2;
		}
		com.strncat(tempstring, name, sizeof(tempstring));
		for (l = com.strlen(name);l < 14;l++)
			com.strncat(tempstring, " ", sizeof(tempstring));
		com.strncat(tempstring, " ", sizeof(tempstring));

		name = PRVM_ValueString((etype_t)d->type, (prvm_eval_t *)v);
		if (com.strlen(name) > sizeof(tempstring2)-4)
		{
			Mem_Copy (tempstring2, name, sizeof(tempstring2)-4);
			tempstring2[sizeof(tempstring2)-4] = tempstring2[sizeof(tempstring2)-3] = tempstring2[sizeof(tempstring2)-2] = '.';
			tempstring2[sizeof(tempstring2)-1] = 0;
			name = tempstring2;
		}
		com.strncat(tempstring, name, sizeof(tempstring));
		com.strncat(tempstring, "\n", sizeof(tempstring));
		if (com.strlen(tempstring) >= sizeof(tempstring)/2)
		{
			Msg(tempstring);
			tempstring[0] = 0;
		}
	}
	if (tempstring[0]) Msg(tempstring);
}

/*
=============
PRVM_ED_Write

For savegames
=============
*/
void PRVM_ED_Write( edict_t *ed, void *buffer, void *numpairs, setpair_t callback )
{
	ddef_t		*d;
	int		*v;
	int		i, j;
	const char	*name, *value;
	int		type;

	if( !callback ) return;
	if( ed->priv.ed->free )
	{
		// freed entity too has serialnumber!
		callback( NULL, NULL, buffer, numpairs );
		return;
	}

	for( i = 1; i < vm.prog->progs->numfielddefs; i++ )
	{
		d = &vm.prog->fielddefs[i];
		name = PRVM_GetString( d->s_name );
		if(name[com.strlen(name) - 2] == '_')
			continue;	// skip _x, _y, _z vars

		v = (int *)((char *)ed->progs.vp + d->ofs * 4);
		// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;
		for( j = 0; j < prvm_type_size[type]; j++ )
			if( v[j] ) break;
		if( j == prvm_type_size[type] ) continue;

		value = PRVM_UglyValueString((etype_t)d->type, (prvm_eval_t *)v);
		callback( name, value, buffer, numpairs );
	}
}

/*
=============
PRVM_ED_Read

For savegames
=============
*/
void PRVM_ED_Read( int s_table, int entnum, dkeyvalue_t *fields, int numpairs )
{
	const char *keyname, *value;
	ddef_t	*key;
	edict_t	*ent;
	int	i;

	if( entnum >= vm.prog->limit_edicts ) Host_Error( "PRVM_ED_Read: too many edicts in save file\n" );
	while( entnum >= vm.prog->max_edicts) PRVM_MEM_IncreaseEdicts(); // increase edict numbers
	ent = PRVM_EDICT_NUM( entnum );
	PRVM_ED_ClearEdict( ent );

	if( !numpairs )
	{
		// freed edict
		ent->priv.ed->free = true;
		return;
	}

	// go through all the dictionary pairs
	for( i = 0; i < numpairs; i++ )
	{
		keyname = StringTable_GetString( s_table, fields[i].epair[DENT_KEY] );
		value = StringTable_GetString( s_table, fields[i].epair[DENT_VAL] );

		key = PRVM_ED_FindField( keyname );
		if( !key )
		{
			MsgDev( D_WARN, "%s: unknown field '%s'\n", PRVM_NAME, keyname);
			continue;
		}
		// simple huh ?
		if(!PRVM_ED_ParseEpair( ent, key, value )) PRVM_ERROR( "PRVM_ED_ParseEdict: parse error" );
	}

	// all done, restore physics interaction links or somelike
	PRVM_GCALL(restore_edict)(ent);
}

void PRVM_ED_PrintNum (int ent)
{
	PRVM_ED_Print(PRVM_EDICT_NUM(ent));
}

/*
=============
PRVM_ED_PrintEdicts_f

For debugging, prints all the entities in the current server
=============
*/
void PRVM_ED_PrintEdicts_f (void)
{
	int		i;

	if(Cmd_Argc() != 2)
	{
		Msg("prvm_edicts <program name>\n");
		return;
	}


	if(!PRVM_SetProgFromString(Cmd_Argv(1)))
		return;

	Msg("%s: %i entities\n", PRVM_NAME, vm.prog->num_edicts);
	for (i=0 ; i<vm.prog->num_edicts ; i++)
		PRVM_ED_PrintNum (i);

	vm.prog = NULL;
}

/*
=============
PRVM_ED_PrintEdict_f

For debugging, prints a single edict
=============
*/
void PRVM_ED_PrintEdict_f( void )
{
	int		i;

	if( Cmd_Argc() != 3 )
	{
		Msg("prvm_edict <program name> <edict number>\n");
		return;
	}

	if(!PRVM_SetProgFromString(Cmd_Argv( 1 ))) return;

	i = com.atoi (Cmd_Argv(2));
	if( i >= vm.prog->num_edicts )
	{
		Msg( "bad edict number\n" );
		vm.prog = NULL;
		return;
	}

	PRVM_ED_PrintNum( i );
	vm.prog = NULL;
}

/*
=============
PRVM_ED_Count

For debugging
=============
*/
void PRVM_ED_Count_f( void )
{
	int		i;
	edict_t	*ent;
	int		active;

	if(Cmd_Argc() != 2)
	{
		Msg( "prvm_count <program name>\n" );
		return;
	}


	if(!PRVM_SetProgFromString(Cmd_Argv( 1 )))
		return;

	if( vm.prog->count_edicts ) vm.prog->count_edicts();
	else
	{
		active = 0;
		for( i = 0; i < vm.prog->num_edicts; i++ )
		{
			ent = PRVM_EDICT_NUM( i );
			if( ent->priv.ed->free )
				continue;
			active++;
		}

		Msg( "num_edicts:%3i\n", vm.prog->num_edicts );
		Msg( "active    :%3i\n", active );
	}
	vm.prog = NULL;
}

/*
==============================================================================
			ARCHIVING GLOBALS
==============================================================================
*/
/*
=============
PRVM_ED_WriteGlobals
=============
*/
void PRVM_ED_WriteGlobals( void *buffer, void *numpairs, setpair_t callback )
{
	ddef_t		*def;
	const char	*name;
	const char	*value;
	int		i, type;

	// nothing to process ?
	if( !callback ) return;

	for( i = 0; i < vm.prog->progs->numglobaldefs; i++ )
	{
		def = &vm.prog->globaldefs[i];
		type = def->type;
		if(!(def->type & DEF_SAVEGLOBAL))
			continue;
		type &= ~DEF_SAVEGLOBAL;

		if( type != ev_string && type != ev_float && type != ev_entity )
			continue;

		name = PRVM_GetString(def->s_name);
		value = PRVM_UglyValueString((etype_t)type, (prvm_eval_t *)&vm.prog->globals.gp[def->ofs]);
		callback( name, value, buffer, numpairs );
	}
}

/*
=============
PRVM_ED_ReadGlobals
=============
*/
void PRVM_ED_ReadGlobals( int s_table, dkeyvalue_t *globals, int numpairs )
{
	const char	*keyname;
	const char	*value;
	ddef_t		*key;
	int		i;

	for( i = 0; i < numpairs; i++ )
	{
		keyname = StringTable_GetString( s_table, globals[i].epair[DENT_KEY] );
		value = StringTable_GetString( s_table, globals[i].epair[DENT_VAL]);

		key = PRVM_ED_FindGlobal( keyname );
		if( !key )
		{
			MsgDev( D_INFO, "'%s' is not a global on %s\n", keyname, PRVM_NAME );
			continue;
		}
		if( !PRVM_ED_ParseEpair( NULL, key, value )) PRVM_ERROR( "PRVM_ED_ReadGlobals: parse error\n" );
	}
}

//============================================================================


/*
=============
PRVM_ED_ParseEval

Can parse either fields or globals
returns false if error
=============
*/
bool PRVM_ED_ParseEpair( edict_t *ent, ddef_t *key, const char *s )
{
	int		i, l;
	char		*new_p;
	ddef_t		*def;
	prvm_eval_t	*val;
	mfunction_t	*func;

	if( ent ) val = (prvm_eval_t *)((int *)ent->progs.vp + key->ofs);
	else val = (prvm_eval_t *)((int *)vm.prog->globals.gp + key->ofs);

	switch( key->type & ~DEF_SAVEGLOBAL )
	{
	case ev_string:
		l = (int)com.strlen(s) + 1;
		val->string = PRVM_AllocString(l, &new_p);
		for (i = 0;i < l;i++)
		{
			if (s[i] == '\\' && i < l-1)
			{
				i++;
				if (s[i] == 'n')
					*new_p++ = '\n';
				else if (s[i] == 'r')
					*new_p++ = '\r';
				else
					*new_p++ = s[i];
			}
			else
				*new_p++ = s[i];
		}
		break;
	case ev_float:
		while(*s && *s <= ' ') s++;
		val->_float = com.atof( s );
		break;
	case ev_vector:
		for( i = 0; i < 3; i++ )
		{
			while(*s && *s <= ' ') s++;
			if( !*s ) break;
			val->vector[i] = com.atof( s );
			while( *s > ' ' ) s++;
			if (!*s) break;
		}
		break;
	case ev_entity:
		while( *s && *s <= ' ' ) s++;
		i = com.atoi( s );
		if (i >= vm.prog->limit_edicts)
			MsgDev( D_WARN, "PRVM_ED_ParseEpair: ev_entity reference too large (edict %u >= limit_edicts %u) on %s\n", (uint)i, (uint)vm.prog->limit_edicts, PRVM_NAME);
		while( i >= vm.prog->max_edicts ) PRVM_MEM_IncreaseEdicts();
		// if IncreaseEdicts was called the base pointer needs to be updated
		if( ent ) val = (prvm_eval_t *)((int *)ent->progs.vp + key->ofs);
		val->edict = PRVM_EDICT_TO_PROG(PRVM_EDICT_NUM((int)i));
		break;

	case ev_field:
		def = PRVM_ED_FindField(s);
		if( !def )
		{
			MsgDev( D_WARN, "PRVM_ED_ParseEpair: Can't find field %s in %s\n", s, PRVM_NAME );
			return false;
		}
		val->_int = def->ofs;
		break;

	case ev_function:
		func = PRVM_ED_FindFunction(s);
		if( !func )
		{
			MsgDev( D_WARN, "PRVM_ED_ParseEpair: Can't find function %s in %s\n", s, PRVM_NAME );
			return false;
		}
		val->function = func - vm.prog->functions;
		break;

	default:
		MsgDev( D_WARN, "PRVM_ED_ParseEpair: Unknown key->type %i for key \"%s\" on %s\n", key->type, PRVM_GetString(key->s_name), PRVM_NAME );
		return false;
	}
	return true;
}

/*
=============
PRVM_ED_EdictSet_f

Console command to set a field of a specified edict
=============
*/
void PRVM_ED_EdictSet_f(void)
{
	edict_t *ed;
	ddef_t *key;

	if(Cmd_Argc() != 5)
	{
		Msg("prvm_edictset <program name> <edict number> <field> <value>\n");
		return;
	}


	if(!PRVM_SetProgFromString(Cmd_Argv(1)))
	{
		Msg("Wrong program name %s !\n", Cmd_Argv(1));
		return;
	}

	ed = PRVM_EDICT_NUM(com.atoi(Cmd_Argv(2)));

	if((key = PRVM_ED_FindField(Cmd_Argv(3))) == 0)
		MsgDev( D_WARN, "Key %s not found !\n", Cmd_Argv(3));
	else PRVM_ED_ParseEpair(ed, key, Cmd_Argv(4));

	vm.prog = NULL;
}

/*
====================
PRVM_ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
Used for initial level load and for savegames.
====================
*/
const char *PRVM_ED_ParseEdict( const char *data, edict_t *ent )
{
	ddef_t	*key;
	bool	init, newline, anglehack;
	char	keyname[256];
	size_t	n;

	init = false;

	// go through all the dictionary pairs
	MsgDev( D_NOTE, "{\n" );
	while( 1 )
	{
		// parse key
		PR_ParseToken( &data, true );

		if( pr_token[0] == '\0')
			PRVM_ERROR ("PRVM_ED_ParseEdict: EOF without closing brace");

		// just format console messages
		newline = (pr_token[0] == '}') ? true : false;
		if(!newline) MsgDev(D_NOTE, "\"%s\"", pr_token);
		else 
		{
			MsgDev( D_NOTE, "}\n");
			break;
		}

		// anglehack is to allow QuakeEd to write single scalar angles
		// and allow them to be turned into vectors. (FIXME...)
		if( !com.strcmp( pr_token, "angle" ))
		{
			com.strncpy( keyname, "angles", sizeof(keyname));
			anglehack = true;
		}
		else 
		{
			anglehack = false;
			com.strncpy( keyname, pr_token, sizeof(keyname));
		}
		// another hack to fix keynames with trailing spaces
		n = com.strlen( keyname );
		while(n && keyname[n-1] == ' ')
		{
			keyname[n-1] = 0;
			n--;
		}

		// parse value
		if (!PR_ParseToken( &data, true ))
			PRVM_ERROR ("PRVM_ED_ParseEdict: EOF without closing brace");
		MsgDev(D_NOTE, " \"%s\"\n", pr_token);
		if( pr_token[0] == '}' ) PRVM_ERROR( "PRVM_ED_ParseEdict: closing brace without data" );
		init = true;

		// ignore attempts to set key "" (this problem occurs in nehahra neh1m8.bsp)
		if (!keyname[0]) continue;

		// keynames with a leading underscore are used for utility comments,
		// and are immediately discarded by quake
		if(!stricmp( keyname, "light" )) continue;	// ignore lightvalue
		if( keyname[0] == '_' ) continue;

		key = PRVM_ED_FindField( keyname );
		if( !key )
		{
			MsgDev(D_NOTE, "%s: unknown field '%s'\n", PRVM_NAME, keyname);
			continue;
		}

		if( anglehack )
		{
			char	temp[32];
			
			com.strncpy( temp, pr_token, sizeof(temp));
			com.sprintf( pr_token, "0 %s 0", temp );
		}
		if(!PRVM_ED_ParseEpair( ent, key, pr_token )) PRVM_ERROR ("PRVM_ED_ParseEdict: parse error");
	}
	if(!init) ent->priv.ed->free = true;

	return data;
}


/*
================
PRVM_ED_LoadFromFile

The entities are directly placed in the array, rather than allocated with
PRVM_ED_Alloc, because otherwise an error loading the map would have entity
number references out of order.

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.

Used for both fresh maps and savegame loads.  A fresh map would also need
to call PRVM_ED_CallSpawnFunctions () to let the objects initialize themselves.
================
*/
void PRVM_ED_LoadFromFile( const char *data )
{
	edict_t *ent;
	int parsed, inhibited, spawned, died;
	mfunction_t *func;

	parsed = 0;
	inhibited = 0;
	spawned = 0;
	died = 0;

	// parse ents
	while( 1 )
	{
		// parse the opening brace
		PR_ParseToken( &data, true );

		if( pr_token[0] == '\0') break;
		if( pr_token[0] != '{' )
			PRVM_ERROR ("PRVM_ED_LoadFromFile: %s: found %s when expecting {", PRVM_NAME, pr_token );

		// CHANGED: this is not conform to PR_LoadFromFile
		if(vm.prog->loadintoworld)
		{
			vm.prog->loadintoworld = false;
			ent = PRVM_EDICT_NUM(0);
		}
		else ent = PRVM_ED_Alloc();

		// HACKHACK: clear it 
		if( ent != vm.prog->edicts ) Mem_Set( ent->progs.vp, 0, vm.prog->progs->entityfields * 4 );

		data = PRVM_ED_ParseEdict( data, ent );
		parsed++;

		// remove the entity ?
		if(vm.prog->load_edict && !vm.prog->load_edict(ent))
		{
			PRVM_ED_Free(ent);
			inhibited++;
			continue;
		}

		// immediately call spawn function, but only if there is a pev global and a classname
		if(vm.prog->pev && vm.prog->flag & PRVM_FE_CLASSNAME)
		{
			string_t handle = *(string_t*)&((byte*)ent->progs.vp)[PRVM_ED_FindFieldOffset("classname")];
			if( !handle )
			{
				if(prvm_developer >= D_NOTE)
				{
					MsgDev( D_ERROR, "No classname for:\n");
					PRVM_ED_Print(ent);
				}
				PRVM_ED_Free (ent);
				continue;
			}

			// look for the spawn function
			func = PRVM_ED_FindFunction (PRVM_GetString(handle));

			if( !func )
			{
				if( prvm_developer >= D_ERROR )
				{
					MsgDev( D_ERROR, "No spawn function for:\n" );
					PRVM_ED_Print( ent );
				}
				PRVM_ED_Free( ent );
				continue;
			}

			// pev = ent
			PRVM_G_INT(vm.prog->pev->ofs) = PRVM_EDICT_TO_PROG(ent);
			PRVM_ExecuteProgram( func - vm.prog->functions, "", __FILE__, __LINE__ );
		}

		spawned++;
		if (ent->priv.ed->free) died++;
	}
	MsgDev(D_NOTE, "%s: %i new entities parsed, %i new inhibited, %i (%i new) spawned (whereas %i removed self, %i stayed)\n", PRVM_NAME, parsed, inhibited, vm.prog->num_edicts, spawned, died, spawned - died);
}

/*
===============
PRVM_ResetProg
===============
*/

void PRVM_ResetProg( void )
{
	PRVM_GCALL(reset_cmd)();
	Mem_FreePool(&vm.prog->progs_mempool);
	Mem_Set(vm.prog, 0, sizeof(prvm_prog_t));
}

/*
===============
PRVM_LoadProgs
===============
*/
void PRVM_LoadProgs( const char *filename )
{
	dstatement_t	*st;
	ddef_t		*infielddefs;
	dfunction_t	*dfunctions;
	fs_offset_t	filesize;
	int		i, len, complen;
	byte		*s;

	if( vm.prog->loaded )
	{
		PRVM_ERROR ("PRVM_LoadProgs: there is already a %s program loaded!", PRVM_NAME );
	}

	if( vm.prog->progs ) Mem_Free( vm.prog->progs ); // release progs file
	vm.prog->progs = (dprograms_t *)FS_LoadFile(va("%s", filename ), &filesize);

	if( vm.prog->progs == NULL || filesize < (fs_offset_t)sizeof(dprograms_t))
		PRVM_ERROR("PRVM_LoadProgs: couldn't load %s for %s\n", filename, PRVM_NAME);

	MsgDev(D_NOTE, "%s programs occupy %iK.\n", PRVM_NAME, filesize/1024);
	SwapBlock((int *)vm.prog->progs, sizeof(*vm.prog->progs)); 	// byte swap the header
	
	if( vm.prog->progs->version != VPROGS_VERSION)
		PRVM_ERROR( "%s: %s has wrong version number (%i should be %i)", PRVM_NAME, filename, vm.prog->progs->version, VPROGS_VERSION );

	// try to recognize progs.dat by crc
	if( PRVM_GetProgNr() == PRVM_DECOMPILED )
	{
		MsgDev(D_INFO, "Prepare %s [CRC %d]\n", filename, vm.prog->progs->crc );
	}
	else if( vm.prog->progs->crc != vm.prog->filecrc )
	{ 
		PRVM_ERROR("%s: %s system vars have been modified, progdefs.h is out of date %d\n", PRVM_NAME, filename, vm.prog->filecrc );	
	}
	else MsgDev(D_LOAD, "%s [^2CRC %d^7]\n", filename, vm.prog->progs->crc );

	// set initial pointers
	vm.prog->statements = (dstatement_t *)((byte *)vm.prog->progs + vm.prog->progs->ofs_statements);
	vm.prog->globaldefs = (ddef_t *)((byte *)vm.prog->progs + vm.prog->progs->ofs_globaldefs);
	infielddefs = (ddef_t *)((byte *)vm.prog->progs + vm.prog->progs->ofs_fielddefs);
	dfunctions = (dfunction_t *)((byte *)vm.prog->progs + vm.prog->progs->ofs_functions);
	vm.prog->strings = (char *)vm.prog->progs + vm.prog->progs->ofs_strings;
	vm.prog->globals.gp = (float *)((byte *)vm.prog->progs + vm.prog->progs->ofs_globals);

	// debug info
	if( vm.prog->progs->ofssources ) vm.prog->sources = (dsource_t*)((byte *)vm.prog->progs + vm.prog->progs->ofssources);
	if( vm.prog->progs->ofslinenums ) vm.prog->linenums = (int *)((byte *)vm.prog->progs + vm.prog->progs->ofslinenums);
	if( vm.prog->progs->ofs_types ) vm.prog->types = (type_t *)((byte *)vm.prog->progs + vm.prog->progs->ofs_types);

	// decompress progs if needed
	if( vm.prog->progs->flags & COMP_STATEMENTS )
	{
		len = sizeof(dstatement_t) * vm.prog->progs->numstatements;
		complen = LittleLong(*(int*)vm.prog->statements);

		MsgDev(D_NOTE, "Unpacked statements: len %d, comp len %d\n", len, complen );                   
		s = Mem_Alloc(vm.prog->progs_mempool, len ); // alloc memory for inflate block
		VFS_Unpack((char *)(((int *)vm.prog->statements)+1), complen, &s, len ); 
		vm.prog->statements = (dstatement_t *)s;
		filesize += len - complen - sizeof(int); //merge filesize
	}

	if( vm.prog->progs->flags & COMP_DEFS )
	{
		len = sizeof(ddef_t) * vm.prog->progs->numglobaldefs;
		complen = LittleLong(*(int*)vm.prog->globaldefs);

		MsgDev(D_NOTE, "Unpacked defs: len %d, comp len %d\n", len, complen);                   
		s = Mem_Alloc(vm.prog->progs_mempool, len ); // alloc memory for inflate block
		VFS_Unpack((char *)(((int *)vm.prog->globaldefs)+1), complen, &s, len ); 
		vm.prog->globaldefs = (ddef_t *)s;
		filesize += len - complen - sizeof(int); //merge filesize
	}

	if( vm.prog->progs->flags & COMP_FIELDS )
	{
		len = sizeof(ddef_t) * vm.prog->progs->numfielddefs;
		complen = LittleLong(*(int*)infielddefs);

		MsgDev(D_NOTE, "Unpacked fields: len %d, comp len %d\n", len, complen );                   
		s = Mem_Alloc(vm.prog->progs_mempool, len ); // alloc memory for inflate block
		VFS_Unpack((char *)(((int *)infielddefs)+1), complen, &s, len ); 
		infielddefs = (ddef_t *)s;
		filesize += len - complen - sizeof(int); //merge filesize
	}

	if( vm.prog->progs->flags & COMP_FUNCTIONS )
	{
		len = sizeof(dfunction_t) * vm.prog->progs->numfunctions;
		complen = LittleLong(*(int*)dfunctions);

		MsgDev(D_NOTE, "Unpacked functions: len %d, comp len %d\n", len, complen );                   
		s = Mem_Alloc(vm.prog->progs_mempool, len ); // alloc memory for inflate block
		VFS_Unpack((char *)(((int *)dfunctions)+1), complen, &s, len ); 
		dfunctions = (dfunction_t *)s;
		filesize += len - complen - sizeof(int); //merge filesize
	}

	if( vm.prog->progs->flags & COMP_STRINGS )
	{
		len = sizeof(char) * vm.prog->progs->numstrings;
		complen = LittleLong(*(int*)vm.prog->strings);

		MsgDev(D_NOTE, "Unpacked strings: count %d, len %d, comp len %d\n", vm.prog->progs->numstrings, len, complen );                   
		s = Mem_Alloc(vm.prog->progs_mempool, len ); // alloc memory for inflate block
		VFS_Unpack((char *)(((int *)vm.prog->strings)+1), complen, &s, len ); 
		vm.prog->strings = (char *)s;

		vm.prog->progs->ofs_strings += 4;
		filesize += len - complen - sizeof(int); //merge filesize
	}

	if( vm.prog->progs->flags & COMP_GLOBALS )
	{
		len = sizeof(float) * vm.prog->progs->numglobals;
		complen = LittleLong(*(int*)vm.prog->globals.gp);

		MsgDev(D_NOTE, "Unpacked globals: len %d, comp len %d\n", len, complen );                   
		s = Mem_Alloc(vm.prog->progs_mempool, len ); // alloc memory for inflate block
		VFS_Unpack((char *)(((int *)vm.prog->globals.gp)+1), complen, &s, len ); 
		vm.prog->globals.gp = (float *)s;
		filesize += len - complen - sizeof(int); //merge filesize
	}

	if( vm.prog->linenums && vm.prog->progs->flags & COMP_LINENUMS )
	{
		len = sizeof(int) * vm.prog->progs->numstatements;
		complen = LittleLong(*(int*)vm.prog->linenums);

		MsgDev(D_NOTE, "Unpacked linenums: len %d, comp len %d\n", len, complen );                   
		s = Mem_Alloc(vm.prog->progs_mempool, len ); // alloc memory for inflate block
		VFS_Unpack((char *)(((int *)vm.prog->linenums)+1), complen, &s, len ); 
		vm.prog->linenums = (int *)s;
		filesize += len - complen - sizeof(int); //merge filesize
	}

	if( vm.prog->types && vm.prog->progs->flags & COMP_TYPES )
	{
		len = sizeof(type_t) * vm.prog->progs->numtypes;
		complen = LittleLong(*(int*)vm.prog->types);

		MsgDev(D_NOTE, "Unpacked types: len %d, comp len %d\n", len, complen );                   
		s = Mem_Alloc(vm.prog->progs_mempool, len ); // alloc memory for inflate block
		VFS_Unpack((char *)(((int *)vm.prog->types)+1), complen, &s, len ); 
		vm.prog->types = (type_t *)s;
		filesize += len - complen - sizeof(int); //merge filesize
	}

	vm.prog->stringssize = 0;

	for( i = 0; vm.prog->stringssize < vm.prog->progs->numstrings; i++ )
	{
		if( vm.prog->progs->ofs_strings + vm.prog->stringssize >= (int)filesize )
			PRVM_ERROR ("%s: %s strings go past end of file", PRVM_NAME, filename);
		vm.prog->stringssize += (int)com.strlen(vm.prog->strings + vm.prog->stringssize) + 1;
	}
 
	vm.prog->numknownstrings = 0;
	vm.prog->maxknownstrings = 0;
	vm.prog->knownstrings = NULL;
	vm.prog->knownstrings_freeable = NULL;

	// we need to expand the fielddefs list to include all the engine fields,
	// so allocate a new place for it ( + DPFIELDS  )
	vm.prog->fielddefs = (ddef_t *)Mem_Alloc(vm.prog->progs_mempool, (vm.prog->progs->numfielddefs) * sizeof(ddef_t));
	vm.prog->statement_profile = (double *)Mem_Alloc(vm.prog->progs_mempool, vm.prog->progs->numstatements * sizeof(*vm.prog->statement_profile));

	// byte swap the lumps
	for (i = 0; i < vm.prog->progs->numstatements; i++)
	{
		vm.prog->statements[i].op = LittleLong(vm.prog->statements[i].op);
		vm.prog->statements[i].a = LittleLong(vm.prog->statements[i].a);
		vm.prog->statements[i].b = LittleLong(vm.prog->statements[i].b);
		vm.prog->statements[i].c = LittleLong(vm.prog->statements[i].c);
	}
	vm.prog->functions = (mfunction_t *)Mem_Alloc(vm.prog->progs_mempool, sizeof(mfunction_t) * vm.prog->progs->numfunctions);
	for (i = 0; i < vm.prog->progs->numfunctions; i++)
	{
		vm.prog->functions[i].first_statement = LittleLong (dfunctions[i].first_statement);
		vm.prog->functions[i].parm_start = LittleLong (dfunctions[i].parm_start);
		vm.prog->functions[i].s_name = LittleLong (dfunctions[i].s_name);
		vm.prog->functions[i].s_file = LittleLong (dfunctions[i].s_file);
		vm.prog->functions[i].numparms = LittleLong (dfunctions[i].numparms);
		vm.prog->functions[i].locals = LittleLong (dfunctions[i].locals);
		Mem_Copy(vm.prog->functions[i].parm_size, dfunctions[i].parm_size, sizeof(dfunctions[i].parm_size));
	}

	for (i = 0; i < vm.prog->progs->numglobaldefs; i++)
	{
		vm.prog->globaldefs[i].type = LittleLong (vm.prog->globaldefs[i].type);
		vm.prog->globaldefs[i].ofs = LittleLong (vm.prog->globaldefs[i].ofs);
		vm.prog->globaldefs[i].s_name = LittleLong (vm.prog->globaldefs[i].s_name);
	}

	// copy the progs fields to the new fields list
	for (i = 0; i < vm.prog->progs->numfielddefs; i++)
	{
		vm.prog->fielddefs[i].type = LittleLong (infielddefs[i].type);
		if (vm.prog->fielddefs[i].type & DEF_SAVEGLOBAL)
			PRVM_ERROR ("PRVM_LoadProgs: vm.prog->fielddefs[i].type & DEF_SAVEGLOBAL in %s", PRVM_NAME);
		vm.prog->fielddefs[i].ofs = LittleLong (infielddefs[i].ofs);
		vm.prog->fielddefs[i].s_name = LittleLong (infielddefs[i].s_name);
	}

	for( i = 0; i < vm.prog->progs->numglobals; i++ )
		((int *)vm.prog->globals.gp)[i] = LittleLong (((int *)vm.prog->globals.gp)[i]);

	// moved edict_size calculation down here, below field adding code
	// LordHavoc: this no longer includes the edict_t header
	vm.prog->edict_size = vm.prog->progs->entityfields * 4;
	vm.prog->edictareasize = vm.prog->edict_size * vm.prog->limit_edicts;

	// LordHavoc: bounds check anything static
	for (i = 0, st = vm.prog->statements; i < vm.prog->progs->numstatements; i++, st++)
	{
		switch (st->op)
		{
		case OP_IF:
		case OP_IFNOT:
			if((dword)st->a >= vm.prog->progs->numglobals || st->b + i < 0 || st->b + i >= vm.prog->progs->numstatements)
				PRVM_ERROR("PRVM_LoadProgs: out of bounds IF/IFNOT (statement %d) in %s", i, PRVM_NAME);
			break;
		case OP_IFNOTS:
			// FIXME: make work
			break;
		case OP_GOTO:
			if(st->a + i < 0 || st->a + i >= vm.prog->progs->numstatements)
				PRVM_ERROR("PRVM_LoadProgs: out of bounds GOTO (statement %d) in %s", i, PRVM_NAME);
			break;
		// global global global
		case OP_ADD_F:
		case OP_ADD_V:
		case OP_SUB_F:
		case OP_SUB_V:
		case OP_MUL_F:
		case OP_MUL_V:
		case OP_MUL_FV:
		case OP_MUL_VF:
		case OP_DIV_F:
		case OP_BITAND:
		case OP_BITOR:
		case OP_BITSET:
		case OP_BITSETP:
		case OP_BITCLR:
		case OP_BITCLRP:
		case OP_GE:
		case OP_LE:
		case OP_GT:
		case OP_LT:
		case OP_AND:
		case OP_OR:
		case OP_EQ_F:
		case OP_EQ_V:
		case OP_EQ_S:
		case OP_EQ_E:
		case OP_EQ_FNC:
		case OP_NE_F:
		case OP_NE_V:
		case OP_NE_S:
		case OP_NE_E:
		case OP_NE_FNC:
		case OP_ADDRESS:
		case OP_LOAD_F:
		case OP_LOAD_FLD:
		case OP_LOAD_ENT:
		case OP_LOAD_S:
		case OP_LOAD_FNC:
		case OP_LOAD_V:
		case OP_LOADA_F:
		case OP_LOADA_V:	
		case OP_LOADA_S:
		case OP_LOADA_ENT:
		case OP_LOADA_FLD:		
		case OP_LOADA_FNC:
		case OP_LOADA_I:
		case OP_LE_I:
		case OP_GE_I:
		case OP_LT_I:
		case OP_GT_I:
		case OP_LE_IF:
		case OP_GE_IF:
		case OP_LT_IF:
		case OP_GT_IF:
		case OP_LE_FI:
		case OP_GE_FI:
		case OP_LT_FI:
		case OP_GT_FI:
		case OP_EQ_IF:
		case OP_EQ_FI:
		case OP_CONV_ITOF:
		case OP_CONV_FTOI:
		case OP_CP_ITOF:
		case OP_CP_FTOI:
		case OP_GLOBAL_ADD:
		case OP_POINTER_ADD:
			if((dword) st->a >= vm.prog->progs->numglobals || (dword) st->b >= vm.prog->progs->numglobals || (dword)st->c >= vm.prog->progs->numglobals)
				PRVM_ERROR("PRVM_LoadProgs: out of bounds global index (statement %d)", i);
			break;
		// global none global
		case OP_NOT_F:
		case OP_NOT_V:
		case OP_NOT_S:
		case OP_NOT_FNC:
		case OP_NOT_ENT:
			if((dword) st->a >= vm.prog->progs->numglobals || (dword) st->c >= vm.prog->progs->numglobals)
				PRVM_ERROR("PRVM_LoadProgs: out of bounds global index (statement %d) in %s", i, PRVM_NAME);
			break;
		// 2 globals
		case OP_STOREP_F:
		case OP_STOREP_ENT:
		case OP_STOREP_FLD:
		case OP_STOREP_S:
		case OP_STOREP_FNC:
		case OP_STORE_F:
		case OP_STORE_ENT:
		case OP_STORE_FLD:
		case OP_STORE_S:
		case OP_STORE_FNC:
		case OP_STATE:
		case OP_STOREP_V:
		case OP_STORE_V:
		case OP_MULSTORE_F:
		case OP_MULSTORE_V:
		case OP_MULSTOREP_F:
		case OP_MULSTOREP_V:
		case OP_DIVSTORE_F:
		case OP_DIVSTOREP_F:
		case OP_ADDSTORE_F:
		case OP_ADDSTORE_V:
		case OP_ADDSTOREP_F:
		case OP_ADDSTOREP_V:
		case OP_SUBSTORE_F:
		case OP_SUBSTORE_V:
		case OP_SUBSTOREP_F:
		case OP_SUBSTOREP_V:
			if ((dword) st->a >= vm.prog->progs->numglobals || (dword) st->b >= vm.prog->progs->numglobals)
				Host_Error("PRVM_LoadProgs: out of bounds global index (statement %d) in %s", i, PRVM_NAME);
			break;
		// 1 global
		case OP_CALL0:
		case OP_CALL1:
		case OP_CALL2:
		case OP_CALL3:
		case OP_CALL4:
		case OP_CALL5:
		case OP_CALL6:
		case OP_CALL7:
		case OP_CALL8:
		case OP_CALL9:
		case OP_DONE:
		case OP_RETURN:
			if ((dword) st->a >= vm.prog->progs->numglobals)
				Host_Error("PRVM_LoadProgs: out of bounds global index (statement %d) in %s", i, PRVM_NAME);
			break;
		default:
			MsgDev( D_NOTE, "PRVM_LoadProgs: unknown opcode OP_%s at statement %d in %s\n", pr_opcodes[st->op].opname, i, PRVM_NAME);
			break;
		}
	}

	PRVM_Init_Exec();
	vm.prog->loaded = true;

	// set flags & ddef_ts in prog
	vm.prog->flag = 0;
	vm.prog->pev = PRVM_ED_FindGlobal( "pev" ); // critical stuff

	if( PRVM_ED_FindGlobal("time") && PRVM_ED_FindGlobal("time")->type & ev_float )
		vm.prog->time = &PRVM_G_FLOAT(PRVM_ED_FindGlobal("time")->ofs);

	if(PRVM_ED_FindField("chain")) vm.prog->flag |= PRVM_FE_CHAIN;
	if(PRVM_ED_FindField("classname")) vm.prog->flag |= PRVM_FE_CLASSNAME;
	if(PRVM_ED_FindField("nextthink") && PRVM_ED_FindField ("frame") && PRVM_ED_FindField ("think") && vm.prog->flag && vm.prog->pev )
		vm.prog->flag |= PRVM_OP_STATE;
	if(PRVM_ED_FindField ("nextthink") && vm.prog->flag && vm.prog->pev )
		vm.prog->flag |= PRVM_OP_THINKTIME;
	PRVM_GCALL(init_cmd)();

	// init mempools
	PRVM_MEM_Alloc();
}


void PRVM_Fields_f (void)
{
	int i, j, ednum, used, usedamount;
	int *counts;
	char tempstring[MAX_MSGLEN], tempstring2[260];
	const char *name;
	edict_t *ed;
	ddef_t *d;
	int *v;

	// TODO
	/*
	if (!sv.active)
	{
		Msg("no progs loaded\n");
		return;
	}
	*/

	if(Cmd_Argc() != 2)
	{
		Msg("prvm_fields <program name>\n");
		return;
	}


	if(!PRVM_SetProgFromString(Cmd_Argv(1)))
		return;

	counts = (int *)Qalloc(vm.prog->progs->numfielddefs * sizeof(int));
	for (ednum = 0; ednum < vm.prog->max_edicts; ednum++)
	{
		ed = PRVM_EDICT_NUM(ednum);
		if (ed->priv.ed->free)
			continue;
		for (i = 1;i < vm.prog->progs->numfielddefs;i++)
		{
			d = &vm.prog->fielddefs[i];
			name = PRVM_GetString(d->s_name);
			if (name[com.strlen(name)-2] == '_')
				continue;	// skip _x, _y, _z vars
			v = (int *)((char *)ed->progs.vp + d->ofs*4);
			// if the value is still all 0, skip the field
			for (j = 0;j < prvm_type_size[d->type & ~DEF_SAVEGLOBAL];j++)
			{
				if (v[j])
				{
					counts[i]++;
					break;
				}
			}
		}
	}
	used = 0;
	usedamount = 0;
	tempstring[0] = 0;
	for (i = 0;i < vm.prog->progs->numfielddefs;i++)
	{
		d = &vm.prog->fielddefs[i];
		name = PRVM_GetString(d->s_name);
		if (name[com.strlen(name)-2] == '_')
			continue;	// skip _x, _y, _z vars
		switch(d->type & ~DEF_SAVEGLOBAL)
		{
		case ev_string:
			com.strncat(tempstring, "string   ", sizeof(tempstring));
			break;
		case ev_entity:
			com.strncat(tempstring, "entity   ", sizeof(tempstring));
			break;
		case ev_function:
			com.strncat(tempstring, "function ", sizeof(tempstring));
			break;
		case ev_field:
			com.strncat(tempstring, "field    ", sizeof(tempstring));
			break;
		case ev_void:
			com.strncat(tempstring, "void     ", sizeof(tempstring));
			break;
		case ev_float:
			com.strncat(tempstring, "float    ", sizeof(tempstring));
			break;
		case ev_vector:
			com.strncat(tempstring, "vector   ", sizeof(tempstring));
			break;
		case ev_pointer:
			com.strncat(tempstring, "pointer  ", sizeof(tempstring));
			break;
		default:
			com.sprintf (tempstring2, "bad type %i ", d->type & ~DEF_SAVEGLOBAL);
			com.strncat(tempstring, tempstring2, sizeof(tempstring));
			break;
		}
		if (com.strlen(name) > sizeof(tempstring2)-4)
		{
			Mem_Copy (tempstring2, name, sizeof(tempstring2)-4);
			tempstring2[sizeof(tempstring2)-4] = tempstring2[sizeof(tempstring2)-3] = tempstring2[sizeof(tempstring2)-2] = '.';
			tempstring2[sizeof(tempstring2)-1] = 0;
			name = tempstring2;
		}
		com.strncat(tempstring, name, sizeof(tempstring));
		for (j = (int)com.strlen(name);j < 25;j++)
			com.strncat(tempstring, " ", sizeof(tempstring));
		com.sprintf(tempstring2, "%5d", counts[i]);
		com.strncat(tempstring, tempstring2, sizeof(tempstring));
		com.strncat(tempstring, "\n", sizeof(tempstring));
		if (com.strlen(tempstring) >= sizeof(tempstring)/2)
		{
			Msg(tempstring);
			tempstring[0] = 0;
		}
		if (counts[i])
		{
			used++;
			usedamount += prvm_type_size[d->type & ~DEF_SAVEGLOBAL];
		}
	}
	Mem_Free(counts);
	Msg("%s: %i entity fields (%i in use), totalling %i bytes per edict (%i in use), %i edicts allocated, %i bytes total spent on edict fields (%i needed)\n", PRVM_NAME, vm.prog->progs->entityfields, used, vm.prog->progs->entityfields * 4, usedamount * 4, vm.prog->max_edicts, vm.prog->progs->entityfields * 4 * vm.prog->max_edicts, usedamount * 4 * vm.prog->max_edicts);

	vm.prog = NULL;
}

void PRVM_Globals_f (void)
{
	int i;
	// TODO
	/*if (!sv.active)
	{
		Msg("no progs loaded\n");
		return;
	}*/
	if(Cmd_Argc () != 2)
	{
		Msg("prvm_globals <program name>\n");
		return;
	}


	if(!PRVM_SetProgFromString (Cmd_Argv (1)))
		return;

	Msg("%s :", PRVM_NAME);

	for (i = 0;i < vm.prog->progs->numglobaldefs;i++)
		Msg("%s\n", PRVM_GetString(vm.prog->globaldefs[i].s_name));
	Msg("%i global variables, totalling %i bytes\n", vm.prog->progs->numglobals, vm.prog->progs->numglobals * 4);

	vm.prog = NULL;
}

/*
===============
PRVM_Global
===============
*/
void PRVM_Global_f(void)
{
	ddef_t *global;
	if( Cmd_Argc() != 3 )
	{
		Msg( "prvm_global <program name> <global name>\n" );
		return;
	}


	if( !PRVM_SetProgFromString( Cmd_Argv(1) ) )
		return;

	global = PRVM_ED_FindGlobal( Cmd_Argv(2) );
	if( !global )
		Msg( "No global '%s' in %s!\n", Cmd_Argv(2), Cmd_Argv(1) );
	else
		Msg( "%s: %s\n", Cmd_Argv(2), PRVM_ValueString( (etype_t)global->type, (prvm_eval_t *) &vm.prog->globals.gp[ global->ofs ] ) );
	vm.prog = NULL;
}

/*
===============
PRVM_GlobalSet
===============
*/
void PRVM_GlobalSet_f(void)
{
	ddef_t *global;
	if( Cmd_Argc() != 4 ) {
		Msg( "prvm_globalset <program name> <global name> <value>\n" );
		return;
	}


	if( !PRVM_SetProgFromString( Cmd_Argv(1) ) )
		return;

	global = PRVM_ED_FindGlobal( Cmd_Argv(2) );
	if( !global )
		Msg( "No global '%s' in %s!\n", Cmd_Argv(2), Cmd_Argv(1) );
	else
		PRVM_ED_ParseEpair( NULL, global, Cmd_Argv(3) );
	vm.prog = NULL;
}

// LordHavoc: changed this to NOT use a return statement, so that it can be used in functions that must return a value
void VM_Warning( const char *fmt, ... )
{
	va_list		argptr;
	static char	msg[MAX_MSGLEN];

	va_start( argptr, fmt );
	com.vsnprintf( msg, sizeof(msg), fmt, argptr );
	va_end( argptr );

	Msg( msg );
	// TODO: either add a cvar/cmd to control the state dumping or replace some of the calls with Msgf [9/13/2006 Black]
	//PRVM_PrintState();
}

/*
===============
VM_error

Abort the server with a game error
===============
*/
void VM_Error( const char *fmt, ... )
{
	char		msg[1024];
	va_list		argptr;
	
	va_start (argptr, fmt);
	com.vsprintf (msg, fmt, argptr);
	va_end (argptr);

	Host_Error("Prvm error: %s", msg);
}

/*
===============
PRVM_InitProg
===============
*/
void PRVM_InitProg( int prognr )
{
	if(prognr < 0 || prognr >= PRVM_MAXPROGS)
		Host_Error("PRVM_InitProg: Invalid program number %i",prognr);

	vm.prog = &prog_list[prognr];

	if(vm.prog->loaded) PRVM_ResetProg();

	Mem_Set(vm.prog, 0, sizeof(prvm_prog_t));

	vm.prog->time = &vm.prog->_time;
	vm.prog->error_cmd = VM_Error;
}

int PRVM_GetProgNr()
{
	return vm.prog - prog_list;
}

void *_PRVM_Alloc(size_t buffersize, const char *filename, int fileline)
{
	return com.malloc(vm.prog->progs_mempool, buffersize, filename, fileline);
}

void _PRVM_Free(void *buffer, const char *filename, int fileline)
{
	com.free(buffer, filename, fileline);
}

void _PRVM_FreeAll(const char *filename, int fileline)
{
	vm.prog->progs = NULL;
	vm.prog->fielddefs = NULL;
	vm.prog->functions = NULL;
	com.clearpool( vm.prog->progs_mempool, filename, fileline);
}

// LordHavoc: turned PRVM_EDICT_NUM into a #define for speed reasons
edict_t *PRVM_EDICT_NUM_ERROR(int n, char *filename, int fileline)
{
	PRVM_ERROR ("PRVM_EDICT_NUM: %s: bad number %i (called at %s:%i)", PRVM_NAME, n, filename, fileline);
	return NULL;
}

const char *PRVM_GetString(int num)
{
	if( num >= 0 )
	{
		if( num < vm.prog->stringssize ) return vm.prog->strings + num;
		else if( num <= vm.prog->stringssize + vm_tempstringsbuf.maxsize)
		{
			num -= vm.prog->stringssize;
			if( num < vm_tempstringsbuf.cursize )
				return (char *)vm_tempstringsbuf.data + num;
			else
			{
				VM_Warning("PRVM_GetString: Invalid temp-string offset (%i >= %i vm_tempstringsbuf.cursize)\n", num, vm_tempstringsbuf.cursize);
				return "";
			}
		}
		else
		{
			VM_Warning("PRVM_GetString: Invalid constant-string offset (%i >= %i prog->stringssize)\n", num, vm.prog->stringssize);
			return "";
		}
	}
	else
	{
		num = -1 - num;
		if (num < vm.prog->numknownstrings)
		{
			if (!vm.prog->knownstrings[num])
				VM_Warning("PRVM_GetString: Invalid zone-string offset (%i has been freed)\n", num);
			return vm.prog->knownstrings[num];
		}
		else
		{
			VM_Warning("PRVM_GetString: Invalid zone-string offset (%i >= %i)\n", num, vm.prog->numknownstrings);
			return "";
		}
	}
}

int PRVM_SetEngineString( const char *s )
{
	int i;
	if (!s)
		return 0;
	if (s >= vm.prog->strings && s <= vm.prog->strings + vm.prog->stringssize)
		PRVM_ERROR("PRVM_SetEngineString: s in vm.prog->strings area");
	// if it's in the tempstrings area, use a reserved range
	// (otherwise we'd get millions of useless string offsets cluttering the database)
	if (s >= (char *)vm_tempstringsbuf.data && s < (char *)vm_tempstringsbuf.data + vm_tempstringsbuf.maxsize)
		return vm.prog->stringssize + (s - (char *)vm_tempstringsbuf.data);
	// see if it's a known string address
	for (i = 0;i < vm.prog->numknownstrings;i++)
		if (vm.prog->knownstrings[i] == s)
			return -1 - i;
	// new unknown engine string
	MsgDev(D_STRING, "new engine string %p\n", s );
	for (i = vm.prog->firstfreeknownstring;i < vm.prog->numknownstrings;i++)
		if (!vm.prog->knownstrings[i])
			break;
	if (i >= vm.prog->numknownstrings)
	{
		if (i >= vm.prog->maxknownstrings)
		{
			const char **oldstrings = vm.prog->knownstrings;
			const byte *oldstrings_freeable = vm.prog->knownstrings_freeable;
			vm.prog->maxknownstrings += 128;
			vm.prog->knownstrings = (const char **)PRVM_Alloc(vm.prog->maxknownstrings * sizeof(char *));
			vm.prog->knownstrings_freeable = (byte *)PRVM_Alloc(vm.prog->maxknownstrings * sizeof(byte));
			if (vm.prog->numknownstrings)
			{
				Mem_Copy((char **)vm.prog->knownstrings, oldstrings, vm.prog->numknownstrings * sizeof(char *));
				Mem_Copy((char **)vm.prog->knownstrings_freeable, oldstrings_freeable, vm.prog->numknownstrings * sizeof(byte));
			}
		}
		vm.prog->numknownstrings++;
	}
	vm.prog->firstfreeknownstring = i + 1;
	vm.prog->knownstrings[i] = s;
	return -1 - i;
}

// temp string handling

// all tempstrings go into this buffer consecutively, and it is reset
// whenever PRVM_ExecuteProgram returns to the engine
// (technically each PRVM_ExecuteProgram call saves the cursize value and
//  restores it on return, so multiple recursive calls can share the same
//  buffer)
// the buffer size is automatically grown as needed

int PRVM_SetTempString( const char *s )
{
	int size;
	char *t;

	if (!s) return 0;

	size = (int)com.strlen(s) + 1;
	MsgDev( D_STRING, "PRVM_SetTempString: cursize %i, size %i\n", vm_tempstringsbuf.cursize, size);
	if (vm_tempstringsbuf.maxsize < vm_tempstringsbuf.cursize + size)
	{
		size_t old_maxsize = vm_tempstringsbuf.maxsize;
		if( vm_tempstringsbuf.cursize + size >= 1<<28 )
			PRVM_ERROR("PRVM_SetTempString: ran out of tempstring memory!  (refusing to grow tempstring buffer over 256MB, cursize %i, size %i)\n", vm_tempstringsbuf.cursize, size);
		vm_tempstringsbuf.maxsize = max( vm_tempstringsbuf.maxsize, 65536 );
		while( vm_tempstringsbuf.maxsize < vm_tempstringsbuf.cursize + size )
			vm_tempstringsbuf.maxsize *= 2;
		if (vm_tempstringsbuf.maxsize != old_maxsize || vm_tempstringsbuf.data == NULL)
		{
			MsgDev( D_NOTE, "PRVM_SetTempString: enlarging tempstrings buffer (%iKB -> %iKB)\n", old_maxsize/1024, vm_tempstringsbuf.maxsize/1024);
			vm_tempstringsbuf.data = Mem_Realloc( vm.prog->progs_mempool, vm_tempstringsbuf.data, vm_tempstringsbuf.maxsize );
		}
	}
	t = (char *)vm_tempstringsbuf.data + vm_tempstringsbuf.cursize;
	Mem_Copy( t, s, size );
	vm_tempstringsbuf.cursize += size;
	return PRVM_SetEngineString( t );
}

int PRVM_AllocString(size_t bufferlength, char **pointer)
{
	int i;
	if (!bufferlength)
		return 0;
	for (i = vm.prog->firstfreeknownstring;i < vm.prog->numknownstrings;i++)
		if (!vm.prog->knownstrings[i])
			break;
	if (i >= vm.prog->numknownstrings)
	{
		if (i >= vm.prog->maxknownstrings)
		{
			const char **oldstrings = vm.prog->knownstrings;
			const byte *oldstrings_freeable = vm.prog->knownstrings_freeable;
			vm.prog->maxknownstrings += 128;
			vm.prog->knownstrings = (const char **)PRVM_Alloc(vm.prog->maxknownstrings * sizeof(char *));
			vm.prog->knownstrings_freeable = (byte *)PRVM_Alloc(vm.prog->maxknownstrings * sizeof(byte));
			if (vm.prog->numknownstrings)
			{
				Mem_Copy((char **)vm.prog->knownstrings, oldstrings, vm.prog->numknownstrings * sizeof(char *));
				Mem_Copy((char **)vm.prog->knownstrings_freeable, oldstrings_freeable, vm.prog->numknownstrings * sizeof(byte));
			}
		}
		vm.prog->numknownstrings++;
	}
	vm.prog->firstfreeknownstring = i + 1;
	vm.prog->knownstrings[i] = (char *)PRVM_Alloc(bufferlength);
	vm.prog->knownstrings_freeable[i] = true;
	if (pointer)
		*pointer = (char *)(vm.prog->knownstrings[i]);
	return -1 - i;
}

void PRVM_FreeString(int num)
{
	if (num == 0)
		PRVM_ERROR("PRVM_FreeString: attempt to free a NULL string");
	else if (num >= 0 && num < vm.prog->stringssize)
		PRVM_ERROR("PRVM_FreeString: attempt to free a constant string");
	else if (num < 0 && num >= -vm.prog->numknownstrings)
	{
		num = -1 - num;
		if (!vm.prog->knownstrings[num])
			PRVM_ERROR("PRVM_FreeString: attempt to free a non-existent or already freed string");
		if (!vm.prog->knownstrings[num])
			PRVM_ERROR("PRVM_FreeString: attempt to free a string owned by the engine");
		PRVM_Free((char *)vm.prog->knownstrings[num]);
		vm.prog->knownstrings[num] = NULL;
		vm.prog->knownstrings_freeable[num] = false;
		vm.prog->firstfreeknownstring = min(vm.prog->firstfreeknownstring, num);
	}
	else
		PRVM_ERROR("PRVM_FreeString: invalid string offset %i", num);
}

