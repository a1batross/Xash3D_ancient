//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         ui_main.c - progs user interface
//=======================================================================

#include "uimenu.h"

#define M_F_INIT		"m_init"
#define M_F_KEYDOWN		"m_keydown"
#define M_F_KEYUP		"m_keyup"
#define M_F_DRAW		"m_draw"
// normal menu names (rest)
#define M_F_TOGGLE		"m_toggle"
#define M_F_SHUTDOWN	"m_shutdown"

static char *m_required_func[] = {
M_F_INIT,
M_F_KEYDOWN,
M_F_DRAW,
M_F_TOGGLE,
M_F_SHUTDOWN,
};

#ifdef NG_MENU
static bool m_displayed;
#endif

static int m_numrequiredfunc = sizeof(m_required_func) / sizeof(char*);

static func_t m_draw, m_keydown;
static mfunction_t *m_keyup;

void MR_SetRouting (bool forceold);

void MP_Error(const char *format, ...)
{
	static bool processingError = false;
	char errorstring[MAX_INPUTLINE];
	va_list argptr;

	va_start (argptr, format);
	std.vsnprintf (errorstring, sizeof(errorstring), format, argptr);
	va_end (argptr);
	Msg( "Menu_Error: %s\n", errorstring );

	if( !processingError ) {
		processingError = true;
		PRVM_Crash();
		processingError = false;
	} else {
		Msg( "Menu_Error: Recursive call to MP_Error (from PRVM_Crash)!\n" );
	}

	// fall back to the normal menu

	// say it
	Con_Print("Falling back to normal menu\n");

	cls.key_dest = key_game;

	// init the normal menu now -> this will also correct the menu router pointers
	MR_SetRouting (TRUE);

	Host_AbortCurrentFrame();
}

void MP_KeyEvent (int key, char ascii, bool downevent)
{
	PRVM_Begin;
	PRVM_SetProg(PRVM_MENUPROG);

	// set time
	*prog->time = cls.realtime;

	// pass key
	prog->globals.gp[OFS_PARM0] = (float) key;
	prog->globals.gp[OFS_PARM1] = (float) ascii;
	if (downevent)
		PRVM_ExecuteProgram(m_keydown, M_F_KEYDOWN"(float key, float ascii) required\n");
	else if (m_keyup)
		PRVM_ExecuteProgram((func_t)(m_keyup - prog->functions), M_F_KEYUP"(float key, float ascii) required\n");

	PRVM_End;
}

void MP_Draw (void)
{
	PRVM_Begin;
	PRVM_SetProg(PRVM_MENUPROG);

	// set time
	*prog->time = cls.realtime;

	PRVM_ExecuteProgram(m_draw,"");

	PRVM_End;
}

void MP_ToggleMenu_f (void)
{
	PRVM_Begin;
	PRVM_SetProg(PRVM_MENUPROG);

	// set time
	*prog->time = cls.realtime;

#ifdef NG_MENU
	m_displayed = !m_displayed;
	if( m_displayed )
		PRVM_ExecuteProgram((func_t) (PRVM_ED_FindFunction(M_F_DISPLAY) - prog->functions),"");
	else
		PRVM_ExecuteProgram((func_t) (PRVM_ED_FindFunction(M_F_HIDE) - prog->functions),"");
#else
	PRVM_ExecuteProgram((func_t) (PRVM_ED_FindFunction(M_F_TOGGLE) - prog->functions),"");
#endif

	PRVM_End;
}

void MP_Shutdown (void)
{
	PRVM_Begin;
	PRVM_SetProg(PRVM_MENUPROG);

	// set time
	*prog->time = cls.realtime;

	PRVM_ExecuteProgram((func_t) (PRVM_ED_FindFunction(M_F_SHUTDOWN) - prog->functions),"");

	// reset key_dest
	cls.key_dest = key_game;

	// AK not using this cause Im not sure whether this is useful at all instead :
	PRVM_ResetProg();

	PRVM_End;
}

void MP_Fallback (void)
{
	MP_Shutdown();

	cls.key_dest = key_game;

	// init the normal menu now -> this will also correct the menu router pointers
	MR_SetRouting (TRUE);
}

void MP_Init (void)
{
	PRVM_Begin;
	PRVM_InitProg(PRVM_MENUPROG);

	prog->edictprivate_size = 0; // no private struct used
	prog->name = M_NAME;
	prog->num_edicts = 1;
	prog->limit_edicts = M_MAX_EDICTS;
	prog->extensionstring = vm_m_extensions;
	prog->builtins = vm_m_builtins;
	prog->numbuiltins = vm_m_numbuiltins;
	prog->init_cmd = VM_M_Cmd_Init;
	prog->reset_cmd = VM_M_Cmd_Reset;
	prog->error_cmd = MP_Error;

	// allocate the mempools
	prog->progs_mempool = Mem_AllocPool( M_PROG_FILENAME );
	PRVM_LoadProgs(M_PROG_FILENAME, m_numrequiredfunc, m_required_func, 0, NULL);

	// set m_draw and m_keydown
	m_draw = (func_t) (PRVM_ED_FindFunction(M_F_DRAW) - prog->functions);
	m_keydown = (func_t) (PRVM_ED_FindFunction(M_F_KEYDOWN) - prog->functions);
	m_keyup = PRVM_ED_FindFunction(M_F_KEYUP);

	// set time
	*prog->time = cls.realtime;

	// call the prog init
	PRVM_ExecuteProgram((func_t) (PRVM_ED_FindFunction(M_F_INIT) - prog->functions),"");

	PRVM_End;
}

void MP_Restart(void)
{
	MP_Init();
}

//============================================================================
// Menu router

void (*MR_KeyEvent) (int key, char ascii, bool downevent);
void (*MR_Draw) (void);
void (*MR_ToggleMenu_f) (void);
void (*MR_Shutdown) (void);

void MR_SetRouting(bool forceold)
{
	static bool m_init = FALSE, mp_init = FALSE;

	// if the menu prog isnt available or forceqmenu ist set, use the old menu
	if(!FS_FileExists(M_PROG_FILENAME))
	{
		// set menu router function pointers
		/*MR_KeyEvent = M_KeyEvent;
		MR_Draw = M_Draw;
		MR_ToggleMenu_f = M_ToggleMenu_f;
		MR_Shutdown = M_Shutdown;
                    */
                    
		// init
		if(!m_init)
		{
			M_Init();
			m_init = TRUE;
		}
		else M_Restart();
	}
	else
	{
		// set menu router function pointers
		MR_KeyEvent = MP_KeyEvent;
		MR_Draw = MP_Draw;
		MR_ToggleMenu_f = MP_ToggleMenu_f;
		MR_Shutdown = MP_Shutdown;

		if(!mp_init)
		{
			MP_Init();
			mp_init = TRUE;
		}
		else
			MP_Restart();
	}
}

void MR_Restart(void)
{
	MR_Shutdown();
	MR_SetRouting(FALSE);
}

void Call_MR_ToggleMenu_f(void)
{
	if(MR_ToggleMenu_f)
		MR_ToggleMenu_f();
}

void MR_Init_Commands(void)
{
	Cmd_AddCommand("menu_restart",MR_Restart, "restart menu system (reloads menu.dat");
}

void MR_Init(void)
{
}