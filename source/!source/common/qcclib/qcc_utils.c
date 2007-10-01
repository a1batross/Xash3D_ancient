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

int Hash_KeyInsensative(char *name, int modulus)
{
	//fixme: optimize.
	uint key;
	for (key = 0; *name; name++)
	{
		if (*name >= 'A' && *name <= 'Z')
			key += ((key<<3) + (key>>28) + (*name-'A'+'a'));
		else key += ((key<<3) + (key>>28) + *name);
	}
		
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

void *Hash_GetInsensative(hashtable_t *table, char *name)
{
	int bucknum = Hash_KeyInsensative(name, table->numbuckets);
	bucket_t *buck;

	buck = table->bucket[bucknum];
	while(buck)
	{
		if (!stricmp(name, buck->keystring))
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

void *Hash_GetNextInsensative(hashtable_t *table, char *name, void *old)
{
	int bucknum = Hash_KeyInsensative(name, table->numbuckets);
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
	if (!buck)return NULL;

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

void *Hash_AddInsensative(hashtable_t *table, char *name, void *data, bucket_t *buck)
{
	int bucknum = Hash_KeyInsensative(name, table->numbuckets);

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
		z_stream strm = {info, complen, 0, buffer, len, 0, NULL, NULL, NULL, NULL, NULL, Z_BINARY, 0, 0 };
		inflateInit( &strm );

		// decompress it in one go.
		if (Z_STREAM_END != inflate( &strm, Z_FINISH ))
		{
			Msg("Failed block decompression: %s\n", strm.msg );
			return false;
		}
		inflateEnd( &strm );
		Msg("inflanting %s\n", strm.msg );
	}
	else 
	{
		MsgDev(D_WARN, "PR_decode: Bad file encryption routine\n");
		return false;
          }
	return true;
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
		z_stream strm = {in, len, 0, out, sizeof(out), 0, NULL, NULL, NULL, NULL, NULL, Z_BINARY, 0, 0 };

		deflateInit( &strm, Z_BEST_COMPRESSION);

		while(deflate( &strm, Z_FINISH) == Z_OK)
		{
			// compress in chunks of 8192. Saves having to allocate a huge-mega-big buffer
			VFS_Write( handle, out, sizeof(out) - strm.avail_out);
			i += sizeof(out) - strm.avail_out;

			Msg("Zlib statuc %s\n", strm.msg );
			strm.next_out = out;
			strm.avail_out = sizeof(out);
		}
		VFS_Write( handle, out, sizeof(out) - strm.avail_out );
		i += sizeof(out) - strm.avail_out;

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