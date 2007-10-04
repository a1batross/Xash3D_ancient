//=======================================================================
//			Copyright XashXT Group 2007 ©
//			qcc_utils.c - qcc common tools
//=======================================================================

#include "qcclib.h"
#include "zip32.h"

void Hash_InitTable(hashtable_t *table, int numbucks, void *mem)
{
	table->numbuckets = numbucks;
	table->bucket = (bucket_t **)mem;
}

int Hash_Key(char *name, int modulus)
{
	//fixme: optimize.
	uint key;
	for (key = 0; *name; name++)
		key += ((key<<3) + (key>>28) + *name);
		
	return (int)(key%modulus);
}

void *Hash_Get(hashtable_t *table, char *name)
{
	int bucknum = Hash_Key(name, table->numbuckets);
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
	int bucknum = key%table->numbuckets;
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
	int bucknum = Hash_Key(name, table->numbuckets);
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
	int bucknum = Hash_Key(name, table->numbuckets);

	buck->data = data;
	buck->keystring = name;
	buck->next = table->bucket[bucknum];
	table->bucket[bucknum] = buck;

	return buck;
}

void *Hash_AddKey(hashtable_t *table, int key, void *data, bucket_t *buck)
{
	int bucknum = key%table->numbuckets;

	buck->data = data;
	buck->keystring = (char*)key;
	buck->next = table->bucket[bucknum];
	table->bucket[bucknum] = buck;

	return buck;
}

void Hash_Remove(hashtable_t *table, char *name)
{
	int bucknum = Hash_Key(name, table->numbuckets);
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
	int bucknum = Hash_Key(name, table->numbuckets);
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
	int bucknum = key%table->numbuckets;
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
	return;
}

/*
================
PR_decode
================
*/
int PR_decode(int complen, int len, int method, char *info, char **data)
{
	int	i;
	char	*buffer = *data;
	
	if (method == 0)	 // copy
	{
		if (complen != len) Sys_Error("lengths do not match");
		Mem_Copy(buffer, info, len);		
	}
	else if (method == 1)// encryption
	{
		if (complen != len) 
		{
			MsgDev(D_WARN, "lengths do not match");
			return false;
		}
		for (i = 0; i < len; i++) buffer[i] = info[i] ^ 0xA5;		
	}
	else if (method == 2)// compression (ZLIB)
	{
		z_stream strm = {info, complen, 0, buffer, len, 0, NULL, NULL, NULL, NULL, NULL, 0, 0, 0 };
		inflateInit( &strm );

		// decompress it in one go.
		if (Z_STREAM_END != inflate( &strm, Z_FINISH ))
		{
			Sys_Error("Failed block decompression: %s\n", strm.msg );
			return false;
		}
		inflateEnd( &strm );
	}
	else 
	{
		MsgDev(D_WARN, "PR_decode: Bad file encryption routine\n");
		return false;
          }
	return true;
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
int PR_WriteSourceFiles(vfile_t *h, dprograms_t *progs, bool sourceaswell)
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
		idf[num].compmethod = 1;
		idf[num].ofs = VFS_Tell(h);
		idf[num].compsize = PR_encode(f->size, idf[num].compmethod, f->file, h);
		num++;
	}

	ofs = VFS_Tell(h);	
	VFS_Write(h, &num, sizeof(int));
	VFS_Write(h, idf, sizeof(includeddatafile_t)*num);
	sourcefile = NULL;

	return ofs;
}

int PR_WriteBodylessFuncs (vfile_t *handle)
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
				VFS_Write(handle, d->name, strlen(d->name)+1);
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
	int		f = 0;
	word		crc;

	CRC_Init (&crc);
	memset(file, 0, PROGDEFS_MAX_SIZE);
	strncpy(header_name, destfile, MAX_QPATH);
	FS_StripExtension( header_name );
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

	// prvm_fieldvars_t struct
	ADD2("\ntypedef struct prvm_fieldvars_s\n{\n\tint\t\tofs;\n\tint\t\ttype;\n\tconst char\t*name;\n} prvm_fieldvars_t;\n");
	ADD2("\n#define REQFIELDS (sizeof(reqfields) / sizeof(prvm_fieldvars_t))\n");
	ADD2("\nstatic prvm_fieldvars_t reqfields[] = \n{\n");
	
	// write fields
	for (d = pr.def_head.next; d; d = d->next)
	{
		if (!strcmp (d->name, "end_sys_fields")) break;
		if (d->type->type != ev_field) continue;
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

	switch(targetformat)
	{
	case QCF_STANDARD:
		ADD2(va("#define PROGHEADER_CRC %i\n", crc ));
		break;
	case QCF_DEBUG:
	case QCF_RELEASE:
		ADD2(va("#define PROG_CRC_%s\t%i\n", header_name, crc ));
		break;
	}
	ADD2("\n#endif//PROGDEFS_H");
	if(ForcedCRC) crc = ForcedCRC;

	if (FS_CheckParm("-progdefs"))
	{
		PR_Message("writing %s\n", filename);
		FS_WriteFile (filename, file, strlen(file));
	}

	switch (crc)
	{
	case 54730:
		PR_Message("Recognized progs as QuakeWorld\n");
		break;
	case 5927:
		PR_Message("Recognized progs as regular Quake\n");
		break;
	case 38488:
		PR_Message("Recognized progs as original Hexen2\n");
		break;
	case 26905:
		PR_Message("Recognized progs as Hexen2 Mission Pack\n");
		break;
	}
	return crc;
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
		Sys_Error("Too many globals are in use to unmarshal all locals");

	if (maxo-ofs)
		Msg("Total of %i marshalled globals\n", maxo-ofs);
}

/*
================
PR_encode
================
*/
int PR_encode(int len, int method, char *in, vfile_t *handle)
{
	int i = 0;
	if (method == 0)	 // copy
	{		
		VFS_Write(handle, in, len);
		return len;
	}
	else if (method == 1)// encryption
	{
		for (i = 0; i < len; i++) in[i] = in[i] ^ 0xA5;
		VFS_Write(handle, in, len);
		return len;
	}
	else if (method == 2)// compression (ZLIB)
	{
		char out[8192];
		z_stream strm = {in, len, 0, out, sizeof(out), 0, NULL, NULL, NULL, NULL, NULL, 0, 0, 0 };

		deflateInit( &strm, 9 ); // Z_BEST_COMPRESSION

		while(deflate( &strm, Z_FINISH) == Z_OK)
		{
			// compress in chunks of 8192. Saves having to allocate a huge-mega-big buffer
			VFS_Write( handle, out, sizeof(out) - strm.avail_out);
			i += sizeof(out) - strm.avail_out;
			strm.next_out = out;
			strm.avail_out = sizeof(out);
		}
		VFS_Write( handle, out, sizeof(out) - strm.avail_out );
		i += sizeof(out) - strm.avail_out;

		Msg("real length %d, compressed len %d\n", len, i );
		deflateEnd( &strm );
		return i;
	}
	else
	{
		Sys_Error("PR_encode: Bad encryption method\n");
		return 0;
	}
}

byte *PR_LoadFile(char *filename, fs_offset_t *filesizeptr, int type )
{
	char *mem;
	int len = FS_FileSize(filename);
	if (!len) Sys_Error("Couldn't open file %s", filename);

	mem = Qalloc(sizeof(cachedsourcefile_t) + len );	
	((cachedsourcefile_t*)mem)->next = sourcefile;
	sourcefile = (cachedsourcefile_t*)mem;
	sourcefile->size = len;	
	mem += sizeof(cachedsourcefile_t);	
	strcpy(sourcefile->filename, filename);
	sourcefile->file = mem;
	sourcefile->type = type;

	mem = FS_LoadFile( filename, filesizeptr );
	return mem;
}

void PR_WriteBlock(vfile_t *handle, fs_offset_t pos, const void *data, size_t blocksize, bool compress)
{
	int	i, len = 0;

	if (compress)
	{		
		VFS_Write (handle, &len, sizeof(int));	// save for later
		len = PR_encode(blocksize, 2, (char *)data, handle);
		i = VFS_Tell(handle);		// member start of block
		VFS_Seek(handle, pos, SEEK_SET);	// seek back
		len = LittleLong(len);		// block size after deflate
		VFS_Write (handle, &len, sizeof(int));	// write size.
		VFS_Seek(handle, i, SEEK_SET);
	}
	else VFS_Write (handle, data, blocksize);
}