//////////////////////////////////////////////////////////
// sys globals

entity self;

/////////////////////////////////////////////////////////
void		end_sys_globals;
/////////////////////////////////////////////////////////
// sys fields

/////////////////////////////////////////////////////////
void		end_sys_fields;
/////////////////////////////////////////////////////////
// sys functions

void() m_init;
void(float keynr, float ascii) m_keydown;
void() m_draw;
void() m_toggle;
void() m_shutdown;

/////////////////////////////////////////////////////////
// sys constants
///////////////////////////
// key constants

//
// these are the key numbers that should be passed to Key_Event
//
float K_TAB			=	9;
float K_ENTER		=	13;
float K_ESCAPE		=	27;
float K_SPACE		=	32;

// normal keys should be passed as lowercased ascii

float K_BACKSPACE	=	127;
float K_UPARROW		=	128;
float K_DOWNARROW	=	129;
float K_LEFTARROW	=	130;
float K_RIGHTARROW	=	131;

float K_ALT		=	132;
float K_CTRL	=	133;
float K_SHIFT	=	134;
float K_F1		=	135;
float K_F2		=	136;
float K_F3		=	137;
float K_F4		=	138;
float K_F5		=	139;
float K_F6		=	140;
float K_F7		=	141;
float K_F8		=	142;
float K_F9		=	143;
float K_F10		=	144;
float K_F11		=	145;
float K_F12		=	146;
float K_INS		=	147;
float K_DEL		=	148;
float K_PGDN	=	149;
float K_PGUP	=	150;
float K_HOME	=	151;
float K_END		=	152;

float K_PAUSE	=	153;

float K_NUMLOCK		= 154;
float K_CAPSLOCK	= 155;
float K_SCROLLLOCK	= 156;

float K_KP_0	=	157;
float K_KP_INS	=	K_KP_0;
float K_KP_1	=	158;
float K_KP_END	=	K_KP_1;
float K_KP_2	=	159;
float K_KP_DOWNARROW = K_KP_2;
float K_KP_3	=	160;
float K_KP_PGDN = K_KP_3;
float K_KP_4	=	161;
float K_KP_LEFTARROW = K_KP_4;
float K_KP_5	=	162;
float K_KP_6	=	163;
float K_KP_RIGHTARROW = K_KP_6;
float K_KP_7	=	164;
float K_KP_HOME = K_KP_7;
float K_KP_8	=	165;
float K_KP_UPARROW = K_KP_8;
float K_KP_9	= 166;
float K_KP_PGUP = K_KP_9;
float K_KP_PERIOD = 167;
float K_KP_DEL = K_KP_PERIOD;
float K_KP_DIVIDE = 168;
float K_KP_SLASH = K_KP_DIVIDE;
float K_KP_MULTIPLY = 169;
float K_KP_MINUS	= 170;
float K_KP_PLUS		= 171;
float K_KP_ENTER	= 172;
float K_KP_EQUALS	= 173;

// mouse buttons generate virtual keys

float K_MOUSE1	=	512;
float K_MOUSE2	=	513;
float K_MOUSE3	=	514;
float K_MOUSE4	=	515;
float K_MWHEELUP = K_MOUSE4;
float K_MOUSE5	=	516;
float K_MWHEELDOWN = K_MOUSE5;
float K_MOUSE6	=	517;
float K_MOUSE7	=	518;
float K_MOUSE8	=	519;
float K_MOUSE9	=	520;
float K_MOUSE10	=	521;
float K_MOUSE11	=	522;
float K_MOUSE12	=	523;
float K_MOUSE13	=	524;
float K_MOUSE14	=	525;
float K_MOUSE15	=	526;
float K_MOUSE16	=	527;

//
// joystick buttons
//
float K_JOY1 = 768;
float K_JOY2 = 769;
float K_JOY3 = 770;
float K_JOY4 = 771;

//
//
// aux keys are for multi-buttoned joysticks to generate so they can use
// the normal binding process
//
float K_AUX1	=	772;
float K_AUX2	=	773;
float K_AUX3	=	774;
float K_AUX4	=	775;
float K_AUX5	=	776;
float K_AUX6	=	777;
float K_AUX7	=	778;
float K_AUX8	=	779;
float K_AUX9	=	780;
float K_AUX10	=	781;
float K_AUX11	=	782;
float K_AUX12	=	783;
float K_AUX13	=	784;
float K_AUX14	=	785;
float K_AUX15	=	786;
float K_AUX16	=	787;
float K_AUX17	=	788;
float K_AUX18	=	789;
float K_AUX19	=	790;
float K_AUX20	=	791;
float K_AUX21	=	792;
float K_AUX22	=	793;
float K_AUX23	=	794;
float K_AUX24	=	795;
float K_AUX25	=	796;
float K_AUX26	=	797;
float K_AUX27	=	798;
float K_AUX28	=	799;
float K_AUX29	=	800;
float K_AUX30	=	801;
float K_AUX31	=	802;
float K_AUX32	=	803;

///////////////////////////
// key dest constants

float KEY_GAME 		=	0;
float KEY_MENU		=	2;
float KEY_UNKNOWN	= 	3;

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
