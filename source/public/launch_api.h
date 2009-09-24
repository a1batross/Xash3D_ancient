//=======================================================================
//			Copyright XashXT Group 2008 ©
//		      launch_api.h - main header for all dll's
//=======================================================================
#ifndef LAUNCH_APH_H
#define LAUNCH_APH_H

// disable some warnings
#pragma warning(disable : 4244)	// MIPS
#pragma warning(disable : 4018)	// signed/unsigned mismatch
#pragma warning(disable : 4305)	// truncation from const double to float

#define false		0
#define true		1
#define STRING_COLOR_TAG	'^'
#define MAX_STRING		256	// generic string
#define MAX_SYSPATH		1024	// system filepath
#define MAX_MSGLEN		32768	// max length of network message
#define IsColorString( p )	( p && *(p) == STRING_COLOR_TAG && *((p)+1) && *((p)+1) != STRING_COLOR_TAG )
#define bound(min, num, max)	((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))
#define MAX_STRING_TABLES	8	// seperately stringsystems
#define MAX_MODS		128	// environment games that engine can keep visible
#ifndef __cplusplus
#define bool		BOOL	// sizeof( int )
#endif

#include "basetypes.h"

typedef vec_t		vec2_t[2];
typedef vec_t		vec3_t[3];
typedef vec_t		vec4_t[4];
typedef vec_t		quat_t[4];
typedef byte		rgba_t[4];	// unsigned byte colorpack
typedef byte		rgb_t[3];		// unsigned byte colorpack
typedef vec_t		matrix3x3[3][3];
typedef vec_t		matrix4x4[4][4];
typedef char		string[MAX_STRING];
typedef struct pr_edict_s	pr_edict_t;
typedef struct sv_edict_s	sv_edict_t;
typedef struct sv_entvars_s	sv_entvars_t;
typedef struct sv_globalvars_s sv_globalvars_t;
typedef struct physbody_s	physbody_t;

// platform instances
typedef enum
{	
	HOST_OFFLINE = 0,	// host_init( g_Instance ) same much as:
	HOST_CREDITS,	// "splash"	"©anyname"	(easter egg)
	HOST_DEDICATED,	// "normal"	"#gamename"
	HOST_NORMAL,	// "normal"	"gamename"
	HOST_BSPLIB,	// "bsplib"	"bsplib"
	HOST_QCCLIB,	// "qcclib"	"qcc"
	HOST_SPRITE,	// "sprite"	"spritegen"
	HOST_STUDIO,	// "studio"	"studiomdl"
	HOST_WADLIB,	// "wadlib"	"xwad"
	HOST_RIPPER,	// "ripper"	"extragen"
	HOST_XIMAGE,	// "ximage"	"ximage"
	HOST_COUNTS,	// terminator
} instance_t;

enum dev_level
{
	D_INFO = 1,	// "-dev 1", shows various system messages
	D_WARN,		// "-dev 2", shows not critical system warnings
	D_ERROR,		// "-dev 3", shows critical warnings 
	D_LOAD,		// "-dev 4", show messages about loading resources
	D_NOTE,		// "-dev 5", show system notifications for engine develeopers
	D_MEMORY,		// "-dev 6", writes to log memory allocation
	D_STRING,		// "-dev 7", writes to log tempstrings allocation
};

typedef long fs_offset_t;
typedef struct file_s file_t;		// normal file
typedef struct vfile_s vfile_t;	// virtual file
typedef struct wfile_s wfile_t;	// wad file
typedef struct script_s script_t;	// script machine
typedef struct { const char *name; void **func; } dllfunc_t; // Sys_LoadLibrary stuff
typedef struct { int numfilenames; char **filenames; char *filenamesbuffer; } search_t;
typedef struct { int ofs; int type; const char *name; } fields_t;	// prvm custom fields
typedef void ( *cmsave_t )( void* handle, const void* buffer, size_t size );
typedef void ( *cmdraw_t )( rgba_t color, int numpoints, const float *points, const int *elements );
typedef void ( *setpair_t )( const char *key, const char *value, void *buffer, void *numpairs );
typedef enum { mod_bad, mod_world, mod_brush, mod_alias, mod_studio, mod_sprite } modtype_t;
typedef enum { NA_LOOPBACK, NA_BROADCAST, NA_IP } netadrtype_t;
typedef enum { eXYZ, eYZX, eZXY, eXZY, eYXZ, eZYX } euler_t;
typedef enum { NS_CLIENT, NS_SERVER } netsrc_t;
typedef void ( *xcommand_t )( void );

// command buffer modes
enum
{
	EXEC_NOW	= 0,
	EXEC_INSERT,
	EXEC_APPEND,
};

// timestamp modes
enum
{
	TIME_FULL = 0,
	TIME_DATE_ONLY,
	TIME_TIME_ONLY,
	TIME_NO_SECONDS,
	TIME_YEAR_ONLY,
	TIME_FILENAME,
};

// cvar flags
typedef enum
{
	CVAR_ARCHIVE	= BIT(0),	// set to cause it to be saved to vars.rc
	CVAR_USERINFO	= BIT(1),	// added to userinfo  when changed
	CVAR_SERVERINFO	= BIT(2),	// added to serverinfo when changed
	CVAR_SYSTEMINFO	= BIT(3),	// don't changed from console, saved into config.rc
	CVAR_INIT		= BIT(4), // don't allow change from console at all, but can be set from the command line
	CVAR_LATCH	= BIT(5),	// save changes until server restart
	CVAR_READ_ONLY	= BIT(6),	// display only, cannot be set by user at all
	CVAR_USER_CREATED	= BIT(7),	// created by a set command (prvm used)
	CVAR_TEMP		= BIT(8),	// can be set even when cheats are disabled, but is not archived
	CVAR_CHEAT	= BIT(9),	// can not be changed if cheats are disabled
	CVAR_NORESTART	= BIT(10),// do not clear when a cvar_restart is issued
	CVAR_LATCH_VIDEO	= BIT(11),// save changes until render restart
} cvar_flags_t;

typedef struct
{
	netadrtype_t	type;
	byte		ip[4];
	byte		ipx[10];
	word		port;
} netadr_t;

typedef struct sizebuf_s		// FIXME: rename to netbuf
{
	bool	overflowed;	// set to true if the buffer size failed

	byte	*data;
	int	maxsize;
	int	cursize;
	int	readcount;
	bool	error;
} sizebuf_t;

/*
========================================================================

TEXT PARSER

all engine parts using it
========================================================================
*/
typedef enum
{
	TT_EMPTY = 0,		// empty (invalid or whitespace)
	TT_GENERIC,		// generic string separated by spaces
	TT_STRING,		// string (enclosed with double quotes)
	TT_LITERAL,		// literal (enclosed with single quotes)
	TT_NUMBER,		// number
	TT_NAME,			// name
	TT_PUNCTUATION		// punctuation
} tokenType_t;

typedef enum
{
	SC_ALLOW_NEWLINES	 	= BIT(0),	// same as 'true'
	SC_ALLOW_STRINGCONCAT	= BIT(1),
	SC_ALLOW_ESCAPECHARS 	= BIT(2),
	SC_ALLOW_PATHNAMES		= BIT(3),
	SC_ALLOW_PATHNAMES2		= BIT(4),	// allow pathnames with quake symbols (!, %, $, +, -, { )
	SC_PARSE_GENERIC		= BIT(5),
	SC_PRINT_ERRORS		= BIT(6),
	SC_PRINT_WARNINGS	 	= BIT(7),
	SC_PARSE_LINE		= BIT(8),	// read line, ignore whitespaces
	SC_COMMENT_SEMICOLON	= BIT(9),	// using semicolon as mark or begin comment (q2 oldstyle)
} scFlags_t;

typedef struct
{
	tokenType_t	type;
	uint		subType;
	int		line;
	char		string[MAX_SYSPATH];
	int		length;
	double		floatValue;
	uint		integerValue;
} token_t;

/*
========================================================================

SYS EVENT

keep console cmds, network messages, mouse reletives and key buttons
========================================================================
*/
typedef enum
{
	SE_NONE = 0,	// end of events queue
	SE_KEY,		// ev.value[0] is a key code, ev.value[1] is the down flag
	SE_CHAR,		// ev.value[0] is an ascii char
	SE_MOUSE,		// ev.value[0] and ev.value[1] are reletive signed x / y moves
	SE_JOYSTICK,	// evValue is an axis number and evValue2 is the current state (-127 to 127)
	SE_CONSOLE,	// ev.data is a char*
	SE_PACKET,	// ev.data is a netadr_t followed by data bytes to ev.length
} ev_type_t;

typedef struct
{
	ev_type_t		type;
	int		time;	// actual timestamp
	int		value[2];
	void		*data;
	size_t		length;
} sys_event_t;

/*
========================================================================

GAMEINFO stuff

internal shared gameinfo structure (readonly for engine parts)
========================================================================
*/
typedef struct gameinfo_s
{
	// filesystem info
	string		gamefolder;	// used for change game '-game x'
	string		basedir;		// main game directory (like 'id1' for Quake or 'valve' for Half-Life)
	string		gamedir;		// game directory (can be match with basedir, used as primary dir and as write path
	string		vsrcdir;		// virtual machine source directory (qcc know when give source files)
	string		startmap;		// map to start singleplayer game
	string		title;		// Game Main Title
	float		version;		// game version (optional)

	int		viewmode;
	int		gamemode;

	string		sp_entity;	// e.g. info_player_start
	string		dm_entity;	// e.g. info_player_deathmatch
	string		ctf_entity;	// e.g. info_player_ctf
	string		team_entity;	// e.g. info_player_team

	string		imglib_mode;	// image formats to using (optional)
} gameinfo_t;

typedef struct sysinfo_s
{
	string		username;		// OS current username
	float		version;		// engine version
	int		cpunum;		// count of cpu's
	float		cpufreq;		// cpu frequency in MHz
	char		instance;		// global engine instance

	gameinfo_t	*GameInfo;	// current GameInfo
	gameinfo_t	*games[MAX_MODS];	// environment games (founded at each engine start)
	int		numgames;
} sysinfo_t;

/*
========================================================================
internal dll's loader

two main types - native dlls and other win32 libraries will be recognized automatically
NOTE: never change this structure because all dll descriptions in xash code
writes into struct by offsets not names
========================================================================
*/
typedef struct dll_info_s
{
	// generic interface
	const char	*name;	// library name
	const dllfunc_t	*fcts;	// list of dll exports	
	const char	*entry;	// entrypoint name (internal libs only)
	void		*link;	// hinstance of loading library

	// xash interface
	void		*(*main)( void*, void* );
	bool		crash;	// crash if dll not found

	size_t		api_size;	// interface size
	size_t		com_size;	// main interface size == sizeof( stdilib_api_t )
} dll_info_t;

/*
========================================================================

internal image format

typically expanded to rgba buffer
NOTE: number at end of pixelformat name it's a total bitscount e.g. PF_RGB_24 == PF_RGB_888
========================================================================
*/

typedef enum
{
	PF_UNKNOWN = 0,
	PF_INDEXED_24,	// inflated palette (768 bytes)
	PF_INDEXED_32,	// deflated palette (1024 bytes)
	PF_RGBA_32,	// normal rgba buffer
	PF_BGRA_32,	// big endian RGBA (MacOS)
	PF_ARGB_32,	// uncompressed dds image
	PF_ABGR_64,	// uint image
	PF_RGB_24,	// uncompressed dds or another 24-bit image 
	PF_BGR_24,	// big-endian RGB (MacOS)
	PF_RGB_16,	// 5-6-5 weighted image
	PF_DXT1,		// nvidia DXT1 format
	PF_DXT2,		// nvidia DXT2 format
	PF_DXT3,		// nvidia DXT3 format
	PF_DXT4,		// nvidia DXT5 format
	PF_DXT5,		// nvidia DXT5 format
	PF_RXGB,		// doom3 normal maps
	PF_ATI1N,		// ati 1N texture
	PF_ATI2N,		// ati 2N texture
	PF_LUMINANCE,	// b&w dds image
	PF_LUMINANCE_16,	// b&w hi-res image
	PF_LUMINANCE_ALPHA, // b&w dds image with alpha channel
	PF_UV_16,		// EMBM two signed 8bit components
	PF_UV_32,		// EMBM two signed 16bit components
	PF_R_16F,		// red channel half-float image
	PF_R_32F,		// red channel float image
	PF_GR_32F,	// Green-Red channels half-float image (dudv maps)
	PF_GR_64F,	// Green-Red channels float image (dudv maps)
	PF_ABGR_64F,	// ABGR half-float image
	PF_ABGR_128F,	// ABGR float image
	PF_RGBA_GN,	// internal generated texture
	PF_TOTALCOUNT,	// must be last
} pixformat_t;

typedef struct bpc_desc_s
{
	int	format;	// pixelformat
	char	name[16];	// used for debug
	uint	glFormat;	// RGBA format
	uint	glType;	// pixel size (byte, short etc)
	int	bpp;	// channels (e.g. rgb = 3, rgba = 4)
	int	bpc;	// sizebytes (byte, short, float)
	int	block;	// blocksize < 0 needs alternate calc
} bpc_desc_t;

// imagelib global settings
typedef enum
{
	IL_DDS_HARDWARE	= BIT(0),	// instance have dds hardware support (disable software unpacker)
	IL_ATI_FLOAT_EXT	= BIT(1),	// reinstall float images glmask for ati extensions
	IL_NV_FLOAT_EXT	= BIT(2),	// reinstall float images glmask for nv extensions
	IL_USE_LERPING	= BIT(3),	// lerping images during resample
	IL_KEEP_8BIT	= BIT(4),	// don't expand paletted images
	IL_ALLOW_OVERWRITE	= BIT(5),	// allow to overwrite stored images
	IL_IGNORE_MIPS	= BIT(6),	// ignore mip-levels to loading
} ilFlags_t;

// rgbdata output flags
typedef enum
{
	// rgbdata->flags
	IMAGE_CUBEMAP	= BIT(0),		// it's 6-sides cubemap buffer
	IMAGE_HAS_ALPHA	= BIT(1),		// image contain alpha-channel
	IMAGE_HAS_COLOR	= BIT(2),		// image contain RGB-channel
	IMAGE_COLORINDEX	= BIT(3),		// all colors in palette is gradients of last color (decals)
	IMAGE_PREMULT	= BIT(4),		// need to premultiply alpha (DXT2, DXT4)
	IMAGE_HAS_LUMA_Q1	= BIT(5),		// image has luma pixels (q1-style maps)
	IMAGE_HAS_LUMA_Q2	= BIT(6),		// image has luma pixels (q2-style maps)
	IMAGE_HAS_LUMA	= IMAGE_HAS_LUMA_Q1|IMAGE_HAS_LUMA_Q2,

	// Image_Process manipulation flags
	IMAGE_FLIP_X	= BIT(12),	// flip the image by width
	IMAGE_FLIP_Y	= BIT(13),	// flip the image by height
	IMAGE_ROT_90	= BIT(14),	// flip from upper left corner to down right corner
	IMAGE_ROT180	= IMAGE_FLIP_X|IMAGE_FLIP_Y,
	IMAGE_ROT270	= IMAGE_FLIP_X|IMAGE_FLIP_Y|IMAGE_ROT_90,	
	IMAGE_ROUND	= BIT(15),	// round image to nearest Pow2
	IMAGE_RESAMPLE	= BIT(16),	// resample image to specified dims
	IMAGE_PALTO24	= BIT(17),	// turn 32-bit palette into 24-bit mode (only for indexed images)
	IMAGE_COMP_DXT	= BIT(18),	// compress image to DXT format
	IMAGE_ROUNDFILLER	= BIT(19),	// round image to nearest Pow2 and fill unused entries with single color	
	IMAGE_FORCE_RGBA	= BIT(20),	// force image to RGBA buffer
	IMAGE_MAKE_LUMA	= BIT(21),	// create luma texture from indexed
} imgFlags_t;

typedef struct rgbdata_s
{
	word	width;		// image width
	word	height;		// image height
	word	depth;		// multi-layer volume
	byte	numMips;		// mipmap count
	byte	bitsCount;	// RGB bits count
	uint	type;		// compression type
	uint	flags;		// misc image flags
	byte	*palette;		// palette if present
	byte	*buffer;		// image buffer
	size_t	size;		// for bounds checking
} rgbdata_t;

// filesystem flags
#define FS_STATIC_PATH	1	// FS_ClearSearchPath will be ignore this path
#define FS_NOWRITE_PATH	2	// default behavior - last added gamedir set as writedir. This flag disables it

/*
==============================================================================

STDLIB SYSTEM INTERFACE
==============================================================================
*/
typedef struct stdilib_api_s
{
	// interface validator
	size_t		api_size;				// must matched with sizeof(launch_exp_t)
	size_t		com_size;				// must matched with sizeof(stdlib_api_t)

	sysinfo_t		*SysInfo;				// engine sysinfo (filled by engine)

	// base events
	void (*instance)( const char *name, const char *fmsg );	// restart engine with new instance
	void (*print)( const char *msg );			// basic text message
	void (*printf)( const char *msg, ... );			// formatted text message
	void (*dprintf)( int level, const char *msg, ...);	// developer text message
	void (*error)( const char *msg, ... );			// abnormal termination with message
	void (*abort)( const char *msg, ... );			// normal tremination with message
	void (*exit)( void );				// normal silent termination
	void (*sleep)( int msec );				// sleep for some msec
	char *(*clipboard)( void );				// get clipboard data
	void (*queevent)( int time, ev_type_t type, int value, int value2, int length, void *ptr );
	sys_event_t (*getevent)( void );			// get system events

	// crclib.c funcs
	void (*crc_init)(word *crcvalue);			// set initial crc value
	word (*crc_block)(byte *start, int count);		// calculate crc block
	void (*crc_process)(word *crcvalue, byte data);		// process crc byte
	byte (*crc_sequence)(byte *base, int length, int sequence);	// calculate crc for sequence
	uint (*crc_blockchecksum)(void *buffer, int length);	// map checksum
	uint (*crc_blockchecksumkey)(void *buf, int len, int key);	// process key checksum

	// memlib.c funcs
	void (*memcpy)(void *dest, const void *src, size_t size, const char *file, int line);
	void (*memset)(void *dest, int set, size_t size, const char *filename, int fileline);
	void *(*realloc)(byte *pool, void *mem, size_t size, const char *file, int line);
	void (*move)(byte *pool, void **dest, void *src, size_t size, const char *file, int line); // not a memmove
	void *(*malloc)(byte *pool, size_t size, const char *file, int line);
	void (*free)(void *data, const char *file, int line);

	// xash memlib extension - memory pools
	byte *(*mallocpool)(const char *name, const char *file, int line);
	void (*freepool)(byte **poolptr, const char *file, int line);
	void (*clearpool)(byte *poolptr, const char *file, int line);
	void (*memcheck)(const char *file, int line);		// check memory pools for consistensy

	// xash memlib extension - memory arrays
	byte *(*newarray)( byte *pool, size_t elementsize, int count, const char *file, int line );
	void (*delarray)( byte *array, const char *file, int line );
	void *(*newelement)( byte *array, const char *file, int line );
	void (*delelement)( byte *array, void *element, const char *file, int line );
	void *(*getelement)( byte *array, size_t index );
	size_t (*arraysize)( byte *arrayptr );

	// network.c funcs
	void (*NET_Init)( void );
	void (*NET_Shutdown)( void );
	void (*NET_Sleep)( float time );
	void (*NET_Config)( bool net_enable );
	char *(*NET_AdrToString)( netadr_t a );
	bool (*NET_IsLocalAddress)( netadr_t adr );
	char *(*NET_BaseAdrToString)( const netadr_t a );
	bool (*NET_StringToAdr)( const char *s, netadr_t *a );
	bool (*NET_CompareAdr)( const netadr_t a, const netadr_t b );
	bool (*NET_CompareBaseAdr)( const netadr_t a, const netadr_t b );
	bool (*NET_GetPacket)( netsrc_t sock, netadr_t *from, sizebuf_t *msg );
	void (*NET_SendPacket)( netsrc_t sock, size_t length, const void *data, netadr_t to );

	// common functions
	void (*Com_InitRootDir)( char *path );			// init custom rootdir 
	void (*Com_LoadGameInfo)( const char *rootfolder );	// initialize gamedir
	void (*Com_AddGameHierarchy)( const char *dir, int flags );	// add base directory in search list
	void (*Com_AllowDirectPaths)( bool enable );		// allow direct paths e.g. C:\windows
	int  (*Com_CheckParm)( const char *parm );		// check parm in cmdline  
	bool (*Com_GetParm)( char *parm, char *out, size_t size );	// get parm from cmdline
	void (*Com_FileBase)(const char *in, char *out);		// get filename without path & ext
	bool (*Com_FileExists)(const char *filename);		// return true if file exist
	long (*Com_FileSize)(const char *filename);		// same as Com_FileExists but return filesize
	long (*Com_FileTime)(const char *filename);		// same as Com_FileExists but return filetime
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

	// script machine base functions
	script_t *(*Com_OpenScript)( const char *filename, const char *buf, size_t size );
	void (*Com_CloseScript)( script_t *script );		// release current script
	void (*Com_ResetScript)( script_t *script );		// jump to start of scriptfile 
	bool (*Com_EndOfScript)( script_t *script );		// returns true if end of script reached
	void (*Com_SkipBracedSection)(script_t *script, int depth);	// skip braced section with specified depth
	void (*Com_SkipRestOfLine)( script_t *script );		// skip all tokene the rest of line
	bool (*Com_ReadToken)( script_t *script, scFlags_t flags, token_t *token ); // generic reading
	void (*Com_SaveToken)( script_t *script, token_t *token );	// save current token to get it again

	// script machine simple user interface
	bool (*Com_ReadString)( script_t *script, int flags, char *value, size_t size );// string
	bool (*Com_ReadFloat)( script_t *script, int flags, float *value );		// float value
	bool (*Com_ReadDword)( script_t *script, int flags, uint *value );		// unsigned integer
	bool (*Com_ReadLong)( script_t *script, int flags, int *value );		// signed integer

	// patch extension
	void (*PatchEval)( const float *p, int *numcp, const int *tess, float *dest, int comp );
	void (*PatchFlat)( float maxflat, const float *points, int comp, const int *patch_cp, int *flat );

	search_t *(*Com_Search)( const char *pattern, int casecmp ); // returned list of found files
	uint (*Com_HashKey)( const char *string, uint hashSize );	// returns hash key for a string
	byte *(*Com_LoadRes)( const char *filename, size_t *size );	// find internal resource in baserc.dll 

	// console variables
	cvar_t *(*Cvar_Get)(const char *name, const char *value, int flags, const char *desc);
	void (*Cvar_LookupVars)( int checkbit, void *buffer, void *ptr, setpair_t callback );
	void (*Cvar_SetString)( const char *name, const char *value );
	void (*Cvar_SetLatched)( const char *name, const char *value);
	void (*Cvar_FullSet)( char *name, char *value, int flags );
	void (*Cvar_SetValue )( const char *name, float value);
	long (*Cvar_GetInteger)(const char *name);
	float (*Cvar_GetValue )(const char *name);
	char *(*Cvar_GetString)(const char *name);
	cvar_t *(*Cvar_FindVar)(const char *name);
	bool userinfo_modified;				// tell to client about userinfo modified

	// console commands
	void (*Cmd_Exec)(int exec_when, const char *text);	// process cmd buffer
	uint  (*Cmd_Argc)( void );
	char *(*Cmd_Args)( void );
	char *(*Cmd_Argv)( uint arg ); 
	void (*Cmd_LookupCmds)( char *buffer, void *ptr, setpair_t callback );
	void (*Cmd_AddCommand)(const char *name, xcommand_t function, const char *desc);
	void (*Cmd_TokenizeString)(const char *text_in);
	void (*Cmd_DelCommand)(const char *name);

	// real filesystem
	file_t *(*fopen)(const char* path, const char* mode);		// same as fopen
	int (*fclose)(file_t* file);					// same as fclose
	long (*fwrite)(file_t* file, const void* data, size_t datasize);	// same as fwrite
	long (*fread)(file_t* file, void* buffer, size_t buffersize);	// same as fread, can see trough pakfile
	int (*fprint)(file_t* file, const char *msg);			// printed message into file		
	int (*fprintf)(file_t* file, const char* format, ...);		// same as fprintf
	int (*fgetc)( file_t* file );					// same as fgetc
	int (*fgets)(file_t* file, byte *string, size_t bufsize );		// like a fgets, but can return EOF
	int (*fseek)(file_t* file, fs_offset_t offset, int whence);		// fseek, can seek in packfiles too
	bool (*fremove)( const char *path );				// remove sepcified file
	long (*ftell)(file_t* file);					// like a ftell
	bool (*feof)(file_t* file);					// like a feof

	// virtual filesystem
	vfile_t *(*vfcreate)( const byte *buffer, size_t buffsize );	// create virtual stream
	vfile_t *(*vfopen)(file_t *handle, const char* mode);		// virtual fopen
	file_t *(*vfclose)(vfile_t* file);				// free buffer or write dump
	long (*vfwrite)(vfile_t* file, const void* buf, size_t datasize);	// write into buffer
	long (*vfread)(vfile_t* file, void* buffer, size_t buffersize);	// read from buffer
	int  (*vfgets)(vfile_t* file, byte *string, size_t bufsize );	// read text line 
	int  (*vfprint)(vfile_t* file, const char *msg);			// write message
	int  (*vfprintf)(vfile_t* file, const char* format, ...);		// write formatted message
	int (*vfseek)(vfile_t* file, fs_offset_t offset, int whence);	// fseek, can seek in packfiles too
	bool (*vfunpack)( void* comp, size_t size1, void **buf, size_t size2);// deflate zipped buffer
	byte *(*vfbuffer)( vfile_t *file );				// get pointer to virtual filebuff
	long (*vftell)(vfile_t* file);				// like a ftell
	bool (*vfeof)( vfile_t* file);				// like a feof

	// wadstorage filesystem
	int (*wfcheck)( const char *filename );				// validate container
	wfile_t *(*wfopen)( const char *filename, const char *mode );	// open wad file or create new
	void (*wfclose)( wfile_t *wad );				// close wadfile
	long (*wfwrite)( wfile_t *wad, const char *lump, const void* data, size_t size, char type, char cmp );
	byte *(*wfread)( wfile_t *wad, const char *lump, size_t *lumpsizeptr, const char type );

	// filesystem simply user interface
	byte *(*Com_LoadFile)(const char *path, long *filesize );		// load file into heap
	bool (*Com_WriteFile)(const char *path, const void *data, long len );	// write file into disk
	bool (*Com_LoadLibrary)( dll_info_t *dll );			// load library 
	bool (*Com_FreeLibrary)( dll_info_t *dll );			// free library
	void*(*Com_GetProcAddress)( dll_info_t *dll, const char* name );	// gpa
	double (*Com_DoubleTime)( void );				// hi-res timer
	dword (*Com_Milliseconds)( void );				// hi-res timer
	void (*Com_ShellExecute)( const char *p1, const char *p2, bool exit );// execute shell programs

	// built-in imagelib functions
	void (*ImglibSetup)( const char *formats, const uint flags );	// set main attributes
	rgbdata_t *(*ImageLoad)( const char *, const byte *, size_t );	// load image from disk or buffer
	bool (*ImageSave)( const char *name, rgbdata_t *image );		// save image into specified format
	bool (*ImageConvert)( rgbdata_t **pix, int w, int h, uint flags );	// image manipulations
	bpc_desc_t *(*ImagePFDesc)( pixformat_t imagetype );		// get const info about specified fmt
 	void (*ImageFree)( rgbdata_t *pack );				// release image buffer

	// random generator
	long (*Com_RandomLong)( long lMin, long lMax );			// returns random integer
	float (*Com_RandomFloat)( float fMin, float fMax );		// returns random float

	// mathlib.c funcs
	void (*sincos)( float x, float *sin, float *cos );		// fast sincos
	float (*atan2)( float x, float y );				// fast arctan
	float (*acos)( float x );					// fast arccos
	float (*asin)( float x );					// fast arcsin
	float (*sqrt)( float x );					// fast sqrt
	float (*sin)( float x );					// fast sine
	float (*cos)( float x );					// fast cosine
	float (*tan)( float x );					// fast tan
	
	// stdlib.c funcs
	void (*strnupr)(const char *in, char *out, size_t size_out);	// convert string to upper case
	void (*strnlwr)(const char *in, char *out, size_t size_out);	// convert string to lower case
	void (*strupr)(const char *in, char *out);			// convert string to upper case
	void (*strlwr)(const char *in, char *out);			// convert string to lower case
	int (*strlen)( const char *string );				// returns string real length
	int (*cstrlen)( const char *string );				// strlen that stripped color prefixes
	char (*toupper)(const char in );				// convert one charcster to upper case
	char (*tolower)(const char in );				// convert one charcster to lower case
	size_t (*strncat)(char *dst, const char *src, size_t n);		// add new string at end of buffer
	size_t (*strcat)(char *dst, const char *src);			// add new string at end of buffer
	size_t (*strncpy)(char *dst, const char *src, size_t n);		// copy string to existing buffer
	size_t (*strcpy)(char *dst, const char *src);			// copy string to existing buffer
	char *(*stralloc)(byte *mp,const char *in,const char *file,int line);	// create buffer and copy string here
	bool (*is_digit)( const char *str );				// check string for digits
	int (*atoi)(const char *str);					// convert string to integer
	float (*atof)(const char *str);				// convert string to float
	void (*atov)( float *dst, const char *src, size_t n );		// convert string to vector
	char *(*strchr)(const char *s, char c);				// find charcster at start of string
	char *(*strrchr)(const char *s, char c);			// find charcster at end of string
	int (*strnicmp)(const char *s1, const char *s2, int n);		// compare strings with case insensative
	int (*stricmp)(const char *s1, const char *s2);			// compare strings with case insensative
	int (*strncmp)(const char *s1, const char *s2, int n);		// compare strings with case sensative
	int (*strcmp)(const char *s1, const char *s2);			// compare strings with case sensative
	char *(*stristr)( const char *s1, const char *s2 );		// find s2 in s1 with case insensative
	char *(*strstr)( const char *s1, const char *s2 );		// find s2 in s1 with case sensative
	size_t (*strpack)( byte *buf, size_t pos, char *s1, int n );	// put string at end ofbuffer
	size_t (*strunpack)( byte *buf, size_t pos, char *s1 );		// extract string from buffer
	int (*vsprintf)(char *buf, const char *fmt, va_list args);		// format message
	int (*sprintf)(char *buffer, const char *format, ...);		// print into buffer
	char *(*va)(const char *format, ...);				// print into temp buffer
	int (*vsnprintf)(char *buf, size_t size, const char *fmt, va_list args);	// format message
	int (*snprintf)(char *buffer, size_t buffersize, const char *format, ...);	// print into buffer
	char *(*pretifymem)( float value, int digitsafterdecimal );		// pretify memory string
	const char* (*timestamp)( int format );				// returns current time stamp

	// stringtable system
	int (*st_create)( const char *name, size_t max_strings );
	const char *(*st_getstring)( int handle, string_t index );
	string_t (*st_setstring)( int handle, const char *string );
	int (*st_load)( wfile_t *wad, const char *name );
	const char *(*st_getname)( int handle );
	bool (*st_save)( int h, wfile_t *wad );
	void (*st_clear)( int handle );
	void (*st_remove)( int handle );
} stdlib_api_t;

// this is the only function actually exported at the linker level
typedef void *(*launch_t)( stdlib_api_t*, void* );
typedef struct { size_t api_size; size_t com_size; } generic_api_t;

// moved here to enable assertation feature in launch.dll
#define Com_Assert( x )		if( x ) com.abort( "assert failed at %s:%i\n", __FILE__, __LINE__ );

#ifndef LAUNCH_DLL
/*
==============================================================================
		STDLIB GENERIC ALIAS NAMES
don't add aliases for launch.dll because it may be conflicted with real names
==============================================================================
*/
/*
========================================================================
script shared declaration
external and internal script_t struct have some differences
========================================================================
*/
typedef struct script_s
{
	char	*buffer;
	char	*text;
	size_t	size;
	char	TXcommand;
};

#include "cvardef.h"

/*
==========================================
	memory manager funcs
==========================================
*/
#define Mem_Alloc(pool, size)		com.malloc(pool, size, __FILE__, __LINE__)
#define Mem_Realloc(pool, ptr, size)	com.realloc(pool, ptr, size, __FILE__, __LINE__)
#define Mem_Move(pool, ptr, data, size)	com.move(pool, ptr, data, size, __FILE__, __LINE__)
#define Mem_Free(mem)		com.free(mem, __FILE__, __LINE__)
#define Mem_AllocPool(name)		com.mallocpool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool)		com.freepool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool)		com.clearpool(pool, __FILE__, __LINE__)
#define Mem_Copy(dest, src, size )	com.memcpy(dest, src, size, __FILE__, __LINE__)
#define Mem_Set(dest, val, size )	com.memset(dest, val, size, __FILE__, __LINE__)
#define Mem_Check()			com.memcheck(__FILE__, __LINE__)
#define Mem_CreateArray( p, s, n )	com.newarray( p, s, n, __FILE__, __LINE__)
#define Mem_RemoveArray( array )	com.delarray( array, __FILE__, __LINE__)
#define Mem_AllocElement( array )	com.newelement( array, __FILE__, __LINE__)
#define Mem_FreeElement( array, el )	com.delelement( array, el, __FILE__, __LINE__ )
#define Mem_GetElement( array, idx )	com.getelement( array, idx )
#define Mem_ArraySize( array )	com.arraysize( array )

/*
==========================================
	parsing manager funcs
==========================================
*/
#define Com_OpenScript		com.Com_OpenScript
#define Com_CloseScript		com.Com_CloseScript
#define Com_ResetScript		com.Com_ResetScript
#define Com_SkipBracedSection		com.Com_SkipBracedSection
#define Com_SkipRestOfLine		com.Com_SkipRestOfLine
#define Com_EndOfScript		com.Com_EndOfScript
#define Com_ReadToken		com.Com_ReadToken
#define Com_SaveToken		com.Com_SaveToken
#define Com_FreeToken		com.Com_FreeToken

#define Com_ReadString( x, y, z )	com.Com_ReadString( x, y, z, sizeof( z ))
#define Com_ReadFloat		com.Com_ReadFloat
#define Com_ReadUlong		com.Com_ReadDword
#define Com_ReadLong		com.Com_ReadLong

#define Com_HashKey			com.Com_HashKey

/*
===========================================
filesystem manager
===========================================
*/
#define FS_AddGameHierarchy		com.Com_AddGameHierarchy
#define FS_LoadGameInfo		com.Com_LoadGameInfo
#define FS_InitRootDir		com.Com_InitRootDir
#define FS_AllowDirectPaths		com.Com_AllowDirectPaths
#define FS_LoadFile			com.Com_LoadFile
#define FS_Search			com.Com_Search
#define FS_WriteFile		com.Com_WriteFile
#define FS_Open( path, mode )		com.fopen( path, mode )
#define FS_Read( file, buffer, size )	com.fread( file, buffer, size )
#define FS_Write( file, buffer, size )	com.fwrite( file, buffer, size )
#define FS_StripExtension( path )	com.Com_StripExtension( path )
#define FS_ExtractFilePath( src, dest)	com.Com_StripFilePath( src, dest )
#define FS_DefaultExtension		com.Com_DefaultExtension
#define FS_FileExtension( ext )	com.Com_FileExtension( ext )
#define FS_FileExists( file )		com.Com_FileExists( file )
#define FS_FileSize( file )		com.Com_FileSize( file )
#define FS_FileTime( file )		com.Com_FileTime( file )
#define FS_Close( file )		com.fclose( file )
#define FS_FileBase( x, y )		com.Com_FileBase( x, y )
#define FS_LoadInternal( x, y )	com.Com_LoadRes( x, y )
#define FS_Printf			com.fprintf
#define FS_Print			com.fprint
#define FS_Seek			com.fseek
#define FS_Tell			com.ftell
#define FS_Eof			com.feof
#define FS_Getc			com.fgetc
#define FS_Gets			com.fgets
#define FS_Delete			com.fremove
#define FS_Gamedir()		com.SysInfo->GameInfo->gamedir
#define FS_Title()			com.SysInfo->GameInfo->title
#define g_Instance()		com.SysInfo->instance
#define FS_ClearSearchPath		com.Com_ClearSearchPath
#define FS_CheckParm		com.Com_CheckParm
#define FS_GetParmFromCmdLine( a, b )	com.Com_GetParm( a, b, sizeof( b ))

/*
===========================================
network messages
===========================================
*/
#define NET_Init			com.NET_Init
#define NET_Shutdown		com.NET_Shutdown
#define NET_Sleep			com.NET_Sleep
#define NET_Config			com.NET_Config
#define NET_AdrToString		com.NET_AdrToString
#define NET_BaseAdrToString		com.NET_BaseAdrToString
#define NET_IsLocalAddress		com.NET_IsLocalAddress
#define NET_StringToAdr		com.NET_StringToAdr
#define NET_SendPacket		com.NET_SendPacket
#define NET_GetPacket		com.NET_GetPacket
#define NET_CompareAdr		com.NET_CompareAdr
#define NET_CompareBaseAdr		com.NET_CompareBaseAdr

/*
===========================================
console variables
===========================================
*/
#define Cvar_Get			com.Cvar_Get
#define Cvar_LookupVars		com.Cvar_LookupVars
#define Cvar_Set			com.Cvar_SetString
#define Cvar_FullSet		com.Cvar_FullSet
#define Cvar_SetLatched		com.Cvar_SetLatched
#define Cvar_SetValue		com.Cvar_SetValue
#define Cvar_VariableValue		com.Cvar_GetValue
#define Cvar_VariableInteger		com.Cvar_GetInteger
#define Cvar_VariableString		com.Cvar_GetString
#define Cvar_FindVar		com.Cvar_FindVar
#define userinfo_modified		com.userinfo_modified

/*
===========================================
console commands
===========================================
*/
#define Cbuf_ExecuteText		com.Cmd_Exec
#define Cbuf_AddText( text )		com.Cmd_Exec( EXEC_APPEND, text )
#define Cmd_ExecuteString( text )	com.Cmd_Exec( EXEC_NOW, text )
#define Cbuf_InsertText( text ) 	com.Cmd_Exec( EXEC_INSERT, text )
#define Cbuf_Execute()		com.Cmd_Exec( EXEC_NOW, NULL )

#define Cmd_Argc			com.Cmd_Argc
#define Cmd_Args			com.Cmd_Args
#define Cmd_Argv			com.Cmd_Argv
#define Cmd_TokenizeString		com.Cmd_TokenizeString
#define Cmd_LookupCmds		com.Cmd_LookupCmds
#define Cmd_AddCommand		com.Cmd_AddCommand
#define Cmd_RemoveCommand		com.Cmd_DelCommand

/*
===========================================
virtual filesystem manager
===========================================
*/
#define VFS_Create			com.vfcreate
#define VFS_GetBuffer		com.vfbuffer
#define VFS_Open			com.vfopen
#define VFS_Write			com.vfwrite
#define VFS_Read			com.vfread
#define VFS_Print			com.vfprint
#define VFS_Printf			com.vfprintf
#define VFS_Gets			com.vfgets
#define VFS_Seek			com.vfseek
#define VFS_Tell			com.vftell
#define VFS_Eof			com.vfeof
#define VFS_Close			com.vfclose
#define VFS_Unpack			com.vfunpack

/*
===========================================
wadstorage filesystem manager
===========================================
*/
#define WAD_Open			com.wfopen
#define WAD_Check			com.wfcheck
#define WAD_Close			com.wfclose
#define WAD_Write			com.wfwrite
#define WAD_Read			com.wfread

/*
===========================================
bezier curves patchlib
===========================================
*/
#define Patch_Evaluate		com.PatchEval
#define Patch_GetFlatness		com.PatchFlat

/*
===========================================
crclib manager
===========================================
*/
#define CRC_Init			com.crc_init
#define CRC_Block			com.crc_block
#define CRC_ProcessByte		com.crc_process
#define CRC_Sequence		com.crc_sequence
#define Com_BlockChecksum		com.crc_blockchecksum
#define Com_BlockChecksumKey		com.crc_blockchecksumkey

/*
===========================================
imglib manager
===========================================
*/
#define FS_LoadImage		com.ImageLoad
#define FS_SaveImage		com.ImageSave
#define FS_FreeImage		com.ImageFree
#define Image_Init			com.ImglibSetup
#define PFDesc( x )			com.ImagePFDesc( x )
#define Image_Process		com.ImageConvert

/*
===========================================
misc utils
===========================================
*/
#define SI			com.SysInfo
#define GI			com.SysInfo->GameInfo
#define Msg			com.printf
#define MsgDev			com.dprintf
#define Sys_LoadLibrary		com.Com_LoadLibrary
#define Sys_FreeLibrary		com.Com_FreeLibrary
#define Sys_GetProcAddress		com.Com_GetProcAddress
#define Sys_ShellExecute		com.Com_ShellExecute
#define Sys_NewInstance		com.instance
#define Sys_Sleep			com.sleep
#define Sys_Print			com.print
#define Sys_GetEvent		com.getevent
#define Sys_QueEvent		com.queevent
#define Sys_GetClipboardData		com.clipboard
#define Sys_Quit			com.exit
#define Sys_Break			com.abort
#define Sys_DoubleTime		com.Com_DoubleTime
#define Sys_Milliseconds		com.Com_Milliseconds
#define GetNumThreads		com.Com_NumThreads
#define ThreadLock			com.Com_ThreadLock
#define ThreadUnlock		com.Com_ThreadUnlock
#define RunThreadsOnIndividual	com.Com_CreateThread
#define Com_RandomLong		com.Com_RandomLong
#define Com_RandomFloat		com.Com_RandomFloat
#define StringTable_Create		com.st_create
#define StringTable_Delete		com.st_remove
#define StringTable_Clear		com.st_clear
#define StringTable_GetString		com.st_getstring
#define StringTable_SetString		com.st_setstring
#define StringTable_GetName		com.st_getname
#define StringTable_Load		com.st_load
#define StringTable_Save		com.st_save

/*
===========================================
stdlib function names that not across with windows stdlib
===========================================
*/
#define timestamp			com.timestamp
#define copystring( str )		com.stralloc( NULL, str, __FILE__, __LINE__ )
#define memprint( x )		com.pretifymem( x, 2 )
#define va			com.va

#endif//LAUNCH_DLL
#endif//LAUNCH_APH_H