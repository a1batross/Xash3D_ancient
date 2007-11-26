//=======================================================================
//			Copyright XashXT Group 2007 ©
//			 roq_main.c - ROQ video maker
//=======================================================================

#include "roqlib.h"

uint keyBits = 0;
uint motionBits = 0;
uint keyPerc = 600;
uint motionPerc = 200;
uint keyRate = 120;
file_t *roqFile = NULL;
rgbdata_t	*frame = NULL;

void ROQ_Blit(byte *source, byte *dest, dword scanWidth, dword sourceSkip, dword destSkip, dword rows)
{
	while(rows)
	{
		Mem_Copy(dest, source, scanWidth);
		source += sourceSkip;
		dest += destSkip;
		rows--;
	}
}

// Doubles the size of one RGB source into the destination
void ROQ_DoubleSize(byte *source, byte *dest, dword dim)
{
	dword	x,y;
	dword	skip;

	skip = dim * 6;

	for(y = 0; y < dim; y++)
	{
		for(x = 0; x < dim; x++)
		{
			Mem_Copy(dest, source, 3);
			Mem_Copy(dest+3, source, 3);
			Mem_Copy(dest+skip, source, 3);
			Mem_Copy(dest+skip+3, source, 3);
			dest += 6;
			source += 3;
		}
		dest += skip;
	}
}

static roq_compressor_t *ROQ_CreateCompressor(void)
{
	roq_compressor_t *comp;

	comp = RQalloc(sizeof(roq_compressor_t));
	if(!comp) return NULL;

	comp->framesSinceKeyframe = 0;
	comp->preferences[ROQENC_PREF_KEYFRAME] = 1;
	comp->preferences[ROQENC_PREF_WIDTH] = 0;
	comp->preferences[ROQENC_PREF_HEIGHT] = 0;
	comp->preferences[ROQENC_PREF_REFINEMENT_PASSES] = 1;
	comp->preferences[ROQENC_PREF_CULL_THRESHOLD] = 64;
	comp->initialized = 0;
	return comp;
}

void ROQ_DestroyCompressor(roq_compressor_t *comp)
{
	int i;

	RFree(comp->devLUT2);
	RFree(comp->devLUT4);
	RFree(comp->devLUT8);
	RFree(comp->devMotion4);
	RFree(comp->devMotion8);
	RFree(comp->devSkip4);
	for(i = 0; i < 3; i++)
		RFree(comp->frames[i].rgb);
	RFree(comp->listSort);
	RFree(comp->lut2);
	RFree(comp->lut4);
	RFree(comp->lut8);
	RFree(comp->motionVectors4);
	RFree(comp->motionVectors8);
	RFree(comp->pList);
	RFree(comp->preWrite);
	RFree(comp->recon);
	RFree(comp->optimizedOut4);
	RFree(comp);
}

static void ROQ_SetPreference(roq_compressor_t *comp, short num, dword pref)
{
	if(num >= ROQENC_PREF_COUNT) return;
	comp->preferences[num] = pref;
}

static void ROQ_WriteHeader(void *file)
{
	byte roqheader[8] = {0x84, 0x10, 0xff, 0xff, 0xff, 0xff, 0x1e, 0x00};
	FS_Write(file, roqheader, 8);
}

bool PrepareROQVideo ( const char *dir, const char *name, byte params )
{
	roqpool = Mem_AllocPool( "ROQ Encoder" );

	// loading qc-script
	// initialize frame chains
	// loading first frame

	return false;
}

bool MakeROQ ( void )
{
	roq_compressor_t	*comp;
	uint		nextKeyframe = 0;

	if(!frame) Sys_Break("frame not loaded\n");

	if(!keyBits) keyBits = (frame->width * frame->height * (3*8)) * keyPerc / 10000;
	if(!motionBits) motionBits = (frame->width * frame->height * (3*8)) * motionPerc / 10000;

	// write the RoQ header
	comp = ROQ_CreateCompressor();
	if(!comp) Sys_Error("MAkeROQ: couldn't create compressor\n");

	ROQ_SetPreference(comp, ROQENC_PREF_WIDTH, frame->width );
	ROQ_SetPreference(comp, ROQENC_PREF_HEIGHT, frame->height );
	ROQ_WriteHeader(roqFile);

	while( frame && frame->buffer )
	{
		// load a frame here

		if(!nextKeyframe)
		{
			ROQ_SetPreference(comp, ROQENC_PREF_KEYFRAME, 1);
			ROQ_SetPreference(comp, ROQENC_PREF_GOAL_SIZE_BITS, keyBits);
			nextKeyframe = keyRate;
		}
		else ROQ_SetPreference(comp, ROQENC_PREF_GOAL_SIZE_BITS, motionBits);
		nextKeyframe--;

		if(!ROQ_CompressRGB(comp, roqFile, frame->buffer ))
		{
			Sys_Error("MakeROQ: frame encode failed\n");
			return false;
		}
	}

	ROQ_DestroyCompressor(comp);
	Mem_FreePool( &roqpool );
	return true;
}