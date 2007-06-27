//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	sprlib.c - sprite generator
//=======================================================================

#include "platform.h"
#include <basemath.h>
#include "baseutils.h"
#include <sprite.h>

#define MAX_BUFFER_SIZE	0x100000
#define MAX_FRAMES		1024
#define MAX_FRAME_DIM	512

dsprite_t	sprite;
byte	*byteimage, *lbmpalette;
int	byteimagewidth, byteimageheight;
byte	*lumpbuffer = NULL, *plump;
char	spriteoutname[MAX_SYSPATH];
int	framesmaxs[2];
int	framecount;
int	origin_x;
int	origin_y;

typedef struct
{
	frametype_t	type;		// single frame or group of frames
	void		*pdata;		// either a dspriteframe_t or group info
	float		interval;		// only used for frames in groups
	int		numgroupframes;	// only used by group headers
}spritepackage_t;
spritepackage_t frames[MAX_FRAMES];

/*
============
WriteFrame
============
*/
void WriteFrame (file_t *spriteouthandle, int framenum)
{
	dspriteframe_t	*pframe;
	dspriteframe_t	frametemp;

	pframe = (dspriteframe_t *)frames[framenum].pdata;
	frametemp.origin[0] = LittleLong (pframe->origin[0]);
	frametemp.origin[1] = LittleLong (pframe->origin[1]);
	frametemp.width = LittleLong (pframe->width);
	frametemp.height = LittleLong (pframe->height);

	FS_Write (spriteouthandle, &frametemp, sizeof(frametemp));
	FS_Write (spriteouthandle, (byte *)(pframe + 1), pframe->height * pframe->width);
}


/*
============
WriteSprite
============
*/
void WriteSprite (file_t *spriteouthandle)
{
	int i, groupframe, curframe;
	short cnt = 256;
	dsprite_t	spritetemp;

	sprite.boundingradius = sqrt(((framesmaxs[0] >> 1) * (framesmaxs[0] >> 1)) + ((framesmaxs[1] >> 1) * (framesmaxs[1] >> 1)));

	// write out the sprite header
	spritetemp.type = LittleLong (sprite.type);
	spritetemp.texFormat = LittleLong (sprite.texFormat);
	spritetemp.boundingradius = LittleFloat (sprite.boundingradius);
	spritetemp.width = LittleLong (framesmaxs[0]);
	spritetemp.height = LittleLong (framesmaxs[1]);
	spritetemp.numframes = LittleLong (sprite.numframes);
	spritetemp.version = LittleLong (sprite.version);
	spritetemp.ident = LittleLong (IDSPRITEHEADER);

	//not write enchanced parms into hl sprite
	switch( spritetemp.version )
	{
	case SPRITE_VERSION_HALF:
		spritetemp.framerate = 0;//old beamlength
		spritetemp.rgbacolor = 0;//old synctype
		break;
	case SPRITE_VERSION_XASH:
		spritetemp.framerate = LittleFloat (sprite.framerate);
		spritetemp.rgbacolor = LittleLong (sprite.rgbacolor);
		break;
	}
          
	FS_Write(spriteouthandle, &spritetemp, sizeof(spritetemp));

	// Write out palette in 16bit mode
	FS_Write( spriteouthandle, (void *)&cnt, sizeof(cnt));
	FS_Write( spriteouthandle, lbmpalette, cnt * 3 );

	// write out the frames
	curframe = 0;

	for (i = 0; i < sprite.numframes; i++)
	{
		FS_Write (spriteouthandle, &frames[curframe].type, sizeof(frames[curframe].type));

		if (frames[curframe].type == SPR_SINGLE)
		{
			// single (non-grouped) frame
			WriteFrame (spriteouthandle, curframe);
			curframe++;
		}
		else
		{
			int		j, numframes;
			dspritegroup_t	dsgroup;
			float		totinterval;

			groupframe = curframe;
			curframe++;
			numframes = frames[groupframe].numgroupframes;

			// set and write the group header
			dsgroup.numframes = LittleLong (numframes);
			FS_Write (spriteouthandle, &dsgroup, sizeof(dsgroup));

			// write the interval array
			totinterval = 0.0;

			for (j = 0; j < numframes; j++)
			{
				dspriteinterval_t	temp;

				totinterval += frames[groupframe+1+j].interval;
				temp.interval = LittleFloat (totinterval);
				FS_Write (spriteouthandle, &temp, sizeof(temp));
			}

			for (j = 0; j < numframes; j++)
			{
				WriteFrame (spriteouthandle, curframe);
				curframe++;
			}
		}
	}

}

/*
==============
WriteSPRFile
==============
*/
void WriteSPRFile (void)
{
	file_t	*spriteouthandle;

	if(sprite.numframes == 0) Sys_Error ("no frames\n");
	if((plump - lumpbuffer) > MAX_BUFFER_SIZE)
		Sys_Error ("Sprite package too big; increase MAX_BUFFER_SIZE");

	spriteouthandle = FS_Open(spriteoutname, "wb", true, false );
	Msg("writing %s\n", spriteoutname);
	WriteSprite (spriteouthandle);
	
	FS_Close(spriteouthandle);
	
	Msg("%d frame%s\n", sprite.numframes, sprite.numframes > 1 ? "s" : "" );
	Msg("%d ungrouped frame%s, including group headers\n", framecount, framecount > 1 ? "s" : "");
	
	spriteoutname[0] = 0;// clear for a new sprite
}

/*
==============
Load_BMP
==============
*/
void Load_BMP ( char *filename )
{
	byteimage = ReadBMP (filename, &lbmpalette, &byteimagewidth, &byteimageheight);
	if(!byteimage) Sys_Error( "unable to load file \"%s\"\n", filename );
}

/*
==============
Load_TGA
==============
*/
void Load_TGA ( char *filename )
{
	byteimage = ReadTGA (filename, &lbmpalette, &byteimagewidth, &byteimageheight);
	if(!byteimage) Sys_Error( "unable to load file \"%s\"\n", filename );
}

/*
===============
Cmd_Type

syntax: "$type preset"
===============
*/
void Cmd_Type (void)
{
	SC_GetToken (false);

	if (SC_MatchToken( "vp_parallel_upright" )) sprite.type = SPR_VP_PARALLEL_UPRIGHT;
	else if (SC_MatchToken( "facing_upright" )) sprite.type = SPR_FACING_UPRIGHT;
	else if (SC_MatchToken( "vp_parallel" )) sprite.type = SPR_VP_PARALLEL;
	else if (SC_MatchToken( "oriented" )) sprite.type = SPR_ORIENTED;
	else if (SC_MatchToken( "vp_parallel_oriented")) sprite.type = SPR_VP_PARALLEL_ORIENTED;
	else sprite.type = SPR_VP_PARALLEL; //default
}

/*
===============
Cmd_Texture

syntax: "$texture preset"
===============
*/
void Cmd_Texture ( void )
{
	SC_GetToken (false);

	if (SC_MatchToken( "additive")) sprite.texFormat = SPR_ADDITIVE;
	else if (SC_MatchToken( "normal")) sprite.texFormat = SPR_NORMAL;
	else if (SC_MatchToken( "indexalpha")) sprite.texFormat = SPR_INDEXALPHA;
	else if (SC_MatchToken( "alphatest")) sprite.texFormat = SPR_ALPHTEST;
	else if (SC_MatchToken( "glow")) sprite.texFormat = SPR_ADDGLOW;
	else sprite.texFormat = SPR_NORMAL; //default
}

/*
===============
Cmd_Framerate

syntax: "$framerate value"
===============
*/
void Cmd_Framerate( void )
{
	sprite.framerate = atof(SC_GetToken (false));
	sprite.version = SPRITE_VERSION_XASH; //enchaned version
}

/*
===============
Cmd_Load

syntax "$load fire01.bmp"
===============
*/
void Cmd_Load (void)
{
	static byte origpalette[256*3];
          char *name = SC_GetToken ( false );
	const char *ext = FS_FileExtension( name );
	dspriteframe_t	*pframe;
	int x, y, w, h, pix;
	byte *screen_p;

	if(!stricmp(ext, "bmp")) Load_BMP( name );
	else if(!stricmp(ext, "tga")) Load_TGA( name );
	else Sys_Error("unknown graphics type: \"%s\"\n", name );

	if(sprite.numframes == 0) memcpy( origpalette, lbmpalette, sizeof( origpalette ));
	else if (memcmp( origpalette, lbmpalette, sizeof( origpalette )))
		Sys_Error( "bitmap \"%s\" doesn't share a pallette with the previous bitmap\n", name );	

	w = byteimagewidth;
	h = byteimageheight;

	if ((w > MAX_FRAME_DIM) || (h > MAX_FRAME_DIM))
		Sys_Error ("Sprite has a dimension longer than 256");

	pframe = (dspriteframe_t *)plump;
	frames[framecount].pdata = pframe;
	frames[framecount].type = SPR_SINGLE;
	frames[framecount].interval = 0.1f;

	if((origin_x != 0) && (origin_y != 0))
	{
		//write shared origin
		pframe->origin[0] = -origin_x;
		pframe->origin[1] = origin_y;
	}
	else
	{
		//use center of image
		pframe->origin[0] = -(w >> 1);
		pframe->origin[1] = h >> 1;
	}

	pframe->width = w;
	pframe->height = h;

	//adjust maxsize
	if (w > framesmaxs[0]) framesmaxs[0] = w;
	if (h > framesmaxs[1]) framesmaxs[1] = h;

	plump = (byte *)(pframe + 1);
	screen_p = byteimage;

	for (y = 0; y < byteimageheight; y++)
	{
		for (x = 0; x < byteimagewidth; x++)
		{
			pix = *screen_p;
			*screen_p++ = 0;
			*plump++ = pix;
		}
	}

	framecount++;
	if (framecount >= MAX_FRAMES) Sys_Error ("Too many frames in package\n");
}

/*
===============
Cmd_Offset

syntax: $origin "x_pos y_pos"
===============
*/

void Cmd_Offset (void)
{
	origin_x = atoi(SC_GetToken (false));
	origin_y = atoi(SC_GetToken (false));
}

/*
===============
Cmd_Color

synatx: "$color r g b <alpha>"
===============
*/
void Cmd_Color( void )
{
	byte r, g, b, a;
	r = atoi(SC_GetToken (false));
	g = atoi(SC_GetToken (false));
	b = atoi(SC_GetToken (false));

	if (SC_TryToken()) a = atoi(token);
	else a = 0xFF;//fullbright
	
	//pack into one integer
	sprite.rgbacolor = (a<<24) + (b<<16) + (g<<8) + (r<<0);
	sprite.version = SPRITE_VERSION_XASH; //enchaned version
}

/*
===============
Cmd_Group

syntax: 
$group
{
	$load fire01.bmp
	$load fire02.bmp
	$load fire03.bmp
}	
===============
*/

void Cmd_Group (void)
{
	int	groupframe;
	int	is_started = 0;

	groupframe = framecount++;

	frames[groupframe].type = SPR_GROUP;
	frames[groupframe].numgroupframes = 0;

	while (1)
	{
		if(!SC_GetToken (true)) Sys_Error ("End of file during group");

		if(SC_MatchToken( "{" )) is_started = 1;
		else if (SC_MatchToken( "}" )) break;//end of group
		else if (SC_MatchToken("$frame")) while(SC_TryToken());//skip old stuff
		else if (SC_MatchToken("$load" ))
		{
			Cmd_Load ();
			frames[groupframe].numgroupframes++;
                    }
		else if(is_started) Sys_Error("missing }\n");
		else Sys_Error ("$frame or $load expected\n");
	}
	if (frames[groupframe].numgroupframes == 0) Msg("Warning: empty group\n");
}

/*
==============
Cmd_Spritename

syntax: "$spritename outname"
==============
*/
void Cmd_Spritename (void)
{
	strcpy( spriteoutname, SC_GetToken (false));
	FS_DefaultExtension( spriteoutname, ".spr" );
}

void Cmd_SpriteUnknown( void )
{
	Msg("Warning: bad command %s\n", token);
	while(SC_TryToken());
}

void ResetSpriteInfo( void )
{
	//set default sprite parms
	FS_FileBase(gs_mapname, spriteoutname );//kill path and ext
	FS_DefaultExtension( spriteoutname, ".spr" );//set new ext
	
	memset (&sprite, 0, sizeof(sprite));
	framecount = 0;

	framesmaxs[0] = -9999999;
	framesmaxs[1] = -9999999;

	if (!lumpbuffer ) lumpbuffer = Malloc(MAX_BUFFER_SIZE * 2); // *2 for padding

	plump = lumpbuffer;
	sprite.version = SPRITE_VERSION_HALF;//normal sprite
	sprite.type = SPR_VP_PARALLEL;
}

/*
===============
ParseScript	
===============
*/
bool ParseSpriteScript (void)
{
	ResetSpriteInfo();
	
	while (1)
	{
		if(!SC_GetToken (true))break;

		if (SC_MatchToken( "$frame" )) while(SC_TryToken());
		else if (SC_MatchToken( "$spritename" )) Cmd_Spritename();
		else if (SC_MatchToken( "$type" )) Cmd_Type();
		else if (SC_MatchToken( "$texture" )) Cmd_Texture();
		else if (SC_MatchToken( "$origin" )) Cmd_Offset();
		else if (SC_MatchToken( "$framerate" )) Cmd_Framerate();
		else if (SC_MatchToken( "$color" )) Cmd_Color();
		else if (SC_MatchToken( "$load" ))
		{
			Cmd_Load ();
			sprite.numframes++;
		}		
		else if (SC_MatchToken( "$group" ))
		{
			Cmd_Group ();
			sprite.numframes++;
		}
		else if(SC_MatchToken( "$modelname" ))//check for studiomdl script
		{
			Msg("%s probably studio qc.script, skipping...\n", gs_mapname );
			return false;
		}	
		else if(SC_MatchToken( "$body" ))//check for studiomdl script
		{
			Msg("%s probably studio qc.script, skipping...\n", gs_mapname );
			return false;
		}
		else Cmd_SpriteUnknown();
	}
	return true;
}

void ClearSprite( void )
{
	origin_x = 0;
	origin_y = 0;

	memset(frames, 0, sizeof(spritepackage_t) * MAX_FRAMES );
}

bool CompileCurrentSprite( char *name )
{
	bool load = false;
	
	if(name) strcpy( gs_mapname, name );
	FS_DefaultExtension( gs_mapname, ".qc" );
	load = FS_LoadScript( gs_mapname );
	
	if(load)
	{
		if(!ParseSpriteScript())
			return false;
		WriteSPRFile();
		ClearSprite();
		return true;
	}

	Msg("%s not found\n", gs_mapname );
	return false;
}

bool MakeSprite ( void )
{
	search_t	*search;
	int i, numCompiledSprites = 0;

	if(!FS_GetParmFromCmdLine("-qcfile", gs_mapname )) 
	{
		//search for all .ac files in folder		
		search = FS_Search("*.qc", true, false);
		if(!search) Sys_Error("no qcfiles found in this folder!\n");

		for( i = 0; i < search->numfilenames; i++ )
		{
			if(CompileCurrentSprite( search->filenames[i] ))
				numCompiledSprites++;
		}
	}
	else CompileCurrentSprite( NULL );
	
	if(numCompiledSprites > 1) Msg("total %d sprites compiled\n", numCompiledSprites );	
	Sys_Error("");
	return false;
}