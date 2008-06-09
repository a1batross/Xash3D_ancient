// AK
// Basically every vm builtin cmd should be in here.
// All 3 builtin and extension lists can be found here
// cause large (I think they will) parts are from pr_cmds the same copyright like in pr_cmds
// also applies here

#include "engine.h"
#include "server.h"
#include "client.h"

//============================================================================
// Common

// temp string handling
// LordHavoc: added this to semi-fix the problem of using many ftos calls in a print
static char vm_string_temp[VM_STRINGTEMP_BUFFERS][VM_STRINGTEMP_LENGTH];
static int vm_string_tempindex = 0;
extern render_exp_t	*re;

// TODO: (move vm_files and vm_fssearchlist to prvm_prog_t struct)
// TODO: move vm_files and vm_fssearchlist back [9/13/2006 Black]
char *VM_GetTempString(void)
{
	char *s;
	s = vm_string_temp[vm_string_tempindex];
	vm_string_tempindex = (vm_string_tempindex + 1) % VM_STRINGTEMP_BUFFERS;
	return s;
}

void VM_CheckEmptyString (const char *s)
{
	if (s[0] <= ' ')
		PRVM_ERROR ("%s: Bad string", PRVM_NAME);
}

//============================================================================
//BUILT-IN FUNCTIONS

void VM_VarString(int first, char *out, int outlength)
{
	int i;
	const char *s;
	char *outend;

	outend = out + outlength - 1;
	for (i = first;i < prog->argc && out < outend;i++)
	{
		s = PRVM_G_STRING((OFS_PARM0+i*3));
		while (out < outend && *s)
			*out++ = *s++;
	}
	*out++ = 0;
}

/*
=================
VM_checkextension

returns true if the extension is supported by the server

checkextension(extensionname)
=================
*/

// kind of helper function
static bool checkextension(const char *name)
{
	int len;
	char *e, *start;
	len = (int)strlen(name);

	for (e = prog->extensionstring;*e;e++)
	{
		while (*e == ' ')
			e++;
		if (!*e)
			break;
		start = e;
		while (*e && *e != ' ')
			e++;
		if ((e - start) == len && !strncasecmp(start, name, len))
			return true;
	}
	return false;
}

void VM_checkextension (void)
{
	VM_SAFEPARMCOUNT(1,VM_checkextension);

	PRVM_G_FLOAT(OFS_RETURN) = checkextension(PRVM_G_STRING(OFS_PARM0));
}

/*
=================
VM_error

This is a TERMINAL error, which will kill off the entire prog.
Dumps self.

error(value)
=================
*/
void VM_error (void)
{
	edict_t	*ed;
	char string[VM_STRINGTEMP_LENGTH];

	VM_VarString(0, string, sizeof(string));
	Msg("======%s ERROR in %s:\n%s\n", PRVM_NAME, PRVM_GetString(prog->xfunction->s_name), string);
	if(prog->pev)
	{
		ed = PRVM_G_EDICT(prog->pev->ofs);
		PRVM_ED_Print(ed);
	}
	Host_Error("%s: Program error in function %s:\n%s\nTip: read above for entity information\n", PRVM_NAME, PRVM_GetString(prog->xfunction->s_name), string);
}

/*
=================
VM_objerror

Dumps out self, then an error message.  The program is aborted and self is
removed, but the level can continue.

objerror(value)
=================
*/
void VM_objerror (void)
{
	edict_t	*ed;
	char string[VM_STRINGTEMP_LENGTH];

	VM_VarString(0, string, sizeof(string));
	Msg("======OBJECT ERROR======\n", PRVM_NAME, PRVM_GetString(prog->xfunction->s_name), string);
	if(prog->pev)
	{
		ed = PRVM_G_EDICT (prog->pev->ofs);
		PRVM_ED_Print(ed);

		PRVM_ED_Free (ed);
	}
	else
		// objerror has to display the object progs -> else call
		PRVM_ERROR ("VM_objecterror: pev not defined !");
	Msg("%s OBJECT ERROR in %s:\n%s\nTip: read above for entity information\n", PRVM_NAME, PRVM_GetString(prog->xfunction->s_name), string);
}

/*
=================
VM_print (actually used only by client and menu)

print to console

print(string)
=================
*/
void VM_print (void)
{
	char string[VM_STRINGTEMP_LENGTH];

	VM_VarString(0, string, sizeof(string));
	Msg(string);
}

/*
=================
VM_bprint

broadcast print to everyone on server

bprint(...[string])
=================
*/
void VM_bprint (void)
{
	char string[VM_STRINGTEMP_LENGTH];

	VM_VarString(0, string, sizeof(string));
       
	if(sv.state == ss_loading)
	{
		Msg( string );
		return;
	}
	SV_BroadcastPrintf(PRINT_HIGH, string);
}

/*
=================
VM_sprint (menu & client but only if server.active == true)

single print to a specific client

sprint(float clientnum,...[string])
=================
*/
void VM_sprint( void )
{
	int	num;
	char	string[VM_STRINGTEMP_LENGTH];

	//find client for this entity
	num = (int)PRVM_G_FLOAT(OFS_PARM0);
	if (sv.state != ss_active || num < 0 || num >= host.maxclients || svs.clients[num].state != cs_spawned)
	{
		VM_Warning("VM_sprint: %s: invalid client or server is not active !\n", PRVM_NAME);
		return;
	}

	VM_VarString(1, string, sizeof(string));
	SV_ClientPrintf (svs.clients+(num - 1), PRINT_HIGH, "%s", string );
}

/*
=================
VM_centerprint

single print to the screen

centerprint(clientent, value)
=================
*/
void VM_centerprint( void )
{
	char string[VM_STRINGTEMP_LENGTH];

	VM_VarString(0, string, sizeof(string));
	CG_CenterPrint( string, SCREEN_HEIGHT/2, BIGCHAR_WIDTH );
}

void VM_servercmd (void)
{
	char string[VM_STRINGTEMP_LENGTH];

	VM_VarString(0, string, sizeof(string));
	SV_BroadcastCommand( string );
}

/*
=========
VM_clientcmd (used by client and menu)

clientcommand(float client, string s) (for client and menu)
=========
*/
void VM_clientcmd (void)
{
	client_state_t	*temp_client;
	int		i;

	VM_SAFEPARMCOUNT(2, VM_clientcmd);

	i = (int)PRVM_G_FLOAT(OFS_PARM0);
	if (sv.state != ss_active  || i < 0 || i >= maxclients->value || svs.clients[i].state != cs_spawned)
	{
		VM_Warning("VM_clientcommand: %s: invalid client/server is not active !\n", PRVM_NAME);
		return;
	}

	temp_client = sv_client;
	sv_client = svs.clients + i;
	Cmd_ExecuteString (PRVM_G_STRING(OFS_PARM1));
	sv_client = temp_client;
}


/*
=================
VM_normalize

vector normalize(vector)
=================
*/
void VM_normalize (void)
{
	float	*value1;
	vec3_t	newvalue;
	double	f;

	VM_SAFEPARMCOUNT(1,VM_normalize);

	value1 = PRVM_G_VECTOR(OFS_PARM0);

	f = DotProduct(value1, value1);
	if (f)
	{
		f = 1.0 / sqrt(f);
		VectorScale(value1, f, newvalue);
	}
	else VectorClear(newvalue);
	VectorCopy (newvalue, PRVM_G_VECTOR(OFS_RETURN));
}

/*
=================
VM_veclength

scalar veclength(vector)
=================
*/
void VM_veclength (void)
{
	VM_SAFEPARMCOUNT(1, VM_veclength);
	PRVM_G_FLOAT(OFS_RETURN) = VectorLength(PRVM_G_VECTOR(OFS_PARM0));
}

/*
=================
VM_vectoyaw

float vectoyaw(vector)
=================
*/
void VM_vectoyaw (void)
{
	float	*value1;
	float	yaw;

	VM_SAFEPARMCOUNT(1, VM_vectoyaw);

	value1 = PRVM_G_VECTOR(OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0) yaw = 0;
	else
	{
		yaw = (int) (atan2(value1[1], value1[0]) * 180 / M_PI);
		if (yaw < 0) yaw += 360;
	}

	PRVM_G_FLOAT(OFS_RETURN) = yaw;
}


/*
=================
VM_vectoangles

vector vectoangles(vector)
=================
*/
void VM_vectoangles (void)
{
	float	*value1;
	float	forward;
	float	yaw, pitch;

	VM_SAFEPARMCOUNT(1, VM_vectoangles);

	value1 = PRVM_G_VECTOR(OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
	{
		yaw = 0;
		if (value1[2] > 0) pitch = 90;
		else pitch = 270;
	}
	else
	{
		// LordHavoc: optimized a bit
		if (value1[0])
		{
			yaw = (atan2(value1[1], value1[0]) * 180 / M_PI);
			if (yaw < 0) yaw += 360;
		}
		else if (value1[1] > 0) yaw = 90;
		else yaw = 270;

		forward = sqrt(value1[0]*value1[0] + value1[1]*value1[1]);
		pitch = (atan2(value1[2], forward) * 180 / M_PI);
		if (pitch < 0) pitch += 360;
	}

	PRVM_G_FLOAT(OFS_RETURN+0) = pitch; //FIXME: should inverse ?
	PRVM_G_FLOAT(OFS_RETURN+1) = yaw;
	PRVM_G_FLOAT(OFS_RETURN+2) = 0;
}

/*
=================
VM_random

Returns a number in specified range

float random_long( float min, float max)
float random_float( float min, float max)
=================
*/
void VM_random_long (void)
{
	VM_SAFEPARMCOUNT(2, VM_random_long);
	PRVM_G_FLOAT(OFS_RETURN) = RANDOM_LONG(PRVM_G_FLOAT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1));
}

void VM_random_float (void)
{
	VM_SAFEPARMCOUNT(2, VM_random_float);
	PRVM_G_FLOAT(OFS_RETURN) = RANDOM_FLOAT(PRVM_G_FLOAT(OFS_PARM0), PRVM_G_FLOAT(OFS_PARM1));
}


/*
=================
PF_sound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.

=================
*/
/*
void PF_sound (void)
{
	char		*sample;
	int			channel;
	edict_t		*entity;
	int 		volume;
	float attenuation;

	entity = PRVM_G_EDICT(OFS_PARM0);
	channel = PRVM_G_FLOAT(OFS_PARM1);
	sample = PRVM_G_STRING(OFS_PARM2);
	volume = PRVM_G_FLOAT(OFS_PARM3) * 255;
	attenuation = PRVM_G_FLOAT(OFS_PARM4);

	if (volume < 0 || volume > 255)
		Host_Error ("SV_StartSound: volume = %i", volume);

	if (attenuation < 0 || attenuation > 4)
		Host_Error ("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 7)
		Host_Error ("SV_StartSound: channel = %i", channel);

	SV_StartSound (entity, channel, sample, volume, attenuation);
}
*/

/*
=========
VM_localsound

localsound(string sample)
=========
*/
void VM_localsound(void)
{
	const char *s;

	VM_SAFEPARMCOUNT(1, VM_localsound);

	s = PRVM_G_STRING(OFS_PARM0);

	if(!S_StartLocalSound(s))
	{
		PRVM_G_FLOAT(OFS_RETURN) = -4;
		VM_Warning("VM_localsound: Failed to play %s for %s !\n", s, PRVM_NAME);
		return;
	}

	PRVM_G_FLOAT(OFS_RETURN) = 1;
}

/*
=================
VM_break

break()
=================
*/
void VM_break (void)
{
	PRVM_ERROR ("%s: break statement", PRVM_NAME);
}

//============================================================================

/*
=================
VM_cvar

float cvar (string)
=================
*/
void VM_cvar (void)
{
	VM_SAFEPARMCOUNT(1,VM_cvar);

	PRVM_G_FLOAT(OFS_RETURN) = Cvar_VariableValue(PRVM_G_STRING(OFS_PARM0));
}

/*
=================
VM_cvar_string

const string	VM_cvar_string (string)
=================
*/
void VM_cvar_string(void)
{
	char *out;
	const char *name;
	const char *cvar_string;
	VM_SAFEPARMCOUNT(1,VM_cvar_string);

	name = PRVM_G_STRING(OFS_PARM0);

	if(!name)
		PRVM_ERROR("VM_cvar_string: %s: null string", PRVM_NAME);

	VM_CheckEmptyString(name);

	out = VM_GetTempString();

	cvar_string = Cvar_VariableString(name);

	strncpy(out, cvar_string, VM_STRINGTEMP_LENGTH);

	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(out);
}

/*
=================
VM_cvar_set

void cvar_set (string,string)
=================
*/
void VM_cvar_set (void)
{
	VM_SAFEPARMCOUNT(2,VM_cvar_set);

	Cvar_Set(PRVM_G_STRING(OFS_PARM0), PRVM_G_STRING(OFS_PARM1));
}

/*
=========
VM_wprint

wprint(...[string])
=========
*/
void VM_wprint (void)
{
	char string[VM_STRINGTEMP_LENGTH];
	if (host.debug)
	{
		VM_VarString(0, string, sizeof(string));
		Msg("%s", string);
	}
}

/*
=========
VM_ftoa

string ftoa(float)
=========
*/

void VM_ftoa (void)
{
	float v;
	char *s;

	VM_SAFEPARMCOUNT(1, VM_ftoa);

	v = PRVM_G_FLOAT(OFS_PARM0);

	s = VM_GetTempString();
	if ((float)((int)v) == v)
		sprintf(s, "%i", (int)v);
	else sprintf(s, "%f", v);

	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(s);
}

/*
=========
VM_vtoa

string vtoa(vector)
=========
*/

void VM_vtoa (void)
{
	char *s;

	VM_SAFEPARMCOUNT(1,VM_vtoa);

	s = VM_GetTempString();
	sprintf (s, "'%5.1f %5.1f %5.1f'", PRVM_G_VECTOR(OFS_PARM0)[0], PRVM_G_VECTOR(OFS_PARM0)[1], PRVM_G_VECTOR(OFS_PARM0)[2]);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(s);
}

/*
=========
VM_etos

string	etos(entity)
=========
*/

void VM_etos (void)
{
	char *s;

	VM_SAFEPARMCOUNT(1, VM_etos);

	s = VM_GetTempString();
	sprintf (s, "entity %i", PRVM_G_EDICTNUM(OFS_PARM0));
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(s);
}

/*
=========
VM_stof

float atof(...[string])
=========
*/
void VM_atof(void)
{
	char string[VM_STRINGTEMP_LENGTH];
	VM_VarString(0, string, sizeof(string));
	PRVM_G_FLOAT(OFS_RETURN) = atof(string);
}

/*
========================
VM_itof

float itof(intt ent)
========================
*/
void VM_itof(void)
{
	VM_SAFEPARMCOUNT(1, VM_itof);
	PRVM_G_FLOAT(OFS_RETURN) = PRVM_G_INT(OFS_PARM0);
}

/*
========================
VM_ftoe

entity ftoe(float num)
========================
*/
void VM_ftoe(void)
{
	int ent;
	VM_SAFEPARMCOUNT(1, VM_ftoe);

	ent = (int)PRVM_G_FLOAT(OFS_PARM0);
	if (ent < 0 || ent >= MAX_EDICTS || PRVM_PROG_TO_EDICT(ent)->priv.ed->free)
		ent = 0; // return world instead of a free or invalid entity

	PRVM_G_INT(OFS_RETURN) = ent;
}

/*
=========
VM_create

entity create()
=========
*/

void VM_create (void)
{
	edict_t	*ed;
	prog->xfunction->builtinsprofile += 20;
	ed = PRVM_ED_Alloc();
	VM_RETURN_EDICT(ed);
}

/*
=========
VM_remove

remove(entity e)
=========
*/

void VM_remove (void)
{
	edict_t	*ed;
	prog->xfunction->builtinsprofile += 20;

	VM_SAFEPARMCOUNT(1, VM_remove);

	ed = PRVM_G_EDICT(OFS_PARM0);
	if( PRVM_NUM_FOR_EDICT(ed) <= prog->reserved_edicts )
	{
		if (host.developer >= D_INFO)
			VM_Warning( "VM_remove: tried to remove the null entity or a reserved entity!\n" );
	}
	else if( ed->priv.ed->free )
	{
		if (host.developer >= D_INFO)
			VM_Warning( "VM_remove: tried to remove an already freed entity!\n" );
	}
	else PRVM_ED_Free (ed);
}

/*
=========
VM_findchain

entity	findchain(.string field, string match)
=========
*/
// chained search for strings in entity fields
// entity(.string field, string match) findchain = #402;
void VM_findchain (void)
{
	int		i;
	int		f;
	int		chain_of;
	const char	*s, *t;
	edict_t	*ent, *chain;

	VM_SAFEPARMCOUNT(2,VM_findchain);

	// is the same like !(prog->flag & PRVM_FE_CHAIN) - even if the operator precedence is another
	if(!prog->flag & PRVM_FE_CHAIN)
		PRVM_ERROR("VM_findchain: %s doesnt have a chain field !", PRVM_NAME);

	chain_of = PRVM_ED_FindField("chain")->ofs;

	chain = prog->edicts;

	f = PRVM_G_INT(OFS_PARM0);
	s = PRVM_G_STRING(OFS_PARM1);

	// LordHavoc: apparently BloodMage does a find(world, weaponmodel, "") and
	// expects it to find all the monsters, so we must be careful to support
	// searching for ""
	if (!s)
		s = "";

	ent = PRVM_NEXT_EDICT(prog->edicts);
	for (i = 1;i < prog->num_edicts;i++, ent = PRVM_NEXT_EDICT(ent))
	{
		prog->xfunction->builtinsprofile++;
		if (ent->priv.ed->free)
			continue;
		t = PRVM_E_STRING(ent,f);
		if (!t)
			t = "";
		if (strcmp(t,s))
			continue;

		PRVM_E_INT(ent,chain_of) = PRVM_NUM_FOR_EDICT(chain);
		chain = ent;
	}

	VM_RETURN_EDICT(chain);
}

/*
=========
VM_findchainfloat

entity	findchainfloat(.string field, float match)
entity	findchainentity(.string field, entity match)
=========
*/
// LordHavoc: chained search for float, int, and entity reference fields
// entity(.string field, float match) findchainfloat = #403;
void VM_findchainfloat (void)
{
	int		i;
	int		f;
	int		chain_of;
	float	s;
	edict_t	*ent, *chain;

	VM_SAFEPARMCOUNT(2, VM_findchainfloat);

	if(!prog->flag & PRVM_FE_CHAIN)
		PRVM_ERROR("VM_findchainfloat: %s doesnt have a chain field !", PRVM_NAME);

	chain_of = PRVM_ED_FindField("chain")->ofs;

	chain = (edict_t *)prog->edicts;

	f = PRVM_G_INT(OFS_PARM0);
	s = PRVM_G_FLOAT(OFS_PARM1);

	ent = PRVM_NEXT_EDICT(prog->edicts);
	for (i = 1;i < prog->num_edicts;i++, ent = PRVM_NEXT_EDICT(ent))
	{
		prog->xfunction->builtinsprofile++;
		if (ent->priv.ed->free)
			continue;
		if (PRVM_E_FLOAT(ent,f) != s)
			continue;

		PRVM_E_INT(ent,chain_of) = PRVM_EDICT_TO_PROG(chain);
		chain = ent;
	}

	VM_RETURN_EDICT(chain);
}

/*
========================
VM_findflags

entity	findflags(entity start, .float field, float match)
========================
*/
// LordHavoc: search for flags in float fields
void VM_findflags (void)
{
	int		e;
	int		f;
	int		s;
	edict_t	*ed;

	VM_SAFEPARMCOUNT(3, VM_findflags);


	e = PRVM_G_EDICTNUM(OFS_PARM0);
	f = PRVM_G_INT(OFS_PARM1);
	s = (int)PRVM_G_FLOAT(OFS_PARM2);

	for (e++ ; e < prog->num_edicts ; e++)
	{
		prog->xfunction->builtinsprofile++;
		ed = PRVM_EDICT_NUM(e);
		if (ed->priv.ed->free)
			continue;
		if (!PRVM_E_FLOAT(ed,f))
			continue;
		if ((int)PRVM_E_FLOAT(ed,f) & s)
		{
			VM_RETURN_EDICT(ed);
			return;
		}
	}

	VM_RETURN_EDICT(prog->edicts);
}

/*
========================
VM_findchainflags

entity	findchainflags(.float field, float match)
========================
*/
// LordHavoc: chained search for flags in float fields
void VM_findchainflags (void)
{
	int		i;
	int		f;
	int		s;
	int		chain_of;
	edict_t	*ent, *chain;

	VM_SAFEPARMCOUNT(2, VM_findchainflags);

	if(!prog->flag & PRVM_FE_CHAIN)
		PRVM_ERROR("VM_findchainflags: %s doesnt have a chain field !", PRVM_NAME);

	chain_of = PRVM_ED_FindField("chain")->ofs;

	chain = (edict_t *)prog->edicts;

	f = PRVM_G_INT(OFS_PARM0);
	s = (int)PRVM_G_FLOAT(OFS_PARM1);

	ent = PRVM_NEXT_EDICT(prog->edicts);
	for (i = 1;i < prog->num_edicts;i++, ent = PRVM_NEXT_EDICT(ent))
	{
		prog->xfunction->builtinsprofile++;
		if (ent->priv.ed->free)
			continue;
		if (!PRVM_E_FLOAT(ent,f))
			continue;
		if (!((int)PRVM_E_FLOAT(ent,f) & s))
			continue;

		PRVM_E_INT(ent,chain_of) = PRVM_EDICT_TO_PROG(chain);
		chain = ent;
	}

	VM_RETURN_EDICT(chain);
}

/*
=========
VM_coredump

coredump()
=========
*/
void VM_coredump (void)
{
	VM_SAFEPARMCOUNT(0,VM_coredump);

	Cbuf_AddText("prvm_edicts ");
	Cbuf_AddText(PRVM_NAME);
	Cbuf_AddText("\n");
}

/*
=========
VM_stackdump

stackdump()
=========
*/
void VM_stackdump (void)
{
	VM_SAFEPARMCOUNT(0, VM_stackdump);

	PRVM_StackTrace();
}

/*
=========
VM_crash

crash()
=========
*/
void VM_crash(void)
{
	VM_SAFEPARMCOUNT(0, VM_crash);

	PRVM_ERROR("Crash called by %s",PRVM_NAME);
}

/*
=========
VM_traceon

traceon()
=========
*/
void VM_traceon (void)
{
	VM_SAFEPARMCOUNT(0,VM_traceon);

	prog->trace = true;
}

/*
=========
VM_traceoff

traceoff()
=========
*/
void VM_traceoff (void)
{
	VM_SAFEPARMCOUNT(0,VM_traceoff);

	prog->trace = false;
}

/*
=========
VM_eprint

eprint(entity e)
=========
*/
void VM_eprint (void)
{
	VM_SAFEPARMCOUNT(1,VM_eprint);

	PRVM_ED_Print(PRVM_G_EDICT(OFS_PARM0));
}

/*
=============
VM_nextent

entity	nextent(entity)
=============
*/
void VM_nextent( void )
{
	int		i;
	edict_t	*ent;

	i = PRVM_G_EDICTNUM(OFS_PARM0);
	while (1)
	{
		prog->xfunction->builtinsprofile++;
		i++;
		if (i == prog->num_edicts)
		{
			VM_RETURN_EDICT(prog->edicts);
			return;
		}
		ent = PRVM_EDICT_NUM(i);
		if (!ent->priv.ed->free)
		{
			VM_RETURN_EDICT(ent);
			return;
		}
	}
}

//=============================================================================

/*
==============
VM_changelevel
server and menu

changelevel(string map)
==============
*/
void VM_changelevel (void)
{
	const char	*s;

	VM_SAFEPARMCOUNT(1, VM_changelevel);

	if(sv.state != ss_active)
	{
		VM_Warning("VM_changelevel: game is not server (%s)\n", PRVM_NAME);
		return;
	}

	s = PRVM_G_STRING(OFS_PARM0);
	Cbuf_AddText (va("changelevel %s\n",s));
}

/*
=================
VM_randomvec

Returns a vector of length < 1 and > 0

vector randomvec()
=================
*/
void VM_randomvec (void)
{
	vec3_t		temp;

	VM_SAFEPARMCOUNT(0, VM_randomvec);

	//// WTF ??
	do
	{
		temp[0] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
		temp[1] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
		temp[2] = (rand()&32767) * (2.0 / 32767.0) - 1.0;
	} while (DotProduct(temp, temp) >= 1);
	VectorCopy (temp, PRVM_G_VECTOR(OFS_RETURN));
}

//=============================================================================

/*
=========
VM_registercvar

float	registercvar (string name, string value, float flags)
=========
*/
void VM_registercvar (void)
{
	const char *name, *value;
	int	flags;

	VM_SAFEPARMCOUNT(3,VM_registercvar);

	name = PRVM_G_STRING(OFS_PARM0);
	value = PRVM_G_STRING(OFS_PARM1);
	flags = (int)PRVM_G_FLOAT(OFS_PARM2);
	PRVM_G_FLOAT(OFS_RETURN) = 0;

	if(flags > CVAR_MAXFLAGSVAL)
		return;

	// first check to see if it has already been defined
	if (Cvar_FindVar (name))
		return;

	Cvar_Get(name, value, flags);
	PRVM_G_FLOAT(OFS_RETURN) = 1; // success
}

/*
=================
VM_copyentity

copies data from one entity to another

copyentity(entity src, entity dst)
=================
*/
void VM_copyentity (void)
{
	edict_t *in, *out;
	VM_SAFEPARMCOUNT(2,VM_copyentity);
	in = PRVM_G_EDICT(OFS_PARM0);
	out = PRVM_G_EDICT(OFS_PARM1);
	memcpy(out->progs.vp, in->progs.vp, prog->progs->entityfields * 4);
}

void VM_Files_Init(void)
{
	int i;
	for (i = 0;i < PRVM_MAX_OPENFILES;i++)
		prog->openfiles[i] = NULL;
}

void VM_Files_CloseAll( void )
{
	int	i;
	file_t	*f;

	for(i = 0; i < PRVM_MAX_OPENFILES; i++)
	{
		if (prog->openfiles[i])
		{
			f = VFS_Close(prog->openfiles[i]);
			FS_Close( f ); // close real file too
		}
		prog->openfiles[i] = NULL;
	}
}

vfile_t *VM_GetFileHandle( int index )
{
	if (index < 0 || index >= PRVM_MAX_OPENFILES)
	{
		Msg("VM_GetFileHandle: invalid file handle %i used in %s\n", index, PRVM_NAME);
		return NULL;
	}
	if (prog->openfiles[index] == NULL)
	{
		Msg("VM_GetFileHandle: no such file handle %i (or file has been closed) in %s\n", index, PRVM_NAME);
		return NULL;
	}
	return prog->openfiles[index];
}

/*
=========
VM_fopen

float	fopen(string filename, float mode)
=========
*/
// float(string filename, float mode) fopen = #110;
// opens a file inside quake/gamedir/data/ (mode is FILE_READ, FILE_APPEND, or FILE_WRITE),
// returns fhandle >= 0 if successful, or fhandle < 0 if unable to open file for any reason
void VM_fopen( void )
{
	int		filenum, mode;
	const char	*modestring, *filename;

	VM_SAFEPARMCOUNT(2, VM_fopen);

	for (filenum = 0; filenum < PRVM_MAX_OPENFILES; filenum++)
		if (prog->openfiles[filenum] == NULL)
			break;

	if (filenum >= PRVM_MAX_OPENFILES)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -2;
		VM_Warning("VM_fopen: %s ran out of file handles(%i)\n", PRVM_NAME, PRVM_MAX_OPENFILES);
		return;
	}
	mode = (int)PRVM_G_FLOAT(OFS_PARM1);
	switch(mode)
	{
	case 0: // FILE_READ
		modestring = "rb";
		break;
	case 1: // FILE_APPEND
		modestring = "ab";
		break;
	case 2: // FILE_WRITE
		modestring = "wb";
		break;
	case 3: // FILE_WRITE_DEFLATED
		modestring = "wz";
		break;
	default:
		PRVM_G_FLOAT(OFS_RETURN) = -3;
		VM_Warning("VM_fopen: %s: no such mode %i (valid: 0 = read, 1 = append, 2 = write, 3 = deflate)\n", PRVM_NAME, mode);
		return;
	}
	filename = PRVM_G_STRING(OFS_PARM0);

	prog->openfiles[filenum] = VFS_Open(FS_Open(va("temp/%s", filename), modestring), modestring );
	if (prog->openfiles[filenum] == NULL && mode == 0)
		prog->openfiles[filenum] = VFS_Open(FS_Open(va("%s", filename), modestring), modestring );

	if (prog->openfiles[filenum] == NULL)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -1;
		if(host.developer >= D_WARN)
			VM_Warning("VM_fopen: %s: %s mode %s failed\n", PRVM_NAME, filename, modestring);
	}
	else
	{
		PRVM_G_FLOAT(OFS_RETURN) = filenum;
		if(host.developer >= D_WARN)
			Msg("VM_fopen: %s: %s mode %s opened as #%i\n", PRVM_NAME, filename, modestring, filenum);
	}
}

/*
=========
VM_fclose

fclose(float fhandle)
=========
*/
//void(float fhandle) fclose = #111; // closes a file
void VM_fclose( void )
{
	int filenum;

	VM_SAFEPARMCOUNT(1,VM_fclose);

	filenum = (int)PRVM_G_FLOAT(OFS_PARM0);
	if (filenum < 0 || filenum >= PRVM_MAX_OPENFILES)
	{
		VM_Warning("VM_fclose: invalid file handle %i used in %s\n", filenum, PRVM_NAME);
		return;
	}
	if (prog->openfiles[filenum] == NULL)
	{
		VM_Warning("VM_fclose: no such file handle %i (or file has been closed) in %s\n", filenum, PRVM_NAME);
		return;
	}
	FS_Close(VFS_Close(prog->openfiles[filenum]));
	prog->openfiles[filenum] = NULL;
	if (host.developer >= D_WARN)
	{
		Msg("VM_fclose: %s: #%i closed\n", PRVM_NAME, filenum);
	}
}

/*
=========
VM_fgets

string	fgets(float fhandle)
=========
*/
//string(float fhandle) fgets = #112; // reads a line of text from the file and returns as a tempstring
void VM_fgets(void)
{
	int c;
	static char string[VM_STRINGTEMP_LENGTH];
	int filenum;

	VM_SAFEPARMCOUNT(1, VM_fgets);

	filenum = (int)PRVM_G_FLOAT(OFS_PARM0);
	if (filenum < 0 || filenum >= PRVM_MAX_OPENFILES)
	{
		VM_Warning("VM_fgets: invalid file handle %i used in %s\n", filenum, PRVM_NAME);
		return;
	}
	if (prog->openfiles[filenum] == NULL)
	{
		VM_Warning("VM_fgets: no such file handle %i (or file has been closed) in %s\n", filenum, PRVM_NAME);
		return;
	}

	c = VFS_Gets(prog->openfiles[filenum], string, VM_STRINGTEMP_LENGTH );

	if (host.developer >= D_WARN) Msg("fgets: %s: %s\n", PRVM_NAME, string);

	if (c >= 0) PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(string);
	else PRVM_G_INT(OFS_RETURN) = 0;
}

/*
=========
VM_fputs

fputs(float fhandle, string s)
=========
*/
//void(float fhandle, string s) fputs = #113; // writes a line of text to the end of the file
void VM_fputs(void)
{
	int stringlength;
	char string[VM_STRINGTEMP_LENGTH];
	int filenum;

	VM_SAFEPARMCOUNT(2, VM_fputs);

	filenum = (int)PRVM_G_FLOAT(OFS_PARM0);
	if (filenum < 0 || filenum >= PRVM_MAX_OPENFILES)
	{
		VM_Warning("VM_fputs: invalid file handle %i used in %s\n", filenum, PRVM_NAME);
		return;
	}
	if (prog->openfiles[filenum] == NULL)
	{
		VM_Warning("VM_fputs: no such file handle %i (or file has been closed) in %s\n", filenum, PRVM_NAME);
		return;
	}
	VM_VarString(1, string, sizeof(string));
	if ((stringlength = (int)strlen(string))) VFS_Write(prog->openfiles[filenum], string, stringlength);
	if (host.developer >= D_WARN) Msg("fputs: %s: %s\n", PRVM_NAME, string);
}

/*
=========
VM_strlen

float	strlen(string s)
=========
*/
//float(string s) strlen = #114; // returns how many characters are in a string
void VM_strlen(void)
{
	const char *s;

	VM_SAFEPARMCOUNT(1,VM_strlen);

	s = PRVM_G_STRING(OFS_PARM0);
	if (s)
		PRVM_G_FLOAT(OFS_RETURN) = strlen(s);
	else
		PRVM_G_FLOAT(OFS_RETURN) = 0;
}

/*
=========
VM_strcat

string strcat(string,string,...[string])
=========
*/
//string(string s1, string s2) strcat = #115;
// concatenates two strings (for example "abc", "def" would return "abcdef")
// and returns as a tempstring
void VM_strcat(void)
{
	char *s;

	if(prog->argc < 1)
		PRVM_ERROR("VM_strcat wrong parameter count (min. 1 expected ) !");

	s = VM_GetTempString();
	VM_VarString(0, s, VM_STRINGTEMP_LENGTH);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(s);
}

/*
=========
VM_substring

string	substring(string s, float start, float length)
=========
*/
// string(string s, float start, float length) substring = #116;
// returns a section of a string as a tempstring
void VM_substring(void)
{
	int i, start, length;
	const char *s;
	char *string;

	VM_SAFEPARMCOUNT(3,VM_substring);

	string = VM_GetTempString();
	s = PRVM_G_STRING(OFS_PARM0);
	start = (int)PRVM_G_FLOAT(OFS_PARM1);
	length = (int)PRVM_G_FLOAT(OFS_PARM2);
	if (!s)
		s = "";
	for (i = 0;i < start && *s;i++, s++);
	for (i = 0;i < VM_STRINGTEMP_LENGTH - 1 && *s && i < length;i++, s++)
		string[i] = *s;
	string[i] = 0;
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(string);
}

/*
=========
VM_atov

vector	atov(string s)
=========
*/
void VM_atov(void)
{
	char string[VM_STRINGTEMP_LENGTH];
	vec3_t		out;
	const char	*s;
	int		i;

	VM_SAFEPARMCOUNT(1,VM_stov);

	VM_VarString(0, string, sizeof(string));
	
	s = string;
	VectorClear( out );

	if (*s == '\'') s++;
	for (i = 0;i < 3;i++)
	{
		while (*s == ' ' || *s == '\t') s++;
		out[i] = atof(s);
		if (out[i] == 0 && *s != '-' && *s != '+' && (*s < '0' || *s > '9'))
			break; // not a number
		while (*s && *s != ' ' && *s !='\t' && *s != '\'') s++;
		if (*s == '\'') break;
	}
	VectorCopy(out, PRVM_G_VECTOR(OFS_RETURN));
}

/*
=========
VM_allocstring

makes a copy of a string into the string zone and returns it, this is often used to keep around a tempstring 
for longer periods of time (tempstrings are replaced often)

string	AllocString(string s)
=========
*/
void VM_allocstring(void)
{
	char *out;
	char string[VM_STRINGTEMP_LENGTH];
	size_t alloclen;

	VM_SAFEPARMCOUNT(1, VM_allocstring);

	VM_VarString(0, string, sizeof(string));
	alloclen = strlen(string) + 1;
	PRVM_G_INT(OFS_RETURN) = PRVM_AllocString(alloclen, &out);
	Mem_Copy(out, string, alloclen);
}

/*
=========
VM_freestring

removes a copy of a string from the string zone 
you can not use that string again or it may crash!!!

void FreeString(string s)
=========
*/
void VM_freestring(void)
{
	VM_SAFEPARMCOUNT(1, VM_freestring);
	PRVM_FreeString(PRVM_G_INT(OFS_PARM0));
}

/*
=========
VM_tokenize

float tokenize(string s)
=========
*/
//float(string s) tokenize = #441; // takes apart a string into individal words (access them with argv), returns how many
//this function originally written by KrimZon, made shorter by LordHavoc
//20040203: rewritten by LordHavoc (no longer uses allocations)
int num_tokens = 0;
char *tokens[256], tokenbuf[MAX_INPUTLINE];
void VM_tokenize (void)
{
	size_t pos;
	const char *p;

	VM_SAFEPARMCOUNT(1, VM_tokenize);

	p = PRVM_G_STRING(OFS_PARM0);

	num_tokens = 0;
	pos = 0;

	while(Com_ParseToken(&p))
	{
		size_t tokenlen;
		if (num_tokens >= (int)(sizeof(tokens)/sizeof(tokens[0])))
			break;
		tokenlen = strlen(com_token) + 1;
		if (pos + tokenlen > sizeof(tokenbuf))
			break;
		tokens[num_tokens++] = tokenbuf + pos;
		Mem_Copy(tokenbuf + pos, com_token, tokenlen);
		pos += tokenlen;
	}

	PRVM_G_FLOAT(OFS_RETURN) = num_tokens;
}

//string(float n) argv = #442; // returns a word from the tokenized string (returns nothing for an invalid index)
//this function originally written by KrimZon, made shorter by LordHavoc
void VM_argv (void)
{
	int token_num;

	VM_SAFEPARMCOUNT(1, VM_argv);

	token_num = (int)PRVM_G_FLOAT(OFS_PARM0);

	if (token_num >= 0 && token_num < Cmd_Argc())
		PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(Cmd_Argv(token_num));
	else PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(NULL);
}

/*
//void(entity e, entity tagentity, string tagname) setattachment = #443; // attachs e to a tag on tagentity (note: use "" to attach to entity origin/angles instead of a tag)
void PF_setattachment (void)
{
	edict_t *e = PRVM_G_EDICT(OFS_PARM0);
	edict_t *tagentity = PRVM_G_EDICT(OFS_PARM1);
	char *tagname = PRVM_G_STRING(OFS_PARM2);
	prvm_eval_t *v;
	int i, modelindex;
	model_t *model;

	if (tagentity == NULL)
		tagentity = prog->edicts;

	v = PRVM_GETEDICTFIELDVALUE(e, eval_tag_entity);
	if (v)
		fields.server->edict = PRVM_EDICT_TO_PROG(tagentity);

	v = PRVM_GETEDICTFIELDVALUE(e, eval_tag_index);
	if (v)
		fields.server->_float = 0;
	if (tagentity != NULL && tagentity != prog->edicts && tagname && tagname[0])
	{
		modelindex = (int)tagentity->progs.server->modelindex;
		if (modelindex >= 0 && modelindex < MAX_MODELS)
		{
			model = sv.models[modelindex];
			if (model->data_overridetagnamesforskin && (unsigned int)tagentity->progs.server->skin < (unsigned int)model->numskins && model->data_overridetagnamesforskin[(unsigned int)tagentity->progs.server->skin].num_overridetagnames)
				for (i = 0;i < model->data_overridetagnamesforskin[(unsigned int)tagentity->progs.server->skin].num_overridetagnames;i++)
					if (!strcmp(tagname, model->data_overridetagnamesforskin[(unsigned int)tagentity->progs.server->skin].data_overridetagnames[i].name))
						fields.server->_float = i + 1;
			// FIXME: use a model function to get tag info (need to handle skeletal)
			if (fields.server->_float == 0 && model->num_tags)
				for (i = 0;i < model->num_tags;i++)
					if (!strcmp(tagname, model->data_tags[i].name))
						fields.server->_float = i + 1;
			if (fields.server->_float == 0)
				MsgDev("setattachment(edict %i, edict %i, string \"%s\"): tried to find tag named \"%s\" on entity %i (model \"%s\") but could not find it\n", PRVM_NUM_FOR_EDICT(e), PRVM_NUM_FOR_EDICT(tagentity), tagname, tagname, PRVM_NUM_FOR_EDICT(tagentity), model->name);
		}
		else
			MsgDev("setattachment(edict %i, edict %i, string \"%s\"): tried to find tag named \"%s\" on entity %i but it has no model\n", PRVM_NUM_FOR_EDICT(e), PRVM_NUM_FOR_EDICT(tagentity), tagname, tagname, PRVM_NUM_FOR_EDICT(tagentity));
	}
}*/

/*
=========
VM_isserver

float	isserver()
=========
*/
void VM_isserver(void)
{
	VM_SAFEPARMCOUNT(0,VM_serverstate);

	PRVM_G_FLOAT(OFS_RETURN) = (sv.state == ss_active) ? true : false;
}

/*
=========
VM_clientcount

float	clientcount()
=========
*/
void VM_clientcount(void)
{
	VM_SAFEPARMCOUNT(0,VM_clientcount);

	PRVM_G_FLOAT(OFS_RETURN) = host.maxclients;
}

/*
=========
VM_clientstate

float	clientstate()
=========
*/
void VM_clientstate(void)
{
	VM_SAFEPARMCOUNT(0,VM_clientstate);

	PRVM_G_FLOAT(OFS_RETURN) = cls.state;
}

/*
=========
VM_getmousepos

vector	getmousepos()
=========
*/
void VM_getmousepos(void)
{
	VM_SAFEPARMCOUNT(0, VM_getmousepos);

	PRVM_G_VECTOR(OFS_RETURN)[0] = mouse_x;
	PRVM_G_VECTOR(OFS_RETURN)[1] = mouse_y;
	PRVM_G_VECTOR(OFS_RETURN)[2] = 0;
}

/*
=========
VM_gettime

float	gettime(void)
=========
*/
void VM_gettime(void)
{
	VM_SAFEPARMCOUNT(0,VM_gettime);

	PRVM_G_FLOAT(OFS_RETURN) = (float) *prog->time;
}

/*
========================
VM_parseentitydata

parseentitydata(entity ent, string data)
========================
*/
void VM_parseentitydata(void)
{
	edict_t *ent;
	const char *data;

	VM_SAFEPARMCOUNT(2, VM_parseentitydata);

    // get edict and test it
	ent = PRVM_G_EDICT(OFS_PARM0);
	if (ent->priv.ed->free)
		PRVM_ERROR ("VM_parseentitydata: %s: Can only set already spawned entities (entity %i is free)!", PRVM_NAME, PRVM_NUM_FOR_EDICT(ent));

	data = PRVM_G_STRING(OFS_PARM1);

    	// parse the opening brace
	if (!Com_ParseToken(&data) || com_token[0] != '{' )
		PRVM_ERROR ("VM_parseentitydata: %s: Couldn't parse entity data:\n%s", PRVM_NAME, data );

	PRVM_ED_ParseEdict (data, ent);
}

void VM_Search_Init(void)
{
	int i;
	for (i = 0;i < PRVM_MAX_OPENSEARCHES;i++)
		prog->opensearches[i] = NULL;
}

void VM_Search_Reset(void)
{
	int i;
	// reset the fssearch list
	for(i = 0; i < PRVM_MAX_OPENSEARCHES; i++)
	{
		if(prog->opensearches[i])
			Mem_Free(prog->opensearches[i]);
		prog->opensearches[i] = NULL;
	}
}

/*
=========
VM_search_begin

float search_begin(string pattern, float caseinsensitive, float quiet)
=========
*/
void VM_search_begin(void)
{
	int handle;
	const char *pattern;
	int caseinsens, quiet;

	VM_SAFEPARMCOUNT(3, VM_search_begin);

	pattern = PRVM_G_STRING(OFS_PARM0);

	VM_CheckEmptyString(pattern);

	caseinsens = (int)PRVM_G_FLOAT(OFS_PARM1);
	quiet = (int)PRVM_G_FLOAT(OFS_PARM2);

	for(handle = 0; handle < PRVM_MAX_OPENSEARCHES; handle++)
		if(!prog->opensearches[handle])
			break;

	if(handle >= PRVM_MAX_OPENSEARCHES)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -2;
		VM_Warning("VM_search_begin: %s ran out of search handles (%i)\n", PRVM_NAME, PRVM_MAX_OPENSEARCHES);
		return;
	}

	if(!(prog->opensearches[handle] = FS_Search(pattern, false)))
		PRVM_G_FLOAT(OFS_RETURN) = -1;
	else
		PRVM_G_FLOAT(OFS_RETURN) = handle;
}

/*
=========
VM_search_end

void	search_end(float handle)
=========
*/
void VM_search_end(void)
{
	int handle;
	VM_SAFEPARMCOUNT(1, VM_search_end);

	handle = (int)PRVM_G_FLOAT(OFS_PARM0);

	if(handle < 0 || handle >= PRVM_MAX_OPENSEARCHES)
	{
		VM_Warning("VM_search_end: invalid handle %i used in %s\n", handle, PRVM_NAME);
		return;
	}
	if(prog->opensearches[handle] == NULL)
	{
		VM_Warning("VM_search_end: no such handle %i in %s\n", handle, PRVM_NAME);
		return;
	}

	Mem_Free(prog->opensearches[handle]);
	prog->opensearches[handle] = NULL;
}

/*
=========
VM_search_getsize

float	search_getsize(float handle)
=========
*/
void VM_search_getsize(void)
{
	int handle;
	VM_SAFEPARMCOUNT(1, VM_M_search_getsize);

	handle = (int)PRVM_G_FLOAT(OFS_PARM0);

	if(handle < 0 || handle >= PRVM_MAX_OPENSEARCHES)
	{
		VM_Warning("VM_search_getsize: invalid handle %i used in %s\n", handle, PRVM_NAME);
		return;
	}
	if(prog->opensearches[handle] == NULL)
	{
		VM_Warning("VM_search_getsize: no such handle %i in %s\n", handle, PRVM_NAME);
		return;
	}

	PRVM_G_FLOAT(OFS_RETURN) = prog->opensearches[handle]->numfilenames;
}

/*
=========
VM_search_getfilename

string	search_getfilename(float handle, float num)
=========
*/
void VM_search_getfilename(void)
{
	int handle, filenum;
	char *tmp;
	VM_SAFEPARMCOUNT(2, VM_search_getfilename);

	handle = (int)PRVM_G_FLOAT(OFS_PARM0);
	filenum = (int)PRVM_G_FLOAT(OFS_PARM1);

	if(handle < 0 || handle >= PRVM_MAX_OPENSEARCHES)
	{
		VM_Warning("VM_search_getfilename: invalid handle %i used in %s\n", handle, PRVM_NAME);
		return;
	}
	if(prog->opensearches[handle] == NULL)
	{
		VM_Warning("VM_search_getfilename: no such handle %i in %s\n", handle, PRVM_NAME);
		return;
	}
	if(filenum < 0 || filenum >= prog->opensearches[handle]->numfilenames)
	{
		VM_Warning("VM_search_getfilename: invalid filenum %i in %s\n", filenum, PRVM_NAME);
		return;
	}

	tmp = VM_GetTempString();
	strncpy(tmp, prog->opensearches[handle]->filenames[filenum], VM_STRINGTEMP_LENGTH);

	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(tmp);
}

/*
=========
VM_chr

string	chr(float ascii)
=========
*/
void VM_chr(void)
{
	char *tmp;
	VM_SAFEPARMCOUNT(1, VM_chr);

	tmp = VM_GetTempString();
	tmp[0] = (unsigned char) PRVM_G_FLOAT(OFS_PARM0);
	tmp[1] = 0;

	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(tmp);
}

//=============================================================================
// Draw builtins (client & menu)

/*
=========
VM_iscachedpic

float	iscachedpic(string pic)
=========
*/
void VM_iscachedpic(void)
{
	VM_SAFEPARMCOUNT(1,VM_iscachedpic);

	// drawq hasnt such a function, thus always return true
	PRVM_G_FLOAT(OFS_RETURN) = false;
}



/*
========================
VM_cin_open

float cin_open(string name, float bits)
========================
*/
void VM_cin_open( void )
{
	const char	*name;
	int		flags;

	VM_SAFEPARMCOUNT( 2, VM_cin_open );

	name = PRVM_G_STRING( OFS_PARM0 );
	flags = (int)PRVM_G_FLOAT( OFS_PARM1 );

	VM_CheckEmptyString( name );

	if(SCR_PlayCinematic( (char *)name, flags ))
		PRVM_G_FLOAT( OFS_RETURN ) = 1;
	else PRVM_G_FLOAT( OFS_RETURN ) = 0;
}

/*
========================
VM_cin_close

void cin_close( void )
========================
*/
void VM_cin_close( void )
{
	VM_SAFEPARMCOUNT( 0, VM_cin_close );
	SCR_StopCinematic();
}

/*
========================
VM_cin_getstate

float cin_getstate(void)
========================
*/
void VM_cin_getstate( void )
{
	VM_SAFEPARMCOUNT( 0, VM_cin_getstate );
	PRVM_G_FLOAT( OFS_RETURN ) = SCR_GetCinematicState();
}

/*
========================
VM_cin_restart

void cin_restart(void)
========================
*/
void VM_cin_restart( void )
{
	SCR_ResetCinematic();
}

/*
=========
VM_keynumtostring

string keynumtostring(float keynum)
=========
*/
void VM_keynumtostring (void)
{
	int keynum;
	char *tmp;
	VM_SAFEPARMCOUNT(1, VM_keynumtostring);

	keynum = (int)PRVM_G_FLOAT(OFS_PARM0);

	tmp = VM_GetTempString();

	strncpy(tmp, Key_KeynumToString(keynum), VM_STRINGTEMP_LENGTH);

	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(tmp);
}

/*
=========
VM_stringtokeynum

float stringtokeynum(string key)
=========
*/
void VM_stringtokeynum (void)
{
	const char *str;
	VM_SAFEPARMCOUNT( 1, VM_keynumtostring );

	str = PRVM_G_STRING( OFS_PARM0 );

	PRVM_G_INT(OFS_RETURN) = Key_StringToKeynum( (char *)str );
}

/*
==============
VM_vectorvectors

Writes new values for v_forward, v_up, and v_right based on the given forward vector
vectorvectors(vector, vector)
==============
*/
void VM_vectorvectors (void)
{
	DotProduct(PRVM_G_VECTOR(OFS_PARM0), prog->globals.sv->v_forward);
	VectorVectors(prog->globals.sv->v_forward, prog->globals.sv->v_right, prog->globals.sv->v_up);
}

/*
==============
VM_makevectors

Writes new values for v_forward, v_up, and v_right based on angles
makevectors(vector)
==============
*/
void VM_makevectors (void)
{
	AngleVectors(PRVM_G_VECTOR(OFS_PARM0), prog->globals.sv->v_forward, prog->globals.sv->v_right, prog->globals.sv->v_up);
}

void VM_makevectors2 (void)
{
	AngleVectorsFLU(PRVM_G_VECTOR(OFS_PARM0), prog->globals.sv->v_forward, prog->globals.sv->v_right, prog->globals.sv->v_up);
}

// float(float number, float quantity) bitshift (EXT_BITSHIFT)
void VM_bitshift (void)
{
	int n1, n2;
	VM_SAFEPARMCOUNT(2, VM_bitshift);

	n1 = (int)fabs((int)PRVM_G_FLOAT(OFS_PARM0));
	n2 = (int)PRVM_G_FLOAT(OFS_PARM1);
	if(!n1)
		PRVM_G_FLOAT(OFS_RETURN) = n1;
	else
	if(n2 < 0)
		PRVM_G_FLOAT(OFS_RETURN) = (n1 >> -n2);
	else
		PRVM_G_FLOAT(OFS_RETURN) = (n1 << n2);
}

////////////////////////////////////////
// AltString functions
////////////////////////////////////////

/*
========================
VM_altstr_count

float altstr_count(string)
========================
*/
void VM_altstr_count( void )
{
	const char *altstr, *pos;
	int	count;

	VM_SAFEPARMCOUNT( 1, VM_altstr_count );

	altstr = PRVM_G_STRING( OFS_PARM0 );
	//VM_CheckEmptyString( altstr );

	for( count = 0, pos = altstr ; *pos ; pos++ ) {
		if( *pos == '\\' ) {
			if( !*++pos ) {
				break;
			}
		} else if( *pos == '\'' ) {
			count++;
		}
	}

	PRVM_G_FLOAT( OFS_RETURN ) = (float) (count / 2);
}

/*
========================
VM_altstr_prepare

string altstr_prepare(string)
========================
*/
void VM_altstr_prepare( void )
{
	char *outstr, *out;
	const char *instr, *in;
	int size;

	VM_SAFEPARMCOUNT( 1, VM_altstr_prepare );

	instr = PRVM_G_STRING( OFS_PARM0 );
	//VM_CheckEmptyString( instr );
	outstr = VM_GetTempString();

	for( out = outstr, in = instr, size = VM_STRINGTEMP_LENGTH - 1 ; size && *in ; size--, in++, out++ )
		if( *in == '\'' ) {
			*out++ = '\\';
			*out = '\'';
			size--;
		} else
			*out = *in;
	*out = 0;

	PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( outstr );
}

/*
========================
VM_altstr_get

string altstr_get(string, float)
========================
*/
void VM_altstr_get( void )
{
	const char *altstr, *pos;
	char *outstr, *out;
	int count, size;

	VM_SAFEPARMCOUNT( 2, VM_altstr_get );

	altstr = PRVM_G_STRING( OFS_PARM0 );
	//VM_CheckEmptyString( altstr );

	count = (int)PRVM_G_FLOAT( OFS_PARM1 );
	count = count * 2 + 1;

	for( pos = altstr ; *pos && count ; pos++ )
		if( *pos == '\\' ) {
			if( !*++pos )
				break;
		} else if( *pos == '\'' )
			count--;

	if( !*pos ) {
		PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( NULL );
		return;
	}

    outstr = VM_GetTempString();
	for( out = outstr, size = VM_STRINGTEMP_LENGTH - 1 ; size && *pos ; size--, pos++, out++ )
		if( *pos == '\\' ) {
			if( !*++pos )
				break;
			*out = *pos;
			size--;
		} else if( *pos == '\'' )
			break;
		else
			*out = *pos;

	*out = 0;
	PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( outstr );
}

/*
========================
VM_altstr_set

string altstr_set(string altstr, float num, string set)
========================
*/
void VM_altstr_set( void )
{
    int num;
	const char *altstr, *str;
	const char *in;
	char *outstr, *out;

	VM_SAFEPARMCOUNT( 3, VM_altstr_set );

	altstr = PRVM_G_STRING( OFS_PARM0 );
	//VM_CheckEmptyString( altstr );

	num = (int)PRVM_G_FLOAT( OFS_PARM1 );

	str = PRVM_G_STRING( OFS_PARM2 );
	//VM_CheckEmptyString( str );

	outstr = out = VM_GetTempString();
	for( num = num * 2 + 1, in = altstr; *in && num; *out++ = *in++ )
		if( *in == '\\' ) {
			if( !*++in ) {
				break;
			}
		} else if( *in == '\'' ) {
			num--;
		}

	if( !in ) {
		PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( altstr );
		return;
	}
	// copy set in
	for( ; *str; *out++ = *str++ );
	// now jump over the old content
	for( ; *in ; in++ )
		if( *in == '\'' || (*in == '\\' && !*++in) )
			break;

	if( !in ) {
		PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( NULL );
		return;
	}

	strncpy(out, in, VM_STRINGTEMP_LENGTH);
	PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( outstr );
}

/*
========================
VM_altstr_ins
insert after num
string	altstr_ins(string altstr, float num, string set)
========================
*/
void VM_altstr_ins(void)
{
	int num;
	const char *setstr;
	const char *set;
	const char *instr;
	const char *in;
	char *outstr;
	char *out;

	in = instr = PRVM_G_STRING( OFS_PARM0 );
	num = (int)PRVM_G_FLOAT( OFS_PARM1 );
	set = setstr = PRVM_G_STRING( OFS_PARM2 );

	out = outstr = VM_GetTempString();
	for( num = num * 2 + 2 ; *in && num > 0 ; *out++ = *in++ )
		if( *in == '\\' ) {
			if( !*++in ) {
				break;
			}
		} else if( *in == '\'' ) {
			num--;
		}

	*out++ = '\'';
	for( ; *set ; *out++ = *set++ );
	*out++ = '\'';

	strncpy(out, in, VM_STRINGTEMP_LENGTH);
	PRVM_G_INT( OFS_RETURN ) = PRVM_SetEngineString( outstr );
}


////////////////////////////////////////
// BufString functions
////////////////////////////////////////
//[515]: string buffers support
#define MAX_QCSTR_BUFFERS 128
#define MAX_QCSTR_STRINGS 1024

typedef struct
{
	int		num_strings;
	char	*strings[MAX_QCSTR_STRINGS];
}qcstrbuffer_t;

static qcstrbuffer_t	*qcstringbuffers[MAX_QCSTR_BUFFERS];
static int				num_qcstringbuffers;
static int				buf_sortpower;

#define BUFSTR_BUFFER(a) (a>=MAX_QCSTR_BUFFERS) ? NULL : (qcstringbuffers[a])
#define BUFSTR_ISFREE(a) (a<MAX_QCSTR_BUFFERS&&qcstringbuffers[a]&&qcstringbuffers[a]->num_strings<=0) ? 1 : 0

static int BufStr_FindFreeBuffer (void)
{
	int	i;
	if(num_qcstringbuffers == MAX_QCSTR_BUFFERS)
		return -1;
	for(i=0;i<MAX_QCSTR_BUFFERS;i++)
		if(!qcstringbuffers[i])
		{
			qcstringbuffers[i] = (qcstrbuffer_t *)Z_Malloc(sizeof(qcstrbuffer_t));
			memset(qcstringbuffers[i], 0, sizeof(qcstrbuffer_t));
			return i;
		}
	return -1;
}

static void BufStr_ClearBuffer (int index)
{
	qcstrbuffer_t	*b = qcstringbuffers[index];
	int				i;

	if(b)
	{
		if(b->num_strings > 0)
		{
			for(i=0;i<b->num_strings;i++)
				if(b->strings[i])
					Mem_Free(b->strings[i]);
			num_qcstringbuffers--;
		}
		Mem_Free(qcstringbuffers[index]);
		qcstringbuffers[index] = NULL;
	}
}

static int BufStr_FindFreeString (qcstrbuffer_t *b)
{
	int				i;
	for(i=0;i<b->num_strings;i++)
		if(!b->strings[i] || !b->strings[i][0])
			return i;
	if(i == MAX_QCSTR_STRINGS)	return -1;
	else						return i;
}

static int BufStr_SortStringsUP (const void *in1, const void *in2)
{
	const char *a, *b;
	a = *((const char **) in1);
	b = *((const char **) in2);
	if(!a[0])	return 1;
	if(!b[0])	return -1;
	return strncmp(a, b, buf_sortpower);
}

static int BufStr_SortStringsDOWN (const void *in1, const void *in2)
{
	const char *a, *b;
	a = *((const char **) in1);
	b = *((const char **) in2);
	if(!a[0])	return 1;
	if(!b[0])	return -1;
	return strncmp(b, a, buf_sortpower);
}

#ifdef REMOVETHIS
static void VM_BufStr_Init (void)
{
	memset(qcstringbuffers, 0, sizeof(qcstringbuffers));
	num_qcstringbuffers = 0;
}

static void VM_BufStr_ShutDown (void)
{
	int i;
	for(i=0;i<MAX_QCSTR_BUFFERS && num_qcstringbuffers;i++)
		BufStr_ClearBuffer(i);
}
#endif

/*
========================
VM_buf_create
creates new buffer, and returns it's index, returns -1 if failed
float buf_create(void) = #460;
========================
*/
void VM_buf_create (void)
{
	int i;
	VM_SAFEPARMCOUNT(0, VM_buf_create);
	i = BufStr_FindFreeBuffer();
	if(i >= 0)
		num_qcstringbuffers++;
	//else
		//Msg("VM_buf_create: buffers overflow in %s\n", PRVM_NAME);
	PRVM_G_FLOAT(OFS_RETURN) = i;
}

/*
========================
VM_buf_del
deletes buffer and all strings in it
void buf_del(float bufhandle) = #461;
========================
*/
void VM_buf_del (void)
{
	VM_SAFEPARMCOUNT(1, VM_buf_del);
	if(BUFSTR_BUFFER((int)PRVM_G_FLOAT(OFS_PARM0)))
		BufStr_ClearBuffer((int)PRVM_G_FLOAT(OFS_PARM0));
	else
	{
		VM_Warning("VM_buf_del: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
}

/*
========================
VM_buf_getsize
how many strings are stored in buffer
float buf_getsize(float bufhandle) = #462;
========================
*/
void VM_buf_getsize (void)
{
	qcstrbuffer_t	*b;
	VM_SAFEPARMCOUNT(1, VM_buf_getsize);

	b = BUFSTR_BUFFER((int)PRVM_G_FLOAT(OFS_PARM0));
	if(!b)
	{
		PRVM_G_FLOAT(OFS_RETURN) = -1;
		VM_Warning("VM_buf_getsize: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	else
		PRVM_G_FLOAT(OFS_RETURN) = b->num_strings;
}

/*
========================
VM_buf_copy
copy all content from one buffer to another, make sure it exists
void buf_copy(float bufhandle_from, float bufhandle_to) = #463;
========================
*/
void VM_buf_copy (void)
{
	qcstrbuffer_t	*b1, *b2;
	int				i;
	VM_SAFEPARMCOUNT(2, VM_buf_copy);

	b1 = BUFSTR_BUFFER((int)PRVM_G_FLOAT(OFS_PARM0));
	if(!b1)
	{
		VM_Warning("VM_buf_copy: invalid source buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	i = (int)PRVM_G_FLOAT(OFS_PARM1);
	if(i == (int)PRVM_G_FLOAT(OFS_PARM0))
	{
		VM_Warning("VM_buf_copy: source == destination (%i) in %s\n", i, PRVM_NAME);
		return;
	}
	b2 = BUFSTR_BUFFER(i);
	if(!b2)
	{
		VM_Warning("VM_buf_copy: invalid destination buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM1), PRVM_NAME);
		return;
	}

	BufStr_ClearBuffer(i);
	qcstringbuffers[i] = (qcstrbuffer_t *)Z_Malloc(sizeof(qcstrbuffer_t));
	memset(qcstringbuffers[i], 0, sizeof(qcstrbuffer_t));
	b2->num_strings = b1->num_strings;

	for(i=0;i<b1->num_strings;i++)
		if(b1->strings[i] && b1->strings[i][0])
		{
			size_t stringlen;
			stringlen = strlen(b1->strings[i]) + 1;
			b2->strings[i] = (char *)Z_Malloc(stringlen);
			if(!b2->strings[i])
			{
				VM_Warning("VM_buf_copy: not enough memory for buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM1), PRVM_NAME);
				break;
			}
			memcpy(b2->strings[i], b1->strings[i], stringlen);
		}
}

/*
========================
VM_buf_sort
sort buffer by beginnings of strings (sortpower defaults it's lenght)
"backward == TRUE" means that sorting goes upside-down
void buf_sort(float bufhandle, float sortpower, float backward) = #464;
========================
*/
void VM_buf_sort (void)
{
	qcstrbuffer_t	*b;
	int				i;
	VM_SAFEPARMCOUNT(3, VM_buf_sort);

	b = BUFSTR_BUFFER((int)PRVM_G_FLOAT(OFS_PARM0));
	if(!b)
	{
		VM_Warning("VM_buf_sort: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	if(b->num_strings <= 0)
	{
		VM_Warning("VM_buf_sort: tried to sort empty buffer %i in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	buf_sortpower = (int)PRVM_G_FLOAT(OFS_PARM1);
	if(buf_sortpower <= 0)
		buf_sortpower = 99999999;

	if(!PRVM_G_FLOAT(OFS_PARM2))
		qsort(b->strings, b->num_strings, sizeof(char*), BufStr_SortStringsUP);
	else
		qsort(b->strings, b->num_strings, sizeof(char*), BufStr_SortStringsDOWN);

	for(i=b->num_strings-1;i>=0;i--)	//[515]: delete empty lines
		if(b->strings)
		{
			if(b->strings[i][0])
				break;
			else
			{
				Mem_Free(b->strings[i]);
				--b->num_strings;
				b->strings[i] = NULL;
			}
		}
		else
			--b->num_strings;
}

/*
========================
VM_buf_implode
concantenates all buffer string into one with "glue" separator and returns it as tempstring
string buf_implode(float bufhandle, string glue) = #465;
========================
*/
void VM_buf_implode (void)
{
	qcstrbuffer_t	*b;
	char			*k;
	const char		*sep;
	int				i;
	size_t			l;
	VM_SAFEPARMCOUNT(2, VM_buf_implode);

	b = BUFSTR_BUFFER((int)PRVM_G_FLOAT(OFS_PARM0));
	PRVM_G_INT(OFS_RETURN) = 0;
	if(!b)
	{
		VM_Warning("VM_buf_implode: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	if(!b->num_strings)
		return;
	sep = PRVM_G_STRING(OFS_PARM1);
	k = VM_GetTempString();
	k[0] = 0;
	for(l=i=0;i<b->num_strings;i++)
		if(b->strings[i])
		{
			l += strlen(b->strings[i]);
			if(l>=4095)
				break;
			strncat(k, b->strings[i], VM_STRINGTEMP_LENGTH);
			if(sep && (i != b->num_strings-1))
			{
				l += strlen(sep);
				if(l>=4095)
					break;
				strncat(k, sep, VM_STRINGTEMP_LENGTH);
			}
		}
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(k);
}

/*
========================
VM_bufstr_get
get a string from buffer, returns direct pointer, dont str_unzone it!
string bufstr_get(float bufhandle, float string_index) = #465;
========================
*/
void VM_bufstr_get (void)
{
	qcstrbuffer_t	*b;
	int				strindex;
	VM_SAFEPARMCOUNT(2, VM_bufstr_get);

	b = BUFSTR_BUFFER((int)PRVM_G_FLOAT(OFS_PARM0));
	if(!b)
	{
		VM_Warning("VM_bufstr_get: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	strindex = (int)PRVM_G_FLOAT(OFS_PARM1);
	if(strindex < 0 || strindex > MAX_QCSTR_STRINGS)
	{
		VM_Warning("VM_bufstr_get: invalid string index %i used in %s\n", strindex, PRVM_NAME);
		return;
	}
	PRVM_G_INT(OFS_RETURN) = 0;
	if(b->num_strings <= strindex)
		return;
	if(b->strings[strindex])
		PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(b->strings[strindex]);
}

/*
========================
VM_bufstr_set
copies a string into selected slot of buffer
void bufstr_set(float bufhandle, float string_index, string str) = #466;
========================
*/
void VM_bufstr_set (void)
{
	int				bufindex, strindex;
	qcstrbuffer_t	*b;
	const char		*news;
	size_t			alloclen;

	VM_SAFEPARMCOUNT(3, VM_bufstr_set);

	bufindex = (int)PRVM_G_FLOAT(OFS_PARM0);
	b = BUFSTR_BUFFER(bufindex);
	if(!b)
	{
		VM_Warning("VM_bufstr_set: invalid buffer %i used in %s\n", bufindex, PRVM_NAME);
		return;
	}
	strindex = (int)PRVM_G_FLOAT(OFS_PARM1);
	if(strindex < 0 || strindex > MAX_QCSTR_STRINGS)
	{
		VM_Warning("VM_bufstr_set: invalid string index %i used in %s\n", strindex, PRVM_NAME);
		return;
	}
	news = PRVM_G_STRING(OFS_PARM2);
	if(!news)
	{
		VM_Warning("VM_bufstr_set: null string used in %s\n", PRVM_NAME);
		return;
	}
	if(b->strings[strindex])
		Mem_Free(b->strings[strindex]);
	alloclen = strlen(news) + 1;
	b->strings[strindex] = (char *)Z_Malloc(alloclen);
	memcpy(b->strings[strindex], news, alloclen);
}

/*
========================
VM_bufstr_add
adds string to buffer in nearest free slot and returns it
"order == TRUE" means that string will be added after last "full" slot
float bufstr_add(float bufhandle, string str, float order) = #467;
========================
*/
void VM_bufstr_add (void)
{
	int				bufindex, order, strindex;
	qcstrbuffer_t	*b;
	const char		*string;
	size_t			alloclen;

	VM_SAFEPARMCOUNT(3, VM_bufstr_add);

	bufindex = (int)PRVM_G_FLOAT(OFS_PARM0);
	b = BUFSTR_BUFFER(bufindex);
	PRVM_G_FLOAT(OFS_RETURN) = -1;
	if(!b)
	{
		VM_Warning("VM_bufstr_add: invalid buffer %i used in %s\n", bufindex, PRVM_NAME);
		return;
	}
	string = PRVM_G_STRING(OFS_PARM1);
	if(!string)
	{
		VM_Warning("VM_bufstr_add: null string used in %s\n", PRVM_NAME);
		return;
	}

	order = (int)PRVM_G_FLOAT(OFS_PARM2);
	if(order)
		strindex = b->num_strings;
	else
	{
		strindex = BufStr_FindFreeString(b);
		if(strindex < 0)
		{
			VM_Warning("VM_bufstr_add: buffer %i has no free string slots in %s\n", bufindex, PRVM_NAME);
			return;
		}
	}

	while(b->num_strings <= strindex)
	{
		if(b->num_strings == MAX_QCSTR_STRINGS)
		{
			VM_Warning("VM_bufstr_add: buffer %i has no free string slots in %s\n", bufindex, PRVM_NAME);
			return;
		}
		b->strings[b->num_strings] = NULL;
		b->num_strings++;
	}
	if(b->strings[strindex])
		Mem_Free(b->strings[strindex]);
	alloclen = strlen(string) + 1;
	b->strings[strindex] = (char *)Z_Malloc(alloclen);
	memcpy(b->strings[strindex], string, alloclen);
	PRVM_G_FLOAT(OFS_RETURN) = strindex;
}

/*
========================
VM_bufstr_free
delete string from buffer
void bufstr_free(float bufhandle, float string_index) = #468;
========================
*/
void VM_bufstr_free (void)
{
	int				i;
	qcstrbuffer_t	*b;
	VM_SAFEPARMCOUNT(2, VM_bufstr_free);

	b = BUFSTR_BUFFER((int)PRVM_G_FLOAT(OFS_PARM0));
	if(!b)
	{
		VM_Warning("VM_bufstr_free: invalid buffer %i used in %s\n", (int)PRVM_G_FLOAT(OFS_PARM0), PRVM_NAME);
		return;
	}
	i = (int)PRVM_G_FLOAT(OFS_PARM1);
	if(i < 0 || i > MAX_QCSTR_STRINGS)
	{
		VM_Warning("VM_bufstr_free: invalid string index %i used in %s\n", i, PRVM_NAME);
		return;
	}
	if(b->strings[i])
		Mem_Free(b->strings[i]);
	b->strings[i] = NULL;
	if(i+1 == b->num_strings)
		--b->num_strings;
}

//=============

void VM_Cmd_Init(void)
{
	// only init the stuff for the current prog
	VM_Files_Init();
	VM_Search_Init();
}

void VM_Cmd_Reset(void)
{
	VM_Search_Reset();
	VM_Files_CloseAll();
}

