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

// RoQ Video Encoder API
#include "roqe_local.h"
#include "platform.h"

static roqenc_exports_t roqe;
roqenc_imports_t roqi;

static char licenseText[] = \
"RoQLib is (c)2003 Eric Lasota/Orbiter Productions\n" \
"\n" \
"This library is distributed in the hope that it will be useful,\n" \
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n" \
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n" \
"Library General Public License for more details.\n" \
"\n" \
"You should have received a copy of the GNU Library General Public\n" \
"License along with this library; if not, write to\n" \
"\n" \
"The Free Foundation, Inc.\n" \
"59 Temple Place, Suite 330\n" \
"Boston, MA  02111-1307  USA\n" \
;

static roq_compressor_t *RoQEnc_CreateCompressor(void)
{
	roq_compressor_t *comp;

	comp = malloc(sizeof(roq_compressor_t));
	if(!comp)
		return NULL;

	comp->framesSinceKeyframe = 0;

	comp->preferences[ROQENC_PREF_KEYFRAME] = 1;
	comp->preferences[ROQENC_PREF_WIDTH] = 0;
	comp->preferences[ROQENC_PREF_HEIGHT] = 0;

	comp->preferences[ROQENC_PREF_REFINEMENT_PASSES] = 1;

	comp->preferences[ROQENC_PREF_CULL_THRESHOLD] = 64;

	comp->initialized = 0;

	return comp;
}

#define FREE(x) if(x) free(x)
void RoQEnc_DestroyCompressor(roq_compressor_t *comp)
{
	int i;

	FREE(comp->devLUT2);
	FREE(comp->devLUT4);
	FREE(comp->devLUT8);
	FREE(comp->devMotion4);
	FREE(comp->devMotion8);
	FREE(comp->devSkip4);

	for(i=0;i<3;i++)
	{
		FREE(comp->frames[i].rgb);
	}

	FREE(comp->listSort);
	FREE(comp->lut2);
	FREE(comp->lut4);
	FREE(comp->lut8);
	FREE(comp->motionVectors4);
	FREE(comp->motionVectors8);
	FREE(comp->pList);
	FREE(comp->preWrite);
	FREE(comp->recon);

	FREE(comp->optimizedOut4);

	FREE(comp);
}

static void RoQEnc_SetPreference(roq_compressor_t *comp, short num, ulong pref)
{
	if(num >= ROQENC_PREF_COUNT)
		return;
	comp->preferences[num] = pref;
}


static void RoQEnc_WriteHeader(void *file)
{
	static uchar roqheader[8] = {0x84, 0x10, 0xff, 0xff, 0xff, 0xff, 0x1e, 0x00};

	roqi.WriteBuffer(file, roqheader, 8);
}

ROQEXPORT roqenc_exports_t *RoQEnc_APIExchange(roqenc_imports_t *imp)
{
	roqi = *imp;

	roqe.CreateCompressor = RoQEnc_CreateCompressor;
	roqe.DestroyCompressor = RoQEnc_DestroyCompressor;
	roqe.CompressRGB = RoQEnc_CompressRGB;
	roqe.SetPreference = RoQEnc_SetPreference;
	roqe.WriteHeader = RoQEnc_WriteHeader;

	roqe.licenseText = licenseText;

	roqe.stats = &stats;

	return &roqe;
}
