//=======================================================================
//			Copyright XashXT Group 2007 ©
//			network.c - network interface
//=======================================================================

#include <winsock.h>
#include <wsipx.h>
#include "launch.h"
#include "byteorder.h"

#define MAX_IPS		16
#define PORT_ANY		-1
#define PORT_SERVER		27960
#define DO( src, dest )	\
	copy[0] = s[src];	\
	copy[1] = s[src + 1];	\
	sscanf( copy, "%x", &val );	\
	((struct sockaddr_ipx *)sadr)->dest = val
	
// wsock32.dll exports
static int (_stdcall *pWSACleanup)( void );
static word (_stdcall *pNtohs)( word netshort );
static int (_stdcall *pWSAGetLastError)( void );
static int (_stdcall *pCloseSocket)( SOCKET s );
static word (_stdcall *pHtons)( word hostshort );
static dword (_stdcall *pInet_Addr)( const char* cp );
static SOCKET (_stdcall *pSocket)( int af, int type, int protocol );
static struct hostent *(_stdcall *pGetHostByName)( const char* name );
static int (_stdcall *pIoctlSocket)( SOCKET s, long cmd, dword* argp );
static int (_stdcall *pWSAStartup)( word wVersionRequired, LPWSADATA lpWSAData );
static int (_stdcall *pBind)( SOCKET s, const struct sockaddr* addr, int namelen );
static int (_stdcall *pSetSockopt)( SOCKET s, int level, int optname, const char* optval, int optlen );
static int (_stdcall *pRecvFrom)( SOCKET s, char* buf, int len, int flags, struct sockaddr* from, int* fromlen );
static int (_stdcall *pSendTo)( SOCKET s, const char* buf, int len, int flags, const struct sockaddr* to, int tolen );
static int (_stdcall *pSelect)( int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, const struct timeval* timeout );
static int (_stdcall *pConnect)( SOCKET s, const struct sockaddr *name, int namelen );
static int (_stdcall *pSend)( SOCKET s, const char *buf, int len, int flags );
static int (_stdcall *pRecv)( SOCKET s, char *buf, int len, int flags );
static int (_stdcall *pGetHostName)( char *name, int namelen );
static dword (_stdcall *pNtohl)( dword netlong );

static dllfunc_t winsock_funcs[] =
{
	{"bind", (void **) &pBind },
	{"send", (void **) &pSend },
	{"recv", (void **) &pRecv },
	{"ntohs",	(void **) &pNtohs },
	{"htons", (void **) &pHtons },
	{"ntohl", (void **) &pNtohl },
	{"socket", (void **) &pSocket },
	{"select", (void **) &pSelect },
	{"sendto", (void **) &pSendTo },
	{"connect", (void **) &pConnect },
	{"recvfrom", (void **) &pRecvFrom },
	{"inet_addr", (void **) &pInet_Addr },
	{"WSAStartup", (void **) &pWSAStartup },
	{"WSACleanup", (void **) &pWSACleanup },
	{"setsockopt", (void **) &pSetSockopt },
	{"ioctlsocket", (void **) &pIoctlSocket },
	{"closesocket", (void **) &pCloseSocket },
	{"gethostname", (void **) &pGetHostName },
	{"gethostbyname", (void **) &pGetHostByName },
	{"WSAGetLastError", (void **) &pWSAGetLastError },
	{ NULL, NULL }
};

dll_info_t winsock_dll = { "wsock32.dll", winsock_funcs, NULL, NULL, NULL, true, 0 };

static WSADATA winsockdata;
static bool winsockInitialized = false;
static bool usingSocks = false;
static bool net_active = false;

static cvar_t *net_socks_enabled;
static cvar_t *net_socks_server;
static cvar_t *net_socks_port;
static cvar_t *net_socks_username;
static cvar_t *net_socks_password;
static struct sockaddr socksRelayAddr;
static SOCKET ip_socket;
static SOCKET socks_socket;
static SOCKET ipx_socket;
static char socksBuf[4096];
static byte localIP[MAX_IPS][4];
static int numIP;

void NET_OpenWinSock( void )
{
	Sys_LoadLibrary( &winsock_dll ); 	
}

void NET_FreeWinSock( void )
{
	Sys_FreeLibrary( &winsock_dll );
}

/*
====================
NET_ErrorString
====================
*/
char *NET_ErrorString( void )
{
	switch(pWSAGetLastError())
	{
	case WSAEINTR: return "WSAEINTR";
	case WSAEBADF: return "WSAEBADF";
	case WSAEACCES: return "WSAEACCES";
	case WSAEDISCON: return "WSAEDISCON";
	case WSAEFAULT: return "WSAEFAULT";
	case WSAEINVAL: return "WSAEINVAL";
	case WSAEMFILE: return "WSAEMFILE";
	case WSAEWOULDBLOCK: return "WSAEWOULDBLOCK";
	case WSAEINPROGRESS: return "WSAEINPROGRESS";
	case WSAEALREADY: return "WSAEALREADY";
	case WSAENOTSOCK: return "WSAENOTSOCK";
	case WSAEDESTADDRREQ: return "WSAEDESTADDRREQ";
	case WSAEMSGSIZE: return "WSAEMSGSIZE";
	case WSAEPROTOTYPE: return "WSAEPROTOTYPE";
	case WSAENOPROTOOPT: return "WSAENOPROTOOPT";
	case WSAEPROTONOSUPPORT: return "WSAEPROTONOSUPPORT";
	case WSAESOCKTNOSUPPORT: return "WSAESOCKTNOSUPPORT";
	case WSAEOPNOTSUPP: return "WSAEOPNOTSUPP";
	case WSAEPFNOSUPPORT: return "WSAEPFNOSUPPORT";
	case WSAEAFNOSUPPORT: return "WSAEAFNOSUPPORT";
	case WSAEADDRINUSE: return "WSAEADDRINUSE";
	case WSAEADDRNOTAVAIL: return "WSAEADDRNOTAVAIL";
	case WSAENETDOWN: return "WSAENETDOWN";
	case WSAENETUNREACH: return "WSAENETUNREACH";
	case WSAENETRESET: return "WSAENETRESET";
	case WSAECONNABORTED: return "WSWSAECONNABORTEDAEINTR";
	case WSAECONNRESET: return "WSAECONNRESET";
	case WSAENOBUFS: return "WSAENOBUFS";
	case WSAEISCONN: return "WSAEISCONN";
	case WSAENOTCONN: return "WSAENOTCONN";
	case WSAESHUTDOWN: return "WSAESHUTDOWN";
	case WSAETOOMANYREFS: return "WSAETOOMANYREFS";
	case WSAETIMEDOUT: return "WSAETIMEDOUT";
	case WSAECONNREFUSED: return "WSAECONNREFUSED";
	case WSAELOOP: return "WSAELOOP";
	case WSAENAMETOOLONG: return "WSAENAMETOOLONG";
	case WSAEHOSTDOWN: return "WSAEHOSTDOWN";
	case WSASYSNOTREADY: return "WSASYSNOTREADY";
	case WSAVERNOTSUPPORTED: return "WSAVERNOTSUPPORTED";
	case WSANOTINITIALISED: return "WSANOTINITIALISED";
	case WSAHOST_NOT_FOUND: return "WSAHOST_NOT_FOUND";
	case WSATRY_AGAIN: return "WSATRY_AGAIN";
	case WSANO_RECOVERY: return "WSANO_RECOVERY";
	case WSANO_DATA: return "WSANO_DATA";
	default: return "NO ERROR";
	}
}

void NetadrToSockadr( netadr_t *a, struct sockaddr *s )
{
	memset( s, 0, sizeof(*s));

	if( a->type == NA_BROADCAST )
	{
		((struct sockaddr_in *)s)->sin_family = AF_INET;
		((struct sockaddr_in *)s)->sin_port = a->port;
		((struct sockaddr_in *)s)->sin_addr.s_addr = INADDR_BROADCAST;
	}
	else if( a->type == NA_IP )
	{
		((struct sockaddr_in *)s)->sin_family = AF_INET;
		((struct sockaddr_in *)s)->sin_addr.s_addr = *(int *)&a->ip;
		((struct sockaddr_in *)s)->sin_port = a->port;
	}
	else if (a->type == NA_IPX)
	{
		((struct sockaddr_ipx *)s)->sa_family = AF_IPX;
		memcpy(((struct sockaddr_ipx *)s)->sa_netnum, &a->ipx[0], 4);
		memcpy(((struct sockaddr_ipx *)s)->sa_nodenum, &a->ipx[4], 6);
		((struct sockaddr_ipx *)s)->sa_socket = a->port;
	}
	else if (a->type == NA_BROADCAST_IPX)
	{
		((struct sockaddr_ipx *)s)->sa_family = AF_IPX;
		memset(((struct sockaddr_ipx *)s)->sa_netnum, 0, 4);
		memset(((struct sockaddr_ipx *)s)->sa_nodenum, 0xff, 6);
		((struct sockaddr_ipx *)s)->sa_socket = a->port;
	}
}


void SockadrToNetadr( struct sockaddr *s, netadr_t *a )
{
	if( s->sa_family == AF_INET )
	{
		a->type = NA_IP;
		*(int *)&a->ip = ((struct sockaddr_in *)s)->sin_addr.s_addr;
		a->port = ((struct sockaddr_in *)s)->sin_port;
	}
	else if (s->sa_family == AF_IPX)
	{
		a->type = NA_IPX;
		memcpy(&a->ipx[0], ((struct sockaddr_ipx *)s)->sa_netnum, 4);
		memcpy(&a->ipx[4], ((struct sockaddr_ipx *)s)->sa_nodenum, 6);
		a->port = ((struct sockaddr_ipx *)s)->sa_socket;
	}
}


/*
=============
NET_StringToAdr

idnewt
192.246.40.70
12121212.121212121212
=============
*/
bool NET_StringToSockaddr( const char *s, struct sockaddr *sadr )
{
	struct hostent	*h;
	int		val;
	char		copy[MAX_SYSPATH];
	
	memset( sadr, 0, sizeof( *sadr ) );
	// check for an IPX address
	if(( com_strlen( s ) == 21 ) && ( s[8] == '.' ))
	{
		((struct sockaddr_ipx *)sadr)->sa_family = AF_IPX;
		((struct sockaddr_ipx *)sadr)->sa_socket = 0;
		copy[2] = 0;
		DO(0, sa_netnum[0]);
		DO(2, sa_netnum[1]);
		DO(4, sa_netnum[2]);
		DO(6, sa_netnum[3]);
		DO(9, sa_nodenum[0]);
		DO(11, sa_nodenum[1]);
		DO(13, sa_nodenum[2]);
		DO(15, sa_nodenum[3]);
		DO(17, sa_nodenum[4]);
		DO(19, sa_nodenum[5]);
	}
	else
	{
		((struct sockaddr_in *)sadr)->sin_family = AF_INET;
		((struct sockaddr_in *)sadr)->sin_port = 0;

		if( s[0] >= '0' && s[0] <= '9' )
		{
			*(int *)&((struct sockaddr_in *)sadr)->sin_addr = pInet_Addr(s);
		}
		else
		{
			if(( h = pGetHostByName(s)) == 0 )
				return 0;
			*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
		}
	}
	return true;
}

char *NET_AdrToString( netadr_t a )
{
	static char	s[64];

	if( a.type == NA_LOOPBACK )
	{
		com_snprintf( s, sizeof(s), "loopback" );
	}
	else if( a.type == NA_IP )
	{
		com_snprintf( s, sizeof(s), "%i.%i.%i.%i:%hu", a.ip[0], a.ip[1], a.ip[2], a.ip[3], BigShort(a.port));
	}
	else
	{
		com_snprintf( s, sizeof(s), "%02x%02x%02x%02x.%02x%02x%02x%02x%02x%02x:%hu",
		a.ipx[0], a.ipx[1], a.ipx[2], a.ipx[3], a.ipx[4], a.ipx[5], a.ipx[6], a.ipx[7], a.ipx[8], a.ipx[9], 
		BigShort(a.port));
	}
	return s;
}

/*
=============
NET_StringToAdr

idnewt
192.246.40.70
=============
*/
bool NET_StringToAdr( const char *s, netadr_t *a )
{
	struct sockaddr sadr;
	
	if( !NET_StringToSockaddr( s, &sadr ))
		return false;
	
	SockadrToNetadr( &sadr, a );
	return true;
}

/*
==================
NET_GetPacket

Never called by the game logic, just the system event queing
==================
*/
bool NET_GetPacket( netadr_t *net_from, sizebuf_t *net_message )
{
	int 		ret, err;
	struct sockaddr	from;
	int		fromlen;
	int		net_socket;
	int		protocol;

	for( protocol = 0; protocol < 2; protocol++ )
	{
		if( protocol == 0 ) net_socket = ip_socket;
		else net_socket = ipx_socket;

		if( !net_socket ) continue;

		fromlen = sizeof( from );
		ret = pRecvFrom( net_socket, net_message->data, net_message->maxsize, 0, (struct sockaddr *)&from, &fromlen );
		if (ret == SOCKET_ERROR)
		{
			err = pWSAGetLastError();

			if( err == WSAEWOULDBLOCK || err == WSAECONNRESET )
				continue;
			MsgDev( D_ERROR, "NET_GetPacket: %s\n", NET_ErrorString());
			continue;
		}
		if( net_socket == ip_socket )
		{
			memset( ((struct sockaddr_in *)&from)->sin_zero, 0, 8 );
		}

		if( usingSocks && net_socket == ip_socket && memcmp( &from, &socksRelayAddr, fromlen ) == 0 )
		{
			if( ret < 10 || net_message->data[0] != 0 || net_message->data[1] != 0 || net_message->data[2] != 0 || net_message->data[3] != 1 )
				continue;
			net_from->type = NA_IP;
			net_from->ip[0] = net_message->data[4];
			net_from->ip[1] = net_message->data[5];
			net_from->ip[2] = net_message->data[6];
			net_from->ip[3] = net_message->data[7];
			net_from->port = *(short *)&net_message->data[8];
			net_message->readcount = 10;
		}
		else
		{
			SockadrToNetadr( &from, net_from );
			net_message->readcount = 0;
		}

		if( ret == net_message->maxsize )
		{
			MsgDev( D_WARN, "oversize packet from %s\n", NET_AdrToString (*net_from) );
			continue;
		}
		net_message->cursize = ret;
		return true;
	}
	return false;
}

/*
==================
NET_SendPacket
==================
*/
void NET_SendPacket( int length, const void *data, netadr_t to )
{
	int		ret;
	struct sockaddr	addr;
	SOCKET		net_socket;

	switch( to.type )
	{
	case NA_IP:
	case NA_BROADCAST:
		net_socket = ip_socket;
		break;
	case NA_IPX:
	case NA_BROADCAST_IPX:
		net_socket = ipx_socket;		
		break;
	default:
		MsgDev( D_ERROR, "NET_SendPacket: bad address type\n");
		return;
	}

	if( !net_socket ) return;
	NetadrToSockadr( &to, &addr );

	if( usingSocks && to.type == NA_IP )
	{
		socksBuf[0] = 0;	// reserved
		socksBuf[1] = 0;
		socksBuf[2] = 0;	// fragment (not fragmented)
		socksBuf[3] = 1;	// address type: IPV4
		*(int *)&socksBuf[4] = ((struct sockaddr_in *)&addr)->sin_addr.s_addr;
		*(short *)&socksBuf[8] = ((struct sockaddr_in *)&addr)->sin_port;
		memcpy( &socksBuf[10], data, length );
		ret = pSendTo( net_socket, socksBuf, length+10, 0, &socksRelayAddr, sizeof(socksRelayAddr) );
	}
	else ret = pSendTo( net_socket, data, length, 0, &addr, sizeof(addr) );

	if( ret == SOCKET_ERROR )
	{
		int err = pWSAGetLastError();

		// wouldblock is silent
		if( err == WSAEWOULDBLOCK )
			return;

		// some PPP links do not allow broadcasts and return an error
		if(( err == WSAEADDRNOTAVAIL ) && (( to.type == NA_BROADCAST ) || ( to.type == NA_BROADCAST_IPX )))
			return;
		MsgDev( D_ERROR, "NET_SendPacket: %s\n", NET_ErrorString() );
	}
}

/*
==================
NET_IsLANAddress

LAN clients will have their rate var ignored
==================
*/
bool NET_IsLANAddress( netadr_t adr )
{
	int	i;

	if( adr.type == NA_LOOPBACK )
		return true;

	if( adr.type == NA_IPX )
		return true;

	if( adr.type != NA_IP )
		return false;

	// choose which comparison to use based on the class of the address being tested
	// any local adresses of a different class than the address being tested will fail based on the first byte
	if( adr.ip[0] == 127 && adr.ip[1] == 0 && adr.ip[2] == 0 && adr.ip[3] == 1 )
		return true;

	// class A
	if(( adr.ip[0] & 0x80) == 0x00 )
	{
		for( i = 0; i < numIP; i++ )
		{
			if( adr.ip[0] == localIP[i][0] )
				return true;
		}
		// the RFC1918 class a block will pass the above test
		return false;
	}

	// class B
	if(( adr.ip[0] & 0xc0) == 0x80 )
	{
		for( i = 0; i < numIP; i++ )
		{
			if( adr.ip[0] == localIP[i][0] && adr.ip[1] == localIP[i][1] )
				return true;
			// also check against the RFC1918 class b blocks
			if( adr.ip[0] == 172 && localIP[i][0] == 172 && (adr.ip[1] & 0xf0) == 16 && (localIP[i][1] & 0xf0) == 16 )
				return true;
		}
		return false;
	}

	// class C
	for( i = 0; i < numIP; i++ )
	{
		if( adr.ip[0] == localIP[i][0] && adr.ip[1] == localIP[i][1] && adr.ip[2] == localIP[i][2] )
			return true;
		// also check against the RFC1918 class c blocks
		if( adr.ip[0] == 192 && localIP[i][0] == 192 && adr.ip[1] == 168 && localIP[i][1] == 168 )
			return true;
	}
	return false;
}

/*
==================
NET_ShowIP
==================
*/
void NET_ShowIP( void )
{
	int i;

	for( i = 0; i < numIP; i++ )
	{
		Msg( "IP: %i.%i.%i.%i\n", localIP[i][0], localIP[i][1], localIP[i][2], localIP[i][3] );
	}
}

/*
====================
NET_IPSocket
====================
*/
int NET_IPSocket( char *net_interface, int port )
{
	SOCKET		newsocket;
	struct sockaddr_in	address;
	bool		_true = true;
	int		i = 1;
	int		err;

	if( net_interface ) MsgDev( D_INFO, "Opening IP socket: %s:%i\n", net_interface, port );
	else MsgDev( D_INFO, "Opening IP socket: localhost:%i\n", port );

	if(( newsocket = pSocket( AF_INET, SOCK_DGRAM, IPPROTO_UDP )) == INVALID_SOCKET )
	{
		err = pWSAGetLastError();
		if( err != WSAEAFNOSUPPORT )
			MsgDev( D_WARN, "NET_IPSocket: socket: %s\n", NET_ErrorString());
		return 0;
	}
	// make it non-blocking
	if( pIoctlSocket( newsocket, FIONBIO, &_true ) == SOCKET_ERROR )
	{
		MsgDev( D_WARN, "NET_IPSocket: ioctl FIONBIO: %s\n", NET_ErrorString());
		return 0;
	}

	// make it broadcast capable
	if( pSetSockopt( newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i) ) == SOCKET_ERROR )
	{
		MsgDev( D_WARN, "NET_IPSocket: setsockopt SO_BROADCAST: %s\n", NET_ErrorString());
		return 0;
	}

	if( !net_interface || !net_interface[0] || !com_stricmp( net_interface, "localhost"))
		address.sin_addr.s_addr = INADDR_ANY;
	else NET_StringToSockaddr( net_interface, (struct sockaddr *)&address );

	if( port == PORT_ANY ) address.sin_port = 0;
	else address.sin_port = pHtons((short)port);
	address.sin_family = AF_INET;

	if( pBind( newsocket, (void *)&address, sizeof(address) ) == SOCKET_ERROR )
	{
		MsgDev( D_WARN, "UDP_OpenSocket: bind: %s\n", NET_ErrorString());
		pCloseSocket( newsocket );
		return 0;
	}
	return newsocket;
}


/*
====================
NET_OpenSocks
====================
*/
void NET_OpenSocks( int port )
{
	struct sockaddr_in	address;
	bool		rfc1929 = false;
	int		err;
	struct hostent	*h;
	int		len;
	byte		buf[64];

	usingSocks = false;

	MsgDev( D_INFO, "Opening connection to SOCKS server.\n" );

	if(( socks_socket = pSocket( AF_INET, SOCK_STREAM, IPPROTO_TCP )) == INVALID_SOCKET )
	{
		err = pWSAGetLastError();
		MsgDev( D_WARN, "NET_OpenSocks: socket: %s\n", NET_ErrorString());
		return;
	}

	h = pGetHostByName( net_socks_server->string );
	if( h == NULL )
	{
		err = pWSAGetLastError();
		MsgDev( D_WARN, "NET_OpenSocks: gethostbyname: %s\n", NET_ErrorString());
		return;
	}
	if( h->h_addrtype != AF_INET )
	{
		MsgDev( D_WARN, "NET_OpenSocks: gethostbyname: address type was not AF_INET\n");
		return;
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = *(int *)h->h_addr_list[0];
	address.sin_port = pHtons( (short)net_socks_port->integer );

	if( pConnect( socks_socket, (struct sockaddr *)&address, sizeof( address )) == SOCKET_ERROR )
	{
		err = pWSAGetLastError();
		MsgDev( D_WARN, "NET_OpenSocks: connect: %s\n", NET_ErrorString());
		return;
	}

	// send socks authentication handshake
	if( *net_socks_username->string || *net_socks_password->string )
		rfc1929 = true;

	buf[0] = 5; // SOCKS version

	// method count
	if( rfc1929 )
	{
		buf[1] = 2;
		len = 4;
	}
	else
	{
		buf[1] = 1;
		len = 3;
	}
	buf[2] = 0;		// method #1 - method id #00: no authentication
	if( rfc1929 ) buf[2] = 2;	// method #2 - method id #02: username/password

	if( pSend( socks_socket, buf, len, 0 ) == SOCKET_ERROR )
	{
		err = pWSAGetLastError();
		MsgDev( D_WARN, "NET_OpenSocks: send: %s\n", NET_ErrorString());
		return;
	}

	// get the response
	len = pRecv( socks_socket, buf, 64, 0 );
	if( len == SOCKET_ERROR )
	{
		err = pWSAGetLastError();
		MsgDev( D_WARN, "NET_OpenSocks: recv: %s\n", NET_ErrorString());
		return;
	}
	if( len != 2 || buf[0] != 5 )
	{
		MsgDev( D_WARN, "NET_OpenSocks: bad response\n" );
		return;
	}

	switch( buf[1] )
	{
	case 0:	// no authentication
	case 2:	// username/password authentication
		break;
	default:
		MsgDev( D_WARN, "NET_OpenSocks: request denied\n" );
		return;
	}

	// do username/password authentication if needed
	if( buf[1] == 2 )
	{
		int	ulen, plen;

		// build the request
		ulen = com_strlen( net_socks_username->string );
		plen = com_strlen( net_socks_password->string );

		buf[0] = 1;		// username/password authentication version
		buf[1] = ulen;
		if( ulen ) Mem_Copy( &buf[2], net_socks_username->string, ulen );
		buf[2 + ulen] = plen;
		if( plen ) Mem_Copy( &buf[3 + ulen], net_socks_password->string, plen );

		// send it
		if( pSend( socks_socket, buf, 3 + ulen + plen, 0 ) == SOCKET_ERROR )
		{
			err = pWSAGetLastError();
			MsgDev( D_WARN, "NET_OpenSocks: send: %s\n", NET_ErrorString());
			return;
		}

		// get the response
		len = pRecv( socks_socket, buf, 64, 0 );
		if( len == SOCKET_ERROR )
		{
			err = pWSAGetLastError();
			MsgDev( D_WARN, "NET_OpenSocks: recv: %s\n", NET_ErrorString());
			return;
		}
		if( len != 2 || buf[0] != 1 )
		{
			MsgDev( D_WARN, "NET_OpenSocks: bad response\n" );
			return;
		}
		if( buf[1] != 0 )
		{
			MsgDev( D_WARN, "NET_OpenSocks: authentication failed\n" );
			return;
		}
	}

	// send the UDP associate request
	buf[0] = 5; // SOCKS version
	buf[1] = 3; // command: UDP associate
	buf[2] = 0; // reserved
	buf[3] = 1; // address type: IPV4
	*(int *)&buf[4] = INADDR_ANY;
	*(short *)&buf[8] = pHtons((short)port); // port

	if( pSend( socks_socket, buf, 10, 0 ) == SOCKET_ERROR )
	{
		err = pWSAGetLastError();
		MsgDev( D_WARN, "NET_OpenSocks: send: %s\n", NET_ErrorString());
		return;
	}

	// get the response
	len = pRecv( socks_socket, buf, 64, 0 );
	if( len == SOCKET_ERROR )
	{
		err = pWSAGetLastError();
		MsgDev( D_WARN, "NET_OpenSocks: recv: %s\n", NET_ErrorString());
		return;
	}
	if( len < 2 || buf[0] != 5 )
	{
		MsgDev( D_WARN, "NET_OpenSocks: bad response\n" );
		return;
	}
	// check completion code
	if( buf[1] != 0 )
	{
		MsgDev( D_WARN, "NET_OpenSocks: request denied: %i\n", buf[1] );
		return;
	}
	if( buf[3] != 1 )
	{
		MsgDev( D_WARN, "NET_OpenSocks: relay address is not IPV4: %i\n", buf[3] );
		return;
	}
	((struct sockaddr_in *)&socksRelayAddr)->sin_family = AF_INET;
	((struct sockaddr_in *)&socksRelayAddr)->sin_addr.s_addr = *(int *)&buf[4];
	((struct sockaddr_in *)&socksRelayAddr)->sin_port = *(short *)&buf[8];
	memset(((struct sockaddr_in *)&socksRelayAddr)->sin_zero, 0, 8 );
	usingSocks = true;
}

/*
====================
NET_IPXSocket
====================
*/
int NET_IPXSocket( int port )
{
	SOCKET		newsocket;
	struct sockaddr_ipx	address;
	int   		_true = 1;
	int   		err;

	if(( newsocket = pSocket( AF_IPX, SOCK_DGRAM, NSPROTO_IPX )) == INVALID_SOCKET )
	{
		err = pWSAGetLastError();
		if( err != WSAEAFNOSUPPORT )
			MsgDev( D_WARN, "IPX_Socket: socket: %s\n", NET_ErrorString());
		return 0;
	}

	// make it non-blocking
	if( pIoctlSocket( newsocket, FIONBIO, &_true ) == SOCKET_ERROR )
	{
		MsgDev( D_WARN, "IPX_Socket: ioctl FIONBIO: %s\n", NET_ErrorString());
		return 0;
	}

	// make it broadcast capable
	if( pSetSockopt( newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&_true, sizeof( _true ) ) == SOCKET_ERROR )
	{
		MsgDev( D_WARN, "IPX_Socket: setsockopt SO_BROADCAST: %s\n", NET_ErrorString());
		return 0;
	}

	address.sa_family = AF_IPX;
	memset( address.sa_netnum, 0, 4 );
	memset( address.sa_nodenum, 0, 6 );
	if( port == PORT_ANY ) address.sa_socket = 0;
	else address.sa_socket = pHtons( (short)port );

	if( pBind( newsocket, (void *)&address, sizeof(address)) == SOCKET_ERROR )
	{
		MsgDev( D_WARN, "IPX_Socket: bind: %s\n", NET_ErrorString());
		pCloseSocket( newsocket );
		return 0;
	}
	return newsocket;
}


/*
====================
NET_OpenIPX
====================
*/
void NET_OpenIPX( void )
{
	int	port;

	port = Cvar_Get( "net_port", va( "%i", PORT_SERVER ), CVAR_LATCH, "network ip address" )->integer;
	ipx_socket = NET_IPXSocket( port );
}



/*
=====================
NET_GetLocalAddress
=====================
*/
void NET_GetLocalAddress( void )
{
	string		hostname;
	int		error, ip, i = 0;
	struct hostent	*hostInfo;
	char		*p;

	if( pGetHostName( hostname, MAX_STRING ) == SOCKET_ERROR )
	{
		error = pWSAGetLastError();
		return;
	}

	hostInfo = pGetHostByName( hostname );
	if( !hostInfo )
	{
		error = pWSAGetLastError();
		return;
	}

	MsgDev( D_INFO, "Hostname: %s\n", hostInfo->h_name );
	while(( p = hostInfo->h_aliases[i++] ) != NULL )
		MsgDev( D_INFO, "Alias: %s\n", p );

	if( hostInfo->h_addrtype != AF_INET )
		return;

	numIP = 0;
	while(( p = hostInfo->h_addr_list[numIP] ) != NULL && numIP < MAX_IPS )
	{
		ip = pNtohl( *(int *)p);
		localIP[numIP][0] = p[0];
		localIP[numIP][1] = p[1];
		localIP[numIP][2] = p[2];
		localIP[numIP][3] = p[3];
		MsgDev( D_INFO, "IP: %i.%i.%i.%i\n", (ip>>24) & 0xff, (ip>>16) & 0xff, (ip>>8) & 0xff, ip & 0xff );
		numIP++;
	}
}

/*
====================
NET_OpenIP
====================
*/
void NET_OpenIP( void )
{
	cvar_t	*ip;
	int	i, port;

	ip = Cvar_Get( "net_ip", "localhost", CVAR_LATCH, "network ip address" );
	port = Cvar_Get( "net_port", va( "%i", PORT_SERVER ), CVAR_LATCH, "network default port" )->integer;

	// automatically scan for a valid port, so multiple
	// dedicated servers can be started without requiring
	// a different net_port for each one
	for( i = 0; i < 10; i++ )
	{
		ip_socket = NET_IPSocket( ip->string, port + i );
		if( ip_socket )
		{
			Cvar_SetValue( "net_port", port + i );
			if( net_socks_enabled->integer )
				NET_OpenSocks( port + i );
			NET_GetLocalAddress();
			return;
		}
	}
	MsgDev( D_WARN, "Couldn't allocate IP port\n" );
}

/*
====================
NET_GetCvars
====================
*/
static bool NET_GetCvars( void )
{
	bool	modified = false;

	if( net_socks_enabled && net_socks_enabled->modified )
		modified = true;
	net_socks_enabled = Cvar_Get( "net_socksenabled", "0", CVAR_LATCH|CVAR_ARCHIVE, "network using socks" );

	if( net_socks_server && net_socks_server->modified )
		modified = true;
	net_socks_server = Cvar_Get( "net_socksserver", "", CVAR_LATCH|CVAR_ARCHIVE, "network server socket" );

	if( net_socks_port && net_socks_port->modified )
		modified = true;
	net_socks_port = Cvar_Get( "net_socksport", "1080", CVAR_LATCH|CVAR_ARCHIVE, "using socks port" );

	if( net_socks_username && net_socks_username->modified )
		modified = true;
	net_socks_username = Cvar_Get( "net_socksusername", "", CVAR_LATCH|CVAR_ARCHIVE, "autorization login" );

	if( net_socks_password && net_socks_password->modified )
		modified = true;
	net_socks_password = Cvar_Get( "net_sockspassword", "", CVAR_LATCH|CVAR_ARCHIVE, "autorization password" );

	return modified;
}

/*
====================
NET_Config
====================
*/
void NET_Config( bool net_enable )
{
	bool	modified, start, stop;

	// get any latched changes to cvars
	modified = NET_GetCvars();

	// if enable state is the same and no cvars were modified, we have nothing to do
	if( net_enable == net_active && !modified )
		return;

	if( net_enable == net_active )
	{
		if( net_enable )
		{
			stop = true;
			start = true;
		}
		else
		{
			stop = false;
			start = false;
		}
	}
	else
	{
		if( net_enable )
		{
			stop = false;
			start = true;
		}
		else
		{
			stop = true;
			start = false;
		}
		net_active = net_enable;
	}

	if( stop )
	{
		if( ip_socket && ip_socket != INVALID_SOCKET )
		{
			pCloseSocket( ip_socket );
			ip_socket = 0;
		}

		if( socks_socket && socks_socket != INVALID_SOCKET )
		{
			pCloseSocket( socks_socket );
			socks_socket = 0;
		}
		if( ipx_socket && ipx_socket != INVALID_SOCKET )
		{
			pCloseSocket( ipx_socket );
			ipx_socket = 0;
		}
	}
	if( start )
	{
		NET_OpenIP();
		NET_OpenIPX();
	}
}


/*
====================
NET_Init
====================
*/
void NET_Init( void )
{
	int	r;

	NET_OpenWinSock();	// loading wsock32.dll
	r = pWSAStartup(MAKEWORD( 1, 1 ), &winsockdata );
	if( r )
	{
		MsgDev( D_WARN, "NET_Init: winsock initialization failed: %d\n", r );
		return;
	}

	winsockInitialized = true;
	MsgDev( D_NOTE, "NET_Init()\n" );
	NET_GetCvars();	// register cvars
	NET_Config( true );	// FIXME: testing!
}


/*
====================
NET_Shutdown
====================
*/
void NET_Shutdown( void )
{
	if( !winsockInitialized )
		return;
	NET_Config( false );
	pWSACleanup();
	NET_FreeWinSock();
	winsockInitialized = false;
}