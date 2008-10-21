//=======================================================================
//			Copyright XashXT Group 2008 ©
//		      launch_api.h - main header for all dll's
//=======================================================================
#ifndef LAUNCH_APH_H
#define LAUNCH_APH_H

// platform states
typedef enum
{	
	HOST_OFFLINE = 0,	// host_init( g_Instance ) same much as:
	HOST_CREDITS,	// "splash"	"©anyname"	(easter egg)
	HOST_DEDICATED,	// "normal"	"#gamename"
	HOST_NORMAL,	// "normal"	"gamename"
	HOST_BSPLIB,	// "bsplib"	"bsplib"
	HOST_QCCLIB,	// "qcclib"	"qcc"
	HOST_SPRITE,	// "sprite"	"spritegen"
	HOST_STUDIO,	// "studio"	"studiomdl"
	HOST_WADLIB,	// "wadlib"	"xwad"
	HOST_RIPPER,	// "ripper"	"extragen"
	HOST_VIEWER,	// "viewer"	"viewer"
	HOST_COUNTS,	// terminator
} instance_t;

#endif//LAUNCH_APH_H