//=======================================================================
//			Copyright XashXT Group 2007 ©
//			   s_main.c - sound engine
//=======================================================================

#include "sound.h"
#include "const.h"
#include "trace_def.h"

#define MAX_PLAYSOUNDS		256
#define MAX_CHANNELS		64

static playSound_t	s_playSounds[MAX_PLAYSOUNDS];
static playSound_t	s_freePlaySounds;
static playSound_t	s_pendingPlaySounds;
static channel_t s_channels[MAX_CHANNELS];
static listener_t s_listener;

const guid_t DSPROPSETID_EAX20_ListenerProperties = {0x306a6a8, 0xb224, 0x11d2, {0x99, 0xe5, 0x0, 0x0, 0xe8, 0xd8, 0xc7, 0x22}};
const guid_t DSPROPSETID_EAX20_BufferProperties = {0x306a6a7, 0xb224, 0x11d2, {0x99, 0xe5, 0x0, 0x0, 0xe8, 0xd8, 0xc7, 0x22}};

cvar_t	*host_sound;
cvar_t	*s_alDevice;
cvar_t	*s_soundfx;
cvar_t	*s_check_errors;
cvar_t	*s_volume;	// master volume
cvar_t	*s_musicvolume;	// background track volume
cvar_t	*s_pause;
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
bool S_CheckForErrors( void )
{
	int	err;
	char	*str;

	if( !s_check_errors->integer )
		return false;
	if((err = palGetError()) == AL_NO_ERROR)
		return false;

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

	if( al_state.active )
		Host_Error( "S_CheckForErrors: %s", str );
	else MsgDev( D_ERROR, "S_CheckForErrors: %s", str );

	return true;
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
		palGenSources( 1, &ch->sourceNum );
		if( palGetError() != AL_NO_ERROR )
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
		Mem_Set(ch, 0, sizeof(*ch));
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
S_StopChannel
=================
*/
static void S_StopChannel( channel_t *ch )
{
	ch->sfx = NULL;

	palSourceStop( ch->sourceNum );
	palSourcei( ch->sourceNum, AL_BUFFER, 0 );
}

/*
=================
S_PlayChannel
=================
*/
static void S_PlayChannel( channel_t *ch, sfx_t *sfx )
{
	ch->sfx = sfx;

	palSourcei( ch->sourceNum, AL_BUFFER, sfx->bufferNum );
	palSourcei( ch->sourceNum, AL_LOOPING, ch->loopsound );
	palSourcei( ch->sourceNum, AL_SOURCE_RELATIVE, false );
	palSourcei( ch->sourceNum, AL_SAMPLE_OFFSET, 0 );

	if( ch->loopstart >= 0 )
	{
		// kill any looping sounds
		if( s_pause->integer )
		{
			palSourceStop( ch->sourceNum );
			return;
		}
		if( ch->state == CHAN_FIRSTPLAY ) ch->state = CHAN_LOOPED;
		else if( ch->state == CHAN_LOOPED ) palSourcei( ch->sourceNum, AL_SAMPLE_OFFSET, sfx->loopstart );
	}
	palSourcePlay( ch->sourceNum );
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
			if( ch->loopsound ) si.GetSoundSpatialization( ch->loopnum, position, velocity );
			else si.GetSoundSpatialization( ch->entnum, position, velocity );

			palSource3f( ch->sourceNum, AL_POSITION, position[1], position[2], -position[0] );
			palSource3f( ch->sourceNum, AL_VELOCITY, velocity[1], velocity[2], -velocity[0] );
		}
	}

	// update min/max distance
	if( ch->distanceMult )
		palSourcef( ch->sourceNum, AL_REFERENCE_DISTANCE, s_minDistance->value * ch->distanceMult );
	else palSourcef( ch->sourceNum, AL_REFERENCE_DISTANCE, s_maxDistance->value );

	palSourcef( ch->sourceNum, AL_MAX_DISTANCE, s_maxDistance->value );

	// update volume and rolloff factor
	palSourcef( ch->sourceNum, AL_GAIN, ch->volume );
	palSourcef( ch->sourceNum, AL_PITCH, ch->pitch );
	palSourcef( ch->sourceNum, AL_ROLLOFF_FACTOR, s_rolloffFactor->value );
}

/*
=================
S_PickChannel

Tries to find a free channel, or tries to replace an active channel
=================
*/
channel_t *S_PickChannel( int entnum, int channel )
{
	channel_t		*ch;
	int		i;
	int		firstToDie = -1;
	float		oldestTime = Sys_DoubleTime();

	if( entnum < 0 || channel < 0 ) return NULL; // invalid channel or entnum

	for( i = 0, ch = s_channels; i < al_state.num_channels; i++, ch++ )
	{
		// don't let game sounds override streaming sounds
		if( ch->streaming ) continue;

		// check if this channel is active
		if( channel == CHAN_AUTO && !ch->sfx )
		{
			// free channel
			firstToDie = i;
			break;
		}

		// channel 0 never overrides
		if( channel != CHAN_AUTO && (ch->entnum == entnum && ch->entchannel == channel))
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
	ch->entchannel = channel;
	ch->startTime = Sys_DoubleTime();
	ch->state = CHAN_NORMAL;	// remove any loop sound
	ch->loopsound = false;	// clear loopstate
	ch->loopstart = -1;	

	// make sure this channel is stopped
	S_StopChannel( ch );

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
	if( !sfx || !sfx->loaded || sfx->default_sound )
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
		MsgDev( D_ERROR, "dropped sound \"sound/%s\"\n", sfx->name );
		return false;
	}

	ch->loopsound = true;
	ch->loopnum = entnum;
	ch->loopframe = al_state.framecount;
	ch->fixedPosition = false;
	ch->volume = 1.0f;
	ch->pitch = 1.0f;
	ch->distanceMult = 1.0f / ATTN_STATIC;

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
		if( ps->beginTime > Sys_DoubleTime())
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

		// check for looping sounds with "cue " marker
		if( ps->use_loop && ps->sfx->loopstart >= 0 )
		{
			// jump to loopstart at next playing
			ch->state = CHAN_FIRSTPLAY;
			ch->loopstart = ps->sfx->loopstart;
		}

		ch->fixedPosition = ps->fixedPosition;
		VectorCopy( ps->position, ch->position );
		ch->volume = ps->volume;
		ch->pitch = ps->pitch;

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
void S_StartSound( const vec3_t pos, int entnum, int channel, sound_t handle, float vol, float attn, float pitch, bool use_loop )
{
	playSound_t	*ps, *sort;
	sfx_t		*sfx = NULL;

	if(!al_state.initialized )
		return;
	sfx = S_GetSfxByHandle( handle );
	if( !sfx ) return;

	// Make sure the sound is loaded
	if( !S_LoadSound( sfx ))
		return;

	// Allocate a playSound
	ps = S_AllocPlaySound();
	if( !ps )
	{
		if( sfx->name[0] == '#' ) MsgDev( D_ERROR, "dropped sound %s\n", &sfx->name[1] );
		else MsgDev( D_ERROR, "dropped sound \"sound/%s\"\n", sfx->name );
		return;
	}

	ps->sfx = sfx;
	ps->entnum = entnum;
	ps->entchannel = channel;
	ps->use_loop = use_loop;

	if( pos )
	{
		ps->fixedPosition = true;
		VectorCopy( pos, ps->position );
	}
	else ps->fixedPosition = false;
	
	ps->volume = vol;
	ps->pitch = pitch / PITCH_NORM;
	ps->attenuation = attn;
	ps->beginTime = Sys_DoubleTime();

	// sort into the pending playSounds list
	for( sort = s_pendingPlaySounds.next; sort != &s_pendingPlaySounds && sort->beginTime < ps->beginTime; sort = sort->next );

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
bool S_StartLocalSound( const char *name, float volume, float pitch, const float *origin )
{
	sound_t	sfxHandle;

	if( !al_state.initialized )
		return false;

	sfxHandle = S_RegisterSound( name );
	S_StartSound( origin, al_state.clientnum, CHAN_AUTO, sfxHandle, volume, ATTN_NONE, pitch, false );
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
	Mem_Set( s_playSounds, 0, sizeof(s_playSounds));

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
	if( si.PointContents((float *)position ) & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER))
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
	//if( s_pause->integer ) return;		

	// bump frame count
	al_state.framecount++;
	al_state.clientnum = clientnum;

	// set up listener
	VectorSet( s_listener.position, position[1], position[2], -position[0] );
	VectorSet( s_listener.velocity, velocity[1], velocity[2], -velocity[0] );

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
	palListenerf(AL_GAIN, (al_state.active) ? s_volume->value : 0.0f );

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
		if( !ch->sfx ) continue; // not active

		// check for stop
		if( ch->loopsound )
		{
			if( ch->loopframe != al_state.framecount )
			{
				S_StopChannel( ch );
				continue;
			}
		}
		else if( ch->loopstart >= 0 )
		{
			if( S_ChannelState( ch ) == AL_STOPPED )
			{
				S_PlayChannel( ch, ch->sfx );
			}
		}
		else if( S_ChannelState( ch ) == AL_STOPPED )
		{
			S_StopChannel( ch );
			continue;
		}

		// respatialize channel
		S_SpatializeChannel( ch );
	}

	// check for errors
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
	if( active ) palListenerf( AL_GAIN, s_volume->value );
	else palListenerf( AL_GAIN, 0.0 );
}

/*
=================
S_Play_f
=================
*/
void S_PlaySound_f( void )
{
	if( Cmd_Argc() == 1 )
	{
		Msg( "Usage: playsound <soundfile>\n" );
		return;
	}
	S_StartLocalSound( Cmd_Argv( 1 ), 1.0f, PITCH_NORM, NULL );
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
	else Msg( "Usage: music <musicfile> [loopfile]\n" );
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
bool S_Init( void *hInst )
{
	int	num_mono_src, num_stereo_src;

	host_sound = Cvar_Get("host_sound", "1", CVAR_SYSTEMINFO, "enable sound system" );
	s_alDevice = Cvar_Get("s_device", "Generic Software", CVAR_LATCH|CVAR_ARCHIVE, "OpenAL current device name" );
	s_soundfx = Cvar_Get("s_soundfx", "1", CVAR_LATCH|CVAR_ARCHIVE, "allow OpenAl extensions" );
	s_check_errors = Cvar_Get("s_check_errors", "1", CVAR_ARCHIVE, "ignore audio engine errors" );
	s_volume = Cvar_Get("s_volume", "1.0", CVAR_ARCHIVE, "sound volume" );
	s_musicvolume = Cvar_Get("s_musicvolume", "1.0", CVAR_ARCHIVE, "background music volume" );
	s_minDistance = Cvar_Get("s_mindistance", "240.0", CVAR_ARCHIVE, "3d sound min distance" );
	s_maxDistance = Cvar_Get("s_maxdistance", "8192.0", CVAR_ARCHIVE, "3d sound max distance" );
	s_rolloffFactor = Cvar_Get("s_rollofffactor", "1.0", CVAR_ARCHIVE, "3d sound rolloff factor" );
	s_dopplerFactor = Cvar_Get("s_dopplerfactor", "1.0", CVAR_ARCHIVE, "cutoff doppler effect value" );
	s_dopplerVelocity = Cvar_Get("s_dopplervelocity", "10976.0", CVAR_ARCHIVE, "doppler effect maxvelocity" );
	s_pause = Cvar_Get( "paused", "0", 0, "sound engine pause" );

	Cmd_AddCommand( "playsound", S_PlaySound_f, "playing a specified sound file" );
	Cmd_AddCommand( "music", S_Music_f, "starting a background track" );
	Cmd_AddCommand( "s_stop", S_StopSound_f, "stop all sounds" );
	Cmd_AddCommand( "s_info", S_SoundInfo_f, "print sound system information" );
	Cmd_AddCommand( "soundlist", S_SoundList_f, "display loaded sounds" );

	if( !host_sound->integer )
	{
		MsgDev( D_INFO, "Audio: disabled\n" );
		return false;
	}

	if(!S_Init_OpenAL())
	{
		MsgDev( D_INFO, "S_Init: sound system can't initialized\n" );
		return false;
	}

	palcGetIntegerv( al_state.hDevice, ALC_MONO_SOURCES, sizeof(int), &num_mono_src );
	palcGetIntegerv( al_state.hDevice, ALC_STEREO_SOURCES, sizeof(int), &num_stereo_src );
	MsgDev( D_INFO, "mono sources %d, stereo %d\n", num_mono_src, num_stereo_src );

	sndpool = Mem_AllocPool( "Sound Zone" );
	al_state.initialized = true;

	S_AllocChannels();
	S_StopAllSounds();

	// initialize error catched
	if(S_CheckForErrors())
		return false;

	al_state.active = true; // enabled

	return true;
}

/*
=================
S_Shutdown
=================
*/
void S_Shutdown( void )
{

	Cmd_RemoveCommand( "playsound" );
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