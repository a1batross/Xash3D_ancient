# Microsoft Developer Studio Project File - Name="server" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=server - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "server.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "server.mak" CFG="server - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "server - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "server - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/GoldSrc/server", ELEBAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "server - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\server\!release"
# PROP Intermediate_Dir "..\temp\server\!release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /c
# ADD CPP /nologo /MD /W3 /O2 /I "./" /I "ents" /I "game" /I "global" /I "monsters" /I "../common" /I "../pm_shared" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
# SUBTRACT CPP /Fr /YX
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 msvcrt.lib /nologo /subsystem:windows /dll /pdb:none /machine:I386 /nodefaultlib:"libc.lib" /def:".\server.def" /libpath:"..\common\libs"
# SUBTRACT LINK32 /profile /map /debug /nodefaultlib
# Begin Custom Build - Performing Custom Build Step on $(InputPath)
TargetDir=\Xash3D\src_main\temp\server\!release
InputPath=\Xash3D\src_main\temp\server\!release\server.dll
SOURCE="$(InputPath)"

"D:\Xash3D\bin\server.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\server.dll "D:\Xash3D\bin\server.dll"

# End Custom Build

!ELSEIF  "$(CFG)" == "server - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "server___Win32_Debug"
# PROP BASE Intermediate_Dir "server___Win32_Debug"
# PROP BASE Ignore_Export_Lib 1
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\server\!debug"
# PROP Intermediate_Dir "..\temp\server\!debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G5 /MT /W3 /O2 /I "..\server" /I "..\common\engine" /I "..\common" /I "..\server\ents" /I "..\server\global" /I "..\server\weapons" /I "..\server\game" /I "..\server\monsters" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /I "./" /I "ents" /I "game" /I "global" /I "monsters" /I "../common" /I "../pm_shared" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /FD /c
# SUBTRACT CPP /u /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /dll /incremental:yes /machine:I386 /nodefaultlib:"libc" /def:".\server.def" /libpath:"..\common\libs"
# SUBTRACT BASE LINK32 /profile /map /debug
# ADD LINK32 msvcrtd.lib /nologo /subsystem:windows /dll /incremental:yes /debug /machine:I386 /nodefaultlib:"libc.lib" /def:".\server.def" /pdbtype:sept
# SUBTRACT LINK32 /profile /map
# Begin Custom Build - Performing Custom Build Step on $(InputPath)
TargetDir=\Xash3D\src_main\temp\server\!debug
InputPath=\Xash3D\src_main\temp\server\!debug\server.dll
SOURCE="$(InputPath)"

"D:\Xash3D\bin\server.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\server.dll "D:\Xash3D\bin\server.dll"

# End Custom Build

!ENDIF 

# Begin Target

# Name "server - Win32 Release"
# Name "server - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def"
# Begin Source File

SOURCE=.\monsters\ai_sound.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\animating.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\animation.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\apache.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\barnacle.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\barney.cpp
# End Source File
# Begin Source File

SOURCE=.\ents\basebrush.cpp
# End Source File
# Begin Source File

SOURCE=.\ents\baseentity.cpp
# End Source File
# Begin Source File

SOURCE=.\ents\basefunc.cpp
# End Source File
# Begin Source File

SOURCE=.\ents\basefx.cpp
# End Source File
# Begin Source File

SOURCE=.\ents\baseinfo.cpp
# End Source File
# Begin Source File

SOURCE=.\ents\baseitem.cpp
# End Source File
# Begin Source File

SOURCE=.\ents\baselogic.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\basemonster.cpp
# End Source File
# Begin Source File

SOURCE=.\ents\basemover.cpp
# End Source File
# Begin Source File

SOURCE=.\ents\baseother.cpp
# End Source File
# Begin Source File

SOURCE=.\ents\basepath.cpp
# End Source File
# Begin Source File

SOURCE=.\ents\basephys.cpp
# End Source File
# Begin Source File

SOURCE=.\ents\baserockets.cpp
# End Source File
# Begin Source File

SOURCE=.\ents\basetank.cpp
# End Source File
# Begin Source File

SOURCE=.\ents\basetrigger.cpp
# End Source File
# Begin Source File

SOURCE=.\ents\baseutil.cpp
# End Source File
# Begin Source File

SOURCE=.\ents\baseweapon.cpp
# End Source File
# Begin Source File

SOURCE=.\ents\baseworld.cpp
# End Source File
# Begin Source File

SOURCE=.\global\client.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\combat.cpp
# End Source File
# Begin Source File

SOURCE=.\global\decals.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\defaultai.cpp
# End Source File
# Begin Source File

SOURCE=.\global\dll_int.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\flyingmonster.cpp
# End Source File
# Begin Source File

SOURCE=.\game\game.cpp
# End Source File
# Begin Source File

SOURCE=.\game\gamerules.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\generic.cpp
# End Source File
# Begin Source File

SOURCE=.\global\globals.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\gman.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\hassassin.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\headcrab.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\hgrunt.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\leech.cpp
# End Source File
# Begin Source File

SOURCE=.\game\lights.cpp
# End Source File
# Begin Source File

SOURCE=.\game\multiplay_gamerules.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\nodes.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\osprey.cpp
# End Source File
# Begin Source File

SOURCE=.\global\parent.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\player.cpp
# End Source File
# Begin Source File

SOURCE=..\pm_shared\pm_shared.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\rat.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\roach.cpp
# End Source File
# Begin Source File

SOURCE=.\global\saverestore.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\scientist.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\scripted.cpp
# End Source File
# Begin Source File

SOURCE=.\global\sfx.cpp
# End Source File
# Begin Source File

SOURCE=.\game\singleplay_gamerules.cpp
# End Source File
# Begin Source File

SOURCE=.\game\sound.cpp
# End Source File
# Begin Source File

SOURCE=.\global\spectator.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\squadmonster.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\talkmonster.cpp
# End Source File
# Begin Source File

SOURCE=.\game\teamplay_gamerules.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\turret.cpp
# End Source File
# Begin Source File

SOURCE=.\global\utils.cpp
# End Source File
# Begin Source File

SOURCE=.\weapon_generic.cpp
# End Source File
# Begin Source File

SOURCE=.\monsters\zombie.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp"
# Begin Source File

SOURCE=.\monsters\activity.h
# End Source File
# Begin Source File

SOURCE=.\monsters\activitymap.h
# End Source File
# Begin Source File

SOURCE=.\monsters\animation.h
# End Source File
# Begin Source File

SOURCE=.\monsters\baseanimating.h
# End Source File
# Begin Source File

SOURCE=.\ents\basebeams.h
# End Source File
# Begin Source File

SOURCE=.\ents\basebrush.h
# End Source File
# Begin Source File

SOURCE=.\ents\baseentity.h
# End Source File
# Begin Source File

SOURCE=.\ents\baseinfo.h
# End Source File
# Begin Source File

SOURCE=.\ents\baseitem.h
# End Source File
# Begin Source File

SOURCE=.\ents\baselogic.h
# End Source File
# Begin Source File

SOURCE=.\monsters\basemonster.h
# End Source File
# Begin Source File

SOURCE=.\ents\basemover.h
# End Source File
# Begin Source File

SOURCE=.\ents\baserockets.h
# End Source File
# Begin Source File

SOURCE=.\ents\basesprite.h
# End Source File
# Begin Source File

SOURCE=.\ents\basetank.h
# End Source File
# Begin Source File

SOURCE=.\ents\baseweapon.h
# End Source File
# Begin Source File

SOURCE=.\ents\baseworld.h
# End Source File
# Begin Source File

SOURCE=.\cbase.h
# End Source File
# Begin Source File

SOURCE=.\global\cdll_dll.h
# End Source File
# Begin Source File

SOURCE=.\global\client.h
# End Source File
# Begin Source File

SOURCE=.\monsters\damage.h
# End Source File
# Begin Source File

SOURCE=.\global\decals.h
# End Source File
# Begin Source File

SOURCE=.\monsters\defaultai.h
# End Source File
# Begin Source File

SOURCE=.\global\defaults.h
# End Source File
# Begin Source File

SOURCE=.\global\enginecallback.h
# End Source File
# Begin Source File

SOURCE=.\global\extdll.h
# End Source File
# Begin Source File

SOURCE=.\monsters\flyingmonster.h
# End Source File
# Begin Source File

SOURCE=.\game\game.h
# End Source File
# Begin Source File

SOURCE=.\game\gamerules.h
# End Source File
# Begin Source File

SOURCE=.\global\globals.h
# End Source File
# Begin Source File

SOURCE=.\global\hierarchy.h
# End Source File
# Begin Source File

SOURCE=.\monsters\monsterevent.h
# End Source File
# Begin Source File

SOURCE=.\monsters\monsters.h
# End Source File
# Begin Source File

SOURCE=.\monsters\nodes.h
# End Source File
# Begin Source File

SOURCE=.\global\plane.h
# End Source File
# Begin Source File

SOURCE=.\monsters\player.h
# End Source File
# Begin Source File

SOURCE=.\global\saverestore.h
# End Source File
# Begin Source File

SOURCE=.\monsters\schedule.h
# End Source File
# Begin Source File

SOURCE=.\monsters\scripted.h
# End Source File
# Begin Source File

SOURCE=.\monsters\scriptevent.h
# End Source File
# Begin Source File

SOURCE=.\global\sfx.h
# End Source File
# Begin Source File

SOURCE=.\global\shake.h
# End Source File
# Begin Source File

SOURCE=.\global\soundent.h
# End Source File
# Begin Source File

SOURCE=.\game\spectator.h
# End Source File
# Begin Source File

SOURCE=.\monsters\squad.h
# End Source File
# Begin Source File

SOURCE=.\monsters\squadmonster.h
# End Source File
# Begin Source File

SOURCE=.\monsters\talkmonster.h
# End Source File
# Begin Source File

SOURCE=.\game\teamplay_gamerules.h
# End Source File
# Begin Source File

SOURCE=.\global\utils.h
# End Source File
# Begin Source File

SOURCE=.\global\vector.h
# End Source File
# End Group
# End Target
# End Project
