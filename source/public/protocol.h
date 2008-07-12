//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        protocol.h - engine network protocol
//=======================================================================
#ifndef PROTOCOL_H
#define PROTOCOL_H

#define USE_COORD_FRAC

#define MAX_MSGLEN			1600		// max length of network message

// network precision coords factor
#ifdef USE_COORD_FRAC
	#define SV_COORD_FRAC	(8.0f / 1.0f)
	#define CL_COORD_FRAC	(1.0f / 8.0f)
#else
	#define SV_COORD_FRAC	1.0f
	#define CL_COORD_FRAC	1.0f
#endif

#define SV_ANGLE_FRAC		(360.0f / 1.0f )
#define CL_ANGLE_FRAC		(1.0f / 360.0f )

typedef enum { NA_NONE, NA_LOOPBACK, NA_BROADCAST, NA_IP, NA_IPX, NA_BROADCAST_IPX } netadrtype_t;
typedef enum { NS_CLIENT, NS_SERVER } netsrc_t;

typedef struct
{
	netadrtype_t	type;
	byte		ip[4];
	byte		ipx[10];
	word		port;
} netadr_t;

typedef struct sizebuf_s
{
	bool	overflowed;	// set to true if the buffer size failed

	byte	*data;
	int	maxsize;
	int	cursize;
	int	readcount;
	int	errorcount;	// cause by errors
	int	bit;		// for bitwise reads and writes
} sizebuf_t;


#endif//PROTOCOL_H