//=======================================================================
//			Copyright XashXT Group 2010 ©
//			snd_utils.c - sound common tools
//=======================================================================

#include "soundlib.h"
#include "mathlib.h"

/*
=============================================================================

	XASH3D LOAD IMAGE FORMATS

=============================================================================
*/
// stub
static const loadwavformat_t load_null[] =
{
{ NULL, NULL, NULL }
};

// version0 - using only ogg sounds
static const loadwavformat_t load_stalker[] =
{
{ "sound/%s%s.%s", "ogg", Sound_LoadOGG },
{ "%s%s.%s", "ogg", Sound_LoadOGG },
{ NULL, NULL, NULL }
};

// version1 - using only Doom1 sounds
static const loadwavformat_t load_doom1[] =
{
{ "%s%s.%s", "snd", Sound_LoadSND },
{ NULL, NULL, NULL }
};

// version2 - using only Quake1 sounds
static const loadwavformat_t load_quake1[] =
{
{ "sound/%s%s.%s", "wav", Sound_LoadWAV },
{ "%s%s.%s", "wav", Sound_LoadWAV },
{ NULL, NULL, NULL }
};

// version11 - Xash3D default sound profile
static const loadwavformat_t load_xash[] =
{
{ "sound/%s%s.%s", "wav", Sound_LoadWAV },
{ "%s%s.%s", "wav", Sound_LoadWAV },
{ "sound/%s%s.%s", "ogg", Sound_LoadOGG },
{ "%s%s.%s", "ogg", Sound_LoadOGG },
{ NULL, NULL, NULL }
};

/*
=============================================================================

	XASH3D SAVE SOUND FORMATS

=============================================================================
*/
// stub
static const savewavformat_t save_null[] =
{
{ NULL, NULL, NULL }
};

// version1 - extract all sounds into pcm wav
static const savewavformat_t save_extragen[] =
{
{ "%s%s.%s", "wav", Sound_SaveWAV },
{ NULL, NULL, NULL }
};

void Sound_Init( void )
{
	// init pools
	Sys.soundpool = Mem_AllocPool( "SoundLib Pool" );

	sound.baseformats = load_xash;

	// install image formats (can be re-install later by Sound_Setup)
	switch( Sys.app_name )
	{
	case HOST_NORMAL:
		Sound_Setup( "default", 0 ); // re-initialized later		
		sound.saveformats = save_null;
		break;
	case HOST_RIPPER:
		sound.loadformats = load_null;
		sound.saveformats = save_extragen;
		break;
	default:	// all other instances not using soundlib or will be reinstalling later
		sound.loadformats = load_null;
		sound.saveformats = save_null;
		break;
	}
	sound.tempbuffer = NULL;
}

void Sound_Setup( const char *formats, const uint flags )
{
	if( flags != -1 ) sound.cmd_flags = flags;
	if( formats == NULL ) return;	// used for change flags only

	MsgDev( D_NOTE, "Sound_Init( %s )\n", formats );

	// reinstall loadformats by magic keyword :)
	if( !com.stricmp( formats, "Xash3D" ) || !com.stricmp( formats, "Xash" ))
	{
		sound.loadformats = load_xash;
	}
	else if( !com.stricmp( formats, "stalker" ) || !com.stricmp( formats, "S.T.A.L.K.E.R" ))
	{
		sound.loadformats = load_stalker;
	}
	else if( !com.stricmp( formats, "Doom1" ) || !com.stricmp( formats, "Doom2" ))
	{
		sound.loadformats = load_doom1;
	}
	else if( !com.stricmp( formats, "Quake1" ))
	{
		sound.loadformats = load_quake1; 
	}
	else if( !com.stricmp( formats, "Quake2" ))
	{
		sound.loadformats = load_quake1;
	}
	else if( !com.stricmp( formats, "Quake3" ))
	{
		sound.loadformats = load_quake1;
	}
	else if( !com.stricmp( formats, "Quake4" ) || !com.stricmp( formats, "Doom3" ))
	{
		sound.loadformats = load_quake1;
	}
	else if( !com.stricmp( formats, "hl1" ) || !com.stricmp( formats, "Half-Life" ))
	{
		sound.loadformats = load_quake1;
	}
	else if( !com.stricmp( formats, "hl2" ) || !com.stricmp( formats, "Half-Life 2" ))
	{
		sound.loadformats = load_quake1;
	}
	else
	{
		sound.loadformats = load_xash; // unrecognized version, use default
	}

	if( Sys.app_name == HOST_RIPPER )
		sound.baseformats = sound.loadformats;
}

void Sound_Shutdown( void )
{
	Mem_Check(); // check for leaks
	Mem_FreePool( &Sys.soundpool );
}

bool Sound_Process( wavdata_t **wav, int rate, int width, uint flags )
{
	// FIXME: not implemented
	return false;
}