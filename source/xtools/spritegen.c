//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    	sprlib.c - sprite generator
//=======================================================================

#include "xtools.h"
#include "byteorder.h"
#include "utils.h"
#include "mathlib.h"

#define MAX_FRAMES		256
#define MAX_FRAME_DIM	512
#define MIN_INTERVAL	0.001f
#define MAX_INTERVAL	64.0f

dsprite_t	sprite;
byte	*spritepool;
byte	*sprite_pal;
rgbdata_t	*frame = NULL;
script_t	*spriteqc = NULL;
string	spriteoutname;
float	frameinterval;
int	framecount;
int	origin_x;
int	origin_y;
bool	need_resample;
bool	ignore_resample;
int	resample_w;
int	resample_h;

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
	dspriteframe_t		*pframe;

	pframe = (dspriteframe_t *)frames[framenum].pdata;
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

	Com_CloseScript( spriteqc );
	if( sprite.numframes == 0 ) 
	{
		MsgDev(D_WARN, "WriteSPRFile: ignoring blank sprite %s\n", spriteoutname );
		return false;
	}

	f = FS_Open( spriteoutname, "wb" );
	Msg( "writing %s\n", spriteoutname );
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
	token_t	token;

	Com_ReadToken( spriteqc, 0, &token );

	if( !com.stricmp( token.string, "vp_parallel_upright" )) sprite.type = SPR_FWD_PARALLEL_UPRIGHT;
	else if( !com.stricmp( token.string, "facing_upright" )) sprite.type = SPR_FACING_UPRIGHT;
	else if( !com.stricmp( token.string, "vp_parallel" )) sprite.type = SPR_FWD_PARALLEL;
	else if( !com.stricmp( token.string, "oriented" )) sprite.type = SPR_ORIENTED;
	else if( !com.stricmp( token.string, "vp_parallel_oriented")) sprite.type = SPR_FWD_PARALLEL_ORIENTED;
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
	token_t	token;

	Com_ReadToken( spriteqc, 0, &token );

	if( !com.stricmp( token.string, "additive")) sprite.texFormat = SPR_ADDITIVE;
	else if( !com.stricmp( token.string, "normal")) sprite.texFormat = SPR_NORMAL;
	else if( !com.stricmp( token.string, "indexalpha")) sprite.texFormat = SPR_INDEXALPHA;
	else if( !com.stricmp( token.string, "alphatest")) sprite.texFormat = SPR_ALPHTEST;
	else if( !com.stricmp( token.string, "glow")) sprite.texFormat = SPR_ADDGLOW;
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
	token_t	token;

	Com_ReadToken( spriteqc, 0, &token );

	if( !com.stricmp( token.string, "normal")) sprite.facetype = SPR_SINGLE_FACE;
	else if( !com.stricmp( token.string, "twoside")) sprite.facetype = SPR_DOUBLE_FACE;
	else if( !com.stricmp( token.string, "xcross")) sprite.facetype = SPR_XCROSS_FACE;
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
	float	framerate;

	Com_ReadFloat( spriteqc, false, &framerate );
	if( framerate <= 0.0f ) return; // negative framerate not allowed
	frameinterval = bound( MIN_INTERVAL, (1.0f/framerate), MAX_INTERVAL );	
}

/*
===============
Cmd_Resample

syntax: "$resample <w h>"
===============
*/
void Cmd_Resample( void )
{
	if(Com_ReadUlong( spriteqc, false, &resample_w ))
		Com_ReadUlong( spriteqc, false, &resample_h );
	else resample_w = resample_h = 0; // Image_Resample will be found nearest pow2
	if( !ignore_resample ) need_resample = true;
}

/*
===============
Cmd_NoResample

syntax: "$noresample"
===============
*/
void Cmd_NoResample( void )
{
	ignore_resample = true;
}

/*
===============
Cmd_Load

syntax "$load fire01.bmp"
===============
*/
void Cmd_Load( void )
{
	string		framename;
	static byte	base_pal[256*3];
	int		flags = 0;
	token_t		token;

	Com_ReadString( spriteqc, false, framename );

	if( frame ) FS_FreeImage( frame );
	frame = FS_LoadImage( framename, error_bmp, error_bmp_size );
	if( !frame ) Sys_Break( "unable to load %s\n", framename ); // no error.bmp, missing frame...
	Image_Process( &frame, 0, 0, IMAGE_PALTO24 ); 
	if( sprite.numframes == 0 ) Mem_Copy( base_pal, frame->palette, sizeof( base_pal ));
	else if( memcmp( base_pal, frame->palette, sizeof( base_pal )))
		MsgDev( D_WARN, "Cmd_Load: %s doesn't share a pallette with the previous frame\n", framename );
	sprite_pal = (byte *)(&base_pal[0]);

	Msg( "grabbing %s\n", framename );
	while( Com_ReadToken( spriteqc, 0, &token ))
	{
		if( !com.stricmp(token.string, "flip_diagonal")) flags |= IMAGE_ROT_90;
		else if( !com.stricmp( token.string, "flip_y" )) flags |= IMAGE_FLIP_Y;
		else if( !com.stricmp( token.string, "flip_x" )) flags |= IMAGE_FLIP_X;
	}

	// apply changes
	Image_Process( &frame, 0, 0, flags ); 
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
	int		org_x, org_y;
	int		pixels, linedelta;
	bool		resampled = false;
	dspriteframe_t		*pframe;
	byte		*fin, *plump;

	if( !frame || !frame->buffer ) Sys_Break( "frame not loaded\n" );
	if( framecount >= MAX_FRAMES ) Sys_Break( "too many frames in package\n" );
	pixels = frame->width * frame->height;

	Com_ReadLong( spriteqc, false, &xl );
	Com_ReadLong( spriteqc, false, &yl );
	Com_ReadLong( spriteqc, false, &w );
	Com_ReadLong( spriteqc, false, &h );

	if((xl & 0x07)||(yl & 0x07)||(w & 0x07)||(h & 0x07))
	{
		if( need_resample )
		{
			Image_Process( &frame, resample_w, resample_h, IMAGE_RESAMPLE ); 
			if( resample_w == frame->width && resample_h == frame->height )
				resampled = true;
		}
		MsgDev( D_NOTE, "frame dimensions not multiples of 8\n" );
	}
	if((w > MAX_FRAME_DIM) || (h > MAX_FRAME_DIM))
		Sys_Break( "sprite has a dimension longer than %d\n", MAX_FRAME_DIM );

	// get interval
	if( Com_ReadFloat( spriteqc, false, &frames[framecount].interval )) 
	{
		frames[framecount].interval = bound( MIN_INTERVAL, frames[framecount].interval, MAX_INTERVAL );

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

	if(Com_ReadLong( spriteqc, false, &org_x ))
	{
		org_x = -org_x; // inverse x coord
		Com_ReadLong( spriteqc, false, &org_y );
	}
	else if((origin_x != 0) && (origin_y != 0))
	{
		// write shared origin
		org_x = -origin_x;
		org_y = origin_y;
	}
	else
	{
		// use center of image
		org_x = -(w>>1);
		org_y = h>>1;
	}

	// merge all sprite info
	if( need_resample && resampled )
	{
		// check for org[n] == size[n] and org[n] == size[n]/2
		// another cases will be not changed
		if( org_x == -w ) org_x = -frame->width;
		else if( org_x == -(w>>1)) org_x = -frame->width>>1;
		if( org_y == h ) org_y = frame->height;
		else if( org_y == (h>>1)) org_y = frame->height>>1;
		w = frame->width;
		h = frame->height;
	}

	xh = xl + w;
	yh = yl + h;

	plump = (byte *)Mem_Alloc( spritepool, sizeof(dspriteframe_t) + (w * h));
	pframe = (dspriteframe_t *)plump;
	frames[framecount].pdata = plump;
	frames[framecount].type = SPR_SINGLE;

	pframe->origin[0] = org_x;
	pframe->origin[1] = org_y;
	pframe->width = w;
	pframe->height = h;

	// adjust maxsize
	if(w > sprite.bounds[0]) sprite.bounds[0] = w;
	if(h > sprite.bounds[1]) sprite.bounds[1] = h;

	plump = (byte *)(pframe + 1); // move pointer
	fin = frame->buffer + yl * frame->width + xl;
	linedelta = frame->width - w;

	// apply scissor to source
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
void Cmd_SpriteUnknown( const char *token )
{
	MsgDev( D_WARN, "Cmd_SpriteUnknown: bad command %s\n", token );
	Com_SkipRestOfLine( spriteqc );
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
	token_t	token;

	groupframe = framecount++;

	frames[groupframe].type = angled ? SPR_ANGLED : SPR_GROUP;
	need_resample = resample_w = resample_h = 0; // invalidate resample for group 
	frames[groupframe].numgroupframes = 0;

	while( 1 )
	{
		if( !Com_ReadToken( spriteqc, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token )) 
		{
			if( is_started ) Sys_Break( "missing }\n" );
			break;
                    }

		if( !com.stricmp( token.string, "{" )) is_started = 1;
		else if( !com.stricmp( token.string, "}" )) break; // end of group
		else if( !com.stricmp( token.string, "$framerate" )) Cmd_Framerate();
		else if( !com.stricmp( token.string, "$resample" )) Cmd_Resample();
		else if( !com.stricmp( token.string, "$frame" ))
		{
			Cmd_Frame();
			frames[groupframe].numgroupframes++;
		}
		else if( !com.stricmp( token.string, "$load" )) Cmd_Load();
		else if( is_started ) Sys_Break( "missing }\n" );
		else Cmd_SpriteUnknown( token.string ); // skip unknown commands
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

	// back to single frames, invalidate resample
	need_resample = resample_w = resample_h = 0;
}

/*
===============
Cmd_Origin

syntax: $origin "x_pos y_pos"
===============
*/
static void Cmd_Origin( void )
{
	Com_ReadLong( spriteqc, false, &origin_x );
	Com_ReadLong( spriteqc, false, &origin_y );
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
void Cmd_Spritename( void )
{
	Com_ReadString( spriteqc, false, spriteoutname );
	FS_DefaultExtension( spriteoutname, ".spr" );
}

void ResetSpriteInfo( void )
{
	// set default sprite parms
	spriteoutname[0] = 0;
	FS_FileBase( gs_filename, spriteoutname );
	FS_DefaultExtension( spriteoutname, ".spr" );

	Mem_Set( &sprite, 0, sizeof( sprite ));
	Mem_Set( frames, 0, sizeof( frames ));
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
bool ParseSpriteScript( void )
{
	token_t	token;

	ResetSpriteInfo();
	
	while( 1 )
	{
		if( !Com_ReadToken( spriteqc, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token )) break;
		if( !com.stricmp( token.string, "$spritename" )) Cmd_Spritename();
		else if( !com.stricmp( token.string, "$noresample" )) Cmd_NoResample();
		else if( !com.stricmp( token.string, "$resample" )) Cmd_Resample();
		else if( !com.stricmp( token.string, "$texture" )) Cmd_RenderMode();
		else if( !com.stricmp( token.string, "$facetype" )) Cmd_FaceType();
		else if( !com.stricmp( token.string, "$origin" )) Cmd_Origin();
		else if( !com.stricmp( token.string, "$rand" )) Cmd_Rand();
		else if( !com.stricmp( token.string, "$load" )) Cmd_Load();
		else if( !com.stricmp( token.string, "$type" )) Cmd_Type();
		else if( !com.stricmp( token.string, "$frame" ))
		{
			Cmd_Frame();
			sprite.numframes++;
		}
		else if( !com.stricmp( token.string, "$group" )) 
		{
			Cmd_Group( false );
			sprite.numframes++;
		}
		else if( !com.stricmp( token.string, "$angled" )) 
		{
			Cmd_Group( true );
			sprite.numframes++;
		}
		else if( !Com_ValidScript( token.string, QC_SPRITEGEN )) return false;
		else Cmd_SpriteUnknown( token.string );
	}
	return true;
}

bool CompileCurrentSprite( const char *name )
{
	if( name ) com.strcpy( gs_filename, name );
	FS_DefaultExtension( gs_filename, ".qc" );
	spriteqc = Com_OpenScript( gs_filename, NULL, 0 );
	
	if( spriteqc )
	{
		if(!ParseSpriteScript())
			return false;
		return WriteSPRFile();
	}

	Msg( "%s not found\n", gs_filename );
	return false;
}

bool CompileSpriteModel( byte *mempool, const char *name, byte parms )
{
	if( mempool ) spritepool = mempool;
	else
	{
		MsgDev( D_ERROR, "can't allocate memory pool.\nAbort compilation\n");
		return false;
	}
	return CompileCurrentSprite( name );	
}