//=======================================================================
//			Copyright XashXT Group 2007 ©
//		       conv_sound.c - convert wav-files
//=======================================================================

#include "ripper.h"
#include <sys/stat.h>
#include <stdio.h>

#define MUSIDHEADER		((0x1A<<24)+('S'<<16)+('U'<<8)+'M')	// little-endian "MUS "
#define MIDIDHEADER		"MThd\000\000\000\006\000\001"
#define TRACKMAGIC1		"\000\377\003\035"
#define TRACKMAGIC2		"\000\377\057\000"
#define TRACKMAGIC3		"\000\377\002\026"
#define TRACKMAGIC4		"\000\377\131\002\000\000"
#define TRACKMAGIC5		"\000\377\121\003\011\243\032"
#define TRACKMAGIC6		"\000\377\057\000"

#define MIDBUFFER		0x20000
#define last(e)		((byte)(e & 0x80))
#define event_type(e)	((byte)((e & 0x7F)>>4))
#define channel(e)		((byte)(e & 0x0F))

typedef struct
{
	int	ident;
	word	ScoreLength;
	word	ScoreStart;
	word	channels;		// count of primary channels
	word	SecChannels;	// count of secondary channels
	word	InstrCnt;
	word	dummy;
	word	*instruments;
} mus_t;

struct track_s
{
	dword	 current;
	char	vel;
	long	DeltaTime;
	byte	LastEvent;
	char	*data;		// primary data
};

#define mid_write1 VFS_Write
size_t mid_write2( vfile_t *file, const uint *ptr, size_t size )
{
	uint	i, rev = 0;

	for( i = 0; (size_t)i < size; i++ )
		rev = (rev << 8) + (((*ptr) >>(i*8)) & 0xFF) ;

	return VFS_Write( file, &rev, size );
}

void Conv_WriteMIDheader( vfile_t *file, uint ntrks, uint division )
{
	mid_write1( file, MIDIDHEADER, 10 );
	mid_write2( file, &ntrks, 2 );
	mid_write2( file, &division, 2 );
}

void Conv_WriteTrack( vfile_t *file, int tracknum, struct track_s track[] )
{
	uint	size;
	size_t	quot, rem;

	// do we risk overflow here ?
	size = (uint)track[tracknum].current + 4;
	mid_write1( file, "MTrk", 4 );
	if( !tracknum ) size += 33;

	mid_write2( file, &size, 4 );
	if( !tracknum) mid_write1( file, TRACKMAGIC1 "written by Xash MusLib Ripper", 33 );
	quot = (size_t)(track[tracknum].current / 4096);
	rem = (size_t)(track[tracknum].current - quot * 4096);
	mid_write1( file, track[tracknum].data, 4096 * quot );
	mid_write1( file, ((const byte *)track[tracknum].data) + 4096 * quot, rem );
	mid_write1( file, TRACKMAGIC2, 4 );
}

void Conv_WriteFirstTrack( vfile_t *file )
{
	uint size = 43;

	mid_write1( file, "MTrk", 4);
	mid_write2( file, &size, 4 );
	mid_write1( file, TRACKMAGIC3, 4 );
	mid_write1( file, "by XashXT Group 2007 ©", 22 );
	mid_write1( file, TRACKMAGIC4, 6 );
	mid_write1( file, TRACKMAGIC5, 7 );
	mid_write1( file, TRACKMAGIC6, 4 );
}

bool Conv_ReadMusHeader( vfile_t *f, mus_t *hdr )
{
	bool result = true;

	VFS_Read( f, &hdr->ident, 4 );
	if( hdr->ident != MUSIDHEADER )
		return false;

	VFS_Read(f, &(hdr->ScoreLength), sizeof(word));
	VFS_Read(f, &(hdr->ScoreStart), sizeof(word));
	VFS_Read(f, &(hdr->channels), sizeof(word));
	VFS_Read(f, &(hdr->SecChannels), sizeof(word));
	VFS_Read(f, &(hdr->InstrCnt), sizeof(word));
	VFS_Read(f, &(hdr->dummy), sizeof(word));
	hdr->instruments = (word *)Mem_Alloc( zonepool, hdr->InstrCnt * sizeof(word));

	if(VFS_Read( f, hdr->instruments, hdr->InstrCnt * sizeof(word)) != hdr->InstrCnt * sizeof(word))
		result = false;
	Mem_Free( hdr->instruments );
	return result;
}

char Conv_GetChannel( signed char MUS2MIDchannel[] )
{
	signed char old15 = MUS2MIDchannel[15], max = -1;
	int	i;

	MUS2MIDchannel[15] = -1;

	for( i = 0; i < 16; i++ )
	{
		if( MUS2MIDchannel[i] > max )
			max = MUS2MIDchannel[i];
	}
	MUS2MIDchannel[15] = old15;
	return (max == 8 ? 10 : max + 1);
}

void Conv_FreeTracks( struct track_s track[] )
{
	int i ;

	for( i = 0; i < 16; i++ )
	{
		if(track[i].data) Mem_Free( track[i].data ) ;
	}
}

uint Conv_ReadTime( vfile_t *file )
{
	register uint	time = 0;
	int		newbyte;

	do
	{
		VFS_Read( file, &newbyte, 1 );
		if( newbyte != EOF ) time = (time << 7) + (newbyte & 0x7F);
	} while((newbyte != EOF) && (newbyte & 0x80));
	return time ;
}

void Conv_WriteByte( char MIDItrack, char byte, struct track_s track[] )
{
	uint	pos;

	pos = track[MIDItrack].current;
	if( pos < MIDBUFFER )
	{
		// need to reallocte ?
		track[MIDItrack].data[pos] = byte;
	}
	else
	{
		Conv_FreeTracks( track );
		Sys_Break("Not enough memory" );
	}
	track[MIDItrack].current++;
}

void Conv_WriteVarLen( int tracknum, register uint value, struct track_s track[] )
{
	register uint buffer;

	buffer = value & 0x7f;
	while((value >>= 7))
	{
		buffer<<= 8;
		buffer |= 0x80;
		buffer += (value & 0x7f);
	}
	while( 1 )
	{
		Conv_WriteByte( tracknum, buffer, track );
		if( buffer & 0x80 ) buffer >>= 8;
		else break;
	}
}

bool Conv_Mus2Mid( const char *musicname, byte *buffer, int bufsize )
{
	struct track_s	track[16];
	word		TrackCnt = 0;
	word		division = 90;
	byte		et, MUSchannel, MIDIchannel, MIDItrack, NewEvent;
	uint		i, DeltaTime, TotalTime = 0, n = 0;
	char		event, data, ouch = 0;
	signed char	MUS2MIDchannel[16];
	vfile_t		*file_mid, *file_mus = VFS_Create( buffer, bufsize );
	file_t		*f;
	static mus_t	MUSh;
	byte		MUS2MIDcontrol[15] =
	{
		0,                          // program change - not a MIDI control change
		0x00,                       // bank select
		0x01,                       // modulation pot
		0x07,                       // volume
		0x0A,                       // pan pot
		0x0B,                       // expression pot
		0x5B,                       // reverb depth
		0x5D,                       // chorus depth
		0x40,                       // sustain pedal
		0x43,                       // soft pedal
		0x78,                       // all sounds off
		0x7B,                       // all notes off
		0x7E,                       // mono
		0x7F,                       // poly
		0x79                        // reset all controllers
	}, MIDIchan2track[16];

	if(!Conv_ReadMusHeader( file_mus, &MUSh ))
	{
		VFS_Close( file_mus );
		MsgDev(D_ERROR, "Conv_Mus2Mid: can't read mus header\n" );
		return false;
	}

	if( VFS_Seek( file_mus, MUSh.ScoreStart, SEEK_SET ))
	{
		VFS_Close( file_mus );
		MsgDev(D_ERROR,"Conv_Mus2Mid: can't seek scores\n" );
		return false;
	}

	if( MUSh.channels > 15 )
	{
		VFS_Close( file_mus );
		MsgDev(D_ERROR,"Conv_Mus2Mid: too many channels\n" );
		return false;
	}

	for( i = 0; i < 16; i++ )
	{
		MUS2MIDchannel[i] = -1;
		track[i].current = 0;
		track[i].vel = 64;
		track[i].DeltaTime = 0;
		track[i].LastEvent = 0;
		track[i].data = NULL;
	}

	VFS_Read( file_mus, &event, 1 );
	et = event_type( event );
	MUSchannel = channel( event );

	while((et != 6) && !VFS_Eof( file_mus ) && (event != EOF))
	{
		if( MUS2MIDchannel[MUSchannel] == -1 )
		{
			MIDIchannel = MUS2MIDchannel[MUSchannel] = (MUSchannel == 15 ? 9:Conv_GetChannel(MUS2MIDchannel));
			MIDItrack = MIDIchan2track[MIDIchannel] = (byte)(TrackCnt++);
			track[MIDItrack].data = (char *)Mem_Alloc( zonepool, MIDBUFFER );
		}
		else
		{
			MIDIchannel = MUS2MIDchannel[MUSchannel];
			MIDItrack = MIDIchan2track [MIDIchannel];
		}
		Conv_WriteVarLen( MIDItrack, track[MIDItrack].DeltaTime, track );
		track[MIDItrack].DeltaTime = 0;

		switch( et )
		{
		case 0:	// release note
			NewEvent = 0x90 | MIDIchannel;
			if( NewEvent != track[MIDItrack].LastEvent )
			{
				Conv_WriteByte( MIDItrack, NewEvent, track );
				track[MIDItrack].LastEvent = NewEvent;
			}
			else n++;
			VFS_Read( file_mus, &data, 1 );
			Conv_WriteByte( MIDItrack, data, track );
			Conv_WriteByte( MIDItrack, 0, track );
			break;
		case 1:
			NewEvent = 0x90 | MIDIchannel;
			if( NewEvent != track[MIDItrack].LastEvent )
			{
				Conv_WriteByte( MIDItrack, NewEvent, track );
				track[MIDItrack].LastEvent = NewEvent;
			}
			else n++;
			VFS_Read( file_mus, &data, 1 );
			Conv_WriteByte( MIDItrack, data & 0x7F, track );
			if( data & 0x80 ) VFS_Read( file_mus, &track[MIDItrack].vel, 1 );
			Conv_WriteByte( MIDItrack, track[MIDItrack].vel, track );
			break;
		case 2:
			NewEvent = 0xE0 | MIDIchannel;
			if( NewEvent != track[MIDItrack].LastEvent )
			{
				Conv_WriteByte( MIDItrack, NewEvent, track );
				track[MIDItrack].LastEvent = NewEvent;
			}
			else n++;
			VFS_Read( file_mus, &data, 1 );
			Conv_WriteByte( MIDItrack, (data & 1) << 6, track );
			Conv_WriteByte( MIDItrack, data >> 1, track );
			break;
		case 3:
			NewEvent = 0xB0 | MIDIchannel;
			if( NewEvent != track[MIDItrack].LastEvent )
			{
				Conv_WriteByte( MIDItrack, NewEvent, track );
				track[MIDItrack].LastEvent = NewEvent;
			}
			else n++;
			VFS_Read( file_mus, &data, 1 );
			Conv_WriteByte( MIDItrack, MUS2MIDcontrol[data], track );
			if( data == 12 ) Conv_WriteByte( MIDItrack, MUSh.channels + 1, track );
			else Conv_WriteByte( MIDItrack, 0, track );
			break;
		case 4:
			VFS_Read( file_mus, &data, 1 );
			if( data )
			{
				NewEvent = 0xB0 | MIDIchannel;
				if( NewEvent != track[MIDItrack].LastEvent )
				{
					Conv_WriteByte( MIDItrack, NewEvent, track );
					track[MIDItrack].LastEvent = NewEvent;
				}
				else n++;
				Conv_WriteByte( MIDItrack, MUS2MIDcontrol[data], track );
			}
			else
			{
				NewEvent = 0xC0 | MIDIchannel;
				if( NewEvent != track[MIDItrack].LastEvent )
				{
					Conv_WriteByte( MIDItrack, NewEvent, track );
					track[MIDItrack].LastEvent = NewEvent;
				}
				else n++;
			}
			VFS_Read( file_mus, &data, 1 );
			Conv_WriteByte( MIDItrack, data, track );
			break;
		case 5:
		case 7:
			Conv_FreeTracks( track );
			MsgDev(D_ERROR, "Conv_Mus2Mid: bad event\n" );
			return false;
        		default:
        			break;
		}

		if(last( event ))
		{
			DeltaTime = Conv_ReadTime( file_mus );
			TotalTime += DeltaTime;
			for( i = 0; i < (int)TrackCnt; i++ )
				track[i].DeltaTime += DeltaTime;
		}
		VFS_Read( file_mus, &event, 1 );
		if( event != EOF )
		{
			et = event_type( event );
			MUSchannel = channel( event );
		}
		else ouch = 1;
	}
	if( ouch ) MsgDev(D_WARN, "Conv_Mus2Mid: %s.mus - end of file probably corrupted\n", musicname );

	f = FS_Open(va("%s/music/%s.mid", gs_gamedir, musicname ), "wb" );
	file_mid = VFS_Open( f, "w" ); 

	Conv_WriteMIDheader( file_mid, TrackCnt + 1, division );
	Conv_WriteFirstTrack( file_mid );

	for( i = 0; i < (int)TrackCnt; i++ )
		Conv_WriteTrack( file_mid, i, track );
	Conv_FreeTracks( track );
	FS_Close(VFS_Close( file_mid ));
	VFS_Close( file_mus );

	return true;
}

/*
============
ConvMID
============
*/
bool ConvMID( const char *name, char *buffer, int filesize )
{
	string	musicname;

	if(FS_FileExists(va("%s/music/%s.mid", gs_gamedir, musicname )))
		return true; // already existed

	FS_FileBase( name, musicname );
	if(Conv_Mus2Mid( musicname, buffer, filesize ))
	{
		Msg("%s.mid\n", musicname ); // echo to console about current music
		return true;
	}
	return false;
}

/*
============
ConvSND
============
*/
bool ConvSND( const char *name, char *buffer, int filesize )
{
	string	soundname;

	FS_FileBase( name, soundname );

	/*
	FIXME: write sound convertor
	1. Load doom sound as 8-bit unsigned 11 025 kHz uncompressed wav file (mono)
	2. Resample it to 16-bit signed 22 050 kHz (mono)
	3. Write wav header, write chunks
	FS_WriteFile(va("%s/sound/%s.wav", gs_gamedir, soundname ), buffer, filesize );
	Msg("%s.wav\n", soundname ); // echo to console about current sound
	*/

	return false;
}