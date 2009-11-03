//=======================================================================
//			Copyright XashXT Group 2007 ©
//			xtools.h - Xash Game Tools
//=======================================================================
#ifndef XTOOLS_H
#define XTOOLS_H

#include <windows.h>
#include "launch_api.h"

//=====================================
//	platform export
//=====================================

void InitPlatform ( int argc, char **argv ); // init host
void RunPlatform ( void ); // host frame
void ClosePlatform ( void ); // close host

//=====================================
//	extragen export
//=====================================
bool ConvertResource( byte *mempool, const char *filename, byte parms );
void Bsp_PrintLog( const char *pMsg );
void Skin_FinalizeScript( void );
void Bsp_Shutdown( void );
void Conv_RunSearch( void );

// shared tools
void ClrMask( void );
void AddMask( const char *mask );
extern string searchmask[];
extern int num_searchmask;

#endif//XTOOLS_H