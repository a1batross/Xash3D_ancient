//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        com_library.h - custom dlls loader
//=======================================================================
#ifndef COM_LIBRARY
#define COM_LIBRARY

#define DOS_SIGNATURE		0x5A4D		// MZ
#define NT_SIGNATURE		0x00004550	// PE00
#define NUMBEROF_DIRECTORY_ENTRIES	16

typedef struct
{	
	// dos .exe header
	word	e_magic;		// magic number
	word	e_cblp;		// bytes on last page of file
	word	e_cp;		// pages in file
	word	e_crlc;		// relocations
	word	e_cparhdr;	// size of header in paragraphs
	word	e_minalloc;	// minimum extra paragraphs needed
	word	e_maxalloc;	// maximum extra paragraphs needed
	word	e_ss;		// initial (relative) SS value
	word	e_sp;		// initial SP value
	word	e_csum;		// checksum
	word	e_ip;		// initial IP value
	word	e_cs;		// initial (relative) CS value
	word	e_lfarlc;		// file address of relocation table
	word	e_ovno;		// overlay number
	word	e_res[4];		// reserved words
	word	e_oemid;		// OEM identifier (for e_oeminfo)
	word	e_oeminfo;	// OEM information; e_oemid specific
	word	e_res2[10];	// reserved words
	long	e_lfanew;		// file address of new exe header
} DOS_HEADER;

typedef struct
{	
	// win .exe header
	word	Machine;
	word	NumberOfSections;
	dword	TimeDateStamp;
	dword	PointerToSymbolTable;
	dword	NumberOfSymbols;
	word	SizeOfOptionalHeader;
	word	Characteristics;
} PE_HEADER;

typedef struct
{
	byte	Name[8];	// dos name length

	union
	{
		dword	PhysicalAddress;
		dword	VirtualSize;
	} Misc;

	dword	VirtualAddress;
	dword	SizeOfRawData;
	dword	PointerToRawData;
	dword	PointerToRelocations;
	dword	PointerToLinenumbers;
	word	NumberOfRelocations;
	word	NumberOfLinenumbers;
	dword	Characteristics;
} SECTION_HEADER;

typedef struct
{
	dword	VirtualAddress;
	dword	Size;
} DATA_DIRECTORY;

typedef struct
{
	word	Magic;
	byte	MajorLinkerVersion;
	byte	MinorLinkerVersion;
	dword	SizeOfCode;
	dword	SizeOfInitializedData;
	dword	SizeOfUninitializedData;
	dword	AddressOfEntryPoint;
	dword	BaseOfCode;
	dword	BaseOfData;
	dword	ImageBase;
	dword	SectionAlignment;
	dword	FileAlignment;
	word	MajorOperatingSystemVersion;
	word	MinorOperatingSystemVersion;
	word	MajorImageVersion;
	word	MinorImageVersion;
	word	MajorSubsystemVersion;
	word	MinorSubsystemVersion;
	dword	Win32VersionValue;
	dword	SizeOfImage;
	dword	SizeOfHeaders;
	dword	CheckSum;
	word	Subsystem;
	word	DllCharacteristics;
	dword	SizeOfStackReserve;
	dword	SizeOfStackCommit;
	dword	SizeOfHeapReserve;
	dword	SizeOfHeapCommit;
	dword	LoaderFlags;
	dword	NumberOfRvaAndSizes;

	DATA_DIRECTORY	DataDirectory[NUMBEROF_DIRECTORY_ENTRIES];
} OPTIONAL_HEADER;

typedef struct
{
	dword	Characteristics;
	dword	TimeDateStamp;
	word	MajorVersion;
	word	MinorVersion;
	dword	Name;
	dword	Base;
	dword	NumberOfFunctions;
	dword	NumberOfNames;
	dword	AddressOfFunctions;		// RVA from base of image
	dword	AddressOfNames;		// RVA from base of image
	dword	AddressOfNameOrdinals;	// RVA from base of image
} EXPORT_DIRECTORY;

void *Com_LoadLibrary( const char *name );
FARPROC Com_GetProcAddress( void *hInstance, const char *name );
void Com_FreeLibrary( void *hInstance );
void Com_BuildPathExt( const char *dllname, char *fullpath, size_t size );
#define Com_BuildPath( a, b )	Com_BuildPathExt( a, b, sizeof( b ))

#endif//COM_LIBRARY