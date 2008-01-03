//=======================================================================
//			Copyright XashXT Group 2007 ©
//		conv_shader.c - analyze and write texture shader
//=======================================================================

#include "idconv.h"
#include "mathlib.h"

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

bool Conv_CreateShader( const char *name, rgbdata_t *pic )
{
	string	shadername, imagename;
	string	shaderpath, imagepath;
	vec3_t	radiocity = {0,0,0};
	float	intensity = 0;
	int	i, flags = 0;
	file_t	*f;

	// extract fodler name from path
	FS_ExtractFilePath( name, shadername );
	FS_FileBase( name, imagename );
	com.snprintf( shaderpath, MAX_STRING, "%s/scripts/shaders/%s.txt", gs_gamedir, shadername );
	com.snprintf( imagepath, MAX_STRING, "textures/%s", name );

	if(com.strchr(imagename, '~') || stristr(imagename, "lit" ))
	{
		float	r, scale;
		vec3_t	color = {0,0,0};
		int	texels = pic->width * pic->height;

		// calculate reflectivity
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
		intensity = texels * scale; // basic intensity value
		flags |= SURF_LIGHT;
	} 
	else if(imagename[0] == '!' || imagename[0] == '*') flags |= SURF_WARP; // liquids
	else if(imagename[0] == '{') flags |= CONTENTS_TRANSLUCENT; // grates
	else if(imagename[0] == '-')
	{
		// random tileing
		Msg("%s it's random tileing texture\n", imagename );
	}
	else if(imagename[0] == '+')
	{
		// animchain
		Msg("%s it's animation texture\n", imagename );
	}


	// search for keywords
	if(!com.strnicmp(imagename, "sky", 3 )) flags |= SURF_SKY;
	else if(!com.strnicmp(imagename, "origin",6)) flags |= CONTENTS_ORIGIN;
	else if(!com.strnicmp(imagename, "hint", 4 )) flags |= SURF_HINT;
	else if(!com.strnicmp(imagename, "skip", 4 )) flags |= SURF_SKIP;
	else if(!com.strnicmp(imagename, "null", 4 )) flags |= SURF_NULL;
	else if(!com.strnicmp(imagename, "translucent", 11 )) flags |= CONTENTS_TRANSLUCENT;
	else if(!com.strnicmp(imagename, "mirror", 6 )) flags |= SURF_MIRROR;

	if(!flags) return false;
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
		FS_Printf(f,"//=======================================================================\n");
		FS_Printf(f,"//\t\t\tCopyright XashXT Group 2007 ©\n");
		FS_Printf(f,"//\t\t\t%s.txt - Shader Description\n", imagename );
		FS_Printf(f,"//=======================================================================\n");
	}
	FS_Printf(f, "\n%s\n{\n", imagepath ); // writepath
	if(flags & SURF_LIGHT)
	{
		FS_Print( f, "\tsurfaceparm\tlight\n" );
		FS_Printf(f, "\tradiocity\t\t%.f %.f %.f\n", radiocity[0], radiocity[1], radiocity[2] );
		FS_Printf(f, "\tintencity\t\t%.f %.f %.f\n", radiocity[0], radiocity[1], radiocity[2] );
	}
	if(flags & SURF_WARP)
	{
		// server relevant contents
		if(stristr(imagename, "water" ))
			FS_Print( f, "\tsurfaceparm\twater\n" );
		else if(stristr(imagename, "slime" ))		
			FS_Print( f, "\tsurfaceparm\tslime\n" );
		else if(stristr(imagename, "lava" ))
			FS_Print( f, "\tsurfaceparm\tlava\n" );
		else FS_Print( f, "\tsurfaceparm\twater\n" );
	}
	if(flags & SURF_SKY) FS_Print( f, "\tsurfaceparm\tsky\n" );
	if(flags & SURF_HINT) FS_Print( f, "\tsurfaceparm\thint\n" );
	if(flags & SURF_SKIP) FS_Print( f, "\tsurfaceparm\tskip\n" );
	if(flags & SURF_NULL) FS_Print( f, "\tsurfaceparm\tnull\n" );
	if(flags & CONTENTS_ORIGIN) FS_Print( f, "\tsurfaceparm\torigin\n" );
	if(flags & CONTENTS_TRANSLUCENT) FS_Print( f, "\tsurfaceparm\ttrans\n" );
	if(flags & SURF_MIRROR) FS_Print( f, "\tsurfaceparm\tmirror\n" );

	FS_Print( f, "}\n");
	FS_Close( f );

	return true;
}