 //=======================================================================
//			Copyright XashXT Group 2007 ©
//		  ref_dllapi.h - shared ifaces between engine parts
//=======================================================================
#ifndef REF_DLLAPI_H
#define REF_DLLAPI_H

#ifndef NULL
#define NULL		((void *)0)
#endif

#include "ref_dfiles.h"

enum host_state
{	// paltform states
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
	HOST_VIEWER,	// "viewer"	"viewer"
	HOST_COUNTS,	// terminator
};

#define STRING_COLOR_TAG	'^'
#define IsColorString(p)	( p && *(p) == STRING_COLOR_TAG && *((p)+1) && *((p)+1) != STRING_COLOR_TAG )
#define bound(min, num, max)	((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))
#define DLLEXPORT		__declspec(dllexport)

typedef long fs_offset_t;
typedef enum { NA_BAD, NA_LOOPBACK, NA_BROADCAST, NA_IP, NA_IPX, NA_BROADCAST_IPX } netadrtype_t;
typedef enum { NS_CLIENT, NS_SERVER } netsrc_t;
typedef struct { uint b:5; uint g:6; uint r:5; } color16;
typedef struct { byte r:8; byte g:8; byte b:8; } color24;
typedef struct { byte r; byte g; byte b; byte a; } color32;
typedef struct { const char *name; void **func; } dllfunc_t;	// Sys_LoadLibrary stuff
typedef struct { int ofs; int type; const char *name; } fields_t;	// prvm custom fields
typedef void (*cmread_t) (void* handle, void* buffer, size_t size);
typedef enum { mod_bad, mod_world, mod_brush, mod_studio, mod_sprite } modtype_t;
typedef void (*cmsave_t) (void* handle, const void* buffer, size_t size);
typedef void (*cmdraw_t)( int color, int numpoints, const float *points );
typedef struct { int numfilenames; char **filenames; char *filenamesbuffer; } search_t;
typedef void (*setpair_t)(const char *key, const char *value, void *buffer, void *numpairs );

typedef struct
{
	netadrtype_t	type;
	byte		ip[4];
	byte		ipx[10];
	word		port;
} netadr_t;

typedef struct sizebuf_s
{
	bool	overflowed;	// set to true if the buffer size failed

	byte	*data;
	int	maxsize;
	int	cursize;
	int	readcount;
} sizebuf_t;

typedef struct edict_s		edict_t;
typedef struct sv_edict_s		sv_edict_t;
typedef struct cl_edict_s		cl_edict_t;
typedef struct ui_edict_s		ui_edict_t;
typedef struct sv_entvars_s		sv_entvars_t;
typedef struct cl_entvars_s		cl_entvars_t;
typedef struct ui_entvars_s		ui_entvars_t;
typedef struct sv_globalvars_s	sv_globalvars_t;
typedef struct cl_globalvars_s	cl_globalvars_t;
typedef struct ui_globalvars_s	ui_globalvars_t;
typedef struct physbody_s		physbody_t;
typedef struct cvar_s		cvar_t;
typedef struct file_s		file_t;		// normal file
typedef struct vfile_s		vfile_t;		// virtual file
typedef struct wfile_s		wfile_t;		// wad file
typedef void (*xcommand_t) (void);

typedef enum
{
	ED_SPAWNED = 0,	// this entity requris to set own type with SV_ClassifyEdict
	ED_STATIC,	// this is a logic without model or entity with static model
	ED_AMBIENT,	// this is entity emitted ambient sounds only
	ED_NORMAL,	// normal entity with model (and\or) sound
	ED_BSPBRUSH,	// brush entity (a part of level)
	ED_CLIENT,	// this is a client entity
	ED_MONSTER,	// monster or bot (generic npc with AI)
	ED_TEMPENTITY,	// this edict will be removed on server when "lifetime" exceeds 
	ED_BEAM,		// laser beam (needs to recalculate pvs and frustum)
	ED_MOVER,		// func_train, func_door and another bsp or mdl movers
	ED_VIEWMODEL,	// client or bot viewmodel (for spectating)
	ED_ITEM,		// holdable items
	ED_RAGDOLL,	// dead body with simulated ragdolls
	ED_RIGIDBODY,	// simulated physic
	ED_TRIGGER,	// just for sorting on a server
	ED_PORTAL,	// realtime display, portal or mirror brush or model
	ED_MISSILE,	// greande, rocket e.t.c
	ED_DECAL,		// render will be merge real coords and normal
	ED_VEHICLE,	// controllable vehicle
	ED_MAXTYPES,
} edtype_t;

// phys movetype
typedef enum
{
	MOVETYPE_NONE,	// never moves
	MOVETYPE_NOCLIP,	// origin and angles change with no interaction
	MOVETYPE_PUSH,	// no clip to world, push on box contact
	MOVETYPE_WALK,	// gravity
	MOVETYPE_STEP,	// gravity, special edge handling
	MOVETYPE_FLY,
	MOVETYPE_TOSS,	// gravity
	MOVETYPE_BOUNCE,
	MOVETYPE_FOLLOW,	// attached models
	MOVETYPE_CONVEYOR,
	MOVETYPE_PUSHABLE,
	MOVETYPE_PHYSIC,	// phys simulation
} movetype_t;

// phys collision mode
typedef enum
{
	SOLID_NOT = 0,    	// no interaction with other objects
	SOLID_TRIGGER,	// only touch when inside, after moving
	SOLID_BBOX,	// touch on edge
	SOLID_BSP,    	// bsp clip, touch on edge
	SOLID_BOX,	// physbox
	SOLID_SPHERE,	// sphere
	SOLID_CYLINDER,	// cylinder e.g. barrel
	SOLID_MESH,	// custom convex hull
} solid_t;

// model_state_t communication
typedef struct model_state_s
{
	int		index;		// server & client shared modelindex	
	int		colormap;		// change base color for some textures or sprite frames
	float		scale;		// model or sprite scale, affects to physics too
	float		frame;		// % playback position in animation sequences (0..255)
	float		animtime;		// auto-animating time
	float		framerate;	// custom framerate, specified by QC
	int		sequence;		// animation sequence (0 - 255)
	int		gaitsequence;	// client\nps\bot gaitsequence
	int		skin;		// skin for studiomodels
	int		body;		// sub-model selection for studiomodels
	float		blending[16];	// studio animation blending
	float		controller[16];	// studio bone controllers
} model_state_t;

// entity_state_t communication
typedef struct entity_state_s
{
	// engine specific
	uint		number;		// edict index
	edtype_t		ed_type;		// edict type
	string_t		classname;	// edict classname
	int		soundindex;	// looped ambient sound

	// physics information
	vec3_t		origin;
	vec3_t		angles;		// entity angles, not viewangles
	vec3_t		velocity;		// player velocity
	vec3_t		old_origin;	// for lerping animation
	vec3_t		infotarget;	// portal camera, etc
	model_state_t	model;		// general entity model
	solid_t		solidtype;	// entity solidtype
	movetype_t	movetype;		// entity movetype
	int		gravity;		// gravity multiplier
	int		aiment;		// attahced entity (not a physic attach, only rendering)
	int		solid;		// using for symmetric bboxes
	vec3_t		mins;		// not symmetric entity bbox    
	vec3_t		maxs;

	// render information
	uint		effects;		// effect flags like q1 and hl1
	int		renderfx;		// render effects same as hl1
	float		renderamt;	// alpha value or like somewhat
	vec3_t		rendercolor;	// hl1 legacy stuff, working, but not needed
	int		rendermode;	// hl1 legacy stuff, working, but not needed

	// client specific
	int		pm_type;		// client movetype
	int		pm_flags;		// ducked, jump_held, etc
	int		pm_time;		// each unit = 8 ms
	vec3_t		delta_angles;	// add to command angles to get view direction 
	vec3_t		punch_angles;	// add to view direction to get render angles 
	vec3_t		viewangles;	// already calculated view angles on server-side
	vec3_t		viewoffset;	// viewoffset over ground
	int		maxspeed;		// sv_maxspeed will be duplicate on all clients
	float		health;		// client health (other parms can be send by custom messages)
	float		fov;		// horizontal field of view
	model_state_t	pmodel;		// weaponmodel info
} entity_state_t;

// usercmd_t communication
typedef struct usercmd_s
{
	int		msec;
	int		angles[3];
	int		forwardmove;
	int		sidemove;
	int		upmove;
	int		buttons;
	int		impulse;
	int		lightlevel;

} usercmd_t;

/*
========================================================================

SYS EVENT

keep console cmds, network messages, mouse reletives and key buttons
========================================================================
*/
typedef enum
{
	SE_NONE = 0,	// ev.time is still valid
	SE_KEY,		// ev.value is a key code, ev.value2 is the down flag
	SE_CHAR,		// ev.value is an ascii char
	SE_MOUSE,		// ev.value and ev.value2 are reletive signed x / y moves
	SE_CONSOLE,	// ev.data is a char*
	SE_PACKET		// ev.data is a netadr_t followed by data bytes to ev.length
} ev_type_t;

typedef struct
{
	dword		time;
	ev_type_t		type;
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
	string	basedir;
	string	gamedir;
	string	startmap;
	string	title;
	string	username;		// OS current user
	float	version;		// engine or mod version
	
	int	viewmode;
	int	gamemode;

	// shared system info
	int	cpunum;		// count of cpu's
	int64	tickcount;	// cpu frequency in 'ticks'
	float	cpufreq;		// cpu frequency in MHz
	bool	rdtsc;		// rdtsc support (profiler stuff)

	string	vprogs_dir;	// default progs directory 
	string	source_dir;	// default source directory

	string	gamedirs[128];	// environment folders
	int	numgamedirs;
	
	char	TXcommand;	// quark .map comment
	char	instance;		// global engine instance
} gameinfo_t;

/*
========================================================================

internal dll's loader

two main types - native dlls and other win32 libraries will be recognized automatically
========================================================================
*/
typedef struct dll_info_s
{
	// generic interface
	const char	*name;		// library name
	const dllfunc_t	*fcts;		// list of dll exports	
	const char	*entry;		// entrypoint name (internal libs only)
	void		*link;		// hinstance of loading library

	// xash interface
	void		*(*main)( void*, void* );
	bool		crash;		// crash if dll not found

	size_t		api_size;		// interface size

} dll_info_t;

/*
========================================================================

internal image format

typically expanded to rgba buffer
========================================================================
*/
enum comp_format
{
	PF_UNKNOWN = 0,
	PF_INDEXED_24,	// studio model skins
	PF_INDEXED_32,	// sprite 32-bit palette
	PF_RGBA_32,	// already prepared ".bmp", ".tga" or ".jpg" image 
	PF_ARGB_32,	// uncompressed dds image
	PF_RGB_24,	// uncompressed dds or another 24-bit image 
	PF_DXT1,		// nvidia DXT1 format
	PF_DXT3,		// nvidia DXT3 format
	PF_DXT5,		// nvidia DXT5 format
	PF_LUMINANCE,	// b&w dds image
	PF_LUMINANCE_16,	// b&w hi-res image
	PF_LUMINANCE_ALPHA, // b&w dds image with alpha channel
	PF_ABGR_64,	// uint image
	PF_ABGR_128F,	// float image
	PF_RGBA_GN,	// internal generated texture
	PF_TOTALCOUNT,	// must be last
};

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
{PF_UNKNOWN,	"raw",	0x1908,	0x1401, 0,  0,  0 },
{PF_INDEXED_24,	"pal 24",	0x1908,	0x1401, 1,  1,  0 },// expand data to RGBA buffer
{PF_INDEXED_32,	"pal 32",	0x1908,	0x1401, 1,  1,  0 },
{PF_RGBA_32,	"RGBA 32",0x1908,	0x1401, 4,  1, -4 },
{PF_ARGB_32,	"ARGB 32",0x1908,	0x1401, 4,  1, -4 },
{PF_RGB_24,	"RGB 24",	0x1908,	0x1401, 3,  1, -3 },
{PF_DXT1,		"DXT1",	0x1908,	0x1401, 4,  1,  8 },
{PF_DXT3,		"DXT3",	0x1908,	0x1401, 4,  1, 16 },
{PF_DXT5,		"DXT5",	0x1908,	0x1401, 4,  1, 16 },
{PF_LUMINANCE,	"LUM 8",	0x1909,	0x1401, 1,  1, -1 },
{PF_LUMINANCE_16,	"LUM 16", 0x1909,	0x1401, 2,  2, -2 },
{PF_LUMINANCE_ALPHA,"LUM A",	0x190A,	0x1401, 2,  1, -2 },
{PF_ABGR_64,	"ABGR 64",0x80E1,	0x1401, 4,  2, -8 },
{PF_ABGR_128F,	"ABGR128",0x1908,	0x1406, 4,  4, -16},
{PF_RGBA_GN,	"system",	0x1908,	0x1401, 4,  1, -4 },
};

// description flags
#define IMAGE_CUBEMAP	0x00000001
#define IMAGE_HAS_ALPHA	0x00000002
#define IMAGE_PREMULT	0x00000004	// indices who need in additional premultiply
#define IMAGE_GEN_MIPS	0x00000008	// must generate mips
#define IMAGE_CUBEMAP_FLIP	0x00000010	// it's a cubemap with flipped sides( dds pack )
#define IMAGE_ONLY_PALETTE	0x00000020	// image not valid, returns palette only

enum img_process
{
	IMAGE_FLIP_X = 0,
	IMAGE_FLIP_Y,
};

typedef struct rgbdata_s
{
	word	width;		// image width
	word	height;		// image height
	byte	numLayers;	// multi-layer volume
	byte	numMips;		// mipmap count
	byte	bitsCount;	// RGB bits count
	uint	type;		// compression type
	uint	hint;		// save to specified format
	uint	flags;		// misc image flags
	byte	*palette;		// palette if present
	byte	*buffer;		// image buffer
	vec3_t	color;		// radiocity reflectivity
	float	bump_scale;	// internal bumpscale
	uint	size;		// for bounds checking
} rgbdata_t;

enum dev_level
{
	D_INFO = 1,	// "-dev 1", shows various system messages
	D_WARN,		// "-dev 2", shows not critical system warnings
	D_ERROR,		// "-dev 3", shows critical warnings 
	D_LOAD,		// "-dev 4", show messages about loading resources
	D_NOTE,		// "-dev 5", show system notifications for engine develeopers
	D_MEMORY,		// "-dev 6", show memory allocation
	D_STRING,		// "-dev 7", show tempstrings allocation
};

/*
==============================================================================

STDLIB SYSTEM INTERFACE
==============================================================================
*/
typedef struct stdilib_api_s
{
	// interface validator
	size_t	api_size;					// must matched with sizeof(stdlib_api_t)
	
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
	void (*queevent)( dword time, ev_type_t type, int value, int value2, int length, void *ptr );
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
	void (*NET_ShowIP)( void );
	void (*NET_Config)( bool net_enable );
	char *(*NET_AdrToString)( netadr_t a );
	bool (*NET_IsLANAddress)( netadr_t adr );
	bool (*NET_StringToAdr)( const char *s, netadr_t *a );
	void (*NET_SendPacket)( int length, const void *data, netadr_t to  );

	// common functions
	void (*Com_InitRootDir)( char *path );			// init custom rootdir 
	void (*Com_LoadGameInfo)( const char *filename );		// gate game info from script file
	void (*Com_AddGameHierarchy)(const char *dir);		// add base directory in search list
	int  (*Com_CheckParm)( const char *parm );		// check parm in cmdline  
	bool (*Com_GetParm)( char *parm, char *out );		// get parm from cmdline
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
	bool (*Com_LoadScript)(const char *name,char *buf,int size);// load script into stack from file or bufer
	bool (*Com_AddScript)(const char *name,char *buf, int size);// include script from file or buffer
	void (*Com_ResetScript)( void );			// reset current script state
	void (*Com_PushScript)( const char **data_p );		// save script to stack
	void (*Com_PopScript)( const char **data_p );		// restore script from stack
	void (*Com_SkipBracedSection)( char **data_p, int depth );	// skip braced section
	char *(*Com_ReadToken)( bool newline );			// get next token on a line or newline
	bool (*Com_TryToken)( void );				// return 1 if have token on a line 
	void (*Com_FreeToken)( void );			// free current token to may get it again
	void (*Com_SkipToken)( void );			// skip current token and jump into newline
	bool (*Com_MatchToken)( const char *match );		// compare current token with user keyword
	bool (*Com_ParseToken_Simple)(const char **data_p);	// basic parse (can't handle single characters)
	char *(*Com_ParseToken)(const char **data, bool newline );	// parse token from char buffer
	char *(*Com_ParseWord)( const char **data, bool newline );	// parse word from char buffer
	search_t *(*Com_Search)(const char *pattern, int casecmp );	// returned list of found files
	bool (*Com_Filter)(char *filter, char *name, int casecmp ); // compare keyword by mask with filter
	uint (*Com_HashKey)( const char *string, uint hashSize );	// returns hash key for a string
	byte *(*Com_LoadRes)( const char *filename, size_t *size );	// find internal resource in baserc.dll 
	char *com_token;					// contains current token

	// console variables
	cvar_t *(*Cvar_Get)(const char *name, const char *value, int flags, const char *desc);
	void (*Cvar_LookupVars)( int checkbit, void *buffer, void *ptr, setpair_t callback );
	void (*Cvar_SetString)( const char *name, const char *value );
	void (*Cvar_SetLatched)( const char *name, const char *value);
	void (*Cvar_FullSet)( char *name, char *value, int flags );
	void (*Cvar_SetValue )( const char *name, float value);
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
	int (*fgets)(file_t* file, byte *string, size_t bufsize );		// like a fgets, but can return EOF
	int (*fseek)(file_t* file, fs_offset_t offset, int whence);		// fseek, can seek in packfiles too
	long (*ftell)(file_t* file);					// like a ftell

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
	long (*wfwrite)( wfile_t *wad, const char *lump, const void* data, size_t datasize, char type, char cmp );
	byte *(*wfread)( wfile_t *wad, const char *lump, size_t *lumpsizeptr, const char type );

	// filesystem simply user interface
	byte *(*Com_LoadFile)(const char *path, long *filesize );		// load file into heap
	bool (*Com_WriteFile)(const char *path, const void *data, long len );	// write file into disk
	bool (*Com_LoadLibrary)( dll_info_t *dll );			// load library 
	bool (*Com_FreeLibrary)( dll_info_t *dll );			// free library
	void*(*Com_GetProcAddress)( dll_info_t *dll, const char* name );	// gpa
	double (*Com_DoubleTime)( void );				// hi-res timer
	dword (*Com_Milliseconds)( void );				// hi-res timer

	// built-in imagelib functions
	rgbdata_t *(*LoadImage)( const char *path, const byte *buf, size_t filesize );	// return 8, 24 or 32 bit buffer with image info
	void (*GetImageColor)( rgbdata_t *pic );			// stored result in rgbdata_t->color
	void (*SaveImage)( const char *filename, rgbdata_t *buffer );	// save image into specified format 
	void (*FreeImage)( rgbdata_t *pack );				// free image buffer

	// image manipulation
	void (*ImagePal32to24)( rgbdata_t *pic );
	bool (*ResampleImage)( rgbdata_t **pix, int w, int h, bool free_org );// resample image
	bool (*ProcessImage)( rgbdata_t **pix, int adj_type, bool free_org );	// flip, rotate e.t.c

	// random generator
	long (*Com_RandomLong)( long lMin, long lMax );			// returns random integer
	float (*Com_RandomFloat)( float fMin, float fMax );		// returns random float

	// stdlib.c funcs
	void (*strnupr)(const char *in, char *out, size_t size_out);	// convert string to upper case
	void (*strnlwr)(const char *in, char *out, size_t size_out);	// convert string to lower case
	void (*strupr)(const char *in, char *out);			// convert string to upper case
	void (*strlwr)(const char *in, char *out);			// convert string to lower case
	int (*strlen)( const char *string );				// returns string real length
	int (*cstrlen)( const char *string );				// return string length without color prefixes
	char (*toupper)(const char in );				// convert one charcster to upper case
	char (*tolower)(const char in );				// convert one charcster to lower case
	size_t (*strncat)(char *dst, const char *src, size_t n);		// add new string at end of buffer
	size_t (*strcat)(char *dst, const char *src);			// add new string at end of buffer
	size_t (*strncpy)(char *dst, const char *src, size_t n);		// copy string to existing buffer
	size_t (*strcpy)(char *dst, const char *src);			// copy string to existing buffer
	char *(*stralloc)(byte *mp,const char *in,const char *file,int line);	// create buffer and copy string here
	int (*atoi)(const char *str);					// convert string to integer
	float (*atof)(const char *str);				// convert string to float
	void (*atov)( float *dst, const char *src, size_t n );		// convert string to vector
	char *(*strchr)(const char *s, char c);				// find charcster in string at left side
	char *(*strrchr)(const char *s, char c);			// find charcster in string at right side
	int (*strnicmp)(const char *s1, const char *s2, int n);		// compare strings with case insensative
	int (*stricmp)(const char *s1, const char *s2);			// compare strings with case insensative
	int (*strncmp)(const char *s1, const char *s2, int n);		// compare strings with case sensative
	int (*strcmp)(const char *s1, const char *s2);			// compare strings with case sensative
	char *(*stristr)( const char *s1, const char *s2 );		// find s2 in s1 with case insensative
	char *(*strstr)( const char *s1, const char *s2 );		// find s2 in s1 with case sensative
	size_t (*strpack)( byte *buf, size_t pos, char *s1, int n );	// include string into buffer (same as strncat)
	size_t (*strunpack)( byte *buf, size_t pos, char *s1 );		// extract string from buffer
	int (*vsprintf)(char *buf, const char *fmt, va_list args);		// format message
	int (*sprintf)(char *buffer, const char *format, ...);		// print into buffer
	char *(*va)(const char *format, ...);				// print into temp buffer
	int (*vsnprintf)(char *buf, size_t size, const char *fmt, va_list args);	// format message
	int (*snprintf)(char *buffer, size_t buffersize, const char *format, ...);	// print into buffer
	const char* (*timestamp)( int format );				// returns current time stamp

	// stringtable system
	int (*st_create)( const char *name, size_t max_strings );
	const char *(*st_getstring)( int handle, string_t index );
	string_t (*st_setstring)( int handle, const char *string );
	int (*st_load)( wfile_t *wad, const char *name );
	bool (*st_save)( int h, wfile_t *wad );
	void (*st_remove)( int handle );
	
	// misc utils	
	gameinfo_t *GameInfo;					// user game info (filled by engine)

} stdlib_api_t;

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
#define CVAR_SYSTEMINFO	8	// don't changed from console, saved into config.dll
#define CVAR_INIT		16	// don't allow change from console at all, but can be set from the command line
#define CVAR_LATCH		32	// save changes until server restart
#define CVAR_READ_ONLY	64	// display only, cannot be set by user at all
#define CVAR_USER_CREATED	128	// created by a set command (prvm used)
#define CVAR_TEMP		256	// can be set even when cheats are disabled, but is not archived
#define CVAR_CHEAT		512	// can not be changed if cheats are disabled
#define CVAR_NORESTART	1024	// do not clear when a cvar_restart is issued
#define CVAR_MAXFLAGSVAL	2047	// maximum number of flags

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
#define Mem_Alloc(pool, size)		com.malloc(pool, size, __FILE__, __LINE__)
#define Mem_Realloc(pool, ptr, size)	com.realloc(pool, ptr, size, __FILE__, __LINE__)
#define Mem_Move(pool, ptr, data, size)	com.move(pool, ptr, data, size, __FILE__, __LINE__)
#define Mem_Free(mem)		com.free(mem, __FILE__, __LINE__)
#define Mem_AllocPool(name)		com.mallocpool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool)		com.freepool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool)		com.clearpool(pool, __FILE__, __LINE__)
#define Mem_Copy(dest, src, size )	com.memcpy(dest, src, size, __FILE__, __LINE__)
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
#define Com_ParseToken	com.Com_ParseToken
#define Com_ParseWord	com.Com_ParseWord
#define Com_SimpleGetToken	com.Com_ParseToken_Simple
#define Com_SkipBracedSection	com.Com_SkipBracedSection
#define Com_Filter		com.Com_Filter
#define Com_HashKey		com.Com_HashKey
#define Com_LoadScript	com.Com_LoadScript
#define Com_IncludeScript	com.Com_AddScript
#define Com_ResetScript	com.Com_ResetScript
#define Com_GetToken	com.Com_ReadToken
#define Com_TryToken	com.Com_TryToken
#define Com_FreeToken	com.Com_FreeToken
#define Com_SkipToken	com.Com_SkipToken
#define Com_MatchToken	com.Com_MatchToken
#define Com_PushScript	com.Com_PushScript
#define Com_PopScript	com.Com_PopScript
#define com_token		com.com_token
#define g_TXcommand		com.GameInfo->TXcommand
#define g_Instance		com.GameInfo->instance

/*
===========================================
filesystem manager
===========================================
*/
#define FS_AddGameHierarchy		com.Com_AddGameHierarchy
#define FS_LoadGameInfo		com.Com_LoadGameInfo
#define FS_InitRootDir		com.Com_InitRootDir
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
#define FS_Gets			com.fgets
#define FS_Gamedir			com.GameInfo->gamedir
#define FS_Title			com.GameInfo->title
#define FS_ClearSearchPath		com.Com_ClearSearchPath
#define FS_CheckParm		com.Com_CheckParm
#define FS_GetParmFromCmdLine		com.Com_GetParm

/*
===========================================
network messages
===========================================
*/
#define NET_Init			com.NET_Init
#define NET_Shutdown		com.NET_Shutdown
#define NET_ShowIP			com.NET_ShowIP
#define NET_Config			com.NET_Config
#define NET_AdrToString		com.NET_AdrToString
#define NET_IsLANAddress		com.NET_IsLANAddress
#define Sys_StringToAdr		com.NET_StringToAdr
#define Sys_SendPacket		com.NET_SendPacket

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

#define Cmd_Argc		com.Cmd_Argc
#define Cmd_Args		com.Cmd_Args
#define Cmd_Argv		com.Cmd_Argv
#define Cmd_TokenizeString	com.Cmd_TokenizeString
#define Cmd_LookupCmds	com.Cmd_LookupCmds
#define Cmd_AddCommand	com.Cmd_AddCommand
#define Cmd_RemoveCommand	com.Cmd_DelCommand

/*
===========================================
virtual filesystem manager
===========================================
*/
#define VFS_Create		com.vfcreate
#define VFS_GetBuffer	com.vfbuffer
#define VFS_Open		com.vfopen
#define VFS_Write		com.vfwrite
#define VFS_Read		com.vfread
#define VFS_Print		com.vfprint
#define VFS_Printf		com.vfprintf
#define VFS_Gets		com.vfgets
#define VFS_Seek		com.vfseek
#define VFS_Tell		com.vftell
#define VFS_Eof		com.vfeof
#define VFS_Close		com.vfclose
#define VFS_Unpack		com.vfunpack

/*
===========================================
wadstorage filesystem manager
===========================================
*/
#define WAD_Open		com.wfopen
#define WAD_Check		com.wfcheck
#define WAD_Close		com.wfclose
#define WAD_Write		com.wfwrite
#define WAD_Read		com.wfread

/*
===========================================
crclib manager
===========================================
*/
#define CRC_Init		com.crc_init
#define CRC_Block		com.crc_block
#define CRC_ProcessByte	com.crc_process
#define CRC_Sequence	com.crc_sequence
#define Com_BlockChecksum	com.crc_blockchecksum
#define Com_BlockChecksumKey	com.crc_blockchecksumkey

/*
===========================================
imglib manager
===========================================
*/
#define FS_LoadImage	com.LoadImage
#define FS_SaveImage	com.SaveImage
#define FS_FreeImage	com.FreeImage
#define Image_GetColor	com.GetImageColor
#define Image_ConvertPalette	com.ImagePal32to24
#define Image_Resample	com.ResampleImage
#define Image_Process	com.ProcessImage

/*
===========================================
misc utils
===========================================
*/
#define GI			com.GameInfo
#define Msg			com.printf
#define MsgDev			com.dprintf
#define Sys_LoadLibrary		com.Com_LoadLibrary
#define Sys_FreeLibrary		com.Com_FreeLibrary
#define Sys_GetProcAddress		com.Com_GetProcAddress
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
#define StringTable_GetString		com.st_getstring
#define StringTable_SetString		com.st_setstring
#define StringTable_Load		com.st_load
#define StringTable_Save		com.st_save

/*
===========================================
stdlib function names that not across with windows stdlib
===========================================
*/
#define timestamp			com.timestamp
#define copystring( str )		com.stralloc( NULL, str, __FILE__, __LINE__ )
#define va			com.va

#endif//LAUNCH_DLL

/*
========================================================================

internal physic data

hold linear and angular velocity, current position stored too
========================================================================
*/
typedef struct physdata_s
{
	vec3_t		origin;
	vec3_t		angles;
	vec3_t		velocity;
	vec3_t		avelocity;	// "omega" in newton
	vec3_t		mins;		// for calculate size 
	vec3_t		maxs;		// and setup offset matrix

	int		movetype;		// moving type
	int		hulltype;		// solid type

	physbody_t	*body;		// ptr to physic body
} physdata_t;



/*
========================================================================

console variables

external and internal cvars struct have some differences
========================================================================
*/
#ifndef LAUNCH_DLL
typedef struct cvar_s
{
	char	*name;
	char	*string;		// normal string
	float	value;		// atof( string )
	int	integer;		// atoi( string )
	bool	modified;		// set each time the cvar is changed
} cvar_t;
#endif

// euler angle order
#define PITCH			0
#define YAW			1
#define ROLL			2

//
// engine constnat limits, touching networking protocol modify with cation
//
#define MAX_DLIGHTS			128	// dynamic lights (per one frame)
#define MAX_LIGHTSTYLES		256	// can be blindly increased
#define MAX_CLASSNAMES		512	// maxcount of various edicts classnames
#define MAX_SOUNDS			512	// openal software limit
#define MAX_MODELS			4096	// total count of brush & studio various models per one map
#define MAX_PARTICLES		32768	// pre one frame
#define MAX_EDICTS			65535	// absolute limit that never be reached, (do not edit!)

// FIXME: player_state_t->renderfx
#define RDF_NOWORLDMODEL		(1<<0)		// used for player configuration screen
#define RDF_IRGOGGLES		(1<<1)
#define RDF_PAIN			(1<<2)

// encoded bmodel mask
#define SOLID_BMODEL	0xffffff

/*
==============================================================================

MAP CONTENTS & SURFACES DESCRIPTION
==============================================================================
*/
#define PLANE_X			0	// 0 - 2 are axial planes
#define PLANE_Y			1	// 3 needs alternate calc
#define PLANE_Z			2

/*
==============================================================================

ENGINE TRACE FORMAT
==============================================================================
*/
typedef struct cplane_s
{
	vec3_t	normal;
	float	dist;
	byte	type;		// for fast side tests
	byte	signbits;		// signx + (signy<<1) + (signz<<1)
	byte	pad[2];
} cplane_t;

typedef struct cmesh_s
{
	vec3_t	*verts;
	int	*indices;
	uint	numverts;
	uint	numtris;
} cmesh_t;

typedef struct cmodel_s
{
	string	name;		// model name
	byte	*mempool;		// personal mempool
	int	registration_sequence;

	vec3_t	mins, maxs;	// model boundbox
	int	type;		// model type
	int	firstface;	// used to create collision tree
	int	numfaces;		
	int	firstbrush;	// used to create collision brush
	int	numbrushes;
	int	numframes;	// sprite framecount
	int	numbodies;	// physmesh numbody
	cmesh_t	*col[256];	// max bodies
	byte	*extradata;	// server studio uses this

	// g-cont: stupid pushmodel stuff
	vec3_t	normalmins;	// bounding box at angles '0 0 0'
	vec3_t	normalmaxs;
	vec3_t	yawmins;		// bounding box if yaw angle is not 0, but pitch and roll are
	vec3_t	yawmaxs;
	vec3_t	rotatedmins;	// bounding box if pitch or roll are used
	vec3_t	rotatedmaxs;

	// custom traces for various model types
	void (*TraceBox)( const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, struct cmodel_s *model, struct trace_s *trace, int brushsmask );
	int (*PointContents)( const vec3_t point, struct cmodel_s *model );
} cmodel_t;

typedef struct csurface_s
{
	string	name;
	int	surfaceflags;
	int	contentflags;
	int	value;

	vec3_t	mins;
	vec3_t	maxs;

	// patches support
	int	numtriangles;
	int	numvertices;
	int	*indices;
	float	*vertices;
	int	markframe;
} csurface_t;

typedef struct trace_s
{
	bool		allsolid;		// if true, plane is not valid
	bool		startsolid;	// if true, the initial point was in a solid area
	bool		startstuck;	// trace started from solid entity
	float		fraction;		// time completed, 1.0 = didn't hit anything
	float		realfraction;	// like fraction but is not nudged away from the surface
	vec3_t		endpos;		// final position
	cplane_t		plane;		// surface normal at impact
	csurface_t	*surface;		// surface hit
	int		contents;		// contents on other side of surface hit
	int		startcontents;
	int		contentsmask;
	int		surfaceflags;
	int		hitgroup;		// hit a studiomodel hitgroup #
	int		flags;		// misc trace flags
	edict_t		*ent;		// not set by CM_*() functions
} trace_t;

// pmove_state_t is the information necessary for client side movement
#define PM_NORMAL		0 // can accelerate and turn
#define PM_SPECTATOR	1
#define PM_DEAD		2 // no acceleration or turning
#define PM_GIB		3 // different bounding box
#define PM_FREEZE		4
#define PM_INTERMISSION	5
#define PM_NOCLIP		6

// pmove->pm_flags
#define PMF_DUCKED		1
#define PMF_JUMP_HELD	2
#define PMF_ON_GROUND	4
#define PMF_TIME_WATERJUMP	8	// pm_time is waterjump
#define PMF_TIME_LAND	16	// pm_time is time before rejump
#define PMF_TIME_TELEPORT	32	// pm_time is non-moving time
#define PMF_NO_PREDICTION	64	// temporarily disables prediction (used for grappling hook)
#define PMF_ALL_TIMES	(PMF_TIME_WATERJUMP|PMF_TIME_LAND|PMF_TIME_TELEPORT)

// button bits
#define	BUTTON_ATTACK	1
#define	BUTTON_ATTACK2	2
#define	BUTTON_USE	4
#define	BUTTON_WALKING	8
#define	BUTTON_ANY	128 // any key whatsoever

#define PM_MAXTOUCH		32

// sound channels
#define CHAN_AUTO		0
#define CHAN_WEAPON		1
#define CHAN_VOICE		2
#define CHAN_ITEM		3
#define CHAN_BODY		4
#define CHAN_ANNOUNCER	5  // announcer
// flags
#define CHAN_NO_PHS_ADD	8  // Send to all clients, not just ones in PHS (ATTN 0 will also do this)
#define CHAN_RELIABLE	16 // Send by reliable message, not datagram

// Sound attenuation values
#define ATTN_NONE		0  // Full volume the entire level
#define ATTN_NORM		1
#define ATTN_IDLE		2
#define ATTN_STATIC		3  // Diminish very rapidly with distance

typedef struct vrect_s
{
	int	x, y;
	int	width;
	int	height;
} vrect_t;

typedef struct
{
	vrect_t		rect;		// screen rectangle
	float		fov_x;		// field of view by vertical
	float		fov_y;		// field of view by horizontal
	vec3_t		vieworg;		// client origin + viewoffset
	vec3_t		viewangles;	// client angles
	float		time;		// time is used to shaders auto animate
	float		oldtime;		// oldtime using for lerping studio models
	uint		rdflags;		// client view effects: RDF_UNDERWATER, RDF_MOTIONBLUR, etc
	byte		*areabits;	// if not NULL, only areas with set bits will be drawn
} refdef_t;

typedef struct pmove_s
{
	entity_state_t	ps;		// state (in / out)

	// command (in)
	usercmd_t		cmd;

	physbody_t	*body;		// pointer to physobject

	// results (out)
	int		numtouch;
	edict_t		*touchents[PM_MAXTOUCH];	// max touch
	edict_t		*groundentity;

	vec3_t		mins, maxs;	// bounding box size
	int		watertype;
	int		waterlevel;
	float		xyspeed;		// avoid to compute it twice

	// callbacks to test the world
	void		(*trace)( vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, trace_t *tr );
	int		(*pointcontents)( vec3_t point );
} pmove_t;

/*
==============================================================================

LAUNCH.DLL INTERFACE
==============================================================================
*/
typedef struct launch_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(launch_api_t)

	void ( *Init ) ( int argc, char **argv );		// init host
	void ( *Main ) ( void );				// host frame
	void ( *Free ) ( void );				// close host
	void (*CPrint) ( const char *msg );			// host print
	void (*MSG_Init)( sizebuf_t *buf, byte *data, size_t len );	// MSG init network buffer
} launch_exp_t;


/*
==============================================================================

BASERC.DLL INTERFACE

just a resource package library
==============================================================================
*/
typedef struct baserc_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(imglib_api_t)

	byte *(*LoadFile)( const char *filename, fs_offset_t *size );
} baserc_exp_t;

/*
==============================================================================

RENDER.DLL INTERFACE
==============================================================================
*/
typedef struct render_exp_s
{
	// interface validator
	size_t	api_size;			// must matched with sizeof(render_exp_t)

	// initialize
	bool	(*Init)( void );		// init all render systems
	void	(*Shutdown)( void );	// shutdown all render systems

	void	(*BeginRegistration)( const char *map );
	bool	(*RegisterModel)( const char *name, int sv_index );
	bool	(*RegisterImage)( const char *name, int sv_index );
	bool	(*PrecacheImage)( const char *name );
	void	(*SetSky) (char *name, float rotate, vec3_t axis);
	void	(*EndRegistration)( void );

	// prepare frame to rendering
	bool	(*AddRefEntity)( entity_state_t *s1, entity_state_t *s2, float lerp );
	bool	(*AddDynLight)( vec3_t org, vec3_t color, float intensity );
	bool	(*AddParticle)( vec3_t org, float alpha, int color );
	bool	(*AddLightStyle)( int stylenum, vec3_t color );
	void	(*ClearScene)( void );

	void	(*BeginFrame)( void );
	void	(*RenderFrame)( refdef_t *fd );
	void	(*EndFrame)( void );

	void	(*SetColor)( const float *rgba );
	bool	(*ScrShot)( const char *filename, int shot_type ); // write screenshot with same name 
	void	(*DrawFill)(float x, float y, float w, float h );
	void	(*DrawStretchRaw)( int x, int y, int w, int h, int cols, int rows, byte *data, bool redraw );
	void	(*DrawStretchPic)( float x, float y, float w, float h, float s1, float t1, float s2, float t2, const char *name );

	void	(*DrawGetPicSize)( int *w, int *h, const char *name );

} render_exp_t;

typedef struct render_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(render_imp_t)

	// client fundamental callbacks
	void	(*StudioEvent)( mstudioevent_t *event, entity_state_t *ent );
	void	(*ShowCollision)( cmdraw_t callback );	// debug
	long	(*WndProc)( void *hWnd, uint uMsg, uint wParam, long lParam );
	entity_state_t *(*GetClientEdict)( int index );
	entity_state_t *(*GetLocalPlayer)( void );
	int	(*GetMaxClients)( void );
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
	bool (*Init)( void );				// init all physic systems
	void (*Shutdown)( void );				// shutdown all render systems

	void (*LoadBSP)( const void *buf );			// load bspdata ( bsplib use this )
	void (*FreeBSP)( void );				// free bspdata
	void (*WriteCollisionLump)( file_t *f, cmsave_t callback );	// write collision data into LUMP_COLLISION

	void (*DrawCollision)( cmdraw_t callback );		// debug draw world
	void (*Frame)( float time );				// physics frame

	cmodel_t *(*BeginRegistration)( const char *name, bool clientload, uint *checksum );
	cmodel_t *(*RegisterModel)( const char *name );
	void (*EndRegistration)( void );

	void (*SetAreaPortals)( byte *portals, size_t size );
	void (*GetAreaPortals)( byte **portals, size_t *size );
	void (*SetAreaPortalState)( int area, int otherarea, bool open );

	int (*NumClusters)( void );
	int (*NumTextures)( void );
	int (*NumBmodels )( void );
	const char *(*GetEntityString)( void );
	const char *(*GetTextureName)( int index );
	int (*FatPVS)( const vec3_t org, vec_t radius, byte *fatpvs, size_t fatpvs_size, bool merge );
	byte *(*ClusterPVS)( int cluster );
	byte *(*ClusterPHS)( int cluster );
	int (*PointLeafnum)( vec3_t p );
	int (*BoxLeafnums)( vec3_t mins, vec3_t maxs, int *list, int listsize, int *lastleaf );
	int (*LeafCluster)( int leafnum );
	int (*LeafArea)( int leafnum );
	bool (*AreasConnected)( int area1, int area2 );
	int (*WriteAreaBits)( byte *buffer, int area );

	void (*ClipToGenericEntity)( trace_t *trace, cmodel_t *model, const vec3_t bodymins, const vec3_t bodymaxs, int bodysupercontents, matrix4x4 matrix, matrix4x4 inversematrix, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int contentsmask );
	void (*ClipToWorld)( trace_t *trace, cmodel_t *model, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int contentsmask );
	void (*CombineTraces)( trace_t *cliptrace, const trace_t *trace, edict_t *touch, bool is_bmodel );

	// player movement code
	void (*PlayerMove)( pmove_t *pmove, bool clientmove );
	
	// simple objects
	physbody_t *(*CreateBody)( sv_edict_t *ed, cmodel_t *mod, matrix4x3 transform, int solid );
	physbody_t *(*CreatePlayer)( sv_edict_t *ed, cmodel_t *mod, matrix4x3 transform );

	void (*SetOrigin)( physbody_t *body, vec3_t origin );
	void (*SetParameters)( physbody_t *body, cmodel_t *mod, int material, float mass );
	bool (*GetForce)(physbody_t *body, vec3_t vel, vec3_t avel, vec3_t force, vec3_t torque );
	void (*SetForce)(physbody_t *body, vec3_t vel, vec3_t avel, vec3_t force, vec3_t torque );
	bool (*GetMassCentre)( physbody_t *body, matrix3x3 mass );
	void (*SetMassCentre)( physbody_t *body, matrix3x3 mass );
	void (*RemoveBody)( physbody_t *body );
} physic_exp_t;

typedef struct physic_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(physic_imp_t)

	void (*ClientMove)( sv_edict_t *ed );
	void (*Transform)( sv_edict_t *ed, matrix4x3 transform );
	void (*PlaySound)( sv_edict_t *ed, float volume, const char *sample );
	float *(*GetModelVerts)( sv_edict_t *ent, int *numvertices );
} physic_imp_t;

/*
==============================================================================

VPROGS.DLL INTERFACE
==============================================================================
*/
#define PRVM_MAX_STACK_DEPTH		1024
#define PRVM_LOCALSTACK_SIZE		16384
#define PRVM_MAX_OPENFILES		256
#define PRVM_MAX_OPENSEARCHES		128

// prog flags
#define PRVM_OP_STATE		1
#define PRVM_FE_CHAIN		4
#define PRVM_FE_CLASSNAME		8
#define PRVM_OP_THINKTIME		16

enum
{
	PRVM_SERVERPROG = 0,
	PRVM_CLIENTPROG,
	PRVM_MENUPROG,
	PRVM_DECOMPILED,
	PRVM_MAXPROGS,	// must be last			
};

typedef void (*prvm_builtin_t)( void );
typedef struct edict_state_s
{
	bool	free;
	float	freetime;

} vm_edict_t;

struct edict_s
{
	// engine-private fields (stored in dynamically resized array)
	union
	{
		void			*vp;	// generic edict
		vm_edict_t		*ed;	// vm edict state 
		sv_edict_t		*sv;	// sv edict state
		cl_edict_t		*cl;	// cl edict state
		vm_edict_t		*ui;	// ui edict state
	} priv;

	// QuakeC prog fields (stored in dynamically resized array)
	union
	{
		void			*vp;	// generic entvars
		sv_entvars_t		*sv;	// server entvars
		cl_entvars_t		*cl;	// client entvars
		ui_entvars_t		*ui;	// uimenu entvars
	} progs;

};

typedef struct mfunction_s
{
	int		first_statement;	// negative numbers are builtins
	int		parm_start;
	int		locals;		// total ints of parms + locals

	// these are doubles so that they can count up to 54bits or so rather than 32bit
	double		profile;		// runtime
	double		builtinsprofile;	// cost of builtin functions called by this function
	double		callcount;	// times the functions has been called since the last profile call

	int		s_name;
	int		s_file;		// source file defined in
	int		numparms;
	byte		parm_size[MAX_PARMS];
} mfunction_t;

typedef struct prvm_stack_s
{
	int			s;
	mfunction_t		*f;
} prvm_stack_t;

typedef struct prvm_prog_s
{
	dprograms_t	*progs;
	mfunction_t	*functions;
	char		*strings;
	int		stringssize;
	ddef_t		*fielddefs;
	ddef_t		*globaldefs;
	dstatement_t	*statements;
	dsource_t		*sources;		// debug version include packed source files
	int		*linenums;	// debug versions only
	void		*types;		// (type_t *)
	int		edict_size;	// in bytes
	int		edictareasize;	// in bytes (for bound checking)
	int		pev_save;		// used by PRVM_PUSH_GLOBALS\PRVM_POP_GLOBALS
	int		other_save;	// used by PRVM_PUSH_GLOBALS\PRVM_POP_GLOBALS	
	int		*statement_linenums;// NULL if not available
	double		*statement_profile; // only incremented if prvm_statementprofiling is on

	union
	{
		float			*gp;
		sv_globalvars_t		*sv;
		cl_globalvars_t		*cl;
		ui_globalvars_t		*ui;
	} globals;

	int		maxknownstrings;
	int		numknownstrings;
	int		firstfreeknownstring;
	const char	**knownstrings;
	byte		*knownstrings_freeable;
	const char	***stringshash;
	byte		*progs_mempool;
	prvm_builtin_t	*builtins;
	int		numbuiltins;
	int		argc;
	int		trace;
	mfunction_t	*xfunction;
	int		xstatement;
	prvm_stack_t	stack[PRVM_MAX_STACK_DEPTH + 1];
	int		depth;
	int		localstack[PRVM_LOCALSTACK_SIZE];
	int		localstack_used;
	word		filecrc;
	int		intsize;
	vfile_t		*file[PRVM_MAX_OPENFILES];
	search_t		*search;
	int		num_edicts;
	int		max_edicts;
	int		limit_edicts;
	int		reserved_edicts;
	edict_t		*edicts;
	void		*edictsfields;
	void		*edictprivate;
	int		edictprivate_size;
	float		*time;
	float		_time;
	bool		protect_world;
	bool		loadintoworld;
	bool		loaded;
	char		*name;
	int		flag;
	ddef_t		*pev; // if pev != 0 then there is a global pev

	// function pointers
	void		(*begin_increase_edicts)(void);
	void		(*end_increase_edicts)(void);
	void		(*init_edict)(edict_t *edict);
	void		(*free_edict)(edict_t *ed);
	void		(*count_edicts)(void);
	bool		(*load_edict)(edict_t *ent);		// initialize edict for first loading
	void		(*restore_edict)(edict_t *ent);	// restore edict from savegame or changelevel
	void		(*init_cmd)(void);
	void		(*reset_cmd)(void);
	void		(*error_cmd)(const char *format, ...);

} prvm_prog_t;

typedef struct vprogs_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(vprogs_api_t)

	void ( *Init ) ( int argc, char **argv );	// init host
	void ( *Free ) ( void );			// close host

	// compiler functions
	void ( *PrepareDAT )( const char *dir, const char *name );
	void ( *CompileDAT )( void );
	void ( *Update )( dword time );		// refreshing compile, exec some programs e.t.c

	// edict operations
	edict_t *(*AllocEdict)( void );
	void (*FreeEdict)( edict_t *ed );
	void (*PrintEdict)( edict_t *ed );

	// savegame stuff
	void (*WriteGlobals)( void *buffer, void *ptr, setpair_t callback );
	void (*ReadGlobals)( int s_table, dkeyvalue_t *globals, int count );
	void (*WriteEdict)( edict_t *ed, void *buffer, void *ptr, setpair_t callback );
	void (*ReadEdict)( int s_table, int ednum, dkeyvalue_t *fields, int numpairs );

	// load ents description
	void (*LoadFromFile)( const char *data );

	// string manipulations
	const char *(*GetString)( int num );
	int (*SetEngineString)( const char *s );
	int (*SetTempString)( const char *s );

	void (*InitProg)( int prognr );
	void (*SetProg)( int prognr );
	bool (*ProgLoaded)( int prognr );
	void (*LoadProgs)( const char *filename );
	void (*ResetProg)( void );

	// abstract layer for searching globals and locals
	func_t (*FindFunctionOffset)( const char *function );
	int (*FindGlobalOffset)( const char *global );	
	int (*FindFieldOffset)( const char *field );	
	mfunction_t *(*FindFunction)( const char *name );
	ddef_t *(*FindGlobal)( const char *name );
	ddef_t *(*FindField)( const char *name );

	void (*StackTrace)( void );
	void (*Warning)( const char *fmt, ... );
	void (*Error)( const char *fmt, ... );

	void (*ExecuteProgram)( func_t fnum, const char *name, const char *file, const int line );
	void (*Crash)( void );	// crash virtual machine

	prvm_prog_t *prog;		// virtual machine current state

} vprogs_exp_t;

/*
==============================================================================

VSOUND.DLL INTERFACE
==============================================================================
*/
typedef struct vsound_exp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(vprogs_api_t)

	void (*Init)( void *hInst );	// init host
	void (*Shutdown)( void );	// close host

	// sound manager
	void (*BeginRegistration)( void );
	sound_t (*RegisterSound)( const char *name );
	void (*EndRegistration)( void );

	void (*StartSound)( const vec3_t pos, int entnum, int channel, sound_t sfx, float vol, float attn, bool use_loop );
	void (*StreamRawSamples)( int samples, int rate, int width, int channels, const byte *data );
	bool (*AddLoopingSound)( int entnum, sound_t handle, float volume, float attn );
	bool (*StartLocalSound)( const char *name );
	void (*StartBackgroundTrack)( const char *introTrack, const char *loopTrack );
	void (*StopBackgroundTrack)( void );

	void (*StartStreaming)( void );
	void (*StopStreaming)( void );

	void (*Frame)( int entnum, const vec3_t pos, const vec3_t vel, const vec3_t at, const vec3_t up );
	void (*StopAllSounds)( void );
	void (*FreeSounds)( void );

	void (*Activate)( bool active );

} vsound_exp_t;

typedef struct vsound_imp_s
{
	// interface validator
	size_t	api_size;		// must matched with sizeof(vsound_imp_t)

	void (*GetSoundSpatialization)( int entnum, vec3_t origin, vec3_t velocity );
	int  (*PointContents)( vec3_t point );
	void (*AddLoopingSounds)( void );

} vsound_imp_t;

// this is the only function actually exported at the linker level
typedef void *(*launch_t)( stdlib_api_t*, void* );
typedef struct { size_t api_size; } generic_api_t;

#endif//REF_DLLAPI_H