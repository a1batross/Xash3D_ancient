//=======================================================================
//			Copyright XashXT Group 2007 ©
//		conv_shader.c - analyze and write texture shader
//=======================================================================

#include "idconv.h"
#include "mathlib.h"

string	nextanimchain;

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
		FS_Print(f,"//\t\t\t Created by Xash IdConvertor\n");
		FS_Print(f,"//=======================================================================\n");
	}
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

bool Conv_ShaderGetFlags( const char *imagename, const char *shadername, int *flags, int *contents )
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
		if(FS_FileExists( va("textures/%s/%s.mip", shadername, temp1 )))
			com.snprintf( nextanimchain, MAX_STRING, "%s/%s", shadername, temp1 );
		else if(FS_FileExists( va("textures/%s/%s.mip", shadername, temp2 )))
			com.snprintf( nextanimchain, MAX_STRING, "%s/%s", shadername, temp2 );
		else
		{
			temp1[1] = '0'; // hmmm ...
			com.snprintf( nextanimchain, MAX_STRING, "%s/%s", shadername, temp1 );
		}
	}

	// light definition
	if(com.strchr(imagename, '~')) *flags |= SURF_LIGHT;

	// search for keywords
	if(!com.strnicmp(imagename, "sky", 3 )) *flags |= SURF_SKY;
	else if(!com.strnicmp(imagename, "origin",6)) *contents |= CONTENTS_ORIGIN;
	else if(!com.strnicmp(imagename, "clip", 4 )) *contents |= (CONTENTS_CLIP);
	else if(!com.strnicmp(imagename, "hint", 4 )) *flags |= SURF_HINT;
	else if(!com.strnicmp(imagename, "skip", 4 )) *flags |= SURF_SKIP;
	else if(!com.strnicmp(imagename, "null", 4 )) *flags |= SURF_NULL;
	else if(!com.strnicmp(imagename, "translucent", 11 )) *flags |= CONTENTS_TRANSLUCENT;
	else if(!com.strnicmp(imagename, "mirror", 6 )) *flags |= SURF_MIRROR;

	return true;
}

bool Conv_CreateShader( const char *name, rgbdata_t *pic, const char *animchain, int imageflags, int imagecontents )
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
	if(stristr(name, "textures")) com.snprintf( imagepath, MAX_STRING, "%s", name ); // full path
	else com.snprintf( imagepath, MAX_STRING, "textures/%s", name ); // build full path
	nextanimchain[0] = 0; // clear chain

	if(animchain && com.strlen(animchain)) // wally animname
		com.strncpy( nextanimchain, animchain, MAX_STRING );
	flags |= imageflags; // .wal can transmit flags here
	contents |= imagecontents; 

	Conv_ShaderGetFlags( imagename, shadername, &flags, &contents );

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