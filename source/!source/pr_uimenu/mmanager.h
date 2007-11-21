///////////////////////////////////////////////
// Menu Manager Source File
///////////////////////
// This file belongs to dpmod/darkplaces
// AK contains all manager constants, etc.
///////////////////////////////////////////////
// constants

const float MENU_NORMAL = 0;
const float MENU_INGAME = 1;

const float MENU_ENFORCELOADING = false;

// define these menus in the menu def files or dont
// if not defined there will be added default items
const string MENU_NORMAL_NAME = "normal";
const string MENU_INGAME_NAME = "ingame";

const string MENU_NORMAL_DEFAULT =
"// default normal menu\n"
"{\n"
"	\"type\" 	\"ITEM_WINDOW\"\n"
"	\"name\"		\"normal\"\n"
"}";

const string MENU_INGAME_DEFAULT =
"// default ingame menu\n"
"{\n"
"	\"type\" 	\"ITEM_WINDOW\"\n"
"	\"name\"		\"ingame\"\n"
"}";

// insert the files here
var string MENU_FILENAME_LIST = "menu/main.menu menu/options.menu";

const float MENU_ALLOWINGAME = FALSE;

// globals

entity menu_activewindow;

// points to the lowest selected menu item (that has no child item selected)
entity menu_selected;

// used to build up the local coord system
vector menu_localorigin;

vector menu_clip_pos, menu_clip_size; // global clip area

// local coord cursor
vector menu_cursor;

///////////
// fields
///

// controly type
.string type;

// managing stuff
.entity _parent;
.string parent;

//.entity _history;	// used to set up the history -> selectdown prefers _history over _parent

.string name;

.entity _next, _prev;		// point to the next, respectively, the previous item

.entity _child;			// points to the first child

// updating stuff
.vector click_pos, click_size;

.float orderpos;	// if FLAG_NOSELECT or FLAG_HIDDEN is set, it cant be selected
					// has to be set always to a correct value or to 0 then it is set
.float flag;

// drawing
// the clip_* are only used by menu's (at the moment)
.vector clip_pos, clip_size; // set clip_size_x or clip_size_y to 0 to disable clipping

.vector origin;

// function pointers
.void(void) init; 	// called once at object creation
.void(void) reinit;
.void(void)	destroy;
.void(void) mouse_enter;
.void(void) mouse_leave;
.void(void) refresh;
.void(void) action;
.void(void) draw;
.float(float keynr, float ascii) key; // if it returns TRUE, the key was processed by the function

// hidden function pointers - actually these are called by the manager
// and they call the normal ones (used to make controls more generic
.void(void) _reinit;	// called in performreinit
.void(void) _destroy;	// called when the item is removed -> menu_removeitem
.void(void) _mouse_enter;
.void(void) _mouse_leave;
.void(void) _refresh;
.void(void) _action;
.void(void) _draw;
.void(float keynr, float ascii) _key;

///////////////
// prototypes
///

// used for global managing
void(void) menu_init;
// used to reload everything mmanager related
void(void) menu_restart;
// loads all files the file lists consists of
void(void) menu_load;
// used to reset the menu states everytime the menu is activated
void(void) menu_performreinit;

// decide whether to toggle the menu
void(void) menu_toggle;

// use this to add a file to the file list
void(string file) menu_addfiletolist;
// these 2 functions are pretty private, so dont call them !
void(string file) menu_loadmenu;
void(void) menu_linkwindows;

void(void) menu_frame;
void(void) menu_draw;
void(float keynr, float ascii) menu_keydown;
void(void) menu_shutdown;

// used for menu handling
void(void) menu_loopnext;
void(void) menu_loopprev;
void(void) menu_selectnext;
void(void) menu_selectprev;
void(void) menu_selectup;
void(void) menu_selectdown;
void(void) menu_reselect;

void(entity menu, float setactive) menu_jumptowindow;
void(entity menu) menu_drawwindow;

// when selectalways is true, you can use menu_processmouse to return the last object in the
// menu list whose click rectangle fits to the cursor pos (although it only returns an item
// if that item allows events
void(entity par, float selectalways) menu_processmouse;

float(entity e) menu_hasevents;
float(entity e) menu_isvisible;
float(entity e) menu_selectable;

entity(string item_name) menu_getitem;

void(entity ent) menu_removeitem;

// history stuff
// MMANAGER_HISTORY
//.entity _prev;	<- points to the previous history element
//.entity _child; 	<- points to the old/calling selected item
//.entity _parent; 	<- points to the old active window
//.entity _next;	<- points to the item which the history is used for

// points to the last element of the history
entity menu_history;

void(entity ent) menu_pushhistory;
void(void) menu_pophistory;
float(entity ent) menu_verifyhistory;
void(void)	menu_clearhistory;