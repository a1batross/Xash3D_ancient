//=======================================================================
//			Copyright XashXT Group 2008 ©
//			imagelib.h - engine image lib
//=======================================================================
#ifndef IMAGELIB_H
#define IMAGELIB_H

#include "launch.h"
#include "byteorder.h"

// skyorder_q2[6] = { 2, 3, 1, 0, 4, 5, }; // Quake, Half-Life skybox ordering
// skyorder_ms[6] = { 4, 5, 1, 0, 2, 3  }; // Microsoft DDS ordering (reverse)

typedef enum
{
	IL_HINT_NO	= 0,

	// dds cubemap hints ( Microsoft sides order )
	IL_HINT_POSX,
	IL_HINT_NEGX,
	IL_HINT_POSZ,
	IL_HINT_NEGZ,
	IL_HINT_POSY,
	IL_HINT_NEGY,

	// palette choosing
	IL_HINT_Q1,
	IL_HINT_Q2,
	IL_HINT_HL,
} image_hint_t;

typedef struct loadformat_s
{
	const char *formatstring;
	const char *ext;
	bool (*loadfunc)( const char *name, const byte *buffer, size_t filesize );
	image_hint_t hint;
} loadformat_t;

typedef struct saveformat_s
{
	const char *formatstring;
	const char *ext;
	bool (*savefunc)( const char *name, rgbdata_t *pix, int saveformat );
} saveformat_t;

typedef struct imglib_s
{
	const loadformat_t	*loadformats;
	const saveformat_t	*saveformats;

	// current 2d image state
	int		width;
	int		height;
	byte		num_layers;	// num layers in
	byte		num_mips;		// build mipmaps
	uint		type;		// main type switcher
	uint		flags;		// additional image flags
	byte		bits_count;	// bits per RGBA
	size_t		size;		// image rgba size (for bounds checking)
	uint		ptr;		// safe image pointer
	byte		*rgba;		// image pointer (see image_type for details)

	// current cubemap state
	int		source_width;	// locked cubemap dims (all wrong sides will be automatically resampled)
	int		source_height;
	uint		source_type;	// shared image type for all mipmaps or cubemap sides
	int		num_sides;	// how much sides is loaded 
	byte		*cubemap;		// cubemap pack

	// indexed images state
	uint		*d_currentpal;	// installed version of internal palette
	int		d_rendermode;	// palette rendermode
	byte		*palette;		// palette pointer

	// global parms
	int		curwidth;		// cubemap side, layer or mipmap width
	int		curheight;	// cubemap side, layer or mipmap height
	int		curdepth;		// current layer number
	int		cur_mips;		// number of mips
	int		bpp;		// PFDesc[type].bpp
	int		bpc;		// PFDesc[type].bpc
	int		bps;		// width * bpp * bpc
	int		SizeOfPlane;	// bps * height
	int		SizeOfData;	// SizeOfPlane * bps
	int		SizeOfFile;	// Image_DxtGetSize
	bool		(*decompress)( uint, int, int, uint, uint, uint, const void* );

	image_hint_t	hint;		// hint for some loaders
	byte		*tempbuffer;	// for convert operations
	int		cmd_flags;
} imglib_t;

/*
========================================================================

.PCX image format	(ZSoft Paintbrush)

========================================================================
*/
typedef struct
{
	char	manufacturer;
	char	version;
	char	encoding;
	char	bits_per_pixel;
	word	xmin,ymin,xmax,ymax;
	word	hres,vres;
	byte	palette[48];
	char	reserved;
	char	color_planes;
	word	bytes_per_line;
	word	palette_type;
	char	filler[58];
} pcx_t;

/*
========================================================================

.WAL image format	(Wally textures)

========================================================================
*/
typedef struct wal_s
{
	char	name[32];
	uint	width, height;
	uint	offsets[4];	// four mip maps stored
	char	animname[32];	// next frame in animation chain
	int	flags;
	int	contents;
	int	value;
} wal_t;

/*
========================================================================

.LMP image format	(Quake1 gfx lumps)

========================================================================
*/
typedef struct flat_s
{
	short	width;
	short	height;
	short	desc[2];		// probably not used
} flat_t;

/*
========================================================================

.BMP image format

========================================================================
*/
typedef struct
{
	char	id[2];		//bmfh.bfType
	dword	fileSize;		//bmfh.bfSize
	dword	reserved0;	//bmfh.bfReserved1 + bmfh.bfReserved2
	dword	bitmapDataOffset;	//bmfh.bfOffBits
	dword	bitmapHeaderSize;	//bmih.biSize
	dword	width;		//bmih.biWidth
	dword	height;		//bmih.biHeight
	word	planes;		//bmih.biPlanes
	word	bitsPerPixel;	//bmih.biBitCount
	dword	compression;	//bmih.biCompression
	dword	bitmapDataSize;	//bmih.biSizeImage
	dword	hRes;		//bmih.biXPelsPerMeter
	dword	vRes;		//bmih.biYPelsPerMeter
	dword	colors;		//bmih.biClrUsed
	dword	importantColors;	//bmih.biClrImportant
	byte	palette[256][4];	//RGBQUAD palette
} bmp_t;

/*
========================================================================

.TGA image format	(Truevision Targa)

========================================================================
*/
typedef struct tga_s
{
	byte	id_length;
	byte	colormap_type;
	byte	image_type;
	word	colormap_index;
	word	colormap_length;
	byte	colormap_size;
	word	x_origin;
	word	y_origin;
	word	width;
	word	height;
	byte	pixel_size;
	byte	attributes;
} tga_t;

/*
========================================================================

.JPG image format

========================================================================
*/
typedef struct huffman_table_s
{
	// Huffman coding tables
	byte	bits[16];
	byte	hval[256];
	byte	size[256];
	word	code[256];
} huffman_table_t;

typedef struct jpg_s
{
	// not a real header
	file_t	*file;		// file
	byte	*buffer;		// jpg buffer
	
	int	width;		// width image
	int	height;		// height image
	byte	*data;		// image
	int	data_precision;	// bit per component
	int	num_components;	// number component
	int	restart_interval;	// restart interval
	bool	progressive_mode;	// progressive format

	struct
	{
		int     id;	// identifier
		int     h;	// horizontal sampling factor
		int     v;	// vertical sampling factor
		int     t;	// quantization table selector
		int     td;	// DC table selector
		int     ta;	// AC table selector
	} component_info[3];	// RGB (alpha not supported)
    
	huffman_table_t hac[4];	// AC table
	huffman_table_t hdc[4];	// DC table

	int	qtable[4][64];	// quantization table

	struct
	{
		int     ss,se;	// progressive jpeg spectral selection
		int     ah,al;	// progressive jpeg successive approx
	} scan;

	int	dc[3];
	int	curbit;
	byte	curbyte;

} jpg_t;

/*
========================================================================

.DDS image format

========================================================================
*/
#define DDSHEADER	((' '<<24)+('S'<<16)+('D'<<8)+'D') // little-endian "DDS "

// various four-cc types
#define TYPE_DXT1	(('1'<<24)+('T'<<16)+('X'<<8)+'D') // little-endian "DXT1"
#define TYPE_DXT2	(('2'<<24)+('T'<<16)+('X'<<8)+'D') // little-endian "DXT2"
#define TYPE_DXT3	(('3'<<24)+('T'<<16)+('X'<<8)+'D') // little-endian "DXT3"
#define TYPE_DXT4	(('4'<<24)+('T'<<16)+('X'<<8)+'D') // little-endian "DXT4"
#define TYPE_DXT5	(('5'<<24)+('T'<<16)+('X'<<8)+'D') // little-endian "DXT5"

#define TYPE_ATI1	(('1'<<24)+('I'<<16)+('T'<<8)+'A') // little-endian "ATI1"
#define TYPE_ATI2	(('2'<<24)+('I'<<16)+('T'<<8)+'A') // little-endian "ATI2"

#define TYPE_RXGB	(('B'<<24)+('G'<<16)+('X'<<8)+'R') // little-endian "RXGB" doom3 normalmaps
#define TYPE_$	(('\0'<<24)+('\0'<<16)+('\0'<<8)+'$') // little-endian "$"
#define TYPE_t	(('\0'<<24)+('\0'<<16)+('\0'<<8)+'t') // little-endian "t"

#define DDS_CAPS				0x00000001L
#define DDS_HEIGHT				0x00000002L
#define DDS_WIDTH				0x00000004L

#define DDS_RGB				0x00000040L
#define DDS_PIXELFORMAT			0x00001000L
#define DDS_LUMINANCE			0x00020000L

#define DDS_ALPHAPIXELS			0x00000001L
#define DDS_ALPHA				0x00000002L
#define DDS_FOURCC				0x00000004L
#define DDS_PITCH				0x00000008L
#define DDS_COMPLEX				0x00000008L
#define DDS_CUBEMAP				0x00000200L
#define DDS_TEXTURE				0x00001000L
#define DDS_MIPMAPCOUNT			0x00020000L
#define DDS_LINEARSIZE			0x00080000L
#define DDS_VOLUME				0x00200000L
#define DDS_MIPMAP				0x00400000L
#define DDS_DEPTH				0x00800000L

#define DDS_CUBEMAP_POSITIVEX			0x00000400L
#define DDS_CUBEMAP_NEGATIVEX			0x00000800L
#define DDS_CUBEMAP_POSITIVEY			0x00001000L
#define DDS_CUBEMAP_NEGATIVEY			0x00002000L
#define DDS_CUBEMAP_POSITIVEZ			0x00004000L
#define DDS_CUBEMAP_NEGATIVEZ			0x00008000L

typedef struct dds_pf_s
{
	uint	dwSize;
	uint	dwFlags;
	uint	dwFourCC;
	uint	dwRGBBitCount;
	uint	dwRBitMask;
	uint	dwGBitMask;
	uint	dwBBitMask;
	uint	dwABitMask;
} dds_pixf_t;

//  DDCAPS2
typedef struct dds_caps_s
{
	uint	dwCaps1;
	uint	dwCaps2;
	uint	dwCaps3;
	uint	dwCaps4;
} dds_caps_t;

typedef struct dds_s
{
	uint		dwIdent;		// must matched with DDSHEADER
	uint		dwSize;
	uint		dwFlags;		// determines what fields are valid
	uint		dwHeight;
	uint		dwWidth;
	uint		dwLinearSize;	// Formless late-allocated optimized surface size
	uint		dwDepth;		// depth if a volume texture
	uint		dwMipMapCount;	// number of mip-map levels requested
	uint		dwAlphaBitDepth;	// depth of alpha buffer requested
	uint		dwReserved1[10];	// reserved for future expansions
	dds_pixf_t	dsPixelFormat;
	dds_caps_t	dsCaps;
	uint		dwTextureStage;
} dds_t;

/*
========================================================================

.VTF image format (Half-Life 2 dxt wrapper)

========================================================================
*/
#define VTFHEADER		(('\0'<<24)+('F'<<16)+('T'<<8)+'V')
#define VTF_VERSION		7
#define VTF_SUBVERSION1	1	// some old textures from beta
#define VTF_SUBVERSION2	2	// current subversion
	
typedef enum 
{ 
	VTF_UNKNOWN = -1,		// image absent
	VTF_RGBA8888 = 0,		// PF_RGBA_32
	VTF_ABGR8888,		// unsupported
	VTF_RGB888,		// PF_RGB_24 
	VTF_BGR888,		// unsupported
	VTF_RGB565,		// unsupported 
	VTF_I8,			// PF_LUMINANCE
	VTF_IA88,			// PF_LUMINANCE_ALPHA
	VTF_P8,			// PF_INDEXED_24 ??
	VTF_A8,			// unsupported
	VTF_RGB888_BLUESCREEN,	// PF_RGB_24 - just fill alpha pixels with (0 0 255) color
	VTF_BGR888_BLUESCREEN,	// unsupported
	VTF_ARGB8888,		// PF_ARGB_32
	VTF_BGRA8888,		// unsupported
	VTF_DXT1,			// PF_DXT1
	VTF_DXT3,			// PF_DXT3
	VTF_DXT5,			// PF_DXT5
	VTF_BGRX8888,		// unsupported
	VTF_BGR565,		// unsupported
	VTF_BGRX5551,		// unsupported
	VTF_BGRA4444,		// unsupported
	VTF_DXT1_ONEBITALPHA,	// PF_DXT1 - loader automatically detected alpha bits
	VTF_BGRA5551,		// unsupported
	VTF_UV88,			// unsupported
	VTF_UVWQ8888,		// unsupported
	VTF_RGBA16161616F,		// PF_ABGR_64_F ??
	VTF_RGBA16161616,		// unsupported
	VTF_UVLX8888,		// unsupported
	VTF_TOTALCOUNT
} vtf_format_t;

typedef enum
{
	// flags from the *.txt config file
	VF_POINTSAMPLE	= BIT(0),
	VF_TRILINEAR	= BIT(1),
	VF_CLAMPS		= BIT(2),
	VF_CLAMPT		= BIT(3),
	VF_ANISOTROPIC	= BIT(4),
	VF_HINT_DXT5	= BIT(5),
	VF_NOCOMPRESS	= BIT(6),
	VF_NORMAL		= BIT(7),
	VF_NOMIP		= BIT(8),
	VF_NOLOD		= BIT(9),
	VF_MINMIP		= BIT(10),
	VF_PROCEDURAL	= BIT(11),
	
	// these are automatically generated by vtex from the texture data.
	VF_ONEBITALPHA	= BIT(12),
	VF_EIGHTBITALPHA	= BIT(13),

	// newer flags from the *.txt config file
	VF_ENVMAP		= BIT(14),
	VF_RENDERTARGET	= BIT(15),
	VF_DEPTHRENDERTARGET= BIT(16),
	VF_NODEBUGOVERRIDE	= BIT(17),
	VF_SINGLECOPY	= BIT(18),
	VF_ONEALPHAMIP	= BIT(19),
	VF_PREMULTCOLOR	= BIT(20),
	VF_NORMALTODUDV	= BIT(21),
	VF_ALPHATESTMIPGEN	= BIT(22),
	VF_NODEPTHBUFFER	= BIT(23),
	VF_NICEFILTERED	= BIT(24),
	VF_LASTFLAG	= BIT(24),
} vtf_flags_t;

// dirty Valve tricks with compiler features...
#pragma pack( 1 )
__declspec(align( 16 )) typedef struct vtf_s
{
	int		ident;		// must matched with VTFHEADER
	int		ver_major;	// current version is 7
	int		ver_minor;	// 1 or 2
	int		hdr_size;		// ver 7.1 == 64 bytes, ver 7.2 == 80 bytes
	word		width;		// maxwidth 2048
	word		height;		// maxheight 2048
	uint		flags;		// misc image flags
	int		num_frames;	// sprite frames
	int		start_frame;	// ???
	vec4_t		reflectivity;	// vrad precomputed texcolor
	float		bumpScale;	// ovverided from vmt file
	vtf_format_t	imageFormat;	// see vtf_format_t for details
	byte		numMipLevels;	// never reached 255 mipLevels

	// probably it's used for WorldCraft texture explorer
	vtf_format_t	lowResImageFormat;	
	byte		lowResImageWidth;	// maxWidth = 256
	byte		lowResImageHeight;	// maxHeight = 256	

	// 20 additonal bytes for subversion 2 probably used for HDR settings
	int		unknown[4]; 
} vtf_t;
#pragma pack()

// imagelib definitions
#define IMAGE_MAXWIDTH	4096
#define IMAGE_MAXHEIGHT	4096
#define LUMP_MAXWIDTH	1024	// WorldCraft limits
#define LUMP_MAXHEIGHT	1024

#define TRANS_THRESHOLD	10	// in pixels
enum
{
	LUMP_NORMAL = 0,
	LUMP_TRANSPARENT,
	LUMP_DECAL,
	LUMP_QFONT
};

extern imglib_t image;
extern cvar_t *img_oldformats;
extern cvar_t *fs_wadsupport;
extern byte *fs_mempool;
extern const bpc_desc_t PFDesc[];

void Image_RoundDimensions( int *scaled_width, int *scaled_height );
byte *Image_ResampleInternal( const void *indata, int inwidth, int inheight, int outwidth, int outheight, int intype );
byte *Image_FlipInternal( const byte *in, int *srcwidth, int *srcheight, int type, int flags );
void Image_FreeImage( rgbdata_t *pack );
void Image_Save( const char *filename, rgbdata_t *pix );
size_t Image_DXTGetLinearSize( int type, int width, int height, int depth, int rgbcount );
rgbdata_t *Image_Load(const char *filename, const byte *buffer, size_t buffsize );
bool Image_Copy8bitRGBA( const byte *in, byte *out, int pixels );
bool FS_AddMipmapToPack( const byte *in, int width, int height );
void Image_SetPixelFormat( int width, int height, int depth );
void Image_ConvertPalTo24bit( rgbdata_t *pic );
void Image_DecompressDDS( const byte *buffer, uint target );
void Image_GetPaletteLMP( const byte *pal, int rendermode );
void Image_GetPalettePCX( const byte *pal );
void Image_CopyPalette24bit( void );
void Image_CopyPalette32bit( void );
uint Image_ShortToFloat( word y );
void Image_GetPaletteQ2( void );
void Image_GetPaletteQ1( void );
void Image_GetPaletteD1( void );	// doom 2 on TNT :)


//
// formats load
//
bool Image_LoadMIP( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadMDL( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadSPR( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadTGA( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadDDS( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadBMP( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadJPG( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadVTF( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadPNG( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadPCX( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadLMP( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadWAL( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadFLT( const char *name, const byte *buffer, size_t filesize );
bool Image_LoadPAL( const char *name, const byte *buffer, size_t filesize );

//
// formats save
//
bool Image_SaveTGA( const char *name, rgbdata_t *pix, int saveformat );
bool Image_SaveDDS( const char *name, rgbdata_t *pix, int saveformat );
bool Image_SaveBMP( const char *name, rgbdata_t *pix, int saveformat );
bool Image_SavePNG( const char *name, rgbdata_t *pix, int saveformat );
bool Image_SavePCX( const char *name, rgbdata_t *pix, int saveformat );

//
// img_utils.c
//
byte *Image_Copy( size_t size );
bool Image_ValidSize( const char *name );
bool Image_LumpValidSize( const char *name );

#endif//IMAGELIB_H