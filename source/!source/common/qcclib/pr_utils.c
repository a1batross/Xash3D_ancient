//=======================================================================
//			Copyright XashXT Group 2007 ©
//			qcc_utils.c - qcc common tools
//=======================================================================

#include "qcclib.h"

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
	bucket_t *buck;

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
	bucket_t *buck;

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
	bucket_t *buck;

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
	bucket_t *buck;	

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
	bucket_t *buck;	

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
	bucket_t *buck;	

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
	case CMPW_COPY:
		if (complen != len) 
		{
			MsgDev(D_WARN, "PR_decode: complen[%d] != len[%d]\n", complen, len);
			return false;
		}		
		Mem_Copy(buffer, src, len);
		break;
	case CMPW_ENCRYPT:
		if (complen != len) 
		{
			MsgDev(D_WARN, "PR_decode: complen[%d] != len[%d]\n", complen, len);
			return false;
		}
		for (i = 0; i < len; i++) buffer[i] = src[i] ^ 0xA5;
		break;
	case CMPW_DEFLATE:
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
	case CMPW_COPY:
		FS_Write(h, src, len);
		return len;
	case CMPW_ENCRYPT:
		for (i = 0; i < len; i++) src[i] = src[i] ^ 0xA5;
		FS_Write(h, src, len);
		return len;
	case CMPW_DEFLATE:
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
			if (!strcmp(s, str)) return s - strings;
	}

	old = strofs;
	strcpy (strings + strofs, str);
	strofs += strlen(str)+1;
	return old;
}

/*
================
PR_WriteSourceFiles

include sources into progs.dat for fake decompile
if it needs
================
*/
int PR_WriteSourceFiles(file_t *h, dprograms_t *progs, bool sourceaswell)
{
	includeddatafile_t	*idf;
	cachedsourcefile_t	*f;
	int		num = 0;
	int		ofs;

	for (f = sourcefile, num = 0; f; f = f->next)
	{
		if (f->type == FT_CODE && !sourceaswell)
			continue;
		num++;
	}
	if (!num) return 0;

	idf = Qalloc(sizeof(includeddatafile_t) * num);
	for (f = sourcefile, num = 0; f; f = f->next)
	{
		if (f->type == FT_CODE && !sourceaswell)
			continue;
		strcpy(idf[num].filename, f->filename);
		idf[num].size = f->size;
		idf[num].compmethod = CMPW_DEFLATE;
		idf[num].ofs = FS_Tell(h);
		idf[num].compsize = PR_encode(f->size, idf[num].compmethod, f->file, h);
		num++;
	}
	ofs = FS_Tell(h);	
	FS_Write(h, &num, sizeof(int));
	FS_Write(h, idf, sizeof(includeddatafile_t)*num);
	sourcefile = NULL;

	return ofs;
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
				FS_Write(handle, d->name, strlen(d->name)+1);
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
	int	i = strlen(file);

	if (i + strlen(p) + 1 >= PROGDEFS_MAX_SIZE)
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
word PR_WriteProgdefs (char *filename)
{
	char		file[PROGDEFS_MAX_SIZE];
	char		header_name[MAX_QPATH];
	def_t		*d;
	int		f = 0, k = 1;
	word		crc;

	CRC_Init (&crc);
	memset(file, 0, PROGDEFS_MAX_SIZE);
	FS_FileBase( progsoutname, header_name );
	strupr( header_name ); // as part of define name
	
	ADD2("\n// progdefs.h\n// generated by Xash3D QuakeC compiler\n\n#ifndef PROGDEFS_H\n#define PROGDEFS_H\n");
	ADD3("\n/* file generated by qcc, do not modify */\n"); // this comment as part of crc
	ADD1("\ntypedef struct");
	ADD2(" globalvars_s");
	ADD1(va("\n{"));	
	ADD2(va("\n"));
	ADD1(va("\tint\tpad[%i];\n", RESERVED_OFS));

	// write globalvars_t
	for (d = pr.def_head.next; d; d = d->next)
	{
		if (!strcmp (d->name, "end_sys_globals")) break;
		if (d->ofs < RESERVED_OFS) continue;
			
		switch (d->type->type)
		{
		case ev_float:
			ADD1(va("\tfloat\t%s;\n",d->name));
			break;
		case ev_vector:
			ADD1(va("\tvec3_t\t%s;\n",d->name));
			d = d->next->next->next; // skip the elements
			break;
		case ev_string:
			ADD1(va("\tstring_t\t%s;\n",d->name));
			break;
		case ev_function:
			ADD1(va("\tfunc_t\t%s;\n",d->name));
			break;
		case ev_entity:
			ADD1(va("\tint\t%s;\n",d->name));
			break;
		case ev_integer:
			ADD1(va("\tint\t%s;\n",d->name));
			break;
		default:
			ADD1(va("\tint\t%s;\n",d->name));
			break;
		}
	}
	ADD1("} globalvars_t;\n\n");

	// write entvars_t
	ADD1("typedef struct");
	ADD2(" entvars_s");
	ADD1("\n{\n");

	for (d = pr.def_head.next; d; d = d->next)
	{
		if (!strcmp (d->name, "end_sys_fields")) break;
		if (d->type->type != ev_field) continue;
			
		switch (d->type->aux_type->type)
		{
		case ev_float:
			ADD1(va("\tfloat\t%s;\n",d->name));
			break;
		case ev_vector:
			ADD1(va("\tvec3_t\t%s;\n",d->name));
			d = d->next->next->next; // skip the elements
			break;
		case ev_string:
			ADD1(va("\tstring_t\t%s;\n",d->name));
			break;
		case ev_function:
			ADD1(va("\tfunc_t\t%s;\n",d->name));
			break;
		case ev_entity:
			ADD1(va("\tint\t%s;\n",d->name));
			break;
		case ev_integer:
			ADD1(va("\tint\t%s;\n",d->name));
			break;
		default:
			ADD1(va("\tint\t%s;\n",d->name));
			break;
		}
	}
	ADD1("} entvars_t;\n\n");

	// begin custom fields
	ADD2("\n#define REQFIELDS (sizeof(reqfields) / sizeof(fields_t))\n");
	ADD2("\nstatic fields_t reqfields[] = \n{\n");
	
	// write fields
	for (d = pr.def_head.next; d; d = d->next)
	{
		if (!strcmp (d->name, "end_sys_fields")) k = 0;
		if (d->type->type != ev_field) continue;
		if (k) continue; //write only user fields
		if (f) ADD2(",\n");	
		f = 1;

		switch (d->type->aux_type->type)
		{
		case ev_float:
			ADD2(va("\t{%i,\t%i,\t\"%s\"}",G_INT(d->ofs), d->type->aux_type->type, d->name));
			break;
		case ev_vector:
			ADD2(va("\t{%i,\t%i,\t\"%s\"}",G_INT(d->ofs), d->type->aux_type->type, d->name));
			d = d->next->next->next; // skip the elements
			break;
		case ev_string:
			ADD2(va("\t{%i,\t%i,\t\"%s\"}",G_INT(d->ofs), d->type->aux_type->type, d->name));
			break;
		case ev_function:
			ADD2(va("\t{%i,\t%i,\t\"%s\"}",G_INT(d->ofs), d->type->aux_type->type, d->name));
			break;
		case ev_entity:
			ADD2(va("\t{%i,\t%i,\t\"%s\"}",G_INT(d->ofs), d->type->aux_type->type, d->name));
			break;
		case ev_integer:
			ADD2(va("\t{%i,\t%i,\t\"%s\"}",G_INT(d->ofs), d->type->aux_type->type, d->name));
			break;
		default:
			ADD2(va("\t{%i,\t%i,\t\"%s\"}",G_INT(d->ofs), d->type->aux_type->type, d->name));
			break;
		}
	}
	ADD2("\n};\n\n");

	switch(target_version)
	{
	case QPROGS_VERSION:
		ADD2(va("#define PROGHEADER_CRC %i\n", crc ));
		break;
	case FPROGS_VERSION:
	case VPROGS_VERSION:
		ADD2(va("#define PROG_CRC_%s\t%i\n", header_name, crc ));
		break;
	}
	ADD2("\n#endif//PROGDEFS_H");
	if(ForcedCRC) crc = ForcedCRC;

	if (FS_CheckParm("-progdefs"))
	{
		PR_Message("writing %s\n", filename);
		FS_WriteFile(filename, file, strlen(file));
	}

	switch (crc)
	{
	case 54730:
		PR_Message("QuakeWorld unmodified qwprogs.dat\n");
		if(!strcmp(progsoutname, "unknown.dat")) strcpy(progsoutname, "qwprogs.dat");
		break;
	case 32401:
		PR_Message("Tenebrae unmodified progs.dat\n");
		if(!strcmp(progsoutname, "unknown.dat")) strcpy(progsoutname, "progs.dat");
		break;
	case 5927:
		PR_Message("Quake1 unmodified progs.dat\n");
		if(!strcmp(progsoutname, "unknown.dat")) strcpy(progsoutname, "progs.dat");
		break;
	case 64081:
		PR_Message("Xash3D unmodified server.dat\n");
		if(!strcmp(progsoutname, "unknown.dat")) strcpy(progsoutname, "server.dat");
		break;
	case 12923:
		PR_Message("Blank progs.dat\n");
		if(!strcmp(progsoutname, "unknown.dat")) strcpy(progsoutname, "blank.dat");
		break;
	default:
		PR_Message("Custom progs crc %d\n", crc );
		if(!strcmp(progsoutname, "unknown.dat")) strcpy(progsoutname, "progs.dat");
		break;
	}
	return crc;
}

void PR_WriteLNOfile(char *destname)
{
	dlno_t	lno;
	file_t	*f;
	char	filename[MAX_QPATH];

	if(!opt_writelinenums) return;

	strncpy( filename, destname, MAX_QPATH );
	FS_StripExtension( filename );
	FS_DefaultExtension( filename, ".lno" );

	lno.header = LINENUMSHEADER;
	lno.version = LNNUMS_VERSION; 
	lno.numglobaldefs = numglobaldefs;
	lno.numglobals = numpr_globals;
	lno.numfielddefs = numfielddefs;
	lno.numstatements = numstatements;

	f = FS_Open( filename, "w" );
	FS_Write(f, &lno, sizeof(dlno_t)); // header
	FS_Write(f, statement_linenums, numstatements * sizeof(int));

	MsgDev(D_INFO, "Writing %s, total size %d bytes\n", filename, ((FS_Tell(f)+3) & ~3));
	FS_Close( f );
}

void PR_WriteDAT( void )
{
	ddef_t		*dd;
	file_t		*f;
	dprograms_t	progs; // header
	char		element[MAX_NAME];
	int		i, progsize, num_ref;
	int		crc, outputsize = 16;
	def_t		*def, *comp_x, *comp_y, *comp_z;

	crc = PR_WriteProgdefs ("progdefs.h"); // write progdefs.h

	progs.blockscompressed = 0;

	if(numstatements > MAX_STATEMENTS) PR_ParseError(ERR_INTERNAL, "Too many statements - %i\n", numstatements );
	if(strofs > MAX_STRINGS) PR_ParseError(ERR_INTERNAL, "Too many strings - %i\n", strofs );

	PR_UnmarshalLocals();

	switch (target_version)
	{
	case QPROGS_VERSION:
		if (bodylessfuncs) PR_Message("warning: There are some functions without bodies.\n");
		if( numpr_globals <= 65530 )
		{
			for (i = 0; pr_optimisations[i].enabled; i++)
			{
				if(*pr_optimisations[i].enabled && pr_optimisations[i].flags & FLAG_V7_ONLY)
					*pr_optimisations[i].enabled = false; // uncompatiable
			}
			// not much of a different format. Rewrite output to get it working on original executors?
			if (numpr_globals >= 32768) 
				PR_Warning(WARN_IMAGETOOBIG, NULL, 0, "globals limit exceeds 32768, image may not run\n");
			break;
		}
		else
		{
			target_version = FPROGS_VERSION;
			outputsize = 32;
			PR_Message("force target to version %d[%dbit]\n", target_version, outputsize );
		}
		// intentional falltrough
	case FPROGS_VERSION:
		if (numpr_globals > 65530) outputsize = 32;
		if(opt_compstrings) progs.blockscompressed |= COMP_STRINGS;
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

		// compression of blocks?
		if (opt_writelinenums) progs.blockscompressed |= COMP_LINENUMS;	
		if (opt_writetypes) progs.blockscompressed |= COMP_TYPES;
		break;
	case VPROGS_VERSION:
		outputsize = 32; //as default		
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
			// do the references, so we don't get loadsa not referenced VEC_HULL_MINS_x
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
			dd->type = def->type->aux_type->type;
			dd->s_name = PR_CopyString (def->name, opt_noduplicatestrings );
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

		if (opt_locals && (def->scope || !strcmp(def->name, "IMMEDIATE")))
		{
			dd->s_name = 0;
		}
		else dd->s_name = PR_CopyString (def->name, opt_noduplicatestrings );
		dd->ofs = def->ofs;
	}
	if(numglobaldefs > MAX_GLOBALS) PR_ParseError(ERR_INTERNAL, "Too many globals - %i\n", numglobaldefs );

	strofs = (strofs + 3) & ~3;

	PR_Message("Linking...\n");
	MsgDev(D_INFO, "%6i strofs (of %i)\n", strofs, MAX_STRINGS);
	MsgDev(D_INFO, "%6i numstatements (of %i)\n", numstatements, MAX_STATEMENTS);
	MsgDev(D_INFO, "%6i numfunctions (of %i)\n", numfunctions, MAX_FUNCTIONS);
	MsgDev(D_INFO, "%6i numglobaldefs (of %i)\n", numglobaldefs, MAX_GLOBALS);
	MsgDev(D_INFO, "%6i numfielddefs (%i unique) (of %i)\n", numfielddefs, pr.size_fields, MAX_FIELDS);
	MsgDev(D_INFO, "%6i numpr_globals (of %i)\n", numpr_globals, MAX_REGS);	
	
	f = FS_Open( progsoutname, "wb" );
	FS_Write(f, &progs, sizeof(progs));
	FS_Write(f, "\r\n\r\n", 4);
	FS_Write(f, v_copyright, strlen(v_copyright) + 1);
	FS_Write(f, "\r\n\r\n", 4);

	progs.ofs_strings = FS_Tell(f);
	progs.numstrings = strofs;

	PR_WriteBlock(f, progs.ofs_strings, strings, strofs, progs.blockscompressed & COMP_STRINGS);

	progs.ofs_statements = FS_Tell(f);
	progs.numstatements = numstatements;

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

		PR_WriteBlock(f, progs.ofs_statements, statements32, progs.numstatements*sizeof(dstatement32_t), progs.blockscompressed & COMP_STATEMENTS);
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
		PR_WriteBlock(f, progs.ofs_statements, statements16, progs.numstatements*sizeof(dstatement16_t), progs.blockscompressed & COMP_STATEMENTS);
		break;
	}

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

	PR_WriteBlock(f, progs.ofs_functions, functions, progs.numfunctions*sizeof(dfunction_t), progs.blockscompressed & COMP_FUNCTIONS);

	switch(outputsize)
	{
	case 32:
		progs.ofs_globaldefs = FS_Tell(f);
		progs.numglobaldefs = numglobaldefs;
		for (i = 0; i < numglobaldefs; i++)
		{
			qcc_globals32[i].type = LittleLong(qcc_globals32[i].type);
			qcc_globals32[i].ofs = LittleLong(qcc_globals32[i].ofs);
			qcc_globals32[i].s_name = LittleLong(qcc_globals32[i].s_name);
		}

		PR_WriteBlock(f, progs.ofs_globaldefs, qcc_globals32, progs.numglobaldefs*sizeof(ddef32_t), progs.blockscompressed & COMP_DEFS);

		progs.ofs_fielddefs = FS_Tell(f);
		progs.numfielddefs = numfielddefs;

		for (i = 0; i < numfielddefs; i++)
		{
			fields32[i].type = LittleLong(fields32[i].type);
			fields32[i].ofs = LittleLong(fields32[i].ofs);
			fields32[i].s_name = LittleLong(fields32[i].s_name);
		}

		PR_WriteBlock(f, progs.ofs_fielddefs, fields32, progs.numfielddefs*sizeof(ddef32_t), progs.blockscompressed & COMP_FIELDS);
		break;
	case 16:
	default:
		progs.ofs_globaldefs = FS_Tell(f);
		progs.numglobaldefs = numglobaldefs;
		for (i = 0; i < numglobaldefs; i++)
		{
			qcc_globals16[i].type = (word)LittleShort ((word)qcc_globals32[i].type);
			qcc_globals16[i].ofs = (word)LittleShort ((word)qcc_globals32[i].ofs);
			qcc_globals16[i].s_name = LittleLong(qcc_globals32[i].s_name);
		}

		PR_WriteBlock(f, progs.ofs_globaldefs, qcc_globals16, progs.numglobaldefs*sizeof(ddef16_t), progs.blockscompressed & COMP_DEFS);

		progs.ofs_fielddefs = FS_Tell(f);
		progs.numfielddefs = numfielddefs;

		for (i = 0; i < numfielddefs; i++)
		{
			fields16[i].type = (word)LittleShort ((word)fields32[i].type);
			fields16[i].ofs = (word)LittleShort ((word)fields32[i].ofs);
			fields16[i].s_name = LittleLong (fields32[i].s_name);
		}

		PR_WriteBlock(f, progs.ofs_fielddefs, fields16, progs.numfielddefs*sizeof(ddef16_t), progs.blockscompressed & COMP_FIELDS);
		break;
	}

	progs.ofs_globals = FS_Tell(f);
	progs.numglobals = numpr_globals;

	for (i = 0; (uint) i < numpr_globals; i++) ((int *)pr_globals)[i] = LittleLong (((int *)pr_globals)[i]);
	PR_WriteBlock(f, progs.ofs_globals, pr_globals, numpr_globals*4, progs.blockscompressed & COMP_GLOBALS);

	if(opt_writetypes)
	{
		for (i = 0; i < numtypeinfos; i++)
		{
			if (qcc_typeinfo[i].aux_type) qcc_typeinfo[i].aux_type = (type_t*)(qcc_typeinfo[i].aux_type - qcc_typeinfo);
			if (qcc_typeinfo[i].next) qcc_typeinfo[i].next = (type_t*)(qcc_typeinfo[i].next - qcc_typeinfo);
			qcc_typeinfo[i].name = (char *)PR_CopyString(qcc_typeinfo[i].name, true );
		}
	}

	progs.ofsfiles = 0;
	progs.ofslinenums = 0;
	progs.ident = 0;
	progs.ofsbodylessfuncs = 0;
	progs.numbodylessfuncs = 0;
	progs.ofs_types = 0;
	progs.numtypes = 0;
	progs.version = target_version;

	switch(target_version)
	{
	case QPROGS_VERSION:
		PR_WriteLNOfile( progsoutname );
		break;
	case FPROGS_VERSION:
		if (outputsize == 16) progs.ident = VPROGSHEADER16;
		if (outputsize == 32) progs.ident = VPROGSHEADER32;

		progs.ofsbodylessfuncs = FS_Tell(f);
		progs.numbodylessfuncs = PR_WriteBodylessFuncs(f);		

		if (opt_writelinenums)
		{
			progs.ofslinenums = FS_Tell(f);
			PR_WriteBlock(f, progs.ofslinenums, statement_linenums, numstatements*sizeof(int), progs.blockscompressed & COMP_LINENUMS);
		}
		else progs.ofslinenums = 0;

		if (opt_writetypes)
		{
			progs.ofs_types = FS_Tell(f);
			progs.numtypes = numtypeinfos;
			PR_WriteBlock(f, progs.ofs_types, qcc_typeinfo, progs.numtypes*sizeof(type_t), progs.blockscompressed & COMP_TYPES);
		}
		else
		{
			progs.ofs_types = 0;
			progs.numtypes = 0;
		}
		progs.ofsfiles = PR_WriteSourceFiles(f, &progs, opt_writesources);
		break;
	case VPROGS_VERSION:
		break;
	}

	progsize = ((FS_Tell(f)+3) & ~3);
	progs.entityfields = pr.size_fields;
	progs.crc = crc;

	// byte swap the header and write it out
	for (i = 0; i < sizeof(progs)/4; i++)((int *)&progs)[i] = LittleLong (((int *)&progs)[i]);
	FS_Seek(f, 0, SEEK_SET);
	FS_Write(f, &progs, sizeof(progs));
	if (asmfile) FS_Close(asmfile);

	MsgDev(D_INFO, "Writing %s, total size %i bytes\n", progsoutname, progsize );
	FS_Close(f);
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

byte *PR_LoadFile(char *filename, bool crash, int type )
{
	int	length;
	cachedsourcefile_t	*newfile;
	byte	*file = FS_LoadFile( filename, &length );
	
	if (!length) 
	{
		if(crash) PR_ParseError(ERR_INTERNAL, "Couldn't open file %s", filename);
		else return NULL;
	}
	newfile = (cachedsourcefile_t*)Qalloc( sizeof(cachedsourcefile_t) );
	newfile->next = sourcefile;
	sourcefile = newfile; // make chain
          
	strcpy(sourcefile->filename, filename);
	sourcefile->file = file;
	sourcefile->type = type;
	sourcefile->size = length;

	return sourcefile->file;
}

bool PR_Include(char *filename)
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

	strcpy(fname, filename);
	newfile = QCC_LoadFile(fname, true );
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
	search_t		*qc = FS_Search( "*.qc", true );
	const char	*datname = "unknown.dat\n";	// outname will be set by PR_WriteProgdefs
	byte		*newprogs_src = NULL;
	char		headers[2][MAX_QPATH];	// contains filename with struct description
	int		i, j = 0, found = 0;

	if(!qc) 
	{
		PR_ParseError(ERR_INTERNAL, "Couldn't open file progs.src" );
		return NULL;
	}
	memset(headers, '/0', 2 * MAX_QPATH);

	for(i = 0; i < qc->numfilenames; i++)
	{
		if(Com_LoadScript( qc->filenames[i], NULL, 0 ))
		{
			while ( 1 )
			{
				// parse all sources for "end_sys_globals"
				if (!Com_GetToken( true )) break; //EOF
				if(Com_MatchToken( "end_sys_globals" ))
				{
					strncpy(headers[0], qc->filenames[i], MAX_QPATH );
					found++;
				}
				else if(Com_MatchToken( "end_sys_fields" ))
				{
					strncpy(headers[1], qc->filenames[i], MAX_QPATH );
					found++;
				}
				if(found > 1) goto buildnewlist; // end of parsing
			}
		}
	}	

buildnewlist:

	newprogs_src = Qrealloc(newprogs_src, j + strlen(datname) + 1); // outfile name
	Mem_Copy(newprogs_src + j, (char *)datname, strlen(datname));
	j += strlen(datname) + 1; // null term

	// file contains "sys_globals" and possible "sys_fields"
	newprogs_src = Qrealloc(newprogs_src, j + strlen(headers[0]) + 2); // first file
	strncat(newprogs_src, va("%s\n", headers[0]), strlen(headers[0]) + 1);
	j += strlen(headers[0]) + 2; //null term

	if(STRCMP(headers[0], headers[1] ))
	{
		// file contains sys_fields description
		newprogs_src = Qrealloc(newprogs_src, j + strlen(headers[1]) + 2); // second file (option)
		strncat(newprogs_src, va("%s\n", headers[1]), strlen(headers[1]) + 1);
		j += strlen(headers[1]) + 2; //null term
	}
	// add other sources
	for(i = 0; i < qc->numfilenames; i++)
	{
		if(!strcmp(qc->filenames[i], headers[0]) || !strcmp(qc->filenames[i], headers[1]))
			continue; //we already have it, just skip
		newprogs_src = Qrealloc( newprogs_src, j + strlen(qc->filenames[i]) + 2);
		strncat(newprogs_src, va("%s\n", qc->filenames[i]), strlen(qc->filenames[i]) + 1);
		j += strlen(qc->filenames[i]) + 2;
	}
	autoprototype = true;
	Free( qc ); // free search
	FS_WriteFile("progs.src", newprogs_src, j );

	return newprogs_src;
}