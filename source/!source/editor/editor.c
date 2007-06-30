//=======================================================================
//			Copyright XashXT Group 2007 ©
//			editor.c - game editor dll
//=======================================================================

#include "editor.h"

stdio_api_t std;

/*
==================
DllMain

==================
*/
edit_api_t DLLEXPORT *CreateAPI( stdio_api_t *input )
{
	static edit_api_t ei;

	std = *input;

	ei.editor_init = InitEditor;
	ei.editor_main = EditorMain;
	ei.editor_free = FreeEditor;

	return &ei;
}