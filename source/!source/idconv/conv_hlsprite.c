//=======================================================================
//			Copyright XashXT Group 2007 ©
//		  conv_hlsprite.c - convert half-life sprites
//=======================================================================

#include "idconv.h"

int sprhl_type = 0;
int sprhl_texFormat = 0;
int spr_width = 1;
int spr_height = 1;
int sprhl_numframes = 1;
int sprhl_sequence = 0;
byte sprhl_palette[256][4];
float sprhl_framerate = 15.0f;
string *sprhl_framenames;
vec2_t *sprhl_origin;

const char *SPR_RenderMode( void )
{
	switch( sprhl_texFormat )
	{
	case SPR_ADDGLOW: return "glow";
	case SPR_ALPHTEST: return "decal";
	case SPR_INDEXALPHA: return "decal";
	case SPR_ADDITIVE: return "additive";
	case SPR_NORMAL: return "normal";
	default: return "normal";
	}
}

const char *SPR_RenderType( void )
{
	switch( sprhl_type )
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

	// extract sprite name from path
	FS_FileBase( name, framename );
	com.strcat( framename, va("_#%i%i", framenum/10, framenum%10 ));
	memset( &pix, 0, sizeof(pix));

	pinframe = (dspriteframe_t *)pin;
	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	pixels = width * height;

	fin =  (byte *)(pinframe + 1);
	fout = Mem_Alloc( zonepool, pixels * 4 );
	fpal = (byte *)(&sprhl_palette[0][0]);

	// expand image to 32-bit rgba buffer
	for (i = 0; i < pixels; i++ )
	{
		p = fin[i];
		fout[(i<<2)+0] = fpal[(p<<2)+0];
		fout[(i<<2)+1] = fpal[(p<<2)+1];
		fout[(i<<2)+2] = fpal[(p<<2)+2];
		fout[(i<<2)+3] = fpal[(p<<2)+3];
	}

	// store framename
	com.strncpy( sprhl_framenames[framenum], framename, MAX_STRING );
	// store origin
	sprhl_origin[framenum][0] = (float)LittleLong(pinframe->origin[0]);
	sprhl_origin[framenum][1] = (float)LittleLong(pinframe->origin[1]);

	// preparing for write
	pix.width = width;
	pix.height = height;
	pix.numLayers = 1;
	pix.numMips = 1;
	pix.type = PF_RGBA_32;
	pix.buffer = fout;
	pix.size = pixels * 4; 
	if( sprhl_texFormat >= SPR_INDEXALPHA )
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
	float		*poutintervals;
	void		*ptemp;

	pingroup = (dspritegroup_t *)pin;
	numframes = LittleLong( pingroup->numframes );
	pin_intervals = (dspriteinterval_t *)(pingroup + 1);
	poutintervals = Mem_Alloc(zonepool, numframes * sizeof(float));

	for (i = 0; i < numframes; i++)
	{
		*poutintervals = LittleFloat(pin_intervals->interval);
		if(*poutintervals <= 0.0) return NULL; // negate interval
		poutintervals++;
		pin_intervals++;
	}
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
	file_t	*f = FS_Open( va("%s/sprites/%s.txt", gs_gamedir, name ), "w" );

	if( !f )
	{
		Msg("Can't write sprite header \"%s.txt\"\n", name );
		return false;
	}

	// description
	FS_Printf(f,"//=======================================================================\n");
	FS_Printf(f,"//\t\t\tCopyright XashXT Group 2007 ©\n");
	FS_Printf(f,"//\t\t\t%s.spr - Xash Sprite Format\n", name );
	FS_Printf(f,"//=======================================================================\n");

	// sprite header
	FS_Printf(f,"\nheader \"%s\"\n{\n\ttype\t\"%s\"\n\trender\t\"%s\"\n",name,SPR_RenderType(),SPR_RenderMode());
	FS_Printf(f,"\tframerate\t\"%.1f\"\n\tsequence\t\"%d\"\n}\n", sprhl_framerate, sprhl_sequence );

	// frames description
	FS_Printf(f,"\nframes \"%s\"\n{\n", name );
	
	for( i = 0; i < sprhl_numframes; i++)
	{
		FS_Printf(f,"\tframe\t\"sprites/%s\"", sprhl_framenames[i] );
		if(!sprhl_origin[i][0] && !sprhl_origin[i][1]) FS_Print(f, "\n" ); 
		else FS_Printf(f, "\t%d\t%d\n", (int)sprhl_origin[i][0], (int)sprhl_origin[i][1] );
	}
	FS_Print(f,"}\n" );
	FS_Printf(f,"\nsequence 0\n{\n\tpattern\t" ); // default sequence

	if(sprhl_numframes > 1) FS_Printf(f,"\"%d..%d\"", 0, sprhl_numframes - 1 );
	else FS_Print(f,"\"0\"" );
	FS_Print(f,"\n}\n" );

	FS_Close(f);
	Mem_Free( sprhl_framenames );
	Mem_Free( sprhl_origin );
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
	dsprite_t		*pin;
	short		*numi;

	pin = (dsprite_t *)buffer;
	version = LittleLong( pin->version );

	if( pin->ident != IDSPRHLHEADER || pin->version != SPRITE_VERSION )
	{
		Msg("\"%s.spr\" it's not a Half-Life sprite\n", name );
		return false;
	}
	sprhl_numframes = LittleLong( pin->numframes );
	// maximum size of frame
	sprhl_texFormat = LittleLong( pin->texFormat );
	sprhl_type = LittleLong( pin->type );
	numi = (short *)(pin + 1);
	sprhl_framenames = Mem_Alloc( zonepool, sprhl_numframes * MAX_STRING );
	sprhl_origin = Mem_Alloc( zonepool, sprhl_numframes * sizeof(vec2_t));

	if( LittleShort(*numi) == 256 )
	{
		byte *src = (byte *)(numi + 1);	

		switch( sprhl_texFormat )
		{
		case SPR_ADDGLOW:
		case SPR_ADDITIVE:
			for (i = 0; i < 256; i++)
			{
				sprhl_palette[i][0] = *src++;
				sprhl_palette[i][1] = *src++;
				sprhl_palette[i][2] = *src++;
				sprhl_palette[i][3] = 255;
			}
			break;
		case SPR_INDEXALPHA:
			for (i = 0; i < 256; i++)
			{
				sprhl_palette[i][0] = *src++;
				sprhl_palette[i][1] = *src++;
				sprhl_palette[i][2] = *src++;
				sprhl_palette[i][3] = i;
			}
			break;
		case SPR_ALPHTEST:		
			for (i = 0; i < 256; i++)
			{
				sprhl_palette[i][0] = *src++;
				sprhl_palette[i][1] = *src++;
				sprhl_palette[i][2] = *src++;
				sprhl_palette[i][3] = 255;
			}
			memset( sprhl_palette[255], 0, sizeof(uint)); // last entry
			break;
		default:
			Msg("Warning: %s.spr have unknown texFormat %i\n", name, sprhl_texFormat );
		case SPR_NORMAL:
			for (i = 0; i < 256; i++)
			{
				sprhl_palette[i][0] = *src++;
				sprhl_palette[i][1] = *src++;
				sprhl_palette[i][2] = *src++;
				sprhl_palette[i][3] = 0;
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

	// save frames as normal images (.tga or .dds)
	for (i = 0; i < sprhl_numframes; i++ )
	{
		frametype_t frametype = LittleLong( pframetype->type );

		if( frametype == SPR_SINGLE )
		{
			pframetype = (dframetype_t *)SPR_ConvertFrame( name, pframetype + 1,  i );
		}
		else if( frametype == SPR_GROUP )
		{
			pframetype = (dframetype_t *)SPR_ConvertFrame( name, pframetype + 1,  i );
		}
		if( pframetype == NULL ) break; // technically an error
	}

	// write script file and out
	return SPR_WriteScript( name );
}