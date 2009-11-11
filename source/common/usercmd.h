//=======================================================================
//			Copyright XashXT Group 2009 ©
//		        usercmd.h - usercmd communication
//=======================================================================
#ifndef USERCMD_H
#define USERCMD_H

// usercmd_t communication (a part of network protocol)
typedef struct usercmd_s
{
	int		msec;
	long		servertime;
	vec3_t		angles;
	float		forwardmove;
	float		sidemove;
	float		upmove;
	int		buttons;
} usercmd_t;

#endif//USERCMD_H