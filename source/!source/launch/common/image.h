//=======================================================================
//			Copyright XashXT Group 2007 ©
//			image.h - tga, pcx image headers
//=======================================================================

#ifndef IMAGE_H
#define IMAGE_H

extern int image_width, image_height;
extern byte image_num_layers;	// num layers in
extern byte image_num_mips;	// build mipmaps
extern uint image_type;	// main type switcher
extern uint image_flags;	// additional image flags
extern byte image_bits_count;	// bits per RGBA
extern size_t image_size;	// image rgba size
extern byte *image_palette;	// palette pointer
extern byte *image_rgba;	// image pointer (see image_type for details)
extern uint image_ptr;	// common moveable pointer

// image lib utilites
void Image_RoundDimensions(int *scaled_width, int *scaled_height);
byte *Image_Resample(uint *in, int inwidth, int inheight, int outwidth, int outheight, int in_type);

#endif//IMAGE_H