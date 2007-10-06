//=======================================================================
//			Copyright XashXT Group 2007 ©
//			vprogs.h - virtual machine header
//=======================================================================
#ifndef VPROGS_H
#define VPROGS_H

/*
==============================================================================

VIRTUAL MACHINE

a internal virtual machine like as QuakeC, but it has more extensions
==============================================================================
*/

// header
#define QPROGS_VERSION	6	// Quake1 progs version
#define FPROGS_VERSION	7	// Fte progs version
#define VPROGS_VERSION	8	// xash progs version

#define VPROGSHEADER16	(('6'<<24)+('1'<<16)+('D'<<8)+'I') // little-endian "ID16"
#define VPROGSHEADER32	(('2'<<24)+('3'<<16)+('D'<<8)+'I') // little-endian "ID32"

// global ofssets
#define OFS_NULL		0
#define OFS_RETURN		1
#define OFS_PARM0		4
#define OFS_PARM1		7
#define OFS_PARM2		10
#define OFS_PARM3		13
#define OFS_PARM4		16
#define OFS_PARM5		19
#define OFS_PARM6		22
#define OFS_PARM7		25
#define RESERVED_OFS	28


#define DEF_SHARED		(1<<14)
#define DEF_SAVEGLOBAL	(1<<15)

// compression block flags
#define COMP_STATEMENTS	1
#define COMP_DEFS		2
#define COMP_FIELDS		4
#define COMP_FUNCTIONS	8
#define COMP_STRINGS	16
#define COMP_GLOBALS	32
#define COMP_LINENUMS	64
#define COMP_TYPES		128
#define COMP_SOURCE		256

#define MAX_PARMS		8

// 16-bit mode
#define dstatement_t	dstatement16_t
#define ddef_t		ddef16_t		//these should be the same except the string type

typedef enum 
{
	ev_void,
	ev_string,
	ev_float,
	ev_vector,
	ev_entity,
	ev_field,
	ev_function,
	ev_pointer,
	ev_integer,
	ev_variant,
	ev_struct,
	ev_union,
} etype_t;

enum {
	OP_DONE,		// 0
	OP_MUL_F,
	OP_MUL_V,
	OP_MUL_FV,	// (vec3_t) * (float)
	OP_MUL_VF,          // (float) * (vec3_t)
	OP_DIV_F,
	OP_ADD_F,
	OP_ADD_V,
	OP_SUB_F,
	OP_SUB_V,
	
	OP_EQ_F,		// 10
	OP_EQ_V,
	OP_EQ_S,
	OP_EQ_E,
	OP_EQ_FNC,
	
	OP_NE_F,
	OP_NE_V,
	OP_NE_S,
	OP_NE_E,
	OP_NE_FNC,
	
	OP_LE,		// = (float) <= (float);
	OP_GE,		// = (float) >= (float);
	OP_LT,		// = (float) <  (float);
	OP_GT,		// = (float) >  (float);

	OP_LOAD_F,
	OP_LOAD_V,
	OP_LOAD_S,
	OP_LOAD_ENT,
	OP_LOAD_FLD,
	OP_LOAD_FNC,

	OP_ADDRESS,	// 30

	OP_STORE_F,
	OP_STORE_V,
	OP_STORE_S,
	OP_STORE_ENT,
	OP_STORE_FLD,
	OP_STORE_FNC,

	OP_STOREP_F,
	OP_STOREP_V,
	OP_STOREP_S,
	OP_STOREP_ENT,	// 40
	OP_STOREP_FLD,
	OP_STOREP_FNC,

	OP_RETURN,
	OP_NOT_F,
	OP_NOT_V,
	OP_NOT_S,
	OP_NOT_ENT,
	OP_NOT_FNC,
	OP_IF,
	OP_IFNOT,		// 50
	OP_CALL0,
	OP_CALL1,
	OP_CALL2,
	OP_CALL3,
	OP_CALL4,
	OP_CALL5,
	OP_CALL6,
	OP_CALL7,
	OP_CALL8,
	OP_STATE,		// 60
	OP_GOTO,
	OP_AND,
	OP_OR,
	
	OP_BITAND,	// = (float) & (float); // of cource converting into integer in real code
	OP_BITOR,

	// version 7 started
	OP_MULSTORE_F,	// f *= f
	OP_MULSTORE_V,	// v *= f
	OP_MULSTOREP_F,	// e.f *= f
	OP_MULSTOREP_V,	// e.v *= f

	OP_DIVSTORE_F,	// f /= f
	OP_DIVSTOREP_F,	// e.f /= f

	OP_ADDSTORE_F,	// f += f
	OP_ADDSTORE_V,	// v += v
	OP_ADDSTOREP_F,	// e.f += f
	OP_ADDSTOREP_V,	// e.v += v

	OP_SUBSTORE_F,	// f -= f
	OP_SUBSTORE_V,	// v -= v
	OP_SUBSTOREP_F,	// e.f -= f
	OP_SUBSTOREP_V,	// e.v -= v

	OP_FETCH_GBL_F,	// 80
	OP_FETCH_GBL_V,
	OP_FETCH_GBL_S,
	OP_FETCH_GBL_E,
	OP_FETCH_G_FNC,

	OP_CSTATE,
	OP_CWSTATE,

	OP_THINKTIME,

	OP_BITSET,	// b  (+) a
	OP_BITSETP,	// .b (+) a
	OP_BITCLR,	// b  (-) a
	OP_BITCLRP,	// .b (-) a

	OP_RAND0,
	OP_RAND1,
	OP_RAND2,
	OP_RANDV0,
	OP_RANDV1,
	OP_RANDV2,

	OP_SWITCH_F,	// switches
	OP_SWITCH_V,	// 100
	OP_SWITCH_S,
	OP_SWITCH_E,
	OP_SWITCH_FNC,

	OP_CASE,
	OP_CASERANGE,

	OP_STORE_I,
	OP_STORE_IF,
	OP_STORE_FI,
	
	OP_ADD_I,
	OP_ADD_FI,	// 110
	OP_ADD_IF,
  
	OP_SUB_I,
	OP_SUB_FI,
	OP_SUB_IF,

	OP_CONV_ITOF,
	OP_CONV_FTOI,
	OP_CP_ITOF,
	OP_CP_FTOI,
	OP_LOAD_I,
	OP_STOREP_I,	// 120
	OP_STOREP_IF,
	OP_STOREP_FI,

	OP_BITAND_I,
	OP_BITOR_I,

	OP_MUL_I,
	OP_DIV_I,
	OP_EQ_I,
	OP_NE_I,

	OP_IFNOTS,
	OP_IFS,		// 130

	OP_NOT_I,

	OP_DIV_VF,

	OP_POWER_I,
	OP_RSHIFT_I,
	OP_LSHIFT_I,

	OP_GLOBAL_ADD,
	OP_POINTER_ADD,	// pointer to 32 bit (remember to *3 for vectors)

	OP_LOADA_F,
	OP_LOADA_V,	
	OP_LOADA_S,	// 140
	OP_LOADA_ENT,
	OP_LOADA_FLD,		
	OP_LOADA_FNC,
	OP_LOADA_I,

	OP_STORE_P,
	OP_LOAD_P,

	OP_LOADP_F,
	OP_LOADP_V,	
	OP_LOADP_S,
	OP_LOADP_ENT,	// 150
	OP_LOADP_FLD,
	OP_LOADP_FNC,
	OP_LOADP_I,

	OP_LE_I,            // (int)c = (int)a <= (int)b;
	OP_GE_I,		// (int)c = (int)a >= (int)b;
	OP_LT_I,		// (int)c = (int)a <  (int)b;
	OP_GT_I,		// (int)c = (int)a >  (int)b;

	OP_LE_IF,           // (float)c = (int)a <= (float)b;
	OP_GE_IF,		// (float)c = (int)a >= (float)b;
	OP_LT_IF,		// (float)c = (int)a <  (float)b;
	OP_GT_IF,		// (float)c = (int)a >  (float)b;

	OP_LE_FI,		// (float)c = (float)a <= (int)b;
	OP_GE_FI,		// (float)c = (float)a >= (int)b;
	OP_LT_FI,		// (float)c = (float)a <  (int)b;
	OP_GT_FI,		// (float)c = (float)a >  (int)b;

	OP_EQ_IF,
	OP_EQ_FI,

	OP_ADD_SF,	// (char*)c = (char*)a + (float)b
	OP_SUB_S,		// (float)c = (char*)a - (char*)b
	OP_STOREP_C,	// (float)c = *(char*)b = (float)a
	OP_LOADP_C,	// (float)c = *(char*) // 170

	OP_MUL_IF,
	OP_MUL_FI,
	OP_MUL_VI,
	OP_MUL_IV,
	OP_DIV_IF,
	OP_DIV_FI,
	OP_BITAND_IF,
	OP_BITOR_IF,
	OP_BITAND_FI,	// 180
	OP_BITOR_FI,
	OP_AND_I,
	OP_OR_I,
	OP_AND_IF,
	OP_OR_IF,
	OP_AND_FI,
	OP_OR_FI,
	OP_NE_IF,
	OP_NE_FI,

	OP_GSTOREP_I,	// 190
	OP_GSTOREP_F,		
	OP_GSTOREP_ENT,
	OP_GSTOREP_FLD,	// integers
	OP_GSTOREP_S,
	OP_GSTOREP_FNC,	// pointers
	OP_GSTOREP_V,
	OP_GADDRESS,
	OP_GLOAD_I,
	OP_GLOAD_F,
	OP_GLOAD_FLD,	// 200
	OP_GLOAD_ENT,
	OP_GLOAD_S,
	OP_GLOAD_FNC,
	OP_GLOAD_V,
	OP_BOUNDCHECK,

	OP_STOREP_P,	// back to ones that we do use.	
	OP_PUSH,
	OP_POP,

	// version 8 started
	OP_NUMOPS,
};


typedef struct statement16_s
{
	word		op;
	short		a,b,c;

} dstatement16_t;

typedef struct statement32_s
{
	dword		op;
	long		a,b,c;

} dstatement32_t;

typedef struct ddef16_s
{
	word		type;		// if DEF_SAVEGLOBAL bit is set
					// the variable needs to be saved in savegames
	word		ofs;
	string_t		s_name;
} ddef16_t;

typedef struct ddef32_s
{
	dword		type;		// if DEF_SAVEGLOBAL bit is set
					// the variable needs to be saved in savegames
	dword		ofs;
	string_t		s_name;
} ddef32_t;

typedef struct fdef_s
{
	uint		type;		// if DEF_SAVEGLOBAL bit is set
					// the variable needs to be saved in savegames
	uint		ofs;
	uint		progsofs;		// used at loading time, so maching field offsets (unions/members) 
					// are positioned at the same runtime offset.
	char		*name;
} fdef_t;

typedef struct
{
	int		first_statement;	// negative numbers are builtins
	int		parm_start;
	int		locals;		// total ints of parms + locals
	
	int		profile;		// runtime
	
	string_t		s_name;
	string_t		s_file;		// source file defined in
	
	int		numparms;
	byte		parm_size[MAX_PARMS];
} dfunction_t;

typedef struct
{
	int		version;
	int		crc;		// check of header file
	
	uint		ofs_statements;	// comp 1
	uint		numstatements;	// statement 0 is an error

	uint		ofs_globaldefs;	// comp 2
	uint		numglobaldefs;
	
	uint		ofs_fielddefs;	// comp 4
	uint		numfielddefs;
	
	uint		ofs_functions;	// comp 8
	uint		numfunctions;	// function 0 is an empty
	
	uint		ofs_strings;	// comp 16
	uint		numstrings;	// first string is a null string

	uint		ofs_globals;	// comp 32
	uint		numglobals;
	
	uint		entityfields;

	// version 7 extensions
	uint		ofsfiles;		// source files always compressed
	uint		ofslinenums;	// numstatements big // comp 64
	uint		ofsbodylessfuncs;	// no comp
	uint		numbodylessfuncs;

	uint		ofs_types;	// comp 128
	uint		numtypes;
	uint		blockscompressed;	// who blocks are compressed

	int		header;		// strange "header", erh...
} dprograms_t;

typedef struct mfunction_s
{
	int		first_statement;	// negative numbers are builtins
	int		parm_start;
	int		locals;		// total ints of parms + locals

	// these are doubles so that they can count up to 54bits or so rather than 32bit

	double		profile;		// runtime
	double		builtinsprofile;	// cost of builtin functions called by this function
	double		callcount;	// times the functions has been called since the last profile call

	int		s_name;
	int		s_file;		// source file defined in

	int		numparms;
	byte		parm_size[MAX_PARMS];
} mfunction_t;

typedef struct
{
	char		filename[128];
	int		size;
	int		compsize;
	int		compmethod;
	int		ofs;

} includeddatafile_t;

typedef struct type_s
{
	etype_t		type;

	struct type_s	*parentclass;	// type_entity...
	struct type_s	*next;

	struct type_s	*aux_type;	// return type or field type
	struct type_s	*param;
	int		num_parms;	// -1 = variable args
	uint		ofs;		// inside a structure.
	uint		size;
	char		*name;
} type_t;

#endif//VPROGS_H