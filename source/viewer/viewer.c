//=======================================================================
//			Copyright XashXT Group 2007 ©
//			editor.c - game editor dll
//=======================================================================

#include "viewer.h"

stdlib_api_t com;

/*
==================
DllMain

==================
*/
launch_exp_t DLLEXPORT *CreateAPI( stdlib_api_t *input, void *unused )
{
	static launch_exp_t Viewer;

	com = *input;

	Viewer.api_size = sizeof(launch_exp_t);

	Viewer.Init = InitViewer;
	Viewer.Main = ViewerMain;
	Viewer.Free = FreeViewer;
	Viewer.CPrint = GUI_Print;

	return &Viewer;
}