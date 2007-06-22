//=======================================================================
//			Copyright XashXT Group 2007 ©
//			editor.c - game editor dll
//=======================================================================

#include "editor.h"

system_api_t gSysFuncs;

/*
==================
DllMain

==================
*/
edit_api_t DLLEXPORT CreateAPI( system_api_t sysapi )
{
	edit_api_t ei;

	gSysFuncs = sysapi;

	ei.editor_init = InitEditor;
	ei.editor_main = EditorMain;
	ei.editor_free = FreeEditor;

	return ei;
}