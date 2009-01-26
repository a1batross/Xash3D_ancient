@echo off

set MSDEV=BuildConsole
set CONFIG=/ShowTime /ShowAgent /nologo /cfg=
set MSDEV=msdev
set CONFIG=/make 
set build_type=release
set BUILD_ERROR=
call vcvars32

%MSDEV% baserc/baserc.dsp %CONFIG%"baserc - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% client/client.dsp %CONFIG%"client - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% engine/engine.dsp %CONFIG%"engine - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% launch/launch.dsp %CONFIG%"launch - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% physic/physic.dsp %CONFIG%"physic - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% render/render.dsp %CONFIG%"render - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% server/server.dsp %CONFIG%"server - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% vprogs/vprogs.dsp %CONFIG%"vprogs - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% vsound/vsound.dsp %CONFIG%"vsound - Win32 Release" %build_target%
if errorlevel 1 set BUILD_ERROR=1

%MSDEV% xtools/xtools.dsp %CONFIG%"xtools - Win32 Release" %build_target%
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
if exist baserc\baserc.plg del /f /q baserc\baserc.plg
if exist client\client.plg del /f /q client\client.plg
if exist engine\engine.plg del /f /q engine\engine.plg
if exist launch\launch.plg del /f /q launch\launch.plg
if exist physic\physic.plg del /f /q physic\physic.plg
if exist render\render.plg del /f /q render\render.plg
if exist server\server.plg del /f /q server\server.plg
if exist vprogs\vprogs.plg del /f /q vprogs\vprogs.plg
if exist vsound\vsound.plg del /f /q vsound\vsound.plg
if exist xtools\xtools.plg del /f /q xtools\xtools.plg

echo 	     Build succeeded!
echo Please wait. Xash is now loading
cd D:\Xash3D\
quake.exe -game xash -dev 3 -log +map dm_qstyle
:done