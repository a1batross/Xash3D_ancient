///////////////////////////////////////////////
// Controls/Item Source File
///////////////////////
// This file belongs to dpmod/darkplaces
// AK contains all item specific stuff
////////////////////////////////////////////////

////////////////
// ITEM_WINDOW
//

void(void) ITEM_WINDOW =
{
	pev.flag = pev.flag | FLAG_NOSELECT | FLAG_DRAWONLY;

	item_init(
		defct_reinit,
		defct_destroy,
		defct_key,
		defct_draw,
		defct_mouse_enter,
		defct_mouse_leave,
		defct_action,
		defct_refresh);

	ctcall_init();
};

//////////////////
// ITEM_REFERENCE
///

void(void) ITEM_REFERENCE =
{
	pev.flag = pev.flag | FLAG_NOSELECT | FLAG_DRAWONLY;

	if(pev.link == "")
	{
		remove(pev);	// no need to call terminate
		return;
	}

		pev._child = menu_getitem(pev.link);
	if(pev._child == null_entity)
	{
		print(pev.name, " removed, cause link ", pev.link, " not found\n");
		remove(pev);	// no need to call terminate
		return;
	}

	item_init(
		defct_reinit,
		defct_destroy,
		defct_key,
		defct_draw,
		defct_mouse_enter,
		defct_mouse_leave,
		defct_action,
		defct_refresh);

	ctcall_init();
};

////////////////
// ITEM_CUSTOM
////

void(void) ITEM_CUSTOM =
{
	item_init(defct_reinit,	defct_destroy, defct_key, defct_draw, defct_mouse_enter, defct_mouse_leave, defct_action, defct_refresh);
	ctcall_init();
};

/////////////////
// ITEM_PICTURE
///

// ITEM_PICTURE has a special draw function
void(void) ITEM_PICTURE_DRAW =
{
	// align to the rect pos - (pos + size)
	vector alignpos;

	// now check the alignement
	if(pev.alignment & TEXT_ALIGN_CENTER)
	{
		alignpos_x = pev.pos_x + (pev.pos_x - pev.size_x) / 2;
		alignpos_y = pev.pos_y + (pev.pos_y - pev.size_y) / 2;
	}
	else alignpos = pev.pos;

	menu_drawpic(alignpos, pev.picture, pev.size, pev.color, pev.alpha, pev.drawflag);
	ctcall_draw();
};

void(void) ITEM_PICTURE_DESTROY =
{
	gfx_unloadpic(pev.picture);

	ctcall_destroy();
};

void(void) ITEM_PICTURE =
{
	if(pev.picture == "")
		// a picture has to have a picture
		remove(pev);		// no need to call terminate

	// load the picture if it isnt loaded already
	gfx_loadpic(pev.picture, MENU_ENFORCELOADING);

	// if flag wasnt set yet, then set it to FLAG_DRAWONLY
	if(pev.flag == 0) pev.flag = FLAG_DRAWONLY;

	if(pev.color == '0 0 0') pev.color = ITEM_PICTURE_NORMAL_COLOR;
	if(pev.alpha == 0) pev.alpha = ITEM_PICTURE_NORMAL_ALPHA;

	item_init(
		defct_reinit,
		ITEM_PICTURE_DESTROY,
		defct_key,
		ITEM_PICTURE_DRAW,
		defct_mouse_enter,
		defct_mouse_leave,
		defct_action,
		defct_refresh);

	ctcall_init();
};

/////////////
// ITEM_TEXT
///

void(void) ITEM_TEXT_REFRESH =
{
	// first do own refresh, *then* call the default refresh !
	if(pev.size == '0 0 0')
	{
		if(pev.font_size == '0 0 0')
			pev.font_size = ITEM_TEXT_FONT_SIZE;

		pev.size_x = pev.font_size_x * strlen(pev.text);
		pev.size_y = pev.font_size_y;
	} else if(pev.font_size == '0 0 0')
	{
			pev.font_size_x = pev.size_x / strlen(pev.text);
			pev.font_size_y = pev.size_y;
	}

	def_refresh();
	ctcall_refresh();
};

void(void) ITEM_TEXT_DRAW =
{
	if(pev.text)
	{
		// align to the rect pos - (pos + size)
		vector alignpos;
		// now check the alignement
		if(pev.alignment & TEXT_ALIGN_CENTER)
			alignpos_x = pev.pos_x + (pev.size_x - strlen(pev.text) * pev.font_size_x) / 2;
		else if(pev.alignment & TEXT_ALIGN_RIGHT)
			alignpos_x = pev.pos_x + pev.size_x - strlen(pev.text) * pev.font_size_x;
		else
			alignpos_x = pev.pos_x;
		alignpos_y = pev.pos_y;

		menu_drawstring(alignpos, pev.text, pev.font_size, pev.color, pev.alpha, pev.drawflag);
	}
	ctcall_draw();
};

void(void) ITEM_TEXT =
{
	if(pev.flag == 0)
		pev.flag = FLAG_DRAWONLY;

	if(pev.color == '0 0 0')
		pev.color = ITEM_TEXT_NORMAL_COLOR;
	if(pev.alpha == 0)
		pev.alpha = ITEM_TEXT_NORMAL_ALPHA;

	ITEM_TEXT_REFRESH();
	if(pev.alignment & TEXT_ALIGN_CENTERPOS)
	{
		pev.pos_x = pev.pos_x - pev.size_x / 2;
	} else	if(pev.alignment & TEXT_ALIGN_LEFTPOS)
	{
		pev.pos_x = pev.pos_x - pev.size_x;
	}

	item_init(
		defct_reinit,
		defct_destroy,
		defct_key,
		ITEM_TEXT_DRAW,
		defct_mouse_enter,
		defct_mouse_leave,
		defct_action,
		ITEM_TEXT_REFRESH);

	ctcall_init();
};

/////////////////
// ITEM_RECTANLE
///

void(void) ITEM_RECTANGLE_DRAW =
{
	menu_fillarea(pev.pos, pev.size, pev.color, pev.alpha, pev.drawflag);
};

void(void) ITEM_RECTANGLE =
{
	if(pev.flag == 0)
		pev.flag = FLAG_DRAWONLY;

	item_init(
		defct_reinit,
		defct_destroy,
		defct_key,
		ITEM_RECTANGLE_DRAW,
		defct_mouse_enter,
		defct_mouse_leave,
		defct_action,
		defct_refresh);

	ctcall_init();
};

////////////////
// ITEM_BUTTON
///

void(void) ITEM_BUTTON_DRAW =
{
	if(pev._button_state == BUTTON_NORMAL)
		menu_drawpic(pev.pos, pev.picture, pev.size, pev.color, pev.alpha, pev.drawflag);
	else if(pev._button_state == BUTTON_SELECTED)
		menu_drawpic(pev.pos, pev.picture_selected, pev.size, pev.color_selected, pev.alpha_selected, pev.drawflag_selected);
	else
		menu_drawpic(pev.pos, pev.picture_pressed, pev.size, pev.color_pressed, pev.alpha_pressed, pev.drawflag_pressed);

	ctcall_draw();
};

void(void) ITEM_BUTTON_REFRESH =
{

	if((pev.hold_pressed + pev._press_time < time && pev._button_state == BUTTON_PRESSED) || (menu_selected != pev && pev._button_state == BUTTON_SELECTED))
	{
		pev._button_state = BUTTON_NORMAL;
	}
	if(menu_selected == pev && pev._button_state == BUTTON_NORMAL)
	{
		pev._button_state = BUTTON_SELECTED;
		if(pev.sound_selected)
			snd_play(pev.sound_selected);
	}
	def_refresh();
	ctcall_refresh();
};

void(float keynr, float ascii) ITEM_BUTTON_KEY =
{
	if(ctcall_key(keynr, ascii))
		return;

	if(keynr == K_ENTER || keynr == K_MOUSE1)
	{
		pev._action();
	} 
	else def_keyevent(keynr, ascii);
};

void(void) ITEM_BUTTON_ACTION =
{
	pev._press_time = time;
	pev._button_state = BUTTON_PRESSED;
	if(pev.sound_pressed)
		snd_play(pev.sound_pressed);

	ctcall_action();
};
void(void) ITEM_BUTTON_REINIT =
{
	pev._button_state = BUTTON_NORMAL;

	ctcall_reinit();
};

void(void) ITEM_BUTTON_DESTROY =
{
	gfx_unloadpic(pev.picture);
	gfx_unloadpic(pev.picture_selected);
	gfx_unloadpic(pev.picture_pressed);

	ctcall_destroy();
};

void(void) ITEM_BUTTON =
{
	if(pev.picture == "" || pev.picture_selected == "" || pev.picture_pressed == "")
		// a picture has to have pictures
		remove(pev);	// no need to call terminate

	// load the picture if it isnt loaded already
	gfx_loadpic(pev.picture, MENU_ENFORCELOADING);
	gfx_loadpic(pev.picture_selected, MENU_ENFORCELOADING);
	gfx_loadpic(pev.picture_pressed, MENU_ENFORCELOADING);

	if(pev.sound_selected != "")
		snd_loadsound(pev.sound_selected, SOUND_ENFORCELOADING);
	else
		pev.sound_selected = SOUND_SELECT;

	if(pev.sound_pressed != "")
		snd_loadsound(pev.sound_pressed, SOUND_ENFORCELOADING);
	else
		pev.sound_pressed = SOUND_ACTION;

	// if flag wasnt set yet, then set it to FLAG_DRAWONLY
	if(pev.flag == 0)
		pev.flag = FLAG_AUTOSETCLICK;

	if(pev.color == '0 0 0')
		pev.color = ITEM_PICTURE_NORMAL_COLOR;
	if(pev.alpha == 0)
		pev.alpha = ITEM_PICTURE_NORMAL_ALPHA;
	if(pev.color_selected == '0 0 0')
		pev.color_selected = ITEM_PICTURE_SELECTED_COLOR;
	if(pev.alpha_selected == 0)
		pev.alpha_selected = ITEM_PICTURE_SELECTED_ALPHA;
	if(pev.color_pressed == '0 0 0')
		pev.color_pressed = ITEM_PICTURE_PRESSED_COLOR;
	if(pev.alpha_pressed == 0)
		pev.alpha_pressed = ITEM_PICTURE_PRESSED_ALPHA;

	if(pev.hold_pressed == 0)
		pev.hold_pressed = ITEM_BUTTON_HOLD_PRESSED;

	item_init(
		ITEM_BUTTON_REINIT,
		ITEM_BUTTON_DESTROY,
		ITEM_BUTTON_KEY,
		ITEM_BUTTON_DRAW,
		defct_mouse_enter,
		defct_mouse_leave,
		defct_action,
		ITEM_BUTTON_REFRESH);

	ctcall_init();
};

////////////////////
// ITEM_TEXTBUTTON
///

void(void) ITEM_TEXTBUTTON_REFRESH =
{
	// first do own refresh, *then* call the default refresh !
	if(pev.size == '0 0 0')
	{
		if(pev.font_size == '0 0 0')
			pev.font_size = ITEM_TEXT_FONT_SIZE;

		pev.size_x = pev.font_size_x * strlen(pev.text);
		pev.size_y = pev.font_size_y;
	}
	else if(pev.font_size == '0 0 0')
	{
		pev.font_size_x = pev.size_x / strlen(pev.text);
		pev.font_size_y = pev.size_y;
	}

	if((pev.hold_pressed + pev._press_time < time && pev._button_state == BUTTON_PRESSED) || (menu_selected != pev && pev._button_state == BUTTON_SELECTED))
	{
		pev._button_state = BUTTON_NORMAL;
	}
	if(menu_selected == pev && pev._button_state == BUTTON_NORMAL)
	{
		pev._button_state = BUTTON_SELECTED;
		if(pev.sound_selected)
			snd_play(pev.sound_selected);
	}

	def_refresh();
	ctcall_refresh();
};

void(void) ITEM_TEXTBUTTON_DRAW =
{
	if(pev.text == "")
		return;

	// align to the rect pos - (pos + size)
	vector alignpos;
	// now check the alignement
	if(pev.alignment & TEXT_ALIGN_CENTER)
		alignpos_x = pev.pos_x + (pev.size_x - strlen(pev.text) * pev.font_size_x) / 2;
	else if(pev.alignment & TEXT_ALIGN_RIGHT)
		alignpos_x = pev.pos_x + pev.size_x - strlen(pev.text) * pev.font_size_x;
	else alignpos_x = pev.pos_x;

	alignpos_y = pev.pos_y;

	if(pev.style == TEXTBUTTON_STYLE_OUTLINE && pev._button_state != BUTTON_NORMAL)
	{
		vector p,s;
		// left
		p_x = pev.pos_x;
		p_y = pev.pos_y;
		s_x = TEXTBUTTON_OUTLINE_WIDTH;
		s_y = pev.size_y;
		if(pev._button_state == BUTTON_PRESSED)
		{
			menu_fillarea(p, s, pev.color_pressed, pev.alpha_pressed, pev.drawflag_pressed);
		}
		else if(pev._button_state == BUTTON_SELECTED)
		{
			menu_fillarea(p, s, pev.color_selected, pev.alpha_selected, pev.drawflag_selected);
		}
		// right
		p_x = pev.pos_x + pev.size_x - TEXTBUTTON_OUTLINE_WIDTH;
		p_y = pev.pos_y;
		s_x = TEXTBUTTON_OUTLINE_WIDTH;
		s_y = pev.size_y;
		if(pev._button_state == BUTTON_PRESSED)
		{
			menu_fillarea(p, s, pev.color_pressed, pev.alpha_pressed, pev.drawflag_pressed);
		}
		else if(pev._button_state == BUTTON_SELECTED)
		{
			menu_fillarea(p, s, pev.color_selected, pev.alpha_selected, pev.drawflag_selected);
		}
		// top
		p_x = pev.pos_x;
		p_y = pev.pos_y;
		s_y = TEXTBUTTON_OUTLINE_WIDTH;
		s_x = pev.size_x;
		if(pev._button_state == BUTTON_PRESSED)
		{
			menu_fillarea(p, s, pev.color_pressed, pev.alpha_pressed, pev.drawflag_pressed);
		}
		else if(pev._button_state == BUTTON_SELECTED)
		{
			menu_fillarea(p, s, pev.color_selected, pev.alpha_selected, pev.drawflag_selected);
		}
		// bottom
		p_x = pev.pos_x;
		p_y = pev.pos_y + pev.size_y - TEXTBUTTON_OUTLINE_WIDTH;
		s_y = TEXTBUTTON_OUTLINE_WIDTH;
		s_x = pev.size_x;
		if(pev._button_state == BUTTON_PRESSED)
		{
			menu_fillarea(p, s, pev.color_pressed, pev.alpha_pressed, pev.drawflag_pressed);
		}
		else if(pev._button_state == BUTTON_SELECTED)
		{
			menu_fillarea(p, s, pev.color_selected, pev.alpha_selected, pev.drawflag_selected);
		}
	} 
	else if(pev.style == TEXTBUTTON_STYLE_BOX)
	{
		if(pev._button_state == BUTTON_PRESSED)
		{
			menu_fillarea(alignpos, pev.size, pev.color_pressed, pev.alpha_pressed, pev.drawflag_pressed);
		}
		else if(pev._button_state == BUTTON_SELECTED)
		{
			menu_fillarea(alignpos, pev.size, pev.color_selected, pev.alpha_selected, pev.drawflag_selected);
		}
	}

	if(pev._button_state == BUTTON_NORMAL || pev.style == TEXTBUTTON_STYLE_BOX || pev.style == TEXTBUTTON_STYLE_OUTLINE)
		menu_drawstring(alignpos, pev.text, pev.font_size, pev.color, pev.alpha, pev.drawflag);

	if(pev.style == TEXTBUTTON_STYLE_TEXT)
	{
		if(pev._button_state == BUTTON_PRESSED)
		{
			menu_drawstring(alignpos, pev.text, pev.font_size, pev.color_pressed, pev.alpha_pressed, pev.drawflag_pressed);
		}
		else if(pev._button_state == BUTTON_SELECTED)
		{
			menu_drawstring(alignpos, pev.text, pev.font_size, pev.color_selected, pev.alpha_selected, pev.drawflag_selected);
		}
	}

	ctcall_draw();
};

void(void) ITEM_TEXTBUTTON_ACTION =
{
	pev._press_time = time;
	pev._button_state = BUTTON_PRESSED;
	if(pev.sound_pressed)
		snd_play(pev.sound_pressed);

	ctcall_action();
};

void(float keynr, float ascii) ITEM_TEXTBUTTON_KEY =
{
	if(ctcall_key(keynr, ascii))
		return;

	if(keynr == K_ENTER || keynr == K_MOUSE1)
	{
		pev._action();
	} else
		def_keyevent(keynr, ascii);
};

void(void) ITEM_TEXTBUTTON_REINIT =
{
	pev._button_state = BUTTON_NORMAL;

	ctcall_reinit();
};

void(void) ITEM_TEXTBUTTON =
{
	if(pev.flag == 0)
		pev.flag = FLAG_AUTOSETCLICK;

	if(pev.color == '0 0 0')
		pev.color = ITEM_TEXT_NORMAL_COLOR;
	if(pev.alpha == 0)
		pev.alpha = ITEM_TEXT_NORMAL_ALPHA;
	if(pev.color_selected == '0 0 0')
		pev.color_selected = ITEM_TEXT_SELECTED_COLOR;
	if(pev.alpha_selected == 0)
		pev.alpha_selected = ITEM_TEXT_SELECTED_ALPHA;
	if(pev.color_pressed == '0 0 0')
		pev.color_pressed = ITEM_TEXT_PRESSED_COLOR;
	if(pev.alpha_pressed == 0)
		pev.alpha_pressed = ITEM_TEXT_PRESSED_ALPHA;

	if(pev.hold_pressed == 0)
		pev.hold_pressed = ITEM_BUTTON_HOLD_PRESSED;

	if(pev.sound_selected != "")
		snd_loadsound(pev.sound_selected, SOUND_ENFORCELOADING);
	else
		pev.sound_selected = SOUND_SELECT;

	if(pev.sound_pressed != "")
		snd_loadsound(pev.sound_pressed, SOUND_ENFORCELOADING);
	else
		pev.sound_pressed = SOUND_ACTION;

	ITEM_TEXTBUTTON_REFRESH();
	if(pev.alignment & TEXT_ALIGN_CENTERPOS)
	{
		pev.pos_x = pev.pos_x - pev.size_x / 2;
	} else	if(pev.alignment & TEXT_ALIGN_LEFTPOS)
	{
		pev.pos_x = pev.pos_x - pev.size_x;
	}

	item_init(
		ITEM_TEXTBUTTON_REINIT,
		defct_destroy,
		ITEM_TEXTBUTTON_KEY,
		ITEM_TEXTBUTTON_DRAW,
		defct_mouse_enter,
		defct_mouse_leave,
		ITEM_TEXTBUTTON_ACTION,
		ITEM_TEXTBUTTON_REFRESH);

	ctcall_init();
};

// ITEM_SLIDER

void(void) ITEM_SLIDER_DRAW =
{
	vector	slider_pos, temp;
	float	i, slider_range;

	// draw the bar
	if(pev.picture_bar != "")
	{
		menu_drawpic(pev.pos, pev.picture_bar, pev.size, pev.color, pev.alpha, pev.drawflag);
	}
	else
	{
		// Quake-Style slider
		menu_drawchar( pev.pos, 128, pev.slider_size, pev.color, pev.alpha, pev.drawflag );
		for ( i = 1; i < 10; i++ )
		{
			temp = pev.pos;
			temp_x = pev.pos_x + (i * pev.slider_size_x);
			temp_y = pev.pos_y;
			menu_drawchar( temp, 129, pev.slider_size, pev.color, pev.alpha, pev.drawflag );
		}
		menu_drawchar( temp, 130, pev.slider_size, pev.color, pev.alpha, pev.drawflag );
	}

	slider_range = (pev.value - pev.min_value) / (pev.max_value - pev.min_value);
	slider_range = bound( 0, slider_range, 1 ); // bound range

	// draw the slider
	slider_pos = pev.pos;          
          slider_pos_x = pev.pos_x + pev.slider_size_x + pev.slider_size_x * (slider_range * 7);//FIXME
	if(pev.picture != "") menu_drawpic(slider_pos, pev.picture, pev.slider_size, pev.color, pev.alpha, pev.drawflag);
	else menu_drawchar( slider_pos, 131, pev.slider_size, pev.color, pev.alpha, pev.drawflag );
};

void(void) ITEM_SLIDER_UPDATESLIDER =
{
	pev.value = bound(pev.min_value, pev.value, pev.max_value);
	if(pev.slidermove)
		pev.slidermove();
};

void(float keynr, float ascii) ITEM_SLIDER_KEY =
{
	if(ctcall_key(keynr, ascii))
		return;

	if(keynr == K_LEFTARROW)
	{
		pev.value = (rint(pev.value / pev.step) - 1) * pev.step;
		ITEM_SLIDER_UPDATESLIDER();
	}
	else if(keynr == K_RIGHTARROW)
	{
		pev.value = (rint(pev.value / pev.step) + 1)* pev.step;
		ITEM_SLIDER_UPDATESLIDER();
	}
	else if(keynr == K_MOUSE1)
	{
		if(inrect(menu_cursor, pev.pos, pev.size))
		{
			pev.value = pev.min_value + ((menu_cursor_x - pev.slider_size_x / 2) - pev.pos_x) * ((pev.max_value - pev.min_value) / (pev.size_x - pev.slider_size_x));
		}
	}
	else
	{
		def_keyevent(keynr, ascii);
		return;
	}
	// play sound
	snd_play(pev.sound_changed);
	ITEM_SLIDER_UPDATESLIDER();
};

void(void) ITEM_SLIDER_DESTROY =
{
	if(pev.picture != "")
		gfx_unloadpic(pev.picture);
	if(pev.picture_bar != "")
		gfx_unloadpic(pev.picture);

	ctcall_destroy();
};

void(void) ITEM_SLIDER =
{
	if(pev.picture != "")
		pev.picture = gfx_loadpic(pev.picture, MENU_ENFORCELOADING);
	if(pev.picture_bar != "")
		pev.picture_bar = gfx_loadpic(pev.picture_bar, MENU_ENFORCELOADING);
	if(pev.sound_changed == "")
		pev.sound_changed = SOUND_CHANGE;

	if(pev.color == '0 0 0')
		pev.color = ITEM_SLIDER_COLOR;
	if(pev.alpha == 0)
		pev.alpha = ITEM_SLIDER_ALPHA;
	if(pev.step == 0)
		pev.step = ITEM_SLIDER_STEP;
	if(pev.slider_size == '0 0 0')
	{
		if(pev.picture != "")
			pev.slider_size = gfx_getimagesize(pev.picture);
		else
			pev.slider_size = ITEM_SLIDER_SIZE;
	}

	item_init(
		defct_reinit,
		defct_destroy,
		ITEM_SLIDER_KEY,
		ITEM_SLIDER_DRAW,
		defct_mouse_enter,
		defct_mouse_leave,
		defct_action,
		defct_refresh);

	ctcall_init();
};

// ITEM_TEXTSWITCH

void(void) ITEM_TEXTSWITCH_DRAW =
{
	string temp;

	// get the current text
	temp = pev.text;
	pev.text = getaltstring(pev.value, pev.text);
	// call ITEM_TEXT
	ITEM_TEXT_DRAW();
	pev.text = temp;
};

void(void) ITEM_TEXTSWITCH_REFRESH =
{
	string temp;

	temp = pev.text;
	pev.text = getaltstring(pev.value, pev.text);

	ITEM_TEXT_REFRESH();

	pev.text = temp;
};

void(float keynr, float ascii)	ITEM_TEXTSWITCH_KEY =
{
	if(ctcall_key(keynr, ascii))
		return;

	if(keynr == K_LEFTARROW || keynr == K_MOUSE2)
	{
		pev.value = pev.value - 1;
		if(pev.value < 0)
			pev.value = getaltstringcount(pev.text) - 1;
	}
	else if(keynr == K_RIGHTARROW || keynr == K_MOUSE1 || keynr == K_ENTER)
	{
		pev.value = pev.value + 1;
		if(pev.value > getaltstringcount(pev.text) - 1)
			pev.value = 0;
	} else
	{
		def_keyevent(keynr, ascii);
		return;
	}
	snd_play(pev.sound_changed);
	if(pev.switchchange)
		pev.switchchange();
};

void(void) ITEM_TEXTSWITCH =
{
	string temp;

	if(pev.sound_changed != "")
		snd_loadsound(pev.sound_changed, SOUND_ENFORCELOADING);
	else
		pev.sound_changed = SOUND_CHANGE;

	temp = pev.text;
	pev.text = getaltstring(pev.value, pev.text);
	ITEM_TEXT();
	pev.text = temp;

	item_init(
		defct_reinit,
		defct_destroy,
		ITEM_TEXTSWITCH_KEY,
		ITEM_TEXTSWITCH_DRAW,
		defct_mouse_enter,
		defct_mouse_leave,
		defct_action,
		ITEM_TEXTSWITCH_REFRESH);
};

/*
/////////////////////////////////////////////////////////////////////////
// The "actual" funtions

void(void) ctinit_picture =
{

};

// draws a text (uses the std. way)
void(string text, vector pos, vector size, float alignment, float style, float state) ctdrawtext =
{
	vector alignpos;

	if(text == "")
		return;

	// align to the rect pos - (pos + size)

	// now check the alignement
	if(alignment & TEXT_ALIGN_CENTER)
		alignpos_x = pos_x + (size_x - strlen(text) * pev.font_size_x) / 2;
	else if(alignment & TEXT_ALIGN_RIGHT)
		alignpos_x = pos_x + size_x - strlen(text) * pev.font_size_x;
	else
		alignpos_x = pos_x;
		alignpos_y = pos_y;

	if(style == TEXTBUTTON_STYLE_OUTLINE && state != BUTTON_NORMAL)
	{
		vector p,s;
		// left
		p_x = pos_x;
		p_y = pos_y;
		s_x = TEXTBUTTON_OUTLINE_WIDTH;
		s_y = size_y;
		if(state == BUTTON_PRESSED)
		{
			menu_fillarea(p, s, pev.color_pressed, pev.alpha_pressed, pev.drawflag_pressed);
		}
		else if(state == BUTTON_SELECTED)
		{
			menu_fillarea(p, s, pev.color_selected, pev.alpha_selected, pev.drawflag_selected);
		}
		// right
		p_x = pos_x + size_x - TEXTBUTTON_OUTLINE_WIDTH;
		p_y = pos_y;
		s_x = TEXTBUTTON_OUTLINE_WIDTH;
		s_y = size_y;
		if(state == BUTTON_PRESSED)
		{
			menu_fillarea(p, s, pev.color_pressed, pev.alpha_pressed, pev.drawflag_pressed);
		}
		else if(state == BUTTON_SELECTED)
		{
			menu_fillarea(p, s, pev.color_selected, pev.alpha_selected, pev.drawflag_selected);
		}
		// top
		p_x = pos_x;
		p_y = pos_y;
		s_y = TEXTBUTTON_OUTLINE_WIDTH;
		s_x = size_x;
		if(state == BUTTON_PRESSED)
		{
			menu_fillarea(p, s, pev.color_pressed, pev.alpha_pressed, pev.drawflag_pressed);
		}
		else if(pev._button_state == BUTTON_SELECTED)
		{
			menu_fillarea(p, s, pev.color_selected, pev.alpha_selected, pev.drawflag_selected);
		}
		// bottom
		p_x = pos_x;
		p_y = pos_y + size_y - TEXTBUTTON_OUTLINE_WIDTH;
		s_y = TEXTBUTTON_OUTLINE_WIDTH;
		s_x = size_x;
		if(state == BUTTON_PRESSED)
		{
			menu_fillarea(p, s, pev.color_pressed, pev.alpha_pressed, pev.drawflag_pressed);
		}
		else if(state == BUTTON_SELECTED)
		{
			menu_fillarea(p, s, pev.color_selected, pev.alpha_selected, pev.drawflag_selected);
		}
	} 
	else if(style == TEXTBUTTON_STYLE_BOX)
	{
		if(state == BUTTON_PRESSED)
		{
			menu_fillarea(alignpos, size, pev.color_pressed, pev.alpha_pressed, pev.drawflag_pressed);
		}
		else if(pev._button_state == BUTTON_SELECTED)
		{
			menu_fillarea(alignpos, size, pev.color_selected, pev.alpha_selected, pev.drawflag_selected);
		}
	}

	if(state == BUTTON_NORMAL || style == TEXTBUTTON_STYLE_BOX || style == TEXTBUTTON_STYLE_OUTLINE)
		menu_drawstring(alignpos, text, pev.font_size, pev.color, pev.alpha, pev.drawflag);

	if(style == TEXTBUTTON_STYLE_TEXT)
	{
		if(state == BUTTON_PRESSED)
		{
			menu_drawstring(alignpos, text, pev.font_size, pev.color_pressed, pev.alpha_pressed, pev.drawflag_pressed);
		}
		else if(state == BUTTON_SELECTED)
		{
			menu_drawstring(alignpos, text, pev.font_size, pev.color_selected, pev.alpha_selected, pev.drawflag_selected);
		}
	}
};*/
