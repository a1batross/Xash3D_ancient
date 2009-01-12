# Microsoft Developer Studio Project File - Name="client" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=client - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "client.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "client.mak" CFG="client - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "client - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "client - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/GoldSrc/client", HGEBAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "client - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\!Release"
# PROP BASE Intermediate_Dir ".\!Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\client\!release"
# PROP Intermediate_Dir "..\temp\client\!release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "./" /I "../common" /I "global" /I "hud" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
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
# ADD LINK32 msvcrt.lib /nologo /subsystem:windows /dll /machine:I386 /nodefaultlib:"libc" /def:".\client.def" /libpath:"..\common\libs"
# SUBTRACT LINK32 /map
# Begin Custom Build
TargetDir=\Xash3D\src_main\temp\client\!release
InputPath=\Xash3D\src_main\temp\client\!release\client.dll
SOURCE="$(InputPath)"

"D:\Xash3D\tmpQuArK\bin\client.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\client.dll "D:\Xash3D\tmpQuArK\bin\client.dll"

# End Custom Build

!ELSEIF  "$(CFG)" == "client - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "client___Win32_Debug"
# PROP BASE Intermediate_Dir "client___Win32_Debug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\client\!debug"
# PROP Intermediate_Dir "..\temp\client\!debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /Zi /O2 /I "..\common\vgui" /I "..\client" /I "..\client\render" /I ".\hud" /I "..\common\engine" /I "..\common" /I "..\server" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "CLIENT_DLL" /YX /FD /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /I "./" /I "../common" /I "global" /I "hud" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib opengl32.lib glu32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib winmm.lib vgui.lib wsock32.lib cvaLib.lib /nologo /subsystem:windows /dll /machine:I386 /nodefaultlib:"libc" /def:".\client.def" /libpath:"..\common\libs"
# SUBTRACT BASE LINK32 /map
# ADD LINK32 msvcrtd.lib /nologo /subsystem:windows /dll /incremental:yes /debug /machine:I386 /nodefaultlib:"libc.lib" /def:".\client.def" /pdbtype:sept /libpath:"..\common\libs"
# SUBTRACT LINK32 /map
# Begin Custom Build
TargetDir=\Xash3D\src_main\temp\client\!debug
InputPath=\Xash3D\src_main\temp\client\!debug\client.dll
SOURCE="$(InputPath)"

"D:\Xash3D\tmpQuArK\bin\client.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\client.dll "D:\Xash3D\tmpQuArK\bin\client.dll"

# End Custom Build

!ENDIF 

# Begin Target

# Name "client - Win32 Release"
# Name "client - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\global\dll_int.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_ammo.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_ammohistory.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_battery.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_death.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_flashlight.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_geiger.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_health.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_icons.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_menu.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_message.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_motd.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_msg.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_saytext.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_scoreboard.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_sound.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_statusbar.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_text.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_train.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_utils.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_warhead.cpp
# End Source File
# Begin Source File

SOURCE=.\hud\hud_zoom.cpp
# End Source File
# Begin Source File

SOURCE=.\global\tempents.cpp
# End Source File
# Begin Source File

SOURCE=.\global\triapi.cpp
# End Source File
# Begin Source File

SOURCE=.\global\view.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\global\enginecallback.h
# End Source File
# Begin Source File

SOURCE=.\global\extdll.h
# End Source File
# Begin Source File

SOURCE=.\hud\hud.h
# End Source File
# Begin Source File

SOURCE=.\hud\hud_ammo.h
# End Source File
# Begin Source File

SOURCE=.\hud\hud_ammohistory.h
# End Source File
# Begin Source File

SOURCE=.\hud\hud_health.h
# End Source File
# Begin Source File

SOURCE=.\hud\hud_iface.h
# End Source File
# Begin Source File

SOURCE=.\global\vector.h
# End Source File
# End Group
# End Target
# End Project
