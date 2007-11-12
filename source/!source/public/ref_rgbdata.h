//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       ref_rgbdata.h - imagelib interface
//=======================================================================
#ifndef REF_RGBDATA_H
#define REF_RGBDATA_H

// opengl mask
#define GL_COLOR_INDEX	0x1900
#define GL_STENCIL_INDEX	0x1901
#define GL_DEPTH_COMPONENT	0x1902
#define GL_RED		0x1903
#define GL_GREEN		0x1904
#define GL_BLUE		0x1905
#define GL_ALPHA		0x1906
#define GL_RGB		0x1907
#define GL_RGBA		0x1908
#define GL_LUMINANCE	0x1909
#define GL_LUMINANCE_ALPHA	0x190A
#define GL_BGR		0x80E0
#define GL_BGRA		0x80E1

// gl data type
#define GL_BYTE		0x1400
#define GL_UNSIGNED_BYTE	0x1401
#define GL_SHORT		0x1402
#define GL_UNSIGNED_SHORT	0x1403
#define GL_INT		0x1404
#define GL_UNSIGNED_INT	0x1405
#define GL_FLOAT		0x1406
#define GL_2_BYTES		0x1407
#define GL_3_BYTES		0x1408
#define GL_4_BYTES		0x1409
#define GL_DOUBLE		0x140A

// description flags
#define IMAGE_CUBEMAP	0x00000001
#define IMAGE_HAS_ALPHA	0x00000002
#define IMAGE_PREMULT	0x00000004	// indices who need in additional premultiply
#define IMAGE_GEN_MIPS	0x00000008	// must generate mips
#define IMAGE_CUBEMAP_FLIP	0x00000010	// it's a cubemap with flipped sides( dds pack )

enum comp_format
{
	PF_UNKNOWN = 0,
	PF_INDEXED_24,	// studio model skins
	PF_INDEXED_32,	// sprite 32-bit palette
	PF_RGBA_32,	// already prepared ".bmp", ".tga" or ".jpg" image 
	PF_ARGB_32,	// uncompressed dds image
	PF_RGB_24,	// uncompressed dds or another 24-bit image 
	PF_RGB_24_FLIP,	// flip image for screenshots
	PF_DXT1,		// nvidia DXT1 format
	PF_DXT2,		// nvidia DXT2 format
	PF_DXT3,		// nvidia DXT3 format
	PF_DXT4,		// nvidia DXT4 format
	PF_DXT5,		// nvidia DXT5 format
	PF_ATI1N,		// ati 1N texture
	PF_ATI2N,		// ati 2N texture
	PF_LUMINANCE,	// b&w dds image
	PF_LUMINANCE_16,	// b&w hi-res image
	PF_LUMINANCE_ALPHA, // b&w dds image with alpha channel
	PF_RXGB,		// doom3 normal maps
	PF_ABGR_64,	// uint image
	PF_RGBA_GN,	// internal generated texture
	PF_TOTALCOUNT,	// must be last
};

// format info table
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
{PF_UNKNOWN,	"raw",	GL_RGBA,		GL_UNSIGNED_BYTE, 0,  0,  0 },
{PF_INDEXED_24,	"pal 24",	GL_RGBA,		GL_UNSIGNED_BYTE, 3,  1,  0 },// expand data to RGBA buffer
{PF_INDEXED_32,	"pal 32",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1,  0 },
{PF_RGBA_32,	"RGBA 32",GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, -4 },
{PF_ARGB_32,	"ARGB 32",GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, -4 },
{PF_RGB_24,	"RGB 24",	GL_RGBA,		GL_UNSIGNED_BYTE, 3,  1, -3 },
{PF_RGB_24_FLIP,	"RGB 24",	GL_RGBA,		GL_UNSIGNED_BYTE, 3,  1, -3 },
{PF_DXT1,		"DXT1",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1,  8 },
{PF_DXT2,		"DXT2",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, 16 },
{PF_DXT3,		"DXT3",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, 16 },
{PF_DXT4,		"DXT4",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, 16 },
{PF_DXT5,		"DXT5",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, 16 },
{PF_ATI1N,	"ATI1N",	GL_RGBA,		GL_UNSIGNED_BYTE, 1,  1,  8 },
{PF_ATI2N,	"3DC",	GL_RGBA,		GL_UNSIGNED_BYTE, 3,  1, 16 },
{PF_LUMINANCE,	"LUM 8",	GL_LUMINANCE,	GL_UNSIGNED_BYTE, 1,  1, -1 },
{PF_LUMINANCE_16,	"LUM 16", GL_LUMINANCE,	GL_UNSIGNED_BYTE, 2,  2, -2 },
{PF_LUMINANCE_ALPHA,"LUM A",	GL_LUMINANCE_ALPHA,	GL_UNSIGNED_BYTE, 2,  1, -2 },
{PF_RXGB,		"RXGB",	GL_RGBA,		GL_UNSIGNED_BYTE, 3,  1, 16 },
{PF_ABGR_64,	"ABGR 64",GL_BGRA,		GL_UNSIGNED_BYTE, 4,  2, -8 },
{PF_RGBA_GN,	"system",	GL_RGBA,		GL_UNSIGNED_BYTE, 4,  1, -4 },
};

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
	uint	size;		// for bounds checking
};

#endif//REF_RGBDATA_H