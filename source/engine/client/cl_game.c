//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        cl_game.c - client dlls interaction
//=======================================================================

#include "common.h"
#include "client.h"
#include "byteorder.h"
#include "matrix_lib.h"
#include "const.h"
#include "com_library.h"
#include "triangle_api.h"
#include "effects_api.h"
#include "pm_defs.h"

/*
====================
CL_GetClientEntity

Render callback for studio models
====================
*/
edict_t *CL_GetEdictByIndex( int index )
{
	if( index == MAX_EDICTS - 1 )
		return &clgame.playermodel;

	if( index < 0 || index > clgame.globals->numEntities )
	{
		if( index == VIEWENT_INDEX ) return &clgame.viewent;
		if( index == NULLENT_INDEX ) return NULL;
		MsgDev( D_ERROR, "CL_GetEntityByIndex: invalid entindex %i\n", index );
		return NULL;
	}

	if( EDICT_NUM( index )->free )
		return NULL;
	return EDICT_NUM( index );
}

/*
====================
CL_GetServerTime

don't clamped time that come from server
====================
*/
int CL_GetServerTime( void )
{
	return cl.frame.servertime;
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

bool CL_GetAttachment( int entityIndex, int number, vec3_t origin, vec3_t angles )
{
	edict_t	*ed = CL_GetEdictByIndex( entityIndex );

	if( !ed || ed->free || !ed->pvClientData )
		return false;

	number = bound( 1, number, MAXSTUDIOATTACHMENTS );

	if( origin ) VectorAdd( ed->v.origin, ed->pvClientData->origin[number-1], origin );	
	if( angles ) VectorCopy( ed->pvClientData->angles[number-1], angles );

	return true;
}

bool CL_SetAttachment( int entityIndex, int number, vec3_t origin, vec3_t angles )
{
	edict_t	*ed = CL_GetEdictByIndex( entityIndex );

	if( !ed || ed->free || !ed->pvClientData )
		return false;

	if( number > MAXSTUDIOATTACHMENTS )
		number = MAXSTUDIOATTACHMENTS;

	if( origin ) VectorCopy( origin, ed->pvClientData->origin[number-1] );	
	if( angles ) VectorCopy( angles, ed->pvClientData->angles[number-1] );

	return true;
}

byte CL_GetMouthOpen( int entityIndex )
{
	edict_t	*ed;

	if( entityIndex <= 0 || entityIndex >= clgame.globals->numEntities )
		return 0;

	ed = CL_GetEdictByIndex( entityIndex );

	if( !ed || ed->free || !ed->pvClientData )
		return 0;
	return ed->pvClientData->mouth.open;
}

prevframe_t *CL_GetPrevFrame( int entityIndex )
{
	edict_t	*pEnt = CL_GetEdictByIndex( entityIndex );

	if( !pEnt || !pEnt->pvClientData )
		return NULL;

	return &pEnt->pvClientData->latched;
}

/*
================
CL_FadeAlpha
================
*/
void CL_FadeAlpha( int starttime, int endtime, rgba_t color )
{
	int	time, fade_time;

	if( starttime == 0 )
	{
		MakeRGBA( color, 255, 255, 255, 255 );
		return;
	}

	time = cls.realtime - starttime;

	if( time >= endtime )
	{
		MakeRGBA( color, 255, 255, 255, 0 );
		return;
	}

	// fade time is 1/4 of endtime
	fade_time = endtime / 4;
	fade_time = bound( 300, fade_time, 10000 );

	color[0] = color[1] = color[2] = 255;

	// fade out
	if(( endtime - time ) < fade_time )
		color[3] = bound( 0, ((endtime - time) * 1.0f / fade_time) * 255, 255 );
	else color[3] = 255;
}

/*
=============
CL_DrawCenterPrint

called each frame
=============
*/
void CL_DrawCenterPrint( void )
{
	char	*start;
	int	l, x, y, w;
	rgba_t	color;

	if( !re || !clgame.ds.centerPrintTime ) return;

	CL_FadeAlpha( clgame.ds.centerPrintTime, scr_centertime->value * 1000, color );

	if( *(int *)color == 0x00FFFFFF ) 
	{
		// faded out
		clgame.ds.centerPrintTime = 0;
		return;
	}

	re->SetColor( color );
	start = clgame.ds.centerPrint;
	y = clgame.ds.centerPrintY - clgame.ds.centerPrintLines * BIGCHAR_HEIGHT / 2;

	while( 1 )
	{
		char	linebuffer[1024];

		for( l = 0; l < 50; l++ )
		{
			if( !start[l] || start[l] == '\n' )
				break;
			linebuffer[l] = start[l];
		}
		linebuffer[l] = 0;

		w = clgame.ds.centerPrintCharWidth * com.cstrlen( linebuffer );
		x = (SCREEN_WIDTH - w)>>1;

		SCR_DrawStringExt( x, y, clgame.ds.centerPrintCharWidth, SMALLCHAR_HEIGHT, linebuffer, color, false );

		y += clgame.ds.centerPrintCharWidth * 1.5;
		while( *start && ( *start != '\n' )) start++;
		if( !*start ) break;
		start++;
	}
	re->SetColor( NULL );
}

/*
=============
CL_CenterPrint

print centerscreen message
=============
*/
void CL_CenterPrint( const char *text, int y, int charWidth )
{
	char	*s;

	com.strncpy( clgame.ds.centerPrint, text, sizeof( clgame.ds.centerPrint ));
	clgame.ds.centerPrintTime = cls.realtime;
	clgame.ds.centerPrintCharWidth = charWidth;
	clgame.ds.centerPrintY = y;

	// count the number of lines for centering
	clgame.ds.centerPrintLines = 1;
	s = clgame.ds.centerPrint;

	while( *s )
	{
		if( *s == '\n' )
			clgame.ds.centerPrintLines++;
		s++;
	}
}

/*
====================
SPR_AdjustSize

draw hudsprite routine
====================
*/
static void SPR_AdjustSize( float *x, float *y, float *w, float *h )
{
	float	xscale, yscale;

	if( !x && !y && !w && !h ) return;

	// scale for screen sizes
	xscale = clgame.scrInfo.iRealWidth / (float)clgame.scrInfo.iWidth;
	yscale = clgame.scrInfo.iRealHeight / (float)clgame.scrInfo.iHeight;

	if( x ) *x *= xscale;
	if( y ) *y *= yscale;
	if( w ) *w *= xscale;
	if( h ) *h *= yscale;
}

static bool SPR_Scissor( float *x, float *y, float *width, float *height, float *u0, float *v0, float *u1, float *v1 )
{
	float	dudx, dvdy;

	// clip sub rect to sprite
	if(( width == 0 ) || ( height == 0 ))
		return false;

	if( *x + *width <= clgame.ds.scissor_x )
		return false;
	if( *x >= clgame.ds.scissor_x + clgame.ds.scissor_width )
		return false;
	if( *y + *height <= clgame.ds.scissor_y )
		return false;
	if( *y >= clgame.ds.scissor_y + clgame.ds.scissor_height )
		return false;

	dudx = (*u1 - *u0) / *width;
	dvdy = (*v1 - *v0) / *height;

	if( *x < clgame.ds.scissor_x )
	{
		*u0 += (clgame.ds.scissor_x - *x) * dudx;
		*width -= clgame.ds.scissor_x - *x;
		*x = clgame.ds.scissor_x;
	}

	if( *x + *width > clgame.ds.scissor_x + clgame.ds.scissor_width )
	{
		*u1 -= (*x + *width - (clgame.ds.scissor_x + clgame.ds.scissor_width)) * dudx;
		*width = clgame.ds.scissor_x + clgame.ds.scissor_width - *x;
	}

	if( *y < clgame.ds.scissor_y )
	{
		*v0 += (clgame.ds.scissor_y - *y) * dvdy;
		*height -= clgame.ds.scissor_y - *y;
		*y = clgame.ds.scissor_y;
	}

	if( *y + *height > clgame.ds.scissor_y + clgame.ds.scissor_height )
	{
		*v1 -= (*y + *height - (clgame.ds.scissor_y + clgame.ds.scissor_height)) * dvdy;
		*height = clgame.ds.scissor_y + clgame.ds.scissor_height - *y;
	}
	return true;
}

/*
====================
SPR_DrawGeneric

draw hudsprite routine
====================
*/
static void SPR_DrawGeneric( int frame, float x, float y, float width, float height, const wrect_t *prc )
{
	float	s1, s2, t1, t2;

	if( !re ) return;

	if( width == -1 && height == -1 )
	{
		int	w, h;

		// undoc feature: get drawsizes from image
		re->GetParms( &w, &h, NULL, frame, clgame.ds.hSprite );

		width = w;
		height = h;
	}

	if( prc )
	{
		// calc user-defined rectangle
		s1 = (float)prc->left / width;
		t1 = (float)prc->top / height;
		s2 = (float)prc->right / width;
		t2 = (float)prc->bottom / height;
		width = prc->right - prc->left;
		height = prc->bottom - prc->top;
	}
	else
	{
		s1 = t1 = 0.0f;
		s2 = t2 = 1.0f;
	}

	// pass scissor test if supposed
	if( clgame.ds.scissor_test && !SPR_Scissor( &x, &y, &width, &height, &s1, &t1, &s2, &t2 ))
		return;

	// scale for screen sizes
	SPR_AdjustSize( &x, &y, &width, &height );
	re->DrawStretchPic( x, y, width, height, s1, t1, s2, t2, clgame.ds.hSprite );
	re->SetColor( NULL );
}

/*
====================
CL_InitTitles

parse all messages that declared in titles.txt
and hold them into permament memory pool 
====================
*/
static void CL_InitTitles( const char *filename )
{
	token_t		token;
	client_textmessage_t state;
	char		*pName = NULL;
	char		*pMessage = NULL;
	script_t		*script;

	Mem_Set( &state, 0, sizeof( state ));	
	Mem_Set( &clgame.titles, 0, sizeof( clgame.titles ));
	clgame.numTitles = 0;

	script = Com_OpenScript( filename, NULL, 0 );
	if( !script ) return;

	while( script )
	{
		if( !Com_ReadToken( script, SC_ALLOW_NEWLINES, &token ))
			break;

		if( !com.stricmp( token.string, "$" ))	// skip dollar
			Com_ReadToken( script, false, &token );

		if( !com.stricmp( token.string, "position" ))
		{
			Com_ReadFloat( script, false, &state.x );
			Com_ReadFloat( script, false, &state.y );
		}
		else if( !com.stricmp( token.string, "effect" ))
		{
			Com_ReadUlong( script, false, &state.effect );
		}
		else if( !com.stricmp( token.string, "fadein" ))
		{
			Com_ReadFloat( script, false, &state.fadein );
		}
		else if( !com.stricmp( token.string, "fadeout" ))
		{
			Com_ReadFloat( script, false, &state.fadeout );
		}
		else if( !com.stricmp( token.string, "fxtime" ))
		{
			Com_ReadFloat( script, false, &state.fxtime );
		}
		else if( !com.stricmp( token.string, "holdtime" ))
		{
			Com_ReadFloat( script, false, &state.holdtime );
		}
		else if( !com.strnicmp( token.string, "color2", 6 ))
		{
			uint	temp;

			Com_ReadUlong( script, false, &temp );
			state.r2 = temp;
			Com_ReadUlong( script, false, &temp );
			state.g2 = temp;
			Com_ReadUlong( script, false, &temp );
			state.b2 = temp;

			if( Com_ReadUlong( script, false, &temp ))
				state.a2 = temp; // optional, nevers used in Half-Life
			else state.a2 = 255;
		}
		else if( !com.stricmp( token.string, "color" ))
		{
			uint	temp;

			Com_ReadUlong( script, false, &temp );
			state.r1 = temp;
			Com_ReadUlong( script, false, &temp );
			state.g1 = temp;
			Com_ReadUlong( script, false, &temp );
			state.b1 = temp;

			if( Com_ReadUlong( script, false, &temp ))
				state.a1 = temp; // optional, nevers used in Half-Life
			else state.a1 = 255;
		}
		else if( !com.stricmp( token.string, "{" ))
		{
			client_textmessage_t *newmsg;
			const char	*buffer, *end;
			size_t		size;
                              
                              Com_SaveToken( script, &token );
			
			// parse the message
			buffer = script->text;
			Com_SkipBracedSection( script, 0 );
			end = script->text - 1; // skip '}'

			if( !buffer ) buffer = script->buffer; // missing body ?
			if( !end ) end = script->buffer + script->size;	// EOF ?
			size = end - buffer;

			pMessage = Mem_Alloc( cls.mempool, size + 1 );
			Mem_Copy( pMessage, buffer, size );
			pMessage[size] = 0; // terminator

			// create new client textmessage
			newmsg = &clgame.titles[clgame.numTitles];
			Mem_Copy( newmsg, &state, sizeof( *newmsg ));

			newmsg->pName = pName;
			newmsg->pMessage = pMessage;
			clgame.numTitles++;	// registered
		}
		else
		{
			// begin message declaration
			pName = com.stralloc( cls.mempool, token.string, __FILE__, __LINE__ );
		}
	}
	Com_CloseScript( script );
}

/*
====================
CL_GetLocalPlayer

Render callback for studio models
====================
*/
edict_t *CL_GetLocalPlayer( void )
{
	if( cls.state == ca_active )
	{
		edict_t	*player = EDICT_NUM( cl.playernum + 1 );
		if( CL_IsValidEdict( player )) return player;
		Host_Error( "CL_GetLocalPlayer: invalid edict\n" );
	}
	return NULL;
}

/*
====================
CL_GetMaxlients

Render callback for studio models
====================
*/
int CL_GetMaxClients( void )
{
	return clgame.globals->maxClients;
}

void CL_DrawCrosshair( void )
{
	int	x, y, width, height;
	edict_t	*pPlayer;

	if( !re || clgame.ds.hCrosshair <= 0 || cl.refdef.crosshairangle[2] || !cl_crosshair->integer )
		return;

	pPlayer = CL_GetLocalPlayer();

	if( pPlayer->v.health <= 0.0f || pPlayer->v.flags & FL_FROZEN )
		return;

	// camera on
	if( pPlayer->serialnumber != cl.refdef.viewentity )
		return;

	// get crosshair dimension
	width = clgame.ds.rcCrosshair.right - clgame.ds.rcCrosshair.left;
	height = clgame.ds.rcCrosshair.bottom - clgame.ds.rcCrosshair.top;

	// g-cont - cl.refdef.crosshairangle is the autoaim angle.
	// if we're not using autoaim, just draw in the middle of the 
	// screen
	if( !VectorIsNull( cl.refdef.crosshairangle ))
	{
		vec3_t	angles;
		vec3_t	forward;
		vec3_t	point, screen;

		// FIXME: this code is wrong
		VectorAdd( cl.refdef.cl_viewangles, cl.refdef.crosshairangle, angles );
		AngleVectors( angles, forward, NULL, NULL );
		VectorAdd( cl.refdef.vieworg, forward, point );
		re->WorldToScreen( point, screen );

		if( clgame.scrInfo.iFlags & SCRINFO_VIRTUALSPACE )
		{
			float	xscale, yscale;
		
			// NOTE: WorldToScreen returns real coordinates, we need to divide it
			// into virtual screenspace
			xscale = clgame.scrInfo.iRealWidth / (float)clgame.scrInfo.iWidth;
			yscale = clgame.scrInfo.iRealHeight / (float)clgame.scrInfo.iHeight;
			x = screen[0] / xscale;
			y = screen[1] / yscale;
		}
		else
		{
			x = screen[0];
			y = screen[1];
		}
	}
	else
	{
		x = clgame.scrInfo.iWidth / 2; 
		y = clgame.scrInfo.iHeight / 2;
	}

	clgame.ds.hSprite = clgame.ds.hCrosshair;
	re->SetColor( clgame.ds.rgbaCrosshair );
	re->SetParms( clgame.ds.hSprite, kRenderTransAlpha, 0 );
	SPR_DrawGeneric( 0, x - 0.5f * width, y - 0.5f * height, -1, -1, &clgame.ds.rcCrosshair );
}

void CL_DrawHUD( int state )
{
	if( state == CL_ACTIVE && !cl.video_prepped )
		state = CL_LOADING;

	if( state == CL_ACTIVE && cl.refdef.paused )
		state = CL_PAUSED;

	clgame.dllFuncs.pfnRedraw( cl.time * 0.001f, state );

	if( state == CL_ACTIVE || state == CL_PAUSED )
	{
		CL_DrawCrosshair ();
		CL_DrawCenterPrint ();
	}

	if( state == CL_ACTIVE )
		clgame.dllFuncs.pfnFrame( cl.time * 0.001f );
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

/*
=======================
CL_MsgNumbers
=======================
*/
static int CL_MsgNumbers( const void *a, const void *b )
{
	user_message_t	*msga, *msgb;

	msga = (user_message_t *)a;
	msgb = (user_message_t *)b;

	if( msga == NULL )
	{
		if ( msgb == NULL ) return 0;
		else return -1;
	}
	else if ( msgb == NULL ) return 1;

	if( msga->number < msgb->number )
		return -1;
	return 1;
}

void CL_SortUserMessages( void )
{
//	qsort( clgame.msg, clgame.numMessages, sizeof( user_message_t ), CL_MsgNumbers );
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

static void CL_RegisterEvent( int lastnum, const char *szEvName, pfnEventHook func )
{
	user_event_t	*ev;

	if( lastnum == MAX_EVENTS )
	{
		MsgDev( D_ERROR, "CL_RegisterEvent: MAX_EVENTS hit!!!\n" );
		return;
	}

	ev = clgame.events[lastnum];

	// clear existing or allocate new one
	if( ev ) Mem_Set( ev, 0, sizeof( *ev ));
	else ev = clgame.events[lastnum] = Mem_Alloc( cls.mempool, sizeof( *ev ));

	com.strncpy( ev->name, szEvName, CS_SIZE );
	ev->func = func;
	// ev->index will be set later
}

void CL_SetEventIndex( const char *szEvName, int ev_index )
{
	user_event_t	*ev;
	int		i;

	if( !szEvName || !*szEvName ) return; // ignore blank names

	// search event by name to link with
	for( i = 0; i < MAX_EVENTS; i++ )
	{
		ev = clgame.events[i];
		if( !ev ) break;

		if( !com.strcmp( ev->name, szEvName ))
		{
			ev->index = ev_index;
			return;
		}
	}
}

void CL_InitEdict( edict_t *pEdict )
{
	Com_Assert( pEdict == NULL );
	Com_Assert( pEdict->pvPrivateData != NULL );
	Com_Assert( pEdict->pvClientData != NULL );

	pEdict->v.pContainingEntity = pEdict; // make cross-links for consistency
	pEdict->pvClientData = (cl_priv_t *)Mem_Alloc( cls.mempool, sizeof( cl_priv_t ));
	pEdict->pvPrivateData = NULL;
	pEdict->serialnumber = NUM_FOR_EDICT( pEdict );	// merged on first update
	pEdict->free = false;
}

void CL_FreeEdict( edict_t *pEdict )
{
	Com_Assert( pEdict == NULL );
	Com_Assert( pEdict->free );

	// unlink from world
	CL_UnlinkEdict( pEdict );

	if( pEdict->pvClientData ) Mem_Free( pEdict->pvClientData );
	if( pEdict->pvPrivateData ) Mem_Free( pEdict->pvPrivateData );
	Mem_Set( pEdict, 0, sizeof( *pEdict ));

	// mark edict as freed
	pEdict->freetime = cl.time * 0.001f;
	pEdict->v.nextthink = -1;
	pEdict->free = true;
}

edict_t *CL_AllocEdict( void )
{
	edict_t	*pEdict;
	int	i;

	for( i = clgame.globals->maxClients + 1; i < clgame.globals->numEntities; i++ )
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

	if( i == clgame.globals->maxEntities )
		Host_Error( "CL_AllocEdict: no free edicts\n" );

	clgame.globals->numEntities++;
	pEdict = EDICT_NUM( i );
	CL_InitEdict( pEdict );

	return pEdict;
}

void CL_InitWorld( void )
{
	edict_t	*ent;
	int	i;

	ent = EDICT_NUM( 0 );
	if( ent->free ) CL_InitEdict( ent );
	ent->v.classname = MAKE_STRING( "worldspawn" );
	ent->v.model = MAKE_STRING( cl.configstrings[CS_MODELS+1] );
	ent->v.modelindex = 1;	// world model
	ent->v.solid = SOLID_BSP;
	ent->v.movetype = MOVETYPE_PUSH;
	clgame.globals->numEntities = 1;

	clgame.globals->mapname = MAKE_STRING( cl.configstrings[CS_NAME] );

	clgame.globals->coop = Cvar_VariableInteger( "coop" );
	clgame.globals->teamplay = Cvar_VariableInteger( "teamplay" );
	clgame.globals->deathmatch = Cvar_VariableInteger( "deathmatch" );
	clgame.globals->serverflags = com.atoi( cl.configstrings[CS_SERVERFLAGS] );

	for( i = 0; i < clgame.globals->maxClients; i++ )
	{
		// setup all clients
		ent = EDICT_NUM( i + 1 );
		CL_InitEdict( ent );
		clgame.globals->numClients++;
	}

	// clear viewmodel prevstate
	Mem_Set( &clgame.viewent.pvClientData->latched, 0, sizeof( prevframe_t ));
}

void CL_InitEdicts( void )
{
	edict_t	*e;
	int	i;

	Com_Assert( clgame.edicts != NULL );
	Com_Assert( clgame.baselines != NULL );

	clgame.edicts = Mem_Alloc( clgame.mempool, sizeof( edict_t ) * clgame.globals->maxEntities );
	clgame.baselines = Mem_Alloc( clgame.mempool, sizeof( entity_state_t ) * clgame.globals->maxEntities );
	for( i = 0, e = clgame.edicts; i < clgame.globals->maxEntities; i++, e++ )
		e->free = true; // mark all edicts as freed
}

void CL_FreeEdicts( void )
{
	int	i;
	edict_t	*ent;

	if( clgame.edicts )
	{
		for( i = 0; i < clgame.globals->maxEntities; i++ )
		{
			ent = EDICT_NUM( i );
			if( ent->free ) continue;
			CL_FreeEdict( ent );
		}
		Mem_Free( clgame.edicts );
	}

	if( clgame.baselines ) Mem_Free( clgame.baselines );

	// clear globals
	StringTable_Clear( clgame.hStringTable );
	clgame.globals->numEntities = 0;
	clgame.globals->numClients = 0;
	clgame.baselines = NULL;
	clgame.edicts = NULL;
}

bool CL_IsValidEdict( const edict_t *e )
{
	if( !e ) return false;
	if( e->free ) return false;
	if( e == EDICT_NUM( 0 )) return false;	// world is the read-only entity
	if( !e->pvServerData ) return false;
	// edict without pvPrivateData is valid edict
	// server.dll know how allocate it
	return true;
}

const char *CL_ClassName( const edict_t *e )
{
	if( !e ) return "(null)";
	if( e->free ) return "freed";
	return STRING( e->v.classname );
}

/*
===============================================================================
	CGame Builtin Functions

===============================================================================
*/
/*
=========
pfnSPR_Load

=========
*/
static HSPRITE pfnSPR_Load( const char *szPicName )
{
	if( !re ) return 0; // render not initialized
	if( !szPicName || !*szPicName )
	{
		MsgDev( D_ERROR, "CL_SpriteLoad: invalid spritename\n" );
		return -1;
	}

	return re->RegisterShader( szPicName, SHADER_NOMIP );	// replace with SHADER_GENERIC ?
}

/*
=========
pfnSPR_Load

=========
*/
static int pfnSPR_Frames( HSPRITE hPic )
{
	int	numFrames;

	if( !re ) return 1;
	re->GetParms( NULL, NULL, &numFrames, 0, hPic );

	return numFrames;
}

/*
=========
pfnSPR_Load

=========
*/
static int pfnSPR_Height( HSPRITE hPic, int frame )
{
	int	sprHeight;

	if( !re ) return 0;
	re->GetParms( NULL, &sprHeight, NULL, frame, hPic );

	return sprHeight;
}

/*
=========
pfnSPR_Load

=========
*/
static int pfnSPR_Width( HSPRITE hPic, int frame )
{
	int	sprWidth;

	if( !re ) return 0;
	re->GetParms( &sprWidth, NULL, NULL, frame, hPic );

	return sprWidth;
}

/*
=========
pfnSPR_Load

=========
*/
static void pfnSPR_Set( HSPRITE hPic, int r, int g, int b, int a )
{
	rgba_t	color;

	if( !re ) return; // render not initialized

	clgame.ds.hSprite = hPic;
	MakeRGBA( color, r, g, b, a );
	re->SetColor( color );
}

/*
=========
pfnSPR_Draw

=========
*/
static void pfnSPR_Draw( int frame, int x, int y, int width, int height, const wrect_t *prc )
{
	if( !re ) return; // render not initialized

	re->SetParms( clgame.ds.hSprite, kRenderNormal, frame );
	SPR_DrawGeneric( frame, x, y, width, height, prc );
}

/*
=========
pfnSPR_DrawTrans

=========
*/
static void pfnSPR_DrawTrans( int frame, int x, int y, int width, int height, const wrect_t *prc )
{
	if( !re ) return; // render not initialized

	re->SetParms( clgame.ds.hSprite, kRenderTransColor, frame );
	SPR_DrawGeneric( frame, x, y, width, height, prc );
}

/*
=========
pfnSPR_DrawHoles

=========
*/
static void pfnSPR_DrawHoles( int frame, int x, int y, int width, int height, const wrect_t *prc )
{
	if( !re ) return; // render not initialized

	re->SetParms( clgame.ds.hSprite, kRenderTransAlpha, frame );
	SPR_DrawGeneric( frame, x, y, width, height, prc );
}

/*
=========
pfnSPR_DrawAdditive

=========
*/
static void pfnSPR_DrawAdditive( int frame, int x, int y, int width, int height, const wrect_t *prc )
{
	if( !re ) return; // render not initialized

	re->SetParms( clgame.ds.hSprite, kRenderTransAdd, frame );
	SPR_DrawGeneric( frame, x, y, width, height, prc );
}

/*
=========
pfnSPR_EnableScissor

=========
*/
static void pfnSPR_EnableScissor( int x, int y, int width, int height )
{
	// check bounds
	x = bound( 0, x, clgame.scrInfo.iWidth );
	y = bound( 0, y, clgame.scrInfo.iHeight );
	width = bound( 0, width, clgame.scrInfo.iWidth - x );
	height = bound( 0, height, clgame.scrInfo.iHeight - y );

	clgame.ds.scissor_x = x;
	clgame.ds.scissor_width = width;
	clgame.ds.scissor_y = y;
	clgame.ds.scissor_height = height;
	clgame.ds.scissor_test = true;
}

/*
=========
pfnSPR_DisableScissor

=========
*/
static void pfnSPR_DisableScissor( void )
{
	clgame.ds.scissor_x = 0;
	clgame.ds.scissor_width = 0;
	clgame.ds.scissor_y = 0;
	clgame.ds.scissor_height = 0;
	clgame.ds.scissor_test = false;
}

/*
=========
pfnSPR_GetList

FIXME: implement original hl1 SPR_GetList
=========
*/
static client_sprite_t *pfnSPR_GetList( char *psz, int *piCount )
{
	return NULL;
}

/*
=============
pfnFillRGBA

=============
*/
static void pfnFillRGBA( int x, int y, int width, int height, int r, int g, int b, int a )
{
	rgba_t	color;

	if( !re ) return;

	MakeRGBA( color, r, g, b, a );
	re->SetColor( color );

	SPR_AdjustSize( (float *)&x, (float *)&y, (float *)&width, (float *)&height );
	re->DrawStretchPic( x, y, width, height, 0, 0, 1, 1, cls.fillShader );
	re->SetColor( NULL );
}

/*
=============
pfnGetScreenInfo

get actual screen info
=============
*/
static int pfnGetScreenInfo( SCREENINFO *pscrinfo )
{
	int	i;

	// setup screen info
	clgame.scrInfo.iRealWidth = scr_width->integer;
	clgame.scrInfo.iRealHeight = scr_height->integer;

	if( pscrinfo && pscrinfo->iFlags & SCRINFO_VIRTUALSPACE )
	{
		// virtual screen space 640x480
		// see cl_screen.c from Quake3 code for more details
		clgame.scrInfo.iWidth = SCREEN_WIDTH;
		clgame.scrInfo.iHeight = SCREEN_HEIGHT;
		clgame.scrInfo.iFlags |= SCRINFO_VIRTUALSPACE;
	}
	else
	{
		clgame.scrInfo.iWidth = scr_width->integer;
		clgame.scrInfo.iHeight = scr_height->integer;
		clgame.scrInfo.iFlags &= ~SCRINFO_VIRTUALSPACE;
	}

	// TODO: build real table of fonts widthInChars
	// TODO: load half-life credits font from wad
	for( i = 0; i < 256; i++ )
		clgame.scrInfo.charWidths[i] = SMALLCHAR_WIDTH;
	clgame.scrInfo.iCharHeight = SMALLCHAR_HEIGHT;
	clgame.ds.hHudFont = cls.clientFont;

	if( !pscrinfo ) return 0;
	*pscrinfo = clgame.scrInfo;	// copy screeninfo out

	return 1;
}

/*
=============
pfnSetCrosshair

setup auto-aim crosshair
=============
*/
static void pfnSetCrosshair( HSPRITE hspr, wrect_t rc, int r, int g, int b )
{
	clgame.ds.rgbaCrosshair[0] = (byte)r;
	clgame.ds.rgbaCrosshair[1] = (byte)g;
	clgame.ds.rgbaCrosshair[2] = (byte)b;
	clgame.ds.rgbaCrosshair[3] = (byte)0xFF;
	clgame.ds.hCrosshair = hspr;
	clgame.ds.rcCrosshair = rc;
}

/*
=============
pfnAddCommand

=============
*/
static void pfnAddCommand( const char *cmd_name, xcommand_t func, const char *cmd_desc )
{
	if( !cmd_name || !*cmd_name ) return;
	if( !cmd_desc ) cmd_desc = ""; // hidden for makehelep system

	// NOTE: if( func == NULL ) cmd will be forwarded to a server
	Cmd_AddCommand( cmd_name, func, cmd_desc );
}

/*
=============
pfnHookUserMsg

=============
*/
static void pfnHookUserMsg( const char *szMsgName, pfnUserMsgHook pfn )
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
static void pfnServerCmd( const char *szCmdString )
{
	// server command adding in cmds queue
	Cbuf_AddText( va( "cmd %s", szCmdString ));
}

/*
=============
pfnClientCmd

=============
*/
static void pfnClientCmd( const char *szCmdString )
{
	// client command executes immediately
	Cmd_ExecuteString( szCmdString );
}

/*
=============
pfnGetPlayerInfo

=============
*/
static void pfnGetPlayerInfo( int ent_num, hud_player_info_t *pinfo )
{
	player_info_t	*player;

	ent_num -= 1; // player list if offset by 1 from ents

	if( ent_num >= clgame.globals->maxClients || ent_num < 0 || !cl.players[ent_num].name[0] )
	{
		Mem_Set( pinfo, 0, sizeof( *pinfo ));
		return;
	}

	player = &cl.players[ent_num];
	pinfo->thisplayer = ( ent_num == cl.playernum ) ? true : false;

	pinfo->name = player->name;
	pinfo->model = player->model;
		
	pinfo->ping = com.atoi( Info_ValueForKey( player->userinfo, "ping" ));
	pinfo->spectator = com.atoi( Info_ValueForKey( player->userinfo, "spectator" ));
	pinfo->packetloss = com.atoi( Info_ValueForKey( player->userinfo, "loss" ));
	pinfo->topcolor = com.atoi( Info_ValueForKey( player->userinfo, "topcolor" ));
	pinfo->bottomcolor = com.atoi( Info_ValueForKey( player->userinfo, "bottomcolor" ));
}

/*
=============
pfnPlaySoundByName

=============
*/
static void pfnPlaySoundByName( const char *szSound, float volume, int pitch, const float *org )
{
	S_StartLocalSound( szSound, volume, pitch, org );
}

/*
=============
pfnPlaySoundByIndex

=============
*/
static void pfnPlaySoundByIndex( int iSound, float volume, int pitch, const float *org )
{
	// make sure what we in-bounds
	iSound = bound( 0, iSound, MAX_SOUNDS );

	if( cl.sound_precache[iSound] == 0 )
	{
		MsgDev( D_ERROR, "CL_PlaySoundByIndex: invalid sound handle %i\n", iSound );
		return;
	}
	S_StartSound( org, cl.refdef.viewentity, CHAN_AUTO, cl.sound_precache[iSound], volume, ATTN_NORM, pitch, 0 );
}

/*
=============
pfnTextMessageGet

returns specified message from titles.txt
=============
*/
static client_textmessage_t *pfnTextMessageGet( const char *pName )
{
	int	i;

	// find desired message
	for( i = 0; i < clgame.numTitles; i++ )
	{
		if( !com.strcmp( pName, clgame.titles[i].pName ))
			return clgame.titles + i;
	}
	return NULL; // found nothing
}

/*
=============
pfnDrawCharacter

returns drawed chachter width (in real screen pixels)
=============
*/
static int pfnDrawCharacter( int x, int y, int number, int r, int g, int b )
{
	float	size, frow, fcol;
	float	ax, ay, aw, ah;
	int	fontWidth, fontHeight;
	rgba_t	color;

	number &= 255;

	if( !re ) return 0;
	if( number == ' ' ) return 0;
	if( y < -clgame.scrInfo.iCharHeight )
		return 0;

	ax = x;
	ay = y;
	aw = clgame.scrInfo.charWidths[number];
	ah = clgame.scrInfo.iCharHeight;
	SPR_AdjustSize( &ax, &ay, &aw, &ah );
	re->GetParms( &fontWidth, &fontHeight, NULL, 0, clgame.ds.hHudFont );

	MakeRGBA( color, r, g, b, 255 );
	re->SetColor( color );
	
	frow = (number >> 4)*0.0625f + (0.5f / (float)fontWidth);
	fcol = (number & 15)*0.0625f + (0.5f / (float)fontHeight);
	size = 0.0625f - (1.0f / (float)fontWidth);

	re->DrawStretchPic( ax, ay, aw, ah, fcol, frow, fcol + size, frow + size, clgame.ds.hHudFont );
	re->SetColor( NULL ); // don't forget reset color

	return clgame.scrInfo.charWidths[number];
}

/*
=============
pfnDrawConsoleString

drawing string like a console string 
=============
*/
static int pfnDrawConsoleString( int x, int y, char *string )
{
	if( !string || !*string ) return 0; // silent ignore
	SCR_DrawSmallStringExt( x, y, string, NULL, false );
	return com.cstrlen( string ) * SMALLCHAR_WIDTH; // not includes color prexfixes
}

/*
=============
pfnDrawSetTextColor

set color for anything
=============
*/
static void pfnDrawSetTextColor( float r, float g, float b )
{
	rgba_t	color;

	// bound color and convert to byte
	color[0] = (byte)bound( 0, r * 255, 255 );
	color[1] = (byte)bound( 0, g * 255, 255 );
	color[2] = (byte)bound( 0, b * 255, 255 );
	color[3] = (byte)0xFF;
	if( re ) re->SetColor( color );
}

/*
=============
pfnDrawConsoleStringLen

returns width and height (in real pixels)
for specified string
=============
*/
static void pfnDrawConsoleStringLen(  const char *string, int *length, int *height )
{
	// console used fixed font size
	if( length ) *length = com.cstrlen( string ) * SMALLCHAR_WIDTH;
	if( height ) *height = SMALLCHAR_HEIGHT;
}

/*
=============
pfnConsolePrint

prints dirctly into console (can skip notify)
=============
*/
static void pfnConsolePrint( const char *string )
{
	if( !string || !*string ) return;
	if( *string == 1 ) Con_Print( string + 1 ); // show notify
	else Con_Print( va( "[skipnotify]%s", string )); // skip notify
}

/*
=============
pfnCenterPrint

holds and fade message at center of screen
like trigger_multiple message in q1
=============
*/
static void pfnCenterPrint( const char *string )
{
	if( !string || !*string ) return; // someone stupid joke
	CL_CenterPrint( string, 160, SMALLCHAR_WIDTH );
}
	
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
pfnDelCommand

=============
*/
void pfnDelCommand( const char *cmd_name )
{
	if( !cmd_name || !*cmd_name ) return;

	Cmd_RemoveCommand( cmd_name );
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
	case KEY_GAME:
		cls.key_dest = key_game;
		break;
	case KEY_HUDMENU:
		cls.key_dest = key_hudmenu;
		break;
	default:
		MsgDev( D_ERROR, "CL_SetKeyDest: wrong destination %i!\n", key_dest );
		break;
	}
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
	return CL_GetMaxClients ();
}

/*
=============
pfnGetViewModel

can return NULL
=============
*/
edict_t* pfnGetViewModel( void )
{
	return &clgame.viewent;
}

/*
=============
pfnGetModelPtr

returns pointer to a studiomodel
=============
*/
static void *pfnGetModelPtr( edict_t *pEdict )
{
	if( !pEdict || pEdict->free )
		return NULL;

	return Mod_Extradata( pEdict->v.modelindex );
}

/*
=============
pfnGetAttachment

=============
*/
static void pfnGetAttachment( const edict_t *pEdict, int iAttachment, float *rgflOrigin, float *rgflAngles )
{
	if( !pEdict )
	{
		if( rgflOrigin ) VectorClear( rgflOrigin );
		if( rgflAngles ) VectorClear( rgflAngles );
		return;
	}
	CL_GetAttachment( pEdict->serialnumber, iAttachment, rgflOrigin, rgflAngles );
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
	trace_t	result;

	if( VectorIsNAN( v1 ) || VectorIsNAN( v2 ))
		Host_Error( "TraceLine: NAN errors detected '%f %f %f', '%f %f %f'\n", v1[0], v1[1], v1[2], v2[0], v2[1], v2[2] );
	result = CL_Move( v1, vec3_origin, vec3_origin, v2, fNoMonsters, pentToSkip );
	if( ptr ) Mem_Copy( ptr, &result, sizeof( *ptr ));
}

/*
=================
pfnTraceToss

=================
*/
static void pfnTraceToss( edict_t* pent, edict_t* pentToIgnore, TraceResult *ptr )
{
	trace_t	result;

	if( !CL_IsValidEdict( pent ))
	{
		MsgDev( D_WARN, "CL_MoveToss: invalid entity %s\n", CL_ClassName( pent ));
		return;
	}

	result = CL_MoveToss( pent, pentToIgnore );
	if( ptr ) Mem_Copy( ptr, &result, sizeof( *ptr ));
}

/*
=================
pfnTraceHull

=================
*/
static void pfnTraceHull( const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t *pentToSkip, TraceResult *ptr )
{
	trace_t	result;
	float	*mins, *maxs;

	hullNumber = bound( 0, hullNumber, 3 );
	mins = GI->client_mins[hullNumber];
	maxs = GI->client_maxs[hullNumber];

	if( VectorIsNAN( v1 ) || VectorIsNAN( v2 ))
		Host_Error( "TraceHull: NAN errors detected '%f %f %f', '%f %f %f'\n", v1[0], v1[1], v1[2], v2[0], v2[1], v2[2] );
	result = CL_Move( v1, mins, maxs, v2, fNoMonsters, pentToSkip );
	if( ptr ) Mem_Copy( ptr, &result, sizeof( *ptr ));
}

static void pfnTraceModel( const float *v1, const float *v2, edict_t *pent, TraceResult *ptr )
{
	trace_t	result;

	if( !CL_IsValidEdict( pent ))
	{
		MsgDev( D_WARN, "CL_TraceModel: invalid entity %s\n", CL_ClassName( pent ));
		return;
	}

	if( VectorIsNAN( v1 ) || VectorIsNAN( v2 ))
		Host_Error( "TraceModel: NAN errors detected '%f %f %f', '%f %f %f'\n", v1[0], v1[1], v1[2], v2[0], v2[1], v2[2] );
	result = CL_ClipMoveToEntity( pent, v1, pent->v.mins, pent->v.maxs, v2, MASK_SOLID, 0 );
	if( ptr ) Mem_Copy( ptr, &result, sizeof( *ptr ));
}

static const char *pfnTraceTexture( edict_t *pTextureEntity, const float *v1, const float *v2 )
{
	if( VectorIsNAN( v1 ) || VectorIsNAN( v2 ))
		Host_Error( "TraceTexture: NAN errors detected '%f %f %f', '%f %f %f'\n", v1[0], v1[1], v1[2], v2[0], v2[1], v2[2] );
	if( !pTextureEntity || pTextureEntity->free ) return NULL; 
	return CL_ClipMoveToEntity( pTextureEntity, v1, vec3_origin, vec3_origin, v2, MASK_SOLID, 0 ).pTexName;
}

/*
=============
pfnPrecacheEvent

=============
*/
static word pfnPrecacheEvent( int type, const char* psz )
{
	return CL_PrecacheEvent( psz );
}

/*
=============
pfnHookEvent

=============
*/
static void pfnHookEvent( const char *name, pfnEventHook pfn )
{
	word		event_index = CL_PrecacheEvent( name );
	user_event_t	*ev;
	int		i;

	// ignore blank names
	if( !name || !*name ) return;	

	// second call can change EventFunc
	for( i = 0; i < MAX_EVENTS; i++ )
	{
		ev = clgame.events[i];		
		if( !ev ) break;

		if( !com.strcmp( name, ev->name ))
		{
			if( ev->func != pfn )
				ev->func = pfn;
			return;
		}
	}

	CL_RegisterEvent( i, name, pfn );
}

/*
=============
pfnPlaybackEvent

=============
*/
static void pfnPlaybackEvent( int flags, const edict_t *pInvoker, word eventindex, float delay, event_args_t *args )
{
	event_args_t	dummy;
	int		invokerIndex = 0;

	// first check event for out of bounds
	if( eventindex < 1 || eventindex > MAX_EVENTS )
	{
		MsgDev( D_ERROR, "CL_PlaybackEvent: invalid eventindex %i\n", eventindex );
		return;
	}
	// check event for precached
	if( !CL_PrecacheEvent( cl.configstrings[CS_EVENTS+eventindex] ))
	{
		MsgDev( D_ERROR, "CL_PlaybackEvent: event %i was not precached\n", eventindex );
		return;		
	}

	flags |= FEV_CLIENT; // it's a client event
	flags &= ~(FEV_NOTHOST|FEV_HOSTONLY|FEV_GLOBAL);

	if( delay < 0.0f )
		delay = 0.0f; // fixup negative delays

	if( CL_IsValidEdict( pInvoker ))
		invokerIndex = NUM_FOR_EDICT( pInvoker );

	if( args == NULL )
	{
		Mem_Set( &dummy, 0, sizeof( dummy ));
		args = &dummy;
	}

	if( flags & FEV_RELIABLE )
	{
		args->ducking = 0;
		VectorClear( args->velocity );
	}
	else if( invokerIndex )
	{
		// get up some info from invoker
		if( VectorIsNull( args->origin )) 
			VectorCopy( pInvoker->v.origin, args->origin );
		if( VectorIsNull( args->angles )) 
			VectorCopy( pInvoker->v.angles, args->angles );
		VectorCopy( pInvoker->v.velocity, args->velocity );
		args->ducking = (pInvoker->v.flags & FL_DUCKING) ? true : false;
	}

	CL_QueueEvent( flags, eventindex, delay, args );
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
=================
pfnFindExplosionPlane

=================
*/
static void pfnFindExplosionPlane( const float *origin, float radius, float *result )
{
	CL_FindExplosionPlane( origin, radius, result );
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
static void Tri_AddPolygon( void )
{
	if( clgame.pTri->caps[TRI_SHADER] )
		clgame.pTri->currentPolygon.shadernum = clgame.pTri->currentShader;
	else clgame.pTri->currentPolygon.shadernum = cls.fillShader;

	if( clgame.pTri->hasNormals )
		clgame.pTri->currentPolygon.normals = clgame.pTri->normals;		
	else clgame.pTri->currentPolygon.normals = NULL; // no normals

	clgame.pTri->currentPolygon.verts = clgame.pTri->verts;
	clgame.pTri->currentPolygon.stcoords = clgame.pTri->stcoords;
	clgame.pTri->currentPolygon.colors = clgame.pTri->colors;
	clgame.pTri->currentPolygon.elems = clgame.pTri->elems;

	clgame.pTri->currentPolygon.numverts = clgame.pTri->numVertex;
	clgame.pTri->currentPolygon.numelems = clgame.pTri->numIndex;

	clgame.pTri->numVertex = clgame.pTri->numIndex = clgame.pTri->numColor = 0;

	if( re ) re->AddPolygon( &clgame.pTri->currentPolygon );
}

static void Tri_CheckOverflow( int numIndices, int numVertices )
{
	if( numIndices > MAX_TRIELEMS )
		Host_Error( "Tri_CheckOverflow: %i > MAX_TRIELEMS\n", numIndices );
	if( numVertices > MAX_TRIVERTS )
		Host_Error( "Tri_CheckOverflow: %i > MAX_TRIVERTS\n", numVertices );			

	if( clgame.pTri->numIndex + numIndices <= MAX_TRIELEMS && clgame.pTri->numVertex + numVertices <= MAX_TRIVERTS )
		return;

	Tri_AddPolygon();
}

static void Tri_SetVertex( float x, float y, float z )
{
	uint	oldIndex = clgame.pTri->numIndex;

	switch( clgame.pTri->drawMode )
	{
	case TRI_LINES:
		clgame.pTri->elems[clgame.pTri->numIndex++] = clgame.pTri->numVertex;
		if( clgame.pTri->vertexState++ == 1 )
		{
			Tri_SetVertex( x + 1, y + 1, z + 1 );
			clgame.pTri->vertexState = 0;
			clgame.pTri->checkFlush = true; // Flush for long sequences of quads.
		}
		break;
	case TRI_TRIANGLES:
		clgame.pTri->elems[clgame.pTri->numIndex++] = clgame.pTri->numVertex;
		if( clgame.pTri->vertexState++ == 2 )
		{
			clgame.pTri->vertexState = 0;
			clgame.pTri->checkFlush = true; // Flush for long sequences of triangles.
		}
		break;
	case TRI_QUADS:
		if( clgame.pTri->vertexState++ < 3 )
		{
			clgame.pTri->elems[clgame.pTri->numIndex++] = clgame.pTri->numVertex;
		}
		else
		{
			// we've already done triangle (0, 1, 2), now draw (2, 3, 0)
			clgame.pTri->elems[clgame.pTri->numIndex++] = clgame.pTri->numVertex - 1;
			clgame.pTri->elems[clgame.pTri->numIndex++] = clgame.pTri->numVertex;
			clgame.pTri->elems[clgame.pTri->numIndex++] = clgame.pTri->numVertex - 3;
			clgame.pTri->vertexState = 0;
			clgame.pTri->checkFlush = true; // flush for long sequences of quads.
		}
		break;
	case TRI_TRIANGLE_STRIP:
		if( clgame.pTri->numVertex + clgame.pTri->vertexState > MAX_TRIVERTS )
		{
			// This is a strip that's too big for us to buffer.
			// (We can't just flush the buffer because we have to keep
			// track of the last two vertices.
			Host_Error( "Tri_SetVertex: overflow: %i > MAX_TRIVERTS\n", clgame.pTri->numVertex + clgame.pTri->vertexState );
		}
		if( clgame.pTri->vertexState++ < 3 )
		{
			clgame.pTri->elems[clgame.pTri->numIndex++] = clgame.pTri->numVertex;
		}
		else
		{
			// flip triangles between clockwise and counter clockwise
			if( clgame.pTri->vertexState & 1 )
			{
				// draw triangle [n-2 n-1 n]
				clgame.pTri->elems[clgame.pTri->numIndex++] = clgame.pTri->numVertex - 2;
				clgame.pTri->elems[clgame.pTri->numIndex++] = clgame.pTri->numVertex - 1;
				clgame.pTri->elems[clgame.pTri->numIndex++] = clgame.pTri->numVertex;
			}
			else
			{
				// draw triangle [n-1 n-2 n]
				clgame.pTri->elems[clgame.pTri->numIndex++] = clgame.pTri->numVertex - 1;
				clgame.pTri->elems[clgame.pTri->numIndex++] = clgame.pTri->numVertex - 2;
				clgame.pTri->elems[clgame.pTri->numIndex++] = clgame.pTri->numVertex;
			}
		}
		break;
	case TRI_POLYGON:
	case TRI_TRIANGLE_FAN:	// same as polygon
		if( clgame.pTri->numVertex + clgame.pTri->vertexState > MAX_TRIVERTS )
		{
			// This is a polygon or fan that's too big for us to buffer.
			// (We can't just flush the buffer because we have to keep
			// track of the starting vertex.
			Host_Error( "Tri_SetVertex: overflow: %i > MAX_TRIVERTS\n", clgame.pTri->numVertex + clgame.pTri->vertexState );
		}
		if( clgame.pTri->vertexState++ < 3 )
		{
			clgame.pTri->elems[clgame.pTri->numIndex++] = clgame.pTri->numVertex;
		}
		else
		{
			// draw triangle [0 n-1 n]
			clgame.pTri->elems[clgame.pTri->numIndex++] = clgame.pTri->numVertex - ( clgame.pTri->vertexState - 1 );
			clgame.pTri->elems[clgame.pTri->numIndex++] = clgame.pTri->numVertex - 1;
			clgame.pTri->elems[clgame.pTri->numIndex++] = clgame.pTri->numVertex;
		}
		break;
	default:
		Host_Error( "Tri_SetVertex: unknown mode: %i\n", clgame.pTri->drawMode );
		break;
	}

	// copy current vertex
	clgame.pTri->verts[clgame.pTri->numVertex][0] = x;
	clgame.pTri->verts[clgame.pTri->numVertex][1] = y;
	clgame.pTri->verts[clgame.pTri->numVertex][2] = z;

	// copy current color
	if( clgame.pTri->numColor == 0 )
		Vector4Copy( clgame.pTri->color, clgame.pTri->colors[clgame.pTri->numVertex] );

	clgame.pTri->numVertex++;

	// flush buffer if needed
	if( clgame.pTri->checkFlush )
		Tri_CheckOverflow( clgame.pTri->numIndex - oldIndex, clgame.pTri->vertexState );
}

void TriRenderMode( kRenderMode_t mode )
{
	if( !re ) return;

	// FIXME: implement it properly
//	re->SetParms( clgame.pTri->currentShader, mode, clgame.pTri->currentFrame );
}

void TriBind( shader_t shader, int frame )
{
	clgame.pTri->currentShader = shader;
	clgame.pTri->currentFrame = frame;
}

void TriBegin( TRI_DRAW mode )
{
	clgame.pTri->drawMode = mode;
	clgame.pTri->vertexState = 0;
	clgame.pTri->checkFlush = false;
	clgame.pTri->hasNormals = false;

	clgame.pTri->verts = clgame.pTri->vertsArray[clgame.pTri->numPolys];
	clgame.pTri->normals = clgame.pTri->normalsArray[clgame.pTri->numPolys];
	clgame.pTri->stcoords = clgame.pTri->stcoordsArray[clgame.pTri->numPolys];
	clgame.pTri->colors = clgame.pTri->colorsArray[clgame.pTri->numPolys];
	clgame.pTri->elems = clgame.pTri->elemsArray[clgame.pTri->numPolys];
}

void TriEnd( void )
{
	if( clgame.pTri->numIndex == 0 ) return;

	Tri_AddPolygon();

	if( ++clgame.pTri->numPolys > MAX_TRIPOLYS );
		clgame.pTri->numPolys = 0; // reset it

	clgame.pTri->verts = NULL;
	clgame.pTri->normals = NULL;
	clgame.pTri->stcoords = NULL;
	clgame.pTri->colors = NULL;
	clgame.pTri->elems = NULL;
}

void TriEnable( int cap )
{
	if( cap < 0 || cap > TRI_MAXCAPS ) return;
	clgame.pTri->caps[cap] = true;
}

void TriDisable( int cap )
{
	if( cap < 0 || cap > TRI_MAXCAPS ) return;
	clgame.pTri->caps[cap] = false;
}

void TriVertex3f( float x, float y, float z )
{
	Tri_SetVertex( x, y, z );
}

void TriVertex3fv( const float *v )
{
	if( !v ) return;
	Tri_SetVertex( v[0], v[1], v[2] );
}

void TriNormal3f( float x, float y, float z )
{
	clgame.pTri->hasNormals = true; // draw has normals
	clgame.pTri->normals[clgame.pTri->numVertex][0] = x;
	clgame.pTri->normals[clgame.pTri->numVertex][1] = y;
	clgame.pTri->normals[clgame.pTri->numVertex][2] = z;
}

void TriNormal3fv( const float *v )
{
	if( !v ) return;

	clgame.pTri->hasNormals = true; // draw has normals
	clgame.pTri->normals[clgame.pTri->numVertex][0] = v[0];
	clgame.pTri->normals[clgame.pTri->numVertex][1] = v[1];
	clgame.pTri->normals[clgame.pTri->numVertex][2] = v[2];
}

void TriColor4f( float r, float g, float b, float a )
{
	if( clgame.pTri->colors )
	{
		clgame.pTri->colors[clgame.pTri->numVertex][0] = (byte)bound( 0, r * 255, 255 );
		clgame.pTri->colors[clgame.pTri->numVertex][1] = (byte)bound( 0, g * 255, 255 );
		clgame.pTri->colors[clgame.pTri->numVertex][2] = (byte)bound( 0, b * 255, 255 );
		clgame.pTri->colors[clgame.pTri->numVertex][3] = (byte)bound( 0, a * 255, 255 );
		clgame.pTri->numColor++;
	}
	else
	{
		clgame.pTri->color[0] = (byte)bound( 0, r * 255, 255 );
		clgame.pTri->color[1] = (byte)bound( 0, g * 255, 255 );
		clgame.pTri->color[2] = (byte)bound( 0, b * 255, 255 );
		clgame.pTri->color[3] = (byte)bound( 0, a * 255, 255 );
	}
}

void TriColor4ub( byte r, byte g, byte b, byte a )
{
	if( clgame.pTri->colors )
	{
		clgame.pTri->colors[clgame.pTri->numVertex][0] = r;
		clgame.pTri->colors[clgame.pTri->numVertex][1] = g;
		clgame.pTri->colors[clgame.pTri->numVertex][2] = b;
		clgame.pTri->colors[clgame.pTri->numVertex][3] = a;
		clgame.pTri->numColor++;
	}
	else Vector4Set( clgame.pTri->color, r, g, b, a );
}

void TriTexCoord2f( float u, float v )
{
	clgame.pTri->stcoords[clgame.pTri->numVertex][0] = u;
	clgame.pTri->stcoords[clgame.pTri->numVertex][1] = v;
}

void TriCullFace( TRI_CULL mode )
{
	// FIXME: implement
}

void TriScreenToWorld( float *screen, float *world )
{
	if( !re ) return;
	re->ScreenToWorld( screen, world );
} 

int TriWorldToScreen( float *world, float *screen )
{
	if( !re ) return 0;
	return re->WorldToScreen( world, screen );
}

void TriFog( float flFogColor[3], float flStart, float flEnd, int bOn )
{
	// FIXME: implement
}

static triapi_t gTriApi =
{
	sizeof( triapi_t ),	
	TriRenderMode,
	TriBegin,
	TriEnd,
	TriEnable,
	TriDisable,
	TriVertex3f,
	TriVertex3fv,
	TriNormal3f,
	TriNormal3fv,
	TriColor4f,
	TriColor4ub,
	TriTexCoord2f,
	TriBind,
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
	pfnSPR_Load,
	pfnSPR_Frames,
	pfnSPR_Height,
	pfnSPR_Width,
	pfnSPR_Set,
	pfnSPR_Draw,
	pfnSPR_DrawHoles,
	pfnSPR_DrawTrans,
	pfnSPR_DrawAdditive,
	pfnSPR_EnableScissor,
	pfnSPR_DisableScissor,
	pfnSPR_GetList,
	pfnFillRGBA,
	pfnGetScreenInfo,
	pfnSetCrosshair,
	pfnCVarRegister,
	pfnCVarGetValue,
	pfnCVarGetString,
	pfnAddCommand,
	pfnHookUserMsg,
	pfnServerCmd,
	pfnClientCmd,
	pfnGetPlayerInfo,
	pfnPlaySoundByName,
	pfnPlaySoundByIndex,
	AngleVectors,
	pfnTextMessageGet,
	pfnDrawCharacter,
	pfnDrawConsoleString,
	pfnDrawSetTextColor,
	pfnDrawConsoleStringLen,
	pfnConsolePrint,
	pfnCenterPrint,

	pfnMemAlloc,
	pfnMemCopy,
	pfnMemFree,
	pfnLoadShader,
	pfnCVarSetString,
	pfnCVarSetValue,
	pfnDelCommand,
	pfnSetKeyDest,
	pfnCmdArgc,
	pfnCmdArgv,	
	pfnAlertMessage,
	pfnGetViewAngles,
	CL_GetEdictByIndex,
	CL_GetLocalPlayer,
	pfnIsSpectateOnly,
	pfnGetClientTime,
	CL_GetLerpFrac,
	pfnGetMaxClients,
	pfnGetViewModel,
	pfnGetModelPtr,
	pfnGetAttachment,
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
	pfnRemoveFile,				
	pfnLoadLibrary,
	pfnGetProcAddress,
	pfnFreeLibrary,		
	Host_Error,
	&gTriApi,
	&gEfxApi
};

void CL_UnloadProgs( void )
{
	// initialize game
	clgame.dllFuncs.pfnShutdown();

	CL_FreeEdicts();
	CL_FreeEdict( &clgame.viewent );
	CL_FreeEdict( &clgame.playermodel );

	StringTable_Delete( clgame.hStringTable );
	Com_FreeLibrary( clgame.hInstance );
	Mem_FreePool( &cls.mempool );
	Mem_FreePool( &clgame.mempool );
	Mem_FreePool( &clgame.private );
}

bool CL_LoadProgs( const char *name )
{
	static CLIENTAPI		GetClientAPI;
	static cl_globalvars_t	gpGlobals;
	static tri_state_t		gpTriState;
	static playermove_t		gpMove;
	string			libpath;

	if( clgame.hInstance ) CL_UnloadProgs();

	// setup globals
	clgame.globals = &gpGlobals;

	// initialize TriAPI
	clgame.pmove = &gpMove;
	clgame.pTri = &gpTriState;
	clgame.pTri->currentPolygon.fognum = -1;
	clgame.pTri->numPolys = 0;
	
	Com_BuildPath( name, libpath );
	cls.mempool = Mem_AllocPool( "Client Static Pool" );
	clgame.mempool = Mem_AllocPool( "Client Edicts Zone" );
	clgame.private = Mem_AllocPool( "Client Private Zone" );
	clgame.baselines = NULL;
	clgame.edicts = NULL;

	clgame.hInstance = Com_LoadLibrary( libpath );
	if( !clgame.hInstance ) return false;

	GetClientAPI = (CLIENTAPI)Com_GetProcAddress( clgame.hInstance, "CreateAPI" );

	if( !GetClientAPI )
	{
          	MsgDev( D_ERROR, "CL_LoadProgs: failed to get address of CreateAPI proc\n" );
		return false;
	}

	if( !GetClientAPI( &clgame.dllFuncs, &gEngfuncs, clgame.globals ))
	{
		MsgDev( D_ERROR, "CL_LoadProgs: can't init client API\n" );
		return false;
	}

	// 65535 unique strings should be enough ...
	clgame.hStringTable = StringTable_Create( "Client", 0x10000 );
	clgame.globals->maxEntities = GI->max_edicts; // merge during loading

	// register svc_bad message
	pfnHookUserMsg( "bad", NULL );
	CL_LinkUserMessage( "bad@0", svc_bad );

	CL_InitEdict( &clgame.viewent );
	clgame.viewent.serialnumber = VIEWENT_INDEX;

	CL_InitEdict( &clgame.playermodel );
	clgame.playermodel.serialnumber = MAX_EDICTS - 1;

	CL_InitTitles( "scripts/titles.txt" );

	// initialize game
	clgame.dllFuncs.pfnInit();

	return true;
}