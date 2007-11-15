/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#ifndef COMMON_H
#define COMMON_H

// common.h -- definitions common between client and server, but not game.dll


#define	MAX_STRING_CHARS		1024		// max length of a string passed to Cmd_TokenizeString
#define	MAX_STRING_TOKENS		80		// max tokens resulting from Cmd_TokenizeString
#define	MAX_TOKEN_CHARS		128		// max length of an individual token

// game print flags
#define	PRINT_LOW			0		// pickup messages
#define	PRINT_MEDIUM		1		// death messages
#define	PRINT_HIGH		2		// critical messages
#define	PRINT_CHAT		3		// chat messages

#define	SND_VOLUME		(1<<0)		// a byte
#define	SND_ATTENUATION		(1<<1)		// a byte
#define	SND_POS			(1<<2)		// three coordinates
#define	SND_ENT			(1<<3)		// a short 0-2: channel, 3-12: entity
#define	SND_OFFSET		(1<<4)		// a byte, msec offset from frame start

// sound channels
// channel 0 never willingly overrides
// other channels (1-7) allways override a playing sound on that channel
#define	CHAN_AUTO           0
#define	CHAN_WEAPON         1
#define	CHAN_VOICE          2
#define	CHAN_ITEM           3
#define	CHAN_BODY		4
#define	CHAN_ANNOUNCER	5
// flags
#define	CHAN_NO_PHS_ADD	8	// send to all clients, not just ones in PHS (ATTN 0 will also do this)
#define	CHAN_RELIABLE	16	// send by reliable message, not datagram

#define DEFAULT_SOUND_PACKET_VOLUME		1.0
#define DEFAULT_SOUND_PACKET_ATTENUATION	1.0

/*
==============================================================

MISC

==============================================================
*/
uint Com_BlockChecksum (void *buffer, int length);
float frand(void);	// 0 to 1
float crand(void);	// -1 to 1

#endif//COMMON_H