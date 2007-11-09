//=======================================================================
//			Copyright XashXT Group 2007 ©
//			options.h - editor saved options
//=======================================================================
#ifndef OPTIONS_H
#define OPTIONS_H

#define IDEDITORHEADER	(('V'<<24)+('R'<<16)+('D'<<8)+'I') // little-endian "IDRV"

typedef struct wnd_options_s
{
	int	id;	//must be "IDRV"
	size_t	csize;
	float	con_scale;
	float	exp_scale;
	bool	show_console;

	// system font variables
	int	font_size;//console font size
	char	fontname[32]; //default as "Courier"
	int	font_type;
	int	font_color;

	int	width;
	int	height;
	
} wnd_options_t;

#endif//OPTIONS_H