/*
** Copyright (C) 2003 Eric Lasota/Orbiter Productions
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation; either version 2.1 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/


// RoQLib video decoder library
#ifndef __ROQLIB_H__
#define __ROQLIB_H__

// ***************************************************************
//

#include "platform.h"

#ifndef ROQ_LIB_BUILD
#  define ROQ_INTERNAL_STRUCT long
#endif

#define ROQ_AUDIO_INITIALIZED 1
#define ROQ_VIDEO_INITIALIZED 2

struct roq_decompressor_s
{
	unsigned short width;
	unsigned short height;

	unsigned char *rgb;

	short *audioSamples;
	unsigned long availableAudioSamples;

	int flags;

	ROQ_INTERNAL_STRUCT local;
};

typedef struct roq_decompressor_s roq_decompressor_t;

// Frame decode results
#define ROQ_MARKER_INFO         0x1001
#define ROQ_MARKER_CODEBOOK     0x1002
#define ROQ_MARKER_VIDEO        0x1011
#define ROQ_MARKER_AUDIO_MONO   0x1020
#define ROQ_MARKER_AUDIO_STEREO 0x1021
#define ROQ_MARKER_EOF          0xffff

// Function prototypes
struct roq_exports_s
{
	roq_decompressor_t *(*CreateDecompressor)(const void *fileReader);
	void (*DestroyDecompressor)(roq_decompressor_t *);

	int (*DecompressFrame)(roq_decompressor_t *);
};

struct roq_imports_s
{
	int (*ReadData)(const void *fileReader, void *buffer, unsigned long bytes);
};

typedef struct roq_exports_s roq_exports_t;
typedef struct roq_imports_s roq_imports_t;


#ifdef __cplusplus
extern "C"
#endif

ROQEXPORT roq_exports_t *RoQDec_APIExchange(roq_imports_t *);

#endif
