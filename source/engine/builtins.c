// base events
void Msg( ... )			// send message into console
void MsgWarn( ... )			// send warning message into console
void Error( ... )			// aborting current level with error message
void Exit( void )			// quit from game ( potentially unsafe built-in )
string Argv( float num )		// returns parm from server or client command
float Argc( void )			// returns num params from server or client command

// common functions
void COM_Trace( float state )		// enable or disable PRVM trace
float COM_FileExists( string filename ) // check file for existing
float COM_GetFileSize( string filename )// get filesize
float COM_GetFileTime( string filename )// get file time
float COM_LoadScript( string filename )	// loading script file
void COM_ResetScript( void )		// reset script to can parse it again
string COM_ReadToken( float newline )	// read next token from scriptfile
float COM_FilterToken( string mask, string s, float casecmp )	// filter current token by specified mask
string COM_Token( void )		// returns current token
float COM_Search( string mask, float casecmp )			// search files by mask
void COM_FreeSearch( float handle )	// release current search
float COM_SearchNumFiles( float handle )// returns number of find files
string COM_SearchFilename( float handle, float num )		// get filename from search list
float COM_RandomLong( float min, float max )			// returns random value in specified range
float COM_RandomFloat( float min, float max )			// returns random value in specified range
void COM_DumpEdict( entity e )	// print info about current edict

// console variables
void Cvar_Register( string name, string value, float flags )	// register new cvar variable 
void Cvar_SetValue( string name, string value )			// set new cvar value
float Cvar_GetValue( string name )				// get cvar value
string Cvar_GetString( string name )				// get cvar string

// filesystem ( uses Xash Virtual filesystem )
float fopen( string filename, float mode )			// open file for read or write
void fclose( float fhandle )		// close current file
string fgets( float fhandle )		// get string from file
entity fgete( float fhandle )		// get entity from file	// same as parse edict
void fputs( float fhandle, string s )	// put string into file
void fpute( float fhandle, entity e )	// put entity into file

// mathlib
float min(float a, float b )
float max(float a, float b )
float bound(float min, float val, float max)
float pow (float x, float y)
float sin (float f)
float cos (float f)
float tan (float f)
float sqrt (float f)
float rint (float v)
float floor(float v)
float ceil (float v)
float fabs (float f)
float fmod (float f)

// veclib
vector normalize(vector v) 
float veclength(vector v) 
float vectoyaw(vector v) 
vector vectoangles(vector v) 
vector randomvec( void )
void vectorvectors(vector dir)
void makevectors(vector dir)
void makevectors2(vector dir)

// entity operations (with compiler support don't rename them)
entity spawn( void )
void remove( entity e )
entity nextent( entity e )	

// stdlib
float atof(string s)	// all other conversion VMach can make automatically
string ftoa(float f)
string vtoa(vector v)
vector atov(string s)
float strlen(string s)		// returns string length
string strcat(string s1, string s2)	// put one string at end of another
string va( format, ... )		// var_args format
string timestamp( float fmt )		// returns timestamp