//=======================================================================
//			Copyright (C) XashXT Group 2006
//			     All Rights Reserved
//			zip32.h - zlib custom build
//=======================================================================
#ifndef COM_ZLIB_H
#define COM_ZLIB_H

#define ZLIB_VERSION "1.2.3"

#define MAX_WBITS		15
#define PUP(a) *(a)++
#define DEF_MEM_LEVEL	8

// compression levels
#define Z_DEFAULT_COMPRESSION	(-1)
#define Z_NO_COMPRESSION	0
#define Z_BEST_SPEED	1
#define Z_BEST_COMPRESSION	9

// compression strategy; see deflateInit2() below for details
#define Z_DEFAULT_STRATEGY	0
#define Z_FILTERED		1
#define Z_HUFFMAN_ONLY	2

// Possible values of the data_type field
#define Z_BINARY		0
#define Z_ASCII		1
#define Z_UNKNOWN		2

// tree types
#define STORED_BLOCK	0
#define STATIC_TREES	1
#define DYN_TREES		2

//zflags
#define PRESET_DICT 0x20	// preset dictionary flag in zlib header

#define MIN_LOOKAHEAD	(258+3+1)// MAX_MATCH+MIN_MATCH+1
#define MAX_DIST(s)		((s)->w_size-MIN_LOOKAHEAD)
#define LENGTH_CODES	29
#define LITERALS		256
#define L_CODES		(LITERALS+1+LENGTH_CODES)
#define D_CODES		30
#define BL_CODES		19
#define HEAP_SIZE		(2*L_CODES+1)

#define ZipAlloc(strm, items, size) (*((strm)->zalloc))((strm)->opaque, (items), (size))
#define ZipFree(strm, addr) (*((strm)->zfree))((strm)->opaque, (void*)(addr))

typedef struct
{
	byte	op;	// operation, extra bits, table bits
	byte	bits;	// bits in this part of the code
	word	val;	// offset in table or code value
} code;

typedef enum
{
	CODES,
	LENS,
	DISTS
} codetype;

typedef enum
{
	need_more,      // block not completed, need more input or more output
	block_done,     // block flush performed	
	finish_started, // finish started, need only more output at next deflate
	finish_done     // finish done, accept no more input or output
} block_state;

typedef struct ct_data_s
{
	union
	{
		word	freq;	// frequency count
		word	code;	// bit string
	} fc;
	union
	{
		word	dad;	// father node in Huffman tree
		word	len;	// length of bit string
	} dl;
} ct_data;

typedef struct static_tree_desc_s  static_tree_desc;

typedef struct tree_desc_s
{
	ct_data		*dyn_tree;  // the dynamic tree
	int		max_code;   // largest code with non zero frequency
	static_tree_desc	*stat_desc; // the corresponding static tree
} tree_desc;

typedef byte* (*alloc_func)();
typedef void  (*free_func)();
typedef block_state (*compress_func)();

// Possible inflate modes between inflate() calls
typedef enum
{
	HEAD,       	// i: waiting for magic header
	FLAGS,      	// i: waiting for method and flags (gzip)
	TIME,       	// i: waiting for modification time (gzip)
	OS,         	// i: waiting for extra flags and operating system (gzip)
	EXLEN,      	// i: waiting for extra length (gzip)
	EXTRA,      	// i: waiting for extra bytes (gzip)
	NAME,       	// i: waiting for end of file name (gzip)
	COMMENT,    	// i: waiting for end of comment (gzip)
	HCRC,       	// i: waiting for header crc (gzip)
	DICTID,     	// i: waiting for dictionary check value
	DICT,       	// waiting for inflateSetDictionary() call
	TYPE,       	// i: waiting for type bits, including last-flag bit
	TYPEDO,     	// i: same, but skip check to exit inflate on new block
	STORED,     	// i: waiting for stored size (length and complement)
	COPY,       	// i/o: waiting for input or output to copy stored block
	TABLE,      	// i: waiting for dynamic block table lengths
	LENLENS,    	// i: waiting for code length code lengths
	CODELENS,   	// i: waiting for length/lit and distance code lengths
	LEN,        	// i: waiting for length/lit code
	LENEXT,     	// i: waiting for length extra bits
	DIST,       	// i: waiting for distance code
	DISTEXT,    	// i: waiting for distance extra bits
	MATCH,      	// o: waiting for output space to copy string
	LIT,        	// o: waiting for output space to write literal
	CHECK,      	// i: waiting for 32-bit check value
	LENGTH,     	// i: waiting for 32-bit length (gzip)
	DONE,       	// finished check, done -- remain here until reset
	BAD,        	// got a data error -- remain here until reset
	MEM,        	// got an inflate() memory error -- remain here until reset
	SYNC        	// looking for synchronization bytes to restart inflate()
} inflate_mode;

typedef struct gz_header_s
{
	int	text;	// true if compressed data believed to be text
	dword	time;	// modification time
	int	xflags;	// extra flags (not used when writing a gzip file)
	int	os;	// operating system
	byte	*extra;	// pointer to extra field or Z_NULL if none
	dword	extra_len;// extra field length (valid if extra != Z_NULL)
	dword	extra_max;// space at extra (only when reading header)
	byte	*name;	// pointer to zero-terminated file name or Z_NULL
	dword	name_max;	// space at name (only when reading header)
	byte	*comment;	// pointer to zero-terminated comment or Z_NULL
	dword	comm_max;	// space at comment (only when reading header)
	int	hcrc;	// true if there was or will be a header crc
	int	done;	// true when done reading gzip header (not used when writing a gzip file)
}gz_header;

typedef gz_header *gz_headerp;

typedef struct z_stream_s
{
	byte		*next_in;		// next input byte
	dword		avail_in;		// number of bytes available at next_in
	dword		total_in;		// total nb of input bytes read so far
	byte		*next_out;	// next output byte should be put there
	dword		avail_out;	// remaining free space at next_out
	dword		total_out;	// total nb of bytes output so far
	char		*msg;		// last error message, NULL if no error
	struct int_state	*state;		// not visible by applications
	alloc_func	zalloc;		// used to allocate the internal state
	free_func		zfree;		// used to free the internal state
	byte*		opaque;		// private data object passed to zalloc and zfree
	int		data_type;	// best guess about the data type: binary or text
	dword		adler;		// adler32 value of the uncompressed data
	dword		reserved;		// reserved for future use
}z_stream;

typedef z_stream *z_streamp;

struct inflate_state
{
	inflate_mode	mode;		// current inflate mode
	int		last;		// true if processing last block
	int		wrap;		// bit 0 true for zlib, bit 1 true for gzip
	int		havedict;		// true if dictionary provided
	int		flags;		// gzip header method and flags (0 if zlib)
	uint		dmax;		// zlib header max distance (INFLATE_STRICT)
	dword		check;		// protected copy of check value
	dword		total;		// protected copy of output count
	gz_headerp	head;		// where to save gzip header information
					// sliding window
	uint		wbits;		// log base 2 of requested window size
	uint		wsize;		// window size or zero if not using window
	uint		whave;		// valid bytes in the window
	uint		write;		// window write index
	byte		*window;		// allocated sliding window, if needed
					// bit accumulator
	dword		hold;		// input bit accumulator
	uint		bits;		// number of bits in "in"
					// for string and stored block copying
	uint		length;		// literal or length of data to copy
	uint		offset;		// distance back to copy string from
					// for table and code decoding
	uint		extra;		// extra bits needed
					// fixed and dynamic code tables
	code const	*lencode;		// starting table for length/literal codes
	code const	*distcode;	// starting table for distance codes
	uint		lenbits;		// index bits for lencode
	uint		distbits;		// index bits for distcode
					// dynamic table building
	uint		ncode;		// number of code length code lengths
	uint		nlen;		// number of length code lengths
	uint		ndist;		// number of distance code lengths
	uint		have;		// number of code lengths in lens[]
	code		*next;		// next available space in codes[]
	word		lens[320];	// temporary storage for code lengths
	word		work[288];	// work area for code table building
	code		codes[2048];	// space for code tables
};

typedef struct int_state
{
	z_streamp		strm;		/* pointer back to this zlib stream */
	int		status;		/* as the name implies */
	byte		*pending_buf;	/* output still pending */
	dword		pending_buf_size;	/* size of pending_buf */
	byte		*pending_out;	/* next pending byte to output to the stream */
	int		pending;		/* nb of bytes in the pending buffer */
	int		noheader;		/* suppress zlib header and adler32 */
	byte		data_type;	/* UNKNOWN, BINARY or ASCII */
	byte		method;		/* STORED (for zip only) or DEFLATED */
	int		last_flush;	/* value of flush param for previous deflate call */

	uint		w_size;		/* LZ77 window size (32K by default) */
	uint		w_bits;		/* log2(w_size)  (8..16) */
	uint		w_mask;		/* w_size - 1 */

	byte		*window;
	dword		window_size;

	word		*prev;
	word		*head;		/* Heads of the hash chains or NIL. */

	uint		ins_h;		/* hash index of string to be inserted */
	uint		hash_size;	/* number of elements in hash table */
	uint		hash_bits;	/* log2(hash_size) */
	uint		hash_mask;	/* hash_size-1 */

	uint		hash_shift;

	long		block_start;

	uint		match_length;	/* length of best match */
	uint		prev_match;	/* previous match */
	int		match_available;	/* set if previous match exists */
	uint		strstart;		/* start of string to insert */
	uint		match_start;	/* start of matching string */
	uint		lookahead;	/* number of valid bytes ahead in window */

	uint		prev_length;
	uint		max_chain_length;

	uint		max_lazy_match;

	int		level;		// compression level (1..9)
	int		strategy;		// favor or force Huffman coding

	uint		good_match;
	int		nice_match;	// Stop searching when current match exceeds this

	byte		*l_buf;		// buffer for literals or lengths
	uint		lit_bufsize;
	uint		last_lit;		// running index in l_buf
	word		*d_buf;

	ct_data		dyn_ltree[HEAP_SIZE];   // literal and length tree
	ct_data		dyn_dtree[2*D_CODES+1]; // distance tree
	ct_data		bl_tree[2*BL_CODES+1];  // Huffman tree for bit lengths

	tree_desc		l_desc;		// desc. for literal tree
	tree_desc		d_desc;		// desc. for distance tree
	tree_desc		bl_desc;		// desc. for bit length tree

	word		bl_count[MAX_WBITS+1];

	int		heap[2*L_CODES+1];	// heap used to build the Huffman trees
	int		heap_len;		// number of elements in the heap
	int		heap_max;		// element of largest frequency

	byte		depth[2*L_CODES+1];

	dword		opt_len;		// bit length of current block with optimal trees
	dword		static_len;	// bit length of current block with static trees
	uint		matches;		// number of string matches in current block
	int		last_eob_len;	// bit length of EOB code for last block

	word		bi_buf;
	int		bi_valid;

} deflate_state;

//zlib errors
#define Z_OK		0
#define Z_STREAM_END	1
#define Z_NEED_DICT		2
#define Z_ERRNO		(-1)
#define Z_STREAM_ERROR	(-2)
#define Z_DATA_ERROR	(-3)
#define Z_MEM_ERROR		(-4)
#define Z_BUF_ERROR		(-5)
#define Z_VERSION_ERROR	(-6)
#define Z_NO_FLUSH		0
#define Z_PARTIAL_FLUSH	1
#define Z_SYNC_FLUSH	2
#define Z_FULL_FLUSH	3
#define Z_FINISH		4
#define Z_BLOCK		5
#define Z_DEFLATED		8

// zlib states
#define INIT_STATE		42
#define BUSY_STATE		113
#define FINISH_STATE	666

#define put_byte(s, c) { s->pending_buf[s->pending++] = (c); }
#define put_short(s, w) { put_byte(s, (byte)((w) & 0xff)); put_byte(s, (byte)((byte)(w) >> 8)); }
_inline void putShortMSB (deflate_state *s, uint b){ put_byte(s, (byte)(b >> 8)); put_byte(s, (byte)(b & 0xff)); }   

// exported functions
dword adler32(dword adler, const byte *buf, dword len);

extern int inflate(z_streamp strm, int flush);
extern int inflateEnd(z_streamp strm);
extern int inflateInit_(z_streamp strm, const char *version, int stream_size);
extern int inflateInit2_(z_streamp strm, int windowBits, const char *version, int stream_size);
extern int inflateReset(z_streamp strm);

extern int deflate (z_streamp strm, int flush);
extern int deflateEnd (z_streamp strm);
extern int deflateInit_(z_streamp strm, int level, const char *version, int stream_size);
extern int deflateInit2_(z_streamp strm, int level, int windowBits, const char *version, int stream_size);
extern int deflateReset (z_streamp strm);

#define inflateInit(strm) inflateInit_((strm), ZLIB_VERSION, sizeof(z_stream))
#define inflateInit2(strm, windowBits) inflateInit2_((strm), (windowBits), ZLIB_VERSION, sizeof(z_stream))

#define deflateInit(strm, level) deflateInit_((strm), (level), ZLIB_VERSION, sizeof(z_stream))
#define deflateInit2(strm, level, windowBits) deflateInit2_((strm),(level),(windowBits), ZLIB_VERSION, sizeof(z_stream))

#endif//COM_ZLIB_H