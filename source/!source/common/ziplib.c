//=======================================================================
//			Copyright (C) XashXT Group 2006
//			     All Rights Reserved
//=======================================================================

#include "platform.h"
#include "baseutils.h"
#include "zip32.h"

#define BASE 65521UL    // largest prime smaller than 65536
#define NMAX 5552

//crc 32
#define DO1 crc = crc_table[0][((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8)
#define DO8 DO1; DO1; DO1; DO1; DO1; DO1; DO1; DO1

//adler 32
#define BASE 65521UL    /* largest prime smaller than 65536 */
#define ADO1(buf,i)  {adler += (buf)[i]; sum2 += adler;}
#define ADO2(buf,i)  ADO1(buf,i); ADO1(buf,i+1);
#define ADO4(buf,i)  ADO2(buf,i); ADO2(buf,i+2);
#define ADO8(buf,i)  ADO4(buf,i); ADO4(buf,i+4);
#define ADO16(buf)   ADO8(buf,0); ADO8(buf,8);
#define MOD(a) a %= BASE
#define MOD4(a) a %= BASE

//memory opertions
byte* zcalloc (byte* opaque, uint items, uint size)
{
	return (byte *)Malloc( items * size );
}

void zcfree (byte* opaque, byte* ptr)
{
	Free( ptr );
}

static const code lenfix[512] = {
	{96,7,0},{0,8,80},{0,8,16},{20,8,115},{18,7,31},{0,8,112},{0,8,48},
	{0,9,192},{16,7,10},{0,8,96},{0,8,32},{0,9,160},{0,8,0},{0,8,128},
	{0,8,64},{0,9,224},{16,7,6},{0,8,88},{0,8,24},{0,9,144},{19,7,59},
	{0,8,120},{0,8,56},{0,9,208},{17,7,17},{0,8,104},{0,8,40},{0,9,176},
	{0,8,8},{0,8,136},{0,8,72},{0,9,240},{16,7,4},{0,8,84},{0,8,20},
	{21,8,227},{19,7,43},{0,8,116},{0,8,52},{0,9,200},{17,7,13},{0,8,100},
	{0,8,36},{0,9,168},{0,8,4},{0,8,132},{0,8,68},{0,9,232},{16,7,8},
	{0,8,92},{0,8,28},{0,9,152},{20,7,83},{0,8,124},{0,8,60},{0,9,216},
	{18,7,23},{0,8,108},{0,8,44},{0,9,184},{0,8,12},{0,8,140},{0,8,76},
	{0,9,248},{16,7,3},{0,8,82},{0,8,18},{21,8,163},{19,7,35},{0,8,114},
	{0,8,50},{0,9,196},{17,7,11},{0,8,98},{0,8,34},{0,9,164},{0,8,2},
	{0,8,130},{0,8,66},{0,9,228},{16,7,7},{0,8,90},{0,8,26},{0,9,148},
	{20,7,67},{0,8,122},{0,8,58},{0,9,212},{18,7,19},{0,8,106},{0,8,42},
	{0,9,180},{0,8,10},{0,8,138},{0,8,74},{0,9,244},{16,7,5},{0,8,86},
	{0,8,22},{64,8,0},{19,7,51},{0,8,118},{0,8,54},{0,9,204},{17,7,15},
	{0,8,102},{0,8,38},{0,9,172},{0,8,6},{0,8,134},{0,8,70},{0,9,236},
	{16,7,9},{0,8,94},{0,8,30},{0,9,156},{20,7,99},{0,8,126},{0,8,62},
	{0,9,220},{18,7,27},{0,8,110},{0,8,46},{0,9,188},{0,8,14},{0,8,142},
	{0,8,78},{0,9,252},{96,7,0},{0,8,81},{0,8,17},{21,8,131},{18,7,31},
	{0,8,113},{0,8,49},{0,9,194},{16,7,10},{0,8,97},{0,8,33},{0,9,162},
	{0,8,1},{0,8,129},{0,8,65},{0,9,226},{16,7,6},{0,8,89},{0,8,25},
	{0,9,146},{19,7,59},{0,8,121},{0,8,57},{0,9,210},{17,7,17},{0,8,105},
	{0,8,41},{0,9,178},{0,8,9},{0,8,137},{0,8,73},{0,9,242},{16,7,4},
	{0,8,85},{0,8,21},{16,8,258},{19,7,43},{0,8,117},{0,8,53},{0,9,202},
	{17,7,13},{0,8,101},{0,8,37},{0,9,170},{0,8,5},{0,8,133},{0,8,69},
	{0,9,234},{16,7,8},{0,8,93},{0,8,29},{0,9,154},{20,7,83},{0,8,125},
	{0,8,61},{0,9,218},{18,7,23},{0,8,109},{0,8,45},{0,9,186},{0,8,13},
	{0,8,141},{0,8,77},{0,9,250},{16,7,3},{0,8,83},{0,8,19},{21,8,195},
	{19,7,35},{0,8,115},{0,8,51},{0,9,198},{17,7,11},{0,8,99},{0,8,35},
	{0,9,166},{0,8,3},{0,8,131},{0,8,67},{0,9,230},{16,7,7},{0,8,91},
	{0,8,27},{0,9,150},{20,7,67},{0,8,123},{0,8,59},{0,9,214},{18,7,19},
	{0,8,107},{0,8,43},{0,9,182},{0,8,11},{0,8,139},{0,8,75},{0,9,246},
	{16,7,5},{0,8,87},{0,8,23},{64,8,0},{19,7,51},{0,8,119},{0,8,55},
	{0,9,206},{17,7,15},{0,8,103},{0,8,39},{0,9,174},{0,8,7},{0,8,135},
	{0,8,71},{0,9,238},{16,7,9},{0,8,95},{0,8,31},{0,9,158},{20,7,99},
	{0,8,127},{0,8,63},{0,9,222},{18,7,27},{0,8,111},{0,8,47},{0,9,190},
	{0,8,15},{0,8,143},{0,8,79},{0,9,254},{96,7,0},{0,8,80},{0,8,16},
	{20,8,115},{18,7,31},{0,8,112},{0,8,48},{0,9,193},{16,7,10},{0,8,96},
	{0,8,32},{0,9,161},{0,8,0},{0,8,128},{0,8,64},{0,9,225},{16,7,6},
	{0,8,88},{0,8,24},{0,9,145},{19,7,59},{0,8,120},{0,8,56},{0,9,209},
	{17,7,17},{0,8,104},{0,8,40},{0,9,177},{0,8,8},{0,8,136},{0,8,72},
	{0,9,241},{16,7,4},{0,8,84},{0,8,20},{21,8,227},{19,7,43},{0,8,116},
	{0,8,52},{0,9,201},{17,7,13},{0,8,100},{0,8,36},{0,9,169},{0,8,4},
	{0,8,132},{0,8,68},{0,9,233},{16,7,8},{0,8,92},{0,8,28},{0,9,153},
	{20,7,83},{0,8,124},{0,8,60},{0,9,217},{18,7,23},{0,8,108},{0,8,44},
	{0,9,185},{0,8,12},{0,8,140},{0,8,76},{0,9,249},{16,7,3},{0,8,82},
	{0,8,18},{21,8,163},{19,7,35},{0,8,114},{0,8,50},{0,9,197},{17,7,11},
	{0,8,98},{0,8,34},{0,9,165},{0,8,2},{0,8,130},{0,8,66},{0,9,229},
	{16,7,7},{0,8,90},{0,8,26},{0,9,149},{20,7,67},{0,8,122},{0,8,58},
	{0,9,213},{18,7,19},{0,8,106},{0,8,42},{0,9,181},{0,8,10},{0,8,138},
	{0,8,74},{0,9,245},{16,7,5},{0,8,86},{0,8,22},{64,8,0},{19,7,51},
	{0,8,118},{0,8,54},{0,9,205},{17,7,15},{0,8,102},{0,8,38},{0,9,173},
	{0,8,6},{0,8,134},{0,8,70},{0,9,237},{16,7,9},{0,8,94},{0,8,30},
	{0,9,157},{20,7,99},{0,8,126},{0,8,62},{0,9,221},{18,7,27},{0,8,110},
	{0,8,46},{0,9,189},{0,8,14},{0,8,142},{0,8,78},{0,9,253},{96,7,0},
	{0,8,81},{0,8,17},{21,8,131},{18,7,31},{0,8,113},{0,8,49},{0,9,195},
	{16,7,10},{0,8,97},{0,8,33},{0,9,163},{0,8,1},{0,8,129},{0,8,65},
	{0,9,227},{16,7,6},{0,8,89},{0,8,25},{0,9,147},{19,7,59},{0,8,121},
	{0,8,57},{0,9,211},{17,7,17},{0,8,105},{0,8,41},{0,9,179},{0,8,9},
	{0,8,137},{0,8,73},{0,9,243},{16,7,4},{0,8,85},{0,8,21},{16,8,258},
	{19,7,43},{0,8,117},{0,8,53},{0,9,203},{17,7,13},{0,8,101},{0,8,37},
	{0,9,171},{0,8,5},{0,8,133},{0,8,69},{0,9,235},{16,7,8},{0,8,93},
	{0,8,29},{0,9,155},{20,7,83},{0,8,125},{0,8,61},{0,9,219},{18,7,23},
	{0,8,109},{0,8,45},{0,9,187},{0,8,13},{0,8,141},{0,8,77},{0,9,251},
	{16,7,3},{0,8,83},{0,8,19},{21,8,195},{19,7,35},{0,8,115},{0,8,51},
	{0,9,199},{17,7,11},{0,8,99},{0,8,35},{0,9,167},{0,8,3},{0,8,131},
	{0,8,67},{0,9,231},{16,7,7},{0,8,91},{0,8,27},{0,9,151},{20,7,67},
	{0,8,123},{0,8,59},{0,9,215},{18,7,19},{0,8,107},{0,8,43},{0,9,183},
	{0,8,11},{0,8,139},{0,8,75},{0,9,247},{16,7,5},{0,8,87},{0,8,23},
	{64,8,0},{19,7,51},{0,8,119},{0,8,55},{0,9,207},{17,7,15},{0,8,103},
	{0,8,39},{0,9,175},{0,8,7},{0,8,135},{0,8,71},{0,9,239},{16,7,9},
	{0,8,95},{0,8,31},{0,9,159},{20,7,99},{0,8,127},{0,8,63},{0,9,223},
	{18,7,27},{0,8,111},{0,8,47},{0,9,191},{0,8,15},{0,8,143},{0,8,79},
	{0,9,255}
};

static const code distfix[32] = {
	{16,5,1},{23,5,257},{19,5,17},{27,5,4097},{17,5,5},{25,5,1025},
	{21,5,65},{29,5,16385},{16,5,3},{24,5,513},{20,5,33},{28,5,8193},
	{18,5,9},{26,5,2049},{22,5,129},{64,5,0},{16,5,2},{23,5,385},
	{19,5,25},{27,5,6145},{17,5,7},{25,5,1537},{21,5,97},{29,5,24577},
	{16,5,4},{24,5,769},{20,5,49},{28,5,12289},{18,5,13},{26,5,3073},
	{22,5,193},{64,5,0}
};

dword adler32(dword adler, const byte *buf, dword len)
{
	dword	sum2;
	uint	n;

	// split Adler-32 into component sums
	sum2 = (adler >> 16) & 0xffff;
	adler &= 0xffff;

	/* in case user likes doing a byte at a time, keep it fast */
	if (len == 1)
	{
		adler += buf[0];
		if (adler >= BASE) adler -= BASE;
		sum2 += adler;
		if (sum2 >= BASE) sum2 -= BASE;
		return adler | (sum2 << 16);
	}

	/* initial Adler-32 value (deferred check for len == 1 speed) */
	if (buf == NULL) return 1L;

	/* in case short lengths are provided, keep it somewhat fast */
	if (len < 16)
	{
		while (len--)
		{
			adler += *buf++;
			sum2 += adler;
		}

		if (adler >= BASE) adler -= BASE;
		MOD4(sum2);		// only added so many BASE's
		return adler | (sum2 << 16);
	}

	// do length NMAX blocks -- requires just one modulo operation
	while (len >= NMAX)
	{
		len -= NMAX;
		n = NMAX / 16;		// NMAX is divisible by 16
		do
		{
			ADO16(buf);	// 16 sums unrolled
			buf += 16;
		} while (--n);
        		MOD(adler);
		MOD(sum2);
	}

	// do remaining bytes (less than NMAX, still just one modulo)
	if (len) // avoid modulos if none remaining
	{
		while (len >= 16)
		{
			len -= 16;
			ADO16(buf);
			buf += 16;
		}
		while (len--)
		{
			adler += *buf++;
			sum2 += adler;
		}
		MOD(adler);
		MOD(sum2);
	}

	// return recombined sums
	return adler | (sum2 << 16);
}

//=======================================================================
//			INFALTE STUFF
//=======================================================================

int inflateInit_(z_streamp strm, const char *version, int stream_size )
{
	//internal checking
	return inflateInit2_(strm, MAX_WBITS, version, stream_size);
}

int inflateInit2_(z_streamp strm, int windowBits, const char *version, int stream_size)
{
	struct inflate_state *state;

	if (version == NULL || version[0] != ZLIB_VERSION[0] || stream_size != (int)(sizeof(z_stream)))
		return Z_VERSION_ERROR;
    
	if (strm == NULL) return Z_STREAM_ERROR;
	strm->msg = NULL; //in case we return an error
	if (strm->zalloc == NULL)
	{
		strm->zalloc = zcalloc;
		strm->opaque = NULL;
    	}

	if (strm->zfree == NULL) strm->zfree = zcfree;
	state = (struct inflate_state *) ZipAlloc(strm, 1, sizeof(struct inflate_state));
	if (state == NULL) return Z_MEM_ERROR;
	strm->state = (struct int_state *)state;
	if (windowBits < 0)
	{
		state->wrap = 0;
		windowBits = -windowBits;
	}
	else state->wrap = (windowBits >> 4) + 1;
	if (windowBits < 8 || windowBits > 15)
	{
		ZipFree(strm, (byte*)state );
		strm->state = NULL;
		return Z_STREAM_ERROR;
	}

	state->wbits = (uint)windowBits;
	state->window = NULL;
	return inflateReset(strm);
}

int inflate_table(codetype type, unsigned short *lens, unsigned codes, code **table, unsigned *bits, unsigned short *work)
{
	unsigned len;               /* a code's length in bits */
	unsigned sym;               /* index of code symbols */
	unsigned min, max;          /* minimum and maximum code lengths */
	unsigned root;              /* number of index bits for root table */
	unsigned curr;              /* number of index bits for current table */
	unsigned drop;              /* code bits to drop for sub-table */
	int left;                   /* number of prefix codes available */
	unsigned used;              /* code entries in table used */
	unsigned huff;              /* Huffman code */
	unsigned incr;              /* for incrementing code, index */
	unsigned fill;              /* index for replicating entries */
	unsigned low;               /* low bits for current root entry */
	unsigned mask;              /* mask for low root bits */
	code this;                  /* table entry for duplication */
	code *next;                 /* next available space in table */
	const unsigned short *base; /* base value table to use */
	const unsigned short *extra;/* extra bits table to use */
	int end;                    /* use base and extra for symbol > end */
	unsigned short count[MAX_WBITS+1];    /* number of codes of each length */
	unsigned short offs[MAX_WBITS+1];     /* offsets in table for each length */
	static const unsigned short lbase[31] = { /* Length codes 257..285 base */
	3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
	35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0};
	static const unsigned short lext[31] = { /* Length codes 257..285 extra */
	16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 18,
	19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 16, 201, 196};
	static const unsigned short dbase[32] = { /* Distance codes 0..29 base */
	1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
	257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
	8193, 12289, 16385, 24577, 0, 0};
	static const unsigned short dext[32] = { /* Distance codes 0..29 extra */
	16, 16, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22,
	23, 23, 24, 24, 25, 25, 26, 26, 27, 27,
	28, 28, 29, 29, 64, 64};

	/* accumulate lengths for codes (assumes lens[] all in 0..MAXBITS) */
	for (len = 0; len <= MAX_WBITS; len++) count[len] = 0;
	for (sym = 0; sym < codes; sym++) count[lens[sym]]++;

	/* bound code lengths, force root to be within code lengths */
	root = *bits;
	for (max = MAX_WBITS; max >= 1; max--) if (count[max] != 0) break;
	if (root > max) root = max;
	if (max == 0)                     /* no symbols to code at all */
	{
		this.op = (unsigned char)64;    /* invalid code marker */
		this.bits = (unsigned char)1;
		this.val = (unsigned short)0;
		*(*table)++ = this;             /* make a table to force an error */
		*(*table)++ = this;
		*bits = 1;
		return 0;     /* no symbols, but wait for decoding to report error */
	}
	for (min = 1; min <= MAX_WBITS; min++) if (count[min] != 0) break;
	if (root < min) root = min;

	/* check for an over-subscribed or incomplete set of lengths */
	left = 1;
	for (len = 1; len <= MAX_WBITS; len++)
	{
		left <<= 1;
		left -= count[len];
		if (left < 0) return -1;        /* over-subscribed */
	}
	if (left > 0 && (type == CODES || max != 1)) return -1;  /* incomplete set */

	/* generate offsets into symbol table for each length for sorting */
	offs[1] = 0;
	for (len = 1; len < MAX_WBITS; len++) offs[len + 1] = offs[len] + count[len];

	/* sort symbols by length, by symbol order within each length */
	for (sym = 0; sym < codes; sym++) if (lens[sym] != 0) work[offs[lens[sym]]++] = (unsigned short)sym;


	/* set up for code type */
	switch (type)
	{
	case CODES:
		base = extra = work;    /* dummy value--not used */
		end = 19;
		break;
	case LENS:
		base = lbase;
		base -= 257;
		extra = lext;
		extra -= 257;
		end = 256;
		break;
	default:            /* DISTS */
		base = dbase;
		extra = dext;
		end = -1;
	}

	/* initialize state for loop */
	huff = 0;                   /* starting code */
	sym = 0;                    /* starting code symbol */
	len = min;                  /* starting code length */
	next = *table;              /* current table to fill in */
	curr = root;                /* current table index bits */
	drop = 0;                   /* current bits to drop from code for index */
	low = (unsigned)(-1);       /* trigger new sub-table when len > root */
	used = 1U << root;          /* use root table entries */
	mask = used - 1;            /* mask for comparing low */

	/* check available table space */
	if (type == LENS && used >= 2048 - 592) return 1;

	/* process all codes and make table entries */
	for (;;)
	{
		/* create table entry */
		this.bits = (unsigned char)(len - drop);
		if ((int)(work[sym]) < end)
		{
			this.op = (unsigned char)0;
			this.val = work[sym];
		}
		else if ((int)(work[sym]) > end)
		{
			this.op = (unsigned char)(extra[work[sym]]);
			this.val = base[work[sym]];
		}
		else
		{
			this.op = (unsigned char)(32 + 64);         /* end of block */
			this.val = 0;
		}

		/* replicate for those indices with low len bits equal to huff */
		incr = 1U << (len - drop);
		fill = 1U << curr;
		min = fill;                 /* save offset to next table */
		do{
            	fill -= incr;
            	next[(huff >> drop) + fill] = this;
		}while (fill != 0);

		/* backwards increment the len-bit code huff */
		incr = 1U << (len - 1);
		while (huff & incr) incr >>= 1;
		if (incr != 0)
		{
			huff &= incr - 1;
			huff += incr;
		}
		else huff = 0;

		/* go to next symbol, update count, len */
		sym++;
		if (--(count[len]) == 0)
		{
			if (len == max) break;
			len = lens[work[sym]];
		}

		/* create new sub-table if needed */
		if (len > root && (huff & mask) != low)
		{
			/* if first time, transition to sub-tables */
			if (drop == 0) drop = root;
			/* increment past last table */
			next += min; /* here min is 1 << curr */

			/* determine length of next table */
			curr = len - drop;
			left = (int)(1 << curr);
			while (curr + drop < max)
			{
				left -= count[curr + drop];
				if (left <= 0) break;
				curr++;
				left <<= 1;
			}

			/* check for enough space */
			used += 1U << curr;
			if (type == LENS && used >= 2048 - 592) return 1;

			/* point entry in root table to sub-table */
			low = huff & mask;
			(*table)[low].op = (unsigned char)curr;
			(*table)[low].bits = (unsigned char)root;
			(*table)[low].val = (unsigned short)(next - *table);
		}
	}

	this.op = (unsigned char)64;                /* invalid code marker */
	this.bits = (unsigned char)(len - drop);
	this.val = (unsigned short)0;
	while (huff != 0)
	{
		/* when done with sub-table, drop back to root table */
		if (drop != 0 && (huff & mask) != low)
		{
			drop = 0;
			len = root;
			next = *table;
			this.bits = (unsigned char)len;
		}

		/* put invalid code marker in table */
		next[huff >> drop] = this;

		/* backwards increment the len-bit code huff */
		incr = 1U << (len - 1);
		while (huff & incr) incr >>= 1;
		if (incr != 0)
		{
			huff &= incr - 1;
			huff += incr;
		}
		else huff = 0;
	}

	// set return parameters
	*table += used;
	*bits = root;
	return 0;
}

void inflate_fast(z_streamp strm, unsigned start)
{
	struct inflate_state *state;
	byte 		*in;      /* local strm->next_in */
	byte		*last;    /* while in < last, enough input available */
	unsigned char *out;     /* local strm->next_out */
	unsigned char *beg;     /* inflate()'s initial strm->next_out */
	unsigned char *end;     /* while out < end, enough space available */
	unsigned wsize;             /* window size or zero if not using window */
	unsigned whave;             /* valid bytes in the window */
	unsigned write;             /* window write index */
	unsigned char *window;      /* allocated sliding window, if wsize != 0 */
	unsigned long hold;         /* local strm->hold */
	unsigned bits;              /* local strm->bits */
	code const *lcode;          /* local strm->lencode */
	code const *dcode;          /* local strm->distcode */
	unsigned lmask;             /* mask for first level of length codes */
	unsigned dmask;             /* mask for first level of distance codes */
	code this;                  /* retrieved table entry */
	unsigned op;                /* code bits, operation, extra bits, or */
                                      /*  window position, window bytes to copy */
	unsigned len;               /* match length, unused bytes */
	unsigned dist;              /* match distance */
	unsigned char *from;    /* where to copy match from */

	/* copy state to local variables */
	state = (struct inflate_state *)strm->state;
	in = strm->next_in;
	last = in + (strm->avail_in - 5);
	out = strm->next_out;
	beg = out - (start - strm->avail_out);
	end = out + (strm->avail_out - 257);
	wsize = state->wsize;
	whave = state->whave;
	write = state->write;
	window = state->window;
	hold = state->hold;
	bits = state->bits;
	lcode = state->lencode;
	dcode = state->distcode;
	lmask = (1U << state->lenbits) - 1;
	dmask = (1U << state->distbits) - 1;

	/* decode literals and length/distances until end-of-block or not enough
	input data or output space */
	do {
	if (bits < 15)
	{
		hold += (unsigned long)(PUP(in)) << bits;
		bits += 8;
		hold += (unsigned long)(PUP(in)) << bits;
		bits += 8;
	}
	this = lcode[hold & lmask];
dolen:
	op = (unsigned)(this.bits);
	hold >>= op;
	bits -= op;
	op = (unsigned)(this.op);
	if (op == 0) PUP(out) = (byte)(this.val);
	else if (op & 16)                     /* length base */
	{
		len = (unsigned)(this.val);
		op &= 15;                   /* number of extra bits */
		if (op)
		{
			if (bits < op)
			{
				hold += (unsigned long)(PUP(in)) << bits;
				bits += 8;
			}
			len += (unsigned)hold & ((1U << op) - 1);
			hold >>= op;
			bits -= op;
		}
		if (bits < 15)
		{
			hold += (dword)(PUP(in)) << bits;
			bits += 8;
			hold += (dword)(PUP(in)) << bits;
			bits += 8;
		}
		this = dcode[hold & dmask];
dodist:
		op = (unsigned)(this.bits);
		hold >>= op;
		bits -= op;
		op = (unsigned)(this.op);
		if (op & 16)                      /* distance base */
		{                
			dist = (unsigned)(this.val);
			op &= 15;               /* number of extra bits */
			if (bits < op)
			{
				hold += (unsigned long)(PUP(in)) << bits;
				bits += 8;
				if (bits < op)
				{
					hold += (unsigned long)(PUP(in)) << bits;
					bits += 8;
				}
			}
			dist += (unsigned)hold & ((1U << op) - 1);
			hold >>= op;
			bits -= op;
			op = (unsigned)(out - beg);     /* max distance in output */
			if (dist > op)                  /* see if copy from window */
			{
				op = dist - op;       /* distance back in window */
				if (op > whave)
				{
					strm->msg = (char *)"invalid distance too far back";
					state->mode = BAD;
					break;
				}
				from = window;
				if (write == 0)        // very common case
				{
					from += wsize - op;
					if (op < len) // some from window
					{
						len -= op;
						do{
						PUP(out) = PUP(from);
						}while (--op);
						from = out - dist;  // rest from output
					}
				}
				else if (write < op)      // wrap around window
				{
					from += wsize + write - op;
					op -= write;
					if (op < len)   /* some from end of window */
					{                            
						len -= op;
						do{
						PUP(out) = PUP(from);
						}while (--op);
						from = window;
						if (write < len)  /* some from start of window */
						{
							op = write;
							len -= op;
							do{
							PUP(out) = PUP(from);
							}while (--op);
							from = out - dist;      /* rest from output */
						}
					}
				}
				else                      /* contiguous in window */
				{
					from += write - op;
					if (op < len)   /* some from window */
					{
						len -= op;
						do{
						PUP(out) = PUP(from);
						}while (--op);
						from = out - dist;  /* rest from output */
					}
				}
				while (len > 2)
				{
					PUP(out) = PUP(from);
					PUP(out) = PUP(from);
					PUP(out) = PUP(from);
					len -= 3;
				}
				if (len)
				{
					PUP(out) = PUP(from);
					if (len > 1) PUP(out) = PUP(from);
				}
			}
			else
			{
				from = out - dist;		// copy direct from output
				do			// minimum length is three
				{
					PUP(out) = PUP(from);
					PUP(out) = PUP(from);
					PUP(out) = PUP(from);
					len -= 3;
				}while (len > 2);
				if (len)
				{
					PUP(out) = PUP(from);
					if (len > 1) PUP(out) = PUP(from);
				}
			}
		}
		else if ((op & 64) == 0)          // 2nd level distance code
		{
			this = dcode[this.val + (hold & ((1U << op) - 1))];
			goto dodist;
		}
		else
		{
			strm->msg = (char *)"invalid distance code";
			state->mode = BAD;
			break;
		}
	}
	else if ((op & 64) == 0)              //2nd level length code
	{
		this = lcode[this.val + (hold & ((1U << op) - 1))];
		goto dolen;
	}
	else if (op & 32)                     /* end-of-block */
	{
		state->mode = TYPE;
		break;
	}
	else
	{
		strm->msg = (char *)"invalid literal/length code";
		state->mode = BAD;
		break;
	}
	}while (in < last && out < end);

	// return unused bytes (on entry, bits < 8, so in won't go too far back)
	len = bits >> 3;
	in -= len;
	bits -= len << 3;
	hold &= (1U << bits) - 1;

	// update state and return
	strm->next_in = in;
	strm->next_out = out;
	strm->avail_in = (unsigned)(in < last ? 5 + (last - in) : 5 - (in - last));
	strm->avail_out = (unsigned)(out < end ? 257 + (end - out) : 257 - (out - end));
	state->hold = hold;
	state->bits = bits;
	return;  //???
}

void fixedtables(struct inflate_state *state)
{
	state->lencode = lenfix;
	state->lenbits = 9;
	state->distcode = distfix;
	state->distbits = 5;
}

int updatewindow(z_streamp strm, unsigned out)
{
	struct inflate_state *state;
	unsigned copy, dist;

	state = (struct inflate_state *)strm->state;

	/* if it hasn't been done already, allocate space for the window */
	if (state->window == NULL)
	{
		state->window = (unsigned char *)
		ZipAlloc(strm, 1U << state->wbits, (sizeof(byte)));
		if (state->window == NULL) return 1;
	}

	/* if window not in use yet, initialize */
	if (state->wsize == 0)
	{
		state->wsize = 1U << state->wbits;
		state->write = 0;
		state->whave = 0;
	}

	/* copy state->wsize or less output bytes into the circular window */
	copy = out - strm->avail_out;
	if (copy >= state->wsize)
	{
		Mem_Copy(state->window, strm->next_out - state->wsize, state->wsize);
		state->write = 0;
		state->whave = state->wsize;
	}
	else
	{
		dist = state->wsize - state->write;
		if (dist > copy) dist = copy;
		memcpy(state->window + state->write, strm->next_out - copy, dist);
		copy -= dist;

		if (copy)
		{
			memcpy(state->window, strm->next_out - copy, copy);
			state->write = copy;
			state->whave = state->wsize;
		}
		else
		{
			state->write += dist;
			if (state->write == state->wsize) state->write = 0;
			if (state->whave < state->wsize) state->whave += dist;
		}
	}
	return 0;
}

// Macros for inflate():

// check function to use adler32() for zlib or crc32() for gzip
#define UPDATE(check, buf, len) adler32(check, buf, len)

// Load registers with state in inflate() for speed
#define LOAD() \
	do { \
	put = strm->next_out; \
	left = strm->avail_out; \
	next = strm->next_in; \
	have = strm->avail_in; \
	hold = state->hold; \
	bits = state->bits; \
	} while (0)

// Restore state from registers in inflate()
#define RESTORE() \
	do { \
	strm->next_out = put; \
	strm->avail_out = left; \
	strm->next_in = next; \
	strm->avail_in = have; \
	state->hold = hold; \
	state->bits = bits; \
	} while (0)

// Clear the input bit accumulator
#define INITBITS() \
	do { \
	hold = 0; \
	bits = 0; \
	} while (0)

// Get a byte of input into the bit accumulator, or return from inflate()
// if there is no input available.
#define PULLBYTE() \
	do { \
	if (have == 0) goto inf_leave; \
	have--; \
	hold += (unsigned long)(*next++) << bits; \
	bits += 8; \
	} while (0)

// Assure that there are at least n bits in the bit accumulator.  If there is
// not enough available input to do that, then return from inflate().
#define NEEDBITS(n) \
	do { \
	while (bits < (unsigned)(n)) \
	PULLBYTE(); \
	} while (0)

// Return the low n bits of the bit accumulator (n < 16)
#define BITS(n) \
	((unsigned)hold & ((1U << (n)) - 1))

// Remove n bits from the bit accumulator
#define DROPBITS(n) \
	do { \
	hold >>= (n); \
	bits -= (unsigned)(n); \
	} while (0)

// Remove zero to seven bits as needed to go to a byte boundary
#define BYTEBITS() \
	do { \
	hold >>= bits & 7; \
	bits -= bits & 7; \
	} while (0)

// Reverse the bytes in a 32-bit value
#define REVERSE(q) \
	((((q) >> 24) & 0xff) + (((q) >> 8) & 0xff00) + \
	(((q) & 0xff00) << 8) + (((q) & 0xff) << 24))

int inflate(z_streamp strm, int flush)
{
	struct inflate_state *state;
	unsigned char *next;    /* next input */
	unsigned char *put;     /* next output */
	unsigned have, left;        /* available input and output */
	unsigned long hold;         /* bit buffer */
	unsigned bits;              /* bits in bit buffer */
	unsigned in, out;           /* save starting available input and output */
	unsigned copy;              /* number of stored or match bytes to copy */
	unsigned char *from;        /* where to copy match bytes from */
	code this;                  /* current decoding table entry */
	code last;                  /* parent table entry */
	unsigned len;               /* length to copy for repeats, bits to drop */
	int ret;                    /* return code */
          
	// permutation of code lengths
	static const unsigned short order[19] ={16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

	if (strm == NULL || strm->state == NULL || strm->next_out == NULL || (strm->next_in == NULL && strm->avail_in != 0))
		return Z_STREAM_ERROR;

	state = (struct inflate_state *)strm->state;
	if (state->mode == TYPE) state->mode = TYPEDO;      /* skip check */
	LOAD();
	in = have;
	out = left;
	ret = Z_OK;

	for (;;)
	switch (state->mode)
	{
	case HEAD:
		if (state->wrap == 0)
		{
			state->mode = TYPEDO;
			break;
		}
		NEEDBITS(16);

		if (((BITS(8) << 8) + (hold >> 8)) % 31) // check if zlib header allowed
		{
			strm->msg = (char *)"incorrect header check";
			state->mode = BAD;
			break;
		}
		if (BITS(4) != Z_DEFLATED)
		{
			strm->msg = (char *)"unknown compression method";
			state->mode = BAD;
			break;
		}
		DROPBITS(4);
		len = BITS(4) + 8;
		if (len > state->wbits)
		{
			strm->msg = (char *)"invalid window size";
			state->mode = BAD;
			break;
		}
		state->dmax = 1U << len;
		strm->adler = state->check = adler32(0L, NULL, 0);
		state->mode = hold & 0x200 ? DICTID : TYPE;
		INITBITS();
		break;
	case DICTID:
		NEEDBITS(32);
		strm->adler = state->check = REVERSE(hold);
		INITBITS();
		state->mode = DICT;
	case DICT:
		if (state->havedict == 0)
		{
			RESTORE();
			return Z_NEED_DICT;
		}
		strm->adler = state->check = adler32(0L, NULL, 0);
		state->mode = TYPE;
	case TYPE:
		if (flush == Z_BLOCK) goto inf_leave;
	case TYPEDO:
		if (state->last)
		{
			BYTEBITS();
			state->mode = CHECK;
			break;
		}
		NEEDBITS(3);
		state->last = BITS(1);
		DROPBITS(1);
		switch (BITS(2))
		{
		case 0:                             /* stored block */
			state->mode = STORED;
			break;
		case 1:                             /* fixed block */
			fixedtables(state);
			state->mode = LEN;              /* decode codes */
			break;
		case 2:                             /* dynamic block */
			state->mode = TABLE;
			break;
		case 3:
			strm->msg = (char *)"invalid block type";
			state->mode = BAD;
		}
		DROPBITS(2);
		break;
	case STORED:
		BYTEBITS();                         /* go to byte boundary */
		NEEDBITS(32);
		if ((hold & 0xffff) != ((hold >> 16) ^ 0xffff))
		{
			strm->msg = (char *)"invalid stored block lengths";
			state->mode = BAD;
			break;
		}
		state->length = (unsigned)hold & 0xffff;
		INITBITS();
		state->mode = COPY;
	case COPY:
		copy = state->length;
		if (copy)
		{
			if (copy > have) copy = have;
			if (copy > left) copy = left;
			if (copy == 0) goto inf_leave;
			memcpy(put, next, copy);
			have -= copy;
			next += copy;
			left -= copy;
			put += copy;
			state->length -= copy;
			break;
		}
		state->mode = TYPE;
		break;
	case TABLE:
		NEEDBITS(14);
		state->nlen = BITS(5) + 257;
		DROPBITS(5);
		state->ndist = BITS(5) + 1;
		DROPBITS(5);
		state->ncode = BITS(4) + 4;
		DROPBITS(4);
		if (state->nlen > 286 || state->ndist > 30)
		{
			strm->msg = (char *)"too many length or distance symbols";
			state->mode = BAD;
			break;
		}
		state->have = 0;
		state->mode = LENLENS;
	case LENLENS:
		while (state->have < state->ncode)
		{
			NEEDBITS(3);
			state->lens[order[state->have++]] = (unsigned short)BITS(3);
			DROPBITS(3);
		}
		while (state->have < 19) state->lens[order[state->have++]] = 0;
		state->next = state->codes;
		state->lencode = (code const *)(state->next);
		state->lenbits = 7;
		ret = inflate_table(CODES, state->lens, 19, &(state->next), &(state->lenbits), state->work);
		if (ret)
		{
			strm->msg = (char *)"invalid code lengths set";
			state->mode = BAD;
			break;
		}
		state->have = 0;
		state->mode = CODELENS;
	case CODELENS:
		while (state->have < state->nlen + state->ndist)
		{
			for (;;)
			{
				this = state->lencode[BITS(state->lenbits)];
				if ((unsigned)(this.bits) <= bits) break;
				PULLBYTE();
			}
			if (this.val < 16)
			{
				NEEDBITS(this.bits);
				DROPBITS(this.bits);
				state->lens[state->have++] = this.val;
			}
			else
			{
				if (this.val == 16)
				{
					NEEDBITS(this.bits + 2);
					DROPBITS(this.bits);
					if (state->have == 0)
					{
						strm->msg = (char *)"invalid bit length repeat";
						state->mode = BAD;
						break;
					}
					len = state->lens[state->have - 1];
					copy = 3 + BITS(2);
					DROPBITS(2);
				}
				else if (this.val == 17)
				{
					NEEDBITS(this.bits + 3);
					DROPBITS(this.bits);
					len = 0;
					copy = 3 + BITS(3);
					DROPBITS(3);
				}
				else
				{
					NEEDBITS(this.bits + 7);
					DROPBITS(this.bits);
					len = 0;
					copy = 11 + BITS(7);
					DROPBITS(7);
				}
				if (state->have + copy > state->nlen + state->ndist)
				{
					strm->msg = (char *)"invalid bit length repeat";
					state->mode = BAD;
					break;
				}
				while (copy--) state->lens[state->have++] = (unsigned short)len;
			}
		}

		// handle error breaks in while
		if (state->mode == BAD) break;

		// build code tables
		state->next = state->codes;
		state->lencode = (code const *)(state->next);
		state->lenbits = 9;
		ret = inflate_table(LENS, state->lens, state->nlen, &(state->next), &(state->lenbits), state->work);
		if (ret)
		{
			strm->msg = (char *)"invalid literal/lengths set";
			state->mode = BAD;
			break;
		}
		state->distcode = (code const *)(state->next);
		state->distbits = 6;
		ret = inflate_table(DISTS, state->lens + state->nlen, state->ndist, &(state->next), &(state->distbits), state->work);
		if (ret)
		{
			strm->msg = (char *)"invalid distances set";
			state->mode = BAD;
			break;
		}
		state->mode = LEN;
	case LEN:
		if (have >= 6 && left >= 258)
		{
			RESTORE();
			inflate_fast(strm, out);
			LOAD();
			break;
		}
		for (;;)
		{
			this = state->lencode[BITS(state->lenbits)];
			if ((unsigned)(this.bits) <= bits) break;
			PULLBYTE();
		}
		if (this.op && (this.op & 0xf0) == 0)
		{
			last = this;
			for (;;)
			{
				this = state->lencode[last.val + (BITS(last.bits + last.op) >> last.bits)];
				if ((unsigned)(last.bits + this.bits) <= bits) break;
				PULLBYTE();
			}
			DROPBITS(last.bits);
		}
		DROPBITS(this.bits);
		state->length = (unsigned)this.val;
		if ((int)(this.op) == 0)
		{
			state->mode = LIT;
			break;
		}
		if (this.op & 32)
		{
			state->mode = TYPE;
			break;
		}
		if (this.op & 64)
		{
			strm->msg = (char *)"invalid literal/length code";
			state->mode = BAD;
			break;
		}
		state->extra = (unsigned)(this.op) & 15;
		state->mode = LENEXT;
	case LENEXT:
		if (state->extra)
		{
			NEEDBITS(state->extra);
			state->length += BITS(state->extra);
			DROPBITS(state->extra);
		}
		state->mode = DIST;
	case DIST:
		for (;;)
		{
			this = state->distcode[BITS(state->distbits)];
                		if ((unsigned)(this.bits) <= bits) break;
			PULLBYTE();
		}
		if ((this.op & 0xf0) == 0)
		{
			last = this;
			for (;;)
			{
				this = state->distcode[last.val + (BITS(last.bits + last.op) >> last.bits)];
				if ((unsigned)(last.bits + this.bits) <= bits) break;
				PULLBYTE();
			}
			DROPBITS(last.bits);
		}
		DROPBITS(this.bits);
		if (this.op & 64)
		{
			strm->msg = (char *)"invalid distance code";
			state->mode = BAD;
			break;
		}
		state->offset = (unsigned)this.val;
		state->extra = (unsigned)(this.op) & 15;
		state->mode = DISTEXT;
	case DISTEXT:
		if (state->extra)
		{
			NEEDBITS(state->extra);
			state->offset += BITS(state->extra);
			DROPBITS(state->extra);
		}
		if (state->offset > state->whave + out - left)
		{
			strm->msg = (char *)"invalid distance too far back";
			state->mode = BAD;
			break;
		}
		state->mode = MATCH;
	case MATCH:
		if (left == 0) goto inf_leave;
		copy = out - left;
		if (state->offset > copy)         /* copy from window */
		{
			copy = state->offset - copy;
			if (copy > state->write)
			{
				copy -= state->write;
				from = state->window + (state->wsize - copy);
			}
			else from = state->window + (state->write - copy);
			if (copy > state->length) copy = state->length;
		}
		else                              /* copy from output */
		{
			from = put - state->offset;
			copy = state->length;
		}
		if (copy > left) copy = left;
		left -= copy;
		state->length -= copy;
		do{
		*put++ = *from++;
		}while (--copy);
		if (state->length == 0) state->mode = LEN;
		break;
	case LIT:
		if (left == 0) goto inf_leave;
		*put++ = (unsigned char)(state->length);
		left--;
		state->mode = LEN;
		break;
	case CHECK:
		if (state->wrap)
		{
			NEEDBITS(32);
			out -= left;
			strm->total_out += out;
			state->total += out;
			if (out) strm->adler = state->check =
			UPDATE(state->check, put - out, out);
			out = left;
			if ((REVERSE(hold)) != state->check)
			{
				strm->msg = (char *)"incorrect data check";
				state->mode = BAD;
				break;
			}
			INITBITS();
		}
		state->mode = DONE;
	case DONE:
		ret = Z_STREAM_END;
		goto inf_leave;
	case BAD:
		ret = Z_DATA_ERROR;
		goto inf_leave;
	case MEM:
		return Z_MEM_ERROR;
	case SYNC:
	default: return Z_STREAM_ERROR;
	}

	//Return from inflate(), updating the total counts and the check value.
	//If there was no progress during the inflate() call, return a buffer
	//error.  Call updatewindow() to create and/or update the window state.
	//Note: a memory error from inflate() is non-recoverable.

inf_leave:
	RESTORE();
	if (state->wsize || (state->mode < CHECK && out != strm->avail_out))
	if (updatewindow(strm, out))
	{
		state->mode = MEM;
		return Z_MEM_ERROR;
	}
	in -= strm->avail_in;
	out -= strm->avail_out;
	strm->total_in += in;
	strm->total_out += out;
	state->total += out;
	if (state->wrap && out) strm->adler = state->check = UPDATE(state->check, strm->next_out - out, out);
	strm->data_type = state->bits + (state->last ? 64 : 0) + (state->mode == TYPE ? 128 : 0);
	if (((in == 0 && out == 0) || flush == Z_FINISH) && ret == Z_OK) ret = Z_BUF_ERROR;
	return ret;
}

int inflateEnd(z_streamp strm)
{
	struct inflate_state *state;
	if (strm == NULL || strm->state == NULL || strm->zfree == NULL)
		return Z_STREAM_ERROR;
	state = (struct inflate_state *)strm->state;
	if (state->window != NULL) ZipFree(strm, (byte*)state->window);
	ZipFree(strm, (byte*)strm->state);
	strm->state = NULL;
	return Z_OK;
}

int inflateReset(z_streamp strm)
{
	struct inflate_state *state;

	if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;
	state = (struct inflate_state *)strm->state;
	strm->total_in = strm->total_out = state->total = 0;
	strm->msg = (char *)"Ok";	// no errors
	strm->adler = 1;		// to support ill-conceived Java test suite
	state->mode = HEAD;
	state->last = 0;
	state->havedict = 0;
	state->dmax = 32768U;
	state->head = NULL;
	state->wsize = 0;
	state->whave = 0;
	state->write = 0;
	state->hold = 0;
	state->bits = 0;
	state->lencode = state->distcode = state->next = state->codes;
	return Z_OK;
}

//=======================================================================
//			DEFALTE STUFF
//=======================================================================
#define CLEAR_HASH(s) s->head[s->hash_size-1] = 0; memset((byte *)s->head, 0, (uint)(s->hash_size - 1) * sizeof(*s->head));
#define UPDATE_HASH(s,h,c) (h = (((h)<<s->hash_shift) ^ (c)) & s->hash_mask)
#define INSERT_STRING(s, str, match_head) (UPDATE_HASH(s, s->ins_h, s->window[(str) + (3-1)]), s->prev[(str) & s->w_mask] = match_head = s->head[s->ins_h], s->head[s->ins_h] = (word)(str))

typedef struct config_s
{
	word	good_length;	// reduce lazy search above this match length
	word	max_lazy;		// do not perform lazy search above this match length
	word	nice_length;	// quit search above this match length
	word	max_chain;
	compress_func  func;
} config;

/*
=========================================================================
Flush as much pending output as possible. All deflate() output goes
through this function so some applications may wish to modify it
to avoid allocating a large strm->next_out buffer and copying into it.
(See also read_buf()).
=========================================================================
*/
void flush_pending(z_streamp strm)
{
	uint	len = strm->state->pending;

	if (len > strm->avail_out) len = strm->avail_out;
	if (len == 0) return;

	Mem_Copy(strm->next_out, strm->state->pending_out, len);
	strm->next_out  += len;
	strm->state->pending_out  += len;
	strm->total_out += len;
	strm->avail_out  -= len;
	strm->state->pending -= len;

	if (strm->state->pending == 0) strm->state->pending_out = strm->state->pending_buf;
}

/*
===========================================================================
Read a new buffer from the current input stream, update the adler32
and total number of bytes read.  All deflate() input goes through
this function so some applications may wish to modify it to avoid
allocating a large strm->next_in buffer and copying from it.
(See also flush_pending()).
===========================================================================
*/
int read_buf(z_streamp strm, byte *buf, uint size)
{
	uint	len = strm->avail_in;

	if(len > size) len = size;
	if(len == 0) return 0;

	strm->avail_in  -= len;

	if (!strm->state->noheader)
	{
		strm->adler = adler32(strm->adler, strm->next_in, len);
	}
	Mem_Copy(buf, strm->next_in, len);
	strm->next_in  += len;
	strm->total_in += len;

	return (int)len;
}

/* 
===========================================================================
Set match_start to the longest match starting at the given string and
return its length. Matches shorter or equal to prev_length are discarded,
in which case the result is equal to prev_length and match_start is
garbage.
IN assertions: cur_match is the head of the hash chain for the current
string (strstart) and its distance is <= MAX_DIST, and prev_length >= 1
OUT assertion: the match length is not greater than s->lookahead.
===========================================================================
*/
uint longest_match(deflate_state *s, uint cur_match)
{
	uint		chain_length = s->max_chain_length; // max hash chain length
	register byte	*scan = s->window + s->strstart; // current string
	register byte	*match; // matched string
	register int	len; // length of current match
	int		best_len = s->prev_length; // best match length so far
	int		nice_match = s->nice_match; // stop if match long enough
	uint		limit = s->strstart > (uint)MAX_DIST(s) ? s->strstart - (uint)MAX_DIST(s) : 0;

	// Stop when cur_match becomes <= limit. To simplify the code,
	// we prevent matches with the string of window index 0.
	word		*prev = s->prev;
	uint		wmask = s->w_mask;
	register byte	*strend = s->window + s->strstart + 258;
	register byte	scan_end1 = scan[best_len-1];
	register byte	scan_end = scan[best_len];

	// The code is optimized for HASH_BITS >= 8 and MAX_MATCH-2 multiple of 16.
	// It is easy to get rid of this optimization if necessary.

	// Do not waste too much time if we already have a good match:
	if (s->prev_length >= s->good_match)
	{
		chain_length >>= 2;
	}

	// Do not look for matches beyond the end of the input. This is necessary
	// to make deflate deterministic.
	if ((uint)nice_match > s->lookahead) nice_match = s->lookahead;

	do
	{
		match = s->window + cur_match;
		// Skip to next match if the match length cannot increase
		// or if the match length is less than 2:

		if(match[best_len] != scan_end || match[best_len-1] != scan_end1 || *match != *scan || *++match != scan[1])
			continue;

		// The check at best_len-1 can be removed because it will be made
		// again later. (This heuristic is not always a win.)
		// It is not necessary to compare scan[2] and match[2] since they
		// are always equal when the other bytes match, given that
		// the hash keys are equal and that HASH_BITS >= 8.
		scan += 2, match++;

		// We check for insufficient lookahead only every 8th comparison;
		// the 256th check will be made at strstart+258.
		while (*++scan == *++match && *++scan == *++match && *++scan == *++match && *++scan == *++match && *++scan == *++match && *++scan == *++match && *++scan == *++match && *++scan == *++match && scan < strend);
		len = 258 - (int)(strend - scan);
		scan = strend - 258;

		if (len > best_len)
		{
			s->match_start = cur_match;
			best_len = len;
			if (len >= nice_match) break;
			scan_end1  = scan[best_len-1];
			scan_end   = scan[best_len];
		}
	} while ((cur_match = prev[cur_match & wmask]) > limit && --chain_length != 0);

	if ((uint)best_len <= s->lookahead) return (uint)best_len;
	return s->lookahead;
}

/*
===========================================================================
Fill the window when the lookahead becomes insufficient.
Updates strstart and lookahead.

IN assertion: lookahead < MIN_LOOKAHEAD
OUT assertions: strstart <= window_size-MIN_LOOKAHEAD
At least one byte has been read, or avail_in == 0; reads are
performed for at least two bytes (required for the zip translate_eol
option -- not supported here).
===========================================================================
*/

void fill_window(deflate_state *s)
{
	register uint	n, m;
	register word	*p;
	uint		more; // Amount of free space at the end of the window.
	uint		wsize = s->w_size;

	do
	{
		more = (uint)(s->window_size -(dword)s->lookahead -(dword)s->strstart);

		// Deal with !@#$% 64K limit:
		if (more == 0 && s->strstart == 0 && s->lookahead == 0)
		{
			more = wsize;
		}
		else if (more == (uint)(-1))
		{
			// Very unlikely, but possible on 16 bit machine if strstart == 0
			// and lookahead == 1 (input done one byte at time)
			more--;
			// If the window is almost full and there is insufficient lookahead,
			// move the upper half to the lower one to make room in the upper half.
		}
		else if (s->strstart >= wsize + MAX_DIST(s))
		{
			Mem_Copy(s->window, s->window+wsize, (uint)wsize);
			s->match_start -= wsize;
			s->strstart -= wsize; // we now have strstart >= MAX_DIST
			s->block_start -= (long) wsize;

			// Slide the hash table (could be avoided with 32 bit values
			// at the expense of memory usage). We slide even when level == 0
			// to keep the hash table consistent if we switch back to level > 0
			// later. (Using level 0 permanently is not an optimal usage of
			// zlib, so we don't care about this pathological case.)
			n = s->hash_size;
			p = &s->head[n];

			do
			{
				m = *--p;
				*p = (word)(m >= wsize ? m - wsize : 0);
			} while (--n);
			n = wsize;
			p = &s->prev[n];

			do
			{
				m = *--p;
				*p = (word)(m >= wsize ? m-wsize : 0);

				// If n is not on any hash chain, prev[n] is garbage but
				// its value will never be used.
			} while (--n);
			more += wsize;
		}
		if (s->strm->avail_in == 0) return;

		n = read_buf(s->strm, s->window + s->strstart + s->lookahead, more);
		s->lookahead += n;

		// Initialize the hash value now that we have some input:
		if (s->lookahead >= 3)
		{
			s->ins_h = s->window[s->strstart];
			UPDATE_HASH(s, s->ins_h, s->window[s->strstart+1]);
		}
	} while (s->lookahead < MIN_LOOKAHEAD && s->strm->avail_in != 0);
}

//=================
// Huffman trees
//=================
struct static_tree_desc_s
{
	const ct_data	*static_tree;	// static tree or NULL
	const int		*extra_bits;	// extra bits for each code or NULL
	int		extra_base;	// base index for extra_bits
	int		elems;		// max number of elements in the tree
	int		max_length;	// max bit length for the codes
};

const int extra_lbits[LENGTH_CODES] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
const int extra_dbits[D_CODES] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
const int extra_blbits[BL_CODES] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,3,7};
const byte bl_order[BL_CODES] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

const byte _length_code[258-3+1] =
{
 0,  1,  2,  3,  4,  5,  6,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12, 12, 12, 12,
13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16,
17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19,
19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22,
22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23,
23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 26, 26, 26, 26, 26, 26, 26, 26,
26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 28
};

const byte _dist_code[512] = 
{
 0,  1,  2,  3,  4,  4,  5,  5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  8,
 8,  8,  8,  8,  9,  9,  9,  9,  9,  9,  9,  9, 10, 10, 10, 10, 10, 10, 10, 10,
10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13,
13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15,
15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,  0,  0, 16, 17,
18, 18, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22,
23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27,
27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
27, 27, 27, 27, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29
};

const int base_length[LENGTH_CODES] = 
{
0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, 56,
64, 80, 96, 112, 128, 160, 192, 224, 0
};

const int base_dist[D_CODES] = 
{
    0,     1,     2,     3,     4,     6,     8,    12,    16,    24,
   32,    48,    64,    96,   128,   192,   256,   384,   512,   768,
 1024,  1536,  2048,  3072,  4096,  6144,  8192, 12288, 16384, 24576
};

ct_data static_ltree[L_CODES+2];
ct_data static_dtree[D_CODES];

static_tree_desc static_l_desc = { static_ltree, extra_lbits, LITERALS+1, L_CODES, MAX_WBITS };
static_tree_desc static_d_desc = { static_dtree, extra_dbits, 0, D_CODES, MAX_WBITS};
static_tree_desc static_bl_desc ={(const ct_data *)0, extra_blbits, 0, BL_CODES, 7 };

#define Freq fc.freq
#define Code fc.code
#define Dad  dl.dad
#define Len  dl.len
#define Buf_size (8 * 2 * sizeof(char))
#define d_code(dist) ((dist) < 256 ? _dist_code[dist] : _dist_code[256+((dist)>>7)])

#define _tr_tally_lit(s, c, flush) \
	{ byte cc = (c); \
	s->d_buf[s->last_lit] = 0; \
	s->l_buf[s->last_lit++] = cc; \
	s->dyn_ltree[cc].Freq++; \
	flush = (s->last_lit == s->lit_bufsize-1); \
	}

#define _tr_tally_dist(s, distance, length, flush) \
	{ byte len = (length); \
	word dist = (distance); \
	s->d_buf[s->last_lit] = dist; \
	s->l_buf[s->last_lit++] = len; \
	dist--; \
	s->dyn_ltree[_length_code[len]+LITERALS+1].Freq++; \
	s->dyn_dtree[d_code(dist)].Freq++; \
	flush = (s->last_lit == s->lit_bufsize-1); \
	}

#define send_bits(s, value, length) \
	{ int len = length; \
	if (s->bi_valid > (int)Buf_size - len) \
	{ int val = value; \
	s->bi_buf |= (val << s->bi_valid); \
	put_short(s, s->bi_buf); \
	s->bi_buf = (word)val >> (Buf_size - s->bi_valid); \
	s->bi_valid += len - Buf_size; \
	} else { \
	s->bi_buf |= (value) << s->bi_valid; \
	s->bi_valid += len; \
	} \
}

#define send_code(s, c, tree) send_bits(s, tree[c].Code, tree[c].Len)

// Flush the bit buffer and align the output on a byte boundary
void bi_windup(deflate_state *s)
{
	if (s->bi_valid > 8)
	{
		put_short(s, s->bi_buf);
	}
	else if (s->bi_valid > 0)
	{
		put_byte(s, (byte)s->bi_buf);
	}
	s->bi_buf = 0;
	s->bi_valid = 0;
}

// Flush the bit buffer, keeping at most 7 bits in it.
void bi_flush(deflate_state *s)
{
	if (s->bi_valid == 16)
	{
		put_short(s, s->bi_buf);
		s->bi_buf = 0;
		s->bi_valid = 0;
	}
	else if (s->bi_valid >= 8)
	{
		put_byte(s, (byte)s->bi_buf);
		s->bi_buf >>= 8;
		s->bi_valid -= 8;
	}
}

/*
===========================================================================
Reverse the first len bits of a code, using straightforward code (a faster
method would use a table)
IN assertion: 1 <= len <= 15
===========================================================================
*/
uint bi_reverse(uint code, int len)
{
	register uint res = 0;

	do
	{
		res |= code & 1;
		code >>= 1, res <<= 1;
	} while (--len > 0);

	return res >> 1;
}

// Initialize a new block.
void init_block(deflate_state *s)
{
	int	n; // iterates over tree elements

	// Initialize the trees.
	for (n = 0; n < L_CODES; n++) s->dyn_ltree[n].Freq = 0;
	for (n = 0; n < D_CODES; n++) s->dyn_dtree[n].Freq = 0;
	for (n = 0; n < BL_CODES; n++) s->bl_tree[n].Freq = 0;

	s->dyn_ltree[256].Freq = 1;
	s->opt_len = s->static_len = 0L;
	s->last_lit = s->matches = 0;
}

// Copy a stored block, storing first the length and its
// one's complement if requested.
void copy_block(deflate_state *s, char *buf, uint len, int header)
{
	bi_windup(s);        /* align on byte boundary */
	s->last_eob_len = 8; /* enough lookahead for inflate */

	if (header)
	{
		put_short(s, (word)len);   
		put_short(s, (word)~len);
	}
	while (len--){ put_byte(s, *buf++); }
}


// Initialize the various 'constant' tables.
void _tr_init(deflate_state *s)
{
	s->l_desc.dyn_tree = s->dyn_ltree;
	s->l_desc.stat_desc = &static_l_desc;
	s->d_desc.dyn_tree = s->dyn_dtree;
	s->d_desc.stat_desc = &static_d_desc;

	s->bl_desc.dyn_tree = s->bl_tree;
	s->bl_desc.stat_desc = &static_bl_desc;

	s->bi_buf = 0;
	s->bi_valid = 0;
	s->last_eob_len = 8; /* enough lookahead for inflate */

	// Initialize the first block of the first file:
	init_block(s);
}

/* 
===========================================================================
Send a literal or distance tree in compressed form, using the codes in
bl_tree.
===========================================================================
*/
void send_tree(deflate_state *s, ct_data *tree,int max_code)
{
	int	n;                     // iterates over all tree elements
	int	prevlen = -1;          // last emitted length
	int	curlen;                // length of current code
	int	nextlen = tree[0].Len; // length of next code
	int	count = 0;             // repeat count of the current code
	int	max_count = 7;         // max repeat count
	int	min_count = 4;         // min repeat count

	if (nextlen == 0) max_count = 138, min_count = 3;

	for (n = 0; n <= max_code; n++)
	{
		curlen = nextlen; nextlen = tree[n+1].Len;
		if (++count < max_count && curlen == nextlen)
			continue;
		else if (count < min_count)
		{
			do { send_code(s, curlen, s->bl_tree); } while (--count != 0);
		}
		else if (curlen != 0)
		{
			if (curlen != prevlen)
			{
				send_code(s, curlen, s->bl_tree);
				count--;
			}
			send_code(s, 16, s->bl_tree); send_bits(s, count-3, 2);
		}
		else if (count <= 10)
		{
			send_code(s, 17, s->bl_tree); send_bits(s, count-3, 3);
		}
		else
		{
			send_code(s, 18, s->bl_tree); send_bits(s, count-11, 7);
		}
		count = 0;
		prevlen = curlen;

		if (nextlen == 0)
		{
			max_count = 138, min_count = 3;
		}
		else if (curlen == nextlen)
		{
			max_count = 6, min_count = 3;
		}
		else
		{
			max_count = 7, min_count = 4;
		}
	}
}


/* 
===========================================================================
Send the header for a block using dynamic Huffman trees: the counts, the
lengths of the bit length codes, the literal tree and the distance tree.
IN assertion: lcodes >= 257, dcodes >= 1, blcodes >= 4.
===========================================================================
*/
void send_all_trees(deflate_state *s, int lcodes, int dcodes, int blcodes)
{
	int		rank;// index in bl_order

	send_bits(s, lcodes-257, 5); // not +255 as stated in appnote.txt
	send_bits(s, dcodes-1,   5);
	send_bits(s, blcodes-4,  4); // not -3 as stated in appnote.txt

	for (rank = 0; rank < blcodes; rank++)
	{
		send_bits(s, s->bl_tree[bl_order[rank]].Len, 3);
	}
	send_tree(s, (ct_data *)s->dyn_ltree, lcodes-1); /* literal tree */
	send_tree(s, (ct_data *)s->dyn_dtree, dcodes-1); /* distance tree */
}

void _tr_stored_block(deflate_state *s, char *buf, dword stored_len, int eof)
{
	send_bits(s, (STORED_BLOCK<<1)+eof, 3);  // send block type
	copy_block(s, buf, (uint)stored_len, 1); // with header
}

void _tr_align(deflate_state *s)
{
	send_bits(s, STATIC_TREES<<1, 3);
	send_code(s, 256, static_ltree);
	bi_flush(s);
	if (1 + s->last_eob_len + 10 - s->bi_valid < 9)
	{
		send_bits(s, STATIC_TREES<<1, 3);
		send_code(s, 256, static_ltree);
		bi_flush(s);
	}
	s->last_eob_len = 7;
}

/*
===========================================================================
Remove the smallest element from the heap and recreate the heap with
one less element. Updates heap and heap_len.
===========================================================================
*/
#define pqremove(s, tree, top) \
	{ top = s->heap[1]; \
	s->heap[1] = s->heap[s->heap_len--]; \
	pqdownheap(s, tree, 1); \
	}

/* 
===========================================================================
Compares to subtrees, using the tree depth as tie breaker when
the subtrees have equal frequency. This minimizes the worst case length.
===========================================================================
*/
#define smaller(tree, n, m, depth) (tree[n].Freq < tree[m].Freq || (tree[n].Freq == tree[m].Freq && depth[n] <= depth[m]))

/* 
===========================================================================
Restore the heap property by moving down the tree starting at node k,
exchanging a node with the smallest of its two sons if necessary, stopping
when the heap property is re-established (each father smaller than its
two sons).
===========================================================================
*/
void pqdownheap(deflate_state *s, ct_data *tree, int k)
{
	int	v = s->heap[k];
	int	j = k << 1; // left son of k

	while (j <= s->heap_len)
	{
		// Set j to the smallest of the two sons:
		if (j < s->heap_len && smaller(tree, s->heap[j+1], s->heap[j], s->depth)) j++;

		// Exit if v is smaller than both sons
		if (smaller(tree, v, s->heap[j], s->depth)) break;

		// Exchange v with the smallest son
		s->heap[k] = s->heap[j];
		k = j;

		// And continue down the tree, setting j to the left son of k
		j <<= 1;
	}
	s->heap[k] = v;
}

/*
===========================================================================
Compute the optimal bit lengths for a tree and update the total bit length
for the current block.
IN assertion: the fields freq and dad are set, heap[heap_max] and
above are the tree nodes sorted by increasing frequency.
OUT assertions: the field len is set to the optimal bit length, the
array bl_count contains the frequencies for each bit length.
The length opt_len is updated; static_len is also updated if stree is
not null.
===========================================================================
*/
void gen_bitlen(deflate_state *s, tree_desc *desc)
{
	ct_data		*tree = desc->dyn_tree;
	int		max_code = desc->max_code;
	const ct_data	*stree = desc->stat_desc->static_tree;
	const int		*extra = desc->stat_desc->extra_bits;
	int		base = desc->stat_desc->extra_base;
	int		max_length = desc->stat_desc->max_length;
	int		h;	// heap index
	int		n, m;	// iterate over the tree elements
	int		bits;	// bit length
	int		xbits;	// extra bits
	word		f;	// frequency
	int		overflow = 0; // number of elements with bit length too large

	for (bits = 0; bits <= MAX_WBITS; bits++) s->bl_count[bits] = 0;

	// In a first pass, compute the optimal bit lengths (which may
	// overflow in the case of the bit length tree).
	tree[s->heap[s->heap_max]].Len = 0; // root of the heap

	for (h = s->heap_max+1; h < HEAP_SIZE; h++)
	{
		n = s->heap[h];
		bits = tree[tree[n].Dad].Len + 1;
		if (bits > max_length) bits = max_length, overflow++;
		tree[n].Len = (word)bits;

		// We overwrite tree[n].Dad which is no longer needed
		if (n > max_code) continue; // not a leaf node

		s->bl_count[bits]++;
		xbits = 0;
		if (n >= base) xbits = extra[n-base];
		f = tree[n].Freq;
		s->opt_len += (dword)f * (bits + xbits);
		if (stree) s->static_len += (dword)f * (stree[n].Len + xbits);
	}
	if (overflow == 0) return;

	// Find the first bit length which could increase:
	do
	{
		bits = max_length-1;
		while (s->bl_count[bits] == 0) bits--;
		s->bl_count[bits]--;	// move one leaf down the tree
		s->bl_count[bits+1] += 2;	// move one overflow item as its brother
		s->bl_count[max_length]--;

		// The brother of the overflow item also moves one step up,
		// but this does not affect bl_count[max_length]
		overflow -= 2;
	} while (overflow > 0);

	// Now recompute all bit lengths, scanning in increasing frequency.
	// h is still equal to HEAP_SIZE. (It is simpler to reconstruct all
	// lengths instead of fixing only the wrong ones. This idea is taken
	// from 'ar' written by Haruhiko Okumura.)

	for (bits = max_length; bits != 0; bits--)
	{
		n = s->bl_count[bits];
		while (n != 0)
		{
			m = s->heap[--h];
			if (m > max_code) continue;
			if (tree[m].Len != (uint) bits)
			{
				s->opt_len += ((long)bits - (long)tree[m].Len) * (long)tree[m].Freq;
				tree[m].Len = (word)bits;
			}
			n--;
		}
	}
}

/*
===========================================================================
Generate the codes for a given tree and bit counts (which need not be
optimal).
IN assertion: the array bl_count contains the bit length statistics for
the given tree and the field len is set for all tree elements.
OUT assertion: the field code is set for all tree elements of non
zero code length.
===========================================================================
*/

void gen_codes (ct_data *tree, int max_code, word *bl_count)
{
	word		next_code[MAX_WBITS+1];	// next code value for each bit length
	word		code = 0;			// running code value
	int		bits;			// bit index
	int		n;			// code index

	// The distribution counts are first used to generate the code values
	// without bit reversal.
	for (bits = 1; bits <= MAX_WBITS; bits++)
	{
		next_code[bits] = code = (code + bl_count[bits-1]) << 1;
	}

	// Check that the bit counts in bl_count are consistent. The last code
	// must be all ones.
	for (n = 0;  n <= max_code; n++)
	{
		int len = tree[n].Len;

		if (len == 0) continue;
		// Now reverse the bits
		tree[n].Code = bi_reverse(next_code[len]++, len);
	}
}

/*
===========================================================================
Construct one Huffman tree and assigns the code bit strings and lengths.
Update the total bit length for the current block.
IN assertion: the field freq is set for all tree elements.
OUT assertions: the fields len and code are set to the optimal bit length
and corresponding code. The length opt_len is updated; static_len is
also updated if stree is not null. The field max_code is set.
===========================================================================
*/
void build_tree(deflate_state *s, tree_desc *desc)
{
	ct_data		*tree = desc->dyn_tree;
	const ct_data	*stree = desc->stat_desc->static_tree;
	int		elems = desc->stat_desc->elems;
	int		n, m; // iterate over heap elements
	int		max_code = -1; // largest code with non zero frequency
	int		node; // new node being created

	// Construct the initial heap, with least frequent element in
	// heap[SMALLEST]. The sons of heap[n] are heap[2*n] and heap[2*n+1].
	// heap[0] is not used.
	s->heap_len = 0, s->heap_max = HEAP_SIZE;

	for (n = 0; n < elems; n++)
	{
		if (tree[n].Freq != 0)
		{
			s->heap[++(s->heap_len)] = max_code = n;
			s->depth[n] = 0;
		}
		else tree[n].Len = 0;
	}

	// The pkzip format requires that at least one distance code exists,
	// and that at least one bit should be sent even if there is only one
	// possible code. So to avoid special checks later on we force at least
	// two codes of non zero frequency.

	while (s->heap_len < 2)
	{
		node = s->heap[++(s->heap_len)] = (max_code < 2 ? ++max_code : 0);
		tree[node].Freq = 1;
		s->depth[node] = 0;
		s->opt_len--; if (stree) s->static_len -= stree[node].Len;
		// node is 0 or 1 so it does not have extra bits
	}
	desc->max_code = max_code;

	// The elements heap[heap_len/2+1 .. heap_len] are leaves of the tree,
	// establish sub-heaps of increasing lengths:

	for (n = s->heap_len/2; n >= 1; n--) pqdownheap(s, tree, n);

	// Construct the Huffman tree by repeatedly combining the least two
	// frequent nodes.

	node = elems; // next internal node of the tree

	do
	{
		pqremove(s, tree, n);  // n = node of least frequency
		m = s->heap[1]; // m = node of next least frequency

		s->heap[--(s->heap_max)] = n; // keep the nodes sorted by frequency
		s->heap[--(s->heap_max)] = m;

		// Create a new node father of n and m
		tree[node].Freq = tree[n].Freq + tree[m].Freq;
		s->depth[node] = (byte) (max(s->depth[n], s->depth[m]) + 1);
		tree[n].Dad = tree[m].Dad = (word)node;
		// and insert the new node in the heap
		s->heap[1] = node++;
		pqdownheap(s, tree, 1);
	} while (s->heap_len >= 2);

	s->heap[--(s->heap_max)] = s->heap[1];

	// At this point, the fields freq and dad are set. We can now
	// generate the bit lengths.
	gen_bitlen(s, (tree_desc *)desc);

	//The field len is now set, we can generate the bit codes
	gen_codes ((ct_data *)tree, max_code, s->bl_count);
}

/*
===========================================================================
Scan a literal or distance tree to determine the frequencies of the codes
in the bit length tree.
===========================================================================
*/
void scan_tree (deflate_state *s, ct_data *tree, int max_code)
{
	int	n;                     // iterates over all tree elements
	int	prevlen = -1;          // last emitted length
	int	curlen;                // length of current code
	int	nextlen = tree[0].Len; // length of next code
	int	count = 0;             // repeat count of the current code
	int	max_count = 7;         // max repeat count
	int	min_count = 4;         // min repeat count

	if (nextlen == 0) max_count = 138, min_count = 3;
	tree[max_code+1].Len = (word)0xffff; // guard

	for (n = 0; n <= max_code; n++)
	{
		curlen = nextlen; nextlen = tree[n+1].Len;
		if (++count < max_count && curlen == nextlen)
			continue;
		else if (count < min_count)
		{
			s->bl_tree[curlen].Freq += count;
		}
		else if (curlen != 0)
		{
			if (curlen != prevlen) 
				s->bl_tree[curlen].Freq++;
			s->bl_tree[16].Freq++;
		}
		else if (count <= 10)
		{
			s->bl_tree[17].Freq++;
		}
		else
		{
			s->bl_tree[18].Freq++;
		}
		count = 0;
		prevlen = curlen;

		if (nextlen == 0)
		{
			max_count = 138, min_count = 3;
		}
		else if (curlen == nextlen)
		{
			max_count = 6, min_count = 3;
		}
		else
		{
			max_count = 7, min_count = 4;
		}
	}
}

/*
===========================================================================
Construct the Huffman tree for the bit lengths and return the index in
bl_order of the last bit length code to send.
===========================================================================
*/
int build_bl_tree(deflate_state *s)
{
	int	max_blindex; // index of last bit length code of non zero freq

	// Determine the bit length frequencies for literal and distance trees
	scan_tree(s, (ct_data *)s->dyn_ltree, s->l_desc.max_code);
	scan_tree(s, (ct_data *)s->dyn_dtree, s->d_desc.max_code);

	// Build the bit length tree:
	build_tree(s, (tree_desc *)(&(s->bl_desc)));
	for (max_blindex = BL_CODES-1; max_blindex >= 3; max_blindex--)
	{
		if (s->bl_tree[bl_order[max_blindex]].Len != 0) break;
	}

	// Update opt_len to include the bit length tree and counts
	s->opt_len += 3*(max_blindex+1) + 5+5+4;

	return max_blindex;
}

/*
===========================================================================
Set the data type to ASCII or BINARY, using a crude approximation:
binary if more than 20% of the bytes are <= 6 or >= 128, ascii otherwise.
IN assertion: the fields freq of dyn_ltree are set and the total of all
frequencies does not exceed 64K (to fit in an int on 16 bit machines).
===========================================================================
*/
void set_data_type(deflate_state *s)
{
	int	n = 0;
	uint	ascii_freq = 0;
	uint	bin_freq = 0;

	while (n < 7) bin_freq += s->dyn_ltree[n++].Freq;
	while (n < 128) ascii_freq += s->dyn_ltree[n++].Freq;
	while (n < LITERALS) bin_freq += s->dyn_ltree[n++].Freq;
	s->data_type = (byte)(bin_freq > (ascii_freq >> 2) ? Z_BINARY : Z_ASCII);
}

/*
===========================================================================
Send the block data compressed using the given Huffman trees
===========================================================================
*/
void compress_block(deflate_state *s, ct_data *ltree, ct_data *dtree)
{
	uint		dist;	// distance of matched string
	int		lc;	// match length or unmatched char (if dist == 0)
	uint		lx = 0;	// running index in l_buf
	uint		code;	// the code to send
	int		extra;	// number of extra bits to send

	if (s->last_lit != 0)
	{ 
		do
		{
			dist = s->d_buf[lx];
			lc = s->l_buf[lx++];
			if (dist == 0)
			{
				send_code(s, lc, ltree); // send a literal byte
			}
			else
			{
				// Here, lc is the match length - MIN_MATCH
				code = _length_code[lc];
				send_code(s, code+LITERALS+1, ltree); // send the length code
				extra = extra_lbits[code];
				if (extra != 0)
				{
					lc -= base_length[code];
					send_bits(s, lc, extra); // send the extra length bits
				}

				dist--;			// dist is now the match distance - 1
				code = d_code(dist);
				send_code(s, code, dtree);	// send the distance code
				extra = extra_dbits[code];

				if (extra != 0)
				{
					dist -= base_dist[code];
					send_bits(s, dist, extra); // send the extra distance bits
				}
			} // literal or match pair ?

			// Check that the overlay between pending_buf and d_buf+l_buf is ok:
		} while (lx < s->last_lit);
	}

	send_code(s, 256, ltree);
	s->last_eob_len = ltree[256].Len;
}

/*
===========================================================================
Determine the best encoding for the current block: dynamic trees, static
trees or store, and output the encoded block to the zip file.
===========================================================================
*/
void _tr_flush_block(deflate_state *s, char *buf, dword stored_len, int eof)
{
	dword	opt_lenb, static_lenb;	// opt_len and static_len in bytes
	int	max_blindex = 0;		// index of last bit length code of non zero freq

	// Build the Huffman trees unless a stored block is forced
	if (s->level > 0)
	{
		// Check if the file is ascii or binary
		if (s->data_type == Z_UNKNOWN) set_data_type(s);

		// Construct the literal and distance trees
		build_tree(s, (tree_desc *)(&(s->l_desc)));
		build_tree(s, (tree_desc *)(&(s->d_desc)));
		max_blindex = build_bl_tree(s);
		opt_lenb = (s->opt_len+3+7)>>3;
		static_lenb = (s->static_len+3+7)>>3;

		if (static_lenb <= opt_lenb) opt_lenb = static_lenb;
	}
	else
	{
		opt_lenb = static_lenb = stored_len + 5; /* force a stored block */
	}

	if (stored_len+4 <= opt_lenb && buf != NULL)
	{
		_tr_stored_block(s, buf, stored_len, eof);
	}
	else if (static_lenb == opt_lenb)
	{
		send_bits(s, (STATIC_TREES<<1)+eof, 3);
		compress_block(s, (ct_data *)static_ltree, (ct_data *)static_dtree);
	}
	else
	{
		send_bits(s, (DYN_TREES<<1)+eof, 3);
		send_all_trees(s, s->l_desc.max_code+1, s->d_desc.max_code+1, max_blindex+1);
		compress_block(s, (ct_data *)s->dyn_ltree, (ct_data *)s->dyn_dtree);
	}

	init_block(s);

	if (eof)
	{
		bi_windup(s);
	}
}

int deflateInit_(z_streamp strm, int level, const char *version, int stream_size)
{
	return deflateInit2_(strm, level, MAX_WBITS, version, stream_size);
}

/* ========================================================================= */
int deflateInit2_(z_streamp strm, int level, int windowBits, const char *version, int stream_size)
{
	deflate_state *s;
	int noheader = 0;
	word	*overlay;

	// defalte settings
	int	method = Z_DEFLATED;
	int	memLevel = DEF_MEM_LEVEL;
	int	strategy = Z_DEFAULT_STRATEGY;

	if (version == NULL || version[0] != ZLIB_VERSION[0] || stream_size != sizeof(z_stream))
		return Z_VERSION_ERROR;

	if (strm == NULL) return Z_STREAM_ERROR;
	strm->msg = NULL;

	if(strm->zalloc == NULL)
	{
		strm->zalloc = zcalloc;
		strm->opaque = NULL;
	}
	if(strm->zfree == NULL) strm->zfree = zcfree;

	if (level == Z_DEFAULT_COMPRESSION) level = 6;

	if (windowBits < 0)
	{
		// undocumented feature: suppress zlib header
		noheader = 1;
		windowBits = -windowBits;
	}

	// bound some parms
	level = bound( 0, level, 9 );
	windowBits = bound( 8, windowBits, 15 );
	strategy = bound( 0, strategy, Z_HUFFMAN_ONLY );
	
	if (method != Z_DEFLATED) return Z_STREAM_ERROR;

	s = (deflate_state *) ZipAlloc(strm, 1, sizeof(deflate_state));
	if (s ==NULL) return Z_MEM_ERROR;
	strm->state = (struct int_state *)s;
	s->strm = strm;

	s->noheader = noheader;
	s->w_bits = windowBits;
	s->w_size = 1 << s->w_bits;
	s->w_mask = s->w_size - 1;

	s->hash_bits = memLevel + 7;
	s->hash_size = 1 << s->hash_bits;
	s->hash_mask = s->hash_size - 1;
	s->hash_shift =  ((s->hash_bits + 3 - 1)/3); // align 3

	s->window = (byte *) ZipAlloc(strm, s->w_size, 2 * sizeof(byte));
	s->prev = (word *)ZipAlloc(strm, s->w_size, sizeof(word));
	s->head = (word *)ZipAlloc(strm, s->hash_size, sizeof(word));

	s->lit_bufsize = 1 << (memLevel + 6); // 16K elements by default

	overlay = (word *)ZipAlloc(strm, s->lit_bufsize, sizeof(word) + 2);
	s->pending_buf = (byte *)overlay;
	s->pending_buf_size = (dword)s->lit_bufsize * (sizeof(word) + 2L);

	if (s->window == NULL || s->prev == NULL || s->head == NULL || s->pending_buf == NULL)
	{
		strm->msg = (char*)"insufficient memory";
		deflateEnd (strm);
		return Z_MEM_ERROR;
	}

	s->d_buf = overlay + s->lit_bufsize/sizeof(word);
	s->l_buf = s->pending_buf + (1+sizeof(word))*s->lit_bufsize;

	s->level = level;
	s->strategy = strategy;
	s->method = (byte)method;

	return deflateReset(strm);
}

/* ===========================================================================
 * Flush the current block, with given end-of-file flag.
 * IN assertion: strstart is set to the end of the current match.
 */
#define FLUSH_BLOCK_ONLY(s, eof) \
	{ \
		_tr_flush_block(s, (s->block_start >= 0L ? \
		(char *)&s->window[(uint)s->block_start] : \
		(char *)NULL), \
		(dword)((long)s->strstart - s->block_start), \
		(eof)); \
		s->block_start = s->strstart; \
		flush_pending(s->strm); \
	} \

/* Same but force premature exit if necessary. */
#define FLUSH_BLOCK(s, eof) \
	{ \
	FLUSH_BLOCK_ONLY(s, eof); \
	if (s->strm->avail_out == 0) return (eof) ? finish_started : need_more; \
	} \

block_state deflate_copy(deflate_state *s, int flush)
{
	/* Stored blocks are limited to 0xffff bytes, pending_buf is limited
	* to pending_buf_size, and each stored block has a 5 byte header:
	*/
	dword	max_block_size = 0xffff;
	dword	max_start;

	if (max_block_size > s->pending_buf_size - 5)
	{
		max_block_size = s->pending_buf_size - 5;
	}

	// Copy as much as possible from input to output:
	while ( 1 )
	{
		// Fill the window as much as possible:
		if (s->lookahead <= 1)
		{
			fill_window(s);
			if (s->lookahead == 0 && flush == Z_NO_FLUSH) return need_more;
			if (s->lookahead == 0) break; // flush the current block
		}
		s->strstart += s->lookahead;
		s->lookahead = 0;

		// Emit a stored block if pending_buf will be full:
		max_start = s->block_start + max_block_size;
		if (s->strstart == 0 || (dword)s->strstart >= max_start)
		{
			// strstart == 0 is possible when wraparound on 16-bit machine
			s->lookahead = (uint)(s->strstart - max_start);
			s->strstart = (uint)max_start;
			FLUSH_BLOCK(s, 0);
		}
		// Flush if we may have to slide, otherwise block_start may become
		// negative and the data will be gone:
		if (s->strstart - (uint)s->block_start >= MAX_DIST(s))
		{
			FLUSH_BLOCK(s, 0);
		}
	}

	FLUSH_BLOCK(s, flush == Z_FINISH);
	return flush == Z_FINISH ? finish_done : block_done;
}

/*
===========================================================================
 Compress as much as possible from the input stream, return the current
 block state.
 This function does not perform lazy evaluation of matches and inserts
 new strings in the dictionary only for unmatched strings or for short
 matches. It is used only for the fast compression options.
===========================================================================
*/
block_state deflate_fast(deflate_state *s, int flush)
{
	uint	hash_head = 0;	// head of the hash chain
	int	bflush;		// set if current block must be flushed

	while ( 1 )
	{
		// Make sure that we always have enough lookahead, except
		// at the end of the input file. We need MAX_MATCH bytes
		// for the next match, plus MIN_MATCH bytes to insert the
		// string following the next match.
		if (s->lookahead < MIN_LOOKAHEAD)
		{
			fill_window(s);
			if (s->lookahead < MIN_LOOKAHEAD && flush == Z_NO_FLUSH)
				return need_more;
		}
		if (s->lookahead == 0) break; // flush the current block

		// Insert the string window[strstart .. strstart+2] in the
		// dictionary, and set hash_head to the head of the hash chain:
		if(s->lookahead >= 3)
		{
			INSERT_STRING(s, s->strstart, hash_head);
		}

		// Find the longest match, discarding those <= prev_length.
		// At this point we have always match_length < MIN_MATCH
		
		if (hash_head && s->strstart - hash_head <= MAX_DIST(s))
		{
			// To simplify the code, we prevent matches with the string
			// of window index 0 (in particular we have to avoid a match
			// of the string with itself at the start of the input file).
			if (s->strategy != Z_HUFFMAN_ONLY)
			{
				s->match_length = longest_match (s, hash_head);
			}
			// longest_match() sets match_start
		}
		if (s->match_length >= 3)
		{
			_tr_tally_dist(s, s->strstart - s->match_start, s->match_length - 3, bflush);
			s->lookahead -= s->match_length;

			// Insert new strings in the hash table only if the match length
			// is not too large. This saves time but degrades compression.
			if (s->match_length <= s->max_lazy_match && s->lookahead >= 3)
			{
				s->match_length--; // string at strstart already in hash table
				do {
					s->strstart++;
					INSERT_STRING(s, s->strstart, hash_head);
					// strstart never exceeds WSIZE-MAX_MATCH, so there are
					// always MIN_MATCH bytes ahead.
				} while (--s->match_length != 0);
				s->strstart++; 
			}
			else
			{
				s->strstart += s->match_length;
				s->match_length = 0;
				s->ins_h = s->window[s->strstart];
				UPDATE_HASH(s, s->ins_h, s->window[s->strstart+1]);
				// If lookahead < MIN_MATCH, ins_h is garbage, but it does not
				// matter since it will be recomputed at next deflate call.
			}
		}
		else
		{
			// No match, output a literal byte
			_tr_tally_lit (s, s->window[s->strstart], bflush);
			s->lookahead--;
			s->strstart++; 
		}
		if (bflush) FLUSH_BLOCK(s, 0);
	}

	FLUSH_BLOCK(s, flush == Z_FINISH);
	return flush == Z_FINISH ? finish_done : block_done;
}

/*
===========================================================================
 Same as above, but achieves better compression. We use a lazy
 evaluation for matches: a match is finally adopted only if there is
 no better match at the next window position.
===========================================================================
*/
block_state deflate_slow(deflate_state *s, int flush)
{
	uint	hash_head = 0;	// head of hash chain
	int	bflush;		// set if current block must be flushed

	// Process the input block.

	while( 1 )
	{
		// Make sure that we always have enough lookahead, except
		// at the end of the input file. We need MAX_MATCH bytes
		// for the next match, plus MIN_MATCH bytes to insert the
		// string following the next match.

		if (s->lookahead < MIN_LOOKAHEAD)
		{
			fill_window(s);
			if (s->lookahead < MIN_LOOKAHEAD && flush == Z_NO_FLUSH)
			{
				return need_more;
			}
			if (s->lookahead == 0) break; // flush the current block
		}

		// Insert the string window[strstart .. strstart+2] in the
		// dictionary, and set hash_head to the head of the hash chain:

		if (s->lookahead >= 3)
		{
			INSERT_STRING(s, s->strstart, hash_head);
		}

		// Find the longest match, discarding those <= prev_length.
		s->prev_length = s->match_length, s->prev_match = s->match_start;
		s->match_length = 3-1;

		if (hash_head && s->prev_length < s->max_lazy_match && s->strstart - hash_head <= MAX_DIST(s))
		{
			// To simplify the code, we prevent matches with the string
			// of window index 0 (in particular we have to avoid a match
			// of the string with itself at the start of the input file).
			if (s->strategy != Z_HUFFMAN_ONLY)
			{
				s->match_length = longest_match (s, hash_head);
			}
			// longest_match() sets match_start
			if (s->match_length <= 5 && (s->strategy == Z_FILTERED || (s->match_length == 3 && s->strstart - s->match_start > 4096)))
			{
				// If prev_match is also MIN_MATCH, match_start is garbage
				// but we will ignore the current match anyway.
				s->match_length = 3 - 1;
			}
		}
		// If there was a match at the previous step and the current
		// match is not better, output the previous match:
		if (s->prev_length >= 3 && s->match_length <= s->prev_length)
		{
			uint max_insert = s->strstart + s->lookahead - 3;
			// Do not insert strings in hash table beyond this.
			_tr_tally_dist(s, s->strstart -1 - s->prev_match, s->prev_length - 3, bflush);

			// Insert in hash table all strings up to the end of the match.
			// strstart-1 and strstart are already inserted. If there is not
			// enough lookahead, the last two strings are not inserted in
			// the hash table.
			s->lookahead -= s->prev_length-1;
			s->prev_length -= 2;

			do
			{
				if (++s->strstart <= max_insert)
				{
					INSERT_STRING(s, s->strstart, hash_head);
				}
			} while (--s->prev_length != 0);

			s->match_available = 0;
			s->match_length = 3 - 1;
			s->strstart++;
			if (bflush) FLUSH_BLOCK(s, 0);

		}
		else if (s->match_available)
		{
			// If there was no match at the previous position, output a
			// single literal. If there was a match but the current match
			// is longer, truncate the previous match to a single literal.
			_tr_tally_lit(s, s->window[s->strstart-1], bflush);
			if (bflush)
			{
				FLUSH_BLOCK_ONLY(s, 0);
			}
			s->strstart++;
			s->lookahead--;
			if (s->strm->avail_out == 0) return need_more;
		}
		else
		{
			// There is no previous match to compare with, wait for
			// the next step to decide.
			s->match_available = 1;
			s->strstart++;
			s->lookahead--;
		}
	}

	if (s->match_available)
	{
		_tr_tally_lit(s, s->window[s->strstart-1], bflush);
		s->match_available = 0;
	}

	FLUSH_BLOCK(s, flush == Z_FINISH);
	return flush == Z_FINISH ? finish_done : block_done;
}

const config configuration_table[10] =
{
{0, 0,  0,  0,	deflate_copy},	// store only
{4, 4,  8,  4,	deflate_fast},	// maximum speed, no lazy matches
{4, 5,  16, 8,	deflate_fast},
{4, 6,  32, 32,	deflate_fast},
{4, 4,  16, 16,	deflate_slow},	// lazy matches
{8, 16, 32, 32,	deflate_slow},
{8, 16, 128,128,	deflate_slow},
{8, 32, 128,256,	deflate_slow},
{32,128,258,1024,	deflate_slow},
{32,258,258,4096,	deflate_slow}	// maximum compression
};

/*
===========================================================================
Initialize the "longest match" routines for a new zlib stream
===========================================================================
*/
void lm_init (deflate_state *s)
{
	s->window_size = (dword)2L * s->w_size;
	CLEAR_HASH(s);

	// Set the default configuration parameters:

	s->max_lazy_match = configuration_table[s->level].max_lazy;
	s->good_match = configuration_table[s->level].good_length;
	s->nice_match = configuration_table[s->level].nice_length;
	s->max_chain_length = configuration_table[s->level].max_chain;

	s->strstart = 0;
	s->block_start = 0L;
	s->lookahead = 0;
	s->match_length = s->prev_length = 3 - 1;
	s->match_available = 0;
	s->ins_h = 0;
}

int deflate (z_streamp strm, int flush)
{
	int old_flush; // value of flush param for previous deflate call
	deflate_state *s;

	if (strm == NULL || strm->state == NULL || flush > Z_FINISH || flush < 0)
		return Z_STREAM_ERROR;
	s = strm->state;

	if (strm->next_out == NULL || (strm->next_in == NULL && strm->avail_in != 0) || (s->status == FINISH_STATE && flush != Z_FINISH))
	{
		strm->msg = (char*)"stream error";
		return Z_STREAM_ERROR;
	}
	if (strm->avail_out == 0) 
	{
		strm->msg = (char*)"buffer error";
		return Z_BUF_ERROR;
	}

	s->strm = strm; // just in case
	old_flush = s->last_flush;
	s->last_flush = flush;

	// Write the zlib header
	if (s->status == INIT_STATE)
	{
		uint header = (Z_DEFLATED + ((s->w_bits-8)<<4)) << 8;
		uint level_flags = (s->level-1) >> 1;

		if (level_flags > 3) level_flags = 3;
		header |= (level_flags << 6);
		if (s->strstart != 0) header |= PRESET_DICT;
		header += 31 - (header % 31);

		s->status = BUSY_STATE;
		putShortMSB(s, header);

		// Save the adler32 of the preset dictionary:
		if (s->strstart != 0)
		{
			putShortMSB(s, (uint)(strm->adler >> 16));
			putShortMSB(s, (uint)(strm->adler & 0xffff));
		}
		strm->adler = 1L;
	}

	// Flush as much pending output as possible
	if (s->pending != 0)
	{
		flush_pending(strm);
		if (strm->avail_out == 0)
		{
			/* Since avail_out is 0, deflate will be called again with
			* more output space, but possibly with both pending and
			* avail_in equal to zero. There won't be anything to do,
			* but this is not an error situation so make sure we
			* return OK instead of BUF_ERROR at next call of deflate:
			*/
			s->last_flush = -1;
			return Z_OK;
		}

		/* Make sure there is something to do and avoid duplicate consecutive
		* flushes. For repeated and useless calls with Z_FINISH, we keep
		* returning Z_STREAM_END instead of Z_BUFF_ERROR.
		*/
	}
	else if (strm->avail_in == 0 && flush <= old_flush && flush != Z_FINISH)
	{
		strm->msg = (char*)"buffer error";
		return Z_BUF_ERROR;
	}

	// User must not provide more input after the first FINISH:
	if (s->status == FINISH_STATE && strm->avail_in != 0)
	{
		strm->msg = (char*)"buffer error";
		return Z_BUF_ERROR;
	}

	// Start a new block or continue the current one.
	if (strm->avail_in != 0 || s->lookahead != 0 || (flush != Z_NO_FLUSH && s->status != FINISH_STATE))
	{
		block_state bstate;
		bstate = (*(configuration_table[s->level].func))(s, flush);

		if (bstate == finish_started || bstate == finish_done)
		{
			s->status = FINISH_STATE;
		}
		if (bstate == need_more || bstate == finish_started)
		{
			if (strm->avail_out == 0)
			{
				s->last_flush = -1; // avoid BUF_ERROR next call, see above
			}
			return Z_OK;
			
			// If flush != Z_NO_FLUSH && avail_out == 0, the next call
			// of deflate should use the same flush parameter to make sure
			// that the flush is complete. So we don't have to output an
			// empty block here, this will be done at next call. This also
			// ensures that for a very small output buffer, we emit at most
			// one empty block.
		}
		if (bstate == block_done)
		{
			if (flush == Z_PARTIAL_FLUSH)
			{
				_tr_align(s);
			}
			else
			{
				// FULL_FLUSH or SYNC_FLUSH
				_tr_stored_block(s, (char*)0, 0L, 0);
				/* For a full flush, this empty block will be recognized
				* as a special marker by inflate_sync().
				*/
				if (flush == Z_FULL_FLUSH)
				{
					CLEAR_HASH(s); // forget history
				}
			}
			flush_pending(strm);
			if (strm->avail_out == 0)
			{
				s->last_flush = -1; /* avoid BUF_ERROR at next call, see above */
				return Z_OK;
			}
		}
	}

	if (flush != Z_FINISH) return Z_OK;
	if (s->noheader) return Z_STREAM_END;

	// Write the zlib trailer (adler32)
	putShortMSB(s, (uint)(strm->adler >> 16));
	putShortMSB(s, (uint)(strm->adler & 0xffff));
	flush_pending(strm);

	// If avail_out is zero, the application will call deflate again
	// to flush the rest.
	s->noheader = -1; // write the trailer only once!
	return s->pending != 0 ? Z_OK : Z_STREAM_END;
}

/* ========================================================================= */
int deflateEnd (z_streamp strm)
{
	int status;

	if (strm == NULL || strm->state == NULL) return Z_STREAM_ERROR;

	status = strm->state->status;
	if (status != INIT_STATE && status != BUSY_STATE && status != FINISH_STATE)
		return Z_STREAM_ERROR;

	// Deallocate in reverse order of allocations:
	ZipFree(strm, strm->state->pending_buf);
	ZipFree(strm, strm->state->head);
	ZipFree(strm, strm->state->prev);
	ZipFree(strm, strm->state->window);

	ZipFree(strm, strm->state);
	strm->state = NULL;

	return status == BUSY_STATE ? Z_DATA_ERROR : Z_OK;
}

int deflateReset (z_streamp strm)
{
	deflate_state *s;
    
	if (strm == NULL || strm->state == NULL || strm->zalloc == NULL || strm->zfree == NULL)
		return Z_STREAM_ERROR;

	strm->total_in = strm->total_out = 0;
	strm->msg = (char *)"Ok";
	strm->data_type = Z_UNKNOWN;
	
	s = (deflate_state *)strm->state;
	s->pending = 0;
	s->pending_out = s->pending_buf;

	if (s->noheader < 0)
	{
		// was set to -1 by deflate(..., Z_FINISH);
		s->noheader = 0; 
	}

	s->status = s->noheader ? BUSY_STATE : INIT_STATE;
	strm->adler = 1;
	s->last_flush = Z_NO_FLUSH;

	_tr_init(s);
	lm_init(s);

	return Z_OK;
}                                                                                                                                                                                                                                                                                                              