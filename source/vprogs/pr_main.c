//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       pr_main.c - PRVM compiler-executor
//=======================================================================

#include "vprogs.h"
#include "builtin.h"

stdlib_api_t	com;
byte		*qccpool;
int		com_argc = 0;
char		**com_argv;
char		v_copyright[1024];
uint		MAX_REGS;
int		MAX_ERRORS;
int		MAX_STRINGS;
int		MAX_GLOBALS;
int		MAX_FIELDS;
int		MAX_STATEMENTS;
int		MAX_FUNCTIONS;
int		MAX_CONSTANTS;
int		numtemps;
bool		compileactive = false;
char		progsoutname[MAX_SYSPATH];
float		*pr_globals;
uint		numpr_globals;
char		*strings;
int		strofs;
dstatement_t	*statements;
int		numstatements;
int		*statement_linenums;
char		sourcedir[MAX_SYSPATH];
cachedsourcefile_t	*sourcefile;
dfunction_t	*functions;
int		numfunctions;
ddef_t		*qcc_globals;
int		numglobaldefs;
ddef_t		*fields;
int		numfielddefs;
bool		pr_warning[WARN_MAX];
int		target_version;
bool		bodylessfuncs;
type_t		*qcc_typeinfo;
int		numtypeinfos;
int		maxtypeinfos;
int		prvm_developer;
int		host_instance;
int		prvm_state;
vprogs_exp_t	vm;

hashtable_t compconstantstable;
hashtable_t globalstable;
hashtable_t localstable;
hashtable_t intconstdefstable;
hashtable_t floatconstdefstable;
hashtable_t stringconstdefstable;

cvar_t *prvm_maxedicts;
cvar_t *prvm_traceqc;
cvar_t *prvm_boundscheck;
cvar_t *prvm_statementprofiling;

void PR_SetDefaultProperties( void )
{
	const_t	*cnst;
	int	i, j;
	char	*name, *val;

	Hash_InitTable( &compconstantstable, MAX_CONSTANTS );
	
	ForcedCRC = 0;
	// enable all warnings
	memset(pr_warning, 0, sizeof(pr_warning));

	// reset all optimizarions
	for( i = 0; pr_optimisations[i].enabled; i++ ) 
		*pr_optimisations[i].enabled = false;

	target_version = QPROGS_VERSION;
	PR_DefineName("_QCLIB"); // compiler type

	// play with default warnings.
	pr_warning[WARN_NOTREFERENCEDCONST] = true;
	pr_warning[WARN_MACROINSTRING] = true;
	pr_warning[WARN_FIXEDRETURNVALUECONFLICT] = true;
	pr_warning[WARN_EXTRAPRECACHE] = true;
	pr_warning[WARN_DEADCODE] = true;
	pr_warning[WARN_INEFFICIENTPLUSPLUS] = true;
	pr_warning[WARN_EXTENSION_USED] = true;
	pr_warning[WARN_IFSTRING_USED] = true;

	for (i = 1; i < com_argc; i++)
	{
		if( !com.strnicmp(com_argv[i], "/D", 2))
		{
			// #define
			name = com_argv[i+1];
			val = com.strchr(name, '=');
			if (val) { *val = '\0', val++; }
			cnst = PR_DefineName(name);
			if (val)
			{
				if(com.strlen(val) + 1 >= sizeof(cnst->value))
					PR_ParseError(ERR_INTERNAL, "compiler constant value is too long\n");
				com.strncpy(cnst->value, val, sizeof(cnst->value)-1);
				PR_Message("value %s\n", val );
				cnst->value[sizeof(cnst->value)-1] = '\0';
			}
		}
		else if ( !com.strnicmp(com_argv[i], "/V", 2))
		{
			// target version
			if (com_argv[i][2] == '6') target_version = QPROGS_VERSION;
			else if (com_argv[i][2] == '7') target_version = FPROGS_VERSION;
			else if (com_argv[i][2] == '8') target_version = VPROGS_VERSION;
			else PR_Warning(0, NULL, WARN_BADPARAMS,"Unrecognized version parametr (%s)",com_argv[i]);
		}
		else if( !com.strnicmp(com_argv[i], "/O", 2))
		{
			int currentlevel = 0; // optimization level
			if(com_argv[i][2] == 'd') currentlevel = MASK_DEBUG; // disable optimizations
			else if (com_argv[i][2] == '0') currentlevel = MASK_LEVEL_O;
			else if (com_argv[i][2] == '1') currentlevel = MASK_LEVEL_1;
			else if (com_argv[i][2] == '2') currentlevel = MASK_LEVEL_2;
			else if (com_argv[i][2] == '3') currentlevel = MASK_LEVEL_3;
			else if (com_argv[i][2] == '4') currentlevel = MASK_LEVEL_4;

			if(currentlevel)
			{
				for (j = 0; pr_optimisations[j].enabled; j++)
				{
					if(pr_optimisations[j].levelmask & currentlevel)
						*pr_optimisations[j].enabled = true;
				}
			}
			else
			{
				char *abbrev = com_argv[i]+2; // custom optimisation
				for (j = 0; pr_optimisations[j].enabled; j++)
				{
					if(!com.strcmp(pr_optimisations[j].shortname, abbrev)) 
					{
						*pr_optimisations[j].enabled = true;
						break;
					}
				}
			}
		}
	}

}

/*
===================
PR_Init

initialize compiler and hash tables
===================
*/
void PR_InitCompile( const char *name )
{
	static char	parmname[12][MAX_PARMS];
	static temp_t	ret_temp;
	int		i;

	com.strncat(v_copyright,"This file was created with Xash3D QuakeC compiler,\n", sizeof(v_copyright));
	com.strncat(v_copyright,"who based on original code of ForeThought's QuakeC compiler.\n",sizeof(v_copyright));
	com.strncat(v_copyright,"Thanks to ID Software at all.", sizeof(v_copyright));

	// tune limits
	MAX_REGS		= 65536;
	MAX_ERRORS	= 10; // per one file
	MAX_STRINGS	= 1000000;
	MAX_GLOBALS	= 32768;
	MAX_FIELDS	= 2048;
	MAX_STATEMENTS	= 0x80000;
	MAX_FUNCTIONS	= 16384;
	maxtypeinfos	= 16384;
	MAX_CONSTANTS	= 2048;

	PR_SetDefaultProperties();
	
	numtemps = 0;
	functemps = NULL;
	sourcefile = NULL;

	strings = (void *)Qalloc(sizeof(char) * MAX_STRINGS);
	strofs = 1;

	statements = (void *)Qalloc(sizeof(dstatement_t) * MAX_STATEMENTS);
	numstatements = 1;

	statement_linenums = (void *)Qalloc(sizeof(int) * MAX_STATEMENTS);

	functions = (void *)Qalloc(sizeof(dfunction_t) * MAX_FUNCTIONS);
	numfunctions = 1;

	pr_bracelevel = 0;

	pr_globals = (void *)Qalloc(sizeof(float) * MAX_REGS);
	numpr_globals = 0;

	Hash_InitTable(&globalstable, MAX_REGS);
	Hash_InitTable(&localstable, MAX_REGS);
	Hash_InitTable(&floatconstdefstable, MAX_REGS+1);
	Hash_InitTable(&intconstdefstable, MAX_REGS+1);
	Hash_InitTable(&stringconstdefstable, MAX_REGS);
	
	qcc_globals = (void *)Qalloc(sizeof(ddef_t) * MAX_GLOBALS);
	numglobaldefs = 1;	

	fields = (void *)Qalloc(sizeof(ddef_t) * MAX_FIELDS);
	numfielddefs = 1;

	qcc_typeinfo = (void *)Qalloc(sizeof(type_t) * maxtypeinfos);
	numtypeinfos = 0;
	bodylessfuncs = 0;

	memset(&pr, 0, sizeof(pr));
	memset(&ret_temp, 0, sizeof(ret_temp));
	memset(pr_immediate_string, 0, sizeof(pr_immediate_string));

	if (opt_locals_marshalling) MsgDev( D_INFO, "Locals marshalling might be buggy. Use with caution\n");
	com.strncpy( sourcefilename, name, sizeof(sourcefilename));

	// default parms
	def_ret.ofs = OFS_RETURN;
	def_ret.name = "return";
	def_ret.temp = &ret_temp;
	def_ret.constant = false;
	def_ret.type = NULL;
	ret_temp.ofs = def_ret.ofs;
	ret_temp.scope = NULL;
	ret_temp.size = 3;
	ret_temp.next = NULL;

	for (i = 0; i < MAX_PARMS; i++)
	{
		def_parms[i].temp = NULL;
		def_parms[i].type = NULL;
		def_parms[i].ofs = OFS_PARM0 + 3*i;
		def_parms[i].name = parmname[i];
		com.sprintf(parmname[i], "parm%i", i);
	}
}

void PR_InitDecompile( const char *name )
{
	string	progsname;

	FS_FileBase( name, progsname );
	PRVM_InitProg( PRVM_DECOMPILED );

	vm.prog->reserved_edicts = 1;
	vm.prog->loadintoworld = true;
	vm.prog->name = copystring( progsname );
	vm.prog->builtins = NULL;
	vm.prog->numbuiltins = 0;
	vm.prog->max_edicts = 4096;
	vm.prog->limit_edicts = 4096;
	vm.prog->edictprivate_size = sizeof(vm_edict_t);
	vm.prog->error_cmd = VM_Error;
	vm.prog->flag |= PRVM_OP_STATE; // enable op_state feature
	vm.prog->progs_mempool = qccpool;

	PR_InitTypes();
}

void PRVM_Init( uint funcname, int argc, char **argv )
{
	char	dev_level[4];

	com_argc = argc;
	com_argv = argv;

	qccpool = Mem_AllocPool( "VM progs" );
	host_instance = funcname;

	if(FS_GetParmFromCmdLine("-dev", dev_level ))
		prvm_developer = com.atoi(dev_level);

	Cmd_AddCommand("prvm_edict", PRVM_ED_PrintEdict_f, "print all data about an entity number in the selected VM (server, client, menu)");
	Cmd_AddCommand("prvm_edicts", PRVM_ED_PrintEdicts_f, "set a property on an entity number in the selected VM (server, client, menu)");
	Cmd_AddCommand("prvm_edictcount", PRVM_ED_Count_f, "prints number of active entities in the selected VM (server, client, menu)");
	Cmd_AddCommand("prvm_profile", PRVM_Profile_f, "prints execution statistics about the most used QuakeC functions in the selected VM (server, client, menu)");
	Cmd_AddCommand("prvm_fields", PRVM_Fields_f, "prints usage statistics on properties (how many entities have non-zero values) in the selected VM (server, client, menu)");
	Cmd_AddCommand("prvm_globals", PRVM_Globals_f, "prints all global variables in the selected VM (server, client, menu)");
	Cmd_AddCommand("prvm_global", PRVM_Global_f, "prints value of a specified global variable in the selected VM (server, client, menu)");
	Cmd_AddCommand("prvm_globalset", PRVM_GlobalSet_f, "sets value of a specified global variable in the selected VM (server, client, menu)");
	Cmd_AddCommand("prvm_edictset", PRVM_ED_EdictSet_f, "changes value of a specified property of a specified entity in the selected VM (server, client, menu)");
	Cmd_AddCommand("prvm_printfunction", PRVM_PrintFunction_f, "prints a disassembly (QuakeC instructions) of the specified function in the selected VM (server, client, menu)");
	Cmd_AddCommand("compile", PRVM_Compile_f, "compile specified VM (server, client, menu), changes will take affect after map restart");

	// LordHavoc: optional runtime bounds checking (speed drain, but worth it for security, on by default - breaks most QCCX features (used by CRMod and others))
	prvm_boundscheck = Cvar_Get( "prvm_boundscheck", "0", 0, "enable vm internal boundschecker" );
	prvm_traceqc = Cvar_Get( "prvm_traceqc", "0", 0, "enable tracing (only for debug)" );
	prvm_statementprofiling = Cvar_Get ("prvm_statementprofiling", "0", 0, "counts how many times each QC statement has been executed" );
	prvm_maxedicts = Cvar_Get( "prvm_maxedicts", "4096", CVAR_SYSTEMINFO, "user limit edicts number fof server, client and renderer, absolute limit 65535" );

	if( host_instance == HOST_NORMAL || host_instance == HOST_DEDICATED )
	{
		// dump internal copies of progs into hdd if missing
		if(!FS_FileExists("vprogs/server.dat")) FS_WriteFile( "vprogs/server.dat", server_dat, sizeof(server_dat)); 
		if(!FS_FileExists("vprogs/client.dat")) FS_WriteFile( "vprogs/client.dat", client_dat, sizeof(client_dat));
		if(!FS_FileExists("vprogs/uimenu.dat")) FS_WriteFile( "vprogs/uimenu.dat", uimenu_dat, sizeof(uimenu_dat));
	}
}

void PRVM_Shutdown( void )
{
	Mem_FreePool( &qccpool );
}

void PRVM_PrepareProgs( const char *dir, const char *name )
{
	int	i;

	switch( host_instance )
	{
	case COMP_QCCLIB:
		FS_InitRootDir((char *)dir);
		PR_InitCompile( name );
		break;
	case RIPP_QCCDEC:
		FS_InitRootDir((char *)dir);
		PR_InitDecompile( name );
		break;
	case HOST_NORMAL:
	case HOST_DEDICATED:
		com_argc = Cmd_Argc();
		for( i = 0; i < com_argc; i++ )
			com_argv[i] = copystring(Cmd_Argv(i));
		com.strncpy( sourcedir, name, MAX_SYSPATH );
		PR_InitCompile( name );
		prvm_state = comp_begin;
		break;
	default:
		Sys_Break("PRVM_PrepareProgs: can't prepare progs for instance %d\n", host_instance );
		break;
	}
}

void PRVM_CompileProgs( void )
{
	PR_BeginCompilation();
	while(PR_ContinueCompile());
	PR_FinishCompilation();          
}

void PRVM_Frame( dword time )
{
	if(setjmp(pr_int_error))
		return;

	switch( prvm_state )
	{
	case comp_begin:
		PR_BeginCompilation();
		prvm_state = comp_frame;
		break;
	case comp_frame:
		if(PR_ContinueCompile());
		else prvm_state = comp_done;
		break;
	case comp_done:
		prvm_state = comp_inactive;
		PR_FinishCompilation();
		break;
	case comp_error:
		prvm_state = comp_inactive;
		break;
	case comp_inactive:
	default: return;
	}	 
}

bool PRVM_DecompileProgs( const char *name )
{
	return PR_Decompile( name );
}

void PRVM_Compile_f( void )
{
	if( Cmd_Argc() < 2)
	{
		Msg( "Usage: compile <program name> </D=DEFINE> </O<level>> </O<abbrev>>\n");
		return;
	}
	if( prvm_state != comp_inactive )
	{
		Msg("Compile already in progress, please wait\n" );
		return;
	}
	// engine already known about vprogs and vsource directory
	PRVM_PrepareProgs( NULL, Cmd_Argv( 1 ));
}

vprogs_exp_t DLLEXPORT *CreateAPI( stdlib_api_t *input, void *unused )
{
	com = *input;

	// generic functions
	vm.api_size = sizeof(vprogs_exp_t);

	vm.Init = PRVM_Init;
	vm.Free = PRVM_Shutdown;
	vm.PrepareDAT = PRVM_PrepareProgs;
	vm.CompileDAT = PRVM_CompileProgs;
	vm.DecompileDAT = PRVM_DecompileProgs;
	vm.Update = PRVM_Frame;

	vm.WriteGlobals = PRVM_ED_WriteGlobals;
	vm.ParseGlobals = PRVM_ED_ParseGlobals;
	vm.PrintEdict = PRVM_ED_Print;
	vm.WriteEdict = PRVM_ED_Write;
	vm.ParseEdict = PRVM_ED_ParseEdict;
	vm.AllocEdict = PRVM_ED_Alloc;
	vm.FreeEdict = PRVM_ED_Free;
	vm.IncreaseEdicts = PRVM_MEM_IncreaseEdicts;

	vm.LoadFromFile = PRVM_ED_LoadFromFile;
	vm.GetString = PRVM_GetString;
	vm.SetEngineString = PRVM_SetEngineString;
	vm.SetTempString = PRVM_SetTempString;
	vm.AllocString = PRVM_AllocString;
	vm.FreeString = PRVM_FreeString;

	vm.InitProg = PRVM_InitProg;
	vm.SetProg = PRVM_SetProg;
	vm.ProgLoaded = PRVM_ProgLoaded; 
	vm.LoadProgs = PRVM_LoadProgs;
	vm.ResetProg = PRVM_ResetProg;

	vm.FindFunctionOffset = PRVM_ED_FindFunctionOffset;
	vm.FindGlobalOffset = PRVM_ED_FindGlobalOffset;	
	vm.FindFieldOffset = PRVM_ED_FindFieldOffset;	
	vm.FindFunction = PRVM_ED_FindFunction;
	vm.FindGlobal = PRVM_ED_FindGlobal;
	vm.FindField = PRVM_ED_FindField;

	vm.StackTrace = PRVM_StackTrace;
	vm.Warning = VM_Warning;
	vm.Error = VM_Error;
	vm.ExecuteProgram = PRVM_ExecuteProgram;
	vm.Crash = PRVM_Crash;

	return &vm;
}