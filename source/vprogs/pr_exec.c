/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "vprogs.h"
#include "mathlib.h"

char *prvm_opnames[] =
{
"^5DONE",

"MUL_F",
"MUL_V",
"MUL_FV",
"MUL_VF",

"DIV",

"ADD_F",
"ADD_V",

"SUB_F",
"SUB_V",

"^2EQ_F",
"^2EQ_V",
"^2EQ_S",
"^2EQ_E",
"^2EQ_FNC",

"^2NE_F",
"^2NE_V",
"^2NE_S",
"^2NE_E",
"^2NE_FNC",

"^2LE",
"^2GE",
"^2LT",
"^2GT",

"^6FIELD_F",
"^6FIELD_V",
"^6FIELD_S",
"^6FIELD_ENT",
"^6FIELD_FLD",
"^6FIELD_FNC",

"^1ADDRESS",

"STORE_F",
"STORE_V",
"STORE_S",
"STORE_ENT",
"STORE_FLD",
"STORE_FNC",

"^1STOREP_F",
"^1STOREP_V",
"^1STOREP_S",
"^1STOREP_ENT",
"^1STOREP_FLD",
"^1STOREP_FNC",

"^5RETURN",

"^2NOT_F",
"^2NOT_V",
"^2NOT_S",
"^2NOT_ENT",
"^2NOT_FNC",
"^2NOT_BITI",
"^2NOT_BITF",

"^5IF",
"^5IFNOT",

"^3CALL0",
"^3CALL1",
"^3CALL2",
"^3CALL3",
"^3CALL4",
"^3CALL5",
"^3CALL6",
"^3CALL7",
"^3CALL8",
"^3CALL9",

"^1STATE",

"^5GOTO",

"^2AND",
"^2OR",

"BITAND",
"BITOR",
"MULSTORE_F",
"MULSTORE_V",
"MULSTOREP_F",
"MULSTOREP_V",

"DIVSTORE_F",
"DIVSTOREP_F",

"ADDSTORE_F",
"ADDSTORE_V",
"ADDSTOREP_F",
"ADDSTOREP_V",

"SUBSTORE_F",
"SUBSTORE_V",
"SUBSTOREP_F",
"SUBSTOREP_V",

"FETCH_GBL_F",
"FETCH_GBL_V",
"FETCH_GBL_S",
"FETCH_GBL_E",
"FETCH_G_FNC",

"^9CSTATE",
"^9CWSTATE",

"^6THINKTIME",

"^4BITSET",
"^4BITSETP",
"^4BITCLR",
"^4BITCLRP",

"^9RAND0",
"^9RAND1",
"^9RAND2",
"^9RANDV0",
"^9RANDV1",
"^9RANDV2",

"^6SWITCH_F",
"^6SWITCH_V",
"^6SWITCH_S",
"^6SWITCH_E",
"^6SWITCH_FNC",

"^6CASE",
"^6CASERANGE"
};

char *PRVM_GlobalString (int ofs);
char *PRVM_GlobalStringNoContents (int ofs);


//=============================================================================

/*
=================
PRVM_PrintStatement
=================
*/
extern cvar_t *prvm_statementprofiling;
void PRVM_PrintStatement (dstatement_t *s)
{
	size_t i;
	int opnum = (int)(s - vm.prog->statements);

	Msg("s%i: ", opnum);
	if( vm.prog->statement_linenums )
		Msg( "%s:%i: ", PRVM_GetString( vm.prog->xfunction->s_file ), vm.prog->statement_linenums[ opnum ] );

	if (prvm_statementprofiling->value)
		Msg("%7.0f ", vm.prog->statement_profile[s - vm.prog->statements]);

	if ( (unsigned)s->op < sizeof(prvm_opnames)/sizeof(prvm_opnames[0]))
	{
		Msg("%s ",  prvm_opnames[s->op]);
		i = strlen(prvm_opnames[s->op]);
		// don't count a preceding color tag when padding the name
		if (prvm_opnames[s->op][0] == STRING_COLOR_TAG)
			i -= 2;
		for ( ; i<10 ; i++)
			Msg(" ");
	}
	if (s->op == OP_IF || s->op == OP_IFNOT)
		Msg("%s, s%i",PRVM_GlobalString((unsigned short) s->a),(signed short)s->b + opnum);
	else if (s->op == OP_GOTO)
		Msg("s%i",(signed short)s->a + opnum);
	else if ( (unsigned)(s->op - OP_STORE_F) < 6)
	{
		Msg(PRVM_GlobalString((unsigned short) s->a));
		Msg(", ");
		Msg(PRVM_GlobalStringNoContents((unsigned short) s->b));
	}
	else if (s->op == OP_ADDRESS || (unsigned)(s->op - OP_LOAD_F) < 6)
	{
		if (s->a)
			Msg(PRVM_GlobalString((unsigned short) s->a));
		if (s->b)
		{
			Msg(", ");
			Msg(PRVM_GlobalStringNoContents((unsigned short) s->b));
		}
		if (s->c)
		{
			Msg(", ");
			Msg(PRVM_GlobalStringNoContents((unsigned short) s->c));
		}
	}
	else
	{
		if (s->a)
			Msg(PRVM_GlobalString((unsigned short) s->a));
		if (s->b)
		{
			Msg(", ");
			Msg(PRVM_GlobalString((unsigned short) s->b));
		}
		if (s->c)
		{
			Msg(", ");
			Msg(PRVM_GlobalStringNoContents((unsigned short) s->c));
		}
	}
	Msg("\n");
}

void PRVM_PrintFunctionStatements( const char *name )
{
	int i, firststatement, endstatement;
	mfunction_t *func;
	func = PRVM_ED_FindFunction (name);
	if (!func)
	{
		Msg("%s progs: no function named %s\n", PRVM_NAME, name);
		return;
	}
	firststatement = func->first_statement;
	if (firststatement < 0)
	{
		Msg("%s progs: function %s is builtin #%i\n", PRVM_NAME, name, -firststatement);
		return;
	}

	// find the end statement
	endstatement = vm.prog->progs->numstatements;
	for (i = 0;i < vm.prog->progs->numfunctions;i++)
		if (endstatement > vm.prog->functions[i].first_statement && firststatement < vm.prog->functions[i].first_statement)
			endstatement = vm.prog->functions[i].first_statement;

	// now print the range of statements
	Msg("%s progs: disassembly of function %s (statements %i-%i):\n", PRVM_NAME, name, firststatement, endstatement);
	for (i = firststatement;i < endstatement;i++)
	{
		PRVM_PrintStatement(vm.prog->statements + i);
		vm.prog->statement_profile[i] = 0;
	}
}

/*
============
PRVM_PrintFunction_f

============
*/
void PRVM_PrintFunction_f (void)
{
	if (Cmd_Argc() != 3)
	{
		Msg("usage: prvm_printfunction <program name> <function name>\n");
		return;
	}

	if(!PRVM_SetProgFromString(Cmd_Argv(1)))
		return;

	PRVM_PrintFunctionStatements(Cmd_Argv(2));

	vm.prog = NULL;
}

/*
============
PRVM_StackTrace
============
*/
void PRVM_StackTrace (void)
{
	mfunction_t	*f;
	int			i;

	vm.prog->stack[vm.prog->depth].s = vm.prog->xstatement;
	vm.prog->stack[vm.prog->depth].f = vm.prog->xfunction;
	for (i = vm.prog->depth;i > 0;i--)
	{
		f = vm.prog->stack[i].f;

		if (!f)
			Msg("<NULL FUNCTION>\n");
		else
			Msg("%12s : %s : statement %i\n", PRVM_GetString(f->s_file), PRVM_GetString(f->s_name), vm.prog->stack[i].s - f->first_statement);
	}
}


void PRVM_Profile (int maxfunctions, int mininstructions)
{
	mfunction_t *f, *best;
	int i, num;
	double max;

	Msg( "%s Profile:\n[CallCount] [Statements] [BuiltinCost]\n", PRVM_NAME );

	num = 0;
	do
	{
		max = 0;
		best = NULL;
		for (i=0 ; i<vm.prog->progs->numfunctions ; i++)
		{
			f = &vm.prog->functions[i];
			if (max < f->profile + f->builtinsprofile + f->callcount)
			{
				max = f->profile + f->builtinsprofile + f->callcount;
				best = f;
			}
		}
		if (best)
		{
			if (num < maxfunctions && max >= mininstructions)
			{
				if (best->first_statement < 0)
					Msg("%9.0f ----- builtin ----- %s\n", best->callcount, PRVM_GetString(best->s_name));
				else
					Msg("%9.0f %9.0f %9.0f %s\n", best->callcount, best->profile, best->builtinsprofile, PRVM_GetString(best->s_name));
			}
			num++;
			best->profile = 0;
			best->builtinsprofile = 0;
			best->callcount = 0;
		}
	} while (best);
}

/*
============
PRVM_Profile_f

============
*/
void PRVM_Profile_f (void)
{
	int howmany;

	howmany = 1<<30;
	if (Cmd_Argc() == 3)
		howmany = com.atoi(Cmd_Argv(2));
	else if (Cmd_Argc() != 2)
	{
		Msg("prvm_profile <program name>\n");
		return;
	}

	if(!PRVM_SetProgFromString(Cmd_Argv(1)))
		return;

	PRVM_Profile(howmany, 1);

	vm.prog = NULL;
}

void PRVM_CrashAll()
{
	int i;
	prvm_prog_t *oldprog = vm.prog;

	for(i = 0; i < PRVM_MAXPROGS; i++)
	{
		if(!PRVM_ProgLoaded(i))
			continue;
		PRVM_SetProg(i);
		PRVM_Crash();
	}
	vm.prog = oldprog;
}

void PRVM_PrintState(void)
{
	int i;
	if( vm.prog->xfunction )
	{
		for(i = -7; i <= 0;i++)
			if (vm.prog->xstatement + i >= vm.prog->xfunction->first_statement)
				PRVM_PrintStatement (vm.prog->statements + vm.prog->xstatement + i);
	}
	else
		Msg("null function executing??\n");
	PRVM_StackTrace ();
}

void PRVM_Crash()
{
	if( vm.prog == NULL )
		return;

	if( vm.prog->depth > 0 )
	{
		Msg("QuakeC crash report for %s:\n", PRVM_NAME);
		PRVM_PrintState();
	}

	// dump the stack so host_error can shutdown functions
	vm.prog->depth = 0;
	vm.prog->localstack_used = 0;

	// reset the prog pointer
	vm.prog = NULL;
}

/*
============================================================================
PRVM_ExecuteProgram

The interpretation main loop
============================================================================
*/

/*
====================
PRVM_EnterFunction

Returns the new program statement counter
====================
*/
int PRVM_EnterFunction( mfunction_t *f )
{
	int		i, j, c, o;

	if( !f ) PRVM_ERROR ("PRVM_EnterFunction: NULL function in %s", PRVM_NAME);

	vm.prog->stack[vm.prog->depth].s = vm.prog->xstatement;
	vm.prog->stack[vm.prog->depth].f = vm.prog->xfunction;
	vm.prog->depth++;
	if (vm.prog->depth >=PRVM_MAX_STACK_DEPTH)
		PRVM_ERROR( "stack overflow\n" );

	// save off any locals that the new function steps on
	c = f->locals;
	if (vm.prog->localstack_used + c > PRVM_LOCALSTACK_SIZE)
		PRVM_ERROR( "PRVM_ExecuteProgram: locals stack overflow in %s", PRVM_NAME );

	for( i = 0; i < c; i++ )
		vm.prog->localstack[vm.prog->localstack_used+i] = ((int *)vm.prog->globals.gp)[f->parm_start + i];
	vm.prog->localstack_used += c;

	// copy parameters
	o = f->parm_start;
	for (i=0 ; i<f->numparms ; i++)
	{
		for (j=0 ; j<f->parm_size[i] ; j++)
		{
			((int *)vm.prog->globals.gp)[o] = ((int *)vm.prog->globals.gp)[OFS_PARM0+i*3+j];
			o++;
		}
	}

	vm.prog->xfunction = f;
	return f->first_statement - 1;	// offset the s++
}

/*
====================
PRVM_LeaveFunction
====================
*/
int PRVM_LeaveFunction( void )
{
	int		i, c;

	if (vm.prog->depth <= 0)
		PRVM_ERROR ("prog stack underflow in %s", PRVM_NAME);

	if (!vm.prog->xfunction)
		PRVM_ERROR ("PR_LeaveFunction: NULL function in %s", PRVM_NAME);
	// restore locals from the stack
	c = vm.prog->xfunction->locals;
	vm.prog->localstack_used -= c;
	if (vm.prog->localstack_used < 0)
		PRVM_ERROR ("PRVM_ExecuteProgram: locals stack underflow in %s", PRVM_NAME);

	for (i=0 ; i < c ; i++)
		((int *)vm.prog->globals.gp)[vm.prog->xfunction->parm_start + i] = vm.prog->localstack[vm.prog->localstack_used+i];

	// up stack
	vm.prog->depth--;
	vm.prog->xfunction = vm.prog->stack[vm.prog->depth].f;
	return vm.prog->stack[vm.prog->depth].s;
}

void PRVM_Init_Exec(void)
{
	// dump the stack
	vm.prog->depth = 0;
	vm.prog->localstack_used = 0;
	// reset the string table
	// nothing here yet
}

/*
====================
PRVM_ExecuteProgram
====================
*/
// LordHavoc: optimized
#define OPA ((prvm_eval_t *)&vm.prog->globals.gp[(word)st->a])
#define OPB ((prvm_eval_t *)&vm.prog->globals.gp[(word)st->b])
#define OPC ((prvm_eval_t *)&vm.prog->globals.gp[(word)st->c])
extern cvar_t *prvm_boundscheck;
extern cvar_t *prvm_traceqc;
extern cvar_t *prvm_statementprofiling;
extern int PRVM_ED_FindFieldOffset (const char *field);
extern ddef_t* PRVM_ED_FindGlobal(const char *name);

void PRVM_ExecuteProgram( func_t fnum, const char *name, const char *file, const int line )
{
	dstatement_t	*st, *startst;
	mfunction_t	*f, *newf;
	pr_edict_t	*ed;
	prvm_eval_t	*ptr, *_switch;
	int		switchtype, exitdepth;
	int		i, jumpcount, cachedpr_trace;

	if( !fnum || fnum >= (uint)vm.prog->progs->numfunctions )
	{
		if( vm.prog->pev && PRVM_G_INT(vm.prog->pev->ofs) )
			PRVM_ED_Print(PRVM_PROG_TO_EDICT(PRVM_G_INT(vm.prog->pev->ofs)));
		PRVM_ERROR( "PRVM_ExecuteProgram: QC function %s is missing( called at %s:%i)\n", name, file, line );
		return;
	}

	f = &vm.prog->functions[fnum];

	vm.prog->trace = prvm_traceqc->value;

	// we know we're done when pr_depth drops to this
	exitdepth = vm.prog->depth;

	// make a stack frame
	st = &vm.prog->statements[PRVM_EnterFunction (f)];
	// save the starting statement pointer for profiling
	// (when the function exits or jumps, the (st - startst) integer value is
	// added to the function's profile counter)
	startst = st;
	// instead of counting instructions, we count jumps
	jumpcount = 0;
	// add one to the callcount of this function because otherwise engine-called functions aren't counted
	vm.prog->xfunction->callcount++;

chooseexecprogram:
	cachedpr_trace = vm.prog->trace;

	while( 1 )
	{
		st++;

		if( vm.prog->trace ) PRVM_PrintStatement(st);
		if( prvm_statementprofiling->value ) vm.prog->statement_profile[st - vm.prog->statements]++;

		switch( st->op )
		{
		case OP_ADD_F:
			OPC->_float = OPA->_float + OPB->_float;
			break;
		case OP_ADD_V:
			OPC->vector[0] = OPA->vector[0] + OPB->vector[0];
			OPC->vector[1] = OPA->vector[1] + OPB->vector[1];
			OPC->vector[2] = OPA->vector[2] + OPB->vector[2];
			break;
		case OP_SUB_F:
			OPC->_float = OPA->_float - OPB->_float;
			break;
		case OP_SUB_V:
			OPC->vector[0] = OPA->vector[0] - OPB->vector[0];
			OPC->vector[1] = OPA->vector[1] - OPB->vector[1];
			OPC->vector[2] = OPA->vector[2] - OPB->vector[2];
			break;
		case OP_MUL_F:
			OPC->_float = OPA->_float * OPB->_float;
			break;
		case OP_MUL_V:
			OPC->_float = OPA->vector[0]*OPB->vector[0] + OPA->vector[1]*OPB->vector[1] + OPA->vector[2]*OPB->vector[2];
			break;
		case OP_MUL_FV:
			OPC->vector[0] = OPA->_float * OPB->vector[0];
			OPC->vector[1] = OPA->_float * OPB->vector[1];
			OPC->vector[2] = OPA->_float * OPB->vector[2];
			break;
		case OP_MUL_VF:
			OPC->vector[0] = OPB->_float * OPA->vector[0];
			OPC->vector[1] = OPB->_float * OPA->vector[1];
			OPC->vector[2] = OPB->_float * OPA->vector[2];
			break;
		case OP_DIV_F:
			// don't divide by zero
			if( OPB->_float == 0.0f ) OPC->_float = 0.0f;
			else OPC->_float = OPA->_float / OPB->_float;
			break;
		case OP_DIV_VF:
			OPC->vector[0] = OPB->_float / OPA->vector[0];
			OPC->vector[1] = OPB->_float / OPA->vector[1];
			OPC->vector[2] = OPB->_float / OPA->vector[2];
			break;
		case OP_BITAND:
			OPC->_float = (float)((int)OPA->_float & (int)OPB->_float);
			break;
		case OP_BITOR:
			OPC->_float = (float)((int)OPA->_float | (int)OPB->_float);
			break;
		case OP_GE:
			OPC->_float = (float)OPA->_float >= OPB->_float;
			break;
		case OP_GE_I:
			OPC->_int = (int)(OPA->_int >= OPB->_int);
			break;
		case OP_GE_IF:
			OPC->_float = (float)(OPA->_int >= OPB->_float);
			break;
		case OP_GE_FI:
			OPC->_float = (float)(OPA->_float >= OPB->_int);
			break;
		case OP_LE:
			OPC->_float = OPA->_float <= OPB->_float;
			break;
		case OP_LE_I:
			OPC->_int = (int)(OPA->_int <= OPB->_int);
			break;
		case OP_LE_IF:
			OPC->_float = (float)(OPA->_int <= OPB->_float);
			break;
		case OP_LE_FI:
			OPC->_float = (float)(OPA->_float <= OPB->_int);
			break;
		case OP_GT:
			OPC->_float = OPA->_float > OPB->_float;
			break;
		case OP_GT_I:
			OPC->_int = (int)(OPA->_int > OPB->_int);
			break;
		case OP_GT_IF:
			OPC->_float = (float)(OPA->_int > OPB->_float);
			break;
		case OP_GT_FI:
			OPC->_float = (float)(OPA->_float > OPB->_int);
			break;
		case OP_LT:
			OPC->_float = OPA->_float < OPB->_float;
			break;
		case OP_LT_I:
			OPC->_int = (int)(OPA->_int < OPB->_int);
			break;
		case OP_LT_IF:
			OPC->_float = (float)(OPA->_int < OPB->_float);
			break;
		case OP_LT_FI:
			OPC->_float = (float)(OPA->_float < OPB->_int);
			break;
		case OP_AND:
			OPC->_float = OPA->_float && OPB->_float;
			break;
		case OP_OR:
			OPC->_float = OPA->_float || OPB->_float;
			break;
		case OP_NOT_F:
			OPC->_float = !OPA->_float;
			break;
		case OP_NOT_V:
			OPC->_float = !OPA->vector[0] && !OPA->vector[1] && !OPA->vector[2];
			break;
		case OP_NOT_S:
			OPC->_float = !OPA->string || !*PRVM_GetString(OPA->string);
			break;
		case OP_NOT_FNC:
			OPC->_float = !OPA->function;
			break;
		case OP_NOT_ENT:
			OPC->_float = (OPA->edict == 0);
			break;
		case OP_NOT_BITI:
			OPC->_int = ~OPA->_int;
			break;
		case OP_NOT_BITF:
			OPC->_float = (float)(~(int)OPA->_float);
			break;
		case OP_EQ_F:
			OPC->_float = (float)(OPA->_float == OPB->_float);
			break;
		case OP_EQ_IF:
			OPC->_float = (float)(OPA->_int == OPB->_float);
			break;
		case OP_EQ_FI:
			OPC->_float = (float)(OPA->_float == OPB->_int);
			break;
		case OP_EQ_V:
			OPC->_float = (OPA->vector[0] == OPB->vector[0]) && (OPA->vector[1] == OPB->vector[1]) && (OPA->vector[2] == OPB->vector[2]);
			break;
		case OP_EQ_S:
			if( OPA->string == OPB->string ) OPC->_float = true; // try fast method first
			else if( !OPA->string )
			{
				if( !OPB->string || !*PRVM_GetString( OPB->string ))
					OPC->_float = true;
				else OPC->_float = false;
			}
			else if( !OPB->string )
			{
				if( !OPA->string || !*PRVM_GetString( OPA->string ))
					OPC->_float = true;
				else OPC->_float = false;
			}
			else OPC->_float = !com.strcmp( PRVM_GetString( OPA->string ), PRVM_GetString( OPB->string ));
			break;
		case OP_EQ_E:
			OPC->_float = (float)(OPA->_int == OPB->_int);
			break;
		case OP_EQ_FNC:
			OPC->_float = OPA->function == OPB->function;
			break;
		case OP_NE_F:
			OPC->_float = (float)(OPA->_float != OPB->_float);
			break;
		case OP_NE_V:
			OPC->_float = (OPA->vector[0] != OPB->vector[0]) || (OPA->vector[1] != OPB->vector[1]) || (OPA->vector[2] != OPB->vector[2]);
			break;
		case OP_NE_S:
			if (OPA->string == OPB->string) OPC->_float = false; // try fast method first
			else if (!OPA->string)
			{
				if (!OPB->string || !*PRVM_GetString(OPB->string))
					OPC->_float = false;
				else OPC->_float = true;
			}
			else if (!OPB->string)
			{
				if (!OPA->string || !*PRVM_GetString(OPA->string))
					OPC->_float = false;
				else OPC->_float = true;
			}
			else OPC->_float = com.strcmp(PRVM_GetString(OPA->string), PRVM_GetString(OPB->string));
			break;
		case OP_NE_E:
			OPC->_float = (float)(OPA->_int != OPB->_int);
			break;
		case OP_NE_FNC:
			OPC->_float = (float)(OPA->function != OPB->function);
			break;
		case OP_STORE_IF:
			OPB->_float = (float)OPA->_int;
			break;
		case OP_STORE_FI:
			OPB->_int = (int)OPA->_float;
			break;
		case OP_STORE_F:
		case OP_STORE_I:
		case OP_STORE_ENT:
		case OP_STORE_FLD:		// integers
		case OP_STORE_S:
		case OP_STORE_FNC:		// pointers
			OPB->_int = OPA->_int;
			break;
		case OP_STORE_V:
			OPB->vector[0] = OPA->vector[0];
			OPB->vector[1] = OPA->vector[1];
			OPB->vector[2] = OPA->vector[2];
			break;
		case OP_STOREP_IF:
			PRVM_CHECK_PTR(OPB, 4);
			ptr = PRVM_ED_POINTER(OPB);
			ptr->_float = (float)OPA->_int;
			break;
		case OP_STOREP_FI:
			PRVM_CHECK_PTR(OPB, 4);
			ptr = PRVM_ED_POINTER(OPB);
			ptr->_int = (int)OPA->_float;
			break;
		case OP_STOREP_I:
			PRVM_CHECK_PTR(OPB, 4);
			ptr = PRVM_ED_POINTER(OPB);
			ptr->_int = OPA->_int;
			break;
		case OP_STOREP_F:
		case OP_STOREP_ENT:
		case OP_STOREP_FLD:		// integers
		case OP_STOREP_S:
		case OP_STOREP_FNC:		// pointers
			PRVM_CHECK_PTR(OPB, 4);
			ptr = PRVM_ED_POINTER(OPB);
			ptr->_int = OPA->_int;
			break;
		case OP_STOREP_V:
			PRVM_CHECK_PTR(OPB, 12);
			ptr = PRVM_ED_POINTER(OPB);
			ptr->vector[0] = OPA->vector[0];
			ptr->vector[1] = OPA->vector[1];
			ptr->vector[2] = OPA->vector[2];
			break;
		case OP_STOREP_C:	//store character in a string
			PRVM_CHECK_PTR(OPB, 4);
			ptr = PRVM_ED_POINTER(OPB);
			*(byte *)ptr = (char)OPA->_float;
			break;
		case OP_MULSTORE_F:
			OPB->_float *= OPA->_float;
			break;
		case OP_MULSTORE_V:
			OPB->vector[0] *= OPA->_float;
			OPB->vector[1] *= OPA->_float;
			OPB->vector[2] *= OPA->_float;
			break;
		case OP_MULSTOREP_F:
			PRVM_CHECK_PTR(OPB, 4);
			ptr = PRVM_ED_POINTER(OPB);
			OPC->_float = (ptr->_float *= OPA->_float);
			break;
		case OP_MULSTOREP_V:
			PRVM_CHECK_PTR(OPB, 12);
			ptr = PRVM_ED_POINTER(OPB);
			OPC->vector[0] = (ptr->vector[0] *= OPA->_float);
			OPC->vector[0] = (ptr->vector[1] *= OPA->_float);
			OPC->vector[0] = (ptr->vector[2] *= OPA->_float);
			break;
		case OP_DIVSTORE_F:
			OPB->_float /= OPA->_float;
			break;
		case OP_DIVSTOREP_F:
			PRVM_CHECK_PTR(OPB, 4);
			ptr = PRVM_ED_POINTER(OPB);
			OPC->_float = (ptr->_float /= OPA->_float);
			break;
		case OP_ADDSTORE_F:
			OPB->_float += OPA->_float;
			break;
		case OP_ADDSTORE_V:
			OPB->vector[0] += OPA->vector[0];
			OPB->vector[1] += OPA->vector[1];
			OPB->vector[2] += OPA->vector[2];
			break;
		case OP_ADDSTOREP_F:
			PRVM_CHECK_PTR(OPB, 4);
			ptr = PRVM_ED_POINTER(OPB);
			OPC->_float = (ptr->_float += OPA->_float);
			break;
		case OP_ADDSTOREP_V:
			PRVM_CHECK_PTR(OPB, 12);
			ptr = PRVM_ED_POINTER(OPB);
			OPC->vector[0] = (ptr->vector[0] += OPA->vector[0]);
			OPC->vector[1] = (ptr->vector[1] += OPA->vector[1]);
			OPC->vector[2] = (ptr->vector[2] += OPA->vector[2]);
			break;
		case OP_SUBSTORE_F:
			OPB->_float -= OPA->_float;
			break;
		case OP_SUBSTORE_V:
			OPB->vector[0] -= OPA->vector[0];
			OPB->vector[1] -= OPA->vector[1];
			OPB->vector[2] -= OPA->vector[2];
			break;
		case OP_SUBSTOREP_F:
			PRVM_CHECK_PTR(OPB, 4);
			ptr = PRVM_ED_POINTER(OPB);
			OPC->_float = (ptr->_float -= OPA->_float);
			break;
		case OP_SUBSTOREP_V:
			PRVM_CHECK_PTR(OPB, 12);
			ptr = PRVM_ED_POINTER(OPB);
			OPC->vector[0] = (ptr->vector[0] -= OPA->vector[0]);
			OPC->vector[1] = (ptr->vector[1] -= OPA->vector[1]);
			OPC->vector[2] = (ptr->vector[2] -= OPA->vector[2]);
			break;
		case OP_ADDRESS:
			if (prvm_boundscheck->value && ((uint)(OPB->_int) >= (uint)(vm.prog->progs->entityfields)))
			{
				vm.prog->xfunction->profile += (st - startst);
				vm.prog->xstatement = st - vm.prog->statements;
				PRVM_ERROR("%s attempted to address an invalid field (%i) in an edict", PRVM_NAME, OPB->_int);
				return;
			}
			if (OPA->edict == 0 && vm.prog->protect_world)
			{
				vm.prog->xfunction->profile += (st - startst);
				vm.prog->xstatement = st - vm.prog->statements;
				PRVM_ERROR("forbidden assignment to null/world entity in %s", PRVM_NAME);
				return;
			}
			ed = PRVM_PROG_TO_EDICT(OPA->edict);
			OPC->_int = (byte *)((int *)ed->progs.vp + OPB->_int) - (byte *)vm.prog->edictsfields;
			break;
		case OP_LOAD_I:
		case OP_LOAD_F:
		case OP_LOAD_FLD:
		case OP_LOAD_ENT:
		case OP_LOAD_S:
		case OP_LOAD_FNC:
			if (prvm_boundscheck->value && ((uint)(OPB->_int) >= (uint)(vm.prog->progs->entityfields)))
			{
				vm.prog->xfunction->profile += (st - startst);
				vm.prog->xstatement = st - vm.prog->statements;
				PRVM_ERROR("%s attempted to read an invalid field in an edict (%i)", PRVM_NAME, OPB->_int);
				return;
			}
			ed = PRVM_PROG_TO_EDICT(OPA->edict);
			OPC->_int = ((prvm_eval_t *)((int *)ed->progs.vp + OPB->_int))->_int;
			break;
		case OP_LOAD_V:
			if (prvm_boundscheck->value && (OPB->_int < 0 || OPB->_int + 2 >= vm.prog->progs->entityfields))
			{
				vm.prog->xfunction->profile += (st - startst);
				vm.prog->xstatement = st - vm.prog->statements;
				PRVM_ERROR("%s attempted to read an invalid field in an edict (%i)", PRVM_NAME, OPB->_int);
				return;
			}
			ed = PRVM_PROG_TO_EDICT(OPA->edict);
			OPC->vector[0] = ((prvm_eval_t *)((int *)ed->progs.vp + OPB->_int))->vector[0];
			OPC->vector[1] = ((prvm_eval_t *)((int *)ed->progs.vp + OPB->_int))->vector[1];
			OPC->vector[2] = ((prvm_eval_t *)((int *)ed->progs.vp + OPB->_int))->vector[2];
			break;
		case OP_IFNOTS:
			if (!OPA->string || !*PRVM_GetString(OPA->string))
			{
				vm.prog->xfunction->profile += (st - startst);
				st += st->b - 1;	// offset the s++
				startst = st;
				PRVM_CHECK_INFINITE();
			}
			break;
		case OP_IFNOT:
			if (!OPA->_int)
			{
				vm.prog->xfunction->profile += (st - startst);
				st += st->b - 1;	// offset the s++
				startst = st;
				PRVM_CHECK_INFINITE();
			}
			break;
		case OP_IFS:
			if (OPA->string && *PRVM_GetString(OPA->string))
			{
				vm.prog->xfunction->profile += (st - startst);
				st += st->b - 1;	// offset the s++
				startst = st;
				PRVM_CHECK_INFINITE();
			}
			break;
		case OP_IF:
			if (OPA->_int)
			{
				vm.prog->xfunction->profile += (st - startst);
				st += st->b - 1;	// offset the s++
				startst = st;
				PRVM_CHECK_INFINITE();
			}
			break;
		case OP_GOTO:
			vm.prog->xfunction->profile += (st - startst);
			st += st->a - 1;	// offset the s++
			startst = st;
			PRVM_CHECK_INFINITE();
			break;
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
			vm.prog->xfunction->profile += (st - startst);
			startst = st;
			vm.prog->xstatement = st - vm.prog->statements;
			vm.prog->argc = st->op - OP_CALL0;
			if( !OPA->function )
				PRVM_ERROR( "NULL function in %s (%s)(called at %s:%i)\n", name, PRVM_NAME, file, line );

			newf = &vm.prog->functions[OPA->function];
			newf->callcount++;

			if( newf->first_statement < 0 )
			{
				// negative statements are built in functions
				int builtinnumber = -newf->first_statement;
				vm.prog->xfunction->builtinsprofile++;
				if (builtinnumber < vm.prog->numbuiltins && vm.prog->builtins[builtinnumber])
					vm.prog->builtins[builtinnumber]();
				else PRVM_ERROR( "No such builtin #%i in %s\n", builtinnumber, PRVM_NAME );
			}
			else st = vm.prog->statements + PRVM_EnterFunction(newf);
			startst = st;
			break;
		case OP_DONE:
		case OP_RETURN:
			vm.prog->xfunction->profile += (st - startst);
			vm.prog->xstatement = st - vm.prog->statements;

			vm.prog->globals.gp[OFS_RETURN+0] = vm.prog->globals.gp[(word) st->a+0];
			vm.prog->globals.gp[OFS_RETURN+1] = vm.prog->globals.gp[(word) st->a+1];
			vm.prog->globals.gp[OFS_RETURN+2] = vm.prog->globals.gp[(word) st->a+2];

			st = vm.prog->statements + PRVM_LeaveFunction();
			startst = st;
			if (vm.prog->depth <= exitdepth) return; // all done
			if (vm.prog->trace != cachedpr_trace) goto chooseexecprogram;
			break;
		case OP_STATE:
			if(vm.prog->flag & PRVM_OP_STATE)
			{
				ed = PRVM_PROG_TO_EDICT(PRVM_G_INT(vm.prog->pev->ofs));
				PRVM_E_FLOAT(ed, PRVM_ED_FindField ("nextthink")->ofs) = *vm.prog->time + 0.1;
				PRVM_E_FLOAT(ed, PRVM_ED_FindField ("frame")->ofs) = OPA->_float;
				*(func_t *)((float*)ed->progs.vp + PRVM_ED_FindField ("think")->ofs) = OPB->function;
			}
			else
			{
				vm.prog->xfunction->profile += (st - startst);
				vm.prog->xstatement = st - vm.prog->statements;
				PRVM_ERROR("OP_STATE not supported by %s", PRVM_NAME);
			}
			break;
		case OP_ADD_I:		
			OPC->_int = OPA->_int + OPB->_int;
			break;
		case OP_ADD_FI:
			OPC->_float = OPA->_float + (float)OPB->_int;
			break;
		case OP_ADD_IF:
			OPC->_float = (float)OPA->_int + OPB->_float;
			break;
		case OP_SUB_I:
			OPC->_int = OPA->_int - OPB->_int;
			break;
		case OP_SUB_FI:
			OPC->_float = OPA->_float - (float)OPB->_int;
			break;
		case OP_SUB_IF:
			OPC->_float = (float)OPA->_int - OPB->_float;
			break;
		case OP_CONV_ITOF:
			OPC->_float = (float)OPA->_int;
			break;
		case OP_CONV_FTOI:
			OPC->_int = (int)OPA->_float;
			break;
		case OP_CP_ITOF:
			ptr = PRVM_EV_POINTER(OPA);
			OPC->_float = (float)ptr->_int;
			break;
		case OP_CP_FTOI:
			ptr = PRVM_EV_POINTER(OPA);
			OPC->_int = (int)ptr->_float;
			break;
		case OP_BITAND_I:
			OPC->_int = (OPA->_int & OPB->_int);
			break;
		case OP_BITOR_I:
			OPC->_int = (OPA->_int | OPB->_int);
			break;
		case OP_MUL_I:		
			OPC->_int = OPA->_int * OPB->_int;
			break;
		case OP_DIV_I:
			// don't divide by zero
			if (OPB->_int == 0) OPC->_int = 0;
			else OPC->_int = OPA->_int / OPB->_int;
			break;
		case OP_EQ_I:
			OPC->_int = (OPA->_int == OPB->_int);
			break;
		case OP_NE_I:
			OPC->_int = (OPA->_int != OPB->_int);
			break;
		case OP_GLOBAL_ADD:
			ed = PRVM_PROG_TO_EDICT(OPA->edict);
			OPC->_int = (byte *)((int *)OPB->_int) - (byte *)vm.prog->edictsfields;
			break;
		case OP_POINTER_ADD:
			OPC->_int = OPA->_int + OPB->_int * 4;
			break;
		case OP_LOADA_I:
		case OP_LOADA_F:
		case OP_LOADA_FLD:
		case OP_LOADA_ENT:
		case OP_LOADA_S:
		case OP_LOADA_FNC:
			ptr = (prvm_eval_t *)(&OPA->_int + OPB->_int);
			OPC->_int = ptr->_int;
			break;
		case OP_LOADA_V:
			ptr = (prvm_eval_t *)(&OPA->_int + OPB->_int);
			OPC->vector[0] = ptr->vector[0];
			OPC->vector[1] = ptr->vector[1];
			OPC->vector[2] = ptr->vector[2];
			break;
		case OP_ADD_SF:
			OPC->_int = OPA->_int + (int)OPB->_float;
			break;
		case OP_SUB_S:
			OPC->_int = OPA->_int - OPB->_int;
			break;
		case OP_LOADP_C:
			ptr = PRVM_EM_POINTER(OPA->_int + (int)OPB->_float);
			OPC->_float = *(byte *)ptr;
			break;
		case OP_LOADP_I:
		case OP_LOADP_F:
		case OP_LOADP_FLD:
		case OP_LOADP_ENT:
		case OP_LOADP_S:
		case OP_LOADP_FNC:
			ptr = PRVM_EM_POINTER(OPA->_int + OPB->_int);
			OPC->_int = ptr->_int;
			break;
		case OP_LOADP_V:
			ptr = PRVM_EM_POINTER(OPA->_int + OPB->_int);
			OPC->vector[0] = ptr->vector[0];
			OPC->vector[1] = ptr->vector[1];
			OPC->vector[2] = ptr->vector[2];
			break;
		case OP_POWER_I:
			OPC->_int = OPA->_int ^ OPB->_int;
			break;
		case OP_RSHIFT_I:
			OPC->_int = OPA->_int >> OPB->_int;
			break;
		case OP_LSHIFT_I:
			OPC->_int = OPA->_int << OPB->_int;
			break;
		case OP_RSHIFT_F:
			OPC->_float = (float)((int)OPA->_float >> (int)OPB->_float);
			break;
		case OP_LSHIFT_F:
			OPC->_float = (float)((int)OPA->_float << (int)OPB->_float);
			break;
		case OP_FETCH_GBL_F:
		case OP_FETCH_GBL_S:
		case OP_FETCH_GBL_E:
		case OP_FETCH_G_FNC:
			i = (int)OPB->_float;
			if(prvm_boundscheck->value && (i < 0 || i > ((prvm_eval_t *)&vm.prog->globals.gp[st->a-1])->_int))
			{
				vm.prog->xfunction->profile += (st - startst);
				vm.prog->xstatement = st - vm.prog->statements;
				PRVM_ERROR ("%s Progs array index out of bounds", PRVM_NAME);
				return;
			}
			ptr = (prvm_eval_t *)&vm.prog->globals.gp[(word)st->a + i];
			OPC->_int = ptr->_int;
			break;
		case OP_FETCH_GBL_V:
			i = (int)OPB->_float;
			if(prvm_boundscheck->value && (i < 0 || i > ((prvm_eval_t *)&vm.prog->globals.gp[st->a-1])->_int))
			{
				vm.prog->xfunction->profile += (st - startst);
				vm.prog->xstatement = st - vm.prog->statements;
				PRVM_ERROR ("%s Progs array index out of bounds", PRVM_NAME);
				return;
			}
			ptr = (prvm_eval_t *)&vm.prog->globals.gp[(word)st->a + ((int)OPB->_float)*3];
			OPC->vector[0] = ptr->vector[0];
			OPC->vector[1] = ptr->vector[1];
			OPC->vector[2] = ptr->vector[2];
			break;
		case OP_BITSET:
			OPB->_float = (float)((int)OPB->_float | (int)OPA->_float);
			break;
		case OP_BITSETP:
			ptr = PRVM_ED_POINTER(OPB);
			ptr->_float = (float)((int)ptr->_float | (int)OPA->_float);
			break;
		case OP_BITCLR:
			OPB->_float = (float)((int)OPB->_float & ((int)OPA->_float));
			break;
		case OP_BITCLRP:
			ptr = PRVM_ED_POINTER(OPB);
			ptr->_float = (float)((int)ptr->_float & ((int)OPA->_float));
			break;
		case OP_SWITCH_F:
		case OP_SWITCH_V:
		case OP_SWITCH_S:
		case OP_SWITCH_E:
		case OP_SWITCH_FNC:
			_switch = OPA;
			switchtype = st->op;
			vm.prog->xfunction->profile += (st - startst);
			st += st->b - 1;	// offset the s++
			startst = st;
			PRVM_CHECK_INFINITE();
			break;
		case OP_CASE:
			switch(switchtype)
			{
			case OP_SWITCH_F:
				if (_switch->_float == OPA->_float)
				{
					vm.prog->xfunction->profile += (st - startst);
					st += st->b - 1;	// offset the s++
					startst = st;
					PRVM_CHECK_INFINITE();
				}
				break;
			case OP_SWITCH_E:
			case OP_SWITCH_FNC:
				if (_switch->_int == OPA->_int)
				{
					vm.prog->xfunction->profile += (st - startst);
					st += st->b - 1;	// offset the s++
					startst = st;
					PRVM_CHECK_INFINITE();
				}
				break;
			case OP_SWITCH_S:
				if (_switch->_int == OPA->_int)
				{
					vm.prog->xfunction->profile += (st - startst);
					st += st->b - 1;	// offset the s++
					startst = st;
					PRVM_CHECK_INFINITE();
				}
				if((!_switch->_int && PRVM_GetString(OPA->string)) || (!OPA->_int && PRVM_GetString(_switch->string)))
					break;
				if(!com.strcmp(PRVM_GetString(_switch->string), PRVM_GetString(OPA->string)))
				{
					vm.prog->xfunction->profile += (st - startst);
					st += st->b - 1;	// offset the s++
					startst = st;
					PRVM_CHECK_INFINITE();
				}
				break;
			case OP_SWITCH_V:
				if (_switch->vector[0] == OPA->vector[0] && _switch->vector[1] == OPA->vector[1] && _switch->vector[2] == OPA->vector[2])
				{
					vm.prog->xfunction->profile += (st - startst);
					st += st->b - 1;	// offset the s++
					startst = st;
					PRVM_CHECK_INFINITE();
				}
				break;
			default:
				PRVM_ERROR ("%s Progs OP_CASE with bad/missing OP_SWITCH", PRVM_NAME);
				break;
			}
			break;
		case OP_CASERANGE:
			switch(switchtype)
			{
			case OP_SWITCH_F:
				if (_switch->_float >= OPA->_float && _switch->_float <= OPB->_float)
				{
					vm.prog->xfunction->profile += (st - startst);
					st += st->b - 1;	// offset the s++
					startst = st;
					PRVM_CHECK_INFINITE();
				}
				break;
			default:
				PRVM_ERROR ("%s Progs OP_CASERANGE with bad/missing OP_SWITCH", PRVM_NAME);
				break;
			}
			break;
		case OP_BITAND_IF:
			OPC->_int = (OPA->_int & (int)OPB->_float);
			break;
		case OP_BITOR_IF:
			OPC->_int = (OPA->_int | (int)OPB->_float);
			break;
		case OP_BITAND_FI:
			OPC->_int = ((int)OPA->_float & OPB->_int);
			break;
		case OP_BITOR_FI:
			OPC->_int = ((int)OPA->_float | OPB->_int);
			break;
		case OP_MUL_IF:
			OPC->_float = (OPA->_int * OPB->_float);
			break;
		case OP_MUL_FI:
			OPC->_float = (OPA->_float * OPB->_int);
			break;
		case OP_MUL_VI:
			OPC->vector[0] = OPA->vector[0] * OPB->_int;
			OPC->vector[1] = OPA->vector[0] * OPB->_int;
			OPC->vector[2] = OPA->vector[0] * OPB->_int;
			break;
		case OP_MUL_IV:
			OPC->vector[0] = OPB->_int * OPA->vector[0];
			OPC->vector[1] = OPB->_int * OPA->vector[1];
			OPC->vector[2] = OPB->_int * OPA->vector[2];
			break;
		case OP_DIV_IF:
			OPC->_float = (OPA->_int / OPB->_float);
			break;
		case OP_DIV_FI:
			OPC->_float = (OPA->_float / OPB->_int);
			break;
		case OP_AND_I:
			OPC->_int = (OPA->_int && OPB->_int);
			break;
		case OP_OR_I:
			OPC->_int = (OPA->_int || OPB->_int);
			break;
		case OP_AND_IF:
			OPC->_int = (OPA->_int && OPB->_float);
			break;
		case OP_OR_IF:
			OPC->_int = (OPA->_int || OPB->_float);
			break;
		case OP_AND_FI:
			OPC->_int = (OPA->_float && OPB->_int);
			break;
		case OP_OR_FI:
			OPC->_int = (OPA->_float || OPB->_int);
			break;
		case OP_NOT_I:
			OPC->_int = !OPA->_int;
			break;
		case OP_NE_IF:
			OPC->_int = (OPA->_int != OPB->_float);
			break;
		case OP_NE_FI:
			OPC->_int = (OPA->_float != OPB->_int);
			break;
		case OP_GSTOREP_I:
		case OP_GSTOREP_F:
		case OP_GSTOREP_ENT:
		case OP_GSTOREP_FLD:		// integers
		case OP_GSTOREP_S:
		case OP_GSTOREP_FNC:		// pointers
			if (prvm_boundscheck->value && (OPB->_int < 0 || OPB->_int >= (uint)vm.prog->progs->numglobaldefs))
			{
				vm.prog->xfunction->profile += (st - startst);
				vm.prog->xstatement = st - vm.prog->statements;
				PRVM_ERROR ("%s Progs attempted to write to an invalid indexed global", PRVM_NAME);
				return;
			}
			vm.prog->globals.gp[OPB->_int] = OPA->_float;
			break;
		case OP_GSTOREP_V:
			if (prvm_boundscheck->value && (OPB->_int < 0 || OPB->_int + 2 >= (uint)vm.prog->progs->numglobaldefs))
			{
				vm.prog->xfunction->profile += (st - startst);
				vm.prog->xstatement = st - vm.prog->statements;
				PRVM_ERROR ("%s Progs attempted to write to an invalid indexed global", PRVM_NAME);
				return;
			}
			vm.prog->globals.gp[OPB->_int+0] = OPA->vector[0];
			vm.prog->globals.gp[OPB->_int+1] = OPA->vector[1];
			vm.prog->globals.gp[OPB->_int+2] = OPA->vector[2];
			break;
		case OP_GADDRESS:
			i = OPA->_int + (int)OPB->_float;
			if (prvm_boundscheck->value && (i < 0 || i >= (uint)vm.prog->progs->numglobaldefs))
			{
				vm.prog->xfunction->profile += (st - startst);
				vm.prog->xstatement = st - vm.prog->statements;
				PRVM_ERROR ("%s Progs attempted to address an out of bounds global", PRVM_NAME);
				return;
			}
			OPC->_float = vm.prog->globals.gp[i];
			break;	
		case OP_GLOAD_I:
		case OP_GLOAD_F:
		case OP_GLOAD_FLD:
		case OP_GLOAD_ENT:
		case OP_GLOAD_S:
		case OP_GLOAD_FNC:
			if (prvm_boundscheck->value && (OPA->_int < 0 || OPA->_int >= (uint)vm.prog->progs->numglobaldefs))
			{
				vm.prog->xfunction->profile += (st - startst);
				vm.prog->xstatement = st - vm.prog->statements;
				PRVM_ERROR ("%s Progs attempted to read an invalid indexed global", PRVM_NAME);
				return;
			}
			OPC->_float = vm.prog->globals.gp[OPA->_int];
			break;
		case OP_GLOAD_V:
			if (prvm_boundscheck->value && (OPA->_int < 0 || OPA->_int + 2 >= (uint)vm.prog->progs->numglobaldefs))
			{
				vm.prog->xfunction->profile += (st - startst);
				vm.prog->xstatement = st - vm.prog->statements;
				PRVM_ERROR ("%s Progs attempted to read an invalid indexed global", PRVM_NAME);
				return;
			}
			OPC->vector[0] = vm.prog->globals.gp[OPA->_int+0];
			OPC->vector[1] = vm.prog->globals.gp[OPA->_int+1];
			OPC->vector[2] = vm.prog->globals.gp[OPA->_int+2];
			break;
		case OP_BOUNDCHECK:
			if (OPA->_int < 0 || OPA->_int >= st->b)
			{
				vm.prog->xfunction->profile += (st - startst);
				vm.prog->xstatement = st - vm.prog->statements;
				PRVM_ERROR ("%s Progs boundcheck failed at line number %d, value is < 0 or >= %d", PRVM_NAME, st->b, st->c);
				return;
			}
			break;
		case OP_CSTATE:
			if( vm.prog->flag & PRVM_OP_STATE )
			{
				ed = PRVM_PROG_TO_EDICT(PRVM_G_INT(vm.prog->pev->ofs));
				PRVM_E_FLOAT(ed, PRVM_ED_FindField ("nextthink")->ofs) = *vm.prog->time + 0.1f;
				PRVM_E_FLOAT(ed, PRVM_ED_FindField ("frame")->ofs) = OPA->_float;
				*(func_t *)((float*)ed->progs.vp + PRVM_ED_FindField ("think")->ofs) = OPB->function;
			}
			else
			{
				vm.prog->xfunction->profile += (st - startst);
				vm.prog->xstatement = st - vm.prog->statements;
				PRVM_ERROR("OP_STATE not supported by %s", PRVM_NAME);
			}
			break;
		case OP_THINKTIME:
			if( vm.prog->flag & PRVM_OP_THINKTIME )
			{
				ed = PRVM_PROG_TO_EDICT(PRVM_G_INT(vm.prog->pev->ofs));
				PRVM_E_FLOAT(ed, PRVM_ED_FindField ("nextthink")->ofs) = *vm.prog->time + OPB->_float;
			}
			else
			{
				vm.prog->xfunction->profile += (st - startst);
				vm.prog->xstatement = st - vm.prog->statements;
				PRVM_ERROR("OP_THINKTIME not supported by %s", PRVM_NAME);
			}
			break;
		case OP_MODULO_I:
			OPC->_int = (int)(OPA->_int % OPB->_int);
		case OP_MODULO_F:
			OPC->_float = fmod( OPA->_float, OPB->_float );
			break;
		default:
			vm.prog->xfunction->profile += (st - startst);
			vm.prog->xstatement = st - vm.prog->statements;
			PRVM_ERROR( "Bad opcode %i in %s (called at %s:%i)\n", st->op, PRVM_NAME, file, line );
		}
	}
}
