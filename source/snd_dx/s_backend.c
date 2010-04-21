//=======================================================================
//			Copyright XashXT Group 2009 ©
//		        s_direct.c - sound hardware output
//=======================================================================

#include <dsound.h>
#include "sound.h"

#define	iDirectSoundCreate( a, b, c )	pDirectSoundCreate( a, b, c )

static HRESULT ( _stdcall *pDirectSoundCreate)(GUID* lpGUID, LPDIRECTSOUND* lplpDS, IUnknown* pUnkOuter );

static dllfunc_t dsound_funcs[] =
{
{ "DirectSoundCreate", (void **) &pDirectSoundCreate },
{ NULL, NULL }
};

dll_info_t dsound_dll = { "dsound.dll", dsound_funcs, NULL, NULL, NULL, false, 0, 0 };

// 64K is > 1 second at 16-bit, 22050 Hz
#define WAV_BUFFERS			64
#define WAV_MASK			0x3F
#define WAV_BUFFER_SIZE		0x0400
#define SECONDARY_BUFFER_SIZE		0x10000

typedef enum
{
	SIS_SUCCESS,
	SIS_FAILURE,
	SIS_NOTAVAIL
} si_state_t;

cvar_t		*s_wavonly;

static HWND	snd_hwnd;
static bool	dsound_init;
static bool	wav_init;
static bool	snd_firsttime = true;
static bool	snd_isdirect, snd_iswave;
static bool	primary_format_set;
static int	snd_buffer_count = 0;
static int	snd_sent, snd_completed;
static int	sample16;

/* 
=======================================================================
Global variables. Must be visible to window-procedure function 
so it can unlock and free the data block after it has been played.
=======================================================================
*/ 
DWORD		locksize;
HANDLE		hData;
HPSTR		lpData, lpData2;
HGLOBAL		hWaveHdr;
LPWAVEHDR		lpWaveHdr;
HWAVEOUT		hWaveOut; 
WAVEOUTCAPS	wavecaps;
DWORD		gSndBufSize;
MMTIME		mmstarttime;
LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;
LPDIRECTSOUND	pDS;

bool SNDDMA_InitDirect( void *hInst );
bool SNDDMA_InitWav( void );
void SNDDMA_FreeSound( void );

static const char *DSoundError( int error )
{
	switch( error )
	{
	case DSERR_BUFFERLOST:
		return "DSERR_BUFFERLOST";
	case DSERR_INVALIDCALL:
		return "DSERR_INVALIDCALLS";
	case DSERR_INVALIDPARAM:
		return "DSERR_INVALIDPARAM";
	case DSERR_PRIOLEVELNEEDED:
		return "DSERR_PRIOLEVELNEEDED";
	}
	return "Unknown Error";
}

/*
==================
DS_CreateBuffers
==================
*/
static bool DS_CreateBuffers( void *hInst )
{
	DSBUFFERDESC	dsbuf;
	DSBCAPS		dsbcaps;
	WAVEFORMATEX	pformat, format;

	Mem_Set( &format, 0, sizeof( format ));

	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = dma.channels;
	format.wBitsPerSample = dma.samplebits;
	format.nSamplesPerSec = dma.speed;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign; 
	format.cbSize = 0;

	MsgDev( D_NOTE, "DS_CreateBuffers: initialize\n" );

	MsgDev( D_NOTE, "DS_CreateBuffers: setting EXCLUSIVE coop level " );
	if( DS_OK != pDS->lpVtbl->SetCooperativeLevel( pDS, hInst, DSSCL_EXCLUSIVE ))
	{
		MsgDev( D_NOTE, "- failed\n" );
		SNDDMA_FreeSound();
		return false;
	}
	MsgDev( D_NOTE, "- ok\n" );

	// get access to the primary buffer, if possible, so we can set the sound hardware format
	Mem_Set( &dsbuf, 0, sizeof( dsbuf ));
	dsbuf.dwSize = sizeof( DSBUFFERDESC );
	dsbuf.dwFlags = DSBCAPS_PRIMARYBUFFER;
	dsbuf.dwBufferBytes = 0;
	dsbuf.lpwfxFormat = NULL;

	Mem_Set( &dsbcaps, 0, sizeof( dsbcaps ));
	dsbcaps.dwSize = sizeof( dsbcaps );
	primary_format_set = false;

	MsgDev( D_NOTE, "DS_CreateBuffers: creating primary buffer " );
	if( pDS->lpVtbl->CreateSoundBuffer( pDS, &dsbuf, &pDSPBuf, NULL ) == DS_OK )
	{
		pformat = format;

		MsgDev( D_NOTE, "- ok\n" );
		if( snd_firsttime )
			MsgDev( D_NOTE, "DS_CreateBuffers: setting primary sound format " );

		if( pDSPBuf->lpVtbl->SetFormat( pDSPBuf, &pformat ) != DS_OK )
		{
			if( snd_firsttime )
				MsgDev( D_NOTE, "- failed\n" );
		}
		else
		{
			if( snd_firsttime )
				MsgDev( D_NOTE, "- ok\n" );
			primary_format_set = true;
		}
	}
	else MsgDev( D_NOTE, "- failed\n" );

	if( !primary_format_set || !s_primary->integer )
	{
		// create the secondary buffer we'll actually work with
		Mem_Set( &dsbuf, 0, sizeof( dsbuf ));
		dsbuf.dwSize = sizeof( DSBUFFERDESC );
		dsbuf.dwFlags = (DSBCAPS_CTRLFREQUENCY|DSBCAPS_LOCSOFTWARE);
		dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
		dsbuf.lpwfxFormat = &format;

		Mem_Set( &dsbcaps, 0, sizeof( dsbcaps ));
		dsbcaps.dwSize = sizeof( dsbcaps );

		MsgDev( D_NOTE, "DS_CreateBuffers: creating secondary buffer " );
		if( pDS->lpVtbl->CreateSoundBuffer( pDS, &dsbuf, &pDSBuf, NULL ) == DS_OK )
		{
			MsgDev( D_NOTE, "- ok\n" );
		}
		else
		{
			// couldn't get hardware, fallback to software.
			dsbuf.dwFlags = (DSBCAPS_LOCSOFTWARE|DSBCAPS_GETCURRENTPOSITION2);
			if( pDS->lpVtbl->CreateSoundBuffer( pDS, &dsbuf, &pDSBuf, NULL ) != DS_OK )
			{
				MsgDev( D_NOTE, "- failed\n" );
				SNDDMA_FreeSound ();
				return false;
			}
			MsgDev( D_INFO, "- failed. forced to software\n" );
		}

		dma.channels = format.nChannels;
		dma.samplebits = format.wBitsPerSample;
		dma.speed = format.nSamplesPerSec;

		if( pDSBuf->lpVtbl->GetCaps( pDSBuf, &dsbcaps ) != DS_OK )
		{
			MsgDev( D_ERROR, "DS_CreateBuffers: GetCaps failed\n");
			SNDDMA_FreeSound ();
			return false;
		}
		MsgDev( D_NOTE, "DS_CreateBuffers: using secondary sound buffer\n" );
	}
	else
	{
		MsgDev( D_NOTE, "DS_CreateBuffers: using primary sound buffer\n" );
		MsgDev( D_NOTE, "DS_CreateBuffers: setting WRITEPRIMARY coop level " );
		if( pDS->lpVtbl->SetCooperativeLevel( pDS, hInst, DSSCL_WRITEPRIMARY ) != DS_OK )
		{
			MsgDev( D_NOTE, "- failed\n" );
			SNDDMA_FreeSound ();
			return false;
		}
		MsgDev( D_NOTE, "- ok\n" );

		if( pDSPBuf->lpVtbl->GetCaps( pDSPBuf, &dsbcaps ) != DS_OK )
		{
			MsgDev( D_ERROR, "DS_CreateBuffers: GetCaps failed\n");
			SNDDMA_FreeSound ();
			return false;
		}
		pDSBuf = pDSPBuf;
	}

	// make sure mixer is active
	if( pDSBuf->lpVtbl->Play( pDSBuf, 0, 0, DSBPLAY_LOOPING ) != DS_OK )
	{
		MsgDev( D_ERROR, "DS_CreateBuffers: looped sound play failed\n" );
		SNDDMA_FreeSound ();
		return false;
	}

	// we don't want anyone to access the buffer directly w/o locking it first
	lpData = NULL;
	dma.samplepos = 0;
	snd_hwnd = (HWND)hInst;
	gSndBufSize = dsbcaps.dwBufferBytes;
	dma.samples = gSndBufSize / (dma.samplebits / 8 );
	dma.submission_chunk = 1;
	dma.buffer = (byte *)lpData;
	sample16 = (dma.samplebits / 8) - 1;

	SNDDMA_BeginPainting();
	if( dma.buffer ) Mem_Set( dma.buffer, 0, dma.samples * dma.samplebits / 8 );
	SNDDMA_Submit();

	return true;
}

/*
==================
DS_DestroyBuffers
==================
*/
static void DS_DestroyBuffers( void )
{
	MsgDev( D_NOTE, "DS_DestroyBuffers: shutdown\n" );

	if( pDS )
	{
		MsgDev( D_NOTE, "DS_DestroyBuffers: setting NORMAL coop level\n" );
		pDS->lpVtbl->SetCooperativeLevel( pDS, snd_hwnd, DSSCL_NORMAL );
	}

	if( pDSBuf )
	{
		MsgDev( D_NOTE, "DS_DestroyBuffers: stopping and releasing sound buffer\n" );
		pDSBuf->lpVtbl->Stop( pDSBuf );
		pDSBuf->lpVtbl->Release( pDSBuf );
	}

	// only release primary buffer if it's not also the mixing buffer we just released
	if( pDSPBuf && ( pDSBuf != pDSPBuf ))
	{
		MsgDev( D_NOTE, "DS_DestroyBuffers: releasing primary buffer\n" );
		pDSPBuf->lpVtbl->Release( pDSPBuf );
	}

	pDSBuf = NULL;
	pDSPBuf = NULL;
	dma.buffer = NULL;
}

/*
==================
SNDDMA_FreeSound
==================
*/
void SNDDMA_FreeSound( void )
{
	int	i;

	if( pDS )
	{
		DS_DestroyBuffers();
		pDS->lpVtbl->Release( pDS );
		Sys_FreeLibrary( &dsound_dll );
	}

	if( hWaveOut )
	{
		waveOutReset( hWaveOut );

		if( lpWaveHdr )
		{
			for( i = 0; i < WAV_BUFFERS; i++ )
				waveOutUnprepareHeader( hWaveOut, lpWaveHdr + i, sizeof( WAVEHDR ));
		}

		waveOutClose( hWaveOut );

		if( hWaveHdr )
		{
			GlobalUnlock( hWaveHdr );
			GlobalFree( hWaveHdr );
		}

		if( hData )
		{
			GlobalUnlock( hData );
			GlobalFree( hData );
		}

	}

	pDS = NULL;
	pDSBuf = NULL;
	pDSPBuf = NULL;
	hWaveOut = 0;
	hData = 0;
	hWaveHdr = 0;
	lpData = NULL;
	lpWaveHdr = NULL;
	dsound_init = false;
	wav_init = false;
}

/*
==================
SNDDMA_InitDirect

Direct-Sound support
==================
*/
si_state_t SNDDMA_InitDirect( void *hInst )
{
	DSCAPS	dscaps;
	HRESULT	hresult;

	dma.channels = 2;
	dma.samplebits = 16;

	switch( s_khz->integer )
	{
	case 44: dma.speed = 44100; break;
	case 22: dma.speed = 22050; break;
	default: dma.speed = 11025; break;
	}

	MsgDev( D_NOTE, "SNDDMA_InitDirect: initializing DirectSound ");

	if( !dsound_dll.link )
	{
		if( !Sys_LoadLibrary( NULL, &dsound_dll ))
		{
			MsgDev( D_NOTE, "- failed\n" );
			return SIS_FAILURE;
		}
		MsgDev( D_NOTE, "- ok\n" );
	}

	MsgDev( D_NOTE, "SNDDMA_InitDirect: creating DS object " );
	if(( hresult = iDirectSoundCreate( NULL, &pDS, NULL )) != DS_OK )
	{
		if( hresult != DSERR_ALLOCATED )
		{
			MsgDev( D_NOTE, "- failed\n" );
			return SIS_FAILURE;
		}

		MsgDev( D_NOTE, "- failed, hardware already in use\n" );
		return SIS_NOTAVAIL;
	}

	MsgDev( D_NOTE, "- ok\n" );
	dscaps.dwSize = sizeof( dscaps );

	if( pDS->lpVtbl->GetCaps( pDS, &dscaps ) != DS_OK )
		MsgDev( D_ERROR, "SNDDMA_InitDirect: GetCaps failed\n");

	if( dscaps.dwFlags & DSCAPS_EMULDRIVER )
	{
		MsgDev( D_ERROR, "SNDDMA_InitDirect: no DSound driver found\n" );
		SNDDMA_FreeSound();
		return SIS_FAILURE;
	}

	if( !DS_CreateBuffers( hInst ))
		return SIS_FAILURE;

	dsound_init = true;

	return SIS_SUCCESS;
}


/*
==================
SNDDM_InitWav

Crappy windows multimedia base
==================
*/
si_state_t SNDDMA_InitWav( void )
{
	WAVEFORMATEX	format; 
	HRESULT		hr;
	int		i;

	snd_sent = 0;
	snd_completed = 0;

	dma.channels = 2;
	dma.samplebits = 16;

	switch( s_khz->integer )
	{
	case 44: dma.speed = 44100; break;
	case 22: dma.speed = 22050; break;
	default: dma.speed = 11025; break;
	}

	Mem_Set( &format, 0, sizeof( format ));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = dma.channels;
	format.wBitsPerSample = dma.samplebits;
	format.nSamplesPerSec = dma.speed;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign; 
	format.cbSize = 0;	

	// open a waveform device for output using window callback. 
	MsgDev( D_NOTE, "SNDDMA_InitWav: initializing wave sound " );
	if(( hr = waveOutOpen((LPHWAVEOUT)&hWaveOut, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL)) != MMSYSERR_NOERROR )
	{
		if( hr != MMSYSERR_ALLOCATED )
		{
			MsgDev( D_NOTE, "- failed\n" );
			return SIS_FAILURE;
		}

		MsgDev( D_NOTE, "- failed, hardware already in use\n" );
		return SIS_NOTAVAIL;
	} 

	MsgDev( D_NOTE, "- ok\n" );

	// allocate and lock memory for the waveform data. The memory 
	// for waveform data must be globally allocated with 
	// GMEM_MOVEABLE and GMEM_SHARE flags. 

	gSndBufSize = WAV_BUFFERS * WAV_BUFFER_SIZE;
	hData = GlobalAlloc( GMEM_MOVEABLE|GMEM_SHARE, gSndBufSize ); 
	if( !hData ) 
	{ 
		SNDDMA_FreeSound();
		return SIS_FAILURE; 
	}

	lpData = GlobalLock( hData );
	if( !lpData )
	{ 
		SNDDMA_FreeSound ();
		return SIS_FAILURE; 
	} 

	Mem_Set( lpData, 0, gSndBufSize );

	// Allocate and lock memory for the header. This memory must 
	// also be globally allocated with GMEM_MOVEABLE and 
	// GMEM_SHARE flags. 
	hWaveHdr = GlobalAlloc( GMEM_MOVEABLE|GMEM_SHARE, (DWORD)sizeof( WAVEHDR ) * WAV_BUFFERS ); 

	if( hWaveHdr == NULL )
	{ 
		SNDDMA_FreeSound ();
		return SIS_FAILURE; 
	} 

	lpWaveHdr = (LPWAVEHDR)GlobalLock( hWaveHdr ); 

	if( lpWaveHdr == NULL )
	{ 
		SNDDMA_FreeSound();
		return SIS_FAILURE; 
	}

	Mem_Set( lpWaveHdr, 0, sizeof( WAVEHDR ) * WAV_BUFFERS );

	// After allocation, set up and prepare headers.
	for( i = 0; i < WAV_BUFFERS; i++ )
	{
		lpWaveHdr[i].dwBufferLength = WAV_BUFFER_SIZE; 
		lpWaveHdr[i].lpData = lpData + i * WAV_BUFFER_SIZE;

		if( waveOutPrepareHeader( hWaveOut, lpWaveHdr+i, sizeof( WAVEHDR )) != MMSYSERR_NOERROR )
		{
			SNDDMA_FreeSound();
			return SIS_FAILURE;
		}
	}

	dma.samplepos = 0;
	dma.samples = gSndBufSize / ( dma.samplebits / 8 );
	dma.submission_chunk = 512;
	dma.buffer = (byte *)lpData;
	sample16 = (dma.samplebits / 8) - 1;
	wav_init = true;

	return SIS_SUCCESS;
}

/*
==================
SNDDMA_Init

Try to find a sound device to mix for.
Returns false if nothing is found.
==================
*/
int SNDDMA_Init( void *hInst )
{
	si_state_t	stat = SIS_FAILURE;	// assume DirectSound won't initialize

	Mem_Set( &dma, 0, sizeof( dma ));

	s_wavonly = Cvar_Get( "s_wavonly", "0", CVAR_LATCH_AUDIO|CVAR_ARCHIVE, "force to use WaveOutput only" );
	dsound_init = wav_init = 0;

	// init DirectSound
	if( !s_wavonly->integer )
	{
		if( snd_firsttime || snd_isdirect )
		{
			stat = SNDDMA_InitDirect( hInst );

			if( stat == SIS_SUCCESS )
			{
				snd_isdirect = true;

				if( snd_firsttime )
					MsgDev( D_INFO, "Audio: DirectSound\n" );
			}
			else snd_isdirect = false;
		}
	}

	// if DirectSound didn't succeed in initializing, try to initialize
	// waveOut sound, unless DirectSound failed because the hardware is
	// already allocated (in which case the user has already chosen not
	// to have sound)
	if( !dsound_init && ( stat != SIS_NOTAVAIL ))
	{
		if( snd_firsttime || snd_iswave )
		{
			stat = SNDDMA_InitWav();

			if( stat == SIS_SUCCESS )
			{
				snd_iswave = true;
				if( snd_firsttime )
					MsgDev( D_INFO, "Audio: WaveOutput\n" );
			}
			else snd_iswave = false;
		}
	}

	snd_buffer_count = 1;

	if( !dsound_init && !wav_init )
	{
		if( snd_firsttime )
			MsgDev( D_ERROR, "SNDDMA_Init: can't initialize sound device\n" );
		return false;
	}

	snd_firsttime = false;
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
	int	s;

	if( dsound_init ) 
	{
		MMTIME	mmtime;
		DWORD	dwWrite;
	
		mmtime.wType = TIME_SAMPLES;
		pDSBuf->lpVtbl->GetCurrentPosition( pDSBuf, &mmtime.u.sample, &dwWrite );
		s = mmtime.u.sample - mmstarttime.u.sample;
	}
	else if( wav_init )
	{        
		s = snd_sent * WAV_BUFFER_SIZE;
	}


	s >>= sample16;
	s &= (dma.samples - 1);

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
	DWORD	dwSize2;
	DWORD	*pbuf, *pbuf2;
	HRESULT	hr;
	DWORD	dwStatus;

	if( !pDSBuf ) return;

	// if the buffer was lost or stopped, restore it and/or restart it
	if( pDSBuf->lpVtbl->GetStatus( pDSBuf, &dwStatus ) != DS_OK )
		MsgDev( D_WARN, "SNDDMA_BeginPainting: couldn't get sound buffer status\n" );
	
	if( dwStatus & DSBSTATUS_BUFFERLOST )
		pDSBuf->lpVtbl->Restore( pDSBuf );
	
	if(!( dwStatus & DSBSTATUS_PLAYING ))
		pDSBuf->lpVtbl->Play( pDSBuf, 0, 0, DSBPLAY_LOOPING );

	// lock the dsound buffer
	dma.buffer = NULL;
	reps = 0;

	while(( hr = pDSBuf->lpVtbl->Lock( pDSBuf, 0, gSndBufSize, &pbuf, &locksize, &pbuf2, &dwSize2, 0 )) != DS_OK )
	{
		if( hr != DSERR_BUFFERLOST )
		{
			MsgDev( D_ERROR, "SNDDMA_BeginPainting: lock error '%s'\n", DSoundError( hr ));
			S_Shutdown ();
			return;
		}
		else pDSBuf->lpVtbl->Restore( pDSBuf );

		if( ++reps > 2 ) return;
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
	LPWAVEHDR	h;
	int wResult;

	if( !dma.buffer )
		return;

	// unlock the dsound buffer
	if( pDSBuf ) pDSBuf->lpVtbl->Unlock( pDSBuf, dma.buffer, locksize, NULL, 0 );

	if( !wav_init ) return;

	// find which sound blocks have completed
	while( 1 )
	{
		if( snd_completed == snd_sent )
			break;

		if(!( lpWaveHdr[snd_completed & WAV_MASK].dwFlags & WHDR_DONE ))
			break;
		snd_completed++; // this buffer has been played
	}

	// submit a few new sound blocks
	while((( snd_sent - snd_completed ) >> sample16 ) < 8 )
	{
		h = lpWaveHdr + ( snd_sent & WAV_MASK );

		if( paintedtime / 256 <= snd_sent )
			break;
		snd_sent++;

		// Now the data block can be sent to the output device. The 
		// waveOutWrite function returns immediately and waveform 
		// data is sent to the output device in the background. 
		wResult = waveOutWrite( hWaveOut, h, sizeof( WAVEHDR )); 

		if( wResult != MMSYSERR_NOERROR )
		{ 
			MsgDev( D_ERROR, "SNDDMA_Submit: failed to write block to device\n" );
			SNDDMA_FreeSound ();
			return; 
		} 
	}
}

/*
==============
SNDDMA_Shutdown

Reset the sound device for exiting
===============
*/
void SNDDMA_Shutdown( void )
{
	SNDDMA_FreeSound();
}


/*
===========
S_Activate

Called when the main window gains or loses focus.
The window have been destroyed and recreated
between a deactivate and an activate.
===========
*/
void S_Activate( bool active )
{
	if( active )
	{
		if( pDS && snd_hwnd && snd_isdirect )
			DS_CreateBuffers( snd_hwnd );
	}
	else
	{
		if( pDS && snd_hwnd && snd_isdirect )
			DS_DestroyBuffers();
		else if( snd_iswave )
			waveOutReset( hWaveOut );
	}
}