//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       stdapi.h - generic shared interfaces
//=======================================================================
#ifndef REF_SYSTEM_H
#define REF_SYSTEM_H

#include "stdref.h"

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
	void (*print)( const char *msg );			// basic text message
	void (*printf)( const char *msg, ... );			// formatted text message
	void (*dprintf)( int level, const char *msg, ...);	// developer text message
	void (*wprintf)( const char *msg, ... );		// warning text message
	void (*error)( const char *msg, ... );			// abnormal termination with message
	void (*abort)( const char *msg, ... );			// normal tremination with message
	void (*exit)( void );				// normal silent termination
	char *(*input)( void );				// system console input	
	void (*sleep)( int msec );				// sleep for some msec
	char *(*clipboard)( void );				// get clipboard data
	uint (*keyevents)( void );				// peek windows message

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

	// xash memlib extension - memory arrays
	byte *(*newarray)( byte *pool, size_t elementsize, int count, const char *file, int line );
	void (*delarray)( byte *array, const char *file, int line );
	void *(*newelement)( byte *array, const char *file, int line );
	void (*delelement)( byte *array, void *element, const char *file, int line );
	void *(*getelement)( byte *array, size_t index );
	size_t (*arraysize)( byte *arrayptr );

	// common functions
	void (*Com_InitRootDir)( char *path );			// init custom rootdir 
	void (*Com_LoadGameInfo)( const char *filename );		// gate game info from script file
	void (*Com_AddGameHierarchy)(const char *dir);		// add base directory in search list
	int  (*Com_CheckParm)( const char *parm );		// check parm in cmdline  
	bool (*Com_GetParm)( char *parm, char *out );		// get parm from cmdline
	void (*Com_FileBase)(const char *in, char *out);		// get filename without path & ext
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
	bool (*Com_ParseToken_Simple)(const char **data_p);	// basic parse (can't handle single characters)
	char *(*Com_ParseToken)(const char **data );		// parse token from char buffer
	char *(*Com_ParseWord)( const char **data );		// parse word from char buffer
	search_t *(*Com_Search)(const char *pattern, int casecmp );	// returned list of found files
	bool (*Com_Filter)(char *filter, char *name, int casecmp ); // compare keyword by mask with filter
	char *com_token;					// contains current token

	// console variables
	cvar_t *(*Cvar_Get)(const char *name, const char *value, int flags, const char *desc);
	void (*Cvar_LookupVars)( int checkbit, char *buffer, void *ptr, cvarcmd_t callback );
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
	void (*Cmd_LookupCmds)( char *buffer, void *ptr, cvarcmd_t callback );
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
	bool (*Com_ProcessImage)( const char *name, rgbdata_t **pix, int w, int h ); // resample image
	void (*Com_FreeImage)( rgbdata_t *pack );			// free image buffer
	bool (*Com_LoadLibrary)( dll_info_t *dll );			// load library 
	bool (*Com_FreeLibrary)( dll_info_t *dll );			// free library
	void*(*Com_GetProcAddress)( dll_info_t *dll, const char* name );	// gpa
	double (*Com_DoubleTime)( void );				// hi-res timer

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
	char *(*stralloc)(const char *in,const char *file,int line);	// create buffer and copy string here
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
	
	// misc utils	
	gameinfo_t *GameInfo;					// user game info (filled by engine)

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
#define Mem_Alloc(pool, size)		com.malloc(pool, size, __FILE__, __LINE__)
#define Mem_Realloc(pool, ptr, size)	com.realloc(pool, ptr, size, __FILE__, __LINE__)
#define Mem_Move(pool, ptr, data, size)	com.move(pool, ptr, data, size, __FILE__, __LINE__)
#define Mem_Free(mem)		com.free(mem, __FILE__, __LINE__)
#define Mem_AllocPool(name)		com.mallocpool(name, __FILE__, __LINE__)
#define Mem_FreePool(pool)		com.freepool(pool, __FILE__, __LINE__)
#define Mem_EmptyPool(pool)		com.clearpool(pool, __FILE__, __LINE__)
#define Mem_Copy(dest, src, size )	com.memcpy(dest, src, size, __FILE__, __LINE__)
#define Mem_Set(dest, src, size )	com.memset(dest, src, size, __FILE__, __LINE__)
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
#define Com_Filter		com.Com_Filter
#define Com_LoadScript	com.Com_LoadScript
#define Com_IncludeScript	com.Com_AddScript
#define Com_ResetScript	com.Com_ResetScript
#define Com_GetToken	com.Com_ReadToken
#define Com_TryToken	com.Com_TryToken
#define Com_FreeToken	com.Com_FreeToken
#define Com_SkipToken	com.Com_SkipToken
#define Com_MatchToken	com.Com_MatchToken
#define com_token		com.com_token
#define g_TXcommand		com.GameInfo->TXcommand // get rid of this

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
#define FS_Close( file )		com.fclose( file )
#define FS_FileBase( x, y )		com.Com_FileBase( x, y )
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
#define FS_LoadImage		com.Com_LoadImage
#define FS_SaveImage		com.Com_SaveImage
#define FS_FreeImage		com.Com_FreeImage

/*
===========================================
console variables
===========================================
*/
#define Cvar_Get(name, value, flags)	com.Cvar_Get(name, value, flags, "no description" ) //FIXME
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
#define VFS_Open		com.vfopen
#define VFS_Write		com.vfwrite
#define VFS_Read		com.vfread
#define VFS_Print		com.vfprint
#define VFS_Printf		com.vfprintf
#define VFS_Gets		com.vfgets
#define VFS_Seek		com.vfseek
#define VFS_Tell		com.vftell
#define VFS_Close		com.vfclose
#define VFS_Unpack		com.vfunpack

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
imagelib utils
===========================================
*/
#define Image_Processing	com.Com_ProcessImage

/*
===========================================
misc utils
===========================================
*/
#define GI			com.GameInfo
#define Msg			com.printf
#define MsgDev			com.dprintf
#define MsgWarn			com.wprintf
#define Sys_LoadLibrary		com.Com_LoadLibrary
#define Sys_FreeLibrary		com.Com_FreeLibrary
#define Sys_GetProcAddress		com.Com_GetProcAddress
#define Sys_Sleep			com.sleep
#define Sys_Print			com.print
#define Sys_ConsoleInput		com.input
#define Sys_GetKeyEvents		com.keyevents
#define Sys_GetClipboardData		com.clipboard
#define Sys_Quit			com.exit
#define Sys_Break			com.abort
#define Sys_ConsoleInput		com.input
#define Sys_DoubleTime		com.Com_DoubleTime
#define GetNumThreads		com.Com_NumThreads
#define ThreadLock			com.Com_ThreadLock
#define ThreadUnlock		com.Com_ThreadUnlock
#define RunThreadsOnIndividual	com.Com_CreateThread

/*
===========================================
stdlib functions that not across with win stdlib
===========================================
*/
#define timestamp			com.timestamp
#define copystring(str)		com.stralloc(str, __FILE__, __LINE__)
#define strcasecmp			com.stricmp
#define strncasecmp			com.strnicmp
#define strlower			com.strlwr
#define stristr			com.stristr
#define va			com.va

#endif//LAUNCH_DLL

#endif//REF_SYSTEM_H