//=======================================================================
//			Copyright XashXT Group 2007 ©
//		      ref_launcher.h - launcher dll header
//=======================================================================
#ifndef REF_LAUNCHER_H
#define REF_LAUNCHER_H

#define LAUNCHER_VERSION		0.1
#define EDITOR_VERSION		1

//app name
#define DEFAULT			0         // host_init( funcname *arg ) same much as:
#define HOST_SHARED			1	// "host_shared"
#define HOST_DEDICATED		2	// "host_dedicated"
#define HOST_EDITOR			3	// "host_editor"
#define BSPLIB			4	// "bsplib"
#define SPRITE			5	// "sprite"
#define STUDIO			6	// "studio"
#define QCCLIB			7	// "qcclib"
#define CREDITS			8

//main core interface
typedef struct host_api_s
{
	//xash engine interface
	void ( *host_init ) ( char *funcname, int argc, char **argv ); //init host
	void ( *host_main ) ( void );	//host frame
	void ( *host_free ) ( void );	//close host

}host_api_t;

//engine.dll handle
typedef host_api_t (*host_t)(system_api_t);

//generic editor interface
typedef struct edit_api_s
{
	//xash editor interface
	void ( *editor_init ) ( char *funcname, int argc, char **argv ); //init editor
	void ( *editor_main ) ( void ); //editor main cycle
	void ( *editor_free ) ( void ); //close editor

}edit_api_t;

//editor.dll handle
typedef edit_api_t (*editor_t)(system_api_t);

#endif//REF_LAUNCHER_H