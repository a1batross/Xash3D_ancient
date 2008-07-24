//=======================================================================
//			Copyright (C) XashXT Group 2006
//			     All Rights Reserved
//			zip32.h - zlib custom build
//=======================================================================
#ifndef COM_ZLIB_H
#define COM_ZLIB_H

/*
========================================================================
PAK FILES

The .pak files are just a linear collapse of a directory tree
========================================================================
*/
// header
#define IDPACKV1HEADER	(('K'<<24)+('C'<<16)+('A'<<8)+'P')	// little-endian "PACK"
#define IDPACKV2HEADER	(('2'<<24)+('K'<<16)+('A'<<8)+'P')	// little-endian "PAK2"
#define IDPACKV3HEADER	(('\4'<<24)+('\3'<<16)+('K'<<8)+'P')	// little-endian "PK\3\4"
#define IDPK3CDRHEADER	(('\2'<<24)+('\1'<<16)+('K'<<8)+'P')	// little-endian "PK\1\2"
#define IDPK3ENDHEADER	(('\6'<<24)+('\5'<<16)+('K'<<8)+'P')	// little-endian "PK\5\6"

#define MAX_FILES_IN_PACK	65536 // pack\pak2

typedef struct
{
	int		ident;
	int		dirofs;
	int		dirlen;
} dpackheader_t;

typedef struct
{
	char		name[56];		// total 64 bytes
	int		filepos;
	int		filelen;
} dpackfile_t;

typedef struct
{
	char		name[116];	// total 128 bytes
	int		filepos;
	int		filelen;
	uint		attribs;		// file attributes
} dpak2file_t;

typedef struct
{
	int		ident;
	word		disknum;
	word		cdir_disknum;	// number of the disk with the start of the central directory
	word		localentries;	// number of entries in the central directory on this disk
	word		nbentries;	// total number of entries in the central directory on this disk
	uint		cdir_size;	// size of the central directory
	uint		cdir_offset;	// with respect to the starting disk number
	word		comment_size;
} dpak3file_t;

#define ZLIB_VERSION	"1.2.3"
#define MAX_WBITS		15

// zlib errors
#define Z_OK		0
#define Z_STREAM_END	1
#define Z_SYNC_FLUSH	2
#define Z_FINISH		4

typedef struct z_stream_s
{
	byte		*next_in;		// next input byte
	uint		avail_in;		// number of bytes available at next_in
	dword		total_in;		// total nb of input bytes read so far
	byte		*next_out;	// next output byte should be put there
	uint		avail_out;	// remaining free space at next_out
	dword		total_out;	// total nb of bytes output so far
	char		*msg;		// last error message, NULL if no error
	byte		*state;		// not visible by applications
	byte*		(*zalloc)();	// used to allocate the internal state
	void		(*zfree)();	// used to free the internal state
	byte*		opaque;		// private data object passed to zalloc and zfree
	int		data_type;	// best guess about the data type: binary or text
	dword		adler;		// adler32 value of the uncompressed data
	dword		reserved;		// reserved for future use
} z_stream;

// exported functions
extern int inflate(z_stream *strm, int flush);
extern int inflateEnd(z_stream *strm);
extern int inflateInit_(z_stream *strm, const char *version, int stream_size);
extern int inflateInit2_(z_stream *strm, int windowBits, const char *version, int stream_size);
extern int inflateReset(z_stream *strm);
extern int deflate (z_stream *strm, int flush);
extern int deflateEnd (z_stream *strm);
extern int deflateInit_(z_stream *strm, int level, const char *version, int stream_size);
extern int deflateInit2_ (z_stream *strm, int level, int method, int windowBits, int memLevel, int strategy, const char *version, int stream_size);
extern int deflateReset (z_stream *strm);

#define inflateInit(strm) inflateInit_((strm), ZLIB_VERSION, sizeof(z_stream))
#define inflateInit2(strm, windowBits) inflateInit2_((strm), (windowBits), ZLIB_VERSION, sizeof(z_stream))
#define deflateInit(strm, level) deflateInit_((strm), (level), ZLIB_VERSION, sizeof(z_stream))
#define deflateInit2(strm, level, method, windowBits, memLevel, strategy) deflateInit2_((strm),(level),(method),(windowBits),(memLevel),(strategy), ZLIB_VERSION, sizeof(z_stream))

#endif//COM_ZLIB_H