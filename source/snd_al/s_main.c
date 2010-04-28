//=======================================================================
//			Copyright XashXT Group 2007 ©
//			   s_main.c - sound engine
//=======================================================================

#include "sound.h"
#include "const.h"
#include "trace_def.h"

#define MAX_CHANNELS	128
#define CSXROOM		29
int MAX_DYNAMIC_CHANNELS	= 24;	// can be reduced by openAL limitations

typedef struct 
{
	int	dwEnvironment;
	float	fVolume;
	float	fDecay;
	float	fDamping;
} dsproom_t;

static soundfade_t	soundfade;
static channel_t	s_channels[MAX_CHANNELS];
static listener_t	s_listener;

const guid_t DSPROPSETID_EAX20_ListenerProperties = {0x306a6a8, 0xb224, 0x11d2, {0x99, 0xe5, 0x0, 0x0, 0xe8, 0xd8, 0xc7, 0x22}};
const guid_t DSPROPSETID_EAX20_BufferProperties = {0x306a6a7, 0xb224, 0x11d2, {0x99, 0xe5, 0x0, 0x0, 0xe8, 0xd8, 0xc7, 0x22}};

const dsproom_t eax_preset[CSXROOM] =
{
{ EAX_ENVIRONMENT_GENERIC,		0.0F,	0.0F,	0.0F	},
{ EAX_ENVIRONMENT_ROOM,		0.417F,	0.4F,	0.666F	},
{ EAX_ENVIRONMENT_BATHROOM,		0.3F,	1.499F,	0.166F	},
{ EAX_ENVIRONMENT_BATHROOM,		0.4F,	1.499F,	0.166F	},
{ EAX_ENVIRONMENT_BATHROOM,		0.6F,	1.499F,	0.166F	},
{ EAX_ENVIRONMENT_SEWERPIPE,		0.4F,	2.886F,	0.25F	},
{ EAX_ENVIRONMENT_SEWERPIPE,		0.6F,	2.886F,	0.25F	},
{ EAX_ENVIRONMENT_SEWERPIPE,		0.8F,	2.886F,	0.25F	},
{ EAX_ENVIRONMENT_STONEROOM,		0.5F,	2.309F,	0.888F	},
{ EAX_ENVIRONMENT_STONEROOM,		0.65F,	2.309F,	0.888F	},
{ EAX_ENVIRONMENT_STONEROOM,		0.8F,	2.309F,	0.888F	},
{ EAX_ENVIRONMENT_STONECORRIDOR,	0.3F,	2.697F,	0.638F	},
{ EAX_ENVIRONMENT_STONECORRIDOR,	0.5F,	2.697F,	0.638F	},
{ EAX_ENVIRONMENT_STONECORRIDOR,	0.65F,	2.697F,	0.638F	},
{ EAX_ENVIRONMENT_UNDERWATER,		1.0F,	1.499F,	0.0F	},
{ EAX_ENVIRONMENT_UNDERWATER,		1.0F,	2.499F,	0.0F	},
{ EAX_ENVIRONMENT_UNDERWATER,		1.0F,	3.499F,	0.0F	},
{ EAX_ENVIRONMENT_GENERIC,		0.65F,	1.493F,	0.5F	},
{ EAX_ENVIRONMENT_GENERIC,		0.85F,	1.493F,	0.5F	},
{ EAX_ENVIRONMENT_GENERIC,		1.0F,	1.493F,	0.5F	},
{ EAX_ENVIRONMENT_ARENA,		0.40F,	7.284F,	0.332F	},
{ EAX_ENVIRONMENT_ARENA,		0.55F,	7.284F,	0.332F	},
{ EAX_ENVIRONMENT_ARENA,		0.70F,	7.284F,	0.332F	},
{ EAX_ENVIRONMENT_CONCERTHALL,	0.5F,	3.961F,	0.5F	},
{ EAX_ENVIRONMENT_CONCERTHALL,	0.7F,	3.961F,	0.5F	},
{ EAX_ENVIRONMENT_CONCERTHALL,	1.0F,	3.961F,	0.5F	},
{ EAX_ENVIRONMENT_DIZZY,		0.2F,	17.234F,	0.666F	},
{ EAX_ENVIRONMENT_DIZZY,		0.3F,	17.234F,	0.666F	},
{ EAX_ENVIRONMENT_DIZZY,		0.4F,	17.234F,	0.666F	},
};

cvar_t	*s_alDevice;
cvar_t	*s_allowEAX;
cvar_t	*s_allowA3D;
cvar_t	*s_check_errors;
cvar_t	*s_volume;	// master volume
cvar_t	*s_musicvolume;	// background track volume
cvar_t	*s_room_type;
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
	if(( err = palGetError( )) == AL_NO_ERROR )
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
		Host_Error( "S_CheckForErrors: %s\n", str );
	else MsgDev( D_ERROR, "S_CheckForErrors: %s\n", str );

	return true;
}

/*
=================
S_GetMasterVolume
=================
*/
float S_GetMasterVolume( void )
{
	float	scale = 1.0f;

	if( si.IsInGame() && soundfade.percent != 0 )
	{
		scale = bound( 0.0f, soundfade.percent / 100.0f, 1.0f );
		scale = 1.0f - scale;
	}
	return s_volume->value * scale;
}

/*
=================
S_FadeClientVolume
=================
*/
void S_FadeClientVolume( float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds )
{
	soundfade.starttime	= si.GetServerTime() * 0.001f;
	soundfade.initial_percent = fadePercent;       
	soundfade.fadeouttime = fadeOutSeconds;    
	soundfade.holdtime = holdTime;   
	soundfade.fadeintime = fadeInSeconds;
}

/*
=================
S_UpdateSoundFade
=================
*/
void S_UpdateSoundFade( void )
{
	float	f, totaltime, elapsed;

	// determine current fade value.
	// assume no fading remains
	soundfade.percent = 0;  

	totaltime = soundfade.fadeouttime + soundfade.fadeintime + soundfade.holdtime;

	elapsed = (si.GetServerTime() * 0.001f) - soundfade.starttime;

	// clock wrapped or reset (BUG) or we've gone far enough
	if( elapsed < 0.0f || elapsed >= totaltime || totaltime <= 0.0f )
		return;

	// We are in the fade time, so determine amount of fade.
	if( soundfade.fadeouttime > 0.0f && ( elapsed < soundfade.fadeouttime ))
	{
		// ramp up
		f = elapsed / soundfade.fadeouttime;
	}
	else if( elapsed <= ( soundfade.fadeouttime + soundfade.holdtime ))	// Inside the hold time
	{
		// stay
		f = 1.0f;
	}
	else
	{
		// ramp down
		f = ( elapsed - ( soundfade.fadeouttime + soundfade.holdtime ) ) / soundfade.fadeintime;
		f = 1.0f - f; // backward interpolated...
	}

	// spline it.
	f = SimpleSpline( f );
	f = bound( 0.0f, f, 1.0f );

	soundfade.percent = soundfade.initial_percent * f;
}

/*
=================
S_IsClient
=================
*/
bool S_IsClient( int entnum )
{
	return ( entnum == al_state.clientnum );
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
		al_state.max_channels++;
	}

	if( al_state.max_channels <= MAX_DYNAMIC_CHANNELS )
	{
		// there are not enough channels!
		MAX_DYNAMIC_CHANNELS = (int)(al_state.max_channels / 3);
		MsgDev( D_WARN, "S_AllocChannels: reduced dynamic channels count from 24 to %i\n", MAX_DYNAMIC_CHANNELS );
	}
	al_state.initialized = true;
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

	for( i = 0, ch = s_channels; i < al_state.max_channels; i++, ch++ )
	{
		palDeleteSources( 1, &ch->sourceNum );
		Mem_Set( ch, 0, sizeof( *ch ));
	}
	al_state.max_channels = 0;
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
	if( sfx == NULL )
	{
		S_StopChannel( ch );
		return;
	}
	ch->sfx = sfx;

	palSourcei( ch->sourceNum, AL_BUFFER, sfx->bufferNum );
	palSourcei( ch->sourceNum, AL_LOOPING, false );
	palSourcei( ch->sourceNum, AL_SOURCE_RELATIVE, false );
	palSourcei( ch->sourceNum, AL_SAMPLE_OFFSET, 0 );

	if( ch->use_loop )
	{
		if( ch->sfx->loopStart >= 0 )
		{
			if( ch->state == CHAN_FIRSTPLAY )
			{
				// play first from start, then from loopOffset
				ch->state = CHAN_LOOPED;
			}
			else if( ch->state == CHAN_LOOPED )
			{
				palSourcei( ch->sourceNum, AL_SAMPLE_OFFSET, sfx->loopStart );
			}
		}
		else
		{
			ch->state = CHAN_LOOPED;
			palSourcei( ch->sourceNum, AL_LOOPING, true );
		}
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
	vec3_t		position, velocity;
	soundinfo_t	SndInfo;
	
	// update position and velocity
	if( ch->entnum == al_state.clientnum || !ch->dist_mult )
	{
		palSourcefv( ch->sourceNum, AL_POSITION, s_listener.position );
		palSourcefv( ch->sourceNum, AL_VELOCITY, s_listener.velocity );
	}
	else
	{
		if( ch->staticsound )
		{
			palSource3f( ch->sourceNum, AL_POSITION, ch->position[1], ch->position[2], -ch->position[0] );
			palSource3f( ch->sourceNum, AL_VELOCITY, 0, 0, 0 );
		}
		else
		{
			SndInfo.pOrigin = position;
			SndInfo.pAngles = velocity;	// FIXME: this is angles, not velocity
			SndInfo.pflRadius = NULL;

			si.GetEntitySpatialization( ch->entnum, &SndInfo );

			palSource3f( ch->sourceNum, AL_POSITION, position[1], position[2], -position[0] );
			palSource3f( ch->sourceNum, AL_VELOCITY, velocity[1], velocity[2], -velocity[0] );
		}
	}

	// update min/max distance
	if( ch->dist_mult )
		palSourcef( ch->sourceNum, AL_REFERENCE_DISTANCE, s_minDistance->value * ch->dist_mult );
	else palSourcef( ch->sourceNum, AL_REFERENCE_DISTANCE, s_maxDistance->value );

	palSourcef( ch->sourceNum, AL_MAX_DISTANCE, s_maxDistance->value );

	// update volume and rolloff factor
	palSourcef( ch->sourceNum, AL_GAIN, ch->volume );
	palSourcef( ch->sourceNum, AL_PITCH, ch->pitch );
	palSourcef( ch->sourceNum, AL_ROLLOFF_FACTOR, s_rolloffFactor->value );
}

/*
=================
SND_PickDynamicChannel

Select a channel from the dynamic channel allocation area.  For the given entity, 
override any other sound playing on the same channel (see code comments below for
exceptions).
=================
*/
channel_t *SND_PickDynamicChannel( int entnum, int channel, sfx_t *sfx )
{
	int	ch_idx;
	int	first_to_die;
	int	life_left;

	// check for replacement sound, or find the best one to replace
	first_to_die = -1;
	life_left = 0x7fffffff;

	for( ch_idx = 0; ch_idx < MAX_DYNAMIC_CHANNELS; ch_idx++ )
	{
		channel_t	*ch = &s_channels[ch_idx];

		// Never override a streaming sound that is currently playing or
		// voice over IP data that is playing or any sound on CHAN_VOICE( acting )
		if( ch->sfx && ( ch->entchannel == CHAN_STREAM ))
		{
			continue;
		}

		if( channel != 0 && ch->entnum == entnum && ( ch->entchannel == channel || channel == -1 ))
		{
			// always override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if( ch->sfx && S_IsClient( ch->entnum ) && !S_IsClient( entnum ))
			continue;

		// don't let monster sounds override player sounds
		if( entnum != al_state.clientnum && ch->entnum == al_state.clientnum )
			continue;

		// replace the oldest sound
		if( ch->startTime < life_left )
		{
			life_left = ch->startTime;
			first_to_die = ch_idx;
		}
	}

	if( first_to_die == -1 )
		return NULL;

	if( s_channels[first_to_die].sfx )
	{
		// don't restart looping sounds for the same entity
		if( sfx->loopStart != -1 )
		{
			channel_t	*ch = &s_channels[first_to_die];

			if( ch->entnum == entnum && ch->entchannel == channel && ch->sfx == sfx )
			{
				// same looping sound, same ent, same channel, don't restart the sound
				return NULL;
			}
		}

		// be sure and release previous channel if sentence.
		S_StopChannel( &( s_channels[first_to_die] ));
	}

	return &s_channels[first_to_die];
}

/*
=====================
SND_PickStaticChannel

Pick an empty channel from the static sound area, or allocate a new
channel.  Only fails if we're at max_channels (128!!!) or if 
we're trying to allocate a channel for a stream sound that is 
already playing.
=====================
*/
channel_t *SND_PickStaticChannel( int entnum, sfx_t *sfx )
{
	channel_t	*ch = NULL;
	int	i;

	// check for replacement sound, or find the best one to replace
 	for( i = MAX_DYNAMIC_CHANNELS; i < al_state.total_channels; i++ )
 	{
		// music or cinematic channel
		if( s_channels[i].entchannel == CHAN_STREAM )
			continue;

		if( s_channels[i].sfx == NULL )
			break;
	}

	if( i < al_state.total_channels ) 
	{
		// reuse an empty static sound channel
		ch = &s_channels[i];
	}
	else
	{
		// no empty slots, alloc a new static sound channel
		if( al_state.total_channels >= al_state.max_channels )
		{
			MsgDev( D_ERROR, "S_PickChannel: no free channels\n" );
			return NULL;
		}
		// get a channel for the static sound
		ch = &s_channels[al_state.total_channels];
		al_state.total_channels++;
	}
	return ch;
}

/*
=================
S_AlterChannel

search through all channels for a channel that matches this
soundsource, entchannel and sfx, and perform alteration on channel
as indicated by 'flags' parameter. If shut down request and
sfx contains a sentence name, shut off the sentence.
returns TRUE if sound was altered,
returns FALSE if sound was not found (sound is not playing)
=================
*/
int S_AlterChannel( int entnum, int channel, sfx_t *sfx, float vol, int pitch, int flags )
{
	channel_t	*ch;
	int	i;	

	if( S_TestSoundChar( sfx->name, '!' ))
	{
		// This is a sentence name.
		// For sentences: assume that the entity is only playing one sentence
		// at a time, so we can just shut off
		// any channel that has ch->isSentence >= 0 and matches the entnum.

		for( i = 0, ch = s_channels; i < al_state.total_channels; i++, ch++ )
		{
			if( ch->entnum == entnum && ch->entchannel == channel && ch->sfx && ch->isSentence )
			{
				if( flags & SND_CHANGE_PITCH )
					ch->pitch = pitch / (float)PITCH_NORM;
				
				if( flags & SND_CHANGE_VOL )
					ch->volume = vol;
				
				if( flags & SND_STOP )
					S_StopChannel( ch );

				return true;
			}
		}
		// channel not found
		return false;

	}

	// regular sound or streaming sound
	for( i = 0, ch = s_channels; i < al_state.total_channels; i++, ch++ )
	{
		if( ch->entnum == entnum && ch->entchannel == channel && ch->sfx )
		{
			if( flags & SND_CHANGE_PITCH )
				ch->pitch = pitch / (float)PITCH_NORM;

			if( flags & SND_CHANGE_VOL )
				ch->volume = vol;

			if( flags & SND_STOP )
				S_StopChannel( ch );

			return true;
		}
	}
	return false;
}

/*
=================
S_StartSound

Validates the parms and queues the sound up.
if origin is NULL, the sound will be dynamically sourced from the entity.
entchannel 0 will never override a playing sound.
=================
*/
void S_StartSound( const vec3_t pos, int ent, int chan, sound_t handle, float fvol, float attn, int pitch, int flags )
{
	sfx_t	*sfx = NULL;
	channel_t	*target_chan;
	int	vol, fsentence = 0;

	sfx = S_GetSfxByHandle( handle );
	if( !sfx ) return;

	vol = bound( 0, fvol * 255, 255 );

	if( flags & ( SND_STOP|SND_CHANGE_VOL|SND_CHANGE_PITCH ))
	{
		if( S_AlterChannel( ent, chan, sfx, vol, pitch, flags ))
			return;

		if( flags & SND_STOP ) return;
		// fall through - if we're not trying to stop the sound, 
		// and we didn't find it (it's not playing), go ahead and start it up
	}

	if( pitch == 0 )
	{
		MsgDev( D_WARN, "S_StartSound: ( %s ) ignored, called with pitch 0\n", sfx->name );
		return;
	}

	if( !pos ) pos = vec3_origin;

	// pick a channel to play on
	target_chan = SND_PickDynamicChannel( ent, chan, sfx );
	if( !target_chan )
	{
		MsgDev( D_ERROR, "dropped sound \"sound/%s\"\n", sfx->name );
		return;
	}

	target_chan->entnum = ent;
	target_chan->entchannel = chan;
	target_chan->startTime = Sys_Milliseconds();
	VectorCopy( pos, target_chan->position );
	target_chan->volume = vol;
	target_chan->entnum = ent;
	target_chan->entchannel = chan;
	target_chan->isSentence = false;
	target_chan->dist_mult = (attn / 1000.0f);
	target_chan->state = CHAN_FIRSTPLAY;
	target_chan->sfx = sfx;
	target_chan->pitch = pitch / (float)PITCH_NORM;

	// make sure the sound is loaded
	if( !S_LoadSound( sfx ))
	{
		S_StopChannel( target_chan );
		return;
	}

	// only using loop for looped sounds
	if( flags & SND_STOP_LOOPING )
		target_chan->use_loop = false;
	else target_chan->use_loop = ( sfx->loopStart == -1 ) ? false : true;

	S_SpatializeChannel( target_chan );
	S_PlayChannel( target_chan, sfx );
}

/*
=================
S_StartStaticSound

Start playback of a sound, loaded into the static portion of the channel array.
Currently, this should be used for looping ambient sounds, looping sounds
that should not be interrupted until complete, non-creature sentences,
and one-shot ambient streaming sounds.  Can also play 'regular' sounds one-shot,
in case designers want to trigger regular game sounds.
Pitch changes playback pitch of wave by % above or below 100.  Ignored if pitch == 100

NOTE: volume is 0.0 - 1.0 and attenuation is 0.0 - 1.0 when passed in.
=================
*/
void S_StaticSound( const vec3_t pos, int ent, int chan, sound_t handle, float fvol, float attn, int pitch, int flags )
{
	channel_t	*ch;
	sfx_t	*sfx = NULL;
	int	vol, fvox = 0;
	
	sfx = S_GetSfxByHandle( handle );
	if( !sfx ) return;

	vol = bound( 0, fvol * 255, 255 );

	if( flags & (SND_STOP|SND_CHANGE_VOL|SND_CHANGE_PITCH))
	{
		if( S_AlterChannel( ent, chan, sfx, vol, pitch, flags ))
			return;
		if( flags & SND_STOP ) return;
	}
	
	if( pitch == 0 )
	{
		MsgDev( D_WARN, "S_StartStaticSound: ( %s ) ignored, called with pitch 0\n", sfx->name );
		return;
	}

	if( !pos ) pos = vec3_origin;

	// pick a channel to play on from the static area
	ch = SND_PickStaticChannel( ent, sfx );	// autolooping sounds are always fixed origin(?)
	if( !ch ) return;

	// make sure the sound is loaded
	if( !S_LoadSound( sfx ))
	{
		S_StopChannel( ch );
		return;
	}

	VectorCopy( pos, ch->position );

	ch->entnum = ent;
	ch->entchannel = chan;
	ch->startTime = Sys_Milliseconds();
	ch->state = CHAN_FIRSTPLAY;
	VectorCopy( pos, ch->position );
	ch->volume = vol;
	ch->entnum = ent;
	ch->entchannel = chan;
	ch->isSentence = false;
	ch->dist_mult = (attn / 1000.0f);
	ch->sfx = sfx;
	ch->pitch = pitch / (float)PITCH_NORM;
	ch->staticsound = true;

	// only using loop for looped sounds
	if( flags & SND_STOP_LOOPING )
		ch->use_loop = false;
	else ch->use_loop = ( sfx->loopStart == -1 ) ? false : true;

	S_SpatializeChannel( ch );
	S_PlayChannel( ch, sfx );
}

/*
=================
S_StartLocalSound

menu sound
=================
*/
bool S_StartLocalSound( const char *name, float volume, int pitch, const float *origin )
{
	sound_t	sfxHandle;

	sfxHandle = S_RegisterSound( name );
	S_StartSound( origin, al_state.clientnum, CHAN_AUTO, sfxHandle, volume, ATTN_NONE, pitch, SND_STOP_LOOPING );
	return true;
}

/*
==================
S_StopAllSounds

stop all sounds for entity on a channel.
==================
*/
void S_StopSound( int entnum, int channel, const char *soundname )
{
	int	i;

	for( i = 0; i < al_state.total_channels; i++ )
	{
		channel_t	*ch = &s_channels[i];

		if( !ch->sfx ) continue; // already freed
		if( ch->entnum == entnum && ch->entchannel == channel )
		{
			if( soundname && com.strcmp( ch->sfx->name, soundname ))
				continue;
			S_StopChannel( ch );
		}
	}
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

	al_state.total_channels = MAX_DYNAMIC_CHANNELS; // no statics

	// Stop all the channels
	for( i = 0, ch = s_channels; i < al_state.max_channels; i++, ch++ )
	{
		if( !ch->sfx ) continue;
		S_StopChannel( ch );
	}

	// clear any remaining soundfade
	Mem_Set( &soundfade, 0, sizeof( soundfade ));
	
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
void S_AddEnvironmentEffects( void )
{
	uint	eaxEnv;

	if( !al_config.allow_3DMode ) return;
          
	// if eax is enabled, apply listener environmental effects
	if( s_listener.waterlevel > 2 )
	{
		eaxEnv = EAX_ENVIRONMENT_UNDERWATER;
	}
	else
	{
		eaxEnv = EAX_ENVIRONMENT_GENERIC;
		eaxEnv = eax_preset[bound( 0, s_room_type->integer, CSXROOM - 1 )].dwEnvironment;
	}
	al_config.Set3DMode( &DSPROPSETID_EAX20_ListenerProperties, DSPROPERTY_EAXLISTENER_ENVIRONMENT|DSPROPERTY_EAXLISTENER_DEFERRED, 0, &eaxEnv, sizeof(eaxEnv));
}

/*
=================
S_Update

Called once each time through the main loop
=================
*/
void S_Update( ref_params_t *fd )
{
	channel_t	*ch;
	int	i;

	if( !fd ) return;

	// bump frame count
	al_state.framecount++;
	al_state.paused = fd->paused;
	al_state.clientnum = fd->viewentity;
	al_state.refdef = fd; // for using everthing else

	// update any client side sound fade
	if( !fd->paused && si.IsInGame( ))
		S_UpdateSoundFade();

	// set up listener
	VectorSet( s_listener.position, fd->simorg[1], fd->simorg[2], -fd->simorg[0] );
	VectorSet( s_listener.velocity, fd->simvel[1], fd->simvel[2], -fd->simvel[0] );

	// set listener orientation matrix
	s_listener.orientation[0] =  fd->forward[1];
	s_listener.orientation[1] = -fd->forward[2];
	s_listener.orientation[2] = -fd->forward[0];
	s_listener.orientation[3] =  fd->up[1];
	s_listener.orientation[4] = -fd->up[2];
	s_listener.orientation[5] = -fd->up[0];
	s_listener.waterlevel = fd->waterlevel;

	palListenerfv( AL_POSITION, s_listener.position );
	palListenerfv( AL_VELOCITY, s_listener.velocity );
	palListenerfv( AL_ORIENTATION, s_listener.orientation );
	palListenerf( AL_GAIN, (al_state.active) ? S_GetMasterVolume () : 0.0f );

	// Set state
	palDistanceModel( AL_INVERSE_DISTANCE_CLAMPED );

	palDopplerFactor( s_dopplerFactor->value );
	palDopplerVelocity( s_dopplerVelocity->value );

	S_AddEnvironmentEffects ();

	// Stream background track
	S_StreamBackgroundTrack ();

	// update spatialization for all sounds
	for( i = 0, ch = s_channels; i < al_state.total_channels; i++, ch++ )
	{
		if( !ch->sfx ) continue; // not active

		// check for stop
		if( ch->use_loop && ch->sfx->loopStart >= 0 )
		{
			int	samplePos;

			palGetSourcei( ch->sourceNum, AL_SAMPLE_OFFSET, &samplePos );

			if( samplePos + ch->sfx->sampleStep >= ch->sfx->samples )
			{
				palSourcei( ch->sourceNum, AL_SAMPLE_OFFSET, ch->sfx->loopStart );
			}
			else if( S_ChannelState( ch ) == AL_STOPPED )
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
	al_state.active = active;
	if( active ) palListenerf( AL_GAIN, S_GetMasterVolume( ));
	else palListenerf( AL_GAIN, 0.0f );
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
	Msg( "\n" );
	Msg( "AL_VENDOR: %s\n", al_config.vendor_string );
	Msg( "AL_RENDERER: %s\n", al_config.renderer_string );
	Msg( "AL_VERSION: %s\n", al_config.version_string );
	Msg( "AL_EXTENSIONS: %s\n", al_config.extensions_string );
	Msg( "\n" );
	Msg( "DEVICE: %s\n", s_alDevice->string );
	Msg( "CHANNELS: %i\n", al_state.max_channels );
	Msg( "3D sound: %s\n", (al_config.allow_3DMode) ? "enabled" : "disabled" );
	Msg( "\n" );
}

/*
=================
 S_Init
=================
*/
bool S_Init( void *hInst )
{
	int	num_mono_src, num_stereo_src;

	Cmd_ExecuteString( "sndlatch\n" );

	s_alDevice = Cvar_Get("s_device", "Generic Software", CVAR_LATCH_AUDIO|CVAR_ARCHIVE, "OpenAL current device name" );
	s_allowEAX = Cvar_Get("s_allowEAX", "1", CVAR_LATCH_AUDIO|CVAR_ARCHIVE, "allow EAX 2.0 extension" );
	s_allowA3D = Cvar_Get("s_allowA3D", "1", CVAR_LATCH_AUDIO|CVAR_ARCHIVE, "allow A3D 2.0 extension" );
	s_check_errors = Cvar_Get("s_check_errors", "1", CVAR_ARCHIVE, "ignore audio engine errors" );
	s_volume = Cvar_Get("s_volume", "1.0", CVAR_ARCHIVE, "sound volume" );
	s_musicvolume = Cvar_Get("s_musicvolume", "1.0", CVAR_ARCHIVE, "background music volume" );
	s_minDistance = Cvar_Get("s_mindistance", "240.0", CVAR_ARCHIVE, "3d sound min distance" );
	s_maxDistance = Cvar_Get("s_maxdistance", "8192.0", CVAR_ARCHIVE, "3d sound max distance" );
	s_rolloffFactor = Cvar_Get("s_rollofffactor", "1.0", CVAR_ARCHIVE, "3d sound rolloff factor" );
	s_dopplerFactor = Cvar_Get("s_dopplerfactor", "1.0", CVAR_ARCHIVE, "cutoff doppler effect value" );
	s_dopplerVelocity = Cvar_Get("s_dopplervelocity", "10976.0", CVAR_ARCHIVE, "doppler effect maxvelocity" );
	s_room_type = Cvar_Get( "room_type", "0", 0, "dsp room type" );

	Cmd_AddCommand( "playsound", S_PlaySound_f, "playing a specified sound file" );
	Cmd_AddCommand( "stopsound", S_StopSound_f, "stop all sounds" );
	Cmd_AddCommand( "music", S_Music_f, "starting a background track" );
	Cmd_AddCommand( "s_info", S_SoundInfo_f, "print sound system information" );
	Cmd_AddCommand( "soundlist", S_SoundList_f, "display loaded sounds" );

	if(!S_Init_OpenAL())
	{
		MsgDev( D_INFO, "S_Init: sound system can't initialized\n" );
		return false;
	}

	palcGetIntegerv( al_state.hDevice, ALC_MONO_SOURCES, sizeof( int ), &num_mono_src );
	palcGetIntegerv( al_state.hDevice, ALC_STEREO_SOURCES, sizeof( int ), &num_stereo_src );
	MsgDev( D_NOTE, "S_Init: mono sources %d, stereo %d\n", num_mono_src, num_stereo_src );

	sndpool = Mem_AllocPool( "Sound Zone" );

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
	Cmd_RemoveCommand( "stopsound" );
	Cmd_RemoveCommand( "music" );
	Cmd_RemoveCommand( "s_info" );
	Cmd_RemoveCommand( "soundlist" );

	S_FreeSounds();
	S_FreeChannels();

	Mem_FreePool( &sndpool );
	S_Free_OpenAL();
}