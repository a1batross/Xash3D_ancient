//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        basefiles.h - xash supported formats
//=======================================================================
#ifndef BASE_FILES_H
#define BASE_FILES_H

/*
==============================================================================

.aur particle file format
==============================================================================
*/
#define IDAURORAHEADER	(('R'<<24)+('U'<<16)+('A'<<8)+'I') // little-endian "IAUR"
#define AURORA_VERSION	1

typedef struct
{
	int	width, height;
	int	origin_x, origin_y;		// raster coordinates inside pic
	char	name[MAX_SKINNAME];		// name of pcx file
} daurframe_t;

typedef struct
{
	int		ident;
	int		version;
	int		numframes;

	// aurora description
	float		startcolor[3];	// RGBA
	float		finalcolor[3];	// RGBA
	float		startalpha;	// alpha-value
	float		finalalpha;
	float		framerate;
	byte		rendermode;


	dsprframe_t	frames[1];	// variable sized
} daurora_t;

#endif//BASE_FILES_H