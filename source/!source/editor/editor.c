//=======================================================================
//			Copyright XashXT Group 2007 ©
//			editor.c - game editor dll
//=======================================================================

#include "editor.h"

stdlib_api_t com;

/*
==================
DllMain

==================
*/
launch_exp_t DLLEXPORT *CreateAPI( stdlib_api_t *input, void *unused )
{
	static launch_exp_t Editor;

	com = *input;

	Editor.api_size = sizeof(launch_exp_t);

	Editor.Init = InitEditor;
	Editor.Main = EditorMain;
	Editor.Free = FreeEditor;
	Editor.CPrint = GUI_Print;

	return &Editor;
}