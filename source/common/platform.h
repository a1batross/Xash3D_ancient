//=======================================================================
//			Copyright XashXT Group 2007 ©
//			platform.h - game platform dll
//=======================================================================
#ifndef BASEPLATFORM_H
#define BASEPLATFORM_H

#include <stdio.h>
#include <setjmp.h>
#include <windows.h>
#include "basetypes.h"
#include "ref_dllapi.h"

//=====================================
//	platform export
//=====================================

void InitPlatform ( int argc, char **argv ); // init host
void RunPlatform ( void ); // host frame
void ClosePlatform ( void ); // close host

#endif//BASEPLATFORM_H