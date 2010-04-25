//=======================================================================
//			Copyright XashXT Group 2009 ©
//		         net_sound.h - sound message options
//=======================================================================
#ifndef NET_SOUND_H
#define NET_SOUND_H

// sound flags
// see declaration flags 1 - 64 in const.h

#define SND_SENTENCE		(1<<7)	// set if sound num is actually a sentence num
#define SND_VOLUME			(1<<8)	// a scaled byte
#define SND_ATTENUATION		(1<<9)	// a byte
#define SND_PITCH			(1<<10)	// a byte
#define SND_FIXED_ORIGIN		(1<<11)	// a vector

#endif//NET_SOUND_H