//=======================================================================
//			Copyright XashXT Group 2007 ©
//		  conv_sprite.c - convert q1\hl\spr32 sprites
//=======================================================================

#include "ripper.h"
#include "mathlib.h"
#include "pal_utils.h"
#include "qc_gen.h"

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

typedef struct
{
	int		origin[2];
	int		width;
	int		height;
} dspriteframe_t;

//
// sprite_decompiler.c
//

void *SPR_ConvertFrame( const char *name, void *pin, int framenum, int groupframenum )
{
	rgbdata_t		pix;
	dspriteframe_t	*pinframe;
	char		framename[256];
	byte		*fin, *fout, *fpal;
	int		i, p, pixels, width, height;

	pinframe = (dspriteframe_t *)pin;
	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	fin =  (byte *)(pinframe + 1);
	if( width <= 0 || height <= 0 )
	{
		// Note: in Q1 exist sprites with blank frames
		spr.totalframes--;
		return (void *)((byte *)(pinframe + 1));
	}
	fout = Mem_Alloc( zonepool, width * height * 4 );
	// extract sprite name from path
	FS_FileBase( name, framename );
	com.strcat( framename, va("_%i", framenum ));
	memset( &pix, 0, sizeof(pix));

	if( spr.truecolor )
	{
		pixels = width * height * 4;
		Mem_Copy( fout, fin, pixels );
	}
	else
	{
		pixels = width * height;
		fpal = (byte *)(&spr.palette[0][0]);

		// expand image to 32-bit rgba buffer
		for (i = 0; i < pixels; i++ )
		{
			p = fin[i];
			fout[(i<<2)+0] = fpal[(p<<2)+0];
			fout[(i<<2)+1] = fpal[(p<<2)+1];
			fout[(i<<2)+2] = fpal[(p<<2)+2];
			fout[(i<<2)+3] = fpal[(p<<2)+3];
		}
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

	// preparing for write
	pix.width = width;
	pix.height = height;
	pix.numLayers = 1;
	pix.numMips = 1;
	pix.type = PF_RGBA_32;
	pix.buffer = fout;
	pix.size = width * height * 4; 
	if( spr.texFormat >= SPR_INDEXALPHA )
		pix.flags |= IMAGE_HAS_ALPHA;

	Image->SaveImage( va("%s/sprites/%s.tga", gs_gamedir, framename ), &pix );
	Mem_Free( fout ); // release buffer

	// jump to next frame
	return (void *)((byte *)(pinframe + 1) + pixels ); // no mipmap stored
}

void *SPR_ConvertGroup( const char *name, void *pin, int framenum )
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
		ptemp = SPR_ConvertFrame( name, ptemp, framenum + i, i + 1 );
	}
	spr.numgroup++;
	return ptemp;
}

bool SPR_WriteScript( const char *name )
{
	int	i, j;
	file_t	*f = FS_Open( va("%s/sprites/%s.qc", gs_gamedir, name ), "w" );

	if( !f )
	{
		Msg("Can't generate qc-script \"%s.qc\"\n", name );
		return false;
	}

	// description
	FS_Printf(f,"//=======================================================================\n");
	FS_Printf(f,"//\t\t\tCopyright XashXT Group 2007 ©\n");
	FS_Printf(f,"//\t\t\twritten by Xash Miptex Decompiler\n", name );
	FS_Printf(f,"//=======================================================================\n");

	// sprite header
	FS_Printf(f, "\n$spritename\t%s.spr\n", name );
	FS_Printf(f, "$type\t\t%s\n",SPR_RenderType());
	FS_Printf(f, "$render\t\t%s\n\n",SPR_RenderMode());

	// frames description
	for( i = 0; i < spr.totalframes - spr.numgroup; i++)
	{
		FS_Printf(f,"$load\t\t%s.tga\n", spr.frame[i].name );
		FS_Printf(f,"$frame\t\t0 0 %d %d", spr.frame[i].width, spr.frame[i].height );
		if(!spr.frame[i].origin[0] && !spr.frame[i].origin[1]) FS_Print(f, "\n" ); 
		else FS_Printf(f, " %.1f %d %d\n", 0.1f, spr.frame[i].origin[0],spr.frame[i].origin[1]);
	}

	for( i = 0; i < spr.numgroup; i++ )
	{
		FS_Print(f, "$group\n{\n" );
		for( j = 0; j < spr.group[i].numframes; j++)
		{
			FS_Printf(f,"\t$load\t\t%s.tga\n", spr.group[i].frame[j].name );
			FS_Printf(f,"\t$frame\t\t0 0 %d %d", spr.group[i].frame[j].width, spr.group[i].frame[j].height );
			if( spr.group[i].interval[j] ) FS_Printf(f, " %g", spr.group[i].interval[j] );
			if(!spr.group[i].frame[j].origin[0] && !spr.group[i].frame[j].origin[1]) FS_Print(f, "\n" ); 
			else FS_Printf(f, " %d %d\n", spr.group[i].frame[j].origin[0],spr.group[i].frame[j].origin[1]);
		}
		FS_Print(f, "}\n" );
	}

	FS_Print(f,"\n" );
	FS_Close(f);
	Msg("%s.spr\n", name ); // echo to console about current sprite
	return true;
}

/*
============
ConvSPR
============
*/
bool ConvSPR( const char *name, char *buffer, int filesize )
{
	int		i, version;
	dframetype_t	*pframetype;
	string		scriptname;
	dspriteq1_t		*pin;
	dspritehl_t	*pinhl;
	short		*numi;

	pin = (dspriteq1_t *)buffer;
	version = LittleLong( pin->version );
	memset( &spr, 0, sizeof(spr));

	if( pin->ident != IDSPRQ1HEADER )
	{
		Msg("\"%s\" have invalid header\n", name );
		return false;
	}
	switch( version )
	{
	case SPRITEQ1_VERSION:
		spr.totalframes = LittleLong( pin->numframes );
		spr.texFormat = SPR_INDEXALPHA; // constant
		spr.type = LittleLong( pin->type );

		// palette setup
		Conv_GetPaletteQ1(); // setup palette
		Mem_Copy( spr.palette, d_currentpal, 1024 );

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

		if( LittleShort(*numi) == 256 )
		{
			byte *src = (byte *)(numi + 1);	

			switch( spr.texFormat )
			{
			case SPR_ADDGLOW:
			case SPR_ADDITIVE:
				for (i = 0; i < 256; i++)
				{
					spr.palette[i][0] = *src++;
					spr.palette[i][1] = *src++;
					spr.palette[i][2] = *src++;
					spr.palette[i][3] = 255;
				}
				break;
			case SPR_INDEXALPHA:
				for (i = 0; i < 256; i++)
				{
					spr.palette[i][0] = *src++;
					spr.palette[i][1] = *src++;
					spr.palette[i][2] = *src++;
					spr.palette[i][3] = i;
				}
				break;
			case SPR_ALPHTEST:		
				for (i = 0; i < 256; i++)
				{
					spr.palette[i][0] = *src++;
					spr.palette[i][1] = *src++;
					spr.palette[i][2] = *src++;
					spr.palette[i][3] = 255;
				}
				memset( spr.palette[255], 0, sizeof(uint)); // last entry
				break;
			default: Msg("Warning: %s.spr have unknown texFormat %i\n", name, spr.texFormat );
			case SPR_NORMAL:
				for (i = 0; i < 256; i++)
				{
					spr.palette[i][0] = *src++;
					spr.palette[i][1] = *src++;
					spr.palette[i][2] = *src++;
					spr.palette[i][3] = 0;
				}
				break;
			}
			// get frametype for first frame
			pframetype = (dframetype_t *)(src);
		}
		else
		{
			Msg("\"%s.spr\" have invalid palette size\n", name );
			return false;
		}
		break;
	default:
		Msg("\"%s.spr\" unknown version\n", name );
		return false;
	}

	// save frames as normal images
	for (i = 0; i < spr.totalframes; i++ )
	{
		frametype_t frametype = LittleLong( pframetype->type );

		if( frametype == SPR_SINGLE )
		{
			pframetype = (dframetype_t *)SPR_ConvertFrame( name, (pframetype + 1), i, 0 );
		}
		else if( frametype == SPR_GROUP )
		{
			pframetype = (dframetype_t *)SPR_ConvertGroup( name, (pframetype + 1), i );
		}
		if( pframetype == NULL ) break; // technically an error
	}

	// write script file and out
	FS_FileBase( name, scriptname );
	return SPR_WriteScript( scriptname );
}