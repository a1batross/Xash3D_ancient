//=======================================================================
//			Copyright XashXT Group 2007 ©
//		  filesystem.c - game filesystem based on DP fs
//=======================================================================

#include <limits.h>
#include <fcntl.h>
#include <direct.h>
#include <sys/stat.h>
#include <io.h>
#include "platform.h"
#include "baseutils.h"
#include "zip32.h"

#define ZIP_DATA_HEADER		0x504B0304  // "PK\3\4"
#define ZIP_CDIR_HEADER		0x504B0102  // "PK\1\2"
#define ZIP_END_HEADER		0x504B0506  // "PK\5\6"
#define ZIP_MAX_COMMENTS_SIZE		((word)0xFFFF)
#define ZIP_END_CDIR_SIZE		22
#define ZIP_CDIR_CHUNK_BASE_SIZE	46
#define ZIP_LOCAL_CHUNK_BASE_SIZE	30

#define FILE_FLAG_PACKED		(1 << 0) // inside a package (PAK or PK3)
#define FILE_FLAG_DEFLATED		(1 << 1) //(PK3 only)
#define FILE_BUFF_SIZE		2048
#define PACKFILE_FLAG_TRUEOFFS	(1 << 0)// the offset in packfile_t is the true contents offset
#define PACKFILE_FLAG_DEFLATED	(1 << 1)// file compressed using the deflate algorithm
#define MAX_FILES_IN_PACK		65536

typedef struct
{
	z_stream		zstream;
	size_t		comp_length;		// length of the compressed file
	size_t		in_ind, in_len;		// input buffer current index and length
	size_t		in_position;		// position in the compressed file
	byte		input [FILE_BUFF_SIZE];
} ztoolkit_t;

struct file_s
{
	int		flags;
	int		handle;			// file descriptor
	fs_offset_t	real_length;		// uncompressed file size (for files opened in "read" mode)
	fs_offset_t	position;			// current position in the file
	fs_offset_t	offset;			// offset into the package (0 if external file)
	int		ungetc;			// single stored character from ungetc, cleared to EOF when read
						// Contents buffer
	fs_offset_t	buff_ind, buff_len;		// buffer current index and length
	byte		buff [FILE_BUFF_SIZE];
	ztoolkit_t*	ztk;			// For zipped files
};

typedef struct vfile_s
{
	byte		*buff;
	file_t		*file;// ptr to real stream
	int		mode;

	fs_offset_t	buffsize;
	fs_offset_t	length;
	fs_offset_t	offset;
};

typedef struct pk3_s
{
	uint		signature;
	word		disknum;
	word		cdir_disknum;	// number of the disk with the start of the central directory
	word		localentries;	// number of entries in the central directory on this disk
	word		nbentries;	// total number of entries in the central directory on this disk
	uint		cdir_size;	// size of the central directory
	uint		cdir_offset;	// with respect to the starting disk number
	word		comment_size;
}pk3_t;

typedef struct dpackfile_s
{
	char		name[56];
	int		filepos;
	int		filelen;
} dpackfile_t;

typedef struct dpackheader_s
{
	char		id[4];
	int		dirofs;
	int		dirlen;
} dpackheader_t;

typedef struct packfile_s
{
	char		name[MAX_STRING];
	int		flags;
	fs_offset_t	offset;
	fs_offset_t	packsize;	// size in the package
	fs_offset_t	realsize;	// real file size (uncompressed)
} packfile_t;

typedef struct pack_s
{
	char		filename [MAX_SYSPATH];
	int		handle;
	int		ignorecase;// PK3 ignores case
	int		numfiles;
	packfile_t	*files;
} pack_t;

typedef struct searchpath_s
{
	char		filename[MAX_SYSPATH];
	pack_t		*pack;
	struct searchpath_s *next;
} searchpath_t;

typedef struct stringlist_s
{
	// maxstrings changes as needed, causing reallocation of strings[] array
	int maxstrings;
	int numstrings;
	char **strings;
} stringlist_t;

byte *fs_mempool;
searchpath_t *fs_searchpaths = NULL;

static void FS_InitMemory( void );
const char *FS_FileExtension (const char *in);
static searchpath_t *FS_FindFile (const char *name, int* index, bool quiet);
static packfile_t* FS_AddFileToPack (const char* name, pack_t* pack, fs_offset_t offset, fs_offset_t packsize, fs_offset_t realsize, int flags);
static bool FS_SysFileExists (const char *path);

char fs_rootdir[ MAX_SYSPATH ]; //engine root directory
char fs_basedir[ MAX_SYSPATH ]; //base directory of game
char fs_gamedir[ MAX_SYSPATH ]; //game current directory
char gs_basedir[ MAX_SYSPATH ]; //initial dir before loading gameinfo.txt (used for compilers too)
char gs_mapname[ 64 ]; //used for compilers only

//command ilne parms
int fs_argc;
char **fs_argv;

gameinfo_t GI;

/*
=============================================================================

PRIVATE FUNCTIONS - PK3 HANDLING

=============================================================================
*/
/*
====================
PK3_GetEndOfCentralDir

Extract the end of the central directory from a PK3 package
====================
*/
bool PK3_GetEndOfCentralDir (const char *packfile, int packhandle, pk3_t *eocd)
{
	fs_offset_t filesize, maxsize;
	byte *buffer, *ptr;
	int ind;

	// Get the package size
	filesize = lseek (packhandle, 0, SEEK_END);
	if (filesize < ZIP_END_CDIR_SIZE) return false;

	// Load the end of the file in memory
	if (filesize < ZIP_MAX_COMMENTS_SIZE + ZIP_END_CDIR_SIZE)
		maxsize = filesize;
	else maxsize = ZIP_MAX_COMMENTS_SIZE + ZIP_END_CDIR_SIZE;
	buffer = (byte *)Malloc (maxsize);
	lseek (packhandle, filesize - maxsize, SEEK_SET);
	if (read (packhandle, buffer, maxsize) != (fs_offset_t) maxsize)
	{
		Free (buffer);
		return false;
	}
	
	// Look for the end of central dir signature around the end of the file
	maxsize -= ZIP_END_CDIR_SIZE;
	ptr = &buffer[maxsize];
	ind = 0;
	while (BuffBigLong(ptr) != ZIP_END_HEADER)
	{
		if (ind == maxsize)
		{
			Free (buffer);
			return false;
		}
		ind++;
		ptr--;
	}

	memcpy (eocd, ptr, ZIP_END_CDIR_SIZE);

	eocd->signature = LittleLong (eocd->signature);
	eocd->disknum = LittleShort (eocd->disknum);
	eocd->cdir_disknum = LittleShort (eocd->cdir_disknum);
	eocd->localentries = LittleShort (eocd->localentries);
	eocd->nbentries = LittleShort (eocd->nbentries);
	eocd->cdir_size = LittleLong (eocd->cdir_size);
	eocd->cdir_offset = LittleLong (eocd->cdir_offset);
	eocd->comment_size = LittleShort (eocd->comment_size);
          
	Free (buffer);

	return true;
}


/*
====================
PK3_BuildFileList

Extract the file list from a PK3 file
====================
*/
int PK3_BuildFileList (pack_t *pack, const pk3_t *eocd)
{
	byte *central_dir, *ptr;
	uint ind;
	fs_offset_t remaining;

	// Load the central directory in memory
	central_dir = (byte *)Malloc(eocd->cdir_size);
	lseek (pack->handle, eocd->cdir_offset, SEEK_SET);
	read (pack->handle, central_dir, eocd->cdir_size);

	// Extract the files properties
	// The parsing is done "by hand" because some fields have variable sizes and
	// the constant part isn't 4-bytes aligned, which makes the use of structs difficult
	remaining = eocd->cdir_size;
	pack->numfiles = 0;
	ptr = central_dir;
	for (ind = 0; ind < eocd->nbentries; ind++)
	{
		fs_offset_t namesize, count;

		// Checking the remaining size
		if (remaining < ZIP_CDIR_CHUNK_BASE_SIZE)
		{
			Free (central_dir);
			return -1;
		}
		remaining -= ZIP_CDIR_CHUNK_BASE_SIZE;

		// Check header
		if (BuffBigLong (ptr) != ZIP_CDIR_HEADER)
		{
			Free (central_dir);
			return -1;
		}

		namesize = BuffLittleShort (&ptr[28]);	// filename length

		// Check encryption, compression, and attributes
		// 1st uint8  : general purpose bit flag
		//    Check bits 0 (encryption), 3 (data descriptor after the file), and 5 (compressed patched data (?))
		// 2nd uint8 : external file attributes
		//    Check bits 3 (file is a directory) and 5 (file is a volume (?))
		if ((ptr[8] & 0x29) == 0 && (ptr[38] & 0x18) == 0)
		{
			// Still enough bytes for the name?
			if (remaining < namesize || namesize >= (int)sizeof (*pack->files))
			{
				Free (central_dir);
				return -1;
			}

			// WinZip doesn't use the "directory" attribute, so we need to check the name directly
			if (ptr[ZIP_CDIR_CHUNK_BASE_SIZE + namesize - 1] != '/')
			{
				char filename [sizeof (pack->files[0].name)];
				fs_offset_t offset, packsize, realsize;
				int flags;

				// Extract the name (strip it if necessary)
				namesize = min(namesize, (int)sizeof (filename) - 1);
				memcpy (filename, &ptr[ZIP_CDIR_CHUNK_BASE_SIZE], namesize);
				filename[namesize] = '\0';

				if (BuffLittleShort (&ptr[10]))
					flags = PACKFILE_FLAG_DEFLATED;
				else
					flags = 0;
				offset = BuffLittleLong (&ptr[42]);
				packsize = BuffLittleLong (&ptr[20]);
				realsize = BuffLittleLong (&ptr[24]);
				FS_AddFileToPack (filename, pack, offset, packsize, realsize, flags);
			}
		}

		// Skip the name, additionnal field, and comment
		// 1er uint16 : extra field length
		// 2eme uint16 : file comment length
		count = namesize + BuffLittleShort (&ptr[30]) + BuffLittleShort (&ptr[32]);
		ptr += ZIP_CDIR_CHUNK_BASE_SIZE + count;
		remaining -= count;
	}
	// If the package is empty, central_dir is NULL here
	if (central_dir != NULL) Free (central_dir);
	return pack->numfiles;
}


/*
====================
FS_LoadPackPK3

Create a package entry associated with a PK3 file
====================
*/
pack_t *FS_LoadPackPK3 (const char *packfile)
{
	int packhandle;
	pk3_t eocd;
	pack_t *pack;
	int real_nb_files;

	packhandle = open(packfile, O_RDONLY | O_BINARY);
	if (packhandle < 0) return NULL;
	
	if (!PK3_GetEndOfCentralDir (packfile, packhandle, &eocd))
	{
		Msg("%s is not a PK3 file\n", packfile);
		close(packhandle);
		return NULL;
	}

	// Multi-volume ZIP archives are NOT allowed
	if (eocd.disknum != 0 || eocd.cdir_disknum != 0)
	{
		Msg("%s is a multi-volume ZIP archive\n", packfile);
		close(packhandle);
		return NULL;
	}

	// Create a package structure in memory
	pack = (pack_t *)Mem_Alloc(fs_mempool, sizeof (pack_t));
	pack->ignorecase = true; //PK3 ignores case
	strncpy (pack->filename, packfile, sizeof (pack->filename));
	pack->handle = packhandle;
	pack->numfiles = eocd.nbentries;
	pack->files = (packfile_t *)Mem_Alloc(fs_mempool, eocd.nbentries * sizeof(packfile_t));

	real_nb_files = PK3_BuildFileList (pack, &eocd);
	if (real_nb_files < 0)
	{
		Msg("%s is not a valid PK3 file\n", packfile);
		close(pack->handle);
		Free(pack);
		return NULL;
	}

	MsgDev("Adding packfile %s (%i files)\n", packfile, real_nb_files);
	return pack;
}

/*
====================
PK3_GetTrueFileOffset

Find where the true file data offset is
====================
*/
bool PK3_GetTrueFileOffset (packfile_t *pfile, pack_t *pack)
{
	byte buffer [ZIP_LOCAL_CHUNK_BASE_SIZE];
	fs_offset_t count;

	// Already found?
	if (pfile->flags & PACKFILE_FLAG_TRUEOFFS) return true;

	// Load the local file description
	lseek (pack->handle, pfile->offset, SEEK_SET);
	count = read (pack->handle, buffer, ZIP_LOCAL_CHUNK_BASE_SIZE);
	if (count != ZIP_LOCAL_CHUNK_BASE_SIZE || BuffBigLong (buffer) != ZIP_DATA_HEADER)
	{
		Msg("Can't retrieve file %s in package %s\n", pfile->name, pack->filename);
		return false;
	}

	// Skip name and extra field
	pfile->offset += BuffLittleShort (&buffer[26]) + BuffLittleShort (&buffer[28]) + ZIP_LOCAL_CHUNK_BASE_SIZE;
	pfile->flags |= PACKFILE_FLAG_TRUEOFFS;
	return true;
}

/*
=============================================================================

FILEMATCH COMMON SYSTEM

=============================================================================
*/
int matchpattern(const char *in, const char *pattern, bool caseinsensitive)
{
	int c1, c2;
	while (*pattern)
	{
		switch (*pattern)
		{
		case 0:   return 1; // end of pattern
		case '?': // match any single character
			if (*in == 0 || *in == '/' || *in == '\\' || *in == ':') return 0; // no match
			in++;
			pattern++;
			break;
		case '*': // match anything until following string
			if (!*in) return 1; // match
			pattern++;
			while (*in)
			{
				if (*in == '/' || *in == '\\' || *in == ':') break;
				// see if pattern matches at this offset
				if (matchpattern(in, pattern, caseinsensitive)) return 1;
				// nope, advance to next offset
				in++;
			}
			break;
		default:
			if (*in != *pattern)
			{
				if (!caseinsensitive) return 0; // no match
				c1 = *in;
				if (c1 >= 'A' && c1 <= 'Z') c1 += 'a' - 'A';
				c2 = *pattern;
				if (c2 >= 'A' && c2 <= 'Z') c2 += 'a' - 'A';
				if (c1 != c2) return 0; // no match
			}
			in++;
			pattern++;
			break;
		}
	}
	if (*in) return 0; // reached end of pattern but not end of input
	return 1; // success
}

void stringlistinit(stringlist_t *list)
{
	memset(list, 0, sizeof(*list));
}

void stringlistfreecontents(stringlist_t *list)
{
	int i;
	for (i = 0;i < list->numstrings;i++)
	{
		if (list->strings[i]) Free(list->strings[i]);
		list->strings[i] = NULL;
	}
	list->numstrings = 0;
	list->maxstrings = 0;
	if (list->strings) Free(list->strings);
}

void stringlistappend(stringlist_t *list, char *text)
{
	size_t textlen;
	char **oldstrings;

	if (list->numstrings >= list->maxstrings)
	{
		oldstrings = list->strings;
		list->maxstrings += 4096;
		list->strings = Z_Malloc(list->maxstrings * sizeof(*list->strings));
		if (list->numstrings) Mem_Copy(list->strings, oldstrings, list->numstrings * sizeof(*list->strings));
		if (oldstrings) Free(oldstrings);
	}
	textlen = strlen(text) + 1;
	list->strings[list->numstrings] = Z_Malloc(textlen);
	Mem_Copy(list->strings[list->numstrings], text, textlen);
	list->numstrings++;
}

void stringlistsort(stringlist_t *list)
{
	int i, j;
	char *temp;
	// this is a selection sort (finds the best entry for each slot)
	for (i = 0;i < list->numstrings - 1;i++)
	{
		for (j = i + 1;j < list->numstrings;j++)
		{
			if (strcmp(list->strings[i], list->strings[j]) > 0)
			{
				temp = list->strings[i];
				list->strings[i] = list->strings[j];
				list->strings[j] = temp;
			}
		}
	}
}

void listdirectory(stringlist_t *list, const char *path)
{
	int i;
	char pattern[4096], *c;
	struct _finddata_t n_file;
	long hFile;
	strncpy (pattern, path, sizeof (pattern));
	strncat (pattern, "*", sizeof (pattern));
	// ask for the directory listing handle
	hFile = _findfirst(pattern, &n_file);
	if(hFile == -1)
		return;
	// start a new chain with the the first name
	stringlistappend(list, n_file.name);
	// iterate through the directory
	while (_findnext(hFile, &n_file) == 0)
		stringlistappend(list, n_file.name);
	_findclose(hFile);

	// convert names to lowercase because windows does not care, but pattern matching code often does
	for (i = 0;i < list->numstrings;i++)
		for (c = list->strings[i];*c;c++)
			if (*c >= 'A' && *c <= 'Z')
				*c += 'a' - 'A';
}

/*
=============================================================================

OTHER PRIVATE FUNCTIONS

=============================================================================
*/

/*
====================
FS_AddFileToPack

Add a file to the list of files contained into a package
====================
*/
static packfile_t* FS_AddFileToPack (const char* name, pack_t* pack, fs_offset_t offset, fs_offset_t packsize, fs_offset_t realsize, int flags)
{
	int (*strcmp_funct) (const char* str1, const char* str2);
	int left, right, middle;
	packfile_t *pfile;

	strcmp_funct = pack->ignorecase ? stricmp : strcmp;

	// Look for the slot we should put that file into (binary search)
	left = 0;
	right = pack->numfiles - 1;
	while (left <= right)
	{
		int diff;

		middle = (left + right) / 2;
		diff = strcmp_funct (pack->files[middle].name, name);

		// If we found the file, there's a problem
		if (!diff) Msg ("Package %s contains the file %s several times\n", pack->filename, name);

		// If we're too far in the list
		if (diff > 0) right = middle - 1;
		else left = middle + 1;
	}

	// We have to move the right of the list by one slot to free the one we need
	pfile = &pack->files[left];
	memmove (pfile + 1, pfile, (pack->numfiles - left) * sizeof (*pfile));
	pack->numfiles++;

	strncpy (pfile->name, name, sizeof (pfile->name));
	pfile->offset = offset;
	pfile->packsize = packsize;
	pfile->realsize = realsize;
	pfile->flags = flags;

	return pfile;
}


/*
============
FS_CreatePath

Only used for FS_Open.
============
*/
void FS_CreatePath (char *path)
{
	char *ofs, save;

	for (ofs = path+1 ; *ofs ; ofs++)
	{
		if (*ofs == '/' || *ofs == '\\')
		{
			// create the directory
			save = *ofs;
			*ofs = 0;
			_mkdir (path);
			*ofs = save;
		}
	}
}


/*
============
FS_Path

debug info
============
*/
void FS_Path (void)
{
	searchpath_t *s;

	MsgDev("Current search path:\n");
	for (s=fs_searchpaths ; s ; s=s->next)
	{
		if (s->pack) MsgDev("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		else MsgDev("%s\n", s->filename);
	}
}

/*
============
FS_FileBase

Extracts the base name of a file (no path, no extension, assumes '/' as path separator)
============
*/
void FS_FileBase (char *in, char *out)
{
	int len, start, end;

	len = strlen( in );
	
	// scan backward for '.'
	end = len - 1;
	while ( end && in[end] != '.' && in[end] != '/' && in[end] != '\\' )
		end--;
	
	if ( in[end] != '.' ) end = len-1; // no '.', copy to end
	else end--; // Found ',', copy to left of '.'


	// Scan backward for '/'
	start = len - 1;
	while ( start >= 0 && in[start] != '/' && in[start] != '\\' )
		start--;

	if ( start < 0 || ( in[start] != '/' && in[start] != '\\' ) )
		start = 0;
	else  start++;

	// Length of new sting
	len = end - start + 1;

	if(in[start] == '#')start++;

	// Copy partial string
	strncpy( out, &in[start], len );
	out[len] = 0;
}

/*
=================
FS_LoadPackPAK

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
pack_t *FS_LoadPackPAK (const char *packfile)
{
	dpackheader_t header;
	int i, numpackfiles;
	int packhandle;
	pack_t *pack;
	dpackfile_t *info;

	packhandle = open (packfile, O_RDONLY | O_BINARY);
	if (packhandle < 0) return NULL;
	read (packhandle, (void *)&header, sizeof(header));
	if (memcmp(header.id, "PACK", 4))
	{
		Msg("%s is not a packfile\n", packfile);
		close(packhandle);
		return NULL;
	}
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	if (header.dirlen % sizeof(dpackfile_t))
	{
		Msg("%s has an invalid directory size\n", packfile);
		close(packhandle);
		return NULL;
	}

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

	if (numpackfiles > MAX_FILES_IN_PACK)
	{
		Msg("%s has %i files\n", packfile, numpackfiles);
		close(packhandle);
		return NULL;
	}

	info = (dpackfile_t *)Malloc( sizeof(*info) * numpackfiles);
	lseek (packhandle, header.dirofs, SEEK_SET);
	if(header.dirlen != read (packhandle, (void *)info, header.dirlen))
	{
		Msg("%s is an incomplete PAK, not loading\n", packfile);
		Free(info);
		close(packhandle);
		return NULL;
	}

	pack = (pack_t *)Mem_Alloc(fs_mempool, sizeof (pack_t));
	pack->ignorecase = false; // PAK is case sensitive
	strncpy (pack->filename, packfile, sizeof (pack->filename));
	pack->handle = packhandle;
	pack->numfiles = 0;
	pack->files = (packfile_t *)Mem_Alloc(fs_mempool, numpackfiles * sizeof(packfile_t));

	// parse the directory
	for (i = 0;i < numpackfiles;i++)
	{
		fs_offset_t offset = LittleLong (info[i].filepos);
		fs_offset_t size = LittleLong (info[i].filelen);
		FS_AddFileToPack (info[i].name, pack, offset, size, size, PACKFILE_FLAG_TRUEOFFS);
	}

	Free(info);
	MsgDev("Adding packfile %s (%i files)\n", packfile, numpackfiles);
	return pack;
}

/*
================
FS_AddPack_Fullpath

Adds the given pack to the search path.
The pack type is autodetected by the file extension.

Returns true if the file was successfully added to the
search path or if it was already included.

If keep_plain_dirs is set, the pack will be added AFTER the first sequence of
plain directories.
================
*/
static bool FS_AddPack_Fullpath(const char *pakfile, bool *already_loaded, bool keep_plain_dirs)
{
	searchpath_t *search;
	pack_t *pak = NULL;
	const char *ext = FS_FileExtension(pakfile);
	
	for(search = fs_searchpaths; search; search = search->next)
	{
		if(search->pack && !stricmp(search->pack->filename, pakfile))
		{
			if(already_loaded) *already_loaded = true;
			return true; // already loaded
		}
	}

	if(already_loaded) *already_loaded = false;

	if(!stricmp(ext, "pak")) pak = FS_LoadPackPAK (pakfile);
	else if(!stricmp(ext, "pk2")) pak = FS_LoadPackPK3 (pakfile);
	else if(!stricmp(ext, "pk3")) pak = FS_LoadPackPK3 (pakfile);
	else Msg("\"%s\" does not have a pack extension\n", pakfile);

	if (pak)
	{
		if(keep_plain_dirs)
		{
			// find the first item whose next one is a pack or NULL
			searchpath_t *insertion_point = 0;
			if(fs_searchpaths && !fs_searchpaths->pack)
			{
				insertion_point = fs_searchpaths;
				for(;;)
				{
					if(!insertion_point->next) break;
					if(insertion_point->next->pack) break;
					insertion_point = insertion_point->next;
				}
			}
			// If insertion_point is NULL, this means that either there is no
			// item in the list yet, or that the very first item is a pack. In
			// that case, we want to insert at the beginning...
			if(!insertion_point)
			{
				search = (searchpath_t *)Mem_Alloc(fs_mempool, sizeof(searchpath_t));
				search->pack = pak;
				search->next = fs_searchpaths;
				fs_searchpaths = search;
			}
			else // otherwise we want to append directly after insertion_point.
			{
				search = (searchpath_t *)Mem_Alloc(fs_mempool, sizeof(searchpath_t));
				search->pack = pak;
				search->next = insertion_point->next;
				insertion_point->next = search;
			}
		}
		else
		{
			search = (searchpath_t *)Mem_Alloc(fs_mempool, sizeof(searchpath_t));
			search->pack = pak;
			search->next = fs_searchpaths;
			fs_searchpaths = search;
		}
		return true;
	}
	else
	{
		MsgDev("unable to load pak \"%s\"\n", pakfile);
		return false;
	}
}


/*
================
FS_AddPack

Adds the given pack to the search path and searches for it in the game path.
The pack type is autodetected by the file extension.

Returns true if the file was successfully added to the
search path or if it was already included.

If keep_plain_dirs is set, the pack will be added AFTER the first sequence of
plain directories.
================
*/
bool FS_AddPack(const char *pakfile, bool *already_loaded, bool keep_plain_dirs)
{
	char fullpath[ MAX_STRING ];
	int index;
	searchpath_t *search;

	if(already_loaded) *already_loaded = false;

	// then find the real name...
	search = FS_FindFile(pakfile, &index, true);
	if(!search || search->pack)
	{
		MsgDev("could not find pak \"%s\"\n", pakfile);
		return false;
	}
	sprintf(fullpath, "%s%s", search->filename, pakfile);
	return FS_AddPack_Fullpath(fullpath, already_loaded, keep_plain_dirs);
}

/*
================
FS_AddGameDirectory

Sets fs_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ...
================
*/
void FS_AddGameDirectory (const char *dir)
{
	int i;
	stringlist_t list;
	searchpath_t *search;
	char pakfile[MAX_OSPATH];

	strncpy (fs_gamedir, dir, sizeof (fs_gamedir));

	stringlistinit(&list);
	listdirectory(&list, dir);
	stringlistsort(&list);

	// add any PAK package in the directory
	for (i = 0;i < list.numstrings;i++)
	{
		if (!strcasecmp(FS_FileExtension(list.strings[i]), "pak"))
		{
			sprintf (pakfile, "%s%s", dir, list.strings[i]);
			FS_AddPack_Fullpath(pakfile, NULL, false);
		}
	}

	// add any PK3 package in the directory
	for (i = 0;i < list.numstrings;i++)
	{
		if (!strcasecmp(FS_FileExtension(list.strings[i]), "pk3"))
		{
			sprintf (pakfile, "%s%s", dir, list.strings[i]);
			FS_AddPack_Fullpath(pakfile, NULL, false);
		}
	}

	stringlistfreecontents(&list);

	// Add the directory to the search path
	// (unpacked files have the priority over packed files)
	search = (searchpath_t *)Mem_Alloc(fs_mempool, sizeof(searchpath_t));
	strncpy (search->filename, dir, sizeof (search->filename));
	search->next = fs_searchpaths;
	fs_searchpaths = search;
}


/*
================
FS_AddGameHierarchy
================
*/
void FS_AddGameHierarchy (const char *dir)
{
	// Add the common game directory
	if(dir || *dir)FS_AddGameDirectory (va("%s%s/", fs_basedir, dir));
}


/*
============
FS_FileExtension
============
*/
const char *FS_FileExtension (const char *in)
{
	const char *separator, *backslash, *colon, *dot;

	separator = strrchr(in, '/');
	backslash = strrchr(in, '\\');
	if (!separator || separator < backslash) separator = backslash;
	colon = strrchr(in, ':');
	if (!separator || separator < colon) separator = colon;

	dot = strrchr(in, '.');
	if (dot == NULL || (separator && (dot < separator)))
		return "";

	return dot + 1;
}


/*
============
FS_FileWithoutPath
============
*/
const char *FS_FileWithoutPath (const char *in)
{
	const char *separator, *backslash, *colon;

	separator = strrchr(in, '/');
	backslash = strrchr(in, '\\');
	if (!separator || separator < backslash) separator = backslash;
	colon = strrchr(in, ':');
	if (!separator || separator < colon) separator = colon;
	return separator ? separator + 1 : in;
}

/*
============
FS_ExtractFilePath
============
*/
void FS_ExtractFilePath(const char* const path, char* dest)
{
	const char* src;
	src = path + strlen(path) - 1;

	// back up until a \ or the start
	while (src != path && !PATHSEPARATOR(*(src - 1)))
		src--;

	memcpy(dest, path, src - path);
	dest[src - path] = 0;
}

/*
================
FS_ClearSearchPath
================
*/
void FS_ClearSearchPath (void)
{
	while (fs_searchpaths)
	{
		searchpath_t *search = fs_searchpaths;
		fs_searchpaths = search->next;
		if (search->pack)
		{
			if (search->pack->files) 
				Free(search->pack->files);
			Free(search->pack);
		}
		Free(search);
	}
}

/*
====================
FS_CheckNastyPath

Return true if the path should be rejected due to one of the following:
1: path elements that are non-portable
2: path elements that would allow access to files outside the game directory,
   or are just not a good idea for a mod to be using.
====================
*/
int FS_CheckNastyPath (const char *path, bool isgamedir)
{
	// all: never allow an empty path, as for gamedir it would access the parent directory and a non-gamedir path it is just useless
	if (!path[0]) return 2;

	// Windows: don't allow \ in filenames (windows-only), period.
	// (on Windows \ is a directory separator, but / is also supported)
	if (strstr(path, "\\")) return 1; // non-portable

	// Mac: don't allow Mac-only filenames - : is a directory separator
	// instead of /, but we rely on / working already, so there's no reason to
	// support a Mac-only path
	// Amiga and Windows: : tries to go to root of drive
	if (strstr(path, ":")) return 1; // non-portable attempt to go to root of drive

	// Amiga: // is parent directory
	if (strstr(path, "//")) return 1; // non-portable attempt to go to parent directory

	// all: don't allow going to parent directory (../ or /../)
	if (strstr(path, "..")) return 2; // attempt to go outside the game directory

	// Windows and UNIXes: don't allow absolute paths
	if (path[0] == '/') return 2; // attempt to go outside the game directory

	// all: don't allow . characters before the last slash (it should only be used in filenames, not path elements), this catches all imaginable cases of ./, ../, .../, etc
	if (strchr(path, '.'))
	{
		if (isgamedir) return 2; // gamedir is entirely path elements, so simply forbid . entirely
		if (strchr(path, '.') < strrchr(path, '/')) return 2; // possible attempt to go outside the game directory
	}

	// all: forbid trailing slash on gamedir
	if (isgamedir && path[strlen(path)-1] == '/') return 2;

	// all: forbid leading dot on any filename for any reason
	if (strstr(path, "/.")) return 2; // attempt to go outside the game directory

	// after all these checks we're pretty sure it's a / separated filename
	// and won't do much if any harm
	return false;
}

/*
================
FS_Rescan
================
*/
void FS_Rescan (void)
{
	FS_ClearSearchPath();

	FS_AddGameHierarchy (GI.basedir);
	FS_AddGameHierarchy (GI.gamedir);
}

void FS_ResetGameInfo( void )
{
	strcpy(GI.title, gs_basedir );	
	strcpy(GI.key, "DEMO" );	
	GI.version = 0.0f;
	GI.viewmode = 1;
	GI.gamemode = 1;
}

void FS_CreateGameInfo( const char *filename )
{
	char *buffer = Malloc( MAX_SYSPATH );

	// make simply gameinfo.txt
	strcat(buffer, "// generated by Xash3D\r\r\nbasedir\t\"xash\"\n");//add new string
	strcat(buffer, va("gamedir\t\"%s\"\n", gs_basedir ));
	strcat(buffer, "title\t\"New Game\"\rversion\t\"1.0\"\rviewmode\t\"firstperson\"\r");
	strcat(buffer, "gamemode\t\"singleplayer\"\rgamekey\t\"\"");

	FS_WriteFile( filename, buffer, strlen(buffer));
	Free( buffer );
} 

void FS_LoadGameInfo( const char *filename )
{
          bool fs_modified = false;
	bool load = false;
	char *fs_path;
	
	//prepare to loading
	FS_ClearSearchPath();
	FS_AddGameHierarchy( gs_basedir );
	FS_ResetGameInfo();

	// create default gameinfo
	if(!FS_FileExists( filename )) FS_CreateGameInfo( filename );

	//now we have use search path and can load gameinfo.txt
	load = FS_LoadScript( filename, NULL, 0 );
	
	while( load )
	{
		if(!SC_GetToken( true )) break;
		if(SC_MatchToken( "basedir"))
		{
			fs_path = SC_GetToken( false );

			if(!SC_MatchToken(GI.basedir))
			{
				strcpy(GI.basedir, fs_path);
				fs_modified = true;
			}
		}
		else if(SC_MatchToken( "gamedir"))
		{
			fs_path = SC_GetToken( false );
			if(!SC_MatchToken(GI.gamedir))
			{
				strcpy(GI.gamedir, fs_path);
				fs_modified = true;
			}
		}
		else if(SC_MatchToken( "title"))
			strcpy(GI.title, SC_GetToken( false ));
		else if(SC_MatchToken( "version"))
			GI.version = atof(SC_GetToken( false ));
		else if(SC_MatchToken( "viewmode"))
		{
			SC_GetToken( false );
			if(SC_MatchToken( "firstperson")) GI.viewmode = 1;
			if(SC_MatchToken( "thirdperson")) GI.viewmode = 2;
		}
		else if(SC_MatchToken( "gamemode"))								
		{
			SC_GetToken( false );
			if(SC_MatchToken( "singleplayer")) GI.gamemode = 1;
			if(SC_MatchToken( "multiplayer")) GI.gamemode = 2;
		}
		else if(SC_MatchToken( "gamekey"))
			strcpy(GI.key, SC_GetToken( false ));
	}
	if(fs_modified) FS_Rescan(); //create new filesystem

	FS_Path(); //debug
}

/*
================
FS_Init
================
*/
void FS_Init( int argc, char **argv )
{
	char		szTemp[4096];
	stringlist_t	dirs;
	int		i;
	
	FS_InitMemory();
	FS_InitCmdLine( argc, argv );

	stringlistinit(&dirs);
	listdirectory(&dirs, "./");
	stringlistsort(&dirs);

	if(!FS_GetParmFromCmdLine("-game", gs_basedir ))
	{
		if( GetModuleFileName( NULL, szTemp, MAX_SYSPATH ))
			FS_FileBase( szTemp, gs_basedir );
		else strcpy(gs_basedir, "xash" );//default dir
	}

	// checked nasty path
	if(FS_CheckNastyPath(gs_basedir, true ) || !stricmp("bin", gs_basedir )) // "bin" it's a reserved word
	{
		Msg("FS_Init: invalid game directory \"%s\". Reset to default.\n", gs_basedir );		
		strcpy(gs_basedir, "xash" );//default dir
	}
	
	//validate directories
	for (i = 0; i < dirs.numstrings; i++)
	{
		if(!stricmp(gs_basedir, dirs.strings[i]))
		break;
	}

	if(i == dirs.numstrings) strcpy(gs_basedir, "xash" );//default dir
	stringlistfreecontents(&dirs);
}

void FS_InitRootDir( char *path )
{
	char	szTemp[4096];

	//just set cwd
	GetModuleFileName( NULL, szTemp, MAX_SYSPATH );
	FS_ExtractFilePath( szTemp, szTemp );	
	SetCurrentDirectory ( szTemp );

	FS_ClearSearchPath();
	FS_AddGameHierarchy( path );

	FS_Path();
}

bool FS_GetParmFromCmdLine( char *parm, char *out )
{
	int argc = FS_CheckParm( parm );

	if(!argc) return false;
	if(!out) return false;	

	strcpy( out, fs_argv[argc+1]);
	return true;
}

/*
================
FS_Shutdown
================
*/
void FS_Shutdown (void)
{
	Mem_FreePool (&fs_mempool);
}

/*
====================
FS_SysOpen

Internal function used to create a file_t and open the relevant non-packed file on disk
====================
*/
static file_t* FS_SysOpen (const char* filepath, const char* mode, bool nonblocking)
{
	file_t* file;
	int mod, opt;
	unsigned int ind;

	// Parse the mode string
	switch (mode[0])
	{
		case 'r':
			mod = O_RDONLY;
			opt = 0;
			break;
		case 'w':
			mod = O_WRONLY;
			opt = O_CREAT | O_TRUNC;
			break;
		case 'a':
			mod = O_WRONLY;
			opt = O_CREAT | O_APPEND;
			break;
		default:
			Msg("FS_SysOpen(%s, %s): invalid mode\n", filepath, mode);
			return NULL;
	}
	for (ind = 1; mode[ind] != '\0'; ind++)
	{
		switch (mode[ind])
		{
			case '+':
				mod = O_RDWR;
				break;
			case 'b':
				opt |= O_BINARY;
				break;
			default:
				Msg("FS_SysOpen(%s, %s): unknown character in mode (%c)\n", filepath, mode, mode[ind]);
		}
	}
	if (nonblocking) opt |= O_NONBLOCK;

	file = (file_t *)Mem_Alloc (fs_mempool, sizeof (*file));
	memset (file, 0, sizeof (*file));
	file->ungetc = EOF;

	file->handle = open (filepath, mod | opt, 0666);
	if (file->handle < 0)
	{
		Free (file);
		return NULL;
	}
	file->real_length = lseek (file->handle, 0, SEEK_END);

	// For files opened in append mode, we start at the end of the file
	if (mod & O_APPEND) file->position = file->real_length;
	else lseek (file->handle, 0, SEEK_SET);

	return file;
}


/*
===========
FS_OpenPackedFile

Open a packed file using its package file descriptor
===========
*/
file_t *FS_OpenPackedFile (pack_t* pack, int pack_ind)
{
	packfile_t *pfile;
	int dup_handle;
	file_t* file;

	pfile = &pack->files[pack_ind];

	// If we don't have the true offset, get it now
	if (! (pfile->flags & PACKFILE_FLAG_TRUEOFFS))
		if (!PK3_GetTrueFileOffset (pfile, pack))
			return NULL;

	if (lseek (pack->handle, pfile->offset, SEEK_SET) == -1)
	{
		Msg("FS_OpenPackedFile: can't lseek to %s in %s (offset: %d)\n", pfile->name, pack->filename, pfile->offset);
		return NULL;
	}

	dup_handle = dup (pack->handle);
	if (dup_handle < 0)
	{
		Msg("FS_OpenPackedFile: can't dup package's handle (pack: %s)\n", pack->filename);
		return NULL;
	}

	file = (file_t *)Mem_Alloc (fs_mempool, sizeof (*file));
	memset (file, 0, sizeof (*file));
	file->handle = dup_handle;
	file->flags = FILE_FLAG_PACKED;
	file->real_length = pfile->realsize;
	file->offset = pfile->offset;
	file->position = 0;
	file->ungetc = EOF;

	if (pfile->flags & PACKFILE_FLAG_DEFLATED)
	{
		ztoolkit_t *ztk;

		file->flags |= FILE_FLAG_DEFLATED;

		// We need some more variables
		ztk = (ztoolkit_t *)Mem_Alloc (fs_mempool, sizeof (*ztk));
		ztk->comp_length = pfile->packsize;

		// Initialize zlib stream
		ztk->zstream.next_in = ztk->input;
		ztk->zstream.avail_in = 0;

		if (inflateInit2 (&ztk->zstream, -MAX_WBITS) != Z_OK)
		{
			Msg("FS_OpenPackedFile: inflate init error (file: %s)\n", pfile->name);
			close(dup_handle);
			Mem_Free(file);
			return NULL;
		}
		else Msg("zip32: %s\n", ztk->zstream.msg );

		ztk->zstream.next_out = file->buff;
		ztk->zstream.avail_out = sizeof (file->buff);
		file->ztk = ztk;
	}
	return file;
}

/*
==================
FS_SysFileExists

Look for a file in the filesystem only
==================
*/
bool FS_SysFileExists (const char *path)
{
	int desc;
     
	desc = open (path, O_RDONLY | O_BINARY);

	if (desc < 0) return false;
	close (desc);
	return true;
}

/*
====================
FS_FindFile

Look for a file in the packages and in the filesystem

Return the searchpath where the file was found (or NULL)
and the file index in the package if relevant
====================
*/
static searchpath_t *FS_FindFile (const char *name, int* index, bool quiet)
{
	searchpath_t *search;
	pack_t *pak;

	// search through the path, one element at a time
	for (search = fs_searchpaths; search; search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
			int (*strcmp_funct) (const char* str1, const char* str2);
			int left, right, middle;

			pak = search->pack;
			strcmp_funct = pak->ignorecase ? stricmp : strcmp;

			// Look for the file (binary search)
			left = 0;
			right = pak->numfiles - 1;
			while (left <= right)
			{
				int diff;

				middle = (left + right) / 2;
				diff = strcmp_funct (pak->files[middle].name, name);

				// Found it
				if (!diff)
				{
					if (!quiet) MsgDev("FS_FindFile: %s in %s\n", pak->files[middle].name, pak->filename);
					if (index != NULL) *index = middle;
					return search;
				}

				// If we're too far in the list
				if (diff > 0) right = middle - 1;
				else left = middle + 1;
			}
		}
		else
		{
			char netpath[MAX_SYSPATH];
			sprintf(netpath, "%s%s", search->filename, name);
			if (FS_SysFileExists(netpath))
			{
				if (!quiet) MsgDev("FS_FindFile: %s\n", netpath);
				if (index != NULL) *index = -1;
				return search;
			}
		}
	}

	if (!quiet) MsgDev("FS_FindFile: can't find %s\n", name);
	if (index != NULL) *index = -1;

	return NULL;
}


/*
===========
FS_OpenReadFile

Look for a file in the search paths and open it in read-only mode
===========
*/
file_t *FS_OpenReadFile (const char *filename, const char *mode, bool quiet, bool nonblocking)
{
	searchpath_t *search;
	int pack_ind;

	search = FS_FindFile (filename, &pack_ind, quiet);

	// Not found?
	if (search == NULL) return NULL; 

	// Found in the filesystem?
	if (pack_ind < 0)
	{
		char path [MAX_SYSPATH];
		sprintf (path, "%s%s", search->filename, filename);
		return FS_SysOpen (path, mode, nonblocking);
	}

	// So, we found it in a package...
	return FS_OpenPackedFile (search->pack, pack_ind);
}

/*
=============================================================================

MAIN PUBLIC FUNCTIONS

=============================================================================
*/

/*
====================
FS_Open

Open a file. The syntax is the same as fopen
====================
*/
file_t* _FS_Open (const char* filepath, const char* mode, bool quiet, bool nonblocking)
{
	if (FS_CheckNastyPath(filepath, false))
	{
		Sys_Error("FS_Open(\"%s\", \"%s\", %s): nasty filename rejected\n", filepath, mode, quiet ? "true" : "false");
		return NULL;
	}

	// If the file is opened in "write", "append", or "read/write" mode
	if (mode[0] == 'w' || mode[0] == 'a' || strchr (mode, '+'))
	{
		char real_path [MAX_SYSPATH];

		// Open the file on disk directly
		sprintf (real_path, "%s/%s", fs_gamedir, filepath);
		Msg("create path %s\n", real_path );
		FS_CreatePath (real_path);// Create directories up to the file
		return FS_SysOpen (real_path, mode, nonblocking);
	}
	
	// Else, we look at the various search paths and open the file in read-only mode
	return FS_OpenReadFile (filepath, mode, quiet, nonblocking);
}

file_t* FS_Open (const char* filepath, const char* mode )
{
	return _FS_Open (filepath, mode, true, false );
}

/*
====================
FS_Close

Close a file
====================
*/
int FS_Close (file_t* file)
{
	if (close (file->handle)) return EOF;

	if (file->ztk)
	{
		inflateEnd (&file->ztk->zstream);
		Free (file->ztk);
	}
	Free (file);
	return 0;
}


/*
====================
FS_Write

Write "datasize" bytes into a file
====================
*/
fs_offset_t FS_Write (file_t* file, const void* data, size_t datasize)
{
	fs_offset_t result;

	// If necessary, seek to the exact file position we're supposed to be
	if (file->buff_ind != file->buff_len)
		lseek (file->handle, file->buff_ind - file->buff_len, SEEK_CUR);

	// Purge cached data
	FS_Purge (file);

	// Write the buffer and update the position
	result = write (file->handle, data, (fs_offset_t)datasize);
	file->position = lseek (file->handle, 0, SEEK_CUR);
	if (file->real_length < file->position)
		file->real_length = file->position;

	if (result < 0) return 0;
	return result;
}


/*
====================
FS_Read

Read up to "buffersize" bytes from a file
====================
*/
fs_offset_t FS_Read (file_t* file, void* buffer, size_t buffersize)
{
	fs_offset_t count, done;

	if (buffersize == 0) return 0;

	// Get rid of the ungetc character
	if (file->ungetc != EOF)
	{
		((char*)buffer)[0] = file->ungetc;
		buffersize--;
		file->ungetc = EOF;
		done = 1;
	}
	else done = 0;

	// First, we copy as many bytes as we can from "buff"
	if (file->buff_ind < file->buff_len)
	{
		count = file->buff_len - file->buff_ind;

		done += ((fs_offset_t)buffersize > count) ? count : (fs_offset_t)buffersize;
		memcpy (buffer, &file->buff[file->buff_ind], done);
		file->buff_ind += done;

		buffersize -= done;
		if (buffersize == 0) return done;
	}

	// NOTE: at this point, the read buffer is always empty

	// If the file isn't compressed
	if (! (file->flags & FILE_FLAG_DEFLATED))
	{
		fs_offset_t nb;

		// We must take care to not read after the end of the file
		count = file->real_length - file->position;

		// If we have a lot of data to get, put them directly into "buffer"
		if (buffersize > sizeof (file->buff) / 2)
		{
			if (count > (fs_offset_t)buffersize)
				count = (fs_offset_t)buffersize;
			lseek (file->handle, file->offset + file->position, SEEK_SET);
			nb = read (file->handle, &((unsigned char*)buffer)[done], count);
			if (nb > 0)
			{
				done += nb;
				file->position += nb;
				// Purge cached data
				FS_Purge (file);
			}
		}
		else
		{
			if (count > (fs_offset_t)sizeof (file->buff))
				count = (fs_offset_t)sizeof (file->buff);
			lseek (file->handle, file->offset + file->position, SEEK_SET);
			nb = read (file->handle, file->buff, count);
			if (nb > 0)
			{
				file->buff_len = nb;
				file->position += nb;

				// Copy the requested data in "buffer" (as much as we can)
				count = (fs_offset_t)buffersize > file->buff_len ? file->buff_len : (fs_offset_t)buffersize;
				memcpy (&((unsigned char*)buffer)[done], file->buff, count);
				file->buff_ind = count;
				done += count;
			}
		}

		return done;
	}

	// If the file is compressed, it's more complicated...
	// We cycle through a few operations until we have read enough data
	while (buffersize > 0)
	{
		ztoolkit_t *ztk = file->ztk;
		int error;

		// NOTE: at this point, the read buffer is always empty

		// If "input" is also empty, we need to refill it
		if (ztk->in_ind == ztk->in_len)
		{
			// If we are at the end of the file
			if (file->position == file->real_length)
				return done;

			count = (fs_offset_t)(ztk->comp_length - ztk->in_position);
			if (count > (fs_offset_t)sizeof (ztk->input))
				count = (fs_offset_t)sizeof (ztk->input);
			lseek (file->handle, file->offset + (fs_offset_t)ztk->in_position, SEEK_SET);
			if (read (file->handle, ztk->input, count) != count)
			{
				Msg("FS_Read: unexpected end of file\n");
				break;
			}
			ztk->in_ind = 0;
			ztk->in_len = count;
			ztk->in_position += count;
		}

		ztk->zstream.next_in = &ztk->input[ztk->in_ind];
		ztk->zstream.avail_in = (unsigned int)(ztk->in_len - ztk->in_ind);

		// Now that we are sure we have compressed data available, we need to determine
		// if it's better to inflate it in "file->buff" or directly in "buffer"

		// Inflate the data in "file->buff"
		if (buffersize < sizeof (file->buff) / 2)
		{
			ztk->zstream.next_out = file->buff;
			ztk->zstream.avail_out = sizeof (file->buff);
			error = inflate (&ztk->zstream, Z_SYNC_FLUSH);
			if (error != Z_OK && error != Z_STREAM_END)
			{
				Msg("FS_Read: Can't inflate file: %s\n", ztk->zstream.msg );
				break;
			}
			ztk->in_ind = ztk->in_len - ztk->zstream.avail_in;

			file->buff_len = (fs_offset_t)sizeof (file->buff) - ztk->zstream.avail_out;
			file->position += file->buff_len;

			// Copy the requested data in "buffer" (as much as we can)
			count = (fs_offset_t)buffersize > file->buff_len ? file->buff_len : (fs_offset_t)buffersize;
			memcpy (&((unsigned char*)buffer)[done], file->buff, count);
			file->buff_ind = count;
		}
		else // Else, we inflate directly in "buffer"
		{
			ztk->zstream.next_out = &((unsigned char*)buffer)[done];
			ztk->zstream.avail_out = (unsigned int)buffersize;
			error = inflate (&ztk->zstream, Z_SYNC_FLUSH);
			if (error != Z_OK && error != Z_STREAM_END)
			{
				Msg("FS_Read: Can't inflate file: %s\n", ztk->zstream.msg);
				break;
			}
			ztk->in_ind = ztk->in_len - ztk->zstream.avail_in;

			// How much data did it inflate?
			count = (fs_offset_t)(buffersize - ztk->zstream.avail_out);
			file->position += count;

			// Purge cached data
			FS_Purge (file);
		}
		done += count;
		buffersize -= count;
	}

	return done;
}


/*
====================
FS_Print

Print a string into a file
====================
*/
int FS_Print (file_t* file, const char *msg)
{
	return (int)FS_Write (file, msg, strlen (msg));
}

/*
====================
FS_Printf

Print a string into a file
====================
*/
int FS_Printf(file_t* file, const char* format, ...)
{
	int result;
	va_list args;

	va_start (args, format);
	result = FS_VPrintf (file, format, args);
	va_end (args);

	return result;
}


/*
====================
FS_VPrintf

Print a string into a file
====================
*/
int FS_VPrintf (file_t* file, const char* format, va_list ap)
{
	int len;
	fs_offset_t buff_size = MAX_INPUTLINE;
	char *tempbuff;

	while( true )
	{
		tempbuff = (char *)Malloc(buff_size);
		len = vsprintf (tempbuff, format, ap);
		if (len >= 0 && len < buff_size) break;
		Free (tempbuff);
		buff_size *= 2;
	}

	len = write (file->handle, tempbuff, len);
	Mem_Free (tempbuff);

	return len;
}

/*
====================
FS_Getc

Get the next character of a file
====================
*/
int FS_Getc (file_t* file)
{
	char c;

	if (FS_Read (file, &c, 1) != 1)
		return EOF;

	return c;
}


/*
====================
FS_UnGetc

Put a character back into the read buffer (only supports one character!)
====================
*/
int FS_UnGetc (file_t* file, unsigned char c)
{
	// If there's already a character waiting to be read
	if (file->ungetc != EOF)
		return EOF;

	file->ungetc = c;
	return c;
}

int FS_Gets (file_t* file, byte *string, size_t bufsize )
{
	int c, end = 0;

	while( 1 )
	{
		c = FS_Getc( file );
		if (c == '\r' || c == '\n' || c < 0)
			break;
		if (end < bufsize - 1)
			string[end++] = c;
	}
	string[end] = 0;

	// remove \n following \r
	if (c == '\r')
	{
		c = FS_Getc(file);
		if (c != '\n') FS_UnGetc(file, (byte)c);
	}
	MsgDev("FS_Gets: %s\n", string);

	return c;
}

/*
====================
FS_Seek

Move the position index in a file
====================
*/
int FS_Seek (file_t* file, fs_offset_t offset, int whence)
{
	ztoolkit_t *ztk;
	unsigned char* buffer;
	fs_offset_t buffersize;

	// Compute the file offset
	switch (whence)
	{
		case SEEK_CUR:
			offset += file->position - file->buff_len + file->buff_ind;
			break;
		case SEEK_SET:
			break;
		case SEEK_END:
			offset += file->real_length;
			break;

		default: 
			return -1;
	}
	
	if (offset < 0 || offset > (long)file->real_length) return -1;

	// If we have the data in our read buffer, we don't need to actually seek
	if (file->position - file->buff_len <= offset && offset <= file->position)
	{
		file->buff_ind = offset + file->buff_len - file->position;
		return 0;
	}

	// Purge cached data
	FS_Purge (file);

	// Unpacked or uncompressed files can seek directly
	if (! (file->flags & FILE_FLAG_DEFLATED))
	{
		if (lseek (file->handle, file->offset + offset, SEEK_SET) == -1)
			return -1;
		file->position = offset;
		return 0;
	}

	// Seeking in compressed files is more a hack than anything else,
	// but we need to support it, so here we go.
	ztk = file->ztk;

	// If we have to go back in the file, we need to restart from the beginning
	if (offset <= file->position)
	{
		ztk->in_ind = 0;
		ztk->in_len = 0;
		ztk->in_position = 0;
		file->position = 0;
		lseek (file->handle, file->offset, SEEK_SET);

		// Reset the Zlib stream
		ztk->zstream.next_in = ztk->input;
		ztk->zstream.avail_in = 0;
		inflateReset (&ztk->zstream);
	}

	// We need a big buffer to force inflating into it directly
	buffersize = 2 * sizeof (file->buff);
	buffer = (unsigned char *)Malloc(buffersize);

	// Skip all data until we reach the requested offset
	while (offset > file->position)
	{
		fs_offset_t diff = offset - file->position;
		fs_offset_t count, len;

		count = (diff > buffersize) ? buffersize : diff;
		len = FS_Read (file, buffer, count);
		if (len != count)
		{
			Free (buffer);
			return -1;
		}
	}
	Free (buffer);
	return 0;
}


/*
====================
FS_Tell

Give the current position in a file
====================
*/
fs_offset_t FS_Tell (file_t* file)
{
	return file->position - file->buff_len + file->buff_ind;
}

bool FS_Eof( file_t* file)
{
	return (file->position == file->real_length) ? true : false;
}

/*
====================
FS_Purge

Erases any buffered input or output data
====================
*/
void FS_Purge (file_t* file)
{
	file->buff_len = 0;
	file->buff_ind = 0;
	file->ungetc = EOF;
}


/*
============
FS_LoadFile

Filename are relative to the xash directory.
Always appends a 0 byte.
============
*/
byte *FS_LoadFile (const char *path, fs_offset_t *filesizeptr )
{
	file_t *file;
	byte *buf = NULL;
	fs_offset_t filesize = 0;

	file = _FS_Open (path, "rb", true, false);
	if (file)
	{
		filesize = file->real_length;
		buf = (byte *)Malloc(filesize + 1);
		buf[filesize] = '\0';
		FS_Read (file, buf, filesize);
		FS_Close (file);
	}
	
	if (filesizeptr) *filesizeptr = filesize;
	return buf;
}


/*
============
FS_WriteFile

The filename will be prefixed by the current game directory
============
*/
bool FS_WriteFile (const char *filename, void *data, fs_offset_t len)
{
	file_t *file;

	file = _FS_Open (filename, "wb", false, false);
	if (!file)
	{
		Msg("FS_WriteFile: failed on %s\n", filename);
		return false;
	}

	MsgDev("FS_WriteFile: %s\n", filename);
	FS_Write (file, data, len);
	FS_Close (file);
	return true;
}

/*
=============================================================================

OTHERS PUBLIC FUNCTIONS

=============================================================================
*/

/*
============
FS_StripExtension
============
*/
void FS_StripExtension (char *path)
{
	int	length;

	length = strlen(path)-1;
	while (length > 0 && path[length] != '.')
	{
		length--;
		if (path[length] == '/' || path[length] == '\\' || path[length] == ':')
			return; // no extension
	}
	if (length) path[length] = 0;
}


/*
==================
FS_DefaultExtension
==================
*/
void FS_DefaultExtension (char *path, const char *extension )
{
	const char *src;

	// if path doesn't have a .EXT, append extension
	// (extension should include the .)
	src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		// it has an extension
		if (*src == '.') return;                 
		src--;
	}
	strcat (path, extension );
}

/*
==================
FS_FileExists

Look for a file in the packages and in the filesystem
==================
*/
bool FS_FileExists (const char *filename)
{
	return (FS_FindFile(filename, NULL, true) != NULL);
}


/*
==================
FS_FileSize

return size of file in bytes
==================
*/
fs_offset_t FS_FileSize (const char *filename)
{
	file_t	*fp;
	int	length = 0;
	
	fp = _FS_Open(filename, "rb", true, false );

	if (fp)
	{
		// it exists
		FS_Seek(fp, 0, SEEK_END);
		length = FS_Tell(fp);
		FS_Close(fp);
	}

	return length;
}


/*
===========
FS_Search

Allocate and fill a search structure with information on matching filenames.
===========
*/
static search_t *_FS_Search(const char *pattern, int caseinsensitive, int quiet )
{
	search_t *search;
	searchpath_t *searchpath;
	pack_t *pak;
	int i, basepathlength, numfiles, numchars, resultlistindex, dirlistindex;
	stringlist_t resultlist;
	stringlist_t dirlist;
	const char *slash, *backslash, *colon, *separator;
	char *basepath;
	char netpath[MAX_OSPATH];
	char temp[MAX_OSPATH];

	for (i = 0;pattern[i] == '.' || pattern[i] == ':' || pattern[i] == '/' || pattern[i] == '\\';i++)
		;

	if (i > 0)
	{
		Msg("Don't use punctuation at the beginning of a search pattern!\n");
		return NULL;
	}

	stringlistinit(&resultlist);
	stringlistinit(&dirlist);
	search = NULL;
	slash = strrchr(pattern, '/');
	backslash = strrchr(pattern, '\\');
	colon = strrchr(pattern, ':');
	separator = max(slash, backslash);
	separator = max(separator, colon);
	basepathlength = separator ? (separator + 1 - pattern) : 0;
	basepath = Malloc(basepathlength + 1);
	if (basepathlength) memcpy(basepath, pattern, basepathlength);
	basepath[basepathlength] = 0;

	// search through the path, one element at a time
	for (searchpath = fs_searchpaths;searchpath;searchpath = searchpath->next)
	{
		// is the element a pak file?
		if (searchpath->pack)
		{
			// look through all the pak file elements
			pak = searchpath->pack;
			for (i = 0;i < pak->numfiles;i++)
			{
				strncpy(temp, pak->files[i].name, sizeof(temp));
				while (temp[0])
				{
					if (matchpattern(temp, (char *)pattern, true))
					{
						for (resultlistindex = 0;resultlistindex < resultlist.numstrings;resultlistindex++)
							if (!strcmp(resultlist.strings[resultlistindex], temp))
								break;
						if (resultlistindex == resultlist.numstrings)
						{
							stringlistappend(&resultlist, temp);
							if (!quiet) MsgDev("SearchPackFile: %s : %s\n", pak->filename, temp);
						}
					}
					// strip off one path element at a time until empty
					// this way directories are added to the listing if they match the pattern
					slash = strrchr(temp, '/');
					backslash = strrchr(temp, '\\');
					colon = strrchr(temp, ':');
					separator = temp;
					if (separator < slash)
						separator = slash;
					if (separator < backslash)
						separator = backslash;
					if (separator < colon)
						separator = colon;
					*((char *)separator) = 0;
				}
			}
		}
		else
		{
			// get a directory listing and look at each name
			sprintf(netpath, "%s%s", searchpath->filename, basepath);
			stringlistinit(&dirlist);
			listdirectory(&dirlist, netpath);
			for (dirlistindex = 0;dirlistindex < dirlist.numstrings;dirlistindex++)
			{
				sprintf(temp, "%s%s", basepath, dirlist.strings[dirlistindex]);
				if (matchpattern(temp, (char *)pattern, true))
				{
					for (resultlistindex = 0;resultlistindex < resultlist.numstrings;resultlistindex++)
						if (!strcmp(resultlist.strings[resultlistindex], temp))
							break;
					if (resultlistindex == resultlist.numstrings)
					{
						stringlistappend(&resultlist, temp);
						if (!quiet) MsgDev("SearchDirFile: %s\n", temp);
					}
				}
			}
			stringlistfreecontents(&dirlist);
		}
	}

	if (resultlist.numstrings)
	{
		stringlistsort(&resultlist);
		numfiles = resultlist.numstrings;
		numchars = 0;
		for (resultlistindex = 0;resultlistindex < resultlist.numstrings;resultlistindex++)
			numchars += (int)strlen(resultlist.strings[resultlistindex]) + 1;
		search = Z_Malloc(sizeof(search_t) + numchars + numfiles * sizeof(char *));
		search->filenames = (char **)((char *)search + sizeof(search_t));
		search->filenamesbuffer = (char *)((char *)search + sizeof(search_t) + numfiles * sizeof(char *));
		search->numfilenames = (int)numfiles;
		numfiles = 0;
		numchars = 0;
		for (resultlistindex = 0;resultlistindex < resultlist.numstrings;resultlistindex++)
		{
			size_t textlen;
			search->filenames[numfiles] = search->filenamesbuffer + numchars;
			textlen = strlen(resultlist.strings[resultlistindex]) + 1;
			memcpy(search->filenames[numfiles], resultlist.strings[resultlistindex], textlen);
			numfiles++;
			numchars += (int)textlen;
		}
	}
	stringlistfreecontents(&resultlist);

	Free(basepath);
	return search;
}

search_t *FS_Search(const char *pattern, int caseinsensitive )
{
	return _FS_Search( pattern, caseinsensitive, true );
}

void FS_FreeSearch(search_t *search)
{
	Free(search);
}

void FS_InitCmdLine( int argc, char **argv )
{
	fs_argc = argc;
	fs_argv = argv;
}

void FS_InitMemory( void )
{
	fs_mempool = Mem_AllocPool( "file management" );	

	// add a path separator to the end of the basedir if it lacks one
	if (fs_basedir[0] && fs_basedir[strlen(fs_basedir) - 1] != '/' && fs_basedir[strlen(fs_basedir) - 1] != '\\')
		strncat(fs_basedir, "/", sizeof(fs_basedir));
}

/*
================
FS_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int FS_CheckParm (const char *parm)
{
	int i;

	for (i = 1; i < fs_argc; i++ )
	{
		// NEXTSTEP sometimes clears appkit vars.
		if (!fs_argv[i]) continue;
		if (!strcmp (parm, fs_argv[i])) return i;
	}
	return 0;
}

/*
=============================================================================

EXTERNAL FILESYSTEM INTERFACE
=============================================================================
*/
filesystem_api_t FS_GetAPI( void )
{
	static filesystem_api_t	fs;

	fs.api_size = sizeof(filesystem_api_t);

	fs.FileBase = FS_FileBase;
	fs.FileExists = FS_FileExists;
	fs.FileSize = FS_FileSize;
	fs.FileExtension = FS_FileExtension;
	fs.FileWithoutPath = FS_FileWithoutPath;
	fs.StripExtension = FS_StripExtension;
	fs.StripFilePath = FS_ExtractFilePath;
	fs.DefaultExtension = FS_DefaultExtension;
	fs.ClearSearchPath = FS_ClearSearchPath;

	fs.Search = FS_Search;
	fs.FreeSearch = FS_FreeSearch;

	fs.Open = FS_Open;
	fs.Close = FS_Close;
	fs.Write = FS_Write;
	fs.Read = FS_Read;
	fs.Print = FS_Print;
	fs.Printf = FS_Printf;
	fs.Gets = FS_Gets;
	fs.Seek = FS_Seek;
	fs.Tell = FS_Tell;

	fs.LoadFile = FS_LoadFile;
	fs.WriteFile = FS_WriteFile;
	fs.LoadImage = FS_LoadImage;
	fs.FreeImage = FS_FreeImage;

	return fs;
}

/*
=============================================================================

VIRTUAL FILE SYSTEM - WRITE DATA INTO MEMORY

=============================================================================
*/
vfile_t *VFS_Create(byte *buffer, size_t buffsize)
{
	vfile_t *file = (vfile_t *)Mem_Alloc (fs_mempool, sizeof (*file));

	file->file = NULL; //no real stream
	file->length = file->buffsize = buffsize;
	file->buff = Mem_Alloc(fs_mempool, (file->buffsize));	
	file->offset = 0;
	file->mode = O_RDONLY;
	Mem_Copy(file->buff, buffer, buffsize );

	return file;
}

vfile_t *VFS_Open(file_t* real_file, const char* mode)
{
	vfile_t *file = (vfile_t *)Mem_Alloc (fs_mempool, sizeof (*file));

	// If the file is opened in "write", "append", or "read/write" mode
	if (mode[0] == 'w')
	{
		file->file = real_file;
		file->buffsize = (64 * 1024); // will be resized if need
		file->buff = Mem_Alloc(fs_mempool, (file->buffsize));
		file->length = 0;
		file->offset = 0;
		file->mode = O_WRONLY;
	}
	else if (mode[0] == 'r')
	{
		int curpos, endpos;

		file->file = real_file;

		curpos = FS_Tell(file->file);
		FS_Seek(file->file, 0, SEEK_END);
		endpos = FS_Tell(file->file);
		FS_Seek(file->file, curpos, SEEK_SET);

		file->buffsize = endpos - curpos;
		file->buff = Mem_Alloc(fs_mempool, (file->buffsize));

		FS_Read (file->file, file->buff, file->buffsize);		
		file->length = file->buffsize;
		file->offset = 0;
		file->mode = O_RDONLY;
	}
	else
	{
		Msg("VFS_Open: unsupported mode %s\n", mode );
		return NULL;
	}
	return file;
}

fs_offset_t VFS_Read( vfile_t* file, void* buffer, size_t buffersize)
{
	if (buffersize == 0) return 0;

	//check for enough room
	if(file->offset >= file->length)
	{
		return -1; //hit EOF
	}
	if(file->offset + buffersize <= file->length)
	{
		Mem_Copy( buffer, file->buff + file->offset, buffersize );
		file->offset += buffersize;
	}
	else
	{
		int reduced_size = file->length - file->offset;
		Mem_Copy( buffer, file->buff + file->offset, reduced_size );
		file->offset += reduced_size;
		Msg("VFS_Read: warning, vfs buffer is out\n");
	}
	return 1;
}

fs_offset_t VFS_Write( vfile_t *file, const void *buf, size_t size )
{
	if(!file) return -1;

	if (file->offset + size >= file->buffsize)
	{
		int newsize = file->offset + size + (64 * 1024);

		if (file->buffsize < newsize)
		{
			//reallocate buffer now
			file->buff = Mem_Realloc(fs_mempool, file->buff, newsize );		
			file->buffsize = newsize; //merge buffsize
		}
	}

	// write into buffer
	Mem_Copy(file->buff + file->offset, (byte *)buf, size );
	file->offset += size;

	if (file->offset > file->length) 
		file->length = file->offset;

	return file->length;
}

fs_offset_t VFS_Tell (vfile_t* file)
{
	return file->offset;
}

int VFS_Seek( vfile_t *file, fs_offset_t offset, int whence )
{
	// Compute the file offset
	switch (whence)
	{
		case SEEK_CUR:
			offset += file->offset;
			break;
		case SEEK_SET:
			break;
		case SEEK_END:
			offset += file->length;
			break;
		default: 
			return -1;
	}

	if (offset < 0 || offset > (long)file->length) return -1;

	file->offset = offset;
	return 0;
}

int VFS_Close( vfile_t *file )
{
	if(!file) return -1;

	if(file->mode == O_WRONLY)
	{
		// write real file into disk
		FS_Write (file->file, file->buff, (file->length + 3) & ~3);// align
	}

	Free( file->buff );
	Free( file ); //himself

	return 0;
}

/*
=============================================================================

VIRTUAL FILESYSTEM INTERFACE
=============================================================================
*/
vfilesystem_api_t VFS_GetAPI( void )
{
	static vfilesystem_api_t	vfs;

	vfs.api_size = sizeof(vfilesystem_api_t);

	vfs.Create = VFS_Create;
	vfs.Open = VFS_Open;
	vfs.Close = VFS_Close;
	vfs.Write = VFS_Write;
	vfs.Read = VFS_Read;
	vfs.Seek = VFS_Seek;
	vfs.Tell = VFS_Tell;

	return vfs;
}