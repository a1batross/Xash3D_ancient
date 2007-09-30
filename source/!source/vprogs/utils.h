//=======================================================================
//			Copyright XashXT Group 2007 ©
//			utils.h - vm builtin functions
//=======================================================================
#define MSG_BROADCAST	1 // unreliable to all
#define MSG_PHS		2 // unreliable to potential hearable set
#define MSG_PVS		3 // unreliable to potential visible set
#define MSG_ONE		4 // relaible to one
#define MSG_ALL		5 // relaible to all

// network messaging
void MsgBegin (float dest)			= #1;	// message type (see SVC_ for details)
void WriteByte (float f) 			= #2;	// write unsigned char
void WriteChar (float f) 			= #3;	// write signed char
void WriteShort (float f)			= #4;	// write signed short
void WriteLong (float f) 			= #5;	// write signed long
void WriteFloat (float f) 			= #6;	// write float
void WriteAngle (float f)			= #7;	// wirte angle in range 0 - 360 degrees
void WriteCoord (float f) 			= #8;	// write coord with step 1/8 or 1/32 (see network.txt)
void WriteString (string s) 			= #9;	// write string with random length
void WriteEntity (entity s) 			= #10;	// write entity or tempentity to client
void MsgEnd(float to, vector pos, entity e) 	= #11;	// end of message

// mathlib
float min(float a, float b )			= #12;	// returned min value 
float max(float a, float b )			= #13; 	// returned max value
float bound(float min, float val, float max)	= #14;	// bound value beetwen min and max 
float pow(float a, float b)			= #15;	//  
float sin(float f)				= #16; 
float cos(float f)				= #17; 
float sqrt(float f)				= #18; 
float rint (float v)			= #19; 
float floor(float v)			= #20; 
float ceil (float v)			= #21; 
float fabs (float f)			= #22; 
float random_long( void )			= #23; 
float random_float( void )			= #24; 

// vector mathlib
vector normalize(vector v) 			= #31;	// normalize vector
float veclength(vector v) 			= #32;	// return vector length (scalar)
float vectoyaw(vector v) 			= #33;	// convert vec to yaw
vector vectoangles(vector v) 			= #34;	// convert vec to pitch and yaw, not roll
vector randomvec( void )			= #35;	// return a random vector
void vectorvectors(vector dir)		= #36;	// vectorvectors with global up,right,forward 
void makevectors(vector dir)			= #37;	// make right-handed coord 
void makevectors2(vector dir)			= #38;	// make left-handed coord. probably not used, just in case 

// stdlib functions
float atof(string s)			= #41;	// convert string to float 
string ftoa(float s)			= #42;	// convert float to string 
string vtoa(vector v)			= #43;	// convert vector to string 
vector atov(string s)			= #44;	// convert string to vector 
void Msg( ... )				= #45;	// message into console 
void MsgWarn( ... )				= #46;	// warning message into console 
void Error( ... )				= #47;	// error message and abort current level 
void bprint( ... )				= #48;	// broadcast printing 
void sprint(entity client, string s)		= #49;	// send text message for current client 
void centerprint(entity client, string s)	= #50;	// printing into center of screen for current client  
float cvar(string s)			= #51;	// get cvar value (it will be create if not exist)
void cvar_set(string var, string val)		= #52;	// set new cvar value (it will be create if not exist)
string AllocString(string s)			= #53;	// same as ALLOC_STRING in Half-Life, probably not used 
void FreeString(string s)			= #54;	// freeing alloced string
float strlen(string s)			= #55;	// return symbol count for current string 
string strcat(string s1, string s2)		= #56;	// append "b" string at end string "a" 
string argv( float parm )			= #57;	// return current cmd args

// internal debugger
void break( void )				= #61;	// abort current level
void crash( void )				= #62;	// invoke crash
void coredump( void )			= #63;	// dump prvm core info 
void stackdump( void )			= #64;	// dump contain of localstack  
void trace_on( void )			= #65;	// enable PRVM trace 
void trace_off( void )			= #66;	// disable PRVM trace 
void dump_edict(entity e)			= #67;	// dump info about current edict
entity nextent(entity e)			= #68;	// move to next edict in chain

// engine functions (like half-life enginefuncs_s)
float precache_model(string s)				= #71; 
float precache_sound(string s)				= #72; 
float setmodel(entity e, string m)				= #73;
float model_index(string s)					= #74;
float decal_index(string s)					= #75; 
float model_frames(float modelindex)				= #76; 
void setsize(entity e, vector min, vector max)			= #77; 
void changelevel(string mapname, string spotname) 		= #78;
void ChangeYaw( void )					= #79; 
void ChangePitch( void )					= #80; 
entity find(entity st, .string fld, string key)			= #81; 
float getEntityIllum( entity e )				= #82; 
entity FindInSphere(vector org, float rad)			= #83; 
float InPVS( vector v1, vector v2 )				= #84; 				
float InPHS( vector v1, vector v2 )				= #85; 
entity create( string class, string model, vector org )		= #86; 
void remove( entity e )					= #87; 
float droptofloor( void )					= #88; 
float walkmove(float yaw, float dist)				= #89; 
void setorigin(entity e, vector org)				= #90; 
void sound(entity e, float chan, string samp, float vol, float attn)	= #91; 
void ambientsound(entity e, string samp)			= #92; 
void traceline(vector v1, vector v2, float mask, entity ignore)	= #93; 
void tracetoss (entity e, entity ignore)			= #94; 
void tracebox (vector v1, vector mins, vector maxs, vector v2, float mask, entity ignore) = #95 
float checkbottom(entity e)					= #96;
void lightstyle(float style, string value)			= #97;
float pointcontents(vector v)					= #98;
vector aim(entity e, float speed)				= #99; 
void server_command( string command )				= #100; 
void client_command( entity e, string s)			= #101; 
void particle(vector o, vector d, float color, float count)		= #102; 
void areaportal_state( float num, float state )			= #103; 
void setstats(entity e, float f, string stats)			= #104; 
void configstring(float num, string s)				= #105;
void makestatic(entity e)					= #106;
string precache_pic(string pic)				= #107;
 