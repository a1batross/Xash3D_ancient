//=======================================================================
//			Copyright XashXT Group 2007 ©
//			image.h - tga, pcx image headers
//=======================================================================

#ifndef IMAGE_H
#define IMAGE_H

/*
========================================================================

.BMP image format

========================================================================
*/
typedef struct
{
	char id[2];		//bmfh.bfType
	dword fileSize;		//bmfh.bfSize
	dword reserved0;		//bmfh.bfReserved1 + bmfh.bfReserved2
	dword bitmapDataOffset;	//bmfh.bfOffBits
	dword bitmapHeaderSize;	//bmih.biSize
	dword width;		//bmih.biWidth
	dword height;		//bmih.biHeight
	word planes;		//bmih.biPlanes
	word bitsPerPixel;		//bmih.biBitCount
	dword compression;		//bmih.biCompression
	dword bitmapDataSize;	//bmih.biSizeImage
	dword hRes;		//bmih.biXPelsPerMeter
	dword vRes;		//bmih.biYPelsPerMeter
	dword colors;		//bmih.biClrUsed
	dword importantColors;	//bmih.biClrImportant
	byte palette[256][4];	//RGBQUAD palette
} bmp_t;

/*
========================================================================

.PCX image format

========================================================================
*/
typedef struct
{
	char	manufacturer;
	char	version;
	char	encoding;
	char	bits_per_pixel;
	word	xmin,ymin,xmax,ymax;
	word	hres,vres;
	byte	palette[48];
	char	reserved;
	char	color_planes;
	word	bytes_per_line;
	word	palette_type;
	char	filler[58];
	byte	data; // unbounded
} pcx_t;


/*
========================================================================

.TGA image format

========================================================================
*/
typedef struct
{
	byte	id_length;
	byte	colormap_type;
	byte	image_type;
	word	colormap_index;
	word	colormap_length;
	byte	colormap_size;
	word	x_origin;
	word	y_origin;
	word	width;
	word	height;
	byte	pixel_size;
	byte	attributes;
} tga_t;
/*
========================================================================

.PNG image format

========================================================================
*/
#define PNG_LIBPNG_VER_STRING		"1.2.4"

#define PNG_COLOR_MASK_PALETTE	1
#define PNG_COLOR_MASK_COLOR		2
#define PNG_COLOR_MASK_ALPHA		4
#define PNG_COLOR_TYPE_GRAY		0
#define PNG_COLOR_TYPE_PALETTE	(PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_PALETTE)
#define PNG_COLOR_TYPE_RGB		(PNG_COLOR_MASK_COLOR)
#define PNG_COLOR_TYPE_RGB_ALPHA	(PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_ALPHA)
#define PNG_COLOR_TYPE_GRAY_ALPHA	(PNG_COLOR_MASK_ALPHA)

#define PNG_COLOR_TYPE_RGBA		PNG_COLOR_TYPE_RGB_ALPHA
#define PNG_COLOR_TYPE_GA		PNG_COLOR_TYPE_GRAY_ALPHA
#define PNG_INFO_tRNS		0x0010

extern void  png_set_sig_bytes	(void*, int);
extern int   png_sig_cmp		(const byte*, size_t, size_t);
extern void* png_create_read_struct	(const char*, void*, void*, void*);
extern void* png_create_info_struct	(void*);
extern void  png_read_info		(void*, void*);
extern void  png_set_expand		(void*);
extern void  png_set_gray_1_2_4_to_8	(void*);
extern void  png_set_palette_to_rgb	(void*);
extern void  png_set_tRNS_to_alpha	(void*);
extern void  png_set_gray_to_rgb	(void*);
extern void  png_set_filler		(void*, unsigned int, int);
extern void  png_read_update_info	(void*, void*);
extern void  png_read_image		(void*, unsigned char**);
extern void  png_read_end		(void*, void*);
extern void  png_destroy_read_struct	(void**, void**, void**);
extern void  png_set_read_fn		(void*, void*, void*);
extern dword png_get_valid		(void*, void*, unsigned int);
extern dword png_get_rowbytes		(void*, void*);
extern byte  png_get_channels		(void*, void*);
extern byte  png_get_bit_depth	(void*, void*);
extern dword png_get_IHDR		(void*, void*, dword*, dword*, int *, int *, int *, int *, int *);
extern char* png_get_libpng_ver	(void*);
extern void  png_set_strip_16		(void*);

static struct
{
	const byte	*tmpBuf;
	int		tmpBuflength;
	int		tmpi;
	//int		FBgColor;
	//int		FTransparent;
	dword		FRowBytes;
	//double		FGamma;
	//double		FScreenGamma;
	byte		**FRowPtrs;
	byte		*Data;
	//char		*Title;
	//char		*Author;
	//char		*Description;
	int		BitDepth;
	int		BytesPerPixel;
	int		ColorType;
	unsigned int	Height;
	unsigned int	Width;
	int		Interlace;
	int		Compression;
	int		Filter;
	//double		LastModified;
	//int		Transparent;
}png_buf;

/*
========================================================================

.JPG image format

========================================================================
*/
#define JPEG_LIB_VERSION  62  // Version 6b

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
}J_COLOR_SPACE;

typedef enum {JPEG_DUMMY1} J_DCT_METHOD;
typedef enum {JPEG_DUMMY2} J_DITHER_MODE;
typedef dword JDIMENSION;

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
	void* (*alloc_small) (j_common_ptr cinfo, int pool_id, size_t sizeofobject);
	void (*alloc_large) ();
	void (*alloc_sarray) ();
	void (*alloc_barray) ();
	void (*request_virt_sarray) ();
	void (*request_virt_barray) ();
	void (*realize_virt_arrays) ();
	void (*access_virt_sarray) ();
	void (*access_virt_barray) ();
	void (*free_pool) ();
	void (*self_destruct) ();
	long max_memory_to_use;
	long max_alloc_chunk;
};

struct jpeg_error_mgr
{
	void (*error_exit) (j_common_ptr cinfo);
	void (*emit_message) (j_common_ptr cinfo, int msg_level);
	void (*output_message) (j_common_ptr cinfo);
	void (*format_message) (j_common_ptr cinfo, char * buffer);
	void (*reset_error_mgr) (j_common_ptr cinfo);
	int msg_code;
	union
	{
		int i[8];
		char s[JMSG_STR_PARM_MAX];
	}msg_parm;

	int trace_level;
	long num_warnings;
	const char * const * jpeg_message_table;
	int last_jpeg_message;
	const char * const * addon_message_table;
	int first_addon_message;
	int last_addon_message;
};

struct jpeg_source_mgr
{
	const unsigned char *next_input_byte;
	size_t bytes_in_buffer;

	void (*init_source) (j_decompress_ptr cinfo);
	jboolean (*fill_input_buffer) (j_decompress_ptr cinfo);
	void (*skip_input_data) (j_decompress_ptr cinfo, long num_bytes);
	jboolean (*resync_to_restart) (j_decompress_ptr cinfo, int desired);
	void (*term_source) (j_decompress_ptr cinfo);
};

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
	dword			scale_num, scale_denom;
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
	JDIMENSION		output_width;
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
	void			*comp_info;
	jboolean			progressive_mode;
	jboolean			arith_code;
	byte			arith_dc_L[NUM_ARITH_TBLS];
	byte			arith_dc_U[NUM_ARITH_TBLS];
	byte			arith_ac_K[NUM_ARITH_TBLS];
	dword			restart_interval;
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
	void			*cur_comp_info[MAX_COMPS_IN_SCAN];
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
	void			*comp_info;
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

	dword			restart_interval;
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
	void			*cur_comp_info[MAX_COMPS_IN_SCAN];
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
	byte* next_output_byte;
	size_t free_in_buffer;

	void (*init_destination) (j_compress_ptr cinfo);
	jboolean (*empty_output_buffer) (j_compress_ptr cinfo);
	void (*term_destination) (j_compress_ptr cinfo);
};


#define jpeg_create_compress(cinfo) jpeg_CreateCompress((cinfo), JPEG_LIB_VERSION, (size_t) sizeof(struct jpeg_compress_struct))
#define jpeg_create_decompress(cinfo) jpeg_CreateDecompress((cinfo), JPEG_LIB_VERSION, (size_t) sizeof(struct jpeg_decompress_struct))

extern void jpeg_CreateCompress (j_compress_ptr cinfo, int version, size_t structsize);
extern void jpeg_CreateDecompress (j_decompress_ptr cinfo, int version, size_t structsize);
extern void jpeg_destroy_compress (j_compress_ptr cinfo);
extern void jpeg_destroy_decompress (j_decompress_ptr cinfo);
extern void jpeg_finish_compress (j_compress_ptr cinfo);
extern jboolean jpeg_finish_decompress (j_decompress_ptr cinfo);
extern jboolean jpeg_resync_to_restart (j_decompress_ptr cinfo, int desired);
extern int jpeg_read_header (j_decompress_ptr cinfo, jboolean require_image);
extern JDIMENSION jpeg_read_scanlines (j_decompress_ptr cinfo, unsigned char** scanlines, JDIMENSION max_lines);
extern void jpeg_set_defaults (j_compress_ptr cinfo);
extern void jpeg_set_quality (j_compress_ptr cinfo, int quality, jboolean force_baseline);
extern jboolean jpeg_start_compress (j_compress_ptr cinfo, jboolean write_all_tables);
extern jboolean jpeg_start_decompress (j_decompress_ptr cinfo);
extern struct jpeg_error_mgr* jpeg_std_error (struct jpeg_error_mgr *err);
extern JDIMENSION jpeg_write_scanlines (j_compress_ptr cinfo, unsigned char** scanlines, JDIMENSION num_lines);

#endif//IMAGE_H