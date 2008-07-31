//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         net_msg.h - message io functions
//=======================================================================
#ifndef NET_MSG_H
#define NET_MSG_H

enum net_types_e
{
	NET_BAD = 0,
	NET_CHAR,
	NET_BYTE,
	NET_SHORT,
	NET_WORD,
	NET_LONG,
	NET_FLOAT,
	NET_ANGLE,
	NET_SCALE,
	NET_COORD,
	NET_COLOR,
	NET_TYPES,
};

typedef struct net_desc_s
{
	int	type;	// pixelformat
	char	name[8];	// used for debug
	int	min_range;
	int	max_range;
} net_desc_t;

// communication state description
typedef struct net_field_s
{
	char	*name;
	int	offset;
	int	bits;
	bool	force;			// will be send for newentity
} net_field_t;

// server to client
enum svc_ops_e
{
	// user messages
	svc_bad = 0,
	svc_sound,		// <see code>
	svc_temp_entity,		// client effects
	svc_print,		// [byte] id [string] null terminated string
	svc_centerprint,		// [string] to put in center of the screen

	// engine messages
	svc_nop = 201,		// end of user messages
	svc_disconnect,		// kick client from server
	svc_reconnect,		// reconnecting server request
	svc_stufftext,		// [string] stuffed into client's console buffer, should be \n terminated
	svc_serverdata,		// [long] protocol ...
	svc_configstring,		// [short] [string]
	svc_spawnbaseline,		// valid only at spawn		
	svc_download,		// [short] size [size bytes]
	svc_playerinfo,		// variable
	svc_packetentities,		// [...]
	svc_deltapacketentities,	// [...]
	svc_frame,		// server frame
};

// client to server
enum clc_ops_e
{
	clc_bad = 0,

	// engine messages
	clc_nop = 201, 		
	clc_move,				// [[usercmd_t]
	clc_userinfo,			// [[userinfo string]
	clc_stringcmd,			// [string] message
};

typedef enum
{
	MSG_ONE,
	MSG_ALL,
	MSG_PHS,
	MSG_PVS,
	MSG_ONE_R,	// reliable messages
	MSG_ALL_R,
	MSG_PHS_R,
	MSG_PVS_R,
} msgtype_t;

static const net_desc_t NWDesc[] =
{
{ NET_BAD,	"none",	0,		0	}, // full range
{ NET_CHAR,	"Char",	-128,		127	},
{ NET_BYTE,	"Byte",	0,		255	},
{ NET_SHORT,	"Short",	-32767,		32767	},
{ NET_WORD,	"Word",	0,		65535	},
{ NET_LONG,	"Long",	0,		0	},
{ NET_FLOAT,	"Float",	0,		0	},
{ NET_ANGLE,	"Angle",	-360,		360	},
{ NET_SCALE,	"Scale",	0,		0	},
{ NET_COORD,	"Coord",	-262140,		262140	},
{ NET_COLOR,	"Color",	0,		255	},
};

/*
==========================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

==========================================================
*/

// per-level limits
#define MAX_DLIGHTS			128	// dynamic lights
#define MAX_CLIENTS			256	// absolute limit
#define MAX_LIGHTSTYLES		256	// compiler limit
#define MAX_SOUNDS			512	// so they cannot be blindly increased
#define MAX_IMAGES			256	// hud graphics
#define MAX_DECALS			256	// various decals
#define MAX_ITEMS			128	// player items
#define MAX_GENERAL			(MAX_CLIENTS * 2)	// general config strings

#define ES_FIELD(x)			#x,(int)&((entity_state_t*)0)->x
#define CM_FIELD(x)			#x,(int)&((usercmd_t*)0)->x

// config strings are a general means of communication from
// the server to all connected clients.
// each config string can be at most MAX_QPATH characters.
#define	CS_NAME			0
#define	CS_SKY			1
#define	CS_SKYAXIS		2	// %f %f %f format
#define	CS_SKYROTATE		3
#define	CS_STATUSBAR		4	// hud_program section name
#define	CS_AIRACCEL		5	// air acceleration control
#define	CS_MAXCLIENTS		6
#define	CS_MAPCHECKSUM		7	// for catching cheater maps

// reserved config strings

#define	CS_MODELS			16
#define	CS_SOUNDS			(CS_MODELS+MAX_MODELS)
#define	CS_IMAGES			(CS_SOUNDS+MAX_SOUNDS)
#define	CS_LIGHTS			(CS_IMAGES+MAX_IMAGES)
#define	CS_DECALS			(CS_LIGHTS+MAX_LIGHTSTYLES)
#define	CS_ITEMS			(CS_DECALS+MAX_DECALS)
#define	CS_PLAYERSKINS		(CS_ITEMS+MAX_ITEMS)
#define	CS_GENERAL		(CS_PLAYERSKINS+MAX_CLIENTS)
#define	MAX_CONFIGSTRINGS		(CS_GENERAL+MAX_GENERAL)

// sound flags
#define SND_VOL			(1<<0)	// a scaled byte
#define SND_ATTN			(1<<1)	// a byte
#define SND_POS			(1<<2)	// three coordinates
#define SND_ENT			(1<<3)	// a short 0 - 2: channel, 3 - 12: entity

static net_field_t ent_fields[] =
{
{ ES_FIELD(ed_type),		NET_BYTE,	 true	},
{ ES_FIELD(soundindex),		NET_WORD,	 false	},	// 512 sounds ( OpenAL software limit is 255 )
{ ES_FIELD(origin[0]),		NET_FLOAT, false	},
{ ES_FIELD(origin[1]),		NET_FLOAT, false	},
{ ES_FIELD(origin[2]),		NET_FLOAT, false	},
{ ES_FIELD(angles[0]),		NET_FLOAT, false	},
{ ES_FIELD(angles[1]),		NET_FLOAT, false	},
{ ES_FIELD(angles[2]),		NET_FLOAT, false	},
{ ES_FIELD(velocity[0]),		NET_FLOAT, false	},
{ ES_FIELD(velocity[1]),		NET_FLOAT, false	},
{ ES_FIELD(velocity[2]),		NET_FLOAT, false	},
{ ES_FIELD(old_origin[0]),		NET_FLOAT, true	},	// send always
{ ES_FIELD(old_origin[1]),		NET_FLOAT, true	},
{ ES_FIELD(old_origin[2]),		NET_FLOAT, true	},
{ ES_FIELD(old_velocity[0]),		NET_FLOAT, true	},	// client velocity
{ ES_FIELD(old_velocity[1]),		NET_FLOAT, true	},
{ ES_FIELD(old_velocity[2]),		NET_FLOAT, true	},
{ ES_FIELD(model.index),		NET_WORD,	 false	},	// 4096 models
{ ES_FIELD(model.colormap),		NET_WORD,	 false	},	// encoded as two shorts for top and bottom color
{ ES_FIELD(model.scale),		NET_COLOR, false	},	// 0-255 values
{ ES_FIELD(model.frame),		NET_FLOAT, false	},	// interpolate value
{ ES_FIELD(model.animtime),		NET_FLOAT, false	},	// auto-animating time
{ ES_FIELD(model.framerate),		NET_FLOAT, false	},	// custom framerate
{ ES_FIELD(model.sequence),		NET_WORD,	 false	},	// 1024 sequences
{ ES_FIELD(model.gaitsequence),	NET_WORD,	 false	},	// 1024 gaitsequences
{ ES_FIELD(model.skin),		NET_BYTE,	 false	},	// 255 skins
{ ES_FIELD(model.body),		NET_BYTE,	 false	},	// 255 bodies
{ ES_FIELD(model.blending[0]),	NET_COLOR, false	},	// animation blending
{ ES_FIELD(model.blending[1]),	NET_COLOR, false	},
{ ES_FIELD(model.blending[2]),	NET_COLOR, false	},
{ ES_FIELD(model.blending[3]),	NET_COLOR, false	},
{ ES_FIELD(model.blending[4]),	NET_COLOR, false	},	// send flags (first 4 bytes)
{ ES_FIELD(model.blending[5]),	NET_COLOR, false	},
{ ES_FIELD(model.blending[6]),	NET_COLOR, false	},
{ ES_FIELD(model.blending[7]),	NET_COLOR, false	},
{ ES_FIELD(model.blending[8]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[0]),	NET_COLOR, false	},	// bone controllers #
{ ES_FIELD(model.controller[1]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[2]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[3]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[4]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[5]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[6]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[7]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[8]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[9]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[10]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[11]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[12]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[13]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[14]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[15]),	NET_COLOR, false	},
{ ES_FIELD(model.controller[16]),	NET_COLOR, false	},	// FIXME: sending as array
{ ES_FIELD(solidtype),		NET_BYTE,	 false	},
{ ES_FIELD(movetype),		NET_BYTE,	 false	},        // send flags (second 4 bytes)
{ ES_FIELD(gravity),		NET_SHORT, false	},	// gravity multiplier
{ ES_FIELD(aiment),			NET_WORD,	 false	},	// entity index
{ ES_FIELD(solid),			NET_LONG,	 false	},	// encoded mins/maxs
{ ES_FIELD(mins[0]),		NET_FLOAT, false	},
{ ES_FIELD(mins[1]),		NET_FLOAT, false	},
{ ES_FIELD(mins[2]),		NET_FLOAT, false	},
{ ES_FIELD(maxs[0]),		NET_FLOAT, false	},
{ ES_FIELD(maxs[1]),		NET_FLOAT, false	},
{ ES_FIELD(maxs[2]),		NET_FLOAT, false	},	
{ ES_FIELD(effects),		NET_LONG,	 false	},	// effect flags
{ ES_FIELD(renderfx),		NET_LONG,	 false	},	// renderfx flags
{ ES_FIELD(renderamt),		NET_COLOR, false	},	// alpha amount
{ ES_FIELD(rendercolor[0]),		NET_COLOR, false	},	// animation blending
{ ES_FIELD(rendercolor[1]),		NET_COLOR, false	},
{ ES_FIELD(rendercolor[2]),		NET_COLOR, false	},
{ ES_FIELD(rendermode),		NET_BYTE,  false	},	// render mode (legacy stuff)
{ ES_FIELD(pm_type),		NET_BYTE,  false	},	// 16 player movetypes allowed
{ ES_FIELD(pm_flags),		NET_WORD,  true	},	// 16 movetype flags allowed
{ ES_FIELD(pm_time),		NET_BYTE,  true	},	// each unit 8 msec
{ ES_FIELD(delta_angles[0]),		NET_FLOAT, false	},
{ ES_FIELD(delta_angles[1]),		NET_FLOAT, false	},
{ ES_FIELD(delta_angles[2]),		NET_FLOAT, false	},
{ ES_FIELD(punch_angles[0]),		NET_SCALE, false	},
{ ES_FIELD(punch_angles[1]),		NET_SCALE, false	},
{ ES_FIELD(punch_angles[2]),		NET_SCALE, false	},
{ ES_FIELD(viewangles[0]),		NET_FLOAT, false	},	// for fixed views
{ ES_FIELD(viewangles[1]),		NET_FLOAT, false	},
{ ES_FIELD(viewangles[2]),		NET_FLOAT, false	},
	// FIXME: replace with viewoffset 
	//{ ES_FIELD(viewheight),		NET_SHORT, false	},	// client viewheight
{ ES_FIELD(viewoffset[0]),		NET_SCALE, false	},	// get rid of this
{ ES_FIELD(viewoffset[1]),		NET_SCALE, false	},
{ ES_FIELD(viewoffset[2]),		NET_SCALE, false	},
{ ES_FIELD(maxspeed),		NET_WORD,  false	},	// send flags (third 4 bytes )
{ ES_FIELD(fov),			NET_FLOAT, false	},	// client horizontal field of view
{ ES_FIELD(vmodel.index),		NET_WORD,  false	},	// 4096 models 
{ ES_FIELD(vmodel.colormap),		NET_LONG,  false	},	// 4096 models 
{ ES_FIELD(vmodel.sequence),		NET_WORD,  false	},	// 1024 sequences
{ ES_FIELD(vmodel.frame),		NET_FLOAT, false	},	// interpolate value
{ ES_FIELD(vmodel.body),		NET_BYTE,  false	},	// 255 bodies
{ ES_FIELD(vmodel.skin),		NET_BYTE,  false	},	// 255 skins
{ ES_FIELD(pmodel.index),		NET_WORD,  false	},	// 4096 models 
{ ES_FIELD(pmodel.colormap),		NET_LONG,  false	},	// 4096 models 
{ ES_FIELD(pmodel.sequence),		NET_WORD,  false	},	// 1024 sequences
{ ES_FIELD(vmodel.frame),		NET_FLOAT, false	},	// interpolate value
{ NULL },							// terminator
};

// probably usercmd_t never reached 32 field integer limit (in theory of course)
static net_field_t cmd_fields[] =
{
{ CM_FIELD(msec),		NET_BYTE,  true	},
{ CM_FIELD(angles[0]),	NET_WORD,  false	},
{ CM_FIELD(angles[1]),	NET_WORD,  false	},
{ CM_FIELD(angles[2]),	NET_WORD,  false	},
{ CM_FIELD(forwardmove),	NET_SHORT, false	},
{ CM_FIELD(sidemove),	NET_SHORT, false	},
{ CM_FIELD(upmove),		NET_SHORT, false	},
{ CM_FIELD(buttons),	NET_BYTE,  false	},
{ CM_FIELD(impulse),	NET_BYTE,  false	},
{ CM_FIELD(lightlevel),	NET_BYTE,  false	},
{ NULL },
};

/*
==============================================================================

			MESSAGE IO FUNCTIONS
	       Handles byte ordering and avoids alignment errors
==============================================================================
*/
void MSG_Init( sizebuf_t *buf, byte *data, size_t length );
void MSG_Clear( sizebuf_t *buf );
void MSG_Print( sizebuf_t *msg, const char *data );
void MSG_Bitstream( sizebuf_t *buf, bool state );
void _MSG_WriteBits( sizebuf_t *msg, int value, int bits, const char *filename, const int fileline );
long _MSG_ReadBits( sizebuf_t *msg, int bits, const char *filename, const int fileline );
void _MSG_Begin( int dest, const char *filename, int fileline );
void _MSG_WriteString( sizebuf_t *sb, const char *s, const char *filename, int fileline );
void _MSG_WriteFloat( sizebuf_t *sb, float f, const char *filename, int fileline );
void _MSG_WritePos( sizebuf_t *sb, vec3_t pos, const char *filename, int fileline );
void _MSG_WriteData( sizebuf_t *sb, const void *data, size_t length, const char *filename, int fileline );
void _MSG_WriteDeltaUsercmd( sizebuf_t *sb, struct usercmd_s *from, struct usercmd_s *cmd, const char *filename, const int fileline );
void _MSG_WriteDeltaEntity( struct entity_state_s *from, struct entity_state_s *to, sizebuf_t *msg, bool force, bool newentity, const char *filename, int fileline );
void _MSG_Send( msgtype_t to, vec3_t origin, edict_t *ent, const char *filename, int fileline );

#define MSG_Begin( x ) _MSG_Begin( x, __FILE__, __LINE__)
#define MSG_WriteChar(x,y) _MSG_WriteBits (x, y, NET_CHAR, __FILE__, __LINE__)
#define MSG_WriteByte(x,y) _MSG_WriteBits (x, y, NET_BYTE, __FILE__, __LINE__)
#define MSG_WriteShort(x,y) _MSG_WriteBits(x, y, NET_SHORT,__FILE__, __LINE__)
#define MSG_WriteWord(x,y) _MSG_WriteBits (x, y, NET_WORD, __FILE__, __LINE__)
#define MSG_WriteLong(x,y) _MSG_WriteBits (x, y, NET_LONG, __FILE__, __LINE__)
#define MSG_WriteFloat(x,y) _MSG_WriteFloat(x, y, __FILE__, __LINE__)
#define MSG_WriteString(x,y) _MSG_WriteString (x, y, __FILE__, __LINE__)
#define MSG_WriteCoord16(x, y) _MSG_WriteBits(x, y, NET_COORD, __FILE__, __LINE__)
#define MSG_WriteCoord32(x, y) _MSG_WriteBits(x, y, NET_FLOAT, __FILE__, __LINE__)
#define MSG_WriteAngle16(x, y) _MSG_WriteBits(x, y, NET_ANGLE, __FILE__, __LINE__)
#define MSG_WriteAngle32(x, y) _MSG_WriteBits(x, y, NET_FLOAT, __FILE__, __LINE__)
#define MSG_WritePos(x, y) _MSG_WritePos( x, y, __FILE__, __LINE__ )
#define MSG_WriteData(x,y,z) _MSG_WriteData (x, y, z, __FILE__, __LINE__)
#define MSG_WriteDeltaUsercmd(x, y, z) _MSG_WriteDeltaUsercmd (x, y, z, __FILE__, __LINE__)
#define MSG_WriteDeltaEntity(from, to, msg, force, new ) _MSG_WriteDeltaEntity (from, to, msg, force, new, __FILE__, __LINE__)
#define MSG_WriteBits( buf, value, bits ) _MSG_WriteBits( buf, value, bits, __FILE__, __LINE__ )
#define MSG_ReadBits( buf, bits ) _MSG_ReadBits( buf, bits, __FILE__, __LINE__ )
#define MSG_Send(x, y, z) _MSG_Send(x, y, z, __FILE__, __LINE__)

void MSG_BeginReading (sizebuf_t *sb);
#define MSG_ReadChar( x ) _MSG_ReadBits( x, NET_CHAR, __FILE__, __LINE__ )
#define MSG_ReadByte( x ) _MSG_ReadBits( x, NET_BYTE, __FILE__, __LINE__ )
#define MSG_ReadShort( x) _MSG_ReadBits( x, NET_SHORT, __FILE__, __LINE__ )
#define MSG_ReadWord( x ) _MSG_ReadBits( x, NET_WORD, __FILE__, __LINE__ )
#define MSG_ReadLong( x ) _MSG_ReadBits( x, NET_LONG, __FILE__, __LINE__ )
float MSG_ReadFloat( sizebuf_t *msg );
char *MSG_ReadString( sizebuf_t *sb );
char *MSG_ReadStringLine( sizebuf_t *sb );
void MSG_ReadPos( sizebuf_t *sb, vec3_t pos );
void MSG_ReadData( sizebuf_t *sb, void *buffer, size_t size );
void MSG_ReadDeltaUsercmd( sizebuf_t *sb, usercmd_t *from, usercmd_t *cmd );
void MSG_ReadDeltaEntity( sizebuf_t *sb, entity_state_t *from, entity_state_t *to, int number );
void MSG_WriteDeltaPlayerstate( entity_state_t *from, entity_state_t *to, sizebuf_t *msg );
void MSG_ReadDeltaPlayerstate( sizebuf_t *msg, entity_state_t *from, entity_state_t *to );

// huffman compression
void Huff_Init( void );
void Huff_CompressPacket( sizebuf_t *msg, int offset );
void Huff_DecompressPacket( sizebuf_t *msg, int offset );

/*
==============================================================

NET

==============================================================
*/
bool NET_GetLoopPacket( netsrc_t sock, netadr_t *from, sizebuf_t *msg );
void NET_SendPacket( netsrc_t sock, int length, void *data, netadr_t to );
bool NET_StringToAdr( const char *s, netadr_t *a );
bool NET_CompareBaseAdr( netadr_t a, netadr_t b );
bool NET_CompareAdr( netadr_t a, netadr_t b );
bool NET_IsLocalAddress( netadr_t adr );
void NET_Sleep( int msec );

typedef struct netchan_s
{
	bool			fatal_error;
	netsrc_t			sock;

	int			dropped;			// between last packet and previous
	bool			compress;			// enable huffman compression

	int			last_received;		// for timeouts
	int			last_sent;		// for retransmits

	netadr_t			remote_address;
	int			qport;			// qport value to write when transmitting

	// sequencing variables
	int			incoming_sequence;
	int			incoming_acknowledged;
	int			incoming_reliable_acknowledged;	// single bit

	int			incoming_reliable_sequence;		// single bit, maintained local

	int			outgoing_sequence;
	int			reliable_sequence;			// single bit
	int			last_reliable_sequence;		// sequence number of last send

	// reliable staging and holding areas
	sizebuf_t			message;				// writing buffer to send to server
	byte			message_buf[MAX_MSGLEN-16];		// leave space for header

	// message is copied to this buffer when it is first transfered
	int			reliable_length;
	byte			reliable_buf[MAX_MSGLEN-16];		// unacked reliable message

} netchan_t;

#define PROTOCOL_VERSION	36
#define PORT_MASTER		27900
#define PORT_CLIENT		27901
#define PORT_SERVER		27910
#define UPDATE_BACKUP	64	// copies of entity_state_t to keep buffered, must be power of two
#define UPDATE_MASK		(UPDATE_BACKUP - 1)

void Netchan_Init( void );
void Netchan_Setup( netsrc_t sock, netchan_t *chan, netadr_t adr, int qport );
bool Netchan_NeedReliable( netchan_t *chan );
void Netchan_Transmit( netchan_t *chan, int length, byte *data );
void Netchan_OutOfBand( int net_socket, netadr_t adr, int length, byte *data );
void Netchan_OutOfBandPrint( int net_socket, netadr_t adr, char *format, ... );
bool Netchan_Process( netchan_t *chan, sizebuf_t *msg );
bool Netchan_CanReliable( netchan_t *chan );

#endif//NET_MSG_H