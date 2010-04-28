//=======================================================================
//			Copyright XashXT Group 2009 ©
//			   s_main.c - sound engine
//=======================================================================

#include "sound.h"
#include "const.h"

#define SND_TRACE_UPDATE_MAX  	2		// max of N channels may be checked for obscured source per frame
#define SND_RADIUS_MAX		(20.0f * 12.0f)	// max sound source radius
#define SND_RADIUS_MIN		(2.0f * 12.0f)	// min sound source radius
#define SND_OBSCURED_LOSS_DB		-2.70f		// dB loss due to obscured sound source

// calculate gain based on atmospheric attenuation.
// as gain excedes threshold, round off (compress) towards 1.0 using spline
#define SND_GAIN_COMP_EXP_MAX		2.5f		// Increasing SND_GAIN_COMP_EXP_MAX fits compression curve more closely
						// to original gain curve as it approaches 1.0.  
#define SND_GAIN_FADE_TIME		0.25f		// xfade seconds between obscuring gain changes
#define SND_GAIN_COMP_EXP_MIN		0.8f	
#define SND_GAIN_COMP_THRESH		0.5f		// gain value above which gain curve is rounded to approach 1.0
#define SND_DB_MAX			140.0f		// max db of any sound source
#define SND_DB_MED			90.0f		// db at which compression curve changes
#define SND_DB_MIN			60.0f		// min db of any sound source
#define SND_GAIN_PLAYER_WEAPON_DB	2.0f		// increase player weapon gain by N dB

#define DOPPLER_DIST_LEFT_TO_RIGHT	(4 * 12)		// separate left/right sounds by 4'
#define DOPPLER_DIST_MAX		(20 * 12)		// max distance - causes min pitch
#define DOPPLER_DIST_MIN		(1 * 12)		// min distance - causes max pitch
#define DOPPLER_PITCH_MAX		1.5f		// max pitch change due to distance
#define DOPPLER_PITCH_MIN		0.25f		// min pitch change due to distance
#define DOPPLER_RANGE_MAX		(10 * 12)		// don't play doppler wav unless within this range

#define SNDLVL_TO_DIST_MULT( sndlvl ) \
	( sndlvl ? ((pow( 10, s_refdb->value / 20 ) / pow( 10, (float)sndlvl / 20 )) / s_refdist->value ) : 0 )

#define DIST_MULT_TO_SNDLVL( dist_mult ) \
	(int)( dist_mult ? ( 20 * log10( pow( 10, s_refdb->value / 20 ) / (dist_mult * s_refdist->value ))) : 0 )

dma_t		dma;
byte		*sndpool;
static soundfade_t	soundfade;
channel_t   	channels[MAX_CHANNELS];
listener_t	s_listener;
int		total_channels;
int		s_framecount;	// for autosounds checking
int		soundtime;	// sample PAIRS
int   		paintedtime; 	// sample PAIRS
static int	trace_count = 0;
static int	last_trace_chan = 0;

cvar_t		*s_check_errors;
cvar_t		*s_volume;
cvar_t		*s_musicvolume;
cvar_t		*s_khz;
cvar_t		*s_show;
cvar_t		*snd_foliage_db_loss; 
cvar_t		*snd_gain;
cvar_t		*snd_gain_max;
cvar_t		*snd_gain_min;
cvar_t		*s_refdist;
cvar_t		*s_refdb;
cvar_t		*s_mixahead;
cvar_t		*s_primary;
cvar_t		*s_allowEAX;
cvar_t		*s_allowA3D;

/*
=============================================================================

		SOUND COMMON UTILITES

=============================================================================
*/
// dB = 20 log (amplitude/32768)		0 to -90.3dB
// amplitude = 32768 * 10 ^ (dB/20)		0 to +/- 32768
// gain = amplitude/32768			0 to 1.0
_inline float Gain_To_dB( float gain ) { return 20 * log( gain ); }
_inline float dB_To_Gain ( float dB ) { return pow( 10, dB / 20.0f ); }
_inline float Gain_To_Amplitude( float gain ) { return gain * 32768; }
_inline float Amplitude_To_Gain( float amplitude ) { return amplitude / 32768; }

// convert sound db level to approximate sound source radius,
// used only for determining how much of sound is obscured by world
_inline float dB_To_Radius( float db )
{
	return (SND_RADIUS_MIN + (SND_RADIUS_MAX - SND_RADIUS_MIN) * (db - SND_DB_MIN) / (SND_DB_MAX - SND_DB_MIN));
}

/*
=============================================================================

		SOUNDS PROCESSING

=============================================================================
*/
/*
=================
S_GetMasterVolume
=================
*/
float S_GetMasterVolume( void )
{
	float	scale = 1.0f;

	if( s_listener.ingame && soundfade.percent != 0 )
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
S_IsClient
=================
*/
bool S_IsClient( int entnum )
{
	return ( entnum == s_listener.entnum );
}


// free channel so that it may be allocated by the
// next request to play a sound.  If sound is a 
// word in a sentence, release the sentence.
// Works for static, dynamic, sentence and stream sounds
/*
=================
S_FreeChannel
=================
*/
void S_FreeChannel( channel_t *ch )
{
	ch->isSentence = false;
	ch->sfx = NULL;

	SND_CloseMouth( ch );
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
SND_ChannelOkToTrace

All new sounds must traceline once,
but cap the max number of tracelines performed per frame
for longer or looping sounds to SND_TRACE_UPDATE_MAX.
=================
*/
bool SND_ChannelOkToTrace( channel_t *ch )
{
	int 	i, j;

	// always trace first time sound is spatialized
	if( ch->bfirstpass )
		return true;

	// if already traced max channels, return
	if( trace_count >= SND_TRACE_UPDATE_MAX )
		return false;

	// search through all channels starting at g_snd_last_trace_chan index
	j = last_trace_chan;

 	for( i = 0; i < total_channels; i++ )
	{
		if( &( channels[j] ) == ch )
		{
			ch->bTraced = true;
			trace_count++;
			return true;
		}

		// wrap channel index
		j++;
		if( j >= total_channels )
			j = 0;
	}
	
	// why didn't we find this channel?
	return false;			
}


/*
=================
SND_ChannelTraceReset

reset counters for traceline limiting per audio update
=================
*/
void SND_ChannelTraceReset( void )
{
	int	i;

	// reset search point - make sure we start counting from a new spot 
	// in channel list each time
	last_trace_chan += SND_TRACE_UPDATE_MAX;
	
	// wrap at total_channels
	if( last_trace_chan >= total_channels )
		last_trace_chan = last_trace_chan - total_channels;

	// reset traceline counter
	trace_count = 0;

	// reset channel traceline flag
	for( i = 0; i < total_channels; i++ )
		channels[i].bTraced = false; 
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
		channel_t	*ch = &channels[ch_idx];
		
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

		if( ch->end - paintedtime < life_left )
		{
			life_left = ch->end - paintedtime;
			first_to_die = ch_idx;
		}
	}

	if( first_to_die == -1 )
		return NULL;

	if( channels[first_to_die].sfx )
	{
		// don't restart looping sounds for the same entity
		wavdata_t	*sc = channels[first_to_die].sfx->cache;

		if( sc && sc->loopStart != -1 )
		{
			channel_t	*ch = &channels[first_to_die];

			if( ch->entnum == entnum && ch->entchannel == channel && ch->sfx == sfx )
			{
				// same looping sound, same ent, same channel, don't restart the sound
				return NULL;
			}
		}

		// be sure and release previous channel if sentence.
		S_FreeChannel( &( channels[first_to_die] ));
	}

	return &channels[first_to_die];
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
 	for( i = MAX_DYNAMIC_CHANNELS; i < total_channels; i++ )
 	{
		if( channels[i].sfx == NULL )
			break;
	}

	if( i < total_channels ) 
	{
		// reuse an empty static sound channel
		ch = &channels[i];
	}
	else
	{
		// no empty slots, alloc a new static sound channel
		if( total_channels == MAX_CHANNELS )
		{
			MsgDev( D_ERROR, "S_PickChannel: no free channels\n" );
			return NULL;
		}
		// get a channel for the static sound
		ch = &channels[total_channels];
		total_channels++;
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
int S_AlterChannel( int entnum, int channel, sfx_t *sfx, int vol, int pitch, int flags )
{
	channel_t	*ch;
	int	i;	

	if( S_TestSoundChar( sfx->name, '!' ))
	{
		// This is a sentence name.
		// For sentences: assume that the entity is only playing one sentence
		// at a time, so we can just shut off
		// any channel that has ch->isSentence >= 0 and matches the entnum.

		for( i = 0, ch = channels; i < total_channels; i++, ch++ )
		{
			if( ch->entnum == entnum && ch->entchannel == channel && ch->sfx && ch->isSentence )
			{
				if( flags & SND_CHANGE_PITCH )
					ch->basePitch = pitch;
				
				if( flags & SND_CHANGE_VOL )
					ch->master_vol = vol;
				
				if( flags & SND_STOP )
					S_FreeChannel( ch );

				return true;
			}
		}
		// channel not found
		return false;

	}

	// regular sound or streaming sound
	for( i = 0, ch = channels; i < total_channels; i++, ch++ )
	{
		if( ch->entnum == entnum && ch->entchannel == channel && ch->sfx )
		{
			if( flags & SND_CHANGE_PITCH )
				ch->basePitch = pitch;
				
			if( flags & SND_CHANGE_VOL )
				ch->master_vol = vol;

			if( flags & SND_STOP )
				S_FreeChannel( ch );

			return true;
		}
	}
	return false;
}

// calculate point of closest approach - caller must ensure that the 
// forward facing vector of the entity playing this sound points in exactly the direction of 
// travel of the sound. ie: for bullets or tracers, forward vector must point in traceline direction.
// return true if sound is to be played, false if sound cannot be heard (shot away from player)
bool SND_GetClosestPoint( channel_t *pChannel, vec3_t source_angles, vec3_t vnearpoint )
{
	vec3_t	SF;	// sound source forward direction unit vector
	vec3_t	SL;	// sound -> listener vector
	vec3_t	SD;	// sound->closest point vector
	float	dSLSF;	// magnitude of project of SL onto SF

	// S - sound source origin
	// L - listener origin
	// P = SF (SF . SL) + S

	// only perform this calculation for doppler wavs
	if( !pChannel->doppler_effect )
		return false;

	// get vector 'SL' from sound source to listener
	VectorSubtract( s_listener.origin, pChannel->origin, SL );

	// compute sound->forward vector 'SF' for sound entity
	AngleVectors( source_angles, SF, NULL, NULL );
	VectorNormalize( SF );
	
	dSLSF = DotProduct( SL, SF );

	if( dSLSF <= 0 )
	{
		// source is pointing away from listener, don't play anything.
		return false;
	}
		
	// project dSLSF along forward unit vector from sound source
	VectorScale( SF, dSLSF, SD );

	// output vector - add SD to sound source origin
	VectorAdd( SD, pChannel->origin, vnearpoint );

	return true;
}

// given point of nearest approach and sound source facing angles, 
// return vector pointing into quadrant in which to play 
// doppler left wav (incomming) and doppler right wav (outgoing).
// doppler left is point in space to play left doppler wav
// doppler right is point in space to play right doppler wav
// Also modifies channel pitch based on distance to nearest approach point
void SND_GetDopplerPoints( channel_t *pChannel, vec3_t source_angles, vec3_t vnearpoint, vec3_t source_doppler_left, vec3_t source_doppler_right )
{
	vec3_t	SF;	// direction sound source is facing (forward)
	vec3_t	LN;	// vector from listener to closest approach point
	vec3_t	DL;
	vec3_t	DR;
	float	pitch, dist;

	// nearpoint is closest point of approach, when playing CHAR_DOPPLER sounds
	// SF is normalized vector in direction sound source is facing

	AngleVectors( source_angles, SF, NULL, NULL );
	VectorNormalize( SF );

	// source_doppler_left - location in space to play doppler left wav (incomming)
	// source_doppler_right - location in space to play doppler right wav (outgoing)
	VectorScale( SF, ( -1.0f * DOPPLER_DIST_LEFT_TO_RIGHT ), DL );
	VectorScale( SF, DOPPLER_DIST_LEFT_TO_RIGHT, DR );

	VectorAdd( vnearpoint, DL, source_doppler_left );
	VectorAdd( vnearpoint, DR, source_doppler_right );
	
	// set pitch of channel based on nearest distance to listener
	// LN is vector from listener to closest approach point
	VectorSubtract( vnearpoint, s_listener.origin, LN );

	dist = VectorLength( LN );
	
	// dist varies 0->1
	dist = bound( DOPPLER_DIST_MIN, dist, DOPPLER_DIST_MAX );
	dist = ( dist - DOPPLER_DIST_MIN ) / ( DOPPLER_DIST_MAX - DOPPLER_DIST_MIN );

	// pitch varies from max to min
	pitch = DOPPLER_PITCH_MAX - dist * ( DOPPLER_PITCH_MAX - DOPPLER_PITCH_MIN );
	pChannel->basePitch = (int)(pitch * 100.0f);
}

// always ramp channel gain changes over time
// returns ramped gain, given new target gain
float SND_FadeToNewGain( channel_t *ch, float gain_new )
{
	float	speed, frametime;

	if( gain_new == -1.0 )
	{
		// if -1 passed in, just keep fading to existing target
		gain_new = ch->ob_gain_target;
	}

	// if first time updating, store new gain into gain & target, return
	// if gain_new is close to existing gain, store new gain into gain & target, return
	if( ch->bfirstpass || ( fabs( gain_new - ch->ob_gain ) < 0.01f ))
	{
		ch->ob_gain = gain_new;
		ch->ob_gain_target = gain_new;
		ch->ob_gain_inc = 0.0f;
		return gain_new;
	}

	// set up new increment to new target
	frametime = s_listener.frametime;
	speed = ( frametime / SND_GAIN_FADE_TIME ) * ( gain_new - ch->ob_gain );

	ch->ob_gain_inc = fabs( speed );

	// ch->ob_gain_inc = fabs( gain_new - ch->ob_gain ) / 10.0f;
	ch->ob_gain_target = gain_new;

	// if not hit target, keep approaching
	if( fabs( ch->ob_gain - ch->ob_gain_target ) > 0.01f )
	{
		ch->ob_gain = ApproachVal( ch->ob_gain_target, ch->ob_gain, ch->ob_gain_inc );
	}
	else
	{
		// close enough, set gain = target
		ch->ob_gain = ch->ob_gain_target;
	}
	return ch->ob_gain;
}

// drop gain on channel if sound emitter obscured by
// world, unbroken windows, closed doors, large solid entities etc.
float SND_GetGainObscured( channel_t *ch, bool fplayersound, bool flooping )
{
	float	gain = 1.0f;
	vec3_t	endpoint;
	int	count = 1;
	trace_t	tr;

	if( fplayersound )
		return gain;

	// during signon just apply regular state machine since world hasn't been
	// created or settled yet...
	if( !si.IsActive( ))
	{
		gain = SND_FadeToNewGain( ch, -1.0f );
		return gain;
	}

	// don't do gain obscuring more than once on short one-shot sounds
	if( !ch->bfirstpass && !ch->isSentence && !flooping && ( ch->entchannel != CHAN_STREAM ))
	{
		gain = SND_FadeToNewGain( ch, -1.0f );
		return gain;
	}

	// if long or looping sound, process N channels per frame - set 'processed' flag, clear by
	// cycling through all channels - this maintains a cap on traces per frame
	if( !SND_ChannelOkToTrace( ch ))
	{
		// just keep updating fade to existing target gain - no new trace checking
		gain = SND_FadeToNewGain( ch, -1.0 );
		return gain;
	}

	// set up traceline from player eyes to sound emitting entity origin
	VectorCopy( ch->origin, endpoint );

	tr = si.TraceLine( s_listener.vieworg, endpoint );

	if(( tr.flFraction < 1.0f || tr.fAllSolid || tr.fStartSolid ) && tr.flFraction < 0.99f )
	{
		// can't see center of sound source:
		// build extents based on dB sndlvl of source,
		// test to see how many extents are visible,
		// drop gain by g_snd_obscured_loss_db per extent hidden
		vec3_t	endpoints[4];
		int	i, sndlvl = DIST_MULT_TO_SNDLVL( ch->dist_mult );
		float	radius;
		vec3_t	vecl, vecr, vecl2, vecr2;
		vec3_t	vsrc_forward;
		vec3_t	vsrc_right;
		vec3_t	vsrc_up;

		// get radius
		if( ch->radius > 0 ) radius = ch->radius;
		else radius = dB_To_Radius( sndlvl ); // approximate radius from soundlevel
		
		// set up extent endpoints - on upward or downward diagonals, facing player

		for( i = 0; i < 4; i++ )
			VectorCopy( endpoint, endpoints[i] );

		// vsrc_forward is normalized vector from sound source to listener
		VectorSubtract( s_listener.origin, endpoint, vsrc_forward );
		VectorNormalize( vsrc_forward );
		VectorVectors( vsrc_forward, vsrc_right, vsrc_up );

		VectorAdd( vsrc_up, vsrc_right, vecl );
		
		// if src above listener, force 'up' vector to point down - create diagonals up & down
		if( endpoint[2] > s_listener.origin[2] + ( 10 * 12 ))
			vsrc_up[2] = -vsrc_up[2];

		VectorSubtract( vsrc_up, vsrc_right, vecr );
		VectorNormalize( vecl );
		VectorNormalize( vecr );

		// get diagonal vectors from sound source 
		VectorScale( vecl, radius, vecl2 );
		VectorScale( vecr, radius, vecr2 );
		VectorScale( vecl, (radius / 2.0f), vecl );
		VectorScale( vecr, (radius / 2.0f), vecr );

		// endpoints from diagonal vectors
		VectorAdd( endpoints[0], vecl, endpoints[0] );
		VectorAdd( endpoints[1], vecr, endpoints[1] );
		VectorAdd( endpoints[2], vecl2, endpoints[2] );
		VectorAdd( endpoints[3], vecr2, endpoints[3] );

		// drop gain for each point on radius diagonal that is obscured
		for( count = 0, i = 0; i < 4; i++ )
		{
			// UNDONE: some endpoints are in walls - in this case, trace from the wall hit location
			tr = si.TraceLine( s_listener.vieworg, endpoints[i] );
				
			if(( tr.flFraction < 1.0f || tr.fAllSolid || tr.fStartSolid ) && tr.flFraction < 0.99f && !tr.fStartSolid )
			{
				count++;	// skip first obscured point: at least 2 points + center should be obscured to hear db loss
				if( count > 1 ) gain = gain * dB_To_Gain( SND_OBSCURED_LOSS_DB );
			}
		}
	}

	// crossfade to new gain
	gain = SND_FadeToNewGain( ch, gain );

	return gain;
}

// The complete gain calculation, with SNDLVL given in dB is:
// GAIN = 1/dist * snd_refdist * 10 ^ ( ( SNDLVL - snd_refdb - (dist * snd_foliage_db_loss / 1200)) / 20 )
// for gain > SND_GAIN_THRESH, start curve smoothing with
// GAIN = 1 - 1 / (Y * GAIN ^ SND_GAIN_POWER)
// where Y = -1 / ( (SND_GAIN_THRESH ^ SND_GAIN_POWER) * ( SND_GAIN_THRESH - 1 ))
// gain curve construction
float SND_GetGain( channel_t *ch, bool fplayersound, bool flooping, float dist )
{
	float	gain = snd_gain->value;

	if( ch->dist_mult )
	{
		// test additional attenuation
		// at 30c, 14.7psi, 60% humidity, 1000Hz == 0.22dB / 100ft.
		// dense foliage is roughly 2dB / 100ft
		float additional_dB_loss = snd_foliage_db_loss->value * (dist / 1200);
		float additional_dist_mult = pow( 10, additional_dB_loss / 20);
		float relative_dist = dist * ch->dist_mult * additional_dist_mult;

		// hard code clamp gain to 10x normal (assumes volume and external clipping)
		if( relative_dist > 0.1f )
		{
			gain *= ( 1.0f / relative_dist );
		}
		else gain *= 10.0f;

		// if gain passess threshold, compress gain curve such that gain smoothly approaches 1.0
		if( gain > SND_GAIN_COMP_THRESH )
		{
			float	snd_gain_comp_power = SND_GAIN_COMP_EXP_MAX;
			int	sndlvl = DIST_MULT_TO_SNDLVL( ch->dist_mult );
			float	Y;
			
			// decrease compression curve fit for higher sndlvl values
			if( sndlvl > SND_DB_MED )
			{
				// snd_gain_power varies from max to min as sndlvl varies from 90 to 140
				snd_gain_comp_power = RemapVal((float)sndlvl, SND_DB_MED, SND_DB_MAX, SND_GAIN_COMP_EXP_MAX, SND_GAIN_COMP_EXP_MIN );
			}

			// calculate crossover point
			Y = -1.0f / ( pow( SND_GAIN_COMP_THRESH, snd_gain_comp_power ) * ( SND_GAIN_COMP_THRESH - 1 ));
			
			// calculate compressed gain

			gain = 1.0f - 1.0f / (Y * pow( gain, snd_gain_comp_power ));
			gain = gain * snd_gain_max->value;
		}

		if( gain < snd_gain_min->value )
		{
			// sounds less than snd_gain_min fall off to 0 in distance it took them to fall to snd_gain_min

			gain = snd_gain_min->value * ( 2.0f - relative_dist * snd_gain_min->value );
			if( gain <= 0.0f ) gain = 0.001f; // don't propagate 0 gain
		}
	}

	if( fplayersound )
	{
		// player weapon sounds get extra gain - this compensates
		// for npc distance effect weapons which mix louder as L+R into L,R
		// Hack.
		if( ch->entchannel == CHAN_WEAPON )
			gain = gain * dB_To_Gain( SND_GAIN_PLAYER_WEAPON_DB );
	}

	// modify gain if sound source not visible to player
	gain = gain * SND_GetGainObscured( ch, fplayersound, flooping );

	return gain; 
}

void S_SpatializeChannel( int *left_vol, int *right_vol, int master_vol, float gain, float dotRight )
{
	float	lscale, rscale, scale;

	rscale = 1.0f + dotRight;
	lscale = 1.0f - dotRight;

	// add in distance effect
	scale = gain * rscale / 2;
	*right_vol = (int)( master_vol * scale );

	scale = gain * lscale / 2;
	*left_vol = (int)( master_vol * scale );

	*right_vol = bound( 0, *right_vol, 255 );
	*left_vol = bound( 0, *left_vol, 255 );

}

/*
=================
SND_Spatialize
=================
*/
void SND_Spatialize( channel_t *ch )
{
	float		dotRight = 0;
	float		dotFront = 0;
	float		dist;
	vec3_t		ent_origin;
	vec3_t		source_vec;
	vec3_t		source_vec_DL;
	vec3_t		source_vec_DR;
	vec3_t		source_doppler_left;
	vec3_t		source_doppler_right;
	vec3_t		source_angles;
	float		dotRightDL = 0;
	float		dotRightDR = 0;
	bool		fdopplerwav = false;
	bool		fplaydopplerwav = false;
	bool		fvalidentity;
	soundinfo_t	SndInfo;
	wavdata_t		*pSource;
	float		gain;
	bool		fplayersound = false;
	bool		looping = false;
	
	if( ch->entnum == s_listener.entnum )
	{
		// sounds coming from listener actually come from a short distance directly in front of listener
		fplayersound = true;
	}

	// update channel's position in case ent that made the sound is moving.

	VectorClear( source_angles );
	VectorCopy( ch->origin, ent_origin );

	pSource = ch->sfx ? ch->sfx->cache : NULL;

	if( pSource )
	{
		looping = (pSource->loopStart == -1) ? false : true;
	}

	SndInfo.pOrigin = ent_origin;
	SndInfo.pAngles = source_angles;
	SndInfo.pflRadius = NULL;

	fvalidentity = si.GetEntitySpatialization( ch->entnum, &SndInfo );

	if( ch->staticsound )
	{
		VectorAngles( ch->direction, source_angles );
	}
	else
	{
		AngleVectors( source_angles, ch->direction, NULL, NULL );
		VectorCopy( ent_origin, ch->origin );
	}

	// turn off the sound while the entity doesn't exist or is not in the PVS.
	if( !fvalidentity ) goto ClearAllVolumes;
	
	fdopplerwav = ( ch->doppler_effect  && !fplayersound );

	if( fdopplerwav )
	{
		vec3_t	vnearpoint;		// point of closest approach to listener, 
						// along sound source forward direction (doppler wavs)
		VectorCopy( ch->origin, vnearpoint );	// default nearest sound approach point

		// calculate point of closest approach for CHAR_DOPPLER wavs, replace source_vec
		fplaydopplerwav = SND_GetClosestPoint( ch, source_angles, vnearpoint );

		// if doppler sound was 'shot' away from listener, don't play it
		if( !fplaydopplerwav ) goto ClearAllVolumes;

		// find location of doppler left & doppler right points
		SND_GetDopplerPoints( ch, source_angles, vnearpoint, source_doppler_left, source_doppler_right );
	
		// source_vec_DL is vector from listener to doppler left point
		// source_vec_DR is vector from listener to doppler right point
		VectorSubtract( source_doppler_left, s_listener.origin, source_vec_DL );
		VectorSubtract( source_doppler_right, s_listener.origin, source_vec_DR );

		// normalized vectors to left and right doppler locations
		dist = VectorNormalizeLength( source_vec_DL );
		VectorNormalize( source_vec_DR );

		// don't play doppler if out of range
		if( dist > DOPPLER_RANGE_MAX ) goto ClearAllVolumes;
	}
	else
	{
		// source_vec is vector from listener to sound source
		// player sounds come from 1' in front of player
		if( fplayersound ) VectorScale( s_listener.forward, 12.0f, source_vec );
		else VectorSubtract( ch->origin, s_listener.origin, source_vec );

		// normalize source_vec and get distance from listener to source
		dist = VectorNormalizeLength( source_vec );
	}

	// get right and front dot products for all sound sources
	// dot: 1.0 - source is directly to right of listener, -1.0 source is directly left of listener
	if( fdopplerwav )
	{
		// get right/front dot products for left doppler position
		dotRightDL = DotProduct( s_listener.right, source_vec_DL );

		// get right/front dot products for right doppler position
		dotRightDR = DotProduct( s_listener.right, source_vec_DR );
	}
	else
	{
		dotRight = DotProduct( s_listener.right, source_vec );
	}

	// for sounds with a radius, spatialize left/right/front/rear evenly within the radius
	if( ch->radius > 0 && dist < ch->radius && !fdopplerwav )
	{
		float	interval = ch->radius * 0.5f;
		float	blend = dist - interval;

		if( blend < 0 ) blend = 0;
		blend /= interval;	

		// blend is 0.0 - 1.0, from 50% radius -> 100% radius

		// at radius * 0.5, dotRight is 0 (ie: sound centered left/right)
		// at radius dotRight == dotRight
		dotRight *= blend;
		dotRightDL *= blend;
		dotRightDR *= blend;
	}

	// calculate gain based on distance, atmospheric attenuation, interposed objects
	// perform compression as gain approaches 1.0
	gain = SND_GetGain( ch, fplayersound, looping, dist );
	
	// don't pan sounds with no attenuation
	if( ch->dist_mult <= 0 && !fdopplerwav )
	{
		// sound is centered left/right/front/back
		dotRight = 0.0f;
		dotRightDL = 0.0f;
		dotRightDR = 0.0f;
	}

	if( fdopplerwav )
	{
		// fill out channel volumes for both doppler locations
		S_SpatializeChannel( &ch->leftvol, &ch->rightvol, ch->master_vol, gain, dotRightDL );
		S_SpatializeChannel( &ch->dleftvol, &ch->drightvol, ch->master_vol, gain, dotRightDR );		
	}
	else
	{
		// fill out channel volumes for single location
		S_SpatializeChannel( &ch->leftvol, &ch->rightvol, ch->master_vol, gain, dotRight );
	}

	// if playing a word, set volume
	VOX_SetChanVol( ch );

	// end of first time spatializing sound
	if( si.IsActive( ))
	{
		ch->bfirstpass = false;
	}
	return;

ClearAllVolumes:
	// Clear all volumes and return. 
	// This shuts the sound off permanently.
	ch->leftvol = ch->rightvol = 0;
	ch->dleftvol = ch->drightvol = 0;

	// end of first time spatializing sound
	ch->bfirstpass = false;
} 

/*
====================
S_StartSound

Start a sound effect for the given entity on the given channel (ie; voice, weapon etc).  
Try to grab a channel out of the 8 dynamic spots available.
Currently used for looping sounds, streaming sounds, sentences, and regular entity sounds.
NOTE: volume is 0.0 - 1.0 and attenuation is 0.0 - 1.0 when passed in.
Pitch changes playback pitch of wave by % above or below 100.  Ignored if pitch == 100

NOTE: it's not a good idea to play looping sounds through StartDynamicSound, because
if the looping sound starts out of range, or is bumped from the buffer by another sound
it will never be restarted.  Use StartStaticSound (pass CHAN_STATIC to EMIT_SOUND or
SV_StartSound.
====================
*/
void S_StartSound( const vec3_t pos, int ent, int chan, sound_t handle, float fvol, float attn, int pitch, int flags )
{
	wavdata_t	*pSource;
	sfx_t	*sfx = NULL;
	channel_t	*target_chan, *check;
	int	vol, fsentence = 0;
	int	ch_idx;

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

	if( S_TestSoundChar( sfx->name, '!' ))
		fsentence = true;

	// spatialize
	Mem_Set( target_chan, 0, sizeof( *target_chan ));

	VectorCopy( pos, target_chan->origin );
	VectorCopy( vec3_origin, target_chan->direction );	// initially unused
	target_chan->staticsound = ( ent == 0 ) ? true : false;	// world static sound
	target_chan->use_loop = (flags & SND_STOP_LOOPING) ? false : true;
	target_chan->dist_mult = (attn / 1000.0f);
	target_chan->master_vol = vol;
	target_chan->entnum = ent;
	target_chan->entchannel = chan;
	target_chan->basePitch = pitch;
	target_chan->isSentence = false;
	target_chan->radius = 0;
	target_chan->sfx = sfx;

	// initialize gain due to obscured sound source
	target_chan->bfirstpass = true;
	target_chan->ob_gain = 0.0f;
	target_chan->ob_gain_inc = 0.0f;
	target_chan->ob_gain_target = 0.0f;
	target_chan->bTraced = false;

	pSource = NULL;

	if( fsentence )
	{
		// this is a sentence
		// link all words and load the first word
		// NOTE: sentence names stored in the cache lookup are
		// prepended with a '!'.  Sentence names stored in the
		// sentence file do not have a leading '!'. 
		VOX_LoadSound( target_chan, S_SkipSoundChar( sfx->name ));
	}
	else
	{
		// regular or streamed sound fx
		pSource = S_LoadSound( sfx );
	}

	if( !pSource )
	{
		S_FreeChannel( target_chan );
		return;
	}

	SND_Spatialize( target_chan );

	// If a client can't hear a sound when they FIRST receive the StartSound message,
	// the client will never be able to hear that sound. This is so that out of 
	// range sounds don't fill the playback buffer. For streaming sounds, we bypass this optimization.
	if( !target_chan->leftvol && !target_chan->rightvol )
	{
		// looping sounds don't use this optimization because they should stick around until they're killed.
		if( !sfx->cache || sfx->cache->loopStart == -1 )
		{
			// if this is a streaming sound, play the whole thing.
			if( chan != CHAN_STREAM )
			{
				S_FreeChannel( target_chan );
				return; // not audible at all
			}
		}
	}

	// Init client entity mouth movement vars
	SND_InitMouth( ent, chan );

	target_chan->pos = 0;
	target_chan->end = paintedtime + sfx->cache->samples;

	for( ch_idx = 0, check = channels; ch_idx < MAX_DYNAMIC_CHANNELS; ch_idx++, check++ )
	{
		if( check == target_chan ) continue;

		if( check->sfx == sfx && !check->pos )
		{
			// skip up to 0.1 seconds of audio
			int skip = Com_RandomLong( 0, (long)( 0.1f * dma.speed ));

			if( skip >= target_chan->end )
				skip = target_chan->end - 1;
			target_chan->pos += skip;
			target_chan->end -= skip;
			break;
		}
	}
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
void S_StartStaticSound( const vec3_t pos, int ent, int chan, sound_t handle, float fvol, float attn, int pitch, int flags )
{
	channel_t	*ch;
	soundinfo_t SndInfo;
	wavdata_t	*pSource = NULL;
	sfx_t	*sfx = NULL;
	int	vol, fvox = 0;
	float	flSoundRadius = 0.0f;	
	bool	looping = false;
	
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

	// First, make sure the sound source entity is even in the PVS.	
	SndInfo.pOrigin = NULL;
	SndInfo.pAngles = NULL;
	SndInfo.pflRadius = &flSoundRadius;

	si.GetEntitySpatialization( ent, &SndInfo );

	// pick a channel to play on from the static area
	ch = SND_PickStaticChannel( ent, sfx );	// autolooping sounds are always fixed origin(?)
	if( !ch ) return;

	if( S_TestSoundChar( sfx->name, '!' ))
	{
		// this is a sentence. link words to play in sequence.
		// NOTE: sentence names stored in the cache lookup are
		// prepended with a '!'.  Sentence names stored in the
		// sentence file do not have a leading '!'. 

		// link all words and load the first word
		VOX_LoadSound( ch, S_SkipSoundChar( sfx->name ));
		fvox = 1;
	}
	else
	{
		// load regular or stream sound
		pSource = S_LoadSound( sfx );
		ch->sfx = sfx;
		ch->isSentence = false;
	}

	VectorCopy( pos, ch->origin );
	VectorCopy( vec3_origin, ch->direction );

	// never update positions if source entity is 0
	ch->staticsound = ( ent == 0 ) ? true : false;
	ch->use_loop = (flags & SND_STOP_LOOPING) ? false : true;
	ch->master_vol = vol;
	ch->dist_mult = (attn / 1000.0f);
	ch->basePitch = pitch;
	ch->entnum = ent;
	ch->entchannel = chan;

	// set the default radius
	ch->radius = flSoundRadius;

	// initialize gain due to obscured sound source
	ch->bfirstpass = true;
	ch->ob_gain = 0.0;
	ch->ob_gain_inc = 0.0;
	ch->ob_gain_target = 0.0;
	ch->bTraced = false;
	ch->pos = 0;
	ch->end = paintedtime + sfx->cache->samples;

	SND_Spatialize( ch );
}

/*
==================
S_StartLocalSound
==================
*/
bool S_StartLocalSound(  const char *name, float volume, int pitch, const float *org )
{
	sound_t	sfxHandle;
	
	sfxHandle = S_RegisterSound( name );
	S_StartSound( org, s_listener.entnum, CHAN_AUTO, sfxHandle, volume, ATTN_NONE, pitch, SND_STOP_LOOPING );

	return true;
}

/*
==================
S_ClearBuffer
==================
*/
void S_ClearBuffer( void )
{
	int	clear;
		
	s_rawend = 0;

	if( dma.samplebits == 8 )
		clear = 0x80;
	else clear = 0;

	SNDDMA_BeginPainting ();
	if( dma.buffer ) Mem_Set( dma.buffer, clear, dma.samples * dma.samplebits / 8 );
	SNDDMA_Submit ();
}

/*
==================
S_StopSound

stop all sounds for entity on a channel.
==================
*/
void S_StopSound( int entnum, int channel, const char *soundname )
{
	int	i;

	for( i = 0; i < total_channels; i++ )
	{
		channel_t	*ch = &channels[i];

		if( !ch->sfx ) continue; // already freed
		if( ch->entnum == entnum && ch->entchannel == channel )
		{
			if( soundname && com.strcmp( ch->sfx->name, soundname ))
				continue;
			S_FreeChannel( ch );
		}
	}
}

/*
==================
S_StopAllSounds
==================
*/
void S_StopAllSounds( void )
{
	int	i;

	total_channels = MAX_DYNAMIC_CHANNELS;	// no statics

	for( i = 0; i < MAX_CHANNELS; i++ ) 
	{
		if( !channels[i].sfx ) continue;
		S_FreeChannel( &channels[i] );
	}

	// clear any remaining soundfade
	Mem_Set( &soundfade, 0, sizeof( soundfade ));
	s_volume->modified = true; // rebuild scaletable

	// clear all the channels
	Mem_Set( channels, 0, sizeof( channels ));
	S_ClearBuffer ();

	s_framecount = 0;
}

//=============================================================================
void S_UpdateChannels( void )
{
	uint	endtime;
	int	samps;

	SNDDMA_BeginPainting();

	if( !dma.buffer ) return;

	// updates DMA time
	soundtime = SNDDMA_GetSoundtime();

	// check to make sure that we haven't overshot
	if( paintedtime < soundtime ) paintedtime = soundtime;

	// mix ahead of current position
	endtime = soundtime + s_mixahead->value * dma.speed;

	// mix to an even submission block size
	endtime = (endtime + dma.submission_chunk - 1) & ~(dma.submission_chunk - 1);
	samps = dma.samples >> (dma.channels - 1);
	if( endtime - soundtime > samps ) endtime = soundtime + samps;

	S_PaintChannels( endtime );
	SNDDMA_Submit();
}

/*
============
S_RenderFrame

Called once each time through the main loop
============
*/
void S_RenderFrame( ref_params_t *fd )
{
	int	i, total;
	channel_t	*ch;

	if( !fd ) return;

	// bump frame count
	s_framecount++;

	// if the loading plaque is up, clear everything
	// out to make sure we aren't looping a dirty
	// dma buffer while loading
	if( fd->paused && !s_listener.paused )
	{
		S_ClearBuffer();
	}

	// update any client side sound fade
	S_UpdateSoundFade();

	if( s_listener.ingame ^ si.IsInGame())
	{
		// state changed, rebuild scaletable
		s_volume->modified = true;
	}

	s_listener.entnum = fd->viewentity;	// can be camera entity too
	s_listener.frametime = fd->frametime;
	s_listener.waterlevel = fd->waterlevel;
	s_listener.ingame = si.IsInGame();
	s_listener.paused = fd->paused;

	if( s_listener.paused ) return;

	VectorCopy( fd->simorg, s_listener.origin );
	VectorCopy( fd->vieworg, s_listener.vieworg );
	VectorCopy( fd->simvel, s_listener.velocity );
	VectorCopy( fd->forward, s_listener.forward );
	VectorCopy( fd->right, s_listener.right );
	VectorCopy( fd->up, s_listener.up );

	// rebuild scale tables if volume is modified
	if( s_volume->modified || soundfade.percent != 0 )
		S_InitScaletable();

	SND_ChannelTraceReset();

	// update spatialization for static and dynamic sounds	
	for( i = 0, ch = channels; i < total_channels; i++, ch++ )
	{
		if( !ch->sfx ) continue;
		SND_Spatialize( ch ); // respatialize channel
	}

	// debugging output
	if( s_show->value )
	{
		for( i = total = 0, ch = channels; i < MAX_CHANNELS; i++, ch++ )
		{
			if( ch->sfx && ( ch->leftvol || ch->rightvol ))
			{
				MsgDev( D_INFO, "%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name );
				total++;
			}
		}
		Msg( "----(%i)---- painted: %i\n", total, paintedtime );
	}

	S_StreamBackgroundTrack ();

	// mix some sound
	S_UpdateChannels ();
}

/*
===============================================================================

console functions

===============================================================================
*/
void S_Play_f( void )
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
	S_PrintDeviceName();
	Msg( "%5d channel(s)\n", dma.channels );
	Msg( "%5d samples\n", dma.samples );
	Msg( "%5d bits/sample\n", dma.samplebits );
	Msg( "%5d bytes/sec\n", dma.speed );
	Msg( "%5d total_channels\n", total_channels );
	
	MsgDev( D_NOTE, "%5d samplepos\n", dma.samplepos );
	MsgDev( D_NOTE, "%5d submission_chunk\n", dma.submission_chunk );
	MsgDev( D_NOTE, "0x%x dma buffer\n", dma.buffer );
}

/*
================
S_Init
================
*/
bool S_Init( void *hInst )
{
	Cmd_ExecuteString( "sndlatch\n" );

	s_volume = Cvar_Get( "s_volume", "0.7", CVAR_ARCHIVE, "sound volume" );
	s_musicvolume = Cvar_Get("s_musicvolume", "1.0", CVAR_ARCHIVE, "background music volume" );
	s_khz = Cvar_Get( "s_khz", "22", CVAR_LATCH_AUDIO|CVAR_ARCHIVE, "output sound frequency" );
	s_mixahead = Cvar_Get( "s_mixahead", "0.2", CVAR_ARCHIVE, "how much sound to mix ahead of time" );
	s_show = Cvar_Get( "s_show", "0", 0, "show playing sounds" );
	s_primary = Cvar_Get( "s_primary", "0", CVAR_LATCH_AUDIO|CVAR_ARCHIVE, "use direct primary buffer" ); 
	snd_foliage_db_loss = Cvar_Get( "snd_foliage_db_loss", "4", 0, "foliage loss factor" ); 
	snd_gain = Cvar_Get( "snd_gain", "1", 0, "sound default gain" );
	snd_gain_max = Cvar_Get( "snd_gain_max", "1", 0, "gain maximal threshold" );
	snd_gain_min = Cvar_Get( "snd_gain_min", "0.01", 0, "gain minimal threshold" );
	s_refdist = Cvar_Get( "s_refdist", "36", 0, "soundlevel reference distance" );
	s_refdb = Cvar_Get( "s_refdb", "60", 0, "soundlevel refernce dB" );
	s_check_errors = Cvar_Get( "s_check_errors", "1", CVAR_ARCHIVE, "ignore audio engine errors" );
	s_allowEAX = Cvar_Get("s_allowEAX", "1", CVAR_LATCH_AUDIO|CVAR_ARCHIVE, "allow EAX 2.0 extension" );
	s_allowA3D = Cvar_Get("s_allowA3D", "1", CVAR_LATCH_AUDIO|CVAR_ARCHIVE, "allow A3D 2.0 extension" );

	Cmd_AddCommand( "playsound", S_Play_f, "playing a specified sound file" );
	Cmd_AddCommand( "stopsound", S_StopSound_f, "stop all sounds" );
	Cmd_AddCommand( "music", S_Music_f, "starting a background track" );
	Cmd_AddCommand( "soundlist", S_SoundList_f, "display loaded sounds" );
	Cmd_AddCommand( "s_info", S_SoundInfo_f, "print sound system information" );

	if( !SNDDMA_Init( hInst ))
	{
		MsgDev( D_INFO, "S_Init: sound system can't be initialized\n" );
		return false;
	}

	sndpool = Mem_AllocPool( "Sound Zone" );
	soundtime = 0;
	paintedtime = 0;

	S_InitScaletable ();
	S_StopAllSounds ();
	SX_Init ();

	return true;
}

// =======================================================================
// Shutdown sound engine
// =======================================================================
void S_Shutdown( void )
{
	Cmd_RemoveCommand( "playsound" );
	Cmd_RemoveCommand( "stopsound" );
	Cmd_RemoveCommand( "music" );
	Cmd_RemoveCommand( "soundlist" );
	Cmd_RemoveCommand( "s_info" );

	S_StopAllSounds ();
	S_FreeSounds ();
	SX_Free ();

	SNDDMA_Shutdown ();
	Mem_FreePool( &sndpool );
}