//=======================================================================
//			Copyright XashXT Group 2007 ©
//		conv_shader.c - analyze and write texture shader
//=======================================================================

#include "ripper.h"
#include "mathlib.h"
#include "utils.h"

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
#define CONTENTS_CLIP	(CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP)

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
#define CONTENTS_TRIGGER	0x40000000	// trigger

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
#define SURF_ALPHATEST	0x00040000	// alpha surface

string	animmap[256];	// should be enoguh
int	animcount;	// process counter
int	num_anims;	// shader total count
file_t	*f;

bool Conv_WriteShader( const char *shaderpath, const char *imagepath, rgbdata_t *p, float *rad, float scale, int flags, int contents )
{
	file_t	*f = NULL;
	string	qcname, qcpath;
	string	temp, lumpname;
	string	wadname, shadername;
	int	i, lightmap_stage = false;

	// write also wadlist.qc for xwad compiler
	FS_ExtractFilePath( imagepath, temp );
	FS_FileBase( imagepath, lumpname );
	FS_FileBase( temp, qcname );
	FS_FileBase( temp, wadname );
	com.snprintf( shadername, MAX_STRING, "%s/%s", temp, lumpname );
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
		if( p->flags & IMAGE_HAS_LUMA ) // also add luma image if present
			FS_Printf( f,"$mipmap\t%s_luma\t0 0 %d %d\n", lumpname, p->width, p->height );
		FS_Close( f ); // all done		
	}

check_shader:
	// invalid animation counter, kill it	
	if( num_anims == 1 ) animcount = num_anims = 0;

	// nothing to write
	if( !flags && !contents && !num_anims && !(p->flags & IMAGE_HAS_LUMA))
		return false;

	if(FS_FileExists( shaderpath ))
	{
		script_t	*test;
		token_t	token;

		// already exist, search for current shader
		test = Com_OpenScript( shaderpath, NULL, 0 );
		while( Com_ReadToken( test, SC_ALLOW_NEWLINES|SC_ALLOW_PATHNAMES2, &token ))
		{
			if( !com.stricmp( token.string, shadername ))
			{
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

	FS_Printf( f, "\n%s\n{\n", shadername ); // write shadername

	if( contents & CONTENTS_CLIP && contents && CONTENTS_PLAYERCLIP )
		FS_Print( f, "\tsurfaceparm\tclip\n" );
	else if( contents & CONTENTS_MONSTERCLIP ) FS_Print( f, "\tsurfaceparm\tmonsterclip\n" );
	else if( contents & CONTENTS_PLAYERCLIP ) FS_Print( f, "\tsurfaceparm\tplayerclip\n" );
	else if( contents & CONTENTS_WINDOW ) FS_Print( f, "\tsurfaceparm\twindow\n" );
	else if( contents & CONTENTS_ORIGIN ) FS_Print( f, "\tsurfaceparm\torigin\n" );
	else if( contents & CONTENTS_TRANSLUCENT ) FS_Print( f, "\tsurfaceparm\ttrans\n" );
	else if( contents & CONTENTS_AREAPORTAL ) FS_Print( f, "\tsurfaceparm\tareaportal\n" );
	else if( contents & CONTENTS_TRIGGER )  FS_Print( f, "\tsurfaceparm\ttrigger\n" );
	else if( contents & CONTENTS_DETAIL ) FS_Print( f, "\tsurfaceparm\tdetail\n" );

	if( flags & SURF_LIGHT )
	{
		if(!VectorIsNull( rad )) FS_Printf(f, "\tq3map_lightRGB\t%g %g %g\n", rad[0], rad[1], rad[2] );
		if( scale ) FS_Printf(f, "\tq3map_surfacelight\t%.f\n", scale );
		if( !num_anims )
		{
			FS_Printf( f, "\t{\n\t\tmap\t%s\n\t}\n", shadername );
			lightmap_stage = true;
		}
	}

	if( flags & SURF_WARP )
	{
		FS_Print( f, "\tq3map_globaltexture\n" );
		FS_Print( f, "\tsurfaceparm\tnoLightMap\n" );
		FS_Print( f, "\ttessSize\t\t64\n\n" );

		// server relevant contents
		if(contents & CONTENTS_WATER)
			FS_Print( f, "\tsurfaceparm\twater\n" );
		else if(contents & CONTENTS_SLIME)		
			FS_Print( f, "\tsurfaceparm\tslime\n" );
		else if(contents & CONTENTS_LAVA)
			FS_Print( f, "\tsurfaceparm\tlava\n" );
		else FS_Print( f, "\tsurfaceparm\twater\n" );

		FS_Printf( f, "\t{\n\t\tmap\t%s\n", shadername );	// save basemap
		if( flags & (SURF_TRANS33|SURF_TRANS66))
		{
			FS_Print( f, "\t\tblendFunc\tGL_SRC_ALPHA\tGL_ONE_MINUS_SRC_ALPHA\n" );
			FS_Print( f, "\t\tAlphaGen\t\tvertex\n" ); 
		}
		FS_Print( f, "\t\ttcGen\twarp\n\t}\n" );	// warp
		lightmap_stage = false;
	}
	else if( flags & SURF_SKY ) FS_Print( f, "\tsurfaceparm\tsky\n" );
	else if( flags & SURF_HINT ) FS_Print( f, "\tsurfaceparm\thint\n" );
	else if( flags & SURF_SKIP ) FS_Print( f, "\tsurfaceparm\tskip\n" );
	else if( flags & (SURF_TRANS33|SURF_TRANS66))
	{
		FS_Printf( f, "\t{\n\t\tmap\t%s\n\n", shadername );	// save basemap
		FS_Print( f, "\t\tblendFunc\tGL_SRC_ALPHA\tGL_ONE_MINUS_SRC_ALPHA\n" );
		FS_Print( f, "\t\tAlphaGen\t\tentity\n\t}\n" ); 
		lightmap_stage = true;
	}
	else if( flags & SURF_ALPHATEST )
	{
		FS_Printf( f, "\t{\n\t\tmap\t%s\n\n", shadername );	// save basemap
		FS_Print( f, "\t\talphaFunc\tGL_GREATER 0.666f\t// id Software magic value\n" );
		FS_Print( f, "\t\tAlphaGen\tidentity\n\t}\n" ); 
		lightmap_stage = true;
	}
	else if( flags & SURF_NODRAW ) FS_Print( f, "\tsurfaceparm\tnodraw\n" );

	if( num_anims )
	{
		FS_Printf( f, "\t{\n\t\tAnimFrequency\t%i\n", animcount );	// #frames per second
		for( i = 0; i < num_anims; i++ )		
			FS_Printf( f, "\t\tmap\t\t%s\n", animmap[i] );
		FS_Printf( f, "\t}\n" );	// close section
		lightmap_stage = true;
	}
	else if( p->flags & IMAGE_HAS_LUMA && !( flags & SURF_WARP ))
	{
		FS_Printf( f, "\t{\n\t\tmap\t%s\n\t}\n", shadername );
		lightmap_stage = true;
	}

	if( lightmap_stage )
	{
		FS_Print( f, "\t{\n\t\tmap\t$lightmap\n" );	// lightmap stage
		FS_Print( f, "\t\tblendFunc\tfilter\n" );
		FS_Print( f, "\t}\n" );
	}

	if( p->flags & IMAGE_HAS_LUMA )
	{
		if( num_anims )
		{
			FS_Printf( f, "\t{\n\t\tAnimFrequency\t%i\n", animcount );	// #frames per second
			for( i = 0; i < num_anims; i++ )		
				FS_Printf( f, "\t\tmap\t\t%s_luma\n", animmap[i] ); // anim luma stage
			FS_Printf( f, "\t\tblendFunc\t\tadd\n" );
			FS_Printf( f, "\t}\n" );	// close section
			animcount = num_anims = 0;	// done
		}
		else
		{
			FS_Printf( f, "\t{\n\t\tmap\t%s_luma\n", shadername );	// save luma
			FS_Printf( f, "\t\tblendFunc\tadd\n" );
			if( flags & SURF_WARP ) FS_Print( f, "\t\ttcGen\twarp\n" );
			FS_Printf( f, "\t}\n" );	// close section
		}
	}

	FS_Print( f, "}\n" ); // close shader
	FS_Close( f );

	return true;
} 

/*
=============
generic flags extractor
=============
*/
void Conv_ShaderGetFlags( const char *imagename, const char *shadername, const char *ext, int *flags, int *contents, const char *anim )
{
	if( game_family == GAME_DOOM1 )
	{
		num_anims = animcount = 0; // valid onlt for current frame so reset it
	
		if( !com.strnicmp( imagename, "sky", 3 )) *flags |= SURF_SKY;
		if( com.stristr( imagename, "lit" )) *flags |= SURF_LIGHT;

		if( !com.strnicmp( "sw", imagename, 2 ) || !com.strnicmp( "{sw", imagename, 3 ))
		{
			// wallbuttons anim support
			string	temp1;
			char	c1;
			int	i, j;
                              
			if( imagename[0] == '{' && imagename[4] == '_' )
				c1 = imagename[5], j = 5;
			else if( imagename[3] == 's' )
				c1 = imagename[4], j = 4;
			else if( imagename[4] == '_' )
				c1 = imagename[5], j = 5;

			if( c1 >= '0' && c1 <= '9' );
			else return;

			com.strncpy( temp1, imagename, MAX_STRING );
			for( i = 0; i < 10; i++ )	// Doom1 anim: 0 - 9
			{
				if( !FS_FileExists( va( "%s/%s.flt", shadername, temp1 ))) break;
				com.snprintf( animmap[animcount++], MAX_STRING, "%s/%s", shadername, temp1 );
				temp1[j]++; // increase symbol
			}
			num_anims = animcount;	// can be dump now
		}
	}
	else if( game_family == GAME_QUAKE1 || game_family == GAME_HALFLIFE )
	{
		num_anims = animcount = 0; // valid onlt for current frame so reset it

		if( com.stristr( imagename, "water" )) 
		{
			*contents |= CONTENTS_WATER;
			*flags |= SURF_WARP; // liquids
		}
		else if( com.stristr( imagename, "slime" ))
		{
			*contents |= CONTENTS_SLIME;
			*flags |= SURF_WARP; // liquids
		}
		else if( com.stristr( imagename, "lava" ))
		{
			*contents |= CONTENTS_LAVA;
			*flags |= SURF_WARP; // liquids
		}

		// search for keywords
		if( !com.strnicmp( imagename, "sky", 3 )) *flags |= SURF_SKY;
		else if( !com.strnicmp( imagename, "origin",6)) *contents |= CONTENTS_ORIGIN;
		else if( !com.strnicmp( imagename, "clip", 4 )) *contents |= CONTENTS_CLIP;
		else if( !com.strnicmp( imagename, "hint", 4 )) *flags |= SURF_HINT;
		else if( !com.strnicmp( imagename, "skip", 4 )) *flags |= SURF_SKIP;
		else if( !com.strnicmp( imagename, "null", 4 )) *flags |= SURF_NODRAW;
		else if( !com.strnicmp( imagename, "translucent", 11 )) *contents |= CONTENTS_TRANSLUCENT;
		else if( !com.strnicmp( imagename, "glass", 5 )) *flags |= SURF_TRANS66;
		else if( !com.strnicmp( imagename, "mirror", 6 )) *flags |= SURF_MIRROR;
		else if( !com.strnicmp( imagename, "portal", 6 )) *flags |= SURF_PORTAL;
		else if( com.stristr( imagename, "trigger" )) *contents |= CONTENTS_TRIGGER;
		else if( com.stristr( imagename, "lite" )) *flags |= SURF_LIGHT;

		// try to exctract contents and flags directly form mip-name
		if( imagename[0] == '!' || imagename[0] == '*' ) *flags |= SURF_WARP; // liquids
		else if( imagename[0] == '{' )
		{
			*flags |= SURF_ALPHATEST; // grates
			*contents |= CONTENTS_TRANSLUCENT;
		}
		else if( imagename[0] == '~' ) *flags |= SURF_LIGHT; // light definition
		else if( imagename[0] == '+' )
		{
			char	c1 = imagename[1];
			string	temp1;
			int	i;

			// HL: first map is off second map is on
			if( c1 != '0' && c1 != 'a' && c1 != 'A' )
			{
				return;
			}
			com.strncpy( temp1, imagename, MAX_STRING );
			for( i = 0; i < 10; i++ )	// Quake anim: 0 - 9
			{
				if( !MipExist( va("%s/%s.mip", shadername, temp1 ))) break;
				com.snprintf( animmap[animcount++], MAX_STRING, "%s/%s", shadername, temp1 );
				temp1[1]++; // increase symbol
			}
			if( i > 1 )
			{
				num_anims = animcount; // can be dump now
				return;
			}

			if( c1 == 'a' || c1 == 'A' ) temp1[1] = '0';
			else if( c1 == '0' ) temp1[1] = 'A';

			for( i = 0; i < 10; i++ )	// Quake anim A - K
			{
				if( !MipExist( va("%s/%s.mip", shadername, temp1 ))) break;
				com.snprintf( animmap[animcount++], MAX_STRING, "%s/%s", shadername, temp1 );
				temp1[1]++; // increase symbol
			}
			num_anims = animcount;	// can be dump now
		}
	}
	else if( game_family == GAME_QUAKE2 )
	{
/*
		// this code it's totally wrong, disabled for now
		if( animcount && !com.strlen( anim ))
		{
			// end of chain, dump now
			num_anims = animcount;
			return;
		}

		if( anim && com.strlen( anim ))
		{ 
			int	i;

			if( animcount == 0 )  // add himself first
				com.snprintf( animmap[animcount++], MAX_STRING, "%s/%s", shadername, imagename );

			for( i = 0; i < animcount; i++ )
			{
				if( !com.stricmp( animmap[i], anim ))
				{
					// chain is looped, dump now
					num_anims = animcount;
					return;
				}
			}

			// add next frame
			if( animcount == i ) com.strncpy( animmap[animcount++], anim, MAX_STRING );
		}
		// UNDONE: remove some flags
*/
	}
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
	com.snprintf( shaderpath, MAX_STRING, "%s/scripts/%s.shader", gs_gamedir, shadername );
	com.strncpy( imagepath, name, MAX_STRING ); // full path

	flags |= surf; // .wal can transmit flags here
	contents |= cnt; 

	Conv_ShaderGetFlags( imagename, shadername, ext, &flags, &contents, anim );
	if( animcount >= 256 ) Sys_Break( "Conv_CreateShader: too many animations in shader\n" );

	if( pic->flags & IMAGE_HAS_LUMA )
	{
		// write luma image silently
		Image_Process( &pic, 0, 0, IMAGE_MAKE_LUMA );
		FS_SaveImage( va("%s/%s_luma.%s", gs_gamedir, name, ext ), pic );
	}
	if( flags & SURF_LIGHT )
	{
		int	j, texels;
		byte	*pal, *fin;
		float	scale;

		texels = pic->width * pic->height;
		fin = pic->buffer;
		pal = pic->palette;

		switch( pic->type )
		{
		case PF_RGBA_32:
			for( j = 0; j < texels; j++, fin += 4 )
			{
				radiocity[0] += fin[0];
				radiocity[1] += fin[1];
				radiocity[2] += fin[2];
			}
			texels *= 4;
			break;
		case PF_BGRA_32:
		case PF_ABGR_64:
			for( j = 0; j < texels; j++, fin += 4 )
			{
				radiocity[0] += fin[2];
				radiocity[1] += fin[1];
				radiocity[2] += fin[0];
			}
			texels *= 4;
			break;
		case PF_RGB_24:
			for( j = 0; j < texels; j++, fin += 3 )
			{
				radiocity[0] += fin[0];
				radiocity[1] += fin[1];
				radiocity[2] += fin[2];
			}
			texels *= 3;
			break;			
		case PF_INDEXED_24:
		case PF_INDEXED_32:
			for( j = 0; j < texels; j++ )
			{
				radiocity[0] += pal[fin[j]+0];
				radiocity[1] += pal[fin[j]+1];
				radiocity[2] += pal[fin[j]+2];
			}		
			break;
		default:
			MsgDev( D_WARN, "Conv_CreateShader: %s can't calculate reflectivity\n", name );
			return Conv_WriteShader( shaderpath, imagepath, pic, vec3_origin, 0.0f, flags, contents );
		}

		for( j = 0; j < 3; j++ )
			radiocity[j] /= texels;
		scale = ColorNormalize( radiocity, radiocity );
		if( scale < 0.5f )
		{
			scale *= 2.0f;
			VectorScale( radiocity, scale, radiocity );
		}
		intencity = texels * 255.0 / scale; // basic intensity value
	}
	return Conv_WriteShader( shaderpath, imagepath, pic, radiocity, intencity, flags, contents );
}