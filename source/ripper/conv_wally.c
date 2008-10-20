//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       conv_pcximage.c - convert pcximages
//=======================================================================

#include "ripper.h"
#include "qc_gen.h"

/*
========================================================================

.WAL image format	(Wally textures)

========================================================================
*/
typedef struct wal_s
{
	char	name[32];
	uint	width, height;
	uint	offsets[4];	// four mip maps stored
	char	animname[32];	// next frame in animation chain
	int	flags;
	int	contents;
	int	value;
} wal_t;

/*
============
ConvWAL
============
*/
bool ConvWAL( const char *name, char *buffer, int filesize )
{
	wal_t 	*wal;
	string	shadername;
	int	flags, value, contents; // wal additional parms
	rgbdata_t	*pic = FS_LoadImage( "#internal.wal", buffer, filesize );

	wal = (wal_t *)buffer;
	contents = LittleLong( wal->contents );
	flags = LittleLong( wal->flags );
	value = LittleLong( wal->value );

	if( pic )
	{
		FS_StripExtension((char *)name );
		FS_SaveImage( va("%s/%s.bmp", gs_gamedir, name ), PF_INDEXED_32, pic ); // save converted image
		FS_FileBase( name, shadername );
		Conv_CreateShader( name, pic, "wal", wal->animname, flags, contents );
		FS_FreeImage( pic ); // release buffer
		Msg("%s.wal\n", name ); // echo to console about current pic
		return true;
	}
	return false;
}