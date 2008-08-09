//=======================================================================
//			Copyright XashXT Group 2007 ©
//		  conv_sprite2.c - convert quake2 sprites
//=======================================================================

#include "ripper.h"
#include "qc_gen.h"

bool PCX_ConvertImage( const char *name, char *buffer, int filesize );

/*
========================================================================

.SP2 sprite file format

========================================================================
*/
#define SPRITEQ2_VERSION		2
#define IDSPRQ2HEADER		(('2'<<24)+('S'<<16)+('D'<<8)+'I') // little-endian "IDS2"

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

//
// sprite2_decompiler.c
//
void SP2_ConvertFrame( const char *name, int framenum )
{
	byte		*fin;
	int		filesize;

	// store framename
	FS_FileBase( name, spr.frame[framenum].name ); 

	fin = FS_LoadFile( name, &filesize );
	PCX_ConvertImage( name, fin, filesize );
	Mem_Free( fin ); // release buffer
}

bool SP2_WriteScript( const char *name )
{
	int	i;
	file_t	*f = FS_Open( va("%s/sprites/%s.qc", gs_gamedir, name ), "w" );

	if( !f )
	{
		Msg("Can't write qc-script \"%s.qc\"\n", name );
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
	FS_Printf(f, "$texture\t\t%s\n\n",SPR_RenderMode());

	// frames description
	for( i = 0; i < spr.totalframes - spr.numgroup; i++)
	{
		FS_Printf(f,"$load\t\t%s.bmp\n", spr.frame[i].name );
		FS_Printf(f,"$frame\t\t0 0 %d %d", spr.frame[i].width, spr.frame[i].height );
		if(!spr.frame[i].origin[0] && !spr.frame[i].origin[1]) FS_Print(f, "\n" ); 
		else FS_Printf(f, " %.1f %d %d\n", 0.1f, spr.frame[i].origin[0],spr.frame[i].origin[1]);
	}

	FS_Close(f);
	Msg("%s.sp2\n", name ); // echo to console about current sprite
	return true;
}

/*
============
ConvSP2
============
*/
bool ConvSP2( const char *name, char *buffer, int filesize )
{
	dspriteq2_t	*pin;
	string		scriptname;
	int		i;

	pin = (dspriteq2_t *)buffer;
	memset( &spr, 0, sizeof(spr));

	if( LittleLong(pin->ident) != IDSPRQ2HEADER || LittleLong(pin->version) != SPRITEQ2_VERSION )
	{
		Msg("\"%s.spr\" it's not a Quake2 sprite\n", name );
		return false;
	}
	spr.totalframes = LittleLong (pin->numframes);
	spr.texFormat = SPR_ALPHTEST; // constant
	spr.type = SPR_VP_PARALLEL;

	// byte swap everything
	for(i = 0; i < spr.totalframes; i++)
	{
		spr.frame[i].width = LittleLong(pin->frames[i].width);
		spr.frame[i].height = LittleLong(pin->frames[i].height);
		spr.frame[i].origin[0] = LittleLong(pin->frames[i].origin_x);
		spr.frame[i].origin[1] = LittleLong(pin->frames[i].origin_y);
		SP2_ConvertFrame( pin->frames[i].name, i );
	}

	// write script file and out
	FS_FileBase( name, scriptname );
	return SP2_WriteScript( scriptname );
}