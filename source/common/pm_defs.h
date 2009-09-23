//=======================================================================
//			Copyright XashXT Group 2009 ©
//		        pm_defs.h - shared player movement code
//=======================================================================
#ifndef PM_DEFS_H
#define PM_DEFS_H

#define MAX_TOUCH_ENTS		32

typedef struct pmove_s
{
	// state (in / out)
	edict_t		ps;		// client state

	// command (in)
	usercmd_t		cmd;
	bool		snapinitial;	// if is has been changed outside pmove
	movevars_t	*movevars;	// sv.movevars
	
	// results (out)
	int		numtouch;
	edict_t		*touchents[MAX_TOUCH_ENTS];

	vec3_t		viewangles;	// clamped
	float		viewheight;

	vec3_t		mins, maxs;	// bounding box size

	edict_t		*groundentity;
	int		watertype;
	int		waterlevel;

	// engine toolbox
	trace_t		(*trace)( vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end );
	int		(*pointcontents)( vec3_t point );
};

#endif//PM_DEFS_H