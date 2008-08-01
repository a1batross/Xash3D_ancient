//=======================================================================
//			Copyright XashXT Group 2007 ©
//		memory.c - zone memory allocation from DarkPlaces
//=======================================================================

#include "launch.h"

#define MEMCLUMPSIZE	(65536 - 1536)	// give malloc padding so we can't waste most of a page at the end
#define MEMUNIT		8		// smallest unit we care about is this many bytes
#define MEMBITS		(MEMCLUMPSIZE / MEMUNIT)
#define MEMBITINTS		(MEMBITS / 32)

#define MEMCLUMP_SENTINEL	0xABADCAFE
#define MEMHEADER_SENTINEL1	0xDEADF00D
#define MEMHEADER_SENTINEL2	0xDF

typedef struct memheader_s
{
	struct memheader_s	*next;		// next and previous memheaders in chain belonging to pool
	struct memheader_s	*prev;
	struct mempool_s	*pool;		// pool this memheader belongs to
	struct memclump_s	*clump;		// clump this memheader lives in, NULL if not in a clump
	size_t		size;		// size of the memory after the header (excluding header and sentinel2)
	const char	*filename;	// file name and line where Mem_Alloc was called
	uint		fileline;
	uint		sentinel1;	// should always be MEMHEADER_SENTINEL1

	// immediately followed by data, which is followed by a MEMHEADER_SENTINEL2 byte
} memheader_t;

typedef struct memclump_s
{
	byte		block[MEMCLUMPSIZE];// contents of the clump
	uint		sentinel1;	// should always be MEMCLUMP_SENTINEL
	int		bits[MEMBITINTS];	// if a bit is on, it means that the MEMUNIT bytes it represents are allocated, otherwise free
	uint		sentinel2;	// should always be MEMCLUMP_SENTINEL
	size_t		blocksinuse;	// if this drops to 0, the clump is freed
	size_t		largestavailable;	// largest block of memory available
	struct memclump_s	*chain;		// next clump in the chain
} memclump_t;

typedef struct mempool_s
{
	uint		sentinel1;	// should always be MEMHEADER_SENTINEL1
	struct memheader_s	*chain;		// chain of individual memory allocations
	struct memclump_s	*clumpchain;	// chain of clumps (if any)
	size_t		totalsize;	// total memory allocated in this pool (inside memheaders)
	size_t		realsize;		// total memory allocated in this pool (actual malloc total)
	size_t		lastchecksize;	// updated each time the pool is displayed by memlist
	struct mempool_s	*next;		// linked into global mempool list
	const char	*filename;	// file name and line where Mem_AllocPool was called
	int		fileline;
	char		name[MAX_QPATH];	// name of the pool
	uint		sentinel2;	// should always be MEMHEADER_SENTINEL1
} mempool_t;

typedef struct memcluster_s
{
	byte	*data;
	byte	*allocflags;
	size_t	numflaggedrecords;
} memcluster_t;

typedef struct memarray_s
{
	byte		*mempool;
	size_t		recordsize;
	size_t		count;
	size_t		numarrays;
	size_t		maxarrays;
	memcluster_t	*arrays;

} memarray_t;

mempool_t *poolchain = NULL; // critical stuff

void _mem_copy(void *dest, const void *src, size_t size, const char *filename, int fileline)
{
	if (src == NULL || size <= 0) return; // nothing to copy
	if (dest == NULL) Sys_Error("Mem_Copy: dest == NULL (called at %s:%i)\n", filename, fileline);

	// copy block
	memcpy( dest, src, size );
}

void *_mem_alloc(byte *poolptr, size_t size, const char *filename, int fileline)
{
	int i, j, k, needed, endbit, largest;
	memclump_t *clump, **clumpchainpointer;
	memheader_t *mem;
	mempool_t *pool = (mempool_t *)((byte *)poolptr);
	if (size <= 0) return NULL;

	if (poolptr == NULL) Sys_Error("Mem_Alloc: pool == NULL (alloc at %s:%i)\n", filename, fileline);
	pool->totalsize += size;

	if (size < 4096)
	{
		// clumping
		needed = (sizeof(memheader_t) + size + sizeof(int) + (MEMUNIT - 1)) / MEMUNIT;
		endbit = MEMBITS - needed;
		for (clumpchainpointer = &pool->clumpchain;*clumpchainpointer;clumpchainpointer = &(*clumpchainpointer)->chain)
		{
			clump = *clumpchainpointer;
			if (clump->sentinel1 != MEMCLUMP_SENTINEL)
				Sys_Error("Mem_Alloc: trashed clump sentinel 1 (alloc at %s:%d)\n", filename, fileline);
			if (clump->sentinel2 != MEMCLUMP_SENTINEL)
				Sys_Error("Mem_Alloc: trashed clump sentinel 2 (alloc at %s:%d)\n", filename, fileline);
			if (clump->largestavailable >= needed)
			{
				largest = 0;
				for (i = 0;i < endbit;i++)
				{
					if (clump->bits[i >> 5] & (1 << (i & 31)))
						continue;
					k = i + needed;
					for (j = i;i < k;i++)
						if (clump->bits[i >> 5] & (1 << (i & 31)))
							goto loopcontinue;
					goto choseclump;
loopcontinue:;
					if (largest < j - i)
						largest = j - i;
				}
				// since clump falsely advertised enough space (nothing wrong
				// with that), update largest count to avoid wasting time in
				// later allocations
				clump->largestavailable = largest;
			}
		}
		pool->realsize += sizeof(memclump_t);
		clump = malloc(sizeof(memclump_t));
		if (clump == NULL) Sys_Error("Mem_Alloc: out of memory (alloc at %s:%i)\n", filename, fileline);
		memset(clump, 0, sizeof(memclump_t));
		*clumpchainpointer = clump;
		clump->sentinel1 = MEMCLUMP_SENTINEL;
		clump->sentinel2 = MEMCLUMP_SENTINEL;
		clump->chain = NULL;
		clump->blocksinuse = 0;
		clump->largestavailable = MEMBITS - needed;
		j = 0;
choseclump:
		mem = (memheader_t *)((byte *) clump->block + j * MEMUNIT);
		mem->clump = clump;
		clump->blocksinuse += needed;
		for (i = j + needed;j < i;j++) clump->bits[j >> 5] |= (1 << (j & 31));
	}
	else
	{
		// big allocations are not clumped
		pool->realsize += sizeof(memheader_t) + size + sizeof(int);
		mem = (memheader_t *)malloc(sizeof(memheader_t) + size + sizeof(int));
		if (mem == NULL) Sys_Error("Mem_Alloc: out of memory (alloc at %s:%i)\n", filename, fileline);
		mem->clump = NULL;
	}

	mem->filename = filename;
	mem->fileline = fileline;
	mem->size = size;
	mem->pool = pool;
	mem->sentinel1 = MEMHEADER_SENTINEL1;
	// we have to use only a single byte for this sentinel, because it may not be aligned
	// and some platforms can't use unaligned accesses
	*((byte *) mem + sizeof(memheader_t) + mem->size) = MEMHEADER_SENTINEL2;
	// append to head of list
	mem->next = pool->chain;
	mem->prev = NULL;
	pool->chain = mem;
	if (mem->next) mem->next->prev = mem;
	memset((void *)((byte *) mem + sizeof(memheader_t)), 0, mem->size);

	MsgDev(D_MEMORY, "Malloc: %s, by \"%s\" at (%s:%i)\n", Mem_Pretify( size ), pool->name, filename, fileline );
	return (void *)((byte *) mem + sizeof(memheader_t));
}

static void _mem_freeblock(memheader_t *mem, const char *filename, int fileline)
{
	int i, firstblock, endblock;
	memclump_t *clump, **clumpchainpointer;
	mempool_t *pool;

	if (mem->sentinel1 != MEMHEADER_SENTINEL1) Sys_Error("Mem_Free: trashed header sentinel 1 (alloc at %s:%i, free at %s:%i)\n", mem->filename, mem->fileline, filename, fileline);
	if (*((byte *) mem + sizeof(memheader_t) + mem->size) != MEMHEADER_SENTINEL2)
		Sys_Error("Mem_Free: trashed header sentinel 2 (alloc at %s:%i, free at %s:%i)\n", mem->filename, mem->fileline, filename, fileline);
	pool = mem->pool;
	// unlink memheader from doubly linked list
	if ((mem->prev ? mem->prev->next != mem : pool->chain != mem) || (mem->next && mem->next->prev != mem))
		Sys_Error("Mem_Free: not allocated or double freed (free at %s:%i)\n", filename, fileline);
	if (mem->prev) mem->prev->next = mem->next;
	else pool->chain = mem->next;
	if (mem->next) mem->next->prev = mem->prev;
	// memheader has been unlinked, do the actual free now
	pool->totalsize -= mem->size;

	if((clump = mem->clump))
	{
		if (clump->sentinel1 != MEMCLUMP_SENTINEL)
			Sys_Error("Mem_Free: trashed clump sentinel 1 (free at %s:%i)\n", filename, fileline);
		if (clump->sentinel2 != MEMCLUMP_SENTINEL)
			Sys_Error("Mem_Free: trashed clump sentinel 2 (free at %s:%i)\n", filename, fileline);
		firstblock = ((byte *) mem - (byte *) clump->block);
		if (firstblock & (MEMUNIT - 1))
			Sys_Error("Mem_Free: address not valid in clump (free at %s:%i)\n", filename, fileline);
		firstblock /= MEMUNIT;
		endblock = firstblock + ((sizeof(memheader_t) + mem->size + sizeof(int) + (MEMUNIT - 1)) / MEMUNIT);
		clump->blocksinuse -= endblock - firstblock;
		// could use &, but we know the bit is set
		for (i = firstblock;i < endblock;i++)
			clump->bits[i >> 5] -= (1 << (i & 31));
		if (clump->blocksinuse <= 0)
		{
			// unlink from chain
			for (clumpchainpointer = &pool->clumpchain;*clumpchainpointer;clumpchainpointer = &(*clumpchainpointer)->chain)
			{
				if (*clumpchainpointer == clump)
				{
					*clumpchainpointer = clump->chain;
					break;
				}
			}
			pool->realsize -= sizeof(memclump_t);
			memset(clump, 0xBF, sizeof(memclump_t));
			free( clump );
		}
		else
		{
			// clump still has some allocations
			// force re-check of largest available space on next alloc
			clump->largestavailable = MEMBITS - clump->blocksinuse;
		}
	}
	else
	{
		pool->realsize -= sizeof(memheader_t) + mem->size + sizeof(int);
		MsgDev(D_MEMORY, "Free: %s, by \"%s\" at (%s:%i)\n", Mem_Pretify( mem->size ), pool->name, filename, fileline );
		free( mem );
	}
}

void _mem_free(void *data, const char *filename, int fileline)
{
	if (data == NULL) Sys_Error("Mem_Free: data == NULL (called at %s:%i)\n", filename, fileline);
	_mem_freeblock((memheader_t *)((byte *) data - sizeof(memheader_t)), filename, fileline);
}

void *_mem_realloc(byte *poolptr, void *memptr, size_t size, const char *filename, int fileline)
{
	char		*nb;
	memheader_t	*memhdr;

	if (size <= 0) return memptr; // no need to reallocate
	nb = _mem_alloc(poolptr, size, filename, fileline);

	if( memptr ) // first allocate?
	{ 
		// get size of old block
		memhdr = (memheader_t *)((byte *)memptr - sizeof(memheader_t));
		_mem_copy( nb, memptr, memhdr->size, filename, fileline );
		_mem_free( memptr, filename, fileline); // free unused old block
          }

	return (void *)nb;
}

void _mem_move(byte *poolptr, void **dest, void *src, size_t size, const char *filename, int fileline)
{
	memheader_t	*mem;
	void		*memptr = *dest;

	if(!memptr) Sys_Error("Mem_Move: dest == NULL (called at %s:%i)\n", filename, fileline);
	if(!src) Sys_Error("Mem_Move: src == NULL (called at %s:%i)\n", filename, fileline);

	if (size <= 0)
	{
		// just free memory 
		_mem_free( memptr, filename, fileline );
		*dest = src; // swap blocks		
		return;
	}

	mem = (memheader_t *)((byte *) memptr - sizeof(memheader_t));	// get size of old block
	if(mem->size != size) 
	{
		_mem_free( memptr, filename, fileline );		// release old buffer
		memptr = _mem_alloc( poolptr, size, filename, fileline );	// alloc new size
	}
	else memset( memptr, 0, size );				// no need to reallocate buffer
	
	_mem_copy( memptr, src, size, filename, fileline );		// move memory...
	_mem_free( src, filename, fileline );				// ...and free old pointer

	*dest = memptr;
}

byte *_mem_allocpool(const char *name, const char *filename, int fileline)
{
	mempool_t *pool;
	pool = (mempool_t *)malloc(sizeof(mempool_t));
	if (pool == NULL) Sys_Error("Mem_AllocPool: out of memory (allocpool at %s:%i)\n", filename, fileline);
	memset(pool, 0, sizeof(mempool_t));

	// fill header
	pool->sentinel1 = MEMHEADER_SENTINEL1;
	pool->sentinel2 = MEMHEADER_SENTINEL1;
	pool->filename = filename;
	pool->fileline = fileline;
	pool->chain = NULL;
	pool->totalsize = 0;
	pool->realsize = sizeof(mempool_t);
	com_strncpy(pool->name, name, sizeof (pool->name));
	pool->next = poolchain;
	poolchain = pool;

	MsgDev(D_MEMORY, "Create pool: \"%s\", at (%s:%i)\n", pool->name, filename, fileline );
	return (byte *)((mempool_t *)pool);
}

void _mem_freepool(byte **poolptr, const char *filename, int fileline)
{
	mempool_t	*pool = (mempool_t *)((byte *) *poolptr);
	mempool_t	**chainaddress;
          
	if (pool)
	{
		// unlink pool from chain
		for (chainaddress = &poolchain;*chainaddress && *chainaddress != pool;chainaddress = &((*chainaddress)->next));
		if (*chainaddress != pool) Sys_Error("Mem_FreePool: pool already free (freepool at %s:%i)\n", filename, fileline);
		if (pool->sentinel1 != MEMHEADER_SENTINEL1) Sys_Error("Mem_FreePool: trashed pool sentinel 1 (allocpool at %s:%i, freepool at %s:%i)\n", pool->filename, pool->fileline, filename, fileline);
		if (pool->sentinel2 != MEMHEADER_SENTINEL1) Sys_Error("Mem_FreePool: trashed pool sentinel 2 (allocpool at %s:%i, freepool at %s:%i)\n", pool->filename, pool->fileline, filename, fileline);
		*chainaddress = pool->next;

		MsgDev(D_MEMORY, "Free pool: \"%s\", at (%s:%i)\n", pool->name, filename, fileline );

		// free memory owned by the pool
		while (pool->chain) _mem_freeblock(pool->chain, filename, fileline);
		// free the pool itself
		memset(pool, 0xBF, sizeof(mempool_t));
		free(pool);
		*poolptr = NULL;
	}
}

void _mem_emptypool(byte *poolptr, const char *filename, int fileline)
{
	mempool_t *pool = (mempool_t *)((byte *)poolptr);
	if (poolptr == NULL) Sys_Error("Mem_EmptyPool: pool == NULL (emptypool at %s:%i)\n", filename, fileline);

	if (pool->sentinel1 != MEMHEADER_SENTINEL1) Sys_Error("Mem_EmptyPool: trashed pool sentinel 1 (allocpool at %s:%i, emptypool at %s:%i)\n", pool->filename, pool->fileline, filename, fileline);
	if (pool->sentinel2 != MEMHEADER_SENTINEL1) Sys_Error("Mem_EmptyPool: trashed pool sentinel 2 (allocpool at %s:%i, emptypool at %s:%i)\n", pool->filename, pool->fileline, filename, fileline);

	// free memory owned by the pool
	while (pool->chain) _mem_freeblock(pool->chain, filename, fileline);
}

bool _mem_allocated( mempool_t *pool, void *data )
{
	memheader_t *header, *target;

	if( pool )
	{
		// search only one pool
		target = (memheader_t *)((byte *)data - sizeof(memheader_t));
		for( header = pool->chain; header; header = header->next )
			if( header == target ) return true;
	}
	else
	{
		// search all pools
		for (pool = poolchain; pool; pool = pool->next)
			if(_mem_allocated( pool, data ))
				return true;
	}
	return false;
}

/*
========================
Check pointer for memory
allocated by memlib.c
========================
*/
bool _is_allocated( byte *poolptr, void *data )
{
	mempool_t	*pool = NULL;
	if( poolptr ) pool = (mempool_t *)((byte *)poolptr);

	return _mem_allocated( pool, data );
}

byte *_mem_alloc_array( byte *poolptr, size_t recordsize, int count, const char *filename, int fileline )
{
	memarray_t *l = _mem_alloc( poolptr, sizeof(memarray_t), filename, fileline);

	l->mempool = poolptr;
	l->recordsize = recordsize;
	l->count = count;

	return (byte *)((memarray_t *)l);
}

void _mem_free_array( byte *arrayptr, const char *filename, int fileline )
{
	memarray_t *l = (memarray_t *)((byte *)arrayptr);
	size_t i;

	if(l == NULL) Sys_Error("Mem_RemoveArray: array == NULL (called at %s:%i)\n", filename, fileline );

	if (l->maxarrays)
	{
		for (i = 0; i != l->numarrays; i++)
			_mem_free(l->arrays[i].data, filename, fileline );
		_mem_free(l->arrays, filename, fileline );
	}
	_mem_free( l, filename, fileline); // himself
}

void *_mem_alloc_array_element( byte *arrayptr, const char *filename, int fileline )
{
	memarray_t	*l = (memarray_t *)((byte *)arrayptr);
	size_t		i, j;

	if(l == NULL) Sys_Error("Mem_AllocElement: array == NULL (called at %s:%i)\n", filename, fileline );

	for(i = 0; ; i++)
	{
		if(i == l->numarrays)
		{
			if (l->numarrays == l->maxarrays)
			{
				memcluster_t *oldarrays = l->arrays;
				l->maxarrays = max(l->maxarrays * 2, 128);
				l->arrays = _mem_alloc( l->mempool, l->maxarrays * sizeof(*l->arrays), filename, fileline);
				if (oldarrays)
				{
					_mem_copy(l->arrays, oldarrays, l->numarrays * sizeof(*l->arrays), filename, fileline);
					_mem_free(oldarrays, filename, fileline);
				}
			}
			l->arrays[i].numflaggedrecords = 0;
			l->arrays[i].data = _mem_alloc(l->mempool, (l->recordsize + 1) * l->count, filename, fileline);
			l->arrays[i].allocflags = l->arrays[i].data + l->recordsize * l->count;
			l->numarrays++;
		}
		if(l->arrays[i].numflaggedrecords < l->count)
		{
			for (j = 0; j < l->count; j++)
			{
				if(!l->arrays[i].allocflags[j])
				{
					l->arrays[i].allocflags[j] = true;
					l->arrays[i].numflaggedrecords++;
					return (void *)(l->arrays[i].data + l->recordsize * j);
				}
			}
		}
	}
}

void _mem_free_array_element( byte *arrayptr, void *element, const char *filename, int fileline )
{
	size_t	i, j;
	byte	*p = (byte *)element;
	memarray_t *l = (memarray_t *)((byte *)arrayptr);

	if(l == NULL) Sys_Error("Mem_FreeElement: array == NULL (called at %s:%i)\n", filename, fileline );

	for (i = 0; i != l->numarrays; i++)
	{
		if(p >= l->arrays[i].data && p < (l->arrays[i].data + l->recordsize * l->count))
		{
			j = (p - l->arrays[i].data) / l->recordsize;
			if (p != l->arrays[i].data + j * l->recordsize)
				Sys_Error("Mem_FreeArrayElement: no such element %p\n", p);
			if (!l->arrays[i].allocflags[j])
				Sys_Error("Mem_FreeArrayElement: element %p is already free!\n", p);
			l->arrays[i].allocflags[j] = false;
			l->arrays[i].numflaggedrecords--;
			return;
		}
	}
}

size_t _mem_array_size( byte *arrayptr )
{
	size_t i, j, k;
	memarray_t *l = (memarray_t *)((byte *)arrayptr);
	
	if(l && l->numarrays)
	{
		i = l->numarrays - 1;

		for (j = 0, k = 0; k < l->arrays[i].numflaggedrecords; j++)
			if (l->arrays[i].allocflags[j]) k++;

		return l->count * i + j;
	}
	return 0;
}

void *_mem_get_array_element( byte *arrayptr, size_t index )
{
	size_t	i, j;
	memarray_t *l = (memarray_t *)((byte *)arrayptr);

	if(!l) return NULL;

	i = index / l->count;
	j = index % l->count;
	if (i >= l->numarrays || !l->arrays[i].allocflags[j])
		return NULL;

	return (void *)(l->arrays[i].data + j * l->recordsize);
}

void _mem_checkheadersentinels( void *data, const char *filename, int fileline )
{
	memheader_t *mem;

	if (data == NULL) Sys_Error("Mem_CheckSentinels: data == NULL (sentinel check at %s:%i)\n", filename, fileline);
	mem = (memheader_t *)((byte *) data - sizeof(memheader_t));
	if (mem->sentinel1 != MEMHEADER_SENTINEL1)
		Sys_Error("Mem_CheckSentinels: trashed header sentinel 1 (block allocated at %s:%i, sentinel check at %s:%i)\n", mem->filename, mem->fileline, filename, fileline);
	if (*((byte *) mem + sizeof(memheader_t) + mem->size) != MEMHEADER_SENTINEL2)
		Sys_Error("Mem_CheckSentinels: trashed header sentinel 2 (block allocated at %s:%i, sentinel check at %s:%i)\n", mem->filename, mem->fileline, filename, fileline);
}

static void _mem_checkclumpsentinels( memclump_t *clump, const char *filename, int fileline )
{
	// this isn't really very useful
	if (clump->sentinel1 != MEMCLUMP_SENTINEL)
		Sys_Error("Mem_CheckClumpSentinels: trashed sentinel 1 (sentinel check at %s:%i)\n", filename, fileline);
	if (clump->sentinel2 != MEMCLUMP_SENTINEL)
		Sys_Error("Mem_CheckClumpSentinels: trashed sentinel 2 (sentinel check at %s:%i)\n", filename, fileline);
}

void _mem_check(const char *filename, int fileline)
{
	memheader_t *mem;
	mempool_t *pool;
	memclump_t *clump;

	for (pool = poolchain; pool; pool = pool->next)
	{
		if (pool->sentinel1 != MEMHEADER_SENTINEL1)
			Sys_Error("Mem_CheckSentinelsGlobal: trashed pool sentinel 1 (allocpool at %s:%i, sentinel check at %s:%i)\n", pool->filename, pool->fileline, filename, fileline);
		if (pool->sentinel2 != MEMHEADER_SENTINEL1)
			Sys_Error("Mem_CheckSentinelsGlobal: trashed pool sentinel 2 (allocpool at %s:%i, sentinel check at %s:%i)\n", pool->filename, pool->fileline, filename, fileline);
	}
	for (pool = poolchain; pool; pool = pool->next)
		for (mem = pool->chain; mem; mem = mem->next)
			_mem_checkheadersentinels((void *)((byte *) mem + sizeof(memheader_t)), filename, fileline);

	for (pool = poolchain; pool; pool = pool->next)
		for (clump = pool->clumpchain; clump; clump = clump->chain)
			_mem_checkclumpsentinels(clump, filename, fileline);
}

void _mem_printstats( void )
{
	size_t count = 0, size = 0, realsize = 0;
	mempool_t *pool;

	Mem_Check();
	for (pool = poolchain; pool; pool = pool->next)
	{
		count++;
		size += pool->totalsize;
		realsize += pool->realsize;
	}
	Msg("^3%lu^7 memory pools, totalling: ^1%s\n", (dword)count, Mem_Pretify( size ));
	Msg("Total allocated size: ^1%s\n", Mem_Pretify( realsize ));
}

void _mem_printlist( size_t minallocationsize )
{
	mempool_t		*pool;
	memheader_t	*mem;

	Mem_Check();

	Msg("memory pool list:\n""  ^3size                    name\n");
	for (pool = poolchain; pool; pool = pool->next)
	{
		Msg("%5luk (%5luk actual) %s (%+3li byte change)\n", (dword) ((pool->totalsize + 1023) / 1024), (dword)((pool->realsize + 1023) / 1024), pool->name, (long)pool->totalsize - pool->lastchecksize );
		pool->lastchecksize = pool->totalsize;
		for( mem = pool->chain; mem; mem = mem->next )
			if( mem->size >= minallocationsize )
				Msg("%10lu bytes allocated at %s:%i\n", (dword)mem->size, mem->filename, mem->fileline );
	}
}

void MemList_f( void )
{
	switch(Cmd_Argc())
	{
	case 1:
		_mem_printlist(1<<30);
		_mem_printstats();
		break;
	case 2:
		_mem_printlist(com.atoi(Cmd_Argv(1)) * 1024);
		_mem_printstats();
		break;
	default:
		Msg("memlist: usage: memlist <all>\n");
		break;
	}
}

/*
========================
Memory_Init
========================
*/
void Memory_Init( void )
{
	poolchain = NULL; // init mem chain
	Sys.basepool = Mem_AllocPool( "Main pool" );
	Sys.stringpool = Mem_AllocPool( "New string" );
}

void Memory_Shutdown( void )
{
	Mem_FreePool( &Sys.basepool );
	Mem_FreePool( &Sys.stringpool );
}

void Memory_Init_Commands( void )
{
	Cmd_AddCommand( "memlist", MemList_f, "prints memory pool information (or if used as memlist 5 lists individual allocations of 5K or larger, 0 lists all allocations)");
}