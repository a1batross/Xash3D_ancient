//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        pr_main.c - QuakeC progs compiler
//=======================================================================

#include "qcclib.h"

byte		*qccpool;
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
hashtable_t	compconstantstable;
hashtable_t	globalstable;
hashtable_t	localstable;
hashtable_t	intconstdefstable;
hashtable_t	floatconstdefstable;
hashtable_t	stringconstdefstable;
bool		pr_warning[WARN_MAX];
int		target_version;
bool		bodylessfuncs;
type_t		*qcc_typeinfo;
int		numtypeinfos;
int		maxtypeinfos;

void PR_SetDefaultProperties (void)
{
	const_t	*cnst;
	int	i, j;
	char	*name, *val;

	Hash_InitTable(&compconstantstable, MAX_CONSTANTS);
	
	ForcedCRC = 0;
	// enable all warnings
	memset(pr_warning, 0, sizeof(pr_warning));

	// reset all optimizarions
	for (i = 0; pr_optimisations[i].enabled; i++) 
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

	for (i = 1; i < fs_argc; i++)
	{
		if( !strnicmp(fs_argv[i], "/D", 2))
		{
			// #define
			name = fs_argv[i+1];
			val = strchr(name, '=');
			if (val) { *val = '\0', val++; }
			cnst = PR_DefineName(name);
			if (val)
			{
				if(strlen(val) + 1 >= sizeof(cnst->value))
					PR_ParseError(ERR_INTERNAL, "compiler constant value is too long\n");
				strncpy(cnst->value, val, sizeof(cnst->value)-1);
				PR_Message("value %s\n", val );
				cnst->value[sizeof(cnst->value)-1] = '\0';
			}
		}
		else if ( !strnicmp(fs_argv[i], "/V", 2))
		{
			// target version
			if (fs_argv[i][2] == '6') target_version = QPROGS_VERSION;
			else if (fs_argv[i][2] == '7') target_version = FPROGS_VERSION;
			else if (fs_argv[i][2] == '8') target_version = VPROGS_VERSION;
			else PR_Warning(0, NULL, WARN_BADPARAMS, "Unrecognised version parametr (%s)", fs_argv[i]);
		}
		else if( !strnicmp(fs_argv[i], "/O", 2))
		{
			int currentlevel = 0; // optimization level
			if(fs_argv[i][2] == 'd') currentlevel = MASK_DEBUG; // disable optimizations
			else if (fs_argv[i][2] == '0') currentlevel = MASK_LEVEL_O;
			else if (fs_argv[i][2] == '1') currentlevel = MASK_LEVEL_1;
			else if (fs_argv[i][2] == '2') currentlevel = MASK_LEVEL_2;
			else if (fs_argv[i][2] == '3') currentlevel = MASK_LEVEL_3;
			else if (fs_argv[i][2] == '4') currentlevel = MASK_LEVEL_4;

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
				char *abbrev = fs_argv[i]+2; // custom optimisation
				for (j = 0; pr_optimisations[j].enabled; j++)
				{
					if(!strcmp(pr_optimisations[j].shortname, abbrev)) 
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
void PR_Init( const char *name )
{
	static char	parmname[12][MAX_PARMS];
	static temp_t	ret_temp;
	int		i;

	strncat(v_copyright, "This file was created with Xash3D QuakeC compiler,\n", sizeof(v_copyright));
	strncat(v_copyright, "who based on original code of ForeThought's QuakeC compiler.\n", sizeof(v_copyright));
	strncat(v_copyright, "Thanks to ID Software at all.", sizeof(v_copyright));

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

	if (opt_locals_marshalling) MsgWarn("Locals marshalling might be buggy. Use with caution\n");
	strncpy( sourcefilename, name, sizeof(sourcefilename));

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
		sprintf(parmname[i], "parm%i", i);
	}
}

bool PrepareDATProgs ( const char *dir, const char *name, byte params )
{
	qccpool = Mem_AllocPool( "QCC Compiler" );

	FS_InitRootDir( (char *)dir );
	PR_Init( name );

	return true;
}

bool CompileDATProgs ( void )
{
	PR_BeginCompilation();
	while(PR_ContinueCompile());
	PR_FinishCompilation();          

	Mem_FreePool( &qccpool );

	return true;
}