//=======================================================================
//			Copyright XashXT Group 2007 ©
//		memory.c - zone memory allocation from DarkPlaces
//=======================================================================

#include "launcher.h"

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
}
memheader_t;

typedef struct memclump_s
{
	byte		block[MEMCLUMPSIZE];// contents of the clump
	uint		sentinel1;	// should always be MEMCLUMP_SENTINEL
	int		bits[MEMBITINTS];	// if a bit is on, it means that the MEMUNIT bytes it represents are allocated, otherwise free
	uint		sentinel2;	// should always be MEMCLUMP_SENTINEL
	size_t		blocksinuse;	// if this drops to 0, the clump is freed
	size_t		largestavailable;	// largest block of memory available
	struct memclump_s	*chain;		// next clump in the chain
}
memclump_t;

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
}
mempool_t;

mempool_t *poolchain = NULL;

void _mem_copy(void *dest, void *src, size_t size, const char *filename, int fileline)
{
	if (src == NULL || size <= 0) return; // nothing to copy
	if (dest == NULL) Sys_Error("Mem_Copy: dest == NULL (called at %s:%i)", filename, fileline);

	// copy block
	memcpy( dest, src, size );
}

void _mem_set(void *dest, int set, size_t size, const char *filename, int fileline)
{
	if(size <= 0) return; // nothing to set
	if(dest == NULL) Sys_Error("Mem_Set: dest == NULL (called at %s:%i)", filename, fileline);

	// fill block
	memset( dest, set, size );
}

void *_mem_alloc(byte *poolptr, size_t size, const char *filename, int fileline)
{
	int i, j, k, needed, endbit, largest;
	memclump_t *clump, **clumpchainpointer;
	memheader_t *mem;
	mempool_t *pool = (mempool_t *)((byte *)poolptr);
	if (size <= 0) return NULL;

	if (poolptr == NULL) Sys_Error("Mem_Alloc: pool == NULL (alloc at %s:%i)", filename, fileline);
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
				Sys_Error("Mem_Alloc: trashed clump sentinel 1 (alloc at %s:%d)", filename, fileline);
			if (clump->sentinel2 != MEMCLUMP_SENTINEL)
				Sys_Error("Mem_Alloc: trashed clump sentinel 2 (alloc at %s:%d)", filename, fileline);
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
		if (clump == NULL) Sys_Error("Mem_Alloc: out of memory (alloc at %s:%i)", filename, fileline);
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
		if (mem == NULL) Sys_Error("Mem_Alloc: out of memory (alloc at %s:%i)", filename, fileline);
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
	return (void *)((byte *) mem + sizeof(memheader_t));
}

static void _mem_freeblock(memheader_t *mem, const char *filename, int fileline)
{
	int i, firstblock, endblock;
	memclump_t *clump, **clumpchainpointer;
	mempool_t *pool;

	if (mem->sentinel1 != MEMHEADER_SENTINEL1) Sys_Error("Mem_Free: trashed header sentinel 1 (alloc at %s:%i, free at %s:%i)", mem->filename, mem->fileline, filename, fileline);
	if (*((byte *) mem + sizeof(memheader_t) + mem->size) != MEMHEADER_SENTINEL2)
		Sys_Error("Mem_Free: trashed header sentinel 2 (alloc at %s:%i, free at %s:%i)", mem->filename, mem->fileline, filename, fileline);
	pool = mem->pool;
	// unlink memheader from doubly linked list
	if ((mem->prev ? mem->prev->next != mem : pool->chain != mem) || (mem->next && mem->next->prev != mem))
		Sys_Error("Mem_Free: not allocated or double freed (free at %s:%i)", filename, fileline);
	if (mem->prev) mem->prev->next = mem->next;
	else pool->chain = mem->next;
	if (mem->next) mem->next->prev = mem->prev;
	// memheader has been unlinked, do the actual free now
	pool->totalsize -= mem->size;

	if ((clump = mem->clump))
	{
		if (clump->sentinel1 != MEMCLUMP_SENTINEL)
			Sys_Error("Mem_Free: trashed clump sentinel 1 (free at %s:%i)", filename, fileline);
		if (clump->sentinel2 != MEMCLUMP_SENTINEL)
			Sys_Error("Mem_Free: trashed clump sentinel 2 (free at %s:%i)", filename, fileline);
		firstblock = ((byte *) mem - (byte *) clump->block);
		if (firstblock & (MEMUNIT - 1))
			Sys_Error("Mem_Free: address not valid in clump (free at %s:%i)", filename, fileline);
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
			free(clump);
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
		free(mem);
	}
}

void _mem_free(void *data, const char *filename, int fileline)
{
	if (data == NULL) Sys_Error("Mem_Free: data == NULL (called at %s:%i)", filename, fileline);
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

	if(!memptr) Sys_Error("Mem_Move: dest == NULL (called at %s:%i)", filename, fileline);
	if(!src) Sys_Error("Mem_Move: src == NULL (called at %s:%i)", filename, fileline);

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
	else _mem_set( memptr, 0, size, filename, fileline );		// no need to reallocate buffer
	
	_mem_copy( memptr, src, size, filename, fileline );		// move memory...
	_mem_free( src, filename, fileline );				// ...and free old pointer

	*dest = memptr;
}

byte *_mem_allocpool(const char *name, const char *filename, int fileline)
{
	mempool_t *pool;
	pool = (mempool_t *)malloc(sizeof(mempool_t));
	if (pool == NULL) Sys_Error("Mem_AllocPool: out of memory (allocpool at %s:%i)", filename, fileline);
	_mem_set(pool, 0, sizeof(mempool_t), filename, fileline );

	// fill header
	pool->sentinel1 = MEMHEADER_SENTINEL1;
	pool->sentinel2 = MEMHEADER_SENTINEL1;
	pool->filename = filename;
	pool->fileline = fileline;
	pool->chain = NULL;
	pool->totalsize = 0;
	pool->realsize = sizeof(mempool_t);
	strlcpy (pool->name, name, sizeof (pool->name));
	pool->next = poolchain;
	poolchain = pool;
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
		if (*chainaddress != pool) Sys_Error("Mem_FreePool: pool already free (freepool at %s:%i)", filename, fileline);
		if (pool->sentinel1 != MEMHEADER_SENTINEL1) Sys_Error("Mem_FreePool: trashed pool sentinel 1 (allocpool at %s:%i, freepool at %s:%i)", pool->filename, pool->fileline, filename, fileline);
		if (pool->sentinel2 != MEMHEADER_SENTINEL1) Sys_Error("Mem_FreePool: trashed pool sentinel 2 (allocpool at %s:%i, freepool at %s:%i)", pool->filename, pool->fileline, filename, fileline);
		*chainaddress = pool->next;

		// free memory owned by the pool
		while (pool->chain) _mem_freeblock(pool->chain, filename, fileline);
		// free the pool itself
		_mem_set(pool, 0xBF, sizeof(mempool_t), filename, fileline);
		free(pool);
		*poolptr = NULL;
	}
}

void _mem_emptypool(byte *poolptr, const char *filename, int fileline)
{
	mempool_t *pool = (mempool_t *)((byte *)poolptr);
	if (poolptr == NULL) Sys_Error("Mem_EmptyPool: pool == NULL (emptypool at %s:%i)", filename, fileline);

	if (pool->sentinel1 != MEMHEADER_SENTINEL1) Sys_Error("Mem_EmptyPool: trashed pool sentinel 1 (allocpool at %s:%i, emptypool at %s:%i)", pool->filename, pool->fileline, filename, fileline);
	if (pool->sentinel2 != MEMHEADER_SENTINEL1) Sys_Error("Mem_EmptyPool: trashed pool sentinel 2 (allocpool at %s:%i, emptypool at %s:%i)", pool->filename, pool->fileline, filename, fileline);

	// free memory owned by the pool
	while (pool->chain) _mem_freeblock(pool->chain, filename, fileline);
}

void _mem_checkheadersentinels(void *data, const char *filename, int fileline)
{
	memheader_t *mem;

	if (data == NULL) Sys_Error("Mem_CheckSentinels: data == NULL (sentinel check at %s:%i)", filename, fileline);
	mem = (memheader_t *)((byte *) data - sizeof(memheader_t));
	if (mem->sentinel1 != MEMHEADER_SENTINEL1)
		Sys_Error("Mem_CheckSentinels: trashed header sentinel 1 (block allocated at %s:%i, sentinel check at %s:%i)", mem->filename, mem->fileline, filename, fileline);
	if (*((byte *) mem + sizeof(memheader_t) + mem->size) != MEMHEADER_SENTINEL2)
		Sys_Error("Mem_CheckSentinels: trashed header sentinel 2 (block allocated at %s:%i, sentinel check at %s:%i)", mem->filename, mem->fileline, filename, fileline);
}

static void _mem_checkclumpsentinels(memclump_t *clump, const char *filename, int fileline)
{
	// this isn't really very useful
	if (clump->sentinel1 != MEMCLUMP_SENTINEL)
		Sys_Error("Mem_CheckClumpSentinels: trashed sentinel 1 (sentinel check at %s:%i)", filename, fileline);
	if (clump->sentinel2 != MEMCLUMP_SENTINEL)
		Sys_Error("Mem_CheckClumpSentinels: trashed sentinel 2 (sentinel check at %s:%i)", filename, fileline);
}

void _mem_check(const char *filename, int fileline)
{
	memheader_t *mem;
	mempool_t *pool;
	memclump_t *clump;

	for (pool = poolchain;pool;pool = pool->next)
	{
		if (pool->sentinel1 != MEMHEADER_SENTINEL1)
			Sys_Error("Mem_CheckSentinelsGlobal: trashed pool sentinel 1 (allocpool at %s:%i, sentinel check at %s:%i)", pool->filename, pool->fileline, filename, fileline);
		if (pool->sentinel2 != MEMHEADER_SENTINEL1)
			Sys_Error("Mem_CheckSentinelsGlobal: trashed pool sentinel 2 (allocpool at %s:%i, sentinel check at %s:%i)", pool->filename, pool->fileline, filename, fileline);
	}
	for (pool = poolchain;pool;pool = pool->next)
		for (mem = pool->chain;mem;mem = mem->next)
			_mem_checkheadersentinels((void *)((byte *) mem + sizeof(memheader_t)), filename, fileline);

	for (pool = poolchain;pool;pool = pool->next)
		for (clump = pool->clumpchain;clump;clump = clump->chain)
			_mem_checkclumpsentinels(clump, filename, fileline);
}

/*
========================
Memory_Init
========================
*/
void InitMemory (void)
{
	poolchain = NULL;//init mem chain
}