//=======================================================================
//			Copyright XashXT Group 2008 ©
//		  conv_sprite.c - convert q1\q2\hl\spr32 sprites
//=======================================================================

#include "ripper.h"
#include "byteorder.h"

// sprite_decompiler.c
#define SPR_VP_PARALLEL_UPRIGHT	0
#define SPR_FACING_UPRIGHT		1
#define SPR_VP_PARALLEL		2
#define SPR_ORIENTED		3	// all axis are valid
#define SPR_VP_PARALLEL_ORIENTED	4

#define SPR_NORMAL			0	// solid sprite
#define SPR_ADDITIVE		1
#define SPR_INDEXALPHA		2
#define SPR_ALPHTEST		3
#define SPR_ADDGLOW			4	// same as additive, but without depthtest

typedef struct frame_s
{
	char	name[64];		// framename

	int	width;		// lumpwidth
	int	height;		// lumpheight
	int	origin[2];	// monster origin
} frame_t;

typedef struct group_s
{
	frame_t	frame[64];	// max groupframes
	float	interval[64];	// frame intervals
	int	numframes;	// num group frames;
} group_t;

struct qcsprite_s
{
	group_t	group[8];		// 64 frames for each group
	frame_t	frame[512];	// or 512 ungroupped frames

	int	type;		// rendering type
	int	texFormat;	// half-life texture
	bool	truecolor;	// spr32
	byte	palette[256][4];	// shared palette

	int	numgroup;		// groups counter
	int	totalframes;	// including group frames
} spr;

/*
========================================================================

.SPR sprite file format

========================================================================
*/
#define IDSPRQ1HEADER		(('P'<<24)+('S'<<16)+('D'<<8)+'I')	// little-endian "IDSP"

#define SPRITEQ1_VERSION		1
#define SPRITEHL_VERSION		2
#define SPRITE32_VERSION		32

typedef struct
{
	int		ident;
	int		version;
	int		type;
	int		boundingradius;
	int		width;
	int		height;
	int		numframes;
	float		beamlength;
	synctype_t	synctype;
} dspriteq1_t;

typedef struct
{
	int		ident;
	int		version;
	int		type;
	int		texFormat;	// Half-Life stuff only
	float		boundingradius;	// software rendering stuff
	int		width;
	int		height;
	int		numframes;
	float		beamlength;	// software rendering stuff
	synctype_t	synctype;
} dspritehl_t;

/*
========================================================================

.SP2 sprite file format

========================================================================
*/
#define IDSPRQ2HEADER		(('2'<<24)+('S'<<16)+('D'<<8)+'I') // little-endian "IDS2"

#define SPRITEQ2_VERSION		2

typedef struct
{
	int		width;
	int		height;
	int		origin_x;
	int		origin_y;		// raster coordinates inside pic
	char		name[64];		// name of pcx file
} dsprframeq2_t;

typedef struct
{
	int		ident;
	int		version;
	int		numframes;
	dsprframeq2_t	frames[1];	// variable sized
} dspriteq2_t;

_inline const char *SPR_RenderMode( void )
{
	switch( spr.texFormat )
	{
	case SPR_ADDGLOW: return "glow";
	case SPR_ALPHTEST: return "alphatest";
	case SPR_INDEXALPHA: return "indexalpha";
	case SPR_ADDITIVE: return "additive";
	case SPR_NORMAL: return "solid";
	default: return "normal";
	}
}

_inline const char *SPR_RenderType( void )
{
	switch( spr.type )
	{
	case SPR_ORIENTED: return "oriented";
	case SPR_VP_PARALLEL: return "vp_parallel";
	case SPR_FACING_UPRIGHT: return "facing_upright";
	case SPR_VP_PARALLEL_UPRIGHT: return "vp_parallel_upright";
	case SPR_VP_PARALLEL_ORIENTED: return "vp_parallel_oriented";
	default: return "oriented";
	}
}

void *SPR_ConvertFrame( const char *name, const char *ext, void *pin, int framenum, int groupframenum )
{
	rgbdata_t		*pix;
	dspriteframe_t	*pinframe;
	string		framename;
	byte		*fin, *fout;
	int		i, pixels, width, height;

	pinframe = (dspriteframe_t *)pin;
	width = LittleLong( pinframe->width );
	height = LittleLong( pinframe->height );
	fin =  (byte *)(pinframe + 1);
	if( width <= 0 || height <= 0 )
	{
		// NOTE: in Q1 existing sprites with blank frames
		spr.totalframes--;
		return (void *)((byte *)(pinframe + 1));
	}

	// extract sprite name from path
	FS_FileBase( name, framename );
	com.strcat( framename, va("_%i", framenum ));
	pixels = width * height;
	if( spr.truecolor )	pixels *= 4;

	// frame exist, go next
	if( FS_FileExists( va("%s/sprites/%s.%s", gs_gamedir, framename, ext )))
		return (void *)((byte *)(pinframe + 1) + pixels );		

	if( spr.truecolor )	
	{
		// HACKHACK: manually create rgbdata_t
		pix = Mem_Alloc( basepool, sizeof( *pix ));
		fout = Mem_Alloc( basepool, pixels );
		Mem_Copy( fout, fin, pixels );
		if( spr.texFormat >= SPR_INDEXALPHA )
			pix->flags |= IMAGE_HAVE_ALPHA;
		pix->type = PF_RGBA_32;
		pix->width = width;
		pix->height = height;
		pix->size = pixels; 
		pix->numLayers = 1;
		pix->numMips = 1;
		pix->buffer = fout;
	}
	else
	{
		pix = FS_LoadImage( va( "#%s.spr", framename ), pin, pixels );
		Image_Process( &pix, 0, 0, IMAGE_PALTO24 );
	}

	if( groupframenum )
	{
		i = groupframenum - 1;
		com.strncpy( spr.group[spr.numgroup].frame[i].name, framename, MAX_STRING );
		spr.group[spr.numgroup].frame[i].origin[0] = -(float)LittleLong(pinframe->origin[0]);
		spr.group[spr.numgroup].frame[i].origin[1] = (float)LittleLong(pinframe->origin[1]);
		spr.group[spr.numgroup].frame[i].width = (float)LittleLong(pinframe->width);
		spr.group[spr.numgroup].frame[i].height = (float)LittleLong(pinframe->height);
	}
	else
	{
		com.strncpy( spr.frame[framenum].name, framename, MAX_STRING );
		spr.frame[framenum].origin[0] = -(float)LittleLong(pinframe->origin[0]);
		spr.frame[framenum].origin[1] = (float)LittleLong(pinframe->origin[1]);
		spr.frame[framenum].width = (float)LittleLong(pinframe->width);
		spr.frame[framenum].height = (float)LittleLong(pinframe->height);
	}

	FS_SaveImage( va("%s/sprites/%s.%s", gs_gamedir, framename, ext ), pix );
	FS_FreeImage( pix ); // free image

	// jump to next frame
	return (void *)((byte *)(pinframe + 1) + pixels ); // no mipmap stored
}

bool SP2_ConvertFrame( const char *name, const char *ext, int framenum )
{
	byte	*fin;
	size_t	filesize;
	string	barename;

	// store framename
	FS_FileBase( name, spr.frame[framenum].name ); 
	com.strncpy( barename, name, MAX_STRING );
	FS_StripExtension( barename );
		
	if( FS_FileExists( va("%s/%s.%s", gs_gamedir, barename, ext )))
		return true;

	fin = FS_LoadFile( name, &filesize );
	if( fin )
	{
		ConvPCX( barename, fin, filesize, ext );
		Mem_Free( fin );
		return true;
	}
	return false;
}

void *SPR_ConvertGroup( const char *name, const char *ext, void *pin, int framenum )
{
	dspritegroup_t	*pingroup;
	int		i, numframes;
	dspriteinterval_t	*pin_intervals;
	void		*ptemp;

	pingroup = (dspritegroup_t *)pin;
	numframes = LittleLong( pingroup->numframes );
	pin_intervals = (dspriteinterval_t *)(pingroup + 1);

	for (i = 0; i < numframes; i++) 
	{
		spr.group[spr.numgroup].interval[i] = LittleLong( pin_intervals->interval );
		pin_intervals++;
	}

	// update global numframes
	spr.group[spr.numgroup].numframes = numframes - 1;
	ptemp = (void *)pin_intervals;

	for (i = 0; i < numframes; i++ )
	{
		ptemp = SPR_ConvertFrame( name, ext, ptemp, framenum + i, i + 1 );
	}
	spr.numgroup++;
	return ptemp;
}

bool SPR_WriteScript( const char *name, const char *ext )
{
	string	shortname;
	int	i, j;
	file_t	*f;

	FS_FileBase( name ,shortname );
	if( FS_FileExists( va("%s/sprites/%s.qc", gs_gamedir, shortname )))
		return true;
	f = FS_Open( va("%s/sprites/%s.qc", gs_gamedir, shortname ), "w" );

	if( !f )
	{
		Msg( "Can't create qc-script \"%s.qc\"\n", shortname );
		return false;
	}

	// description
	FS_Printf(f,"//=======================================================================\n");
	FS_Printf(f,"//\t\t\tCopyright XashXT Group %s ©\n", timestamp( TIME_YEAR_ONLY ));
	FS_Printf(f,"//\t\t\twritten by Xash Miptex Decompiler\n", name );
	FS_Printf(f,"//=======================================================================\n");

	// sprite header
	FS_Printf(f, "\n$spritename\t%s.spr\n", name );
	FS_Printf(f, "$type\t\t%s\n", SPR_RenderType());
	FS_Printf(f, "$texture\t\t%s\n\n", SPR_RenderMode());

	// frames description
	for( i = 0; i < spr.totalframes - spr.numgroup; i++)
	{
		FS_Printf(f,"$load\t\t%s.%s\n", spr.frame[i].name, ext );
		FS_Printf(f,"$frame\t\t0 0 %d %d", spr.frame[i].width, spr.frame[i].height );
		if(!spr.frame[i].origin[0] && !spr.frame[i].origin[1]) FS_Print(f, "\n" ); 
		else FS_Printf(f, " %.1f %d %d\n", 0.1f, spr.frame[i].origin[0],spr.frame[i].origin[1]);
	}

	for( i = 0; i < spr.numgroup; i++ )
	{
		FS_Print(f, "$group\n{\n" );
		for( j = 0; j < spr.group[i].numframes; j++)
		{
			FS_Printf(f,"\t$load\t\t%s.%s\n", spr.group[i].frame[j].name, ext );
			FS_Printf(f,"\t$frame\t\t0 0 %d %d", spr.group[i].frame[j].width, spr.group[i].frame[j].height );
			if( spr.group[i].interval[j] ) FS_Printf(f, " %g", spr.group[i].interval[j] );
			if(!spr.group[i].frame[j].origin[0] && !spr.group[i].frame[j].origin[1]) FS_Print(f, "\n" ); 
			else FS_Printf(f, " %d %d\n", spr.group[i].frame[j].origin[0],spr.group[i].frame[j].origin[1]);
		}
		FS_Print(f, "}\n" );
	}

	FS_Print(f,"\n" );
	FS_Close( f );
	Msg( "%s.spr\n", name ); // echo to console
	return true;
}

/*
============
ConvSPR
============
*/
bool ConvSPR( const char *name, byte *buffer, size_t filesize, const char *ext )
{
	dframetype_t	*pframetype = NULL;
	dspriteq2_t	*pinq2;
	dspritehl_t	*pinhl;
	dspriteq1_t	*pin;
	short		*numi;
	int		i;

	// defaulting to q1
	pin = (dspriteq1_t *)buffer;
	memset( &spr, 0, sizeof( spr ));

	switch( LittleLong( pin->ident ))
	{
	case IDSPRQ1HEADER:
		switch( LittleLong( pin->version ))
		{
		case SPRITEQ1_VERSION:
			spr.totalframes = LittleLong( pin->numframes );
			spr.texFormat = SPR_ALPHTEST; // constant
			spr.type = LittleLong( pin->type );
			pframetype = (dframetype_t *)(pin + 1);
			spr.truecolor = false;
			break;
		case SPRITE32_VERSION:
			spr.totalframes = LittleLong( pin->numframes );
			spr.texFormat = SPR_ADDITIVE; // constant
			spr.type = LittleLong( pin->type );

			pframetype = (dframetype_t *)(pin + 1);
			spr.truecolor = true;
			break;
		case SPRITEHL_VERSION:
			pinhl = (dspritehl_t *)buffer; // reorganize header
			spr.totalframes = LittleLong( pinhl->numframes );
			spr.texFormat = LittleLong( pinhl->texFormat );
			spr.type = LittleLong( pinhl->type );
			numi = (short *)(pinhl + 1);
			spr.truecolor = false;

			if( LittleShort( *numi ) == 256 )
			{
				byte *src = (byte *)(numi + 1);	
				rgbdata_t	*pal = NULL;

				// install palette
				switch( spr.texFormat )
				{
                    		case SPR_INDEXALPHA:
				case SPR_ALPHTEST:		
					pal = FS_LoadImage( "#transparent.pal", src, 768 );
					break;
				case SPR_NORMAL:
				case SPR_ADDGLOW:
				case SPR_ADDITIVE:
				default:
					pal = FS_LoadImage( "#normal.pal", src, 768 );
					break;
				}

				// get frametype for first frame
				pframetype = (dframetype_t *)(src + 768);
				//FS_FreeImage( pal ); // palette installed, no reason to keep this data
			}
			else
			{
				Msg("\"%s.spr\" have invalid palette size\n", name );
				return false;
			}
			break;
		default:
			Msg("\"%s.%s\" unknown version %i\n", name, "spr", LittleLong( pin->version ));
			return false;
		}
		break;
	case IDSPRQ2HEADER:
		switch( LittleLong( pin->version ))
		{
		case SPRITEQ2_VERSION:
			pinq2 = (dspriteq2_t *)buffer;
			spr.totalframes = LittleLong( pinq2->numframes );
			spr.texFormat = SPR_ALPHTEST; // constants
			spr.type = SPR_FWD_PARALLEL;
			spr.truecolor = false;
			for( i = 0; i < spr.totalframes; i++ )
			{
				spr.frame[i].width = LittleLong( pinq2->frames[i].width );
				spr.frame[i].height = LittleLong( pinq2->frames[i].height );
				spr.frame[i].origin[0] = LittleLong( pinq2->frames[i].origin_x );
				spr.frame[i].origin[1] = LittleLong( pinq2->frames[i].origin_y );
				SP2_ConvertFrame( pinq2->frames[i].name, ext, i );
			}
			break;
		default:
			Msg("\"%s.%s\" unknown version %i\n", name, "sp2", LittleLong( pin->version ));
			return false;
		}
		break;
	default: 
		Msg("\"%s\" have invalid header\n", name );
		return false;
	}

	// .SPR save frames as normal images
	for( i = 0; pframetype && i < spr.totalframes; i++ )
	{
		frametype_t frametype = LittleLong( pframetype->type );

		if( frametype == SPR_SINGLE )
			pframetype = (dframetype_t *)SPR_ConvertFrame( name, ext, (pframetype + 1), i, 0 );
		else pframetype = (dframetype_t *)SPR_ConvertGroup( name, ext, (pframetype + 1), i );
	}
	return SPR_WriteScript( name, ext ); // write script file and done
}