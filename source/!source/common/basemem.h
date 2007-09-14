//=======================================================================
//			Copyright XashXT Group 2007 ©
//			basemem.h - base memory manager
//=======================================================================
#ifndef BASEMEMORY_H
#define BASEMEMORY_H

// give malloc padding so we can't waste most of a page at the end
#define MEMCLUMPSIZE (65536 - 1536)
// smallest unit we care about is this many bytes
#define MEMUNIT 8
#define MEMBITS (MEMCLUMPSIZE / MEMUNIT)
#define MEMBITINTS (MEMBITS / 32)
#define MEMCLUMP_SENTINEL 0xABADCAFE

#define MEMHEADER_SENTINEL1 0xDEADF00D
#define MEMHEADER_SENTINEL2 0xDF

#include <setjmp.h>

typedef struct memheader_s
{
	// next and previous memheaders in chain belonging to pool
	struct memheader_s	*next;
	struct memheader_s	*prev;
	struct mempool_s	*pool;// pool this memheader belongs to

	// clump this memheader lives in, NULL if not in a clump
	struct memclump_s *clump;

	size_t size; // size of the memory after the header (excluding header and sentinel2)
	const char *filename;// file name and line where Mem_Alloc was called
	int fileline;
	unsigned int sentinel1;// should always be MEMHEADER_SENTINEL1
	// immediately followed by data, which is followed by a MEMHEADER_SENTINEL2 byte
}
memheader_t;

typedef struct memclump_s
{
	// contents of the clump
	byte block[MEMCLUMPSIZE];
	// should always be MEMCLUMP_SENTINEL
	unsigned int sentinel1;
	// if a bit is on, it means that the MEMUNIT bytes it represents are
	// allocated, otherwise free
	int bits[MEMBITINTS];
	// should always be MEMCLUMP_SENTINEL
	unsigned int sentinel2;
	// if this drops to 0, the clump is freed
	size_t blocksinuse;
	// largest block of memory available (this is reset to an optimistic
	// number when anything is freed, and updated when alloc fails the clump)
	size_t largestavailable;
	// next clump in the chain
	struct memclump_s *chain;
}
memclump_t;

typedef struct mempool_s
{
	
	unsigned int sentinel1;// should always be MEMHEADER_SENTINEL1
	struct memheader_s *chain;// chain of individual memory allocations
	struct memclump_s *clumpchain;// chain of clumps (if any)
	size_t totalsize;// total memory allocated in this pool (inside memheaders)
	size_t realsize;// total memory allocated in this pool (actual malloc total)
	// updated each time the pool is displayed by memlist, shows change from previous time (unless pool was freed)
	size_t lastchecksize;
	struct mempool_s *next;// linked into global mempool list
	const char *filename;// file name and line where Mem_AllocPool was called
	int fileline;
	char name[128];// name of the pool
	unsigned int sentinel2;// should always be MEMHEADER_SENTINEL1
}
mempool_t;

extern byte *basepool;
extern byte *zonepool;

#endif//BASEMEMORY_H