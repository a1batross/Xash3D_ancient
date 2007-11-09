//=======================================================================
//			Copyright XashXT Group 2007 ©
//			editor.c - game editor dll
//=======================================================================

#include "editor.h"

stdlib_api_t std;

/*
==================
DllMain

==================
*/
launch_exp_t DLLEXPORT *CreateAPI( stdlib_api_t *input )
{
	static launch_exp_t Editor;

	// Sys_LoadLibrary can create fake instance, to check
	// api version and api size, but first argument will be 0
	// and always make exception, run simply check for avoid it
	if(input)std = *input;

	Editor.api_size = sizeof(launch_exp_t);

	Editor.Init = InitEditor;
	Editor.Main = EditorMain;
	Editor.Free = FreeEditor;

	return &Editor;
}