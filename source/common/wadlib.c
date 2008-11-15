//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        wadlib.c.c - wad archive compiler
//=======================================================================

#include "platform.h"
#include "byteorder.h"
#include "mathlib.h"
#include "const.h"
#include "utils.h"

string		wadoutname;
bool		wad_append = false;
script_t		*wadqc = NULL;
wfile_t		*handle = NULL;
string		lumpname;
byte		*wadpool;

float		linearpalette[256][3];
int		color_used[256];
float		maxdistortion;
int		colors_used;
byte		pixdata[256];
rgbdata_t		*image = NULL;
vec3_t		d_color;

byte Pal_AddColor( float r, float g, float b )
{
	int	i;

	for( i = 0; i < 255; i++ )
	{
		if( !color_used[i] )
		{
			linearpalette[i][0] = r;
			linearpalette[i][1] = g;
			linearpalette[i][2] = b;
			if( r < 0.0 ) r = 0.0;
			if( r > 1.0 ) r = 1.0;
			image->palette[i*3+0] = pow( r, 1.0 / 2.2) * 255;
			if( g < 0.0 ) g = 0.0;
			if( g > 1.0 ) g = 1.0;
			image->palette[i*3+1] = pow( g, 1.0 / 2.2) * 255;
			if( b < 0.0 ) b = 0.0;
			if( b > 1.0 ) b = 1.0;
			image->palette[i*3+2] = pow( b, 1.0 / 2.2) * 255;
			color_used[i] = 1;
			colors_used++;
			return i;
		}
	}
	return 0;
}

/*
=============
Mip_AveragePixels

FIXME: share this code by imglib someday
=============
*/
byte Mip_AveragePixels( int count )
{
	float	r = 0, g = 0, b = 0;
	int	i, pix, vis = 0;
	float	bestdistortion, distortion;
	int	bestcolor;
	vec3_t	color;

	for( i = 0; i < count; i++ )
	{
		pix = pixdata[i];
		r += linearpalette[pix][0];
		g += linearpalette[pix][1];
		b += linearpalette[pix][2];
	}

	r /= count;
	g /= count;
	b /= count;

	r += d_color[0];
	g += d_color[1];
	b += d_color[2];
	
	// find the best color
	bestdistortion = 3.0;
	bestcolor = -1;

	for( i = 0; i < 255; i++ )
	{
		if( color_used[i] )
		{
			pix = i;

			VectorSet( color, r-linearpalette[i][0], g-linearpalette[i][1], b-linearpalette[i][2]);
			distortion = DotProduct( color, color );
			if( distortion < bestdistortion )
			{
				if( !distortion )
				{
					VectorClear( d_color );	// no distortion yet
					return pix;		// perfect match
				}
				bestdistortion = distortion;
				bestcolor = pix;
			}
		}
	}

	if( bestdistortion > 0.001 && colors_used < 255 )
	{
		bestcolor = Pal_AddColor( r, g, b );
		VectorClear( d_color );
		bestdistortion = 0;
	}
	else
	{
		// error diffusion
		d_color[0] = r - linearpalette[bestcolor][0];
		d_color[1] = g - linearpalette[bestcolor][1];
		d_color[2] = b - linearpalette[bestcolor][2];
	}

	if( bestdistortion > maxdistortion )
		maxdistortion = bestdistortion;

	// index in palette (new or completely matched)
	return bestcolor;
}

void Wad3_NewWad( void )
{
	if( wad_append )
		handle = WAD_Open( wadoutname, "a+" );
	else handle = WAD_Open( wadoutname, "wb" );
	if( !handle ) Sys_Break( "Wad3_NewWad: can't create %s\n", wadoutname );
}

/*
===============
AddLump
===============
*/
void Wad3_AddLump( const byte *buffer, size_t lumpsize, int lump_type, bool compress )
{
	int result;
	if( !handle ) Wad3_NewWad(); 	// create wad file
	result = WAD_Write( handle, lumpname, buffer, lumpsize, lump_type, ( compress ? CMP_ZLIB : CMP_NONE ));
	if( result != -1 ) Msg("Add %s\t#%3i\n", lumpname, result ); // FIXME: align message
}

/*
==============
Cmd_GrabMip

$mipmap filename x y width height
==============
*/
void Cmd_GrabMip( void )
{
	int	i, j, x, y, xl, yl, xh, yh, w, h;
	byte	*plump, *plump_end, *screen_p, *source;
	int	miplevel,	mipstep, flags = 0;
	byte	*lump, testpixel;
	int	xx, yy, count;
	int	linedelta;
	size_t	plump_size;
	mip_t	*mip;

	Com_ReadString( wadqc, SC_PARSE_GENERIC, lumpname );

	// load mip image
	image = FS_LoadImage( lumpname, NULL, 0 );	
	if( !image )
	{
		Com_SkipRestOfLine( wadqc );
		// no fatal error, just ignore this image for adding into wad-archive
		MsgDev( D_ERROR, "Cmd_LoadMip: unable to loading %s\n", lumpname );
		return;
	}
          
	flags |= IMAGE_PALTO24;

	if(Com_ReadUlong( wadqc, false, &xl ))
	{
		Com_ReadUlong( wadqc, false, &yl);
		Com_ReadUlong( wadqc, false, &w );
		Com_ReadUlong( wadqc, false, &h );
	}
	else
	{
		xl = yl = 0;
		w = image->width;
		h = image->height;
	}

	// reflood image with new size
	if(( w & 15) || (h & 15)) flags |= IMAGE_ROUNDFILLER;
	Image_Process( &image, 0, 0, flags );

	if( flags & IMAGE_ROUNDFILLER )
	{
		// updates image size
		w = image->width;
		h = image->height;
	}

	xh = xl + w;
	yh = yl + h;

	// mip_t + mipmap0[w>>0*h>>0] + mipmap1[w>>1*h>>1] + mipmap2[w>>2*h>>2] + mipmap3[w>>3*h>>3]
	// + numolors[short] + palette[768];
	plump_size = (int)sizeof(*mip) + ((w * h * 85)>>6) + sizeof(short) + 768;
	plump = lump = (byte *)Mem_Alloc( wadpool, plump_size );
	plump_end = plump + plump_size; // sentinel

	mip = (mip_t *)plump;
	mip->width = LittleLong( w );
	mip->height = LittleLong( h );
	com.strncpy( mip->name, lumpname, sizeof(mip->name)); 
	plump = (byte *)&mip->offsets[4];
	
	screen_p = image->buffer + yl * image->width + xl;
	linedelta = image->width - w;

	source = plump;
	mip->offsets[0] = LittleLong( plump - (byte *)mip );

	// apply scissor to source
	for( y = yl; y < yh; y++ )
	{
		for( x = xl; x < xh; x++ )
			*plump++ = *screen_p++;
		screen_p += linedelta;
	}

	// calculate gamma corrected linear palette
	for( i = 0; i < 256; i++ )
	{
		for( j = 0; j < 3; j++ )
		{
			// assume textures are done at 2.2, we want to remap them at 1.0
			float f = image->palette[i*3+j] / 255.0;
			linearpalette[i][j] = pow( f, 2.2 );
		}
	}

	maxdistortion = 0;
	if(!(image->flags & IMAGE_HAS_ALPHA ))
	{
		// figure out what palette entries are actually used
		colors_used = 0;
		Mem_Set( color_used, 0, sizeof(int) * 256 );

		for( x = 0; x < w; x++ )
		{
			for( y = 0; y < h; y++ )
			{
				if(!color_used[source[y * w + x]])
				{
					color_used[source[y * w + x]] = 1;
					colors_used++;
				}
			}
		}
	}
	else
	{
		// assume palette full if it's a transparent texture
		colors_used = 256;
		Mem_Set( color_used, 1, sizeof(int) * 256 );
	}

	// subsample for greater mip levels
	for( miplevel = 1; miplevel < 4; miplevel++ )
	{
		int	pixTest;

		VectorClear( d_color );				// no distortion yet
		mip->offsets[miplevel] = LittleLong(plump - (byte *)mip);
		
		mipstep = 1<<miplevel;
		pixTest = (int)((float)(mipstep * mipstep) * 0.4 );	// 40% of pixels

		for( y = 0; y < h; y += mipstep )
		{
			for( x = 0; x < w; x += mipstep )
			{
				count = 0;
				for( yy = 0; yy < mipstep; yy++ )
				{
					for( xx = 0; xx < mipstep; xx++ )
					{
						testpixel = source[(y + yy) * w + x + xx];
						
						// if 255 is not transparent, or this isn't
						// a transparent pixel add it in to the image filter
						if(!(image->flags & IMAGE_HAS_ALPHA ) || testpixel != 255)
						{
							pixdata[count] = testpixel;
							count++;
						}
					}
				}
				// solid pixels account for < 40% of this pixel, make it transparent
				if( count <= pixTest ) *plump++ = 255;
				else *plump++ = Mip_AveragePixels( count );
			}	
		}
	}

	*(word*)plump = 256;	// palette size
	plump += sizeof( short );

	// bounds checker
	if( plump + 768 == plump_end )
	{
		Mem_Copy( plump, image->palette, 768 );
		plump += 768;

		// write out and release intermediate buffers
		Wad3_AddLump( lump, plump_size, TYPE_MIPTEX, false );
	}
	else MsgDev( D_WARN, "lump %s have invalid size, ignore\n", lumpname ); 
	FS_FreeImage( image );
	Mem_Free( lump );
}

/*
==============
Cmd_GrabPic

$gfxpic filename x y width height
==============
*/
void Cmd_GrabPic( void )
{
	int	x, y, xl, yl, xh, yh;
	byte	*plump, *lump;
	size_t	plump_size;
	lmp_t 	*pic;

	Com_ReadString( wadqc, SC_PARSE_GENERIC, lumpname );

	// load lmp image
	image = FS_LoadImage( lumpname, NULL, 0 );	
	if( !image )
	{
		Com_SkipRestOfLine( wadqc );
		// no fatal error, just ignore this image for adding into wad-archive
		MsgDev( D_ERROR, "Cmd_LoadPic: unable to loading %s\n", lumpname );
		return;
	}

	Image_Process( &image, 0, 0, IMAGE_PALTO24 );	// turn into 24-bit mode

	if(Com_ReadUlong( wadqc, false, &xl ))
	{
		Com_ReadUlong( wadqc, false, &yl);
		Com_ReadUlong( wadqc, false, &x );
		Com_ReadUlong( wadqc, false, &y );
		xh = xl + x;
		yh = yl + y;
	}
	else
	{
		xl = yl = 0;
		xh = image->width;
		yh = image->height;
	}

	if( xh < xl || yh < yl || xl < 0 || yl < 0 )
	{
		xl = yl = 0;
		xh = image->width;
		yh = image->height;
	}

	// lmp_t + picture[w*h] + numolors[short] + palette[768];
	plump_size = (int)sizeof(*pic) + (xh * yh) + sizeof(short) + 768;
	plump = lump = (byte *)Mem_Alloc( wadpool, plump_size );
	pic = (lmp_t *)plump;
	pic->width = LittleLong( xh - xl );
	pic->height = LittleLong( yh - yl );

	// apply scissor to source
	plump = (byte *)(pic + 1);
	for( y = yl; y < yh; y++ )
		for( x = xl; x < xh; x++ )
			*plump++ = (*(image->buffer + (y) * image->width + x));

	*(word*)plump = 256;	// palette size
	plump += sizeof(short);
	Mem_Copy( plump, image->palette, 768 );
	plump += 768;

	// write out and release intermediate buffers
	Wad3_AddLump( lump, plump_size, TYPE_QPIC, false );
	FS_FreeImage( image );
	Mem_Free( lump );
}

/*
==============
Cmd_GrabScript

$wadqc filename
==============
*/
void Cmd_GrabScript( void )
{
	byte	*lump;
	size_t	plump_size;

	Com_ReadString( wadqc, SC_PARSE_GENERIC, lumpname );

	// load mip image or replaced with error.bmp
	lump = FS_LoadFile( lumpname, &plump_size );
	
	if( !lump || !plump_size )
	{
		Com_SkipRestOfLine( wadqc );
		// no fatal error, just ignore this image for adding into wad-archive
		MsgDev( D_ERROR, "Cmd_LoadScript: unable to loading %s\n", lumpname );
		return;
	}

	// write out and release intermediate buffers
	Wad3_AddLump( lump, plump_size, TYPE_SCRIPT, true ); // always compress text files
	Mem_Free( lump );
}

/*
==============
Cmd_GrabProgs

$vprogs filename
==============
*/
void Cmd_GrabProgs( void )
{
	byte		*lump;
	size_t		plump_size;
	dprograms_t	*hdr;

	Com_ReadString( wadqc, SC_PARSE_GENERIC, lumpname );

	// load mip image or replaced with error.bmp
	lump = FS_LoadFile( lumpname, &plump_size );
	
	if( !lump || !plump_size || plump_size < sizeof(dprograms_t))
	{
		Com_SkipRestOfLine( wadqc );
		// no fatal error, just ignore this image for adding into wad-archive
		MsgDev( D_ERROR, "Cmd_LoadProgs: unable to loading %s\n", lumpname );
		return;
	}
	// validate progs
	hdr = (dprograms_t *)lump;

	if( hdr->ident != VPROGSHEADER32 || hdr->version != VPROGS_VERSION )
	{
		// no fatal error, just ignore this image for adding into wad-archive
		MsgDev( D_ERROR, "Cmd_LoadProgs: %s invalid progs version, ignore\n", lumpname );
		Mem_Free( lump );
		return;
	}

	// write out and release intermediate buffers
	Wad3_AddLump( lump, plump_size, TYPE_VPROGS, !hdr->flags ); // release progs may be already packed
	Mem_Free( lump );
}

void Cmd_WadName( void )
{
	string	parm;

	Com_ReadString( wadqc, SC_ALLOW_PATHNAMES2, wadoutname );

	FS_StripExtension( wadoutname );
	FS_DefaultExtension( wadoutname, ".wad" );

	if( Com_ReadString( wadqc, SC_ALLOW_PATHNAMES2, parm ))
		if( !com.stricmp( parm, "append" )) wad_append = true;	
}

/*
==============
Cmd_WadUnknown

syntax: "blabla"
==============
*/
void Cmd_WadUnknown( const char *token )
{
	MsgDev( D_WARN, "Cmd_WadUnknown: skip command \"%s\"\n", token );
	Com_SkipRestOfLine( wadqc );
}

void ResetWADInfo( void )
{
	FS_FileBase( gs_filename, wadoutname );		// kill path and ext
	FS_DefaultExtension( wadoutname, ".wad" );	// set new ext
	handle = NULL;
}

/*
===============
ParseScript	
===============
*/
bool ParseWADfileScript( void )
{
	token_t	token;

	ResetWADInfo();

	while( 1 )
	{
		if(!Com_ReadToken( wadqc, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token ))
			break;

		if( !com.stricmp( token.string, "$wadname" )) Cmd_WadName();
		else if( !com.stricmp( token.string, "$mipmap" )) Cmd_GrabMip();
		else if( !com.stricmp( token.string, "$gfxpic" )) Cmd_GrabPic();
		else if( !com.stricmp( token.string, "$wadqc" )) Cmd_GrabScript();
		else if( !com.stricmp( token.string, "$vprogs" )) Cmd_GrabProgs();
		else if( !Com_ValidScript( token.string, QC_WADLIB )) return false;
		else Cmd_WadUnknown( token.string );
	}
	return true;
}

bool WriteWADFile( void )
{
	if( !handle ) return false;
	WAD_Close( handle );
	Com_CloseScript( wadqc );
	return true;
}

bool BuildCurrentWAD( const char *name )
{
	if( name ) com.strncpy( gs_filename, name, sizeof( gs_filename ));
	FS_DefaultExtension( gs_filename, ".qc" );
	wadqc = Com_OpenScript( gs_filename, NULL, 0 );
	
	if( wadqc )
	{
		if(!ParseWADfileScript())
			return false;
		return WriteWADFile();
	}

	Msg( "%s not found\n", gs_filename );
	return false;
}

bool CompileWad3Archive( byte *mempool, const char *name, byte parms )
{
	if( mempool ) wadpool = mempool;
	else
	{
		Msg( "Wadlib: can't allocate memory pool.\nAbort compilation\n" );
		return false;
	}
	return BuildCurrentWAD( name );	
}