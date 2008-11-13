//=======================================================================
//			Copyright XashXT Group 2007 ©
//		conv_shader.c - analyze and write texture shader
//=======================================================================

#include "ripper.h"
#include "mathlib.h"

// q2 wal contents
#define CONTENTS_SOLID	0x00000001	// an eye is never valid in a solid
#define CONTENTS_WINDOW	0x00000002	// translucent, but not watery
#define CONTENTS_AUX	0x00000004
#define CONTENTS_LAVA	0x00000008
#define CONTENTS_SLIME	0x00000010
#define CONTENTS_WATER	0x00000020
#define CONTENTS_MIST	0x00000040

// remaining contents are non-visible, and don't eat brushes
#define CONTENTS_AREAPORTAL	0x00008000
#define CONTENTS_PLAYERCLIP	0x00010000
#define CONTENTS_MONSTERCLIP	0x00020000

// currents can be added to any other contents, and may be mixed
#define CONTENTS_CURRENT_0	0x00040000
#define CONTENTS_CURRENT_90	0x00080000
#define CONTENTS_CURRENT_180	0x00100000
#define CONTENTS_CURRENT_270	0x00200000
#define CONTENTS_CURRENT_UP	0x00400000
#define CONTENTS_CURRENT_DOWN	0x00800000

#define CONTENTS_ORIGIN	0x01000000	// removed before BSP'ing an entity

#define CONTENTS_MONSTER	0x02000000	// should never be on a brush, only in game
#define CONTENTS_DEADMONSTER	0x04000000
#define CONTENTS_DETAIL	0x08000000	// brushes to be added after vis leafs
#define CONTENTS_TRANSLUCENT	0x10000000	// auto set if any surface has trans
#define CONTENTS_LADDER	0x20000000

#define SURF_LIGHT		0x00000001	// value will hold the light strength
#define SURF_SLICK		0x00000002	// effects game physics
#define SURF_SKY		0x00000004	// don't draw, but add to skybox
#define SURF_WARP		0x00000008	// turbulent water warp
#define SURF_TRANS33	0x00000010	// 33% opacity
#define SURF_TRANS66	0x00000020	// 66% opacity
#define SURF_FLOWING	0x00000040	// scroll towards angle
#define SURF_NODRAW		0x00000080	// don't bother referencing the texture
#define SURF_HINT		0x00000100	// make a primary bsp splitter
#define SURF_SKIP		0x00000200	// completely ignore, allowing non-closed brushes

// xash 0.45 surfaces replacement table
#define SURF_MIRROR		0x00010000	// mirror surface
#define SURF_PORTAL		0x00020000	// portal surface

string	nextanimchain;
file_t	*f;

bool Conv_WriteShader( const char *shaderpath, const char *imagepath, rgbdata_t *p, float *rad, float scale, int flags, int contents )
{
	file_t	*f = NULL;
	string	qcname, qcpath;
	string	temp, lumpname;
	string	wadname;

	// write also wadlist.qc for xwad compiler
	FS_ExtractFilePath( imagepath, temp );
	FS_FileBase( imagepath, lumpname );
	FS_FileBase( temp, qcname );
	FS_FileBase( temp, wadname );
	FS_DefaultExtension( qcname, ".qc" );
	FS_DefaultExtension( wadname, ".wad" ); // check for wad later
	com.snprintf( qcpath, MAX_STRING, "%s/%s/$%s", gs_gamedir, temp, qcname );

	if( write_qscsript )
	{	
		if(FS_FileExists( qcpath ))
		{
			script_t	*test;
			token_t	token;

			// already exist, search for current name
			test = Com_OpenScript( qcpath, NULL, 0 );
			while( Com_ReadToken( test, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token ))
			{
				if( !com.stricmp( token.string, "$mipmap" ))
				{
					Com_ReadToken( test, SC_PARSE_GENERIC, &token );
					if( !com.stricmp( token.string, lumpname ))
					{
						Com_CloseScript( test );
						goto check_shader;	// already exist
					}
				}
				Com_SkipRestOfLine( test );
			}
			Com_CloseScript( test );
			f = FS_Open( qcpath, "a" );	// append
		}
		else
		{
			FS_StripExtension( qcname ); // no need anymore
			f = FS_Open( qcpath, "w" );	// new file
			// write description
			FS_Print(f,"//=======================================================================\n");
			FS_Printf(f,"//\t\t\tCopyright XashXT Group %s ©\n", timestamp( TIME_YEAR_ONLY ));
			FS_Print(f,"//\t\t\twritten by Xash Miptex Decompiler\n");
			FS_Print(f,"//=======================================================================\n");
			FS_Printf(f,"$wadname\t%s.wad\n\n", qcname );
		}
	}

	if( f && p )
	{
		FS_Printf( f,"$mipmap\t%s\t0 0 %d %d\n", lumpname, p->width, p->height );
		FS_Close( f ); // all done		
	}

check_shader:
	// nothing to write
	if(!flags && !contents && !com.strlen(nextanimchain))
		return false;

	if(FS_FileExists( shaderpath ))
	{
		script_t	*test;
		token_t	token;

		// already exist, search for current shader
		test = Com_OpenScript( shaderpath, NULL, 0 );
		while( Com_ReadToken( test, SC_ALLOW_NEWLINES, &token ))
		{
			if( !com.stricmp( token.string, imagepath ))
			{
				Msg("%s already exist\n", imagepath );
				Com_CloseScript( test );
				return false;	// already exist
			}
			Com_SkipRestOfLine( test );
		}
		Com_CloseScript( test );
		f = FS_Open( shaderpath, "a" );	// append
	}
	else
	{
		f = FS_Open( shaderpath, "w" );	// new file
		// write description
		FS_Print(f,"//=======================================================================\n");
		FS_Printf(f,"//\t\t\tCopyright XashXT Group %s ©\n", timestamp( TIME_YEAR_ONLY ));
		FS_Print(f,"//\t\t\twritten by Xash Miptex Decompiler\n");
		FS_Print(f,"//=======================================================================\n");
	}
	FS_StripExtension( (char *)imagepath );

	if(FS_FileExists( wadname ))
		FS_Printf(f, "\ntextures/%s\n{\n", lumpname ); // it's wad texture, kill path
	else FS_Printf(f, "\n%s\n{\n", imagepath ); // writepath
	if(flags & SURF_LIGHT)
	{
		FS_Print( f, "\tsurfaceparm\tlight\n" );
		if(!VectorIsNull(rad)) FS_Printf(f, "\tradiocity\t\t%.f %.f %.f\n", rad[0], rad[1], rad[2] );
		if( scale ) FS_Printf(f, "\tintencity\t\t%.f\n", scale );
	}
	if(flags & SURF_WARP)
	{
		// server relevant contents
		if(contents & CONTENTS_WATER)
			FS_Print( f, "\tsurfaceparm\twater\n" );
		else if(contents & CONTENTS_SLIME)		
			FS_Print( f, "\tsurfaceparm\tslime\n" );
		else if(contents & CONTENTS_LAVA)
			FS_Print( f, "\tsurfaceparm\tlava\n" );
		else FS_Print( f, "\tsurfaceparm\twater\n" );
	}
	if(flags & SURF_SKY) FS_Print( f, "\tsurfaceparm\tsky\n" );
	if(flags & SURF_HINT) FS_Print( f, "\tsurfaceparm\thint\n" );
	if(flags & SURF_SKIP) FS_Print( f, "\tsurfaceparm\tskip\n" );
	if(flags & SURF_MIRROR) FS_Print( f, "\tsurfaceparm\tmirror\n" );
	if(flags & SURF_NODRAW) FS_Print( f, "\tsurfaceparm\tnull\n" );
	if(flags & SURF_TRANS33) FS_Print( f, "\tsurfaceparm\tblend\n" );
	if(flags & SURF_TRANS66) FS_Print( f, "\tsurfaceparm\tblend\n" );

	if(contents & CONTENTS_MONSTERCLIP && contents && CONTENTS_PLAYERCLIP)
		FS_Print( f, "\tsurfaceparm\tclip\n" );
	else if(contents & CONTENTS_MONSTERCLIP)
		FS_Print( f, "\tsurfaceparm\tmonsterclip\n" );
	else if(contents & CONTENTS_PLAYERCLIP)
		FS_Print( f, "\tsurfaceparm\tplayerclip\n" );
	if(contents & CONTENTS_WINDOW) FS_Print( f, "\tsurfaceparm\twindow\n" );
	if(contents & CONTENTS_ORIGIN) FS_Print( f, "\tsurfaceparm\torigin\n" );
	if(contents & CONTENTS_TRANSLUCENT) FS_Print( f, "\tsurfaceparm\ttrans\n" );
	if(contents & CONTENTS_AREAPORTAL) FS_Print( f, "\tsurfaceparm\tareaportal\n" );
	if(contents & CONTENTS_DETAIL) FS_Print( f, "\tsurfaceparm\tdetail\n" );
	if(com.strlen(nextanimchain)) FS_Printf( f, "\tnextframe\t\t%s\n", nextanimchain );

	FS_Print( f, "}\n"); // close shader
	FS_Close( f );

	return true;
} 

/*
=============
Doom1
=============
*/
bool Conv_ShaderGetFlags1( const char *imagename, const char *shadername, const char *ext, int *flags, int *contents )
{
	if(!com.strnicmp(imagename, "sky", 3 )) *flags |= SURF_SKY;
	if(com.stristr(imagename, "lit" )) *flags |= SURF_LIGHT;

	if(!com.strnicmp( "sw", imagename, 2 ))
	{
		// wallbuttons anim support
		char	c1 = imagename[5]; // anim number
		string	temp1;

		com.strncpy( temp1, imagename, MAX_STRING );
		if(c1 >= '1' && c1 < '9') c1++; // go to next symbol
		temp1[5] = c1; // replace
                    
		if(FS_FileExists( va("textures/%s/%s.%s", shadername, temp1, ext )))
			com.snprintf( nextanimchain, MAX_STRING, "%s/%s", shadername, temp1 );
		else
		{
			c1 -= 2; // evil stuff...
			temp1[5] = c1; // try to close animchain...
			com.snprintf( nextanimchain, MAX_STRING, "%s/%s", shadername, temp1 );
		}
	}
	return true;
}

/*
=============
Quake and Hl
=============
*/
bool Conv_ShaderGetFlags2( const char *imagename, const char *shadername, const char *ext, int *flags, int *contents )
{
	if(*flags || *contents) return false; // wally flags already exist	

	// try to exctract contents and flags directly form mip-name
	if(imagename[0] == '!' || imagename[0] == '*') *flags |= SURF_WARP; // liquids
	else if(imagename[0] == '{') *contents |= CONTENTS_TRANSLUCENT; // grates
	else if(imagename[0] == '+')
	{
		char	c1 = imagename[1], c2;
		string	temp1, temp2;

		com.strncpy( temp1, imagename, MAX_STRING );
		com.strncpy( temp2, imagename, MAX_STRING );

		if(c1 == 'a' || c1 == 'A') c2 = '0';
		if(c1 == '0' ) c2 = 'A';		
		c1++; // go to next symbol

		temp1[1] = c1; // replace
		temp2[1] = c2; // replace

		// make animchain
		if(FS_FileExists( va("textures/%s/%s.%s", shadername, temp1, ext )))
			com.snprintf( nextanimchain, MAX_STRING, "%s/%s", shadername, temp1 );
		else if(FS_FileExists( va("textures/%s/%s.%s", shadername, temp2, ext )))
			com.snprintf( nextanimchain, MAX_STRING, "%s/%s", shadername, temp2 );
		else if(imagename[1] != '0')
		{
			temp1[1] = '0'; // try to close animchain...
			com.snprintf( nextanimchain, MAX_STRING, "%s/%s", shadername, temp1 );
		}
		else if(imagename[1] != 'A')
		{
			temp1[1] = 'A'; // try to close animchain...
			com.snprintf( nextanimchain, MAX_STRING, "%s/%s", shadername, temp1 );
		}
	}

	// light definition
	if(com.strchr(imagename, '~')) *flags |= SURF_LIGHT;

	if(com.stristr( imagename, "water" )) 
	{
		*contents |= CONTENTS_WATER;
		*flags |= SURF_WARP; // liquids
	}
	else if(com.stristr( imagename, "slime" ))
	{
		*contents |= CONTENTS_SLIME;
		*flags |= SURF_WARP; // liquids
	}
	else if(com.stristr( imagename, "lava" ))
	{
		*contents |= CONTENTS_LAVA;
		*flags |= SURF_WARP; // liquids
	}

	// search for keywords
	if(!com.strnicmp(imagename, "sky", 3 )) *flags |= SURF_SKY;
	else if(!com.strnicmp(imagename, "origin",6)) *contents |= CONTENTS_ORIGIN;
	else if(!com.strnicmp(imagename, "clip", 4 )) *contents |= CONTENTS_MONSTERCLIP|CONTENTS_PLAYERCLIP;
	else if(!com.strnicmp(imagename, "hint", 4 )) *flags |= SURF_HINT;
	else if(!com.strnicmp(imagename, "skip", 4 )) *flags |= SURF_SKIP;
	else if(!com.strnicmp(imagename, "null", 4 )) *flags |= SURF_NODRAW;
	else if(!com.strnicmp(imagename, "translucent", 11 )) *flags |= CONTENTS_TRANSLUCENT;
	else if(!com.strnicmp(imagename, "glass", 5 )) *flags |= SURF_TRANS66;
	else if(!com.strnicmp(imagename, "mirror", 6 )) *flags |= SURF_MIRROR;
	else if(!com.strnicmp(imagename, "portal", 6 )) *flags |= SURF_PORTAL;

	return true;
}

bool Conv_CreateShader( const char *name, rgbdata_t *pic, const char *ext, const char *anim, int surf, int cnt )
{
	string	shadername, imagename;
	string	shaderpath, imagepath;
	vec3_t	radiocity = {0,0,0};
	float	intencity = 0;
	int	flags = 0, contents = 0;

	// extract fodler name from path
	FS_ExtractFilePath( name, shadername );
	FS_FileBase( shadername, shadername ); // remove "textures" from path
	FS_FileBase( name, imagename );
	com.snprintf( shaderpath, MAX_STRING, "%s/scripts/%s.txt", gs_gamedir, shadername );
	com.snprintf( imagepath, MAX_STRING, "%s", name ); // full path
	nextanimchain[0] = 0; // clear chain

	if(anim && com.strlen(anim)) // wally animname
		com.strncpy( nextanimchain, anim, MAX_STRING );
	flags |= surf; // .wal can transmit flags here
	contents |= cnt; 

	if(!com.strnicmp(ext, "flt", 3 )) // doom definitions
		Conv_ShaderGetFlags1( imagename, shadername, ext, &flags, &contents );
	else Conv_ShaderGetFlags2( imagename, shadername, ext, &flags, &contents );

	if( flags & SURF_LIGHT )
	{
		// FIXME: calculate image light color here
	}
	return Conv_WriteShader( shaderpath, imagepath, pic, radiocity, intencity, flags, contents );
}