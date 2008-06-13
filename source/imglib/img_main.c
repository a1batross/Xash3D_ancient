//=======================================================================
//			Copyright XashXT Group 2007 ©
//			img_main.c - image processing library
//=======================================================================

#include "imagelib.h"

stdlib_api_t com;
byte *zonepool;
int app_name = 0;
cvar_t	*img_resample_lerp;

void ImageLib_Init ( uint funcname )
{
	// init pools
	zonepool = Mem_AllocPool( "ImageLib Pool" );
          app_name = funcname;

	img_resample_lerp = Cvar_Get( "img_lerping", "1", CVAR_SYSTEMINFO );
}

void ImageLib_Free ( void )
{
	Mem_Check(); // check for leaks
	Mem_FreePool( &zonepool );
}

imglib_exp_t DLLEXPORT *CreateAPI( stdlib_api_t *input, void *unused )
{
	static imglib_exp_t		Com;

	com = *input;

	// generic functions
	Com.api_size = sizeof(imglib_exp_t);

	Com.Init = ImageLib_Init;
	Com.Free = ImageLib_Free;
	Com.LoadImage = Image_Load;
	Com.SaveImage = Image_Save;
	Com.DecompressDXTC = Image_DecompressDXTC;
	Com.DecompressARGB = Image_DecompressARGB;
	Com.ResampleImage = Image_Resample;
	Com.FreeImage = Image_FreeImage;

	return &Com;
}