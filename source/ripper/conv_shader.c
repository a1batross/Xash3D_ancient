//=======================================================================
//			Copyright XashXT Group 2007 ©
//		conv_shader.c - analyze and write texture shader
//=======================================================================

#include "ripper.h"
#include "mathlib.h"
#include "qc_gen.h"

string	nextanimchain;
file_t	*f;

vec_t Conv_NormalizeColor(vec3_t in, vec3_t out)
{
	float	max, scale;

	max = in[0];
	if(in[1] > max) max = in[1];
	if(in[2] > max) max = in[2];

	// ignore green color
	if(max == 0) return 0;
	scale = 255.0f / max;
	VectorScale( in, scale, out );
	return max;
}

bool Conv_WriteShader( const char *shaderpath, const char *imagepath, float *rad, float scale, int flags, int contents )
{
	file_t *f;

	// nothing to write
	if(!flags && !contents && !com.strlen(nextanimchain))
		return false;

	if(FS_FileExists( shaderpath ))
	{
		// already exist, search for current shader
		Com_LoadScript( shaderpath, NULL, 0 );
		while(Com_GetToken( true ))
		{
			if(Com_MatchToken( imagepath ))
				return false;	// already exist
		}
		f = FS_Open( shaderpath, "a" );	// append
	}
	else
	{
		f = FS_Open( shaderpath, "w" );	// new file
		// write description
		FS_Print(f,"//=======================================================================\n");
		FS_Print(f,"//\t\t\tCopyright XashXT Group 2007 ©\n");
		FS_Print(f,"//\t\t\twritten by Xash Miptex Decompiler\n");
		FS_Print(f,"//=======================================================================\n");
	}
	FS_StripExtension( (char *)imagepath );
	FS_Printf(f, "\n%s\n{\n", imagepath ); // writepath
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
	if(flags & SURF_NULL) FS_Print( f, "\tsurfaceparm\tnull\n" );
	if(flags & SURF_MIRROR) FS_Print( f, "\tsurfaceparm\tmirror\n" );
	if(flags & SURF_NODRAW) FS_Print( f, "\tsurfaceparm\tnodraw\n" );
	if(flags & SURF_BLEND) FS_Print( f, "\tsurfaceparm\tblend\n" );

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
	else if(!com.strnicmp(imagename, "clip", 4 )) *contents |= CONTENTS_CLIP;
	else if(!com.strnicmp(imagename, "hint", 4 )) *flags |= SURF_HINT;
	else if(!com.strnicmp(imagename, "skip", 4 )) *flags |= SURF_SKIP;
	else if(!com.strnicmp(imagename, "null", 4 )) *flags |= SURF_NULL;
	else if(!com.strnicmp(imagename, "translucent", 11 )) *flags |= CONTENTS_TRANSLUCENT;
	else if(!com.strnicmp(imagename, "mirror", 6 )) *flags |= SURF_MIRROR;

	return true;
}

bool Conv_CreateShader( const char *name, rgbdata_t *pic, const char *ext, const char *anim, int surf, int cnt )
{
	string	shadername, imagename;
	string	shaderpath, imagepath;
	vec3_t	radiocity = {0,0,0};
	float	intencity = 0;
	int	i, flags = 0, contents = 0;

	// extract fodler name from path
	FS_ExtractFilePath( name, shadername );
	FS_FileBase( shadername, shadername ); // remove "textures" from path
	FS_FileBase( name, imagename );
	com.snprintf( shaderpath, MAX_STRING, "%s/scripts/shaders/%s.txt", gs_gamedir, shadername );
	if(com.stristr(name, "textures")) com.snprintf( imagepath, MAX_STRING, "%s", name ); // full path
	else com.snprintf( imagepath, MAX_STRING, "textures/%s", name ); // build full path
	nextanimchain[0] = 0; // clear chain

	if(anim && com.strlen(anim)) // wally animname
		com.strncpy( nextanimchain, anim, MAX_STRING );
	flags |= surf; // .wal can transmit flags here
	contents |= cnt; 

	if(!com.strnicmp(ext, "flt", 3 )) // doom definitions
		Conv_ShaderGetFlags1( imagename, shadername, ext, &flags, &contents );
	else Conv_ShaderGetFlags2( imagename, shadername, ext, &flags, &contents );

	if(flags & SURF_LIGHT)
	{
		float	r, scale;
		vec3_t	color = {0,0,0};
		int	texels = pic->width * pic->height;

		// needs calculate reflectivity
		for (i = 0; i < texels; i++ )
		{
			color[0] += pic->buffer[i*4+0];
			color[1] += pic->buffer[i*4+1];
			color[2] += pic->buffer[i+4+2];
		}
		for (i = 0; i < 3; i++)
		{
			r = color[i]/texels;
			radiocity[i] = r;
		}
		// scale the reflectivity up, because the textures are so dim
		scale = Conv_NormalizeColor( radiocity, radiocity );
		intencity = texels * 255.0 / scale; // basic intensity value
	}
	return Conv_WriteShader( shaderpath, imagepath, radiocity, intencity, flags, contents );
}

void Skin_WriteSequence( void )
{
	int	i;

	// time to dump frames :)
	if( flat.angledframes == 8 )
	{
		// angled group is full, dump it!
		FS_Print(f, "\n$angled\n{\n" );
		FS_Printf(f, "\t// frame '%c'\n", flat.frame[0].name[4] );
		for( i = 0; i < 8; i++)
		{
			FS_Printf(f,"\t$load\t\t%s.tga", flat.frame[i].name );
			if( flat.frame[i].xmirrored )FS_Print(f," flip_x\n"); 
			else FS_Print(f, "\n" );
			FS_Printf(f,"\t$frame\t\t0 0 %d %d", flat.frame[i].width, flat.frame[i].height );
			FS_Printf(f, " 0.1 %d %d\n", flat.frame[i].origin[0], flat.frame[i].origin[1] );
		}
		FS_Print(f, "}\n" );
	}
	else if( flat.normalframes == 1 )
	{
		// single frame stored
		FS_Print(f, "\n" );
		FS_Printf(f, "// frame '%c'\n", flat.frame[0].name[4] );
		FS_Printf(f,"$load\t\t%s.tga\n", flat.frame[0].name );
		FS_Printf(f,"$frame\t\t0 0 %d %d", flat.frame[0].width, flat.frame[0].height );
		FS_Printf(f, " 0.1 %d %d\n", flat.frame[0].origin[0], flat.frame[0].origin[1]);
	}

	// drop mirror frames too
	if( flat.mirrorframes == 8 )
	{
		// mirrored group is always flipped
		FS_Print(f, "\n$angled\n{\n" );
		FS_Printf(f, "\t// frame '%c' (mirrored form '%c')\n", flat.frame[0].name[6],flat.frame[0].name[4]);
		for( i = 2; i > -1; i--)
		{
			FS_Printf(f,"\t$load\t\t%s.tga flip_x\n", flat.frame[i].name );
			FS_Printf(f,"\t$frame\t\t0 0 %d %d", flat.frame[i].width, flat.frame[i].height );
			FS_Printf(f, " 0.1 %d %d\n", flat.frame[i].origin[0], flat.frame[i].origin[1] );
		}
		for( i = 7; i > 2; i--)
		{
			FS_Printf(f,"\t$load\t\t%s.tga flip_x\n", flat.frame[i].name );
			FS_Printf(f,"\t$frame\t\t0 0 %d %d", flat.frame[i].width, flat.frame[i].height );
			FS_Printf(f, " 0.1 %d %d\n", flat.frame[i].origin[0], flat.frame[i].origin[1] );
		}
		FS_Print(f, "}\n" );
	}

	memset( &flat.frame, 0, sizeof(flat.frame)); 
	flat.angledframes = flat.normalframes = flat.mirrorframes = 0; // clear all
}

void Skin_FindSequence( const char *name, rgbdata_t *pic )
{
	uint	headlen;
	char	num, header[10];

	// create header from flat name
	com.strncpy( header, name, 10 );
	headlen = com.strlen( name );

	if( flat.animation != header[4] )
	{
		// write animation
		Skin_WriteSequence();
		flat.animation = header[4];
	}

	if( flat.animation == header[4] )
	{
		// continue collect frames
		if( headlen == 6 )
		{
			num = header[5] - '0';
			if(num == 0) flat.normalframes++;	// animation frame
			if(num == 8) num = 0;		// merge 
			flat.angledframes++; 		// angleframe stored
			com.strncpy( flat.frame[num].name, header, 9 );
			flat.frame[num].width = pic->width;
			flat.frame[num].height = pic->height;
			flat.frame[num].origin[0] = pic->width>>1;	// center
			flat.frame[num].origin[1] = pic->height;	// floor
			flat.frame[num].xmirrored = false;
		}
		else if( headlen == 8 )
		{
			// normal image
			num = header[5] - '0';
			if(num == 8) num = 0;  // merge 
			com.strncpy( flat.frame[num].name, header, 9 );
			flat.frame[num].width = pic->width;
			flat.frame[num].height = pic->height;
			flat.frame[num].origin[0] = pic->width>>1;	// center
			flat.frame[num].origin[1] = pic->height;	// floor
			flat.frame[num].xmirrored = false;
			flat.angledframes++; // frame stored

			if( header[4] != header[6] )
			{
				// mirrored groups
				flat.mirrorframes++;
				return;
			}

			// mirrored image
			num = header[7] - '0'; // angle it's a direct acess to group
			if(num == 8) num = 0;  // merge 
			com.strncpy( flat.frame[num].name, header, 9 );
			flat.frame[num].width = pic->width;
			flat.frame[num].height = pic->height;
			flat.frame[num].origin[0] = pic->width>>1;	// center
			flat.frame[num].origin[1] = pic->height;	// floor
			flat.frame[num].xmirrored = true;		// it's mirror frame
			flat.angledframes++; // frame stored
		}
		else Sys_Break("Skin_CreateScript: invalid name %s\n", name );
	}
}

void Skin_ProcessScript( const char *name )
{
	if(flat.in_progress )
	{
		// finish script
		Skin_WriteSequence();
		FS_Close( f );
		flat.in_progress = false;
	}
	if(!flat.in_progress )
	{
		// start from scratch
		com.strncpy( flat.membername, name, 5 );		
		f = FS_Open( va("%s/models/%s.qc", gs_gamedir, flat.membername ), "w" );
		flat.in_progress = true;

		// write description
		FS_Print(f,"//=======================================================================\n");
		FS_Print(f,"//\t\t\tCopyright XashXT Group 2007 ©\n");
		FS_Print(f,"//\t\t\twritten by Xash Miptex Decompiler\n");
		FS_Print(f,"//=======================================================================\n");

		// write sprite header
		FS_Printf(f, "\n$spritename\t%s.spr\n", flat.membername );
		FS_Print(f,  "$type\t\tfacing_upright\n"  ); // constant
		FS_Print(f,  "$texture\t\tindexalpha\n");
	}
}

void Skin_FinalizeScript( void )
{
	if(!flat.in_progress ) return;

	// finish script
	Skin_WriteSequence();
	FS_Close( f );
	flat.in_progress = false;
}

void Skin_CreateScript( const char *name, rgbdata_t *pic )
{
	if(com.strnicmp( name, flat.membername, 4 ))
          	Skin_ProcessScript( name );

	if( flat.in_progress )
		Skin_FindSequence( name, pic );
}