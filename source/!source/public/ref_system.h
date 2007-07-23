//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    ref_system.h - generic shared interfaces
//=======================================================================
#ifndef REF_SYSTEM_H
#define REF_SYSTEM_H

//bsplib compile flags
#define BSP_ONLYENTS	0x01
#define BSP_ONLYVIS		0x02
#define BSP_ONLYRAD		0x04
#define BSP_FULLCOMPILE	0x08

enum comp_format
{
	PF_UNKNOWN = 0,
	PF_INDEXED_8,	// pcx or wal images with transparent color in palette
	PF_INDEXED_24,	// studio model skins
	PF_INDEXED_32,	// sprite 32-bit palette
	PF_RGBA_32,	// already prepared ".bmp", ".tga" or ".jpg" image 
	PF_ARGB_32,	// uncompressed dds image
	PF_RGB_24,	// uncompressed dds or another 24-bit image 
	PF_DXT1,		// nvidia DXT5 format
	PF_DXT2,		// nvidia DXT5 format
	PF_DXT3,		// nvidia DXT5 format
	PF_DXT4,		// nvidia DXT5 format
	PF_DXT5,		// nvidia DXT5 format
	PF_ATI1N,		// ati 1N texture
	PF_ATI2N,		// ati 2N texture
	PF_LUMINANCE,	// b&w dds image
	PF_LUMINANCE_16,	// b&w hi-res image
	PF_LUMINANCE_ALPHA, // b&w dds image with alpha channel
	PF_RXGB,		// doom3 normal maps
	PF_ABGR_64,	// uint image
	PF_ABGR_128F,	// float image
	PF_PROCEDURE_TEX,	// internal special texture
	PF_TOTALCOUNT,	// must be last
};

typedef enum
{
	MSG_ONE,
	MSG_ALL,
	MSG_PHS,
	MSG_PVS,
	MSG_ALL_R,
	MSG_PHS_R,
	MSG_PVS_R,
} msgtype_t;

//format info table
typedef struct
{
	int	format;	// pixelformat
	char	name[8];	// used for debug
	int	bpp;	// channels (e.g. rgb = 3, rgba = 4)
	int	bpc;	// sizebytes (byte, short, float)
	int	block;	// blocksize < 0 needs alternate calc
} bpc_desc_t;

static bpc_desc_t PixelFormatDescription[] =
{
{PF_INDEXED_8,	"pal 8",  4,  1,  0 },
{PF_INDEXED_24,	"pal 24",	3,  1,  0 },
{PF_INDEXED_32,	"pal 32",	4,  1,  0 },
{PF_RGBA_32,	"RGBA",	4,  1, -4 },
{PF_ARGB_32,	"ARGB",	4,  1, -4 },
{PF_RGB_24,	"RGB",	3,  1, -3 },
{PF_DXT1,		"DXT1",	4,  1,  8 },
{PF_DXT2,		"DXT2",	4,  1, 16 },
{PF_DXT3,		"DXT3",	4,  1, 16 },
{PF_DXT4,		"DXT4",	4,  1, 16 },
{PF_DXT5,		"DXT5",	4,  1, 16 },
{PF_ATI1N,	"ATI1N",	1,  1,  8 },
{PF_ATI2N,	"3DC",	3,  1, 16 },
{PF_LUMINANCE,	"LUM 8",	1,  1, -1 },
{PF_LUMINANCE_16,	"LUM 16",	2,  2, -2 },
{PF_LUMINANCE_ALPHA,"LUM A",	2,  1, -2 },
{PF_RXGB,		"RXGB",	3,  1, 16 },
{PF_ABGR_64,	"ABGR",	4,  2, -8 },
{PF_ABGR_128F,	"ABGR128",4,  4, -16},
{PF_PROCEDURE_TEX,	"system",	4,  1, -32},
{PF_UNKNOWN,	"",	0,  0,  0 },
};

#define IMAGE_CUBEMAP	0x00000001
#define IMAGE_HAS_ALPHA	0x00000002
#define IMAGE_PREMULT	0x00000004	// indices who need in additional premultiply
#define IMAGE_GEN_MIPS	0x00000008	// must generate mips

#define CUBEMAP_POSITIVEX	0x00000400L
#define CUBEMAP_NEGATIVEX	0x00000800L
#define CUBEMAP_POSITIVEY	0x00001000L
#define CUBEMAP_NEGATIVEY	0x00002000L
#define CUBEMAP_POSITIVEZ	0x00004000L
#define CUBEMAP_NEGATIVEZ	0x00008000L

typedef struct search_s
{
	int	numfilenames;
	char	**filenames;
	char	*filenamesbuffer;
} search_t;

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
} rgbdata_t;

typedef struct gameinfo_s
{
	//filesystem info
	char basedir[128];
	char gamedir[128];
	char title[128];
          float version;
	
	int viewmode;
	int gamemode;

	//system info
	int cpunum;
	float cpufreq;
	
	char key[16];
} gameinfo_t;

/*
==============================================================================

FILESYSTEM ENGINE INTERFACE
==============================================================================
*/
typedef struct filesystem_api_s
{
	//interface validator
	size_t	api_size;		// must matched with sizeof(filesystem_api_t)

	//base functions
	void (*FileBase)(char *in, char *out);			// get filename without path & ext
	bool (*FileExists)(const char *filename);		// return true if file exist
	long (*FileSize)(const char *filename);			// same as FileExists but return filesize
	const char *(*FileExtension)(const char *in);		// return extension of file
	const char *(*FileWithoutPath)(const char *in);		// return file without path
	void (*StripExtension)(char *path);			// remove extension if present
	void (*StripFilePath)(const char* const src, char* dst);	// get file path without filename.ext
	void (*DefaultExtension)(char *path, const char *ext );	// append extension if not present
	void (*ClearSearchPath)( void );			// delete all search pathes

	//built-in search interface
	search_t *(*Search)(const char *pattern, int casecmp );	// returned list of found files
	search_t *(*SearchDirs)(const char *pattern, int casecmp );	// returned list of found directories
	void (*FreeSearch)( search_t *search );			// free search results

	//file low-level operations
	file_t *(*Open)(const char* path, const char* mode);		// same as fopen
	int (*Close)(file_t* file);					// same as fclose
	long (*Write)(file_t* file, const void* data, size_t datasize);	// same as fwrite
	long (*Read)(file_t* file, void* buffer, size_t buffersize);	// same as fread, can see trough pakfile
	int (*Print)(file_t* file, const char *msg);			// printed message into file		
	int (*Printf)(file_t* file, const char* format, ...);		// same as fprintf
	int (*Gets)(file_t* file, byte *string, size_t bufsize );		// like a fgets, but can return EOF
	int (*Seek)(file_t* file, fs_offset_t offset, int whence);		// fseek, can seek in packfiles too
	long (*Tell)(file_t* file);					// like a ftell

	//fs simply user interface
	byte *(*LoadFile)(const char *path, long *filesize );		// load file into heap
	bool (*WriteFile)(const char *filename, void *data, long len);	// write file into disk
	rgbdata_t *(*LoadImage)(const char *filename, char *data, int size );	// returned rgb data image
	void (*FreeImage)( rgbdata_t *pack );				// release image buffer

} filesystem_api_t;

typedef struct vfilesystem_api_s
{
	//interface validator
	size_t	api_size;		// must matched with sizeof(vfilesystem_api_t)

	//file low-level operations
	vfile_t *(*Create)(byte *buffer, size_t buffsize);		// create virtual stream
	vfile_t *(*Open)(file_t* file, const char* mode);			// virtual fopen
	int (*Close)(vfile_t* file);					// free buffer or write dump
	long (*Write)(vfile_t* file, const void* data, size_t datasize);	// write into buffer
	long (*Read)(vfile_t* file, void* buffer, size_t buffersize);	// read from buffer
	int (*Seek)(vfile_t* file, fs_offset_t offset, int whence);		// fseek, can seek in packfiles too
	long (*Tell)(vfile_t* file);					// like a ftell

} vfilesystem_api_t;

/*
==============================================================================

MEMORY MANAGER ENGINE INTERFACE
==============================================================================
*/
typedef struct memsystem_api_s
{
	//interface validator
	size_t	api_size;		// must matched with sizeof(memsystem_api_t)

	// memsystem base functions
	byte *(*AllocPool)(const char *name, const char *file, int line);	// alloc memory pool
	void (*EmptyPool)(byte *poolptr, const char *file, int line);	// drain memory pool
	void (*FreePool)(byte **poolptr, const char *file, int line);	// release memory pool
	void (*CheckSentinelsGlobal)(const char *file, int line);		// check memory sentinels

	// user simply interface
	void *(*Alloc)(byte *pool, size_t size, const char *file, int line);			//same as malloc
	void *(*Realloc)(byte *pool, void *mem, size_t size, const char *file, int line);	//same as realloc
	void (*Move)(void *dest, void *src, size_t size, const char *file, int line);		//same as memmove
	void (*Copy)(void *dest, void *src, size_t size, const char *file, int line);		//same as memcpy
	void (*Free)(void *data, const char *file, int line);				//same as free

} memsystem_api_t;

/*
==============================================================================

PARSE STUFF SYSTEM INTERFACE
==============================================================================
*/
typedef struct scriptsystem_api_s
{
	//interface validator
	size_t	api_size;		// must matched with sizeof(scriptsystem_api_t)

	//user interface
	bool (*Load)( const char *name, char *buf, int size );// load script into stack from file or bufer
	bool (*Include)( const char *name, char *buf, int size );	// include script from file or buffer
	char *(*GetToken)( bool newline );			// get next token on a line or newline
	char *(*Token)( void );				// just return current token
	bool (*TryToken)( void );				// return 1 if have token on a line 
	void (*FreeToken)( void );				// free current token to may get it again
	void (*SkipToken)( void );				// skip current token and jump into newline
	bool (*MatchToken)( const char *match );		// compare current token with user keyword
	char *(*ParseToken)(const char **data_p);		// parse token from char buffer

} scriptsystem_api_t;

/*
==============================================================================

NETWORK MESSAGES INTERFACE
==============================================================================
*/

typedef struct message_api_s
{
	void (*Begin)( msgtype_t type, int dest, vec3_t origin, edict_t *ent, bool reliable );
	void (*WriteChar) (int c);
	void (*WriteByte) (int c);
	void (*WriteShort) (int c);
	void (*WriteLong) (int c);
	void (*WriteFloat) (float f);
	void (*WriteString) (char *s);
	void (*WriteCoord) (vec3_t pos);	// some fractional bits
	void (*WriteDir) (vec3_t pos);	// single byte encoded, very coarse
	void (*WriteAngle) (float f);
	void (*End)( void );		//marker of end message

} message_api_t;

/*
==============================================================================

INTERNAL COMPILERS INTERFACE
==============================================================================
*/
typedef struct compilers_api_s
{
	//interface validator
	size_t	api_size;		// must matched with sizeof(compilers_api_t)

	bool (*Studio)( byte *mempool, const char *name, byte parms );	// input name of qc-script
	bool (*Sprite)( byte *mempool, const char *name, byte parms );	// input name of qc-script
	bool (*PrepareBSP)( const char *dir, const char *name, byte params );	// compile map in gamedir 
	bool (*BSP)( void );

} compilers_api_t;

/*
==============================================================================

STDIO SYSTEM INTERFACE
==============================================================================
*/
// that interface will never be expanded or extened. No need to check api_size anymore.
typedef struct stdio_api_s
{
	//interface validator
	size_t	api_size;		// must matched with sizeof(stdio_api_t)
	
	//base events
	void (*print)( char *msg );		// basic text message
	void (*printf)( char *msg, ... );	// normal text message
	void (*dprintf)( char *msg, ... );	// developer text message
	void (*error)( char *msg, ... );	// abnormal termination with message
	void (*exit)( void );		// normal silent termination
	char *(*input)( void );		// system console input	

} stdio_api_t;

/*
==============================================================================

CVAR SYSTEM INTERFACE
==============================================================================
*/
typedef struct cvar_api_s
{
	//interface validator
	size_t	api_size;		// must matched with sizeof(cvar_api_t)

	//cvar_t *(*Register)(const char *name, const char *value, int flags, const char *description );
	//cvar_t *(*SetString)(const char *name, char *value);
	//void (*SetValue)(const char *name, float value);

} cvar_api_t;

/*
==============================================================================

PLATFORM.DLL INTERFACE
==============================================================================
*/

#define PLATFORM_API_VERSION	2

typedef struct platform_api_s
{
	//interface validator
	int	apiversion;	// must matched with PLATFORM_API_VERSION
	size_t	api_size;		// must matched with sizeof(platform_api_t)

	// initialize
	void (*Init)( void );	// init all platform systems
	void (*Shutdown)( void );	// shutdown all platform systems

	//platform systems
	filesystem_api_t	Fs;
	vfilesystem_api_t	VFs;
	memsystem_api_t	Mem;
	scriptsystem_api_t	Script;
	compilers_api_t	Compile;

	// path initialization
	void (*InitRootDir)( char *path );		// init custom rootdir 
	void (*LoadGameInfo)( const char *filename );	// gate game info from script file
	void (*AddGameHierarchy)(const char *dir);	// add base directory in search list

	//misc utils
	double (*DoubleTime)( void );
          gameinfo_t (*GameInfo)( void );

} platform_api_t;

typedef platform_api_t (*platform_t)( stdio_api_t );

#endif//REF_SYSTEM_H