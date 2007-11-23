//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         ui_main.c - progs user interface
//=======================================================================

#include "uimenu.h"

#define M_F_INIT		"m_init"
#define M_F_KEYDOWN		"m_keydown"
#define M_F_KEYUP		"m_keyup"
#define M_F_DRAW		"m_draw"
#define M_F_TOGGLE		"m_toggle"
#define M_F_SHUTDOWN	"m_shutdown"

static char *m_required_func[] =
{
M_F_INIT,
M_F_KEYDOWN,
M_F_DRAW,
M_F_TOGGLE,
M_F_SHUTDOWN,
};

static int m_numrequiredfunc = sizeof(m_required_func) / sizeof(char*);

static func_t m_draw, m_keydown;
static mfunction_t *m_keyup;

void UI_Error(const char *format, ...)
{
	static bool	processingError = false;
	char		errorstring[MAX_INPUTLINE];
	va_list		argptr;

	va_start( argptr, format );
	std.vsnprintf(errorstring, sizeof(errorstring), format, argptr);
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

void UI_KeyEvent(menuframework_s *m, int key)
{
	PRVM_Begin;
	PRVM_SetProg( PRVM_MENUPROG );

	// set time
	*prog->time = cls.realtime;

	// pass key
	prog->globals.gp[OFS_PARM0] = (float)key;
	prog->globals.gp[OFS_PARM1] = (string_t)PRVM_SetEngineString(Key_KeynumToString(key));
	PRVM_ExecuteProgram(m_keydown, M_F_KEYDOWN"(menuframework_s *m, int key) required\n");

	PRVM_End;
}

void UI_Draw( void )
{
	PRVM_Begin;
	PRVM_SetProg(PRVM_MENUPROG);

	// set time
	*prog->time = cls.realtime;

	PRVM_ExecuteProgram( m_draw, "" );
	PRVM_End;
}

void UI_ToggleMenu_f( void )
{
	PRVM_Begin;
	PRVM_SetProg(PRVM_MENUPROG);

	// set time
	*prog->time = cls.realtime;

	PRVM_ExecuteProgram((func_t)(PRVM_ED_FindFunction(M_F_TOGGLE) - prog->functions), "" );
	PRVM_End;
}

void UI_Shutdown( void )
{
	PRVM_Begin;
	PRVM_SetProg(PRVM_MENUPROG);

	// set time
	//*prog->time = cls.realtime;

	PRVM_ExecuteProgram((func_t) (PRVM_ED_FindFunction(M_F_SHUTDOWN) - prog->functions),"");

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

	prog->edictprivate_size = 0; // no private struct used
	prog->name = M_NAME;
	prog->num_edicts = 1;
	prog->limit_edicts = M_MAX_EDICTS;
	prog->extensionstring = "";
	prog->builtins = vm_m_builtins;
	prog->numbuiltins = vm_m_numbuiltins;
	prog->init_cmd = VM_Cmd_Init;
	prog->reset_cmd = VM_Cmd_Reset;
	prog->error_cmd = UI_Error;

	// allocate the mempools
	prog->progs_mempool = Mem_AllocPool( M_PROG_FILENAME );
	PRVM_LoadProgs( M_PROG_FILENAME, m_numrequiredfunc, m_required_func, 0, NULL);

	// set m_draw and m_keydown
	m_draw = (func_t)(PRVM_ED_FindFunction(M_F_DRAW) - prog->functions);
	m_keydown = (func_t)(PRVM_ED_FindFunction(M_F_KEYDOWN) - prog->functions);
	m_keyup = PRVM_ED_FindFunction(M_F_KEYUP);

	// set time
	*prog->time = cls.realtime;

	// call the prog init
	PRVM_ExecuteProgram((func_t)(PRVM_ED_FindFunction(M_F_INIT) - prog->functions),"");
	PRVM_End;
}