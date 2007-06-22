//=======================================================================
//			Copyright (C) XashXT Group 2006
//			     All Rights Reserved
//			zip32.h - zlib custom build
//=======================================================================
#ifndef COM_ZLIB_H
#define COM_ZLIB_H

#define ZLIB_VERSION "1.2.3"

#define MAX_WBITS	15
#define PUP(a) *(a)++

typedef byte* (*alloc_func)();
typedef void   (*free_func)();

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

struct inflate_state
{
	inflate_mode	mode;		// current inflate mode
	int		last;		// true if processing last block
	int		wrap;		// bit 0 true for zlib, bit 1 true for gzip
	int		havedict;		// true if dictionary provided
	int		flags;		// gzip header method and flags (0 if zlib)
	unsigned		dmax;		// zlib header max distance (INFLATE_STRICT)
	unsigned long	check;		// protected copy of check value
	unsigned long	total;		// protected copy of output count
	gz_headerp	head;		// where to save gzip header information
					// sliding window
	unsigned		wbits;		// log base 2 of requested window size
	unsigned		wsize;		// window size or zero if not using window
	unsigned		whave;		// valid bytes in the window
	unsigned		write;		// window write index
	byte		*window;		// allocated sliding window, if needed
					// bit accumulator
	dword		hold;		// input bit accumulator
	unsigned		bits;		// number of bits in "in"
					// for string and stored block copying
	unsigned		length;		// literal or length of data to copy
	unsigned		offset;		// distance back to copy string from
					// for table and code decoding
	unsigned		extra;		// extra bits needed
					// fixed and dynamic code tables
	code const	*lencode;		// starting table for length/literal codes
	code const	*distcode;	// starting table for distance codes
	unsigned		lenbits;		// index bits for lencode
	unsigned		distbits;		// index bits for distcode
					// dynamic table building
	unsigned		ncode;		// number of code length code lengths
	unsigned		nlen;		// number of length code lengths
	unsigned		ndist;		// number of distance code lengths
	unsigned		have;		// number of code lengths in lens[]
	code		*next;		// next available space in codes[]
	word		lens[320];	// temporary storage for code lengths
	word		work[288];	// work area for code table building
	code		codes[2048];	// space for code tables
};


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

//exported functions
unsigned long crc32(unsigned long crc, const unsigned char *buf, unsigned len);	//crc32
unsigned long adler32(dword adler, const byte *buf, dword len);
extern int inflate(z_streamp strm, int flush);
extern int inflateEnd(z_streamp strm);
extern int inflateInit_(z_streamp strm, const char *version, int stream_size);
extern int inflateInit2_(z_streamp strm, int windowBits, const char *version, int stream_size);
extern int inflateReset(z_streamp strm);

#define inflateInit(strm) inflateInit_((strm), ZLIB_VERSION, sizeof(z_stream))
#define inflateInit2(strm, windowBits) inflateInit2_((strm), (windowBits), ZLIB_VERSION, sizeof(z_stream))

#endif//COM_ZLIB_H