//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        cl_game.c - client dlls interaction
//=======================================================================

#include "common.h"
#include "client.h"
#include "byteorder.h"
#include "matrix_lib.h"
#include "com_library.h"
#include "const.h"
#include "triangle_api.h"
#include "effects_api.h"

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
		if( index == VMODEL_ENTINDEX ) return &cl.viewent;
		if( index == WMODEL_ENTINDEX ) return NULL;
		MsgDev( D_ERROR, "CL_GetEntityByIndex: invalid entindex %i\n", index );
		return NULL;
	}
	return EDICT_NUM( index );
}

/*
====================
StudioEvent

Event callback for studio models
====================
*/
void CL_StudioEvent( dstudioevent_t *event, edict_t *pEdict )
{
	clgame.dllFuncs.pfnStudioEvent( event, pEdict );
}

/*
====================
Studio_FxTransform

apply fxtransforms for each studio bone
====================
*/
void CL_StudioFxTransform( edict_t *ent, float transform[4][4] )
{
	clgame.dllFuncs.pfnStudioFxTransform( ent, transform );
}

static trace_t CL_TraceToss( edict_t *tossent, edict_t *ignore)
{
	int	i;
	float	gravity;
	vec3_t	move, end;
	vec3_t	original_origin;
	vec3_t	original_velocity;
	vec3_t	original_angles;
	vec3_t	original_avelocity;
	trace_t	trace;

	VectorCopy( tossent->v.origin, original_origin );
	VectorCopy( tossent->v.velocity, original_velocity );
	VectorCopy( tossent->v.angles, original_angles );
	VectorCopy( tossent->v.avelocity, original_avelocity );
	gravity = tossent->v.gravity * clgame.movevars.gravity * 0.05;

	for( i = 0; i < 200; i++ )
	{
		// LordHavoc: sanity check; never trace more than 10 seconds
		CL_CheckVelocity( tossent );
		tossent->v.velocity[2] -= gravity;
		VectorMA( tossent->v.angles, 0.05, tossent->v.avelocity, tossent->v.angles );
		VectorScale( tossent->v.velocity, 0.05, move );
		VectorAdd( tossent->v.origin, move, end );
		trace = CL_Trace( tossent->v.origin, tossent->v.mins, tossent->v.maxs, end, MOVE_NORMAL, tossent, CL_ContentsMask( tossent ));
		VectorCopy( trace.endpos, tossent->v.origin );
		if( trace.fraction < 1 ) break;
	}
	VectorCopy( original_origin, tossent->v.origin );
	VectorCopy( original_velocity, tossent->v.velocity );
	VectorCopy( original_angles, tossent->v.angles );
	VectorCopy( original_avelocity, tossent->v.avelocity );

	return trace;
}

/*
====================
CL_RenderTrace

simplified version of CL_Trace for renderer purposes
====================
*/
bool CL_RenderTrace( const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end )
{
	return (CL_Trace( start, mins, maxs, end, MOVE_NORMAL, NULL, MASK_SOLID ).fraction == 1) ? true : false;
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
CL_FadeAlpha
================
*/
void CL_FadeAlpha( float starttime, float endtime, rgba_t color )
{
	float	time, fade_time;

	if( starttime == 0 )
	{
		MakeRGBA( color, 255, 255, 255, 255 );
		return;
	}
	time = (cls.realtime * 0.001f) - starttime;
	if( time >= endtime )
	{
		MakeRGBA( color, 255, 255, 255, 0 );
		return;
	}

	// fade time is 1/4 of endtime
	fade_time = endtime / 4;
	fade_time = bound( 0.3f, fade_time, 10.0f );

	color[0] = color[1] = color[2] = 255;

	// fade out
	if(( endtime - time ) < fade_time )
		color[3] = bound( 0, ((endtime - time) * 1.0f / fade_time) * 255, 255 );
	else color[3] = 255;
}

void CL_DrawHUD( int state )
{
	if( state == CL_LOADING )
	{
		// fill unused entries with black color
		SCR_FillRect( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, g_color_table[0] );
	}

	if( state == CL_ACTIVE && !cl.video_prepped )
		state = CL_LOADING;

	if( state == CL_ACTIVE && cl_paused->integer )
		state = CL_PAUSED;

	clgame.dllFuncs.pfnRedraw( cl.time * 0.001f, state );

	if( state == CL_ACTIVE )
	{
		clgame.dllFuncs.pfnFrame( cl.time * 0.001f );
	}
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

	out->pTexName = trace.pTexName;
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
	if( iSize > 0 ) pbuf = Mem_Alloc( clgame.private, iSize );

	// parse user message into buffer
	MSG_ReadData( net_buffer, pbuf, iSize );

	if( msg->func ) msg->func( msg->name, iSize, pbuf );
	else MsgDev( D_WARN, "CL_ParseUserMessage: %s not hooked\n", msg->name );
	if( pbuf ) Mem_Free( pbuf );
}

void CL_InitEdict( edict_t *pEdict )
{
	Com_Assert( pEdict == NULL );

	pEdict->v.pContainingEntity = pEdict; // make cross-links for consistency
	if( !pEdict->pvClientData )
		pEdict->pvClientData = (cl_priv_t *)Mem_Alloc( cls.mempool, sizeof( cl_priv_t ));
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
	return com.malloc( clgame.private, cb, filename, fileline );
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
shader_t pfnLoadShader( const char *szShaderName, int fShaderNoMip )
{
	if( !re ) return 0; // render not initialized
	if( !szShaderName || !*szShaderName )
	{
		MsgDev( D_ERROR, "CL_LoadShader: invalid shadername (%s)\n", fShaderNoMip ? "nomip" : "generic" );
		return -1;
	}

	if( fShaderNoMip )
		return re->RegisterShader( szShaderName, SHADER_NOMIP );
	return re->RegisterShader( szShaderName, SHADER_GENERIC );
}

/*
=============
pfnFillRGBA

=============
*/
void pfnFillRGBA( int x, int y, int width, int height, byte r, byte g, byte b, byte alpha )
{
	rgba_t	color;

	if( !re ) return;

	MakeRGBA( color, r, g, b, alpha );
	re->SetColor( color );
	re->DrawFill( x, y, width, height );
	re->SetColor( NULL );
}

/*
=============
pfnDrawImageExt

=============
*/
void pfnDrawImageExt( HSPRITE shader, float x, float y, float w, float h, float s1, float t1, float s2, float t2 )
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
void pfnSetColor( byte r, byte g, byte b, byte a )
{
	rgba_t	color;

	if( !re ) return; // render not initialized
	MakeRGBA( color, r, g, b, a );
	re->SetColor( color );
}

/*
=============
pfnCvarSetString

=============
*/
void pfnCvarSetString( const char *szName, const char *szValue )
{
	Cvar_Set( szName, szValue );
}

/*
=============
pfnCvarSetValue

=============
*/
void pfnCvarSetValue( const char *szName, float flValue )
{
	Cvar_SetValue( szName, flValue );
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

	// second call can change msgFunc	
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
pfnSetKeyDest

=============
*/
void pfnSetKeyDest( int key_dest )
{
	switch( key_dest )
	{
	case key_game:
		cls.key_dest = key_game;
		break;
	case key_hudmenu:
		cls.key_dest = key_hudmenu;
		break;
	case key_menu:
		cls.key_dest = key_menu;
		break;
	case key_message:
		cls.key_dest = key_message;
		break;
	default:
		MsgDev( D_ERROR, "CL_SetKeyDest: wrong destination %i!\n", key_dest );
		break;
	}
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
	rgba_t	color;

	if( !cl.centerPrintTime ) return;
	CL_FadeAlpha( cl.centerPrintTime, scr_centertime->value, color );

	if( *( int *)color == 0xFFFFFFFF ) 
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
	cl.centerPrintTime = cls.realtime * 0.001f;
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
CL_GetLerpFrac

=============
*/
float CL_GetLerpFrac( void )
{
	return cl.lerpFrac;
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
pfnGetModelPtr

returns pointer to a studiomodel
=============
*/
static void *pfnGetModelPtr( edict_t* pEdict )
{
	cmodel_t	*mod;

	mod = cl.models[pEdict->v.modelindex];

	if( !mod ) return NULL;
	return mod->extradata;
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
	Cbuf_AddText( "wait 1\nlevelshot\n" );
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
=================
pfnTraceToss

=================
*/
static void pfnTraceToss( edict_t* pent, edict_t* pentToIgnore, TraceResult *ptr )
{
	trace_t		trace;

	if( pent == EDICT_NUM( 0 )) return;
	trace = CL_TraceToss( pent, pentToIgnore );
	CL_CopyTraceResult( ptr, trace );
}

/*
=================
pfnTraceHull

=================
*/
static void pfnTraceHull( const float *v1, const float *mins, const float *maxs, const float *v2, int fNoMonsters, edict_t *pentToSkip, TraceResult *ptr )
{
	trace_t	trace;
	int	move;

	move = (fNoMonsters) ? MOVE_NOMONSTERS : MOVE_NORMAL;

	if( IS_NAN(v1[0]) || IS_NAN(v1[1]) || IS_NAN(v1[2]) || IS_NAN(v2[0]) || IS_NAN(v1[2]) || IS_NAN(v2[2] ))
		Host_Error( "CL_TraceHull: NAN errors detected ('%f %f %f', '%f %f %f'\n", v1[0], v1[1], v1[2], v2[0], v2[1], v2[2] );

	trace = CL_Trace( v1, mins, mins, v2, move, pentToSkip, CL_ContentsMask( pentToSkip ));
	CL_CopyTraceResult( ptr, trace );
}

static void pfnTraceModel( const float *v1, const float *v2, edict_t *pent, TraceResult *ptr )
{
	// FIXME: implement
}

static const char *pfnTraceTexture( edict_t *pTextureEntity, const float *v1, const float *v2 )
{
	trace_t	trace;

	if( IS_NAN(v1[0]) || IS_NAN(v1[1]) || IS_NAN(v1[2]) || IS_NAN(v2[0]) || IS_NAN(v1[2]) || IS_NAN(v2[2] ))
		Host_Error( "CL_TraceTexture: NAN errors detected ('%f %f %f', '%f %f %f'\n", v1[0], v1[1], v1[2], v2[0], v2[1], v2[2] );

	trace = CL_Trace( v1, vec3_origin, vec3_origin, v2, MOVE_NOMONSTERS, NULL, CL_ContentsMask( pTextureEntity ));

	return trace.pTexName;
}

/*
=============
pfnPrecacheEvent

=============
*/
static word pfnPrecacheEvent( int type, const char* psz )
{
	// FIXME: implement
	return 0;
}

/*
=============
pfnHookEvent

=============
*/
static void pfnHookEvent( const char *name, pfnEventHook pfn )
{
	// FIXME: implement
}

/*
=============
pfnPlaybackEvent

=============
*/
static void pfnPlaybackEvent( int flags, const edict_t *pInvoker, word eventindex, float delay, event_args_t *args )
{
	// FIXME: implement
}

/*
=============
pfnKillEvent

=============
*/
static void pfnKillEvent( word eventindex )
{
	// FIXME: implement
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

/*
=============
CL_LoadLibrary

=============
*/
static void *CL_LoadLibrary( const char *name )
{
	string	libpath;

	Com_BuildPath( name, libpath );
	return Com_LoadLibrary( libpath );
}

/*
=============
CL_GetProcAddress

=============
*/
static void *CL_GetProcAddress( void *hInstance, const char *name )
{
	if( !hInstance ) return NULL;
	return Com_GetProcAddress( hInstance, name );
}

/*
=============
CL_FreeLibrary

=============
*/
static void CL_FreeLibrary( void *hInstance )
{
	Com_FreeLibrary( hInstance );
}
	
/*
=================
pfnFindExplosionPlane

=================
*/
static void pfnFindExplosionPlane( const float *origin, float radius, float *result )
{
	static vec3_t	planes[6] = {{0, 0, 1}, {0, 1, 0}, {1, 0, 0}, {0, 0, -1}, {0, -1, 0}, {-1, 0, 0}};
	float		best = 1.0f;
	vec3_t		point, dir;
	trace_t		trace;
	int		i;

	if( !result ) return;
	VectorClear( dir );

	for( i = 0; i < 6; i++ )
	{
		VectorMA( origin, radius, planes[i], point );

		trace = CL_Trace( origin, vec3_origin, vec3_origin, point, MOVE_WORLDONLY, NULL, MASK_SOLID );
		if( trace.allsolid || trace.fraction == 1.0 )
			continue;

		if( trace.fraction < best )
		{
			best = trace.fraction;
			VectorCopy( trace.plane.normal, dir );
		}
	}
	VectorCopy( dir, result );
}

/*
=================
pfnDecalIndexFromName

=================
*/
static int pfnDecalIndexFromName( const char *szDecalName )
{
	int	i;

	// look through the loaded sprite name list for SpriteName
	for( i = 0; i < MAX_DECALS && cl.configstrings[CS_DECALS+i+1][0]; i++ )
	{
		if( !strcmp( szDecalName, cl.configstrings[CS_DECALS+i+1] ))
			return cl.decal_shaders[i+1];
	}
	return 0; // invalid sprite
}

/*
=================
pfnDecalIndex

=================
*/
static int pfnDecalIndex( int id )
{
	id = bound( 0, id, MAX_DECALS - 1 );
	return cl.decal_shaders[id];
}

/*
=================
TriApi implementation

=================
*/
void TriRenderMode( kRenderMode_t mode )
{
}

void TriBind( shader_t shader )
{
}

void TriBegin( TRI_DRAW mode )
{
}

void TriEnd( void )
{
}

void TriEnable( int cap )
{
}

void TriDisable( int cap )
{
}

void TriVertex2f( float x, float y )
{
}

void TriVertex3f( float x, float y, float z )
{
}

void TriVertex2fv( const float *v )
{
}

void TriVertex3fv( const float *v )
{
}

void TriColor3f( float r, float g, float b )
{
}

void TriColor4f( float r, float g, float b, float a )
{
}

void TriColor4ub( byte r, byte g, byte b, byte a )
{
}

void TriTexCoord2f( float u, float v )
{
}

void TriTexCoord2fv( const float *v )
{
}

void TriCullFace( TRI_CULL mode )
{
}

void TriScreenToWorld( float *screen, float *world )
{
} 

int TriWorldToScreen( float *world, float *screen )
{
	return 0;
}

void TriFog( float flFogColor[3], float flStart, float flEnd, int bOn )
{
}

static triapi_t gTriApi =
{
	sizeof( triapi_t ),	
	TriRenderMode,
	TriBind,
	TriBegin,
	TriEnd,
	TriEnable,
	TriDisable,
	TriVertex2f,
	TriVertex3f,
	TriVertex2fv,
	TriVertex3fv,
	TriColor3f,
	TriColor4f,
	TriColor4ub,
	TriTexCoord2f,
	TriTexCoord2fv,
	TriCullFace,
	TriScreenToWorld,
	TriWorldToScreen,
	TriFog
};

static efxapi_t gEfxApi =
{
	sizeof( efxapi_t ),	
	pfnAddParticle,
	pfnAddDecal,
	pfnAddDLight,
	pfnFindExplosionPlane,
	pfnDecalIndexFromName,
	pfnDecalIndex,
};
					
// engine callbacks
static cl_enginefuncs_t gEngfuncs = 
{
	sizeof( cl_enginefuncs_t ),
	pfnMemAlloc,
	pfnMemCopy,
	pfnMemFree,
	pfnLoadShader,
	pfnFillRGBA,
	pfnDrawImageExt,
	pfnSetColor,
	pfnCVarRegister,
	pfnCvarSetString,
	pfnCvarSetValue,
	pfnGetCvarFloat,
	pfnGetCvarString,
	pfnAddCommand,
	pfnHookUserMsg,
	pfnServerCmd,
	pfnClientCmd,
	pfnSetKeyDest,
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
	pfnDrawString,		
	pfnGetDrawParms,
	pfnSetDrawParms,
	pfnGetViewAngles,
	CL_GetEdictByIndex,
	CL_GetLocalPlayer,
	pfnIsSpectateOnly,
	pfnGetClientTime,
	CL_GetLerpFrac,
	pfnGetMaxClients,
	pfnGetViewModel,
	pfnGetModelPtr,
	pfnMakeLevelShot,						
	pfnPointContents,
	pfnTraceLine,
	pfnTraceToss,
	pfnTraceHull,
	pfnTraceModel,
	pfnTraceTexture,
	pfnPrecacheEvent,
	pfnHookEvent,
	pfnPlaybackEvent,
	pfnKillEvent,
	CL_AllocString,
	CL_GetString,	
	pfnRandomLong,
	pfnRandomFloat,
	pfnLoadFile,
	pfnFileExists,
	pfnGetGameDir,				
	CL_LoadLibrary,
	CL_GetProcAddress,
	CL_FreeLibrary,		
	Host_Error,
	&gTriApi,
	&gEfxApi
};

void CL_UnloadProgs( void )
{
	// initialize game
	clgame.dllFuncs.pfnShutdown();

	StringTable_Delete( clgame.hStringTable );
	Com_FreeLibrary( clgame.hInstance );
	Mem_FreePool( &cls.mempool );
	Mem_FreePool( &clgame.private );
}

bool CL_LoadProgs( const char *name )
{
	static CLIENTAPI		GetClientAPI;
	string			libpath;
	edict_t			*e;
	int			i;

	if( clgame.hInstance ) CL_UnloadProgs();

	// fill it in
	Com_BuildPath( name, libpath );
	cls.mempool = Mem_AllocPool( "Client Edicts Zone" );
	clgame.private = Mem_AllocPool( "Client Private Zone" );

	clgame.hInstance = Com_LoadLibrary( libpath );
	if( !clgame.hInstance ) return false;

	GetClientAPI = (CLIENTAPI)Com_GetProcAddress( clgame.hInstance, "CreateAPI" );

	if( !GetClientAPI )
	{
          	MsgDev( D_ERROR, "CL_LoadProgs: failed to get address of CreateAPI proc\n" );
		return false;
	}

	if( !GetClientAPI( &clgame.dllFuncs, &gEngfuncs ))
	{
		MsgDev( D_ERROR, "CL_LoadProgs: can't init client API\n" );
		return false;
	}

	// 65535 unique strings should be enough ...
	clgame.hStringTable = StringTable_Create( "Client", 0x10000 );
	clgame.maxEntities = host.max_edicts;	// FIXME: must come from CS_MAXENTITIES
	clgame.maxClients = Host_MaxClients();
	clgame.edicts = Mem_Alloc( cls.mempool, sizeof( edict_t ) * clgame.maxEntities );

	// register svc_bad message
	pfnHookUserMsg( "bad", NULL );
	CL_LinkUserMessage( "bad@0", svc_bad );

	clgame.movevars.gravity = com.atof( DEFAULT_GRAVITY );
	clgame.movevars.maxvelocity = com.atof( DEFAULT_MAXVELOCITY );
	clgame.movevars.rollangle = com.atof( DEFAULT_ROLLANGLE );
	clgame.movevars.rollspeed = com.atof( DEFAULT_ROLLSPEED );
	clgame.movevars.maxspeed = com.atof( DEFAULT_MAXSPEED );
	clgame.movevars.stepheight = com.atof( DEFAULT_STEPHEIGHT );
	clgame.movevars.accelerate = com.atof( DEFAULT_ACCEL );
	clgame.movevars.airaccelerate = com.atof( DEFAULT_AIRACCEL );
	clgame.movevars.friction = com.atof( DEFAULT_FRICTION );

	for( i = 0, e = EDICT_NUM( 0 ); i < clgame.maxEntities; i++, e++ )
		e->free = true; // mark all edicts as freed

	// initialize game
	clgame.dllFuncs.pfnInit();

	return true;
}