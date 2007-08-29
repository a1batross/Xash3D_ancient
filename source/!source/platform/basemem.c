//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    memory.c - zone memory allocation from DP
//=======================================================================

#include "platform.h"
#include "baseutils.h"

mempool_t *poolchain = NULL;
byte *basepool;
byte *zonepool;

void *_Mem_Alloc(byte *poolptr, size_t size, const char *filename, int fileline)
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

void *_Mem_Realloc(byte *poolptr, void *memptr, size_t size, const char *filename, int fileline)
{
	char *nb;
	memheader_t *hdr;

	if (size <= 0) return memptr;//no need to reallocate
	nb = _Mem_Alloc(poolptr, size, filename, fileline);

	if (memptr) //first allocate?
	{ 
		//get size of old block
		hdr = (memheader_t *)((byte *) memptr - sizeof(memheader_t));
		_Mem_Copy( nb, memptr, hdr->size, filename, fileline );
		_Mem_Free( memptr, filename, fileline);//free unused old block
          }
	else MsgWarn("Mem_Realloc: memptr == NULL (called at %s:%i)\n", filename, fileline);

	return (void *)nb;
}

static void _Mem_FreeBlock(memheader_t *mem, const char *filename, int fileline)
{
	int i, firstblock, endblock;
	memclump_t *clump, **clumpchainpointer;
	mempool_t *pool;

	if (mem->sentinel1 != MEMHEADER_SENTINEL1) Sys_Error("Mem_Free: trashed header sentinel 1 (alloc at %s:%i, free at %s:%i)", mem->filename, mem->fileline, filename, fileline);
	if (*((unsigned char *) mem + sizeof(memheader_t) + mem->size) != MEMHEADER_SENTINEL2)
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
		firstblock = ((unsigned char *) mem - (unsigned char *) clump->block);
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

void _Mem_Free(void *data, const char *filename, int fileline)
{
	if (data == NULL) Sys_Error("Mem_Free: data == NULL (called at %s:%i)", filename, fileline);
	_Mem_FreeBlock((memheader_t *)((unsigned char *) data - sizeof(memheader_t)), filename, fileline);
}

byte *_Mem_AllocPool(const char *name, const char *filename, int fileline)
{
	mempool_t *pool;
	pool = (mempool_t *)malloc(sizeof(mempool_t));
	if (pool == NULL) Sys_Error("Mem_AllocPool: out of memory (allocpool at %s:%i)", filename, fileline);
	memset(pool, 0, sizeof(mempool_t));
	pool->sentinel1 = MEMHEADER_SENTINEL1;
	pool->sentinel2 = MEMHEADER_SENTINEL1;
	pool->filename = filename;
	pool->fileline = fileline;
	pool->chain = NULL;
	pool->totalsize = 0;
	pool->realsize = sizeof(mempool_t);
	strncpy (pool->name, name, sizeof (pool->name));
	pool->next = poolchain;
	poolchain = pool;
	return (byte *)((mempool_t *)pool);
}

void _Mem_FreePool(byte **poolptr, const char *filename, int fileline)
{
	mempool_t *pool = (mempool_t *)((byte *) *poolptr);
	mempool_t **chainaddress;
          
	if (pool)
	{
		// unlink pool from chain
		for (chainaddress = &poolchain;*chainaddress && *chainaddress != pool;chainaddress = &((*chainaddress)->next));
		if (*chainaddress != pool) Sys_Error("Mem_FreePool: pool already free (freepool at %s:%i)", filename, fileline);
		if (pool->sentinel1 != MEMHEADER_SENTINEL1) Sys_Error("Mem_FreePool: trashed pool sentinel 1 (allocpool at %s:%i, freepool at %s:%i)", pool->filename, pool->fileline, filename, fileline);
		if (pool->sentinel2 != MEMHEADER_SENTINEL1) Sys_Error("Mem_FreePool: trashed pool sentinel 2 (allocpool at %s:%i, freepool at %s:%i)", pool->filename, pool->fileline, filename, fileline);
		*chainaddress = pool->next;

		// free memory owned by the pool
		while (pool->chain) _Mem_FreeBlock(pool->chain, filename, fileline);
		// free the pool itself
		memset(pool, 0xBF, sizeof(mempool_t));
		free(pool);
		*poolptr = NULL;
	}
}

void _Mem_EmptyPool(byte *poolptr, const char *filename, int fileline)
{
	mempool_t *pool = (mempool_t *)((byte *)poolptr);
	if (poolptr == NULL) Sys_Error("Mem_EmptyPool: pool == NULL (emptypool at %s:%i)", filename, fileline);

	if (pool->sentinel1 != MEMHEADER_SENTINEL1) Sys_Error("Mem_EmptyPool: trashed pool sentinel 1 (allocpool at %s:%i, emptypool at %s:%i)", pool->filename, pool->fileline, filename, fileline);
	if (pool->sentinel2 != MEMHEADER_SENTINEL1) Sys_Error("Mem_EmptyPool: trashed pool sentinel 2 (allocpool at %s:%i, emptypool at %s:%i)", pool->filename, pool->fileline, filename, fileline);

	// free memory owned by the pool
	while (pool->chain) _Mem_FreeBlock(pool->chain, filename, fileline);
}

void _Mem_CheckSentinels(void *data, const char *filename, int fileline)
{
	memheader_t *mem;

	if (data == NULL) Sys_Error("Mem_CheckSentinels: data == NULL (sentinel check at %s:%i)", filename, fileline);
	mem = (memheader_t *)((unsigned char *) data - sizeof(memheader_t));
	if (mem->sentinel1 != MEMHEADER_SENTINEL1)
		Sys_Error("Mem_CheckSentinels: trashed header sentinel 1 (block allocated at %s:%i, sentinel check at %s:%i)", mem->filename, mem->fileline, filename, fileline);
	if (*((unsigned char *) mem + sizeof(memheader_t) + mem->size) != MEMHEADER_SENTINEL2)
		Sys_Error("Mem_CheckSentinels: trashed header sentinel 2 (block allocated at %s:%i, sentinel check at %s:%i)", mem->filename, mem->fileline, filename, fileline);
}

static void _Mem_CheckClumpSentinels(memclump_t *clump, const char *filename, int fileline)
{
	// this isn't really very useful
	if (clump->sentinel1 != MEMCLUMP_SENTINEL)
		Sys_Error("Mem_CheckClumpSentinels: trashed sentinel 1 (sentinel check at %s:%i)", filename, fileline);
	if (clump->sentinel2 != MEMCLUMP_SENTINEL)
		Sys_Error("Mem_CheckClumpSentinels: trashed sentinel 2 (sentinel check at %s:%i)", filename, fileline);
}

void _Mem_CheckSentinelsGlobal(const char *filename, int fileline)
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
			_Mem_CheckSentinels((void *)((unsigned char *) mem + sizeof(memheader_t)), filename, fileline);

	for (pool = poolchain;pool;pool = pool->next)
		for (clump = pool->clumpchain;clump;clump = clump->chain)
			_Mem_CheckClumpSentinels(clump, filename, fileline);
}

void _Mem_Move (void *dest, void *src, size_t size, const char *filename, int fileline)
{
	if (src == NULL || size <= 0) return;
	if (dest == NULL) dest = _Mem_Alloc( basepool, size, filename, fileline); //allocate room

	// move block
	memmove( dest, src, size );
}

void _Mem_Copy (void *dest, void *src, size_t size, const char *filename, int fileline)
{
	if (src == NULL || size <= 0) return; //nothing to copy
	if (dest == NULL) Sys_Error("Mem_Copy: dest == NULL (called at %s:%i)", filename, fileline);

	//copy block
	memcpy( dest, src, size );
}

/*
=============================================================================

EXTERNAL MEMORY MANAGER INTERFACE
=============================================================================
*/
memsystem_api_t Mem_GetAPI( void )
{
	static memsystem_api_t	mem;

	mem.api_size = sizeof(memsystem_api_t);

	mem.AllocPool = _Mem_AllocPool;
	mem.EmptyPool = _Mem_EmptyPool;
	mem.FreePool = _Mem_FreePool;
	mem.CheckSentinelsGlobal = _Mem_CheckSentinelsGlobal;

	mem.Alloc = _Mem_Alloc;
	mem.Realloc = _Mem_Realloc;
	mem.Move = _Mem_Move;
	mem.Copy = _Mem_Copy;
	mem.Free = _Mem_Free;

	return mem;
}

/*
========================
Memory_Init
========================
*/
void InitMemory (void)
{
	poolchain = NULL;//init mem chain
	basepool = Mem_AllocPool( "Temp" );
	zonepool = Mem_AllocPool( "Zone" );
}

void FreeMemory( void )
{
	Mem_FreePool( &basepool );
	Mem_FreePool( &zonepool );

	//abnormal freeing pools
	Mem_FreePool( &studiopool );
}