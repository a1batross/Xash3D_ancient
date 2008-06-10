//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         ui_main.c - progs user interface
//=======================================================================

#include "uimenu.h"

bool ui_active = false;

void UI_Error(const char *format, ...)
{
	static bool	processingError = false;
	char		errorstring[MAX_INPUTLINE];
	va_list		argptr;

	va_start( argptr, format );
	com.vsnprintf(errorstring, sizeof(errorstring), format, argptr);
	va_end( argptr );
	Host_Error( "Menu_Error: %s\n", errorstring );

	if( !processingError )
	{
		processingError = true;
		PRVM_Crash();
		processingError = false;
	}
	else Host_Error( "Menu_RecursiveError: call to UI_Error (from PRVM_Crash)!\n" );

	// fall back to the normal menu
	Msg("Falling back to normal menu\n");
	cls.key_dest = key_game;

	// init the normal menu now -> this will also correct the menu router pointers
	Host_AbortCurrentFrame();
}

void UI_KeyEvent( int key )
{
	const char *ascii = Key_KeynumToString(key);
	PRVM_Begin;
	PRVM_SetProg( PRVM_MENUPROG );

	// set time
	*prog->time = cls.realtime;

	// setup args
	PRVM_G_FLOAT(OFS_PARM0) = key;
	PRVM_G_INT(OFS_PARM1) = PRVM_SetEngineString(ascii);
	PRVM_ExecuteProgram (prog->globals.ui->m_keydown, "QC function m_keydown is missing");

	PRVM_End;

	switch( key )
	{
	case K_ESCAPE:
		UI_HideMenu();
		break;
	}
}

void UI_Draw( void )
{
	PRVM_Begin;
	PRVM_SetProg(PRVM_MENUPROG);

	// set time
	*prog->time = cls.realtime;

	PRVM_ExecuteProgram (prog->globals.ui->m_draw, "QC function m_draw is missing");
	PRVM_End;
}

void UI_ShowMenu( void )
{
	PRVM_Begin;
	PRVM_SetProg(PRVM_MENUPROG);

	// set time
	*prog->time = cls.realtime;
	ui_active = true;
	PRVM_ExecuteProgram (prog->globals.ui->m_show, "QC function m_toggle is missing");
	PRVM_End;
}

void UI_HideMenu( void )
{
	PRVM_Begin;
	PRVM_SetProg(PRVM_MENUPROG);

	// set time
	*prog->time = cls.realtime;
	ui_active = false;
	PRVM_ExecuteProgram (prog->globals.ui->m_hide, "QC function m_toggle is missing");
	PRVM_End;
}

void UI_Shutdown( void )
{
	PRVM_Begin;
	PRVM_SetProg(PRVM_MENUPROG);

	// set time
	//*prog->time = cls.realtime;

	PRVM_ExecuteProgram (prog->globals.ui->m_shutdown, "QC function m_shutdown is missing");

	// reset key_dest
	cls.key_dest = key_game;

	// AK not using this cause Im not sure whether this is useful at all instead :
	PRVM_ResetProg();

	PRVM_End;
}

void UI_Init( void )
{
	PRVM_Begin;
	PRVM_InitProg( PRVM_MENUPROG );

	prog->progs_mempool = Mem_AllocPool( "Uimenu Progs" );
	prog->builtins = vm_ui_builtins;
	prog->numbuiltins = vm_ui_numbuiltins;
	prog->edictprivate_size = sizeof(ui_edict_t);
	prog->limit_edicts = UI_MAX_EDICTS;
	prog->name = "uimenu";
	prog->num_edicts = 1;
	prog->loadintoworld = false;
	prog->init_cmd = VM_Cmd_Init;
	prog->reset_cmd = VM_Cmd_Reset;
	prog->error_cmd = UI_Error;

	PRVM_LoadProgs( GI->uimenu_prog, 0, NULL, UI_NUM_REQFIELDS, ui_reqfields );
	*prog->time = cls.realtime;

	PRVM_ExecuteProgram (prog->globals.ui->m_init, "QC function m_init is missing");
	PRVM_End;
}