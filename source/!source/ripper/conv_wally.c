//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       conv_pcximage.c - convert pcximages
//=======================================================================

#include "ripper.h"
#include "pal_utils.h"

/*
============
ConvWAL
============
*/
bool ConvWAL( const char *name, char *buffer, int filesize )
{
	wal_t 	wal;
	int	pixels, ofs[4], mipsize;
	int	i, flags, value, contents; // wal additional parms
	string	shadername;
	rgbdata_t	pic;

	if (filesize < (int)sizeof(wal))
	{
		MsgWarn("LoadWAL: file (%s) have invalid size\n", name );
		return false;
	}
	Mem_Copy(&wal, buffer, sizeof(wal));
	memset( &pic, 0, sizeof(pic));

	flags = LittleLong(wal.flags);
	value = LittleLong(wal.value);
	contents = LittleLong(wal.contents);
	pic.width = LittleLong(wal.width);
	pic.height = LittleLong(wal.height);
	for(i = 0; i < 4; i++) ofs[i] = LittleLong(wal.offsets[i]);
	if(!Lump_ValidSize((char *)name, &pic, 512, 512 ))
		return false;

	pixels = pic.width * pic.height;
	pic.size = pixels * 4;
	mipsize = (int)sizeof(wal) + ofs[0] + pixels;
	if( pixels > 256 && filesize < mipsize )
	{
		// note: wal's with dimensions < 32 have invalid sizes.
		MsgWarn("LoadWAL: file (%s) have invalid size\n", name );
		return false;
	}
	pic.buffer = Mem_Alloc( zonepool, pic.size );
	pic.numLayers = 1;
	pic.type = PF_RGBA_32;

	Conv_GetPaletteQ2(); // hardcoded
	Conv_Copy8bitRGBA( buffer + ofs[0], pic.buffer, pixels );
	
	FS_StripExtension( (char *)name );
	FS_SaveImage(va("%s/%s.tga", gs_gamedir, name ), &pic ); // save converted image
	FS_FileBase( name, shadername );
	Conv_CreateShader( name, &pic, "wal", wal.animname, flags, contents );
	Mem_Free( pic.buffer ); // release buffer
	Msg("%s.wal\n", name ); // echo to console about current texture

	return true;
}