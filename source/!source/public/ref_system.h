//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    ref_system.h - generic shared interfaces
//=======================================================================
#ifndef REF_SYSTEM_H
#define REF_SYSTEM_H

#include "ref_format.h" 
#include "version.h"

#define MAX_LIGHTSTYLES	256

#define ENTITY_FLAGS	68
#define POWERSUIT_SCALE	4.0F

#define	EF_TELEPORTER	(1<<0)		// particle fountain
#define	EF_ROTATE		(1<<1)		// rotate (bonus items)

// shared client/renderer flags
#define	RF_MINLIGHT	1		// allways have some light (viewmodel)
#define	RF_VIEWERMODEL	2		// don't draw through eyes, only mirrors
#define	RF_WEAPONMODEL	4		// only draw through eyes
#define	RF_FULLBRIGHT	8		// allways draw full intensity
#define	RF_DEPTHHACK	16		// for view weapon Z crunching
#define	RF_TRANSLUCENT	32
#define	RF_FRAMELERP	64
#define	RF_BEAM		128
#define	RF_IR_VISIBLE	256		// skin is an index in image_precache
#define	RF_GLOW		512		// pulse lighting for bonus items

// render private flags
#define	RDF_NOWORLDMODEL	1		// used for player configuration screen
#define	RDF_IRGOGGLES	2
#define	RDF_PAIN           	4

// phys movetype
#define MOVETYPE_NONE		0	// never moves
#define MOVETYPE_NOCLIP		1	// origin and angles change with no interaction
#define MOVETYPE_PUSH		2	// no clip to world, push on box contact
#define MOVETYPE_WALK		3	// gravity
#define MOVETYPE_STEP		4	// gravity, special edge handling
#define MOVETYPE_FLY		5
#define MOVETYPE_TOSS		6	// gravity
#define MOVETYPE_BOUNCE		7
#define MOVETYPE_FOLLOW		8	// attached models
#define MOVETYPE_CONVEYOR		9
#define MOVETYPE_PUSHABLE		10

// opengl mask
#define GL_COLOR_INDEX	0x1900
#define GL_STENCIL_INDEX	0x1901
#define GL_DEPTH_COMPONENT	0x1902
#define GL_RED		0x1903
#define GL_GREEN		0x1904
#define GL_BLUE		0x1905
#define GL_ALPHA		0x1906
#define GL_RGB		0x1907
#define GL_RGBA		0x1908
#define GL_LUMINANCE	0x1909
#define GL_LUMINANCE_ALPHA	0x190A
#define GL_BGR		0x80E0
#define GL_BGRA		0x80E1

// gl data type
#define GL_BYTE		0x1400
#define GL_UNSIGNED_BYTE	0x1401
#define GL_SHORT		0x1402
#define GL_UNSIGNED_SHORT	0x1403
#define GL_INT		0x1404
#define GL_UNSIGNED_INT	0x1405
#define GL_FLOAT		0x1406
#define GL_2_BYTES		0x1407
#define GL_3_BYTES		0x1408
#define GL_4_BYTES		0x1409
#define GL_DOUBLE		0x140A

// description flags
#define IMAGE_CUBEMAP	0x00000001
#define IMAGE_HAS_ALPHA	0x00000002
#define IMAGE_PREMULT	0x00000004	// indices who need in additional premultiply
#define IMAGE_GEN_MIPS	0x00000008	// must generate mips
#define IMAGE_CUBEMAP_FLIP	0x00000010	// it's a cubemap with flipped sides( dds pack )

enum comp_format
{
	PF_UNKNOWN = 0,
	PF_INDEXED_24,	// studio model skins
	PF_INDEXED_32,	// sprite 32-bit palette
	PF_RGBA_32,	// already prepared ".bmp", ".tga" or ".jpg" image 
	PF_ARGB_32,	// uncompressed dds image
	PF_RGB_24,	// uncompressed dds or another 24-bit image 
	PF_RGB_24_FLIP,	// flip image for screenshots
	PF_DXT1,		// nvidia DXT1 format
	PF_DXT2,		// nvidia DXT2 format
	PF_DXT3,		// nvidia DXT3 format
	PF_DXT4,		// nvidia DXT4 format
	PF_DXT5,		// nvidia DXT5 format
	PF_ATI1N,		// ati 1N texture
	PF_ATI2N,		// ati 2N texture
	PF_LUMINANCE,	// b&w dds image
	PF_LUMINANCE_16,	// b&w hi-res image
	PF_LUMINANCE_ALPHA, // b&w dds image with alpha channel
	PF_RXGB,		// doom3 normal maps
	PF_ABGR_64,	// uint image
	PF_RGBA_GN,	// internal generated texture
	PF_TOTALCOUNT,	// must be last
};

// format info table
typedef struct bpc_desc_s
{
	int	format;	// pixelformat
	char	name[8];	// used for debug
	uint	glmask;	// RGBA mask
	uint	gltype;	// open gl datatype
	int	bpp;	// channels (e.g. rgb = 3, rgba = 4)
	int	bpc;	// sizebytes (byte, short, float)
	int	block;	// blocksize < 0 needs alternate calc
} bpc_desc_t;

static const bpc_desc_t PFDesc[] =
{
{PF_UNKNOWN,	"raw",	GL_RGBA,		GL_UNSIGNED_BYTE, 0,  0,  0 },
{PF_INDEXED_24,	"pal 24",	GL_RGBA,		GL_UNSIGNED_BYTE, 3,  1,  0 },// expand data to RGBA buffer
{PF_INDEXED_32,	"pal 32",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1,  0 },
{PF_RGBA_32,	"RGBA 32",GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, -4 },
{PF_ARGB_32,	"ARGB 32",GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, -4 },
{PF_RGB_24,	"RGB 24",	GL_RGBA,		GL_UNSIGNED_BYTE, 3,  1, -3 },
{PF_RGB_24_FLIP,	"RGB 24",	GL_RGBA,		GL_UNSIGNED_BYTE, 3,  1, -3 },
{PF_DXT1,		"DXT1",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1,  8 },
{PF_DXT2,		"DXT2",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, 16 },
{PF_DXT3,		"DXT3",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, 16 },
{PF_DXT4,		"DXT4",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, 16 },
{PF_DXT5,		"DXT5",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, 16 },
{PF_ATI1N,	"ATI1N",	GL_RGBA,		GL_UNSIGNED_BYTE, 1,  1,  8 },
{PF_ATI2N,	"3DC",	GL_RGBA,		GL_UNSIGNED_BYTE, 3,  1, 16 },
{PF_LUMINANCE,	"LUM 8",	GL_LUMINANCE,	GL_UNSIGNED_BYTE, 1,  1, -1 },
{PF_LUMINANCE_16,	"LUM 16", GL_LUMINANCE,	GL_UNSIGNED_BYTE, 2,  2, -2 },
{PF_LUMINANCE_ALPHA,"LUM A",	GL_LUMINANCE_ALPHA,	GL_UNSIGNED_BYTE, 2,  1, -2 },
{PF_RXGB,		"RXGB",	GL_RGBA,		GL_UNSIGNED_BYTE, 3,  1, 16 },
{PF_ABGR_64,	"ABGR 64",GL_BGRA,		GL_UNSIGNED_BYTE, 4,  2, -8 },
{PF_RGBA_GN,	"system",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, -4 },
};

typedef struct rgbdata_s
{
	word	width;		// image width
	word	height;		// image height
	byte	numLayers;	// multi-layer volume
	byte	numMips;		// mipmap count
	byte	bitsCount;	// RGB bits count
	uint	type;		// compression type
	uint	flags;		// misc image flags
	byte	*palette;		// palette if present
	byte	*buffer;		// image buffer
	uint	size;		// for bounds checking
};

typedef struct physdata_s
{
	vec3_t		origin;
	vec3_t		angles;
	vec3_t		velocity;
	vec3_t		avelocity;	// "omega" in newton
	vec3_t		mins;		// for calculate size 
	vec3_t		maxs;		// and setup offset matrix

	NewtonCollision	*collision;
	NewtonBody	*physbody;	// ptr to physic body
} physdata_t;

typedef struct gameinfo_s
{
	//filesystem info
	char	basedir[128];
	char	gamedir[128];
	char	title[128];
          float	version;
	
	int	viewmode;
	int	gamemode;

	// system info
	int	cpunum;
	int64	tickcount;
	float	cpufreq;
	bool	rdtsc;
	
	char key[16];

} gameinfo_t;

typedef struct dll_info_s
{
	// generic interface
	const char	*name;		// library name
	const dllfunc_t	*fcts;		// list of dll exports	
	const char	*entry;		// entrypoint name (internal libs only)
	void		*link;		// hinstance of loading library

	// xash dlls entrypoint
	void		*(*main)( void*, void* );
	bool		crash;		// crash if dll not found

	// xash dlls validator
	size_t		api_size;		// generic interface size

} dll_info_t;

typedef struct cvar_s
{
	char	*name;
	char	*string;		// normal string
	float	value;		// atof( string )
	int	integer;		// atoi( string )

	char	*reset_string;	// cvar_restart will reset to this value
	char	*latched_string;	// for CVAR_LATCH vars
	char	*description;	// variable descrition info
	uint	flags;		// state flags
	bool	modified;		// set each time the cvar is changed
	uint	modificationCount;	// incremented each time the cvar is changed

	struct cvar_s *next;
	struct cvar_s *hash;
} cvar_t;

typedef struct dlight_s
{
	vec3_t	origin;
	vec3_t	color;
	float	intensity;

} dlight_t;

typedef struct particle_s
{
	vec3_t	origin;
	int	color;
	float	alpha;

} particle_t;

typedef struct lightstyle_s
{
	float	rgb[3];		// 0.0 - 2.0
	float	white;		// highest of rgb

} lightstyle_t;

typedef struct latchedvars_s
{
	float		animtime;
	float		sequencetime;
	vec3_t		origin;
	vec3_t		angles;		

	int		sequence;
	float		frame;

	byte		blending[MAXSTUDIOBLENDS];
	byte		seqblending[MAXSTUDIOBLENDS];
	byte		controller[MAXSTUDIOCONTROLLERS];

} latchedvars_t;

// client entity
typedef struct entity_s
{
	model_t		*model;		// opaque type outside refresh
	model_t		*weaponmodel;	// opaque type outside refresh	

	latchedvars_t	prev;		//previous frame values for lerping
	
	vec3_t		angles;
	vec3_t		origin;		// also used as RF_BEAM's "from"
	float		oldorigin[3];	// also used as RF_BEAM's "to"

          float		animtime;	
	float		frame;		// also used as RF_BEAM's diameter
	float		framerate;

	int		body;
	int		skin;
	
	byte		blending[MAXSTUDIOBLENDS];
	byte		controller[MAXSTUDIOCONTROLLERS];
	byte		mouth;		//TODO: move to struct
	
          int		movetype;		//entity moving type
	int		sequence;
	float		scale;
	
	vec3_t		attachment[MAXSTUDIOATTACHMENTS];
	
	// misc
	float		backlerp;		// 0.0 = current, 1.0 = old
	int		skinnum;		// also used as RF_BEAM's palette index

	int		lightstyle;	// for flashing entities
	float		alpha;		// ignore if RF_TRANSLUCENT isn't set

	image_t		*image;		// NULL for inline skin
	int		flags;

} entity_t;

typedef struct
{
	int		x, y, width, height;// in virtual screen coordinates
	float		fov_x, fov_y;
	float		vieworg[3];
	float		viewangles[3];
	float		blend[4];		// rgba 0-1 full screen blend
	float		time;		// time is used to auto animate
	int		rdflags;		// RDF_UNDERWATER, etc

	byte		*areabits;	// if not NULL, only areas with set bits will be drawn

	lightstyle_t	*lightstyles;	// [MAX_LIGHTSTYLES]

	int		num_entities;
	entity_t		*entities;

	int		num_dlights;
	dlight_t		*dlights;

	int		num_particles;
	particle_t	*particles;

} refdef_t;

/*
==============================================================================

STDLIB SYSTEM INTERFACE
==============================================================================
*/
typedef struct stdilib_api_s
{
	// interface validator
	size_t	api_size;				// must matched with sizeof(stdlib_api_t)
	
	// base events
	void (*print)( const char *msg );		// basic text message
	void (*printf)( const char *msg, ... );		// formatted text message
	void (*dprintf)( int level, const char *msg, ...);// developer text message
	void (*wprintf)( const char *msg, ... );	// warning text message
	void (*error)( const char *msg, ... );		// abnormal termination with message
	void (*abort)( const char *msg, ... );		// normal tremination with message
	void (*exit)( void );			// normal silent termination
	char *(*input)( void );			// system console input	
	void (*sleep)( int msec );			// sleep for some msec
	char *(*clipboard)( void );			// get clipboard data
	uint (*keyevents)( void );			// peek windows message

	// crclib.c funcs
	void (*crc_init)(word *crcvalue);			// set initial crc value
	word (*crc_block)(byte *start, int count);		// calculate crc block
	void (*crc_process)(word *crcvalue, byte data);		// process crc byte
	byte (*crc_sequence)(byte *base, int length, int sequence);	// calculate crc for sequence
	uint (*crc_blockchecksum)(void *buffer, int length);	// map checksum
	uint (*crc_blockchecksumkey)(void *buf, int len, int key);	// process key checksum

	// memlib.c funcs
	void (*memcpy)(void *dest, void *src, size_t size, const char *file, int line);
	void (*memset)(void *dest, int set, size_t size, const char *file, int line);
	void *(*realloc)(byte *pool, void *mem, size_t size, const char *file, int line);
	void (*move)(byte *pool, void **dest, void *src, size_t size, const char *file, int line); // not a memmove
	void *(*malloc)(byte *pool, size_t size, const char *file, int line);
	void (*free)(void *data, const char *file, int line);

	// xash memlib extension - memory pools
	byte *(*mallocpool)(const char *name, const char *file, int line);
	void (*freepool)(byte **poolptr, const char *file, int line);
	void (*clearpool)(byte *poolptr, const char *file, int line);
	void (*memcheck)(const char *file, int line);		// check memory pools for consistensy

	// common functions
	void (*Com_InitRootDir)( char *path );			// init custom rootdir 
	void (*Com_LoadGameInfo)( const char *filename );		// gate game info from script file
	void (*Com_AddGameHierarchy)(const char *dir);		// add base directory in search list
	int  (*Com_CheckParm)( const char *parm );		// check parm in cmdline  
	bool (*Com_GetParm)( char *parm, char *out );		// get parm from cmdline
	void (*Com_FileBase)(char *in, char *out);		// get filename without path & ext
	bool (*Com_FileExists)(const char *filename);		// return true if file exist
	long (*Com_FileSize)(const char *filename);		// same as Com_FileExists but return filesize
	const char *(*Com_FileExtension)(const char *in);		// return extension of file
	const char *(*Com_RemovePath)(const char *in);		// return file without path
	void (*Com_StripExtension)(char *path);			// remove extension if present
	void (*Com_StripFilePath)(const char* const src, char* dst);// get file path without filename.ext
	void (*Com_DefaultExtension)(char *path, const char *ext );	// append extension if not present
	void (*Com_ClearSearchPath)( void );			// delete all search pathes
	void (*Com_CreateThread)(int, bool, void(*fn)(int));	// run individual thread
	void (*Com_ThreadLock)( void );			// lock current thread
	void (*Com_ThreadUnlock)( void );			// unlock numthreads
	int (*Com_NumThreads)( void );			// returns count of active threads
	bool (*Com_LoadScript)(const char *name,char *buf,int size);// load script into stack from file or bufer
	bool (*Com_AddScript)(const char *name,char *buf, int size);// include script from file or buffer
	void (*Com_ResetScript)( void );			// reset current script state
	char *(*Com_ReadToken)( bool newline );			// get next token on a line or newline
	bool (*Com_TryToken)( void );				// return 1 if have token on a line 
	void (*Com_FreeToken)( void );			// free current token to may get it again
	void (*Com_SkipToken)( void );			// skip current token and jump into newline
	bool (*Com_MatchToken)( const char *match );		// compare current token with user keyword
	char *(*Com_ParseToken)(const char **data );		// parse token from char buffer
	char *(*Com_ParseWord)( const char **data );		// parse word from char buffer
	search_t *(*Com_Search)(const char *pattern, int casecmp );	// returned list of found files
	bool (*Com_Filter)(char *filter, char *name, int casecmp ); // compare keyword by mask with filter
	char *com_token;					// contains current token

	// console variables
	cvar_t *(*Cvar_Get)(const char *name, const char *value, int flags, const char *desc);
	void (*Cvar_CommandCompletion)( void(*callback)(const char *s, const char *m));
	void (*Cvar_SetString)( const char *name, const char *value );
	void (*Cvar_SetLatched)( const char *name, const char *value);
	void (*Cvar_FullSet)( char *name, char *value, int flags );
	void (*Cvar_SetValue )( const char *name, float value);
	float (*Cvar_GetValue )(const char *name);
	char *(*Cvar_GetString)(const char *name);
	cvar_t *(*Cvar_FindVar)(const char *name);
	bool userinfo_modified;				// tell to client about userinfo modified
	cvar_t **cvar_vars;					// ptr to hash-table

	// console commands
	void (*Cmd_Exec)(int exec_when, const char *text);// process cmd buffer
	uint  (*Cmd_Argc)( void );
	char *(*Cmd_Args)( void );
	char *(*Cmd_Argv)( uint arg ); 
	void (*Cmd_CommandCompletion)( void(*callback)(const char *s, const char *m));
	void (*Cmd_AddCommand)(const char *name, xcommand_t function, const char *desc);
	void (*Cmd_TokenizeString)(const char *text_in);
	void (*Cmd_DelCommand)(const char *name);
	void (*Cmd_ForwardToServer)( void );				// engine callback

	// real filesystem
	file_t *(*fopen)(const char* path, const char* mode);		// same as fopen
	int (*fclose)(file_t* file);					// same as fclose
	long (*fwrite)(file_t* file, const void* data, size_t datasize);	// same as fwrite
	long (*fread)(file_t* file, void* buffer, size_t buffersize);	// same as fread, can see trough pakfile
	int (*fprint)(file_t* file, const char *msg);			// printed message into file		
	int (*fprintf)(file_t* file, const char* format, ...);		// same as fprintf
	int (*fgets)(file_t* file, byte *string, size_t bufsize );		// like a fgets, but can return EOF
	int (*fseek)(file_t* file, fs_offset_t offset, int whence);		// fseek, can seek in packfiles too
	long (*ftell)(file_t* file);					// like a ftell

	// virtual filesystem
	vfile_t *(*vfcreate)(byte *buffer, size_t buffsize);		// create virtual stream
	vfile_t *(*vfopen)(file_t *handle, const char* mode);		// virtual fopen
	file_t *(*vfclose)(vfile_t* file);				// free buffer or write dump
	long (*vfwrite)(vfile_t* file, const void* buf, size_t datasize);	// write into buffer
	long (*vfread)(vfile_t* file, void* buffer, size_t buffersize);	// read from buffer
	int  (*vfgets)(vfile_t* file, byte *string, size_t bufsize );	// read text line 
	int  (*vfprint)(vfile_t* file, const char *msg);			// write message
	int  (*vfprintf)(vfile_t* file, const char* format, ...);		// write formatted message
	int (*vfseek)(vfile_t* file, fs_offset_t offset, int whence);	// fseek, can seek in packfiles too
	bool (*vfunpack)( void* comp, size_t size1, void **buf, size_t size2);// deflate zipped buffer
	long (*vftell)(vfile_t* file);				// like a ftell

	// filesystem simply user interface
	byte *(*Com_LoadFile)(const char *path, long *filesize );		// load file into heap
	bool (*Com_WriteFile)(const char *path, void *data, long len);	// write file into disk
	rgbdata_t *(*Com_LoadImage)(const char *path, char *data, int size );	// extract image into rgba buffer
	void (*Com_SaveImage)(const char *filename, rgbdata_t *buffer );	// save image into specified format
	bool (*Com_ProcessImage)( const char *name, rgbdata_t **pix );	// convert and resample image
	void (*Com_FreeImage)( rgbdata_t *pack );			// free image buffer
	bool (*Com_LoadLibrary)( dll_info_t *dll );			// load library 
	bool (*Com_FreeLibrary)( dll_info_t *dll );			// free library
	void*(*Com_GetProcAddress)( dll_info_t *dll, const char* name );	// gpa
	double (*Com_DoubleTime)( void );				// hi-res timer

	// stdlib.c funcs
	void (*strnupr)(const char *in, char *out, size_t size_out);// convert string to upper case
	void (*strnlwr)(const char *in, char *out, size_t size_out);// convert string to lower case
	void (*strupr)(const char *in, char *out);		// convert string to upper case
	void (*strlwr)(const char *in, char *out);		// convert string to lower case
	int (*strlen)( const char *string );			// returns string real length
	int (*cstrlen)( const char *string );			// return string length without color prefixes
	char (*toupper)(const char in );			// convert one charcster to upper case
	char (*tolower)(const char in );			// convert one charcster to lower case
	size_t (*strncat)(char *dst, const char *src, size_t n);	// add new string at end of buffer
	size_t (*strcat)(char *dst, const char *src);		// add new string at end of buffer
	size_t (*strncpy)(char *dst, const char *src, size_t n);	// copy string to existing buffer
	size_t (*strcpy)(char *dst, const char *src);		// copy string to existing buffer
	char *(*stralloc)(const char *in,const char *file,int line);// create buffer and copy string here
	int (*atoi)(const char *str);				// convert string to integer
	float (*atof)(const char *str);			// convert string to float
	void (*atov)( float *dst, const char *src, size_t n );	// convert string to vector
	char *(*strchr)(const char *s, char c);			// find charcster in string at left side
	char *(*strrchr)(const char *s, char c);		// find charcster in string at right side
	int (*strnicmp)(const char *s1, const char *s2, int n);	// compare strings with case insensative
	int (*stricmp)(const char *s1, const char *s2);		// compare strings with case insensative
	int (*strncmp)(const char *s1, const char *s2, int n);	// compare strings with case sensative
	int (*strcmp)(const char *s1, const char *s2);		// compare strings with case sensative
	char *(*stristr)( const char *s1, const char *s2 );	// find s2 in s1 with case insensative
	char *(*strstr)( const char *s1, const char *s2 );	// find s2 in s1 with case sensative
	size_t (*strpack)( byte *buf, size_t pos, char *s1, int n );// include string into buffer (same as strncat)
	size_t (*strunpack)( byte *buf, size_t pos, char *s1 );	// extract string from buffer
	int (*vsprintf)(char *buf, const char *fmt, va_list args);	// format message
	int (*sprintf)(char *buffer, const char *format, ...);	// print into buffer
	char *(*va)(const char *format, ...);			// print into temp buffer
	int (*vsnprintf)(char *buf, size_t size, const char *fmt, va_list args);	// format message
	int (*snprintf)(char *buffer, size_t buffersize, const char *format, ...);	// print into buffer
	const char* (*timestamp)( int format );			// returns current time stamp
	
	// misc utils	
	gameinfo_t *GameInfo;				// user game info (filled by engine)
	char com_TXcommand;					// quark command (get rid of this)

} stdlib_api_t;

#ifndef LAUNCH_DLL
/*
==============================================================================

		STDLIB GENERIC ALIAS NAMES
  don't add aliases for launch.dll so it will be conflicted width real names
==============================================================================
*/

/*
==========================================
	memory manager funcs
==========================================
*/
#define Mem_Alloc(pool, size) std.malloc(pool, size, __FILE__, __LINE__)
#define Mem_Realloc(pool, ptr, size) std.realloc(pool, ptr, size, __FILE__, __LINE__)
#define Mem_Move(pool, ptr, data, size) std.move(pool, ptr, data, size, __FILE__, __LINE__)
#define Mem_Free(mem) std.free(mem, __FILE__, __LINE__)
#define Mem_AllocPool(name) std.mallocpool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool) std.freepool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool) std.clearpool(pool, __FILE__, __LINE__)
#define Mem_Copy(dest, src, size ) std.memcpy(dest, src, size, __FILE__, __LINE__)
#define Mem_Set(dest, src, size ) std.memset(dest, src, size, __FILE__, __LINE__)
#define Mem_Check() std.memcheck(__FILE__, __LINE__)

/*
==========================================
	parsing manager funcs
==========================================
*/
#define Com_ParseToken std.Com_ParseToken
#define Com_ParseWord std.Com_ParseWord
#define Com_Filter std.Com_Filter
#define Com_LoadScript std.Com_LoadScript
#define Com_IncludeScript std.Com_AddScript
#define Com_ResetScript std.Com_ResetScript
#define Com_GetToken std.Com_ReadToken
#define Com_TryToken std.Com_TryToken
#define Com_FreeToken std.Com_FreeToken
#define Com_SkipToken std.Com_SkipToken
#define Com_MatchToken std.Com_MatchToken
#define com_token std.com_token
#define g_TXcommand std.com_TXcommand		// get rid of this

/*
===========================================
filesystem manager
===========================================
*/
#define FS_AddGameHierarchy std.Com_AddGameHierarchy
#define FS_LoadGameInfo std.Com_LoadGameInfo
#define FS_InitRootDir std.Com_InitRootDir
#define FS_LoadFile(name, size) std.Com_LoadFile(name, size)
#define FS_Search std.Com_Search
#define FS_WriteFile(name, data, size) std.Com_WriteFile(name, data, size )
#define FS_Open( path, mode ) std.fopen( path, mode )
#define FS_Read( file, buffer, size ) std.fread( file, buffer, size )
#define FS_Write( file, buffer, size ) std.fwrite( file, buffer, size )
#define FS_StripExtension( path ) std.Com_StripExtension( path )
#define FS_DefaultExtension( path, ext ) std.Com_DefaultExtension( path, ext )
#define FS_FileExtension( ext ) std.Com_FileExtension( ext )
#define FS_FileExists( file ) std.Com_FileExists( file )
#define FS_Close( file ) std.fclose( file )
#define FS_FileBase( x, y ) std.Com_FileBase( x, y )
#define FS_Printf std.fprintf
#define FS_Print std.fprint
#define FS_Seek std.fseek
#define FS_Tell std.ftell
#define FS_Gets std.fgets
#define FS_Gamedir std.GameInfo->gamedir
#define FS_Title std.GameInfo->title
#define FS_ClearSearchPath std.Com_ClearSearchPath
#define FS_CheckParm std.Com_CheckParm
#define FS_GetParmFromCmdLine std.Com_GetParm
#define FS_LoadImage std.Com_LoadImage
#define FS_SaveImage std.Com_SaveImage
#define FS_FreeImage std.Com_FreeImage
/*
===========================================
console variables
===========================================
*/
#define Cvar_Get(name, value, flags) std.Cvar_Get(name, value, flags, "no description" ) //FIXME
#define Cvar_CommandCompletion std.Cvar_CommandCompletion
#define Cvar_Set std.Cvar_SetString
#define Cvar_FullSet std.Cvar_FullSet
#define Cvar_SetLatched std.Cvar_SetLatched
#define Cvar_SetValue std.Cvar_SetValue
#define Cvar_VariableValue std.Cvar_GetValue
#define Cvar_VariableString std.Cvar_GetString
#define Cvar_FindVar std.Cvar_FindVar
#define userinfo_modified std.userinfo_modified
#define cvar_vars *std.cvar_vars

/*
===========================================
console commands
===========================================
*/
#define Cbuf_ExecuteText std.Cmd_Exec
#define Cbuf_AddText( text ) std.Cmd_Exec( EXEC_APPEND, text )
#define Cmd_ExecuteString( text ) std.Cmd_Exec( EXEC_NOW, text )
#define Cbuf_InsertText( text ) std.Cmd_Exec( EXEC_INSERT, text )
#define Cbuf_Execute() std.Cmd_Exec( EXEC_NOW, NULL )
#define Cmd_Argc std.Cmd_Argc
#define Cmd_Args std.Cmd_Args
#define Cmd_Argv std.Cmd_Argv
#define Cmd_TokenizeString std.Cmd_TokenizeString
#define Cmd_CommandCompletion std.Cmd_CommandCompletion
#define Cmd_AddCommand std.Cmd_AddCommand
#define Cmd_RemoveCommand std.Cmd_DelCommand

/*
===========================================
virtual filesystem manager
===========================================
*/
#define VFS_Open	std.vfopen
#define VFS_Write	std.vfwrite
#define VFS_Read	std.vfread
#define VFS_Print	std.vfprint
#define VFS_Printf	std.vfprintf
#define VFS_Gets	std.vfgets
#define VFS_Seek	std.vfseek
#define VFS_Tell	std.vftell
#define VFS_Close	std.vfclose
#define VFS_Unpack	std.vfunpack

/*
===========================================
crclib manager
===========================================
*/
#define CRC_Init		std.crc_init
#define CRC_Block		std.crc_block
#define CRC_ProcessByte	std.crc_process
#define CRC_Sequence	std.crc_sequence
#define Com_BlockChecksum	std.crc_blockchecksum
#define Com_BlockChecksumKey	std.crc_blockchecksumkey

/*
===========================================
imagelib utils
===========================================
*/
#define Image_Processing std.Com_ProcessImage

/*
===========================================
misc utils
===========================================
*/
#define GI		std.GameInfo
#define Sys_LoadLibrary	std.Com_LoadLibrary
#define Sys_FreeLibrary	std.Com_FreeLibrary
#define Sys_GetProcAddress	std.Com_GetProcAddress
#define Sys_Sleep		std.sleep
#define Sys_Print		std.print
#define Sys_ConsoleInput	std.input
#define Sys_GetKeyEvents	std.keyevents
#define Sys_GetClipboardData	std.clipboard
#define Sys_Quit		std.exit
#define Sys_Break		std.abort
#define Sys_ConsoleInput	std.input
#define GetNumThreads	std.Com_NumThreads
#define ThreadLock		std.Com_ThreadLock
#define ThreadUnlock	std.Com_ThreadUnlock
#define RunThreadsOnIndividual std.Com_CreateThread

/*
===========================================
misc utils
===========================================
*/
#define timestamp		std.timestamp
#define copystring(str)	std.stralloc(str, __FILE__, __LINE__)
#define strcasecmp		std.stricmp
#define strncasecmp		std.strnicmp
#define va		std.va

#endif

/*
==============================================================================

LAUNCH.DLL INTERFACE
==============================================================================
*/
typedef struct launch_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(launch_api_t)

	void ( *Init ) ( uint funcname, int argc, char **argv ); // init host
	void ( *Main ) ( void ); // host frame
	void ( *Free ) ( void ); // close host

} launch_exp_t;

/*
==============================================================================

RENDER.DLL INTERFACE
==============================================================================
*/
typedef struct render_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(render_exp_t)

	// initialize
	bool (*Init)( void *hInstance, void *WndProc );	// init all render systems
	void (*Shutdown)( void );	// shutdown all render systems

	void	(*BeginRegistration) (char *map);
	model_t	*(*RegisterModel) (char *name);
	image_t	*(*RegisterSkin) (char *name);
	image_t	*(*RegisterPic) (char *name);
	void	(*SetSky) (char *name, float rotate, vec3_t axis);
	void	(*EndRegistration) (void);

	void	(*BeginFrame)( void );
	void	(*RenderFrame) (refdef_t *fd);
	void	(*EndFrame)( void );

	void	(*SetColor)( const float *rgba );
	bool	(*ScrShot)( const char *filename, bool force_gamma ); // write screenshot with same name 
	void	(*DrawFill)(float x, float y, float w, float h );
	void	(*DrawStretchRaw) (int x, int y, int w, int h, int cols, int rows, byte *data, bool redraw );
	void	(*DrawStretchPic)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, char *name);

	void	(*DrawGetPicSize) (int *w, int *h, char *name);	// get rid of this

} render_exp_t;

typedef struct render_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(render_imp_t)

	// client fundamental callbacks
	void	(*StudioEvent)( mstudioevent_t *event, entity_t *ent );
	void	(*ShowCollision)( void );	// debug

} render_imp_t;

/*
==============================================================================

PHYSIC.DLL INTERFACE
==============================================================================
*/
typedef struct physic_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(physic_exp_t)

	// initialize
	bool (*Init)( void );	// init all physic systems
	void (*Shutdown)( void );	// shutdown all render systems

	void (*LoadBSP)( uint *buf );	// generate tree collision
	void (*FreeBSP)( void );	// release tree collision
	void (*ShowCollision)( void );// debug
	void (*Frame)( float time );	// physics frame

	// simple objects
	void (*CreateBOX)( sv_edict_t *ed, vec3_t mins, vec3_t maxs, vec3_t org, vec3_t ang, NewtonCollision **newcol, NewtonBody **newbody );
	void (*RemoveBOX)( NewtonBody *body );

} physic_exp_t;

typedef struct physic_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(physic_imp_t)

	void (*Transform)( sv_edict_t *ed, vec3_t origin, vec3_t angles );

} physic_imp_t;

// this is the only function actually exported at the linker level
typedef void *(*launch_t)( stdlib_api_t*, void* );

#endif//REF_SYSTEM_H