//=======================================================================
//			Copyright XashXT Group 2007 ©
//			ui_cmds.c - ui menu builtins
//=======================================================================

#include "uimenu.h"

/*
=========
VM_M_precache_file

string	precache_file(string)
=========
*/
void VM_M_precache_file (void)
{	
	// precache_file is only used to copy files with qcc, it does nothing
	VM_SAFEPARMCOUNT(1, VM_precache_file);
	PRVM_G_INT(OFS_RETURN) = PRVM_G_INT(OFS_PARM0);
}

/*
=========
VM_M_preache_error

used instead of the other VM_precache_* functions in the builtin list
=========
*/
void VM_M_precache_error (void)
{
	PRVM_ERROR("PF_Precache: Precache can only be done in spawn functions");
}

/*
=========
VM_M_precache_sound

string	precache_sound (string sample)
=========
*/
void VM_M_precache_sound( void )
{
	const char	*s;

	VM_SAFEPARMCOUNT(1, VM_precache_sound);

	s = PRVM_G_STRING(OFS_PARM0);
	PRVM_G_INT(OFS_RETURN) = PRVM_G_INT(OFS_PARM0);
	VM_CheckEmptyString( s );

	// fake precaching
	if(!FS_FileExists( va("sound/%s", s )))
	{
		VM_Warning("VM_precache_sound: Failed to load %s for %s\n", s, PRVM_NAME);
		return;
	}
}

/*
=========
VM_M_setmousetarget

setmousetarget(float target)
=========
*/
void VM_M_setmousetarget(void)
{
	VM_SAFEPARMCOUNT(1, VM_M_setmousetarget);
}

/*
=========
VM_M_getmousetarget

float	getmousetarget
=========
*/
void VM_M_getmousetarget(void)
{
	VM_SAFEPARMCOUNT(0, VM_M_getmousetarget);

	PRVM_G_FLOAT(OFS_RETURN) = 1;
}



/*
=========
VM_M_setkeydest

setkeydest(float dest)
=========
*/
void VM_M_setkeydest(void)
{
	VM_SAFEPARMCOUNT(1, VM_M_setkeydest);
        
	switch((int)PRVM_G_FLOAT(OFS_PARM0))
	{
	case key_game:
		// key_game
		cls.key_dest = key_game;
		break;
	case key_menu:
		// key_menu
		cls.key_dest = key_menu;
		break;
	case key_message:
		// key_message
		// cls.key_dest = key_message
		// break;
	default:
		PRVM_ERROR("VM_M_setkeydest: wrong destination %f !", PRVM_G_FLOAT(OFS_PARM0));
	}
}

/*
=========
VM_M_getkeydest

float	getkeydest
=========
*/
void VM_M_getkeydest(void)
{
	VM_SAFEPARMCOUNT(0, VM_M_getkeydest);

	// key_game = 0, key_message = 1, key_menu = 2, unknown = 3
	switch(cls.key_dest)
	{
	case key_game:
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		break;
	case key_menu:
		PRVM_G_FLOAT(OFS_RETURN) = 2;
		break;
	case key_message:
		// not supported
		// PRVM_G_FLOAT(OFS_RETURN) = 1;
		// break;
	default:
		PRVM_G_FLOAT(OFS_RETURN) = 3;
	}
}

/*
=========
VM_M_callfunction

	callfunction(...,string function_name)
Extension: pass
=========
*/
void VM_M_callfunction( void )
{
	mfunction_t *func;
	const char *s;

	if(prog->argc == 0) PRVM_ERROR("VM_M_callfunction: 1 parameter is required !");
	s = PRVM_G_STRING(OFS_PARM0 + (prog->argc - 1));
	if(!s) PRVM_ERROR("VM_M_callfunction: null string !");

	VM_CheckEmptyString(s);
	func = PRVM_ED_FindFunction(s);

	if(!func) 
	{
		PRVM_ERROR("VM_M_callfunciton: function %s not found !", s);
	}
	else if(func->first_statement < 0)
	{
		// negative statements are built in functions
		int builtinnumber = -func->first_statement;
		prog->xfunction->builtinsprofile++;
		if (builtinnumber < prog->numbuiltins && prog->builtins[builtinnumber])
			prog->builtins[builtinnumber]();
		else PRVM_ERROR("No such builtin #%i in %s", builtinnumber, PRVM_NAME);
	}
	else if(func - prog->functions > 0)
	{
		prog->argc--;
		PRVM_ExecuteProgram(func - prog->functions,"");
		prog->argc++;
	}
}

/*
=========
VM_M_isfunction

float	isfunction(string function_name)
=========
*/
void VM_M_isfunction(void)
{
	mfunction_t *func;
	const char *s;

	VM_SAFEPARMCOUNT(1, VM_M_isfunction);

	s = PRVM_G_STRING(OFS_PARM0);

	if(!s) PRVM_ERROR("VM_M_isfunction: null string !");
	VM_CheckEmptyString(s);

	func = PRVM_ED_FindFunction(s);

	if(!func) PRVM_G_FLOAT(OFS_RETURN) = false;
	else PRVM_G_FLOAT(OFS_RETURN) = true;
}

/*
=========
VM_M_writetofile

	writetofile(float fhandle, entity ent)
=========
*/
void VM_M_writetofile(void)
{
	edict_t	*ent;
	vfile_t	*file;

	VM_SAFEPARMCOUNT(2, VM_M_writetofile);

	file = VM_GetFileHandle( (int)PRVM_G_FLOAT(OFS_PARM0) );
	if( !file )
	{
		VM_Warning("VM_M_writetofile: invalid or closed file handle\n");
		return;
	}

	ent = PRVM_G_EDICT(OFS_PARM1);
	if(ent->priv.ed->free)
	{
		VM_Warning("VM_M_writetofile: %s: entity %i is free !\n", PRVM_NAME, PRVM_EDICT_NUM(OFS_PARM1));
		return;
	}
	PRVM_ED_Write(file, ent);
}

/*
=========
VM_M_findkeysforcommand

string	findkeysforcommand(string command)

the returned string is an altstring
=========
*/
void VM_M_findkeysforcommand(void)
{
	const char	*cmd;
	char		*ret;
	int		keys[2];
	int		i;

	VM_SAFEPARMCOUNT(1, VM_M_findkeysforcommand);

	cmd = PRVM_G_STRING(OFS_PARM0);

	VM_CheckEmptyString(cmd);

	(ret = VM_GetTempString())[0] = 0;

	M_FindKeysForCommand((char *)cmd, keys);
	for(i = 0; i < 2; i++) std.strncat(ret, va(" \'%i\'", keys[i]), VM_STRINGTEMP_LENGTH);
	PRVM_G_INT(OFS_RETURN) = PRVM_SetEngineString(ret);
}

prvm_builtin_t vm_ui_builtins[] = 
{
	0, // to be consistent with the old vm
	// common builtings (mostly)
	VM_checkextension,
	VM_error,
	VM_objerror,
	VM_print,
	VM_bprint,
	VM_sprint,
	VM_centerprint,
	VM_normalize,
	VM_veclength,
	VM_vectoyaw,	// #10
	VM_vectoangles,
	VM_random_float,
	VM_localcmd,
	VM_cvar,
	VM_cvar_set,
	VM_print,
	VM_ftoa,
	VM_fabs,
	VM_vtoa,
	VM_etos,		// 20
	VM_atof,
	VM_create,
	VM_remove,
	VM_find,
	VM_findfloat,
	VM_findchain,
	VM_findchainfloat,
	VM_M_precache_file,
	VM_M_precache_sound,
	VM_coredump,	// 30
	VM_traceon,
	VM_traceoff,
	VM_eprint,
	VM_rint,
	VM_floor,
	VM_ceil,
	VM_nextent,
	VM_sin,
	VM_cos,
	VM_sqrt,		// 40
	VM_randomvec,
	VM_registercvar,
	VM_min,
	VM_max,
	VM_bound,
	VM_pow,
	VM_copyentity,
	VM_fopen,
	VM_fclose,
	VM_fgets,			// 50
	VM_fputs,
	VM_strlen,
	VM_strcat,
	VM_substring,
	VM_atov,
	VM_allocstring,
	VM_freestring,
	VM_tokenize,
	VM_argv,
	VM_isserver,		// 60
	VM_clientcount,
	VM_clientstate,
	VM_clientcmd,
	VM_changelevel,
	VM_localsound,
	VM_getmousepos,
	VM_gettime,
	VM_loadfromdata,
	VM_loadfromfile,
	VM_modulo,		// 70
	VM_cvar_string,
	VM_crash,
	VM_stackdump,		// 73
	VM_search_begin,
	VM_search_end,
	VM_search_getsize,
	VM_search_getfilename,	// 77
	VM_chr,
	VM_itof,
	VM_ftoe,			// 80
	VM_itof,			// isString
	VM_altstr_count,
	VM_altstr_prepare,
	VM_altstr_get,
	VM_altstr_set,
	VM_altstr_ins,
	VM_findflags,
	VM_findchainflags,
	NULL,			// 89
	NULL,			// 90
	e10,			// 100
	e100,			// 200
	e100,			// 300
	e100,			// 400
	e10,			// 410
	e10,			// 420
	e10,			// 430
	e10,			// 440
	e10,			// 450
	// draw functions
	VM_iscachedpic,
	VM_precache_pic,
	NULL,
	VM_drawcharacter,
	VM_drawstring,
	VM_drawpic,
	VM_drawfill,
	NULL,
	NULL,
	VM_getimagesize,		// 460
	VM_cin_open,
	VM_cin_close,
	NULL,
	VM_cin_getstate,
	VM_cin_restart,		// 465
	NULL,			// 466
	NULL,
	NULL,
	NULL,
	NULL,			// 470
	e10,			// 480
	e10,			// 490
	e10,			// 500
	e100,			// 600
	// menu functions
	VM_M_setkeydest,
	VM_M_getkeydest,
	VM_M_setmousetarget,
	VM_M_getmousetarget,
	VM_M_callfunction,
	VM_M_writetofile,
	VM_M_isfunction,
	NULL,
	VM_keynumtostring,
	VM_M_findkeysforcommand,	// 610
	NULL,
	NULL,
	VM_parseentitydata,
	VM_stringtokeynum,		// 614
};

const int vm_ui_numbuiltins = sizeof(vm_ui_builtins) / sizeof(prvm_builtin_t);