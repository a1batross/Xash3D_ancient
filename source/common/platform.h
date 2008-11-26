//=======================================================================
//			Copyright XashXT Group 2007 ©
//			platform.h - game platform dll
//=======================================================================
#ifndef BASEPLATFORM_H
#define BASEPLATFORM_H

#include <windows.h>
#include "launch_api.h"
#include "qfiles_ref.h"
#include "vprogs_api.h"

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
void Skin_FinalizeScript( void );
void Conv_RunSearch( void );

// shared tools
void ClrMask( void );
void AddMask( const char *mask );
extern string searchmask[];
extern int num_searchmask;

#endif//BASEPLATFORM_H