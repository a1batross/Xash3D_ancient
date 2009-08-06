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

#define TINY_BLOCK_COPY	64		// upper limit for movsd type copy
#define IN_CACHE_COPY	64 * 1024		// upper limit for movq/movq copy w/SW prefetch
#define UNCACHED_COPY	197 * 1024	// upper limit for movq/movntq w/SW prefetch
#define BLOCK_PREFETCH_COPY	infinity		// no limit for movq/movntq w/block prefetch 
#define CACHEBLOCK		80h		// number of 64-byte blocks (cache lines) for block prefetch

typedef enum
{
	PREFETCH_READ,		// prefetch assuming that buffer is used for reading only
	PREFETCH_WRITE,		// prefetch assuming that buffer is used for writing only
	PREFETCH_READWRITE		// prefetch assuming that buffer is used for both reading and writing
} e_prefetch;

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
	char		name[64];		// name of the pool
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

// crt safe version
void _crt_mem_copy( void *dest, const void *src, size_t count, const char *filename, int fileline )
{
	if( src == NULL || count <= 0 ) return; // nothing to copy
	if( dest == NULL ) Sys_Error( "Mem_Copy: dest == NULL (called at %s:%i)\n", filename, fileline );
	memcpy( dest, src, count );
}

// q_memcpy
void _com_mem_copy( void *dest, const void *src, size_t count, const char *filename, int fileline )
{
	int	i;

	if( src == NULL || count <= 0 ) return; // nothing to copy
	if( dest == NULL ) Sys_Error( "Mem_Copy: dest == NULL (called at %s:%i)\n", filename, fileline );
	
	if((((long)dest | (long)src | count) & 3) == 0 )
	{
		count>>=2;
		for( i = 0; i < count; i++ )
			((int *)dest)[i] = ((int *)src)[i];
	}
	else
	{
		for( i = 0; i < count; i++ )
			((byte *)dest)[i] = ((byte *)src)[i];
	}
}

// mmx version
void _mmx_mem_copy8B( void *dest, const void *src, size_t count, const char *filename, int fileline )
{
	if( src == NULL || count <= 0 ) return; // nothing to copy
	if( dest == NULL ) Sys_Error( "Mem_Copy: dest == NULL (called at %s:%i)\n", filename, fileline );

	_asm
	{
		mov esi, src
		mov edi, dest
		mov ecx, count
		shr ecx, 3	// 8 bytes per iteration
loop1:
		movq mm1, [ESI]	// read in source data
		movntq [EDI], mm1	// non-temporal stores

		add esi, 8
		add edi, 8
		dec ecx
		jnz loop1
		emms
	}
}

void _mmx_mem_copy64B( void *dest, const void *src, size_t count, const char *filename, int fileline )
{
	if( src == NULL || count <= 0 ) return; // nothing to copy
	if( dest == NULL ) Sys_Error( "Mem_Copy: dest == NULL (called at %s:%i)\n", filename, fileline );

	_asm
	{
		mov esi, src
		mov edi, dest
		mov ecx, count
		shr ecx, 6		// 64 bytes per iteration 
loop1:
		prefetchnta 64[ESI]		// prefetch next loop, non-temporal 
		prefetchnta 96[ESI]

		movq mm1, [ESI]		// read in source data 
		movq mm2, 8[ESI]
		movq mm3, 16[ESI]
		movq mm4, 24[ESI]
		movq mm5, 32[ESI]
		movq mm6, 40[ESI]
		movq mm7, 48[ESI]
		movq mm0, 56[ESI]

		movntq [EDI], mm1		// non-temporal stores 
		movntq 8[EDI], mm2
		movntq 16[EDI], mm3
		movntq 24[EDI], mm4
		movntq 32[EDI], mm5
		movntq 40[EDI], mm6
		movntq 48[EDI], mm7
		movntq 56[EDI], mm0

		add esi, 64
		add edi, 64
		dec ecx
		jnz loop1
		emms
	}
}

void _mmx_mem_copy2kB( void *dest, const void *src, size_t count, const char *filename, int fileline )
{
	byte	buf[2048];
	byte	*tbuf = &buf[0];

	if( src == NULL || count <= 0 ) return; // nothing to copy
	if( dest == NULL ) Sys_Error( "Mem_Copy: dest == NULL (called at %s:%i)\n", filename, fileline );

	__asm
	{
		push ebx
		mov esi, src
		mov ebx, count
		shr ebx, 11		// 2048 bytes at a time 
		mov edi, dest
loop2k:
		push edi			// copy 2k into temporary buffer 
		mov edi, tbuf
		mov ecx, 32
loopMemToL1:
		prefetchnta 64[ESI]		// prefetch next loop, non-temporal 
		prefetchnta 96[ESI]

		movq mm1, [ESI]		// read in source data 
		movq mm2, 8[ESI]
		movq mm3, 16[ESI]
		movq mm4, 24[ESI]
		movq mm5, 32[ESI]
		movq mm6, 40[ESI]
		movq mm7, 48[ESI]
		movq mm0, 56[ESI]

		movq [EDI], mm1		// store into L1 
		movq 8[EDI], mm2
		movq 16[EDI], mm3
		movq 24[EDI], mm4
		movq 32[EDI], mm5
		movq 40[EDI], mm6
		movq 48[EDI], mm7
		movq 56[EDI], mm0
		add esi, 64
		add edi, 64
		dec ecx
		jnz loopMemToL1

		pop edi			// now copy from L1 to system memory 
		push esi
		mov esi, tbuf
		mov ecx, 32
loopL1ToMem:
		movq mm1, [ESI]		// read in source data from L1 
		movq mm2, 8[ESI]
		movq mm3, 16[ESI]
		movq mm4, 24[ESI]
		movq mm5, 32[ESI]
		movq mm6, 40[ESI]
		movq mm7, 48[ESI]
		movq mm0, 56[ESI]

		movntq [EDI], mm1		// Non-temporal stores 
		movntq 8[EDI], mm2
		movntq 16[EDI], mm3
		movntq 24[EDI], mm4
		movntq 32[EDI], mm5
		movntq 40[EDI], mm6
		movntq 48[EDI], mm7
		movntq 56[EDI], mm0

		add esi, 64
		add edi, 64
		dec ecx
		jnz loopL1ToMem

		pop esi			// do next 2k block 
		dec ebx
		jnz loop2k
		pop ebx
		emms
	}
}

void _mmx_mem_copy( void *dest, const void *src, size_t size, const char *filename, int fileline )
{
	if( src == NULL || size <= 0 ) return; // nothing to copy
	if( dest == NULL ) Sys_Error( "Mem_Copy: dest == NULL (called at %s:%i)\n", filename, fileline );

	// if copying more than 16 bytes and we can copy 8 byte aligned
	if( size > 16 && !(((int)dest ^ (int)src) & 7 ))
	{
		byte	*dest_p = (byte *)dest;
		byte	*src_p = (byte *)src;
		int	count = ((int)dest_p) & 7;

		// copy up to the first 8 byte aligned boundary
		_crt_mem_copy( dest_p, src_p, count, filename, fileline );
		dest_p += count;
		src_p += count;
		count = size - count;

		// if there are multiple blocks of 2kB
		if( count & ~4095 )
		{
			_mmx_mem_copy2kB( dest_p, src_p, count, filename, fileline );
			src_p += (count & ~2047);
			dest_p += (count & ~2047);
			count &= 2047;
		}
		// if there are blocks of 64 bytes
		if( count & ~63 )
		{
			_mmx_mem_copy64B( dest_p, src_p, count, filename, fileline );
			src_p += (count & ~63);
			dest_p += (count & ~63);
			count &= 63;
		}

		// if there are blocks of 8 bytes
		if( count & ~7 )
		{
			_mmx_mem_copy8B( dest_p, src_p, count, filename, fileline );
			src_p += (count & ~7);
			dest_p += (count & ~7);
			count &= 7;
		}
		// copy any remaining bytes
		_crt_mem_copy( dest_p, src_p, count, filename, fileline );
	}
	else
	{
		// use the regular one if we cannot copy 8 byte aligned
		_crt_mem_copy( dest, src, size, filename, fileline );
	}
}

void _amd_mem_copy( void *dest, const void *src, size_t count, const char *filename, int fileline )
{
	if( src == NULL || count <= 0 ) return; // nothing to copy
	if( dest == NULL ) Sys_Error( "Mem_Copy: dest == NULL (called at %s:%i)\n", filename, fileline );

	__asm
	{
		mov	ecx, [count]	// number of bytes to copy
		mov	edi, [dest]	// destination
		mov	esi, [src]	// source
		mov	ebx, ecx		// keep a copy of count

		cld
		cmp	ecx, TINY_BLOCK_COPY
		jb	$memcopy_ic_3			// tiny? skip mmx copy

		cmp	ecx, 32*1024			// don't align between 32k-64k because
		jbe	$memcopy_do_align			//  it appears to be slower
		cmp	ecx, 64*1024
		jbe	$memcopy_align_done
$memcopy_do_align:
		mov	ecx, 8				// a trick that's faster than rep movsb...
		sub	ecx, edi				// align destination to qword
		and	ecx, 111b				// get the low bits
		sub	ebx, ecx				// update copy count
		neg	ecx				// set up to jump into the array
		add	ecx, offset $memcopy_align_done
		jmp	ecx				// jump to array of movsb's

align 4
		movsb
		movsb
		movsb
		movsb
		movsb
		movsb
		movsb
		movsb

$memcopy_align_done:					// destination is dword aligned
		mov	ecx, ebx				// number of bytes left to copy
		shr	ecx, 6				// get 64-byte block count
		jz	$memcopy_ic_2			// finish the last few bytes

		cmp	ecx, IN_CACHE_COPY/64		// too big 4 cache? use uncached copy
		jae	$memcopy_uc_test

align 16
$memcopy_ic_1:						// 64-byte block copies, in-cache copy

		prefetchnta [esi + (200*64/34+192)]		// start reading ahead

		movq	mm0, [esi+0]			// read 64 bits
		movq	mm1, [esi+8]
		movq	[edi+0], mm0			// write 64 bits
		movq	[edi+8], mm1			// NOTE:  the normal movq writes the
		movq	mm2, [esi+16]			// data to cache; a cache line will be
		movq	mm3, [esi+24]			// allocated as needed, to store the data
		movq	[edi+16], mm2
		movq	[edi+24], mm3
		movq	mm0, [esi+32]
		movq	mm1, [esi+40]
		movq	[edi+32], mm0
		movq	[edi+40], mm1
		movq	mm2, [esi+48]
		movq	mm3, [esi+56]
		movq	[edi+48], mm2
		movq	[edi+56], mm3

		add	esi, 64				// update source pointer
		add	edi, 64				// update destination pointer
		dec	ecx				// count down
		jnz	$memcopy_ic_1			// last 64-byte block?

$memcopy_ic_2:
		mov	ecx, ebx				// has valid low 6 bits of the byte count
$memcopy_ic_3:
		shr	ecx, 2				// dword count
		and	ecx, 1111b			// only look at the "remainder" bits
		neg	ecx				// set up to jump into the array
		add	ecx, offset $memcopy_last_few
		jmp	ecx				// jump to array of movsd's

$memcopy_uc_test:
		cmp	ecx, UNCACHED_COPY/64		// big enough? use block prefetch copy
		jae	$memcopy_bp_1

$memcopy_64_test:
		or	ecx, ecx				// tail end of block prefetch will jump here
		jz	$memcopy_ic_2			// no more 64-byte blocks left

align 16
$memcopy_uc_1:						// 64-byte blocks, uncached copy

		prefetchnta [esi + (200*64/34+192)]		// start reading ahead

		movq	mm0,[esi+0]			// read 64 bits
		add	edi,64				// update destination pointer
		movq	mm1,[esi+8]
		add	esi,64				// update source pointer
		movq	mm2,[esi-48]
		movntq	[edi-64], mm0			// write 64 bits, bypassing the cache
		movq	mm0,[esi-40]			//    NOTE: movntq also prevents the CPU
		movntq	[edi-56], mm1			//    from READING the destination address
		movq	mm1,[esi-32]			//    into the cache, only to be over-written
		movntq	[edi-48], mm2			//    so that also helps performance
		movq	mm2,[esi-24]
		movntq	[edi-40], mm0
		movq	mm0,[esi-16]
		movntq	[edi-32], mm1
		movq	mm1,[esi-8]
		movntq	[edi-24], mm2
		movntq	[edi-16], mm0
		dec	ecx
		movntq	[edi-8], mm1
		jnz	$memcopy_uc_1			// last 64-byte block?

		jmp	$memcopy_ic_2			// almost done

$memcopy_bp_1:						// large blocks, block prefetch copy

		cmp		ecx, CACHEBLOCK		// big enough to run another prefetch loop?
		jl		$memcopy_64_test		// no, back to regular uncached copy

		mov		eax, CACHEBLOCK / 2		// block prefetch loop, unrolled 2X
		add		esi, CACHEBLOCK * 64	// move to the top of the block
align 16
$memcopy_bp_2:
		mov		edx, [esi-64]		// grab one address per cache line
		mov		edx, [esi-128]		// grab one address per cache line
		sub		esi, 128			// go reverse order
		dec		eax			// count down the cache lines
		jnz		$memcopy_bp_2		// keep grabbing more lines into cache

		mov		eax, CACHEBLOCK		// now that it's in cache, do the copy
align 16
$memcopy_bp_3:
		movq	mm0, [esi   ]			// read 64 bits
		movq	mm1, [esi+ 8]
		movq	mm2, [esi+16]
		movq	mm3, [esi+24]
		movq	mm4, [esi+32]
		movq	mm5, [esi+40]
		movq	mm6, [esi+48]
		movq	mm7, [esi+56]
		add	esi, 64				// update source pointer
		movntq	[edi   ], mm0			// write 64 bits, bypassing cache
		movntq	[edi+ 8], mm1			// NOTE: movntq also prevents the CPU
		movntq	[edi+16], mm2			// from READING the destination address 
		movntq	[edi+24], mm3			// into the cache, only to be over-written,
		movntq	[edi+32], mm4			// so that also helps performance
		movntq	[edi+40], mm5
		movntq	[edi+48], mm6
		movntq	[edi+56], mm7
		add	edi, 64				// update dest pointer

		dec	eax				// count down

		jnz	$memcopy_bp_3			// keep copying
		sub	ecx, CACHEBLOCK			// update the 64-byte block count
		jmp	$memcopy_bp_1			// keep processing chunks

align 4
		movsd
		movsd					// perform last 1-15 dword copies
		movsd
		movsd
		movsd
		movsd
		movsd
		movsd
		movsd
		movsd					// perform last 1-7 dword copies
		movsd
		movsd
		movsd
		movsd
		movsd
		movsd

$memcopy_last_few:						// dword aligned from before movsd's
		mov		ecx, ebx			// has valid low 2 bits of the byte count
		and		ecx, 11b			// the last few cows must come home
		jz		$memcopy_final		// no more, let's leave
		rep		movsb			// the last 1, 2, or 3 bytes

$memcopy_final: 
		emms					// clean up the MMX state
		sfence					// flush the write buffer
		mov		eax, [dest]		// ret value = destination pointer
	}
}

void _mem_prefetch( const void *s, const uint bytes, e_prefetch type )
{
	// write buffer prefetching is performed only if
	// the processor benefits from it. read and read/write
	// prefetching is always performed.

	switch( type )
	{
		case PREFETCH_WRITE : break;
		case PREFETCH_READ:
		case PREFETCH_READWRITE:

		__asm
		{
			mov	ebx, s
			mov	ecx, bytes
			cmp	ecx, 4096	// clamp to 4kB
			jle	skipClump
			mov	ecx, 4096
skipClump:
			add	ecx, 0x1f
			shr	ecx, 5	// number of cache lines
			jz	skip
			jmp	loopie

			align 16
	loopie:	test	byte ptr [ebx], al
			add	ebx, 32
			dec	ecx
			jnz	loopie
		skip:
		}
		break;
	}
}

void _mem_copy_dword( uint *dest, const uint constant, const uint count )
{
	__asm
	{
		mov	edx,dest
		mov	eax,constant
		mov	ecx,count
		and	ecx,~7
		jz 	padding
		sub	ecx,8
		jmp	loopu
		align	16
loopu:		
		test	[edx+ecx*4 + 28],ebx	// fetch next block destination to L1 cache
		mov	[edx+ecx*4 + 0],eax
		mov	[edx+ecx*4 + 4],eax
		mov	[edx+ecx*4 + 8],eax
		mov	[edx+ecx*4 + 12],eax
		mov	[edx+ecx*4 + 16],eax
		mov	[edx+ecx*4 + 20],eax
		mov	[edx+ecx*4 + 24],eax
		mov	[edx+ecx*4 + 28],eax
		sub	ecx,8
		jge	loopu
padding:		mov	ecx,count
		mov	ebx,ecx
		and	ecx,7
		jz 	outta
		and	ebx,~7
		lea	edx,[edx+ebx*4]		// advance dest pointer
		test	[edx+0],eax		// fetch destination to L1 cache
		cmp	ecx,4
		jl 	skip4
		mov	[edx+0],eax
		mov	[edx+4],eax
		mov	[edx+8],eax
		mov	[edx+12],eax
		add	edx,16
		sub	ecx,4
skip4:		cmp	ecx,2
		jl 	skip2
		mov	[edx+0],eax
		mov	[edx+4],eax
		add	edx,8
		sub	ecx,2
skip2:		cmp	ecx,1
		jl 	outta
		mov	[edx+0],eax
outta:
	}
}

void _asm_mem_copy( void *dest, const void *src, size_t count, const char *filename, int fileline )
{
	if( src == NULL || count <= 0 ) return; // nothing to copy
	if( dest == NULL) Sys_Error("Mem_Copy: dest == NULL (called at %s:%i)\n", filename, fileline );

	// copy block
	_mem_prefetch( src, count, PREFETCH_READ );
	__asm
	{
		push	edi
		push	esi
		mov	ecx,count
		cmp	ecx,0			// count = 0 check (just to be on the safe side)
		je	outta
		mov	edx,dest
		mov	ebx,src
		cmp	ecx,32			// padding only?
		jl	padding

		mov	edi,ecx					
		and	edi,~31			// edi = count&~31
		sub	edi,32

		align 16
loopMisAligned:
		mov	eax,[ebx + edi + 0 + 0*8]
		mov	esi,[ebx + edi + 4 + 0*8]
		mov	[edx+edi+0 + 0*8],eax
		mov	[edx+edi+4 + 0*8],esi
		mov	eax,[ebx + edi + 0 + 1*8]
		mov	esi,[ebx + edi + 4 + 1*8]
		mov	[edx+edi+0 + 1*8],eax
		mov	[edx+edi+4 + 1*8],esi
		mov	eax,[ebx + edi + 0 + 2*8]
		mov	esi,[ebx + edi + 4 + 2*8]
		mov	[edx+edi+0 + 2*8],eax
		mov	[edx+edi+4 + 2*8],esi
		mov	eax,[ebx + edi + 0 + 3*8]
		mov	esi,[ebx + edi + 4 + 3*8]
		mov	[edx+edi+0 + 3*8],eax
		mov	[edx+edi+4 + 3*8],esi
		sub	edi,32
		jge	loopMisAligned
		
		mov	edi,ecx
		and	edi,~31
		add	ebx,edi			// increase src pointer
		add	edx,edi			// increase dst pointer
		and	ecx,31			// new count
		jz	outta			// if count = 0, get outta here

padding:
		cmp	ecx,16
		jl	skip16
		mov	eax,dword ptr [ebx]
		mov	dword ptr [edx],eax
		mov	eax,dword ptr [ebx+4]
		mov	dword ptr [edx+4],eax
		mov	eax,dword ptr [ebx+8]
		mov	dword ptr [edx+8],eax
		mov	eax,dword ptr [ebx+12]
		mov	dword ptr [edx+12],eax
		sub	ecx,16
		add	ebx,16
		add	edx,16
skip16:
		cmp	ecx,8
		jl	skip8
		mov	eax,dword ptr [ebx]
		mov	dword ptr [edx],eax
		mov	eax,dword ptr [ebx+4]
		sub	ecx,8
		mov	dword ptr [edx+4],eax
		add	ebx,8
		add	edx,8
skip8:
		cmp	ecx,4
		jl	skip4
		mov	eax,dword ptr [ebx]		// here 4-7 bytes
		add	ebx,4
		sub	ecx,4
		mov	dword ptr [edx],eax
		add	edx,4
skip4:						// 0-3 remaining bytes
		cmp	ecx,2
		jl	skip2
		mov	ax,word ptr [ebx]		// two bytes
		cmp	ecx,3			// less than 3?
		mov	word ptr [edx],ax
		jl	outta
		mov	al,byte ptr [ebx+2]		// last byte
		mov	byte ptr [edx+2],al
		jmp	outta
skip2:
		cmp	ecx,1
		jl	outta
		mov	al,byte ptr [ebx]
		mov	byte ptr [edx],al
outta:
		pop	esi
		pop	edi
	}
}

void _crt_mem_set( void *dest, int set, size_t count, const char *filename, int fileline )
{
	if( dest == NULL ) Sys_Error( "Mem_Set: dest == NULL (called at %s:%i)\n", filename, fileline );
	memset( dest, set, count );
}

void _com_mem_set( void *dest, int set, size_t count, const char *filename, int fileline )
{
	int	i;

	if( dest == NULL ) Sys_Error( "Mem_Set: dest == NULL (called at %s:%i)\n", filename, fileline );	
	if( !( ((long)dest | count) & 3 ))
	{
		count >>= 2;
		set = (set<<0) | (set<<8) | (set<<16) | (set<<24);
		for( i = 0; i < count; i++ ) ((int *)dest)[i] = set;
	}
	else
	{
		for( i = 0; i < count; i++ ) ((byte *)dest)[i] = set;
	}
}


void _asm_mem_set( void* dest, int set, size_t count, const char *filename, int fileline )
{
	uint	fillval;

	if( dest == NULL ) Sys_Error( "Mem_Set: dest == NULL (called at %s:%i)\n", filename, fileline );
	if( count < 8 )
	{
		__asm
		{
			mov	edx,dest
			mov	eax, set
			mov	ah,al
			mov	ebx,eax
			and	ebx, 0xffff
			shl	eax,16
			add	eax,ebx		// eax now contains pattern
			mov	ecx,count
			cmp	ecx,4
			jl	skip4
			mov	[edx],eax		// copy first dword
			add	edx,4
			sub	ecx,4
	skip4:	cmp		ecx,2
			jl	skip2
			mov	word ptr [edx],ax	// copy 2 bytes
			add	edx,2
			sub	ecx,2
	skip2:	cmp		ecx,0
			je	skip1
			mov	byte ptr [edx],al	// copy single byte
	skip1:
		}
		return;
	}

	fillval = set;
	fillval = fillval|(fillval<<8);
	fillval = fillval|(fillval<<16);		// fill dword with 8-bit pattern

	_mem_copy_dword((uint *)(dest), fillval, count / 4 );
	
	__asm					// padding of 0-3 bytes
	{
		mov	ecx,count
		mov	eax,ecx
		and	ecx,3
		jz	skipA
		and	eax,~3
		mov	ebx,dest
		add	ebx,eax
		mov	eax,fillval
		cmp	ecx,2
		jl	skipB
		mov	word ptr [ebx],ax
		cmp	ecx,2
		je	skipA					
		mov	byte ptr [ebx+2],al		
		jmp	skipA
skipB:		
		cmp	ecx,0
		je	skipA
		mov	byte ptr [ebx],al
skipA:
	}
}

void _mmx_mem_set( void* dest, int set, size_t size, const char *filename, int fileline )
{
	union
	{
		byte	bytes[8];
		WORD	words[4];
		DWORD	dwords[2];
	} dat;

	byte	*dst = (byte *)dest;
	int	count = size;

	if( dest == NULL ) Sys_Error( "Mem_Set: dest == NULL (called at %s:%i)\n", filename, fileline );

	while( count > 0 && (((int)dst) & 7) )
	{
		*dst = set;
		dst++;
		count--;
	}
	if( !count ) return;

	dat.bytes[0] = set;
	dat.bytes[1] = set;
	dat.words[1] = dat.words[0];
	dat.dwords[1] = dat.dwords[0];

	if( count >= 64 )
	{
		__asm
		{
			mov edi, dst
			mov ecx, count
			shr ecx, 6		// 64 bytes per iteration
			movq mm1, dat	// Read in source data
			movq mm2, mm1
			movq mm3, mm1
			movq mm4, mm1
			movq mm5, mm1
			movq mm6, mm1
			movq mm7, mm1
			movq mm0, mm1
loop1:
			movntq 0[EDI], mm1	// Non-temporal stores
			movntq 8[EDI], mm2
			movntq 16[EDI], mm3
			movntq 24[EDI], mm4
			movntq 32[EDI], mm5
			movntq 40[EDI], mm6
			movntq 48[EDI], mm7
			movntq 56[EDI], mm0

			add edi, 64
			dec ecx
			jnz loop1
		}
		dst += ( count & ~63 );
		count &= 63;
	}

	if ( count >= 8 )
	{
		__asm
		{
			mov edi, dst
			mov ecx, count
			shr ecx, 3		// 8 bytes per iteration
			movq mm1, dat	// Read in source data
			loop2:
			movntq 0[EDI], mm1	// Non-temporal stores

			add edi, 8
			dec ecx
			jnz loop2
		}
		dst += (count & ~7);
		count &= 7;
	}

	while( count > 0 )
	{
		*dst = set;
		dst++;
		count--;
	}

	__asm
	{
		emms
	}
}

void *_mem_alloc( byte *poolptr, size_t size, const char *filename, int fileline )
{
	int		i, j, k, needed, endbit, largest;
	memclump_t	*clump, **clumpchainpointer;
	memheader_t	*mem;
	mempool_t		*pool = (mempool_t *)((byte *)poolptr);

	if( size <= 0 ) return NULL;
	if( poolptr == NULL ) Sys_Error( "Mem_Alloc: pool == NULL (alloc at %s:%i)\n", filename, fileline );
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
		com.memset( clump, 0, sizeof( memclump_t ), filename, fileline );
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
	com.memset((void *)((byte *) mem + sizeof(memheader_t)), 0, mem->size, filename, fileline );

	MsgDev( D_MEMORY, "Mem_Alloc: \"%s\"[%s], at (%s:%i)\n", pool->name, com_pretifymem( size, 1 ), filename, fileline );

	return (void *)((byte *) mem + sizeof(memheader_t));
}

static const char *_mem_check_filename( const char *filename )
{
	static const char	*dummy = "<corrupted>\0";
	const char	*out = filename;
	int		i;

	if( !out ) return dummy;
	for( i = 0; i < 32; i++, out++ )
		if( out == '\0' ) break; // valid name
	if( i == 32 ) return dummy;
	return filename;
}

static void _mem_freeblock(memheader_t *mem, const char *filename, int fileline)
{
	int i, firstblock, endblock;
	memclump_t *clump, **clumpchainpointer;
	mempool_t *pool;

	if( mem->sentinel1 != MEMHEADER_SENTINEL1 )
	{
		mem->filename = _mem_check_filename( mem->filename ); // make sure what we don't crash var_args
		Sys_Error( "Mem_Free: trashed header sentinel 1 (alloc at %s:%i, free at %s:%i)\n", mem->filename, mem->fileline, filename, fileline );
	}
	if(*((byte *) mem + sizeof(memheader_t) + mem->size) != MEMHEADER_SENTINEL2 )
	{	
		mem->filename = _mem_check_filename( mem->filename ); // make sure what we don't crash var_args
		Sys_Error( "Mem_Free: trashed header sentinel 2 (alloc at %s:%i, free at %s:%i)\n", mem->filename, mem->fileline, filename, fileline );
	}
	pool = mem->pool;
	// unlink memheader from doubly linked list
	if ((mem->prev ? mem->prev->next != mem : pool->chain != mem) || (mem->next && mem->next->prev != mem))
		Sys_Error("Mem_Free: not allocated or double freed (free at %s:%i)\n", filename, fileline);
	if (mem->prev) mem->prev->next = mem->next;
	else pool->chain = mem->next;
	if (mem->next) mem->next->prev = mem->prev;
	// memheader has been unlinked, do the actual free now
	pool->totalsize -= mem->size;

	MsgDev(D_MEMORY, "Mem_Free: \"%s\"[%s], at (%s:%i)\n", pool->name, com_pretifymem( mem->size, 1 ), filename, fileline );
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
			pool->realsize -= sizeof( memclump_t );
			com.memset( clump, 0xBF, sizeof( memclump_t ), filename, fileline );
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

	if( size <= 0 ) return memptr; // no need to reallocate
	nb = _mem_alloc( poolptr, size, filename, fileline );

	if( memptr ) // first allocate?
	{ 
		size_t	newsize;

		// get size of old block
		memhdr = (memheader_t *)((byte *)memptr - sizeof(memheader_t));
		newsize = memhdr->size < size ? memhdr->size : size; // upper data can be trucnated!
		com.memcpy( nb, memptr, newsize, filename, fileline );
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
	else com.memset( memptr, 0x00, size, filename, fileline );		// no need to reallocate buffer
	
	com.memcpy( memptr, src, size, filename, fileline );		// move memory...
	_mem_free( src, filename, fileline );				// ...and free old pointer

	*dest = memptr;
}

byte *_mem_allocpool(const char *name, const char *filename, int fileline)
{
	mempool_t *pool;
	pool = (mempool_t *)malloc(sizeof(mempool_t));
	if( pool == NULL ) Sys_Error( "Mem_AllocPool: out of memory (allocpool at %s:%i)\n", filename, fileline );
	com.memset( pool, 0, sizeof(mempool_t), filename, fileline );

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

	MsgDev( D_MEMORY, "Create pool: \"%s\", at (%s:%i)\n", pool->name, filename, fileline );
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
		while (pool->chain) _mem_freeblock( pool->chain, filename, fileline );
		// free the pool itself
		com.memset( pool, 0xBF, sizeof( mempool_t ), filename, fileline );
		free( pool );
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
					com.memcpy(l->arrays, oldarrays, l->numarrays * sizeof(*l->arrays), filename, fileline);
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
	if( mem->sentinel1 != MEMHEADER_SENTINEL1 )
	{
		mem->filename = _mem_check_filename( mem->filename ); // make sure what we don't crash var_args
		Sys_Error("Mem_CheckSentinels: trashed header sentinel 1 (block allocated at %s:%i, sentinel check at %s:%i)\n", mem->filename, mem->fileline, filename, fileline);
	}
	if(*((byte *) mem + sizeof(memheader_t) + mem->size) != MEMHEADER_SENTINEL2 )
	{	
		mem->filename = _mem_check_filename( mem->filename ); // make sure what we don't crash var_args
		Sys_Error("Mem_CheckSentinels: trashed header sentinel 2 (block allocated at %s:%i, sentinel check at %s:%i)\n", mem->filename, mem->fileline, filename, fileline);
	}
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
	for( pool = poolchain; pool; pool = pool->next )
	{
		// poolnames can contain color symbols, make sure what color is reset
		Msg( "%5luk (%5luk actual) %s (^7%+3li byte change)\n", (dword) ((pool->totalsize + 1023) / 1024), (dword)((pool->realsize + 1023) / 1024), pool->name, (long)pool->totalsize - pool->lastchecksize );
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
		_mem_printlist(com_atoi(Cmd_Argv(1)) * 1024);
		_mem_printstats();
		break;
	default:
		Msg( "Usage: memlist <all>\n" );
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
	Sys.scriptpool = Mem_AllocPool( "Parse Lib" );
}

void Memory_Shutdown( void )
{
	Mem_FreePool( &Sys.basepool );
	Mem_FreePool( &Sys.stringpool );
	Mem_FreePool( &Sys.scriptpool );
}

void Memory_Init_Commands( void )
{
	Cmd_AddCommand( "memlist", MemList_f, "prints memory pool information (or if used as memlist 5 lists individual allocations of 5K or larger, 0 lists all allocations)");
	Cmd_AddCommand( "stringlist", StringTable_Info_f, "prints all known strings for selected StringTable" );
}