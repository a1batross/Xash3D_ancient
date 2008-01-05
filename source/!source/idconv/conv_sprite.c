//=======================================================================
//			Copyright XashXT Group 2007 ©
//		  conv_sprite.c - convert q1\hl\spr32 sprites
//=======================================================================

#include "idconv.h"
#include "mathlib.h"
#include "basefiles.h"

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

int spr_type;
int spr_texFormat;
int spr_width;
int spr_height;
int spr_numframes;
int spr_sequence;
byte spr_palette[256][4];
bool spr_truecolor = false;
string spr_framenames[256];
string spr_groupnames[256];
vec2_t spr_origin[256];
vec2_t spr_size[256];

const char *SPR_RenderMode( void )
{
	switch( spr_texFormat )
	{
	case SPR_ADDGLOW: return "glow";
	case SPR_ALPHTEST: return "alphatest";
	case SPR_INDEXALPHA: return "indexalpha";
	case SPR_ADDITIVE: return "additive";
	case SPR_NORMAL: return "normal";
	default: return "normal";
	}
}

const char *SPR_RenderType( void )
{
	switch( spr_type )
	{
	case SPR_ORIENTED: return "oriented";
	case SPR_VP_PARALLEL: return "vp_parallel";
	case SPR_FACING_UPRIGHT: return "facing_upright";
	case SPR_VP_PARALLEL_UPRIGHT: return "vp_parallel_upright";
	case SPR_VP_PARALLEL_ORIENTED: return "vp_parallel_oriented";
	default: return "oriented";
	}
}

void *SPR_ConvertFrame( const char *name, void *pin, int framenum )
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
		spr_numframes--;
		return (void *)((byte *)(pinframe + 1));
	}
	fout = Mem_Alloc( zonepool, width * height * 4 );
	// extract sprite name from path
	FS_FileBase( name, framename );
	com.strcat( framename, va("_%i", framenum ));
	memset( &pix, 0, sizeof(pix));

	if( spr_truecolor )
	{
		pixels = width * height * 4;
		Mem_Copy( fout, fin, pixels );
	}
	else
	{
		pixels = width * height;
		fpal = (byte *)(&spr_palette[0][0]);

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

	com.strncpy( spr_framenames[framenum], framename, MAX_STRING );
	spr_origin[framenum][0] = (float)LittleLong(pinframe->origin[0]);
	spr_origin[framenum][1] = (float)LittleLong(pinframe->origin[1]);
	spr_size[framenum][0] = (float)LittleLong(pinframe->width);
	spr_size[framenum][1] = (float)LittleLong(pinframe->height);

	// preparing for write
	pix.width = width;
	pix.height = height;
	pix.numLayers = 1;
	pix.numMips = 1;
	pix.type = PF_RGBA_32;
	pix.buffer = fout;
	pix.size = width * height * 4; 
	if( spr_texFormat >= SPR_INDEXALPHA )
		pix.flags |= IMAGE_HAS_ALPHA;

	FS_SaveImage( va("%s/sprites/%s.tga", gs_gamedir, framename ), &pix );
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
		//Msg("frame %d, interval %g\n", i, *pin_intervals );
		pin_intervals++;
	}

	// update global numframes
	spr_numframes += numframes - 1;
	ptemp = (void *)pin_intervals;

	for (i = 0; i < numframes; i++ )
	{
		ptemp = SPR_ConvertFrame( name, ptemp, framenum + i );
	}
	return ptemp;
}

bool SPR_WriteScript( const char *name )
{
	int	i;
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
	FS_Printf(f, "$texture\t\t%s\n",SPR_RenderMode());

	// frames description
	for( i = 0; i < spr_numframes; i++)
	{
		FS_Printf(f,"$load\t\t%s.tga\n", spr_framenames[i] );
		FS_Printf(f,"$frame\t\t0   0   %.f   %.f", spr_size[i][0], spr_size[i][1] );
		if(!spr_origin[i][0] && !spr_origin[i][1]) FS_Print(f, "\n" ); 
		else FS_Printf(f, "   %.1f   %.f   %.f\n", 0.1f, fabs(spr_origin[i][0]), spr_origin[i][1] );
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
	dsprite_t		*pin;
	dspritehl_t	*pinhl;
	short		*numi;

	pin = (dsprite_t *)buffer;
	version = LittleLong( pin->version );

	if( pin->ident != IDSPRQ1HEADER )
	{
		Msg("\"%s.spr\" have invalid header\n", name );
		return false;
	}
	switch( version )
	{
	case SPRITEQ1_VERSION:
		spr_numframes = LittleLong( pin->numframes );
		spr_texFormat = SPR_INDEXALPHA; // constant
		spr_type = LittleLong( pin->type );
		spr_width = LittleLong( pin->width );
		spr_height = LittleLong( pin->height );

		// palette setup
		Conv_GetPaletteQ1(); // setup palette
		Mem_Copy( spr_palette, d_currentpal, 1024 );

		pframetype = (dframetype_t *)(pin + 1);
		spr_truecolor = false;
		break;
	case SPRITE32_VERSION:
		spr_numframes = LittleLong( pin->numframes );
		spr_texFormat = SPR_ADDITIVE; // constant
		spr_type = LittleLong( pin->type );
		spr_width = LittleLong( pin->width );
		spr_height = LittleLong( pin->height );

		pframetype = (dframetype_t *)(pin + 1);
		spr_truecolor = true;
		break;
	case SPRITEHL_VERSION:
		pinhl = (dspritehl_t *)buffer; // reorganize header
		spr_numframes = LittleLong( pinhl->numframes );
		spr_texFormat = LittleLong( pinhl->texFormat );
		spr_width = LittleLong( pinhl->width );
		spr_height = LittleLong( pinhl->height );
		spr_type = LittleLong( pinhl->type );
		numi = (short *)(pinhl + 1);
		spr_truecolor = false;

		if( LittleShort(*numi) == 256 )
		{
			byte *src = (byte *)(numi + 1);	

			switch( spr_texFormat )
			{
			case SPR_ADDGLOW:
			case SPR_ADDITIVE:
				for (i = 0; i < 256; i++)
				{
					spr_palette[i][0] = *src++;
					spr_palette[i][1] = *src++;
					spr_palette[i][2] = *src++;
					spr_palette[i][3] = 255;
				}
				break;
			case SPR_INDEXALPHA:
				for (i = 0; i < 256; i++)
				{
					spr_palette[i][0] = *src++;
					spr_palette[i][1] = *src++;
					spr_palette[i][2] = *src++;
					spr_palette[i][3] = i;
				}
				break;
			case SPR_ALPHTEST:		
				for (i = 0; i < 256; i++)
				{
					spr_palette[i][0] = *src++;
					spr_palette[i][1] = *src++;
					spr_palette[i][2] = *src++;
					spr_palette[i][3] = 255;
				}
				memset( spr_palette[255], 0, sizeof(uint)); // last entry
				break;
			default: Msg("Warning: %s.spr have unknown texFormat %i\n", name, spr_texFormat );
			case SPR_NORMAL:
				for (i = 0; i < 256; i++)
				{
					spr_palette[i][0] = *src++;
					spr_palette[i][1] = *src++;
					spr_palette[i][2] = *src++;
					spr_palette[i][3] = 0;
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

	memset(spr_framenames, 0, 256 * MAX_STRING );
	memset(spr_origin, 0, 256 * sizeof(vec2_t) );

	// save frames as normal images
	for (i = 0; i < spr_numframes; i++ )
	{
		frametype_t frametype = LittleLong( pframetype->type );

		if( frametype == SPR_SINGLE )
		{
			pframetype = (dframetype_t *)SPR_ConvertFrame( name, (pframetype + 1), i );
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