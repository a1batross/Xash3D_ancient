//=======================================================================
//			Copyright XashXT Group 2007 ©
//		         net_msg.h - message io functions
//=======================================================================
#ifndef NET_MSG_H
#define NET_MSG_H

// server to client
enum svc_ops_e
{
	// user messages
	svc_bad = 0,
	svc_sound,		// <see code>
	svc_ambientsound,		// ambient looping sound
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
	clc_bad,
	clc_nop, 		
	clc_move,				// [[usercmd_t]
	clc_userinfo,			// [[userinfo string]
	clc_stringcmd			// [string] message
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

#define PS_M_TYPE			(1<<0)
#define PS_M_ORIGIN			(1<<1)
#define PS_M_VELOCITY		(1<<2)
#define PS_M_TIME			(1<<3)
#define PS_M_COMMANDMSEC		(1<<4)
#define PS_M_FLAGS			(1<<5)
#define PS_M_GRAVITY		(1<<6)
#define PS_M_DELTA_ANGLES		(1<<7)
#define PS_VIEWOFFSET		(1<<8)
#define PS_VIEWANGLES		(1<<9)
#define PS_KICKANGLES		(1<<10)
#define PS_BLEND			(1<<11)
#define PS_FOV			(1<<12)
#define PS_WEAPONINDEX		(1<<13)
#define PS_WEAPONOFFSET		(1<<14)
#define PS_WEAPONANGLES		(1<<15)
#define PS_WEAPONFRAME		(1<<16)
#define PS_WEAPONSEQUENCE		(1<<17)
#define PS_WEAPONBODY		(1<<18)
#define PS_WEAPONSKIN		(1<<19)
#define PS_RDFLAGS			(1<<20)

// ms and light always sent, the others are optional
#define	CM_ANGLE1 	(1<<0)
#define	CM_ANGLE2 	(1<<1)
#define	CM_ANGLE3 	(1<<2)
#define	CM_FORWARD	(1<<3)
#define	CM_SIDE		(1<<4)
#define	CM_UP		(1<<5)
#define	CM_BUTTONS	(1<<6)
#define	CM_IMPULSE	(1<<7)

// dmflags->value flags
#define	DF_NO_HEALTH		0x00000001	// 1
#define	DF_NO_ITEMS		0x00000002	// 2
#define	DF_WEAPONS_STAY		0x00000004	// 4
#define	DF_NO_FALLING		0x00000008	// 8
#define	DF_INSTANT_ITEMS		0x00000010	// 16
#define	DF_SAME_LEVEL		0x00000020	// 32
#define	DF_SKINTEAMS		0x00000040	// 64
#define	DF_MODELTEAMS		0x00000080	// 128
#define	DF_NO_FRIENDLY_FIRE		0x00000100	// 256
#define	DF_SPAWN_FARTHEST		0x00000200	// 512
#define	DF_FORCE_RESPAWN		0x00000400	// 1024
#define	DF_NO_ARMOR		0x00000800	// 2048
#define	DF_ALLOW_EXIT		0x00001000	// 4096
#define	DF_INFINITE_AMMO		0x00002000	// 8192
#define	DF_QUAD_DROP		0x00004000	// 16384
#define	DF_FIXED_FOV		0x00008000	// 32768
#define	DF_QUADFIRE_DROP		0x00010000	// 65536

// try to pack the common update flags into the first byte
#define	U_ORIGIN1		(1<<0)
#define	U_ORIGIN2		(1<<1)
#define	U_ANGLE2		(1<<2)
#define	U_ANGLE3		(1<<3)
#define	U_FRAME		(1<<4)		// sv. interpolated value
#define	U_SKIN8		(1<<5)
#define	U_REMOVE		(1<<6)		// REMOVE this entity, don't add it
#define	U_MOREBITS1	(1<<7)		// read one additional byte

// second byte
#define	U_NUMBER16	(1<<8)		// NUMBER8 is implicit if not set
#define	U_ORIGIN3		(1<<9)
#define	U_ANGLE1		(1<<10)
#define	U_MODEL		(1<<11)
#define	U_RENDERFX8	(1<<12)		// fullbright, etc
#define	U_EFFECTS8	(1<<14)		// autorotate, trails, etc
#define	U_MOREBITS2	(1<<15)		// read one additional byte

// third byte
#define	U_SKIN16		(1<<16)
#define	U_ANIMTIME	(1<<17)		// auto-animating time
#define	U_RENDERFX16	(1<<18)		// 8 + 16 = 32
#define	U_EFFECTS16	(1<<19)		// 8 + 16 = 32
#define	U_WEAPONMODEL	(1<<20)		// weapons, flags, etc
#define	U_SOUNDIDX	(1<<21)
#define	U_SEQUENCE	(1<<22)		// animation sequence
#define	U_MOREBITS3	(1<<23)		// read one additional byte

// fourth byte
#define	U_OLDORIGIN	(1<<24)		// FIXME: get rid of this
#define	U_BODY		(1<<25)
#define	U_SOLID		(1<<26)
#define	U_ALPHA		(1<<27)		// alpha value

#define	U_MOREBITS4	(1<<31)		// read one additional byte

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

	short		skin;		// skin for studiomodels
	float		frame;		// % playback position in animation sequences (0..512)
	byte		body;		// sub-model selection for studiomodels
	byte		sequence;		// animation sequence (0 - 255)
	uint		effects;		// PGM - we're filling it, so it needs to be unsigned
	int		renderfx;
	int		solid;		// for client side prediction, 8*(bits 0-4) is x/y radius
					// 8*(bits 5-9) is z down distance, 8(bits10-15) is z up
					// gi.linkentity sets this properly
	float		alpha;		// alpha value
	float		animtime;		// auto-animating time

} entity_state_t;

/*
==========================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

==========================================================
*/

// per-level limits
#define MAX_ENTITIES		512	// refdef ents
#define MAX_DLIGHTS			64
#define MAX_CLIENTS			256	// absolute limit
#define MAX_LIGHTSTYLES		256
#define MAX_SOUNDS			512	// so they cannot be blindly increased
#define MAX_IMAGES			256	// hud graphics
#define MAX_DECALS			256	// various decals
#define MAX_ITEMS			128	// player items
#define MAX_GENERAL			(MAX_CLIENTS * 2)	// general config strings

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
#define SND_VOLUME			(1<<0)		// a byte
#define SND_ATTENUATION		(1<<1)		// a byte
#define SND_POS			(1<<2)		// three coordinates
#define SND_ENT			(1<<3)		// a short 0-2: channel, 3-12: entity
#define SND_OFFSET			(1<<4)		// a byte, msec offset from frame start

// 0-compressed fields
#define DEFAULT_SOUND_PACKET_VOL	1.0
#define DEFAULT_SOUND_PACKET_ATTN	1.0

/*
==============================================================================

			MESSAGE IO FUNCTIONS
	       Handles byte ordering and avoids alignment errors
==============================================================================
*/
#define SZ_GetSpace(buf, len) _SZ_GetSpace(buf, len, __FILE__, __LINE__ )
#define SZ_Write(buf, data, len) _SZ_Write(buf, data, len, __FILE__, __LINE__ )
void SZ_Init (sizebuf_t *buf, byte *data, int length);
void SZ_Clear (sizebuf_t *buf);
void *_SZ_GetSpace (sizebuf_t *buf, int length, const char *filename, int fileline);
void _SZ_Write (sizebuf_t *buf, const void *data, int length, const char *filename, int fileline);
void SZ_Print (sizebuf_t *buf, char *data);	// strcats onto the sizebuf

void _MSG_Begin ( int dest, const char *filename, int fileline );
void _MSG_WriteChar (sizebuf_t *sb, int c, const char *filename, int fileline);
void _MSG_WriteByte (sizebuf_t *sb, int c, const char *filename, int fileline);
void _MSG_WriteShort (sizebuf_t *sb, int c, const char *filename, int fileline);
void _MSG_WriteWord (sizebuf_t *sb, int c, const char *filename, int fileline);
void _MSG_WriteLong (sizebuf_t *sb, int c, const char *filename, int fileline);
void _MSG_WriteFloat (sizebuf_t *sb, float f, const char *filename, int fileline);
void _MSG_WriteString (sizebuf_t *sb, const char *s, const char *filename, int fileline);
void _MSG_WriteCoord16(sizebuf_t *sb, int f, const char *filename, int fileline);
void _MSG_WriteCoord32(sizebuf_t *sb, float f, const char *filename, int fileline);
void _MSG_WriteAngle16(sizebuf_t *sb, float f, const char *filename, int fileline);
void _MSG_WriteAngle32(sizebuf_t *sb, float f, const char *filename, int fileline);
void _MSG_WritePos16(sizebuf_t *sb, int pos[3], const char *filename, int fileline);
void _MSG_WritePos32(sizebuf_t *sb, vec3_t pos, const char *filename, int fileline);
void _MSG_WriteUnterminatedString (sizebuf_t *sb, const char *s, const char *filename, int fileline);
void _MSG_WriteDeltaUsercmd (sizebuf_t *sb, struct usercmd_s *from, struct usercmd_s *cmd, const char *filename, int fileline);
void _MSG_WriteDeltaEntity (struct entity_state_s *from, struct entity_state_s *to, sizebuf_t *msg, bool force, bool newentity, const char *filename, int fileline);
void _MSG_Send (msgtype_t to, vec3_t origin, edict_t *ent, const char *filename, int fileline);

#define MSG_Begin( x ) _MSG_Begin( x, __FILE__, __LINE__);
#define MSG_WriteChar(x,y) _MSG_WriteChar (x, y, __FILE__, __LINE__);
#define MSG_WriteByte(x,y) _MSG_WriteByte (x, y, __FILE__, __LINE__);
#define MSG_WriteShort(x,y) _MSG_WriteShort (x, y, __FILE__, __LINE__);
#define MSG_WriteWord(x,y) _MSG_WriteWord (x, y, __FILE__, __LINE__);
#define MSG_WriteLong(x,y) _MSG_WriteLong (x, y, __FILE__, __LINE__);
#define MSG_WriteFloat(x, y) _MSG_WriteFloat (x, y, __FILE__, __LINE__);
#define MSG_WriteString(x,y) _MSG_WriteString (x, y, __FILE__, __LINE__);
#define MSG_WriteCoord16(x, y) _MSG_WriteCoord16(x, y, __FILE__, __LINE__);
#define MSG_WriteCoord32(x, y) _MSG_WriteCoord32(x, y, __FILE__, __LINE__);
#define MSG_WriteAngle16(x, y) _MSG_WriteAngle16(x, y, __FILE__, __LINE__);
#define MSG_WriteAngle32(x, y) _MSG_WriteAngle32(x, y, __FILE__, __LINE__);
#define MSG_WritePos16(x, y) _MSG_WritePos16(x, y, __FILE__, __LINE__);
#define MSG_WritePos32(x, y) _MSG_WritePos32(x, y, __FILE__, __LINE__);
#define MSG_WriteUnterminatedString(x, y) _MSG_WriteUnterminatedString (x, y, __FILE__, __LINE__);
#define MSG_WriteDeltaUsercmd(x, y, z) _MSG_WriteDeltaUsercmd (x, y, z, __FILE__, __LINE__);
#define MSG_WriteDeltaEntity(x, y, z, t, m) _MSG_WriteDeltaEntity (x, y, z, t, m, __FILE__, __LINE__);
#define MSG_Send(x, y, z) _MSG_Send(x, y, z, __FILE__, __LINE__);

void MSG_BeginReading (sizebuf_t *sb);
int MSG_ReadChar (sizebuf_t *sb);
int MSG_ReadByte (sizebuf_t *sb);
int MSG_ReadShort (sizebuf_t *sb);
int MSG_ReadLong (sizebuf_t *sb);
float MSG_ReadFloat (sizebuf_t *sb);
char *MSG_ReadString (sizebuf_t *sb);
char *MSG_ReadStringLine (sizebuf_t *sb);
float MSG_ReadCoord16(sizebuf_t *sb);
float MSG_ReadCoord32(sizebuf_t *sb);
float MSG_ReadAngle16(sizebuf_t *sb);
float MSG_ReadAngle32(sizebuf_t *sb);
void MSG_ReadPos16(sizebuf_t *sb, int pos[3]);
void MSG_ReadPos32(sizebuf_t *sb, vec3_t pos);
void MSG_ReadData (sizebuf_t *sb, void *buffer, int size);
void MSG_ReadDeltaUsercmd (sizebuf_t *sb, struct usercmd_s *from, struct usercmd_s *cmd);
void MSG_ReadDeltaEntity(sizebuf_t *sb, entity_state_t *from, entity_state_t *to, int number, int bits);

/*
==============================================================

NET

==============================================================
*/
bool NET_GetLoopPacket( netsrc_t sock, netadr_t *net_from, sizebuf_t *net_message );
void NET_SendPacket( netsrc_t sock, int length, void *data, netadr_t to );
bool NET_GetPacket( netsrc_t sock, netadr_t *from, sizebuf_t *msg );
bool NET_StringToAdr( const char *s, netadr_t *a );
bool NET_CompareBaseAdr( netadr_t a, netadr_t b );
bool NET_CompareAdr( netadr_t a, netadr_t b );
bool NET_IsLocalAddress( netadr_t adr );

//============================================================================

#define OLD_AVG		0.99		// total = oldtotal*OLD_AVG + new*(1-OLD_AVG)
#define MAX_LATENT		32

typedef struct netchan_s
{
	bool			fatal_error;
	netsrc_t			sock;

	int			dropped;			// between last packet and previous

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

extern netadr_t	net_from;
extern sizebuf_t	net_message;
extern byte	net_message_buffer[MAX_MSGLEN];

#define PROTOCOL_VERSION	35
#define PORT_MASTER		27900
#define PORT_CLIENT		27901
#define PORT_SERVER		27910
#define UPDATE_BACKUP	64	// copies of entity_state_t to keep buffered, must be power of two
#define UPDATE_MASK		(UPDATE_BACKUP - 1)

void Netchan_Init (void);
void Netchan_Setup (netsrc_t sock, netchan_t *chan, netadr_t adr, int qport);
bool Netchan_NeedReliable (netchan_t *chan);
void Netchan_Transmit (netchan_t *chan, int length, byte *data);
void Netchan_OutOfBand (int net_socket, netadr_t adr, int length, byte *data);
void Netchan_OutOfBandPrint (int net_socket, netadr_t adr, char *format, ...);
bool Netchan_Process (netchan_t *chan, sizebuf_t *msg);
bool Netchan_CanReliable (netchan_t *chan);

#endif//NET_MSG_H