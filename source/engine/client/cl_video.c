//=======================================================================
//			Copyright XashXT Group 2009 ©
//			cl_video.c - roq video player
//=======================================================================

#include "common.h"
#include "client.h"

/*
=================================================================

ROQ PLAYING

=================================================================
*/

/*
==================
SCR_StopCinematic
==================
*/
void SCR_StopCinematic( void )
{
	cinematics_t	*cin = cl.cin;

	if( !cin || !cin->file )
		return;

	cl.cin = NULL;
	cin->time = 0;	// done
	cin->pic = NULL;
	cin->pic_pending = NULL;

	if( cin->file ) FS_Close( cin->file );
	cin->file = NULL;

	Mem_Free( cin->name );
	cin->name = NULL;

	if( cin->vid_buffer )
	{
		Mem_Free( cin->vid_buffer );
		cin->vid_buffer = NULL;
	}

	cls.state = ca_disconnected;
	UI_SetActiveMenu( UI_MAINMENU );
}

//==========================================================================

/*
==================
SCR_InitCinematic
==================
*/
void SCR_InitCinematic( void )
{
	CIN_Init ();
}

/*
==================
SCR_InitCinematic
==================
*/
uint SCR_GetCinematicTime( void )
{
	cinematics_t	*cin = cl.cin;
	return (cin ? cin->time : 0);
}

/*
==================
SCR_RunCinematic
==================
*/
void SCR_RunCinematic( void )
{
	uint		frame;
	cinematics_t	*cin = cl.cin;

	if( !cin || cin->time == 0 )
	{
		SCR_StopCinematic ();
		return;
	}

	frame = (Host_Milliseconds() - cin->time) * (float)(RoQ_FRAMERATE) / 1000;
	if( frame <= cin->frame ) return;

	if( frame > cin->frame + 1 )
	{
		MsgDev( D_WARN, "dropped frame: %i > %i\n", frame, cin->frame + 1 );
		cin->time = Host_Milliseconds() - cin->frame * 1000 / RoQ_FRAMERATE;
	}

	cin->pic = cin->pic_pending;
	cin->pic_pending = CIN_ReadNextFrame( cin, false );

	if( !cin->pic_pending )
	{
		SCR_StopCinematic ();
		return;
	}
}

/*
==================
SCR_DrawCinematic

Returns true if a cinematic is active, meaning the view rendering
should be skipped
==================
*/
bool SCR_DrawCinematic( void )
{
	cinematics_t	*cin = cl.cin;
	float		x, y, w, h;
 
	if( !re || !cin || cin->time <= 0 )
		return false;
	if( !cin->pic )
		return true;

	x = y = 0;
	w = SCREEN_WIDTH;
	h = SCREEN_HEIGHT;
	SCR_AdjustSize( &x, &y, &w, &h );
	re->DrawStretchRaw( x, y, w, h, cin->width, cin->height, cin->pic, true );

	return true;
}

/*
==================
SCR_PlayCinematic
==================
*/
bool SCR_PlayCinematic( const char *arg )
{
	size_t		name_size;
	static cinematics_t	clientCin;
	cinematics_t	*cin = cl.cin = &clientCin;
	droqchunk_t	*chunk = &cin->chunk;

	if( cls.state == ca_cinematic )
	{
		// first stop the old movie
		SCR_StopCinematic ();
	}

	name_size = com.strlen( "media/" ) + com.strlen( arg ) + com.strlen( ".roq" ) + 1;
	cin->name = Mem_Alloc( cls.mempool, name_size );
	com.snprintf( cin->name, name_size, "media/%s", arg );
	FS_DefaultExtension( cin->name, ".roq" );

	// nasty hack
	cin->s_rate = 22050;
	cin->s_width = 2;
	cin->width = cin->height = 0;

	cin->frame = 0;
	cin->file = FS_Open( cin->name, "rb" );

	if( !cin->file )
	{
		MsgDev( D_INFO, "SCR_PlayCinematic: unable to find %s\n", cin->name );
		SCR_StopCinematic ();
		return false;
	}

	// read header
	CIN_ReadChunk( cin );

	if( chunk->id != RoQ_HEADER1 || chunk->size != RoQ_HEADER2 || chunk->argument != RoQ_HEADER3 )
	{
		MsgDev( D_ERROR, "%s invalid header chunk %x\n", cin->name, chunk->id );
		SCR_StopCinematic();
		return false;
	}

	UI_SetActiveMenu( UI_CLOSEMENU );
	S_StopAllSounds();
	S_StartStreaming();

	cls.state = ca_cinematic;

	cin->headerlen = FS_Tell( cin->file );
	cin->frame = 0;
	cin->pic = cin->pic_pending = CIN_ReadNextFrame( cin, false );
	cin->time = Host_Milliseconds ();

	return true;
}