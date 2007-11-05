//=======================================================================
//			Copyright XashXT Group 2007 ©
//		      snd_dma.c - sound direct memory acess
//=======================================================================

#include "client.h"
#include "snd_loc.h"

void S_Play_f(void);
void S_SoundList_f(void);
void S_Music_f(void);

void S_Update_();
void S_StopAllSounds(void);
void S_UpdateBackgroundTrack( void );

static file_t	*s_backgroundFile;
static wavinfo_t	s_backgroundInfo;
static int	s_backgroundSamples;
static char	s_backgroundLoop[MAX_QPATH];

// =======================================================================
// Internal sound data & structures
// =======================================================================

// only begin attenuating sound volumes when outside the FULLVOLUME range
#define SOUND_FULLVOLUME		80
#define SOUND_ATTENUATE		0.0008f

channel_t s_channels[MAX_CHANNELS];
channel_t loop_channels[MAX_CHANNELS];
int numLoopChannels;

static int s_soundStarted;
static bool s_soundMuted;
dma_t dma;
static int listener_number;
static vec3_t listener_origin;
static vec3_t listener_axis[3];
int s_soundtime;// sample PAIRS
int s_paintedtime;// sample PAIRS

// MAX_SFX may be larger than MAX_SOUNDS because
// of custom player sounds
#define MAX_SFX			4096
sfx_t s_knownSfx[MAX_SFX];
int s_numSfx = 0;

#define LOOP_HASH			128
static sfx_t *sfxHash[LOOP_HASH];

cvar_t *s_volume;
cvar_t *s_testsound;
cvar_t *s_khz;
cvar_t *s_show;
cvar_t *s_mixahead;
cvar_t *s_mixPreStep;
cvar_t *s_musicVolume;
cvar_t *s_separation;
cvar_t *s_doppler;

static loopSound_t loopSounds[MAX_MAP_ENTITIES];
static channel_t *freelist = NULL;

int s_rawend;
portable_samplepair_t s_rawsamples[MAX_RAW_SAMPLES];


// ====================================================================
// User-setable variables
// ====================================================================
void S_SoundInfo_f( void )
{	
	Msg("----- Sound Info -----\n" );
	if (!s_soundStarted)
	{
		Msg("sound system not started\n");
	} 
	else
	{
		if ( s_soundMuted ) Msg("sound system is muted\n");
		Msg("%5d stereo\n", dma.channels - 1);
		Msg("%5d samples\n", dma.samples);
		Msg("%5d samplebits\n", dma.samplebits);
		Msg("%5d submission_chunk\n", dma.submission_chunk);
		Msg("%5d speed\n", dma.speed);
		Msg("0x%x dma buffer\n", dma.buffer);
		if ( s_backgroundFile ) Msg("Background file: %s\n", s_backgroundLoop );
		else Msg("No background file.\n" );
	}
	Msg("----------------------\n" );
}

/*
================
S_Init
================
*/
void S_Init( void )
{
	cvar_t	*cv;
	bool	r;

	Msg("\n------- sound initialization -------\n");

	s_volume = Cvar_Get ("s_volume", "0.8", CVAR_ARCHIVE);
	s_musicVolume = Cvar_Get ("s_musicvolume", "0.25", CVAR_ARCHIVE);
	s_separation = Cvar_Get ("s_separation", "0.5", CVAR_ARCHIVE);
	s_doppler = Cvar_Get ("s_doppler", "1", CVAR_ARCHIVE);
	s_khz = Cvar_Get ("s_khz", "22", CVAR_ARCHIVE);
	s_mixahead = Cvar_Get ("s_mixahead", "0.2", CVAR_ARCHIVE);
	s_mixPreStep = Cvar_Get ("s_mixPreStep", "0.05", CVAR_ARCHIVE);
	s_show = Cvar_Get ("s_show", "0", CVAR_CHEAT);
	s_testsound = Cvar_Get ("s_testsound", "0", CVAR_CHEAT);

	cv = Cvar_Get ("s_initsound", "1", 0);
	if ( !cv->value )
	{
		Msg ("not initializing.\n");
		Msg("------------------------------------\n");
		return;
	}

	Cmd_AddCommand("play", S_Play_f);
	Cmd_AddCommand("music", S_Music_f);
	Cmd_AddCommand("s_list", S_SoundList_f);
	Cmd_AddCommand("s_info", S_SoundInfo_f);
	Cmd_AddCommand("s_stop", S_StopAllSounds);

	r = SNDDMA_Init();
	Msg("------------------------------------\n");

	if ( r )
	{
		s_soundStarted = 1;
		s_soundMuted = 1;
		memset(sfxHash, 0, sizeof(sfx_t *) * LOOP_HASH);
		s_soundtime = 0;
		s_paintedtime = 0;
		S_StopAllSounds ();
		S_SoundInfo_f();
	}

}

void S_ChannelFree(channel_t *v)
{
	v->thesfx = NULL;
	*(channel_t **)v = freelist;
	freelist = (channel_t*)v;
}

channel_t* S_ChannelMalloc( void )
{
	channel_t *v;
	if (freelist == NULL)
		return NULL;

	v = freelist;
	freelist = *(channel_t **)freelist;
	v->allocTime = Sys_DoubleTime();
	return v;
}

void S_ChannelSetup( void )
{
	channel_t	*p, *q;

	// clear all the sounds so they don't
	memset( s_channels, 0, sizeof( s_channels ));

	p = s_channels;
	q = p + MAX_CHANNELS;

	while (--q > p) *(channel_t **)q = q-1;
	*(channel_t **)q = NULL;
	freelist = p + MAX_CHANNELS - 1;
	MsgDev(D_INFO, "S_ChannelSetup: memory manager started\n");
}

// =======================================================================
// Shutdown sound engine
// =======================================================================
void S_Shutdown( void )
{
	if ( !s_soundStarted ) return;
	SNDDMA_Shutdown();
	s_soundStarted = 0;

	Cmd_RemoveCommand("play");
	Cmd_RemoveCommand("music");
	Cmd_RemoveCommand("stopsound");
	Cmd_RemoveCommand("soundlist");
	Cmd_RemoveCommand("soundinfo");
}


// =======================================================================
// Load a sound
// =======================================================================
/*
================
return a hash value for the sfx name
================
*/
static long S_HashSFXName(const char *name)
{
	int		i = 0;
	long		hash = 0;
	char		letter;

	while (name[i] != '\0')
	{
		letter = tolower(name[i]);
		if (letter =='.') break;		// don't include extension
		if (letter =='\\') letter = '/';	// damn path names
		hash += (long)(letter)*(i+119);
		i++;
	}
	hash &= (LOOP_HASH-1);
	return hash;
}

/*
==================
S_FindName

Will allocate a new sfx if it isn't found
==================
*/
static sfx_t *S_FindName( const char *name )
{
	int	i, hash;
	sfx_t	*sfx;

	if (!name || !name[0])
	{
		MsgWarn("S_FindName: empty name\n");
		return NULL;
	}
	if (strlen(name) >= MAX_QPATH)
	{
		MsgWarn("S_FindName: sound name too long: %s", name);
		return NULL;
	}

	hash = S_HashSFXName(name);
	sfx = sfxHash[hash];
	
	// see if already loaded
	while (sfx)
	{
		if (!stricmp(sfx->soundName, name))
			return sfx;
		sfx = sfx->next;
	}

	// find a free sfx slot
	for (i = 0; i < s_numSfx; i++)
	{
		if(!s_knownSfx[i].soundName[0])
			break;
	}

	if (i == s_numSfx)
	{
		if (s_numSfx == MAX_SFX)
		{
			MsgWarn("S_FindName: MAX_SFX limit exceeded\n");
			return NULL;
		}
		s_numSfx++;
	}
	
	sfx = &s_knownSfx[i];
	memset (sfx, 0, sizeof(*sfx));
	strcpy (sfx->soundName, name);
	sfx->next = sfxHash[hash];
	sfxHash[hash] = sfx;

	return sfx;
}

/*
=================
S_DefaultSound
=================
*/
void S_DefaultSound( sfx_t *sfx )
{
	int		i;

	sfx->soundLength = 512;
	sfx->soundData = SND_malloc();
	sfx->soundData->next = NULL;

	for ( i = 0; i < sfx->soundLength; i++ )
		sfx->soundData->sndChunk[i] = i;
}

/*
===================
S_DisableSounds

Disables sounds until the next S_BeginRegistration.
This is called when the hunk is cleared and the sounds
are no longer valid.
===================
*/
void S_DisableSounds( void )
{
	S_StopAllSounds();
	s_soundMuted = true;
}

void S_EnableSounds( void )
{
	s_soundMuted = false;
}

/*
=====================
S_BeginRegistration

=====================
*/
void S_BeginRegistration( void )
{
	S_EnableSounds(); // we can play again

	if (s_numSfx == 0)
	{
		SND_setup();
		s_numSfx = 0;
		memset( s_knownSfx, 0, sizeof( s_knownSfx ) );
		memset(sfxHash, 0, sizeof(sfx_t *)*LOOP_HASH);
		S_RegisterSound("misc/menu1.wav");
	}
}

/*
=====================
S_EndRegistration

=====================
*/
void S_EndRegistration( void )
{
}

/*
==================
S_RegisterSound

Creates a default buzz sound if the file can't be loaded
==================
*/
sound_t S_RegisterSound( const char *name )
{
	sfx_t		*sfx;

	if (!s_soundStarted) return 0;

	if( strlen( name ) >= MAX_QPATH )
	{
		MsgWarn( "Sound name exceeds MAX_QPATH\n" );
		return 0;
	}

	sfx = S_FindName( name );
	if ( sfx->soundData )
	{
		if( sfx->defaultSound )
		{
			MsgWarn("S_RegisterSound: could not find %s\n", sfx->soundName );
			return 0;
		}
		return sfx - s_knownSfx;
	}
	sfx->inMemory = false;
	sfx->soundCompressed = false;
	S_memoryLoad(sfx);

	if ( sfx->defaultSound )
	{
		MsgWarn("S_RegisterSound: could not find %s\n", sfx->soundName );
		return 0;
	}
	return sfx - s_knownSfx;
}

void S_memoryLoad(sfx_t *sfx)
{
	// load the sound file
	if ( !S_LoadSound ( sfx ) )
		sfx->defaultSound = true;
	sfx->inMemory = true;
}

/*
=================
S_SpatializeOrigin

Used for spatializing s_channels
=================
*/
void S_SpatializeOrigin (vec3_t origin, int master_vol, int *left_vol, int *right_vol)
{
	vec_t		dot;
	vec_t		dist;
	vec_t		lscale, rscale, scale;
	vec3_t		source_vec;
	vec3_t		vec;
	const float dist_mult = SOUND_ATTENUATE;
	
	// calculate stereo seperation and distance attenuation
	VectorSubtract(origin, listener_origin, source_vec);

	dist = VectorNormalize(source_vec);
	dist -= SOUND_FULLVOLUME;
	if (dist < 0) dist = 0; // close enough to be at full volume
	dist *= dist_mult; // different attenuation levels
	VectorRotate( source_vec, listener_axis, vec );

	dot = -vec[1];

	if (dma.channels == 1)
	{
		// no attenuation = no spatialization
		rscale = 1.0;
		lscale = 1.0;
	}
	else
	{
		rscale = 0.5 * (1.0 + dot);
		lscale = 0.5 * (1.0 - dot);
		//rscale = s_separation->value + ( 1.0 - s_separation->value ) * dot;
		//lscale = s_separation->value - ( 1.0 - s_separation->value ) * dot;
		if ( rscale < 0 ) rscale = 0;
		if ( lscale < 0 ) lscale = 0;
	}

	// add in distance effect
	scale = (1.0 - dist) * rscale;
	*right_vol = (master_vol * scale);
	if (*right_vol < 0) *right_vol = 0;

	scale = (1.0 - dist) * lscale;
	*left_vol = (master_vol * scale);
	if (*left_vol < 0) *left_vol = 0;
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
void S_StartSound(vec3_t origin, int entityNum, int entchannel, sound_t sfxHandle )
{
	channel_t		*ch;
	sfx_t		*sfx;
	int		i, chosen;
	int		inplay, allowed;
	float		time, oldest;

	if ( !s_soundStarted || s_soundMuted ) return;

	if(origin && ( entityNum < 0 || entityNum > MAX_MAP_ENTITIES ))
	{
		MsgWarn("S_StartSound: bad entitynum %i", entityNum );
		return;
	}
	if ( sfxHandle < 0 || sfxHandle >= s_numSfx )
	{
		MsgWarn("S_StartSound: handle %i out of range\n", sfxHandle );
		return;
	}

	sfx = &s_knownSfx[ sfxHandle ];
	if (sfx->inMemory == false) S_memoryLoad(sfx);

	if ( s_show->value == 1 ) Msg( "%i : %s\n", s_paintedtime, sfx->soundName );

	time = Sys_DoubleTime();

	// pick a channel to play on
	allowed = 4;
	if (entityNum == listener_number) allowed = 8;
	ch = s_channels;
	inplay = 0;
	for ( i = 0; i < MAX_CHANNELS ; i++, ch++ )
	{		
		if (ch[i].entnum == entityNum && ch[i].thesfx == sfx)
		{
			if (time - ch[i].allocTime < 0.05f)
				return;
			inplay++;
		}
	}

	if (inplay>allowed) return;
	sfx->lastTimeUsed = time;

	ch = S_ChannelMalloc();

	if(!ch)
	{
		ch = s_channels;

		oldest = sfx->lastTimeUsed;
		chosen = -1;
		for ( i = 0; i < MAX_CHANNELS; i++, ch++ )
		{
			if (ch->entnum != listener_number && ch->entnum == entityNum && ch->allocTime < oldest && ch->entchannel != CHAN_ANNOUNCER)
			{
				oldest = ch->allocTime;
				chosen = i;
			}
		}
		if (chosen == -1)
		{
			ch = s_channels;
			for ( i = 0; i < MAX_CHANNELS; i++, ch++ )
			{
				if (ch->entnum != listener_number && ch->allocTime < oldest && ch->entchannel != CHAN_ANNOUNCER)
				{
					oldest = ch->allocTime;
					chosen = i;
				}
			}
			if (chosen == -1)
			{
				if (ch->entnum == listener_number)
				{
					for ( i = 0; i < MAX_CHANNELS; i++, ch++ )
					{
						if (ch->allocTime < oldest)
						{
							oldest = ch->allocTime;
							chosen = i;
						}
					}
				}
				if (chosen == -1)
				{
					MsgWarn("S_StartSound: dropping\n");
					return;
				}
			}
		}
		ch = &s_channels[chosen];
		ch->allocTime = sfx->lastTimeUsed;
	}

	if (origin)
	{
		VectorCopy (origin, ch->origin);
		ch->fixed_origin = true;
	}
	else ch->fixed_origin = false;

	ch->master_vol = 127;
	ch->entnum = entityNum;
	ch->thesfx = sfx;
	ch->startSample = START_SAMPLE_IMMEDIATE;
	ch->entchannel = entchannel;
	ch->leftvol = ch->master_vol;	 // these will get calced at next spatialize
	ch->rightvol = ch->master_vol; // unless the game isn't running
	ch->doppler = false;
}


/*
==================
S_StartLocalSound
==================
*/
int S_StartLocalSound( const char *name )
{
	sound_t	sfxHandle;

	if(!s_soundStarted || s_soundMuted )
		return false;

	sfxHandle = S_RegisterSound( name );

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx )
	{
		MsgWarn("S_StartLocalSound: handle %i out of range\n", sfxHandle );
		return false;
	}

	S_StartSound (NULL, listener_number, CHAN_AUTO, sfxHandle );
	return true;
}


/*
==================
S_ClearSoundBuffer

If we are about to perform file access, clear the buffer
so sound doesn't stutter.
==================
*/
void S_ClearSoundBuffer( void )
{
	int		clear;
		
	if (!s_soundStarted) return;

	// stop looping sounds
	memset(loopSounds, 0, MAX_MAP_ENTITIES * sizeof(loopSound_t));
	memset(loop_channels, 0, MAX_CHANNELS * sizeof(channel_t));
	numLoopChannels = 0;

	S_ChannelSetup();
	s_rawend = 0;

	if (dma.samplebits == 8) clear = 0x80;
	else clear = 0;

	SNDDMA_BeginPainting ();
	if (dma.buffer) memset(dma.buffer, clear, dma.samples * dma.samplebits/8);
	SNDDMA_Submit ();
}

/*
==================
S_StopAllSounds
==================
*/
void S_StopAllSounds(void)
{
	if ( !s_soundStarted ) return;

	// stop the background music
	S_StopBackgroundTrack();
	S_ClearSoundBuffer ();
}

/*
==============================================================

continuous looping sounds are added each frame

==============================================================
*/
void S_StopLoopingSound(int entityNum)
{
	loopSounds[entityNum].active = false;
	loopSounds[entityNum].kill = false;
}

/*
==================
S_ClearLoopingSounds

==================
*/
void S_ClearLoopingSounds( bool killall )
{
	int	i;

	for ( i = 0; i < MAX_MAP_ENTITIES; i++)
	{
		if (killall || loopSounds[i].kill == true || (loopSounds[i].sfx && loopSounds[i].sfx->soundLength == 0))
		{
			loopSounds[i].kill = false;
			S_StopLoopingSound( i );
		}
	}
	numLoopChannels = 0;
}

/*
==================
S_AddLoopingSound

Called during entity generation for a frame
Include velocity in case I get around to doing doppler...
==================
*/
void S_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sound_t sfxHandle )
{
	sfx_t	*sfx;

	if ( !s_soundStarted || s_soundMuted ) return;

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx )
	{
		MsgWarn("S_AddLoopingSound: handle %i out of range\n", sfxHandle );
		return;
	}

	sfx = &s_knownSfx[ sfxHandle ];

	if(sfx->inMemory == false) S_memoryLoad(sfx);
	if(!sfx->soundLength )
	{
		Host_Error("S_AddLoopingSound: %s with length 0", sfx->soundName );
		return;
	}

	VectorCopy( origin, loopSounds[entityNum].origin );
	VectorCopy( velocity, loopSounds[entityNum].velocity );
	loopSounds[entityNum].active = true;
	loopSounds[entityNum].kill = true;
	loopSounds[entityNum].doppler = false;
	loopSounds[entityNum].oldDopplerScale = 1.0;
	loopSounds[entityNum].dopplerScale = 1.0;
	loopSounds[entityNum].sfx = sfx;

	if (s_doppler->value && VectorLength2(velocity) > 0.0)
	{
		vec3_t	out;
		float	lena, lenb;

		loopSounds[entityNum].doppler = true;
		lena = VectorDistance2(loopSounds[listener_number].origin, loopSounds[entityNum].origin);
		VectorAdd(loopSounds[entityNum].origin, loopSounds[entityNum].velocity, out);
		lenb = VectorDistance2(loopSounds[listener_number].origin, out);
		if ((loopSounds[entityNum].framenum + 1) != cls.framecount)
			loopSounds[entityNum].oldDopplerScale = 1.0;
		else loopSounds[entityNum].oldDopplerScale = loopSounds[entityNum].dopplerScale;
		loopSounds[entityNum].dopplerScale = lenb/(lena * 100);
		if (loopSounds[entityNum].dopplerScale<=1.0)
			loopSounds[entityNum].doppler = false;	// don't bother doing the math
	}
	loopSounds[entityNum].framenum = cls.framecount;
}

/*
==================
S_AddLoopingSound

Called during entity generation for a frame
Include velocity in case I get around to doing doppler...
==================
*/
void S_AddRealLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sound_t sfxHandle )
{
	sfx_t *sfx;

	if ( !s_soundStarted || s_soundMuted ) return;

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx )
	{
		MsgWarn("S_AddRealLoopingSound: handle %i out of range\n", sfxHandle );
		return;
	}

	sfx = &s_knownSfx[ sfxHandle ];

	if(sfx->inMemory == false) S_memoryLoad(sfx);
	if(!sfx->soundLength )
	{
		Host_Error("S_AddLoopingSound: %s with length 0", sfx->soundName );
		return;
	}
	
	VectorCopy( origin, loopSounds[entityNum].origin );
	VectorCopy( velocity, loopSounds[entityNum].velocity );
	loopSounds[entityNum].sfx = sfx;
	loopSounds[entityNum].active = true;
	loopSounds[entityNum].kill = false;
	loopSounds[entityNum].doppler = false;
}



/*
==================
S_AddLoopSounds

Spatialize all of the looping sounds.
All sounds are on the same cycle, so any duplicates can just
sum up the channel multipliers.
==================
*/
void S_AddLoopSounds (void)
{
	int		i, j, left_total, right_total, left, right;
	loopSound_t	*loop, *loop2;
	static int	loopFrame;
	channel_t		*ch;
	float		time;

	numLoopChannels = 0;

	time = Sys_DoubleTime();

	loopFrame++;
	for ( i = 0; i < MAX_MAP_ENTITIES; i++)
	{
		loop = &loopSounds[i];
		if ( !loop->active || loop->mergeFrame == loopFrame )
			continue;	// already merged into an earlier sound

		if (loop->kill) S_SpatializeOrigin( loop->origin, 127, &left_total, &right_total); // 3d
		else S_SpatializeOrigin( loop->origin, 90,  &left_total, &right_total); // sphere
		loop->sfx->lastTimeUsed = time;

		for (j = (i+1); j < MAX_MAP_ENTITIES; j++)
		{
			loop2 = &loopSounds[j];
			if ( !loop2->active || loop2->doppler || loop2->sfx != loop->sfx)
				continue;
			loop2->mergeFrame = loopFrame;

			if (loop2->kill) S_SpatializeOrigin( loop2->origin, 127, &left, &right); // 3d
			else S_SpatializeOrigin( loop2->origin, 90,  &left, &right); // sphere

			loop2->sfx->lastTimeUsed = time;
			left_total += left;
			right_total += right;
		}

		if (left_total == 0 && right_total == 0) continue; // not audible
		// allocate a channel
		ch = &loop_channels[numLoopChannels];
		
		if (left_total > 255) left_total = 255;
		if (right_total > 255) right_total = 255;
		
		ch->master_vol = 127;
		ch->leftvol = left_total;
		ch->rightvol = right_total;
		ch->thesfx = loop->sfx;
		ch->doppler = loop->doppler;
		ch->dopplerScale = loop->dopplerScale;
		ch->oldDopplerScale = loop->oldDopplerScale;
		numLoopChannels++;

		if (numLoopChannels == MAX_CHANNELS)
			return;
	}
}

/*
=================
S_ByteSwapRawSamples

If raw data has been loaded in little endien binary form, this must be done.
If raw data was calculated, as with ADPCM, this should not be called.
=================
*/
void S_ByteSwapRawSamples( int samples, int width, int s_channels, const byte *data )
{
	int	i;

	if( width != 2 ) return;
	if( LittleShort( 256 ) == 256 ) return;

	if ( s_channels == 2 ) samples <<= 1;
	for ( i = 0; i < samples; i++ )
		((short *)data)[i] = LittleShort( ((short *)data)[i] );
}

portable_samplepair_t *S_GetRawSamplePointer( void )
{
	return s_rawsamples;
}

/*
============
S_RawSamples

Music streaming
============
*/
void S_RawSamples( int samples, int rate, int width, int s_channels, const byte *data, float volume )
{
	int		i;
	int		src, dst;
	float		scale;
	int		intVolume;

	if ( !s_soundStarted || s_soundMuted ) return;

	intVolume = 256 * volume;

	if ( s_rawend < s_soundtime )
	{
		MsgDev( D_WARN, "S_RawSamples: resetting minimum: %i < %i\n", s_rawend, s_soundtime );
		s_rawend = s_soundtime;
	}

	scale = (float)rate / dma.speed;

	if (s_channels == 2 && width == 2)
	{
		if (scale == 1.0)
		{	
			// optimized case
			for (i = 0; i < samples; i++)
			{
				dst = s_rawend&(MAX_RAW_SAMPLES-1);
				s_rawend++;
				s_rawsamples[dst].left = ((short *)data)[i*2] * intVolume;
				s_rawsamples[dst].right = ((short *)data)[i*2+1] * intVolume;
			}
		}
		else
		{
			for (i = 0;;i++)
			{
				src = i*scale;
				if (src >= samples) break;
				dst = s_rawend&(MAX_RAW_SAMPLES-1);
				s_rawend++;
				s_rawsamples[dst].left = ((short *)data)[src*2] * intVolume;
				s_rawsamples[dst].right = ((short *)data)[src*2+1] * intVolume;
			}
		}
	}
	else if(s_channels == 1 && width == 2)
	{
		for (i = 0;;i++)
		{
			src = i*scale;
			if (src >= samples) break;
			dst = s_rawend&(MAX_RAW_SAMPLES-1);
			s_rawend++;
			s_rawsamples[dst].left = ((short *)data)[src] * intVolume;
			s_rawsamples[dst].right = ((short *)data)[src] * intVolume;
		}
	}
	else if(s_channels == 2 && width == 1)
	{
		intVolume *= 256;

		for (i = 0;;i++)
		{
			src = i*scale;
			if (src >= samples) break;
			dst = s_rawend&(MAX_RAW_SAMPLES-1);
			s_rawend++;
			s_rawsamples[dst].left = ((char *)data)[src*2] * intVolume;
			s_rawsamples[dst].right = ((char *)data)[src*2+1] * intVolume;
		}
	}
	else if (s_channels == 1 && width == 1)
	{
		intVolume *= 256;

		for (i = 0;;i++)
		{
			src = i*scale;
			if (src >= samples) break;
			dst = s_rawend&(MAX_RAW_SAMPLES-1);
			s_rawend++;
			s_rawsamples[dst].left = (((byte *)data)[src]-128) * intVolume;
			s_rawsamples[dst].right = (((byte *)data)[src]-128) * intVolume;
		}
	}

	if ( s_rawend > s_soundtime + MAX_RAW_SAMPLES )
		MsgDev(D_WARN,"S_RawSamples: overflowed %i > %i\n", s_rawend, s_soundtime );
}

/*
=====================
S_UpdateEntityPosition

let the sound system know where an entity currently is
======================
*/
void S_UpdateEntityPosition( int entityNum, const vec3_t origin )
{
	if ( entityNum < 0 || entityNum > MAX_MAP_ENTITIES )
		Host_Error("S_UpdateEntityPosition: bad entitynum %i\n", entityNum );
	VectorCopy( origin, loopSounds[entityNum].origin );
}

/*
============
S_Respatialize

Change the volumes of all the playing sounds for changes in their positions
============
*/
void S_Respatialize( int entityNum, const vec3_t head, vec3_t v_forward, vec3_t v_right, vec3_t v_up )
{
	int		i;
	channel_t		*ch;
	vec3_t		origin;

	if ( !s_soundStarted || s_soundMuted ) return;

	listener_number = entityNum;
	VectorCopy(head, listener_origin);
	VectorCopy(v_forward, listener_axis[0]);
	VectorCopy(v_right, listener_axis[1]);
	VectorCopy(v_up, listener_axis[2]);

	// update spatialization for dynamic sounds	
	ch = s_channels;
	for ( i = 0; i < MAX_CHANNELS; i++, ch++ )
	{
		if ( !ch->thesfx ) continue;
		// anything coming from the view entity will always be full volume
		if (ch->entnum == listener_number)
		{
			ch->leftvol = ch->master_vol;
			ch->rightvol = ch->master_vol;
		}
		else
		{
			if (ch->fixed_origin)
			{
				VectorCopy( ch->origin, origin );
			}
			else
			{
				VectorCopy( loopSounds[ ch->entnum ].origin, origin );
			}
			S_SpatializeOrigin (origin, ch->master_vol, &ch->leftvol, &ch->rightvol);
		}
	}
	// add loopsounds
	S_AddLoopSounds ();
}

/*
========================
S_ScanChannelStarts

Returns true if any new sounds were started since the last mix
========================
*/
bool S_ScanChannelStarts( void )
{
	channel_t		*ch;
	int		i;
	bool		newSamples;

	newSamples = false;
	ch = s_channels;

	for (i = 0; i < MAX_CHANNELS; i++, ch++)
	{
		if ( !ch->thesfx ) continue;
		if ( ch->startSample == START_SAMPLE_IMMEDIATE )
		{
			ch->startSample = s_paintedtime;
			newSamples = true;
			continue;
		}

		// if it is completely finished by now, clear it
		if ( ch->startSample + (ch->thesfx->soundLength) <= s_paintedtime )
			S_ChannelFree(ch);
	}
	return newSamples;
}

/*
============
S_Update

Called once each time through the main loop
============
*/
void S_Update( void )
{
	int		i, total;
	channel_t		*ch;

	if ( !s_soundStarted || s_soundMuted ) return;

	// debugging output
	if ( s_show->value == 2 )
	{
		total = 0;
		ch = s_channels;
		for (i = 0; i < MAX_CHANNELS; i++, ch++)
		{
			if (ch->thesfx && (ch->leftvol || ch->rightvol) )
			{
				Msg("%f %f %s\n", ch->leftvol, ch->rightvol, ch->thesfx->soundName);
				total++;
			}
		}
		Msg ("----(%i)---- painted: %i\n", total, s_paintedtime);
	}

	S_UpdateBackgroundTrack(); // add raw data from streamed samples
	S_Update_();// mix some sound
}

void S_GetSoundtime( void )
{
	int		samplepos;
	static int	buffers;
	static int	oldsamplepos;
	int		fullsamples;
	
	fullsamples = dma.samples / dma.channels;

	// it is possible to miscount buffers if it has wrapped twice between
	// calls to S_Update. Oh well.
	samplepos = SNDDMA_GetDMAPos();
	if (samplepos < oldsamplepos)
	{
		buffers++; // buffer wrapped
		
		if (s_paintedtime > 0x40000000)
		{	
			// time to chop things off to avoid 32 bit limits
			buffers = 0;
			s_paintedtime = fullsamples;
			S_StopAllSounds ();
		}
	}

	oldsamplepos = samplepos;
	s_soundtime = buffers * fullsamples + samplepos/dma.channels;

	// check to make sure that we haven't overshot
	if (s_paintedtime < s_soundtime)
	{
		Msg("S_Update_ : overflow\n");
		s_paintedtime = s_soundtime;
	}

	if ( dma.submission_chunk < 256 )
		s_paintedtime = s_soundtime + s_mixPreStep->value * dma.speed;
	else s_paintedtime = s_soundtime + dma.submission_chunk;
}


void S_Update_(void)
{
	float		endtime;
	int		samps;
	static float	lastTime = 0.0f;
	float		ma, op;
	float		thisTime, sane;
	static float	ot = -1;

	if ( !s_soundStarted || s_soundMuted ) return;

	thisTime = Sys_DoubleTime();

	// Updates s_soundtime
	S_GetSoundtime();

	if (s_soundtime == ot) return;
	ot = s_soundtime;

	// clear any sound effects that end before the current time,
	// and start any new sounds
	S_ScanChannelStarts();

	sane = thisTime - lastTime;
	if (sane < 11) sane = 11; // 85hz
	ma = s_mixahead->value * dma.speed;
	op = s_mixPreStep->value + sane*dma.speed * 0.01;

	if (op < ma) ma = op;

	// mix ahead of current position
	endtime = s_soundtime + ma;

	// mix to an even submission block size
	endtime = (int)(endtime + dma.submission_chunk-1) & ~(dma.submission_chunk-1);

	// never mix more than the complete buffer
	samps = dma.samples >> (dma.channels-1);
	if (endtime - s_soundtime > samps) endtime = s_soundtime + samps;

	SNDDMA_BeginPainting();
	S_PaintChannels( endtime );
	SNDDMA_Submit();

	lastTime = thisTime;
}

/*
===============================================================================

console functions

===============================================================================
*/
void S_Play_f( void )
{
	int 	i = 1;
	char	name[256];
	
	while ( i < Cmd_Argc())
	{
		if ( !strrchr(Cmd_Argv(i), '.')) sprintf( name, "%s.wav", Cmd_Argv(1) );
		else strncpy( name, Cmd_Argv(i), sizeof(name) );
		S_StartLocalSound( name );
		i++;
	}
}

void S_Music_f( void )
{
	int	c = Cmd_Argc();

	if ( c == 2 )
	{
		S_StartBackgroundTrack( Cmd_Argv(1), Cmd_Argv(1) );
		s_backgroundLoop[0] = 0;
	} 
	else if ( c == 3 )
	{
		S_StartBackgroundTrack( Cmd_Argv(1), Cmd_Argv(2) );
	}
	else Msg("music <musicfile> [loopfile]\n");
}

void S_SoundList_f( void )
{
	sfx_t	*sfx;
	int	i, size, total = 0;
	char	type[4][16];
	char	mem[2][16];

	strcpy(type[0], "16bit");
	strcpy(type[1], "adpcm");
	strcpy(type[2], "daub4");
	strcpy(type[3], "mulaw");
	strcpy(mem[0], "paged out");
	strcpy(mem[1], "resident ");

	for (sfx = s_knownSfx, i = 0; i < s_numSfx; i++, sfx++)
	{
		size = sfx->soundLength;
		total += size;
		Msg("%6i[%s] : %s[%s]\n", size, type[sfx->soundCompressionMethod], sfx->soundName, mem[sfx->inMemory] );
	}
	Msg ("Total resident: %i\n", total);
	S_DisplayFreeMemory();
}


/*
===============================================================================

background music functions

===============================================================================
*/

int GetLittleLong( file_t *f )
{
	int		v;

	FS_Read( f, &v, sizeof(v));
	return LittleLong(v);
}

int GetLittleShort( file_t *f )
{
	short	v;

	FS_Read( f, &v, sizeof(v));
	return LittleShort(v);
}

int S_FindWavChunk( file_t *f, char *chunk )
{
	char	name[5];
	int	r, len;

	name[4] = 0;
	len = 0;
	r = FS_Read( f, name, 4 );

	if ( r != 4 ) return 0;
	len = GetLittleLong( f );
	if ( len < 0 || len > 0xfffffff )
	{
		len = 0;
		return 0;
	}
	len = (len + 1 ) & ~1; // pad to word boundary

	if(strcmp( name, chunk )) return 0;

	return len;
}

/*
======================
S_StopBackgroundTrack
======================
*/
void S_StopBackgroundTrack( void )
{
	if ( !s_backgroundFile ) return;

	FS_Close( s_backgroundFile );
	s_backgroundFile = NULL;
	s_rawend = 0;
}

/*
======================
S_StartBackgroundTrack
======================
*/
void S_StartBackgroundTrack( const char *intro, const char *loop )
{
	int	len;
	char	dump[16];
	char	name[MAX_QPATH];

	if ( !intro ) intro = "";
	if ( !loop || !loop[0] ) loop = intro;
	MsgDev(D_INFO,"S_StartBackgroundTrack( %s, %s )\n", intro, loop );

	FS_StripExtension( (char *)intro );
	sprintf (name, "music/%s", intro );
	FS_DefaultExtension( name, ".wav" );

	if ( !intro[0] ) return;

	strncpy( s_backgroundLoop, loop, sizeof( s_backgroundLoop ) );

	// close the background track, but DON'T reset s_rawend
	// if restarting the same back ground track
	if ( s_backgroundFile )
	{
		FS_Close( s_backgroundFile );
		s_backgroundFile = NULL;
	}

	// open up a wav file and get all the info
	s_backgroundFile = FS_Open( name, "rb" );
	if ( !s_backgroundFile )
	{
		MsgWarn("S_StartBackgroundTrack: couldn't open music file %s\n", name );
		return;
	}

	// skip the riff wav header
	FS_Read(s_backgroundFile, dump, 12 );

	if(!S_FindWavChunk( s_backgroundFile, "fmt " ))
	{
		MsgDev(D_WARN,"S_StartBackgroundTrack: no fmt chunk in %s\n", name );
		FS_Close( s_backgroundFile );
		s_backgroundFile = NULL;
		return;
	}

	// save name for soundinfo
	s_backgroundInfo.format = GetLittleShort( s_backgroundFile );
	s_backgroundInfo.channels = GetLittleShort( s_backgroundFile );
	s_backgroundInfo.rate = GetLittleLong( s_backgroundFile );
	GetLittleLong(  s_backgroundFile );
	GetLittleShort(  s_backgroundFile );
	s_backgroundInfo.width = GetLittleShort( s_backgroundFile ) / 8;

	if ( s_backgroundInfo.format != WAV_FORMAT_PCM )
	{
		FS_Close( s_backgroundFile );
		s_backgroundFile = 0;
		MsgWarn("S_StartBackgroundTrack: not a microsoft PCM format wav: %s\n", name);
		return;
	}

	if ( s_backgroundInfo.channels != 2 || s_backgroundInfo.rate != 22050 )
		MsgWarn("S_StartBackgroundTrack: music file %s is not 22k stereo\n", name );

	if (( len = S_FindWavChunk( s_backgroundFile, "data" )) == 0 )
	{
		FS_Close( s_backgroundFile );
		s_backgroundFile = NULL;
		MsgDev(D_WARN, "S_StartBackgroundTrack: no data chunk in %s\n", name);
		return;
	}

	s_backgroundInfo.samples = len / (s_backgroundInfo.width * s_backgroundInfo.channels);
	s_backgroundSamples = s_backgroundInfo.samples;
}

/*
======================
S_UpdateBackgroundTrack
======================
*/
void S_UpdateBackgroundTrack( void )
{
	int		bufferSamples;
	int		fileSamples;
	byte		raw[30000]; // just enough to fit in a mac stack frame
	int		fileBytes;
	int		r;
	static float	musicVolume = 0.5f;

	if ( !s_backgroundFile ) return;

	// graeme see if this is OK
	musicVolume = (musicVolume + (s_musicVolume->value * 2))/4.0f;

	// don't bother playing anything if musicvolume is 0
	if ( musicVolume <= 0 ) return;

	// see how many samples should be copied into the raw buffer
	if ( s_rawend < s_soundtime ) s_rawend = s_soundtime;

	while ( s_rawend < s_soundtime + MAX_RAW_SAMPLES )
	{
		bufferSamples = MAX_RAW_SAMPLES - (s_rawend - s_soundtime);

		// decide how much data needs to be read from the file
		fileSamples = bufferSamples * s_backgroundInfo.rate / dma.speed;

		// don't try and read past the end of the file
		if ( fileSamples > s_backgroundSamples )
			fileSamples = s_backgroundSamples;

		// our max buffer size
		fileBytes = fileSamples * (s_backgroundInfo.width * s_backgroundInfo.channels);
		if ( fileBytes > sizeof(raw) )
		{
			fileBytes = sizeof(raw);
			fileSamples = fileBytes / (s_backgroundInfo.width * s_backgroundInfo.channels);
		}

		r = FS_Read( s_backgroundFile, raw, fileBytes );
		if ( r != fileBytes )
		{
			S_StopBackgroundTrack();
			return;
		}

		// byte swap if needed
		S_ByteSwapRawSamples( fileSamples, s_backgroundInfo.width, s_backgroundInfo.channels, raw );

		// add to raw buffer
		S_RawSamples( fileSamples, s_backgroundInfo.rate, s_backgroundInfo.width, s_backgroundInfo.channels, raw, musicVolume );

		s_backgroundSamples -= fileSamples;
		if ( !s_backgroundSamples )
		{
			// loop
			if (s_backgroundLoop[0])
			{
				FS_Close( s_backgroundFile );
				s_backgroundFile = NULL;
				S_StartBackgroundTrack( s_backgroundLoop, s_backgroundLoop );
				if ( !s_backgroundFile ) return; // loop failed to restart
			}
			else
			{
				s_backgroundFile = NULL;
				return;
			}
		}
	}
}


/*
======================
S_FreeOldestSound
======================
*/
void S_FreeOldestSound( void )
{
	int	i, used = 0;
	sfx_t	*sfx;
	sndBuffer	*buffer, *nbuffer;
	float	oldest;

	oldest = Sys_DoubleTime();

	for (i = 1; i < s_numSfx; i++)
	{
		sfx = &s_knownSfx[i];
		if (sfx->inMemory && sfx->lastTimeUsed<oldest)
		{
			used = i;
			oldest = sfx->lastTimeUsed;
		}
	}

	sfx = &s_knownSfx[used];
	MsgDev(D_INFO, "S_FreeOldestSound: freeing sound %s\n", sfx->soundName);

	buffer = sfx->soundData;
	while(buffer != NULL)
	{
		nbuffer = buffer->next;
		SND_free(buffer);
		buffer = nbuffer;
	}
	sfx->inMemory = false;
	sfx->soundData = NULL;
}
