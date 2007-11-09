/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include <windows.h>
#include <dsound.h>
#include "client.h"
#include "snd_loc.h"

static HRESULT (_stdcall *pDirectSoundCreate)(GUID* lpGUID, LPDIRECTSOUND* lplpDS, IUnknown* pUnkOuter);

static dllfunc_t dsound_funcs[] =
{
	{"DirectSoundCreate", (void **) &pDirectSoundCreate },
	{ NULL, NULL }
};
dll_info_t dsound_dll = { "dsound.dll", dsound_funcs, NULL, NULL, NULL, false, 0 };

#define SECONDARY_BUFFER_SIZE	0x10000
extern HWND cl_hwnd;
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
			pDS->lpVtbl->SetCooperativeLevel( pDS, cl_hwnd, DSSCL_PRIORITY );
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
	if(DS_OK != pDS->lpVtbl->SetCooperativeLevel( pDS, cl_hwnd, DSSCL_PRIORITY ))
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

	if( DS_OK != pDS->lpVtbl->SetCooperativeLevel( pDS, cl_hwnd, DSSCL_PRIORITY ))
	{
		MsgDev(D_INFO, "sound SetCooperativeLevel failed\n");
		SNDDMA_Shutdown();
	}
}


