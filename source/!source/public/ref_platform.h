//=======================================================================
//			Copyright XashXT Group 2007 ©
//		      ref_platform.h - platform dll header
//=======================================================================
#ifndef REF_PLATFORM_H
#define REF_PLATFORM_H

#include "ref_system.h"

#define PLATFORM_VERSION		0.1

typedef struct platform_api_s
{
	float api_version;

	//base functions
	//xash engine interface
	void ( *plat_init ) ( char *funcname, int argc, char **argv ); //init platform
	void ( *plat_main ) ( void );	//frame
	void ( *plat_free ) ( void );	//close platform

	//memory manager
	void *(*MS_Alloc)		(byte *pool, size_t size, const char *filename, int fileline); 
	void (*MS_Free)		(void *data, const char *filename, int fileline);
	byte *(*MS_AllocPool)	(const char *name, const char *filename, int fileline);
	void (*MS_FreePool)		(byte **pool, const char *filename, int fileline);
	void (*MS_EmptyPool)	(byte *pool, const char *filename, int fileline);

	//timer
	double (*DoubleTime)	( void );
          gameinfo_t (*GameInfo)	( void );
	
	//filesystem
	byte *(*FS_LoadFile)	(const char *path, fs_offset_t *filesizeptr );
	byte *(*FS_LoadImage)	(char *filename, int *width, int *height);
	byte *(*FS_LoadImageData)	(char *filename, char *buffer, int size, int *width, int *height);
	bool (*FS_WriteFile)	(const char *filename, void *data, fs_offset_t len);
	search_t *(*FS_Search)	(const char *pattern, int caseinsensitive, int quiet);
	file_t *(*FS_Open)		(const char* filepath, const char* mode, bool quiet, bool nonblocking);
	fs_offset_t (*FS_Read)	(file_t* file, void* buffer, size_t buffersize);
	fs_offset_t (*FS_Write)	(file_t* file, const void* data, size_t datasize);
	int (*FS_Seek)		(file_t* file, fs_offset_t offset, int whence);
	fs_offset_t (*FS_Tell)	(file_t* file);
	int (*FS_UnGetc)		(file_t* file, byte c);
	int (*FS_Getc)		(file_t* file);
	int (*FS_Printf)		(file_t* file, const char* format, ...);
	int (*FS_Close)		(file_t* file);
	bool (*FS_FileExists)	(const char *filename);
	void (*FS_StripExtension)	(char *path);
	void (*FS_DefaultExtension)	(char *path, const char *extension );
	void (*FS_FileBase)		(char *in, char *out);

}platform_api_t;

//dll handle
typedef platform_api_t (*platform_t)(system_api_t);

#endif//REF_PLATFORM_H