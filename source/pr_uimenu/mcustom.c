///////////////////////////////////////////////
// Custom Menu Source File
///////////////////////
// This file belongs to dpmod/darkplaces
// AK contains menu specific stuff that is made especially for dpmod
// AK this file is used e.g. for defining some special event functions
////////////////////////////////////////////////

////////////////
// global stuff
///

void(void) dpmod_slidertext =
{
	entity ent;
	if(pev.link == "")
	{
		print("No link specified\n");
		eprint(pev);
		pev.init = null_function;
		return;
	}

	ent = menu_getitem(pev.link);
	if(ent == null_entity)
	{
		objerror("No link found for ", pev.link,"\n");
	}

	pev._link = ent;

	pev.flag = pev.flag | FLAG_DRAWREFRESHONLY;

	pev.refresh = _dpmod_slidertext_refresh;
};

void(void) _dpmod_slidertext_refresh =
{
	pev.text = ftos(pev._link.value);
	if(pev.maxlen > 0)
		pev.text = substring(pev.text,0, pev.maxlen);
	// reset the size, so its set
	pev.size = '0 0 0';
};

float(float keynr, string ascii) dpmod_redirect_key =
{
	if(keynr == K_ENTER || keynr == K_LEFTARROW || keynr == K_RIGHTARROW || (keynr >= K_MOUSE1 && keynr <= K_MOUSE3))
	{
		raise_key(pev._child, keynr, ascii);
		return true;
	}
	return false;
};

void(void) dpmod_cvar_slider =
{
	pev.value = cvar(pev.cvarname);
	pev.slidermove = pev.switchchange = _dpmod_cvar_slider;
	pev.refresh = _dpmod_cvar_slider_refresh;
};

void(void) _dpmod_cvar_slider_refresh =
{
	if(pev.cvartype == CVAR_INT || pev.cvartype == CVAR_FLOAT || pev.cvartype == CVAR_STEP)
		pev.value = cvar(pev.cvarname);
};

void(void) _dpmod_cvar_slider =
{
	if(pev.cvarname == "")
		return;
	if(pev.cvartype == CVAR_INT) // || pev.cvartype == CVAR_STRING)
		pev.value = rint(pev.value);
	if(pev.cvartype == CVAR_STEP)
		pev.value = rint(pev.value / pev.step) * pev.step;
	if(pev.cvartype == CVAR_INT || pev.cvartype == CVAR_FLOAT || pev.cvartype == CVAR_STEP)
		cvar_set(pev.cvarname, ftos(pev.value));
	/*if(cvartype == CVAR_STRING)
	{
		string s;
		s = getaltstring(pev.value, pev.cvarvalues);
		cvar_set(pev.cvarname, s);
	}
	*/
};

//////////////
// main.menu
///

// display the options menu

void(void) dpmod_display_options =
{
	entity ent;
	ent = menu_getitem("options");
	menu_jumptowindow(ent, true);
};

// display the options menu

void(void) dpmod_display_video =
{
	entity ent;
	ent = menu_getitem("video");
	menu_jumptowindow(ent, true);
};

void vid_apply_changes( void )
{
	cmd( "vid_restart\n" );
	menu_selectup();
}

void vid_cancel_changes( void )
{
	menu_selectup();
}

// quit menu
void(void) dpmod_quit_choose =
{
	entity e;
	// because of the missing support for real array, we have to do it the stupid way
	// (we also have to use strzone for the text, cause it the temporary strings wont work
	// for it)
	e = menu_getitem("quit_msg_0");
	e.text = getaltstring(0, dpmod_quitmsg[dpmod_quitrequest]);
	e.text = strzone(e.text);

	e = menu_getitem("quit_msg_1");
	e.text = getaltstring(1, dpmod_quitmsg[dpmod_quitrequest]);
	e.text = strzone(e.text);

	dpmod_quitrequest++;
	if(dpmod_quitrequest == DPMOD_QUIT_MSG_COUNT)
		dpmod_quitrequest = 0;
};

void(void) dpmod_quit =
{
	entity ent;
	// choose a quit message
	dpmod_quit_choose();

	// change the flags
	ent = menu_getitem("main");
	ent.flag = ent.flag | FLAG_CHILDDRAWONLY;
	ent = menu_getitem("quit");
	ent.flag = FLAG_NOSELECT;
	menu_jumptowindow(ent, false);
};

void(void) dpmod_quit_yes =
{
	cmd("quit\n");
};

void(void) dpmod_quit_no =
{
	entity ent;

	ent = menu_getitem("quit_msg_0");
	strunzone(ent.text);

	ent = menu_getitem("quit_msg_1");
	strunzone(ent.text);

	ent = menu_getitem("quit");
	ent.flag = FLAG_HIDDEN;
	ent = menu_getitem("main");
	ent.flag = ent.flag - FLAG_CHILDDRAWONLY;
	menu_selectup();
};

float(float keynr, string ascii) dpmod_quit_key =
{
	if(keynr == K_LEFTARROW)
		return false;
	if(keynr == K_RIGHTARROW)
		return false;
	if(keynr == K_ENTER)
		return false;
	if(keynr == K_MOUSE1)
		return false;
	if(ascii == "Y" || ascii == "y")
		dpmod_quit_yes();
	if(ascii == "N" || ascii == "n" || keynr == K_ESCAPE)
		dpmod_quit_no();
	return true;
};

/////////////////
// options.menu
///

void(void) dpmod_options_alwaysrun_switchchange =
{
	if(pev.value)
	{
		cvar_set("cl_forwardspeed","400");
		cvar_set("cl_backspeed","400");
	}
	else
	{
		cvar_set("cl_forwardspeed","200");
		cvar_set("cl_backspeed","200");
	}
};

void(void) dpmod_options_alwaysrun_refresh =
{
	if(cvar("cl_forwardspeed") > 200)
		pev.value = 1;
	else
		pev.value = 0;
};

void(void) dpmod_options_invmouse_switchchange =
{
	float old;
	old = 0 - cvar("m_pitch");
	cvar_set("m_pitch",ftos(old));
};

void(void) dpmod_options_invmouse_refresh =
{
	if(cvar("m_pitch") > 0)
		pev.value = 0;
	else
		pev.value = 1;
};

////////////////////////////////////////////////////
// Test Stuff
///
void(void) initbrightness =
{
	pev.value = cvar("scr_conbrightness");
};

void(void) setbrightness =
{
	cvar_set("scr_conbrightness",ftos(pev.value));
};

void(void)	dpmod_main_exit =
{
	entity e;
	e = menu_getitem("MAIN_MENU");
	e.flag = FLAG_NOSELECT + FLAG_CHILDDRAWONLY;
	e = menu_getitem("MAIN_EXIT_MENU");
	e.flag = FLAG_NOSELECT;
	menu_jumptowindow(e, false);
};

void(void) dpmod_main_exit_no =
{
	entity e;
	e = menu_getitem("MAIN_EXIT_MENU");
	e.flag = FLAG_NOSELECT + FLAG_HIDDEN;
	e = menu_getitem("MAIN_MENU");
	e.flag = FLAG_NOSELECT;
	menu_selectup();
};

void(void) dpmod_main_exit_yes =
{
	cmd("quit\n");
};

float(float keynr, string ascii) dpmod_main_exit_key =
{
	if(keynr == K_ESCAPE)
	{
		dpmod_main_exit_no();
		return true;
	}
	return false;
}

void(void) 	dorestart =
{
	cmd("menu_restart\n");
};