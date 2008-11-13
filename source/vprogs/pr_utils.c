//=======================================================================
//			Copyright XashXT Group 2007 ©
//			qcc_utils.c - qcc common tools
//=======================================================================

#include "vprogs.h"
#include "const.h"

void Hash_InitTable(hashtable_t *table, int numbucks)
{
	table->numbuckets = numbucks;
	table->bucket = (bucket_t **)Qalloc(BytesForBuckets(numbucks));
}

uint Hash_Key(char *name, int modulus)
{
	//FIXME: optimize.
	uint	key;
	for (key = 0; *name; name++)
		key += ((key<<3) + (key>>28) + *name);
		
	return (key%modulus);
}

void *Hash_Get(hashtable_t *table, char *name)
{
	uint	bucknum = Hash_Key(name, table->numbuckets);
	bucket_t	*buck;

	buck = table->bucket[bucknum];
	while(buck)
	{
		if (!STRCMP(name, buck->keystring))
			return buck->data;

		buck = buck->next;
	}

	return NULL;
}

void *Hash_GetKey(hashtable_t *table, int key)
{
	uint	bucknum = key%table->numbuckets;
	bucket_t	*buck;

	buck = table->bucket[bucknum];
	while(buck)
	{
		if ((int)buck->keystring == key)
			return buck->data;

		buck = buck->next;
	}

	return NULL;
}

void *Hash_GetNext(hashtable_t *table, char *name, void *old)
{
	uint	bucknum = Hash_Key(name, table->numbuckets);
	bucket_t	*buck;

	buck = table->bucket[bucknum];
	while(buck)
	{
		if (!STRCMP(name, buck->keystring))
		{
			if (buck->data == old)//found the old one
				break;
		}
		buck = buck->next;
	}
	if (!buck) return NULL;

	buck = buck->next;//don't return old
	while(buck)
	{
		if (!STRCMP(name, buck->keystring))
			return buck->data;

		buck = buck->next;
	}
	return NULL;
}

void *Hash_Add(hashtable_t *table, char *name, void *data, bucket_t *buck)
{
	uint	bucknum = Hash_Key(name, table->numbuckets);

	buck->data = data;
	buck->keystring = name;
	buck->next = table->bucket[bucknum];
	table->bucket[bucknum] = buck;

	return buck;
}

void *Hash_AddKey(hashtable_t *table, int key, void *data, bucket_t *buck)
{
	uint	bucknum = key%table->numbuckets;

	buck->data = data;
	buck->keystring = (char*)key;
	buck->next = table->bucket[bucknum];
	table->bucket[bucknum] = buck;

	return buck;
}

void Hash_Remove(hashtable_t *table, char *name)
{
	uint	bucknum = Hash_Key(name, table->numbuckets);
	bucket_t	*buck;	

	buck = table->bucket[bucknum];
	if (!STRCMP(name, buck->keystring))
	{
		table->bucket[bucknum] = buck->next;
		return;
	}

	while(buck->next)
	{
		if (!STRCMP(name, buck->next->keystring))
		{
			buck->next = buck->next->next;
			return;
		}

		buck = buck->next;
	}
}

void Hash_RemoveData(hashtable_t *table, char *name, void *data)
{
	uint	bucknum = Hash_Key(name, table->numbuckets);
	bucket_t	*buck;	

	buck = table->bucket[bucknum];
	if (buck->data == data)
	{
		if (!STRCMP(name, buck->keystring))
		{
			table->bucket[bucknum] = buck->next;
			return;
		}
	}
	while(buck->next)
	{
		if (buck->next->data == data)
		{
			if (!STRCMP(name, buck->next->keystring))
			{
				buck->next = buck->next->next;
				return;
			}
		}
		buck = buck->next;
	}
}


void Hash_RemoveKey(hashtable_t *table, int key)
{
	uint	bucknum = key%table->numbuckets;
	bucket_t	*buck;	

	buck = table->bucket[bucknum];
	if ((int)buck->keystring == key)
	{
		table->bucket[bucknum] = buck->next;
		return;
	}
	while(buck->next)
	{
		if ((int)buck->next->keystring == key)
		{
			buck->next = buck->next->next;
			return;
		}
		buck = buck->next;
	}
}

/*
================
PR_decode
================
*/
int PR_decode(int complen, int len, int method, char *src, char **dst)
{
	int	i;
	char	*buffer = *dst;

	switch(method)
	{
	case CMP_NONE:
		if (complen != len) 
		{
			MsgDev(D_WARN, "PR_decode: complen[%d] != len[%d]\n", complen, len);
			return false;
		}		
		Mem_Copy(buffer, src, len);
		break;
	case CMP_LZSS:
		if (complen != len) 
		{
			MsgDev(D_WARN, "PR_decode: complen[%d] != len[%d]\n", complen, len);
			return false;
		}
		for (i = 0; i < len; i++) buffer[i] = src[i] ^ 0xA5;
		break;
	case CMP_ZLIB:
		VFS_Unpack( src, complen, dst, len );
		break;
	default:
		MsgDev(D_WARN, "PR_decode: invalid method\n");
		return false;
	}	
	return true;
}

/*
================
PR_encode
================
*/
int PR_encode(int len, int method, char *src, file_t *h)
{
	int	i, pos;
	vfile_t	*vf;

	switch(method)
	{
	case CMP_NONE:
		FS_Write(h, src, len);
		return len;
	case CMP_LZSS:
		for (i = 0; i < len; i++) src[i] = src[i] ^ 0xA5;
		FS_Write(h, src, len);
		return len;
	case CMP_ZLIB:
		vf = VFS_Open( h, "wz" );
		pos = FS_Tell(h); // member ofs
		VFS_Write( vf, src, len );
		VFS_Close( vf );
		return FS_Tell(h) - pos;
	default:
		MsgDev(D_WARN, "PR_encode: invalid method\n");
		return false;
	}
}

// CopyString returns an offset from the string heap
int PR_CopyString (char *str, bool noduplicate )
{
	int	old;
	char	*s;

	if (noduplicate)
	{
		if (!str || !*str) return 0;
		for (s = strings; s < strings + strofs; s++)
			if (!com.strcmp(s, str)) return s - strings;
	}

	old = strofs;
	com.strcpy (strings + strofs, str);
	strofs += com.strlen(str)+1;
	return old;
}

/*
================
PR_WriteSourceFiles

include sources into progs.dat for fake decompile
if it needs
================
*/
bool PR_WriteSourceFiles( file_t *h, dprograms_t *progs, bool sourceaswell )
{
	dsource_t		*idf;
	cachedsourcefile_t	*f;
	int		num = 0;
	int		ofs;

	for (f = sourcefile, num = 0; f; f = f->next)
	{
		if (f->type == FT_CODE && !sourceaswell)
			continue;
		num++;
	}
	if( !num )
	{
		progs->ofssources = progs->numsources = 0;
		return false;
	}
	idf = Qalloc(sizeof(dsource_t) * num);
	for( f = sourcefile, num = 0; f; f = f->next )
	{
		if( f->type == FT_CODE && !sourceaswell )
			continue;
		com.strncpy( idf[num].name, f->filename, 64 );
		idf[num].size = f->size;
		idf[num].compression = CMP_ZLIB;
		idf[num].filepos = FS_Tell( h );
		idf[num].disksize = PR_encode(f->size, idf[num].compression, f->file, h );
		num++;
	}
	ofs = FS_Tell( h );	
	FS_Write( h, idf, sizeof(dsource_t) * num );
	sourcefile = NULL;

	// update header
	progs->ofssources = ofs;
	progs->numsources = num;
	return true;
}

int PR_WriteBodylessFuncs (file_t *handle)
{
	def_t		*d;
	int		ret = 0;

	for (d = pr.def_head.next; d; d = d->next)
	{
		// function parms are ok
		if (d->type->type == ev_function && !d->scope)
		{
			if (d->initialized != 1 && d->references > 0)
			{
				FS_Write(handle, d->name, com.strlen(d->name)+1);
				ret++;
			}
		}
	}
	return ret;
}

/*
====================
AddBuffer

copy string into buffer and process crc
====================
*/
#define NEED_CRC	1 // calcaulate crc
#define NEED_CPY	2 // copy string
#define NEED_ALL	NEED_CRC|NEED_CPY

#define ADD1(p) AddBuffer(p, &crc, file, NEED_ALL)
#define ADD2(p) AddBuffer(p, &crc, file, NEED_CPY)
#define ADD3(p) AddBuffer(p, &crc, file, NEED_CRC)

_inline void AddBuffer(char *p, word *crc, char *file, byte flags )
{
	char	*s;
	int	i = com.strlen(file);

	if (i + com.strlen(p) + 1 >= PROGDEFS_MAX_SIZE)
		return;

	for(s = p; *s; s++, i++)
	{
		if(flags & NEED_CRC) CRC_ProcessByte(crc, *s);
		if(flags & NEED_CPY) file[i] = *s;
	}
	if(flags & NEED_CPY) file[i] = '\0';
}

/*
====================
PR_WriteProgdefs

write advanced progdefs.h into disk
====================
*/
word PR_WriteProgdefs( void )
{
	char		file[PROGDEFS_MAX_SIZE];
	string		header_name, lwr_header;
	char		lwr_prefix[8], upr_prefix[8];
	char		array_size[8]; // g-cont: mega name!
	int		f = 0, k = 1;
	string		path;
	def_t		*d;
	word		crc;

	CRC_Init( &crc );
	Mem_Set( file, 0, PROGDEFS_MAX_SIZE );
	FS_FileBase( progsoutname, header_name );
	com.strupr( header_name, header_name ); // as part of define name
	com.strlwr( header_name, lwr_header ); // as part of head comment

	// convert progsoutname to prefix
	// using special case for "server", because "se" prefix looks ugly
	if(!com.stricmp( header_name, "server" )) com.strncpy( lwr_prefix, "sv", 8 );
	else com.strnlwr( header_name, lwr_prefix, 3 );
	com.strnupr( lwr_prefix, upr_prefix, 3 );

	ADD1("//=======================================================================\n");
	ADD1("//			Copyright XashXT Group 2008 ©\n");
	ADD2(va("//			%s_edict.h - %s prvm edict\n", lwr_prefix, lwr_header ));
	ADD1("//=======================================================================\n");
	ADD2(va("#ifndef %s_EDICT_H\n#define %s_EDICT_H\n", upr_prefix, upr_prefix ));
	ADD1("\nstruct");
	ADD2(va(" %s_globalvars_s", lwr_prefix ));
	ADD1(va("\n{"));	
	ADD2(va("\n"));
	ADD1(va("\tint\tpad[%i];\n", RESERVED_OFS));

	// write globalvars_t
	for( d = pr.def_head.next; d; d = d->next )
	{
		if( !com.strcmp (d->name, "end_sys_globals")) break;
		if( d->ofs < RESERVED_OFS ) continue;
		if( com.strchr( d->name, '[' )) continue; // skip arrays
		if( d->arraysize > 1 ) com.snprintf( array_size, 8, "[%i]", d->arraysize );
		else array_size[0] = '\0'; // clear array
			
		switch( d->type->type )
		{
		case ev_float:
			ADD1(va("\tfloat\t%s%s;\n",d->name, array_size));
			break;
		case ev_vector:
			ADD1(va("\tvec3_t\t%s%s;\n",d->name, array_size));
			d = d->next->next->next; // skip the elements
			break;
		case ev_string:
			ADD1(va("\tstring_t\t%s%s;\n",d->name, array_size));
			break;
		case ev_function:
			ADD1(va("\tfunc_t\t%s%s;\n",d->name, array_size));
			break;
		case ev_entity:
			ADD1(va("\tint\t%s%s;\n",d->name, array_size));
			break;
		case ev_integer:
			ADD1(va("\tint\t%s%s;\n",d->name, array_size));
			break;
		default:
			ADD1(va("\tint\t%s%s;\n",d->name, array_size));
			break;
		}
	}
	ADD1("};\n\n");

	// write entvars_t
	ADD1("struct");
	ADD2(va(" %s_entvars_s", lwr_prefix ));
	ADD1("\n{\n");

	// write entvars_t
	for( d = pr.def_head.next; d; d = d->next )
	{
		if( !com.strcmp (d->name, "end_sys_fields")) break;
		if( d->type->type != ev_field ) continue;
		if( com.strchr( d->name, '[' )) continue; // skip arrays
		if( d->arraysize > 1 ) com.snprintf( array_size, 8, "[%i]", d->arraysize );
		else array_size[0] = '\0'; // clear array

		switch( d->type->aux_type->type )
		{
		case ev_float:
			ADD1(va("\tfloat\t%s%s;\n",d->name, array_size ));
			break;
		case ev_vector:
			ADD1(va("\tvec3_t\t%s%s;\n",d->name, array_size));
			d = d->next->next->next; // skip the elements
			break;
		case ev_string:
			ADD1(va("\tstring_t\t%s%s;\n",d->name, array_size));
			break;
		case ev_function:
			ADD1(va("\tfunc_t\t%s%s;\n",d->name, array_size));
			break;
		case ev_entity:
			ADD1(va("\tint\t%s%s;\n",d->name, array_size));
			break;
		case ev_integer:
			ADD1(va("\tint\t%s%s;\n",d->name, array_size));
			break;
		default:
			ADD1(va("\tint\t%s%s;\n",d->name, array_size));
			break;
		}
	}
	ADD1("};\n\n");
	ADD2(va("#define PROG_CRC_%s\t\t%i\n", header_name, crc ));
	ADD2(va("\n#endif//%s_EDICT_H", upr_prefix ));

	if( ForcedCRC ) crc = ForcedCRC;	// potentially backdoor

	if( host_instance == HOST_NORMAL || host_instance == HOST_DEDICATED )
	{
		// make sure what progs file will be placed into right directory
		com.snprintf( progsoutname, MAX_SYSPATH, "%s/%s.dat", GI->vprogs_dir, header_name );
	}

	switch( crc )
	{
	case 9691:
		PR_Message("Xash3D unmodified server.dat\n");
		if(!com.strcmp(progsoutname, "unknown.dat")) com.strcpy(progsoutname, "server.dat");
		break;
	case 3720:
		PR_Message("Xash3D unmodified client.dat\n");
		if(!com.strcmp(progsoutname, "unknown.dat")) com.strcpy(progsoutname, "client.dat");
		break;		
	case 2158:
		PR_Message("Xash3D unmodified uimenu.dat\n");
		if(!com.strcmp(progsoutname, "unknown.dat")) com.strcpy(progsoutname, "uimenu.dat");
		break;
	default:
		PR_Message("Custom progs crc %d\n", crc );
		if(!com.strcmp(progsoutname, "unknown.dat")) com.strcpy(progsoutname, "progs.dat");
		if( host_instance != HOST_QCCLIB ) break;
		com.snprintf( path, MAX_STRING, "../%s_edict.h", lwr_prefix );
		PR_Message( "writing %s\n", path ); // auto-determine
		FS_WriteFile( path, file, com.strlen(file));
		break;
	}
	return crc;
}

void PR_WriteDAT( void )
{
	ddef_t		*dd;
	file_t		*f;
	dprograms_t	progs; // header
	char		element[MAX_NAME];
	int		i, crc, progsize, num_ref;
	def_t		*def, *comp_x, *comp_y, *comp_z;

	crc = PR_WriteProgdefs(); // write progdefs.h

	progs.flags = 0;

	if(numstatements > MAX_STATEMENTS) PR_ParseError(ERR_INTERNAL, "Too many statements - %i\n", numstatements );
	if(strofs > MAX_STRINGS) PR_ParseError(ERR_INTERNAL, "Too many strings - %i\n", strofs );

	PR_UnmarshalLocals();

	if(opt_compstrings) progs.flags |= COMP_STRINGS;
	if(opt_compfunctions)
	{
		progs.flags |= COMP_FUNCTIONS;
		progs.flags |= COMP_STATEMENTS;
	}
	if(opt_compress_other)
	{
		progs.flags |= COMP_DEFS;
		progs.flags |= COMP_FIELDS;
		progs.flags |= COMP_GLOBALS;
	}

	// compression of blocks?
	if( opt_writelinenums ) progs.flags |= COMP_LINENUMS;	
	if( opt_writetypes ) progs.flags |= COMP_TYPES;

	// part of how compilation works. This def is always present, and never used.
	def = PR_GetDef(NULL, "end_sys_globals", NULL, false, 0);
	if(def) def->references++;

	def = PR_GetDef(NULL, "end_sys_fields", NULL, false, 0);
	if(def) def->references++;

	for (def = pr.def_head.next; def; def = def->next)
	{
		if (def->type->type == ev_vector || (def->type->type == ev_field && def->type->aux_type->type == ev_vector))
		{	
			// do the references, so we don't get loadsa not referenced VEC_HULL_MINS_x
			com.sprintf(element, "%s_x", def->name);
			comp_x = PR_GetDef(NULL, element, def->scope, false, 0);
			com.sprintf(element, "%s_y", def->name);
			comp_y = PR_GetDef(NULL, element, def->scope, false, 0);
			com.sprintf(element, "%s_z", def->name);
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
			if( opt_function_names && functions[G_FUNCTION(def->ofs)].first_statement < 0 )
			{
				def->name = "";
			}
			if( !def->timescalled )
			{
				if( def->references <= 1 )
					PR_Warning(WARN_DEADCODE, strings + def->s_file, def->s_line, "%s is never directly called or referenced (spawn function or dead code)", def->name);
			}
			if( opt_stripfunctions && def->timescalled >= def->references - 1 )	
			{
				// make sure it's not copied into a different var.
				// if it ever does self.think then it could be needed for saves.
				// if it's only ever called explicitly, the engine doesn't need to know.
				continue;
			}
		}
		else if( def->type->type == ev_field )
		{
			dd = &fields[numfielddefs];
			dd->type = def->type->aux_type->type;
			dd->s_name = PR_CopyString( def->name, opt_noduplicatestrings );
			dd->ofs = G_INT(def->ofs);
			numfielddefs++;
		}
		else if ((def->scope||def->constant) && (def->type->type != ev_string || opt_constant_names_strings))
		{
			if (opt_constant_names) continue;
		}

		dd = &qcc_globals[numglobaldefs];
		numglobaldefs++;

		if (opt_writetypes) dd->type = def->type-qcc_typeinfo;
		else dd->type = def->type->type;

		if ( def->saved && ((!def->initialized || def->type->type == ev_function) && def->type->type != ev_field && def->scope == NULL))
			dd->type |= DEF_SAVEGLOBAL;

		if (def->shared) dd->type |= DEF_SHARED;

		if (opt_locals && (def->scope || !com.strcmp(def->name, "IMMEDIATE")))
		{
			dd->s_name = 0;
		}
		else dd->s_name = PR_CopyString (def->name, opt_noduplicatestrings );
		dd->ofs = def->ofs;
	}
	if(numglobaldefs > MAX_GLOBALS) PR_ParseError(ERR_INTERNAL, "Too many globals - %i\n", numglobaldefs );

	strofs = (strofs + 3) & ~3;

	PR_Message("Linking...\n");
	if( host_instance == HOST_QCCLIB )
	{
		// don't flood into engine console
		MsgDev(D_INFO, "%6i strofs (of %i)\n", strofs, MAX_STRINGS);
		MsgDev(D_INFO, "%6i numstatements (of %i)\n", numstatements, MAX_STATEMENTS);
		MsgDev(D_INFO, "%6i numfunctions (of %i)\n", numfunctions, MAX_FUNCTIONS);
		MsgDev(D_INFO, "%6i numglobaldefs (of %i)\n", numglobaldefs, MAX_GLOBALS);
		MsgDev(D_INFO, "%6i numfielddefs (%i unique) (of %i)\n", numfielddefs, pr.size_fields, MAX_FIELDS);
		MsgDev(D_INFO, "%6i numpr_globals (of %i)\n", numpr_globals, MAX_REGS);	
	}	

	f = FS_Open( progsoutname, "wb" );
	if( !f ) PR_ParseError( ERR_INTERNAL, "Couldn't open file %s", progsoutname );
	FS_Write(f, &progs, sizeof(progs));
	FS_Write(f, "\r\n\r\n", 4);
	FS_Write(f, v_copyright, com.strlen(v_copyright) + 1);
	FS_Write(f, "\r\n\r\n", 4);

	progs.ofs_strings = FS_Tell(f);
	progs.numstrings = strofs;

	PR_WriteBlock(f, progs.ofs_strings, strings, strofs, progs.flags & COMP_STRINGS);

	progs.ofs_statements = FS_Tell(f);
	progs.numstatements = numstatements;

	for (i = 0; i < numstatements; i++)
	{
		statements[i].op = LittleLong(statements[i].op);
		statements[i].a = LittleLong(statements[i].a);
		statements[i].b = LittleLong(statements[i].b);
		statements[i].c = LittleLong(statements[i].c);
	}
	PR_WriteBlock(f, progs.ofs_statements, statements, progs.numstatements*sizeof(dstatement_t), progs.flags & COMP_STATEMENTS);

	progs.ofs_functions = FS_Tell(f);
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

	PR_WriteBlock(f, progs.ofs_functions, functions, progs.numfunctions*sizeof(dfunction_t), progs.flags & COMP_FUNCTIONS);

	progs.ofs_globaldefs = FS_Tell(f);
	progs.numglobaldefs = numglobaldefs;
	for (i = 0; i < numglobaldefs; i++)
	{
		qcc_globals[i].type = LittleLong(qcc_globals[i].type);
		qcc_globals[i].ofs = LittleLong(qcc_globals[i].ofs);
		qcc_globals[i].s_name = LittleLong(qcc_globals[i].s_name);
	}

	PR_WriteBlock(f, progs.ofs_globaldefs, qcc_globals, progs.numglobaldefs*sizeof(ddef_t), progs.flags & COMP_DEFS);

	progs.ofs_fielddefs = FS_Tell(f);
	progs.numfielddefs = numfielddefs;

	for (i = 0; i < numfielddefs; i++)
	{
		fields[i].type = LittleLong(fields[i].type);
		fields[i].ofs = LittleLong(fields[i].ofs);
		fields[i].s_name = LittleLong(fields[i].s_name);
	}

	PR_WriteBlock(f, progs.ofs_fielddefs, fields, progs.numfielddefs*sizeof(ddef_t), progs.flags & COMP_FIELDS);

	progs.ofs_globals = FS_Tell(f);
	progs.numglobals = numpr_globals;

	for (i = 0; (uint) i < numpr_globals; i++) ((int *)pr_globals)[i] = LittleLong (((int *)pr_globals)[i]);
	PR_WriteBlock(f, progs.ofs_globals, pr_globals, numpr_globals*4, progs.flags & COMP_GLOBALS);

	if(opt_writetypes)
	{
		for (i = 0; i < numtypeinfos; i++)
		{
			if (qcc_typeinfo[i].aux_type) qcc_typeinfo[i].aux_type = (type_t*)(qcc_typeinfo[i].aux_type - qcc_typeinfo);
			if (qcc_typeinfo[i].next) qcc_typeinfo[i].next = (type_t*)(qcc_typeinfo[i].next - qcc_typeinfo);
			qcc_typeinfo[i].name = (char *)PR_CopyString(qcc_typeinfo[i].name, true );
		}
	}

	progs.ident = VPROGSHEADER32;
	progs.version = VPROGS_VERSION;
	progs.ofssources = 0;
	progs.ofslinenums = 0;
	progs.ident = 0;
	progs.ofsbodylessfuncs = 0;
	progs.numbodylessfuncs = 0;
	progs.ofs_types = 0;
	progs.numtypes = 0;
	progs.ofsbodylessfuncs = FS_Tell(f);
	progs.numbodylessfuncs = PR_WriteBodylessFuncs(f);		

	if( opt_writelinenums )
	{
		progs.ofslinenums = FS_Tell(f);
		PR_WriteBlock(f, progs.ofslinenums, statement_linenums, numstatements*sizeof(int), progs.flags & COMP_LINENUMS);
	}

	if( opt_writetypes )
	{
		progs.ofs_types = FS_Tell(f);
		progs.numtypes = numtypeinfos;
		PR_WriteBlock(f, progs.ofs_types, qcc_typeinfo, progs.numtypes*sizeof(type_t), progs.flags & COMP_TYPES);
	}
	
	PR_WriteSourceFiles( f, &progs, opt_writesources );

	progsize = ((FS_Tell(f)+3) & ~3);
	progs.entityfields = pr.size_fields;
	progs.crc = crc;

	// byte swap the header and write it out
	for (i = 0; i < sizeof(progs)/4; i++)((int *)&progs)[i] = LittleLong (((int *)&progs)[i]);
	FS_Seek(f, 0, SEEK_SET);
	FS_Write(f, &progs, sizeof(progs));
	if( asmfile ) FS_Close(asmfile);

	if( host_instance == HOST_QCCLIB )
		MsgDev(D_INFO, "Writing %s, total size %i bytes\n", progsoutname, progsize );
	FS_Close( f );
}

/*
=======================
PR_UnmarshalLocals

marshalled locals remaps all the functions to use the range MAX_REGS 
onwards for the offset to thier locals.
this function remaps all the locals back into the function.
=======================
*/
void PR_UnmarshalLocals( void )
{
	def_t	*def;
	uint	ofs;
	uint	maxo;
	int	i;

	ofs = numpr_globals;
	maxo = ofs;

	for (def = pr.def_head.next ; def ; def = def->next)
	{
		if (def->ofs >= MAX_REGS)	//unmap defs.
		{
			def->ofs = def->ofs + ofs - MAX_REGS;
			if (maxo < def->ofs)
				maxo = def->ofs;
		}
	}

	for (i = 0; i < numfunctions; i++)
	{
		if (functions[i].parm_start == MAX_REGS)
			functions[i].parm_start = ofs;
	}

	PR_RemapOffsets(0, numstatements, MAX_REGS, MAX_REGS + maxo-numpr_globals + 3, ofs);

	numpr_globals = maxo+3;
	if (numpr_globals > MAX_REGS)
		PR_ParseError(ERR_INTERNAL, "Too many globals are in use to unmarshal all locals");

	if (maxo-ofs) PR_Message("Total of %i marshalled globals\n", maxo-ofs);
}

byte *PR_LoadFile( char *filename, bool crash, int type )
{
	int		length;
	cachedsourcefile_t	*newfile;
	string		fullname;
	byte		*file;
	char		*path;

	// NOTE: in-game we can't use ../pathes, but root directory always
	// ahead over ../pathes, so translate path from
	// ../common/svc_user.h to source/common/svc_user.h
	if( host_instance == HOST_NORMAL || host_instance == HOST_DEDICATED )
	{
		if((path = com.strstr( filename, ".." )))
		{
			path += 2; // skip ..
			com.snprintf( fullname, MAX_STRING, "%s%s", GI->source_dir, path );
		}
		else com.snprintf( fullname, MAX_STRING, "%s/%s/%s", GI->source_dir, sourcedir, filename );
	}
	else com.strncpy( fullname, filename, MAX_STRING );
 
	file = FS_LoadFile( fullname, &length );
	
	if( !file || !length ) 
	{
		if( crash ) PR_ParseError( ERR_INTERNAL, "Couldn't open file %s", fullname );
		else return NULL;
	}
	newfile = (cachedsourcefile_t*)Qalloc(sizeof(cachedsourcefile_t));
	newfile->next = sourcefile;
	sourcefile = newfile; // make chain
          
	com.strcpy(sourcefile->filename, fullname );
	sourcefile->file = file;
	sourcefile->type = type;
	sourcefile->size = length;

	return sourcefile->file;
}

bool PR_Include( char *filename )
{
	char		*newfile;
	char		fname[512];
	char		*opr_file_p;
	string_t		os_file, os_file2;
	int		opr_source_line;
	char		*ocompilingfile;
	includechunk_t	*oldcurrentchunk;

	ocompilingfile = compilingfile;
	os_file = s_file;
	os_file2 = s_file2;
	opr_source_line = pr_source_line;
	opr_file_p = pr_file_p;
	oldcurrentchunk = currentchunk;

	com.strcpy(fname, filename);
	newfile = QCC_LoadFile( fname, true );
	currentchunk = NULL;
	pr_file_p = newfile;
	PR_CompileFile( newfile, fname );
	currentchunk = oldcurrentchunk;

	compilingfile = ocompilingfile;
	s_file = os_file;
	s_file2 = os_file2;
	pr_source_line = opr_source_line;
	pr_file_p = opr_file_p;

	return true;
}

void PR_WriteBlock(file_t *f, fs_offset_t pos, const void *data, size_t blocksize, bool compress)
{
	vfile_t	*h;
	int	len = 0;

	if (compress)
	{		
		h = VFS_Open( f, "wz" );
		VFS_Write( h, data, blocksize );	// write into buffer
		FS_Write(f, &len, sizeof(int));	// first four bytes it's a compressed filesize
		f = VFS_Close(h);			// deflate, then write into disk
		len = LittleLong(FS_Tell(f) - pos);	// calculate complength
		FS_Seek(f, pos, SEEK_SET);		// seek back to start block...
		FS_Write(f, &len, sizeof(int));	// ... and write real complength value.
		FS_Seek(f, 0, SEEK_END);		// return
	}
	else FS_Write(f, data, blocksize);		// just write data
}

byte *PR_CreateProgsSRC( void )
{
	search_t		*qc = FS_Search( "*", true );
	const char	*datname = "unknown.dat\n";	// outname will be set by PR_WriteProgdefs
	byte		*newprogs_src = NULL;
	char		searchmask[8][16];
	string		headers[2];		// contains filename with struct description
	bool		have_entvars = 0;
	bool		have_globals = 0;
	int		i, k, j = 0;

	// hardcoded table! don't change!
	com.strcpy( searchmask[0], "qh" );	// quakec header
	com.strcpy( searchmask[1], "h" );	// c-style header
	com.strcpy( searchmask[2], "qc" );	// quakec sources
	com.strcpy( searchmask[3], "c" );	// c-style sources

	if(!qc)
	{
		PR_ParseError(ERR_INTERNAL, "Couldn't open file progs.src" );
		return NULL;
	}
	Mem_Set( headers, '/0', MAX_STRING * 2 );

	for( i = 0; i < qc->numfilenames; i++ )
	{
		// search by mask		
		for( k = 0; k < 8; k++)
		{
			// skip blank mask
			if(!com.strlen( searchmask[k] )) continue;
			if(!com.stricmp( searchmask[k], FS_FileExtension( qc->filenames[i] ))) // validate ext
			{
				script_t	*source = Com_OpenScript( qc->filenames[i], NULL, 0 );
				token_t	token;

				if( source )
				{
					while( Com_ReadToken( source, SC_ALLOW_NEWLINES, &token ))
					{
						// parse all sources for "end_sys_globals"
						if( !com.strcmp( token.string, "end_sys_globals" ))
						{
							com.strncpy( headers[0], qc->filenames[i], MAX_STRING );
							have_globals = true;
						}
						else if( !com.strcmp( token.string, "end_sys_fields" ))
						{
							com.strncpy( headers[1], qc->filenames[i], MAX_STRING );
							have_entvars = true;
						}
						if( have_globals && have_entvars )
						{
							Com_CloseScript( source );
							goto buildnewlist; // end of parsing
						}
					}
					Com_CloseScript( source );
				}
			}	
		}
	}

	// globals and locals not declared
	PR_ParseError( ERR_INTERNAL, "Couldn't open file progs.src" );
	return NULL;
buildnewlist:

	newprogs_src = Qrealloc( newprogs_src, j + com.strlen( datname ) + 1 ); // outfile name
	Mem_Copy( newprogs_src + j, (char *)datname, com.strlen( datname ));
	j += com.strlen(datname) + 1; // null term

	// file contains "sys_globals" and possible "sys_fields"
	newprogs_src = Qrealloc(newprogs_src, j + com.strlen(headers[0]) + 2); // first file
	com.strncat(newprogs_src, va("%s\n", headers[0]), com.strlen(headers[0]) + 1);
	j += com.strlen(headers[0]) + 2; //null term

	if(STRCMP( headers[0], headers[1] ))
	{
		// file contains sys_fields description
		newprogs_src = Qrealloc( newprogs_src, j + com.strlen( headers[1] ) + 2 ); // second file (option)
		com.strncat( newprogs_src, va("%s\n", headers[1]), com.strlen(headers[1]) + 1);
		j += com.strlen(headers[1]) + 2; //null term
	}

	// add headers
	for( i = 0; i < qc->numfilenames; i++ )
	{
		for( k = 0; k < 2; k++)
		{
			// skip blank mask
			if(!com.strlen(searchmask[k])) continue;
			if(!com.stricmp(searchmask[k], FS_FileExtension(qc->filenames[i]))) // validate ext
			{
				if(!com.strcmp(qc->filenames[i], headers[0]) || !com.strcmp(qc->filenames[i], headers[1]))
					break; //we already have it, just skip
				newprogs_src = Qrealloc( newprogs_src, j + com.strlen(qc->filenames[i]) + 2);
				com.strncat(newprogs_src, va("%s\n", qc->filenames[i]), com.strlen(qc->filenames[i]) + 1);
				j += com.strlen(qc->filenames[i]) + 2;
			}
		}
	}
	
	// add other sources
	for(i = 0; i < qc->numfilenames; i++)
	{
		for( k = 2; k < 8; k++)
		{
			// skip blank mask
			if(!com.strlen(searchmask[k])) continue;
			if(!com.stricmp(searchmask[k], FS_FileExtension(qc->filenames[i]))) // validate ext
			{
				if(!com.strcmp(qc->filenames[i], headers[0]) || !com.strcmp(qc->filenames[i], headers[1]))
					break; //we already have it, just skip
				newprogs_src = Qrealloc( newprogs_src, j + com.strlen(qc->filenames[i]) + 2);
				com.strncat(newprogs_src, va("%s\n", qc->filenames[i]), com.strlen(qc->filenames[i]) + 1);
				j += com.strlen(qc->filenames[i]) + 2;
			}
		}
	}
	Mem_Free( qc ); // free search
	FS_WriteFile( "progs.src", newprogs_src, j );

	return newprogs_src;
}