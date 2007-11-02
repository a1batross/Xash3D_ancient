//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         snd_mem.c - sound memory manager
//=======================================================================

#include "client.h"
#include "snd_loc.h"

#define DEF_COMSOUNDMEGS	"8"
/*
===============================================================================

memory management

===============================================================================
*/
static sndBuffer *buffer = NULL;
static sndBuffer *freelist = NULL;
static int inUse = 0;
static int totalInUse = 0;

short *sfxScratchBuffer = NULL;
sfx_t *sfxScratchPointer = NULL;
int sfxScratchIndex = 0;

void SND_free(sndBuffer *v)
{
	*(sndBuffer **)v = freelist;
	freelist = (sndBuffer*)v;
	inUse += sizeof(sndBuffer);
}

sndBuffer* SND_malloc( void )
{
	sndBuffer *v;
redo:
	if(freelist == NULL)
	{
		S_FreeOldestSound();
		goto redo;
	}
	inUse -= sizeof(sndBuffer);
	totalInUse += sizeof(sndBuffer);

	v = freelist;
	freelist = *(sndBuffer **)freelist;
	v->next = NULL;
	return v;
}

void SND_setup( void )
{
	sndBuffer	*p, *q;
	cvar_t	*cv;
	int	scs;

	cv = Cvar_Get( "com_soundMegs", DEF_COMSOUNDMEGS, CVAR_LATCH | CVAR_ARCHIVE );

	scs = (cv->value * 1536);

	if(!buffer) buffer = Z_Malloc(scs * sizeof(sndBuffer));
	// allocate the stack based hunk allocator
	if(sfxScratchBuffer) sfxScratchBuffer = Z_Malloc(SND_CHUNK_SIZE * sizeof(short) * 4);
	sfxScratchPointer = NULL;

	inUse = scs*sizeof(sndBuffer);
	p = buffer;
	q = p + scs;
	while (--q > p) *(sndBuffer **)q = q-1;
	*(sndBuffer **)q = NULL;
	freelist = p + scs - 1;

	Msg("Sound memory manager started\n");
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
		Z_Free( data );
		return false;
	}

	if ( info.width == 1 ) MsgDev(D_INFO, "S_LoadSound: %s is a 8 bit wav file\n", sfx->soundName);
	if ( info.rate != 22050 ) MsgDev(D_INFO, "S_LoadSound: %s is not a 22kHz wav file\n", sfx->soundName);

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
	
	Z_Free( samples );
	Z_Free( data );

	return true;
}

void S_DisplayFreeMemory( void )
{
	Msg("%d bytes free sound buffer memory, %d total used\n", inUse, totalInUse);
}
