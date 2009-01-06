//=======================================================================
//			Copyright XashXT Group 2008 �
//		        cl_game.c - client dlls interaction
//=======================================================================

#include "common.h"
#include "client.h"
#include "byteorder.h"
#include "matrix_lib.h"
#include "com_library.h"
#include "const.h"

/*
====================
CL_GetClientEntity

Render callback for studio models
====================
*/
edict_t *CL_GetEdictByIndex( int index )
{
	if( index < 0 || index > clgame.numEntities )
	{
		if( index == -1 ) return &cl.viewent;
		MsgDev( D_ERROR, "CL_GetEntityByIndex: invalid entindex %i\n", index );
		return NULL;
	}
	return EDICT_NUM( index );
}

/*
====================
CL_GetLocalPlayer

Render callback for studio models
====================
*/
edict_t *CL_GetLocalPlayer( void )
{
	return EDICT_NUM( cl.playernum + 1 );
}

/*
====================
CL_GetMaxlients

Render callback for studio models
====================
*/
int CL_GetMaxClients( void )
{
	return com.atoi( cl.configstrings[CS_MAXCLIENTS] );
}

/*
================
CL_FadeColor
================
*/
float *CL_FadeColor( float starttime, float endtime )
{
	static vec4_t	color;
	float		time, fade_time;

	if( starttime == 0 ) return NULL;
	time = (cls.realtime * 0.001f) - starttime;
	if( time >= endtime ) return NULL;

	// fade time is 1/4 of endtime
	fade_time = endtime / 4;
	fade_time = bound( 0.3f, fade_time, 10.0f );

	// fade out
	if((endtime - time) < fade_time)
		color[3] = (endtime - time) * 1.0f / fade_time;
	else color[3] = 1.0;
	color[0] = color[1] = color[2] = 1.0f;

	return color;
}

void CL_DrawHUD( int state )
{
	cls.dllFuncs.pfnRedraw( cl.time * 0.001f, state );
}

void CL_CopyTraceResult( TraceResult *out, trace_t trace )
{
	if( !out ) return;

	out->fAllSolid = trace.allsolid;
	out->fStartSolid = trace.startsolid;
	out->fStartStuck = trace.startstuck;
	out->flFraction = trace.fraction;
	out->iStartContents = trace.startcontents;
	out->iContents = trace.contents;
	out->iHitgroup = trace.hitgroup;
	out->flPlaneDist = trace.plane.dist;
	VectorCopy( trace.endpos, out->vecEndPos );
	VectorCopy( trace.plane.normal, out->vecPlaneNormal );

	if( trace.surface )
		out->pTexName = trace.surface->name;
	else out->pTexName = NULL;
	out->pHit = trace.ent;
}

static void CL_CreateUserMessage( int lastnum, const char *szMsgName, int svc_num, int iSize, pfnUserMsgHook pfn )
{
	user_message_t	*msg;

	if( lastnum == clgame.numMessages )
	{
		if( clgame.numMessages == MAX_USER_MESSAGES )
		{
			MsgDev( D_ERROR, "CL_CreateUserMessage: user messages limit is out\n" );
			return;
		}
		clgame.numMessages++;
	}

	msg = clgame.msg[lastnum];

	// clear existing or allocate new one
	if( msg ) Mem_Set( msg, 0, sizeof( *msg ));
	else msg = clgame.msg[lastnum] = Mem_Alloc( cls.mempool, sizeof( *msg ));

	com.strncpy( msg->name, szMsgName, CS_SIZE );
	msg->number = svc_num;
	msg->size = iSize;
	msg->func = pfn;
}

void CL_LinkUserMessage( char *pszName, const int svc_num )
{
	user_message_t	*msg;
	char		*end;
	char		msgName[CS_SIZE];
	int		i, msgSize;

	if( !pszName || !*pszName ) return; // ignore blank names

	com.strncpy( msgName, pszName, CS_SIZE );
	end = com.strchr( msgName, '@' );
	if( !end )
	{
		MsgDev( D_ERROR, "CL_LinkUserMessage: can't register message %s\n", msgName );
		return;
	}

	msgSize = com.atoi( end + 1 );
	msgName[end-msgName] = '\0'; // remove size description from MsgName

	// search message by name to link with
	for( i = 0; i < clgame.numMessages; i++ )
	{
		msg = clgame.msg[i];
		if( !msg ) continue;

		if( !com.strcmp( msg->name, msgName ))
		{
			msg->number = svc_num;
			msg->size = msgSize;
			return;
		}
	}

	// create an empty message
	CL_CreateUserMessage( i, msgName, svc_num, msgSize, NULL );
}

void CL_SortUserMessages( void )
{
	// FIXME: implement
}

void CL_ParseUserMessage( sizebuf_t *net_buffer, int svc_num )
{
	user_message_t	*msg;
	int		i, iSize;
	byte		*pbuf;

	// NOTE: any user message parse on engine, not in client.dll
	if( svc_num >= clgame.numMessages )
	{
		// unregister message can't be parsed
		Host_Error( "CL_ParseUserMessage: illegible server message %d\n", svc_num );
		return;
	}

	// search for svc_num
	for( i = 0; i < clgame.numMessages; i++ )
	{
		msg = clgame.msg[i];	
		if( !msg ) continue;
		if( msg->number == svc_num )
			break;
	}

	if( i == clgame.numMessages || !msg )
	{
		// unregistered message ?
		Host_Error( "CL_ParseUserMessage: illegible server message %d\n", svc_num );
		return;
	}

	iSize = msg->size;
	pbuf = NULL;

	// message with variable sizes receive an actual size as first byte
	if( iSize == -1 ) iSize = MSG_ReadByte( net_buffer );
	if( iSize > 0 ) pbuf = Mem_Alloc( cls.private, iSize );

	// parse user message into buffer
	MSG_ReadData( net_buffer, pbuf, iSize );

	if( msg->func ) msg->func( msg->name, iSize, pbuf );
	else MsgDev( D_WARN, "CL_ParseUserMessage: %s not hooked\n", msg->name );
	if( pbuf ) Mem_Free( pbuf );
}

void CL_InitEdict( edict_t *pEdict )
{
	Com_Assert( pEdict == NULL );
	Com_Assert( pEdict->pvClientData != NULL );

	pEdict->v.pContainingEntity = pEdict; // make cross-links for consistency
	pEdict->pvClientData = (cl_priv_t *)Mem_Alloc( cls.mempool,  sizeof( cl_priv_t ));
	pEdict->serialnumber = NUM_FOR_EDICT( pEdict );	// merged on first update
	pEdict->free = false;
}

void CL_FreeEdict( edict_t *pEdict )
{
	Com_Assert( pEdict == NULL );
	Com_Assert( pEdict->free );

	// unlink from world
	// CL_UnlinkEdict( pEdict );

	if( pEdict->pvClientData ) Mem_Free( pEdict->pvClientData );
	Mem_Set( &pEdict->v, 0, sizeof( entvars_t ));

	pEdict->pvClientData = NULL;

	// mark edict as freed
	pEdict->freetime = cl.time * 0.001f;
	pEdict->serialnumber = 0;
	pEdict->free = true;
}

edict_t *CL_AllocEdict( void )
{
	edict_t	*pEdict;
	int	i;

	for( i = 0; i < clgame.numEntities; i++ )
	{
		pEdict = EDICT_NUM( i );
		// the first couple seconds of client time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if( pEdict->free && ( pEdict->freetime < 2.0f || ((cl.time * 0.001f) - pEdict->freetime) > 0.5f ))
		{
			CL_InitEdict( pEdict );
			return pEdict;
		}
	}

	if( i == clgame.maxEntities )
		Host_Error( "CL_AllocEdict: no free edicts\n" );

	clgame.numEntities++;
	pEdict = EDICT_NUM( i );
	CL_InitEdict( pEdict );

	return pEdict;
}

void CL_FreeEdicts( void )
{
	int	i;
	edict_t	*ent;

	for( i = 0; i < clgame.numEntities; i++ )
	{
		ent = EDICT_NUM( i );
		if( ent->free ) continue;
		CL_FreeEdict( ent );
	}

	// clear globals
	StringTable_Clear( clgame.hStringTable );
	clgame.numEntities = 0;
}

/*
===============================================================================
	CGame Builtin Functions

===============================================================================
*/

/*
=========
pfnMemAlloc

=========
*/
static void *pfnMemAlloc( size_t cb, const char *filename, const int fileline )
{
	return com.malloc( cls.private, cb, filename, fileline );
}

/*
=========
pfnMemFree

=========
*/
static void pfnMemFree( void *mem, const char *filename, const int fileline )
{
	com.free( mem, filename, fileline );
}

/*
=============
pfnLoadShader

=============
*/
shader_t pfnLoadShader( const char *szShaderName )
{
	if( !re ) return 0; // render not initialized
	if( !szShaderName || !*szShaderName )
	{
		MsgDev( D_ERROR, "CL_LoadShader: invalid shadername\n" );
		return -1;
	}
	return re->RegisterShader( szShaderName, SHADER_NOMIP );
}

/*
=============
pfnFillRGBA

=============
*/
void pfnFillRGBA( int x, int y, int width, int height, const float *color, float alpha )
{
	if( !re ) return;

	re->SetColor( GetRGBA( color[0], color[1], color[2], alpha ));
	re->DrawFill( x, y, width, height );
	re->SetColor( NULL );
}

/*
=============
pfnDrawImageExt

=============
*/
void pfnDrawImageExt( HSPRITE shader, int x, int y, int w, int h, float s1, float t1, float s2, float t2 )
{
	if( !re ) return;

	re->DrawStretchPic( x, y, w, h, s1, t1, s2, t2, shader );
	re->SetColor( NULL );
}

/*
=============
pfnSetColor

=============
*/
void pfnSetColor( float r, float g, float b, float a )
{
	if( !re ) return; // render not initialized
	re->SetColor( GetRGBA( r, g, b, a ));
}

/*
=============
pfnRegisterVariable

=============
*/
void pfnRegisterVariable( const char *szName, const char *szValue, int flags, const char *szDesc )
{
	// FIXME: translate client.dll flags to real cvar flags
	Cvar_Get( szName, szValue, flags, szDesc );
}

/*
=============
pfnCvarSetValue

=============
*/
void pfnCvarSetValue( const char *cvar, float value )
{
	Cvar_SetValue( cvar, value );
}

/*
=============
pfnGetCvarFloat

=============
*/
float pfnGetCvarFloat( const char *szName )
{
	return Cvar_VariableValue( szName );
}

/*
=============
pfnGetCvarString

=============
*/
char* pfnGetCvarString( const char *szName )
{
	return Cvar_VariableString( szName );
}

/*
=============
pfnGetCvarString

=============
*/
void pfnAddCommand( const char *cmd_name, xcommand_t func, const char *cmd_desc )
{
	// NOTE: if( func == NULL ) cmd will be forwarded to a server
	Cmd_AddCommand( cmd_name, func, cmd_desc );
}

/*
=============
pfnHookUserMsg

=============
*/
void pfnHookUserMsg( const char *szMsgName, pfnUserMsgHook pfn )
{
	user_message_t	*msg;
	int		i;

	// ignore blank names
	if( !szMsgName || !*szMsgName ) return;	

	// duplicate call can change msgFunc	
	for( i = 0; i < clgame.numMessages; i++ )
	{
		msg = clgame.msg[i];	
		if( !msg ) continue;

		if( !com.strcmp( szMsgName, msg->name ))
		{
			if( msg->func != pfn )
				msg->func = pfn;
			return;
		}
	}

	// allocate a new one
	CL_CreateUserMessage( i, szMsgName, 0, 0, pfn );
}

/*
=============
pfnServerCmd

=============
*/
void pfnServerCmd( const char *szCmdString )
{
	// server command adding in cmds queue
	Cbuf_AddText( va( "cmd %s", szCmdString ));
}

/*
=============
pfnClientCmd

=============
*/
void pfnClientCmd( const char *szCmdString )
{
	// client command executes immediately
	Cmd_ExecuteString( szCmdString );
}

/*
=============
pfnGetPlayerInfo

=============
*/
void pfnGetPlayerInfo( int player_num, hud_player_info_t *pinfo )
{
	// FIXME: implement
	static hud_player_info_t null_info;

	Mem_Copy( pinfo, &null_info, sizeof( null_info ));
}

/*
=============
pfnTextMessageGet

=============
*/
client_textmessage_t *pfnTextMessageGet( const char *pName )
{
	// FIXME: implement or move to client.dll
	static client_textmessage_t null_msg;

	return &null_msg;
}

/*
=============
pfnCmdArgc

=============
*/
int pfnCmdArgc( void )
{
	return Cmd_Argc();
}

/*
=============
pfnCmdArgv

=============
*/	
char *pfnCmdArgv( int argc )
{
	if( argc >= 0 && argc < Cmd_Argc())
		return Cmd_Argv( argc );
	return "";
}

/*
=============
pfnPlaySoundByName

=============
*/
void pfnPlaySoundByName( const char *szSound, float volume, int pitch, const float *org )
{
	S_StartLocalSound( szSound, volume, pitch, org );
}

/*
=============
pfnPlaySoundByIndex

=============
*/
void pfnPlaySoundByIndex( int iSound, float volume, int pitch, const float *org )
{
	// make sure what we in-bounds
	iSound = bound( 0, iSound, MAX_SOUNDS );

	if( cl.sound_precache[iSound] == 0 )
	{
		MsgDev( D_ERROR, "CL_PlaySoundByIndex: invalid sound handle %i\n", iSound );
		return;
	}
	S_StartSound( org, cl.playernum + 1, CHAN_AUTO, cl.sound_precache[iSound], volume, ATTN_NORM, pitch );
}

/*
=============
pfnDrawCenterPrint

called each frame
=============
*/
void pfnDrawCenterPrint( void )
{
	char	*start;
	int	l, x, y, w;
	float	*color;

	if( !cl.centerPrintTime ) return;
	color = CL_FadeColor( cl.centerPrintTime * 0.001f, scr_centertime->value );
	if( !color ) 
	{
		cl.centerPrintTime = 0;
		return;
	}

	re->SetColor( color );
	start = cl.centerPrint;
	y = cl.centerPrintY - cl.centerPrintLines * BIGCHAR_HEIGHT / 2;

	while( 1 )
	{
		char	linebuffer[1024];

		for ( l = 0; l < 50; l++ )
		{
			if ( !start[l] || start[l] == '\n' )
				break;
			linebuffer[l] = start[l];
		}
		linebuffer[l] = 0;

		w = cl.centerPrintCharWidth * com.cstrlen( linebuffer );
		x = ( SCREEN_WIDTH - w )>>1;

		SCR_DrawStringExt( x, y, cl.centerPrintCharWidth, BIGCHAR_HEIGHT, linebuffer, color, false );

		y += cl.centerPrintCharWidth * 1.5;
		while( *start && ( *start != '\n' )) start++;
		if( !*start ) break;
		start++;
	}
	if( re ) re->SetColor( NULL );
}

/*
=============
pfnCenterPrint

called once from message
=============
*/
void pfnCenterPrint( const char *text, int y, int charWidth )
{
	char	*s;

	com.strncpy( cl.centerPrint, text, sizeof( cl.centerPrint ));
	cl.centerPrintTime = cls.realtime;
	cl.centerPrintY = y;
	cl.centerPrintCharWidth = charWidth;

	// count the number of lines for centering
	cl.centerPrintLines = 1;
	s = cl.centerPrint;
	while( *s )
	{
		if( *s == '\n' )
			cl.centerPrintLines++;
		s++;
	}
}

/*
=============
pfnDrawCharacter

=============
*/
int pfnDrawCharacter( int x, int y, int width, int height, int number )
{
	if( number < 32 || number > 255 )
	{
		MsgDev( D_WARN, "SCR_DrawChar: passed non-printable character %c\n", (char )number );
		return false;
	}

	SCR_DrawChar( x, y, width, height, number );
	if( re ) re->SetColor( NULL );
	return true;
}

/*
=============
pfnDrawString

=============
*/
void pfnDrawString( int x, int y, int width, int height, const char *text )
{
	if( !text || !*text )
	{
		MsgDev( D_ERROR, "SCR_DrawStringExt: passed null string!\n" );
		return;
	}

	SCR_DrawStringExt( x, y, width, height, text, g_color_table[7], false ); 
	if( re ) re->SetColor( NULL );
}

/*
=============
pfnGetImageSize

=============
*/
void pfnGetDrawParms( int *w, int *h, int *f, int frame, shader_t shader )
{
	if( re ) re->GetParms( w, h, f, frame, shader );
	else
	{
		if( w ) *w = 0;
		if( h ) *h = 0;
		if( f ) *f = 1;
	}
}

/*
=============
pfnSetDrawParms

=============
*/
void pfnSetDrawParms( shader_t handle, kRenderMode_t rendermode, int frame )
{
	if( re ) re->SetParms( handle, rendermode, frame );
}

/*
=============
pfnGetViewAngles

return interpolated angles from previous frame
=============
*/
void pfnGetViewAngles( float *angles )
{
	if( angles == NULL ) return;
	VectorCopy( cl.refdef.viewangles, angles );
}

/*
=============
pfnIsSpectateOnly

=============
*/
int pfnIsSpectateOnly( void )
{
	// FIXME: implement
	return 0;
}

/*
=============
pfnGetClientTime

=============
*/
float pfnGetClientTime( void )
{
	return cl.time * 0.001f;
}

/*
=============
pfnGetMaxClients

=============
*/
int pfnGetMaxClients( void )
{
	return com.atoi( cl.configstrings[CS_MAXCLIENTS] );
}

/*
=============
pfnGetViewModel

can return NULL
=============
*/
edict_t* pfnGetViewModel( void )
{
	return &cl.viewent;
}

/*
=============
pfnMakeLevelShot

force to make levelshot
=============
*/
void pfnMakeLevelShot( void )
{
	if( !cl.need_levelshot ) return;

	Con_ClearNotify();

	// make levelshot at nextframe()
	Cbuf_ExecuteText( EXEC_APPEND, "levelshot\n" );
}

/*
=============
pfnPointContents

=============
*/
static int pfnPointContents( const float *rgflVector )
{
	return CL_PointContents( rgflVector );
}

/*
=============
pfnTraceLine

=============
*/
static void pfnTraceLine( const float *v1, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr )
{
	trace_t		trace;
	int		move;

	move = (fNoMonsters) ? MOVE_NOMONSTERS : MOVE_NORMAL;

	if( IS_NAN(v1[0]) || IS_NAN(v1[1]) || IS_NAN(v1[2]) || IS_NAN(v2[0]) || IS_NAN(v1[2]) || IS_NAN(v2[2] ))
		Host_Error( "CL_Trace: NAN errors detected ('%f %f %f', '%f %f %f'\n", v1[0], v1[1], v1[2], v2[0], v2[1], v2[2] );

	trace = CL_Trace( v1, vec3_origin, vec3_origin, v2, move, pentToSkip, CL_ContentsMask( pentToSkip ));
	CL_CopyTraceResult( ptr, trace );
}

/*
=============
CL_AllocString

=============
*/
string_t CL_AllocString( const char *szValue )
{
	return StringTable_SetString( clgame.hStringTable, szValue );
}		

/*
=============
CL_GetString

=============
*/
const char *CL_GetString( string_t iString )
{
	return StringTable_GetString( clgame.hStringTable, iString );
}

static triapi_t gTriApi =
{
	sizeof( triapi_t ),	
};
					
// engine callbacks
static cl_enginefuncs_t gEngfuncs = 
{
	sizeof( cl_enginefuncs_t ),
	pfnMemAlloc,
	pfnMemFree,
	pfnLoadShader,
	pfnFillRGBA,
	pfnDrawImageExt,
	pfnSetColor,
	pfnRegisterVariable,
	pfnCvarSetValue,
	pfnGetCvarFloat,
	pfnGetCvarString,
	pfnAddCommand,
	pfnHookUserMsg,
	pfnServerCmd,
	pfnClientCmd,
	pfnGetPlayerInfo,
	pfnTextMessageGet,
	pfnCmdArgc,
	pfnCmdArgv,	
	pfnAlertMessage,
	pfnPlaySoundByName,
	pfnPlaySoundByIndex,	
	AngleVectors,
	pfnDrawCenterPrint,
	pfnCenterPrint,
	pfnDrawCharacter,
	pfnDrawString,		
	pfnGetDrawParms,
	pfnSetDrawParms,
	pfnGetViewAngles,
	CL_GetEdictByIndex,
	CL_GetLocalPlayer,
	pfnIsSpectateOnly,
	pfnGetClientTime,
	pfnGetMaxClients,
	pfnGetViewModel,
	pfnMakeLevelShot,						
	pfnPointContents,
	pfnTraceLine,
	pfnRandomLong,
	pfnRandomFloat,
	pfnLoadFile,
	pfnFileExists,
	pfnGetGameDir,				
	Host_Error,
	&gTriApi
};

/*
====================
StudioEvent

Event callback for studio models
====================
*/
void CL_StudioEvent( dstudioevent_t *event, edict_t *pEdict )
{
	cls.dllFuncs.pfnStudioEvent( event, pEdict );
}

void CL_UnloadProgs( void )
{
	// initialize game
	cls.dllFuncs.pfnShutdown();

	StringTable_Delete( clgame.hStringTable );
	Com_FreeLibrary( cls.game );
	Mem_FreePool( &cls.mempool );
	Mem_FreePool( &cls.private );
}

bool CL_LoadProgs( const char *name )
{
	static CLIENTAPI		GetClientAPI;
	string			libname;
	edict_t			*e;
	int			i;

	if( cls.game ) CL_UnloadProgs();

	// fill it in
	com.snprintf( libname, MAX_STRING, "bin/%s.dll", name );
	cls.mempool = Mem_AllocPool( "Client Edicts Zone" );
	cls.private = Mem_AllocPool( "Client Private Zone" );

	cls.game = Com_LoadLibrary( libname );
	if( !cls.game ) return false;

	GetClientAPI = (CLIENTAPI)Com_GetProcAddress( cls.game, "CreateAPI" );

	if( !GetClientAPI )
	{
          	MsgDev( D_ERROR, "CL_LoadProgs: failed to get address of CreateAPI proc\n" );
		return false;
	}

	if( !GetClientAPI( &cls.dllFuncs, &gEngfuncs, INTERFACE_VERSION ))
	{
		MsgDev( D_ERROR, "CL_LoadProgs: can't init client API\n" );
		return false;
	}

	// 65535 unique strings should be enough ...
	clgame.hStringTable = StringTable_Create( "Client", 0x10000 );
	clgame.maxEntities = host.max_edicts;	// FIXME: must come from CS_MAXENTITIES
	clgame.maxClients = Host_MaxClients();
	cls.edicts = Mem_Alloc( cls.mempool, sizeof( edict_t ) * clgame.maxEntities );

	// register svc_bad message
	pfnHookUserMsg( "bad", NULL );
	CL_LinkUserMessage( "bad@0", svc_bad );

	for( i = 0, e = EDICT_NUM( 0 ); i < clgame.maxEntities; i++, e++ )
		e->free = true; // mark all edicts as freed

	// initialize game
	cls.dllFuncs.pfnInit();

	return true;
}