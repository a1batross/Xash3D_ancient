# Microsoft Developer Studio Project File - Name="ripper" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=ripper - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ripper.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ripper.mak" CFG="ripper - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ripper - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "ripper - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ripper - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\ripper\!release"
# PROP Intermediate_Dir "..\temp\ripper\!release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PLATFORM_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "./" /I "../public" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
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
# ADD LINK32 msvcrt.lib /nologo /dll /profile /machine:I386 /nodefaultlib:"libc.lib" /opt:nowin98
# Begin Custom Build
TargetDir=\Xash3D\src_main\temp\ripper\!release
InputPath=\Xash3D\src_main\temp\ripper\!release\ripper.dll
SOURCE="$(InputPath)"

"D:\Xash3D\bin\ripper.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\ripper.dll "D:\Xash3D\bin\ripper.dll"

# End Custom Build

!ELSEIF  "$(CFG)" == "ripper - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\temp\ripper\!debug"
# PROP Intermediate_Dir "..\temp\ripper\!debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PLATFORM_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /Gi /GX /ZI /I "./" /I "../public" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /FD /GZ /c
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
# ADD LINK32 msvcrt.lib /nologo /dll /debug /machine:I386 /nodefaultlib:"msvcrtd.lib" /pdbtype:sept
# SUBTRACT LINK32 /incremental:no /nodefaultlib
# Begin Custom Build
TargetDir=\Xash3D\src_main\temp\ripper\!debug
InputPath=\Xash3D\src_main\temp\ripper\!debug\ripper.dll
SOURCE="$(InputPath)"

"D:\Xash3D\bin\ripper.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\ripper.dll "D:\Xash3D\bin\ripper.dll"

# End Custom Build

!ENDIF 

# Begin Target

# Name "ripper - Win32 Release"
# Name "ripper - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\conv_bsplumps.c
# End Source File
# Begin Source File

SOURCE=.\conv_jpgimage.c
# End Source File
# Begin Source File

SOURCE=.\conv_main.c
# End Source File
# Begin Source File

SOURCE=.\conv_palette.c
# End Source File
# Begin Source File

SOURCE=.\conv_pcximage.c
# End Source File
# Begin Source File

SOURCE=.\conv_raw.c
# End Source File
# Begin Source File

SOURCE=.\conv_shader.c
# End Source File
# Begin Source File

SOURCE=.\conv_sound.c
# End Source File
# Begin Source File

SOURCE=.\conv_sprite.c
# End Source File
# Begin Source File

SOURCE=.\conv_sprite2.c
# End Source File
# Begin Source File

SOURCE=.\conv_vtfimage.c
# End Source File
# Begin Source File

SOURCE=.\conv_wadlumps.c
# End Source File
# Begin Source File

SOURCE=.\conv_wally.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\qc_gen.h
# End Source File
# Begin Source File

SOURCE=.\ripper.h
# End Source File
# End Group
# End Target
# End Project
