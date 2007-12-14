//=======================================================================
//			Copyright XashXT Group 2007 ©
//			basetypes.h - base engine types
//=======================================================================
#ifndef BASETYPES_H
#define BASETYPES_H

#pragma warning(disable : 4244)	// MIPS
#pragma warning(disable : 4136)	// X86
#pragma warning(disable : 4051)	// ALPHA
#pragma warning(disable : 4018)	// signed/unsigned mismatch
#pragma warning(disable : 4305)	// truncation from const double to float
#pragma warning(disable : 4201)	// nameless struct/union
#pragma warning(disable : 4514)	// unreferenced inline function removed
#pragma warning(disable : 4100)	// unreferenced formal parameter

#define DLLEXPORT		__declspec(dllexport)
#define DLLIMPORT		__declspec(dllimport)

#define MIN_PATH		64
#define MAX_QPATH		64	// get rid of this
#define MAX_OSPATH		128
#define MAX_NUM_ARGVS	128
#define MAX_STRING		256
#define MAX_SYSPATH		1024
#define MAX_INPUTLINE	16384	// many buffers use this size
#define MAX_INFO_KEY	64
#define MAX_INFO_VALUE	64
#define MAX_INFO_STRING	512
#define MAX_STRING_TOKENS	80
#define MAX_TOKEN_CHARS	128
#define MAX_STRING_CHARS	1024

#define COLOR_BLACK		'0'
#define COLOR_RED		'1'
#define COLOR_GREEN		'2'
#define COLOR_YELLOW	'3'
#define COLOR_BLUE		'4'
#define COLOR_CYAN		'5'
#define COLOR_MAGENTA	'6'
#define COLOR_WHITE		'7'
#define STRING_COLOR_TAG	'^'
#define ColorIndex(c)	(((c) - '0') & 7)
#define IsColorString(p)	( p && *(p) == STRING_COLOR_TAG && *((p)+1) && *((p)+1) != STRING_COLOR_TAG )

// command buffer modes
#define EXEC_NOW		0
#define EXEC_INSERT		1
#define EXEC_APPEND		2

// timestamp modes
#define TIME_FULL		0
#define TIME_DATE_ONLY	1
#define TIME_TIME_ONLY	2
#define TIME_NO_SECONDS	3

// cvar flags
#define CVAR_ARCHIVE	1	// set to cause it to be saved to vars.rc
#define CVAR_USERINFO	2	// added to userinfo  when changed
#define CVAR_SERVERINFO	4	// added to serverinfo when changed
#define CVAR_SYSTEMINFO	8	// these cvars will be duplicated on all clients
#define CVAR_INIT		16	// don't allow change from console at all, but can be set from the command line
#define CVAR_LATCH		32	// save changes until server restart
#define CVAR_READ_ONLY	64	// display only, cannot be set by user at all
#define CVAR_USER_CREATED	128	// created by a set command (prvm used)
#define CVAR_TEMP		256	// can be set even when cheats are disabled, but is not archived
#define CVAR_CHEAT		512	// can not be changed if cheats are disabled
#define CVAR_NORESTART	1024	// do not clear when a cvar_restart is issued
#define CVAR_MAXFLAGSVAL	2047	// maximum number of flags

// euler angle order
#define PITCH		0
#define YAW		1
#define ROLL		2

#ifndef NULL
#define NULL		((void *)0)
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK		0
#endif

#ifndef __cplusplus
typedef enum{ false, true }	bool;
#endif
typedef unsigned char 	byte;
typedef unsigned short	word;
typedef unsigned long	dword;
typedef unsigned int	uint;
typedef signed __int64	int64;
typedef struct cvar_s	cvar_t;
typedef struct file_s	file_t;
typedef struct vfile_s	vfile_t;
typedef struct image_s	image_t;
typedef struct model_s	model_t;
typedef int		func_t;
typedef struct edict_s	edict_t;
typedef int		sound_t;
typedef int		string_t;
typedef void (*xcommand_t)	(void);
typedef struct gclient_s	gclient_t;
typedef struct sv_edict_s	sv_edict_t;
typedef struct cl_edict_s	cl_edict_t;
typedef struct ui_edict_s	ui_edict_t;
typedef int		progsnum_t;
typedef struct progfuncs_s	progfuncs_t;
typedef struct physbody_s	physbody_t;
typedef float		vec_t;
typedef vec_t		vec3_t[3];
typedef vec_t		vec4_t[4];
typedef long		fs_offset_t;
typedef vec_t		matrix3x4[3][4];
typedef vec_t		matrix4x4[4][4];
typedef struct { size_t api_size; } generic_api_t;
typedef struct { int fileofs; int filelen; } lump_t;		// many formats use lumps to store blocks
typedef struct { int type; char *name; } activity_map_t;		// studio activity map conversion
typedef struct { uint b:5; uint g:6; uint r:5; } color16;
typedef struct { byte r:8; byte g:8; byte b:8; } color24;
typedef struct { byte r; byte g; byte b; byte a; } color32;
typedef struct { const char *name; void **func; } dllfunc_t;	// Sys_LoadLibrary stuff
typedef struct { int ofs; int type; const char *name; } fields_t;	// prvm custom fields
typedef void (*cmread_t) (void* handle, void* buffer, size_t size);
typedef void (*cmsave_t) (void* handle, const void* buffer, size_t size);
typedef void (*cmdraw_t)( int color, int numpoints, const float *points );
typedef struct { int numfilenames; char **filenames; char *filenamesbuffer; } search_t;
typedef void (*cvarcmd_t)(const char *s, const char *m, const char *d, void *ptr );

enum host_state
{	// paltform states
	HOST_OFFLINE = 0,	// host_init( funcname *arg ) same much as:
	HOST_NORMAL,	// "host_shared"
	HOST_DEDICATED,	// "host_dedicated"
	HOST_EDITOR,	// "host_editor"
	BSPLIB,		// "bsplib"
	IMGLIB,		// "imglib"
	QCCLIB,		// "qcclib"
	ROQLIB,		// "roqlib"
	SPRITE,		// "sprite"
	STUDIO,		// "studio"
	PAKLIB,		// "paklib"
	CREDITS,		// "splash"
	HOST_INSTALL,	// "install"
};

enum dev_level
{
	D_INFO = 1,	// "-dev 1", shows various system messages
	D_WARN,		// "-dev 2", shows not critical system warnings, same as MsgWarn
	D_ERROR,		// "-dev 3", shows critical warnings 
	D_LOAD,		// "-dev 4", show messages about loading resources
	D_NOTE,		// "-dev 5", show system notifications for engine develeopers
	D_MEMORY,		// "-dev 6", show memory allocation
};

static vec4_t g_color_table[8] =
{
{0.0, 0.0, 0.0, 1.0},
{1.0, 0.0, 0.0, 1.0},
{0.0, 1.0, 0.0, 1.0},
{1.0, 1.0, 0.0, 1.0},
{0.0, 0.0, 1.0, 1.0},
{0.0, 1.0, 1.0, 1.0},
{1.0, 0.0, 1.0, 1.0},
{1.0, 1.0, 1.0, 1.0},
};


#include "byteorder.h"	// byte ordering swap functions
#include "stdref.h"		// reference xash formats
#include "stdapi.h"		// reference xash stdlib api
#include "dllapi.h"		// shared api's between engine parts

#endif//BASETYPES_H