//=======================================================================
//			Copyright XashXT Group 2009 ©
//			   s_main.c - sound engine
//=======================================================================

#include "sound.h"
#include "const.h"

// only begin attenuating sound volumes when outside the FULLVOLUME range
#define SOUND_FULLVOLUME	80
#define SOUND_LOOPATTENUATE	0.003
#define MAX_PLAYSOUNDS	128

dma_t		dma;
channel_t   	channels[MAX_CHANNELS];
bool		sound_started = false;
vec3_t		listener_origin;
vec3_t		listener_velocity;
vec3_t		listener_forward;
vec3_t		listener_right;
vec3_t		listener_up;
int		listener_waterlevel;

int		total_channels;
int		s_framecount;	// for autosounds checking
int		s_clientnum;	// cl.playernum + 1
int		soundtime;	// sample PAIRS
int   		paintedtime; 	// sample PAIRS
byte		*sndpool;

playsound_t	s_playsounds[MAX_PLAYSOUNDS];
playsound_t	s_freeplays;
playsound_t	s_pendingplays;
int		s_beginofs;

cvar_t		*host_sound;
cvar_t		*s_check_errors;
cvar_t		*s_volume;
cvar_t		*s_testsound;
cvar_t		*s_loadas8bit;
cvar_t		*s_khz;
cvar_t		*s_show;
cvar_t		*s_mixahead;
cvar_t		*s_primary;

/*
=============================================================================

		SOUNDS PROCESSING
		
=============================================================================
*/
/*
=================
S_PickChannel
=================
*/
channel_t *S_PickChannel( int entnum, int channel )
{
	int		i;
	int		firstToDie = -1;
	int		oldestTime = 0x7fffffff;
	channel_t		*ch;

	if( entnum < 0 || channel < 0 ) return NULL; // invalid channel or entnum

	for( i = 0, ch = channels; i < total_channels; i++, ch++ )
	{
		// check if this channel is active
		if( channel == CHAN_AUTO && !ch->sfx )
		{
			// free channel
			firstToDie = i;
			break;
		}

		// channel 0 never overrides
		if( channel != CHAN_AUTO && ( ch->entnum == entnum && ch->entchannel == channel ))
		{	
			// always override sound from same entity
			firstToDie = i;
			break;
		}

		// don't let monster sounds override player sounds
		if( entnum != s_clientnum && ch->entnum == s_clientnum && ch->sfx )
			continue;

		// replace the oldest sound
		if( ch->end - paintedtime < oldestTime )
		{
			oldestTime = ch->end - paintedtime;
			firstToDie = i;
		}
	}

	if( firstToDie == -1 )
		return NULL;

	ch = &channels[firstToDie];
	Mem_Set( ch, 0, sizeof( *ch ));

	return ch;
}       

int S_AlterChannel( int entnum, int chan, sfx_t *sfx, int vol, int pitch, int flags )
{
	int		i;
	channel_t		*ch;
	
	if( S_TestSoundChar( sfx->name, '!' ))
	{
		// This is a sentence name.
		// For sentences: assume that the entity is only playing one sentence
		// at a time, so we can just shut off
		// any channel that has ch->isentence >= 0 and matches the
		// soundsource.
		for( i = 0, ch = channels; i < total_channels; i++ )
		{
			if( ch->entnum == entnum && ch->entchannel == chan && ch->sfx && ch->fsentence )
			{
				if( flags & SND_CHANGE_PITCH )
					ch->basePitch = pitch;
				
				if( flags & SND_CHANGE_VOL )
					ch->master_vol = vol;
				
				if( flags & SND_STOP )
					Mem_Set( ch, 0, sizeof( channel_t ));
				return true;
			}
		}
		// channel not found
		return false;

	}

	// regular sound or streaming sound
	for( i = 0, ch = channels; i < total_channels; i++ )
	{
		if( ch->entnum == entnum && ch->entchannel == chan && ch->sfx )
		{
         			Msg( "S_StartSound: vol %i, pitch %i\n", vol, pitch );
			if( flags & SND_CHANGE_PITCH )
				ch->basePitch = pitch;
				
			if( flags & SND_CHANGE_VOL )
				ch->master_vol = vol;
				
			if( flags & SND_STOP )
				Mem_Set( ch, 0, sizeof( channel_t ));
			return true;
		}
	}
	return false;
}

/*
=================
S_SpatializeOrigin

Used for spatializing channels and autosounds
=================
*/
void S_SpatializeOrigin( vec3_t pos, vec3_t vel, float master_vol, float dist_mult, int *left_vol, int *right_vol )
{
	float	dot;
	float	dist;
	float	lscale, rscale, scale;
	vec3_t	source_vec;
	vec3_t	source_vel;

	// calculate stereo seperation and distance attenuation
	VectorSubtract( pos, listener_origin, source_vec );
	VectorCopy( vel, source_vel );

	dist = VectorNormalizeLength( source_vec );
	dist -= SOUND_FULLVOLUME;
	if( dist < 0 ) dist = 0;	// close enough to be at full volume
	dist *= dist_mult;		// different attenuation levels
	
	dot = DotProduct( listener_right, source_vec );

	if( dma.channels == 1 || !dist_mult )
	{
		// no attenuation = no spatialization
		rscale = 1.0f;
		lscale = 1.0f;
	}
	else
	{
		rscale = 0.5f * (1.0f + dot);
		lscale = 0.5f * (1.0f - dot);
	}

	// add in distance effect
	scale = (1.0f - dist) * rscale;
	*right_vol = (int)( master_vol * scale );
	if( *right_vol < 0 ) *right_vol = 0;

	scale = (1.0f - dist) * lscale;
	*left_vol = (int)( master_vol * scale );
	if( *left_vol < 0 ) *left_vol = 0;
}

/*
=================
S_Spatialize
=================
*/
void S_SpatializeChannel( channel_t *ch )
{
	vec3_t	position, velocity;

	// anything coming from the view entity will always be full volume
	if( ch->entnum == s_clientnum || !ch->dist_mult )
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

	if( ch->fixed_origin )
	{
		VectorCopy( ch->origin, position );
		VectorSet( velocity, 0, 0, 0 );
	}
	else
	{
		if( ch->autosound ) si.GetSoundSpatialization( ch->loopnum, position, velocity );
		else si.GetSoundSpatialization( ch->entnum, position, velocity );
	}

	S_SpatializeOrigin( position, velocity, ch->master_vol, ch->dist_mult, &ch->leftvol, &ch->rightvol );
}           


/*
=================
S_AllocPlaysound
=================
*/
playsound_t *S_AllocPlaysound( void )
{
	playsound_t	*ps;

	ps = s_freeplays.next;
	if( ps == &s_freeplays )
		return NULL; // no free playsounds

	// unlink from freelist
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;
	
	return ps;
}


/*
=================
S_FreePlaysound
=================
*/
void S_FreePlaysound( playsound_t *ps )
{
	// unlink from channel
	ps->prev->next = ps->next;
	ps->next->prev = ps->prev;

	// add to free list
	ps->next = s_freeplays.next;
	s_freeplays.next->prev = ps;
	ps->prev = &s_freeplays;
	s_freeplays.next = ps;
}

/*
===============
S_IssuePlaysound

Take the next playsound and begin it on the channel
This is never called directly by S_Play*, but only
by the update loop.
===============
*/
void S_IssuePlaysound( playsound_t *ps )
{
	channel_t		*ch;
	sfxcache_t	*sc;

	if( s_show->value ) MsgDev( D_INFO, "Issue %i\n", ps->begin );

	// pick a channel to play on
	ch = S_PickChannel( ps->entnum, ps->entchannel );
	if( !ch )
	{
		if( ps->sfx->name[0] == '#' ) MsgDev( D_ERROR, "dropped sound %s\n", &ps->sfx->name[1] );
		else MsgDev( D_ERROR, "dropped sound \"sound/%s\"\n", ps->sfx->name );
		S_FreePlaysound( ps );
		return;
	}

	// spatialize
	if( ps->attenuation == ATTN_STATIC )
		ch->dist_mult = ps->attenuation * 0.001;
	else ch->dist_mult = ps->attenuation * 0.0005;

	ch->master_vol = ps->volume;
	ch->entnum = ps->entnum;
	ch->entchannel = ps->entchannel;
	ch->sfx = ps->sfx;
	ch->use_loop = ps->use_loop;
	VectorCopy( ps->origin, ch->origin );
	ch->fixed_origin = ps->fixed_origin;

	S_SpatializeChannel( ch );

	ch->pos = 0;
	sc = S_LoadSound( ch->sfx );
	ch->end = paintedtime + sc->length;

	// free the playsound
	S_FreePlaysound( ps );
}

// =======================================================================
// Start a sound effect
// =======================================================================
/*
====================
S_StartSound

Validates the parms and ques the sound up
if pos is NULL, the sound will be dynamically sourced from the entity
Entchannel 0 will never override a playing sound
====================
*/
void S_StartSound( const vec3_t pos, int ent, int chan, sound_t handle, float fvol, float attn, float pitch, int flags )
{
	sfxcache_t	*sc;
	int		vol, start;
	playsound_t	*ps, *sort;
	sfx_t		*sfx = NULL;
	float		timeofs = 0; // FIXME: implement into network protocol

	if( !sound_started )
		return;

	sfx = S_GetSfxByHandle( handle );
	if( !sfx ) return;

	// make sure the sound is loaded
	sc = S_LoadSound( sfx );
	if( !sc ) return; // couldn't load the sound's data

	vol = fvol * 255;

	if( flags & (SND_STOP|SND_CHANGE_VOL|SND_CHANGE_PITCH))
	{
		if( S_AlterChannel( ent, chan, sfx, vol, pitch, flags ))
		{
			Msg( "S_AlterChannel( %s, %i, %i )\n", sfx->name, ent, chan );
			return;
		}

		if( flags & SND_STOP ) return;
		// fall through - if we're not trying to stop the sound, 
		// and we didn't find it (it's not playing), go ahead and start it up
	}

	if( pitch == 0 )
	{
		MsgDev( D_WARN, "S_StartSound: ( %s ) ignored, called with pitch 0\n", sfx->name );
		return;
	}

	// allocate the playsound_t
	ps = S_AllocPlaysound();
	if( !ps )
	{
		if( sfx->name[0] == '#' ) MsgDev( D_ERROR, "dropped sound %s\n", &sfx->name[1] );
		else MsgDev( D_ERROR, "dropped sound \"sound/%s\"\n", sfx->name );
		return;
	}

	if( pos )
	{
		VectorCopy( pos, ps->origin );
		ps->fixed_origin = true;
	}
	else ps->fixed_origin = false;

	ps->entnum = ent;
	ps->entchannel = chan;
	ps->attenuation = attn;
	ps->use_loop = (flags & SND_STOP_LOOPING) ? false : true;
	ps->volume = vol;
	ps->sfx = sfx;

	// drift s_beginofs
	start = si.GetServerTime() * 0.001 * dma.speed + s_beginofs;
	if( start < paintedtime )
	{
		start = paintedtime;
		s_beginofs = start - ( si.GetServerTime() * 0.001 * dma.speed );
	}
	else if( start > paintedtime + 0.3 * dma.speed )
	{
		start = paintedtime + 0.1 * dma.speed;
		s_beginofs = start - ( si.GetServerTime() * 0.001 * dma.speed );
	}
	else s_beginofs -= 10;

	if( !timeofs ) ps->begin = paintedtime;
	else ps->begin = start + timeofs * dma.speed;

	// sort into the pending sound list
	for( sort = s_pendingplays.next; sort != &s_pendingplays && sort->begin < ps->begin; sort = sort->next );

	ps->next = sort;
	ps->prev = sort->prev;
	ps->next->prev = ps;
	ps->prev->next = ps;
}

/*
==================
S_StartLocalSound
==================
*/
bool S_StartLocalSound(  const char *name, float volume, float pitch, const float *origin )
{
	sound_t	sfxHandle;

	if( !sound_started )
		return false;
		
	sfxHandle = S_RegisterSound( name );
	S_StartSound( origin, s_clientnum, CHAN_AUTO, sfxHandle, volume, ATTN_NONE, pitch, SND_STOP_LOOPING );

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
		
	if( !sound_started )
		return;

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
S_StopAllSounds
==================
*/
void S_StopAllSounds( void )
{
	int	i;

	if( !sound_started )
		return;

	// clear all the playsounds
	Mem_Set( s_playsounds, 0, sizeof( s_playsounds ));
	s_freeplays.next = s_freeplays.prev = &s_freeplays;
	s_pendingplays.next = s_pendingplays.prev = &s_pendingplays;

	for( i = 0; i < MAX_PLAYSOUNDS; i++ )
	{
		s_playsounds[i].prev = &s_freeplays;
		s_playsounds[i].next = s_freeplays.next;
		s_playsounds[i].prev->next = &s_playsounds[i];
		s_playsounds[i].next->prev = &s_playsounds[i];
	}

	total_channels = MAX_CHANNELS; // no statics

	// clear all the channels
	Mem_Set( channels, 0, sizeof( channels ));
	S_ClearBuffer ();

	s_framecount = 0;
}

/*
==================
S_AddLoopinSound

Entities with a ->sound field will generated looped sounds
that are automatically started, stopped, and merged together
as the entities are sent to the client
==================
*/
bool S_AddLoopingSound( int entnum, sound_t handle, float volume, float attn )
{
	channel_t		*ch;
	sfx_t		*sfx = NULL;
	int		i;

	if( !sound_started )
		return false;
	sfx = S_GetSfxByHandle( handle );

	// default looped sound it's terrible :)
	if( !sfx || sfx->default_sound || !sfx->cache )
		return false;

	// if this entity is already playing the same sound effect on an
	// active channel, then simply update it
	for( i = 0, ch = channels; i < MAX_CHANNELS; i++, ch++ )
	{
		if( ch->sfx != sfx ) continue;
		if( !ch->autosound ) continue;
		if( ch->loopnum != entnum ) continue;
		if( ch->loopframe + 1 != s_framecount )
			continue;

		ch->loopframe = s_framecount;
		break;
	}
	if( i != MAX_CHANNELS )
		return false;

	// otherwise pick a channel and start the sound effect
	ch = S_PickChannel( 0, 0 );
	if( !ch )
	{
		MsgDev( D_ERROR, "dropped sound \"sound/%s\"\n", sfx->name );
		return false;
	}

	ch->sfx = sfx;
	ch->use_loop = true;	// autosounds never comes from S_StartLocalSound
	ch->autosound = true;	// remove next frame
	ch->loopnum = entnum;
	ch->loopframe = s_framecount;
	ch->fixed_origin = false;
	ch->dist_mult = ATTN_STATIC * 0.001;
	ch->pos = paintedtime % sfx->cache->length;
	ch->end = paintedtime + sfx->cache->length - ch->pos;

	// now we can spatialize channel
	S_SpatializeChannel( ch );

	return true;
}

//=============================================================================

void GetSoundtime( void )
{
	static int	buffers, oldsamplepos;
	int		samplepos, fullsamples;
	
	fullsamples = dma.samples / dma.channels;

	// it is possible to miscount buffers
	// if it has wrapped twice between
	// calls to S_Update.  Oh well.
	samplepos = SNDDMA_GetDMAPos();

	if( samplepos < oldsamplepos )
	{
		buffers++; // buffer wrapped

		if( paintedtime > 0x40000000 )
		{	
			// time to chop things off to avoid 32 bit limits
			buffers = 0;
			paintedtime = fullsamples;

			S_StopAllSounds();
		}
	}

	oldsamplepos = samplepos;
	soundtime = buffers * fullsamples + samplepos / dma.channels;
}

void S_UpdateChannels( void )
{
	uint	endtime;
	int	samps;

	if( !sound_started )
		return;

	SNDDMA_BeginPainting();

	if( !dma.buffer )
		return;

	// updates DMA time
	GetSoundtime();

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
S_Update

Called once each time through the main loop
============
*/
void S_Update( ref_params_t *fd )
{
	int	i, total;
	channel_t	*ch, *combine = NULL;

	if( !sound_started || !fd )
		return;

	// bump frame count
	s_framecount++;

	// if the loading plaque is up, clear everything
	// out to make sure we aren't looping a dirty
	// dma buffer while loading
	if( fd->paused )
	{
		S_ClearBuffer();
		return;
	}

	// rebuild scale tables if volume is modified
	if( s_volume->modified ) S_InitScaletable();

	s_clientnum = fd->viewentity;
	VectorCopy( fd->simorg, listener_origin );
	VectorCopy( fd->simvel, listener_velocity );
	VectorCopy( fd->forward, listener_forward );
	VectorCopy( fd->right, listener_right );
	VectorCopy( fd->up, listener_up );
	listener_waterlevel = fd->waterlevel;

	// Add looping sounds
	si.AddLoopingSounds();

	// update spatialization for dynamic sounds	
	for( i = 0, ch = channels; i < MAX_CHANNELS; i++, ch++ )
	{
		if( !ch->sfx ) continue;
		if( ch->autosound )
		{	
			if( ch->loopframe != s_framecount )
			{
				Mem_Set( ch, 0, sizeof( *ch )); // stopped
				continue;
			}
		}

		// respatialize channel
		S_SpatializeChannel( ch );

		if( !ch->leftvol && !ch->rightvol )
		{
			Mem_Set( ch, 0, sizeof( *ch )); // not audible
			continue;
		}
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

	// mix some sound
	S_UpdateChannels();
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
	if( !sound_started )
	{
		Msg( "sound system not started\n" );
		return;
	}
	
	Msg( "%5d channel(s)\n", dma.channels );
	Msg( "%5d samples\n", dma.samples );
	Msg( "%5d bits/sample\n", dma.samplebits );
	Msg( "%5d bytes/sec\n", dma.speed );

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

	host_sound = Cvar_Get( "host_sound", "1", CVAR_SYSTEMINFO, "enable sound system" );
	s_volume = Cvar_Get( "s_volume", "0.7", CVAR_ARCHIVE, "sound volume" );
	s_khz = Cvar_Get( "s_khz", "11", CVAR_LATCH_AUDIO|CVAR_ARCHIVE, "output sound frequency" );
	s_loadas8bit = Cvar_Get( "s_loadas8bit", "1", CVAR_LATCH_AUDIO|CVAR_ARCHIVE, "resample all sounds to 8-bit" );
	s_mixahead = Cvar_Get( "s_mixahead", "0.2", CVAR_ARCHIVE, "how much sound to mix ahead of time" );
	s_show = Cvar_Get( "s_show", "0", 0, "show playing sounds" );
	s_testsound = Cvar_Get( "s_testsound", "0", 0, "generate sine 1 khz wave to testing audio subsystem" );
	s_primary = Cvar_Get( "s_primary", "0", CVAR_LATCH_AUDIO|CVAR_ARCHIVE, "use direct primary buffer" ); 
	s_check_errors = Cvar_Get( "s_check_errors", "1", CVAR_ARCHIVE, "ignore audio engine errors" );

	Cmd_AddCommand( "playsound", S_Play_f, "playing a specified sound file" );
	Cmd_AddCommand( "stopsound", S_StopSound_f, "stop all sounds" );
	Cmd_AddCommand( "soundlist", S_SoundList_f, "display loaded sounds" );
	Cmd_AddCommand( "s_info", S_SoundInfo_f, "print sound system information" );

	if( !host_sound->integer )
	{
		MsgDev( D_INFO, "Audio: disabled\n" );
		return false;
	}

	if( !SNDDMA_Init( hInst ))
	{
		MsgDev( D_INFO, "S_Init: sound system can't initialized\n" );
		return false;
	}
	S_InitScaletable();

	sndpool = Mem_AllocPool( "Sound Zone" );
	sound_started = true;
	soundtime = 0;
	paintedtime = 0;

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
	Cmd_RemoveCommand( "soundlist" );
	Cmd_RemoveCommand( "s_info" );

	if( !sound_started ) return;
	sound_started = false;

	SNDDMA_Shutdown();

	S_FreeSounds();
	SX_Free ();

	Mem_FreePool( &sndpool );
}