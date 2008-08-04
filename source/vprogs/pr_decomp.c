//=======================================================================
//			Copyright XashXT Group 2007 ©
//			pr_decomp.c - progs decompiler
//=======================================================================

#include "vprogs.h"
#include "mathlib.h"

type_t	**ofstype;
byte	*ofsflags;
file_t	*file;
file_t	*progs_src;
string	synth_name;
int	fake_name;
int	file_num;
char	*src_filename[1024];
char	*src_function[16384];
jmp_buf	pr_decompile_abort;

// stupid legacy
static char *pr_filenames[] =
{
	"makevectors",		"defs.qc",
	"button_wait",		"buttons.qc",
	"anglemod",		"ai.qc",
	"boss_face",		"boss.qc",
	"info_intermission",	"client.qc",
	"CanDamage",		"combat.qc",
	"demon1_stand1",		"demon.qc",
	"dog_bite",		"dog.qc",
	"door_blocked",		"doors.qc",
	"Laser_Touch",		"enforcer.qc",
	"knight_attack",		"fight.qc",
	"f_stand1",		"fish.qc",
	"hknight_shot",		"hknight.qc",
	"SUB_regen",		"items.qc",
	"knight_stand1",		"knight.qc",
	"info_null",		"misc.qc",
	"monster_use",		"monsters.qc",
	"OgreGrenadeExplode",	"ogre.qc",
	"old_idle1",	 	"oldone.qc",
	"plat_spawn_inside_trigger",	"plats.qc",
	"player_stand1",		"player.qc",
	"shal_stand",		"shalrath.qc",
	"sham_stand1",		"shambler.qc",
	"army_stand1",		"soldier.qc",
	"SUB_Null",		"subs.qc",
	"tbaby_stand1",		"tarbaby.qc",
	"trigger_reactivate",	"triggers.qc",
	"W_Precache",		"weapons.qc",
	"LaunchMissile",		"wizard.qc",
	"main",			"world.qc",
	"zombie_stand1",		"zombie.qc"
};

static char *pr_builtins[] =
{
NULL,
"void (vector ang)",
"void (entity e, vector o)",
"void (entity e, string m)",
"void (entity e, vector min, vector max)",
NULL,
"void ()",
"float ()",
"void (entity e, float chan, string samp, float vol, float atten)",
"vector (vector v)",
"void (string e)",
"void (string e)",
"float (vector v)",
"float (vector v)",
"entity ()",
"void (entity e)",
"void (vector v1, vector v2, float nomonsters, entity forent)",
"entity ()",
"entity (entity start, .string fld, string match)",
"string (string s)",
"string (string s)",
"void (entity client, string s)",
"entity (vector org, float rad)",
"void (string s)",
"void (entity client, string s)",
"void (string s)",
"string (float f)",
"string (vector v)",
"void ()",
"void ()",
"void ()",
"void (entity e)",
"float (float yaw, float dist)",
NULL,
"float (float yaw, float dist)",
"void (float style, string value)",
"float (float v)",
"float (float v)",
"float (float v)",
NULL,
"float (entity e)",
"float (vector v)",
NULL,
"float (float f)",
"vector (entity e, float speed)",
"float (string s)",
"void (string s)",
"entity (entity e)",
"void (vector o, vector d, float color, float count)",
"void ()",
NULL,
"vector (vector v)",
"void (float to, float f)",
"void (float to, float f)",
"void (float to, float f)",
"void (float to, float f)",
"void (float to, float f)",
"void (float to, float f)",
"void (float to, string s)",
"void (float to, entity s)",
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
"void (float step)",
"string (string s)",
"void (entity e)",
"void (string s)",
NULL,
"void (string var, string val)",
"void (entity client, string s, ...)",
"void (vector pos, string samp, float vol, float atten)",
"string (string s)",
"string (string s)",
"string (string s)",
"void (entity e)",
"void(entity killer, entity killee)",
"string(entity e, string key)",
"float(string s)",
"void(vector where, float set)"
};

bool PR_SynthName( const char *first )
{
	int i;

	// try to figure out the filename
	// based on the first function in the file
	for( i = 0; i < 62; i += 2 )
	{
		if(!com.strcmp( pr_filenames[i], first ))
		{
			com.sprintf( synth_name, pr_filenames[i + 1] );
			return true;
		}
	}
	return false;
}

bool PR_AlreadySeen( char *fname )
{
	int	i;

	if( file_num >= 1024 ) Sys_Break( "Error: too many source files.\n" );
	for( i = 0; i < file_num; i++ )
	{
		if( !com.strcmp( fname, src_filename[i]))
			return 1;
	}

	src_filename[file_num++] = copystring( fname );
	return 0;
}

char *PR_TempType( word temp, dstatement_t *start, mfunction_t *mf )
{
	int		i;
	dstatement_t	*stat;
	opcode_t		*op;

	stat = start - 1;
	op  = &pr_opcodes[stat->op];

	// determine the type of a temp
	while( stat > statements )
	{
		if( temp == stat->a ) return op->type_a ? basictypenames[(*op->type_a)->type] : NULL;
		else if( temp == stat->b ) return op->type_b ? basictypenames[(*op->type_b)->type] : NULL;
		else if( temp == stat->c ) return op->type_c ? basictypenames[(*op->type_c)->type] : NULL;
		stat--;
	}

	// method 2
	// find a call to this function
	for( i = 0; i < vm.prog->progs->numstatements; i++ )
	{
		stat = &vm.prog->statements[i];

		if (stat->op >= OP_CALL0 && stat->op <= OP_CALL8 && ((prvm_eval_t *)&vm.prog->globals.gp[stat->a])->function == mf - vm.prog->functions )
		{
			for( i++; i < numstatements; i++ )
			{
				stat = &vm.prog->statements[i];
				op  = &pr_opcodes[stat->op];
				if( OFS_RETURN == stat->a && (*op->type_a)->type != ev_void )
					return basictypenames[(*op->type_a)->type];
				else if( OFS_RETURN == stat->b && (*op->type_b)->type != ev_void )
					return basictypenames[(*op->type_b)->type];
				else if( stat->op == OP_DONE)
					break;
				else if( stat->op >= OP_CALL0 && stat->op <= OP_CALL8 && stat->a != mf - vm.prog->functions )
					break;
			}
		}
	}

	PR_ParseWarning( WARN_UNKNOWNTEMPTYPE, "Could not determine return type for %s, assuming extern returning float\n", PRVM_GetString( mf->s_name ));
	return "float";
}

char *PR_TypeName( ddef_t *def )
{
	static char	fname[1024];
	ddef_t		*j;

	switch( def->type )
	{
	case ev_field:
	case ev_pointer:
		j = PRVM_ED_FindField( PRVM_GetString( def->s_name ));
		if( j ) return va(".%s", basictypenames[j->type]);
		else return basictypenames[def->type];
	case ev_void:
	case ev_string:
	case ev_entity:
	case ev_vector:
	case ev_float:
		return basictypenames[def->type];
	case ev_function:
		return "void()";
	default:
		return "float";
	}
}

char *PR_PrintParameter( ddef_t *def )
{
	static char line[128];

	line[0] = '0';
	if( !com.strcmp( PRVM_GetString( def->s_name ), "IMMEDIATE"))
		com.sprintf( line, "%s", PRVM_ValueString((etype_t)(def->type), (prvm_eval_t *)&vm.prog->globals.gp[def->ofs]));
	else com.sprintf( line, "%s %s", PR_TypeName( def ), PRVM_GetString( def->s_name ));

	return line;
}

int PR_FuncIndex( string_t name )
{
	mfunction_t	*mf;

	mf = PRVM_ED_FindFunction( PRVM_GetString( name ));

	return mf - vm.prog->functions;
}

bool PR_GetFuncNames( void )
{
	int		i, j, ps;
	static char	fname[512];
	static char	line[512];
	dstatement_t	*ds, *rds;
	ddef_t		*par;
	mfunction_t	*mf;
	word		dom;

	for( i = 1; i < vm.prog->progs->numfunctions; i++ )
	{
		mf = vm.prog->functions + i;
		fname[0] = '\0';
		line[0] = '\0';
		src_function[i] = NULL;

		if( mf->first_statement <= 0 ) 
		{
			if((mf->first_statement > -82) && pr_builtins[-mf->first_statement])
			{
				com.sprintf( fname, "%s %s", pr_builtins[-mf->first_statement], PRVM_GetString( mf->s_name ));
			}
			else
			{
				com.sprintf( fname, "void () %s", PRVM_GetString( mf->s_name ));
				MsgDev( D_WARN, "unknown builtin %s\n", PRVM_GetString( mf->s_name ));
			}
		}
		else
		{

			ds = vm.prog->statements + mf->first_statement;
			rds = NULL;

			// find a return statement, to determine the result type 
			while( 1 ) 
			{
				dom = (ds->op) % 100;
				if( !dom ) break;
				if( dom == OP_RETURN)
				{
					if( ds->a != 0 )
					{
						if( PRVM_ED_GlobalAtOfs( ds->a ))
						{
							rds = ds;
							break;
						}
					}
					if( rds == NULL ) rds = ds;
				}
				ds++;
			}

			// print the return type  
			if((rds != NULL) && (rds->a != 0))
			{
				par = PRVM_ED_GlobalAtOfs( rds->a );

				if( par ) com.sprintf( fname, "%s ", PR_TypeName(par));
				else com.sprintf( fname, "%s ", PR_TempType( rds->a, rds, mf ));
			}
			else com.sprintf( fname, "void " );
			com.strcat( fname, "(" );

			// determine overall parameter size 
			for( j = 0, ps = 0; j < mf->numparms; j++ )
				ps += mf->parm_size[j];

			if( ps > 0 ) 
			{
				for( j = mf->parm_start; j < (mf->parm_start) + ps; j++ )
				{
					line[0] = '\0';
					par = PRVM_ED_GlobalAtOfs( j );

					if( !par )
					{
						MsgDev( D_WARN, "no parameter names with offset %i\n", j );
						if( j < (mf->parm_start) + ps - 1 )
							com.sprintf( line, "float par%i, ", j - mf->parm_start );
						else com.sprintf( line, "float par%i", j - mf->parm_start );
					}
					else
					{
						if( par->type == ev_vector ) j += 2;
						if( j < (mf->parm_start) + ps - 1 )
							com.sprintf(line, "%s, ", PR_PrintParameter( par ));
						else com.sprintf(line, "%s", PR_PrintParameter( par ));
					}
					com.strcat( fname, line );
				}
			}
			com.strcat( fname, ") " );
			line[0] = '\0';
			com.sprintf( line, PRVM_GetString( mf->s_name ));
			com.strcat( fname, line );

		}
		if( i >= 16384 )
		{
			PR_Message( "Error: too many functions.\n");
			return false;
		}
		src_function[i] = copystring( fname );
	}
	return true;
}

bool PR_IsConstant( ddef_t *def )
{
	dstatement_t	*d;
	int		i;

	if( def->type & DEF_SAVEGLOBAL )
		return false;

	for( i = 1; i < numstatements; i++ )
	{
		d = &vm.prog->statements[i];
		if( d->b == def->ofs )
		{
			if( pr_opcodes[d->op].associative == ASSOC_RIGHT )
			{
				if( d->op - OP_STORE_F < 6 )//what a hell ???
					return false;
			}
		}
	}
	return true;
}

gofs_t PR_ScaleIndex( mfunction_t *mf, gofs_t ofs )
{
	gofs_t	nofs = 0;

	if( ofs > RESERVED_OFS )
		nofs = ofs - mf->parm_start + RESERVED_OFS;
	else nofs = ofs;

	return nofs;
}

char *PR_DecodeString( string_t newstring )
{
	static string	buf;
	const char	*str;
	char		*s;
	int		c = 1;

	s = buf;
	*s++ = '"';

	str = PRVM_GetString( newstring );

	while( str && *str )
	{
		if( c == sizeof(buf) - 2 )
			break;
		if( *str == '\n' )
		{
			*s++ = '\\';
			*s++ = 'n';
			c++;
		}
		else if( *str == '"')
		{
			*s++ = '\\';
			*s++ = '"';
			c++;
		}
		else
		{
			*s++ = *str;
			c++;
		}
		str++;
		if( c > (int)(sizeof(buf) - 10))
		{
			*s++ = '.';
			*s++ = '.';
			*s++ = '.';
			c += 3;
			break;
		}
	}
	*s++ = '"';
	*s++ = 0;

	return buf;
}

char *PR_ValueString( etype_t type, prvm_eval_t *val )
{
	static char	line[1024];

	line[0] = '\0';

	switch( type )
	{
	case ev_string:
		com.sprintf( line, "%s", PR_DecodeString( val->string ));
		break;
	case ev_void:
		com.sprintf( line, "void" );
		break;
	case ev_float:
		if( val->_float > 999999 || val->_float < -999999 ) // ugh
			com.sprintf( line, "%.f", val->_float );
		else if( val->_float < 0.001 && val->_float > 0 )
			com.sprintf( line, "%.6f", val->_float );
		else com.sprintf( line, "%g", val->_float );
		break;
	case ev_vector:
		com.sprintf( line, "'%g %g %g'", val->vector[0], val->vector[1], val->vector[2] );
		break;
	default:
		com.sprintf( line, "bad type %i", type );
		break;
	}
	return line;
}

char *PR_GetGlobal( gofs_t ofs, type_t *req )
{
	ddef_t		*def;
	static string	line;

	line[0] = '\0';
	def = PRVM_ED_GlobalAtOfs( ofs );

	if( def )
	{
		if( !com.strcmp( PRVM_GetString( def->s_name ), "IMMEDIATE"))
			com.sprintf( line, "%s", PR_ValueString((etype_t)(def->type), (prvm_eval_t *)&vm.prog->globals.gp[def->ofs]));
		else 
		{
			com.sprintf( line, "%s", PRVM_GetString( def->s_name ));
			if( def->type == ev_vector && req->aux_type == type_float )
				com.strcat( line, "_x" );

		}
		return copystring( line );
	}

	return NULL;
}

char *PR_GetImmediate( mfunction_t *mf, gofs_t ofs, int fun, char *knew )
{
	int		i;
	static char	*IMMEDIATES[4096];
	gofs_t		nofs;

	// free 'em all 
	if( fun == 0 ) 
	{
		for( i = 0; i < 4096; i++)
		{
			if( IMMEDIATES[i] ) 
			{
				Mem_Free(IMMEDIATES[i]);
				IMMEDIATES[i] = NULL;
			}
		}
		return NULL;
	}
	nofs = PR_ScaleIndex( mf, ofs );

	// check consistency 
	if((nofs <= 0) || (nofs > 4096 - 1))
		Sys_Break( "Error: index (%i) out of bounds.\n", nofs );

	if( fun == 1 ) 
	{
		// insert at nofs 
		if(IMMEDIATES[nofs]) Mem_Free(IMMEDIATES[nofs]);
		IMMEDIATES[nofs] = copystring( knew );
	}
	else if (fun == 2)
	{
		// get from nofs 
		if( IMMEDIATES[nofs] )
			return copystring( IMMEDIATES[nofs] );
		else
		{
			MsgDev( D_ERROR, "%i not defined.\n", nofs );
			return copystring(va( "unk%i", nofs));
		}
	}
	return NULL;
}

char *PR_DecompileGet( mfunction_t *mf, gofs_t ofs, type_t *req )
{
	char	*farg1 = PR_GetGlobal( ofs, req );
	if(!farg1) farg1 = PR_GetImmediate( mf, ofs, 2, NULL );
	return farg1;
}

void PR_Indent( int c )
{
	int	i;

	if( c < 0 ) c = 0;
	for( i = 0; i < c; i++ ) 
		FS_Printf( file, "\t" );
}

void PR_GetStatement( mfunction_t *mf, dstatement_t *s, int *indent )
{
	static char	line[512], fnam[512];
	char		*arg1, *arg2, *arg3;
    	type_t		*typ1, *typ2, *typ3;
	word		dom, doc, ifc, tom;
	int		dum, nargs, i, j;
	dstatement_t	*t, *k;
	ddef_t		*par;

	arg1 = arg2 = arg3 = NULL;
	line[0] = '\0';
	fnam[0] = '\0';

	dom = s->op;
	doc = dom / 10000;
	ifc = (dom % 10000) / 100;

	// use program flow information 
	for( i = 0; i < ifc; i++ ) 
	{
		(*indent)--;
		PR_Indent(*indent);
		FS_Printf( file, "}\n" ); // FrikaC style modification
	}

	for( i = 0; i < doc; i++ ) 
	{
		PR_Indent(*indent);
		FS_Printf( file, "do\n" );
		PR_Indent(*indent);
		FS_Printf( file, "{\n" );
		(*indent)++;
	}

	// remove all program flow information 
	s->op %= 100;
	typ1 = pr_opcodes[s->op].type_a ? pr_opcodes[s->op].type_a[0] : NULL;
	typ2 = pr_opcodes[s->op].type_b ? pr_opcodes[s->op].type_b[0] : NULL;
	typ3 = pr_opcodes[s->op].type_c ? pr_opcodes[s->op].type_c[0] : NULL;

	// states are handled at top level 
	if( s->op == OP_DONE )
	{

	}
	else if( s->op == OP_STATE ) 
	{
		par = PRVM_ED_GlobalAtOfs( s->a );
		if( !par ) Sys_Break( "Error: can't determine frame number.\n");
		arg2 = PR_DecompileGet( mf, s->b, NULL );
		if( !arg2 ) Sys_Break( "Error: no state parameter with offset %i.\n", s->b );
		PR_Indent(*indent);
		FS_Printf( file, "state [ %s, %s ];\n", PR_ValueString((etype_t)(par->type), (prvm_eval_t *)&vm.prog->globals.gp[par->ofs]), arg2 );
	}
	else if( s->op == OP_RETURN )
	{
		PR_Indent(*indent);
		FS_Printf( file, "return" );

		if( s->a )
		{
			FS_Printf( file, " " );
			arg1 = PR_DecompileGet( mf, s->a, typ1 );
			FS_Printf( file, "(%s)", arg1 );
		}
		FS_Printf( file, ";\n" );
	}
	else if(( OP_MUL_F <= s->op && s->op <= OP_SUB_V) || (OP_EQ_F <= s->op && s->op <= OP_GT) || (OP_AND <= s->op && s->op <= OP_BITOR))
	{
		arg1 = PR_DecompileGet( mf, s->a, typ1 );
		arg2 = PR_DecompileGet( mf, s->b, typ2 );
		arg3 = PR_GetGlobal( s->c, typ3 );

		if( arg3 ) 
		{
			PR_Indent(*indent);
			FS_Printf( file, "%s = %s %s %s;\n", arg3, arg1, pr_opcodes[s->op].name, arg2 );
		}
		else
		{
			com.sprintf( line, "(%s %s %s)", arg1, pr_opcodes[s->op].name, arg2 );
			PR_GetImmediate( mf, s->c, 1, line );
		}
	}
	else if( OP_LOAD_F <= s->op && s->op <= OP_ADDRESS )
	{
		// RIGHT HERE
		arg1 = PR_DecompileGet( mf, s->a, typ1 );
		arg2 = PR_DecompileGet( mf, s->b, typ2 );
		arg3 = PR_GetGlobal( s->c, typ3 );

		if( arg3 )
		{
			PR_Indent(*indent);
			FS_Printf( file, "%s = %s.%s;\n", arg3, arg1, arg2 );
		}
		else
		{
			com.sprintf( line, "%s.%s", arg1, arg2 );
			PR_GetImmediate( mf, s->c, 1, line );
		}
	}
	else if( OP_STORE_F <= s->op && s->op <= OP_STORE_FNC )
	{
		arg1 = PR_DecompileGet( mf, s->a, typ1 );
		arg3 = PR_GetGlobal( s->b, typ2 );

		if( arg3 )
		{
			PR_Indent(*indent);
			FS_Printf( file, "%s = %s;\n", arg3, arg1 );
		}
		else
		{
			com.sprintf( line, "%s", arg1 );
			PR_GetImmediate( mf, s->b, 1, line );
		}
	}
	else if( OP_STOREP_F <= s->op && s->op <= OP_STOREP_FNC )
	{
		arg1 = PR_DecompileGet( mf, s->a, typ2 );
		arg2 = PR_DecompileGet( mf, s->b, typ2 );

		PR_Indent(*indent);
		FS_Printf( file, "%s = %s;\n", arg2, arg1 );
	}
	else if( OP_NOT_F <= s->op && s->op <= OP_NOT_FNC )
	{
		arg1 = PR_DecompileGet( mf, s->a, typ1 );
		com.sprintf( line, "!%s", arg1 );
		PR_GetImmediate( mf, s->c, 1, line );
	}
	else if( OP_CALL0 <= s->op && s->op <= OP_CALL8 )
	{
		nargs = s->op - OP_CALL0;
		arg1 = PR_DecompileGet( mf, s->a, NULL );
		com.sprintf( line, "%s (", arg1 );
		com.sprintf( fnam, "%s", arg1 );

		for( i = 0; i < nargs; i++ )
		{
			typ1 = NULL;
			j = 4 + 3 * i;
			if( arg1 ) Mem_Free( arg1 );
			arg1 = PR_DecompileGet( mf, j, typ1 );
			com.strcat( line, arg1 );
			if( i < nargs - 1 ) com.strcat( line, ", " ); // frikqcc modified
		}
		com.strcat( line, ")" );
		PR_GetImmediate( mf, 1, 1, line );

		// g-cont: hey what a hell this ?
		if((((s + 1)->a != 1) && ((s + 1)->b != 1) && ((s + 2)->a != 1) && ((s + 2)->b != 1)) || ((((s + 1)->op) % 100 == OP_CALL0) && ((((s + 2)->a != 1)) || ((s + 2)->b != 1))))
		{
			PR_Indent(*indent);
			FS_Printf( file, "%s;\n", line );
		}
	}
	else if( s->op == OP_IF || s->op == OP_IFNOT )
	{
		arg1 = PR_DecompileGet( mf, s->a, NULL );
		arg2 = PR_GetGlobal( s->a, NULL );

		if( s->op == OP_IFNOT )
		{
			if( s->b < 1 ) Sys_Break( "Error: found a negative IFNOT jump.\n" );

			// get instruction right before the target 
			t = s + s->b - 1;
			tom = t->op % 100;

			if( tom != OP_GOTO )
			{
				// pure if 
				PR_Indent(*indent);
				FS_Printf( file, "if (%s)\n", arg1 ); // FrikaC modified
				PR_Indent(*indent);
				FS_Printf( file, "{\n" );
				(*indent)++;
			}
			else
			{
				if( t->a > 0 )
				{
					// ite 
					PR_Indent(*indent);
		    			FS_Printf( file, "if (%s)\n", arg1 ); // FrikaC modified
					PR_Indent(*indent);
					FS_Printf( file, "{\n" );
					(*indent)++;
				}
				else
				{
					if((t->a + s->b) > 1)
					{
						// pure if  
						PR_Indent(*indent);
						FS_Printf( file, "if (%s)\n", arg1 ); //FrikaC modified
						PR_Indent(*indent);
						FS_Printf( file, "{\n" );
						(*indent)++;
					}
					else
					{
						dum = 1;
						for( k = t + (t->a); k < s; k++ )
						{
							tom = k->op % 100;
							if( tom == OP_GOTO || tom == OP_IF || tom == OP_IFNOT )
								dum = 0;
						}
						if( dum ) 
						{
							PR_Indent(*indent);
							FS_Printf( file, "while (%s)\n", arg1 );
							PR_Indent(*indent);	// FrikaC
							FS_Printf( file, "{\n" );
			    				(*indent)++;
						}
						else
						{
							PR_Indent(*indent);
							FS_Printf( file, "if (%s)\n", arg1 ); //FrikaC modified
							PR_Indent(*indent);
							FS_Printf( file, "{\n" );
			    				(*indent)++;
						}
					}
				}
			}
		}
		else
		{
			// do ... while
			(*indent)--;
			FS_Printf( file, "\n" );
			PR_Indent(*indent);
			FS_Printf( file, "} while (%s);\n", arg1 );
		}
	}
	else if( s->op == OP_GOTO )
	{
		if( s->a > 0 )
		{
			// else 
			(*indent)--;
			PR_Indent(*indent);
			FS_Printf( file, "}\n" );
			PR_Indent(*indent);
			FS_Printf( file, "else\n" );
			PR_Indent(*indent);
			FS_Printf( file, "{\n" );
			(*indent)++;
		}
		else
		{
			// while 
			(*indent)--;
			PR_Indent(*indent);
			FS_Printf( file, "}\n" );

		}
	}
	else
	{
		if( s->op <= OP_BITOR ) MsgDev( D_WARN, "unknown usage of OP_%s", pr_opcodes[s->op].opname );
		else MsgDev( D_WARN, "unknown opcode OP_%s\n", pr_opcodes[s->op].opname );
	}

	if( arg1 ) Mem_Free( arg1 );
	if( arg2 ) Mem_Free( arg2 );
	if( arg3 ) Mem_Free( arg3 );
}

void PR_GetFunction( mfunction_t *mf )
{
	dstatement_t	*ds;
	int		indent;

	// initialize 
	PR_GetImmediate( mf, 0, 0, NULL );
	indent = 1;
	ds = vm.prog->statements + mf->first_statement;
	if( ds->op == OP_STATE ) ds++;

	while( 1 ) 
	{
		PR_GetStatement( mf, ds, &indent );
		if( !ds->op ) break;
		ds++;
	}
	if( indent != 1 ) MsgDev( D_WARN, "Indentation structure corrupt\n" );
}

void PR_DecodeFunction( const char *name )
{
	int		i, dum, findex, ps;
	int		j, start, end;
	mfunction_t	*mfpred, *mf;
	dstatement_t	*k, *ds, *ts;
	ddef_t		*ef, *par;
	char		*arg2;
	word		dom, tom;
	static string	line;

	// find function
	mf = PRVM_ED_FindFunction( name );
	if( !mf ) Sys_Break( "Error: No function named \"%s\"\n", name);
	findex = mf - vm.prog->functions;

	// check ''local globals'' 
	mfpred = mf - 1;

	for( j = 0, ps = 0; j < mfpred->numparms; j++ )
		ps += mfpred->parm_size[j];

	start = mfpred->parm_start + mfpred->locals + ps;
	if( mfpred->first_statement < 0 && mf->first_statement > 0 )
		start -= 1;
	if( start == 0 ) start = 1;
	end = mf->parm_start;

	for( j = start; j < end; j++ )
	{
		par = PRVM_ED_GlobalAtOfs( j );
		if( par )
		{
			if( par->type & DEF_SAVEGLOBAL ) par->type -= DEF_SAVEGLOBAL;
			if( par->type == ev_function )
			{
				if(com.strcmp(PRVM_GetString( par->s_name ), "IMMEDIATE"))
				{
					if(com.strcmp(PRVM_GetString( par->s_name ), name))
						FS_Printf( file, "%s;\n", src_function[PR_FuncIndex( par->s_name )] );
				}
				else if( par->type != ev_pointer )
				{
					if(com.strcmp( PRVM_GetString(par->s_name), "IMMEDIATE"))
					{
						if( par->type == ev_field )
						{
							ef = PRVM_ED_FindField(PRVM_GetString(par->s_name));
							if(!ef) Sys_Break( "Error: Could not locate a field named \"%s\"\n", PRVM_GetString(par->s_name));
						}
						if( ef->type == ev_vector ) j += 3;
						FS_Printf( file, ".%s %s;\n", PR_TypeName( ef ), PRVM_GetString( ef->s_name ));
					}
					else
					{
						if( par->type == ev_vector ) j += 2;
						if( par->type == ev_entity || par->type == ev_void )
							FS_Printf( file, "%s %s;\n", PR_TypeName(par), PRVM_GetString( par->s_name ));
						else
						{
							line[0] = '\0';
							com.sprintf( line, "%s", PR_ValueString((etype_t)(par->type), (prvm_eval_t *)&vm.prog->globals.gp[par->ofs]));

							if(PR_IsConstant( par ))
							{
								FS_Printf( file, "%s %s    = %s;\n", PR_TypeName(par), strings + par->s_name, line);
							}
							else
							{
								if(vm.prog->globals.gp[par->ofs] != 0)
									FS_Printf( file, "%s %s ;\n", PR_TypeName( par ), PRVM_GetString( par->s_name ));
								else FS_Printf( file, "%s %s;\n", PR_TypeName( par ), PRVM_GetString( par->s_name ));
							}
						}
					}
				}
			}
		}
	}

	// check ''local globals'' 
	if( mf->first_statement <= 0 )
	{
		FS_Printf( file, "%s", src_function[findex]);
		FS_Printf( file, " = #%i; \n", -mf->first_statement );
		return;
	}

	ds = vm.prog->statements + mf->first_statement;

	while( 1 )
	{
		dom = (ds->op) % 100;

		if(!dom) break;
		else if( dom == OP_GOTO )
		{
			// check for i-t-e 
			if( ds->a > 0 )
			{
				ts = ds + ds->a;
				ts->op += 100;		// mark the end of a if/ite construct 
			}
		}
		else if( dom == OP_IFNOT )
		{
			// check for pure if 
			ts = ds + ds->b;
			tom = (ts - 1)->op % 100;

			if( tom != OP_GOTO ) ts->op += 100;	// mark the end of a if/ite construct 
			else if( (ts - 1)->a < 0 )
			{
				if(((ts - 1)->a + ds->b) > 1)
				{
					// pure if
					ts->op += 100;	// mark the end of a if/ite construct 
				}
				else
				{
					dum = 1;
					for( k = (ts - 1) + ((ts - 1)->a); k < ds; k++ )
					{
						tom = k->op % 100;
						if( tom == OP_GOTO || tom == OP_IF || tom == OP_IFNOT )
							dum = 0;
					}
					if( !dum )
					{
						// pure if  
						ts->op += 100;	// mark the end of a if/ite construct 
					}
				}
			}
		}
		else if( dom == OP_IF )
		{
			ts = ds + ds->b;
			ts->op += 10000;	// mark the start of a do construct 

		}
		ds++;
	}

	// print the prototype 
	FS_Printf( file, "\n%s", src_function[findex] );

	// handle state functions 
	ds = vm.prog->statements + mf->first_statement;

	if( ds->op == OP_STATE )
	{
		par = PRVM_ED_GlobalAtOfs( ds->a );
		if( !par ) Sys_Break( "Error: Can't determine frame number.");
		arg2 = PR_DecompileGet( mf, ds->b, NULL );
		if( !arg2 ) Sys_Break( "Error: No state parameter with offset %i.", ds->b);
		FS_Printf( file, " = [ %s, %s ]", PR_ValueString((etype_t)(par->type), (prvm_eval_t *)&vm.prog->globals.gp[par->ofs]), arg2 );
		Mem_Free( arg2 );
	}
	else FS_Printf( file, " =" );

	FS_Printf( file, "\n{\n" );

	// calculate the parameter size 
	for( j = 0, ps = 0; j < mf->numparms; j++ )
		ps += mf->parm_size[j];

	// print the locals 
	if( mf->locals > 0 )
	{
		if( (mf->parm_start) + mf->locals - 1 >= (mf->parm_start) + ps )
		{
			for( i = mf->parm_start + ps; i < (mf->parm_start) + mf->locals; i++ ) 
			{
				par = PRVM_ED_GlobalAtOfs( i );
				if( !par ) continue; // temps
				else
				{
					if (!com.strcmp( PRVM_GetString( par->s_name ), "IMMEDIATE"))
						continue; // immediates don't belong
					if( par->type == ev_function )
						MsgDev( D_WARN, "fields and functions must be global\n");
					else FS_Printf( file, "\tlocal %s;\n", PR_PrintParameter(par));
					if( par->type == ev_vector ) i += 2;
				}
			}
			FS_Printf( file, "\n");
		}
	}

	// do the hard work 
	PR_GetFunction( mf );
	FS_Printf( file, "};\n");
}

bool PR_DecodeProgs( void )
{
	bool		bogusname;
	char		fname[512];
	mfunction_t	*m;
	file_t		*f;
	uint		i;

	progs_src = FS_Open( va("%s/progs.src", sourcefilename), "w" ); // using dat name as store dir
	if( !progs_src )
	{
		Msg( "Error: Could not open \"progs.src\" for output.\n" );
		return false;
	}
	FS_Printf( progs_src, "./progs.dat\n\n" );

	for( i = 1; i < vm.prog->progs->numfunctions; i++ )
	{
		m = &vm.prog->functions[i];

		fname[0] = '\0';
		if( m->s_file <= vm.prog->progs->numstrings && m->s_file >= 0 )
			com.sprintf( fname, "%s", PRVM_GetString( m->s_file ));

		// FrikaC -- not sure if this is cool or what?
		bogusname = false;
		if( com.strlen(fname) <= 0 ) bogusname = true;
		else
		{
			// kill any pathes, leave only filename.ext
			const char *ext = FS_FileExtension( fname );
			FS_FileBase( fname, fname );
			com.strcat( fname, va(".%s", ext ));
		}

		if( bogusname )
		{
			if(!PR_AlreadySeen( fname ))
			{
				synth_name[0] = 0;
				if(!PR_SynthName(va("%s", PRVM_GetString( m->s_name ))))
					fake_name++;
			}
			if( synth_name[0] ) com.sprintf( fname, synth_name );
			else com.sprintf( fname, "source%i.qc", fake_name );
		}
		if(!PR_AlreadySeen( fname ))
		{
			Msg( "%s\n", fname );
			FS_Printf( progs_src, "%s\n", fname );
			f = FS_Open( va("%s/%s", sourcefilename, fname ), "w" );
		}
		else f = FS_Open( va( "%s/%s", sourcefilename, fname ), "a+" );
		if( !f )
		{
			Msg( "Error: Could not open \"%s\" for output.\n", fname);
			return false;
		}
		file = f;
		PR_DecodeFunction(PRVM_GetString( m->s_name ));
		FS_Close( f );
	}
	FS_Close( progs_src );
	return true;
}

void PR_InitTypes( void )
{
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
}

bool PR_Decompile( const char *name )
{
	bool	result;

	// sanity check
	if(!FS_FileExists( name )) return false;
	PRVM_LoadProgs( name );	
	FS_FileBase( name, sourcefilename );

	Msg("\nDecompiling...\n");

	if( vm.prog->sources ) // source always are packed
	{
		int	i;
		dsource_t	*src = vm.prog->sources;
		char	*in, *file;

		for( i = 0; i < vm.prog->progs->numsources; i++, src++ )
		{
			Msg( "%s\n", src->name );
			file = Mem_Alloc( qccpool, src->size ); // alloc memory for inflate block
			in = (char *)(((byte *)vm.prog->progs) + src->filepos );			
			if(PR_decode( src->disksize, src->size, src->compression, in, &file ))
				FS_WriteFile( va( "%s/%s", sourcefilename, src->name ), file, src->size );
			else Msg("Warning: can't decompile %s\n", src->name );
			Mem_Free( file );
		}
		vm.prog->sources = NULL;
		vm.prog->loaded = false;
		return true;
	}
	else
	{
		if(!PR_GetFuncNames())
			return false;
		result = PR_DecodeProgs();
		vm.prog->loaded = false;
		return result;
	}
}