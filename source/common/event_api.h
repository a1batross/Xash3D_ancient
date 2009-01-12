//=======================================================================
//			Copyright XashXT Group 2009 ©
//		     event_api.h - network event definition
//=======================================================================
#ifndef EVENT_API_H
#define EVENT_API_H


#define FEV_NOTHOST		(1<<0)	// skip local host for event send.    

// Send the event reliably.  You must specify the origin and angles and use
// PLAYBACK_EVENT_FULL for this to work correctly on the server for anything
// that depends on the event origin/angles.  I.e., the origin/angles are not
// taken from the invoking edict for reliable events.
#define FEV_RELIABLE	(1<<1)	 

// Don't restrict to PAS/PVS, send this event to _everybody_ on the server
// ( useful for stopping CHAN_STATIC sounds started by client event when client
// is not in PVS anymore ( hwguy in TFC e.g. ).
#define FEV_GLOBAL		(1<<2)

// If this client already has one of these events in its queue,
// just update the event instead of sending it as a duplicate
#define FEV_UPDATE		(1<<3)
#define FEV_HOSTONLY	(1<<4)	// only send to entity specified as the invoker
#define FEV_SERVER		(1<<5)	// only send if the event was created on the server.
#define FEV_CLIENT		(1<<6)	// only issue event client side ( from shared code )

#define FEVENT_ORIGIN	(1<<0)	// event was invoked with stated origin
#define FEVENT_ANGLES	(1<<1)	// event was invoked with stated angles

typedef struct event_args_s
{
	int	flags;
	int	entindex;		// transmitted always

	float	origin[3];
	float	angles[3];
	float	velocity[3];

	int	ducking;
	float	fparam1;
	float	fparam2;

	int	iparam1;
	int	iparam2;
	int	bparam1;
	int	bparam2;
} event_args_t;

#endif//EVENT_API_H