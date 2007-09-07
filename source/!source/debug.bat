@echo off

set MSDEV=BuildConsole
set CONFIG=/ShowTime /ShowAgent /nologo /cfg=
set MSDEV=msdev
set CONFIG=/make 
set build_type=debug
set BUILD_ERROR=
call vcvars32

%MSDEV% editor/editor.dsp %CONFIG%"editor - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% engine/engine.dsp %CONFIG%"engine - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% launcher/launcher.dsp %CONFIG%"launcher - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% platform/platform.dsp %CONFIG%"platform - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% renderer/renderer.dsp %CONFIG%"renderer - Win32 Debug" %build_target%
if errorlevel 1 set BUILD_ERROR=1

progs\qcclib.exe
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
if exist editor\editor.plg del /f /q editor\editor.plg
if exist engine\engine.plg del /f /q engine\engine.plg
if exist launcher\launcher.plg del /f /q launcher\launcher.plg
if exist platform\platform.plg del /f /q platform\platform.plg
if exist renderer\renderer.plg del /f /q renderer\renderer.plg
if exist progs\server.dat copy progs\server.dat D:\Xash3D\xash\server.dat

echo 	     Build succeeded!
echo Please wait. Xash is now loading
cd D:\Xash3D\
xash.exe +map qctest -log
:done