//=======================================================================
//			Copyright XashXT Group 2007 ©
//			editor.c - game editor dll
//=======================================================================

#include "editor.h"

stdinout_api_t std;

/*
==================
DllMain

==================
*/
launcher_exp_t DLLEXPORT *CreateAPI( stdinout_api_t *input )
{
	static launcher_exp_t Editor;

	// Sys_LoadLibrary can create fake instance, to check
	// api version and api size, but first argument will be 0
	// and always make exception, run simply check for avoid it
	if(input)std = *input;

	Editor.apiversion = LAUNCHER_API_VERSION;
	Editor.api_size = sizeof(launcher_exp_t);

	Editor.Init = InitEditor;
	Editor.Main = EditorMain;
	Editor.Free = FreeEditor;

	return &Editor;
}