///////////////////////////////////////////////
// Cursor Source File
///////////////////////
// This file belongs to dpmod/darkplaces
// AK contains all cursor specific functions
///////////////////////////////////////////////

void(void) cursor_reset =
{
	cursor_x = rint(GFX_WIDTH /2);
	cursor_y = rint(GFX_HEIGHT /2);

	cursor_rel = '0 0 0';

	cursor_last_frame_time = time;
	cursor_color = CURSOR_COLOR;
	cursor_transparency = CURSOR_TRANSPARENCY;
	cursor_type = CT_NORMAL;
};

void(void) cursor_toggle =
{
	cursor_x = rint(GFX_WIDTH /2);
	cursor_y = rint(GFX_HEIGHT /2);
	cursor_type = CT_NORMAL;
};

void(void) cursor_init =
{
	// load the images
	if(CF_NORMAL != "") CF_NORMAL = gfx_loadpic(CF_NORMAL, CURSOR_ENFORCELOADING);

	if(CF_NORMAL == "") CF_NORMAL = gfx_loadpic("ui/mousepointer.tga", true); // always

	if(CF_PULSE[0] != "") CF_PULSE[0] = gfx_loadpic(CF_PULSE[0], CURSOR_ENFORCELOADING);
	if(CF_PULSE[1] != "") CF_PULSE[1] = gfx_loadpic(CF_PULSE[1], CURSOR_ENFORCELOADING);
	if(CF_PULSE[2] != "") CF_PULSE[2] = gfx_loadpic(CF_PULSE[2], CURSOR_ENFORCELOADING);
	if(CF_PULSE[3] != "") CF_PULSE[3] = gfx_loadpic(CF_PULSE[3], CURSOR_ENFORCELOADING);
	if(CF_PULSE[4] != "") CF_PULSE[4] = gfx_loadpic(CF_PULSE[4], CURSOR_ENFORCELOADING);
	if(CF_PULSE[5] != "") CF_PULSE[5] = gfx_loadpic(CF_PULSE[5], CURSOR_ENFORCELOADING);
	if(CF_PULSE[6] != "") CF_PULSE[6] = gfx_loadpic(CF_PULSE[6], CURSOR_ENFORCELOADING);

	// init the values
	cursor_reset();
};

void(void) cursor_shutdown =
{
	if(CF_NORMAL != "") gfx_unloadpic(CF_NORMAL);

	if(CF_PULSE[0] != "") gfx_unloadpic(CF_PULSE[0]);
	if(CF_PULSE[1] != "") gfx_unloadpic(CF_PULSE[1]);
	if(CF_PULSE[2] != "") gfx_unloadpic(CF_PULSE[2]);
	if(CF_PULSE[3] != "") gfx_unloadpic(CF_PULSE[3]);
	if(CF_PULSE[4] != "") gfx_unloadpic(CF_PULSE[4]);
	if(CF_PULSE[5] != "") gfx_unloadpic(CF_PULSE[5]);
	if(CF_PULSE[6] != "") gfx_unloadpic(CF_PULSE[6]);
};

void(void) cursor_frame =
{
	// update cursor animations

	if(cursor_type > CT_LAST_PULSE || cursor_type  < CT_FIRST_PULSE)
		cursor_last_frame_time = time;
	else if(cursor_last_frame_time + CA_PULSE_SPEED <= time)
		{
		cursor_type = CT_FIRST_PULSE +
		mod((time - cursor_last_frame_time) / CA_PULSE_SPEED, CT_LAST_PULSE - CT_FIRST_PULSE +1);

		cursor_last_frame_time += rint((time - cursor_last_frame_time) / CA_PULSE_SPEED) * CA_PULSE_SPEED;
		}

	// update cursor position
	cursor_rel = getmousepos();
	cursor_rel = gfx_converttogfx(cursor_rel);
	// cursor speed, etc
	cursor_rel = cursor_rel * CURSOR_SPEED;
	cursor = cursor + cursor_rel;

	cursor_x = bound(0, cursor_x, GFX_WIDTH);
	cursor_y = bound(0, cursor_y, GFX_HEIGHT);
};

void(void) cursor_draw =
{
	string pic;

	if(cursor_type == CT_FIRST_PULSE + 0 && CF_PULSE[0] != "") pic = CF_PULSE[0];
	else if(cursor_type == CT_FIRST_PULSE + 1 && CF_PULSE[1] != "") pic = CF_PULSE[1];
	else if(cursor_type == CT_FIRST_PULSE + 2 && CF_PULSE[2] != "") pic = CF_PULSE[2];
	else if((cursor_type == CT_FIRST_PULSE + 3 || cursor_type == CT_GLOW) && CF_PULSE[3] != "") pic = CF_PULSE[3];
	else if(cursor_type == CT_FIRST_PULSE + 4 && CF_PULSE[4] != "") pic = CF_PULSE[4];
	else if(cursor_type == CT_FIRST_PULSE + 5 && CF_PULSE[5] != "") pic = CF_PULSE[5];
	else if(cursor_type == CT_FIRST_PULSE + 6 && CF_PULSE[6] != "") pic = CF_PULSE[6];
	else // if(cursor_tpye = CT_NORMAL)
		pic = CF_NORMAL;

	gfx_drawpic(cursor, pic, gfx_getimagesize(pic) * CURSOR_SCALE, cursor_color, cursor_transparency, 0);
};