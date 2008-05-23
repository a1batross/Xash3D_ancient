//=======================================================================
//			Copyright XashXT Group 2007 ©
//			   cl_sound.c - sound engine
//=======================================================================

#include "client.h"
#include <dsound.h>

#define	PAINTBUFFER_SIZE	4096			// this is in samples

#define SND_CHUNK_SIZE	1024			// samples
#define SND_CHUNK_SIZE_FLOAT	(SND_CHUNK_SIZE/2)		// floats
#define SND_CHUNK_SIZE_BYTE	(SND_CHUNK_SIZE*2)		// floats

typedef struct
{
	int	left;	// the final values will be clamped to +/- 0x00ffff00 and shifted down
	int	right;
} portable_samplepair_t;

typedef struct adpcm_state
{
	short	sample;	// Previous output value
	char	index;	// Index into stepsize table
} adpcm_state_t;

typedef	struct sndBuffer_s {
	short					sndChunk[SND_CHUNK_SIZE];
	struct sndBuffer_s		*next;
    int						size;
	adpcm_state_t			adpcm;
} sndBuffer;

typedef struct sfx_s {
	sndBuffer		*soundData;
	bool		defaultSound;			// couldn't be loaded, so use buzz
	bool		inMemory;				// not in Memory
	bool		soundCompressed;		// not in Memory
	int		soundCompressionMethod;	
	int 		soundLength;
	char 		soundName[MAX_QPATH];
	float		lastTimeUsed;
	struct sfx_s	*next;
} sfx_t;

typedef struct {
	int			channels;
	int			samples;				// mono samples in buffer
	int			submission_chunk;		// don't mix less than this #
	int			samplebits;
	int			speed;
	byte		*buffer;
} dma_t;

#define START_SAMPLE_IMMEDIATE	0x7fffffff

typedef struct loopSound_s {
	vec3_t		origin;
	vec3_t		velocity;
	sfx_t		*sfx;
	int			mergeFrame;
	bool	active;
	bool	kill;
	bool	doppler;
	float		dopplerScale;
	float		oldDopplerScale;
	int			framenum;
} loopSound_t;

typedef struct
{
	float	allocTime;
	int	startSample;	// START_SAMPLE_IMMEDIATE = set immediately on next mix
	int	entnum;		// to allow overriding a specific sound
	int	entchannel;	// to allow overriding a specific sound
	int	leftvol;		// 0-255 volume after spatialization
	int	rightvol;		// 0-255 volume after spatialization
	int	master_vol;	// 0-255 volume before spatialization
	float	dopplerScale;
	float	oldDopplerScale;
	vec3_t	origin;		// only use if fixed_origin is set
	bool	fixed_origin;	// use origin instead of fetching entnum's origin
	sfx_t	*thesfx;		// sfx structure
	bool	doppler;
} channel_t;


#define	WAV_FORMAT_PCM		1


typedef struct
{
	int	format;
	int	rate;
	int	width;
	int	channels;
	int	samples;
	int	dataofs;		// chunk starts this many bytes from file start
} wavinfo_t;


/*
====================================================================

  SYSTEM SPECIFIC FUNCTIONS

====================================================================
*/

// initializes cycling through a DMA buffer and returns information on it
bool SNDDMA_Init(void);

// gets the current DMA position
int		SNDDMA_GetDMAPos(void);

// shutdown the DMA xfer.
void	SNDDMA_Shutdown(void);

void	SNDDMA_BeginPainting (void);

void	SNDDMA_Submit(void);

//====================================================================

#define	MAX_CHANNELS			96

extern channel_t	s_channels[MAX_CHANNELS];
extern channel_t	loop_channels[MAX_CHANNELS];
extern int	numLoopChannels;

extern	int		s_paintedtime;
extern	int		s_rawend;
extern	vec3_t	listener_forward;
extern	vec3_t	listener_right;
extern	vec3_t	listener_up;
extern	dma_t	dma;

#define	MAX_RAW_SAMPLES	16384
extern	portable_samplepair_t	s_rawsamples[MAX_RAW_SAMPLES];

bool S_LoadSound( sfx_t *sfx );

void		SND_free(sndBuffer *v);
sndBuffer*	SND_malloc();
void		SND_setup();

void S_PaintChannels(int endtime);

void S_memoryLoad(sfx_t *sfx);
portable_samplepair_t *S_GetRawSamplePointer();

// spatializes a channel
void S_Spatialize(channel_t *ch);

// adpcm functions
int  S_AdpcmMemoryNeeded( const wavinfo_t *info );
void S_AdpcmEncodeSound( sfx_t *sfx, short *samples );
void S_AdpcmGetSamples(sndBuffer *chunk, short *to);

// wavelet function

#define SENTINEL_MULAW_ZERO_RUN 127
#define SENTINEL_MULAW_FOUR_BIT_RUN 126

void S_FreeOldestSound();

#define	NXStream byte

void encodeWavelet(sfx_t *sfx, short *packets);
void decodeWavelet( sndBuffer *stream, short *packets);

void encodeMuLaw( sfx_t *sfx, short *packets);
extern short mulawToShort[256];

extern short *sfxScratchBuffer;
extern sfx_t *sfxScratchPointer;
extern int sfxScratchIndex;

static HRESULT (_stdcall *pDirectSoundCreate)(GUID* lpGUID, LPDIRECTSOUND* lplpDS, IUnknown* pUnkOuter);

static dllfunc_t dsound_funcs[] =
{
	{"DirectSoundCreate", (void **) &pDirectSoundCreate },
	{ NULL, NULL }
};
dll_info_t dsound_dll = { "dsound.dll", dsound_funcs, NULL, NULL, NULL, false, 0 };

#define SECONDARY_BUFFER_SIZE	0x10000
static bool dsound_init;
static int sample16;
static dword gSndBufSize;
static dword locksize;
static LPDIRECTSOUND pDS;
static LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;

#undef DEFINE_GUID

#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
EXTERN_C const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

DEFINE_GUID(CLSID_DirectSound, 0x47d4d946, 0x62e8, 0x11cf, 0x93, 0xbc, 0x44, 0x45, 0x53, 0x54, 0x0, 0x0);
DEFINE_GUID(CLSID_DirectSound8, 0x3901cc3f, 0x84b5, 0x4fa4, 0xba, 0x35, 0xaa, 0x81, 0x72, 0xb8, 0xa0, 0x9b);
DEFINE_GUID(IID_IDirectSound8, 0xC50A7E93, 0xF395, 0x4834, 0x9E, 0xF6, 0x7F, 0xA9, 0x9D, 0xE5, 0x09, 0x66);
DEFINE_GUID(IID_IDirectSound, 0x279AFA83, 0x4981, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60);

static const char *DSoundError( int error )
{
	switch ( error )
	{
	case DSERR_BUFFERLOST: return "DSERR_BUFFERLOST";
	case DSERR_INVALIDCALL: return "DSERR_INVALIDCALLS";
	case DSERR_INVALIDPARAM: return "DSERR_INVALIDPARAM";
	case DSERR_PRIOLEVELNEEDED: return "DSERR_PRIOLEVELNEEDED";
	}
	return "unknown error";
}

/*
==================
SNDDMA_Shutdown
==================
*/
void SNDDMA_Shutdown( void )
{
	MsgDev(D_INFO, "Shutting down sound system\n" );

	if ( pDS )
	{
		MsgDev(D_INFO, "Destroying DS buffers\n" );
		if ( pDS )
		{
			MsgDev(D_INFO, "...setting NORMAL coop level\n" );
			pDS->lpVtbl->SetCooperativeLevel( pDS, host.hWnd, DSSCL_PRIORITY );
		}

		if ( pDSBuf )
		{
			MsgDev(D_INFO, "...stopping and releasing sound buffer\n" );
			pDSBuf->lpVtbl->Stop( pDSBuf );
			pDSBuf->lpVtbl->Release( pDSBuf );
		}
		if ( pDSPBuf && ( pDSBuf != pDSPBuf ) )
		{
			MsgDev(D_INFO, "...releasing primary buffer\n" );
			pDSPBuf->lpVtbl->Release( pDSPBuf );
		}
		pDSBuf = NULL;
		pDSPBuf = NULL;
		dma.buffer = NULL;

		MsgDev(D_INFO,  "...releasing DS object\n" );
		pDS->lpVtbl->Release( pDS );
	}

	Sys_FreeLibrary( &dsound_dll );

	pDS = NULL;
	pDSBuf = NULL;
	pDSPBuf = NULL;
	dsound_init = false;
	memset ((void *)&dma, 0, sizeof (dma));
	CoUninitialize();
}

int SNDDMA_InitDS( void )
{
	HRESULT		hresult;
	DSBUFFERDESC	dsbuf;
	DSBCAPS		dsbcaps;
	WAVEFORMATEX	format;
	int		use8 = 1;

	MsgDev(D_INFO, "Initializing DirectSound - ");

	// Create IDirectSound using the primary sound device
	if(FAILED( hresult = CoCreateInstance(&CLSID_DirectSound8, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectSound8, (void **)&pDS)))
	{
		use8 = 0;
		if( FAILED( hresult = CoCreateInstance(&CLSID_DirectSound, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectSound, (void **)&pDS)))
		{
			MsgDev(D_INFO, "failed\n");
			SNDDMA_Shutdown();
			return false;
		}
	}

	hresult = pDS->lpVtbl->Initialize( pDS, NULL);
	MsgDev(D_INFO, "ok\n" );

	MsgDev(D_INFO, "...setting DSSCL_PRIORITY coop level: " );
	if(DS_OK != pDS->lpVtbl->SetCooperativeLevel( pDS, host.hWnd, DSSCL_PRIORITY ))
	{
		MsgDev(D_INFO, "failed\n");
		SNDDMA_Shutdown ();
		return false;
	}
	MsgDev(D_INFO, "ok\n" );

	// create the secondary buffer we'll actually work with
	dma.channels = 2;
	dma.samplebits = 16;
	dma.speed = 22050;
	memset (&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = dma.channels;
	format.wBitsPerSample = dma.samplebits;
	format.nSamplesPerSec = dma.speed;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec*format.nBlockAlign; 

	memset (&dsbuf, 0, sizeof(dsbuf));
	dsbuf.dwSize = sizeof(DSBUFFERDESC);
	dsbuf.dwFlags = DSBCAPS_LOCHARDWARE;
	if (use8) dsbuf.dwFlags |= DSBCAPS_GETCURRENTPOSITION2;
	dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
	dsbuf.lpwfxFormat = &format;
	
	memset(&dsbcaps, 0, sizeof(dsbcaps));
	dsbcaps.dwSize = sizeof(dsbcaps);
	
	MsgDev(D_INFO, "...creating secondary buffer: " );
	if (DS_OK == pDS->lpVtbl->CreateSoundBuffer(pDS, &dsbuf, &pDSBuf, NULL))
	{
		MsgDev(D_INFO, "locked hardware.  ok\n" );
	}
	else
	{
		// Couldn't get hardware, fallback to software.
		dsbuf.dwFlags = DSBCAPS_LOCSOFTWARE;
		if (use8) dsbuf.dwFlags |= DSBCAPS_GETCURRENTPOSITION2;
		if (DS_OK != pDS->lpVtbl->CreateSoundBuffer(pDS, &dsbuf, &pDSBuf, NULL))
		{
			MsgDev(D_INFO, "failed\n" );
			SNDDMA_Shutdown ();
			return false;
		}
		MsgDev(D_INFO, "forced to software.  ok\n" );
	}
		
	// Make sure mixer is active
	if ( DS_OK != pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING))
	{
		Msg("*** Looped sound play failed ***\n");
		SNDDMA_Shutdown();
		return false;
	}

	// get the returned buffer size
	if ( DS_OK != pDSBuf->lpVtbl->GetCaps (pDSBuf, &dsbcaps))
	{
		Msg("*** GetCaps failed ***\n");
		SNDDMA_Shutdown();
		return false;
	}
	
	gSndBufSize = dsbcaps.dwBufferBytes;
	dma.channels = format.nChannels;
	dma.samplebits = format.wBitsPerSample;
	dma.speed = format.nSamplesPerSec;
	dma.samples = gSndBufSize/(dma.samplebits/8);
	dma.submission_chunk = 1;
	dma.buffer = NULL; // must be locked first

	sample16 = (dma.samplebits/8) - 1;

	SNDDMA_BeginPainting ();
	if (dma.buffer) memset(dma.buffer, 0, dma.samples * dma.samplebits/8);
	SNDDMA_Submit ();
	return 1;
}

/*
==================
SNDDMA_Init

Initialize direct sound
Returns false if failed
==================
*/
bool SNDDMA_Init(void)
{
	memset ((void *)&dma, 0, sizeof (dma));
	dsound_init = 0;
	CoInitialize(NULL);
	if(!SNDDMA_InitDS()) return false;

	dsound_init = true;
	MsgDev(D_INFO, "Completed successfully\n" );

	return true;
}


/*
==============
SNDDMA_GetDMAPos

return the current sample position (in mono samples read)
inside the recirculating dma buffer, so the mixing code will know
how many sample are required to fill it up.
===============
*/
int SNDDMA_GetDMAPos( void )
{
	MMTIME	mmtime;
	int	s;
	DWORD	dwWrite;

	if( !dsound_init ) return 0;

	mmtime.wType = TIME_SAMPLES;
	pDSBuf->lpVtbl->GetCurrentPosition(pDSBuf, &mmtime.u.sample, &dwWrite);

	s = mmtime.u.sample;
	s >>= sample16;
	s &= (dma.samples-1);

	return s;
}

/*
==============
SNDDMA_BeginPainting

Makes sure dma.buffer is valid
===============
*/
void SNDDMA_BeginPainting( void )
{
	int	reps;
	dword	dwSize2;
	dword	*pbuf, *pbuf2;
	HRESULT	hresult;
	dword	dwStatus;

	if ( !pDSBuf ) return;

	// if the buffer was lost or stopped, restore it and/or restart it
	if ( pDSBuf->lpVtbl->GetStatus (pDSBuf, &dwStatus) != DS_OK )
		MsgDev(D_INFO, "Couldn't get sound buffer status\n");
	
	if (dwStatus & DSBSTATUS_BUFFERLOST) pDSBuf->lpVtbl->Restore(pDSBuf);
	if (!(dwStatus & DSBSTATUS_PLAYING)) pDSBuf->lpVtbl->Play(pDSBuf, 0, 0, DSBPLAY_LOOPING);

	// lock the dsound buffer
	reps = 0;
	dma.buffer = NULL;

	while ((hresult = pDSBuf->lpVtbl->Lock(pDSBuf, 0, gSndBufSize, &pbuf, &locksize, &pbuf2, &dwSize2, 0)) != DS_OK)
	{
		if (hresult != DSERR_BUFFERLOST)
		{
			MsgDev(D_INFO, "SNDDMA_BeginPainting: Lock failed with error '%s'\n", DSoundError( hresult ));
			S_Shutdown ();
			return;
		}
		else pDSBuf->lpVtbl->Restore( pDSBuf );

		if (++reps > 2) return;
	}
	dma.buffer = (byte *)pbuf;
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
Also unlocks the dsound buffer
===============
*/
void SNDDMA_Submit( void )
{
	// unlock the dsound buffer
	if ( pDSBuf ) pDSBuf->lpVtbl->Unlock(pDSBuf, dma.buffer, locksize, NULL, 0);
}


/*
=================
SNDDMA_Activate

When we change windows we need to do this
=================
*/
void SNDDMA_Activate( void )
{
	if( !pDS ) return;

	if( DS_OK != pDS->lpVtbl->SetCooperativeLevel( pDS, host.hWnd, DSSCL_PRIORITY ))
	{
		MsgDev(D_INFO, "sound SetCooperativeLevel failed\n");
		SNDDMA_Shutdown();
	}
}

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

	s_volume = Cvar_Get ("s_volume", "0.8", CVAR_ARCHIVE);
	s_musicVolume = Cvar_Get ("s_musicvolume", "0.25", CVAR_ARCHIVE);
	s_separation = Cvar_Get ("s_separation", "0.5", CVAR_ARCHIVE);
	s_doppler = Cvar_Get ("s_doppler", "1", CVAR_ARCHIVE);
	s_khz = Cvar_Get ("s_khz", "22", CVAR_ARCHIVE);
	s_mixahead = Cvar_Get ("s_mixahead", "0.2", CVAR_ARCHIVE);
	s_mixPreStep = Cvar_Get ("s_mixPreStep", "0.05", CVAR_ARCHIVE);
	s_show = Cvar_Get ("s_show", "0", CVAR_CHEAT);
	s_testsound = Cvar_Get ("s_testsound", "0", CVAR_CHEAT);

	cv = Cvar_Get("s_initsound", "1", 0);
	if ( !cv->value ) return;

	Cmd_AddCommand("play", S_Play_f, "playing a specified sound file" );
	Cmd_AddCommand("music", S_Music_f, "starting a background track" );
	Cmd_AddCommand("snd_list", S_SoundList_f, "display loaded sounds" );
	Cmd_AddCommand("snd_info", S_SoundInfo_f, "print sound system information" );
	Cmd_AddCommand("snd_stop", S_StopAllSounds, "stop all sounds" );

	if (SNDDMA_Init())
	{
		s_soundStarted = 1;
		s_soundMuted = 1;
		memset(sfxHash, 0, sizeof(sfx_t *) * LOOP_HASH);
		s_soundtime = 0;
		s_paintedtime = 0;
		S_StopAllSounds ();
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
			MsgDev(D_WARN, "couldn't load %s\n", sfx->soundName );
			return 0;
		}
		return sfx - s_knownSfx;
	}
	sfx->inMemory = false;
	sfx->soundCompressed = false;
	S_memoryLoad(sfx);

	if ( sfx->defaultSound )
	{
		MsgDev(D_LOAD, "couldn't load %s\n", sfx->soundName );
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
		if( killall || loopSounds[i].kill == true || (loopSounds[i].sfx && loopSounds[i].sfx->soundLength == 0))
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
		s_paintedtime = s_soundtime;

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

int FS_GetLittleLong( file_t *f )
{
	int		v;

	FS_Read( f, &v, sizeof(v));
	return LittleLong(v);
}

int FS_GetLittleShort( file_t *f )
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
	len = FS_GetLittleLong( f );
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
	s_backgroundInfo.format = FS_GetLittleShort( s_backgroundFile );
	s_backgroundInfo.channels = FS_GetLittleShort( s_backgroundFile );
	s_backgroundInfo.rate = FS_GetLittleLong( s_backgroundFile );
	FS_GetLittleLong(  s_backgroundFile );
	FS_GetLittleShort(  s_backgroundFile );
	s_backgroundInfo.width = FS_GetLittleShort( s_backgroundFile ) / 8;

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

/*
===============================================================================

memory management

===============================================================================
*/
static sndBuffer *buffer = NULL;
static sndBuffer *buflist = NULL;
static int inUse = 0;
static int totalInUse = 0;

short *sfxScratchBuffer = NULL;
sfx_t *sfxScratchPointer = NULL;
int sfxScratchIndex = 0;

void SND_free(sndBuffer *v)
{
	*(sndBuffer **)v = buflist;
	buflist = (sndBuffer*)v;
	inUse += sizeof(sndBuffer);
}

sndBuffer* SND_malloc( void )
{
	sndBuffer *v;
redo:
	if(buflist == NULL)
	{
		S_FreeOldestSound();
		goto redo;
	}
	inUse -= sizeof(sndBuffer);
	totalInUse += sizeof(sndBuffer);

	v = buflist;
	buflist = *(sndBuffer **)buflist;
	v->next = NULL;
	return v;
}

void SND_setup( void )
{
	sndBuffer	*p, *q;
	cvar_t	*cv;
	int	scs;

	cv = Cvar_Get( "s_memory", "4", CVAR_LATCH|CVAR_ARCHIVE );

	scs = (cv->integer * 1536);

	if(!buffer) buffer = Z_Malloc(scs * sizeof(sndBuffer));
	// allocate the stack based hunk allocator
	if(sfxScratchBuffer) sfxScratchBuffer = Z_Malloc(SND_CHUNK_SIZE * sizeof(short) * 4);
	sfxScratchPointer = NULL;

	inUse = scs * sizeof(sndBuffer);
	p = buffer;
	q = p + scs;
	while (--q > p) *(sndBuffer **)q = q-1;
	*(sndBuffer **)q = NULL;
	buflist = p + scs - 1;
}

/*
===============================================================================

WAV loading

===============================================================================
*/

static byte *data_p;
static byte *iff_end;
static byte *last_chunk;
static byte *iff_data;
static int iff_chunk_len;

static short GetLittleShort(void)
{
	short val = 0;
	val = *data_p;
	val = val + (*(data_p+1)<<8);
	data_p += 2;
	return val;
}

static int GetLittleLong(void)
{
	int val = 0;
	val = *data_p;
	val = val + (*(data_p+1)<<8);
	val = val + (*(data_p+2)<<16);
	val = val + (*(data_p+3)<<24);
	data_p += 4;
	return val;
}

static void FindNextChunk(char *name)
{
	while (1)
	{
		data_p = last_chunk;

		if (data_p >= iff_end)
		{	
			// didn't find the chunk
			data_p = NULL;
			return;
		}
		
		data_p += 4;
		iff_chunk_len = GetLittleLong();
		if (iff_chunk_len < 0)
		{
			data_p = NULL;
			return;
		}
		data_p -= 8;
		last_chunk = data_p + 8 + ( (iff_chunk_len + 1) & ~1 );
		if (!strncmp((char *)data_p, name, 4)) return;
	}
}

static void FindChunk(char *name)
{
	last_chunk = iff_data;
	FindNextChunk (name);
}

/*
============
GetWavinfo
============
*/
static wavinfo_t GetWavinfo (char *name, byte *wav, int wavlength)
{
	wavinfo_t	info;

	memset (&info, 0, sizeof(info));

	if (!wav) return info;
		
	iff_data = wav;
	iff_end = wav + wavlength;

	// find "RIFF" chunk
	FindChunk("RIFF");
	if(!(data_p && !strncmp((char *)data_p+8, "WAVE", 4)))
	{
		MsgWarn("GetWavinfo: missing RIFF/WAVE chunks\n");
		return info;
	}

	// get "fmt " chunk
	iff_data = data_p + 12;

	FindChunk("fmt ");
	if (!data_p)
	{
		MsgWarn("GetWavinfo: missing fmt chunk\n");
		return info;
	}
	data_p += 8;
	info.format = GetLittleShort();
	info.channels = GetLittleShort();
	info.rate = GetLittleLong();
	data_p += 4 + 2;
	info.width = GetLittleShort() / 8;

	if (info.format != 1)
	{
		MsgWarn("GetWavinfo: microsoft PCM format only\n");
		return info;
	}


	// find data chunk
	FindChunk("data");
	if (!data_p)
	{
		MsgWarn("GetWavinfo: missing data chunk\n");
		return info;
	}

	data_p += 4;
	info.samples = GetLittleLong () / info.width;
	info.dataofs = data_p - wav;

	return info;
}


/*
================
ResampleSfx

resample / decimate to the current source rate
================
*/
static void ResampleSfx( sfx_t *sfx, int inrate, int inwidth, byte *data, bool compressed )
{
	int		outcount;
	int		srcsample;
	float		stepscale;
	int		sample, samplefrac = 0, fracstep;
	int		i, part;
	sndBuffer		*chunk;

	// this is usually 0.5, 1, or 2	
	stepscale = (float)inrate / dma.speed;

	outcount = sfx->soundLength / stepscale;
	sfx->soundLength = outcount;

	fracstep = stepscale * 256;
	chunk = sfx->soundData;

	for (i = 0; i < outcount; i++)
	{
		srcsample = samplefrac >> 8;
		samplefrac += fracstep;
		if( inwidth == 2 ) sample = LittleShort ( ((short *)data)[srcsample] );
		else sample = (int)( (unsigned char)(data[srcsample]) - 128) << 8;
		part = (i & (SND_CHUNK_SIZE-1));

		if (part == 0)
		{
			sndBuffer	*newchunk;
			newchunk = SND_malloc();
			if (chunk == NULL)
				sfx->soundData = newchunk;
			else chunk->next = newchunk;
			chunk = newchunk;
		}
		chunk->sndChunk[part] = sample;
	}
}

/*
================
ResampleSfx

resample / decimate to the current source rate
================
*/
static int ResampleSfxRaw( short *sfx, int inrate, int inwidth, int samples, byte *data )
{
	int	outcount;
	int	srcsample;
	float	stepscale;
	int	i, sample, samplefrac, fracstep;

	// this is usually 0.5, 1, or 2	
	stepscale = (float)inrate / dma.speed;

	outcount = samples / stepscale;

	samplefrac = 0;
	fracstep = stepscale * 256;

	for (i = 0; i < outcount; i++)
	{
		srcsample = samplefrac>>8;
		samplefrac += fracstep;
		if( inwidth == 2 ) sample = LittleShort ( ((short *)data)[srcsample] );
		else sample = (int)( (unsigned char)(data[srcsample]) - 128)<<8;
		sfx[i] = sample;
	}

	return outcount;
}


//=============================================================================

/*
==============
S_LoadSound

The filename may be different than sfx->name in the case
of a forced fallback of a player specific sound
==============
*/
bool S_LoadSound( sfx_t *sfx )
{
	byte	*data;
	short	*samples;
	wavinfo_t	info;
	int	size;
	char	namebuffer[MAX_QPATH];

	// player specific sounds are never directly loaded
	if ( sfx->soundName[0] == '*') return false;

	// load it in
	if (sfx->soundName[0] == '#') strcpy(namebuffer, &sfx->soundName[1]);
	else sprintf (namebuffer, "sound/%s", sfx->soundName);
	data = FS_LoadFile( namebuffer, &size );
	if ( !data ) return false;

	info = GetWavinfo( sfx->soundName, data, size );
	if ( info.channels != 1 )
	{
		MsgDev(D_INFO, "%s is a stereo wav file\n", sfx->soundName);
		Mem_Free( data );
		return false;
	}

	if ( info.width == 1 ) MsgDev(D_NOTE, "S_LoadSound: %s is a 8 bit wav file\n", sfx->soundName);
	if ( info.rate != 22050 ) MsgDev(D_NOTE, "S_LoadSound: %s is not a 22kHz wav file\n", sfx->soundName);

	samples = Z_Malloc(info.samples * sizeof(short) * 2);
	sfx->lastTimeUsed = Sys_DoubleTime() + 0.001;

	// each of these compression schemes works just fine
	// but the 16bit quality is much nicer and with a local
	// install assured we can rely upon the sound memory
	// manager to do the right thing for us and page
	// sound in as needed

	if( sfx->soundCompressed == true)
	{
		sfx->soundCompressionMethod = 1;
		sfx->soundData = NULL;
		sfx->soundLength = ResampleSfxRaw( samples, info.rate, info.width, info.samples, (data + info.dataofs));
		S_AdpcmEncodeSound(sfx, samples);
	}
	else
	{
		sfx->soundCompressionMethod = 0;
		sfx->soundLength = info.samples;
		sfx->soundData = NULL;
		ResampleSfx( sfx, info.rate, info.width, data + info.dataofs, false );
	}
	
	Mem_Free( samples );
	Mem_Free( data );

	return true;
}

void S_DisplayFreeMemory( void )
{
	Msg("%d bytes free sound buffer memory, %d total used\n", inUse, totalInUse);
}

static portable_samplepair_t paintbuffer[PAINTBUFFER_SIZE];
static int snd_vol;

static int* snd_p;  
static int snd_linear_count;
short* snd_out;

void S_WriteLinearBlastStereo16 (void)
{
	int	i, val;

	for (i = 0; i < snd_linear_count; i += 2)
	{
		val = snd_p[i]>>8;
		if (val > 0x7fff) snd_out[i] = 0x7fff;
		else if (val < -32768) snd_out[i] = -32768;
		else snd_out[i] = val;

		val = snd_p[i+1]>>8;
		if (val > 0x7fff) snd_out[i+1] = 0x7fff;
		else if (val < -32768) snd_out[i+1] = -32768;
		else snd_out[i+1] = val;
	}
}

void S_TransferStereo16 (unsigned long *pbuf, int endtime)
{
	int		lpos;
	int		ls_paintedtime;
	
	snd_p = (int *)paintbuffer;
	ls_paintedtime = s_paintedtime;

	while (ls_paintedtime < endtime)
	{
		// handle recirculating buffer issues
		lpos = ls_paintedtime & ((dma.samples>>1)-1);

		snd_out = (short *) pbuf + (lpos<<1);

		snd_linear_count = (dma.samples>>1) - lpos;
		if (ls_paintedtime + snd_linear_count > endtime)
			snd_linear_count = endtime - ls_paintedtime;

		snd_linear_count <<= 1;

		// write a linear blast of samples
		S_WriteLinearBlastStereo16 ();
		snd_p += snd_linear_count;
		ls_paintedtime += (snd_linear_count>>1);
	}
}

/*
===================
S_TransferPaintBuffer

===================
*/
void S_TransferPaintBuffer(int endtime)
{
	int 	*p, out_idx, count;
	int 	out_mask, step, val;
	dword	*pbuf;

	pbuf = (dword *)dma.buffer;

	if ( s_testsound->value )
	{
		int	i, count;

		// write a fixed sine wave
		count = (endtime - s_paintedtime);
		for (i = 0; i < count; i++)
			paintbuffer[i].left = paintbuffer[i].right = sin((s_paintedtime+i)*0.1) * 20000 * 256;
	}


	if (dma.samplebits == 16 && dma.channels == 2)
	{	
		// optimized case
		S_TransferStereo16 (pbuf, endtime);
	}
	else
	{	// general case
		p = (int *) paintbuffer;
		count = (endtime - s_paintedtime) * dma.channels;
		out_mask = dma.samples - 1; 
		out_idx = s_paintedtime * dma.channels & out_mask;
		step = 3 - dma.channels;

		if (dma.samplebits == 16)
		{
			short *out = (short *) pbuf;
			while (count--)
			{
				val = *p >> 8;
				p+= step;
				if (val > 0x7fff) val = 0x7fff;
				else if (val < -32768) val = -32768;
				out[out_idx] = val;
				out_idx = (out_idx + 1) & out_mask;
			}
		}
		else if (dma.samplebits == 8)
		{
			byte *out = (byte *)pbuf;
			while (count--)
			{
				val = *p >> 8;
				p+= step;
				if (val > 0x7fff) val = 0x7fff;
				else if (val < -32768) val = -32768;
				out[out_idx] = (val>>8) + 128;
				out_idx = (out_idx + 1) & out_mask;
			}
		}
	}
}


/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/
static void S_PaintChannelFrom16( channel_t *ch, const sfx_t *sc, int count, int sampleOffset, int bufferOffset )
{
	int			data, aoff, boff;
	int			leftvol, rightvol;
	int			i, j;
	portable_samplepair_t	*samp;
	sndBuffer			*chunk;
	short			*samples;
	float			ooff, fdata, fdiv, fleftvol, frightvol;

	samp = &paintbuffer[ bufferOffset ];

	if (ch->doppler) sampleOffset = sampleOffset * ch->oldDopplerScale;

	chunk = sc->soundData;
	while (sampleOffset >= SND_CHUNK_SIZE)
	{
		chunk = chunk->next;
		sampleOffset -= SND_CHUNK_SIZE;
		if (!chunk) chunk = sc->soundData;
	}

	if (!ch->doppler || ch->dopplerScale==1.0f)
	{
		leftvol = ch->leftvol*snd_vol;
		rightvol = ch->rightvol*snd_vol;
		samples = chunk->sndChunk;

		for ( i = 0; i < count; i++ )
		{
			data  = samples[sampleOffset++];
			samp[i].left += (data * leftvol)>>8;
			samp[i].right += (data * rightvol)>>8;

			if (sampleOffset == SND_CHUNK_SIZE)
			{
				chunk = chunk->next;
				samples = chunk->sndChunk;
				sampleOffset = 0;
			}
		}
	}
	else
	{
		fleftvol = ch->leftvol * snd_vol;
		frightvol = ch->rightvol * snd_vol;
		ooff = sampleOffset;
		samples = chunk->sndChunk;

		for ( i = 0; i < count; i++ )
		{
			aoff = ooff;
			ooff = ooff + ch->dopplerScale;
			boff = ooff;
			fdata = 0;
			for (j = aoff; j < boff; j++)
			{
				if (j == SND_CHUNK_SIZE)
				{
					chunk = chunk->next;
					if (!chunk) chunk = sc->soundData;
					samples = chunk->sndChunk;
					ooff -= SND_CHUNK_SIZE;
				}
				fdata += samples[j & (SND_CHUNK_SIZE-1)];
			}
			fdiv = 256 * (boff - aoff);
			samp[i].left += (fdata * fleftvol) / fdiv;
			samp[i].right += (fdata * frightvol) / fdiv;
		}
	}
}

void S_PaintChannelFromWavelet( channel_t *ch, sfx_t *sc, int count, int sampleOffset, int bufferOffset )
{
	int			i = 0, data;
	int			leftvol, rightvol;
	portable_samplepair_t	*samp;
	sndBuffer			*chunk;
	short			*samples;

	leftvol = ch->leftvol*snd_vol;
	rightvol = ch->rightvol*snd_vol;

	samp = &paintbuffer[ bufferOffset ];
	chunk = sc->soundData;
	while (sampleOffset>=(SND_CHUNK_SIZE_FLOAT * 4))
	{
		chunk = chunk->next;
		sampleOffset -= (SND_CHUNK_SIZE_FLOAT * 4);
		i++;
	}

	if (i != sfxScratchIndex || sfxScratchPointer != sc)
	{
		S_AdpcmGetSamples( chunk, sfxScratchBuffer );
		sfxScratchIndex = i;
		sfxScratchPointer = sc;
	}

	samples = sfxScratchBuffer;

	for ( i = 0; i < count; i++ )
	{
		data  = samples[sampleOffset++];
		samp[i].left += (data * leftvol)>>8;
		samp[i].right += (data * rightvol)>>8;

		if (sampleOffset == SND_CHUNK_SIZE * 2)
		{
			chunk = chunk->next;
			decodeWavelet(chunk, sfxScratchBuffer);
			sfxScratchIndex++;
			sampleOffset = 0;
		}
	}
}

void S_PaintChannelFromADPCM( channel_t *ch, sfx_t *sc, int count, int sampleOffset, int bufferOffset )
{
	int			i = 0, data;
	int			leftvol, rightvol;
	portable_samplepair_t	*samp;
	sndBuffer			*chunk;
	short			*samples;

	leftvol = ch->leftvol*snd_vol;
	rightvol = ch->rightvol*snd_vol;

	samp = &paintbuffer[ bufferOffset ];
	chunk = sc->soundData;

	if (ch->doppler) sampleOffset = sampleOffset * ch->oldDopplerScale;

	while (sampleOffset>=(SND_CHUNK_SIZE * 4))
	{
		chunk = chunk->next;
		sampleOffset -= (SND_CHUNK_SIZE * 4);
		i++;
	}

	if (i != sfxScratchIndex || sfxScratchPointer != sc)
	{
		S_AdpcmGetSamples( chunk, sfxScratchBuffer );
		sfxScratchIndex = i;
		sfxScratchPointer = sc;
	}

	samples = sfxScratchBuffer;

	for ( i = 0; i < count; i++ )
	{
		data  = samples[sampleOffset++];
		samp[i].left += (data * leftvol)>>8;
		samp[i].right += (data * rightvol)>>8;

		if (sampleOffset == SND_CHUNK_SIZE * 4)
		{
			chunk = chunk->next;
			S_AdpcmGetSamples( chunk, sfxScratchBuffer);
			sampleOffset = 0;
			sfxScratchIndex++;
		}
	}
}

void S_PaintChannelFromMuLaw( channel_t *ch, sfx_t *sc, int count, int sampleOffset, int bufferOffset )
{
	int			data;
	int			leftvol, rightvol;
	int			i;
	portable_samplepair_t	*samp;
	sndBuffer			*chunk;
	byte			*samples;
	float			ooff;

	leftvol = ch->leftvol * snd_vol;
	rightvol = ch->rightvol * snd_vol;

	samp = &paintbuffer[ bufferOffset ];
	chunk = sc->soundData;
	while (sampleOffset>=(SND_CHUNK_SIZE * 2))
	{
		chunk = chunk->next;
		sampleOffset -= (SND_CHUNK_SIZE * 2);
		if (!chunk) chunk = sc->soundData;
	}

	if (!ch->doppler)
	{
		samples = (byte *)chunk->sndChunk + sampleOffset;
		for ( i = 0; i < count; i++ )
		{
			data  = mulawToShort[*samples];
			samp[i].left += (data * leftvol)>>8;
			samp[i].right += (data * rightvol)>>8;
			samples++;
			if (samples == (byte *)chunk->sndChunk + (SND_CHUNK_SIZE * 2))
			{
				chunk = chunk->next;
				samples = (byte *)chunk->sndChunk;
			}
		}
	}
	else
	{
		ooff = sampleOffset;
		samples = (byte *)chunk->sndChunk;
		for ( i = 0; i < count; i++ )
		{
			data  = mulawToShort[samples[(int)(ooff)]];
			ooff = ooff + ch->dopplerScale;
			samp[i].left += (data * leftvol)>>8;
			samp[i].right += (data * rightvol)>>8;
			if (ooff >= SND_CHUNK_SIZE * 2)
			{
				chunk = chunk->next;
				if(!chunk) chunk = sc->soundData;
				samples = (byte *)chunk->sndChunk;
				ooff = 0.0;
			}
		}
	}
}

/*
===================
S_PaintChannels
===================
*/
void S_PaintChannels( int endtime )
{
	int 		i, end;
	channel_t		*ch;
	sfx_t		*sc;
	int		ltime, count;
	int		sampleOffset;


	snd_vol = s_volume->value * 255;

	while ( s_paintedtime < endtime )
	{
		// if paintbuffer is smaller than DMA buffer
		// we may need to fill it multiple times
		end = endtime;
		if ( endtime - s_paintedtime > PAINTBUFFER_SIZE )
			end = s_paintedtime + PAINTBUFFER_SIZE;

		// clear the paint buffer to either music or zeros
		if ( s_rawend < s_paintedtime )
		{
			memset(paintbuffer, 0, (end - s_paintedtime) * sizeof(portable_samplepair_t));
		}
		else
		{
			// copy from the streaming sound source
			int	s, stop;

			stop = (end < s_rawend) ? end : s_rawend;
			for ( i = s_paintedtime; i < stop; i++ )
			{
				s = i&(MAX_RAW_SAMPLES-1);
				paintbuffer[i-s_paintedtime] = s_rawsamples[s];
			}
			for (; i < end; i++ )
			{
				paintbuffer[i - s_paintedtime].left = 0;
				paintbuffer[i - s_paintedtime].right = 0;
			}
		}

		// paint in the channels.
		ch = s_channels;
		for ( i = 0; i < MAX_CHANNELS; i++, ch++ )
		{		
			if ( !ch->thesfx || (ch->leftvol<0.25 && ch->rightvol<0.25 ))
				continue;

			ltime = s_paintedtime;
			sc = ch->thesfx;

			sampleOffset = ltime - ch->startSample;
			count = end - ltime;
			if ( sampleOffset + count > sc->soundLength )
				count = sc->soundLength - sampleOffset;

			if ( count > 0 )
			{	
				if( sc->soundCompressionMethod == 1)
				{
					S_PaintChannelFromADPCM(ch, sc, count, sampleOffset, ltime - s_paintedtime);
				}
				else if( sc->soundCompressionMethod == 2)
				{
					S_PaintChannelFromWavelet(ch, sc, count, sampleOffset, ltime - s_paintedtime);
				}
				else if( sc->soundCompressionMethod == 3)
				{
					S_PaintChannelFromMuLaw(ch, sc, count, sampleOffset, ltime - s_paintedtime);
				}
				else S_PaintChannelFrom16 (ch, sc, count, sampleOffset, ltime - s_paintedtime);
			}
		}

		// paint in the looped channels.
		ch = loop_channels;
		for ( i = 0; i < numLoopChannels; i++, ch++ )
		{		
			if ( !ch->thesfx || (!ch->leftvol && !ch->rightvol ))
				continue;

			ltime = s_paintedtime;
			sc = ch->thesfx;

			if (sc->soundData==NULL || sc->soundLength==0)
				continue;

			// we might have to make two passes if it
			// is a looping sound effect and the end of
			// the sample is hit
			do
			{
				sampleOffset = (ltime % sc->soundLength);
				count = end - ltime;

				if ( sampleOffset + count > sc->soundLength )
					count = sc->soundLength - sampleOffset;

				if ( count > 0 )
				{	
					if( sc->soundCompressionMethod == 1)
					{
						S_PaintChannelFromADPCM(ch, sc, count, sampleOffset, ltime - s_paintedtime);
					}
					else if( sc->soundCompressionMethod == 2)
					{
						S_PaintChannelFromWavelet(ch, sc, count, sampleOffset, ltime - s_paintedtime);
					}
					else if( sc->soundCompressionMethod == 3)
					{
						S_PaintChannelFromMuLaw(ch, sc, count, sampleOffset, ltime - s_paintedtime);
					}
					else S_PaintChannelFrom16(ch, sc, count, sampleOffset, ltime - s_paintedtime);
					ltime += count;
				}
			} while ( ltime < end);
		}
		// transfer out according to DMA format
		S_TransferPaintBuffer( end );
		s_paintedtime = end;
	}
}


// Intel ADPCM step variation table
static int indexTable[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8,};

static int stepsizeTable[89] = 
{
7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

   
void S_AdpcmEncode( short indata[], char outdata[], int len, struct adpcm_state *state )
{
	short		*inp;	// Input buffer pointer
	signed char	*outp;	// output buffer pointer
	int		val;	// Current input sample value
	int		sign;	// Current adpcm sign bit
	int		delta;	// Current adpcm output value
	int		diff;	// Difference between val and sample
	int		step;	// Stepsize
	int		valpred;	// Predicted output value
	int		vpdiff;	// Current change to valpred
	int		index;	// Current step change index
	int		outputbuffer;	// place to keep previous 4-bit value
	int		bufferstep;	// toggle between outputbuffer/output

	outp = (signed char *)outdata;
	inp = indata;

	valpred = state->sample;
	index = state->index;
	step = stepsizeTable[index];
    
	outputbuffer = 0;	// quiet a compiler warning
	bufferstep = 1;

	for(; len > 0; len-- )
	{
		val = *inp++;

		// Step 1 - compute difference with previous value 
		diff = val - valpred;
		sign = (diff < 0) ? 8 : 0;
		if ( sign ) diff = (-diff);

		// Step 2 - Divide and clamp
		// Note:
		// This code *approximately* computes:
		//    delta = diff*4/step;
		//    vpdiff = (delta+0.5)*step/4;
		// but in shift step bits are dropped. The net result of this is
		// that even if you have fast mul/div hardware you cannot put it to
		// good use since the fixup would be too expensive.
		delta = 0;
		vpdiff = (step >> 3);
		
		if ( diff >= step )
		{
			delta = 4;
			diff -= step;
			vpdiff += step;
		}
		step >>= 1;
		if ( diff >= step  )
		{
			delta |= 2;
			diff -= step;
			vpdiff += step;
		}
		step >>= 1;
		if ( diff >= step )
		{
			delta |= 1;
			vpdiff += step;
		}

		// Step 3 - Update previous value
		if ( sign ) valpred -= vpdiff;
		else valpred += vpdiff;

		// Step 4 - Clamp previous value to 16 bits
		if ( valpred > 32767 ) valpred = 32767;
		else if ( valpred < -32768 ) valpred = -32768;

		// Step 5 - Assemble value, update index and step values
		delta |= sign;
		
		index += indexTable[delta];
		if ( index < 0 ) index = 0;
		if ( index > 88 ) index = 88;
		step = stepsizeTable[index];

		// Step 6 - Output value
		if ( bufferstep ) outputbuffer = (delta << 4) & 0xf0;
		else *outp++ = (delta & 0x0f) | outputbuffer;
		bufferstep = !bufferstep;
	}

	// Output last step, if needed
	if( !bufferstep ) *outp++ = outputbuffer;

	state->sample = valpred;
	state->index = index;
}


void S_AdpcmDecode( const char indata[], short *outdata, int len, struct adpcm_state *state )
{
	signed char	*inp;	// Input buffer pointer
	int		outp;	// output buffer pointer
	int		sign;	// Current adpcm sign bit
	int		delta;	// Current adpcm output value
	int		step;	// Stepsize
	int		valpred;	// Predicted value
	int		vpdiff;	// Current change to valpred
	int		index;	// Current step change index
	int		inputbuffer;	// place to keep next 4-bit value
	int		bufferstep;	// toggle between inputbuffer/input

	outp = 0;
	inp = (signed char *)indata;

	valpred = state->sample;
	index = state->index;
	step = stepsizeTable[index];

	bufferstep = 0;
	inputbuffer = 0;	// quiet a compiler warning

	for (; len > 0; len-- )
	{
		// Step 1 - get the delta value
		if ( bufferstep ) delta = inputbuffer & 0xf;
		else
		{
			inputbuffer = *inp++;
			delta = (inputbuffer >> 4) & 0xf;
		}
		bufferstep = !bufferstep;

		// Step 2 - Find new index value (for later)
		index += indexTable[delta];
		if ( index < 0 ) index = 0;
		if ( index > 88 ) index = 88;

		// Step 3 - Separate sign and magnitude
		sign = delta & 8;
		delta = delta & 7;

		// Step 4 - Compute difference and new predicted value
		// Computes 'vpdiff = (delta+0.5)*step/4', but see comment
		// in adpcm_coder.
		vpdiff = step >> 3;
		if ( delta & 4 ) vpdiff += step;
		if ( delta & 2 ) vpdiff += step>>1;
		if ( delta & 1 ) vpdiff += step>>2;

		if ( sign ) valpred -= vpdiff;
		else valpred += vpdiff;

		// Step 5 - clamp output value
		if ( valpred > 32767 ) valpred = 32767;
		else if ( valpred < -32768 ) valpred = -32768;

		// Step 6 - Update step value
		step = stepsizeTable[index];

		// Step 7 - Output value
		outdata[outp] = valpred;
		outp++;
	}

	state->sample = valpred;
	state->index = index;
}


/*
====================
S_AdpcmMemoryNeeded

Returns the amount of memory (in bytes) needed to store the samples in out internal adpcm format
====================
*/
int S_AdpcmMemoryNeeded( const wavinfo_t *info )
{
	float	scale;
	int	scaledSampleCount;
	int	sampleMemory;
	int	blockCount;
	int	headerMemory;

	// determine scale to convert from input sampling rate to desired sampling rate
	scale = (float)info->rate / dma.speed;

	// calc number of samples at playback sampling rate
	scaledSampleCount = info->samples / scale;

	// calc memory need to store those samples using ADPCM at 4 bits per sample
	sampleMemory = scaledSampleCount / 2;

	// calc number of sample blocks needed of PAINTBUFFER_SIZE
	blockCount = scaledSampleCount / PAINTBUFFER_SIZE;
	if( scaledSampleCount % PAINTBUFFER_SIZE ) blockCount++;

	// calc memory needed to store the block headers
	headerMemory = blockCount * sizeof(adpcm_state_t);

	return sampleMemory + headerMemory;
}


/*
====================
S_AdpcmGetSamples
====================
*/
void S_AdpcmGetSamples(sndBuffer *chunk, short *to)
{
	adpcm_state_t	state;
	byte		*out;

	// get the starting state from the block header
	state.index = chunk->adpcm.index;
	state.sample = chunk->adpcm.sample;

	out = (byte *)chunk->sndChunk;
	S_AdpcmDecode( out, to, SND_CHUNK_SIZE_BYTE * 2, &state );
}


/*
====================
S_AdpcmEncodeSound
====================
*/
void S_AdpcmEncodeSound( sfx_t *sfx, short *samples )
{
	adpcm_state_t	state;
	int		inOffset = 0;
	int		n, count;
	sndBuffer		*newchunk, *chunk;
	byte		*out;

	count = sfx->soundLength;
	state.index = 0;
	state.sample = samples[0];

	chunk = NULL;
	while( count )
	{
		n = count;
		if( n > SND_CHUNK_SIZE_BYTE * 2 ) n = SND_CHUNK_SIZE_BYTE * 2;

		newchunk = SND_malloc();
		if (sfx->soundData == NULL) sfx->soundData = newchunk;
		else chunk->next = newchunk;
		chunk = newchunk;

		// output the header
		chunk->adpcm.index  = state.index;
		chunk->adpcm.sample = state.sample;

		out = (byte *)chunk->sndChunk;

		// encode the samples
		S_AdpcmEncode( samples + inOffset, out, n, &state );
		inOffset += n;
		count -= n;
	}
}

#define C0 0.4829629131445341
#define C1 0.8365163037378079
#define C2 0.2241438680420134
#define C3 -0.1294095225512604

void daub4(float b[], dword n, int isign)
{
	float	wksp[4097];
	float	*a=b-1; // numerical recipies so a[1] = b[0]
	dword	nh,nh1,i,j;

	if (n < 4) return;

	nh1 = (nh = n>>1)+1;
	if (isign >= 0)
	{
		for (i = 1, j = 1; j <= n - 3; j += 2, i++)
		{
			wksp[i] = C0*a[j]+C1*a[j+1]+C2*a[j+2]+C3*a[j+3];
			wksp[i+nh] = C3*a[j]-C2*a[j+1]+C1*a[j+2]-C0*a[j+3];
		}
		wksp[i] = C0*a[n-1]+C1*a[n]+C2*a[1]+C3*a[2];
		wksp[i+nh] = C3*a[n-1]-C2*a[n]+C1*a[1]-C0*a[2];
	}
	else
	{
		wksp[1] = C2*a[nh]+C1*a[n]+C0*a[1]+C3*a[nh1];
		wksp[2] = C3*a[nh]-C0*a[n]+C1*a[1]-C2*a[nh1];
		for (i = 1, j = 3; i < nh; i++)
		{
			wksp[j++] = C2*a[i]+C1*a[i+nh]+C0*a[i+1]+C3*a[i+nh1];
			wksp[j++] = C3*a[i]-C0*a[i+nh]+C1*a[i+1]-C2*a[i+nh1];
		}
	}
	for (i = 1; i <= n; i++) a[i] = wksp[i];
}

void wt1(float a[], dword n, int isign)
{
	dword	nn;
	int	inverseStartLength = n/4;
	if (n < inverseStartLength) return;
	if (isign >= 0) for (nn = n; nn >= inverseStartLength; nn >>= 1) daub4(a, nn, isign);
	else for (nn = inverseStartLength; nn <= n; nn <<= 1) daub4(a, nn, isign);
}

// The number of bits required by each value
static byte numBits[] =
{
0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
};

byte MuLawEncode(short s)
{
	dword	adjusted;
	byte	sign, exponent, mantissa;

	sign = (s < 0) ? 0:0x80;

	if (s < 0) s = -s;
	adjusted = (long)s<<(16-sizeof(short)*8);
	adjusted += 128L + 4L;
	if (adjusted > 32767) adjusted = 32767;
	exponent = numBits[(adjusted>>7)&0xff] - 1;
	mantissa = (adjusted>>(exponent+3))&0xf;
	return ~(sign | (exponent<<4) | mantissa);
}

short MuLawDecode(byte uLaw)
{
	signed long adjusted;
	byte exponent, mantissa;

	uLaw = ~uLaw;
	exponent = (uLaw>>4) & 0x7;
	mantissa = (uLaw&0xf) + 16;
	adjusted = (mantissa << (exponent +3)) - 128 - 4;

	return (uLaw & 0x80)? adjusted : -adjusted;
}

short mulawToShort[256];
static bool madeTable = false;
static int NXStreamCount;

void NXPutc(NXStream *stream, char out)
{
	stream[NXStreamCount++] = out;
}

void encodeWavelet( sfx_t *sfx, short *packets)
{
	float	wksp[4097], temp;
	int	i, samples, size;
	sndBuffer	*newchunk, *chunk;
	byte	*out;

	if (!madeTable)
	{
		for (i = 0; i < 256; i++) mulawToShort[i] = (float)MuLawDecode((byte)i);
		madeTable = true;
	}
	chunk = NULL;

	samples = sfx->soundLength;
	while(samples > 0)
	{
		size = samples;
		if(size > (SND_CHUNK_SIZE*2)) size = (SND_CHUNK_SIZE*2);
		if(size < 4) size = 4;

		newchunk = SND_malloc();
		if (sfx->soundData == NULL) sfx->soundData = newchunk;
		else chunk->next = newchunk;
		chunk = newchunk;
		for(i = 0; i < size; i++)
		{
			wksp[i] = *packets;
			packets++;
		}
		wt1(wksp, size, 1);
		out = (byte *)chunk->sndChunk;

		for(i = 0; i < size; i++)
		{
			temp = wksp[i];
			if (temp > 32767) temp = 32767; else if (temp<-32768) temp = -32768;
			out[i] = MuLawEncode((short)temp);
		}
		chunk->size = size;
		samples -= size;
	}
}

void decodeWavelet(sndBuffer *chunk, short *to)
{
	float	wksp[4097];
	int	i, size = chunk->size;
	byte	*out;
	
	out = (byte *)chunk->sndChunk;
	for(i = 0; i < size; i++) wksp[i] = mulawToShort[out[i]];

	wt1(wksp, size, -1);
	if(!to) return;

	for(i = 0; i < size; i++) to[i] = wksp[i];
}

void encodeMuLaw( sfx_t *sfx, short *packets)
{
	int	i, samples, size, grade, poop;
	sndBuffer	*newchunk, *chunk;
	byte	*out;

	if (!madeTable)
	{
		for (i = 0; i < 256; i++)
		{
			mulawToShort[i] = (float)MuLawDecode((byte)i);
		}
		madeTable = true;
	}

	chunk = NULL;
	samples = sfx->soundLength;
	grade = 0;

	while(samples > 0)
	{
		size = samples;
		if (size > (SND_CHUNK_SIZE * 2)) size = (SND_CHUNK_SIZE * 2);

		newchunk = SND_malloc();
		if (sfx->soundData == NULL) sfx->soundData = newchunk;
		else chunk->next = newchunk;
		chunk = newchunk;
		out = (byte *)chunk->sndChunk;
		for(i = 0; i < size; i++)
		{
			poop = packets[0]+grade;
			if (poop > 32767) poop = 32767;
			else if (poop<-32768) poop = -32768;
			out[i] = MuLawEncode((short)poop);
			grade = poop - mulawToShort[out[i]];
			packets++;
		}
		chunk->size = size;
		samples -= size;
	}
}

void decodeMuLaw(sndBuffer *chunk, short *to)
{
	byte	*out;
	int	i, size = chunk->size;
	
	out = (byte *)chunk->sndChunk;
	for(i = 0; i < size; i++) to[i] = mulawToShort[out[i]];
}