//=======================================================================
//			Copyright XashXT Group 2007 ©
//			pr_comp.c - progs compiler base
//=======================================================================

#include "qcclib.h"

#define TOP_PRIORITY	7
#define NOT_PRIORITY	5

// standard qcc keywords
#define keyword_do		1
#define keyword_return	1
#define keyword_if		1
#define keyword_else	1
#define keyword_local	1
#define keyword_while	1

// extended keywords.
bool keyword_asm;
bool keyword_break;
bool keyword_case;
bool keyword_class;
bool keyword_const;		// FIXME
bool keyword_continue;
bool keyword_default;
bool keyword_entity;	// for skipping the local
bool keyword_float;		// for skipping the local
bool keyword_for;
bool keyword_goto;
bool keyword_int;		// for skipping the local
bool keyword_integer;	// for skipping the local
bool keyword_state;
bool keyword_string;	// for skipping the local
bool keyword_struct;
bool keyword_switch;
bool keyword_var;		// allow it to be initialised and set around the place.
bool keyword_vector;	// for skipping the local
bool keyword_enum;		// kinda like in c, but typedef not supported.
bool keyword_enumflags;	// like enum, but doubles instead of adds 1.
bool keyword_typedef;	// FIXME
bool keyword_extern;	// function is external, don't error or warn if the body was not found
bool keyword_shared;	// mark global to be copied over when progs changes
bool keyword_noref;		// nowhere else references this, don't strip it.
bool keyword_nosave;	// don't write the def to the output.
bool keyword_union;		// you surly know what a union is!

bool keywords_coexist;	// don't disable a keyword simply because a var was made with the same name.
bool output_parms;		// emit some PARMX fields. confuses decompilers.
bool autoprototype;		// take two passes over the source code. First time round doesn't enter and functions or initialise variables.
bool pr_subscopedlocals;	// causes locals to be valid ONLY within thier statement block. (they simply can't be referenced by name outside of it)
bool flag_ifstring;		// makes if (blah) equivelent to if (blah != "") which resolves some issues in multiprogs situations.
bool flag_laxcasts;		// Allow lax casting. This'll produce loadsa warnings of course. But allows compilation of certain dodgy code.
bool flag_hashonly;		// Allows use of only #constant for precompiler constants, allows certain preqcc using mods to compile
bool flag_fastarrays;	// Faster arrays, dynamically detected, activated only in supporting engines.

bool opt_overlaptemps;	// reduce numpr_globals by reuse of temps. When they are not needed they are freed for reuse. The way this is implemented is better than frikqcc's. (This is the single most important optimisation)
bool opt_assignments;	// STORE_F isn't used if an operation wrote to a temp.
bool opt_shortenifnots;	// if(!var) is made an IF rather than NOT IFNOT
bool opt_noduplicatestrings;	// brute force string check. time consuming but more effective than the equivelent in frikqcc.
bool opt_constantarithmatic;	// 3*5 appears as 15 instead of the extra statement.
bool opt_nonvec_parms;	// store_f instead of store_v on function calls, where possible.
bool opt_constant_names;	// take out the defs and name strings of constants.
bool opt_constant_names_strings;//removes the defs of strings too. plays havok with multiprogs.
bool opt_precache_file;	// remove the call, the parameters, everything.
bool opt_filenames;		// strip filenames. hinders older decompilers.
bool opt_unreferenced;	// strip defs that are not referenced.
bool opt_function_names;	// strip out the names of builtin functions.
bool opt_locals;		// strip out the names of locals and immediates.
bool opt_dupconstdefs;	// float X = 5; and float Y = 5; occupy the same global with this.
bool opt_return_only;	// RETURN; DONE; at the end of a function strips out the done statement if there is no way to get to it.
bool opt_compound_jumps;	// jumps to jump statements jump to the final point.
bool opt_stripfunctions;	// if a functions is only ever called directly or by exe, don't emit the def.
bool opt_locals_marshalling;	// make the local vars of all functions occupy the same globals.
bool opt_logicops;		// don't make conditions enter functions if the return value will be discarded due to a previous value. (C style if statements)
bool opt_vectorcalls;	// vectors can be packed into 3 floats, which can yield lower numpr_globals, but cost two more statements per call (only works for q1 calling conventions).
bool opt_simplifiedifs;	// if (f != 0) -> if (f). if (f == 0) -> ifnot (f)

bool		simplestore;
file_t		*asmfile;
pr_info_t		pr;
freeoffset_t	*freeofs;
int		conditional;
int		basictypefield[ev_union + 1];

char *basictypenames[] = 
{
	"void",
	"string",
	"float",
	"vector",
	"entity",
	"field",
	"function",
	"pointer",
	"integer",
	"struct",
	"union"
};

//========================================

def_t	*pr_scope;	// the function being parsed, or NULL
type_t	*pr_classtype;
bool	pr_dumpasm;
string_t	s_file, s_file2;	// filename for function definition
uint	locals_start;	// for tracking local variables vs temps
uint	locals_end;	// for tracking local variables vs temps
jmp_buf	pr_parse_abort;	// longjump with this on parse error
void	PR_ParseDefs (char *classname);
bool	qcc_usefulstatement;
int	max_breaks;
int	max_continues;
int	max_cases;
int	num_continues;
int	num_breaks;
int	num_cases;
int	*pr_breaks;
int	*pr_continues;
int	*pr_cases;
def_t	**pr_casesdef;
def_t	**pr_casesdef2;

typedef struct
{
	int statementno;
	int lineno;
	char name[256];
} gotooperator_t;

int		max_labels;
int		max_gotos;
gotooperator_t	*pr_labels;
gotooperator_t	*pr_gotos;
int		num_gotos;
int		num_labels;
temp_t		*functemps; // floats/strings/funcs/ents...
def_t		*extra_parms[MAX_PARMS_EXTRA];
//========================================

// FIXME: modifiy list so most common GROUPS are first
// use look up table for value of first char and sort by first char and most common...?

// if true, effectivly { b = a; return a; }
opcode_t pr_opcodes[] =
{
// VERSION 6
{6, "<DONE>",	"DONE",		-1,	ASSOC_LEFT, &type_void,	&type_void,	&type_void},
{6, "*",		"MUL_F",		3,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{6, "*",		"MUL_V",		3,	ASSOC_LEFT, &type_vector,	&type_vector,	&type_float},
{6, "*",		"MUL_FV",		3,	ASSOC_LEFT, &type_float,	&type_vector,	&type_vector},
{6, "*",		"MUL_VF",		3,	ASSOC_LEFT, &type_vector,	&type_float,	&type_vector},
{6, "/",		"DIV_F",		3,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{6, "+",		"ADD_F",		4,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{6, "+",		"ADD_V",		4,	ASSOC_LEFT, &type_vector,	&type_vector,	&type_vector},
{6, "-",		"SUB_F",		4,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{6, "-",		"SUB_V",		4,	ASSOC_LEFT, &type_vector,	&type_vector,	&type_vector},
{6, "==",		"EQ_F",		5,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{6, "==",		"EQ_V",		5,	ASSOC_LEFT, &type_vector,	&type_vector,	&type_float},
{6, "==",		"EQ_S",		5,	ASSOC_LEFT, &type_string,	&type_string,	&type_float},
{6, "==",		"EQ_E",		5,	ASSOC_LEFT, &type_entity,	&type_entity,	&type_float},
{6, "==",		"EQ_FNC",		5,	ASSOC_LEFT, &type_function,	&type_function,	&type_float},
{6, "!=",		"NE_F",		5,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{6, "!=",		"NE_V",		5,	ASSOC_LEFT, &type_vector,	&type_vector,	&type_float},
{6, "!=",		"NE_S",		5,	ASSOC_LEFT, &type_string,	&type_string,	&type_float},
{6, "!=",		"NE_E",		5,	ASSOC_LEFT, &type_entity,	&type_entity,	&type_float},
{6, "!=",		"NE_FNC",		5,	ASSOC_LEFT, &type_function,	&type_function,	&type_float},
{6, "<=",		"LE",		5,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{6, ">=",		"GE",		5,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{6, "<",		"LT",		5,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{6, ">",		"GT",		5,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{6, ".",		"INDIRECT_F",	1,	ASSOC_LEFT, &type_entity,	&type_field,	&type_float},
{6, ".",		"INDIRECT_V",	1,	ASSOC_LEFT, &type_entity,	&type_field,	&type_vector},
{6, ".",		"INDIRECT_S",	1,	ASSOC_LEFT, &type_entity,	&type_field,	&type_string},
{6, ".",		"INDIRECT_E",	1,	ASSOC_LEFT, &type_entity,	&type_field,	&type_entity},
{6, ".",		"INDIRECT_FI",	1,	ASSOC_LEFT, &type_entity,	&type_field,	&type_field},
{6, ".",		"INDIRECT_FU",	1,	ASSOC_LEFT, &type_entity,	&type_field,	&type_function},
{6, ".",		"ADDRESS",	1,	ASSOC_LEFT, &type_entity,	&type_field,	&type_pointer},
{6, "=",		"STORE_F",	6,	ASSOC_RIGHT,&type_float,	&type_float,	&type_float},
{6, "=",		"STORE_V",	6,	ASSOC_RIGHT,&type_vector,	&type_vector,	&type_vector},
{6, "=",		"STORE_S",	6,	ASSOC_RIGHT,&type_string,	&type_string,	&type_string},
{6, "=",		"STORE_ENT",	6,	ASSOC_RIGHT,&type_entity,	&type_entity,	&type_entity},
{6, "=",		"STORE_FLD",	6,	ASSOC_RIGHT,&type_field,	&type_field,	&type_field},
{6, "=",		"STORE_FNC",	6,	ASSOC_RIGHT,&type_function,	&type_function,	&type_function},
{6, "=",		"STOREP_F",	6,	ASSOC_RIGHT,&type_pointer,	&type_float,	&type_float},
{6, "=",		"STOREP_V",	6,	ASSOC_RIGHT,&type_pointer,	&type_vector,	&type_vector},
{6, "=",		"STOREP_S",	6,	ASSOC_RIGHT,&type_pointer,	&type_string,	&type_string},
{6, "=",		"STOREP_ENT",	6,	ASSOC_RIGHT,&type_pointer,	&type_entity,	&type_entity},
{6, "=",		"STOREP_FLD",	6,	ASSOC_RIGHT,&type_pointer,	&type_field,	&type_field},
{6, "=",		"STOREP_FNC",	6,	ASSOC_RIGHT,&type_pointer,	&type_function,	&type_function},
{6, "<RETURN>",	"RETURN",		-1,	ASSOC_LEFT, &type_float,	&type_void,	&type_void},
{6, "!",		"NOT_F",		-1,	ASSOC_LEFT, &type_float,	&type_void,	&type_float},
{6, "!",		"NOT_V",		-1,	ASSOC_LEFT, &type_vector,	&type_void,	&type_float},
{6, "!",		"NOT_S",		-1,	ASSOC_LEFT, &type_vector,	&type_void,	&type_float},
{6, "!",		"NOT_ENT",	-1,	ASSOC_LEFT, &type_entity,	&type_void,	&type_float},
{6, "!",		"NOT_FNC",	-1,	ASSOC_LEFT, &type_function,	&type_void,	&type_float},
{6, "<IF>",	"IF",		-1,	ASSOC_RIGHT,&type_float,	NULL,		&type_void},
{6, "<IFNOT>",	"IFNOT",		-1,	ASSOC_RIGHT,&type_float,	NULL,		&type_void},
{6, "<CALL0>",	"CALL0",		-1,	ASSOC_LEFT, &type_function,	&type_void,	&type_void},
{6, "<CALL1>",	"CALL1",		-1,	ASSOC_LEFT, &type_function,	&type_void,	&type_void},
{6, "<CALL2>",	"CALL2",		-1,	ASSOC_LEFT, &type_function,	&type_void,	&type_void}, 
{6, "<CALL3>",	"CALL3",		-1,	ASSOC_LEFT, &type_function,	&type_void,	&type_void}, 
{6, "<CALL4>",	"CALL4",		-1,	ASSOC_LEFT, &type_function,	&type_void,	&type_void},
{6, "<CALL5>",	"CALL5",		-1,	ASSOC_LEFT, &type_function,	&type_void,	&type_void},
{6, "<CALL6>",	"CALL6",		-1,	ASSOC_LEFT, &type_function,	&type_void,	&type_void},
{6, "<CALL7>",	"CALL7",		-1,	ASSOC_LEFT, &type_function,	&type_void,	&type_void},
{6, "<CALL8>",	"CALL8",		-1,	ASSOC_LEFT, &type_function,	&type_void,	&type_void},
{6, "<STATE>",	"STATE",		-1,	ASSOC_LEFT, &type_float,	&type_float,	&type_void},
{6, "<GOTO>",	"GOTO",		-1,	ASSOC_RIGHT,NULL,		&type_void,	&type_void},
{6, "&&",		"AND",		7,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{6, "||",		"OR",		7,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{6, "&",		"BITAND",		3,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{6, "|",		"BITOR",		3,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
// VERSION 7
{7, "*=",		"MULSTORE_F",	6,	ASSOC_RIGHT,&type_float,	&type_float,	&type_float},
{7, "*=",		"MULSTORE_V",	6,	ASSOC_RIGHT,&type_vector,	&type_float,	&type_vector},
{7, "*=",		"MULSTOREP_F",	6,	ASSOC_RIGHT,&type_pointer,	&type_float,	&type_float},
{7, "*=",		"MULSTOREP_V",	6,	ASSOC_RIGHT,&type_pointer,	&type_float,	&type_vector},
{7, "/=",		"DIVSTORE_F",	6,	ASSOC_RIGHT,&type_float,	&type_float,	&type_float},
{7, "/=",		"DIVSTOREP_F",	6,	ASSOC_RIGHT,&type_pointer,	&type_float,	&type_float},
{7, "+=",		"ADDSTORE_F",	6,	ASSOC_RIGHT,&type_float,	&type_float,	&type_float},
{7, "+=",		"ADDSTORE_V",	6,	ASSOC_RIGHT,&type_vector,	&type_vector,	&type_vector},
{7, "+=",		"ADDSTOREP_F",	6,	ASSOC_RIGHT,&type_pointer,	&type_float,	&type_float},
{7, "+=",		"ADDSTOREP_V",	6,	ASSOC_RIGHT,&type_pointer,	&type_vector,	&type_vector},
{7, "-=",		"SUBSTORE_F",	6,	ASSOC_RIGHT,&type_float,	&type_float,	&type_float},
{7, "-=",		"SUBSTORE_V",	6,	ASSOC_RIGHT,&type_vector,	&type_vector,	&type_vector},
{7, "-=",		"SUBSTOREP_F",	6,	ASSOC_RIGHT,&type_pointer,	&type_float,	&type_float},
{7, "-=",		"SUBSTOREP_V",	6,	ASSOC_RIGHT,&type_pointer,	&type_vector,	&type_vector},
{7, "<FETCH_GBL_F>","FETCH_GBL_F",	-1,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{7, "<FETCH_GBL_V>","FETCH_GBL_V",	-1,	ASSOC_LEFT, &type_vector,	&type_float,	&type_vector},
{7, "<FETCH_GBL_S>","FETCH_GBL_S",	-1,	ASSOC_LEFT, &type_string,	&type_float,	&type_string},
{7, "<FETCH_GBL_E>","FETCH_GBL_E",	-1,	ASSOC_LEFT, &type_entity,	&type_float,	&type_entity},
{7, "<FETCH_G_FNC>","FETCH_G_FNC",	-1,	ASSOC_LEFT, &type_function,	&type_float,	&type_function},
{7, "<CSTATE>",	"CSTATE",		-1,	ASSOC_LEFT, &type_float,	&type_float,	&type_void},
{7, "<CWSTATE>",	"CWSTATE",	-1,	ASSOC_LEFT, &type_float,	&type_float,	&type_void},
{7, "|=",		"BITSET_F",	6,	ASSOC_RIGHT,&type_float,	&type_float,	&type_float},
{7, "|=",		"BITSETP_F",	6,	ASSOC_RIGHT,&type_pointer,	&type_float,	&type_float},
{7, "(-)",	"BITCLR_F",	6,	ASSOC_RIGHT,&type_float,	&type_float,	&type_float},
{7, "(-)",	"BITCLRP_F",	6,	ASSOC_RIGHT,&type_pointer,	&type_float,	&type_float},
{7, "<SWITCH_F>",	"SWITCH_F",	-1,	ASSOC_LEFT, &type_void,	NULL,		&type_void},
{7, "<SWITCH_V>",	"SWITCH_V",	-1,	ASSOC_LEFT, &type_void,	NULL,		&type_void},
{7, "<SWITCH_S>",	"SWITCH_S",	-1,	ASSOC_LEFT, &type_void,	NULL,		&type_void},
{7, "<SWITCH_E>",	"SWITCH_E",	-1,	ASSOC_LEFT, &type_void,	NULL,		&type_void},
{7, "<SWITCH_FNC>",	"SWITCH_FNC",	-1,	ASSOC_LEFT, &type_void,	NULL,		&type_void},
{7, "<CASE>",	"CASE",		-1,	ASSOC_LEFT, &type_void,	NULL,		&type_void},
{7, "<CASERANGE>",	"CASERANGE",	-1,	ASSOC_LEFT, &type_void,	&type_void,	NULL},
{7, "=",		"STORE_I",	6,	ASSOC_RIGHT,&type_integer,	&type_integer,	&type_integer},
{7, "=",		"STORE_IF",	6,	ASSOC_RIGHT,&type_integer,	&type_float,	&type_integer},
{7, "=",		"STORE_FI",	6,	ASSOC_RIGHT,&type_float,	&type_integer,	&type_float},
{7, "+",		"ADD_I",		4,	ASSOC_LEFT, &type_integer,	&type_integer,	&type_integer},
{7, "+",		"ADD_FI",		4,	ASSOC_LEFT, &type_float,	&type_integer,	&type_float},
{7, "+",		"ADD_IF",		4,	ASSOC_LEFT, &type_integer,	&type_float,	&type_float},
{7, "-",		"SUB_I",		4,	ASSOC_LEFT, &type_integer,	&type_integer,	&type_integer},
{7, "-",		"SUB_FI",		4,	ASSOC_LEFT, &type_float,	&type_integer,	&type_float},
{7, "-",		"SUB_IF",		4,	ASSOC_LEFT, &type_integer,	&type_float,	&type_float},
{7, "<CIF>",	"C_ITOF",		-1,	ASSOC_LEFT, &type_integer,	&type_void,	&type_float},
{7, "<CFI>",	"C_FTOI",		-1,	ASSOC_LEFT, &type_float,	&type_void,	&type_integer},
{7, "<CPIF>",	"CP_ITOF",	-1,	ASSOC_LEFT, &type_pointer,	&type_integer,	&type_float},
{7, "<CPFI>",	"CP_FTOI",	-1,	ASSOC_LEFT, &type_pointer,	&type_float,	&type_integer},
{7, ".",		"INDIRECT",	1,	ASSOC_LEFT, &type_entity,	&type_field,	&type_integer},
{7, "=",		"STOREP_I",	6,	ASSOC_RIGHT,&type_pointer,	&type_integer,	&type_integer},
{7, "=",		"STOREP_IF",	6,	ASSOC_RIGHT,&type_pointer,	&type_float,	&type_integer},
{7, "=",		"STOREP_FI",	6,	ASSOC_RIGHT,&type_pointer,	&type_integer,	&type_float},
{7, "&",		"BITAND_I",	3,	ASSOC_LEFT, &type_integer,	&type_integer,	&type_integer},
{7, "|",		"BITOR_I",	3,	ASSOC_LEFT, &type_integer,	&type_integer,	&type_integer},
{7, "*",		"MUL_I",		3,	ASSOC_LEFT, &type_integer,	&type_integer,	&type_integer},
{7, "/",		"DIV_I",		3,	ASSOC_LEFT, &type_integer,	&type_integer,	&type_integer},
{7, "==",		"EQ_I",		5,	ASSOC_LEFT, &type_integer,	&type_integer,	&type_integer},
{7, "!=",		"NE_I",		5,	ASSOC_LEFT, &type_integer,	&type_integer,	&type_integer},
{7, "<IFNOTS>",	"IFNOTS",		-1,	ASSOC_RIGHT,&type_string,	NULL,		&type_void},
{7, "<IFS>",	"IFS",		-1,	ASSOC_RIGHT,&type_string,	NULL,		&type_void},
{7, "!",		"NOT_I",		-1,	ASSOC_LEFT, &type_integer,	&type_void,	&type_integer},
{7, "/",		"DIV_VF",		3,	ASSOC_LEFT, &type_vector,	&type_float,	&type_float},
{7, "^",		"POWER_I",	3,	ASSOC_LEFT, &type_integer,	&type_integer,	&type_integer},
{7, ">>",		"RSHIFT_I",	3,	ASSOC_LEFT, &type_integer,	&type_integer,	&type_integer},
{7, "<<",		"LSHIFT_I",	3,	ASSOC_LEFT, &type_integer,	&type_integer,	&type_integer},
{7, "<ARRAY>",	"GET_POINTER",	-1,	ASSOC_LEFT, &type_float,	&type_integer,	&type_pointer},
{7, "<ARRAY>",	"ARRAY_OFS",	-1,	ASSOC_LEFT, &type_pointer,	&type_integer,	&type_pointer},
{7, "=",		"LOADA_F",	6,	ASSOC_LEFT, &type_float,	&type_integer,	&type_float},
{7, "=",		"LOADA_V",	6,	ASSOC_LEFT, &type_vector,	&type_integer,	&type_vector},
{7, "=",		"LOADA_S",	6,	ASSOC_LEFT, &type_string,	&type_integer,	&type_string},
{7, "=",		"LOADA_ENT",	6,	ASSOC_LEFT, &type_entity,	&type_integer,	&type_entity},
{7, "=",		"LOADA_FLD",	6,	ASSOC_LEFT, &type_field,	&type_integer,	&type_field},
{7, "=",		"LOADA_FNC",	6,	ASSOC_LEFT, &type_function,	&type_integer,	&type_function},
{7, "=",		"LOADA_I",	6,	ASSOC_LEFT, &type_integer,	&type_integer,	&type_integer},
{7, "=",		"STORE_P",	6,	ASSOC_RIGHT,&type_pointer,	&type_pointer,	&type_void},
{7, ".",		"INDIRECT_P",	1,	ASSOC_LEFT, &type_entity,	&type_field,	&type_pointer},
{7, "=",		"LOADP_F",	6,	ASSOC_LEFT, &type_pointer,	&type_integer,	&type_float},
{7, "=",		"LOADP_V",	6,	ASSOC_LEFT, &type_pointer,	&type_integer,	&type_vector},
{7, "=",		"LOADP_S",	6,	ASSOC_LEFT, &type_pointer,	&type_integer,	&type_string},
{7, "=",		"LOADP_ENT",	6,	ASSOC_LEFT, &type_pointer,	&type_integer,	&type_entity},
{7, "=",		"LOADP_FLD",	6,	ASSOC_LEFT, &type_pointer,	&type_integer,	&type_field},
{7, "=",		"LOADP_FNC",	6,	ASSOC_LEFT, &type_pointer,	&type_integer,	&type_function},
{7, "=",		"LOADP_I",	6,	ASSOC_LEFT, &type_pointer,	&type_integer,	&type_integer},
{7, "<=",		"LE_I",		5,	ASSOC_LEFT, &type_integer,	&type_integer,	&type_integer},
{7, ">=",		"GE_I",		5,	ASSOC_LEFT, &type_integer,	&type_integer,	&type_integer},
{7, "<",		"LT_I",		5,	ASSOC_LEFT, &type_integer,	&type_integer,	&type_integer},
{7, ">",		"GT_I",		5,	ASSOC_LEFT, &type_integer,	&type_integer,	&type_integer},
{7, "<=",		"LE_IF",		5,	ASSOC_LEFT, &type_integer,	&type_float,	&type_integer},
{7, ">=",		"GE_IF",		5,	ASSOC_LEFT, &type_integer,	&type_float,	&type_integer},
{7, "<",		"LT_IF",		5,	ASSOC_LEFT, &type_integer,	&type_float,	&type_integer},
{7, ">",		"GT_IF",		5,	ASSOC_LEFT, &type_integer,	&type_float,	&type_integer},
{7, "<=",		"LE_FI",		5,	ASSOC_LEFT, &type_float,	&type_integer,	&type_integer},
{7, ">=",		"GE_FI",		5,	ASSOC_LEFT, &type_float,	&type_integer,	&type_integer},
{7, "<",		"LT_FI",		5,	ASSOC_LEFT, &type_float,	&type_integer,	&type_integer},
{7, ">",		"GT_FI",		5,	ASSOC_LEFT, &type_float,	&type_integer,	&type_integer},
{7, "==",		"EQ_IF",		5,	ASSOC_LEFT, &type_integer,	&type_float,	&type_integer},
{7, "==",		"EQ_FI",		5,	ASSOC_LEFT, &type_float,	&type_integer,	&type_float},
{7, "+",		"ADD_SF",		4,	ASSOC_LEFT, &type_string,	&type_float,	&type_string},
{7, "-",		"SUB_S",		4,	ASSOC_LEFT, &type_string,	&type_string,	&type_float},
{7, "<STOREP_C>",	"STOREP_C",	1,	ASSOC_RIGHT,&type_string,	&type_float,	&type_float},
{7, "<LOADP_C>",	"LOADP_C",	1,	ASSOC_LEFT, &type_string,	&type_void,	&type_float},
{7, "*",		"MUL_IF",		5,	ASSOC_LEFT, &type_integer,	&type_float,	&type_integer},
{7, "*",		"MUL_FI",		5,	ASSOC_LEFT, &type_float,	&type_integer,	&type_float},
{7, "*",		"MUL_VI",		5,	ASSOC_LEFT, &type_vector,	&type_integer,	&type_vector},
{7, "*",		"MUL_IV",		5,	ASSOC_LEFT, &type_integer,	&type_vector,	&type_vector},
{7, "/",		"DIV_IF",		5,	ASSOC_LEFT, &type_integer,	&type_float,	&type_integer},
{7, "/",		"DIV_FI",		5,	ASSOC_LEFT, &type_float,	&type_integer,	&type_float},
{7, "&",		"BITAND_IF",	5,	ASSOC_LEFT, &type_integer,	&type_float,	&type_integer},
{7, "|",		"BITOR_IF",	5,	ASSOC_LEFT, &type_integer,	&type_float,	&type_integer},
{7, "&",		"BITAND_FI",	5,	ASSOC_LEFT, &type_float,	&type_integer,	&type_float},
{7, "|",		"BITOR_FI",	5,	ASSOC_LEFT, &type_float,	&type_integer,	&type_float},
{7, "&&",		"AND_I",		5,	ASSOC_LEFT, &type_integer,	&type_integer,	&type_integer},
{7, "||",		"OR_I",		5,	ASSOC_LEFT, &type_integer,	&type_integer,	&type_integer},
{7, "&&",		"AND_IF",		5,	ASSOC_LEFT, &type_integer,	&type_float,	&type_integer},
{7, "||",		"OR_IF",		5,	ASSOC_LEFT, &type_integer,	&type_float,	&type_integer},
{7, "&&",		"AND_FI",		5,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{7, "||",		"OR_FI",		5,	ASSOC_LEFT, &type_float,	&type_float,	&type_integer},
{7, "!=",		"NE_IF",		5,	ASSOC_LEFT, &type_integer,	&type_float,	&type_integer},
{7, "!=",		"NE_FI",		5,	ASSOC_LEFT, &type_integer,	&type_float,	&type_integer},
{7, "<>",		"GSTOREP_I",	-1,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{7, "<>",		"GSTOREP_F",	-1,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{7, "<>",		"GSTOREP_ENT",	-1,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{7, "<>",		"GSTOREP_FLD",	-1,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{7, "<>",		"GSTOREP_S",	-1,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{7, "<>",		"GSTORE_PFNC",	-1,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{7, "<>",		"GSTOREP_V",	-1,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{7, "<>",		"GADDRESS",	-1,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{7, "<>",		"GLOAD_I",	-1,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{7, "<>",		"GLOAD_F",	-1,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{7, "<>",		"GLOAD_FLD",	-1,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{7, "<>",		"GLOAD_ENT",	-1,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{7, "<>",		"GLOAD_S",	-1,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{7, "<>",		"GLOAD_FNC",	-1,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{7, "<>",		"BOUNDCHECK",	-1,	ASSOC_LEFT, &type_float,	&type_float,	&type_float},
{7, "=",		"STOREP_P",	6,	ASSOC_RIGHT,&type_pointer,	&type_pointer,	&type_void},
{7, "<PUSH>",	"PUSH",		-1,	ASSOC_RIGHT,&type_float,	&type_void,	&type_pointer},
{7, "<POP>",	"POP",		-1,	ASSOC_RIGHT,&type_float,	&type_void,	&type_void},
{7, "|=",		"BITSET_I",	6,	ASSOC_RIGHT,&type_integer,	&type_integer,	&type_integer},
{7, "|=",		"BITSETP_I",	6,	ASSOC_RIGHT,&type_pointer,	&type_integer,	&type_integer},
{7, "*=",		"MULSTORE_I",	6,	ASSOC_RIGHT,&type_integer,	&type_integer,	&type_integer},
{7, "*=",		"MULSTOREP_I",	6,	ASSOC_RIGHT,&type_pointer,	&type_integer,	&type_vector},
{7, "/=",		"DIVSTORE_I",	6,	ASSOC_RIGHT,&type_integer,	&type_integer,	&type_integer},
{7, "/=",		"DIVSTOREP_I",	6,	ASSOC_RIGHT,&type_pointer,	&type_integer,	&type_vector},
{7, "+=",		"ADDSTORE_I",	6,	ASSOC_RIGHT,&type_integer,	&type_integer,	&type_integer},
{7, "+=",		"ADDSTOREP_I",	6,	ASSOC_RIGHT,&type_pointer,	&type_integer,	&type_integer},
{7, "-=",		"SUBSTORE_I",	6,	ASSOC_RIGHT,&type_integer,	&type_integer,	&type_integer},
{7, "-=",		"SUBSTOREP_I",	6,	ASSOC_RIGHT,&type_pointer,	&type_vector,	&type_vector},
// VERSION 8
{0, NULL,		NULL,		-1,	ASSOC_LEFT, NULL,		NULL,		NULL}
};

// this system cuts out 10/120
// these evaluate as top first.
opcode_t *opcodeprioritized[TOP_PRIORITY+1][64] =
{
	{	// don't use
		NULL
	},
	{
		// 1
		&pr_opcodes[OP_LOAD_F],
		&pr_opcodes[OP_LOAD_V],
		&pr_opcodes[OP_LOAD_S],
		&pr_opcodes[OP_LOAD_ENT],
		&pr_opcodes[OP_LOAD_FLD],
		&pr_opcodes[OP_LOAD_FNC],
		&pr_opcodes[OP_LOAD_I],
		&pr_opcodes[OP_LOAD_P],
		&pr_opcodes[OP_ADDRESS],
		NULL
	},
	{
		// 2
		NULL
	},
	{
		// 3
		&pr_opcodes[OP_MUL_F],
		&pr_opcodes[OP_MUL_V],
		&pr_opcodes[OP_MUL_FV],
		&pr_opcodes[OP_MUL_VF],
		&pr_opcodes[OP_MUL_I],
		&pr_opcodes[OP_DIV_F],
		&pr_opcodes[OP_DIV_I],
		&pr_opcodes[OP_DIV_VF],
		&pr_opcodes[OP_BITAND],
		&pr_opcodes[OP_BITAND_I],
		&pr_opcodes[OP_BITOR],
		&pr_opcodes[OP_BITOR_I],
		&pr_opcodes[OP_POWER_I],
		&pr_opcodes[OP_RSHIFT_I],
		&pr_opcodes[OP_LSHIFT_I],
		NULL
	},
	{
		// 4
		&pr_opcodes[OP_ADD_F],
		&pr_opcodes[OP_ADD_V],
		&pr_opcodes[OP_ADD_I],
		&pr_opcodes[OP_ADD_FI],
		&pr_opcodes[OP_ADD_IF],
		&pr_opcodes[OP_ADD_SF],
		&pr_opcodes[OP_SUB_F],
		&pr_opcodes[OP_SUB_V],
		&pr_opcodes[OP_SUB_I],
		&pr_opcodes[OP_SUB_FI],
		&pr_opcodes[OP_SUB_IF],
		&pr_opcodes[OP_SUB_S],
		NULL
	},
	{
		// 5
		&pr_opcodes[OP_EQ_F],
		&pr_opcodes[OP_EQ_V],
		&pr_opcodes[OP_EQ_S],
		&pr_opcodes[OP_EQ_E],
		&pr_opcodes[OP_EQ_FNC],
		&pr_opcodes[OP_EQ_I],
		&pr_opcodes[OP_EQ_IF],
		&pr_opcodes[OP_EQ_FI],
		&pr_opcodes[OP_NE_F],
		&pr_opcodes[OP_NE_V],
		&pr_opcodes[OP_NE_S],
		&pr_opcodes[OP_NE_E],
		&pr_opcodes[OP_NE_FNC],
		&pr_opcodes[OP_NE_I],
		&pr_opcodes[OP_LE],
		&pr_opcodes[OP_LE_I],
		&pr_opcodes[OP_LE_IF],
		&pr_opcodes[OP_LE_FI],
		&pr_opcodes[OP_GE],
		&pr_opcodes[OP_GE_I],
		&pr_opcodes[OP_GE_IF],
		&pr_opcodes[OP_GE_FI],
		&pr_opcodes[OP_LT],
		&pr_opcodes[OP_LT_I],
		&pr_opcodes[OP_LT_IF],
		&pr_opcodes[OP_LT_FI],
		&pr_opcodes[OP_GT],
		&pr_opcodes[OP_GT_I],
		&pr_opcodes[OP_GT_IF],
		&pr_opcodes[OP_GT_FI],
		NULL
	},
	{
		// 6
		&pr_opcodes[OP_STORE_F],
		&pr_opcodes[OP_STORE_V],
		&pr_opcodes[OP_STORE_S],
		&pr_opcodes[OP_STORE_ENT],
		&pr_opcodes[OP_STORE_FLD],
		&pr_opcodes[OP_STORE_FNC],
		&pr_opcodes[OP_STORE_I],
		&pr_opcodes[OP_STORE_IF],
		&pr_opcodes[OP_STORE_FI],
		&pr_opcodes[OP_STORE_P],
		&pr_opcodes[OP_STOREP_F],
		&pr_opcodes[OP_STOREP_V],
		&pr_opcodes[OP_STOREP_S],
		&pr_opcodes[OP_STOREP_ENT],
		&pr_opcodes[OP_STOREP_FLD],
		&pr_opcodes[OP_STOREP_FNC],
		&pr_opcodes[OP_STOREP_I],
		&pr_opcodes[OP_STOREP_IF],
		&pr_opcodes[OP_STOREP_FI],
		&pr_opcodes[OP_STOREP_P],
		&pr_opcodes[OP_DIVSTORE_F],
		&pr_opcodes[OP_DIVSTOREP_F],
		&pr_opcodes[OP_MULSTORE_F],
		&pr_opcodes[OP_MULSTORE_V],
		&pr_opcodes[OP_MULSTOREP_F],
		&pr_opcodes[OP_MULSTOREP_V],
		&pr_opcodes[OP_ADDSTORE_F],
		&pr_opcodes[OP_ADDSTORE_V],
		&pr_opcodes[OP_ADDSTOREP_F],
		&pr_opcodes[OP_ADDSTOREP_V],
		&pr_opcodes[OP_SUBSTORE_F],
		&pr_opcodes[OP_SUBSTORE_V],
		&pr_opcodes[OP_SUBSTOREP_F],
		&pr_opcodes[OP_SUBSTOREP_V],
		&pr_opcodes[OP_BITSET],
		&pr_opcodes[OP_BITSETP],
		&pr_opcodes[OP_BITCLR],
		&pr_opcodes[OP_BITCLRP],
		NULL
	},
	{
		// 7
		&pr_opcodes[OP_AND],
		&pr_opcodes[OP_OR],
		NULL
	}
};

bool PR_OPCodeValid(opcode_t *op)
{
	int num = op - pr_opcodes;

	switch(targetformat)
	{
	case QCF_STANDARD:
		if (num < OP_MULSTORE_F)
			return true;
		return false;
	case QCF_RELEASE:
	case QCF_DEBUG:
		return true;
	default:
		return false;
	}
}

//===========================================================================
/*
============
PR_Statement

Emits a primitive statement, returning the var it places it's value in
============
*/

int __inline PR_ShouldConvert(def_t *var, etype_t wanted)
{
	if (var->type->type == ev_integer && wanted == ev_function)
		return 0;
	if (var->type->type == ev_pointer && var->type->aux_type)
	{
		if (var->type->aux_type->type == ev_float && wanted == ev_integer)
			return OP_CP_FTOI;		

		if (var->type->aux_type->type == ev_integer && wanted == ev_float)
			return OP_CP_ITOF;
	}
	else
	{
		if (var->type->type == ev_float && wanted == ev_integer)
			return OP_CONV_FTOI;

		if (var->type->type == ev_integer && wanted == ev_float)
			return OP_CONV_ITOF;
	}
	return -1;
}

def_t *PR_SupplyConversion(def_t *var, etype_t wanted)
{
	int o;

	if (pr_classtype && var->type->type == ev_field && wanted != ev_field)
	{
		if (pr_classtype)
		{	
			// load pevname.var into a temp
			def_t *pev;
			pev = PR_GetDef(type_entity, pevname, NULL, true, 1);
			switch(wanted)
			{
			case ev_float:
				return PR_Statement(pr_opcodes+OP_LOAD_F, pev, var, NULL);
			case ev_string:
				return PR_Statement(pr_opcodes+OP_LOAD_S, pev, var, NULL);
			case ev_function:
				return PR_Statement(pr_opcodes+OP_LOAD_FNC, pev, var, NULL);
			case ev_vector:
				return PR_Statement(pr_opcodes+OP_LOAD_V, pev, var, NULL);
			case ev_entity:
				return PR_Statement(pr_opcodes+OP_LOAD_ENT, pev, var, NULL);
			default:
				Sys_Error("Inexplicit field load failed, try explicit");
			}
		}
	}

	o = PR_ShouldConvert(var, wanted);

	if (o <= 0) return var;// no conversion
	return PR_Statement(&pr_opcodes[o], var, NULL, NULL);	//conversion return value
}

// assistant functions. This can safly be bipassed with the old method for more complex things.
gofs_t PR_GetFreeOffsetSpace(uint size)
{
	int ofs;
	if (opt_locals_marshalling)
	{
		freeoffset_t *fofs, *prev;
		for (fofs = freeofs, prev = NULL; fofs; fofs=fofs->next)
		{
			if (fofs->size == size)
			{
				if (prev) prev->next = fofs->next;
				else freeofs = fofs->next;
				return fofs->ofs;
			}
			prev = fofs;
		}
		for (fofs = freeofs, prev = NULL; fofs; fofs=fofs->next)
		{
			if (fofs->size > size)
			{
				fofs->size -= size;
				fofs->ofs += size;
				return fofs->ofs-size;
			}
			prev = fofs;
		}
	}
	
	ofs = numpr_globals;
	numpr_globals+=size;

	if (numpr_globals >= MAX_REGS)
	{
		if (!opt_overlaptemps || !opt_locals_marshalling)
			Sys_Error("numpr_globals exceeded MAX_REGS - you'll need to use more optimisations");
		else Sys_Error("numpr_globals exceeded MAX_REGS");
	}

	return ofs;
}

void PR_FreeOffset(gofs_t ofs, uint size)
{
	freeoffset_t *fofs;
	if (ofs + size == numpr_globals)
	{	
		// FIXME: is this a bug?
		numpr_globals -= size;
		return;
	}

	for (fofs = freeofs; fofs; fofs = fofs->next)
	{
		// FIXME: if this means the last block becomes free, free them all.
		if (fofs->ofs == ofs + size)
		{
			fofs->ofs -= size;
			fofs->size += size;
			return;
		}
		if (fofs->ofs+fofs->size == ofs)
		{
			fofs->size += size;
			return;
		}
	}

	fofs = Qalloc(sizeof(freeoffset_t));
	fofs->next = freeofs;
	fofs->ofs = ofs;
	fofs->size = size;

	freeofs = fofs;
	return;
}

static def_t *PR_GetTemp(type_t *type)
{
	def_t	*var_c;
	temp_t	*t;

	var_c = (void *)Qalloc(sizeof(def_t));
	var_c->type = type;
	var_c->name = "temp";

	// don't exceed. This lets us allocate a huge block, and still be able to compile smegging big funcs.
	if (opt_overlaptemps)
	{
		for (t = functemps; t; t = t->next)
		{
			if (!t->used && t->size == type->size)
				break;
		}
		if (t && t->scope && t->scope != pr_scope)
			Sys_Error("Internal error temp has scope not equal to current scope");

		if (!t)
		{
			// allocate a new one
			t = Qalloc(sizeof(temp_t));
			t->size = type->size;
			t->next = functemps;
			functemps = t;
			
			t->ofs = PR_GetFreeOffsetSpace(t->size);
			numtemps += t->size;
		}

		// use a previous one.
		var_c->ofs = t->ofs;
		var_c->temp = t;
		t->lastfunc = pr_scope;
	}
	else if (opt_locals_marshalling)
	{
		// allocate a new one
		t = Qalloc(sizeof(temp_t));
		t->size = type->size;

		t->next = functemps;
		functemps = t;

		t->ofs = PR_GetFreeOffsetSpace(t->size);

		numtemps += t->size;

		var_c->ofs = t->ofs;
		var_c->temp = t;
		t->lastfunc = pr_scope;
	}
	else
	{
		// we're not going to reallocate any temps so allocate permanently
		var_c->ofs = PR_GetFreeOffsetSpace(type->size);
		numtemps+=type->size;
	}

	var_c->s_file = s_file;
	var_c->s_line = pr_source_line;

	if (var_c->temp) var_c->temp->used = true;

	return var_c;
}

// nothing else references this temp.
static void PR_FreeTemp(def_t *t)
{
	if (t && t->temp) t->temp->used = false;
}

static void PR_UnFreeTemp(def_t *t)
{
	if (t->temp) t->temp->used = true;
}

// We've just parsed a statement.
// We can gaurentee that any used temps are now not used.
#ifdef _DEBUG
static void PR_FreeTemps( void )
{
	temp_t *t;

	t = functemps;
	while(t)
	{
		if (t->used && !pr_error_count) // don't print this after an error jump out.
		{
			PR_ParseWarning(WARN_DEBUGGING, "Temp was used in %s", pr_scope->name);
			t->used = false;
		}
		t = t->next;
	}
}
#else
#define PR_FreeTemps()
#endif

// temps that are still in use over a function call can be considered dodgy.
// we need to remap these to locally defined temps, on return from the function so we know we got them all.
static void PR_LockActiveTemps(void)
{
	temp_t *t;

	t = functemps;
	while(t)
	{
		if (t->used) t->scope = pr_scope;
		t = t->next;
	}
	
}

static void PR_RemapLockedTemp(temp_t *t, int firststatement, int laststatement)
{
	char		buffer[128];

	def_t		*def;
	int		newofs = 0;
	dstatement_t	*st;
	int		i;

	for (i = firststatement, st = &statements[i]; i < laststatement; i++, st++)
	{
		if (pr_opcodes[st->op].type_a && st->a == t->ofs)
		{
			if (!newofs)
			{
				newofs = PR_GetFreeOffsetSpace(t->size);
				numtemps+=t->size;

				def = PR_DummyDef(type_float, NULL, pr_scope, t->size, newofs, false);
				def->nextlocal = pr.localvars;
				def->constant = false;
				sprintf(buffer, "locked_%i", t->ofs);
				def->name = Qalloc(strlen(buffer)+1);
				strcpy(def->name, buffer);
				pr.localvars = def;
			}
			st->a = newofs;
		}
		if (pr_opcodes[st->op].type_b && st->b == t->ofs)
		{
			if (!newofs)
			{
				newofs = PR_GetFreeOffsetSpace(t->size);
				numtemps+=t->size;

				def = PR_DummyDef(type_float, NULL, pr_scope, t->size, newofs, false);
				def->nextlocal = pr.localvars;
				def->constant = false;
				sprintf(buffer, "locked_%i", t->ofs);
				def->name = Qalloc(strlen(buffer)+1);
				strcpy(def->name, buffer);
				pr.localvars = def;
			}
			st->b = newofs;
		}
		if (pr_opcodes[st->op].type_c && st->c == t->ofs)
		{
			if (!newofs)
			{
				newofs = PR_GetFreeOffsetSpace(t->size);
				numtemps+=t->size;

				def = PR_DummyDef(type_float, NULL, pr_scope, t->size, newofs, false);
				def->nextlocal = pr.localvars;
				def->constant = false;
				sprintf(buffer, "locked_%i", t->ofs);
				def->name = Qalloc(strlen(buffer)+1);
				strcpy(def->name, buffer);
				pr.localvars = def;
			}
			st->c = newofs;
		}
	}
}

static void PR_RemapLockedTemps(int firststatement, int laststatement)
{
	temp_t *t;

	t = functemps;
	while(t)
	{
		if (t->scope || opt_locals_marshalling)
		{
			PR_RemapLockedTemp(t, firststatement, laststatement);
			t->scope = NULL;
			t->lastfunc = NULL;
		}
		t = t->next;
	}
}

static void PR_fprintfLocals(file_t *f, gofs_t paramstart, gofs_t paramend)
{
	def_t	*var;
	temp_t	*t;
	int	i;

	for (var = pr.localvars; var; var = var->nextlocal)
	{
		if (var->ofs >= paramstart && var->ofs < paramend)
			continue;
		FS_Printf(f, "local %s %s;\n", TypeName(var->type), var->name);
	}

	for (t = functemps, i = 0; t; t = t->next, i++)
	{
		if (t->lastfunc == pr_scope)
		{
			FS_Printf(f, "local %s temp_%i;\n", (t->size == 1)?"float":"vector", i);
		}
	}
}

static const char *PR_VarAtOffset(uint ofs, uint size)
{
	static char	message[1024];
	def_t		*var;
	temp_t		*t;
	int		i;

	for (t = functemps, i = 0; t; t = t->next, i++)
	{
		if (ofs >= t->ofs && ofs < t->ofs + t->size)
		{
			if (size < t->size) sprintf(message, "temp_%i_%c", i, 'x' + (ofs-t->ofs)%3);
			else sprintf(message, "temp_%i", i);
			return message;
		}
	}

	for (var = pr.localvars; var; var = var->nextlocal)
	{
		if (var->scope && var->scope != pr_scope)
			continue;	// this should be an error
		if (ofs >= var->ofs && ofs < var->ofs + var->type->size)
		{
			if (*var->name)
			{
				// continue, don't get bogged down by multiple bits of code
				if (!STRCMP(var->name, "IMMEDIATE")) continue;
				if (size < var->type->size) sprintf(message, "%s_%c", var->name, 'x' + (ofs-var->ofs)%3);
				else sprintf(message, "%s", var->name);
				return message;
			}
		}
	}

	for (var = pr.def_head.next; var; var = var->next)
	{
		if (var->scope && var->scope != pr_scope)
			continue;

		if (ofs >= var->ofs && ofs < var->ofs + var->type->size)
		{
			if (*var->name)
			{
				if (!STRCMP(var->name, "IMMEDIATE"))
				{
					switch(var->type->type)
					{
					case ev_string:
						sprintf(message, "\"%.1020s\"", &strings[((int *)pr_globals)[var->ofs]]);
						return message;
					case ev_integer:
						sprintf(message, "%i", ((int *)pr_globals)[var->ofs]);
						return message;
					case ev_float:
						sprintf(message, "%f", pr_globals[var->ofs]);
						return message;
					case ev_vector:
						sprintf(message, "'%f %f %f'", pr_globals[var->ofs], pr_globals[var->ofs+1], pr_globals[var->ofs+2]);
						return message;
					default:
						sprintf(message, "IMMEDIATE");
						return message;
					}
				}
				if (size < var->type->size) sprintf(message, "%s_%c", var->name, 'x' + (ofs-var->ofs)%3);
				else sprintf(message, "%s", var->name);
				return message;
			}
		}
	}

	if (size >= 3)
	{
		if (ofs >= OFS_RETURN && ofs < OFS_PARM0) sprintf(message, "return");
		else if (ofs >= OFS_PARM0 && ofs < RESERVED_OFS) sprintf(message, "parm%i", (ofs-OFS_PARM0)/3);
		else sprintf(message, "offset_%i", ofs);
	}
	else
	{
		if (ofs >= OFS_RETURN && ofs < OFS_PARM0) sprintf(message, "return_%c", 'x' + ofs-OFS_RETURN);
		else if (ofs >= OFS_PARM0 && ofs < RESERVED_OFS) sprintf(message, "parm%i_%c", (ofs-OFS_PARM0)/3, 'x' + (ofs-OFS_PARM0)%3);
		else sprintf(message, "offset_%i", ofs);
	}
	return message;
}

def_t *PR_Statement ( opcode_t *op, def_t *var_a, def_t *var_b, dstatement_t **outstatement)
{
	dstatement_t	*statement;
	def_t		*var_c = NULL, *temp = NULL;

	if (outstatement == (dstatement_t **)0xffffffff) outstatement = NULL;
	else if (op->priority != -1)
	{
		if (op->associative != ASSOC_LEFT)
		{
			if (op->type_a == &type_pointer) var_b = PR_SupplyConversion(var_b, (*op->type_b)->type);
			else var_b = PR_SupplyConversion(var_b, (*op->type_a)->type);
		}
		else
		{
			if (var_a) var_a = PR_SupplyConversion(var_a, (*op->type_a)->type);
			if (var_b) var_b = PR_SupplyConversion(var_b, (*op->type_b)->type);
		}
	}

	if (var_a)
	{
		var_a->references++;
		PR_FreeTemp(var_a);
	}
	if (var_b)
	{
		var_b->references++;
		PR_FreeTemp(var_b);
	}

	if (keyword_class && var_a && var_b)
	{
		if (var_a->type->type == ev_entity && var_b->type->type == ev_entity)
		{
			if (var_a->type != var_b->type)
			{
				if (strcmp(var_a->type->name, var_b->type->name))
					PR_ParseWarning(0, "Inexplict cast");
			}
		}
	}

	// maths operators
	if (opt_constantarithmatic && (var_a && var_a->constant) && (var_b && var_b->constant))
	{
		switch (op - pr_opcodes)// improve some of the maths.
		{
		case OP_BITOR:
			return PR_MakeFloatDef((float)((int)G_FLOAT(var_a->ofs) | (int)G_FLOAT(var_b->ofs)));
		case OP_BITAND:
			return PR_MakeFloatDef((float)((int)G_FLOAT(var_a->ofs) & (int)G_FLOAT(var_b->ofs)));
		case OP_MUL_F:
			return PR_MakeFloatDef(G_FLOAT(var_a->ofs) * G_FLOAT(var_b->ofs));
		case OP_DIV_F:
			return PR_MakeFloatDef(G_FLOAT(var_a->ofs) / G_FLOAT(var_b->ofs));
		case OP_ADD_F:
			return PR_MakeFloatDef(G_FLOAT(var_a->ofs) + G_FLOAT(var_b->ofs));
		case OP_SUB_F:
			return PR_MakeFloatDef(G_FLOAT(var_a->ofs) - G_FLOAT(var_b->ofs));
		case OP_BITOR_I:
			return PR_MakeIntDef(G_INT(var_a->ofs) | G_INT(var_b->ofs));
		case OP_BITAND_I:
			return PR_MakeIntDef(G_INT(var_a->ofs) & G_INT(var_b->ofs));
		case OP_MUL_I:
			return PR_MakeIntDef(G_INT(var_a->ofs) * G_INT(var_b->ofs));
		case OP_DIV_I:
			return PR_MakeIntDef(G_INT(var_a->ofs) / G_INT(var_b->ofs));
		case OP_ADD_I:
			return PR_MakeIntDef(G_INT(var_a->ofs) + G_INT(var_b->ofs));
		case OP_SUB_I:
			return PR_MakeIntDef(G_INT(var_a->ofs) - G_INT(var_b->ofs));
		}
	}

	switch (op - pr_opcodes)
	{
	case OP_AND:
		if (var_a->ofs == var_b->ofs)
			PR_ParseWarning(WARN_CONSTANTCOMPARISON, "Parameter offsets for && are the same");
		if (var_a->constant || var_b->constant)
			PR_ParseWarning(WARN_CONSTANTCOMPARISON, "Result of comparison is constant");
		break;
	case OP_OR:
		if (var_a->ofs == var_b->ofs)
			PR_ParseWarning(WARN_CONSTANTCOMPARISON, "Parameters for || are the same");
		if (var_a->constant || var_b->constant)
			PR_ParseWarning(WARN_CONSTANTCOMPARISON, "Result of comparison is constant");
		break;
	case OP_EQ_F:
	case OP_EQ_S:
	case OP_EQ_E:
	case OP_EQ_FNC:
	case OP_EQ_V:

	case OP_NE_F:
	case OP_NE_V:
	case OP_NE_S:
	case OP_NE_E:
	case OP_NE_FNC:

	case OP_LE:
	case OP_GE:
	case OP_LT:
	case OP_GT:
		if ((var_a->constant && var_b->constant && !var_a->temp && !var_b->temp) || var_a->ofs == var_b->ofs)
			PR_ParseWarning(WARN_CONSTANTCOMPARISON, "Result of comparison is constant");
		break;
	case OP_IFS:
	case OP_IFNOTS:
	case OP_IF:
	case OP_IFNOT:
		if (var_a->constant && !var_a->temp)
			PR_ParseWarning(WARN_CONSTANTCOMPARISON, "Result of comparison is constant");
		break;
	default:
		break;
	}

	if (numstatements)
	{	
		// optimise based on last statement.
		if (op - pr_opcodes == OP_IFNOT)
		{
			if (opt_shortenifnots && var_a && (statements[numstatements-1].op == OP_NOT_F || statements[numstatements-1].op == OP_NOT_FNC || statements[numstatements-1].op == OP_NOT_ENT))
			{
				if (statements[numstatements-1].c == var_a->ofs)
				{
					static def_t nvara;
					op = &pr_opcodes[OP_IF];
					numstatements--;
					PR_FreeTemp(var_a);
					Mem_Copy(&nvara, var_a, sizeof(nvara));
					nvara.ofs = statements[numstatements].a;
					var_a = &nvara;
				}
			}
		}
		else if (op - pr_opcodes == OP_IFNOTS)
		{
			if (opt_shortenifnots && var_a && statements[numstatements-1].op == OP_NOT_S)
			{
				if (statements[numstatements-1].c == var_a->ofs)
				{
					static def_t nvara;
					op = &pr_opcodes[OP_IFS];
					numstatements--;
					PR_FreeTemp(var_a);
					Mem_Copy(&nvara, var_a, sizeof(nvara));
					nvara.ofs = statements[numstatements].a;
					var_a = &nvara;
				}
			}
		}
		else if (((uint) ((op - pr_opcodes) - OP_STORE_F) < 6))
		{
			if (opt_assignments && var_a && var_a->ofs == statements[numstatements-1].c)
			{
				if (var_a->type->type == var_b->type->type)
				{
					if (var_a->temp)
					{
						statement = &statements[numstatements-1];
						statement->c = var_b->ofs;

						if (var_a->type->type != var_b->type->type)
							PR_ParseWarning(0, "store type mismatch");
						var_b->references++;
						var_a->references--;
						PR_FreeTemp(var_a);
						simplestore = true;

						PR_UnFreeTemp(var_b);
						return var_b;
					}
				}
			}
		}
	}

	simplestore = false;
	statement = &statements[numstatements];
	numstatements++;

	if (!PR_OPCodeValid(op))
	{
		switch(op - pr_opcodes)
		{
		case OP_IFS:
			var_c = PR_GetDef(type_string, "string_null", NULL, true, 1);
			numstatements--;
			var_a = PR_Statement(&pr_opcodes[OP_NE_S], var_a, var_c, NULL);
			statement = &statements[numstatements];
			numstatements++;

			PR_FreeTemp(var_a);
			op = &pr_opcodes[OP_IF];
			break;

		case OP_IFNOTS:
			var_c = PR_GetDef(type_string, "string_null", NULL, true, 1);
			numstatements--;
			var_a = PR_Statement(&pr_opcodes[OP_NE_S], var_a, var_c, NULL);
			statement = &statements[numstatements];
			numstatements++;

			PR_FreeTemp(var_a);
			op = &pr_opcodes[OP_IFNOT];
			break;

		case OP_ADDSTORE_F:
			op = &pr_opcodes[OP_ADD_F];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;

		case OP_SUBSTORE_F:
			op = &pr_opcodes[OP_SUB_F];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;

		case OP_DIVSTORE_F:
			op = &pr_opcodes[OP_DIV_F];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;

		case OP_MULSTORE_F:
			op = &pr_opcodes[OP_MUL_F];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;

		case OP_ADDSTORE_V:
			op = &pr_opcodes[OP_ADD_V];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;

		case OP_SUBSTORE_V:
			op = &pr_opcodes[OP_SUB_V];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;

		case OP_MULSTORE_V:
			op = &pr_opcodes[OP_MUL_V];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;

		case OP_BITSET:
			op = &pr_opcodes[OP_BITOR];
			var_c = var_b;
			var_b = var_a;
			var_a = var_c;
			var_c = var_a;
			break;

		case OP_BITCLR:
			// b = var, a = bit field.

			PR_UnFreeTemp(var_a);
			PR_UnFreeTemp(var_b);

			numstatements--;
			var_c = PR_Statement(&pr_opcodes[OP_BITAND], var_b, var_a, NULL);
			PR_FreeTemp(var_c);
			statement = &statements[numstatements];
			numstatements++;
			
			PR_FreeTemp(var_a);
			PR_FreeTemp(var_b);

			op = &pr_opcodes[OP_SUB_F];
			var_a = var_b;
			var_b = var_c;
			var_c = var_a;
			break;

		case OP_SUBSTOREP_F:
		case OP_ADDSTOREP_F:
		case OP_MULSTOREP_F:
		case OP_DIVSTOREP_F:
		case OP_BITSETP:
		case OP_BITCLRP:
			PR_UnFreeTemp(var_a);
			PR_UnFreeTemp(var_b);
			// don't chain these... this expansion is not the same.
			{
				int st;

				for (st = numstatements - 2; st >= 0; st--)
				{
					if (statements[st].op == OP_ADDRESS)
						if (statements[st].c == var_b->ofs)
							break;

					if (statements[st].c == var_b->ofs)
						PR_ParseWarning(0, "Temp - reuse may have broken your %s\n", pr_opcodes);
				}
				if (st < 0) PR_ParseError(ERR_INTERNAL, "XSTOREP_F couldn't find pointer generation");
				var_c = PR_GetTemp(*op->type_c);

				statement_linenums[statement-statements] = pr_source_line;
				statement->op = OP_LOAD_F;
				statement->a = statements[st].a;
				statement->b = statements[st].b;
				statement->c = var_c->ofs;
			}

			statement = &statements[numstatements];
			numstatements++;

			statement_linenums[statement-statements] = pr_source_line;
			switch(op - pr_opcodes)
			{
			case OP_SUBSTOREP_F:
				statement->op = OP_SUB_F;
				break;
			case OP_ADDSTOREP_F:
				statement->op = OP_ADD_F;
				break;
			case OP_MULSTOREP_F:
				statement->op = OP_MUL_F;
				break;
			case OP_DIVSTOREP_F:
				statement->op = OP_DIV_F;
				break;
			case OP_BITSETP:
				statement->op = OP_BITOR;
				break;
			case OP_BITCLRP:
				// float pointer float
				temp = PR_GetTemp(type_float);
				statement->op = OP_BITAND;
				statement->a = var_c ? var_c->ofs : 0;
				statement->b = var_a ? var_a->ofs : 0;
				statement->c = temp->ofs;

				statement = &statements[numstatements];
				numstatements++;

				statement_linenums[statement-statements] = pr_source_line;
				statement->op = OP_SUB_F;

				// t = c & i
				// c = c - t
				break;
			default:	// no way will this be hit...
				PR_ParseError(ERR_INTERNAL, "opcode invalid 3 times %i", op - pr_opcodes);
			}
			if (op - pr_opcodes == OP_BITCLRP)
			{
				statement->a = var_c ? var_c->ofs : 0;
				statement->b = temp ? temp->ofs : 0;
				statement->c = var_c->ofs;
				PR_FreeTemp(temp);
				var_b = var_b; // this is the ptr.
				PR_FreeTemp(var_a);
				var_a = var_c; // this is the value.
			}
			else
			{
				statement->a = var_c ? var_c->ofs : 0;
				statement->b = var_a ? var_a->ofs : 0;
				statement->c = var_c->ofs;
				var_b = var_b; // this is the ptr.
				PR_FreeTemp(var_a);
				var_a = var_c; // this is the value.
			}

			op = &pr_opcodes[OP_STOREP_F];
			PR_FreeTemp(var_c);
			var_c = NULL;
			PR_FreeTemp(var_b);

			statement = &statements[numstatements];
			numstatements++;
			break;

		case OP_MULSTOREP_V:
		case OP_SUBSTOREP_V:
		case OP_ADDSTOREP_V:
			PR_UnFreeTemp(var_a);
			PR_UnFreeTemp(var_b);
			// don't chain these... this expansion is not the same.
			{
				int st;
				for (st = numstatements-2; st>=0; st--)
				{
					if (statements[st].op == OP_ADDRESS)
						if (statements[st].c == var_b->ofs)
							break;
				}
				if (st < 0) PR_ParseError(ERR_INTERNAL, "XSTOREP_V couldn't find pointer generation");
				var_c = PR_GetTemp(*op->type_c);

				statement_linenums[statement-statements] = pr_source_line;
				statement->op = OP_LOAD_V;
				statement->a = statements[st].a;
				statement->b = statements[st].b;
				statement->c = var_c ? var_c->ofs : 0;
			}

			statement = &statements[numstatements];
			numstatements++;

			statement_linenums[statement-statements] = pr_source_line;
			switch(op - pr_opcodes)
			{
			case OP_SUBSTOREP_V:
				statement->op = OP_SUB_V;
				break;
			case OP_ADDSTOREP_V:
				statement->op = OP_ADD_V;
				break;
			case OP_MULSTOREP_V:
				statement->op = OP_MUL_V;
				break;
			default:	// no way will this be hit...
				PR_ParseError(ERR_INTERNAL, "opcode invalid 3 times %i", op - pr_opcodes);
			}
			statement->a = var_a ? var_a->ofs : 0;
			statement->b = var_c ? var_c->ofs : 0;
			PR_FreeTemp(var_c);
			var_c = PR_GetTemp(*op->type_c);
			statement->c = var_c ? var_c->ofs : 0;

			var_b = var_b; // this is the ptr.
			PR_FreeTemp(var_a);
			var_a = var_c; // this is the value.
			op = &pr_opcodes[OP_STOREP_V];
			
			PR_FreeTemp(var_c);
			var_c = NULL;
			PR_FreeTemp(var_b);

			statement = &statements[numstatements];
			numstatements++;
			break;
		default:
			PR_ParseError(ERR_BADEXTENSION, "Opcode \"%s|%s\" not valid for target", op->name, op->opname);
			break;
		}
	}
	if (outstatement) *outstatement = statement;
	
	statement_linenums[statement-statements] = pr_source_line;
	statement->op = op - pr_opcodes;
	statement->a = var_a ? var_a->ofs : 0;
	statement->b = var_b ? var_b->ofs : 0;
	if (var_c != NULL)
	{
		statement->c = var_c->ofs;
	}
	else if (op->type_c == &type_void || op->associative == ASSOC_RIGHT || op->type_c == NULL)
	{
		var_c = NULL;
		statement->c = 0;	// ifs, gotos, and assignments don't need vars allocated
	}
	else
	{	// allocate result space
		var_c = PR_GetTemp(*op->type_c);
		statement->c = var_c->ofs;
		if (op->type_b == &type_field)
		{
			var_c->name = var_b->name;
			var_c->s_file = var_b->s_file;
			var_c->s_line = var_b->s_line;
		}
	}

	if ((op - pr_opcodes >= OP_LOAD_F && op - pr_opcodes <= OP_LOAD_FNC) || op - pr_opcodes == OP_LOAD_I)
	{
		if (var_b->constant == 2) var_c->constant = true;
	}

	if (!var_c)
	{
		if (var_a) PR_UnFreeTemp(var_a);
		return var_a;
	}
	return var_c;
}

/*
============
PR_SimpleStatement

Emits a primitive statement, returning the var it places it's value in
============
*/
dstatement_t *PR_SimpleStatement( int op, int var_a, int var_b, int var_c, int force)
{
	dstatement_t	*statement;

	if (!force && !PR_OPCodeValid(pr_opcodes + op))
	{
		PR_ParseError(ERR_BADEXTENSION, "Opcode \"%s|%s\" not valid for target\n", pr_opcodes[op].name, pr_opcodes[op].opname);
	}

	statement_linenums[numstatements] = pr_source_line;
	statement = &statements[numstatements];

	numstatements++;
	statement->op = op;
	statement->a = var_a;
	statement->b = var_b;
	statement->c = var_c;
	return statement;
}

void PR_Statement3( opcode_t *op, def_t *var_a, def_t *var_b, def_t *var_c, int force)
{
	dstatement_t	*statement;	

	if (!force && !PR_OPCodeValid(op))
	{
		PR_ParseError(ERR_BADEXTENSION, "Opcode \"%s|%s\" not valid for target\n", op->name, op->opname);
	}

	statement = &statements[numstatements];	
	numstatements++;
	
	statement_linenums[statement-statements] = pr_source_line;
	statement->op = op - pr_opcodes;
	statement->a = var_a ? var_a->ofs : 0;
	statement->b = var_b ? var_b->ofs : 0;
	statement->c = var_c ? var_c->ofs : 0;
}

/*
============
PR_ParseImmediate

Looks for a preexisting constant
============
*/
def_t *PR_ParseImmediate (void)
{
	def_t	*cn;

	if (pr_immediate_type == type_float)
	{
		cn = PR_MakeFloatDef(pr_immediate._float);
		PR_Lex();
		return cn;
	}
	if (pr_immediate_type == type_integer)
	{
		cn = PR_MakeIntDef(pr_immediate._int);
		PR_Lex();
		return cn;
	}

	if (pr_immediate_type == type_string)
	{
		cn = PR_MakeStringDef(pr_immediate_string);
		PR_Lex();
		return cn;
	}

	// check for a constant with the same value
	for (cn = pr.def_head.next; cn; cn = cn->next)// FIXME - hashtable.
	{
		if (!cn->initialized) continue;
		if (!cn->constant) continue;
		if (cn->type != pr_immediate_type) continue;
		if (pr_immediate_type == type_string)
		{
			if (!STRCMP(G_STRING(cn->ofs), pr_immediate_string) )
			{
				PR_Lex();
				return cn;
			}
		}
		else if (pr_immediate_type == type_float)
		{
			if ( G_FLOAT(cn->ofs) == pr_immediate._float )
			{
				PR_Lex();
				return cn;
			}
		}
		else if (pr_immediate_type == type_integer)
		{			
			if ( G_INT(cn->ofs) == pr_immediate._int )
			{
				PR_Lex ();
				return cn;
			}
		}
		else if (pr_immediate_type == type_vector)
		{
			if (( G_FLOAT(cn->ofs) == pr_immediate.vector[0] )
			&& (G_FLOAT(cn->ofs+1) == pr_immediate.vector[1] )
			&& (G_FLOAT(cn->ofs+2) == pr_immediate.vector[2]))
			{
				PR_Lex();
				return cn;
			}
		}
		else PR_ParseError (ERR_BADIMMEDIATETYPE, "weird immediate type");
	}

	// allocate a new one
	cn = (void *)Qalloc (sizeof(def_t));
	cn->next = NULL;
	pr.def_tail->next = cn;
	pr.def_tail = cn;

	cn->type = pr_immediate_type;
	cn->name = "IMMEDIATE";
	cn->constant = true;
	cn->initialized = 1;
	cn->scope = NULL; // always share immediates

	// copy the immediate to the global area
	cn->ofs = PR_GetFreeOffsetSpace(type_size[pr_immediate_type->type]);

	if (pr_immediate_type == type_string)
		pr_immediate.string = PR_CopyString (pr_immediate_string, opt_noduplicatestrings );
	Mem_Copy(pr_globals + cn->ofs, &pr_immediate, 4*type_size[pr_immediate_type->type]);
	PR_Lex ();

	return cn;
}

void PR_PrecacheSound (def_t *e, int ch)
{
	char		*n;
	int		i;

	if (e->type->type != ev_string) return;
	if (!e->ofs || e->temp || !e->constant) return;

	n = G_STRING(e->ofs);
	if (!*n) return;

	for (i = 0; i < numsounds; i++)
	{
		if (!STRCMP(n, precache_sounds[i])) return;
	}

	if (numsounds == MAX_SOUNDS) return;
	strcpy (precache_sounds[i], n);
	numsounds++;
}

void PR_PrecacheModel (def_t *e, int ch)
{
	char		*n;
	int		i;

	if (e->type->type != ev_string) return;
	if (!e->ofs || e->temp || !e->constant) return;	

	n = G_STRING(e->ofs);
	if (!*n) return;
	for (i = 0; i < nummodels; i++)
	{
		if (!STRCMP(n, precache_models[i])) return;
	}

	if (nummodels == MAX_MODELS) return;
	strcpy (precache_models[i], n);
	nummodels++;
}

void PR_SetModel (def_t *e)
{
	char		*n;
	int		i;

	if (e->type->type != ev_string) return;
	if (!e->ofs || e->temp || !e->constant) return;	

	n = G_STRING(e->ofs);
	if (!*n)return;

	for (i = 0; i < nummodels; i++)
	{
		if (!STRCMP(n, precache_models[i])) return;
	}

	if (nummodels == MAX_MODELS) return;
	strcpy (precache_models[i], n);
	nummodels++;
}

void PR_PrecacheTexture (def_t *e, int ch)
{
	char		*n;
	int		i;

	if (e->type->type != ev_string) return;
	if (!e->ofs || e->temp || !e->constant) return;

	n = G_STRING(e->ofs);
	if (!*n) return;

	for (i = 0; i < numtextures; i++)
	{
		if (!STRCMP(n, precache_textures[i])) return;
	}

	if (nummodels == MAX_MODELS) return;
	strcpy (precache_textures[i], n);
	numtextures++;
}

void PR_PrecacheFile (def_t *e, int ch)
{
	char		*n;
	int		i;

	if (e->type->type != ev_string) return;
	if (!e->ofs || e->temp || !e->constant) return;

	n = G_STRING(e->ofs);
	if (!*n) return;

	for (i = 0; i < numfiles; i++)
	{
		if (!STRCMP(n, precache_files[i])) return;
	}

	if (numfiles == MAX_FILES) return;
	strcpy (precache_files[i], n);
	numfiles++;
}

void PR_PrecacheFileOptimised (char *n, int ch)
{
	int		i;

	for (i = 0; i < numfiles; i++)
	{
		if (!STRCMP(n, precache_files[i])) return;
	}

	if (numfiles == MAX_FILES) return;
	strcpy (precache_files[i], n);
	numfiles++;
}

/*
============
PR_ParseFunctionCall

warning, the func could have no name set if it's a field call.
============
*/
def_t *PR_ParseFunctionCall (def_t *func)
{
	def_t		*e, *d, *old, *opev;
	int		arg = 0, i, np;
	type_t		*t, *p;
	dstatement_t	*st;
	int		extraparms = false;
	int		laststatement = numstatements;
	def_t		*param[MAX_PARMS+MAX_PARMS_EXTRA];

	func->timescalled++;
	t = func->type;

	if (t->type == ev_variant) t->aux_type = type_variant;
	if (t->type != ev_function && t->type != ev_variant)
	{
		PR_ParseErrorPrintDef (ERR_NOTAFUNCTION, func, "not a function");
	}

	if (!t->num_parms && t->type != ev_variant)	
	{
		// intrinsics. These base functions have variable arguments. I would check for (...) args too, 
		// but that might be used for extended builtin functionality. (this code wouldn't compile otherwise)
		if (!strcmp(func->name, "spawn"))
		{
			type_t *rettype;
			if (PR_CheckToken(")"))
			{
				rettype = type_entity;
			}
			else
			{
				rettype = TypeForName(PR_ParseName());
				if (!rettype || rettype->type != ev_entity)
					PR_ParseError(ERR_NOTANAME, "Spawn operator with undefined class");

				PR_Expect(")");
			}

			if (def_ret.temp->used)
				PR_ParseWarning(0, "Return value conflict - output is likly to be invalid");
			def_ret.temp->used = true;

			if (rettype != type_entity)
			{
				char genfunc[2048];
				sprintf(genfunc, "Class*%s", rettype->name);
				func = PR_GetDef(type_function, genfunc, NULL, true, 1);
				func->references++;
			}
			PR_SimpleStatement(OP_CALL0, func->ofs, 0, 0, false);
			def_ret.type = rettype;
			return &def_ret;
		}
		else if (!strcmp(func->name, "entnum") && !PR_CheckToken(")"))
		{
			// t = (a/%1) / (nextent(world)/%1)
			// a/%1 does a (int)entity to float conversion type thing

			e = PR_Expression(TOP_PRIORITY, false);
			PR_Expect(")");
			e = PR_Statement(&pr_opcodes[OP_DIV_F], e, PR_MakeIntDef(1), (dstatement_t **)0xffffffff);

			d = PR_GetDef(NULL, "nextent", NULL, false, 0);
			if (!d) PR_ParseError(0, "the nextent builtin is not defined");
			PR_FreeTemp(PR_Statement(&pr_opcodes[OP_STORE_F], e, &def_parms[0], (dstatement_t **)0xffffffff));
			d = PR_Statement(&pr_opcodes[OP_CALL0], d, NULL, NULL);
			d = PR_Statement(&pr_opcodes[OP_DIV_F], d, PR_MakeIntDef(1), (dstatement_t **)0xffffffff);

			e = PR_Statement(&pr_opcodes[OP_DIV_F], e, d, (dstatement_t **)0xffffffff);

			return e;
		}
	}	// so it's not an intrinsic.

	if (opt_precache_file)
	{
		// should we strip out all precache_file calls?
		if (!strncmp(func->name,"precache_file", 13))
		{
			if (pr_token_type == tt_immediate && pr_immediate_type->type == ev_string)
			{
				PR_Lex();
				PR_Expect(")");
				PR_PrecacheFileOptimised (pr_immediate_string, func->name[13]);
				def_ret.type = type_void;
				return &def_ret;
			}
		}
	}
	PR_LockActiveTemps(); // any temps before are likly to be used with the return value.

	// any temps referenced to build the parameters don't need to be locked.
	// copy the arguments to the global parameter variables
	if (t->type == ev_variant)
	{
		extraparms = true;
		np = 0;
	}
	else if (t->num_parms < 0)
	{
		extraparms = true;
		np = (t->num_parms * -1) - 1;
	}
	else np = t->num_parms;

	if (opt_vectorcalls && (t->num_parms == 1 && t->param->type == ev_vector))
	{	
		// if we're using vectorcalls
		// if it's a function, takes a vector

		// vectorcalls is an evil hack
		// it'll make your mod bigger and less efficient.
		// however, it'll cut down on numpr_globals, so your mod can become a much greater size.
		vec3_t arg;

		if (pr_token_type == tt_immediate && pr_immediate_type == type_vector)
		{
			Mem_Copy(arg, pr_immediate.vector, sizeof(arg));
			while(*pr_file_p == ' ' || *pr_file_p == '\t' || *pr_file_p == '\n') pr_file_p++;
			if (*pr_file_p == ')')
			{	
				// woot
				def_parms[0].ofs = OFS_PARM0 + 0;
				PR_FreeTemp(PR_Statement (&pr_opcodes[OP_STORE_F], PR_MakeFloatDef(arg[0]), &def_parms[0], (dstatement_t **)0xffffffff));
				def_parms[0].ofs = OFS_PARM0 + 1;
				PR_FreeTemp(PR_Statement (&pr_opcodes[OP_STORE_F], PR_MakeFloatDef(arg[1]), &def_parms[0], (dstatement_t **)0xffffffff));
				def_parms[0].ofs = OFS_PARM0 + 2;
				PR_FreeTemp(PR_Statement (&pr_opcodes[OP_STORE_F], PR_MakeFloatDef(arg[2]), &def_parms[0], (dstatement_t **)0xffffffff));
				def_parms[0].ofs = OFS_PARM0;

				PR_Lex();
				PR_Expect(")");
			}
			else
			{	// bum
				e = PR_Expression (TOP_PRIORITY, false);
				if (e->type->type != ev_vector)
				{
					if (flag_laxcasts)
					{
						PR_ParseWarning(WARN_LAXCAST, "type mismatch on parm %i - (%s should be %s)", 1, TypeName(e->type), TypeName(type_vector));
						PR_ParsePrintDef(WARN_LAXCAST, func);
					}
					else PR_ParseErrorPrintDef (ERR_TYPEMISMATCHPARM, func, "type mismatch on parm %i - (%s should be %s)", 1, TypeName(e->type), TypeName(type_vector));
				}
				PR_Expect(")");
				PR_FreeTemp(PR_Statement (&pr_opcodes[OP_STORE_V], e, &def_parms[0], (dstatement_t **)0xffffffff));
			}
		}
		else
		{	// bother
			e = PR_Expression (TOP_PRIORITY, false);
			if (e->type->type != ev_vector)
			{
				if (flag_laxcasts)
				{
					PR_ParseWarning(WARN_LAXCAST, "type mismatch on parm %i - (%s should be %s)", 1, TypeName(e->type), TypeName(type_vector));
					PR_ParsePrintDef(WARN_LAXCAST, func);
				}
				else PR_ParseErrorPrintDef (ERR_TYPEMISMATCHPARM, func, "type mismatch on parm %i - (%s should be %s)", 1, TypeName(e->type), TypeName(type_vector));
			}
			PR_Expect(")");
			PR_FreeTemp(PR_Statement (&pr_opcodes[OP_STORE_V], e, &def_parms[0], (dstatement_t **)0xffffffff));
		}
	}
	else
	{
		if (!PR_CheckToken(")"))
		{
			p = t->param;
			do
			{
				if (extraparms && arg >= MAX_PARMS)
					PR_ParseErrorPrintDef (ERR_TOOMANYPARAMETERSVARARGS, func, "More than %i parameters on varargs function", MAX_PARMS);
				else if (arg >= MAX_PARMS+MAX_PARMS_EXTRA)
					PR_ParseErrorPrintDef (ERR_TOOMANYTOTALPARAMETERS, func, "More than %i parameters", MAX_PARMS+MAX_PARMS_EXTRA);
				if (!extraparms && arg >= t->num_parms)
				{
					PR_ParseWarning (WARN_TOOMANYPARAMETERSFORFUNC, "too many parameters");
					PR_ParsePrintDef(WARN_TOOMANYPARAMETERSFORFUNC, func);
				}

				e = PR_Expression (TOP_PRIORITY, false);
				if (arg == 0 && func->name)
				{
					// save information for model and sound caching
					if (!strncmp(func->name,"precache_", 9))
					{
						if (!strncmp(func->name+9,"sound", 5))
							PR_PrecacheSound (e, func->name[14]);
						else if (!strncmp(func->name+9,"model", 5))
							PR_PrecacheModel (e, func->name[14]);
						else if (!strncmp(func->name+9,"texture", 7))
							PR_PrecacheTexture (e, func->name[16]);
						else if (!strncmp(func->name+9,"file", 4))
							PR_PrecacheFile (e, func->name[13]);
					}
				}
				if (arg >= MAX_PARMS)
				{
					if (!extra_parms[arg - MAX_PARMS])
					{
						d = (def_t *) Qalloc (sizeof(def_t));
						d->name = "extra parm";
						d->ofs = PR_GetFreeOffsetSpace (3);
						extra_parms[arg - MAX_PARMS] = d;
					}
					d = extra_parms[arg - MAX_PARMS];
				}
				else d = &def_parms[arg];

				if (pr_classtype && e->type->type == ev_field && p->type != ev_field)
				{	
					// convert.
					opev = PR_GetDef(type_entity, pevname, NULL, true, 1);
					switch(e->type->aux_type->type)
					{
					case ev_string:
						e = PR_Statement(pr_opcodes+OP_LOAD_S, opev, e, NULL);
						break;
					case ev_integer:
						e = PR_Statement(pr_opcodes+OP_LOAD_I, opev, e, NULL);
						break;
					case ev_float:
						e = PR_Statement(pr_opcodes+OP_LOAD_F, opev, e, NULL);
						break;
					case ev_function:
						e = PR_Statement(pr_opcodes+OP_LOAD_FNC, opev, e, NULL);
						break;
					case ev_vector:
						e = PR_Statement(pr_opcodes+OP_LOAD_V, opev, e, NULL);
						break;
					case ev_entity:
						e = PR_Statement(pr_opcodes+OP_LOAD_ENT, opev, e, NULL);
						break;
					default:
						Sys_Error("Bad member type. Try forced expansion");
					}
				}

				if (p)
				{
					if (typecmp(e->type, p))
					{
						if (p->type == ev_integer && e->type->type == ev_float)
						{
							// convert float -> int... is this a constant?
							e = PR_Statement(pr_opcodes+OP_CONV_FTOI, e, NULL, NULL);
						}
						else if (p->type == ev_float && e->type->type == ev_integer)
						{
							// convert float -> int... is this a constant?
							e = PR_Statement(pr_opcodes+OP_CONV_ITOF, e, NULL, NULL);
						}
						else if (p->type == ev_function && e->type->type == ev_integer && e->constant && !((int*)pr_globals)[e->ofs])
						{	
							// you're allowed to use int 0 to pass a null function pointer
							// this is basically because __NULL__ is defined as ~0 (int 0)
						}
						else if (p->type != ev_variant)
						{
							// can cast to variant whatever happens
							if (flag_laxcasts || (p->type == ev_function && e->type->type == ev_function))
							{
								PR_ParseWarning(WARN_LAXCAST, "type mismatch on parm %i - (%s should be %s)", arg+1, TypeName(e->type), TypeName(p));
								PR_ParsePrintDef(WARN_LAXCAST, func);
							}
							else PR_ParseErrorPrintDef (ERR_TYPEMISMATCHPARM, func, "type mismatch on parm %i - (%s should be %s)", arg+1, TypeName(e->type), TypeName(p));
						}
					}
					d->type = p;
					p = p->next;
				}		
				else d->type = type_void; // a vector copy will copy everything

				if (arg == 1 && !STRCMP(func->name, "setmodel"))
				{
					PR_SetModel(e);
				}

				param[arg] = e;
				arg++;
			} while (PR_CheckToken (","));

			if (t->num_parms != -1 && arg < np)
				PR_ParseWarning (WARN_TOOFEWPARAMS, "too few parameters on call to %s", func->name);
			PR_Expect (")");
		}
		else if (np)
		{
			PR_ParseWarning (WARN_TOOFEWPARAMS, "%s: Too few parameters", func->name);
			PR_ParsePrintDef (WARN_TOOFEWPARAMS, func);
		}

		for (i = 0; i < arg; i++)
		{
			if (i >= MAX_PARMS) d = extra_parms[i - MAX_PARMS];
			else d = &def_parms[i];

			if (param[i]->type->size>1 || !opt_nonvec_parms)
				PR_FreeTemp(PR_Statement (&pr_opcodes[OP_STORE_V], param[i], d, (dstatement_t **)0xffffffff));
			else
			{
				d->type = param[i]->type;
				PR_FreeTemp(PR_Statement (&pr_opcodes[OP_STORE_F], param[i], d, (dstatement_t **)0xffffffff));
			}
		}
	}

	if (def_ret.temp->used)
	{
		old = PR_GetTemp(def_ret.type);
		if (def_ret.type->size == 3) PR_FreeTemp(PR_Statement(&pr_opcodes[OP_STORE_V], &def_ret, old, NULL));
		else PR_FreeTemp(PR_Statement(&pr_opcodes[OP_STORE_F], &def_ret, old, NULL));
		PR_UnFreeTemp(old);
		PR_UnFreeTemp(&def_ret);
		PR_ParseWarning(WARN_FIXEDRETURNVALUECONFLICT, "Return value conflict - output is inefficient");
	}
	else old = NULL;

	if (strchr(func->name, ':') && laststatement && statements[laststatement-1].op == OP_LOAD_FNC && statements[laststatement-1].c == func->ofs)
	{	
		// we're entering C++ code with a different pevname.
		// FIXME: problems could occur with hexen2 calling conventions when parm0/1 is 'pev' or 'self'
		// thiscall. copy the right ent into 'pev' or 'self' (if it's not the same offset)

		d = PR_GetDef(type_entity, pevname, NULL, true, 1);
		if (statements[laststatement-1].a != d->ofs)
		{
			opev = PR_GetTemp(type_entity);
			PR_SimpleStatement(OP_STORE_ENT, d->ofs, opev->ofs, 0, false);
			PR_SimpleStatement(OP_STORE_ENT, statements[laststatement-1].a, d->ofs, 0, false);
		}
		else
		{
			opev = NULL;
			d = NULL;
		}
	}
	else
	{
		opev = NULL;
		d = NULL;
	}

	if (arg>MAX_PARMS) PR_FreeTemp(PR_Statement (&pr_opcodes[OP_CALL1-1+MAX_PARMS], func, 0, (dstatement_t **)&st));
	else if (arg) PR_FreeTemp(PR_Statement (&pr_opcodes[OP_CALL1-1+arg], func, 0, (dstatement_t **)&st));
	else PR_FreeTemp(PR_Statement (&pr_opcodes[OP_CALL0], func, 0, (dstatement_t **)&st));
	if (opev) PR_SimpleStatement(OP_STORE_ENT, opev->ofs, d->ofs, 0, false);

	for(; arg; arg--)
	{
		PR_FreeTemp(param[arg-1]);
	}

	if (old)
	{
		if (t->type == ev_variant)
		{
			d = PR_GetTemp(type_variant);
			PR_FreeTemp(PR_Statement(pr_opcodes+OP_STORE_F, &def_ret, d, NULL));
		}
		else
		{
			d = PR_GetTemp(t->aux_type);
			if (t->aux_type->size == 3) PR_FreeTemp(PR_Statement(pr_opcodes+OP_STORE_V, &def_ret, d, NULL));
			else PR_FreeTemp(PR_Statement(pr_opcodes+OP_STORE_F, &def_ret, d, NULL));
		}
		if (def_ret.type->size == 3) PR_FreeTemp(PR_Statement(pr_opcodes+OP_STORE_V, old, &def_ret, NULL));
		else PR_FreeTemp(PR_Statement(pr_opcodes+OP_STORE_F, old, &def_ret, NULL));
		PR_FreeTemp(old);
		PR_UnFreeTemp(&def_ret);
		PR_UnFreeTemp(d);

		return d;
	}
	
	if (t->type == ev_variant) def_ret.type = type_variant;
	else def_ret.type = t->aux_type;
	if (def_ret.temp->used) PR_ParseWarning(WARN_FIXEDRETURNVALUECONFLICT, "Return value conflict - output is inefficient");
	def_ret.temp->used = true;

	return &def_ret;
}

def_t *PR_MakeIntDef(int value)
{
	def_t	*cn;

	cn = Hash_GetKey(&intconstdefstable, value );
	if (cn) return cn;

	// allocate a new one
	cn = (void *)Qalloc (sizeof(def_t));
	cn->next = NULL;
	pr.def_tail->next = cn;
	pr.def_tail = cn;

	cn->type = type_integer;
	cn->name = "IMMEDIATE";
	cn->constant = true;
	cn->initialized = 1;
	cn->scope = NULL; // always share immediates
	cn->arraysize = 1;

	// copy the immediate to the global area
	cn->ofs = PR_GetFreeOffsetSpace (type_size[type_integer->type]);
	Hash_AddKey(&intconstdefstable, value, cn, Qalloc(sizeof(bucket_t)));
	G_INT(cn->ofs) = value;	

	return cn;
}

def_t *PR_MakeFloatDef(float value)
{
	def_t	*cn;
	union { float f; int i; } fi;

	fi.f = value;

	cn = Hash_GetKey(&floatconstdefstable, fi.i);
	if (cn) return cn;

	// allocate a new one
	cn = (void *)Qalloc(sizeof(def_t));
	cn->next = NULL;
	pr.def_tail->next = cn;
	pr.def_tail = cn;

	cn->type = type_float;
	cn->name = "IMMEDIATE";
	cn->constant = true;
	cn->initialized = 1;
	cn->scope = NULL; // always share immediates
	cn->arraysize = 1;

	// copy the immediate to the global area
	cn->ofs = PR_GetFreeOffsetSpace (type_size[type_integer->type]);
	Hash_AddKey(&floatconstdefstable, fi.i, cn, Qalloc(sizeof(bucket_t)));
	G_FLOAT(cn->ofs) = value;	

	return cn;
}

def_t *PR_MakeStringDef(char *value)
{
	def_t	*cn;
	int	string;

	cn = Hash_Get(&stringconstdefstable, value);
	if (cn) return cn;

	// allocate a new one
	cn = (void *)Qalloc (sizeof(def_t));
	cn->next = NULL;
	pr.def_tail->next = cn;
	pr.def_tail = cn;

	cn->type = type_string;
	cn->name = "IMMEDIATE";
	cn->constant = true;
	cn->initialized = 1;
	cn->scope = NULL; // always share immediates
	cn->arraysize = 1;

	// copy the immediate to the global area
	cn->ofs = PR_GetFreeOffsetSpace (type_size[type_integer->type]);
	string = PR_CopyString (value, opt_noduplicatestrings );
	Hash_Add(&stringconstdefstable, strings+string, cn, Qalloc(sizeof(bucket_t)));
	G_INT(cn->ofs) = string;	

	return cn;
}

type_t *PR_PointerTypeTo(type_t *type)
{
	type_t *newtype;
	newtype = PR_NewType("POINTER TYPE", ev_pointer);
	newtype->aux_type = type;
	return newtype;
}

def_t *PR_MemberInParentClass(char *name, type_t *class)
{	
	// if a member exists, return the member field (rather than mapped-to field)
	type_t	*mt;
	def_t	*def;
	int	p, np;
	char	membername[2048];

	if (!class)
	{
		def = PR_GetDef(NULL, name, NULL, 0, 0);
		if (def && def->type->type == ev_field)
			return def; // the member existed as a normal entity field.
		return NULL;
	}

	np = class->num_parms;
	for (p = 0, mt = class->param; p < np; p++, mt = mt->next)
	{
		if (strcmp(mt->name, name))
			continue;

		// the parent has it.
		sprintf(membername, "%s::"MEMBERFIELDNAME, class->name, mt->name);
		def = PR_GetDef(NULL, membername, NULL, false, 0);
		return def;
	}
	return PR_MemberInParentClass(name, class->parentclass);
}

/*
=========================
PR_EmitFieldsForMembers

create fields for the types, instanciate the members to the fields.
we retouch the parents each time to guarentee polymorphism works.
FIXME: virtual methods will not work properly. Need to trace down to see if a parent already defined it
=========================
*/
void PR_EmitFieldsForMembers(type_t *class)
{
	char	membername[2048];
	int	p, np, a;
	uint	o;
	type_t	*mt, *ft;
	def_t	*f, *m;
	
	// we created fields for each class when we defined the actual classes.
	// we need to go through each member and match it to the offset of it's parent class, 
	// if overloaded, or create a new field if not..
	// basictypefield is cleared before we do this
	// we emit the parent's fields first (every time), 
	// thus ensuring that we don't reuse parent fields on a child class.

	if (class->parentclass != type_entity)
	{
		// parents MUST have all thier fields set or inheritance would go crazy.
		PR_EmitFieldsForMembers(class->parentclass);
	}

	np = class->num_parms;
	mt = class->param;

	for (p = 0; p < np; p++, mt = mt->next)
	{
		sprintf(membername, "%s::"MEMBERFIELDNAME, class->name, mt->name);
		m = PR_GetDef(NULL, membername, NULL, false, 0);

		f = PR_MemberInParentClass(mt->name, class->parentclass);
		if(f)
		{
			if (m->arraysize > 1) Sys_Error("QCCLIB does not support overloaded arrays of members");
			a = 0;
			for (o = 0; o < m->type->size; o++)
			{
				((int *)pr_globals)[o+a*mt->size+m->ofs] = ((int *)pr_globals)[o+f->ofs];
			}
			continue;
		}
		for (a = 0; a < m->arraysize; a++)
		{
			// we need the type in here so saved games can still work 
			// without saving ints as floats. (would be evil)

			ft = PR_NewType(basictypenames[mt->type], ev_field);
			ft->aux_type = PR_NewType(basictypenames[mt->type], mt->type);
			ft->aux_type->aux_type = type_void;
			ft->size = ft->aux_type->size;
			ft = PR_FindType(ft);
			sprintf(membername, "__f_%s_%i", ft->name, ++basictypefield[mt->type]);
			f = PR_GetDef(ft, membername, NULL, true, 1);
		
			for (o = 0; o < m->type->size; o++)
			{
				((int *)pr_globals)[o+a*mt->size+m->ofs] = ((int *)pr_globals)[o+f->ofs];
			}	
			f->references++;
		}
	}
}

/*
=========================
PR_EmitClassFunctionTable

go through clas, do the virtual thing only if the child class does not override.
=========================
*/
void PR_EmitClassFunctionTable(type_t *class, type_t *childclass, def_t *ed, def_t **constructor)
{	
	char	membername[2048];
	type_t	*type, *oc;
	def_t	*point, *member, *virt;
	int	p;

	if (class->parentclass)
		PR_EmitClassFunctionTable(class->parentclass, childclass, ed, constructor);

	type = class->param;
	for (p = 0; p < class->num_parms; p++, type = type->next)
	{
		for (oc = childclass; oc != class; oc = oc->parentclass)
		{
			sprintf(membername, "%s::"MEMBERFIELDNAME, oc->name, type->name);
			if (PR_GetDef(NULL, membername, NULL, false, 0))
				break; // a child class overrides.
		}
		if (oc != class) continue;
		if (type->type == ev_function)
		{
			// FIXME: inheritance will not install all the member functions.
			sprintf(membername, "%s::"MEMBERFIELDNAME, class->name, type->name);
			member = PR_GetDef(NULL, membername, NULL, false, 1);
			if (!member)
			{
				PR_Warning(0, NULL, 0, "Member function %s was not defined", membername);
				continue;
			}
			if (!strcmp(type->name, class->name))
			{
				*constructor = member;
			}
			point = PR_Statement(&pr_opcodes[OP_ADDRESS], ed, member, NULL);
			sprintf(membername, "%s::%s", class->name, type->name);
			virt = PR_GetDef(type, membername, NULL, false, 1);
			PR_Statement(&pr_opcodes[OP_STOREP_FNC], virt, point, NULL);
		}
	}
}

/*
=========================
PR_EmitClassFromFunction

take all functions in the type, and parent types, and make sure the links all work properly.
=========================
*/
void PR_EmitClassFromFunction(def_t *scope, char *tname)
{
	type_t		*basetype;
	dfunction_t	*df;
	def_t		*virt;
	def_t		*ed, *opev, *pev;
	def_t		*constructor = NULL;

	basetype = TypeForName(tname);
	if (!basetype) PR_ParseError(ERR_INTERNAL, "Type %s was not defined...", tname);

	pr_scope = NULL;
	memset(basictypefield, 0, sizeof(basictypefield));
	PR_EmitFieldsForMembers(basetype);

	pr_scope = scope;
	df = &functions[numfunctions];
	numfunctions++;

	df->s_file = 0;
	df->s_name = 0;
	df->first_statement = numstatements;
	df->parm_size[0] = 1;
	df->numparms = 0;
	df->parm_start = numpr_globals;

	G_FUNCTION(scope->ofs) = df - functions;

	// locals here...
	ed = PR_GetDef(type_entity, "ent", pr_scope, true, 1);

	virt = PR_GetDef(type_function, "spawn", NULL, false, 0);
	if (!virt) PR_ParseError(ERR_INTERNAL, "spawn function was not defined\n");
	PR_SimpleStatement(OP_CALL0, virt->ofs, 0, 0, false); // calling convention doesn't come into it.
	
	PR_FreeTemp(PR_Statement(&pr_opcodes[OP_STORE_ENT], &def_ret, ed, NULL));

	ed->references = 1;	//there may be no functions.
	PR_EmitClassFunctionTable(basetype, basetype, ed, &constructor);

	if (constructor)
	{	
		pev = PR_GetDef(type_entity, pevname, NULL, false, 0);
		opev = PR_GetDef(type_entity, opevname, scope, true, 1);
		PR_FreeTemp(PR_Statement(&pr_opcodes[OP_STORE_ENT], pev, opev, NULL));
		PR_FreeTemp(PR_Statement(&pr_opcodes[OP_STORE_ENT], ed, pev, NULL));// return to our old pev. boom boom.
		PR_SimpleStatement(OP_CALL0, constructor->ofs, 0, 0, false);
		PR_FreeTemp(PR_Statement(&pr_opcodes[OP_STORE_ENT], opev, pev, NULL));
	}
	// apparently we do actually have to return something. *sigh*...
	PR_FreeTemp(PR_Statement(&pr_opcodes[OP_RETURN], ed, NULL, NULL));
	PR_FreeTemp(PR_Statement(&pr_opcodes[OP_DONE], NULL, NULL, NULL));
	PR_WriteAsmFunction(scope, df->first_statement, df->parm_start);
	locals_end = numpr_globals + basetype->size;
	df->locals = locals_end - df->parm_start;
	pr.localvars = NULL;
}
/*
============
PR_ParseValue

Returns the global ofs for the current token
============
*/
def_t *PR_ParseValue (type_t *assumeclass)
{
	def_t		*ao = NULL;// arrayoffset
	def_t		*d, *nd, *od;
	char		*name, membername[2048];
	int		i;
	
	// if the token is an immediate, allocate a constant for it
	if (pr_token_type == tt_immediate) return PR_ParseImmediate();

	if (PR_CheckToken("[")) // vector array acess
	{	
		// looks like a funky vector. :)
		vec3_t v;

		pr_immediate_type = type_vector;
		v[0] = pr_immediate._float;
		PR_Lex();
		v[1] = pr_immediate._float;
		PR_Lex();
		v[2] = pr_immediate._float;
		pr_immediate.vector[0] = v[0];
		pr_immediate.vector[1] = v[1];
		pr_immediate.vector[2] = v[2];
		pr_immediate_type = type_vector;
		d = PR_ParseImmediate();
		PR_Expect("]");
		return d;
	}
	name = PR_ParseName ();

	if (assumeclass && assumeclass->parentclass)
	{	
		// 'testvar' becomes 'pev::testvar'
		type_t	*type = assumeclass;
		d = NULL;

		// try getting a member.
		while(type != type_entity && type)
		{
			sprintf(membername, "%s::"MEMBERFIELDNAME, type->name, name);
			od = d = PR_GetDef (NULL, membername, pr_scope, false, 0);
			if (d) break;
			type = type->parentclass;
		}
		if (!d) od = d = PR_GetDef (NULL, name, pr_scope, false, 0);
	}
	else od = d = PR_GetDef (NULL, name, pr_scope, false, 0); // look through the defs
	
	if (!d)
	{
		// intrinsics, any old function with no args will do.
		if ((!strcmp(name, "spawn")) || (!strcmp(name, "entnum")))
			od = d = PR_GetDef (type_function, name, NULL, true, 1);
		else if (keyword_class && !strcmp(name, "this"))
		{
			if (!pr_classtype) PR_ParseError(ERR_NOTANAME, "Cannot use 'this' outside of an OO function\n");
			od = PR_GetDef(NULL, pevname, NULL, true, 1);
			od = d = PR_DummyDef(pr_classtype, "this", pr_scope, 1, od->ofs, true);
		}
		else if (keyword_class && !strcmp(name, "super"))
		{
			if (!pr_classtype) PR_ParseError(ERR_NOTANAME, "Cannot use 'super' outside of an OO function\n");
			od = PR_GetDef(NULL, pevname, NULL, true, 1);
			od = d = PR_DummyDef(pr_classtype, "super", pr_scope, 1, od->ofs, true);
		}
		else
		{
			od = d = PR_GetDef (type_variant, name, pr_scope, true, 1);
			if (!d) PR_ParseError (ERR_UNKNOWNVALUE, "Unknown value \"%s\"", name);
			else PR_ParseWarning (ERR_UNKNOWNVALUE, "Unknown value \"%s\".", name);
		}
	}
reloop:
	// FIXME: Make this work with double arrays/2nd level structures.
	// Should they just jump back to here?

	if (PR_CheckToken("["))
	{
		type_t	*newtype;
		if (ao)
		{
			numstatements--; // remove the last statement			
			nd = PR_Expression (TOP_PRIORITY, true);
			PR_Expect("]");

			if (d->type->size != 1) // we need to multiply it to find the offset.						
			{
				if (ao->type->type == ev_integer) nd = PR_Statement(&pr_opcodes[OP_MUL_I], nd, PR_MakeIntDef(d->type->size), NULL); // get add part
				else if (ao->type->type == ev_float) nd = PR_Statement(&pr_opcodes[OP_MUL_F], nd, PR_MakeFloatDef((float)d->type->size), NULL); // get add part
				else
				{
					PR_ParseError(ERR_BADARRAYINDEXTYPE, "Array offset is not of integer or float type");
					nd = NULL;
				}
			}
			if (nd->type->type == ao->type->type)
			{
				if (ao->type->type == ev_integer) ao = PR_Statement(&pr_opcodes[OP_ADD_I], ao, nd, NULL);	// get add part
				else if (ao->type->type == ev_float) ao = PR_Statement(&pr_opcodes[OP_ADD_F], ao, nd, NULL); // get add part
				else
				{
					PR_ParseError(ERR_BADARRAYINDEXTYPE, "Array offset is not of integer or float type");
					nd = NULL;
				}
			}
			else
			{
				if (nd->type->type == ev_float) nd = PR_Statement (&pr_opcodes[OP_CONV_FTOI], nd, 0, NULL);
				ao = PR_Statement(&pr_opcodes[OP_ADD_I], ao, nd, NULL); // get add part
			}
			newtype = d->type;
			d = od;
		}
		else
		{
			ao = PR_Expression (TOP_PRIORITY, true);
			PR_Expect("]");

			if (PR_OPCodeValid(&pr_opcodes[OP_LOADA_F]) && d->type->size != 1) // we need to multiply it to find the offset.
			{
				if (ao->type->type == ev_integer) ao = PR_Statement(&pr_opcodes[OP_MUL_I], ao, PR_MakeIntDef(d->type->size), NULL); // get add part
				else if (ao->type->type == ev_float) ao = PR_Statement(&pr_opcodes[OP_MUL_F], ao, PR_MakeFloatDef((float)d->type->size), NULL); // get add part
				else
				{
					nd = NULL;
					PR_ParseError(ERR_BADARRAYINDEXTYPE, "Array offset is not of integer or float type");
				}
			}
			newtype = d->type;
		}
		if (ao->type->type == ev_integer)
		{
			switch(newtype->type)
			{
			case ev_float:
				nd = PR_Statement(&pr_opcodes[OP_LOADA_F], d, ao, NULL); // get pointer to precise def.
				break;
			case ev_string:
				if (d->arraysize <= 1)
				{
					nd = PR_Statement(&pr_opcodes[OP_LOADP_C], d, PR_Statement (&pr_opcodes[OP_CONV_ITOF], ao, 0, NULL), NULL);	//get pointer to precise def.
					newtype = nd->type; //don't be fooled
				}
				else nd = PR_Statement(&pr_opcodes[OP_LOADA_S], d, ao, NULL); // get pointer to precise def.
				break;
			case ev_vector:
				nd = PR_Statement(&pr_opcodes[OP_LOADA_V], d, ao, NULL); // get pointer to precise def.
				break;
			case ev_entity:
				nd = PR_Statement(&pr_opcodes[OP_LOADA_ENT], d, ao, NULL); // get pointer to precise def.			
				break;
			case ev_field:
				nd = PR_Statement(&pr_opcodes[OP_LOADA_FLD], d, ao, NULL); // get pointer to precise def.
				break;
			case ev_function:
				nd = PR_Statement(&pr_opcodes[OP_LOADA_FNC], d, ao, NULL); // get pointer to precise def.
				nd->type = d->type;
				break;
			case ev_integer:
				nd = PR_Statement(&pr_opcodes[OP_LOADA_I], d, ao, NULL);// get pointer to precise def.
				break;
			case ev_struct:
				nd = PR_Statement(&pr_opcodes[OP_LOADA_I], d, ao, NULL);// get pointer to precise def.
				nd->type = d->type;
				break;
			default:
				PR_ParseError(ERR_NOVALIDOPCODES, "No op available. Try assembler");
				nd = NULL;
				break;
			}
			d = nd;
		}
		else if (ao->type->type == ev_float)
		{
			if (!PR_OPCodeValid(&pr_opcodes[OP_LOADA_F])) // q1 compatable.
			{	
				//you didn't see this, okay?
				def_t *funcretr;
				if (d->scope) PR_ParseError(0, "Scoped array without specific engine support");
				if (def_ret.temp->used && ao != &def_ret) PR_ParseWarning(0, "RETURN VALUE ALREADY IN USE");

				if (PR_CheckToken("="))
				{
					funcretr = PR_GetDef(type_function, va("ArraySet*%s", d->name), NULL, true, 1);
					nd = PR_Expression(TOP_PRIORITY, true);
					if (nd->type->type != d->type->type) PR_ParseErrorPrintDef(ERR_TYPEMISMATCH, d, "Type Mismatch on array assignment");

					def_parms[0].type = type_float;
					PR_FreeTemp(PR_Statement (&pr_opcodes[OP_STORE_F], ao, &def_parms[0], NULL));
					def_parms[1].type = nd->type;
					PR_FreeTemp(PR_Statement (&pr_opcodes[OP_STORE_V], nd, &def_parms[1], NULL));
					PR_Statement (&pr_opcodes[OP_CALL2], funcretr, 0, NULL);
					qcc_usefulstatement = true;
				}
				else
				{
					def_parms[0].type = type_float;
					PR_FreeTemp(PR_Statement (&pr_opcodes[OP_STORE_F], ao, &def_parms[0], NULL));
					funcretr = PR_GetDef(type_function, va("ArrayGet*%s", d->name), NULL, true, 1);
					PR_Statement (&pr_opcodes[OP_CALL1], funcretr, 0, NULL);
				}

				nd = &def_ret;
				d = nd;
				d->type = newtype;
				return d;
			}
			else
			{
				switch(newtype->type)
				{
				case ev_pointer:
					if (d->arraysize>1)	// use the array
					{
						nd = PR_Statement(&pr_opcodes[OP_LOADA_I], d, PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL); // get pointer to precise def.
						nd->type = d->type->aux_type;
					}
					else
					{	
						// dereference the pointer.
						switch(newtype->aux_type->type)
						{
						case ev_pointer:
							nd = PR_Statement(&pr_opcodes[OP_LOADP_I], d, PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL); // get pointer to precise def.
							nd->type = d->type->aux_type;
							break;
						case ev_float:
							nd = PR_Statement(&pr_opcodes[OP_LOADP_F], d, PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL); // get pointer to precise def.
							nd->type = d->type->aux_type;
							break;
						case ev_integer:
							nd = PR_Statement(&pr_opcodes[OP_LOADP_I], d, PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL); // get pointer to precise def.
							nd->type = d->type->aux_type;
							break;
						default:
							PR_ParseError(ERR_NOVALIDOPCODES, "No op available. Try assembler");
							nd = NULL;
							break;
						}
					}
					break;
				case ev_float:
					nd = PR_Statement(&pr_opcodes[OP_LOADA_F], d, PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);// get pointer to precise def.
					break;
				case ev_string:
					if (d->arraysize <= 1)
					{
						nd = PR_Statement(&pr_opcodes[OP_LOADP_C], d, ao, NULL);	//get pointer to precise def.
						newtype = nd->type;//don't be fooled
					}
					else nd = PR_Statement(&pr_opcodes[OP_LOADA_S], d, PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);// get pointer to precise def.
					break;
				case ev_vector:
					nd = PR_Statement(&pr_opcodes[OP_LOADA_V], d, PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);// get pointer to precise def.
					break;
				case ev_entity:
					nd = PR_Statement(&pr_opcodes[OP_LOADA_ENT], d, PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);// get pointer to precise def.			
					break;
				case ev_field:
					nd = PR_Statement(&pr_opcodes[OP_LOADA_FLD], d, PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);// get pointer to precise def.
					break;
				case ev_function:
					nd = PR_Statement(&pr_opcodes[OP_LOADA_FNC], d, PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);// get pointer to precise def.
					nd->type = d->type;
					break;
				case ev_integer:
					nd = PR_Statement(&pr_opcodes[OP_LOADA_I], d, PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);// get pointer to precise def.
					break;
                              	case ev_struct:
					nd = PR_Statement(&pr_opcodes[OP_LOADA_I], d, PR_Statement (&pr_opcodes[OP_CONV_FTOI], ao, 0, NULL), NULL);// get pointer to precise def.
					nd->type = d->type;
					break;
				default:
					PR_ParseError(ERR_NOVALIDOPCODES, "No op available. Try assembler");
					nd = NULL;
					break;
				}
			}
		}
		d = nd;
		d->type = newtype;
		goto reloop;
	}

	i = d->type->type;
	if (i == ev_pointer)
	{
		int j;
		type_t *type;
		if (PR_CheckToken(".") || PR_CheckToken("->"))
		{
			for (i = d->type->num_parms, type = d->type+1; i; i--, type++)
			{
				if (PR_CheckName(type->name))
				{
					// give result
					if (ao)
					{
						numstatements--;	//remove the last statement
						d = od;

						nd = PR_MakeIntDef(type->ofs);
						ao = PR_Statement(&pr_opcodes[OP_ADD_I], ao, nd, NULL);// get add part						
						// so that we may offset it and readd it.
					}
					else ao = PR_MakeIntDef(type->ofs);

					switch (type->type)
					{
					case ev_float:
						nd = PR_Statement(&pr_opcodes[OP_LOADP_F], d, ao, NULL); // get pointer to precise def.
						break;
					case ev_string:
						nd = PR_Statement(&pr_opcodes[OP_LOADP_S], d, ao, NULL); // get pointer to precise def.
						break;
					case ev_vector:
						nd = PR_Statement(&pr_opcodes[OP_LOADP_V], d, ao, NULL); // get pointer to precise def.
						break;
					case ev_entity:
						nd = PR_Statement(&pr_opcodes[OP_LOADP_ENT], d, ao, NULL); // get pointer to precise def.			
						break;
					case ev_field:
						nd = PR_Statement(&pr_opcodes[OP_LOADP_FLD], d, ao, NULL); // get pointer to precise def.
						break;
					case ev_function:
						nd = PR_Statement(&pr_opcodes[OP_LOADP_FNC], d, ao, NULL); // get pointer to precise def.
						nd->type = type;
						break;
					case ev_integer:
						nd = PR_Statement(&pr_opcodes[OP_LOADP_I], d, ao, NULL); // get pointer to precise def.
						break;
					default:
						PR_ParseError(ERR_NOVALIDOPCODES, "No op available. Try assembler");
						nd = NULL;
						break;
					}					
					d = nd;
					break;
				}
				if (type->num_parms) for (j = type->num_parms; j; j--) type++;
			}			
			if (!i) PR_ParseError (ERR_MEMBERNOTVALID, "\"%s\" is not a member of \"%s\"", pr_token, od->type->name);
			goto reloop;
		}
	}
	else if (i == ev_struct || i == ev_union)
	{
		int	j;
		type_t	*type;

		if (PR_CheckToken(".") || PR_CheckToken("->"))
		{
			for (i = d->type->num_parms, type = d->type+1; i; i--, type++)
			{
				if (PR_CheckName(type->name))
				{
					// give result
					if (ao)
					{
						numstatements--; // remove the last statement
						d = od;

						nd = PR_MakeIntDef(type->ofs);
						ao = PR_Statement(&pr_opcodes[OP_ADD_I], ao, nd, NULL); // get add part						

						// so that we may offset it and readd it.
					}
					else ao = PR_MakeIntDef(type->ofs);

					switch (type->type)
					{
					case ev_float:
						nd = PR_Statement(&pr_opcodes[OP_LOADA_F], d, ao, NULL); // get pointer to precise def.
						break;
					case ev_string:
						nd = PR_Statement(&pr_opcodes[OP_LOADA_S], d, ao, NULL); // get pointer to precise def.
						break;
					case ev_vector:
						nd = PR_Statement(&pr_opcodes[OP_LOADA_V], d, ao, NULL); // get pointer to precise def.
						break;
					case ev_entity:
						nd = PR_Statement(&pr_opcodes[OP_LOADA_ENT], d, ao, NULL); // get pointer to precise def.			
						break;
					case ev_field:
						nd = PR_Statement(&pr_opcodes[OP_LOADA_FLD], d, ao, NULL); // get pointer to precise def.
						break;
					case ev_function:
						nd = PR_Statement(&pr_opcodes[OP_LOADA_FNC], d, ao, NULL); // get pointer to precise def.
						nd->type = type;
						break;
					case ev_integer:
						nd = PR_Statement(&pr_opcodes[OP_LOADA_I], d, ao, NULL); // get pointer to precise def.
						break;
					default:
						PR_ParseError(ERR_NOVALIDOPCODES, "No op available. Try assembler");
						nd = NULL;
						break;
					}					
					d = nd;
					break;
				}
				if (type->num_parms) for (j = type->num_parms; j;j--) type++;
			}			
			if (!i) PR_ParseError (ERR_MEMBERNOTVALID, "\"%s\" is not a member of \"%s\"", pr_token, od->type->name);
			goto reloop;
		}
	}

	if (!keyword_class) return d;

	if (d->type->parentclass || d->type->type == ev_entity) // class
	{
		if (PR_CheckToken(".") || PR_CheckToken("->"))
		{
			def_t	*field;

			if (PR_CheckToken("("))
			{
				field = PR_Expression(TOP_PRIORITY, true);
				PR_Expect(")");
			}
			else field = PR_ParseValue(d->type);

			if (field->type->type == ev_field)
			{
				if (!field->type->aux_type)
				{
					PR_ParseWarning(ERR_INTERNAL, "Field with null aux_type");
					return PR_Statement(&pr_opcodes[OP_LOAD_FLD], d, field, NULL);
				}
				else
				{
					switch(field->type->aux_type->type)
					{
					case ev_integer:
						return PR_Statement(&pr_opcodes[OP_LOAD_I], d, field, NULL);
					case ev_field:
						d = PR_Statement(&pr_opcodes[OP_LOAD_FLD], d, field, NULL);
						nd = (void *)Qalloc (sizeof(def_t));
						memset (nd, 0, sizeof(def_t));		
						nd->type = field->type->aux_type;
						nd->ofs = d->ofs;
						nd->temp = d->temp;
						nd->constant = false;
						nd->name = d->name;
						return nd;
					case ev_float:
						return PR_Statement(&pr_opcodes[OP_LOAD_F], d, field, NULL);
					case ev_string:
						return PR_Statement(&pr_opcodes[OP_LOAD_S], d, field, NULL);
					case ev_vector:
						return PR_Statement(&pr_opcodes[OP_LOAD_V], d, field, NULL);
					case ev_function:
						// complicated for a typecast
						d = PR_Statement(&pr_opcodes[OP_LOAD_FNC], d, field, NULL);
						nd = (void *)Qalloc (sizeof(def_t));
						memset (nd, 0, sizeof(def_t));		
						nd->type = field->type->aux_type;
						nd->ofs = d->ofs;
						nd->temp = d->temp;
						nd->constant = false;
						nd->name = d->name;
						return nd;
					case ev_entity:
						return PR_Statement(&pr_opcodes[OP_LOAD_ENT], d, field, NULL);
					default:
						PR_ParseError(ERR_INTERNAL, "Bad field type");
						return d;
					}
				}
			}
			else PR_IncludeChunk(".", false, NULL);
		}
	}	
	return d;
}


/*
============
PR_Term
============
*/
def_t *PR_Term( void )
{
	def_t	*e, *e2;
	etype_t	t;

	if (pr_token_type == tt_punct) // a little extra speed...
	{
		if (PR_CheckToken("++"))
		{
			qcc_usefulstatement = true;
			e = PR_Term ();
			if (e->constant) PR_ParseWarning(WARN_ASSIGNMENTTOCONSTANT, "Assignment to constant %s", e->name);
			if (e->temp) PR_ParseWarning(WARN_ASSIGNMENTTOCONSTANT, "Hey! That's a temp! ++ operators cannot work on temps!");

			switch (e->type->type)
			{
			case ev_integer:
				PR_Statement3(&pr_opcodes[OP_ADD_I], e, PR_MakeIntDef(1), e, false);
				break;
			case ev_float:
				PR_Statement3(&pr_opcodes[OP_ADD_F], e, PR_MakeFloatDef(1), e, false);
				break;
			default:
				PR_ParseError(ERR_BADPLUSPLUSOPERATOR, "++ operator on unsupported type");
				break;
			}
			return e;
		}
		else if (PR_CheckToken("--"))
		{
			qcc_usefulstatement = true;
			e = PR_Term ();
			if (e->constant) PR_ParseWarning(WARN_ASSIGNMENTTOCONSTANT, "Assignment to constant %s", e->name);
			if (e->temp) PR_ParseWarning(WARN_ASSIGNMENTTOCONSTANT, "Hey! That's a temp! -- operators cannot work on temps!");
			switch (e->type->type)
			{
			case ev_integer:
				PR_Statement3(&pr_opcodes[OP_SUB_I], e, PR_MakeIntDef(1), e, false);
				break;
			case ev_float:
				PR_Statement3(&pr_opcodes[OP_SUB_F], e, PR_MakeFloatDef(1), e, false);
				break;
			default:
				PR_ParseError(ERR_BADPLUSPLUSOPERATOR, "-- operator on unsupported type");
				break;
			}
			return e;
		}
		if (PR_CheckToken ("!"))
		{
			e = PR_Expression (NOT_PRIORITY, false);
			t = e->type->type;
			switch(t)
			{
			case ev_float: 
				e2 = PR_Statement (&pr_opcodes[OP_NOT_F], e, 0, NULL); 
				break;
			case ev_string: 
				e2 = PR_Statement (&pr_opcodes[OP_NOT_S], e, 0, NULL);
				break;
			case ev_entity:
				e2 = PR_Statement (&pr_opcodes[OP_NOT_ENT], e, 0, NULL);
				break;
			case ev_vector:
				e2 = PR_Statement (&pr_opcodes[OP_NOT_V], e, 0, NULL);
				break;
			case ev_function:
				e2 = PR_Statement (&pr_opcodes[OP_NOT_FNC], e, 0, NULL);
				break;
			case ev_integer:
				e2 = PR_Statement (&pr_opcodes[OP_NOT_FNC], e, 0, NULL);
				break; // functions are integer values too.
			case ev_pointer:
				e2 = PR_Statement (&pr_opcodes[OP_NOT_FNC], e, 0, NULL);
				break; // Pointers are too. 
			default:
				e2 = NULL; // shut up compiler warning;
				PR_ParseError (ERR_BADNOTTYPE, "type mismatch for !");
				break;
			}
			return e2;
		}
		else if (PR_CheckToken ("&"))
		{
			int st = numstatements;
			e = PR_Expression (NOT_PRIORITY, false);
			t = e->type->type;

			if (st != numstatements) // woo, something like ent.field?
			{
				if ((unsigned)(statements[numstatements-1].op - OP_LOAD_F) < 6 || statements[numstatements-1].op == OP_LOAD_I || statements[numstatements-1].op == OP_LOAD_P)
				{
					statements[numstatements-1].op = OP_ADDRESS;
					PR_ParseWarning(0, "debug: &ent.field");
					e->type = PR_PointerType(e->type);
					return e;
				}
				else // this is a restriction that could be lifted, I just want to make sure that I got all the bits first.
				{
					PR_ParseError (ERR_BADNOTTYPE, "type mismatch for '&' Must be singular expression or field reference");
					return e;
				}
			}
			if (!PR_OPCodeValid(&pr_opcodes[OP_GLOBAL_ADD]))
				PR_ParseError (ERR_BADEXTENSION, "Cannot use addressof operator ('&') on a global. Please use the RELEASE or DEBUG target.");
			e2 = PR_Statement (&pr_opcodes[OP_GLOBAL_ADD], e, 0, NULL);
			e2->type = PR_PointerType(e->type);
			return e2;
		}
		else if (PR_CheckToken ("*"))
		{
			e = PR_Expression (NOT_PRIORITY, false);
			t = e->type->type;

			if (t != ev_pointer) PR_ParseError (ERR_BADNOTTYPE, "type mismatch for *");

			switch(e->type->aux_type->type)
			{
			case ev_float:
				e2 = PR_Statement (&pr_opcodes[OP_LOADP_F], e, 0, NULL);
				break;
			case ev_string:
				e2 = PR_Statement (&pr_opcodes[OP_LOADP_S], e, 0, NULL);
				break;
			case ev_vector:
				e2 = PR_Statement (&pr_opcodes[OP_LOADP_V], e, 0, NULL);
				break;
			case ev_entity:
				e2 = PR_Statement (&pr_opcodes[OP_LOADP_ENT], e, 0, NULL);
				break;
			case ev_field:
				e2 = PR_Statement (&pr_opcodes[OP_LOADP_FLD], e, 0, NULL);
				break;
			case ev_function:
				e2 = PR_Statement (&pr_opcodes[OP_LOADP_FLD], e, 0, NULL);
				break;
			case ev_integer:
				e2 = PR_Statement (&pr_opcodes[OP_LOADP_I], e, 0, NULL);
				break;
			case ev_pointer:
				e2 = PR_Statement (&pr_opcodes[OP_LOADP_I], e, 0, NULL);
				break;

			default:
				PR_ParseError (ERR_BADNOTTYPE, "type mismatch for * (unrecognized type)");
				e2 = NULL;
				break;
			}
			e2->type = e->type->aux_type;
			return e2;
		}
		else if (PR_CheckToken ("-"))
		{
			e = PR_Expression (NOT_PRIORITY, false);
			switch(e->type->type)
			{
			case ev_float:
				e2 = PR_Statement (&pr_opcodes[OP_SUB_F], PR_MakeFloatDef(0), e, NULL);
				break;
			case ev_integer:
				e2 = PR_Statement (&pr_opcodes[OP_SUB_I], PR_MakeIntDef(0), e, NULL);
				break;
			default:
				PR_ParseError (ERR_BADNOTTYPE, "type mismatch for -");
				e2 = NULL;
				break;
			}
			return e2;
		}
		else if (PR_CheckToken ("+"))
		{
			e = PR_Expression (NOT_PRIORITY, false);
			switch(e->type->type)
			{
			case ev_float:
				e2 = PR_Statement (&pr_opcodes[OP_ADD_F], PR_MakeFloatDef(0), e, NULL);
				break;
			case ev_integer:
				e2 = PR_Statement (&pr_opcodes[OP_ADD_I], PR_MakeIntDef(0), e, NULL);
				break;
			default:
				PR_ParseError (ERR_BADNOTTYPE, "type mismatch for +");
				e2 = NULL;
				break;
			}
			return e2;
		}
		if (PR_CheckToken ("("))
		{
			if (keyword_float && PR_CheckToken("float"))	//check for type casts
			{
				PR_Expect (")");
				e = PR_Term();
				if (e->type->type == ev_float) return e;
				else if (e->type->type == ev_integer)
					return PR_Statement (&pr_opcodes[OP_CONV_ITOF], e, 0, NULL);
				else if (e->type->type == ev_function) return e;
				PR_ParseWarning (0, "Not all vars make sence as floats");

				e2 = (void *)Qalloc (sizeof(def_t));
				memset (e2, 0, sizeof(def_t));		
				e2->type = type_float;
				e2->ofs = e->ofs;
				e2->constant = true;
				e2->temp = e->temp;
				return e2;
			}
			else if (PR_CheckKeyword(keyword_class, "class"))
			{
				type_t *classtype = TypeForName(PR_ParseName());
				if (!classtype) PR_ParseError(ERR_NOTANAME, "Class not defined for cast");

				PR_Expect (")");
				e = PR_Term();
				e2 = (void *)Qalloc (sizeof(def_t));
				memset (e2, 0, sizeof(def_t));		
				e2->type = classtype;
				e2->ofs = e->ofs;
				e2->constant = true;
				e2->temp = e->temp;
				return e2;
			}
			else if (PR_CheckKeyword(keyword_integer, "integer")) // check for type casts
			{
				PR_Expect (")");
				e = PR_Term();
				if (e->type->type == ev_integer) return e;
				else if (e->type->type == ev_float)
					return PR_Statement (&pr_opcodes[OP_CONV_FTOI], e, 0, NULL);
				else PR_ParseError (ERR_BADTYPECAST, "invalid typecast");
			}
			else if (PR_CheckKeyword(keyword_int, "int")) // check for type casts
			{
				PR_Expect (")");
				e = PR_Term();
				if (e->type->type == ev_integer) return e;
				else if (e->type->type == ev_float)
					return PR_Statement (&pr_opcodes[OP_CONV_FTOI], e, 0, NULL);
				else PR_ParseError (ERR_BADTYPECAST, "invalid typecast");
			}
			else
			{
				bool oldcond = conditional;
				conditional = conditional ? 2:0;
				e = PR_Expression (TOP_PRIORITY, false);
				PR_Expect (")");
				conditional = oldcond;
			}
			return e;
		}
	}
	return PR_ParseValue (pr_classtype);
}

int PR_canConv(def_t *from, etype_t to)
{
	if (from->type->type == to) return 0;
	if (from->type->type == ev_vector && to == ev_float) return 4;

	if (pr_classtype)
	{
		if (from->type->type == ev_field)
		{
			if (from->type->aux_type->type == to)
				return 1;
		}
	}
	
	if (from->type->type == ev_integer && to == ev_function)
		return 1;

	return -100;
}

/*
==============
PR_Expression
==============
*/
def_t *PR_Expression (int priority, bool allowcomma)
{
	dstatement_t	*st;
	def_t		*e, *e2;
	opcode_t		*op, *oldop, *bestop;
	int		opnum, numconversions, c;
	etype_t		type_a, type_b, type_c;

	if (priority == 0) return PR_Term();

	e = PR_Expression(priority - 1, allowcomma);

	while (1)
	{
		if (priority == 1)
		{
			if (PR_CheckToken ("(") )
			{
				qcc_usefulstatement = true;
				return PR_ParseFunctionCall(e);
			}
			if (PR_CheckToken ("?"))
			{
				dstatement_t *fromj, *elsej;
				PR_Statement(&pr_opcodes[OP_IFNOT], e, NULL, &fromj);
				e = PR_Expression(TOP_PRIORITY, true);
				e2 = PR_GetTemp(e->type);
				PR_Statement(&pr_opcodes[(e2->type->size>=3)?OP_STORE_V:OP_STORE_F], e, e2, NULL);

				PR_Expect(":");
				PR_Statement(&pr_opcodes[OP_GOTO], NULL, NULL, &elsej);
				fromj->b = &statements[numstatements] - fromj;
				e = PR_Expression(TOP_PRIORITY, true);

				if (typecmp(e->type, e2->type) != 0) PR_ParseError(0, "Ternary operator with mismatching types\n");
				PR_Statement(&pr_opcodes[(e2->type->size>=3)?OP_STORE_V:OP_STORE_F], e, e2, NULL);

				elsej->a = &statements[numstatements] - elsej;
				return e2;
			}
			if (allowcomma && PR_CheckToken (","))
			{
				PR_FreeTemp(e);
				return PR_Expression(TOP_PRIORITY, true);
			}
		}
		opnum = 0;

		if (pr_token_type == tt_immediate)
		{
			if (pr_immediate_type->type == ev_float)
			{
				if (pr_immediate._float < 0)
				{
					// hehehe... was a minus all along...
					PR_IncludeChunk(pr_token, true, NULL);
					strcpy(pr_token, "+"); // two negatives would make a positive.
					pr_token_type = tt_punct;
				}
			}
		}
		if (pr_token_type != tt_punct)
		{
			PR_ParseWarning(WARN_UNEXPECTEDPUNCT, "Expected punctuation");
		}

		// go straight for the correct priority.
		for (op = opcodeprioritized[priority][opnum]; op; op = opcodeprioritized[priority][++opnum])
		{
			if (!PR_CheckToken (op->name)) continue;
			st = NULL;

			if ( op->associative != ASSOC_LEFT )
			{
				// if last statement is an indirect, change it to an address of
				if (!simplestore && ((uint)(statements[numstatements-1].op - OP_LOAD_F) < 6 || statements[numstatements-1].op == OP_LOAD_I || statements[numstatements-1].op == OP_LOAD_P) && statements[numstatements-1].c == e->ofs)
				{
					qcc_usefulstatement=true;
					statements[numstatements-1].op = OP_ADDRESS;
					type_pointer->aux_type->type = e->type->type;
					e->type = type_pointer;
				}
				// if last statement retrieved a value, switch it to retrieve a usable pointer.
				if ( !simplestore && (unsigned)(statements[numstatements-1].op - OP_LOADA_F) < 7)// || statements[numstatements-1].op == OP_LOADA_C)
				{
					statements[numstatements-1].op = OP_GLOBAL_ADD;
					type_pointer->aux_type->type = e->type->type;
					e->type = type_pointer;
				}
				if ( !simplestore && (unsigned)(statements[numstatements-1].op - OP_LOADP_F) < 7)
				{
					statements[numstatements-1].op = OP_ADD_I;
				}
				if ( !simplestore && statements[numstatements-1].op == OP_LOADP_C && e->ofs == statements[numstatements-1].c)
				{
					statements[numstatements-1].op = OP_ADD_SF;
					e->type = type_string;

					// now we want to make sure that string = float can't work without it being a dereferenced pointer. (we don't want to allow storep_c without dereferece)
					e2 = PR_Expression (priority, allowcomma);
					if (e2->type->type == ev_float) op = &pr_opcodes[OP_STOREP_C];
				}
				else e2 = PR_Expression (priority, allowcomma);
			}
			else
			{
				if (op->priority == 7 && opt_logicops)
				{
					st = &statements[numstatements];
					if (*op->name == '&')	//statement 3 because we don't want to optimise this into if from not ifnot
						PR_Statement3(&pr_opcodes[OP_IFNOT], e, NULL, NULL, false);
					else PR_Statement3(&pr_opcodes[OP_IF], e, NULL, NULL, false);
				}
				e2 = PR_Expression (priority-1, allowcomma);
			}

			// type check
			type_a = e->type->type;
			type_b = e2->type->type;

			if (op->name[0] == '.')
			{
				// field access gets type from field
				if (e2->type->aux_type) type_c = e2->type->aux_type->type;
				else type_c = -1; // not a field
			}
			else type_c = ev_void;
				
			oldop = op;
			bestop = NULL;
			numconversions = 32767;			

			while (op)
			{
				if (!(type_c != ev_void && type_c != (*op->type_c)->type))
				{
					if(!STRCMP (op->name , oldop->name)) // matches
					{
						// return values are never converted - what to?
						if (op->associative!=ASSOC_LEFT)
						{
							// assignment
							if (op->type_a == &type_pointer) // ent var
							{
								if (e->type->type != ev_pointer) c = -200; // don't cast to a pointer.
								else if ((*op->type_c)->type == ev_void && op->type_b == &type_pointer && e2->type->type == ev_pointer)
									c = 0; // generic pointer... FIXME: is this safe? make sure both sides are equivelent
								else if (e->type->aux_type->type != (*op->type_b)->type) // if e isn't a pointer to a type_b
									c = -200;	// don't let the conversion work
								else c = PR_canConv(e2, (*op->type_c)->type);
							}
							else
							{
								c = PR_canConv(e2, (*op->type_b)->type);
								if (type_a != (*op->type_a)->type) // in this case, a is the final assigned value
									c = -300;	// don't use this op, as we must not change var b's type
							}
						}
						else
						{
							if (op->type_a == &type_pointer) // ent var
							{
								// if e isn't a pointer to a type_b
								if (e2->type->type != ev_pointer || e2->type->aux_type->type != (*op->type_b)->type)
									c = -200;	// don't let the conversion work
								else c = 0;
							}
							else
							{
								c = PR_canConv(e, (*op->type_a)->type);
								c += PR_canConv(e2, (*op->type_b)->type);
							}
						}

						if (c >= 0 && c < numconversions)
						{
							bestop = op;
							numconversions = c;
							if (c == 0) break; // can't get less conversions than 0...
						}
					}				
					else break;
				}
				op = opcodeprioritized[priority][++opnum];
			}
			if (bestop == NULL)
			{
				if (oldop->priority == TOP_PRIORITY) op = oldop;
				else
				{
					if (flag_laxcasts)
					{
						op = oldop;
						PR_ParseWarning(WARN_LAXCAST, "type mismatch for %s (%s and %s)", oldop->name, e->type->name, e2->type->name);
					}
					else PR_ParseError (ERR_TYPEMISMATCH, "type mismatch for %s (%s and %s)", oldop->name, e->type->name, e2->type->name);
				}
			}
			else
			{
				if (numconversions>3) PR_ParseWarning(WARN_IMPLICITCONVERSION, "Implicit conversion");
				op = bestop;
			}

			if (st) st->b = &statements[numstatements] - st;
			if (op->associative != ASSOC_LEFT)
			{
				qcc_usefulstatement = true;
				if (e->constant || e->ofs < OFS_PARM0)
				{
					if (e->type->type == ev_function)
					{
						PR_ParseWarning(WARN_ASSIGNMENTTOCONSTANTFUNC, "Assignment to function %s", e->name);
						PR_ParsePrintDef(WARN_ASSIGNMENTTOCONSTANTFUNC, e);
					}
					else
					{
						PR_ParseWarning(WARN_ASSIGNMENTTOCONSTANT, "Assignment to constant %s", e->name);
						PR_ParsePrintDef(WARN_ASSIGNMENTTOCONSTANT, e);
					}
				}
				if (conditional & 1) PR_ParseWarning(WARN_ASSIGNMENTINCONDITIONAL, "Assignment in conditional");
				e = PR_Statement (op, e2, e, NULL);
			}
			else e = PR_Statement (op, e, e2, NULL);
			
			// field access gets type from field
			if (type_c != ev_void) e->type = e2->type->aux_type;
			break;
		}
		if (!op)
		{
			if (e == NULL) PR_ParseError(ERR_INTERNAL, "e == null");
			if (!STRCMP(pr_token, "++"))
			{
				// if the last statement was an ent.float (or something)
				if (((uint)(statements[numstatements-1].op - OP_LOAD_F) < 6 || statements[numstatements-1].op == OP_LOAD_I) && statements[numstatements-1].c == e->ofs)
				{	
					def_t	*e3; // we have our load.
					// the only inefficiency here is with an extra temp (we can't reuse the original)
					// this is not a problem, as the optimise temps or locals marshalling can clean these up for us
					// 1. load
					// 2. add to temp
					// 3. store temp to offset
					// 4. return original loaded (which is not at the same offset as the pointer we store to)

					qcc_usefulstatement = true;
					e2 = PR_GetTemp(type_float);
					e3 = PR_GetTemp(type_pointer);
					PR_SimpleStatement(OP_ADDRESS, statements[numstatements-1].a, statements[numstatements-1].b, e3->ofs, false);
					if (e->type->type == ev_float)
					{
						PR_Statement3(&pr_opcodes[OP_ADD_F], e, PR_MakeFloatDef(1), e2, false);
						PR_Statement3(&pr_opcodes[OP_STOREP_F], e2, e3, NULL, false);
					}
					else if (e->type->type == ev_integer)
					{
						PR_Statement3(&pr_opcodes[OP_ADD_I], e, PR_MakeIntDef(1), e2, false);
						PR_Statement3(&pr_opcodes[OP_STOREP_I], e2, e3, NULL, false);
					}
					else
					{
						PR_ParseError(ERR_PARSEERRORS, "-- suffix operator results in nonstandard behaviour. Use -=1 or prefix form instead");
						PR_IncludeChunk("-=1", false, NULL);
					}
					PR_FreeTemp(e2);
					PR_FreeTemp(e3);
				}
				else if (e->type->type == ev_float)
				{
					// 1. copy to temp
					// 2. add to original
					// 3. return temp (which == original)

					PR_ParseWarning(WARN_INEFFICIENTPLUSPLUS, "++ suffix operator results in inefficient behaviour. Use += 1 or prefix form instead");
					qcc_usefulstatement=true;

					e2 = PR_GetTemp(type_float);
					PR_Statement3(&pr_opcodes[OP_STORE_F], e, e2, NULL, false);
					PR_Statement3(&pr_opcodes[OP_ADD_F], e, PR_MakeFloatDef(1), e, false);
					PR_FreeTemp(e);
					e = e2;
				}
				else if (e->type->type == ev_integer)
				{
					PR_ParseWarning(WARN_INEFFICIENTPLUSPLUS, "++ suffix operator results in inefficient behaviour. Use +=1 or prefix form instead");
					qcc_usefulstatement=true;

					e2 = PR_GetTemp(type_integer);
					PR_Statement3(&pr_opcodes[OP_STORE_I], e, e2, NULL, false);
					PR_Statement3(&pr_opcodes[OP_ADD_I], e, PR_MakeIntDef(1), e, false);
					PR_FreeTemp(e);
					e = e2;
				}
				else
				{
					PR_ParseWarning(WARN_NOTSTANDARDBEHAVIOUR, "++ suffix operator results in nonstandard behaviour. Use +=1 or prefix form instead");
					PR_IncludeChunk("+=1", false, NULL);
				}
				PR_Lex();
			}
			else if (!STRCMP(pr_token, "--"))
			{
				if (((uint)(statements[numstatements-1].op - OP_LOAD_F) < 6 || statements[numstatements-1].op == OP_LOAD_I) && statements[numstatements-1].c == e->ofs)
				{	
					def_t		*e3; // we have our load.
					// 1. load
					// 2. add to temp
					// 3. store temp to offset
					// 4. return original loaded (which is not at the same offset as the pointer we store to)

					e2 = PR_GetTemp(type_float);
					e3 = PR_GetTemp(type_pointer);
					PR_SimpleStatement(OP_ADDRESS, statements[numstatements-1].a, statements[numstatements-1].b, e3->ofs, false);
					if (e->type->type == ev_float)
					{
						PR_Statement3(&pr_opcodes[OP_SUB_F], e, PR_MakeFloatDef(1), e2, false);
						PR_Statement3(&pr_opcodes[OP_STOREP_F], e2, e3, NULL, false);
					}
					else if (e->type->type == ev_integer)
					{
						PR_Statement3(&pr_opcodes[OP_SUB_I], e, PR_MakeIntDef(1), e2, false);
						PR_Statement3(&pr_opcodes[OP_STOREP_I], e2, e3, NULL, false);
					}
					else
					{
						PR_ParseError(ERR_PARSEERRORS, "-- suffix operator results in nonstandard behaviour. Use -=1 or prefix form instead");
						PR_IncludeChunk("-=1", false, NULL);
					}
					PR_FreeTemp(e2);
					PR_FreeTemp(e3);
				}
				else if (e->type->type == ev_float)
				{
					PR_ParseWarning(WARN_INEFFICIENTPLUSPLUS, "-- suffix operator results in inefficient behaviour. Use -= 1 or prefix form instead");
					qcc_usefulstatement=true;

					e2 = PR_GetTemp(type_float);
					PR_Statement3(&pr_opcodes[OP_STORE_F], e, e2, NULL, false);
					PR_Statement3(&pr_opcodes[OP_SUB_F], e, PR_MakeFloatDef(1), e, false);
					PR_FreeTemp(e);
					e = e2;
				}
				else if (e->type->type == ev_integer)
				{
					PR_ParseWarning(WARN_INEFFICIENTPLUSPLUS, "-- suffix operator results in inefficient behaviour. Use -= 1 or prefix form instead");
					qcc_usefulstatement=true;

					e2 = PR_GetTemp(type_integer);
					PR_Statement3(&pr_opcodes[OP_STORE_I], e, e2, NULL, false);
					PR_Statement3(&pr_opcodes[OP_SUB_I], e, PR_MakeIntDef(1), e, false);
					PR_FreeTemp(e);
					e = e2;
				}
				else
				{
					PR_ParseWarning(WARN_NOTSTANDARDBEHAVIOUR, "-- suffix operator results in nonstandard behaviour. Use -= 1 or prefix form instead");
					PR_IncludeChunk("-=1", false, NULL);
				}
				PR_Lex();
			}
			break; // next token isn't at this priority level
		}
	}
	if (e == NULL) PR_ParseError(ERR_INTERNAL, "e == null");
	return e;
}

void PR_GotoStatement (dstatement_t *patch2, char *labelname)
{
	if (num_gotos >= max_gotos)
	{
		max_gotos += 8;
		// realloc also used for first allocate
		pr_gotos = Qrealloc(pr_gotos, sizeof(*pr_gotos)*max_gotos);
	}

	strncpy(pr_gotos[num_gotos].name, labelname, sizeof(pr_gotos[num_gotos].name) -1);
	pr_gotos[num_gotos].lineno = pr_source_line;
	pr_gotos[num_gotos].statementno = patch2 - statements;
	num_gotos++;
}

bool PR_StatementBlocksMatch(dstatement_t *p1, int p1count, dstatement_t *p2, int p2count)
{
	if (p1count != p2count) return false;

	while(p1count > 0)
	{
		if (p1->op != p2->op) return false;
		if (p1->a != p2->a) return false;
		if (p1->b != p2->b) return false;
		if (p1->c != p2->c) return false;
		p1++, p2++, p1count--;
	}
	return true;
}

/*
============
PR_ParseStatement

============
*/
void PR_ParseStatement (void)
{
	int		continues;
	int		breaks;
	int		cases;
	int		i;
	def_t		*e, *e2;
	dstatement_t	*patch1, *patch2, *patch3;
	int		statementstart = pr_source_line;

	if (PR_CheckToken ("{"))
	{
		e = pr.localvars;
		while (!PR_CheckToken("}")) PR_ParseStatement ();

		if (pr_subscopedlocals)
		{
			for (e2 = pr.localvars; e2 != e; e2 = e2->nextlocal)
			{
				Hash_RemoveData(&localstable, e2->name, e2);
			}
		}
		return;
	}
	if (PR_CheckKeyword(keyword_return, "return"))
	{
		if (PR_CheckToken (";"))
		{
			if (pr_scope->type->aux_type->type != ev_void)
				PR_ParseWarning(WARN_MISSINGRETURNVALUE, "\'%s\' should return %s", pr_scope->name, pr_scope->type->aux_type->name);
			if (opt_return_only) PR_FreeTemp(PR_Statement (&pr_opcodes[OP_DONE], 0, 0, NULL));
			else PR_FreeTemp(PR_Statement (&pr_opcodes[OP_RETURN], 0, 0, NULL));
			return;
		}
		e = PR_Expression (TOP_PRIORITY, true);
		PR_Expect (";");
		if (pr_scope->type->aux_type->type != e->type->type)
			PR_ParseWarning(WARN_WRONGRETURNTYPE, "\'%s\' returned %s, expected %s", pr_scope->name, e->type->name, pr_scope->type->aux_type->name);
		PR_FreeTemp(PR_Statement (&pr_opcodes[OP_RETURN], e, 0, NULL));
		return;
	}
	if (PR_CheckKeyword(keyword_while, "while"))
	{
		continues = num_continues;
		breaks = num_breaks;

		PR_Expect ("(");
		patch2 = &statements[numstatements];
		conditional = 1;
		e = PR_Expression (TOP_PRIORITY, true);
		conditional = 0;
		if (((e->constant && !e->temp) || !STRCMP(e->name, "IMMEDIATE")) && opt_compound_jumps)
		{
			if (!G_INT(e->ofs))
			{
				PR_ParseWarning(0, "while(0)?");
				PR_FreeTemp(PR_Statement (&pr_opcodes[OP_GOTO], 0, 0, &patch1));
			}
			else patch1 = NULL;
		}
		else
		{
			if (e->constant && !e->temp)
			{
				if (!G_FLOAT(e->ofs))
					PR_FreeTemp(PR_Statement (&pr_opcodes[OP_GOTO], 0, 0, &patch1));
				else patch1 = NULL;
			}
			else if (!typecmp( e->type, type_string) && flag_ifstring)	//special case, as strings are now pointers, not offsets from string table
			{
				PR_ParseWarning(0, "while (string) can result in bizzare behaviour");
				PR_FreeTemp(PR_Statement (&pr_opcodes[OP_IFNOTS], e, 0, &patch1));
			}
			else PR_FreeTemp(PR_Statement (&pr_opcodes[OP_IFNOT], e, 0, &patch1));
		}
		PR_Expect (")"); // after the line number is noted..
		PR_ParseStatement ();
		PR_FreeTemp(PR_Statement (&pr_opcodes[OP_GOTO], NULL, 0, &patch3));
		patch3->a = patch2 - patch3;
		if (patch1)
		{
			if (patch1->op == OP_GOTO) patch1->a = &statements[numstatements] - patch1;
			else patch1->b = &statements[numstatements] - patch1;
		}

		if (breaks != num_breaks)
		{
			for(i = breaks; i < num_breaks; i++)
			{
				patch1 = &statements[pr_breaks[i]]; // jump to after the return-to-top goto
				statements[pr_breaks[i]].a = &statements[numstatements] - patch1;
			}
			num_breaks = breaks;
		}
		if (continues != num_continues)
		{
			for(i = continues; i < num_continues; i++)
			{
				patch1 = &statements[pr_continues[i]];
				statements[pr_continues[i]].a = patch2 - patch1; // jump back to top
			}
			num_continues = continues;
		}
		return;
	}
	if (PR_CheckKeyword(keyword_for, "for"))
	{
		int		old_numstatements;
		int		numtemp, i;
		int		linenum[32];
		dstatement_t	temp[sizeof(linenum)/sizeof(linenum[0])];

		continues = num_continues;
		breaks = num_breaks;

		PR_Expect("(");
		if (!PR_CheckToken(";"))
		{
			PR_FreeTemp(PR_Expression(TOP_PRIORITY, true));
			PR_Expect(";");
		}

		patch2 = &statements[numstatements];
		if (!PR_CheckToken(";"))
		{
			conditional = 1;
			e = PR_Expression(TOP_PRIORITY, true);
			conditional = 0;
			PR_Expect(";");
		}
		else e = NULL;

		if (!PR_CheckToken(")"))
		{
			old_numstatements = numstatements;
			PR_FreeTemp(PR_Expression(TOP_PRIORITY, true));
			numtemp = numstatements - old_numstatements;
			if (numtemp > sizeof(linenum)/sizeof(linenum[0]))
				PR_ParseError(ERR_TOOCOMPLEX, "Update expression too large");
			numstatements = old_numstatements;
			for (i = 0 ; i < numtemp ; i++)
			{
				linenum[i] = statement_linenums[numstatements + i];
				temp[i] = statements[numstatements + i];
			}
			PR_Expect(")");
		}
		else numtemp = 0;

		if (e) PR_FreeTemp(PR_Statement(&pr_opcodes[OP_IFNOT], e, 0, &patch1));
		else patch1 = NULL;
		if (!PR_CheckToken(";")) PR_ParseStatement(); // don't give the hanging ';' warning.
		patch3 = &statements[numstatements];

		for (i = 0; i < numtemp; i++)
		{
			statement_linenums[numstatements] = linenum[i];
			statements[numstatements++] = temp[i];
		}

		PR_SimpleStatement(OP_GOTO, patch2 - &statements[numstatements], 0, 0, false);
		if (patch1) patch1->b = &statements[numstatements] - patch1;

		if (breaks != num_breaks)
		{
			for(i = breaks; i < num_breaks; i++)
			{	
				patch1 = &statements[pr_breaks[i]];
				statements[pr_breaks[i]].a = &statements[numstatements] - patch1;
			}
			num_breaks = breaks;
		}
		if (continues != num_continues)
		{
			for(i = continues; i < num_continues; i++)
			{
				patch1 = &statements[pr_continues[i]];
				statements[pr_continues[i]].a = patch3 - patch1;
			}
			num_continues = continues;
		}
		return;
	}
	if (PR_CheckKeyword(keyword_do, "do"))
	{
		continues = num_continues;
		breaks = num_breaks;

		patch1 = &statements[numstatements];
		PR_ParseStatement ();
		PR_Expect ("while");
		PR_Expect ("(");
		conditional = 1;
		e = PR_Expression (TOP_PRIORITY, true);
		conditional = 0;

		if (e->constant && !e->temp)
		{
			if (G_FLOAT(e->ofs))
			{
				PR_FreeTemp(PR_Statement (&pr_opcodes[OP_GOTO], NULL, 0, &patch2));
				patch2->a = patch1 - patch2;
			}
		}
		else
		{
			if (!typecmp( e->type, type_string) && flag_ifstring)
			{
				PR_ParseWarning(WARN_IFSTRING_USED, "do {} while(string) can result in bizzare behaviour");
				PR_FreeTemp(PR_Statement (&pr_opcodes[OP_IFS], e, NULL, &patch2));
			}
			else PR_FreeTemp(PR_Statement (&pr_opcodes[OP_IF], e, NULL, &patch2));
			patch2->b = patch1 - patch2;
		}
		PR_Expect (")");
		PR_Expect (";");

		if (breaks != num_breaks)
		{
			for(i = breaks; i < num_breaks; i++)
			{
				patch2 = &statements[pr_breaks[i]];
				statements[pr_breaks[i]].a = &statements[numstatements] - patch2;
			}
			num_breaks = breaks;
		}
		if (continues != num_continues)
		{
			for(i = continues; i < num_continues; i++)
			{
				patch2 = &statements[pr_continues[i]];
				statements[pr_continues[i]].a = patch1 - patch2;
			}
			num_continues = continues;
		}
		return;
	}
	
	if (PR_CheckKeyword(keyword_local, "local"))
	{
		type_t *functionsclasstype = pr_classtype; 
		PR_ParseDefs( NULL );
		pr_classtype = functionsclasstype;
		locals_end = numpr_globals;
		return;
	}

	if (pr_token_type == tt_name)
	{
		bool result = false;

		if (keyword_var && !STRCMP ("var", pr_token)) result = true;
		else if (keyword_string && !STRCMP ("string", pr_token)) result = true;
		else if (keyword_float && !STRCMP ("float", pr_token)) result = true;
		else if (keyword_entity && !STRCMP ("entity", pr_token)) result = true;
		else if (keyword_vector && !STRCMP ("vector", pr_token)) result = true;
		else if (keyword_integer && !STRCMP ("integer", pr_token)) result = true;
		else if (keyword_int && !STRCMP ("int", pr_token)) result = true;
		else if (keyword_class && !STRCMP ("class", pr_token)) result = true; 
		else if (keyword_const && !STRCMP ("const", pr_token)) result = true;
		else result = false;

		if(result)
		{
			PR_ParseDefs (NULL);
			locals_end = numpr_globals;
			return;
		}
	}
	if (PR_CheckKeyword(keyword_state, "state"))
	{
		PR_Expect("[");
		PR_ParseState();
		PR_Expect(";");
		return;
	}
	if (PR_CheckToken("#"))
	{
		char	*name;
		float	frame = pr_immediate._float;
		PR_Lex();
		name = PR_ParseName();
		PR_FreeTemp(PR_Statement (&pr_opcodes[OP_STATE], PR_MakeFloatDef(frame), PR_GetDef(type_function, name, NULL, false, 0), NULL));
		PR_Expect(";");
		return;
	}
	
	if (PR_CheckKeyword(keyword_if, "if"))
	{
		PR_Expect ("(");
		conditional = 1;
		e = PR_Expression (TOP_PRIORITY, true);
		conditional = 0;

		if (!typecmp( e->type, type_string) && flag_ifstring)
		{
			PR_ParseWarning(WARN_IFSTRING_USED, "if (string) can result in bizzare behaviour");
			PR_FreeTemp(PR_Statement (&pr_opcodes[OP_IFNOTS], e, 0, &patch1));
		}
		else PR_FreeTemp(PR_Statement (&pr_opcodes[OP_IFNOT], e, 0, &patch1));

		PR_Expect (")"); // close bracket is after we save the statement to mem (so debugger does not show the if statement as being on the line after
		PR_ParseStatement ();

		if (PR_CheckKeyword (keyword_else, "else"))
		{
			int lastwasreturn;
			lastwasreturn = statements[numstatements-1].op == OP_RETURN || statements[numstatements-1].op == OP_DONE;

			// the last statement of the if was a return, so we don't need the goto at the end
			if (lastwasreturn && opt_compound_jumps && !PR_AStatementJumpsTo(numstatements, patch1-statements, numstatements))
			{
				patch1->b = &statements[numstatements] - patch1;
				PR_ParseStatement ();
			}
			else
			{
				PR_FreeTemp(PR_Statement (&pr_opcodes[OP_GOTO], 0, 0, &patch2));
				patch1->b = &statements[numstatements] - patch1;
				PR_ParseStatement ();
				patch2->a = &statements[numstatements] - patch2;

				if (PR_StatementBlocksMatch(patch1+1, patch2-patch1, patch2+1, &statements[numstatements] - patch2))
					PR_ParseWarning(0, "Two identical blocks each side of an else");
			}
		}
		else patch1->b = &statements[numstatements] - patch1;
		return;
	}
	if (PR_CheckKeyword(keyword_switch, "switch"))
	{
		int	op;
		int	hcstyle;
		int	defaultcase = -1;
		temp_t	*et;
		int	oldst;

		breaks = num_breaks;
		cases = num_cases;
		PR_Expect ("(");

		conditional = 1;
		e = PR_Expression (TOP_PRIORITY, true);
		conditional = 0;

		if (e == &def_ret) et = NULL;
		else
		{
			et = e->temp;
			e->temp = NULL; // so noone frees it until we finish this loop
		}

		/*
		expands from:

		switch (CONDITION)
		{
		case 1:
			break;
		case 2:
		default:
			break;
		}
		
		to:

		x = CONDITION, goto start
		l1:
			goto end
		l2:
		def:
			goto end
			goto end		P1
		start:
			if (x == 1) goto l1;
			if (x == 2) goto l2;
			goto def
		end:
		*/
		// x is emitted in an opcode, stored as a register that we cannot access later.
		// it should be possible to nest these.
		
		switch(e->type->type)
		{
		case ev_float:
			op = OP_SWITCH_F;
			break;
		case ev_entity: // whu???
			op = OP_SWITCH_E;
			break;
		case ev_vector:
			op = OP_SWITCH_V;
			break;
		case ev_string:
			op = OP_SWITCH_S;
			break;
		case ev_function:
			op = OP_SWITCH_FNC;
			break;
		default:	// err hmm.
			op = 0;
			break;
		}

		if (op) hcstyle = PR_OPCodeValid(&pr_opcodes[op]);
		else hcstyle = false;
		if (hcstyle) PR_FreeTemp(PR_Statement (&pr_opcodes[op], e, 0, &patch1));
		else PR_FreeTemp(PR_Statement (&pr_opcodes[OP_GOTO], e, 0, &patch1));

		PR_Expect (")"); // close bracket is after we save the statement to mem (so debugger does not show the if statement as being on the line after
		
		oldst = numstatements;
		PR_ParseStatement ();

		// this is so that a missing goto at the end of your switch doesn't end up in the jumptable again
		if (oldst == numstatements || !PR_StatementIsAJump(numstatements-1, numstatements-1))
		{
			PR_FreeTemp(PR_Statement (&pr_opcodes[OP_GOTO], 0, 0, &patch2)); // the P1 statement/the theyforgotthebreak statement.
		}
		else patch2 = NULL;
		if (hcstyle) patch1->b = &statements[numstatements] - patch1; // the goto start part
		else patch1->a = &statements[numstatements] - patch1; // the goto start part

		for (i = cases; i < num_cases; i++)
		{
			if (!pr_casesdef[i])
			{
				if (defaultcase >= 0) PR_ParseError(ERR_MULTIPLEDEFAULTS, "Duplicated default case");
				defaultcase = i;
			}
			else
			{
				if (pr_casesdef[i]->type->type != e->type->type)
				{
					if (e->type->type == ev_integer && pr_casesdef[i]->type->type == ev_float)
						pr_casesdef[i] = PR_MakeIntDef((int)pr_globals[pr_casesdef[i]->ofs]);
					else PR_ParseWarning(WARN_SWITCHTYPEMISMATCH, "switch case type mismatch");
				}
				if (pr_casesdef2[i])
				{
					if (pr_casesdef2[i]->type->type != e->type->type)
					{
						if (e->type->type == ev_integer && pr_casesdef[i]->type->type == ev_float)
							pr_casesdef2[i] = PR_MakeIntDef((int)pr_globals[pr_casesdef2[i]->ofs]);
						else PR_ParseWarning(WARN_SWITCHTYPEMISMATCH, "switch caserange type mismatch");
					}

					if (hcstyle)
					{
						PR_FreeTemp(PR_Statement (&pr_opcodes[OP_CASERANGE], pr_casesdef[i], pr_casesdef2[i], &patch3));
						patch3->c = &statements[pr_cases[i]] - patch3;
					}
					else
					{
						def_t	*e3;

						if (e->type->type == ev_float)
						{
							e2 = PR_Statement (&pr_opcodes[OP_GE], e, pr_casesdef[i], NULL);
							e3 = PR_Statement (&pr_opcodes[OP_LE], e, pr_casesdef2[i], NULL);
							e2 = PR_Statement (&pr_opcodes[OP_AND], e2, e3, NULL);
							PR_FreeTemp(PR_Statement (&pr_opcodes[OP_IF], e2, 0, &patch3));
							patch3->b = &statements[pr_cases[i]] - patch3;
						}
						else if (e->type->type == ev_integer)
						{
							e2 = PR_Statement (&pr_opcodes[OP_GE_I], e, pr_casesdef[i], NULL);
							e3 = PR_Statement (&pr_opcodes[OP_LE_I], e, pr_casesdef2[i], NULL);
							e2 = PR_Statement (&pr_opcodes[OP_AND], e2, e3, NULL);
							PR_FreeTemp(PR_Statement (&pr_opcodes[OP_IF], e2, 0, &patch3));
							patch3->b = &statements[pr_cases[i]] - patch3;
						}
						else PR_ParseWarning(WARN_SWITCHTYPEMISMATCH, "switch caserange MUST be a float or integer");
					}
				}
				else
				{
					if (hcstyle)
					{
						PR_FreeTemp(PR_Statement (&pr_opcodes[OP_CASE], pr_casesdef[i], 0, &patch3));
						patch3->b = &statements[pr_cases[i]] - patch3;
					}
					else
					{
						if (!pr_casesdef[i]->constant || G_INT(pr_casesdef[i]->ofs))
						{
							switch(e->type->type)
							{
							case ev_float:
								e2 = PR_Statement(&pr_opcodes[OP_EQ_F], e, pr_casesdef[i], NULL);
								break;
							case ev_entity: // whu???
								e2 = PR_Statement(&pr_opcodes[OP_EQ_E], e, pr_casesdef[i], &patch1);
								break;
							case ev_vector:
								e2 = PR_Statement(&pr_opcodes[OP_EQ_V], e, pr_casesdef[i], &patch1);
								break;
							case ev_string:
								e2 = PR_Statement(&pr_opcodes[OP_EQ_S], e, pr_casesdef[i], &patch1);
								break;
							case ev_function:
								e2 = PR_Statement(&pr_opcodes[OP_EQ_FNC], e, pr_casesdef[i], &patch1);
								break;
							case ev_field:
								e2 = PR_Statement(&pr_opcodes[OP_EQ_FNC], e, pr_casesdef[i], &patch1);
								break;
							case ev_integer:
								e2 = PR_Statement(&pr_opcodes[OP_EQ_I], e, pr_casesdef[i], &patch1);
								break;
							default:
								PR_ParseError(ERR_BADSWITCHTYPE, "Bad switch type");
								e2 = NULL;
								break;
							}
							PR_FreeTemp(PR_Statement (&pr_opcodes[OP_IF], e2, 0, &patch3));
						}
						else
						{
							if (e->type->type == ev_string)
								PR_FreeTemp(PR_Statement (&pr_opcodes[OP_IFNOTS], e, 0, &patch3));
							else PR_FreeTemp(PR_Statement (&pr_opcodes[OP_IFNOT], e, 0, &patch3));
						}
						patch3->b = &statements[pr_cases[i]] - patch3;
					}
				}
			}	
		}
		if (defaultcase>=0)
		{
			PR_FreeTemp(PR_Statement (&pr_opcodes[OP_GOTO], 0, 0, &patch3));
			patch3->a = &statements[pr_cases[defaultcase]] - patch3;
		}

		num_cases = cases;
		patch3 = &statements[numstatements];
		if (patch2) patch2->a = patch3 - patch2; // set P1 jump

		if (breaks != num_breaks)
		{
			for(i = breaks; i < num_breaks; i++)
			{
				patch2 = &statements[pr_breaks[i]];
				patch2->a = patch3 - patch2;
			}
			num_breaks = breaks;
		}

		if (et)
		{
			e->temp = et;
			PR_FreeTemp(e);
		}
		return;
	}
	if (PR_CheckKeyword(keyword_asm, "asm") || PR_CheckKeyword(keyword_asm, "_asm"))
	{
		if (PR_CheckToken("{"))
		{
			while (!PR_CheckToken("}"))
				PR_ParseAsm ();
		}
		else PR_ParseAsm();
		return;
	}
	if (PR_CheckToken(":"))
	{
		if (pr_token_type != tt_name)
		{
			PR_ParseError(ERR_BADLABELNAME, "invalid label name \"%s\"", pr_token);
			return;
		}

		for (i = 0; i < num_labels; i++)
		{
			if (!STRNCMP(pr_labels[i].name, pr_token, sizeof(pr_labels[num_labels].name) -1))
			{
				PR_ParseWarning(WARN_DUPLICATELABEL, "Duplicate label %s", pr_token);
				PR_Lex();
				return;
			}
		}
		if (num_labels >= max_labels)
		{
			max_labels += 8;
			pr_labels = Qrealloc(pr_labels, sizeof(*pr_labels)*max_labels);
		}

		strncpy(pr_labels[num_labels].name, pr_token, sizeof(pr_labels[num_labels].name) -1);
		pr_labels[num_labels].lineno = pr_source_line;
		pr_labels[num_labels].statementno = numstatements;
		num_labels++;

		PR_Lex();
		return;
	}
	if (PR_CheckKeyword(keyword_goto, "goto"))
	{
		if (pr_token_type != tt_name)
		{
			PR_ParseError(ERR_NOLABEL, "invalid label name \"%s\"", pr_token);
			return;
		}

		PR_Statement (&pr_opcodes[OP_GOTO], 0, 0, &patch2);
		PR_GotoStatement (patch2, pr_token);
		PR_Lex();
		PR_Expect(";");
		return;
	}
	if (PR_CheckKeyword(keyword_break, "break"))
	{
		if (!STRCMP ("(", pr_token))
		{	
			// make sure it wasn't a call to the break function.
			PR_IncludeChunk("break(", true, NULL);
			PR_Lex();	//so it sees the break.
		}
		else
		{
			if (num_breaks >= max_breaks)
			{
				max_breaks += 8;
				pr_breaks = Qrealloc(pr_breaks, sizeof(*pr_breaks) * max_breaks);
			}
			pr_breaks[num_breaks] = numstatements;
			PR_Statement (&pr_opcodes[OP_GOTO], 0, 0, NULL);
			num_breaks++;
			PR_Expect(";");
			return;
		}
	}
	if (PR_CheckKeyword(keyword_continue, "continue"))
	{
		if (num_continues >= max_continues)
		{
			max_continues += 8;
			pr_continues = Qrealloc(pr_continues, sizeof(*pr_continues)*max_continues);
		}
		pr_continues[num_continues] = numstatements;
		PR_Statement (&pr_opcodes[OP_GOTO], 0, 0, NULL);
		num_continues++;
		PR_Expect(";");
		return;
	}
	if (PR_CheckKeyword(keyword_case, "case"))
	{
		if (num_cases >= max_cases)
		{
			max_cases += 8;
			pr_cases = Qrealloc(pr_cases, sizeof(*pr_cases)*max_cases);
			pr_casesdef = Qrealloc(pr_casesdef, sizeof(*pr_casesdef)*max_cases);
			pr_casesdef2 = Qrealloc(pr_casesdef2, sizeof(*pr_casesdef2)*max_cases);
		}
		pr_cases[num_cases] = numstatements;
		pr_casesdef[num_cases] = PR_Expression (TOP_PRIORITY, false);
		if (PR_CheckToken(".."))
		{
			pr_casesdef2[num_cases] = PR_Expression (TOP_PRIORITY, false);
			if (pr_casesdef[num_cases]->constant && pr_casesdef2[num_cases]->constant && !pr_casesdef[num_cases]->temp && !pr_casesdef2[num_cases]->temp)
			{
				if (G_FLOAT(pr_casesdef[num_cases]->ofs) >= G_FLOAT(pr_casesdef2[num_cases]->ofs))
					PR_ParseError(ERR_CASENOTIMMEDIATE, "Caserange statement uses backwards range\n");
			}
		}
		else pr_casesdef2[num_cases] = NULL;

		if (numstatements != pr_cases[num_cases]) PR_ParseError(ERR_CASENOTIMMEDIATE, "Case statements may not use formulas\n");
		num_cases++;
		PR_Expect(":");
		return;
	}
	if (PR_CheckKeyword(keyword_default, "default"))
	{
		if (num_cases >= max_cases)
		{
			max_cases += 8;
			pr_cases = Qrealloc(pr_cases, sizeof(*pr_cases)*max_cases);
			pr_casesdef = Qrealloc(pr_casesdef, sizeof(*pr_casesdef)*max_cases);
			pr_casesdef2 = Qrealloc(pr_casesdef2, sizeof(*pr_casesdef2)*max_cases);
		}
		pr_cases[num_cases] = numstatements;
		pr_casesdef[num_cases] = NULL;
		pr_casesdef2[num_cases] = NULL;
		num_cases++;
		PR_Expect(":");
		return;
	}
	if (PR_CheckToken(";"))
	{
		int osl = pr_source_line;
		pr_source_line = statementstart;
		PR_ParseWarning(WARN_POINTLESSSTATEMENT, "Hanging ';'");
		pr_source_line = osl;
		return;
	}

	qcc_usefulstatement = false;
	e = PR_Expression (TOP_PRIORITY, true);
	PR_Expect (";");

	if (e->type->type != ev_void && !qcc_usefulstatement)
	{
		int osl = pr_source_line;
		pr_source_line = statementstart;
		PR_ParseWarning(WARN_POINTLESSSTATEMENT, "Effectless statement");
		pr_source_line = osl;
	}
	PR_FreeTemp(e);
}


/*
==============
PR_ParseState

States are special functions made for convenience.  They automatically
set frame, nextthink (implicitly), and think (allowing forward definitions).

// void() name = [framenum, nextthink] { code }
// expands to:
// function void name ()
// {
//		pev.frame = framenum;
//		pev.nextthink = time + 0.1;
//		pev.think = nextthink
//		<code>
// };
==============
*/
void PR_ParseState (void)
{
	char	*name;
	def_t	*s1, *def, *sc = pr_scope;
	char	f;

	f = *pr_token;

	if (PR_CheckToken("++") || PR_CheckToken("--"))
	{
		s1 = PR_ParseImmediate ();
		PR_Expect("..");
		def = PR_ParseImmediate ();
		PR_Expect ("]");

		if (s1->type->type != ev_float || def->type->type != ev_float)
			PR_ParseError(ERR_STATETYPEMISMATCH, "state type mismatch");

		if (PR_OPCodeValid(&pr_opcodes[OP_CSTATE]))
		{
			PR_FreeTemp(PR_Statement (&pr_opcodes[OP_CSTATE], s1, def, NULL));
		}
		else
		{
			def_t *t1, *t2;
			def_t *framef, *frame;
			def_t *pev;
			def_t *cycle_wrapped;
			temp_t *ftemp;

			pev = PR_GetDef(type_entity, pevname, NULL, false, 0);
			framef = PR_GetDef(NULL, "frame", NULL, false, 0);
			cycle_wrapped = PR_GetDef(type_float, "cycle_wrapped", NULL, false, 0);

			frame = PR_Statement(&pr_opcodes[OP_LOAD_F], pev, framef, NULL);
			if (cycle_wrapped) PR_FreeTemp(PR_Statement(&pr_opcodes[OP_STORE_F], PR_MakeFloatDef(0), cycle_wrapped, NULL));
			PR_UnFreeTemp(frame);

			// make sure the frame is within the bounds given.
			ftemp = frame->temp;
			frame->temp = NULL;
			t1 = PR_Statement(&pr_opcodes[OP_LT], frame, s1, NULL);
			t2 = PR_Statement(&pr_opcodes[OP_GT], frame, def, NULL);
			t1 = PR_Statement(&pr_opcodes[OP_OR], t1, t2, NULL);
			PR_SimpleStatement(OP_IFNOT, t1->ofs, 2, 0, false);
			PR_FreeTemp(t1); PR_FreeTemp(PR_Statement(&pr_opcodes[OP_STORE_F], s1, frame, NULL));
			PR_SimpleStatement(OP_GOTO, t1->ofs, 13, 0, false);

			t1 = PR_Statement(&pr_opcodes[OP_GE], def, s1, NULL);
			PR_SimpleStatement(OP_IFNOT, t1->ofs, 7, 0, false);
			PR_FreeTemp(t1); // this block is the 'it's in a forwards direction'
			PR_SimpleStatement(OP_ADD_F, frame->ofs, PR_MakeFloatDef(1)->ofs, frame->ofs, false);
			t1 = PR_Statement(&pr_opcodes[OP_GT], frame, def, NULL);
			PR_SimpleStatement(OP_IFNOT, t1->ofs,2, 0, false);
			PR_FreeTemp(t1);
			PR_FreeTemp(PR_Statement(&pr_opcodes[OP_STORE_F], s1, frame, NULL));
			PR_UnFreeTemp(frame);
			if (cycle_wrapped) PR_FreeTemp(PR_Statement(&pr_opcodes[OP_STORE_F], PR_MakeFloatDef(1), cycle_wrapped, NULL));

			PR_SimpleStatement(OP_GOTO, 6, 0, 0, false);
			// reverse animation.
			PR_SimpleStatement(OP_SUB_F, frame->ofs, PR_MakeFloatDef(1)->ofs, frame->ofs, false);
			t1 = PR_Statement(&pr_opcodes[OP_LT], frame, s1, NULL);
			PR_SimpleStatement(OP_IFNOT, t1->ofs,2, 0, false);
			PR_FreeTemp(t1);
			PR_FreeTemp(PR_Statement(&pr_opcodes[OP_STORE_F], def, frame, NULL));
			PR_UnFreeTemp(frame);
			if (cycle_wrapped) PR_FreeTemp(PR_Statement(&pr_opcodes[OP_STORE_F], PR_MakeFloatDef(1), cycle_wrapped, NULL));
	
			// pev.frame = frame happens with the normal state opcode.
			PR_FreeTemp(PR_Statement (&pr_opcodes[OP_STATE], frame, pr_scope, NULL));

			frame->temp = ftemp;
			PR_FreeTemp(frame);
		}
		return;
	}
	
	if (pr_token_type != tt_immediate || pr_immediate_type != type_float)
		PR_ParseError (ERR_STATETYPEMISMATCH, "state frame must be a number");
	s1 = PR_ParseImmediate ();
	PR_CheckToken (",");

	name = PR_ParseName ();
	pr_scope = NULL;
	def = PR_GetDef (type_function, name, NULL, true, 1);
	pr_scope = sc;
		
	PR_Expect ("]");
	PR_FreeTemp(PR_Statement (&pr_opcodes[OP_STATE], s1, def, NULL));
}

void PR_ParseAsm(void)
{
	dstatement_t *patch1;
	int op, p;
	def_t *a, *b, *c;

	if (PR_CheckKeyword(keyword_local, "local"))
	{
		PR_ParseDefs (NULL);
		locals_end = numpr_globals;
		return;
	}

	for (op = 0; op < OP_NUMOPS; op++)
	{
		if (!STRCMP(pr_token, pr_opcodes[op].opname))
		{
			PR_Lex();
			if (pr_opcodes[op].priority == -1 && pr_opcodes[op].associative != ASSOC_LEFT)
			{
				if (pr_opcodes[op].type_a==NULL)
				{
					patch1 = &statements[numstatements];
					PR_Statement3(&pr_opcodes[op], NULL, NULL, NULL, true);

					if (pr_token_type == tt_name)
					{
						PR_GotoStatement(patch1, PR_ParseName());
					}
					else
					{
						p = (int)pr_immediate._float;
						patch1->a = (int)p;
					}
					PR_Lex();
				}
				else if (pr_opcodes[op].type_b==NULL)
				{
					patch1 = &statements[numstatements];
					a = PR_ParseValue(pr_classtype);
					PR_Statement3(&pr_opcodes[op], a, NULL, NULL, true);

					if (pr_token_type == tt_name)
					{
						PR_GotoStatement(patch1, PR_ParseName());
					}
					else
					{
						p = (int)pr_immediate._float;
						patch1->b = (int)p;
					}
					PR_Lex();
				}
				else
				{
					patch1 = &statements[numstatements];
					a = PR_ParseValue(pr_classtype);
					b = PR_ParseValue(pr_classtype);
					PR_Statement3(&pr_opcodes[op], a, b, NULL, true);

					if (pr_token_type == tt_name)
					{
						PR_GotoStatement(patch1, PR_ParseName());
					}
					else
					{
						p = (int)pr_immediate._float;
						patch1->c = (int)p;
					}
					PR_Lex();
				}
			}
			else
			{				
				if (pr_opcodes[op].type_a != &type_void)
					a = PR_ParseValue(pr_classtype);
				else a = NULL;
				if (pr_opcodes[op].type_b != &type_void)
					b = PR_ParseValue(pr_classtype);
				else b = NULL;
				if (pr_opcodes[op].associative==ASSOC_LEFT && pr_opcodes[op].type_c != &type_void)
					c = PR_ParseValue(pr_classtype);
				else c = NULL;
				PR_Statement3(&pr_opcodes[op], a, b, c, true);
			}
			PR_Expect(";");
			return;
		}
	}
	PR_ParseError(ERR_BADOPCODE, "Bad op code name %s", pr_token);
}

bool PR_FuncJumpsTo(int first, int last, int statement)
{
	int	st;

	for (st = first; st < last; st++)
	{
		if (pr_opcodes[statements[st].op].type_a == NULL)
		{
			if (st + (signed)statements[st].a == statement)
			{
				if (st != first)
				{
					if (statements[st-1].op == OP_RETURN) continue;
					if (statements[st-1].op == OP_DONE) continue;
					return true;
				}
			}
		}
		if (pr_opcodes[statements[st].op].type_b == NULL)
		{
			if (st + (signed)statements[st].b == statement)
			{
				if (st != first)
				{
					if (statements[st-1].op == OP_RETURN) continue;
					if (statements[st-1].op == OP_DONE) continue;
					return true;
				}
			}
		}
		if (pr_opcodes[statements[st].op].type_c == NULL)
		{
			if (st + (signed)statements[st].c == statement)
			{
				if (st != first)
				{
					if (statements[st-1].op == OP_RETURN) continue;
					if (statements[st-1].op == OP_DONE) continue;
					return true;
				}
			}
		}
	}
	return false;
}

bool PR_FuncJumpsToRange(int first, int last, int firstr, int lastr)
{
	int	st;

	for (st = first; st < last; st++)
	{
		if (pr_opcodes[statements[st].op].type_a == NULL)
		{
			if (st + (signed)statements[st].a >= firstr && st + (signed)statements[st].a <= lastr)
			{
				if (st != first)
				{
					if (statements[st-1].op == OP_RETURN) continue;
					if (statements[st-1].op == OP_DONE) continue;
					return true;
				}
			}
		}
		if (pr_opcodes[statements[st].op].type_b == NULL)
		{
			if (st + (signed)statements[st].b >= firstr && st + (signed)statements[st].b <= lastr)
			{
				if (st != first)
				{
					if (statements[st-1].op == OP_RETURN) continue;
					if (statements[st-1].op == OP_DONE) continue;
					return true;
				}
			}
		}
		if (pr_opcodes[statements[st].op].type_c == NULL)
		{
			if (st + (signed)statements[st].c >= firstr && st + (signed)statements[st].c <= lastr)
			{
				if (st != first)
				{
					if (statements[st-1].op == OP_RETURN) continue;
					if (statements[st-1].op == OP_DONE) continue;
					return true;
				}
			}
		}
	}
	return false;
}

/*
==========================
PR_CompoundJumps

jumps to jumps are reordered so they become jumps to the final target.
==========================
*/
void PR_CompoundJumps(int first, int last)
{
	int	statement;
	int	st;
	int	infloop;

	for (st = first; st < last; st++)
	{
		if (pr_opcodes[statements[st].op].type_a == NULL)
		{
			statement = st + (signed)statements[st].a;
			if (statements[statement].op == OP_RETURN || statements[statement].op == OP_DONE)
			{
				// goto leads to return. Copy the command out to remove the goto.
				statements[st].op = statements[statement].op;
				statements[st].a = statements[statement].a;
				statements[st].b = statements[statement].b;
				statements[st].c = statements[statement].c;
			}
			infloop = 1000;
			while (statements[statement].op == OP_GOTO)
			{
				if (!infloop--)
				{
					PR_ParseWarning(0, "Infinate loop detected");
					break;
				}
				statements[st].a = (statement+statements[statement].a - st);
				statement = st + (signed)statements[st].a;
			}
		}
		if (pr_opcodes[statements[st].op].type_b == NULL)
		{
			statement = st + (signed)statements[st].b;
			infloop = 1000;
			while (statements[statement].op == OP_GOTO)
			{
				if (!infloop--)
				{
					PR_ParseWarning(0, "Infinate loop detected");
					break;
				}
				statements[st].b = (statement+statements[statement].a - st);
				statement = st + (signed)statements[st].b;
			}
		}
		if (pr_opcodes[statements[st].op].type_c == NULL)
		{
			statement = st + (signed)statements[st].c;
			infloop = 1000;
			while (statements[statement].op == OP_GOTO)
			{
				if (!infloop--)
				{
					PR_ParseWarning(0, "Infinate loop detected");
					break;
				}
				statements[st].c = (statement+statements[statement].a - st);
				statement = st + (signed)statements[st].c;
			}
		}
	}
}

void PR_CheckForDeadAndMissingReturns(int first, int last, int rettype)
{
	int	st, st2;

	if (statements[last-1].op == OP_DONE) last--; // don't want the done
	
	if (rettype != ev_void)
	{
		if (statements[last-1].op != OP_RETURN)
		{
			if (statements[last-1].op != OP_GOTO || (signed)statements[last-1].a > 0)
			{
				PR_ParseWarning(WARN_MISSINGRETURN, "%s: not all control paths return a value", pr_scope->name );
				return;
			}
		}
	}

	for (st = first; st < last; st++)
	{
		if (statements[st].op == OP_RETURN || statements[st].op == OP_GOTO)
		{
			st++;
			if (st == last) continue; // erm... end of function doesn't count as unreachable.

			if (!opt_compound_jumps)
			{	
				// we can ignore single statements like these without compound jumps (compound jumps correctly removes all).
				if (statements[st].op == OP_GOTO) continue; // inefficient compiler, we can ignore this.
				if (statements[st].op == OP_DONE) continue; // inefficient compiler, we can ignore this.
				if (statements[st].op == OP_RETURN) continue;// inefficient compiler, we can ignore this.
			}

			//make sure something goes to just after this return.
			for (st2 = first; st2 < last; st2++)
			{
				if (pr_opcodes[statements[st2].op].type_a == NULL)
				{
					if (st2 + (signed)statements[st2].a == st)
						break;
				}
				if (pr_opcodes[statements[st2].op].type_b == NULL)
				{
					if (st2 + (signed)statements[st2].b == st)
						break;
				}
				if (pr_opcodes[statements[st2].op].type_c == NULL)
				{
					if (st2 + (signed)statements[st2].c == st)
						break;
				}
			}
			if (st2 == last) PR_ParseWarning(WARN_UNREACHABLECODE, "%s: contains unreachable code", pr_scope->name );
			continue;
		}
		if (rettype != ev_void)
		{
			if (pr_opcodes[statements[st].op].type_a == NULL)
			{
				if (st + (signed)statements[st].a == last)
				{
					PR_ParseWarning(WARN_MISSINGRETURN, "%s: not all control paths return a value", pr_scope->name );
					return;
				}
			}
			if (pr_opcodes[statements[st].op].type_b == NULL)
			{
				if (st + (signed)statements[st].b == last)
				{
					PR_ParseWarning(WARN_MISSINGRETURN, "%s: not all control paths return a value", pr_scope->name );
					return;
				}
			}
			if (pr_opcodes[statements[st].op].type_c == NULL)
			{
				if (st + (signed)statements[st].c == last)
				{
					PR_ParseWarning(WARN_MISSINGRETURN, "%s: not all control paths return a value", pr_scope->name );
					return;
				}
			}
		}
	}
}

bool PR_StatementIsAJump(int stnum, int notifdest)
{
	if (statements[stnum].op == OP_RETURN) return true;
	if (statements[stnum].op == OP_DONE) return true;
	if (statements[stnum].op == OP_GOTO)
	{
		if ((int)statements[stnum].a != notifdest)
			return true;
	}
	return false;
}

int PR_AStatementJumpsTo(int targ, int first, int last)
{
	int st;

	for (st = first; st < last; st++)
	{
		if (pr_opcodes[statements[st].op].type_a == NULL)
		{
			if (st + (signed)statements[st].a == targ && statements[st].a)
				return true;
		}
		if (pr_opcodes[statements[st].op].type_b == NULL)
		{
			if (st + (signed)statements[st].b == targ)
				return true;
		}
		if (pr_opcodes[statements[st].op].type_c == NULL)
		{
			if (st + (signed)statements[st].c == targ)
				return true;
		}
	}

	for (st = 0; st < num_labels; st++) // assume it's used.
	{
		if (pr_labels[st].statementno == targ)
			return true;
	}
	return false;
}

void PR_RemapOffsets(uint firststatement, uint laststatement, uint min, uint max, uint newmin)
{
	dstatement_t *st;
	uint i;

	for (i = firststatement, st = &statements[i]; i < laststatement; i++, st++)
	{
		if (pr_opcodes[st->op].type_a && st->a >= min && st->a < max)
			st->a = st->a - min + newmin;
		if (pr_opcodes[st->op].type_b && st->b >= min && st->b < max)
			st->b = st->b - min + newmin;
		if (pr_opcodes[st->op].type_c && st->c >= min && st->c < max)
			st->c = st->c - min + newmin;
	}
}

void PR_Marshal_Locals(int first, int laststatement)
{
	def_t	*local;
	uint	newofs;

	if (!pr.localvars)	//nothing to marshal
	{
		locals_start = numpr_globals;
		locals_end = numpr_globals;
		return;
	}

	if (!opt_locals_marshalling)
	{
		pr.localvars = NULL;
		return;
	}

	// initial backwards bounds.
	locals_start = MAX_REGS;
	locals_end = 0;
	newofs = MAX_REGS;	// this is a handy place to put it. :)

	// the params need to be in the order that they were allocated
	// so we allocate in a backwards order.
	for (local = pr.localvars; local; local = local->nextlocal)
	{
		if (local->constant) continue;

		newofs += local->type->size*local->arraysize;
		if (local->arraysize > 1) newofs++;
	}

	locals_start = MAX_REGS;
	locals_end = newofs;

	for (local = pr.localvars; local; local = local->nextlocal)
	{
		if (local->constant) continue;

		if (((int*)pr_globals)[local->ofs]) PR_ParseError(ERR_INTERNAL, "Marshall of a set value");
		newofs -= local->type->size*local->arraysize;
		if (local->arraysize > 1) newofs--;

		PR_RemapOffsets(first, laststatement, local->ofs, local->ofs+local->type->size*local->arraysize, newofs);
		PR_FreeOffset(local->ofs, local->type->size*local->arraysize);
		local->ofs = newofs;
	}
	pr.localvars = NULL;
}

void PR_WriteAsmFunction(def_t *sc, uint firststatement, gofs_t firstparm)
{
	uint	i;
	uint	p;
	gofs_t	o;
	type_t	*type;
	def_t	*param;

	if (!asmfile) return;

	type = sc->type;
	FS_Printf(asmfile, "%s(", TypeName(type->aux_type));
	p = type->num_parms;
	for (o = firstparm, i = 0, type = type->param; i < p; i++, type = type->next)
	{
		if (i) FS_Printf(asmfile, ", ");
		for (param = pr.localvars; param; param = param->nextlocal)
		{
			if (param->ofs == o) break;
		}
		if (param) FS_Printf(asmfile, "%s %s", TypeName(type), param->name);
		else FS_Printf(asmfile, "%s", TypeName(type));
		o += type->size;
	}

	FS_Printf(asmfile, ") %s = asm\n{\n", sc->name);
	PR_fprintfLocals(asmfile, firstparm, o);

	for (i = firststatement; i < (uint)numstatements; i++)
	{
		FS_Printf(asmfile, "\t%s", pr_opcodes[statements[i].op].opname);
		if (pr_opcodes[statements[i].op].type_a != &type_void)
		{
			if (strlen(pr_opcodes[statements[i].op].opname)<6)
				FS_Printf(asmfile, "\t");
			if (pr_opcodes[statements[i].op].type_a)
				FS_Printf(asmfile, "\t%s", PR_VarAtOffset(statements[i].a, (*pr_opcodes[statements[i].op].type_a)->size));
			else FS_Printf(asmfile, "\t%i", statements[i].a);

			if (pr_opcodes[statements[i].op].type_b != &type_void)
			{
				if (pr_opcodes[statements[i].op].type_b)
					FS_Printf(asmfile, ",\t%s", PR_VarAtOffset(statements[i].b, (*pr_opcodes[statements[i].op].type_b)->size));
				else FS_Printf(asmfile, ",\t%i", statements[i].b);
				if (pr_opcodes[statements[i].op].type_c != &type_void && pr_opcodes[statements[i].op].associative==ASSOC_LEFT)
				{
					if (pr_opcodes[statements[i].op].type_c)
						FS_Printf(asmfile, ",\t%s", PR_VarAtOffset(statements[i].c, (*pr_opcodes[statements[i].op].type_c)->size));
					else FS_Printf(asmfile, ",\t%i", statements[i].c);
				}
			}
			else
			{
				if (pr_opcodes[statements[i].op].type_c != &type_void)
				{
					if (pr_opcodes[statements[i].op].type_c)
						FS_Printf(asmfile, ",\t%s", PR_VarAtOffset(statements[i].c, (*pr_opcodes[statements[i].op].type_c)->size));
					else FS_Printf(asmfile, ",\t%i", statements[i].c);
				}
			}
		}
		FS_Printf(asmfile, ";\n");
	}
	FS_Printf(asmfile, "}\n\n");
}

/*
============
PR_ParseImmediateStatements

Parse a function body
============
*/
function_t *PR_ParseImmediateStatements (type_t *type)
{
	int		i;
	function_t	*f;
	def_t		*defs[MAX_PARMS+MAX_PARMS_EXTRA], *e2;
	type_t		*parm;
	bool		needsdone = false;
	freeoffset_t	*oldfofs;

	conditional = 0;
	f = (void *)Qalloc (sizeof(function_t));

	// check for builtin function definition #1, #2, etc
	if (PR_CheckToken ("#"))
	{
		if (pr_token_type != tt_immediate || pr_immediate_type != type_float || pr_immediate._float != (int)pr_immediate._float)
			PR_ParseError (ERR_BADBUILTINIMMEDIATE, "Bad builtin immediate");
		f->builtin = (int)pr_immediate._float;
		PR_Lex ();
		locals_start = locals_end = OFS_PARM0; // hmm...
		return f;
	}

	if (type->num_parms < 0) PR_ParseError (ERR_FUNCTIONWITHVARGS, "QC function with variable arguments and function body");
	f->builtin = 0;

	// define the parms
	locals_start = locals_end = numpr_globals;
	oldfofs = freeofs;
	freeofs = NULL;
	parm = type->param;

	for (i = 0; i < type->num_parms; i++)
	{
		if (!*pr_parm_names[i]) PR_ParseError(ERR_PARAMWITHNONAME, "Parameter is not named");
		defs[i] = PR_GetDef (parm, pr_parm_names[i], pr_scope, true, 1);

		defs[i]->references++;
		if (i < MAX_PARMS)
		{
			f->parm_ofs[i] = defs[i]->ofs;
			if (i > 0 && f->parm_ofs[i] < f->parm_ofs[i-1]) Sys_Error ("bad parm order");
			if (i > 0 && f->parm_ofs[i] != f->parm_ofs[i-1]+defs[i-1]->type->size) 
				Sys_Error ("parms not packed");
		}
		parm = parm->next;
	}

	if (type->num_parms) locals_start = locals_end = defs[0]->ofs;
	freeofs = oldfofs;
	f->code = numstatements;

	if (type->num_parms > MAX_PARMS)
	{
		for (i = MAX_PARMS; i < type->num_parms; i++)
		{
			if (!extra_parms[i - MAX_PARMS])
			{
				e2 = (def_t *) Qalloc (sizeof(def_t));
				e2->name = "extra parm";
				e2->ofs = PR_GetFreeOffsetSpace(3);
				extra_parms[i - MAX_PARMS] = e2;
			}
			extra_parms[i - MAX_PARMS]->type = defs[i]->type;
			if (defs[i]->type->type != ev_vector)
				PR_Statement (&pr_opcodes[OP_STORE_F], extra_parms[i - MAX_PARMS], defs[i], NULL);
			else PR_Statement (&pr_opcodes[OP_STORE_V], extra_parms[i - MAX_PARMS], defs[i], NULL);
		}
	}
	PR_RemapLockedTemps(-1, -1);

	// check for a state opcode
	if (PR_CheckToken ("[")) PR_ParseState ();

	if (PR_CheckKeyword (keyword_asm, "asm") || PR_CheckKeyword (keyword_asm, "_asm"))
	{
		PR_Expect ("{");
		while (!PR_CheckToken("}")) PR_ParseAsm ();
	}
	else
	{
		PR_Expect ("{");
		// parse regular statements
		while (!PR_CheckToken("}"))
		{
			PR_ParseStatement ();
			PR_FreeTemps();
		}
	}
	PR_FreeTemps();

	if (f->code == numstatements) needsdone = true;
	else if (statements[numstatements - 1].op != OP_RETURN && statements[numstatements - 1].op != OP_DONE)
		needsdone = true;

	if (num_gotos)
	{
		int	j;

		for (i = 0; i < num_gotos; i++)
		{
			for (j = 0; j < num_labels; j++)
			{
				if (!strcmp(pr_gotos[i].name, pr_labels[j].name))
				{
					if (!pr_opcodes[statements[pr_gotos[i].statementno].op].type_a)
						statements[pr_gotos[i].statementno].a += pr_labels[j].statementno - pr_gotos[i].statementno;
					else if (!pr_opcodes[statements[pr_gotos[i].statementno].op].type_b)
						statements[pr_gotos[i].statementno].b += pr_labels[j].statementno - pr_gotos[i].statementno;
					else statements[pr_gotos[i].statementno].c += pr_labels[j].statementno - pr_gotos[i].statementno;
					break;
				}
			}
			if (j == num_labels)
			{
				num_gotos = 0;
				PR_ParseError(ERR_NOLABEL, "Goto statement with no matching label \"%s\"", pr_gotos[i].name);
			}
		}
		num_gotos = 0;
	}

	if (opt_return_only && !needsdone) needsdone = PR_FuncJumpsTo(f->code, numstatements, numstatements);

	// emit an end of statements opcode
	if (!opt_return_only || needsdone) PR_Statement (pr_opcodes, 0,0, NULL);

	PR_CheckForDeadAndMissingReturns(f->code, numstatements, type->aux_type->type);
	if (opt_compound_jumps) PR_CompoundJumps(f->code, numstatements);
	PR_RemapLockedTemps(f->code, numstatements);
	locals_end = numpr_globals;
	PR_WriteAsmFunction(pr_scope, f->code, locals_start);
	PR_Marshal_Locals(f->code, numstatements);
	if (num_labels) num_labels = 0;

	if (num_continues)
	{
		num_continues = 0;
		PR_ParseError(ERR_ILLEGALCONTINUES, "%s: function contains illegal continues\n", pr_scope->name);
	}
	if (num_breaks)
	{
		num_breaks = 0;
		PR_ParseError(ERR_ILLEGALBREAKS, "%s: function contains illegal breaks\n", pr_scope->name);
	}
	if (num_cases)
	{
		num_cases = 0;
		PR_ParseError(ERR_ILLEGALCASES, "%s: function contains illegal cases\n", pr_scope->name);
	}
	return f;
}

void PR_ArrayRecurseDivideRegular(def_t *array, def_t *index, int min, int max, bool usingvectors)
{
	dstatement_t	*st;
	def_t		*eq;

	if (min == max || min+1 == max)
	{
		eq = PR_Statement(pr_opcodes+OP_LT, index, PR_MakeFloatDef(min+0.5f), NULL);
		PR_UnFreeTemp(index);
		PR_FreeTemp(PR_Statement(pr_opcodes+OP_IFNOT, eq, 0, &st));
		st->b = 2;
		PR_Statement(pr_opcodes+OP_RETURN, 0, 0, &st);
		if(usingvectors) st->a = array->ofs + min * 3;
		else st->a = array->ofs + min * array->type->size;
	}
	else
	{
		int mid = min + (max-min)/2;

		if (max-min > 4)
		{
			eq = PR_Statement(pr_opcodes+OP_LT, index, PR_MakeFloatDef(mid+0.5f), NULL);
			PR_UnFreeTemp(index);
			PR_FreeTemp(PR_Statement(pr_opcodes+OP_IFNOT, eq, 0, &st));
		}
		else st = NULL;

		PR_ArrayRecurseDivideRegular(array, index, min, mid, usingvectors );
		if (st) st->b = numstatements - (st-statements);
		PR_ArrayRecurseDivideRegular(array, index, mid, max, usingvectors );
	}
}

/*
=======================
PR_EmitArrayGetVector

returns a vector overlapping the result needed.
=======================
*/
def_t *PR_EmitArrayGetVector(def_t *array)
{
	dfunction_t	*df;
	def_t		*temp, *index, *func;

	func = PR_GetDef(type_function, va("ArrayGetVec*%s", array->name), NULL, true, 1);
	pr_scope = func;
	df = &functions[numfunctions];
	numfunctions++;

	df->s_file = 0;
	df->s_name = PR_CopyString(func->name, opt_noduplicatestrings );
	df->first_statement = numstatements;
	df->parm_size[0] = 1;
	df->numparms = 1;
	df->parm_start = numpr_globals;
	index = PR_GetDef(type_float, "index___", func, true, 1);
	index->references++;
	temp = PR_GetDef(type_float, "div3___", func, true, 1);
	locals_end = numpr_globals;
	df->locals = locals_end - df->parm_start;
	PR_Statement3(pr_opcodes+OP_DIV_F, index, PR_MakeFloatDef(3), temp, false);
	PR_Statement3(pr_opcodes+OP_BITAND, temp, temp, temp, false); // round down to int
	PR_ArrayRecurseDivideRegular(array, temp, 0, (array->arraysize + 2)/3, true); // round up

	PR_Statement(pr_opcodes+OP_RETURN, PR_MakeFloatDef(0), 0, NULL); // err... we didn't find it, give up.
	PR_Statement(pr_opcodes+OP_DONE, 0, 0, NULL); // err... we didn't find it, give up.

	G_FUNCTION(func->ofs) = df - functions;
	func->initialized = 1;
	return func;
}

void PR_EmitArrayGetFunction(def_t *scope, char *arrayname)
{
	def_t		*vectortrick;
	dfunction_t	*df;
	dstatement_t	*st;
	def_t		*eq, *def, *index;
	def_t		*fasttrackpossible;

	if (flag_fastarrays) fasttrackpossible = PR_GetDef(type_float, "__ext__fasttrackarrays", NULL, true, 1);
	else fasttrackpossible = NULL;

	def = PR_GetDef(NULL, arrayname, NULL, false, 0);

	if (def->arraysize >= 15 && def->type->size == 1) vectortrick = PR_EmitArrayGetVector(def);
	else vectortrick = NULL;

	pr_scope = scope;

	df = &functions[numfunctions];
	numfunctions++;

	df->s_file = 0;
	df->s_name = PR_CopyString(scope->name, opt_noduplicatestrings );
	df->first_statement = numstatements;
	df->parm_size[0] = 1;
	df->numparms = 1;
	df->parm_start = numpr_globals;
	index = PR_GetDef(type_float, "indexg___", def, true, 1);

	G_FUNCTION(scope->ofs) = df - functions;

	if (fasttrackpossible)
	{
		PR_Statement(pr_opcodes+OP_IFNOT, fasttrackpossible, NULL, &st);
		// fetch_gbl takes: (float size, variant array[]), float index, variant pos
		// note that the array size is coded into the globals, one index before the array.
		if (def->type->size >= 3) PR_Statement3(&pr_opcodes[OP_FETCH_GBL_V], def, index, &def_ret, true);
		else PR_Statement3(&pr_opcodes[OP_FETCH_GBL_F], def, index, &def_ret, true);

		// finish the jump
		st->b = &statements[numstatements] - st;
	}

	if (vectortrick)
	{
		def_t *div3, *intdiv3, *ret;

		// okay, we've got a function to retrieve the var as part of a vector.
		// we need to work out which part, x/y/z that it's stored in.
		// 0, 1, 2 = i - ((int)i/3 *) 3;

		div3 = PR_GetDef(type_float, "div3___", def, true, 1);
		intdiv3 = PR_GetDef(type_float, "intdiv3___", def, true, 1);

		// escape clause - should call some sort of error function instead.. that'd rule!
		eq = PR_Statement(pr_opcodes+OP_GE, index, PR_MakeFloatDef((float)def->arraysize), NULL);
		PR_FreeTemp(PR_Statement(pr_opcodes+OP_IFNOT, eq, 0, &st));
		st->b = 2;
		PR_Statement(pr_opcodes+OP_RETURN, PR_MakeFloatDef(0), 0, &st);

		div3->references++;
		PR_Statement3(pr_opcodes+OP_BITAND, index, index, index, false);
		PR_Statement3(pr_opcodes+OP_DIV_F, index, PR_MakeFloatDef(3), div3, false);
		PR_Statement3(pr_opcodes+OP_BITAND, div3, div3, intdiv3, false);

		PR_Statement3(pr_opcodes+OP_STORE_F, index, &def_parms[0], NULL, false);
		PR_Statement3(pr_opcodes+OP_CALL1, vectortrick, NULL, NULL, false);
		vectortrick->references++;
		ret = PR_GetDef(type_vector, "vec__", pr_scope, true, 1);
		ret->references += 4;
		PR_Statement3(pr_opcodes+OP_STORE_V, &def_ret, ret, NULL, false);

		div3 = PR_Statement(pr_opcodes+OP_MUL_F, intdiv3, PR_MakeFloatDef(3), NULL);
		PR_Statement3(pr_opcodes+OP_SUB_F, index, div3, index, false);
		PR_FreeTemp(div3);

		eq = PR_Statement(pr_opcodes+OP_LT, index, PR_MakeFloatDef(0+0.5f), NULL);
		PR_FreeTemp(PR_Statement(pr_opcodes+OP_IFNOT, eq, 0, &st));
		st->b = 2;
		PR_Statement(pr_opcodes+OP_RETURN, 0, 0, &st);
		st->a = ret->ofs + 0;

		eq = PR_Statement(pr_opcodes+OP_LT, index, PR_MakeFloatDef(1+0.5f), NULL);
		PR_FreeTemp(PR_Statement(pr_opcodes+OP_IFNOT, eq, 0, &st));
		st->b = 2;
		PR_Statement(pr_opcodes+OP_RETURN, 0, 0, &st);
		st->a = ret->ofs + 1;

		eq = PR_Statement(pr_opcodes+OP_LT, index, PR_MakeFloatDef(2+0.5), NULL);
		PR_FreeTemp(PR_Statement(pr_opcodes+OP_IFNOT, eq, 0, &st));
		st->b = 2;
		PR_Statement(pr_opcodes+OP_RETURN, 0, 0, &st);
		st->a = ret->ofs + 2;
		PR_FreeTemp(ret);
		PR_FreeTemp(index);
	}
	else
	{
		PR_Statement3(pr_opcodes+OP_BITAND, index, index, index, false);
		PR_ArrayRecurseDivideRegular(def, index, 0, def->arraysize, false );
	}

	PR_Statement(pr_opcodes+OP_RETURN, PR_MakeFloatDef(0), 0, NULL);
	PR_Statement(pr_opcodes+OP_DONE, 0, 0, NULL);

	locals_end = numpr_globals;
	df->locals = locals_end - df->parm_start;

	PR_WriteAsmFunction(pr_scope, df->first_statement, df->parm_start);
	PR_FreeTemps();
}

void PR_ArraySetRecurseDivide(def_t *array, def_t *index, def_t *value, int min, int max)
{
	dstatement_t	*st;
	def_t		*eq;

	if (min == max || min+1 == max)
	{
		eq = PR_Statement(pr_opcodes+OP_EQ_F, index, PR_MakeFloatDef((float)min), NULL);
		PR_UnFreeTemp(index);
		PR_FreeTemp(PR_Statement(pr_opcodes+OP_IFNOT, eq, 0, &st));
		st->b = 3;
		if (array->type->size == 3) PR_Statement(pr_opcodes+OP_STORE_V, value, array, &st);
		else PR_Statement(pr_opcodes+OP_STORE_F, value, array, &st);
		st->b = array->ofs + min*array->type->size;
		PR_Statement(pr_opcodes+OP_RETURN, 0, 0, &st);
	}
	else
	{
		int mid = min + (max-min)/2;

		if (max-min>4)
		{
			eq = PR_Statement(pr_opcodes+OP_LT, index, PR_MakeFloatDef((float)mid), NULL);
			PR_UnFreeTemp(index);
			PR_FreeTemp(PR_Statement(pr_opcodes+OP_IFNOT, eq, 0, &st));
		}
		else st = NULL;
		PR_ArraySetRecurseDivide(array, index, value, min, mid);
		if (st) st->b = numstatements - (st-statements);
		PR_ArraySetRecurseDivide(array, index, value, mid, max);
	}
}

void PR_EmitArraySetFunction(def_t *scope, char *arrayname)
{
	dfunction_t	*df;
	def_t		*def, *index, *value;
	def_t		*fasttrackpossible;

	if (flag_fastarrays) fasttrackpossible = PR_GetDef(NULL, "__ext__fasttrackarrays", NULL, true, 1);
	else fasttrackpossible = NULL;

	def = PR_GetDef(NULL, arrayname, NULL, false, 0);
	pr_scope = scope;

	df = &functions[numfunctions];
	numfunctions++;

	df->s_file = 0;
	df->s_name = PR_CopyString(scope->name, opt_noduplicatestrings );
	df->first_statement = numstatements;
	df->parm_size[0] = 1;
	df->parm_size[1] = def->type->size;
	df->numparms = 2;
	df->parm_start = numpr_globals;
	index = PR_GetDef(type_float, "indexs___", def, true, 1);
	value = PR_GetDef(def->type, "value___", def, true, 1);
	locals_end = numpr_globals;
	df->locals = locals_end - df->parm_start;

	G_FUNCTION(scope->ofs) = df - functions;

	if (fasttrackpossible)
	{
		dstatement_t *st;

		PR_Statement(pr_opcodes+OP_IFNOT, fasttrackpossible, NULL, &st);
		// note that the array size is coded into the globals, one index before the array.

		// address stuff is integer based, but standard qc (which this accelerates in supported engines) only supports floats
		PR_Statement3(&pr_opcodes[OP_CONV_FTOI], index, NULL, index, true);
		PR_SimpleStatement (OP_BOUNDCHECK, index->ofs, ((int*)pr_globals)[def->ofs-1], 0, true); // annoy the programmer. :p
		if (def->type->size != 1)//shift it upwards for larger types
			PR_Statement3(&pr_opcodes[OP_MUL_I], index, PR_MakeIntDef(def->type->size), index, true);
		PR_Statement3(&pr_opcodes[OP_GLOBAL_ADD], def, index, index, true); // comes with built in add
		if (def->type->size >= 3) PR_Statement3(&pr_opcodes[OP_STOREP_V], value, index, NULL, true); // *b = a
		else PR_Statement3(&pr_opcodes[OP_STOREP_F], value, index, NULL, true);

		// finish the jump
		st->b = &statements[numstatements] - st;
	}

	PR_Statement3(pr_opcodes+OP_BITAND, index, index, index, false);
	PR_ArraySetRecurseDivide(def, index, value, 0, def->arraysize);
	PR_Statement(pr_opcodes+OP_DONE, 0, 0, NULL);
	PR_WriteAsmFunction(pr_scope, df->first_statement, df->parm_start);
	PR_FreeTemps();
}

/*
====================
PR_DummyDef

register a def, and all of it's sub parts.
only the main def is of use to the compiler.
the subparts are emitted to the compiler and allow correct saving/loading
be careful with fields, this doesn't allocated space, so will it allocate fields. 
It only creates defs at specified offsets.
====================
*/
def_t *PR_DummyDef(type_t *type, char *name, def_t *scope, int arraysize, uint ofs, int referable)
{
	char	array[64];
	char	newname[256];
	int	a;
	def_t	*def, *first=NULL;

	if (name)
	{
		KEYWORD(var);
		KEYWORD(for);
		KEYWORD(switch);
		KEYWORD(case);
		KEYWORD(default);
		KEYWORD(goto);
		if (type->type != ev_function)
			KEYWORD(break);
		KEYWORD(continue);
		KEYWORD(state);
		KEYWORD(string);
		KEYWORD(float);
		KEYWORD(entity);
		KEYWORD(vector);
		KEYWORD(const);
		KEYWORD(asm);
	}

	for (a = 0; a < arraysize; a++)
	{
		if (a == 0) *array = '\0';
		else sprintf(array, "[%i]", a);

		if (name) sprintf(newname, "%s%s", name, array);
		else *newname = *"";

		// allocate a new def
		def = (void *)Qalloc (sizeof(def_t));
		memset (def, 0, sizeof(*def));
		def->next = NULL;
		def->arraysize = arraysize;
		if (name)
		{
			pr.def_tail->next = def;
			pr.def_tail = def;
		}

		if (a > 0) def->references++;

		def->s_line = pr_source_line;
		def->s_file = s_file;
		def->name = (void *)Qalloc (strlen(newname)+1);
		strcpy (def->name, newname);
		def->type = type;
		def->scope = scope;	
		def->constant = true;

		if (ofs + type->size*a >= MAX_REGS) Sys_Error("MAX_REGS is too small");
		def->ofs = ofs + type->size*a;
		if (!first) first = def;

		if (type->type == ev_struct)
		{
			int	partnum;
			type_t	*parttype = type->param;

			for (partnum = 0; partnum < type->num_parms; partnum++)
			{
				switch (parttype->type)
				{
				case ev_vector:
					sprintf(newname, "%s%s.%s", name, array, parttype->name);
					PR_DummyDef(parttype, newname, scope, 1, ofs + type->size*a + parttype->ofs, false);
					sprintf(newname, "%s%s.%s_x", name, array, parttype->name);
					PR_DummyDef(type_float, newname, scope, 1, ofs + type->size*a + parttype->ofs, false);
					sprintf(newname, "%s%s.%s_y", name, array, parttype->name);
					PR_DummyDef(type_float, newname, scope, 1, ofs + type->size*a + parttype->ofs+1, false);
					sprintf(newname, "%s%s.%s_z", name, array, parttype->name);
					PR_DummyDef(type_float, newname, scope, 1, ofs + type->size*a + parttype->ofs+2, false);
					break;
				case ev_float:
				case ev_string:
				case ev_entity:
				case ev_field:				
				case ev_pointer:
				case ev_integer:
				case ev_struct:
				case ev_union:
				case ev_variant: // for lack of any better alternative
					sprintf(newname, "%s%s.%s", name, array, parttype->name);
					PR_DummyDef(parttype, newname, scope, 1, ofs + type->size*a + parttype->ofs, false);
					break;
				case ev_function:
					sprintf(newname, "%s%s.%s", name, array, parttype->name);
					PR_DummyDef(parttype, newname, scope, 1, ofs + type->size*a +parttype->ofs, false)->initialized = true;
					break;
				case ev_void:
					break;
				}
				parttype = parttype->next;
			}			
		}
		else if (type->type == ev_vector)
		{	
			// do the vector thing.
			sprintf(newname, "%s%s_x", name, array);
			PR_DummyDef(type_float, newname, scope, 1, ofs + type->size*a+0, referable);
			sprintf(newname, "%s%s_y", name, array);
			PR_DummyDef(type_float, newname, scope, 1, ofs + type->size*a+1, referable);
			sprintf(newname, "%s%s_z", name, array);
			PR_DummyDef(type_float, newname, scope, 1, ofs + type->size*a+2, referable);
		}
		else if (type->type == ev_field)
		{
			if (type->aux_type->type == ev_vector)
			{
				// do the vector thing.
				sprintf(newname, "%s%s_x", name, array);
				PR_DummyDef(type_floatfield, newname, scope, 1, ofs + type->size*a+0, referable);
				sprintf(newname, "%s%s_y", name, array);
				PR_DummyDef(type_floatfield, newname, scope, 1, ofs + type->size*a+1, referable);
				sprintf(newname, "%s%s_z", name, array);
				PR_DummyDef(type_floatfield, newname, scope, 1, ofs + type->size*a+2, referable);
			}
		}
	}
	if (referable)
	{
		// anything above needs to be left in, and so warning about not using it is just going to pee people off.
		if (!Hash_Get(&globalstable, "end_sys_fields")) first->references++;
		if (arraysize <= 1) first->constant = false;
		if (scope) Hash_Add(&localstable, first->name, first, Qalloc(sizeof(bucket_t)));
		else Hash_Add(&globalstable, first->name, first, Qalloc(sizeof(bucket_t)));
		if (!scope && asmfile) FS_Printf(asmfile, "%s %s;\n", TypeName(first->type), first->name);
	}
	return first;
}

/*
============
PR_GetDef

If type is NULL, it will match any type
If allocate is true, a new def will be allocated if it can't be found
============
*/
def_t *PR_GetDef (type_t *type, char *name, def_t *scope, bool allocate, int arraysize)
{
	int		ofs;
	def_t		*def;
	uint		i;

	if (scope)
	{
		def = Hash_Get(&localstable, name);
		while(def)
		{
			if ( def->scope && def->scope != scope)
			{
				def = Hash_GetNext(&localstable, name, def);
				continue;	// in a different function
			}

			if (type && typecmp(def->type, type)) PR_ParseError (ERR_TYPEMISMATCHREDEC, "Type mismatch on redeclaration of %s. %s, should be %s",name, TypeName(type), TypeName(def->type));
			if (def->arraysize != arraysize && arraysize)
				PR_ParseError (ERR_TYPEMISMATCHARRAYSIZE, "Array sizes for redecleration of %s do not match",name);
			if (allocate && scope)
			{
				PR_ParseWarning (WARN_DUPLICATEDEFINITION, "%s duplicate definition ignored", name);
				PR_ParsePrintDef(WARN_DUPLICATEDEFINITION, def);
			}
			return def;
		}
	}

	def = Hash_Get(&globalstable, name);
	while(def)
	{
		if ( def->scope && def->scope != scope)
		{
			def = Hash_GetNext(&globalstable, name, def);
			continue;	// in a different function
		}

		if (type && typecmp(def->type, type))
		{
			if (!pr_scope) PR_ParseError (ERR_TYPEMISMATCHREDEC, "Type mismatch on redeclaration of %s. %s, should be %s",name, TypeName(type), TypeName(def->type));
		}
		if (def->arraysize != arraysize && arraysize)
			PR_ParseError (ERR_TYPEMISMATCHARRAYSIZE, "Array sizes for redecleration of %s do not match",name);
		if (allocate && scope)
		{
			if (pr_scope)
			{	
				// warn? or would that be pointless?
				def = Hash_GetNext(&globalstable, name, def);
				continue;	// in a different function
			}

			PR_ParseWarning (WARN_DUPLICATEDEFINITION, "%s duplicate definition ignored", name);
			PR_ParsePrintDef(WARN_DUPLICATEDEFINITION, def);
		}
		return def;
	}

	if (Hash_Get != &Hash_Get && !allocate) // do we want to try case insensative too?
	{
		if (scope)
		{
			def = Hash_Get(&localstable, name);
			while(def)
			{
				if ( def->scope && def->scope != scope)
				{
					def = Hash_GetNext(&localstable, name, def);
					continue;	// in a different function
				}

				if (type && typecmp(def->type, type))
					PR_ParseError (ERR_TYPEMISMATCHREDEC, "Type mismatch on redeclaration of %s. %s, should be %s",name, TypeName(type), TypeName(def->type));
				if (def->arraysize != arraysize && arraysize)
					PR_ParseError (ERR_TYPEMISMATCHARRAYSIZE, "Array sizes for redecleration of %s do not match",name);
				if (allocate && scope)
				{
					PR_ParseWarning (WARN_DUPLICATEDEFINITION, "%s duplicate definition ignored", name);
					PR_ParsePrintDef(WARN_DUPLICATEDEFINITION, def);
				}
				return def;
			}
		}

		def = Hash_Get(&globalstable, name);
		while(def)
		{
			if ( def->scope && def->scope != scope)
			{
				def = Hash_GetNext(&globalstable, name, def);
				continue;	 // in a different function
			}

			if (type && typecmp(def->type, type))
			{
				if (!pr_scope) PR_ParseError (ERR_TYPEMISMATCHREDEC, "Type mismatch on redeclaration of %s. %s, should be %s",name, TypeName(type), TypeName(def->type));
			}
			if (def->arraysize != arraysize && arraysize)
				PR_ParseError (ERR_TYPEMISMATCHARRAYSIZE, "Array sizes for redecleration of %s do not match",name);
			if (allocate && scope)
			{
				if (pr_scope)
				{	
					// warn? or would that be pointless?
					def = Hash_GetNext(&globalstable, name, def);
					continue;	// in a different function
				}

				PR_ParseWarning (WARN_DUPLICATEDEFINITION, "%s duplicate definition ignored", name);
				PR_ParsePrintDef(WARN_DUPLICATEDEFINITION, def);
			}
			return def;
		}
	}

	if (!allocate) return NULL;
	if (arraysize < 1) PR_ParseError (ERR_ARRAYNEEDSSIZE, "First declaration of array %s with no size",name);

	if (scope && PR_GetDef(type, name, NULL, false, arraysize))
	{
		PR_ParseWarning(WARN_SAMENAMEASGLOBAL, "Local \"%s\" defined with name of a global", name);
	}

	ofs = numpr_globals;
	if (arraysize > 1)
	{	
		// write the array size
		ofs = PR_GetFreeOffsetSpace(1 + (type->size * arraysize));
		// An array needs the size written first. This is a hexen2 opcode thing.
		((int *)pr_globals)[ofs] = arraysize - 1;
		ofs++;
	}
	else ofs = PR_GetFreeOffsetSpace(type->size * arraysize);

	def = PR_DummyDef(type, name, scope, arraysize, ofs, true);

	// fix up fields.
	if (type->type == ev_field && allocate != 2)
	{
		for (i = 0; i < type->size*arraysize; i++) // make arrays of fields work.
			*(int *)&pr_globals[def->ofs+i] = pr.size_fields+i;
		pr.size_fields += i;
	}

	if (scope)
	{
		def->nextlocal = pr.localvars;
		pr.localvars = def;
		def->local = true;
	}
	return def;
}

def_t *PR_DummyFieldDef(type_t *type, char *name, def_t *scope, int arraysize, uint *fieldofs)
{
	char	array[64];
	char	newname[256];
	int	a, parms;
	def_t	*def, *first=NULL;
	uint	startfield = *fieldofs;
	uint	maxfield = startfield;
	type_t	*ftype;
	bool	isunion;

	for (a = 0; a < arraysize; a++)
	{
		if (a == 0) *array = '\0';
		else sprintf(array, "[%i]", a);

		if (*name)
		{
			sprintf(newname, "%s%s", name, array);

			// allocate a new def
			def = (void *)Qalloc (sizeof(def_t));
			memset (def, 0, sizeof(*def));
			def->next = NULL;
			def->arraysize = arraysize;

			pr.def_tail->next = def;
			pr.def_tail = def;

			def->s_line = pr_source_line;
			def->s_file = s_file;

			def->name = (void *)Qalloc (strlen(newname)+1);
			strcpy (def->name, newname);
			def->type = type;
			def->scope = scope;	
			def->ofs = PR_GetFreeOffsetSpace(1);
			((int *)pr_globals)[def->ofs] = *fieldofs;
			*fieldofs++;
			if (!first) first = def;
		}
		else def = NULL;

		if ((type)->type == ev_struct || (type)->type == ev_union)
		{
			int	partnum;
			type_t	*parttype;

			if (def) def->references++;
			parttype = (type)->param;
			isunion = ((type)->type == ev_union);

			for (partnum = 0, parms = (type)->num_parms; partnum < parms; partnum++)
			{
				switch (parttype->type)
				{
				case ev_union:
				case ev_struct:
					if (*name) sprintf(newname, "%s%s.%s", name, array, parttype->name);
					else sprintf(newname, "%s%s", parttype->name, array);
					def = PR_DummyFieldDef(parttype, newname, scope, 1, fieldofs);
					break;
				case ev_float:
				case ev_string:
				case ev_vector:
				case ev_entity:
				case ev_field:
				case ev_pointer:
				case ev_integer:
				case ev_variant:
					if (*name) sprintf(newname, "%s%s.%s", name, array, parttype->name);
					else sprintf(newname, "%s%s", parttype->name, array);
					ftype = PR_NewType("FIELD TYPE", ev_field);
					ftype->aux_type = parttype;
					// vector fields create a _y and _z too, so we need this still.
					if (parttype->type == ev_vector) ftype->size = parttype->size;
					def = PR_GetDef(NULL, newname, scope, false, 1);
					if (!def) def = PR_GetDef(ftype, newname, scope, true, 1);
					else
					{
						PR_ParseWarning(WARN_CONFLICTINGUNIONMEMBER, "conflicting offsets for union/struct expansion of %s. Ignoring new def.", newname);
						PR_ParsePrintDef(WARN_CONFLICTINGUNIONMEMBER, def);
					}
					break;
				case ev_function:
					if (*name) sprintf(newname, "%s%s.%s", name, array, parttype->name);
					else sprintf(newname, "%s%s", parttype->name, array);
					ftype = PR_NewType("FIELD TYPE", ev_field);
					ftype->aux_type = parttype;
					def = PR_GetDef(ftype, newname, scope, true, 1);
					def->initialized = true;
					((int *)pr_globals)[def->ofs] = *fieldofs;
					*fieldofs += parttype->size;
					break;
				case ev_void:
					break;
				}
				if (*fieldofs > maxfield) maxfield = *fieldofs;
				if (isunion) *fieldofs = startfield;

				type = parttype;
				parttype = parttype->next;
			}			
		}
	}

	*fieldofs = maxfield; // final size of the union.
	return first;
}



void PR_ExpandUnionToFields(type_t *type, int *fields)
{
	type_t *pass = type->aux_type;
	PR_DummyFieldDef(pass, "", pr_scope, 1, fields);
}

/*
================
PR_ParseDefs

Called at the outer layer and when a local statement is hit
================
*/
void PR_ParseDefs (char *classname)
{
	char		*name;
	type_t		*type, *parm;
	def_t		*def, *d;
	function_t	*f;
	dfunction_t	*df;
	int		i;
	bool		shared = false;
	bool		externfnc = false;
	bool		isconstant = false;
	bool		isvar = false;
	bool		noref = false;
	bool		nosave = false;
	bool		allocatenew = true;
	int		ispointer;
	gofs_t		oldglobals;
	int		arraysize;

	if(PR_CheckKeyword(keyword_enum, "enum"))
	{
		float v = 0;
		PR_Expect("{");
		i = 0;
		d = NULL;
		while(1)
		{
			name = PR_ParseName();
			if (PR_CheckToken("="))
			{
				if (pr_token_type != tt_immediate && pr_immediate_type->type != ev_float)
				{
					def = PR_GetDef(NULL, PR_ParseName(), NULL, false, 0);
					if (def)
					{
						if (!def->constant) PR_ParseError(ERR_NOTANUMBER, "enum - %s is not a constant", def->name);
						else v = G_FLOAT(def->ofs);
					}
					else PR_ParseError(ERR_NOTANUMBER, "enum - not a number");
				}
				else
				{
					v = pr_immediate._float;
					PR_Lex();
				}
			}
			def = PR_MakeFloatDef(v);
			Hash_Add(&globalstable, name, def, Qalloc(sizeof(bucket_t)));
			v++;

			if (PR_CheckToken("}")) break;
			PR_Expect(",");
		}
		PR_Expect(";");
		return;
	}

	if (PR_CheckKeyword(keyword_enumflags, "enumflags"))
	{
		float v = 1;
		int bits;
		PR_Expect("{");
		i = 0;
		d = NULL;
		while(1)
		{
			name = PR_ParseName();
			if (PR_CheckToken("="))
			{
				if (pr_token_type != tt_immediate && pr_immediate_type->type != ev_float)
				{
					def = PR_GetDef(NULL, PR_ParseName(), NULL, false, 0);
					if (def)
					{
						if (!def->constant)
							PR_ParseError(ERR_NOTANUMBER, "enumflags - %s is not a constant", def->name);
						else v = G_FLOAT(def->ofs);
					}
					else PR_ParseError(ERR_NOTANUMBER, "enumflags - not a number");
				}
				else
				{
					v = pr_immediate._float;
					PR_Lex();
				}
			}

			bits = 0;
			i = (int)v;
			if (i != v) PR_ParseWarning(WARN_ENUMFLAGS_NOTINTEGER, "enumflags - %f not an integer", v);
			else
			{
				while(i)
				{
					if (((i>>1)<<1) != i) bits++;
					i>>=1;
				}
				if (bits != 1) PR_ParseWarning(WARN_ENUMFLAGS_NOTBINARY, "enumflags - value %i not a single bit", (int)v);
			}

			def = PR_MakeFloatDef(v);
			Hash_Add(&globalstable, name, def, Qalloc(sizeof(bucket_t)));

			v *= 2;
			if (PR_CheckToken("}")) break;
			PR_Expect(",");
		}
		PR_Expect(";");
		return;
	}
	if (PR_CheckKeyword (keyword_typedef, "typedef"))
	{
		type = PR_ParseType( true );
		if (!type) PR_ParseError(ERR_NOTANAME, "typedef found unexpected tokens");
		type->name = PR_CopyString(pr_token, opt_noduplicatestrings ) + strings;
		PR_Lex();
		PR_Expect(";");
		return;
	}
	while(1)
	{
		if (PR_CheckKeyword(keyword_extern, "extern")) externfnc = true;
		else if (PR_CheckKeyword(keyword_shared, "shared"))
		{
			shared=true;
			if (pr_scope) PR_ParseError (ERR_NOSHAREDLOCALS, "Cannot have shared locals");
		}
		else if (PR_CheckKeyword(keyword_const, "const")) isconstant = true;
		else if (PR_CheckKeyword(keyword_var, "var")) isvar = true;
		else if (PR_CheckKeyword(keyword_noref, "noref")) noref = true;
		else if (PR_CheckKeyword(keyword_nosave, "nosave")) nosave = true;
		else break;
	}

	type = PR_ParseType (false);
	if (type == NULL) return;

	if (externfnc && type->type != ev_function)
	{
		PR_Message("Only functions may be defined as external (yet)\n");
		externfnc = false;
	}
	do
	{
		if (PR_CheckToken ("*"))
		{
			ispointer = 1;
			while(PR_CheckToken ("*")) ispointer++;
			name = PR_ParseName ();
		}
		else if (PR_CheckToken (";"))
		{
			if (type->type == ev_field && (type->aux_type->type == ev_union || type->aux_type->type == ev_struct))
			{
				PR_ExpandUnionToFields(type, &pr.size_fields);
				return;
			}
			PR_ParseError (ERR_TYPEWITHNONAME, "type with no name");
			name = NULL;
			ispointer = false;
		}
		else
		{
			name = PR_ParseName ();
			ispointer = false;
		}
		if (PR_CheckToken("::") && !classname)
		{
			classname = name;
			name = PR_ParseName();
		}

		// check for an array
		if ( PR_CheckToken ("[") )
		{
			char *oldprfile = pr_file_p;
			arraysize = 0;
			if (PR_CheckToken("]"))
			{
				PR_Expect("=");
				PR_Expect("{");
				PR_Lex();
				arraysize++;
				while(1)
				{
					if(pr_token_type == tt_eof) break;
					if (PR_CheckToken(",")) arraysize++;
					if (PR_CheckToken("}")) break;
					PR_Lex();
				}
				pr_file_p = oldprfile;
				PR_Lex();
			}
			else
			{
				def = PR_Expression(TOP_PRIORITY, true);
				if (!def->constant) PR_ParseError(ERR_BADARRAYSIZE, "Array size is not a constant value");
				else if (def->type->type == ev_integer) arraysize = G_INT(def->ofs);
				else if (def->type->type == ev_float)
				{
					arraysize = (int)G_FLOAT(def->ofs);
					if ((float)arraysize != G_FLOAT(def->ofs))
						PR_ParseError(ERR_BADARRAYSIZE, "Array size is not a constant value");
				}
				else PR_ParseError(ERR_BADARRAYSIZE, "Array size must be of int value");
			}
			if (arraysize < 1)
			{
				PR_ParseError (ERR_BADARRAYSIZE, "Definition of array (%s) size is not of a numerical value", name);
				arraysize = 0; // grrr...
			}
			PR_Expect("]");
		}
		else arraysize = 1;

		if (PR_CheckToken("(")) type = PR_ParseFunctionType(false, type);

		if (classname)
		{
			char *membername = name;
			name = Qalloc(strlen(classname) + strlen(name) + 3);
			sprintf(name, "%s::"MEMBERFIELDNAME, classname, membername);
			if (!PR_GetDef(NULL, name, NULL, false, 0))
				PR_ParseError(ERR_NOTANAME, "%s %s is not a member of class %s\n", TypeName(type), membername, classname);
			sprintf(name, "%s::%s", classname, membername);

			pr_classtype = TypeForName(classname);
			if (!pr_classtype || !pr_classtype->parentclass)
				PR_ParseError(ERR_NOTANAME, "%s is not a class\n", classname);
		}
		else pr_classtype = NULL;

		oldglobals = numpr_globals;
		if (ispointer)
		{
			parm = type;
			while(ispointer)
			{
				ispointer--;
				parm = PR_PointerTypeTo(parm);
			}
			def = PR_GetDef (parm, name, pr_scope, allocatenew, arraysize);
		}
		else def = PR_GetDef (type, name, pr_scope, allocatenew, arraysize);

		if (!def) PR_ParseError(ERR_NOTANAME, "%s is not part of class %s", name, classname);

		if (noref) def->references++;
		if (nosave) def->saved = false;
		else def->saved = true;

		// shared count as initiialized
		if (!def->initialized && shared)
		{	
			def->shared = shared;
			def->initialized = true;
		}
		if (externfnc) def->initialized = 2;

		// check for an initialization
		if (type->type == ev_function && (pr_scope))
		{
			if ( PR_CheckToken ("=") )
				PR_ParseError (ERR_INITIALISEDLOCALFUNCTION, "local functions may not be initialised");

			arraysize = def->arraysize;
			d = def;	//apply to ALL elements
			while(arraysize--)
			{
				d->initialized = 1;	//fake function
				G_FUNCTION(d->ofs) = 0;
				d = d->next;
			}
			continue;
		}
		// this is an initialisation (or a function)
		if ( PR_CheckToken ("=") || ((type->type == ev_function) && (pr_token[0] == '{' || pr_token[0] == '[' || pr_token[0] == ':')))
		{
			if (def->shared)
				PR_ParseError (ERR_SHAREDINITIALISED, "shared values may not be assigned an initial value", name);
			if (def->initialized == 1)
			{
				if (def->type->type == ev_function)
				{
					i = G_FUNCTION(def->ofs);
					df = &functions[i];
					PR_ParseErrorPrintDef (ERR_REDECLARATION, def, "%s redeclared, prev instance is in %s", name, strings+df->s_file);
				}
				else PR_ParseErrorPrintDef(ERR_REDECLARATION, def, "%s redeclared", name);
			}

			if (autoprototype)
			{	
				// ignore the code and stuff
				if (PR_CheckToken("["))
				{
					while (!PR_CheckToken("]"))
					{
						if (pr_token_type == tt_eof) break;
						PR_Lex();
					}
				}
				if (PR_CheckToken("{"))
				{
					int blev = 1;
					// balance out the { and }
					while(blev)
					{
						if (pr_token_type == tt_eof) break;
						if (PR_CheckToken("{")) blev++;
						else if (PR_CheckToken("}")) blev--;
						else PR_Lex(); // ignore it.
					}
				}
				else
				{
					PR_CheckToken("#");
					PR_Lex();
				}
				continue;
			}

			if (pr_token_type == tt_name)
			{
				uint i;

				if (def->arraysize>1)
					PR_ParseError(ERR_ARRAYNEEDSBRACES, "Array initialisation requires curly braces");

				d = PR_GetDef(NULL, pr_token, pr_scope, false, 0);
				if (!d) PR_ParseError(ERR_NOTDEFINED, "%s was not defined\n", name);
				if (typecmp(def->type, d->type))
					PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate type for %s", name);
				for (i = 0; i < d->type->size; i++) G_INT(def->ofs) = G_INT(d->ofs);
				PR_Lex();

				if (type->type == ev_function)
				{
					def->initialized = 1;
					def->constant = !isvar;
				}
				continue;
			}
			else if (type->type == ev_function)
			{
				if (isvar) def->constant = false;
				else def->constant = true;

				if (PR_CheckToken("0"))
				{
					// fake function
					def->constant = 0;
					def->initialized = 1;	
					G_FUNCTION(def->ofs) = 0;
					continue;
				}
				if (!def->constant && arraysize == 1)
				{
					// fake function
					def->constant = 0;
					def->initialized = 1;	

					name = PR_ParseName ();
					d = PR_GetDef (NULL, name, pr_scope, false, 0);
					if (!d) PR_ParseError(ERR_NOTDEFINED, "%s was not previously defined", name);
					G_FUNCTION(def->ofs+i) = G_FUNCTION(d->ofs);
					continue;
				}

				if (arraysize > 1)
				{
					int i;

					// fake function
					def->initialized = 1;	
					PR_Expect ("{");
					i = 0;

					do
					{
						if (PR_CheckToken("0")) G_FUNCTION(def->ofs+i) = 0;
						else
						{
							name = PR_ParseName ();
							d = PR_GetDef (NULL, name, pr_scope, false, 0);
							if (!d) PR_ParseError(ERR_NOTDEFINED, "%s was not defined", name);
							else
							{
								if (!d->initialized)
									PR_ParseWarning(WARN_NOTDEFINED, "initialisation of function arrays must be placed after the body of all functions used (%s)", name);
								G_FUNCTION(def->ofs+i) = G_FUNCTION(d->ofs);
							}
						}
						i++;
					} while(PR_CheckToken(","));

					arraysize = def->arraysize;
					d = def; // apply to ALL elements
					while(arraysize--)
					{
						d->initialized = 1;	//fake function
						d = d->next;
					}

					PR_Expect("}");
					if (i > def->arraysize) PR_ParseError(ERR_TOOMANYINITIALISERS, "Too many initializers");
					continue;
				}
				if (!def->constant) PR_ParseError(0, "Initialised functions must be constant");

				def->references++;
				pr_scope = def;
				f = PR_ParseImmediateStatements (type);
				pr_scope = NULL;
				def->initialized = 1;
				G_FUNCTION(def->ofs) = numfunctions;
				f->def = def;

				// fill in the dfunction
				df = &functions[numfunctions];
				numfunctions++;
				if (f->builtin) df->first_statement = -f->builtin;
				else df->first_statement = f->code;

				if (f->builtin && opt_function_names);
				else df->s_name = PR_CopyString (f->def->name, opt_noduplicatestrings );
				df->s_file = s_file2;
				df->numparms =  f->def->type->num_parms;
				df->locals = locals_end - locals_start;
				df->parm_start = locals_start;
				for (i=0,parm = type->param ; i<df->numparms ; i++, parm = parm->next)
				{
					df->parm_size[i] = parm->size;
				}
				continue;
			}

			else if (type->type == ev_struct)
			{
				int arraypart, partnum;
				type_t *parttype;
				def->initialized = 1;
				if (isvar) def->constant = true;
				else def->constant = false;
				
				// FIXME: should do this recursivly
				PR_Expect("{");
				for (arraypart = 0; arraypart < arraysize; arraypart++)
				{
					parttype = type->param;
					PR_Expect("{");
					for (partnum = 0; partnum < type->num_parms; partnum++)
					{
						switch (parttype->type)
						{
						case ev_float:
						case ev_integer:
						case ev_vector:
							if (pr_token_type == tt_punct)
							{
								if (PR_CheckToken("{")) PR_Expect("}");
								else PR_ParseError(ERR_UNEXPECTEDPUNCTUATION, "Unexpected punctuation");

							}
							else if (pr_token_type == tt_immediate)
							{
								if (pr_immediate_type->type == ev_float && parttype->type == ev_integer)
									G_INT(def->ofs + arraypart*type->size + parttype->ofs) = (int)pr_immediate._float;
								else if (pr_immediate_type->type != parttype->type)
									PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate subtype for %s.%s", def->name, parttype->name);
								else Mem_Copy (pr_globals + def->ofs + arraypart*type->size + parttype->ofs, &pr_immediate, 4*type_size[pr_immediate_type->type]);
							}
							else if (pr_token_type == tt_name)
							{
								d = PR_GetDef(NULL, pr_token, pr_scope, false, 0);
								if (!d) PR_ParseError(ERR_NOTDEFINED, "%s was not defined\n", pr_token);
								else if (d->type->type != parttype->type)
									PR_ParseError (ERR_WRONGSUBTYPE, "wrong subtype for %s.%s", def->name, parttype->name);
								else if (!d->constant) PR_ParseError(ERR_NOTACONSTANT, "%s isn't a constant\n", pr_token);
								Mem_Copy (pr_globals + def->ofs + arraypart*type->size + parttype->ofs, pr_globals + d->ofs, 4*d->type->size);
							}
							else PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate subtype for %s.%s", def->name, parttype->name);
							PR_Lex ();
							break;
						case ev_string:
							if (pr_token_type == tt_punct)
							{
								if (PR_CheckToken("{"))
								{
									uint i;
									for (i = 0; i < parttype->size; i++)
									{
										G_INT(def->ofs+arraypart*type->size+parttype->ofs+i) = PR_CopyString(pr_immediate_string, opt_noduplicatestrings );
										PR_Lex ();

										if (!PR_CheckToken(","))
										{
											i++;
											break;
										}
									}
									for (; i < parttype->size; i++)
									{
										G_INT(def->ofs+arraypart*type->size+parttype->ofs+i) = 0;										
									}
									PR_Expect("}");
								}
								else
									PR_ParseError(ERR_UNEXPECTEDPUNCTUATION, "Unexpected punctuation");
							}
							else
							{
								G_INT(def->ofs+arraypart*type->size+parttype->ofs) = PR_CopyString(pr_immediate_string, opt_noduplicatestrings );
								PR_Lex ();
							}
							break;
						case ev_function:
							if (pr_token_type == tt_immediate)
							{
								if (pr_immediate._int != 0)
									PR_ParseError(ERR_NOTFUNCTIONTYPE, "Expected function name or NULL");
								G_FUNCTION(def->ofs+arraypart*type->size+parttype->ofs) = 0;
								PR_Lex();
							}
							else
							{
								name = PR_ParseName ();

								d = PR_GetDef (NULL, name, pr_scope, false, 0);
								if (!d) PR_ParseError(ERR_NOTDEFINED, "%s was not defined\n", name);
								else G_FUNCTION(def->ofs+arraypart*type->size+parttype->ofs) = G_FUNCTION(d->ofs);
							}
							break;
						default:
							PR_ParseError(ERR_TYPEINVALIDINSTRUCT, "type %i not valid in a struct", parttype->type);							
							PR_Lex();
							break;
						}
						if (!PR_CheckToken(",")) break;
						parttype = parttype->next;
					}
					PR_Expect("}");
					if (!PR_CheckToken(",")) break;
				}
				PR_Expect("}");
				continue;
			}
			else if (type->type == ev_integer)
			{
				// handle these differently, because they may need conversions
				if (isvar) def->constant = false;
				else def->constant = true;

				def->initialized = 1;
				Mem_Copy (pr_globals + def->ofs, &pr_immediate, 4*type_size[pr_immediate_type->type]);
				PR_Lex();

				if (pr_immediate_type->type == ev_float) G_INT(def->ofs) = (int)pr_immediate._float;
				else if (pr_immediate_type->type != ev_integer)
					PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate type for %s", name);
				continue;
			}
			else if (type->type == ev_string)
			{
				if (arraysize>=1 && PR_CheckToken("{"))
				{
					int i;

					for (i = 0; i < arraysize; i++)
					{
						// the executor defines strings as true c strings, but reads in index from string table.
						// structures can hide these strings.
						if (i != 0) // not for the first entry - already a string def for that
						{
							d = (void *)Qalloc (sizeof(def_t));
							d->next = NULL;
							pr.def_tail->next = d;
							pr.def_tail = d;

							d->type = type_string;
							d->name = "IMMEDIATE";
							if (isvar) d->constant = false;
							else d->constant = true;
							d->initialized = 1;
							d->scope = NULL;

							d->ofs = def->ofs+i;
							if (d->ofs >= MAX_REGS) Sys_Error("MAX_REGS is too small");
						}
						(((int *)pr_globals)[def->ofs+i]) = PR_CopyString(pr_immediate_string, opt_noduplicatestrings );
						PR_Lex ();
						if (!PR_CheckToken(",")) break;
					}
					PR_Expect("}");
					continue;
				}
				else if (arraysize<=1)
				{
					if (isvar) def->constant = false;
					else def->constant = true;
					def->initialized = 1;
					(((int *)pr_globals)[def->ofs]) = PR_CopyString(pr_immediate_string, opt_noduplicatestrings );
					PR_Lex();

					if (pr_immediate_type->type == ev_float) G_INT(def->ofs) = (int)pr_immediate._float;
					else if (pr_immediate_type->type != ev_string)
						PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate type for %s", name);
					continue;
				}
				else PR_ParseError(ERR_ARRAYNEEDSBRACES, "Array initialization needs curly braces");
			}
			else if (type->type == ev_float)
			{
				if (arraysize >=1 && PR_CheckToken("{"))
				{
					int	i;
					for (i = 0; i < arraysize; i++)
					{
						if (pr_immediate_type->type != ev_float)
							PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate type for %s", name);
						(((float *)pr_globals)[def->ofs+i]) = pr_immediate._float;
						PR_Lex ();
						if (!PR_CheckToken(",")) break;
					}
					PR_Expect("}");
					continue;
				}
				else if (arraysize<=1)
				{
					if (isvar) def->constant = false;
					else def->constant = true;
					def->initialized = 1;

					if (pr_immediate_type->type != ev_float)
						PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate type for %s", name);

					if (def->constant && opt_dupconstdefs)
					{
						if (def->ofs == oldglobals)
						{
							Hash_GetKey(&floatconstdefstable, *(int*)&pr_immediate._float);
							PR_FreeOffset(def->ofs, def->type->size);
							d = PR_MakeFloatDef(pr_immediate._float);
							d->references++;
							def->ofs = d->ofs;
							PR_Lex();
							continue;
						}
					}
					(((float *)pr_globals)[def->ofs]) = pr_immediate._float;
					PR_Lex ();
					continue;
				}
				else PR_ParseError(ERR_ARRAYNEEDSBRACES, "Array initialisation requires curly brasces");
			}
			else if (type->type == ev_vector)
			{
				if (arraysize >= 1 && PR_CheckToken("{"))
				{
					int	i;
					for (i = 0; i < arraysize; i++)
					{
						if (pr_immediate_type->type != ev_vector)
							PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate type for %s", name);
						(((float *)pr_globals)[def->ofs+i*3+0]) = pr_immediate.vector[0];
						(((float *)pr_globals)[def->ofs+i*3+1]) = pr_immediate.vector[1];
						(((float *)pr_globals)[def->ofs+i*3+2]) = pr_immediate.vector[2];
						PR_Lex ();

						if (!PR_CheckToken(",")) break;
					}
					PR_Expect("}");
					continue;
				}
				else if (arraysize<=1)
				{
					if (isvar) def->constant = false;
					else def->constant = true;
					def->initialized = 1;
					(((float *)pr_globals)[def->ofs+0]) = pr_immediate.vector[0];
					(((float *)pr_globals)[def->ofs+1]) = pr_immediate.vector[1];
					(((float *)pr_globals)[def->ofs+2]) = pr_immediate.vector[2];
					PR_Lex ();

					if (pr_immediate_type->type != ev_vector)
						PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate type for %s", name);
					continue;
				}
				else PR_ParseError(ERR_ARRAYNEEDSBRACES, "Array initialization needs curly braces");
			}
			else if (pr_token_type == tt_name)
			{
				d = PR_GetDef (NULL, pr_token, pr_scope, false, 0);
				if (!d) PR_ParseError (ERR_NOTDEFINED, "initialisation name not defined : %s", pr_token);
				if (!d->constant)
				{
					PR_ParseWarning (WARN_NOTCONSTANT, "initialisation name not a constant : %s", pr_token);
					PR_ParsePrintDef(WARN_NOTCONSTANT, d);
				}
				Mem_Copy(def, d, sizeof(*d));
				def->name = name;
				def->initialized = true;
			}
			else if (pr_token_type != tt_immediate)
				PR_ParseError (ERR_BADIMMEDIATETYPE, "not an immediate for %s - %s", name, pr_token);
			else if (pr_immediate_type->type != type->type)
				PR_ParseError (ERR_BADIMMEDIATETYPE, "wrong immediate type for %s - %s", name, pr_token);
			else Mem_Copy (pr_globals + def->ofs, &pr_immediate, 4*type_size[pr_immediate_type->type]);

			if (isvar) def->constant = false;
			else def->constant = true;
			def->initialized = true;
			PR_Lex ();
		}
		else
		{
			if (type->type == ev_function && isvar)
			{
				isconstant = !isvar;
				def->initialized = 1;
			}

			// special flag on fields, 2, makes the pointer obtained from them also constant.
			if (isconstant && type->type == ev_field)
				def->constant = 2;
			else def->constant = isconstant;
		}

	} while (PR_CheckToken (","));

	if (type->type == ev_function) PR_CheckToken (";");
	else
	{
		if (!PR_CheckToken (";"))
			PR_ParseWarning(WARN_UNDESIRABLECONVENTION, "Missing semicolon at end of definition");
	}
}

/*
============
PR_CompileFile

compiles the 0 terminated text, adding defintions to the pr structure
============
*/
void PR_CompileFile (char *string, char *filename)
{	
	jmp_buf		abort_parse;

	pr_error_count = 0;	 // reset counter for new file
	PR_ClearGrabMacros();// clear the frame macros
	compilingfile = filename;
		
	if (opt_filenames)
	{
		pr_file_p = Qalloc(strlen(filename)+1);
		strcpy(pr_file_p, filename);
		s_file = pr_file_p - strings;
		s_file2 = 0;
	}
	else s_file = s_file2 = PR_CopyString (filename, opt_noduplicatestrings );

	pr_file_p = string;
	pr_source_line = 0;
	PR_NewLine (false);
	PR_Lex (); // read first token

	// svae state
	Mem_Copy(&abort_parse, &pr_parse_abort, sizeof(abort_parse));

	while (pr_token_type != tt_eof)
	{
		if (setjmp(pr_parse_abort))
		{
			pr_total_error_count++; // increase total err counter 
			pr_error_count++; //increase local err counter
			if (pr_error_count > MAX_ERRORS)
			{
				PR_ParseWarning(ERR_EXCEEDERRCOUNT, "error count exceeds  %i; stopping compilation\n", MAX_ERRORS);
				Mem_Copy(&pr_parse_abort, &abort_parse, sizeof(abort_parse));
				return;
			}
			PR_SkipToSemicolon();
			if (pr_token_type == tt_eof)
			{
				Mem_Copy(&pr_parse_abort, &abort_parse, sizeof(abort_parse));
				return;
			}
		}
		// outside all functions
		pr_scope = NULL;
		PR_ParseDefs (NULL);
	}

	// restore state
	Mem_Copy(&pr_parse_abort, &abort_parse, sizeof(abort_parse));
}

bool PR_Include(char *filename)
{
	char *newfile;
	char fname[512];
	char *opr_file_p;
	string_t os_file, os_file2;
	int opr_source_line;
	char *ocompilingfile;
	includechunk_t *oldcurrentchunk;

	ocompilingfile = compilingfile;
	os_file = s_file;
	os_file2 = s_file2;
	opr_source_line = pr_source_line;
	opr_file_p = pr_file_p;
	oldcurrentchunk = currentchunk;

	strcpy(fname, filename);
	newfile = QCC_LoadFile(fname);
	currentchunk = NULL;
	pr_file_p = newfile;
	PR_CompileFile(newfile, fname);
	currentchunk = oldcurrentchunk;

	compilingfile = ocompilingfile;
	s_file = os_file;
	s_file2 = os_file2;
	pr_source_line = opr_source_line;
	pr_file_p = opr_file_p;

	return true;
}