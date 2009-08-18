/* -------------------------------------------------------------------------------

Copyright (C) 1999-2007 id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

----------------------------------------------------------------------------------

This code has been altered significantly from its original form, to support
several games based on the Quake III Arena engine, in the form of "Q3Map2."

------------------------------------------------------------------------------- */



/* marker */
#define IMAGE_C



/* dependencies */
#include "q3map2.h"



/* -------------------------------------------------------------------------------

this file contains image pool management with reference counting. note: it isn't
reentrant, so only call it from init/shutdown code or wrap calls in a mutex

------------------------------------------------------------------------------- */

/*
ImageInit()
implicitly called by every function to set up image list
*/

static void ImageInit( void )
{
	if( numImages <= 0 )
	{
		size_t	size;
		byte	*buf = FS_LoadInternal( "checkerboard.dds", &size ); // quake1 missing texture :)

		Image_Init( NULL, IL_ALLOW_OVERWRITE|IL_IGNORE_MIPS|IL_USE_LERPING );

		// clear images (FIXME: this could theoretically leak)
		Mem_Set( images, 0, sizeof( images ));
		
		// generate *bogus image
		images[0].name = copystring( DEFAULT_IMAGE );
		images[0].pic = FS_LoadImage(  "#checkerboard.dds", buf, size );
		images[0].refCount = 1;
	}
}



/*
ImageFree()
frees an rgba image
*/

void ImageFree( image_t *image )
{
	/* dummy check */
	if( image == NULL )
		return;
	
	/* decrement refcount */
	image->refCount--;
	
	if( image->refCount <= 0 )
	{
		if( image->name ) Mem_Free( image->name );
		image->name = NULL;
		FS_FreeImage( image->pic );
		numImages--;
	}
}



/*
ImageFind()
finds an existing rgba image and returns a pointer to the image_t struct or NULL if not found
*/
image_t *ImageFind( const char *filename )
{
	int	i;
	char	name[MAX_SYSPATH];
	
	
	/* init */
	ImageInit();
	
	/* dummy check */
	if( filename == NULL || filename[0] == '\0' )
		return NULL;
	
	/* strip file extension off name */
	com.strcpy( name, filename );
	FS_StripExtension( name );
	
	// search list
	for( i = 0; i < MAX_IMAGES; i++ )
	{
		if( images[i].name != NULL && !com.strcmp( name, images[i].name ))
			return &images[i];
	}
	return NULL;
}



/*
ImageLoad()
loads an rgba image and returns a pointer to the image_t struct or NULL if not found
*/
image_t *ImageLoad( const char *filename )
{
	int		i;
	image_t		*image;
	char		name[MAX_SYSPATH];
	
	ImageInit();
	
	// dummy check
	if( filename == NULL || filename[ 0 ] == '\0' )
		return NULL;
	
	com.strcpy( name, filename );
	FS_StripExtension( name );
	
	// try to find existing image
	image = ImageFind( name );
	if( image )
	{
		image->refCount++;
		return image;
	}
	
	// search for free spot
	for( i = 0; i < MAX_IMAGES; i++ )
	{
		if( images[i].name == NULL )
		{
			image = &images[i];
			break;
		}
	}
	
	// too many images?
	if( !image ) Sys_Break( "MAX_IMAGES (%d) exceeded, there are too many images used\n", MAX_IMAGES );
	
	image->name = copystring( name );
	image->pic = FS_LoadImage( name, NULL, 0 );

	// make sure everything's kosher
	if( image->pic == NULL )
	{
		Mem_Free( image->name );
		image->name = NULL;
		return NULL;
	}

	image->refCount = 1;
	numImages++;
	
	return image;
}