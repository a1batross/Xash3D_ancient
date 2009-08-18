# Microsoft Developer Studio Project File - Name="xtools" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=xtools - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "xtools.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "xtools.mak" CFG="xtools - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "xtools - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "xtools - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "xtools - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\xtools\!release"
# PROP Intermediate_Dir "..\temp\xtools\!release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PLATFORM_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "./" /I "../public" /I "bsplib" /I "models" /I "ripper" /I "../common" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
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
# ADD LINK32 msvcrt.lib /nologo /dll /pdb:none /machine:I386 /nodefaultlib:"libc.lib" /opt:nowin98
# SUBTRACT LINK32 /profile
# Begin Custom Build
TargetDir=\Xash3D\src_main\temp\xtools\!release
InputPath=\Xash3D\src_main\temp\xtools\!release\xtools.dll
SOURCE="$(InputPath)"

"D:\Xash3D\bin\xtools.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\xtools.dll "D:\Xash3D\bin\xtools.dll"

# End Custom Build

!ELSEIF  "$(CFG)" == "xtools - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\temp\xtools\!debug"
# PROP Intermediate_Dir "..\temp\xtools\!debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PLATFORM_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /I "./" /I "../public" /I "bsplib" /I "models" /I "ripper" /I "../common" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /FD /GZ /c
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
# ADD LINK32 msvcrtd.lib user32.lib /nologo /dll /debug /machine:I386 /nodefaultlib:"libc.lib" /pdbtype:sept
# SUBTRACT LINK32 /incremental:no /nodefaultlib
# Begin Custom Build
TargetDir=\Xash3D\src_main\temp\xtools\!debug
InputPath=\Xash3D\src_main\temp\xtools\!debug\xtools.dll
SOURCE="$(InputPath)"

"D:\Xash3D\bin\xtools.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\xtools.dll "D:\Xash3D\bin\xtools.dll"

# End Custom Build

!ENDIF 

# Begin Target

# Name "xtools - Win32 Release"
# Name "xtools - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\bsplib\brush.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\brush_primit.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\bsp.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\bspfile_abstract.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\bspfile_ibsp.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\bspfile_rbsp.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\bsplib.c
# End Source File
# Begin Source File

SOURCE=.\ripper\conv_bsplumps.c
# End Source File
# Begin Source File

SOURCE=.\ripper\conv_doom.c
# End Source File
# Begin Source File

SOURCE=.\ripper\conv_image.c
# End Source File
# Begin Source File

SOURCE=.\ripper\conv_main.c
# End Source File
# Begin Source File

SOURCE=.\ripper\conv_shader.c
# End Source File
# Begin Source File

SOURCE=.\ripper\conv_sprite.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\convert_ase.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\convert_map.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\decals.c
# End Source File
# Begin Source File

SOURCE=.\dpvencoder.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\facebsp.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\fog.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\image.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\leakfile.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\light.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\light_bounce.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\light_trace.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\light_ydnar.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\lightmaps_ydnar.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\map.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\md5.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\mesh.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\model.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\patch.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\path_init.c
# End Source File
# Begin Source File

SOURCE=.\models\picointernal.c
# End Source File
# Begin Source File

SOURCE=.\models\picomodel.c
# End Source File
# Begin Source File

SOURCE=.\models\picomodules.c
# End Source File
# Begin Source File

SOURCE=.\models\pm_3ds.c
# End Source File
# Begin Source File

SOURCE=.\models\pm_ase.c
# End Source File
# Begin Source File

SOURCE=.\models\pm_fm.c
# End Source File
# Begin Source File

SOURCE=.\models\pm_md2.c
# End Source File
# Begin Source File

SOURCE=.\models\pm_md3.c
# End Source File
# Begin Source File

SOURCE=.\models\pm_mdc.c
# End Source File
# Begin Source File

SOURCE=.\models\pm_ms3d.c
# End Source File
# Begin Source File

SOURCE=.\models\pm_obj.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\polylib.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\portals.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\prtfile.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\scriplib.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\shaders.c
# End Source File
# Begin Source File

SOURCE=.\spritegen.c
# End Source File
# Begin Source File

SOURCE=.\studio.c
# End Source File
# Begin Source File

SOURCE=.\studio_utils.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\surface.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\surface_extra.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\surface_foliage.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\surface_fur.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\surface_meta.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\tjunction.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\tree.c
# End Source File
# Begin Source File

SOURCE=.\utils.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\vis.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\visflow.c
# End Source File
# Begin Source File

SOURCE=.\wadlib.c
# End Source File
# Begin Source File

SOURCE=.\bsplib\writebsp.c
# End Source File
# Begin Source File

SOURCE=.\xtools.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\bsplib\game_ef.h
# End Source File
# Begin Source File

SOURCE=.\bsplib\game_etut.h
# End Source File
# Begin Source File

SOURCE=.\bsplib\game_ja.h
# End Source File
# Begin Source File

SOURCE=.\bsplib\game_jk2.h
# End Source File
# Begin Source File

SOURCE=.\bsplib\game_qfusion.h
# End Source File
# Begin Source File

SOURCE=.\bsplib\game_quake3.h
# End Source File
# Begin Source File

SOURCE=.\bsplib\game_sof2.h
# End Source File
# Begin Source File

SOURCE=.\bsplib\game_tenebrae.h
# End Source File
# Begin Source File

SOURCE=.\bsplib\game_tremulous.h
# End Source File
# Begin Source File

SOURCE=.\bsplib\game_wolf.h
# End Source File
# Begin Source File

SOURCE=.\bsplib\game_wolfet.h
# End Source File
# Begin Source File

SOURCE=.\mdllib.h
# End Source File
# Begin Source File

SOURCE=.\models\picointernal.h
# End Source File
# Begin Source File

SOURCE=.\models\picomodel.h
# End Source File
# Begin Source File

SOURCE=.\models\pm_fm.h
# End Source File
# Begin Source File

SOURCE=.\bsplib\q3map2.h
# End Source File
# Begin Source File

SOURCE=.\ripper\ripper.h
# End Source File
# Begin Source File

SOURCE=.\utils.h
# End Source File
# Begin Source File

SOURCE=.\xtools.h
# End Source File
# End Group
# End Target
# End Project
