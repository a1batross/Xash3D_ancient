//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    ref_system.h - generic shared interfaces
//=======================================================================
#ifndef REF_SYSTEM_H
#define REF_SYSTEM_H

#include "studio.h"
#include "sprite.h"
#include "bspmodel.h" 
#include "version.h"


#define RENDERER_API_VERSION	4
#define PLATFORM_API_VERSION	2
#define LAUNCHER_API_VERSION	2


//bsplib compile flags
#define BSP_ONLYENTS	0x01
#define BSP_ONLYVIS		0x02
#define BSP_ONLYRAD		0x04
#define BSP_FULLCOMPILE	0x08

#define MAX_DLIGHTS		32
#define MAX_ENTITIES	128
#define MAX_PARTICLES	4096
#define MAX_LIGHTSTYLES	256

#define ENTITY_FLAGS	68
#define POWERSUIT_SCALE	4.0F

#define SHELL_RED_COLOR	0xF2
#define SHELL_GREEN_COLOR	0xD0
#define SHELL_BLUE_COLOR	0xF3

#define SHELL_RG_COLOR	0xDC
#define SHELL_RB_COLOR	0x68
#define SHELL_BG_COLOR	0x78

//ROGUE
#define SHELL_DOUBLE_COLOR	0xDF // 223
#define SHELL_HALF_DAM_COLOR	0x90
#define SHELL_CYAN_COLOR	0x72
//ROGUE
#define SHELL_WHITE_COLOR	0xD7

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
	MSG_ONE_R,	// reliable
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

#define CVAR_ARCHIVE	1	// set to cause it to be saved to vars.rc
#define CVAR_USERINFO	2	// added to userinfo  when changed
#define CVAR_SERVERINFO	4	// added to serverinfo when changed
#define CVAR_NOSET		8	// don't allow change from console at all, but can be set from the command line
#define CVAR_LATCH		16	// save changes until server restart

#define CVAR_MAXFLAGSVAL	31	// maximum number of flags

typedef struct cvar_s
{
	int	flags;
	char	*name;

	char	*string;
	char	*description;
	int	integer;
	float	value;
	float	vector[3];
	char	*defstring;

	struct cvar_s *next;
	struct cvar_s *hash;

	//FIXME: remove these old variables
	char	*latched_string;	// for CVAR_LATCH vars
	bool	modified;		// set each time the cvar is changed
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

INFOSTRING MANAGER ENGINE INTERFACE
==============================================================================
*/
typedef struct infostring_api_s
{
	//interface validator
	size_t	api_size;		// must matched with sizeof(infostring_api_t)

	void (*Print) (char *s);
	bool (*Validate) (char *s);
	void (*RemoveKey) (char *s, char *key);
	char *(*ValueForKey) (char *s, char *key);
	void (*SetValueForKey) (char *s, char *key, char *value);

} infostring_api_t;


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

typedef struct message_write_s
{
	//interface validator
	size_t	api_size;				// must matched with sizeof(message_write_t)
	
	void (*Begin)( int dest );			// marker of start message
	void (*WriteChar) (int c);
	void (*WriteByte) (int c);
	void (*WriteWord) (int c);
	void (*WriteShort) (int c);
	void (*WriteLong) (int c);
	void (*WriteFloat) (float f);
	void (*WriteString) (char *s);
	void (*WriteCoord) (vec3_t pos);		// some fractional bits
	void (*WriteDir) (vec3_t pos);		// single byte encoded, very coarse
	void (*WriteAngle) (float f);
	void (*Send)( msgtype_t type, vec3_t origin, edict_t *ent );// end of message
} message_write_t;

typedef struct message_read_s
{
	//interface validator
	size_t	api_size;				// must matched with sizeof(message_read_t)
	
	void (*Begin)( void );			// begin reading
	int (*ReadChar) ( void );
	int (*ReadByte) ( void );
	int (*ReadLong) ( void );
	int (*ReadShort) ( void );
	float *(*ReadDir) ( void );			// return value from anorms.h
	float (*ReadFloat) ( void );
	float (*ReadAngle) ( void );
	void *(*ReadData) (int len );
	float *(*ReadCoord) ( void );			// x, y, z coords
	char *(*ReadString) ( bool line );		// get line once only
	void (*End)( void );			// message received
} message_read_t;

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
typedef struct stdinout_api_s
{
	//interface validator
	size_t	api_size;			// must matched with sizeof(stdinout_api_t)
	
	//base events
	void (*print)( char *msg );		// basic text message
	void (*printf)( char *msg, ... );	// normal text message
	void (*dprintf)( char *msg, ... );	// developer text message
	void (*wprintf)( char *msg, ... );	// warning text message
	void (*error)( char *msg, ... );	// abnormal termination with message
	void (*exit)( void );		// normal silent termination
	char *(*input)( void );		// system console input	

} stdinout_api_t;

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

LAUNCHER.DLL INTERFACE
==============================================================================
*/
typedef struct launcher_exp_s
{
	//interface validator
	int	apiversion;	// must matched with LAUNCHER_API_VERSION
	size_t	api_size;		// must matched with sizeof(launcher_api_t)

	void ( *Init ) ( char *funcname, int argc, char **argv ); // init host
	void ( *Main ) ( void ); // host frame
	void ( *Free ) ( void ); // close host

} launcher_exp_t;

/*
==============================================================================

PLATFORM.DLL INTERFACE
==============================================================================
*/

typedef struct platform_exp_s
{
	//interface validator
	int	apiversion;	// must matched with PLATFORM_API_VERSION
	size_t	api_size;		// must matched with sizeof(platform_api_t)

	// initialize
	bool (*Init)( int argc, char **argv );	// init all platform systems
	void (*Shutdown)( void );	// shutdown all platform systems

	//platform systems
	filesystem_api_t	Fs;
	vfilesystem_api_t	VFs;
	memsystem_api_t	Mem;
	scriptsystem_api_t	Script;
	compilers_api_t	Compile;
	infostring_api_t	Info;

	// path initialization
	void (*InitRootDir)( char *path );		// init custom rootdir 
	void (*LoadGameInfo)( const char *filename );	// gate game info from script file
	void (*AddGameHierarchy)(const char *dir);	// add base directory in search list

	//misc utils
	double (*DoubleTime)( void );
	gameinfo_t (*GameInfo)( void );

} platform_exp_t;

/*
==============================================================================

RENDERER.DLL INTERFACE
==============================================================================
*/

typedef struct renderer_exp_s
{
	//interface validator
	int	apiversion;	// must matched with RENDERER_API_VERSION
	size_t	api_size;		// must matched with sizeof(renderer_exp_t)

	// initialize
	bool (*Init)( void *hInstance, void *WndProc );	// init all renderer systems
	void (*Shutdown)( void );	// shutdown all renderer systems

	void	(*BeginRegistration) (char *map);
	model_t	*(*RegisterModel) (char *name);
	image_t	*(*RegisterSkin) (char *name);
	image_t	*(*RegisterPic) (char *name);
	void	(*SetSky) (char *name, float rotate, vec3_t axis);
	void	(*EndRegistration) (void);

	void	(*RenderFrame) (refdef_t *fd);

	void	(*DrawGetPicSize) (int *w, int *h, char *name);	// will return 0 0 if not found
	void	(*DrawPic) (int x, int y, char *name);
	void	(*DrawStretchPic) (int x, int y, int w, int h, char *name);
	void	(*DrawChar) (int x, int y, int c);
	void	(*DrawString) (int x, int y, char *str);
	void	(*DrawTileClear) (int x, int y, int w, int h, char *name);
	void	(*DrawFill) (int x, int y, int w, int h, int c);
	void	(*DrawFadeScreen) (void);

	// Draw images for cinematic rendering (which can have a different palette). Note that calls
	void	(*DrawStretchRaw) (int x, int y, int w, int h, int cols, int rows, byte *data);

	// video mode and refresh state management entry points
	void	(*CinematicSetPalette)( const byte *palette);	// NULL = game palette
	void	(*BeginFrame)( float camera_separation );
	void	(*EndFrame) (void);

	void	(*AppActivate)( bool activate );		// ??

} renderer_exp_t;

typedef struct renderer_imp_s
{
	//shared xash systems
	filesystem_api_t	Fs;
	vfilesystem_api_t	VFs;
	memsystem_api_t	Mem;
	scriptsystem_api_t	Script;
	compilers_api_t	Compile;
	stdinout_api_t	Stdio;

	void	(*Cmd_AddCommand) (char *name, void(*cmd)(void));
	void	(*Cmd_RemoveCommand) (char *name);
	int	(*Cmd_Argc) (void);
	char	*(*Cmd_Argv) (int i);
	void	(*Cmd_ExecuteText) (int exec_when, char *text);

	//client fundamental callbacks
          void	(*StudioEvent)( mstudioevent_t *event, entity_t *ent );

	// gamedir will be the current directory that generated
	// files should be stored to, ie: "f:\quake\id1"
	char	*(*gamedir)	( void );
	char	*(*title)		( void );

	cvar_t	*(*Cvar_Get) (char *name, char *value, int flags);
	cvar_t	*(*Cvar_Set)( char *name, char *value );
	void	(*Cvar_SetValue)( char *name, float value );

	bool	(*Vid_GetModeInfo)( int *width, int *height, int mode );
	void	(*Vid_MenuInit)( void );
	void	(*Vid_NewWindow)( int width, int height );

} renderer_imp_t;


// this is the only function actually exported at the linker level
typedef renderer_exp_t *(*renderer_t)( renderer_imp_t );
typedef platform_exp_t *(*platform_t)( stdinout_api_t );
typedef launcher_exp_t *(*launcher_t)( stdinout_api_t );

#endif//REF_SYSTEM_H