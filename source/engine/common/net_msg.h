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
	svc_clientindex,		// [byte] edictnumber
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
{ NET_COLOR,	"Color",	0,		0	},
};

/*
==========================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

==========================================================
*/

// per-level limits


#define ES_FIELD(x)			#x,(int)&((entity_state_t*)0)->x
#define CM_FIELD(x)			#x,(int)&((usercmd_t*)0)->x

// config strings are a general means of communication from
// the server to all connected clients.
// each config string can be at most MAX_QPATH characters.
#define CS_NAME			0	// map name
#define CS_MAPCHECKSUM		1	// level checksum (for catching cheater maps)
#define CS_SKYNAME			2	// skybox name
#define CS_SKYANGLES		3	// <angles> %f %f %f
#define CS_SKYSPEED			4	// <speed> %f
#define CS_MAXCLIENTS		5	// server maxclients value (0-255)

// reserved config strings

#define CS_MODELS			16				// configstrings starts here
#define CS_SOUNDS			(CS_MODELS+MAX_MODELS)		// sound names
#define CS_CLASSNAMES		(CS_SOUNDS+MAX_SOUNDS)		// edicts classnames
#define CS_LIGHTSTYLES		(CS_CLASSNAMES+MAX_CLASSNAMES)	// lightstyle patterns
#define MAX_CONFIGSTRINGS		(CS_LIGHTSTYLES+MAX_LIGHTSTYLES)	// total count

// sound flags (get rid of this)
#define SND_VOL			(1<<0)	// a scaled byte
#define SND_ATTN			(1<<1)	// a byte
#define SND_POS			(1<<2)	// three coordinates
#define SND_ENT			(1<<3)	// a short 0 - 2: channel, 3 - 12: entity

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
entity_state_t MSG_ParseDeltaPlayer( entity_state_t *from, entity_state_t *to );

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