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

#ifndef __ROQ_ENCODER_H__
#define __ROQ_ENCODER_H__

enum
{
	ROQENC_PREF_KEYFRAME,
	ROQENC_PREF_WIDTH,
	ROQENC_PREF_HEIGHT,
	ROQENC_PREF_GOAL_SIZE_BITS,
	ROQENC_PREF_REFINEMENT_PASSES,

	ROQENC_PREF_CULL_THRESHOLD,

	ROQENC_PREF_COUNT,
};


typedef struct
{
	unsigned char codebookResults4[256*4*4*3];
	unsigned char codebookResults2[256*2*2*3];

	unsigned int vectorsProvided4;
	unsigned int vectorsQuantized4;

	int bytesConsumed;
	int bytesMax;

	unsigned char *reconstructResult;
} roq_stats_t;


typedef struct
{
	struct roq_compressor_s *(*CreateCompressor) (void);
	void (*DestroyCompressor) (struct roq_compressor_s *);
	void (*SetPreference) (struct roq_compressor_s *, short, unsigned long);

	unsigned long (*CompressRGB) (struct roq_compressor_s *compressor, void *file, unsigned char *rgb);

	void (*WriteHeader) (void *);

	char *licenseText;

	roq_stats_t *stats;
} roqenc_exports_t;

typedef struct
{
	void (*SaveCodebook) (unsigned char *data);
	int (*ReadCodebook) (unsigned char *data);

	void (*WriteBuffer) (void *, const void *, unsigned long);
} roqenc_imports_t;

#include "platform.h"

#ifdef __cplusplus
extern "C"
#endif

ROQEXPORT roqenc_exports_t *RoQEnc_APIExchange(roqenc_imports_t *imp);

#endif
