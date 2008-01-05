//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       conv_wadlumps.c - convert wad lumps
//=======================================================================

#include "idconv.h"
#include "mathlib.h"

/*
============
ConvPAL
============
*/
bool ConvPAL( char *name, char *buffer, int filesize )
{
	if( filesize != 768 )
	{
		MsgWarn("LoadPAL: file (%s) have invalid size\n", name );
		return false;
	}

	// not a real image, just palette lump
	Conv_GetPaletteLMP( buffer, LUMP_NORMAL );

	// write palette out ?
	return true;
}

typedef struct angled_s
{
	char	name[10];		// copy of skin name

	int	width;		// lumpwidth
	int	height;		// lumpheight
	int	origin[2];	// monster origin
	byte	xmirrored;	// swap left and right
} angled_t;

struct angledframe_s
{
	angled_t	frame[8];		// angled group or single frame
	byte	angledframes;	// count of angled frames max == 8
	byte	normalframes;	// count of anim frames max == 1

	char	membername[8];	// current model name, four characsters
	char	animation;	// current animation number
	bool	in_progress;	// current state
} qc;

file_t	*f;

void Skin_WriteSequence( void )
{
	int	i;

	// time to dump frames :)
	if( qc.angledframes == 8 )
	{
		// angled group is full, dump it!
		FS_Print(f, "\n$angled\n{\n" );
		for( i = 0; i < 8; i++)
		{
			FS_Printf(f,"\t$load\t\t%s.tga\n", qc.frame[i].name );
			FS_Printf(f,"\t$frame\t\t0 0 %d %d", qc.frame[i].width, qc.frame[i].height );
			FS_Printf(f, " 0.1 %d %d", qc.frame[i].origin[0], qc.frame[i].origin[1]);
			if( qc.frame[i].xmirrored ) FS_Print(f, "\tflip_x\n" ); 
			else FS_Print(f, "\n" );
		}
		FS_Print(f, "}\n" );
	}
	else if( qc.normalframes == 1 )
	{
		Msg("write %s\n", qc.frame[0].name );
	
		// single frame stored
		FS_Print(f, "\n" );
		FS_Printf(f,"$load\t\t%s.tga\n", qc.frame[0].name );
		FS_Printf(f,"$frame\t\t0 0 %d %d", qc.frame[0].width, qc.frame[0].height );
		FS_Printf(f, " 0.1 %d %d\n", qc.frame[0].origin[0], qc.frame[0].origin[1]);
	}

	memset( &qc.frame, 0, sizeof(qc.frame)); 
	qc.angledframes = qc.normalframes = 0; // clear all
}

void Skin_FindSequence( const char *name, rgbdata_t *pic )
{
	int	num, headlen;
	char	header[8];

	// create header from flat name
	com.strncpy( header, name, 9 );
	headlen = com.strlen( name );

	if( qc.animation != header[4] )
	{
		// write animation
		Skin_WriteSequence();
		qc.animation = header[4];
	}

	if( qc.animation == header[4] )
	{
		// continue collect frames
		if( headlen == 6 )
		{
			num = header[5] - '0';
			if(num == 0) qc.normalframes++;	// animation frame
			if(num == 8) num = 0;	// merge 
			qc.angledframes++; 		// angleframe stored
			com.strncpy( qc.frame[num].name, header, 9 );
			qc.frame[num].width = pic->width;
			qc.frame[num].height = pic->height;
			qc.frame[num].origin[0] = pic->width>>1;	// center
			qc.frame[num].origin[1] = pic->height;	// floor
			qc.frame[num].xmirrored = false;
		}
		else if( headlen == 8 )
		{
			// normal image
			num = header[5] - '0';
			if(num == 8) num = 0;  // merge 
			com.strncpy( qc.frame[num].name, header, 9 );
			qc.frame[num].width = pic->width;
			qc.frame[num].height = pic->height;
			qc.frame[num].origin[0] = pic->width>>1;	// center
			qc.frame[num].origin[1] = pic->height;	// floor
			qc.frame[num].xmirrored = false;
			qc.angledframes++; // frame stored

			// mirrored image
			num = header[7] - '0'; // angle it's a direct acess to group
			if(num == 8) num = 0;  // merge 
			com.strncpy( qc.frame[num].name, header, 9 );
			qc.frame[num].width = pic->width;
			qc.frame[num].height = pic->height;
			qc.frame[num].origin[0] = pic->width>>1;	// center
			qc.frame[num].origin[1] = pic->height;	// floor
			qc.frame[num].xmirrored = true;		// it's mirror frame
			qc.angledframes++; // frame stored
		}
		else Sys_Break("Skin_CreateScript: invalid name %s\n", name );
	}
}

void Skin_ProcessScript( const char *name )
{
	if(qc.in_progress )
	{
		// finish script
		Skin_WriteSequence();
		FS_Close( f );
		qc.in_progress = false;
	}
	if(!qc.in_progress )
	{
		// start from scratch
		com.strncpy( qc.membername, name, 5 );		
		f = FS_Open( va("%s/models/%s.qc", gs_gamedir, qc.membername ), "w" );
		qc.in_progress = true;

		// write description
		FS_Print(f,"//=======================================================================\n");
		FS_Print(f,"//\t\t\tCopyright XashXT Group 2007 ©\n");
		FS_Print(f,"//\t\t\twritten by Xash Miptex Decompiler\n");
		FS_Print(f,"//=======================================================================\n");

		// write sprite header
		FS_Printf(f, "\n$spritename\t%s.spr\n", qc.membername );
		FS_Print(f,  "$type\t\tvp_parallel\n"  ); // constant
		FS_Print(f,  "$texture\t\tindexalpha\n");
		Msg("open script %s\n", qc.membername );
	}
}

void Skin_FinalizeScript( void )
{
	if(!qc.in_progress ) return;

	// finish script
	Skin_WriteSequence();
	FS_Close( f );
	qc.in_progress = false;
}

void Skin_CreateScript( const char *name, rgbdata_t *pic )
{
	if(com.strnicmp( name, qc.membername, 4 ))
          	Skin_ProcessScript( name );

	if( qc.in_progress )
		Skin_FindSequence( name, pic );
}

/*
============
ConvFLT
============
*/
rgbdata_t *Conv_FlatImage( const char *name, char *buffer, int filesize )
{
	flat_t	flat;
	vfile_t	*f;
	word	column_loop, row_loop;
	int	i, column_offset, pointer_position, first_pos, pixels;
	byte	*Data, post, topdelta, length;
	rgbdata_t	*pic = Mem_Alloc( zonepool, sizeof(*pic));

	if(filesize < (int)sizeof(flat))
	{
		MsgWarn("LoadFLAT: file (%s) have invalid size\n", name );
		return NULL;
	}

	// stupid copypaste from DevIL, but it works
	f = VFS_Create( buffer, filesize );
	first_pos = VFS_Tell( f );
	VFS_Read(f, &flat, sizeof(flat));

	pic->width  = LittleShort( flat.width );
	pic->height = LittleShort( flat.height );
	flat.desc[0] = LittleShort( flat.desc[0] );
	flat.desc[1] = LittleShort( flat.desc[1] );
	if(!Lump_ValidSize( (char *)name, pic, 256, 256 ))
		return NULL;
	Data = (byte *)Mem_Alloc( zonepool, pic->width * pic->height );
	memset( Data, 247, pic->width * pic->height ); // default mask
	pic->numLayers = 1;
	pic->type = PF_RGBA_32;

	for( column_loop = 0; column_loop < pic->width; column_loop++ )
	{
		VFS_Read(f, &column_offset, sizeof(int));
		pointer_position = VFS_Tell( f );
		VFS_Seek( f, first_pos + column_offset, SEEK_SET );

		while( 1 )
		{
			if(VFS_Read(f, &topdelta, 1) != 1) return NULL;
			if(topdelta == 255) break;
			if(VFS_Read(f, &length, 1) != 1) return NULL;
			if(VFS_Read(f, &post, 1) != 1) return NULL;

			for (row_loop = 0; row_loop < length; row_loop++)
			{
				if(VFS_Read(f, &post, 1) != 1) return NULL;
				if(row_loop + topdelta < pic->height)
					Data[(row_loop + topdelta) * pic->width + column_loop] = post;
			}
			VFS_Read(f, &post, 1);
		}
		VFS_Seek(f, pointer_position, SEEK_SET );
	}
	VFS_Close( f );

	// scan for transparency
	for (i = 0; i < pic->width * pic->height; i++)
	{
		if( Data[i] == 247 )
		{
			pic->flags |= IMAGE_HAS_ALPHA;
			break;
		}
	}

	pixels = pic->width * pic->height;
	pic->size = pixels * 4;
	pic->buffer = (byte *)Mem_Alloc( zonepool, pic->size );

	Conv_GetPaletteD1();
	Conv_Copy8bitRGBA( Data, pic->buffer, pixels );
	Mem_Free( Data );

	return pic;
}

bool ConvSKN( const char *name, char *buffer, int filesize )
{
	rgbdata_t *pic = Conv_FlatImage( name, buffer, filesize );

	if( pic )
	{
		string	skinpath;
		FS_FileBase( name, skinpath );
		FS_SaveImage(va("%s/models/%s.tga", gs_gamedir, skinpath ), pic );
		Skin_CreateScript( skinpath, pic );
		Mem_Free( pic->buffer ); // release buffer
		Mem_Free( pic ); // release buffer
		Msg("%s.skin\n", skinpath ); // echo to console about current skin
		return true;
	}
	return false;
}

bool ConvFLT( const char *name, char *buffer, int filesize )
{
	rgbdata_t *pic = Conv_FlatImage( name, buffer, filesize );

	if( pic )
	{
		FS_StripExtension( (char *)name );
		FS_SaveImage(va("%s/textures/%s.tga", gs_gamedir, name ), pic );
		Conv_CreateShader( name, pic, NULL, 0, 0 );
		Mem_Free( pic->buffer ); // release buffer
		Mem_Free( pic ); // release buffer
		Msg("%s.flat\n", name ); // echo to console about current texture
		return true;
	}
	return false;
}

/*
============
ConvMIP
============
*/
bool ConvMIP( const char *name, char *buffer, int filesize )
{
	mip_t	mip;
	rgbdata_t	pic;
	byte	*fin, *pal;
	int	ofs[4], rendermode;
	int	i, pixels, numcolors;

	if (filesize < (int)sizeof(mip))
	{
		MsgWarn("LoadMIP: file (%s) have invalid size\n", name );
		return false;
	}
	memset( &pic, 0, sizeof(pic));

	Mem_Copy(&mip, buffer, sizeof(mip));
	pic.width = LittleLong(mip.width);
	pic.height = LittleLong(mip.height);
	for(i = 0; i < 4; i++) ofs[i] = LittleLong(mip.offsets[i]);
	pixels = pic.width * pic.height;
	pic.numLayers = 1;
	pic.type = PF_RGBA_32;

	if(!com.stricmp( name, "gfx/conchars.mip" ))
	{
		// greatest hack from id software
		pic.width = pic.height = 128;
		pic.flags |= IMAGE_HAS_ALPHA;
		rendermode = LUMP_QFONT;
		pixels = pic.width * pic.height;
		pal = NULL; // clear palette
		fin = buffer;
	}
	else if(filesize >= (int)sizeof(mip) + ((pixels * 85)>>6) + sizeof(short) + 768)
	{
		// half-life 1.0.0.1 mip version with palette
		fin = buffer + mip.offsets[0];
		pal = buffer + mip.offsets[0] + (((pic.width * pic.height) * 85)>>6);
		numcolors = BuffLittleShort( pal );
		if(numcolors != 256) pal = NULL; // corrupted mip ?
		else  pal += sizeof(short); // skip colorsize 
		// detect rendermode
		if( name[0] == '{' )
		{
			// qlumpy used this color for transparent textures, otherwise it's decals
 			if(pal[255*3+0] == 0 && pal[255*3+1] == 0 && pal[255*3+2] == 255)
				rendermode = LUMP_TRANSPARENT;
			else rendermode = LUMP_DECAL;
			pic.flags |= IMAGE_HAS_ALPHA;
		}
		else rendermode = LUMP_NORMAL;
	}
	else if(filesize >= (int)sizeof(mip) + ((pixels * 85)>>6))
	{
		// quake1 1.01 mip version without palette
		pal = NULL; // clear palette
		rendermode = LUMP_NORMAL;
		fin = buffer + mip.offsets[0];
	}
	else
	{
		MsgWarn("LoadMIP: lump (%s) is corrupted\n", name );
		return false;
	} 

	if(!Lump_ValidSize((char *)name, &pic, 640, 640 )) return false;
	pic.size = pixels * 4;
	pic.buffer = (byte *)Mem_Alloc(zonepool, pic.size );
	Conv_GetPaletteLMP( pal, rendermode );
	Conv_Copy8bitRGBA( fin, pic.buffer, pixels );

	FS_StripExtension( (char *)name );
	FS_SaveImage(va("%s/textures/%s.tga", gs_gamedir, name ), &pic ); // save converted image
	Conv_CreateShader( name, &pic, NULL, 0, 0 );
	Mem_Free( pic.buffer ); // release buffer
	Msg("%s.mip\n", name ); // echo to console about current texture

	return true;
}

/*
============
ConvLMP
============
*/
bool ConvLMP( const char *name, char *buffer, int filesize )
{
	lmp_t	lmp;
	byte	*fin, *pal;
	int	pixels;
	rgbdata_t	pic;

	if (filesize < (int)sizeof(lmp))
	{
		MsgWarn("LoadLMP: file (%s) have invalid size\n", name );
		return false;
	}
	memset( &pic, 0, sizeof(pic));

	fin = buffer;
	Mem_Copy(&lmp, fin, sizeof(lmp));
	pic.width = LittleLong(lmp.width);
	pic.height = LittleLong(lmp.height);
	fin += sizeof(lmp);
	pixels = pic.width * pic.height;

	if(filesize < (int)sizeof(lmp) + pixels)
	{
		MsgWarn("LoadLMP: file (%s) have invalid size %d\n", name, filesize );
		return false;
	}

	if(!Lump_ValidSize((char *)name, &pic, 640, 480 )) return false;         
          pic.size = pixels * 4;
	pic.numMips = 1;

	pic.buffer = (byte *)Mem_Alloc(zonepool, pic.size );
	pic.numLayers = 1;
	pic.type = PF_RGBA_32;

	// half-life 1.0.0.1 lmp version with palette
	if( filesize > (int)sizeof(lmp) + pixels )
	{
		int numcolors;
		pal = fin + pixels;
		numcolors = BuffLittleShort( pal );
		if( numcolors != 256 ) pal = NULL; // corrupted lump ?
		else pal += sizeof(short);
	}
	else pal = NULL;
	if(fin[0] == 255) pic.flags |= IMAGE_HAS_ALPHA;

	pic.numLayers = 1;
	pic.type = PF_RGBA_32;

	FS_StripExtension((char *)name );
	Conv_GetPaletteLMP( pal, LUMP_NORMAL );
	Conv_Copy8bitRGBA( fin, pic.buffer, pixels );
	FS_SaveImage(va("%s/graphics/%s.tga", gs_gamedir, name ), &pic ); // save converted image
	Mem_Free( pic.buffer ); // release buffer
	Msg("%s.lmp\n", name ); // echo to console about current image

	return true;
}

/*
============
ConvFNT
============
*/
bool ConvFNT( const char *name, char *buffer, int filesize )
{
	qfont_t	font;
	byte	*pal, *fin;
	int	i, pixels, fullsize;
	int	numcolors;
	rgbdata_t	pic;

	if(filesize < (int)sizeof(font))
	{
		MsgWarn("LoadFNT: file (%s) have invalid size\n", name );
		return false;
	}
	Mem_Copy(&font, buffer, sizeof(font));

	// swap lumps
	font.width = LittleShort(font.width);
	font.height = LittleShort(font.height);
	font.rowcount = LittleShort(font.rowcount);
	font.rowheight = LittleShort(font.rowheight);
	for(i = 0; i < 256; i++)
	{
		font.fontinfo[i].startoffset = LittleShort( font.fontinfo[i].startoffset ); 
		font.fontinfo[i].charwidth = LittleShort( font.fontinfo[i].charwidth );
	}
	
	// last sixty four bytes - what the hell ????
	fullsize = sizeof(qfont_t)-4+(128 * font.width * QCHAR_WIDTH) + sizeof(short) + 768 + 64;

	if( fullsize != filesize )
	{
		// oldstyle font: "conchars" or "creditsfont"
		pic.width = 256; // hardcoded
		pic.height = font.height;
	}
	else
	{
		// Half-Life 1.1.0.0 font style (qfont_t)
		pic.width = font.width * QCHAR_WIDTH;
		pic.height = font.height;
	}

	pic.numLayers = 1;
	pic.type = PF_RGBA_32;
	if(!Lump_ValidSize((char *)name, &pic, 512, 512 )) return false;
	fin = buffer + sizeof(font) - 4;

	pixels = pic.width * pic.height;
	pal = fin + pixels;
	numcolors = BuffLittleShort( pal ), pal += sizeof(short);
	pic.flags |= IMAGE_HAS_ALPHA; // fonts always have transparency

	if( numcolors == 768 )
	{
		// newstyle font
		Conv_GetPaletteLMP( pal, LUMP_QFONT );
	}
	else if( numcolors == 256 )
	{
		// oldstyle font
		Conv_GetPaletteLMP( pal, LUMP_TRANSPARENT );
	}
	else 
	{
		MsgWarn("LoadFNT: file (%s) have invalid palette size %d\n", name, numcolors );
		return false;
	}

	pixels = pic.width * pic.height;
	pic.size = pixels * 4;
	pic.buffer = (byte *)Mem_Alloc(zonepool, pic.size );
	pic.numLayers = 1;
	pic.type = PF_RGBA_32;

	FS_StripExtension((char *)name );
	Conv_Copy8bitRGBA( fin, pic.buffer, pixels );
	FS_SaveImage(va("%s/graphics/%s.tga", gs_gamedir, name ), &pic ); // save converted image
	Mem_Free( pic.buffer ); // release buffer
	Msg("%s.qfont\n", name ); // echo to console about current font

	return true;
}