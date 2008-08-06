//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	sprlib.c - sprite generator
//=======================================================================

#include "platform.h"
#include "byteorder.h"
#include "utils.h"
#include "mathlib.h"

#define MAX_FRAMES		512
#define MAX_FRAME_DIM	512
#define MIN_INTERVAL	0.001f
#define MAX_INTERVAL	64.0f

dsprite_t	sprite;
byte	*spritepool;
byte	*sprite_pal;
byte	*frame_buffer;
int	frame_width;
int	frame_height;
char	spriteoutname[MAX_SYSPATH];
float	frameinterval;
int	framecount;
int	origin_x;
int	origin_y;

struct
{
	frametype_t	type;		// single frame or group of frames
	void		*pdata;		// either a dspriteframe_t or group info
	float		interval;		// only used for frames in groups
	int		numgroupframes;	// only used by group headers
} frames[MAX_FRAMES];

/*
============
WriteFrame
============
*/
void WriteFrame( file_t *f, int framenum )
{
	dframe_t		*pframe;

	pframe = (dframe_t *)frames[framenum].pdata;
	pframe->origin[0] = LittleLong( pframe->origin[0] );
	pframe->origin[1] = LittleLong( pframe->origin[1] );
	pframe->width = LittleLong (pframe->width);
	pframe->height = LittleLong (pframe->height);

	// write frame as 32-bit indexed image
	FS_Write(f, pframe, sizeof(*pframe));
	FS_Write(f, (byte *)(pframe + 1), pframe->height * pframe->width );
}

/*
============
WriteSprite
============
*/
void WriteSprite( file_t *f )
{
	int	i;
	short	cnt = 256;
	int	curframe = 0;
	int	groupframe = 0;

	// calculate bounding radius
	sprite.boundingradius = sqrt(((sprite.bounds[0]>>1) * (sprite.bounds[0]>>1))
		+ ((sprite.bounds[1]>>1) * (sprite.bounds[1]>>1)));

	// write out the sprite header
	SwapBlock((int *)&sprite, sizeof(dsprite_t));
	FS_Write( f, &sprite, sizeof(sprite));

	// write out palette (768 bytes)
	FS_Write( f, (void *)&cnt, sizeof(cnt));
	FS_Write( f, sprite_pal, cnt * 3 );

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
			FS_Write( f, &dsgroup, sizeof(dsgroup));
			totinterval = 0.0f; // write the interval array

			for( j = 0; j < numframes; j++ )
			{
				dspriteinterval_t	temp;

				totinterval += frames[groupframe+1+j].interval;
				temp.interval = LittleFloat(totinterval);
				FS_Write(f, &temp, sizeof(temp));
			}
			for( j = 0; j < numframes; j++ )
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
bool WriteSPRFile( void )
{
	file_t	*f;
	uint	i, groups = 0, grpframes = 0, sngframes = framecount;

	if( sprite.numframes == 0 ) 
	{
		MsgDev(D_WARN, "WriteSPRFile: ignoring blank sprite %s\n", spriteoutname );
		return false;
	}
	f = FS_Open( spriteoutname, "wb" );
	Msg("writing %s\n", spriteoutname );
	WriteSprite( f );
	FS_Close( f );

	// release framebuffer
	for( i = 0; i < framecount; i++)
	{
		if( frames[i].pdata ) Mem_Free( frames[i].pdata );
		if( frames[i].numgroupframes ) 
		{
			groups++;
			sngframes -= frames[i].numgroupframes;
			grpframes += frames[i].numgroupframes;
		}
	}

	// display info about current sprite
	if( groups )
	{
		Msg("%d group%s,", groups, groups > 1 ? "s":"" );
		Msg(" contain %d frame%s\n", grpframes, grpframes > 1 ? "s":"" );
	}
	if( sngframes - groups )
		Msg("%d ungrouped frame%s\n", sngframes - groups, (sngframes - groups) > 1 ? "s" : "" );	
	return true;
}

/*
===============
Cmd_Type

syntax: "$type preset"
===============
*/
void Cmd_Type( void )
{
	Com_GetToken (false);

	if (Com_MatchToken( "vp_parallel_upright" )) sprite.type = SPR_FWD_PARALLEL_UPRIGHT;
	else if (Com_MatchToken( "facing_upright" )) sprite.type = SPR_FACING_UPRIGHT;
	else if (Com_MatchToken( "vp_parallel" )) sprite.type = SPR_FWD_PARALLEL;
	else if (Com_MatchToken( "oriented" )) sprite.type = SPR_ORIENTED;
	else if (Com_MatchToken( "vp_parallel_oriented")) sprite.type = SPR_FWD_PARALLEL_ORIENTED;
	else sprite.type = SPR_FWD_PARALLEL; // default
}

/*
===============
Cmd_RenderMode

syntax: "$rendermode preset"
===============
*/
void Cmd_RenderMode( void )
{
	Com_GetToken( false );

	if (Com_MatchToken( "additive")) sprite.texFormat = SPR_ADDITIVE;
	else if (Com_MatchToken( "normal")) sprite.texFormat = SPR_NORMAL;
	else if (Com_MatchToken( "indexalpha")) sprite.texFormat = SPR_INDEXALPHA;
	else if (Com_MatchToken( "alphatest")) sprite.texFormat = SPR_ALPHTEST;
	else if (Com_MatchToken( "glow")) sprite.texFormat = SPR_ADDGLOW;
	else sprite.texFormat = SPR_ADDITIVE; // default
}

/*
==============
Cmd_FaceType

syntax: "$facetype"
==============
*/
void Cmd_FaceType( void )
{
	Com_GetToken( false );

	if (Com_MatchToken( "normal")) sprite.facetype = SPR_SINGLE_FACE;
	else if (Com_MatchToken( "twoside")) sprite.facetype = SPR_DOUBLE_FACE;
	else if (Com_MatchToken( "xcross")) sprite.facetype = SPR_XCROSS_FACE;
	else sprite.facetype = SPR_SINGLE_FACE; // default
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
void Cmd_Load( void )
{
	char		*framename;
	static byte	base_pal[256*3];

	framename = Com_GetToken( false );
	FS_DefaultExtension( framename, ".bmp" );
	frame_buffer = ReadBMP( framename, &sprite_pal, &frame_width, &frame_height );

	if( sprite.numframes == 0 ) Mem_Copy( base_pal, sprite_pal, sizeof( base_pal ));
	else if( memcmp( base_pal, sprite_pal, sizeof( base_pal )))
		MsgDev( D_WARN, "Cmd_Load: %s doesn't share a pallette with the previous frame\n", framename );

	if(Com_TryToken())
	{
 		uint	line = frame_width;
		byte	*fout, *fin;
		int	x, y;

		fin = frame_buffer;
		fout = Mem_Alloc( zonepool, frame_width * frame_height );
			
		if(Com_MatchToken("flip_x"))
		{
			for( y = 0; y < frame_height; y++ )
				for( x = frame_width - 1; x >= 0; x--)
					fout[y*line+x] = *fin;
		}
		else if(Com_MatchToken("flip_y"))
		{
			for( y = frame_height - 1; y >= 0; y-- )
				for( x = 0; x < frame_width; x++)
					fout[y*line+x] = *fin;
		}
		Mem_Free( frame_buffer );
		frame_buffer = fout;
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
	int		x, y, xl, yl, xh, yh, w, h;
	int		pixels, linedelta;
	dframe_t		*pframe;
	byte		*fin, *plump;

	if( !frame_buffer ) Sys_Break( "frame not loaded\n" );
	if( framecount >= MAX_FRAMES ) Sys_Break( "too many frames in package\n" );
	pixels = frame_width * frame_height;
	xl = com.atoi(Com_GetToken(false));
	yl = com.atoi(Com_GetToken(false));
	w  = com.atoi(Com_GetToken(false));
	h  = com.atoi(Com_GetToken(false));

	if((xl & 0x07)||(yl & 0x07)||(w & 0x07)||(h & 0x07))
	{
		// render will be resampled image, just throw warning
		MsgDev( D_WARN, "frame dimensions not multiples of 8\n" );
	}
	if((w > MAX_FRAME_DIM) || (h > MAX_FRAME_DIM))
		Sys_Break( "sprite has a dimension longer than %d\n", MAX_FRAME_DIM );

	if((w > frame_width) || (h > frame_height))
		Sys_Break("frame size [%ix%i] longer than image [%ix%i]\n", w, h, frame_width, frame_height );

	xh = xl + w;
	yh = yl + h;

	plump = (byte *)Mem_Alloc( spritepool, sizeof(dframe_t) + (w * h));
	pframe = (dframe_t *)plump;
	frames[framecount].pdata = plump;
	frames[framecount].type = SPR_SINGLE;

	// get interval
	if( Com_TryToken()) 
	{
		frames[framecount].interval = bound(MIN_INTERVAL, com.atof(com_token), MAX_INTERVAL );

	}
	else if( frameinterval != 0 )
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
	if(w > sprite.bounds[0]) sprite.bounds[0] = w;
	if(h > sprite.bounds[1]) sprite.bounds[1] = h;

	plump = (byte *)(pframe + 1); // move pointer
	fin = frame_buffer + yl * frame_width + xl;
	linedelta = frame_width - w;

	for( y = yl; y < yh; y++ )
	{
		for( x = xl; x < xh; x++ )
			*plump++ = *fin++;
		fin += linedelta;
	}
	framecount++;
}

/*
==============
Cmd_SpriteUnknown

syntax: "blabla"
==============
*/
void Cmd_SpriteUnknown( void )
{
	MsgDev( D_WARN, "Cmd_SpriteUnknown: bad command %s\n", com_token);
	while(Com_TryToken());
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
		if(!Com_GetToken(true)) 
		{
			if( is_started ) Sys_Break("missing }\n");
			break;
                    }
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
		else Cmd_SpriteUnknown(); // skip unknown commands
	}
	if( frames[groupframe].numgroupframes == 0 ) 
	{
		// don't create blank groups, rewind frames
		framecount--, sprite.numframes--;
		MsgDev( D_WARN, "Cmd_Group: remove blank group\n" );
	}
	else if( angled && frames[groupframe].numgroupframes != 8 ) 
	{
		// don't create blank groups, rewind frames
		framecount--, sprite.numframes--;
		MsgDev(D_WARN, "Cmd_Group: Remove angled group with invalid framecount\n" );
	}
}

/*
===============
Cmd_Origin

syntax: $origin "x_pos y_pos"
===============
*/
static void Cmd_Origin( void )
{
	origin_x = com.atoi(Com_GetToken (false));
	origin_y = com.atoi(Com_GetToken (false));
}


/*
===============
Cmd_Rand

syntax: $rand
===============
*/
static void Cmd_Rand( void )
{
	sprite.synctype = ST_RAND;
}

/*
==============
Cmd_Spritename

syntax: "$spritename outname"
==============
*/
void Cmd_Spritename (void)
{
	com.strcpy( spriteoutname, Com_GetToken(false));
	FS_DefaultExtension( spriteoutname, ".spr" );
}

void ResetSpriteInfo( void )
{
	// set default sprite parms
	spriteoutname[0] = 0;
	FS_FileBase(gs_filename, spriteoutname );
	FS_DefaultExtension( spriteoutname, ".spr" );

	memset (&sprite, 0, sizeof(sprite));
	memset(frames, 0, sizeof(frames));
	framecount = origin_x = origin_y = 0;
	frameinterval = 0.0f;

	sprite.bounds[0] = -9999;
	sprite.bounds[1] = -9999;
	sprite.ident = IDSPRITEHEADER;
	sprite.version = SPRITE_VERSION;
	sprite.type = SPR_FWD_PARALLEL;
	sprite.facetype = SPR_SINGLE_FACE;
	sprite.synctype = ST_SYNC;
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
		else if (Com_MatchToken( "$texture" )) Cmd_RenderMode();
		else if (Com_MatchToken( "$facetype" )) Cmd_FaceType();
		else if (Com_MatchToken( "$origin" )) Cmd_Origin();
		else if (Com_MatchToken( "$rand" )) Cmd_Rand();
		else if (Com_MatchToken( "$load" )) Cmd_Load();
		else if (Com_MatchToken( "$type" )) Cmd_Type();
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
	
	if( load )
	{
		if(!ParseSpriteScript())
			return false;
		return WriteSPRFile();
	}

	Msg("%s not found\n", gs_filename );
	return false;
}

bool CompileSpriteModel ( byte *mempool, const char *name, byte parms )
{
	if( mempool ) spritepool = mempool;
	else
	{
		MsgDev( D_ERROR, "can't allocate memory pool.\nAbort compilation\n");
		return false;
	}
	return CompileCurrentSprite( name );	
}