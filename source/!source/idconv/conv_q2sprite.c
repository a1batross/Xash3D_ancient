//=======================================================================
//			Copyright XashXT Group 2007 ©
//		  conv_q2sprite.c - convert quake2 sprites
//=======================================================================

#include "idconv.h"

/*
========================================================================

.SP2 sprite file format

========================================================================
*/
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

int sprq2_width = 1;
int sprq2_height = 1;
int sprq2_numframes = 1;
int sprq2_sequence = 0;
float sprq2_framerate = 15.0f;
string *sprq2_framenames;
vec2_t *sprq2_origin;

const char *SP2_RenderMode( void )
{
	return "decal";
}

const char *SP2_RenderType( void )
{
	return "vp_parallel";
}

void SP2_ConvertFrame( const char *name, int framenum )
{
	byte		*fin;
	int		filesize;

	// store framename
	com.strncpy( sprq2_framenames[framenum], name, MAX_STRING );
	FS_StripExtension( sprq2_framenames[framenum] ); 

	fin = FS_LoadFile( name, &filesize );
	PCX_ConvertImage( va("%s", sprq2_framenames[framenum] ), fin, filesize );
	Mem_Free( fin ); // release buffer
}

bool SP2_WriteScript( const char *name )
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
	FS_Printf(f,"\nheader \"%s\"\n{\n\ttype\t\"%s\"\n\trender\t\"%s\"\n",name,SP2_RenderType(),SP2_RenderMode());
	FS_Printf(f,"\tframerate\t\"%.1f\"\n\tsequence\t\"%d\"\n}\n", sprq2_framerate, sprq2_sequence );

	// frames description
	FS_Printf(f,"\nframes \"%s\"\n{\n", name );
	
	for( i = 0; i < sprq2_numframes; i++)
	{
		// because q2 sprites already have "sprites/" in the path
		FS_Printf(f,"\tframe\t\"%s\"", sprq2_framenames[i] );
		if(!sprq2_origin[i][0] && !sprq2_origin[i][1]) FS_Print(f, "\n" ); 
		else FS_Printf(f, "\t%d\t%d\n", (int)sprq2_origin[i][0], (int)sprq2_origin[i][1] );
	}
	FS_Print(f,"}\n" );
	FS_Printf(f,"\nsequence 0\n{\n\tpattern\t" ); // default sequence

	if(sprq2_numframes > 1) FS_Printf(f,"\"%d..%d\"", 0, sprq2_numframes - 1 );
	else FS_Print(f,"\"0\"" );
	FS_Print(f,"\n}\n" );

	FS_Close(f);
	Mem_Free( sprq2_framenames );
	Mem_Free( sprq2_origin );
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
	int		i;

	pin = (dspriteq2_t *)buffer;

	if( LittleLong(pin->ident) != IDSPRQ2HEADER || LittleLong(pin->version) != SPRITE_VERSION )
	{
		Msg("\"%s.spr\" it's not a Quake2 sprite\n", name );
		return false;
	}

	sprq2_numframes = LittleLong (pin->numframes);
	sprq2_framenames = Mem_Alloc( zonepool, sprq2_numframes * MAX_STRING );
	sprq2_origin = Mem_Alloc( zonepool, sprq2_numframes * sizeof(vec2_t));

	// byte swap everything
	for(i = 0; i < sprq2_numframes; i++)
	{
		sprq2_origin[i][0] = (float)LittleLong(pin->frames[i].origin_x);
		sprq2_origin[i][1] = (float)LittleLong(pin->frames[i].origin_y);
		SP2_ConvertFrame( pin->frames[i].name, i );
	}

	// write script file and out
	return SP2_WriteScript( name );
}