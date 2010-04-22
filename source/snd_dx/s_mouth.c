//=======================================================================
//			Copyright XashXT Group 2010 ©
//			 s_mouth.c - animate mouth
//=======================================================================

#include "sound.h"
#include "const.h"

void SND_InitMouth( int entnum, int entchannel )
{
	if(( entchannel == CHAN_VOICE || entchannel == CHAN_STREAM ) && entnum > 0 )
	{
		edict_t	*clientEntity;

		// init mouth movement vars
		clientEntity = si.GetClientEdict( entnum );

		if( clientEntity )
		{
			mouth_t	*m = si.GetEntityMouth( clientEntity );

			if( m )
			{
				m->mouthopen = 0;
				m->sndavg = 0;
				m->sndcount = 0;
			}
		}
	}
}

void SND_CloseMouth( channel_t *ch )
{
	edict_t	*clientEntity = si.GetClientEdict( ch->entnum );

	if( clientEntity )
	{
		mouth_t	*m = si.GetEntityMouth( clientEntity );

		if( m && ( ch->entchannel == CHAN_VOICE || ch->entchannel == CHAN_STREAM ))
		{
			// shut mouth
			m->mouthopen = 0;
		}
	}
}