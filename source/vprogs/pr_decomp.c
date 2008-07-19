//=======================================================================
//			Copyright XashXT Group 2007 ©
//			pr_decomp.c - progs decompiler
//=======================================================================

#include "vprogs.h"
#include "mathlib.h"

type_t	**ofstype;
byte	*ofsflags;
file_t	*file;

char *PR_VarAtOfs( int ofs )
{
	static char	buf[MAX_INPUTLINE];
	int		typen;
	ddef_t		*def;

	if( ofsflags[ofs] & 8 ) def = PRVM_ED_GlobalAtOfs( ofs );
	else def = NULL;

	if( !def )
	{
		if( ofsflags[ofs] & 3 )
		{
			if( ofstype[ofs] ) com.sprintf( buf, "_v_%s_%i", ofstype[ofs]->name, ofs );
			else com.sprintf( buf, "_v_%i", ofs );
		}
		else
		{
			if( ofstype[ofs] )
			{
				typen = ofstype[ofs]->type;
				goto evaluate_immediate;
			}
			else com.sprintf( buf, "_c_%i", ofs );
		}
		return buf;
	}

	if(!PRVM_GetString( def->s_name ) || !com.strcmp(PRVM_GetString( def->s_name ), "IMMEDIATE" ))
	{
		if( vm.prog->types ) typen = vm.prog->types[def->type & ~DEF_SHARED].type;
		else typen = def->type & ~(DEF_SHARED|DEF_SAVEGLOBAL);
		
evaluate_immediate:
		switch( typen )
		{
		case ev_float:
			com.sprintf( buf, "%f", PRVM_G_FLOAT(ofs));
			return buf;
		case ev_vector:
			com.sprintf( buf, "\'%f %f %f\'", PRVM_G_FLOAT(ofs+0), PRVM_G_FLOAT(ofs+1), PRVM_G_FLOAT(ofs+2));
			return buf;
		case ev_string:
			{
				char	*s, *s2;

				s = buf;
				*s++ = '\"';
				s2 = vm.prog->strings + PRVM_G_INT( ofs );

				if( s2 )
				{
					while( *s2 )
					{
						if( *s2 == '\n' )
						{
							*s++ = '\\';
							*s++ = 'n';
							s2++;
						}
						else if( *s2 == '\"' )
						{
							*s++ = '\\';
							*s++ = '\"';
							s2++;
						}
						else if( *s2 == '\t' )
						{
							*s++ = '\\';
							*s++ = 't';
							s2++;
						}
						else *s++ = *s2++;
					}
					*s++ = '\"';
					*s++ = '\0';
				}
			}
			return buf;
		case ev_pointer:
			com.sprintf( buf, "_c_pointer_%i", ofs );
			return buf;
		default:
			com.sprintf( buf, "_c_%i", ofs );
			return buf;
		}
	}
	return (char *)PRVM_GetString( def->s_name );
}

int PR_ImmediateReadLater( uint ofs, int firstst )
{
	dstatement_t	*st;

	
	if( ofsflags[ofs] & 8 ) return false; // this is a global/local/pramater, not a temp
	if(!(ofsflags[ofs] & 3)) return false; // this is a constant.

	for( st = &((dstatement_t*)vm.prog->statements)[firstst]; ; st++, firstst++ )
	{	
		// if written, return false, if read, return true.
		if( st->op >= OP_CALL0 && st->op <= OP_CALL8 )
		{
			if( ofs == OFS_RETURN ) return false;
			if( ofs < OFS_PARM0 + 3*((uint)st->op - OP_CALL0 ))
				return true;
		}
		else if( pr_opcodes[st->op].associative == ASSOC_RIGHT )
		{
			if( ofs == st->b ) return false;
			if( ofs == st->a ) return true;
		}
		else
		{
			if( st->a == ofs ) return true;
			if( st->b == ofs ) return true;
			if( st->c == ofs ) return false;
		}

		// we missed our chance. (return/done ends any code coherancy).
		if( st->op == OP_DONE || st->op == OP_RETURN )
			return false;
	}
	return false;
}

int PR_ProductReadLater( int stnum )
{
	dstatement_t	*st = &((dstatement_t*)vm.prog->statements)[stnum];

	if( pr_opcodes[st->op].priority == -1 )
	{
		if( st->op >= OP_CALL0 && st->op <= OP_CALL7 )
			return PR_ImmediateReadLater( OFS_RETURN, stnum + 1 );
		return false; // these don't have products...
	}

	if( pr_opcodes[st->op].associative == ASSOC_RIGHT )
		return PR_ImmediateReadLater( st->b, stnum + 1 );
	else return PR_ImmediateReadLater( st->c, stnum + 1 );
}

/*
=======================
PR_WriteStatementProducingOfs

recursive, works backwards
=======================
*/
void PR_WriteStatementProducingOfs( int lastnum, int firstpossible, int ofs )
{
	dstatement_t	*st;
	ddef_t		*def;
	int		i;

	if( ofs == 0 ) longjmp( pr_parse_abort, 1 );

	for( ; lastnum >= firstpossible; lastnum-- )
	{
		st = &((dstatement_t*)vm.prog->statements)[lastnum];
		if( st->op >= OP_CALL0 && st->op < OP_CALL7 )
		{
			if( ofs != OFS_RETURN ) continue;
			PR_WriteStatementProducingOfs( lastnum - 1, firstpossible, st->a );
			FS_Printf( file, "(" );
			for( i = 0; i < st->op - OP_CALL0; i++ )
			{
				PR_WriteStatementProducingOfs( lastnum-1, firstpossible, OFS_PARM0 + i*3 );
				if( i != st->op - OP_CALL0 - 1 ) FS_Printf( file, ", " );
			}
			FS_Printf( file, ")" );
			return;
		}
		else if( pr_opcodes[st->op].associative == ASSOC_RIGHT )
		{
			if( st->b != ofs ) continue;
			if(!PR_ImmediateReadLater( st->b, lastnum + 1 ))
			{
				FS_Printf( file, "(" );
				PR_WriteStatementProducingOfs( lastnum - 1, firstpossible, st->b );
				FS_Printf( file, " " );
				FS_Printf( file, pr_opcodes[st->op].name );
				FS_Printf( file, " ");
				PR_WriteStatementProducingOfs( lastnum - 1, firstpossible, st->a );
				FS_Printf( file, ")" );
				return;
			}
			PR_WriteStatementProducingOfs( lastnum - 1, firstpossible, st->a );
			return;
		}
		else
		{
			if( st->c != ofs ) continue;

			if(!PR_ImmediateReadLater( st->c, lastnum + 1 ))
			{
				PR_WriteStatementProducingOfs( lastnum - 1, firstpossible, st->c );
				FS_Printf( file, " = " );
			}
			FS_Printf( file, "(" );
			PR_WriteStatementProducingOfs( lastnum-1, firstpossible, st->a );

			if( !com.strcmp( pr_opcodes[st->op].name, "." ))
				FS_Printf( file, pr_opcodes[st->op].name ); // extra spaces around .s are ugly.
			else
			{
				FS_Printf( file, " " );
				FS_Printf( file, pr_opcodes[st->op].name);
				FS_Printf( file, " " );
			}
			PR_WriteStatementProducingOfs( lastnum-1, firstpossible, st->b );
			FS_Printf( file, ")" );
			return;
		}
	}

	def = PRVM_ED_GlobalAtOfs( ofs );
	if( def )
	{
		if(!com.strcmp( PRVM_GetString( def->s_name ), "IMMEDIATE" ))
			FS_Printf( file, "%s", PR_VarAtOfs( ofs ));
		else FS_Printf( file, "%s", PRVM_GetString( def->s_name ));
	}
	else FS_Printf( file, "%s", PR_VarAtOfs( ofs ));
}

int PR_WriteStatement( int stnum, int firstpossible )
{
	int		count, skip;
	dstatement_t	*st = &((dstatement_t*)vm.prog->statements)[stnum];

	switch( st->op )
	{
	case OP_IFNOT:
		count = (signed short)st->b;
		FS_Printf(file, "if (");
		PR_WriteStatementProducingOfs( stnum, firstpossible, st->a );
		FS_Printf( file, ")\r\n" );
		FS_Printf( file, "{\r\n" );
		firstpossible = stnum + 1;
		count--;
		stnum++;
		while( count )
		{
			if(PR_ProductReadLater( stnum ))
			{
				count--;
				stnum++;
				continue;
			}
			skip = PR_WriteStatement( stnum, firstpossible );
			count -= skip;
			stnum += skip;
		}
		FS_Printf( file, "}\r\n" );
		st = &((dstatement_t*)vm.prog->statements)[stnum];
		if( st->op == OP_GOTO )
		{
			count = (signed short)st->b;
			count--;
			stnum++;

			FS_Printf( file, "else\r\n" );
			FS_Printf( file, "{\r\n" );
			while( count )
			{
				if( PR_ProductReadLater( stnum ))
				{
					count--;
					stnum++;
					continue;
				}
				skip = PR_WriteStatement( stnum, firstpossible );
				count -= skip;
				stnum += skip;
			}
			FS_Printf( file, "}\r\n" );
		}
		break;
	case OP_IF:
		longjmp( pr_parse_abort, 1 );
		break;
	case OP_GOTO:
		longjmp( pr_parse_abort, 1 );
		break;
	case OP_RETURN:
	case OP_DONE:
		if( st->a ) PR_WriteStatementProducingOfs( stnum - 1, firstpossible, st->a );
		break;
	case OP_CALL0:
	case OP_CALL1:
	case OP_CALL2:
	case OP_CALL3:
	case OP_CALL4:
	case OP_CALL5:
	case OP_CALL6:
	case OP_CALL7:
		PR_WriteStatementProducingOfs( stnum, firstpossible, OFS_RETURN );
		FS_Printf( file, ";\r\n" );
		break;
	default:
		if( pr_opcodes[st->op].associative == ASSOC_RIGHT )
			PR_WriteStatementProducingOfs( stnum, firstpossible, st->b );
		else PR_WriteStatementProducingOfs( stnum, firstpossible, st->c );
		FS_Printf( file, ";\r\n" );
		break;
	}
	return 1;
}

void PR_WriteAsmStatements( file_t *f, int num, const char *functionname )
{
	int		stn = vm.prog->functions[num].first_statement;
	int		fileofs, ofs, i;
	dstatement_t	*st = NULL;
	ddef_t		*def;
	opcode_t		*op;
	prvm_eval_t	*v;

	// we wrote this one...
	if( !functionname && stn < 0 ) return;

	if( stn >= 0 )
	{
		for( stn = vm.prog->functions[num].first_statement; stn < (signed int)vm.prog->progs->numstatements; stn++ )
		{
			st = &((dstatement_t*)vm.prog->statements)[stn];
			if( st->op == OP_DONE || st->op == OP_RETURN )
			{
				if( !st->a ) FS_Printf(f, "void(");
				else if( ofstype[st->a] )
				{
					FS_Printf( f, "%s", ofstype[st->a]->name );
					FS_Printf( f, "(" );
				}
				else FS_Printf( f, "function(" );
				break;
			}
		}
		st = NULL;
		stn = vm.prog->functions[num].first_statement;
	}
	else FS_Printf( f, "function(" );
	for( ofs = vm.prog->functions[num].parm_start, i = 0; i < vm.prog->functions[num].numparms; i++, ofs += vm.prog->functions[num].parm_size[i] )
	{
		ofsflags[ofs] |= 4;

		def = PRVM_ED_GlobalAtOfs( ofs );
		if( def && stn >= 0 )
		{
			if( st ) FS_Printf( f, ", " );
			st = (void *)0xffff;

			if( !PRVM_GetString( def->s_name ))
			{
				char	mem[64];
				com.sprintf( mem, "_p_%i", def->ofs );
				def->s_name = PRVM_SetTempString( mem );
			}
			
			if( vm.prog->types )
			{
				FS_Printf( f, "%s %s", vm.prog->types[def->type&~(DEF_SHARED|DEF_SAVEGLOBAL)].name, PRVM_GetString( def->s_name ));
			}
			else
				switch( def->type&~(DEF_SHARED|DEF_SAVEGLOBAL))
				{
				case ev_string:
					FS_Printf( f, "%s %s", "string", PRVM_GetString( def->s_name ));
					break;
				case ev_float:
					FS_Printf(f, "%s %s", "float", PRVM_GetString( def->s_name ));
					break;
				case ev_entity:
					FS_Printf(f, "%s %s", "entity", PRVM_GetString( def->s_name ));
					break;
				case ev_vector:
					FS_Printf(f, "%s %s", "vector", PRVM_GetString( def->s_name ));
					break;
				default:					
					FS_Printf(f, "%s %s", "unknown", PRVM_GetString( def->s_name ));
					break;
				}
		}
	}
	for( ofs = vm.prog->functions[num].parm_start + vm.prog->functions[num].numparms, i = vm.prog->functions[num].numparms; i < vm.prog->functions[num].locals; i++, ofs++ )
		ofsflags[ofs] |= 4;

	if( !PRVM_GetString( vm.prog->functions[num].s_name ))
	{
		if( !functionname )
		{
			char	mem[64];
			com.sprintf( mem, "_bi_%i", num );
			vm.prog->functions[num].s_name = PRVM_SetTempString( mem );
		}
		else vm.prog->functions[num].s_name = PRVM_SetTempString( functionname );
	}

	FS_Printf( f, ") %s", PRVM_GetString( vm.prog->functions[num].s_name ));

	if( stn < 0 )
	{
		stn *= -1;
		FS_Printf( f, " = #%i;\r\n", stn );
		return;
	}

	if( functionname )
	{
		// parsing defs
		FS_Printf(f, ";\r\n");
		return;
	}
	
	fileofs = FS_Seek( f, 0, SEEK_CUR );
	if( setjmp(pr_parse_abort))
	{
		FS_Printf( f, "*/\r\n" );
		FS_Printf( f, " = asm {\r\n" );

		stn = vm.prog->functions[num].first_statement;
		for( ofs = vm.prog->functions[num].parm_start + vm.prog->functions[num].numparms, i = vm.prog->functions[num].numparms; i < vm.prog->functions[num].locals; i++, ofs++ )
		{
			def = PRVM_ED_GlobalAtOfs( ofs );
			if( def )
			{	
				v = (prvm_eval_t *)&((int *)vm.prog->globals.gp)[def->ofs];
				if( vm.prog->types )
					FS_Printf( f, "\tlocal %s %s;\r\n", vm.prog->types[def->type&~(DEF_SHARED|DEF_SAVEGLOBAL)].name, PRVM_GetString( def->s_name ));
				else
				{
					if(!PRVM_GetString( def->s_name ))
					{
						char	mem[64];
						com.sprintf( mem, "_l_%i", def->ofs );
						def->s_name = PRVM_SetTempString( mem );
					}

					switch( def->type&~(DEF_SHARED|DEF_SAVEGLOBAL))
					{
					case ev_string:
						FS_Printf( f, "\tlocal %s %s;\r\n", "string", PRVM_GetString( def->s_name ));
						break;
					case ev_float:
						FS_Printf(f, "\tlocal %s %s;\r\n", "float", PRVM_GetString( def->s_name ));
						break;
					case ev_entity:
						FS_Printf(f, "\tlocal %s %s;\r\n", "entity", PRVM_GetString( def->s_name ));
						break;
					case ev_vector:
						if(!VectorIsNull( v->vector ))
							FS_Printf( f, "\tlocal vector %s = '%f %f %f';\r\n", PRVM_GetString( def->s_name ), v->vector[0], v->vector[1], v->vector[2] );
						else FS_Printf( f, "\tlocal %s %s;\r\n", "vector", PRVM_GetString( def->s_name ));
						ofs += 2;	// skip floats;
						break;
					default:					
						FS_Printf( f, "\tlocal %s %s;\r\n", "unknown", PRVM_GetString( def->s_name ));
						break;
					}
				}
			}
		}

		while( 1 )
		{
			st = &((dstatement_t*)vm.prog->statements)[stn];
			if( !st->op ) break; // end of function statement!

			op = &pr_opcodes[st->op];		
			FS_Printf( f, "\t%s", op->opname );

			if( op->priority == -1 && op->associative == ASSOC_RIGHT )
			{
				// last param is a goto
				if( op->type_b == &type_void )
				{
					if( st->a ) FS_Printf( f, " %i", (signed short)st->a );
				}
				else if( op->type_c == &type_void )
				{
					if( st->a ) FS_Printf( f, " %s", PR_VarAtOfs(st->a));
					if( st->b ) FS_Printf( f, " %i", (signed short)st->b );
				}
				else
				{
					if( st->a ) FS_Printf( f, " %s", PR_VarAtOfs(st->a));
					if( st->b ) FS_Printf( f, " %s", PR_VarAtOfs(st->b));
					if( st->c ) FS_Printf( f, " %i", (signed short)st->c ); // rightness means it uses a as c
				}
			}
			else
			{
				if( st->a )
				{
					if( op->type_a == NULL ) FS_Printf( f, " %i", (signed short)st->a );
					else FS_Printf(f, " %s", PR_VarAtOfs(st->a));
				}
				if( st->b )
				{
					if( op->type_b == NULL ) FS_Printf( f, " %i", (signed short)st->b );
					else FS_Printf( f, " %s", PR_VarAtOfs(st->b));
				}
				if( st->c && op->associative != ASSOC_RIGHT )
				{
					// rightness means it uses a as c
					if( op->type_c == NULL ) FS_Printf( f, " %i", (signed short)st->c );
					else FS_Printf( f, " %s", PR_VarAtOfs(st->c));
				}
			}
			FS_Printf( f, ";\r\n" );
			stn++;
		}
	}
	else
	{
		if( !com.strcmp(PRVM_GetString( vm.prog->functions[num].s_name ), "SUB_Remove" ))
			file = NULL;
		file = f;
		FS_Printf( f, "/*\r\n" );

		FS_Printf( f, " =\r\n{\r\n" );

		for( ofs = vm.prog->functions[num].parm_start + vm.prog->functions[num].numparms, i = vm.prog->functions[num].numparms; i < vm.prog->functions[num].locals; i++, ofs++ )
		{
			def = PRVM_ED_GlobalAtOfs( ofs );
			if( def )
			{	
				v = (prvm_eval_t *)&((int *)vm.prog->globals.gp)[def->ofs];
				if( vm.prog->types) FS_Printf( f, "\tlocal %s %s;\r\n", vm.prog->types[def->type&~(DEF_SHARED|DEF_SAVEGLOBAL)].name, PRVM_GetString( def->s_name ));
				else
				{
					if(!PRVM_GetString( def->s_name ))
					{
						char	mem[64];
						com.sprintf( mem, "_l_%i", def->ofs );
						def->s_name = PRVM_SetTempString( mem );
					}

					switch( def->type&~(DEF_SHARED|DEF_SAVEGLOBAL))
					{
					case ev_string:
						FS_Printf( f, "\tlocal %s %s;\r\n", "string", PRVM_GetString( def->s_name ));
						break;
					case ev_float:
						FS_Printf( f, "\tlocal %s %s;\r\n", "float", PRVM_GetString( def->s_name ));
						break;
					case ev_entity:
						FS_Printf( f, "\tlocal %s %s;\r\n", "entity", PRVM_GetString( def->s_name ));
						break;
					case ev_vector:
						if(!VectorIsNull( v->vector ))
							FS_Printf( f, "\tlocal vector %s = '%f %f %f';\r\n", PRVM_GetString( def->s_name ), v->vector[0], v->vector[1], v->vector[2] );
						else FS_Printf( f, "\tlocal %s %s;\r\n", "vector", PRVM_GetString( def->s_name ));
						ofs += 2;	// skip floats;
						break;
					default:					
						FS_Printf( f, "\tlocal %s %s;\r\n", "unknown", PRVM_GetString( def->s_name ));
						break;
					}
				}
			}
		}

		for( stn = vm.prog->functions[num].first_statement; stn < (signed int)vm.prog->progs->numstatements; stn++ )
		{
			if( PR_ProductReadLater( stn )) continue;
			st = &((dstatement_t*)vm.prog->statements)[stn];
			if( !st->op ) break;
			PR_WriteStatement( stn, vm.prog->functions[num].first_statement );
		}
		longjmp( pr_parse_abort, 1 );
	}
	FS_Printf( f, "};\r\n" );
}


void PR_FigureOutTypes( void )
{
	ddef_t		*def;
	opcode_t		*op;
	uint		i, p;
	dstatement_t	*st;
	int		parmofs[MAX_PARMS];
	
	ofstype	= Qalloc(sizeof(*ofstype ) * 65535);
	ofsflags	= Qalloc(sizeof(*ofsflags) * 65535);

	maxtypeinfos = 256;
	qcc_typeinfo = (void *)Qalloc( sizeof(type_t) * maxtypeinfos );
	numtypeinfos = 0;

	memset( ofstype, 0, sizeof(*ofstype)*65535);
	memset( ofsflags, 0, sizeof(*ofsflags)*65535);

	type_void = PR_NewType( "void", ev_void );
	type_string = PR_NewType( "string", ev_string );
	type_float = PR_NewType( "float", ev_float );
	type_vector = PR_NewType( "vector", ev_vector );
	type_entity = PR_NewType( "entity", ev_entity );
	type_field = PR_NewType( "field", ev_field );	
	type_function = PR_NewType( "function", ev_function );
	type_pointer = PR_NewType( "pointer", ev_pointer );	
	type_integer = PR_NewType( "integer", ev_integer );
	type_floatfield = PR_NewType("fieldfloat", ev_field);
	type_floatfield->aux_type = type_float;
	type_pointer->aux_type = PR_NewType( "pointeraux", ev_float );
	type_function->aux_type = type_void;

	for( i = 0, st = vm.prog->statements; i < vm.prog->progs->numstatements; i++, st++ )
	{
		op = &pr_opcodes[st->op];
		if( st->op >= OP_CALL1 && st->op <= OP_CALL8 )
		{
			for( p = 0; p < (uint)st->op - OP_CALL0; p++ )
				ofstype[parmofs[p]] = ofstype[OFS_PARM0 + p * 3];
		}
		else if( op->associative == ASSOC_RIGHT )
		{	
			// assignment
			ofsflags[st->b] |= 1;
			if( st->b >= OFS_PARM0 && st->b < RESERVED_OFS )
				parmofs[(st->b-OFS_PARM0)/3] = st->a;

			if( op->type_c && op->type_c != &type_void )
				ofstype[st->a] = *op->type_c;
			if( op->type_b && op->type_b != &type_void )
				ofstype[st->b] = *op->type_b;
		}
		else if( op->type_c )
		{
			ofsflags[st->c] |= 2;

			if( st->c >= OFS_PARM0 && st->b < RESERVED_OFS )	// too complicated
				parmofs[(st->b-OFS_PARM0)/3] = 0;
			if( op->type_a && op->type_a != &type_void )
				ofstype[st->a] = *op->type_a;
			if( op->type_b && op->type_b != &type_void )
				ofstype[st->b] = *op->type_b;
			if( op->type_c && op->type_c != &type_void )
				ofstype[st->c] = *op->type_c;
		}
	}
	for( i = 0; i < vm.prog->progs->numglobaldefs; i++ )
	{
		def = &vm.prog->globaldefs[i];
		ofsflags[def->ofs] |= 8;
		switch( def->type )
		{
		case ev_float:
			ofstype[def->ofs] = type_float;
			break;
		case ev_string:
			ofstype[def->ofs] = type_string;
			break;
		case ev_vector:
			ofstype[def->ofs] = type_vector;
			break;
		default: break;
		}
	}
}

bool PR_Decompile( void )
{
	uint		i, fld = 0;
	int		type;
	prvm_eval_t	*v;
	file_t		*f;

	if(!FS_FileExists( sourcefilename ))
		return false;

	PRVM_LoadProgs( sourcefilename, 0, NULL, 0, NULL );	
	f = FS_Open( "qcdtest/defs.qc", "w" );
	FS_Print( f, "// decompiled code can contain little type info.\r\n" );

	PR_FigureOutTypes();

	for( i = 1; i < vm.prog->progs->numglobaldefs; i++ )
	{
		if( !com.strcmp(PRVM_GetString( vm.prog->globaldefs[i].s_name ), "IMMEDIATE" )) 
			continue;

		if( ofsflags[vm.prog->globaldefs[i].ofs] & 4 )
			continue;	// this is a local.

		if( vm.prog->types )
			type = vm.prog->types[vm.prog->globaldefs[i].type & ~(DEF_SHARED|DEF_SAVEGLOBAL)].type;
		else type = vm.prog->globaldefs[i].type & ~(DEF_SHARED|DEF_SAVEGLOBAL);
		v = (prvm_eval_t *)&((int *)vm.prog->globals.gp)[vm.prog->globaldefs[i].ofs];

		if(!PRVM_GetString( vm.prog->globaldefs[i].s_name ))
		{
			char	mem[64];
			if( ofsflags[vm.prog->globaldefs[i].ofs] & 3 )
			{
				ofsflags[vm.prog->globaldefs[i].ofs] &= ~8;
				continue;	// this is a constant...
			}

			com.sprintf( mem, "_g_%i", vm.prog->globaldefs[i].ofs );
			vm.prog->globaldefs[i].s_name = PRVM_SetTempString( mem );
		}

		switch( type )
		{
		case ev_void:
			FS_Printf( f, "void %s;\r\n", PRVM_GetString(vm.prog->globaldefs[i].s_name ));
			break;
		case ev_string:
			if( v->string && *(vm.prog->strings + v->_int ))
				FS_Printf( f, "string %s = \"%s\";\r\n", PRVM_GetString(vm.prog->globaldefs[i].s_name ), PRVM_GetString( v->_int ));
			else FS_Printf( f, "string %s;\r\n", PRVM_GetString(vm.prog->globaldefs[i].s_name ));
			break;
		case ev_float:
			if( v->_float ) FS_Printf( f, "float %s = %f;\r\n", PRVM_GetString( vm.prog->globaldefs[i].s_name ), v->_float);
			else FS_Printf( f, "float %s;\r\n", PRVM_GetString(vm.prog->globaldefs[i].s_name ));
			break;
		case ev_vector:
			if(!VectorIsNull( v->vector ))
				FS_Printf( f, "vector %s = '%f %f %f';\r\n", PRVM_GetString(vm.prog->globaldefs[i].s_name ), v->vector[0], v->vector[1], v->vector[2]);
			else FS_Printf( f, "vector %s;\r\n", PRVM_GetString(vm.prog->globaldefs[i].s_name ));
			i += 3; // skip the floats
			break;
		case ev_entity:
			FS_Printf( f, "entity %s;\r\n", PRVM_GetString(vm.prog->globaldefs[i].s_name ));
			break;
		case ev_field:
			fld++;	// wierd
			if( !v->_int ) FS_Printf( f, "var " );
			switch( vm.prog->fielddefs[fld].type )
			{
			case ev_string:
				FS_Printf(f, ".string %s;", PRVM_GetString(vm.prog->globaldefs[i].s_name ));
				break;
			case ev_float:
				FS_Printf(f, ".float %s;", PRVM_GetString(vm.prog->globaldefs[i].s_name ));
				break;
			case ev_vector:
				FS_Printf(f, ".float %s;", PRVM_GetString(vm.prog->globaldefs[i].s_name ));
				break;
			case ev_entity:
				FS_Printf(f, ".float %s;", PRVM_GetString(vm.prog->globaldefs[i].s_name ));
				break;
			case ev_function:
				FS_Printf(f, ".void() %s;", PRVM_GetString(vm.prog->globaldefs[i].s_name ));
				break;
			default:
				FS_Printf(f, "field %s;", PRVM_GetString(vm.prog->globaldefs[i].s_name ));
				break;
			}
			if( v->_int ) FS_Printf( f, "/* %i */", v->_int );
			FS_Printf( f, "\r\n" );
			break;
		case ev_function:
			// wierd			
			PR_WriteAsmStatements( f, ((int *)vm.prog->globals.gp)[vm.prog->globaldefs[i].ofs], PRVM_GetString(vm.prog->globaldefs[i].s_name ));
			break;
		case ev_pointer:
			FS_Printf( f, "pointer %s;\r\n", PRVM_GetString(vm.prog->globaldefs[i].s_name ));
			break;
		case ev_integer:
			FS_Printf( f, "integer %s;\r\n", PRVM_GetString(vm.prog->globaldefs[i].s_name ));
			break;
		case ev_union:
			FS_Printf(f, "union %s;\r\n", PRVM_GetString(vm.prog->globaldefs[i].s_name ));
			break;
		case ev_struct:
			FS_Printf(f, "struct %s;\r\n", PRVM_GetString(vm.prog->globaldefs[i].s_name ));
			break;
		default:	break;
		}
	}
	for( i = 0; i < vm.prog->progs->numfunctions; i++ )
	{
		PR_WriteAsmStatements( f, i, NULL );
	}
	FS_Close( f );

	return true;
}