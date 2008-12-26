/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
/*

===== globals.cpp ========================================================

  DLL-wide global variable definitions.
  They're all defined here, for convenient centralization.
  Source files that need them should "extern ..." declare each
  variable, to better document what globals they care about.

*/

#include "extdll.h"
#include "utils.h"
#include "cbase.h"
#include "baseweapon.h"
#include "soundent.h"

DLL_GLOBAL ULONG		g_ulFrameCount;
DLL_GLOBAL ULONG		g_ulModelIndexEyes;
DLL_GLOBAL ULONG		g_ulModelIndexPlayer;
DLL_GLOBAL Vector		g_vecAttackDir;
DLL_GLOBAL int		g_iSkillLevel;
DLL_GLOBAL int		gDisplayTitle;
DLL_GLOBAL BOOL		g_fGameOver;
DLL_GLOBAL BOOL		g_startSuit;
DLL_GLOBAL const Vector	g_vecZero = Vector(0,0,0);
DLL_GLOBAL int		g_Language;
DLL_GLOBAL short		g_sModelIndexLaser;
DLL_GLOBAL int    		g_sModelIndexLaserDot;
DLL_GLOBAL short   		g_sModelIndexFireball;
DLL_GLOBAL short   		g_sModelIndexSmoke;
DLL_GLOBAL short   		g_sModelIndexWExplosion;
DLL_GLOBAL short		g_sModelIndexBubbles;
DLL_GLOBAL short		g_sModelIndexBloodDrop;
DLL_GLOBAL short		g_sModelIndexBloodSpray;
DLL_GLOBAL int		g_sModelIndexErrorSprite;
DLL_GLOBAL int		g_sModelIndexErrorModel;
DLL_GLOBAL int		g_sModelIndexNullModel;
DLL_GLOBAL int		g_sModelIndexNullSprite;

int GetStdLightStyle (int iStyle)
{
	switch (iStyle)
	{
	case 0: return MAKE_STRING("m"); // 0 normal
	case 1: return MAKE_STRING("mmnmmommommnonmmonqnmmo"); // 1 FLICKER (first variety)
	case 2: return MAKE_STRING("abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba"); // 2 SLOW STRONG PULSE
	case 3: return MAKE_STRING("mmmmmaaaaammmmmaaaaaabcdefgabcdefg"); // 3 CANDLE (first variety)
	case 4: return MAKE_STRING("mamamamamama"); // 4 FAST STROBE
	case 5: return MAKE_STRING("jklmnopqrstuvwxyzyxwvutsrqponmlkj"); // 5 GENTLE PULSE 1
	case 6: return MAKE_STRING("nmonqnmomnmomomno"); // 6 FLICKER (second variety)
	case 7: return MAKE_STRING("mmmaaaabcdefgmmmmaaaammmaamm"); // 7 CANDLE (second variety)
	case 8: return MAKE_STRING("mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa"); // 8 CANDLE (third variety)
	case 9: return MAKE_STRING("aaaaaaaazzzzzzzz"); // 9 SLOW STROBE (fourth variety)
	case 10: return MAKE_STRING("mmamammmmammamamaaamammma"); // 10 FLUORESCENT FLICKER
	case 11: return MAKE_STRING("abcdefghijklmnopqrrqponmlkjihgfedcba"); // 11 SLOW PULSE NOT FADE TO BLACK
	case 12: return MAKE_STRING("mmnnmmnnnmmnn");  // 12 UNDERWATER LIGHT MUTATION
	case 13: return MAKE_STRING("a"); // 13 OFF
	case 14: return MAKE_STRING("aabbccddeeffgghhiijjkkllmmmmmmmmmmmmmm"); // 14 SLOW FADE IN
	case 15: return MAKE_STRING("abcdefghijklmmmmmmmmmmmmmmmmmmmmmmmmmm"); // 15 MED FADE IN 
	case 16: return MAKE_STRING("acegikmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm"); // 16 FAST FADE IN
	case 17: return MAKE_STRING("llkkjjiihhggffeeddccbbaaaaaaaaaaaaaaaa"); // 17 SLOW FADE OUT
	case 18: return MAKE_STRING("lkjihgfedcbaaaaaaaaaaaaaaaaaaaaaaaaaaa"); // 18 MED FADE OUT
	case 19: return MAKE_STRING("kigecaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"); // 19 FAST FADE OUT
	default: return MAKE_STRING("m");
	}
}