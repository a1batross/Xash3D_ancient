//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        protocol.h - engine network protocol
//=======================================================================
#ifndef PROTOCOL_H
#define PROTOCOL_H

#define USE_COORD_FRAC

#define MAXEDICT_BITS		12		// 4096 entity per one map
#define MAX_EDICTS			(1<<MAXEDICT_BITS)	// must change protocol to increase more
#define MAX_DATAGRAM		1400		// datagram size
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

typedef enum { NA_BAD, NA_LOOPBACK, NA_BROADCAST, NA_IP, NA_IPX, NA_BROADCAST_IPX } netadrtype_t;
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
	bool	huffman;		// using huffman compression

	byte	*data;
	int	maxsize;
	int	cursize;
	int	readcount;
	int	errorcount;	// cause by errors
	int	bit;		// for bitwise reads and writes
} sizebuf_t;

// communication state description
typedef struct net_field_s
{
	char	*name;
	int	offset;
	int	bits;
} net_field_t;

// entity_state_t communication
typedef struct entity_state_s
{
	uint		number;		// edict index

	vec3_t		origin;
	vec3_t		angles;
	vec3_t		old_origin;	// for lerping animation
	int		modelindex;
	int		soundindex;
	int		weaponmodel;

	int		skin;		// skin for studiomodels
	float		frame;		// % playback position in animation sequences (0..512)
	int		body;		// sub-model selection for studiomodels
	int		sequence;		// animation sequence (0 - 255)
	uint		effects;		// PGM - we're filling it, so it needs to be unsigned
	int		renderfx;
	int		solid;		// for client side prediction, 8*(bits 0-4) is x/y radius
					// 8*(bits 5-9) is z down distance, 8(bits10-15) is z up
					// gi.linkentity sets this properly
	float		alpha;		// alpha value
	float		animtime;		// auto-animating time

} entity_state_t;

// viewmodel state
typedef struct vmodel_state_s
{
	int		index;	// client modelindex
	vec3_t		angles;	// can be some different with viewangles
	vec3_t		offset;	// center offset
	int		sequence;	// studio animation sequence
	float		frame;	// studio frame
	int		body;	// weapon body
	int		skin;	// weapon skin 
} vmodel_state_t;

// thirdperson model state
typedef struct pmodel_state_s
{
	int		index;	// client modelindex
	int		sequence;	// studio animation sequence
	float		frame;	// studio frame
} pmodel_state_t;

// player_state_t communication
typedef struct player_state_s
{
	int		bobcycle;		// for view bobbing and footstep generation
	float		bobtime;
	int		pm_type;		// player movetype
	int		pm_flags;		// ducked, jump_held, etc
	int		pm_time;		// each unit = 8 ms
	
	int		origin[3];
	int		velocity[3];
	vec3_t		delta_angles;	// add to command angles to get view direction
	int		gravity;		// gravity value
	int		speed;		// maxspeed
	edict_t		*groundentity;	// current ground entity
	int		viewheight;	// height over ground
	int		effects;		// copied to entity_state_t->effects
	vec3_t		viewangles;	// for fixed views
	vec3_t		viewoffset;	// add to pmovestate->origin
	vec3_t		kick_angles;	// add to view direction to get render angles
	vec3_t		oldviewangles;	// for lerping viewmodel position
	vec4_t		blend;		// rgba full screen effect
	int		stats[32];	// integer limit
	float		fov;		// horizontal field of view

	// player model and viewmodel
	vmodel_state_t	vmodel;
	pmodel_state_t	pmodel;

} player_state_t;

// usercmd_t communication
typedef struct usercmd_s
{
	byte		msec;
	short		angles[3];
	short		forwardmove;
	short		sidemove;
	short		upmove;
	byte		buttons;
	byte		impulse;
	byte		lightlevel;

} usercmd_t;

// 0 bits invoke to sending half or full float ( 13 or 31 bit )
#define ENT_FIELD(x) #x,(int)&((entity_state_t*)0)->x
#define PS_FIELD(x) #x,(int)&((player_state_t*)0)->x

static net_field_t ent_fields[] =
{
{ ENT_FIELD(number), MAXEDICT_BITS },		// 4096 ents
{ ENT_FIELD(origin[0]), 0 },
{ ENT_FIELD(origin[1]), 0 },
{ ENT_FIELD(origin[2]), 0 },
{ ENT_FIELD(angles[0]), 0 },
{ ENT_FIELD(angles[1]), 0 },
{ ENT_FIELD(angles[2]), 0 },
{ ENT_FIELD(old_origin[0]), 0 },
{ ENT_FIELD(old_origin[1]), 0 },
{ ENT_FIELD(old_origin[2]), 0 },
{ ENT_FIELD(modelindex), MAXEDICT_BITS },	// 4096 models
{ ENT_FIELD(soundindex), 10 },		// 512 sounds ( OpenAL software limit is 255 )
{ ENT_FIELD(weaponmodel), MAXEDICT_BITS },	// 4096 models
{ ENT_FIELD(skin), 8 },			// 255 skins
{ ENT_FIELD(frame), 0 },			// interpolate value
{ ENT_FIELD(body), 8 },			// 255 bodies
{ ENT_FIELD(sequence), 10 },			// 1024 sequences
{ ENT_FIELD(effects), 32 },			// effect flags
{ ENT_FIELD(renderfx), 32 },			// renderfx flags
{ ENT_FIELD(solid), 24 },			// encoded mins/maxs
{ ENT_FIELD(alpha), 0 },			// alpha value (FIXME: send a byte ? )
{ ENT_FIELD(animtime), 0 },			// auto-animating time
};

static net_field_t ps_fields[] =
{
{ PS_FIELD(pm_type), 4 },			// 16 player movetypes allowed
{ PS_FIELD(pm_flags), 16 },			// 16 movetype flags allowed
{ PS_FIELD(pm_time), 8 },			// each unit 8 msec
{ PS_FIELD(origin[0]), 32 },
{ PS_FIELD(origin[1]), 32 },
{ PS_FIELD(origin[2]), 32 },
{ PS_FIELD(velocity[0]), 32 },
{ PS_FIELD(velocity[1]), 32 },
{ PS_FIELD(velocity[2]), 32 },
{ PS_FIELD(delta_angles[0]), 0 },
{ PS_FIELD(delta_angles[1]), 0 },
{ PS_FIELD(delta_angles[2]), 0 },
{ PS_FIELD(gravity), 16 },			// may be 12 bits ?
{ PS_FIELD(speed), 16 },
{ PS_FIELD(viewheight), -8 },			// why hegative ?
{ PS_FIELD(effects), 32 },			// copied to entity_state_t->effects
{ PS_FIELD(viewangles[0]), 0 },		// for fixed views
{ PS_FIELD(viewangles[1]), 0 },
{ PS_FIELD(viewangles[2]), 0 },
{ PS_FIELD(viewoffset[0]), 0 },		// get rid of this
{ PS_FIELD(viewoffset[1]), 0 },
{ PS_FIELD(viewoffset[2]), 0 },
{ PS_FIELD(kick_angles[0]), 0 },
{ PS_FIELD(kick_angles[1]), 0 },
{ PS_FIELD(kick_angles[2]), 0 },
{ PS_FIELD(blend[0]), 0 },			// FIXME: calc on client, don't send over the net
{ PS_FIELD(blend[1]), 0 },
{ PS_FIELD(blend[2]), 0 },
{ PS_FIELD(blend[3]), 0 },
{ PS_FIELD(fov), 0 },			// FIXME: send with userinfo ?
{ PS_FIELD(vmodel.index), MAXEDICT_BITS },	// 4096 models 
{ PS_FIELD(vmodel.angles[0]), 0 },		// can be some different with viewangles
{ PS_FIELD(vmodel.angles[1]), 0 },
{ PS_FIELD(vmodel.angles[2]), 0 },
{ PS_FIELD(vmodel.offset[0]), 0 },		// center offset
{ PS_FIELD(vmodel.offset[1]), 0 },
{ PS_FIELD(vmodel.offset[2]), 0 },
{ PS_FIELD(vmodel.sequence), 10 },		// 1024 sequences
{ PS_FIELD(vmodel.frame), 0 },		// interpolate value
{ PS_FIELD(vmodel.body), 8 },			// 255 bodies
{ PS_FIELD(vmodel.skin), 8 },			// 255 skins
{ PS_FIELD(pmodel.index), MAXEDICT_BITS },	// 4096 models 
{ PS_FIELD(pmodel.sequence), 10 },		// 1024 sequences
{ PS_FIELD(vmodel.frame), 0 },		// interpolate value
};


#endif//PROTOCOL_H