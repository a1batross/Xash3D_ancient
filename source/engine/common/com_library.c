//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        com_library.c - custom dlls loader
//=======================================================================

#include "common.h"
#include "com_library.h"

// FIXME: broken code
//#define	EXPORT_FROM_MEMORY

typedef struct
{
	PIMAGE_NT_HEADERS	headers;
	byte		*codeBase;
	void		**modules;
	int		numModules;
	int		initialized;
} MEMORYMODULE, *PMEMORYMODULE;

typedef BOOL (WINAPI *DllEntryProc)( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved );

#define GET_HEADER_DICTIONARY( module, idx )	&(module)->headers->OptionalHeader.DataDirectory[idx]
#define CALCULATE_ADDRESS( base, offset )	(((DWORD)(base)) + (offset))

static void CopySections( const byte *data, PIMAGE_NT_HEADERS old_headers, PMEMORYMODULE module )
{
	int 	i, size;
	byte	*dest;
	byte	*codeBase = module->codeBase;
	PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(module->headers);

	for( i = 0; i < module->headers->FileHeader.NumberOfSections; i++, section++ )
	{
		if( section->SizeOfRawData == 0 )
		{
			// section doesn't contain data in the dll itself, but may define
			// uninitialized data
			size = old_headers->OptionalHeader.SectionAlignment;
			if( size > 0 )
			{
				dest = (byte *)VirtualAlloc((byte *)CALCULATE_ADDRESS(codeBase, section->VirtualAddress), size, MEM_COMMIT, PAGE_READWRITE );
				section->Misc.PhysicalAddress = (DWORD)dest;
				Mem_Set( dest, 0, size );
			}
			// section is empty
			continue;
		}

		// commit memory block and copy data from dll
		dest = (byte *)VirtualAlloc((byte *)CALCULATE_ADDRESS(codeBase, section->VirtualAddress), section->SizeOfRawData, MEM_COMMIT, PAGE_READWRITE );
		Mem_Copy( dest, (byte *)CALCULATE_ADDRESS(data, section->PointerToRawData), section->SizeOfRawData );
		section->Misc.PhysicalAddress = (DWORD)dest;
	}
}

static void FreeSections( PIMAGE_NT_HEADERS old_headers, PMEMORYMODULE module )
{
	int	i, size;
	byte	*codeBase = module->codeBase;
	PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(module->headers);

	for( i = 0; i < module->headers->FileHeader.NumberOfSections; i++, section++ )
	{
		if( section->SizeOfRawData == 0 )
		{
			size = old_headers->OptionalHeader.SectionAlignment;
			if( size > 0 )
			{
				VirtualFree( codeBase + section->VirtualAddress, size, MEM_DECOMMIT );
				section->Misc.PhysicalAddress = 0;
			}
			continue;
		}

		VirtualFree( codeBase + section->VirtualAddress, section->SizeOfRawData, MEM_DECOMMIT );
		section->Misc.PhysicalAddress = 0;
	}
}

// Protection flags for memory pages (Executable, Readable, Writeable)
static int ProtectionFlags[2][2][2] =
{
{
{ PAGE_NOACCESS, PAGE_WRITECOPY },		// not executable
{ PAGE_READONLY, PAGE_READWRITE },
},
{
{ PAGE_EXECUTE, PAGE_EXECUTE_WRITECOPY },	// executable
{ PAGE_EXECUTE_READ, PAGE_EXECUTE_READWRITE },
},
};

static void FinalizeSections( MEMORYMODULE *module )
{
	PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION( module->headers );
	int	i;
	
	// loop through all sections and change access flags
	for( i = 0; i < module->headers->FileHeader.NumberOfSections; i++, section++ )
	{
		DWORD	protect, oldProtect, size;
		int	executable = (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
		int	readable = (section->Characteristics & IMAGE_SCN_MEM_READ) != 0;
		int	writeable = (section->Characteristics & IMAGE_SCN_MEM_WRITE) != 0;

		if( section->Characteristics & IMAGE_SCN_MEM_DISCARDABLE )
		{
			// section is not needed any more and can safely be freed
			VirtualFree((LPVOID)section->Misc.PhysicalAddress, section->SizeOfRawData, MEM_DECOMMIT);
			continue;
		}

		// determine protection flags based on characteristics
		protect = ProtectionFlags[executable][readable][writeable];
		if( section->Characteristics & IMAGE_SCN_MEM_NOT_CACHED )
			protect |= PAGE_NOCACHE;

		// determine size of region
		size = section->SizeOfRawData;
		if( size == 0 )
		{
			if( section->Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA )
				size = module->headers->OptionalHeader.SizeOfInitializedData;
			else if( section->Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA )
				size = module->headers->OptionalHeader.SizeOfUninitializedData;
		}

		if( size > 0 )
		{         
			// change memory access flags
			if( !VirtualProtect((LPVOID)section->Misc.PhysicalAddress, size, protect, &oldProtect ))
				Host_Error( "Com_FinalizeSections: error protecting memory page\n" );
		}
	}
}

static void PerformBaseRelocation( MEMORYMODULE *module, DWORD delta )
{
	DWORD	i;
	byte	*codeBase = module->codeBase;
	PIMAGE_DATA_DIRECTORY directory = GET_HEADER_DICTIONARY( module, IMAGE_DIRECTORY_ENTRY_BASERELOC );

	if( directory->Size > 0 )
	{
		PIMAGE_BASE_RELOCATION relocation = (PIMAGE_BASE_RELOCATION)CALCULATE_ADDRESS( codeBase, directory->VirtualAddress );
		for( ; relocation->VirtualAddress > 0; )
		{
			byte	*dest = (byte *)CALCULATE_ADDRESS( codeBase, relocation->VirtualAddress );
			word	*relInfo = (word *)((byte *)relocation + IMAGE_SIZEOF_BASE_RELOCATION );

			for( i = 0; i<((relocation->SizeOfBlock-IMAGE_SIZEOF_BASE_RELOCATION) / 2); i++, relInfo++ )
			{
				DWORD	*patchAddrHL;
				int	type, offset;

				// the upper 4 bits define the type of relocation
				type = *relInfo >> 12;
				// the lower 12 bits define the offset
				offset = *relInfo & 0xfff;
				
				switch( type )
				{
				case IMAGE_REL_BASED_ABSOLUTE:
					// skip relocation
					break;
				case IMAGE_REL_BASED_HIGHLOW:
					// change complete 32 bit address
					patchAddrHL = (DWORD *)CALCULATE_ADDRESS( dest, offset );
					*patchAddrHL += delta;
					break;
				default:
					MsgDev( D_ERROR, "PerformBaseRelocation: unknown relocation: %d\n", type );
					break;
				}
			}

			// advance to next relocation block
			relocation = (PIMAGE_BASE_RELOCATION)CALCULATE_ADDRESS( relocation, relocation->SizeOfBlock );
		}
	}
}

static int BuildImportTable( MEMORYMODULE *module )
{
	int	result=1;
	byte	*codeBase = module->codeBase;
	PIMAGE_DATA_DIRECTORY directory = GET_HEADER_DICTIONARY( module, IMAGE_DIRECTORY_ENTRY_IMPORT );

	if( directory->Size > 0 )
	{
		PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)CALCULATE_ADDRESS( codeBase, directory->VirtualAddress );

		for( ; !IsBadReadPtr( importDesc, sizeof( IMAGE_IMPORT_DESCRIPTOR )) && importDesc->Name; importDesc++ )
		{
			DWORD	*thunkRef, *funcRef;
			void	*handle;

#ifdef EXPORT_FROM_MEMORY
			char	libpath[MAX_SYSPATH];

			FS_AllowDirectPaths( true );			
			Com_BuildPath((LPCSTR)CALCULATE_ADDRESS(codeBase, importDesc->Name), libpath );
			handle = Com_LoadLibrary( libpath );
			FS_AllowDirectPaths( false );
#else
			handle = LoadLibrary((LPCSTR)CALCULATE_ADDRESS(codeBase, importDesc->Name));
#endif                               
			if( handle == NULL )
			{
				MsgDev( D_ERROR, "couldn't load library\n" );
				result = 0;
				break;
			}

			module->modules = (void *)Z_Realloc( module->modules, (module->numModules + 1) * (sizeof( void* )));
			if( module->modules == NULL )
			{
				result = 0;
				break;
			}

			module->modules[module->numModules++] = handle;
			if( importDesc->OriginalFirstThunk )
			{
				thunkRef = (DWORD *)CALCULATE_ADDRESS( codeBase, importDesc->OriginalFirstThunk );
				funcRef = (DWORD *)CALCULATE_ADDRESS( codeBase, importDesc->FirstThunk );
			}
			else
			{
				// no hint table
				thunkRef = (DWORD *)CALCULATE_ADDRESS( codeBase, importDesc->FirstThunk );
				funcRef = (DWORD *)CALCULATE_ADDRESS( codeBase, importDesc->FirstThunk );
			}
			for( ; *thunkRef; thunkRef++, funcRef++ )
			{
				if( IMAGE_SNAP_BY_ORDINAL( *thunkRef ))
				{
#ifdef EXPORT_FROM_MEMORY
					*funcRef = (DWORD)Com_GetProcAddress( handle, (LPCSTR)IMAGE_ORDINAL( *thunkRef ));
#else
					*funcRef = (DWORD)GetProcAddress( handle, (LPCSTR)IMAGE_ORDINAL( *thunkRef ));
#endif
				}
				else
				{
					PIMAGE_IMPORT_BY_NAME thunkData = (PIMAGE_IMPORT_BY_NAME)CALCULATE_ADDRESS( codeBase, *thunkRef );
#ifdef EXPORT_FROM_MEMORY
					*funcRef = (DWORD)Com_GetProcAddress( handle, (LPCSTR)&thunkData->Name );
#else
					*funcRef = (DWORD)GetProcAddress( handle, (LPCSTR)&thunkData->Name );
#endif
				}
				if( *funcRef == 0 )
				{
					result = 0;
					break;
				}
			}
			if( !result ) break;
		}
	}
	return result;
}

void *Com_LoadLibrary( const char *name )
{
	MEMORYMODULE	*result;
	PIMAGE_DOS_HEADER	dos_header;
	PIMAGE_NT_HEADERS	old_header;
	byte		*code, *headers;
	DWORD		locationDelta;
	DllEntryProc	DllEntry;
	BOOL		successfull;
	void		*data;

	data = FS_LoadFile( name, NULL );
	if( !data ) return NULL;

	dos_header = (PIMAGE_DOS_HEADER)data;
	if( dos_header->e_magic != IMAGE_DOS_SIGNATURE )
	{
		MsgDev( D_NOTE, "%s it's not a valid executable file\n", name );
		Mem_Free( data );
		return NULL;
	}

	old_header = (PIMAGE_NT_HEADERS)&((const byte *)(data))[dos_header->e_lfanew];
	if( old_header->Signature != IMAGE_NT_SIGNATURE )
	{
		MsgDev( D_NOTE, "library %s: no PE header found\n", name );
		Mem_Free( data );
		return NULL;
	}

	// reserve memory for image of library
	code = (byte *)VirtualAlloc((LPVOID)(old_header->OptionalHeader.ImageBase), old_header->OptionalHeader.SizeOfImage, MEM_RESERVE, PAGE_READWRITE );

	if( code == NULL )
	{
		// try to allocate memory at arbitrary position
		code = (byte *)VirtualAlloc( NULL, old_header->OptionalHeader.SizeOfImage, MEM_RESERVE, PAGE_READWRITE );
	}    
	if( code == NULL )
	{
		MsgDev( D_NOTE, "library %s: can't reserve memory\n", name );
		Mem_Free( data );
		return NULL;
	}

	result = (MEMORYMODULE *)HeapAlloc( GetProcessHeap(), 0, sizeof( MEMORYMODULE ));
	result->codeBase = code;
	result->numModules = 0;
	result->modules = NULL;
	result->initialized = 0;

	// XXX: is it correct to commit the complete memory region at once?
	// calling DllEntry raises an exception if we don't...
	VirtualAlloc( code, old_header->OptionalHeader.SizeOfImage, MEM_COMMIT, PAGE_READWRITE );

	// commit memory for headers
	headers = (byte *)VirtualAlloc( code, old_header->OptionalHeader.SizeOfHeaders, MEM_COMMIT, PAGE_READWRITE );
	
	// copy PE header to code
	Mem_Copy( headers, dos_header, dos_header->e_lfanew + old_header->OptionalHeader.SizeOfHeaders );
	result->headers = (PIMAGE_NT_HEADERS)&((const byte *)(headers))[dos_header->e_lfanew];

	// update position
	result->headers->OptionalHeader.ImageBase = (DWORD)code;

	// copy sections from DLL file block to new memory location
	CopySections( data, old_header, result );

	// adjust base address of imported data
	locationDelta = (DWORD)(code - old_header->OptionalHeader.ImageBase);
	if( locationDelta != 0 ) PerformBaseRelocation( result, locationDelta );

	// load required dlls and adjust function table of imports
	if( !BuildImportTable( result )) goto error;

	// mark memory pages depending on section headers and release
	// sections that are marked as "discardable"
	FinalizeSections( result );

	// get entry point of loaded library
	if( result->headers->OptionalHeader.AddressOfEntryPoint != 0 )
	{
		DllEntry = (DllEntryProc)CALCULATE_ADDRESS( code, result->headers->OptionalHeader.AddressOfEntryPoint );
		if( DllEntry == 0 )
		{
			MsgDev( D_NOTE, "library %s: has no entry point\n", name );
			goto error;
		}

		// notify library about attaching to process
		successfull = (*DllEntry)((HINSTANCE)code, DLL_PROCESS_ATTACH, 0 );
		if( !successfull )
		{
			MsgDev( D_ERROR, "can't attach library %s\n", name );
			goto error;
		}
		result->initialized = 1;
	}
	Mem_Free( data ); // release memory

	return (void *)result;
error:
	// cleanup
	Mem_Free( data );
	Com_FreeLibrary( result );
	return NULL;
}

FARPROC Com_GetProcAddress( void *module, const char *name )
{
	int	idx = -1;
	DWORD	i, *nameRef;
	WORD	*ordinal;
	PIMAGE_EXPORT_DIRECTORY exports;
	byte	*codeBase = ((PMEMORYMODULE)module)->codeBase;
	PIMAGE_DATA_DIRECTORY directory = GET_HEADER_DICTIONARY((MEMORYMODULE *)module, IMAGE_DIRECTORY_ENTRY_EXPORT );

	if( directory->Size == 0 )
	{
		// no export table found
		return NULL;
	}

	exports = (PIMAGE_EXPORT_DIRECTORY)CALCULATE_ADDRESS( codeBase, directory->VirtualAddress );
	if( exports->NumberOfNames == 0 || exports->NumberOfFunctions == 0 )
	{
		// DLL doesn't export anything
		return NULL;
	}

	// search function name in list of exported names
	nameRef = (DWORD *)CALCULATE_ADDRESS( codeBase, exports->AddressOfNames );
	ordinal = (WORD *)CALCULATE_ADDRESS( codeBase, exports->AddressOfNameOrdinals );
	for( i = 0; i < exports->NumberOfNames; i++, nameRef++, ordinal++ )
	{
		// GetProcAddress case insensative ?????
		if( !com.stricmp( name, (const char *)CALCULATE_ADDRESS( codeBase, *nameRef )))
		{
			idx = *ordinal;
			break;
		}
	}
	if( idx == -1 )
	{
		// exported symbol not found
		return NULL;
	}
	if((DWORD)idx > exports->NumberOfFunctions )
	{
		// name <-> ordinal number don't match
		return NULL;
	}

	// addressOfFunctions contains the RVAs to the "real" functions
	return (FARPROC)CALCULATE_ADDRESS( codeBase, *(DWORD *)CALCULATE_ADDRESS( codeBase, exports->AddressOfFunctions + (idx * 4)));
}

void Com_FreeLibrary( void *hInstance )
{
	MEMORYMODULE *module = (MEMORYMODULE *)hInstance;
	int	i;

	if( module != NULL )
	{
		if( module->initialized != 0 )
		{
			// notify library about detaching from process
			DllEntryProc DllEntry = (DllEntryProc)CALCULATE_ADDRESS( module->codeBase, module->headers->OptionalHeader.AddressOfEntryPoint );
			(*DllEntry)((HINSTANCE)module->codeBase, DLL_PROCESS_DETACH, 0 );
			module->initialized = 0;
		}

		if( module->modules != NULL )
		{
			// free previously opened libraries
			for( i = 0; i < module->numModules; i++ )
			{
				if( module->modules[i] != NULL )
				{
#ifdef EXPORT_FROM_MEMORY
					Com_FreeLibrary( module->modules[i] );
#else
					FreeLibrary( module->modules[i] );
#endif
				}
			}
			Mem_Free( module->modules ); // Z_Realloc end
		}

		FreeSections( module->headers, module );

		if( module->codeBase != NULL )
		{
			// release memory of library
			VirtualFree( module->codeBase, 0, MEM_RELEASE );
		}
		HeapFree( GetProcessHeap(), 0, module );
	}
}

void Com_BuildPathExt( const char *dllname, char *fullpath, size_t size )
{
	string	name;

	if( !dllname || !fullpath || size <= 0 ) return;

	// only libraries with extension .dll are valid
	com.strncpy( name, dllname, sizeof( string ));
	FS_FileBase( name, name );

	// game path (Xash3D/game/bin/)
	com.snprintf( fullpath, size, "bin/%s.dll", name );
	if( FS_FileExists( fullpath )) return; // found

	// absoulte path (Xash3D/bin/)
	com.snprintf( fullpath, size, "%s.dll", name );	
	if( FS_FileExists( fullpath )) return; // found

	fullpath[0] = 0;
}