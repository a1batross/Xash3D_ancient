/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "client.h"

roq_dec_t		*cin;
/*
==================
SCR_StopCinematic
==================
*/
void SCR_StopCinematic (void)
{
	cl.cinematictime = 0;	// done
	if(!cin) return;

	if(cin->restart_sound)
	{
		// switch back down to 11 khz sound if necessary
		cin->restart_sound = false;
		CL_Snd_Restart_f();
	}
	Msg("total [%d] frames played\n", cin->frameNum );
	Com->Roq.FreeVideo( cin );
	cin = NULL;
}

/*
====================
SCR_FinishCinematic

Called when either the cinematic completes, or it is aborted
====================
*/
void SCR_FinishCinematic (void)
{
	// tell the server to advance to the next map / cinematic
	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, va("nextserver %i\n", cl.servercount));
}

/*
==================
SCR_ReadNextFrame
==================
*/
bool SCR_ReadNextFrame (void)
{
	switch(Com->Roq.ReadFrame( cin ))
	{
		case ROQ_MARKER_INFO:
			break;
		case ROQ_MARKER_VIDEO:
			//re->DrawStretchRaw(0, 0, viddef.width, viddef.height, cin->width, cin->height, cin->rgb );
			break;
		case ROQ_MARKER_EOF:
			return false;
		case ROQ_MARKER_SND_MONO:
			S_RawSamples (cin->audioSize, 22050, 2, 1, (byte *)cin->audioSamples, 1.0f );
			break;
		case ROQ_MARKER_SND_STEREO:
			S_RawSamples (cin->audioSize, 22050, 2, 2, (byte *)cin->audioSamples, 1.0f );
			break;
	}
	return true;
}


/*
==================
SCR_RunCinematic

==================
*/
void SCR_RunCinematic (void)
{
	int		frame;

	if (cl.cinematictime <= 0)
	{
		SCR_StopCinematic ();
		return;
	}

	/*if (cls.key_dest != key_game)
	{	
		// pause if menu or console is up
		cl.cinematictime = cls.realtime - cin->frameNum / 30;
		return;
	}*/

	frame = (cls.realtime - cl.cinematictime) * 30.0;
	if (frame + 1 < cin->frameNum) return; //too early
	if (frame > cin->frameNum + 1)
	{
		MsgWarn("SCR_RunCinematic: dropped frame: %i > %i\n", frame, cin->frameNum + 1);
		cl.cinematictime = cls.realtime - cin->frameNum / 30;
	}

	if(!SCR_ReadNextFrame())
	{
		SCR_StopCinematic();
		SCR_FinishCinematic();
		cl.cinematictime = 1; // hack to get the black screen behind loading
		SCR_BeginLoadingPlaque();
		cl.cinematictime = 0;
	}
}

/*
==================
SCR_DrawCinematic

Returns true if a cinematic is active, meaning the view rendering
should be skipped
==================
*/
bool SCR_DrawCinematic (void)
{
	if (cl.cinematictime <= 0) return false;
	if (!cin || !cin->rgb) return true;

	re->DrawStretchRaw(0, 0, viddef.width, viddef.height, cin->width, cin->height, cin->rgb );

	return true;
}

/*
==================
SCR_PlayCinematic

==================
*/
void SCR_PlayCinematic (char *arg)
{
	int		old_khz;

	cin = Com->Roq.LoadVideo( arg );
	if (!cin)
	{
		SCR_FinishCinematic ();
		return;
	}

	SCR_EndLoadingPlaque();
	cls.state = ca_active;

	// switch up to 22 khz sound if necessary
	old_khz = Cvar_VariableValue ("s_khz");
	if (old_khz != 22)
	{
		cin->restart_sound = true;
		Cvar_SetValue ("s_khz", 22 );
		CL_Snd_Restart_f();
		Cvar_SetValue ("s_khz", old_khz );
	}

	SCR_ReadNextFrame(); //first frame
	cl.cinematictime = Sys_DoubleTime();
}
