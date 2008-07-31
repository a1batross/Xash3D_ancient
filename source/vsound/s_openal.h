//=======================================================================
//			Copyright XashXT Group 2007 ©
//			s_openal.h - openal definition
//=======================================================================

#ifndef S_OPENAL_H
#define S_OPENAL_H

typedef struct aldevice_s aldevice;
typedef struct alcontext_s alcontext;

#define NVIDIA_DEVICE_NAME      		"NVIDIA(R) nForce(TM) Audio"
#define MAX_EFFECTS				128

#define AL_VENDOR                                 0xB001
#define AL_VERSION                                0xB002
#define AL_RENDERER                               0xB003
#define AL_EXTENSIONS                             0xB004
#define ALC_DEFAULT_DEVICE_SPECIFIER              0x1004
#define ALC_DEVICE_SPECIFIER                      0x1005
#define ALC_EXTENSIONS                            0x1006
#define ALC_MAJOR_VERSION                         0x1000
#define ALC_MINOR_VERSION                         0x1001
#define ALC_ATTRIBUTES_SIZE                       0x1002
#define ALC_ALL_ATTRIBUTES                        0x1003
#define ALC_FREQUENCY			0x1007
#define ALC_MAX_AUXILIARY_SENDS		0x20003
#define ALC_MONO_SOURCES			0x1010
#define ALC_STEREO_SOURCES			0x1011

#define AL_SOURCE_RELATIVE                        0x202
#define AL_CONE_INNER_ANGLE                       0x1001
#define AL_CONE_OUTER_ANGLE                       0x1002
#define AL_PITCH                                  0x1003
#define AL_POSITION                               0x1004
#define AL_DIRECTION                              0x1005
#define AL_VELOCITY                               0x1006
#define AL_LOOPING                                0x1007
#define AL_BUFFER                                 0x1009
#define AL_GAIN                                   0x100A
#define AL_MIN_GAIN                               0x100D
#define AL_MAX_GAIN                               0x100E
#define AL_ORIENTATION                            0x100F

#define AL_SOURCE_STATE                           0x1010
#define AL_INITIAL                                0x1011
#define AL_PLAYING                                0x1012
#define AL_PAUSED                                 0x1013
#define AL_STOPPED                                0x1014

#define AL_REFERENCE_DISTANCE                     0x1020
#define AL_ROLLOFF_FACTOR                         0x1021
#define AL_CONE_OUTER_GAIN                        0x1022
#define AL_MAX_DISTANCE                           0x1023

#define AL_DISTANCE_MODEL                         0xD000
#define AL_INVERSE_DISTANCE                       0xD001
#define AL_INVERSE_DISTANCE_CLAMPED               0xD002
#define AL_LINEAR_DISTANCE                        0xD003
#define AL_LINEAR_DISTANCE_CLAMPED                0xD004
#define AL_EXPONENT_DISTANCE                      0xD005
#define AL_EXPONENT_DISTANCE_CLAMPED              0xD006

// sound format
#define AL_FORMAT_MONO8                           0x1100
#define AL_FORMAT_MONO16                          0x1101
#define AL_FORMAT_STEREO8                         0x1102
#define AL_FORMAT_STEREO16                        0x1103

// buffer queues
#define AL_BUFFERS_QUEUED                         0x1015
#define AL_BUFFERS_PROCESSED                      0x1016

// source buffer position information
#define AL_SEC_OFFSET                             0x1024
#define AL_SAMPLE_OFFSET                          0x1025
#define AL_BYTE_OFFSET                            0x1026

// openal errors
#define AL_NO_ERROR                               0
#define AL_INVALID_NAME                           0xA001
#define AL_INVALID_ENUM                           0xA002
#define AL_INVALID_VALUE                          0xA003
#define AL_INVALID_OPERATION                      0xA004
#define AL_OUT_OF_MEMORY                          0xA005

// openal effects
#define AL_EFFECT_TYPE			0x8001
#define AL_EFFECT_NULL			0x0000
#define AL_EFFECT_REVERB			0x0001
#define AL_EFFECT_CHORUS			0x0002
#define AL_EFFECT_DISTORTION			0x0003
#define AL_EFFECT_ECHO			0x0004
#define AL_EFFECT_FLANGER			0x0005
#define AL_EFFECT_FREQUENCY_SHIFTER		0x0006
#define AL_EFFECT_VOCAL_MORPHER		0x0007
#define AL_EFFECT_PITCH_SHIFTER		0x0008
#define AL_EFFECT_RING_MODULATOR		0x0009
#define AL_EFFECT_AUTOWAH			0x000A
#define AL_EFFECT_COMPRESSOR			0x000B
#define AL_EFFECT_EQUALIZER			0x000C
#define AL_EFFECT_EAXREVERB			0x8000

// openal filters
#define AL_FILTER_TYPE			0x8001
#define AL_FILTER_NULL			0x0000
#define AL_FILTER_LOWPASS			0x0001
#define AL_FILTER_HIGHPASS			0x0002
#define AL_FILTER_BANDPASS			0x0003

enum
{
	EAX_ENVIRONMENT_GENERIC,
	EAX_ENVIRONMENT_PADDEDCELL,
	EAX_ENVIRONMENT_ROOM,
	EAX_ENVIRONMENT_BATHROOM,
	EAX_ENVIRONMENT_LIVINGROOM,
	EAX_ENVIRONMENT_STONEROOM,
	EAX_ENVIRONMENT_AUDITORIUM,
	EAX_ENVIRONMENT_CONCERTHALL,
	EAX_ENVIRONMENT_CAVE,
	EAX_ENVIRONMENT_ARENA,
	EAX_ENVIRONMENT_HANGAR,
	EAX_ENVIRONMENT_CARPETEDHALLWAY,
	EAX_ENVIRONMENT_HALLWAY,
	EAX_ENVIRONMENT_STONECORRIDOR,
	EAX_ENVIRONMENT_ALLEY,
	EAX_ENVIRONMENT_FOREST,
	EAX_ENVIRONMENT_CITY,
	EAX_ENVIRONMENT_MOUNTAINS,
	EAX_ENVIRONMENT_QUARRY,
	EAX_ENVIRONMENT_PLAIN,
	EAX_ENVIRONMENT_PARKINGLOT,
	EAX_ENVIRONMENT_SEWERPIPE,
	EAX_ENVIRONMENT_UNDERWATER,
	EAX_ENVIRONMENT_DRUGGED,
	EAX_ENVIRONMENT_DIZZY,
	EAX_ENVIRONMENT_PSYCHOTIC,

	EAX_ENVIRONMENT_COUNT
};

typedef enum
{
	DSPROPERTY_EAXLISTENER_NONE,
	DSPROPERTY_EAXLISTENER_ALLPARAMETERS,
	DSPROPERTY_EAXLISTENER_ROOM,
	DSPROPERTY_EAXLISTENER_ROOMHF,
	DSPROPERTY_EAXLISTENER_ROOMROLLOFFFACTOR,
	DSPROPERTY_EAXLISTENER_DECAYTIME,
	DSPROPERTY_EAXLISTENER_DECAYHFRATIO,
	DSPROPERTY_EAXLISTENER_REFLECTIONS,
	DSPROPERTY_EAXLISTENER_REFLECTIONSDELAY,
	DSPROPERTY_EAXLISTENER_REVERB,
	DSPROPERTY_EAXLISTENER_REVERBDELAY,
	DSPROPERTY_EAXLISTENER_ENVIRONMENT,
	DSPROPERTY_EAXLISTENER_ENVIRONMENTSIZE,
	DSPROPERTY_EAXLISTENER_ENVIRONMENTDIFFUSION,
	DSPROPERTY_EAXLISTENER_AIRABSORPTIONHF,

	DSPROPERTY_EAXLISTENER_FLAGS
} DSPROPERTY_EAX_LISTENERPROPERTY;

// or these flags with property id
#define DSPROPERTY_EAXLISTENER_IMMEDIATE 0x00000000 // changes take effect immediately
#define DSPROPERTY_EAXLISTENER_DEFERRED  0x80000000 // changes take effect later

// openal32.dll exports
aldevice *(_cdecl *palcOpenDevice)( const char *devicename );
char (_cdecl *palcCloseDevice)( aldevice *device );
alcontext *(_cdecl *palcCreateContext)( aldevice *device, const int *attrlist );
void (_cdecl *palcDestroyContext)( alcontext *context );
char (_cdecl *palcMakeContextCurrent)( alcontext *context );
void (_cdecl *palcProcessContext)( alcontext *context );
void (_cdecl *palcSuspendContext)( alcontext *context );
alcontext *(_cdecl *palcGetCurrentContext)( void );
aldevice *(_cdecl *palcGetContextsDevice)( alcontext *context );
const char *(_cdecl *palcGetString )( aldevice *device, int param );
void (_cdecl *palcGetIntegerv)( aldevice *device, int param, int size, int *dest );
int (_cdecl *palcGetError)( aldevice *device );
char (_cdecl *palcIsExtensionPresent)( aldevice *device, const char *extname );
void *(_cdecl *palcGetProcAddress)( aldevice *device, const char *function );
int (_cdecl *palcGetEnumValue)( aldevice *device, const char *enumname );

void (_cdecl *palBufferData)( uint bid, int format, const void* data, int size, int freq );
void (_cdecl *palDeleteBuffers)( int n, const uint* buffers );
void (_cdecl *palDeleteSources)( int n, const uint* sources );
void (_cdecl *palDisable)( int capability );
void (_cdecl *palDistanceModel)( int distanceModel );
void (_cdecl *palDopplerFactor)( float value );
void (_cdecl *palDopplerVelocity)( float value );
void (_cdecl *palEnable)( int capability );
void (_cdecl *palGenBuffers)( int n, uint* buffers );
void (_cdecl *palGenSources)( int n, uint* sources );
char (_cdecl *palGetBoolean)( int param );
void (_cdecl *palGetBooleanv)( int param, char* data );
void (_cdecl *palGetBufferf)( uint bid, int param, float* value );
void (_cdecl *palGetBufferi)( uint bid, int param, int* value );
double (_cdecl *palGetDouble)( int param );
void (_cdecl *palGetDoublev)( int param, double* data );
int (_cdecl *palGetEnumValue)( const char* ename );
int (_cdecl *palGetError)( void );
float (_cdecl *palGetFloat)( int param );
void (_cdecl *palGetFloatv)( int param, float* data );
int (_cdecl *palGetInteger)( int param );
void (_cdecl *palGetIntegerv)( int param, int* data );
void (_cdecl *palGetListener3f)( int param, float *value1, float *value2, float *value3 );
void (_cdecl *palGetListenerf)( int param, float* value );
void (_cdecl *palGetListenerfv)( int param, float* values );
void (_cdecl *palGetListeneri)( int param, int* value );
void *(_cdecl *palGetProcAddress)( const char* fname );
void (_cdecl *palGetSource3f)( uint sid, int param, float* value1, float* value2, float* value3 );
void (_cdecl *palGetSourcef)( uint sid, int param, float* value );
void (_cdecl *palGetSourcefv)( uint sid, int param, float* values );
void (_cdecl *palGetSourcei)( uint sid, int param, int* value );
const char *(_cdecl *palGetString)( int param );
char (_cdecl *palIsBuffer)( uint bid );
char (_cdecl *palIsEnabled)( int capability ); 
char (_cdecl *palIsExtensionPresent)(const char* extname );
char (_cdecl *palIsSource)( uint sid );
void (_cdecl *palListener3f)( int param, float value1, float value2, float value3 );
void (_cdecl *palListenerf)( int param, float value );
void (_cdecl *palListenerfv)( int param, const float* values );
void (_cdecl *palListeneri)( int param, int value );
void (_cdecl *palSource3f)( uint sid, int param, float value1, float value2, float value3 );
void (_cdecl *palSourcef)( uint sid, int param, float value ); 
void (_cdecl *palSourcefv)( uint sid, int param, const float* values );
void (_cdecl *palSourcei)( uint sid, int param, int value);
void (_cdecl *palSourcePause)( uint sid );
void (_cdecl *palSourcePausev)( int ns, const uint *sids );
void (_cdecl *palSourcePlay)( uint sid );
void (_cdecl *palSourcePlayv)( int ns, const uint *sids );
void (_cdecl *palSourceQueueBuffers)(uint sid, int numEntries, const uint *bids );
void (_cdecl *palSourceRewind)( uint sid );
void (_cdecl *palSourceRewindv)( int ns, const uint *sids );
void (_cdecl *palSourceStop)( uint sid );
void (_cdecl *palSourceStopv)( int ns, const uint *sids );
void (_cdecl *palSourceUnqueueBuffers)(uint sid, int numEntries, uint *bids );

// openal32.dll internal exports which can be get with by algetProcAddress
void (*alGenEffects)( int n, uint* effects );
void (*alDeleteEffects)( int n, uint* effects );
char (*alIsEffect)( uint eid );
void (*alEffecti)( uint eid, int param, int value);
void (*alEffectiv)( uint eid, int param, int* values );
void (*alEffectf)( uint eid, int param, float value);
void (*alEffectfv)( uint eid, int param, float* values );
void (*alGetEffecti)( uint eid, int pname, int* value );
void (*alGetEffectiv)( uint eid, int pname, int* values );
void (*alGetEffectf)( uint eid, int pname, float* value );
void (*alGetEffectfv)( uint eid, int pname, float* values );
void (*alGenFilters)( int n, uint* filters );
void (*alDeleteFilters)( int n, uint* filters );
char (*alIsFilter)( uint fid );
void (*alFilteri)( uint fid, int param, int value );
void (*alFilteriv)( uint fid, int param, int* values );
void (*alFilterf)( uint fid, int param, float value);
void (*alFilterfv)( uint fid, int param, float* values );
void (*alGetFilteri)( uint fid, int pname, int* value );
void (*alGetFilteriv)( uint fid, int pname, int* values );
void (*alGetFilterf)( uint fid, int pname, float* value );
void (*alGetFilterfv)( uint fid, int pname, float* values );
void (*alGenAuxiliaryEffectSlots)( int n, uint* slots );
void (*alDeleteAuxiliaryEffectSlots)( int n, uint* slots );
char (*alIsAuxiliaryEffectSlot)( uint slot );
void (*alAuxiliaryEffectSloti)( uint asid, int param, int value );
void (*alAuxiliaryEffectSlotiv)( uint asid, int param, int* values );
void (*alAuxiliaryEffectSlotf)( uint asid, int param, float value );
void (*alAuxiliaryEffectSlotfv)( uint asid, int param, float* values );
void (*alGetAuxiliaryEffectSloti)( uint asid, int pname, int* value );
void (*alGetAuxiliaryEffectSlotiv)( uint asid, int pname, int* values );
void (*alGetAuxiliaryEffectSlotf)( uint asid, int pname, float* value );
void (*alGetAuxiliaryEffectSlotfv)( uint asid, int pname, float* values );

typedef struct guid_s
{
	dword	Data1;
	word	Data2;
	word	Data3;
	byte	Data4[8];
} guid_t;

// EAX 2.0 extension
int (*alEAXSet)( const guid_t*, uint, uint, void*, uint );
int (*alEAXGet)( const guid_t*, uint, uint, void*, uint );

// I3DL2 OpenAL Extension
int (*I3DL2Get)( const guid_t*, uint, uint, void*, uint );
int (*I3DL2Set)( const guid_t*, uint, uint, void*, uint );

bool S_Init_OpenAL( void );
void S_Free_OpenAL( void );

#endif//S_OPENAL_H