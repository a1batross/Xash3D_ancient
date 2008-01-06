//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	sprlib.c - sprite generator
//=======================================================================

#include "platform.h"
#include "basefiles.h"
#include "mathlib.h"
#include "utils.h"

#define MAX_BUFFER_SIZE	((sizeof(frames) * MAX_FRAMES) + (MAX_FRAME_DIM * 2 * MAX_FRAMES))
#define MAX_FRAMES		512
#define MAX_FRAME_DIM	512
#define MIN_INTERVAL	0.001f
#define MAX_INTERVAL	64.0f

dsprite_t	sprite;
byte	*spritepool;
rgbdata_t	*byteimage;
byte	*lumpbuffer = NULL, *plump;
char	spriteoutname[MAX_SYSPATH];
float	frameinterval;
int	framecount;
int	origin_x;
int	origin_y;

typedef struct
{
	frametype_t	type;		// single frame or group of frames
	void		*pdata;		// either a dspriteframe_t or group info
	float		interval;		// only used for frames in groups
	int		numgroupframes;	// only used by group headers
} spritepackage_t;

spritepackage_t frames[MAX_FRAMES];

/*
============
WriteFrame
============
*/
void WriteFrame( file_t *f, int framenum )
{
	dframe_t		*pframe, frametemp;
	vfile_t		*h = VFS_Open( f, "wz" );
	fs_offset_t	hdrstart, bufstart, bufend;	

	pframe = (dframe_t *)frames[framenum].pdata;
	frametemp.origin[0] = LittleLong (pframe->origin[0]);
	frametemp.origin[1] = LittleLong (pframe->origin[1]);
	frametemp.width = LittleLong (pframe->width);
	frametemp.height = LittleLong (pframe->height);
	frametemp.compsize = 0; // not used

	hdrstart = FS_Tell(f);
	FS_Write(f, &frametemp, sizeof(frametemp));
	bufstart = FS_Tell(f);
	VFS_Write(h, (byte *)(pframe + 1), pframe->height * pframe->width * 4 );
	f = VFS_Close(h); // compress frame buffer
	bufend = FS_Tell(f);
	frametemp.compsize = bufend - bufstart;		// size of compressed frame

	FS_Seek(f, hdrstart, SEEK_SET );
	FS_Write(f, &frametemp, sizeof(frametemp));	// merge header
	FS_Seek(f, bufend, SEEK_SET );		// go to end of frame
}

/*
============
WriteSprite
============
*/
void WriteSprite( file_t *f )
{
	int	i, curframe = 0, groupframe = 0;

	// write out the sprite header
	SwapBlock( (int *)&sprite, sizeof(dsprite_t));
	FS_Write( f, &sprite, sizeof(sprite));

	for (i = 0; i < sprite.numframes; i++)
	{
		FS_Write( f, &frames[curframe].type, sizeof(frames[curframe].type));
		if( frames[curframe].type == SPR_SINGLE )
		{
			// single (non-grouped) frame
			WriteFrame( f, curframe );
			curframe++;
		}
		else // angled or sequence
		{
			int		j, numframes;
			dspritegroup_t	dsgroup;
			float		totinterval;

			groupframe = curframe;
			curframe++;
			numframes = frames[groupframe].numgroupframes;

			// set and write the group header
			dsgroup.numframes = LittleLong( numframes );
			FS_Write(f, &dsgroup, sizeof(dsgroup));
			totinterval = 0.0f; // write the interval array

			for(j = 0; j < numframes; j++)
			{
				dspriteinterval_t	temp;

				totinterval += frames[groupframe+1+j].interval;
				temp.interval = LittleFloat(totinterval);
				FS_Write(f, &temp, sizeof(temp));
			}
			for(j = 0; j < numframes; j++)
			{
				WriteFrame(f, curframe);
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
void WriteSPRFile( void )
{
	file_t	*f;

	if(sprite.numframes == 0) MsgDev(D_WARN, "WriteSPRFile: create blank sprite %s\n", spriteoutname );
	if((plump - lumpbuffer) > MAX_BUFFER_SIZE)
		Sys_Break("Can't write %s, sprite package too big", spriteoutname );

	f = FS_Open( spriteoutname, "wb" );
	Msg("writing %s\n", spriteoutname );
	WriteSprite( f );
	FS_Close( f );

	Msg("%d frame%s\n", sprite.numframes, sprite.numframes > 1 ? "s" : "" );
	Msg("%d ungrouped frame%s, including group headers\n", framecount, framecount > 1 ? "s" : "");	
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
	Com_GetToken (false);

	if (Com_MatchToken( "vp_parallel_upright" )) sprite.type = SPR_VP_PARALLEL_UPRIGHT;
	else if (Com_MatchToken( "facing_upright" )) sprite.type = SPR_FACING_UPRIGHT;
	else if (Com_MatchToken( "vp_parallel" )) sprite.type = SPR_VP_PARALLEL;
	else if (Com_MatchToken( "oriented" )) sprite.type = SPR_ORIENTED;
	else if (Com_MatchToken( "vp_parallel_oriented")) sprite.type = SPR_VP_PARALLEL_ORIENTED;
	else sprite.type = SPR_VP_PARALLEL; // default
}

/*
===============
Cmd_Texture

syntax: "$texture preset"
===============
*/
void Cmd_Texture ( void )
{
	Com_GetToken (false);

	if (Com_MatchToken( "additive")) sprite.rendermode = SPR_ADDITIVE;
	else if (Com_MatchToken( "normal")) sprite.rendermode = SPR_NORMAL;
	else if (Com_MatchToken( "indexalpha")) sprite.rendermode = SPR_INDEXALPHA;
	else if (Com_MatchToken( "alphatest")) sprite.rendermode = SPR_ALPHTEST;
	else if (Com_MatchToken( "glow")) sprite.rendermode = SPR_ADDGLOW;
	else sprite.rendermode = SPR_NORMAL; // default
}

/*
===============
Cmd_Framerate

syntax: "$framerate value"
===============
*/
void Cmd_Framerate( void )
{
	float framerate = com.atof(Com_GetToken(false));
	if(framerate <= 0.0f) return; // negative framerate not allowed
	frameinterval = bound( MIN_INTERVAL, (1.0f/framerate), MAX_INTERVAL );	
}

/*
===============
Cmd_Load

syntax "$load fire01.tga"
===============
*/
void Cmd_Load (void)
{
	char *framename;

	if( byteimage ) FS_FreeImage( byteimage ); // release old image
	framename = Com_GetToken(false);
	FS_DefaultExtension( framename, ".tga" );
	byteimage = FS_LoadImage( framename, NULL, 0 );
	if( !byteimage ) Sys_Break("Error: unable to load \"%s\"\n", framename );

	if(Com_TryToken())
	{
 		uint	line = byteimage->width * 4;
		byte	*fout, *fin;
		int	x, y, c;

		fin = byteimage->buffer;
		fout = Mem_Alloc( zonepool, byteimage->size );
			
		if(Com_MatchToken("flip_x"))
		{
			for (y = 0; y < byteimage->height; y++)
				for (x = byteimage->width - 1; x >= 0; x--)
					for(c = 0; c < 4; c++, fin++)
						fout[y*line+x*4+c] = *fin;
		}
		else if(Com_MatchToken("flip_y"))
		{
			for (y = byteimage->height - 1; y >= 0; y--)
				for (x = 0; x < byteimage->width; x++)
					for(c = 0; c < 4; c++, fin++)
						fout[y*line+x*4+c] = *fin;
		}
		Mem_Free( byteimage->buffer );
		byteimage->buffer = fout;
	}
}

/*
===============
Cmd_Frame

syntax "$frame xoffset yoffset width height <interval> <origin x> <origin y>"
===============
*/
void Cmd_Frame( void )
{
	int		i, x, y, xl, yl, xh, yh, w, h;
	int		pixels, linedelta;
	dframe_t		*pframe;
	byte		*fin;

	if (framecount >= MAX_FRAMES) Sys_Error("Too many frames in package\n");
	if( !byteimage )
	{
		// frame not loaded, just skip arguments
		while(Com_TryToken());
		return;
	}

	pixels = byteimage->width * byteimage->height;
	xl = atoi(Com_GetToken(false));
	yl = atoi(Com_GetToken(false));
	w  = atoi(Com_GetToken(false));
	h  = atoi(Com_GetToken(false));

	if((xl & 0x07)||(yl & 0x07)||(w & 0x07)||(h & 0x07))
	{
		MsgWarn("frame dimensions not multiples of 8\n" );
		//return;
	}
	if ((w > MAX_FRAME_DIM) || (h > MAX_FRAME_DIM))
	{
		w = min(w, MAX_FRAME_DIM);
		h = min(h, MAX_FRAME_DIM);
		Image_Processing( "frame", &byteimage, w, h);
	}

	xh = xl + w;
	yh = yl + h;

	pframe = (dframe_t *)plump;
	frames[framecount].pdata = pframe;
	frames[framecount].type = SPR_SINGLE;

	// get interval
	if(Com_TryToken()) 
	{
		frames[framecount].interval = bound(MIN_INTERVAL, com.atof(com_token), MAX_INTERVAL );

	}
	else if(frameinterval != 0)
	{
		frames[framecount].interval = frameinterval;
	}
	else
	{
		// use default interval
		frames[framecount].interval = (float)0.05f;
	} 
	
	if(Com_TryToken())
	{
		pframe->origin[0] = -com.atoi(com_token);
		pframe->origin[1] = com.atoi(Com_GetToken(false));
	}
	else if((origin_x != 0) && (origin_y != 0))
	{
		// write shared origin
		pframe->origin[0] = -origin_x;
		pframe->origin[1] = origin_y;
	}
	else
	{
		// use center of image
		pframe->origin[0] = -(w>>1);
		pframe->origin[1] = h>>1;
	}

	pframe->width = w;
	pframe->height = h;

	// adjust maxsize
	if (w > sprite.width) sprite.width = w;
	if (h > sprite.height) sprite.height = h;

	plump = (byte *)(pframe + 1); // move pointer
	fin = byteimage->buffer + yl * byteimage->width + xl;
	linedelta = byteimage->width - w;

	for (y = yl; y < yh; y++)
	{
		for( x = xl; x < xh; x++ )
			for( i = 0; i < 4; i++)
				*plump++ = *fin++;
		fin += linedelta * 4;
	}
	framecount++;
}

/*
===============
Cmd_Group

syntax: 
$group or $angled
{
	$load fire01.bmp
	$frame xoffset yoffset width height <interval> <origin x> <origin y>
	$load fire02.bmp
	$frame xoffset yoffset width height <interval> <origin x> <origin y>"
	$load fire03.bmp
	$frame xoffset yoffset width height <interval> <origin x> <origin y>
}	
===============
*/
void Cmd_Group( bool angled )
{
	int	groupframe;
	int	is_started = 0;

	groupframe = framecount++;

	frames[groupframe].type = angled ? SPR_ANGLED : SPR_GROUP;
	frames[groupframe].numgroupframes = 0;

	while( 1 )
	{
		if(!Com_GetToken(true)) Sys_Error("End of file during group\n");

		if(Com_MatchToken( "{" )) is_started = 1;
		else if(Com_MatchToken( "}" )) break; // end of group
		else if(Com_MatchToken( "$framerate" )) Cmd_Framerate();
		else if(Com_MatchToken("$frame"))
		{
			Cmd_Frame();
			frames[groupframe].numgroupframes++;
		}
		else if(Com_MatchToken("$load" )) Cmd_Load();
		else if(is_started) Sys_Break("missing }\n");
		else Sys_Break("$frame or $load expected\n");
	}
	if( frames[groupframe].numgroupframes == 0 ) MsgDev(D_WARN, "Cmd_Group: create blank group\n");
}

/*
===============
Cmd_Offset

syntax: $origin "x_pos y_pos"
===============
*/
void Cmd_Offset (void)
{
	origin_x = atoi(Com_GetToken (false));
	origin_y = atoi(Com_GetToken (false));
}

/*
==============
Cmd_Sync

syntax: "$sync"
==============
*/
void Cmd_Sync( void )
{
	sprite.synctype = ST_SYNC;
}

/*
==============
Cmd_Spritename

syntax: "$spritename outname"
==============
*/
void Cmd_Spritename (void)
{
	strcpy( spriteoutname, Com_GetToken(false));
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
	MsgWarn("Cmd_SpriteUnknown: bad command %s\n", com_token);
	while(Com_TryToken());
}

void ResetSpriteInfo( void )
{
	//set default sprite parms
	FS_FileBase(gs_filename, spriteoutname );//kill path and ext
	FS_DefaultExtension( spriteoutname, ".spr" );//set new ext

	memset (&sprite, 0, sizeof(sprite));
	memset(frames, 0, sizeof(frames));
	framecount = origin_x = origin_y = 0;
	frameinterval = 0.0f;

	if (!lumpbuffer ) lumpbuffer = Mem_Alloc( spritepool, (MAX_BUFFER_SIZE) * 2); // *2 for padding

	plump = lumpbuffer;
	sprite.width = -9999999;
	sprite.height = -9999999;
	sprite.ident = IDSPRITEHEADER;
	sprite.version = SPRITE_VERSION;
	sprite.type = SPR_VP_PARALLEL;
	sprite.synctype = ST_RAND; // default
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
		if(!Com_GetToken (true)) break;
		if (Com_MatchToken( "$spritename" )) Cmd_Spritename();
		else if (Com_MatchToken( "$texture" )) Cmd_Texture();
		else if (Com_MatchToken( "$origin" )) Cmd_Offset();
		else if (Com_MatchToken( "$load" )) Cmd_Load();
		else if (Com_MatchToken( "$type" )) Cmd_Type();
		else if (Com_MatchToken( "$sync" )) Cmd_Sync();
		else if (Com_MatchToken( "$frame" ))
		{
			Cmd_Frame();
			sprite.numframes++;
		}
		else if (Com_MatchToken( "$group" )) 
		{
			Cmd_Group( false );
			sprite.numframes++;
		}
		else if (Com_MatchToken( "$angled" )) 
		{
			Cmd_Group( true );
			sprite.numframes++;
		}
		else if (!Com_ValidScript( QC_SPRITEGEN )) return false;
		else Cmd_SpriteUnknown();
	}
	return true;
}

bool CompileCurrentSprite( const char *name )
{
	bool load = false;
	
	if(name) strcpy( gs_filename, name );
	FS_DefaultExtension( gs_filename, ".qc" );
	load = Com_LoadScript( gs_filename, NULL, 0 );
	
	if(load)
	{
		if(!ParseSpriteScript())
			return false;
		WriteSPRFile();
		return true;
	}

	Msg("%s not found\n", gs_filename );
	return false;
}

bool CompileSpriteModel ( byte *mempool, const char *name, byte parms )
{
	if(mempool) spritepool = mempool;
	else
	{
		MsgWarn("Spritegen: can't allocate memory pool.\nAbort compilation\n");
		return false;
	}
	return CompileCurrentSprite( name );	
}