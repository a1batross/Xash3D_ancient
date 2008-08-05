//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cl_video.c - playback cinematic
//=======================================================================

#include "common.h"
#include "client.h"

#define DPVIDEO_VERSION		1

typedef enum
{
	DPVERROR_NONE = 0,
	DPVERROR_EOF,
	DPVERROR_READERROR,
	DPVERROR_INVALIDRMASK,
	DPVERROR_INVALIDGMASK,
	DPVERROR_INVALIDBMASK,
	DPVERROR_COLORMASKSOVERLAP,
	DPVERROR_COLORMASKSEXCEEDBPP,
	DPVERROR_UNSUPPORTEDBPP,
	DPVERROR_UNKNOWN
} e_dpv_errors;

typedef enum
{
	cin_uninitialized = 0,
	cin_playback,
	cin_loop,
	cin_pause,
	cin_firstframe,	
} e_cin_status;

#define HZREADERROR_OK 0
#define HZREADERROR_EOF 1
#define HZREADERROR_MALLOCFAILED 2
#define HZREADBLOCKSIZE 1048576

typedef struct hz_bitstream_read_s
{
	file_t		*file;
	int		endoffile;
} hz_bitstream_read_t;

typedef struct hz_block_s
{
	struct hz_block_s	*next;
	uint		size;
	byte		data[HZREADBLOCKSIZE];
} hz_block_t;

typedef struct hz_blocks_s
{
	hz_block_t	*blocks;
	hz_block_t	*current;
	uint		position;
	uint		store;
	int		count;
} hz_blocks_t;

hz_bitstream_read_t *hz_bitstream_read_open( const char *filename )
{
	file_t		*file;
	hz_bitstream_read_t	*stream;

	file = FS_Open( filename, "rb" );
	if( file )
	{
		stream = (hz_bitstream_read_t *)Mem_Alloc( cls.mempool, sizeof(hz_bitstream_read_t));
		stream->file = file;
		return stream;
	}
	return NULL;
}

void hz_bitstream_read_close( hz_bitstream_read_t *stream )
{
	if( stream )
	{
		FS_Close( stream->file );
		Mem_Free( stream );
	}
}

hz_blocks_t *hz_bitstream_read_blocks_new( void )
{
	hz_blocks_t	*blocks;

	blocks = (hz_blocks_t *)Mem_Alloc( cls.mempool, sizeof(hz_blocks_t));
	return blocks;
}

void hz_bitstream_read_blocks_free( hz_blocks_t *blocks )
{
	hz_block_t	*cur, *next;

	if( blocks == NULL ) return;
	for( cur = blocks->blocks; cur; cur = next )
	{
		next = cur->next;
		Mem_Free( cur );
	}
	Mem_Free( blocks );
}

void hz_bitstream_read_flushbits( hz_blocks_t *blocks )
{
	blocks->store = 0;
	blocks->count = 0;
}

bool hz_bitstream_read_blocks_read( hz_blocks_t *blocks, hz_bitstream_read_t *stream, uint size )
{
	int		s = size;
	hz_block_t	*b = blocks->blocks, *p = NULL;

	while( s > 0 )
	{
		if( b == NULL )
		{
			b = (hz_block_t *)Mem_Alloc( cls.mempool, sizeof(hz_block_t));
			b->next = NULL;
			b->size = 0;
			if( p != NULL )
				p->next = b;
			else blocks->blocks = b;
		}
		if( s > HZREADBLOCKSIZE )
			b->size = HZREADBLOCKSIZE;
		else b->size = s;
		s -= b->size;
		if(FS_Read( stream->file, b->data, b->size) != (fs_offset_t)b->size)
		{
			stream->endoffile = 1;
			break;
		}
		p = b;
		b = b->next;
	}
	while( b )
	{
		b->size = 0;
		b = b->next;
	}

	blocks->current = blocks->blocks;
	blocks->position = 0;
	hz_bitstream_read_flushbits(blocks);
	if( stream->endoffile )
		return false;
	return true;
}

uint hz_bitstream_read_blocks_getbyte(hz_blocks_t *blocks)
{
	while (blocks->current != NULL && blocks->position >= blocks->current->size)
	{
		blocks->position = 0;
		blocks->current = blocks->current->next;
	}
	if (blocks->current == NULL)
		return 0;
	return blocks->current->data[blocks->position++];
}

int hz_bitstream_read_bit( hz_blocks_t *blocks )
{
	if( !blocks->count )
	{
		blocks->count += 8;
		blocks->store <<= 8;
		blocks->store |= hz_bitstream_read_blocks_getbyte(blocks) & 0xFF;
	}
	blocks->count--;
	return (blocks->store >> blocks->count) & 1;
}

uint hz_bitstream_read_bits( hz_blocks_t *blocks, int size )
{
	uint num = 0;

	// we can only handle about 24 bits at a time safely
	// (there might be up to 7 bits more than we need in the bit store)
	if( size > 24 )
	{
		size -= 8;
		num |= hz_bitstream_read_bits(blocks, 8) << size;
	}
	while( blocks->count < size )
	{
		blocks->count += 8;
		blocks->store <<= 8;
		blocks->store |= hz_bitstream_read_blocks_getbyte(blocks) & 0xFF;
	}
	blocks->count -= size;
	num |= (blocks->store >> blocks->count) & ((1 << size) - 1);
	return num;
}

uint hz_bitstream_read_byte( hz_blocks_t *blocks )
{
	return hz_bitstream_read_blocks_getbyte( blocks );
}

uint hz_bitstream_read_short( hz_blocks_t *blocks )
{
	return (hz_bitstream_read_byte(blocks) << 8)|(hz_bitstream_read_byte(blocks));
}

uint hz_bitstream_read_long( hz_blocks_t *blocks )
{
	return (hz_bitstream_read_byte(blocks) << 24)|(hz_bitstream_read_byte(blocks) << 16)|(hz_bitstream_read_byte(blocks) << 8)|(hz_bitstream_read_byte(blocks));
}

void hz_bitstream_read_bytes( hz_blocks_t *blocks, void *outdata, uint size )
{
	byte	*out;

	out = (byte *)outdata;
	while( size-- ) *out++ = hz_bitstream_read_byte( blocks );
}

typedef struct dpvstream_s
{
	hz_bitstream_read_t	*bitstream;
	hz_blocks_t	*framedatablocks;

	int		error;

	float		info_framerate;
	uint		info_frames;

	uint		info_imagewidth;
	uint		info_imageheight;
	uint		info_imagebpp;
	uint		info_imageRloss;
	uint		info_imageRmask;
	uint		info_imageRshift;
	uint		info_imageGloss;
	uint		info_imageGmask;
	uint		info_imageGshift;
	uint		info_imageBloss;
	uint		info_imageBmask;
	uint		info_imageBshift;
	uint		info_imagesize;

	// current video frame data(needed because of delta compression)
	int		videoframenum;
	uint		*videopixels;

	// channel the sound file is being played on
	int		sndchan;
} dpvstream_t;

static int dpv_setpixelformat( dpvstream_t *s, uint Rmask, uint Gmask, uint Bmask, uint bpp )
{
	int Rshift, Rbits, Gshift, Gbits, Bshift, Bbits;
	if(!Rmask)
	{
		s->error = DPVERROR_INVALIDRMASK;
		return s->error;
	}
	if(!Gmask)
	{
		s->error = DPVERROR_INVALIDGMASK;
		return s->error;
	}
	if(!Bmask)
	{
		s->error = DPVERROR_INVALIDBMASK;
		return s->error;
	}
	if(Rmask & Gmask || Rmask & Bmask || Gmask & Bmask)
	{
		s->error = DPVERROR_COLORMASKSOVERLAP;
		return s->error;
	}
	switch( bpp )
	{
	case 2:
		if((Rmask | Gmask | Bmask) > 65536)
		{
			s->error = DPVERROR_COLORMASKSEXCEEDBPP;
			return s->error;
		}
		break;
	case 4:
		break;
	default:
		s->error = DPVERROR_UNSUPPORTEDBPP;
		return s->error;
		break;
	}
	for (Rshift = 0;!(Rmask & 1);Rshift++, Rmask >>= 1);
	for (Gshift = 0;!(Gmask & 1);Gshift++, Gmask >>= 1);
	for (Bshift = 0;!(Bmask & 1);Bshift++, Bmask >>= 1);
	if (((Rmask + 1) & Rmask) != 0)
	{
		s->error = DPVERROR_INVALIDRMASK;
		return s->error;
	}
	if (((Gmask + 1) & Gmask) != 0)
	{
		s->error = DPVERROR_INVALIDGMASK;
		return s->error;
	}
	if (((Bmask + 1) & Bmask) != 0)
	{
		s->error = DPVERROR_INVALIDBMASK;
		return s->error;
	}
	for (Rbits = 0;Rmask & 1;Rbits++, Rmask >>= 1);
	for (Gbits = 0;Gmask & 1;Gbits++, Gmask >>= 1);
	for (Bbits = 0;Bmask & 1;Bbits++, Bmask >>= 1);
	if (Rbits > 8)
	{
		Rshift += (Rbits - 8);
		Rbits = 8;
	}
	if (Gbits > 8)
	{
		Gshift += (Gbits - 8);
		Gbits = 8;
	}
	if (Bbits > 8)
	{
		Bshift += (Bbits - 8);
		Bbits = 8;
	}
	s->info_imagebpp = bpp;
	s->info_imageRloss = 16 + (8 - Rbits);
	s->info_imageGloss =  8 + (8 - Gbits);
	s->info_imageBloss =  0 + (8 - Bbits);
	s->info_imageRmask = (1 << Rbits) - 1;
	s->info_imageGmask = (1 << Gbits) - 1;
	s->info_imageBmask = (1 << Bbits) - 1;
	s->info_imageRshift = Rshift;
	s->info_imageGshift = Gshift;
	s->info_imageBshift = Bshift;
	s->info_imagesize = s->info_imagewidth * s->info_imageheight * s->info_imagebpp;
	return s->error;
}

// opening and closing streams

// opens a stream
void *dpv_open( const char *filename, char **errorstring )
{
	dpvstream_t	*s;
	char		t[8];

	if( errorstring != NULL ) *errorstring = NULL;

	s = (dpvstream_t *)Mem_Alloc( cls.mempool, sizeof(dpvstream_t));
	s->bitstream = hz_bitstream_read_open( filename );
	if( s->bitstream != NULL )
	{
		// check file identification
		s->framedatablocks = hz_bitstream_read_blocks_new();
		hz_bitstream_read_blocks_read(s->framedatablocks, s->bitstream, 8);
		hz_bitstream_read_bytes(s->framedatablocks, t, 8);
		if(!memcmp(t, "DPVideo", 8))
		{
			// check version number
			hz_bitstream_read_blocks_read( s->framedatablocks, s->bitstream, 2 );
			if( hz_bitstream_read_short(s->framedatablocks) == DPVIDEO_VERSION )
			{
				hz_bitstream_read_blocks_read(s->framedatablocks, s->bitstream, 12);
				s->info_imagewidth = hz_bitstream_read_short(s->framedatablocks);
				s->info_imageheight = hz_bitstream_read_short(s->framedatablocks);
				s->info_framerate = (float)hz_bitstream_read_long(s->framedatablocks) * (1.0/65536.0);

				if( s->info_framerate > 0.0f )
				{
					s->videopixels = (uint *)Mem_Alloc( cls.mempool, s->info_imagewidth * s->info_imageheight * sizeof(*s->videopixels));
					s->videoframenum = MAX_HEARTBEAT;
					return s;
				}
				else if( errorstring != NULL ) *errorstring = "error in video info chunk";
			}
			else if( errorstring != NULL ) *errorstring = "read error";
		}
		else if( errorstring != NULL )  *errorstring = "not a dpvideo file";
 		hz_bitstream_read_blocks_free(s->framedatablocks);
	}
	else if( errorstring != NULL ) *errorstring = "unable to open file";
	Mem_Free( s );

	return NULL;
}

// closes a stream
void dpv_close( void *stream )
{
	dpvstream_t *s = (dpvstream_t *)stream;

	if( s == NULL ) return;
	if( s->videopixels ) Mem_Free( s->videopixels );
	if( s->sndchan != -1 ) S_StopAllSounds();	//FIXME
	if( s->framedatablocks ) hz_bitstream_read_blocks_free( s->framedatablocks );
	if( s->bitstream ) hz_bitstream_read_close( s->bitstream );
	Mem_Free( s );
}

// utilitarian functions
int dpv_error( void *stream, char **errorstring )
{
	dpvstream_t	*s = (dpvstream_t *)stream;
	int		e = s->error;

	s->error = 0;	// clear last error
	if( errorstring )
	{
		switch( e )
		{
		case DPVERROR_NONE:
			*errorstring = "no error";
			break;
		case DPVERROR_EOF:
			*errorstring = "end of file reached (this is not an error)";
			break;
		case DPVERROR_READERROR:
			*errorstring = "read error (corrupt or incomplete file)";
			break;
		case DPVERROR_INVALIDRMASK:
			*errorstring = "invalid red bits mask";
			break;
		case DPVERROR_INVALIDGMASK:
			*errorstring = "invalid green bits mask";
			break;
		case DPVERROR_INVALIDBMASK:
			*errorstring = "invalid blue bits mask";
			break;
		case DPVERROR_COLORMASKSOVERLAP:
			*errorstring = "color bit masks overlap";
			break;
		case DPVERROR_COLORMASKSEXCEEDBPP:
			*errorstring = "color masks too big for specified bytes per pixel";
			break;
		case DPVERROR_UNSUPPORTEDBPP:
			*errorstring = "unsupported bytes per pixel (must be 2 for 16bit, or 4 for 32bit)";
			break;
		default:
			*errorstring = "unknown error";
			break;
		}
	}
	return e;
}

// returns the width of the image data
uint dpv_getwidth( const void *stream )
{
	dpvstream_t *s = (dpvstream_t *)stream;
	return s->info_imagewidth;
}

// returns the height of the image data
uint dpv_getheight( const void *stream )
{
	dpvstream_t *s = (dpvstream_t *)stream;
	return s->info_imageheight;
}

// returns the framerate of the stream
float dpv_getframerate( const void *stream )
{
	dpvstream_t *s = (dpvstream_t *)stream;
	return s->info_framerate;
}


static int dpv_convertpixels( dpvstream_t *s, void *imagedata, int imagebytesperrow )
{
	uint	a, x, y, width, height;
	uint	Rloss, Rmask, Rshift, Gloss, Gmask, Gshift, Bloss, Bmask, Bshift;
	uint	*in;

	width = s->info_imagewidth;
	height = s->info_imageheight;

	Rloss = s->info_imageRloss;
	Rmask = s->info_imageRmask;
	Rshift = s->info_imageRshift;
	Gloss = s->info_imageGloss;
	Gmask = s->info_imageGmask;
	Gshift = s->info_imageGshift;
	Bloss = s->info_imageBloss;
	Bmask = s->info_imageBmask;
	Bshift = s->info_imageBshift;

	in = s->videopixels;
	if( s->info_imagebpp == 4 )
	{
		uint *outrow;
		for( y = 0; y < height; y++ )
		{
			outrow = (uint *)((byte *)imagedata + y * imagebytesperrow);
			for (x = 0;x < width;x++)
			{
				a = *in++;
				outrow[x] = (((a >> Rloss) & Rmask) << Rshift) | (((a >> Gloss) & Gmask) << Gshift) | (((a >> Bloss) & Bmask) << Bshift);
			}
		}
	}
	else
	{
		word *outrow;
		for( y = 0; y < height; y++ )
		{
			outrow = (word *)((byte *)imagedata + y * imagebytesperrow);
			if (Rloss == 19 && Gloss == 10 && Bloss == 3 && Rshift == 11 && Gshift == 5 && Bshift == 0)
			{
				// optimized
				for (x = 0;x < width;x++)
				{
					a = *in++;
					outrow[x] = ((a >> 8) & 0xF800) | ((a >> 5) & 0x07E0) | ((a >> 3) & 0x001F);
				}
			}
			else
			{
				for (x = 0;x < width;x++)
				{
					a = *in++;
					outrow[x] = (((a >> Rloss) & Rmask) << Rshift) | (((a >> Gloss) & Gmask) << Gshift) | (((a >> Bloss) & Bmask) << Bshift);
				}
			}
		}
	}
	return s->error;
}

static int dpv_decompressimage( dpvstream_t *s )
{
	int	i, a, b, colors, x1, y1, bw, bh, width, height, palettebits;
	uint	palette[256], *outrow, *out;

	width = s->info_imagewidth;
	height = s->info_imageheight;

	for( y1 = 0; y1 < height; y1 += 8 )
	{
		outrow = s->videopixels + y1 * width;
		bh = 8;
		if( y1 + bh > height ) bh = height - y1;
		for( x1 = 0; x1 < width; x1 += 8 )
		{
			out = outrow + x1;
			bw = 8;
			if( x1 + bw > width ) bw = width - x1;
			if( hz_bitstream_read_bit(s->framedatablocks))
			{
				// updated block
				palettebits = hz_bitstream_read_bits( s->framedatablocks, 3 );
				colors = 1 << palettebits;
				for (i = 0; i < colors; i++ )
					palette[i] = hz_bitstream_read_bits(s->framedatablocks, 24);
				if( palettebits )
				{
					for( b = 0; b < bh; b++, out += width )
						for( a = 0; a < bw; a++)
							out[a] = palette[hz_bitstream_read_bits(s->framedatablocks, palettebits)];
				}
				else
				{
					for( b = 0; b < bh; b++, out += width )
						for( a = 0; a < bw; a++) out[a] = palette[0];
				}
			}
		}
	}
	return s->error;
}

// decodes a video frame to the supplied output pixels
int dpv_video( void *stream, void *imagedata, uint Rmask, uint Gmask, uint Bmask, uint bpp, int bpr )
{
	dpvstream_t	*s = (dpvstream_t *)stream;
	uint		framedatasize;
	char		t[4];

	if( !stream ) return DPVERROR_UNKNOWN;

	s->error = DPVERROR_NONE;
	if( dpv_setpixelformat( s, Rmask, Gmask, Bmask, bpp ))
		return s->error;

	hz_bitstream_read_blocks_read( s->framedatablocks, s->bitstream, 8 );
	hz_bitstream_read_bytes( s->framedatablocks, t, 4 );
	if(memcmp(t, "VID0", 4))
	{
		if( t[0] == 0 ) return (s->error = DPVERROR_EOF);
		return (s->error = DPVERROR_READERROR);
	}
	framedatasize = hz_bitstream_read_long( s->framedatablocks );
	hz_bitstream_read_blocks_read( s->framedatablocks, s->bitstream, framedatasize );
	if (dpv_decompressimage(s))
		return s->error;

	dpv_convertpixels( s, imagedata, bpr );
	return s->error;
}


void *stream;
byte *frame_data = NULL;
uint frame_width;
uint frame_height;
int frame_num;
int start_time;
float frame_rate;
int frame_bpp;
int cin_state;
int cin_rmask;
int cin_gmask;
int cin_bmask;

/*
==================
CIN_StopCinematic
==================
*/
void CIN_StopCinematic( void )
{
	if( stream ) 
	{
		dpv_close( stream );
		stream = NULL;
	}

	if( cin_state == cin_playback )
	{
		cin_state = cin_firstframe;
		cls.state = ca_disconnected;
	}
}

/*
==================
CIN_RunCinematic

decompress next frame
==================
*/
void CIN_RunCinematic( void )
{
	int	destframe;

	if( cls.state != ca_cinematic )
		return;

	if( cin_state == cin_firstframe )
	{
		destframe = 0;
		cin_state = cin_playback;
	}
	else destframe = (int)(((cls.realtime - start_time) * 0.001) * frame_rate);

	if( destframe < 0 ) destframe = 0;
	if( destframe > frame_num )
	{
		do {
			frame_num++;
			if( dpv_video( stream, frame_data, cin_rmask, cin_gmask, cin_bmask, 4, frame_width * 4 ))
			{
				// finished?
				CIN_StopCinematic();
				return;
			}
		} while( frame_num < destframe );
	}
}

/*
==================
CIN_DrawCinematic

==================
*/
void CIN_DrawCinematic( void )
{
	float	x, y, w, h;

	if( cls.state != ca_cinematic )
		return;

	x = y = 0;
	w = SCREEN_WIDTH;
	h = SCREEN_HEIGHT;
	SCR_AdjustSize( &x, &y, &w, &h );

	re->DrawStretchRaw( x, y, w, h, frame_width, frame_height, frame_data, true );
}

/*
==================
CIN_PlayCinematic

==================
*/
bool CIN_PlayCinematic( const char *filename )
{
	char	*errorstring;
	union
	{
		byte	b[4];
		uint	i;
	} bgra;

	stream = dpv_open( filename, &errorstring );
	if( !stream )
	{
		CIN_StopCinematic();
		MsgDev( D_ERROR, "%s\n", errorstring );
		return false;
	}

	// set masks in an endian-independent way (as they really represent bytes)
	bgra.i = 0;
	bgra.b[2] = 0xFF;
	cin_bmask = bgra.i;

	bgra.i = 0;
	bgra.b[1] = 0xFF;
	cin_gmask = bgra.i;

	bgra.i = 0;
	bgra.b[0] = 0xFF;
	cin_rmask = bgra.i;

	frame_width = dpv_getwidth( stream );
	frame_height = dpv_getheight( stream );
	frame_data = Mem_Realloc( cls.mempool, frame_data, frame_width * frame_height * 4 );

	cin_state = cin_firstframe;
	frame_num = -1;
	frame_rate = dpv_getframerate( stream );
	start_time = cls.realtime;
	com.strncpy( cls.servername, filename, sizeof(cls.servername));
	cls.state = ca_cinematic;
				
	return true;
}