//=======================================================================
//			Copyright XashXT Group 2007 ©
//			r_texture.c - load & convert textures
//=======================================================================

#include "gl_local.h"

/*
=============================================================

  TEXTURES UPLOAD

=============================================================
*/

int ColorIndex[16] = { 0, 31, 47, 63, 79, 95, 111, 127, 143, 159, 175, 191, 199, 207, 223, 231 };
unsigned ColorPercent[16] = { 25, 51, 76, 102, 114, 127, 140, 153, 165, 178, 191, 204, 216, 229, 237, 247 };
image_t gltextures[MAX_GLTEXTURES];
int numgltextures;
byte intensitytable[256];
byte gammatable[256];
extern cvar_t *gl_picmip;
cvar_t *gl_maxsize;
byte *r_imagepool;
byte *imagebuffer;
int imagebufsize;
byte *uploadbuffer;
int uploadbufsize;
cvar_t *intensity;
unsigned	d_8to24table[256];

static byte palette_int[] =
{
#include "palette.h"
};

/*
===============
R_ImageList_f
===============
*/
void R_ImageList_f (void)
{
	int	i;
	image_t	*image;
	const char *palstrings[2] =
	{
		"RGB",
		"PAL"
	};

	Msg( "------------------\n");

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (image->texnum <= 0) continue;
		switch (image->type)
		{
		case it_skin: Msg( "Skin"); break;
		case it_sprite: Msg( "Spr "); break;
		case it_wall: Msg( "Wall"); break;
		case it_pic: Msg( "Pic "); break;
		case it_sky: Msg( "Sky "); break;
		default: Msg( "Sys "); break;
		}
		Msg( " %3i %3i %s: %s\n", image->width, image->height, palstrings[image->paletted], image->name);
	}
	Msg( "Total images count (not counting mipmaps): %i\n", numgltextures);
}

/*
===============
R_GetPalette
===============
*/
void R_GetPalette (void)
{
	unsigned	v;
	int	i, r, g, b;
	byte	*pal = palette_int;

	//used by particle system once only
	for (i=0 ; i<256 ; i++)
	{
		r = pal[i*3+0];
		g = pal[i*3+1];
		b = pal[i*3+2];
		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		d_8to24table[i] = LittleLong(v);
	}
	d_8to24table[255] &= LittleLong(0xffffff); // 255 is transparent
}

/*
===============
R_ShutdownTextures
===============
*/
void R_ShutdownTextures (void)
{
	int		i;
	image_t	*image;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		// free image_t slot
		if (!image->registration_sequence) continue;

		// free it
		qglDeleteTextures (1, &image->texnum);
		memset (image, 0, sizeof(*image));
	}
}

/*
===============
R_InitTextures
===============
*/
void R_InitTextures( void )
{
	int texsize, i, j;
          float g = vid_gamma->value;
	
	r_imagepool = Mem_AllocPool("Image Memory");
          gl_maxsize = ri.Cvar_Get ("gl_maxsize", "0", 0);
	
	qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &texsize);
	if (gl_maxsize->value > texsize)
		ri.Cvar_SetValue ("gl_maxsize", texsize);

	if(texsize < 2048) imagebufsize = 2048*2048*4;
	else imagebufsize = texsize * texsize * 4;
	uploadbufsize = texsize * texsize * 4;
	
	//create intermediate & upload image buffer
	imagebuffer = Mem_Alloc( r_imagepool, imagebufsize );
          uploadbuffer = Mem_Alloc( r_imagepool, uploadbufsize );

	registration_sequence = 1;

	// init intensity conversions
	intensity = ri.Cvar_Get ("intensity", "2", 0);
	if ( intensity->value <= 1 ) ri.Cvar_Set( "intensity", "1" );

	gl_state.inverse_intensity = 1 / intensity->value;

	R_GetPalette();

	for ( i = 0; i < 256; i++ )
	{
		if ( g == 1 ) gammatable[i] = i;
		else
		{
			float inf;

			inf = 255 * pow ( (i+0.5)/255.5 , g ) + 0.5;
			if (inf < 0) inf = 0;
			if (inf > 255) inf = 255;
			gammatable[i] = inf;
		}
	}
	for (i=0 ; i<256 ; i++)
	{
		j = i * intensity->value;
		if (j > 255) j = 255;
		intensitytable[i] = j;
	}
}

void R_ImageMipmap (byte *in, int width, int height)
{
	int		i, j;
	byte	*out;

	width <<=2;
	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=8, out+=4, in+=8)
		{
			out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
			out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
			out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
			out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
		}
	}
}

/*
================
R_ImageLightScale
================
*/
void R_ImageLightScale (unsigned *in, int inwidth, int inheight )
{
	int	i, c = inwidth * inheight;
	byte	*p = (byte *)in;

	for (i = 0; i < c; i++, p += 4)
	{
		p[0] = gammatable[p[0]];
		p[1] = gammatable[p[1]];
		p[2] = gammatable[p[2]];
	}
}

void R_RoundImageDimensions(int *scaled_width, int *scaled_height, bool mipmap)
{
	int width = *scaled_width;
	int height = *scaled_height;
	for (*scaled_width = 1; *scaled_width < width; *scaled_width<<=1);
	for (*scaled_height = 1; *scaled_height < height; *scaled_height<<=1);

	if (mipmap)
	{
		*scaled_width >>= (int)gl_picmip->value;
		*scaled_height >>= (int)gl_picmip->value;
	}

	if (gl_maxsize->value)
	{
		if (*scaled_width > gl_maxsize->value) *scaled_width = gl_maxsize->value;
		if (*scaled_height > gl_maxsize->value) *scaled_height = gl_maxsize->value;
	}

	if (*scaled_width < 1) *scaled_width = 1;
	if (*scaled_height < 1) *scaled_height = 1;
}

void R_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight)
{
	int	i, j;
	unsigned	*inrow;
	unsigned	frac, fracstep;

	fracstep = inwidth * 0x10000/outwidth;
	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(i*inheight/outheight);
		frac = outwidth*fracstep;
		j = outwidth - 1;
		while ((j+1) &3)
		{
			out[j] = inrow[frac>>16];
			frac -= fracstep;
			j--;
		}
		for (; j >= 0; j -= 4)
		{
			out[j+3] = inrow[frac>>16];
			frac -= fracstep;
			out[j+2] = inrow[frac>>16];
			frac -= fracstep;
			out[j+1] = inrow[frac>>16];
			frac -= fracstep;
			out[j+0] = inrow[frac>>16];
			frac -= fracstep;
		}
	}
}

/*
===============
R_LoadImage32
===============
*/
bool R_LoadImage32 ( unsigned *data, int width, int height, bool mipmap, int alpha)
{
	int miplevel = 0;
	int samples;
	unsigned	*scaled = (unsigned *)uploadbuffer;
	int scaled_width, scaled_height;
	
	scaled_width = width;
	scaled_height = height;
	R_RoundImageDimensions(&scaled_width, &scaled_height, mipmap);

	if (scaled_width * scaled_height > uploadbufsize/4)
	{
		Msg("R_LoadImage32: too big");
		return false;
          }
        
	samples = alpha ? gl_tex_alpha_format : gl_tex_solid_format;

	if (gl_config.sgis_generate_mipmap && mipmap)
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
	}

	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap || gl_config.sgis_generate_mipmap)
		{
			qglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			goto done;
		}
		memcpy (scaled, data, width*height*4);
	}
	else R_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);
          
	R_ImageLightScale(scaled, scaled_width, scaled_height );
	
	qglTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	if (mipmap && !gl_config.sgis_generate_mipmap)
	{
		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1)
		{
			R_ImageMipmap((byte *)scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1) scaled_width = 1;
			if (scaled_height < 1) scaled_height = 1;
			miplevel++;
			qglTexImage2D (GL_TEXTURE_2D, miplevel, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		}
	}
done:
	if (gl_config.sgis_generate_mipmap&&mipmap)
		qglTexParameterf(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_FALSE);

	if (mipmap)
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	return true;
}


/*
===============
R_LoadImage8
===============
*/
bool R_LoadImage8( byte *data, int width, int height, bool mipmap, int alpha)
{
	unsigned	*trans = (unsigned *)imagebuffer;
	int	i, s, p;
	bool	noalpha;

	if (width * height > imagebufsize/4)
	{
		Msg("R_LoadImage8: image too big (%i * %i)", width, height);
		return false;
	}
	
	s = width * height;

	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha)
	{
		noalpha = true;
		for (i = 0; i < s; i++)
		{
			p = data[i];
			if (p == 255)
			{
				noalpha = false;
				trans[i] = 0;
			}
			else trans[i] = d_8to24table[p];
		}

		switch( alpha )
		{
		default:
			if (alpha && noalpha) alpha = false;
			break;
		case 2:
			alpha = true;
			for (i=0 ; i<s ; i++)
			{
				p = data[i];
				if (p == 0) trans[i] &= 0x00ffffff;
				else if( p & 1 )
				{
					trans[i] &= 0x00ffffff;
					trans[i] |= ( ( int )( 255 * 0.5 ) ) << 24;
				}
				else trans[i] |= 0xff000000;
			}
			break;
		case 3:
			alpha = true;
			for (i=0 ; i<s ; i++)
			{
				p = data[i];
				if (p == 0) trans[i] &= 0x00ffffff;
			}
			break;
		case 4:
			alpha = true;
			for (i=0 ; i<s ; i++)
			{
				p = data[i];
				trans[i] = d_8to24table[ColorIndex[p>>4]] & 0x00ffffff;
				trans[i] |= ( int )ColorPercent[p&15] << 24;
				//trans[i] = 0x7fff0000;
			}
			break;
		}
	}
	else
	{
		for (i = (s & ~3) - 4; i >= 0; i -= 4)
		{
			trans[i] = d_8to24table[data[i]];
			trans[i+1] = d_8to24table[data[i+1]];
			trans[i+2] = d_8to24table[data[i+2]];
			trans[i+3] = d_8to24table[data[i+3]];
		}
		for (i = s & ~3; i < s; i++)//wow, funky
		{
			trans[i] = d_8to24table[data[i]];
		}
	}
	return R_LoadImage32 ( trans, width, height, mipmap, alpha);
}

bool R_LoadImage8to32 (byte *data, byte *pal, int width, int height, bool mipmap, int alpha)
{
	byte	*trans = imagebuffer;
	int	i, s;

	s = width*height;
	if (s > imagebufsize/4)
	{
		Sys_Error("R_LoadImage8to32: image too big (%i*%i)", width, height);
          	return false;
          }
	if (s&3) Sys_Error ("R_LoadImage8to32: s&3");
	for (i=0 ; i<s ; i+=1)
	{
		trans[(i<<2)+0] = gammatable[pal[data[i]*4+0]];
		trans[(i<<2)+1] = gammatable[pal[data[i]*4+1]];
		trans[(i<<2)+2] = gammatable[pal[data[i]*4+2]];
		trans[(i<<2)+3] = gammatable[pal[data[i]*4+3]];
	}
	return R_LoadImage32((unsigned*)trans, width, height, mipmap, true);
}

/*
===============
R_LoadImage8to24
===============
*/
bool R_LoadImage8to24(byte *data, byte *pal, int width, int height, bool mipmap, int alpha)
{
	byte	*trans = imagebuffer;
	int	i, s;
	bool	noalpha;
	int	p;

	s = width*height;
	if (s > imagebufsize/4)
	{
		Msg("R_LoadImage8to24: image too big (%i * %i)", width, height);
          	return false;
          }

	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha)
	{
		noalpha = true;
		for (i=0 ; i<s ; i++)
		{
			p = data[i];
			if (p == 255) noalpha = false;
			trans[(i<<2)+0] = pal[p*3+0];
			trans[(i<<2)+1] = pal[p*3+1];
			trans[(i<<2)+2] = pal[p*3+2];
			trans[(i<<2)+3] = (p==255)?0:255;
		}
		if (alpha && noalpha) alpha = false;
	}
	else
	{
		if (s&3) Sys_Error ("R_LoadImage8to24: s&3");
		for (i=0 ; i<s ; i+=1)
		{
			trans[(i<<2)+0] = pal[data[i]*3+0];
			trans[(i<<2)+1] = pal[data[i]*3+1];
			trans[(i<<2)+2] = pal[data[i]*3+2];
			trans[(i<<2)+3] = 255;
		}
	}
	return R_LoadImage32((unsigned*)trans, width, height, mipmap, alpha);
}

/*
===============
R_FindImage

Finds or loads the given image
===============
*/
image_t *R_FindImage (char *name, char *buffer, int size, imagetype_t type)
{
	image_t	*image;
	byte	*pic = NULL;
	int	i, width, height;
          
	if (!name ) return NULL;
          
	// look for it
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!strcmp(name, image->name))
		{
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	pic = ri.FS_LoadImage(name, &width, &height); //loading form disk

	if(!pic && buffer && size) //try to load internal resource
		pic = ri.FS_LoadImageData(name, buffer, size, &width, &height);

	image = R_LoadImage(name, pic, NULL, width, height, type, 32, false );
	if (pic) Mem_Free(pic);

	return image;
}

/*
================
R_LoadImage

This is also used as an entry point for the generated r_notexture
================
*/
image_t *R_LoadImage(char *name, byte *pic, byte *pal, int width, int height, imagetype_t type, int bits, int alpha )
{
	image_t	*image;
	int	i, iResult;
          bool	mipmap = true;
	
	//nothing to load
	if (!pic) return NULL;
	
	// find a free image_t
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!image->texnum) break;
	}
	if (i == numgltextures)
	{
		if (numgltextures == MAX_GLTEXTURES)
		{
			Msg("ERROR: GL Textures limit is out\n");
			return NULL;
		}
		numgltextures++;
	}
	image = &gltextures[i];

	if (strlen(name) >= sizeof(image->name))
	{
		Msg( "R_LoadImage: \"%s\" is too long", name);
		return NULL;
	}
	
	strcpy (image->name, name);
	image->registration_sequence = registration_sequence;

	image->width = width;
	image->height = height;
	image->type = type;
          image->paletted = pal ? true : false;
	
	//don't build mips for sky & hud pics
	if(image->type == it_pic) { mipmap = false; alpha = true; }
	if(image->type == it_sky) mipmap = false;
	
	image->texnum = TEXNUM_IMAGES + (image - gltextures);
	GL_Bind(image->texnum);

	if(image->paletted)
	{
		if(bits == 8 )iResult = 0;
		if(bits == 16)iResult = 0;
		if(bits == 24)iResult = R_LoadImage8to24(pic, pal, width, height, mipmap, alpha );
		if(bits == 32)iResult = R_LoadImage8to32(pic, pal, width, height, mipmap, alpha );
	}
	else
	{
		if(bits == 8 )iResult = R_LoadImage8 ( pic, width, height, mipmap, alpha );
		if(bits == 16)iResult = 0;
		if(bits == 24)iResult = 0;
		if(bits == 32)iResult = R_LoadImage32( (unsigned *)pic, width, height, mipmap, alpha );
	}
          
	//check for errors
	if(!iResult)
	{
		Msg("R_LoadImage: can't loading %s %s with bpp %d\n", pal ? "paletted" : "", name, bits ); 
		image->name[0] = '\0';
		image->registration_sequence = 0;
		return NULL;
	} 
	return image;
}

/*
================
R_ImageFreeUnused

Any image that was not touched on this registration sequence
will be freed.
================
*/
void R_ImageFreeUnused(void)
{
	int		i;
	image_t	*image;

	// never free r_notexture or particle texture
	r_notexture->registration_sequence = registration_sequence;
	r_particletexture->registration_sequence = registration_sequence;

	for (i=0, image=gltextures ; i<numgltextures ; i++, image++)
	{
		// used this sequence
		if (image->registration_sequence == registration_sequence) continue;
		if (!image->registration_sequence) continue; // free image_t slot
		if (image->type == it_pic) continue; // don't free pics
		qglDeleteTextures (1, &image->texnum);// free it
		memset (image, 0, sizeof(*image));
	}
}
