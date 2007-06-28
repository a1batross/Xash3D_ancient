//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    ref_system.h - generic shared interfaces
//=======================================================================
#ifndef REF_SYSTEM_H
#define REF_SYSTEM_H

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
	byte	bitsperpixel;	// 8-16-24-32 bits
	byte	*palette;		// palette if present
	byte	*buffer;		// image buffer
} rgbdata_t;

/*
==============================================================================

FILESYSTEM ENGINE INTERFACE
==============================================================================
*/

#define FS_API_VERSION	0.31	//29 functions

typedef struct filesystem_api_s
{
	//interface validator
	float	api_version;	//must matched with FS_API_VERSION
	size_t	api_size;		//must matched with sizeof(filesystem_api_t)

	// initialize
	void (*Init)( void );
	void (*Shutdown)( void );

	//base functions
	void (*GetRootDir)( char *out );			// get root directory of engine
	void (*InitRootDir)( char *path );			// init custom rootdir 
	void (*LoadGameInfo)( const char *filename );		// gate game info from script file
	void (*FileBase)(char *in, char *out);			// get filename without path & ext
	bool (*FileExists)(const char *filename);		// return true if file exist
	long (*FileSize)(const char *filename);			// same as FileExists but return filesize
	void (*AddGameHierarchy)(const char *dir);		// add base directory in search list
	const char *(*FileExtension)(const char *in);		// return extension of file
	const char *(*FileWithoutPath)(const char *in);		// return file without path
	void (*StripExtension)(char *path);			// remove extension if present
	void (*DefaultExtension)(char *path, const char *ext );	// append extension if not present
	void (*ClearSearchPath)( void );			// delete all search pathes

	//built-in search interface
	search_t *(*Search)(const char *pattern, int casecmp );	// returned list of found files
	search_t *(*SearchDirs)(const char *pattern, int casecmp );	// returned list of found directories
	void (*FreeSearch)( search_t *search );			// free search results

	//file low-level operations
	file_t *(*Open)(const char* path, const char* mode);		// same as fread but see trough pakfile
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

} filesystem_api_t;


/*
==============================================================================

MEMORY MANAGER ENGINE INTERFACE
==============================================================================
*/

#define MEM_API_VERSION	0.26	//12 functions	

typedef struct memsystem_api_s
{
	//interface validator
	float	api_version;	//must matched with MEM_API_VERSION
	size_t	api_size;		//must matched with sizeof(memsystem_api_t)

	// initialize
	void (*Init)( void );
	void (*Shutdown)( void );

	// memsystem base functions
	byte *(*AllocPool)(const char *name, const char *file, int line);	// alloc memory pool
	void (*EmptyPool)(byte *poolptr, const char *file, int line);	// drain memory pool
	void (*FreePool)(byte **poolptr, const char *file, int line);	// release memory pool
	void (*CheckSentinelsGlobal)(const char *file, int line);		// check memory sentinels

	// user simply interface
	void *(*Alloc)(byte *pool, size_t size, const char *file, int line);			//same as malloc
	void *(*Realloc)(byte *pool, void *mem, size_t size, const char *file, int line);	//same as realloc
	void (*Move)(byte *pool, void *dest, void *src, size_t size, const char *file, int line);	//same as memmove
	void (*Copy)(void *dest, void *src, size_t size, const char *file, int line);		//same as memcpy
	void (*Set)(void *mem, int value, size_t size, const char *file, int line);		//same as memset
	void (*Free)(void *data, const char *file, int line);				//same as free

} memsystem_api_t;

/*
==============================================================================

PARSE STUFF SYSTEM INTERFACE
==============================================================================
*/

#define SCRIPT_API_VERSION	0.42	//11 functions

typedef struct scriptsystem_api_s
{
	//interface validator
	float	api_version;	//must matched with SCRIPT_API_VERSION
	size_t	api_size;		//must matched with sizeof(scriptsystem_api_t)

	// initialize
	void (*Init)( void );
	void (*Shutdown)( void );

	//user interface
	bool (*LoadScript)( const char *name, char *buf, int size );// load script into stack from file or bufer
	bool (*AddScript)( const char *name, char *buf, int size );	// include script from file or buffer
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

INTERNAL COMPILERS INTERFACE
==============================================================================
*/

#define COMPILER_API_VERSION	0.1	//6 functions

typedef struct compilers_api_s
{
	//interface validator
	float	api_version;	//must matched with COMPILER_API_VERSION
	size_t	api_size;		//must matched with sizeof(compilers_api_t)

	// initialize
	void (*Init)( void );
	void (*Shutdown)( void );

	bool (*CompileStudio)( byte *mempool, const char *name );		// input name of qc-script
	bool (*CompileSprite)( byte *mempool, const char *name );		// input name of qc-script
	bool (*CompileBSP)( const char *dir, const char *name, byte params );	// compile map in gamedir 
	int (*LoadShaderInfo)( void );

} compilers_api_t;

#endif//REF_SYSTEM_H