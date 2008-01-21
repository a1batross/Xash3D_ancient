@echo off

set MSDEV=BuildConsole
set CONFIG=/ShowTime /ShowAgent /nologo /cfg=
set MSDEV=msdev
set CONFIG=/make 
set build_type=release
set BUILD_ERROR=
call vcvars32

%MSDEV% engine/engine.dsp %CONFIG%"engine - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% launch/launch.dsp %CONFIG%"launch - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% common/common.dsp %CONFIG%"common - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% ripper/ripper.dsp %CONFIG%"ripper - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% physic/physic.dsp %CONFIG%"physic - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% render/render.dsp %CONFIG%"render - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% viewer/viewer.dsp %CONFIG%"viewer - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

if "%BUILD_ERROR%"=="" goto build_ok

echo *********************
echo *********************
echo *** Build Errors! ***
echo *********************
echo *********************
echo press any key to exit
echo *********************
pause>nul
goto done


@rem
@rem Successful build
@rem
:build_ok

rem //delete log files
if exist engine\engine.plg del /f /q engine\engine.plg
if exist launch\launch.plg del /f /q launch\launch.plg
if exist common\common.plg del /f /q common\common.plg
if exist ripper\ripper.plg del /f /q ripper\ripper.plg
if exist physic\physic.plg del /f /q physic\physic.plg
if exist render\render.plg del /f /q render\render.plg
if exist viewer\viewer.plg del /f /q viewer\viewer.plg

echo 	     Build succeeded!
echo Please wait. Xash is now loading
cd D:\Xash3D\
xash.exe +map qctest -dev 3 -debug -log
:done