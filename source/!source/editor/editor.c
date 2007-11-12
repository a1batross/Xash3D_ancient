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
launch_exp_t DLLEXPORT *CreateAPI( stdlib_api_t *input, generic_api_t *unused )
{
	static launch_exp_t Editor;

	std = *input;

	Editor.api_size = sizeof(launch_exp_t);

	Editor.Init = InitEditor;
	Editor.Main = EditorMain;
	Editor.Free = FreeEditor;

	return &Editor;
}