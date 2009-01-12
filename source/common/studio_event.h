//=======================================================================
//			Copyright XashXT Group 2009 ©
//		     studio_event.h - model event definition
//=======================================================================
#ifndef STUDIO_EVENT_H
#define STUDIO_EVENT_H

typedef struct dstudioevent_s
{
	int 		frame;
	int		event;
	int		type;
	char		options[64];
} dstudioevent_t;

#endif//STUDIO_EVENT_H