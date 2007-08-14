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
launcher_exp_t DLLEXPORT CreateAPI( stdinout_api_t input )
{
	static launcher_exp_t Editor;

	std = input;

	Editor.Init = InitEditor;
	Editor.Main = EditorMain;
	Editor.Free = FreeEditor;

	return Editor;
}