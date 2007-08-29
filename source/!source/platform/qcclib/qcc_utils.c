//=======================================================================
//			Copyright XashXT Group 2007 ©
//			qcc_utils.c - qcc common tools
//=======================================================================

#include "qcclib.h"

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
char *PR_decode(int complen, int len, int method, char *info, char *buffer)
{
	int i;
	if (method == 0)	//copy
	{
		if (complen != len) Sys_Error("lengths do not match");
		memcpy(buffer, info, len);		
	}
	else if (method == 1)//encryption
	{
		if (complen != len) Sys_Error("lengths do not match");
		for (i = 0; i < len; i++) buffer[i] = info[i] ^ 0xA5;		
	}
	else Sys_Error("Bad file encryption routine\n");


	return buffer;
}

/*
================
PR_encode
================
*/
int PR_encode(int len, int method, char *in, vfile_t *handle)
{
	int i;
	if (method == 0) //copy
	{		
		VFS_Write(handle, in, len);
		return len;
	}
	else if (method == 1)//encryption
	{
		for (i = 0; i < len; i++) in[i] = in[i] ^ 0xA5;
		VFS_Write(handle, in, len);
		return len;
	}
	else
	{
		Sys_Error("Wierd method");
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