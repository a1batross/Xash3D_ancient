//=======================================================================
//			Copyright XashXT Group 2007 ©
//			s_openal.h - openal definition
//=======================================================================

#ifndef S_STREAM_H
#define S_STREAM_H

typedef __int64 ogg_int64_t;
typedef __int32 ogg_int32_t;
typedef unsigned __int32 ogg_uint32_t;
typedef __int16 ogg_int16_t;
typedef unsigned __int16 ogg_uint16_t;

/*
=======================================================================
		OGG PACK DEFINITION
=======================================================================
*/
struct ogg_chain_s
{
	void		*ptr;
	struct alloc_chain	*next;
};

typedef struct oggpack_buffer_s
{
	long		endbyte;
	int		endbit;
	byte		*buffer;
	byte		*ptr;
	long		storage;
} oggpack_buffer_t;

typedef struct ogg_sync_state_s
{
	byte		*data;
	int		storage;
	int		fill;
	int		returned;
	int		unsynced;
	int		headerbytes;
	int		bodybytes;
} ogg_sync_state_t;

typedef struct ogg_stream_state_s
{
	byte		*body_data;
	long		body_storage;
	long		body_fill;
	long		body_returned;
	int		*lacing_vals;
	ogg_int64_t	*granule_vals;
	long		lacing_storage;
	long		lacing_fill;
	long		lacing_packet;
	long		lacing_returned;
	byte		header[282];
	int		header_fill;
	int		e_o_s;
	int		b_o_s;
	long		serialno;
	long		pageno;
	ogg_int64_t	packetno;
	ogg_int64_t	granulepos;
} ogg_stream_state_t;

/*
=======================================================================
		VORBIS FILE DEFINITION
=======================================================================
*/
typedef struct vorbis_info_s
{
	int		version;
	int		channels;
	long		rate;
	long		bitrate_upper;
	long		bitrate_nominal;
	long		bitrate_lower;
	long		bitrate_window;
	void		*codec_setup;
} vorbis_info_t;

typedef struct vorbis_comment_s
{
	char		**user_comments;
	int		*comment_lengths;
	int		comments;
	char		*vendor;
} vorbis_comment_t;

typedef struct vorbis_dsp_state_s
{
	int		analysisp;
	vorbis_info_t	*vi;
	float		**pcm;
	float		**pcmret;
	int		pcm_storage;
	int		pcm_current;
	int		pcm_returned;
	int		preextrapolate;
	int		eofflag;
	long		lW;
	long		W;
	long		nW;
	long		centerW;
	ogg_int64_t	granulepos;
	ogg_int64_t	sequence;
	ogg_int64_t	glue_bits;
	ogg_int64_t	time_bits;
	ogg_int64_t	floor_bits;
	ogg_int64_t	res_bits;
	void		*backend_state;
} vorbis_dsp_state_t;

typedef struct vorbis_block_s
{
	float		**pcm;
	oggpack_buffer_t	opb;
	long		lW;
	long		W;
	long		nW;
	int		pcmend;
	int		mode;
	int		eofflag;
	ogg_int64_t	granulepos;
	ogg_int64_t	sequence;
	vorbis_dsp_state_t	*vd;
	void		*localstore;
	long		localtop;
	long		localalloc;
	long		totaluse;
	struct ogg_chain_s	*reap;
	long		glue_bits;
	long		time_bits;
	long		floor_bits;
	long		res_bits;
	void		*internal;
} vorbis_block_t;

typedef struct ov_callbacks_s
{
	size_t (*read_func)( void *ptr, size_t size, size_t nmemb, void *datasource );
	int (*seek_func)( void *datasource, ogg_int64_t offset, int whence );
	int (*close_func)( void *datasource );
	long (*tell_func)( void *datasource );
} ov_callbacks_t;

typedef struct vorbisfile_s
{
	void		*datasource;
	int		seekable;
	ogg_int64_t	offset;
	ogg_int64_t	end;
	ogg_sync_state_t	oy;
	int		links;
	ogg_int64_t	*offsets;
	ogg_int64_t	*dataoffsets;
	long		*serialnos;
	ogg_int64_t	*pcmlengths;
	vorbis_info_t	*vi;
	vorbis_comment_t	*vc;
	ogg_int64_t	pcm_offset;
	int		ready_state;
	long		current_serialno;
	int		current_link;
	double		bittrack;
	double		samptrack;
	ogg_stream_state_t	os;
	vorbis_dsp_state_t	vd;
	vorbis_block_t	vb;
	ov_callbacks_t	callbacks;

} vorbisfile_t;

// libvorbis exports
int ov_open_callbacks( void *datasource, vorbisfile_t *vf, char *initial, long ibytes, ov_callbacks_t callbacks );
long ov_read( vorbisfile_t *vf, char *buffer, int length, int bigendianp, int word, int sgned, int *bitstream );
char *vorbis_comment_query( vorbis_comment_t *vc, char *tag, int count );
vorbis_comment_t *ov_comment( vorbisfile_t *vf, int link );
ogg_int64_t ov_pcm_total( vorbisfile_t *vf, int i );
vorbis_info_t *ov_info( vorbisfile_t *vf, int link );
int ov_raw_seek( vorbisfile_t *vf, ogg_int64_t pos );
ogg_int64_t ov_raw_tell( vorbisfile_t *vf );
int ov_clear( vorbisfile_t *vf);

#endif//S_STREAM_H