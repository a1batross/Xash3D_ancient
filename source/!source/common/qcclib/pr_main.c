//=======================================================================
//			Copyright XashXT Group 2007 й
//		        pr_main.c - QuakeC progs compiler
//=======================================================================

#include "qcclib.h"

#define defaultkeyword	(FLAG_MIDCOMPILE | FLAG_DEFAULT)
#define nondefaultkeyword	(FLAG_MIDCOMPILE)
#define defaultoption	(FLAG_MIDCOMPILE | FLAG_DEFAULT)

byte		*qccpool;
char		v_copyright[1024];
int		writeasm;
int		level;
uint		MAX_REGS;
int		MAX_ERRORS;
int		MAX_STRINGS;
int		MAX_GLOBALS;
int		MAX_FIELDS;
int		MAX_STATEMENTS;
int		MAX_FUNCTIONS;
int		MAX_CONSTANTS;
int		max_temps;
int		*qcc_tempofs;
int		tempsstart;
int		numtemps;
bool		compressoutput = false;
bool		compileactive = false;
char		destfile[1024];
float		*pr_globals;
uint		numpr_globals;
char		*strings;
int		strofs;
char		*qccmsrc;
char		*qccmsrc2;
char		qccmfilename[1024];
char		qccmprogsdat[1024];
char		qccmsourcedir[1024];
cachedsourcefile_t	*sourcefile;
dstatement_t	*statements;
int		numstatements;
int		*statement_linenums;
char		*originalqccmsrc;
dfunction_t	*functions;
int		numfunctions;
ddef_t		*qcc_globals;
int		numglobaldefs;
ddef_t		*fields;
int		numfielddefs;
int		numsounds;
int		numtextures;
int		nummodels;
int		numfiles;
PATHSTRING	*precache_sounds;
PATHSTRING	*precache_textures;
PATHSTRING	*precache_models;
PATHSTRING	*precache_files;
hashtable_t	compconstantstable;
hashtable_t	globalstable;
hashtable_t	localstable;
hashtable_t	intconstdefstable;
hashtable_t	floatconstdefstable;
hashtable_t	stringconstdefstable;
bool		pr_warning[WARN_MAX];
targetformat_t	targetformat;
bool		bodylessfuncs;
type_t		*qcc_typeinfo;
int		numtypeinfos;
int		maxtypeinfos;

optimisations_t optimisations[] =
{
	// level debug = include debug info
	{&opt_writesources,		"c", "write source",	FL_DBG,	FLAG_V7_ONLY	}, 
	{&opt_writelinenums,	"a", "write linenums",	FL_DBG,	FLAG_V7_ONLY	},
	{&opt_writetypes,		"b", "write types",		FL_DBG,	FLAG_V7_ONLY	},

	// level 0 = fixed some qcc errors
	{&opt_nonvec_parms,		"g", "fix nonvec parms",	FL_OP0,	FLAG_DEFAULT	},

	// level 1 = size optimizations
	{&opt_shortenifnots,	"f", "shorten if(!a)",	FL_OP1,	FLAG_DEFAULT	},
	{&opt_assignments,		"e", "assigments",		FL_OP1,	FLAG_DEFAULT	},
	{&opt_dupconstdefs,		"j", "no dup constants",	FL_OP1,	FLAG_DEFAULT	},
	{&opt_noduplicatestrings,	"k", "no dup strings",	FL_OP1,	0,		},
	{&opt_locals,		"l", "strip local names",	FL_OP1,	0		},
	{&opt_function_names,	"m", "strip func names",	FL_OP1,	0		},
	{&opt_filenames,		"n", "strip file names",	FL_OP1,	0		},
	{&opt_unreferenced,		"o", "strip unreferenced",	FL_OP1,	FLAG_DEFAULT	},
	{&opt_overlaptemps,		"p", "optimize overlaptemps", FL_OP1,	FLAG_DEFAULT	},
	{&opt_constantarithmatic,	"q", "precompute constnts",	FL_OP1,	FLAG_DEFAULT	},
	{&opt_compstrings,		"x", "deflate prog strings",	FL_OP1,	FLAG_V7_ONLY	},

	// level 2 = speed optimizations
	{&opt_constant_names,	"h", "strip const names",	FL_OP2,	0		},
	{&opt_precache_file,	"r", "strip precache files",	FL_OP2,	0		},
	{&opt_compfunctions,	"y", "deflate prog funcs",	FL_OP2,	FLAG_V7_ONLY	},

	// level 3 = dodgy optimizations
	{&opt_return_only,		"s", "optimize return calls",	FL_OP3,	0		},
	{&opt_compound_jumps,	"t", "optimize num of jumps",	FL_OP3,	0		},
	{&opt_stripfunctions,	"u", "strip functions",	FL_OP3,	0		},
	{&opt_constant_names_strings,	"i", "strip const strings",	FL_OP3,	0		},
	{&opt_compress_other,	"z", "deflate all prog",	FL_OP3,	FLAG_V7_ONLY	},
	
	// level 4 = use with caution, may be bugly
	{&opt_locals_marshalling,	"y", "reduce locals, buggly",	FL_OP4,	FLAG_V7_ONLY	},
	{&opt_vectorcalls,		"w", "optimize vector calls",	FL_OP4,	FLAG_V7_ONLY	},
	{NULL,			"",  "",			0,	0		},
};

compiler_flag_t compiler_flag[] = 
{
	// keywords
	{&keyword_asm,	defaultkeyword,	"asm"		},
	{&keyword_break,	defaultkeyword,	"break"		},
	{&keyword_case,	defaultkeyword,	"case"		},
	{&keyword_class,	defaultkeyword,	"class"		},
	{&keyword_const,	defaultkeyword,	"const"		},
	{&keyword_continue,	defaultkeyword,	"continue"	},
	{&keyword_default,	defaultkeyword,	"default"		},
	{&keyword_entity,	defaultkeyword,	"entity"		},
	{&keyword_enum,	defaultkeyword,	"enum"		}, // kinda like in c, but typedef not supported.
	{&keyword_enumflags,defaultkeyword,	"enumflags"	}, // like enum, but doubles instead of adds 1.
	{&keyword_extern,	defaultkeyword,	"extern"		}, // function is external, don't error or warn if the body was not found
	{&keyword_float,	defaultkeyword,	"float"		},
	{&keyword_for,	defaultkeyword,	"for"		},
	{&keyword_goto,	defaultkeyword,	"goto"		},
	{&keyword_int,	defaultkeyword,	"int"		},
	{&keyword_integer,	defaultkeyword,	"integer"		},
	{&keyword_noref,	defaultkeyword,	"noref"		}, // nowhere else references this, don't strip it.
	{&keyword_nosave,	defaultkeyword,	"nosave"		}, // don't write the def to the output.
	{&keyword_shared,	defaultkeyword,	"shared"		}, // mark global to be copied over when progs changes
	{&keyword_state,	nondefaultkeyword,	"state"		},
	{&keyword_string,	defaultkeyword,	"string"		},
	{&keyword_struct,	defaultkeyword,	"struct"		},
	{&keyword_switch,	defaultkeyword,	"switch"		},
	{&keyword_typedef,	defaultkeyword,	"typedef"		}, // FIXME
	{&keyword_union,	defaultkeyword,	"union"		}, // you surly know what a union is!
	{&keyword_var,	defaultkeyword,	"var"		},
	{&keyword_vector,	defaultkeyword,	"vector"		},


	// options
	{&keywords_coexist,	FLAG_DEFAULT,	"kce"		},//"Keywords Coexist",		"If you want keywords to NOT be disabled when they a variable by the same name is defined, check here."},
	{&output_parms,	0,	     	"parms"		},//"Define offset parms",	"if PARM0 PARM1 etc should be defined by the compiler. These are useful if you make use of the asm keyword for function calls, or you wish to create your own variable arguments. This is an easy way to break decompilers."},//controls weather to define PARMx for the parms (note - this can screw over some decompilers)
	{&autoprototype,	0,	     	"autoproto"	},//"Automatic Prototyping",	"Causes compilation to take two passes instead of one. The first pass, only the definitions are read. The second pass actually compiles your code. This means you never have to remember to prototype functions again."},	//so you no longer need to prototype functions and things in advance.
	{&writeasm,	0,		"wasm",		},//"Dump Assembler",		"Writes out a qc.asm which contains all your functions but in assembler. This is a great way to look for bugs in qcclib, but can also be used to see exactly what your functions turn into, and thus how to optimise statements better."},//spit out a qc.asm file, containing an assembler dump of the ENTIRE progs. (Doesn't include initialisation of constants)
	{&flag_ifstring,	FLAG_MIDCOMPILE,	"ifstring"	},//"if(string) fix",		"Causes if(string) to behave identically to if(string!="") This is most useful with addons of course, but also has adverse effects with FRIK_FILE's fgets, where it becomes impossible to determin the end of the file. In such a case, you can still use asm {IF string 2;RETURN} to detect eof and leave the function."},//correction for if(string) no-ifstring to get the standard behaviour.
	{&flag_laxcasts,	FLAG_MIDCOMPILE,	"lax"		},//"Lax type checks",		"Disables many errors (generating warnings instead) when function calls or operations refer to two normally incompatable types. This is required for reacc support, and can also allow certain (evil) mods to compile that were originally written for frikqcc."}, //Allow lax casting. This'll produce loadsa warnings of course. But allows compilation of certain dodgy code.
	{&flag_hashonly,	FLAG_MIDCOMPILE,	"hashonly"	},//"Hash-only constants",	"Allows use of only #constant for precompiler constants, allows certain preqcc using mods to compile"},
	{&opt_logicops,	FLAG_MIDCOMPILE,	"lo"		},//"Logic ops",		"This changes the behaviour of your code. It generates additional if operations to early-out in if statements. With this flag, the line if (0 && somefunction()) will never call the function. It can thus be considered an optimisation. However, due to the change of behaviour, it is not considered so by qcclib. Note that due to inprecisions with floats, this flag can cause runaway loop errors within the player walk and run functions. This code is advised:\nplayer_stand1:\n    if (self.velocity_x || self.velocity_y)\nplayer_run\n    if (!(self.velocity_x || self.velocity_y))"},
	{&flag_fastarrays,	defaultoption,	"fastarrays"	},//"fast arrays where possible",	"Generates extra instructions inside array handling functions to detect engine and use extension opcodes only in supporting engines.\nAdds a global which is set by the engine if the engine supports the extra opcodes. Note that this applies to all arrays or none."}, // correction for if(string) no-ifstring to get the standard behaviour.
	{NULL}
};

target_t targets[] = 
{
	{QCF_STANDARD,	"standard"},
	{QCF_STANDARD,	"q1"},
	{QCF_STANDARD,	"quakec"},
	{QCF_RELEASE,	"release"},
	{QCF_DEBUG,	"debug"},
	{0,		NULL}
};

void PR_CommandLinePrecompilerOptions (void)
{
	const_t	*cnst;
	int	level, i, j, p = 0;
	char	*name, *val;

	// default state
	for (i = 0; optimisations[i].enabled; i++) 
		*optimisations[i].enabled = false;

	for (i = 1; i < fs_argc; i++)
	{
		// #define
		if( !strnicmp(fs_argv[i], "/D", 2))
		{
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
		else if( !strnicmp(fs_argv[i], "/O", 2))
		{
			int currentlevel;
			name = fs_argv[i], name += 2;// skip "/O"
			level = atoi(name);

			if(fs_argv[i][2] == 'd') currentlevel = MASK_DEBUG; // disable optimizations
			else if (fs_argv[i][2] == '0') currentlevel = MASK_LEVEL_O;
			else if (fs_argv[i][2] == '1') currentlevel = MASK_LEVEL_1;
			else if (fs_argv[i][2] == '2') currentlevel = MASK_LEVEL_2;
			else if (fs_argv[i][2] == '3') currentlevel = MASK_LEVEL_3;
			else if (fs_argv[i][2] == '4') currentlevel = MASK_LEVEL_4;

			for (i = 0; optimisations[i].enabled; i++)
			{
				if(optimisations[i].levelmask & currentlevel)
				{
					*optimisations[i].enabled = true;
					Msg("use opt: \"%s\"\n", optimisations[i].fullname );
				}
			}
		}
		
		else if ( !strnicmp(fs_argv[i], "-K", 2) || !strnicmp(fs_argv[i], "/K", 2) )
		{
			p = 0;
			if (!strnicmp(fs_argv[i]+2, "no-", 3))
			{
				for (p = 0; compiler_flag[p].enabled; p++)
					if (!stricmp(fs_argv[i]+5, compiler_flag[p].name))
					{
						*compiler_flag[p].enabled = false;
						break;
					}
			}
			else
			{
				for (p = 0; compiler_flag[p].enabled; p++)
					if (!stricmp(fs_argv[i]+2, compiler_flag[p].name))
					{
						*compiler_flag[p].enabled = true;
						break;
					}
			}

			if (!compiler_flag[p].enabled)
				PR_Warning(0, NULL, WARN_BADPARAMS, "Unrecognised keyword parameter (%s)", fs_argv[i]);
		}
		else if ( !strnicmp(fs_argv[i], "-F", 2) || !strnicmp(fs_argv[i], "/F", 2) )
		{
			p = 0;
			if (!strnicmp(fs_argv[i]+2, "no-", 3))
			{
				for (p = 0; compiler_flag[p].enabled; p++)
					if (!stricmp(fs_argv[i]+5, compiler_flag[p].name))
					{
						*compiler_flag[p].enabled = false;
						break;
					}
			}
			else
			{
				for (p = 0; compiler_flag[p].enabled; p++)
					if (!stricmp(fs_argv[i]+2, compiler_flag[p].name))
					{
						*compiler_flag[p].enabled = true;
						break;
					}
			}

			if (!compiler_flag[p].enabled)
				PR_Warning(0, NULL, WARN_BADPARAMS, "Unrecognised flag parameter (%s)", fs_argv[i]);
		}


		else if ( !strncmp(fs_argv[i], "-T", 2) || !strncmp(fs_argv[i], "/T", 2) )
		{
			p = 0;
			for (p = 0; targets[p].name; p++)
				if (!stricmp(fs_argv[i]+2, targets[p].name))
				{
					targetformat = targets[p].target;
					break;
				}

			if (!targets[p].name)
				PR_Warning(0, NULL, WARN_BADPARAMS, "Unrecognised target parameter (%s)", fs_argv[i]);
		}

		else if ( !strnicmp(fs_argv[i], "-W", 2) || !strnicmp(fs_argv[i], "/W", 2) )
		{
			if (!stricmp(fs_argv[i]+2, "all"))
				memset(pr_warning, 0, sizeof(pr_warning));
			else if (!stricmp(fs_argv[i]+2, "none"))
				memset(pr_warning, 1, sizeof(pr_warning));
			else if (!stricmp(fs_argv[i]+2, "no-mundane"))
			{	//disable mundane performance/efficiency/blah warnings that don't affect code.
				pr_warning[WARN_SAMENAMEASGLOBAL] = true;
				pr_warning[WARN_DUPLICATEDEFINITION] = true;
				pr_warning[WARN_CONSTANTCOMPARISON] = true;
				pr_warning[WARN_ASSIGNMENTINCONDITIONAL] = true;
				pr_warning[WARN_DEADCODE] = true;
				pr_warning[WARN_NOTREFERENCEDCONST] = true;
				pr_warning[WARN_NOTREFERENCED] = true;
				pr_warning[WARN_POINTLESSSTATEMENT] = true;
				pr_warning[WARN_ASSIGNMENTTOCONSTANTFUNC] = true;
				pr_warning[WARN_BADPRAGMA] = true;	//C specs say that these should be ignored. We're close enough to C that I consider that a valid statement.
				pr_warning[WARN_IDENTICALPRECOMPILER] = true;
				pr_warning[WARN_UNDEFNOTDEFINED] = true;
			}
		}
	}
}


void PR_SetDefaultProperties (void)
{
	int i, p;



	for (p = 0; compiler_flag[p].enabled; p++)
		*compiler_flag[p].enabled = compiler_flag[p].flags & FLAG_DEFAULT;

	Hash_InitTable(&compconstantstable, MAX_CONSTANTS);

	ForcedCRC = 0;
	PR_DefineName("QCCLIB");

	//enable all warnings
	memset(pr_warning, 0, sizeof(pr_warning));

	//play with default warnings.
	pr_warning[WARN_NOTREFERENCEDCONST] = true;
	pr_warning[WARN_MACROINSTRING] = true;
//	pr_warning[WARN_ASSIGNMENTTOCONSTANT] = true;
	pr_warning[WARN_FIXEDRETURNVALUECONFLICT] = true;
	pr_warning[WARN_EXTRAPRECACHE] = true;
	pr_warning[WARN_DEADCODE] = true;
	pr_warning[WARN_INEFFICIENTPLUSPLUS] = true;
	pr_warning[WARN_EXTENSION_USED] = true;
	pr_warning[WARN_IFSTRING_USED] = true;

	//Check the command line
	PR_CommandLinePrecompilerOptions();

	//targetformat = QCF_STANDARD;

	//FIXME
	switch(targetformat)
	{
	case QCF_DEBUG:
	case QCF_RELEASE:
		strncpy(pevname, "pev", 3 );
		strncpy(pevname, "opev", 4 );
		break;
	default:
	case QCF_STANDARD:
		strncpy(pevname, "self", 4 );
		strncpy(opevname, "oself", 5 );

		for (i = 0; optimisations[i].enabled; i++)
		{
			if(*optimisations[i].enabled && optimisations[i].flags & FLAG_V7_ONLY)
			{
				*optimisations[i].enabled = false;
				Msg("\"%s\" not supported with standard target, disable\n", optimisations[i].fullname );
			}
		}
		break;
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

	precache_sounds = (void *)Qalloc(sizeof(char) * MAX_NAME * MAX_SOUNDS);
	numsounds = 0;
	precache_textures = (void *)Qalloc(sizeof(char) * MAX_NAME * MAX_TEXTURES);
	numtextures = 0;
	precache_models = (void *)Qalloc(sizeof(char) * MAX_NAME * MAX_MODELS);
	nummodels = 0;
	precache_files = (void *)Qalloc(sizeof(char) * MAX_NAME * MAX_FILES);
	numfiles = 0;

	qcc_typeinfo = (void *)Qalloc(sizeof(type_t) * maxtypeinfos);
	numtypeinfos = 0;

	qcc_tempofs = Qalloc(sizeof(int) * max_temps);
	tempsstart = 0;
	bodylessfuncs = 0;

	memset(&pr, 0, sizeof(pr));
	memset(&ret_temp, 0, sizeof(ret_temp));
	memset(pr_immediate_string, 0, sizeof(pr_immediate_string));

	if (opt_locals_marshalling) MsgWarn("Locals marshalling might be buggy. Use with caution\n");
	strncpy( qccmprogsdat, name, sizeof(qccmprogsdat));

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

void PR_WriteData (int crc)
{
	char		element[MAX_NAME];
	def_t		*def, *comp_x, *comp_y, *comp_z;
	ddef_t		*dd;
	dprograms_t	progs;
	vfile_t		*h;
	file_t		*pr_file;
	int		i, num_ref;
	bool		debugtarget = false;
	bool		types = false;
	int		outputsize = 16;

	progs.blockscompressed = 0;

	if (numstatements > MAX_STATEMENTS)
		Sys_Error("Too many statements - %i\nAdd \"MAX_STATEMENTS\" \"%i\" to qcc.cfg", numstatements, (numstatements+32768)&~32767);

	if (strofs > MAX_STRINGS)
		Sys_Error("Too many strings - %i\nAdd \"MAX_STRINGS\" \"%i\" to qcc.cfg", strofs, (strofs+32768)&~32767);

	PR_UnmarshalLocals();

	switch (targetformat)
	{
	case QCF_STANDARD:
		if (bodylessfuncs)
			Msg("Warning: There are some functions without bodies.\n");

		if (numpr_globals > 65530 )
		{
			Msg("Forcing target to RELEASE32 due to numpr_globals\n");
			outputsize = 32;
		}
		else
		{
			// not much of a different format. Rewrite output to get it working on original executors?
			if (numpr_globals >= 32768)
				Msg("Quake1 32bit virtual machine\nAn enhanced executor will be required\n");
			else Msg("Quake1 16bit virtual machine\n");
			break;
		}
		// intentional falltrough
		targetformat = QCF_RELEASE;
	case QCF_DEBUG:
	case QCF_RELEASE:
		if (targetformat == QCF_DEBUG)
			debugtarget = true;
		if (numpr_globals > 65530)
		{
			Msg("Using 32 bit target due to numpr_globals\n");
			outputsize = 32;
		}

		if(opt_compstrings) 
		{
			progs.blockscompressed |= COMP_STRINGS;
		}
		if(opt_compfunctions)
		{
			progs.blockscompressed |= COMP_FUNCTIONS;
			progs.blockscompressed |= COMP_STATEMENTS;
		}
		if(opt_compress_other)
		{
			progs.blockscompressed |= COMP_DEFS;
			progs.blockscompressed |= COMP_FIELDS;
			progs.blockscompressed |= COMP_GLOBALS;
		}

		//compression of blocks?
		if (compressoutput)	
		{
			progs.blockscompressed |= COMP_LINENUMS;
			progs.blockscompressed |= COMP_TYPES;
		}

		types = debugtarget; // include a type block?
		Msg("Xash3D virtual machine\n");
		break;
	}

	// part of how compilation works. This def is always present, and never used.
	def = PR_GetDef(NULL, "end_sys_globals", NULL, false, 0);
	if(def) def->references++;

	def = PR_GetDef(NULL, "end_sys_fields", NULL, false, 0);
	if(def) def->references++;

	for (def = pr.def_head.next; def; def = def->next)
	{
		if (def->type->type == ev_vector || (def->type->type == ev_field && def->type->aux_type->type == ev_vector))
		{	
			//do the references, so we don't get loadsa not referenced VEC_HULL_MINS_x
			sprintf(element, "%s_x", def->name);
			comp_x = PR_GetDef(NULL, element, def->scope, false, 0);
			sprintf(element, "%s_y", def->name);
			comp_y = PR_GetDef(NULL, element, def->scope, false, 0);
			sprintf(element, "%s_z", def->name);
			comp_z = PR_GetDef(NULL, element, def->scope, false, 0);

			num_ref = def->references;
			if (comp_x && comp_y && comp_z)
			{
				num_ref += comp_x->references;
				num_ref += comp_y->references;
				num_ref += comp_z->references;

				if (!def->references)
				{
					if (!comp_x->references || !comp_y->references || !comp_z->references)				
						num_ref = 0; // one of these vars is useless...
				}
				def->references = num_ref;

				if (!num_ref) num_ref = 1;
				if (comp_x) comp_x->references = num_ref;
				if (comp_y) comp_y->references = num_ref;
				if (comp_z) comp_z->references = num_ref;
			}
		}
		if (def->references <= 0)
		{
			if(def->local) PR_Warning(WARN_NOTREFERENCED, strings + def->s_file, def->s_line, "'%s' : unreferenced local variable", def->name);
			if (opt_unreferenced && def->type->type != ev_field) continue;
		}
		if (def->type->type == ev_function)
		{
			if (opt_function_names && functions[G_FUNCTION(def->ofs)].first_statement<0)
			{
				def->name = "";
			}
			if (!def->timescalled)
			{
				if (def->references <= 1)
					PR_Warning(WARN_DEADCODE, strings + def->s_file, def->s_line, "%s is never directly called or referenced (spawn function or dead code)", def->name);
			}
			if (opt_stripfunctions && def->timescalled >= def->references-1)	
			{
				// make sure it's not copied into a different var.
				// if it ever does self.think then it could be needed for saves.
				// if it's only ever called explicitly, the engine doesn't need to know.
				continue;
			}
		}
		else if (def->type->type == ev_field)
		{
			dd = &fields[numfielddefs];
			numfielddefs++;
			dd->type = def->type->aux_type->type;
			dd->s_name = PR_CopyString (def->name, opt_noduplicatestrings );
			dd->ofs = G_INT(def->ofs);
		}
		else if ((def->scope||def->constant) && (def->type->type != ev_string || opt_constant_names_strings))
		{
			if (opt_constant_names) continue;
		}

		dd = &qcc_globals[numglobaldefs];
		numglobaldefs++;

		if (types) dd->type = def->type-qcc_typeinfo;
		else dd->type = def->type->type;

		if ( def->saved && ((!def->initialized || def->type->type == ev_function) && def->type->type != ev_field && def->scope == NULL))
			dd->type |= DEF_SAVEGLOBAL;

		if (def->shared) dd->type |= DEF_SHARED;

		if (opt_locals && (def->scope || !strcmp(def->name, "IMMEDIATE")))
		{
			dd->s_name = 0;
		}
		else dd->s_name = PR_CopyString (def->name, opt_noduplicatestrings );
		dd->ofs = def->ofs;
	}

	if (numglobaldefs > MAX_GLOBALS)
		Sys_Error("Too many globals - %i\nAdd \"MAX_GLOBALS\" \"%i\" to qcc.cfg", numglobaldefs, (numglobaldefs+32768)&~32767);

	strofs = (strofs + 3) & ~3;

	Msg ("%6i strofs (of %i)\n", strofs, MAX_STRINGS);
	Msg ("%6i numstatements (of %i)\n", numstatements, MAX_STATEMENTS);
	Msg ("%6i numfunctions (of %i)\n", numfunctions, MAX_FUNCTIONS);
	Msg ("%6i numglobaldefs (of %i)\n", numglobaldefs, MAX_GLOBALS);
	Msg ("%6i numfielddefs (%i unique) (of %i)\n", numfielddefs, pr.size_fields, MAX_FIELDS);
	Msg ("%6i numpr_globals (of %i)\n", numpr_globals, MAX_REGS);	
	
	if(!*destfile) strcpy(destfile, "progs.dat");
	Msg("Writing %s\n", destfile);
	
	pr_file = FS_Open( destfile, "wb" );
	h = VFS_Open (pr_file, "w" );

	VFS_Write (h, &progs, sizeof(progs));
	VFS_Write (h, "\r\n\r\n", 4);
	VFS_Write (h, v_copyright, strlen(v_copyright) + 1);
	VFS_Write (h, "\r\n\r\n", 4);

	while(VFS_Tell(h) & 3)
	{
		// this is a lame way to do it
		VFS_Seek (h, 0, SEEK_CUR);
		VFS_Write (h, "\0", 1);
	}

	progs.ofs_strings = VFS_Tell(h);
	progs.numstrings = strofs;

	PR_WriteBlock(h, progs.ofs_strings, strings, strofs, progs.blockscompressed & COMP_STRINGS);

	progs.ofs_statements = VFS_Tell(h);
	progs.numstatements = numstatements;

	for (i = 0; i < numstatements; i++)

	switch(outputsize)
	{
	case 32:
		for (i = 0; i < numstatements; i++)
		{
			statements32[i].op = LittleLong(statements32[i].op);
			statements32[i].a = LittleLong(statements32[i].a);
			statements32[i].b = LittleLong(statements32[i].b);
			statements32[i].c = LittleLong(statements32[i].c);
		}

		PR_WriteBlock(h, progs.ofs_statements, statements32, progs.numstatements*sizeof(dstatement32_t), progs.blockscompressed & COMP_STATEMENTS);
		break;
	case 16:
	default:
		for (i = 0; i < numstatements; i++) // resize as we go - scaling down
		{
			statements16[i].op = LittleShort((word)statements32[i].op);
			if (statements32[i].a < 0) statements16[i].a = LittleShort((short)statements32[i].a);
			else statements16[i].a = (word)LittleShort((word)statements32[i].a);
			if (statements32[i].b < 0) statements16[i].b = LittleShort((short)statements32[i].b);
			else statements16[i].b = (word)LittleShort((word)statements32[i].b);
			if (statements32[i].c < 0) statements16[i].c = LittleShort((short)statements32[i].c);
			else statements16[i].c = (word)LittleShort((word)statements32[i].c);
		}
		PR_WriteBlock(h, progs.ofs_statements, statements16, progs.numstatements*sizeof(dstatement16_t), progs.blockscompressed & COMP_STATEMENTS);
		break;
	}

	progs.ofs_functions = VFS_Tell(h);
	progs.numfunctions = numfunctions;

	for (i = 0; i < numfunctions; i++)
	{
		functions[i].first_statement = LittleLong (functions[i].first_statement);
		functions[i].parm_start = LittleLong (functions[i].parm_start);
		functions[i].s_name = LittleLong (functions[i].s_name);
		functions[i].s_file = LittleLong (functions[i].s_file);
		functions[i].numparms = LittleLong ((functions[i].numparms > MAX_PARMS) ? MAX_PARMS : functions[i].numparms);
		functions[i].locals = LittleLong (functions[i].locals);
	}

	PR_WriteBlock(h, progs.ofs_functions, functions, progs.numfunctions*sizeof(dfunction_t), progs.blockscompressed & COMP_FUNCTIONS);

	switch(outputsize)
	{
	case 32:
		progs.ofs_globaldefs = VFS_Tell(h);
		progs.numglobaldefs = numglobaldefs;
		for (i = 0; i < numglobaldefs; i++)
		{
			qcc_globals32[i].type = LittleLong(qcc_globals32[i].type);
			qcc_globals32[i].ofs = LittleLong(qcc_globals32[i].ofs);
			qcc_globals32[i].s_name = LittleLong(qcc_globals32[i].s_name);
		}

		PR_WriteBlock(h, progs.ofs_globaldefs, qcc_globals32, progs.numglobaldefs*sizeof(ddef32_t), progs.blockscompressed & COMP_DEFS);

		progs.ofs_fielddefs = VFS_Tell(h);
		progs.numfielddefs = numfielddefs;

		for (i = 0; i < numfielddefs; i++)
		{
			fields32[i].type = LittleLong(fields32[i].type);
			fields32[i].ofs = LittleLong(fields32[i].ofs);
			fields32[i].s_name = LittleLong(fields32[i].s_name);
		}

		PR_WriteBlock(h, progs.ofs_fielddefs, fields32, progs.numfielddefs*sizeof(ddef32_t), progs.blockscompressed & COMP_FIELDS);
		break;
	case 16:
	default:
		progs.ofs_globaldefs = VFS_Tell(h);
		progs.numglobaldefs = numglobaldefs;
		for (i = 0; i < numglobaldefs; i++)
		{
			qcc_globals16[i].type = (word)LittleShort ((word)qcc_globals32[i].type);
			qcc_globals16[i].ofs = (word)LittleShort ((word)qcc_globals32[i].ofs);
			qcc_globals16[i].s_name = LittleLong(qcc_globals32[i].s_name);
		}

		PR_WriteBlock(h, progs.ofs_globaldefs, qcc_globals16, progs.numglobaldefs*sizeof(ddef16_t), progs.blockscompressed & COMP_DEFS);

		progs.ofs_fielddefs = VFS_Tell(h);
		progs.numfielddefs = numfielddefs;

		for (i = 0; i < numfielddefs; i++)
		{
			fields16[i].type = (word)LittleShort ((word)fields32[i].type);
			fields16[i].ofs = (word)LittleShort ((word)fields32[i].ofs);
			fields16[i].s_name = LittleLong (fields32[i].s_name);
		}

		PR_WriteBlock(h, progs.ofs_fielddefs, fields16, progs.numfielddefs*sizeof(ddef16_t), progs.blockscompressed & COMP_FIELDS);
		break;
	}

	progs.ofs_globals = VFS_Tell(h);
	progs.numglobals = numpr_globals;

	for (i = 0; (uint) i < numpr_globals; i++) ((int *)pr_globals)[i] = LittleLong (((int *)pr_globals)[i]);

	PR_WriteBlock(h, progs.ofs_globals, pr_globals, numpr_globals*4, progs.blockscompressed & COMP_GLOBALS);

	if(types)
	{
		for (i = 0; i < numtypeinfos; i++)
		{
			if (qcc_typeinfo[i].aux_type)
				qcc_typeinfo[i].aux_type = (type_t*)(qcc_typeinfo[i].aux_type - qcc_typeinfo);
			if (qcc_typeinfo[i].next)
				qcc_typeinfo[i].next = (type_t*)(qcc_typeinfo[i].next - qcc_typeinfo);
			qcc_typeinfo[i].name = (char *)PR_CopyString(qcc_typeinfo[i].name, true );
		}
	}

	progs.ofsfiles = 0;
	progs.ofslinenums = 0;
	progs.header = 0;
	progs.ofsbodylessfuncs = 0;
	progs.numbodylessfuncs = 0;
	progs.ofs_types = 0;
	progs.numtypes = 0;

	switch(targetformat)
	{
	case QCF_STANDARD:
		progs.version = QPROGS_VERSION; // QuakeC engine v 1.08
		break;
	case QCF_DEBUG:
	case QCF_RELEASE:
                    progs.version = VPROGS_VERSION;

		if (outputsize == 16) progs.header = VPROGSHEADER16;
		if (outputsize == 32) progs.header = VPROGSHEADER32;

		progs.ofsbodylessfuncs = VFS_Tell(h);
		progs.numbodylessfuncs = PR_WriteBodylessFuncs(h);		

		if (opt_writelinenums)
		{
			progs.ofslinenums = VFS_Tell(h);
			PR_WriteBlock(h, progs.ofslinenums, statement_linenums, numstatements*sizeof(int), progs.blockscompressed & COMP_LINENUMS);
		}
		else progs.ofslinenums = 0;

		if (opt_writetypes)
		{
			progs.ofs_types = VFS_Tell(h);
			progs.numtypes = numtypeinfos;
			PR_WriteBlock(h, progs.ofs_types, qcc_typeinfo, progs.numtypes*sizeof(type_t), progs.blockscompressed & COMP_TYPES);
		}
		else
		{
			progs.ofs_types = 0;
			progs.numtypes = 0;
		}
		progs.ofsfiles = PR_WriteSourceFiles(h, &progs, opt_writesources);
		break;
	}

	Msg ("%6i TOTAL SIZE\n", VFS_Tell(h));
	progs.entityfields = pr.size_fields;

	progs.crc = crc;

	// byte swap the header and write it out
	for (i = 0; i < sizeof(progs)/4; i++) ((int *)&progs)[i] = LittleLong (((int *)&progs)[i]);		
	
	VFS_Seek (h, 0, SEEK_SET);
	VFS_Write (h, &progs, sizeof(progs));

	VFS_Close (h); // write real file into disk
}

/*
==============
PR_BeginCompilation

called before compiling a batch of files, clears the pr struct
==============
*/
void PR_BeginCompilation ( void )
{

	int		i;
	char name[16];

	pr.def_tail = &pr.def_head;

	PR_ResetErrorScope();
	pr_scope = NULL;

/*	numpr_globals = RESERVED_OFS;	
	
	for (i=0 ; i<RESERVED_OFS ; i++)
		pr_global_defs[i] = &def_void;
*/
	
	type_void = PR_NewType("void", ev_void);
	type_string = PR_NewType("string", ev_string);
	type_float = PR_NewType("float", ev_float);
	type_vector = PR_NewType("vector", ev_vector);
	type_entity = PR_NewType("entity", ev_entity);
	type_field = PR_NewType("field", ev_field);	
	type_function = PR_NewType("function", ev_function);
	type_pointer = PR_NewType("pointer", ev_pointer);	
	type_integer = PR_NewType("__integer", ev_integer);
	type_variant = PR_NewType("__variant", ev_variant);

	type_floatfield = PR_NewType("fieldfloat", ev_field);
	type_floatfield->aux_type = type_float;
	type_pointer->aux_type = PR_NewType("pointeraux", ev_float);

	type_function->aux_type = type_void;

	//type_field->aux_type = type_float;

	if (keyword_int)
		PR_NewType("int", ev_integer);
	if (keyword_integer)
		PR_NewType("integer", ev_integer);
	


	if (output_parms)
	{	//this tends to confuse the brains out of decompilers. :)
		numpr_globals = 1;
		PR_GetDef(type_vector, "RETURN", NULL, true, 1)->references++;
		for (i = 0; i < MAX_PARMS; i++)
		{
			sprintf(name, "PARM%i", i);
			PR_GetDef(type_vector, name, NULL, true, 1)->references++;
		}
	}
	else
	{
		numpr_globals = RESERVED_OFS;
//		for (i=0 ; i<RESERVED_OFS ; i++)
//			pr_global_defs[i] = NULL;
	}

// link the function type in so state forward declarations match proper type
	pr.types = NULL;
//	type_function->next = NULL;
	pr_error_count = 0;
	pr_total_error_count = 0;
	pr_warning_count = 0;
	recursivefunctiontype = 0;

	freeofs = NULL;
	sprintf (qccmprogsdat, "%sprogs.src", qccmsourcedir);
	qccmsrc = QCC_LoadFile (qccmprogsdat);

	if (writeasm)
	{
		asmfile = FS_Open("qc.asm", "wb" );
		if (!asmfile) Sys_Error ("Couldn't open file for asm output.");
	}

	while(*qccmsrc && *qccmsrc < ' ')
		qccmsrc++;

	pr_file_p = SC_ParseToken(&qccmsrc);
	strcpy (destfile, token);
	FS_StripExtension( token );

	// msvc6.0 style message
	Msg("------------Configuration: %s - Vm16 %s------------\n", token, level <= 0 ? "Debug" : "Release" ); 
	
	pr_dumpasm = false;
	currentchunk = NULL;
	originalqccmsrc = qccmsrc;
	compileactive = true;
}

//=============================================================================




//============================================================================

/*
============
main
============
*/



void PR_FinishCompile(void);

// called between exe frames - won't loose net connection (is the theory)...
void PR_ContinueCompile(void)
{
	char *s, *s2;
	currentchunk = NULL;
	if (!compileactive) return;

	SC_ParseToken(&qccmsrc);

	if (!qccmsrc)
	{
		if (autoprototype)
		{
			qccmsrc = originalqccmsrc;
			PR_SetDefaultProperties();
			autoprototype = false;
			Msg("Compiling...\n");
			return;
		}
		PR_FinishCompile();
		return;
	}
	s = token;

	strcpy (qccmfilename, qccmsourcedir);
	while(1)
	{
		if (!strncmp(s, "..\\", 3))
		{
			s2 = qccmfilename + strlen(qccmfilename)-2;
			while (s2>=qccmfilename)
			{
				if (*s2 == '/' || *s2 == '\\')
				{
					s2[1] = '\0';
					break;
				}
				s2--;
			}
			s+=3;
			continue;
		}
		if (!strncmp(s, ".\\", 2))
		{
			s+=2;
			continue;
		}

		break;
	}
	strcat (qccmfilename, s);
	Msg ("%s\n", qccmfilename);

	qccmsrc2 = QCC_LoadFile (qccmfilename);
	PR_CompileFile (qccmsrc2, qccmfilename);
}

void PR_FinishCompile(void)
{	
	int	crc;
	currentchunk = NULL;

	if (setjmp(pr_parse_abort)) Sys_Error(""); // freeze console
	if (!PR_FinishCompilation()) 
	{
		Sys_Error("%s - %i error(s), %i warning(s)\n", destfile, pr_error_count, pr_warning_count);
	}	
	// write progdefs.h
	crc = PR_WriteProgdefs ("progdefs.h");
	
	// write data file
	PR_WriteData (crc);
	
	// report the data files
	if (numsounds > 0) Msg ("%3i unique precache_sounds\n", numsounds);
	if (nummodels > 0) Msg ("%3i unique precache_models\n", nummodels);
	if (numtextures > 0) Msg ("%3i unique precache_textures\n", numtextures);
	if (numfiles > 0) Msg ("%3i unique precache_files\n", numfiles);
	
	Msg ("Скопировано файлов:         1.\n\n");// enigma from M$ :)
	Msg("%s - %i error(s), %i warning(s)\n", destfile, pr_total_error_count, pr_warning_count);

	compileactive = false;
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

	if (autoprototype) Msg ("Prototyping...\n");
	else Msg("Compiling...\n");
	while(compileactive)
		PR_ContinueCompile();

	if (asmfile) FS_Close(asmfile);
          
	Mem_FreePool( &qccpool );
	return true;
}