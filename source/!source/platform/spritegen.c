//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	sprlib.c - sprite generator
//=======================================================================

#include "platform.h"
#include <basemath.h>
#include "baseutils.h"
#include <sprite.h>

#define MAX_BUFFER_SIZE	0x100000
#define MAX_FRAMES		1000
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

spritepackage_t	frames[MAX_FRAMES];

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
	spritetemp.beamlength = LittleFloat (sprite.beamlength);
	spritetemp.synctype = LittleFloat (sprite.synctype);
	spritetemp.version = LittleLong (SPRITE_VERSION);
	spritetemp.ident = LittleLong (IDSPRITEHEADER);

	FS_Write(spriteouthandle, &spritetemp, sizeof(spritetemp));

	// Write out palette in 16bit mode
	FS_Write( spriteouthandle, (void *) &cnt, sizeof(cnt) );
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
===============
Cmd_Type
===============
*/
void Cmd_Type (void)
{
	SC_GetToken (false);
	if (SC_MatchToken( "vp_parallel_upright" ))	sprite.type = SPR_VP_PARALLEL_UPRIGHT;
	else if (SC_MatchToken( "facing_upright" ))	sprite.type = SPR_FACING_UPRIGHT;
	else if (SC_MatchToken( "vp_parallel" ))	sprite.type = SPR_VP_PARALLEL;
	else if (SC_MatchToken( "oriented" ))		sprite.type = SPR_ORIENTED;
	else if (SC_MatchToken( "vp_parallel_oriented"))	sprite.type = SPR_VP_PARALLEL_ORIENTED;
	else sprite.type = SPR_VP_PARALLEL; //default
}


/*
===============
Cmd_Texture
===============
*/
void Cmd_Texture (void)
{
	SC_GetToken (false);

	if (SC_MatchToken( "additive"))		sprite.texFormat = SPR_ADDITIVE;
	else if (SC_MatchToken( "normal"))		sprite.texFormat = SPR_NORMAL;
	else if (SC_MatchToken( "indexalpha"))		sprite.texFormat = SPR_INDEXALPHA;
	else if (SC_MatchToken( "alphatest"))		sprite.texFormat = SPR_ALPHTEST;
	else sprite.texFormat = SPR_NORMAL;
}

/*
===============
Cmd_Beamlength
===============
*/
void Cmd_Beamlength ()
{
	sprite.beamlength = atof (SC_GetToken (false));
}

/*
===============
Cmd_Load
===============
*/
void Cmd_Load (void)
{
	static byte origpalette[256*3];
          char *name = SC_GetToken (false);
	
	FS_DefaultExtension( name, ".bmp" );
	byteimage = ReadBMP (name, &lbmpalette, &byteimagewidth, &byteimageheight);
	if(!byteimage) Sys_Error( "unable to load file \"%s\"\n", name );

	if(sprite.numframes == 0) memcpy( origpalette, lbmpalette, sizeof( origpalette ));
	else if (memcmp( origpalette, lbmpalette, sizeof( origpalette )) != 0)
		Sys_Error( "bitmap \"%s\" doesn't share a pallette with the previous bitmap\n", name );	
}

/*
===============
Cmd_Offset
===============
*/

void Cmd_Offset (void)
{
	origin_x = atoi(SC_GetToken (false));
	origin_y = atoi(SC_GetToken (false));
}

void Cmd_Sync( void )
{
	sprite.synctype = ST_SYNC;
}


/*
===============
Cmd_Frame
===============
*/
void Cmd_Frame ()
{
	int		x,y,xl,yl,xh,yh,w,h;
	byte		*screen_p, *source;
	int		linedelta;
	dspriteframe_t	*pframe;
	int		pix;
	
	xl = atoi(SC_GetToken (false));
	yl = atoi(SC_GetToken (false));
	w  = atoi(SC_GetToken (false));
	h  = atoi(SC_GetToken (false));

	if ((xl & 0x07) || (yl & 0x07) || (w & 0x07) || (h & 0x07))
		Sys_Error ("Sprite dimensions not multiples of 8\n");

	if ((w > MAX_FRAME_DIM) || (h > MAX_FRAME_DIM))
		Sys_Error ("Sprite has a dimension longer than 256");

	xh = xl+w;
	yh = yl+h;

	pframe = (dspriteframe_t *)plump;
	frames[framecount].pdata = pframe;
	frames[framecount].type = SPR_SINGLE;

	if (SC_TryToken())
	{
		frames[framecount].interval = atof (token);
		if (frames[framecount].interval <= 0.0)
			Sys_Error ("Non-positive interval");
	}
	else frames[framecount].interval = (float)0.1;
	
	if (SC_TryToken())
	{
		pframe->origin[0] = -atoi (token);
		pframe->origin[1] = atoi(SC_GetToken(false));
	}
	else if((origin_x != 0) && (origin_y != 0))
	{
		//write shared origin
		pframe->origin[0] = -origin_x;
		pframe->origin[1] = origin_y;
	}
	else
	{
		pframe->origin[0] = -(w >> 1);
		pframe->origin[1] = h >> 1;
	}

	pframe->width = w;
	pframe->height = h;

	if (w > framesmaxs[0]) framesmaxs[0] = w;
	if (h > framesmaxs[1]) framesmaxs[1] = h;
	
	plump = (byte *)(pframe + 1);
	screen_p = byteimage + yl * byteimagewidth + xl;
	linedelta = byteimagewidth - w;

	source = plump;

	for (y = yl; y < yh; y++)
	{
		for (x = xl; x < xh; x++)
		{
			pix = *screen_p;
			*screen_p++ = 0;
			*plump++ = pix;
		}
		screen_p += linedelta;
	}

	framecount++;
	if (framecount >= MAX_FRAMES) Sys_Error ("Too many frames; increase MAX_FRAMES\n");
}

/*
===============
Cmd_GroupStart	
===============
*/
void Cmd_GroupStart (void)
{
	int	groupframe;

	groupframe = framecount++;

	frames[groupframe].type = SPR_GROUP;
	frames[groupframe].numgroupframes = 0;

	while (1)
	{
		if(!SC_GetToken (true)) Sys_Error ("End of file during group");

		if (SC_MatchToken( "$frame" ))
		{
			Cmd_Frame ();
			frames[groupframe].numgroupframes++;
		}
		else if (SC_MatchToken( "$load" ))
		{
			Cmd_Load ();
		}
		else if (SC_MatchToken( "$groupend" ))
		{
			break;
		}
		else Sys_Error ("$frame, $load, or $groupend expected\n");
	}

	if (frames[groupframe].numgroupframes == 0) Sys_Error ("Empty group\n");
}

/*
==============
Cmd_Spritename
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
	sprite.synctype = ST_RAND; // default
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
	
		if (SC_MatchToken( "$load" )) Cmd_Load();
		else if (SC_MatchToken( "$spritename" )) Cmd_Spritename();
		else if (SC_MatchToken( "$type" )) Cmd_Type();
		else if (SC_MatchToken( "$texture" )) Cmd_Texture();
		else if (SC_MatchToken( "$origin" )) Cmd_Offset();
		else if (SC_MatchToken( "$beamlength" )) Cmd_Beamlength();
		else if (SC_MatchToken( "$sync" )) Cmd_Sync();
		else if (SC_MatchToken( "$frame" ))
		{
			Cmd_Frame ();
			sprite.numframes++;
		}		
		else if (SC_MatchToken( "$groupstart" ))
		{
			Cmd_GroupStart ();
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

	return false;
}