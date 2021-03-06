# Microsoft Developer Studio Project File - Name="engine" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=engine - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "engine.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "engine.mak" CFG="engine - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "engine - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "engine - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "engine - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\engine\!release"
# PROP Intermediate_Dir "..\temp\engine\!release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "./" /I "common" /I "server" /I "client" /I "uimenu" /I "../public" /I "../common" /I "../game_shared" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386 /opt:nowin98
# ADD LINK32 user32.lib msvcrt.lib /nologo /subsystem:windows /dll /pdb:none /machine:I386 /nodefaultlib:"libc.lib" /opt:nowin98
# SUBTRACT LINK32 /debug /nodefaultlib
# Begin Custom Build
TargetDir=\Xash3D\src_main\temp\engine\!release
InputPath=\Xash3D\src_main\temp\engine\!release\engine.dll
SOURCE="$(InputPath)"

"D:\Xash3D\bin\engine.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\engine.dll "D:\Xash3D\bin\engine.dll"

# End Custom Build

!ELSEIF  "$(CFG)" == "engine - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\temp\engine\!debug"
# PROP Intermediate_Dir "..\temp\engine\!debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /I "./" /I "common" /I "server" /I "client" /I "uimenu" /I "../public" /I "../common" /I "../game_shared" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 user32.lib msvcrtd.lib /nologo /subsystem:windows /dll /debug /machine:I386 /nodefaultlib:"msvcrt.lib" /pdbtype:sept
# SUBTRACT LINK32 /incremental:no /map /nodefaultlib
# Begin Custom Build
TargetDir=\Xash3D\src_main\temp\engine\!debug
InputPath=\Xash3D\src_main\temp\engine\!debug\engine.dll
SOURCE="$(InputPath)"

"D:\Xash3D\bin\engine.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\engine.dll "D:\Xash3D\bin\engine.dll"

# End Custom Build

!ENDIF 

# Begin Target

# Name "engine - Win32 Release"
# Name "engine - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=.\build.c
# End Source File
# Begin Source File

SOURCE=.\common\cinematic.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_cmds.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_demo.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_effects.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_frame.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_game.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_main.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_move.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_parse.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_phys.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_scrn.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_tent.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_video.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_view.c
# End Source File
# Begin Source File

SOURCE=.\client\cl_world.c
# End Source File
# Begin Source File

SOURCE=.\common\com_keys.c
# End Source File
# Begin Source File

SOURCE=.\common\com_world.c
# End Source File
# Begin Source File

SOURCE=.\common\con_main.c
# End Source File
# Begin Source File

SOURCE=.\common\con_utils.c
# End Source File
# Begin Source File

SOURCE=.\common\engfuncs.c
# End Source File
# Begin Source File

SOURCE=.\host.c
# End Source File
# Begin Source File

SOURCE=.\common\infostring.c
# End Source File
# Begin Source File

SOURCE=.\common\input.c
# End Source File
# Begin Source File

SOURCE=.\common\net_chan.c
# End Source File
# Begin Source File

SOURCE=.\common\net_huff.c
# End Source File
# Begin Source File

SOURCE=.\common\net_msg.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_client.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_cmds.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_frame.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_game.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_init.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_main.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_move.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_phys.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_save.c
# End Source File
# Begin Source File

SOURCE=.\server\sv_world.c
# End Source File
# Begin Source File

SOURCE=.\common\titles.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_advcontrols.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_audio.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_configuration.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_controls.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_creategame.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_credits.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_customgame.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_gameoptions.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_langame.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_loadgame.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_main.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_menu.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_multiplayer.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_newgame.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_playdemo.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_playersetup.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_playrec.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_qmenu.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_recdemo.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_savegame.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_saveload.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_video.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_vidmodes.c
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_vidoptions.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=.\client\client.h
# End Source File
# Begin Source File

SOURCE=.\common.h
# End Source File
# Begin Source File

SOURCE=.\common\net_msg.h
# End Source File
# Begin Source File

SOURCE=.\common\safeproc.h
# End Source File
# Begin Source File

SOURCE=.\server\server.h
# End Source File
# Begin Source File

SOURCE=.\uimenu\ui_local.h
# End Source File
# End Group
# End Target
# End Project
