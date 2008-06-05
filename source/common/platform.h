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
#include "stdapi.h"
#include "stdref.h"
#include "basefiles.h"
#include "dllapi.h"

//=====================================
//	platform export
//=====================================

void InitPlatform ( uint funcname, int argc, char **argv ); // init host
void RunPlatform ( void ); // host frame
void ClosePlatform ( void ); // close host

#endif//BASEPLATFORM_H