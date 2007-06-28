//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	sprlib.c - sprite generator
//=======================================================================

#include "platform.h"
#include <basemath.h>
#include "baseutils.h"
#include <sprite.h>

#define MAX_BUFFER_SIZE	((sizeof(frames) * MAX_FRAMES) + (MAX_FRAME_DIM * 2 * MAX_FRAMES))
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
	int		type;	// single frame or group of frames
	void		*pdata;	// either a dspriteframe_t

} spritepackage_t;

spritepackage_t frames[MAX_FRAMES];

/*
============
WriteFrame
============
*/
void WriteFrame (file_t *f, int framenum)
{
	dspriteframe_t	*pframe;
	dspriteframe_t	frametemp;

	pframe = (dspriteframe_t *)frames[framenum].pdata;
	frametemp.origin[0] = LittleLong (pframe->origin[0]);
	frametemp.origin[1] = LittleLong (pframe->origin[1]);
	frametemp.width = LittleLong (pframe->width);
	frametemp.height = LittleLong (pframe->height);

	FS_Write (f, &frametemp, sizeof(frametemp));
	FS_Write (f, (byte *)(pframe + 1), pframe->height * pframe->width);
}


/*
============
WriteSprite
============
*/
void WriteSprite (file_t *f)
{
	int i, curframe;
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
          
	FS_Write( f, &spritetemp, sizeof(spritetemp));

	// Write out palette in 16bit mode
	FS_Write( f, (void *)&cnt, sizeof(cnt));
	FS_Write( f, lbmpalette, cnt * 3 );

	// write out the frames
	curframe = 0;

	for (i = 0; i < sprite.numframes; i++)
	{
		FS_Write ( f, &frames[curframe].type, sizeof(frames[curframe].type));
		WriteFrame ( f, curframe); // only single frame supported
		curframe++;
	}

}

/*
==============
WriteSPRFile
==============
*/
void WriteSPRFile (void)
{
	file_t	*f;

	if(sprite.numframes == 0) Sys_Error ("no frames\n");
	if((plump - lumpbuffer) > MAX_BUFFER_SIZE)
		Sys_Error ("Can't write %s, sprite package too big", spriteoutname );

	f = FS_Open(spriteoutname, "wb", true, false );
	Msg("writing %s\n", spriteoutname);
	WriteSprite( f );
	FS_Close( f );
	
	Msg("%d frame%s\n", sprite.numframes, sprite.numframes > 1 ? "s" : "" );
	spriteoutname[0] = 0;// clear for a new sprite
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
	dspriteframe_t	*pframe;
	int x, y, w, h, pix;
	byte *screen_p;

	FS_DefaultExtension( name, ".bmp" );

	byteimage = ReadBMP (name, &lbmpalette, &byteimagewidth, &byteimageheight);
	if(!byteimage) Sys_Error( "unable to load file \"%s\"\n", name );

	if(sprite.numframes == 0) memcpy( origpalette, lbmpalette, sizeof( origpalette ));
	else if (memcmp( origpalette, lbmpalette, sizeof( origpalette )))
		Msg("Warning: %s doesn't share a pallette with the previous frame\n", name );	
          
	w = byteimagewidth;
	h = byteimageheight;

	if ((w > MAX_FRAME_DIM) || (h > MAX_FRAME_DIM))
		Sys_Error ("Sprite has a dimension longer than %i", MAX_FRAME_DIM);

	pframe = (dspriteframe_t *)plump;
	frames[framecount].pdata = pframe;
	frames[framecount].type = 0;//SPR_SINGLE

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
	sprite.numframes++;
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

/*
==============
Cmd_SpriteUnknown

syntax: "blabla"
==============
*/
void Cmd_SpriteUnknown( void )
{
	MsgDev("Warning: bad command %s\n", token);
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

	if (!lumpbuffer )lumpbuffer = Malloc((MAX_BUFFER_SIZE) * 2); // *2 for padding

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

		if (SC_MatchToken( "$spritename" )) Cmd_Spritename();
		else if (SC_MatchToken( "$framerate" )) Cmd_Framerate();
		else if (SC_MatchToken( "$texture" )) Cmd_Texture();
		else if (SC_MatchToken( "$origin" )) Cmd_Offset();
		else if (SC_MatchToken( "$color" )) Cmd_Color();
		else if (SC_MatchToken( "$load" )) Cmd_Load();
		else if (SC_MatchToken( "$type" )) Cmd_Type();
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

	memset(frames, 0, sizeof(frames));
	if(lumpbuffer) Free( lumpbuffer );
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

	MsgDev("Spritegen. Ver: 0.2\n");
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
	return false;
}