//=======================================================================
//			Copyright XashXT Group 2007 ©
//			roq_dec.c - ROQ video decoder
//=======================================================================

#include "roqlib.h"

// MAXIMUM CHUNK SIZE DETERMINATION
// Worst-case = Quad (Quad Quad Quad Quad)
// 10 bits from markers
// 128 bits from data
// (Width x Height) x 138 = Max bits
// Add 10 bytes to pad
// While this is NOT within the normal RoQ specifications, it is
// consistant in output from the Id/Trilobyte encoder and SwitchBlade

dword MaxVideoChunkSize(dword width, dword height)
{
	return (((width * height) * 138) / 8) + 10;
}

// Localized marker data
short		marker;
dword		size;
short		argument;
dword		width;
dword		height;
dword		rowScanWidth;
block2		*cb2;
block4		*cb4;
block8		*cb4large;
byte		*roqpool;
roq_state_t	local;

int ROQ_ProcessInfoTag(roq_dec_t *dc)
{
	byte	data[8];

	if(size != 8)
	{
		MsgDev(D_WARN, "ROQ_ProcessInfoTag: Corrupt info block\n");
		return ROQ_MARKER_EOF; // Corrupt info block
	}

	if(dc->flags & ROQ_VIDEO_INITIALIZED)
	{
		MsgDev(D_WARN, "ROQ_ProcessInfoTag: Video already ready\n");
		return ROQ_MARKER_EOF; // Video already initialized
	}

	if(!FS_Read(local.cinematic, data, 8))
	{
		MsgDev(D_WARN, "ROQ_ProcessInfoTag: No tag\n");
		return ROQ_MARKER_EOF;
	}

	// Parse data
	width = data[0] | (data[1] << 8);
	height = data[2] | (data[3] << 8);

	if(!width || !height)
	{
		MsgDev(D_WARN, "ROQ_ProcessInfoTag: Bad dimensions\n");
		return ROQ_MARKER_EOF;
	}

	if((width & 15) || (height & 15))
	{
		MsgDev(D_WARN, "ROQ_ProcessInfoTag: Bad dimensions 2\n");
		return ROQ_MARKER_EOF; // Bad dimensions
	}

	// Initialize video.  Use one big pile of memory for simplicity's sake
	local.maxVideo = MaxVideoChunkSize(width, height);
	local.videoMemory = RQalloc(local.maxVideo + (width*height*3*2));
	local.videoBuffers[0] = local.videoMemory;
	local.videoBuffers[1] = local.videoMemory + (width*height*3);
	local.videoChunk = local.videoMemory + (width*height*3*2);

	dc->width = (word)width;
	dc->height = (word)height;

	// Set as initialized
	dc->flags |= ROQ_VIDEO_INITIALIZED;

	return ROQ_MARKER_INFO;
}

int ROQ_ProcessMonoAudio(roq_dec_t *dc)
{
	short	lastValue;
	short	newValue;
	byte	*input;
	short	*output;
	byte	v;
	short	base;
	int	sign;

	if(size > AUDIO_SAMPLE_MAX)
	{
		MsgDev(D_WARN, "ROQ_ProcessMonoAudio: overflow\n");
		return ROQ_MARKER_EOF; // Audio overflow
	}

	// Read data
	if(!FS_Read(local.cinematic, local.audioChunk, size))
	{
		MsgDev(D_WARN, "ROQ_ProcessMonoAudio: can't read\n");
		return ROQ_MARKER_EOF;
	}

	// Decode samples
	input = local.audioChunk;
	dc->audioSize = size;
	output = dc->audioSamples;

	lastValue = argument;
	while(size)
	{
		v = *input++;

		// Separate base and sign
		sign = v & 128;
		base = v & 127;

		if(sign) newValue = lastValue - (base * base);
		else newValue = lastValue + (base * base);
		*output++ = lastValue = newValue;
		size--;
	}
	return ROQ_MARKER_SND_MONO;
}

int ROQ_ProcessStereoAudio(roq_dec_t *dc)
{
	short	lastValueL;
	short	lastValueR;
	short	newValue;
	byte	*input;
	short	*output;
	byte	v;
	short	base;
	int	sign;

	if(size & 1)
	{
		MsgDev(D_WARN, "ROQ_ProcessStereoAudio: corrupt data\n");
		return ROQ_MARKER_EOF; // Oops
	}
	size /= 2;

	if(size > AUDIO_SAMPLE_MAX)
	{
		MsgDev(D_WARN, "ROQ_ProcessStereoAudio: overflow\n");
		return ROQ_MARKER_EOF; // Audio overflow
	}

	// Read data
	if(!FS_Read(local.cinematic, local.audioChunk, size*2))
	{
		MsgDev(D_WARN, "ROQ_ProcessStereoAudio: can't read\n");
		return ROQ_MARKER_EOF;
	}

	// Decode samples
	input = local.audioChunk;
	dc->audioSize = size;
	output = dc->audioSamples;

	lastValueL = argument & 0xFF00;
	lastValueR = (argument & 255) << 8;
	while(size)
	{
		v = *input++;

		// Separate base and sign
		sign = v & 128;
		base = v & 127;

		if(sign) newValue = lastValueL - (base * base);
		else newValue = lastValueL + (base * base);
		*output++ = lastValueL = newValue;
		v = *input++;

		// Separate base and sign
		sign = v & 128;
		base = v & 127;

		if(sign) newValue = lastValueR - (base*base);
		else newValue = lastValueR + (base*base);
		*output++ = lastValueR = newValue;
		size--;
	}
	return ROQ_MARKER_SND_STEREO;
}

void YUVtoRGB(int y, int u, int v, byte *r, byte *g, byte *b)
{
	int	rc,gc,bc;

	u -= 128;
	v -= 128;

	rc = (y<<6) + (90*(int)v);
	gc = (y<<6) - (22*(int)u) - (46*(int)v);
	bc = (y<<6) + (113*(int)u);
	if(rc < 0) *r = 0; else if(rc > 16320) *r = 255; else *r = (byte)(rc>>6);
	if(gc < 0) *g = 0; else if(gc > 16320) *g = 255; else *g = (byte)(gc>>6);
	if(bc < 0) *b = 0; else if(bc > 16320) *b = 255; else *b = (byte)(bc>>6);
}

int ROQ_ProcessCodebooks(roq_dec_t *dc)
{
	dword	block;
	byte	block2data[1536];
	byte	block4data[1024];
	byte	*input;
	byte	*output;
	byte	y1,y2,y3,y4,u,v;

	// Parse out count
	dword	blockCount2;
	dword	blockCount4;

	cb2 = local.cb2;
	cb4 = local.cb4;
	cb4large = local.cb4large;

	blockCount2 = (argument >> 8) & 255;
	blockCount4 = argument & 255;
	if(!blockCount2) blockCount2 = 256;
	if(!blockCount4 && blockCount2 * 6 < size) blockCount4 = 256;

	// Read in the codebook
	if(size > blockCount2*6 + blockCount4*4)
	{
		MsgDev(D_WARN, "ROQ_ProcessCodebooks: bad codebook count\n");
		return ROQ_MARKER_EOF; // Too small to be possible
	}
	if(!FS_Read(local.cinematic, block2data, blockCount2*6))
	{
		MsgDev(D_WARN, "ROQ_ProcessCodebooks: read failed on cb2\n");
		return ROQ_MARKER_EOF;
	}
	if(!FS_Read(local.cinematic, block4data, blockCount4*4))
	{
		MsgDev(D_WARN, "ROQ_ProcessCodebooks: read failed on cb4\n");
		return ROQ_MARKER_EOF;
	}

	// Decode the 2x2 blocks
	input = block2data;
	for(block = 0; block < blockCount2; block++)
	{
		y1 = *input++;
		y2 = *input++;
		y3 = *input++;
		y4 = *input++;
		u = *input++;
		v = *input++;

		// Decode YUV values
		output = cb2[block];

		// 0  1  2    3  4  5
		// 6  7  8    9  10 11
		YUVtoRGB(y1, u, v, output, output+1, output+2);
		YUVtoRGB(y2, u, v, output+3, output+4, output+5);
		YUVtoRGB(y3, u, v, output+6, output+7, output+8);
		YUVtoRGB(y4, u, v, output+9, output+10, output+11);
	}

	// Decode the 4x4 blocks
	input = block4data;
	for(block = 0; block < blockCount4; block++)
	{
		y1 = *input++;
		y2 = *input++;
		y3 = *input++;
		y4 = *input++;

		// Blit the cb2 blocks on to the cb4 blocks
		output = cb4[block];

		ROQ_Blit(cb2[y1], output, 6, 6, 12, 2);
		ROQ_Blit(cb2[y2], output+6, 6, 6, 12, 2);
		ROQ_Blit(cb2[y3], output+(12*2), 6, 6, 12, 2);
		ROQ_Blit(cb2[y4], output+(12*2)+6, 6, 6, 12, 2);

		// Create 8x8 blocks
		ROQ_DoubleSize(output, cb4large[block], 4);
	}

	return ROQ_MARKER_CODEBOOK;
}


#define GETBYTE(x) if(!available) { return ROQ_MARKER_EOF; } x = *vidinput++; available--
#define GETWORD(x) if(available < 2) { return ROQ_MARKER_EOF; } x = (*vidinput++); x |= (*vidinput++) << 8; available -= 2
#define UNSPOOL(x) \
	if(parameterBits==0)\
	{\
		GETWORD(parameterWord);\
		parameterBits=16;\
	}\
	parameterBits -= 2;\
	x = (parameterWord >> parameterBits) & 3;

#define BLOCKMARK_SKIP	0
#define BLOCKMARK_MOTION	1
#define BLOCKMARK_VECTOR	2
#define BLOCKMARK_QUAD	3

byte	*frontBuffer;
byte	*backBuffer;
byte	*vidinput;
dword	available;
word	parameterWord;
word	parameterBits;
char	dx,dy;

int ROQ_ProcessQuad2(long x, long y)
{
	byte	blockid;
	GETBYTE(blockid);

	// Blit in a 2x2 block
	ROQ_Blit(cb2[blockid], frontBuffer+(x*3)+(rowScanWidth*y), 2*3, 2*3, rowScanWidth, 2);
	return ROQ_MARKER_VIDEO;
}

int ROQ_ProcessQuad4(long x, long y)
{
	int	mx,my;
	int	blockmark;
	int	movement;
	byte	blockid;

	UNSPOOL(blockmark);

	switch(blockmark)
	{
	case BLOCKMARK_SKIP:
		return ROQ_MARKER_VIDEO;
	case BLOCKMARK_MOTION:
		GETBYTE(movement);

		// Decode offsets
		mx = x + 8 - (int)(movement >> 4) - (int)dx;
		my = y + 8 - (int)(movement & 15) - (int)dy;

		if(my < 0 || mx < 0 || (dword)mx > width-4 || (dword)my > height-4)
		{
			MsgDev(D_WARN, "ROQ_ProcessQuad4: bad motion\n");
			return ROQ_MARKER_EOF; // Out of bounds
		}

		// Blit the entire block
		ROQ_Blit(backBuffer+(mx*3)+(my*rowScanWidth), frontBuffer+(x*3)+(y*rowScanWidth), 4*3, rowScanWidth, rowScanWidth, 4);
		return ROQ_MARKER_VIDEO;
	case BLOCKMARK_VECTOR:
		GETBYTE(blockid);
		// Blit a 4x4 block
		ROQ_Blit(cb4[blockid], frontBuffer+(x*3)+(y*rowScanWidth), 4*3, 4*3, rowScanWidth, 4);
		return ROQ_MARKER_VIDEO;
	case BLOCKMARK_QUAD:
		// Process 4 subsection quads
		if(ROQ_ProcessQuad2(x, y) == ROQ_MARKER_EOF)
		{
			MsgDev(D_WARN, "ROQ_ProcessQuad4: quad2 failed\n");
			return ROQ_MARKER_EOF;
		}
		if(ROQ_ProcessQuad2(x+2, y) == ROQ_MARKER_EOF)
		{
			MsgDev(D_WARN, "ROQ_ProcessQuad4: quad2 failed 2\n");
			return ROQ_MARKER_EOF;
		}
		if(ROQ_ProcessQuad2(x, y+2) == ROQ_MARKER_EOF)
		{
			MsgDev(D_WARN, "ROQ_ProcessQuad4: quad2 failed 3\n");
			return ROQ_MARKER_EOF;
		}
		if(ROQ_ProcessQuad2(x+2, y+2) == ROQ_MARKER_EOF)
		{
			MsgDev(D_WARN, "ROQ_ProcessQuad4: quad2 failed 4\n");
			return ROQ_MARKER_EOF;
		}
		return ROQ_MARKER_VIDEO;
	}
	return ROQ_MARKER_EOF; // Shut up compiler warning
}

int ROQ_ProcessQuad8(long x, long y)
{
	int	mx,my;
	int	blockmark;
	byte	movement;
	byte	blockid;

	UNSPOOL(blockmark);

	switch(blockmark)
	{
	case BLOCKMARK_SKIP:
		return ROQ_MARKER_VIDEO;
	case BLOCKMARK_MOTION:
		GETBYTE(movement);

		// Decode offsets
		mx = x + 8 - ((movement >> 4) & 15) - (int)dx;
		my = y + 8 - (movement & 15) - (int)dy;

		if(mx < 0 || my < 0 || (dword)mx > width-8 || (dword)my > height-8)
		{
			MsgDev(D_WARN, "ROQ_ProcessQuad8: quad8 out of bounds\n");
			return ROQ_MARKER_EOF; // Out of bounds
		}

		// Blit the entire block
		ROQ_Blit(backBuffer+(mx*3)+(my*rowScanWidth), frontBuffer+(x*3)+(y*rowScanWidth), 8*3, rowScanWidth, rowScanWidth, 8);
		return ROQ_MARKER_VIDEO;
	case BLOCKMARK_VECTOR:
		GETBYTE(blockid);
		// Blit an oversized 4x4 block
		ROQ_Blit(cb4large[blockid], frontBuffer+(x*3)+(y*rowScanWidth), 8*3, 8*3, rowScanWidth, 8);
		return ROQ_MARKER_VIDEO;
	case BLOCKMARK_QUAD:
		// Process 4 subsection quads
		if(ROQ_ProcessQuad4(x, y) == ROQ_MARKER_EOF)
			return ROQ_MARKER_EOF;
		if(ROQ_ProcessQuad4(x+4, y) == ROQ_MARKER_EOF)
			return ROQ_MARKER_EOF;
		if(ROQ_ProcessQuad4(x, y+4) == ROQ_MARKER_EOF)
			return ROQ_MARKER_EOF;
		if(ROQ_ProcessQuad4(x+4, y+4) == ROQ_MARKER_EOF)
			return ROQ_MARKER_EOF;
		return ROQ_MARKER_VIDEO;
	}
	return ROQ_MARKER_EOF; // Shut up compiler warning
}

int ROQ_ProcessQuad16(long x, long y)
{
	// Process 8x8 quads
	if(ROQ_ProcessQuad8(x, y) == ROQ_MARKER_EOF)
		return ROQ_MARKER_EOF;
	if(ROQ_ProcessQuad8(x+8, y) == ROQ_MARKER_EOF)
		return ROQ_MARKER_EOF;
	if(ROQ_ProcessQuad8(x, y+8) == ROQ_MARKER_EOF)
		return ROQ_MARKER_EOF;
	if(ROQ_ProcessQuad8(x+8, y+8) == ROQ_MARKER_EOF)
		return ROQ_MARKER_EOF;
	return ROQ_MARKER_VIDEO;
}

int ROQ_ProcessVideo(roq_dec_t *dc)
{
	long	x, y;
	parameterBits = 0;

	dc->frameNum++;

	if(!dc->flags & ROQ_VIDEO_INITIALIZED)
		return ROQ_MARKER_EOF; // Can't decompress without video initialized

	available = size;
	if(size > local.maxVideo) return ROQ_MARKER_EOF; // Too much video data
	if(!FS_Read(local.cinematic, local.videoChunk, size)) 
		return ROQ_MARKER_EOF; // Not enough real video data

	// Select buffers (and swap)
	frontBuffer = local.videoBuffers[local.currentBuffer];
	local.currentBuffer = !local.currentBuffer;
	backBuffer = local.videoBuffers[local.currentBuffer];

	// Blit the entire previous image to the backbuffer if this is the second frame
	// This is a stupid hack to keep some Id/Trilobyte encoded videos from screwing
	// up when they use SKIP markers in the second frame despite the fact that the
	// backbuffer should be empty
	if(dc->frameNum == 2) Mem_Copy(frontBuffer, backBuffer, width*height*3); // RGB

	width = dc->width;
	height = dc->height;
	rowScanWidth = width * 3;

	cb2 = local.cb2;
	cb4 = local.cb4;
	cb4large = local.cb4large;

	// Parse out average motion
	dx = ((argument >> 8) & 255);
	dy = (argument & 255);

	vidinput = local.videoChunk;
	dc->rgb = frontBuffer;

	for(y = 0; y < (long)height; y += 16)
	{
		for(x = 0; x < (long)width; x += 16)
		{
			if(ROQ_ProcessQuad16(x, y) == ROQ_MARKER_EOF)
				return ROQ_MARKER_EOF;
		}
	}
	return ROQ_MARKER_VIDEO;
}

int ROQ_ReadFrame(roq_dec_t *dc)
{
	byte	blockHeader[8];

	if(!FS_Read(local.cinematic, blockHeader, 8))
		return ROQ_MARKER_EOF;

	marker = (blockHeader[0] | (blockHeader[1] << 8));
	size = (blockHeader[2] | (blockHeader[3] << 8) | (blockHeader[4] << 16) | (blockHeader[5] << 24));
	argument = (blockHeader[6] | (blockHeader[7] << 8));

	switch(marker)
	{
	case ROQ_MARKER_INFO:
		return ROQ_ProcessInfoTag(dc);
	case ROQ_MARKER_SND_MONO:
		return ROQ_ProcessMonoAudio(dc);
	case ROQ_MARKER_SND_STEREO:
		return ROQ_ProcessStereoAudio(dc);
	case ROQ_MARKER_CODEBOOK:
		return ROQ_ProcessCodebooks(dc);
	case ROQ_MARKER_VIDEO:
		return ROQ_ProcessVideo(dc);
	default:
		return ROQ_MARKER_EOF; // Unknown
	}
}

// CreateDecompressor -- Creates an RoQ decompressor object
roq_dec_t *ROQ_OpenVideo(const char *name)
{
	byte		junk[8];
	roq_dec_t		*dc;
	file_t		*cin_file;
	char		roqname[MAX_QPATH];

	sprintf (roqname, "video/%s", name );
	cin_file = FS_Open( roqname, "rb" );
	if(!cin_file) return NULL;

	// Skip the first 8 bytes of the file
	if(!FS_Read(cin_file, junk, 8))
		return NULL;

	roqpool = Mem_AllocPool( "roqlib zone");
	dc = RQalloc(sizeof(roq_dec_t));

	dc->flags = 0;
	local.currentBuffer = 0;
	local.cinematic = cin_file;
	dc->frameNum = 0;
	dc->audioSamples = local.audioSamples;

	return dc;
}

void ROQ_FreeVideo(roq_dec_t *dec)
{
	if(local.cinematic) FS_Close( local.cinematic );
	if(roqpool) Mem_FreePool( &roqpool );// drain memory
	memset(&local, 0, sizeof(roq_state_t));
	if(dec) dec = NULL; // reset
}

roqlib_api_t ROQ_GetAPI( void )
{
	static roqlib_api_t	Roq;

	Roq.api_size = sizeof(roqlib_api_t);

	Roq.LoadVideo = ROQ_OpenVideo;
	Roq.FreeVideo = ROQ_FreeVideo;
	Roq.ReadFrame = ROQ_ReadFrame;

	return Roq;
}
