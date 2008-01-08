//=======================================================================
//			Copyright XashXT Group 2007 ©
//			 roq_main.c - ROQ video maker
//=======================================================================

#include "roqlib.h"
#include "mathlib.h"

uint keyBits = 0;
uint motionBits = 0;
uint keyPerc = 600;
uint motionPerc = 200;
uint keyRate = 120;
uint frameRate = 30;
bool neverCache = false;
bool forceRegen = false;
file_t *roqFile = NULL;
file_t *muxFile = NULL;
file_t *cbFile = NULL;
rgbdata_t	*frame = NULL;
uint nextKeyframe = 0;
uint iTotalFrames = 0;
dword addedData = 0;
char movieoutname[MAX_SYSPATH];
char tempoutname[MAX_SYSPATH];
char codebookname[MAX_SYSPATH];
char soundname[MAX_SYSPATH];
search_t *frames;
byte *buffer;

#define SAMPLES_PER_FRAME	735	// 22050/30
#define MAX_SAMPLES		(SAMPLES_PER_FRAME * 8)
void FPUTC( char buf, file_t *f)
{
	addedData++;
	FS_Write(f, &buf, 1);
}

#define MAX_DPCM		16129 // 127*127
byte dpcmValues[MAX_DPCM];
short channel1[MAX_SAMPLES*2];
short channel2[MAX_SAMPLES*2];
short temp[MAX_SAMPLES*4];
short previousValues[2];
byte stepIndex[2];

void CopyBytes(dword len)
{
	byte	bytes[1024];

	while(len > 1024)
	{
		FS_Read(roqFile, bytes, 1024 );
		FS_Write(muxFile, bytes, 1024);
		len -= 1024;
	}
	FS_Read(roqFile, bytes, len );
	FS_Write(muxFile, bytes, len);
}

short CopyChunk(void)
{
	word	id;
	dword	size;
	byte	buffer[8];

	// Read the chunk header
	if(!FS_Read(roqFile, buffer, 8 ))
		return 0;

	// Parse info
	id = BuffLittleShort( buffer );
	size = BuffLittleLong( &buffer[2] );

	// don't copy audio chunks
	if(id == 0x1020 || id == 0x1021)
	{
		FS_Seek(roqFile, size, SEEK_CUR);
		return id;
	}
	
	// copy the chunk header back out
	FS_Write(muxFile, buffer, 8 );

	// copy the rest
	CopyBytes(size);

	return id;
}

void dpcmInit(void)
{
	word	i;

	short	diff;
	short	diff2;
	word	baseline;
	word	projection;
	word	projectionIndex;

	projectionIndex = 0;
	baseline = 0;
	projection = 1;

	// Create a table of quick DPCM values
	for(i = 0; i < MAX_DPCM; i++)
	{
		// check the difference of the last projection
		// and (possibly) the next projection
		diff = i - baseline;
		diff2 = i - projection;

		if(diff < 0) diff = -diff;
		if(diff2 < 0) diff2 = -diff2;

		// move the DPCM index up a notch if it's closer
		if(diff2 < diff)
		{
			projectionIndex++;
			baseline = projection;
			projection = (projectionIndex+1)*(projectionIndex+1);
		}
		dpcmValues[i] = (byte)projectionIndex;
	}
	previousValues[0] = 0;
	previousValues[1] = 0;
}

short EncodeDPCM( int previous, int current)
{
	byte	signMask;
	int	output;
	int	diff;

	diff = current - previous;

	if(diff < 0)
	{
		signMask = 128;
		diff = -diff;
	}
	else signMask = 0;

	if(diff >= MAX_DPCM)
		output = 127;
	else output = dpcmValues[diff];

	diff = output*output;
	if(signMask) diff = -diff;

	if(previous + diff < -32768 || previous + diff > 32767)
		output--;	// Tone down to prevent overflow

	diff = output*output;
	if(signMask) diff = -diff;

	FPUTC(output | signMask, muxFile);
	
	return previous + diff;
}

void EncodeStereo(unsigned long samples)
{
	dword	doubleSamples = samples * 2;
	dword	i;

	FPUTC(0x21, muxFile); // stereo audio
	FPUTC(0x10, muxFile);

	FPUTC(doubleSamples & 255, muxFile);
	FPUTC((doubleSamples>>8) & 255, muxFile);
	FPUTC((doubleSamples>>16) & 255, muxFile);
	FPUTC((doubleSamples>>24) & 255, muxFile);

	previousValues[0] &= 0xFF00;
	previousValues[1] &= 0xFF00;

	FPUTC((previousValues[1] >> 8), muxFile);
	FPUTC((previousValues[0] >> 8), muxFile);

	// Encode samples
	for(i = 0; i < samples; i++)
	{
		previousValues[0] = EncodeDPCM(previousValues[0], channel1[i]);
		previousValues[1] = EncodeDPCM(previousValues[1], channel2[i]);
	}
}

void EncodeMono(dword samples)
{
	dword	i;

	FPUTC(0x20, muxFile); // mono audio
	FPUTC(0x10, muxFile);

	FPUTC(samples & 255, muxFile);
	FPUTC((samples>>8) & 255, muxFile);
	FPUTC((samples>>16) & 255, muxFile);
	FPUTC((samples>>24) & 255, muxFile);

	FPUTC(previousValues[0] & 255, muxFile);
	FPUTC((previousValues[0] >> 8) & 255, muxFile);

	// encode samples
	for(i = 0; i < samples; i++)
		previousValues[0] = EncodeDPCM(previousValues[0], channel1[i]);
}

byte *ROQ_MakeUprightRGB( const char *name, rgbdata_t *frame )
{
	byte	*in, *out;
	int	bpp;

	switch(frame->type)
	{
	case PF_RGB_24:
	case PF_RGB_24_FLIP:
	case PF_RGBA_32:
		bpp = PFDesc[frame->type].bpp;
		break;
	default:
		Sys_Break("Frame %s have unsupported image type %s\n", name, PFDesc[frame->type].name );
		break;
	}

	// flip image at 180 degrees
	for (in = frame->buffer, out = buffer; in < frame->buffer + frame->width * frame->height * bpp; in += bpp)
	{
		*out++ = in[0];
		*out++ = in[1];
		*out++ = in[2];
	}
	return buffer;
}

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

int ROQ_ReadCodebook( byte *buffer )
{
	if(forceRegen) return 0;
	return FS_Read(cbFile, buffer, CB_FRAME_SIZE );
}

void ROQ_SaveCodebook( byte *buffer )
{
	if(neverCache) return;
	FS_Write(cbFile, buffer, CB_FRAME_SIZE );
}

static roq_compressor_t *ROQ_CreateCompressor(void)
{
	comp = RQalloc(sizeof(roq_compressor_t));
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
	long i;

	Mem_Free(comp->devLUT2);
	Mem_Free(comp->devLUT4);
	Mem_Free(comp->devLUT8);
	Mem_Free(comp->devMotion4);
	Mem_Free(comp->devMotion8);
	Mem_Free(comp->devSkip4);
	for(i = 0; i < 3; i++)
		Mem_Free(comp->frames[i].rgb);
	Mem_Free(comp->listSort);
	Mem_Free(comp->lut2);
	Mem_Free(comp->lut4);
	Mem_Free(comp->lut8);
	Mem_Free(comp->motionVectors4);
	Mem_Free(comp->motionVectors8);
	Mem_Free(comp->pList);
	Mem_Free(comp->preWrite);
	Mem_Free(comp->recon);
	Mem_Free(comp->optimizedOut4);
	Mem_Free(comp);
}

static void ROQ_SetPreference(roq_compressor_t *comp, short num, dword pref)
{
	if(num >= ROQENC_PREF_COUNT) return;
	comp->preferences[num] = pref;
}

static void ROQ_WriteHeader(file_t *file)
{
	roq_t hdr;

	hdr.ident = IDQMOVIEHEADER;
	hdr.flags = 0xffff;
	hdr.flags2 = 0xffff;
	hdr.fps = frameRate;

	FS_Write(file, &hdr, sizeof(roq_t));
}

// Doubles the size of one RGB source longo the destination
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

/*
===============
Cmd_Sound

syntax: "$sound soundname"
===============
*/
void Cmd_Sound( void )
{
	com.strcpy( soundname, Com_GetToken (false));
	FS_DefaultExtension( soundname, ".wav" );
}

/*
===============
Cmd_FpsMovie

syntax: "$framerate value"
===============
*/
void Cmd_FpsMovie( void )
{
	int fps = com.atoi(Com_GetToken(false));

	frameRate = bound(12, fps, 36 );
}

/*
==============
Cmd_Videoname

syntax: "$videoname outname"
==============
*/
void Cmd_Videoname (void)
{
	com.strcpy( movieoutname, Com_GetToken (false));
	FS_DefaultExtension( movieoutname, ".roq" );
}

/*
==============
Cmd_FrameMask

syntax: "$framemask "shot*.tga""
==============
*/
void Cmd_FrameMask( void )
{
	frames = FS_Search(Com_GetToken(false), true );
}

/*
==============
Cmd_Quality

syntax: "$quality "keyword""
==============
*/
void Cmd_Quality( void )
{
	Com_GetToken(false);

	if(Com_MatchToken("high"))
	{
		motionPerc = 550;
	}
	else if(Com_MatchToken("normal"))
	{
		motionPerc = 350;
	}
	else if(Com_MatchToken("low"))
	{
		motionPerc = 200;
	}
	else motionPerc = 350;
}

/*
==============
Cmd_MovieUnknown

syntax: "blabla"
==============
*/
void Cmd_MovieUnknown( void )
{
	MsgWarn("Cmd_MovieUnknown: bad command %s\n", com_token);
	while(Com_TryToken());
}

void ResetMovieInfo( void )
{
	// set default sprite parms
	FS_FileBase(gs_filename, movieoutname ); // kill path and ext
	FS_DefaultExtension( movieoutname, ".roq" );//set new ext

	motionBits = 0;
	keyPerc = 600;
	motionPerc = 200;
	keyRate = 120;
	frameRate = 30;
	keyBits = nextKeyframe = 0;
	neverCache = forceRegen = 0;
	iTotalFrames = 0;
	addedData = 0;

	if(roqFile) FS_Close(roqFile);
	if(muxFile) FS_Close(muxFile);
	if(cbFile) FS_Close(cbFile);
	if(frame) RFree( frame );
	if(frames) RFree( frames );

	roqFile = cbFile = muxFile = NULL;
	frame = NULL;
	frames = NULL;	
}


/*
===============
ParseScript	
===============
*/
bool ROQ_ParseMovieScript (void)
{
	ResetMovieInfo();

	while (1)
	{
		if(!Com_GetToken (true))break;

		if (Com_MatchToken( "$videoname" )) Cmd_Videoname();
		else if (Com_MatchToken( "$fps" )) Cmd_FpsMovie();
		else if (Com_MatchToken( "$framemask" )) Cmd_FrameMask();
		else if (Com_MatchToken( "$sound" )) Cmd_Sound();
		else if (Com_MatchToken( "$quality")) Cmd_Quality();
		else if (!Com_ValidScript( QC_ROQLIB )) return false;
		else Cmd_MovieUnknown();
	}
	return true;
}

void ROQ_ProcessFrame( int num )
{
	frame = FS_LoadImage( frames->filenames[num], NULL, 0 ); 
	if(!frame || !frame->buffer) return; // technically an error
	if(!nextKeyframe)
	{
		ROQ_SetPreference(comp, ROQENC_PREF_KEYFRAME, 1);
		ROQ_SetPreference(comp, ROQENC_PREF_GOAL_SIZE_BITS, keyBits);
		nextKeyframe = keyRate;
	}
	else ROQ_SetPreference(comp, ROQENC_PREF_GOAL_SIZE_BITS, motionBits);
	nextKeyframe--;

	if(!ROQ_CompressRGB(comp, roqFile, ROQ_MakeUprightRGB( frames->filenames[num], frame )))
	{
		MsgWarn("ROQ_CompressRGB: frame encode failed\n");
		return;
	}
	Sys_GetKeyEvents(); // update console output
	if(frame) RFree( frame );
	iTotalFrames++;
}

void ROQ_ProcessVideo( void )
{
	roq_compressor_t	*comp;
	uint		i = 0;

	if(!frames) frames = FS_Search("frame?", true );
	if(!frames) Sys_Break("RoqLib: movie frames not found\n");
	roqFile = FS_Open( movieoutname, "wb" );

	com.strcpy(codebookname, movieoutname );
	FS_StripExtension( codebookname );
	FS_DefaultExtension( codebookname, ".cb" );

	if(!forceRegen)
	{
		cbFile = FS_Open( codebookname, "rb");
		if(!cbFile) forceRegen = true;
		else neverCache = true;
	}

	if(forceRegen && !neverCache)
	{
		cbFile = FS_Open( codebookname, "wb");
		if(!cbFile) Sys_Break("ROQ_ProcessVideo: could not open %s\n", codebookname );
	}

	frame = FS_LoadImage( frames->filenames[i], NULL, 0 ); 
	if(!frame) Sys_Break("frame not loaded\n");

	// allocate transform buffer
	buffer = RQalloc( frame->width * frame->height * 3);
	if(!keyBits) keyBits = (frame->width * frame->height * (3*8)) * keyPerc / 10000;
	if(!motionBits) motionBits = (frame->width * frame->height * (3*8)) * motionPerc / 10000;

	// write the RoQ header
	comp = ROQ_CreateCompressor();
	ROQ_SetPreference(comp, ROQENC_PREF_WIDTH, frame->width );
	ROQ_SetPreference(comp, ROQENC_PREF_HEIGHT, frame->height );
	ROQ_WriteHeader( roqFile );

	Msg ("Compress Video...\n");
	RunThreadsOnIndividual(frames->numfilenames, true, ROQ_ProcessFrame );
	Msg("Total Frames compressed: %i\n", iTotalFrames );

	// close file
	ROQ_DestroyCompressor(comp);
	RFree( buffer );
	RFree( frames );
	if(roqFile) FS_Close( roqFile );
	if(!forceRegen && !neverCache) FS_Close( cbFile );
}

void ROQ_ProcessAudio( void )
{
	return;

	if(!com.strlen(soundname)) return;

	com.strcpy( tempoutname, movieoutname );
	FS_StripExtension( tempoutname );
	FS_DefaultExtension( tempoutname, ".mux" );

	roqFile = FS_Open( movieoutname, "rb" );
	muxFile = FS_Open( tempoutname, "wb");
}

bool CompileCurrentMovie( const char *name )
{
	bool load = false;
	
	if( name ) com.strcpy( gs_filename, name );
	FS_DefaultExtension( gs_filename, ".qc" );
	load = Com_LoadScript( gs_filename, NULL, 0 );
	
	if(load)
	{
		if(!ROQ_ParseMovieScript())
			return false;
		ROQ_ProcessVideo();
		ROQ_ProcessAudio();
		return true;
	}

	Msg("%s not found\n", gs_filename );
	return false;
}

bool CompileROQVideo( byte *mempool, const char *name, byte parms )
{
	if(mempool) roqpool = mempool;
	else
	{
		Msg("RoqLib: can't allocate memory pool.\nAbort compilation\n");
		return false;
	}
	return CompileCurrentMovie( name );	
}