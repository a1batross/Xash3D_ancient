//=======================================================================
//			Copyright XashXT Group 2007 ©
//			   s_main.c - sound engine
//=======================================================================

#include "sound.h"

cvar_t	*s_initSound;
cvar_t	*s_show;
cvar_t	*s_alDevice;
cvar_t	*s_allowExtensions;
cvar_t	*s_soundfx;
cvar_t	*s_ignoreALErrors;
cvar_t	*s_masterVolume;
cvar_t	*s_sfxVolume;
cvar_t	*s_musicVolume;
cvar_t	*s_minDistance;
cvar_t	*s_maxDistance;
cvar_t	*s_rolloffFactor;
cvar_t	*s_dopplerFactor;
cvar_t	*s_dopplerVelocity;

#define MAX_PLAYSOUNDS		128

#define MAX_CHANNELS		64

static playSound_t	s_playSounds[MAX_PLAYSOUNDS];
static playSound_t	s_freePlaySounds;
static playSound_t	s_pendingPlaySounds;

static channel_t	s_channels[MAX_CHANNELS];
static int			s_numChannels;

static listener_t	s_listener;

static int			s_frameCount;

const guid_t DSPROPSETID_EAX20_ListenerProperties = {0x306a6a8, 0xb224, 0x11d2, {0x99, 0xe5, 0x0, 0x0, 0xe8, 0xd8, 0xc7, 0x22}};
const guid_t DSPROPSETID_EAX20_BufferProperties = {0x306a6a7, 0xb224, 0x11d2, {0x99, 0xe5, 0x0, 0x0, 0xe8, 0xd8, 0xc7, 0x22}};

cvar_t	*s_initSound;
cvar_t	*s_show;
cvar_t	*s_alDriver;
cvar_t	*s_alDevice;
cvar_t	*s_allowExtensions;
cvar_t	*s_ext_eax;
cvar_t	*s_ignoreALErrors;
cvar_t	*s_masterVolume;
cvar_t	*s_sfxVolume;
cvar_t	*s_musicVolume;
cvar_t	*s_minDistance;
cvar_t	*s_maxDistance;
cvar_t	*s_rolloffFactor;
cvar_t	*s_dopplerFactor;
cvar_t	*s_dopplerVelocity;


/*
=================
S_CheckForErrors
=================
*/
static void S_CheckForErrors( void )
{
	int		err;
	char	*str;

	if((err = palGetError()) == AL_NO_ERROR)
		return;

	switch( err )
	{
	case AL_INVALID_NAME:
		str = "AL_INVALID_NAME";
		break;
	case AL_INVALID_ENUM:
		str = "AL_INVALID_ENUM";
		break;
	case AL_INVALID_VALUE:
		str = "AL_INVALID_VALUE";
		break;
	case AL_INVALID_OPERATION:
		str = "AL_INVALID_OPERATION";
		break;
	case AL_OUT_OF_MEMORY:
		str = "AL_OUT_OF_MEMORY";
		break;
	default:
		str = "UNKNOWN ERROR";
		break;
	}
	Host_Error( "S_CheckForErrors: %s", str );
}

/*
=================
S_AllocChannels
=================
*/
static void S_AllocChannels( void )
{
	channel_t		*ch;
	int		i;

	for( i = 0, ch = s_channels; i < MAX_CHANNELS; i++, ch++)
	{
		palGenSources(1, &ch->sourceNum);
		if(palGetError() != AL_NO_ERROR)
			break;
		al_state.num_channels++;
	}
}

/*
=================
S_FreeChannels
=================
*/
static void S_FreeChannels( void )
{
	channel_t	*ch;
	int	i;

	for( i = 0, ch = s_channels; i < al_state.num_channels; i++, ch++ )
	{
		palDeleteSources(1, &ch->sourceNum);
		memset(ch, 0, sizeof(*ch));
	}
	al_state.num_channels = 0;
}

/*
=================
S_ChannelState
=================
*/
static int S_ChannelState( channel_t *ch )
{
	int		state;

	palGetSourcei( ch->sourceNum, AL_SOURCE_STATE, &state );
	return state;
}

/*
=================
S_PlayChannel
=================
*/
static void S_PlayChannel( channel_t *ch, sfx_t *sfx )
{
	ch->sfx = sfx;

	palSourcei(ch->sourceNum, AL_BUFFER, sfx->bufferNum);
	palSourcei(ch->sourceNum, AL_LOOPING, ch->loopsound);
	palSourcei(ch->sourceNum, AL_SOURCE_RELATIVE, false);
	palSourcePlay(ch->sourceNum);
}

/*
=================
S_StopChannel
=================
*/
static void S_StopChannel( channel_t *ch )
{
	ch->sfx = NULL;

	palSourceStop(ch->sourceNum);
	palSourcei(ch->sourceNum, AL_BUFFER, 0);
}

/*
=================
S_SpatializeChannel
=================
*/
static void S_SpatializeChannel( channel_t *ch )
{
	vec3_t	position, velocity;

	// update position and velocity
	if( ch->entnum == al_state.clientnum || !ch->distanceMult )
	{
		palSourcefv(ch->sourceNum, AL_POSITION, s_listener.position);
		palSourcefv(ch->sourceNum, AL_VELOCITY, s_listener.velocity);
	}
	else
	{
		if( ch->fixedPosition )
		{
			palSource3f(ch->sourceNum, AL_POSITION, ch->position[1], ch->position[2], -ch->position[0]);
			palSource3f(ch->sourceNum, AL_VELOCITY, 0, 0, 0);
		}
		else
		{
			if( ch->loopsound ) si.GetSoundSpatialization(ch->loopnum, position, velocity);
			else si.GetSoundSpatialization( ch->entnum, position, velocity);

			palSource3f(ch->sourceNum, AL_POSITION, position[1], position[2], -position[0]);
			palSource3f(ch->sourceNum, AL_VELOCITY, velocity[1], velocity[2], -velocity[0]);
		}
	}

	// Update min/max distance
	if( ch->distanceMult )
		palSourcef(ch->sourceNum, AL_REFERENCE_DISTANCE, s_minDistance->value * ch->distanceMult );
	else palSourcef(ch->sourceNum, AL_REFERENCE_DISTANCE, s_maxDistance->value );

	palSourcef( ch->sourceNum, AL_MAX_DISTANCE, s_maxDistance->value );

	// update volume and rolloff factor
	palSourcef( ch->sourceNum, AL_GAIN, s_sfxVolume->value * ch->volume );
	palSourcef( ch->sourceNum, AL_ROLLOFF_FACTOR, s_rolloffFactor->value );
}

/*
=================
S_PickChannel

Tries to find a free channel, or tries to replace an active channel
=================
*/
channel_t *S_PickChannel( int entnum, int entChannel )
{
	channel_t		*ch;
	int		i;
	int		firstToDie = -1;
	int		oldestTime = si.GetClientTime();

	if( entnum < 0 || entChannel < 0 )
		Host_Error( "S_PickChannel: entnum or entChannel less than 0" );

	for( i = 0, ch = s_channels; i < al_state.num_channels; i++, ch++ )
	{
		// don't let game sounds override streaming sounds
		if( ch->streaming )continue;

		// check if this channel is active
		if( !ch->sfx )
		{
			// free channel
			firstToDie = i;
			break;
		}

		// channel 0 never overrides
		if( entChannel != 0 && (ch->entnum == entnum && ch->entchannel == entChannel))
		{
			// always override sound from same entity
			firstToDie = i;
			break;
		}

		// don't let monster sounds override player sounds
		if( entnum != al_state.clientnum && ch->entnum == al_state.clientnum )
			continue;

		// replace the oldest sound
		if( ch->startTime < oldestTime )
		{
			oldestTime = ch->startTime;
			firstToDie = i;
		}
	}

	if( firstToDie == -1 ) return NULL;
	ch = &s_channels[firstToDie];

	ch->entnum = entnum;
	ch->entchannel = entChannel;
	ch->startTime = si.GetClientTime();

	// Make sure this channel is stopped
	palSourceStop(ch->sourceNum);
	palSourcei(ch->sourceNum, AL_BUFFER, 0);

	return ch;
}

/*
=================
S_AddLoopingSounds

Entities with a sound field will generate looping sounds that are
automatically started and stopped as the entities are sent to the
client
=================
*/
bool S_AddLoopingSound( int entnum, sound_t handle, float volume, float attn )
{
	channel_t		*ch;
	sfx_t		*sfx = NULL;
	int		i;

	if(!al_state.initialized )
		return false;
	sfx = S_GetSfxByHandle( handle );

	// default looped sound it's terrible :)
	if( !sfx || !sfx->loaded || sfx->default_snd )
		return false;

	// if this entity is already playing the same sound effect on an
	// active channel, then simply update it
	for( i = 0, ch = s_channels; i < al_state.num_channels; i++, ch++ )
	{
		if( ch->sfx != sfx ) continue;
		if( !ch->loopsound ) continue;
		if( ch->loopnum != entnum ) continue;
		if( ch->loopframe + 1 != al_state.framecount )
			continue;

		ch->loopframe = al_state.framecount;
		break;
	}
	if( i != al_state.num_channels )
		return false;

	// otherwise pick a channel and start the sound effect
	ch = S_PickChannel( 0, 0 );
	if( !ch )
	{
		if( sfx->name[0] == '#' ) MsgDev(D_ERROR, "dropped sound %s\n", &sfx->name[1] );
		else MsgDev( D_ERROR, "dropped sound \"sound/%s\"\n", sfx->name );
		return false;
	}

	ch->loopsound = true;
	ch->loopnum = entnum;
	ch->loopframe = al_state.framecount;
	ch->fixedPosition = false;
	ch->volume = 1.0;
	ch->distanceMult = 1.0 / ATTN_STATIC;

	S_SpatializeChannel( ch );
	S_PlayChannel( ch, sfx );
	return true;
}

/*
=================
S_AllocPlaySound
=================
*/
static playSound_t *S_AllocPlaySound( void )
{
	playSound_t	*ps;

	ps = s_freePlaySounds.next;
	if( ps == &s_freePlaySounds )
		return NULL; // No free playSounds

	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;

	return ps;
}

/*
=================
S_FreePlaySound
=================
*/
static void S_FreePlaySound( playSound_t *ps )
{
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;

	// add to free list
	ps->next = s_freePlaySounds.next;
	s_freePlaySounds.next->prev = ps;
	ps->prev = &s_freePlaySounds;
	s_freePlaySounds.next = ps;
}

/*
 =================
S_IssuePlaySounds

Take all the pending playSounds and begin playing them.
This is never called directly by S_Start*, but only by the update loop.
=================
*/
static void S_IssuePlaySounds( void )
{
	playSound_t	*ps;
	channel_t		*ch;

	while( 1 )
	{
		ps = s_pendingPlaySounds.next;
		if(ps == &s_pendingPlaySounds)
			break; // no more pending playSounds

		if( ps->beginTime > si.GetClientTime())
			break;	// No more pending playSounds this frame

		// pick a channel and start the sound effect
		ch = S_PickChannel( ps->entnum, ps->entchannel );
		if(!ch)
		{
			if( ps->sfx->name[0] == '#' ) MsgDev(D_ERROR, "dropped sound %s\n", &ps->sfx->name[1] );
			else MsgDev( D_ERROR, "dropped sound \"sound/%s\"\n", ps->sfx->name );
			S_FreePlaySound( ps );
			continue;
		}

		ch->loopsound = false;
		ch->fixedPosition = ps->fixedPosition;
		VectorCopy( ps->position, ch->position );
		ch->volume = ps->volume;

		if( ps->attenuation != ATTN_NONE ) ch->distanceMult = 1.0 / ps->attenuation;
		else ch->distanceMult = 0.0;

		S_SpatializeChannel( ch );
		S_PlayChannel( ch, ps->sfx );

		// free the playSound
		S_FreePlaySound( ps );
	}
}

/*
=================
S_StartSound

Validates the parms and queues the sound up.
if origin is NULL, the sound will be dynamically sourced from the entity.
entchannel 0 will never override a playing sound.
=================
*/
void S_StartSound( const vec3_t position, int entnum, int entChannel, sound_t handle, float volume, float attenuation )
{
	playSound_t	*ps, *sort;
	sfx_t		*sfx = NULL;

	if(!al_state.initialized )
		return;

	sfx = S_GetSfxByHandle( handle );
	if( !sfx ) return;

	// Make sure the sound is loaded
	if(!S_LoadSound(sfx)) return;

	// Allocate a playSound
	ps = S_AllocPlaySound();
	if(!ps)
	{
		if( sfx->name[0] == '#' ) MsgDev(D_ERROR, "dropped sound %s\n", &sfx->name[1] );
		else MsgDev( D_ERROR, "dropped sound \"sound/%s\"\n", sfx->name );
		return;
	}

	ps->sfx = sfx;
	ps->entnum = entnum;
	ps->entchannel = entChannel;

	if( position )
	{
		ps->fixedPosition = true;
		VectorCopy( position, ps->position );
	}
	else ps->fixedPosition = false;

	ps->volume = volume;
	ps->attenuation = attenuation;
	ps->beginTime = si.GetClientTime();

	// Sort into the pending playSounds list
	for( sort = s_pendingPlaySounds.next; sort != &s_pendingPlaySounds&& sort->beginTime < ps->beginTime; sort = sort->next );

	ps->next = sort;
	ps->prev = sort->prev;
	ps->next->prev = ps;
	ps->prev->next = ps;
}

/*
=================
S_StartLocalSound

menu sound
=================
*/
bool S_StartLocalSound( const char *name )
{
	sound_t	sfxHandle;

	if(!al_state.initialized)
		return false;

	sfxHandle = S_RegisterSound( name );
	S_StartSound( NULL, al_state.clientnum, 0, sfxHandle, 1.0f, ATTN_NONE );
	return true;
}

/*
=================
S_StopAllSounds
=================
*/
void S_StopAllSounds( void )
{
	channel_t	*ch;
	int	i;

	if(!al_state.initialized)
		return;

	// Clear all the playSounds
	memset( s_playSounds, 0, sizeof(s_playSounds));

	s_freePlaySounds.next = s_freePlaySounds.prev = &s_freePlaySounds;
	s_pendingPlaySounds.next = s_pendingPlaySounds.prev = &s_pendingPlaySounds;

	for( i = 0; i < MAX_PLAYSOUNDS; i++ )
	{
		s_playSounds[i].prev = &s_freePlaySounds;
		s_playSounds[i].next = s_freePlaySounds.next;
		s_playSounds[i].prev->next = &s_playSounds[i];
		s_playSounds[i].next->prev = &s_playSounds[i];
	}

	// Stop all the channels
	for( i = 0, ch = s_channels; i < al_state.num_channels; i++, ch++ )
	{
		if( !ch->sfx ) continue;
		S_StopChannel( ch );
	}
	
	S_StopStreaming();		// stop streaming channel
	S_StopBackgroundTrack();	// stop background track
	al_state.framecount = 0;	// reset frame count
}

/*
=================
S_AddEnvironmentEffects

process all effects here
=================
*/
void S_AddEnvironmentEffects( const vec3_t position )
{
	uint	eaxEnv;

	if( !al_config.allow_3DMode ) return;

	// if eax is enabled, apply listener environmental effects
	if( si.PointContents((float *)position ) & MASK_WATER )
		eaxEnv = EAX_ENVIRONMENT_UNDERWATER;
	else eaxEnv = EAX_ENVIRONMENT_GENERIC;
	al_config.Set3DMode(&DSPROPSETID_EAX20_ListenerProperties, DSPROPERTY_EAXLISTENER_ENVIRONMENT|DSPROPERTY_EAXLISTENER_DEFERRED, 0, &eaxEnv, sizeof(eaxEnv));
}

/*
=================
 S_Update

Called once each time through the main loop
=================
*/
void S_Update( int clientnum, const vec3_t position, const vec3_t velocity, const vec3_t at, const vec3_t up )
{
	channel_t		*ch;
	int		i;

	if(!al_state.initialized ) return;

	// Bump frame count
	al_state.framecount++;
	al_state.clientnum = clientnum;

	// Set up listener
	VectorSet( s_listener.position, position[1], position[2], -position[0]);
	VectorSet( s_listener.velocity, velocity[1], velocity[2], -velocity[0]);

	// set listener orientation matrix
	s_listener.orientation[0] = at[1];
	s_listener.orientation[1] = -at[2];
	s_listener.orientation[2] = -at[0];
	s_listener.orientation[3] = up[1];
	s_listener.orientation[4] = -up[2];
	s_listener.orientation[5] = -up[0];

	palListenerfv(AL_POSITION, s_listener.position);
	palListenerfv(AL_VELOCITY, s_listener.velocity);
	palListenerfv(AL_ORIENTATION, s_listener.orientation);
	palListenerf(AL_GAIN, (al_state.active) ? s_masterVolume->value : 0.0f );

	// Set state
	palDistanceModel( AL_INVERSE_DISTANCE_CLAMPED );

	palDopplerFactor(s_dopplerFactor->value);
	palDopplerVelocity(s_dopplerVelocity->value);

	S_AddEnvironmentEffects( position );

	// Stream background track
	S_StreamBackgroundTrack();

	// Add looping sounds
	si.AddLoopingSounds();

	// Issue playSounds
	S_IssuePlaySounds();

	// update spatialization for all sounds
	for( i = 0, ch = s_channels; i < al_state.num_channels; i++, ch++ )
	{
		if(!ch->sfx) continue; // not active

		// check for stop
		if( ch->loopsound )
		{
			if(ch->loopframe != al_state.framecount)
			{
				S_StopChannel( ch );
				continue;
			}
		}
		else
		{
			if( S_ChannelState( ch ) == AL_STOPPED )
			{
				S_StopChannel( ch );
				continue;
			}
		}

		// respatialize channel
		S_SpatializeChannel( ch );
	}

	// check for errors
	if(!s_ignoreALErrors->integer)
		S_CheckForErrors();
}

/*
=================
S_Activate

Called when the main window gains or loses focus.
The window may have been destroyed and recreated between a deactivate 
and an activate.
=================
*/
void S_Activate( bool active )
{
	if(!al_state.initialized )
		return;

	al_state.active = active;
	if( active ) palListenerf( AL_GAIN, s_masterVolume->value );
	else palListenerf( AL_GAIN, 0.0 );
}

/*
=================
S_Play_f
=================
*/
void S_Play_f( void )
{
	int 	i = 1;
	string	name;

	if( Cmd_Argc() == 1 )
	{
		Msg("Usage: play <soundfile> [...]\n");
		return;
	}
	while ( i < Cmd_Argc())
	{
		if ( !com.strrchr(Cmd_Argv(i), '.')) com.sprintf( name, "%s.wav", Cmd_Argv(1) );
		else com.strncpy( name, Cmd_Argv(i), sizeof(name) );
		S_StartLocalSound( name );
		i++;
	}
}

/*
=================
S_Music_f
=================
*/
void S_Music_f( void )
{
	int	c = Cmd_Argc();

	if( c == 2 ) S_StartBackgroundTrack( Cmd_Argv(1), Cmd_Argv(1) );
	else if( c == 3 ) S_StartBackgroundTrack( Cmd_Argv(1), Cmd_Argv(2) );
	else Msg("Usage: music <musicfile> [loopfile]\n");
}

/*
 =================
 S_StopSound_f
 =================
*/
void S_StopSound_f( void )
{
	S_StopAllSounds();
}

/*
=================
S_SoundInfo_f
=================
*/
void S_SoundInfo_f( void )
{
	if(!al_state.initialized )
	{
		Msg("Sound system not started\n");
		return;
	}

	Msg("\n");
	Msg("AL_VENDOR: %s\n", al_config.vendor_string);
	Msg("AL_RENDERER: %s\n", al_config.renderer_string);
	Msg("AL_VERSION: %s\n", al_config.version_string);
	Msg("AL_EXTENSIONS: %s\n", al_config.extensions_string);
	Msg("\n");
	Msg("DEVICE: %s\n", s_alDevice->string );
	Msg("CHANNELS: %i\n", al_state.num_channels);
	Msg("3D sound: %s\n", (al_config.allow_3DMode) ? "enabled" : "disabled" );
	Msg("\n");
}

/*
=================
 S_Init
=================
*/
void S_Init( void *hInst )
{
	s_initSound = Cvar_Get("s_initsound", "1", CVAR_SYSTEMINFO );
	s_alDevice = Cvar_Get("s_device", "Generic Software", CVAR_LATCH|CVAR_ARCHIVE );
	s_allowExtensions = Cvar_Get("s_allowextensions", "1", CVAR_LATCH|CVAR_ARCHIVE );
	s_soundfx = Cvar_Get("s_soundfx", "1", CVAR_LATCH|CVAR_ARCHIVE );
	s_ignoreALErrors = Cvar_Get("s_ignoreALErrors", "1", CVAR_ARCHIVE );
	s_masterVolume = Cvar_Get("s_volume", "1.0", CVAR_ARCHIVE );
	s_sfxVolume = Cvar_Get("s_soundvolume", "1.0", CVAR_ARCHIVE );
	s_musicVolume = Cvar_Get("s_musicvolume", "1.0", CVAR_ARCHIVE );
	s_minDistance = Cvar_Get("s_mindistance", "240.0", CVAR_ARCHIVE );
	s_maxDistance = Cvar_Get("s_maxdistance", "8192.0", CVAR_ARCHIVE );
	s_rolloffFactor = Cvar_Get("s_rollofffactor", "1.0", CVAR_ARCHIVE );
	s_dopplerFactor = Cvar_Get("s_dopplerfactor", "1.0", CVAR_ARCHIVE );
	s_dopplerVelocity = Cvar_Get("s_dopplervelocity", "10976.0", CVAR_ARCHIVE );

	Cmd_AddCommand("play", S_Play_f, "playing a specified sound file" );
	Cmd_AddCommand("music", S_Music_f, "starting a background track" );
	Cmd_AddCommand("s_stop", S_StopSound_f, "stop all sounds" );
	Cmd_AddCommand("s_info", S_SoundInfo_f, "print sound system information" );
	Cmd_AddCommand("soundlist", S_SoundList_f, "display loaded sounds" );

	if(!s_initSound->integer)
	{
		MsgDev(D_INFO, "S_Init: sound system disabled\n" );
		return;
	}

	if(!S_Init_OpenAL())
	{
		MsgDev( D_INFO, "S_Init: sound system can't initialized\n");
		return;
	}

	sndpool = Mem_AllocPool("Sound Zone");
	al_state.initialized = true;

	S_AllocChannels();
	S_StopAllSounds();

	if(!s_ignoreALErrors->integer)
		S_CheckForErrors();

	al_state.active = true; // enabled
}

/*
 =================
 S_Shutdown
 =================
*/
void S_Shutdown( void )
{

	Cmd_RemoveCommand( "play" );
	Cmd_RemoveCommand( "music" );
	Cmd_RemoveCommand( "s_stop" );
	Cmd_RemoveCommand( "s_info" );
	Cmd_RemoveCommand( "soundlist" );

	if( !al_state.initialized )
		return;

	S_FreeSounds();
	S_FreeChannels();

	Mem_FreePool( &sndpool );
	S_Free_OpenAL();
	al_state.initialized = false;
}