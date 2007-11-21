///////////////////////////////////////////////
// Custom Menu Header File
///////////////////////
// This file belongs to dpmod/darkplaces
// AK contains menu specific stuff that is made especially for dpmod
// AK this file is used e.g. for defining some special event functions
///////////////////////////////////////////////

/*Templates

Sliders with text and description:
----------------------------------

Template Params:

cccc - Name of the text (on the left of the menu)
nnn	- Name of the object group
ppp	- Parent of the object group
ccvv	- Name of the cvar
yypp	- y position

//////
// cccc
////
// Menu Button
{
	classname	ITEM_TEXTBUTTON
	name		ppp_nnn_text
	parent		ppp
	text		"cccc"
	pos			"0 yypp 0"
	//font_size	"10 10 0"
	alignment	16		// TEXT_ALIGN_LEFTPOS
	flag		256		// FLAG_AUTOSETCLICK
	key			dpmod_redirect_key	// redirects input to the child
}
// Slider
{
	classname	ITEM_SLIDER
	name		ppp_nnn_slider
	parent		ppp_nnn_text
	pos			"10 yypp 0"
	size		"100 8 0"
	//size		"100 10 0"
	flag		260		// FLAG_AUTOSETCLICK | FLAG_NOSELECT
	cvarname	"ccvv"
	cvartype	1		// CVAR_INT
	min_value	240
	max_value	1536
	step		25.92
	init		dpmod_cvar_slider
}
// Text
{
	classname 	ITEM_TEXT
	name		ppp_nnn_slidertext
	parent		ppp_nnn_text
	link		ppp_nnn_slider
	pos			"120 yypp 0"
	//font_size "10 10 0"
	maxlen		4
	init		dpmod_slidertext
}

Text switch with text :

cccc 	- name of it (and caption of the text)
ppp		- parent
nnn		- name of the object group
yypp	- y position of the object group
ccvv	- name of the cvar
ssss 	- switches 'No' 'Yes'

//////
// cccc
////
// Text
{
	classname 	ITEM_TEXTBUTTON
	name		ppp_nnn_text
	parent		ppp
	text		"cccc"
	pos			"0 yypp 0"
	//font_size	"10 10 0"
	alignment	16		// TEXT_ALIGN_RIGHTPOS
	flag		256		// FLAG_AUTOSETCLICK
	key			dpmod_redirect_key
}
// Switch
{
	classname	ITEM_TEXTSWITCH
	name		ppp_nnn_switch
	parent		ppp_nnn_text
	pos			"10 yypp 0"
	//font_size	"10 10 0"
	flag 		260		// FLAG_AUTOSETCLICK | FLAG_NOSELECT
	text		"ssss"
	cvarname	"ccvv"
	cvartype	1		// CVAR_INT
	reinit		dpmod_cvar_slider	// can use it also here
}

*/

// global stuff

.string cvarname;
.float cvartype;
//.string cvarvalues;

const float CVAR_FLOAT = 0;
const float CVAR_INT = 1;
//const float CVAR_STRING = 2;
const float CVAR_STEP = 4;

.entity _link;
.float 	maxlen;
void(void) dpmod_cvar_slider; // set reinit to this
void(void) _dpmod_cvar_slider_refresh;
void(void) _dpmod_cvar_slider;

float(float keynr, float ascii) dpmod_redirect_key;

void(void) dpmod_slidertext;
void(void) _dpmod_slidertext_refresh;

// option menu stuff
void(void) dpmod_options_alwaysrun_switchchange;
void(void) dpmod_options_alwaysrun_refresh;

void(void) dpmod_options_invmouse_switchchange;
void(void) dpmod_options_invmouse_refresh;

// quit message stuff
const float DPMOD_QUIT_MSG_COUNT = 4;

string dpmod_quitmsg[DPMOD_QUIT_MSG_COUNT] =
{
"'Tired of fragging''already ?'",
"'Quit now and forfeit''your bodycount ?'",
"'Are you sure you''want to quit ?'",
"'Off to do something''constructive ?'"
};

float dpmod_quitrequest;

void(void) dpmod_quit_choose;
void(void) dpmod_quit;
void(void) dpmod_quit_yes;
void(void) dpmod_quit_no;
float(float keynr, float ascii) dpmod_quit_key;