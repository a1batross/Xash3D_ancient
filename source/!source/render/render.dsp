# Microsoft Developer Studio Project File - Name="render" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=render - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "render.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "render.mak" CFG="render - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "render - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "render - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "render - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir "."
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\render\!release"
# PROP Intermediate_Dir "..\temp\render\!release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir "."
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MD /W3 /GX /O2 /Ob0 /I "../public" /I "../platform/formats" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
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
# ADD LINK32 msvcrt.lib user32.lib gdi32.lib /nologo /subsystem:windows /dll /pdb:none /machine:I386 /nodefaultlib:"libc.lib" /libpath:"../public/libs/"
# SUBTRACT LINK32 /debug
# Begin Custom Build
TargetDir=\XASH3D\src_main\!source\temp\render\!release
InputPath=\XASH3D\src_main\!source\temp\render\!release\render.dll
SOURCE="$(InputPath)"

"D:\Xash3D\bin\render.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\render.dll "D:\Xash3D\bin\render.dll"

# End Custom Build

!ELSEIF  "$(CFG)" == "render - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir "."
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\temp\render\!debug"
# PROP Intermediate_Dir "..\temp\render\!debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir "."
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /I "../public" /I "../platform/formats" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /FD /c
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
# ADD LINK32 msvcrt.lib user32.lib gdi32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /nodefaultlib:"msvcrtd.lib" /pdbtype:sept /libpath:"../public/libs/"
# SUBTRACT LINK32 /profile /incremental:no /map
# Begin Custom Build
TargetDir=\XASH3D\src_main\!source\temp\render\!debug
InputPath=\XASH3D\src_main\!source\temp\render\!debug\render.dll
SOURCE="$(InputPath)"

"D:\Xash3D\bin\render.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\render.dll "D:\Xash3D\bin\render.dll"

# End Custom Build

!ENDIF 

# Begin Target

# Name "render - Win32 Release"
# Name "render - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\gl_draw.c
# End Source File
# Begin Source File

SOURCE=.\gl_light.c
# End Source File
# Begin Source File

SOURCE=.\gl_model.c
# End Source File
# Begin Source File

SOURCE=.\gl_rmain.c
# End Source File
# Begin Source File

SOURCE=.\gl_rmisc.c
# End Source File
# Begin Source File

SOURCE=.\gl_rsurf.c
# End Source File
# Begin Source File

SOURCE=.\gl_warp.c
# End Source File
# Begin Source File

SOURCE=.\gl_widnt.c
# End Source File
# Begin Source File

SOURCE=.\glw_imp.c
# End Source File
# Begin Source File

SOURCE=.\r_backend.c
# End Source File
# Begin Source File

SOURCE=.\r_bloom.c
# End Source File
# Begin Source File

SOURCE=.\r_bspmod.c
# End Source File
# Begin Source File

SOURCE=.\r_sprite.c
# End Source File
# Begin Source File

SOURCE=.\r_studio.c
# End Source File
# Begin Source File

SOURCE=.\r_texture.c
# End Source File
# Begin Source File

SOURCE=.\r_utils.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=..\public\const.h
# End Source File
# Begin Source File

SOURCE=.\gl_local.h
# End Source File
# Begin Source File

SOURCE=.\gl_model.h
# End Source File
# Begin Source File

SOURCE=.\qmenu.h
# End Source File
# Begin Source File

SOURCE=.\r_opengl.h
# End Source File
# Begin Source File

SOURCE=.\warpsin.h
# End Source File
# End Group
# End Target
# End Project