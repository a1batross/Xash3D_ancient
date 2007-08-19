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
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "server - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir "."
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\server\!release"
# PROP Intermediate_Dir "..\temp\server\!release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir "."
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /W3 /GX /O2 /I ".\\" /I ".\monsters" /I ".\ents" /I ".\global" /I "../public" /I "../public/libs" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib winmm.lib /nologo /subsystem:windows /dll /pdb:none /machine:I386
# SUBTRACT LINK32 /debug
# Begin Custom Build
TargetDir=\XASH3D\src_main\!source\temp\server\!release
InputPath=\XASH3D\src_main\!source\temp\server\!release\server.dll
SOURCE="$(InputPath)"

"D:\Xash3D\xash\bin\server.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\server.dll "D:\Xash3D\xash\bin\server.dll"

# End Custom Build

!ELSEIF  "$(CFG)" == "server - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir "."
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\temp\server\!debug"
# PROP Intermediate_Dir "..\temp\server\!debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir "."
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I ".\\" /I ".\monsters" /I ".\ents" /I ".\global" /I "../public" /I "../public/libs" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib winmm.lib /nologo /subsystem:windows /dll /debug /machine:I386
# SUBTRACT LINK32 /incremental:no /map
# Begin Custom Build
TargetDir=\XASH3D\src_main\!source\temp\server\!debug
InputPath=\XASH3D\src_main\!source\temp\server\!debug\server.dll
SOURCE="$(InputPath)"

"D:\Xash3D\xash\bin\server.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\server.dll "D:\Xash3D\xash\bin\server.dll"

# End Custom Build

!ENDIF 

# Begin Target

# Name "server - Win32 Release"
# Name "server - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\global\g_ai.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_camera.c
# End Source File
# Begin Source File

SOURCE=.\global\g_chase.c
# End Source File
# Begin Source File

SOURCE=.\global\g_cmds.c
# End Source File
# Begin Source File

SOURCE=.\global\g_combat.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_crane.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_fog.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_func.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_items.c
# End Source File
# Begin Source File

SOURCE=.\global\g_jetpack.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_lights.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_lock.c
# End Source File
# Begin Source File

SOURCE=.\global\g_main.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_misc.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_model.c
# End Source File
# Begin Source File

SOURCE=.\monsters\g_monster.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_mtrain.c
# End Source File
# Begin Source File

SOURCE=.\global\g_newai.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_pendulum.c
# End Source File
# Begin Source File

SOURCE=.\global\g_phys.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_reflect.c
# End Source File
# Begin Source File

SOURCE=.\global\g_save.c
# End Source File
# Begin Source File

SOURCE=.\global\g_sound.c
# End Source File
# Begin Source File

SOURCE=.\global\g_spawn.c
# End Source File
# Begin Source File

SOURCE=.\global\g_svcmds.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_target.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_thing.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_tracktrain.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_trigger.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_turret.c
# End Source File
# Begin Source File

SOURCE=.\global\g_utils.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_vehicle.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_weapon.c
# End Source File
# Begin Source File

SOURCE=.\ents\g_wes.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_actor.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_actor_weap.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_berserk.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_boss2.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_boss3.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_boss31.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_boss32.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_brain.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_chick.c
# End Source File
# Begin Source File

SOURCE=.\m_flash.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_flipper.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_float.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_flyer.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_gladiator.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_gunner.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_hover.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_infantry.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_insane.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_medic.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_move.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_mutant.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_parasite.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_soldier.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_supertank.c
# End Source File
# Begin Source File

SOURCE=.\monsters\m_tank.c
# End Source File
# Begin Source File

SOURCE=.\p_chase.c
# End Source File
# Begin Source File

SOURCE=.\p_client.c
# End Source File
# Begin Source File

SOURCE=.\p_hud.c
# End Source File
# Begin Source File

SOURCE=.\p_menu.c
# End Source File
# Begin Source File

SOURCE=.\p_text.c
# End Source File
# Begin Source File

SOURCE=.\p_trail.c
# End Source File
# Begin Source File

SOURCE=.\p_view.c
# End Source File
# Begin Source File

SOURCE=.\p_weapon.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=..\common\const.h
# End Source File
# Begin Source File

SOURCE=.\global\fmod.h
# End Source File
# Begin Source File

SOURCE=.\g_local.h
# End Source File
# Begin Source File

SOURCE=.\game.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_actor.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_berserk.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_boss2.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_boss31.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_boss32.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_brain.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_chick.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_flipper.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_float.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_flyer.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_gladiator.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_gunner.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_hover.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_infantry.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_insane.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_medic.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_mutant.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_parasite.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_player.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_rider.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_soldier.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_supertank.h
# End Source File
# Begin Source File

SOURCE=.\monsters\m_tank.h
# End Source File
# Begin Source File

SOURCE=.\p_menu.h
# End Source File
# Begin Source File

SOURCE=.\p_text.h
# End Source File
# Begin Source File

SOURCE=.\pak.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
