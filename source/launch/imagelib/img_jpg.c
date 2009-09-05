//=======================================================================
//			Copyright XashXT Group 2009 ©
//			img_jpg.c - jpg format load & save
//=======================================================================

#include <setjmp.h>
#include "imagelib.h"

// jboolean is unsigned char instead of int on Win32
#ifdef WIN32
typedef byte	jboolean;
#else
typedef int	jboolean;
#endif

#define JPEG_LIB_VERSION		62	// version 6b
#define JPEG_OUTPUT_BUF_SIZE		4096

typedef void *j_common_ptr;
typedef struct jpeg_compress_struct *j_compress_ptr;
typedef struct jpeg_decompress_struct *j_decompress_ptr;

typedef enum
{
	JCS_UNKNOWN,
	JCS_GRAYSCALE,
	JCS_RGB,
	JCS_YCbCr,
	JCS_CMYK,
	JCS_YCCK
} J_COLOR_SPACE;
typedef enum { JPEG_DUMMY1 } J_DCT_METHOD;
typedef enum { JPEG_DUMMY2 } J_DITHER_MODE;
typedef uint JDIMENSION;

#define JPOOL_PERMANENT	0	// lasts until master record is destroyed
#define JPOOL_IMAGE		1	// lasts until done with image/datastream
#define JPEG_EOI		0xD9	// EOI marker code
#define JMSG_STR_PARM_MAX	80
#define DCTSIZE2		64
#define NUM_QUANT_TBLS	4
#define NUM_HUFF_TBLS	4
#define NUM_ARITH_TBLS	16
#define MAX_COMPS_IN_SCAN	4
#define C_MAX_BLOCKS_IN_MCU	10
#define D_MAX_BLOCKS_IN_MCU	10

struct jpeg_memory_mgr
{
	void*		(*alloc_small)( j_common_ptr cinfo, int pool_id, size_t sizeofobject );
	void		(*alloc_large)( void );
	void		(*alloc_sarray)( void );
	void		(*alloc_barray)( void );
	void		(*request_virt_sarray)( void );
	void		(*request_virt_barray)( void );
	void		(*realize_virt_arrays)( void );
	void		(*access_virt_sarray)( void );
	void		(*access_virt_barray)( void );
	void		(*free_pool)( void );
	void		(*self_destruct)( void );
	long		max_memory_to_use;
	long		max_alloc_chunk;
};

struct jpeg_error_mgr
{
	void		(*error_exit)( j_common_ptr cinfo );
	void		(*emit_message)( j_common_ptr cinfo, int msg_level );
	void		(*output_message)( j_common_ptr cinfo );
	void		(*format_message)( j_common_ptr cinfo, char *buffer );
	void		(*reset_error_mgr)( j_common_ptr cinfo );
	int		msg_code;

	union
	{
		int	i[8];
		char	s[JMSG_STR_PARM_MAX];
	} msg_parm;

	int		trace_level;
	long		num_warnings;
	const char *const	*jpeg_message_table;
	int		last_jpeg_message;
	const char *const	*addon_message_table;
	int		first_addon_message;
	int		last_addon_message;
};

struct jpeg_source_mgr
{
	const byte	*next_input_byte;
	size_t		bytes_in_buffer;

	void		(*init_source)( j_decompress_ptr cinfo );
	jboolean		(*fill_input_buffer)( j_decompress_ptr cinfo );
	void		(*skip_input_data)( j_decompress_ptr cinfo, long num_bytes );
	jboolean		(*resync_to_restart)( j_decompress_ptr cinfo, int desired );
	void		(*term_source)( j_decompress_ptr cinfo );
};

typedef struct
{
	// these values are fixed over the whole image.
	// for compression, they must be supplied by parameter setup;
	// for decompression, they are read from the SOF marker.
	int		component_id;             // identifier for this component (0..255)
	int		component_index;          // its index in SOF or cinfo->comp_info[]
	int		h_samp_factor;            // horizontal sampling factor (1..4)
	int		v_samp_factor;            // vertical sampling factor (1..4)
	int		quant_tbl_no;             // quantization table selector (0..3)

	// these values may vary between scans.
	// for compression, they must be supplied by parameter setup;
	// for decompression, they are read from the SOS marker.
	// the decompressor output side may not use these variables.
	int		dc_tbl_no;                // DC entropy table selector (0..3)
	int		ac_tbl_no;                // AC entropy table selector (0..3)
  
	// Remaining fields should be treated as private by applications.
  
	// these values are computed during compression or decompression startup:
	// component's size in DCT blocks.
	// any dummy blocks added to complete an MCU are not counted; 
	// therefore these values do not depend on whether a scan is interleaved or not.
	JDIMENSION	width_in_blocks;
	JDIMENSION	height_in_blocks;

	// size of a DCT block in samples. always DCTSIZE for compression.
	// for decompression this is the size of the output from one DCT block,
	// reflecting any scaling we choose to apply during the IDCT step.
	// values of 1,2,4,8 are likely to be supported.  note that different
	// components may receive different IDCT scalings.

	int		DCT_scaled_size;
	// The downsampled dimensions are the component's actual, unpadded number
	// of samples at the main buffer (preprocessing/compression interface), thus
	// downsampled_width = ceil(image_width * Hi/Hmax)
	// and similarly for height. for decompression, IDCT scaling is included, so
	// downsampled_width = ceil(image_width * Hi/Hmax * DCT_scaled_size/DCTSIZE)
	JDIMENSION	downsampled_width;  // actual width in samples
	JDIMENSION	downsampled_height; // actual height in samples

	// This flag is used only for decompression.  In cases where some of the
	// components will be ignored (eg grayscale output from YCbCr image),
	// we can skip most computations for the unused components.
	jboolean		component_needed;	// do we need the value of this component?

	// these values are computed before starting a scan of the component.
	// the decompressor output side may not use these variables.
	int		MCU_width;	// number of blocks per MCU, horizontally
	int		MCU_height;	// number of blocks per MCU, vertically
	int		MCU_blocks;	// MCU_width * MCU_height
	int		MCU_sample_width;	// MCU width in samples, MCU_width*DCT_scaled_size
	int		last_col_width;	// # of non-dummy blocks across in last MCU
	int		last_row_height;	// # of non-dummy blocks down in last MCU

	// saved quantization table for component; NULL if none yet saved.
	// see jdinput.c comments about the need for this information.
	// this field is currently used only for decompression.
	void		*quant_table;

	// private per-component storage for DCT or IDCT subsystem.
	void		*dct_table;
} jpeg_component_info;

struct jpeg_decompress_struct
{
	struct jpeg_error_mgr	*err;		// USED
	struct jpeg_memory_mgr	*mem;		// USED

	void			*progress;
	void			*client_data;
	jboolean			is_decompressor;
	int			global_state;

	struct jpeg_source_mgr	*src;		// USED
	JDIMENSION		image_width;	// USED
	JDIMENSION		image_height;	// USED

	int			num_components;
	J_COLOR_SPACE		jpeg_color_space;
	J_COLOR_SPACE		out_color_space;
	uint			scale_num;
	uint			scale_denom;
	double			output_gamma;
	jboolean			buffered_image;
	jboolean			raw_data_out;
	J_DCT_METHOD		dct_method;
	jboolean			do_fancy_upsampling;
	jboolean			do_block_smoothing;
	jboolean			quantize_colors;
	J_DITHER_MODE		dither_mode;
	jboolean			two_pass_quantize;
	int			desired_number_of_colors;
	jboolean			enable_1pass_quant;
	jboolean			enable_external_quant;
	jboolean			enable_2pass_quant;
	JDIMENSION		output_width;	// USED
	JDIMENSION		output_height;	// USED
	int			out_color_components;
	int			output_components;	// USED
	int			rec_outbuf_height;
	int			actual_number_of_colors;
	void			*colormap;
	JDIMENSION		output_scanline;	// USED
	int			input_scan_number;
	JDIMENSION		input_iMCU_row;
	int			output_scan_number;
	JDIMENSION		output_iMCU_row;
	int			(*coef_bits)[DCTSIZE2];
	void			*quant_tbl_ptrs[NUM_QUANT_TBLS];
	void			*dc_huff_tbl_ptrs[NUM_HUFF_TBLS];
	void			*ac_huff_tbl_ptrs[NUM_HUFF_TBLS];
	int			data_precision;
	jpeg_component_info		*comp_info;
	jboolean			progressive_mode;
	jboolean			arith_code;
	byte			arith_dc_L[NUM_ARITH_TBLS];
	byte			arith_dc_U[NUM_ARITH_TBLS];
	byte			arith_ac_K[NUM_ARITH_TBLS];
	uint			restart_interval;
	jboolean			saw_JFIF_marker;
	byte			JFIF_major_version;
	byte			JFIF_minor_version;
	byte			density_unit;
	word			X_density;
	word			Y_density;
	jboolean			saw_Adobe_marker;
	byte			Adobe_transform;
	jboolean			CCIR601_sampling;
	void			*marker_list;
	int			max_h_samp_factor;
	int			max_v_samp_factor;
	int			min_DCT_scaled_size;
	JDIMENSION		total_iMCU_rows;
	void			*sample_range_limit;
	int			comps_in_scan;
	jpeg_component_info		*cur_comp_info[MAX_COMPS_IN_SCAN];
	JDIMENSION		MCUs_per_row;
	JDIMENSION		MCU_rows_in_scan;
	int			blocks_in_MCU;
	int			MCU_membership[D_MAX_BLOCKS_IN_MCU];
	int			Ss, Se, Ah, Al;
	int			unread_marker;
	void			*master;
	void			*main;
	void			*coef;
	void			*post;
	void			*inputctl;
	void			*marker;
	void			*entropy;
	void			*idct;
	void			*upsample;
	void			*cconvert;
	void			*cquantize;
};

struct jpeg_compress_struct
{
	struct jpeg_error_mgr	*err;
	struct jpeg_memory_mgr	*mem;
	void			*progress;
	void			*client_data;
	jboolean			is_decompressor;
	int			global_state;

	void			*dest;
	JDIMENSION		image_width;
	JDIMENSION		image_height;
	int			input_components;
	J_COLOR_SPACE		in_color_space;
	double			input_gamma;
	int			data_precision;
	int			num_components;
	J_COLOR_SPACE		jpeg_color_space;
	jpeg_component_info		*comp_info;
	void			*quant_tbl_ptrs[NUM_QUANT_TBLS];
	void			*dc_huff_tbl_ptrs[NUM_HUFF_TBLS];
	void			*ac_huff_tbl_ptrs[NUM_HUFF_TBLS];
	byte			arith_dc_L[NUM_ARITH_TBLS];
	byte			arith_dc_U[NUM_ARITH_TBLS];
	byte			arith_ac_K[NUM_ARITH_TBLS];
	int			num_scans;
	const void		*scan_info;
	jboolean			raw_data_in;
	jboolean			arith_code;
	jboolean			optimize_coding;
	jboolean			CCIR601_sampling;
	int			smoothing_factor;
	J_DCT_METHOD		dct_method;
	uint			restart_interval;
	int			restart_in_rows;
	jboolean			write_JFIF_header;
	byte			JFIF_major_version;
	byte			JFIF_minor_version;
	byte			density_unit;
	word			X_density;
	word			Y_density;
	jboolean			write_Adobe_marker;
	JDIMENSION		next_scanline;
	jboolean			progressive_mode;
	int			max_h_samp_factor;
	int			max_v_samp_factor;
	JDIMENSION		total_iMCU_rows;
	int			comps_in_scan;
	jpeg_component_info		*cur_comp_info[MAX_COMPS_IN_SCAN];
	JDIMENSION		MCUs_per_row;
	JDIMENSION		MCU_rows_in_scan;
	int			blocks_in_MCU;
	int			MCU_membership[C_MAX_BLOCKS_IN_MCU];
	int			Ss, Se, Ah, Al;
	void			*master;
	void			*main;
	void			*prep;
	void			*coef;
	void			*marker;
	void			*cconvert;
	void			*downsample;
	void			*fdct;
	void			*entropy;
	void			*script_space;
	int			script_space_size;
};

struct jpeg_destination_mgr
{
	byte	*next_output_byte;
	size_t	free_in_buffer;

	void	(*init_destination)( j_compress_ptr cinfo );
	jboolean	(*empty_output_buffer)( j_compress_ptr cinfo );
	void	(*term_destination)( j_compress_ptr cinfo );
};

// Functions exported from libjpeg
#define jpeg_create_compress( cinfo ) \
	jpeg_CreateCompress((cinfo), JPEG_LIB_VERSION, (size_t) sizeof( struct jpeg_compress_struct ))
#define jpeg_create_decompress( cinfo ) \
	jpeg_CreateDecompress((cinfo), JPEG_LIB_VERSION, (size_t) sizeof( struct jpeg_decompress_struct ))

extern void jpeg_CreateCompress( j_compress_ptr cinfo, int version, size_t structsize );
extern void jpeg_CreateDecompress( j_decompress_ptr cinfo, int version, size_t structsize );
extern void jpeg_destroy_compress( j_compress_ptr cinfo );
extern void jpeg_destroy_decompress( j_decompress_ptr cinfo );
extern void jpeg_finish_compress( j_compress_ptr cinfo );
extern jboolean jpeg_finish_decompress( j_decompress_ptr cinfo );
extern jboolean jpeg_resync_to_restart( j_decompress_ptr cinfo, int desired );
extern int jpeg_read_header( j_decompress_ptr cinfo, jboolean require_image );
extern JDIMENSION jpeg_read_scanlines( j_decompress_ptr cinfo, byte** scanlines, JDIMENSION max_lines );
extern void jpeg_set_defaults( j_compress_ptr cinfo );
extern void jpeg_set_quality( j_compress_ptr cinfo, int quality, jboolean force_baseline );
extern jboolean jpeg_start_compress( j_compress_ptr cinfo, jboolean write_all_tables );
extern jboolean jpeg_start_decompress( j_decompress_ptr cinfo );
extern struct jpeg_error_mgr* jpeg_std_error( struct jpeg_error_mgr *err );
extern JDIMENSION jpeg_write_scanlines( j_compress_ptr cinfo, byte** scanlines, JDIMENSION num_lines );

static byte	jpeg_eoi_marker [2] = {0xFF, JPEG_EOI};
static jmp_buf	error_in_jpeg;
static bool	jpeg_toolarge;

// our own output manager for JPEG compression
typedef struct
{
	struct jpeg_destination_mgr	pub;

	file_t			*outfile;
	byte			*buffer;
	size_t			bufsize; // used if outfile is NULL
} dest_mgr;

typedef dest_mgr			*dest_ptr;

/*
=================================================================

	JPEG decompression

=================================================================
*/
static void JPEG_Noop( j_decompress_ptr cinfo )
{
}

static jboolean JPEG_FillInputBuffer( j_decompress_ptr cinfo )
{
	// insert a fake EOI marker
	cinfo->src->next_input_byte = jpeg_eoi_marker;
	cinfo->src->bytes_in_buffer = 2;
	return TRUE;
}

static void JPEG_SkipInputData( j_decompress_ptr cinfo, long num_bytes )
{
	if( cinfo->src->bytes_in_buffer <= (dword)num_bytes )
	{
		cinfo->src->bytes_in_buffer = 0;
		return;
	}

	cinfo->src->next_input_byte += num_bytes;
	cinfo->src->bytes_in_buffer -= num_bytes;
}

static void JPEG_MemSrc( j_decompress_ptr cinfo, const byte *buffer, size_t filesize )
{
	cinfo->src = (struct jpeg_source_mgr *)cinfo->mem->alloc_small((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof( struct jpeg_source_mgr ));

	cinfo->src->next_input_byte = buffer;
	cinfo->src->bytes_in_buffer = filesize;

	cinfo->src->init_source = JPEG_Noop;
	cinfo->src->fill_input_buffer = JPEG_FillInputBuffer;
	cinfo->src->skip_input_data = JPEG_SkipInputData;
	cinfo->src->resync_to_restart = jpeg_resync_to_restart;	// use the default method
	cinfo->src->term_source = JPEG_Noop;
}

static void JPEG_ErrorExit( j_common_ptr cinfo )
{
	((struct jpeg_decompress_struct*)cinfo)->err->output_message( cinfo );
	longjmp( error_in_jpeg, 1 );
}

/*
=============
Image_LoadJPG
=============
*/
bool Image_LoadJPG( const char *name, const byte *buffer, size_t filesize )
{
	struct jpeg_decompress_struct	cinfo;
	struct jpeg_error_mgr	jerr;
	byte			*image_buffer = NULL;
	byte			*scanline = NULL;
	uint			line;

	cinfo.err = jpeg_std_error( &jerr );
	jpeg_create_decompress( &cinfo );
	if( setjmp( error_in_jpeg )) goto error_caught;
	cinfo.err = jpeg_std_error( &jerr );
	cinfo.err->error_exit = JPEG_ErrorExit;
	JPEG_MemSrc( &cinfo, buffer, filesize );
	jpeg_read_header( &cinfo, TRUE );
	jpeg_start_decompress( &cinfo );

	image.width = cinfo.image_width;
	image.height = cinfo.image_height;
	if(!Image_ValidSize( name )) return false;

	image.size = image.width * image.height * 4;
	image.rgba = (byte *)Mem_Alloc( Sys.imagepool, image.size );
	scanline = (byte *)Mem_Alloc( Sys.imagepool, image.width * cinfo.output_components );

	// decompress the image, line by line
	line = 0;
	while( cinfo.output_scanline < cinfo.output_height )
	{
		byte	*buffer_ptr;
		int	ind;

		jpeg_read_scanlines( &cinfo, &scanline, 1 );

		// convert the image to RGBA
		switch( cinfo.output_components )
		{
		case 3:// RGB images
			buffer_ptr = &image.rgba[image.width * line * 4];
			for( ind = 0; ind < image.width * 3; ind += 3, buffer_ptr += 4 )
			{
				buffer_ptr[0] = scanline[ind+0];
				buffer_ptr[1] = scanline[ind+1];
				buffer_ptr[2] = scanline[ind+2];
				buffer_ptr[3] = 0xFF;
			}
			break;
		case 1:
		default:	// greyscale images (default to it, just in case)
			buffer_ptr = &image.rgba[image.width * line * 4];
			for( ind = 0; ind < image.width; ind++, buffer_ptr += 4 )
			{
				buffer_ptr[0] = scanline[ind];
				buffer_ptr[1] = scanline[ind];
				buffer_ptr[2] = scanline[ind];
				buffer_ptr[3] = 255;
			}
		}
		line++;
	}
	Mem_Free( scanline );

	if( cinfo.output_components == 3 )
		image.flags |= IMAGE_HAS_COLOR;

	jpeg_finish_decompress( &cinfo );
	jpeg_destroy_decompress( &cinfo );

	image.type = PF_RGBA_32;
	image.depth = 1;
	image.num_mips = 1;

	return true;
error_caught:
	if( scanline ) Mem_Free( scanline );
	if( image.rgba ) Mem_Free( image.rgba );
	jpeg_destroy_decompress( &cinfo );

	return false;
}

/*
=================================================================

  JPEG compression

=================================================================
*/
static void JPEG_InitDestination( j_compress_ptr cinfo )
{
	dest_ptr	dest = (dest_ptr)cinfo->dest;

	dest->buffer = (byte*)cinfo->mem->alloc_small ((j_common_ptr) cinfo, JPOOL_IMAGE, JPEG_OUTPUT_BUF_SIZE * sizeof( byte ));
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = JPEG_OUTPUT_BUF_SIZE;
}

static jboolean JPEG_EmptyOutputBuffer( j_compress_ptr cinfo )
{
	dest_ptr	dest = (dest_ptr)cinfo->dest;

	if( FS_Write( dest->outfile, dest->buffer, JPEG_OUTPUT_BUF_SIZE ) != (size_t)JPEG_OUTPUT_BUF_SIZE )
		longjmp( error_in_jpeg, 1 );

	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = JPEG_OUTPUT_BUF_SIZE;
	return true;
}

static void JPEG_TermDestination( j_compress_ptr cinfo )
{
	dest_ptr	dest = (dest_ptr)cinfo->dest;
	size_t	datacount = JPEG_OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

	// write any data remaining in the buffer
	if( datacount > 0 )
		if( FS_Write( dest->outfile, dest->buffer, datacount ) != (fs_offset_t)datacount )
			longjmp( error_in_jpeg, 1 );
}

static void JPEG_FileDest( j_compress_ptr cinfo, file_t *outfile )
{
	dest_ptr	dest;

	// first time for this JPEG object?
	if( cinfo->dest == NULL )
		cinfo->dest = (struct jpeg_destination_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof( dest_mgr ));

	dest = (dest_ptr)cinfo->dest;
	dest->pub.init_destination = JPEG_InitDestination;
	dest->pub.empty_output_buffer = JPEG_EmptyOutputBuffer;
	dest->pub.term_destination = JPEG_TermDestination;
	dest->outfile = outfile;
}

/*
=============
Image_SaveJPG
=============
*/
bool Image_SaveJPG( const char *name, rgbdata_t *pix )
{
	struct jpeg_compress_struct	cinfo;
	struct jpeg_error_mgr	jerr;
	byte			*scanline;
	uint			linesize;
	file_t			*file;

	if( FS_FileExists( name ) && !(image.cmd_flags & IL_ALLOW_OVERWRITE ))
		return false; // already existed

/*
	if( pix->flags & IMAGE_HAS_ALPHA )
	{
		string	tempname;

		// save alpha if any
		com.strncpy( tempname, name, sizeof( tempname ));
		FS_StripExtension( tempname );
		FS_DefaultExtension( tempname, ".png" );
		return FS_SaveImage( tempname, pix );
	}
*/
	// Open the file
	file = FS_Open( name, "wb" );
	if( !file ) return false;

	if( setjmp( error_in_jpeg ))
		goto error_caught;
	cinfo.err = jpeg_std_error( &jerr );
	cinfo.err->error_exit = JPEG_ErrorExit;

	jpeg_create_compress( &cinfo );
	JPEG_FileDest( &cinfo, file );

	// Set the parameters for compression
	cinfo.image_width = pix->width;
	cinfo.image_height = pix->height;

	// get image description
	switch( pix->type )
	{
	case PF_RGB_24: cinfo.input_components = 3; break;
	case PF_RGBA_32: cinfo.input_components = 4; break;	
	default:
		MsgDev( D_ERROR, "Image_SaveJPG: unsupported image type %s\n", PFDesc[pix->type].name );
		goto error_caught;
	}

	if( pix->flags & IMAGE_HAS_COLOR )
		cinfo.in_color_space = JCS_RGB;
	else cinfo.in_color_space = JCS_GRAYSCALE;

	jpeg_set_defaults( &cinfo );
	jpeg_set_quality( &cinfo, (jpg_quality->integer * 10), TRUE );

	// turn off subsampling (to make text look better)
	cinfo.optimize_coding = 1;
	cinfo.comp_info[0].h_samp_factor = 1;
	cinfo.comp_info[0].v_samp_factor = 1;
	cinfo.comp_info[1].h_samp_factor = 1;
	cinfo.comp_info[1].v_samp_factor = 1;
	cinfo.comp_info[2].h_samp_factor = 1;
	cinfo.comp_info[2].v_samp_factor = 1;

	jpeg_start_compress( &cinfo, true );

	// compress each scanline
	linesize = cinfo.image_width * cinfo.input_components;
	while( cinfo.next_scanline < cinfo.image_height )
	{
		scanline = &pix->buffer[cinfo.next_scanline * linesize];
		jpeg_write_scanlines( &cinfo, &scanline, 1 );
	}

	jpeg_finish_compress( &cinfo );
	jpeg_destroy_compress( &cinfo );

	FS_Close( file );
	return true;

error_caught:
	jpeg_destroy_compress( &cinfo );
	FS_Close( file );
	return false;
}