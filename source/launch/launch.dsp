# Microsoft Developer Studio Project File - Name="launch" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=launch - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "launch.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "launch.mak" CFG="launch - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "launch - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "launch - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "launch - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\launch\!release"
# PROP Intermediate_Dir "..\temp\launch\!release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LAUNCH_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "./" /I "imagelib" /I "../public" /I "../common" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
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
# ADD LINK32 zlib.lib png.lib jpg.lib ogg.lib vorbis.lib user32.lib gdi32.lib shell32.lib advapi32.lib winmm.lib /nologo /dll /pdb:none /machine:I386 /nodefaultlib:"libc" /nodefaultlib:"libcmt" /libpath:"./imagelib" /libpath:"./soundlib" /opt:nowin98
# Begin Custom Build
TargetDir=\Xash3D\src_main\temp\launch\!release
InputPath=\Xash3D\src_main\temp\launch\!release\launch.dll
SOURCE="$(InputPath)"

"D:\Xash3D\bin\launch.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\launch.dll "D:\Xash3D\bin\launch.dll"

# End Custom Build

!ELSEIF  "$(CFG)" == "launch - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\temp\launch\!debug"
# PROP Intermediate_Dir "..\temp\launch\!debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LAUNCH_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /I "./" /I "imagelib" /I "../public" /I "../common" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /FD /GZ /c
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
# ADD LINK32 zlib.lib png.lib jpg.lib ogg.lib vorbis.lib user32.lib gdi32.lib shell32.lib advapi32.lib winmm.lib /nologo /dll /debug /machine:I386 /nodefaultlib:"libcmt" /nodefaultlib:"libc" /pdbtype:sept /libpath:"./imagelib" /libpath:"./soundlib"
# Begin Custom Build
TargetDir=\Xash3D\src_main\temp\launch\!debug
InputPath=\Xash3D\src_main\temp\launch\!debug\launch.dll
SOURCE="$(InputPath)"

"D:\Xash3D\bin\launch.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\launch.dll "D:\Xash3D\bin\launch.dll"

# End Custom Build

!ENDIF 

# Begin Target

# Name "launch - Win32 Release"
# Name "launch - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\cmd.c
# End Source File
# Begin Source File

SOURCE=.\console.c
# End Source File
# Begin Source File

SOURCE=.\cpuinfo.c
# End Source File
# Begin Source File

SOURCE=.\crclib.c
# End Source File
# Begin Source File

SOURCE=.\cvar.c
# End Source File
# Begin Source File

SOURCE=.\export.c
# End Source File
# Begin Source File

SOURCE=.\filesystem.c
# End Source File
# Begin Source File

SOURCE=.\imagelib\img_bmp.c
# End Source File
# Begin Source File

SOURCE=.\imagelib\img_dds.c
# End Source File
# Begin Source File

SOURCE=.\imagelib\img_jpg.c
# End Source File
# Begin Source File

SOURCE=.\imagelib\img_main.c
# End Source File
# Begin Source File

SOURCE=.\imagelib\img_pcx.c
# End Source File
# Begin Source File

SOURCE=.\imagelib\img_png.c
# End Source File
# Begin Source File

SOURCE=.\imagelib\img_tga.c
# End Source File
# Begin Source File

SOURCE=.\imagelib\img_utils.c
# End Source File
# Begin Source File

SOURCE=.\imagelib\img_vtf.c
# End Source File
# Begin Source File

SOURCE=.\imagelib\img_wad.c
# End Source File
# Begin Source File

SOURCE=.\library.c
# End Source File
# Begin Source File

SOURCE=.\memlib.c
# End Source File
# Begin Source File

SOURCE=.\network.c
# End Source File
# Begin Source File

SOURCE=.\parselib.c
# End Source File
# Begin Source File

SOURCE=.\patch.c
# End Source File
# Begin Source File

SOURCE=.\soundlib\snd_main.c
# End Source File
# Begin Source File

SOURCE=.\soundlib\snd_ogg.c
# End Source File
# Begin Source File

SOURCE=.\soundlib\snd_raw.c
# End Source File
# Begin Source File

SOURCE=.\soundlib\snd_utils.c
# End Source File
# Begin Source File

SOURCE=.\soundlib\snd_wav.c
# End Source File
# Begin Source File

SOURCE=.\stdlib.c
# End Source File
# Begin Source File

SOURCE=.\system.c
# End Source File
# Begin Source File

SOURCE=.\utils.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\filesystem.h
# End Source File
# Begin Source File

SOURCE=.\launch.h
# End Source File
# Begin Source File

SOURCE=.\library.h
# End Source File
# Begin Source File

SOURCE=.\soundlib\soundlib.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
