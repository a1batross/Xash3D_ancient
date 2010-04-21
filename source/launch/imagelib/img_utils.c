//=======================================================================
//			Copyright XashXT Group 2007 ©
//			img_utils.c - image common tools
//=======================================================================

#include "imagelib.h"
#include "mathlib.h"

cvar_t *gl_round_down;
cvar_t *png_compression;
cvar_t *jpg_quality;

#define LERPBYTE(i)		r = resamplerow1[i];out[i] = (byte)((((resamplerow2[i] - r) * lerp)>>16) + r)

uint d_8toD1table[256];
uint d_8toQ1table[256];
uint d_8toQ2table[256];
uint d_8to24table[256];
bool d1palette_init = false;
bool q1palette_init = false;
bool q2palette_init = false;

static byte palette_d1[768] = 
{
0,0,0,31,23,11,23,15,7,75,75,75,255,255,255,27,27,27,19,19,19,11,11,11,7,7,7,47,55,31,35,43,15,23,31,7,15,23,0,79,
59,43,71,51,35,63,43,27,255,183,183,247,171,171,243,163,163,235,151,151,231,143,143,223,135,135,219,123,123,211,115,
115,203,107,107,199,99,99,191,91,91,187,87,87,179,79,79,175,71,71,167,63,63,163,59,59,155,51,51,151,47,47,143,43,43,
139,35,35,131,31,31,127,27,27,119,23,23,115,19,19,107,15,15,103,11,11,95,7,7,91,7,7,83,7,7,79,0,0,71,0,0,67,0,0,255,
235,223,255,227,211,255,219,199,255,211,187,255,207,179,255,199,167,255,191,155,255,187,147,255,179,131,247,171,123,
239,163,115,231,155,107,223,147,99,215,139,91,207,131,83,203,127,79,191,123,75,179,115,71,171,111,67,163,107,63,155,
99,59,143,95,55,135,87,51,127,83,47,119,79,43,107,71,39,95,67,35,83,63,31,75,55,27,63,47,23,51,43,19,43,35,15,239,
239,239,231,231,231,223,223,223,219,219,219,211,211,211,203,203,203,199,199,199,191,191,191,183,183,183,179,179,179,
171,171,171,167,167,167,159,159,159,151,151,151,147,147,147,139,139,139,131,131,131,127,127,127,119,119,119,111,111,
111,107,107,107,99,99,99,91,91,91,87,87,87,79,79,79,71,71,71,67,67,67,59,59,59,55,55,55,47,47,47,39,39,39,35,35,35,
119,255,111,111,239,103,103,223,95,95,207,87,91,191,79,83,175,71,75,159,63,67,147,55,63,131,47,55,115,43,47,99,35,39,
83,27,31,67,23,23,51,15,19,35,11,11,23,7,191,167,143,183,159,135,175,151,127,167,143,119,159,135,111,155,127,107,147,
123,99,139,115,91,131,107,87,123,99,79,119,95,75,111,87,67,103,83,63,95,75,55,87,67,51,83,63,47,159,131,99,143,119,83,
131,107,75,119,95,63,103,83,51,91,71,43,79,59,35,67,51,27,123,127,99,111,115,87,103,107,79,91,99,71,83,87,59,71,79,51,
63,71,43,55,63,39,255,255,115,235,219,87,215,187,67,195,155,47,175,123,31,155,91,19,135,67,7,115,43,0,255,255,255,255,
219,219,255,187,187,255,155,155,255,123,123,255,95,95,255,63,63,255,31,31,255,0,0,239,0,0,227,0,0,215,0,0,203,0,0,191,
0,0,179,0,0,167,0,0,155,0,0,139,0,0,127,0,0,115,0,0,103,0,0,91,0,0,79,0,0,67,0,0,231,231,255,199,199,255,171,171,255,
143,143,255,115,115,255,83,83,255,55,55,255,27,27,255,0,0,255,0,0,227,0,0,203,0,0,179,0,0,155,0,0,131,0,0,107,0,0,83,
255,255,255,255,235,219,255,215,187,255,199,155,255,179,123,255,163,91,255,143,59,255,127,27,243,115,23,235,111,15,
223,103,15,215,95,11,203,87,7,195,79,0,183,71,0,175,67,0,255,255,255,255,255,215,255,255,179,255,255,143,255,255,107,
255,255,71,255,255,35,255,255,0,167,63,0,159,55,0,147,47,0,135,35,0,79,59,39,67,47,27,55,35,19,47,27,11,0,0,83,0,0,71,
0,0,59,0,0,47,0,0,35,0,0,23,0,0,11,0,255,255,255,159,67,255,231,75,255,123,255,255,0,255,207,0,207,159,0,155,111,0,107,
167,107,107
};

static byte palette_q1[768] =
{
0,0,0,15,15,15,31,31,31,47,47,47,63,63,63,75,75,75,91,91,91,107,107,107,123,123,123,139,139,139,155,155,155,171,
171,171,187,187,187,203,203,203,219,219,219,235,235,235,15,11,7,23,15,11,31,23,11,39,27,15,47,35,19,55,43,23,63,
47,23,75,55,27,83,59,27,91,67,31,99,75,31,107,83,31,115,87,31,123,95,35,131,103,35,143,111,35,11,11,15,19,19,27,
27,27,39,39,39,51,47,47,63,55,55,75,63,63,87,71,71,103,79,79,115,91,91,127,99,99,139,107,107,151,115,115,163,123,
123,175,131,131,187,139,139,203,0,0,0,7,7,0,11,11,0,19,19,0,27,27,0,35,35,0,43,43,7,47,47,7,55,55,7,63,63,7,71,71,
7,75,75,11,83,83,11,91,91,11,99,99,11,107,107,15,7,0,0,15,0,0,23,0,0,31,0,0,39,0,0,47,0,0,55,0,0,63,0,0,71,0,0,79,
0,0,87,0,0,95,0,0,103,0,0,111,0,0,119,0,0,127,0,0,19,19,0,27,27,0,35,35,0,47,43,0,55,47,0,67,55,0,75,59,7,87,67,7,
95,71,7,107,75,11,119,83,15,131,87,19,139,91,19,151,95,27,163,99,31,175,103,35,35,19,7,47,23,11,59,31,15,75,35,19,
87,43,23,99,47,31,115,55,35,127,59,43,143,67,51,159,79,51,175,99,47,191,119,47,207,143,43,223,171,39,239,203,31,255,
243,27,11,7,0,27,19,0,43,35,15,55,43,19,71,51,27,83,55,35,99,63,43,111,71,51,127,83,63,139,95,71,155,107,83,167,123,
95,183,135,107,195,147,123,211,163,139,227,179,151,171,139,163,159,127,151,147,115,135,139,103,123,127,91,111,119,
83,99,107,75,87,95,63,75,87,55,67,75,47,55,67,39,47,55,31,35,43,23,27,35,19,19,23,11,11,15,7,7,187,115,159,175,107,
143,163,95,131,151,87,119,139,79,107,127,75,95,115,67,83,107,59,75,95,51,63,83,43,55,71,35,43,59,31,35,47,23,27,35,
19,19,23,11,11,15,7,7,219,195,187,203,179,167,191,163,155,175,151,139,163,135,123,151,123,111,135,111,95,123,99,83,
107,87,71,95,75,59,83,63,51,67,51,39,55,43,31,39,31,23,27,19,15,15,11,7,111,131,123,103,123,111,95,115,103,87,107,
95,79,99,87,71,91,79,63,83,71,55,75,63,47,67,55,43,59,47,35,51,39,31,43,31,23,35,23,15,27,19,11,19,11,7,11,7,255,
243,27,239,223,23,219,203,19,203,183,15,187,167,15,171,151,11,155,131,7,139,115,7,123,99,7,107,83,0,91,71,0,75,55,
0,59,43,0,43,31,0,27,15,0,11,7,0,0,0,255,11,11,239,19,19,223,27,27,207,35,35,191,43,43,175,47,47,159,47,47,143,47,
47,127,47,47,111,47,47,95,43,43,79,35,35,63,27,27,47,19,19,31,11,11,15,43,0,0,59,0,0,75,7,0,95,7,0,111,15,0,127,23,
7,147,31,7,163,39,11,183,51,15,195,75,27,207,99,43,219,127,59,227,151,79,231,171,95,239,191,119,247,211,139,167,123,
59,183,155,55,199,195,55,231,227,87,127,191,255,171,231,255,215,255,255,103,0,0,139,0,0,179,0,0,215,0,0,255,0,0,255,
243,147,255,247,199,255,255,255,159,91,83
};

static byte palette_q2[768] =
{
0,0,0,15,15,15,31,31,31,47,47,47,63,63,63,75,75,75,91,91,91,107,107,107,123,123,123,139,139,139,155,155,155,171,171,
171,187,187,187,203,203,203,219,219,219,235,235,235,99,75,35,91,67,31,83,63,31,79,59,27,71,55,27,63,47,23,59,43,23,
51,39,19,47,35,19,43,31,19,39,27,15,35,23,15,27,19,11,23,15,11,19,15,7,15,11,7,95,95,111,91,91,103,91,83,95,87,79,91,
83,75,83,79,71,75,71,63,67,63,59,59,59,55,55,51,47,47,47,43,43,39,39,39,35,35,35,27,27,27,23,23,23,19,19,19,143,119,
83,123,99,67,115,91,59,103,79,47,207,151,75,167,123,59,139,103,47,111,83,39,235,159,39,203,139,35,175,119,31,147,99,
27,119,79,23,91,59,15,63,39,11,35,23,7,167,59,43,159,47,35,151,43,27,139,39,19,127,31,15,115,23,11,103,23,7,87,19,0,
75,15,0,67,15,0,59,15,0,51,11,0,43,11,0,35,11,0,27,7,0,19,7,0,123,95,75,115,87,67,107,83,63,103,79,59,95,71,55,87,67,
51,83,63,47,75,55,43,67,51,39,63,47,35,55,39,27,47,35,23,39,27,19,31,23,15,23,15,11,15,11,7,111,59,23,95,55,23,83,47,
23,67,43,23,55,35,19,39,27,15,27,19,11,15,11,7,179,91,79,191,123,111,203,155,147,215,187,183,203,215,223,179,199,211,
159,183,195,135,167,183,115,151,167,91,135,155,71,119,139,47,103,127,23,83,111,19,75,103,15,67,91,11,63,83,7,55,75,7,
47,63,7,39,51,0,31,43,0,23,31,0,15,19,0,7,11,0,0,0,139,87,87,131,79,79,123,71,71,115,67,67,107,59,59,99,51,51,91,47,
47,87,43,43,75,35,35,63,31,31,51,27,27,43,19,19,31,15,15,19,11,11,11,7,7,0,0,0,151,159,123,143,151,115,135,139,107,
127,131,99,119,123,95,115,115,87,107,107,79,99,99,71,91,91,67,79,79,59,67,67,51,55,55,43,47,47,35,35,35,27,23,23,19,
15,15,11,159,75,63,147,67,55,139,59,47,127,55,39,119,47,35,107,43,27,99,35,23,87,31,19,79,27,15,67,23,11,55,19,11,43,
15,7,31,11,7,23,7,0,11,0,0,0,0,0,119,123,207,111,115,195,103,107,183,99,99,167,91,91,155,83,87,143,75,79,127,71,71,
115,63,63,103,55,55,87,47,47,75,39,39,63,35,31,47,27,23,35,19,15,23,11,7,7,155,171,123,143,159,111,135,151,99,123,
139,87,115,131,75,103,119,67,95,111,59,87,103,51,75,91,39,63,79,27,55,67,19,47,59,11,35,47,7,27,35,0,19,23,0,11,15,
0,0,255,0,35,231,15,63,211,27,83,187,39,95,167,47,95,143,51,95,123,51,255,255,255,255,255,211,255,255,167,255,255,
127,255,255,83,255,255,39,255,235,31,255,215,23,255,191,15,255,171,7,255,147,0,239,127,0,227,107,0,211,87,0,199,71,
0,183,59,0,171,43,0,155,31,0,143,23,0,127,15,0,115,7,0,95,0,0,71,0,0,47,0,0,27,0,0,239,0,0,55,55,255,255,0,0,0,0,255,
43,43,35,27,27,23,19,19,15,235,151,127,195,115,83,159,87,51,123,63,27,235,211,199,199,171,155,167,139,119,135,107,87,
159,91,83	
};

/*
=============================================================================

	XASH3D LOAD IMAGE FORMATS

=============================================================================
*/
// stub
static const loadpixformat_t load_null[] =
{
{ NULL, NULL, NULL, IL_HINT_NO }
};

// version0 - using only dds images
static const loadpixformat_t load_stalker[] =
{
{ "%s%s.%s", "dds", Image_LoadDDS, IL_HINT_NO },
{ NULL, NULL, NULL, IL_HINT_NO }
};

// version1 - using only Doom1 images
static const loadpixformat_t load_doom1[] =
{
{ "%s%s.%s", "flt", Image_LoadFLT, IL_HINT_NO },	// flat textures, sprites
{ NULL, NULL, NULL, IL_HINT_NO }
};

// version2 - using only Quake1 images
// wad files not requires path
static const loadpixformat_t load_quake1[] =
{
{ "%s%s.%s", "mip", Image_LoadMIP, IL_HINT_Q1 },	// q1 textures from wad or buffer
{ "%s%s.%s", "mdl", Image_LoadMDL, IL_HINT_Q1 },	// q1 alias model skins
{ "%s%s.%s", "spr", Image_LoadSPR, IL_HINT_Q1 },	// q1 sprite frames
{ "%s%s.%s", "lmp", Image_LoadLMP, IL_HINT_Q1 },	// q1 menu images
{ NULL, NULL, NULL, IL_HINT_NO }
};

// version3 - using only Quake2 images
static const loadpixformat_t load_quake2[] =
{
{ "textures/%s%s.%s", "wal", Image_LoadWAL, IL_HINT_Q2 },	// map textures
{ "%s%s.%s", "wal", Image_LoadWAL, IL_HINT_Q2 },	// map textures
{ "%s%s.%s", "pcx", Image_LoadPCX, IL_HINT_NO },	// pics, skins, skies (q2 palette does wrong result for some images)
{ "%s%s.%s", "tga", Image_LoadTGA, IL_HINT_NO },	// skies (indexed TGA's? never see in Q2)
{ NULL, NULL, NULL, IL_HINT_NO }
};

// version4 - using only Quake3 images
// g-cont: probably q3 used only explicit paths
static const loadpixformat_t load_quake3[] =
{
{ "%s%s.%s", "jpg", Image_LoadJPG, IL_HINT_NO },	// textures or gfx
{ "%s%s.%s", "tga", Image_LoadTGA, IL_HINT_NO },	// textures or gfx
{ NULL, NULL, NULL, IL_HINT_NO }
};

// version5 - using only Quake4(Doom3) images
// g-cont: probably q4 used only explicit paths
static const loadpixformat_t load_quake4[] =
{
{ "%s%s.%s", "dds", Image_LoadDDS, IL_HINT_NO },	// textures or gfx
{ "%s%s.%s", "tga", Image_LoadTGA, IL_HINT_NO },	// textures or gfx
{ NULL, NULL, NULL, IL_HINT_NO }
};

// version6 - using only Half-Life images
// wad files not requires path
static const loadpixformat_t load_hl1[] =
{
{ "%s%s.%s", "tga", Image_LoadTGA, IL_HINT_NO },	// hl vgui menus
{ "%s%s.%s", "bmp", Image_LoadBMP, IL_HINT_NO },	// hl skyboxes
{ "%s%s.%s", "mip", Image_LoadMIP, IL_HINT_HL },	// hl textures from wad or buffer
{ "%s%s.%s", "mdl", Image_LoadMDL, IL_HINT_HL },	// hl studio model skins
{ "%s%s.%s", "spr", Image_LoadSPR, IL_HINT_HL },	// hl sprite frames
{ "%s%s.%s", "lmp", Image_LoadLMP, IL_HINT_HL },	// hl menu images (cached.wad etc)
{ "%s%s.%s", "fnt", Image_LoadFNT, IL_HINT_HL },	// hl menu images (cached.wad etc)
{ "%s%s.%s", "pal", Image_LoadPAL, IL_HINT_NO },	// install studio palette
{ NULL, NULL, NULL, IL_HINT_NO }
};

// version7 - using only Half-Life 2 images
static const loadpixformat_t load_hl2[] =
{
{ "%s%s.%s", "vtf", Image_LoadVTF, IL_HINT_NO },	// hl2 dds wrapper
{ "%s%s.%s", "tga", Image_LoadTGA, IL_HINT_NO },	// for Vgui, saveshots etc
{ NULL, NULL, NULL, IL_HINT_NO }
};

// version8 - studiomdl profile
static const loadpixformat_t load_studiomdl[] =
{
{ "%s%s.%s", "bmp", Image_LoadBMP, IL_HINT_NO },	// classic studio textures
{ "%s%s.%s", "pcx", Image_LoadPCX, IL_HINT_NO },	// q2 skins or somewhat
{ "%s%s.%s", "wal", Image_LoadWAL, IL_HINT_Q2 },	// q2 textures as skins ???
{ NULL, NULL, NULL, IL_HINT_NO }
};

// version9 - spritegen profile
static const loadpixformat_t load_spritegen[] =
{
{ "%s%s.%s", "bmp", Image_LoadBMP, IL_HINT_NO },	// classic sprite frames
{ "%s%s.%s", "pcx", Image_LoadPCX, IL_HINT_NO },	// q2 sprite frames
{ NULL, NULL, NULL, IL_HINT_NO }
};

// version10 - xwad profile
static const loadpixformat_t load_wadlib[] =
{
{ "%s%s.%s", "bmp", Image_LoadBMP, IL_HINT_NO },	// there all wad package types
{ "%s%s.%s", "pcx", Image_LoadPCX, IL_HINT_NO }, 
{ "%s%s.%s", "wal", Image_LoadWAL, IL_HINT_NO },
{ "%s%s.%s", "mip", Image_LoadMIP, IL_HINT_NO },	// from another wads
{ "%s%s.%s", "lmp", Image_LoadLMP, IL_HINT_NO },
{ "%s%s.%s", "flt", Image_LoadFLT, IL_HINT_NO },
{ NULL, NULL, NULL, IL_HINT_NO }
};

// version11 - Xash3D default image profile
static const loadpixformat_t load_xash[] =
{
{ "%s%s.%s", "dds", Image_LoadDDS, IL_HINT_NO },	// cubemaps, depthmaps, 2d textures
{ "%s%s.%s", "png", Image_LoadPNG, IL_HINT_NO },	// levelshot save as .png
{ "%s%s.%s", "tga", Image_LoadTGA, IL_HINT_NO },	// screenshots, etc
{ "%s%s.%s", "jpg", Image_LoadJPG, IL_HINT_NO },	// 2d textures
{ "%s%s.%s", "mip", Image_LoadMIP, IL_HINT_HL },	// hl textures (WorldCraft support)
{ "%s%s.%s", "lmp", Image_LoadLMP, IL_HINT_HL },	// hl pictures
{ "%s%s.%s", "fnt", Image_LoadFNT, IL_HINT_HL },	// qfonts (console, titles etc)
{ "%s%s.%s", "mdl", Image_LoadMDL, IL_HINT_NO },	// hl or q1 model skins
{ "%s%s.%s", "spr", Image_LoadSPR, IL_HINT_HL },	// hl sprite frames
{ "%s%s.%s", "pal", Image_LoadPAL, IL_HINT_NO },	// install palettes
{ NULL, NULL, NULL, IL_HINT_NO }
};

static const loadpixformat_t load_ximage[] =
{
{ "%s%s.%s", "dds", Image_LoadDDS, IL_HINT_NO },	// cubemaps, depthmaps, 2d textures
{ "%s%s.%s", "png", Image_LoadPNG, IL_HINT_NO },	// levelshot save as .png
{ "%s%s.%s", "tga", Image_LoadTGA, IL_HINT_NO },	// screenshots, etc
{ "%s%s.%s", "jpg", Image_LoadJPG, IL_HINT_NO },	// 2d textures
{ "%s%s.%s", "bmp", Image_LoadBMP, IL_HINT_NO },	// there all wad package types
{ "%s%s.%s", "pcx", Image_LoadPCX, IL_HINT_NO }, 
{ NULL, NULL, NULL, IL_HINT_NO }
};

/*
=============================================================================

	XASH3D SAVE IMAGE FORMATS

=============================================================================
*/
// stub
static const savepixformat_t save_null[] =
{
{ NULL, NULL, NULL }
};

// version0 - extragen save formats
static const savepixformat_t save_extragen[] =
{
{ "%s%s.%s", "tga", Image_SaveTGA },		// tga screenshots
{ "%s%s.%s", "jpg", Image_SaveJPG },		// jpg screenshots
{ "%s%s.%s", "png", Image_SavePNG },		// png levelshots
{ "%s%s.%s", "dds", Image_SaveDDS },		// vtf use this
{ "%s%s.%s", "pcx", Image_SavePCX },		// just in case
{ "%s%s.%s", "bmp", Image_SaveBMP },		// all 8-bit images
{ NULL, NULL, NULL }
};

// Xash3D normal instance
static const savepixformat_t save_xash[] =
{
{ "%s%s.%s", "tga", Image_SaveTGA },		// tga screenshots
{ "%s%s.%s", "jpg", Image_SaveJPG },		// tga levelshots or screenshots
{ "%s%s.%s", "png", Image_SavePNG },		// png levelshots
{ "%s%s.%s", "dds", Image_SaveDDS },		// dds envshots
{ NULL, NULL, NULL }
};

static const savepixformat_t save_ximage[] =
{
{ "%s%s.%s", "tga", Image_SaveTGA },
{ "%s%s.%s", "jpg", Image_SaveJPG },
{ "%s%s.%s", "png", Image_SavePNG },
{ "%s%s.%s", "dds", Image_SaveDDS },
{ "%s%s.%s", "pcx", Image_SavePCX },
{ "%s%s.%s", "bmp", Image_SaveBMP },
{ NULL, NULL, NULL }
};

void Image_Init( void )
{
	// init pools
	Sys.imagepool = Mem_AllocPool( "ImageLib Pool" );
	gl_round_down = Cvar_Get( "gl_round_down", "0", CVAR_SYSTEMINFO, "down size non-power of two textures" );
	png_compression = Cvar_Get( "png_compression", "9", CVAR_SYSTEMINFO, "pnglib compression level" );
	jpg_quality = Cvar_Get( "jpg_quality", "7", CVAR_SYSTEMINFO, "jpglib quality level" );
	Cvar_SetValue( "png_compression", bound( 0, png_compression->integer, 9 ));
	Cvar_SetValue( "jpg_quality", bound( 0, jpg_quality->integer, 10 ));

	image.baseformats = load_xash;

	// install image formats (can be re-install later by Image_Setup)
	switch( Sys.app_name )
	{
	case HOST_SPRITE:
		image.loadformats = load_spritegen;
		break;
	case HOST_STUDIO:
		image.loadformats = load_studiomdl;
		break;
	case HOST_WADLIB:
		image.loadformats = load_wadlib;
		break;
	case HOST_XIMAGE:
		image.loadformats = load_ximage;
		image.saveformats = save_ximage;
		break;
	case HOST_NORMAL:
	case HOST_BSPLIB:
		Image_Setup( "default", 0 ); // re-initialized later		
		image.saveformats = save_xash;
		break;
	case HOST_RIPPER:
		image.loadformats = load_null;
		image.saveformats = save_extragen;
		break;
	default:	// all other instances not using imagelib or will be reinstalling later
		image.loadformats = load_null;
		image.saveformats = save_null;
		break;
	}
	image.tempbuffer = NULL;
}

void Image_SetPaths( const char *envpath, const char *scrshot_ext, const char *levshot_ext, const char *savshot_ext )
{
	if( envpath ) com.strncpy( SI.envpath, envpath, sizeof( SI.envpath ));
	if( scrshot_ext ) com.strncpy( SI.scrshot_ext, scrshot_ext, sizeof( SI.scrshot_ext ));
	if( levshot_ext ) com.strncpy( SI.levshot_ext, levshot_ext, sizeof( SI.levshot_ext ));
	if( savshot_ext ) com.strncpy( SI.savshot_ext, savshot_ext, sizeof( SI.savshot_ext ));

	// use saveshot extension for envshots too
	if( savshot_ext ) com.strncpy( SI.envshot_ext, savshot_ext, sizeof( SI.envshot_ext ));
}

void Image_Setup( const char *formats, const uint flags )
{
	if( flags != -1 ) image.cmd_flags = flags;
	if( formats == NULL ) return;	// used for change flags only

	MsgDev( D_NOTE, "Image_Init( %s )\n", formats );

	// reinstall loadformats by magic keyword :)
	if( !com.stricmp( formats, "Xash3D" ) || !com.stricmp( formats, "Xash" ))
	{
		image.loadformats = load_xash;
		Image_SetPaths( "env", "jpg", "png", "png" );
	}
	else if( !com.stricmp( formats, "stalker" ) || !com.stricmp( formats, "S.T.A.L.K.E.R" ))
	{
		image.loadformats = load_stalker;
		Image_SetPaths( "textures/sky", "jpg", "dds", "dds" );
	}
	else if( !com.stricmp( formats, "Doom1" ) || !com.stricmp( formats, "Doom2" ))
	{
		image.loadformats = load_doom1;
		Image_SetPaths( "gfx/env", "tga", "tga", "tga" );
	}
	else if( !com.stricmp( formats, "Quake1" ))
	{
		image.loadformats = load_quake1; 
		Image_SetPaths( "gfx/env", "tga", "tga", "tga" );
	}
	else if( !com.stricmp( formats, "Quake2" ))
	{
		image.loadformats = load_quake2;
		Image_SetPaths( "env", "tga", "tga", "tga" );
	}
	else if( !com.stricmp( formats, "Quake3" ))
	{
		image.loadformats = load_quake3;
		Image_SetPaths( "env", "jpg", "jpg", "jpg" );
	}
	else if( !com.stricmp( formats, "Quake4" ) || !com.stricmp( formats, "Doom3" ))
	{
		image.loadformats = load_quake4;
		Image_SetPaths( "env", "dds", "dds", "dds" );
	}
	else if( !com.stricmp( formats, "hl1" ) || !com.stricmp( formats, "Half-Life" ))
	{
		image.loadformats = load_hl1;
		Image_SetPaths( "gfx/env", "jpg", "tga", "tga" );
	}
	else if( !com.stricmp( formats, "hl2" ) || !com.stricmp( formats, "Half-Life 2" ))
	{
		image.loadformats = load_hl2;
		Image_SetPaths( "materials/skybox", "jpg", "tga", "tga" );
	}
	else
	{
		image.loadformats = load_xash; // unrecognized version, use default
		Image_SetPaths( "env", "jpg", "png", "png" );
	}

	if( Sys.app_name == HOST_RIPPER )
		image.baseformats = image.loadformats;
}

void Image_Shutdown( void )
{
	Mem_Check(); // check for leaks
	Mem_FreePool( &Sys.imagepool );
}

byte *Image_Copy( size_t size )
{
	byte	*out;

	out = Mem_Alloc( Sys.imagepool, size );
	Mem_Copy( out, image.tempbuffer, size );
	return out; 
}

void Image_RoundDimensions( int *width, int *height )
{
	// find nearest power of two, rounding down if desired
	*width = NearestPOW( *width, gl_round_down->integer );
	*height = NearestPOW( *height, gl_round_down->integer );
}

bool Image_ValidSize( const char *name )
{
	if( image.width > IMAGE_MAXWIDTH || image.height > IMAGE_MAXHEIGHT || image.width <= 0 || image.height <= 0 )
		return false;
	return true;
}

bool Image_LumpValidSize( const char *name )
{
	if( image.width > LUMP_MAXWIDTH || image.height > LUMP_MAXHEIGHT || image.width <= 0 || image.height <= 0 )
	{
		if(!com_stristr( name, "#internal" )) // internal errors are silent
			MsgDev(D_WARN, "Image_LumpValidSize: (%s) dims out of range[%dx%d]\n", name, image.width,image.height );
		return false;
	}
	return true;
}

void Image_SetPalette( const byte *pal, uint *d_table )
{
	int	i;
	byte	rgba[4];
	
	// setup palette
	switch( image.d_rendermode )
	{
	case LUMP_DECAL:
		for( i = 0; i < 256; i++ )
		{
			rgba[3] = pal[765];
			rgba[2] = pal[766];
			rgba[1] = pal[767];
			rgba[0] = i;
			d_table[i] = BuffBigLong( rgba );
		}
		break;
	case LUMP_TRANSPARENT:
		for( i = 0; i < 256; i++ )
		{
			rgba[3] = pal[i*3+0];
			rgba[2] = pal[i*3+1];
			rgba[1] = pal[i*3+2];
			rgba[0] = pal[i] == 255 ? pal[i] : 0xFF;
			d_table[i] = BuffBigLong( rgba );
		}
		break;
	case LUMP_QFONT:
		for (i = 1; i < 256; i++)
		{
			rgba[3] = pal[i*3+0];
			rgba[2] = pal[i*3+1];
			rgba[1] = pal[i*3+2];
			rgba[0] = 0xFF;
			d_table[i] = BuffBigLong( rgba );
		}
		break;
	case LUMP_NORMAL:
		for (i = 0; i < 256; i++)
		{
			rgba[3] = pal[i*3+0];
			rgba[2] = pal[i*3+1];
			rgba[1] = pal[i*3+2];
			rgba[0] = 0xFF;
			d_table[i] = BuffBigLong( rgba );
		}
		break;
	case LUMP_EXTENDED:
		for (i = 0; i < 256; i++)
		{
			rgba[3] = pal[i*4+0];
			rgba[2] = pal[i*4+1];
			rgba[1] = pal[i*4+2];
			rgba[0] = pal[i*4+3];
			d_table[i] = BuffBigLong( rgba );
		}
		break;	
	}
}

void Image_GetPaletteD1( void )
{
	image.d_rendermode = LUMP_NORMAL;

	if( !d1palette_init )
	{
		byte	temp[4];

		Image_SetPalette( palette_d1, d_8toD1table );
		d_8toD1table[247] = 0; // Image_LoadFLT will be convert transparency from 247 into 255 color
		d1palette_init = true;

		// also swap palette colors: from 247 to 255
		Mem_Copy( temp, &d_8toD1table[255], 4 );
		Mem_Copy( &d_8toD1table[255], &d_8toD1table[247], 4 );		
		Mem_Copy( &d_8toD1table[247], temp, 4 );
	}
	image.d_currentpal = d_8toD1table;
}

void Image_GetPaletteQ1( void )
{
	image.d_rendermode = LUMP_NORMAL;

	if(!q1palette_init)
	{
		Image_SetPalette( palette_q1, d_8toQ1table );
		d_8toQ1table[255] = 0; // 255 is transparent
		q1palette_init = true;
	}
	image.d_currentpal = d_8toQ1table;
}

void Image_GetPaletteQ2( void )
{
	image.d_rendermode = LUMP_NORMAL;

	if(!q2palette_init)
	{
		Image_SetPalette( palette_q2, d_8toQ2table );
		d_8toQ2table[255] &= LittleLong(0xffffff);
		q2palette_init = true;
	}
	image.d_currentpal = d_8toQ2table;
}

void Image_GetPalettePCX( const byte *pal )
{
	image.d_rendermode = LUMP_NORMAL;

	if( pal )
	{
		Image_SetPalette( pal, d_8to24table );
		d_8to24table[255] &= LittleLong(0xffffff);
		image.d_currentpal = d_8to24table;
	}
	else Image_GetPaletteQ2();          
}

void Image_GetPaletteBMP( const byte *pal )
{
	image.d_rendermode = LUMP_EXTENDED;

	if( pal )
	{
		Image_SetPalette( pal, d_8to24table );
		image.d_currentpal = d_8to24table;
	}
}

void Image_GetPaletteLMP( const byte *pal, int rendermode )
{
	image.d_rendermode = rendermode;

	if( pal )
	{
		Image_SetPalette( pal, d_8to24table );
		d_8to24table[255] &= LittleLong(0xffffff);
		image.d_currentpal = d_8to24table;
	}
	else if( rendermode == LUMP_QFONT )
	{
		// quake1 base palette and font palette have some diferences
		Image_SetPalette( palette_q1, d_8to24table );
		d_8to24table[0] = 0;
		image.d_currentpal = d_8to24table;
	}
	else Image_GetPaletteQ1(); // default quake palette          
}

void Image_ConvertPalTo24bit( rgbdata_t *pic )
{
	byte	*pal32, *pal24;
	byte	*converted;
	int	i;

	if( !pic || !pic->palette )
	{
		MsgDev(D_ERROR,"Image_ConvertPalTo24bit: no palette found\n");
		return;
	}
	if( pic->type == PF_INDEXED_24 )
	{
		MsgDev( D_ERROR, "Image_ConvertPalTo24bit: palette already converted\n" );
		return;
	}

	pal24 = converted = Mem_Alloc( Sys.imagepool, 768 );
	pal32 = pic->palette;

	for( i = 0; i < 256; i++, pal24 += 3, pal32 += 4 )
	{
		pal24[0] = pal32[0];
		pal24[1] = pal32[1];
		pal24[2] = pal32[2];
	}
	Mem_Free( pic->palette );
	pic->palette = converted;
	pic->type = PF_INDEXED_24;
}

void Image_CopyPalette32bit( void )
{
	if( image.palette ) return; // already created ?
	image.palette = Mem_Alloc( Sys.imagepool, 1024 );
	Mem_Copy( image.palette, image.d_currentpal, 1024 );
}

void Image_CopyParms( rgbdata_t *src )
{
	Image_Reset();

	image.width = src->width;
	image.height = src->height;
	image.depth = src->depth;
	image.num_mips = src->numMips;
	image.type = src->type;
	image.flags = src->flags;
	image.bits_count = src->bitsCount;
	image.size = src->size;
	image.rgba = src->buffer;
	image.palette = src->palette;	// may be NULL
}

/*
============
Image_Copy8bitRGBA

NOTE: must call Image_GetPaletteXXX before used
============
*/
bool Image_Copy8bitRGBA( const byte *in, byte *out, int pixels )
{
	int	*iout = (int *)out;
	color32	*col;

	if( !image.d_currentpal )
	{
		MsgDev( D_ERROR, "Image_Copy8bitRGBA: no palette set\n" );
		return false;
	}
	if( !in )
	{
		MsgDev( D_ERROR, "Image_Copy8bitRGBA: no input image\n" );
		return false;
	}

	while( pixels >= 8 )
	{
		iout[0] = image.d_currentpal[in[0]];
		iout[1] = image.d_currentpal[in[1]];
		iout[2] = image.d_currentpal[in[2]];
		iout[3] = image.d_currentpal[in[3]];
		iout[4] = image.d_currentpal[in[4]];
		iout[5] = image.d_currentpal[in[5]];
		iout[6] = image.d_currentpal[in[6]];
		iout[7] = image.d_currentpal[in[7]];

		col = (color32 *)iout;
		if( col->r != col->g || col->g != col->b )
			image.flags |= IMAGE_HAS_COLOR;

		in += 8;
		iout += 8;
		pixels -= 8;
	}
	if( pixels & 4 )
	{
		iout[0] = image.d_currentpal[in[0]];
		iout[1] = image.d_currentpal[in[1]];
		iout[2] = image.d_currentpal[in[2]];
		iout[3] = image.d_currentpal[in[3]];
		in += 4;
		iout += 4;
	}
	if( pixels & 2 )
	{
		iout[0] = image.d_currentpal[in[0]];
		iout[1] = image.d_currentpal[in[1]];
		in += 2;
		iout += 2;
	}
	if( pixels & 1 ) // last byte
		iout[0] = image.d_currentpal[in[0]];

	image.type = PF_RGBA_32;	// update image type;
	return true;
}

/*
====================
Image_ShortToFloat
====================
*/
uint Image_ShortToFloat( word y )
{
	int s = (y >> 15) & 0x00000001;
	int e = (y >> 10) & 0x0000001f;
	int m =  y & 0x000003ff;

	// float: 1 sign bit, 8 exponent bits, 23 mantissa bits
	// half: 1 sign bit, 5 exponent bits, 10 mantissa bits

	if( e == 0 )
	{
		if( m == 0 ) return s << 31; // Plus or minus zero
		else // denormalized number -- renormalize it
		{
			while(!(m & 0x00000400))
			{
				m <<= 1;
				e -=  1;
			}
			e += 1;
			m &= ~0x00000400;
		}
	}
	else if( e == 31 )
	{
		if( m == 0 ) return (s << 31) | 0x7f800000; // positive or negative infinity
		else return (s << 31) | 0x7f800000 | (m << 13); // NAN - preserve sign and significand bits
	}

	// normalized number
	e = e + (127 - 15);
	m = m << 13;
	return (s << 31) | (e << 23) | m; // assemble s, e and m.
}

static void Image_Resample32LerpLine (const byte *in, byte *out, int inwidth, int outwidth)
{
	int	j, xi, oldx = 0, f, fstep, endx, lerp;

	fstep = (int)(inwidth * 65536.0f/outwidth);
	endx = (inwidth-1);

	for( j = 0, f = 0; j < outwidth; j++, f += fstep )
	{
		xi = f>>16;
		if( xi != oldx )
		{
			in += (xi - oldx) * 4;
			oldx = xi;
		}
		if( xi < endx )
		{
			lerp = f & 0xFFFF;
			*out++ = (byte)((((in[4] - in[0]) * lerp)>>16) + in[0]);
			*out++ = (byte)((((in[5] - in[1]) * lerp)>>16) + in[1]);
			*out++ = (byte)((((in[6] - in[2]) * lerp)>>16) + in[2]);
			*out++ = (byte)((((in[7] - in[3]) * lerp)>>16) + in[3]);
		}
		else // last pixel of the line has no pixel to lerp to
		{
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
			*out++ = in[3];
		}
	}
}

static void Image_Resample24LerpLine( const byte *in, byte *out, int inwidth, int outwidth )
{
	int	j, xi, oldx = 0, f, fstep, endx, lerp;

	fstep = (int)(inwidth * 65536.0f/outwidth);
	endx = (inwidth-1);

	for( j = 0, f = 0; j < outwidth; j++, f += fstep )
	{
		xi = f>>16;
		if( xi != oldx )
		{
			in += (xi - oldx) * 3;
			oldx = xi;
		}
		if( xi < endx )
		{
			lerp = f & 0xFFFF;
			*out++ = (byte)((((in[3] - in[0]) * lerp)>>16) + in[0]);
			*out++ = (byte)((((in[4] - in[1]) * lerp)>>16) + in[1]);
			*out++ = (byte)((((in[5] - in[2]) * lerp)>>16) + in[2]);
		}
		else // last pixel of the line has no pixel to lerp to
		{
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
		}
	}
}

void Image_Resample32Lerp(const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight)
{
	int	i, j, r, yi, oldy = 0, f, fstep, lerp, endy = (inheight - 1);
	int	inwidth4 = inwidth * 4;
	int	outwidth4 = outwidth * 4;
	const byte *inrow;
	byte	*out;
	byte	*resamplerow1;
	byte	*resamplerow2;

	out = (byte *)outdata;
	fstep = (int)(inheight * 65536.0f/outheight);

	resamplerow1 = (byte *)Mem_Alloc( Sys.imagepool, outwidth * 4 * 2);
	resamplerow2 = resamplerow1 + outwidth * 4;

	inrow = (const byte *)indata;

	Image_Resample32LerpLine( inrow, resamplerow1, inwidth, outwidth );
	Image_Resample32LerpLine( inrow + inwidth4, resamplerow2, inwidth, outwidth );

	for( i = 0, f = 0; i < outheight; i++, f += fstep )
	{
		yi = f>>16;

		if( yi < endy )
		{
			lerp = f & 0xFFFF;
			if( yi != oldy )
			{
				inrow = (byte *)indata + inwidth4 * yi;
				if (yi == oldy+1) Mem_Copy( resamplerow1, resamplerow2, outwidth4 );
				else Image_Resample32LerpLine( inrow, resamplerow1, inwidth, outwidth );
				Image_Resample32LerpLine( inrow + inwidth4, resamplerow2, inwidth, outwidth );
				oldy = yi;
			}
			j = outwidth - 4;
			while( j >= 0 )
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				LERPBYTE( 6);
				LERPBYTE( 7);
				LERPBYTE( 8);
				LERPBYTE( 9);
				LERPBYTE(10);
				LERPBYTE(11);
				LERPBYTE(12);
				LERPBYTE(13);
				LERPBYTE(14);
				LERPBYTE(15);
				out += 16;
				resamplerow1 += 16;
				resamplerow2 += 16;
				j -= 4;
			}
			if( j & 2 )
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				LERPBYTE( 6);
				LERPBYTE( 7);
				out += 8;
				resamplerow1 += 8;
				resamplerow2 += 8;
			}
			if( j & 1 )
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				out += 4;
				resamplerow1 += 4;
				resamplerow2 += 4;
			}
			resamplerow1 -= outwidth4;
			resamplerow2 -= outwidth4;
		}
		else
		{
			if( yi != oldy )
			{
				inrow = (byte *)indata + inwidth4*yi;
				if( yi == oldy + 1 ) Mem_Copy( resamplerow1, resamplerow2, outwidth4 );
				else Image_Resample32LerpLine( inrow, resamplerow1, inwidth, outwidth);
				oldy = yi;
			}
			Mem_Copy( out, resamplerow1, outwidth4 );
		}
	}

	Mem_Free( resamplerow1 );
	resamplerow1 = NULL;
	resamplerow2 = NULL;
}

void Image_Resample32Nolerp( const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight )
{
	int	i, j;
	uint	frac, fracstep;
	int	*inrow, *out = (int *)outdata; // relies on int being 4 bytes

	fracstep = inwidth * 0x10000/outwidth;

	for( i = 0; i < outheight; i++)
	{
		inrow = (int *)indata + inwidth * (i * inheight/outheight);
		frac = fracstep>>1;
		j = outwidth - 4;

		while( j >= 0 )
		{
			out[0] = inrow[frac >> 16];frac += fracstep;
			out[1] = inrow[frac >> 16];frac += fracstep;
			out[2] = inrow[frac >> 16];frac += fracstep;
			out[3] = inrow[frac >> 16];frac += fracstep;
			out += 4;
			j -= 4;
		}
		if( j & 2 )
		{
			out[0] = inrow[frac >> 16];frac += fracstep;
			out[1] = inrow[frac >> 16];frac += fracstep;
			out += 2;
		}
		if( j & 1 )
		{
			out[0] = inrow[frac >> 16];frac += fracstep;
			out += 1;
		}
	}
}

void Image_Resample24Lerp( const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight )
{
	int	i, j, r, yi, oldy, f, fstep, lerp, endy = (inheight - 1);
	int	inwidth3 = inwidth * 3;
	int	outwidth3 = outwidth * 3;
	const byte *inrow;
	byte	*out = (byte *)outdata;
	byte	*resamplerow1;
	byte	*resamplerow2;
	
	fstep = (int)(inheight * 65536.0f / outheight);

	resamplerow1 = (byte *)Mem_Alloc(Sys.imagepool, outwidth * 3 * 2);
	resamplerow2 = resamplerow1 + outwidth*3;

	inrow = (const byte *)indata;
	oldy = 0;
	Image_Resample24LerpLine( inrow, resamplerow1, inwidth, outwidth );
	Image_Resample24LerpLine( inrow + inwidth3, resamplerow2, inwidth, outwidth );

	for( i = 0, f = 0; i < outheight; i++, f += fstep )
	{
		yi = f>>16;

		if( yi < endy )
		{
			lerp = f & 0xFFFF;
			if( yi != oldy )
			{
				inrow = (byte *)indata + inwidth3 * yi;
				if( yi == oldy + 1) Mem_Copy( resamplerow1, resamplerow2, outwidth3 );
				else Image_Resample24LerpLine( inrow, resamplerow1, inwidth, outwidth );
				Image_Resample24LerpLine( inrow + inwidth3, resamplerow2, inwidth, outwidth );
				oldy = yi;
			}
			j = outwidth - 4;
			while( j >= 0 )
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				LERPBYTE( 6);
				LERPBYTE( 7);
				LERPBYTE( 8);
				LERPBYTE( 9);
				LERPBYTE(10);
				LERPBYTE(11);
				out += 12;
				resamplerow1 += 12;
				resamplerow2 += 12;
				j -= 4;
			}
			if( j & 2 )
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				out += 6;
				resamplerow1 += 6;
				resamplerow2 += 6;
			}
			if( j & 1 )
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				out += 3;
				resamplerow1 += 3;
				resamplerow2 += 3;
			}
			resamplerow1 -= outwidth3;
			resamplerow2 -= outwidth3;
		}
		else
		{
			if( yi != oldy )
			{
				inrow = (byte *)indata + inwidth3*yi;
				if( yi == oldy + 1) Mem_Copy( resamplerow1, resamplerow2, outwidth3 );
				else Image_Resample24LerpLine( inrow, resamplerow1, inwidth, outwidth );
				oldy = yi;
			}
			Mem_Copy( out, resamplerow1, outwidth3 );
		}
	}
	Mem_Free( resamplerow1 );
	resamplerow1 = NULL;
	resamplerow2 = NULL;
}

void Image_Resample24Nolerp( const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight )
{
	int	i, j, f, inwidth3 = inwidth * 3;
	uint	frac, fracstep;
	byte	*inrow, *out = (byte *)outdata;

	fracstep = inwidth * 0x10000/outwidth;

	for( i = 0; i < outheight; i++)
	{
		inrow = (byte *)indata + inwidth3 * (i * inheight/outheight);
		frac = fracstep>>1;
		j = outwidth - 4;

		while( j >= 0 )
		{
			f = (frac >> 16)*3;
			*out++ = inrow[f+0];
			*out++ = inrow[f+1];
			*out++ = inrow[f+2];
			frac += fracstep;
			f = (frac >> 16)*3;
			*out++ = inrow[f+0];
			*out++ = inrow[f+1];
			*out++ = inrow[f+2];
			frac += fracstep;
			f = (frac >> 16)*3;
			*out++ = inrow[f+0];
			*out++ = inrow[f+1];
			*out++ = inrow[f+2];
			frac += fracstep;
			f = (frac >> 16)*3;
			*out++ = inrow[f+0];
			*out++ = inrow[f+1];
			*out++ = inrow[f+2];
			frac += fracstep;
			j -= 4;
		}
		if( j & 2 )
		{
			f = (frac >> 16)*3;
			*out++ = inrow[f+0];
			*out++ = inrow[f+1];
			*out++ = inrow[f+2];
			frac += fracstep;
			f = (frac >> 16)*3;
			*out++ = inrow[f+0];
			*out++ = inrow[f+1];
			*out++ = inrow[f+2];
			frac += fracstep;
			out += 2;
		}
		if( j & 1 )
		{
			f = (frac >> 16)*3;
			*out++ = inrow[f+0];
			*out++ = inrow[f+1];
			*out++ = inrow[f+2];
			frac += fracstep;
			out += 1;
		}
	}
}

void Image_Resample8Nolerp( const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight )
{
	int	i, j;
	byte	*in, *inrow;
	uint	frac, fracstep;
	byte	*out = (byte *)outdata;

	in = (byte *)indata;
	fracstep = inwidth * 0x10000 / outwidth;
	for( i = 0; i < outheight; i++, out += outwidth )
	{
		inrow = in + inwidth*(i*inheight/outheight);
		frac = fracstep>>1;
		for( j = 0; j < outwidth; j++ )
		{
			out[j] = inrow[frac>>16];
			frac += fracstep;
		}
	}
}

/*
================
Image_Resample
================
*/
byte *Image_ResampleInternal( const void *indata, int inwidth, int inheight, int outwidth, int outheight, int type )
{
	bool	quality = (image.cmd_flags & IL_USE_LERPING);

	// nothing to resample ?
	if( inwidth == outwidth && inheight == outheight )
		return (byte *)indata;

	// alloc new buffer
	switch( type )
	{
	case PF_INDEXED_24:
	case PF_INDEXED_32:
		image.tempbuffer = (byte *)Mem_Realloc( Sys.imagepool, image.tempbuffer, outwidth * outheight );
		Image_Resample8Nolerp( indata, inwidth, inheight, image.tempbuffer, outwidth, outheight );
		break;		
	case PF_RGB_24:
		image.tempbuffer = (byte *)Mem_Realloc( Sys.imagepool, image.tempbuffer, outwidth * outheight * 3 );
		if( quality ) Image_Resample24Lerp( indata, inwidth, inheight, image.tempbuffer, outwidth, outheight );
		else Image_Resample24Nolerp( indata, inwidth, inheight, image.tempbuffer, outwidth, outheight );
		break;
	case PF_RGBA_32:
		image.tempbuffer = (byte *)Mem_Realloc( Sys.imagepool, image.tempbuffer, outwidth * outheight * 4 );
		if( quality ) Image_Resample32Lerp( indata, inwidth, inheight, image.tempbuffer, outwidth, outheight );
		else Image_Resample32Nolerp( indata, inwidth, inheight, image.tempbuffer, outwidth, outheight );
		break;
	default:
		MsgDev( D_WARN, "Image_Resample: unsupported format %s\n", PFDesc[type].name );
		return (byte *)indata;	
	}
	return image.tempbuffer;
}

/*
================
Image_Flood
================
*/
byte *Image_FloodInternal( const byte *indata, int inwidth, int inheight, int outwidth, int outheight, int type )
{
	bool	quality = (image.cmd_flags & IL_USE_LERPING);
	int	samples = PFDesc[type].bpp;
	int	newsize, x, y, i;
	byte	*in, *out;

	// nothing to reflood ?
	if( inwidth == outwidth && inheight == outheight )
		return (byte *)indata;

	// alloc new buffer
	switch( type )
	{
	case PF_INDEXED_24:
	case PF_INDEXED_32:
	case PF_RGB_24:
	case PF_BGR_24:
	case PF_BGRA_32:
	case PF_RGBA_32:
		in = ( byte *)indata;
		newsize = outwidth * outheight * samples;
		out = image.tempbuffer = (byte *)Mem_Realloc( Sys.imagepool, image.tempbuffer, newsize );
		break;
	default:
		MsgDev( D_WARN, "Image_Flood: unsupported format %s\n", PFDesc[type].name );
		return (byte *)indata;	
	}

	if( samples == 1 ) Mem_Set( out, 0xFF, newsize );	// last palette color
	else  Mem_Set( out, 0x00808080, newsize );	// gray (alpha leaved 0x00)

	for( y = 0; y < outheight; y++ )
		for( x = 0; y < inheight && x < outwidth; x++ )
			for( i = 0; i < samples; i++ )
				if( x < inwidth ) *out++ = *in++;
				else *out++;
	return image.tempbuffer;
}

/*
================
Image_Flip
================
*/
byte *Image_FlipInternal( const byte *in, word *srcwidth, word *srcheight, int type, int flags )
{
	int	i, x, y;
	word	width = *srcwidth;
	word	height = *srcheight; 
	int	samples = PFDesc[type].bpp;
	bool	flip_x = ( flags & IMAGE_FLIP_X ) ? true : false;
	bool	flip_y = ( flags & IMAGE_FLIP_Y ) ? true : false;
	bool	flip_i = ( flags & IMAGE_ROT_90 ) ? true : false;
	int	row_inc = ( flip_y ? -samples : samples ) * width;
	int	col_inc = ( flip_x ? -samples : samples );
	int	row_ofs = ( flip_y ? ( height - 1 ) * width * samples : 0 );
	int	col_ofs = ( flip_x ? ( width - 1 ) * samples : 0 );
	const byte *p, *line;
	byte	*out;

	// nothing to process
	if(!(flags & (IMAGE_FLIP_X|IMAGE_FLIP_Y|IMAGE_ROT_90)))
		return (byte *)in;

	switch( type )
	{
	case PF_INDEXED_24:
	case PF_INDEXED_32:
	case PF_RGB_24:
	case PF_RGBA_32:
		image.tempbuffer = Mem_Realloc( Sys.imagepool, image.tempbuffer, width * height * samples );
		break;
	default:
		// we can flip DXT without expanding to RGBA ? hmmm...
		MsgDev( D_WARN, "Image_Flip: unsupported format %s\n", PFDesc[type].name );
		return (byte *)in;	
	}
	out = image.tempbuffer;

	if( flip_i )
	{
		for( x = 0, line = in + col_ofs; x < width; x++, line += col_inc )
			for( y = 0, p = line + row_ofs; y < height; y++, p += row_inc, out += samples )
				for( i = 0; i < samples; i++ )
					out[i] = p[i];
	}
	else
	{
		for( y = 0, line = in + row_ofs; y < height; y++, line += row_inc )
			for( x = 0, p = line + col_ofs; x < width; x++, p += col_inc, out += samples )
				for( i = 0; i < samples; i++ )
					out[i] = p[i];
	}

	// update dims
	if( flags & IMAGE_ROT_90 )
	{
		*srcwidth = height;
		*srcheight = width;		
	}
	else
	{
		*srcwidth = width;
		*srcheight = height;	
	}
	return image.tempbuffer;
}

byte *Image_CreateLumaInternal( const byte *fin, int width, int height, int type, int flags )
{
	byte	*out;
	int	i;

	if(!( flags & IMAGE_HAS_LUMA ))
	{
		MsgDev( D_WARN, "Image_MakeLuma: image doesn't has luma pixels\n" );
		return (byte *)fin;	  
	}

	switch( type )
	{
	case PF_INDEXED_24:
	case PF_INDEXED_32:
		out = image.tempbuffer = Mem_Realloc( Sys.imagepool, image.tempbuffer, width * height );
		for( i = 0; i < width * height; i++ )
		{
			if( flags & IMAGE_HAS_LUMA_Q1 )
				*out++ = fin[i] > 224 ? fin[i] : 0;
			else if( flags & IMAGE_HAS_LUMA_Q2 )
				*out++ = (fin[i] > 208 && fin[i] < 240) ? fin[i] : 0;
                    }
		break;
	default:
		// another formats does ugly result :(
		MsgDev( D_WARN, "Image_MakeLuma: unsupported format %s\n", PFDesc[type].name );
		return (byte *)fin;	
	}
	return image.tempbuffer;
}

rgbdata_t *Image_DecompressInternal( rgbdata_t *pic )
{
	int	i, offset, numsides = 1;
	uint	target = 1;
	byte	*buf;

	// quick case to reject unneeded conversions
	switch( pic->type )
	{
	case PF_UV_32:
	case PF_RGBA_GN:
	case PF_RGBA_32:
	case PF_ABGR_128F:
		pic->type = PF_RGBA_32;
		return pic; // just change type
	}

	Image_CopyParms( pic );

	if( image.flags & IMAGE_CUBEMAP ) numsides = 6;
	Image_SetPixelFormat( image.width, image.height, image.depth ); // setup
	image.size = image.ptr = 0;
	if( image.cmd_flags & IL_IGNORE_MIPS ) image.cur_mips = 1;
	else image.cur_mips = image.num_mips;
	image.num_mips = 0; // clear mipcount
	buf = image.rgba;

	for( i = 0, offset = 0; i < numsides; i++ )
	{
		Image_SetPixelFormat( image.curwidth, image.curheight, image.curdepth );
		offset = image.SizeOfFile; // move pointer

		Image_DecompressDDS( buf, target + i );
		buf += offset;
	}
	// now we can change type to RGBA
	if( image.filter != CB_HINT_NO )
		image.flags &= ~IMAGE_CUBEMAP; // side extracted
	image.type = PF_RGBA_32;

	FS_FreeImage( pic );	// free original

	return ImagePack();
}

bool Image_Process( rgbdata_t **pix, int width, int height, uint flags )
{
	rgbdata_t	*pic = *pix;
	bool	result = true;
	byte	*out;
				
	// check for buffers
	if( !pic || !pic->buffer )
	{
		MsgDev( D_WARN, "Image_Process: NULL image\n" );
		return false;
	}

	if( flags & IMAGE_MAKE_LUMA )
	{
		out = Image_CreateLumaInternal( pic->buffer, pic->width, pic->height, pic->type, pic->flags );
		if( pic->buffer != out ) Mem_Copy( pic->buffer, image.tempbuffer, pic->size );
	}

	// update format to RGBA if any
	if( flags & IMAGE_FORCE_RGBA ) pic = Image_DecompressInternal( pic );

	// NOTE: flip and resample algorythms can't difference palette size
	if( flags & IMAGE_PALTO24 ) Image_ConvertPalTo24bit( pic );
	out = Image_FlipInternal( pic->buffer, &pic->width, &pic->height, pic->type, flags );
	if( pic->buffer != out ) Mem_Copy( pic->buffer, image.tempbuffer, pic->size );

	if(( flags & IMAGE_RESAMPLE && width > 0 && height > 0 ) || flags & IMAGE_ROUND || flags & IMAGE_ROUNDFILLER )
	{
		int	w, h;

		if( flags & IMAGE_ROUND || flags & IMAGE_ROUNDFILLER )
		{
			w = pic->width;
			h = pic->height;

			// round to nearest pow
			// NOTE: images with dims less than 8x8 may causing problems
			Image_RoundDimensions( &w, &h );
			w = bound( 8, w, IMAGE_MAXWIDTH );	// 8 - 4096
			h = bound( 8, h, IMAGE_MAXHEIGHT);	// 8 - 4096
		}
		else
		{
			// custom size (user choise without limitations)
			w = bound( 1, width, IMAGE_MAXWIDTH );	// 1 - 4096
			h = bound( 1, height, IMAGE_MAXHEIGHT);	// 1 - 4096
		}
		if( flags & IMAGE_ROUNDFILLER )
	         		out = Image_FloodInternal( pic->buffer, pic->width, pic->height, w, h, pic->type );
		else out = Image_ResampleInternal((uint *)pic->buffer, pic->width, pic->height, w, h, pic->type );

		if( out != pic->buffer ) // resampled or filled
		{
			MsgDev( D_NOTE, "Image_Resample: from[%d x %d] to [%d x %d]\n", pic->width, pic->height, w, h );
			pic->width = w, pic->height = h;
			pic->size = w * h * PFDesc[pic->type].bpp;
			Mem_Free( pic->buffer );		// free original image buffer
			pic->buffer = Image_Copy( pic->size );	// unzone buffer (don't touch image.tempbuffer)
		}
		else result = false; // not a resampled or filled
	}
	*pix = pic;

	return result;
}