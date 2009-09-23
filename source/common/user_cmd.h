//=======================================================================
//			Copyright XashXT Group 2009 ©
//		        user_cmd.h - usercmd communication
//=======================================================================
#ifndef USER_CMD_H
#define USER_CMD_H

// usercmd_t communication (a part of network protocol)
typedef struct usercmd_s
{
	int		msec;
	long		servertime;
	vec3_t		angles;
	int		forwardmove;
	int		sidemove;
	int		upmove;
	int		buttons;
};

#endif//USER_CMD_H