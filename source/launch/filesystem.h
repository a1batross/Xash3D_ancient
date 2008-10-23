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

#define MAX_FILES_IN_PACK	65536 // pak\pk2

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

/*
========================================================================
.WAD archive format	(WhereAllData - WAD)

List of compressed files, that can be identify only by TYPE_*

<format>
header:	dwadinfo_t[dwadinfo_t]
file_1:	byte[dwadinfo_t[num]->disksize]
file_2:	byte[dwadinfo_t[num]->disksize]
file_3:	byte[dwadinfo_t[num]->disksize]
...
file_n:	byte[dwadinfo_t[num]->disksize]
infotable	dlumpinfo_t[dwadinfo_t->numlumps]
========================================================================
*/
#define IDIWADHEADER	(('D'<<24)+('A'<<16)+('W'<<8)+'I')	// little-endian "IWAD" doom1 game wad
#define IDPWADHEADER	(('D'<<24)+('A'<<16)+('W'<<8)+'P')	// little-endian "PWAD" doom1 game wad
#define IDWAD2HEADER	(('2'<<24)+('D'<<16)+('A'<<8)+'W')	// little-endian "WAD2" quake1 gfx.wad
#define IDWAD3HEADER	(('3'<<24)+('D'<<16)+('A'<<8)+'W')	// little-endian "WAD3" half-life wads

#define WAD3_NAMELEN	16
#define MAX_FILES_IN_WAD	8192

// hidden virtual lump types
#define TYPE_ANY		-1	// any type can be accepted
#define TYPE_NONE		0	// unknown lump type
#define TYPE_FLMP		1	// doom1 hud picture (doom1 mapped lump)
#define TYPE_SND		2	// doom1 wav sound (doom1 mapped lump)
#define TYPE_MUS		3	// doom1 music file (doom1 mapped lump)
#define TYPE_SKIN		4	// doom1 sprite model (doom1 mapped lump)
#define TYPE_FLAT		5	// doom1 wall texture (doom1 mapped lump)
#define TYPE_MAXHIDDEN	63	// after this number started typeing letters ( 'a', 'b' etc )

#include "const.h"

typedef struct
{
	int		ident;		// should be IWAD, WAD2 or WAD3
	int		numlumps;		// num files
	int		infotableofs;
} dwadinfo_t;

// doom1 and doom2 lump header
typedef struct
{
	int		filepos;
	int		size;
	char		name[8];		// null not included
} dlumpfile_t;

typedef struct
{
	int		filepos;
	int		disksize;
	int		size;		// uncompressed
	char		type;
	char		compression;	// probably not used
	char		pad1;
	char		pad2;
	char		name[16];		// must be null terminated
} dlumpinfo_t;

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

// zlib exported functions
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