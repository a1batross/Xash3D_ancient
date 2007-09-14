# Microsoft Developer Studio Project File - Name="common" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=common - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "common.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "common.mak" CFG="common - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "common - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "common - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "common - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\common\!release"
# PROP Intermediate_Dir "..\temp\common\!release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PLATFORM_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /Ob0 /I "./" /I "../public" /I "./bsplib/" /I "./qcclib" /I "./mdllib" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386 /opt:nowin98
# ADD LINK32 msvcrt.lib winmm.lib /nologo /dll /pdb:none /machine:I386 /nodefaultlib:"libc.lib" /opt:nowin98
# Begin Custom Build
TargetDir=\XASH3D\src_main\!source\temp\common\!release
InputPath=\XASH3D\src_main\!source\temp\common\!release\common.dll
SOURCE="$(InputPath)"

"D:\Xash3D\bin\common.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\common.dll "D:\Xash3D\bin\common.dll"

# End Custom Build

!ELSEIF  "$(CFG)" == "common - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\temp\common\!debug"
# PROP Intermediate_Dir "..\temp\common\!debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PLATFORM_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /I "./" /I "../public" /I "./bsplib/" /I "./qcclib" /I "./common" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 msvcrt.lib winmm.lib /nologo /dll /debug /machine:I386 /nodefaultlib:"msvcrtd.lib" /pdbtype:sept
# SUBTRACT LINK32 /incremental:no /nodefaultlib
# Begin Custom Build
TargetDir=\XASH3D\src_main\!source\temp\common\!debug
InputPath=\XASH3D\src_main\!source\temp\common\!debug\common.dll
SOURCE="$(InputPath)"

"D:\Xash3D\bin\common.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\common.dll "D:\Xash3D\bin\common.dll"

# End Custom Build

!ENDIF 

# Begin Target

# Name "common - Win32 Release"
# Name "common - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\basemem.c
# End Source File
# Begin Source File

SOURCE=.\baseutils.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\brushbsp.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\bspfile.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\csg.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\faces.c
# End Source File
# Begin Source File

SOURCE=.\filesystem.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\flow.c
# End Source File
# Begin Source File

SOURCE=.\imglib.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\leakfile.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\lightmap.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\map.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\patches.c
# End Source File
# Begin Source File

SOURCE=.\platform.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\portals.c
# End Source File
# Begin Source File

SOURCE=.\qcclib\pr_comp.c
# End Source File
# Begin Source File

SOURCE=.\qcclib\pr_lex.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\prtfile.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\qbsp3.c
# End Source File
# Begin Source File

SOURCE=.\qcclib\qcc_utils.c
# End Source File
# Begin Source File

SOURCE=.\qcclib\qccmain.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\qrad3.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\qvis3.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\shaders.c
# End Source File
# Begin Source File

SOURCE=.\spritegen.c
# End Source File
# Begin Source File

SOURCE=.\common\studio.c
# End Source File
# Begin Source File

SOURCE=.\common\studio_utils.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\textures.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\trace.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\tree.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\winding.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\writebsp.c
# End Source File
# Begin Source File

SOURCE=.\ziplib.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\basemem.h
# End Source File
# Begin Source File

SOURCE=.\baseutils.h
# End Source File
# Begin Source File

SOURCE=.\blankframe.h
# End Source File
# Begin Source File

SOURCE=.\bsplib\bsplib.h
# End Source File
# Begin Source File

SOURCE=.\image.h
# End Source File
# Begin Source File

SOURCE=.\common\mdllib.h
# End Source File
# Begin Source File

SOURCE=.\platform.h
# End Source File
# Begin Source File

SOURCE=.\qcclib\qcclib.h
# End Source File
# Begin Source File

SOURCE=..\public\ref_system.h
# End Source File
# Begin Source File

SOURCE=.\zip32.h
# End Source File
# Begin Source File

SOURCE=.\ziplib.h
# End Source File
# End Group
# End Target
# End Project
