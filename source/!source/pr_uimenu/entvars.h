//=======================================================================
//			Copyright XashXT Group 2007 ©
//			    entvars.h - menu edicts
//=======================================================================
#pragma version 7		// set right version

	entity	pev;
	void	m_init( void );
	void	m_shutdown( void );
	void	m_keydown( float keynr, string ascii );
	void	m_draw( void );
	void	m_toggle( void );
void end_sys_globals;

	.string	type;	// ITEM_* type
	.string	parent;
	.string	name;	// item name (for linking)

	.entity	_parent;	// pointer to parent entity
	.entity	_next;	// point to the next, respectively, the previous item
	.entity	_prev;
	.entity	_child;	// points to the first child
void end_sys_fields;

// these are the key numbers that should be passed to Key_Event
enum
{
	// keyboard
	K_TAB = 9,
	K_ENTER = 13,
	K_ESCAPE = 27,
	K_SPACE = 32,
	K_BACKSPACE = 127,
	K_COMMAND = 128,
	K_CAPSLOCK,
	K_POWER,
	K_PAUSE,
	K_UPARROW,
	K_DOWNARROW,
	K_LEFTARROW,
	K_RIGHTARROW,
	K_ALT,
	K_CTRL,
	K_SHIFT,
	K_INS,
	K_DEL,
	K_PGDN,
	K_PGUP,
	K_HOME,
	K_END,
	K_F1,
	K_F2,
	K_F3,
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	K_F11,
	K_F12,
	K_F13,
	K_F14,
	K_F15,
	K_KP_HOME,
	K_KP_UPARROW,
	K_KP_PGUP,
	K_KP_LEFTARROW,
	K_KP_5,
	K_KP_RIGHTARROW,
	K_KP_END,
	K_KP_DOWNARROW,
	K_KP_PGDN,
	K_KP_ENTER,
	K_KP_INS,
	K_KP_DEL,
	K_KP_SLASH,
	K_KP_MINUS,
	K_KP_PLUS,
	K_KP_NUMLOCK,
	K_KP_STAR,
	K_KP_EQUALS,

	// mouse
	K_MOUSE1,
	K_MOUSE2,
	K_MOUSE3,
	K_MOUSE4,
	K_MOUSE5,
	K_MWHEELDOWN,
	K_MWHEELUP,

	K_LAST_KEY // this had better be < 256!
};


///////////////////////////
// key dest constants

enum
{
	KEY_GAME,
	KEY_CONSOLE,
	KEY_MESSAGE,
	KEY_MENU
};

///////////////////////////
// file constants

float FILE_READ = 0;
float FILE_APPEND = 1;
float FILE_WRITE = 2;

///////////////////////////
// logical constants (just for completeness)

float TRUE 	= 1;
float FALSE = 0;

///////////////////////////
// boolean constants

float true = 1;
float false = 0;

///////////////////////////
// msg constants

float MSG_BROADCAST		= 0;		// unreliable to all
float MSG_ONE			= 1;		// reliable to one (msg_entity)
float MSG_ALL			= 2;		// reliable to all
float MSG_INIT			= 3;		// write to the init string

/////////////////////////////
// mouse target constants

float MT_MENU = 1;
float MT_CLIENT = 2;

/////////////////////////
// client state constants

float CS_DEDICATED 		= 0;
float CS_DISCONNECTED 	= 1;
float CS_CONNECTED		= 2;

///////////////////////////
// blend flags

float DRAWFLAG_NORMAL		= 0;
float DRAWFLAG_ADDITIVE 	= 1;
float DRAWFLAG_MODULATE 	= 2;
float DRAWFLAG_2XMODULATE   = 3;

///////////////////////////
// null entity (actually it is the same like the world entity)

entity null_entity;

///////////////////////////
// error constants

// file handling
float ERR_CANNOTOPEN			= -1; // fopen
float ERR_NOTENOUGHFILEHANDLES 	= -2; // fopen
float ERR_INVALIDMODE 			= -3; // fopen
float ERR_BADFILENAME 			= -4; // fopen

// drawing functions

float ERR_NULLSTRING			= -1;
float ERR_BADDRAWFLAG			= -2;
float ERR_BADSCALE				= -3;
float ERR_BADSIZE				= ERR_BADSCALE;
float ERR_NOTCACHED				= -4;

/* not supported at the moment
///////////////////////////
// os constants

float OS_WINDOWS	= 0;
float OS_LINUX		= 1;
float OS_MAC		= 2;
*/

